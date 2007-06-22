/*
 * Copyright 2002, 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
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



/* =======================================================================
 * =======================================================================
 *
 *  Module: gcm.cxx
 *  $Revision: 1.1.1.1 $
 *  $Date: 2005/10/21 19:00:00 $
 *  $Author: marcel $
 *  $Source: /proj/osprey/CVS/open64/osprey1.0/be/cg/gcm.cxx,v $
 *
 *  Description:
 *  ============
 *
 *  Global Code Motion (GCM)
 *
 * =======================================================================
 * =======================================================================
 */

#include <stdint.h>
#include <alloca.h>
#include "defs.h"
#include "config.h"
#include "mempool.h"
#include "tracing.h"
#include "timing.h"
#include "cgir.h"
#include "cg.h"
#include "cg_flags.h"
#include "cgprep.h"
#include "ttype.h"
#include "targ_sim.h"
#include "bb.h"
#include "variants.h"
#include "bitset.h"
#include "bb_set.h"
#include "freq.h"
#include "cgtarget.h"
#include "cxx_memory.h"
#include "whirl2ops.h"
#include "dominate.h"
#include "findloops.h"
#include "cg_vector.h"
#include "hb_sched.h"
#include "reg_live.h"
#include "gcm.h"
#include "glob.h"
#include "cflow.h"
#include "tn_set.h"
#include "cgemit.h"
#include "gtn_universe.h"
#include "gtn_set.h"
#include "gra_live.h"
#include "tn_map.h"
#include "cg_sched_est.h"
#include "cg_loop.h"
#include "pf_cg.h"
#include "targ_proc_properties.h"
#ifdef TARG_X8664
#include "lra.h"
#endif

// cgdriver flags variabes 
BOOL GCM_POST_Spec_Loads = TRUE;
BOOL GCM_PRE_Spec_Loads = TRUE;
BOOL GCM_Use_Sched_Est = FALSE;
BOOL GCM_Forw_Circ_Motion = TRUE;
BOOL GCM_POST_Force_Scheduling = FALSE;
BOOL CG_Skip_GCM = FALSE;
INT32 GCM_From_BB = -1;
INT32 GCM_To_BB = -1;
INT32 GCM_Result_TN = -1;
#ifdef KEY
INT32 GCM_BB_Limit = -1;
INT32 cumulative_cand_bb;
#endif

// Internal state flags to store speculative information
#define SPEC_NONE		0x00	// no speculative motion
#define SPEC_EAGER_PTR		0x01	// eager ptr speculation
#define SPEC_EAGER_NULL_PTR	0x02	// eager null ptr speculation
#define SPEC_CIRC_PTR_ABOVE     0x04    // circluar ptr speculation (above)
#define SPEC_CSAFE_PTR          0x08    // safe control speculation 
#define SPEC_DSAFE_PTR          0x10    // safe data speculation
#define SPEC_CDSAFE_PTR         (SPEC_CSAFE_PTR | SPEC_DSAFE_PTR) // safe control (and data) speculation
#define SPEC_PSAFE_PTR          0x20    // safe predicate promotion

#define EAGER_NONE(o)			((o) & SPEC_NONE)
#define Set_EAGER_NONE(o)		((o) |= SPEC_NONE)
#define Reset_EAGER_NONE(o) 		((o) &= ~SPEC_NONE)
#define EAGER_PTR_SPEC(o)		((o) & SPEC_EAGER_PTR)
#define Set_EAGER_PTR_SPEC(o)		((o) |= SPEC_EAGER_PTR)
#define Reset_EAGER_PTR_SPEC(o)		((o) &= ~SPEC_EAGER_PTR)
#define EAGER_NULL_PTR_SPEC(o)		((o) & SPEC_EAGER_NULL_PTR)
#define Set_EAGER_NULL_PTR_SPEC(o)	((o) |= SPEC_EAGER_NULL_PTR)
#define Reset_EAGER_NULL_PTR_SPEC(o)	((o) &= ~SPEC_EAGER_NULL_PTR)
#define CIRC_PTR_SPEC(o)		((o) & SPEC_CIRC_PTR_ABOVE)
#define Set_CIRC_PTR_SPEC(o)	        ((o) |= SPEC_CIRC_PTR_ABOVE)
#define Reset_CIRC_PTR_SPEC(o)	        ((o) &= ~SPEC_CIRC_PTR_ABOVE)
#define CSAFE_PTR_SPEC(o)		((o) & SPEC_CSAFE_PTR)
#define Set_CSAFE_PTR_SPEC(o)	        ((o) |= SPEC_CSAFE_PTR)
#define Reset_CSAFE_PTR_SPEC(o)	        ((o) &= ~SPEC_CSAFE_PTR)
#define DSAFE_PTR_SPEC(o)		((o) & SPEC_DSAFE_PTR)
#define Set_DSAFE_PTR_SPEC(o)	        ((o) |= SPEC_DSAFE_PTR)
#define Reset_DSAFE_PTR_SPEC(o)	        ((o) &= ~SPEC_DSAFE_PTR)
#define CDSAFE_PTR_SPEC(o)		((o) & SPEC_CDSAFE_PTR)
#define Set_CDSAFE_PTR_SPEC(o)	        ((o) |= SPEC_CDSAFE_PTR)
#define Reset_CDSAFE_PTR_SPEC(o)        ((o) &= ~SPEC_CDSAFE_PTR)
#define PSAFE_PTR_SPEC(o)		((o) & SPEC_PSAFE_PTR)
#define Set_PSAFE_PTR_SPEC(o)	        ((o) |= SPEC_PSAFE_PTR)
#define Reset_PSAFE_PTR_SPEC(o)         ((o) &= ~SPEC_PSAFE_PTR)

/* TODO: change speculation ratio with a compile time option */
static float speculation_ratio_wfb = 0.35;
static float speculation_ratio_fb = 0.75;

// static variables to keep track of state while performing the GCM phase, includes tracing
// flags as well.
static BOOL Trace_GCM = FALSE;
static BOOL Trace_GCM_Reg_Usage = FALSE;
static BOOL Trace_Fill_Delay_Slots = FALSE;
static BOOL GCM_Internal_Flag = TRUE; // internal flag for extraneous .T dumps
static BB* GCM_Loop_Prolog; // internal variable to keep track of loop prologue bbs
// static BOOL GCM_PRE_Pass_Enabled = FALSE; // flag enabled if pre_gcm invoked.

static INT32 mispredict, fixed, taken;
static double times;

static BB_MAP bbsch_map;

// Memory pool for LOOP_DESCR 
static MEM_POOL loop_descr_pool;

// Memory pool for BB dom/pdom sets
static MEM_POOL gcm_loop_pool;

static HBS_TYPE cur_hbs_type;	// for saving GCM_Schedule_Region's parameter

// Map and accessors to associate a set of cycles with each BB.
// A loop may contain more than one cycle, so we assign each cycle
// an ID. Then for each BB, we create a set of cycle IDs which identifies
// the cycles that contains that BB.
//
// If a loop has only one cycle we don't create these sets.
// Therefore if the map entry for a BB is NULL, the BB is either not in
// a loop or in only one cycle. Also, if a BB has a map entry than it's
// cycle set is non-empty.
static BB_MAP bb_cycle_set_map;

#define BB_cycle_set(bb)	((BS *)BB_MAP_Get(bb_cycle_set_map, (bb)))
#define Set_BB_cycle_set(bb, bs) (BB_MAP_Set(bb_cycle_set_map, (bb), (bs)))

// This variable is set to TRUE if we want to run cflow again due to
// branch delay-slot filling creating empty basic blocks.
static BOOL Run_Cflow_Delay;
// This variable is set to TRUE if we want to run cflow again after GCM
BOOL Run_Cflow_GCM;
GCM_TYPE Cur_Gcm_Type;

static BOOL Ignore_TN_Dep; // to identify if TN dependences can be ignored
static INT cur_pc = 0; // to hold the pc- value

// Sort routines must return negative/0/positive according to qsort.
// =======================================================================
// Sort_by_bb_frequency
// returns TRUE if bb1 has higher frequency estimate than bb2.
// =======================================================================
static INT
sort_by_bb_frequency (const void *bb1, const void *bb2)
{
#ifdef KEY
  const BB* A = *(BB**)bb1;  
  const BB* B = *(BB**)bb2;

  if( BB_freq(A) > BB_freq(B) )
    return -1;
  if( BB_freq(A) < BB_freq(B) )
    return 1;

  return BB_id(A) < BB_id(B) ? -1 : 1;

#else
  if (BB_freq((BB *)bb1) > BB_freq((BB *)bb2)) return 1;
  else if (BB_freq((BB *)bb1) < BB_freq((BB *)bb2)) return -1;  
  else return 0;
#endif
}

// =======================================================================
// Sort_by_edge_probability
// returns TRUE if edge bl1 has higher probability
// estimate than bl2. For this to be meaningful, the two edges must be
// out of the same BB.
// =======================================================================
static INT
sort_by_edge_probability (const void *bl1, const void *bl2)
{
  if (BBLIST_prob((BBLIST *)bl1) > BBLIST_prob((BBLIST *)bl2)) return 1;
  else if (BBLIST_prob((BBLIST *)bl1) < BBLIST_prob((BBLIST *)bl2)) return -1;
  else return 0;
}

// =======================================================================
// Sort_by_bb_live_in
// returns TRUE if the number of live variables (weighted by the frequency)
// from bb1 are more than that of bb2. 
// =======================================================================
static INT
sort_by_bb_live_in (const void *bb1, const void *bb2)
{
  UINT8 bb1_count = 0, bb2_count = 0;

  TN* x;
  FOR_ALL_GTN_SET_members(BB_live_in((BB *)bb1), x) bb1_count++;
  FOR_ALL_GTN_SET_members(BB_live_in((BB *)bb2), x) bb2_count++;

  if ((BB_freq((BB *)bb1) * bb1_count) > (BB_freq((BB *)bb2) * bb2_count)) return 1;
  else if ((BB_freq((BB *)bb1) * bb1_count) < (BB_freq((BB *)bb2) * bb2_count)) return -1;
  else return 0;
}

// =======================================================================
// Is_BB_Empty
// Check if a basic block has any executable instructions. Return TRUE
// if the block is empty.
// =======================================================================
static BOOL
Is_BB_Empty (BB *bb)
{
  for (OP *op = BB_first_op(bb); op != NULL; op = OP_next(op)) {
    if (OP_Real_Ops(op) != 0) return FALSE;
  }
  return TRUE;
}

// =======================================================================
// Print_Trace_File
// Common Trace routine
// =======================================================================
static void
Print_Trace_File(OP *cand_op, BB *src_bb, BB *cand_bb, BOOL success)
{
  const char *str = (success) ? "SUCCESS" : "FAIL";
  fprintf (TFile, "%s_GCM(%s): MOVE ",Ignore_TN_Dep ? "POST":"PRE",str);
  Print_OP_No_SrcLine (cand_op);
  fprintf (TFile,"	FROM BB:%d => TO BB:%d:\n", 
	   BB_id(src_bb), BB_id(cand_bb));
}

// =======================================================================
// OP_Is_Expensive
// checks to see if <cur_op> is expensive. These ops are long latency
// ops and the benefit of speculating them are minimal. 
// TODO. need to provide framework in local scheduler which will take
// into account dangling latencies (across basic blocks) for this expensive
// ops. This will help prune the cases further. this is a 7.3 affair.
// need to take into account CGTARG_ option as well.
// =======================================================================
static BOOL
OP_Is_Expensive (OP *cur_op)
{
  return CGTARG_Is_Long_Latency(OP_code(cur_op));
}

// =======================================================================
// First_Inst_Of_BB
// Return the first instruction of the basic block <bb>. Return NULL if
// we are unable to find one. 
// =======================================================================
static OP *
First_Inst_Of_BB (BB *bb)
{
  OP *op;

  if (bb == NULL) return NULL;
  FOR_ALL_BB_OPs_FWD (bb, op) {
    if (OP_dummy(op)) continue;
    return op;
  }
  // If the bb has only dummy ops, look at the following bb.
  if (BB_succs(bb) != NULL) {
    BB *succ_bb = BBLIST_item (BB_succs(bb));
    if (BBlist_Len (BB_preds(succ_bb)) == 1) {
      return First_Inst_Of_BB (succ_bb);
    }
  }
  return NULL;
}

inline BOOL within_bounds(INT num1, INT num2, INT lower_bound,INT upper_bound){
  return (((num1 - num2) < upper_bound) && ((num1 - num2) > lower_bound));
}

// =======================================================================
// OP_Offset_Within_Limit
// Checks to see if the offset field of an memory op lies between the
// lower and upper bound
// =======================================================================
static BOOL 
OP_Offset_Within_Limit(OP *mem_op1, OP *mem_op2, INT lower_bound, 
		       INT upper_bound)
{

  INT offset_opnd_num1 = TOP_Find_Operand_Use(OP_code(mem_op1), OU_offset);
  INT offset_opnd_num2 = TOP_Find_Operand_Use(OP_code(mem_op2), OU_offset);

  TN *offset1, *offset2;

  offset1 = (offset_opnd_num1 < 0) ? NULL : OP_opnd(mem_op1, offset_opnd_num1);
  offset2 = (offset_opnd_num2 < 0) ? NULL : OP_opnd(mem_op2, offset_opnd_num2);

  if (offset1 && offset2 && 
      TN_has_value(offset1) && TN_has_value(offset2)) {
    return within_bounds(TN_value(offset1), 
			 TN_value(offset2), 
			 lower_bound, 
			 upper_bound);
  }
   
  return FALSE;
}

// Bug 8298: Split_BB() splits a basic block into blocks that are 
// (approximately) half or 3/4-th the Split_BB_Length value in 
// length each. Hence we do a (/2) - as the more conservative of the
// two divisions. We retain the (- 60).

#define Large_BB(bb, loop) \
        (LOOP_DESCR_nestlevel((loop)) == 0 && \
        BB_length((bb)) >= (Split_BB_Length/2 - 60))

// =======================================================================
// Check_If_Ignore_BB
// Placeholder for all compile speed heuristics. If any of heuristics 
// match, return TRUE (i.e. the block is avoided any further processing).
// =======================================================================
static BOOL
Check_If_Ignore_BB(BB *bb, LOOP_DESCR *loop)
{
  // Avoid processing infrequent basic blocks
  if (BB_freq(bb) < 0.02) 
    return TRUE;

  // Avoid processing large blocks which are not part of any loop. We
  // expect that HBS would have come up with a good schedule.
#ifdef TARG_IA64
  if (LOOP_DESCR_nestlevel(loop) == 0 && BB_length(bb) >= (Split_BB_Length - 60))
#else
  if (Large_BB(bb, loop))
#endif
    return TRUE;

#if 0
  // If PRE-GCM enabled, invok POST-GCM phase, only if the register 
  // allocator has inserted any spill instructions (i.e. reset the bb_schedule
  // flag. Otherwise, we expect that the PRE-GCM phase has explored all
  // previous possibilities.
  if (!GCM_POST_Force_Scheduling && 
      (Cur_Gcm_Type & GCM_AFTER_GRA) &&
      GCM_PRE_Pass_Enabled) {
    if (BB_scheduled(bb) && !BB_scheduled_hbs(bb))
      return TRUE;
  }
#endif

  return FALSE;
}

// =======================================================================
// Macros to set the limit for applying the pointer speculation
// heurtistics (i.e. Eager_Ptr_Deref_Spec and Null_Ptr_Deref_Spec). 
// TODO: fix an appropriate limit.
// =======================================================================

inline BOOL Similar_Ptr_Offset_ok(OP *cur_op, OP *deref_op) {

  INT cur_offset_num = TOP_Find_Operand_Use(OP_code(cur_op), OU_offset);
  INT deref_offset_num = TOP_Find_Operand_Use(OP_code(deref_op), OU_offset);
  INT deref_base_num = TOP_Find_Operand_Use (OP_code(deref_op), OU_base);
  INT cur_base_num = TOP_Find_Operand_Use(OP_code(cur_op), OU_base);

  TN *deref_base_tn = OP_opnd(deref_op, deref_base_num);
  TN *cur_base_tn = OP_opnd(cur_op, cur_base_num);

  if (cur_offset_num < 0 && deref_offset_num < 0) {
    DEF_KIND kind;
    OP *defop = TN_Reaching_Value_At_Op(cur_base_tn, cur_op, &kind, TRUE);
    if (defop && OP_iadd(defop) && kind == VAL_KNOWN) {
      TN *defop_offset_tn = OP_opnd(defop, 1);
      TN *defop_base_tn = OP_opnd(defop, 2);
      if (defop_base_tn == deref_base_tn && TN_has_value(defop_offset_tn))
	return TRUE;
    }
  } else {

    // #669168: Set the legal offset thresholds based on Opt level.
    // at -O2, range is 32 (-16 .. +16)
    // >= O3, range is 128 (-64 .. +64)

    return (CG_opt_level > 2) ? 
      OP_Offset_Within_Limit(cur_op, deref_op, -64, 64) :
      OP_Offset_Within_Limit(cur_op, deref_op, -16, 16);
  }

  return FALSE;
}

#define Null_Ptr_Offset_ok(op)    OP_Offset_Within_Limit(op, -1, 256)

// =======================================================================
// Similar_Ptr_Addrs_Match
// Return TRUE if the <pred_op> and <succ_op> have conflicting addresses
// assumes that <pred_op> and <succ_op> are memory ops
// a more generic routine needs to be used (eg. a generic version of same_addr
// routine in cg_dep_graph) should be used. If I try to use its current
// version, get into all sorts of dep-graph intrinsic problems.
// =======================================================================
static BOOL
Similar_Ptr_Addrs_Match (OP *pred_op, OP *succ_op)
{
  if (OP_unalign_mem(pred_op) || OP_unalign_mem(succ_op))
 	return TRUE;// don't know thereby have to make a conservative estimate

  INT pred_base_num = TOP_Find_Operand_Use(OP_code(pred_op), OU_base);
  INT succ_base_num = TOP_Find_Operand_Use(OP_code(succ_op),  OU_base);

  TN *pred_base_tn = OP_opnd(pred_op, pred_base_num); 
  TN *succ_base_tn = OP_opnd(succ_op, succ_base_num);

  BOOL identical = FALSE;
  if (	TNs_Are_Equivalent(pred_base_tn, succ_base_tn) &&
	CG_DEP_Mem_Ops_Offsets_Overlap(pred_op, succ_op, &identical))
    return TRUE;

  // if the base register is $0, turn off further analysis. Cannot
  // assume that loads from different offsets of $0 are related.
  // (see pv669168 for more details).
  if (TN_is_zero_reg(pred_base_tn) && TN_is_zero_reg(succ_base_tn))
    return TRUE;

  return FALSE;
}


// =======================================================================
// OP_Has_Restrictions
// Return TRUE if the <op> has a special meaning w.r.t <source_bb> or
// <target_bb> such that it should not be considered as candidate for code 
// motion. For example, stack-adjustment, glue-copy OPs,.. etc are checked
// here.
// =======================================================================
static BOOL 
OP_Has_Restrictions(OP *op, BB *source_bb, BB *target_bb, mINT32 motion_type)
{
  if (CGTARG_Is_OP_Intrinsic(op)) return TRUE;

#ifdef TARG_X8664
  if( OP_icmp(op) )
    return TRUE;

  if( TOP_is_change_rflags( OP_code(op) ) ||
      OP_reads_rflags( op ) )
    return TRUE;
#endif

  if (OP_has_hazard(op)) return TRUE;

  if ((cur_hbs_type & HBS_BEFORE_GRA) != 0 && OP_no_move_before_gra(op)) 
    return TRUE;

  // If <OP> accesses rotating register banks, return FALSE.
  if (OP_access_reg_bank(op)) return TRUE;

  if ((BB_entry(source_bb) && BB_entry_sp_adj_op(source_bb) == op) ||
     (BB_exit(source_bb) && BB_exit_sp_adj_op(source_bb) == op))
    return TRUE;

  // SP def OPs are a trouble.
  if (OP_Defs_TN(op, SP_TN) || OP_Defs_Reg(op, REGISTER_CLASS_sp, REGISTER_sp))
    return TRUE;

  // Do extra processing for BRP instructions.
  if (OP_branch_predict(op)) {
    UINT64 num_insts = 0;
    BB *prev_bb;
    for (prev_bb = source_bb; prev_bb && prev_bb != target_bb; 
	 prev_bb = BB_prev(prev_bb)) {
      num_insts += BB_length(prev_bb);
    }
    
    // It's assumed that about 1/3 nops will be added later, so include the
    // expansion factor. The below condition checks that BRP instructions
    // are not scheduled too early, such that they violate the offset
    // restrictions.

    if ((num_insts * 1.3 * INST_BYTES) >= DEFAULT_BRP_BRANCH_LIMIT)
      return TRUE;
  }

  // Even SP ref OPS are a trouble for circular scheduling
  if (motion_type & GCM_CIRC_ABOVE) {
     if (OP_Refs_TN(op,  SP_TN) || 
	 OP_Refs_Reg(op, REGISTER_CLASS_sp, REGISTER_sp)) return TRUE;

     // No need to circular-schedule BRP ops.
     if (OP_branch_predict(op)) return TRUE;

     if (OP_memory(op)) {
       if (OP_no_alias(op) || OP_prefetch(op)) return FALSE;

       // If <PROC> has delayed exception mechanism, either by speculative
       // loads or predication, return FALSE.
       if (PROC_has_delayed_exception() && OP_has_predicate(op)) { 
	 return FALSE;
       } else {

	 // TODO: Need to add more relaxation rules w.r.t memory accesses 
	 // which can be verified as safe. Disallow memory references which 
	 // belong to KIND_ARRAY type since circular scheduling them can lead 
	 // to out-of-bound accesses. Need to change constant operand 
	 // reference by a generic item. 

	 if (TN_is_symbol(OP_opnd(op, 1)) && 
	     ST_class(TN_var(OP_opnd(op, 1))) == CLASS_VAR && 
	     TY_kind(ST_type(TN_var(OP_opnd(op, 1)))) != KIND_ARRAY) 
	   return FALSE;
       }
       return TRUE;
     } 
  }

  // for the PRE-GCM stage, don't let local TN's which are ideal 
  // candidates for peephole opportunities (eg. copy ops) be global TN's.

  if (!Ignore_TN_Dep && (OP_copy(op) || OP_glue(op))) return TRUE;

  //TODO: need to check if this is not too conservative.
  INT i;
  for (i = 0; i < OP_results(op); ++i) {
    TN *result_tn = OP_result(op, i);
    ISA_REGISTER_CLASS result_cl = TN_register_class (result_tn);

        // For Pre-GCM stage, return FALSE, if TN is dedicated or homeable.

    if ((!Ignore_TN_Dep && 
	(TN_is_dedicated(result_tn) || TN_is_gra_homeable(result_tn))) ||

	// For Post-GCM stage (or SWP rotating reg allocated), return FALSE,
	// if TN is rotating register type.

	((Ignore_TN_Dep || TN_is_register(result_tn)) &&
	 REGISTER_Is_Rotating(result_cl, TN_register(result_tn)))) 

      return TRUE;
    }

  // TODO: Need to prune this further. Need to analyze home locations, 
  // analyze any potential overlaps etc. 
  for (i = 0; i < OP_opnds(op); ++i) {
    TN *opnd_tn = OP_opnd(op, i);
    if (TN_is_constant(opnd_tn)) continue;

    ISA_REGISTER_CLASS opnd_cl = TN_register_class (opnd_tn);

    // homeable TNs not included for Pre-GCM phase.
    if ((!Ignore_TN_Dep && TN_is_gra_homeable(opnd_tn)) ||
	
	// SWP rotating register TNs are not included.
	((Ignore_TN_Dep || TN_is_register(opnd_tn)) &&
	 REGISTER_Is_Rotating(opnd_cl, TN_register(opnd_tn)))) 
      return TRUE;
  }

#ifdef TARG_X8664
  // If OP must use or define a specific real register, then don't move it in
  // order to avoid possible conflicts with OPs in the target BB which also use
  // that real register.  For example, for the candidate OP "TN105 = ld32_m",
  // which writes to %rax, and this target BB:
  //
  //		  call		; returns value in %rax
  //  target_BB:  TN100 = %rax	; existing OP in target_bb which also uses %rax
  //		  jne		; xfer_op
  //
  // During the GCM_BEFORE_GRA pass, if GCM moves ld32_m to target_BB, GCM
  // would insert it before "TN100 = %rax", giving bad code since ld32_m kills
  // the previous %rax value.  Bug 9466.
  ASM_OP_ANNOT* asm_info = (OP_code(op) == TOP_asm) ?
    (ASM_OP_ANNOT*) OP_MAP_Get(OP_Asm_Map, op) : NULL;

  for (int i = 0; i < OP_opnds(op); i++) {
    ISA_REGISTER_SUBCLASS subclass = asm_info ?
      ASM_OP_opnd_subclass(asm_info)[i] : OP_opnd_reg_subclass(op, i);
    if (Single_Register_Subclass(subclass) != REGISTER_UNDEFINED)
      return TRUE;
  }
  for (int i = 0; i < OP_results(op); i++) {
    ISA_REGISTER_SUBCLASS subclass = asm_info ?
      ASM_OP_result_subclass(asm_info)[i] : OP_result_reg_subclass(op, i);
    if (Single_Register_Subclass(subclass) != REGISTER_UNDEFINED)
      return TRUE;
  }
#endif

  return FALSE;
}

// =======================================================================
// Can_Do_Safe_Predicate_Movement
// Checks to see if an unsafe <cur_op> can be moved from <src_bb> to <tgt_bb>
// by converting it to safe op by doing instruction predication (i.e. guarding
// it by a control predicate).
// =======================================================================
BOOL
Can_Do_Safe_Predicate_Movement(OP        *cur_op, 
			       BB        *src_bb, 
			       BB        *tgt_bb, 
			       mINT32    motion_type)
{

  // For circular scheduling, need to make sure that the branch predicate for
  // the target block executes under non p0 conditions and has a compare inst
  // defined. 
  if (motion_type & (GCM_CIRC_ABOVE | GCM_SPEC_ABOVE)) {

    // if <cur_op> has an existing non p0 qualifying predicate and is
    // not safe-speculatable, return FALSE.
    // TODO: Add support to allow speculation of predicated instructions
    // as well. 
    if (OP_has_predicate(cur_op) &&
	!TN_is_true_pred(OP_opnd(cur_op, OP_PREDICATE_OPND)) &&
	!CGTARG_Can_Be_Speculative(cur_op)) return FALSE;

    OP *tgt_br_op = BB_branch_op(tgt_bb);
    if (tgt_br_op && OP_has_predicate(tgt_br_op)) {

      // TODO: Check if <src_bb> has a unique predecessor <tgt_bb>. For,
      // more than one predecessor cases, need to compute new predicate
      // expression and allow movement.
      if (!TN_is_true_pred(OP_opnd(tgt_br_op, OP_PREDICATE_OPND)) &&
	  (BB_Unique_Source(src_bb) ==  tgt_bb)) {
	TN *tn1, *tn2;
	OP *cmp_op;
	CGTARG_Analyze_Compare(tgt_br_op, &tn1, &tn2, &cmp_op);

	// if <cmp_op> found and has both result (true/false) predicate
	// registers defined, return TRUE.
	if (cmp_op && cmp_op != tgt_br_op && OP_results(cmp_op) == 2) {
	  TN *r0 = OP_result(cmp_op, 0);
	  TN *r1 = OP_result(cmp_op, 1);

	  // if <cmp_op> has a non p0 qualifying predicate, need to be
	  // conservative, i.e. it has other predicate expressions that can
	  // satisfy the condition. Unless, we can accurately determine
	  // these expressions, be conservative at the moment.

	  if (OP_has_predicate(cmp_op) &&
	      !TN_is_true_pred(OP_opnd(cmp_op, OP_PREDICATE_OPND)))
	    return FALSE;

	  // For GCM phase (before register allocation), we need to guarantee
	  // that (1) either both the TNs are globals, or (2) need to make
	  // sure if they indeed become global TNs, update the GTN sets
	  // accordingly.

	  if (!Ignore_TN_Dep && (!TN_is_global_reg(r0) || !TN_is_global_reg(r1)))
	    return FALSE;

	  if (!TN_is_true_pred(r0) && !TN_is_true_pred(r1)) return TRUE;

	}
      }
    }
  }

  return FALSE;
}

// =======================================================================
// Eager_Ptr_Deref_Spec
// checks to see if a pointer reference in <src> can be moved to <dest>.
// A pointer referencing the same base address with a slight difference in
// the offsets among two references in <src> and <dest> can be speculated.
// This is controlled by <Eager_Ptr_Deref>. This routine is called only
// when <CG_DEP_Mem_Ops_Alias> returns TRUE.
// =======================================================================
static BOOL
Eager_Ptr_Deref_Spec(OP *deref_op, BB *dest_bb, BOOL forw)
{
  OP *cur_op;
  OP *limit_op;
  BOOL valid_addrs_found = FALSE;

  limit_op = NULL;
  TN *deref_base_tn;
 
  INT dbase_num = TOP_Find_Operand_Use(OP_code(deref_op), OU_base);
  INT doffset_num = TOP_Find_Operand_Use(OP_code(deref_op), OU_offset);

#ifdef KEY
  deref_base_tn = dbase_num >= 0 ? OP_opnd(deref_op, dbase_num) : NULL;
#else
  deref_base_tn = OP_opnd(deref_op, dbase_num);
#endif // KEY

  for (cur_op = (forw) ? BB_last_op(dest_bb) : BB_first_op(dest_bb);
       cur_op && cur_op != limit_op;
       cur_op = (forw) ?  OP_prev(cur_op) : OP_next(cur_op)) {

    if (OP_dummy(cur_op)) continue;
   
    // collect all memory references in the <dest> bb 
    
    if (OP_load(cur_op) || OP_store(cur_op)) { 

      INT cbase_num = TOP_Find_Operand_Use(OP_code(cur_op), OU_base);
      INT coffset_num = TOP_Find_Operand_Use(OP_code(cur_op), OU_offset);

#ifdef KEY
      TN *cur_base_tn = cbase_num >= 0 ? OP_opnd(cur_op, cbase_num) : NULL;
#else
      TN *cur_base_tn = OP_opnd(cur_op, cbase_num);
#endif // KEY
      TN *cur_result_tn = OP_load(cur_op) ? OP_result(cur_op, 0) : NULL;

      if (!Similar_Ptr_Addrs_Match(cur_op, deref_op)) {
	if (Similar_Ptr_Offset_ok(cur_op, deref_op)) {
	  
	  if (coffset_num < 0 && doffset_num < 0) {
	    valid_addrs_found = TRUE;
	    break;
	  } else {
	    // Need to check if the OP doesn;t modify the base.

	    BOOL modifies_base = cur_result_tn &&
#ifdef KEY
	      ( cur_base_tn != NULL ) &&
#endif // KEY
	      TNs_Are_Equivalent(cur_result_tn, cur_base_tn);

	    if (!modifies_base && 
		TNs_Are_Equivalent(deref_base_tn, cur_base_tn)){ 
	      valid_addrs_found = TRUE;
	      break;
	    }
	  }
	}
      } else {
	// no need to look further since <deref_op> can't move past
	// <cur_op> anyway.
	valid_addrs_found = FALSE;
	break;
      }
    }

    // if the memory reference is being modified by this <op>, removed it 
    // from the list of valid memory references.

#ifdef TARG_IA64
    BOOL base_redef = FALSE;
    for (INT i = 0; i < OP_results(cur_op); ++i) {
      TN *result = OP_result(cur_op,i);
      if (Ignore_TN_Dep) {
	REGISTER result_reg = TN_register(result);

	// If there was a previous update of <result_reg>, remove it.
	// Sometimes, this may be the first occurence of <result_reg>, so
	// discontinue further.
	if (result_reg == TN_register(deref_base_tn)) 
	  base_redef = TRUE;
      } else {
	if (TNs_Are_Equivalent(result, deref_base_tn)) base_redef = TRUE;
      }
    }

    // No need to look further if there exists a base redef.
    if (base_redef) {
      valid_addrs_found = FALSE;
      break;
    }
#else // TARG_IA64
#ifdef KEY
    if( deref_base_tn != NULL )
#endif // KEY
      {
	BOOL base_redef = FALSE;
	for (INT i = 0; i < OP_results(cur_op); ++i) {
	  TN *result = OP_result(cur_op,i);
	  if (Ignore_TN_Dep) {
	    REGISTER result_reg = TN_register(result);

	    // If there was a previous update of <result_reg>, remove it.
	    // Sometimes, this may be the first occurence of <result_reg>, so
	    // discontinue further.
	    if (result_reg == TN_register(deref_base_tn)) 
	      base_redef = TRUE;
	  } else {
	    if (TNs_Are_Equivalent(result, deref_base_tn)) base_redef = TRUE;
	  }
	}
	
	// No need to look further if there exists a base redef.
	if (base_redef) {
	  valid_addrs_found = FALSE;
	  break;
	}
      }
#endif

    // use CG_DEP_Call_Aliases interface to determine if the
    // deref_op is being read/write in the called procedure. uses info
    // from WOPT alias manager. 
    if (OP_call(cur_op) && 
	!CG_DEP_Can_OP_Move_Across_Call(deref_op, cur_op,forw,Ignore_TN_Dep)) {
      valid_addrs_found = FALSE;
      break;
    }
  }

  // If there exits a valid address, return TRUE.
  if (valid_addrs_found) return TRUE;

  return FALSE;
}

// =======================================================================
// Null_Ptr_Deref_Spec
// checks to see if there is any null pointer reference in path from <src> 
// to <dest>. Null pointer tests are used for checking the bounds or exit
// conditions. Any other valid pointer derefence can be speculated beyond it.
// The test isn't really necessary since this special case is alreday 
// incorporated in the general framework. the main reason is if this condition
// is used in conjunction with other conditions (as is very frequent), we can
// safely eliminate the branch condition (upon valid page references) in the 
// block. This is controlled by <Eager_Null_Ptr_Deref>.
// =======================================================================
static BOOL
Null_Ptr_Deref_Spec(OP *deref_op, BB *src, BB *dest)
{
  REGISTER condition_reg; // reg value which is being compared with zero
  TN *condition_tn;       // tn value which is being compared with zero
  BOOL taken_path;

  OP *branch_op = BB_branch_op(dest);
  // <dest> block doesn't contain any conditional branch
  if (branch_op == NULL || !OP_cond(branch_op)) return FALSE;

  // this condition is too restrictive and should be removed. 
  // if (!Null_Ptr_Offset_ok(deref_op)) return FALSE;

  TN *opnd1, *opnd2;
  INT variant;

  // Invoke the target-independent interface to analyze the branch.
  variant = CGTARG_Analyze_Branch(branch_op, &opnd1, &opnd2);

  // Some branches only have one operand, e.g. mips branch on fcc,
  // these aren't interesting.
  if (opnd2 == NULL) return FALSE;

  Is_True(opnd1, ("expected two operand TNs from CGTARG_Analyze_Branch"));

  // either of the branch operands must be zero to qualify for this case
  // and determine the condition reg whose value is being compared with zero
  if (TN_is_zero(opnd1)) {
    condition_tn = opnd2;
  } else if (TN_is_zero(opnd2)) {
    condition_tn = opnd1;
  } else {
    return FALSE;
  }

  if (Ignore_TN_Dep)
    condition_reg = TN_register(condition_tn);

  // now determine the taken path
  switch (variant) {
    // beq, beql:
  case V_BR_I8EQ:
    taken_path = FALSE;
    break;

    // bne, bnel:
    // bgez, bgezl, bgtz, bgtzl,
    // blez, blezl, bltz, bltzl
  case V_BR_I8NE:
  case V_BR_I8GE:
  case V_BR_I8GT:
  case V_BR_I8LE:
  case V_BR_I8LT:
    taken_path = TRUE;
    break;

  default:
    condition_reg = 0;
    taken_path = FALSE;
  }
  
  // need to make sure that the condition in <dest> is actually the boundary 
  // test condition for <op> in <src> and that <dest> post-dominates <src>.
  BOOL post_dom = BS_MemberP (BB_dom_set(src), BB_id(dest)) &&
		  !BS_MemberP (BB_pdom_set(dest), BB_id(src));
  if (post_dom || (dest == BB_prev(src))) { 
	if (taken_path) return FALSE;
  } else {
	if (!taken_path) return FALSE;
  }

#ifdef TARG_X8664
  const int base_idx = TOP_Find_Operand_Use( OP_code(deref_op),OU_base );
  if( base_idx < 0 )
    return FALSE;
  TN* base_tn = OP_opnd( deref_op, base_idx );
  TN* offset_tn = OP_opnd( deref_op,
			   TOP_Find_Operand_Use( OP_code(deref_op),OU_offset ) );
#else
  // !TARG_X8664
  TN *base_tn = OP_load(deref_op) ? OP_opnd(deref_op, 0) : 
				    OP_opnd(deref_op, 1);

  TN *offset_tn = OP_load(deref_op) ? OP_opnd(deref_op, 1): 
				      OP_opnd(deref_op, 2);
#endif  // TARG_X8664

  // TODO: actually, any positive constant offsets which fit into page 
  // boundary can be considered
  if (Ignore_TN_Dep) {
      REGISTER base_reg = TN_register(base_tn);
      if (base_reg == condition_reg && TN_value(offset_tn) >= 0)
	return TRUE;
  } else {
    if (TN_number(base_tn) == TN_number(condition_tn) && 
	TN_value(offset_tn) >= 0)
      return TRUE;
  }

  return FALSE;
}

// =======================================================================
// Can_Mem_Op_Be_Moved
// checks to see if a <mem_op> can be moved from <src> to <dest>. For 
// speculative movement this is in addition to
// CGTARG_Can_Be_Speculative (cgtarget.c) and is specific to memory ops.
// Other pointer heuristics are applied here before deciding whether it's 
// safe or not. This is controlled by <Enable_Spec_Loads>.
// =======================================================================
static BOOL
Can_Mem_Op_Be_Moved(OP *mem_op, BB *cur_bb, BB *src_bb, BB *dest_bb, 
		    mINT32 motion_type)
{

  if ((Cur_Gcm_Type & GCM_AFTER_GRA) && !GCM_POST_Spec_Loads) 
    return FALSE;

  if ((Cur_Gcm_Type & GCM_BEFORE_GRA) && !GCM_PRE_Spec_Loads)
    return FALSE;
  
  // TODO: first, we filter out the most easy cases (for fast compile time). 
  // I am sure there will be more to filter out (for other motion types as 
  // well) in the future.
  BOOL forw = motion_type & (GCM_EQUIV_FWD | GCM_SPEC_ABOVE | 
			     GCM_DUP_ABOVE | GCM_CIRC_ABOVE);
  if (motion_type & (GCM_SPEC_ABOVE | GCM_SPEC_BELOW | GCM_CIRC_ABOVE)) {

    // stores can't be speculated
    if (OP_store(mem_op)) 
	return FALSE;
  }

#ifdef TARG_X8664
  /* Do not allow a load operation under -mcmodel=medium to across a
     call, since such load will overwrite %rax that holds the return value.
     (bug#2419)

     TODO ???:
     The right fix should be done inside OP_To_Move(), but <failed_reg_defs>
     is not updated if an op has restrictions.
   */
  if( mcmodel >= MEDIUM &&
      !Ignore_TN_Dep    &&
      forw ){
    BB* prev = BB_prev( src_bb );

    if( prev != NULL &&
	BB_call( prev ) ){
      const TOP top = OP_code(mem_op);
      if( top == TOP_ld8_m  || top == TOP_ld16_m ||
	  top == TOP_ld32_m || top == TOP_ld64_m )
	return FALSE;
    }
  }
#endif

  // volatile ops shouldn't be touched at all
  if (OP_volatile(mem_op)) return FALSE;

  // prefetches don't alias with anything
  if (OP_prefetch(mem_op)) return TRUE;

  OP *cur_op, *br_op, *limit_op;
  BOOL definite = FALSE;
  // we only look for memory dependences here; register dependences and 
  // call dependences will be considered in the later phase only after we 
  // ensure that there aren't any memory dependences.
  OP *last_op = BB_last_op(dest_bb);
  if (cur_bb == dest_bb)
    // check that the inst can be moved to <dest>
    limit_op = (br_op = BB_xfer_op(dest_bb)) ? OP_prev(br_op) :
	((last_op = BB_last_op(dest_bb)) ? OP_prev(last_op) : last_op);
  else 
    limit_op = (cur_bb == src_bb) ? mem_op : NULL;

  BOOL read_read_dep;
  for (cur_op = ((forw && cur_bb != src_bb) || (!forw && cur_bb == src_bb)) ?
       BB_last_op(cur_bb) : BB_first_op(cur_bb);
       cur_op && cur_op != limit_op;
       cur_op = ((forw && cur_bb != src_bb) || (!forw && cur_bb == src_bb)) ?
       OP_prev(cur_op) : OP_next(cur_op)) {

    // dummy ops don't alias with anything
    if (OP_dummy(cur_op)) continue;

    // exclude prefetch ops with zero omegas for circular scheduling.
    if (OP_prefetch(cur_op)) {
      if (motion_type & GCM_CIRC_ABOVE) {

	// found PF_POINTER from memop
	WN  *memwn = Get_WN_From_Memory_OP(mem_op);
	PF_POINTER *pf_ptr1 = memwn ?
	  (PF_POINTER *)WN_MAP_Get(WN_MAP_PREFETCH, memwn) : NULL;

	// found PF_POINTER from prefetch
	WN *wn = Get_WN_From_Memory_OP( cur_op);
	PF_POINTER *pf_ptr2 = wn ? (PF_POINTER *) WN_MAP_Get(WN_MAP_PREFETCH,wn) : NULL;
	
	if (pf_ptr1 == pf_ptr2)
	  return FALSE;
	
      } else continue;
    }
    
    if (OP_memory(cur_op) 
#ifdef TARG_X8664
	|| OP_load_exe(cur_op)
#endif
	) {
#ifdef TARG_X8664
      read_read_dep = ( OP_load(cur_op) || OP_load_exe(cur_op) ) &&
	( OP_load(mem_op) || OP_load_exe(mem_op) );
#else
      read_read_dep = OP_load(cur_op) && OP_load(mem_op);
#endif

      // No need to process read-read memory dependences
      if (!read_read_dep &&
	  CG_DEP_Mem_Ops_Alias(cur_op, mem_op, &definite)) {

	return FALSE;

      }
    }
  }

  // #642858, #641258;
  // If we have reached this point, have convinced ourselves, that there
  // exists no memory ops in <cur_bb> which aliases with <deref_op> with
  // non-matching base addresses.

  return TRUE;
}

// =======================================================================
// Can_Inst_Be_Moved
// checks to see if <op> inst_type can be moved or not.
// =======================================================================
static BOOL
Can_Inst_Be_Moved (OP *op, VECTOR succs_vector, INT succ_num)
{
  // If there is only one successor, it is safe to move the <op>.
  if (VECTOR_count(succs_vector) == 1) return TRUE;

  if (!CGTARG_Can_Be_Speculative (op)) return FALSE;

  if (OP_has_hazard(op) || OP_imul(op) || OP_idiv(op)) {
    return FALSE;
  }

  // If the <op> has a result, check if the result-register is live on
  // entry to any of the other successors.
  for (INT i = 0; i < OP_results(op); ++i) {
    TN *result = OP_result(op,i);
    ISA_REGISTER_CLASS result_cl = TN_register_class (result);
    REGISTER result_reg = TN_register (result);
    for (INT i = 0; i < VECTOR_count(succs_vector); i++) {
      if (i == succ_num) continue;
      BBLIST *succ_bl = (BBLIST *) VECTOR_element(succs_vector, i);
      BB *succ_bb = BBLIST_item(succ_bl);
      if (REG_LIVE_Into_BB (result_cl, result_reg, succ_bb)) return FALSE;
    }
  }
  return TRUE;
}

// =======================================================================
// Find_Vacant_Slots_BB
// Determines the vacant slots present in <bb> w.r.t <targ_alignment>
// It dynamically recomputes the <bb> start_pc address from it's prior
// scheduled predecessors and 
// =======================================================================
static INT16
Find_Vacant_Slots_BB(BB *bb, INT targ_alignment)
{
  BBLIST *bblist;
  INT16 vacant_slots = 0;

  BBSCH *bbsch = (BBSCH *)BB_MAP_Get (bbsch_map, bb);
  // ignore if it's a loophead bb. since code motion is never done beyond
  // loop boundaries, it's isn;t necessary to recompute it. 
  // TODO: might need to revisit for bottom-loading (if at all)
  if (!BB_loop_head_bb(bb)) {
    FOR_ALL_BB_PREDS(bb, bblist) {
      BB *pred_bb = BBLIST_item(bblist);
      BBSCH *pred_bbsch = (BBSCH *)BB_MAP_Get (bbsch_map, pred_bb);

      // determine the MAX bb_start pc addresses taking into account
      // the start address and the number of real ops in predecessor bbs
      if (bbsch && pred_bbsch) {
	BBSCH_bb_start_pc(bbsch) = 
                     MAX (BBSCH_bb_start_pc(bbsch),
			  BBSCH_bb_start_pc(pred_bbsch) +
			  BBSCH_num_real_ops(pred_bbsch));
	BB_MAP_Set (bbsch_map, bb, bbsch);
      }
    }
  }
					
  // vacant_slots is used to determine how many vacant_slots are
  // available if at all to align this <bb>.
  vacant_slots = (targ_alignment - ((BBSCH_bb_start_pc(bbsch) + 
				     BBSCH_num_real_ops(bbsch)) %
				    targ_alignment));

  return vacant_slots;
}

// =======================================================================
// Find_OP_For_Delay_Slot
// placeholder to pick the best op from <bb> which can be put in the
// branch delay slot. currently, it just returns the first instruction 
// of the basic block.
// =======================================================================
static OP*
Find_OP_For_Delay_Slot (BB *bb)
{
  return First_Inst_Of_BB (bb);
}

// =======================================================================
// Fill_From_Successor
// checks to see if <xfer_op> can be filled and put in br_delay slot in <bb>
// =======================================================================
static BOOL
Fill_From_Successor (BB *bb, OP *xfer_op, VECTOR succs_vector, INT succ_num)
{
  BBLIST *succ_bl = (BBLIST *) VECTOR_element(succs_vector, succ_num);
  BB *succ_bb = BBLIST_item(succ_bl);
  OP *first_op = Find_OP_For_Delay_Slot (succ_bb); 

  // pv566961. if <succ_bb> is a loophead block and that <bb> and <succ_bb>
  // are enclosed within the same region, avoid filling the delay slot 
  // of the branch. This is due to the fact that region insertion phase 
  // currently doesn't insert empty prelude/postlude blocks to the 
  // currently processed region.

  if ( BB_rid(succ_bb) &&
       BB_id(succ_bb) == BB_id(REGION_First_BB)) return FALSE;

  typedef enum { 
    FILL_NONE,
    FILL_DELETE,
    FILL_MOVE,
    FILL_DUP
  } FILL_TYPE;

  FILL_TYPE fill_type = FILL_NONE;
  if (Is_Delay_Slot_Op (xfer_op, first_op) &&
      !OP_Has_Restrictions(first_op, bb, succ_bb, GCM_DUP_ABOVE)) {
    TOP blikely_opcode;
    BOOL fallthru = (succ_bb == BB_next(bb));
    BOOL single_pred = (BBlist_Len (BB_preds(succ_bb)) == 1);
    if (Can_Inst_Be_Moved (first_op, succs_vector, succ_num)) {
      // For jr instruction we can only move the target instruction. Don't
      // try to copy target instruction since we don't want to change
      // the branch target in the switch table.
      if (single_pred) {
        fill_type = FILL_MOVE;
      }
      else if (!(OP_ijump(xfer_op) && !OP_call(xfer_op))) {
	// deleting the nop in the delay slot can be done only if the 
	// target of the branch is something other than the fallthru bb.
        if (fallthru && VECTOR_count(succs_vector) != 1) {
	  fill_type = FILL_DELETE;
        }
        else {
	  // if it's an unconditional branch with unique successor <succ_bb>
	  // and that its unique predecessor and <succ_bb> are equivalent
	  // means that performing the <FILL_DUP> is unnecessary since the
	  // unconditional branch in <bb> can effectively be eliminated by
	  // cflow. assuming that <unique_pred> are equivalent maybe too
	  // STRICT. need to investigate further.
	  if (OP_br(xfer_op) && !OP_cond(xfer_op) && 
	     (VECTOR_count(succs_vector) == 1) &&
	     (BBlist_Len (BB_preds(bb)) == 1)) {
	    BB *unique_pred = BB_Unique_Predecessor (bb);
	    // kludge to match if the frequencies of the <unique_pred> and
	    // the <succ_bb> are same without recomputing the dominators
	    // and post-dominators (compspeed purposes)
	    if (unique_pred && (BB_freq(unique_pred) == BB_freq(succ_bb))) {
	      fill_type = FILL_NONE;
	      Run_Cflow_Delay = TRUE;
	    }
	    else fill_type = FILL_DUP;
	  }
  	  else fill_type = FILL_DUP;
	}
      }
    }
    else if (!fallthru && 
	     CGTARG_Can_Change_To_Brlikely (xfer_op, &blikely_opcode) &&
	     CGTARG_Use_Brlikely (BBLIST_prob(succ_bl))
	     ) 
    {
      OP_Change_Opcode(xfer_op, blikely_opcode);
      fill_type = (single_pred) ? FILL_MOVE : FILL_DUP;
      if (Trace_Fill_Delay_Slots) {
	#pragma mips_frequency_hint NEVER 
        fprintf (TFile, "DELAY_FILL>   changed to BRLIKELY\n");
      }
    }
  }

  OP *delay_op = BB_last_op(bb);
  BB *from_bb;

  switch (fill_type) {
  case FILL_DELETE:
    if (Trace_Fill_Delay_Slots) {
      #pragma mips_frequency_hint NEVER
      fprintf (TFile,"DELAY_FILL> Remove delay-slot op in BB:%d freq = %#.2f\n",
	       BB_id(bb), BB_freq(bb));
      Print_OP_No_SrcLine (delay_op);
    }
    BB_Remove_Op (bb, delay_op);
    break;
  case FILL_MOVE:
    if (Trace_Fill_Delay_Slots) {
      #pragma mips_frequency_hint NEVER
      fprintf (TFile, "DELAY_FILL> Move OP: from BB:%d freq = %#.2f to BB:%d freq = %#.2f\n",
	       BB_id(OP_bb(first_op)), BB_freq(OP_bb(first_op)), 
	       BB_id(bb), BB_freq(bb));
      Print_OP_No_SrcLine (first_op);
    }
    BB_Remove_Op (bb, delay_op);
    from_bb = OP_bb(first_op);
    BB_Move_Op_To_End (bb, OP_bb(first_op), first_op);
    if (Is_BB_Empty (from_bb)) {
      Run_Cflow_Delay = TRUE;
    }
    break;
  case FILL_DUP:
    if (Trace_Fill_Delay_Slots) {
      #pragma mips_frequency_hint NEVER
      fprintf (TFile, "DELAY_FILL>   Copy OP: from BB:%d freq = %#.2f to BB:%d frequency = %#.2f\n",
	       BB_id(OP_bb(first_op)), BB_freq(OP_bb(first_op)), 
	       BB_id(bb), BB_freq(bb));
      Print_OP_No_SrcLine (first_op);
    }
    BB_Remove_Op (bb, delay_op);
    BB_Append_Op (bb, Dup_OP (first_op));
    TN *label_tn = NULL;
    for (INT i = 0; i < OP_opnds(xfer_op); ++i) {
      if (TN_is_label(OP_opnd(xfer_op,i))) {
	label_tn = OP_opnd(xfer_op,i);
	Set_OP_opnd (xfer_op, i, Gen_Adjusted_TN (label_tn, 4));
      }
    }
    FmtAssert (label_tn != NULL, ("Fill_From_Successor: no label in xfer_op"));
    break;
  }
  return (fill_type != FILL_NONE);
}

// ======================================================================
// GCM_Fill_Branch_Delay_Slots
// Try filling branch delay slots with the first instruction of one of 
// the target basic blocks.
// ======================================================================
void
GCM_Fill_Branch_Delay_Slots (void)
{
  // check if this optimization has been disabled.
  if (!GCM_Enable_Fill_Delay_Slots) return;

  Run_Cflow_Delay = FALSE;

  MEM_POOL *pool = &MEM_local_pool;
  L_Save ();

  Trace_Fill_Delay_Slots = Get_Trace (TP_GCM, 0x02);

  // compute live-in sets for registers.
  REG_LIVE_Analyze_Region ();

  for (BB *bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    BBLIST *bl;

    // needn't process already processed regions
    if ( (BB_rid(bb)) && (RID_level(BB_rid(bb)) >= RL_CGSCHED)) continue;

    // There might be an empty basic block left from an earlier phase.
    // This happens when LRA deletes instructions from a bb. We should
    // run cflow if we detect this case.
    if (Is_BB_Empty (bb)) {
      Run_Cflow_Delay = TRUE;
      continue;
    }

    if (BB_call(bb)) continue;
    OP *delay_op = BB_last_op(bb);
    // check if the last op in bb is a nop.
    if (delay_op == NULL || !OP_noop(delay_op)) continue;
    OP *xfer_op = OP_prev(delay_op);
    if (xfer_op == NULL || !OP_xfer(xfer_op)) continue;
    if (BB_exit(bb)) {

      // At this point we know bb's last two insts are "jr $31; noop".
      // If we haven't already noted we need cflow, see if there is
      // an opportunity to optimize a unconditional branch to a return.
      // There is an opportunity if the predecessor contains an
      // unconditional branch, and it will target the return inst
      // after delay slot filling is complete.
      if (!Run_Cflow_Delay && BB_length(bb) <= 3) {
	FOR_ALL_BB_PREDS (bb, bl) {
	  BB *pred = BBLIST_item(bl);
	  OP *br = BB_branch_op(pred);
	  if (   br 
	      && OP_br(br)
	      && !OP_cond(br)
	      && (BB_length(bb) == 2 || OP_noop(BB_last_op(pred)))
	  ) {
	    Run_Cflow_Delay = TRUE;
	    break;
	  }
	}
      }
      continue;
    }
    INT num_succs = BBlist_Len (BB_succs(bb));
    if (Trace_Fill_Delay_Slots) {
      #pragma mips_frequency_hint NEVER
      fprintf (TFile, "DELAY_FILL> Empty delay slot in BB:%d, succs:%d\n", 
		      BB_id(bb), num_succs);
    }
    // create a vector of all the successors, sorted in the order of 
    // decreasing frequency estimate.
    VECTOR succs_vector = VECTOR_Init (num_succs, pool);
    FOR_ALL_BB_SUCCS (bb, bl) {
      VECTOR_Sorted_Add_Element (succs_vector, (void *)bl, 
				 sort_by_edge_probability);
    }
    for (INT i = 0; i < num_succs; i++) {
      if (Fill_From_Successor (bb, xfer_op, succs_vector, i)) break;
    }
  }

  L_Free ();

  if ((Run_Cflow_Delay || Run_Cflow_GCM) && GCM_Enable_Cflow) {

    /* Filling delay slots may have caused BBs to become empty
     * and possibly resulted in branches to the next BB. So call
     * cflow with only branch optimizations and unreachable block
     * removal enabled. NOTE: the other opts work, but they probably
     * are't profitiable at this stage. so don't waste the compile time.
     */

    CFLOW_Optimize(CFLOW_BRANCH | CFLOW_UNREACHABLE | CFLOW_FILL_DELAY_SLOTS,
		   "CFLOW (from gcm)");
  }

  // Finish with reg_live after cflow so that it can benefit from the info.
  REG_LIVE_Finish ();
}

// ======================================================================
// Find_Limit_OP
// finds the <limit_op> of <tgt_bb>. The <limit_op> is the last <op> in
// <tgt_bb> the <cur_op> can be moved before. The calculation of <limit_op>
// also depends on whether its' PRE/POST GCM phase.
// ======================================================================
static OP*
Find_Limit_OP(OP *cur_op, BB *cur_bb, BB *src_bb, BB *tgt_bb)
{
  OP *limit_op;

  if (Cur_Gcm_Type & GCM_BEFORE_GRA) 
    limit_op = BB_copy_xfer_op(tgt_bb);
  else 
    limit_op = BB_xfer_op(tgt_bb);

  if (cur_bb == tgt_bb)
    limit_op = (limit_op) ? OP_prev(limit_op) : BB_last_op(cur_bb);
  else 
    limit_op = (cur_bb == src_bb) ? cur_op : NULL;
 
  return limit_op;
}

// ======================================================================
// Can_OP_Move
// returns TRUE if <op> can be moved thru all blocks between <src_bb> and
// <tgt_bb>. 
// ======================================================================
static BOOL
Can_OP_Move(OP *cur_op, BB *src_bb, BB *tgt_bb, BB_SET **pred_bbs, 
	    void *defs[2], void *uses[2], mINT32 motion_type,mUINT8 *spec_type)
{
   BB *cur_bb;
   REGISTER_SET mid_reg_defs[ISA_REGISTER_CLASS_MAX+1], 
		mid_reg_uses[ISA_REGISTER_CLASS_MAX+1];
   GTN_SET *mid_gtn_defs, *mid_gtn_uses;
   TN_SET *mid_tn_defs, *mid_tn_uses;

   // if the requested motion type is speculation and <cur_op> cannot be
   // speculated, return FALSE. need to consider possibility of doing safe
   // speculation (or predication) as well.
   BOOL can_spec = TRUE;
   BOOL safe_spec = FALSE;
   *spec_type = SPEC_NONE;
   if (motion_type & (GCM_SPEC_ABOVE | GCM_SPEC_BELOW | GCM_CIRC_ABOVE)) {
     // if cur_op is expensive, needn't speculate it.
     if (OP_Is_Expensive(cur_op)) return FALSE;
     if (!CGTARG_Can_Be_Speculative (cur_op)) {
       // excluding loads for further pruning.
       if (!OP_load(cur_op)) return FALSE;
       else {
	 can_spec = FALSE; // save the state

	 // At this point, <cur_op> is a possible unsafe load. Perform the 
	 // following safety checks in the prescribed order.
	 // (1) Do safe predication first, i.e if there exists a predicate 
	 //     which previously controlled the branch, use the predicate 
	 //     to control it's execution.
	 // (2) Do eager_ptr_speculation next. Check for safety references in 
	 //     the target block and deduce that it's safe to move.
	 // (3) Check for any target-specific speculative loads.
	 // (4) Check for NULL pointer speculation.

	 // if PROC has delayed exception mechanism, do specific tests.
	 // Check to see if instruction can be converted to safe OP using
	 // predication.

	 if (PROC_has_delayed_exception()) {
	   if (GCM_Pointer_Spec && GCM_Predicated_Loads &&
	       Can_Do_Safe_Predicate_Movement(cur_op, src_bb, tgt_bb, motion_type)) {
	     safe_spec = TRUE;
	     Set_PSAFE_PTR_SPEC(*spec_type);
	   }
	 } 

	 // Assumes OP_load only. Invoke eager_ptr_speculation routine 
	 // to check if there exist any valid memory references in 
	 // target_block which match <cur_op>.
	 
	 if (GCM_Pointer_Spec) {
	   if (GCM_Eager_Ptr_Deref && 
	       Eager_Ptr_Deref_Spec(cur_op, tgt_bb, TRUE)) {
	     safe_spec = TRUE;
	     Set_EAGER_PTR_SPEC(*spec_type);
	   }
#ifndef TARG_MIPS
	   else if (GCM_Speculative_Loads && Is_Target_Itanium() &&
		    OP_load(cur_op)) {
	     safe_spec = TRUE;
	     Set_CSAFE_PTR_SPEC(*spec_type);
	   }
#endif
	 }

	 // Check for any NULL ptr speculation cases (MIPS only).
	 if (!PROC_has_delayed_exception() && 
	     Null_Ptr_Deref_Spec(cur_op, src_bb, tgt_bb)) {
	   if (!(GCM_Pointer_Spec && GCM_Eager_Null_Ptr_Deref)) {
	     return FALSE;
	   } else {
	     Set_EAGER_NULL_PTR_SPEC(*spec_type);
	     safe_spec = TRUE;
	   }
	 }
       }
     }
   } // if (motion_type & (GCM_SPEC_ABOVE | GCM_SPEC_BELOW | GCM_CIRC_ABOVE))

   // If memory_op and either it's safe to speculate or has been proven to
   // be safe, by other safety tests, then proceed further.
#ifdef TARG_X8664
   BOOL op_access_mem = OP_memory(cur_op) || OP_load_exe(cur_op);

   /* bug#1470
      An asm instruction could access memory also.
    */
   if( !op_access_mem &&
       OP_code(cur_op) == TOP_asm ){
     ASM_OP_ANNOT* asm_info = (ASM_OP_ANNOT*)OP_MAP_Get(OP_Asm_Map, cur_op);
     for( int i = 0; i < OP_results(cur_op); i++ ){
       if( ASM_OP_result_memory(asm_info)[i] )
	 op_access_mem = TRUE;
     }

     for( int i = 0; i < OP_opnds(cur_op); i++ ){
       if( ASM_OP_opnd_memory(asm_info)[i] )
	 op_access_mem = TRUE;
     }     
   }

   if (op_access_mem && (can_spec || safe_spec)) {
#else
   if (OP_memory(cur_op) && (can_spec || safe_spec)) {
#endif // TARG_X8664
     
     FOR_ALL_BB_SET_members (*pred_bbs, cur_bb) {
     
       if (CG_Skip_GCM && BB_id(cur_bb) == GCM_To_BB)
	 return FALSE;
     
       // need to check for memory alias dependences here
       if (!Can_Mem_Op_Be_Moved(cur_op, cur_bb, src_bb, tgt_bb, motion_type)) 
	 return FALSE;
     
       // detect cases where movement of mem ops past the null ptr test
       // condition can be possible and allow them only when the
       // corresponding flags are true.
       BOOL equiv_fwd  = BS_MemberP (BB_pdom_set(tgt_bb), BB_id(cur_bb)) &&
	                 BS_MemberP (BB_dom_set(cur_bb), BB_id(tgt_bb));
       if (equiv_fwd && Null_Ptr_Deref_Spec(cur_op, src_bb, cur_bb)) {
	 if (!(GCM_Pointer_Spec && GCM_Eager_Null_Ptr_Deref)) {
	   return FALSE;
	 } else {
	   Set_EAGER_NULL_PTR_SPEC(*spec_type);
	   Use_Page_Zero = TRUE;
	 }
       }
     }
   }

   // if <cur_op> can't be speculated (as shown by <can_spec>) and 
   // that the eager_null_ptr test returned FALSE (as shown by <spec_type>)
   // then it's safe to conclude that <cur_op> can't be speculated.
   
   if (!can_spec && 
       !(*spec_type & SPEC_EAGER_NULL_PTR) &&
       !(*spec_type & SPEC_EAGER_PTR) && 
       !(*spec_type & SPEC_CIRC_PTR_ABOVE) &&
       !(*spec_type & SPEC_CSAFE_PTR) &&
       !(*spec_type & SPEC_DSAFE_PTR) &&
       !(*spec_type & SPEC_CDSAFE_PTR) &&
       !(*spec_type & SPEC_PSAFE_PTR))
     return FALSE;

   if (*spec_type & SPEC_EAGER_NULL_PTR) Use_Page_Zero = TRUE;

   if (Ignore_TN_Dep) {
     REGSET_CLEAR(mid_reg_defs);
     REGSET_CLEAR(mid_reg_uses);
   } else {
     // initialize mid_gtn_uses to be the live-in GTN set.
     mid_gtn_defs = GTN_SET_Create(GTN_UNIVERSE_size, &MEM_local_pool);
     mid_gtn_uses = GTN_SET_Create(GTN_UNIVERSE_size, &MEM_local_pool);

     mid_tn_defs = TN_SET_Create_Empty(Last_TN + 1, &MEM_local_pool);
     mid_tn_uses = TN_SET_Create_Empty(Last_TN + 1, &MEM_local_pool);

     GTN_SET_ClearD (mid_gtn_defs);
     GTN_SET_ClearD (mid_gtn_uses);
   }

   // Second, check for register dependences.
   FOR_ALL_BB_SET_members (*pred_bbs, cur_bb) {

     if (CG_Skip_GCM && BB_id(cur_bb) == GCM_To_BB)
       return FALSE;

     BBLIST *succ_list;
     // check to see if def of cur_op is live out on a successor that's not
     // on the pred_bb's set. don't need to check if <cur_bb> == <src_bb> 
     // since all the successors of <src_bb> are unnecessary.
     // TODO: need to maintain a similar list of <succ_bbs> for downward
     // code motion.
     if (cur_bb != src_bb) {
       for (INT i = 0; i < OP_results(cur_op); ++i) {
	 TN *result = OP_result(cur_op,i);
	 if (CG_Skip_GCM && TN_number(result) == GCM_Result_TN)
	   return FALSE;
	 FOR_ALL_BB_SUCCS(cur_bb, succ_list) {
	   BB *succ_bb = BBLIST_item(succ_list);
	   if (!BB_SET_MemberP(*pred_bbs, succ_bb)) {
	     if (Ignore_TN_Dep) {
	       ISA_REGISTER_CLASS result_cl = TN_register_class (result);
	       REGISTER result_reg = TN_register (result);
	       if (REG_LIVE_Into_BB (result_cl, result_reg, succ_bb) ||
		   
		   // #776729: Sometimes during circular-scheduling, we
		   // insert new blocks. These blocks don't have their
		   // REG_LIVE sets updated, because REG_LIVE can't cope
		   // with interactive updates (fixed-structures, uugghhh !!!)
		   // As a workaround, we also check for GRA_LIVE here.
		   // REAL FIX: To interactively update REG_LIVE sets.

		   (TN_is_global_reg(result) &&
		    GTN_SET_MemberP(BB_live_in(succ_bb), result))

#ifdef KEY	   // Continue the above workaround for #776729.  Check
		   // liveness for assigned registers since they are globals
		   // too.  Bug 8726.
		   || (result_reg != REGISTER_UNDEFINED &&
		       GTN_SET_MemberP(BB_live_in(succ_bb),
				       Build_Dedicated_TN(result_cl,
							  result_reg, 0)))
#endif
		   )
		 return FALSE;
	     } else {
	       if (TN_is_global_reg(result) &&
		   (GTN_SET_MemberP(BB_live_in(succ_bb), result) ||
		    GTN_SET_MemberP(BB_live_def(succ_bb), result)))
		 return FALSE;
	     }
	   }
	 }
       }
     }

     OP *limit_op;
     limit_op = Find_Limit_OP(cur_op, cur_bb, src_bb, tgt_bb);

     // accumulate all the mid_defs and mid_uses  (register/TN dependences)
     OP *op;
     BOOL forw = motion_type & (GCM_EQUIV_FWD | GCM_SPEC_ABOVE | 
				GCM_DUP_ABOVE | GCM_CIRC_ABOVE);
     for (op = ((forw && cur_bb != src_bb) || (!forw && cur_bb == src_bb)) ?
	    BB_last_op(cur_bb) : BB_first_op(cur_bb);
	  op && op != limit_op;
	  op = ((forw && cur_bb != src_bb) || (!forw && cur_bb == src_bb)) ?
	    OP_prev(op) : OP_next(op)) {

       if (OP_dummy(op)) {
	 if (!CGTARG_Is_OP_Barrier(op)) continue;
	 else if (OP_memory(cur_op)) return FALSE;
       }

       // making sure that we don't check the op with's itself
       if (op == cur_op) continue;

       // making sure that the <call_op> doesn't read/write <cur_op>
       if (OP_call(op) && 
	   !CG_DEP_Can_OP_Move_Across_Call(cur_op, op, forw, Ignore_TN_Dep))
	 return FALSE;

       // accumulate all the mid_defs
       INT i;
       for (i = 0; i < OP_results(op); ++i) {
	 TN *result_tn = OP_result(op,i);
	 if (Ignore_TN_Dep) {
	   REGISTER result_reg = TN_register (result_tn);
	   ISA_REGISTER_CLASS cl = TN_register_class (result_tn);
	   mid_reg_defs[cl] = REGISTER_SET_Union1(mid_reg_defs[cl],result_reg);
	 } else {
	   if (TN_is_global_reg(result_tn)) {
	     mid_gtn_defs = GTN_SET_Union1D(mid_gtn_defs, result_tn,
					    &MEM_local_pool);
	   } else if (cur_bb == src_bb) {
	     mid_tn_defs = TN_SET_Union1D(mid_tn_defs, result_tn, 
					  &MEM_local_pool);
	   }
	 }
       }

       // accumulate all the mid_uses
       for (i = 0; i < OP_opnds(op); ++i) {
	 TN *opnd_tn = OP_opnd(op,i);
	 if (TN_is_constant(opnd_tn)) continue;
	 if (Ignore_TN_Dep) {
	   REGISTER opnd_reg = TN_register (opnd_tn);
	   ISA_REGISTER_CLASS cl = TN_register_class (opnd_tn);
	   mid_reg_uses[cl] = REGISTER_SET_Union1(mid_reg_uses[cl], opnd_reg);
	   
	   // In case of circular scheduling, need to prevent speculated
	   // code from executing one extra time, than the condition 
	   // controlling them. This is prevented by converting branch 
	   // operands which control their accesses be converted to definitions
	   // to prevent their movement.
	   if ((motion_type & GCM_CIRC_ABOVE) && OP_br(op)) {
	     mid_reg_defs[cl] = REGISTER_SET_Union1(mid_reg_defs[cl], opnd_reg);
	   }
	 } else {
	   if (TN_is_global_reg(opnd_tn)) {
	     mid_gtn_uses = GTN_SET_Union1D(mid_gtn_uses, opnd_tn,
					    &MEM_local_pool);
	   } else if (cur_bb == src_bb) {
	     mid_tn_uses = TN_SET_Union1D(mid_tn_uses, opnd_tn, 
					  &MEM_local_pool);
	   }
	 }
       } // accumulate all the mid_uses
     }
   }

   // if the target of the instruction to be moved is either defined in the
   // middle blocks (output-dependence) OR there are prior uses of the target 
   // in the middle blocks (anti-dependence) OR there are flow- dependences.
   // In reality, these may not be this STRICT and can be relaxed either with
   // TN renaming or inserting predicated ops.

   BOOL can_move = 

     // check for register dependences for POST-GCM stage.
     ((Ignore_TN_Dep && !REGSET_INTERSECT((REGSET)defs[0], mid_reg_defs) &&
       !REGSET_INTERSECT((REGSET)defs[0], mid_reg_uses) &&
       !REGSET_INTERSECT((REGSET)uses[0], mid_reg_defs)) ||

      // check for global TN dependences for PRE-GCM stage.
      (!Ignore_TN_Dep && 
       (!GTN_SET_IntersectsP((GTN_SET *)defs[0], mid_gtn_defs) &&
	!GTN_SET_IntersectsP((GTN_SET *)defs[0], mid_gtn_uses) &&
	!GTN_SET_IntersectsP((GTN_SET *)uses[0], mid_gtn_defs)) &&

       (!TN_SET_IntersectsP((TN_SET *)defs[1], mid_tn_defs) &&
	!TN_SET_IntersectsP((TN_SET *)uses[1], mid_tn_uses) &&
	!TN_SET_IntersectsP((TN_SET *)uses[1], mid_tn_defs))));

   return can_move;
}

// ======================================================================
// GTN_Live_Out_From_BB
// Returns TRUE if <opnd_tn> is really live-out from <cand_bb>, i.e. 
// there exists a real use of <opnd_tn> in some successor block. Else,
// return FALSE.
// ======================================================================
static BOOL
GTN_Live_Out_From_BB(BB *cand_bb, TN *use_tn)
{
  BBLIST *succ_list;
  FOR_ALL_BB_SUCCS(cand_bb, succ_list) {
    BB *cur_bb = BBLIST_item(succ_list);
    if (TN_is_global_reg(use_tn) &&
	GTN_SET_MemberP(BB_live_in(cur_bb), use_tn))
      return TRUE;
  }
  
  return FALSE;
}

// ======================================================================
// Update_Live_In_Sets
// The assumption is that <use_tn> is now a no longer a global TN. 
// Therefore, need to update the live-in/live-out sets of all the
// intermediate blocks in <pred_bbs> set to reflect that. 
// ======================================================================
static void
Update_Live_In_Sets(TN *use_tn, BB *src_bb, BB *tgt_bb, BB_SET **pred_bbs)
{

  BB *cur_bb;

  // Check to see if <use_tn> is live-out of any exit from the intermediate
  // blocks in <pred_bbs>.
  
  BOOL tn_use = FALSE;
  FOR_ALL_BB_SET_members (*pred_bbs, cur_bb) {

    if (cur_bb == src_bb) continue;

    BBLIST *succ_list;
    FOR_ALL_BB_SUCCS(cur_bb, succ_list) {
      BB *succ_bb = BBLIST_item(succ_list);
      if ((/* cur_bb != tgt_bb &&  */
	   GTN_SET_MemberP(BB_live_use(cur_bb), use_tn)) ||
	  (!BB_SET_MemberP(*pred_bbs, succ_bb) &&
	  (GTN_SET_MemberP(BB_live_in(succ_bb), use_tn) ||
	   GTN_SET_MemberP(BB_live_def(succ_bb), use_tn)))) {
	tn_use = TRUE;
	break;
      }
    }
  }

  if (!tn_use) {
    FOR_ALL_BB_SET_members(*pred_bbs, cur_bb) {
      if (cur_bb == src_bb || cur_bb == tgt_bb) continue;
    
      // If there exists no use and that it's not live-use, update the
      // live-in/live-out sets of <cur_bb>.
	GRA_LIVE_Remove_Live_In_GTN(cur_bb, use_tn);
	GRA_LIVE_Remove_Live_Out_GTN(cur_bb, use_tn);
    }
  }
}

// ======================================================================
// Update_Live_Use_Counts
// This routine updates the <live_use> counts (if any) for <cur_op> in
// <cur_bb> and updates the <usage_map> structure.
// ======================================================================
static void
Update_Live_Use_Counts(BB *cur_bb, OP *cur_op, hTN_MAP *usage_map)
{
  UINT32 count, *counter, *tn_count;

  for (INT i = 0; i < OP_opnds(cur_op); ++i) {
    TN *opnd_tn = OP_opnd(cur_op, i);
    if (TN_is_constant(opnd_tn)) continue;

    BOOL use_opnd = FALSE;

    // if <opnd_tn> is used in <cur_bb>, or is redef'd (unlikely-case),
    // increment the usage counter.
    if (TN_is_global_reg(opnd_tn) &&
	(GTN_SET_MemberP(BB_live_use(cur_bb), opnd_tn) ||
	 GTN_SET_MemberP(BB_live_def(cur_bb), opnd_tn))) {
      use_opnd = TRUE;
    }

    BBLIST *succ_list;
    FOR_ALL_BB_SUCCS(cur_bb, succ_list) {
      BB *succ_bb = BBLIST_item(succ_list);
      if (TN_is_global_reg(opnd_tn) &&
	  GTN_SET_MemberP(BB_live_in(succ_bb), opnd_tn))
	use_opnd = TRUE;
    }

    // If there exists a use somewhere, increment the count. We are not
    // worried about exact counts, just something more than zero. 
    if (use_opnd) {
      tn_count = (UINT32 *) hTN_MAP_Get (*usage_map, opnd_tn);
      
      count = (tn_count) ? *tn_count : 0;
      counter = (tn_count) ? tn_count : (UINT32 *) malloc(sizeof(UINT32));
      *counter = ++count;
      
      hTN_MAP_Set (*usage_map, opnd_tn, counter);
    }
  }
}

// ======================================================================
// Update_GRA_Live_Sets
// This routine is the placeholder to update all the GRA_LIVE sets as 
// a result of moving <cand_op> from <bb> to <cand_bb>. This needs to be
// updated correctly so that the GRA phase will not be confused. The
// <pred_bbs> is the set containing all BBs between <bb> and <cand_bb>
// whose LIVE sets need to be updated as well.
// ======================================================================
static void
Update_GRA_Live_Sets(OP *cand_op, BB *bb, BB *cand_bb, BB_SET **pred_bbs)
{
  // Update the live info accordingly (PRE-GCM)

  INT i;
  BB *cur_bb;
  BOOL re_def = FALSE;
  hTN_MAP usage_map;
  UINT32 count, *counter, *tn_count;

  MEM_POOL_Push(&MEM_local_pool);
  usage_map = hTN_MAP_Create (&MEM_local_pool);


  // (1) First calculate OP counts for all TNs defined (and referenced) in 
  // <bb>.

  OP *succ_op;
  FOR_ALL_BB_OPs_FWD(bb, succ_op) {
    if (OP_dummy(succ_op)) continue;
    TN *result_tn = NULL;
    for (i = 0; i < OP_results(succ_op); ++i) {
      result_tn = OP_result(succ_op,i);
      tn_count = (UINT32 *) hTN_MAP_Get (usage_map, result_tn);

      count = (tn_count) ? *tn_count : 0;
      counter = (tn_count) ? tn_count : (UINT32 *) alloca(sizeof(UINT32));
      *counter = ++count;
      
      hTN_MAP_Set (usage_map, result_tn, counter);
    }

    for (i = 0; i < OP_opnds(succ_op); ++i) {
      TN *opnd_tn = OP_opnd(succ_op, i);
      if (TN_is_constant(opnd_tn)) continue;
      
      // if <opnd_tn> is a global reg, need to increment its usage 
      // accordingly.
      if (TN_is_global_reg(opnd_tn)) {
	tn_count = (UINT32 *) hTN_MAP_Get (usage_map, opnd_tn);
	
	count = (tn_count) ? *tn_count : 0;
	counter = (tn_count) ? tn_count : (UINT32 *) alloca(sizeof(UINT32));
	*counter = ++count;

	hTN_MAP_Set (usage_map, opnd_tn, counter);
      }
    }
  }

  // (2): Check to see if any of the result ans opnd TNs of <cand_op> are
  // now live-in or live-out because of the movement. Update the Live-Sets
  // accordingly.

  for (i = 0; i < OP_results(cand_op); ++i) {

    TN *result = OP_result(cand_op, i);
    FOR_ALL_BB_OPs_FWD(bb, succ_op) {
      if (OP_dummy(succ_op)) continue;
      TN *result_tn = NULL;
      INT j;

      for (j = 0; j < OP_opnds(succ_op); ++j) {
	TN *opnd_tn = OP_opnd(succ_op, j);
	if (TN_is_constant(opnd_tn)) continue;

	// Add it to the GRA Universe set to register it as a global TN.
	if (!re_def && result &&
	    (TN_number(result) == TN_number(opnd_tn))) {
	  GTN_UNIVERSE_Add_TN(opnd_tn);
	  // This should come only after <opnd_tn> has been added to 
	  // GTN_UNIVERSE
	  GRA_LIVE_Add_Live_Use_GTN(bb, opnd_tn);
	}
      }

      for (j = 0; j < OP_results(succ_op); ++j) {
	result_tn = OP_result(succ_op, j);

	// if there has been a redefinition of <result_tn>, no need to make
	// any further local use of the result TN's as a global TN.

	if (result_tn && result &&
	    TN_number(result) == TN_number(result_tn)) re_def = TRUE;
      }
    }

    // Remove the <result> TN from <bb>_live_def and add it to 
    // <cand_bb>_live_def set

    if (result) {
      if (TN_is_global_reg(result)) GRA_LIVE_Remove_Live_Def_GTN(bb, result);
      GTN_UNIVERSE_Add_TN(result);
      GRA_LIVE_Add_Live_Def_GTN(cand_bb, result);
    }

    // Now update the live-in/defreach-in/live-out/defreach-out sets of all
    // the intermediate BBs as well. There is phase-ordering issue here. This
    // should come only after <result> is made a GTN.

    FOR_ALL_BB_SET_members (*pred_bbs, cur_bb) {
      // No need to update if <cur_bb> == <cand_bb>
      if (cur_bb != cand_bb && result) {
	GRA_LIVE_Add_Live_In_GTN(cur_bb, result);
	GRA_LIVE_Add_Defreach_In_GTN(cur_bb, result);
      }

      // check for intermediate uses of any operands TNs of <cand_op> 
      // from the path between <cur_bb> and <cand_bb>.
      if (cur_bb != bb && cur_bb != cand_bb)
	Update_Live_Use_Counts(cur_bb, cand_op, &usage_map);

      tn_count = result ? (UINT32 *) hTN_MAP_Get (usage_map, result) : NULL;
      count = (tn_count) ? *tn_count : 0;

      // Need to update only if <cur_bb != bb> or that there are other defs
      // besides <cand_op> coming out of <bb>.
      if (result && (cur_bb != bb || count != 0)) {
	GRA_LIVE_Add_Live_Out_GTN(cur_bb, result);
	GRA_LIVE_Add_Defreach_Out_GTN(cur_bb, result);
      }
    }
  }

  // (3) Add the corresponding operand uses of <cand_op> to <cand_bb> live_use 
  // set and remove it from <bb> live-use accordingly (i.e. if there are
  // no other succeeding uses)

  for (i = 0; i < OP_opnds(cand_op); ++i) {
    TN *opnd_tn = OP_opnd(cand_op, i);
    if (TN_is_constant(opnd_tn)) continue;

    BOOL single_use = FALSE;

    if (TN_is_global_reg(opnd_tn)) {
      
      // There exists more than one use of <opnd_tn> in <cand_bb>
      if (!GTN_SET_MemberP(BB_live_in(cand_bb), opnd_tn)) {
	single_use = TRUE;
      }
      
      GRA_LIVE_Add_Live_Use_GTN(cand_bb, opnd_tn);

      if (!GTN_SET_MemberP(BB_live_def(cand_bb), opnd_tn))
	GRA_LIVE_Add_Defreach_In_GTN(cand_bb, opnd_tn);

      tn_count = (UINT32 *) hTN_MAP_Get (usage_map, opnd_tn);
      count = (tn_count) ? *tn_count : 0;

      if (count == 0) {
	// There are no more uses of <opnd_tn> in <bb>, so remove it
	// from the live_use set. If <opnd_tn> is not live-out (i.e. last
	// use), remove the live-in entry as well.
	GRA_LIVE_Remove_Live_Use_GTN(bb, opnd_tn);

	if (!GTN_SET_MemberP(BB_live_out(bb), opnd_tn))
	  GRA_LIVE_Remove_Live_In_GTN(bb, opnd_tn);
	Update_Live_In_Sets(opnd_tn, bb, cand_bb, pred_bbs);

	// Sometimes, a local TN becomes a GTN if it has a future
	// use. However, if the use is also moved, the GTN def can be
	// converted back to a local def. check for those cases.
	if (GTN_SET_MemberP(BB_live_def(cand_bb), opnd_tn) &&
	    !GTN_Live_Out_From_BB(cand_bb, opnd_tn)) {
	  UINT32 *opndtn_count = (UINT32 *) hTN_MAP_Get (usage_map, opnd_tn);
	  UINT32 opnd_count = (opndtn_count) ? *opndtn_count : 0;	  
	  if (opnd_count == 0 && single_use) {
	    GRA_LIVE_Remove_Live_Use_GTN(cand_bb, opnd_tn);
	    GRA_LIVE_Remove_Defreach_Out_GTN(cand_bb, opnd_tn);
	    GRA_LIVE_Remove_Live_Out_GTN(cand_bb, opnd_tn);
	  } 
	}
      }
    }
  }

  MEM_POOL_Pop(&MEM_local_pool);
}
		     
// ======================================================================
// Is_OP_Move_Better
// need to take into account all factors (here) in deciding whether 
// <cur_op> is better than <best_op>. The decision is made purely on
// source and target blocks properties. Op's properties are decided later
// once it's chosen that this op is really better
// ======================================================================
static BOOL
Is_OP_Move_Better(OP *cur_op, OP *best_op, mINT32 motion_type)
{

  // TODO: compute the less bottleneck resources in target block. this is 
  // necessary to decide profitability of picking an op

  switch (motion_type) {

  case GCM_EQUIV_FWD:
  case GCM_EQUIV:
  case GCM_SPEC_ABOVE:
  case GCM_CIRC_ABOVE:
	// arbitrary heuristic: used just to test the code
	if (OP_scycle(cur_op) < OP_scycle(best_op)) return TRUE;
	if (OP_scycle(cur_op) >= OP_scycle(best_op)) return FALSE;
	break;
  case GCM_EQUIV_BKWD:
  case GCM_SPEC_BELOW:
	// arbitrary heuristic: used just to test the code
	if (OP_scycle(cur_op) > OP_scycle(best_op)) return TRUE;
	if (OP_scycle(cur_op) <= OP_scycle(best_op)) return FALSE;
	break;
  case GCM_DUP_ABOVE:
  case GCM_DUP_BELOW:
	break;
  default:
	FmtAssert(FALSE, ("unexpected code motion type in GCM phase"));
  }
  return FALSE;
}

// ======================================================================
// Determine_Motion_Type
// need to take into account all factors (here) in deciding the 
// appropriate motion_type for this <bb> present in <loop>. <bbsch> is used 
// to derive the properties of this basic block
// ======================================================================
static mINT32
Determine_Motion_Type(LOOP_DESCR *loop, BB *bb, BBSCH *bbsch)
{
  mINT32 motion_type;

  // Do forward circular motion
  if (GCM_Forw_Circ_Motion && !BB_CIRC_ABOVE(bbsch) && 
      !Get_Trace(TP_GCM, 0x0080) &&
      
      // Currently enable circular scheduling during post-GCM stage.
      Ignore_TN_Dep &&

      // Circular scheduling for fully unrolled-loops doesn't make sense.
      (LOOP_DESCR_loophead(loop) == bb) && !BB_unrolled_fully(bb) &&

      // Circular scheduling requires a new prolog, make sure we can add one.
      LOOP_DESCR_Can_Retarget_Loop_Entrances(loop) &&

      // TODO: we don't yet know how to handle loophead blocks which
      // contain memory barrier insructions. Avoid processing region blocks.
      // Too complicated.
      !BB_MEM_BARRIER(bbsch) && (BB_rid(bb) == NULL)) {
    motion_type = GCM_CIRC_ABOVE;
    Set_BB_CIRC_ABOVE(bbsch);
  }
  // Do equivalent code motion first since it's always useful and will
  // never result in any degradation.
  else if (!BB_EQUIV_FWD(bbsch) && !Get_Trace(TP_GCM, 0x0020)) {
    motion_type = GCM_EQUIV_FWD;
    Set_BB_EQUIV_FWD(bbsch);
  } 
  // Do forward speculation
  else if (!BB_SPEC_ABOVE(bbsch) && !Get_Trace(TP_GCM, 0x0040)) {
    motion_type = GCM_SPEC_ABOVE;
    Set_BB_SPEC_ABOVE(bbsch);
  } 
  // Do nothing. 
  else 
    motion_type = GCM_NONE;

  return motion_type;
}

// ======================================================================
// Determine_Candidate_Blocks
// need to take into account all factors (here) in deciding the 
// appropriate set of candidate blocks <cand_bbvector> for this bb for the
// concerned <motion_type>
// ======================================================================
static void
Determine_Candidate_Blocks(BB *bb, LOOP_DESCR *loop, mINT32 motion_type,
			VECTOR *priority_vector, VECTOR *cand_bbvector)
{
  BB *cand_bb;
  for (INT i = 0; i < VECTOR_count(*priority_vector); i++) {
    cand_bb = (BB *)VECTOR_element(*priority_vector, i);

    if (cand_bb == bb) continue;

    if (BB_scheduled(cand_bb) && !BB_scheduled_hbs(cand_bb)) continue;

    // don't consider empty blocks as they along with branches around them
    // can possibly be eliminated (by cflow)
    if (Is_BB_Empty(cand_bb)) continue;

    // differentiate the cases between equiv_fwd and equiv_bkwd separately.
    BOOL equiv_bkwd =  	BS_MemberP (BB_pdom_set(bb), BB_id(cand_bb)) &&
		     	BS_MemberP (BB_dom_set(cand_bb), BB_id(bb));
    BOOL equiv_fwd  = 	BS_MemberP (BB_pdom_set(cand_bb), BB_id(bb)) &&
			BS_MemberP (BB_dom_set(bb), BB_id(cand_bb));

#ifdef KEY
    /* Fix for bug#1406
       Although <cand_bb> dominates <bb>, and <bb> post-dominates <cand_bb>,
       it does not mean they are really equivalent, if <cand_bb> has two branches,
       and one lead to another loop.
       Should we fix up <equiv_bkwd> here ???
     */
    equiv_fwd = equiv_fwd && BB_Has_One_Succ( cand_bb );
#endif

    BOOL equiv = equiv_fwd && equiv_bkwd;

    // don't try candidate blocks that are above/below a frequency threshold
    // make the knob higher if frequency is feedback- directed
    if ((BB_freq(cand_bb) * (CG_PU_Has_Feedback ?
                             speculation_ratio_fb : speculation_ratio_wfb))
                > BB_freq(bb)) {


        if (!CG_PU_Has_Feedback && !equiv) {

	   // (1) Special case for speculative execution:
	   // where <bb> has a very few instructions and allowing them to
	   // speculate in <cand_bb> (if profitable) will also result
	   // in eliminating the unconditional branch around it. 
	   // Don't want to do this if feedback info is provided.
	 
           if ((BB_branch_op(bb) != NULL) &&
                (BB_Unique_Predecessor(bb) == cand_bb) &&
                (BB_length(bb) < 4)) goto profitable_candidate;
	   else continue;
       } else if (!GCM_Test) continue;
    }

profitable_candidate:
  // if <cand_bb> is a loophead bb comprising of more than one loop, then 
  // it isn't safe to move ops there. pv704881. more pruning needs to be
  // done.
  BOOL multiple_loop = (BB_loop_head_bb(cand_bb) && 
			BB_in_succs(bb, cand_bb) && 
			(BB_preds_len(cand_bb) > 2));

    // other metrics like block properties, FU-usage counts etc. also need
    // to be considered here for candidate blocks selections.
    BOOL cond = FALSE;

    switch (motion_type) {
	
    case GCM_EQUIV_FWD:
      cond = equiv_fwd && !multiple_loop;
      break;	

    case GCM_EQUIV_BKWD:
      cond = equiv_bkwd;
      break;

    case GCM_EQUIV:
      cond = equiv;
      break;

      // need to fill the holes here as well
    case GCM_SPEC_ABOVE:
      if (!multiple_loop &&
	  BS_MemberP (BB_dom_set(bb), BB_id(cand_bb)) &&
	  !BS_MemberP (BB_pdom_set(cand_bb), BB_id(bb)))
	cond = TRUE;
      break;

    case GCM_CIRC_ABOVE:
      break;

    case GCM_SPEC_BELOW:
    case GCM_DUP_ABOVE:
    case GCM_DUP_BELOW:

    default:
      FmtAssert(FALSE, ("unexpected code motion type in GCM phase"));
    }
    // add the candidate block if the condition is true and proceed
    if (cond) {
     VECTOR_Add_Element(*cand_bbvector, (void *)cand_bb);
    }
  } 
  // if <motion_type> is forward circular motion (bottom-loading) and the
  // nesting level of the loop is greater than zero, add the unique tail
  // block to the candidate blocks list.
  if (motion_type & GCM_CIRC_ABOVE) {
    if ((LOOP_DESCR_nestlevel(loop) > 0) && 
	(cand_bb = LOOP_DESCR_Find_Unique_Tail(loop)))
      VECTOR_Add_Element(*cand_bbvector, (void *)cand_bb);
  }
}

// =======================================================================
// Perform_Post_GCM_Steps
// is responsible (if any) for cleanup tasks after performing
// the code motion type as described in <motion_type> on <cand_op>
// success when <TRUE> tells that the code motion has been successful
// otherwise revert some of the changes (eg. ldst/addiu fixups that need
// to be restored to their original form)
// =======================================================================
static void
Perform_Post_GCM_Steps(BB *bb, BB *cand_bb, OP *cand_op, mINT32 motion_type,
		       mUINT8 spec_type, BB_SET **pred_bbs, 
		       LOOP_DESCR *loop, BOOL success)
{
  BBSCH *bbsch = (BBSCH *)BB_MAP_Get (bbsch_map, bb);
  BBSCH *cand_bbsch = (BBSCH *)BB_MAP_Get (bbsch_map, cand_bb);

  // one post-GCM step is to avoid speculation of two output dependent
  // variables simultaneously.
  // eg. if (cond) x =5; else x=3;
  // to avoid speculation of write of "x" from both paths, need to update
  // the live-in sets after speculating the first one so that suceeding 
  // speculation which computes a new value that is live on exit from the 
  // original target block is avoided.

  if (success) {
    if (motion_type & (GCM_SPEC_ABOVE | GCM_EQUIV_FWD | GCM_CIRC_ABOVE)) {

      INT i;
      if (Ignore_TN_Dep) {
	for (i = 0; i < OP_results(cand_op); ++i) {
	  TN *result = OP_result(cand_op, i);
	  BB *cur_bb;

	  // Update the live info accordingly (POST-GCM)
	  REGISTER result_reg = TN_register(result);
	  ISA_REGISTER_CLASS result_cl = TN_register_class (result);
	  FOR_ALL_BB_SET_members (*pred_bbs, cur_bb) {
	    REG_LIVE_Update(result_cl, result_reg, cur_bb);
	  }
	  REG_LIVE_Update(result_cl, result_reg, bb);
	}
      }
	
      // Extra work needed for GCM_CIRC_ABOVE. 
      // (1) Generate loop prolog blocks (if not already present)
      // (2) Move <cand_op> to the prolog block and insert the predicate 
      //     (if reqd).
      if (motion_type & GCM_CIRC_ABOVE) {

	// If <GCM_Loop_Prolog> not already present, generate a new one and 
	// prepend it to the loop.
	if (GCM_Loop_Prolog == NULL) {
	  GCM_Loop_Prolog = CG_LOOP_Gen_And_Prepend_To_Prolog(bb, loop);
	  GRA_LIVE_Compute_Liveness_For_BB(GCM_Loop_Prolog);
#ifdef KEY
	  // Need to update register liveness info for newly generated blocks.
	  // There is no procedure to copy liveness info from one block to 
	  // another. Better run REG_LIVE_Analyze_Region once.
	  if (Ignore_TN_Dep) {
	    REG_LIVE_Finish();
	    REG_LIVE_Analyze_Region();
	  }
#endif	
	  if (Trace_GCM) {
#pragma mips_frequency_hint NEVER
	    fprintf (TFile, "GCM: Circular Motion:\n");
	    fprintf (TFile, "GCM: Add New BB:%d before loophead BB:%d\n", 
		     BB_id(GCM_Loop_Prolog), BB_id(bb));
	  }
	} else {
	  if (Trace_GCM) {
#pragma mips_frequency_hint NEVER
	    fprintf (TFile, "GCM: Circular Motion:\n");
	    fprintf (TFile, "GCM: using existing loop prolog BB:%d \n", 
		     BB_id(GCM_Loop_Prolog));
	  }
	}

	// Update the dominator sets appropriately.
	OP *br_op = BB_branch_op(GCM_Loop_Prolog);
	BB *new_bb;
	if (br_op) {
	  new_bb = CG_LOOP_Append_BB_To_Prolog(GCM_Loop_Prolog, bb);
	  GRA_LIVE_Compute_Liveness_For_BB(new_bb);
	  Set_BB_dom_set(new_bb, BS_Create_Empty(2+PU_BB_Count+1, 
						 &gcm_loop_pool));
	  BS_Union1D(BB_dom_set(new_bb), BB_id(new_bb), NULL);
	  BS_UnionD(BB_dom_set(new_bb), BB_dom_set(bb), &gcm_loop_pool);
	    
	  Set_BB_pdom_set(new_bb, BS_Create_Empty(2+PU_BB_Count+1, 
						  &gcm_loop_pool));
	  BS_Union1D(BB_pdom_set(new_bb), BB_id(new_bb), NULL);
	  BS_UnionD(BB_pdom_set(new_bb), BB_pdom_set(bb), &gcm_loop_pool);
	  Run_Cflow_GCM = TRUE;
	}

	OP *dup_op = Dup_OP(cand_op);
	BB_Append_Op(GCM_Loop_Prolog, dup_op);
	if (PSAFE_PTR_SPEC(spec_type)) 
	  Set_OP_opnd(dup_op, OP_PREDICATE_OPND, True_TN);
	//copy the dom/pdom sets to the new_bb
	Set_BB_dom_set(GCM_Loop_Prolog, BS_Create_Empty(2+PU_BB_Count+1, 
							&gcm_loop_pool));
	BS_Union1D(BB_dom_set(GCM_Loop_Prolog), BB_id(GCM_Loop_Prolog), 
		   NULL);
	BS_UnionD(BB_dom_set(GCM_Loop_Prolog), BB_dom_set(bb), 
		  &gcm_loop_pool);

	Set_BB_pdom_set(GCM_Loop_Prolog, BS_Create_Empty(2+PU_BB_Count+1, 
							 &gcm_loop_pool));
	BS_Union1D(BB_pdom_set(GCM_Loop_Prolog), BB_id(GCM_Loop_Prolog), 
		   NULL);
	BS_UnionD(BB_pdom_set(GCM_Loop_Prolog), BB_pdom_set(bb), 
		  &gcm_loop_pool);
	
	if (Trace_GCM) {
#pragma mips_frequency_hint NEVER
	  Print_Trace_File(cand_op, bb, GCM_Loop_Prolog, TRUE);
	}
      }

      // Update GRA_LIVE sets for pre-GCM phase.
      if (!Ignore_TN_Dep) Update_GRA_Live_Sets(cand_op, bb, cand_bb, pred_bbs);
      // since the motion was successful, need to update the info
      // dynamically.
      if (bbsch && cand_bbsch) {
	BBSCH_num_real_ops(bbsch)--;
	BBSCH_num_real_ops(cand_bbsch)++;
	BBSCH_bb_start_pc(bbsch)++;
	BB_MAP_Set (bbsch_map, bb, bbsch);
	BB_MAP_Set (bbsch_map, cand_bb, cand_bbsch);
      }
    }
  } else {
    // sometimes, we move the addiu past the load/store op making the
    // necessary adjustments in the offset and may have to backtrack since
    // it wasn't a profitable code movement. this phase is responsible for
    // adjusting the load/store offsets back to their original form. This
    // is faster than actually calling the dep_graph builder and walking thru
    // the succ arcs.
    if (CGTARG_Is_OP_Addr_Incr(cand_op) &&
	!TN_is_sp_reg(OP_result(cand_op,0 /*???*/))) {
	INT64 addiu_const = TN_value (OP_opnd(cand_op,1));
	OP *succ_op;
	for (succ_op = cand_op;
	     succ_op != NULL;
	     succ_op = OP_next(succ_op)) {
	  if (OP_memory(succ_op)) {
	    // check if the memory OP has an offset field (i.e. it is not
	    // indexed load/store prefx
	    INT offset_opndnum = Memory_OP_Offset_Opndnum (succ_op);
	    INT base_opndnum = Memory_OP_Base_Opndnum (succ_op);
#ifdef TARG_X8664
	    FmtAssert( base_opndnum >= 0, ("NYI") );
#endif
	    if (TN_has_value(OP_opnd(succ_op, offset_opndnum))) {
	      if ((Ignore_TN_Dep && 
		   (TN_register(OP_opnd(succ_op, base_opndnum)) == 
		    TN_register(OP_result(cand_op,0 /*???*/)))) ||
		  
		  (!Ignore_TN_Dep &&
		   (TN_number(OP_opnd(succ_op, base_opndnum)) ==
		    TN_number(OP_result(cand_op,0 /*???*/)))))
		
		Fixup_Ldst_Offset (succ_op, addiu_const, +1, HBS_FROM_GCM);
	    }
	  }
	}
    }
    if (OP_memory(cand_op)) {
      INT offset_opndnum = Memory_OP_Offset_Opndnum (cand_op);
      if (TN_has_value(OP_opnd(cand_op, offset_opndnum))) {
	INT base_opndnum = Memory_OP_Base_Opndnum (cand_op);
#ifdef TARG_X8664
	FmtAssert( base_opndnum >= 0, ("NYI") );
#endif
	OP *succ_op;
	for (succ_op= OP_next(cand_op); 
	     succ_op != NULL; 
	     succ_op = OP_next(succ_op))
	  {
	    if (CGTARG_Is_OP_Addr_Incr(succ_op)) {
	      if ((Ignore_TN_Dep && 
		   (TN_register(OP_opnd(cand_op, base_opndnum)) ==
		    TN_register(OP_result(succ_op,0 /*???*/)))) ||
		  
		  (!Ignore_TN_Dep &&
		   (TN_number(OP_opnd(cand_op, base_opndnum)) ==
		    TN_number(OP_result(succ_op,0 /*???*/)))))
		{
		  INT64 addiu_const = TN_value (OP_opnd(succ_op,1));
		  Fixup_Ldst_Offset (cand_op, addiu_const, -1, HBS_FROM_GCM);
		  DevWarn ("Memory OP offset adjusted in GCM");
		}
	    }
	  }
      }
    }
  }
}

// =======================================================================
// OP_To_Move
// is responsible for picking an <op> to move (dependent on <motion_type>)
// from <bb> to <tgt_bb>. The selection of <op> is dependent on various
// factors. <motion_type> controls the type of code transformations that
// can be performed. <spec_type> returns the type of speculative movement,
// if required.
// =======================================================================
static OP *
OP_To_Move (BB *bb, BB *tgt_bb, BB_SET **pred_bbs, mINT32 motion_type, mUINT8 *spec_type)
{
  OP *cur_op;
  OP *best_op = NULL;

  // select a candidate instruction */
  // TODO: use a better heuristic and it should be dependent on the 
  // op's properties, the requested <motion_type> and <bb>, <tgt_bb>
  // properties

  REGISTER_SET 	reg_defs[ISA_REGISTER_CLASS_MAX+1], 
		reg_uses[ISA_REGISTER_CLASS_MAX+1],
		failed_reg_defs[ISA_REGISTER_CLASS_MAX+1], 
		failed_reg_uses[ISA_REGISTER_CLASS_MAX+1];

  GTN_SET       *gtn_defs, *gtn_uses, *failed_gtn_defs, *failed_gtn_uses;
  TN_SET        *tn_defs, *tn_uses, *failed_tn_defs, *failed_tn_uses;

  if (Ignore_TN_Dep) {
    REGSET_CLEAR (failed_reg_defs);
    REGSET_CLEAR (failed_reg_uses);
  } else {

    failed_gtn_defs = GTN_SET_Create(GTN_UNIVERSE_size, &MEM_local_pool);
    failed_gtn_uses = GTN_SET_Create(GTN_UNIVERSE_size, &MEM_local_pool);
    failed_tn_defs = TN_SET_Create_Empty(Last_TN + 1, &MEM_local_pool);
    failed_tn_uses = TN_SET_Create_Empty(Last_TN + 1, &MEM_local_pool);

    gtn_defs = GTN_SET_Create(GTN_UNIVERSE_size, &MEM_local_pool);
    gtn_uses = GTN_SET_Create(GTN_UNIVERSE_size, &MEM_local_pool);
    tn_defs = TN_SET_Create_Empty(Last_TN + 1, &MEM_local_pool);
    tn_uses = TN_SET_Create_Empty(Last_TN + 1, &MEM_local_pool);

    GTN_SET_ClearD (failed_gtn_uses);
    GTN_SET_ClearD (failed_gtn_defs);
  }

  // need to check if the motion is forward or backward and that we are not
  // moving code across procedure calls (unless full IPA summary is 
  // available) in either direction
  BOOL forw = motion_type & (GCM_EQUIV_FWD | GCM_SPEC_ABOVE | 
			     GCM_DUP_ABOVE | GCM_CIRC_ABOVE);
  OP *call_op = BB_call(bb) ? BB_xfer_op(bb) : NULL;
  for (cur_op = (forw) ? BB_first_op(bb) : BB_last_op(bb); cur_op; 
		cur_op = (forw) ? OP_next(cur_op) : OP_prev(cur_op)) {

    // don't consider dummy or transfer ops
    if (OP_xfer(cur_op) || OP_noop(cur_op)) continue;

#ifdef KEY // bug 4850
    if (CGTARG_Is_OP_Barrier(cur_op)) continue;
#endif

    // All real OPs and some dummy OPs which have real operands/results.
    // typo?: if (OP_Real_Ops(cur_op) != 1 || OP_Real_Ops(cur_op) != 0) {
    if (OP_Real_Ops(cur_op) != 1 && OP_Real_Ops(cur_op) != 0) {
      continue;
    }

    // has already been picked before, ignore it now
    if (OP_visited(cur_op)) continue;

    // op has a special meaning w.r.t <bb> and therefor shouldn't be 
    // considered as candidate for code movement. these special case
    // applies to all stack adjustment ops, or OPs with glue_code
    // attribute.
    if (OP_Has_Restrictions(cur_op, bb, tgt_bb, motion_type)) continue;

    if (Ignore_TN_Dep) {
      REGSET_CLEAR(reg_defs);
      REGSET_CLEAR(reg_uses);
    } else {

      GTN_SET_ClearD (gtn_defs);
      GTN_SET_ClearD (gtn_uses);
    }
    
    BOOL move_better = TRUE;
    if (best_op == NULL || (move_better = 
			Is_OP_Move_Better(cur_op, best_op, motion_type)))
    {
      if (*pred_bbs == NULL) {
       *pred_bbs = BB_SET_Singleton (tgt_bb, &MEM_local_pool);

       /* If <bb> is visited while looking for its predecessors till <tgt_bb>,
        * this implies that <bb> is part of a loop that does not include
        * <tgt_bb>. For this case, don't move instructions. We don't want 
        * to be doing loop invariant code motion.
        */
       if (motion_type & GCM_CIRC_ABOVE) {
	 *pred_bbs = BB_SET_Union1 (*pred_bbs, bb, &MEM_local_pool);
       } else {
	 if (BB_Add_Ancestors (pred_bbs, bb, bb, &MEM_local_pool)) return NULL;
       }
      }

      INT i;
      for (i = 0; i < OP_results(cur_op); ++i) {
       TN *result_tn = OP_result(cur_op,i);
       if (Ignore_TN_Dep) {
         REGISTER result_reg = TN_register (result_tn);
         ISA_REGISTER_CLASS result_cl = TN_register_class (result_tn);
         reg_defs[result_cl] = REGISTER_SET_Union1(reg_defs[result_cl],
                                                   result_reg);
       } else {
	 if (TN_is_global_reg(result_tn)) 
	   GTN_SET_Union1D(gtn_defs, result_tn, &MEM_local_pool);
	 else
	   tn_defs = TN_SET_Union1D(tn_defs, result_tn, &MEM_local_pool);
       }
      } // take care of result defs

      for (i = 0; i < OP_opnds(cur_op); ++i) {
	 TN *opnd_tn = OP_opnd(cur_op,i);
	 if (TN_is_constant(opnd_tn)) continue;
         if (Ignore_TN_Dep) {
           REGISTER opnd_reg = TN_register (opnd_tn);
           ISA_REGISTER_CLASS opnd_cl = TN_register_class (opnd_tn);
           reg_uses[opnd_cl] = REGISTER_SET_Union1(reg_uses[opnd_cl],
                                                   opnd_reg);
         } else {
	   if (TN_is_global_reg(opnd_tn))
	     GTN_SET_Union1D(gtn_uses, opnd_tn, &MEM_local_pool);
	   else
	     tn_uses = TN_SET_Union1D(tn_uses, opnd_tn, &MEM_local_pool);
         }
      } // take care of opnd uses

      // need to check if <cur_op> has any relation with prior unsuccessful 
      // ops in <bb>.
      BOOL succ_intrsct =

        ( Ignore_TN_Dep && !REGSET_INTERSECT(reg_defs, failed_reg_defs) &&
          !REGSET_INTERSECT(reg_defs, failed_reg_uses) &&
          !REGSET_INTERSECT(reg_uses, failed_reg_defs) ||

         (!Ignore_TN_Dep &&
	  (!GTN_SET_IntersectsP(gtn_defs, failed_gtn_defs) &&
	   !GTN_SET_IntersectsP(gtn_defs, failed_gtn_uses) &&
	   !GTN_SET_IntersectsP(gtn_uses, failed_gtn_defs)) &&

	  (!TN_SET_IntersectsP(tn_defs, failed_tn_defs) &&
	   !TN_SET_IntersectsP(tn_defs, failed_tn_uses) &&
	   !TN_SET_IntersectsP(tn_uses, failed_tn_defs))));

      void *defs[2], *uses[2];
      defs[0] = Ignore_TN_Dep ? (void *)&reg_defs : (void *)gtn_defs;
      uses[0] = Ignore_TN_Dep ? (void *)&reg_uses : (void *)gtn_uses;
      defs[1] = Ignore_TN_Dep ? NULL : (void *)tn_defs;
      uses[1] = Ignore_TN_Dep ? NULL : (void *)tn_uses;
      
      if (!OP_unsafe(cur_op) &&
	  succ_intrsct &&

	  // need to check if <cur_op> can be moved past <call_op> 
	  // in the same bb.

	  CG_DEP_Can_OP_Move_Across_Call(cur_op, call_op, forw, Ignore_TN_Dep) &&

	  // need to see if <cur_op> has any dependence conflicts between 
	  // <bb> and <tgt_bb>

	  // if GCM_MINIMIZE_REGS enabled, it also checks for register usage
	  // requirements in addition to checking for legality. If the 
	  // register requirements of any <bb> exceed the limits as a result
	  // of performing the code movement, the operation is not performed.


	  Can_OP_Move(cur_op, bb, tgt_bb, pred_bbs, defs, uses, motion_type, spec_type))
	best_op = cur_op;
      else {
        if (Ignore_TN_Dep) {
          REGSET_OR(failed_reg_defs, reg_defs);
          REGSET_OR(failed_reg_uses, reg_uses);
        } else {
          failed_gtn_defs = GTN_SET_Union(failed_gtn_defs, gtn_defs,
                                          &MEM_local_pool);
          failed_gtn_uses = GTN_SET_Union(failed_gtn_uses, gtn_uses,
                                          &MEM_local_pool);
        }
      }
    }

    // compseed metric: now that the <best_op> has been picked among list
    // of possible choices and no further move is better, ideal spot to 
    // quit. Note, that the <bb> is already scheduled. so, the chances that
    // the most profitable candidates are caught early enough are very
    // high.
    else {
      if (best_op && !move_better) break;
    }
  }

  return best_op;
}

// =======================================================================
// Adjust_Qualifying_Predicate
// This routine adjust the qualifying predicate of <cand_op> (if required)
// as a result of movement of <cand_op> from <src_bb> to <tgt_bb>. 
// <motion_type> tells the type of transformation performed and <spec_type>
// determines the type of speculation (PSAFE, CSAFE,..).
// 
// =======================================================================
static void
Adjust_Qualifying_Predicate(OP *cand_op, BB *src_bb, BB *tgt_bb, 
			    mINT32 motion_type, mUINT8 spec_type)
{

  if (motion_type & (GCM_CIRC_ABOVE | GCM_SPEC_ABOVE)) {
    // do predicate promotion/assignment. 
    if (PSAFE_PTR_SPEC(spec_type)) {
      OP *tgt_br_op = BB_xfer_op(tgt_bb);
      if (tgt_br_op && OP_has_predicate(tgt_br_op)) {
	TN *tn1, *tn2;
	OP *cmp;
	CGTARG_Analyze_Compare(tgt_br_op, &tn1, &tn2, &cmp);
#ifdef TARG_IA64
        Remove_Explicit_Branch(tgt_bb);
#endif
	BOOL fall_thru = BB_Fall_Thru_Successor(tgt_bb) == src_bb;
	
	// If <!fall_through> set the predicate of <cand_op> to the
	// controlling predicate of <tgt_br_op>.
	if (!fall_thru) {
	  Set_OP_opnd(cand_op, OP_PREDICATE_OPND, 
		      OP_opnd(tgt_br_op, OP_PREDICATE_OPND));
	} else if (cmp && OP_results(cmp) == 2) {

	  // If <fall_through>, then need to determine the exact condition,
	  // depending on whether the branch is on the true or false
	  // predicate. Check the conditions here.

	  BOOL branch_on_true = CGTARG_Branches_On_True(tgt_br_op, cmp);
	  TN *pred_tn;
	  if (branch_on_true) {
	    pred_tn = OP_result(cmp, 1);
	    Set_OP_opnd(cand_op, OP_PREDICATE_OPND, pred_tn);

	    // Set the global reg bit accordingly.
	    if (OP_bb(cmp) != OP_bb(cand_op) && !TN_is_global_reg(pred_tn)) {
	      GTN_UNIVERSE_Add_TN(pred_tn);
	      // TODO: Need to update the live-sets accordingly.
	    }
	  } else {
	    pred_tn = OP_result(cmp, 0);
	    Set_OP_opnd(cand_op, OP_PREDICATE_OPND, pred_tn);

	    // Set the global reg bit accordingly.
	    if (OP_bb(cmp) != OP_bb(cand_op) && !TN_is_global_reg(pred_tn)) {
	      GTN_UNIVERSE_Add_TN(pred_tn);
	      // TODO: Need to update the live-sets accordingly.
	    }
	  }
	}
      }
    }  // PSAFE_PTR_SPEC
  }  // (.. (GCM_CIRC_ABOVE | GCM_SPEC_ABOVE))
}

// =======================================================================
// Append_Op_To_BB
// Appends <cand_op> to <bb>. need to consider where to append depending 
// on whether it's PRE/POST GCM phase.
// =======================================================================
static void
Append_Op_To_BB(OP *cand_op, BB *cand_bb, BB *src_bb,
		mINT32 motion_type, mUINT8 spec_type)
{
  OP *limit_op;

  // find the limit_op for the two phases.
  // limit_op is the <xfer_op> for the POST- GCM phase AND
  // can either be a <xfer_op> or <copy/glue_op> whichever comes last for
  // the PRE- GCM phase.

  if (Cur_Gcm_Type & GCM_BEFORE_GRA) 
    limit_op = BB_copy_xfer_op(cand_bb);
  else 
    limit_op = BB_xfer_op(cand_bb);

#ifdef TARG_X8664
  if( limit_op != NULL && OP_cond( limit_op ) ){
    FmtAssert( !TOP_is_change_rflags(OP_code(cand_op)), ("cand_op modifies rflags") );
  }
#endif

  // Insert before the <limit_op> or just append it.
  if (limit_op) 
    BB_Insert_Op_Before (cand_bb, limit_op, cand_op);
  else
    BB_Append_Op (cand_bb, cand_op);

  // Adjust the qualifying predicate if required.
  Adjust_Qualifying_Predicate(cand_op, src_bb, cand_bb, motion_type,spec_type);
  
}

// =======================================================================
// Adjust_BBSCH
// Accounts for final adjustments to local/global regcosts sets as a 
// result of moving <cand_op> fro <bb> to <cand_bb>. The corresponding
// BBSCH structures are updated as a result of it.
// =======================================================================
static void
Adjust_BBSCH (OP *cand_op, BB *cand_bb, BB *bb,
	      BBSCH *new_cand_bbsch, BBSCH *new_bbsch)
{
  // Account the cost of converting a TN to a GTN or vice-versa.
  if (OP_has_result(cand_op)) {
    TN *result_tn = OP_result(cand_op, 0);
    if (!TN_is_global_reg(result_tn)) {
      BBSCH_global_regcost(new_cand_bbsch)++;
      BBSCH_global_regcost(new_bbsch)++;
#ifndef TARG_MIPS
      BBSCH_local_regcost(new_bbsch)--; 
#endif
    } else {
      BBSCH_global_regcost(new_cand_bbsch)++;
      BBSCH_global_regcost(new_bbsch)--;
    }
  }

  // Now account for operands estimates as well.
  for (INT i = 0; i < OP_opnds(cand_op); ++i) {
    TN *opnd_tn = OP_opnd(cand_op, i);
    if (TN_is_constant(opnd_tn)) continue;
    
    if (TN_is_global_reg(opnd_tn)) {
      if (!GTN_SET_MemberP(BB_live_out(cand_bb), opnd_tn)) {
	BBSCH_global_regcost(new_cand_bbsch)--;
#ifndef TARG_MIPS
	BBSCH_local_regcost(new_cand_bbsch)++;
#endif
      }
      if (!GTN_SET_MemberP(BB_live_out(bb), opnd_tn))
	BBSCH_global_regcost(new_bbsch)--;
    }
  }
}

// =======================================================================
// Is_Schedule_Worse
// Determines if the weighted schedules as a result of code movement is
// worse or not. All the OOO adjustments should be made here.
// =======================================================================
static BOOL
Is_Schedule_Worse(BB *bb, BB *cand_bb, BBSCH *new_bbsch, 
		  BBSCH *new_cand_bbsch, BBSCH *old_bbsch,
		  BBSCH *old_cand_bbsch)
{

  UINT32 new_from_time, new_to_time, old_from_time, old_to_time;
  mINT8  old_from_regcost[ISA_REGISTER_CLASS_MAX+1], 
         old_to_regcost[ISA_REGISTER_CLASS_MAX+1],
         new_from_regcost[ISA_REGISTER_CLASS_MAX+1], 
         new_to_regcost[ISA_REGISTER_CLASS_MAX+1];

  old_from_time = BBSCH_schedule_length(old_bbsch);
  old_to_time = BBSCH_schedule_length(old_cand_bbsch);
  new_from_time = BBSCH_schedule_length(new_bbsch);
  new_to_time = BBSCH_schedule_length(new_cand_bbsch);

  if (Cur_Gcm_Type & GCM_MINIMIZE_REGS) {
    mINT8 *old_from_local_regcost, *old_to_local_regcost,
          *new_from_local_regcost, *new_to_local_regcost;

    old_from_local_regcost = BBSCH_local_regcost(old_bbsch);
    old_to_local_regcost = BBSCH_local_regcost(old_cand_bbsch);
    new_from_local_regcost = BBSCH_local_regcost(new_bbsch);
    new_to_local_regcost = BBSCH_local_regcost(new_cand_bbsch);


    if (Trace_GCM && Trace_GCM_Reg_Usage && GCM_Internal_Flag) {
      #pragma mips_frequency_hint NEVER
      fprintf (TFile, "\n FROM BB:%d => TO BB:%d\n", BB_id(bb), BB_id(cand_bb));
      fprintf (TFile, "\n LOCAL REGISTER COST");
    }

    // TODO: need to update similar ISA_REGISTER_CLASS values for global regs
    // as well.
    INT i;
    FOR_ALL_ISA_REGISTER_CLASS(i) {

      old_from_regcost[i] =  BBSCH_global_regcost(old_bbsch) + 
	                     ((old_from_local_regcost) ? old_from_local_regcost[i] : 0);

      old_to_regcost[i] =    BBSCH_global_regcost(old_cand_bbsch) + 
	                     ((old_to_local_regcost) ? old_to_local_regcost[i] : 0);

      new_from_regcost[i] =  BBSCH_global_regcost(new_bbsch) + 
	                     ((new_from_local_regcost) ? new_from_local_regcost[i] : 0);
      
      new_to_regcost[i] =    BBSCH_global_regcost(new_cand_bbsch) + 
	                     ((new_to_local_regcost) ? new_to_local_regcost[i] : 0);

      if (Trace_GCM && Trace_GCM_Reg_Usage && GCM_Internal_Flag) {
        #pragma mips_frequency_hint NEVER
        fprintf (TFile, "\nold_from_local_regcost[%d]=%d, old_to_local_regcost[%d]=%d\n",
		 i, (old_from_local_regcost) ? old_from_local_regcost[i] : 0, 
		 i, (old_to_local_regcost) ? old_to_local_regcost[i] : 0);   	
        

        fprintf (TFile, "new_from_local_regcost[%d]=%d, new_to_local_regcost[%d]=%d\n",
		 i, (new_from_local_regcost) ? new_from_local_regcost[i] : 0, 
		 i, (new_to_local_regcost) ? new_to_local_regcost[i] : 0);   	
       
      }
    }
    if (Trace_GCM && Trace_GCM_Reg_Usage && GCM_Internal_Flag) {
      #pragma mips_frequency_hint NEVER
      fprintf (TFile, "\n GLOBAL REGISTER COST");
      fprintf (TFile, "\nold_from_global_regcost=%d, old_to_global_regcost=%d\n",
	       BBSCH_global_regcost(old_bbsch), BBSCH_global_regcost(old_cand_bbsch)); 

      fprintf (TFile, "new_from_global_regcost=%d, new_to_global_regcost=%d\n",
	       BBSCH_global_regcost(new_bbsch), BBSCH_global_regcost(new_cand_bbsch)); 
     }
  }
  
  BOOL equiv_blocks = 	
    BS_MemberP (BB_pdom_set(bb), BB_id(cand_bb)) &&
    BS_MemberP (BB_dom_set(cand_bb), BB_id(bb)) ||
    BS_MemberP (BB_pdom_set(cand_bb), BB_id(bb)) &&
    BS_MemberP (BB_dom_set(bb), BB_id(cand_bb));

  UINT32 branch_penalty;
  float freq_ratio = BB_freq(cand_bb)/BB_freq(bb);

  // OOO adjustments (necessary?)
  if (CG_DEP_Adjust_OOO_Latency && !equiv_blocks && (freq_ratio < 0.4))
    branch_penalty = (UINT32) (freq_ratio * (1 - freq_ratio) * mispredict);
  else
    branch_penalty = 0;

  // make the schedule 10 times worse if GCM_Test is enabled just to
  // invoke more transformations (and testing coverage).
  // Include the branch misprediction penalty when considering OOO effects.
  // This will bias the fall-thru path slightly since the cost of misprediction
  // is high too.

  INT times = (GCM_Test) ? 10 : 1;
  BOOL worsen_schedule =
    (((BB_freq(bb) * new_from_time) + 
      (BB_freq(cand_bb) * new_to_time)) >
     (times * ((BB_freq(bb) * old_from_time) + 
	       (BB_freq(cand_bb) * old_to_time) + 
	       branch_penalty)) ||
     
     // to check that we only do speculation if it's free
     (!equiv_blocks && (new_to_time > (times * (old_to_time + 
						branch_penalty)))));

  // TODO: need to consider register class and consider costs separately
  // for each class.

  BOOL improve_reg_pressure = TRUE;
  if (Cur_Gcm_Type & GCM_MINIMIZE_REGS) {
    for (INT i = ISA_REGISTER_CLASS_MIN; i <= ISA_REGISTER_CLASS_MAX &&
				     improve_reg_pressure; i++) {
      UINT8 delta_from = new_from_regcost[i] - old_from_regcost[i];
      UINT8 delta_to = new_to_regcost[i] - old_to_regcost[i];

#ifdef KEY
      // Implementing the TODO: need to consider register class and 
      // consider costs separately for each class
      improve_reg_pressure =  improve_reg_pressure &&
	(old_from_regcost[i] <= REGISTER_CLASS_info[i].register_count &&
	 old_to_regcost[i] <= REGISTER_CLASS_info[i].register_count &&
	 ((old_from_regcost[i] + delta_from) <= REGISTER_CLASS_info[i].register_count) &&
	 ((old_to_regcost[i] + delta_to) <= REGISTER_CLASS_info[i].register_count));
#else
      improve_reg_pressure =  improve_reg_pressure &&
	(old_from_regcost[i] <= REGISTER_MAX &&
	 old_to_regcost[i] <= REGISTER_MAX &&
	 ((old_from_regcost[i] + delta_from) <= REGISTER_MAX) &&
	 ((old_to_regcost[i] + delta_to) <= REGISTER_MAX));
#endif
    }
  }

  return worsen_schedule || !improve_reg_pressure;
}
	    
// =======================================================================
// Schedule_BB_For_GCM
// calls the hyperblock scheduler (HBS) for single-BBs to derive some of 
// the blocks properties in <bbsch>.
// =======================================================================
static BBSCH *
Schedule_BB_For_GCM (BB *bb, HBS_TYPE hb_type, HB_Schedule **Sched)
{
  BBSCH *bbsch = (BBSCH *)BB_MAP_Get (bbsch_map, bb);

  if (bbsch == NULL) {
    bbsch = TYPE_MEM_POOL_ALLOC (BBSCH, &gcm_loop_pool);
    bzero (bbsch, sizeof (BBSCH));
    Set_BB_SCHEDULE(bbsch); // need to schedule this block
  }

  if (BB_SCHEDULE(bbsch) ) {
    if (!GCM_Use_Sched_Est) {
      if (! *Sched) {
	*Sched = CXX_NEW(HB_Schedule(), &MEM_local_pool);
      }

      (*Sched)->Init(bb, hb_type, INT32_MAX, bbsch, NULL);
      (*Sched)->Schedule_BB(bb, bbsch);
    } else {
      Cur_Gcm_Type |= GCM_USE_SCHED_EST;
      BBSCH_schedule_length(bbsch) = CG_SCHED_EST_BB_Cycles(bb,
							    SCHED_EST_FOR_GCM);
    }
    BB_MAP_Set (bbsch_map, bb, bbsch);
  }
  return bbsch;
}

// =======================================================================
// Visit_BB_Preds
//
// Recursively visit the prececessors of <bb> which are in the set
// <loop_bbs> and mark that they are included in cycle <icycle>. 
// Stop when we reach <head>.
// 
// =======================================================================
static void Visit_BB_Preds(
  BB *bb,
  BB *head,
  BB_SET *loop_bbs,
  INT icycle,
  INT ncycles)
{

  // Create a cycle set for this BB if we haven't already.
  BS *cycle_set = BB_cycle_set(bb);
  if (cycle_set == NULL) {
    cycle_set = BS_Create_Empty(ncycles, &MEM_local_pool);
    Set_BB_cycle_set(bb, cycle_set);
  }

  BS_Union1D(cycle_set, icycle, NULL);

  if (bb == head) return;

  BBLIST *edge;
  FOR_ALL_BB_PREDS(bb, edge) {
    BB *pred = BBLIST_item(edge);
    if (BB_SET_MemberP(loop_bbs, pred)) {
      cycle_set = BB_cycle_set(pred);
      if (!cycle_set || !BS_MemberP(cycle_set, icycle)) {
	Visit_BB_Preds(pred, head, loop_bbs, icycle, ncycles);
      }
    }
  }
}

// =======================================================================
// GCM_For_Loop
// performs code motion within the loop. all inner loops are considered as
// black boxes. The basic blocks within this <loop> are considered in a
// priority order and the code motion transformations done on them, as 
// required.
// =======================================================================
static INT
GCM_For_Loop (LOOP_DESCR *loop, BB_SET *processed_bbs, HBS_TYPE hb_type)
{
  BB *bb;
  RID *rid;
  VECTOR bbvector, cand_bbvector;
  INT num_moves = 0;
  PRIORITY_TYPE priority_type;

  L_Save();
 
  bb_cycle_set_map = BB_MAP_Create ();

  BB_SET *loop_bbs = LOOP_DESCR_bbset(loop);
  BB *loop_head = LOOP_DESCR_loophead(loop);

  // For real loops...
  if (loop_head) {

    // Find the number of cycles shared by this loop head.
    BBLIST *edge;
    INT ncycles = 0;
    FOR_ALL_BB_PREDS(loop_head, edge) {
      BB *pred = BBLIST_item(edge);
      if (BB_SET_MemberP(loop_bbs, pred)) ++ncycles;
    }

    // If the loop contains more than one cycle, then create a map
    // that identifies which cycles a BB is part of.
    if (ncycles > 1) {
      INT icycle = 0;
      FOR_ALL_BB_PREDS(loop_head, edge) {
	BB *pred = BBLIST_item(edge);
	if (BB_SET_MemberP(loop_bbs, pred)) {
	  Visit_BB_Preds(pred, loop_head, loop_bbs, icycle, ncycles);
	  ++icycle;
	}
      }
    }
  }

  HBS_TYPE from_hbs_type, to_hbs_type;

  from_hbs_type = to_hbs_type = hb_type;
  bbvector = VECTOR_Init (PU_BB_Count+2, &MEM_local_pool);

  priority_type = SORT_BY_BB_FREQ;

  if (hb_type & (HBS_BEFORE_GRA | HBS_BEFORE_LRA))
    priority_type |= SORT_BY_REG_USAGE;
  else
    priority_type |= SORT_BY_BB_PARALLELISM;

  /* order the bbs in the loop in decreasing order of frequencies */
  FOR_ALL_BB_SET_members (loop_bbs, bb) {
    /* if bb has already been processed, ignore it. */
    if (BB_SET_MemberP (processed_bbs, bb)) continue;

    /* don't process already scheduled (or SWP) blocks */
    if ( (rid = BB_rid(bb)) && (RID_level(rid) >= RL_CGSCHED)) continue;
    if (BB_scheduled(bb) && !BB_scheduled_hbs(bb)) continue;

    // TODO: need to change sort_by_bb_frequency to a more general purpose 
    // priority function which can fit itself depending on which phase is 
    // calling GCM

    VECTOR_Add_Element (bbvector, (void *)bb);
  }

  VECTOR_Sort (bbvector, sort_by_bb_frequency);

  INT count = VECTOR_count(bbvector);
  /* skip single basic block loops */
  if (count <= 1) return 0;

  if (Trace_GCM) {
    #pragma mips_frequency_hint NEVER
    fprintf (TFile, "GCM_For_Loop: Ordered list of bbs:\n");
    for (INT i = 0; i < count; i++) {
      BB *bb = (BB *)VECTOR_element(bbvector, i);
      fprintf (TFile, "\tBB:%d\tfreq:%g\n", BB_id(bb), BB_freq(bb));
    }
  }
       
  // need to normalize the threshold or lookahead window to modulate
  // compile-time factors while considering blocks for GCM 

  mINT32 motion_type;

  // compspeed metric: not worthwile looking at more than 300 profitable
  // blocks in any region and 25 blocks in any non-loop region
  // TODO: need to tune this further.
  INT bb_limit = (LOOP_DESCR_nestlevel(loop) == 0) ? 25 : 100;

  /* traverse all the bbs in bbvector */

  BBSCH *old_bbsch = NULL, *old_cand_bbsch = NULL;
  HB_Schedule *Sched = NULL;
  for (INT i = 0; i < count; i++) {

    if (bb_limit-- <= 0) break;

    BB *bb = (BB *)VECTOR_element(bbvector, i);

    if (Check_If_Ignore_BB(bb, loop)) continue;

    from_hbs_type |= (Ignore_TN_Dep) ? HBS_FROM_POST_GCM_SCHED :
                                        HBS_FROM_PRE_GCM_SCHED;
    to_hbs_type |= (Ignore_TN_Dep) ? HBS_FROM_POST_GCM_SCHED :
                                      HBS_FROM_PRE_GCM_SCHED;
    from_hbs_type |= HBS_FROM_GCM_FROM_BB;

    BBSCH *bbsch = Schedule_BB_For_GCM (bb, from_hbs_type, &Sched);
    if (old_bbsch == NULL) {
      old_bbsch = TYPE_MEM_POOL_ALLOC(BBSCH, &MEM_local_pool);
      bzero (old_bbsch, sizeof (BBSCH));
    }
    bcopy(bbsch, old_bbsch, sizeof (BBSCH));
    Reset_BB_SCHEDULE(bbsch);

    // Determine the <motion_type> for this <bb> (in decreasing order of
    // priority).
    while ((motion_type = Determine_Motion_Type(loop, bb, bbsch)) != GCM_NONE){

      cand_bbvector = VECTOR_Init (PU_BB_Count+2, &MEM_local_pool);

      // determine the priority list of candidate blocks (in decreasing 
      // order) which fits the motion type and the source blocks properties

      Determine_Candidate_Blocks(bb, loop, motion_type, &bbvector,
				 &cand_bbvector);

      // Try moving instructions from only a limited number of basic blocks.
      // TODO: Set this limit based on the nesting level and opt-level

      INT cand_bb_limit = GCM_Test ? 30 : 10;
      INT cand_bbcount = VECTOR_count(cand_bbvector);

      // walk thru the <cand_bbvector> list in descending priority order and
      // backwards since it increases the scope of doing code motion across
      // large distances.
      for (INT j = cand_bbcount - 1; j >= 0; j--) {
        OP *cand_op;
        BB_SET *pred_bbs = NULL;
	BB *cand_bb = (BB *)VECTOR_element(cand_bbvector, j);

        if (Large_BB(cand_bb, loop))
           continue;

	if (CG_Skip_GCM) {
	  if (BB_id(bb) == GCM_From_BB && (GCM_To_BB < 0))
	    continue;
	  if (BB_id(cand_bb) == GCM_To_BB && (GCM_From_BB < 0))
	    continue;
	  if (BB_id(bb) == GCM_From_BB && BB_id(cand_bb) == GCM_To_BB)
	    continue;
	}
	  
        if (cand_bb_limit-- <= 0) break;

#ifdef KEY
	// Consider at most GCM_BB_Limit number of candidate bb's.
	if (GCM_BB_Limit != -1 &&
	    cumulative_cand_bb++ >= GCM_BB_Limit)
	  break;
#endif

  	/* don't make the target basic block too large. */
  	if (BB_length(cand_bb) >= (Split_BB_Length - 50)) continue;

	// The target and candidate BBs must be in the same cycles.
	// Skip this candidate if that's not true.
	BS *bb_cycle_set = BB_cycle_set(bb);
	BS *cand_bb_cycle_set = BB_cycle_set(cand_bb);
	if (bb_cycle_set && cand_bb_cycle_set) {
	  if (!BS_EqualP(bb_cycle_set, cand_bb_cycle_set)) continue;
	} else if (bb_cycle_set || cand_bb_cycle_set) {
	    continue;
	}

    	// TODO: we right now consider the motion from <bb> to <cand_bb>
    	// it could be the other way around as well.

	to_hbs_type |= HBS_FROM_GCM_TO_BB;
        BBSCH *cand_bbsch = Schedule_BB_For_GCM (cand_bb, to_hbs_type, &Sched);
        if (old_cand_bbsch == NULL) {
	  old_cand_bbsch = TYPE_MEM_POOL_ALLOC(BBSCH, &MEM_local_pool);
	  bzero (old_cand_bbsch, sizeof (BBSCH));
	}
        bcopy(cand_bbsch, old_cand_bbsch, sizeof (BBSCH));
	Reset_BB_SCHEDULE(cand_bbsch);

	L_Save();
	mUINT8 spec_type;

        // TODO: need to make this a OP_LIST of cand_op's (i.e. aggregrate code
        // movement)
        while ((cand_op = 
		OP_To_Move (bb, cand_bb, &pred_bbs, motion_type, &spec_type)) != NULL) {

	  BB_Remove_Op (bb, cand_op);
	  Set_BB_SCHEDULE(bbsch);
	  from_hbs_type |= (Ignore_TN_Dep) ? HBS_FROM_POST_GCM_SCHED_AGAIN : 
	                                      HBS_FROM_PRE_GCM_SCHED_AGAIN;

	  to_hbs_type |= (Ignore_TN_Dep) ? HBS_FROM_POST_GCM_SCHED_AGAIN : 
	                                    HBS_FROM_PRE_GCM_SCHED_AGAIN;

	  // Append before any terminating branch (if any) or else just 
	  // insert <cand_op> at the end. Also, check if the <cand_op>
	  // itself needs to be adjusted.

	  TN *old_pred_tn = OP_opnd(cand_op, OP_PREDICATE_OPND);
	  Append_Op_To_BB(cand_op, cand_bb, bb, motion_type, spec_type);
	  Set_BB_SCHEDULE(cand_bbsch);

	  /*  TODO. right now, we reschedule both blocks. need to implement
	      a faster and accurate version of determining if the motion is 
	      profitable, without actually rescheduling both blocks. */
	
	  Set_OP_moved(cand_op);
	  BBSCH *new_bbsch = Schedule_BB_For_GCM (bb, from_hbs_type, &Sched);
	  Set_BB_flags(new_bbsch, BBSCH_flags(old_bbsch));
	  BBSCH *new_cand_bbsch = Schedule_BB_For_GCM(cand_bb, to_hbs_type, &Sched);

	  from_hbs_type &= (Ignore_TN_Dep) ? ~HBS_FROM_POST_GCM_SCHED_AGAIN: 
					      ~HBS_FROM_PRE_GCM_SCHED_AGAIN;

	  to_hbs_type &= (Ignore_TN_Dep) ? ~HBS_FROM_POST_GCM_SCHED_AGAIN: 
					    ~HBS_FROM_PRE_GCM_SCHED_AGAIN;

	  // Account for any fine-grain register estimate corrections 
	  if (Cur_Gcm_Type & GCM_MINIMIZE_REGS)
	    Adjust_BBSCH(cand_op, cand_bb, bb, new_cand_bbsch, new_bbsch);

	  Reset_OP_moved(cand_op);
	  Reset_BB_SCHEDULE(bbsch);
	  Reset_BB_SCHEDULE(cand_bbsch);

	  Set_OP_visited(cand_op);

	  INT targ_alignment = (Align_Instructions) ? Align_Instructions:
	                                              CGTARG_Text_Alignment();
	  targ_alignment /= INST_BYTES; // so word sized
	  INT16 cand_bb_vacant_slots = Find_Vacant_Slots_BB(cand_bb, 
							    targ_alignment);
	  INT16 bb_vacant_slots = Find_Vacant_Slots_BB(bb, targ_alignment);

	  /* If schedule is not better, undo the movement, don't worry 
	     about local minima at the moment. 
	     also, check to see if the <bb> which MUST be aligned is now
	     not as a result of code motion */
	  if ((Ignore_TN_Dep && BB_ALIGNED(new_cand_bbsch) && 
	       ((BB_freq(cand_bb)/BB_freq(bb)) < 1.5) && 
	       ((2 * cand_bb_vacant_slots > targ_alignment))) ||

	      Is_Schedule_Worse(bb, cand_bb, new_bbsch, new_cand_bbsch,
				old_bbsch, old_cand_bbsch)) {

	    // TODO: need to do similar thing for downward code motion
	    Perform_Post_GCM_Steps(bb, cand_bb, cand_op, motion_type, 
				   spec_type, &pred_bbs, loop, FALSE);
	    BB_Remove_Op (cand_bb, cand_op);
	    BB_Prepend_Op (bb, cand_op);
	    Set_OP_opnd(cand_op, OP_PREDICATE_OPND, old_pred_tn);

	    if (Trace_GCM && GCM_Internal_Flag) {
	      #pragma mips_frequency_hint NEVER
	      Print_Trace_File(cand_op, bb, cand_bb, FALSE);
	      fprintf (TFile, "GCM: OLD Schedule length: MOVEDFROM = %d MOVEDTO = %d\n",
		       BBSCH_schedule_length(old_bbsch), BBSCH_schedule_length(old_cand_bbsch));
	      fprintf (TFile, "GCM: NEW Schedule length: MOVEDFROM = %d MOVEDTO = %d\n",
		       BBSCH_schedule_length(new_bbsch), BBSCH_schedule_length(new_cand_bbsch));
	    }

	    Set_BB_SCHEDULE(bbsch);
	    Set_BB_SCHEDULE(cand_bbsch);
#ifdef KEY
	    // Due to the way the control is organized, it is possible that
	    // the bb, and cand_bb never get scheduled again.
	    // see compilation of gcc.c-torture/compile/950922-1.c
	    // There is no harm in re-scheduling because these are the schedule info that
	    // is latest and is going to be passed around to other modules.
	    bbsch = Schedule_BB_For_GCM (bb, from_hbs_type, &Sched);
	    cand_bbsch = Schedule_BB_For_GCM (cand_bb, to_hbs_type, &Sched);
#endif
	  }
	  else {
	    num_moves++;
	    Run_Cflow_GCM |= Is_BB_Empty(bb);
	    Reset_OP_visited(cand_op);
	    Reset_BB_scheduled(bb);
	    Reset_BB_scheduled(cand_bb);

	    Perform_Post_GCM_Steps(bb, cand_bb, cand_op, motion_type,
				   spec_type, &pred_bbs, loop, TRUE);
	    bcopy(new_bbsch, old_bbsch, sizeof (BBSCH));
	    bcopy(new_cand_bbsch, old_cand_bbsch, sizeof (BBSCH));
	    if (Trace_GCM) {
	      #pragma mips_frequency_hint NEVER
	      Print_Trace_File(cand_op, bb, cand_bb, TRUE);
	      fprintf (TFile, "GCM: OLD Schedule length: MOVEDFROM = %d MOVEDTO = %d\n",
		       BBSCH_schedule_length(old_bbsch), BBSCH_schedule_length(old_cand_bbsch));
	      fprintf (TFile, "GCM: NEW Schedule length: MOVEDFROM = %d MOVEDTO = %d\n",
		       BBSCH_schedule_length(new_bbsch), BBSCH_schedule_length(new_cand_bbsch));
	    }
          } // else block

	  // sometimes exit blocks remain with their delay slots unfilled
	  // check for those special cases here and reset the flags, if
	  // necessary.
	  if (BB_exit(bb) && BB_length(bb) >= 2) {
	    OP *delay_op = BB_last_op(bb);
	    // check if the last op in bb is a nop
	    if (delay_op == NULL || !OP_noop(delay_op)) Reset_BB_scheduled(bb);
	  }

	} // OP_To_Move...
	L_Free();
      } // while <motion_type> loop
    } // for <bb> loop
  }

  BB_MAP_Delete (bb_cycle_set_map);

  if (Sched) {
	CXX_DELETE(Sched, &MEM_local_pool);
  }
  L_Free();

  return num_moves;
}

// =======================================================================
// GCM_Schedule_Region
//
// The main driver for the global code motion (GCM) phase. The hbs_type will
// determine whether the GCM phase is invoked before or after register 
// allocation. The 
// -O2:  perform GCM phase after register allocation.
// -O3:  perform GCM phase both before/after register allocation
// 
// =======================================================================
void GCM_Schedule_Region (HBS_TYPE hbs_type)
{
  LOOP_DESCR *loop_list;
  LOOP_DESCR *outer_loop;
  BB_SET *all_bbs;
  INT max_nestlevel;	/* for use as circuit breaker */
  BB *bb;
  RID *rid;
  INT totalloops = 0;
  INT innerloops = 0;
  INT callloops = 0;
  INT multibbloops = 0;
  INT exitloops = 0;
  INT loopinfoloops = 0;

  hbs_type |= HBS_FROM_GCM;
  cur_hbs_type = hbs_type;
  Run_Cflow_GCM = FALSE;
  if (hbs_type & (HBS_BEFORE_LRA | HBS_BEFORE_GRA)) {
    /* TODO: need to implement pre- GCM phase */
    Ignore_TN_Dep = FALSE;
    Cur_Gcm_Type = GCM_BEFORE_GRA;
    // GCM_PRE_Pass_Enabled = TRUE;
    if (GCM_Min_Reg_Usage) Cur_Gcm_Type |= GCM_MINIMIZE_REGS;

    if (!GCM_PRE_Enable_Scheduling) return;
  } else {
    Ignore_TN_Dep = TRUE;
    Cur_Gcm_Type = GCM_AFTER_GRA;

    if (!GCM_POST_Enable_Scheduling) return;
  }

#ifdef KEY
  cumulative_cand_bb = 0;
#endif

  if (Trace_GCM) {
    #pragma mips_frequency_hint NEVER
    fprintf (TFile, "GCM_For_Region: PU %s\n", Cur_PU_Name);
  }

  Start_Timer (T_GCM_CU);
  Set_Error_Phase ("Global Code Motion");
  Trace_GCM = Get_Trace (TP_GCM, 0x01);

  MEM_POOL_Initialize (&loop_descr_pool, "LOOP_DESCR_pool", FALSE);
  MEM_POOL_Initialize (&gcm_loop_pool, "GCM loop pool", FALSE);
  MEM_POOL_Push(&loop_descr_pool);
  MEM_POOL_Push(&gcm_loop_pool);

  Calculate_Dominators ();
  if (Ignore_TN_Dep) REG_LIVE_Analyze_Region ();
  CGTARG_Compute_Branch_Parameters(&mispredict, &fixed, &taken, &times);

  L_Save();

  loop_list = LOOP_DESCR_Detect_Loops (&loop_descr_pool);

  /* Add the whole region as the outermost loop to make sure we do
   * GCM for the basic blocks outside loops. 
   */
  outer_loop = TYPE_L_ALLOC (LOOP_DESCR);
  BB_Mark_Unreachable_Blocks ();
  all_bbs = BB_SET_Create_Empty (PU_BB_Count+2, &loop_descr_pool);
  bbsch_map = BB_MAP_Create ();
  INT16 num_real_ops;
  INT num_of_nops; // no. of nops required if <bb> was aligned
  OP *op;

  // Initial Pass:
  // Determine if each <bb> will be aligned and store the state.
  // Also, restore other <bb> properties in <bbsch> structure as well
  // Ignore SWP blocks and <bbs> which are unreachable
  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
    if (BB_unreachable(bb)) continue;

    BBSCH *bbsch = (BBSCH *)BB_MAP_Get (bbsch_map, bb);
    if (bbsch == NULL) {
      bbsch = TYPE_MEM_POOL_ALLOC (BBSCH, &gcm_loop_pool);
      bzero (bbsch, sizeof (BBSCH));
      Set_BB_SCHEDULE(bbsch); // need to change this flag
    }

    // check to see if <bb> should be aligned. Entry <bb> of a procedure
    // are automatically aligned.
    num_of_nops = Check_If_Should_Align_BB(bb, INST_BYTES * cur_pc);
    if (num_of_nops || BB_entry(bb)) {
      Set_BB_num_align_nops(bbsch, num_of_nops);
      cur_pc = 0;
      Set_BB_ALIGNED(bbsch);
    }

    Set_BB_start_pc(bbsch, cur_pc);
    INT16 num_ops = 0;
    for (op = BB_first_op(bb); op; op = OP_next(op)) {
      num_real_ops = OP_Real_Ops (op);
      cur_pc += num_real_ops;
      num_ops += num_real_ops;
    }
    Set_BB_num_real_ops(bbsch, num_ops);
    BB_MAP_Set (bbsch_map, bb, bbsch);

    if ( (rid = BB_rid(bb)) && (RID_level(rid) >= RL_CGSCHED)) continue;
    if (BB_scheduled(bb) && !BB_scheduled_hbs(bb)) continue;

    all_bbs = BB_SET_Union1D (all_bbs, bb, &loop_descr_pool);
  }

  outer_loop->mem_pool = &loop_descr_pool;
  LOOP_DESCR_bbset(outer_loop) = all_bbs;
  LOOP_DESCR_loophead(outer_loop) = NULL;
  LOOP_DESCR_nestlevel(outer_loop) = 0;
  LOOP_DESCR_num_exits(outer_loop) = 0;
  LOOP_DESCR_next(outer_loop) = NULL;
  LOOP_DESCR_flags(outer_loop) = 0;

  /*  Add this "loop" to the end of our loop_list. Compute max_nestlevel */
  max_nestlevel = 0;
  if (loop_list == NULL) {
    loop_list = outer_loop;
  }
  else {
    LOOP_DESCR *lastloop = NULL;
    for (LOOP_DESCR *cloop = loop_list;
	 cloop != NULL;
         cloop = LOOP_DESCR_next(cloop))
    {
      lastloop = cloop;
      if (LOOP_DESCR_nestlevel(cloop) > max_nestlevel)
	max_nestlevel = LOOP_DESCR_nestlevel (cloop);
    }
    LOOP_DESCR_next(lastloop) = outer_loop;
  }

  BB_SET *processed_bbs = BB_SET_Create_Empty (PU_BB_Count+2, &loop_descr_pool);
  INT num_moves = 0;

  /* Determine the loop properties (eg. no. of innerloops, multibb-loops,
     call-loops, exit-loops. This info is used later in deciding the various
     code motion types */
  LOOP_DESCR *cloop;
  for (cloop = loop_list;
       cloop != NULL;
       cloop = LOOP_DESCR_next(cloop))
  {

    /* skip the last loop (it is the procedure) 
    if (LOOP_DESCR_next(cloop) == NULL) break; */
    totalloops++;
    BB_SET *loop_bbs = LOOP_DESCR_bbset(cloop);
    BB_SET *tmpset = BB_SET_Intersection (processed_bbs, 
					  loop_bbs,
					  &loop_descr_pool);
    if (BB_SET_EmptyP (tmpset)) {
      Set_Inner_Loop(cloop);
      innerloops++;
      if (LOOP_DESCR_loopinfo(cloop) != NULL) loopinfoloops++;
      INT bb_cnt = 0;
      BOOL has_call = FALSE;
      FOR_ALL_BB_SET_members (loop_bbs, bb) {
	bb_cnt++;
	if (BB_call (bb)) has_call = TRUE;
      }
      if (bb_cnt > 1) {
	multibbloops++;
	Set_Multibb_Loop(cloop);
	if (has_call) {
	  callloops++;
	  Set_Call_Loop(cloop);
	}
	if (LOOP_DESCR_num_exits(cloop) > 1) {
	  exitloops++;
	  Set_Exit_Loop(cloop);
	}
      }
    }
  }

  /* When processing outer loops, we ignore bbs that have already been 
   * processed. These correspond to inner loops.
   */
  BB_SET_ClearD(processed_bbs);
  for (cloop = loop_list; cloop != NULL; cloop = LOOP_DESCR_next(cloop))
  {
    /* Look only at 'CG_opt_level'  deepest levels of nesting. */
    /* TODO: add a knob to vary this */
    if (LOOP_DESCR_nestlevel(cloop) <= (max_nestlevel - CG_opt_level)) 
	continue;
    GCM_Loop_Prolog = NULL;

    num_moves += GCM_For_Loop (cloop, processed_bbs, hbs_type);

    processed_bbs = BB_SET_UnionD (processed_bbs, LOOP_DESCR_bbset(cloop), 
				   &loop_descr_pool); 
  }

  if (Trace_GCM) {
    #pragma mips_frequency_hint NEVER
    fprintf (TFile, "GCM_For_Loop: Loop characteristics \n");
    fprintf (TFile, "\ttotalloops %d\n", totalloops);
    fprintf (TFile, "\tinnerloops %d\n", innerloops);
    fprintf (TFile, "\tmultibbloops %d\n", multibbloops);
    fprintf (TFile, "\texitloops %d\n", exitloops);
    fprintf (TFile, "\tcallloops %d\n", callloops);
    fprintf (TFile, "\tloopinfoloops %d\n", loopinfoloops);
    fprintf (TFile, "\tNumber of moves: %d\n", num_moves);
  }

  if (Ignore_TN_Dep) REG_LIVE_Finish ();
  BB_MAP_Delete (bbsch_map);
  L_Free();
  Free_Dominators_Memory ();
  MEM_POOL_Pop (&loop_descr_pool);
  MEM_POOL_Pop (&gcm_loop_pool);
  MEM_POOL_Delete (&loop_descr_pool);
  MEM_POOL_Delete (&gcm_loop_pool);

  Stop_Timer (T_GCM_CU);
  Check_for_Dump (TP_GCM, NULL);
}
