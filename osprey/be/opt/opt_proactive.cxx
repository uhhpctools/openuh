/*
 * Copyright (C) 2008-2010 Advanced Micro Devices, Inc.  All Rights Reserved.
 */
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

#ifndef opt_proactive_INCLUDED
#include "opt_proactive.h"
#endif
#ifndef opt_htable_INCLUDED
#include "opt_htable.h"
#endif
#ifndef opt_cfg_INCLUDED
#include "opt_cfg.h"
#endif
#ifndef wn_simp_INCLUDED
#include "wn_simp.h"
#endif
#ifndef config_opt_INCLUDED
#include "config_opt.h"
#endif

using std::insert_iterator;
using std::map;
using std::set;

// Reset/clear fields.
void 
SC_NODE::Clear()
{
  type = SC_NONE;
  _id = 0;
  _class_id = 0;
  _depth = 0;
  pool = NULL;
  u1.bb_rep = NULL;
  u1.bbs = NULL;
  parent = NULL;
  kids = NULL;
  _flag = 0;
}

// Unmask given value from this SC_NODE's flag.
// See SC_NODE_FLAG for values of bitmask.
void
SC_NODE::Remove_flag(int bitmask)
{
  if (Has_flag(bitmask))
    _flag -= bitmask;
}

// Append given sc as this SC_NODE's last kid.
void 
SC_NODE::Append_kid(SC_NODE *sc)
{
  FmtAssert(this->Type() != SC_BLOCK, ("Unexpect kid for SC_BLOCK"));

  if (kids == NULL)
    kids = (SC_LIST*)CXX_NEW(SC_LIST(sc), pool);
  else {
    FmtAssert(!kids->Contains(sc), ("Repeated kids"));
    kids = kids->Append(sc,pool);
  }
}

// Prepend given sc as this SC_NODE's first kid.
void
SC_NODE::Prepend_kid(SC_NODE *sc)
{
  FmtAssert(this->Type() != SC_BLOCK, ("Unexpect kid for SC_BLOCK"));

  if (kids == NULL)
    kids = (SC_LIST*)CXX_NEW(SC_LIST(sc), pool);
  else {
    FmtAssert(!kids->Contains(sc), ("Repeated kids"));
    kids = kids->Prepend(sc,pool);
  }
}

// Insert given node before this node.
void
SC_NODE::Insert_before(SC_NODE * sc)
{
  SC_NODE * sc_parent = this->Parent();
  SC_NODE * sc_prev = this->Prev_sibling();

  sc->Set_parent(sc_parent);

  if (sc_prev == NULL)
    sc_parent->Prepend_kid(sc);
  else {
    SC_LIST * sc_list = sc_parent->Kids();
    SC_LIST_ITER sc_list_iter;
    SC_NODE * sc_tmp;
    sc_parent->Set_kids(NULL);

    FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init(sc_list)) {
      sc_parent->Append_kid(sc_tmp);
      if (sc_tmp == sc_prev)
	sc_parent->Append_kid(sc);
    }

    while (sc_list) {
      sc_tmp = sc_list->Node();
      sc_list = sc_list->Remove(sc_tmp, pool);
    }
  }
}

// Insert given node after this node.
void
SC_NODE::Insert_after(SC_NODE * sc)
{
  SC_NODE * sc_parent = this->Parent();
  
  sc->Set_parent(sc_parent);

  SC_LIST * sc_list = sc_parent->Kids();
  SC_LIST_ITER sc_list_iter;
  SC_NODE * sc_tmp;
  sc_parent->Set_kids(NULL);

  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init(sc_list)) {
    sc_parent->Append_kid(sc_tmp);
    if (sc_tmp == this)
      sc_parent->Append_kid(sc);
  }

  while (sc_list) {
    sc_tmp = sc_list->Node();
    sc_list = sc_list->Remove(sc_tmp, pool);
  }
}

// Remove given SC_NODE from this SC_NODE's kids.
void SC_NODE::Remove_kid(SC_NODE *sc)
{
  if (kids != NULL)
    kids = kids->Remove(sc, pool);
}

// Obtain last kid of this SC_NODE.
SC_NODE *
SC_NODE::Last_kid()
{
  if (kids == NULL)
    return NULL;

  return kids->Last_elem();
}

// Unlink this SC_NODE from the SC tree.
void
SC_NODE::Unlink()
{
  parent->Remove_kid(this);
  this->Set_parent(NULL);
}

// Convert type to new_type 
void
SC_NODE::Convert(SC_TYPE new_type)
{
  SC_TYPE old_type = type;

  if (old_type == new_type)
    return;

  if (SC_type_has_bbs(old_type) && SC_type_has_rep(new_type)) {
    BB_LIST * bb_list = Get_bbs();
    FmtAssert(((bb_list != NULL) && !bb_list->Multiple_bbs()), 
	      ("Expect a single block"));
    BB_NODE * bb = bb_list->Node();
    bb_list->Remove(bb, pool);
    Set_bbs(NULL);
    type = new_type;
    Set_bb_rep(bb);
  }
  else if (SC_type_has_rep(old_type) && SC_type_has_bbs(new_type)) {
    BB_NODE * bb = Get_bb_rep();
    Set_bb_rep(NULL);
    type = new_type;
    Append_bbs(bb);
  }
  else
    FmtAssert(FALSE, ("TODO"));
}

// Obtain first kid of this SC_NODE.
SC_NODE *
SC_NODE::First_kid()
{
  if (kids == NULL)
    return NULL;
  
  return kids->First_elem();

}

// Return next sibling SC_NODE from the same parent.

SC_NODE *
SC_NODE::Next_sibling()
{
  if (parent == NULL)
    return NULL;
  
  SC_LIST_ITER sc_list_iter(parent->Kids());
  SC_NODE * tmp = NULL;
  BOOL found = FALSE;

  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    if (found)
      return tmp;
    else if (tmp == this) {
      found = TRUE;
    }
  }

  return NULL;
}

// Return next SC_NODE in the SC tree that immediately succeeds this SC_NODE in source order.
SC_NODE *
SC_NODE::Next_in_tree()
{
  SC_NODE * cur = this;

  while (cur) {
    if (cur->Next_sibling())
      return cur->Next_sibling();
    cur = cur->Parent();
  }

  return NULL;
}

// Get this node's outermost nesting SC_IF that is bounded by sc_bound
SC_NODE *
SC_NODE::Get_nesting_if(SC_NODE * sc_bound)
{
  SC_NODE * sc_tmp = this->Parent();
  SC_NODE * ret_val = NULL;

  if (sc_bound->Is_pred_in_tree(this)) {
    while (sc_tmp && (sc_tmp != sc_bound)) {
      if (sc_tmp->Type() == SC_IF)
	ret_val = sc_tmp;
      sc_tmp = sc_tmp->Parent();
    }
  }

  return ret_val;
}

// Return closest next sibling SC_NODE of the given type
SC_NODE *
SC_NODE::Next_sibling_of_type(SC_TYPE match_type)
{
  if (parent == NULL)
    return NULL;
  
  SC_LIST_ITER sc_list_iter(parent->Kids());
  SC_NODE * tmp = NULL;
  BOOL found = FALSE;

  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    if (found) {
      if (tmp->Type() == match_type)
	return tmp;
    }
    else if (tmp == this) {
      found = TRUE;
    }
  }

  return NULL;
}

// Find the first kid that matches the given type.
SC_NODE  *
SC_NODE::First_kid_of_type(SC_TYPE match_type)
{
  SC_LIST_ITER kids_iter;
  SC_NODE * tmp;

  FOR_ALL_ELEM(tmp, kids_iter, Init(kids)) {
    if (tmp->Type() == match_type)
      return tmp;
  }

  return NULL;
}

// Return previous sibling of this SC_NODE.
SC_NODE *
SC_NODE::Prev_sibling()
{
  if (parent == NULL)
    return NULL;

  SC_LIST_ITER sc_list_iter(parent->Kids());
  SC_NODE * tmp = NULL;
  SC_NODE * prev = NULL;

  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    if (tmp == this)
      return prev;

    prev = tmp;
  }
  
  return NULL;
}

// Find the first kid that matches the given type
SC_NODE * 
SC_NODE::Find_kid_of_type(SC_TYPE kid_type)
{
  SC_LIST_ITER sc_list_iter(kids);
  SC_NODE * tmp;

  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    if (tmp->Type() == kid_type)
      return tmp;
  }
  
  return NULL;
}

// Obtain the first node on a then-path.
BB_NODE *
SC_NODE::Then()
{
  FmtAssert((this->Type() == SC_IF), ("Expect a SC_IF"));
  BB_NODE * head = this->Get_bb_rep();
  BB_IFINFO * ifinfo = head->Ifinfo();
  return ifinfo->Then();
}

// Obtain the first node on a else-path.
BB_NODE *
SC_NODE::Else()
{
  FmtAssert((this->Type() == SC_IF), ("Expect a SC_IF"));
  BB_NODE * head = this->Get_bb_rep();
  BB_IFINFO * ifinfo = head->Ifinfo();
  return ifinfo->Else();
}

// Obtain the first BB_NODE in source order for the SC tree rooted at this SC_NODE.
BB_NODE *
SC_NODE::First_bb()
{
  BB_NODE * bb_tmp = Get_bb_rep();

  if (bb_tmp)
    return bb_tmp;

  BB_LIST * bb_list = Get_bbs();
  if (bb_list)
    return bb_list->Node();

  SC_LIST_ITER sc_list_iter(kids);
  SC_NODE * sc_tmp;

  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init()) {
    bb_tmp = sc_tmp->First_bb();
    if (bb_tmp)
      return bb_tmp;
  }
  
  return NULL;
}

// Return the first real statement in this SC_NODE.
WN *
SC_NODE::First_executable_stmt()
{
  BB_NODE * bb_tmp = Get_bb_rep();
  WN * wn = NULL;

  if (bb_tmp) {
    wn = bb_tmp->First_executable_stmt();
    if (wn)
      return wn;
  }

  BB_LIST * bb_list = Get_bbs();
  if (bb_list) {
    BB_LIST_ITER bb_list_iter(bb_list);

    FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init()) {
      wn = bb_tmp->First_executable_stmt();
      if (wn)
	return wn;
    }
  }

  SC_LIST_ITER sc_list_iter(kids);
  SC_NODE * sc_tmp;

  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init()) {
    wn = sc_tmp->First_executable_stmt();
    if (wn)
      return wn;
  }
  
  return NULL;
}

// Walk upward in the ancestor sub-tree of this node and look for real nodes that
// are not boundary delimiters.
SC_NODE *
SC_NODE::Get_real_parent()
{
  SC_NODE * ret_val = NULL;

  if (parent) {
    SC_NODE * c_node = parent;
    SC_TYPE c_type = c_node->Type();

    while (ret_val == NULL) {
      switch (c_type) {
      case SC_THEN:
      case SC_ELSE:
      case SC_LP_START:
      case SC_LP_COND:
      case SC_LP_STEP:
      case SC_LP_BACKEDGE:
      case SC_LP_BODY:
	c_node = c_node->Parent();
	c_type = c_node->Type();
	break;
      default:
	ret_val = c_node;
      }
    }
  }

  return ret_val;
}

// Obtain the last BB_NODE in source order for the SC tree rooted at this SC_NODE.
BB_NODE *
SC_NODE::Last_bb()
{
  SC_NODE * last_kid = Last_kid();
  BB_NODE * last_bb = NULL;
  
  while (last_kid) {
    last_bb = last_kid->Last_bb();

    if (last_bb)
      return last_bb;

    last_kid = last_kid->Prev_sibling();
  }

  last_bb = Get_bb_rep();

  if (last_bb)
    return last_bb;

  BB_LIST * bb_list = Get_bbs();
  BB_LIST_ITER bb_list_iter(bb_list);
  BB_NODE * tmp;

  FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
    last_bb = tmp;
  }

  return last_bb;
}

// If this SC_NODE is a SC_LOOP, obtain loop info.
BB_LOOP * 
SC_NODE::Loopinfo()
{
  FmtAssert((type == SC_LOOP), ("Expect a SC_LOOP"));
  
  SC_LIST_ITER sc_list_iter;
  SC_NODE * tmp;
  SC_NODE * sc_cond = NULL;

  FOR_ALL_ELEM(tmp, sc_list_iter, Init(kids)) {
    if (tmp->Type() == SC_LP_COND) {
      sc_cond = tmp;
      break;
    }
  }

  FmtAssert(sc_cond, ("Loop cond not found"));
  BB_NODE * bb_cond = NULL;

  FOR_ALL_ELEM(tmp, sc_list_iter, Init(sc_cond->Kids())) {
    if (tmp->Type() == SC_BLOCK) {
      bb_cond = tmp->Get_bbs()->Node();
      break;
    }
  }

  FmtAssert(bb_cond, ("BB cond not found"));
  BB_LOOP * loopinfo = bb_cond->Loop();
  FmtAssert(loopinfo, ("Loop info not found"));
  return loopinfo;
}

// Obtain the merge block of a if-region or a loop-region.
BB_NODE *
SC_NODE::Merge()
{
  if (type == SC_IF) {
    BB_NODE * head = this->Get_bb_rep();
    BB_IFINFO * ifinfo = head->Ifinfo();
    return ifinfo->Merge();
  }
  else if (type == SC_LOOP) {
    BB_LOOP * loopinfo = Loopinfo();
    return loopinfo->Merge();
  }
  else
    return NULL;
}

// Set merge for this SC_NODE.
void
SC_NODE::Set_merge(BB_NODE * bb)
{
  if (type == SC_IF) 
    this->Head()->Ifinfo()->Set_merge(bb);
  else if (type == SC_LOOP)
    this->Loopinfo()->Set_merge(bb);
}

// Find an exit of this SC_NODE
BB_NODE *
SC_NODE::Exit()
{
  BB_NODE * exit = NULL;
  BB_NODE * merge = NULL;
  BB_NODE * tmp;
  BB_LIST_ITER bb_list_iter;

  switch (type) {
  case SC_LOOP:
    merge = Merge();
    FOR_ALL_ELEM(tmp, bb_list_iter, Init(merge->Pred())) {
      if (Contains(tmp)) {
	exit = tmp;
	break;
      }
    }
    break;
  default:
    FmtAssert(FALSE, ("TODO: find exit"));
  }

  return exit;
}

// Get loop index if this node is a SC_LOOP.
WN *
SC_NODE::Index()
{
  if (type == SC_LOOP) {
    BB_LOOP * loop_info = Loopinfo();
    return loop_info->Index();
  }

  return NULL;
}

// Obtain the head block of a if-region or a loop-region.
BB_NODE *
SC_NODE::Head()
{
  if (type == SC_IF) {
    return Get_bb_rep();
  }
  else if (type == SC_LOOP) {
    BB_NODE * bb = First_bb();
    FmtAssert(bb, ("First BB not found"));
    return bb;
  }
  else {
    FmtAssert(FALSE, ("Expect a SC_IF or a SC_LOOP"));
  }
  return NULL;
}

// Query whether the SC tree rooted at this SC_NODE contains bb.

BOOL
SC_NODE::Contains(BB_NODE * bb)
{
  BB_NODE * tmp = this->Get_bb_rep();
  if ((tmp != NULL) && (tmp == bb))
    return TRUE;

  BB_LIST * bb_list = this->Get_bbs();

  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      if (tmp == bb)
	return TRUE;
    }
  }

  SC_LIST * kids = this->Kids();

  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE *sc_tmp;
    FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init()) {
      if (sc_tmp->Contains(bb))
	return TRUE;
    }
  }

  return FALSE;
}

// Obtain the last block of a then-path.
BB_NODE *
SC_NODE::Then_end()
{
  FmtAssert((this->Type() == SC_IF), ("Expect a SC_IF"));
  SC_NODE * sc_then = Find_kid_of_type(SC_THEN);

  BB_NODE * merge = this->Merge();
  BB_LIST_ITER bb_list_iter(merge->Pred());
  BB_NODE * tmp;

  FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
    if (sc_then->Contains(tmp))
      return tmp;
  }
  return NULL;
}

// Obtain the last block of a else-path.
BB_NODE *
SC_NODE::Else_end()
{
  FmtAssert((this->Type() == SC_IF), ("Expect a SC_IF"));
  SC_NODE * sc_else = Find_kid_of_type(SC_ELSE);

  BB_NODE * merge = this->Merge();
  BB_LIST_ITER bb_list_iter(merge->Pred());
  BB_NODE * tmp;

  FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
    if (sc_else->Contains(tmp))
      return tmp;
  }
  return NULL;
}

// Delete SC tree rooted at this SC_NODE.
void
SC_NODE::Delete()
{
  SC_LIST_ITER sc_list_iter(kids);
  SC_NODE * tmp;

  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    tmp->Delete();
  }

  if (SC_type_has_bbs(type)) {
    BB_LIST * bbs = Get_bbs();
    while (bbs) {
      BB_NODE * bb = bbs->Node();
      bbs = bbs->Remove(bb, pool);
    }
  }

  SC_LIST * cur;
  SC_LIST * next;

  for (cur = kids; cur; cur = next) {
    next = cur->Next();
    CXX_DELETE(cur, pool);
  }
  
  CXX_DELETE(this, pool);
}

// Query whether this SC_NODE is well-behaved.
// A well-behaved if-region has a head block, a then-path, a else-path
// and a merge block. 
// The Next() of the head block is the first block on the then-path.
// The Next() of the last block on the then-path is the first block on the else-path.
// The Next() of the last block on the else-path is the merge block.
// The last block on the then-path is a predecessor of the merge block.
// The last block on the else-path is a predecessor of the merge block.

BOOL
SC_NODE::Is_well_behaved()
{
  BB_NODE * bb_head = Get_bb_rep();
  BB_NODE * bb_then = Then();
  BB_NODE * bb_then_end = Then_end();
  BB_NODE * bb_else = Else();
  BB_NODE * bb_else_end = Else_end();
  BB_NODE * bb_merge = Merge();

  if (!bb_head || !bb_then || !bb_then_end 
      || !bb_else || !bb_else_end || !bb_merge)
    return FALSE;

  if (bb_head->Next() != bb_then)
    return FALSE;
  
  if (bb_then_end->Next() != bb_else)
    return FALSE;

  if (bb_else_end->Next() != bb_merge)
    return FALSE;

  if (!bb_then_end->Succ()
      || !bb_then_end->Succ()->Contains(bb_merge))
    return FALSE;

  if (!bb_else_end->Succ() 
      || !bb_else_end->Succ()->Contains(bb_merge))
    return FALSE;

  return TRUE;
}

// Query whether given BB_NODE is a member of the SC tree rooted at this SC_NODE.
BOOL
SC_NODE::Is_member(BB_NODE * bb)
{
  if (bb == this->Get_bb_rep())
    return TRUE;

  BB_LIST_ITER bb_list_iter(Get_bbs());
  BB_NODE * bb_tmp;

  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init()) {
    if (bb_tmp == bb)
      return TRUE;
  }
  
  SC_LIST_ITER sc_list_iter(kids);
  SC_NODE * sc_tmp;
  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init()) {
    if (sc_tmp->Is_member(bb))
      return TRUE;
  }

  return FALSE;
}

// Query whether this SC_NODE has a single-entry and a single-exit.
// Return the single-entry and single-exit in the given parameters.
BOOL
SC_NODE::Is_sese()
{
  BOOL ret_val = FALSE;
  BB_NODE * bb_head;
  BB_NODE * bb_merge;
  BB_LIST_ITER bb_list_iter;
  BB_NODE * bb_tmp;
  BB_NODE * bb_first;

  switch (type) {
  case SC_BLOCK:
    ret_val = TRUE;
    bb_first = First_bb();

    FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(this->Get_bbs())) {
      if ((bb_tmp != bb_first)
	  && (!bb_first->Dominates(bb_tmp) || !bb_tmp->Postdominates(bb_first)
	      || !bb_tmp->Pred()
	      || (bb_tmp->Pred()->Len() != 1))) {
	ret_val = FALSE;
	break;
      }
    }

    break;

  case SC_IF:
    if (Is_well_behaved()) {
      bb_head = Head();
      bb_merge = Merge();

      if (bb_head->Is_dom(this)
	  && bb_merge->Is_postdom(this))
	ret_val= TRUE;
    }
    break;

  case SC_LOOP:
    bb_head = Head();
    bb_merge = Merge();
    
    if (bb_head->Is_dom(this)
	&& bb_merge->Is_postdom(this)) {
      BB_LIST * pred = bb_merge->Pred();

      if (pred->Len() == 1) {
	BB_NODE * tmp = pred->Node();
	if (this->Is_member(tmp))
	  ret_val = TRUE;
      }
    }

    break;

  case SC_LP_BODY:
    bb_first = this->First_bb();
    bb_tmp = this->Last_bb();

    if (bb_first->Is_dom(this)
	&& bb_tmp->Is_postdom(this))
      ret_val = TRUE;

    break;
    
  default:
    ;
  }

  return ret_val;
}

// Query whether this SC_NODE is a predessor of sc in the SC tree.

BOOL
SC_NODE::Is_pred_in_tree(SC_NODE * sc)
{
  SC_NODE * p_sc = sc->Parent();

  while (p_sc) {
    if (this == p_sc)
      return TRUE;
    p_sc = p_sc->Parent();
  }

  return FALSE;
}

// Find least common predecessor of this SC_NODE and sc in the SC tree.

SC_NODE *
SC_NODE::Find_lcp(SC_NODE * sc)
{
  if ((this->Parent() == NULL)
      || (sc->Parent() == NULL))
    return NULL;

  if (Is_pred_in_tree(sc))
    return this;
  else if (sc->Is_pred_in_tree(this))
    return sc;
  else {
    SC_NODE * p_sc = parent;
    
    while (p_sc) {
      if (p_sc->Is_pred_in_tree(sc))
	return p_sc;
      p_sc = p_sc->Parent();
    }
  }

  FmtAssert(FALSE, ("LCP not found"));
  return NULL;
}

// For every pair of WHILR nodes in the WHIRL tree rooted
// at wn1 and wn2, check whether operators are identical.
// If the node is a constant, check whether constant value
// is identical.

static BOOL
Has_same_shape(WN * wn1, WN * wn2)
{
  if (WN_operator(wn1) != WN_operator(wn2))
    return FALSE;

  switch (WN_operator(wn1)) {
  case OPR_INTCONST:
    if (WN_const_val(wn1) != WN_const_val(wn2))
      return FALSE;

    break;

  default:
    ;
  }

  if (WN_kid_count(wn1) != WN_kid_count(wn2))
    return FALSE;

  for (int i = 0; i < WN_kid_count(wn1); i++) {
    if (!Has_same_shape(WN_kid(wn1,i), WN_kid(wn2,i)))
      return FALSE;
  }

  return TRUE;
}

// Query whether this SC_NODE has the same loop structure as sc
BOOL
SC_NODE::Has_same_loop_struct(SC_NODE * sc)
{
  if ((type != SC_LOOP) || (type != sc->Type()))
    return FALSE;

  if (kids->Len() != sc->Kids()->Len())
    return FALSE;

  SC_NODE * sc1 = First_kid();
  SC_NODE * sc2 = sc->First_kid();
  BB_NODE * bb1;
  BB_NODE * bb2;
  WN * wn1;
  WN * wn2;
  
  while (sc1) {
    if (sc1->Type() != sc2->Type())
      return FALSE;

    bb1 = sc1->First_bb();
    bb2 = sc2->First_bb();

    switch (sc1->Type()) {
    case SC_LP_START:
    case SC_LP_COND:
    case SC_LP_STEP:

      wn1 = bb1->Laststmt();
      wn2 = bb2->Laststmt();
      
      if (WN_operator(wn1) == OPR_GOTO)
	wn1 = WN_prev(wn1);

      if (WN_operator(wn2) == OPR_GOTO)
	wn2 = WN_prev(wn2);

      if (!wn1 || !wn2 || !Has_same_shape(wn1, wn2))
	return FALSE;
      
    default:
      ;
    }

    sc1 = sc1->Next_sibling();
    sc2 = sc2->Next_sibling();
  }

  return TRUE;
}

// Query this SC_NODE and the sc have symmetric path.
// Find LCP, for every pair of noded on the path from LCP to this SC_NODE, and 
// on the path from LCP to the sc, the following condition must be satisfied:
// - Same type, and the type must be {SC_IF, SC_LOOP, SC_THEN, SC_ELSE}.
// - If the type is a SC_IF, condition expression should have the same shape.
// - If the type is a SC_LOOP, loop structure should be the same
// - Two pathes have the same length.
//
// If "check_buddy" is TRUE, 
// - allow type mismatch at lcp's immediate children under the condition that the lcp is a SC_IF.
// - disallow SC_LOOP on the path.
BOOL
SC_NODE::Has_symmetric_path(SC_NODE * sc, BOOL check_buddy)
{
  SC_NODE * sc1 = this;
  SC_NODE * sc2 = sc;
  SC_NODE * lcp = Find_lcp(sc);

  if (!lcp)
    return FALSE;

  while (sc1 && sc2) {
    if ((sc1 == lcp) && (sc2 != lcp))
      return FALSE;
    else if ((sc1 != lcp) && (sc2 == lcp))
      return FALSE;
    else if ((sc1 == lcp) && (sc2 == lcp))
      return TRUE;
    else {
      SC_TYPE type1 = sc1->Type();
      SC_TYPE type2 = sc2->Type();

      if (type1 != type2) {
	if (!check_buddy || (sc1->Parent() != lcp)
	    || (lcp->Type() != SC_IF))
	  return FALSE;
      }
      else if (check_buddy && (sc != sc2)
	       && (type1 == SC_LOOP))
	return FALSE;
      
      if ((type1 != SC_IF) && (type1 != SC_LOOP)
	  && (type1 != SC_THEN) && (type1 != SC_ELSE))
	return FALSE;
      
      if (type1 == SC_IF) {
	BB_NODE * bb1 = sc1->Get_bb_rep();
	BB_NODE * bb2 = sc2->Get_bb_rep();

	if (!bb1->Compare_Trees(bb2))
	  return FALSE;
      }

      if ((type1 == SC_LOOP)
	  && !sc1->Has_same_loop_struct(sc2))
	return FALSE;
    }

    sc1 = sc1->Parent();
    sc2 = sc2->Parent();
  }

  return FALSE;
}

// Count number of loops on the path from this SC_NODE to given sc_root.
// this_is_exc indicates whether to exclude this SC_NODE.
// root_is_exc indicates whether to exclude sc_root.
int 
SC_NODE::Num_of_loops(SC_NODE * sc_root, BOOL this_is_exc, BOOL root_is_exc)
{
  FmtAssert(sc_root->Is_pred_in_tree(this), ("Expect a pred in the SC tree"));
  int count = 0;
  SC_NODE * sc_node;

  if (this_is_exc)
    sc_node = this->Parent();
  else
    sc_node = this;

  while (sc_node) {
    if (sc_node == sc_root) {
      if (root_is_exc)
	break;
    }
    
    if (sc_node->Type() == SC_LOOP)
      count++;

    if (sc_node == sc_root)
      break;

    sc_node = sc_node->Parent();
  }

  return count;
}

// Count number of statements for all BB_NODEs in the SC tree rooted at this SC_NODE.
int
SC_NODE::Executable_stmt_count()
{
  int count = 0;
  BB_NODE * bb = Get_bb_rep();
  
  if (bb)
    count += bb->Executable_stmt_count();

  BB_LIST * bb_list = Get_bbs();
  BB_LIST_ITER bb_list_iter(bb_list);
  BB_NODE * bb_tmp;

  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init()) {
    count += (bb_tmp->Executable_stmt_count());
  }

  SC_LIST_ITER sc_list_iter(kids);
  SC_NODE * sc_tmp;

  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init()) {
    count += (sc_tmp->Executable_stmt_count());
  }

  return count;
}

// Query whether there exists a SC_LOOP in the SC-tree rooted at this SC_NODE.
BOOL
SC_NODE::Has_loop()
{
  if (type == SC_LOOP)
    return TRUE;
  
  SC_LIST_ITER sc_list_iter(kids);
  SC_NODE * sc_tmp;
  
  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init()) {
    if ((sc_tmp->Type() == SC_LOOP)
	|| sc_tmp->Has_loop())
      return TRUE;
  }

  return FALSE;
}

// Query whether this SC_NODE contains empty blocks.
BOOL
SC_NODE::Is_empty_block()
{
  if ((type != SC_BLOCK) || (Executable_stmt_count() > 0))
    return FALSE;

  BB_NODE * tmp;
  BB_LIST_ITER bb_list_iter(Get_bbs());

  FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
    if (tmp->Kind() != BB_GOTO)
      return FALSE;
  }

  return TRUE;
}

// Dump a SC_NODE.  If dump_tree is TRUE, dump the SC tree
// rooetd at this SC_NODE.
void 
SC_NODE::Print(FILE *fp, BOOL dump_tree) const
{
  fprintf(fp, "\n--- SC:%d %s ---\n", _id, this->Type_name());

  if (SC_type_has_rep(type)) {
    BB_NODE * bb = Get_bb_rep();
    if (bb) 
      fprintf(fp, " rep BB:%d", bb->Id());
  }
  else if (SC_type_has_bbs(type)) {
    BB_LIST  * bbs = Get_bbs();
    if (bbs) {
      fprintf(fp, " component BBs:");
      bbs->Print(fp);
    }
  }

  if (parent)
    fprintf(fp, " parent:%d", parent->Id());

  if (kids) {
    fprintf(fp, " kids:");
    kids->Print(fp);
    
    if (dump_tree) {
      SC_LIST_ITER sc_list_iter(kids);
      SC_NODE *tmp = NULL;

      FOR_ALL_ELEM(tmp, sc_list_iter, Init()) 
	tmp->Print(fp, TRUE);
    }
  }

  fprintf(fp, "\n");
}

SC_LIST*
SC_LIST::Append(SC_NODE *sc, MEM_POOL *pool)
{
  SLIST sc_list_container(this);
  SC_LIST *new_sclst = (SC_LIST*)CXX_NEW(SC_LIST(sc), pool);
  if (new_sclst == NULL) ErrMsg ( EC_No_Mem, "SC_LIST::Append" );
  sc_list_container.Append(new_sclst);
  return (SC_LIST*)sc_list_container.Head();
}

SC_LIST *
SC_LIST::Remove(SC_NODE *sc, MEM_POOL *pool)
{
  SC_LIST *prev, *cur, *retval = this;
  
  if (sc == NULL) return this;

  for (prev=NULL,cur=this; cur && cur->node != sc; cur = cur->Next()) {
    prev = cur;
  }

  if (cur == NULL)
    return this;

  if (cur == this)
    retval = Next();

  cur->SLIST_NODE::Remove(prev);
  CXX_DELETE(cur, pool);
  return retval;
}

BOOL
SC_LIST::Contains(SC_NODE *sc) const
{
  SC_LIST_ITER sc_list_iter(this);
  SC_NODE *tmp;
  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    if (tmp == sc)
      return TRUE;
  }
  return FALSE;
}

SC_NODE *
SC_LIST::Last_elem()
{
  SC_LIST_ITER sc_list_iter(this);
  SC_NODE *tmp = NULL;
  SC_NODE * last = NULL;
  
  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    last = tmp;
  }

  return last;
}

SC_NODE *
SC_LIST::First_elem()
{
  SC_LIST_ITER sc_list_iter(this);
  SC_NODE *tmp = NULL;

  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    return tmp;
  }

  return NULL;
}

void
SC_LIST::Print(FILE *fp) const
{
  SC_LIST_ITER sc_list_iter(this);
  SC_NODE * tmp;
  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    if (tmp)
      fprintf(fp, "%d ",tmp->Id());
  }
  fprintf(fp, "\n ");
}

void
SC_LIST_ITER::Validate_unique(FILE *fp)
{
  for (First(); !Is_Empty(); Next()) {
    SC_NODE *tmp = Cur()->Node();
    if (tmp == NULL) {
      fprintf(fp, "Empty Node in the sc_list!!!\n");
      break;
    }
    if (Peek_Next()) {
      if (Peek_Next()->Contains(tmp)) {
	fprintf(fp, "The sc_list has redundant sc_node");
	this->Head()->Print(fp);
      }
    }
  }  
}

void
SC_LIST_CONTAINER::Append(SC_NODE *sc, MEM_POOL *pool)
{
  SC_LIST * new_sclst = (SC_LIST*)CXX_NEW(SC_LIST(sc), pool);
  if (new_sclst == NULL) ErrMsg ( EC_No_Mem, "SC_LIST::Append" );
  Append(new_sclst);
}


void 
SC_LIST_CONTAINER::Prepend(SC_NODE *sc, MEM_POOL *pool)
{
  SC_LIST *new_sclst = (SC_LIST*)CXX_NEW( SC_LIST(sc), pool );
  if ( new_sclst == NULL ) ErrMsg ( EC_No_Mem, "SC_LIST::Prepend" );
  Prepend(new_sclst);
}

void
SC_LIST_CONTAINER::Remove  (SC_NODE *sc, MEM_POOL *pool)
{
  Warn_todo("SC_LIST_CONTAINER::Remove: remove this call");
  SC_LIST *prev, *cur;

  if (sc == NULL) return;
  for (prev=NULL,cur=Head(); cur && cur->Node() != sc; cur = cur->Next())
    prev = cur;

  CXX_DELETE(cur->Remove(prev), pool);
}

SC_NODE *
SC_LIST_CONTAINER::Remove_head(MEM_POOL *pool)
{
  Warn_todo("SC_LIST_CONTAINER::Remove_head: remove this call");
  SC_NODE *sc;
  SC_LIST *head;

  head = Head();
  if (head == NULL)
    return NULL;
  sc = head->Node();
  CXX_DELETE(Remove_Headnode(), pool);
  return sc;
}

BOOL
SC_LIST_CONTAINER::Contains(SC_NODE *sc) const
{
  SC_LIST_ITER sc_list_iter(this);
  SC_NODE* tmp;
  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    if (tmp == sc)
      return TRUE;
  }
  return FALSE;
}

// reset/clear fields.
void
IF_MERGE_TRANS::Clear(void)
{
  CFG_TRANS::Clear();
  _action = DO_NONE;
  _pass = PASS_NONE;
  _region_id = 0;
}

// Create a new _invar_map.
void
CFG_TRANS::New_invar_map()
{
  _invar_map = CXX_NEW(MAP(CFG_BB_TAB_SIZE, _pool), _pool);
}

// Delete _invar_map.
void
CFG_TRANS::Delete_invar_map()
{
  if (_invar_map)
    CXX_DELETE(_invar_map, _pool);
  _invar_map = NULL;
}

// Query whether given aux_id represents a scalar non-address-taken non-virtual variable.

BOOL CFG_TRANS::Is_trackable_var(AUX_ID aux_id)
{
  OPT_STAB * op_stab = _cu->Opt_stab();

  if (aux_id && (aux_id <= op_stab->Lastidx())) {
    AUX_STAB_ENTRY * aux_entry = op_stab->Aux_stab_entry(aux_id);
    if (aux_entry && (aux_entry->Stype() == VT_NO_LDA_SCALAR)) {
      ST * st = aux_entry->St();

      if (st && (ST_class(st) == CLASS_VAR) 
	  && !ST_addr_passed(st)
	  && !ST_addr_saved(st))
	return TRUE;
    }
  }
  return FALSE;
}

// For every load in the expression tree rooted at the given wn, check whether
// it loads a scalar trackable variable.  See CFG_TRANS::Is_trackable_var.

BOOL CFG_TRANS::Is_trackable_expr(WN * wn)
{
  int i;
  OPCODE opc = WN_opcode(wn);
  if (!OPCODE_is_expression(opc))
    return FALSE;

  if ((opc == OPC_IO) || OPCODE_is_call(opc)) 
    return FALSE;

  if (OPCODE_is_load(opc)) {
    if (OPERATOR_is_scalar_load(WN_operator(wn))) {
      AUX_ID aux_id = WN_aux(wn);
      if (!Is_trackable_var(aux_id)) 
	return FALSE;
    }
    else
      return FALSE;
  }

  for ( i = 0; i < WN_kid_count(wn); i++) 
    if (!Is_trackable_expr(WN_kid(wn,i)))
      return FALSE;

  return TRUE;
}


// Query whether the values of loads in the given expression tree 
// are modified by the given SC_NODE.  We perform a quick and simple
// value-number hashing to the given SC_NODE. 
// eval_true indicates whether wn is evaluated to TRUE at the entry
// of sc.
BOOL CFG_TRANS::Val_mod(SC_NODE * sc, WN * wn, BOOL eval_true)
{
  BOOL ret_val = TRUE;

  OPT_POOL_Push(_pool, MEM_DUMP_FLAG + 1);
  Init_val_map(wn, eval_true);

  if (_val_map != NULL) {
    Track_val(sc, sc->First_bb(), wn);
    ret_val = !Val_match(wn);
  }

  Delete_val_map();
  OPT_POOL_Pop(_pool, MEM_DUMP_FLAG + 1);
  return ret_val;
}

// Query whether the values of loads in the given expression tree match
// values hashed in _val_map.
BOOL CFG_TRANS::Val_match(WN * wn)
{
  FmtAssert((_val_map != NULL), ("Expect non-NULL _val_map"));
  
  if (OPERATOR_is_scalar_load(WN_operator(wn))) {
    AUX_ID aux_id = WN_aux(wn);
    AUX_ID val = (AUX_ID) Get_val(aux_id);
    if (val != aux_id)
      return FALSE;
  }

  for (int i = 0; i < WN_kid_count(wn); i++)
    if (!Val_match(WN_kid(wn,i)))
      return FALSE;

  return TRUE;
}

// Free storage of _val_map.
void
CFG_TRANS::Delete_val_map()
{
  if (_val_map) {
    CXX_DELETE(_val_map, _pool);
    _val_map = NULL;
  }

  if (_true_val)
    _true_val = NULL;
}

// Initialize _val_map by hashing all loads in the given wn.
// eval_true indicates whether wn is evaluated to TRUE.
void
CFG_TRANS::Init_val_map(WN * wn, BOOL eval_true)
{
  if (OPERATOR_is_scalar_load(WN_operator(wn))) {
    AUX_ID aux_id = WN_aux(wn);
    if (aux_id) {
      if (_val_map == NULL)
	_val_map = CXX_NEW(MAP(CFG_BB_TAB_SIZE, _pool), _pool);
      _val_map->Add_map((POINTER) aux_id, (POINTER)aux_id);
    }
  }

  OPERATOR op = WN_operator(wn);

  // Infer non-zero values
  if (eval_true
      && ((op == OPR_NE) || (op == OPR_EQ))
      && (WN_operator(WN_kid1(wn)) == OPR_INTCONST)
      && OPERATOR_is_scalar_load(WN_operator(WN_kid0(wn)))) {
    AUX_ID aux_id = WN_aux(WN_kid0(wn));
    INT64 const_val = WN_const_val(WN_kid1(wn));

    if (((op == OPR_NE) && (const_val == 0))
	|| ((op == OPR_EQ) && (const_val != 0))) {
      if (_true_val == NULL) 
	_true_val = BS_Create_Empty(_cu->Opt_stab()->Lastidx() + 1, _pool);

      _true_val = BS_Union1D(_true_val, aux_id, _pool);
    }
  }

  for ( int i = 0; i < WN_kid_count(wn); i++) 
    Init_val_map(WN_kid(wn, i), FALSE);
}

// Obtain the hashed value number for the given AUX_ID.
AUX_ID
CFG_TRANS::Get_val(AUX_ID aux_id)
{
  AUX_ID val = 0;
  if (aux_id)
    val = (AUX_ID) (unsigned long) _val_map->Get_val((POINTER) aux_id);
  return val;
}

// Hash aux_id to val.
void CFG_TRANS::Set_val(AUX_ID aux_id, AUX_ID val)
{
  if (aux_id) {
    MAP_LIST * map_lst = _val_map->Find_map_list((POINTER) aux_id);
    if (map_lst)
      map_lst->Set_val((POINTER) val);
  }
}

// Interface to invoke alias info queries.
BOOL CFG_TRANS::Is_aliased(WN * wn1, WN * wn2)
{
  ALIAS_MANAGER * alias_mgr = _cu->Alias_mgr();
  WN * call_wn = NULL;
  WN * load_wn = NULL;

  if (WN_operator(wn1) == OPR_CALL)
    call_wn = wn1;
  else if (WN_operator(wn2) == OPR_CALL)
    call_wn = wn2;

  if (OPERATOR_is_scalar_load(WN_operator(wn1)))
    load_wn = wn1;
  else if (OPERATOR_is_scalar_load(WN_operator(wn2)))
    load_wn = wn2;

  if (Aliased(alias_mgr, wn1, wn2) != NOT_ALIASED) {
    if (call_wn && load_wn) {
      AUX_ID load_aux = WN_aux(load_wn);
      OPT_STAB * opt_stab = _cu->Opt_stab();
      ST * load_st = NULL;

      if (load_aux && (load_aux <= opt_stab->Lastidx()))
	load_st = opt_stab->Aux_stab_entry(load_aux)->St();

      if (Is_trackable_var(load_aux)) {
	ST * call_st = WN_sym(call_wn);
	INT mod = 0;
	INT ref = 0;
      
	opt_stab->check_ipa_mod_ref_info(call_st, load_st, &mod, &ref);
	if (mod == 0)
	  return FALSE;

	if (Is_Global_Symbol(load_st)
	    && _true_val
	    && BS_MemberP(_true_val, load_aux)) {
	  INT same_entry_exit_value_or_1 = 0;	  
	  opt_stab->check_ipa_same_entry_exit_value_or_1_info(call_st, load_st,
							      &same_entry_exit_value_or_1);
	  if (same_entry_exit_value_or_1)
	    return FALSE;
	}
      }
      else if (load_st && (ST_class(load_st) == CLASS_PREG))
	return FALSE;
    }
    return TRUE;
  }

  return FALSE;
}

// Given a WN, query whether all of its kids and itself can be speculative.

BOOL CFG_TRANS::Can_be_speculative(WN * wn)
{
  INT i;
  struct ALIAS_MANAGER * alias = _cu->Alias_mgr();
  
  for (i=0; i<WN_kid_count(wn); i++) {
    if (!Can_be_speculative(WN_kid(wn,i)))
      return FALSE;
  }

  OPERATOR op = WN_operator(wn);

  if (OPERATOR_is_store(op) || OPERATOR_is_load(op)) {
    if (WN_Is_Volatile_Mem(wn))
      return FALSE;
  }
  if ((op == OPR_ALLOCA) || (op == OPR_DEALLOCA)
      || (op == OPR_ASM_STMT)
      || (op == OPR_FORWARD_BARRIER) || (op == OPR_BACKWARD_BARRIER))
    return FALSE;
  
  if (OPCODE_is_call(WN_opcode(wn)))
    return FALSE;

  return TRUE;
}

// Given a BB_NODE, query whether all of its real statements can be speculative.
BOOL
CFG_TRANS::Can_be_speculative(BB_NODE * bb)
{
  WN * tmp;

  for (tmp = bb->Firststmt(); tmp != NULL; tmp = WN_next(tmp)) {
    if (!WN_is_executable(tmp))
      continue;
    
    if (!Can_be_speculative(tmp))
      return FALSE;
  }

  return TRUE;
}

// Given a SC_NODE, query whether all of its BB_NODEs can be speculative.
BOOL
CFG_TRANS::Can_be_speculative(SC_NODE * sc)
{
  BB_LIST_ITER bb_list_iter;
  BB_NODE * tmp;

  tmp = sc->Get_bb_rep();

  if ((tmp != NULL) && !Can_be_speculative(tmp))
    return FALSE;

  FOR_ALL_ELEM(tmp, bb_list_iter,Init(sc->Get_bbs())) {
    if (!Can_be_speculative(tmp))
      return FALSE;
  }

  SC_LIST_ITER sc_list_iter(sc->Kids());
  SC_NODE * sc_tmp;

  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init()) {
    if (!Can_be_speculative(sc_tmp))
      return FALSE;
  }
  
  return TRUE;
}

// For every load in wn, if it aliases with wn_iter,
// remove its value from _val_map

void CFG_TRANS::Remove_val(WN * wn_iter, WN * wn)
{
  FmtAssert((_val_map != NULL), ("Expect non-NULL _val_map"));
  OPCODE opc = WN_opcode(wn);
  
  if (OPCODE_is_load(opc)) {
    if (Is_aliased(wn_iter, wn))
      Set_val(WN_aux(wn), 0);
  }
  
  for (int i = 0; i < WN_kid_count(wn); i++) 
    Remove_val(wn_iter, WN_kid(wn,i));
}

// Track the flow of values for loads in the given WN tree by 
// evaluating all blocks in the given sc in source order. 
// bb_entry gives the initial Gen block of tracked values.
//
// Current implementation is quick and light-weight.
// In the future, consider using existing value-numbering
// mechanism.
 
void CFG_TRANS::Track_val(SC_NODE * sc, BB_NODE * bb_entry, WN * wn)
{

  FmtAssert(_val_map, ("Expect non-NULL _val_map"));
  BB_NODE * bb = sc->Get_bb_rep();

  if (bb != NULL)
    Track_val(bb, bb_entry, wn);
  
  BB_LIST * bb_list = sc->Get_bbs();

  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;
    
    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      Track_val(tmp, bb_entry, wn);
    }
  }

  SC_LIST * kids = sc->Kids();
  
  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE *tmp;
    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      Track_val(tmp, bb_entry, wn);
    }
  }
}

// Track the flow of values for loads in the given WN tree by
// evaluating all statements in the given bb_cur. bb_entry gives
// the initial Gen block of tracked values.

void CFG_TRANS::Track_val(BB_NODE * bb_cur, BB_NODE * bb_entry, WN * wn)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;
  ALIAS_MANAGER * alias_mgr = _cu->Alias_mgr();

  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb_cur->Firststmt(), bb_cur->Laststmt())) {
    OPCODE opc = WN_opcode(wn_iter);

    // Check kills.
    if (OPCODE_is_call(opc) || (opc == OPC_IO))
      Remove_val(wn_iter, wn);

    if (OPERATOR_is_scalar_store(WN_operator(wn_iter))) {
      WN * wn_data = WN_kid0(wn_iter);
      AUX_ID rval = (WN_has_sym(wn_data) &&
		     OPERATOR_is_load(WN_operator(wn_data))) ? Get_val(WN_aux(wn_data)) : 0;
      AUX_ID lval = Get_val(WN_aux(wn_iter));
      
      if (Is_trackable_var(WN_aux(wn_iter)) && rval
	  && bb_entry->Dominates(bb_cur)
	  && bb_cur->Postdominates(bb_entry)) {
	// For a store to a trackable variable in a block that is control-equivalent
	// to the bb_entry, transfer the RHS value (if exists) to the LHS.
	Set_val(WN_aux(wn_iter), rval);
      }
      else if (lval)
	Set_val(WN_aux(wn_iter), 0);
    }
  }
}

// Do top-down if-merging for the SC tree rooted at the given SC_NODE.
// CFG, LOOPs and SC tree are updated upon exit of this routine.

void
IF_MERGE_TRANS::Top_down_trans(SC_NODE * sc)
{
  SC_LIST * kids = sc->Kids();

  if (kids == NULL)
    return;

  CFG * cfg = _cu->Cfg();
  BOOL do_analyze_loops = cfg->Loops_valid(); 

  SC_LIST_ITER kids_iter;
  SC_NODE * tmp;
  SC_NODE * sc1 = NULL;
  SC_NODE * sc2 = NULL;
  SC_NODE * sc_new;

  // Do if-collapsing first. See IF_MERGE_TRANS::Is_uncond_cand.
  _action = DO_IFCOLLAPSE;
  sc1 = sc->First_kid_of_type(SC_IF);

  if (sc1 != NULL) {
    sc2 = sc1->Next_sibling_of_type(SC_IF);
    
    while (sc2 != NULL) {
      sc_new = Do_merge(sc1, sc2);

      if (sc_new) {
	sc1 = sc_new;
	sc2 = sc1->Next_sibling_of_type(SC_IF);
      }
      else {
	sc1 = sc2;
	sc2 = sc1->Next_sibling_of_type(SC_IF);
      }
    }
  }

  // Do if-merging
  _action = DO_IFMERGE;
  sc1 = sc->First_kid_of_type(SC_IF);
  while (sc1 != NULL) {
    sc2 = sc1->Next_sibling_of_type(SC_IF);
    
    while (sc2 != NULL) {
      sc_new = Do_merge(sc1, sc2);
      
      if (sc_new) {
	sc1 = sc_new;
	sc2 = sc1->Next_sibling_of_type(SC_IF);
      }
      else
	sc2 = sc2->Next_sibling_of_type(SC_IF);
    }

    sc1 = sc1->Next_sibling_of_type(SC_IF);
  }

  if (WOPT_Simplify_Bit_Op) {
    // Do if-flipping
    _action = DO_IFFLIP;
    sc1 = sc->First_kid_of_type(SC_IF);
    while (sc1 != NULL) {
      sc2 = sc1->Next_sibling_of_type(SC_IF);
    
      while (sc2 != NULL) {
	sc_new = Do_merge(sc1, sc2);
      
	if (sc_new) {
	  sc1 = sc_new;
	  sc2 = sc1->Next_sibling_of_type(SC_IF);
	}
	else
	  sc2 = sc2->Next_sibling_of_type(SC_IF);
      }

      sc1 = sc1->Next_sibling_of_type(SC_IF);
    }
  }

  _action = DO_IFMERGE;
  FOR_ALL_ELEM(tmp, kids_iter, Init(sc->Kids())) {
    this->Top_down_trans(tmp);
  }
  
  if (do_analyze_loops && !cfg->Loops_valid()) 
    cfg->Analyze_loops();
}

// Lower level if-merging of two if-regions.
// Upon exit of this routine, CFG are updated.
// Loops are invalidated but not updated.
void
IF_MERGE_TRANS::Merge_CFG(SC_NODE * sc1, SC_NODE * sc2)
{
  FmtAssert(((sc1->Type() == SC_IF) && (sc2->Type() == SC_IF)), ("Expect SC_Ifs"));
  CFG * cfg = _cu->Cfg();
  MEM_POOL * pool = cfg->Mem_pool();

  // find boundary BB_NODEs

  BB_NODE * sc1_then_end = sc1->Then_end();
  BB_NODE * sc1_else_end = sc1->Else_end();
  BB_NODE * sc1_else = sc1->Else();
  BB_NODE * sc1_merge = sc1->Merge();
  BB_NODE * sc2_then = sc2->Then();
  BB_NODE * sc2_else = sc2->Else();
  BB_NODE * sc2_then_end = sc2->Then_end();
  BB_NODE * sc2_else_end = sc2->Else_end();
  BB_NODE * sc2_merge = sc2->Merge();
  BB_NODE * sc2_head = sc2->Get_bb_rep();

  if (_trace) {
    if (Transform_count() == 0) {
      printf("\n*********** If-merge for %s(%d) ***********\n",
	     Current_PU_Name(), Current_PU_Count());
    }

    switch (_action) {
    case DO_IFFLIP:
      // From:
      // if (a & (1 << b)) 
      //   block 1;
      // a ^= (1 << b);
      // if (a & (1 << b))
      //   block 2;
      //
      // To:
      // if (a & ( 1 << b)) {
      //   block 1;
      //   a ^= ( 1 << b);
      // }
      // else {
      //   a ^= (1 << b);
      //   block 2;
      // }
      
      printf("\n\t If-flip (SC%d,SC%d)\n", 
	     sc1->Id(), sc2->Id());
      break;
    case DO_IFMERGE:
      // From:
      // if (a)
      //  block 1;
      // else
      //  block 2;
      // if (a)
      //   block 3;
      // else
      //   block 4;
      //
      // To:
      // if (a) {
      //   block 1;
      //   block 3;
      // }
      // else {
      //   block 2;
      //   block 4;
      // }
      
      printf("\n\t If-merge (SC%d,SC%d)\n", 
	     sc1->Id(), sc2->Id());
      break;
    case DO_IFCOLLAPSE:
      // From:
      // if (a) 
      //  x = const1;
      // else
      //  x = const2;
      // if (x = const1)
      //   block1;
      // else
      //   block2;
      // 
      // where const1 != const2
      //
      // To:
      // if (a) {
      //  x = const1;
      //  block1;
      // }
      // else {
      //  x= const2;
      //  block 2;
      // }
      printf("\n\t If-collapse (SC%d,SC%d)\n", 
	     sc1->Id(), sc2->Id());
      break;
    default:
      ;
    }
  }

  if (_dump) {
    fprintf(TFile, "\n Before if-merge\n");
    cfg->Print(TFile, false, (unsigned) -1) ;
  }

  sc1_then_end->Replace_succ(sc1_merge, sc2_then);

  if (cfg->Feedback())
    cfg->Feedback()->Move_edge_dest(sc1_then_end->Id(), sc1_merge->Id(), sc2_then->Id());

  sc1_else_end->Replace_succ(sc1_merge, sc2_else);

  if (cfg->Feedback())
    cfg->Feedback()->Move_edge_dest(sc1_else_end->Id(), sc1_merge->Id(), sc2_else->Id());
  
  sc2_then->Replace_pred(sc2_head, sc1_then_end);
  
  if (cfg->Feedback())
    cfg->Feedback()->Delete_edge(sc2_head->Id(), sc2_then->Id());

  sc2_else->Replace_pred(sc2_head, sc1_else_end);

  if (cfg->Feedback())
    cfg->Feedback()->Delete_edge(sc2_head->Id(), sc2_else->Id());

  sc2_then_end->Replace_succ(sc2_merge, sc1_merge);
  
  if (cfg->Feedback())
    cfg->Feedback()->Move_edge_dest(sc2_then_end->Id(), sc2_merge->Id(), sc1_merge->Id());

  sc2_else_end->Replace_succ(sc2_merge, sc1_merge);
  
  if (cfg->Feedback())
    cfg->Feedback()->Move_edge_dest(sc2_else_end->Id(), sc2_merge->Id(), sc1_merge->Id());

  sc1_merge->Replace_pred(sc1_then_end, sc2_then_end);
  sc1_merge->Replace_pred(sc1_else_end, sc2_else_end);

  sc2_head->Remove_succ(sc2_then, pool);
  sc2_head->Replace_succ(sc2_else, sc2_merge);

  sc2_merge->Remove_pred(sc2_then_end, pool);
  sc2_merge->Replace_pred(sc2_else_end, sc2_head);

  if (cfg->Feedback()) {
    cfg->Feedback()->Add_edge(sc2_head->Id(), sc2_merge->Id(), FB_EDGE_OUTGOING, 
			      cfg->Feedback()->Get_edge_freq(sc2_then_end->Id(), sc1_merge->Id())
			      + cfg->Feedback()->Get_edge_freq(sc2_else_end->Id(), sc1_merge->Id()));

  }

  Delete_branch(sc2_head);

  // Fix Prev() and Next()
  sc1_then_end->Set_next(sc2_then);
  sc2_then->Set_prev(sc1_then_end);

  sc2_then_end->Set_next(sc1_else);
  sc1_else->Set_prev(sc2_then_end);

  sc1_else_end->Set_next(sc2_else);
  sc2_else->Set_prev(sc1_else_end);
  
  sc2_else_end->Set_next(sc1_merge);
  sc1_merge->Set_prev(sc2_else_end);
  
  sc2_head->Set_next(sc2_merge);
  sc2_merge->Set_prev(sc2_head);

  cfg->Invalidate_and_update_aux_info(FALSE);
  cfg->Invalidate_loops();

}

// Merge sc2 into sc1:
// sc1 takes over sc2's kids, unlink sc2 from the SC tree.

void
IF_MERGE_TRANS::Merge_SC(SC_NODE * sc1, SC_NODE * sc2)
{
  FmtAssert((sc1->Type() == sc2->Type()), ("Expect same SC type"));
  SC_LIST * kids2 = sc2->Kids();

  // If sc2 has no kids, simply unlink it from the SC tree.
  if (kids2 == NULL) {
    sc2->Unlink();
    return;
  }
  
  // If Both sc1's last kid and sc2's first kid are SC_BLOCKs, merge BB_NODEs
  SC_NODE * last1 = sc1->Last_kid();
  SC_NODE * first2 = sc2->First_kid();

  if ((last1 != NULL) && (last1->Type() == first2->Type())
      && (last1->Type() == SC_BLOCK)) {
    BB_LIST * bbs = first2->Get_bbs();
    MEM_POOL * pool = first2->Get_pool();

    while (bbs) {
      BB_NODE * bb = bbs->Node();
      last1->Append_bbs(bb);
      bbs = bbs->Remove(bb, pool);
    }
    
    first2->Set_bbs(NULL);
    first2->Unlink();
  }

  // Merge remaining kids from sc2 into sc1.
  first2 = sc2->First_kid();

  while (first2) {
    sc1->Append_kid(first2);
    first2->Set_parent(sc1);
    sc2->Remove_kid(first2);
    first2 = sc2->First_kid();
  }
  
  sc2->Unlink();
  sc2->Delete();
}

// Attempt if-merging for the given pair of SC_NODEs.  If successful, return
// the merged SC_NODE.
SC_NODE *
IF_MERGE_TRANS::Do_merge(SC_NODE * sc1, SC_NODE * sc2)
{
  if ((WOPT_Enable_Pro_Loop_Limit >= 0)
      && (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
    return NULL;

  if (!Is_candidate(sc1, sc2, FALSE))
    return NULL;

  // Merge CFG BBs.
  Merge_CFG(sc1, sc2);

  CFG * cfg = _cu->Cfg();

  if (_dump) {
    fprintf(TFile, "\n====SC tree before if-merge====\n");
    cfg->SC_root()->Print(TFile,1);
  }
  
  // Merge SC tree.
  SC_NODE * then1 = sc1->Find_kid_of_type(SC_THEN);
  SC_NODE * else1 = sc1->Find_kid_of_type(SC_ELSE);
  SC_NODE * then2 = sc2->Find_kid_of_type(SC_THEN);
  SC_NODE * else2 = sc2->Find_kid_of_type(SC_ELSE);

  Merge_SC(then1, then2);
  Merge_SC(else1, else2);

  sc2->Convert(SC_BLOCK);
  
  // Converting sc2 into a SC_BLOCK could expose opportunities to 
  // merge SC_BLOCKs among sc2's immediate siblings.

  SC_NODE * sc_blk = sc2->Prev_sibling();
  
  if ((sc_blk == NULL) || (sc_blk->Type() != SC_BLOCK))
    sc_blk = sc2;
  
  FmtAssert((sc_blk->Kids() == NULL), ("SC_BLOCK should have no kid"));
  
  SC_NODE * next_sibling = sc_blk->Next_sibling();
  
  while ((next_sibling) && (next_sibling->Type() == SC_BLOCK)) {
    // merge next_sibling's BB_NODES into sc_blk.
    BB_LIST * bb_list = next_sibling->Get_bbs();
    SC_NODE * next = next_sibling->Next_sibling();
    MEM_POOL * pool = next_sibling->Get_pool();
    
    FmtAssert((next_sibling->Kids() == NULL), ("SC_BLOCK should have no kid"));
    
    while (bb_list) {
      BB_NODE * bb = bb_list->Node();
      bb_list = bb_list->Remove(bb, pool);
      sc_blk->Append_bbs(bb);
    }
    
    next_sibling->Unlink();
    next_sibling = next;
  }

  if (_dump) {
    fprintf(TFile, "\n====SC tree After if-merge====\n");
    cfg->SC_root()->Print(TFile,1);
  }

  FmtAssert(sc1->Is_well_behaved(), ("Not well-behaved after if-merge"));
  Inc_transform_count();
  return sc1;
}

// Query whether loads/stores in the WN tree rooted at wn_root could alias with wn1,
// where wn1 is a store/call statement.

BOOL
CFG_TRANS::Maybe_assigned_expr(WN * wn1, WN * wn_root)
{
  OPCODE opc = WN_opcode(wn_root);

  if (OPCODE_is_load(opc) || OPCODE_is_store(opc) || (opc == OPC_IO)) {
    OPCODE opc = WN_opcode(wn1);

    // OPC_IO has side effect and hidden control flow.
    if (opc == OPC_IO)
      return TRUE;

    ALIAS_MANAGER * alias_mgr = _cu->Alias_mgr();

    if ((OPCODE_is_store(opc) || OPCODE_is_call(opc))
	&& Is_aliased(wn1, wn_root))
      return TRUE;
  }
  if (WOPT_Simplify_Bit_Op) {
    // No alias if wn1 is a reduction of a single bit operation on an object, 
    // and wn_root is a bit operation on a different bit of the same object.
    WN * wn_bit_op = WN_get_bit_reduction(wn1);

    if (wn_bit_op && WN_is_bit_op(wn_root)
	&& (WN_Simp_Compare_Trees(WN_kid(wn_root,0), WN_kid(wn_bit_op, 0)) == 0)) {
      WN * wn_tmp1 = WN_kid(wn_root, 1);

      if (WN_is_power_of_2(wn_tmp1)) {
	WN * wn_tmp2 = WN_kid(wn_bit_op, 1);

	Match_def(wn_tmp2);
	Match_def(wn_tmp1);

	if (WN_has_disjoint_val_range(wn_tmp2, wn_tmp1, _low_map, _high_map))
	  return FALSE;
      }
    }
  }

  for ( int i = 0; i < WN_kid_count(wn_root); i++) {
    if (Maybe_assigned_expr(wn1, WN_kid(wn_root,i)))
      return TRUE;
  }

  return FALSE;
}

// Query whether loads/stores in the WN tree rooted at wn_root could alias with
// a store/call statement in bb.

BOOL
CFG_TRANS::Maybe_assigned_expr(BB_NODE * bb, WN * wn_root)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;

  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb->Firststmt(), bb->Laststmt())) {
    if (Maybe_assigned_expr(wn_iter, wn_root))
      return TRUE;
  }

  return FALSE;
}

// Query whether loads/stores in the WHILE tree rooted at wn_root could
// alias with a store/call statement  in the SC tree rooted at sc. 
// eval_true indicates whether wn_root is evaluated to TRUE at the entry
// of the sc.

BOOL
CFG_TRANS::Maybe_assigned_expr(SC_NODE * sc, WN * wn_root, BOOL eval_true)
{
  OPERATOR opr = WN_operator(wn_root);

  if ((opr == OPR_INTCONST) || (opr == OPR_CONST))
    return FALSE;
  
  if (Is_trackable_expr(wn_root)) {
    // Track whether values of loads in the WHILR tree rooted at wn_root
    // are modified by sc
    if (Val_mod(sc, wn_root, eval_true))
      return TRUE;
    else
      return FALSE;
  }

  BB_NODE * bb = sc->Get_bb_rep();

  if ((bb != NULL) && Maybe_assigned_expr(bb, wn_root))
    return TRUE;
  
  BB_LIST * bb_list = sc->Get_bbs();

  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;
    
    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      if (Maybe_assigned_expr(tmp, wn_root))
	return TRUE;
    }
  }

  SC_LIST * kids = sc->Kids();
  
  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE *tmp;
    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      if (Maybe_assigned_expr(tmp, wn_root, FALSE))
	return TRUE;
    }
  }

  return FALSE;
}

// Query whether loads/stores of WN trees in bb could alias with a store/call
// statement in the SC tree rooted at sc.

BOOL
CFG_TRANS::Maybe_assigned_expr(SC_NODE *sc1, BB_NODE *bb)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;

  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb->Firststmt(), bb->Laststmt())) {
    if ((WN_operator(wn_iter) == OPR_FALSEBR)
	|| (WN_operator(wn_iter) == OPR_TRUEBR))
      wn_iter = WN_kid0(wn_iter);

    if (Maybe_assigned_expr(sc1, wn_iter, FALSE))
      return TRUE;
  }

  return FALSE;
}

// Query whether loads/stores of WN trees in bb2 could alias with a store/call
// statement in bb1.

BOOL
CFG_TRANS::Maybe_assigned_expr(BB_NODE *bb1, BB_NODE *bb2)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;

  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb2->Firststmt(), bb2->Laststmt())) {
    if (Maybe_assigned_expr(bb1, wn_iter))
      return TRUE;
  }

  return FALSE;
}

// Query whether loads/stores of WN tree in SC tree rooted at sc could alias
// with a store/call statement in bb.

BOOL
CFG_TRANS::Maybe_assigned_expr(BB_NODE * bb, SC_NODE * sc)
{
  BB_NODE * tmp = sc->Get_bb_rep();
  
  if ((tmp != NULL) && Maybe_assigned_expr(bb, tmp))
    return TRUE;

  BB_LIST * bb_list = sc->Get_bbs();

  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;
    
    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      if (Maybe_assigned_expr(bb, tmp))
	return TRUE;
    }
  }
  
  SC_LIST * kids = sc->Kids();

  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE *tmp;
    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      if (Maybe_assigned_expr(bb, tmp))
	return TRUE;
    }
  }
  
  return FALSE;
}

// Query whether loads/stores of WN trees in the SC tree rooted at sc2 could alias
// with a store/call statement in the SC tree rooted at sc1.

BOOL
CFG_TRANS::Maybe_assigned_expr(SC_NODE * sc1, SC_NODE * sc2)
{
  BB_NODE * bb = sc2->Get_bb_rep();

  if ((bb != NULL) && Maybe_assigned_expr(sc1, bb))
    return TRUE;

  BB_LIST * bb_list = sc2->Get_bbs();
  
  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;

    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      if (Maybe_assigned_expr(sc1, tmp))
	return TRUE;
    }
  }

  SC_LIST * kids = sc2->Kids();

  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE *tmp;
    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      if (Maybe_assigned_expr(sc1, tmp))
	return TRUE;
    }
  }
  
  return FALSE;
}

// Check whether sc2 can be collapsed into sc1
// Look for the pattern like below. This can be considered as a special
// case of if-merging.
// if () {
//   ......
//   x = const1;
// }
// else {
//   ......
//   x = const2;
// }
// if (x == const1) {
//   ......
// }
// else {
//   ......
// }
// where the value of const1 is not equal to the value of const2.
BOOL
IF_MERGE_TRANS::Is_if_collapse_cand(SC_NODE * sc1, SC_NODE * sc2)
{
  if (sc1->Next_sibling() != sc2)
    return FALSE;

  WN * wn;
  BB_NODE * head2 = sc2->Head();

  // Count non-label statements in head2
  STMT_ITER stmt_iter;
  int count = 0;

  FOR_ALL_ELEM (wn, stmt_iter, Init(head2->Firststmt(), head2->Laststmt())) {
    if (WN_is_executable(wn))
      count++;
  }
  
  // head2 should only contain a comparison on EQ between a I4 constant and a scalar load.
  if (count != 1)
    return FALSE;

  wn = head2->Laststmt();
  wn = WN_kid0(wn);
  if (WN_operator(wn) != OPR_EQ)
    return FALSE;

  WN * wn_const = NULL;
  WN * wn_load = NULL;

  for ( int i = 0; i < WN_kid_count(wn); i ++) {
    WN * kid = WN_kid(wn,i);
    if (WN_operator(kid) == OPR_INTCONST)
      wn_const = kid;
    else if (OPERATOR_is_scalar_load(WN_operator(kid)))
      wn_load = kid;
  }

  if (!wn_const || !wn_load)
    return FALSE;
  
  // Check whether both then-path and else-path of sc1 end with a scalar store of a
  // constant value, the store has the same symbol as wn_load, the constant value
  // on the then-path is the same as wn_const, and the constant value on the else-path
  // is different from wn_const.
  
  BB_NODE * then_end = sc1->Then_end();
  BB_NODE * else_end = sc1->Else_end();

  wn = then_end->Laststmt();
  if (!wn 
      || !OPERATOR_is_scalar_store(WN_operator(wn))
      || (WN_aux(wn) != WN_aux(wn_load))
      || (WN_operator(WN_kid0(wn)) != OPR_INTCONST)
      || (WN_const_val(WN_kid0(wn)) != WN_const_val(wn_const)))
    return FALSE;

  wn = else_end->Laststmt();
  if (!wn
      || !OPERATOR_is_scalar_store(WN_operator(wn))
      || (WN_aux(wn) != WN_aux(wn_load))
      || (WN_operator(WN_kid0(wn)) != OPR_INTCONST)
      || (WN_const_val(WN_kid0(wn)) == WN_const_val(wn_const)))
    return FALSE;

  return TRUE;
}

// Check whether the given pair of SC_NODEs are if-merging candidates.
// where, sc1 precedes sc2 in source order.  Do not invoke tail duplication
// transformation if do_query is TRUE.

BOOL
IF_MERGE_TRANS::Is_candidate(SC_NODE * sc1, SC_NODE * sc2, BOOL do_query)
{
  if ((sc1->Type() != SC_IF) || (sc2->Type() != SC_IF))
    return FALSE;

  // should have the same parent in the SC tree.
  if (sc1->Parent() != sc2->Parent())
    return FALSE;

  BB_NODE * rep1 = sc1->Get_bb_rep();
  BB_NODE * rep2 = sc2->Get_bb_rep();

  BB_IFINFO * info1 = rep1->Ifinfo();
  BB_IFINFO * info2 = rep2->Ifinfo();

  BB_NODE * tail1 = info1->Merge();
  BB_NODE * tail2 = info2->Merge();
  BB_NODE * head1 = rep1;
  BB_NODE * head2 = rep2;

  if (tail1->Pred()->Len() != 2)
    return FALSE;

  if (tail2->Pred()->Len() != 2)
    return FALSE;
  
  // tail1 should be control-equivalent to head2.
  if ((tail1 != head2)
      && (!tail1->Dominates(head2) || !head2->Postdominates(tail1)))
    return FALSE;

  if (!sc1->Is_well_behaved())
    return FALSE;

  if (!sc2->Is_well_behaved())
    return FALSE;

  // If merge SC is a SC_BLOCK, it should be a SESE.

  SC_NODE * next_sibling = sc1->Next_sibling();

  if (next_sibling
      && (next_sibling->Type() == SC_BLOCK)
      && !next_sibling->Is_sese())
    return FALSE;

  next_sibling = sc2->Next_sibling();
  
  if (next_sibling
      && (next_sibling->Type() == SC_BLOCK)
      && !next_sibling->Is_sese())
    return FALSE;
  
  // Check whether sc2 can be if-collapsed with sc1.
  // See IF_MERGE_TRANS:Is_if_collapse_cand.
  if (_action == DO_IFCOLLAPSE) {
    if (Is_if_collapse_cand(sc1, sc2)) 
      return TRUE;
    else
      return FALSE;
  }

  // Should have the same condition expression value
  WN * cond1 = head1->Laststmt();
  WN * cond2 = head2->Laststmt();  
  OPCODE op1 = WN_opcode(cond1);
  OPCODE op2 = WN_opcode(cond2);

  FmtAssert(((op1 == OPC_FALSEBR) || (op1 == OPC_TRUEBR)), ("Unexpect cond"));
  FmtAssert(((op2 == OPC_FALSEBR) || (op2 == OPC_TRUEBR)), ("Unexpect cond"));
  
  if (op1 != op2)
    return FALSE;

  WN * expr1 = WN_kid0(cond1);
  WN * expr2 = WN_kid0(cond2);

  if (WN_Simp_Compare_Trees(expr1, expr2) != 0)
    return FALSE;

  SC_LIST_ITER sc_list_iter;
  SC_NODE* tmp;
  BOOL no_alias = FALSE;

  // Use hashed result.
  if (_invar_map) {
    SC_NODE * loop1 = (SC_NODE *) _invar_map->Get_val((POINTER) head1->Id());
    SC_NODE * loop2 = (SC_NODE *) _invar_map->Get_val((POINTER) head2->Id());
    
    if (loop1 && (loop1 == loop2) && 
	(loop1->Is_pred_in_tree(sc1) || (_region_id == loop1->Id())))
      no_alias = TRUE;
  }

  Infer_val_range(sc1, sc2);

  BOOL do_flip = FALSE;

  if (!no_alias) {
    // sc1' then-path and else-path should not modify condition expression.
    FOR_ALL_ELEM(tmp, sc_list_iter, Init(sc1->Kids())) {
      if (Maybe_assigned_expr(tmp, expr1, (tmp->Type() == SC_THEN) ? TRUE : FALSE)) {
	Delete_val_range_maps();
	return FALSE;
      }
    }

    next_sibling = sc1->Next_sibling();

    if (_action == DO_IFFLIP) {
      // Match pattern:
      // if (a & (1 << b)) {
      //   ....
      // }
      // a ^= (1 << b);
      // if (a & (1 << b)) {
      //   ......
      // }

      OPERATOR opr = WN_operator(expr1);
      if ((opr != OPR_EQ) && (opr != OPR_NE)) {
	Delete_val_range_maps();
	return FALSE;
      }
      
      WN * wn_const = WN_kid(expr1, 1);
      if ((WN_operator(wn_const) != OPR_INTCONST)
	  || (WN_const_val(wn_const) != 0)) {
	Delete_val_range_maps();
	return FALSE;
      }
      
      WN * wn_and = WN_kid(expr1, 0);

      if (!wn_and || (WN_operator(wn_and) != OPR_BAND)) {
	Delete_val_range_maps();
	return FALSE;
      }

      WN * wn1 = WN_kid(wn_and, 1);
      
      if (!WN_is_power_of_2(wn1)) {
	Delete_val_range_maps();
	return FALSE;
      }
      
      // All siblings between sc1 and sc2 (exclusive) should contain only one statement
      // that flips the condition expression.
      int stmt_cnt = 0;
      while (next_sibling && (next_sibling != sc2)) {
	int local_cnt = next_sibling->Executable_stmt_count();
	
	if (local_cnt == 0) {
	  next_sibling = next_sibling->Next_sibling();
	  continue;
	}
	else {
	  stmt_cnt += local_cnt;
	  if (stmt_cnt > 1) {
	    Delete_val_range_maps();
	    return FALSE;
	  }
	  
	  WN * wn_flip = next_sibling->First_executable_stmt();
	  
	  if (!wn_flip) {
	    Delete_val_range_maps();
	    return FALSE;
	  }

	  WN * wn_xor = WN_get_bit_reduction(wn_flip);

	  if (!wn_xor || (WN_operator(wn_xor) != OPR_BXOR)) {
	    Delete_val_range_maps();
	    return FALSE;
	  }

	  if (WN_Simp_Compare_Trees(WN_kid(wn_and, 0), WN_kid(wn_xor,0)) != 0) {
	    Delete_val_range_maps();
	    return FALSE;
	  }
	  
	  WN * wn2 = WN_kid(wn_xor, 1);

	  if (!WN_is_power_of_2(wn2)) {
	    Delete_val_range_maps();
	    return FALSE;
	  }

	  if (WN_Simp_Compare_Trees(wn1, wn2) != 0) {
	    Delete_val_range_maps();
	    return FALSE;
	  }

	  do_flip = TRUE;
	}
	next_sibling = next_sibling->Next_sibling();
      }
    }
    else {
      // All siblings between sc1 and sc2 (exclusive) should not
      // modify condition expression.
      while (next_sibling && (next_sibling != sc2)) {
	if (Maybe_assigned_expr(next_sibling, expr2, FALSE)) {
	  Delete_val_range_maps();
	  return FALSE;
	}

	next_sibling = next_sibling->Next_sibling();
      }
    }

    // head2 should not modifiy condition expression.
    if (Maybe_assigned_expr(head2, expr2)) {
      Delete_val_range_maps();
      return FALSE;
    }
  }

  // sc2's then-path and else-path should have no dependency on head2
  // excluding the conditional branch.

  if (head2->Executable_stmt_count() > 1) {
    BOOL has_non_sp = !Can_be_speculative(head2);
    
    FOR_ALL_ELEM(tmp, sc_list_iter, Init(sc2->Kids())) {
      if (!no_alias && Has_dependency(tmp, head2)) {
	Delete_val_range_maps();
	return FALSE;
      }

      // Do not reorder operations that can not be speculative.
      if (has_non_sp && !Can_be_speculative(tmp)) {
	Delete_val_range_maps();
	return FALSE;
      }
    }
  }

  // For every sibling between sc1 and sc2 (exclusive), if it has 
  // dependency on sc2, and all siblings are SC_BLOCKs, do tail
  // duplication.

  BOOL has_dep = FALSE;
  BOOL all_blk = TRUE;
  BOOL has_non_sp = FALSE;
  BOOL all_sese = TRUE;

  next_sibling = sc1->Next_sibling();
  int count = 0;

  while (next_sibling && (next_sibling != sc2)) {
    count++;

    FOR_ALL_ELEM(tmp, sc_list_iter, Init(sc2->Kids())) {
      if (Has_dependency(next_sibling, tmp))
	has_dep = TRUE;
    }

    if (!Can_be_speculative(next_sibling))
      has_non_sp = TRUE;

    if (next_sibling->Type() != SC_BLOCK)
      all_blk = FALSE;
    else if (!next_sibling->Is_sese())
      all_sese = FALSE;
    
    next_sibling = next_sibling->Next_sibling();
  }

  Delete_val_range_maps();

  if (!has_dep) {
    // Do not reorder operations that can not be speculative.
    if (has_non_sp && !Can_be_speculative(sc2))
      return FALSE;
    else if (_action == DO_IFFLIP)
      return FALSE;
    else
      return TRUE;
  }

  if (!all_blk || !all_sese)
    return FALSE;

  // Heuristic: in the global pass, avoid tail duplication unless both
  // SC_NODEs contain loop.

  if ((_pass == PASS_GLOBAL)
      && (!sc1->Has_loop() || !sc2->Has_loop()))
    return FALSE;

  if (!do_query) {
    next_sibling = sc1->Next_sibling();
  
    while (next_sibling && (next_sibling != sc2)) {
      Do_tail_duplication(next_sibling, sc1);
      next_sibling = sc1->Next_sibling();
    }

    // Swap SC_THEN and SC_ELSE for sc2.
    if (do_flip) {
      CFG * cfg = _cu->Cfg();
      MEM_POOL * pool = cfg->Mem_pool();
      SC_NODE * sc_then = sc2->Find_kid_of_type(SC_THEN);
      SC_NODE * sc_else = sc2->First_kid_of_type(SC_ELSE);
      BB_NODE * bb_head = sc2->Head();
      BB_NODE * bb_then = sc2->Then();
      BB_NODE * bb_else = sc2->Else();
      BB_NODE * bb_then_end = sc2->Then_end();
      BB_NODE * bb_else_end = sc2->Else_end();
      BB_NODE * bb_tmp = bb_then->Prev();
      bb_tmp->Set_next(bb_else);
      bb_else->Set_prev(bb_tmp);
      bb_tmp = bb_else_end->Next();
      bb_else_end->Set_next(bb_then);
      bb_then->Set_prev(bb_else_end);
      bb_then_end->Set_next(bb_tmp);
      bb_tmp->Set_prev(bb_then_end);
      bb_tmp = bb_head->Last_succ();
      bb_head->Remove_succ(bb_tmp, pool);
      bb_head->Prepend_succ(bb_tmp, pool);
      cfg->Add_label_with_wn(bb_then);
      WN * branch_wn = bb_head->Branch_wn();
      WN_label_number(branch_wn) = bb_then->Labnam();
      
      SC_LIST * then_kids = sc_then->Kids();
      SC_LIST * else_kids = sc_else->Kids();
      sc_then->Set_kids(else_kids);
      sc_else->Set_kids(then_kids);
      
      SC_LIST_ITER sc_list_iter;
      SC_NODE * sc_tmp;

      FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init(else_kids)) {
	sc_tmp->Set_parent(sc_then);
      }

      FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init(then_kids)) {
	sc_tmp->Set_parent(sc_else);
      }
     
      cfg->Fix_info(sc2);
      cfg->Invalidate_and_update_aux_info(FALSE);
      cfg->Invalidate_loops();
      Inc_transform_count();
    }
  }

  return TRUE;
}

// Query whether sc1 and sc2 have dependency on each other.
BOOL
CFG_TRANS::Has_dependency(SC_NODE * sc1, SC_NODE * sc2)
{
  if (sc1->Type() == SC_IF) {
    // Recursively query TRUE path and FALSE path
    // to detect value equality on two paths separately.
    if (Has_dependency(sc1->First_kid(), sc2)
	|| Has_dependency(sc1->Last_kid(), sc2))
      return TRUE;

    // Check head of SC_IF.
    BB_NODE * head = sc1->Get_bb_rep();

    if (Maybe_assigned_expr(head, sc2)
	|| Maybe_assigned_expr(sc2, head))
      return TRUE;
  }
  else  if (Maybe_assigned_expr(sc1, sc2)
	    || Maybe_assigned_expr(sc2, sc1))
    return TRUE;

  return FALSE;
}

// Query whether SC_NODE and BB_NODE have dependency on each other.
BOOL
CFG_TRANS::Has_dependency(SC_NODE * sc, BB_NODE * bb)
{
  if (sc->Type() == SC_IF) {
    if (Has_dependency(sc->First_kid(), bb)
	|| Has_dependency(sc->Last_kid(), bb))
      return TRUE;

    // Check head of SC_IF.
    BB_NODE * head = sc->Get_bb_rep();

    if (Maybe_assigned_expr(head, bb)
	|| Maybe_assigned_expr(bb, head))
      return TRUE;
  }
  else if (Maybe_assigned_expr(sc, bb)
	   || Maybe_assigned_expr(bb, sc))
    return TRUE;

  return FALSE;
}

// Query whether bb1 and bb2 have dependency on each other.
BOOL
CFG_TRANS::Has_dependency(BB_NODE * bb1, BB_NODE * bb2)
{
  if (Maybe_assigned_expr(bb1, bb2)
      || Maybe_assigned_expr(bb2, bb1))
    return TRUE;
  
  return FALSE;
}

// Query whether bb and wn have dependency on each other.
BOOL
CFG_TRANS::Has_dependency(BB_NODE * bb, WN * wn)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;

  if (!WN_is_executable(wn))
    return FALSE;

  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb->Firststmt(), bb->Laststmt())) {
    if (Maybe_assigned_expr(wn_iter, wn)
	|| Maybe_assigned_expr(wn, wn_iter))
      return TRUE;
  }

  return FALSE;
}

// Query whether sc and wn have dependency on each other.
BOOL
CFG_TRANS::Has_dependency(SC_NODE * sc, WN * wn)
{
  if (!WN_is_executable(wn))
    return FALSE;
  
  BB_NODE * bb = sc->Get_bb_rep();

  if ((bb != NULL) && Has_dependency(bb, wn))
    return TRUE;

  BB_LIST * bb_list = sc->Get_bbs();

  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;

    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      if (Has_dependency(tmp, wn))
	return TRUE;
    }
  }

  SC_LIST * kids = sc->Kids();

  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE *tmp;
    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      if (Has_dependency(tmp, wn))
	return TRUE;
    }
  }
  
  return FALSE;
}

// Remove branch in the given BB_NODE *.
void
CFG_TRANS::Delete_branch(BB_NODE * bb_head)
{
  WN * branch_wn = bb_head->Branch_wn();
  WN * last_stmt = bb_head->Laststmt();

  if (branch_wn && (branch_wn == last_stmt)) {
    if (bb_head->Ifinfo())
      bb_head->Set_ifinfo(NULL);

    WN * prev_stmt = WN_prev(last_stmt);

    if (prev_stmt)
      WN_next(prev_stmt) = NULL;
    WN_prev(last_stmt) = NULL;

    bb_head->Set_laststmt(prev_stmt);
	      
    if (prev_stmt == NULL)
      bb_head->Set_firststmt(NULL);

    bb_head->Set_kind(BB_GOTO);
  }
}

// Check whether wn has a side effect that could modify the program's state.
BOOL
CFG_TRANS::Has_side_effect(WN * wn)
{
  if (!Can_be_speculative(wn))
    return TRUE;

  OPERATOR opr = WN_operator(wn);

  if (!WN_is_executable(wn))
    return FALSE;

  OPT_STAB * op_stab = _cu->Opt_stab();

  if (OPERATOR_is_store(opr)) {
    // scalar stores to local non-address taken variables have no side effect.
    if (OPERATOR_is_scalar_store(opr)) {
      AUX_ID aux_id = WN_aux(wn);
      if (!aux_id)
	return TRUE;
      
      ST * st = Get_st(wn);
      if (st && (ST_sclass(st) == SCLASS_AUTO)
	  && !ST_addr_passed(st) 
	  && !ST_addr_saved(st))
	return FALSE;
    }
  }
  // scalar load has no side effect.
  else if (OPERATOR_is_scalar_load(opr))
    return FALSE;
  
  return TRUE;
}

// Delete _def_cnt_map.
void
PRO_LOOP_INTERCHANGE_TRANS::Delete()
{
  if (_def_cnt_map)
    CXX_DELETE(_def_cnt_map, _pool);

  _def_cnt_map = NULL;
}


// Hash _def_cnt_map for all statements in the given sc.
void
PRO_LOOP_INTERCHANGE_TRANS::Hash_def_cnt_map(BB_NODE * bb)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;

  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb->Firststmt(), bb->Laststmt())) {
    if (OPERATOR_is_scalar_store(WN_operator(wn_iter))
	&& !Has_side_effect(wn_iter)) {
      AUX_ID aux_id = WN_aux(wn_iter);
      unsigned long def_cnt = Get_def_cnt(aux_id);
      unsigned long new_cnt = def_cnt + 1;

      if (def_cnt == 0)
	_def_cnt_map->Add_map((POINTER) aux_id, (POINTER) new_cnt);
      else {
	MAP_LIST * map_lst = _def_cnt_map->Find_map_list((POINTER) aux_id);
	map_lst->Set_val((POINTER) new_cnt);
      }
    }
  }
}

// Hash _def_cnt_map for all statements in the given sc.
void
PRO_LOOP_INTERCHANGE_TRANS::Hash_def_cnt_map(SC_NODE * sc)
{
  if (_def_cnt_map == NULL)
    _def_cnt_map = CXX_NEW(MAP(CFG_BB_TAB_SIZE, _pool), _pool);

  BB_NODE * bb = sc->Get_bb_rep();

  if (bb != NULL)
    Hash_def_cnt_map(bb);

  BB_LIST * bb_list = sc->Get_bbs();

  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;

    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      Hash_def_cnt_map(tmp);
    }
  }

  SC_LIST * kids = sc->Kids();

  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE * tmp;

    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      Hash_def_cnt_map(tmp);
    }
  }
}

// Get ST * if wn is a scalar load/store.
ST *
CFG_TRANS::Get_st(WN * wn)
{
  OPERATOR opr = WN_operator(wn);

  if (OPERATOR_is_scalar_load(opr) 
      || OPERATOR_is_scalar_store(opr)) {
    AUX_ID aux_id = WN_aux(wn);
    if (aux_id) {
      OPT_STAB * opt_stab = _cu->Opt_stab();
      return opt_stab->Aux_stab_entry(aux_id)->St();
    }
  }

  return NULL;
}

// Collect classified loops for the SC tree rooted at sc.
void
PRO_LOOP_FUSION_TRANS::Collect_classified_loops(SC_NODE * sc)
{
  if ((sc->Type() == SC_LOOP) && (sc->Class_id() != 0)) {
    if (_loop_list == NULL)
      _loop_list = (SC_LIST *) CXX_NEW(SC_LIST(sc), _pool);
    else
      _loop_list->Append(sc, _pool);
  }

  SC_LIST_ITER sc_list_iter(sc->Kids());
  SC_NODE * tmp;
  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    Collect_classified_loops(tmp);
  }
}

// Check whether it is worthy to do proactive loop fusion transformation
// for the given SC_NODE. Currently we avoid loops of small trip counts.

#define TRIP_COUNT_THRESHOLD 1000

BOOL
PRO_LOOP_FUSION_TRANS::Is_worthy(SC_NODE * sc)
{
  FmtAssert(sc->Type() == SC_LOOP, ("Expect a SC_LOOP."));

  SC_NODE * sc_start = sc->Find_kid_of_type(SC_LP_START);
  SC_NODE * sc_end = sc->Find_kid_of_type(SC_LP_COND);
  SC_NODE * sc_step = sc->Find_kid_of_type(SC_LP_STEP);

  if (!sc_start || !sc_end || !sc_step)
    return FALSE;

  BB_NODE * bb_start = sc_start->First_bb();
  BB_NODE * bb_end = sc_end->First_bb();
  BB_NODE * bb_step = sc_step->First_bb();

  if (!bb_start || !bb_end || !bb_step)
    return FALSE;

  WN * wn_start = bb_start->Laststmt();  
  WN * wn_end = bb_end->Laststmt();
  WN * wn_step = bb_step->Laststmt();

  if (wn_step && (WN_operator(wn_step) == OPR_GOTO))
    wn_step = WN_prev(wn_step);

  if (!wn_start || !wn_end || !wn_step)
    return FALSE;

  WN * wn_lower = NULL;
  WN * wn_upper = NULL;
  WN * wn_incr = NULL;
  WN * wn_kid;
  WN * wn_kid0;
  WN * wn_kid1;
  OPERATOR opr;
  OPCODE op;

  // Find trip count lower bound.
  if (WN_operator(wn_start) == OPR_STID) 
    wn_lower = WN_kid0(wn_start);

  // Find trip count upper bound.
  if ((WN_operator(wn_end) == OPR_FALSEBR)
      || (WN_operator(wn_end) == OPR_TRUEBR)) {
    wn_kid = WN_kid0(wn_end);
    opr = WN_operator(wn_kid);
    op = WN_opcode(wn_kid);

    if (((opr == OPR_EQ) || (opr == OPR_NE) 
	 || (opr == OPR_LT) || (opr == OPR_LE)
	 || (opr == OPR_GT) || (opr == OPR_GE))
	&& MTYPE_is_integral(OPCODE_desc(op))) {
      wn_kid0 = WN_kid0(wn_kid);
      wn_kid1 = WN_kid1(wn_kid);
      wn_upper = (WN_operator(wn_kid0) == OPR_LDID) ? wn_kid1 :
	((WN_operator(wn_kid1) == OPR_LDID) ? wn_kid0 : NULL);
    }
  }
  
  // Find step.
  if (wn_step && (WN_operator(wn_step) == OPR_STID)) {
    wn_kid = WN_kid0(wn_step);
    opr = WN_operator(wn_kid);

    if ((opr == OPR_ADD) || (opr == OPR_SUB)) {
      wn_kid0 = WN_kid0(wn_kid);
      wn_kid1 = WN_kid1(wn_kid);
      wn_incr = (WN_operator(wn_kid0) == OPR_LDID) ? wn_kid1 :
	((WN_operator(wn_kid1) == OPR_LDID) ? wn_kid0 : NULL);
    }
  }
  
  if (wn_lower && wn_upper && wn_incr) {
    if ((WN_operator(wn_lower) == OPR_INTCONST)
	&& (WN_operator(wn_step) == OPR_INTCONST)
	&& (WN_operator(wn_upper) == OPR_INTCONST)) {
      int trip_count = abs(WN_const_val(wn_upper) - WN_const_val(wn_lower))
	/ abs(WN_const_val(wn_incr));
      
      if (trip_count < TRIP_COUNT_THRESHOLD)
	return FALSE;
    }
  }

  return TRUE;
}

// Find a pair of proactive loop fusion candidates for the SC_tree rooted at sc_root.
// a non-NULL sc_begin gives the initial search point in _loop_list.
// This routine can only be called from PRO_LOOP_FUSION_TRANS::Top_down_trans.
void
PRO_LOOP_FUSION_TRANS::Find_cand
(
 SC_NODE *  sc_root, 
 SC_NODE ** cand1_ptr, 
 SC_NODE ** cand2_ptr, 
 SC_NODE *  sc_begin
)
{
  SC_NODE * cand1 = NULL;
  SC_NODE * cand2 = NULL;
  SC_NODE * tmp1;
  SC_NODE * tmp2;
  SC_LIST * list1;
  SC_LIST * list2;

  // Find a pair of loops such that the pair (loop1 and loop2)
  // 1.1 Has the same loop class id.
  // 1.2 Is a single entry single exit
  // 1.3 LCP is sc_root.
  // 1.4 Not control equivalent or not adjacent to each 
  // 1.5 No SC_LOOP on the path from loop1 to LCP, and on the path from
  //    loop2 to LCP excluding loop1 and loop2.  (Future implementation
  //    can relax this rule if loop transformation is invoked in this
  //    transformation).
  // 
  // When such a pair is found, walk up SC tree to find a pair of
  // transformation candidates such that the pair (cand1 and cand2)
  // 2.1 Is kid of LCP.
  // 2.2 SC_TYPE is a SC_IF or a SC_LOOP.
  // 2.3 For all the sibling nodes between cand1 and cand2 EXCLUSIVE,
  //    - It must be a SC_BLOCK or a SC_IF.
  //    - In the case of a SC_BLOCK, it should have no dependency on loop1.
  //      It can be speculative.  It should have a single-entry and single-exit.
  //      It should also have a single successor that has a single predecessor.
  //      If cand1 is a loop, cand1 should be control equivalent to the SC_BLOCK
  //      for safe code motion.  The requirement of single successor is to make
  //      sure the successor can become a merge block for a loop after the code
  //      motion or a merge block for a SC_IF after tail duplication.
  //    - In the case of a SC_IF, there must exist at least one path that does not
  //      have dependency on both loop1 and loop2. 
  //
  // Limit is used to control code size bloat due to head/tail duplication.
  // 
  // 2.4 For all sibling nodes between cand1 and cand2 INCLUSIVE, 
  //     - If it is a SC_IF, it must be well-behaved; its head has no dependency
  //       on preceding siblings unless the preceding sibling is a SC_IF that can 
  //       be if-merged with this SC_IF and its merge has no dependency on succeeding
  //       siblings unless the succeeding SC_IF is a SC_IF that can be if-merged with
  //       this SC_IF.   This is a legality check for head/tail duplication.  There 
  //       should exist at most one non single-entry-singe-exit (SESE) SC_IFs since we
  //       don't duplicate non-SESE SC_IFs to avoid complexity.  If cand2 is a loop, 
  //       its merge block must have a single predecessor so that it can be tail-duplicated
  //       into the SC_IF.

  for (list1 = _loop_list; list1; list1 = list1->Next()) {
    tmp1 = list1->Node();

    SC_NODE * prev_sibling = tmp1->Prev_sibling();
    if (prev_sibling && (prev_sibling->Class_id() == tmp1->Class_id()))
      continue;

    if ((sc_begin != NULL) && (tmp1 != sc_begin))
      continue;

    if (!sc_root->Is_pred_in_tree(tmp1))
      continue;

    // Condition 1.2
    if (!tmp1->Is_sese())
      continue;

    // Condition 1.5
    if (tmp1->Num_of_loops(sc_root, TRUE, TRUE) != 0)
      continue;

    // heuristic to avoid unprofitable loops.
    if (!Is_worthy(tmp1))
      continue;

    // Condition 1.2
    if (tmp1->Is_sese()) {
      for (list2 = list1->Next(); list2; list2 = list2->Next()) {
	tmp2 = list2->Node();

	if (!sc_root->Is_pred_in_tree(tmp2))
	  continue;

	// Condition 1.2
	if (!tmp2->Is_sese())
	  continue;

	// Condition 1.5
	if (tmp2->Num_of_loops(sc_root, TRUE, TRUE) != 0)
	  continue;

	// Condition 1.1
	if (tmp1->Class_id() == tmp2->Class_id()) {
	  SC_NODE * lcp = tmp1->Find_lcp(tmp2);
	  
	  // Condition 1.3
	  if (lcp == sc_root) {
	    BB_NODE * bb1 = tmp1->First_bb();
	    BB_NODE * bb2 = tmp2->First_bb();

	    // Condition 1.4
	    if (!bb1->Dominates(bb2)
		|| !bb2->Postdominates(bb1)
		|| tmp1->Next_sibling() != tmp2) {
	      cand1 = tmp1;
	      cand2 = tmp2;
	      
	      // Condition 2.1
	      while (cand1->Parent() != lcp)
		cand1 = cand1->Parent();

	      while (cand2->Parent() != lcp)
		cand2 = cand2->Parent();

	      // Condition 2.2
	      if (!Is_cand_type(cand1->Type())
		  || !Is_cand_type(cand2->Type())) {
		cand1 = NULL;
		cand2 = NULL;
		continue;
	      }

	      // Condition 2.4, inclusive check.
	      SC_NODE * sc_tmp1 = cand1;
	      SC_NODE * sc_tmp2;
	      int non_sese_count = 0;

	      while (sc_tmp1) {
		if (sc_tmp1->Type() == SC_IF) {
		  if (!sc_tmp1->Is_well_behaved()) {
		    cand1 = NULL;
		    cand2 = NULL;
		    break;
		  }

		  if (!sc_tmp1->Is_sese()) {
		    non_sese_count++;
		    if (non_sese_count > 1) {
		      cand1 = NULL;
		      cand2 = NULL;
		      break;
		    }
		  }

		  // Check preceding siblings		  
		  BB_NODE * bb_head = sc_tmp1->Head();
		  sc_tmp2 = cand1;

		  while (sc_tmp2) {
		    if (sc_tmp2 == sc_tmp1)
		      break;
		    
		    if ((sc_tmp2->Type() == SC_IF)
			&& IF_MERGE_TRANS::Is_candidate(sc_tmp2, sc_tmp1, TRUE)) {
		    }
		    else if (Has_dependency(sc_tmp2, bb_head)) {
		      cand1 = NULL;
		      cand2 = NULL;
		      break;
		    }
		    sc_tmp2 = sc_tmp2->Next_sibling();
		  }
		  
		  if ((cand1 == NULL) || (cand2 == NULL))
		    break;

		  // Check succeeding siblings
		  sc_tmp2 = sc_tmp1->Next_sibling();
		  BB_NODE * bb_merge = sc_tmp1->Merge();

		  while (sc_tmp2) {
		    if ((sc_tmp2->Type() == SC_IF)
			&& IF_MERGE_TRANS::Is_candidate(sc_tmp1, sc_tmp2, TRUE)) {

		    }
		    else if (Has_dependency(sc_tmp2, bb_merge)) {
		      cand1 = NULL;
		      cand2 = NULL;
		      break;
		    }

		    if (sc_tmp2 == cand2)
		      break;
		    sc_tmp2 = sc_tmp2->Next_sibling();
		  }
		  
		  if ((cand1 == NULL) || (cand2 == NULL))
		    break;

		  if ((cand2->Type() == SC_LOOP)
		      && (cand2->Merge()->Pred()->Multiple_bbs())) {
		    cand1 = NULL;
		    cand2 = NULL;
		    break;
		  }
		}

		if (sc_tmp1 == cand2)
		  break;

		sc_tmp1 = sc_tmp1->Next_sibling();
	      }

	      if ((cand1 == NULL) || (cand2 == NULL))
		continue;

	      // Condition 2.3, exclusive check.
	      SC_NODE * next = cand1->Next_sibling();
	      int stmt_count = 0;  
	      int orig_stmt_count = 0;
	      
	      while (next != cand2) {
		SC_TYPE next_type = next->Type();
		
		if (next_type == SC_BLOCK) {
		  BB_NODE * next_last_bb = next->Last_bb();
		  if (!next_last_bb->Succ()
		      || next_last_bb->Succ()->Multiple_bbs()
		      || next_last_bb->Succ()->Node()->Pred()->Multiple_bbs()
		      ) {
		    cand1 = NULL;
		    cand2 = NULL;
		    break;
		  }

		  if (!next->Is_sese()) {
		    cand1 = NULL;
		    cand2 = NULL;
		    break;
		  }

		  BB_LIST_ITER bb_list_iter;
		  BB_NODE * tmp;

		  if (cand1->Type() == SC_LOOP) {
		    BB_NODE * cand1_first_bb = cand1->First_bb();

		    FOR_ALL_ELEM(tmp, bb_list_iter,Init(next->Get_bbs())) {
		      if (!cand1_first_bb->Dominates(tmp)
			  || !tmp->Postdominates(cand1_first_bb)) {
			cand1 = NULL;
			cand2 = NULL;
			break;
		      }
		    }
		    if (cand1 == NULL)
		      break;
		  }
		  
		  // No dependency on loop1.
		  if (Has_dependency(tmp1, next)) {
		    cand1 = NULL;
		    cand2 = NULL;
		    break;
		  }

		  // Can be speculative
		  if (!Can_be_speculative(next)) {
		    cand1 = NULL;
		    cand2 = NULL;
		    break;
		  }
		}
		else if (next_type == SC_IF) {
		  int i;
		  BOOL find_path = FALSE;
		  for (i = 0; i <= 1; i++) {
		    SC_NODE * sc_tmp = (i == 0) ? next->First_kid() : next->Last_kid();
		    if (!Has_dependency(sc_tmp, tmp1)
			&& !Has_dependency(sc_tmp, tmp2)) {
		      find_path = TRUE;
		      break;
		    }
		  }

		  if (!find_path) {
		    cand1 = NULL;
		    cand2 = NULL;
		    break;
		  }
		  
		  // Control code size bloat due to head/tail duplication.
		  if ((WOPT_Tail_Dup_Max_Clone > 0)
		      && ((_code_bloat_count + stmt_count - orig_stmt_count)
			  >= WOPT_Tail_Dup_Max_Clone)) {
		    cand1 = NULL;
		    cand2 = NULL;
		    break;
		  }

		  // Head/tail duplication doubles statement count.
		  stmt_count *= 2;
		}
		else {
		  // Must be a SC_BLOCK or a SC_IF
		  cand1 = NULL;
		  cand2 = NULL;
		  break;
		}
		 
		int cur_count = next->Executable_stmt_count();
		orig_stmt_count += cur_count;
		stmt_count += cur_count;
		next = next->Next_sibling();
	      }
		
	      if (cand1 && cand2) 
		break;
	    }
	  }
	}
      }

      if (cand1 && cand2)
	break;
    }
  }

  *cand1_ptr = cand1;
  *cand2_ptr = cand2;
}

void
CFG_TRANS::Clear()
{
  _cu = NULL;
  _trace = FALSE;
  _dump = FALSE;
  _transform_count = 0;
  _pool = NULL;
  _code_bloat_count = 0;
  _val_map = NULL;
  _true_val = NULL;
  _invar_map = NULL;
  _low_map = -1;
  _high_map = -1;
  _def_wn_map = NULL;
  _const_wn_map = NULL;
}


// Get COMP_UNIT *
COMP_UNIT * 
CFG_TRANS::Get_cu()
{
  return _cu;
}

// Move BB_NODEs in sc2 before the first BB_NODE in sc1.
// The caller of this routine should be responsible for the legality check.
void
CFG_TRANS::Do_code_motion(SC_NODE * sc1, SC_NODE * sc2)
{
  FmtAssert((sc1->Parent() == sc2->Parent()), ("Expect sibling SC_NODEs"));
  BB_NODE * first_bb1 = sc1->First_bb();
  BB_NODE * first_bb2 = sc2->First_bb();
  BB_NODE * last_bb2 = sc2->Last_bb();
  CFG * cfg = _cu->Cfg();
  SC_NODE * sc1_prev = sc1->Prev_sibling();
  SC_NODE * sc2_prev = sc2->Prev_sibling();

  // Other kinds of loops not tested yet.
  if (sc1->Type() == SC_LOOP)
    FmtAssert((sc1->Loopinfo()->Is_flag_set(LOOP_PRE_DO)), ("TODO: other loops"));    

  if (_trace) {
    printf("\n\t\t Code-motion (SC%d,SC%d)\n", 
	   sc1->Id(), sc2->Id());
  }

  BB_LIST_ITER bb_list_iter;
  BB_NODE * tmp;

  // last_bb2_succ will become new loop merge
  FmtAssert((last_bb2->Succ()->Len() == 1), ("Expect single successor"));
  BB_NODE * last_bb2_succ = last_bb2->Succ()->Node();

  FmtAssert((first_bb2->Pred()->Len() == 1), ("Expect single predecessor"));
  BB_NODE * first_bb2_pred = first_bb2->Pred()->Node();

  // Fix pred/succ
  FOR_ALL_ELEM(tmp, bb_list_iter, Init(first_bb1->Pred())) {
    if (sc1->Type() == SC_LOOP)
      FmtAssert(!sc1->Contains(tmp), ("TODO: back edge"));
    tmp->Replace_succ(first_bb1, first_bb2);

    if (cfg->Feedback()) 
      cfg->Feedback()->Move_edge_dest(tmp->Id(), first_bb1->Id(), first_bb2->Id());
  }
  
  last_bb2_succ->Replace_pred(last_bb2, first_bb2_pred);
  first_bb2_pred->Replace_succ(first_bb2, last_bb2_succ);

  if (cfg->Feedback())
    cfg->Feedback()->Move_edge_dest(first_bb2_pred->Id(), first_bb2->Id(), last_bb2_succ->Id());

  first_bb2->Set_pred(first_bb1->Pred());
  last_bb2->Replace_succ(last_bb2_succ, first_bb1);

  if (cfg->Feedback())
    cfg->Feedback()->Move_edge_dest(last_bb2->Id(), last_bb2_succ->Id(), first_bb1->Id());

  BB_LIST * new_pred = CXX_NEW(BB_LIST(last_bb2), cfg->Mem_pool());
  first_bb1->Set_pred(new_pred);

  // Fix up prev/next 
  BB_NODE * first_bb1_prev = first_bb1->Prev();
  BB_NODE * first_bb2_prev = first_bb2->Prev();
  BB_NODE * last_bb2_next = last_bb2->Next();

  first_bb1_prev->Set_next(first_bb2);
  first_bb2->Set_prev(first_bb1_prev);
  last_bb2->Set_next(first_bb1);
  first_bb1->Set_prev(last_bb2);
  
  first_bb2_prev->Set_next(last_bb2_next);
  last_bb2_next->Set_prev(first_bb2_prev);

  if (last_bb2_succ->Prev() == last_bb2)
    last_bb2_succ->Set_prev(first_bb1_prev);

  SC_NODE * parent = sc1->Parent();
  
  if (sc1->Type() == SC_LOOP) {
    // Update loop info.
    BB_NODE * merge = sc1->Merge();
    FmtAssert(merge == first_bb2, ("Unexpected merge block"));
    sc1->Loopinfo()->Set_merge(last_bb2_succ);

    // fix label on loop exit.
    cfg->Add_label_with_wn(last_bb2_succ);
    BB_NODE * bb_exit = sc1->Exit();
    WN * branch_wn = bb_exit->Branch_wn();
    FmtAssert(WN_label_number(branch_wn), ("Null label"));
    WN_label_number(branch_wn) = last_bb2_succ->Labnam();
  }

  // Fix label on branch to sc1.
  if (first_bb1->Labnam() != 0) {
    cfg->Add_label_with_wn(first_bb2);
      
    FOR_ALL_ELEM(tmp, bb_list_iter, Init(first_bb2->Pred())) {
      FmtAssert(!sc1->Contains(tmp), ("TODO: back edge"));

      if (tmp->Is_branch_to(first_bb1)) {
	WN * branch_wn = tmp->Branch_wn();	  
	WN_label_number(branch_wn) = first_bb2->Labnam();
	  
	if (parent->Type() != SC_IF) {
	  // If first_bb1 has a label WN and first_bb2 does not have
	  // a label WN, create one for first_bb2.	    
	  WN * wn_label = first_bb1->Firststmt();
	  if (wn_label && (WN_operator(wn_label) == OPR_LABEL)) {
	    wn_label = first_bb2->Firststmt();
	    if (!wn_label || (WN_operator(wn_label) != OPR_LABEL)) {
	      wn_label = WN_CreateLabel(0, first_bb2->Labnam(), 0, NULL);
	      cfg->Prepend_wn_in(first_bb2, wn_label);
	    }
	  }
	}
      }
    }
  }

  // swap sc1 and sc2 in their parent's kids.
  SC_LIST * cur_list;

  for (cur_list = parent->Kids(); cur_list; cur_list = cur_list->Next()) {
    SC_NODE * cur_node = cur_list->Node();
    if (cur_node == sc1)
      cur_list->Set_node(sc2);
    else if (cur_node == sc2)
      cur_list->Set_node(sc1);
  }

  // Fix previous sibling's merge info 
  SC_NODE * prev_sibling = sc2->Prev_sibling();

  if (prev_sibling) {
    BB_NODE * merge = prev_sibling->Merge();
    if (merge) {
      FmtAssert((merge == first_bb1), ("Unexpected merge block"));
      prev_sibling->Set_merge(first_bb2);
    }
  }

  // Fix parent info.
  Fix_parent_info(sc1, sc2);

  if (sc1_prev)
    cfg->Fix_info(sc1_prev);
  
  if (sc2_prev)
    cfg->Fix_info(sc2_prev);

  cfg->Invalidate_and_update_aux_info(FALSE);
  cfg->Invalidate_loops();
  Inc_transform_count();
}

// Insert a single-entry-single-exit region defined by (src_entry, src_exit) between
// dst_begin and dst_end.
// Before insertion, src_entry should have no predecessor, src_exit should have at most
// one successor. Also fix Prev/Next links.
void
CFG_TRANS::Insert_region
(
 BB_NODE * src_entry, 
 BB_NODE * src_exit, 
 BB_NODE * dst_begin, 
 BB_NODE * dst_end,
 MEM_POOL * pool
)
{
  FmtAssert(!src_entry->Pred(), ("Expect no predecessor"));
  FmtAssert((!src_exit->Succ() || (src_exit->Succ()->Len() == 1)), ("Expect unique successor"));

  dst_begin->Replace_succ(dst_end, src_entry);
  dst_end->Replace_pred(dst_begin, src_exit);
  src_entry->Append_pred(dst_begin, pool);

  // Next() is normally the last successor. If src_exit already has a successor, 
  // the successor must be in the same region and is the Next(). Therefore
  // we use prepend here.
  src_exit->Prepend_succ(dst_end, pool);

  BB_NODE * src_last = NULL;
  BB_NODE * bb_tmp = src_entry;
  while (bb_tmp) {
    src_last = bb_tmp;
    bb_tmp = bb_tmp->Next();
  }

  FmtAssert(src_last, ("Last BB_NODE not found"));

  // Insert into prev/next links.
  if (dst_end->Pred()->Len() == 1) {
    BB_NODE * old_prev = dst_end->Prev();
    old_prev->Set_next(src_entry);
    src_entry->Set_prev(old_prev);
    dst_end->Set_prev(src_last);
    src_last->Set_next(dst_end);
  }
  else {
    BB_NODE * old_next = dst_begin->Next();
    dst_begin->Set_next(src_entry);
    src_entry->Set_prev(dst_begin);
    src_last->Set_next(old_next);
    old_next->Set_prev(src_last);
  }
}

// Fix parent ifinfo and loop info after sc1 is head-duplicated into sc2
// or after sc2 is moved above sc1.  
void
CFG_TRANS::Fix_parent_info(SC_NODE * sc1, SC_NODE * sc2)
{
  SC_NODE * parent = sc2->Parent();
  SC_TYPE parent_type = parent->Type();

  if ((parent_type == SC_THEN) || (parent_type == SC_ELSE)) {
    BB_NODE * bb_cond = parent->Parent()->Head();
    BB_IFINFO * ifinfo = bb_cond->Ifinfo();
    
    if (parent_type == SC_THEN)
      ifinfo->Set_then(parent->First_bb());
    else
      ifinfo->Set_else(parent->First_bb());
  }
  else if (parent_type == SC_LP_BODY) {
    BB_LOOP * loop = parent->Parent()->Loopinfo();
    if (loop->Body() == sc1->First_bb()) {
      loop->Set_body(sc2->First_bb());
    }
  }
}

// Do head duplication of sc_src into sc_dst.
// Caller of this routine should take the responsiblity of legality check.
void
CFG_TRANS::Do_head_duplication(SC_NODE * sc_src, SC_NODE * sc_dst)
{
  FmtAssert((sc_dst->Type() == SC_IF), ("Expect a SC_IF"));
  FmtAssert(sc_src->Is_sese(), ("Expect a single entry single exit"));

  // Other kinds of loop not tested yet.
  if (sc_src->Type() == SC_LOOP)
    FmtAssert((sc_src->Loopinfo()->Is_flag_set(LOOP_PRE_DO)), ("TODO: Test code motion"));  

  if (_trace) {
    printf("\n\t\t Head-duplication (SC%d,SC%d)\n", 
	   sc_src->Id(), sc_dst->Id());
  }

  SC_NODE * sc_prev = sc_src->Prev_sibling();
  BB_NODE * dst_merge = sc_dst->Merge();
  BB_NODE * dst_head = sc_dst->Head();
  BB_NODE * dst_else = sc_dst->Else();
  BB_NODE * dst_then = sc_dst->Then();
  BB_IFINFO * ifinfo;
  CFG * cfg = _cu->Cfg();
  float scale = 0.0;
  BB_LIST * bb_list;
  FB_FREQ freq;
  IDTYPE edge;

  if (cfg->Feedback()) {
    freq = cfg->Feedback()->Get_edge_freq(dst_head->Id(), dst_else->Id()) 
      / cfg->Feedback()->Get_node_freq_out(dst_head->Id()) * 1.0;
    if (freq.Known())
      scale = freq.Value();
  }

  SC_NODE * prev_sibling = sc_src->Prev_sibling();

  SC_NODE * sc_new = cfg->Clone_sc(sc_src, TRUE, scale);
  FmtAssert(sc_new, ("NULL clone"));

  // Fix CFG.

  BB_NODE * new_entry;
  BB_NODE * new_exit;
  BB_NODE * old_entry;
  BB_NODE * old_exit;

  MEM_POOL * pool = cfg->Mem_pool();
  BB_LOOP * loopinfo;
  SC_NODE * sc_insert_before;
  BB_LIST_ITER bb_list_iter;
  BB_NODE * tmp;
  BB_NODE * tmp2;
  BB_NODE * src_merge;
  WN * branch_wn;
  SC_NODE * sc_then;
  SC_NODE * sc_else;
  SC_NODE * sc_tmp;
  
  switch (sc_new->Type()) {
  case SC_LOOP:
    old_entry = sc_src->Head();
    src_merge = sc_src->Merge();
    old_exit = sc_src->Exit();
    new_entry = cfg->Get_cloned_bb(old_entry);
    new_exit = cfg->Get_cloned_bb(old_exit);

    // Fix label on if-branch.
    if (dst_head->Is_branch_to(dst_else)) {
      branch_wn = dst_head->Branch_wn();
      cfg->Add_label_with_wn(new_entry);
      WN_label_number(branch_wn) = new_entry->Labnam();
    }

    // Insert BB_NODEs into else-path.
    Insert_region(new_entry, new_exit, dst_head, dst_else, pool);

    if (cfg->Feedback()) {
      FB_EDGE_TYPE edge_type = cfg->Feedback()->Get_edge_type(old_exit->Id(), src_merge->Id());
      cfg->Feedback()->Move_edge_dest(dst_head->Id(), dst_else->Id(), new_entry->Id());
      cfg->Feedback()->Add_edge(new_exit->Id(), dst_else->Id(), 
				edge_type,
				cfg->Feedback()->Get_edge_freq(dst_head->Id(), new_entry->Id()));
    }
    
    // Prepend sc_new to SC_ELSE's kids.
    sc_insert_before = sc_dst->Find_kid_of_type(SC_ELSE);
    sc_insert_before->Prepend_kid(sc_new);
    sc_new->Set_parent(sc_insert_before);

    // Add merge to loopinfo.
    loopinfo = sc_new->Loopinfo();
    loopinfo->Set_merge(dst_else);

    // Fix label on loop exit
    cfg->Add_label_with_wn(dst_else);
    branch_wn = new_exit->Branch_wn();
    FmtAssert(WN_label_number(branch_wn), ("NULL label"));
    WN_label_number(branch_wn) = dst_else->Labnam();

    // Update sc_dst's ifinfo
    ifinfo = dst_head->Ifinfo();
    ifinfo->Set_else(new_entry);

    // Disconnect BB_NODEs in src_src from CFG and then insert it into then-path.
    FOR_ALL_ELEM(tmp, bb_list_iter, Init(old_entry->Pred())) {
      tmp->Replace_succ(old_entry, src_merge);

      if (tmp->Is_branch_to(old_entry)) {
	branch_wn = tmp->Branch_wn();
	FmtAssert(src_merge->Labnam(), ("Expect a non-NULL label"));
	WN_label_number(branch_wn) = src_merge->Labnam();
      }

      if (cfg->Feedback()) 
	cfg->Feedback()->Move_edge_dest(tmp->Id(), old_entry->Id(), src_merge->Id());
    }
    src_merge->Remove_pred(old_exit, pool);
    src_merge->Set_pred(old_entry->Pred());
    old_entry->Set_pred(NULL);
    old_exit->Remove_succ(src_merge, pool);

    if (cfg->Feedback()) {
       cfg->Feedback()->Move_edge_dest(dst_head->Id(), dst_then->Id(), old_entry->Id());
       cfg->Feedback()->Move_edge_dest(old_exit->Id(), src_merge->Id(), dst_then->Id());
       edge = cfg->Feedback()->Get_edge(old_exit->Id(), dst_then->Id());
       freq = cfg->Feedback()->Get_edge_freq(dst_head->Id(), old_entry->Id());
       cfg->Feedback()->Change_edge_freq(edge, freq);
     }

    tmp = src_merge->Prev();
    tmp->Set_next(NULL);
    tmp = old_entry->Prev();
    tmp->Set_next(src_merge);
    src_merge->Set_prev(tmp);
    old_entry->Set_prev(NULL);

    // Fix label on if-branch.
    if (dst_head->Is_branch_to(dst_then)) {
      branch_wn = dst_head->Branch_wn();
      cfg->Add_label_with_wn(old_entry);
      WN_label_number(branch_wn) = old_entry->Labnam();
    }

    Insert_region(old_entry, old_exit, dst_head, dst_then, pool);

    // Unlink sc_src from SC tree and prepend it to SC_THEN's kids.
    sc_src->Unlink();
    sc_insert_before = sc_dst->Find_kid_of_type(SC_THEN);
    sc_insert_before->Prepend_kid(sc_src);
    sc_src->Set_parent(sc_insert_before);

    // Update loopinfo merge
    loopinfo = sc_src->Loopinfo();
    loopinfo->Set_merge(dst_then);

    // Fix label on loop exit.
    cfg->Add_label_with_wn(dst_then);
    branch_wn = old_exit->Branch_wn();
    FmtAssert(WN_label_number(branch_wn), ("NULL label"));
    WN_label_number(branch_wn) = dst_then->Labnam();
    
    // Update sc_dst's ifinfo
    ifinfo = dst_head->Ifinfo();
    ifinfo->Set_then(old_entry);
    
    Fix_parent_info(sc_src, sc_dst);
    break;
  case SC_BLOCK:
    old_entry = sc_src->First_bb();
    old_exit = sc_src->Last_bb();
    new_entry = cfg->Get_cloned_bb(old_entry);
    new_exit = cfg->Get_cloned_bb(old_exit);

    FmtAssert((old_exit->Succ()->Len() == 1), ("Expect single successor"));
    tmp2 = old_exit->Nth_succ(0);

    FOR_ALL_ELEM(tmp, bb_list_iter, Init(old_entry->Pred())) {
      tmp->Replace_succ(old_entry, tmp2);

      if (tmp->Is_branch_to(old_entry)) {
	branch_wn = tmp->Branch_wn();
	cfg->Add_label_with_wn(dst_head);
	WN_label_number(branch_wn) = dst_head->Labnam();
      }

      if (cfg->Feedback()) 
	cfg->Feedback()->Move_edge_dest(tmp->Id(), old_entry->Id(), tmp2->Id());
    }

    if (cfg->Feedback())
      cfg->Feedback()->Delete_edge(old_exit->Id(), tmp2->Id());

    bb_list = tmp2->Pred();
    while (bb_list)
      bb_list = bb_list->Remove(bb_list->Node(), pool);

    tmp2->Set_pred(old_entry->Pred());
    
    bb_list = old_exit->Succ();
    while (bb_list)
      bb_list = bb_list->Remove(bb_list->Node(), pool);

    tmp = old_entry->Prev();
    tmp2 = old_exit->Next();
    tmp->Set_next(tmp2);
    tmp2->Set_prev(tmp);
    
    old_entry->Set_pred(NULL);
    old_exit->Set_succ(NULL);
    old_entry->Set_prev(NULL);
    old_exit->Set_next(NULL);

    if (cfg->Feedback()) {
      freq = cfg->Feedback()->Get_edge_freq(dst_head->Id(), dst_then->Id());
      cfg->Feedback()->Move_edge_dest(dst_head->Id(), dst_then->Id(), old_entry->Id());
      cfg->Feedback()->Add_edge(old_exit->Id(), dst_then->Id(), FB_EDGE_OUTGOING, freq);
      freq = cfg->Feedback()->Get_edge_freq(dst_head->Id(), dst_else->Id());
      cfg->Feedback()->Move_edge_dest(dst_head->Id(), dst_else->Id(), new_entry->Id());
      cfg->Feedback()->Add_edge(new_exit->Id(), dst_else->Id(), FB_EDGE_OUTGOING, freq);
    }

    if (dst_head->Is_branch_to(dst_else)) {
      branch_wn = dst_head->Branch_wn();
      cfg->Add_label_with_wn(new_entry);
      WN_label_number(branch_wn) = new_entry->Labnam();
    }

    Insert_region(old_entry, old_exit, dst_head, dst_then, pool);
    Insert_region(new_entry, new_exit, dst_head, dst_else, pool);
    
    sc_src->Unlink();

    sc_then = sc_dst->Find_kid_of_type(SC_THEN);
    sc_tmp = sc_then->First_kid();

    if (sc_tmp->Type() == SC_BLOCK) {
      bb_list = sc_tmp->Get_bbs();
      FOR_ALL_ELEM(tmp, bb_list_iter, Init(bb_list)) {
	sc_src->Append_bbs(tmp);
      }
      sc_tmp->Set_bbs(sc_src->Get_bbs());
      sc_src->Set_bbs(NULL);
    }
    else {
      sc_then->Prepend_kid(sc_src);
      sc_src->Set_parent(sc_then);
    }

    sc_else = sc_dst->Find_kid_of_type(SC_ELSE);
    sc_tmp = sc_else->First_kid();

    if (sc_tmp->Type() == SC_BLOCK) {
      bb_list = sc_tmp->Get_bbs();
      FOR_ALL_ELEM(tmp, bb_list_iter, Init(bb_list)) {
	sc_new->Append_bbs(tmp);
      }
      sc_tmp->Set_bbs(sc_new->Get_bbs());
      sc_new->Set_bbs(NULL);
      sc_new->Delete();
    }
    else {
      sc_else->Prepend_kid(sc_new);
      sc_new->Set_parent(sc_else);
    }

    ifinfo = dst_head->Ifinfo();
    ifinfo->Set_then(old_entry);
    ifinfo->Set_else(new_entry);

    cfg->Fix_info(sc_dst->Get_real_parent());
    break;
  case SC_IF:
    FmtAssert(FALSE,("TODO"));
    break;
  default:
    FmtAssert(FALSE, ("Unexpected SC type"));
  }

  if (prev_sibling) {
    BB_NODE * merge = prev_sibling->Merge();
    if (merge) {
      FmtAssert((merge == sc_src->First_bb()), ("Unexpected merge block"));
      prev_sibling->Set_merge(dst_head);
    }
  }

  if (sc_prev)
    cfg->Fix_info(sc_prev);
  
  cfg->Invalidate_and_update_aux_info(FALSE);
  cfg->Invalidate_loops();

  _code_bloat_count += sc_src->Executable_stmt_count();
  Inc_transform_count();
}

// Do tail duplication of sc_src into sc_dst.
// Caller of this routine should take the responsiblity of legality check.

void
CFG_TRANS::Do_tail_duplication(SC_NODE * sc_src, SC_NODE * sc_dst)
{
  FmtAssert((sc_dst->Type() == SC_IF), ("Expect a SC_IF"));
  FmtAssert(sc_src->Is_sese(), ("Expect a single entry single exit"));

  // Other kinds of loops not tested yet.
  if (sc_src->Type() == SC_LOOP)
    FmtAssert((sc_src->Loopinfo()->Is_flag_set(LOOP_PRE_DO)), ("TODO: test other loops"));

  if (_trace) {
    printf("\n\t\t Tail-duplication (SC%d,SC%d)\n", 
	   sc_src->Id(), sc_dst->Id());
  }
  
  BB_NODE * dst_head = sc_dst->Head();
  BB_NODE * dst_merge = sc_dst->Merge();
  BB_NODE * dst_else = sc_dst->Else();
  BB_NODE * dst_else_end = sc_dst->Else_end();
  BB_NODE * dst_then_end = sc_dst->Then_end();
  CFG * cfg = _cu->Cfg();
  float scale = 0.0;
  FB_FREQ      then_edge_freq;
  FB_FREQ      else_edge_freq;
  FB_EDGE_TYPE exit_edge_type;

  if (cfg->Feedback()) {
    FB_FREQ freq = cfg->Feedback()->Get_edge_freq(dst_head->Id(), dst_else->Id()) 
      / cfg->Feedback()->Get_node_freq_out(dst_head->Id()) * 1.0;
    if (freq.Known())
      scale = freq.Value();
    then_edge_freq = cfg->Feedback()->Get_edge_freq(dst_then_end->Id(), dst_merge->Id());
    else_edge_freq = cfg->Feedback()->Get_edge_freq(dst_else_end->Id(), dst_merge->Id());

  }

  SC_NODE * sc_new = cfg->Clone_sc(sc_src, TRUE, scale);
  FmtAssert(sc_new, ("NULL clone"));

  // Fix CFG
  
  BB_NODE * new_entry;
  BB_NODE * new_exit;
  BB_NODE * old_entry;
  BB_NODE * old_exit;

  MEM_POOL * pool = cfg->Mem_pool();
  BB_LOOP * loopinfo;
  BB_IFINFO * ifinfo;
  SC_NODE * sc_insert_after;
  BB_LIST_ITER bb_list_iter;
  BB_NODE * tmp;
  BB_NODE * tmp2;
  BB_NODE * src_merge;
  BB_NODE * new_merge;
  BB_LIST * bb_list;
  MAP_LIST * map_lst;
  SC_NODE * sc_blk;
  WN * branch_wn;

  switch (sc_new->Type()) {
  case SC_LOOP:
    old_entry = sc_src->Head();
    src_merge = sc_src->Merge();
    old_exit = sc_src->Exit();
    new_entry = cfg->Get_cloned_bb(old_entry);
    new_exit = cfg->Get_cloned_bb(old_exit);

    FmtAssert(!src_merge->Pred()->Multiple_bbs(), ("Expect single predecessor"));

    new_merge = cfg->Create_and_allocate_bb(BB_GOTO);

    if (cfg->Feedback()) {
      cfg->Feedback()->Add_node(new_merge->Id());
      exit_edge_type = cfg->Feedback()->Get_edge_type(old_exit->Id(), src_merge->Id());
    }

    // Disconnect BB_NODEs in src_src from CFG
    FOR_ALL_ELEM(tmp, bb_list_iter, Init(old_entry->Pred())) {
      tmp->Replace_succ(old_entry, src_merge);
      if (cfg->Feedback() && (old_entry != dst_merge))
	cfg->Feedback()->Move_edge_dest(tmp->Id(), old_entry->Id(), src_merge->Id());

      if (tmp->Is_branch_to(old_entry)) 
	FmtAssert(FALSE, ("TODO: fix label"));
    }
    
    if (cfg->Feedback()) {
      if (old_entry == dst_merge)
	cfg->Feedback()->Move_edge_dest(dst_else_end->Id(), dst_merge->Id(), new_entry->Id());
      cfg->Feedback()->Move_edge_dest(old_exit->Id(), src_merge->Id(), new_merge->Id());
      cfg->Feedback()->Add_edge(new_merge->Id(), src_merge->Id(), FB_EDGE_OUTGOING, then_edge_freq);

      if (!cfg->Feedback()->Edge_has_freq(dst_then_end->Id(), old_entry->Id()))
	cfg->Feedback()->Add_edge(dst_then_end->Id(), old_entry->Id(), 
				  FB_EDGE_OUTGOING, then_edge_freq);
    }

    old_exit->Replace_succ(src_merge, new_merge);
    new_merge->Append_pred(old_exit, pool);
    src_merge->Remove_pred(old_exit, pool);
    src_merge->Set_pred(old_entry->Pred());
    old_entry->Set_pred(NULL);

    // Fix ifinfo
    ifinfo = dst_head->Ifinfo();
    ifinfo->Set_merge(src_merge);

    // Fix label on loop exit
    cfg->Add_label_with_wn(new_merge);
    branch_wn = old_exit->Branch_wn();
    FmtAssert(WN_label_number(branch_wn), ("NULL label"));
    WN_label_number(branch_wn) = new_merge->Labnam();

    // Fix loop info
    loopinfo = sc_src->Loopinfo();
    loopinfo->Set_merge(new_merge);

    tmp = src_merge->Prev();
    tmp->Set_next(new_merge);
    new_merge->Set_prev(tmp);
    tmp = old_entry->Prev();
    tmp->Set_next(src_merge);
    src_merge->Set_prev(tmp);
    old_entry->Set_prev(NULL);
    
    dst_merge = src_merge;

    Insert_region(old_entry, new_merge, dst_then_end, dst_merge, pool);
      
    // UNlink sc_src from SC tree and append it to SC_THEN's kids.
    sc_src->Unlink();
    sc_insert_after = sc_dst->Find_kid_of_type(SC_THEN);
    sc_insert_after->Append_kid(sc_src);
    sc_src->Set_parent(sc_insert_after);

    sc_blk = sc_src->Prev_sibling();
    if (sc_blk)
      cfg->Fix_info(sc_blk);
    
    sc_blk = cfg->Create_sc(SC_BLOCK);
    sc_blk->Append_bbs(new_merge);
    sc_insert_after->Append_kid(sc_blk);
    sc_blk->Set_parent(sc_insert_after);
    
    // Insert new BB_NODEs into else-path
    new_merge = cfg->Create_and_allocate_bb(BB_GOTO);
    
    if (cfg->Feedback()) {
      cfg->Feedback()->Add_node(new_merge->Id());
      cfg->Feedback()->Add_edge(new_exit->Id(), new_merge->Id(), exit_edge_type, else_edge_freq);
      cfg->Feedback()->Add_edge(new_merge->Id(), dst_merge->Id(), FB_EDGE_OUTGOING, else_edge_freq);
      if (!cfg->Feedback()->Edge_has_freq(dst_else_end->Id(), new_entry->Id()))
	cfg->Feedback()->Add_edge(dst_else_end->Id(), new_entry->Id(),
				  FB_EDGE_OUTGOING, else_edge_freq);
    }

    new_exit->Prepend_succ(new_merge, pool);
    new_merge->Append_pred(new_exit, pool);
    
    tmp = new_entry;
    while (tmp) {
      tmp2 = tmp;
      tmp = tmp->Next();
    }

    tmp2->Set_next(new_merge);
    new_merge->Set_prev(tmp2);

    Insert_region(new_entry, new_merge, dst_else_end, dst_merge, pool);

    // Append src_new to SC_ELSE's kids
    sc_insert_after = sc_dst->Find_kid_of_type(SC_ELSE);
    sc_insert_after->Append_kid(sc_new);
    sc_new->Set_parent(sc_insert_after);

    sc_blk = sc_new->Prev_sibling();

    if (sc_blk)
      cfg->Fix_info(sc_blk);

    sc_blk = cfg->Create_sc(SC_BLOCK);
    sc_blk->Append_bbs(new_merge);
    sc_insert_after->Append_kid(sc_blk);
    sc_blk->Set_parent(sc_insert_after);
    
    // Add merge to loopinfo
    loopinfo = sc_new->Loopinfo();
    loopinfo->Set_merge(new_merge);

    // Fix label on loop exit
    cfg->Add_label_with_wn(new_merge);    
    branch_wn = new_exit->Branch_wn();
    FmtAssert(WN_label_number(branch_wn), ("NULL label"));
    WN_label_number(branch_wn) = new_merge->Labnam();

    break;
  case SC_BLOCK:
    old_entry = sc_src->First_bb();
    old_exit = sc_src->Last_bb();
    new_entry = sc_new->First_bb();
    new_exit = sc_new->Last_bb();

    // Disconnect BB_NODEs in sc_src from CFG.
    FmtAssert(!old_exit->Succ()->Multiple_bbs(), ("Expect singe successor"));
    tmp2 = old_exit->Succ()->Node();

    FOR_ALL_ELEM(tmp, bb_list_iter, Init(old_entry->Pred())) {
      tmp->Replace_succ(old_entry, tmp2);
      if (cfg->Feedback() && (old_entry != dst_merge)) 
	cfg->Feedback()->Move_edge_dest(tmp->Id(), old_entry->Id(), tmp2->Id());
    }

    if (cfg->Feedback())
      cfg->Feedback()->Delete_edge(old_exit->Id(), tmp2->Id());
    
    bb_list = tmp2->Pred();
    while (bb_list) {
      bb_list = bb_list->Remove(bb_list->Node(), pool);
    }
    tmp2->Set_pred(old_entry->Pred());
    old_entry->Set_pred(NULL);

    bb_list = old_exit->Succ();
    while (bb_list) {
      bb_list = bb_list->Remove(bb_list->Node(), pool);
    }

    old_exit->Set_succ(NULL);
    
    tmp = old_entry->Prev();
    tmp->Set_next(tmp2);
    tmp2->Set_prev(tmp);
    old_entry->Set_prev(NULL);
    old_exit->Set_next(NULL);

    if (cfg->Feedback()) {
      cfg->Feedback()->Move_edge_dest(dst_else_end->Id(), dst_merge->Id(), new_entry->Id());
      cfg->Feedback()->Move_edge_dest(dst_then_end->Id(), dst_merge->Id(), old_entry->Id());
    }
    
    if (dst_merge == old_entry) {
      ifinfo = dst_head->Ifinfo();
      ifinfo->Set_merge(tmp2);
      dst_merge = tmp2;
    }

    if (cfg->Feedback()) {
      cfg->Feedback()->Add_edge(new_exit->Id(), dst_merge->Id(),
				FB_EDGE_OUTGOING,
				cfg->Feedback()->Get_edge_freq(dst_else_end->Id(), new_entry->Id()));
      cfg->Feedback()->Add_edge(old_exit->Id(), dst_merge->Id(),
				FB_EDGE_OUTGOING,
				cfg->Feedback()->Get_edge_freq(dst_then_end->Id(), old_entry->Id()));
    }
    
    // Insert BB_NODEs into else-path
    Insert_region(new_entry, new_exit, dst_else_end, dst_merge, pool);
    
    // Append sc_new to SC_ELSE's kids
    sc_insert_after = sc_dst->Find_kid_of_type(SC_ELSE);
    sc_blk = sc_insert_after->Last_kid();

    if (sc_blk->Type() == SC_BLOCK) {
      bb_list = sc_new->Get_bbs();
      FOR_ALL_ELEM(tmp, bb_list_iter, Init(bb_list)) {
	sc_blk->Append_bbs(tmp);
      }
    }
    else {
      sc_insert_after->Append_kid(sc_new);
      sc_new->Set_parent(sc_insert_after);

      if (sc_blk)
	cfg->Fix_info(sc_blk);
    }

    // insert it into then-path.
    Insert_region(old_entry, old_exit, dst_then_end, dst_merge,pool);

    // Unlink src_src from SC tree and append it to SC_THEN's kids.
    sc_src->Unlink();
    sc_insert_after = sc_dst->Find_kid_of_type(SC_THEN);
    sc_blk = sc_insert_after->Last_kid();

    if (sc_blk->Type() == SC_BLOCK) {
      bb_list = sc_src->Get_bbs();
      FOR_ALL_ELEM(tmp, bb_list_iter, Init(bb_list)) {
	sc_blk->Append_bbs(tmp);
      }
    }
    else {
      sc_insert_after->Append_kid(sc_src);
      sc_src->Set_parent(sc_insert_after);

      if (sc_blk)
	cfg->Fix_info(sc_blk);
    }
    break;

  default:
    FmtAssert(FALSE, ("Unexpected SC type"));
  }
  
  cfg->Invalidate_and_update_aux_info(FALSE);
  cfg->Invalidate_loops();
  _code_bloat_count += sc_src->Executable_stmt_count();
  Inc_transform_count();
}

// Traverse siblings between sc1 and sc2, do code motion or head/tail duplication
// to bring sc1 and sc2 adjacent to each other. Return TRUE if all transformations
// during the traversal are successful.
//
// This routine can only be called from PRO_LOOP_FUSION_TRANS::Top_down_trans.
BOOL
PRO_LOOP_FUSION_TRANS::Traverse_trans(SC_NODE * sc1, SC_NODE * sc2)
{
  FmtAssert((sc1->Parent() == sc2->Parent()), ("Expect siblings"));
  SC_NODE * sc = sc1;
  BOOL ret_val = TRUE;

  if (_trace)
    printf("\n\t Traverse (SC%d,SC%d)\n", 
	   sc1->Id(), sc2->Id());

  while (sc != sc2) {
    SC_NODE * next = sc->Next_sibling();
    SC_TYPE sc_type = sc->Type();
    SC_TYPE next_type = next->Type();

    FmtAssert(((sc_type == SC_IF) || (sc_type == SC_LOOP)),
	      ("Unexpect SC type"));
    
    switch (next_type) {
    case SC_BLOCK:
      if (sc_type == SC_LOOP) {
	if (sc->Loopinfo()->Is_flag_set(LOOP_PRE_DO)) 
	  Do_code_motion(sc, next);
	else {
	  ret_val = FALSE;
	  if (_trace)
	    printf("\n\t\t  Skip non-DO-LOOP (SC%d)\n", sc->Id());
	}
      }
      else if (sc_type == SC_IF) 
	Do_tail_duplication(next, sc);
      else 
	FmtAssert(FALSE, ("Unexpect SC type"));

      break;
    case SC_IF:
      if (sc_type == SC_IF) {
	if (IF_MERGE_TRANS::Is_candidate(sc, next, TRUE))
	  Do_merge(sc, next);
	else {
	  // FmtAssert(FALSE, ("TODO"));
	  ret_val = FALSE;
	}
      }
      else if (sc_type == SC_LOOP) {
	if (sc->Loopinfo()->Is_flag_set(LOOP_PRE_DO)) {
	  // Do head duplication.
	  Do_head_duplication(sc, next);
	  sc = next;
	}
	else {
	  ret_val = FALSE;
	  if (_trace)
	    printf("\n\t\t  Skip non-DO-LOOP (SC%d)\n", sc->Id());
	}
      }

      break;
    case SC_LOOP:
      if (sc_type == SC_IF) {
	if (next->Loopinfo()->Is_flag_set(LOOP_PRE_DO)) {
	  Do_tail_duplication(next, sc);
	  // exhause transformation opportunities on current loop classifications.
	  Nonrecursive_trans(sc->Find_kid_of_type(SC_THEN), FALSE);
	  Nonrecursive_trans(sc->Find_kid_of_type(SC_ELSE), FALSE);
	}
	else {
	  ret_val = FALSE;
	  if (_trace)
	    printf("\n\t\t  Skip non-DO-LOOP (SC%d)\n", next->Id());
	}
      }
      break;
    default:
      FmtAssert(FALSE, ("Unexpect SC type"));
    }

    if ((next == sc2) || (ret_val == FALSE))
      break;
  }

  return ret_val;
}

// Query whether the traverse transformation between the given pair
// should be delayed.
BOOL
PRO_LOOP_FUSION_TRANS::Is_delayed(SC_NODE * sc1, SC_NODE * sc2)
{
  BOOL ret_val = FALSE;

  // Case 1. Both sc1 and sc2 are SC_LOOPs, and all sibling nodes between them
  // are SC_BLOCKs.  In this scenario, we should search further for same-scenario
  // candidates and do traverse transformation on those candidates before sc1
  // and sc2 are processed.  Imposing such a delay is to reduce state transitions
  // of traverse transformation.

  if ((sc1->Type() == SC_LOOP) && (sc2->Type() == SC_LOOP)
      && (sc1->Parent() == sc2->Parent())) {
    ret_val = TRUE;
    SC_NODE * next_sibling = sc1->Next_sibling();

    while (next_sibling && (next_sibling != sc2)) {
      if (next_sibling->Type() != SC_BLOCK) {
	ret_val = FALSE;
	break;
      }
      next_sibling = next_sibling->Next_sibling();
    }
  }

  return ret_val;
}

// Do non-recursive tail-duplication transformation for candidates whose lcp is sc_root.
void
PRO_LOOP_FUSION_TRANS::Nonrecursive_trans(SC_NODE * sc_root, BOOL do_find) 
{
  if (do_find) {
    _loop_list = NULL;
    Collect_classified_loops(sc_root);
  }

  while (1) {
    if ((WOPT_Enable_Pro_Loop_Limit >= 0)
	&& (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
      break;
      
    SC_NODE * cand1 = NULL;
    SC_NODE * cand2 = NULL;
    Find_cand(sc_root, &cand1, &cand2, NULL);

    if (cand1 && cand2) {
      SC_NODE * tmp1 = cand1;
      SC_NODE * tmp2 = cand2;
      SC_NODE * last1 = NULL;
      SC_NODE * last2 = NULL;

      while (tmp1 && tmp2 && Is_delayed(tmp1, tmp2)) {
	last1 = tmp1;
	last2 = tmp2;
	Find_cand(sc_root, &tmp1, &tmp2, tmp2);
      }
      
      if (last1 && last2) {
	cand1 = last1;
	cand2 = last2;
      }
    }

    if (cand1 && cand2) {
      if (!Traverse_trans(cand1, cand2)) 
	break;
    }
    else
      break;
  }

  if (do_find) {
    while (_loop_list) {
      SC_NODE * tmp = _loop_list->Node();
      _loop_list = _loop_list->Remove(tmp, _pool);
    }
  }
}

// Top down do proactive loop fusion transformation for the SC tree rooted at the given sc_root.
void
PRO_LOOP_FUSION_TRANS::Top_down_trans(SC_NODE * sc_root)
{
  if (sc_root->Has_flag(HAS_SYMM_LOOP)) {
    int orig_transform_count = _transform_count;
    Nonrecursive_trans(sc_root, TRUE);

    if (_transform_count > orig_transform_count) {
      IF_MERGE_TRANS::Top_down_trans(sc_root);
      Classify_loops(sc_root);
    }
  }

  SC_LIST_ITER sc_list_iter;
  SC_NODE * kid;
  FOR_ALL_ELEM(kid, sc_list_iter, Init(sc_root->Kids())) {
    Top_down_trans(kid);
  }
}

// Reset/clear fields
void
PRO_LOOP_FUSION_TRANS::Clear()
{
  IF_MERGE_TRANS::Clear();
  _last_class_id = 0;
  _loop_depth_to_loop_map = NULL;
  _loop_list = NULL;
  _edit_loop_class = FALSE;
}

// Reset related loop classification fields for the SC tree rooted at sc.
// Rebuild map of SC tree depth to a list of SC_LOOP nodes.
// The routine can only be invoked by PRO_LOOP_FUSION_TRANS::Classify_loops.

void
PRO_LOOP_FUSION_TRANS::Reset_loop_class(SC_NODE * sc, int cur_depth)
{
  FmtAssert(_edit_loop_class, ("Not in edit mode"));

  sc->Set_class_id(0);
  sc->Set_depth(cur_depth);
  sc->Remove_flag(HAS_SYMM_LOOP);

  if (sc->Type() == SC_LOOP) {
    SC_LIST * sc_list = (SC_LIST *) _loop_depth_to_loop_map->Get_val((POINTER) cur_depth);

    if (!sc_list) {
      sc_list = (SC_LIST *) CXX_NEW(SC_LIST(sc), _pool);
      _loop_depth_to_loop_map->Add_map((POINTER) cur_depth, (POINTER) sc_list);
    }
  
    sc_list = sc_list->Append(sc, _pool);
  }

  SC_LIST_ITER sc_list_iter(sc->Kids());
  SC_NODE *tmp = NULL;

  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    Reset_loop_class(tmp, cur_depth+1);
  }
}

// Mark SC_LOOPs with symmetric paths the same class id.
// The routine can only be invoked by PRO_LOOP_FUSION_TRANS::Classify_loops.

void
PRO_LOOP_FUSION_TRANS::Find_loop_class(SC_NODE * sc)
{
  FmtAssert(_edit_loop_class, ("Not in edit mode"));

  if ((sc->Type() == SC_LOOP) && (sc->Class_id() == 0)) {
    SC_LIST * sc_list = (SC_LIST *) _loop_depth_to_loop_map->Get_val((POINTER) sc->Depth());
    SC_LIST_ITER sc_list_iter(sc_list);
    SC_NODE *tmp = NULL;
    int new_id = New_class_id();
    sc->Set_class_id(new_id);

    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      if ((tmp->Class_id() != 0) ||  (tmp == sc))
	continue;

      if (sc->Has_symmetric_path(tmp, FALSE)) {
	tmp->Set_class_id(sc->Class_id());
	SC_NODE * lcp = sc->Find_lcp(tmp);
	lcp->Add_flag(HAS_SYMM_LOOP);
      }
    }
  }
  
  SC_LIST_ITER sc_list_iter(sc->Kids());
  SC_NODE * tmp;
  FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
    Find_loop_class(tmp);
  }
}

// Classify loops for the SC tree rooted at sc.
void
PRO_LOOP_FUSION_TRANS::Classify_loops(SC_NODE *sc)
{
  _edit_loop_class = TRUE;
  OPT_POOL_Push(_pool, MEM_DUMP_FLAG + 1);
  _loop_depth_to_loop_map = CXX_NEW(MAP(CFG_BB_TAB_SIZE, _pool), _pool);
  Reset_loop_class(sc, 0);
  Find_loop_class(sc);
  CXX_DELETE(_loop_depth_to_loop_map, _pool);
  _loop_depth_to_loop_map = NULL;
  OPT_POOL_Pop(_pool, MEM_DUMP_FLAG + 1);
  _edit_loop_class = FALSE;
}

// Reset/clear fields
void
PRO_LOOP_INTERCHANGE_TRANS::Clear()
{
  IF_MERGE_TRANS::Clear();
  _pool = NULL;
  _outer_stack = NULL;
  _inner_stack = NULL;
  _local_stack = NULL;
  _restart_stack = NULL;
  _tmp_stack = NULL;
  _action = DO_INTERCHANGE_NONE;
  _def_map = NULL;
  _unlink_sc = NULL;
  _def_cnt_map = NULL;
}

// Query whether given SC_LOOP has a perfect loop nest, i.e.,
// - has only one child loop.
// - The child loop is the first kid.
// - All siblings of the child loop are empty blocks.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Is_perfect_loop_nest(SC_NODE * sc)
{
  if (sc->Type() == SC_LOOP) {
    SC_NODE * sc_tmp = sc->Find_kid_of_type(SC_LP_BODY);
    SC_NODE * inner_loop = sc_tmp->First_kid();

    if (inner_loop->Type() != SC_LOOP)
      return FALSE;

    sc_tmp = inner_loop->Next_sibling();
    while (sc_tmp) {
      if (!sc_tmp->Is_empty_block())
	return FALSE;
      sc_tmp = sc_tmp->Next_sibling();
    }

    return TRUE;
  }

  return FALSE;
}

// Given sc_inner of type SC_LOOP, where the sc_inner is the first loop among
// its siblings, find its buddy SC_LOOP from loops given in sc_stack that
// satisfies:
// - The LCP of sc_inner and the buddy is a SC_IF.
// - sc_inner and its buddy are on symmetric pathes. (see SC_NODE::Has_symmetric_path)
//
// Return the buddy if exists.
//
// Proactive loop fusion can be performed in lock-steps for sc_inner and its buddy.
// See PRO_LOOP_INTERCHANGE_TRANS::Do_lock_step_fusion.
SC_NODE *
PRO_LOOP_INTERCHANGE_TRANS::Find_fusion_buddy(SC_NODE * sc_inner, STACK<SC_NODE *> * sc_stack)
{
  if (sc_inner->Parent() == NULL)
    return NULL;

  if (sc_inner->Prev_sibling() && (sc_inner->Prev_sibling()->Type() == SC_LOOP))
    return NULL;

  if (sc_stack) {
    for (int i = 0; i < sc_stack->Elements(); i++) {
      SC_NODE * sc_cur = sc_stack->Top_nth(i);

      if (sc_cur->Prev_sibling() && (sc_cur->Prev_sibling()->Type() == SC_LOOP))
	continue;
      
      if (sc_cur != sc_inner) {
	SC_NODE * lcp = sc_inner->Find_lcp(sc_cur);
	if (lcp && (lcp->Type() == SC_IF) 
	    && sc_inner->Has_symmetric_path(sc_cur, TRUE)) {
	  return sc_cur;
	}
      }
    }
  }
  return NULL;
}

// Driver for synchronized lock-step loop fusion for a pair of buddy loops.
// See Find_fusion_buddy for definition of buddy loops.
void
PRO_LOOP_INTERCHANGE_TRANS::Do_lock_step_fusion(SC_NODE * sc1, SC_NODE * sc2)
{
  SC_NODE * sc_lcp = sc1->Find_lcp(sc2);

  if (!Is_invariant(sc1, sc_lcp->Head(), 0)
      || !Is_invariant(sc2, sc_lcp->Head(), 0))
    return;

  if (_trace)
    printf("\n\t Lock-step fusion (SC%d,SC%d)\n", sc1->Id(), sc2->Id());

  Set_region_id(sc_lcp->Id());
  
  _action = DO_INTERCHANGE_NONE;
  _action |= DO_TREE_HEIGHT_RED;

  // Do canonicalization and tree height reduction in lock steps.
  Do_canon(sc_lcp, sc1, HEAD_DUP | TAIL_DUP);
  Nonrecursive_trans(sc_lcp, sc1);
  Do_canon(sc_lcp, sc2, HEAD_DUP | TAIL_DUP);
  Nonrecursive_trans(sc_lcp, sc2);
  
  SC_NODE * sc_tmp1 = sc1->Get_nesting_if(sc_lcp);
  SC_NODE * sc_tmp2 = sc2->Get_nesting_if(sc_lcp);

  // Do if-condition distribution and if-merging in lock steps.
  while (sc_tmp1 && sc_tmp2) {
    if (!Do_if_cond_dist(sc_lcp))
      break;

    if (sc_tmp1->Next_sibling_of_type(SC_IF) == sc_tmp2) {
      Do_merge(sc_tmp1, sc_tmp2);
      IF_MERGE_TRANS::Top_down_trans(sc_tmp1);
    }
    else if (sc_tmp2->Next_sibling_of_type(SC_IF) == sc_tmp1) {
      Do_merge(sc_tmp2, sc_tmp1);
      IF_MERGE_TRANS::Top_down_trans(sc_tmp2);
    }
    else
      break;

    sc_lcp = sc1->Find_lcp(sc2);
    Do_canon(sc_lcp, sc1, HEAD_DUP | TAIL_DUP);
    Do_canon(sc_lcp, sc2, HEAD_DUP | TAIL_DUP);
    sc_tmp1 = sc1->Get_nesting_if(sc_lcp);
    sc_tmp2 = sc2->Get_nesting_if(sc_lcp);

    // Do reversed loop unswitching in lock steps.
    if (!sc_tmp1 && !sc_tmp2) {
      if (!Do_reverse_loop_unswitching(sc_lcp, sc2, NULL))
	break;

      if (!Do_reverse_loop_unswitching(sc_lcp, sc1, NULL))
	break;
    }
  }

  Set_region_id(0);
}

// Check whether the given loop nest is interchangable assuming it is a perfect loop nest.
// (See PRO_LOOP_INTERCHANGE_TRANS::Is_perfect_loop_nest)
// 
// Here we only do it for the simplest loop-nest that satisfies:
// 1. Has unique memory reference (see PRO_LOOP_INTERCHANGE_TRANS::Get_unique_ref).
// 2. The memory reference iterarates only on the inner loop's dimension.
// 3. Loop index for both the outer loop and the inner loop are a AUTO or a REG.
// 4. LP_START, LP_COND and LP_STEP only reference loop indexes or loop invariants w.r.t.
//   the outer loop.
//
// (1) and (2) guarantee that the loop-nest has a zero distance vector and therefore
// is fully permutable.
// TODO: code sharing with LNO for legality and profitability check.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Can_interchange(SC_NODE * outer_loop, SC_NODE * inner_loop)
{
  if (!outer_loop->Loopinfo()->Is_flag_set(LOOP_PRE_DO)
      || !inner_loop->Loopinfo()->Is_flag_set(LOOP_PRE_DO))
    return FALSE;

  WN * wn_tmp = NULL;
  WN * wn_load = Get_index_load(inner_loop);
  AUX_ID inner_aux_id = wn_load ? WN_aux(wn_load) : 0;

  // Check whether inner loop has unique memory reference.
  SC_NODE * sc_tmp = inner_loop->Find_kid_of_type(SC_LP_BODY);
  Get_unique_ref(sc_tmp, inner_loop, &wn_tmp);

  // Check whether the unique memory reference iterates only on the inner loop's dimension,
  // also check the inner loop and the outer loop's index, start, condition and step expressions
  // for loop interchange legality.
  if ((wn_tmp != NULL)
      && inner_aux_id
      && Is_invariant(outer_loop, wn_tmp, inner_aux_id)
      && Check_index(inner_loop)
      && Check_index(outer_loop)
      && Check_iteration(inner_loop, SC_LP_START, outer_loop) 
      && Check_iteration(inner_loop, SC_LP_COND, outer_loop) 
      && Check_iteration(inner_loop, SC_LP_STEP, outer_loop) 
      && Check_iteration(outer_loop, SC_LP_START, outer_loop)
      && Check_iteration(outer_loop, SC_LP_COND, outer_loop)
      && Check_iteration(outer_loop, SC_LP_STEP, outer_loop))
    return TRUE;

  return FALSE;
}

// Visit the SC tree rooted at the given node in a top down order
// and invoke proactive loop interchange transformation. 
// Inner loop nests are processed before outer loop nests.
// Return TRUE if there is no need to continue the iteration.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Top_down_trans(SC_NODE * sc, BOOL do_init)
{
  SC_NODE * outer_loop = NULL;
  SC_NODE * inner_loop;

  if (do_init) {
    New_invar_map();
    _local_stack = CXX_NEW(STACK<SC_NODE *>(_pool), _pool);
    _outer_stack = CXX_NEW(STACK<SC_NODE *>(_pool), _pool);
    _inner_stack = CXX_NEW(STACK<SC_NODE *>(_pool), _pool);
    _restart_stack = CXX_NEW(STACK<SC_NODE *>(_pool), _pool);
    _unlink_sc = CXX_NEW(STACK<SC_NODE *>(_pool), _pool); 
  }

  if (sc->Type() == SC_LOOP) {
    if (!_local_stack->Is_Empty()) {
      outer_loop = _local_stack->Top();
      _outer_stack->Push(outer_loop);
      _inner_stack->Push(sc);
    }
    _local_stack->Push(sc);
  }

  SC_NODE * child;
  SC_LIST_ITER sc_iter;

  FOR_ALL_ELEM(child, sc_iter, Init(sc->Kids())) {
    if (Top_down_trans(child, FALSE))
      return FALSE;
  }

  BOOL do_restart = FALSE;

  if (sc->Type() == SC_LOOP) {
    FmtAssert(((_local_stack->Top() == sc)), ("Unmatched SC_LOOP stack"));
    outer_loop = sc;
    int i;

    _local_stack->Pop();

    STACK<SC_NODE *> * sc_stack = NULL;
    STACK<SC_NODE *> * buddy_stack = CXX_NEW(STACK<SC_NODE *>(_pool), _pool);
    int size = 0;
    
    while (!_outer_stack->Is_Empty() && (_outer_stack->Top() == sc)) {
      _outer_stack->Pop();
      inner_loop = _inner_stack->Pop();

      // Do canonicalization.
      Do_canon(outer_loop, inner_loop, SPLIT_IF_HEAD);
      
      // Find candidate loop nests.
      if (Is_candidate(outer_loop, inner_loop)) {
	// Find loop fusion buddies and do lock-step buddy fusion first.
	SC_NODE * sc_buddy = Find_fusion_buddy(inner_loop, buddy_stack);
	
	if (sc_buddy) 
	  Do_lock_step_fusion(inner_loop, sc_buddy);
	else
	  buddy_stack->Push(inner_loop);

	if (!sc_stack) 
	  sc_stack = CXX_NEW(STACK<SC_NODE *>(_pool), _pool);	  
	sc_stack->Push(inner_loop);
	size++;
      }
    }

    // Destruct buddy_stack.
    while (!buddy_stack->Is_Empty()) 
      buddy_stack->Pop();

    CXX_DELETE(buddy_stack, _pool);
    buddy_stack = NULL;

    SC_NODE * sc_p = outer_loop->Parent();

    // Pop restart points that are not in the same predecessor sub-tree.
    while (!_restart_stack->Is_Empty()) {
      SC_NODE * sc_tmp = _restart_stack->Top();
      if (!sc_tmp->Is_pred_in_tree(outer_loop))
	_restart_stack->Pop();
      else
	break;
    }

    if (size > 0) {
      _action = DO_INTERCHANGE_NONE;
      _action |= DO_TREE_HEIGHT_RED;
      _action |= DO_IF_COND_DIS;     
      _action |= DO_LOOP_UNS;
      _action |= DO_REV_LOOP_UNS;

      for (i = 0; i < size; i++) {
	inner_loop = sc_stack->Top_nth(i);

	if (inner_loop->Parent() == NULL)
	  continue;

	// Do proactive loop interchange transformations.
	int ret_val = Nonrecursive_trans(outer_loop, inner_loop);

	// If loop unswitching happens, reiterate on the outer_loop's parent.
	if ((ret_val & DO_LOOP_UNS) != 0) {
	  do_restart = TRUE;
	  break;
	}
      }

      CXX_DELETE(sc_stack, _pool);

      if (do_restart) {
	// Save restart point
	_restart_stack->Push(sc_p);
	Top_down_trans(sc_p, FALSE);
      }
    }
    else if (!_restart_stack->Is_Empty()) {
      SC_NODE * sc_bk = _restart_stack->Top();
      // Do loop distribution, loop fusion and loop interchange on restarting iterations
      // (Hence we restrict LNO transformations to loop nests processed by proactive
      // loop interchange automatons).
      // 
      // Loop distribution and loop fusion can enable loop interchange. Loop interchange 
      // can expose more opportunities for proactive loop fusion.  We try to solve 
      // phase-ordering problem here by selectively doing some LNO transformations on 
      // simplest loop nests.
      // TODO: code sharing with LNO.
      if (sc_bk->Is_pred_in_tree(outer_loop)
	  && outer_loop->Loopinfo()->Is_flag_set(LOOP_PRE_DO)) {
	SC_NODE * sc_tmp = outer_loop->Find_kid_of_type(SC_LP_BODY);
	SC_NODE * inner_loop = sc_tmp->Find_kid_of_type(SC_LOOP);

	if (inner_loop && inner_loop->Loopinfo()->Is_flag_set(LOOP_PRE_DO)) {
	  // Do loop fusion 
	  if (Can_fuse(inner_loop)) {
	    sc_tmp = Do_loop_fusion(inner_loop);
	    if (sc_tmp)
	      IF_MERGE_TRANS::Top_down_trans(sc_tmp->Find_kid_of_type(SC_LP_BODY));
	  }

	  // Do loop distribution
	  SC_NODE * sc_new = Do_loop_dist(outer_loop, TRUE);
	  if (sc_new) 
	    outer_loop = sc_new;

	  // Do loop interchange for a perfect loop-nest.	
	  if (Is_perfect_loop_nest(outer_loop)) {
	    sc_tmp = outer_loop->Find_kid_of_type(SC_LP_BODY);
	    inner_loop = sc_tmp->Find_kid_of_type(SC_LOOP);

	    if (Can_interchange(outer_loop, inner_loop)) 
	      Do_loop_interchange(outer_loop, inner_loop);
	  }
	}
      }
    }
  }

  if (do_init) {
    Delete_invar_map();
    CXX_DELETE(_local_stack, _pool);
    _local_stack = NULL;
    CXX_DELETE(_outer_stack, _pool);
    _outer_stack = NULL;
    CXX_DELETE(_inner_stack, _pool);
    _inner_stack = NULL;
    
    while (!_restart_stack->Is_Empty())
      _restart_stack->Pop();
    CXX_DELETE(_restart_stack, _pool);
    _restart_stack = NULL;

    while (!_unlink_sc->Is_Empty()) {
      SC_NODE * sc_tmp = _unlink_sc->Pop();
      sc_tmp->Delete();
    }
    CXX_DELETE(_unlink_sc, _pool);
    _unlink_sc = NULL;
  }

  return do_restart;
}

// Remove the given block from the CFG.
void
PRO_LOOP_INTERCHANGE_TRANS::Remove_block(BB_NODE * bb)
{
  CFG * cfg = _cu->Cfg();
  BB_NODE * bb_succ = bb->Nth_succ(0);
  BB_NODE * bb_tmp;
  BB_LIST_ITER bb_list_iter;

  FmtAssert((bb->Kind() == BB_GOTO), ("Expect a BB_GOTO"));

  if (cfg->Feedback())
    cfg->Feedback()->Delete_edge(bb->Id(), bb_succ->Id());

  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb->Pred())) {
    if (bb_tmp->Is_branch_to(bb)) {
      WN * branch_wn = bb_tmp->Branch_wn();
      cfg->Add_label_with_wn(bb_succ);
      WN_label_number(branch_wn) = bb_succ->Labnam();
    }

    bb_tmp->Replace_succ(bb, bb_succ);
    
    if (cfg->Feedback())
      cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb->Id(), bb_succ->Id());
  }
  
  bb_succ->Set_pred(bb->Pred());
  bb_tmp = bb->Prev();
  bb_tmp->Set_next(bb_succ);
  bb_succ->Set_prev(bb_tmp);
}

// Remove all blocks in the given SC_BLOCK from the CFG.
void
PRO_LOOP_INTERCHANGE_TRANS::Remove_block(SC_NODE * sc)
{
  FmtAssert((sc->Type() == SC_BLOCK), ("Expect a SC_BLOCK"));
  BB_NODE * bb_tmp;
  BB_LIST_ITER bb_list_iter;
  SC_NODE * sc_prev = sc->Prev_sibling();
  SC_NODE * sc_parent = sc->Get_real_parent();
  CFG * cfg = _cu->Cfg();

  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(sc->Get_bbs())) {
    Remove_block(bb_tmp);
  }

  sc->Unlink();
  sc->Delete();

  if (sc_prev)
    cfg->Fix_info(sc_prev);

  if (sc_parent)
    cfg->Fix_info(sc_parent);
}

// Backward iterate statements in sc1, which is a single-BB-NODE SC_BLOCK,
// find those having no dependencies to sc_loop and sc_loop's
// consecutive siblings and create a new BB_NODE to host them.
SC_NODE *
PRO_LOOP_INTERCHANGE_TRANS::Split(SC_NODE * sc1, SC_NODE * sc_loop)
{
  CFG * cfg = _cu->Cfg();
  MEM_POOL * pool = cfg->Mem_pool();
  BB_LIST * bb_list = sc1->Get_bbs();    
  
  if ((sc1->Type() == SC_BLOCK)
      && (bb_list->Len() == 1)
      && (sc1->Executable_stmt_count() > 1)) {
    BB_NODE * bb = bb_list->Node();
    WN * wn_iter = bb->Laststmt();
    WN * wn_par = NULL;
    BOOL is_real = FALSE;
    
    while (wn_iter) {
      BOOL has_dep = FALSE;

      SC_NODE * sc_tmp = sc_loop;	
      while (sc_tmp) {
	if (Has_dependency(sc_tmp, wn_iter)) {
	  has_dep = TRUE;
	  break;
	}
	sc_tmp = sc_tmp->Next_sibling();
      }
      
      if (!has_dep) {
	wn_par = wn_iter;
	wn_iter = WN_prev(wn_iter);
	if (WN_is_executable(wn_par))
	  is_real = TRUE;
      }
      else
	break;
    }

    WN * wn_last = bb->Laststmt();
    WN * wn_first = bb->Firststmt();

    if (wn_par && (wn_par != wn_first) && is_real) {
      BB_NODE * bb_new = cfg->Create_and_allocate_bb(bb->Kind());
      WN * prev_stmt = WN_prev(wn_par);
      BB_NODE * bb_tmp;

      WN_next(prev_stmt) = NULL;
      WN_prev(wn_par) = NULL;
      bb->Set_laststmt(prev_stmt);
      bb_new->Set_firststmt(wn_par);
      bb_new->Set_laststmt(wn_last);

      bb_tmp = bb->Succ()->Node();
      bb_new->Set_succ(bb->Succ());
      bb_tmp->Replace_pred(bb, bb_new);

      bb_list = CXX_NEW(BB_LIST(bb_new), pool);
      bb->Set_succ(bb_list);
      
      bb_list = CXX_NEW(BB_LIST(bb), pool);
      bb_new->Set_pred(bb_list);

      if (cfg->Feedback()) {
	cfg->Feedback()->Add_node(bb_new->Id());
	cfg->Feedback()->Move_edge_dest(bb->Id(), bb_tmp->Id(), bb_new->Id());
	FB_FREQ freq = cfg->Feedback()->Get_edge_freq(bb->Id(), bb_new->Id());
	cfg->Feedback()->Add_edge(bb_new->Id(), bb_tmp->Id(), FB_EDGE_OUTGOING, freq);
      }
      
      bb_tmp = bb->Next();
      bb_new->Set_next(bb_tmp);
      bb_tmp->Set_prev(bb_new);
      bb->Set_next(bb_new);
      bb_new->Set_prev(bb);

      SC_NODE * sc_new = cfg->Create_sc(SC_BLOCK);
      sc_new->Append_bbs(bb_new);
      sc1->Insert_after(sc_new);
      cfg->Fix_info(sc1->Get_real_parent());

      return sc1;
    }
  }

  return NULL;
}

// Add map of aux_id to WN * in _def_map.
void
PRO_LOOP_INTERCHANGE_TRANS::Add_def_map(AUX_ID aux_id, WN * wn)
{
  WN * wn_tmp = (WN *) _def_map->Get_val((POINTER) aux_id);
  if (wn_tmp == NULL)
    _def_map->Add_map((POINTER) aux_id, (POINTER) wn);
  else {
    MAP_LIST * map_lst = _def_map->Find_map_list((POINTER) aux_id);
    map_lst->Set_val((POINTER) wn);
  }
}

// Do copy propagation for all loads in the wn.
void
PRO_LOOP_INTERCHANGE_TRANS::Copy_prop(WN * wn)
{
  for ( int i = 0; i < WN_kid_count(wn); i++) {
    WN * wn_kid = WN_kid(wn, i);
    OPERATOR opr = WN_operator(wn_kid);

    if (OPERATOR_is_scalar_load(opr)) {
      AUX_ID aux_id = WN_aux(wn_kid);
      if (aux_id) {
	WN * wn_val = (WN *) _def_map->Get_val((POINTER) aux_id);
	if (wn_val) {
	  WN_kid(wn, i) = WN_COPY_Tree_With_Map(wn_val); 
	  continue;
	}
      }
    }
    
    Copy_prop(wn_kid);
  }
}

// Do copy propagation for all loads in the bb.
void
PRO_LOOP_INTERCHANGE_TRANS::Copy_prop(BB_NODE * bb)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;
  
  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb->Firststmt(), bb->Laststmt())) {
    Copy_prop(wn_iter);
  }
}

// Do copy propagation for all loads in the sc.
void
PRO_LOOP_INTERCHANGE_TRANS::Copy_prop(SC_NODE * sc)
{
  BB_NODE * bb = sc->Get_bb_rep();
  
  if (bb != NULL)
    Copy_prop(bb);

  BB_LIST * bb_list = sc->Get_bbs();
  
  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;

    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      Copy_prop(tmp);
    }
  }
  
  SC_LIST * kids = sc->Kids();

  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE * tmp;

    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      Copy_prop(tmp);
    }
  }
}

// Partition sc_loop's siblings into two groups via code motion.
// Group 2 initially contains the sc_loop and its succeeding siblings.
// Group 1 contains the sc_loop's preceding siblings that do not have
// dependency on Group 2.
// Return the first element in Group 2.

SC_NODE *
PRO_LOOP_INTERCHANGE_TRANS::Do_partition(SC_NODE * sc_loop)
{
  SC_NODE * sc_begin = sc_loop;
  SC_NODE * sc1 = sc_begin->Prev_sibling();
  SC_NODE * sc2;

  while (sc1) {
    BOOL has_dep = FALSE;
    sc2 = sc_begin;

    while (sc2) {
      if (Has_dependency(sc1, sc2)) {
	has_dep = TRUE;
	break;
      }
      sc2 = sc2->Next_sibling();
    }

    SC_NODE * sc_next = sc1->Next_sibling();
    
    if (has_dep) {
      if (sc_next == sc_begin) 
	sc_begin = sc1;
      else if (!Has_dependency(sc1, sc_next)
	       && Can_be_speculative(sc_next))
	Do_code_motion(sc1, sc_next);
      else 
	sc_begin = sc1;

      sc1 = sc_begin->Prev_sibling();
    }
    else
      sc1 = sc1->Prev_sibling();
  }
  
  return sc_begin;
}

// Move all SC_BLOCKs succeeding the loop before the loop excluding the last empty block.
// Return TRUE if successful.
BOOL 
PRO_LOOP_INTERCHANGE_TRANS::Hoist_succ_blocks(SC_NODE * sc_loop)
{
  SC_NODE * sc_parent = sc_loop->Parent();
  SC_NODE * sc1 = sc_loop;
  SC_NODE * sc2 = sc1->Next_sibling();

  while (sc1 && sc2) {
    if ((sc1->Type() == SC_LOOP) && (sc2->Type() == SC_BLOCK)) {
      if (Has_dependency(sc1, sc2)
	  || !Can_be_speculative(sc2))
	return FALSE;

      if (sc2 == sc_parent->Last_kid()) {
	if (!sc2->Is_empty_block())
	  return FALSE;

	break;
      }
      else {
	Do_code_motion(sc1, sc2);

	sc1 = sc2->Prev_sibling();
	if ((sc1 == NULL) || (sc1->Type() != SC_LOOP)) {
	  sc1 = sc2;
	  sc2 = sc1->Next_sibling();
	}
      }
    }
    else {
      sc1 = sc2;
      sc2 = sc2->Next_sibling();
    }
  }

  return TRUE;
}

// Do canonicalization to prepare for loop distribution.
// - Rearrange order of sc_loop's siblings so that:
//   1. All SC_BLOCKs excluding the last SC_LOOP's merge appear before the SC_LOOPs.
//   2. All nodes having dependency to SC_LOOPs are adjacent to SC_LOOPs.
//
// - If possible, do copy propagation to remove the dependency of SC_LOOPs on SC_BLOCKS.
//
// Return the first SC_NODE * that has dependency on the SC_LOOPs.
SC_NODE * 
PRO_LOOP_INTERCHANGE_TRANS::Do_pre_dist(SC_NODE * sc_loop, SC_NODE * outer_loop)
{
  CFG * cfg = _cu->Cfg();
  SC_NODE * sc1;
  SC_NODE * sc2;
  SC_NODE * sc_parent = sc_loop->Parent();

  // All siblings of sc_loop should be a SC_LOOP or a SC_BLOCK.  
  sc1 = sc_parent->First_kid();
  while (sc1) {
    SC_TYPE sc_type = sc1->Type();
    if ((sc_type != SC_BLOCK) && (sc_type != SC_LOOP))
      return NULL;
    sc1 = sc1->Next_sibling();
  }

  if ((WOPT_Enable_Pro_Loop_Limit >= 0)
      && (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
    return NULL;

  // Iterate sc_loop's succeeding siblings, split empty SC_BLOCKS so that each of them
  // contains one BB_NODE *.
  sc1 = sc_loop->Next_sibling_of_type(SC_BLOCK);

  while (sc1 && (sc1->Type() == SC_BLOCK)) {
    if (sc1->Is_empty_block() 
	&& (sc1->Get_bbs()->Len() > 1)) {
      sc1 = cfg->Split(sc1);
      continue;
    }
    sc1 = sc1->Next_sibling();
  }

  // Move all SC_BLOCKs succeeding the loop above the loop excluding the last empty block.
  if (!Hoist_succ_blocks(sc_loop))
    return NULL;

  sc1 = sc_parent->Last_kid();
  if (!sc1->Is_empty_block()) 
    cfg->Insert_block_after(sc1);

  // Iterate sc_loop's preceding siblings, remove empty SC_BLOCKS, split
  // SC_BLOCKS so that each SC_BLOCK contains one BB_NODE *, split SC_BLOCKS
  // having single-BB-NODE to separate out statements having no dependencies to
  // SC_LOOPs.
  sc1 = sc_parent->First_kid();

  while (sc1 && (sc1 != sc_loop)) {
    sc2 = sc1->Next_sibling();

    if (sc1->Is_empty_block()) {
      Remove_block(sc1);
      sc1 = sc2;
    }
    else if (sc1->Get_bbs()->Len() > 1) 
      sc1 = cfg->Split(sc1);
    else {
      sc1 = Split(sc1, sc_loop);
      
      if (sc1 == NULL)
	sc1 = sc2;
    }
  }

  cfg->Fix_info(sc_loop->Get_real_parent());

  // Partition sc_loop's siblings into two groups, where the 2nd group contains
  // all SC_LOOPs and their dependent SC_BLOCKs.

  SC_NODE * sc_begin = Do_partition(sc_loop);

  // Check whether the dependent blocks only contain store statements that can be
  // copied progagated to its uses.
  // 1. Stores have no side effect.
  // 2. Stores's data are expressions of loop invariant or loop index
  //    w.r.t. to the outer_loop.
  // 3. All of stores' uses appear in the SC_LOOPs (This is TRUE if the store
  //    has a single-def).
  
  _def_map = CXX_NEW(MAP(CFG_BB_TAB_SIZE, _pool), _pool);
  sc1 = sc_begin;

  while (sc1 != sc_loop) {
    BB_LIST * bb_list = sc1->Get_bbs();
    BB_NODE * bb_tmp;
    BB_LIST_ITER bb_list_iter;
    
    if (bb_list != NULL) {
      STMT_ITER stmt_iter;
      WN * wn_iter;
      WN * wn_index = NULL;
      AUX_ID loop_aux_id = 0;

      if (outer_loop) {
	WN * wn_load = Get_index_load(outer_loop);
	loop_aux_id = wn_load ? WN_aux(wn_load) : 0;
      }
      
      FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_list)) {
	FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb_tmp->Firststmt(), bb_tmp->Laststmt())) {
	  OPERATOR opr = WN_operator(wn_iter);

	  if (OPERATOR_is_store(opr)) {
	    WN * wn_data = WN_kid0(wn_iter);
	    if (Has_side_effect(wn_iter)
		|| (outer_loop && !Is_invariant(outer_loop, wn_data, loop_aux_id))
		|| (Get_def_cnt(WN_aux(wn_iter)) > 1)) {
	      sc_begin = NULL;
	      break;
	    }

	    Add_def_map(WN_aux(wn_iter), wn_data);
	  }
	  else if (WN_is_executable(wn_iter)) {
	    sc_begin = NULL;
	    break;
	  }
	}
      }

      if (sc_begin == NULL)
	break;
    }

    sc1 = sc1->Next_sibling();
  }

  // Do copy propagation if possible to make sc_loop a partition point.
  if (sc_begin && (sc_begin != sc_loop)) {
    sc1 = sc_loop;

    while (sc1) {
      Copy_prop(sc1);
      sc1 = sc1->Next_sibling();
    }

    // Re-partition after copy propagation.
    sc_begin = Do_partition(sc_loop);
  }

  CXX_DELETE(_def_map, _pool);
  Inc_transform_count();

  return sc_begin;
}

// Split head of sc_if so that it contains a single statement.
void
PRO_LOOP_INTERCHANGE_TRANS::Do_split_if_head(SC_NODE * sc_if)
{
  CFG * cfg = _cu->Cfg();
  MEM_POOL * pool = cfg->Mem_pool();
  BB_NODE * bb_head = sc_if->Head();
  WN * branch_wn = bb_head->Branch_wn();
  BB_NODE * bb_new;      
  BB_NODE * bb_tmp;
  BB_LIST_ITER bb_list_iter;
  BB_IFINFO * ifinfo;
  BB_LIST * bb_list_new;
  FB_FREQ freq;
  SC_NODE * sc_new;

  if (bb_head->Executable_stmt_count() > 1) {
    WN * wn_prev = WN_prev(branch_wn);	  
    ifinfo = bb_head->Ifinfo();

    if (_trace)
      printf("\n\t\t Split if-head(SC%d)\n", sc_if->Id());

    WN_next(wn_prev) = NULL;
    WN_prev(branch_wn) = NULL;
    bb_head->Set_laststmt(wn_prev);

    bb_new = cfg->Create_and_allocate_bb(bb_head->Kind());
    bb_new->Set_firststmt(branch_wn);
    bb_new->Set_laststmt(branch_wn);

    if (cfg->Feedback())
      cfg->Feedback()->Add_node(bb_new->Id());

    FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_head->Succ())) {
      bb_tmp->Replace_pred(bb_head, bb_new);

      if (cfg->Feedback()) {
	FB_EDGE_TYPE edge_type = cfg->Feedback()->Get_edge_type(bb_head->Id(), bb_tmp->Id());
	freq = cfg->Feedback()->Get_edge_freq(bb_head->Id(), bb_tmp->Id());
	cfg->Feedback()->Delete_edge(bb_head->Id(), bb_tmp->Id());
	cfg->Feedback()->Add_edge(bb_new->Id(), bb_tmp->Id(), edge_type, freq);
      }
    }
	  
    if (cfg->Feedback()) {
      freq = cfg->Feedback()->Get_node_freq_in(bb_head->Id());
      cfg->Feedback()->Add_edge(bb_head->Id(), bb_new->Id(), FB_EDGE_OUTGOING, freq);
    }

    bb_new->Set_succ(bb_head->Succ());
    bb_list_new = CXX_NEW(BB_LIST(bb_new), pool);
    bb_head->Set_succ(bb_list_new);
    bb_list_new = CXX_NEW(BB_LIST(bb_head), pool);
    bb_new->Set_pred(bb_list_new);

    bb_tmp = bb_head->Next();
    bb_tmp->Set_prev(bb_new);
    bb_new->Set_next(bb_tmp);
    bb_new->Set_prev(bb_head);
    bb_head->Set_next(bb_new);

    ifinfo->Set_cond(bb_new);
    bb_head->Set_ifinfo(NULL);
    bb_head->Set_kind(BB_GOTO);
    bb_new->Set_ifinfo(ifinfo);

    sc_if->Set_bb_rep(bb_new);
    sc_new = cfg->Create_sc(SC_BLOCK);
    sc_new->Append_bbs(bb_head);
    sc_if->Insert_before(sc_new);
    cfg->Fix_info(sc_if->Get_real_parent());
    cfg->Invalidate_and_update_aux_info(FALSE);
    cfg->Invalidate_loops();
    Inc_transform_count();
  }
}

// Do canonicalization to produce a perfectly nested SC_IFs between the inner loop
// and the outer loop, i.e., for all SC_IFs in-between:
// 1. Head contains single statement.
// 2. Head is the 1st block of its parent.
// 3. Merge is an empty block.
void
PRO_LOOP_INTERCHANGE_TRANS::Do_canon(SC_NODE * outer_loop, SC_NODE * inner_loop, int action)
{
  CFG * cfg = _cu->Cfg();
  MEM_POOL * pool = cfg->Mem_pool();
  int i;

  if (_tmp_stack == NULL)
    _tmp_stack = CXX_NEW(STACK<SC_NODE *>(_pool), _pool);
  else {
    while (!_tmp_stack->Is_Empty())
      _tmp_stack->Pop();
  }
  
  SC_NODE * sc_cur = inner_loop->Get_real_parent();
  
  while (sc_cur && (sc_cur != outer_loop)) {
    if (sc_cur->Type() == SC_IF) 
      _tmp_stack->Push(sc_cur);
    sc_cur = sc_cur->Get_real_parent();
  }

  int size = _tmp_stack->Elements();
  
  if (size == 0)
    return;

  // Split head of SC_IF.
  if ((action & SPLIT_IF_HEAD) != 0) {
    for (i = 0; i < size; i++) {
      SC_NODE * sc_if = _tmp_stack->Top_nth(i);
      Do_split_if_head(sc_if);
    }
  }

  // Do head duplication so that head is the first block of its parent.
  
  if ((action & HEAD_DUP) != 0) {
    for (i = 0; i < size; i++) {
      SC_NODE * sc_if = _tmp_stack->Top_nth(i);
      sc_cur = sc_if->Prev_sibling();
      while (sc_cur) {
	Do_head_duplication(sc_cur, sc_if);
	sc_cur = sc_if->Prev_sibling();
      }
    }
  }

  // Do tail duplication so that the merge of a if-region is an empty block.
  if ((action & TAIL_DUP) != 0) {
    for (i = 0; i < size; i++) {
      SC_NODE * sc_if = _tmp_stack->Top_nth(i);
      SC_NODE * sc_parent = sc_if->Parent();
      SC_NODE * next_sibling;
      SC_NODE * last_sibling = sc_parent->Last_kid();
      FmtAssert((last_sibling->Type() == SC_BLOCK), ("Expect a SC_BLOCK"));
      
      if (!last_sibling->Is_empty_block()) 
	cfg->Insert_block_after(last_sibling);
      
      last_sibling = sc_parent->Last_kid();
      next_sibling = sc_if->Next_sibling();
      
      while (next_sibling && (next_sibling != last_sibling)) {
	if (!next_sibling->Is_empty_block()) 
	  Do_tail_duplication(next_sibling, sc_if);
	else 
	  Remove_block(next_sibling);
	
	next_sibling = sc_if->Next_sibling();
      }
    }
  }
}

// Query whether statements in the given block are invariants w.r.t. the given sc.
// st_index gives loop index. If st_index is non-zero, skip stores and loads w.r.t. to 
// the loop index.
BOOL
CFG_TRANS::Is_invariant(SC_NODE * sc, BB_NODE * bb, AUX_ID st_index)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;
  SC_TYPE type = sc->Type();

  // Query from hash.
  MAP * invar_map = Get_invar_map();
  SC_NODE * sc_hash;

  if (invar_map)
    sc_hash = (SC_NODE *) invar_map->Get_val((POINTER) bb->Id());

  if (sc_hash 
      && (sc_hash == sc || sc_hash->Is_pred_in_tree(sc)))
    return TRUE;

  if ((bb->Executable_stmt_count() == 1)
      && ((type == SC_THEN) || (type == SC_ELSE))) {
    WN * wn1 = bb->Laststmt();
    BB_NODE * bb_head = sc->Parent()->Head();
    
    if (bb_head->Executable_stmt_count() == 1) {
      WN * wn2 = bb_head->Laststmt();

      if (WN_opcode(wn1) == WN_opcode(wn2)) {
	wn1 = WN_kid0(wn1);
	wn2 = WN_kid0(wn2);

	if (WN_Simp_Compare_Trees(wn1, wn2) == 0) 
	  return !Maybe_assigned_expr(sc, wn1, (type == SC_THEN) ? TRUE : FALSE);
      }
    }
  }

  BB_NODE * tmp = sc->Get_bb_rep();

  if ((tmp != NULL) && (!Is_invariant(tmp, bb, st_index)))
    return FALSE;

  BB_LIST * bb_list = sc->Get_bbs();

  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);

    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      if (!Is_invariant(tmp, bb, st_index))
	return FALSE;
    }
  }
  
  SC_LIST * kids = sc->Kids();
  
  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE *tmp;
    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      if (!Is_invariant(tmp, bb, st_index))
	return FALSE;
    }
  }

  // Hash query results.
  Hash_invar(bb, sc);

  return TRUE;
}

// Hash BB_NODE Id to SC_NODE *, where bb is an invariant w.r.t. the sc.
void
CFG_TRANS::Hash_invar(BB_NODE * bb, SC_NODE * sc)
{
  MAP * invar_map = Get_invar_map();
  SC_NODE * sc_tmp = (SC_NODE *) invar_map->Get_val((POINTER) bb->Id());

  if (!sc_tmp)
    invar_map->Add_map((POINTER) bb->Id(), (POINTER) sc);
  else if ((sc_tmp->Parent() == NULL) || sc->Is_pred_in_tree(sc_tmp)) {
    MAP_LIST * map_lst = invar_map->Find_map_list((POINTER) bb->Id());    
    map_lst->Set_val((POINTER) sc);
  }
}

// Query whether statements in bb2 are invariants w.r.t. to bb1.
// st_index gives loop index. If st_index is non-zero, skip stores and loads 
// w.r.t. to the loop index.
BOOL
CFG_TRANS::Is_invariant(BB_NODE * bb1, BB_NODE * bb2, AUX_ID index)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;

  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb2->Firststmt(), bb2->Laststmt())) {
    if (!Is_invariant(bb1, wn_iter, index))
      return FALSE;
  }

  return TRUE;
}

// Query whether expressions in the WHIRL tree rooted at the wn are invariants w.r.t.
// the given bb.  st_index gives loop index. If st_index is non-zero, skip stores and loads 
// w.r.t. to the loop index.
BOOL
CFG_TRANS::Is_invariant(BB_NODE * bb, WN * wn, AUX_ID st_index)
{
  if (st_index) {
    OPERATOR opr = WN_operator(wn);
    if (OPERATOR_is_scalar_load(opr)) {
      if (WN_aux(wn) == st_index)
	return TRUE;
    }
    else if (OPERATOR_is_scalar_store(opr)) {
      if (WN_aux(wn) == st_index)
	return Is_invariant(bb, WN_kid0(wn), st_index);
    }

    int kid_count = WN_kid_count(wn);

    if (kid_count == 0)
      return !Maybe_assigned_expr(bb, wn);
    else {
      for (int i = 0; i < kid_count; i++) {
	if (!Is_invariant(bb, WN_kid(wn, i), st_index))
	  return FALSE;
      }
    }

    return TRUE;
  }
  else 
    return !Maybe_assigned_expr(bb, wn);
}

// Query whether statements in the given sc are loop invariants w.r.t. the given loop.
// st_index gives loop index. If st_index is non-zero, skip stores and loads w.r.t to the
// loop index.
BOOL
CFG_TRANS::Is_invariant(SC_NODE * loop, SC_NODE * sc, AUX_ID st_index)
{
  BB_NODE * bb = sc->Get_bb_rep();

  if ((bb != NULL) && !Is_invariant(loop, bb, st_index))
    return FALSE;
  
  BB_LIST * bb_list = sc->Get_bbs();
  
  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;

    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      if (!Is_invariant(loop, tmp, st_index))
	return FALSE;
    }
  }

  SC_LIST * kids = sc->Kids();
  
  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE *tmp;
    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      if (!Is_invariant(loop, tmp, st_index))
	return FALSE;
    }
  }

  return TRUE;
}

// Query whether loads/stores in the given wn are invariants w.r.t. the given sc.
// st_index gives loop index. If st_index is non-zero, skip stores and loads w.r.t to the
// loop index.
BOOL
CFG_TRANS::Is_invariant(SC_NODE * sc, WN * wn, AUX_ID st_index)
{
  BB_NODE * bb = sc->Get_bb_rep();

  if ((bb != NULL) && !Is_invariant(bb, wn, st_index))
    return FALSE;

  BB_LIST * bb_list = sc->Get_bbs();
  
  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;

    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      if (!Is_invariant(tmp, wn, st_index))
	return FALSE;
    }
  }
  
  SC_LIST * kids = sc->Kids();
  
  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE *tmp;
    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      if (!Is_invariant(tmp, wn, st_index))
	return FALSE;
    }
  }
  
  return TRUE;
}

// Find a loop distribution candidate among sc's children that satisfies the 
// following conditions:
// - It is the first loop among sc's children.
// - It is not the first kid of sc.
// - Every preceding sibling has no dependency on it.
// - Every succeeding sibling is either a loop fusion candidate or an empty block.
SC_NODE *
PRO_LOOP_INTERCHANGE_TRANS::Find_dist_cand(SC_NODE * sc)
{
  SC_NODE * sc_loop = sc->Find_kid_of_type(SC_LOOP);
  SC_NODE * sc_cur;

  if (sc_loop) {
    sc_cur = sc->First_kid();
    
    if (sc_cur == sc_loop)
      return NULL;

    while (sc_cur && (sc_cur != sc_loop)) {
      if (Has_dependency(sc_cur, sc_loop))
	return NULL;
      sc_cur = sc_cur->Next_sibling();
    }
    
    sc_cur = sc_loop->Next_sibling();
    while (sc_cur && (sc_cur->Type() == SC_LOOP)) {
      if (!sc_cur->Has_same_loop_struct(sc_loop))
	return NULL;

      sc_cur = sc_cur->Next_sibling();
    }

    while (sc_cur) {
      if (!sc_cur->Is_empty_block())
	return NULL;
      sc_cur = sc_cur->Next_sibling();
    }

    return sc_loop;
  }

  return NULL;
}

// Check sibling of sc on Rule 2 and 3 in PRO_LOOP_INTERCHANGE_TRANS::Is_candidate.
BOOL 
PRO_LOOP_INTERCHANGE_TRANS::Check_sibling(SC_NODE * sc, SC_NODE * inner_loop)
{
  SC_NODE * sc_parent = sc->Parent();
  SC_NODE * sc_cur = sc_parent->First_kid();
  BOOL check_pred = TRUE;

  if (sc == inner_loop) {
    SC_NODE * sc_dist = Find_dist_cand(sc_parent);
    if (sc_dist == sc) 
      return TRUE;
  }

  while (sc_cur) {
    if (sc_cur == sc) {
      if (sc_cur->Type() == SC_LOOP) {
	// skip succeeding consecutive loop fusion candidates.
	SC_NODE * next_sibling = sc_cur->Next_sibling();
	while (next_sibling && (next_sibling->Type() == SC_LOOP)
	       && sc_cur->Has_same_loop_struct(next_sibling)) {
	  next_sibling = next_sibling->Next_sibling();
	}
	sc_cur = next_sibling;
      }
      check_pred = FALSE;
    }

    if (sc_cur) {
      if (sc_cur != sc) {
	if ((sc_cur->Type() != SC_BLOCK) || !sc_cur->Is_sese())
	  return FALSE;
	
	if (check_pred && (sc->Type() == SC_IF)) {
	  // Check head duplication legality.
	  SC_NODE * sc_iter = inner_loop->Parent();
	  while (sc_iter) {
	    if ((sc_iter->Type() == SC_IF)
		&& Has_dependency(sc_cur, sc_iter->Head()))
	      return FALSE;

	    if (sc_iter == sc)
	      break;
	    sc_iter = sc_iter->Parent();
	  }
	}
      }

      sc_cur = sc_cur->Next_sibling();
    }
  }

  return TRUE;
}

// Query whether given loop-nest is a candidate for proactive loop interchange transformation.
// For every node on the path from the inner_loop to the outer loop (left inclusive).
// 1. Its type is SC_IF, SC_ELSE, SC_THEN, SC_LOOP or SC_LP_BODY. 
// 2. In the case of a SC_IF, it is a loop invariant w.r.t. either the outer_loop or the inner_loop;
//    it is well-behaved; Its head has a single statement.  If it is a loop variant w.r.t the outer
//    loop, its condition expression is re-orderable.  If there exists a preceding sibling, 
//    the sibling must be a SC_BLOCK which has no dependencies on all SC_IF nodes on the successor
//    part of the path (head duplication legality). If there exists a succeeding sibling,
//    the succeeding sibling must be a SC_BLOCK.
// 3. In the case of a SC_LOOP, it must be the inner loop itself and must also be the first loop
//    among its siblings.  It must be a loop distribution point, otherwise, for any preceding
//    sibling, it must be a SC_BLOCK.  For any succeeding siblings, if it is a loop, it must appear
//    in a consecutive sequence of loops that are loop fusion candidates w.r.t. to the inner loop; 
//    if it is not a loop, it must be a SC_BLOCK.
// 4. Both the outer loop and the inner loop are a do-loop.
// 5. The outer loop and the inner loop are interchangable if there was no intervening statement
//    between them.
//
// Note that the restriction of "SC_BLOCK" is to simplify the low level transformation machinery
// (head/tail duplication, code motion etc.).  It is not a restriction of the generic proactive
// loop interchange algorithm.
BOOL 
PRO_LOOP_INTERCHANGE_TRANS::Is_candidate(SC_NODE * outer_loop, SC_NODE * inner_loop)
{
  SC_NODE * cur_node = inner_loop;
  BOOL is_cand = TRUE;

  if (inner_loop->Parent() == NULL)
    return FALSE;

  if (inner_loop->Get_real_parent() == outer_loop)
    return FALSE;

  // Rule 5.
  if (!Can_interchange(outer_loop, inner_loop))
    return FALSE;

  // Rule 4.
  if (((outer_loop->Type() == SC_LOOP) && !outer_loop->Loopinfo()->Is_flag_set(LOOP_PRE_DO))
      || !inner_loop->Loopinfo()->Is_flag_set(LOOP_PRE_DO))
    return FALSE;

  while (cur_node && (cur_node != outer_loop)) {
    SC_TYPE type = cur_node->Type();
    SC_NODE * parent_node = cur_node->Parent();
    BB_NODE * bb;
    BOOL is_invar_outer = FALSE;
    BOOL is_invar_inner = FALSE;
    WN * wn_cond = NULL;
    
    // Rule 1.
    switch (type) {
    case SC_THEN:
    case SC_ELSE:
      break;
    case SC_LOOP:
      // Rule 3.
      if ((cur_node != inner_loop) 
	  || (parent_node->Find_kid_of_type(SC_LOOP) != cur_node)
	  || !Check_sibling(cur_node, inner_loop))
	is_cand = FALSE;
      break;

    case SC_IF:
      // Rule 2.
      bb = cur_node->Get_bb_rep();
      wn_cond = WN_kid0(cur_node->Head()->Branch_wn());

      if (Is_invariant(outer_loop, bb, 0))
	is_invar_outer = TRUE;

      if (Is_invariant(inner_loop, bb, 0))
	is_invar_inner = TRUE;
      
      if ((!is_invar_outer && !is_invar_inner)
	  || (!is_invar_outer && !Can_reorder_cond(wn_cond, NULL))
	  || !cur_node->Is_well_behaved()
	  || (bb->Executable_stmt_count() > 1)
	  || !Check_sibling(cur_node, inner_loop))
	is_cand = FALSE;

      break;
    case SC_LP_BODY:
      if (cur_node->Parent() == outer_loop)
	break;

    default:
      is_cand = FALSE;
    }

    if (!is_cand)
      break;
    
    cur_node = cur_node->Parent();
  }

  return is_cand;
}


// Given a SC_IF, obtain a copy of its condition-setting WN and
// invert its opcode if do_invert is TRUE.
WN *
PRO_LOOP_INTERCHANGE_TRANS::Get_cond(SC_NODE * sc, BOOL do_invert)
{
  if (sc->Type() == SC_IF) {
    BB_NODE * bb = sc->Get_bb_rep();
    WN * last_wn = bb->Laststmt();
    OPERATOR opr = WN_operator(last_wn);
    
    if ((opr == OPR_FALSEBR) || (opr == OPR_TRUEBR)) {
      WN * old_cond = WN_kid0(last_wn);
      OPCODE opc_inv = get_inverse_relop(WN_opcode(old_cond));
      
      if (opc_inv != OPCODE_UNKNOWN) {
	WN *new_cond = WN_COPY_Tree_With_Map(old_cond);
	if (do_invert) 
	  WN_set_opcode(new_cond, opc_inv);
	return new_cond;
      }
    }
  }

  return NULL;
}

// Merge two condition expression WNs using the given operator.
WN *
PRO_LOOP_INTERCHANGE_TRANS::Merge_cond(WN * wn1, WN * wn2, OPERATOR opr)
{
  if (wn1 == NULL)
    return wn2;
  else if (wn2 == NULL)
    return wn1;
  else {
    OPCODE op_cand = OPCODE_make_op(opr, Boolean_type, MTYPE_V);
    WN * wn = WN_CreateExp2(op_cand, wn1, wn2);
    return wn;
  }
}

// Reduce height of if-condition tree between sc1 and sc2, where sc1 is in the ancestor 
// sub-tree of sc2.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Do_if_cond_tree_height_reduction(SC_NODE * sc1, SC_NODE * sc2)
{
  SC_NODE * cur_node;
  SC_NODE * parent_node;
  WN * new_cond = NULL;
  WN * old_cond;
  CFG * cfg = _cu->Cfg();
  MEM_POOL * pool = cfg->Mem_pool();

  if ((WOPT_Enable_Pro_Loop_Limit >= 0)
      && (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
    return FALSE;

  FmtAssert(((sc1->Type() == SC_IF) && (sc2->Type() == SC_ELSE) || (sc2->Type() == SC_THEN)),
	    ("Expect a SC_ELSE or SC_THEN"));

  // Al SC_IFs in-between sc2 and sc1 (right exclusive) should have empty merge block.
  cur_node = sc2;

  while (cur_node != sc1) {
    if ((cur_node->Type() == SC_IF)
	&& (cur_node->Merge()->Executable_stmt_count() > 0))
      return FALSE;
    cur_node = cur_node->Parent();
  }

  if (_trace)
    printf("\n\t\t If-cond tree height reduction (SC%d,SC%d)\n", 
	   sc1->Id(), sc2->Id());

  cur_node = sc2;
  parent_node = cur_node->Parent();
  
  while (cur_node != sc1) {
    SC_TYPE cur_type = cur_node->Type();
    BOOL do_inverse = FALSE;

    if (cur_type == SC_ELSE) 
      do_inverse = TRUE;

    old_cond = Get_cond(parent_node, do_inverse);
    new_cond = Merge_cond(old_cond, new_cond, OPR_CAND);
    
    cur_node = parent_node;
    parent_node = cur_node->Parent();
  }

  FmtAssert(new_cond, ("NULL condition"));

  // Remove a then-path or a else-path in a CFG (last successor is fall-through).
  // BB39 is head of sc1, BB42 is head of sc2.
  // 
  //      5                      5
  //      |                      |
  //      39                     39   
  //    /    \                  /   \
  //   41    40               41    40
  //  /  \    |             /   \    |     |
  // 62  42   |            62   43   |     42
  // |  /  \  |            |    |    |     |
  // | 51  43 |  ==>       |    44   |     51
  // | |   |  |            |    |    |     |
  // | 60  44 |            |    61   |     60
  // |  \  /  |             \   /    |     |
  // |   61   |              63      |
  // \   /    |               \      /
  //   63     |                 64
  //    \    /                \  |
  //     64                     65
  //  \  |
  //    65
  SC_NODE * sc1_p = sc1->Get_real_parent();
  SC_NODE * sc2_p = sc2->Parent();  
  SC_NODE * sc1_prev = sc1->Prev_sibling();

  FmtAssert((sc2_p->Type() == SC_IF), ("Expect a SC_IF"));
  SC_NODE * sc41 = sc2_p->Get_real_parent();
  BB_NODE * bb41 = sc41->Get_bb_rep();
  BB_NODE * bb42 = sc2_p->Get_bb_rep();
  BB_NODE * bb61 = sc2_p->Merge(); 
  BB_NODE * bb63 = sc41->Merge();
  BB_NODE * bb51 = sc2->First_bb();   
  BB_NODE * bb60 = sc2->Last_bb();    
  SC_NODE * sc2_s = sc2_p->First_kid_of_type((sc2->Type() == SC_ELSE) ? SC_THEN : SC_ELSE);
  BB_NODE * bb43 = sc2_s->First_bb(); 
  BB_NODE * bb44 = sc2_s->Last_bb(); 
  BB_NODE * bb39 = sc1->Get_bb_rep();
  BB_NODE * bb64 = sc1->Merge();
  SC_NODE * sc_tmp1;
  SC_NODE * sc_tmp2;
  BB_NODE * bb_tmp;
  WN * branch_wn;
  FB_FREQ freq;
  FB_EDGE_TYPE edge_type;
  FB_EDGE_TYPE ft_edge_type;
  FB_EDGE_TYPE br_edge_type;

  // Obtain edge types of the fall-through edge and the non-fall-through edge.
  if (cfg->Feedback()) {
    ft_edge_type = cfg->Feedback()->Get_edge_type(bb42->Id(), bb42->If_then()->Id());
    br_edge_type = cfg->Feedback()->Get_edge_type(bb42->Id(), bb42->If_else()->Id());
  }

  bb41->Replace_succ(bb42, bb43);
  BB_IFINFO * info = bb41->Ifinfo();

  if (bb41->Is_branch_to(bb42)) {
    branch_wn = bb41->Branch_wn();
    cfg->Add_label_with_wn(bb43);
    WN_label_number(branch_wn) = bb43->Labnam();
    info->Set_else(bb43);
  }
  else
    info->Set_then(bb43);
  
  bb43->Replace_pred(bb42, bb41);
  bb42->Remove_succ(bb43, pool);
  bb61->Remove_pred(bb60, pool);

  bb_tmp = bb42->Prev();
  bb_tmp->Set_next(bb43);
  bb43->Set_prev(bb_tmp);
  bb44->Set_next(bb61);
  bb61->Set_prev(bb44);
  bb51->Set_prev(bb42);
  bb42->Set_next(bb51);

  IDTYPE edge;

  if (cfg->Feedback()) {
    // Invalidate edge freq for edges connected to the removed path.
    sc_tmp1 = sc2_p;
    while (sc_tmp1 != sc1) {
      sc_tmp2 = sc_tmp1->Get_real_parent();
      edge = cfg->Feedback()->Get_edge(sc_tmp2->Get_bb_rep()->Id(),
				       sc_tmp1->Get_bb_rep()->Id());
      if (edge != IDTYPE_NULL)
	cfg->Feedback()->Change_edge_freq(edge, FB_FREQ_UNKNOWN );

      edge = cfg->Feedback()->Get_edge(sc_tmp1->Merge()->Id(),
				       sc_tmp2->Merge()->Id());
      if (edge != IDTYPE_NULL)
	cfg->Feedback()->Change_edge_freq(edge, FB_FREQ_UNKNOWN );
      sc_tmp1 = sc_tmp2;
    }

    edge_type = cfg->Feedback()->Get_edge_type(bb41->Id(), bb42->Id());
    cfg->Feedback()->Delete_edge(bb41->Id(), bb42->Id());
    freq = cfg->Feedback()->Get_edge_freq(bb42->Id(), bb43->Id());
    cfg->Feedback()->Delete_edge(bb42->Id(), bb43->Id());
    cfg->Feedback()->Add_edge(bb41->Id(), bb43->Id(), edge_type, freq);
    edge = cfg->Feedback()->Get_edge(bb42->Id(), bb51->Id());
    cfg->Feedback()->Set_edge_type(edge, ft_edge_type);

    edge = cfg->Feedback()->Get_edge(bb61->Id(), bb63->Id());
    
    if (edge != IDTYPE_NULL)
      cfg->Feedback()->Change_edge_freq(edge, freq);
  }

  // Link in removed path. Replace condition expressions in BB42 with new_cond.
  // 
  //     5                           5
  //     |                           |
  //     39                         42
  //   /    \                     /     \
  //  41     40                 39       51
  // /  \    |	              /   \      |
  // 62   43 |               41   40     60
  // |    |  |              /  \   |     |
  // |    44 |  ==>       62   43  |     |
  // |    |  |            |    |   |     |
  // |   61  |            |    44  |     |
  // \    /  |            |    |   |     |
  //   63    |            |    61  |     |
  //    \   /             \    /   |     |
  //     64                 63     |     |
  // \   |                    \   /      |
  //   65                       new      |
  //                             \      /
  //                                64
  //                             \   |
  //                                65

  BB_LIST_ITER bb_list_iter;

  // All nested if-condition should have single predecessor, otherwise there must
  // exist intervening statements that would have disqualified it from being a candidate.
  FmtAssert((bb42->Pred()->Len() == 1), ("Expect single predecessor"));

  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb39->Pred())) {
    bb_tmp->Replace_succ(bb39, bb42);
    if (cfg->Feedback()) {
      cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb39->Id(), bb42->Id());
    }
  }
  
  bb42->Set_pred(bb39->Pred());
  bb42->Prepend_succ(bb39, pool);
  
  BB_LIST * bb_list_new = CXX_NEW(BB_LIST(bb42), pool);
  bb39->Set_pred(bb_list_new);
  
  BB_NODE * bb_new = cfg->Create_and_allocate_bb(bb64->Kind());

  FmtAssert(bb60->Succ()->Len() == 1, ("Expect single succ"));
  bb60->Replace_succ(bb61, bb64);

  if (cfg->Feedback()) {
    freq = cfg->Feedback()->Get_edge_freq(bb42->Id(), bb51->Id());
    cfg->Feedback()->Add_node(bb_new->Id());
    cfg->Feedback()->Add_edge(bb42->Id(), bb39->Id(), br_edge_type, FB_FREQ_UNKNOWN);
    cfg->Feedback()->Move_edge_dest(bb60->Id(), bb61->Id(), bb64->Id());
    cfg->Feedback()->Add_edge(bb_new->Id(), bb64->Id(), FB_EDGE_OUTGOING, FB_FREQ_UNKNOWN);
  }
  
  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb64->Pred())) {
    bb_tmp->Replace_succ(bb64, bb_new);
    if (cfg->Feedback()) 
      cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb64->Id(), bb_new->Id());
  }

  bb_new->Set_pred(bb64->Pred());
  bb_list_new = CXX_NEW(BB_LIST(bb64), pool);
  bb_new->Set_succ(bb_list_new);

  bb_list_new = CXX_NEW(BB_LIST(bb_new), pool);
  bb64->Set_pred(bb_list_new);
  bb64->Append_pred(bb60, pool);

  // Make bb42 jumps to bb39.
  branch_wn = bb42->Branch_wn();
  cfg->Add_label_with_wn(bb39);
  WN_label_number(branch_wn) = bb39->Labnam();
  WN_kid0(branch_wn) = new_cond;

  bb_tmp = bb39->Prev();
  bb_tmp->Set_next(bb42);
  bb42->Set_prev(bb_tmp);
  bb60->Set_next(bb39);
  bb39->Set_prev(bb60);
  bb_tmp = bb64->Prev();
  bb_new->Set_prev(bb_tmp);
  bb_tmp->Set_next(bb_new);
  bb64->Set_prev(bb_new);
  bb_new->Set_next(bb64);
  
  info = bb42->Ifinfo();
  info->Set_then(bb51);
  info->Set_else(bb39);
  info->Set_merge(bb64);

  info = bb39->Ifinfo();
  info->Set_merge(bb_new);

  // Update SC tree
  // Last successor is SC_ELSE, sc76 is sc2.
  //
  //        72                     72
  //      /    \                  /  \             73
  //     73      107             75  107         /   \
  //    /  \          =>                        74   76
  //   74  76                                       / | \
  //   |  / | \
  //  75

  SC_NODE * sc73 = sc2_p;
  SC_NODE * sc74 = sc2_s;
  SC_NODE * sc76 = sc2;
  SC_NODE * sc72 = sc73->Parent();
  
  sc72->Set_kids(sc74->Kids());
  SC_LIST_ITER sc_list_iter;
  FOR_ALL_ELEM(sc_tmp1, sc_list_iter, Init(sc72->Kids())) {
    sc_tmp1->Set_parent(sc72);
  }

  //    73              73
  //   /  \            /   \
  //  74  76     =>   76   74
  //     / | \       / | \

  if (sc76->Type() == SC_ELSE) {
    sc76->Set_type(SC_THEN);
    sc74->Set_type(SC_ELSE);
    sc74->Unlink();
    sc73->Append_kid(sc74);
    sc74->Set_parent(sc73);
  }

  // 67 is sc1
  //    9                 9
  //  /   \                \
  //      66   =>          66
  //      /  \            /  \
  //     67  111         73   NEW
  //                    /   \
  //                   76    74
  //                  /|\   /  \
  //                       67  111

  SC_NODE * sc67 = sc1;
  SC_NODE * sc66 = sc67->Parent();
  SC_NODE * sc111 = sc67->Next_sibling();
  SC_NODE * sc_new = cfg->Create_sc(SC_BLOCK);

  sc111->Set_bbs(NULL);
  sc111->Append_bbs(bb_new);

  sc_new->Append_bbs(bb64);
  sc74->Set_kids(sc66->Kids());
  FOR_ALL_ELEM(sc_tmp1, sc_list_iter, Init(sc74->Kids())) {
    sc_tmp1->Set_parent(sc74);
  }

  sc66->Set_kids(NULL);
  sc66->Append_kid(sc73);
  sc66->Append_kid(sc_new);
  sc_new->Set_parent(sc66);
  sc73->Set_parent(sc66);

  cfg->Fix_info(sc1_p);

  if (sc1_prev)
    cfg->Fix_info(sc1_prev);
  
  if (cfg->Feedback()) 
    cfg->Freq_propagate(sc66);
  
  cfg->Invalidate_and_update_aux_info(FALSE);
  cfg->Invalidate_loops();
  Inc_transform_count();

  return TRUE;
}

// Invalidate invariant maps for the given BB_NODE.
void
PRO_LOOP_INTERCHANGE_TRANS::Invalidate_invar(BB_NODE * bb)
{
  MAP * invar_map = Get_invar_map();
  MAP_LIST * map_lst = invar_map->Find_map_list((POINTER) bb->Id());    

  if (map_lst)
    map_lst->Set_val((POINTER) NULL);
}

// Invalidate invariant maps for all BB_NODEs in the given sc.
void
PRO_LOOP_INTERCHANGE_TRANS::Invalidate_invar(SC_NODE * sc)
{
  BB_NODE * bb = sc->Get_bb_rep();

  if (bb != NULL)
    Invalidate_invar(bb);

  BB_LIST * bb_list = sc->Get_bbs();

  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;

    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      Invalidate_invar(tmp);
    }
  }

  SC_LIST * kids = sc->Kids();

  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE * tmp;

    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      Invalidate_invar(tmp);
    }
  }
}

// Do loop unswitching.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Do_loop_unswitching(SC_NODE * sc1, SC_NODE * sc2)
{
  FmtAssert(((sc1->Type() == SC_IF) && (sc2->Type() == SC_LOOP)), ("Unexpect SC type"));

  CFG * cfg = _cu->Cfg();
  MEM_POOL * pool = cfg->Mem_pool();
  SC_NODE * sc_tmp = sc1->Next_sibling();
  SC_NODE * sc2_prev = sc2->Prev_sibling();

  // Only do it if the if-region and its merge are the only children
  // of the loop, and the merge is an empty block.
  if (sc1->Prev_sibling() || sc_tmp->Next_sibling()
      || !sc_tmp->Is_empty_block() || !sc2->Is_sese())
    return FALSE;

  if ((WOPT_Enable_Pro_Loop_Limit >= 0)
      && (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
    return FALSE;

  if (_trace)
    printf("\n\t\t Loop unswitching (SC%d,SC%d)\n", 
	   sc1->Id(), sc2->Id());

  // Create a new SC_IF and let it become the new merge for sc2.
  //
  //      \   /              \       /
  //     bb_merge    ==>     bb_head_new
  //      /  \                /     \
  //                        bb_e2   bb_e1
  //                          \     /
  //                          bb_merge
  //                          /     \

  BB_NODE * bb_head = sc1->Head();
  BB_NODE * bb_then = sc1->Then();
  BB_NODE * bb_else = sc1->Else();
  BB_NODE * bb_merge = sc2->Merge();
  BB_NODE * bb_head_new = NULL;
  cfg->Clone_bbs(bb_head, bb_head, &bb_head_new, &bb_head_new, TRUE, 1.0);
  BB_NODE * bb_e1 = cfg->Create_and_allocate_bb(BB_GOTO);
  BB_NODE * bb_e2 = cfg->Create_and_allocate_bb(BB_GOTO);
  SC_NODE * sc_if = cfg->Create_sc(SC_IF);
  SC_NODE * sc_then = cfg->Create_sc(SC_THEN);
  SC_NODE * sc_else = cfg->Create_sc(SC_ELSE);
  SC_NODE * sc_e1 = cfg->Create_sc(SC_BLOCK);
  SC_NODE * sc_e2 = cfg->Create_sc(SC_BLOCK);

  sc_if->Set_bb_rep(bb_head_new);
  sc_e1->Append_bbs(bb_e1);
  sc_e2->Append_bbs(bb_e2);

  BB_IFINFO * if_info = bb_head_new->Ifinfo();
  if_info->Set_merge(bb_merge);
  if_info->Set_then(bb_e1);
  if_info->Set_else(bb_e2);

  if (cfg->Feedback()) {
    cfg->Feedback()->Add_node(bb_head_new->Id());
    cfg->Feedback()->Add_node(bb_e1->Id());
    cfg->Feedback()->Add_node(bb_e2->Id());
  }

  BB_LIST_ITER bb_list_iter;
  BB_LIST * bb_list_tmp;
  BB_NODE * bb_tmp;
  WN * branch_wn;
  FB_FREQ freq;
  IDTYPE edge;
  float scale = 1.0;

  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_merge->Pred())) {
    bb_tmp->Replace_succ(bb_merge, bb_head_new);
    if (bb_tmp->Is_branch_to(bb_merge)) {
      branch_wn = bb_tmp->Branch_wn();
      cfg->Add_label_with_wn(bb_head_new);
      WN_label_number(branch_wn) = bb_head_new->Labnam();
    }

    if (cfg->Feedback()) 
      cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb_merge->Id(), bb_head_new->Id());
  }

  bb_head_new->Set_pred(bb_merge->Pred());
  bb_list_tmp = CXX_NEW(BB_LIST(bb_e2), pool);
  bb_head_new->Set_succ(bb_list_tmp);
  bb_head_new->Append_succ(bb_e1, pool);
  bb_list_tmp = CXX_NEW(BB_LIST(bb_head_new), pool);
  bb_e1->Set_pred(bb_list_tmp);
  bb_list_tmp = CXX_NEW(BB_LIST(bb_head_new), pool);
  bb_e2->Set_pred(bb_list_tmp);
  bb_list_tmp = CXX_NEW(BB_LIST(bb_merge), pool);
  bb_e1->Set_succ(bb_list_tmp);
  bb_list_tmp = CXX_NEW(BB_LIST(bb_merge), pool);
  bb_e2->Set_succ(bb_list_tmp);
  bb_list_tmp = CXX_NEW(BB_LIST(bb_e2), pool);
  bb_merge->Set_pred(bb_list_tmp);
  bb_merge->Append_pred(bb_e1, pool);
  
  if (cfg->Feedback()) {
    freq = cfg->Feedback()->Get_node_freq_out(bb_merge->Id());
    FB_FREQ freq1 = cfg->Feedback()->Get_edge_freq(bb_head->Id(), bb_then->Id());
    FB_FREQ freq2 = cfg->Feedback()->Get_edge_freq(bb_head->Id(), bb_else->Id());
    freq1 = freq1/(freq1 + freq2) * freq;
    FB_EDGE_TYPE edge_type = cfg->Feedback()->Get_edge_type(bb_head->Id(), bb_then->Id());
    cfg->Feedback()->Add_edge(bb_head_new->Id(), bb_e1->Id(), edge_type, freq1);
    cfg->Feedback()->Add_edge(bb_e1->Id(), bb_merge->Id(), FB_EDGE_OUTGOING, freq1);
    edge_type = cfg->Feedback()->Get_edge_type(bb_head->Id(), bb_else->Id());
    cfg->Feedback()->Add_edge(bb_head_new->Id(), bb_e2->Id(), edge_type, freq - freq1);
    cfg->Feedback()->Add_edge(bb_e2->Id(), bb_merge->Id(), FB_EDGE_OUTGOING, freq - freq1);
  }
  
  branch_wn = bb_head_new->Branch_wn();
  cfg->Add_label_with_wn(bb_e2);
  WN_label_number(branch_wn) = bb_e2->Labnam();
  
  bb_tmp = bb_merge->Prev();
  bb_tmp->Set_next(bb_head_new);
  bb_head_new->Set_prev(bb_tmp);
  bb_head_new->Set_next(bb_e1);
  bb_e1->Set_prev(bb_head_new);
  bb_e1->Set_next(bb_e2);
  bb_e2->Set_prev(bb_e1);
  bb_e2->Set_next(bb_merge);
  bb_merge->Set_prev(bb_e2);

  sc_then->Set_parent(sc_if);
  sc_else->Set_parent(sc_if);
  sc_if->Append_kid(sc_then);
  sc_if->Append_kid(sc_else);
  sc_e1->Set_parent(sc_then);
  sc_then->Append_kid(sc_e1);
  sc_e2->Set_parent(sc_else);
  sc_else->Append_kid(sc_e2);

  sc2->Loopinfo()->Set_merge(bb_head_new);
  sc2->Insert_after(sc_if);
  cfg->Fix_info(sc2->Get_real_parent());

  cfg->Invalidate_and_update_aux_info(FALSE);
  cfg->Invalidate_loops();

  // Do head duplication of sc2 into sc_if.

  Do_head_duplication(sc2, sc_if);
  SC_NODE * sc_body;
  SC_NODE * sc_merge;
  SC_NODE * sc_next;
  SC_NODE * sc_prune;
  SC_LIST_ITER sc_list_iter;

  for (int i = 0; i < 2; i ++) {
    sc_tmp = sc_if->Find_kid_of_type((i == 0) ? SC_THEN : SC_ELSE);
    sc_tmp = sc_tmp->First_kid();
    sc_body = sc_tmp->Find_kid_of_type(SC_LP_BODY);
    sc_prune = sc_body->First_kid();
    FmtAssert((sc_prune->Type() == SC_IF), ("Expect a SC_IF"));
    sc_merge = sc_prune->Next_sibling();
    bb_head = sc_prune->Head();
    bb_merge = sc_prune->Merge();

    BB_NODE * bb_first = (i == 0) ? sc_prune->Then() : sc_prune->Else();
    BB_NODE * bb_last = (i == 0) ? sc_prune->Then_end() : sc_prune->Else_end();

    if (cfg->Feedback()) {
      freq = cfg->Feedback()->Get_node_freq_in(bb_head->Id());
      FB_FREQ freq1 = cfg->Feedback()->Get_edge_freq(bb_head->Id(), bb_first->Id());
      scale = freq.Value() / freq1.Value();
      
      if (i == 0)
	cfg->Freq_scale(sc_prune->Find_kid_of_type(SC_THEN), scale);
      else
	cfg->Freq_scale(sc_prune->Find_kid_of_type(SC_ELSE), scale);
    }
    
    // Remove else/then path.
    sc_next = sc_prune->Find_kid_of_type((i == 0) ? SC_THEN : SC_ELSE);
    sc_prune->Unlink();
    Invalidate_invar(sc_prune);
    sc_merge->Unlink();

    sc_body->Set_kids(sc_next->Kids());
    sc_next->Set_kids(NULL);

    FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init(sc_body->Kids())) {
      sc_tmp->Set_parent(sc_body);
    }
    
    sc_body->Append_kid(sc_merge);
    sc_merge->Set_parent(sc_body);
    sc_prune->Delete();

    // Remove if-condition.
    FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_head->Succ())) {
      if (cfg->Feedback()) 
	cfg->Feedback()->Delete_edge(bb_head->Id(), bb_tmp->Id());
    }

    FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_head->Pred())) {
      bb_tmp->Replace_succ(bb_head, bb_first);

      if (bb_tmp->Is_branch_to(bb_head)) {
	branch_wn = bb_tmp->Branch_wn();
	cfg->Add_label_with_wn(bb_first);
	WN_label_number(branch_wn) = bb_first->Labnam();
      }

      if (cfg->Feedback()) 
	cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb_head->Id(), bb_first->Id());
    }
  
    bb_first->Set_pred(bb_head->Pred());
  
    FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_merge->Pred())) {
      if (bb_tmp != bb_last) {
	bb_merge->Remove_pred(bb_tmp, pool);

	if (cfg->Feedback()) 
	  cfg->Feedback()->Delete_edge(bb_tmp->Id(), bb_merge->Id());
	break;
      }
    }

    // remove branch in bb_last.
    if (bb_last->Is_branch_to(bb_merge)) 
      Delete_branch(bb_last);
    
    bb_tmp = bb_head->Prev();
    bb_tmp->Set_next(bb_first);
    bb_first->Set_prev(bb_tmp);
    bb_last->Set_next(bb_merge);
    bb_merge->Set_prev(bb_last);

    cfg->Fix_info(sc_body->Parent());
  }    

  if (sc2_prev)
    cfg->Fix_info(sc2_prev);
  
  cfg->Invalidate_and_update_aux_info(FALSE);
  cfg->Invalidate_loops();
  Inc_transform_count();

  return TRUE;
}

// Swap executable statements for the given pair of BB_NODE, keep labels unchanged.
void
PRO_LOOP_INTERCHANGE_TRANS::Swap_stmt(BB_NODE * bb1, BB_NODE * bb2)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;
  WN * wn_insert1 = NULL;
  WN * wn_insert2 = NULL;
  
  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb1->Firststmt(), bb1->Laststmt())) {
    if (WN_is_executable(wn_iter))
      break;
    else
      wn_insert1 = wn_iter;
  }

  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb2->Firststmt(), bb2->Laststmt())) {
    if (WN_is_executable(wn_iter))
      break;
    else
      wn_insert2 = wn_iter;
  }

  WN * wn_begin1 = wn_insert1 ? WN_next(wn_insert1) : bb1->Firststmt();
  WN * wn_begin2 = wn_insert2 ? WN_next(wn_insert2) : bb2->Firststmt();
  WN * wn_end1 = bb1->Laststmt();
  WN * wn_end2 = bb2->Laststmt();
  WN * wn_branch1 = bb1->Branch_wn();
  WN * wn_branch2 = bb2->Branch_wn();
  INT32 label1;
  INT32 label2;

  if (wn_branch1)
    label1 = WN_label_number(wn_branch1);

  if (wn_branch2)
    label2 = WN_label_number(wn_branch2);

  if (wn_insert1) {
    WN_next(wn_insert1) = wn_begin2;
    WN_prev(wn_begin2) = wn_insert1;
    bb1->Set_laststmt(wn_end2);
  }
  else {
    bb1->Set_firststmt(wn_begin2);
    bb1->Set_laststmt(wn_end2);
  }

  if (wn_insert2) {
    WN_next(wn_insert2) = wn_begin1;
    WN_prev(wn_begin1) = wn_insert2;
    bb2->Set_laststmt(wn_end1);
  }
  else {
    bb2->Set_firststmt(wn_begin1);
    bb2->Set_laststmt(wn_end1);
  }

  wn_branch1 = bb1->Branch_wn();
  wn_branch2 = bb2->Branch_wn();

  if (wn_branch1)
    WN_label_number(wn_branch1) = label1;
  
  if (wn_branch2)
    WN_label_number(wn_branch2) = label2;
}

// Do loop interchange of the given loop nest.  Caller of this routine should check legality.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Do_loop_interchange(SC_NODE * sc_outer, SC_NODE * sc_inner)
{
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 3; j++) {
      SC_NODE * sc_tmp1 = NULL;
      SC_NODE * sc_tmp2 = NULL;
      BB_NODE * bb_tmp1 = NULL;
      BB_NODE * bb_tmp2 = NULL;

      switch (j) {
      case 0:
	sc_tmp1 = sc_outer->Find_kid_of_type(SC_LP_START);
	sc_tmp2 = sc_inner->Find_kid_of_type(SC_LP_START);
	break;
      case 1:
	sc_tmp1 = sc_outer->Find_kid_of_type(SC_LP_COND);
	sc_tmp2 = sc_inner->Find_kid_of_type(SC_LP_COND);
	break;
      case 2:
	sc_tmp1 = sc_outer->Find_kid_of_type(SC_LP_STEP);      
	sc_tmp2 = sc_inner->Find_kid_of_type(SC_LP_STEP);
	break;
      default:
	;
      }
      
      bb_tmp1 = sc_tmp1->Last_bb();
      bb_tmp2 = sc_tmp2->Last_bb();

      if (i == 0) {
	if ((sc_tmp1->First_bb() != bb_tmp1)
	    || (sc_tmp2->First_bb() != bb_tmp2))
	  return FALSE;

	if ((bb_tmp1->Executable_stmt_count() == 0)
	    || (bb_tmp2->Executable_stmt_count() == 0))
	  return FALSE;

	WN * wn1 = bb_tmp1->Branch_wn();
	WN * wn2 = bb_tmp2->Branch_wn();

	if (wn1 && wn2) {
	  if (WN_operator(wn1) != WN_operator(wn2))
	    return FALSE;
	}
      }
      else {
	if ((WOPT_Enable_Pro_Loop_Limit >= 0)
	    && (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
	  return FALSE;
	
	if (j == 0) {
	  if (_trace)
	    printf("\n\t\t Loop interchange (SC%d,SC%d)\n", 
		   sc_outer->Id(), sc_inner->Id());
	}

	Swap_stmt(bb_tmp1, bb_tmp2);
      }
    }
  }

  CFG * cfg = _cu->Cfg();
  WN * wn_index1 = sc_outer->Index();
  WN * wn_index2 = sc_inner->Index();

  BB_LOOP * loop = sc_outer->Loopinfo();
  loop->Set_index(wn_index2);
  loop = sc_inner->Loopinfo();
  loop->Set_index(wn_index1);

  cfg->Fix_info(sc_outer);
  cfg->Fix_info(sc_inner);

  if (cfg->Feedback()) {
    SC_NODE * sc_body1 = sc_outer->Find_kid_of_type(SC_LP_BODY);
    SC_NODE * sc_body2 = sc_inner->Find_kid_of_type(SC_LP_BODY);
    BB_NODE * bb_cond1 = sc_outer->Find_kid_of_type(SC_LP_COND)->Last_bb();    
    BB_NODE * bb_body1 = sc_body1->First_bb();
    BB_NODE * bb_step1 = sc_outer->Find_kid_of_type(SC_LP_STEP)->Last_bb();    
    BB_NODE * bb_cond2 = sc_inner->Find_kid_of_type(SC_LP_COND)->Last_bb();
    BB_NODE * bb_body2 = sc_body2->First_bb();
    BB_NODE * bb_step2 = sc_inner->Find_kid_of_type(SC_LP_STEP)->Last_bb();
    FB_FREQ freq1 = cfg->Feedback()->Get_edge_freq(bb_cond1->Id(), bb_body1->Id());
    FB_FREQ freq2 = cfg->Feedback()->Get_edge_freq(bb_cond2->Id(), bb_body2->Id());
    float scale;
    IDTYPE edge;

    if ((freq1 > FB_FREQ_ZERO) && (freq2 > FB_FREQ_ZERO)) {
      scale = freq2.Value()/freq1.Value();
      cfg->Freq_scale(sc_body1, scale);
    }

    if ((freq1 > FB_FREQ_ZERO) && (freq2 > FB_FREQ_ZERO)) {
      scale = freq1.Value()/freq2.Value();
      cfg->Freq_scale(sc_body2, scale);
    }
    
    edge = cfg->Feedback()->Get_edge(bb_cond1->Id(), bb_body1->Id());
    if (edge)
      cfg->Feedback()->Change_edge_freq(edge, freq2);

    edge = cfg->Feedback()->Get_edge(bb_step1->Id(), bb_cond1->Id());
    if (edge)
      cfg->Feedback()->Change_edge_freq(edge, freq2);

    edge = cfg->Feedback()->Get_edge(bb_cond2->Id(), bb_body2->Id());
    if (edge)
      cfg->Feedback()->Change_edge_freq(edge, freq1);
    
    edge = cfg->Feedback()->Get_edge(bb_step2->Id(), bb_cond2->Id());
    if (edge)
      cfg->Feedback()->Change_edge_freq(edge, freq1);
  }

  Inc_transform_count();
  return TRUE;
}

// Do loop distributition for the given loop.
// Return the last loop after loop distribution.
// See PRO_LOOP_INTERCHANGE::Find_dist_cand for rules to find the distribution point.
// "do_interchange" indicates whether the purpose of loop distribution 
// is to enable loop interchange.
SC_NODE *
PRO_LOOP_INTERCHANGE_TRANS::Do_loop_dist(SC_NODE * sc_loop, BOOL do_interchange)
{
  FmtAssert((sc_loop->Type() == SC_LOOP), ("Expect a SC_LOOP"));

  if (!sc_loop->Is_sese()) 
    return NULL;

  SC_NODE * sc_parent = sc_loop->Find_kid_of_type(SC_LP_BODY);
  SC_NODE * inner_loop = sc_parent->Find_kid_of_type(SC_LOOP);

  if (!inner_loop)
    return NULL;

  SC_NODE * sc_par = Find_dist_cand(sc_parent);

  if (sc_par == NULL) {
    Do_pre_dist(inner_loop, sc_loop);
    sc_par = Find_dist_cand(sc_parent);
  }

  if (sc_par == NULL)
    return NULL;

  SC_NODE * sc_tmp = sc_parent->First_kid();
  BOOL is_empty = TRUE;

  while (sc_tmp != sc_par) {
    if (!sc_tmp->Is_empty_block()) {
      is_empty = FALSE;
      break;
    }
    sc_tmp = sc_tmp->Next_sibling();
  }

  if (is_empty)
    return NULL;

  sc_tmp = sc_par->Next_sibling();
  while (sc_tmp) {
    if ((sc_tmp->Type() == SC_LOOP)
	&& !sc_tmp->Has_same_loop_struct(sc_par))
      return NULL;
    sc_tmp = sc_tmp->Next_sibling();
  }

  if (do_interchange) {
    if ((sc_par->Type() != SC_LOOP)
	|| !Can_interchange(sc_loop, sc_par))
      return NULL;
  }

  SC_NODE * sc_last = sc_parent->Last_kid();
  CFG * cfg = _cu->Cfg();
  MEM_POOL * pool = cfg->Mem_pool();

  if ((WOPT_Enable_Pro_Loop_Limit >= 0)
      && (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
    return NULL;

  if (!sc_last->Is_empty_block())
    return NULL;

  BB_NODE * bb_first = sc_par->First_bb();
  BB_NODE * bb_last = sc_last->Last_bb();

  if (bb_first->Pred()->Len() != 1)
    return NULL;

  BB_NODE * bb_pred = bb_first->Pred()->Node();

  if (bb_pred->Succ()->Len() != 1)
    return NULL;

  BB_NODE * bb_succ = bb_last->Succ()->Node();

  if (bb_succ->Pred()->Len() != 1)
    return NULL;

  if ((WOPT_Enable_Pro_Loop_Limit >= 0)
      && (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
    return NULL;
  
  if (_trace)
    printf("\n\t\t Loop distribution (SC%d)\n", sc_loop->Id());

  FB_EDGE_TYPE edge_type;

  if (cfg->Feedback()) 
    edge_type = cfg->Feedback()->Get_edge_type(bb_last->Id(), bb_succ->Id());
  
  // Unlink blocks in (bb_pred, bb_succ) from CFG and SC tree.
  bb_pred->Replace_succ(bb_first, bb_succ);
  bb_succ->Replace_pred(bb_last, bb_pred);

  bb_pred->Set_next(bb_succ);
  bb_succ->Set_prev(bb_pred);

  if (cfg->Feedback()) {
    cfg->Feedback()->Move_edge_dest(bb_pred->Id(), bb_first->Id(), bb_succ->Id());
    cfg->Feedback()->Delete_edge(bb_last->Id(), bb_succ->Id());
  }
  
  SC_NODE * sc_tmp1 = sc_last;
  SC_NODE * sc_tmp2;
  STACK<SC_NODE *> * sc_stack = CXX_NEW(STACK<SC_NODE *>(_pool), _pool);

  while (sc_tmp1) {
    sc_tmp2 = sc_tmp1;
    sc_tmp1 = sc_tmp1->Prev_sibling();
    sc_stack->Push(sc_tmp2);
    sc_tmp2->Unlink();
    
    if (sc_tmp2 == sc_par)
      break;
  }

  // Clone sc_loop and insert before it.
  SC_NODE * sc_new = cfg->Clone_sc(sc_loop, TRUE, 1.0);
  BB_NODE * bb_lp_head = sc_loop->Head();
  BB_NODE * bb_lp_end = bb_lp_head->Loopend();
  BB_NODE * bb_lp_merge = sc_loop->Merge();
  BB_NODE * bb_lp_head_new = sc_new->Head();
  BB_NODE * bb_lp_end_new = bb_lp_head_new->Loopend();
  BB_NODE * bb_lp_step_new = bb_lp_head_new->Loopstep();
  BB_NODE * bb_tmp;
  BB_LIST_ITER bb_list_iter;
  BB_LIST * bb_list;

  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_lp_head->Pred())) {
    bb_tmp->Replace_succ(bb_lp_head, bb_lp_head_new);

    if (bb_tmp->Is_branch_to(bb_lp_head)) {
      WN * branch_wn = bb_tmp->Branch_wn();
      cfg->Add_label_with_wn(bb_lp_head_new);
      WN_label_number(branch_wn) = bb_lp_head_new->Labnam();
    }

    if (cfg->Feedback())
      cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb_lp_head->Id(), bb_lp_head_new->Id());
  }

  bb_lp_head_new->Set_pred(bb_lp_head->Pred());
  bb_lp_end_new->Prepend_succ(bb_lp_head, pool);
  WN * branch_wn = bb_lp_end_new->Branch_wn();
  cfg->Add_label_with_wn(bb_lp_head);
  WN_label_number(branch_wn) = bb_lp_head->Labnam();
  bb_list = CXX_NEW(BB_LIST(bb_lp_end_new), pool);
  bb_lp_head->Set_pred(bb_list);

  if (cfg->Feedback()) {
    FB_EDGE_TYPE edge_type = cfg->Feedback()->Get_edge_type(bb_lp_end->Id(), bb_lp_merge->Id());
    cfg->Feedback()->Add_edge(bb_lp_end_new->Id(), bb_lp_head->Id(), edge_type,
			      cfg->Feedback()->Get_node_freq_in(bb_lp_head_new->Id()));
  }
  
  bb_tmp = bb_lp_head->Prev();
  bb_tmp->Set_next(bb_lp_head_new);
  bb_lp_head_new->Set_prev(bb_tmp);
  bb_lp_step_new->Set_next(bb_lp_head);
  bb_lp_head->Set_prev(bb_lp_step_new);

  SC_NODE * sc_prev = sc_loop->Prev_sibling();
  sc_loop->Insert_before(sc_new);

  cfg->Fix_info(sc_new);
  cfg->Fix_info(sc_new->Get_real_parent());

  // Recover unlinked blocks in (bb_pred, bb_succ)
  bb_pred->Replace_succ(bb_succ, bb_first);
  bb_succ->Replace_pred(bb_pred, bb_last);

  if (cfg->Feedback()) {
    cfg->Feedback()->Move_edge_dest(bb_pred->Id(), bb_succ->Id(), bb_first->Id());
    cfg->Feedback()->Add_edge(bb_last->Id(), bb_succ->Id(), edge_type,
			      cfg->Feedback()->Get_node_freq_in(bb_first->Id()));
  }

  bb_pred->Set_next(bb_first);
  bb_succ->Set_prev(bb_last);

  sc_tmp2 = sc_parent->Last_kid();

  while (!sc_stack->Is_Empty()) {
    sc_tmp1 = sc_stack->Pop();
    sc_tmp2->Insert_after(sc_tmp1);
    sc_tmp2 = sc_tmp1;
  }

  bb_first = sc_parent->First_bb();
  bb_last = sc_par->Prev_sibling()->Last_bb();
  bb_succ = sc_par->First_bb();

  // Unlink blocks in [bb_first, bb_last] from CFG and SC tree.
  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_first->Pred())) {
    bb_tmp->Replace_succ(bb_first, bb_succ);
    if (bb_tmp->Is_branch_to(bb_first)) {
      WN * branch_wn = bb_tmp->Branch_wn();
      cfg->Add_label_with_wn(bb_succ);
      WN_label_number(branch_wn) = bb_succ->Labnam();
    }
  
    if (cfg->Feedback())
      cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb_first->Id(), bb_succ->Id());
  }

  bb_succ->Set_pred(bb_first->Pred());

  if (cfg->Feedback())
    cfg->Feedback()->Delete_edge(bb_last->Id(), bb_succ->Id());

  bb_tmp = bb_first->Prev();
  bb_tmp->Set_next(bb_succ);
  bb_succ->Set_prev(bb_tmp);
  
  sc_tmp1 = sc_parent->First_kid();
  while (sc_tmp1 && (sc_tmp1 != sc_par)) {
    sc_tmp2 = sc_tmp1;
    sc_tmp1 = sc_tmp1->Next_sibling();
    sc_tmp2->Unlink();
    sc_tmp2->Delete();
  }

  cfg->Fix_info(sc_loop);

  if (sc_prev)
    cfg->Fix_info(sc_prev);

  CXX_DELETE(sc_stack, _pool);

  cfg->Invalidate_and_update_aux_info(FALSE);    
  cfg->Invalidate_loops();
  Inc_transform_count();
  return sc_loop;
}

// Duplicate a if-condition bb_cond and wrap it around the given SC_NODE *,
// "is_then" tells whether the wrapped region should show up in the then-path.
void
PRO_LOOP_INTERCHANGE_TRANS::Do_if_cond_wrap( BB_NODE * bb_cond,
				     SC_NODE * sc_begin,
				     BOOL is_then)
{
  CFG * cfg = _cu->Cfg();
  MEM_POOL * pool = cfg->Mem_pool();

  SC_NODE * sc_p = sc_begin->Get_real_parent();
  BB_NODE * bb_new = NULL;
  cfg->Clone_bbs(bb_cond, bb_cond, &bb_new, &bb_new, TRUE, 1.0);

  BB_NODE * bb_then = bb_cond->If_then();
  BB_NODE * bb_else = bb_cond->If_else();

  BB_NODE * bb_first = sc_begin->First_bb();
  BB_NODE * bb_last = sc_begin->Last_bb();

  BB_NODE * bb_e = cfg->Create_and_allocate_bb(BB_GOTO);
  BB_NODE * bb_merge = cfg->Create_and_allocate_bb(BB_GOTO);

  SC_NODE * sc_if = cfg->Create_sc(SC_IF);
  SC_NODE * sc_then = cfg->Create_sc(SC_THEN);
  SC_NODE * sc_else = cfg->Create_sc(SC_ELSE);
  SC_NODE * sc_merge = cfg->Create_sc(SC_BLOCK);
  SC_NODE * sc_e = cfg->Create_sc(SC_BLOCK);
  SC_LIST_ITER sc_list_iter;
  SC_NODE * sc_tmp;

  sc_if->Set_bb_rep(bb_new);
  sc_merge->Append_bbs(bb_merge);
  sc_e->Append_bbs(bb_e);
  
  //           |                         |
  //          bb_first               bb_new(IF)
  //        /    \                  /        \
  //      48     47  ==>          bb_first   |
  //        \   /                 /    \     bb_e
  //          bb_last            48    47    |
  //           |                  \    /     |
  //                              bb_last    |
  //                                 \      /
  //                                 bb_merge
  //                                    |
  BB_NODE * bb_tmp;
  BB_LIST_ITER  bb_list_iter;
  BB_LIST * bb_list_tmp;
  WN * branch_wn;
  BB_IFINFO * if_info = bb_new->Ifinfo();
  IDTYPE edge;
  FB_FREQ freq;
  FB_FREQ freq_then;
  FB_FREQ freq_else;
  FB_EDGE_TYPE ft_edge_type;
  FB_EDGE_TYPE br_edge_type;
  float scale;
  
  if_info->Set_cond(bb_new);
  if_info->Set_merge(bb_merge);

  if (cfg->Feedback()) {
    ft_edge_type = cfg->Feedback()->Get_edge_type(bb_cond->Id(), bb_then->Id());
    br_edge_type = cfg->Feedback()->Get_edge_type(bb_cond->Id(), bb_else->Id());
    cfg->Feedback()->Add_node(bb_new->Id());
    cfg->Feedback()->Add_node(bb_e->Id());
    cfg->Feedback()->Add_node(bb_merge->Id());
    freq_then = cfg->Feedback()->Get_edge_freq(bb_cond->Id(), bb_then->Id());
    freq_else = cfg->Feedback()->Get_edge_freq(bb_cond->Id(), bb_else->Id());
  }

  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_first->Pred())) {
    bb_tmp->Replace_succ(bb_first, bb_new);
    if (bb_tmp->Is_branch_to(bb_first)) {
      branch_wn = bb_tmp->Branch_wn();
      cfg->Add_label_with_wn(bb_new);
      WN_label_number(branch_wn) = bb_new->Labnam();
    }

    if (cfg->Feedback()) 
      cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb_first->Id(), bb_new->Id());
  }

  if (cfg->Feedback()) {
    freq = cfg->Feedback()->Get_node_freq_in(bb_new->Id());
    freq_then = freq * (freq_then / (freq_then + freq_else));
    freq_else = freq - freq_then;
  }

  bb_new->Set_pred(bb_first->Pred());
  bb_list_tmp = CXX_NEW(BB_LIST(bb_e), pool);
  bb_new->Set_succ(bb_list_tmp);

  bb_list_tmp = CXX_NEW(BB_LIST(bb_e), pool);
  bb_merge->Set_pred(bb_list_tmp);

  bb_tmp = bb_first->Prev();
  bb_tmp->Set_next(bb_new);
  bb_new->Set_prev(bb_tmp);
  bb_tmp = bb_last->Next();
  bb_tmp->Set_prev(bb_merge);
  bb_merge->Set_next(bb_tmp);

  if (is_then) {
    bb_new->Append_succ(bb_first, pool);
    bb_tmp = bb_e;
    bb_merge->Append_pred(bb_last, pool);
    bb_new->Set_next(bb_first);
    bb_first->Set_prev(bb_new);
    bb_last->Set_next(bb_e);
    bb_e->Set_prev(bb_last);
    bb_e->Set_next(bb_merge);
    bb_merge->Set_prev(bb_e);
    if_info->Set_then(bb_first);
    if_info->Set_else(bb_e);

    if (cfg->Feedback()) {
      cfg->Feedback()->Add_edge(bb_new->Id(), bb_first->Id(), ft_edge_type, freq_then);
      cfg->Feedback()->Add_edge(bb_new->Id(), bb_e->Id(), br_edge_type, freq_else);
      cfg->Feedback()->Add_edge(bb_e->Id(), bb_merge->Id(), FB_EDGE_OUTGOING, freq_else);
      freq = freq_then / freq;
      scale = freq.Value();
    }
  }
  else {
    bb_new->Prepend_succ(bb_first, pool);
    bb_tmp = bb_first;
    bb_merge->Prepend_pred(bb_last, pool);
    bb_new->Set_next(bb_e);
    bb_e->Set_prev(bb_new);
    bb_first->Set_prev(bb_e);
    bb_e->Set_next(bb_first);
    bb_last->Set_next(bb_merge);
    bb_merge->Set_prev(bb_last);
    if_info->Set_then(bb_e);
    if_info->Set_else(bb_first);

    if (cfg->Feedback()) {
      cfg->Feedback()->Add_edge(bb_new->Id(), bb_first->Id(), br_edge_type, freq_else);
      cfg->Feedback()->Add_edge(bb_new->Id(), bb_e->Id(), ft_edge_type, freq_then);
      cfg->Feedback()->Add_edge(bb_e->Id(), bb_merge->Id(), FB_EDGE_OUTGOING, freq_then);
      freq = freq_else / freq;
      scale = freq.Value();
    }
  }

  branch_wn = bb_new->Branch_wn();
  cfg->Add_label_with_wn(bb_tmp);
  WN_label_number(branch_wn) = bb_tmp->Labnam();
  
  bb_list_tmp = CXX_NEW(BB_LIST(bb_new), pool);
  bb_e->Set_pred(bb_list_tmp);

  bb_list_tmp = CXX_NEW(BB_LIST(bb_new), pool);
  bb_first->Set_pred(bb_list_tmp);

  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_last->Succ())) {
    bb_tmp->Replace_pred(bb_last, bb_merge);
  }
  
  bb_merge->Set_succ(bb_last->Succ());

  if (cfg->Feedback()) {
    FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_last->Succ())) {
      freq = cfg->Feedback()->Get_edge_freq(bb_last->Id(), bb_tmp->Id());
      cfg->Feedback()->Delete_edge(bb_last->Id(), bb_tmp->Id());
      cfg->Feedback()->Add_edge(bb_merge->Id(), bb_tmp->Id(), FB_EDGE_OUTGOING, freq);
    }
    
    bb_last->Set_succ(NULL);
    cfg->Freq_scale(sc_begin, scale);

    if (is_then) 
      cfg->Feedback()->Add_edge(bb_last->Id(), bb_merge->Id(), FB_EDGE_OUTGOING, freq_then);
    else 
      cfg->Feedback()->Add_edge(bb_last->Id(), bb_merge->Id(), FB_EDGE_OUTGOING, freq_else);
  }
  
  bb_list_tmp = CXX_NEW(BB_LIST(bb_merge), pool);
  bb_last->Set_succ(bb_list_tmp);

  bb_list_tmp = CXX_NEW(BB_LIST(bb_merge), pool);
  bb_e->Set_succ(bb_list_tmp);

  if (sc_begin->Type() == SC_LP_BODY) {
    //                           |
    //                         LP_BODY
    //                        /     \
    //                      SC_IF   SC_MERGE
    //     |              /       \
    //    LP_BODY ==>   SC_THEN  SC_ELSE
    //   / | \                  /  |  \
    //                         

    FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init(sc_begin->Kids())) {
      if (is_then) 
	sc_tmp->Set_parent(sc_then);
      else
	sc_tmp->Set_parent(sc_else);
    }

    if (is_then) {
      sc_then->Set_kids(sc_begin->Kids());
      sc_else->Append_kid(sc_e);
      sc_e->Set_parent(sc_else);
    }
    else {
      sc_else->Set_kids(sc_begin->Kids());
      sc_then->Append_kid(sc_e);
      sc_e->Set_parent(sc_then);
    }
    sc_begin->Set_kids(NULL);
    sc_begin->Append_kid(sc_if);
    sc_if->Set_parent(sc_begin);
    sc_begin->Append_kid(sc_merge);
    sc_merge->Set_parent(sc_begin);
    sc_if->Append_kid(sc_then);
    sc_if->Append_kid(sc_else);
    sc_then->Set_parent(sc_if);
    sc_else->Set_parent(sc_if);
  }
  else {
    FmtAssert(FALSE, ("TODO"));
  }

  cfg->Fix_info(sc_p);
}

// Find whether the given sc has a unique read/write reference, i.e.,
// - Has no scalar store.
// - Scalar loads are reference to loop invariants.
// - All non-scalar loads and stores have the same address.
//
// Return the address of the non-scalar load/store in *wn_addr.
// Return TRUE if the uniqueness is verified.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Get_unique_ref(SC_NODE * sc, SC_NODE * sc_loop, WN ** wn_addr)
{
  BB_NODE * bb = sc->Get_bb_rep();

  if (bb != NULL) {
    if (!Get_unique_ref(bb, sc_loop, wn_addr))
      return FALSE;
  }

  BB_LIST * bb_list = sc->Get_bbs();

  if (bb_list != NULL) {
    BB_LIST_ITER bb_list_iter(bb_list);
    BB_NODE * tmp;
    
    FOR_ALL_ELEM(tmp, bb_list_iter, Init()) {
      if (!Get_unique_ref(tmp, sc_loop, wn_addr))
	return FALSE;
    }
  }

  SC_LIST * kids = sc->Kids();

  if (kids != NULL) {
    SC_LIST_ITER sc_list_iter(kids);
    SC_NODE * tmp;
    
    FOR_ALL_ELEM(tmp, sc_list_iter, Init()) {
      if (!Get_unique_ref(tmp, sc_loop, wn_addr))
	return FALSE;
    }
  }

  return TRUE;
}

// Find whether the given bb has a unique read/write reference.
// See PRO_LOOP_INTERCHANGE_TRANS::Get_unique_ref(SC_NODE *, SC_NODE *, WN **)
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Get_unique_ref(BB_NODE * bb, SC_NODE * sc_loop, WN ** wn_addr)
{
  STMT_ITER stmt_iter;
  WN * wn_iter;
  FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb->Firststmt(), bb->Laststmt())) {
    if (!Get_unique_ref(wn_iter, sc_loop, wn_addr))
      return FALSE;
  }

  return TRUE;
}

// Find whether the given wn has a unique read/write reference.
// See PRO_LOOP_INTERCHANGE_TRANS::Get_unique_ref(SC_NODE *, SC_NODE *, WN **)
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Get_unique_ref(WN * wn, SC_NODE * sc_loop, WN ** wn_addr)
{
  OPERATOR opr = WN_operator(wn);

  // WN_aux(wn) may not be set correctly in the memory references
  // in the expression trees for IO statements, so we should avoid
  // traversing these statements.
  if (opr == OPR_IO)
    return FALSE;
  if (OPERATOR_is_load(opr) || OPERATOR_is_store(opr)) {
    if (OPERATOR_is_scalar_store(opr)) 
      return FALSE;
    else if (OPERATOR_is_scalar_load(opr))
      return Is_invariant(sc_loop, wn, 0);
    else {
      WN * wn_tmp = OPERATOR_is_load(opr) ? WN_kid0(wn) : WN_kid1(wn);

      if ((*wn_addr) == NULL) 
	*wn_addr = wn_tmp;
      else if (WN_Simp_Compare_Trees(*wn_addr, wn_tmp) != 0)
	return FALSE;
    }

    return TRUE;
  }

  for (int i = 0; i < WN_kid_count(wn); i++) {
    if (!Get_unique_ref(WN_kid(wn, i), sc_loop, wn_addr))
      return FALSE;
  }

  return TRUE;
}

// Find a load of the given ST * in the WHIRL tree rooted at wn.
WN *
PRO_LOOP_INTERCHANGE_TRANS::Get_index_load(WN * wn, ST * st)
{
  OPT_STAB * opt_stab = _cu->Opt_stab();
  OPERATOR opr = WN_operator(wn);
  
  if (OPERATOR_is_scalar_load(opr)) {
    AUX_ID aux_id = WN_aux(wn);
    ST * cur_st = opt_stab->Aux_stab_entry(aux_id)->St();
      
    if (cur_st && (cur_st == st))
      return wn;
  }
  
  for ( int i = 0; i < WN_kid_count(wn); i++) {
    WN * wn_tmp = Get_index_load(WN_kid(wn,i), st);
    if (wn_tmp)
      return wn_tmp;
  }

  return NULL;
}

// Given a SC_LOOP, find a load of its index.
WN *
PRO_LOOP_INTERCHANGE_TRANS::Get_index_load(SC_NODE * sc)
{
  if (sc->Type() == SC_LOOP) {
    WN * wn_index = sc->Index();
    ST * st_index = WN_st(wn_index);
    SC_NODE * sc_tmp = sc->Find_kid_of_type(SC_LP_STEP);
    BB_NODE * bb = sc_tmp->First_bb();
    STMT_ITER stmt_iter;
    WN * wn_iter;

    FOR_ALL_ELEM (wn_iter, stmt_iter, Init(bb->Firststmt(), bb->Laststmt())) {
      WN  * wn_tmp = Get_index_load(wn_iter, st_index);
      if (wn_tmp)
	return wn_tmp;
    }
    
    FmtAssert(FALSE, ("Load of loop index not found"));
  }

  return NULL;
}

// Do loop fusion for sc and its consecutive loops.
// Return the fused loop.
SC_NODE * 
PRO_LOOP_INTERCHANGE_TRANS::Do_loop_fusion(SC_NODE * sc)
{
  FmtAssert((sc->Type() == SC_LOOP), ("Expect a SC_LOOP"));

  if ((WOPT_Enable_Pro_Loop_Limit >= 0)
      && (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
    return NULL;

  CFG * cfg = _cu->Cfg();
  MEM_POOL * pool = cfg->Mem_pool();

  SC_NODE * sc_body = sc->Find_kid_of_type(SC_LP_BODY);
  BB_LIST * bb_list;
  BB_NODE * bb_tmp;
  BB_NODE * bb_tmp2;
  SC_NODE * sc_tmp = sc->Next_sibling();
  SC_NODE * sc_tmp1;
  SC_NODE * sc_tmp2;
  BB_LIST_ITER bb_list_iter;

  if (sc_body->Last_bb()) {
    _def_map = CXX_NEW(MAP(CFG_BB_TAB_SIZE, _pool), _pool);
    WN *  wn_index = Get_index_load(sc);

    while (sc_tmp && (sc_tmp->Type() == SC_LOOP)) {
      BB_NODE * bb2 = sc_body->Last_bb();      
      SC_NODE * cur_sc_body = sc_tmp->Find_kid_of_type(SC_LP_BODY);
      BB_NODE * cur_bb1 = cur_sc_body->First_bb();
      BB_NODE * cur_bb2 = cur_sc_body->Last_bb();
      SC_NODE * sc_next = sc_tmp->Next_sibling();
      WN * cur_wn_index = Get_index_load(sc_tmp);

      // Replace loop indexes.
      Add_def_map(WN_aux(cur_wn_index), wn_index);
      Copy_prop(cur_sc_body);

      if (cur_bb1) {
	if (_trace) 
	  printf("\n\t\t Loop fusion (SC%d,SC%d)\n", sc->Id(), sc_tmp->Id());

	// fuse loop bodies.
	FmtAssert((bb2->Succ()->Len() == 1) && (cur_bb2->Succ()->Len() == 1)
		  && (cur_bb1->Pred()->Len() == 1), ("Expect single pred/succ"));
	bb_tmp = bb2->Succ()->Node();
	bb_tmp->Replace_pred(bb2, cur_bb2);

	if (cfg->Feedback()) {
	  cfg->Feedback()->Move_edge_dest(bb2->Id(), bb_tmp->Id(), cur_bb1->Id());
	  bb_tmp2 = cur_bb2->Succ()->Node();
	  cfg->Feedback()->Move_edge_dest(cur_bb2->Id(), bb_tmp2->Id(), bb_tmp->Id());
	}

	cur_bb2->Set_succ(bb2->Succ());
	bb_tmp = cur_bb1->Pred()->Node();
	cur_bb1->Replace_pred(bb_tmp, bb2);
	bb_list = CXX_NEW(BB_LIST(cur_bb1), pool);
	bb2->Set_succ(bb_list);

	bb_tmp = bb2->Next();
	bb2->Set_next(cur_bb1);
	cur_bb1->Set_prev(bb2);
	cur_bb2->Set_next(bb_tmp);
	bb_tmp->Set_prev(cur_bb2);

	sc_tmp1 = cur_sc_body->First_kid();
	while (sc_tmp1) {
	  sc_tmp2 = sc_tmp1->Next_sibling();
	  sc_tmp1->Unlink();
	  sc_body->Append_kid(sc_tmp1);
	  sc_tmp1->Set_parent(sc_body);
	  sc_tmp1 = sc_tmp2;
	}
      }

      // Unlink the second loop.
      BB_NODE * bb_entry =  sc_tmp->First_bb();
      BB_NODE * bb_merge = sc_tmp->Merge();
      BB_NODE * bb_exit = sc_tmp->Exit();
      
      if (cfg->Feedback())
	cfg->Feedback()->Delete_edge(bb_exit->Id(), bb_merge->Id());

      FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_entry->Pred())) {
	bb_tmp->Replace_succ(bb_entry, bb_merge);
	
	if (bb_tmp->Is_branch_to(bb_entry)) {
	  WN * branch_wn = bb_tmp->Branch_wn();
	  cfg->Add_label_with_wn(bb_merge);
	  WN_label_number(branch_wn) = bb_merge->Labnam();
	}

	if (cfg->Feedback())
	  cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb_entry->Id(), bb_merge->Id());
      }
      
      bb_merge->Set_pred(bb_entry->Pred());

      bb_tmp = bb_entry->Prev();
      bb_tmp->Set_next(bb_merge);
      bb_merge->Set_prev(bb_tmp);
      sc_tmp->Unlink();
      _unlink_sc->Push(sc_tmp);
      sc_tmp = sc_next;

      cfg->Fix_info(sc);
      cfg->Fix_info(sc->Parent());
      cfg->Fix_info(sc->Get_real_parent());
    }

    CXX_DELETE(_def_map, _pool);
  }

  sc_tmp = sc->Next_sibling();
  
  if (!sc_tmp || (sc_tmp->Type() != SC_LOOP)) {
    cfg->Invalidate_and_update_aux_info(FALSE);    
    cfg->Invalidate_loops();
    Inc_transform_count();
    return sc;
  }
  else
    return NULL;
}

// Compare whether wn1 in loop1 is the same as wn2 in loop2.
// Loads of loop indexes are considered to be the same.

BOOL
PRO_LOOP_INTERCHANGE_TRANS::Compare_trees(WN * wn1, SC_NODE * loop1, WN * wn2, SC_NODE * loop2)
{
  OPERATOR op1 = WN_operator(wn1);
  OPERATOR op2 = WN_operator(wn2);
  
  if (OPERATOR_is_scalar_load(op1)
      && OPERATOR_is_scalar_load(op2)) {
    ST * st1 = Get_st(wn1);
    ST * st2 = Get_st(wn2);
    WN * index1 = loop1->Index();
    WN * index2 = loop2->Index();

    if (st1 && st2 && (st1 == WN_st(index1)) && (st2 == WN_st(index2)))
      return TRUE;
    else
      return (WN_Simp_Compare_Trees(wn1, wn2) == 0);
  }

  if ((op1 != op2) || (WN_kid_count(wn1) != WN_kid_count(wn2)))
    return FALSE;

  for (int i = 0; i < WN_kid_count(wn1); i++) {
    WN * kid1 = WN_kid(wn1, i);
    WN * kid2 = WN_kid(wn2, i);
    if (!Compare_trees(kid1, loop1, kid2, loop2))
      return FALSE;
  }

  return TRUE;
}

// Check whether given SC_LOOP's index is a AUTO or a REG.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Check_index(SC_NODE * sc)
{
  WN * wn_index = sc->Index();
  if (!wn_index)
    return FALSE;
  
  // Check whether loop index is a AUTO or a REG.
  ST * index_st = WN_st(wn_index);
  if ((ST_sclass(index_st) != SCLASS_REG) && (ST_sclass(index_st) != SCLASS_AUTO))
    return FALSE;
  
  return TRUE;
}

// Given a sc_loop, check whether loads/stores in the sc_loop's kid of sc_type are either loop 
// invariants w.r.t. sc_ref or loop indexes w.r.t to sc_loop.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Check_iteration(SC_NODE * sc_loop, SC_TYPE sc_type, SC_NODE * sc_ref)
{
  WN * wn_load = Get_index_load(sc_loop);
  AUX_ID loop_aux_id = wn_load ? WN_aux(wn_load) : 0;

  SC_LIST_ITER sc_list_iter;
  SC_NODE * sc_tmp1 = sc_loop->Find_kid_of_type(sc_type);
  SC_NODE * sc_tmp2;
  
  FOR_ALL_ELEM(sc_tmp2, sc_list_iter, Init(sc_tmp1->Kids())) {
    if (!Is_invariant(sc_ref, sc_tmp2, loop_aux_id))
      return FALSE;
  }
  
  return TRUE;
}

// Check whether sc_loop can be fused with its consecutive succeeding loops.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Can_fuse(SC_NODE * sc_loop)
{
  if (sc_loop->Type() != SC_LOOP)
    return FALSE;

  SC_NODE * sc_cur = sc_loop->Next_sibling();
  if (!sc_cur || (sc_cur->Type() != SC_LOOP))
    return FALSE;

  WN * wn_u = NULL;
  SC_NODE * lp_u = NULL;
  sc_cur = sc_loop;

  while (sc_cur && (sc_cur->Type() == SC_LOOP)) {
    if ((sc_cur != sc_loop) && !sc_cur->Has_same_loop_struct(sc_loop))
      return FALSE;

    if (!Check_index(sc_cur)
      || !Check_iteration(sc_cur, SC_LP_COND, sc_cur)
      || !Check_iteration(sc_cur, SC_LP_STEP, sc_cur))
      return FALSE;

    // Only do it for the simplest case where the loops only contains
    // non-scalar memory references of the same address.
    // TODO: code sharing with LNO for legality check.
    WN * wn_tmp = NULL;
    Get_unique_ref(sc_cur->Find_kid_of_type(SC_LP_BODY), sc_cur, &wn_tmp);

    if (wn_tmp == NULL)
      return FALSE;

    if (wn_u == NULL) {
      wn_u = wn_tmp;
      lp_u = sc_cur;
    }
    else if (!Compare_trees(wn_u, lp_u, wn_tmp, sc_cur))
      return FALSE;

    sc_cur = sc_cur->Next_sibling();
  }
  
  return TRUE;
}

// Do reversed loop unswitching.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Do_reverse_loop_unswitching
(SC_NODE * sc1, SC_NODE * sc2, SC_NODE * outer_loop)
{
  FmtAssert((sc1->Type() == SC_IF) && (sc2->Type() == SC_LOOP), ("Unexpected SC type"));
  CFG * cfg = _cu->Cfg();

  // sc1 should a SESE and the only SC_IF among its siblings.
  if (!sc1->Is_sese()
      || (sc1->Next_sibling_of_type(SC_IF) != NULL)
      || (sc1->Parent()->Find_kid_of_type(SC_IF) != sc1))
    return FALSE;
  
  SC_NODE * sc1_p = sc1->Get_real_parent();
  BOOL is_then = FALSE;
  SC_NODE * sc_tmp = sc2->Parent();

  if (sc_tmp->Type() == SC_THEN)
    is_then = TRUE;

  if (sc_tmp->Parent() != sc1)
    return FALSE;

  if (!Check_index(sc2)
      || !Check_iteration(sc2, SC_LP_COND, sc2)
      || !Check_iteration(sc2, SC_LP_STEP, sc2))
    return FALSE;

  SC_NODE * sc_merge = sc1->Next_sibling();

  // sc_merge should be an empty block. Canonicalization guarantees this.
  if (!sc_merge->Is_empty_block())
    return FALSE;
  
  SC_NODE * sc_begin = Do_pre_dist(sc2, outer_loop);
  
  if (sc_begin == NULL)
    return FALSE;

  // Catch cases of zero dependency vector to do loop fusion here.
  BOOL do_fuse = TRUE;
  WN * wn_u = NULL;
  SC_NODE * lp_u = NULL;
  SC_NODE * sc_tmp1;
  SC_NODE * sc_tmp2;
  
  if (!Can_fuse(sc_begin))
    do_fuse = FALSE;
  else {
    Get_unique_ref(sc_begin->Find_kid_of_type(SC_LP_BODY), sc_begin, &wn_u);
    lp_u = sc_begin;

    // Do loop fusion if possible.
    sc_tmp1 = Do_loop_fusion(sc_begin);
    if (sc_tmp1) 
      sc2 = sc_tmp1;
  }

  SC_NODE * sc_end = sc2->Next_sibling_of_type(SC_BLOCK);
  FmtAssert(sc_end && sc_end->Is_empty_block(), ("Expect an empty merge for loop"));

  sc_tmp1 = sc_merge->Next_sibling_of_type(SC_LOOP);

  if (sc_tmp1) {
    // Do not sink if there exists sinked loops and the sinked loops were not fused.    
    if (sc_tmp1->Next_sibling_of_type(SC_LOOP) != NULL)
      return FALSE;

    // Do not sink if sc2 and already-sinked loops are not loop fusion candidates.
    if (!sc_tmp1->Has_same_loop_struct(sc2))
      return FALSE;
    
    WN * wn_tmp = NULL;
    Get_unique_ref(sc_tmp1->Find_kid_of_type(SC_LP_BODY), sc_tmp1, &wn_tmp);

    if ((wn_tmp == NULL) 
	|| (wn_u == NULL)
	|| !Compare_trees(wn_u, lp_u, wn_tmp, sc_tmp1))
      return FALSE;
  }

  if ((WOPT_Enable_Pro_Loop_Limit >= 0)
      && (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
    return FALSE;

  if (_trace)
    printf("\n\t\t Reversed loop unswitching (SC%d,SC%d)\n", sc1->Id(), sc2->Id());

  // Insert an empty block to keep sese for sc1.
  if (!sc_end->Next_sibling())
    cfg->Insert_block_after(sc_end);

  BB_NODE * bb_head = sc1->Get_bb_rep(); 
  BB_NODE * bb_merge = sc1->Merge();
  BB_NODE * bb_merge_end = sc_merge->Last_bb();
  BB_NODE * bb_tmp1;
  BB_NODE * bb_tmp2;
  BB_NODE * bb_tmp;
  BB_NODE * bb_prev;
  BB_NODE * bb_next;
  BB_NODE * bb_44;
  BB_LIST * bb_list_tmp;
  BB_LIST_ITER bb_list_iter;
  WN * branch_wn;
  BB_IFINFO * if_info;
  MEM_POOL * pool = cfg->Mem_pool();
  IDTYPE edge;
  FB_FREQ freq;

  bb_tmp1 = sc_begin->First_bb();
  bb_tmp2 = sc_end->Last_bb();
  bb_44 = bb_tmp2->Succ()->Node();
  
  // Sink blocks in [sc_begin, sc_end] out of if-region, where sc_end is a SC_BLOCK.
  //                                        
  //                   42                     42
  //                    |                     |
  //        bb_head->  79 <--- sc1            79
  //                /       \                /  \
  //              ...       |              ...   |
  //               |        |               |    |
  //    bb_tmp1-> 51        ...       =>    44  ...  
  //               |                        \   /
  //    bb_tmp2-> 52        |                merge <- bb_merge
  //               |        |                 |
  //              44        |                ... <- bb_merge_end
  //               \        /                 |
  //      bb_merge->  merge		     51
  //                  |                       |
  //  bb_merge-end-> ...                     52
  //                  |                       |

  // ... <-> 44
  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_tmp1->Pred())) {
    if (bb_tmp->Is_branch_to(bb_tmp1)) {
      branch_wn = bb_tmp->Branch_wn();
      cfg->Add_label_with_wn(bb_44);
      WN_label_number(branch_wn) = bb_44->Labnam();
    }

    bb_tmp->Replace_succ(bb_tmp1, bb_44);

    if (cfg->Feedback())
      cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb_tmp1->Id(), bb_44->Id());
  }

  bb_44->Set_pred(bb_tmp1->Pred());

  // 52<-
  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_merge_end->Succ())) {
    bb_tmp->Replace_pred(bb_merge_end, bb_tmp2);

    if (cfg->Feedback()) {
      cfg->Feedback()->Move_edge_dest(bb_merge_end->Id(), bb_tmp->Id(), bb_tmp1->Id());
      cfg->Feedback()->Move_edge_dest(bb_tmp2->Id(), bb_44->Id(), bb_tmp->Id());
      freq = cfg->Feedback()->Get_edge_freq(bb_merge_end->Id(), bb_tmp1->Id());
      edge = cfg->Feedback()->Get_edge(bb_tmp2->Id(), bb_tmp->Id());
      cfg->Feedback()->Change_edge_freq(edge, freq);
    }
  }

  // 52->
  bb_tmp2->Set_succ(bb_merge_end->Succ());

  // bb_merge_end<->51
  bb_list_tmp = CXX_NEW(BB_LIST(bb_tmp1), pool);
  bb_merge_end->Set_succ(bb_list_tmp);
  bb_list_tmp = CXX_NEW(BB_LIST(bb_merge_end), pool);
  bb_tmp1->Set_pred(bb_list_tmp);

  bb_prev = bb_tmp1->Prev();
  bb_prev->Set_next(bb_44);
  bb_tmp = bb_44->Prev();
  bb_44->Set_prev(bb_prev);
  bb_next = bb_merge_end->Next();
  bb_next->Set_prev(bb_tmp);
  bb_tmp->Set_next(bb_next);
  bb_merge_end->Set_next(bb_tmp1);
  bb_tmp1->Set_prev(bb_merge_end);

  SC_NODE * sc_insert = sc1->Next_sibling();
  sc_tmp1 = sc_begin;

  while (sc_tmp1) {
    sc_tmp2 = sc_tmp1;
    sc_tmp1 = sc_tmp1->Next_sibling();

    if (sc_tmp2->Type() == SC_LOOP) {
      // Wrap loop body with cloned condition.
      sc_tmp = sc_tmp2->Find_kid_of_type(SC_LP_BODY);
      Do_if_cond_wrap(bb_head, sc_tmp, is_then);
    }

    sc_tmp2->Unlink();
    sc_insert->Insert_after(sc_tmp2);
    cfg->Fix_info(sc_insert);
    cfg->Fix_info(sc_tmp2);
    cfg->Fix_info(sc1_p);
    sc_insert = sc_tmp2;

    if (sc_tmp2 == sc_end)
      break;
  }

  // Fix SC tree.
  cfg->Fix_info(sc1);

  if (cfg->Feedback()) {
    sc_tmp1 = sc_begin;
    while (sc_tmp1) {
      if (sc_tmp1->Type() == SC_LOOP) {
	bb_tmp1 = sc_tmp1->Head();
	sc_tmp = sc_tmp1->Find_kid_of_type(SC_LP_BODY);
	// If head does not belong to loop body, invalidate edge freq for
	// head's successors.
	if (sc_tmp && !sc_tmp->Contains(bb_tmp1)) {
	  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_tmp1->Succ())) {
	    edge = cfg->Feedback()->Get_edge(bb_tmp1->Id(), bb_tmp->Id());
	    if (edge)
	      cfg->Feedback()->Change_edge_freq(edge, FB_FREQ_UNKNOWN);
	  }
	}
	// Invalidate edge freq on loop exits.
	bb_tmp1 = sc_tmp1->Exit();
	bb_tmp2 = sc_tmp1->Merge();
	edge = cfg->Feedback()->Get_edge(bb_tmp1->Id(), bb_tmp2->Id());
	if (edge)
	  cfg->Feedback()->Change_edge_freq(edge, FB_FREQ_UNKNOWN);
      }
      else {
	bb_tmp1 = sc_tmp1->Last_bb();
	FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_tmp1->Succ())) {
	  edge = cfg->Feedback()->Get_edge(bb_tmp1->Id(), bb_tmp->Id());
	  if (edge)
	    cfg->Feedback()->Change_edge_freq(edge, FB_FREQ_UNKNOWN);
	}
      }
      cfg->Freq_propagate(sc_tmp1);

      if (sc_tmp1 == sc_end)
	break;
      sc_tmp1 = sc_tmp1->Next_sibling();
    }
  }

  bb_tmp1 = sc1->Head();
  bb_tmp2 = sc1->Merge();

  // If sc1 degenerates into an empty region, change it into a SC_BLOCK.
  if ((sc1->Then() == bb_tmp2) && (sc1->Else() == bb_tmp2)) {
    sc_tmp = sc1->Find_kid_of_type(SC_THEN);
    sc_tmp->Unlink();
    sc_tmp->Delete();
    sc_tmp = sc1->Find_kid_of_type(SC_ELSE);
    sc_tmp->Unlink();
    sc_tmp->Delete();
    sc1->Convert(SC_BLOCK);
    bb_tmp1->Set_kind(BB_GOTO);
    FmtAssert((bb_tmp1->Succ()->Len() == 2), ("Expect two successors"));
    bb_tmp1->Remove_succ(bb_tmp2, pool);
    bb_tmp2->Remove_pred(bb_tmp1, pool);
    bb_tmp1->Set_laststmt(NULL);
    bb_tmp1->Set_firststmt(NULL);
    
    if (cfg->Feedback()) {
      freq = cfg->Feedback()->Get_node_freq_in(bb_tmp1->Id());
      edge = cfg->Feedback()->Get_edge(bb_tmp1->Id(), bb_tmp2->Id());
      cfg->Feedback()->Change_edge_freq(edge, freq);
    }
  }

  Hoist_succ_blocks(sc2);

  // Remove empty blocks between sc_merge and sc2.
  sc_tmp1 = sc_merge->Next_sibling();
  while (sc_tmp1 && (sc_tmp1 != sc2)) {
    sc_tmp2 = sc_tmp1;
    sc_tmp1 = sc_tmp1->Next_sibling();
    if (sc_tmp2->Is_empty_block())
      Remove_block(sc_tmp2);
  }

  cfg->Invalidate_and_update_aux_info(FALSE);    
  cfg->Invalidate_loops();
  Inc_transform_count();

  if (Can_fuse(sc2)) {
    sc_tmp1 = Do_loop_fusion(sc2);
    if (sc_tmp1) {
      IF_MERGE_TRANS::Top_down_trans(sc_tmp1->Find_kid_of_type(SC_LP_BODY));
    }
  }

  return TRUE;
}

// Check whether the expression tree rooted at the given wn has the following characteristic:
// - All loads are scalars.
// - No stores.
// - Condition sub-expressions are either CAND or COR.
// - Can be speculative.

BOOL
PRO_LOOP_INTERCHANGE_TRANS::Can_reorder_cond(WN * wn, WN * cond_wn)
{
  if (!Can_be_speculative(wn))
    return FALSE;

  OPERATOR opr = WN_operator(wn);
  OPERATOR cond_opr;

  if (cond_wn)
    cond_opr = WN_operator(cond_wn);

  BOOL ret_val = TRUE;

  switch (opr) {
  case OPR_BAND:
  case OPR_CAND:
    if (cond_wn) {
      if ((cond_opr != OPR_BAND) && (cond_opr != OPR_CAND))
	ret_val = FALSE;
    }
    else
      cond_wn = wn;
    break;
  case OPR_BIOR:
  case OPR_CIOR:
    if (cond_wn) {
      if ((cond_opr != OPR_BIOR) && (cond_opr != OPR_CIOR))
	ret_val = FALSE;
    }
    else
      cond_wn = wn;

    break;
  default:
    ;
  }

  if (!ret_val)
    return FALSE;

  if (OPERATOR_is_store(opr))
    return FALSE;
  else if (OPERATOR_is_load(opr) 
	   && !OPERATOR_is_scalar_load(opr))
    return FALSE;
  
  for (int i = 0; i < WN_kid_count(wn); i++) {
    if (!Can_reorder_cond( WN_kid(wn, i), cond_wn))
      return FALSE;
  }
  
  return TRUE;
}

// Distribute if-condition in sc to its children.
BOOL
PRO_LOOP_INTERCHANGE_TRANS::Do_if_cond_dist(SC_NODE * sc)
{
  if ((sc->Type() != SC_IF) || (sc->Head()->Executable_stmt_count() != 1))
    return FALSE;

  SC_NODE * sc_p = sc->Get_real_parent();
  SC_NODE * sc_prev = sc->Prev_sibling();

  //
  //              4                                 4
  //              |                                 |
  //              5                                 6  
  //        /          \                         /    \
  //       /            \                      5_1    5_2
  //      42             6 ->bb_head          /  \    / \
  //    /   \          /   \                  |  8    | |
  //   39    51        8    7                 | /\    | 7
  //  /   \  |       /   \  |                e1 ...  e2 |
  //  ...    ....    .....  |  =======>       | \/    | |
  //  \   /  |       \   /  7_c               | 37    | 7_c
  //    64   60       37    |                 | |     | |
  //    \    /         \   /                  | 38    | 38'
  //      69            38->bb_merge          \ /     \ /
  //      \             /                     new_1    new_2
  //        \          /                       \      /
  //         \        /                           42
  //            65                              /    \
  //                                          5_1'   5_2'
  //                                         /  \   /  \
  //                                        39  |  51  |
  //                                       / \  |  |   |
  //                                       ... e1 ...  e2
  //                                       \ /  |  |   |
  //                                       64   |  60  |
  //                                       |    |  |   |
  //                                       69   |  69' |
  //                                        \  /   \  /
  //                                    '   new_1'   new_2'
  //                                         \      /
  //                                            65

  if (!Can_reorder_cond(WN_kid0(sc->Head()->Branch_wn()), NULL))
    return FALSE;

  SC_NODE * sc_then = sc->Find_kid_of_type(SC_THEN);
  SC_NODE * sc_else = sc->Find_kid_of_type(SC_ELSE);
  SC_NODE * sc_tmp;
  SC_NODE * sc_tmp1;
  SC_NODE * sc_tmp2;
  SC_LIST_ITER sc_list_iter;

  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init(sc_then->Kids())) {
    if ((sc_tmp->Type() == SC_IF) 
	&& (!Can_reorder_cond(WN_kid0(sc_tmp->Head()->Branch_wn()), NULL)
	    || (sc_tmp->Head()->Executable_stmt_count() != 1)
	    || !sc_tmp->Is_well_behaved()))
      return FALSE;
  }

  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init(sc_else->Kids())) {
    if ((sc_tmp->Type() == SC_IF) 
	&& (!Can_reorder_cond(WN_kid0(sc_tmp->Head()->Branch_wn()),NULL)
	    || (sc_tmp->Head()->Executable_stmt_count() != 1)
	    || !sc_tmp->Is_well_behaved()))
      return FALSE;
  }

  // sc's head should have no dependencies on sc_then and sc_else
  if (Has_dependency(sc_then, sc->Head())
      || Has_dependency(sc_else, sc->Head()))
    return FALSE;

  if ((WOPT_Enable_Pro_Loop_Limit >= 0)
      && (Transform_count() >= WOPT_Enable_Pro_Loop_Limit))
    return FALSE;

  if (_trace)
    printf("\n\t\t If-cond dist (SC%d)\n", sc->Id());

  BB_NODE * bb5 = sc->Head();
  BB_NODE * bb65 = sc->Merge();
  BB_NODE * bb_else_begin;
  BB_NODE * bb_else_end;
  BB_NODE * bb_then_begin;
  BB_NODE * bb_then_end;
  BB_NODE * bb_head;
  BB_NODE * bb_merge;
  BB_NODE * bb_merge_c;
  BB_NODE * bb5_1;
  BB_NODE * bb5_2;
  BB_NODE * bb_e1;
  BB_NODE * bb_e2;
  BB_NODE * bb_tmp;
  BB_NODE * bb_new1;
  BB_NODE * bb_new2;
  BB_NODE * bb_last;
  BB_NODE * bb_next;
  BB_LIST * bb_list_new;
  BB_LIST_ITER bb_list_iter;
  WN * wn_tmp;
  BB_IFINFO * info;
  FB_FREQ freq;
  FB_FREQ freq_total;
  FB_EDGE_TYPE ft_edge_type;
  FB_EDGE_TYPE br_edge_type;

  CFG * cfg = _cu->Cfg();
  MEM_POOL * pool = cfg->Mem_pool();

  if (cfg->Feedback()) {
    freq_total = cfg->Feedback()->Get_node_freq_out(bb5->Id());
    ft_edge_type = cfg->Feedback()->Get_edge_type(bb5->Id(), bb5->If_then()->Id());
    br_edge_type = cfg->Feedback()->Get_edge_type(bb5->Id(), bb5->If_else()->Id());
  }
  
  bb_next = NULL;
  bb_last = NULL;

  for (int i = 0; i < 2; i++) {
    sc_tmp = (i == 0) ? sc_then->First_kid() : sc_else->First_kid();

    while (sc_tmp) {
      switch (sc_tmp->Type()) {
      case SC_IF:
	bb_head = sc_tmp->Head();
	bb_merge = sc_tmp->Merge();
	bb_merge_c = NULL;
	bb_next = sc_tmp->Next_sibling()->Next_in_tree()->First_bb();
	cfg->Clone_bbs(bb_merge, bb_merge, &bb_merge_c, &bb_tmp, TRUE, 1.0);
	bb5_1 = NULL;
	bb5_2 = NULL;
	cfg->Clone_bbs(bb5, bb5, &bb5_1, &bb_tmp, TRUE, 1.0);
	cfg->Clone_bbs(bb5, bb5, &bb5_2, &bb_tmp, TRUE, 1.0);
	bb_then_begin = sc_tmp->Find_kid_of_type(SC_THEN)->First_bb();
	bb_then_end = sc_tmp->Find_kid_of_type(SC_THEN)->Last_bb();
	bb_else_begin = sc_tmp->Find_kid_of_type(SC_ELSE)->First_bb();
	bb_else_end = sc_tmp->Find_kid_of_type(SC_ELSE)->Last_bb();
	bb_new1 = cfg->Create_and_allocate_bb(bb_merge->Kind());
	bb_new2 = cfg->Create_and_allocate_bb(bb_merge->Kind());
	bb_e1 = cfg->Create_and_allocate_bb(BB_GOTO);
	bb_e2 = cfg->Create_and_allocate_bb(BB_GOTO);

	// Fix bb_head<->bb5_1, bb_head<->bb5_2,
	// Here-in-after " ->" denotes succ, "<-" denotes pred, "<->" denotes succ and pred.
	if (bb_else_begin)
	  bb_head->Replace_succ(bb_else_begin, bb5_1);
	else
	  bb_head->Replace_succ(bb_merge, bb5_1);

	bb_list_new = CXX_NEW(BB_LIST(bb_head), pool);
	bb5_1->Set_pred(bb_list_new);

	if (bb_then_begin) 
	  bb_head->Replace_succ(bb_then_begin, bb5_2);
	else
	  bb_head->Replace_succ(bb_merge, bb5_2);

	bb_list_new = CXX_NEW(BB_LIST(bb_head), pool);
	bb5_2->Set_pred(bb_list_new);

	// Fix branch label to BB5_1.
	wn_tmp = bb_head->Branch_wn();
	cfg->Add_label_with_wn(bb5_1);
	WN_label_number(wn_tmp) = bb5_1->Labnam();

	// Fix <-bb_merge, bb_then_end-> and <-bb_merge_c
	if (bb_then_end) {
	  bb_merge->Remove_pred(bb_then_end, pool);
	  bb_then_end->Replace_succ(bb_merge, bb_merge_c);
	  bb_list_new = CXX_NEW(BB_LIST(bb_then_end), pool);
	}
	else {
	  bb_merge->Remove_pred(bb_head, pool);
	  bb_list_new = CXX_NEW(BB_LIST(bb5_2), pool);
	}

	bb_merge_c->Set_pred(bb_list_new);

	// Fix bb_merge-> and bb_merge_c->
	bb_merge->Remove_succs(pool);
	bb_list_new = CXX_NEW(BB_LIST(bb_new1), pool);
	bb_merge->Set_succ(bb_list_new);
	bb_list_new = CXX_NEW(BB_LIST(bb_new2), pool);
	bb_merge_c->Set_succ(bb_list_new);

	// Fix 5_1-> and 5_2->
	bb_list_new = CXX_NEW(BB_LIST(bb_e1), pool);
	bb5_1->Set_succ(bb_list_new);
	bb_tmp = bb_else_begin ? bb_else_begin : bb_merge;
	if (i == 0)
	  bb5_1->Append_succ(bb_tmp, pool);
	else
	  bb5_1->Prepend_succ(bb_tmp, pool);

	bb_list_new = CXX_NEW(BB_LIST(bb_e2), pool);
	bb5_2->Set_succ(bb_list_new);
	bb_tmp = bb_then_begin ? bb_then_begin : bb_merge_c;
	if (i == 0)
	  bb5_2->Append_succ(bb_tmp, pool);
	else
	  bb5_2->Prepend_succ(bb_tmp, pool);

	wn_tmp = bb5_1->Branch_wn();
	if (i == 0)
	  bb_tmp = bb_e1;
	else
	  bb_tmp = (bb_else_begin ? bb_else_begin : bb_merge);

	cfg->Add_label_with_wn(bb_tmp);
	WN_label_number(wn_tmp) = bb_tmp->Labnam();

	wn_tmp = bb5_2->Branch_wn();
	if (i == 0)
	  bb_tmp = bb_e2;
	else
	  bb_tmp = (bb_then_begin ? bb_then_begin : bb_merge_c);

	cfg->Add_label_with_wn(bb_tmp);
	WN_label_number(wn_tmp) = bb_tmp->Labnam();

	// Fix <-bb_else_begin, and <-bb_then_begin
	if (bb_then_begin)
	  bb_then_begin->Replace_pred(bb_head, bb5_2);
	
	if (bb_else_begin)
	  bb_else_begin->Replace_pred(bb_head, bb5_1);

	// Fix <-new_1 and <-new_2
	bb_list_new = CXX_NEW(BB_LIST(bb_e1), pool);
	bb_new1->Set_pred(bb_list_new);
	if (i == 0)
	  bb_new1->Append_pred(bb_merge, pool);
	else
	  bb_new1->Prepend_pred(bb_merge, pool);

	bb_list_new = CXX_NEW(BB_LIST(bb_e2), pool);
	bb_new2->Set_pred(bb_list_new);
	if (i == 0)
	  bb_new2->Append_pred(bb_merge_c, pool);
	else
	  bb_new2->Prepend_pred(bb_merge_c, pool);

	// Fix <-e1->  and <-e2->
	bb_list_new = CXX_NEW(BB_LIST(bb5_1), pool);
	bb_e1->Set_pred(bb_list_new);
	bb_list_new = CXX_NEW(BB_LIST(bb_new1), pool);
	bb_e1->Set_succ(bb_list_new);
	bb_list_new = CXX_NEW(BB_LIST(bb5_2), pool);
	bb_e2->Set_pred(bb_list_new);
	bb_list_new = CXX_NEW(BB_LIST(bb_new2), pool);
	bb_e2->Set_succ(bb_list_new);

	// Fix new_1-> and new_2->
	bb_list_new = CXX_NEW(BB_LIST(bb_next), pool);
	bb_new1->Set_succ(bb_list_new);
	bb_list_new = CXX_NEW(BB_LIST(bb_next), pool);
	bb_new2->Set_succ(bb_list_new);

	if (cfg->Feedback()) {
	  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb_next->Pred())) {
	    cfg->Feedback()->Delete_edge(bb_tmp->Id(), bb_next->Id());
	  }
	}
	
	bb_next->Remove_preds(pool);
	bb_list_new = CXX_NEW(BB_LIST(bb_new1), pool);
	bb_next->Set_pred(bb_list_new);
	bb_next->Append_pred(bb_new2, pool);

	// Fix 4<->6
	if (sc_tmp == sc_then->First_kid()) {
	  FOR_ALL_ELEM(bb_tmp, bb_list_iter, Init(bb5->Pred())) {
	    bb_tmp->Replace_succ(bb5, bb_head);
	    bb_head->Replace_pred(bb5, bb_tmp);
	    if (cfg->Feedback()) 
	      cfg->Feedback()->Move_edge_dest(bb_tmp->Id(), bb5->Id(), bb_head->Id());
	  }
	  bb_head->Set_prev(bb5->Prev());
	  bb5->Prev()->Set_next(bb_head);
	}
	
	bb_head->Set_next(bb5_2);
	bb5_2->Set_prev(bb_head);

	if (i == 1) {
	  bb5_2->Set_next(bb_e2);
	  bb_e2->Set_prev(bb5_2);
	  bb_tmp = bb_e2;
	}
	else
	  bb_tmp = bb5_2;

	if (bb_then_begin) {
	  bb_tmp->Set_next(bb_then_begin);
	  bb_then_begin->Set_prev(bb_tmp);
	  bb_then_end->Set_next(bb_merge_c);
	  bb_merge_c->Set_prev(bb_then_end);
	}
	else {
	  bb_tmp->Set_next(bb_merge_c);
	  bb_merge_c->Set_prev(bb_tmp);
	}

	if (i == 0) {
	  bb_merge_c->Set_next(bb_e2);
	  bb_e2->Set_prev(bb_merge_c);
	  bb_e2->Set_next(bb_new2);
	  bb_new2->Set_prev(bb_e2);
	}
	else {
	  bb_merge_c->Set_next(bb_new2);
	  bb_new2->Set_prev(bb_merge_c);
	}

	bb_new2->Set_next(bb5_1);
	bb5_1->Set_prev(bb_new2);

	if (i == 1) {
	  bb5_1->Set_next(bb_e1);
	  bb_e1->Set_prev(bb5_1);
	  bb_tmp = bb_e1;
	}
	else
	  bb_tmp = bb5_1;
	
	if (bb_else_begin) {
	  bb_tmp->Set_next(bb_else_begin);
	  bb_else_begin->Set_prev(bb_tmp);
	  bb_else_end->Set_next(bb_merge);
	  bb_merge->Set_prev(bb_else_end);
	}
	else {
	  bb_tmp->Set_next(bb_merge);
	  bb_merge->Set_prev(bb5_1);
	}

	if (i == 0) {
	  bb_merge->Set_next(bb_e1);
	  bb_e1->Set_prev(bb_merge);
	  bb_e1->Set_next(bb_new1);
	  bb_new1->Set_prev(bb_e1);
	}
	else {
	  bb_merge->Set_next(bb_new1);
	  bb_new1->Set_prev(bb_merge);
	}

	bb_new1->Set_next(bb_next);
	bb_next->Set_prev(bb_new1);

	info = bb_head->Ifinfo();
	info->Set_merge(bb_next);
	info->Set_then(bb5_2);
	info->Set_else(bb5_1);
	
	info = bb5_1->Ifinfo();
	info->Set_cond(bb5_1);
	info->Set_merge(bb_new1);
	bb_tmp = bb_else_begin ? bb_else_begin : bb_merge;
	info->Set_then((i == 0) ? bb_tmp : bb_e1);
	info->Set_else((i == 0) ? bb_e1 : bb_tmp);
	
	info = bb5_2->Ifinfo();
	info->Set_cond(bb5_2);
	info->Set_merge(bb_new2);
	bb_tmp = bb_then_begin ? bb_then_begin : bb_merge_c;
	info->Set_then((i == 0) ? bb_tmp : bb_e2);
	info->Set_else((i == 0) ? bb_e2 : bb_tmp);

	if (cfg->Feedback()) {
	  FB_FREQ freq1;
	  FB_FREQ freq2;
	  BB_IFINFO * ifinfo;
	  BB_NODE * bb_then;
	  BB_NODE * bb_else;

	  cfg->Feedback()->Add_node(bb_new1->Id());
	  cfg->Feedback()->Add_node(bb_new2->Id());
	  cfg->Feedback()->Add_node(bb_e1->Id());
	  cfg->Feedback()->Add_node(bb_e2->Id());
	  
	  if ((i == 0) && bb_next->Ifinfo()) {
	    ifinfo = bb_next->Ifinfo();
	    bb_then = ifinfo->Then();
	    bb_else = ifinfo->Else();
	    freq1 = cfg->Feedback()->Get_edge_freq(bb_next->Id(), bb_else->Id());
	    freq2 = cfg->Feedback()->Get_edge_freq(bb_next->Id(), bb_then->Id());
	  }
	  else if ((i == 1) && bb_last) {
	    ifinfo = bb_last->Ifinfo();
	    bb_then = ifinfo->Then();
	    bb_else = ifinfo->Else();
	    freq1 = cfg->Feedback()->Get_edge_freq(bb_else->Id(), bb_else->Ifinfo()->Then()->Id());
	    freq2 = cfg->Feedback()->Get_edge_freq(bb_then->Id(), bb_then->Ifinfo()->Then()->Id());
	  }
	  else {
	    freq1 = cfg->Feedback()->Get_node_freq_out(bb_head->Id());
	    freq2 = freq_total - freq1;
	    freq1 = freq2/2;
	    freq2 = freq1;
	  }

	  // Delete 5->bb_head
	  cfg->Feedback()->Delete_edge(bb5->Id(), bb_head->Id());
	  
	  // Add 5_1->e1, 5_2->e2, e1->new_1, e2->new_2
	  cfg->Feedback()->Add_edge(bb5_1->Id(), bb_e1->Id(), (i == 0) ? br_edge_type : ft_edge_type, freq1);
	  cfg->Feedback()->Add_edge(bb5_2->Id(), bb_e2->Id(), (i == 0) ? br_edge_type : ft_edge_type, freq2);
	  cfg->Feedback()->Add_edge(bb_e1->Id(), bb_new1->Id(), FB_EDGE_OUTGOING, freq1);
	  cfg->Feedback()->Add_edge(bb_e2->Id(), bb_new2->Id(), FB_EDGE_OUTGOING, freq2);

	  // Add 5_2->bb_then_begin, delete 6->bb_then_begin, 
	  // change bb_then_end->bb_merge to bb_then_end->bb_merge_c,
	  // Add bb_merge_c->new_2
	  if (bb_then_begin) {
	    freq = cfg->Feedback()->Get_edge_freq(bb_head->Id(), bb_then_begin->Id());
	    cfg->Feedback()->Add_edge(bb5_2->Id(), bb_then_begin->Id(), (i == 0) ? ft_edge_type : br_edge_type, freq);
	    cfg->Feedback()->Delete_edge(bb_head->Id(), bb_then_begin->Id());
	    cfg->Feedback()->Move_edge_dest(bb_then_end->Id(), bb_merge->Id(), bb_merge_c->Id());
	  }
	  else {
	    freq = cfg->Feedback()->Get_edge_freq(bb_head->Id(), bb_merge->Id());
	    cfg->Feedback()->Add_edge(bb5_2->Id(), bb_merge_c->Id(), (i == 0) ? ft_edge_type : br_edge_type, freq);
	    cfg->Feedback()->Delete_edge(bb_head->Id(), bb_merge->Id());
	  }

	  cfg->Feedback()->Add_edge(bb_merge_c->Id(), bb_new2->Id(), FB_EDGE_OUTGOING, freq);

	  // Add 6->5_2
	  cfg->Feedback()->Add_edge(bb_head->Id(), bb5_2->Id(), ft_edge_type, freq + freq2);
	  // Add new2->bb_next
	  cfg->Feedback()->Add_edge(bb_new2->Id(), bb_next->Id(), FB_EDGE_OUTGOING, freq + freq2);
	  
	  // Add 5_1->bb_else_begin, delete 6->bb_else_begin,
	  // add  bb_merge->new_1.
	  if (bb_else_begin) {
	    freq = cfg->Feedback()->Get_edge_freq(bb_head->Id(), bb_else_begin->Id());
	    cfg->Feedback()->Add_edge(bb5_1->Id(), bb_else_begin->Id(), (i == 0)? ft_edge_type : br_edge_type, freq);
	    cfg->Feedback()->Delete_edge(bb_head->Id(), bb_else_begin->Id());
	  }
	  else {
	    freq = cfg->Feedback()->Get_edge_freq(bb_head->Id(), bb_merge->Id());
	    cfg->Feedback()->Add_edge(bb5_1->Id(), bb_merge->Id(), (i == 0) ? ft_edge_type : br_edge_type, freq);
	    cfg->Feedback()->Delete_edge(bb_head->Id(), bb_merge->Id());
	  }

	  cfg->Feedback()->Add_edge(bb_merge->Id(), bb_new1->Id(), FB_EDGE_OUTGOING, freq);

	  // Add 6->5_1
	  cfg->Feedback()->Add_edge(bb_head->Id(), bb5_1->Id(), br_edge_type, freq + freq1);
	  // Add new_1->bb_next
	  cfg->Feedback()->Add_edge(bb_new1->Id(), bb_next->Id(), FB_EDGE_OUTGOING, freq + freq1);

	  // Delete 5->bb_head and bb_merge->bb65
	  cfg->Feedback()->Delete_edge(bb5->Id(), bb_head->Id());
	  cfg->Feedback()->Delete_edge(bb_merge->Id(), bb65->Id());
	}
         
        // From:
	//                   SC10(THEN)
	//         /             |          \
	//      SC11(IF,BB6)    SC65(BB38)  ...
        //    /             \
        // SC12(THEN)        SC14(ELSE)            
        //   |       \     /           \
        // SC13(BB7) ...  SC15(IF,BB8) SC64(BB37)
        //
        // To:
	//                                                SC10(THEN)
        //                                   /                 x         \
        //                                  /                  x          \
        //                                 /                   x           \
        //                               SC11(IF,BB6)         SC65(BB38)   ...
        //                        /                      \      	  
        //                       /                        \
        //                      /                          \
        //             SC12(THEN) <- sc_cur                  SC14(ELSE)	  <- sc_cur
        //       /              \                           /            \
        //	/		 \                         /              \
        // SCNEW(IF,BB5_2)   SCNEW(BBnew2)                SCNEW(IF,BB5_1) SCNEW(BBnew1)
        //   /             \                               /             \
        //  /               \                             /               \
        // SCNEW(THEN)     SCNEW(ELSE)                  SCNEW(THEN)        SCNEW(ELSE)
        // /        |  \            \               /         |         \            \
        ///         |   \            \             /          |          \            \
        //SC13(BB7) ... SCNEW(BB38') SCNEW(BBe2) SC15(IF,BB8) SC64(BB37) SCNEW(BB38) SCNEW(BBe1)
        {		       
	  SC_NODE * sc11 = sc_tmp;
	  SC_NODE * sc65 = sc11->Next_sibling();
	  int j ;

	  sc_tmp = sc65->Next_sibling();
	  sc65->Unlink();
	  sc65->Delete();	  

	  for (j = 0; j < 2; j++) {
	    SC_NODE * sc_new_bb_5 = cfg->Create_sc(SC_IF);
	    SC_NODE * sc_new_bb_new = cfg->Create_sc(SC_BLOCK);
	    SC_NODE * sc_new_then = cfg->Create_sc(SC_THEN);
	    SC_NODE * sc_new_else = cfg->Create_sc(SC_ELSE);
	    SC_NODE * sc_new_bb_38 = cfg->Create_sc(SC_BLOCK);
	    SC_NODE * sc_new_bb_e = cfg->Create_sc(SC_BLOCK);
	    SC_NODE * sc_cur = (j == 0) ? sc11->Find_kid_of_type(SC_THEN) :
	      sc11->Find_kid_of_type(SC_ELSE);
	    BB_NODE * bb_5 = (j == 0) ? bb5_2 : bb5_1;
	    BB_NODE * bb_new = (j == 0) ? bb_new2 : bb_new1;
	    BB_NODE * bb_38 = (j == 0) ? bb_merge_c : bb_merge;
	    BB_NODE * bb_e = (j == 0) ? bb_e2 : bb_e1;
	    
	    sc_new_bb_5->Set_bb_rep(bb_5);
	    sc_new_bb_new->Append_bbs(bb_new);
	    sc_new_bb_38->Append_bbs(bb_38);
	    sc_new_bb_e->Append_bbs(bb_e);

	    SC_NODE * sc_kid;
	    SC_NODE * sc_ft = (i == 0) ? sc_new_then : sc_new_else;
	    SC_NODE * sc_nft = (i == 0) ? sc_new_else : sc_new_then;
	    FOR_ALL_ELEM(sc_kid, sc_list_iter, Init(sc_cur->Kids())) {
	      sc_kid->Set_parent(sc_ft);
	    }
	    sc_ft->Set_kids(sc_cur->Kids());
	    sc_ft->Append_kid(sc_new_bb_38);
	    sc_new_bb_38->Set_parent(sc_ft);
	    sc_nft->Append_kid(sc_new_bb_e);
	    sc_new_bb_e->Set_parent(sc_nft);
	    sc_new_bb_5->Append_kid(sc_new_then);
	    sc_new_then->Set_parent(sc_new_bb_5);
	    sc_new_bb_5->Append_kid(sc_new_else);
	    sc_new_else->Set_parent(sc_new_bb_5);
	    sc_cur->Set_kids(NULL);
	    sc_cur->Append_kid(sc_new_bb_5);
	    sc_new_bb_5->Set_parent(sc_cur);
	    sc_cur->Append_kid(sc_new_bb_new);
	    sc_new_bb_new->Set_parent(sc_cur);
	  }
	}

	bb_last = bb_head;
	break;
      default:
	FmtAssert(FALSE, ("TODO"));
	sc_tmp = sc_tmp->Next_sibling();
      }
    }
  }

  // From:
  //                         SC8
  //                   /            \
  //         SC9(IF, BB5)            SC112(BB65)
  //      /               \
  // 	 SC10(THEN)       SC66(ELSE)
  //     |                |
  //     SC11(IF,bb6)     SC73(IF,BB42)
  //
  // To:
  //                        SC8
  //            /            |            \ 				
  //        SC11(IF,bb6)  SC73(IF,BB42)  SC112(BB65)

  SC_NODE * sc9 = sc;
  SC_NODE * sc8 = sc9->Parent();
  SC_NODE * sc10 = sc9->Find_kid_of_type(SC_THEN);
  SC_NODE * sc66 = sc9->Find_kid_of_type(SC_ELSE);
  SC_NODE * sc_insert = sc9->Prev_sibling();

  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init(sc66->Kids())) {
    sc10->Append_kid(sc_tmp);
  }

  sc_tmp1 = sc9->Next_sibling();
  while (sc_tmp1) {
    sc_tmp2 = sc_tmp1;
    sc10->Append_kid(sc_tmp2);
    sc_tmp1 = sc_tmp2->Next_sibling();
    sc_tmp2->Unlink();
  }

  sc9->Unlink();
  _unlink_sc->Push(sc9);
  
  if (sc_insert == NULL)
    sc8->Set_kids(sc10->Kids());

  FOR_ALL_ELEM(sc_tmp, sc_list_iter, Init(sc10->Kids())) {
    if (sc_insert) {
      sc_insert->Insert_after(sc_tmp);
      sc_insert = sc_tmp;
    }
    else
      sc_tmp->Set_parent(sc8);      
  }

  sc66->Set_kids(NULL);
  sc10->Set_kids(NULL);  

  cfg->Fix_info(sc_p);

  if (sc_prev)
    cfg->Fix_info(sc_prev);

  cfg->Invalidate_and_update_aux_info(FALSE);
  cfg->Invalidate_loops();
  Inc_transform_count();

  return TRUE;
}

// Automation of proactive loop interchange for the given loop-nest.
int
PRO_LOOP_INTERCHANGE_TRANS::Nonrecursive_trans
(SC_NODE * outer_loop, SC_NODE * inner_loop)
{
  BOOL success = TRUE;
  int ret_val = 0;

  SC_NODE * cur_node = inner_loop;
  SC_NODE * parent_node = cur_node->Get_real_parent();
  SC_NODE * last_node;
  SC_NODE * sc_tmp;

  while (parent_node != outer_loop) {
    BOOL is_invariant = FALSE;

    last_node = cur_node;

    // Find the outermost if-condition that is loop invariant w.r.t. the outer loop.
    while ((parent_node != outer_loop) && Is_invariant(outer_loop, parent_node->Head(), 0)) {
      FmtAssert((parent_node->Type()  == SC_IF), ("Expect a SC_IF"));
      cur_node = parent_node;
      parent_node = cur_node->Get_real_parent();
      is_invariant = TRUE;
    }
    
    // Find the outermost if-condition that is NOT loop invariant w.r.t. the outer loop.
    if (!is_invariant) {
      while ((parent_node != outer_loop) && !Is_invariant(outer_loop, parent_node->Head(), 0)) {
	FmtAssert((parent_node->Type()  == SC_IF), ("Expect a SC_IF"));
	cur_node = parent_node;
	parent_node = cur_node->Get_real_parent();
      }
    }

    if (is_invariant) {
      if (cur_node != last_node->Get_real_parent()) {
	if ((_action & DO_TREE_HEIGHT_RED) != 0) {
	  Do_canon(outer_loop, inner_loop, HEAD_DUP | TAIL_DUP);
	  
	  // Reduce height of if-condition tree to 1.
	  if (Do_if_cond_tree_height_reduction(cur_node, last_node->Parent())) {
	    cur_node = inner_loop;
	    sc_tmp = last_node->Get_real_parent();
	    FmtAssert((sc_tmp->Type() == SC_IF), ("Expect a SC_IF"));
	    Hash_invar(sc_tmp->Head(), outer_loop);

	    if ((ret_val & DO_TREE_HEIGHT_RED) == 0)
	      ret_val |= DO_TREE_HEIGHT_RED;
	  }
	  else
	    success = FALSE;
	}
      }
      else if (parent_node == outer_loop) {
	if (outer_loop->Type() == SC_LOOP) {
	  if ((_action & DO_LOOP_UNS) != 0) {
	    Do_canon(outer_loop, inner_loop, HEAD_DUP | TAIL_DUP);
	    SC_NODE * sc_p = outer_loop->Parent();

	    // Do loop unswitching w.r.t. the outer loop and terminate.
	    if (Do_loop_unswitching(cur_node, outer_loop)) {
	      IF_MERGE_TRANS::Top_down_trans(sc_p);	
	      
	      if ((ret_val & DO_LOOP_UNS) == 0)
		ret_val |= DO_LOOP_UNS;
	      break;
	    }
	    else
	      success = FALSE;
	  }
	}
      }
    }
    else if ((last_node->Type() == SC_IF)
	     && Is_invariant(outer_loop, last_node->Head(), 0)) {
      if ((_action & DO_IF_COND_DIS) != 0) {
	Do_canon(outer_loop, inner_loop, HEAD_DUP | TAIL_DUP);

	if (Do_if_cond_dist(last_node->Get_real_parent())) {
	  IF_MERGE_TRANS::Top_down_trans(outer_loop);
	  cur_node = inner_loop;

	  if ((ret_val & DO_IF_COND_DIS) == 0)
	    ret_val |= DO_IF_COND_DIS;
	}
	else
	  success = FALSE;
      }
    }
    else if (parent_node == outer_loop) {
      if ((_action & DO_REV_LOOP_UNS) != 0) {
	Do_canon(outer_loop, inner_loop, HEAD_DUP | TAIL_DUP);
	
	// Do a reversed loop unswitching for the inner loop.
	if (Do_reverse_loop_unswitching(inner_loop->Get_real_parent(), inner_loop, outer_loop)) {
	  cur_node = inner_loop;
	  
	  if ((ret_val & DO_REV_LOOP_UNS) == 0)
	    ret_val |= DO_REV_LOOP_UNS;
	}
	else
	  success = FALSE;
      }
    }

    if (!success)
      break;

    parent_node = cur_node->Get_real_parent();
  }

  return ret_val;
}

// Reset/clear fields.
void
PRO_LOOP_TRANS::Clear(void)
{
  PRO_LOOP_FUSION_TRANS::Clear();
  PRO_LOOP_INTERCHANGE_TRANS::Clear();
}

// Delete dynamically allocated objects for all sub-objects.
void
PRO_LOOP_TRANS::Delete(void)
{
  CFG_TRANS::Delete();
  PRO_LOOP_INTERCHANGE_TRANS::Delete();
}

// Delete maps for value ranges.
void
CFG_TRANS::Delete_val_range_maps()
{
  if (_low_map != -1)
    WN_MAP_Delete(_low_map);

  _low_map = -1;

  if (_high_map != -1)
    WN_MAP_Delete(_high_map);

  _high_map = -1;

  if (_def_wn_map)
    CXX_DELETE(_def_wn_map, _pool);

  _def_wn_map = NULL;
}

// Delete dynamically allocated objects.
void
CFG_TRANS::Delete()
{
  if (_const_wn_map) {
    MAP_LIST * tmp;
    MAP_LIST_ITER map_lst_iter;

    for (UINT32 idx = 0; idx < _const_wn_map->Size(); idx++) {
      FOR_ALL_NODE(tmp, map_lst_iter, Init(_const_wn_map->Get_bucket(idx))) {
	WN * wn = (WN *) tmp->Val();
	if (wn)
	  WN_Delete(wn);
      }
    }
  }

  CXX_DELETE(_const_wn_map, _pool);
  _const_wn_map = NULL;
}

// Get a WHIRL that represents an integer constant of the given value
// from _const_wn_map.
WN *
CFG_TRANS::Get_const_wn(long long val)
{
  if (!_const_wn_map)
    _const_wn_map = CXX_NEW(MAP(CFG_BB_TAB_SIZE, _pool), _pool);

  WN * wn = (WN *) _const_wn_map->Get_val((POINTER) val);
  
  if (!wn) {
    MAP_LIST * map_lst = _const_wn_map->Find_map_list((POINTER) val);

    if (map_lst) {
      wn = WN_CreateIntconst(OPR_INTCONST, MTYPE_I8, MTYPE_V, val);
      map_lst->Set_val((POINTER) wn);
    }
  }

  return wn;
}

// Set up map of wn_key to wn_val.  Update _def_wn_map is necessary.
void
CFG_TRANS::Set_map(WN_MAP & map, WN * wn_key, WN * wn_val)
{
  WN_MAP_Set(map, wn_key, wn_val);

  OPERATOR opr = WN_operator(wn_key);
  
  if (OPERATOR_is_scalar_load(opr) || OPERATOR_is_scalar_store(opr)) {
    WN * wn_def = (WN *) _def_wn_map->Get_val((POINTER) WN_aux(wn_key));

    if (!wn_def) {
      MAP_LIST * map_lst = _def_wn_map->Find_map_list((POINTER) WN_aux(wn_key));
      
      if (map_lst)
	map_lst->Set_val((POINTER) wn_key);
    }
  }
}

// Infer value range for the given WN *.  set_high and set_low indicates
// whether to set low boundary or high boundary respectively.
void
CFG_TRANS::Infer_val_range(WN * wn, BOOL set_high, BOOL set_low)
{
  OPERATOR opr = WN_operator(wn);

  if (OPERATOR_is_scalar_store(opr)) {
    WN * wn_data = WN_kid(wn, 0);

    if (set_low)
      Set_map(_low_map, wn, wn_data);

    if (set_high)
      Set_map(_high_map, wn, wn_data);
  }
  else {
    WN * op1;
    WN * op2;
    WN * wn_tmp;
    int val;
    int val2;

    switch (opr) {
    case OPR_GT:
      op1 = WN_kid(wn, 0);
      op2 = WN_kid(wn, 1);

      if (WN_get_val(op2, &val, _low_map)) {
	Set_map(_low_map, op1, op2);
	Infer_val_range(op1, TRUE, TRUE);
      }

      break;

    case OPR_LT:
      op1 = WN_kid(wn, 0);
      op2 = WN_kid(wn, 1);

      if (WN_get_val(op1, &val, _low_map)) {
	Set_map(_low_map, op2, op1);
	Infer_val_range(op2, TRUE, TRUE);
      }
      break;

    case OPR_ADD:
      op1 = WN_kid(wn, 0);
      op2 = WN_kid(wn, 1);

      if (WN_operator(op2) == OPR_INTCONST) {
	if (_low_map) {
	  wn_tmp = (WN *) WN_MAP_Get(_low_map, wn);

	  if (wn_tmp && WN_get_val(wn_tmp, &val, _low_map)) {
	    long long new_val = -1 * WN_const_val(op2) + val;
	    wn_tmp = Get_const_wn(new_val);
	    Set_map(_low_map, op1, wn_tmp);
	    Infer_val_range(op1, TRUE, TRUE);
	  }
	}
	
	if (_high_map) {
	  wn_tmp = (WN *) WN_MAP_Get(_high_map, wn);

	  if (wn_tmp && WN_get_val(wn_tmp, &val, _high_map)) {
	    long long new_val = -1 * WN_const_val(op2) + val;
	    wn_tmp = Get_const_wn(new_val);
	    Set_map(_high_map, op1, wn_tmp);
	    Infer_val_range(op1, TRUE, TRUE);
	  }
	}
      }

      break;

    case OPR_SUB:

      op1 = WN_kid(wn, 0);
      op2 = WN_kid(wn, 1);

      if (_low_map) {
	wn_tmp = (WN * ) WN_MAP_Get(_low_map, wn);

	if (wn_tmp && WN_get_val(wn_tmp, &val, _low_map)) {
	  wn_tmp = (WN *) WN_MAP_Get(_low_map, op2);

	  if (wn_tmp && WN_get_val(op2, &val2, _low_map)) {
	    long long new_val = val2 + val;
	    wn_tmp = Get_const_wn(new_val);
	    Set_map(_low_map, op1, wn_tmp);
	    Infer_val_range(op1, TRUE, TRUE);
	  }
	}
      }

      if (_high_map) {
	wn_tmp = (WN * ) WN_MAP_Get(_high_map, wn);

	if (wn_tmp && WN_get_val(wn_tmp, &val, _high_map)) {
	  wn_tmp = (WN *) WN_MAP_Get(_high_map, op2);
	  
	  if (wn_tmp && WN_get_val(op2, &val2, _high_map)) {
	    long long new_val = val2 + val;
	    wn_tmp = Get_const_wn(new_val);
	    Set_map(_high_map, op1, wn_tmp);
	    Infer_val_range(op1, TRUE, TRUE);
	  }
	}
      }

      break;

    default:
      ;
    }
  }
}

// Given a pair of SC_NODE *, infer value ranges for variables appearing in nesting loops'
// initialization blocks and condition-testing blocks.
void
CFG_TRANS::Infer_val_range(SC_NODE * sc1, SC_NODE * sc2)
{
  SC_NODE * sc_lcp = sc1->Find_lcp(sc2);

  if (_invar_map) {
    _low_map = WN_MAP_Create(_pool);
    _high_map = WN_MAP_Create(_pool);
    _def_wn_map = CXX_NEW(MAP(CFG_BB_TAB_SIZE, _pool), _pool);

    while (sc_lcp) {
      if ((sc_lcp->Type() == SC_LOOP) 
	  && sc_lcp->Loopinfo()->Is_flag_set(LOOP_PRE_DO)) {
	SC_NODE * sc_body = sc_lcp->Find_kid_of_type(SC_LP_BODY);
	SC_NODE * sc_cond = sc_lcp->Find_kid_of_type(SC_LP_COND);
	SC_NODE * sc_init = sc_lcp->Find_kid_of_type(SC_LP_START);
	SC_NODE * sc_step = sc_lcp->Find_kid_of_type(SC_LP_STEP);
	BB_NODE * bb_body = sc_body->First_bb();
	BB_NODE * bb_cond = sc_cond->First_bb();
	BB_NODE * bb_init = sc_init->First_bb();
	BB_NODE * bb_step = sc_step->First_bb();

	if (bb_init && bb_cond && bb_body && bb_step
	    && !bb_cond->Is_branch_to(bb_body)) {
	  BOOL set_high = FALSE;
	  BOOL set_low = FALSE;
	  WN * wn_init = bb_init->Laststmt();
	  WN * wn_step = bb_step->Laststmt();
	  wn_step = WN_prev(wn_step);
	  WN * wn_add = wn_step ? WN_kid(wn_step, 0) : NULL;
	  WN * wn_load = wn_add ? WN_kid(wn_add, 0) : NULL;
	  
	  if (wn_step && OPERATOR_is_scalar_store(WN_operator(wn_step))
	      && wn_init && OPERATOR_is_scalar_store(WN_operator(wn_init))
	      && (WN_aux(wn_init) == WN_aux(wn_step))
	      && wn_load && OPERATOR_is_scalar_load(WN_operator(wn_load))
	      && (WN_aux(wn_load) == WN_aux(wn_step))
	      && (WN_operator(wn_add) == OPR_ADD)) {
	    int val;
	    if (WN_get_val(WN_kid(wn_add,1), &val, _low_map)) {
	      if (val > 0)
		set_low = TRUE;
	      else
		set_high = TRUE;
	    }
	  }
	  
	  if (set_high || set_low) {
	    if (Is_invariant(sc_body, bb_init, 0)) 
	      Infer_val_range(wn_init, set_high, set_low);
	    
	    WN * wn_cond = bb_cond->Laststmt();

	    if (wn_cond && (WN_operator(wn_cond) == OPR_FALSEBR)
		&& Is_invariant(sc_body, bb_cond, 0)) {
	      wn_cond = WN_kid(wn_cond, 0);
	      Match_def(wn_cond);
	      Infer_val_range(wn_cond, TRUE, TRUE);
	    }
	  }
	}
      }

      sc_lcp = sc_lcp->Parent();
    }
  }
}

// For scalar loads in the given WHIRL tree, match definitions and
// set up value range maps.
void
CFG_TRANS::Match_def(WN * wn)
{
  if (OPERATOR_is_scalar_load(WN_operator(wn))) {
    if (_def_wn_map) {
      WN * wn_def = (WN *) _def_wn_map->Get_val((POINTER) WN_aux(wn));

      if (wn_def) {
	if (_low_map) 
	  WN_MAP_Set(_low_map, wn, wn_def);

	if (_high_map) 
	  WN_MAP_Set(_high_map, wn, wn_def);
      }
    }
  }
  
  for (int i = 0; i < WN_kid_count(wn); i++)
    Match_def(WN_kid(wn,i));
}
