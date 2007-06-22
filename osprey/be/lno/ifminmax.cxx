/*
 * Copyright 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
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


#ifdef USE_PCH
#include "lno_pch.h"
#endif // USE_PCH
#pragma hdrstop

#include <sys/types.h>

#include "lnopt_main.h"
#include "config.h"
#include "config_lno.h"
#include "strtab.h"
#include "stab.h"
#include "targ_const.h"

#include "lnoutils.h"
#include "wn_simp.h"
#include "stdlib.h"
#include "lwn_util.h"
#include "optimizer.h"
#include "opt_du.h"
#include "name.h"
#include "forward.h"

static DU_MANAGER* du = NULL; 

//-----------------------------------------------------------------------
// NAME: Matching_Stores
// FUNCTION: Returns TRUE if 'wn_store_one' and 'wn_store_two' are stores
//   to the same location. 
//-----------------------------------------------------------------------

static BOOL Matching_Stores(WN* wn_store_one, 
			    WN* wn_store_two)
{
  OPCODE op_one = WN_opcode(wn_store_one); 
  OPCODE op_two = WN_opcode(wn_store_two); 
  if (op_one != op_two)
    return FALSE; 
  OPERATOR opr = WN_operator(wn_store_one);
  if (opr == OPR_STID)    
    return SYMBOL(wn_store_one) == SYMBOL(wn_store_two); 
  if (opr == OPR_ISTORE) {
    WN* wn_array_one = WN_kid1(wn_store_one); 
    if (WN_operator(wn_array_one) != OPR_ARRAY)
      return FALSE; 
    WN* wn_array_two = WN_kid1(wn_store_two); 
    if (WN_operator(wn_array_two) != OPR_ARRAY)
      return FALSE; 
    WN* wn_base_one = WN_array_base(wn_array_one); 
    WN* wn_base_two = WN_array_base(wn_array_two); 
    ST* st_base_one(Get_ST_Base(wn_base_one)); 
    ST* st_base_two(Get_ST_Base(wn_base_two)); 
    BOOL same_base = (st_base_one == NULL || st_base_two == NULL)
      ? st_base_one == st_base_two 
      : ST_base(st_base_one) == ST_base(st_base_two)
      && ST_ofst(st_base_one) == ST_ofst(st_base_two);
    if (!same_base) 
      return FALSE; 
    ACCESS_ARRAY* aa_one = (ACCESS_ARRAY*) WN_MAP_Get(LNO_Info_Map, 
      wn_array_one);
    ACCESS_ARRAY* aa_two = (ACCESS_ARRAY*) WN_MAP_Get(LNO_Info_Map, 
      wn_array_two);
    if (!(*aa_one == *aa_two))
      return FALSE; 
    return TRUE; 
  } 
  return FALSE; 
} 

//-----------------------------------------------------------------------
// NAME: Store_Expr
// FUNCTION: Returns the expression being stored into 'wn_store'. 
//-----------------------------------------------------------------------

static WN* Store_Expr(WN* wn_store)
{
  switch (WN_operator(wn_store)) {
  case OPR_STID: 
    return WN_kid0(wn_store); 
  case OPR_ISTORE:
#ifdef KEY // Bug 2096
    return WN_kid0(wn_store);
#else
    return WN_kid1(wn_store);
#endif
  default: 
    FmtAssert(TRUE, ("Store_Expr(): Don't understand this store type")); 
    return NULL; 
  } 
} 

//-----------------------------------------------------------------------
// NAME: Matching_Exprs
// FUNCTION: Returns TRUE if 'wn_one' and 'wn_two' are equivalent ex- 
//   pressions. 
//-----------------------------------------------------------------------

static BOOL Matching_Exprs(WN* wn_one, 
			   WN* wn_two)
{
  return WN_Simp_Compare_Trees(wn_one, wn_two) == 0; 
}

//-----------------------------------------------------------------------
// NAME: IFMM_Convertible
// FUNCTION: Returns TRUE if 'wn_tree' is an OPR_IF which can be converted
//   into an OPR_MIN or OPR_MAX.  If so, '*if_mm_max' is set to TRUE if it
//   can be converted to an OPR_MAX , to FALSE if it can be converted
//   to an OPR_MIN.  If no conversion is possible, returns FALSE.  
//-----------------------------------------------------------------------

static BOOL IFMM_Convertible(WN* wn_tree, 
			     BOOL* if_mm_max) 
{
  if (WN_opcode(wn_tree) != OPC_IF)
    return FALSE; 
  LWN_Simplify_Tree(WN_if_test(wn_tree));
  WN* wn_test = WN_if_test(wn_tree);
  OPERATOR opr = WN_operator(wn_test);
  if (!(opr == OPR_LT || opr == OPR_LE || opr == OPR_GT || opr == OPR_GE))
    return FALSE;
  if (WN_first(WN_then(wn_tree)) == NULL)
    return FALSE;  
  if (WN_next(WN_first(WN_then(wn_tree))) != NULL)
    return FALSE;  
  if (WN_first(WN_else(wn_tree)) == NULL)
    return FALSE; 
  if (WN_next(WN_first(WN_else(wn_tree))) != NULL)
    return FALSE; 
  LWN_Simplify_Tree(WN_first(WN_then(wn_tree)));
  LWN_Simplify_Tree(WN_first(WN_else(wn_tree)));
  WN* wn_st_then = WN_first(WN_then(wn_tree));  
  WN* wn_st_else = WN_first(WN_else(wn_tree));  
  if (!Matching_Stores(wn_st_then, wn_st_else))
    return FALSE; 
  WN* wn_expr_then = Store_Expr(wn_st_then);
  WN* wn_expr_else = Store_Expr(wn_st_else);
  WN* wn_expr_left = WN_kid0(wn_test);
  WN* wn_expr_right = WN_kid1(wn_test);
  BOOL mm_max = FALSE; 
  BOOL return_value = FALSE; 
  if (Matching_Exprs(wn_expr_left, wn_expr_else)) { 
    if (Matching_Exprs(wn_expr_right, wn_expr_then)) {
      mm_max = opr == OPR_LT || opr == OPR_LE; 
      return_value = TRUE;
    } 
  } else if (Matching_Exprs(wn_expr_right, wn_expr_else)) { 
    if (Matching_Exprs(wn_expr_left, wn_expr_then)) {
      mm_max = opr == OPR_GT || opr == OPR_GE; 
      return_value = TRUE;
    } 
  } 
  *if_mm_max = mm_max;
  return return_value;
}  

//-----------------------------------------------------------------------
// NAME: IFMM_Convert
// FUNCTION: Converts the tree rooted at 'wn_if' to expression beginning 
//   with OPR_MIN if 'ifmm_max' is FALSE or an expression beginning with
//   with OPR_MAX if 'ifmm_max' is TRUE. 
//-----------------------------------------------------------------------

static WN* IFMM_Convert(WN* wn_if, 
			BOOL ifmm_max)
{
  WN* wn_left = WN_kid0(WN_if_test(wn_if));
  WN* wn_right = WN_kid1(WN_if_test(wn_if));
  WN* wn_result = WN_first(WN_then(wn_if));
  WN* wn_result_expr = Store_Expr(wn_result); 
  INT i;
  for (i = 0; i < WN_kid_count(wn_result_expr); i++) 
    if (WN_kid(LWN_Get_Parent(wn_result_expr), i) == wn_result_expr)
      break;
  INT kid_count = i; 
  TYPE_ID type_cmp = Max_Wtype(WN_rtype(wn_left), WN_rtype(wn_right));
  OPCODE op = OPCODE_make_op(ifmm_max ? OPR_MAX : OPR_MIN, type_cmp, MTYPE_V); 
  WN* wn_cmp = LWN_CreateExp2(op, wn_left, wn_right); 
  WN_kid0(WN_if_test(wn_if)) = NULL; 
  WN_kid1(WN_if_test(wn_if)) = NULL; 
  LWN_Set_Parent(wn_cmp, wn_result);
  WN_kid(wn_result, kid_count) = wn_cmp; 
  LWN_Extract_From_Block(wn_result); 
  LWN_Insert_Block_Before(LWN_Get_Parent(wn_if), wn_if, wn_result); 
  LWN_Extract_From_Block(wn_if);
  LWN_Delete_Tree(wn_if); 
  LWN_Delete_Tree(wn_result_expr);
  return wn_result; 
} 

//-----------------------------------------------------------------------
// NAME: Is_Loop_Lower_Bound
// FUNCTION: Returns TRUE if 'wn_use' is a lower bound of a OPC_DO_LOOP, 
//   (i.e. the WN_kid0(WN_start(wn_loop)) of some loop wn_loop).  Returns
//   FALSE otherwise. 
//-----------------------------------------------------------------------

static BOOL Is_Loop_Lower_Bound(WN* wn_use)
{ 
  if (WN_operator(wn_use) != OPR_LDID)
    return FALSE; 
  WN* wn_start = LWN_Get_Parent(wn_use); 
  if (wn_start == NULL) 
    return FALSE; 
  WN* wn_loop = LWN_Get_Parent(wn_start);
  if (wn_loop == NULL) 
    return FALSE; 
  if (WN_opcode(wn_loop) != OPC_DO_LOOP)
    return FALSE; 
  if (wn_start != WN_start(wn_loop))
    return FALSE; 
  if (wn_use != WN_kid0(wn_start))
    return FALSE; 
  return TRUE;
} 

//-----------------------------------------------------------------------
// NAME: Is_Loop_Upper_Bound
// FUNCTION: Returns TRUE if 'wn_use' is the upper bound of an OPC_DO_LOOP,
//   (i.e. the UBexp(WN_end(wn_loop)) of some loop wn_loop).  Returns 
//   FALSE otherwise. 
//-----------------------------------------------------------------------

static BOOL Is_Loop_Upper_Bound(WN* wn_use)
{
  if (WN_operator(wn_use) != OPR_LDID)
    return FALSE;
  WN* wn_end = LWN_Get_Parent(wn_use);
  if (wn_end == NULL) 
    return FALSE; 
  WN* wn_loop = LWN_Get_Parent(wn_end); 
  if (wn_loop == NULL) 
    return FALSE; 
  if (WN_opcode(wn_loop) != OPC_DO_LOOP)
    return FALSE; 
  if (wn_end != WN_end(wn_loop))
    return FALSE; 
  if (wn_use != UBexp(WN_end(wn_loop)))
    return FALSE; 
  return TRUE; 
} 

//-----------------------------------------------------------------------
// NAME: IFMM_Sink
// FUNCTION: Attempt to sink the expression rooted at 'wn_max_store' 
//   into any loop bounds for which it is a defintion. 
//-----------------------------------------------------------------------

static void IFMM_Sink(WN* wn_max_store)
{ 
  if (WN_operator(wn_max_store) != OPR_STID)
    return; 
  USE_LIST *use_list = du->Du_Get_Use(wn_max_store);
  if (use_list == NULL) 
    return;
  USE_LIST_ITER iter(use_list);
  const DU_NODE* node = NULL;
  const DU_NODE* nnode = NULL;
  for (node = iter.First(); !iter.Is_Empty(); node = nnode) {
    WN* wn_use = node->Wn();
    nnode = iter.Next();
    if (Is_Loop_Lower_Bound(wn_use)) {
      WN* wn_loop = LWN_Get_Parent(LWN_Get_Parent(wn_use)); 
      Forward_Substitute_Ldids(wn_use, du);
      DO_LOOP_INFO* dli = Get_Do_Loop_Info(wn_loop); 
      if (Bound_Is_Too_Messy(dli->LB))
	Hoist_Bounds_One_Level(wn_loop); 
    } else if (Is_Loop_Upper_Bound(wn_use)) { 
      WN* wn_loop = LWN_Get_Parent(LWN_Get_Parent(wn_use)); 
      Forward_Substitute_Ldids(wn_use, du);
      DO_LOOP_INFO* dli = Get_Do_Loop_Info(wn_loop); 
      if (Bound_Is_Too_Messy(dli->UB))
	Hoist_Bounds_One_Level(wn_loop); 
    } 
  }
} 

//-----------------------------------------------------------------------
// NAME: If_MinMax_Traverse
// FUNCTION: Traverse the 'wn_tree', attempting to convert IF statements
//  into MIN and MAX statements and sinking them into the loop bounds. 
//-----------------------------------------------------------------------

static void If_MinMax_Traverse(WN* wn_tree)
{
  BOOL ifmm_max = FALSE; 
  if (IFMM_Convertible(wn_tree, &ifmm_max)) {
    WN* wn_max_store = IFMM_Convert(wn_tree, ifmm_max);
    IFMM_Sink(wn_max_store);
    return; 
  } 

  if (WN_opcode(wn_tree) == OPC_BLOCK) { 
    WN* wnn = NULL; 
    for (WN* wn = WN_first(wn_tree); wn != NULL; wn = wnn) {
      wnn = WN_next(wn); 
      If_MinMax_Traverse(wn); 
    } 
  } else { 
    for (INT i = 0; i < WN_kid_count(wn_tree); i++) 
      If_MinMax_Traverse(WN_kid(wn_tree, i)); 
  } 
} 

//-----------------------------------------------------------------------
// NAME: If_MinMax
// FUNCTION: Convert expressions of the form: 
//   if (expr1 .relop. expr2) then 
//     result = expr1 
//   else 
//     result = expr2 
//   end if  
// to expressions of the form: 
//   result = min(expr1, expr2) 
// or
//   result = max(expr1, expr2) 
// where .relop. is one of .LT., .GT., .LE., and .GE.  
// Attempt sinking converted expressions into loop bounds where 
//   appropriate. 
//-----------------------------------------------------------------------

extern void If_MinMax(WN* func_nd)
{
  if (!LNO_IfMinMax)
    return; 
  if (LNO_Verbose) { 
    fprintf(stdout, "Attempting to convert IFs to MAXs and MINs\n"); 
    fprintf(TFile, "Attempting to convert IFs to MAXs and MINs\n"); 
  } 
  du = Du_Mgr;
  If_MinMax_Traverse(func_nd); 
  if (LNO_Verbose) { 
    fprintf(stdout, "Finished converting IFs to MAXs and MINs\n"); 
    fprintf(TFile, "Finished converting IFs to MAXs and MINs\n"); 
  } 
}
