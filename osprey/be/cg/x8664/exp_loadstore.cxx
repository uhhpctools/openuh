/*
 *  Copyright (C) 2007. QLogic Corporation. All Rights Reserved.
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


/* CGEXP routines for loads and stores */
#include <stdint.h> // for UINT64_MAX
#include "elf_stuff.h"
#include "defs.h"
#include "em_elf.h"
#include "erglob.h"
#include "ercg.h"
#include "tracing.h"
#include "config.h"
#include "config_TARG.h"
#include "config_debug.h"
#include "xstats.h"
#include "topcode.h"
#include "tn.h"
#include "cg_flags.h"
#include "targ_isa_lits.h"
#include "op.h"
#include "stblock.h"
#include "data_layout.h"
#include "strtab.h"
#include "symtab.h"
#include "cg.h"
#include "cgexp.h"
#include "cgexp_internals.h"
#include "cgemit.h"	// for CG_emit_non_gas_syntax
#include "cg_flags.h"	// for CG_valgrind_friendly

static void Exp_Ldst( OPCODE opcode,
		      TN *tn,
		      ST *sym,
		      INT64 ofst,
		      BOOL indirect_call,
		      BOOL is_store,
		      BOOL is_load,
		      OPS *ops,
		      VARIANT variant );

void
Expand_Lda (TN *dest, TN *src, OPS *ops)
{
  FmtAssert(FALSE, ("NYI: Expand_Lda"));
}

static TOP
Pick_Load_Instruction (TYPE_ID rtype, TYPE_ID desc,
		       TN* base,
		       TN* ofst,
		       ISA_REGISTER_CLASS rclass)
{
  const BOOL is_64bit = MTYPE_is_size_double(rtype);
  const BOOL is_reloc_x8664_64 = ( ofst != NULL         &&
				   TN_is_symbol( ofst ) &&
				   TN_is_reloc_x8664_64( ofst ) );

  if( is_reloc_x8664_64 ){
    FmtAssert( ( base == NULL ) && ( rtype == desc ),
	       ("Illegal load operation when mcmodel is medium") );
  }

  switch (desc) {
  case MTYPE_I1:
    if( is_64bit )
      return TOP_ld8_64;
    if( base != NULL )
      return TOP_ld8_32;
    return is_reloc_x8664_64 ? TOP_ld8_m : TOP_ld8_32_n32;
  case MTYPE_U1:
    if( is_64bit )
      return TOP_ldu8_64;
    if( base != NULL )
      return TOP_ldu8_32;
    return is_reloc_x8664_64 ? TOP_ld8_m : TOP_ldu8_32_n32;
  case MTYPE_I2:
    if( is_64bit )
      return TOP_ld16_64;
    if( base != NULL )
      return TOP_ld16_32;
    return is_reloc_x8664_64 ? TOP_ld16_m : TOP_ld16_32_n32;
  case MTYPE_U2:
    if( is_64bit )
      return TOP_ldu16_64;
    if( base != NULL )
      return TOP_ldu16_32;
    return is_reloc_x8664_64 ? TOP_ld16_m : TOP_ldu16_32_n32;
  case MTYPE_I4:
    if( is_64bit )
      return TOP_ld32_64;
    if( base != NULL )
      return TOP_ld32;
    return is_reloc_x8664_64 ? TOP_ld32_m : TOP_ld32_n32;
  case MTYPE_U4:
    if( base != NULL )
      return TOP_ld32;
    return is_reloc_x8664_64 ? TOP_ld32_m : TOP_ld32_n32;
  case MTYPE_I8:
  case MTYPE_U8:
    if (rclass == ISA_REGISTER_CLASS_mmx)
      return base == NULL ? TOP_ld64_2m_n32 : TOP_ld64_2m;
    return is_reloc_x8664_64 ? TOP_ld64_m : TOP_ld64;
  case MTYPE_F4:
    if( rclass == ISA_REGISTER_CLASS_float )
      return base != NULL ? TOP_ldss : TOP_ldss_n32;
    else
      return base != NULL ? TOP_flds : TOP_flds_n32;
  case MTYPE_F8:
    if( rclass == ISA_REGISTER_CLASS_float )
      return base != NULL ? TOP_ldsd : TOP_ldsd_n32;
    else
      return base != NULL ? TOP_fldl : TOP_fldl_n32;
  case MTYPE_F16:
      return TOP_lddqu;
  case MTYPE_FQ:
    return base != NULL ? TOP_fldt : TOP_fldt_n32;
  case MTYPE_V16F4:
  case MTYPE_V16C4:
    return base != NULL ? TOP_ldaps : TOP_ldaps_n32;
  case MTYPE_V8F4:
    return base != NULL ? TOP_ldlps : TOP_ldlps_n32;
  case MTYPE_V16F8:
  case MTYPE_V16C8:
    return base != NULL ? TOP_ldapd : TOP_ldapd_n32;
  case MTYPE_V16I1:
  case MTYPE_V16I2:
  case MTYPE_V16I4:
  case MTYPE_V8I4: 
  case MTYPE_V16I8: return base != NULL ? TOP_lddqa : TOP_lddqa_n32;
  case MTYPE_M8I1:
  case MTYPE_M8I2:
  case MTYPE_M8I4:
  case MTYPE_M8F4: return base != NULL ? TOP_ld64_2m : TOP_ld64_2m_n32;
    
  case MTYPE_V:
    if (rtype != MTYPE_V)
      // use rtype to pick load (e.g. if lda)
      return Pick_Load_Instruction(rtype,rtype,base,ofst,rclass);
    // else fallthru
  default:  
    FmtAssert(FALSE, ("NYI: Pick_Load_Instruction mtype"));
    return TOP_UNDEFINED;
  }
}


static void Expand_Split_Load( OPCODE opcode,
			       TN* result, TN* base, TN* ofst, OPS* ops )
{
  const TYPE_ID desc  = OPCODE_desc(opcode);
  const TYPE_ID rtype = OPCODE_rtype(opcode);
  const TYPE_ID new_mtype = (rtype == MTYPE_I8 ? MTYPE_I4 : MTYPE_U4);
  const TYPE_ID new_desc = MTYPE_is_size_double(desc) ? new_mtype : desc;
  const TOP new_top =
    Pick_Load_Instruction( new_mtype, new_desc,
			   base,
			   ofst,
			   TN_register_class(result) );

  FmtAssert( MTYPE_is_size_double(rtype), ("NYI") );

  // Load the lower half first.

  if( base == NULL )
    Build_OP( new_top, result, ofst, ops );
  else
    Build_OP( new_top, result, base, ofst, ops );

  // Load the higher half, if necessary.
  TN* tn_h = Create_TN_Pair( result, rtype );

  if( MTYPE_is_size_double( desc ) ){
    TN* ofst_h = TN_is_symbol( ofst )
      ? Gen_Symbol_TN( TN_var(ofst), TN_offset(ofst) + 4, TN_RELOC_NONE )
      : Gen_Literal_TN( TN_value(ofst) + 4, TN_size(ofst) );

    if( base == NULL )
      Build_OP( new_top, tn_h, ofst_h, ops );
    else
      Build_OP( new_top, tn_h, base, ofst_h, ops );

    Set_OP_memory_hi( OPS_last( ops ) );

  } else {
    // Case where the source is shorter than 32 bits.

    if( MTYPE_is_unsigned( desc ) ){
      Build_OP( TOP_ldc32, tn_h, Gen_Literal_TN(0,4), ops );

    } else {
      /* Don't use a dedicated tn as operand; otherwise, later localization
	 will be confused. (bug#2777)
      */
      TN* tmp = result;

      if( TN_is_dedicated(result ) ){
	tmp = Build_TN_Like( tn_h );
	if( base == NULL )
	  Build_OP( new_top, tmp, ofst, ops );
	else
	  Build_OP( new_top, tmp, base, ofst, ops );
      }

      Build_OP( TOP_sari32, tn_h, tmp, Gen_Literal_TN(31,4), ops );
    }
  }
  
  return;
}


void
Expand_Load (OPCODE opcode, TN *result, TN *base, TN *ofst, OPS *ops)
{
  if( OP_NEED_PAIR( OPCODE_rtype(opcode) ) ){
    Expand_Split_Load( opcode, result, base, ofst, ops );
    return;
  }

  const TYPE_ID mtype = OPCODE_desc(opcode);
  const BOOL is_reloc_x8664_64 = ( ofst != NULL         &&
				   TN_is_symbol( ofst ) &&
				   TN_is_reloc_x8664_64( ofst ) );
  const TYPE_ID rtype = is_reloc_x8664_64 ? mtype : OPCODE_rtype(opcode);

  TOP top = Pick_Load_Instruction (rtype, mtype,
				   base,
				   ofst,
				   TN_register_class(result));
  Is_True (TN_is_constant(ofst), ("Expand_Load: Illegal offset TN"));

  if (top == TOP_lddqu && mtype == MTYPE_F16 &&
      (base == SP_TN || base == FP_TN) &&
      Stack_Alignment() == 16 ) {
    if (TN_has_value(ofst) &&
	(TN_value(ofst) % 16 == 0))
      top = TOP_lddqa;
    else if (TN_is_symbol(ofst)) {
      ST* base;
      INT64 offset = 0;
      Base_Symbol_And_Offset (TN_var(ofst), &base, &offset);
      offset += TN_offset(ofst);
      if (offset % 16 == 0)
	top = TOP_lddqa;
    }      
  }

  if( base == NULL ){
    if( rtype == OPCODE_rtype(opcode) ){

      if( is_reloc_x8664_64 &&
	  TN_is_dedicated( result ) ){
	TN* tmp_result = Build_TN_Like( result );
	Build_OP( top, tmp_result, ofst, ops );
	Exp_COPY( result, tmp_result, ops );

      } else 
	Build_OP( top, result, ofst, ops );

    } else {
      TN* tmp_result = Build_TN_Of_Mtype( mtype );
      Build_OP( top, tmp_result, ofst, ops );
      const TOP old_top = Pick_Load_Instruction( OPCODE_rtype(opcode),
						 mtype,
						 base,
						 NULL,
						 TN_register_class(result) );

      Exp_COPY_Ext( old_top, result, tmp_result, ops );

      if( TN_is_gra_homeable(result) ){
	Reset_TN_is_gra_homeable( result );
	Set_TN_home( result, NULL );
      }
    }
    return;
  }

  // Handle very large offsets (that consume greater than 16 bits).
  // Try and see if the base and offset can be merged.
  // An example is gcc.c-torture/compile/990617-1.c
  if (TN_is_rematerializable(base)) {
    WN *home = TN_home(base);
    if (WN_operator(home) == OPR_INTCONST) {
      INT64 val = WN_const_val(home);
      BOOL is_double = TN_size(base) == 8;
      TN *tmp_base = Gen_Literal_TN (val + TN_value(ofst), TN_size(base));
      tmp_base = Expand_Immediate_Into_Register(tmp_base, is_double, ops);
      Build_OP (top, result, tmp_base, Gen_Literal_TN (0, 2), ops);
      return;

    } else if( WN_operator(home) == OPR_LDA ){
      ST* sym = WN_st( home );
      const INT64 offset = WN_lda_offset( home ) + TN_value( ofst );
      if( ISA_LC_Value_In_Class(offset, LC_simm32) ){
	Exp_Ldst( opcode, result, sym, offset, FALSE, FALSE, TRUE, ops, 0 );
	return;
      }
    }
  }

  Build_OP (top, result, base, ofst, ops);
}

static TOP
Pick_Store_Instruction( TYPE_ID mtype,
			TN* base,
			TN* ofst,
			ISA_REGISTER_CLASS rclass )
{
  const BOOL is_reloc_x8664_64 = ( ofst != NULL         &&
				   TN_is_symbol( ofst ) &&
				   TN_is_reloc_x8664_64( ofst ) );

  switch (mtype) {
  case MTYPE_I1:
  case MTYPE_U1:
    if( base != NULL )
      return TOP_store8;
    return is_reloc_x8664_64 ? TOP_store8_m : TOP_store8_n32;
  case MTYPE_I2:
  case MTYPE_U2:
    if( base != NULL )
      return TOP_store16;
    return is_reloc_x8664_64 ? TOP_store16_m : TOP_store16_n32;
  case MTYPE_I4:
  case MTYPE_U4:
    if( base != NULL )
      return TOP_store32;
    return is_reloc_x8664_64 ? TOP_store32_m : TOP_store32_n32;
  case MTYPE_I8:
  case MTYPE_U8:
    if (rclass == ISA_REGISTER_CLASS_mmx)
      return base == NULL ? TOP_store64_fm_n32 : TOP_store64_fm;
    return is_reloc_x8664_64 ? TOP_store64_m : TOP_store64;
  case MTYPE_F4:
    if( rclass == ISA_REGISTER_CLASS_float )
      return base != NULL ? TOP_stss : TOP_stss_n32;
    else
      return base != NULL ? TOP_fstps : TOP_fstps_n32;
  case MTYPE_F8:
    if( rclass == ISA_REGISTER_CLASS_float )
      return base != NULL ? TOP_stsd : TOP_stsd_n32;
    else
      return base != NULL ? TOP_fstpl : TOP_fstpl_n32;
  case MTYPE_F16:
    return TOP_stdqu;
  case MTYPE_FQ:
    return base != NULL ? TOP_fstpt : TOP_fstpt_n32;
  case MTYPE_V16F4: 
  case MTYPE_V16C4: 
    return base != NULL ? TOP_staps : TOP_staps_n32;
  case MTYPE_V16F8: 
  case MTYPE_V16C8: 
    return base != NULL ? TOP_stapd : TOP_stapd_n32;
  case MTYPE_V8F4:
    return base != NULL ? TOP_stsd : TOP_stsd_n32;
  case MTYPE_V16I1: 
  case MTYPE_V16I2: 
  case MTYPE_V16I4: 
  case MTYPE_V16I8:
    return base != NULL ? TOP_stdqa : TOP_stdqa_n32;
  case MTYPE_M8I1:
  case MTYPE_M8I2:
  case MTYPE_M8I4:
  case MTYPE_M8F4:
    return base != NULL ? TOP_store64_fm : TOP_store64_fm_n32;
  default:  FmtAssert(FALSE, ("NYI: Pick_Store_Instruction mtype"));
    return TOP_UNDEFINED;
  }
}


static void Expand_Split_Store( TYPE_ID mtype,
				TN *src, TN *base, TN *ofst, OPS *ops )
{
  const TYPE_ID new_mtype = (mtype == MTYPE_I8 ? MTYPE_I4 : MTYPE_U4);
  const TOP new_top =
    Pick_Store_Instruction( new_mtype, base, ofst, TN_register_class(src) );
  TN* src_h = Get_TN_Pair( src );
  TN* ofst_h = TN_is_symbol( ofst )
    ? Gen_Symbol_TN( TN_var(ofst), TN_offset(ofst) + 4, TN_RELOC_NONE )
    : Gen_Literal_TN( TN_value(ofst) + 4, TN_size(ofst) );

  if( src_h == NULL ){
    src_h = Build_TN_Like( src );
    Build_OP( TOP_ldc32, src_h, Gen_Literal_TN( 0, 4 ), ops );
  }

  if( base != NULL ){
    Build_OP( new_top, src,   base, ofst, ops );

    Build_OP( new_top, src_h, base, ofst_h, ops );
    Set_OP_memory_hi( OPS_last( ops ) );

  } else {
    Build_OP( new_top, src,   ofst, ops );

    Build_OP( new_top, src_h, ofst_h, ops );
    Set_OP_memory_hi( OPS_last( ops ) );
  }



  return;
}


void
Expand_Store (TYPE_ID mtype, TN *src, TN *base, TN *ofst, OPS *ops)
{
  TOP top =
    Pick_Store_Instruction( mtype, base, ofst, TN_register_class(src) );
  Is_True (TN_is_constant(ofst), ("Expand_Store: Illegal offset TN"));

  if (top == TOP_stdqu && mtype == MTYPE_F16 &&
      (base == SP_TN || base == FP_TN) &&
      Stack_Alignment() == 16 ) {
    if (TN_has_value(ofst) &&
	(TN_value(ofst) % 16 == 0))
      top = TOP_stdqa;
    else if (TN_is_symbol(ofst)) {
      ST* base;
      INT64 offset = 0;
      Base_Symbol_And_Offset (TN_var(ofst), &base, &offset);
      offset += TN_offset(ofst);
      if (offset % 16 == 0)
	top = TOP_stdqa;
    }      
  }

  if( OP_NEED_PAIR( mtype ) ){
    Expand_Split_Store( mtype, src, base, ofst, ops );
    return;
  }

  if (!TN_has_value(ofst) || ISA_LC_Value_In_Class(TN_value(ofst), LC_simm32)){
    if( base != NULL )
      Build_OP (top, src, base, ofst, ops);
    else
      Build_OP (top, src, ofst, ops);

  } else {
    if( base != NULL )
      Build_OP(top, src, base, Gen_Literal_TN(TN_value(ofst), 4), ops);
    else
      Build_OP(top, src, Gen_Literal_TN(TN_value(ofst), 4), ops);
  }
}

static OPCODE 
OPCODE_make_signed_op(OPERATOR op, TYPE_ID rtype, TYPE_ID desc, BOOL is_signed)
{
  if (MTYPE_is_signed(rtype) != is_signed)
	rtype = MTYPE_complement(rtype);
  if (MTYPE_is_signed(desc) != is_signed)
	desc =	MTYPE_complement(desc);

  return OPCODE_make_op(op, rtype, desc);
}

/* ====================================================================
 *
 * Adjust_Addr_TNs
 *
 * We have a memory reference operation, with a base and displacement,
 * where the displacement is literal.  We want to create another memop
 * with the displacement modified by a small amount.
 *
 * WARNING:  If an add operation is required, it will be expanded here.
 *
 * ====================================================================
 */

static void
Adjust_Addr_TNs (
  TOP	opcode,		/* The new memory operation */
  TN	**base_tn,	/* The base address -- may be modified */
  TN	**disp_tn,	/* The displacement -- may be modified */
  INT16	disp,		/* A displacement to add */
  OPS *ops)
{

  if ( Potential_Immediate_TN_Expr (opcode, *disp_tn, disp) )
  {
    if ( TN_has_value(*disp_tn) ) {
      *disp_tn = Gen_Literal_TN ( TN_value(*disp_tn) + disp, 4 );
    } else {
      *disp_tn = Gen_Symbol_TN ( TN_var(*disp_tn),
				 TN_offset(*disp_tn) + disp, 0);
    }
  } else {
    TN *tmp = Build_TN_Of_Mtype (Pointer_Mtype);
    // because disp may be symbolic reloc on base,
    // want to still add it with base and create new base and disp.
    Expand_Add (tmp, *disp_tn, *base_tn, Pointer_Mtype, ops);
    *base_tn = tmp;
    *disp_tn = Gen_Literal_TN (disp, 4);
  }
}

static void
Expand_Composed_Load ( OPCODE op, TN *result, TN *base, TN *disp, VARIANT variant, OPS *ops)
{
  return Expand_Load( op, result, base, disp, ops );
}

void
Expand_Misaligned_Load ( OPCODE op, TN *result, TN *base, TN *disp, VARIANT variant, OPS *ops)
{
  TYPE_ID mtype = OPCODE_rtype(op);

  if (mtype == MTYPE_V8I1 || mtype == MTYPE_V8I2 ||
      mtype == MTYPE_V8I4 ||
      mtype == MTYPE_V16I1 || mtype == MTYPE_V16I2 ||
      mtype == MTYPE_V16I4 || mtype == MTYPE_V16I8)
    Build_OP (TOP_lddqu, result, base, disp, ops);    
  else if (mtype == MTYPE_V16F8 || mtype == MTYPE_V16C8) {
    TN* ofst = TN_is_symbol( disp )
      ? Gen_Symbol_TN( TN_var(disp), TN_offset(disp) + 8, TN_RELOC_NONE )
      : Gen_Literal_TN( TN_value(disp) + 8, TN_size(disp) );
    if (base != NULL) {
      Build_OP (TOP_ldlpd, result, base, disp, ops);    
      Build_OP (TOP_ldhpd, result, base, ofst, ops);    
    }
    else {
      Build_OP (TOP_ldlpd_n32, result, disp, ops);    
      Build_OP (TOP_ldhpd_n32, result, ofst, ops);    
    }
    Set_OP_cond_def_kind(OPS_last(ops), OP_ALWAYS_COND_DEF);
  }
  else if (mtype == MTYPE_V16F4 || mtype == MTYPE_V16C4) {
    TN* ofst = TN_is_symbol( disp )
      ? Gen_Symbol_TN( TN_var(disp), TN_offset(disp) + 8, TN_RELOC_NONE )
      : Gen_Literal_TN( TN_value(disp) + 8, TN_size(disp) );
    Build_OP (TOP_ldlps, result, base, disp, ops);    
    Build_OP (TOP_ldhps, result, base, ofst, ops);    
    Set_OP_cond_def_kind(OPS_last(ops), OP_ALWAYS_COND_DEF);
  }
  else
    Expand_Composed_Load ( op, result, base, disp, variant, ops );
}


static void
Expand_Composed_Store (TYPE_ID mtype, TN *obj, TN *base, TN *disp, VARIANT variant, OPS *ops)
{
  return Expand_Store( mtype, obj, base, disp, ops );
}

void
Expand_Misaligned_Store (TYPE_ID mtype, TN *obj_tn, TN *base_tn, TN *disp_tn, VARIANT variant, OPS *ops)
{
  if (mtype == MTYPE_V16I1 || mtype == MTYPE_V16I2 ||
      mtype == MTYPE_V16I4 || mtype == MTYPE_V16I8)
    Build_OP (TOP_stdqu, obj_tn, base_tn, disp_tn, ops);
  else if (mtype == MTYPE_V16F4 || mtype == MTYPE_V16C4) {
    TN* ofst = TN_is_symbol( disp_tn )
      ? Gen_Symbol_TN( TN_var(disp_tn), TN_offset(disp_tn) + 8, TN_RELOC_NONE )
      : Gen_Literal_TN( TN_value(disp_tn) + 8, TN_size(disp_tn) );
    Build_OP (TOP_stlps, obj_tn, base_tn, disp_tn, ops);    
    Build_OP (TOP_sthps, obj_tn, base_tn, ofst, ops);    
    Set_OP_cond_def_kind(OPS_last(ops), OP_ALWAYS_COND_DEF);
  } 
  else if (mtype == MTYPE_V16F8 || mtype == MTYPE_V16C8) {
    TN* ofst = TN_is_symbol( disp_tn )
      ? Gen_Symbol_TN( TN_var(disp_tn), TN_offset(disp_tn) + 8, TN_RELOC_NONE )
      : Gen_Literal_TN( TN_value(disp_tn) + 8, TN_size(disp_tn) );
    if (base_tn != NULL) {
      Build_OP (TOP_stlpd, obj_tn, base_tn, disp_tn, ops);    
      Build_OP (TOP_sthpd, obj_tn, base_tn, ofst, ops);    
    }
    else {
      Build_OP (TOP_stlpd_n32, obj_tn, disp_tn, ops);    
      Build_OP (TOP_sthpd_n32, obj_tn, ofst, ops);    
    }
    Set_OP_cond_def_kind(OPS_last(ops), OP_ALWAYS_COND_DEF);
  }
  else
    Expand_Composed_Store (mtype, obj_tn, base_tn, disp_tn, variant, ops);
}

static BOOL is_stack_used = FALSE;

BOOL Is_Stack_Used()
{
  const BOOL state = is_stack_used;
  is_stack_used = FALSE;
  return state;
}

void
CG_Set_Is_Stack_Used()
{
  is_stack_used = TRUE;
}

static void
Exp_Ldst (
  OPCODE opcode,
  TN *tn,
  ST *sym,
  INT64 ofst,
  BOOL indirect_call,
  BOOL is_store,
  BOOL is_load,
  OPS *ops,
  VARIANT variant)
{
  ST* base_sym = NULL;
  INT64 base_ofst = 0;
  TN* base_tn = NULL;
  TN* ofst_tn = NULL;
  TN* tmp_tn = NULL;
  const BOOL is_lda = (!is_load && !is_store);
  OPS newops = OPS_EMPTY;
  OP* op = NULL;
  BOOL on_stack = FALSE;

  if (Trace_Exp2) {
    fprintf(TFile, "exp_ldst %s: ", OPCODE_name(opcode));
    if (tn) Print_TN(tn,FALSE);
    if (is_store) fprintf(TFile, " -> ");
    else fprintf(TFile, " <- ");
    if (ST_class(sym) != CLASS_CONST)
      fprintf(TFile, "%lld (%s)\n", ofst, ST_name(sym));
    else
      fprintf(TFile, "%lld ()\n", ofst);
  }
  
  Allocate_Object(sym);         /* make sure sym is allocated */
  
  Base_Symbol_And_Offset_For_Addressing (sym, ofst, &base_sym, &base_ofst);

  if (base_sym == SP_Sym || base_sym == FP_Sym) {
    is_stack_used = on_stack = TRUE;

    base_tn = (base_sym == SP_Sym) ? SP_TN : FP_TN;
    if (sym == base_sym) {
      // can have direct reference to SP or FP,
      // e.g. if actual stored to stack.
      if (ISA_LC_Value_In_Class(base_ofst, LC_simm32))
	ofst_tn = Gen_Literal_TN (base_ofst, 4);
      else {
	FmtAssert( false, ("NYI") );
      }

    } else {
      /* Because we'd like to see symbol name in .s file, 
       * still reference the symbol rather than the sp/fp base.  
       * Do put in the offset from the symbol.  
       * We put the symbol in the TN and then
       * let cgemit replace symbol with the final offset.
       * We generate a SW reg, <sym>, <SP> rather than SW reg,<sym>
       * because cgemit and others expect a separate tn for the
       * offset and base. 
       */
      if (ISA_LC_Value_In_Class(base_ofst, LC_simm32))
	ofst_tn = Gen_Symbol_TN (sym, ofst, 0);
      else if (! is_lda) {
	FmtAssert( Is_Target_64bit(), ("NYI: 64-bit offset under -m32"));
	TN* tmp_tn = Build_TN_Of_Mtype(Pointer_Mtype);
	TN* result_tn = Build_TN_Like( base_tn );

	Build_OP( TOP_ldc64, tmp_tn, Gen_Literal_TN(base_ofst,8), &newops);
	// Preserve x86-style property for add64 because this OP can be created
	// by LRA for spilling callee-saved regs in the prologue/epilog, after
	// LRA has changed OPs to x86-style.  Bug 7304.
	// We are not allowed to overwrite base_tn.  Instead, add to tmp_tn and
	// call it base_tn.  Bug 9188.
	Build_OP( TOP_add64, tmp_tn, tmp_tn, base_tn, &newops );
	base_tn = tmp_tn;
	ofst_tn = Gen_Literal_TN( 0, 4 );
      }
    }

  } else if ((ST_class(base_sym) == CLASS_BLOCK || ST_class(base_sym)==CLASS_VAR)
	     && ST_gprel(base_sym)) {
    FmtAssert( FALSE, ("x86 does not use gp\n") );
    

  } else {
    if( !ST_is_export_local(base_sym) &&
	!ISA_LC_Value_In_Class(base_ofst, LC_simm32) &&
	mcmodel < MEDIUM ){
      // use %got_page and %got_offset
#if 0 // bug 4622
      FmtAssert( FALSE, ("offset cannot be fit into 32-bit") );
#endif
    }
  }

  if (!strncmp (".gnu.linkonce.", ST_name (base_sym), strlen (".gnu.linkonce."))
      && (ofst == base_ofst))
    base_sym = sym;
  
  if( is_lda ){
    if( ofst_tn != NULL && TN_is_symbol( ofst_tn ) ){
      Build_OP( Is_Target_64bit() ? TOP_lea64 : TOP_lea32,
		tn, base_tn, ofst_tn, &newops );

    } else {
      if (on_stack) {
	if ( ofst_tn != NULL )
	  Build_OP( Is_Target_64bit() ? TOP_addi64 : TOP_addi32, tn,
		    base_sym == SP_Sym ? SP_TN : FP_TN, ofst_tn,
		    &newops );
	else {
	  FmtAssert( Is_Target_64bit(), ("NYI: 64-bit offset under -m32"));
	  TN *tmp_tn = Build_TN_Of_Mtype(MTYPE_I8);
	  Build_OP( TOP_ldc64, tmp_tn, Gen_Literal_TN(base_ofst, 8), &newops );
	  Build_OP( TOP_add64, tn, tmp_tn, base_sym == SP_Sym ? SP_TN : FP_TN, 
		    &newops );
	}
      }
      else if( Is_Target_64bit() ){
	if (Gen_PIC_Shared) {
	  if (! ST_is_export_local(base_sym) ) {
	    TN *tmp = base_ofst == 0 ? tn : Build_TN_Like(tn);
	    Build_OP( TOP_ld64, tmp, Rip_TN(), 
		      Gen_Symbol_TN( base_sym, 0, TN_RELOC_X8664_GOTPCREL ),
		      &newops );
	    // got address should not alias
	    Set_OP_no_alias(OPS_last(&newops));
	    if (base_ofst != 0)
	      Build_OP( TOP_addi64, tn, tmp, Gen_Literal_TN(base_ofst, 8),
		        &newops );
	  }
	  else Build_OP( TOP_lea64, tn, Rip_TN(), 
			 Gen_Symbol_TN( base_sym, base_ofst, TN_RELOC_NONE ),
			 &newops );	      
	}
	else if (ISA_LC_Value_In_Class(base_ofst, LC_simm32) &&
		 mcmodel < MEDIUM ){
	  Build_OP( TOP_ldc64, tn,
		    Gen_Symbol_TN( base_sym, base_ofst, TN_RELOC_X8664_PC32 ),
		    &newops );
	} else {
#if 0 // bug 4622
	  FmtAssert( mcmodel >= MEDIUM, ("code model is not medium or higher") );
#endif
	  TN* sym_tn = NULL;

	  if( ISA_LC_Value_In_Class(base_ofst, LC_simm32) ){
	    Build_OP( TOP_movabsq,
		      tn, Gen_Symbol_TN( base_sym, base_ofst, TN_RELOC_X8664_64 ),
		      &newops );

	  } else {
	    TN* tmp_tn = Build_TN_Of_Mtype(Pointer_Mtype);
	    TN* const_tn = Build_TN_Of_Mtype(Pointer_Mtype);

	    Build_OP( TOP_movabsq,
		      tmp_tn, Gen_Symbol_TN( base_sym, 0, TN_RELOC_X8664_64 ),
		      &newops );

	    Build_OP( TOP_movabsq, const_tn, Gen_Literal_TN(base_ofst,8), &newops );
	    Build_OP( TOP_add64, tn, tmp_tn, const_tn, &newops );
	    base_ofst = 0;
	  }
	}

      } else {
	// The Is_Target_32bit() cases.

	FmtAssert(ISA_LC_Value_In_Class(base_ofst, LC_simm32),
		  ("NYI: 64-bit offset under -m32"));

	if( Gen_PIC_Shared && !ST_is_export_local(base_sym) ){
	  TN* tmp = base_ofst == 0 ? tn : Build_TN_Like(tn);
	  Build_OP( TOP_ld32, tmp, Ebx_TN(),
		    Gen_Symbol_TN( base_sym, 0, TN_RELOC_IA32_GOT ),
		    &newops );
	  // got address should not alias
	  Set_OP_no_alias(OPS_last(&newops));
	  PU_References_GOT = TRUE;

	  if( base_ofst != 0 ){
	    Build_OP( TOP_addi32, tn, tmp, Gen_Literal_TN(base_ofst, 4), &newops );
	  }

	} else {
	  Build_OP( TOP_ldc32, tn,
		    Gen_Symbol_TN( base_sym, base_ofst, TN_RELOC_NONE ),
		    &newops );
	}
      }
    }

  } else {
    if( base_tn == NULL ){
      Is_True(! on_stack, ("Exp_Ldst: unexpected stack reference"));

      if( Is_Target_64bit() ){
	if( mcmodel < MEDIUM && ISA_LC_Value_In_Class(base_ofst, LC_simm32) )
	    base_tn = Rip_TN();
	else {

	  if( ISA_LC_Value_In_Class(base_ofst, LC_simm32) ){
	    ofst_tn = Gen_Symbol_TN( base_sym, base_ofst, TN_RELOC_X8664_64 );
	    base_ofst = 0;

	  } else {
	    ofst_tn = Gen_Symbol_TN( base_sym, 0, TN_RELOC_X8664_64 );
	  }

	  if( base_ofst != 0 ){
// Bug 4461
            base_tn = Build_TN_Of_Mtype(Pointer_Mtype);
                                                                                                                                                             
            Build_OP( TOP_movabsq,
                      base_tn, Gen_Symbol_TN( base_sym, base_ofst, TN_RELOC_X8664_64 ),
                      &newops );
            ofst_tn = Gen_Literal_TN(0, 4);
            base_ofst = 0;
	  }

	  /* The target of a load operation under -mcmodel=medium is %rax.
	     Make sure no conflicts will happen; otherwise, use indirect load.

	     As quoted from i386.md:
	     ;; Stores and loads of ax to arbitrary constant address.
	     ;; We fake an second form of instruction to force reload to load address
	     ;; into register when rax is not available
	  */
	  bool use_iload = 
	    ( ( is_load  && !MTYPE_is_integral( OPCODE_rtype(opcode) ) ) ||
	      ( is_store && !MTYPE_is_integral( OPCODE_desc(opcode) ) ) );

	  if( !use_iload ){
	    /* If <tn> holds a register already, very likely this routine
	       is called by gra spilling. If so, use iload to avoid disturbing
	       the spilling routine.
	    */
	    if( TN_register(tn) != REGISTER_UNDEFINED )
	      use_iload = (base_sym == SP_Sym || base_sym == FP_Sym);
	    else {
	      for( OP* op = OPS_last(ops);
		   op != NULL;
		   op = OP_prev(op) ){
		if( OP_Defs_Reg( op, ISA_REGISTER_CLASS_integer, RAX ) ){
		  use_iload = TRUE;
		  break;
		}
	      }
	    }
	  }

	  if( use_iload && base_tn == NULL){
	    base_tn = Build_TN_Of_Mtype(Pointer_Mtype);
	    Build_OP( TOP_movabsq, base_tn, ofst_tn, &newops );
	    ofst_tn = Gen_Literal_TN( 0, 4 );
	  }
	}
      }

      if( Gen_PIC_Shared && !ST_is_export_local(base_sym) ){
	if( Is_Target_64bit() ){
	  TN *new_base = Build_TN_Of_Mtype(Pointer_Mtype);
	  Build_OP (TOP_ld64, new_base, base_tn, 
		    Gen_Symbol_TN(base_sym, 0, TN_RELOC_X8664_GOTPCREL),
		    &newops);
	  // got address should not alias
	  Set_OP_no_alias(OPS_last(&newops));
	  base_tn = new_base;
	  ofst_tn = Gen_Literal_TN( base_ofst, 4 );

	} else {
	  // for -m32 here
	  TN *new_base = Build_TN_Of_Mtype(Pointer_Mtype);
	  Build_OP (TOP_ld32, new_base, Ebx_TN(), 
		    Gen_Symbol_TN(base_sym, 0, TN_RELOC_IA32_GOT),
		    &newops);
	  // got address should not alias
	  Set_OP_no_alias(OPS_last(&newops));
	  PU_References_GOT = TRUE;
	  base_tn = new_base;
	  ofst_tn = Gen_Literal_TN( base_ofst, 4 );
	}
      }
      else if( ofst_tn == NULL ){
	ofst_tn = Gen_Symbol_TN(base_sym, base_ofst, TN_RELOC_NONE);
      }
    }

    if( is_store ){
      if ( opcode == OPC_V16C8STID || 
	   V_align_all(variant) != 0 ) // Bug 3623 - check if misaligned STID
	Expand_Misaligned_Store (OPCODE_desc(opcode), tn, base_tn, ofst_tn, 
	                         variant, &newops);
      else
	Expand_Store (OPCODE_desc(opcode), tn, base_tn, ofst_tn, &newops);

    } else if( is_load ){
      if ( opcode == OPC_V16C8V16C8LDID ||
	   V_align_all(variant) != 0 ) // Bug 3623 - check if misaligned LDID
	Expand_Misaligned_Load ( opcode, tn, base_tn, ofst_tn,
			         variant, &newops );
      else
	Expand_Load ( opcode, tn, base_tn, ofst_tn, &newops );

    }
  }

  FOR_ALL_OPS_OPs (&newops, op) {
    if (is_load && ST_is_constant(sym) && OP_load(op)) {
      // If we expanded a load of a constant, 
      // nothing else can alias with the loads 
      // we have generated.
      Set_OP_no_alias(op);
    }
    if (Trace_Exp2) {
      fprintf(TFile, "exp_ldst into "); Print_OP (op);
    }
  }
  /* Add the new OPs to the end of the list passed in */
  OPS_Append_Ops(ops, &newops);
}

void Exp_Lda ( 
  TYPE_ID mtype, 
  TN *tgt_tn, 
  ST *sym, 
  INT64 ofst, 
  OPERATOR call_opr,
  OPS *ops)
{
  OPCODE opcode = OPCODE_make_op(OPR_LDA, mtype, MTYPE_V);
  Exp_Ldst (opcode, tgt_tn, sym, ofst, 
	(call_opr == OPR_ICALL),
	FALSE, FALSE, ops, 0);
}

void
Exp_Load (
  TYPE_ID rtype, 
  TYPE_ID desc, 
  TN *tgt_tn, 
  ST *sym, 
  INT64 ofst, 
  OPS *ops, 
  VARIANT variant)
{
  OPCODE opcode = OPCODE_make_op (OPR_LDID, rtype, desc);
  Exp_Ldst (opcode, tgt_tn, sym, ofst, FALSE, FALSE, TRUE, ops, variant);
}

void
Exp_Store (
  TYPE_ID mtype, 
  TN *src_tn, 
  ST *sym, 
  INT64 ofst, 
  OPS *ops, 
  VARIANT variant)
{
  OPCODE opcode = OPCODE_make_op(OPR_STID, MTYPE_V, mtype);
  Exp_Ldst (opcode, src_tn, sym, ofst, FALSE, TRUE, FALSE, ops, variant);
  if (TN_register_class(src_tn) == ISA_REGISTER_CLASS_mmx)
    Build_OP(TOP_emms, ops); // bug 11800
}

static ISA_ENUM_CLASS_VALUE
Pick_Prefetch_Hint (VARIANT variant)
{
  UINT32 pf_flags = V_pf_flags(variant);
  if (PF_GET_READ(pf_flags)) {
    if (PF_GET_STRIDE_1L(pf_flags))
      return ECV_pfhint_L1_load;
    else return ECV_pfhint_L2_load;
  }
  else {
    if (PF_GET_STRIDE_1L(pf_flags))
      return ECV_pfhint_L1_store;
    else return ECV_pfhint_L2_store;
  }
}

void Exp_Prefetch (TOP opc, TN* src1, TN* src2, VARIANT variant, OPS* ops)
{
  FmtAssert(opc == TOP_UNDEFINED,
            ("Prefetch opcode should be selected in Exp_Prefetch"));
  const UINT32 pf_flags = V_pf_flags(variant);
  const ISA_ENUM_CLASS_VALUE pfhint = Pick_Prefetch_Hint(variant);
  TOP top;

  if( PF_GET_WRITE(pf_flags) ){
    if( pfhint != ECV_pfhint_L1_store ){
      Build_OP( TOP_noop, ops );
      return;
    }
    if ( !Is_Target_3DNow() )
#ifdef KEY //bug 10953: LNO wants prefetchna for non-temporal prefetch
      top = PF_GET_NON_TEMPORAL(pf_flags) ? TOP_prefetchnta : TOP_prefetcht0;
#else       
      top = TOP_prefetcht0;
#endif
    else
      top = TOP_prefetchw;

  } else {
    if( pfhint == ECV_pfhint_L1_load )
    {
      if ( !Is_Target_3DNow() )
#ifdef KEY //bug 10953: LNO wants prefetchna for non-temporal prefetch
      top = PF_GET_NON_TEMPORAL(pf_flags) ? TOP_prefetchnta : TOP_prefetcht0;
#else
      top = TOP_prefetcht0;
#endif       
      else
        top = TOP_prefetch;
    }
    else if( pfhint == ECV_pfhint_L2_load )
      top = TOP_prefetcht1;
    else
      FmtAssert( false, ("NYI") );
  }

  Build_OP( top, Gen_Enum_TN(pfhint), src1, src2, ops );
}

/* ======================================================================
 * Exp_Extract_Bits
 * ======================================================================*/
void Exp_Extract_Bits (TYPE_ID rtype, TYPE_ID desc, UINT bit_offset, UINT bit_size,
		       TN *tgt_tn, TN *src_tn, OPS *ops)
{
  TN *tmp1_tn = Build_TN_Like (tgt_tn);
  UINT pos = (Target_Byte_Sex == BIG_ENDIAN || CG_emit_non_gas_syntax)
	     ? MTYPE_bit_size(desc)-bit_offset-bit_size : bit_offset;
  if (pos == 0 && bit_size <= 16 && ! MTYPE_signed(rtype)) {

    if( OP_NEED_PAIR(rtype) &&
	!OP_NEED_PAIR(desc) ){
      TN* tn_hi = Create_TN_Pair( tgt_tn, rtype );
      Exp_Immediate( tn_hi, Gen_Literal_TN(0,4), FALSE, ops );
    }

    Expand_Binary_And( tgt_tn, src_tn, Gen_Literal_TN((1 << bit_size)-1, 4),
		       desc, ops );
    return;
  }

  INT left_shift_amt = MTYPE_bit_size(rtype) - pos - bit_size;
  Expand_Shift( tmp1_tn, src_tn, Gen_Literal_TN(left_shift_amt, 4),
		rtype, shift_left, ops );

  INT right_shift_amt = MTYPE_bit_size(rtype) - bit_size;
  Expand_Shift( tgt_tn, tmp1_tn, Gen_Literal_TN(right_shift_amt, 4),
		rtype, MTYPE_is_signed(rtype) ? shift_aright : shift_lright, ops );
}

/* ======================================================================
 * Bitmask_Of_Size - forms a bit mask of 1's for the number of bits
 * ======================================================================*/
UINT64 Bitmask_Of_Size(INT bsize)
{
  Is_True(bsize != 0, ("Bitmask_Of_Size: bsize cannot be 0"));
  if (bsize >= 64)
    return UINT64_MAX;
  return ((UINT64) 1 << bsize) - 1;
}

/* ======================================================================
 * Exp_Deposit_Bits2 - deposit src2_tn into a field of src1_tn returning
 * the result in tgt_tn using the more straightforward algorithm friendly to
 * Valgrind
 * ======================================================================*/
void Exp_Deposit_Bits2(TYPE_ID rtype, TYPE_ID desc, UINT bit_offset, UINT bit_size,
		       TN *tgt_tn, TN *src1_tn, TN *src2_tn, OPS *ops)
{
  if (!bit_size) return;

  UINT targ_bit_offset = bit_offset;
  if (Target_Byte_Sex == BIG_ENDIAN) {
    targ_bit_offset = MTYPE_bit_size(desc) - bit_offset - bit_size;
  }

  if( Is_Target_32bit()           &&
      MTYPE_is_size_double(rtype) &&
      Get_TN_Pair(src2_tn) == 0 ){
    TN* pair = Create_TN_Pair( src2_tn, rtype );
    Exp_Immediate( pair, Gen_Literal_TN(0,4), false, ops );
  }

  // prepare r.h.s. value
  TN *tmp1_tn = Build_TN_Like(tgt_tn);
  int shift_amt = MTYPE_bit_size(rtype) - bit_size;
  Expand_Shift( tmp1_tn, src2_tn, Gen_Literal_TN(shift_amt, 4),
		rtype, shift_left, ops );

  shift_amt = MTYPE_bit_size(rtype) - bit_size - targ_bit_offset;
  Expand_Shift( tmp1_tn, tmp1_tn, Gen_Literal_TN(shift_amt, 4),
		rtype, shift_lright, ops );

  UINT64 bitmask = ~(Bitmask_Of_Size(bit_size) << targ_bit_offset);
  if (MTYPE_byte_size(rtype) <= 4)
    bitmask &= Bitmask_Of_Size(MTYPE_byte_size(rtype) * 8);
  TN *tmp2_tn = Build_TN_Like(tgt_tn);
  if( Is_Target_32bit()           &&
      MTYPE_is_size_double(rtype) &&
      Get_TN_Pair(tmp2_tn) == 0 )
    (void *) Create_TN_Pair( tmp2_tn, rtype );
  Exp_Immediate(tmp2_tn, Gen_Literal_TN(bitmask, MTYPE_byte_size(rtype)), FALSE, ops );

  Expand_Binary_And( tgt_tn, tmp2_tn, src1_tn, rtype, ops );
  Expand_Binary_Or( tgt_tn, tgt_tn, tmp1_tn, rtype, ops );
}

/* ======================================================================
 * Exp_Deposit_Bits - deposit src2_tn into a field of src1_tn returning
 * the result in tgt_tn.
 * ======================================================================*/
void Exp_Deposit_Bits (TYPE_ID rtype, TYPE_ID desc, UINT bit_offset, UINT bit_size,
		       TN *tgt_tn, TN *src1_tn, TN *src2_tn, OPS *ops)
{
  if (CG_valgrind_friendly) { // bug 9672
    Exp_Deposit_Bits2(rtype, desc, bit_offset, bit_size, tgt_tn, src1_tn, src2_tn, ops);
    return;
  }

  if (!bit_size) return;

  UINT targ_bit_offset = bit_offset;
  if (Target_Byte_Sex == BIG_ENDIAN) {
    targ_bit_offset = MTYPE_bit_size(desc) - bit_offset - bit_size;
  }
  TN *tmp1_tn = Build_TN_Like(tgt_tn);

  Expand_Shift( tmp1_tn, src1_tn, Gen_Literal_TN(targ_bit_offset,4),
		rtype, shift_lright, ops );

  if( Is_Target_32bit()           &&
      MTYPE_is_size_double(rtype) &&
      Get_TN_Pair(src2_tn) == 0 ){
    TN* pair = Create_TN_Pair( src2_tn, rtype );
    Exp_Immediate( pair, Gen_Literal_TN(0,4), false, ops );
  }

  Expand_Binary_Xor( tmp1_tn, tmp1_tn, src2_tn, rtype, ops );

  int shift_amt = MTYPE_bit_size(rtype) - bit_size;
  Expand_Shift( tmp1_tn, tmp1_tn, Gen_Literal_TN(shift_amt, 4),
		rtype, shift_left, ops );

  shift_amt = MTYPE_bit_size(rtype) - bit_size - targ_bit_offset;
  Expand_Shift( tmp1_tn, tmp1_tn, Gen_Literal_TN(shift_amt, 4),
		rtype, shift_lright, ops );

  Expand_Binary_Xor( tgt_tn, src1_tn, tmp1_tn, rtype, ops );
}

void 
Expand_Lda_Label (TN *dest, TN *lab, OPS *ops)
{
  Exp_Immediate(dest, lab, FALSE, ops);
#if 0
  if (Use_32_Bit_Pointers)
    Build_OP(TOP_ld32, dest, rip, lab, ops);
  else
    Build_OP(TOP_ld64, dest, rip, lab, ops);
#endif
}
