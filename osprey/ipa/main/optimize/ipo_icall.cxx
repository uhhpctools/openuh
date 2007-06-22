/*
 * Copyright 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

/* -*-Mode: c++;-*- (Tell emacs to use c++ mode) */
// ====================================================================
// ====================================================================
//
// Module: ipo_icall.cxx
//
// Revision history:
//  02-Jan-2006 - Original Version
//
// Description:  Interprocedural Optimization of Indirect Calls
//
// ====================================================================
// ====================================================================

#define __STDC_LIMIT_MACROS
#include <stdint.h>

#include "ipo_defs.h"
#include "ipo_inline.h"
#include "ipo_alias.h"
#include "ipo_alias_class.h"
#include "ipo_parent.h"

#include <deque>
#include <queue>

struct order_by_callsite_id
{
    bool operator() (IPA_EDGE * first, IPA_EDGE * second)
    {
    	return first->Callsite_Id() > second->Callsite_Id();
    }
};

static BOOL
Is_Return_Store_Stmt (WN * wn)
{
  if (wn && WN_operator (wn) == OPR_STID)
  {
    WN *val = WN_kid (wn, 0);
    if (WN_operator (val) == OPR_LDID)
    {
      ST *st = WN_st (val);
      if (ST_sym_class (st) == CLASS_PREG
          && (st == Return_Val_Preg))
        return TRUE;
    }
  }
  return FALSE;
}

// ======================================================================
// wn: icall
// edge: edge added for call generated in icall transformation.
// Do the actual icall transformation (i.e. generation of if-else stmt)
// Set WN node in edge.
// ======================================================================
static void
Convert_Icall (WN * wn, IPA_EDGE * edge)
{
  IPA_NODE * caller = IPA_Call_Graph->Caller (edge);
  IPA_NODE * callee = IPA_Call_Graph->Callee (edge);
  ST * st_callee = callee->Func_ST();
  TY_IDX ty_callee = ST_pu_type (st_callee);

  // Address of a possible callee.
  WN* tmpkid0 = WN_CreateLda (Use_32_Bit_Pointers ? OPC_U4LDA : OPC_U8LDA,
                                  0, Make_Pointer_Type(ty_callee), st_callee);
  // Pointer from indirect call
  WN* tmpkid1 = WN_COPY_Tree_With_Map(WN_kid (wn, WN_kid_count(wn) - 1));

  // If the pointer is pointing to the callee, ...
  WN* test = WN_Create (Use_32_Bit_Pointers ? OPC_U4U4EQ : OPC_U8U8EQ, 2);

  WN_kid0(test) = tmpkid0;
  WN_kid1(test) = tmpkid1;

  // Then, call "that" callee.
  WN* if_then = WN_Create (WN_opcode(wn), WN_kid_count(wn) - 1);
  WN* if_then_block = WN_CreateBlock();
  WN_set_operator (if_then, OPR_CALL);

  // Set wn for the new edge.
  edge->Set_Whirl_Node (if_then);

  // Arguments for direct call
  for (int i = 0; i < WN_kid_count (if_then); i++)
    WN_kid (if_then, i) = WN_COPY_Tree_With_Map (WN_kid (wn, i));

  WN_st_idx(if_then) = ST_st_idx(st_callee);

  WN_INSERT_BlockLast (if_then_block, if_then);

  // Else, do the indirect call.
  WN* if_else = WN_COPY_Tree_With_Map (wn);
  WN* if_else_block = WN_CreateBlock();
  WN_INSERT_BlockLast (if_else_block, if_else);

  WN * block = WN_Get_Parent (wn, caller->Parent_Map(), caller->Map_Table());
  // Put return stid statements in both branches of IF.
  for (WN* stmt = WN_next(wn);
           stmt != NULL && Is_Return_Store_Stmt (stmt);)
  {
    WN_INSERT_BlockLast (if_then_block, WN_COPY_Tree(stmt));
    WN_INSERT_BlockLast (if_else_block, WN_COPY_Tree(stmt));

    // Empty the stmt
    WN * ret_wn = stmt;
    stmt = WN_next (stmt);

    WN_EXTRACT_FromBlock (block, ret_wn);
  }

  WN_Parentize (if_then_block, caller->Parent_Map(), caller->Map_Table());
  WN_Parentize (if_else_block, caller->Parent_Map(), caller->Map_Table());

  WN* wn_if = WN_CreateIf (test, if_then_block, if_else_block);
  // ************* TEMPORARY WORKAROUND ************************
  // Why does not IPA have feedback at -O0/-O1?
  // We reach here actually means the compilation has feedback.
  if (Cur_PU_Feedback)
  {
    Cur_PU_Feedback->FB_lower_icall (wn, if_else, if_then, wn_if);

    // Delete the map info. We delete it from <Cur_PU_Feedback>
    Cur_PU_Feedback->Delete(wn);
  }

  // Replace wn with call_wn.
  WN_INSERT_BlockAfter (block, wn, wn_if);
  WN_EXTRACT_FromBlock (block, wn);
  WN_Parentize (block, caller->Parent_Map(), caller->Map_Table());
} // Convert_Icall

// ======================================================================
// Top-level function for icall-transformation.
// Called only for functions with icall-opt opportunity. Map_Callsites()
// must be called for other functions.
//
// Maps outgoing edges to call nodes. Adds call nodes when necessary,
// but still mainitaining the edge->call mapping.
// ======================================================================
void
IPO_Process_Icalls (IPA_NODE * node)
{
  FmtAssert (!node->Is_Visited(),
             ("Node is getting visited NOT for the first time"));

  node->Set_Visited();

  if (node->Total_Succ() == 0) return;

  // deque: constant time removal of elements from the front.
  std::deque<WN*> callsite_map;
  // Maintain elements in decreasing order of callsite id.
  std::priority_queue<IPA_EDGE *,
                      vector<IPA_EDGE *>,
                      order_by_callsite_id> edge_order;

  // Get the existing callsites ACTUALLY present in code.
  for (WN_ITER* wni = WN_WALK_TreeIter(node->Whirl_Tree(FALSE));
       wni != NULL;
       wni = WN_WALK_TreeNext(wni))
  {
    WN* wn = WN_ITER_wn (wni);

    switch (WN_operator(wn))
    {
      case OPR_CALL:
          if (WN_opcode(wn) == OPC_VCALL &&
              WN_Fake_Call_EH_Region (wn, Parent_Map))
          break;
          // fall through
      case OPR_ICALL:
      case OPR_INTRINSIC_CALL:
          callsite_map.push_back (wn);
          break;
    }
  }

  IPA_SUCC_ITER succ_iter (node);

  // Edges in decreasing order of callsite-id
  for (succ_iter.First(); !succ_iter.Is_Empty(); succ_iter.Next())
  {
    IPA_EDGE *edge = succ_iter.Current_Edge();
    if (edge)
      edge_order.push (edge);
  }

  // The following counter helps in maintaining the mapping between
  // CALL/ICALL WN nodes and call graph edges.
  //
  // Detects cases where 
  // 1. there are WN nodes but no corresponding edges, like
  //      intrinsic calls, icalls not being converted.
  //        Ignore such WN nodes.
  // 2. there are edges but no WN nodes, like
  //      edges added for icall-opt. 
  //        In such a scenario, either add WN or delete edge from call graph.
  UINT32 callsite_idx = 0;
  while (!edge_order.empty())
  {
    // Edge with the lowest callsite id.
    IPA_EDGE * edge = edge_order.top();
    edge_order.pop();

    // Remove WN nodes for which we don't have an edge
    for (; callsite_idx < edge->Callsite_Id(); callsite_idx++)
      callsite_map.pop_front();

    FmtAssert (callsite_idx == edge->Callsite_Id(),
               ("IPO_Process_Icalls: Invalid callsite index"));

    WN * w = callsite_map.front();
    if (WN_operator (w) == OPR_CALL)
      edge->Set_Whirl_Node (w);
    else // do icall optimization
    {
      FmtAssert (WN_operator (w) == OPR_ICALL,
                 ("IPO_Process_Icalls: Expected ICALL"));
      IPA_EDGE * next_edge = (!edge_order.empty()) ?
                        edge_order.top() : NULL;

      if (edge->Summary_Callsite()->Is_func_ptr())
      { // We decided not to do icall conversion, but IPA data flow
        // has the answer.
        edge->Set_Whirl_Node (w);
      }
      else if (next_edge && next_edge->Summary_Callsite()->Is_func_ptr() &&
               (edge->Callsite_Id() + 1 == next_edge->Callsite_Id()))
      {
        // Delete dummy edge because IPA data flow has resolved the
        // function pointer to the actual function, no need to guess
        // any more.
        IPA_Call_Graph->Graph()->Delete_Edge (edge->Edge_Index());
        callsite_idx++; // account for callsite in deleted edge

        // next_edge should be the edge created by IPA data flow
        next_edge->Set_Whirl_Node (w);
        edge_order.pop();
      }
      else
      {
        // Do the actual transformation, set WN node in edge.
        Convert_Icall (w, edge);
        callsite_idx++;  // for the call added
      }
    }

    callsite_map.pop_front();
    callsite_idx++; // for the popped node
  }
} // IPO_Process_Icalls
