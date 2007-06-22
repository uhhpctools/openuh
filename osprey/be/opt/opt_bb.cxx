//-*-c++-*-

/*
 * Copyright 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

// ====================================================================
// ====================================================================
//
// Module: opt_bb.cxx
// $Revision: 1.1.1.1 $
// $Date: 2005/10/21 19:00:00 $
// $Author: marcel $
// $Source: /proj/osprey/CVS/open64/osprey1.0/be/opt/opt_bb.cxx,v $
//
// Revision history:
//  8-SEP-94 shin - Original Version
//
// ====================================================================
//
// Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.
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
// Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
// Mountain View, CA 94043, or:
//
// http://www.sgi.com
//
// For further information regarding this notice, see:
//
// http://oss.sgi.com/projects/GenInfo/NoticeExplan
//
// ====================================================================
//
// Description:
//
//   Please reference the interface documentation in the opt_bb.h.
//
// ====================================================================
// ====================================================================


#ifdef USE_PCH
#include "opt_pch.h"
#endif // USE_PCH
#pragma hdrstop


#define opt_bb_CXX	"opt_bb.cxx"
#ifdef _KEEP_RCS_ID
static char *rcs_id = 	opt_bb_CXX"$Revision: 1.1.1.1 $";
#endif /* _KEEP_RCS_ID */

#include "defs.h"
#include "wn.h"
#include "errors.h"
#include "erglob.h"
#include "cxx_memory.h"
#include "region_util.h"
#include "fb_whirl.h"	// for FB_DEVWARN_LIMIT

#include "stblock.h"    // for Find_Slink_Symbol

#include "bb_node_set.h"

#include "opt_bb.h"
#include "opt_config.h"
#include "opt_htable.h"
#include "opt_util.h"
#include "opt_wn.h"
#include "opt_ssa.h"
#include "opt_base.h"
#include "bb_node_set.h"
#include "idx_32_set.h"

BB_LOOP*
BB_LOOP::Append (BB_LOOP *loop)
{
  BB_LOOP_CONTAINER bb_loop_container(this);
  bb_loop_container.Append(loop);
  return (BB_LOOP*)bb_loop_container.Head();
}

BB_LOOP*
BB_LOOP::Append_list (BB_LOOP *loop)
{
  BB_LOOP_CONTAINER bb_loop_container(this);
  BB_LOOP_CONTAINER tmp(loop);
  bb_loop_container.Append_List(&tmp);
  return (BB_LOOP*)bb_loop_container.Head();
}

BOOL
BB_LOOP::Contains(const BB_LOOP* l) const
{
  // return TRUE if 'this' is 'l' or its parent loop.
  for (const BB_LOOP *p = l; p != NULL; p = p->Parent())
    if (p == this) return TRUE;
  return FALSE;
}

BB_LIST*
BB_LIST::Append (BB_NODE *bb, MEM_POOL *pool)
{
  SLIST bb_list_container(this);
  BB_LIST *new_bblst = (BB_LIST*)CXX_NEW( BB_LIST(bb), pool );
  if ( new_bblst == NULL ) ErrMsg ( EC_No_Mem, "BB_LIST::Append" );
  bb_list_container.Append(new_bblst);
  return (BB_LIST*)bb_list_container.Head();
}

BB_LIST *
BB_LIST::Remove  (BB_NODE *bb, MEM_POOL *pool)
{
  Warn_todo("BB_LIST::Remove: remove this call");
  BB_LIST *prev, *cur, *retval = this;

  if (bb == NULL) return this;

  for (prev=NULL,cur=this; cur && cur->node != bb; cur = cur->Next())
    prev = cur;

  if ( cur == NULL )
    return this;

  if (cur == this)
    retval = Next();

  cur->SLIST_NODE::Remove(prev);
  CXX_DELETE(cur, pool);
  return retval;
}

BOOL
BB_LIST::Contains(BB_NODE *bb) const
{
  BB_LIST_ITER bb_list_iter(this);
  BB_NODE *tmp;
  FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
    if (tmp == bb)
      return TRUE;
  }
  return FALSE;
}

INT32
BB_LIST::Pos(BB_NODE *nd)
{
  BB_LIST *cur = this;
  INT32 pos = 0;
  while (cur) {
    if (nd == cur->node) {
      Is_True( cur->Next()->Pos(nd) == -1,
	       ( "BB_LIST::Pos(): Node BB%d must not "
		 "appear more than once", nd ) );
      return pos;
    }
    cur = cur->Next();
    pos++;
  }
  return -1;
}

void
BB_LIST_CONTAINER::Append (BB_NODE *bb, MEM_POOL *pool)
{
  BB_LIST *new_bblst = (BB_LIST*)CXX_NEW( BB_LIST(bb), pool );
  if ( new_bblst == NULL ) ErrMsg ( EC_No_Mem, "BB_LIST::Append" );
  Append(new_bblst);
}

void
BB_LIST_CONTAINER::Prepend(BB_NODE *bb, MEM_POOL *pool)
{
  BB_LIST *new_bblst = (BB_LIST*)CXX_NEW( BB_LIST(bb), pool );
  if ( new_bblst == NULL ) ErrMsg ( EC_No_Mem, "BB_LIST::Prepend" );
  Prepend(new_bblst);
}

void
BB_LIST_CONTAINER::Remove  (BB_NODE *bb, MEM_POOL *pool)
{
  Warn_todo("BB_LIST_CONTAINER::Remove: remove this call");
  BB_LIST *prev, *cur;

  if (bb == NULL) return;
  for (prev=NULL,cur=Head(); cur && cur->Node() != bb; cur = cur->Next())
    prev = cur;

  CXX_DELETE(cur->Remove(prev), pool);
}

BB_NODE *
BB_LIST_CONTAINER::Remove_head(MEM_POOL *pool)
{
  Warn_todo("BB_LIST_CONTAINER::Remove_head: remove this call");
  BB_NODE *bb;
  BB_LIST *head;

  head = Head();
  if (head == NULL)
    return NULL;
  bb = head->Node();
  CXX_DELETE(Remove_Headnode(), pool);
  return bb;
}

BOOL
BB_LIST_CONTAINER::Contains(BB_NODE *bb) const
{
  BB_LIST_ITER bb_list_iter(this);
  BB_NODE* tmp;
  FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
    if (tmp == bb)
      return TRUE;
  }
  return FALSE;
}

void
BB_LIST_ITER::Validate_unique(FILE *fp)
{
  for (First(); !Is_Empty(); Next()) {
    BB_NODE *tmp = Cur()->Node();
    if (tmp == NULL) {
      fprintf(fp, "Empty Node in the bb_list!!!\n");
      break;
    }
    if (Peek_Next()) {
      if (Peek_Next()->Contains(tmp)) {
	fprintf(fp, "The bb_list has redundant bb_node");
	this->Head()->Print(fp);
      }
    }
  }
}

// ====================================================================

static inline BOOL
Can_fallthru(OPCODE opc)
{
  if (OPCODE_is_non_scf(opc)) {
    if (opc == OPC_TRUEBR ||
	opc == OPC_FALSEBR ||
	opc == OPC_LABEL) {
      return TRUE;
    }
    else {
      return FALSE;
    }
  }

  // Any stmt or scf can fall through.
  return TRUE;
}

const BB_NODE *
BB_NODE::Falls_thru_to(void) const
{
  if (Firststmt() == NULL) {
    // Already transformed to STMTREP, or empty block.
    if (Last_stmtrep() != NULL) {
      if (!Can_fallthru(Last_stmtrep()->Op())) {
	return NULL;
      }
    }
  }
  else {
    if (!Can_fallthru(WN_opcode(Laststmt()))) {
      return NULL;
    }
  }
  // The last statement can fall through.
  return Next();
}

// ====================================================================
// Replace a pred/succ with another block to maintain ordering
// ====================================================================

void
BB_NODE::Replace_succ( BB_NODE *old_succ, BB_NODE *new_succ )
{
  for ( BB_LIST *succs = Succ(); succs != NULL; succs = succs->Next()) {
    if ( succs->Node() == old_succ ) {
      succs->Set_node(new_succ);
      break;
    }
  }
}

void
BB_NODE::Replace_pred( BB_NODE *old_pred, BB_NODE *new_pred )
{
  for ( BB_LIST *preds = Pred(); preds != NULL; preds = preds->Next()) {
    if ( preds->Node() == old_pred ) {
      preds->Set_node(new_pred);
      break;
    }
  }
}

BB_NODE *
BB_NODE::Nth_pred( INT32 n ) const
{
  INT counter = 0;
  for ( BB_LIST *preds = Pred(); preds != NULL; preds = preds->Next()) {
    if ( counter == n )
      return preds->Node();
    counter++;
  }

  FmtAssert( FALSE,
    ("BB_NODE::Nth_pred: BB:%d does not have a %dth pred",
     Id(), n) );

  return NULL;
}


BB_NODE *
BB_NODE::Nth_succ( INT32 n ) const
{
  INT counter = 0;
  for ( BB_LIST *succs = Succ(); succs != NULL; succs = succs->Next()) {
    if ( counter == n )
      return succs->Node();
    counter++;
  }

  FmtAssert( FALSE,
    ("BB_NODE::Nth_succ: BB:%d does not have a %dth succ",
     Id(), n) );

  return NULL;
}


// ====================================================================
// Remove_phi_reference - update all of this block's phi nodes to
// remove values that come from predecessor whichpred (0 is first pred)
// ====================================================================

void 
BB_NODE::Remove_phi_reference( INT32 whichpred )
{
  if (Phi_list() != NULL)
    Phi_list()->Remove_opnd( whichpred );
}

// ====================================================================
// Clear all of the fields
// ====================================================================

void
BB_NODE::Clear()
{
  _id = 0;
  Set_rpo_id((unsigned) -1);
  _loopdepth = 0;
  _rid_id = 0;
  _flags     = (BB_FLAG)0;
  _kind      = BB_UNKNOWN;
  _pred = NULL;
  _succ = NULL;
  _idom  = NULL;
  _ipdom = NULL;
  _dom_bbs = NULL;
  _pdom_bbs = NULL;
  _dom_dfs_id = 0;
  _dom_dfs_last = 0;
  _pdom_dfs_id = 0;
  _pdom_dfs_last = 0;
  _dom_frontier = NULL;
  _rcfg_dom_frontier = NULL;
  Set_labnam(0);
  Set_label_loop_info(NULL);
  _firststmt = NULL;
  _laststmt = NULL;
  _innermost = NULL;
  _phi_list = NULL;
  _iphi_list = NULL;
  _linenum = (SRCPOS)0;
  _loop = NULL;
  _hi._ifinfo = NULL;
  Set_exp_phi(NULL);
}

// ====================================================================
// Copy constructor
// Please note that we do not allocate new copies of the structures
// so they are shared (possibly incorrectly) between the old and new
// copy.
// ====================================================================

BB_NODE::BB_NODE(const BB_NODE& old)
{
  Clear();
  _id = old._id;
  _loopdepth = old._loopdepth;
  _rid_id = old._rid_id;
  _flags = old._flags;
  _kind = old._kind;
  _pred = old._pred;
  _succ = old._succ;
  _idom = old._idom;
  _ipdom = old._ipdom;
  _dom_bbs = old._dom_bbs;
  _pdom_bbs = old._pdom_bbs;
  _dom_dfs_id = old._dom_dfs_id;
  _dom_dfs_last = old._dom_dfs_last;
  _pdom_dfs_id = old._pdom_dfs_id;
  _pdom_dfs_last = old._pdom_dfs_last;
  _dom_frontier = old._dom_frontier;
  _rcfg_dom_frontier = old._rcfg_dom_frontier;
  _labels = old._labels;
  _firststmt = old._firststmt;
  _laststmt = old._laststmt;
  //NOT copied: _stmtlist = old._stmtlist;
  _innermost = old._innermost;
  _hi = old._hi;
  _phi_list = old._phi_list;
  _iphi_list = old._iphi_list;
  _linenum = old._linenum;
  _u0 = old._u0;
  _u1 = old._u1;
  _u2 = old._u2;
  _u3 = old._u3;
  _u4 = old._u4;
  _u5 = old._u5;
  _u6 = old._u6;
  _u7 = old._u7;
  _u8 = old._u8;
  _u9 = old._u9;
  _u10 = old._u10;
  _u11 = old._u11;
  _u12 = old._u12;
  _u13 = old._u13;
  Set_exp_phi(NULL);
}


const char *
BB_NODE::Kind_name( void ) const
{
  switch ( Kind() ) {
    case BB_UNKNOWN:
    case BB_GOTO:      // single target BB
    case BB_LOGIF:     // logical if
    case BB_VARGOTO:   // variable goto
    case BB_ENTRY:     // the entry bb
    case BB_EXIT:      // the exit bb
    case BB_DOSTART:   // init block
    case BB_DOEND:     // ending condition
    case BB_DOSTEP:    // increment
    case BB_DOHEAD:    // dohead block
    case BB_DOTAIL:    // dotail block
    case BB_IO:	       // io block
    case BB_WHILEEND:  // ending condition for while statement
    case BB_REGIONSTART://first bb in region
    case BB_REGIONEXIT://first bb in region
    case BB_REPEATBODY://first bb in do-while statement
    case BB_REPEATEND: // ending condition for repeat statement
    case BB_SUMMARY:   // summary BB
      return BB_kind_name[Kind()];
    default:
      Warn_todo( "BB_NODE::Kind_name: unknown kind" );
      return "BAD BB_KIND";
  }
}

void
BB_NODE::Validate (FILE *fp) const
{
  // validate assumptions in bb
  Print_head(fp);
  if (Kind() != BB_ENTRY && Kind() != BB_REGIONSTART && Pred() == NULL)
    fprintf(fp, "Validate BB%d: has no predecessor (%s)\n", Id(),Kind_name());
  BB_LIST_ITER pred_iter(Pred());
  pred_iter.Validate_unique(fp);
  if (Kind() != BB_EXIT && Kind() != BB_REGIONEXIT && Succ() == NULL)
    fprintf(fp, "Validate BB%d: has no successor (%s)\n", Id(),Kind_name());
  BB_LIST_ITER succ_iter(Succ());
  succ_iter.Validate_unique(fp);
}

WN *
BB_NODE::Get_do_start() const
{
  // for preopt, do loop access
  FmtAssert(Kind() == BB_DOSTART, ("BB_NODE::Get_do_start, must be BB_DOSTART"));

  return WN_start(Firststmt());
}
WN *
BB_NODE::Get_do_end() const
{
  // for preopt, do loop access
  FmtAssert(Kind() == BB_DOEND, ("BB_NODE::Get_do_end, must be BB_DOEND"));

  return WN_end(Firststmt());
}

WN *
BB_NODE::Get_do_step() const
{
  // for preopt, do loop access
  FmtAssert(Kind() == BB_DOSTEP, ("BB_NODE::Get_do_step, must be BB_DOSTEP"));

  return WN_step(Firststmt());
}

// Add a statement to the end of a block (same as above, but also
// builds the statement from the WHIRL node)
STMTREP *
BB_NODE::Add_stmtnode(WN *wn, MEM_POOL *pool)
{
  STMTREP *stmt = CXX_NEW(STMTREP(WN_opcode(wn)), pool);
  stmt->Set_wn(wn);
  // note: this code should be nearly identical to Append_stmtrep,
  // except for checks to see if it's above a branch if any in the
  // block.  This method should only be called for quickly converting
  // from WNs to STMTREP.
  if (_stmtlist.Head() == NULL)
    Set_linenum(WN_Get_Linenum(wn));
  else if (Linenum() == 0)
    Set_linenum(WN_Get_Linenum(wn));
  stmt->Set_bb(this);
  _stmtlist.Append(stmt);

  return stmt;
}

// Add a statement to the end of a block
// terminates the block
void
BB_NODE::Append_stmtrep(STMTREP *stmt)
{
  Is_True((Kind() != BB_REGIONSTART && Kind() != BB_ENTRY) ||
	  (Kind() == BB_REGIONSTART &&
	  (stmt->Op() == OPC_PRAGMA || stmt->Op() == OPC_XPRAGMA)),
	  ("BB_NODE::Append_stmtrep(), inserting into a %s",Kind_name()));
  Warn_todo(
   "BB_NODE::Append_stmtrep: Guarantee it is added before the branch, if any");
  stmt->Set_bb(this);
  _stmtlist.Append(stmt);
}

// Add a statement to the beginning of a block
// terminates the block
void
BB_NODE::Prepend_stmtrep(STMTREP *stmt)
{
  Is_True(Kind() != BB_REGIONSTART && Kind() != BB_ENTRY,
	  ("BB_NODE::Prepend_stmtrep(), inserting into a %s (bb:%d)",
	   Kind_name(), Id()));

  STMTREP_ITER stmtrep_iter(&_stmtlist);
  STMTREP *tmp, *last_label = NULL;
  FOR_ALL_NODE(tmp, stmtrep_iter, Init())
    if (OPCODE_operator(tmp->Op()) == OPR_LABEL)
      last_label = tmp;
    else
      break;
  stmt->Set_bb(this);
  if (last_label == NULL)
    _stmtlist.Prepend(stmt);
  else
    _stmtlist.Insert_After(stmt, last_label);
}


// Insert a STMTREP before another
void
BB_NODE::Insert_stmtrep_before(STMTREP *stmt, STMTREP *before_stmt)
{
  Is_True(Kind() != BB_REGIONSTART && Kind() != BB_ENTRY,
	  ("BB_NODE::Insert_stmtrep(), inserting into a %s",Kind_name()));

  STMTREP_ITER stmtrep_iter(&_stmtlist);
  stmt->Set_bb(this);
  if (before_stmt == NULL)
    _stmtlist.Prepend(stmt);
  else
    _stmtlist.Insert_Before(stmt, before_stmt);
}

// Insert a STMTREP after another
void
BB_NODE::Insert_stmtrep_after(STMTREP *stmt, STMTREP *after_stmt)
{
  STMTREP_ITER stmtrep_iter(&_stmtlist);
  stmt->Set_bb(this);
  if (after_stmt == NULL)
    _stmtlist.Append(stmt);
  else
    _stmtlist.Insert_After(stmt, after_stmt);
}


// remove the given statement from this block
void
BB_NODE::Remove_stmtrep( STMTREP *stmt )
{
  if ( stmt->Lhs() != NULL ) {
    // only decrement the kids' usecnt because the Lhs node itself
    // should end up with a usecnt of 0 when all of its uses have
    // been removed (or never existed)
    stmt->Lhs()->DecKidsUsecnt_rec();
  }

  if ( stmt->Rhs() != NULL ) {
    stmt->Rhs()->DecUsecnt_rec();
  }

  _stmtlist.Remove(stmt);
}

//====================================================================
// BB_NODE::Append_stmt_before_branch - Insert the stmtrep node as the last 
// statement in the BB. It needs to be inserted before any jump statement.  
//====================================================================
void
BB_NODE::Append_stmt_before_branch(STMTREP *stmt)
{
  STMTREP *last_stmt = Last_stmtrep();

  Is_True(Kind() != BB_REGIONSTART,
	  ("BB_NODE::Append_stmt_before_branch, illegal insertion into BB_REGIONSTART"));

  stmt->Set_linenum(last_stmt != NULL ? last_stmt->Linenum() : Linenum());
  if (last_stmt == NULL || 
      ! OPCODE_is_endsbb(last_stmt->Op()) ||
      OPCODE_is_call(last_stmt->Op())) {
    Stmtlist()->Append(stmt);
    stmt->Set_bb(this);
    }
  else {
    // The following lines used to be:
    // if (bb->Succ()->Next() != NULL || // pred has > 1 succ 
    //    last_stmt->Op() == OPC_GOTO) 
    // bb->Stmtlist()->Insert_Before(stmt, last_stmt);
    // else ...
    //
    // which is wrong because a single-successor BB can still have a
    // conditional branch at the end, i.e., a branch to the
    // fallthrough location that has not been eliminated or converted
    // to an unconditional branch.
    //
    // TODO: What kinds of control flow can we see besides
    // unconditional branches and TRUEBR/FALSEBR nodes?
    if (last_stmt->Op() == OPC_GOTO ||
	last_stmt->Op() == OPC_TRUEBR ||
	last_stmt->Op() == OPC_FALSEBR ||
	// Make sure we still catch all the cases caught by the old
	// incorrect code... (see comment above)
	Succ()->Next() != NULL) {
      Stmtlist()->Insert_Before(stmt, last_stmt);
    }
    else {
      Stmtlist()->Append(stmt);
    }
    stmt->Set_bb(this);
  }
}

// ====================================================================
// Branch_stmtrep - determine if this block has a branch statement that 
// terminates it.  Return it if there is one.
// ====================================================================

STMTREP *
BB_NODE::Branch_stmtrep(void)
{
  STMTREP *branchstmt = Last_stmtrep();
  if ( branchstmt != NULL ) {
    if ( OPCODE_is_endsbb(branchstmt->Op()) )
      return ( branchstmt );
    if ( branchstmt->Op() == OPC_IO && Kind() == BB_IO )
      return ( branchstmt );
    if ( branchstmt->Op() == OPC_REGION && Kind() == BB_GOTO ) // black box
      return ( branchstmt );
  }

  return ( NULL );
}

// ====================================================================
// Label_stmtrep - determine if this block has a LABEL statement
// that will get emitted (and is required if the block gets jumped
// to.)  Return it if there is one.
// ====================================================================

STMTREP *
BB_NODE::Label_stmtrep(void)
{
  STMTREP *labelstmt = First_stmtrep();
  if ( labelstmt != NULL && labelstmt->Op() == OPC_LABEL ) {
    return ( labelstmt );
  }

  return ( NULL );
}

// ====================================================================
// Remove_label_stmtrep --
//   remove all labels from this basic block.
// ====================================================================

void
BB_NODE::Remove_label_stmtrep()
{
  STMTREP *stmt;
  while ((stmt = Label_stmtrep()) != NULL)
    Remove_stmtrep(stmt);
}

// ====================================================================
// Add_label_stmtrep - add a LABEL statement to this block
// ====================================================================

void
BB_NODE::Add_label_stmtrep(MEM_POOL *pool)
{
  Is_True( Label_stmtrep() == NULL,
    ("BB_NODE::Add_label_stmtrep: BB:%d has label statement", Id()) );

  Is_True( Labnam() != 0, ("BB_NODE::Add_label_stmtrep: BB:%d has no label.", Id()));

  STMTREP *new_label = CXX_NEW( STMTREP(OPC_LABEL), pool );
  new_label->Set_live_stmt();
  new_label->Init_Label( NULL, Labnam(), Linenum() );
  new_label->Set_bb(this);
  Prepend_stmtrep(new_label);
}


void
BB_NODE::Add_label(CFG *cfg)
{
  Is_True( Labnam() == 0, ("BB_NODE::Add_label: BB:%d has a label.", Id()));
  cfg->Append_label_map( cfg->Alloc_label(), this );
}


// ====================================================================
// Branch_wn - determine if this block has a branch wn that 
// terminates it.  Return it if there is one.
// ====================================================================

WN *
BB_NODE::Branch_wn(void) const
{
  WN *branchwn = Laststmt();
  if ( branchwn != NULL ) {
    const OPCODE opc = WN_opcode(branchwn);
    if ( OPCODE_is_endsbb(opc) ) {
      return ( branchwn );
    }
  }

  return ( NULL );
}


// ====================================================================
// Label_wn - determine if this block has a LABEL statement
// that will get emitted (and is required if the block gets jumped
// to.)  Return it if there is one.
// ====================================================================

WN *
BB_NODE::Label_wn(void) const
{
  WN *labelwn = Firststmt();
  if ( labelwn != NULL && WN_opcode(labelwn) == OPC_LABEL ) {
    return ( labelwn );
  }

  return ( NULL );
}

// ====================================================================
// Insert wn after after_this (if after_this is null, inserts at the 
// beginning of the block)
// ====================================================================

void
BB_NODE::Insert_wn_after( WN *wn, WN *after_this )
{
  Is_True(Kind() != BB_REGIONSTART && Kind() != BB_ENTRY,
	  ("BB_NODE::Insert_wn_after(), inserting into a %s",Kind_name()));

  if (Firststmt() == NULL) {
    Is_True( after_this == NULL,
      ("BB_NODE::Insert_wn_after: empty block, non-null after_this") );
    
    Init_stmt(wn);
  }
  else {
    if ( after_this == NULL ) {
      WN_prev(Firststmt()) = wn;
      WN_prev(wn) = NULL;
      WN_next(wn) = Firststmt();
      Set_firststmt(wn);
    }
    else {
      WN_prev(wn) = after_this;
      if ( WN_next(after_this) != NULL ) {
	WN_next(wn) = WN_next(after_this);
	WN_prev(WN_next(wn)) = wn;
      }
      else {
	WN_next(wn) = NULL;
	Set_laststmt( wn );
      }
      WN_next(after_this) = wn;
    }
  }
}

// ====================================================================
// Insert wn before before_this (if before_this is null, inserts at the 
// end of the block)
// ====================================================================

void
BB_NODE::Insert_wn_before( WN *wn, WN *before_this )
{
  Is_True(Kind() != BB_REGIONSTART && Kind() != BB_ENTRY,
	  ("BB_NODE::Insert_wn_before(), inserting into a %s",Kind_name()));

  if (Firststmt() == NULL) {
    Is_True( before_this == NULL,
      ("BB_NODE::Insert_wn_before: empty block, non-null before_this"));

    Init_stmt(wn);
  }
  else if ( before_this == NULL ) {
    WN_next(Laststmt()) = wn;
    WN_prev(wn) = Laststmt();
    WN_next(wn) = NULL;
    Set_laststmt(wn);
  }
  else {
    WN_next(wn) = before_this;
    if ( WN_prev(before_this) != NULL ) {
      WN_prev(wn) = WN_prev(before_this);
      WN_next(WN_prev(wn)) = wn;
    }
    else {
      WN_prev(wn) = NULL;
      Set_firststmt( wn );
    }
    WN_prev(before_this) = wn;
  }
}

// ====================================================================
// Append the WN to the end of the block, but before any branch
// ====================================================================

void
BB_NODE::Append_wn_before_branch( WN *wn )
{
  Insert_wn_before( wn, Branch_wn() );
}

// ====================================================================
// Determine if the node is call-related (e.g., a store of the function
// result/dedicated register)
// ====================================================================

static BOOL
WN_is_call_related( const WN *wn )
{
  const OPERATOR opr = WN_operator(wn);

  // check for stores of dedicated registers
  if ( opr == OPR_STID || opr == OPR_ISTORE || opr == OPR_ISTOREX ||
       opr == OPR_ISTBITS )
  {
    WN *val = WN_kid0(wn);
    const OPERATOR val_opr = WN_operator(val);
    if ( val_opr == OPR_LDID ) {
      ST *val_st = WN_st(val);
      if ( ST_class(val_st) == CLASS_PREG && 
	   Preg_Is_Dedicated(WN_offset(val)) )
      {
	return TRUE;
      }
      else if (opr == OPR_STID) {
	Warn_todo("WN_is_call_related: Find_Slink_Symbol performance issue");
	if (WN_st(wn) == Find_Slink_Symbol(CURRENT_SYMTAB))
	  {
	    // treat stores to slink_sym specially, so we insert after them
	    return TRUE;
	  }
      }
    }
  }

  // must not be call-related
  return FALSE;
}

// ====================================================================
// Prepend the WN to the beginning of the block, but after any labels,
// pragmas, etc.
// ====================================================================

void
BB_NODE::Prepend_wn_after_labels( WN *wn )
{
  if (Firststmt() == NULL) {
    Init_stmt(wn);
  }
  else {
    WN *prev = NULL;
    WN *labwn;
    // find the last label/pragma/etc.
    for ( labwn = Firststmt(); labwn != NULL; labwn = WN_next(labwn) ) {
      const OPERATOR labopr = WN_operator(labwn);
      if ( labopr == OPR_LABEL || 
	   labopr == OPR_PRAGMA ||
	   labopr == OPR_COMMENT ||
	  ( Callrel() && WN_is_call_related(labwn) )) {
	prev = labwn;
      }
      else {
	break;
      }
    }

    Insert_wn_after( wn, prev );
  }
}

void
BB_NODE::Add_pragma(WN *wn, STMTREP *stmt, MEM_POOL *pool)
{
  STMTREP *sr = CXX_NEW(STMTREP(OPC_PRAGMA), pool);
  sr->Set_orig_wn(wn);
  if (stmt) {
    sr->Set_bb(this);
    Stmtlist()->Insert_Before(sr, stmt);
  }
  else
    Append_stmtrep(sr);
}

// ====================================================================
// Comcompute
void
BB_NODE::Compute_rcfg_itrdom_frontier( BB_NODE_SET *itrcd ) const
{
  Is_True(itrcd!=NULL,("BB_NODE::Compute_rcfg_itrdom_frontier: NULL pointer"));
  BB_NODE *cdbb;
  BB_NODE_SET_ITER rcfg_iter;
  FOR_ALL_ELEM( cdbb, rcfg_iter, Init( Rcfg_dom_frontier() ) ) {
    if ( ! itrcd->MemberP( cdbb ) ) {
      itrcd->Union1D( cdbb ); 
      cdbb->Compute_rcfg_itrdom_frontier( itrcd );
    }
  }
}


// The basic block is empty other then having a label
// and unconditional goto statements
BOOL
BB_NODE::Is_empty()
{
  if (First_stmtrep() == NULL) return TRUE;
  STMTREP *bb_branch = Branch_stmtrep();
  STMTREP *bb_label = Label_stmtrep();
  if (bb_branch == NULL) {
    return (bb_label == Last_stmtrep());
  } else {
    if (OPC_GOTO == bb_branch->Op()) {
      if (bb_label == NULL)
	return (bb_branch == First_stmtrep());
      else
	return (bb_label->Next() == bb_branch);
    } else
      return FALSE;
  }
}

BOOL
BB_NODE::Clonable(BOOL allow_loop_cloning, const BVECTOR *cr_vol_map)
{
  // Note that we ignore the volatile attributes of codereps if 
  // cr_vol_map==NULL.
  //
  switch (Kind()) {
  case BB_ENTRY:
  case BB_REGIONSTART:
  case BB_REGIONEXIT:
  case BB_EXIT:
  case BB_VARGOTO:
    return FALSE;

  case BB_DOSTART:
  case BB_DOEND:
  case BB_DOSTEP: 
  case BB_DOHEAD:
  case BB_DOTAIL: 
  case BB_WHILEEND:
  case BB_REPEATBODY: 
  case BB_REPEATEND:
    if (!allow_loop_cloning) return FALSE;
  }
  if (Loop() && Loop()->Header()==this)
    if (!allow_loop_cloning) return FALSE;

  // if any of the BB's predecessor is a DO_HEAD, cannot clone
  BB_LIST_ITER bb_pred_iter;
  BB_NODE *pred;
  FOR_ALL_ELEM( pred, bb_pred_iter, Init(Pred()) ) {
    if (!allow_loop_cloning) {
      if (pred->Kind() == BB_DOHEAD ||
	  pred->Kind() == BB_DOSTART)
	return FALSE;
    }
    if (pred->Kind() == BB_VARGOTO)
      return FALSE;
  }

  // if BB contains label whose address is stored, cannot clone
  if ( Labnam() > 0 && LABEL_addr_saved( Labnam() ) )
    return FALSE;

  // iterate through each statement in this bb
  STMTREP_ITER stmt_iter(Stmtlist());
  STMTREP *stmt;       
  FOR_ALL_NODE(stmt, stmt_iter, Init()) {
    OPERATOR opr = OPCODE_operator(stmt->Op());
    if (opr == OPR_PREFETCH) return FALSE;  // prefetch map contains back pointer
    if (opr == OPR_REGION) return FALSE;    // black box region -- very difficult to clone
    if (OPERATOR_is_volatile(opr)) return FALSE;
    if (cr_vol_map != NULL &&
	stmt->Contains_volatile_ref(*cr_vol_map)) return FALSE;
    // The followings are represented by volatile operator
    //
    // if (opr == OPR_FORWARD_BARRIER) return FALSE;   // why clone BARRIERs
    // if (opr == OPR_BACKWARD_BARRIER) return FALSE;   // why clone BARRIERs
    // 1. do not clone call for performance reason
    // 2. PARM node are not shared, need to special case cloning of CALL statements
    // if (OPCODE_is_call(stmt->Op())) return FALSE;  
  }

  return TRUE;
}


// ====================================================================
// Printing methods
// ====================================================================

void
BB_LOOP::Print(FILE *fp) const
{
  if (Well_formed()) {
    fprintf(fp, "LOOP header BB%d\n", Header()->Id());
    fprintf(fp, "Natural loop: preheader %d, loopback %d, ", 	
	    Preheader()->Id(),
	    Loopback()->Id());
    if (Tail()) 
      fprintf(fp, "tail %d\n", Tail()->Id());
    else
      fprintf(fp, " no tail\n");
    fprintf(fp, "%s", Test_at_entry() ? "test-at-entry, " : "not test-at-entry, ");
    fprintf(fp, "%s", Test_at_exit() ? "test-at-exit, " : "not test-at-exit, ");
    fprintf(fp, "%s", Exit_early() ? "exit-early\n" : "not exit-early\n");
    fprintf(fp, "depth=%d, max_depth=%d\n", Depth(), Max_depth());
  } else 
    fprintf(fp, "not a well-formed loop\n");
  if (End() != NULL) {
    fprintf(fp, "SCF: START %d, END %d, BODY %d, MERGE %d\n",
	    Start()->Id(), End()->Id(), Body()->Id(), Merge()->Id());
  }

  BOOL in_mainopt = Start() && Start()->Kind() == BB_DOHEAD;
  INT depth, i;

  switch (_flags & LOOP_TYPES) {
  case LOOP_DO:
    depth = Body()->Loopdepth();
    for (i = 0; i < depth; i++)  fprintf(fp, ".");
    fprintf(fp, "DO: body:%d depth:%d ", _body->Id(), depth);
    break;
  case LOOP_PRE_DO:
    depth = _end->Loopdepth();
    for ( i = 0; i < depth; i++)  fprintf(fp, ".");
    fprintf(fp, "DO: cond:%d depth:%d ", _end->Id(), depth);
    break;
  case LOOP_WHILE:
  case LOOP_PRE_WHILE:
    depth = _body->Loopdepth();
    for ( i = 0; i < depth; i++)  fprintf(fp, ".");
    fprintf(fp, "WHILE: body:%d depth:%d ", _body->Id(), depth);
    break;
  case LOOP_REPEAT:
  case LOOP_PRE_REPEAT:
    depth = _body->Loopdepth();
    for ( i = 0; i < depth; i++)  fprintf(fp, ".");
    fprintf(fp, "REPEAT: body:%d depth:%d ", _body->Id(), depth);
    break;
  }
  fprintf(fp, "loop body set:      ");
  if (Body_set())
    Body_set()->Print(fp);
  else
    fprintf(fp," null\n");
  fprintf(fp, "true loop body set: ");
  True_body_set()->Print(fp);
  fprintf(fp, "\n");
  fprintf(fp, "size estimate: %d\n", _size_estimate);
}


void
BB_LIST::Print(FILE *fp) const
{
  BB_LIST_ITER bb_list_iter(this);
  BB_NODE *tmp;
  FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
    if (tmp)
      fprintf(fp, "%d ",tmp->Id());
  }
}

void
BB_NODE::Print_head (FILE *fp) const
{
  // print the bb structure
  fprintf(fp, "---- BB%d (RPO %d)", Id(),Rpo_id());
  if (Labnam())
    fprintf(fp, " (Lab%d)", Labnam());
  if (Kind() == BB_REGIONSTART || Kind() == BB_REGIONEXIT) {
    if (Regioninfo()) {
      Is_True(Regioninfo()->Rid() != NULL, ("BB_NODE::Print_head, NULL Rid"));
      fprintf(fp, " (%s %d)", Kind_name(), RID_id(Regioninfo()->Rid()) );
    } else
      fprintf(fp, " (%s***)", Kind_name() );
  } else
    fprintf(fp, " (%s)", Kind_name() );
  fprintf(fp, " %s", (Willexit())?"(e)":"(ne)");
  fprintf(fp, " LINE %d", Srcpos_To_Line(Linenum()));
  if (Haspragma())
    fprintf(fp, " (pragmas)" );
  if (Hascall())
    fprintf(fp, " (call)" );
  if ( Loopdepth() > 0 )
    fprintf(fp, " (nest:%d)", Loopdepth() );
  fprintf(fp, " (rid_id:%d)", Rid_id() );
  fprintf(fp, " (flag:%x)", Flag() );

  fprintf(fp, "\n");

  fprintf(fp, "Preds:");
  Pred()->Print(fp);
  fprintf(fp, "\n");

  fprintf(fp, "Succs:");
  Succ()->Print(fp);
  fprintf(fp, "\n");

  if (Falls_thru_to() != NULL) {
    fprintf(fp, "Fallthrough: %d\n", Falls_thru_to()->Id());
  }

  if (Idom())
    fprintf(fp, "Idom  : BB%d\n", Idom()->Id() );
  if (Ipdom())
    fprintf(fp, "Ipdom : BB%d\n", Ipdom()->Id() );

  fprintf(fp, "dom_dfs_id(%u), dom_dfs_last(%u)\n",
	  Dom_dfs_id(), Dom_dfs_last());
  fprintf(fp, "pdom_dfs_id(%u), pdom_dfs_last(%u)\n",
	  Pdom_dfs_id(), Pdom_dfs_last());

  if (Dom_bbs())
  {
    if (WOPT_Enable_SSA_PRE) {
      Dom_bbs()->Print(fp);
      fprintf(fp, "\n");
    }
    else {
      fprintf(fp, "Dom  :");
      Dom_bbs()->Print(fp);
      fprintf(fp, "\n");
    }
  }
  if (Pdom_bbs())
  {
    fprintf(fp, "Pdom :");
    Pdom_bbs()->Print(fp);
    fprintf(fp, "\n");
  }
  if (Dom_frontier())
  {
    fprintf(fp, "Dom Frontier :");
    Dom_frontier()->Print(fp);
    fprintf(fp, "\n");
  }
  if (Rcfg_dom_frontier())
  {
    fprintf(fp, "RCFG Dom Frontier :");
    Rcfg_dom_frontier()->Print(fp);
    fprintf(fp, "\n");
  }
}

void
BB_NODE::Print_wn (FILE *fp) const
{
  STMT_ITER wn_iter(_firststmt, _laststmt);
  wn_iter.Print(fp);
}

void
BB_NODE::Print (FILE *fp) const
{
  Print_head(fp);
  // print the bb structure
  Print_wn(fp);
  if (Phi_list())
    Phi_list()->Print(fp);
  //if (Iphi_list())
  //Iphi_list()->Print(fp);
  _stmtlist.Print(fp);
}

void
BB_NODE::Print_ssa (FILE *fp) const
{
  // only print the ssa representation of the code
  Print_head(fp);
  // print the bb structure
  if (Phi_list())
    Phi_list()->Print(fp);
  _stmtlist.Print(fp);
}

INT32 BB_NODE::Code_size_est(void) const
{
  STMTREP_CONST_ITER stmt_iter(&_stmtlist);
  const STMTREP *stmt;
  INT32 size = 0;
  FOR_ALL_NODE(stmt, stmt_iter, Init()) {
    size++;
    if (OPERATOR_is_call(stmt->Opr()))
      size += 10;
    else if (stmt->Opr() == OPR_ISTORE) {
      if (stmt->Rhs()->Kind() == CK_OP && stmt->Rhs()->Opr() == OPR_SELECT)
        size += 19;
    }
  }
  return size;
}

