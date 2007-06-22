/*
 *  Copyright (C) 2006. QLogic Corporation. All Rights Reserved.
 */

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


/* ====================================================================
 * ====================================================================
 *
 * Module: whirl2ops.cxx
 * $Revision: 1.1.1.1 $
 * $Date: 2005/10/21 19:00:00 $
 * $Author: marcel $
 * $Source: /proj/osprey/CVS/open64/osprey1.0/be/cg/whirl2ops.cxx,v $
 *
 * Description:
 *
 * This	file contains the routines to convert WHIRL which is input to cg
 * into OPs. This includes creating basic blocks, TNs.
 *
 * ====================================================================
 * ====================================================================
 */

#include <stdint.h>
#include <alloca.h>
#include <ctype.h>
#include <vector>

#include "defs.h"
#include "cg_flags.h"
#include "mempool.h"
#include "wn.h"
#include "symtab.h"
#include "const.h"
#include "erbe.h"
#include "erglob.h"
#include "tracing.h"
#include "config.h"
#include "config_TARG.h"
#include "topcode.h"
#include "targ_isa_lits.h"
#include "intrn_info.h"
#include "opcode.h"
#include "w2op.h"
#include "wn_util.h"
#include "ir_reader.h"
#include "region_util.h"
#include "cg_region.h"
#include "bb.h"
#include "op.h"
#include "tn.h"
#include "register.h"
#include "calls.h"
#include "cgexp.h"
#include "stblock.h"
#include "targ_sim.h"
#include "irbdata.h"
#include "ttype.h"
#include "op_map.h"
#include "pf_cg.h"
#include "wn_map.h"
#include "whirl2ops.h"
#include "gtn_universe.h"
#include "cg.h"
#include "cg_internal.h"
#include "variants.h"
#include "targ_sim.h"
#include "eh_region.h"
#include "fb_whirl.h"
#include "xstats.h"
#include "data_layout.h"
#include "cgtarget.h"
#include "cg_spill.h"
#include "label_util.h"
#include "comment.h"
#include "be_symtab.h"
#include "be_util.h"
#include "config_asm.h"

#ifdef TARG_X8664
#include "cgexp_internals.h"
#endif
#ifdef KEY
#include "cxx_template.h" // for STACK
#endif

#ifdef EMULATE_LONGLONG
extern void Add_TN_Pair (TN*, TN*);
extern TN *If_Get_TN_Pair(TN*);
extern TN *Gen_Literal_TN_Pair(UINT64);
#endif

BOOL Compiling_Proper_REGION;
static BOOL Trace_WhirlToOp = FALSE;

/* reference to a dedicated TN in Cur_BB */
static BOOL dedicated_seen;

static BOOL In_Glue_Region = FALSE;	/* in glue-code region */

/* Forward declarations. */
static TN * Expand_Expr (WN *expr, WN *parent, TN *result);
static void initialize_region_stack(WN *);
static RID *region_stack_pop(void);
static void region_stack_push(RID *value);
static void region_stack_eh_set_has_call(void);
static VARIANT WHIRL_Compare_To_OP_variant (OPCODE opcode, BOOL invert);

#ifdef KEY
// Expose the vars to the target-specific expand routines.
#define WHIRL2OPS_STATIC
#else
#define WHIRL2OPS_STATIC static
#endif

/* The cgexp routines now take as input an OPS to which the
 * expanded OPs are added.
 */
WHIRL2OPS_STATIC OPS New_OPs;

WHIRL2OPS_STATIC OP *Last_Processed_OP;
WHIRL2OPS_STATIC SRCPOS current_srcpos;
WHIRL2OPS_STATIC INT total_bb_insts;

/* The current basic block being generated. */
WHIRL2OPS_STATIC BB *Cur_BB;

static RID **region_stack_base;
static RID **region_stack_ptr;
static INT   region_stack_size;
#define current_region (*(region_stack_ptr - 1))
#define region_depth   (region_stack_ptr - region_stack_base)
static BB_NUM min_bb_id;

static WN *last_loop_pragma;

#define return_max 3

/*
 * Last_Mem_OP is used to keep track of the last
 * op processed for the memory op to wn mapping.
 */

static OP *Last_Mem_OP;
OP_MAP OP_to_WN_map;
static OP_MAP predicate_map = NULL;
static WN_MAP WN_to_OP_map;
// map between load (GOT entry) and the associated symbol
OP_MAP OP_Ld_GOT_2_Sym_Map; 

OP_MAP OP_Asm_Map;

#ifdef TARG_IA64
inline BOOL 
WN_Is_Bool_Operator(WN* expr) 
{
  OPERATOR opr;
  opr = WN_operator(expr); 
  if(opr == OPR_EQ || 
     opr == OPR_NE || 
     opr == OPR_LE || 	
     opr == OPR_LT || 
     opr == OPR_GE || 
     opr == OPR_GT )
  {
    return TRUE;
  }else{
    return FALSE;
  }
}

/*
 *     Identify wn like this:
 * 
 *       BBLDID 
 *     I8BCVT
 */

inline BOOL
WN_Is_Convert_Bool(WN* expr)
{
  if(WN_operator_is(expr,OPR_CVT)           &&
     WN_desc(expr) == MTYPE_B               &&
     WN_operator_is(WN_kid0(expr),OPR_LDID) &&
     WN_rtype(WN_kid0(expr)) == MTYPE_B     &&
     WN_class(WN_kid0(expr)) == CLASS_PREG){
    return TRUE;  
  }else{
    return FALSE;  
  }  
}


inline BOOL 
Is_Old_Boolean_Expression(WN* expr)
{
  if(WN_opcode(expr) == OPC_I8BAND || 
     WN_opcode(expr) == OPC_I8BIOR )
  {
    WN* kid0 = WN_kid0(expr);    
    WN* kid1 = WN_kid1(expr);    
    if((WN_Is_Bool_Operator(kid0) || WN_Is_Convert_Bool(kid0)) &&
       (WN_Is_Bool_Operator(kid1) || WN_Is_Convert_Bool(kid1))){
      return TRUE;
    }
  } 
  return FALSE;
}


/*
This function is to transform poorly-handled boolean processing to predicated processing in IA64. 
Predicates is returned as the result.
Entry-check is in function Handle_CONDBR.
Boolean expression must be like:

	A && B
	A || B
        CVT && A  or A && CVT
        CVT || A  or A || CVT
        CVT1 && CVT2
        CVT1 || CVT2
        
A or B is a boolean expression contains only one logical operator(EQ, NE, LE, LT, GE, GT). A/B must be of
type integer, short or char. Signed/unsigned doesn't matter.
CVT is a convert operator. It's desc type must be Boolean. It's kid0 must be a LDID with rtype = MTYPE_B.
Moreover, WN_class(kid0) should be CLASS_PREG

The transformation looks like:

        A && B -->
                     Pi,Pj = A
                     cmp.eq Pn,Pm = r0,r0 (Pm = 0)
                (Pi) Pm,Pn = B
                (Pm) TRUEBR  or 
                (Pn) FALSEBR


        A || B -->
                     Pi,Pj = A
                     cmp.eq Pm,Pn = r0,r0 (Pm = 1)
                (Pj) Pm,Pn = B
                (Pm) TRUEBR   or
                (Pn) FALSEBR
                

        CVT && A 
        A && CVT -->
                     Pi,Pj = CVT
                     cmp.eq Pn,Pm = r0,r0 (Pm = 0)
                (Pi) Pm,Pn = A
                (Pm) TRUEBR   or
                (Pn) FALSEBR

		
        CVT || A
        A || CVT -->
                     Pi,Pj = CVT
                     cmp.eq Pm,Pn = r0,r0 (Pm = 1)
                (Pj) Pm,Pn = A
                (Pm) TRUEBR   or 
                (Pn) FALSEBR

		
        CVT1 && CVT2 -->
                         Pi,Pj = CVT1
                         Pm,Pn = CVT2
                         cmp.eq Ps,Pt = r0,r0 (Ps = 1)
                    (Pj) cmp.eq Pt,Ps = r0,r0
                    (Pn) cmp.eq Pt,Ps = r0,r0
                    (Ps) TRUEBR   or
                    (Pt) FALSEBR

		    
        CVT1 || CVT2 -->
                         Pi,Pj = CVT1
                         Pm,Pn = CVT2
                         cmp.eq Pt,Ps = r0,r0 (Ps = 0)
                    (Pi) cmp.eq Ps,Pt = r0,r0
                    (Pm) cmp.eq Ps,Pt = r0,r0
                    (Ps) TRUEBR   or   
                    (Pt) FALSEBR


P0 is the predicate always true. Pm/Pn is predicate pair to be returned.

Please note that this is not a perfect solution. More complete solution needs to use LAND/LIOR instead of BAND/BIOR
in previous phases in boolean expression processing.
This modification assumes below expressions are short-circuited:
        1.Float compare; won't have problem;
        2.Post/pre increment/decrement;
        3.Operands as function;
*/

static TN*
Handle_Bool_As_Predicate(WN*condition, WN*parent, BOOL invert)
{
    WN* kid0 = NULL;
    WN* kid1 = NULL;
    OP* last_op = NULL;
    OP* kid0_last_op = NULL;
    OP* kid1_last_op = NULL;
    TN* kid0_result = NULL;
    TN* kid1_result = NULL;
    TN* predicate_result0 = NULL;
    TN* predicate_result1 = NULL;
    TN* guarder = NULL;
    OP* first_guardee = NULL;
    OP *preset_op = NULL;
    OPERATOR condition_opr = WN_operator(condition);

    if (Trace_WhirlToOp)
      fprintf(TFile, "Use Predicate register to replace band/bior operation in boolean expression!\n");

    kid0 = WN_kid0(condition);
    kid1 = WN_kid1(condition);
    kid0_result = Build_TN_Of_Mtype(MTYPE_B);
    kid1_result = Build_TN_Of_Mtype(MTYPE_B);

    last_op = OPS_last(&New_OPs);
    
    if(WN_Is_Bool_Operator(kid0)){
      Expand_Expr(kid0, condition, kid0_result);
    }else if(WN_Is_Convert_Bool(kid0)){
      kid0_result = Expand_Expr(WN_kid0(kid0), kid0, NULL);                 
      Is_True(OPS_last(&New_OPs) == last_op,("kid0: kid of CVT can not generate new OPs!")); 
    }else{
      Is_True(FALSE,("Can not handle operators other than bool and convert!"));
    }
    kid0_last_op = OPS_last(&New_OPs);

    if(WN_Is_Bool_Operator(kid1)){
      Expand_Expr(kid1, condition, kid1_result);
    }else if(WN_Is_Convert_Bool(kid1)){
      kid1_result = Expand_Expr(WN_kid0(kid1), kid0, NULL);                  
      Is_True(OPS_last(&New_OPs) == kid0_last_op,("kid1: kid of CVT can not generate new OPs!")); 
    }else{
      Is_True(FALSE,("Can not handle operators other than bool and convert!"));
    }
    kid1_last_op = OPS_last(&New_OPs);

    if(WN_Is_Convert_Bool(kid0) && WN_Is_Convert_Bool(kid1)){
      
      // CVT1 && CVT2; CVT1 || CVT2
      Is_True(kid0_last_op == last_op,("CVT-LDID can not generate OPs!"));
      Is_True(kid1_last_op == last_op,("CVT-LDID can not generate OPs!"));

      predicate_result0 = Build_TN_Of_Mtype(MTYPE_B);      
      predicate_result1 = Build_TN_Of_Mtype(MTYPE_B);      

      TN* kid0_guard = condition_opr == OPR_BIOR ? kid0_result : Get_Complement_TN(kid0_result); 
      TN* kid1_guard = condition_opr == OPR_BIOR ? kid1_result : Get_Complement_TN(kid1_result);

      OP* op1;
      OP* op2;
      if(condition_opr == OPR_BIOR){
        op1 = Mk_OP(TOP_cmp_eq_or_andcm, predicate_result0, predicate_result1, kid0_guard, Zero_TN, Zero_TN);
        op2 = Mk_OP(TOP_cmp_eq_or_andcm, predicate_result0, predicate_result1, kid1_guard, Zero_TN, Zero_TN);
        preset_op = Mk_OP(TOP_cmp_eq, predicate_result1, predicate_result0, True_TN, Zero_TN, Zero_TN);
      }else if(condition_opr == OPR_BAND){
        op1 = Mk_OP(TOP_cmp_eq_or_andcm, predicate_result1, predicate_result0, kid0_guard, Zero_TN, Zero_TN);
        op2 = Mk_OP(TOP_cmp_eq_or_andcm, predicate_result1, predicate_result0, kid1_guard, Zero_TN, Zero_TN);
        preset_op = Mk_OP(TOP_cmp_eq, predicate_result0, predicate_result1, True_TN, Zero_TN, Zero_TN);
      }else{
        Is_True(FALSE,("Can not get to here!")); 
      }
      OPS_Append_Op(&New_OPs,preset_op); 
      OPS_Append_Op(&New_OPs,op1); 
      OPS_Append_Op(&New_OPs,op2);

    }else{

      if(WN_Is_Convert_Bool(kid0) || WN_Is_Convert_Bool(kid1)){

        // CVT && A or A && CVT; CVT || A or A || CVT
        TN* tmp_pred;
        if(WN_Is_Convert_Bool(kid0)){
          tmp_pred = kid0_result;
        }else{
          tmp_pred = kid1_result;
        }
        guarder = condition_opr == OPR_BAND ? tmp_pred : Get_Complement_TN(tmp_pred);
        first_guardee = last_op == NULL ? OPS_first(&New_OPs) : OP_next(last_op);
        FmtAssert(first_guardee != NULL, ("boolean expression doesn't generate OPs"));

      }else{

        // A && B;  A || B     
        Is_True(WN_Is_Bool_Operator(kid0) && WN_Is_Bool_Operator(kid1),("kid0 and kid1 should be boolean expression!"));
        guarder = OP_result(kid0_last_op, condition_opr == OPR_BAND ? 0:1);
        first_guardee = OP_next(kid0_last_op);
        FmtAssert(first_guardee != NULL, ("boolean expression doesn't generate OPs"));
      }

      BOOL all_true = TRUE;
      for(OP* op = first_guardee; op; op = OP_next(op)){
        if(OP_opnd(op,0) != True_TN)
          all_true = FALSE; 
      }

      if(all_true){
        for(OP* op = first_guardee; op; op = OP_next(op)){
          Set_OP_opnd(op, 0, guarder);
        }
      }else{
        Set_OP_opnd(kid1_last_op, 0, guarder);
      }

      predicate_result0 = OP_result(kid1_last_op, 0);
      predicate_result1 = OP_result(kid1_last_op, 1);
    
      // insert OP to preset predicate_result0/1 properly
      // Pls note that this op won't be added to WN2OP and OP2WN map.

      if(condition_opr == OPR_BAND) {
        preset_op = Mk_OP(TOP_cmp_eq_unc, predicate_result1, predicate_result0, True_TN, Zero_TN, Zero_TN);
        OPS_Insert_Op_Before(&New_OPs, first_guardee, preset_op);
      }else if(condition_opr == OPR_BIOR) {
        preset_op = Mk_OP(TOP_cmp_eq_unc, predicate_result0, predicate_result1, True_TN, Zero_TN, Zero_TN);
        OPS_Insert_Op_Before(&New_OPs, first_guardee, preset_op);
      }else{
        FmtAssert(FALSE, ("Condition error. This should never happen.\n"));
      }
    }

    if (Trace_WhirlToOp) {
      fprintf(TFile, "After if-conv has been done: \n");
      OP* op = last_op == NULL ? OPS_first(&New_OPs) : OP_next(last_op);
      for(; op; op = OP_next(op)){
        Print_OP(op);
      }
    }

    // Change the result type
    WN_set_rtype(condition, MTYPE_B);
    return (invert) ? predicate_result1 : predicate_result0;
}
#endif

TN *
Get_Complement_TN(TN *tn)
{
  TN *c_tn;
  PREG_NUM preg = TN_To_PREG(tn);
  if (preg != 0) {
    PREG_NUM c_preg = preg + 1;
    c_tn = PREG_To_TN_Array[c_preg];
  } else {
    c_tn = Build_TN_Like(tn);
  }
  return c_tn;
}


void Copy_WN_For_Memory_OP(OP *dest, OP *src)
{
  WN *wn = Get_WN_From_Memory_OP(src);
  UINT64 predicate = predicate_map ? OP_MAP64_Get(predicate_map, src) : 0;
  if (wn)
    OP_MAP_Set(OP_to_WN_map, dest, wn);
  if (predicate)
    OP_MAP64_Set(predicate_map, dest, predicate);
#ifdef TARG_X8664
  if( Is_Target_32bit() &&
      OP_memory_hi( src ) ){
    Set_OP_memory_hi( dest );
  }
#endif
}


OP *Get_OP_From_WN(WN *wn ) 
{
  return (OP*) WN_MAP_Get(WN_to_OP_map, wn);
}


void Set_Memory_OP_Predicate_Info(OP *memop, TN *pred_tn, UINT8 omega,
				  BOOL inverted)
{
  UINT64 predicate = pred_tn ?
    (TN_number(pred_tn) | (UINT64)omega << 32 | (UINT64)inverted << 40) : 0;
  if (predicate_map == NULL && predicate) predicate_map = OP_MAP64_Create();
  if (predicate || predicate_map && OP_MAP64_Get(predicate_map, memop))
    OP_MAP64_Set(predicate_map, memop, predicate);
}
  
void Get_Memory_OP_Predicate_Info(OP *memop, TN **pred_tn, UINT8 *omega,
				  BOOL *inverted)
{
  UINT64 predicate = predicate_map ? OP_MAP64_Get(predicate_map, memop) : 0;
  *pred_tn = predicate ? TNvec(predicate & 0xffffffff) : NULL;
  *omega = (predicate >> 32) & 0xff;
  *inverted = (predicate >> 40) & 1;
}


/* =======================================================================
 *
 *  initialize_region_stack
 *
 *  initialize the stack giving the current region number.
 *  Make sure that the POOL we use is pushed and popped somewhere.
 *
 * =======================================================================
 */
static void initialize_region_stack(WN *wn)
{
  RID *rid = REGION_get_rid(wn);
  Is_True(rid != NULL, ("initialize_region_stack, NULL RID"));

  region_stack_size = 8;
  region_stack_base = TYPE_MEM_POOL_ALLOC_N( RID *, &MEM_local_pool,
					    region_stack_size );
  region_stack_ptr = region_stack_base;

  region_stack_push( rid );
}

/* =======================================================================
 *
 *  region_stack_pop
 *
 *  pop an element from the region stack
 *
 * =======================================================================
 */
static RID *region_stack_pop(void)
{
  if ( region_stack_ptr == region_stack_base )
    return NULL;
  else
    return *(--region_stack_ptr);
}

/* =======================================================================
 *
 *  region_stack_push
 *
 *  push an element on the region stack; grow the stack if necessary
 *
 * =======================================================================
 */
static void region_stack_push(RID *value)
{
  *(region_stack_ptr++) = value;
  if ( ( region_stack_ptr - region_stack_base ) == region_stack_size ) {
    region_stack_base = TYPE_MEM_POOL_REALLOC_N( RID *, &MEM_local_pool,
						 region_stack_base, region_stack_size, 
						 2 * region_stack_size );
    region_stack_ptr = region_stack_base + region_stack_size;
    region_stack_size = 2 * region_stack_size;
  }
}

/* =======================================================================
 *
 *  region_stack_eh_set_has_call
 *
 *  call EH_Set_Has_Call for every EH region on the region stack
 *
 * =======================================================================
 */

static void region_stack_eh_set_has_call(void)
{
  RID ** p;
  for (p = region_stack_ptr - 1; p >= region_stack_base; --p)
#ifdef KEY
    if (RID_TYPE_eh(*p) && RID_eh_range_ptr(*p))
#else
    if (RID_TYPE_eh(*p))
#endif
      EH_Set_Has_Call(RID_eh_range_ptr(*p));
}
  


#ifdef TARG_X8664
static BOOL WN_pragma_preamble_end_seen = FALSE;

BOOL W2OPS_Pragma_Preamble_End_Seen ()
{
   return WN_pragma_preamble_end_seen;
}
#endif
/* Process the new OPs that have been created since the last call to 
 * this routine. We set their srcpos field and increment the count
 * of number of OPs in the BB. We check if any of the new OPs have
 * a reference to GP and remember the fact.
 */
#ifndef KEY
static
#endif
void
Process_New_OPs (void)
{
  OP *op;
  INT i;

  op = Last_Processed_OP ? OP_next(Last_Processed_OP) : OPS_first(&New_OPs);

  for (; op != NULL; op = OP_next(op)) {
    for (i = OP_opnds(op)-1; i >= 0; i--) {
      if (OP_opnd(op,i) == GP_TN) PU_References_GP = TRUE;
    }
    OP_srcpos(op) = current_srcpos;
    total_bb_insts++;
#ifdef TARG_X8664
    if (WN_pragma_preamble_end_seen) {
      Set_OP_first_after_preamble_end(op);
      WN_pragma_preamble_end_seen = FALSE;
    }
#endif
  }
  Last_Processed_OP = OPS_last(&New_OPs);
}

#ifdef EMULATE_LONGLONG
/***********************************************************************
 *
 * Assume that orig_bb was split into orig_bb followed by new_bb,
 * with the ops being split between the two.
 * 
 * Update Annotations, flags etc appropriately.
 *
 ***********************************************************************/
static void 
Update_BB_Properties (BB *orig_bb, BB *new_bb) {

  BB_rid(new_bb) = BB_rid(orig_bb);

  ANNOTATION *ant = BB_annotations(orig_bb);

  while (ant) {
    switch (ANNOT_kind(ant)) {
    case ANNOT_LABEL:
      // this stays with orig_bb
      break;
    case ANNOT_PRAGMA:
      #pragma mips_frequency_hint NEVER
      FmtAssert (FALSE,
                 ("Update_BB_Properties: foxed by pragma annotation\n"));
      
      /*NOTREACHED*/
    case ANNOT_ENTRYINFO:
      // this stays with orig_bb;
      Reset_BB_entry(new_bb);
      break;
    case ANNOT_EXITINFO:
    case ANNOT_CALLINFO:
    case ANNOT_ASMINFO:
      // exitinfo, callinfo and asminfo move to new_bb
      BB_annotations(orig_bb) = ANNOT_Unlink(BB_annotations(orig_bb),
                                             ant);
      ANNOT_next(ant) = BB_annotations(new_bb);
      BB_annotations(new_bb) = ant;

      if (ANNOT_kind(ant) == ANNOT_CALLINFO) {
        Reset_BB_call(orig_bb);
        Set_BB_call(new_bb);
      } else if (ANNOT_kind(ant) == ANNOT_ASMINFO) {
	Reset_BB_asm(orig_bb);
	Set_BB_asm(new_bb);
      } else {
        Reset_BB_exit(orig_bb);
        Set_BB_exit(new_bb);
      }
      break;
    case ANNOT_NOTE: // we should tolerate comment
      break;
    case ANNOT_LOOPINFO:
    case ANNOT_SWITCH:
    case ANNOT_ROTATING_KERNEL:
    default:
      #pragma mips_frequency_hint NEVER
      FmtAssert (FALSE,
                 ("Update_BB_Properties: Unknown annotation %d\n", ANNOT_kind(ant)));
      /*NOTREACHED*/
    }
    ant = ANNOT_next(ant);
  }
  
}

/***********************************************************************
 *
 * Given a BB, we may need to split it if it contains a
 * jump or a label (or both) in its midst. If so, split it into one or more
 * BBs, so that each of the resulting BBs is guaranteed to not have
 * labels in the middle (through TOP_label) or jumps in the middle.
 * 
 * Return a pointer to the last bb created.
 *
 ***********************************************************************/
static BB *
Split_Jumpy_BB (BB *bb) {

  BOOL need_to_split = FALSE;
  OP *op;
  for (op = BB_first_op(bb); op; op = OP_next(op)) {
    if (OP_xfer(op) && op != BB_last_op(bb)) {
      need_to_split = TRUE;
      break;
    }
    if (OP_code(op) == TOP_label) {
      need_to_split = TRUE;
      break;
    }
    if (op == BB_last_op(bb)) break;
  }

  if (!need_to_split) return bb;

  // do the split
  for (op = BB_first_op(bb); op; ) {

    // Split if it is a jump
    if (OP_xfer(op) && op != BB_last_op(bb)) {
      BB *new_bb = Gen_And_Append_BB (bb);
      op = OP_next(op);

      while (op) {
        OP *tmp_op = OP_next(op);
        BB_Move_Op_To_End (new_bb, bb, op);
        op = tmp_op;
      }

      Update_BB_Properties (bb, new_bb);
      bb = new_bb;
      op = BB_first_op(new_bb);
      continue;
    }

    // split if it is a label
    if (OP_code(op) == TOP_label) {
      BB *new_bb = Gen_And_Append_BB (bb);
      while (op) {
        OP *tmp_op = OP_next(op);
        BB_Move_Op_To_End (new_bb, bb, op);
        op = tmp_op;
      }

      // setup the label, and then 
      // reset values so we can continue outer for
      Update_BB_Properties (bb, new_bb);
      bb = new_bb;
      op = BB_first_op(new_bb);
      Is_True (OP_code(op) == TOP_label,
               ("Split_Jumpy_BB: where did the dummy label go?"));
      TN *label_tn = OP_opnd(op,0);
      LABEL_IDX label = TN_label(label_tn);
      BB_Add_Annotation(bb, ANNOT_LABEL, (void*) label);
      FmtAssert (Get_Label_BB(label) == NULL,
                 ("Split_Jumpy_BB: Label %s defined more than once",
                  LABEL_name(label)));
      Set_Label_BB (label,bb);
      Set_BB_has_label(bb);
      BB_Remove_Op (bb, op);
      op = BB_first_op(bb);
      continue;
    }

    if (op == BB_last_op(bb)) break;

    op = OP_next(op);
  }
  
  return bb;
}
#endif /* EMULATE_LONGLONG */


/* Start a new basic block. Any OPs that have not been put into a BB
 * are added to the current basic block before we create a new one.
 */
static BB *
Start_New_Basic_Block (void)
{
  BB *bb = Cur_BB;

  if (bb == NULL) {
    // Cur_BB is NULL if we don't have any basic blocks in the PU yet.
    // Mark this BB as REGION_First_BB.
    bb = Gen_And_Append_BB (bb);
    REGION_First_BB = bb;
  }
  // check if we already have instructions in the basic block
  FmtAssert (BB_first_op(bb) == NULL, ("Start_New_Basic_Block: error"));

  if (OPS_first(&New_OPs) != NULL) {
    Process_New_OPs ();
    BB_Append_Ops(bb, &New_OPs);
    OPS_Init(&New_OPs);
#ifdef EMULATE_LONGLONG
    bb = Split_Jumpy_BB (bb);
#endif
    if ( dedicated_seen )
      Set_BB_has_globals( bb );
    else
      Reset_BB_has_globals( bb );
    bb = Gen_And_Append_BB (bb);
  } else if (BB_entry(bb) || BB_exit(bb)) {
    // If current basic block is a entry point or an exit, create a new
    // basic block, even if the current one is empty. This is because code 
    // will be added later to the entry/exit bb.
    bb = Gen_And_Append_BB (bb);
  }

  if (PU_has_region(Get_Current_PU()))
    BB_rid(bb) = Non_Transparent_RID(current_region);
  else
    BB_rid(bb) = NULL;

  total_bb_insts = 0;
  Last_Processed_OP = NULL;
  Last_Mem_OP = NULL;
  Cur_BB = bb;
  dedicated_seen = FALSE;

  return bb;
}

/* Check if we are over the threshold of how many instructions
 * we will allow in a single basic block. If we are, then call 
 * 'Start_New_Basic_Block' to start a new basic block.
 */
static void
Process_OPs_For_Stmt (void)
{
  Process_New_OPs ();

#if 0	// now done later in Split_BBs()
  if (Enable_BB_Splitting && (total_bb_insts > Split_BB_Length)) {
    /* We assume in LRA that the number of instructions in a BB fits
     * in 16 bits.
     */
    FmtAssert (total_bb_insts < 32768,
  	  ("Convert_WHIRL_To_OPs: Too many instructions for 1 statment (%d)\n", 
	   total_bb_insts));
    if (Trace_WhirlToOp) {
          fprintf (TFile, "Convert_WHIRL_To_OPs: splitting a large BB (%d)\n", 
		    total_bb_insts);
    }
    Start_New_Basic_Block ();
  }
#endif
}


/* Create a new result TN for the WHIRL node wn. We pass in the opnd_tn
 * array that contains the TNs allocated for each operand. This could
 * used to do value numbering if desired.
 *
 * Currently we allocate a new TN for every invocation of this procedure.
 * 
 */
static TN *
Allocate_Result_TN (WN *wn, TN **opnd_tn)
{
#ifdef EMULATE_LONGLONG

  TYPE_ID mtype = WN_rtype(wn);
  if (mtype == MTYPE_I8 || mtype == MTYPE_U8) {
    TYPE_ID new_mtype = (mtype == MTYPE_I8 ? MTYPE_I4 : MTYPE_U4);
    TN *tn  = Build_TN_Of_Mtype (new_mtype);
    TN *tn2 = Build_TN_Of_Mtype (new_mtype);
    Add_TN_Pair (tn, tn2);
    return tn;
  }
  else return Build_TN_Of_Mtype (WN_rtype(wn));

#else 
#ifdef TARG_X8664 
  if (Is_Target_64bit() && WN_operator(wn) == OPR_INTRINSIC_OP &&
      ((INTRINSIC) WN_intrinsic(wn)) == INTRN_CTZ)
    // bug 11315: use PARM's rtype because bsfq's result needs to be 64-bit reg
    return Build_TN_Of_Mtype (WN_rtype(WN_kid0(wn)));
  else
#endif
  return Build_TN_Of_Mtype (WN_rtype(wn));
#endif
}


/* set the op2wn mappings for memory ops
 */
static void
Set_OP_To_WN_Map(WN *wn)
{
  OP *op;

  // We don't have aliasing information at -O0 and -O1.
  if (CG_opt_level < 2) return;

  op = Last_Mem_OP ? OP_next(Last_Mem_OP) : OPS_first(&New_OPs);
  for ( ; op != NULL; op = OP_next(op)) {
    if ( (!OP_memory(op) 
#ifndef CG_PATHSCALE_MERGE // GRA's homing rely on WN node to determine if an op is a home
	    // location load/store; since no_alias variables are also homeable,
	    // need to remove !no_alias as condition to create the WN mapping
			 || OP_no_alias(op)
#endif
					) && !OP_call(op) 
	&& !CGTARG_Is_OP_Barrier(op) && OP_code(op) != TOP_spadjust) 
	continue;
    if (OP_memory(op) && WN_Is_Volatile_Mem(wn)) Set_OP_volatile(op);
    if (OP_prefetch(op)) {

      /* These maps will have already been created if Alias_Manager
       * is TRUE. However, if we have prefetches and no alias manager
       * (compiled with -O3 -PHASE:w=0 for example) then we create it
       * the first time we need it since we can't otherwise predict
       * it and we don't want to create it for -O0, etc.
       */
      if (OP_to_WN_map == NULL) {
	OP_to_WN_map = OP_MAP_Create();
	Is_True(WN_to_OP_map == WN_MAP_UNDEFINED,
		("WN_to_OP_map has already been created"));
	WN_to_OP_map = WN_MAP_Create(&MEM_phase_pool);
      }
      OP_MAP_Set(OP_to_WN_map, op, wn);
      WN_MAP_Set(WN_to_OP_map, wn, op);
    } else if (Alias_Manager) {
      OP_MAP_Set(OP_to_WN_map, op, wn);
    }
  }

  Last_Mem_OP = OPS_last(&New_OPs);
}

/* don't home structs or arrays of arrays for now */
static BOOL Disallowed_Homeable(ST *sym)
{
  if (Is_Structure_Type(ST_type(sym))) {
    return TRUE;
  } else if (TY_kind(ST_type(sym)) == KIND_ARRAY &&
	     (TY_kind(TY_AR_etype(ST_type(sym))) == KIND_STRUCT || 
	      TY_kind(TY_AR_etype(ST_type(sym))) == KIND_ARRAY)) {
    return TRUE;
  } 
  return FALSE;
}

/*
 * check if TN is rematerializable. Can't handle IDNAME's right now 
 * filter out things like IDNAME, FQCONST and uplevel references
 */
extern WN* 
Preg_Is_Rematerializable(PREG_NUM preg, BOOL *gra_homeable)
{
  WN	*home= Preg_Home(preg);
  OPCODE opc;

  if (home == NULL) 
	return NULL;
  opc = WN_opcode(home);

#ifdef TARG_X8664
  /* GRA does not understand -m32, and gra will reload a double
     type data from memory, yet only assign one register to this value.

     TODO: do we need to spend effort on gra? Not much work that gra can
           do with poor supply of available registers under -m32.
  */
  if( OP_NEED_PAIR( OPCODE_rtype(opc) ) ){
    return NULL;
  }

  if( mcmodel >= MEDIUM ){
    return NULL;
  }
#endif

  /* allow homing on symbols with simple addressing */
  if (WN_operator(home) == OPR_LDID) {
    ST *sym = WN_st(home);
    ST *basesym = Base_Symbol(sym);
    TYPE_ID rtype = OPCODE_rtype(opc);

#ifdef CG_PATHSCALE_MERGE
    if (ST_sclass(sym) == SCLASS_FORMAL_REF)
      return NULL; // the dereferenced value has no home location
    if (ST_is_uplevelTemp(sym))
      return NULL; // homing to the uplevel stack location is expensive
#endif

    /* can't handle quad's without lowerer support.  defer for now.
     * shouldn't see complex as they're expanded when the optimizer
     * sees them, but what the heck.
     */
    if (MTYPE_is_quad(rtype)) {
      return NULL;
    }
    if (MTYPE_is_complex(rtype)) {
      return NULL;
    }

    /* can't do homing without the alias manager.  we shouldn't get
     * here anyway as we won't home without running wopt and thus
     * should have an alias manager, but let's be paranoid.
     */
    if ( !Alias_Manager ) {
      return NULL;
    }

    /* disallow complex data types, but allow common blocks (which sorta
     * look like a complex data type).
     */
    if (GRA_home == TRUE && !Disallowed_Homeable(sym) &&
	ST_class(sym) == CLASS_VAR) {
#if !defined(TARG_X8664) && !defined(TARG_IA32) // skip test of GPREL for CISC's
      if (ST_gprel(basesym) ||
	  (ST_is_split_common(basesym) && ST_gprel(ST_full(basesym)))
	  || ST_on_stack(sym)) 
#endif
      {
#ifdef CG_PATHSCALE_MERGE
	if( gra_homeable != NULL )
#endif
	  *gra_homeable = TRUE;
	return home;
      }
    }
    return NULL;
  }
  if (MTYPE_is_complex(OPCODE_rtype(opc)))
	return NULL;
  if (MTYPE_is_quad(OPCODE_rtype(opc)))
	return NULL;
  if (OPCODE_has_sym(opc) && ST_is_uplevelTemp(WN_st(home)))
	return NULL;

  return home;
}

/* array to map PREGs into TNs. */
TN **PREG_To_TN_Array;
TYPE_ID *PREG_To_TN_Mtype;
static PREG_NUM max_preg_to_tn_index = 0;

// if we create new pregs, then need to realloc space.
static void Realloc_Preg_To_TN_Arrays (PREG_NUM preg_num)
{
  if (preg_num < max_preg_to_tn_index) return;
  	
  max_preg_to_tn_index = Get_Preg_Num (PREG_Table_Size(CURRENT_SYMTAB));
  PREG_To_TN_Array = TYPE_MEM_POOL_REALLOC_N( TN *, &MEM_pu_pool,
	 PREG_To_TN_Array, max_preg_to_tn_index, max_preg_to_tn_index + 10);
  PREG_To_TN_Mtype = TYPE_MEM_POOL_REALLOC_N( TYPE_ID, &MEM_pu_pool,
	 PREG_To_TN_Mtype, max_preg_to_tn_index, max_preg_to_tn_index + 10);
  max_preg_to_tn_index += 10;
}

/* function exported externally for use in LRA. */
TN *
PREG_To_TN (ST *preg_st, PREG_NUM preg_num)
{
  TN *tn;

  Is_True((preg_num <= Get_Preg_Num(PREG_Table_Size(CURRENT_SYMTAB))),
	    ("PREG_To_TN(): preg %d > SYMTAB_last_preg(%d)",
	   preg_num, Get_Preg_Num(PREG_Table_Size(CURRENT_SYMTAB))));

  tn = PREG_To_TN_Array[preg_num];
#ifdef CG_PATHSCALE_MERGE
  // Bug 5159 - ASM preg_num are negative and should not use PREG_To_TN_Array
  // and PREG_To_TN_Mtype arrays.
  if (preg_num < 0)
    tn = NULL;
#endif
  if (tn == NULL)
  {
    ISA_REGISTER_CLASS rclass;
    REGISTER reg;

    if (CGTARG_Preg_Register_And_Class(preg_num, &rclass, &reg))
    {
	Is_True(!Is_Predicate_REGISTER_CLASS(rclass),
		("don't support dedicate predicate pregs"));
#ifdef HAS_STACKED_REGISTERS
	if (ABI_PROPERTY_Is_stacked(
		rclass,
		REGISTER_machine_id(rclass, reg) )) 
	{
		reg = REGISTER_Allocate_Stacked_Register(
			(Is_Int_Output_Preg(preg_num) ? ABI_PROPERTY_caller 
						      : ABI_PROPERTY_callee),
			rclass, reg);

		if (PU_has_syscall_linkage(Get_Current_PU())
			&& ! Is_Int_Output_Preg(preg_num)) 
		{
			// syscall linkage means the input parameters
			// are preserved in the PU, so can restart.  
			// So mark the stacked register such that it
			// won't be available for future allocation
			// (i.e. it can be used if in whirl, but LRA
			// won't ever allocate it).
			// Note that we are assuming here that the only
			// stacked pregs whirl2ops will see will be either
			// input or output parameters.  If this is not true,
			// then instead will need to iterate thru params.
			REGISTER_Unallocate_Stacked_Register (rclass, reg);
		}
	}
#endif
      	tn = Build_Dedicated_TN(rclass, reg, ST_size(preg_st));

#ifdef TARG_X8664
	if( reg == First_Int_Preg_Return_Offset &&
	    Is_Target_32bit() ){
	  if( !CGTARG_Preg_Register_And_Class(Last_Int_Preg_Return_Offset,
					      &rclass, &reg) ){
	    FmtAssert( FALSE, ("NYI") );
	  }
	  TN* pair = Build_Dedicated_TN( rclass, reg, ST_size(preg_st) );
	  Create_TN_Pair( tn, pair );
	}
#endif

#ifdef EMULATE_LONGLONG
        // only on IA-32
        if (reg == First_Int_Preg_Return_Offset) {
          // dedicated and eax
          // map another TN for edx, its longlong pair,
          // for when this tn is used in longlong situations
          // such as longlong return values from functions
          if (CGTARG_Preg_Register_And_Class(Last_Int_Preg_Return_Offset,
                                             &rclass, &reg)) {
            TN *pair = Build_Dedicated_TN(rclass, reg,
                                          ST_size(preg_st));
            
            Add_TN_Pair (tn, pair);
          } else {
	    #pragma mips_frequency_hint NEVER
            FmtAssert (FALSE,
                       ("Could not find reg for Last_Int_Preg_Return_Offset"));
	    /*NOTREACHED*/
          }
        }
#endif
    }
    else
    {
      /* create a TN for this PREG. */
      TYPE_ID mtype = TY_mtype(ST_type(preg_st));
#ifdef TARG_X8664
      /* bug#512
	 MTYPE_C4 is returned in one SSE register. (check wn_lower.cxx)
      */
      if( mtype == MTYPE_C4 ){
	mtype = MTYPE_F8;
      }
#endif

#ifdef EMULATE_LONGLONG
      if (mtype == MTYPE_I8 || mtype == MTYPE_U8) {
        mtype = (mtype == MTYPE_I8 ? MTYPE_I4 : MTYPE_U4);
        tn = Build_TN_Of_Mtype(mtype);
        Add_TN_Pair (tn, Build_TN_Of_Mtype(mtype));
      } else {
        tn = Build_TN_Of_Mtype (mtype);
      }
#else
#ifdef TARG_X8664
      if( OP_NEED_PAIR(mtype) ){
        mtype = (mtype == MTYPE_I8 ? MTYPE_I4 : MTYPE_U4);
        tn = Build_TN_Of_Mtype(mtype);
        Create_TN_Pair( tn, mtype );

      } else
#endif // TARG_X8664
	tn = Build_TN_Of_Mtype (mtype);
#endif

      if (CGSPILL_Rematerialize_Constants)
      {
	BOOL gra_homeable = FALSE;
        WN *home= Preg_Is_Rematerializable(preg_num, &gra_homeable);

	if (home)
	{
	  if (gra_homeable) {
	    if (TN_number(tn) < GRA_non_home_lo ||
		TN_number(tn) > GRA_non_home_hi) {
	      Set_TN_is_gra_homeable(tn);
	      Set_TN_home (tn, home);
	    }
	  } else {
	    Set_TN_is_rematerializable(tn);
	    Set_TN_home (tn, home);
	  }
	}
      }

      TN_MAP_Set( TN_To_PREG_Map, tn, (void *)(INTPTR)preg_num );

      if (Is_Predicate_REGISTER_CLASS(TN_register_class(tn))) {

	// When we create a predicate TN, we actually need to create
	// a pair. The "true" TN corresponds to preg_num; the "false"
	// TN corresponds to preg_num+1.
	Is_True(!TN_is_gra_homeable(tn) && !TN_is_rematerializable(tn),
		("don't support homeable or rematerializable predicate preg"));
	PREG_NUM preg2_num = preg_num + 1;
	TN *tn2 = Build_TN_Of_Mtype (mtype);
	TN_MAP_Set( TN_To_PREG_Map, tn2, (void *)(INTPTR)preg2_num );
	PREG_To_TN_Array[preg2_num] = tn2;
	PREG_To_TN_Mtype[preg2_num] = mtype;
      }
    }
    if (Get_Trace (TP_CGEXP, 16)) {
	fprintf(TFile, "preg %d maps to tn %d", preg_num, TN_number(tn));
	if (TN_is_gra_homeable(tn)) {
	  fprintf(TFile, "(gra_homeable)\n");
	} else if (TN_is_rematerializable(tn)) {
	  fprintf(TFile, "(rematerializable)\n");
        } else {
          fprintf(TFile, "\n");
        }
    }
#ifndef CG_PATHSCALE_MERGE
    PREG_To_TN_Array[preg_num] = tn;
    PREG_To_TN_Mtype[preg_num] = TY_mtype(ST_type(preg_st));
#else
    // Bug 5159 - ASM preg_num are negative and should not use PREG_To_TN_Array
    // and PREG_To_TN_Mtype arrays.
    if (preg_num >= 0) {
      PREG_To_TN_Array[preg_num] = tn;
      PREG_To_TN_Mtype[preg_num] = TY_mtype(ST_type(preg_st));
    }
#endif
  }
  if ( TN_is_dedicated( tn ) ) {
    dedicated_seen = TRUE;
    // For dedicated FP registers, it is important that we use
    // a TN of the right size. So we create a new one if the
    // size of tn does not match the size of preg_st.
    if (TN_is_float(tn) && TN_size(tn) != ST_size(preg_st)) {
      tn = Build_Dedicated_TN (TN_register_class(tn),
                               TN_register(tn),
                               ST_size(preg_st));
    }
#ifdef CG_PATHSCALE_MERGE
    // Do the same for integer class; we have separate set of dedicated TNs
    // of size 4 bytes
    if (!TN_is_float(tn) &&
	TN_size(tn) != ST_size(preg_st)
#ifdef TARG_X8664
	&& Is_Target_64bit()
#endif // TARG_X8664
       ) {
      tn = Build_Dedicated_TN (TN_register_class(tn),
                               TN_register(tn),
                               ST_size(preg_st));
    }
#endif
  }

  return tn;
}


/* Return the physical PREG assigned to the <tn>. */
PREG_NUM
TN_To_Assigned_PREG (TN *tn)
{
  PREG_NUM i;

  FmtAssert (TN_register(tn) != REGISTER_UNDEFINED, 
    ("TN_To_Assigned_PREG: no assigned register for TN%d", TN_number(tn)));

  i = REGISTER_machine_id(TN_register_class(tn), TN_register(tn));
  if (TN_is_float(tn)) {
    i += Float_Preg_Min_Offset;
  }
#if defined(TARG_IA32) || defined(TARG_X8664)
  // There's no ZERO register: eax has machine_id 0, but preg_id 1
  else {
    i += Int_Preg_Min_Offset;
  }
#endif  
  return i;
}


/* See if there is a PREG corresponding to a TN. If there is, return the 
 * preg number, otherwise return 0. 
 * NOTE: This is currently very slow for non-dedicated TNs, use with care.
 */
PREG_NUM
TN_To_PREG (TN *tn)
{
  PREG_NUM i;

  if (TN_is_dedicated(tn)) {
    return TN_To_Assigned_PREG (tn);
  }

  for (i = Last_Dedicated_Preg_Offset; 
       i < Get_Preg_Num(PREG_Table_Size(CURRENT_SYMTAB));
       i++) 
  {
    if (PREG_To_TN_Array[i] == tn) return i;
  }
  return 0;
}

/* =======================================================================
 *
 *  PREG_To_TN_Clear 
 *
 *  Call this when starting a new REGION, so that 
 *  the TNs in the new REGION will be distinct from the
 *  TNs in all previously compiled REGIONs.
 *
 * =======================================================================
 */
void
PREG_To_TN_Clear (void)
{
  PREG_NUM i;

  for (i = Last_Dedicated_Preg_Offset + 1; 
       i <= Get_Preg_Num(PREG_Table_Size(CURRENT_SYMTAB));
       i++) 
  {
    PREG_To_TN_Array[i] = NULL;
    PREG_To_TN_Mtype[i] = (TYPE_ID)0;
  }
  max_preg_to_tn_index = 0;
}

/* =======================================================================
 *
 *  TN_LIST_From_PREG_LIST
 *
 *  Use PREG_To_TN_Array to convert a PREG_LIST to a TN_LIST.
 *
 * =======================================================================
 */
static TN_LIST *
TN_LIST_From_PREG_LIST( PREG_LIST *prl0, MEM_POOL *pool )
{
  PREG_LIST *prl;
  PREG_NUM pr;
  TN_LIST *tnl = NULL;
  TN *tn;

  for ( prl = prl0; prl; prl = PREG_LIST_rest( prl ) ) {
    pr = PREG_LIST_first( prl );
    if (pr > Last_Dedicated_Preg_Offset) {
      tn = PREG_To_TN_Array[pr];
      Is_True(tn != NULL,
	    ("TN_LIST_From_PREG_LIST, NULL TN in PREG to TN map, PREG%d",pr));
      tnl = TN_LIST_Push( tn, tnl, pool );
    }
  }

  return tnl;
}

/* add list of TOP_pregtn to bb, so tn renaming and splitting keeps
 * track of the preg associated with the tn */
static void
Add_PregTNs_To_BB (PREG_LIST *prl0, BB *bb, BOOL prepend)
{
  PREG_LIST *prl;
  PREG_NUM pr;
  TN *tn;
  TOP topcode = (prepend ? TOP_begin_pregtn : TOP_end_pregtn);
  OPS ops;
  OPS_Init(&ops);

  for ( prl = prl0; prl; prl = PREG_LIST_rest( prl ) ) {
    pr = PREG_LIST_first( prl );
    if (pr > Last_Dedicated_Preg_Offset) {
      tn = PREG_To_TN_Array[pr];
      if (tn == NULL) {
	// remove preg from in list, since no longer seen
  	if (Trace_REGION_Interface)
	  fprintf(TFile, "<region> remove preg %d from rid list\n", pr);
	REGION_remove_preg(BB_rid(bb), pr, !prepend /* outset */);
      } else {
      	Build_OP (topcode, tn, Gen_Literal_TN(pr, 4), &ops);
      	Set_OP_glue(OPS_last(&ops));
      }
    }
  }
  if ( Trace_REGION_Interface ) {
    fprintf( TFile, "<region> add pregtns to bb %d:\n", BB_id(bb)  );
    Print_OPS(&ops);
  }
  if (prepend)
    CGSPILL_Prepend_Ops (bb, &ops);
  else
    CGSPILL_Append_Ops (bb, &ops);
}

/* Check if the parent WHIRL node can take an immediate operand
 * of the value const_val. If parent is NULL, return FALSE.
 */
static BOOL
Has_Immediate_Operand (WN *parent, WN *expr)
{
  Is_True( WN_operator_is(expr, OPR_INTCONST),
    ("Has_Immediate_Operand: Not a constant") );

  /* If parent WHIRL node is not given, the constant 
   * cannot be an immediate operand. 
   */
  if (parent == NULL) return FALSE;

  // check for target-specific cases where immediate operand is okay.
  if (Target_Has_Immediate_Operand (parent, expr)) {
	return TRUE;
  }

  /* Check to make sure that the immediate operand is the second
   * operand of the parent
   */
  if (WN_kid_count(parent) < 2 || WN_kid1(parent) != expr) return FALSE;
  /* 
   * ISTORE has 2 kids, but 2nd kid is address not an offset.
   * TODO:  check for any other such cases?
   * This kind of info should probably be somehow kept as an opcode property.
   */
  if (OPCODE_is_store(WN_opcode(parent))) return FALSE;

  /* Can_Be_Immediate on TOP is not what we want here,
   * because some opcodes (e.g. MPY) do not take any immediate,
   * yet we want to pass immediate for later optimized expansion.
   * So instead we need a separate list of those opcodes for which
   * we are prepared to handle immediates.
   */
  if (MTYPE_is_float(WN_rtype(parent)))
	return FALSE;

  switch (WN_operator(parent)) {
  case OPR_ADD:
  case OPR_SUB:
  case OPR_MPY:
  case OPR_DIV:
  case OPR_REM:
  case OPR_DIVREM:
  case OPR_SHL:
  case OPR_LSHR:
  case OPR_ASHR:
  case OPR_RROTATE:
  case OPR_BAND:
  case OPR_BIOR:
  case OPR_BXOR:
  case OPR_EQ:
  case OPR_NE:
  case OPR_LE:
  case OPR_LT:
  case OPR_GE:
  case OPR_GT:
	return TRUE;
  }
  return FALSE;
}



/* Handle the misc. stuff that needs to be done for OPR_CALL, OPR_PICCALL,
 * and OPR_ICALL nodes. This includes:
 *
 *  1. generate code for the actual arguments
 *  2. mark the current PU and bb as having a call.
 *  3. mark the current EH range as having a call.
 */
static void
Handle_Call_Site (WN *call, OPERATOR call_opr)
{
  TN *tgt_tn;
  ST *call_st = (call_opr != OPR_ICALL) ? WN_st(call) : NULL;
  CALLINFO *call_info;

  /* wn_lower_call has already expanded the parameters, so don't need
   * to do anything more for parameters at this point.
   */

  /* Note the presence of a call in the current PU and bb. */
  PU_Has_Calls = TRUE;

#ifdef TARG_X8664
  if( Is_Target_32bit() &&
      Gen_PIC_Shared    &&
      call_st != NULL   &&
      !ST_is_export_local(call_st) ){
    PU_References_GOT = TRUE;
  }
#endif

  /* Generate the call instruction */
  if (call_opr == OPR_CALL) {
    tgt_tn = Gen_Symbol_TN (call_st, 0, 0);
  }
  else {
    /* For PIC calls, force t9 to be the result register. */
    tgt_tn = Gen_PIC_Calls ? Ep_TN : NULL;
    tgt_tn = Expand_Expr (WN_kid(call,WN_kid_count(call)-1), call, tgt_tn);

    /* If call-shared and the call is to a non PREEMPTIBLE symbol,
     * generate a jal instead of a jalr. 
     */
    if (Gen_PIC_Call_Shared && 
	call_st != NULL &&
	!ST_is_preemptible(call_st) )
    {
      tgt_tn = Gen_Symbol_TN (call_st, 0, 0);
      call_opr = OPR_CALL;
    }
  }
  Last_Mem_OP = OPS_last(&New_OPs);
  Exp_Call (call_opr, RA_TN, tgt_tn, &New_OPs);
  Set_OP_To_WN_Map(call);

  call_info = TYPE_PU_ALLOC (CALLINFO);
  CALLINFO_call_st(call_info) = call_st;
  CALLINFO_call_wn(call_info) = call;
  BB_Add_Annotation (Cur_BB, ANNOT_CALLINFO, call_info);

  region_stack_eh_set_has_call();

  /* For now, terminate the basic block. This makes GRA work better since 
   * it has finer granularity. It is also easier for LRA to make the
   * assumption that a procedure call breaks a basic block. 
   */
  Start_New_Basic_Block ();

  // if caller-save-gp and not defined in own dso, then restore gp.
  // if call_st == null, then indirect call, and assume external.
#ifdef TARG_IA64
  if (Is_Caller_Save_GP && !Constant_GP &&
      // begin - fix for OSP_211
	  //         put the ST_is_weak_symbol condition under the guard of 
	  //         call_st == NULL, or else it will segmentation fault.
      (call_st == NULL || ST_export(call_st) == EXPORT_PREEMPTIBLE || ST_is_weak_symbol(call_st)) )
	  // end   -
#else
    if (Is_Caller_Save_GP && !Constant_GP
        && (call_st == NULL || ST_export(call_st) == EXPORT_PREEMPTIBLE))
#endif
  {
	// restore old gp
	// assume that okay to restore gp before return val of call
	TN *caller_gp_tn = PREG_To_TN_Array[ Caller_GP_Preg ];
	if (caller_gp_tn == NULL) {
		caller_gp_tn = Gen_Register_TN ( ISA_REGISTER_CLASS_integer, 
			Pointer_Size);
		TN_MAP_Set( TN_To_PREG_Map, caller_gp_tn, 
			(void *)(INTPTR)Caller_GP_Preg );
		PREG_To_TN_Array[ Caller_GP_Preg ] = caller_gp_tn;
		PREG_To_TN_Mtype[ Caller_GP_Preg ] = TY_mtype(Spill_Int_Type);
	}
	Exp_COPY (GP_TN, caller_gp_tn, &New_OPs);
  }
}


/*
 * Determine the Exp_OP variant for a memory operation.
 */
static VARIANT Memop_Variant(WN *memop)
{
  VARIANT variant = V_NONE;
  INT     required_alignment = MTYPE_RegisterSize(WN_desc(memop));

#ifndef CG_PATHSCALE_MERGE
  /* If volatile, set the flag.
   */
  if (WN_Is_Volatile_Mem(memop)) Set_V_volatile(variant);
#endif

  /* Determine the alignment related variants. We have to check both 
   * ty alignment and the offset alignment.
   */
  Is_True ((required_alignment != 0), ("bad value 0 for required_alignment"));
 
  if (required_alignment > 1) {
    WN_OFFSET offset;
    INT ty_align;
    INT align;

    switch (WN_operator(memop)) {
    case OPR_LDID:
    case OPR_LDBITS:
    case OPR_STID:
    case OPR_STBITS:
      /* store_offset and load_offset are the same, so we share the code
       */
      offset = WN_load_offset(memop);
      ty_align = ST_alignment(WN_st(memop));
#ifdef CG_PATHSCALE_MERGE // bug 8198: in absence of pointed-to type's alignment, need this
      if (offset) {
	INT offset_align = offset % required_alignment;
	if (offset_align) ty_align = MIN(ty_align, offset_align);
      }
#endif
      break;
    case OPR_ILOAD:
    case OPR_ILDBITS:
      {
	TY_IDX ty = WN_load_addr_ty(memop);
	if (TY_kind(ty) == KIND_POINTER) ty = TY_pointed(ty);
	ty_align = TY_align(ty);
	offset = WN_load_offset(memop);
      }
      break;
    case OPR_ISTORE:
    case OPR_ISTBITS:
      {
	TY_IDX ty = WN_ty(memop);
	if (TY_kind(ty) == KIND_POINTER) ty = TY_pointed(ty);
	ty_align = TY_align(ty);
	offset = WN_store_offset(memop);
      }
      break;
    default:
      FmtAssert(FALSE, ("unhandled memop in Memop_Variant"));
      /*NOTREACHED*/
    }

    align = ty_align;
#ifdef TARG_IA64
    if (offset) {
	int ofs_mod = offset % required_alignment;
	if (ofs_mod) {
	    int ofs_align = ffs(ofs_mod) - 1;
	    align = MIN(ty_align, 1U << ofs_align);
	}
    }
#endif

    if (align < required_alignment) {
#ifdef TARG_IA64
      Set_V_alignment(variant, ffs(align) - 1);
#else
      Set_V_alignment(variant, align);
#endif
     /*
      *	TODO
      *	When we have ST information we may be able to compute an
      *	offset(say mod 16) that will give us further information
      */
      Set_V_align_offset_unknown(variant);

      /* We have an unaligned volatile. What to do??
       * Suneel/bean want the object to ignore the atomicity of volatile
       * and generate the unaligned references.
       * This will at least get the users code working
       */
      if (V_volatile(variant)) {
	ErrMsgLine(EC_Ill_Align,
		   SRCPOS_linenum(current_srcpos),
		   ty_align,
		   "reference to unaligned volatile:  volatile atomicity is ignored");
      }
    }
  }

  /* Now get prefetch flags if any
   */
  WN *pf_wn = NULL;
  PF_POINTER *pf_ptr = (PF_POINTER *) WN_MAP_Get(WN_MAP_PREFETCH, memop);
  if (pf_ptr) {
    pf_wn = PF_PTR_wn_pref_2L(pf_ptr);
    if (pf_wn == NULL) pf_wn = PF_PTR_wn_pref_1L(pf_ptr);
  }
  if (pf_wn) Set_V_pf_flags(variant, WN_prefetch_flag(pf_wn));

#ifdef CG_PATHSCALE_MERGE
  /* If volatile, set the flag.
   */
  if (variant)
    if (WN_Is_Volatile_Mem(memop)) Set_V_volatile(variant);
#endif
  return variant;
}


static TN *
Handle_LDA (WN *lda, WN *parent, TN *result, OPCODE opcode)
{
  OPERATOR call_op = OPERATOR_UNKNOWN;
  /* check if the LDA is for a procedure call. */
  if (parent != NULL) call_op = WN_operator(parent);

  if (result == NULL) {
    result = Allocate_Result_TN (lda, NULL);
    if (CGSPILL_Rematerialize_Constants) {
      Set_TN_is_rematerializable(result);
      Set_TN_home (result, lda);
    }
  }

  Last_Mem_OP = OPS_last(&New_OPs);
  Exp_Lda (
      OPCODE_rtype(opcode),
      result,
      WN_st(lda), 
      WN_lda_offset(lda),
      call_op,
      &New_OPs);
  Set_OP_To_WN_Map(lda);

  return result;
}


// return preg for corresponding ST via dreg table
struct find_dreg_preg {
	ST_IDX st;
	find_dreg_preg (const ST *s) : st (ST_st_idx (s)) {}

	BOOL operator () (UINT, const ST_ATTR *st_attr) const {
	    return (ST_ATTR_kind (*st_attr) == ST_ATTR_DEDICATED_REGISTER &&
		    ST_ATTR_st_idx (*st_attr) == st);
    	}
};
PREG_NUM
Find_PREG_For_Symbol (const ST *st)
{
    ST_IDX idx = ST_st_idx (st);
    ST_ATTR_IDX d;

    d = For_all_until (St_Attr_Table, ST_IDX_level (idx),
                          find_dreg_preg(st));
    return ST_ATTR_reg_id(St_Attr_Table(ST_IDX_level (idx), d));
} 


static TN *
Handle_LDID (WN *ldid, TN *result, OPCODE opcode)
{
  if (ST_assigned_to_dedicated_preg(WN_st(ldid))) {
	// replace st with dedicated preg
	WN_offset(ldid) = Find_PREG_For_Symbol(WN_st(ldid));
	WN_st_idx(ldid) = ST_st_idx(MTYPE_To_PREG(ST_mtype(WN_st(ldid))));
  }
  /* Check if we have an LDID of a PREG. Just return the TN corresponding
   * to the PREG. If there is a result TN, generate a copy.
   */
  if (WN_class(ldid) == CLASS_PREG)
  {
    TN *ldid_result = PREG_To_TN (WN_st(ldid), WN_load_offset(ldid));
    if (result == NULL) {
      result = ldid_result;
    }
    else {
#ifdef EMULATE_LONGLONG
      {
        extern void
          Expand_Copy (TN *result, TN *src, TYPE_ID mtype, OPS *ops);
        TYPE_ID mtype =  ST_mtype(WN_st(ldid));
        if (mtype == MTYPE_I8 || mtype == MTYPE_U8) {
          Expand_Copy (result, ldid_result, mtype, &New_OPs);
        } else {
          Exp_COPY (result, ldid_result, &New_OPs);
        }
      }
#else
#ifdef TARG_X8664
      if( OP_NEED_PAIR( ST_mtype(WN_st(ldid) ) ) ){
	Expand_Copy( result, ldid_result, ST_mtype(WN_st(ldid)), &New_OPs );
	
      } else
#endif // TARG_X8664
	Exp_COPY (result, ldid_result, &New_OPs);

#endif // EMULATE_LONGLONG
    }
  } 
  else
  {
    VARIANT variant;

    if (opcode == OPC_U4U8LDID)
    {
      opcode =	OPC_U4U4LDID;
      WN_set_opcode(ldid, opcode);
      if (Target_Byte_Sex == BIG_ENDIAN) {
      	WN_offset(ldid) += 4;	// get low-order word
      }
    }
    else if (opcode == OPC_I4I8LDID)
    {
      opcode = OPC_I4I4LDID;
      WN_set_opcode(ldid, opcode);
      if (Target_Byte_Sex == BIG_ENDIAN) {
      	WN_offset(ldid) += 4;	// get low-order word
      }
    }
    variant = Memop_Variant(ldid);

    if (result == NULL) result = Allocate_Result_TN (ldid, NULL);
    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_Load (OPCODE_rtype(opcode), OPCODE_desc(opcode),
	result, 
	WN_st(ldid), 
	WN_load_offset(ldid), 
	&New_OPs, 
	variant);
    Set_OP_To_WN_Map(ldid);
  }
  return result;
}

static TN *
Handle_LDBITS (WN *ldbits, TN *result, OPCODE opcode)
{
  TN *src_tn;

  if (result == NULL) result = Allocate_Result_TN (ldbits, NULL);

  if (WN_class(ldbits) == CLASS_PREG)
  { /* LDBITS of a PREG */
    src_tn = PREG_To_TN (WN_st(ldbits), WN_load_offset(ldbits));
  } 
  else
  {
    VARIANT variant = Memop_Variant(ldbits);

    src_tn = Allocate_Result_TN (ldbits, NULL);
    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_Load (OPCODE_rtype(opcode), OPCODE_desc(opcode),
	src_tn, 
	WN_st(ldbits), 
	WN_load_offset(ldbits), 
	&New_OPs, 
	variant);
    Set_OP_To_WN_Map(ldbits);
  }
  Exp_Extract_Bits(WN_rtype(ldbits), WN_desc(ldbits), WN_bit_offset(ldbits), 
		   WN_bit_size(ldbits), result, src_tn, &New_OPs);
  return result;
}

static TN *
Handle_EXTRACT_BITS (WN *extrbits, TN *result, OPCODE opcode)
{
  TN *kid0_tn = Expand_Expr (WN_kid0(extrbits), extrbits, NULL);
  if (result == NULL) result = Allocate_Result_TN (extrbits, NULL);

  Exp_Extract_Bits(OPCODE_rtype(opcode), OPCODE_rtype(opcode), 
		   WN_bit_offset(extrbits), WN_bit_size(extrbits), 
		   result, kid0_tn, &New_OPs);
  return result;
}

typedef struct TN_CORRESPOND
{
  TN			*result;
  TN			*result2;
  struct TN_CORRESPOND	*next;
} TN_CORRESPOND, *TN_CORRESPONDp;

static TN_CORRESPONDp tn_correspond_list= NULL;


TN *TN_CORRESPOND_Lookup(TN *result)
{
  TN_CORRESPONDp p;

  for(p= tn_correspond_list; p; p= p->next)
  {
    if (p->result == result)
    {
      return p->result2;
    }
  }
  return NULL;
}

void TN_CORRESPOND_Free(void)
{
  TN_CORRESPONDp next;
  TN_CORRESPONDp p = tn_correspond_list;

  while(p)
  {
    next =	p->next;
    MEM_POOL_FREE(Malloc_Mem_Pool, p);
    p =		next;
  }
  tn_correspond_list= NULL;
}

static TN *
TN_CORRESPOND_Set(TN *result, WN *expr)
{
  TN_CORRESPONDp p;

  Is_True((TN_CORRESPOND_Lookup(result)==NULL),("TN_CORRESPOND_Set(): expected null"));

  p = TYPE_MEM_POOL_ALLOC(TN_CORRESPOND, Malloc_Mem_Pool);
  p->result =		result;
  p->result2= 		Allocate_Result_TN(expr, NULL);
  p->next = 		tn_correspond_list;

  tn_correspond_list=	p;

  return p->result2;
}

TN *TN_CORRESPOND_Get(TN *result, WN *expr)
{
  TN *tn = TN_CORRESPOND_Lookup(result);

  if (tn)
    return tn;

  return TN_CORRESPOND_Set(result, expr);
}


static TN *
Handle_DIVREM(WN *expr, WN *parent, TN *result, OPCODE opcode)
{
  TN	*result2, *kid0_tn, *kid1_tn;

  Is_True ((parent && WN_class(parent) == CLASS_PREG), 
	   ("DIVREM: expected store of preg"));
  Is_True ((result == PREG_To_TN(WN_st(parent), WN_store_offset(parent))), 
	   ("DIVREM: bad result tn"));

  kid0_tn =	Expand_Expr (WN_kid0(expr), expr, NULL);
  kid1_tn =	Expand_Expr (WN_kid1(expr), expr, NULL);

  result2 = TN_CORRESPOND_Get(result, expr);
 
  Exp_OP(opcode, result, result2, kid0_tn, kid1_tn, V_NONE, &New_OPs);

  return result;
}

static TN *
Handle_DIVPART(WN *expr, WN *parent, TN *result)
{
  TN *pregTN;
  WN *kid = WN_kid0(expr);

  Is_True ((WN_class(kid) == CLASS_PREG), ("DIVPART: expected preg"));

  pregTN =  PREG_To_TN(WN_st(kid), WN_store_offset(kid));

  if (result==NULL)
  {
    return pregTN;
  }

#ifdef TARG_X8664
  Exp_COPY (result, pregTN, &New_OPs, TRUE);
#else
  Exp_COPY (result, pregTN, &New_OPs);
#endif

  return result;
}

static TN *
Handle_REMPART(WN *expr, WN *parent, TN *result)
{
  TN *pregTN;
  WN *kid = WN_kid0(expr);

  Is_True ((WN_class(kid) == CLASS_PREG), ("REMPART: expected preg"));

  pregTN =	PREG_To_TN(WN_st(kid), WN_store_offset(kid));
  pregTN =	TN_CORRESPOND_Get(pregTN, expr);

  Is_True ((pregTN),("expected tn correspondence"));

  if (result==NULL)
  {
    return pregTN;
  }

#ifdef TARG_X8664
  Exp_COPY (result, pregTN, &New_OPs, TRUE);
#else
  Exp_COPY (result, pregTN, &New_OPs);
#endif

  return result;
}

static TN *
Handle_MINMAX(WN *expr, WN *parent, TN *result, OPCODE opcode)
{
  TN	*result2, *kid0_tn, *kid1_tn;

  Is_True ((parent && WN_class(parent) == CLASS_PREG), 
	   ("MINMAX: expected store of preg"));
  Is_True ((result == PREG_To_TN(WN_st(parent), WN_store_offset(parent))), 
	   ("MINMAX: bad result tn"));

  kid0_tn =	Expand_Expr (WN_kid0(expr), expr, NULL);
  kid1_tn =	Expand_Expr (WN_kid1(expr), expr, NULL);

  result2 = TN_CORRESPOND_Get(result, expr);
 
  Exp_OP(opcode, result, result2, kid0_tn, kid1_tn, V_NONE, &New_OPs);

  return result;
}

static TN *
Handle_MINPART(WN *expr, WN *parent, TN *result)
{
  TN *pregTN;
  WN *kid = WN_kid0(expr);

  Is_True ((WN_class(kid) == CLASS_PREG), ("MINPART: expected preg"));

  pregTN =  PREG_To_TN(WN_st(kid), WN_store_offset(kid));

  if (result==NULL)
  {
    return pregTN;
  }

#ifdef TARG_X8664
      if( OP_NEED_PAIR( ST_mtype(WN_st(kid) ) ) ){
	Expand_Copy( result, pregTN, ST_mtype(WN_st(kid)), &New_OPs );
	
      } else
#endif // TARG_X8664
  Exp_COPY (result, pregTN, &New_OPs);
  return result;
}

static TN *
Handle_MAXPART(WN *expr, WN *parent, TN *result)
{
  TN *pregTN;
  WN *kid = WN_kid0(expr);

  Is_True ((WN_class(kid) == CLASS_PREG), ("MAXPART: expected preg"));

  pregTN =	PREG_To_TN(WN_st(kid), WN_store_offset(kid));
  pregTN =	TN_CORRESPOND_Get(pregTN, expr);

  Is_True ((pregTN),("expected tn correspondende"));

  if (result==NULL)
  {
    return pregTN;
  }

#ifdef TARG_X8664
      if( OP_NEED_PAIR( ST_mtype(WN_st(kid) ) ) ){
	Expand_Copy( result, pregTN, ST_mtype(WN_st(kid)), &New_OPs );
	
      } else
#endif // TARG_X8664
  Exp_COPY (result, pregTN, &New_OPs);
  return result;
}



static TN *
Handle_ILOAD (WN *iload, TN *result, OPCODE opcode)
{
  VARIANT variant;
  WN *kid0 = WN_kid0(iload);
  ST *st;

  if (opcode == OPC_U4U8ILOAD)
  {
    opcode = OPC_U4U4ILOAD;
    WN_set_opcode(iload, opcode);
    if (Target_Byte_Sex == BIG_ENDIAN) {
	WN_offset(iload) += 4;	// get low-order word
    }
  }
  else if (opcode == OPC_I4I8ILOAD)
  {
    opcode =	OPC_I4I4ILOAD;
    WN_set_opcode(iload, opcode);
    if (Target_Byte_Sex == BIG_ENDIAN) {
	WN_offset(iload) += 4;	// get low-order word
    }
  }
  variant = Memop_Variant(iload);
  if (result == NULL) result = Allocate_Result_TN (iload, NULL);

  /* If the kid of the ILOAD is an LDA, handle the ILOAD like an LDID */
  if (WN_operator_is(kid0, OPR_LDA)) {
    Last_Mem_OP = OPS_last(&New_OPs);
    st = WN_st(kid0);
    /* make sure st is allocated */
    Allocate_Object (st);

    Exp_Load (OPCODE_rtype(opcode), OPCODE_desc(opcode),
	result, 
	st, 
	WN_offset(iload) + WN_lda_offset(kid0),
	&New_OPs,
	variant);
  }
  else {
    TN *kid0_tn = Expand_Expr (kid0, iload, NULL); 
    TN *offset_tn = Gen_Literal_TN (WN_offset(iload), 4);
    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_OP2v (opcode, result, kid0_tn, offset_tn, variant, &New_OPs);
  }
  Set_OP_To_WN_Map(iload);
  return result;
}


static TN *
Handle_ILDBITS (WN *ildbits, TN *result, OPCODE opcode)
{
  VARIANT variant = Memop_Variant(ildbits);
  WN *kid0 = WN_kid0(ildbits);
  ST *st;
  TN *src_tn = Allocate_Result_TN (ildbits, NULL);
  if (result == NULL) result = Allocate_Result_TN (ildbits, NULL);

  /* If the kid of the ILDBITSs is an LDA, handle the ILDBITS like an LDBITS */
  if (WN_operator_is(kid0, OPR_LDA)) {
    Last_Mem_OP = OPS_last(&New_OPs);
    st = WN_st(kid0);
    /* make sure st is allocated */
    Allocate_Object (st);

    Exp_Load (OPCODE_rtype(opcode), OPCODE_desc(opcode),
	src_tn, 
	st, 
	WN_offset(ildbits) + WN_lda_offset(kid0),
	&New_OPs,
	variant);
  }
  else {
    TN *kid0_tn = Expand_Expr (kid0, ildbits, NULL); 
    TN *offset_tn = Gen_Literal_TN (WN_offset(ildbits), 4);
    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_OP2v (OPCODE_make_op(OPR_ILOAD, OPCODE_rtype(opcode), OPCODE_desc(opcode)), 
	      src_tn, kid0_tn, offset_tn, variant, &New_OPs);
  }
  Set_OP_To_WN_Map(ildbits);

  Exp_Extract_Bits(WN_rtype(ildbits), WN_desc(ildbits), WN_bit_offset(ildbits),
		   WN_bit_size(ildbits), result, src_tn, &New_OPs);
  return result;
}

static BOOL Operator_Is_Bool (OPERATOR opr) {

  if (opr == OPR_LNOT ||
      opr == OPR_EQ ||
      opr == OPR_NE ||
      opr == OPR_GE ||
      opr == OPR_GT ||
      opr == OPR_LE ||
      opr == OPR_LT ||
      opr == OPR_LAND ||
      opr == OPR_LIOR) {
    return TRUE;
  } else {
    return FALSE;
  }
}

static void
Handle_STID (WN *stid, OPCODE opcode)
{
  TN *result;

#ifdef EMULATE_LONGLONG
  {
    // long long check: if LHS is I/U8 and RHS is I4, then RHS must be
    // a boolean operator
    TYPE_ID rtype = OPCODE_rtype(opcode);
    TYPE_ID dtype = WN_rtype(WN_kid0(stid));
    if ((rtype == MTYPE_I8 || rtype == MTYPE_U8) && dtype == MTYPE_I4) {
      FmtAssert (Operator_Is_Bool (WN_operator(WN_kid0(stid))),
                 ("Handle_STID: LHS is I/U8, RHS is I4 and not BOOL"));
    }
  }
#endif

  if (ST_assigned_to_dedicated_preg(WN_st(stid))) {
	// replace st with dedicated preg
	WN_offset(stid) = Find_PREG_For_Symbol(WN_st(stid));
	WN_st_idx(stid) = ST_st_idx(MTYPE_To_PREG(ST_mtype(WN_st(stid))));
  }
  /* Check if we have an STID of a PREG. Get the TN corresponding to
   * the PREG and use it as the result TN for the kid0 of the STID.
   */
  if (WN_class(stid) == CLASS_PREG) {
      WN *kid = WN_kid0(stid);

      result = PREG_To_TN (WN_st(stid), WN_store_offset(stid));



#if defined(TARG_IA32)
      if (TN_is_dedicated(result)) {
        // is a return value; create another TN
        TN *tmp = Build_TN_Like(result);
        TN *tmp1 = NULL;
        TN *tmp2 = NULL;

        if (tmp1 = If_Get_TN_Pair(result)) {
          Add_TN_Pair(tmp, tmp2 = Build_TN_Like(tmp1));
        }

        Expand_Expr (kid, stid, tmp);
        Exp_COPY (result, tmp, &New_OPs);
        if (tmp1)
          Exp_COPY (tmp1, tmp2, &New_OPs);
      }
      else {
        Expand_Expr (kid, stid, result);
      }
#else 
#ifdef TARG_X8664 // bug 11088
        if(OPCODE_is_compare(WN_opcode(kid)) && 
                  MTYPE_is_vector(WN_desc(kid))){
        
        TN *op1 = Expand_Expr (WN_kid0(kid), kid, NULL);
        TN *op2 = Expand_Expr (WN_kid1(kid), kid, NULL);
        Exp_Stid_And_VComp(opcode, result, op1, op2, WN_opcode(kid), &New_OPs);
        return;
       }else
#endif
	 // TARG_IA64
      //lets do something here, of course we need to handle this
      Expand_Expr (kid, stid, result); /// I would think this is the problem

#ifdef TARG_X8664
      const TYPE_ID stid_type = OPCODE_desc(opcode);
      const TYPE_ID kid0_type = WN_rtype(WN_kid0(stid));

      if( OP_NEED_PAIR(stid_type)     &&
	  Get_TN_Pair(result) != NULL &&
	  MTYPE_byte_size(kid0_type) < MTYPE_byte_size(stid_type) ){

	if( MTYPE_is_signed(kid0_type) ){
	  DevWarn( "the higher 32-bit of PREG %d is set to 0",
		   WN_store_offset(stid) );
	}

	TN* result_hi = Get_TN_Pair( result );
	Expand_Immediate( result_hi, Gen_Literal_TN(0,4), FALSE, &New_OPs );
      }
#endif // TARG_X8664

#endif // IA_32
      if (In_Glue_Region) {
	if ( Trace_REGION_Interface ) {
	    fprintf(TFile,"set op_glue on preg store in bb %d\n", BB_id(Cur_BB));
	}
	Set_OP_glue(OPS_last(&New_OPs));
      }

     /* If the child is a PREG and has a corresponding TN, it was part of 
      * a DIVREM or MTYPE_B pair. We need to create a correspondence for the 
      * STID, and do an assignment
      */
      if (WN_operator_is(kid, OPR_LDID) && WN_class(kid) == CLASS_PREG) {
        TN *ldidTN = PREG_To_TN (WN_st(kid), WN_load_offset(kid));

	TN *ldidTN2 = TN_CORRESPOND_Lookup(ldidTN);
	if (ldidTN2 != NULL) {
	  TN *stidTN = TN_CORRESPOND_Get(result, kid);
	  Exp_COPY(stidTN, ldidTN2, &New_OPs);
	} else if (Is_Predicate_REGISTER_CLASS(TN_register_class(ldidTN))) {
	  Is_True(Is_Predicate_REGISTER_CLASS(TN_register_class(result)),
		  ("result should be predicate register class"));
	  PREG_NUM cpreg_num = WN_load_offset(kid) + 1;
	  TN *ctn = PREG_To_TN_Array[cpreg_num];
	  PREG_NUM cresult_num = WN_store_offset(stid) + 1;
	  TN *cresult = PREG_To_TN_Array[cresult_num];
	  Exp_COPY (cresult, ctn, &New_OPs);
	}
      }
  }
  else {
    VARIANT variant = Memop_Variant(stid);
    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_Store (
      OPCODE_desc(opcode), 
      Expand_Expr (WN_kid0(stid), stid, NULL),
      WN_st(stid), 
      WN_store_offset(stid),
      &New_OPs,
      variant);
    Set_OP_To_WN_Map(stid);

  }
}


static void
Handle_STBITS (WN *stbits)
{
  VARIANT variant;
  WN *kid = WN_kid0(stbits);
  TN *result;
  TN *field_tn;
  TN *bits_tn = Allocate_Result_TN (kid, NULL);
  const TYPE_ID desc = Mtype_TransferSign(MTYPE_U4, WN_desc(stbits));
  TYPE_ID rtype = Mtype_TransferSize(WN_rtype(kid), desc);
#ifdef CG_PATHSCALE_MERGE // bug 7418
  if (MTYPE_bit_size(rtype) < MTYPE_bit_size(desc))
    rtype = desc;
#endif

  Expand_Expr (kid, stbits, bits_tn);

  /* Check if we have an STBITS of a PREG. Get the TN corresponding to
   * the PREG */
  if (WN_class(stbits) == CLASS_PREG) {
    field_tn = PREG_To_TN (WN_st(stbits), WN_store_offset(stbits));
    result = field_tn;
  } else {
    variant = Memop_Variant(stbits);
    field_tn = Allocate_Result_TN (kid, NULL);
    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_Load(rtype, desc, field_tn, WN_st(stbits), WN_load_offset(stbits),
	     &New_OPs, variant); // must do an unsigned load
    Set_OP_To_WN_Map(stbits);
    result = Allocate_Result_TN (kid, NULL);
  }

  // deposit bits_tn into field_tn returning result in result
  Exp_Deposit_Bits(rtype, desc, WN_bit_offset(stbits),
		   WN_bit_size(stbits), result, field_tn, bits_tn, &New_OPs);

  if (WN_class(stbits) != CLASS_PREG) 
    {
    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_Store(desc, result, WN_st(stbits), WN_store_offset(stbits), &New_OPs,
	      variant);
    Set_OP_To_WN_Map(stbits);
  }
}


static TN *
Handle_COMPOSE_BITS (WN *compbits, TN *result, OPCODE opcode)
{
  TN *kid0_tn = Expand_Expr (WN_kid0(compbits), compbits, NULL);
  TN *kid1_tn = Expand_Expr (WN_kid1(compbits), compbits, NULL);
  if (result == NULL) result = Allocate_Result_TN (compbits, NULL);

  Exp_Deposit_Bits(OPCODE_rtype(opcode), OPCODE_rtype(opcode), 
		   WN_bit_offset(compbits), WN_bit_size(compbits), 
		   result, kid0_tn, kid1_tn, &New_OPs);

  return result;
}


static void
Handle_ISTORE (WN *istore, OPCODE opcode)
{
  VARIANT variant  = Memop_Variant(istore);
  WN *kid1 = WN_kid1(istore);
  TN *kid0_tn = Expand_Expr (WN_kid0(istore), istore, NULL);
  ST *st;

#ifdef EMULATE_LONGLONG
  {
    // long long check: If the LHS is an I/U8, and the RHS is I4,
    // then assert that RHS must be a boolean.
    TYPE_ID rtype = WN_rtype(WN_kid0(istore));
    TYPE_ID dtype = OPCODE_desc(opcode);
    if ((dtype == MTYPE_I8 || dtype == MTYPE_U8) && rtype == MTYPE_I4) {
      FmtAssert (Operator_Is_Bool (WN_operator(WN_kid0(istore))),
                 ("Handle_ISTORE: LHS is I/U8, RHS is I4 and not BOOL"));
    }
  }
#endif

  /* if the kid1 is an LDA, treat the ISTORE as an STID */
  if (WN_operator_is(kid1, OPR_LDA)) {
    Last_Mem_OP = OPS_last(&New_OPs);
    st = WN_st(kid1);
    /* make sure st is allocated */
    Allocate_Object (st);

    Exp_Store (
	OPCODE_desc(opcode),
	kid0_tn,
	st,
	WN_offset(istore) + WN_lda_offset(kid1),
	&New_OPs,
	variant);
  }
  else {
    TN *kid1_tn = Expand_Expr (WN_kid1(istore), istore, NULL);
    TN *offset_tn = Gen_Literal_TN (WN_offset(istore), 4);
    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_OP3v (
	opcode, 
	NULL, 
	kid0_tn,
	kid1_tn,
	offset_tn,
	variant,
	&New_OPs);
  }
  Set_OP_To_WN_Map(istore);
}


static void
Handle_ISTBITS (WN *istbits)
{
  VARIANT variant  = Memop_Variant(istbits);
  WN *kid0 = WN_kid0(istbits);
  WN *kid1 = WN_kid1(istbits);
  TN *bits_tn = Expand_Expr (kid0, istbits, NULL);
  TN *field_tn = Allocate_Result_TN (kid0, NULL);
  TN *result = Allocate_Result_TN (kid0, NULL);
  TYPE_ID desc = Mtype_TransferSign(MTYPE_U4, WN_desc(istbits));
  TYPE_ID rtype = desc;

  // guard against U1MPY or U2MPY
  if (MTYPE_byte_size(rtype) < 4)
    rtype = Mtype_TransferSize(MTYPE_U4, rtype);
  if (MTYPE_byte_size(WN_rtype(kid0)) > MTYPE_byte_size(rtype)) 
    rtype = Mtype_TransferSize(WN_rtype(kid0), rtype);

  /* if the kid1 is an LDA, treat the ISTBITS as an STBITS */
  if (WN_operator_is(kid1, OPR_LDA)) {
    Last_Mem_OP = OPS_last(&New_OPs);
    ST *st = WN_st(kid1);
    /* make sure st is allocated */
    Allocate_Object (st);

    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_Load(rtype, desc, field_tn, st, 
	     WN_store_offset(istbits) + WN_lda_offset(kid1), &New_OPs,
	     variant);
    Set_OP_To_WN_Map(istbits);

    // deposit bits_tn into field_tn returning result in result
    Exp_Deposit_Bits(rtype, desc, WN_bit_offset(istbits), 
		     WN_bit_size(istbits), result, field_tn, bits_tn, &New_OPs);

    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_Store(desc, result, st, WN_store_offset(istbits) + WN_lda_offset(kid1),
	      &New_OPs, variant);
    Set_OP_To_WN_Map(istbits);
  }
  else {
    TN *kid1_tn = Expand_Expr (WN_kid1(istbits), istbits, NULL);
    TN *offset_tn = Gen_Literal_TN (WN_store_offset(istbits), 4);

    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_OP2v(OPCODE_make_op(OPR_ILOAD, rtype, desc),
	     field_tn, kid1_tn, offset_tn, variant, &New_OPs);
    Set_OP_To_WN_Map(istbits);

    // deposit bits_tn into field_tn returning result in result
    Exp_Deposit_Bits(rtype, desc, WN_bit_offset(istbits),
		     WN_bit_size(istbits), result, field_tn, bits_tn, &New_OPs);

    Last_Mem_OP = OPS_last(&New_OPs);
    Exp_OP3v(OPCODE_make_op(OPR_ISTORE, MTYPE_V, desc), 
	     NULL, result, kid1_tn, offset_tn, variant, &New_OPs);
    Set_OP_To_WN_Map(istbits);
  }
}

static TN *
Handle_SELECT(WN *select, TN *result, OPCODE opcode)
{
  TN	*trueop, *falseop, *cond;
  TN	*op1, *op2;
  WN	*compare;
  VARIANT variant;

 /*
  *  Expand the true/false before the condition
  */
  trueop = Expand_Expr (WN_kid1(select), select, NULL);
  falseop = Expand_Expr (WN_kid2(select), select, NULL);
  compare = WN_kid0(select);

  if (result == NULL) result = Allocate_Result_TN (select, NULL);

  variant = WHIRL_Compare_To_OP_variant (WN_opcode(compare), FALSE);
#ifndef CG_PATHSCALE_MERGE
  if (Check_Select_Expansion (WN_opcode(compare)) || (variant == V_BR_NONE)) {
#else
  if (Check_Select_Expansion (WN_opcode(compare)) || 
      (variant == V_BR_NONE && !MTYPE_is_vector(WN_desc(compare)))) {
#endif
	Is_True(   WN_desc(select) != MTYPE_B
		|| (   WN_operator_is(compare, OPR_LDID) 
		    && WN_class(compare) == CLASS_PREG),
		("MTYPE_B SELECT condition must be preg or relop"));
    	cond = Expand_Expr (compare, select, NULL);
	Exp_OP3 (opcode, result, cond, trueop, falseop, &New_OPs);
  }
#ifdef TARG_X8664 //bug 11088
  else if (WN_operator_is(compare, OPR_LDID) && 
                          WN_class(compare) == CLASS_PREG){
        Is_True(variant == V_BR_NONE && MTYPE_is_vector(WN_desc(compare)),
                ("Hanle_Select: ldid as condition must be a vector here!"));
        
        cond = Expand_Expr (compare, select, NULL);
        Exp_Select_And_VLdid(opcode, result, trueop, falseop, WN_opcode(compare),
                             cond,&New_OPs);
  } 
#endif
  else {
    	op1 = Expand_Expr (WN_kid0(compare), compare, NULL);
    	op2 = Expand_Expr (WN_kid1(compare), compare, NULL);
  	Exp_Select_And_Condition (opcode, result, trueop, falseop, 
				  WN_opcode(compare), op1, op2, variant, 
				  &New_OPs);
  }

  return result;
}

#ifdef TARG_X8664
/*
**	Handle the SHUFFLE operator.
**
*/
static TN * 
Handle_SHUFFLE (WN *shuffle, TN *result)
{
  TN	*op1;
  VARIANT variant;

  op1 = Expand_Expr (WN_kid0(shuffle), shuffle, NULL);

  if (result == NULL) result = Allocate_Result_TN (shuffle, NULL);

  variant = WN_offset(shuffle);
  Expand_Shuffle(WN_opcode(shuffle), result, op1, variant, &New_OPs);

  return result;
}

// Handle RROTATE, if it is actually a left-rotate, then generate the
// corresponding x86 instruction.
static TN *
Handle_RROTATE (WN *rotate, TN *result)
{
  if (result == NULL) result = Allocate_Result_TN (rotate, NULL);

  WN * obj = WN_kid0 (rotate);
  WN * rotate_amount = WN_kid1 (rotate);

  // Detect a rotate-left
  if (WN_operator (rotate_amount) == OPR_SUB)
  {
    WN * kid0 = WN_kid0 (rotate_amount);
    WN * kid1 = WN_kid1 (rotate_amount);

    // I >> (sizeof_inbits(I) - N) <==> I << N
    if (WN_operator (kid0) == OPR_INTCONST &&
        WN_const_val (kid0) == MTYPE_bit_size (WN_rtype (obj)))
    {
      TN * op1 = Expand_Expr (obj, rotate, NULL);
      TN * op2 = Expand_Expr (kid1, rotate_amount, NULL);
      Expand_Left_Rotate (result, op1, op2, WN_rtype (rotate),
                          WN_desc (rotate), &New_OPs);

      return result;
    }
  }
  // rotate-right
  TN * op1 = Expand_Expr (WN_kid0(rotate), rotate, NULL);
  TN * op2 = Expand_Expr (WN_kid1(rotate), rotate, NULL);

  Exp_OP2 (WN_opcode (rotate), result, op1, op2, &New_OPs);

  return result;
}
#endif

/*
**	Try to change logical operations into binary operations
**
**	If the operands are boolean, then we can use the binary version
*/
static TN*
Handle_LAND_LIOR(WN *expr, WN *parent, TN *result)
{
  VARIANT variant;
  OPCODE opcode = WN_opcode(expr);
  TN *op1 = Expand_Expr(WN_kid0(expr), expr, NULL);
  TN *op2 = Expand_Expr(WN_kid1(expr), expr, NULL);

  variant = V_NONE;
  if (OPCODE_is_boolean(WN_opcode(WN_kid0(expr)))) Set_V_normalized_op1(variant);
  if (OPCODE_is_boolean(WN_opcode(WN_kid1(expr)))) Set_V_normalized_op2(variant);

  if (result == NULL) result = Allocate_Result_TN (expr, NULL);

  Exp_OP2v(opcode, result, op1, op2, variant, &New_OPs);

  return result;
}

static TN*
Handle_LNOT(WN *expr, WN *parent, TN *result)
{
  VARIANT variant;
  OPCODE opcode = WN_opcode(expr);
  WN *kid0 = WN_kid0(expr);
  TN *op1 = Expand_Expr(kid0, expr, NULL);

  variant = V_NONE;
  if (OPCODE_is_boolean(WN_opcode(kid0))) Set_V_normalized_op1(variant);

  if (result == NULL) result = Allocate_Result_TN (expr, NULL);

  if (   WN_rtype(kid0) == MTYPE_B
      && WN_operator_is(kid0, OPR_LDID)
      && WN_class(kid0) == CLASS_PREG)
  {
    PREG_NUM cpreg_num = WN_load_offset(kid0) + 1;
    TN *ctn = PREG_To_TN_Array[cpreg_num];
    PREG_NUM result_num = TN_To_PREG(result);
    if (result_num) {
      PREG_NUM cresult_num = result_num + 1;
      TN *cresult = PREG_To_TN_Array[cresult_num];
      Exp_COPY (cresult, op1, &New_OPs);
    }
    Exp_COPY (result, ctn, &New_OPs);
  } else {
    Exp_OP1v(opcode, result, op1, variant, &New_OPs);
  }

  return result;
}

static BOOL U4ExprHasUpperBitZero(WN *wn)
{
  switch(WN_opcode(wn))
  {
  case OPC_U4LSHR:
   /*
    *  if we shift by a non zero amount, the expression sign bit will be zero
    */
    if (WN_operator_is(WN_kid1(wn), OPR_INTCONST))
    {
      if (0 < WN_const_val(WN_kid1(wn)))
	return TRUE;
    }
    break;

  case OPC_U4BAND:
   /*
    *  if the constant sign bit is zero, the expression sign bit will be zero
    */
    if (WN_operator_is(WN_kid1(wn), OPR_INTCONST))
    {
      if ((WN_const_val(WN_kid1(wn)) & 0x80000000) == 0)
	return TRUE;
    }
    break;
  }
  return FALSE;
}

static BOOL
Is_CVT_Noop(WN *cvt, WN *parent)
{
  if (WN_opcode(cvt) == OPC_I4U4CVT || WN_opcode(cvt) == OPC_U4I4CVT ||
      WN_opcode(cvt) == OPC_I8U8CVT || WN_opcode(cvt) == OPC_U8I8CVT) {
	// normally this is removed before cg, but sometimes not
	return TRUE;
  }

#ifdef TARG_X8664
  {
    // Bug 3082: Ignore cvt between mmx integer types. This may not be
    // correct, we may need to explicitly handle some of these CVTs.
    TYPE_ID desc = WN_desc (cvt);
    TYPE_ID rtype = WN_rtype (cvt);
    if (MTYPE_is_mmx_vector (desc) && MTYPE_is_integral (desc) &&
        MTYPE_is_mmx_vector (rtype) && MTYPE_is_integral (rtype))
      return TRUE;
  }
#endif

  if (Enable_CVT_Opt)
  {
    switch(WN_opcode(cvt))
    {
#ifdef TARG_IA64
    case OPC_F8F4CVT:
    case OPC_F4F8CVT:
	if (WN_operator(parent) == OPR_TAS) {
	  /* for IA-64, the tas (getf) does the size conversion too,
	   * so don't need the cvt. */
	  /* return TRUE; */
	  /* This trick is not always safe. e.g When the kid is
	   * F8F4CVT, and the F4 value holds a denormal number. A
	   * simple counter example to this trick is:
	   *    prrintf ("%g", (double)f);
	   *  where f is of type float, and it holds demormal value
	   *  say, 2**-127.  --- OSP-191
	   */
	  return FALSE;
	}
	break;
#endif
    case OPC_U8I4CVT:
    case OPC_I8I4CVT:
     /*
      *  if 32-bit ints are sign-extended to 64-bit, then is a nop.
      */
      if ( ! Split_64_Bit_Int_Ops && ! Only_Unsigned_64_Bit_Ops)
      {
#ifdef TARG_X8664
	return FALSE;
#else
	return TRUE;
#endif
      }
      break;

    case OPC_U8U4CVT:
    case OPC_I8U4CVT:
#ifndef EMULATE_LONGLONG
      /*
      *  if we can determine the upper bit:31 is zero, the cast is a nop
      */
      if (U4ExprHasUpperBitZero(WN_kid0(cvt))
#ifdef TARG_X8664
          && !Is_Target_32bit() // bug 6134
#endif // TARG_X8664
         )
      {
	return TRUE;
      }
#endif
      break;

    case OPC_I4U8CVT:
    case OPC_I4I8CVT:
    case OPC_U4U8CVT:
    case OPC_U4I8CVT:
     /*
      *  For truncation converts, the memory operation will
      *  perform the necessary truncation.
      */
      if (parent)
      {
	switch(WN_opcode(parent))
	{
	case OPC_I4STID:
	case OPC_U4STID:
	  if (WN_class(parent) != CLASS_PREG)
	  {
	    return TRUE;
	  }
	  break;

	case OPC_I4ISTORE:
	case OPC_U4ISTORE:
	  return TRUE;
	}
      }
      break;
    case OPC_I4CVTL:
    case OPC_U4CVTL:
    case OPC_I8CVTL:
    case OPC_U8CVTL:
     /*
      *  For truncation converts, the memory operation will
      *  perform the necessary truncation.
      */
      if (parent)
      {
	switch(WN_operator(parent))
	{
	case OPR_STID:
	  if (MTYPE_size_reg(WN_desc(parent)) ==  WN_cvtl_bits(cvt)
		&& WN_class(parent) != CLASS_PREG)
	  {
	    return TRUE;
	  }
	  break;

	case OPR_ISTORE:
	  if (MTYPE_size_reg(WN_desc(parent)) ==  WN_cvtl_bits(cvt))
	  {
	    return TRUE;
	  }
	}
      }
      break;
    }
  }
  return FALSE;
}

/* CVTL opcodes are either CVTLs or CVT that change the integer size */
static BOOL
Is_CVTL_Opcode (OPCODE opc)
{
	switch (opc) {
	case OPC_U8I4CVT:
	case OPC_U8U4CVT:
	case OPC_I8I4CVT:
	case OPC_I8U4CVT:
	case OPC_I4I8CVT:
	case OPC_I4U8CVT:
	case OPC_U4U8CVT:
	case OPC_U4I8CVT:
	case OPC_I8CVTL:
	case OPC_I4CVTL:
	case OPC_U8CVTL:
	case OPC_U4CVTL:
		return TRUE;
	default:
		return FALSE;
	}
}

static TN* 
Handle_ALLOCA (WN *tree, TN *result)
{
#ifdef TARG_IA64
  TN *tsize = Expand_Expr (WN_kid0(tree), tree, NULL);
  // align the size
  if (TN_has_value(tsize)) {
    INT64 size;
    size = TN_value(tsize);
    size += Stack_Alignment() - 1;
    size &= -Stack_Alignment();
    tsize = Gen_Literal_TN (size, Pointer_Size);
  }
  else if ( ! TN_is_zero_reg (tsize)) {
#else
  BOOL is_const = WN_operator(WN_kid0(tree)) == OPR_INTCONST;
  BOOL is_zero = is_const && WN_const_val(WN_kid0(tree)) == 0;
  TN *tsize = NULL;
  if (!is_zero) {
    if (is_const) {
      // align the size
      INT64 size = WN_const_val(WN_kid0(tree));
      size += Stack_Alignment() - 1;
      size &= -Stack_Alignment();
      tsize = Gen_Literal_TN (size, Pointer_Size);
    }
    else {
        tsize  = Expand_Expr (WN_kid0(tree), tree, NULL);
#endif
	TN *tmp1 = Build_TN_Like ( tsize );
	TN *tmp2 = Build_TN_Like ( tsize );
	Exp_ADD (Pointer_Mtype, tmp1, tsize, 
		Gen_Literal_TN (Stack_Alignment()-1, Pointer_Size), 
		&New_OPs);
  	Exp_OP2 ((MTYPE_is_size_double(WN_rtype(tree)) ? OPC_I8BAND : OPC_I4BAND), 
		tmp2, tmp1, 
		Gen_Literal_TN (-Stack_Alignment(), Pointer_Size), 
		&New_OPs);
	tsize = tmp2;
#ifndef TARG_IA64
    }
#endif
  }

  INT64 offset;
  INT stack_adjustment = Stack_Offset_Adjustment_For_PU();
#ifdef TARG_IA64
  if ( ! TN_is_zero(tsize)) {
#else
  if (!is_zero) {
#endif
	Exp_Spadjust (SP_TN, tsize, V_SPADJUST_MINUS, &New_OPs);

#ifdef TARG_X8664
	/* Make the stack pointer be 16-byte aligned after alloca even under -m32.
	   ( Is it a requirement ??? bug#2599)
	 */
	if( Is_Target_32bit() ){
	  Exp_BAND( Pointer_Mtype,
		    SP_TN, SP_TN, Gen_Literal_TN( -15, Pointer_Size ), &New_OPs);
	}
#endif
  	// return sp + arg area
	offset = Current_PU_Actual_Size + stack_adjustment;
  }
  else {
	// return original $sp + stack_adjustment
	offset = stack_adjustment;
  }

  if (offset == 0) {
	if (result == NULL)
		result = SP_TN;
	else
		Exp_COPY(result, SP_TN, &New_OPs);
  }
  else {
  	if (result == NULL) 
		result = Build_TN_Of_Mtype (Pointer_Mtype);
	Exp_ADD (Pointer_Mtype, result, SP_TN,
		Gen_Literal_TN (offset, Pointer_Size), 
		&New_OPs);
  }

  return result;
}

static void
Handle_DEALLOCA (WN *tree)
{
  INT stack_adjustment = Stack_Offset_Adjustment_For_PU();
  TN *val = Expand_Expr (WN_kid0(tree), tree, NULL);
  Last_Mem_OP = OPS_last(&New_OPs);
  if (stack_adjustment != 0) {
	Exp_ADD (Pointer_Mtype, val, val, 
		Gen_Literal_TN (-stack_adjustment, Pointer_Size),
		&New_OPs);
  }
  Exp_Spadjust (SP_TN, val, V_SPADJUST_PLUS, &New_OPs);
  Set_OP_To_WN_Map (tree);
}

#ifdef TARG_X8664
// Returns TN for any immediate operand of intrinsic 'expr'. Returns in
// 'kidno' the kidno for the immediate operand.
static TN*
Handle_Imm_Op (WN * expr, INT * kidno /* counted from 0 */)
{
  Is_True (WN_operator (expr) == OPR_INTRINSIC_OP,
           ("Handle_Imm_Op: Expected INTRINSIC_OP"));

  INTRINSIC id = (INTRINSIC) WN_intrinsic (expr);

  switch (id)
  {
    case INTRN_SHUFPS:
    case INTRN_SHUFPD:
#ifdef Is_True_On
      {
        char * intrn_name = INTRN_c_name (id);
        Is_True (WN_kid_count (expr) == 3,
                 ("Handle_Imm_Op: Invalid # of kids of %s intrn", intrn_name));
        Is_True (WN_operator (WN_kid0 (WN_kid2 (expr))) == OPR_INTCONST,
           ("Handle_Imm_Op: Arg 3 of %s intrn must be immediate constant",
            intrn_name));
      }
#endif
      *kidno = 2;
      return Gen_Literal_TN (WN_const_val (WN_kid0 (WN_kid2 (expr))), 4);

    case INTRN_PSLLDQ:
    case INTRN_PSRLDQ:
    case INTRN_PSHUFD:
    case INTRN_PSHUFW:
#ifdef Is_True_On
      {
        char * intrn_name = INTRN_c_name (id);
        Is_True (WN_kid_count (expr) == 2,
                 ("Handle_Imm_Op: Invalid # of kids of %s intrn", intrn_name));
        Is_True (WN_operator (WN_kid0 (WN_kid1 (expr))) == OPR_INTCONST,
           ("Handle_Imm_Op: Arg 2 of %s intrn must be immediate constant",
            intrn_name));
      }
#endif
      *kidno = 1;
      return Gen_Literal_TN (WN_const_val (WN_kid0 (WN_kid1 (expr))), 4);

    default:
      return NULL;
  }
}
#endif

static TN*
Handle_INTRINSIC_OP (WN *expr, TN *result)
{
  INTRINSIC id = (INTRINSIC) WN_intrinsic (expr);
  INTRN_RETKIND rkind = INTRN_return_kind(id);
  INT numkids = WN_kid_count(expr);
  TN *kid0 = Expand_Expr(WN_kid0(expr), expr, NULL);
#ifdef TARG_X8664
  INT imm_kidno = 0;
  // Get any immediate operand in intrinsic.
  TN * imm_kid = Handle_Imm_Op (expr, &imm_kidno);

  TN * kid1 = NULL, * kid2 = NULL;

  if (imm_kid)
  {
    Is_True (imm_kidno == 1 || imm_kidno == 2,
             ("Immediate kid0 of intrinsic not supported"));
    if (imm_kidno == 1)
    {
      kid1 = imm_kid;
      if (numkids == 3)
        kid2 = Expand_Expr(WN_kid2(expr), expr, NULL);
    }
    else
    { // kid2 is immediate operand
      Is_True (numkids == 3, ("Invalid # of kids for intrinsic"));
      kid1 = Expand_Expr(WN_kid1(expr), expr, NULL);
      kid2 = imm_kid;
    }
  }
  else
  {
    kid1 = (numkids >= 2) ? Expand_Expr(WN_kid1(expr), expr, NULL) : NULL;

    if (numkids == 3) {
      kid2 = Expand_Expr(WN_kid2(expr), expr, NULL);
    }
  }
  
  FmtAssert(numkids <= 3, ("unexpected number of kids in intrinsic_op"));
#else
  TN *kid1 = (numkids == 2) ? Expand_Expr(WN_kid1(expr), expr, NULL) : NULL;
  FmtAssert(numkids <= 2, ("unexpected number of kids in intrinsic_op"));
#endif

  if (rkind != IRETURN_UNKNOWN && result == NULL) {
    result = Allocate_Result_TN(expr, NULL);
  }

#ifdef CG_PATHSCALE_MERGE
  const TYPE_ID mtype = WN_rtype( WN_kid0(expr) );
#ifdef TARG_X8664
  Exp_Intrinsic_Op (id, result, kid0, kid1, kid2, mtype, &New_OPs);
#else
  Exp_Intrinsic_Op (id, result, kid0, kid1, mtype, &New_OPs);
#endif
#else
  Exp_Intrinsic_Op (id, result, kid0, kid1, &New_OPs);
#endif // KEY

  return result;
}

// Return name for nonlocal label, given level and label number.
// Must be fixed name across PU's; the goto won't find the real
// label_idx in the parent pu, it will just have the level and index.
static STR_IDX
Get_Non_Local_Label_Name (SYMTAB_IDX level, LABEL_IDX index)
{
	// create special label name .Lnonlocal.<level>.<index>
  	char *name = (char *) alloca (11 + 1 + 8 + 1 + 8 + 1);
	sprintf(name, ".Lnonlocal%s%d%s%d", Label_Name_Separator, 
		level, Label_Name_Separator, index);
	return Save_Str(name);
}

// Get LABEL from whirl node.
// It is much simpler since now we don't need to create STs for
// label numbers.  The only thing we might need to do is create a
// LABEL_name.
LABEL_IDX
Get_WN_Label (WN *wn)
{
  LABEL_IDX label = WN_label_number(wn);
  char *name;
  char *label_prefix = NULL;

  FmtAssert (OPCODE_has_label(WN_opcode(wn)),
	("Get_WN_Label: invalid opcode %d ", WN_opcode(wn)));
  FmtAssert (label > 0 && label <= LABEL_Table_Size(CURRENT_SYMTAB),
	("Get_WN_Label: label %d greater than last label %d", 
		label, LABEL_Table_Size(CURRENT_SYMTAB)));
  if (LABEL_name_idx(label) == 0) {
	if (LABEL_target_of_goto_outer_block(label)) {
		Set_LABEL_name_idx (Label_Table[label], 
			Get_Non_Local_Label_Name (CURRENT_SYMTAB, label) );
	}
	else {
		label_prefix = ".Lt";
	}
  }
  else if (isdigit(LABEL_name(label)[0])) {
	// prefix with .L so .s file will be legal,
	label_prefix = ".L";
  }
  if (label_prefix != NULL) {
	// create label name:  prefix<name>.<pu-number>.<label-index>
	// pu and index number assure is unique across file
	const char *oldname = (LABEL_name_idx(label) == 0 ? "" : LABEL_name(label));
  	name = (char *) alloca (strlen(label_prefix) + strlen(oldname) 
		+ 1 + 8 + 1 + 8 + 1);
	sprintf(name, "%s%s%s%d%s%d", label_prefix, oldname,
		Label_Name_Separator, Current_PU_Count(), 
		Label_Name_Separator, label);
	Set_LABEL_name_idx (Label_Table[label], Save_Str(name));
  }
  return label;
}


/* Expand a WHIRL expression into a sequence of OPs. Return the result
 * TN. The caller can specify a 'result' TN if already known.
 *
 * The 'parent' WHIRL node for the expression is passed for cases where
 * the context is needed for expansion. Currently, this is used for
 * the following:
 *    - to determine if an INTCONST can be used as an immediate field 
 *      in the parent OP. 
 *    - to determine if an LDA is used directly in a CALL node.
 *    - to determine if a truncation CVT can be eliminated
 *
 * The 'parent' node maybe set to NULL to disable the above checks. It
 * is also NULL for cases where there is no parent (i.e. statement
 * level nodes).
 */
static TN *
Expand_Expr (WN *expr, WN *parent, TN *result)
{
  OPCODE opcode;
  OPERATOR opr;
  INT num_opnds;
  INT i;
  TN *opnd_tn[OP_MAX_FIXED_OPNDS]; // we shouldn't be handling variable OPs
  TN *const_tn;
  TOP top;
  INT const_operands = 0;

  opcode = WN_opcode (expr);
  opr = WN_operator(expr);

  PU_WN_Cnt++;
#if Is_True_On
  if (WN_rtype(expr) == MTYPE_C4 ||
      WN_rtype(expr) == MTYPE_C8 ||
#ifdef TARG_IA64
      WN_rtype(expr) == MTYPE_C10 ||
#endif
      WN_rtype(expr) == MTYPE_CQ) 
  {
    ErrMsg (EC_Unimplemented, "Expand_Expr: COMPLEX");
  }
  if (opcode == OPC_MLOAD || opcode == OPC_MSTORE) {
    ErrMsg (EC_Unimplemented, "Expand_Expr: MLOAD/MSTORE");
  }
#endif

  top = WHIRL_To_TOP (expr);
  if (TOP_is_noop(top)
#ifdef TARG_IA64
	&& (opr == OPR_PAREN || opr == OPR_TAS || opr == OPR_PARM)) 
#else
    && (opr == OPR_PAREN ||
#ifndef TARG_X8664
            opr == OPR_TAS ||
#else
            opr == OPR_TAS && top == TOP_nop ||
#endif
	opr == OPR_PARM))
#endif
  {
    /* For TAS nodes, if the new opcode is noop, we can ignore the TAS.
     * For PAREN and PARM nodes, ignore it for now.
     */
      return Expand_Expr (WN_kid0(expr), parent, result);
  }
  /* get #opnds from topcode or from #kids of whirl
   * (special cases like store handled directly). */
  if (top != TOP_UNDEFINED) {
    num_opnds =   ISA_OPERAND_INFO_Operands(ISA_OPERAND_Info(top))
		- (TOP_is_predicated(top) != 0);
  } else {
    num_opnds = OPCODE_nkids(opcode);
  }
  FmtAssert(num_opnds <= OP_MAX_FIXED_OPNDS, ("too many operands (%d)", num_opnds));

  if (OPCODE_has_sym(opcode) && WN_st(expr) != NULL) {
    /* make sure st is allocated */
    Allocate_Object (WN_st(expr));
  }

#ifdef TARG_IA64
  // Added for handling boolean expressions for OPR_SELECT. Previously BAND/BIOR is used
  // instead of LAND/LIOR and boolean data is interpreted as 0/1(false/true).
if (Is_Old_Boolean_Expression(expr))
	{
		// bug fix for OSP_148
		// Not all cases need the context for expanding this expr
		// which means the parent may be NULL in some cases
		if (parent && WN_operator(parent)==OPR_SELECT) {
		  if (Trace_WhirlToOp) {
		    fprintf(TFile, "Replace select operator\n");
		    }
			return Handle_Bool_As_Predicate(expr, parent, 0);
		  }
		else
			FmtAssert(true, ("!!!Operator may handle boolean result improperly, pls check."));
	}
#endif
  /* Setup the operands */
  switch (opr) {

  case OPR_LDID:
    return Handle_LDID (expr, result, opcode);

  case OPR_LDBITS:
    return Handle_LDBITS (expr, result, opcode);

  case OPR_STID:
    Handle_STID (expr, opcode);
    return NULL;

  case OPR_STBITS:
    Handle_STBITS (expr);
    return NULL;

  case OPR_LDA:
    return Handle_LDA (expr, parent, result, opcode);

  case OPR_LDA_LABEL:
    // create a st that matches the label name
    // that we can then use with a relocation in the lda.
    {
        ST *st = New_ST (CURRENT_SYMTAB);
        ST_Init (st, Save_Str (LABEL_name(Get_WN_Label(expr))),
                 CLASS_NAME, SCLASS_UNKNOWN, EXPORT_LOCAL, WN_ty(expr));
    	opnd_tn[0] = Gen_Symbol_TN (st, 0, 0);
    }
    num_opnds = 1;
    break;

  case OPR_GOTO_OUTER_BLOCK:
    // create a st that matches the label name
    // that we can then use with a relocation in the lda.
    {
        ST *st = New_ST (CURRENT_SYMTAB);
        ST_Init (st, Get_Non_Local_Label_Name (
			WN_label_level(expr), WN_label_number(expr) ),
		CLASS_NAME, SCLASS_UNKNOWN, EXPORT_LOCAL, WN_ty(expr));
    	opnd_tn[0] = Gen_Symbol_TN (st, 0, 0);
    }
    num_opnds = 1;
    break;

  case OPR_ILOAD:
    return Handle_ILOAD (expr, result, opcode);

  case OPR_ILDBITS:
    return Handle_ILDBITS (expr, result, opcode);

  case OPR_EXTRACT_BITS:
    return Handle_EXTRACT_BITS (expr, result, opcode);

  case OPR_ISTORE:
    Handle_ISTORE (expr, opcode);
    return NULL;

  case OPR_ISTBITS:
    Handle_ISTBITS (expr);
    return NULL;

  case OPR_COMPOSE_BITS:
    return Handle_COMPOSE_BITS (expr, result, opcode);

  case OPR_SELECT:
    return Handle_SELECT(expr, result, opcode);

#ifdef TARG_X8664
  case OPR_SHUFFLE:
    return Handle_SHUFFLE(expr, result);

  case OPR_RROTATE:
    return Handle_RROTATE(expr, result);
#endif

  case OPR_CALL:
  case OPR_ICALL:
  case OPR_PICCALL:
    Handle_Call_Site (expr, opr);
    return NULL;

  case OPR_CONST:
    if (result == NULL) {
      result = Allocate_Result_TN (expr, NULL);
      if (CGSPILL_Rematerialize_Constants) {
	Set_TN_is_rematerializable(result);
	Set_TN_home (result, expr);
      }
    }
#ifdef TARG_X8664
    if (WN_rtype(expr) == MTYPE_V16F4 ||
	WN_rtype(expr) == MTYPE_V16F8 ||
	WN_rtype(expr) == MTYPE_V16C4 ||
	WN_rtype(expr) == MTYPE_V16I1 ||
	WN_rtype(expr) == MTYPE_V16I2 ||
	WN_rtype(expr) == MTYPE_V16I4 ||
	WN_rtype(expr) == MTYPE_V16I8) {
      TCON then = ST_tcon_val(WN_st(expr));
      TCON now  = Create_Simd_Const (WN_rtype(expr), then);
      ST *sym = New_Const_Sym (Enter_tcon (now), Be_Type_Tbl(WN_rtype(expr)));
      Allocate_Object(sym);
      opnd_tn[0] = Gen_Symbol_TN (sym, 0, 0);      
    } else       
#endif
    opnd_tn[0] = Gen_Symbol_TN (WN_st(expr), 0, 0);
    num_opnds = 1;
    break;

  case OPR_INTCONST:
    /* Operand is a constant. Extract the information from the whirl
     * node and create a constant TN.
     */
    switch (opcode) {
    case OPC_I8INTCONST:
    case OPC_U8INTCONST:
#ifdef EMULATE_LONGLONG
      const_tn = Gen_Literal_TN_Pair((UINT64) WN_const_val(expr));
#else
      const_tn = Gen_Literal_TN (WN_const_val(expr), 8);
#endif
      break;
#ifndef TARG_X8664
    case OPC_I4INTCONST:
    case OPC_U4INTCONST:
      /* even for U4 we sign-extend the value 
       * so it matches what we want register to look like */
      const_tn = Gen_Literal_TN ((INT32) WN_const_val(expr), 4);
      break;
#else
    case OPC_I4INTCONST:
      /* Let TN_value() and WN_const_val() be consistent.
       */
      const_tn = Gen_Literal_TN ((INT32) WN_const_val(expr), 4);
      break;      
    case OPC_U4INTCONST:
      const_tn = Gen_Literal_TN ((UINT32) WN_const_val(expr), 4);
      break;      
#endif /* TARG_X8664 */
    case OPC_BINTCONST:
      if (result == NULL) result = Allocate_Result_TN (expr, NULL);
      Exp_Pred_Set(result, Get_Complement_TN(result), WN_const_val(expr), &New_OPs);
      return result;
//      const_tn = Gen_Literal_TN (WN_const_val(expr), 0);
//      break;
    default:
      #pragma mips_frequency_hint NEVER
      FmtAssert(FALSE, ("Expand_Expr: %s unhandled", OPCODE_name(opcode)));
      /*NOTREACHED*/
    }

    /* Check if the parent node can take an immediate operand of
     * the given value. If yes, return the const_tn. Otherwise, 
     * load the constant into a register and then use it from there.
     */
    if (Has_Immediate_Operand (parent, expr)) {
      if (Get_Trace (TP_CGEXP, 8)) {
	#pragma mips_frequency_hint NEVER
	fprintf(TFile, "has_immed: %s\n", OPCODE_name(WN_opcode(parent)));
      }
      return const_tn;
    }

    /* If the constant is in a hardwired register, return the register.
     * No need to generate a LDIMM in that case.
     */
    if (result == NULL) {
      if (opcode == OPC_BINTCONST) {

	/* If the constant is boolean 1, it is always available in p0.
	 */
	if (True_TN && WN_const_val(expr) == 1) return True_TN;
      } else {

	/* If the constant is integer 0, it is always available in $0.
	 */
	if (Zero_TN && WN_const_val(expr) == 0) return Zero_TN;
      }
    }

    if (CGSPILL_Rematerialize_Constants && result == NULL) {
      result = Allocate_Result_TN (expr, NULL);
#ifdef TARG_X8664
      // Don't rematerialize a 64-bit imm value under -m32.
      if( !OP_NEED_PAIR( WN_rtype(expr) ) )
#endif // TARG_X8664
	{
	  Set_TN_is_rematerializable(result);
	  Set_TN_home (result, expr);
	}
    }

    opnd_tn[0] = const_tn;
    num_opnds = 1;
    break;

  case OPR_CVTL:
    if (Is_CVT_Noop(expr, parent))
    {
      return Expand_Expr(WN_kid0(expr), parent, result);
    }
    else {
      opnd_tn[0] = Expand_Expr (WN_kid0(expr), expr, NULL);
      opnd_tn[1] = Gen_Literal_TN (WN_cvtl_bits(expr), 4);
      num_opnds = 2;
    }
    break;

  case OPR_CVT:
    if (Is_CVT_Noop(expr, parent))
    {
      return Expand_Expr(WN_kid0(expr), parent, result);
    }
    else if (Is_CVTL_Opcode(opcode))
    {
      opnd_tn[0] = Expand_Expr (WN_kid0(expr), expr, NULL);
      opnd_tn[1] = Gen_Literal_TN (32, 4);
      num_opnds = 2;
    }
    else
    {
      Is_True(WN_desc(expr) != MTYPE_B || WN_rtype(WN_kid0(expr)) == MTYPE_B,
	      ("rtype of xxBCVT kid is not MTYPE_B"));
      opnd_tn[0] = Expand_Expr (WN_kid0(expr), expr, NULL);
      num_opnds = 1;
    }
    break;	/* already set */

  case OPR_PREFETCH:
  case OPR_PREFETCHX:
    if (Prefetch_Kind_Enabled(expr)) {
      VARIANT variant = V_NONE;
      Set_V_pf_flags(variant, WN_prefetch_flag(expr));
      Last_Mem_OP = OPS_last(&New_OPs);
      Exp_Prefetch (top, 
    	Expand_Expr (WN_kid(expr,0), expr, NULL),
    	(opr == OPR_PREFETCH) ?
		Gen_Literal_TN (WN_offset(expr), 4) :
		Expand_Expr (WN_kid(expr,1), expr, NULL),
        variant,
	&New_OPs);
      Set_OP_To_WN_Map(expr);
    }
    return NULL;

  case OPR_DIVREM:
      return Handle_DIVREM(expr, parent, result, opcode);

  case OPR_DIVPART:
      return Handle_DIVPART(expr, parent, result);

  case OPR_REMPART:
      return Handle_REMPART(expr, parent, result);

  case OPR_MINMAX:
      return Handle_MINMAX(expr, parent, result, opcode);

  case OPR_MINPART:
      return Handle_MINPART(expr, parent, result);

  case OPR_MAXPART:
      return Handle_MAXPART(expr, parent, result);

  case OPR_LNOT:
      return Handle_LNOT(expr, parent, result);

  case OPR_LIOR:
  case OPR_LAND:
      return Handle_LAND_LIOR(expr, parent, result);

  case OPR_ALLOCA:
	return Handle_ALLOCA (expr, result);

  case OPR_DEALLOCA:
	Handle_DEALLOCA (expr);
	return NULL;

  case OPR_INTRINSIC_OP:
#ifdef KEY
        if (WN_intrinsic(expr) == INTRN_EXPECT)
          return (Expand_Expr(WN_kid0(WN_kid0(expr)), WN_kid0(expr), result));
#endif
	return Handle_INTRINSIC_OP (expr, result);

  default:
    for (i = 0; i < num_opnds; i++) {
      opnd_tn[i] = Expand_Expr (WN_kid(expr, i), expr, NULL);
      /* TODO: verify that the opnd_tn is the right type. */
      if (TN_has_value(opnd_tn[i])) {
#if TODO_MONGOOSE
	/* Enable this check only when cfold is integrated. */
	Is_True (const_operands == 0, 
		("Expand_Expr: cannot have more than 1 constant operand"));
#else
        if (const_operands != 0) {
	  TN *ldimm_tn = Build_TN_Like (opnd_tn[i]);
	  Last_Mem_OP = OPS_last(&New_OPs);
	  Exp_OP1 (OPC_I4INTCONST, ldimm_tn, opnd_tn[i], &New_OPs); 
	  Set_OP_To_WN_Map(expr);
	  opnd_tn[i] = ldimm_tn;
	  printf ("Expand_Expr: cannot have more than 1 constant operand\n");
	}
#endif /* TODO_MONGOOSE */
	const_operands++;
      }
    }
    break;
  }

  /* if we need a result, make sure we have a valid result tn */
  if (OPCODE_is_expression(opcode) && (result == NULL)) {
    result = Allocate_Result_TN (expr, opnd_tn);
  }

  /* We now have the opcode, operands and the result of the OP. Call the
   * expander to add the expanded OP to New_OPs.
   */
  Last_Mem_OP = OPS_last(&New_OPs);
  if (num_opnds > 3) {
    ErrMsg (EC_Unimplemented, "Expand_Expr: cannot handle more than 3 opnds");
  }
  if (top != TOP_UNDEFINED) {
    // Build_OP uses OP_opnds to determine # operands, 
    // so doesn't matter if we pass extra unused ops.
    if (TOP_is_predicated(top)) {
      Build_OP (top, result, True_TN, opnd_tn[0], opnd_tn[1], opnd_tn[2], 
		 &New_OPs);
    } else {
      Build_OP (top, result, opnd_tn[0], opnd_tn[1], opnd_tn[2], &New_OPs);
    }
  } else {
    switch (num_opnds) {
    case 0:
      Exp_OP0 (opcode, result, &New_OPs);
      break;
    case 1:
      Exp_OP1 (opcode, result, opnd_tn[0], &New_OPs);
      break;
    case 2:
      Exp_OP2 (opcode, result, opnd_tn[0], opnd_tn[1], &New_OPs);
      break;
    case 3:
      Exp_OP3 (opcode, result, opnd_tn[0], opnd_tn[1], opnd_tn[2], &New_OPs);
      break;
    }
  }

  /* The TN_is_fpu_int is set on the result,when the OP is constructed,
   * according to the property of the opcode. For [d]mtc1, this is not
   * unconditionally true, the value in the integer register may
   * have in fact been a floating value. Such is the case when TAS is
   * involved. Detect the case and reset the flag (note that rather
   * than generating some complicated check, we'll reset the flag in
   * cases where it's already clear).
   */
  if (opr == OPR_TAS) Reset_TN_is_fpu_int(result);

  Set_OP_To_WN_Map(expr);
  return result;
}


/* Add a label to the current basic block.
 * This routine returns the bb that the label is attached to.
 */
BB *
Add_Label (LABEL_IDX label)
{
  BB *bb = Start_New_Basic_Block ();
  BB_Add_Annotation (bb, ANNOT_LABEL, (void *)(INTPTR)label);
  FmtAssert (Get_Label_BB(label) == NULL,
	("Add_Label: Label %s defined more than once", LABEL_name(label)));
  Set_Label_BB (label,bb);
  return bb;
}

static void
Link_BBs (BB *bb, LABEL_IDX label)
{
  BB      *dst_bb = Get_Label_BB(label);

  FmtAssert(dst_bb != (LABEL_IDX) 0, 
	    ("Build_CFG: Label %s not defined", ST_name(label)));

  Link_Pred_Succ(bb, dst_bb);
}

/* =======================================================================
 *
 *  label_is_external 
 *
 *  The second argument should be a branch with a label_number.
 *  Return TRUE iff the label points outside the REGION of the third argument.
 *  In this case, the first argument should point to the number
 *  of the corresponding exit from the REGION of the third argument.
 *
 * =======================================================================
 */
static 
BOOL label_is_external ( INT *num, WN *wn, BB *bb )
{
  INT32 label_number;
  LABEL_IDX label;
  BB *target_bb;
  RID *rid = BB_rid( bb );
  INT j;
  WN *goto_wn;
  BOOL match;

  label_number = WN_label_number( wn );
  label = label_number;
  target_bb = Get_Label_BB ( label );

  if ( ( target_bb == NULL ) || ( BB_id( target_bb ) < min_bb_id ) ) 
  {
    FmtAssert (rid, ("RID == NULL, label %d doesn't have a matching target",
		     label_number));
    if ( RID_num_exits( rid ) == 1 ) {
      *num = 0;
      return TRUE;
    }
    FmtAssert( RID_num_exits( rid ) > 1,
	      ("found branch to external label when num_exits <= 0") );
    // can't use REGION_search_block because need to return which exit it is
    goto_wn = WN_first( WN_region_exits ( RID_rwn( rid ) ) );
    match = FALSE;
    for ( j = 0; j < RID_num_exits( rid ); j++ ) {
      if ( label_number == WN_label_number( goto_wn ) ) {
	*num = j;
	match = TRUE;
	break;
      }
      goto_wn = WN_next( goto_wn );
    }
    FmtAssert( match, ("no matching label found in REGION exits for "
		       "external label L%d, RGN %d, BB%d",
		       label_number, RID_id(rid), BB_id(bb)));
    return TRUE;
  }

  return FALSE;
}

/* =======================================================================
 *
 *  Prefectch_Kind_Enabled
 *
 *  Test the kind of the prefetch  { {L1,L2}, {load,store} } against
 *  the four CG_enable_pf flags.  wn must be a prefetch whirl node.
 *
 * =======================================================================
 */
BOOL Prefetch_Kind_Enabled( WN *wn )
{
  BOOL is_read, is_write;
  BOOL is_L1, is_L2;
  BOOL z_conf;
  BOOL confidence_match;
  INT32 nz_conf;

  is_read  = WN_pf_read( wn );
  is_write = WN_pf_write( wn );
  is_L1 = ( WN_pf_stride_1L( wn ) != 0 );
  is_L2 = ( WN_pf_stride_2L( wn ) != 0 );

  z_conf  = ( WN_pf_confidence( wn ) == 0 );
  nz_conf =  WN_pf_confidence( wn );
  confidence_match = (    ( z_conf  && CG_enable_z_conf_prefetch )
		       || ( (nz_conf > 1) && CG_enable_nz_conf_prefetch ) );

  if ( confidence_match ) {
    
    if ( is_read && is_L1 )
      return CG_enable_pf_L1_ld;
    
    if ( is_write && is_L1 )
      return CG_enable_pf_L1_st;
    
    if ( is_read && is_L2 )
      return CG_enable_pf_L2_ld;
    
    if ( is_write && is_L2 )
      return CG_enable_pf_L2_st;
  }

  return FALSE;
}

/* =======================================================================
 *
 *  Has_External_Branch_Target
 *
 *  Return TRUE iff some successor block of bb is outside
 *  the current REGION.
 *
 * =======================================================================
 */
BOOL Has_External_Branch_Target( BB *bb )
{
  WN *branch_wn = BB_branch_wn( bb );
  WN *wn;
  INT i,j;

  if ( branch_wn == NULL )
    return FALSE;

  switch ( WN_opcode( branch_wn ) ) {
  case OPC_TRUEBR:
  case OPC_FALSEBR:
  case OPC_GOTO:
    return label_is_external( &j, branch_wn, bb );
  case OPC_REGION_EXIT:
    return TRUE;
  case OPC_COMPGOTO:
    wn = WN_first( WN_kid1( branch_wn ) );
    for ( i = 0; i < WN_num_entries( branch_wn ); i++ ) {
      if ( label_is_external( &j, wn, bb ) )
	return TRUE;
      wn = WN_next( wn );
    }
    if ( WN_kid_count( branch_wn ) == 3 ) {
      if ( label_is_external( &j, WN_kid( branch_wn, 2 ), bb ) )
	return TRUE;
    }
    return FALSE;
  default:
    #pragma mips_frequency_hint NEVER
    FmtAssert( FALSE, ("unexpected opcode in Has_External_Branch_Target") );
    /*NOTREACHED*/
  }
}

/* =======================================================================
 *
 *  Has_External_Fallthru
 *
 *  Return TRUE iff bb can fall thru to a block outside
 *  the current REGION.
 *
 * =======================================================================
 */
BOOL Has_External_Fallthru( BB *bb )
{
  WN *branch_wn = BB_branch_wn( bb );

  if ( BB_exit( bb ) )
    return FALSE;

  if ( ( branch_wn == NULL )
       || ( WN_opcode( branch_wn ) == OPC_TRUEBR ||
	    WN_opcode( branch_wn ) == OPC_FALSEBR ) ) {
    return ( BB_next( bb ) == NULL );
  }

  switch ( WN_opcode( branch_wn ) ) {
  case OPC_GOTO:
  case OPC_COMPGOTO:
    return FALSE;
  case OPC_REGION_EXIT:
    return TRUE;
  default:
    #pragma mips_frequency_hint NEVER
    FmtAssert( FALSE, ("unexpected opcode in Has_External_Fallthru") );
    /*NOTREACHED*/
  }
}

#ifdef KEY
// Takes a BB. If the branch condition had a __builtin_expect, then
// return the user-expected probability the branch would be taken.
// Return -1 if unable to compute a probability.
static float get_branch_confidence (BB * bb)
{
  WN * branch_wn = BB_branch_wn(bb);
  if (!branch_wn || (WN_operator(branch_wn) != OPR_TRUEBR &&
                     WN_operator(branch_wn) != OPR_FALSEBR))
    return -1.0;

  WN * cond = WN_kid0(branch_wn);
  if (!cond || // fake branch
      (WN_operator(cond) != OPR_EQ && WN_operator(cond) != OPR_NE))
    return -1.0;

  WN * intrn_op = WN_kid0(cond);
  // Quick check for early return.
  if (WN_operator(intrn_op) != OPR_INTRINSIC_OP ||
      WN_intrinsic(intrn_op) != INTRN_EXPECT)
    return -1.0;

  WN * constval = WN_kid1(cond);
  // The prediction cannot be computed if this is not a constant.
  if (WN_operator(constval) != OPR_INTCONST)
    return -1.0;

  // Constant C (2nd operand) in __builtin_expect.
  WN * expected_value = WN_kid0(WN_kid1(intrn_op));
  Is_True (WN_operator(expected_value) == OPR_INTCONST,
           ("get_branch_confidence: 2nd operand of __builtin_expect must "
            "be constant -- front-end should ensure this"));

  INT confidence = WN_const_val(expected_value) == WN_const_val(constval);
  if (WN_operator(cond) == OPR_NE)
    confidence = !confidence;
  if (WN_operator(branch_wn) == OPR_FALSEBR)
    confidence = !confidence;

  // Assign a 90% probability that the user's branch prediction is correct.
  if (confidence) return 0.90;
  else return 0.10;
}
#endif

/* Build the control flow graph for the code generator. */
static void Build_CFG(void)
{
  BB *bb;
  WN *branch_wn, *wn;
  INT i, num;
  RID *rid;
  OP *br_op;
  TN *target_tn;
  LABEL_IDX label;

  // Recompute the list of exits, since with regions
  // some initial exit blocks may get later optimized away.
  while (Exit_BB_Head) {
    Exit_BB_Head = BB_LIST_Delete(BB_LIST_first(Exit_BB_Head), Exit_BB_Head);
  }

  for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {

    if (BB_exit(bb)) {
	Exit_BB_Head = BB_LIST_Push (bb, Exit_BB_Head, &MEM_pu_pool);
    }

    rid = BB_rid(bb);
    if ( rid && RID_level(rid) >= RL_CGSCHED ) {
      /* This block has already been through this process
	 If it is a REGION exit, but not a PU exit, it may not
	 have been linked to all its successors. */
      if ( BB_REGION_Exit( bb, rid ) != NO_REGION_EXIT && !BB_exit( bb ) ) {
	br_op = BB_branch_op( bb );
	if ( br_op == NULL ) {
	  if ( BB_next( bb ) && !BB_exit( bb ) )
	    /* for some reason there is an empty block after a return block
	       but this should not really be a successor */
	    Link_Pred_Succ ( bb, BB_next( bb ) );
	} else {
	  if ( OP_cond( br_op ) ) {
	    if ( BB_next( bb ) )
	      Link_Pred_Succ ( bb, BB_next( bb ) );
	  }
	  /* Is the target tn always operand 0? */
	  target_tn = OP_opnd(br_op, OP_find_opnd_use(br_op, OU_target));
	  FmtAssert( TN_is_label( target_tn ),
		    ("target of branch is not a label") );
	  label = TN_label( target_tn );
	  if ( Get_Label_BB ( label ) != NULL )
	    Link_BBs( bb, label );
	}
      }
      continue;
    }

    if (BB_exit(bb)) {
      /* There are no successors for a procedure exit. */
      continue;
    }

#ifdef KEY
    float confidence = -1.0;
#endif

    branch_wn = BB_branch_wn(bb);
    if (branch_wn != NULL) {
      switch (WN_opcode(branch_wn)) {
      case OPC_FALSEBR:
      case OPC_TRUEBR:
	/* The conditional branch may have expanded into an unconditional
	 * branch or a NOOP (see pv336306 and pv338171). Make sure we still
	 * have a conditional branch!
	 */
	{
	  OP *br_op = BB_branch_op( bb );
	  if ( br_op == NULL || OP_cond( br_op ) ) {
	    if ( ! Has_External_Fallthru( bb ) ) {
#ifdef KEY
              if ((confidence = get_branch_confidence( bb )) != -1.0) {
                if (BB_next(bb) != Get_Label_BB(Get_WN_Label(branch_wn)))
                  confidence = 1 - confidence;
                Link_Pred_Succ_with_Prob(bb, BB_next(bb), confidence,
                                         FALSE, TRUE, TRUE);
              } else
#endif
                Link_Pred_Succ(bb, BB_next(bb));
#ifdef TARG_X8664
	      // Clean up a fake branch_wn generated by
	      // Expand_Unsigned_Long_To_Float().
	      if( WN_kid0(branch_wn) == NULL ){
		BB_branch_wn(bb) = NULL;
	      }
#endif
	    } 
#ifdef CG_PATHSCALE_MERGE
// Generally we have num_exits==0 while creating cgrin, so CGRIN_exits is
// NULL. With exceptions disabled, PU_has_region is generally not set for
// any PU, hence while creating the bb, we assign NULL to BB_rid. So, without
// exceptions we have rid==0 here. With exceptions we set BB_rid, but 
// num_exits is still 0, so we need the extra check.
	    else if ( rid && CGRIN_exits ( RID_cginfo( rid ) ) ) 
#else
	    else if ( rid ) 
#endif
	    {
	      label_is_external( &num, branch_wn, bb );
	      CGRIN_exit_i( RID_cginfo( rid ), num) = bb;
	      CGRIN_exit_label_i( RID_cginfo( rid ), num) = 0;
	    }

	    if ( br_op == NULL ) break;
	  }
	}
	/* fall through and link the branch target. */
      case OPC_GOTO:
      case OPC_REGION_EXIT:
	if ( ! label_is_external( &num, branch_wn, bb ) ) { /*internal label*/
#ifdef KEY
          if ((WN_opcode(branch_wn) == OPC_TRUEBR ||
               WN_opcode(branch_wn) == OPC_FALSEBR) &&
              confidence != -1.0) {
            Link_Pred_Succ_with_Prob(bb,
                                     Get_Label_BB(Get_WN_Label(branch_wn)),
                                     1 - confidence, FALSE, TRUE, TRUE);
          } else
#endif
            Link_BBs(bb, Get_WN_Label(branch_wn));	
        } else if ( rid ) {
	  WN *new_exit;
	  LABEL_IDX new_label;
	  CGRIN *cgrin = RID_Find_Cginfo(bb);
	  Is_True(cgrin != NULL,("Build_CFG, null cginfo"));
	  CGRIN_exit_i( cgrin, num ) = bb;
	  label = Get_WN_Label( branch_wn );
	  CGRIN_exit_label_i( cgrin, num ) = label;
	  new_label = REGION_Exit_Whirl_Labels(
		      CGRIN_exit_glue_i(cgrin, num), bb, label, rid);
	  /* write this new label into an exit block that will
	     eventually become the exit block for a new inner region */
	  Is_True(new_label != (LABEL_IDX) 0,
		  ("Build_CFG, new region exit label is NULL"));
	  new_exit = WN_CreateRegionExit(new_label);
	  WN_INSERT_BlockLast(CGRIN_nested_exit(cgrin),new_exit);
	}
	break;
      case OPC_COMPGOTO:
	if (WN_kid_count(branch_wn) == 3) { /* default case */
	    Link_BBs (bb, Get_WN_Label(WN_kid(branch_wn,2)));
	} /* fall thru */
      case OPC_XGOTO:
	wn = WN_first(WN_kid1(branch_wn));	/* first goto */
	for (i = 0; i < WN_num_entries(branch_wn); i++) {
	  if ( ! label_is_external( &num, wn, bb ) ) {
	    Link_BBs (bb, Get_WN_Label(wn));
	  } else if ( rid ) {
	    CGRIN_exit_i( RID_cginfo( rid ), num ) = bb;
	    label = Get_WN_Label( wn );
	    CGRIN_exit_label_i( RID_cginfo( rid ), num ) = label;
	    REGION_Exit_Whirl_Labels(
		   CGRIN_exit_glue_i(RID_cginfo(rid), num), bb, label, rid);
	  }
	  wn = WN_next(wn);
	}
	if (WN_kid_count(branch_wn) == 3) {
	  if ( ! label_is_external( &num, WN_kid( branch_wn, 2 ), bb ) )
	    /* default case */
	    Link_BBs (bb, Get_WN_Label(WN_kid(branch_wn,2)));
	  else if ( rid ) {
	    CGRIN_exit_i( RID_cginfo( rid ), num ) = bb;
	    label = Get_WN_Label( WN_kid( branch_wn, 2 ) );
	    CGRIN_exit_label_i( RID_cginfo( rid ), num ) = label;
	    REGION_Exit_Whirl_Labels(
		   CGRIN_exit_glue_i(RID_cginfo(rid), num), bb, label, rid);
	  }
	}
	break;
      case OPC_AGOTO:
	{
	  BB *targ;
	  for (targ = REGION_First_BB; targ != NULL; targ = BB_next(targ)) {
	    if (BB_Has_Addr_Taken_Label(targ)) Link_Pred_Succ(bb, targ);
	  }
	}
      }
    } 
    else if (BB_next(bb) != NULL) {
      if (BB_rid(BB_next(bb)) != BB_rid(bb)
	  && BB_rid(BB_next(bb)) != NULL
	  && CGRIN_entry(RID_cginfo(BB_rid(BB_next(bb)))) != BB_next(bb)) {
	BB *region_entry = CGRIN_entry(RID_cginfo(BB_rid(BB_next(bb))));
	ANNOTATION *ant = ANNOT_Get (BB_annotations(region_entry), ANNOT_LABEL);
	OPS ops;
	OPS_Init(&ops);
	DevWarn("first bb in region %d is not the entry bb", 
		RID_id(BB_rid(BB_next(bb))));
	if (ant != NULL) {
	  label = ANNOT_label(ant);
	} else {
	  label = Gen_Temp_Label();
	  BB_Add_Annotation (region_entry, ANNOT_LABEL, (void *)(INTPTR)label);
	  Set_Label_BB (label,region_entry);
	}
	target_tn = Gen_Label_TN (label, 0);
	Exp_OP1 (OPC_GOTO, NULL, target_tn, &ops);
	BB_Append_Ops(bb, &ops);
	Link_Pred_Succ (bb, region_entry);
      } 
      else if (BB_call(bb)
	&& WN_Call_Never_Return( CALLINFO_call_wn(ANNOT_callinfo(
		ANNOT_Get (BB_annotations(bb), ANNOT_CALLINFO) ))) )
      {
	continue;	// no successor
      } 
      else {
	Link_Pred_Succ (bb, BB_next(bb));
      }
    }
  }
}

/* Given a WHIRL comparison opcode, this routine returns
 * the corresponding variant for the BCOND. 
 * For float compares, keep the order of the comparison and
 * set the false_br flag if invert, because NaN comparisons
 * cannot be inverted.  But integer compares can be inverted.
 */

 // KEY: (bug 11573) Added opcodes with U4 rtype, so that an appropriate
 // variant is returned.
static VARIANT
WHIRL_Compare_To_OP_variant (OPCODE opcode, BOOL invert)
{
  VARIANT variant = V_BR_NONE;
  switch (opcode) {
// >> WHIRL 0.30: replaced OPC_T1{EQ,NE,GT,GE,LT,LE} by OPC_BT1, OPC_I4T1 variants
// TODO WHIRL 0.30: get rid of OPC_I4T1 variants

  // ------------------------- OPR_EQ -------------------------
  case OPC_U4I8EQ:
  case OPC_BI8EQ: case OPC_I4I8EQ: variant = V_BR_I8EQ; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_I8I4EQ: case OPC_U8I4EQ:
  case OPC_U4I4EQ:
#endif
  case OPC_BI4EQ: case OPC_I4I4EQ: variant = V_BR_I4EQ; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U8EQ:
  case OPC_U4U8EQ:
#endif
  case OPC_BU8EQ: case OPC_I4U8EQ: variant = V_BR_U8EQ; break;
  case OPC_U4U4EQ:
  case OPC_BU4EQ: case OPC_I4U4EQ: variant = V_BR_U4EQ; break;
  case OPC_U4FQEQ:
  case OPC_BFQEQ: case OPC_I4FQEQ: variant = V_BR_QEQ; break;
#ifdef TARG_IA64
  case OPC_BF10EQ: case OPC_I4F10EQ: variant = V_BR_XEQ; break;
#endif
  case OPC_U4F8EQ:
  case OPC_BF8EQ: case OPC_I4F8EQ: variant = V_BR_DEQ; break;
  case OPC_U4F4EQ:
  case OPC_BF4EQ: case OPC_I4F4EQ: variant = V_BR_FEQ; break;

  // ------------------------- OPR_NE -------------------------
  case OPC_U4I8NE:
  case OPC_BI8NE: case OPC_I4I8NE: variant = V_BR_I8NE; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_I8I4NE: case OPC_U8I4NE:
  case OPC_U4I4NE:
#endif
  case OPC_BI4NE: case OPC_I4I4NE: variant = V_BR_I4NE; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U8NE:
  case OPC_U4U8NE:
#endif
  case OPC_BU8NE: case OPC_I4U8NE: variant = V_BR_U8NE; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U4NE:
  case OPC_U4U4NE:
#endif
  case OPC_BU4NE: case OPC_I4U4NE: variant = V_BR_U4NE; break;
  case OPC_U4FQNE:
  case OPC_BFQNE: case OPC_I4FQNE: variant = V_BR_QNE; break;
#ifdef TARG_IA64
  case OPC_BF10NE: case OPC_I4F10NE: variant = V_BR_XNE; break;
#endif
  case OPC_U4F8NE:
  case OPC_BF8NE: case OPC_I4F8NE: variant = V_BR_DNE; break;
  case OPC_U4F4NE:
  case OPC_BF4NE: case OPC_I4F4NE: variant = V_BR_FNE; break;

  // ------------------------- OPR_GT -------------------------
  case OPC_U4I8GT:
  case OPC_BI8GT: case OPC_I4I8GT: variant = V_BR_I8GT; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_I8I4GT: case OPC_U8I4GT:
  case OPC_U4I4GT:
#endif
  case OPC_BI4GT: case OPC_I4I4GT: variant = V_BR_I4GT; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U8GT:
  case OPC_U4U8GT:
#endif
  case OPC_BU8GT: case OPC_I4U8GT: variant = V_BR_U8GT; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U4GT:
  case OPC_U4U4GT:
#endif
  case OPC_BU4GT: case OPC_I4U4GT: variant = V_BR_U4GT; break;
  case OPC_U4FQGT:
  case OPC_BFQGT: case OPC_I4FQGT: variant = V_BR_QGT; break;
#ifdef TARG_IA64
  case OPC_BF10GT: case OPC_I4F10GT: variant = V_BR_XGT; break;
#endif
  case OPC_U4F8GT:
  case OPC_BF8GT: case OPC_I4F8GT: variant = V_BR_DGT; break;
  case OPC_U4F4GT:
  case OPC_BF4GT: case OPC_I4F4GT: variant = V_BR_FGT; break;

  // ------------------------- OPR_GE -------------------------
  case OPC_U4I8GE:
  case OPC_BI8GE: case OPC_I4I8GE: variant = V_BR_I8GE; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_I8I4GE: case OPC_U8I4GE:
  case OPC_U4I4GE:
#endif
  case OPC_BI4GE: case OPC_I4I4GE: variant = V_BR_I4GE; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U8GE:
  case OPC_U4U8GE:
#endif
  case OPC_BU8GE: case OPC_I4U8GE: variant = V_BR_U8GE; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U4GE:
  case OPC_U4U4GE:
#endif
  case OPC_BU4GE: case OPC_I4U4GE: variant = V_BR_U4GE; break;
  case OPC_U4FQGE:
  case OPC_BFQGE: case OPC_I4FQGE: variant = V_BR_QGE; break;
#ifdef TARG_IA64
  case OPC_BF10GE: case OPC_I4F10GE: variant = V_BR_XGE; break;
#endif
  case OPC_U4F8GE:
  case OPC_BF8GE: case OPC_I4F8GE: variant = V_BR_DGE; break;
  case OPC_U4F4GE:
  case OPC_BF4GE: case OPC_I4F4GE: variant = V_BR_FGE; break;

  // ------------------------- OPR_LT -------------------------
  case OPC_U4I8LT:
  case OPC_BI8LT: case OPC_I4I8LT: variant = V_BR_I8LT; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_I8I4LT: case OPC_U8I4LT:
  case OPC_U4I4LT:
#endif
  case OPC_BI4LT: case OPC_I4I4LT: variant = V_BR_I4LT; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U8LT:
  case OPC_U4U8LT:
#endif
  case OPC_BU8LT: case OPC_I4U8LT: variant = V_BR_U8LT; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U4LT:
  case OPC_U4U4LT:
#endif
  case OPC_BU4LT: case OPC_I4U4LT: variant = V_BR_U4LT; break;
  case OPC_U4FQLT:
  case OPC_BFQLT: case OPC_I4FQLT: variant = V_BR_QLT; break;
#ifdef TARG_IA64
  case OPC_BF10LT: case OPC_I4F10LT: variant = V_BR_XLT; break;
#endif
  case OPC_U4F8LT:
  case OPC_BF8LT: case OPC_I4F8LT: variant = V_BR_DLT; break;
  case OPC_U4F4LT:
  case OPC_BF4LT: case OPC_I4F4LT: variant = V_BR_FLT; break;

  // ------------------------- OPR_LE -------------------------
  case OPC_U4I8LE:
  case OPC_BI8LE: case OPC_I4I8LE: variant = V_BR_I8LE; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_I8I4LE: case OPC_U8I4LE:
  case OPC_U4I4LE:
#endif
  case OPC_BI4LE: case OPC_I4I4LE: variant = V_BR_I4LE; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U8LE:
  case OPC_U4U8LE:
#endif
  case OPC_BU8LE: case OPC_I4U8LE: variant = V_BR_U8LE; break;
#ifdef CG_PATHSCALE_MERGE
  case OPC_U8U4LE:
  case OPC_U4U4LE:
#endif
  case OPC_BU4LE: case OPC_I4U4LE: variant = V_BR_U4LE; break;
  case OPC_U4FQLE:
  case OPC_BFQLE: case OPC_I4FQLE: variant = V_BR_QLE; break;
#ifdef TARG_IA64
  case OPC_BF10LE: case OPC_I4F10LE: variant = V_BR_XLE; break;
#endif
  case OPC_U4F8LE:
  case OPC_BF8LE: case OPC_I4F8LE: variant = V_BR_DLE; break;
  case OPC_U4F4LE:
  case OPC_BF4LE: case OPC_I4F4LE: variant = V_BR_FLE; break;

  case OPC_BBNE: variant = V_BR_PNE; break;
  case OPC_BBEQ: variant = V_BR_PEQ; break;
// << WHIRL 0.30: replaced OPC_T1{EQ,NE,GT,GE,LT,LE} by OPC_BT1, OPC_I4T1 variants
  }
  if (invert) variant = Negate_BR_Variant(variant);

#ifdef Is_True_On
  if (variant == V_BR_NONE && OPERATOR_is_compare(OPCODE_operator(opcode)))
    DevWarn ("KEY: Unknown branch variant found for %s!", OPCODE_name(opcode));
#endif
  return variant;
}

/* Handle expansion of a TRUEBR and FALSEBR. */
static void
Handle_CONDBR (WN *branch)
{
  WN *condition;
  VARIANT variant;
  TN *operand0, *operand1;
  TN *target_tn;
  BOOL invert;

  condition = WN_kid0 (branch);
  invert = (WN_opcode(branch) == OPC_FALSEBR);
  variant = WHIRL_Compare_To_OP_variant (WN_opcode(condition), invert);
  if (variant != V_BR_NONE) {
    operand0 = Expand_Expr (WN_kid0(condition), condition, NULL);
    operand1 = Expand_Expr (WN_kid1(condition), condition, NULL);
  }
  else if (WN_operator_is(condition, OPR_INTCONST)) {
    BOOL cond_is_true = WN_const_val(condition) != 0;
    variant = (cond_is_true ^ invert) ? V_BR_ALWAYS : V_BR_NEVER;
    operand0 = NULL;
    operand1 = NULL;
  }
  else {
#ifdef TARG_IA64
    /* 
    Ideally MTYPE_B should be used for the return result type of the 
    condition operator. However, it is used for other purposes already.
    So the result class type is checked in this case(before checking i
    the 'condition' result type).
    */
    operand0 = NULL;  // set for old branch
    if (Is_Old_Boolean_Expression(condition)) {
      operand0 = Handle_Bool_As_Predicate(condition, branch, invert);
      operand1 = NULL;
   		variant = V_BR_P_TRUE;
    }
    
    if (operand0==NULL) {
      operand0 = Expand_Expr (condition, branch, NULL);
      if (WN_rtype(condition) == MTYPE_B) {
        Is_True(   WN_operator_is(condition, OPR_LDID) 
  	      && WN_class(condition) == CLASS_PREG,
  	      ("MTYPE_B TRUEBR/FALSEBR condition must be preg or relop"));
        operand1 = NULL;
        variant = V_BR_P_TRUE;
        if (invert) {
  	PREG_NUM preg2_num = WN_load_offset(condition) + 1;
  	operand0 = PREG_To_TN_Array[preg2_num];
        }
      } else {
#ifndef TARG_IA32
        operand1 = Zero_TN;
        variant = (invert) ? V_BR_I8EQ : V_BR_I8NE;
#else
        operand1 = Gen_Literal_TN (0, 4);
        variant = (invert) ? V_BR_I4EQ : V_BR_I4NE;
#endif
      }
    }
  }
#else // TARG_IA64
  operand0 = Expand_Expr (condition, branch, NULL);
  if (WN_rtype(condition) == MTYPE_B) {
    Is_True(   WN_operator_is(condition, OPR_LDID) 
	       && WN_class(condition) == CLASS_PREG,
	       ("MTYPE_B TRUEBR/FALSEBR condition must be preg or relop"));
    operand1 = NULL;
    variant = V_BR_P_TRUE;
    if (invert) {
      PREG_NUM preg2_num = WN_load_offset(condition) + 1;
      operand0 = PREG_To_TN_Array[preg2_num];
    }
  } else {
#if !defined(TARG_IA32) && !defined(TARG_X8664)
    operand1 = Zero_TN;
    variant = (invert) ? V_BR_I8EQ : V_BR_I8NE;
#else
    if (TN_size(operand0) == 8 ||// 64-bit operands under m64, bug 9701
	// 64-bit operands under m32, bug 9700.
	OP_NEED_PAIR(OPCODE_rtype(WN_opcode(condition)))) {
      operand1 = Gen_Literal_TN (0, 8);
      variant = (invert) ? V_BR_I8EQ : V_BR_I8NE;
    } else if (TN_size(operand0) == 1 ||
	       TN_size(operand0) == 4) {
      operand1 = Gen_Literal_TN (0, 4);
      variant = (invert) ? V_BR_I4EQ : V_BR_I4NE;
    } else {
      Is_True(FALSE, ("Handle_CONDBR: unexpected size"));
    }
#endif
  }
}
#endif // TARG_IA64
  target_tn = Gen_Label_TN (Get_WN_Label (branch), 0);
  Exp_OP3v (WN_opcode(branch), NULL, target_tn, operand0, operand1, 
	variant, &New_OPs);
}

/* Handle expansion of a XGOTO. */
static void
Handle_XGOTO (WN *branch)
{
  TN *target_tn;
  WN *wn;
  INT i;
  ST *st;
  INITO_IDX ino;
  INITV_IDX inv, prev_inv;

  target_tn = Expand_Expr (WN_kid0(branch), NULL, NULL);
  Exp_Indirect_Branch (target_tn, &New_OPs);

  /* build jump table in init-data list */
  /* get table address */
  st = WN_st(branch);
  BB_Add_Annotation (Cur_BB, ANNOT_SWITCH, st);
  /* make sure st is allocated */
  Allocate_Object(st);
  ino = New_INITO(st);
  prev_inv = INITV_IDX_ZERO;

  wn = WN_first(WN_kid1(branch));	/* first goto */
  for (i = 0; i < WN_num_entries(branch); i++) {
    FmtAssert ((wn && WN_opcode(wn) == OPC_GOTO),
	       ("XGOTO block doesn't have goto's? (%d)", WN_opcode(wn)));
    LABEL_IDX lab = WN_label_number(wn);
    inv = New_INITV();
    INITV_Init_Label (inv, lab);
    prev_inv = Append_INITV (inv, ino, prev_inv);
    wn = WN_next(wn);
  }
}


/* Convert a whirl branch node into a list of expanded OPs. Terminate
 * the current basic block.
 */
static void
Convert_Branch (WN *branch)
{
  OPCODE opcode;
  TN *target_tn;

  opcode = WN_opcode (branch);
  switch (opcode) {
  case OPC_GOTO:
  case OPC_REGION_EXIT:
    target_tn =  Gen_Label_TN (Get_WN_Label (branch), 0);
    Exp_OP1 (OPC_GOTO, NULL, target_tn, &New_OPs);
    break;
  case OPC_FALSEBR:
  case OPC_TRUEBR:
    Handle_CONDBR (branch);
    break;
  case OPC_XGOTO:
    Handle_XGOTO (branch);
    break;
  case OPC_AGOTO:
    target_tn = Expand_Expr (WN_kid0(branch), NULL, NULL);
    Exp_Indirect_Branch (target_tn, &New_OPs);
    break;
  default:
    #pragma mips_frequency_hint NEVER
    FmtAssert (FALSE, 
               ("Convert_Branch: unexpected opcode %s", OPCODE_name(opcode)));
    /*NOTREACHED*/
  }
  BB_branch_wn(Cur_BB) = branch;
  /* Terminate the basic block. */
  Start_New_Basic_Block ();
}


/* For each procedure entrypoint, add the bb to the list of entry points. */
static void Handle_Entry (WN *entry) 
{
  ST *entry_st;
  BB *entry_bb;
  ENTRYINFO *ent;

  if (WN_Label_Is_Handler_Begin(entry)) {
	// handlers are LABELS, but want function ST in entryinfo.
	// So create a dummy func ST for this pseudo-altentry.
	TY_IDX ty  = Make_Function_Type( MTYPE_To_TY(MTYPE_V));
        PU_IDX pu_idx;
        PU&    pu = New_PU (pu_idx);
        PU_Init (pu, ty, CURRENT_SYMTAB);
        entry_st = New_ST (GLOBAL_SYMTAB);

	char *name = (char *) alloca (strlen("Handler")+1+8+1);
	sprintf (name, "Handler.%d", Current_PU_Count());
        ST_Init (entry_st, Save_Str2i (name, ".", Get_WN_Label(entry)),
                 CLASS_FUNC, SCLASS_TEXT, EXPORT_LOCAL, (TY_IDX) pu_idx);
#ifndef TARG_IA64
	PU_Has_Exc_Handler = TRUE;
#endif
        Allocate_Object(entry_st);

  }
  else {
    	entry_st = WN_st(entry);
	if (Read_Global_Data && ST_sclass(entry_st) == SCLASS_EXTERN) {
		// this can happen when IPA creates global list of all funcs.
		// reset sclass to be defined
		Set_ST_sclass(entry_st, SCLASS_TEXT);
		Allocate_Object(entry_st);
	}
  }

  /* start new bb, but don't put in list of labels */
  entry_bb = Start_New_Basic_Block ();
  Set_BB_entry (entry_bb);
  ent = TYPE_PU_ALLOC (ENTRYINFO);
  ENTRYINFO_name(ent) = entry_st;
  ENTRYINFO_entry_wn(ent) = entry;
  ENTRYINFO_srcpos(ent) = WN_Get_Linenum(entry);
  current_srcpos = ENTRYINFO_srcpos(ent);
  BB_Add_Annotation (entry_bb, ANNOT_ENTRYINFO, ent);
  Entry_BB_Head = BB_LIST_Push (entry_bb, Entry_BB_Head, &MEM_pu_pool);
}

static void Handle_Return (void)
{
  EXITINFO *exit_info;
  BB *exit_bb;

  exit_bb = Cur_BB;
  Set_BB_exit(exit_bb);
  exit_info = TYPE_PU_ALLOC (EXITINFO);
  EXITINFO_srcpos(exit_info) = current_srcpos;
  BB_Add_Annotation (exit_bb, ANNOT_EXITINFO, exit_info);
  /* Terminate the basic block */
  Start_New_Basic_Block ();
}


/* Handle traps (from OP_ASSERT or OP_TRAP) */
static void Handle_Trap(WN *trap) 
{
  Exp_OP1(OPC_TRAP, NULL, Gen_Literal_TN(WN_offset(trap), 4), &New_OPs);
}


// Try to find the load/store following the asm that matches
// the out parameter index.  Return NULL if not found.
// TODO:  search past first stmt for matching store.
// Can be okay to not find anything if out store was optimized away.
static WN*
Find_Asm_Out_Parameter_Load (const WN* stmt, PREG_NUM preg_num, ST** ded_st)
{
  WN* ret_load = NULL;
  for(; stmt != NULL; stmt = WN_next(stmt)) {
#ifdef CG_PATHSCALE_MERGE // bug 5733: need to stop searching at the next ASM statement
    if (WN_operator(stmt) == OPR_ASM_STMT)
      return NULL;
#endif
    if (OPERATOR_is_store(WN_operator(stmt))) {
      WN* load = WN_kid0(stmt);
      OPERATOR opr = WN_operator(load);
      if (opr == OPR_CVT || opr == OPR_CVTL) {
        load = WN_kid0(load);
        opr = WN_operator(load);
      }
      if (OPERATOR_is_load(opr) || opr == OPR_LDA) {
        if (WN_has_sym(load) &&
            WN_class(load) == CLASS_PREG && 
            WN_offset(load) == preg_num) {
          ret_load = load;
          break;
        }
      }
    }
  }
  if (ret_load) {
    if (OPERATOR_has_sym(WN_operator(stmt)) &&
        ST_assigned_to_dedicated_preg(WN_st(stmt))) {
      *ded_st = WN_st(stmt);
    }
  }
  else {
    DevWarn("didn't find out store for asm preg %d", preg_num);
  }
  return ret_load;
}

#ifdef CG_PATHSCALE_MERGE
TYPE_ID
Find_Preg_Type ( ST_IDX st_idx )
{
  char *str = &Str_Table[ ST_name_idx (St_Table[st_idx]) ];
  if (strcmp(str, ".preg_U1") == 0 || strcmp(str, ".preg_I1") == 0)
    return MTYPE_I1;
  if (strcmp(str, ".preg_U2") == 0 || strcmp(str, ".preg_I2") == 0)
    return MTYPE_I2;
  if (strcmp(str, ".preg_U4") == 0 || strcmp(str, ".preg_I4") == 0)
    return MTYPE_I4;
  if (strcmp(str, ".preg_U8")  == 0 || strcmp(str, ".preg_I8") == 0)
    return MTYPE_I8;
  return MTYPE_I4;
}
#endif
// This function handles ASM statements using the newly built support
// for OPs with variable numbers of results and operands. Unlike
// Handle_Asm, which allocates registers for ASM operands very early,
// here we will only choose TNs for ASM operands, and record their 
// subclasses and early clobber properties as an annotation of the
// OP with TOP_asm opcode. The annotation will also contain the list
// of dedicated TNs that are clobbered by the ASM OP. LRA and GRA
// will use these annotations when assigning registers to ASM operand
// TNs. The replacement of TN names within the ASM string will happen
// much later during cgemit (this is required at least for IA-32 because
// of the need to do FP stack fixup).
//
static void
Handle_ASM (const WN* asm_wn)
{
  // 'result' and 'opnd' below have a fixed size as well as
  // the arrays in ASM_OP_ANNOT. Define here so we can sanity check.
  enum { MAX_OPNDS = ASM_OP_size, MAX_RESULTS = ASM_OP_size };

  // these two arrays may have to be reallocatable
  TN* result[MAX_RESULTS];
  TN* opnd[MAX_OPNDS]; 
  INT num_results = 0;
  INT num_opnds = 0;

  ISA_REGISTER_SUBCLASS opnd_sc[OP_MAX_FIXED_OPNDS];
  bzero(opnd_sc, sizeof(opnd_sc));
  
  CGTARG_Init_Asm_Constraints();

  ASM_OP_ANNOT* asm_info = TYPE_PU_ALLOC(ASM_OP_ANNOT);
  bzero(asm_info, sizeof(ASM_OP_ANNOT));

  ASM_OP_wn(asm_info) = asm_wn;

#ifdef TARG_IA32
#if 0  
  // Adding eflags register to the clobber set causes a problem
  // in LRA, because a live range that includes such an ASM OP
  // cannot use eflags register for allocation. Given that we
  // currently don't do any dependence-based transformations for
  // IA-32, it should be safe to ignore Asm_Clobbers_Cc flag.
  //
  if (WN_Asm_Clobbers_Cc(asm_wn)) {
    ASM_OP_clobber_set(asm_info)[ISA_REGISTER_CLASS_eflags] = 
      REGISTER_SET_Union1(REGISTER_SET_EMPTY_SET, REGISTER_MIN);
  }
#endif
#endif

  // process ASM clobber list
  for (const WN* clobber_pragma = WN_first(WN_asm_clobbers(asm_wn));
       clobber_pragma != NULL;
       clobber_pragma = WN_next(clobber_pragma)) {
    Is_True(WN_pragma(clobber_pragma) == WN_PRAGMA_ASM_CLOBBER,
            ("Wrong pragma type for ASM clobber"));
    if (WN_operator(clobber_pragma) == OPR_XPRAGMA) {
      WN* idname = WN_kid0(clobber_pragma);
      Is_True(WN_operator(idname) == OPR_IDNAME,
              ("Wrong kid operator for ASM clobber PREG"));
#ifndef CG_PATHSCALE_MERGE
      // bug 4583: keep track of asm clobbered callee-saved registers, we
      // will generate save/restore of these later.
      {
	PREG_NUM preg = WN_offset (idname);
	ISA_REGISTER_CLASS rclass;
	REGISTER reg;
	CGTARG_Preg_Register_And_Class(preg, &rclass, &reg);
	if (ABI_PROPERTY_Is_callee(rclass, preg-REGISTER_MIN))
	{
	  SAVE_REG_LOC sr;
	  extern STACK<SAVE_REG_LOC> Saved_Callee_Saved_Regs;
	  sr.ded_tn = Build_Dedicated_TN(rclass, reg, 0);
	  DevAssert(sr.ded_tn, 
	            ("Missing dedicated TN for callee-saved register %s",
		     REGISTER_name(rclass, reg)));
	  if (Is_Unique_Callee_Saved_Reg (sr.ded_tn))
	  {
	    sr.temp = CGSPILL_Get_TN_Spill_Location (sr.ded_tn, CGSPILL_LCL);
	    sr.user_allocated = TRUE;
	    Saved_Callee_Saved_Regs.Push(sr);
          }
	}
      }
#endif // KEY
      TN* tn = PREG_To_TN(WN_st(idname), WN_offset(idname));
      FmtAssert(tn && TN_is_register(tn) && TN_is_dedicated(tn),
                ("Wrong TN for PREG from ASM clobber list"));
      ISA_REGISTER_CLASS rc = TN_register_class(tn);
      ASM_OP_clobber_set(asm_info)[rc] = 
        REGISTER_SET_Union1(ASM_OP_clobber_set(asm_info)[rc], TN_register(tn));
    }
  }
  
  // process ASM output parameters:
  // the out stores must directly follow the ASM,
  // while the constraints are in kid1
  WN* asm_output_constraints = WN_asm_constraints(asm_wn);
  FmtAssert(WN_operator(asm_output_constraints) == OPR_BLOCK,
            ("asm output constraints not a block?"));
  
  for (WN* out_pragma = WN_first(asm_output_constraints);
       out_pragma != NULL; 
       out_pragma = WN_next(out_pragma)) {

    FmtAssert(num_results < MAX_RESULTS,
	      ("too many asm results in Handle_ASM"));

    FmtAssert(WN_pragma(out_pragma) == WN_PRAGMA_ASM_CONSTRAINT,
              ("not an asm_constraint pragma"));

    const char* constraint = WN_pragma_asm_constraint(out_pragma);
    PREG_NUM preg = WN_pragma_asm_copyout_preg(out_pragma);
    ST* pref_st = NULL;
    WN* load = Find_Asm_Out_Parameter_Load(WN_next(asm_wn), preg, &pref_st);
#ifdef CG_PATHSCALE_MERGE
    TYPE_ID default_type = Find_Preg_Type(WN_st_idx(out_pragma));
#endif
    TN* pref_tn = NULL;
    if (pref_st) {
      pref_tn = PREG_To_TN(MTYPE_To_PREG(ST_mtype(pref_st)),
                           Find_PREG_For_Symbol(pref_st));
    }
    ISA_REGISTER_SUBCLASS subclass = ISA_REGISTER_SUBCLASS_UNDEFINED;

#ifndef CG_PATHSCALE_MERGE
    TN* tn = CGTARG_TN_For_Asm_Operand(constraint, load, pref_tn, &subclass);
#else
    TN* tn = CGTARG_TN_For_Asm_Operand(constraint, load, pref_tn, &subclass, 
				       default_type);
#endif

    ASM_OP_result_constraint(asm_info)[num_results] = constraint;
    ASM_OP_result_subclass(asm_info)[num_results] = subclass;
    ASM_OP_result_position(asm_info)[num_results] = 
      WN_pragma_asm_opnd_num(out_pragma);
    ASM_OP_result_clobber(asm_info)[num_results] = 
      (strchr(constraint, '&') != NULL);
    ASM_OP_result_memory(asm_info)[num_results] = 
#ifndef TARG_X8664
      (strchr(constraint, 'm') != NULL);
#else
    (strchr(constraint, 'm') != NULL || strchr(constraint, 'g') != NULL);
#endif
    
    result[num_results] = tn;
    num_results++;
    
    // in WHIRL store that follows ASM stmt, replace negative
    // negative preg with new_preg mapped to ASM operand TN
    // it is possible that wopt optimized away the output store
    if (load) {
      PREG_NUM new_preg = TN_To_PREG(tn);
      if (new_preg == 0) {
        char preg_name[16];
        sprintf(preg_name,"_asm_result_%d",WN_pragma_asm_opnd_num(out_pragma));
	new_preg = Create_Preg (TY_mtype(ST_type(WN_st(load))), preg_name);
	Realloc_Preg_To_TN_Arrays (new_preg);
        TN_MAP_Set(TN_To_PREG_Map, tn, (void*)(INTPTR)new_preg);
        PREG_To_TN_Array[new_preg] = tn;
        PREG_To_TN_Mtype[new_preg] = TY_mtype(ST_type(WN_st(load)));
      }
      WN_offset(load) = new_preg;
    } 

  }

  // process asm input parameters, which are kids 2-n
  for (INT kid = 2; kid < WN_kid_count(asm_wn); ++kid) {
    FmtAssert(num_opnds < MAX_OPNDS,
	      ("too may asm operands in Handle_ASM"));

    WN* asm_input = WN_kid(asm_wn, kid);
    FmtAssert(WN_operator(asm_input) == OPR_ASM_INPUT,
              ("asm kid not an asm_input?"));

    const char* constraint = WN_asm_input_constraint(asm_input);
    WN* load = WN_kid0(asm_input);
    TN* pref_tn = NULL;
    if (OPERATOR_has_sym(WN_operator(load))) {
      ST* pref_st = WN_st(load);
      if (ST_assigned_to_dedicated_preg(pref_st)) {
        pref_tn = PREG_To_TN(MTYPE_To_PREG(ST_mtype(pref_st)),
                             Find_PREG_For_Symbol(pref_st));
      }
    }
    ISA_REGISTER_SUBCLASS subclass = ISA_REGISTER_SUBCLASS_UNDEFINED;

#ifndef CG_PATHSCALE_MERGE
    TN* tn = CGTARG_TN_For_Asm_Operand(constraint, load, pref_tn, &subclass);
#else
    TN* tn = CGTARG_TN_For_Asm_Operand(constraint, load, pref_tn, &subclass, 
				       MTYPE_I4);
#endif

    ASM_OP_opnd_constraint(asm_info)[num_opnds] = constraint;
    ASM_OP_opnd_subclass(asm_info)[num_opnds] = subclass;
    ASM_OP_opnd_position(asm_info)[num_opnds] = WN_asm_opnd_num(asm_input);
    ASM_OP_opnd_memory(asm_info)[num_opnds] = 
#ifndef TARG_X8664
      (strchr(constraint, 'm') != NULL);
#else
      (strchr(constraint, 'm') != NULL || strchr(constraint, 'g') != NULL);
#endif

    // we should create a TN even if it's an immediate
    // constraints on immediates are target-specific
    if (TN_is_register(tn)) {
#ifdef TARG_X8664
      if( ( TN_register_class(tn) == ISA_REGISTER_CLASS_x87 ) &&
	  ( WN_rtype(load) != MTYPE_FQ ) ){
	const TYPE_ID rtype = WN_rtype(load);
	extern void Expand_Float_To_Float( TN*, TN*, TYPE_ID, TYPE_ID, OPS* );
	FmtAssert( MTYPE_is_float(rtype), ("NYI") );
	TN* tmp_tn = Gen_Typed_Register_TN( rtype, MTYPE_byte_size(rtype) );

	Expand_Expr( load, NULL, tmp_tn );
	Expand_Float_To_Float( tn, tmp_tn, MTYPE_FQ, rtype, &New_OPs );
      } else
#endif // TARG_X8664
	Expand_Expr (load, NULL, tn);
    }

#ifdef TARG_X8664
    /* To save some registers, we need some special handling for
       the "m" constraint.
    */
    /* Bug 5575 - do this only when -fPIC is not used. 
       CGTARG_Process_Asm_m_constraint (added to address bug 3111) is not 
       designed to work when -fPIC is used. 
    */
    if (ASM_OP_opnd_memory(asm_info)[num_opnds] &&
       (!Gen_PIC_Shared ||
        // Local symbols can be calculated using one instruction without a
        // separate add, just like non-PIC code.  Bug 12605.
        (WN_operator(load) == OPR_LDA &&
         ST_is_export_local(WN_st(load))))) {
      TN* new_opnd_tn =
	CGTARG_Process_Asm_m_constraint( load,
					 &ASM_OP_opnd_offset(asm_info)[num_opnds],
					 &New_OPs );
      if( new_opnd_tn != NULL )
	tn = new_opnd_tn;
    }
#endif // TARG_X8664

    opnd[num_opnds] = tn;    
    num_opnds++;
  }

  // now create ASM op
  OP* asm_op = Mk_VarOP(TOP_asm, num_results, num_opnds, result, opnd);
  if (WN_Asm_Volatile(asm_wn)) {
	Set_OP_volatile(asm_op);
  }
  OPS_Append_Op(&New_OPs, asm_op);
  OP_MAP_Set(OP_Asm_Map, asm_op, asm_info);

#ifdef CG_PATHSCALE_MERGE
  BB_Add_Annotation( Cur_BB, ANNOT_ASMINFO, asm_info );

  /* Terminate the basic block */
  Start_New_Basic_Block ();
#endif
}


// replace all occurrences of match string with new string in s string.
static void
Replace_Substring (char *s, char *match, char *newstr)
{
  // need temp buffer since will modify s string
  char *buf = (char*) alloca(strlen(s)+16);
  // iterate until no matches
  while (TRUE) {
	char *p = strstr (s, match);
	if (p == NULL) {	// no match this time
        	return; 	
	}
	char *match_end = p + strlen(match);
	*p = '\0';
	sprintf(buf, "%s%s%s", s, newstr, match_end);
	strcpy(s,buf);
  }
}

static void
Modify_Asm_String (char *asm_string, INT pattern_index, TN *tn, char *tn_name)
{
  char pattern[4];
  sprintf(pattern, "%%%c", '0'+pattern_index);	// %N
  Replace_Substring (asm_string, pattern, tn_name);

  if (tn && TN_is_register(tn)) {
    for (INT i = 0; i < CGTARG_Num_Asm_Opnd_Modifiers; i++) {
      char modifier = CGTARG_Asm_Opnd_Modifiers[i];
      sprintf(pattern, "%%%c%c", modifier, '0'+pattern_index);
      char* mod_name = (char*) CGTARG_Modified_Asm_Opnd_Name(modifier, tn, tn_name);
      Replace_Substring (asm_string, pattern, mod_name);
    }
  }
}

/* Try to find and return a non-zero SRCPOS for the loop starting
 * with <body_label>.  (LABELs apparently don't have linenum info.) 
 * Since this is only for notes/debugging purposes, give up fairly
 * easily.
 */
static SRCPOS get_loop_srcpos(WN *body_label)
{
  if (current_srcpos) {
    return current_srcpos;
  } else if (WN_linenum(body_label)) {
    return WN_linenum(body_label);
  } else {
    WN *wn = WN_next(body_label);
    while (wn && WN_linenum(wn) == 0 && WN_opcode(wn) != OPC_LABEL)
      wn = WN_next(wn);
    return wn ? WN_linenum(wn) : 0;
  }
}



/* Expand a WHIRL statement into a list of OPs and add 
 * them to the current basic block.
 */
static void Expand_Statement (WN *stmt) 
{
  BB *bb;
  WN *loop_info;
  LOOPINFO *info = NULL;
  TN *trip_tn;
  OPCODE opc = WN_opcode(stmt);

  PU_WN_Cnt++;

  switch (opc) {
/*
  case OPC_EXC_SCOPE_BEGIN:
    new_label = Gen_Number_Label( ++SYMTAB_last_label( Current_Symtab ) );
    Allocate_Object(new_label);
    bb = Add_Label (new_label);
    EH_Begin_Range (stmt, new_label);
    break;
  case OPC_EXC_SCOPE_END:
    new_label = Gen_Number_Label( ++SYMTAB_last_label( Current_Symtab ) );
    Allocate_Object(new_label);
    bb = Add_Label (new_label);
    EH_End_Range(new_label);
    break;
*/
  case OPC_TRUEBR:
  case OPC_FALSEBR:
  case OPC_GOTO:
  case OPC_AGOTO:
  case OPC_COMPGOTO:
  case OPC_XGOTO:
  case OPC_REGION_EXIT:
    Convert_Branch (stmt);
    break;
  case OPC_RETURN:
    /* For regions with returns, could try to jump back through nested 
     * regions until reach outer PU, but that is inefficient, so instead
     * put the return code right in the region.  This means that once the
     * PU is finished we will insert exit code (epilog restore code) in
     * the exit_bb in the region.
     */
    Handle_Return ();
    break;
  case OPC_LABEL:
    loop_info = WN_label_loop_info(stmt);
    if (loop_info && CG_opt_level > 1) {
      WN *trip_wn = WN_loop_trip(loop_info);
      SRCPOS srcpos = get_loop_srcpos(stmt);
      if (trip_wn && WN_operator_is(trip_wn, OPR_INTCONST) &&
	  WN_const_val(trip_wn) < 1) {
	/*
	 * Usually, this indicates an error in the trip count computation,
	 * not an actual trip count < 1, since the optimizer removes such
	 * loops.  (Though we can get actual trip counts < 1 if the optimizer
	 * doesn't run, but CG optimization is done.)  So instead of removing
	 * the loop, simply warn and set the trip count to NULL so we won't
	 * believe it.
	 */
	DevWarn("removing loop trip count (line %d) "
		"(< 1, either useless or invalid)",
		Srcpos_To_Line(srcpos));
	WN_set_loop_trip(loop_info, NULL);
	trip_wn = NULL;
      }
      if (trip_wn == NULL) {
	trip_tn = NULL;
      } else {
	if (WN_operator_is(trip_wn, OPR_INTCONST)) {
	  INT64 trip_val = WN_const_val(trip_wn);
	  UINT16 sz = MTYPE_RegisterSize(WN_rtype(trip_wn));
	  /* Correct trip estimate - not always right */
	  WN_loop_trip_est(loop_info) = MIN(trip_val, UINT16_MAX);
	  trip_tn = Gen_Literal_TN(WN_const_val(trip_wn), sz);
	} else {
	  /*
	   * Trip count TN must be defined in the BB immediately preceding
	   * the loop body so that CG_LOOP_Attach_Prolog_And_Epilog can
	   * include this BB in the prolog.  This is necessary so that
	   * the trip count TN is properly marked as live-out by
	   * CG_LOOP_Recompute_Liveness, which limits the liveness
	   * recomputation to the loop region.
	   */
	  if (OPS_first(&New_OPs)) {
	    Start_New_Basic_Block();
	  }
	  trip_tn = Expand_Expr(trip_wn, NULL, NULL);
	  if (OPS_first(&New_OPs) == NULL) {
	    /* Make sure trip TN is defined in this BB. */
	    TN *tmp = Dup_TN_Even_If_Dedicated(trip_tn);
	    Exp_COPY(tmp, trip_tn, &New_OPs);
	    trip_tn = tmp;
	  }
	}
      }
      info = TYPE_P_ALLOC(LOOPINFO);
      LOOPINFO_wn(info) = loop_info;
      LOOPINFO_srcpos(info) = srcpos;
      LOOPINFO_trip_count_tn(info) = trip_tn;
      if (!CG_PU_Has_Feedback && WN_loop_trip_est(loop_info) == 0)
	WN_loop_trip_est(loop_info) = 100;
    }
    if (WN_Label_Is_Handler_Begin(stmt)) {
	LABEL_IDX label = Get_WN_Label(stmt);
	Handle_Entry(stmt);
	bb = BB_LIST_first(Entry_BB_Head);
        Set_BB_handler(bb);
	BB_Add_Annotation (bb, ANNOT_LABEL, (void *)(INTPTR)label);
	FmtAssert (Get_Label_BB(label) == NULL,
       		("Label %s defined more than once", LABEL_name(label)));
	Set_Label_BB (label,bb);
    } else {
    	/* start of a new basic block */
    	bb = Add_Label(Get_WN_Label (stmt));
    }
    if (info) {
      BB_Add_Annotation(bb, ANNOT_LOOPINFO, info);
      if (last_loop_pragma) {
	BB_Add_Annotation(bb, ANNOT_PRAGMA, last_loop_pragma);
	last_loop_pragma = NULL;
      }
    }
    break;
  case OPC_ALTENTRY:
    Handle_Entry (stmt);
    break;
  case OPC_PRAGMA:
  case OPC_XPRAGMA:
    if (WN_pragmas[WN_pragma(stmt)].users == PUSER_CG) {
      if (WN_pragma(stmt) == WN_PRAGMA_UNROLL)
	/*
	 * Will place loop pragmas on loop head BB.  Currently we
	 * need only one pragma per loop, so a single WN * is fine
	 * for tracking this.
	 */
	last_loop_pragma = stmt;
      else
	BB_Add_Annotation(Cur_BB, ANNOT_PRAGMA, stmt);
    }
#ifdef TARG_X8664 
    if (WN_pragma(stmt) == WN_PRAGMA_PREAMBLE_END)
      WN_pragma_preamble_end_seen = TRUE;
#endif
    break;
  case OPC_COMMENT:
    COMMENT_Add(Cur_BB, WN_GetComment(stmt));
    break;
  case OPC_EVAL:
    /* For now, just evaluate the kid0. */
#if defined(TARG_IA32) || defined(TARG_X8664)
    if (WN_has_side_effects(WN_kid0(stmt)))
#endif
    Expand_Expr (WN_kid0(stmt), NULL, NULL);
    break;
  case OPC_TRAP:
    Handle_Trap(stmt);
    break;
  case OPC_ASM_STMT:
    Handle_ASM (stmt);
    break;
  default:
    PU_WN_Cnt--;	/* don't want to count node twice */
    Expand_Expr (stmt, NULL, NULL);
    break;
  }
}


static WN *
Handle_INTRINSIC_CALL (WN *intrncall)
{
  enum {max_intrinsic_opnds = 3};
  TN *result;
  TN *opnd_tn[max_intrinsic_opnds];
  INT i;
  LABEL_IDX label = LABEL_IDX_ZERO;
  OPS loop_ops;

  FmtAssert(WN_num_actuals(intrncall) <= max_intrinsic_opnds,
	    ("too many intrinsic call operands (%d)", WN_num_actuals(intrncall)));

  WN *next_stmt = WN_next(intrncall);
  INTRINSIC id = (INTRINSIC) WN_intrinsic (intrncall);

#ifdef TARG_X8664
  switch( id ){
  case INTRN_SAVE_XMMS:
    {
      Exp_Savexmms_Intrinsic(intrncall, 
			     Expand_Expr(WN_kid0(intrncall), intrncall, NULL), 
			     &label, &New_OPs);
      BB *bb = Start_New_Basic_Block();
      BB_Add_Annotation (bb, ANNOT_LABEL, (void *)(INTPTR)label);
      Set_Label_BB (label,bb);
      return next_stmt;
    }
    break;

  case INTRN_FETCH_AND_ADD_I4:
  case INTRN_FETCH_AND_ADD_I8:
    {
      Exp_Fetch_and_Add( Expand_Expr(WN_kid0(intrncall), intrncall, NULL),
			 Expand_Expr(WN_kid1(intrncall), intrncall, NULL),
			 WN_rtype(intrncall),
			 &New_OPs );

      return next_stmt;
    }
    break;
  case INTRN_FETCH_AND_AND_I4:
  case INTRN_FETCH_AND_AND_I8:
    {
      Exp_Fetch_and_And( Expand_Expr(WN_kid0(intrncall), intrncall, NULL),
                         Expand_Expr(WN_kid1(intrncall), intrncall, NULL),
			 WN_rtype(intrncall),
			 &New_OPs );
      return next_stmt;
    }
    break;
  case INTRN_FETCH_AND_OR_I4:
  case INTRN_FETCH_AND_OR_I8:
    {
      Exp_Fetch_and_Or( Expand_Expr(WN_kid0(intrncall), intrncall, NULL),
                        Expand_Expr(WN_kid1(intrncall), intrncall, NULL),
			WN_rtype(intrncall),
			&New_OPs );
      return next_stmt;
    }
    break;
  case INTRN_FETCH_AND_XOR_I4:
  case INTRN_FETCH_AND_XOR_I8:
    {
      Exp_Fetch_and_Xor( Expand_Expr(WN_kid0(intrncall), intrncall, NULL),
                         Expand_Expr(WN_kid1(intrncall), intrncall, NULL),
			 WN_rtype(intrncall),
			 &New_OPs );
      return next_stmt;
    }
    break;
  case INTRN_FETCH_AND_SUB_I4:
  case INTRN_FETCH_AND_SUB_I8:
    {
      Exp_Fetch_and_Sub( Expand_Expr(WN_kid0(intrncall), intrncall, NULL),
                         Expand_Expr(WN_kid1(intrncall), intrncall, NULL),
			 WN_rtype(intrncall),
			 &New_OPs );
      return next_stmt;
    }
    break;
  case INTRN_COMPARE_AND_SWAP_I4:
  case INTRN_COMPARE_AND_SWAP_I8:
    {
      result = Exp_Compare_and_Swap( Expand_Expr(WN_kid0(intrncall), intrncall, NULL),
			 Expand_Expr(WN_kid1(intrncall), intrncall, NULL),
			 Expand_Expr(WN_kid2(intrncall), intrncall, NULL),
			 WN_rtype(WN_kid1(intrncall)),
			 &New_OPs );

//      return next_stmt;
      goto cont;
    }
    break;
  }
#endif

#ifdef CG_PATHSCALE_MERGE
  opnd_tn[0] = opnd_tn[1] = opnd_tn[2] = NULL;
#endif

  for (i = 0; i < WN_num_actuals(intrncall); i++) {
    // TODO:  currently, literals always get expanded into regs,
    // which may not be ideal for some intrinsics like fetch_and_add.
    opnd_tn[i] = Expand_Expr (WN_kid(intrncall, i), intrncall, NULL);
  }

  // if straight-line code, then label and loop_ops are unused,
  // but might create a loop in which case we need to create bb for it.
  // (other possible ways of doing this would have been to use split_bb
  // or multiple exp_ calls for the different parts).

  OPS_Init(&loop_ops);
  result = Exp_Intrinsic_Call (id, 
	opnd_tn[0], opnd_tn[1], opnd_tn[2], &New_OPs, &label, &loop_ops);

  if (OPS_first(&loop_ops) != NULL && label != LABEL_IDX_ZERO) {
	BB *bb = Start_New_Basic_Block ();
	BB_Add_Annotation (bb, ANNOT_LABEL, (void *)(INTPTR)label);
	Set_Label_BB (label,bb);
  	BB_branch_wn(bb) = WN_Create(OPC_FALSEBR,1);
  	WN_label_number(BB_branch_wn(bb)) = label;
	New_OPs = loop_ops;
	Start_New_Basic_Block ();
  }

#ifdef CG_PATHSCALE_MERGE
  cont:
#endif
  /* Expand the next statement and check if it has a use of $2. If any
   * use of $2 is found, replace it by the 'result' TN of the intrncall.
   */
  if (next_stmt != NULL && 
      result != NULL &&
      !WN_operator_is (next_stmt, OPR_INTRINSIC_CALL))
  {
    OP *op;
    if (Trace_WhirlToOp) {
      fprintf (TFile, "---------------------------------\n");
      fdump_tree_with_freq (TFile, next_stmt, WN_MAP_UNDEFINED);
    }
    Expand_Statement (next_stmt);
    next_stmt = WN_next(next_stmt);
    FOR_ALL_OPS_OPs_REV (&New_OPs, op) {
      if (OP_code(op) == TOP_intrncall) break;
      for (i = 0; i < OP_opnds(op); i++) {
	TN *otn = OP_opnd(op,i);

	if (   TN_is_dedicated(otn)
	    && (TN_register_and_class(otn) == CLASS_AND_REG_v0
#ifdef TARG_X8664
	        ||
	        TN_register_and_class(otn) == CLASS_AND_REG_f0
#endif
	       )
        ) {
	  Set_OP_opnd (op, i, result);
	}
      }
    }
  }
  return next_stmt;
}

/* very similar to routine in cflow; someday should commonize */
static BOOL
Only_Has_Exc_Label(BB *bb)
{
  if (BB_has_label(bb)) {
    ANNOTATION *ant;

    for (ant = ANNOT_First(BB_annotations(bb), ANNOT_LABEL);
         ant != NULL;
         ant = ANNOT_Next(ant, ANNOT_LABEL)
    ) {
      LABEL_IDX lab = ANNOT_label(ant);
      if (!LABEL_begin_eh_range(lab) && !LABEL_end_eh_range(lab))
	return FALSE;
    }
    return TRUE;
  }
  return FALSE;
}


/*
 * Input must be either a REGION or a non-SCF statement
 */
static void
convert_stmt_list_to_OPs(WN *stmt)
{
  RID *rid;
  WN *first;
  BB *prev, *last;
  WN *next_stmt;

  for( ; stmt; stmt = next_stmt ) 
  {
    next_stmt = WN_next(stmt);

    switch( WN_opcode( stmt ) ) {
    case OPC_REGION:
      rid = REGION_get_rid(stmt);
      Is_True(rid != NULL, ("convert_stmt_list_to_OPs NULL RID"));
      if ( RID_level( rid ) < RL_CG ) { /* the region is still WHIRL */
	region_stack_push( rid );

#ifdef TARG_IA64
	// if current PU haven't any landing pad, no need to set EH label
#ifdef OSP_OPT
	if (RID_TYPE_eh(rid) &&
	    RID_eh_range_ptr(rid) &&
	    !PU_Need_Not_Create_LSDA ()) {
#else 
	if (RID_TYPE_eh(rid) && RID_eh_range_ptr(rid)) { 
#endif
#else  // TARG_IA64
	  if (RID_TYPE_eh(rid) && RID_eh_range_ptr(rid)) {
#endif		    
	  EH_Set_Start_Label(RID_eh_range_ptr(rid));
        }

	first = WN_first( WN_region_body( stmt ) );
	Start_New_Basic_Block();
	if (RID_is_glue_code(rid)) {
		In_Glue_Region = TRUE;
	}
	convert_stmt_list_to_OPs( first );
	if (RID_is_glue_code(rid)) {
		In_Glue_Region = FALSE;
	}
#ifdef TARG_IA64
	// if current PU haven't any landing pad, no need to set EH label
#ifdef OSP_OPT
	if (RID_TYPE_eh(rid) && 
	    RID_eh_range_ptr(rid) && 
	    !PU_Need_Not_Create_LSDA ()) {
#else 
	if (RID_TYPE_eh(rid) && RID_eh_range_ptr(rid)) { 
#endif
#else  // TARG_IA64
	  if (RID_TYPE_eh(rid) && RID_eh_range_ptr(rid)) {
#endif		    
	  EH_Set_End_Label(RID_eh_range_ptr(rid));
#ifdef KEY
	  /* When a region is ended, always force to create a new bb, so
	     that the next region will not share any common bb with the
	     current region. (bug#3140)
	   */
	  {
	    BB* old_bb = Cur_BB;
	    Start_New_Basic_Block();
	    if( Cur_BB == old_bb ){
	      Cur_BB = Gen_And_Append_BB( Cur_BB );
	      BB_rid(Cur_BB) = Non_Transparent_RID(current_region);
	    }
	  }
#endif
        }
	rid = region_stack_pop();
      } else {			/* the region has been lowered to OPs */
	BB *old_bb = Cur_BB;
	Start_New_Basic_Block();
	if (old_bb == Cur_BB) {
		/* need empty bb before nested region,
		 * so successor-bb finds outer region and right tns_in list
		 * rather than inner region (i.e. otherwise at bb level it
		 * will look like we transfer directly from pu to inner region).
		 * Also, if BB_has_label want new bb for label, 
		 * but now we always have new bb.
		 */
		Set_BB_gra_spill(Cur_BB);	/* so cflow won't remove */
		Cur_BB = Gen_And_Append_BB(Cur_BB);
		BB_rid(Cur_BB) = Non_Transparent_RID(current_region);
	}

	prev = BB_prev( Cur_BB );
	last = Append_Region_BBs( prev, rid );
	BB_prev( Cur_BB ) = last;
	BB_next( last ) = Cur_BB;

	CGRIN *cginfo = RID_cginfo(rid);
	Is_True(cginfo != NULL, ("convert_stmt_list_to_OPs, null cginfo"));
	if ( CGRIN_min_bb_id( cginfo ) < min_bb_id )
	  min_bb_id = CGRIN_min_bb_id( cginfo );
	if ( CGRIN_first_regular_tn( cginfo ) < First_REGION_TN )
	  First_REGION_TN = CGRIN_first_regular_tn( cginfo );
	if ( CGRIN_first_gtn( cginfo ) < First_REGION_GTN )
	  First_REGION_GTN = CGRIN_first_gtn( cginfo );
      }
      break;
    default:
      Is_True((OPCODE_is_stmt(WN_opcode(stmt))), 
	      ("convert_stmt_list_to_OPs: %d not a stmt", WN_opcode(stmt)));
      
      if (Trace_WhirlToOp) {
	fprintf (TFile,
		 "----- convert_stmt_list_to_OPs, BB%d -------------------\n",
		 BB_id(Cur_BB));
	fdump_tree_with_freq (TFile, stmt, WN_MAP_UNDEFINED);
      }
      if (WN_Get_Linenum(stmt))
	current_srcpos = WN_Get_Linenum(stmt);
      if (WN_operator_is (stmt, OPR_INTRINSIC_CALL)) {
	next_stmt = Handle_INTRINSIC_CALL (stmt);
      } else {
        Expand_Statement( stmt );
      }
      Process_OPs_For_Stmt();
    }
  }
}

/*
 * Top level call for lowering a WHIRL tree to OPs
 * The only hierarchy allowed in the input tree is REGIONs
 */
void 
Convert_WHIRL_To_OPs (WN *tree)
{
  WN *stmt;
  CGRIN *cgrin;
  BB *last_bb;
  INT num_exits, i;
  RID *rid = REGION_get_rid(tree);
  Is_True(rid != NULL, ("Convert_WHIRL_To_OPs, NULL RID"));
  BOOL Trace_BBs = Get_Trace (TP_CGEXP, 512);
  Trace_WhirlToOp = Get_Trace (TP_CGEXP, 2);

  // Initialization; some of this should be PU level others are region level

  if (Trace_WhirlToOp) {
    fprintf(TFile, "%sWHIRL tree input to Convert_WHIRL_To_OPs:\n%s",
	    DBar, DBar);
    fdump_tree_with_freq(TFile, tree, WN_MAP_UNDEFINED);
  }

  initialize_region_stack(tree);
  Cur_BB = NULL;
  current_srcpos = 0;

  switch ( WN_opcode( tree ) ) {
  case OPC_FUNC_ENTRY:
    Compiling_Proper_REGION = FALSE;
    if (RID_cginfo(rid) == NULL) {
      RID_cginfo(rid) = CGRIN_Create(RID_num_exits(rid));
    }
    Handle_Entry( tree );
    stmt = WN_entry_first( tree );
    break;
  case OPC_REGION:
    Compiling_Proper_REGION = TRUE;
    if ( RID_level( rid ) < RL_CG ) {      /* it is WHIRL */
      num_exits = RID_num_exits( rid );
      cgrin = CGRIN_Create( num_exits ); /* creates entry & exit blocks also */
      RID_cginfo( rid ) = cgrin;
      RID_level( rid ) = RL_CG;
      region_stack_push( rid );
      Start_New_Basic_Block();
      CGRIN_entry( cgrin ) = Cur_BB;
      min_bb_id = BB_id( Cur_BB );
      CGRIN_min_bb_id( cgrin ) = min_bb_id;
      BB_rid( Cur_BB ) = rid;
      stmt = WN_first( WN_region_body( tree ) );
    }
    else { /* it is OPs */
      Cur_BB = Append_Region_BBs( Cur_BB, rid );
      if ( CGRIN_first_regular_tn( RID_cginfo( rid ) ) < First_REGION_TN )
	First_REGION_TN = CGRIN_first_regular_tn( RID_cginfo( rid ) );
      if ( CGRIN_first_gtn( RID_cginfo( rid ) ) < First_REGION_GTN )
	First_REGION_GTN = CGRIN_first_gtn( RID_cginfo( rid ) );
      CGRIN_entry( cgrin ) = REGION_First_BB;
      stmt = NULL;
    }
    break;
  default:
    Is_True( FALSE, ("unexpected opcode in Convert_WHIRL_To_OPs") );
    break;
  }

#ifdef TARG_X8664
  // Bug 1509 - reset preamble seen at the beginning of a PU.
  WN_pragma_preamble_end_seen = FALSE;
#endif
  if ( stmt )
    convert_stmt_list_to_OPs( stmt );

  /* If we have any OPs that have not been entered into a basic block,
   * do so now. This can happen if there is no exit for a procedure.
   */
  if (OPS_first(&New_OPs) != NULL) Start_New_Basic_Block ();

  /* On rare occasions we end up with the final BB just falling off
   * the end of the PU. It turns out that through some optimization,
   * this block will never be executed. When this condition occurs,
   * we essentially end up with a broken CFG. Instead of making all
   * consumers have to cope with this case, we just add the missing
   * return. If it truely was correct we should end up removing it.
   */
  if ( ! Compiling_Proper_REGION ) {
    BB *final_bb = Cur_BB;
    if (   BB_length( final_bb ) == 0
	&& ! BB_exit( final_bb )
	&& ! BB_entry( final_bb )
	/* if scope label then okay to back-up, else have to keep block */
	&& (! BB_has_label( final_bb ) || Only_Has_Exc_Label (final_bb) )
    ) {
      final_bb = BB_prev ( final_bb );
    }

    if ( ! BB_exit( final_bb ) ) {
      OP *op = BB_last_op( final_bb );
      if ( op == NULL || OP_cond( op ) ) {
	Handle_Return ();
      }
    }
  }

  /* Because of the way Start_New_Basic_Block works, we almost certainly
   * end up with one extra BB at the end of the chain. Verify that we
   * have an extra BB, and remove it if so. We also determine the
   * last BB for the region code that follows.
   */
  last_bb = Cur_BB;
  if (    ( BB_length( Cur_BB ) == 0 )
       && ( ! BB_has_label( Cur_BB ) )
       && ( ! BB_exit( Cur_BB ) )
  ) {
    last_bb = BB_prev( Cur_BB );
    Remove_BB( Cur_BB );
    if ( BB_id( Cur_BB ) == PU_BB_Count )
      --PU_BB_Count;
  }
  Is_True(last_bb != NULL,("Convert_WHIRL_To_OPs, last_bb is NULL"));

  /* Build the control flow graph */
  Build_CFG();

  switch ( WN_opcode( tree ) ) {
  case OPC_FUNC_ENTRY:
    break;
  case OPC_REGION:
    region_stack_pop();
    rid = REGION_get_rid(tree);
    CGRIN_first_bb( cgrin ) = REGION_First_BB;
    BB_rid( last_bb ) = rid;
    CGRIN_last_bb( cgrin ) = last_bb;
    CGRIN_min_bb_id( cgrin ) = min_bb_id;
    CGRIN_first_regular_tn( RID_cginfo( rid ) ) = First_REGION_TN;
    CGRIN_first_gtn( RID_cginfo( rid ) ) = First_REGION_GTN;
    CGRIN_preg_to_tn_mapping( cgrin ) = NULL;
    CGRIN_tns_in( cgrin ) = NULL;
    if ( RID_pregs_in( rid ) ) {
      Add_PregTNs_To_BB (RID_pregs_in(rid), CGRIN_entry(cgrin),
			 TRUE /*prepend*/);
    }
    num_exits = RID_num_exits(rid);
    if (num_exits > 0) {
      TN_LIST **tno;
      tno = TYPE_MEM_POOL_ALLOC_N( TN_LIST *, &REGION_mem_pool, num_exits );
      CGRIN_tns_out( cgrin ) = tno;
    } else {
      CGRIN_tns_out( cgrin ) = NULL;
    }
    for ( i = 0; i < num_exits; i++ ) {
	FmtAssert(cgrin != NULL && CGRIN_exit_i(cgrin,i),
		  ("missing exit bb for RGN %d, exit %d, cgrin=0x%x",
					  RID_id(rid), i, cgrin));
        Add_PregTNs_To_BB (RID_pregs_out_i(rid,i), CGRIN_exit_i(cgrin,i),
		FALSE /*prepend*/);
    }
    break;
  default:
    #pragma mips_frequency_hint NEVER
    FmtAssert( FALSE, ("unexpected opcode in Convert_WHIRL_To_OPs") );
    /*NOTREACHED*/
  }

  if (Trace_BBs)
    Print_All_BBs ();
}

void Whirl2ops_Initialize(struct ALIAS_MANAGER *alias_mgr)
{
  Alias_Manager = alias_mgr;

  if ( Alias_Manager) {
    OP_to_WN_map = OP_MAP_Create();
    WN_to_OP_map = WN_MAP_Create(&MEM_phase_pool);
  }
  else {
    OP_to_WN_map = NULL;
    WN_to_OP_map = WN_MAP_UNDEFINED;
  }
  last_loop_pragma = NULL;
  OP_Asm_Map = OP_MAP_Create();
  OP_Ld_GOT_2_Sym_Map = OP_MAP_Create();
}

void Whirl2ops_Finalize(void)
{
  /* delete the maps */
  if (predicate_map) {
    OP_MAP_Delete(predicate_map);
    predicate_map = NULL;
  }
  if (OP_to_WN_map) {
    OP_MAP_Delete(OP_to_WN_map);
    OP_to_WN_map = NULL;
  }
  if (WN_to_OP_map != WN_MAP_UNDEFINED) {
    WN_MAP_Delete(WN_to_OP_map);
    WN_to_OP_map = WN_MAP_UNDEFINED;
  }
  if (last_loop_pragma && 
      !WN_pragma_compiler_generated(last_loop_pragma) &&
      CG_opt_level > 1) {
    ErrMsgSrcpos(EC_LNO_Bad_Pragma_String, WN_Get_Linenum(last_loop_pragma),
		 WN_pragmas[WN_pragma(last_loop_pragma)].name,
		 "not followed by a loop, ignored");
  }
  OP_MAP_Delete(OP_Asm_Map);
  OP_MAP_Delete(OP_Ld_GOT_2_Sym_Map); 
}
