//-*-c++-*-

// ====================================================================
// ====================================================================
//
// Copyright (C) 2007, University of Delaware, Hewlett-Packard Company,
// All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it would be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// Further, this software is distributed without any warranty that it
// is free of the rightful claim of any third person regarding
// infringement  or the like.  Any license provided herein, whether
// implied or otherwise, applies only to this software file.  Patent
// licenses, if any, provided herein do not apply to combinations of
// this program with other software, or any other product whatsoever.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
//
// ====================================================================
#include "defs.h"			// INT32, INT64
#include "config.h"			// Alias_Pointer_Parms
#include "opt_defs.h"
#include "tracing.h"			// trace flags
#include "config_opt.h"

#include "erglob.h"
#include "opt_cfg.h"  // for EXITBB_ITER 
#include "opt_sym.h"
#include "opt_misc.h"

static void
Analyze_pu_noreturn_attr (OPT_STAB* stab, PU* pu, ST* pu_st) {
 
  EXITBB_ITER iter(stab->Cfg());
  FOR_ALL_ITEM (iter, Init()) {
    BB_NODE* exit_bb = iter.Cur_exit_bb();
    WN *wn = exit_bb->Laststmt();
    if (wn && 
        (WN_operator (wn) == OPR_RETURN ||
         WN_operator (wn) == OPR_RETURN_VAL)) {
      wn = WN_prev (wn);
    }
    if (!wn || WN_operator (wn) != OPR_CALL) return;

    PU& pu_ent = Pu_Table[ST_pu(WN_st(wn))];
    if (!PU_has_attr_noreturn (pu_ent)) {
      return;
    }
  }
 
  // Now, all exit blocks are ended with call having 
  // __attribute__((noreturn), this function itself 
  // satisify the "noreturn" semantic 
  Set_PU_has_attr_noreturn (*pu);
 
  fprintf (stderr, "%s satisfy noreturn semantic\n", ST_name(pu_st));
}

// Analyze_pu_attr() conducts following things
//
//   - reveal _attribute_ semantics
//   - points-to summary 
// 
void
Analyze_pu_attr (OPT_STAB* opt_stab, ST* pu_st) {

  WN* pu_tree = opt_stab->Pu ();
  if (WN_opcode(pu_tree)!=OPC_FUNC_ENTRY)  {
    // not applicable, give up. 
    return;
  }

  PU& pu_ent = Pu_Table[ST_pu(pu_st)];

  // analyze __attribute__((noreturn)
  if (WOPT_Enable_Noreturn_Attr_Opt && !PU_has_attr_noreturn (pu_ent)) {
    Analyze_pu_noreturn_attr (opt_stab, &pu_ent, pu_st);
  }

  if (WOPT_Enable_Pt_Summary) {
    SET_OPT_PHASE("Points-to Summary");
    opt_stab->Summarize_points_to ();
  }
}

