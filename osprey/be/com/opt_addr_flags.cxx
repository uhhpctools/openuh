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


#include "opt_alias_interface.h"  
#include "opt_points_to.h"        
#include "config_opt.h"           
#include "symtab_access.h"
#include "wn.h"
#include "be_symtab.h"

// Adjust address flags

static BOOL suppress_all_warnings;
#ifndef KEY
static
#endif
void Set_addr_saved_stmt(WN *wn, BOOL use_passed_not_saved);

// wn is an actual parameter.  Search for LDAs under the expr that 
// are not consumed by an ILOAD, and set their addr_saved flag.
// warn is TRUE iff we should issue a DevWarn for each ST whose addr_saved
// flag we set.
#ifndef KEY
static 
#endif
void
Set_addr_saved_expr(WN *wn, BOOL warn)
{
  OPCODE opc = WN_opcode(wn);
  Is_True(OPCODE_is_expression(opc),
	  ("Update_addr_saved: opcode must be expression"));

  if (OPCODE_is_load(opc))
    return;

  if (OPCODE_operator(opc) == OPR_LDA) {
    ST *st = WN_st(wn);
    if (ST_class(st) == CLASS_VAR &&
	!ST_addr_saved(st)) {
      Set_ST_addr_saved(st);
      if (warn && !suppress_all_warnings)
	DevWarn("Set_addr_saved_expr: addr_saved flag of ST (%s) should be set.", 
		ST_name(st));
    }
  }
  if (OPCODE_operator(opc) == OPR_COMMA) {
    	Set_addr_saved_stmt(WN_kid(wn,0), warn);
    	Set_addr_saved_expr(WN_kid(wn,1), warn);
	return;
  }
  if (OPCODE_operator(opc) == OPR_RCOMMA) {
    	Set_addr_saved_expr(WN_kid(wn,0), warn);
    	Set_addr_saved_stmt(WN_kid(wn,1), warn);
	return;
  }
#ifdef KEY // only LDAs from kid 0 of ARRAY and ARRSECTION are relevant
  if (OPCODE_operator(opc) == OPR_ARRAY || 
      OPCODE_operator(opc) == OPR_ARRSECTION)
    Set_addr_saved_expr(WN_kid0(wn), warn);
  else
#endif
  for (INT i = 0; i < WN_kid_count(wn); i++) 
    Set_addr_saved_expr(WN_kid(wn,i), warn);
}


#ifndef KEY
static
#endif
void 
Set_addr_saved_stmt(WN *wn, BOOL use_passed_not_saved)
{
  if (wn == NULL) return;	
  OPCODE opc = WN_opcode(wn);

  if (OPCODE_is_call(opc)
#ifdef KEY
      || OPCODE_operator(opc) == OPR_PURE_CALL_OP
#endif
      ) {
    for (INT32 i = 0; i < WN_kid_count(wn); i++) {
      WN *actual = WN_actual(wn,i);
      // Question: What justification could there be for the
      // following line? Answer: It is a dangerous but cheap hack to
      // avoid processing the function address kid of ICALL as if it
      // were a parameter, which would otherwise happen because
      // WN_actual is naively implemented as WN_kid, with no check
      // for ICALL or other kinds of calls. We count on alias
      // classification or some other relatively conservative phase
      // to assert that the parameters are all PARM nodes.

      // Answer2:  WN_actual() does not guarantee returning a PARM node.
      // In this analysis, we don't care about the function address 
      // because it will not affect setting of addr saved.
      // Consider a direct call to FUNC and indiret call to FUNC should
      // be equivalent, although there is an extra function addr kid to
      // the indirect call.

      if (WN_operator(actual) != OPR_PARM) continue;
      if (!use_passed_not_saved ||
	  !WN_Parm_Passed_Not_Saved(actual))
	Set_addr_saved_expr(WN_kid0(actual), FALSE);
    }
    return;
  }

  switch (OPCODE_operator(opc)) {
  case OPR_FORWARD_BARRIER:
  case OPR_BACKWARD_BARRIER:
  case OPR_ALLOCA:
  case OPR_DEALLOCA:
    return;
  }

  if (OPCODE_is_black_box(opc)) 
    return;
  
  if (opc == OPC_BLOCK) {
    for (WN *stmt = WN_first(wn); stmt != NULL; stmt = WN_next(stmt))  
      Set_addr_saved_stmt(stmt, use_passed_not_saved);
  } else {
    for (INT i = 0; i < WN_kid_count(wn); i++) {
      Set_addr_saved_stmt(WN_kid(wn,i), use_passed_not_saved);
    }
  }
}


// For debugging only!
#ifndef KEY
static
#endif
void 
Recompute_addr_saved_stmt(WN *wn)
{
  if (wn == NULL) return;	
  OPCODE opc = WN_opcode(wn);

  if (OPCODE_is_store(opc)) {
    // the RHS expr of any store is kid0
    // Any idea on how to assert?
    Set_addr_saved_expr(WN_kid0(wn), TRUE);
  }

  if (OPCODE_is_black_box(opc)) 
    return;
  
  if (opc == OPC_BLOCK) {
    for (WN *stmt = WN_first(wn); stmt != NULL; stmt = WN_next(stmt))  
      Recompute_addr_saved_stmt(stmt);
  } else {
    for (INT i = 0; i < WN_kid_count(wn); i++) {
      Recompute_addr_saved_stmt(WN_kid(wn,i));
    }
  }
}


#ifdef Is_True_On

static void Verify_addr_flags_stmt(WN *wn);

static void
Verify_addr_saved_expr(WN *wn)
{
  OPCODE opc = WN_opcode(wn);
  Is_True(OPCODE_is_expression(opc),
	  ("Update_addr_saved: opcode must be expression"));

  if (OPCODE_is_load(opc))
    return;

  if (OPCODE_operator(opc) == OPR_LDA) {
    ST *st = WN_st(wn);
    if (ST_class(st) == CLASS_VAR &&
	!ST_addr_saved(st)) {
      FmtAssert(TRUE, ("PU_adjust_addr_flags:  ST %s should be addr_saved.\n",
		       ST_name(st)));
    }
  }
  if (OPCODE_operator(opc) == OPR_COMMA) {
    	Verify_addr_flags_stmt(WN_kid(wn,0));
    	Verify_addr_saved_expr(WN_kid(wn,1));
	return;
  }
  if (OPCODE_operator(opc) == OPR_RCOMMA) {
    	Verify_addr_saved_expr(WN_kid(wn,0));
    	Verify_addr_flags_stmt(WN_kid(wn,1));
	return;
  }
  for (INT i = 0; i < WN_kid_count(wn); i++) 
    Verify_addr_saved_expr(WN_kid(wn,i));
}

static void 
Verify_addr_flags_stmt(WN *wn)
{
  if (wn == NULL) return;	
  OPCODE opc = WN_opcode(wn);

  if (OPCODE_is_store(opc)) {
    // the RHS expr of any store is kid0
    // Any idea on how to assert?
    Verify_addr_saved_expr(WN_kid0(wn));
  }

  switch (OPCODE_operator(opc)) {
  case OPR_FORWARD_BARRIER:
  case OPR_BACKWARD_BARRIER:
  case OPR_ALLOCA:
  case OPR_DEALLOCA:
    return;
  }

  if (OPCODE_is_black_box(opc)) 
    return;
  
  if (opc == OPC_BLOCK) {
    for (WN *stmt = WN_first(wn); stmt != NULL; stmt = WN_next(stmt))  
      Verify_addr_flags_stmt(stmt);
  } else {
    for (INT i = 0; i < WN_kid_count(wn); i++) {
      Verify_addr_flags_stmt(WN_kid(wn,i));
    }
  }
}
#endif


void
PU_adjust_addr_flags(ST* pu_st, WN *wn)
{
  suppress_all_warnings = FALSE;
#if 1 // Fix 10-26-2002: Enhancement to reset addr_saved flag before Mainopt
  Set_Error_Phase("PU_adjust_addr_flags");
#endif
          // PV 682222: the MP lowerer may introduce LDA's on privatized
	  // ST's which require setting their addr_saved flag before WOPT.
	  // So the MP lowerer sets the PU_needs_addr_flag_adjust bit.
  BOOL has_privatization_LDAs = BE_ST_pu_needs_addr_flag_adjust(pu_st);

  if (OPT_recompute_addr_flags || has_privatization_LDAs) {
    if (!OPT_recompute_addr_flags)
      suppress_all_warnings = TRUE; // LDAs from privatization are OK

#if 1 // Fix 10-26-2002: Enhancement to reset addr_saved flag before Mainopt 
    Clear_local_symtab_addr_flags(Scope_tab[CURRENT_SYMTAB]);
#endif
    Recompute_addr_saved_stmt(wn);
  }

  if (BE_ST_pu_needs_addr_flag_adjust(pu_st))
    Clear_BE_ST_pu_needs_addr_flag_adjust(pu_st);

#ifdef Is_True_On
  if (!PU_smart_addr_analysis(Pu_Table[ST_pu(pu_st)]))
    Verify_addr_flags_stmt(wn);
#endif

  // Adjust addr_saved from actual parameters for non-Fortran programs.
  if (!Is_FORTRAN()) {
    PU& pu = Pu_Table[ST_pu(pu_st)];
    Set_addr_saved_stmt(wn,
			CXX_Alias_Const || 
			(OPT_IPA_addr_analysis && PU_ipa_addr_analysis(pu)));
  }
}
