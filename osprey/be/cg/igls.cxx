/*
 *  Copyright (C) 2006. QLogic Corporation. All Rights Reserved.
 */

/*
 * Copyright 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if 
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston MA 02111-1307, USA.

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/


// =======================================================================
// =======================================================================
//
//  Module: igls.cxx
//  $Revision: 1.1.1.1 $
//  $Date: 2005/10/21 19:00:00 $
//  $Author: marcel $
//  $Source: /proj/osprey/CVS/open64/osprey1.0/be/cg/igls.cxx,v $
//
//  Description:
//  ============
//
//  Integrated Global and Local Scheduling Framework (IGLS). The main driver 
//  controls the execution of the local scheduling phase (LOCS), the
//  hyperblock scheduling phase (HBS) and the global scheduling phase (GCM).
//
// =======================================================================
// =======================================================================

#include <stdint.h>
#include <alloca.h>
#include <math.h>
#include "defs.h"
#include "config.h"
#include "config_TARG.h"
#include "mempool.h"
#include "bb.h"
#include "bb_set.h"
#include "tracing.h"
#include "timing.h"
#include "cgir.h"
#include "glob.h"
#include "tn_map.h"
#include "cg.h"
#include "cg_flags.h"
#include "ercg.h"
#include "cgtarget.h"
#include "cg_vector.h"
#include "dominate.h"
#include "findloops.h"
#include "note.h"
#include "lra.h"
#include "gcm.h"
#include "ti_res.h"
#include "ti_res_res.h"
#include "ti_latency.h"
#include "ti_errors.h"
#include "cg_region.h"
#include "gtn_universe.h"
#include "gtn_set.h"
#include "cxx_memory.h"
#include "hb_sched.h"
#include "hb_hazards.h"
#include "targ_proc_properties.h"

#ifdef TARG_IA64
#include "bb.h"
#include "op.h"
#endif

// ======================================================================
// IGLS_Schedule_Region 
//
// The main driver for invoking all the scheduling phases in CG. They mainly
// include HBS (for single-BBs and hyperblocks) and GCM. The data-speculation
// phase is also invoked here since it's tied very closely with the 
// scheduling phase. 
// The <before_regalloc> parameter indicates whether the scheduler is being 
// invoked before or after register allocation. The amount of work done by 
// the various phases depends on the optimization level. 
//
// -O0 : insert noops to remove hazards.
// -O1 : perform HBS scheduling for local BBs (ONLY) after register allocation.
//       fill branch delay slot nops (for MIPS).
// -O2 : perform hyperblock(s) scheduling before register allocation.
//       provide accurate register estimates for GRA.
//       invoke post-GRA global scheduling (post-GCM) phase
//       invoke THR phase to perform data-speculation (after register-
//       allocation).
// -O3 : perform hyperblock(s) scheduling before register allocation.
//	 provide accurate register estimates for GRA/GCM.
//	 invoke pre-GRA global scheduling (pre-GCM) phase.
//	 invoke post-GRA global scheduling (post-GCM) phase
//
// ======================================================================
void
IGLS_Schedule_Region (BOOL before_regalloc)
{
  BB *bb;
  BOOL should_we_local_schedule;  // controls local scheduling (single BBs).
  BOOL should_we_global_schedule; // controls HB scheduling and GCM.
  BOOL should_we_schedule;        // controls all scheduling (LOCS,  HBS, GCM)
  BOOL should_we_do_thr;          // controls the THR phase in CG.

  RID *rid;
  HBS_TYPE hbs_type;
  HB_Schedule *Sched = NULL;
  CG_THR      *thr = NULL;

  Set_Error_Phase ("Hyperlock Scheduler");
  Start_Timer (T_Sched_CU);
  Trace_HB = Get_Trace (TP_SCHED, 1);
  should_we_schedule = IGLS_Enable_All_Scheduling;
  should_we_do_thr = CG_enable_thr;
  L_Save();

  if (before_regalloc) {

    // schedule if (-O > O1) and
    // -CG:local_sched=on && -CG:pre_local_sched=on.
    should_we_local_schedule = (   CG_opt_level > 1
				   && LOCS_Enable_Scheduling
				   && LOCS_PRE_Enable_Scheduling);

    // global schedule if (-O > O2) and either of the following below are true.
    // -CG:hb_sched=on && -CG:pre_hb_sched=on (hyperblock scheduling).
    // -CG:gcm=on && -CG:pre_gcm=on for GCM.
    should_we_global_schedule = ( CG_opt_level > 2 &&
				  ((IGLS_Enable_HB_Scheduling &&
				    IGLS_Enable_PRE_HB_Scheduling) ||
				   (GCM_PRE_Enable_Scheduling &&
				    GCM_Enable_Scheduling)));

    hbs_type = HBS_BEFORE_GRA | HBS_BEFORE_LRA | HBS_DEPTH_FIRST;
    if (Trace_HB) {
      #pragma mips_frequency_hint NEVER
      fprintf (TFile, "***** HYPERBLOCK SCHEDULER (before GRA) *****\n");
    }
  }
  else {

    // schedule if (-O > O0) and
    // -CG:local_sched=on && -CG:post_local_sched=on.
    should_we_local_schedule = (   CG_opt_level > 0
				   && LOCS_Enable_Scheduling
				   && LOCS_POST_Enable_Scheduling);

    // global schedule if (-O > O1) and either of the following below are true.
    // -CG:hb_sched=on && -CG:post_hb_sched=on (hyperblock scheduling).
    // -CG:gcm=on && -CG:post_gcm=on for GCM.
    should_we_global_schedule = ( CG_opt_level > 1 &&
				  ((IGLS_Enable_HB_Scheduling &&
				   (IGLS_Enable_POST_HB_Scheduling ||
				    IGLS_Enable_PRE_HB_Scheduling)) ||
				   (GCM_Enable_Scheduling &&
				    GCM_POST_Enable_Scheduling)));
    hbs_type = HBS_CRITICAL_PATH;
    if (PROC_has_bundles()) hbs_type |= HBS_MINIMIZE_BUNDLES;

    // allow data-speculation if (-O > O1) and -OPT:space is turned off.
    should_we_do_thr = should_we_do_thr && (CG_opt_level > 1) && !OPT_Space;

    if (Trace_HB) {
      #pragma mips_frequency_hint NEVER
      fprintf (TFile, "***** HYPERBLOCK SCHEDULER (after GRA) *****\n");
    }
  }

  // Before register allocation:
  // - Do hyperblock scheduling first to get perfect schedules at each
  //   hyperblock level (register-sensitive). 
  // - Do GCM next to extract global parallelism. Some work needs to be
  //   done, so that it strictly enforces hyperblock boundaries.
  // - Do local scheduling for BBs which are not part of any hyperblocks.

  if (before_regalloc) {
    if (!should_we_schedule) return;

    // Do HB scheduling for all HBs generated (before register allocation).
    if (IGLS_Enable_HB_Scheduling && IGLS_Enable_PRE_HB_Scheduling &&
	should_we_global_schedule) {
      HB_Remove_Deleted_Blocks();
      std::list<HB*>::iterator hbi;
      FOR_ALL_BB_STLLIST_ITEMS_FWD(HB_list, hbi) {
	if (!Sched) {
	  Sched = CXX_NEW(HB_Schedule(), &MEM_local_pool);
	}

	// Check to see if not SWP'd.
	std::list<BB*> hb_blocks;
	Get_HB_Blocks_List(hb_blocks,*hbi);
	if (Can_Schedule_HB(hb_blocks)) {
	  Sched->Init(hb_blocks, hbs_type, NULL);
	  Sched->Schedule_HB(hb_blocks);
	}
      }
    }

    // Try GCM (before register allocation).
    if (GCM_Enable_Scheduling && should_we_global_schedule) {
	Stop_Timer (T_Sched_CU);

	GCM_Schedule_Region (hbs_type);

	Set_Error_Phase ("Hyperblock Scheduler (HBS)");
	Start_Timer (T_Sched_CU);
    }

    if (!should_we_local_schedule) return;

    // Do local scheduling for BBs which are not part of HBs. 
    // (before register allocation).
    for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
      
      if (    ( rid = BB_rid(bb) )
	      && ( RID_level(rid) >= RL_CGSCHED ) )
	continue;
      
      if (!BB_scheduled(bb)) {
	if (!Sched) {
	  Sched = CXX_NEW(HB_Schedule(), &MEM_local_pool);
	}
	Sched->Init(bb, hbs_type, INT32_MAX, NULL, NULL);
	Sched->Schedule_BB(bb, NULL);
      }
    }
  }
  else {

    // After register allocation:
    // - Perform data-speculation first, since it will expose more 
    //   parallelism and scheduling opportunities at the block level.
    // - Do hyperblock scheduling next to get perfect schedules at each
    //   hyperblock level (parallelism-driven).
    // - Do GCM next to extract global parallelism. Some work needs to be
    //   done, so that it strictly enforces hyperblock boundaries.
    // - Do local scheduling for BBs which are not part of any hyperblocks.

    // Perform data-speculation first, since it will expose parallelism
    // and scheduling opportunities at the block level.
    // TODO: Invoke data-speculation phase before register allocation,
    // requires GRA spill support, and conditionally invoke the phase
    // after register allocation.

    if (should_we_do_thr) {
      Stop_Timer (T_Sched_CU);
      Set_Error_Phase ("Tree-Height Reduction (THR)");
      Start_Timer (T_THR_CU);

      for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
	if (    ( rid = BB_rid(bb) )
		&& ( RID_level(rid) >= RL_CGSCHED ) )
	  continue;

	// Perform data-speculation (if profitable).
	// Avoid processing SWP scheduled blocks, all other scheduled blocks
	// are still considered as candidates for THR.

	if (BB_scheduled(bb) && !BB_scheduled_hbs(bb)) continue;
	if (!thr) {
	  thr = CXX_NEW(CG_THR(), &MEM_local_pool);
	}
	thr->Init(bb, THR_DATA_SPECULATION_NO_RB, FALSE);
	thr->Perform_THR();
	
      } /* for (bb= REGION_First_BB).. */

      Stop_Timer (T_THR_CU);
      Check_for_Dump (TP_THR, NULL);
      Start_Timer (T_Sched_CU);

    } /* should_we_do_thr */

    // Do HB scheduling for all HBs generated (after register allocation).
    if (IGLS_Enable_HB_Scheduling && IGLS_Enable_POST_HB_Scheduling &&
	should_we_schedule && should_we_global_schedule) {

      HB_Remove_Deleted_Blocks();
      std::list<HB*>::iterator hbi;
      FOR_ALL_BB_STLLIST_ITEMS_FWD(HB_list, hbi) {
	if (!Sched) {
	  Sched = CXX_NEW(HB_Schedule(), &MEM_local_pool);
	}
	// Check to see if not SWP'd.
	std::list<BB*> hb_blocks;
	Get_HB_Blocks_List(hb_blocks,*hbi);
	if (Can_Schedule_HB(hb_blocks)) {
	  Sched->Init(hb_blocks, hbs_type, NULL);
	  Sched->Schedule_HB(hb_blocks);
	}
      }
    }

    // Try GCM for the region (after register allocation).
    if (GCM_Enable_Scheduling && should_we_schedule &&
	should_we_global_schedule) {
	Stop_Timer (T_Sched_CU);

 	GCM_Schedule_Region (hbs_type);

        Set_Error_Phase ("Hyperblock Scheduler (HBS)");
	Start_Timer (T_Sched_CU);
    }

    // Do local scheduling for BBs which are not part of HBs. 
    // (after register allocation).
    for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
      if (    ( rid = BB_rid(bb) )
	      && ( RID_level(rid) >= RL_CGSCHED ) )
	continue;

      BOOL skip_bb = BB_scheduled(bb) && !BB_scheduled_hbs(bb);

      if (should_we_do_thr && !skip_bb) Remove_Unnecessary_Check_Instrs(bb);

#ifdef KEY_1873
      /* The original code with Reschedule_BB is meanlingless. I think the original
	 author meant BB_scheduled(bb), not Reschedule_BB(bb).
      */
      const BOOL resched = FALSE;
#else
      BOOL resched = !skip_bb && Reschedule_BB(bb); /* FALSE; */      
#endif // KEY
      if (should_we_schedule && should_we_local_schedule &&
	  (!skip_bb || resched)) {

#ifdef TARG_IA64
        extern void Clean_Up (BB* bb);
        Clean_Up(bb); 
        Reset_BB_scheduled(bb);  
#endif  
  	// TODO: try locs_type = LOCS_DEPTH_FIRST also.
	INT32 max_sched = (resched) ?  OP_scycle(BB_last_op(bb))+1 : INT32_MAX;
	if (LOCS_Enable_Scheduling) {
	  if (!Sched) {
	    Sched = CXX_NEW(HB_Schedule(), &MEM_local_pool);
	  }
	  Sched->Init(bb, hbs_type, max_sched, NULL, NULL);
	  Sched->Schedule_BB(bb, NULL);
	}
      }
      Handle_All_Hazards (bb);
    } /* for (bb= REGION_First_BB).. */

#ifdef TARG_X8664
    {
      extern void CG_Sched( MEM_POOL*, BOOL );
      CG_Sched( &MEM_local_pool, Get_Trace( TP_SCHED, 1 ) );
    }
#endif

    // Do branch optimizations here.
    if (should_we_schedule && should_we_local_schedule) {
      if (GCM_Enable_Scheduling) GCM_Fill_Branch_Delay_Slots ();
      if (Assembly) Add_Scheduling_Notes_For_Loops ();
    }
  }

  // need to explicitly delete Sched and thr
  // so that destructors are called.
  if (Sched) {
	CXX_DELETE(Sched, &MEM_local_pool);
  }
  if (thr) {
	CXX_DELETE(thr, &MEM_local_pool);
  }
  L_Free();
   
 Check_for_Dump (TP_SCHED, NULL);
 Stop_Timer (T_Sched_CU);
}









