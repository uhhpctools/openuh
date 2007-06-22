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


//                     Array Scalarization
//                     -------------------
//
// Description:
//
// 	In loops, convert things that look like
//	do i
//	  a[i] = ...
//	  ... = a[i]
//
//	into
//
//	do i
//	  t = ...
//	  a[i] = t
//	  ... = t
//
//	This is useful because 
//	  1) It gets rid of loads
//	  2) If it gets rid of all the loads to a local array then
//	     the array equivalencing algorithm will get rid of the array
//
//	Because SWP will do 1 as well as we do, we'll only apply this
//	algorithm to local arrays (Although it's trivial to change this).
//
/* ====================================================================
 * ====================================================================
 *
 * Module: sclrze.cxx
 * $Revision: 1.7 $
 * $Date: 05/04/07 19:50:39-07:00 $
 * $Author: kannann@iridot.keyresearch $
 * $Source: be/lno/SCCS/s.sclrze.cxx $
 *
 * Revision history:
 *  dd-mmm-94 - Original Version
 *
 * Description: Scalarize arrays 
 *
 * ====================================================================
 * ====================================================================
 */

#ifdef USE_PCH
#include "lno_pch.h"
#endif // USE_PCH
#pragma hdrstop

static char *source_file = __FILE__;
static char *rcs_id = "$Source: be/lno/SCCS/s.sclrze.cxx $ $Revision: 1.7 $";

#include <sys/types.h>
#include "lnopt_main.h"
#include "dep_graph.h"
#include "lwn_util.h"
#include "opt_du.h"
#include "reduc.h"
#include "sclrze.h"
#include "lnoutils.h"

static void Process_Store(WN *, VINDEX16 , ARRAY_DIRECTED_GRAPH16 *, BOOL,
			  BOOL, REDUCTION_MANAGER *red_manager);
static BOOL Dominates(WN *wn1, WN *wn2);
static BOOL Intervening_Write(INT,VINDEX16, 
			VINDEX16 ,ARRAY_DIRECTED_GRAPH16 *);
static BOOL Is_Invariant(ACCESS_ARRAY *store, WN *store_wn);
static BOOL MP_Problem(WN *wn1, WN *wn2);

void Scalarize_Arrays(ARRAY_DIRECTED_GRAPH16 *dep_graph,
	BOOL do_variants, BOOL do_invariants, REDUCTION_MANAGER *red_manager)
{
  if (Get_Trace(TP_LNOPT,TT_LNO_SCLRZE)) {
    fprintf(TFile,"Scalarizing arrays \n");
  }

  // search for a store
  VINDEX16 v;
  for (v = dep_graph->Get_Vertex(); v; v = dep_graph->Get_Next_Vertex(v)) {
    WN *wn = dep_graph->Get_Wn(v);
    OPCODE opcode = WN_opcode(wn);
    if (OPCODE_is_store(opcode) && (WN_kid_count(wn) == 2)) {
      WN *array = WN_kid1(wn);
      if (WN_operator(array) == OPR_ARRAY) {
        WN *base = WN_array_base(array);
        OPERATOR base_oper = WN_operator(base);
        if ((base_oper == OPR_LDID) || (base_oper == OPR_LDA)) {
          ST *st = WN_st(base);
          // is it local
#ifdef _NEW_SYMTAB
          if (ST_level(st) == CURRENT_SYMTAB) {
#else
          if (ST_symtab_id(st) == SYMTAB_id(Current_Symtab)) {
#endif
	    if (ST_sclass(st) == SCLASS_AUTO &&
		ST_base_idx(st) == ST_st_idx(st)) {
	      if (!ST_has_nested_ref(st)) {
                Process_Store(wn,v,dep_graph,do_variants,do_invariants,
					red_manager);
              }
            }
          }
        }
      }
    }
  }
}

// Process a store
static void Process_Store(WN *store_wn, VINDEX16 v, 
		ARRAY_DIRECTED_GRAPH16 *dep_graph, BOOL do_variants,
		BOOL do_invariants, REDUCTION_MANAGER *red_manager)
{
#ifdef TARG_X8664
  // Do not sclrze vector stores.
  if (MTYPE_is_vector(WN_desc(store_wn))) return;
#endif
#ifdef KEY // Bug 6162 - can not scalarize to MTYPE_M pregs.
  if (WN_desc(store_wn) == MTYPE_M) return;
#endif
  if (Inside_Loop_With_Goto(store_wn)) return;
  INT debug = Get_Trace(TP_LNOPT,TT_LNO_SCLRZE);

  BOOL scalarized_this_store = FALSE;
  WN_OFFSET preg_num=0;
  ST *preg_st=0;
  WN *preg_store = NULL;

  ACCESS_ARRAY *store = 
	(ACCESS_ARRAY *) WN_MAP_Get(LNO_Info_Map,WN_kid1(store_wn));

  if (debug) {
    fprintf(TFile,"Processing the store ");
    store->Print(TFile);
    fprintf(TFile,"\n");
  }

  BOOL is_invariant = Is_Invariant(store,store_wn);
  if (!do_variants && !is_invariant) {
    return;
  }
  if (!do_invariants && is_invariant) {
    return;
  }

  // Don't scalarize reductions as that will break the reduction
  if (red_manager && (red_manager->Which_Reduction(store_wn) != RED_NONE)) {
    return;
  }

  char preg_name[20];
  TYPE_ID store_type = WN_desc(store_wn);
  TYPE_ID type = Promote_Type(store_type);

  EINDEX16 e,next_e=0;
  for (e = dep_graph->Get_Out_Edge(v); e; e=next_e) {
    next_e = dep_graph->Get_Next_Out_Edge(e);
    VINDEX16 sink = dep_graph->Get_Sink(e);
    WN *load_wn = dep_graph->Get_Wn(sink);
    OPCODE opcode = WN_opcode(load_wn);
    if (OPCODE_is_load(opcode)) {
      if (OPCODE_operator(opcode) != OPR_LDID) {
        ACCESS_ARRAY *load = (ACCESS_ARRAY *) 
	  WN_MAP_Get(LNO_Info_Map,WN_kid0(load_wn));
        if (WN_operator(WN_kid0(load_wn)) == OPR_ARRAY && 
	    Equivalent_Access_Arrays(store,load,store_wn,load_wn) &&
            (DEPV_COMPUTE::Base_Test(store_wn,NULL,load_wn,NULL) ==
                       DEP_CONTINUE) 
#ifdef KEY
              &&
            //Bug 9134: scalarizing only if store to and load from the same field
             WN_field_id(store_wn)==WN_field_id(load_wn)
#endif
                      ) {
	  if (Dominates(store_wn,load_wn)) {
           if (!red_manager || 
	     (red_manager->Which_Reduction(store_wn) == RED_NONE)) {
	    if (!Intervening_Write(dep_graph->Depv_Array(e)->Max_Level(),
				v,sink,dep_graph)) {
             if (!MP_Problem(store_wn,load_wn)) {
	      if (!scalarized_this_store) { 
                if (debug) {
                  fprintf(TFile,"Scalarizing the load ");
                  load->Print(TFile);
                  fprintf(TFile,"\n");
                }
	        // Create a new preg
	        preg_st = MTYPE_To_PREG(type);
	        char *array_name =
			ST_name(WN_st(WN_array_base(WN_kid1(store_wn))));
	        INT length = strlen(array_name);
	        if (length < 18) {
		  strcpy(preg_name,array_name);
		  preg_name[length] = '_';
		  preg_name[length+1] = '1';
		  preg_name[length+2] = 0;
#ifdef _NEW_SYMTAB
                  preg_num = Create_Preg(type,preg_name); 
                } else {
                  preg_num = Create_Preg(type, NULL); 
                }
#else
                  preg_num = Create_Preg(type,preg_name, NULL); 
                } else {
                  preg_num = Create_Preg(type, NULL, NULL); 
                }
#endif
	        // replace A[i] = x with "preg = x; A[i] = preg"
	        OPCODE preg_s_opcode = OPCODE_make_op(OPR_STID,MTYPE_V,type);
		// Insert CVTL if necessary (854441)
		WN *wn_value = WN_kid0(store_wn);
		if (MTYPE_byte_size(store_type) < MTYPE_byte_size(type))
		  wn_value = LWN_Int_Type_Conversion(wn_value, store_type);
	        preg_store = LWN_CreateStid(preg_s_opcode,preg_num,
		     preg_st, Be_Type_Tbl(type),wn_value);
                WN_Set_Linenum(preg_store,WN_Get_Linenum(store_wn));
		LWN_Copy_Frequency_Tree(preg_store,store_wn);
                LWN_Insert_Block_Before(LWN_Get_Parent(store_wn),
						store_wn,preg_store);
	        OPCODE preg_l_opcode = OPCODE_make_op(OPR_LDID, type,type);
                WN *preg_load = WN_CreateLdid(preg_l_opcode,preg_num,
			preg_st, Be_Type_Tbl(type));
		LWN_Copy_Frequency(preg_load,store_wn);
	        WN_kid0(store_wn) = preg_load;
                LWN_Set_Parent(preg_load,store_wn);

	        Du_Mgr->Add_Def_Use(preg_store,preg_load);
	      }
	      scalarized_this_store = TRUE;

	      // replace the load with the use of the preg
	      WN *new_load = WN_CreateLdid(OPCODE_make_op(OPR_LDID,
		type,type),preg_num,preg_st,Be_Type_Tbl(type));
	      LWN_Copy_Frequency_Tree(new_load,load_wn);

              WN *parent = LWN_Get_Parent(load_wn);
	      for (INT i = 0; i < WN_kid_count(parent); i++) {
	        if (WN_kid(parent,i) == load_wn) {
	          WN_kid(parent,i) = new_load;
		  LWN_Set_Parent(new_load,parent);
	          LWN_Delete_Tree(load_wn);
		  break;
                }
              }

	      // update def-use for scalar
	      Du_Mgr->Add_Def_Use(preg_store,new_load);
	     }
	    }
	   }
	  }
        }
      }
    }
  }
}

// Does wn1 dominate wn2, be conservative in that you can always say FALSE
// Wn1 must be a statement 
static BOOL Dominates(WN *wn1, WN *wn2)
{
  Is_True(!OPCODE_is_expression(WN_opcode(wn1)),
    ("Non statement 1 in Dominates"));

  // wn1's parent has to be an ancestor of wn2
  WN *parent1 = LWN_Get_Parent(wn1);
  WN *ancestor2 = LWN_Get_Parent(wn2);
  WN *kid2 = wn2;
  while (ancestor2 && (ancestor2 != parent1)) {
   kid2 = ancestor2;
   ancestor2 = LWN_Get_Parent(ancestor2);
  }
  if (!ancestor2) return FALSE;


  // at this point, wn1 is a sibling of kid2
  // return TRUE if wn1 comes before kid2
  wn1 = WN_next(wn1);
  while (wn1) {
    if (wn1 == kid2) {
      return TRUE;
    }
    wn1 = WN_next(wn1);
  }
  return FALSE;
}


// Given that there is a must dependence between store and load,
// is there ary other store that might occur between the the store and the
// load
// If there exists a store2, whose maximum dependence level wrt the 
// load is greater than store's dependence level (INT level below), 
// then store2 is intervening.
//
// If there exists a store2 whose maximum dependence level is equal
// to store's, and there a dependence from store to store2 with dependence
// level >= the previous level, then store2 is intervening 
static BOOL Intervening_Write(INT level,VINDEX16 store_v, 
			VINDEX16 load_v,ARRAY_DIRECTED_GRAPH16 *dep_graph)
{
  EINDEX16 e;
  for (e=dep_graph->Get_In_Edge(load_v); e; e=dep_graph->Get_Next_In_Edge(e)) {
    INT level2 = dep_graph->Depv_Array(e)->Max_Level();
    if (level2 > level) { 
      return TRUE;
    } else if (level2 == level) {
      VINDEX16 store2_v = dep_graph->Get_Source(e);
      EINDEX16 store_store_edge = dep_graph->Get_Edge(store_v,store2_v);
      if (store_store_edge) {
	INT store_store_level = 
		dep_graph->Depv_Array(store_store_edge)->Max_Level();
        if (store_store_level >= level) {
	  return TRUE;
        }
      }
    }
  }
  return FALSE;
}

// Is this reference invariant in its inner loop
static BOOL Is_Invariant(ACCESS_ARRAY *store, WN *store_wn)
{
  // find the do loop info of the store
  WN *wn = LWN_Get_Parent(store_wn);
  while (WN_opcode(wn) != OPC_DO_LOOP) {
    wn = LWN_Get_Parent(wn);
  }
  DO_LOOP_INFO *dli = Get_Do_Loop_Info(wn);
  INT depth = dli->Depth;
  if (store->Too_Messy || (store->Non_Const_Loops() > depth)) {
    return FALSE;
  }

  for (INT i=0; i<store->Num_Vec(); i++) {
    ACCESS_VECTOR *av = store->Dim(i);
    if (av->Too_Messy || av->Loop_Coeff(depth)) {
      return FALSE;
    }
  }
  return TRUE;
}

// Don't scalarize across parallel boundaries
static BOOL MP_Problem(WN *wn1, WN *wn2) 
{
  if (Contains_MP) {
    WN *mp1 = LWN_Get_Parent(wn1);
    while (mp1 && (!Is_Mp_Region(mp1)) &&
	   ((WN_opcode(mp1) != OPC_DO_LOOP) || !Do_Loop_Is_Mp(mp1))) {
      mp1 = LWN_Get_Parent(mp1);
    }
    WN *mp2 = LWN_Get_Parent(wn2);
    while (mp2 && (!Is_Mp_Region(mp2)) &&
            ((WN_opcode(mp2) != OPC_DO_LOOP) || !Do_Loop_Is_Mp(mp2))) {
      mp2 = LWN_Get_Parent(mp2);
    }
    if ((mp1 || mp2) && (mp1 != mp2)) return TRUE;
  }
  return FALSE;
}
