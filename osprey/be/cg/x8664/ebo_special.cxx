/*
 *  Copyright (C) 2006, 2007.  QLogic Corporation. All Rights Reserved.
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


/* =======================================================================
 * =======================================================================
 *
 *  Module: ebo_special.cxx
 *  $Revision: 1.245 $
 *  $Date: 05/11/30 16:23:38-08:00 $
 *  $Author: tkong@hyalite.keyresearch $
 *  $Source: be/cg/x8664/SCCS/s.ebo_special.cxx $
 *
 *  Revision comments:
 *
 *  17-June-1998 - Initial version
 *
 *  Description:
 *  ============
 *
 *  EBO special case optimizations.
 *
 * =======================================================================
 * =======================================================================
 */

#ifdef USE_PCH
#include "cg_pch.h"
#endif // USE_PCH
#pragma hdrstop

#ifdef _KEEP_RCS_ID
static const char source_file[] = __FILE__;
#endif /* _KEEP_RCS_ID */

#include <stdarg.h>
#include "defs.h"
#include "config_TARG.h"
#include "errors.h"
#include "mempool.h"
#include "tracing.h"
#include "timing.h"
#include "cgir.h"
#include "tn_map.h"
#include "cg_loop.h"
#include "cg.h"
#include "cgexp.h"
#include "register.h"
#include "cg_region.h"
#include "wn.h"
#include "region_util.h"
#include "op_list.h"
#include "cgprep.h"
#include "gtn_universe.h"
#include "gtn_set.h"
#include "cg_db_op.h"
#include "whirl2ops.h"
#include "cgtarget.h"
#include "gra_live.h"
#include "reg_live.h"
#include "cflow.h"
#include "cg_spill.h"
#include "cgexp_internals.h"
#include "data_layout.h"
#include "stblock.h"
#include "cxx_hash.h"
#include "op.h"

#include "ebo.h"
#include "ebo_info.h"
#include "ebo_special.h"
#include "ebo_util.h"
#include "cgtarget.h"

#include "config_lno.h"

extern BOOL TN_live_out_of( TN*, BB* );


/* Define a macro to strip off any bits outside of the left most 4 bytes. */
#define TRUNC_32(val) (val & 0x00000000ffffffffll)

/* Define a macro to sign-extend the least signficant 32 bits */
#define SEXT_32(val) (((INT64)(val) << 32) >> 32)

/* ===================================================================== */

typedef HASH_TABLE<ST_IDX, INITV_IDX> ST_TO_INITV_MAP;
static ST_TO_INITV_MAP *st_initv_map = NULL;
static BOOL st_initv_map_inited = FALSE;
static GTN_SET *work_gtn_set = NULL;
static BS *work_defined_set = NULL;
static MEM_POOL *work_pool = NULL;
static INT32 fixed_branch_cost, taken_branch_cost;

static BOOL Convert_Imm_And( OP* op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo );
static BOOL Convert_Imm_Mul( OP* op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo );
static BOOL Convert_Imm_Or( OP* op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo );
static BOOL Convert_Imm_Add( OP* op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo );
static BOOL Convert_Imm_Xor( OP* op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo );
static BOOL Convert_Imm_Cmp( OP* op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo );

enum ADDR_MODE { REG_MODE = 0,     /* reg                      */
		 BASE_MODE,	   /* offset(base)             */
		 BASE_INDEX_MODE,  /* offset(base,index,scale) */
		 INDEX_MODE,       /* offset(,index,scale)     */
		 N32_MODE,         /* offset                   */
		 UNDEFINED_MODE	   /* end marker               */
	       };

static void Init_Addr_Modes();
static TOP Get_Top_For_Addr_Mode (TOP, ADDR_MODE);

static OP *Compose_Mem_Op_And_Copy_Info (OP *op, TN *index, TN *offset,
					 TN *scale, TN *base,
					 EBO_TN_INFO **load_actual_tninfo);

/* Initialize and finalize ebo special routines. */
void
EBO_Special_Start (MEM_POOL *pool)
{
#if 0
  st_initv_map = CXX_NEW(ST_TO_INITV_MAP(31, pool), pool);
  st_initv_map_inited = FALSE;
  work_gtn_set = GTN_SET_Create_Empty(Last_TN + 1, pool);
  work_defined_set = BS_Create_Empty(Last_TN + 1, pool);
  work_pool = pool;

  INT32 idummy;
  double ddummy;
  CGTARG_Compute_Branch_Parameters(&idummy, &fixed_branch_cost,
				   &taken_branch_cost, &ddummy);
#endif

  Init_Addr_Modes();
}

void
EBO_Special_Finish (void)
{
  st_initv_map = NULL;
  st_initv_map_inited = FALSE;
  work_gtn_set = NULL;
  work_defined_set = NULL;
  work_pool = NULL;
}


/*
 * Identify OP's that contain a constant and operate in a way that
 * will allow the constant to be added into an offset field of
 * a load or store instruction.
 */
BOOL EBO_Can_Merge_Into_Offset (OP *op)
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_CAN_MERGE_INTO_OFFSET)) return FALSE;
#endif
  TOP top = OP_code(op);
  
  if( !OP_iadd( op ) && !OP_isub( op ) )
    return FALSE;

  if ((op == BB_entry_sp_adj_op(OP_bb(op))) ||
      (op == BB_exit_sp_adj_op(OP_bb(op))))
    return FALSE;

  /* opnd0 will serve as a base/index register. Check its size first,
     because a negative 32-value value could be a big positive value
     in a 64-bit reg; but it is OK if it is a positive 32-bit value.
   */
  if( MTYPE_is_size_double( Pointer_Mtype ) &&
      TN_size( OP_opnd(op,0) ) == 4 )
    return FALSE;

  TN *tn = OP_opnd(op,1);
  if (TN_Is_Constant(tn))
    return TRUE;

  return FALSE;
}


static
void
EBO_Set_OP_omega (OP *op, ...)
{
  INT opnds = OP_opnds(op);
  INT i;
  va_list tninfos;

  va_start(tninfos, op);
  CG_LOOP_Init_Op(op);
  for (i = 0; i < opnds; i++) {
    EBO_TN_INFO *tninfo = va_arg(tninfos, EBO_TN_INFO *);
    Set_OP_omega (op, i, ((tninfo != NULL) ? tninfo->omega : 0));
  }

  va_end(tninfos);
  return;
}



static
void
EBO_Copy_OP_omega (OP *new_op, OP *old_op)
{
  INT opnds = OP_opnds(new_op);
  INT i;

  CG_LOOP_Init_Op(new_op);
  for (i = 0; i < opnds; i++) {
    Set_OP_omega (new_op, i, OP_omega(old_op,i));
  }

  return;
}


static
void
EBO_OPS_omega (OPS *ops)
{
  OP *next_op = OPS_first(ops);
  while (next_op != NULL) {
    INT opnds = OP_opnds(next_op);
    INT i;

    CG_LOOP_Init_Op(next_op);
    for (i = 0; i < opnds; i++) {
      Set_OP_omega (next_op, i, 0);
    }

    next_op = OP_next(next_op);
  }

  return;
}


BOOL Combine_L1_L2_Prefetches( OP* op, TN** opnd_tn, EBO_TN_INFO** opnd_tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_COMBINE_L1_L2_PREFETCH)) return FALSE;
#endif
  if( !OP_prefetch( op ) )
    return FALSE;

  /* Determine the proper hash value. */
  const int hash_value = EBO_hash_op( op, opnd_tninfo );

  if( EBO_Trace_Hash_Search ){
    #pragma mips_frequency_hint NEVER
    fprintf( TFile,"%sLook for redundant prefetch ops in hash chain %d for\n\t",
	     EBO_trace_pfx,hash_value );
    Print_OP_No_SrcLine(op);
  }

  BOOL replaced = FALSE;
  EBO_OP_INFO* opinfo = EBO_opinfo_table[hash_value];

  while( opinfo != NULL ){

    OP* pred_op = opinfo->in_op;
    if( pred_op == NULL       ||
	!OP_prefetch(pred_op) ||
	OP_opnds(pred_op) != OP_opnds(op) ){
      opinfo = opinfo->same;
      continue;
    }

    // Compare the addresses
    int i = 1;
    for( ; i < OP_opnds(op); i++ ){
      if( OP_opnd( op, i ) != OP_opnd( pred_op, i ) )
	break;
    }
    
    if( i < OP_opnds(op) ){
      opinfo = opinfo->same;
      continue;
    }

    // Compare the hint field
    const ISA_ENUM_CLASS_VALUE pfhint0 = TN_enum( OP_opnd(op,0) );
    const ISA_ENUM_CLASS_VALUE pfhint1 = TN_enum( OP_opnd(pred_op,0) );

    // Remove an identical prefetch op.
    if( pfhint0 == pfhint1 ){
      replaced = TRUE;
      break;
    }

    // Remove a dominated prefetch.
    if( pfhint0 == ECV_pfhint_L1_L2_load &&
	( pfhint1 == ECV_pfhint_L1_load ||
	  pfhint1 == ECV_pfhint_L2_load ) ){
      // pred_op is dominated by the current op.
      remove_op( opinfo );
      OP_Change_To_Noop( opinfo->in_op );
      opinfo->in_op = NULL;
      opinfo->in_bb = NULL;

      break;
    }

    if( pfhint1 == ECV_pfhint_L1_L2_load &&
	( pfhint0 == ECV_pfhint_L1_load ||
	  pfhint0 == ECV_pfhint_L2_load ) ){
      replaced = TRUE;
      break;
    }    

    // Combine L1 & L2 prefetches into an L1_L2 prefetch.
    if( ( pfhint0 == ECV_pfhint_L1_load &&
	  pfhint1 == ECV_pfhint_L2_load ) ||
	( pfhint0 == ECV_pfhint_L2_load &&
	  pfhint1 == ECV_pfhint_L1_load ) ){
      ;
    } else {
      opinfo = opinfo->same;
      continue;
    }

    OP* new_op = Dup_OP( op );
    Set_OP_opnd( new_op, 0 , Gen_Enum_TN( ECV_pfhint_L1_L2_load ) );

    TOP new_top = TOP_prefetcht0;
    if( OP_find_opnd_use( op, OU_base ) < 0 )
      new_top = TOP_prefetcht0xx;
    else if( OP_find_opnd_use( op, OU_index ) >= 0 )
      new_top = TOP_prefetcht0x;

    OP_Change_Opcode( new_op, new_top );

    if( EBO_Trace_Data_Flow ){
      fprintf( TFile, "%sReplace L1 and L2 prefetch OPs ",EBO_trace_pfx );
      Print_OP_No_SrcLine(pred_op);
      Print_OP_No_SrcLine(op);
      fprintf( TFile,"%swith   ",EBO_trace_pfx );
      Print_OP_No_SrcLine(new_op);
    }

    Copy_WN_For_Memory_OP( new_op, op);
    if ( OP_volatile( op ) )
      Set_OP_volatile( new_op );
    OP_srcpos( new_op ) = OP_srcpos( op );

    if( EBO_in_loop ){
      EBO_Copy_OP_omega( new_op, op );
    }

    BB_Insert_Op_After( OP_bb(op), op, new_op );
    replaced = TRUE;

    remove_op( opinfo );
    OP_Change_To_Noop(opinfo->in_op);
    opinfo->in_op = NULL;
    opinfo->in_bb = NULL;

    break;
  }

  return replaced;
}


BOOL
combine_adjacent_loads(OP *op,
                       EBO_TN_INFO **opnd_tninfo,
		       EBO_OP_INFO *opinfo,
                       INT64 offset_pred,
                       INT64 offset_succ)
{
  // TODO
  return FALSE;
}

static void
Expand_Extract_Bits (TYPE_ID rtype, TYPE_ID desc, UINT bit_offset, 
		     UINT bit_size,
		     TN *tgt_tn, TN *src_tn, OPS *ops)
{
  BOOL is_double = MTYPE_is_size_double(rtype);
  FmtAssert(MTYPE_bit_size(rtype) == MTYPE_bit_size(desc), 
	    ("Expand_Extract_Bits: Handle this case (1)")); 
  UINT pos = (Target_Byte_Sex != Host_Byte_Sex)
             ? MTYPE_bit_size(desc)-bit_offset-bit_size : bit_offset;
  if (pos == 0 && bit_size <= 16 && ! MTYPE_signed(rtype)) {
    Build_OP(is_double?TOP_andi64:TOP_andi32, tgt_tn, src_tn, 
	     Gen_Literal_TN((1 << bit_size)-1, is_double?8:4), ops);
    return;
  }

  TN* tmp1_tn = EBO_in_peep
    ? Build_Dedicated_TN (TN_register_class(tgt_tn), 
			  TN_register(tgt_tn), TN_size(tgt_tn))
    : Build_TN_Of_Mtype(rtype);

  if (EBO_in_peep) {
    // after LRA, EBO needs to take care of x86 fixup.
    Exp_COPY(tmp1_tn, src_tn, ops);
    src_tn = tmp1_tn;
  }
  ISA_REGISTER_CLASS rclass = ISA_REGISTER_CLASS_integer;
  INT reg_size = is_double ? 64 : 32;
    //ISA_REGISTER_CLASS_INFO_Bit_Size(ISA_REGISTER_CLASS_Info(rclass));
  TOP left_shift_op = is_double?TOP_shli64:TOP_shli32;
  INT left_shift_amt = reg_size - pos - bit_size;

  if( left_shift_amt == 0 ){
    Build_OP( MTYPE_bit_size(desc) == 64 ? TOP_mov64 : TOP_mov32,
	      tmp1_tn, src_tn, ops );

  } else if (left_shift_amt > 0){
    Build_OP(left_shift_op, tmp1_tn, src_tn, Gen_Literal_TN(left_shift_amt, 4),
	     ops);
  }

  TOP right_shift_op = is_double?TOP_sari64:TOP_sari32;
  INT right_shift_amt = reg_size - bit_size;
  if (! MTYPE_signed(rtype))
    right_shift_op = is_double ? TOP_shri64: TOP_shri32;

  if( right_shift_amt == 0 ){
    Build_OP( MTYPE_bit_size(desc) == 64 ? TOP_mov64 : TOP_mov32,
	      tgt_tn, tmp1_tn, ops );

  } else if (right_shift_amt > 0){
    Build_OP(right_shift_op, tgt_tn, tmp1_tn, 
	     Gen_Literal_TN(right_shift_amt, 4),
	     ops);
  }

  else if (left_shift_amt < 0 && right_shift_amt < 0) {
    if (left_shift_amt < right_shift_amt)
      Build_OP(right_shift_op, tgt_tn, src_tn, 
	       Gen_Literal_TN(right_shift_amt-left_shift_amt, 4),
	       ops);
    else
      Build_OP(left_shift_op, tgt_tn, src_tn, 
	       Gen_Literal_TN(left_shift_amt-right_shift_amt, 4),
	       ops);      
  } else
    FmtAssert( FALSE, ("Expand_Extract_Bits: Handle this case (2)")); 
}

struct SIZE_EXT_INFO {
  mUINT8 src_size;  // measured in bytes
  mUINT8 dest_size; // measured in bytes
  bool sign_ext;
};

#define SET_SIZE_EXT_INFO(o,s,d,sign) \
do {                                  \
  (o)->src_size = s;                  \
  (o)->dest_size = d;                 \
  (o)->sign_ext = sign;               \
} while(0)


static void Get_Size_Ext_Info( TOP top, SIZE_EXT_INFO* info )
{
  switch( top ){
  case TOP_movsbl:  SET_SIZE_EXT_INFO( info, 1, 4, true );   break;
  case TOP_movzbl:  SET_SIZE_EXT_INFO( info, 1, 4, false );  break;
  case TOP_movswl:  SET_SIZE_EXT_INFO( info, 2, 4, true );   break;
  case TOP_movzwl:  SET_SIZE_EXT_INFO( info, 2, 4, false );  break;
  case TOP_movsbq:  SET_SIZE_EXT_INFO( info, 1, 8, true );   break;
  case TOP_movzbq:  SET_SIZE_EXT_INFO( info, 1, 8, false );  break;
  case TOP_movswq:  SET_SIZE_EXT_INFO( info, 2, 8, true );   break;
  case TOP_movzwq:  SET_SIZE_EXT_INFO( info, 2, 8, false );  break;
  case TOP_movslq:  SET_SIZE_EXT_INFO( info, 4, 8, true );   break;
  case TOP_movzlq:  SET_SIZE_EXT_INFO( info, 4, 8, false );  break;

  case TOP_ld32_n32:
  case TOP_ld32:
  case TOP_ldx32:
  case TOP_ldxx32:
  case TOP_ld32_m:
    SET_SIZE_EXT_INFO( info, 4, 4, false );
    break;
  case TOP_ld64:
  case TOP_ldx64:
  case TOP_ldxx64:
  case TOP_ld64_m:
    SET_SIZE_EXT_INFO( info, 8, 8, false );
    break;
  case TOP_ld8_32_n32:
  case TOP_ld8_32:
  case TOP_ldx8_32:
  case TOP_ldxx8_32:
    SET_SIZE_EXT_INFO( info, 1, 4, true );
    break;
  case TOP_ldu8_32_n32:
  case TOP_ldu8_32:
  case TOP_ldxu8_32:
  case TOP_ldxxu8_32:
    SET_SIZE_EXT_INFO( info, 1, 4, false );
    break;
  case TOP_ld8_m:
    SET_SIZE_EXT_INFO( info, 1, 1, false );
    break;
  case TOP_ld16_32_n32:
  case TOP_ld16_32:
  case TOP_ldx16_32:
  case TOP_ldxx16_32:
    SET_SIZE_EXT_INFO( info, 2, 4, true );
    break;
  case TOP_ldu16_32_n32:
  case TOP_ldu16_32:
  case TOP_ldxu16_32:
  case TOP_ldxxu16_32:
    SET_SIZE_EXT_INFO( info, 2, 4, false );
    break;
  case TOP_ld16_m:
    SET_SIZE_EXT_INFO( info, 2, 2, false );
    break;
  case TOP_ld8_64:
  case TOP_ldx8_64:
  case TOP_ldxx8_64:
    SET_SIZE_EXT_INFO( info, 1, 8, true );
    break;
  case TOP_ldu8_64:
  case TOP_ldxu8_64:
  case TOP_ldxxu8_64:
    SET_SIZE_EXT_INFO( info, 1, 8, false );
    break;
  case TOP_ld16_64:
  case TOP_ldx16_64:
  case TOP_ldxx16_64:
    SET_SIZE_EXT_INFO( info, 2, 8, true );
    break;
  case TOP_ldu16_64:
  case TOP_ldxu16_64:
  case TOP_ldxxu16_64:
    SET_SIZE_EXT_INFO( info, 2, 8, false );
    break;
  case TOP_ld32_64:
  case TOP_ldx32_64:
  case TOP_ldxx32_64:
    SET_SIZE_EXT_INFO( info, 4, 8, true );
    break;

  default:
    FmtAssert( FALSE, ("Get_Size_Ext_Info: NYI") );
  }

  return;
}


BOOL
delete_subset_mem_op(OP *op,
                     EBO_TN_INFO **opnd_tninfo,
		     EBO_OP_INFO *opinfo,
                     INT64 offset_pred,
                     INT64 offset_succ)
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_DELETE_SUBSET_MEM_OP)) return FALSE;
#endif
  OP *pred_op = opinfo->in_op;
  BB *bb = OP_bb(op);
  INT opcount = OP_opnds(op);
  TN *pred_result = OP_store(pred_op) 
    ? OP_opnd(pred_op, OP_find_opnd_use(pred_op, OU_storeval))
    : OP_result(pred_op,0);
  TN *succ_result = OP_store(op)
    ? OP_opnd(op, OP_find_opnd_use(op,OU_storeval))
    : OP_result(op,0);
  BOOL succ_is_subset = FALSE;
  INT byte_offset;
  OPS ops = OPS_EMPTY;

  if (EBO_Trace_Data_Flow) {
    fprintf(TFile,"%ssubset    OP in BB:%d    ",EBO_trace_pfx,BB_id(bb));
    Print_OP_No_SrcLine(op);
    fprintf(TFile,"      Matches   OP in BB:%d    ",BB_id(opinfo->in_bb));
    Print_OP_No_SrcLine(pred_op);
  }
  
  if ((Opt_Level < 2) && (bb != opinfo->in_bb)) {
   /* Global TN's aren't supported at low levels of optimization. */
    return FALSE;
  }

  if (!EBO_in_peep &&
      (bb != opinfo->in_bb) &&
      !TN_Is_Constant(pred_result) &&
      has_assigned_reg(pred_result)) {
    if (EBO_Trace_Data_Flow) {
      #pragma mips_frequency_hint NEVER
      fprintf(TFile,"%sShouldn't move dedicated register references across blocks.\n",
              EBO_trace_pfx);
    }
    return FALSE;
  }

  const INT size_pred = CGTARG_Mem_Ref_Bytes(pred_op);
  const INT size_succ = CGTARG_Mem_Ref_Bytes(op);
  if (size_pred < size_succ) return FALSE;

  if ((offset_pred <= offset_succ) &&
      ((offset_pred + size_pred) >= (offset_succ + size_succ))) {
    succ_is_subset = TRUE;
  }

  if (!succ_is_subset) {
    return FALSE;
  }

  byte_offset = offset_succ - offset_pred;

  if (!OP_store(pred_op) || !OP_load(op)) {
   /* Can only optimize Store - Load pattern. */
    return FALSE;
  }

  if (byte_offset > 8) return FALSE;

  if ((TN_register_class(pred_result) != ISA_REGISTER_CLASS_integer) ||
      (TN_register_class(succ_result) != ISA_REGISTER_CLASS_integer)) {
   /* Can only play games with integers. */
    return FALSE;
  }

  if( size_succ == 1 &&
      EBO_in_peep ){
    const REGISTER reg = TN_register(pred_result);
    const REGISTER_SET regs =
      REGISTER_CLASS_eight_bit_regs(TN_register_class(pred_result));
    if( !REGISTER_SET_MemberP( regs, reg ) )
      return FALSE;
  }

  if (EBO_Trace_Optimization) {
#pragma mips_frequency_hint NEVER
    fprintf(TFile,"%sReplace subset load with left/right shifts\n",EBO_trace_pfx);
  }   

  if (offset_succ == offset_pred &&
      size_pred == size_succ) {
    TOP top = OP_code(op);
    if (TOP_is_load_ext(top) &&
	top != OP_code(pred_op))
      // The load needs sign/zero extension
      Exp_COPY_Ext(top, OP_result(op, 0), OP_opnd(pred_op,0), &ops );
    else
      Exp_COPY( OP_result(op,0), OP_opnd(pred_op,0), &ops );
    OP_srcpos(OPS_first(&ops)) = OP_srcpos(op);
    BB_Insert_Ops_After(OP_bb(op), op, &ops);
    return TRUE;
  }    

  /* Since a load will be converted to shifting ops later, and the new shifting
     op will affect the rflags. Thus, make a check first to avoid ruinning the
     rflags.
  */
  for( OP* next_op = OP_next(op); next_op != NULL; next_op = OP_next( next_op ) ){
    if( OP_reads_rflags( next_op ) )
      return FALSE;

    if( TOP_is_change_rflags( OP_code(next_op) ) )
      break;
  }

  INT bit_size = size_succ*8;
  INT succ_offset = (offset_succ - offset_pred);
  INT bit_offset = succ_offset*8;
  TYPE_ID rtype = MTYPE_UNKNOWN, desc = MTYPE_UNKNOWN;

  if( size_pred == 1 )
    desc = MTYPE_I1;
  else if( size_pred == 2 )
    desc = MTYPE_I2;    
  else if( size_pred == 4 )
    desc = MTYPE_I4;
  else if( size_pred == 8 )
    desc = MTYPE_I8;
  else
    FmtAssert( false, ("delete_subset_mem_op: NYI (1)"));

  struct SIZE_EXT_INFO op_size_ext_info;
  Get_Size_Ext_Info( OP_code(op), &op_size_ext_info );
  const BOOL unsigned_op = !op_size_ext_info.sign_ext;

  if (desc == MTYPE_I1) {
    if (unsigned_op)
      rtype = MTYPE_U1;
    else
      rtype = desc;
  } else if (desc == MTYPE_I2) {
    if (unsigned_op)
      rtype = MTYPE_U2;
    else
      rtype = desc;
  } else if (desc == MTYPE_I4) {
    if (unsigned_op)
      rtype = MTYPE_U4;
    else
      rtype = desc;
  } else {
    if (unsigned_op)
      rtype = MTYPE_U8;
    else
      rtype = desc;
  }

  Expand_Extract_Bits (rtype, desc, 
		       bit_offset, bit_size,
		       OP_result(op, 0), OP_opnd(pred_op, 0), &ops);

  OP_srcpos(OPS_first(&ops)) = OP_srcpos(op);
  BB_Insert_Ops_After(OP_bb(op), op, &ops);
  return TRUE;
}

/* 
 * delete_reload_across_dependency
 *
 * For a given load or store and one it matches,
 * attempt to replace one of them.
 * Return TRUE if this op is no longer needed.
 */
BOOL
delete_reload_across_dependency (OP *op,
                                 EBO_TN_INFO **opnd_tninfo,
		                 EBO_OP_INFO *opinfo,
		                 EBO_OP_INFO *intervening_opinfo)
{
  return FALSE;

  BB *bb = OP_bb(op);
  OPS ops = OPS_EMPTY;
  OP *pred_op = opinfo->in_op;
  OP *intervening_op = intervening_opinfo->in_op;
  TOP pred_opcode = OP_code(pred_op);
  TOP intervening_opcode = OP_code(intervening_op);
  INT pred_base_idx = OP_find_opnd_use(pred_op, OU_base);
  INT intervening_base_idx =  
    OP_find_opnd_use(intervening_op, OU_base);
  INT size_pred;
  INT size_succ;
  INT size_intervening;

  TN *pred_result;
  TN *intervening_result;
  TN *pred_index;
  TN *intervening_index;
  TN *predicate1;
  TN *predicate2;

 /* We can't assign registers, so don't optimize if it's already been done. */
  if (EBO_in_loop) return FALSE;
  if (EBO_in_peep) return FALSE;

  if (EBO_Trace_Execution) {
    #pragma mips_frequency_hint NEVER
    fprintf(TFile,"%sEnter delete_reload_across_dependency.\n",EBO_trace_pfx);
    Print_OP_No_SrcLine(pred_op);
    Print_OP_No_SrcLine(intervening_op);
    Print_OP_No_SrcLine(op);
  }

 /* Be sure we have a "Store .. Store .. Load" pattern. */
  if ((pred_op == NULL) ||
      (intervening_op == NULL) ||
      !OP_load(op) ||
      !(OP_load(pred_op) || OP_store(pred_op)) ||
      !OP_store(intervening_op)) return FALSE;

  pred_op = opinfo->in_op;
  intervening_op = intervening_opinfo->in_op;

  if (OP_prefetch(op) ||
      OP_prefetch(pred_op) ||
      OP_prefetch(intervening_op)) return FALSE;

  if (OP_unalign_mem(op) ||
      OP_unalign_mem(pred_op) ||
      OP_unalign_mem(intervening_op)) return FALSE;

  size_succ = CGTARG_Mem_Ref_Bytes(op);
  size_pred = CGTARG_Mem_Ref_Bytes(pred_op);
  size_intervening = CGTARG_Mem_Ref_Bytes(intervening_op);

  if ((size_succ != size_pred) ||
      (size_succ != size_intervening)) return FALSE;

 /* Capture the values in the preceeding memory OPs. */
  pred_result = OP_store(pred_op) ? 
    OP_opnd(pred_op, OP_find_opnd_use(pred_op, OU_storeval))
    : OP_result(pred_op,0);
  intervening_result = OP_opnd(intervening_op,
                               OP_find_opnd_use(intervening_op,OU_storeval));

  if ((TN_register_class(intervening_result) != 
       TN_register_class(pred_result)) ||
      (TN_register_class(intervening_result) != 
       TN_register_class(OP_result(op,0)))) {
    if (EBO_Trace_Data_Flow) {
      #pragma mips_frequency_hint NEVER
      fprintf(TFile,"%sInter-register copies are not supported\n",
              EBO_trace_pfx);
    }
    return FALSE;
  }

  if (TNs_Are_Equivalent( pred_result, intervening_result)) {
    /* It doesn't matter if the addresses are the same or different
       because the value will always be the same! Just Copy the value. */
    Is_True( false, ("delete_reload_across_dependency: NYI (1)"));
    return TRUE;
  }

 /* We need to compare the addresses BEFORE they were incremented.
    If both OPs are incremented by the same value, we can compare
    the addresses AFTER the increment.
 */
  pred_index = OP_opnd(pred_op, pred_base_idx);
  intervening_index = OP_opnd(intervening_op, intervening_base_idx);

 /* Are the index's available from each store? */
  if (!EBO_tn_available (bb, opinfo->actual_opnd[pred_base_idx]) ||
      !EBO_tn_available (bb, intervening_opinfo->actual_opnd[intervening_base_idx])) {
    if (EBO_Trace_Data_Flow) {
      #pragma mips_frequency_hint NEVER
      fprintf(TFile,"%sBase address not available for compare\n",
              EBO_trace_pfx);
    }
    return FALSE;
  }

  /* Compare the reload address with the intervening store address.
     Select the stored value if the address are the same,
     and resuse the predecesor value if they are not the same.
  */

  return TRUE;
}

// Optimize store-load sequence.  STORE_OPINFO specifies the store.  LOAD_OP is
// the load, with LOAD_ACTUAL_TNINFO and LOAD_OPND_TNINFO describing the actual
// and optimal operands, respectively.  Both the store and the load access the
// same memory location.
static BOOL
Special_Store_Load_Sequence (OP *load_op,
			     EBO_TN_INFO **load_actual_tninfo,
			     EBO_TN_INFO **load_opnd_tninfo,
			     EBO_OP_INFO *store_opinfo)
{
  Is_True(OP_store(store_opinfo->in_op) && OP_load(load_op),
	  ("Special_Store_Load_Sequence: not store-load sequence"));

  INT storeval_idx = OP_find_opnd_use(store_opinfo->in_op, OU_storeval);
  Is_True(storeval_idx >= 0,
	  ("Special_Store_Load_Sequence: invalid storeval index"));
  TN *storeval_tn = OP_opnd(store_opinfo->in_op, storeval_idx);

  // Replace:			; bug 7602
  //   xmm     = load mem1	; first_load_op
  //   mem2    = store xmm	; store_op
  //   int_reg = load mem2	; load_op
  // with:
  //   xmm     = load mem1
  //   mem2    = store xmm
  //   int_reg = load mem1	; change to load from mem1
  if (TN_register_class(OP_result(load_op,0)) == ISA_REGISTER_CLASS_integer &&
      TN_register_class(storeval_tn) == ISA_REGISTER_CLASS_float &&
      TN_size(OP_result(load_op, 0)) == TN_size(storeval_tn)) {	// bug 11321
    OP *store_op = store_opinfo->in_op;
    EBO_TN_INFO *storeval_tninfo = store_opinfo->actual_opnd[storeval_idx];
    if (storeval_tninfo != NULL &&
    	storeval_tninfo->in_op != NULL &&
	OP_load(storeval_tninfo->in_op)) {
      INT i;
      OP *first_load_op = storeval_tninfo->in_op;
      EBO_OP_INFO *first_load_opinfo = storeval_tninfo->in_opinfo;
      Is_True(first_load_opinfo != NULL,
	      ("Special_Store_Load_Sequence: opinfo NULL"));

      // Get first_load_op's base/index/offset/scale.  See if its base/index
      // TNs are available at load_op.
      TN *base_tn = NULL;
      TN *index_tn = NULL;
      TN *offset_tn = NULL;
      TN *scale_tn = NULL;

      INT base_idx = TOP_Find_Operand_Use(OP_code(first_load_op), OU_base);
      INT index_idx = TOP_Find_Operand_Use(OP_code(first_load_op), OU_index);
      INT offset_idx = TOP_Find_Operand_Use(OP_code(first_load_op),OU_offset);
      INT scale_idx = TOP_Find_Operand_Use(OP_code(first_load_op), OU_scale);

      EBO_TN_INFO *base_tninfo = NULL, *index_tninfo = NULL;

      if (base_idx >= 0) {	// base
	base_tn = OP_opnd(first_load_op, base_idx);
	base_tninfo = first_load_opinfo->actual_opnd[base_idx];
	if (base_tninfo == NULL ||
	    !EBO_tn_available(OP_bb(load_op), base_tninfo))
	  return FALSE;
      }
      if (index_idx >= 0) {	// index
	index_tn = OP_opnd(first_load_op, index_idx);
	index_tninfo = first_load_opinfo->actual_opnd[index_idx];
	if (index_tninfo == NULL ||
	    !EBO_tn_available(OP_bb(load_op), index_tninfo))
	  return FALSE;
      }
      offset_tn = offset_idx >= 0 ? OP_opnd(first_load_op, offset_idx) : NULL;
      scale_tn = scale_idx >= 0 ? OP_opnd(first_load_op, scale_idx) : NULL;

      // Check for aliased stores to mem1 occuring after first_load_op.
      EBO_OP_INFO *opinfo = EBO_opinfo_table[first_load_opinfo->hash_index];
      Is_True(opinfo != NULL,
	      ("Special_Store_Load_Sequence: OP hash table empty"));
      for ( ; opinfo != first_load_opinfo; opinfo = opinfo->same) {
	if (opinfo->in_op &&	// opinfo->in_op is NULL if OP is deleted
	    OP_store(opinfo->in_op) &&
	    opinfo->in_op != store_op) {
	  return FALSE;		// Potential alias store.
	}
      }

      // If load_op already loads from mem1, then don't replace load_op with
      // another copy of itself.
      {
	INT load_op_base_idx = TOP_Find_Operand_Use(OP_code(first_load_op),
						    OU_base);
	INT load_op_index_idx = TOP_Find_Operand_Use(OP_code(first_load_op),
						     OU_index);
	INT load_op_offset_idx = TOP_Find_Operand_Use(OP_code(first_load_op),
						      OU_offset);
	INT load_op_scale_idx = TOP_Find_Operand_Use(OP_code(first_load_op),
						     OU_scale);
	TN *load_op_base_tn = load_op_base_idx >= 0 ?
				OP_opnd(load_op, base_idx) : NULL;
	TN *load_op_index_tn = load_op_index_idx >= 0 ?
				 OP_opnd(load_op, index_idx) : NULL;
	TN *load_op_offset_tn = load_op_offset_idx >= 0 ?
				  OP_opnd(load_op, offset_idx) : NULL;
	TN *load_op_scale_tn = load_op_scale_idx >= 0 ?
				 OP_opnd(load_op, scale_idx) : NULL;

	if (load_op_base_tn == base_tn &&
	    load_op_index_tn == index_tn &&
	    (load_op_offset_tn == offset_tn ||
	     (TN_has_value(load_op_offset_tn) && TN_has_value(offset_tn) &&
	      TN_value(load_op_offset_tn) == TN_value(offset_tn))) &&
	    (load_op_scale_tn == scale_tn ||
	     (TN_has_value(load_op_scale_tn) && TN_has_value(scale_tn) &&
	      TN_value(load_op_scale_tn) == TN_value(scale_tn)))) {
	  return FALSE;
	}
      }

      // OK to change from load mem1 to load mem2.
      OP *new_op = Compose_Mem_Op_And_Copy_Info(load_op, index_tn, offset_tn,
						scale_tn, base_tn,
						load_actual_tninfo);

      BB_Insert_Op_After(OP_bb(load_op), load_op, new_op);

      // Increment the reference counts for first_load_op's base/index TNs.
      if (base_tninfo != NULL)
	inc_ref_count(base_tninfo);
      if (index_tninfo != NULL)
	inc_ref_count(index_tninfo);

      // Decrement the reference counts for load_op's base/index TNs.
      INT idx;
      idx = TOP_Find_Operand_Use(OP_code(load_op), OU_base);
      if (idx >= 0 &&
	  load_actual_tninfo[idx] != NULL) {
	dec_ref_count(load_actual_tninfo[idx]);
      }

      idx = TOP_Find_Operand_Use(OP_code(load_op), OU_index);
      if (idx >= 0 &&
	  load_actual_tninfo[idx] != NULL) {
	dec_ref_count(load_actual_tninfo[idx]);
      }

      return TRUE;
    }
  }
  return FALSE;
}


/* 
 * delete_memory_op
 *
 * For a given load or store and one it matches,
 * attempt to replace one of them.
 * Return TRUE if this op is no longer needed.
 */
static
BOOL
delete_memory_op (OP *op,
                  EBO_TN_INFO **actual_tninfo,
                  EBO_TN_INFO **opnd_tninfo,
		  EBO_OP_INFO *opinfo)
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_DELETE_MEMORY_OP)) return FALSE;
#endif
  OPS ops = OPS_EMPTY;
  INT size_pred;
  INT size_succ;

  /* In each case below, before attempting to remove a load, store, or
     prefetch, we must make sure the instruction does not side-effect
     any state, etc. If it does, we can't remove it. This check is
     needed in addition to our general mechanism of making all state
     appear live on exit to the function. */

  ISA_REGISTER_CLASS state_rc = ISA_REGISTER_CLASS_UNDEFINED;
  if (state_rc != ISA_REGISTER_CLASS_UNDEFINED)
  {
    INT results = OP_results(op);
    for (UINT i = 0; i < results; i++)
    {
      TN *tn = OP_result(op, i);
      if (TN_is_register(tn) && (TN_register_class(tn) == state_rc))
	return FALSE;
    }
  }
  
  /* Remove the second OP for:
     Prefetch - Prefetch,
     Load - Prefetch,
     Store - Prefetch
  */
  if (OP_prefetch(op))
  {
    if (EBO_Trace_Optimization) {
      fprintf(TFile,"%sRemove following Prefetch combination\n",EBO_trace_pfx);
    }

    return TRUE;
  }

  /* Don't optimize:
     Prefetch - Load,
     Prefetch - Store,
  */
  if (OP_prefetch(opinfo->in_op))
  {
    return FALSE;
  }

  /* Don't try to optimize unaligned or unknown accesses. */
  if (OP_unalign_mem(op) || OP_unalign_mem(opinfo->in_op))
    return FALSE;

  size_pred = CGTARG_Mem_Ref_Bytes(opinfo->in_op);
  size_succ = CGTARG_Mem_Ref_Bytes(op);

  /* Replace the result tn of the second OP for:
     Load - Load,
  */
  if (OP_load(op) && OP_load(opinfo->in_op))
  {
    /* Make sure the result TNs' regclasses and ops match. */

    if (TN_register_class(OP_result(op,0)) !=
	TN_register_class(OP_result(opinfo->in_op, 0)))
    {
      if (EBO_Trace_Optimization)
	fprintf(TFile,"%sRegclass mismatch for Load - Load combination\n",
		EBO_trace_pfx);

      return FALSE;
    }

    /* This check should be moved after the subset check below, since
       here currently prevents us reaching the subsetting... (but too
       close to release to do it now...). */
    if (OP_code(op) != OP_code(opinfo->in_op))
    {
      if (EBO_Trace_Optimization)
	fprintf(TFile,"%sMtype mismatch for Load - Load combination\n",
		EBO_trace_pfx);

      return FALSE;
    }

    /* If the size of the data item loaded by the two loads is
       different, but the starting memory address is the same.
       There is a chance that the predecessor load is a wider load
       and that the new load's data can be extracted.

       Notice that while performing 32-bit operations with a GPR result in
       64-bit mode, opteron will zero-extend the 32-bit result.
    */
    
    if ((size_pred != size_succ) ||
	(OP_results(op) != OP_results(opinfo->in_op)) ||
	( TN_size(OP_result(opinfo->in_op, 0)) != TN_size(OP_result(op, 0)) &&
	  ( size_pred < 4 || size_succ < 4 ) ) )
    {
      if (EBO_Trace_Optimization)
      {
	fprintf(TFile,"%sSize mismatch for Load - Load combination: %d:%d %d:%d \n",
		EBO_trace_pfx,size_pred,size_succ,
		TN_size(OP_result(opinfo->in_op, 0)),TN_size(OP_result(op, 0)));
      }

      return delete_subset_mem_op (op, opnd_tninfo, opinfo, 0, 0);
    }
    
    if (!EBO_in_peep &&
	(OP_bb(op) != OP_bb(opinfo->in_op)) &&
	!TN_Is_Constant(OP_result(opinfo->in_op, 0)) &&
	has_assigned_reg(OP_result(opinfo->in_op, 0)))
    {
      if (EBO_Trace_Data_Flow)
      {
	fprintf(TFile,"%sShouldn't move dedicated register references across blocks.\n",
		EBO_trace_pfx);
      }
	
      return FALSE;
    }

    /* Remove the second load, and replace it with a copy of the first */
    
    if (EBO_Trace_Optimization)
    {
      fprintf(TFile,"%sRemove Load - Load combination\n",EBO_trace_pfx);
    }

    TOP top = OP_code(op);
    if (TOP_is_load_ext(top) &&
	top != OP_code(opinfo->in_op)) {
      // The load needs sign/zero extension
      for (UINT i = 0; i < OP_results(op); i++)
	Exp_COPY_Ext(top, OP_result(op, i), 
			 OP_result(opinfo->in_op, i), &ops);
    } else {
      for (UINT i = 0; i < OP_results(op); i++)
	EBO_Exp_COPY(NULL, OP_result(op, i), 
		     OP_result(opinfo->in_op, i), &ops);
    }
    
    if (EBO_in_loop)
      EBO_OPS_omega (&ops);
    
    BB_Insert_Ops(OP_bb(op), op, &ops, FALSE);
    return TRUE;
  }
  /* Replace the result tn of the second OP for:
     Store - Load
  */
  else if (OP_load(op) && OP_store(opinfo->in_op))
  {
    if( OP_side_effects( opinfo->in_op ) ){
      if( EBO_Trace_Optimization )
	fprintf(TFile,
		"%sStore has side effects for Load - Store combination\n",
		EBO_trace_pfx);

      return FALSE;
    }

    INT storeval_idx = OP_find_opnd_use(opinfo->in_op,OU_storeval);
    if (storeval_idx < 0)
    {
      if (EBO_Trace_Optimization)
	fprintf(TFile,
		"%sStore value TN unknown for Load - Store combination\n",
		EBO_trace_pfx);
	  
      return FALSE;
    }

    TN *storeval_tn = OP_opnd(opinfo->in_op, storeval_idx);
    const int size_storeval = CGTARG_Mem_Ref_Bytes(opinfo->in_op);

    /* Make sure the storeval/result TNs' regclasses and mtypes
       match. It isn't sufficient to just check regclasses since
       user-defined operations for two ctypes in the same regfile can
       have different semantics. Make an exception for 32-bit
       loads/stores to the integer register file, since we know that
       they have the same semantics for both signed and unsigned. */

    if (TN_register_class(OP_result(op,0)) !=
	TN_register_class(storeval_tn))
    {
      if (Special_Store_Load_Sequence(op, actual_tninfo, opnd_tninfo, opinfo))
        return TRUE;

      if (EBO_Trace_Data_Flow)
	fprintf(TFile,"%sRegclass mismatch for Store - Load combination\n",
		EBO_trace_pfx);

      return FALSE;
    }

    if (!EBO_in_peep &&
	(OP_bb(op) != OP_bb(opinfo->in_op)) &&
	!TN_Is_Constant(storeval_tn) &&
	has_assigned_reg(storeval_tn))
    {
      if (EBO_Trace_Data_Flow)
	fprintf(TFile,"%sShouldn't move dedicated register references across blocks.\n",
		EBO_trace_pfx);
      
      return FALSE;
    }

    if (TN_is_dedicated(storeval_tn) && 
	(TN_register(storeval_tn) == RCX || TN_register(storeval_tn) == RDX)) 
    { // bug 3842
      if (EBO_Trace_Data_Flow)
	fprintf(TFile,"%sShould not move special dedicated registers RCX and RDX.\n",
		EBO_trace_pfx);
      return FALSE;
    }

    /* If the size of the data moved to and from memory is the same,
       but the size of the stored value is larger than the size of
       the value we want to load, then mask off the upper portion of
       the stored value and use that instead of the loaded value. */
    if (size_pred == size_succ)
    {
      if (size_storeval > size_succ)
      {
	if (EBO_Trace_Data_Flow)
	  fprintf(TFile,"%sSize mismatch for Store - Load combination: %d %d %d\n",
		  EBO_trace_pfx,size_pred,size_storeval,size_succ);

	return delete_subset_mem_op (op, opnd_tninfo, opinfo, 0, 0);
      }

      if (EBO_Trace_Optimization)
	fprintf(TFile,"%sRemove Store - Load combination\n",EBO_trace_pfx);

      TOP top = OP_code(op);
      if (TOP_is_load_ext(top) &&
	  top != OP_code(opinfo->in_op))
	// The load needs sign/zero extension
	Exp_COPY_Ext(top, OP_result(op, 0), storeval_tn, &ops);
      else
	EBO_Exp_COPY(NULL, OP_result(op, 0), storeval_tn, &ops);

      if (EBO_in_loop) {
	CG_LOOP_Init_Op(OPS_first(&ops));
	Set_OP_omega (OPS_first(&ops), 0, opinfo->actual_opnd[storeval_idx]->omega);
      }

      BB_Insert_Ops(OP_bb(op), op, &ops, FALSE);
      return TRUE;
    }
    /* The size of the memory accesses are different, but the starting
       memory address is the same.  There is a chance that the
       predecessor store is wider than the load. */
    else
    {
      return delete_subset_mem_op (op, opnd_tninfo, opinfo, 0, 0);
    }
  }
  /* Remove the first OP for:
     Store - Store
  */
  else if (OP_store(op) && OP_store(opinfo->in_op) &&
	   (OP_bb(op) == OP_bb(opinfo->in_op)))
  {
    if (size_pred != size_succ)
      return FALSE;

    if (opinfo->op_must_not_be_removed)
      return FALSE;

    if (EBO_Trace_Optimization)
      fprintf(TFile,"%sRemove Store - Store combination\n",EBO_trace_pfx);

    remove_op (opinfo);
    OP_Change_To_Noop(opinfo->in_op);
    opinfo->in_op = NULL;
    opinfo->in_bb = NULL;
    return FALSE;
  }
  /* Don't optimize:
     Load - Store
  */
  else {
    if( EBO_Trace_Optimization ){
      fprintf( TFile, "Load - Store combination is not optimized\n" );
    }
#if 0
    if( OP_load( opinfo->in_op ) &&
	OP_prev(op) == opinfo->in_op &&
	OP_result( opinfo->in_op, 0 ) == OP_opnd( op, 0 ) ){
      return TRUE;
    }
#endif
    return FALSE;
  }

  return FALSE;
}


/* 
 * delete_duplicate_op
 *
 * For a given op and one it matches, attempt to replace 
 * one of them.
 * Return TRUE if this op is no longer needed.
 */
BOOL
delete_duplicate_op (OP *op,
		     EBO_TN_INFO **opnd_tninfo,
		     EBO_OP_INFO *opinfo,
		     EBO_TN_INFO **actual_tninfo)
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_DELETE_DUPLICATE_OP)) return FALSE;
#endif
  INT resnum;
  OPS ops = OPS_EMPTY;

  // integer compare ops can not be deleted because they
  // need to set the rflags once again.
  if (OP_icmp(op))
    return FALSE;

  if (EBO_Trace_Data_Flow) {
    fprintf(TFile,"%sDuplicate OP in BB:%d    ",EBO_trace_pfx,BB_id(OP_bb(op)));
    Print_OP_No_SrcLine(op);
    fprintf(TFile,"      Matches   OP in BB:%d    ",BB_id(opinfo->in_bb));
    Print_OP_No_SrcLine(opinfo->in_op);
  }

  /* Global TN's aren't supported at low levels of optimization. */

  if ((Opt_Level < 2) && (OP_bb(op) != opinfo->in_bb))
    return FALSE;

  /* Separate load/store processing, but logically it's just a special case. */

  if (OP_memory(op))
  {
    return delete_memory_op (op, actual_tninfo, opnd_tninfo, opinfo);
  }
  else
  {
    /* Take good care of the rflags cases.
     */

    if( TOP_is_change_rflags( OP_code(op) ) ){
      for( OP* next_op = OP_next(op); next_op != NULL; next_op = OP_next( next_op ) ){
	if( OP_reads_rflags( next_op ) )
	  return FALSE;

	if( TOP_is_change_rflags( OP_code(next_op) ) )
	  break;
      }
    }

    /* There is no easy way to copy FCC registers, so skip this optimization
     *  if the result is of register class FCC. */

    TOP top = OP_code(op);
    /* Create copies of the result TN's. */

    for (resnum = 0; resnum < OP_results(op); resnum++) {
      if (TOP_is_load_ext(top) &&
	  top != OP_code(opinfo->in_op) )
	// The load needs sign/zero extension
	Exp_COPY_Ext(top, OP_result(op, resnum), 
		     OP_result(opinfo->in_op, resnum), &ops);
      else
	EBO_Exp_COPY(NULL, OP_result(op, resnum), 
		     OP_result(opinfo->in_op, resnum), &ops);
    }

    if (EBO_in_loop)
      EBO_OPS_omega (&ops);

    BB_Insert_Ops(OP_bb(op), op, &ops, FALSE);
    return TRUE;
  }
  
  return FALSE;
}


/* Return the opcode for <op> that can take <imm_val>
   as its <opnd>th operand.
*/
static TOP TOP_with_Imm_Opnd( OP* op, int opnd, INT64 imm_val )
{
  const TOP top = OP_code(op);
  const ISA_OPERAND_INFO* oinfo = ISA_OPERAND_Info(top);
  const ISA_OPERAND_VALTYP* vtype = ISA_OPERAND_INFO_Operand(oinfo, 1);
    
  if( ISA_OPERAND_VALTYP_Is_Literal(vtype) )
    return TOP_UNDEFINED;

  //const ISA_LIT_CLASS lc = ISA_OPERAND_VALTYP_Literal_Class(vtype);
  if( !ISA_LC_Value_In_Class( imm_val, LC_simm32 ) )
    return TOP_UNDEFINED;

  return CGTARG_Immed_To_Reg( top );
}


/* Attempt to convert an add of 'tn' + 'imm_val' into an addi. Return
   TRUE if we succeed, FALSE otherwise. */
static BOOL
Convert_Imm_Add (OP *op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo)
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_CONVERT_IMM_ADD)) return FALSE;
#endif
  OP *new_op = NULL;
  TOP new_opcode;
  BOOL is_64bit = (TN_size(tnr) == 8);
  if (imm_val == 0) {
    if( Is_Target_32bit() ){
      for( OP* next_op = OP_next(op); next_op != NULL; next_op = OP_next( next_op ) ){
	if( OP_reads_rflags( next_op ) ){
	  return FALSE;
	}

	if( TOP_is_change_rflags( OP_code(next_op) ) )
	  break;
      }
    }

    new_opcode = is_64bit ? TOP_mov64 : TOP_mov32;
    new_op = Mk_OP(new_opcode, tnr, tn);

  } else if (ISA_LC_Value_In_Class ( imm_val, LC_simm32)) {
    if ( OP_code(op) == TOP_addi32 || OP_code(op) == TOP_addi64 ||
	 OP_code(op) == TOP_lea32 || OP_code(op) == TOP_lea64 )
      return FALSE;
    new_opcode = is_64bit ? TOP_addi64 : TOP_addi32;
    BOOL rflags_read = FALSE;
    // If there is an instruction that is awaiting a rflags update then, 
    // do not convert the current op.
    for( OP* next_op = OP_next(op); next_op != NULL;
	 next_op = OP_next( next_op ) ){
      if( OP_reads_rflags( next_op ) )
	rflags_read = TRUE;
      if( TOP_is_change_rflags( OP_code(next_op) ) )
	break;
    }
    if( !rflags_read && EBO_in_peep &&
	!TNs_Are_Equivalent(tnr,tn) )
      new_opcode = is_64bit ? TOP_lea64 : TOP_lea32;      
    else if ( imm_val == 1 && CG_use_incdec &&
	      TN_is_register(tnr) && TN_is_register(tn) && 
	      TNs_Are_Equivalent(tnr,tn) )
      new_opcode = is_64bit ? TOP_inc64 : TOP_inc32;      
    else if ( imm_val == -1 && CG_use_incdec && 
	      TN_is_register(tnr) && TN_is_register(tn) && 
	      TNs_Are_Equivalent(tnr,tn) )
      new_opcode = is_64bit ? TOP_dec64 : TOP_dec32;      

    if (new_opcode == OP_code(op) || 
	(rflags_read && 
	 ((TOP_is_change_rflags( new_opcode ) && 
	   !TOP_is_change_rflags( OP_code(op) )) ||
	  (!TOP_is_change_rflags( new_opcode ) && 
	   TOP_is_change_rflags( OP_code(op) )))))
      return FALSE;

    if (new_opcode != TOP_inc32 && new_opcode != TOP_inc64 &&
	new_opcode != TOP_dec32 && new_opcode != TOP_dec64)
      new_op = Mk_OP(new_opcode, tnr, tn, Gen_Literal_TN(imm_val, 4));
    else
      new_op = Mk_OP(new_opcode, tnr, tn);
  } else {
    return FALSE;
    TN *src1 = Gen_Literal_TN(imm_val, TN_size(tnr));
    TN *tmp = Build_TN_Like(tnr);
    new_op = Mk_OP(TOP_ldc64, tmp, src1);
    BB_Insert_Op_After(OP_bb(op), op, new_op);
    op = new_op;
    new_opcode = is_64bit ? TOP_add64 : TOP_add32;
    if( EBO_in_peep ){
      FmtAssert( TNs_Are_Equivalent(tnr,tn), ("Convert_Imm_Add: NYI"));
    }
    new_op = Mk_OP(new_opcode, tnr, tn, tmp);
  }
  BB_Insert_Op_After(OP_bb(op), op, new_op);
  return TRUE;
}


/*
 * Look at an exression that has a constant first operand and attempt to
 * simplify the computations.
 */
BOOL
Constant_Operand0 (OP *op,
                   TN **opnd_tn,
                   EBO_TN_INFO **opnd_tninfo)
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_CONSTANT_OPERAND0)) return FALSE;
#endif
  TOP opcode = OP_code(op);
  INT o0_idx = 0;
  INT o1_idx = (OP_opnds(op) > 1) ? 1 : -1;  

  /* Nothing to optimize if no operands... */
  if (OP_opnds(op) < 1)
    return FALSE;

  if (EBO_Trace_Execution)
  {
    fprintf(TFile, "%sin BB:%d constant0 OP :- %s",
            EBO_trace_pfx, BB_id(OP_bb(op)),TOP_Name(OP_code(op)));
    for (UINT i = 0; i < OP_opnds(op); i++)
    {
      fprintf(TFile," ");
      Print_TN(opnd_tn[i],FALSE);
    }
    fprintf(TFile,"\n");
  }

  TN *tn0 = opnd_tn[o0_idx];
  TN *tn1 = (o1_idx >= 0) ? opnd_tn[o1_idx] : NULL;
  TN *tnr = OP_has_result(op) ? OP_result(op,0) : NULL;

  /* Don't mess with symbols. */
  if (TN_is_symbol(tn0))
    return FALSE;

  /* Conditional moves have two of the three operands marked as opnd1
     and opnd2, so we can reach here (operand representing the use of
     the result register is not marked). However we can't do anything
     special if 'tn0' is constant (we must have this check because
     'tn1' can also be constant when we reach here, we don't go to
     Fold_Constant_Expression because the operand representing the use
     of the result register is not constant). */
  if (TOP_is_cond_move(opcode))
    return FALSE;
  
  /* We should only be called if tn0 is constant and tn1 is not. */
  FmtAssert(TN_Is_Constant(tn0) && 
	    ((OP_opnds(op) > 2) || !tn1 || !TN_Is_Constant(tn1)),
	    ("Constant_Operand0: Unexpected constant/non-constant operands"));

  if (opcode == TOP_add32 ||
      opcode == TOP_add64 ||
      opcode == TOP_lea32 ||
      opcode == TOP_lea64)
    return Convert_Imm_Add(op, tnr, tn1, TN_value(tn0), opnd_tninfo[o1_idx]);

  return FALSE;
}


/* Attempt to convert an int and of 'tn' & '0xffff' into a move ext. Return
   TRUE if we succeed, FALSE otherwise.
*/
static BOOL Convert_Imm_And( OP* op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_CONVERT_IMM_AND)) return FALSE;
#endif
  TOP new_top = TOP_UNDEFINED;
  OPS ops = OPS_EMPTY;

  /* First, handle special cases. */

  if( imm_val == 0xff ){
    // Under m32, not all general purpose registers are byte-addressable.
    if (Is_Target_32bit() &&
	EBO_in_peep) {
      const REGISTER reg = TN_register(tn);
      const REGISTER_SET regs =
	      REGISTER_CLASS_eight_bit_regs(TN_register_class(tn));
      if (!REGISTER_SET_MemberP(regs, reg))
        return FALSE;
    }

    new_top = TN_size(tnr) == 8 ? TOP_movzbq : TOP_movzbl;

  } else if( imm_val == 0xffff ){
    new_top = TN_size(tnr) == 8 ? TOP_movzwq : TOP_movzwl;
    
  } else if( imm_val == 0xffffffff && TN_size(tnr) != 8){
    new_top = TOP_mov32;
  }

  if( new_top != TOP_UNDEFINED ){
    Build_OP( new_top, tnr, tn, &ops );
    BB_Insert_Ops_After( OP_bb(op), op, &ops );

    return TRUE;
  }

  /* Second, convert the opcode to carry the <imm_val>. */

  new_top = TOP_with_Imm_Opnd( op, 1, imm_val );

  if( new_top == TOP_UNDEFINED )
    return FALSE;

  Build_OP( new_top, tnr, tn,
	    Gen_Literal_TN(imm_val,4),
	    &ops );
  BB_Insert_Ops_After( OP_bb(op), op, &ops );

  return TRUE;
}

  
static BOOL Convert_Imm_Or( OP* op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_CONVERT_IMM_OR)) return FALSE;
#endif
  OPS ops = OPS_EMPTY;

  if( imm_val == 0x0 ){
    Exp_COPY( tnr, tn, &ops );
    BB_Insert_Ops_After( OP_bb(op), op, &ops );
    return TRUE;
  }

  const TOP new_top = TOP_with_Imm_Opnd( op, 1, imm_val );
  if( new_top == TOP_UNDEFINED )
    return FALSE;

  Build_OP( new_top, tnr, tn,
	    Gen_Literal_TN(imm_val,4),
	    &ops );
  BB_Insert_Ops_After( OP_bb(op), op, &ops );

  return TRUE;
}

  
static BOOL Convert_Imm_Xor( OP* op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_CONVERT_IMM_XOR)) return FALSE;
#endif
  OPS ops = OPS_EMPTY;

  if( imm_val == 0x0 ){
    Exp_COPY( tnr, tn, &ops );
    BB_Insert_Ops_After( OP_bb(op), op, &ops );
    return TRUE;
  }

  if( imm_val == -1 ){
    Build_OP( TN_size(tnr) == 4 ? TOP_not32 : TOP_not64, tnr, tn, &ops );
    BB_Insert_Ops_After( OP_bb(op), op, &ops );
    return TRUE;
  }

  const TOP new_top = TOP_with_Imm_Opnd( op, 1, imm_val );

  if( new_top == TOP_UNDEFINED )
    return FALSE;

  Build_OP( new_top, tnr, tn,
	    Gen_Literal_TN(imm_val,4),
	    &ops );

  BB_Insert_Ops_After( OP_bb(op), op, &ops );

  return TRUE;
}


static BOOL Convert_Imm_Cmp( OP* op, TN *tnr, TN *tn, INT64 imm_val,
			     EBO_TN_INFO *tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_CONVERT_IMM_CMP)) return FALSE;
#endif
  const TOP new_top = TOP_with_Imm_Opnd( op, 1, imm_val );

  if( new_top == TOP_UNDEFINED )
    return FALSE;

  OPS ops = OPS_EMPTY;
  Build_OP( new_top, tnr, tn, Gen_Literal_TN(imm_val,4), &ops );

  BB_Insert_Ops_After( OP_bb(op), op, &ops );

  return TRUE;
}


/* Attempt to convert an int mul of 'tn' * 'imm_val' into a shift. Return
   TRUE if we succeed, FALSE otherwise.
*/
static BOOL Convert_Imm_Mul( OP *op, TN *tnr, TN *tn, INT64 imm_val, EBO_TN_INFO *tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_CONVERT_IMM_MUL)) return FALSE;
#endif
  TN* tnr1 = ( OP_results(op) == 2 ) ? OP_result( op, 1 ) : NULL;

  OP *new_op = NULL;
  const BOOL is_64bit = (TN_size(tnr) == 8);
  const TYPE_ID mtype = is_64bit ? MTYPE_I8 : MTYPE_I4;
  INT64 val = imm_val < 0 ? -imm_val : imm_val;
  OPS ops = OPS_EMPTY;

  if( imm_val == 0 ){
    Exp_Immediate( tnr, Gen_Literal_TN(0,4), false, &ops );
    if( tnr1 != NULL ){
      Exp_Immediate( tnr1, Gen_Literal_TN(0,4), false, &ops );
    }
    BB_Insert_Ops_After( OP_bb(op), op, &ops );
    return TRUE;
  }

  if( imm_val == 1 ){
    Exp_COPY( tnr, tn, &ops );
    if( tnr1 != NULL ){
      Exp_Immediate( tnr1, Gen_Literal_TN(0,4), false, &ops );
    }
    BB_Insert_Ops_After( OP_bb(op), op, &ops );
    return TRUE;
  }

  if( tnr1 != NULL )
    return FALSE;

  bool need_an_add = false;

  if( val >= 2 &&
      ( (val-1) & (val-2) ) == 0 ){
    val--;
    need_an_add = true;
  }

  /* Check whether it can carry an imm opnd. */

  if( ( val & ( val - 1 ) ) != 0 ){
    const TOP new_top = TOP_with_Imm_Opnd( op, 1, imm_val );

    if( new_top == TOP_UNDEFINED )
      return FALSE;

    Build_OP( new_top, tnr, tn,
	      Gen_Literal_TN(imm_val,4),
	      &ops );
    BB_Insert_Ops_After( OP_bb(op), op, &ops );

    return TRUE;
  }

  if( TNs_Are_Equivalent(tnr, tn ) && need_an_add ){
    if( TN_register(tn) != REGISTER_UNDEFINED )
      return FALSE;

    TN* tmp = Build_TN_Like( tn );
    Exp_COPY( tmp, tn, &ops );
    tn = tmp;
  }

  int power = 0;
  while( val != 1 ){
    power++;
    val >>= 1;
  }

  Expand_Shift( tnr, tn, Gen_Literal_TN( power, 4 ), mtype, shift_left, &ops );

  if( need_an_add ){
    Expand_Add( tnr, tnr, tn, mtype, &ops );
  }

  if( imm_val < 0 )
    Expand_Neg( tnr, tnr, mtype, &ops );

  BB_Insert_Ops_After( OP_bb(op), op, &ops );
  return TRUE;
}


/*
 * Look at an exression that has a constant second operand and attempt to
 * simplify the computations.
 */
BOOL
Constant_Operand1 (OP *op,
                   TN **opnd_tn,
                   EBO_TN_INFO **opnd_tninfo)
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_CONSTANT_OPERAND1)) return FALSE;
#endif
  BB *bb = OP_bb(op);
  TOP opcode = OP_code(op);
  INT o0_idx = 0;
  INT o1_idx = (OP_opnds(op) > 1) ? 1 : -1;  

  /* Nothing to optimize if no operands... */
  if (OP_opnds(op) < 1)
    return FALSE;

  if (EBO_Trace_Execution)
  {
    fprintf(TFile, "%sin BB:%d constant1 OP :- %s",
            EBO_trace_pfx, BB_id(OP_bb(op)), TOP_Name(OP_code(op)));
    for (UINT i = 0; i < OP_opnds(op); i++)
    {
      fprintf(TFile," ");
      Print_TN(opnd_tn[i],FALSE);
    }
    fprintf(TFile,"\n");
  }

  TN *tn0 = opnd_tn[o0_idx];
  TN *tn1 = opnd_tn[o1_idx];
  TN *tnr = OP_has_result(op) ? OP_result(op,0) : NULL;

  /* Don't mess with symbols. */
  if (TN_is_symbol(tn1))
    return FALSE;

  /* Don't treat a memory opnd as a regular opnd. */
  if( o1_idx > 0 &&
      ( OP_find_opnd_use( op, OU_base ) == o1_idx ||
	OP_find_opnd_use( op, OU_index ) == o1_idx ) ){
    return FALSE;
  }

  /* We should only be called if tn1 is constant and tn0 is not. */
  FmtAssert(TN_Is_Constant(tn1) && ((OP_opnds(op) > 2) || !TN_Is_Constant(tn0)),
	    ("Constant_Operand1: Unexpected constant/non-constant operands"));

  /* For all the negative value whose TN_size
     is 4, the higher 32-bit is 0s due to the restriction of opteron.
  */
  const INT64 imm_val = TN_value(tn1);

  if( OP_iand( op ) )
    return Convert_Imm_And(op, tnr, tn0, imm_val, opnd_tninfo[o0_idx]);

  if( OP_ior( op ) )
    return Convert_Imm_Or( op, tnr, tn0, imm_val, opnd_tninfo[o0_idx] );

  if( OP_ixor( op ) )
    return Convert_Imm_Xor(op, tnr, tn0, imm_val, opnd_tninfo[o0_idx]);

  if (opcode == TOP_add32 ||
      opcode == TOP_add64 ||
      opcode == TOP_lea32 || 
      opcode == TOP_lea64 )
    return Convert_Imm_Add(op, tnr, tn0, imm_val, opnd_tninfo[o0_idx]);

  if( OP_imul( op ) )
    return Convert_Imm_Mul( op, tnr, tn0, imm_val, opnd_tninfo[o0_idx] );

  if( OP_icmp( op ) )
    return Convert_Imm_Cmp( op, tnr, tn0, imm_val, opnd_tninfo[o0_idx] );

  /*****************************************************************/
  /* Now, look for sequences ending in 'op' that can be optimized. */

  /* No opnd info if operand is constant. */
  if (opnd_tninfo[o0_idx] == NULL)
    return FALSE;

  OP *pred_op = opnd_tninfo[o0_idx]->in_op;
  if (pred_op == NULL)
    return FALSE;

  TOP pred_opcode = OP_code(pred_op);

  /* Look for a sequence of two addi that can be combined. */
  if (OP_iadd(op) && OP_iadd(pred_op))
  {
    INT ptn0_idx = 0;
    INT ptn1_idx = 1;
    TN *ptn0 = OP_opnd(pred_op, ptn0_idx);
    TN *ptn1 = OP_opnd(pred_op, ptn1_idx);

    if (TN_is_constant(ptn1) && !TN_is_symbol(ptn1))
    {
      EBO_OP_INFO *pred_opinfo = locate_opinfo_entry(opnd_tninfo[o0_idx]);
      EBO_TN_INFO *ptn0_tninfo = pred_opinfo->actual_opnd[ptn0_idx];

      if (EBO_tn_available(bb, ptn0_tninfo))
      {
	const INT64 new_val = imm_val + TN_value(ptn1);
	if (Convert_Imm_Add(op, tnr, ptn0, new_val, ptn0_tninfo))
	{
	  if (EBO_Trace_Optimization)
	    fprintf(TFile,"\tcombine immediate adds\n");

	  return TRUE;
	}
      }
    }
  }

  return FALSE;
}


  
/*
 * Look at a branch exression that has all constant operands and attempt to
 * evaluate the expression.
 *
 */
BOOL
Resolve_Conditional_Branch (OP *op, TN **opnd_tn)
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_RESOLVE_CONDITIONAL_BRANCH)) return FALSE;
#endif
  if( TOP_is_ijump( OP_code(op) ) )
    return FALSE;

  Is_True( false, ("Resolve_Conditional_Branch: NYI") );
  return FALSE;
}


  
/*
 * Look at an exression that has all constant operands and attempt to
 * evaluate the expression.
 *
 * Supported operations are:
 *   add, sub, mult, and, or, xor, nor, sll, srl, slt
 */
BOOL
Fold_Constant_Expression (OP *op,
                          TN **opnd_tn,
                          EBO_TN_INFO **opnd_tninfo)
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_FOLD_CONSTANT_EXPRESSION)) return FALSE;
#endif
  TOP opcode = OP_code(op);
  TN *tnr = OP_result(op,0);

  if (OP_opnds(op) == 0)
    return FALSE;

  /* Only attempt to do compile-time arithmetic on integers. */

  if (TN_register_class(tnr) != ISA_REGISTER_CLASS_integer)
    return FALSE;

  /* Don't remove an op that will change the rflag, and this rflag is necessary
     for the following op.
  */
  if( TOP_is_change_rflags( OP_code(op) ) ){
    for( OP* next_op = OP_next(op); next_op != NULL; next_op = OP_next( next_op ) ){
      if( OP_reads_rflags( next_op ) )
	return FALSE;

      if( TOP_is_change_rflags( OP_code(next_op) ) )
	break;
    }
  }

  if (EBO_Trace_Execution)
  {
    fprintf(TFile, "%sin BB:%d Constant OP :- %s ",
	    EBO_trace_pfx, BB_id(OP_bb(op)),TOP_Name(opcode));

    for (UINT i = 0; i < OP_opnds(op); i++)
    {
      fprintf(TFile," ");
      Print_TN(opnd_tn[i],TRUE);
    }
      
    fprintf(TFile,"\n");
  }

  TN *tn0 = opnd_tn[0];
  TN *tn1 = opnd_tn[1];

  INT64 result_val;
  INT64 tn0_val, tn1_val;
  UINT64 tn0_uval, tn1_uval;

  ST *result_sym = NULL;
  INT32 result_relocs;

  /* Determine the constant value of every operand. */
  
  if (TN_is_symbol(tn0))
  {
    /* Can we handle case where both tn's are symbols? How? */
    if ((OP_opnds(op) == 2) && TN_is_symbol(tn1))
      return FALSE;
      
    tn0_uval = TN_offset(tn0);
    tn0_val = TN_offset(tn0);
    result_sym = TN_var(tn0);
    result_relocs = TN_relocs(tn0);
  }
  else
  {
    tn0_val = tn0_uval = TN_value(tn0);
  }

  if (OP_opnds(op) == 1)
  {
    tn1_val = 0;
    tn1_uval = 0;
  }
  else if (TN_is_symbol(tn1))
  {
    tn1_uval = TN_offset(tn1);
    tn1_val = TN_offset(tn1);
    result_sym = TN_var(tn1);
    result_relocs = TN_relocs(tn1);
  }
  else
  {
    tn1_val = tn1_uval = TN_value(tn1);
  }

  /* All the rest of the operations have at most two operands. */
  
  if (OP_opnds(op) > 2)
    return FALSE;

  /* Addition... */

  if (TOP_is_iadd(opcode))
  {
    result_val = tn0_val + tn1_val;
    if( TN_size(tnr) == 4 && Get_TN_Pair(tnr) == NULL )
      result_val = (INT32)result_val;
    goto Constant_Created;
  }

  if (OP_opnds(op) == 2 && !TN_is_symbol(tn1))
  {
    /* Subtraction... */

    if (opcode == TOP_sub32 || opcode == TOP_sub64 ||
	opcode == TOP_subi32 || opcode == TOP_subi64)
    {
      result_val = tn0_val - tn1_val;
      goto Constant_Created;
    }

    /* Multiplication... */

    if( opcode == TOP_imuli32 ||
	opcode == TOP_imuli64 ){
      result_val = tn0_val * tn1_val;
      if( TN_size(tnr) == 4 && Get_TN_Pair(tnr) == NULL )
	result_val = (INT32)result_val;
      goto Constant_Created;
    }
  }

  /* Logical... */
  
  if (opcode == TOP_and32 || opcode == TOP_and64 ||
      opcode == TOP_andi32 || opcode == TOP_andi64)
  {
    result_val = tn0_uval & tn1_uval;
    goto Constant_Created;
  }
  else if (opcode == TOP_or32 || opcode == TOP_or64 ||
	   opcode == TOP_ori32 || opcode == TOP_ori64)
  {
    result_val = tn0_uval | tn1_uval;
    goto Constant_Created;
  }
  else if (opcode == TOP_xor32 || opcode == TOP_xor64 ||
	   opcode == TOP_xori32 || opcode == TOP_xori64)
  {
    result_val = tn0_uval ^ tn1_uval;
    goto Constant_Created;
  }
    
  /* Shift... */

  if (opcode == TOP_shl32 || opcode == TOP_shli32 )
  {
    result_val = TRUNC_32(tn0_uval << tn1_uval);
    goto Constant_Created;
  }

  if ( opcode == TOP_shl64 || opcode == TOP_shli64)
  {
    result_val = tn0_uval << tn1_uval;
    goto Constant_Created;
  }

  else if (opcode == TOP_sar32 || opcode == TOP_sar64 ||
	   opcode == TOP_sari32 || opcode == TOP_sari64)
  {
    result_val = tn0_val >> tn1_uval;

    // Set the most significant bits according to the sign bit.  Bug 9150.
    if ((opcode == TOP_sar32 ||
	 opcode == TOP_sari32) &&
	(tn0_val & 0x80000000)) {
      result_val |= (~0 << (31 - tn1_uval)) & 0xffffffff;
    }
    goto Constant_Created;
  }
  else if (opcode == TOP_shr32 || opcode == TOP_shri32)
  {
    result_val = TRUNC_32(tn0_val) >> tn1_uval;
    goto Constant_Created;
  }
  else if (opcode == TOP_shr64 || opcode == TOP_shri64)
  {
    result_val = (UINT64)tn0_val >> tn1_uval;
    goto Constant_Created;
  }

  return FALSE;

  /* We've evaluated the expression, so replace it with the result. */

 Constant_Created:

  OPS ops = OPS_EMPTY;
  TN *tnc;

  if (result_sym != NULL)
  {
    /* Don't consider using an offset that does not fit in the LC_simm32 class.
     */
    if( !ISA_LC_Value_In_Class(result_val, LC_simm32) )
      return FALSE;

    tnc = Gen_Symbol_TN(result_sym, result_val, result_relocs);
  }
  else
  {
    const int size = Get_TN_Pair(tnr) == NULL ? TN_size(tnr) : 8;
    tnc = Gen_Literal_TN(result_val, size);
  }
  
  Expand_Immediate (tnr, tnc, OP_result_is_signed(op,0), &ops);

  /* If generating the literal requires more than one instruction,
     then just keep the original instruction. It's not clear that this
     is always the right thing, since by eliminating the instruction
     we could create dead code. */
  
  if (OP_next(OPS_first(&ops)) != NULL)
    return FALSE;

  if (EBO_in_loop)
    EBO_OPS_omega (&ops);

  BB_Insert_Ops(OP_bb(op), op, &ops, FALSE);

  if (EBO_Trace_Optimization)
  {
    fprintf(TFile, "%sin BB:%d Redefine ",
	    EBO_trace_pfx, BB_id(OP_bb(op)));
    Print_TN(tnr,TRUE);
    fprintf(TFile," with load of ");
    Print_TN(tnc,FALSE);
    fprintf(TFile, "\n");
  }

  return TRUE;
}


static TN *_tn_swap_tmp;
static EBO_TN_INFO *_tninfo_swap_tmp;

/* If 'sll_opinfo' is an immediate logical left shift, return its
   EBO_OP_INFO and the immediate shift value in 'shift' (if 'shift' is
   non-NULL). Return NULL otherwise. */
static EBO_OP_INFO *
imm_sll (EBO_OP_INFO *sll_opinfo, INT32 *shift =NULL)
{
  if (!sll_opinfo)
    return NULL;
  
  OP *sll = sll_opinfo->in_op;
  TOP top = OP_code(sll);
  if (top != TOP_shl32 &&
      top != TOP_shl64 )
    return NULL;

  TN *imm = OP_opnd(sll, 1);
  if (!TN_has_value(imm))
    return NULL;

  if (shift)
    *shift = TN_value(imm);
  
  return sll_opinfo;
}

/* If 'sra_opinfo' is an immediate arithmetic right shift, return its
   EBO_OP_INFO and the immediate shift value in 'shift' (if 'shift' is
   non-NULL). Return NULL otherwise. */
static EBO_OP_INFO *
imm_sra (EBO_OP_INFO *sar_opinfo, INT32 *shift =NULL)
{
  if (!sar_opinfo)
    return NULL;
  
  OP *sar = sar_opinfo->in_op;
  TOP top = OP_code(sar);
  if( top != TOP_sar32 &&
      top != TOP_sar64 )
    return NULL;

  TN *imm = OP_opnd(sar, 1);
  if (!TN_has_value(imm))
    return NULL;

  if (shift)
    *shift = TN_value(imm);
  
  return sar_opinfo;
}

/* If 'srl_opinfo' is an immediate logical right shift, return its
   EBO_OP_INFO and the immediate shift value in 'shift' (if 'shift' is
   non-NULL). Return NULL otherwise. */
static EBO_OP_INFO *
imm_srl (EBO_OP_INFO *srl_opinfo, INT32 *shift =NULL)
{
  if (!srl_opinfo)
    return NULL;
  
  OP *srl = srl_opinfo->in_op;
  TOP top = OP_code(srl);
  if (top != TOP_shl32 &&
      top != TOP_shl64 )
    return NULL;

  TN *imm = OP_opnd(srl, 1);
  if (!TN_has_value(imm))
    return NULL;

  if (shift)
    *shift = TN_value(imm);
  
  return srl_opinfo;
}

/* If 'add_opinfo' is an immediate add, return its EBO_OP_INFO and the
   immediate value in 'imm' (if 'imm' is non-NULL). Return NULL
   otherwise. */
static EBO_OP_INFO *
index_add (EBO_OP_INFO *add_opinfo, TN *base, TN **index =NULL)
{
  if (!add_opinfo)
    return NULL;
  
  OP *add = add_opinfo->in_op;
  TOP top = OP_code(add);
  if (! TOP_is_iadd(top))
    return NULL;

  TN *imm_tn = OP_opnd(add, 1);
  TN *other_tn = OP_opnd(add, 0);

  if (imm_tn == base) {
    if (index)
      *index = other_tn;
  } else
    if (index)
      *index = imm_tn;
  
  return add_opinfo;
}


/* If 'and_opinfo' is an and, return its EBO_OP_INFO and the
   operand tns. Return NULL otherwise. */
static EBO_OP_INFO *
decode_and (EBO_OP_INFO *and_opinfo, TN **left, TN **right)
{
  if (!and_opinfo)
    return NULL;
  
  OP *andd = and_opinfo->in_op;
  TOP top = OP_code(andd);
  if (top != TOP_and32 &&
      top != TOP_and64)
    return NULL;

  TN *left_tn = OP_opnd(andd, 0);
  TN *right_tn = OP_opnd(andd, 1);
  if (!TN_is_register(left_tn) || !TN_is_register(right_tn))
    return NULL;

  if (left)
    *left = left_tn;
  if (right)
    *right = right_tn;
  
  return and_opinfo;
}

/*
 * CGTARG_Copy_Operand already catches most of the case we care about,
 * but there are some extra cases involving a 0 first operand and
 * we want to treat int->float and float->int register moves as copies.
 */
INT EBO_Copy_Operand (OP *op)
{
  INT opnd;

  if (OP_copy(op)) {
    TOP topcode = OP_code(op);
    if (topcode == TOP_add32 || topcode == TOP_add64 ||
        topcode == TOP_or32  || topcode == TOP_or64  ||
        topcode == TOP_mov32 || topcode== TOP_mov64 )
      return 0;    
  }

  opnd = CGTARG_Copy_Operand(op);
  if (opnd >= 0) {
    return opnd;
  }

  return -1;
}

/* Make sure 'bb' contains one or more OPs (with perhaps an
   unconditional jump) that produce a single result and that can be
   speculated. If it does, return the last OP in the sequence. Return
   the number of OPs in 'bb' (not counting any jump) in 'len'. If
   'no_define' is non-NULL, then return NULL if that TN is defined by
   any OP. If 'in_defined_tns' is non-NULL, then return NULL if any of
   those TNs are used by any OP. If 'out_defined_tns' is non-NULL,
   return the TNs defined in any OP that should not be allowed to be
   used by cmovable ops in another BB. */
static OP *
cmovable_op (BB *bb, UINT *len, TN *no_define,
	     BS *in_defined_tns, BS **out_defined_tns)
{
  *len = BB_length(bb);
  if (*len == 0)
    return NULL;
  
  OP *jump = BB_xfer_op(bb);
  if (jump && ((*len == 1) || (OP_code(jump) != TOP_jmp)))
    return NULL;

  /* Examine each OP, collecting the set of defined TNs. If any OP
     can't be speculated, return NULL. */

  work_gtn_set = GTN_SET_ClearD(work_gtn_set);

  OP *last = NULL;
  for (OP *op = BB_first_op(bb); op; op = OP_next(op))
  {
    if (OP_xfer(op))
      break;

    last = op;

    if (!CGTARG_Can_Be_Speculative(op))
      return NULL;

    for (UINT i = 0; i < OP_results(op); i++)
    {
      TN *res = OP_result(op, i);
      if (TN_is_register(res) && !TN_is_const_reg(res))
      {
	if (TN_is_global_reg(res))
	  work_gtn_set = GTN_SET_Union1D(work_gtn_set, res, work_pool);

	if (out_defined_tns)
	  *out_defined_tns = BS_Union1D(*out_defined_tns, TN_number(res), work_pool);
      }
    }

    if (in_defined_tns)
    {
      for (UINT i = 0; i < OP_opnds(op); i++)
      {
	TN *opnd = OP_opnd(op, i);
	if (TN_is_register(opnd) && BS_MemberP(in_defined_tns, TN_number(opnd)))
	  return NULL;
      }
    }
  }

  /* If 'last' defines not more than 1 TN, or if any of the TN's
     defined in non-'last' OPs are live out of the block, or any of
     the defined TN's are 'no_define'; then return NULL. */

  if (!last ||
      (OP_results(last) != 1) ||
      ((no_define != NULL) && GTN_SET_MemberP(work_gtn_set, no_define)))
    return NULL;
  
  work_gtn_set = GTN_SET_Difference1D(work_gtn_set, OP_result(last, 0));
  if (GTN_SET_IntersectsP(work_gtn_set, BB_live_out(bb)))
    return NULL;

  if (out_defined_tns)
    *out_defined_tns = BS_Difference1D(*out_defined_tns,
					   TN_number(OP_result(last, 0)));
    
  if (OP_same_res(last))
    return NULL;

  return last;
}

static BOOL is_live_tn(OP *current_op, TN *current_tn)
{
  OP *op;
  Is_True(GRA_LIVE_Phase_Invoked, ("Bad call to is_live_tn"));
  BOOL is_live = tn_has_live_def_into_BB(current_tn, OP_bb(current_op));
  BOOL past_current_op = FALSE;

  FOR_ALL_BB_OPs(OP_bb(current_op), op) {
    INT num_opnds = OP_opnds(op);
    INT num_results = OP_results(op);
    if (op == current_op) {
      past_current_op = TRUE;
      if (!is_live)
	return FALSE;
    }
    if (past_current_op) {
      for (int opndnum = 0; opndnum < num_opnds; opndnum++) {
	if (tn_registers_identical(current_tn, OP_opnd(op,opndnum)))
	  return TRUE;
      }
    }
    for (int resnum = 0; resnum < num_results; resnum++) {
      if (tn_registers_identical(current_tn, OP_result(op,resnum))) {
	if (past_current_op && !OP_cond_def(op))
	  return FALSE;
	else
	  is_live = TRUE;
      }
    }
  }
  Is_True(past_current_op && is_live, ("Bad call to is_live_tn"));
  return GTN_SET_MemberP(BB_live_out(OP_bb(current_op)), current_tn);
}


static BOOL test_is_replaced( OP* alu_op, OP* test_op, const EBO_TN_INFO* tninfo );


static BOOL move_ext_is_replaced( OP* op, const EBO_TN_INFO* tninfo )
{
  struct SIZE_EXT_INFO op_size_ext_info;
  struct SIZE_EXT_INFO pred_size_ext_info;

#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_MOVE_EXT_IS_REPLACED)) return FALSE;
#endif
  OP* pred = tninfo == NULL ? NULL : tninfo->in_op;

  if( pred == NULL )
    return FALSE;

  const TOP pred_top = OP_code(pred);

  if( !TOP_is_move_ext( pred_top ) &&
      !TOP_is_load( pred_top ) ){
    return FALSE;
  }

  // Delete OP if it zero-extends a value that is already zero-extended.  For
  // example:
  //    x = movzbl ...	# zero-extend 8->32
  //  ... = movzbq x	# zero-extend 8->64
  // movzbq can be deleted because movzbl implicitly zero-extends to 64-bit.
  if (TOP_is_move_ext(OP_code(op)) &&
      (TOP_is_move_ext(pred_top) || TOP_is_load_ext(pred_top))) {
    Get_Size_Ext_Info(pred_top, &pred_size_ext_info );
    Get_Size_Ext_Info(OP_code(op), &op_size_ext_info );
    if (op_size_ext_info.dest_size >= pred_size_ext_info.dest_size &&
	op_size_ext_info.src_size >= pred_size_ext_info.src_size &&
	op_size_ext_info.sign_ext == FALSE &&
	pred_size_ext_info.sign_ext == FALSE) {
      // OK to delete OP.  Do this by inserting a copy after OP to copy OP's
      // src to OP's result.  This will make OP dead and be removed.
      OPS ops = OPS_EMPTY;
      Exp_COPY(OP_result(op, 0), OP_opnd(op, 0), &ops);
      BB_Insert_Ops(OP_bb(op), op, &ops, FALSE);
    }
  }

  if( TOP_is_load( pred_top ) &&
      OP_find_opnd_use( pred, OU_index ) >= 0 )
    return FALSE;

  Get_Size_Ext_Info( pred_top, &pred_size_ext_info );
  Get_Size_Ext_Info( OP_code(op), &op_size_ext_info );

  if( pred_size_ext_info.sign_ext &&
      !op_size_ext_info.sign_ext ){
    return FALSE;
  }

  //if( pred_size_ext_info.dest_size != op_size_ext_info.src_size )    return FALSE;

  OP* new_op = NULL;
  OPS ops = OPS_EMPTY;

  if( TOP_is_move_ext( pred_top ) ){
#if 0
    if( ( OP_bb( pred ) != OP_bb( op ) ) &&
	has_assigned_reg( OP_opnd(pred,0) ) )
      return FALSE;
#endif
    EBO_TN_INFO* opnd_info = get_tn_info( OP_opnd(pred,0) );
    if( opnd_info != NULL && opnd_info->sequence_num > tninfo->sequence_num )
      return FALSE;

    const TYPE_ID mtype = op_size_ext_info.dest_size == 4 ? MTYPE_I4 : MTYPE_I8;
    
    /* bug#1711
       Now we only got one situation that <pred> performs zero ext, and
       <op> performs sign ext.
    */
    const bool sign_ext =
      ( op_size_ext_info.src_size <= pred_size_ext_info.src_size ) ?
      op_size_ext_info.sign_ext : pred_size_ext_info.sign_ext;
    // bug 6871 - choose the min size to sign-extend or zero-extend.
    const INT size = 
      ( op_size_ext_info.src_size <= pred_size_ext_info.src_size ) ?
      op_size_ext_info.src_size : pred_size_ext_info.src_size;

    Expand_Convert_Length( OP_result( op, 0 ), OP_opnd( pred, 0 ),
			   Gen_Literal_TN( 8*size, 4 ),
			   mtype,
			   sign_ext,
			   &ops );

    new_op = OPS_first( &ops );

    if( ( OP_code(new_op) == OP_code(op) ) &&
	TNs_Are_Equivalent( OP_result(pred,0), OP_opnd(pred,0) ) )
      return FALSE;


  } else if( TOP_is_load( pred_top ) ){
    return FALSE;

    if( !TNs_Are_Equivalent( OP_result(op,0), OP_opnd(op,0) ) )
      return FALSE;

    if( OP_find_opnd_use( pred, OU_index ) >= 0 )
      return FALSE;

    /* Make sure the result of <pred> is used only once. */
    EBO_TN_INFO* opnd_info = get_tn_info( OP_result(pred,0) );
    if( opnd_info->reference_count > 1 )
      return FALSE;    

    /* Make sure the base ptr is not over-written. */
    opnd_info = get_tn_info( OP_opnd( pred, 0 ) );
    if( opnd_info != NULL && opnd_info->sequence_num > tninfo->sequence_num )
      return FALSE;

    EBO_OP_INFO* pred_opinfo = locate_opinfo_entry((EBO_TN_INFO*)tninfo);
    FmtAssert( pred_opinfo->in_op == pred, ("move_ext_is_replaced: NYI (1)") );

    TOP new_top = TOP_UNDEFINED;

    if( pred_size_ext_info.src_size == 1 ){
      if( op_size_ext_info.dest_size == 4 )
	new_top = pred_size_ext_info.sign_ext ? TOP_ld8_32 : TOP_ldu8_32;
      else if( op_size_ext_info.dest_size == 8 )
	new_top = pred_size_ext_info.sign_ext ? TOP_ld8_64 : TOP_ldu8_64;

    } else if( pred_size_ext_info.src_size == 2 ){
      if( op_size_ext_info.dest_size == 4 )
	new_top = pred_size_ext_info.sign_ext ? TOP_ld16_32 : TOP_ldu16_32;
      else if( op_size_ext_info.dest_size == 8 )
	new_top = pred_size_ext_info.sign_ext ? TOP_ld16_64 : TOP_ldu16_64;

    } else if( pred_size_ext_info.src_size == 4 ){
      if( op_size_ext_info.dest_size == 8 ){
	FmtAssert( op_size_ext_info.sign_ext,("move_ext_is_replaced: NYI (2)"));
	new_top = TOP_ld32_64;
      }
    }

    Is_True( new_top != TOP_UNDEFINED, ("move_ext_is_replaced: NYI (3)") );

    new_op = Mk_OP( new_top,
		    OP_result( op, 0 ),
		    OP_opnd( pred, 0 ),
		    OP_opnd( pred, 1 ) );

    remove_op( pred_opinfo );
    OP_Change_To_Noop( pred );
  }

  if( new_op == NULL )
    return FALSE;

  Set_OP_unrolling( new_op, OP_unrolling(op) );
  Set_OP_orig_idx( new_op, OP_map_idx(op) );
  Set_OP_unroll_bb( new_op, OP_unroll_bb(op) );
	
  Copy_WN_For_Memory_OP( new_op, pred );
  if ( OP_volatile( pred ) )
    Set_OP_volatile( new_op );
  OP_srcpos( new_op ) = OP_srcpos( op );
  BB_Insert_Op_After( OP_bb(op), op, new_op );

  if( EBO_Trace_Data_Flow ){
#pragma mips_frequency_hint NEVER
    fprintf( TFile, "Special_Sequence merges " );
    Print_OP_No_SrcLine( pred );
    fprintf( TFile, "                   with   " );
    Print_OP_No_SrcLine( op );
    
    fprintf( TFile, "                   new op " );
    Print_OP_No_SrcLine( new_op );
  }

  return TRUE;
}

BOOL Delete_Unwanted_Prefetches ( OP* op )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_DELETE_UNWANTED_PREFETCHES)) return FALSE;
#endif
  WN *mem_wn = Get_WN_From_Memory_OP(op);
  TN* base;

  OP *incr = NULL;
  OP *as_opnd = NULL;
  OP *as_result = NULL;
  OP *load_store = NULL;
  BB* bb = OP_bb( op );
  OP *next = BB_first_op( bb );

  INT prefetch_offset = WN_offset(mem_wn);

#ifdef KEY
  // bug 10953: LNO tells me this prefetch must be kept
  if(PF_GET_KEEP_ANYWAY(WN_prefetch_flag(mem_wn)))
   return FALSE;
#endif  
  if (OP_find_opnd_use( op, OU_base ) >= 0)
    base = OP_opnd( op, OP_find_opnd_use( op, OU_base ));
  else
    return FALSE; // Can not analyze further; make safe assumption.
   
  while (next && !incr) {
    
    if ((OP_code(next) == TOP_addi32 || OP_code(next) == TOP_addi64)) {
      if (OP_results(next) != 0 && OP_result(next, 0) == base && 
	  OP_opnd(next, 0) == base)
	incr = next;
      else if (OP_results(next) != 0 && OP_result(next, 0) == base)
	as_result = next;
      else if (OP_opnd(next, 0) == base)
	as_opnd = next;
    }
    
    next = OP_next(next);
  }
  
  if (!incr) {
    if (!as_result && !as_opnd)
      return TRUE;
    else if (as_result)
      incr = as_result;
    else 
      incr = as_opnd;
  }
  
  INT delta_base = TN_value(OP_opnd(incr, 1));

  next = BB_first_op( bb );
  while (next && !load_store) {
    if ((OP_memory(next) || OP_load_exe(next)) &&
	OP_find_opnd_use( next, OU_base ) >= 0 &&
	base == OP_opnd( next, OP_find_opnd_use( next, OU_base ))) {
      INT load_store_offset = 
	TN_value(OP_opnd( next, OP_find_opnd_use( next, OU_offset )));
      INT prefetch_ahead = LNO_Prefetch_Ahead;
      INT Cache_Line_Size = 64 /* bytes */;
      INT leeway = 3; // some ops may be moved around by EBO and scheduler.
      if ((delta_base - prefetch_offset +load_store_offset) < 
	  (Cache_Line_Size*(prefetch_ahead+ leeway)))
	load_store = next;
    }
    
    next = OP_next(next);
  }
  
  if (!load_store)
    return TRUE;

  return FALSE;
}

/*
 * Look at an expression and it's inputs to identify special sequences
 * that can be simplified.
 */
BOOL Special_Sequence( OP *op, TN **opnd_tn, EBO_TN_INFO **opnd_tninfo )
{  
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_SPECIAL_SEQUENCE)) return FALSE;
#endif
  const TOP top = OP_code( op );

  if( top == TOP_lea64  ||
      top == TOP_leax64 ||
      top == TOP_leaxx64 ){
    if( EBO_Merge_Memory_Addr( op, opnd_tn, opnd_tninfo, opnd_tninfo ) ){
      return TRUE;
    }
  }

  if( OP_idiv(op) ){
    /* Convert
       rax rdx :- idiv rax rdx rax
       to
       rax :- 1
       rdx :- 0
    */
    const TN* divisor = OP_opnd( op, 2 );
    const TN* dividend = OP_opnd( op, 0 );

    if( divisor == dividend ){
      TN* quotient = OP_result( op, 0 );
      TN* remainder = OP_result( op , 1 );

      /* x / x = 1 */
      OPS ops = OPS_EMPTY;
      Expand_Immediate( quotient,  Gen_Literal_TN( 1, 4 ), false, &ops );
      Expand_Immediate( remainder, Gen_Literal_TN( 0, 4 ), false, &ops );
      BB_Insert_Ops_After( OP_bb(op), op, &ops );

      OP_srcpos(OPS_first(&ops)) = OP_srcpos(op);
      OP_srcpos(OPS_last(&ops)) = OP_srcpos(op);

      if( EBO_Trace_Data_Flow ){
	fprintf( TFile, "Special_Sequence replaces " );
	Print_OP_No_SrcLine(op);
	fprintf( TFile, "with " );
	Print_OPS_No_SrcLines( (const OPS*)&ops);
      }

      return TRUE;
    }
  }

  /* Folding: TOP_movsbq = TOP_movsbl + TOP_movslq
   */
  if( TOP_is_move_ext( top ) ){
    if( move_ext_is_replaced( op, opnd_tninfo[0] ) )
      return TRUE;
  }

  if( ( top == TOP_test32 || top == TOP_test64 ) &&
      TNs_Are_Equivalent( OP_opnd(op,0), OP_opnd(op,1) ) ){

    const EBO_TN_INFO* tninfo = opnd_tninfo[0];
    OP* alu_op = tninfo == NULL ? NULL : tninfo->in_op;

    if( test_is_replaced( alu_op, op, tninfo ) )
      return TRUE;

  } else if ( top == TOP_movslq ) {
    const EBO_TN_INFO* tninfo = opnd_tninfo[0];
    OP* alu_op = tninfo == NULL ? NULL : tninfo->in_op;

    if (alu_op == NULL || alu_op->bb != op->bb)
      return FALSE;

    if (op->next != NULL &&
	is_live_tn(op->next, OP_result(alu_op, 0))) 
      return FALSE;
    else if (op->next == NULL && 
	     GTN_SET_MemberP(BB_live_out(OP_bb(op)), OP_result(alu_op, 0)))
      return FALSE;

    if (!TN_is_register(OP_opnd(alu_op, 0)))
      return FALSE;
	
    /* Make sure the opnd0 of <alu_op> is not re-defined again.
     */
    const EBO_TN_INFO* alu_opnd0_info = get_tn_info( OP_opnd( alu_op, 0 ) );
    if( alu_opnd0_info &&
	alu_opnd0_info->sequence_num >= tninfo->sequence_num )
      return FALSE;
    
    if (OP_code(alu_op) == TOP_andi32 &&
	(unsigned int)TN_value( OP_opnd( alu_op, 1)) <= 0x7fffffff &&
	(!EBO_in_peep ||
	 TN_is_register( OP_result( op, 0)) &&
	 TN_is_register( OP_opnd( alu_op, 0)) &&
	 TNs_Are_Equivalent( OP_result( op, 0), OP_opnd( alu_op, 0)))) {
      OP* new_op = Mk_OP( OP_code(alu_op), OP_result(op, 0),
			  OP_opnd(alu_op, 0), OP_opnd(alu_op, 1));
      OP_srcpos( new_op ) = OP_srcpos( op );
      BB_Insert_Op_After( OP_bb(op), op, new_op );

      if( EBO_Trace_Data_Flow ){
	fprintf( TFile, "Special_Sequence merges " );
	Print_OP_No_SrcLine(op);
	fprintf( TFile, "and " );
	Print_OP_No_SrcLine(alu_op);
	fprintf( TFile, "with ");
	Print_OP_No_SrcLine(new_op);
      }
      return TRUE;
    }
  } else if ( top == TOP_ldc32 || top == TOP_ldc64 ) {
    TN* tn0 = OP_opnd(op, 0);
    BOOL rflags_read = FALSE;
      
    /* Don't mess with symbols. */
    if (TN_is_symbol(tn0))
      return FALSE;

    /* We should not convert ldc to xor if constant is non-zero */
    if( TN_value(tn0) != 0)
      return FALSE;
      
    // If there is an instruction that is awaiting a rflags update then, 
    // do not convert the current op.
    for( OP* next_op = OP_next(op); next_op != NULL;
	 next_op = OP_next( next_op ) ){
      if( OP_reads_rflags( next_op ) )
	rflags_read = TRUE;
      if( TOP_is_change_rflags( OP_code(next_op) ) )
	break;
    }
    if (!rflags_read && CG_use_xortozero) {
      OP* new_op = Mk_OP(top == TOP_ldc32? TOP_zero32: TOP_zero64, 
		     OP_result(op, 0));
      BB_Insert_Op_After(OP_bb(op), op, new_op);
      return TRUE;
    }
  } 

  else if ( top == TOP_movdq ) {
    const EBO_TN_INFO* tninfo = opnd_tninfo[0];
    OP* alu_op = tninfo == NULL ? NULL : tninfo->in_op;
    
    if (!alu_op)
      return FALSE;

    if ( TOP_is_vector_packed_single ( OP_code (alu_op ) ) )
      op->opr = TOP_movaps;
    else if ( TOP_is_vector_packed_double ( OP_code (alu_op ) ) )
      op->opr = TOP_movapd;
  }

  else if( top == TOP_jne || top == TOP_je ) {
    /* For
     *    sete
     *    test32/test64
     *    jne/je
     * Transform to :
     *    je/jne (inverted branch).
     */
    const EBO_TN_INFO* tninfo = opnd_tninfo[0];
    OP* test_op = tninfo == NULL ? NULL : tninfo->in_op;
    if ( test_op && 
	 ( OP_code( test_op ) == TOP_test32 ||
	   OP_code( test_op ) == TOP_test64 ) &&
	 TNs_Are_Equivalent( OP_opnd(test_op, 0), OP_opnd(test_op, 1) ) ) {
      const EBO_TN_INFO* test_tninfo = get_tn_info( OP_opnd(test_op, 0 ));
      OP* set_op = test_tninfo == NULL ? NULL : test_tninfo->in_op;
      if ( set_op && OP_code( set_op ) == TOP_sete &&
	   !TN_live_out_of( OP_result(set_op,0), OP_bb(test_op) ) ){
	// Skip transform if rflag is redefined between set_op and test_op.
	BOOL skip = FALSE;
	if (OP_bb(set_op) != OP_bb(test_op))
	  skip = TRUE;		// Assume redefined if cross BB.
	else {
	  OP* next;
	  for (next = OP_next(set_op); next != test_op; next = OP_next(next)) {
	    if (!next ||	// test_op doesn't follow set_op
		TOP_is_change_rflags(OP_code(next))) {
	      skip = TRUE;
	      break;
	    }
	  }
	}
	// Delete test_op and change the branch.
	if (!skip) {
	  OP_Change_To_Noop( test_op );
	  op->opr = ( top == TOP_jne ) ? TOP_je : TOP_jne;
	}
      }
    }

    OP* alu_op = tninfo == NULL ? NULL : tninfo->in_op;

    if( test_is_replaced( alu_op, op, tninfo ) )
      return TRUE;
  }

  else if (!EBO_in_peep &&
	   top == TOP_shri64) {
    // Replace shift-left/shift-right with shift-right/move.  Look for
    // bit-field extraction of form:
    //   t = x << a
    //   y = t >> b
    // The field size is 64-b.  If the field size is 8, 16, or 32, then replace
    // the sequence with:
    //   t = x >> (b-a)
    //   y = movz t	; movzbq, movzwq, movzlq depending on field size
    // movz is perferred over the second shift because sometimes we have to mov
    // into a special register anyway, such as rax for returning y (bug 8594).
    //
    // There's no advantage in doing this optimization after register
    // allocation.
    const EBO_TN_INFO* tninfo = opnd_tninfo[0];
    OP* alu_op = tninfo == NULL ? NULL : tninfo->in_op;

    if (alu_op &&
	OP_code(alu_op) == TOP_shli64) {
      TN *left = OP_opnd(alu_op, 1);	// left shift amount
      TN *right = OP_opnd(op, 1);	// right shift amount
      Is_True(TN_Is_Constant(left) && TN_has_value(left) &&
	      TN_Is_Constant(right) && TN_has_value(right),
	      ("Special_Sequence: unexpected shift opnds"));
      int new_right_shift_val = TN_value(right) - TN_value(left);
      if (new_right_shift_val < 0)
	return FALSE;

      // Make sure alu_op opnd is not redefined between alu_op and op.
      EBO_TN_INFO *alu_opnd0_tninfo = get_tn_info(OP_opnd(alu_op, 0));
      EBO_TN_INFO *alu_result_tninfo = get_tn_info(OP_result(alu_op, 0));
      if (// Give up if alu_op result is redefined because
	  // alu_result_tninfo->sequence_num wouldn't correspond to alu_op's
	  // sequence number in this case.  We need alu_op's sequence number
	  // for the comparison below.
	  alu_result_tninfo->in_op != alu_op ||
	  // Test against alu_op's sequence number.
          (alu_opnd0_tninfo == NULL ||
	   alu_opnd0_tninfo->sequence_num >= alu_result_tninfo->sequence_num))
        return FALSE;

      TOP mov_opcode;
      int field_size = 64 - TN_value(right);
      if (field_size == 8) {
	mov_opcode = TOP_movzbq;
      } else if (field_size == 16) {
	mov_opcode = TOP_movzwq;
      } else if (field_size == 32) {
	mov_opcode = TOP_movzlq;
      } else {
        return FALSE;
      }
      OP *new_shift, *new_move;
      TN *tmp_tn = Dup_TN(OP_result(alu_op, 0));
      new_shift = Mk_OP(TOP_shri64, tmp_tn, OP_opnd(alu_op, 0),
			Gen_Literal_TN(new_right_shift_val, TN_size(right)));
      BB_Insert_Op_After(OP_bb(op), op, new_shift);
      new_move = Mk_OP(mov_opcode, OP_result(op, 0), tmp_tn);
      BB_Insert_Op_After(OP_bb(op), new_shift, new_move);
      return TRUE;
    }
  }

  return FALSE;
}

/* This is used to eliminate unnecessry shift operations like:
 *     TN78($2) :-  dsrl32 TN1($0) <const> ;
 * Replace uses of the result with Zero_TN, and eliminate the shift op.
 * The def should not be live out of the current basic block.
 * 
 * Also, as a final step eliminate all copy ops (could be by-product of 
 * the removal of shifts above).
 */
void Redundancy_Elimination()
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_REDUNDANCY_ELIMINATION)) return;
#endif
  for( BB* bb = REGION_First_BB; bb != NULL; bb = BB_next( bb ) ){
    if( BB_rid(bb) && (RID_level(BB_rid(bb)) >= RL_CGSCHED) ){
      // don't change bb's which have already been through CG
      continue;
    }
    for( OP* op = BB_first_op( bb ); op != NULL; op = OP_next( op ) ){
      BOOL done = FALSE;
      INT copy_operand = CGTARG_Copy_Operand(op);
      if (copy_operand >= 0) {
	// Other ops could be rendered uselesss by the shift removal.
	// Example:
	//   TN969 :- srl TN1($0) (0x3) ;
	//   TN969 :- xor TN969<defopnd> TN916 ; 
	// Here, the xor operation is uselss if TN969 is not live 
	// out of this block and can be replaced by TN916.

	/* The whole point is to get rid of result and replace 
	   with copy_operand */

	TN* src = OP_opnd( op, copy_operand );
	TN* result = OP_result( op, 0 );
	
	FmtAssert( TN_is_register( src ) && TN_is_register( result ), 
		   ("Redundancy_Elimination: src/result not registers in EBO_in_peep (1)"));
	
	// do not delete assignment to result if result is live out
	if (REG_LIVE_Outof_BB (TN_register_class(result), 
			       TN_register(result), bb)) {
	  // see if the register is re-defined before block end
	  // we can still delete the op if there is a re-definition
	  BOOL redefined = FALSE;
	  for( OP* dw_op = OP_next( op ); dw_op != NULL; dw_op = OP_next( dw_op ) ){
	    if ((OP_code(dw_op) != TOP_asm) && 
		OP_results(dw_op) &&
		TN_is_register(OP_result(dw_op, 0)) &&
		(TN_register_class(OP_result(dw_op, 0)) == 
		 TN_register_class(result)) &&
		(TN_register(OP_result(dw_op, 0)) == TN_register(result))) {
	      redefined = TRUE;
	      break;
	    }
	  }
	  if (redefined == FALSE)
	    continue;
	}

	/* In the following scenario, the copy_op can not be deleted 
	 *    a = b   << copy_op >>
	 *      = a
	 *    b =     << redef >>
	 *      = a
	 * NOTE: If redef is a copy op and copy operand is 'a'
	 *       then copy_op can be removed
	 */
	BOOL cannot_be_removed = FALSE;
	for( OP* dw_op = OP_next( op ); dw_op != NULL; dw_op = OP_next( dw_op ) ){
	  if ((OP_code(dw_op) != TOP_asm) && 
	      OP_results(dw_op) &&
	      TN_is_register( OP_result( dw_op, 0)) &&
	      TNs_Are_Equivalent( OP_result( dw_op, 0), src)) {
	    
	    // see NOTE above
	    INT dw_op_copy_operand = CGTARG_Copy_Operand(dw_op);
	    TN *dw_op_copy_operand_tn = (dw_op_copy_operand>=0)?OP_opnd(dw_op, dw_op_copy_operand):NULL;
	    if ((dw_op_copy_operand >= 0) && 
		TN_is_register( dw_op_copy_operand_tn) &&
		TNs_Are_Equivalent(dw_op_copy_operand_tn, result)) 
	      break;
	    
	    // search if 'result' is being used after dw_op
	    for( OP* dw1_op = OP_next( dw_op ); dw1_op != NULL; dw1_op = OP_next( dw1_op ) ){
	      for( int i = 0; i < OP_opnds( dw1_op ); i++ ){
		if ( TN_is_register( OP_opnd( dw1_op, i)) &&
		     TNs_Are_Equivalent( OP_opnd( dw1_op, i), result)) {
		  cannot_be_removed = TRUE;
		  break;
		}
	      }
	    }
	    if (cannot_be_removed)
	      break;
	  }
	}
	if (cannot_be_removed)
	  continue;	

	/* Traverse downwards; replace result with src */

	for( OP* dw_op = OP_next( op ); dw_op != NULL; dw_op = OP_next( dw_op ) ){

	  for( int i = 0; i < OP_opnds( dw_op ); i++ ){
	    if ( TN_is_register( OP_opnd( dw_op, i)) && 
		 TNs_Are_Equivalent( OP_opnd( dw_op, i), result)) {
	      Set_OP_opnd( dw_op, i, src );
	      done = TRUE;
	    }
	  }

	  if( OP_results( dw_op ) == 1 ){
	    TN* tnr = OP_result( dw_op, 0 );

	    if( TN_is_register( tnr) && 
		TNs_Are_Equivalent( tnr, result ) || 
		TNs_Are_Equivalent( tnr, src ) )
	      break;
	  }
	}      

	if( done ){
	  OP* dead = op;
	  op = OP_prev( op );
	  
	  if( EBO_Trace_Optimization ){
	    fprintf( TFile, "Redundancy_Elimination removes simplified op - " );
	    Print_OP_No_SrcLine( dead );
	  }
	  
	  BB_Remove_Op( bb, dead );
	  
	  if( op == NULL )
	    op = BB_first_op( bb );
	}       

	continue; // end for this op
      }

      if (OP_code( op ) != TOP_sar32 &&
	  OP_code( op ) != TOP_sar64 &&
	  OP_code( op ) != TOP_shl32 && 
	  OP_code( op ) != TOP_shl64 && 
	  OP_code( op ) != TOP_shr32 && 
	  OP_code( op ) != TOP_shr64 )
	continue;

      /* The whole point is to get rid of result. */

      TN* src = OP_opnd( op, 0 );
      TN* result = OP_result( op, 0 );

      FmtAssert( TN_is_register( src ) && TN_is_register( result ), 
		 ("Redundancy_Elimination: src/result not registers in EBO_in_peep (2)"));
      
      FmtAssert( false, ("Redundancy_Elimination: UNIMPLEMENTED (1)") );

      // do not delete assignment to result if result is live out
      if (REG_LIVE_Outof_BB (TN_register_class(result), 
			     TN_register(result), bb)) {
	// see if the register is re-defined before block end
	// we can still delete the op if there is a re-definition
	BOOL redefined = FALSE;
	for( OP* dw_op = OP_next( op ); dw_op != NULL; dw_op = OP_next( dw_op ) ){
	  if (OP_has_result(dw_op) &&
	      TN_is_register(OP_result(dw_op, 0)) &&
	      (TN_register_class(OP_result(dw_op, 0)) == 
	       TN_register_class(result)) &&
	      (TN_register(OP_result(dw_op, 0)) == TN_register(result))) {
	    redefined = TRUE;
	    break;
	  }
	}
	if (redefined == FALSE)
	  continue;
      }

      /* Traverse downwards; replace result with Zero_TN */

      for( OP* dw_op = OP_next( op ); dw_op != NULL; dw_op = OP_next( dw_op ) ){
	for( int i = 0; i < OP_opnds( dw_op ); i++ ){
	  if( TN_is_register(OP_opnd( dw_op, i)) && 
	      (TN_register_class(OP_opnd(dw_op, i)) == 
	       TN_register_class(result)) &&
	      (TN_register(OP_opnd( dw_op, i )) == TN_register(result))) {
	    FmtAssert( false, ("Redundancy_Elimination: UNIMPLEMENTED (2)") );
	    done = TRUE;
	  }
	}

	if( OP_results( dw_op ) == 1 ){
	  TN* tnr = OP_result( dw_op, 0 );

	  if( TN_is_register( tnr) && 
	      TNs_Are_Equivalent( tnr, result ) )
	    break;
	}
      }      

      if( done ){
	OP* dead = op;
	op = OP_prev( op );

	if( EBO_Trace_Optimization ){
	  fprintf( TFile, "Redundancy_Elimination removes simplified op - " );
	  Print_OP_No_SrcLine( dead );
	}

	BB_Remove_Op( bb, dead );

	if( op == NULL )
	  op = BB_first_op( bb );
      }
    }
  }   
}


static inline TN* OP_opnd_use( OP* op, ISA_OPERAND_USE use )
{
  const int indx = OP_find_opnd_use( op, use );
  return ( indx >= 0 ) ? OP_opnd( op, indx ) : NULL;
}


/* return <ofst> = <ofst1> + <ofst2> * <scale>
 */
static TN* Compose_Addr_offset( TN* ofst1, TN* ofst2, TN* scale )
{
  if( ofst1 == NULL )
    ofst1 = Gen_Literal_TN( 0, 4 );

  if( ofst2 == NULL )
    ofst2 = Gen_Literal_TN( 0, 4 );

  if( scale == NULL )
    scale = Gen_Literal_TN( 1, 4 );

  // Cannot handle two symbols.
  if( TN_is_symbol(ofst1) &&
      TN_is_symbol(ofst2) ){
    return NULL;
  }

  if( TN_is_symbol(ofst1) ){
    ST* sym = TN_var(ofst1);
    const INT64 ofst = TN_value(ofst2) * TN_value(scale) + TN_offset(ofst1);

    return Gen_Symbol_TN( sym, ofst, TN_RELOC_NONE );
  }

  if( TN_is_symbol(ofst2) ){
    if( TN_value(scale) != 1 )
      return NULL;

    ST* sym = TN_var(ofst2);
    const INT64 ofst = TN_value(ofst1) + TN_offset(ofst2);

    return Gen_Symbol_TN( sym, ofst, TN_RELOC_NONE );
  }

  const INT64 value = TN_value(ofst1) + TN_value(ofst2) * TN_value(scale);

  if( !ISA_LC_Value_In_Class( value, LC_simm32 ) )
    return NULL;

  return Gen_Literal_TN( value, 4 );
}

#define IS_VALID_SCALE(s)  ( (s)==1 || (s)==2 || (s)==4 || (s)==8 )

static BOOL Compose_Addr( OP* mem_op, EBO_TN_INFO* pt_tninfo,
			  ISA_OPERAND_USE replace_opnd,
			  TN** opnd_tn,
			  TN** index, TN** offset, TN** scale, TN** base )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_COMPOSE_ADDR)) return FALSE;
#endif
  /* address = ofst + base + index * scale */
  struct ADDRESS_COMPONENT {
    TN* index;
    TN* base;
    TN* offset;
    TN* scale;
  } a, b;

  bzero( &a, sizeof(a) );
  a.scale = Gen_Literal_TN( 1, 4 );

  /* First, extract the address components from a previous op that
     compute the address. The components are stored into <a>.
  */

  OP* addr_op = pt_tninfo->in_op;
  const TOP top = OP_code( addr_op );

  switch( top ){
  case TOP_lea32:
  case TOP_lea64:
    a.base   = OP_opnd_use( addr_op, OU_base );
    a.offset = OP_opnd_use( addr_op, OU_offset );
    break;

  case TOP_leaxx32:
  case TOP_leaxx64:
    a.index  = OP_opnd_use( addr_op, OU_index );
    a.offset = OP_opnd_use( addr_op, OU_offset );
    a.scale  = OP_opnd_use( addr_op, OU_scale );

    if( TN_value(a.scale) == 1 ){
      a.base = a.index;
      a.index = a.scale = NULL;
    }

    break;

  case TOP_leax32:
  case TOP_leax64:
    a.index  = OP_opnd_use( addr_op, OU_index );
    a.offset = OP_opnd_use( addr_op, OU_offset );
    a.scale  = OP_opnd_use( addr_op, OU_scale );
    a.base   = OP_opnd_use( addr_op, OU_base );
    break;

  case TOP_shli32:
    if( Is_Target_64bit() )
      return FALSE;
    // fall thru
  case TOP_shli64:
    {
      TN* tn = OP_opnd( addr_op, 1 );
      const INT64 val = TN_value(tn);
      if( val < 0 || val > 3 )
	return FALSE;

      a.scale = Gen_Literal_TN( 1 << val, 4 );
      a.offset = Gen_Literal_TN( 0, 4 );
      a.index = OP_opnd( addr_op, 0 );
    }
    break;

  case TOP_add32:
    if( Is_Target_64bit() )
      return FALSE;
    // fall thru
  case TOP_add64:
    a.base = OP_opnd( addr_op, 0 );
    a.index = OP_opnd( addr_op, 1 );
    a.offset = Gen_Literal_TN( 0, 4 );
    a.scale = Gen_Literal_TN( 1, 4 );
    break;

  case TOP_addi32:
    if( Is_Target_64bit() )
      return FALSE;
    // fall thru
  case TOP_addi64:
    a.base = OP_opnd( addr_op, 0 );
    a.offset = OP_opnd( addr_op, 1 );
    break;

  case TOP_mov32:
    if( Is_Target_64bit() )
      return FALSE;
    // fall thru
  case TOP_mov64:
    {
      TN* opnd = OP_opnd( addr_op, 0 );
      a.offset = Gen_Literal_TN( 0, 4 );
      a.base = opnd;
    }
    break;

  case TOP_ldc32:
  case TOP_ldc64:
    a.offset = OP_opnd( addr_op, 0 );
    break;

  default:
    return FALSE;
  }

  const int op_offset_idx = OP_find_opnd_use( mem_op, OU_offset );
  TN* op_offset_tn = opnd_tn[op_offset_idx];

  // Replace a.index with a symbol from a previous ldc64 op.
  if( a.index != NULL &&
      TN_value( a.scale ) == 1 ){
    const EBO_TN_INFO* tninfo = get_tn_info( a.index );
    if( tninfo != NULL        &&
	tninfo->in_op != NULL &&
	tninfo->sequence_num < pt_tninfo->sequence_num &&
	( OP_code(tninfo->in_op) == TOP_ldc64 ||
	  OP_code(tninfo->in_op) == TOP_ldc32 ) ){
      TN* offset = Compose_Addr_offset( a.offset,
					OP_opnd( tninfo->in_op, 0 ),
					NULL );
      if( offset != NULL ){
	a.offset = offset;
	a.index  = NULL;
      }
    }
  }

  // Replace a.base with a symbol from a previous ldc64 op.
  if( a.base != NULL ){
    const EBO_TN_INFO* tninfo = get_tn_info( a.base );
    if( tninfo != NULL        &&
	tninfo->in_op != NULL &&
	tninfo->sequence_num < pt_tninfo->sequence_num &&
	( OP_code(tninfo->in_op) == TOP_ldc64 ||
	  OP_code(tninfo->in_op) == TOP_ldc32 ) ){
      TN* offset = Compose_Addr_offset( a.offset,
					OP_opnd( tninfo->in_op, 0 ),
					NULL );
      if( offset != NULL ){
	a.offset = offset;
	a.base = NULL;
      }
    }
  }

  if( !EBO_in_peep ){
    if( OP_bb( addr_op ) != OP_bb( mem_op ) ){
      for( int i = 0; i < OP_opnds(addr_op); i++ ){
	if( has_assigned_reg( OP_opnd(addr_op,i) ) )
	  return FALSE;
      }
    }

    if( a.index != NULL ){
      const REGISTER reg = TN_register( a.index );
      if( reg == RAX || reg == RCX || reg == RDX )
	return FALSE;
    }

    if( a.base != NULL ){
      const REGISTER reg = TN_register( a.base );
      if( reg == RAX || reg == RCX || reg == RDX )
	return FALSE;
    }
  }

  /* Check <index> and <base> will not be re-defined between
     <addr_op> and <mem_op>, inclusive.
  */

  if( a.base != NULL ){
    EBO_TN_INFO* tninfo = get_tn_info( a.base );
    if( tninfo != NULL && tninfo->sequence_num >= pt_tninfo->sequence_num ){
      return FALSE;
    }
  }
  
  if( a.index != NULL ){
    EBO_TN_INFO* tninfo = get_tn_info( a.index );
    if( tninfo != NULL && tninfo->sequence_num >= pt_tninfo->sequence_num ){
      return FALSE;
    }
  }

  /* Second, extract the address components from <mem_op>, and deposit them
     to <b>.
  */

  b.scale  = OP_opnd_use( mem_op, OU_scale );
  b.base   = OP_opnd_use( mem_op, OU_base );
  b.index  = OP_opnd_use( mem_op, OU_index );
  b.offset = OP_opnd_use( mem_op, OU_offset );

  if( b.scale == NULL )
    b.scale = Gen_Literal_TN( 1, 4 );

  /* Now, make up the final address components from <a> and <b>.
   */

  *index = *offset = *base = *scale = NULL;

  if( replace_opnd == OU_base ){
    // offset = b.offset + a.offset
    *offset = Compose_Addr_offset( b.offset, a.offset, NULL );

    // base = a.base
    *base = a.base;

    // index * scale = a.index * a.scale + b.index * b.scale
    if( a.index == NULL &&
	b.index == NULL ){
      *index = *scale = NULL;

    } else if( a.index == NULL ){
      *index = b.index;
      *scale = b.scale;

    } else if( b.index == NULL ){
      *index = a.index;
      *scale = a.scale;
      
    } else {
      if( TNs_Are_Equivalent( a.index, b.index ) ){
	*index = a.index;
	*scale = Gen_Literal_TN( TN_value(a.scale) + TN_value(b.scale), 4 );

      } else {
	if( *base != NULL )
	  return FALSE;
	if( TN_value(b.scale) == 1 ){
	  *base = b.index;
	  *index = a.index;
	  *scale = a.scale;

	} else if( TN_value(a.scale) == 1 ){
	  *base = a.index;
	  *index = b.index;
	  *scale = b.scale;

	} else
	  return FALSE;
      }
    }

  } else if( replace_opnd == OU_index ){
    // offset = b.offset + a.offset * b.scale
    *offset = Compose_Addr_offset( b.offset, a.offset, b.scale );

    // index * scale = a.index * a.scale * b.scale
    *index = a.index;
    *scale = Gen_Literal_TN( TN_value(a.scale) * TN_value(b.scale), 4 );

    // base = b.base + a.base * b.scale
    if( b.base == NULL &&
	a.base == NULL ){
      *base = NULL;

    } else if( b.base == NULL ){
      *base = a.base;

      if( TN_value(b.scale) != 1 ){
	if( *index == NULL ){
	  *scale = b.scale;
	  *index = *base;
	  *base  = NULL;

	} else {
	  if( !TNs_Are_Equivalent( *index, a.base ) )
	    return FALSE;
	  *base = NULL;
	  *scale = Gen_Literal_TN( TN_value(*scale) + TN_value(b.scale), 4 );
	}
      }

    } else if( a.base == NULL ){
      *base = b.base;

    } else {
      if( *index == NULL ){
	*index = a.base;
	*scale = b.scale;
	*base  = b.base;

      } else {
	if( !TNs_Are_Equivalent( b.base, a.base ) ) {
	  // Bug 3724 - If a.base and a.index are identical, 
	  // then we could still fold a.base into b.index
	  // and adjust the scale.
	  if ( !TNs_Are_Equivalent ( a.base, a.index ) )
	    return FALSE;
	  else {
	    *index = b.base;
	  }
	}

	if( TN_value(*scale) != 1 )
	  return FALSE;

	*scale = Gen_Literal_TN( TN_value(b.scale) + 1, 4 );
	*base = *index;
	*index = a.base;
      }
    }

  } else {
    return FALSE;
  }

  /* Filter out any invalid combination. */

  if( *offset == NULL ||
      !ISA_LC_Value_In_Class( TN_value(*offset), LC_simm32 ) )
    return FALSE;

  if( *scale != NULL && !IS_VALID_SCALE( TN_value(*scale) ) ){
    if( *base != NULL ||
	TN_value(*scale) > 8 )
      return FALSE;

    *base = *index;
    *scale = Gen_Literal_TN( TN_value(*scale) - 1, 4 );

    if( !IS_VALID_SCALE( TN_value(*scale) ) )
      return FALSE;
  }

  /* Make sure the index is not %rsp. */

  if( *index != NULL &&
      TN_register(*index) == RSP ){
    if( TN_value( *scale ) != 1 )
      return FALSE;

    TN* tmp_tn = *index;
    *index = *base;
    *base = tmp_tn;
  }

  if( *index == NULL ){
    if( *scale != NULL && TN_value(*scale) > 1 )
      return FALSE;
      
    *scale = NULL;
  }

  if( *base == NULL  &&
      *index != NULL &&
      TN_value(*scale) == 1 ){
    *base = *index;
    *index = *scale = NULL;
  }

  return TRUE;
}


// Group together opcodes that perform the same function but with different
// address modes.
typedef struct {
  TOP reg_mode;
  TOP base_mode;
  TOP base_index_mode;
  TOP index_mode;
  TOP n32_mode;
} Addr_Mode_Group;

// Map an opcode to its address-modes group.
static Addr_Mode_Group *Top_To_Addr_Mode_Group[TOP_count+1];

// List all address mode groups.  Each group gives the opcodes for OPs that
// perform the same function but with different address modes.  Entries can be
// listed in any order.  No duplicates allowed.  The table doesn't have to be
// complete; it may list only those OPs that EBO can do something about.
static Addr_Mode_Group Addr_Mode_Group_Table[] = {
  // REG_MODE	BASE_MODE	BASE_INDEX_MODE	INDEX_MODE	N32_MODE

  // Load and stores.
  {TOP_UNDEFINED, TOP_store8,	TOP_storex8,	TOP_storexx8,	TOP_store8_n32},
  {TOP_UNDEFINED, TOP_store16,	TOP_storex16,	TOP_storexx16, TOP_store16_n32},
  {TOP_UNDEFINED, TOP_store32,	TOP_storex32,	TOP_storexx32, TOP_store32_n32},
  {TOP_UNDEFINED, TOP_store64,	TOP_storex64,	TOP_storexx64,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_stss,	TOP_stssx,	TOP_stssxx,	TOP_stss_n32},
  {TOP_UNDEFINED, TOP_stsd,	TOP_stsdx,	TOP_stsdxx,	TOP_stsd_n32},
  {TOP_UNDEFINED, TOP_stdqa,	TOP_stdqax,	TOP_stdqaxx,	TOP_stdqa_n32},
  {TOP_UNDEFINED, TOP_stntpd,	TOP_stntpdx,	TOP_stntpdxx,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_stdqu,	TOP_stdqux,	TOP_stdquxx,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_fmovsldupx, TOP_fmovsldupxx, TOP_fmovsldupxxx, TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_fmovshdupx, TOP_fmovshdupxx, TOP_fmovshdupxxx, TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_fmovddupx,  TOP_fmovddupxx,  TOP_fmovddupxxx, TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_ldss,	TOP_ldssx,	TOP_ldssxx,	TOP_ldss_n32},
  {TOP_UNDEFINED, TOP_ldsd,	TOP_ldsdx,	TOP_ldsdxx,	TOP_ldsd_n32},
  {TOP_UNDEFINED, TOP_lddqa,	TOP_lddqax,	TOP_lddqaxx,	TOP_lddqa_n32},
  {TOP_UNDEFINED, TOP_lddqu,	TOP_lddqux,	TOP_lddquxx,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_ldlps,	TOP_ldlpsx,	TOP_ldlpsxx,	TOP_ldlps_n32},
  {TOP_UNDEFINED, TOP_ldlpd,	TOP_ldlpdx,	TOP_ldlpdxx,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_ldaps,	TOP_ldapsx,	TOP_ldapsxx,	TOP_ldaps_n32},
  {TOP_UNDEFINED, TOP_ldapd,	TOP_ldapdx,	TOP_ldapdxx,	TOP_ldapd_n32},
  {TOP_UNDEFINED, TOP_stlps,	TOP_stlpsx,	TOP_stlpsxx,	TOP_stlps_n32},
  {TOP_UNDEFINED, TOP_stlpd,	TOP_stlpdx,	TOP_stlpdxx,	TOP_stlpd_n32},
  {TOP_UNDEFINED, TOP_staps,	TOP_stapsx,	TOP_stapsxx,	TOP_staps_n32},
  {TOP_UNDEFINED, TOP_stapd,	TOP_stapdx,	TOP_stapdxx,	TOP_stapd_n32},
  {TOP_UNDEFINED, TOP_ldhps,	TOP_ldhpsx,	TOP_ldhpsxx,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_ldhpd,	TOP_ldhpdx,	TOP_ldhpdxx,	TOP_ldhpd_n32},
  {TOP_UNDEFINED, TOP_sthps,	TOP_sthpsx,	TOP_sthpsxx,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_sthpd,	TOP_sthpdx,	TOP_sthpdxx,	TOP_sthpd_n32},
  {TOP_UNDEFINED, TOP_ld8_64,	TOP_ldx8_64,	TOP_ldxx8_64,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_ldu8_64,	TOP_ldxu8_64,	TOP_ldxxu8_64,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_ld16_64,	TOP_ldx16_64,	TOP_ldxx16_64,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_ldu16_64,	TOP_ldxu16_64,	TOP_ldxxu16_64,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_ld8_32,	TOP_ldx8_32,	TOP_ldxx8_32,  TOP_ld8_32_n32},
  {TOP_UNDEFINED, TOP_ldu8_32,	TOP_ldxu8_32,	TOP_ldxxu8_32, TOP_ldu8_32_n32},
  {TOP_UNDEFINED, TOP_ld16_32,	TOP_ldx16_32,	TOP_ldxx16_32, TOP_ld16_32_n32},
  {TOP_UNDEFINED, TOP_ldu16_32,	TOP_ldxu16_32,	TOP_ldxxu16_32, TOP_ldu16_32_n32},
  {TOP_UNDEFINED, TOP_ld32,	TOP_ldx32,	TOP_ldxx32,	TOP_ld32_n32},
  {TOP_UNDEFINED, TOP_ld32_64,	TOP_ldx32_64,	TOP_ldxx32_64,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_ld64,	TOP_ldx64,	TOP_ldxx64,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_prefetch,	TOP_prefetchx,	TOP_prefetchxx,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_prefetchw,  TOP_prefetchwx,  TOP_prefetchwxx, TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_prefetcht0, TOP_prefetcht0x, TOP_prefetcht0xx, TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_prefetcht1, TOP_prefetcht1x, TOP_prefetcht1xx, TOP_UNDEFINED},

  // LEA
  {TOP_UNDEFINED, TOP_lea32,	TOP_leax32,	TOP_leaxx32,	TOP_UNDEFINED},
  {TOP_UNDEFINED, TOP_lea64,	TOP_leax64,	TOP_leaxx64,	TOP_UNDEFINED},

  // Load-execute.

  {TOP_add32,	TOP_addx32,	TOP_addxx32,	TOP_addxxx32,	TOP_UNDEFINED},
  {TOP_add64,	TOP_addx64,	TOP_addxx64,	TOP_addxxx64,	TOP_UNDEFINED},
  {TOP_addss,	TOP_addxss,	TOP_addxxss,	TOP_addxxxss,	TOP_UNDEFINED},
  {TOP_addsd,	TOP_addxsd,	TOP_addxxsd,	TOP_addxxxsd,	TOP_UNDEFINED},	
  {TOP_add128v8,  TOP_addx128v8,  TOP_addxx128v8,  TOP_addxxx128v8, TOP_UNDEFINED},
  {TOP_add128v16, TOP_addx128v16, TOP_addxx128v16, TOP_addxxx128v16, TOP_UNDEFINED},
  {TOP_add128v32, TOP_addx128v32, TOP_addxx128v32, TOP_addxxx128v32, TOP_UNDEFINED},
  {TOP_add128v64, TOP_addx128v64, TOP_addxx128v64, TOP_addxxx128v64, TOP_UNDEFINED},
  {TOP_fadd128v32,	TOP_faddx128v32,	TOP_faddxx128v32,	TOP_faddxxx128v32,	TOP_UNDEFINED},
  {TOP_fadd128v64,	TOP_faddx128v64,	TOP_faddxx128v64,	TOP_faddxxx128v64,	TOP_UNDEFINED},
  {TOP_fhadd128v32,	TOP_fhaddx128v32,	TOP_fhaddxx128v32,	TOP_fhaddxxx128v32,	TOP_UNDEFINED},
  {TOP_fhadd128v64,	TOP_fhaddx128v64,	TOP_fhaddxx128v64,	TOP_fhaddxxx128v64,	TOP_UNDEFINED},
  {TOP_faddsub128v32,	TOP_faddsubx128v32,	TOP_faddsubxx128v32,	TOP_faddsubxxx128v32,	TOP_UNDEFINED},
  {TOP_faddsub128v64,	TOP_faddsubx128v64,	TOP_faddsubxx128v64,	TOP_faddsubxxx128v64,	TOP_UNDEFINED},
  {TOP_fhsub128v32,	TOP_fhsubx128v32,	TOP_fhsubxx128v32,	TOP_fhsubxxx128v32,	TOP_UNDEFINED},
  {TOP_fhsub128v64,	TOP_fhsubx128v64,	TOP_fhsubxx128v64,	TOP_fhsubxxx128v64,	TOP_UNDEFINED},

  {TOP_sub32,	TOP_subx32,	TOP_subxx32,	TOP_subxxx32,	TOP_UNDEFINED},
  {TOP_sub64,	TOP_subx64,	TOP_subxx64,	TOP_subxxx64,	TOP_UNDEFINED},
  {TOP_subss,	TOP_subxss,	TOP_subxxss,	TOP_subxxxss,	TOP_UNDEFINED},
  {TOP_subsd,	TOP_subxsd,	TOP_subxxsd,	TOP_subxxxsd,	TOP_UNDEFINED},
  {TOP_sub128v8,	TOP_subx128v8,	TOP_subxx128v8,	TOP_subxxx128v8,	TOP_UNDEFINED},
  {TOP_sub128v16,	TOP_subx128v16,	TOP_subxx128v16,	TOP_subxxx128v16,	TOP_UNDEFINED},
  {TOP_sub128v32,	TOP_subx128v32,	TOP_subxx128v32,	TOP_subxxx128v32,	TOP_UNDEFINED},
  {TOP_sub128v64,	TOP_subx128v64,	TOP_subxx128v64,	TOP_subxxx128v64,	TOP_UNDEFINED},
  {TOP_fsub128v32,	TOP_fsubx128v32, TOP_fsubxx128v32,	TOP_fsubxxx128v32,	TOP_UNDEFINED},
  {TOP_fsub128v64,	TOP_fsubx128v64, TOP_fsubxx128v64,	TOP_fsubxxx128v64,	TOP_UNDEFINED},

  {TOP_mulss,	TOP_mulxss,	TOP_mulxxss,	TOP_mulxxxss,	TOP_UNDEFINED},
  {TOP_mulsd,	TOP_mulxsd,	TOP_mulxxsd,	TOP_mulxxxsd,	TOP_UNDEFINED},
  {TOP_fmul128v32, TOP_fmulx128v32, TOP_fmulxx128v32, TOP_fmulxxx128v32,	TOP_UNDEFINED},
  {TOP_fmul128v64, TOP_fmulx128v64, TOP_fmulxx128v64, TOP_fmulxxx128v64,	TOP_UNDEFINED},
  {TOP_cmpgt128v8, TOP_cmpgtx128v8, TOP_cmpgtxx128v8, TOP_cmpgtxxx128v8,	TOP_UNDEFINED},
  {TOP_cmpgt128v16,	TOP_cmpgtx128v16,	TOP_cmpgtxx128v16,	TOP_cmpgtxxx128v16,	TOP_UNDEFINED},
  {TOP_cmpgt128v32,	TOP_cmpgtx128v32,	TOP_cmpgtxx128v32,	TOP_cmpgtxxx128v32,	TOP_UNDEFINED},
  {TOP_cmpeq128v8,	TOP_cmpeqx128v8,	TOP_cmpeqxx128v8,	TOP_cmpeqxxx128v8,	TOP_UNDEFINED},
  {TOP_cmpeq128v16,	TOP_cmpeqx128v16,	TOP_cmpeqxx128v16,	TOP_cmpeqxxx128v16,	TOP_UNDEFINED},
  {TOP_cmpeq128v32,	TOP_cmpeqx128v32,	TOP_cmpeqxx128v32,	TOP_cmpeqxxx128v32,	TOP_UNDEFINED},
  {TOP_max128v8,	TOP_maxx128v8,	TOP_maxxx128v8,	TOP_maxxxx128v8,	TOP_UNDEFINED},
  {TOP_max128v16,	TOP_maxx128v16,	TOP_maxxx128v16, TOP_maxxxx128v16,	TOP_UNDEFINED},
  {TOP_min128v8,	TOP_minx128v8,	TOP_minxx128v8,	TOP_minxxx128v8,	TOP_UNDEFINED},
  {TOP_min128v16,	TOP_minx128v16,	TOP_minxx128v16, TOP_minxxx128v16,	TOP_UNDEFINED},
  {TOP_divss,	TOP_divxss,	TOP_divxxss,	TOP_divxxxss,	TOP_UNDEFINED},
  {TOP_divsd,	TOP_divxsd,	TOP_divxxsd,	TOP_divxxxsd,	TOP_UNDEFINED},
  {TOP_fdiv128v32,	TOP_fdivx128v32,	TOP_fdivxx128v32,	TOP_fdivxxx128v32,	TOP_UNDEFINED},
  {TOP_fdiv128v64,	TOP_fdivx128v64,	TOP_fdivxx128v64,	TOP_fdivxxx128v64,	TOP_UNDEFINED},

  {TOP_and8,	TOP_andx8,	TOP_andxx8,	TOP_andxxx8,	TOP_UNDEFINED},
  {TOP_and16,	TOP_andx16,	TOP_andxx16,	TOP_andxxx16,	TOP_UNDEFINED},
  {TOP_and32,	TOP_andx32,	TOP_andxx32,	TOP_andxxx32,	TOP_UNDEFINED},
  {TOP_and64,	TOP_andx64,	TOP_andxx64,	TOP_andxxx64,	TOP_UNDEFINED},
  {TOP_and128v8,	TOP_andx128v8,	TOP_andxx128v8,	TOP_andxxx128v8,	TOP_UNDEFINED},
  {TOP_and128v16,	TOP_andx128v16,	TOP_andxx128v16,	TOP_andxxx128v16,	TOP_UNDEFINED},
  {TOP_and128v32,	TOP_andx128v32,	TOP_andxx128v32,	TOP_andxxx128v32,	TOP_UNDEFINED },
  {TOP_and128v64,	TOP_andx128v64,	TOP_andxx128v64,	TOP_andxxx128v64,	TOP_UNDEFINED},
  {TOP_fand128v32,	TOP_fandx128v32,	TOP_fandxx128v32,	TOP_fandxxx128v32,	TOP_UNDEFINED},
  {TOP_fand128v64,	TOP_fandx128v64,	TOP_fandxx128v64,	TOP_fandxxx128v64,	TOP_UNDEFINED},
  // andps/andpd share the same load-execute OPs as fand128v32/fand128v64.
  // Must put andps/andpd after fand128v32/fand128v64 so that the load-execute
  // OPs will have fand128v32/fand128v64 as the base mode.
  {TOP_andps,	TOP_fandx128v32,	TOP_fandxx128v32,	TOP_fandxxx128v32,	TOP_UNDEFINED},
  {TOP_andpd,	TOP_fandx128v64,	TOP_fandxx128v64,	TOP_fandxxx128v64,	TOP_UNDEFINED},

  {TOP_or8,	TOP_orx8,	TOP_orxx8,	TOP_orxxx8,	TOP_UNDEFINED},
  {TOP_or16,	TOP_orx16,	TOP_orxx16,	TOP_orxxx16,	TOP_UNDEFINED},
  {TOP_or32,	TOP_orx32,	TOP_orxx32,	TOP_orxxx32,	TOP_UNDEFINED},
  {TOP_or64,	TOP_orx64,	TOP_orxx64,	TOP_orxxx64,	TOP_UNDEFINED},
  {TOP_or128v8,	TOP_orx128v8,	TOP_orxx128v8,	TOP_orxxx128v8,	TOP_UNDEFINED},
  {TOP_or128v16,	TOP_orx128v16,	TOP_orxx128v16,	TOP_orxxx128v16,	TOP_UNDEFINED},
  {TOP_or128v32,	TOP_orx128v32,	TOP_orxx128v32,	TOP_orxxx128v32,	TOP_UNDEFINED},
  {TOP_or128v64,	TOP_orx128v64,	TOP_orxx128v64,	TOP_orxxx128v64,	TOP_UNDEFINED},
  {TOP_for128v32,	TOP_forx128v32,	TOP_forxx128v32,	TOP_forxxx128v32,	TOP_UNDEFINED},
  {TOP_for128v64,	TOP_forx128v64,	TOP_forxx128v64,	TOP_forxxx128v64,	TOP_UNDEFINED},
  // orps/orpd share the same load-execute OPs as for128v32/for128v64.  Must
  // put orps/orpd after for128v32/for128v64 so that the load-execute OPs will
  // have for128v32/for128v64 as the base mode.
  {TOP_orps,	TOP_forx128v32,	TOP_forxx128v32,	TOP_forxxx128v32,	TOP_UNDEFINED},
  {TOP_orpd,	TOP_forx128v64,	TOP_forxx128v64,	TOP_forxxx128v64,	TOP_UNDEFINED},

  {TOP_xor8,	TOP_xorx8,	TOP_xorxx8,	TOP_xorxxx8,	TOP_UNDEFINED},
  {TOP_xor16,	TOP_xorx16,	TOP_xorxx16,	TOP_xorxxx16,	TOP_UNDEFINED},
  {TOP_xor32,	TOP_xorx32,	TOP_xorxx32,	TOP_xorxxx32,	TOP_UNDEFINED},
  {TOP_xor64,	TOP_xorx64,	TOP_xorxx64,	TOP_xorxxx64,	TOP_UNDEFINED},
  {TOP_xor128v8,	TOP_xorx128v8,	TOP_xorxx128v8,	TOP_xorxxx128v8,	TOP_UNDEFINED},
  {TOP_xor128v16,	TOP_xorx128v16,	TOP_xorxx128v16,	TOP_xorxxx128v16,	TOP_UNDEFINED},
  {TOP_xor128v32,	TOP_xorx128v32,	TOP_xorxx128v32,	TOP_xorxxx128v32,	TOP_UNDEFINED},
  {TOP_xor128v64,	TOP_xorx128v64,	TOP_xorxx128v64,	TOP_xorxxx128v64,	TOP_UNDEFINED},
  {TOP_fxor128v32,	TOP_fxorx128v32,	TOP_fxorxx128v32,	TOP_fxorxxx128v32,	TOP_UNDEFINED},
  {TOP_fxor128v64,	TOP_fxorx128v64,	TOP_fxorxx128v64,	TOP_fxorxxx128v64,	TOP_UNDEFINED},
  // xorps/xorpd share the same load-execute OPs as fxor128v32/fxor128v64.
  // Must put xorps/xorpd after fxor128v32/fxor128v64 so that the load-execute
  // OPs will have fxor128v32/fxor128v64 as the base mode.
  {TOP_xorps,	TOP_fxorx128v32,	TOP_fxorxx128v32,	TOP_fxorxxx128v32,	TOP_UNDEFINED},
  {TOP_xorpd,	TOP_fxorx128v64,	TOP_fxorxx128v64,	TOP_fxorxxx128v64,	TOP_UNDEFINED},

  {TOP_fmax128v32,	TOP_fmaxx128v32,	TOP_fmaxxx128v32,	TOP_fmaxxxx128v32,	TOP_UNDEFINED},
  {TOP_fmax128v64,	TOP_fmaxx128v64,	TOP_fmaxxx128v64,	TOP_fmaxxxx128v64,	TOP_UNDEFINED},
  {TOP_fmin128v32,	TOP_fminx128v32,	TOP_fminxx128v32,	TOP_fminxxx128v32,	TOP_UNDEFINED},
  {TOP_fmin128v64,	TOP_fminx128v64,	TOP_fminxx128v64,	TOP_fminxxx128v64,	TOP_UNDEFINED},

  {TOP_cmp8,	TOP_cmpx8,	TOP_cmpxx8,	TOP_cmpxxx8,	TOP_UNDEFINED},
  {TOP_cmp16,	TOP_cmpx16,	TOP_cmpxx16,	TOP_cmpxxx16,	TOP_UNDEFINED},
  {TOP_cmp32,	TOP_cmpx32,	TOP_cmpxx32,	TOP_cmpxxx32,	TOP_UNDEFINED},
  {TOP_cmp64,	TOP_cmpx64,	TOP_cmpxx64,	TOP_cmpxxx64,	TOP_UNDEFINED},

  {TOP_cmpi8,	TOP_cmpxi8,	TOP_cmpxxi8,	TOP_cmpxxxi8,	TOP_UNDEFINED},
  {TOP_cmpi16,	TOP_cmpxi16,	TOP_cmpxxi16,	TOP_cmpxxxi16,	TOP_UNDEFINED},
  {TOP_cmpi32,	TOP_cmpxi32,	TOP_cmpxxi32,	TOP_cmpxxxi32,	TOP_UNDEFINED},
  {TOP_cmpi64,	TOP_cmpxi64,	TOP_cmpxxi64,	TOP_cmpxxxi64,	TOP_UNDEFINED},

  {TOP_test32,	TOP_testx32,	TOP_testxx32,	TOP_testxxx32,	TOP_UNDEFINED},
  {TOP_test64,	TOP_testx64,	TOP_testxx64,	TOP_testxxx64,	TOP_UNDEFINED},
  {TOP_comiss,	TOP_comixss,	TOP_comixxss,	TOP_comixxxss,	TOP_UNDEFINED},
  {TOP_comisd,	TOP_comixsd,	TOP_comixxsd,	TOP_comixxxsd,	TOP_UNDEFINED},

  {TOP_icall,	TOP_icallx,	TOP_icallxx,	TOP_icallxxx,	TOP_UNDEFINED},
  {TOP_ijmp,	TOP_ijmpx,	TOP_ijmpxx,	TOP_ijmpxxx,	TOP_UNDEFINED},

  {TOP_cvtsd2ss,	TOP_cvtsd2ss_x,	TOP_cvtsd2ss_xx,	TOP_cvtsd2ss_xxx,	TOP_UNDEFINED},
  {TOP_cvtsi2ss,	TOP_cvtsi2ss_x,	TOP_cvtsi2ss_xx,	TOP_cvtsi2ss_xxx,	TOP_UNDEFINED},
  {TOP_cvtsi2ssq,	TOP_cvtsi2ssq_x,	TOP_cvtsi2ssq_xx,	TOP_cvtsi2ssq_xxx,	TOP_UNDEFINED},
  {TOP_cvtsi2sd,	TOP_cvtsi2sd_x,	TOP_cvtsi2sd_xx,	TOP_cvtsi2sd_xxx,	TOP_UNDEFINED},
  {TOP_cvtsi2sdq,	TOP_cvtsi2sdq_x,	TOP_cvtsi2sdq_xx,	TOP_cvtsi2sdq_xxx,	TOP_UNDEFINED},

  {TOP_cvtdq2pd,	TOP_cvtdq2pd_x,	TOP_cvtdq2pd_xx,	TOP_cvtdq2pd_xxx,	TOP_UNDEFINED},
  {TOP_cvtdq2ps,	TOP_cvtdq2ps_x,	TOP_cvtdq2ps_xx,	TOP_cvtdq2ps_xxx,	TOP_UNDEFINED},
 {TOP_cvtps2pd,		TOP_cvtps2pd_x,	TOP_cvtps2pd_xx,	TOP_cvtps2pd_xxx,	TOP_UNDEFINED},
  {TOP_cvtpd2ps,	TOP_cvtpd2ps_x,	TOP_cvtpd2ps_xx,	TOP_cvtpd2ps_xxx,	TOP_UNDEFINED},
  {TOP_cvttps2dq,	TOP_cvttps2dq_x,	TOP_cvttps2dq_xx,	TOP_cvttps2dq_xxx,	TOP_UNDEFINED},
  {TOP_cvttpd2dq,	TOP_cvttpd2dq_x,	TOP_cvttpd2dq_xx,	TOP_cvttpd2dq_xxx,	TOP_UNDEFINED}
};

// Associate an address mode group to an opcode.
static void
Add_Addr_Mode_Group (TOP top, Addr_Mode_Group *address_mode_group)
{
  if (top == TOP_UNDEFINED)
    return;

  // Don't redefine the address group for TOP if it is already defined.  This
  // is so that if TOP appears in multiple groups, we use only the first group,
  // which is assumed to be the most authoritative for TOP.
  if (Top_To_Addr_Mode_Group[top] == NULL)
    Top_To_Addr_Mode_Group[top] = address_mode_group;
}

// Build the Top_To_Addr_Modes table.
static void
Init_Addr_Modes()
{
  int i;
  ADDR_MODE mode;
  static bool table_is_initialized = false;

  if( table_is_initialized )
    return;

  table_is_initialized = true;

  for (i = 0; i < TOP_UNDEFINED; i++) {
    Top_To_Addr_Mode_Group[i] = NULL;
  }

  // Make sure the code below is in sync with the number of address modes.
  // Currently the last mode is N32_Mode.
  ADDR_MODE last_mode = N32_MODE;
  ADDR_MODE undefined_mode = UNDEFINED_MODE;
  FmtAssert(1 + (int)last_mode == (int) undefined_mode,
	    ("Init_Addr_Modes: some address modes not handled"));
  
  UINT table_size = sizeof(Addr_Mode_Group_Table) / sizeof(Addr_Mode_Group);
  for (i = 0; i < table_size; i++) {
    Addr_Mode_Group *group = &Addr_Mode_Group_Table[i];

    Add_Addr_Mode_Group(group->reg_mode, group);
    Add_Addr_Mode_Group(group->base_mode, group);
    Add_Addr_Mode_Group(group->base_index_mode, group);
    Add_Addr_Mode_Group(group->index_mode, group);
    Add_Addr_Mode_Group(group->n32_mode, group);
  }
}

static TOP
Get_Top_For_Addr_Mode (TOP top, ADDR_MODE mode)
{
  Addr_Mode_Group *group = Top_To_Addr_Mode_Group[top];
  if (group != NULL) {
    switch (mode) {
      case BASE_MODE:		return group->base_mode;
      case BASE_INDEX_MODE:	return group->base_index_mode;
      case INDEX_MODE:		return group->index_mode;
      case N32_MODE:		return group->n32_mode;
    }
    FmtAssert(FALSE, ("Get_Top_For_Addr_Mode: address mode not handled"));
  }
  return TOP_UNDEFINED;
}

static OP *
Compose_Mem_Op( OP* op, TN* index, TN* offset, TN* scale, TN* base )
{
  Is_True( offset != NULL, ("Compose_Mem_Op: offset is NULL") );

  OP* new_op = NULL;
  ADDR_MODE mode = N32_MODE;

  if (index != NULL)
    mode = base == NULL ? INDEX_MODE : BASE_INDEX_MODE;
  else if (base != NULL)
    mode = BASE_MODE;

  const TOP new_top = Get_Top_For_Addr_Mode(OP_code(op), mode);

  FmtAssert( new_top != TOP_UNDEFINED, ("Compose_Mem_Op: unknown top") );

  if( TOP_is_prefetch( new_top ) ){
    if( mode == INDEX_MODE )
      new_op = Mk_OP( new_top, OP_opnd( op, 0 ), index, scale, offset );
    else
      new_op = Mk_OP( new_top, OP_opnd( op, 0 ), base, offset, index, scale );

  } else {
    TN* storeval = NULL;

    if( TOP_is_store(new_top) ){
      storeval = OP_opnd( op, OP_find_opnd_use( op, OU_storeval ) );
    } else {
      storeval = OP_result( op, 0 );
    }

    if( new_top == TOP_leax64 ){
      Is_True(mode != N32_MODE, ("Compose_Mem_Op: unexpected address mode"));
      if( mode == INDEX_MODE )
	new_op = Mk_OP( new_top, storeval, index, scale, offset );
      else
	new_op = Mk_OP( new_top, storeval, base, index, scale, offset );
    } else {
      if (mode == N32_MODE)
	new_op = Mk_OP( new_top, storeval, offset );
      else if (mode == INDEX_MODE)
	new_op = Mk_OP( new_top, storeval, index, scale, offset );
      else
	new_op = Mk_OP( new_top, storeval, base, offset, index, scale );    
    }
  }

  return new_op;
}


// Compose a memory OP that looks like OP but with the address mode specified
// by OFFSET/BASE/INDEX/SCALE.  Copy OP's info to the new OP.
static OP *
Compose_Mem_Op_And_Copy_Info(OP* op, TN* index_tn, TN* offset_tn, TN* scale_tn,
			     TN* base_tn, EBO_TN_INFO **actual_tninfo)
{
  OP *new_op = Compose_Mem_Op(op, index_tn, offset_tn, scale_tn, base_tn);

  Copy_WN_For_Memory_OP(new_op, op);
  if (OP_volatile(op)) // Bug 4245 - copy "volatile" flag
    Set_OP_volatile(new_op);
  OP_srcpos(new_op) = OP_srcpos(op);

  Set_OP_unrolling(new_op, OP_unrolling(op));
  Set_OP_orig_idx(new_op, OP_map_idx(op));
  Set_OP_unroll_bb(new_op, OP_unroll_bb(op));

  if (EBO_in_loop) {
    CG_LOOP_Init_Op(new_op);
    const INT op_base_idx = OP_find_opnd_use(op, OU_base);
    EBO_TN_INFO *base_tninfo = op_base_idx >= 0 ?
				 actual_tninfo[op_base_idx] : NULL;
    if (base_tninfo != NULL && base_tninfo->omega != 0) {
      Set_OP_omega(new_op, OP_find_opnd_use(new_op, OU_base),
		   base_tninfo->omega);
    }

    if (index_tn != NULL) {
      EBO_TN_INFO *tninfo = get_tn_info(index_tn);
      if (tninfo != NULL && tninfo->omega != 0) {
	Set_OP_omega(new_op, OP_find_opnd_use(new_op, OU_index), tninfo->omega);
      }
    }
  }

  return new_op;
}


BOOL EBO_Merge_Memory_Addr( OP* op,
			    TN** opnd_tn,
			    EBO_TN_INFO** opnd_tninfo,
			    EBO_TN_INFO** actual_tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_MERGE_MEMORY_ADDR)) return FALSE;
#endif
  if( EBO_in_peep ){
    return FALSE;
  }

  if (Get_Top_For_Addr_Mode(OP_code(op), BASE_MODE) == TOP_UNDEFINED) {
    return FALSE;
  }

  const INT op_base_idx = OP_find_opnd_use( op, OU_base );
  EBO_TN_INFO* base_tninfo = op_base_idx >= 0 ? actual_tninfo[op_base_idx] : NULL;
  OP* addr_op = (base_tninfo != NULL) ? base_tninfo->in_op : NULL;
  bool pass = false;
  TN* index_tn  = NULL;
  TN* offset_tn = NULL;
  TN* scale_tn  = NULL;
  TN* base_tn   = NULL;

  // First, try to subsume the base.
  if( addr_op != NULL && !OP_load(addr_op) && !OP_simulated(addr_op) ){
    pass = Compose_Addr( op, base_tninfo, OU_base, opnd_tn,
			 &index_tn, &offset_tn, &scale_tn, &base_tn );
  }

  // Otherwise, try to subsume the index.
  if( !pass ){
    const INT op_index_idx = OP_find_opnd_use( op, OU_index );
    if( op_index_idx < 0 )
      return FALSE;

    EBO_TN_INFO* index_tninfo = actual_tninfo[op_index_idx];
    if( index_tninfo == NULL || index_tninfo->in_op == NULL )
      return FALSE;

    addr_op = index_tninfo->in_op;

    if( addr_op == NULL || OP_load(addr_op) || OP_simulated(addr_op) )
      return FALSE;

    if( !Compose_Addr( op, index_tninfo, OU_index, opnd_tn,
		       &index_tn, &offset_tn, &scale_tn, &base_tn ) )
      return FALSE;
  }

  // TODO: support N32_MODE for -m32
  if( base_tn == NULL && index_tn == NULL )
    return FALSE;

  TN* rip = Rip_TN();
  if ( index_tn == rip || base_tn == rip )
    return FALSE;

  OP* new_op = Compose_Mem_Op_And_Copy_Info(op, index_tn, offset_tn, scale_tn,
					    base_tn, actual_tninfo);

  BB_Insert_Op_After( OP_bb(op), op, new_op);

  if( EBO_Trace_Optimization ){
    #pragma mips_frequency_hint NEVER
    fprintf( TFile,
	     "%sin BB:%d merge memory addr expression (from BB:%d) with offset (in BB:%d)\n",
	     EBO_trace_pfx, BB_id(OP_bb(op)),BB_id(OP_bb(addr_op)),BB_id(OP_bb(op)) );
    Print_OP_No_SrcLine(addr_op);
    Print_OP_No_SrcLine(op);

    fprintf(TFile, "  to insert the new op:\n");
    Print_OP_No_SrcLine(new_op);
    fprintf( TFile, "\n" );
  }

  return TRUE;
}


/* return TRUE if <ex_op> is safe to perform an 8-bit or 16-bit operation;
   the original <ex_op> performs a 32-bit or 64-bit operation.
 */
static BOOL Check_loadbw_execute( int ld_bytes, OP* ex_op )
{
  if( !CG_loadbw_execute ||
      ld_bytes > 2       ||
      OP_opnds( ex_op ) != 2 )
    return FALSE;

  if( TN_size(OP_result(ex_op,0)) == ld_bytes )
    return TRUE;

  /* Check all the opnds of <ex_op> to make sure it is safe for
     <ex_op> to perform 8-bit or 16-bit operation.  (bug#131)
  */

  OP* ld_op[] = { NULL, NULL };

  for( int i = 0; i < OP_opnds( ex_op ); i++ ){
    TN* opnd = OP_opnd( ex_op, i );
    if( TN_is_register( opnd ) ){
      const EBO_TN_INFO* opnd_info = get_tn_info( opnd );
      if( opnd_info == NULL ||
	  opnd_info->in_op == NULL )
	return FALSE;

      OP* pred_op = opnd_info->in_op;

      if( OP_icmp(ex_op) &&
	  ( TOP_is_load_ext(OP_code(pred_op)) ||
	    TOP_is_move_ext(OP_code(pred_op)) ) ){
	/* If the opnd is coming from a zero-extension operation,
	   then don't consider using a shorter format for <ex_op>, because
	   we don't know the run-time msb value of this opnd, and we do not
	   have unsigned cmp. (bug#2197)
	 */
	struct SIZE_EXT_INFO pred_size_ext_info;
	Get_Size_Ext_Info( OP_code(pred_op), &pred_size_ext_info );
	if (!pred_size_ext_info.sign_ext) {
	  // If the consumer of rsp cares only about equality/inequality, then
	  // it is ok to use 8-bit cmp.
	  OP *op;
	  for (op = OP_next(ex_op); op != NULL; op = OP_next(op)) {
	    TOP top = OP_code(op);
	    if (OP_reads_rflags(op) &&
		top != TOP_je &&
		top != TOP_jne &&
		top != TOP_sete &&
		top != TOP_setne &&
		top != TOP_cmove &&
		top != TOP_cmovne) {
	      return FALSE;
	    }
	    if (TOP_is_change_rflags(top))
	      break;
	  }
	}
      }

      if( OP_load( pred_op ) ){
	if( CGTARG_Mem_Ref_Bytes( pred_op ) != ld_bytes )
	  return FALSE;
	ld_op[i] = pred_op;	
      }

    } else if( TN_has_value( opnd ) ){
      const INT64 value = TN_value( opnd );
      if( ( ld_bytes == 1 &&
	    !ISA_LC_Value_In_Class(value, LC_simm8) ) ||
	  ( ld_bytes == 2 &&
	    !ISA_LC_Value_In_Class(value, LC_simm16) ) )
	return FALSE;
    }
  }

  /* If both opnds are coming from load operations, make sure they have the same
     signness.
   */
  if( ld_op[0] != NULL &&
      ld_op[1] != NULL ){
    struct SIZE_EXT_INFO info0;
    struct SIZE_EXT_INFO info1;

    Get_Size_Ext_Info( OP_code(ld_op[0]), &info0 );
    Get_Size_Ext_Info( OP_code(ld_op[1]), &info1 );

    if( info0.sign_ext != info1.sign_ext )
      return FALSE;
  }

  return TRUE;
}


// What is the load-execute instruction corresponding to 
// load "op" and execute "ex_op"
static TOP Load_Execute_Format( OP* ld_op, OP* ex_op, ADDR_MODE mode )
{
  TOP new_top = OP_code( ex_op );
  const int ld_bytes = CGTARG_Mem_Ref_Bytes( ld_op );

  if( Check_loadbw_execute( ld_bytes, ex_op ) ){

    switch( OP_code(ex_op) ){
    case TOP_cmp32: 
      if( ld_bytes == 1 )
	new_top = TOP_cmp8;
      else if( ld_bytes == 2 )
	new_top = TOP_cmp16;
      break;

    case TOP_cmpi32:
      if( ld_bytes == 1 )
	new_top = TOP_cmpi8;
      else if( ld_bytes == 2 )
	new_top = TOP_cmpi16;
      break;
      
    case TOP_xor32:
      if( ld_bytes == 1 )
	new_top = TOP_xor8;
      else if( ld_bytes == 2 )
	new_top = TOP_xor16;
      break;

    case TOP_and32: 
      if( ld_bytes == 1 )
	new_top = TOP_and8;
      else if( ld_bytes == 2 )
	new_top = TOP_and16;
      break;
	
    case TOP_or32: 
      if( ld_bytes == 1 )
	new_top = TOP_or8;
      else if( ld_bytes == 2 )
	new_top = TOP_or16;
      break;
    }
  }

  new_top = Get_Top_For_Addr_Mode(new_top, mode);
  Is_True( new_top != TOP_UNDEFINED, ("Load_Execute_Format: NYI (1)") );
  OP* fake_new_op = Mk_OP( new_top, NULL, NULL, NULL, NULL, NULL, NULL );
  
  if( CGTARG_Mem_Ref_Bytes( fake_new_op ) > ld_bytes )
    new_top = TOP_UNDEFINED;

  /* Not all the registers are 8-bit addressable under i386 target.
   */
  if( Is_Target_32bit() &&
      EBO_in_peep       &&
      OP_opnd_size( fake_new_op, 0 ) == 8 ){
    TN* opnd = OP_opnd( ex_op, 0 );
    const ISA_REGISTER_CLASS cl = TN_register_class(opnd);
    const REGISTER reg = TN_register(opnd);

    if( !REGISTER_SET_MemberP( REGISTER_CLASS_eight_bit_regs(cl), reg ) )
      new_top = TOP_UNDEFINED;
  }

  return new_top;
}


static BOOL test_is_replaced( OP* alu_op, OP* test_op, const EBO_TN_INFO* tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_TEST_IS_REPLACED)) return FALSE;
#endif
  if( alu_op == NULL ||
      OP_bb( alu_op ) != OP_bb( test_op ) )
    return FALSE;

  OP* new_op = NULL;
  TOP top = OP_code( alu_op );

  if( !TOP_is_iadd( top ) &&
      !TOP_is_isub( top ) &&
      !TOP_is_ior( top )  &&
      !TOP_is_ixor( top ) &&
      !TOP_is_iand( top ) ){

    // The conversion from test -> cmpi was intended to facilitate address 
    // folding and bug 5517 has proved that it is detrimental to performance.
    if (CG_use_test)
      return FALSE;

    /* Change a cmp to test if the opnds are from a load, so that more folding
       could happen later.
    */
    if( OP_load( alu_op ) &&
	!TN_live_out_of( OP_result(alu_op,0), OP_bb(test_op) ) ){
      new_op = Mk_OP( OP_code(test_op) == TOP_test64 ? TOP_cmpi64 : TOP_cmpi32,
		      Rflags_TN(), OP_opnd(test_op,0), Gen_Literal_TN( 0, 4) );
      Set_OP_unrolling( new_op, OP_unrolling(test_op) );
      Set_OP_orig_idx( new_op, OP_map_idx(test_op) );
      Set_OP_unroll_bb( new_op, OP_unroll_bb(test_op) );
      
      Copy_WN_For_Memory_OP( new_op, test_op );
      if ( OP_volatile( test_op ) )
        Set_OP_volatile( new_op );
      OP_srcpos( new_op ) = OP_srcpos( test_op );
      BB_Insert_Op_After( OP_bb(test_op), test_op, new_op );

      return TRUE;
    } 

    return FALSE;
  }

  /* Fix bug#963 and bug#1054
     A test is redundant only if the source is coming from an AND operation.
     TODO:
     Add TOP_js and TOP_jns so that more test or cmp operations can be removed.
   */
  if( !TOP_is_iand( top ) )
    return FALSE;

  if( !TOP_is_change_rflags(top) )
    return FALSE;

  /* TODO:
     Handle <alu_op> with memory operand.
   */
  if( OP_opnds( alu_op ) > 2 )
    return FALSE;

  const TN* src1 = OP_opnd( alu_op, 0 );
  const TN* src2 = OP_opnd( alu_op, 1 );

  bool test_is_redundant = false;

  /* Make sure the operands are not re-defined after <alu_op>.
   */
  EBO_TN_INFO* src_info = get_tn_info( src1 );
  if( src_info != NULL ){
    if( src_info->sequence_num > tninfo->sequence_num )
      return FALSE;

    if( EBO_in_loop )
      return FALSE;

    OP* next = OP_next( alu_op );
    while( next != test_op ){
      const TOP top = OP_code(next);
      if( TOP_is_change_rflags( top ) )
	break;
      next = OP_next( next );
    }
 
    test_is_redundant = next == test_op;

    /* Don't pass case like
         a = a - 1
	 test a, a
       to later phase.
    */
    if( src_info->sequence_num == tninfo->sequence_num )
      return test_is_redundant;
  }

  if( TN_is_register( src2 ) ){
    src_info = get_tn_info( src2 );
    if( src_info != NULL && src_info->sequence_num >= tninfo->sequence_num ){
      return test_is_redundant;
    }
  }

  /* Change <test_op> according to different <alu_op>. */
  top = OP_code(test_op);

  if( OP_iand( alu_op ) ){
    if( TN_has_value(src2) ){
      new_op = Mk_OP( top == TOP_test64 ? TOP_testi64 : TOP_testi32,
		      Rflags_TN(), src1, src2 );
    } else
      new_op = Mk_OP( top, Rflags_TN(), src1, src2 );

  } else if( OP_iadd( alu_op ) ){
    if( TN_has_value(src2) &&
	TN_value(src2) < 0 ){
      new_op = Mk_OP( top == TOP_test64 ? TOP_cmpi64 : TOP_cmpi32,
		      Rflags_TN(), src1, Gen_Literal_TN( -TN_value(src2), 4) );
    }

  } else if( OP_isub( alu_op ) ){
    Is_True( TN_is_register(src2), ("test_is_replaced: NYI (1)") );
    new_op = Mk_OP( top == TOP_test64 ? TOP_cmp64 : TOP_cmp32,
		    Rflags_TN(), src1, src2 );
  }

  /* Generate new operation to replace <test_op>. */

  if( new_op != NULL ){
    Set_OP_unrolling( new_op, OP_unrolling(test_op) );
    Set_OP_orig_idx( new_op, OP_map_idx(test_op) );
    Set_OP_unroll_bb( new_op, OP_unroll_bb(test_op) );
      
    Copy_WN_For_Memory_OP( new_op, test_op );
    if ( OP_volatile( test_op ) )
      Set_OP_volatile( new_op );
    OP_srcpos( new_op ) = OP_srcpos( test_op );
    BB_Insert_Op_After( OP_bb(test_op), test_op, new_op );
      
    if( EBO_Trace_Data_Flow ){
#pragma mips_frequency_hint NEVER
      fprintf( TFile, "Special_Sequence replaces " );
      Print_OP_No_SrcLine( test_op );
      fprintf( TFile, "                   with   " );
      Print_OP_No_SrcLine( new_op );
    }

    return TRUE;
  }

  return test_is_redundant;
}


/* A folded load op and an alu op could across <store_op> that will alias
   with the load op. Thus, we need to update the op_must_not_be_moved field
   for all the load ops preceeding <store_op>.
*/
void Update_op_must_not_be_moved( OP* store_op, EBO_TN_INFO** opnd_tninfo )
{
  if( Alias_Manager == NULL )
    return;

  if( OP_has_implicit_interactions(store_op) || OP_unalign_mem(store_op) )
    return;

  Is_True( OP_store(store_op),
	   ("Update_op_must_not_be_moved: must be a store operation") );

  const INT hash_value = EBO_hash_op( store_op, opnd_tninfo );
  EBO_OP_INFO* opinfo = EBO_opinfo_table[hash_value];

  while( opinfo != NULL ){
    OP* load_op = opinfo->in_op;
    if( !opinfo->op_must_not_be_moved &&
	load_op != NULL &&
	OP_load( load_op ) ){
      WN* pred_wn = OP_hoisted(load_op)  ? NULL : Get_WN_From_Memory_OP(load_op);
      WN* succ_wn = OP_hoisted(store_op) ? NULL : Get_WN_From_Memory_OP(store_op);

      if( (pred_wn != NULL) && (succ_wn != NULL) ){
	const ALIAS_RESULT result = Aliased( Alias_Manager, pred_wn, succ_wn );
	if( result == POSSIBLY_ALIASED || result == SAME_LOCATION ){
	  opinfo->op_must_not_be_moved = TRUE;	  
	}
      }
    }

    opinfo = opinfo->same;    
  }
}

static hTN_MAP32 _load_exec_map = NULL;


/* Limitations of load_exec module:
   (1) Cannot handle loads which across bbs, given the way that ebo works;
   (2) Cannot keep track of TNs which are defined more than once, since loads
       can be removed and then created by other optimizations.
*/
void Init_Load_Exec_Map( BB* bb, MEM_POOL* pool )
{
  _load_exec_map = hTN_MAP32_Create( pool );
  // Map each register to the TN that was last defined into the register.
  TN *last_TN[ISA_REGISTER_CLASS_MAX+1][REGISTER_MAX+1];

  if (EBO_in_peep) {
    memset(last_TN, 0,
	   sizeof(TN*) * (ISA_REGISTER_CLASS_MAX + 1) * (REGISTER_MAX + 1));
  }

  OP* op = NULL;
  FOR_ALL_BB_OPs_FWD( bb, op ){
    if( OP_load( op ) ){
      TN* tn = OP_result( op, 0 );

      // If a load op writes to <tn> which is live out of <bb>, then there is
      // no need to perform load_exe on this load op.  (Before register
      // allocation case.)
      if (!EBO_in_peep &&
	  TN_live_out_of(tn, bb)) {
	hTN_MAP32_Set( _load_exec_map, tn, CG_load_execute + 2 );

      } else {
	const INT32 uses = hTN_MAP32_Get( _load_exec_map, tn ) - 1;
	// If TN is loaded more than once in BB, and if an earlier load has
	// more than CG_load_execute number of uses, then don't reset the
	// number of uses.  This will disable load-execution for all loads of
	// the TN.  (A limitation of the current code is that it cannot track
	// separate loads of the same TN separately.)
	if( uses  <= CG_load_execute ){
	  hTN_MAP32_Set( _load_exec_map, tn, 1 );
	}
      }
    }

    for( int i = 0; i < OP_opnds(op); i++ ){
      TN* opnd = OP_opnd( op, i );
      if( TN_is_register( opnd ) ){
	const INT32 uses = hTN_MAP32_Get( _load_exec_map, opnd );
	if( uses > 0 ){
	  hTN_MAP32_Set( _load_exec_map, opnd, uses+1 );
	}
      }
    }

    // Record which registers are used by the OP's result TNs.
    if (EBO_in_peep) {
      for (int i = 0; i < OP_results(op); i++) {
	TN *tn = OP_result(op, i);
	last_TN[TN_register_class(tn)][TN_register(tn)] = tn;
      }
    }
  }

  // Disable load-execution for live-out TNs.  (After register allocation
  // case.)
  if (EBO_in_peep) {
    ISA_REGISTER_CLASS cl;
    FOR_ALL_ISA_REGISTER_CLASS(cl) {
      for (REGISTER reg = 0; reg <= REGISTER_MAX; reg++) {
	if (last_TN[cl][reg] &&
	    REG_LIVE_Outof_BB(cl, reg, bb)) {
	  hTN_MAP32_Set(_load_exec_map, last_TN[cl][reg], CG_load_execute + 2);
	}
      }
    }
  }
}


BOOL EBO_Load_Execution( OP* alu_op, TN** opnd_tn, EBO_TN_INFO** actual_tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_LOAD_EXECUTION)) return FALSE;
#endif
  const TOP top = OP_code(alu_op);

  if( top == TOP_xor64 ||
      top == TOP_or64  ||
      top == TOP_and64 ||
      top == TOP_cmp64 ||
      top == TOP_test64 )
    return FALSE;

  if (TOP_is_load_exe(top) ||		// Skip if TOP is already load-execute.
      Get_Top_For_Addr_Mode(top, BASE_MODE) == TOP_UNDEFINED ||
      !TOP_is_load_exe(Get_Top_For_Addr_Mode(top, BASE_MODE)))
    return FALSE;

  if( OP_opnds(alu_op) == 2               &&
      TN_is_register( OP_opnd(alu_op,1) ) &&
      TNs_Are_Equivalent( OP_opnd(alu_op,0), OP_opnd(alu_op,1) ) )
    return FALSE;

  EBO_TN_INFO* tninfo = NULL;
  int opnd0_indx = 0;  // indicate which opnd will be kept for the new op.

  for( int i = OP_opnds(alu_op) - 1; i >= 0; i-- ){
    if( TN_is_register( OP_opnd( alu_op, i ) ) ){
      tninfo = actual_tninfo[i];
      opnd0_indx = OP_opnds(alu_op) - 1 - i;
      Is_True( opnd0_indx >= 0, ("NYI") );
      break;
    }
  }

  OP* ld_op = tninfo == NULL ? NULL : tninfo->in_op;
  EBO_OP_INFO* ld_opinfo = tninfo == NULL ? NULL : tninfo->in_opinfo;

  if( ld_op == NULL || !OP_load( ld_op ) ||
      ld_opinfo->op_must_not_be_moved ){
      
    // Now, try opnd0

    if( !TOP_is_commutative( OP_code(alu_op) ) )
      return FALSE;

    tninfo = actual_tninfo[0];
    ld_op = tninfo == NULL ? NULL : tninfo->in_op;
    ld_opinfo = tninfo == NULL ? NULL : tninfo->in_opinfo;

    if( ld_op == NULL || !OP_load( ld_op ) ||
	ld_opinfo->op_must_not_be_moved )
      return FALSE;

    // Check whether we can swap opnd0 and opnd1 of <alu_op>
    TN* result = OP_result( alu_op, 0 );
    TN* opnd0 = OP_opnd( alu_op, 0 );

    if( EBO_in_peep && TNs_Are_Equivalent( result, opnd0 ) )
      return FALSE;

    opnd0_indx = 1;
  }

  BB* bb = OP_bb( alu_op );

  if( OP_bb( ld_op ) != bb )
    return FALSE;

  /* bug#1480
     The memory opnd of an alu op must be aligned.
     According to Section 4.4.4. (Data Alignment) of Volumn 1,
     "128-bit media instructions that access a 128-bit operand in memory
     incur a general-protection exception if the operand is not aligned to
     a 16-byte boundary ..."
   */
  if( OP_unalign_mem( ld_op ) &&
      TOP_is_vector_op( OP_code(ld_op) ) ){
    return FALSE;
  }

  /* Check <index> and <base> will not be re-defined between
     <ld_op> and <alu_op>, inclusive.
  */

  const int index_reg = OP_find_opnd_use( ld_op, OU_index );
  const int base_reg  = OP_find_opnd_use( ld_op, OU_base );

  ADDR_MODE mode = BASE_MODE;

  if( index_reg < 0 && base_reg < 0 )
    mode = N32_MODE;

  else if( index_reg >= 0 )
    mode = base_reg < 0 ? INDEX_MODE : BASE_INDEX_MODE;

  if( mode == N32_MODE ){
    // We need to add one more addressing mode for m32.
    //DevWarn( "Support me!!!" );
    return FALSE;
  }

  if( index_reg >= 0 ){
    const TN* opnd = OP_opnd( ld_op, index_reg );
    const EBO_TN_INFO* ptinfo = get_tn_info( opnd );
    if( ptinfo != NULL && ptinfo->sequence_num >= tninfo->sequence_num ){
      return FALSE;
    }
  }

  if( base_reg >= 0 ){
    const TN* opnd = OP_opnd( ld_op, base_reg );
    const EBO_TN_INFO* ptinfo = get_tn_info( opnd );
    if( ptinfo != NULL && ptinfo->sequence_num >= tninfo->sequence_num ){
      return FALSE;
    }
  }

  /* Make sure the value from <ld_op> will be not over-written by
     any store located between <ld_op> and <alu_op>. (bug#2680)
  */

  {
    const INT hash_value = EBO_hash_op( ld_op, NULL );
    EBO_OP_INFO* opinfo = EBO_opinfo_table[hash_value];

    while( opinfo != NULL ){
      OP* pred_op = opinfo->in_op;

      if( pred_op == ld_op )
	break;

      /* It is quite expensive to check the aliasing info here. */
      if(
#ifdef KEY
         pred_op &&	// Bug 7596
#endif
	 OP_store( pred_op ) )
	return FALSE;

      opinfo = opinfo->same;
    }
  }

  const TOP new_top = Load_Execute_Format( ld_op, alu_op, mode );

  if( new_top == TOP_UNDEFINED )
    return FALSE;

  const INT32 load_uses = hTN_MAP32_Get( _load_exec_map, OP_result(ld_op,0) ) - 1;

  /* It is always profitable to perform load execution if the new latency is shorter.
   */
  if( ( load_uses > CG_load_execute ) &&
      ( CGTARG_Latency(top) < CGTARG_Latency(new_top) ) ){
    return FALSE;
  }

  TN* offset = OP_opnd( ld_op, OP_find_opnd_use( ld_op, OU_offset ) );
  TN* base   = base_reg >= 0 ? OP_opnd( ld_op, base_reg ) : NULL;
  TN* index  = index_reg >= 0 ? OP_opnd( ld_op, index_reg ) : NULL;
  TN* result = OP_has_result( alu_op ) ? OP_result( alu_op, 0 ) : NULL;
  TN* scale  = index_reg >= 0 ?
    OP_opnd( ld_op, OP_find_opnd_use( ld_op, OU_scale ) ) : NULL;

  TN* opnd1 = NULL;
  TN* opnd0 = OP_opnd( alu_op, opnd0_indx );

  // For TOP_cmpi cases
  if( opnd0_indx == 1 && TN_has_value(opnd0) ){
    opnd1 = opnd0;
    opnd0 = NULL;
  }

  OP* new_op = NULL;

  if( mode == BASE_MODE ){
    // base + offset
    if( OP_opnds(alu_op) == 1 ){
      if( result == NULL )
	new_op = Mk_OP( new_top, base, offset );
      else
	new_op = Mk_OP( new_top, result, base, offset );
    } else if( opnd1 != NULL )
      new_op = Mk_OP( new_top, result, base, offset, opnd1 );
    else
      new_op = Mk_OP( new_top, result, opnd0, base, offset );

  } else if( mode == BASE_INDEX_MODE ){
    // offset + base + index * scale
    if( OP_opnds(alu_op) == 1 ){
      if( result == NULL )
	new_op = Mk_OP( new_top, base, index, scale, offset );
      else
	new_op = Mk_OP( new_top, result, base, index, scale, offset );
      
    } else if( opnd1 != NULL )
      new_op = Mk_OP( new_top, result, base, index, scale, offset, opnd1 );
    else
      new_op = Mk_OP( new_top, result, opnd0, base, index, scale, offset );

  } else {
    // offset + index * scale
    if( OP_opnds(alu_op) == 1 ){
      if( result == NULL )
	new_op = Mk_OP( new_top, index, scale, offset );
      else
	new_op = Mk_OP( new_top, result, index, scale, offset );
    } else if( opnd1 != NULL )
      new_op = Mk_OP( new_top, result, index, scale, offset, opnd1 );
    else
      new_op = Mk_OP( new_top, result, opnd0, index, scale, offset );
  }

  Is_True( !EBO_in_loop, ("EBO_Load_Execution: NYI (1)") );

  Set_OP_unrolling( new_op, OP_unrolling(alu_op) );
  Set_OP_orig_idx( new_op, OP_map_idx(alu_op) );
  Set_OP_unroll_bb( new_op, OP_unroll_bb(alu_op) );

  Copy_WN_For_Memory_OP( new_op, ld_op );
  if ( OP_volatile( ld_op ) )
    Set_OP_volatile( new_op );
  OP_srcpos( new_op ) = OP_srcpos( alu_op );
  BB_Insert_Op_After( bb, alu_op, new_op );

  // If folding a restore of a spilled value, mark the spill store as needed
  // even if all the restores are deleted.
  ST *spill_loc = CGSPILL_OP_Spill_Location(ld_op);
  if (spill_loc != (ST *)0) {		// It's a spill OP.
    SPILL_SYM_INFO &info = CGSPILL_Get_Spill_Sym_Info(spill_loc);
    info.Set_Used_By_Load_Exe();
  }

  if( EBO_Trace_Data_Flow ){
    #pragma mips_frequency_hint NEVER
    fprintf( TFile, "EBO_Load_Execution merges " );
    Print_OP_No_SrcLine( ld_op );
    fprintf( TFile, "                   with   " );
    Print_OP_No_SrcLine( alu_op );

    fprintf( TFile, "                   new op " );
    Print_OP_No_SrcLine( new_op );
  }

  return TRUE;
}

static INT
Get_Power_Of_2 (INT val)
{
  INT i, pow2mask;

  pow2mask = 1;
  for ( i = 0; i < 5; ++i ) {
    if (val == pow2mask) return i;
    pow2mask <<= 1;
  }

  FmtAssert(FALSE, ("Get_Power_Of_2 unexpected value (%d)", val));
  /* NOTREACHED */
}

BOOL
Check_No_Use_Between (OP* from, OP* to, TN* result)
{
  if (!TN_is_register(result))
    return FALSE;

  for (OP* op = from->next; op && op != to; op = op->next) {
    for (INT i = 0; i < OP_opnds(op); i ++) {
      TN* opnd = OP_opnd(op, i);
      if (TN_is_register(opnd)) {
	if (TNs_Are_Equivalent(result, opnd))
	  return FALSE;

	// Account for fp uses when testing for sp uses.  We may need to adjust
	// the sp before using the fp in order to make the fp access legal.
	// Bug 11209.
	if (result == SP_TN &&
	    TNs_Are_Equivalent(FP_TN, opnd))
	  return FALSE;
      }
    }
  }

  return TRUE;
}

BOOL
Check_No_Redef_Between (OP* from, OP* to, TN* opnd)
{
  if (!TN_is_register(opnd))
    return FALSE;

  for (OP* op = to->prev; op && op != from; op = op->prev) {
    for (INT i = 0; i < OP_opnds(op); i ++) {
      TN* tmp_opnd = OP_opnd(op, i);
      if (TN_is_register(tmp_opnd) &&
	  TNs_Are_Equivalent(tmp_opnd, opnd)) {
	EBO_TN_INFO *src0_info, *src1_info;
	src0_info = get_tn_info( tmp_opnd );
	src1_info = get_tn_info( opnd );
	if (!src0_info || !src1_info)
	  return FALSE;
	if (src0_info->sequence_num != src1_info->sequence_num)
	  return FALSE;
      }
    }
    for (INT i = 0; i < OP_results(op); i ++) {
      TN* tmp_res = OP_result(op, i);
      if (TN_is_register(tmp_res) &&
	  TNs_Are_Equivalent(tmp_res, opnd))
	return FALSE;
    }
  }

  return TRUE;
}

static BOOL
alu_op_defines_rflags_used (OP* alu_op, OP* op)
{
  // Bug 2040 - if alu_op changes rflags and there is an operaton between
  // op and alu_op that reads the rflags, then we can not delete alu_op.
  if (TOP_is_change_rflags( OP_code(alu_op) )) {
    BOOL rflags_read = FALSE;
    for( OP* next_op = OP_next(alu_op); next_op != NULL && next_op != op; 
	 next_op = OP_next( next_op ) ){
      if( OP_reads_rflags( next_op ) )
	rflags_read = TRUE;
      if( TOP_is_change_rflags( OP_code(next_op) ) )
	break;
    }
    if (rflags_read)
      return TRUE; 
  }

  return FALSE;
}

BOOL
EBO_Lea_Insertion( OP* op, TN** opnd_tn, EBO_TN_INFO** actual_tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_LEA_INSERTION)) return FALSE;
#endif
  TOP code = OP_code (op);
  OP* new_op = NULL;
  INT64 offset, offset_tmp;
  INT shift; 
  ST *base, *base_tmp;
  BOOL rflags_read = FALSE;

  // If there is an instruction that is awaiting a rflags update then,
  // do not convert the current op. 
  for( OP* next_op = OP_next(op); next_op != NULL; 
	next_op = OP_next( next_op ) ){
    if( OP_reads_rflags( next_op ) )
      rflags_read = TRUE;
    if( TOP_is_change_rflags( OP_code(next_op) ) )
      break;
  }

  switch (code) {
  case TOP_imul32:
  case TOP_imul64:
    {
      if (!CG_fold_constimul)
        break;
      // Try to fold the second operand	
      OP* alu_op = actual_tninfo[1]->in_op;
      if (alu_op && alu_op->bb == op->bb &&
	  (OP_code(alu_op) == TOP_ldc32 || 
	   OP_code(alu_op) == TOP_ldc64) &&
	  !TN_is_symbol(OP_opnd(alu_op, 0)) &&
	  !TN_live_out_of(OP_result(alu_op, 0), OP_bb(alu_op)) &&
	  ((op->next != NULL &&
	    !is_live_tn(op->next, OP_result(alu_op, 0))) ||
	   (op->next == NULL && 
	    !GTN_SET_MemberP(BB_live_out(OP_bb(op)), 
			     OP_result(alu_op, 0)))) &&
	  Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0))) {
	INT32 val = (INT32)TN_value(OP_opnd(alu_op, 0));
	INT64 val64 = TN_value(OP_opnd(alu_op, 0));
	BOOL is_double = (code == TOP_imul64);
	
	val64 *= val64; // see the use
	if (TNs_Are_Equivalent(OP_opnd(op, 0), OP_opnd(op, 1)))
	  new_op = Mk_OP (is_double?TOP_ldc64:TOP_ldc32, 
	  	          OP_result(op, 0),
			  Gen_Literal_TN(is_double?(INT64)val64:(INT32)val*val,
					 is_double?8:4));
	else if (OP_code(alu_op) == TOP_ldc64) {
	  if (!ISA_LC_Value_In_Class (TN_value(OP_opnd(alu_op, 0)), LC_simm32)) 
	    break;
	}
	if (!new_op)
	  new_op = Mk_OP ((code == TOP_imul32)?TOP_imuli32:TOP_imuli64, 
			  OP_result(op, 0), OP_opnd(op, 0),
			  Gen_Literal_TN(val, 4));
	
	if (alu_op_defines_rflags_used(alu_op, op))
	  return FALSE;

	OP_srcpos( new_op ) = OP_srcpos( op );
	if( EBO_Trace_Data_Flow ){
	  fprintf( TFile, "Lea_Insertion merges " );
	  Print_OP_No_SrcLine(op);
	  fprintf( TFile, "and " );
	  Print_OP_No_SrcLine(alu_op);
	  fprintf( TFile, "with " );
	  Print_OP_No_SrcLine(new_op);
	}
	dec_ref_count(actual_tninfo[1]);
	break;
      }
      if (TN_is_register(OP_opnd(op, 0)) &&
	  !TNs_Are_Equivalent(OP_opnd(op, 0),OP_result(op, 0)) ) {
	// Check if we can interchange the two operands and then see if we 
	// can constant fold.
	OP* alu_op = actual_tninfo[0]->in_op;
	if (alu_op && alu_op->bb == op->bb &&
	    (OP_code(alu_op) == TOP_ldc32 ||
	     OP_code(alu_op) == TOP_ldc64) &&
	    !TN_is_symbol(OP_opnd(alu_op, 0)) &&
	    !TN_live_out_of(OP_result(alu_op, 0), OP_bb(alu_op)) &&
	    ((op->next != NULL &&
	      !is_live_tn(op->next, OP_result(alu_op, 0))) ||
	     (op->next == NULL && 
	      !GTN_SET_MemberP(BB_live_out(OP_bb(op)), 
			       OP_result(alu_op, 0)))) &&
	    Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0))) {
	  INT32 val = (INT32)TN_value(OP_opnd(alu_op, 0));
	  if (OP_code(alu_op) == TOP_ldc64) {
	    if (!ISA_LC_Value_In_Class (TN_value(OP_opnd(alu_op, 0)), LC_simm32)) 
	      break;
	  }
	  new_op = Mk_OP ((code == TOP_imul32)?TOP_imuli32:TOP_imuli64, 
			  OP_result(op, 0), OP_opnd(op, 1), 
			  Gen_Literal_TN(val, 4));

	  if (alu_op_defines_rflags_used(alu_op, op))
	    return FALSE;

	  OP_srcpos( new_op ) = OP_srcpos( op );
	  if( EBO_Trace_Data_Flow ){
	    fprintf( TFile, "Lea_Insertion merges " );
	    Print_OP_No_SrcLine(op);
	    fprintf( TFile, "and " );
	    Print_OP_No_SrcLine(alu_op);
	    fprintf( TFile, "with " );
	    Print_OP_No_SrcLine(new_op);
	  }
	  dec_ref_count(actual_tninfo[0]);
	}
      }
      break;
    }
  case TOP_add32:
  case TOP_add64:
    {
      OP* alu_op = actual_tninfo[0]->in_op;
      if( TN_is_register(OP_opnd(op, 0)) &&
	  TNs_Are_Equivalent(OP_opnd(op, 0),OP_result(op, 0)) ) {
	if (alu_op && alu_op->bb == op->bb && 
	    ((OP_code(alu_op) == TOP_leax32 && code == TOP_add32) ||
	     (OP_code(alu_op) == TOP_leax64 && code == TOP_add64)) &&
	    TN_value(OP_opnd(alu_op, 2)) == 1 &&
	    !TN_is_symbol(OP_opnd(alu_op, 3)) &&
	    TN_value(OP_opnd(alu_op, 3)) == 0 &&
	    TN_is_register(OP_opnd(alu_op, 0)) &&
	    TN_is_register(OP_opnd(alu_op, 1)) &&
	    TN_is_register(OP_opnd(op, 1)) &&
	    TNs_Are_Equivalent(OP_opnd(alu_op, 0),OP_opnd(op, 1)) &&
	    TNs_Are_Equivalent(OP_opnd(alu_op, 0),OP_opnd(alu_op, 1)) &&
	    Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0)) &&
	    // Bug 129: tninfo sequence number is no good after Register
	    // allocation. Another TN could be assigned the same register
	    // and be defined/re-defined between op and alu_op.
	    Check_No_Redef_Between(alu_op, op, OP_opnd(alu_op, 0))) {
	  new_op = Mk_OP ((code == TOP_add32)?TOP_leax32:TOP_leax64, 
			  OP_result(op, 0), OP_opnd(alu_op, 0), 
			  OP_opnd(alu_op, 1), 
			  Gen_Literal_TN(2, 4), Gen_Literal_TN(0, 4));	  

	  if (rflags_read && 
	      ((TOP_is_change_rflags( OP_code(new_op) ) &&
		!TOP_is_change_rflags( OP_code(op) )) ||
	       (!TOP_is_change_rflags( OP_code(new_op) ) &&
		TOP_is_change_rflags( OP_code(op) ))))
	    return FALSE;

	  if (alu_op_defines_rflags_used(alu_op, op))
	    return FALSE;

	  OP_srcpos( new_op ) = OP_srcpos( op );
	  if( EBO_Trace_Data_Flow ){
	    fprintf( TFile, "Lea_Insertion merges " );
	    Print_OP_No_SrcLine(op);
	    fprintf( TFile, "and " );
	    Print_OP_No_SrcLine(alu_op);
	    fprintf( TFile, "with " );
	    Print_OP_No_SrcLine(new_op);
	  }
	  BB_Remove_Op(OP_bb(alu_op), alu_op);
	} else {
	  // merge cases like:
	  //    leal 0(,%rax,4), %edi
	  //    addl %edi,%ebx
	  alu_op = actual_tninfo[1]->in_op;
	  if (alu_op && alu_op->bb == op->bb && 
	      ((OP_code(alu_op) == TOP_leaxx32 && code == TOP_add32) ||
	       (OP_code(alu_op) == TOP_leaxx64 && code == TOP_add64)) &&
	      !TN_live_out_of(OP_result(alu_op, 0), OP_bb(alu_op)) &&
	      ((op->next != NULL &&
		!is_live_tn(op->next, OP_result(alu_op, 0))) ||
	       (op->next == NULL && 
		!GTN_SET_MemberP(BB_live_out(OP_bb(op)), 
				 OP_result(alu_op, 0)))) &&
	      Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0))) {
	    // result of alu_op should not be redefined between alu_op and op
	    EBO_TN_INFO *src0_info, *src1_info;
	    src0_info = get_tn_info( OP_opnd(op, 1));
	    src1_info = get_tn_info( OP_result(alu_op, 0));
	    if (src0_info->sequence_num > src1_info->sequence_num)
	      break;
	    
	    // There should be no redefinitions of alu_op's first opnd.
	    if (!Check_No_Redef_Between(alu_op, op, OP_opnd(alu_op, 0)))
	      break;

	    new_op = Mk_OP ((code == TOP_add32)?TOP_leax32:TOP_leax64, 
			    OP_result(op, 0), OP_opnd(op, 0), 
			    OP_opnd(alu_op, 0), OP_opnd(alu_op, 1), 
			    OP_opnd(alu_op, 2));

	    if (rflags_read && 
		((TOP_is_change_rflags( OP_code(new_op) ) &&
		  !TOP_is_change_rflags( OP_code(op) )) ||
		 (!TOP_is_change_rflags( OP_code(new_op) ) &&
		  TOP_is_change_rflags( OP_code(op) ))))
	      return FALSE;

	    if (alu_op_defines_rflags_used(alu_op, op))
	      return FALSE;

	    OP_srcpos( new_op ) = OP_srcpos( op );
	    if( EBO_Trace_Data_Flow ){
	      fprintf( TFile, "Lea_Insertion merges " );
	      Print_OP_No_SrcLine(op);
	      fprintf( TFile, "and " );
	      Print_OP_No_SrcLine(alu_op);
	      fprintf( TFile, "with " );
	      Print_OP_No_SrcLine(new_op);
	    }
	    BB_Remove_Op(OP_bb(alu_op), alu_op);	    
	  }
	}

	break;
      }

      //opnd0 and result are not equivalent
      if (alu_op && alu_op->bb == op->bb && 
	  ((OP_code(alu_op) == TOP_leax32 && code == TOP_add32) ||
	   (OP_code(alu_op) == TOP_leax64 && code == TOP_add64)) &&
	  TN_value(OP_opnd(alu_op, 2)) == 1 &&
	  !TN_is_symbol(OP_opnd(alu_op, 3)) &&
	  TN_value(OP_opnd(alu_op, 3)) == 0 &&
	  TN_is_register(OP_opnd(alu_op, 0)) &&
	  TN_is_register(OP_opnd(alu_op, 1)) &&
	  TN_is_register(OP_opnd(op, 1)) &&
	  TNs_Are_Equivalent(OP_opnd(alu_op, 0),OP_opnd(op, 1)) &&
	  TNs_Are_Equivalent(OP_opnd(alu_op, 0),OP_opnd(alu_op, 1)) &&
	  !TN_live_out_of(OP_result(alu_op, 0), OP_bb(alu_op)) &&
	  ((op->next != NULL &&
	    !is_live_tn(op->next, OP_result(alu_op, 0))) ||
	   (op->next == NULL && 
	    !GTN_SET_MemberP(BB_live_out(OP_bb(op)), OP_result(alu_op, 0)))) &&
	  Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0))) {
	EBO_TN_INFO *src0_info, *src1_info;
	src0_info = get_tn_info( OP_opnd(op, 1));
	src1_info = get_tn_info( OP_opnd(alu_op, 0));
	// operand redefined?	
	if (src0_info && src1_info &&
	    src0_info->sequence_num > src1_info->sequence_num) {
	  new_op = Mk_OP ((code == TOP_add32)?TOP_leax32:TOP_leax64, 
			  OP_result(op, 0), OP_opnd(alu_op, 0), 
			  OP_opnd(alu_op, 1), 
			  Gen_Literal_TN(2, 4), Gen_Literal_TN(0, 4));	  

	  if (rflags_read && 
	      ((TOP_is_change_rflags( OP_code(new_op) ) &&
		!TOP_is_change_rflags( OP_code(op) )) ||
	       (!TOP_is_change_rflags( OP_code(new_op) ) &&
		TOP_is_change_rflags( OP_code(op) ))))
	    return FALSE;

	  if (alu_op_defines_rflags_used(alu_op, op))
	    return FALSE;

	  OP_srcpos( new_op ) = OP_srcpos( op );
	  if( EBO_Trace_Data_Flow ){
	    fprintf( TFile, "Lea_Insertion merges " );
	    Print_OP_No_SrcLine(op);
	    fprintf( TFile, "and " );
	    Print_OP_No_SrcLine(alu_op);
	    fprintf( TFile, "with " );
	    Print_OP_No_SrcLine(new_op);
	  }
	  BB_Remove_Op(OP_bb(alu_op), alu_op);
	  break;
	}
      }
     
      // Bug 1563 - dont let this module place SP or FP as the index register.
      if (OP_opnd(op, 1) == SP_TN || OP_opnd(op, 1) == FP_TN) {
	// Can not push things around after Adjust_X86_Style_Op
	if (EBO_in_peep) break; 
	// something crazy?
        if (OP_opnd(op, 0) == SP_TN || OP_opnd(op, 0) == FP_TN) break;
        new_op = Mk_OP ((code == TOP_add32)?TOP_leax32:TOP_leax64, 
	  	      OP_result(op, 0), OP_opnd(op, 1), OP_opnd(op, 0), 
		      Gen_Literal_TN(1, 4), Gen_Literal_TN(0, 4));
      } else 
        new_op = Mk_OP ((code == TOP_add32)?TOP_leax32:TOP_leax64, 
	  	      OP_result(op, 0), OP_opnd(op, 0), OP_opnd(op, 1), 
		      Gen_Literal_TN(1, 4), Gen_Literal_TN(0, 4));

      if (rflags_read && 
	  ((TOP_is_change_rflags( OP_code(new_op) ) &&
	    !TOP_is_change_rflags( OP_code(op) )) ||
	   (!TOP_is_change_rflags( OP_code(new_op) ) &&
	    TOP_is_change_rflags( OP_code(op) ))))
	return FALSE;

      OP_srcpos( new_op ) = OP_srcpos( op );
      if( EBO_Trace_Data_Flow ){
	fprintf( TFile, "Lea_Insertion removes " );
	Print_OP_No_SrcLine(op);
	fprintf( TFile, "Lea_Insertion inserts " );
	Print_OP_No_SrcLine(new_op);
      }
      break;
    }
  case TOP_addi32:
  case TOP_addi64:
    {
      base = base_tmp = NULL;
      offset = offset_tmp = 0;
      if (TN_is_symbol (OP_opnd(op, 1))) {
	if (!EBO_in_peep)
	  // symbol offsets are not known until data layout time
	  break;
	TN *t = OP_opnd(op, 1);
	Base_Symbol_And_Offset (TN_var(t), &base, &offset);
	if (base == SP_Sym || base == FP_Sym) {
	  offset += TN_offset(t);
	  if ( TN_is_reloc_neg(t) )
	    offset = -offset;
	  if ( TN_is_reloc_low16(t) )
	    offset = offset & 0xffff;
	  else if ( TN_is_reloc_high16(t) )
	    offset = ( ( offset - (short)offset ) >> 16) & 0xffff;
	  else if ( TN_is_reloc_higher(t) )
	    offset = ( ( offset + 0x80008000LL ) >> 32 ) & 0xffff;
	  else if ( TN_is_reloc_highest(t) )
	    offset = ( ( offset + 0x800080008000LL ) >> 48 ) & 0xffff;
	} 
	
      } else 
	offset = TN_value(OP_opnd(op, 1));
      
      if( TN_is_register(OP_opnd(op, 0)) &&
	  TN_is_register(OP_result(op, 0)) &&
	  TNs_Are_Equivalent(OP_opnd(op, 0),OP_result(op, 0)) ) {
	OP* alu_op = actual_tninfo[0]->in_op;
	if (alu_op && alu_op->bb && alu_op->bb == op->bb && 
	    OP_code(alu_op) == code &&
	    Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0))) {
	  if (TN_is_symbol (OP_opnd(alu_op, 1))) {
	    if (!EBO_in_peep)
	      // symbol offsets are not known until data layout time
	      break;
	    TN *tmp = OP_opnd(alu_op, 1);
	    Base_Symbol_And_Offset (TN_var(tmp), &base_tmp, &offset_tmp);
	    if (base != base_tmp)
	      break;
	    if (base_tmp == SP_Sym || base_tmp == FP_Sym) {
	      offset_tmp += TN_offset(tmp);
	      if ( TN_is_reloc_neg(tmp) )
		offset_tmp = -offset_tmp;
	      if ( TN_is_reloc_low16(tmp) )
		offset_tmp = offset_tmp & 0xffff;
	      else if ( TN_is_reloc_high16(tmp) )
		offset_tmp = 
		( ( offset_tmp - (short)offset_tmp ) >> 16) & 0xffff;
	      else if ( TN_is_reloc_higher(tmp) )
		offset_tmp = ( ( offset_tmp + 0x80008000LL ) >> 32 ) & 0xffff;
	      else if ( TN_is_reloc_highest(tmp) )
		offset_tmp = 
		( ( offset_tmp + 0x800080008000LL ) >> 48 ) & 0xffff;
	    } 
	    
	  } else 
	    offset_tmp = TN_value(OP_opnd(alu_op, 1));
      
	  // see if the value can fit in a simm32 offset field of lea
	  if (ISA_LC_Value_In_Class (offset+offset_tmp, LC_simm32)) {
	    TN* offset_tn = NULL;
	    if (TN_is_symbol(OP_opnd(op, 1)) && 
		base && base != SP_Sym && base != FP_Sym) 
	      offset_tn = Gen_Symbol_TN( TN_var(OP_opnd(op, 0)), 
					TN_offset(OP_opnd(op, 0))+
					offset+offset_tmp,0);
	    new_op = Mk_OP ((code == TOP_addi32)?TOP_addi32:TOP_addi64,
			    OP_result(op, 0), OP_opnd(op, 0), 
			    offset_tn? offset_tn: 
			    Gen_Literal_TN(offset+offset_tmp, 4));

	    if (rflags_read && 
		((TOP_is_change_rflags( OP_code(new_op) ) &&
		  !TOP_is_change_rflags( OP_code(op) )) ||
		 (!TOP_is_change_rflags( OP_code(new_op) ) &&
		  TOP_is_change_rflags( OP_code(op) ))))
	      return FALSE;

	    if (alu_op_defines_rflags_used(alu_op, op))
	      return FALSE;

	    OP_srcpos( new_op ) = OP_srcpos( op );
	    if( EBO_Trace_Data_Flow ){
	      fprintf( TFile, "Lea_Insertion merges " );
	      Print_OP_No_SrcLine(op);
	      fprintf( TFile, "and " );
	      Print_OP_No_SrcLine(alu_op);
	      fprintf( TFile, "with " );
	      Print_OP_No_SrcLine(new_op);
	    }

	    BB_Remove_Op(OP_bb(alu_op), alu_op);
	  }
	}
	break;
      }
      
      // see if the value can fit in a simm32 offset field of lea
      if (ISA_LC_Value_In_Class (offset, LC_simm32))
	new_op = Mk_OP ((code == TOP_addi32)?TOP_lea32:TOP_lea64,
			OP_result(op, 0), OP_opnd(op, 0), OP_opnd(op, 1));

      if (rflags_read && new_op &&
	  ((TOP_is_change_rflags( OP_code(new_op) ) &&
	    !TOP_is_change_rflags( OP_code(op) )) ||
	   (!TOP_is_change_rflags( OP_code(new_op) ) &&
	    TOP_is_change_rflags( OP_code(op) ))))
	return FALSE;
      
      OP_srcpos( new_op ) = OP_srcpos( op );
      if (new_op) {
	if( EBO_Trace_Data_Flow ){
	  fprintf( TFile, "Lea_Insertion removes " );
	  Print_OP_No_SrcLine(op);
	  fprintf( TFile, "Lea_Insertion inserts " );
	  Print_OP_No_SrcLine(new_op);
	}
      }
      break;
    }
  case TOP_shli32:
  case TOP_shli64:
    { 
      // Transform the shifts into leaxx if the result and the first operand 
      // are non-identical.
      if( (TN_is_register(OP_opnd(op, 0)) &&
	   !TNs_Are_Equivalent(OP_opnd(op, 0),OP_result(op, 0)))) {
	// Check if the shift amount is 0/1/2/3
	shift = TN_value(OP_opnd(op,1));
	if (shift != 0 && shift != 1 && shift != 2 && shift != 3)
	  break;
	
	shift = 1 << shift;
	// convert 'shliq $const,opnd0,res' to
	// 'leaxxq 0(,opnd0,1<<$const),res'
	new_op = Mk_OP ((code == TOP_shli32)?TOP_leaxx32:TOP_leaxx64,
			OP_result(op, 0), OP_opnd(op, 0), 
			Gen_Literal_TN(shift, 4), 
			Gen_Literal_TN(0, 4));
      }

      if (rflags_read && new_op &&
	  ((TOP_is_change_rflags( OP_code(new_op) ) &&
	    !TOP_is_change_rflags( OP_code(op) )) ||
	   (!TOP_is_change_rflags( OP_code(new_op) ) &&
	    TOP_is_change_rflags( OP_code(op) ))))
	return FALSE;

      if (new_op) {
	if( EBO_Trace_Data_Flow ){
	  fprintf( TFile, "Lea_Insertion removes " );
	  Print_OP_No_SrcLine(op);
	  fprintf( TFile, "Lea_Insertion inserts " );
	  Print_OP_No_SrcLine(new_op);
	}
      }
      break;
    }
  case TOP_lea32:
  case TOP_lea64: 
    {
      if (TN_is_symbol (OP_opnd(op, 1))) {
	if (!EBO_in_peep)
	  // symbol offsets are not known until data layout time
	  break;
	TN *t = OP_opnd(op, 1);
	Base_Symbol_And_Offset (TN_var(t), &base, &offset);
	if (base == SP_Sym || base == FP_Sym) {
	  offset += TN_offset(t);
	  if ( TN_is_reloc_neg(t) )
	    offset = -offset;
	  if ( TN_is_reloc_low16(t) )
	    offset = offset & 0xffff;
	  else if ( TN_is_reloc_high16(t) )
	    offset = ( ( offset - (short)offset ) >> 16) & 0xffff;
	  else if ( TN_is_reloc_higher(t) )
	    offset = ( ( offset + 0x80008000LL ) >> 32 ) & 0xffff;
	  else if ( TN_is_reloc_highest(t) )
	    offset = ( ( offset + 0x800080008000LL ) >> 48 ) & 0xffff;
	} 
	
      } else 
	offset = TN_value(OP_opnd(op, 1));
      
      // Fold a previous addi or lea into this lea
      OP* alu_op = actual_tninfo[0]->in_op;
      if (alu_op && alu_op->bb == op->bb &&
	  ((OP_code(op) == TOP_lea32 &&
	    (OP_code(alu_op) == TOP_addi32 ||
	     OP_code(alu_op) == TOP_lea32)) ||
	   (OP_code(op) == TOP_lea64 &&
	    (OP_code(alu_op) == TOP_addi64 ||
	     OP_code(alu_op) == TOP_lea64))) &&
	  // we should be able to combine offsets
	  (!TN_is_symbol (OP_opnd(alu_op, 1)) || EBO_in_peep) &&
	  // Can not add up two symbols right now
	  (!TN_is_symbol (OP_opnd(op, 1)) || 
	   !TN_is_symbol (OP_opnd(alu_op, 1))) &&
	  !TN_live_out_of(OP_result(alu_op, 0), OP_bb(alu_op)) &&
	  ((op->next != NULL &&
	    !is_live_tn(op->next, OP_result(alu_op, 0))) ||
	   (op->next == NULL && 
	    !GTN_SET_MemberP(BB_live_out(OP_bb(op)), OP_result(alu_op, 0)))) &&
	  // There should be no other uses of result of alu_op
	  Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0)) &&
	  // There should be no redefinitions of alu_op's first opnd.
	  Check_No_Redef_Between(alu_op, op, OP_opnd(alu_op, 0)) &&
	  // The following scenario occurs when LRA removes a copy op between
	  // a local TN and a GTN, after register allocation, and later EBO
	  // deletes the assignment to the local TN. 
	  // See -O3 176.gcc/local_alloc.c BB:26
	  !TN_live_out_of(OP_opnd(alu_op, 0), OP_bb(alu_op)) &&
	  TN_is_register(OP_result(alu_op, 0)) &&
	  TN_is_register(OP_opnd(alu_op, 0)) &&
	  (!TNs_Are_Equivalent(OP_result(alu_op, 0), OP_opnd(alu_op, 0)) ||
	   !is_live_tn(alu_op->next, OP_opnd(alu_op, 0)))) {
	INT64 tmp_offset, new_offset;
	TN* ofst_tn = NULL;
	if (TN_is_symbol (OP_opnd(alu_op, 1))) {
	  TN *t = OP_opnd(alu_op, 1);
	  Base_Symbol_And_Offset (TN_var(t), &base_tmp, &tmp_offset);
	  if (base_tmp == SP_Sym || base_tmp == FP_Sym) {
	    tmp_offset += TN_offset(t);
	    if ( TN_is_reloc_neg(t) )
	      tmp_offset = -tmp_offset;
	    if ( TN_is_reloc_low16(t) )
	      tmp_offset = tmp_offset & 0xffff;
	    else if ( TN_is_reloc_high16(t) )
	      tmp_offset = 
		( ( tmp_offset - (short)tmp_offset ) >> 16) & 0xffff;
	    else if ( TN_is_reloc_higher(t) )
	      tmp_offset = ( ( tmp_offset + 0x80008000LL ) >> 32 ) & 0xffff;
	    else if ( TN_is_reloc_highest(t) )
	      tmp_offset = 
		( ( tmp_offset + 0x800080008000LL ) >> 48 ) & 0xffff;
	  } 
	  
	} else 
	  tmp_offset = TN_value(OP_opnd(alu_op, 1));
      
	new_offset = offset + tmp_offset;

	if (base_tmp || base) {
	  TN* t = OP_opnd(op, 1);
	  if (TN_is_symbol(t) && base && base != SP_Sym && base != FP_Sym)
	    ofst_tn = Gen_Symbol_TN( TN_var(t), TN_offset(t)+new_offset,0);
	  t = OP_opnd(alu_op, 1);
	  if (TN_is_symbol(t) && 
	      base_tmp && base_tmp != SP_Sym && base_tmp != FP_Sym)
	    ofst_tn = 
	      Gen_Symbol_TN( TN_var(t), TN_offset(t)+new_offset,0);	  
	}

	// check if new_offset can fit in a simm32 field of a new lea
	if (ISA_LC_Value_In_Class (new_offset, LC_simm32)) {
	  new_op = Mk_OP ((code == TOP_lea32)?TOP_lea32:TOP_lea64,
			  OP_result(op, 0), OP_opnd(alu_op, 0), 
			  ofst_tn ? ofst_tn : Gen_Literal_TN(new_offset, 4));
	  
	  if (alu_op_defines_rflags_used(alu_op, op))
	    return FALSE;

	  OP_srcpos( new_op ) = OP_srcpos( op );
	  if( EBO_Trace_Data_Flow ){
	    fprintf( TFile, "Lea_Insertion merges " );
	    Print_OP_No_SrcLine(op);
	    fprintf( TFile, "and " );
	    Print_OP_No_SrcLine(alu_op);
	    fprintf( TFile, "with " );
	    Print_OP_No_SrcLine(new_op);
	  }
	  BB_Remove_Op(OP_bb(alu_op), alu_op);
	  break;
	}
      }

      if( TN_is_register(OP_opnd(op, 0)) &&
	  TNs_Are_Equivalent(OP_opnd(op, 0),OP_result(op, 0))  &&
	  // Do not convert leaq $symbol(%rsi), %rsi to 
	  // addq $symbol, %rsi (no space for 64-bit immediate)
	  !TN_is_symbol(OP_opnd(op, 1))) {
	new_op = Mk_OP ((code == TOP_lea32)?TOP_addi32:TOP_addi64,
			OP_result(op, 0), OP_opnd(op, 0), 
			OP_opnd(op, 1));
      }
      // convert 'leaq 0(%rsp), %rax' to 'movq %rsp, %rax'
      else if (!TN_is_symbol (OP_opnd(op, 1)) && 
	       TN_value(OP_opnd(op, 1)) == 0) {
	new_op = Mk_OP ((code == TOP_lea32)?TOP_mov32:TOP_mov64,
			OP_result(op, 0), OP_opnd(op, 0));
      }
      // convert 'leaq 0(%rsp), %rax' to 'movq %rsp, %rax'
      else if (TN_is_symbol (OP_opnd(op, 1)) && 
	       (base == SP_Sym || base == FP_Sym) &&
	       EBO_in_peep &&
	       offset == 0) {
	new_op = Mk_OP ((code == TOP_lea32)?TOP_mov32:TOP_mov64,
			OP_result(op, 0), OP_opnd(op, 0));
      }

      if (rflags_read && new_op &&
	  ((TOP_is_change_rflags( OP_code(new_op) ) &&
	    !TOP_is_change_rflags( OP_code(op) )) ||
	   (!TOP_is_change_rflags( OP_code(new_op) ) &&
	    TOP_is_change_rflags( OP_code(op) ))))
	return FALSE;

      if (new_op) {
	if( EBO_Trace_Data_Flow ){
	  fprintf( TFile, "Lea_Insertion removes " );
	  Print_OP_No_SrcLine(op);
	  fprintf( TFile, "Lea_Insertion inserts " );
	  Print_OP_No_SrcLine(new_op);
	}
      }
      break;
    }
  case TOP_leax32:
  case TOP_leax64:
    {
      OP* alu_op = actual_tninfo[0]->in_op;
      if( TN_is_register(OP_opnd(op, 0)) &&
	  TN_is_register(OP_opnd(op, 1)) &&
	  TN_is_register(OP_result(op, 0)) &&
	  !TNs_Are_Equivalent(OP_opnd(op, 0),OP_result(op, 0)) &&
	  !TNs_Are_Equivalent(OP_opnd(op, 1),OP_result(op, 0))) {
	//opnd0 and result are not equivalent
	if (alu_op && alu_op->bb == op->bb && 
	    OP_code(alu_op) == code && 
	    TN_value(OP_opnd(alu_op, 2)) == 1 &&
	    !TN_is_symbol(OP_opnd(alu_op, 3)) &&
	    TN_value(OP_opnd(alu_op, 3)) == 0 &&
	    TN_value(OP_opnd(op, 2)) == 1 &&
	    !TN_is_symbol(OP_opnd(op, 3)) &&
	    TN_value(OP_opnd(op, 3)) == 0 &&
	    TN_is_register(OP_opnd(alu_op, 0)) &&
	    TN_is_register(OP_opnd(alu_op, 1)) &&
	    TN_is_register(OP_opnd(op, 1)) &&
	    TNs_Are_Equivalent(OP_opnd(alu_op, 0),OP_opnd(op, 1)) &&
	    TNs_Are_Equivalent(OP_opnd(alu_op, 0),OP_opnd(alu_op, 1)) &&
	    !TN_live_out_of(OP_result(alu_op, 0), OP_bb(alu_op)) &&
	    ((op->next != NULL &&
	      !is_live_tn(op->next, OP_result(alu_op, 0))) ||
	     (op->next == NULL && 
	      !GTN_SET_MemberP(BB_live_out(OP_bb(op)), OP_result(alu_op, 0)))) &&
	    Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0)) &&
	    Check_No_Redef_Between(alu_op, op, OP_opnd(alu_op, 0))) {
	  EBO_TN_INFO *src0_info, *src1_info;
	  src0_info = get_tn_info( OP_opnd(op, 1));
	  src1_info = get_tn_info( OP_opnd(alu_op, 0));

	  if (src0_info && src1_info &&
	      src0_info->sequence_num <= src1_info->sequence_num) {
	    new_op = Mk_OP ((code == TOP_leax32)?TOP_leax32:TOP_leax64, 
			    OP_result(op, 0), OP_opnd(alu_op, 0), 
			    OP_opnd(alu_op, 1), 
			    Gen_Literal_TN(2, 4), Gen_Literal_TN(0, 4));	  

	    if (rflags_read && 
		((TOP_is_change_rflags( OP_code(new_op) ) &&
		  !TOP_is_change_rflags( OP_code(op) )) ||
		 (!TOP_is_change_rflags( OP_code(new_op) ) &&
		  TOP_is_change_rflags( OP_code(op) ))))
	      return FALSE;

	    if (alu_op_defines_rflags_used(alu_op, op))
	      return FALSE;

	    OP_srcpos( new_op ) = OP_srcpos( op );
	    if( EBO_Trace_Data_Flow ){
	      fprintf( TFile, "Lea_Insertion merges " );
	      Print_OP_No_SrcLine(op);
	      fprintf( TFile, "and " );
	      Print_OP_No_SrcLine(alu_op);
	      fprintf( TFile, "with " );
	      Print_OP_No_SrcLine(new_op);
	    }
	    BB_Remove_Op(OP_bb(alu_op), alu_op);
	  }
	} else if (CG_fold_shiftadd && alu_op && alu_op->bb == op->bb && 
		   ((OP_code(alu_op) == TOP_leaxx64 && 
		     code == TOP_lea64) ||
		    (OP_code(alu_op) == TOP_leaxx32 && 
		     code == TOP_lea32)) &&
		   TN_value(OP_opnd(op, 2)) == 1 &&
		   !TN_is_symbol(OP_opnd(alu_op, 2)) &&
		   TN_value(OP_opnd(alu_op, 2)) == 0 &&
		   !TN_is_symbol(OP_opnd(op, 3)) &&
		   TN_value(OP_opnd(op, 3)) == 0 &&
		   !TN_live_out_of(OP_result(alu_op, 0), OP_bb(alu_op)) &&
		   ((op->next != NULL &&
		     !is_live_tn(op->next, OP_result(alu_op, 0))) ||
		    (op->next == NULL && 
		     !GTN_SET_MemberP(BB_live_out(OP_bb(op)), 
				      OP_result(alu_op, 0)))) &&
		   Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0))) {
	  // Fold leaxx into a following leax.
	  // Enabling this slows down crafty by 1 second but we would just 
	  // have the code around to see if it may help in other cases.
	  new_op = Mk_OP ((code == TOP_leax32)?TOP_leax32:TOP_leax64, 
			  OP_result(op, 0), OP_opnd(op, 1), 
			  OP_opnd(alu_op, 0), OP_opnd(alu_op, 1),
			  Gen_Literal_TN(0, 4));

	  if (alu_op_defines_rflags_used(alu_op, op))
	    return FALSE;

	  OP_srcpos( new_op ) = OP_srcpos( op );
	  if( EBO_Trace_Data_Flow ){
	    fprintf( TFile, "Lea_Insertion merges " );
	    Print_OP_No_SrcLine(op);
	    fprintf( TFile, "and " );
	    Print_OP_No_SrcLine(alu_op);
	    fprintf( TFile, "with " );
	    Print_OP_No_SrcLine(new_op);
	  }
	  BB_Remove_Op(OP_bb(alu_op), alu_op);
	} else { 

	  // check for second operand
	  alu_op = actual_tninfo[1]->in_op;
	  if (alu_op && alu_op->bb == op->bb && 
	      OP_code(alu_op) == code && 
	      TN_value(OP_opnd(alu_op, 2)) == 1 &&
	      !TN_is_symbol(OP_opnd(alu_op, 3)) &&
	      TN_value(OP_opnd(alu_op, 3)) == 0 &&
	      TN_value(OP_opnd(op, 2)) == 1 &&
	      !TN_is_symbol(OP_opnd(op, 3)) &&
	      TN_value(OP_opnd(op, 3)) == 0 &&
	      TN_is_register(OP_opnd(alu_op, 0)) &&
	      TN_is_register(OP_opnd(alu_op, 1)) &&
	      TN_is_register(OP_opnd(op, 0)) &&
	      TNs_Are_Equivalent(OP_opnd(alu_op, 0),OP_opnd(op, 0)) &&
	      TNs_Are_Equivalent(OP_opnd(alu_op, 0),OP_opnd(alu_op, 1)) &&
	      !TN_live_out_of(OP_result(alu_op, 0), OP_bb(alu_op)) &&
	      ((op->next != NULL &&
		!is_live_tn(op->next, OP_result(alu_op, 0))) ||
	       (op->next == NULL && 
		!GTN_SET_MemberP(BB_live_out(OP_bb(op)), OP_result(alu_op, 0)))) &&
	      Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0))) {
	    EBO_TN_INFO *src0_info, *src1_info;
	    src0_info = get_tn_info( OP_opnd(op, 0));
	    src1_info = get_tn_info( OP_opnd(alu_op, 0));
	    if (src0_info && src1_info &&
		src0_info->sequence_num <= src1_info->sequence_num) {
	      new_op = Mk_OP ((code == TOP_leax32)?TOP_leax32:TOP_leax64, 
			      OP_result(op, 0), OP_opnd(alu_op, 0), 
			      OP_opnd(alu_op, 1), 
			      Gen_Literal_TN(2, 4), Gen_Literal_TN(0, 4));	  

	      if (rflags_read && 
		  ((TOP_is_change_rflags( OP_code(new_op) ) &&
		    !TOP_is_change_rflags( OP_code(op) )) ||
		   (!TOP_is_change_rflags( OP_code(new_op) ) &&
		    TOP_is_change_rflags( OP_code(op) ))))
		return FALSE;

	      if (alu_op_defines_rflags_used(alu_op, op))
		return FALSE;

	      OP_srcpos( new_op ) = OP_srcpos( op );
	      if( EBO_Trace_Data_Flow ){
		fprintf( TFile, "Lea_Insertion merges " );
		Print_OP_No_SrcLine(op);
		fprintf( TFile, "and " );
		Print_OP_No_SrcLine(alu_op);
		fprintf( TFile, "with " );
		Print_OP_No_SrcLine(new_op);
	      }
	      BB_Remove_Op(OP_bb(alu_op), alu_op);
	    }
	  } else if (CG_fold_shiftadd && alu_op && alu_op->bb == op->bb && 
		     ((OP_code(alu_op) == TOP_leaxx64 && 
		       code == TOP_leax64) ||
		      (OP_code(alu_op) == TOP_leaxx32 && 
		       code == TOP_leax32)) &&
		     !TN_is_symbol(OP_opnd(alu_op, 2)) &&
		     TN_value(OP_opnd(alu_op, 2)) == 0 &&
		     TN_value(OP_opnd(op, 2)) == 1 &&
		     !TN_is_symbol(OP_opnd(op, 3)) &&
		     TN_value(OP_opnd(op, 3)) == 0 &&
		     !TN_live_out_of(OP_result(alu_op, 0), OP_bb(alu_op)) &&
		     ((op->next != NULL &&
		       !is_live_tn(op->next, OP_result(alu_op, 0))) ||
		      (op->next == NULL && 
		       !GTN_SET_MemberP(BB_live_out(OP_bb(op)), 
					OP_result(alu_op, 0)))) &&
		     Check_No_Use_Between(alu_op, op, OP_result(alu_op, 0))) {
	    // Fold leaxx into a following leax.
	    // Enabling this slows down crafty by 1 second but we would just 
	    // have the code around to see if it may help in other cases.
	    new_op = Mk_OP ((code == TOP_leax32)?TOP_leax32:TOP_leax64, 
			    OP_result(op, 0), OP_opnd(op, 0), 
			    OP_opnd(alu_op, 0), OP_opnd(alu_op, 1), 
			    Gen_Literal_TN(0, 4));	  	    
	    
	    if (alu_op_defines_rflags_used(alu_op, op))
	      return FALSE;

	    OP_srcpos( new_op ) = OP_srcpos( op );
	    if( EBO_Trace_Data_Flow ){
	      fprintf( TFile, "Lea_Insertion merges " );
	      Print_OP_No_SrcLine(op);
	      fprintf( TFile, "and " );
	      Print_OP_No_SrcLine(alu_op);
	      fprintf( TFile, "with " );
	      Print_OP_No_SrcLine(new_op);
	    }
	    BB_Remove_Op(OP_bb(alu_op), alu_op);
	  }
	}
	break;
      }	    

      if( ((TN_is_register(OP_opnd(op, 0)) &&
	    TNs_Are_Equivalent(OP_opnd(op, 0),OP_result(op, 0))) ||
	   (TN_is_register(OP_opnd(op, 1)) &&
	    TNs_Are_Equivalent(OP_opnd(op, 1),OP_result(op, 0)))) &&
	  TN_value(OP_opnd(op, 2)) == 1 &&
	  !TN_is_symbol(OP_opnd(op, 3)) &&
	  TN_value(OP_opnd(op, 3)) == 0) {
	TN *opnd1, *opnd2;
	if (TN_is_register(OP_opnd(op, 0)) &&
	    TNs_Are_Equivalent(OP_opnd(op, 0),OP_result(op, 0))) {
	  opnd1 = OP_opnd(op, 0);
	  opnd2 = OP_opnd(op, 1);
	} else {
	  opnd2 = OP_opnd(op, 0);
	  opnd1 = OP_opnd(op, 1);
	}
	new_op = Mk_OP ((code == TOP_leax32)?TOP_add32:TOP_add64,
			OP_result(op, 0), opnd1, opnd2);
      }

      if (rflags_read && new_op && 
	  ((TOP_is_change_rflags( OP_code(new_op) ) &&
	    !TOP_is_change_rflags( OP_code(op) )) ||
	   (!TOP_is_change_rflags( OP_code(new_op) ) &&
	    TOP_is_change_rflags( OP_code(op) ))))
	return FALSE;

      if (new_op) {
	if( EBO_Trace_Data_Flow ){
	  fprintf( TFile, "Lea_Insertion removes " );
	  Print_OP_No_SrcLine(op);
	  fprintf( TFile, "Lea_Insertion inserts " );
	  Print_OP_No_SrcLine(new_op);
	}
      }
      break;
    }
  case TOP_leaxx32:
  case TOP_leaxx64:
    {
      if( ((TN_is_register(OP_opnd(op, 0)) &&
	    TNs_Are_Equivalent(OP_opnd(op, 0),OP_result(op, 0))))) {
	if (!TN_is_symbol(OP_opnd(op, 2)) &&
	    TN_value(OP_opnd(op, 2)) == 0) {
	  shift = Get_Power_Of_2(TN_value(OP_opnd(op, 1)));
	  new_op = Mk_OP ((code == TOP_leaxx32)?TOP_shli32:TOP_shli64,
			  OP_result(op, 0), 
			  OP_result(op, 0), Gen_Literal_TN(shift, 4));
	} 
	else if (TN_is_symbol(OP_opnd(op, 2)) &&
		 TN_value(OP_opnd(op, 1)) == 1) {
	  new_op = Mk_OP ((code == TOP_leaxx32)?TOP_addi32:TOP_addi64,
			  OP_result(op, 0), 
			  OP_result(op, 0), OP_opnd(op, 2));
	}
      }

      if (rflags_read && new_op && 
	  ((TOP_is_change_rflags( OP_code(new_op) ) &&
	    !TOP_is_change_rflags( OP_code(op) )) ||
	   (!TOP_is_change_rflags( OP_code(new_op) ) &&
	    TOP_is_change_rflags( OP_code(op) ))))
	return FALSE;

      if (new_op) {
	if( EBO_Trace_Data_Flow ){
	  fprintf( TFile, "Lea_Insertion removes " );
	  Print_OP_No_SrcLine(op);
	  fprintf( TFile, "Lea_Insertion inserts " );
	  Print_OP_No_SrcLine(new_op);
	}
      }
      break;
    }
    
  default:
    break;
  }
  if (new_op) {
    OP_srcpos(new_op) = OP_srcpos(op);
    BB_Insert_Op_After(OP_bb(op), op, new_op);
    return TRUE;
  }
  
  return FALSE;
}

BOOL
EBO_Fold_Load_Duplicate( OP* op, TN** opnd_tn, EBO_TN_INFO** actual_tninfo )
{
#if Is_True_On
  if (!(EBO_Opt_Mask & EBO_FOLD_LOAD_DUPLICATE)) return FALSE;
#endif

  if (OP_code(op) != TOP_fmovddup)
    return FALSE;

  OP* shuf_op = actual_tninfo[0]->in_op;
  if (!shuf_op || shuf_op->bb != op->bb || 
      (OP_code(shuf_op) != TOP_shufpd && 
       OP_code(shuf_op) != TOP_ldhpd &&
       OP_code(shuf_op) != TOP_ldapd))
    return FALSE;

  if (!TOP_is_load(OP_code(shuf_op)) &&
      (TN_live_out_of(OP_result(shuf_op, 0), OP_bb(shuf_op)) ||
       ((op->next != NULL &&
	 is_live_tn(op->next, OP_result(shuf_op, 0))) ||
	(op->next == NULL && 
	 GTN_SET_MemberP(BB_live_out(OP_bb(op)), 
			 OP_result(shuf_op, 0)))) ||
       !Check_No_Use_Between(shuf_op, op, OP_result(shuf_op, 0))))
    return FALSE;

  // result of shuf_op should not be redefined between shuf_op and op
  EBO_TN_INFO *src0_info, *src1_info;
  src0_info = get_tn_info( OP_opnd(op, 0));
  src1_info = get_tn_info( OP_result(shuf_op, 0));
  if (src0_info->sequence_num > src1_info->sequence_num)
    return FALSE;
    
  if (!TOP_is_load(OP_code(shuf_op)) &&
      (!TN_is_register(OP_opnd(shuf_op, 0)) ||
       !TN_is_register(OP_opnd(shuf_op, 1)) ||
       !TNs_Are_Equivalent(OP_opnd(shuf_op, 0), OP_opnd(shuf_op, 1)) ||
       TN_is_symbol(OP_opnd(shuf_op, 2)) ||
       TN_value(OP_opnd(shuf_op, 2)) != 1))
    return FALSE;

  if (TOP_is_load(OP_code(shuf_op))) /* op uses result of a load */
    shuf_op = op;

  EBO_TN_INFO *loaded_tn_info = get_tn_info( OP_opnd(shuf_op, 0) );
  OP* load = loaded_tn_info->in_op;
  if (!load || load->bb != shuf_op->bb || !TOP_is_load(OP_code(load)))
    return FALSE;

  EBO_TN_INFO *src_info = get_tn_info( OP_result(load, 0) );
  if (loaded_tn_info->sequence_num > src_info->sequence_num)
    return FALSE;
  
  OP* new_op = NULL;
  INT base_loc = OP_find_opnd_use( load, OU_base );
  INT offset_loc = OP_find_opnd_use( load, OU_offset );
  INT index_loc = OP_find_opnd_use( load, OU_index );
  INT scale_loc = OP_find_opnd_use( load, OU_scale );
  
  TN *base   = NULL;
  TN *offset   = NULL;
  TN *index   = NULL;
  TN *scale   = NULL;
  if (base_loc >= 0) 
    base = OP_opnd( load, OP_find_opnd_use( load, OU_base ) );
  if (offset_loc >= 0) 
    offset = OP_opnd( load, OP_find_opnd_use( load, OU_offset ) );
  if (index_loc >= 0) 
    index = OP_opnd( load, OP_find_opnd_use( load, OU_index ) );
  if (scale_loc >= 0) 
    scale = OP_opnd( load, OP_find_opnd_use( load, OU_scale ) );
  
  if (!offset || TN_is_symbol(offset))
    return FALSE;
  
  // base and index, if defined, should not be re-defined between
  // load and op.
  if (base && !Check_No_Redef_Between(load, op, base))
    return FALSE;
  if (index && !Check_No_Redef_Between(load, op, index))
    return FALSE;
  
  if (base && offset && index && scale)
    new_op = Mk_OP (TOP_fmovddupxx, 
		    OP_result(op, 0), 
		    OP_opnd(load, 0), 
		    OP_opnd(load, 1), 
		    OP_opnd(load, 2), 
		    OP_opnd(load, 3));
  else if (base && offset)
    new_op = Mk_OP (TOP_fmovddupx, 
		    OP_result(op, 0), 
		    OP_opnd(load, 0), 
		    OP_opnd(load, 1));
  else if (index && scale && offset)
    new_op = Mk_OP (TOP_fmovddupxxx, 
		    OP_result(op, 0), 
		    OP_opnd(load, 0), 
		    OP_opnd(load, 1), 
		    OP_opnd(load, 2));
  
  if ( op == shuf_op /* op uses result of a load */ &&
       TOP_is_vector_high_loadstore( OP_code( load ) ) ) {
    INT offset_loc = OP_find_opnd_use( new_op, OU_offset );
    INT offset_value = TN_value( OP_opnd( new_op, offset_loc ) );
    Set_OP_opnd( new_op, offset_loc, 
		 Gen_Literal_TN( offset_value - 8, 
				 TN_size( OP_opnd( new_op, offset_loc ) ) ) );    
  }
  else if ( !TOP_is_vector_high_loadstore( OP_code( load ) ) ) {
    INT offset_loc = OP_find_opnd_use( new_op, OU_offset );
    INT offset_value = TN_value( OP_opnd( new_op, offset_loc ) );
    Set_OP_opnd( new_op, offset_loc, 
		 Gen_Literal_TN( offset_value + 8, 
				 TN_size( OP_opnd( new_op, offset_loc ) ) ) );
  }

  if (new_op) {
    if (shuf_op != op) {
      if( EBO_Trace_Data_Flow ){
	fprintf( TFile, "Fold_Load_Duplicate merges " );
	Print_OP_No_SrcLine(op);
	fprintf( TFile, "and " );
	Print_OP_No_SrcLine(shuf_op);
	fprintf( TFile, "Fold_Load_Duplicate inserts " );
	Print_OP_No_SrcLine(new_op);
      }
      OP_srcpos( new_op ) = OP_srcpos( op );
      BB_Remove_Op(OP_bb(shuf_op), shuf_op);
      BB_Insert_Op_After(OP_bb(op), op, new_op);
    } else {
      if( EBO_Trace_Data_Flow ){
	fprintf( TFile, "Fold_Load_Duplicate removes " );
	Print_OP_No_SrcLine(op);
	fprintf( TFile, "Fold_Load_Duplicate inserts " );
	Print_OP_No_SrcLine(new_op);
      }
      OP_srcpos( new_op ) = OP_srcpos( op );
      BB_Insert_Op_After(OP_bb(op), op, new_op);
    }    
    return TRUE;
  }

  return FALSE;
}
