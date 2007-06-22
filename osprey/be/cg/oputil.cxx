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
 * Module: oputil.c
 * $Revision: 1.1.1.1 $
 * $Date: 2005/10/21 19:00:00 $
 * $Author: marcel $
 * $Source: /proj/osprey/CVS/open64/osprey1.0/be/cg/oputil.cxx,v $
 *
 * Revision history:
 *  12-Oct-89 - Original Version
 *  01-Feb-91 - Copied for TP/Muse
 *  12-Jun-91 - Removed INS/INSCH stuff to insutil.c
 *  12-Jun-91 - Added OP insertion/deletion stuff from bbutil.c
 *
 * Description:
 *
 * Utility routines for manipulating the CGIR OP and OPS data
 * structures.  Also implements a few routines that manipulate BBs as
 * well since the BB implementation is intrinsically intertwined with
 * the OP implementation.  See "op.h" and "bb.h" for interfaces.
 *
 * TODO: Combine "op.h" and "bb.h" into "cgir.h", and "oputil.cxx" and
 *       "bbutil.cxx" into "cgir.cxx".
 *
 * ====================================================================
 * ==================================================================== */

#ifdef USE_PCH
#include "cg_pch.h"
#endif // USE_PCH
#pragma hdrstop

#include <stdarg.h>

#include "defs.h"
#include "config.h"
#include "tracing.h"
#include "erglob.h"
#include "printsrc.h"

#include "import.h"
#include "opt_alias_interface.h"        /* for Print_alias_info */
#include "cg.h"                         /* for Alias_Manager */

#include "cgir.h"
#include "cg.h"
#include "register.h"
#include "cg_dep_graph.h"
#include "cgprep.h"
#include "cg_loop.h"
#include "cgtarget.h"

#ifdef TARG_IA64
#include "targ_sim.h"
#endif

#include "wn.h"
#include "whirl2ops.h"
#include "cgexp.h"
#include "xstats.h"
#include "tag.h"

/* Allocate OPs for the duration of the PU. */
#define OP_Alloc(size)  ((OP *)Pu_Alloc(size))


/* OP mutators that are NOT to be made public */
#define Set_OP_code(o,opc)	((o)->opr = (mTOP)(opc))
#define Set_OP_opnds(o,n)	((o)->opnds = (n))
#define Set_OP_results(o,n)	((o)->results = (n))

#ifdef TARG_IA64
BOOL
OP_xfer(OP *op) 
{
  if(TOP_is_xfer(OP_code(op))) 
    return TRUE;

  if(OP_chk(op)){	  
    BB* home_bb = OP_bb(op);
    if(!home_bb) 
        return FALSE;
    if(BB_succs_len(home_bb) != 2) 
        return FALSE;
    if(op != BB_last_op(home_bb)) 
        return FALSE;    
    return TRUE;
  }
  return FALSE;
}


/* -----------------------------------------------------------------------
 * check if an operation that restores b0 register
 * -----------------------------------------------------------------------
 */
BOOL
OP_restore_b0(OP *op)
{
  if(OP_results(op) != 1 || OP_call(op)) return FALSE;
  TN* res_tn = OP_result(op, 0);
  if(TN_is_constant(res_tn)) return FALSE;
  return (TN_register_class(res_tn) == ISA_REGISTER_CLASS_branch && TN_register(res_tn) == REGISTER_MIN+0);
}

/* -----------------------------------------------------------------------
 * check if an operation that restores ar.pfs register
 * -----------------------------------------------------------------------
 */
BOOL
OP_restore_ar_pfs(OP *op)
{
  if(OP_results(op) != 1) return FALSE;
  TN* res_tn = OP_result(op, 0);
  if(TN_is_constant(res_tn)) return FALSE;
  return TN_is_pfs_reg(res_tn);
}

/* -----------------------------------------------------------------------
 * check if an operation that restores ar.lc register
 * -----------------------------------------------------------------------
 */
BOOL
OP_def_ar_lc(OP *op)
{
  if(OP_results(op) != 1) return FALSE;
  TN* res_tn = OP_result(op, 0);
  if(TN_is_constant(res_tn)) return FALSE;
  return TN_is_lc_reg(res_tn);
}
#endif

// ----------------------------------------
// Copy ASM_OP_ANNOT when duplicating an OP
// ----------------------------------------
static inline void
Copy_Asm_OP_Annot(OP* new_op, OP* op) 
{
  if (OP_code(op) == TOP_asm) {
    OP_MAP_Set(OP_Asm_Map, new_op, OP_MAP_Get(OP_Asm_Map, op));
  }
}

#ifdef TARG_IA64
static inline void
Copy_GOT_Sym_Info (OP* new_op, OP* op) {
  if (OP_load_GOT_entry(op)){
     OP_MAP_Set (OP_Ld_GOT_2_Sym_Map, 
                new_op, 
		OP_MAP_Get (OP_Ld_GOT_2_Sym_Map, op));
  }
}
#endif

/* ====================================================================
 *
 * New_OP
 *
 * Create and clear a new OP structure.
 *
 * ====================================================================
 */

#ifdef TARG_IA64
static OP *
New_OP ( INT results, INT opnds, INT hidden_opnds)
{ 
  OP *op = OP_Alloc (OP_sizeof (results, opnds+hidden_opnds));
  PU_OP_Cnt++;
  Set_OP_opnds(op, opnds);
  Set_OP_results(op, results);
  return op;
}
#else
static OP *
New_OP ( INT results, INT opnds )
{
  OP *op = OP_Alloc ( OP_sizeof(results, opnds) );
  PU_OP_Cnt++;
  Set_OP_opnds(op, opnds);
  Set_OP_results(op, results);
  return op;
}
#endif

/* ====================================================================
 *
 * Dup_OP
 *
 * Create a new OP structure as a duplicate of another, with zero ID.
 *
 * ====================================================================
 */

OP *
Dup_OP ( OP *op )
{
  INT results = OP_results(op);
  INT opnds = OP_opnds(op);
#ifdef TARG_IA64
  INT hidden_opnds = CGTARG_Max_Number_of_Hidden_Opnd (OP_code(op)); 
  OP *new_op = New_OP (results, opnds, hidden_opnds);
#else
  OP *new_op = New_OP ( results, opnds );
#endif
  memcpy(new_op, op, OP_sizeof(results, opnds));
  new_op->next = new_op->prev = NULL;
  new_op->bb = NULL;

  Copy_Asm_OP_Annot ( new_op, op );
#ifdef TARG_IA64
  if (OP_load_GOT_entry(op)) {
    Copy_GOT_Sym_Info (new_op, op);
  }
#endif

  if (OP_has_tag(op)) {
	Set_OP_Tag (new_op, Gen_Tag());
  }


#ifdef TARG_X8664
  if ( TOP_is_vector_high_loadstore ( OP_code ( new_op ) ) )
    Set_OP_cond_def_kind(new_op, OP_ALWAYS_COND_DEF);
#endif
  return new_op;
}

/* =====================================================================
 *			      OPS stuff
 *		(see "op.h" for interface description)
 * =====================================================================
 */

/* -----------------------------------------------------------------------
 *
 * void insert_ops_before(OPS *ops, OP *point, OP *first, OP *last)
 * void insert_ops_after(OPS *ops, OP *point, OP *first, OP *last)
 * void append_ops(OPS *ops, OP *first, OP *last)
 * void prepend_ops(OPS *ops, OP *first, OP *last)
 * void insert_ops(OPS *ops, OP *point, OP *first, OP *last, BOOL before)
 *
 * Requires: <last> is a (not necessarily direct) successor of <first>.
 *
 * Insert the OPs from <first> to <last> inclusive in the place implied
 * by the function names and/or <point> and/or <before> arguments when
 * applicable.
 *
 * Basically these are the workhorses for the OPS/BB Insert routines,
 * but they avoid setting any OP attributes other than the next and
 * previous pointers.
 *
 * -----------------------------------------------------------------------
 */

inline void prepend_ops(OPS *ops, OP *first, OP *last)
{
  OP **pprev = OPS_first(ops) ? &OPS_first(ops)->prev : &ops->last;
  first->prev = NULL;
  last->next = OPS_first(ops);
  ops->first = first;
  *pprev = last;
}


inline void append_ops(OPS *ops, OP *first, OP *last)
{
  OP **pnext = OPS_last(ops) ? &OPS_last(ops)->next : &ops->first;
  last->next = NULL;
  first->prev = OPS_last(ops);
  ops->last = last;
  *pnext = first;
}

inline void insert_ops_before(OPS *ops, OP *point, OP *first, OP *last)
{
  OP **prevp = OP_prev(point) ? &OP_prev(point)->next : &ops->first;
  *prevp = first;
  last->next = point;
  first->prev = OP_prev(point);
  point->prev = last;
}

inline void insert_ops_after(OPS *ops, OP *point, OP *first, OP *last)
{
  OP **nextp = OP_next(point) ? &OP_next(point)->prev : &ops->last;
  *nextp = last;
  first->prev = point;
  last->next = OP_next(point);
  point->next = first;
}

inline void insert_ops(OPS *ops, OP *point, OP *first, OP *last, BOOL before)
{
  if (point == NULL) {
    if (before)
      prepend_ops(ops, first, last);
    else
      append_ops(ops, first, last);
  } else {
    if (before)
      insert_ops_before(ops, point, first, last);
    else
      insert_ops_after(ops, point, first, last);
  }
}


/* -----------------------------------------------------------------------
 *   
 *  Sink the OP before point
 *
 * -----------------------------------------------------------------------
 */
void BB_Sink_Op_Before(BB *bb, OP *op, OP *point)
{
  if (OP_next(op) == point) return;

  Is_True(OP_bb(op) == bb && OP_bb(point) == bb,
	  ("Sink_Op_Before: must sink inside the bb."));

  // Disconnect "op" from body
  OP *t1 = OP_prev(op);
  OP *t2 = OP_next(op);
  if (t1) t1->next = t2;
  if (t2) t2->prev = t1;

  // Reconnect "op" to "succ"
  OP *prev = OP_prev(point);
  op->prev = prev;
  op->next = point;
  prev->next = op;
  point->prev = op;

  if (op == BB_first_op(OP_bb(op)))
    OP_bb(op)->ops.first = t2;

}


/* -----------------------------------------------------------------------
 *
 *  void setup_ops(BB *bb, OP *first, OP *last, UINT32 len)
 *
 *  Setup various fields (bb/map_idx/order) on OPs between <first> and
 *  <last>, inclusive, newly inserted into <bb>.  The bb and map_idx
 *  fields are fairly straightforward.  The order field is used to
 *  indicate relative order within <bb> (unless it is NULL).  May also
 *  change the order fields of other OPs in the BB.  Should almost
 *  always execute in time linear in the length of the chain from
 *  <first> to <last>.  Worst case time is linearly dependent on the
 *  size of the BB, but should be tuned to make this case extremely
 *  rare.
 *
 * ----------------------------------------------------------------------- */

/* Assume op->order is some kind of unsigned integer type.
 */
#define ORDER_TYPE UINT16
#define mORDER_TYPE mUINT16
#define mMAP_IDX_TYPE mUINT16
#define ORDER_BITS (sizeof(mORDER_TYPE) * 8)
#define MIN_INITIAL_SPACING \
  ((mORDER_TYPE)1 << (ORDER_BITS-(sizeof(mMAP_IDX_TYPE)*8)))
#define INITIAL_SPACING ((ORDER_TYPE)(MIN_INITIAL_SPACING * 8))
#define MAX_ORDER ((ORDER_TYPE)-1)


static void setup_ops(BB *bb, OP *first, OP *last, UINT32 len)
{
  OP *op;
  ORDER_TYPE incr;
  ORDER_TYPE order_before;
  ORDER_TYPE order_after;
  ORDER_TYPE order;

  /* Empty lists are easy.
   */
  if (len == 0) return;

  /* Get the 'order' number of the OPs immediately before and after
   * the OPs were inserting.
   */
  order_before = OP_prev(first) ? OP_prev(first)->order : 0;
  order_after = OP_next(last) ? OP_next(last)->order : MAX_ORDER;

  /* Compute the increment to use when assigning 'order' numbers
   * so they will fit between the OPs were inserting at. If there
   * isn't enough room, i.e. 'incr' is 0, we'll detect that later.
   */
  incr = (order_after - order_before - 1) / (len + 1);
  if (incr > INITIAL_SPACING) incr = INITIAL_SPACING;

  /* Loop over the OPs being inserted and initialize the necessary
   * fields. Our attempt at generating 'order' numbers is an
   * educated guess, but in many cases we'll guess right.
   */
  order = order_before;
  op = first;
  do {
    FmtAssert(op, ("input ops not connected properly"));
    op->bb = bb;
    op->map_idx = BB_New_Op_Map_Idx(bb);
    order += incr;
    op->order = order;
    REGISTER_CLASS_OP_Update_Mapping (op);
    op = OP_next(op);
  } while (op != OP_next(last));

  /* All done if we were able to squeeze in the 'order' numbers.
   */
  if (incr != 0) goto done;

  /* It was not possible to assign 'order' numbers to the new OPs --
   * we'll have to re-order some of the OPs on the list we're inserting
   * into. Include OPs from before and/or after the inserted OPs until
   * we make a big enough hole that it is possible to re-order.
   *
   * NOTE: We include all 'after' OPs before including any of the 
   * 'before' OPs. This tends to keep the beginning of the list
   * "nicely" ordered. Another approach would be to include from
   * the direction that adds the most to the delta between 
   * 'order_after' and 'order_before'.
   */
  do {
    if (OP_next(last)) {
      last = OP_next(last);
      order_after = OP_next(last) ? OP_next(last)->order : MAX_ORDER;
    } else if (OP_prev(first)) {
      first = OP_prev(first);
      order_before = OP_prev(first) ? OP_prev(first)->order : 0;
    } else {
      FmtAssert(FALSE, ("unable to reorder"));
    }
    len++;
    incr = (order_after - order_before - 1) / (len + 1);
  } while (incr == 0);
  if (incr > INITIAL_SPACING) incr = INITIAL_SPACING;

  /* Re-order the OPs.
   */
  op = first;
  order = order_before;
  do {
    order += incr;
    op->order = order;
    op = OP_next(op);
  } while (op != OP_next(last));

done:
  /* C insists on requiring at least one statement after a label.
   * There won't be any when VERIFY_OPS is not defined, so here ya go...
   */
  ;

#ifdef VERIFY_OPS
  {
    UINT16 len = 1;
    op = first;
    while (OP_prev(op)) op = OP_prev(op);
    while (OP_next(op)) {
      FmtAssert(op->order < OP_next(op)->order, ("OP order set wrong"));
      FmtAssert(op->bb == bb, ("OP bb set wrong"));
      op = OP_next(op);
      len++;
    }
    FmtAssert(len == BB_length(bb), ("BB_length set wrong"));
  }
#endif
}


void OPS_Insert_Op(OPS *ops, OP *point, OP *op, BOOL before)
{
  insert_ops(ops, point, op, op, before);
  ops->length++;
}


void OPS_Insert_Op_Before(OPS *ops, OP *point, OP *op)
{
  insert_ops_before(ops, point, op, op);
  ops->length++;
}


void OPS_Insert_Op_After(OPS *ops, OP *point, OP *op)
{
  insert_ops_after(ops, point, op, op);
  ops->length++;
}


void OPS_Append_Op(OPS *ops, OP *op)
{
  append_ops(ops, op, op);
  ops->length++;
}


void OPS_Prepend_Op(OPS *ops, OP *op)
{
  prepend_ops(ops, op, op);
  ops->length++;
}


void OPS_Insert_Ops(OPS *ops, OP *point, OPS *new_ops, BOOL before)
{
  if (OPS_first(new_ops) == NULL) return;
  insert_ops(ops, point, OPS_first(new_ops), OPS_last(new_ops), before);
  ops->length += OPS_length(new_ops);
}


void OPS_Insert_Ops_Before(OPS *ops, OP *point, OPS *new_ops)
{
  if (OPS_first(new_ops) == NULL) return;
  insert_ops_before(ops, point, OPS_first(new_ops), OPS_last(new_ops));
  ops->length += OPS_length(new_ops);
}


void OPS_Insert_Ops_After(OPS *ops, OP *point, OPS *new_ops)
{
  if (OPS_first(new_ops) == NULL) return;
  insert_ops_after(ops, point, OPS_first(new_ops), OPS_last(new_ops));
  ops->length += OPS_length(new_ops);
}


void OPS_Append_Ops(OPS *ops, OPS *new_ops)
{
  if (OPS_first(new_ops) == NULL) return;
  append_ops(ops, OPS_first(new_ops), OPS_last(new_ops));
  ops->length += OPS_length(new_ops);
}


void OPS_Prepend_Ops(OPS *ops, OPS *new_ops)
{
  if (OPS_first(new_ops) == NULL) return;
  prepend_ops(ops, OPS_first(new_ops), OPS_last(new_ops));
  ops->length += OPS_length(new_ops);
}

// Update OP order
//
void BB_Update_OP_Order(BB *bb)
{
  INT order = 0;
  INT incr = INITIAL_SPACING;
  for (OP *op = BB_first_op(bb); op; op = OP_next(op)) {
    order += incr;
    op->order = order;
  } 
}

// Verify OP order
//
void BB_Verify_OP_Order(BB *bb)
{
  INT prev_order = -1;
  for (OP *op = BB_first_op(bb); op; op = OP_next(op)) {
    FmtAssert(prev_order < op->order,
	      ("BB_Verify_OP_Order: OP_order() is not correct."));
    prev_order = op->order;
  }
}


/* =====================================================================
 *			   (Some) BB stuff
 *		(see "bb.h" for interface description)
 * =====================================================================
 */

void BB_Insert_Op(BB *bb, OP *point, OP *op, BOOL before)
{
  Is_True(bb, ("can't insert in NULL BB"));
  insert_ops(&bb->ops, point, op, op, before);
  bb->ops.length++;
  setup_ops(bb, op, op, 1);
}

 
void BB_Insert_Op_Before(BB *bb, OP *point, OP *op)
{
  Is_True(bb, ("can't insert in NULL BB"));
  insert_ops_before(&bb->ops, point, op, op);
  bb->ops.length++;
  setup_ops(bb, op, op, 1);
}


void BB_Insert_Op_After(BB *bb, OP *point, OP *op)
{
  Is_True(bb, ("can't insert in NULL BB"));
  insert_ops_after(&bb->ops, point, op, op);
  bb->ops.length++;
  setup_ops(bb, op, op, 1);
}


void BB_Prepend_Op(BB *bb, OP *op)
{
  Is_True(bb, ("can't insert in NULL BB"));
  prepend_ops(&bb->ops, op, op);
  bb->ops.length++;
  setup_ops(bb, op, op, 1);
}


void BB_Append_Op(BB *bb, OP *op)
{
  Is_True(bb, ("can't insert in NULL BB"));
  append_ops(&bb->ops, op, op);
  bb->ops.length++;
  setup_ops(bb, op, op, 1);
}


void BB_Insert_Ops(BB *bb, OP *point, OPS *ops, BOOL before)
{
  if (OPS_first(ops) == NULL) return;
  insert_ops(&bb->ops, point, OPS_first(ops), OPS_last(ops), before);
  bb->ops.length += OPS_length(ops);
  setup_ops(bb, OPS_first(ops), OPS_last(ops), OPS_length(ops));
}


void BB_Insert_Ops_Before(BB *bb, OP *point, OPS *ops)
{
  if (OPS_first(ops) == NULL) return;
  insert_ops_before(&bb->ops, point, OPS_first(ops), OPS_last(ops));
  bb->ops.length += OPS_length(ops);
  setup_ops(bb, OPS_first(ops), OPS_last(ops), OPS_length(ops));
}


void BB_Insert_Ops_After(BB *bb, OP *point, OPS *ops)
{
  if (OPS_first(ops) == NULL) return;
  insert_ops_after(&bb->ops, point, OPS_first(ops), OPS_last(ops));
  bb->ops.length += OPS_length(ops);
  setup_ops(bb, OPS_first(ops), OPS_last(ops), OPS_length(ops));
}


void  BB_Insert_Noops(OP *op, INT num, BOOL before)
{
  OPS new_ops = OPS_EMPTY;
  INT i;

  for (i = 0; i < num; i++) {
    Exp_Noop (&new_ops);
  }
  BB_Insert_Ops(OP_bb(op), op, &new_ops, before);
}


void BB_Prepend_Ops(BB *bb, OPS *ops)
{
  if (OPS_first(ops) == NULL) return;
  prepend_ops(&bb->ops, OPS_first(ops), OPS_last(ops));
  bb->ops.length += OPS_length(ops);
  setup_ops(bb, OPS_first(ops), OPS_last(ops), OPS_length(ops));
}


void BB_Append_Ops(BB *bb, OPS *ops)
{
  if (OPS_first(ops) == NULL) return;
  append_ops(&bb->ops, OPS_first(ops), OPS_last(ops));
  bb->ops.length += OPS_length(ops);
  setup_ops(bb, OPS_first(ops), OPS_last(ops), OPS_length(ops));
}


void BB_Move_Op(BB *to_bb, OP *point, BB *from_bb, OP *op, BOOL before)
{
  Is_True(OP_bb(op) == from_bb, ("op not in from_bb"));
  Is_True(OP_bb(point) == to_bb, ("point not in to_bb"));
  OPS_Remove_Op(&from_bb->ops, op);
  insert_ops(&to_bb->ops, point, op, op, before);
  to_bb->ops.length++;
  setup_ops(to_bb, op, op, 1);
}


void BB_Move_Op_Before(BB *to_bb, OP *point, BB *from_bb, OP *op)
{
  Is_True(OP_bb(op) == from_bb, ("op not in from_bb"));
  Is_True(OP_bb(point) == to_bb, ("point not in to_bb"));
  OPS_Remove_Op(&from_bb->ops, op);
  insert_ops_before(&to_bb->ops, point, op, op);
  to_bb->ops.length++;
  setup_ops(to_bb, op, op, 1);
}

void BB_Move_Op_After(BB *to_bb, OP *point, BB *from_bb, OP *op)
{
  Is_True(OP_bb(op) == from_bb, ("op not in from_bb"));
  Is_True(OP_bb(point) == to_bb, ("point not in to_bb"));
  OPS_Remove_Op(&from_bb->ops, op);
  insert_ops_after(&to_bb->ops, point, op, op);
  to_bb->ops.length++;
  setup_ops(to_bb, op, op, 1);
}


void BB_Move_Op_To_Start(BB *to_bb, BB *from_bb, OP *op)
{
  Is_True(OP_bb(op) == from_bb, ("op not in from_bb"));
  OPS_Remove_Op(&from_bb->ops, op);
  prepend_ops(&to_bb->ops, op, op);
  to_bb->ops.length++;
  setup_ops(to_bb, op, op, 1);
}


void BB_Move_Op_To_End(BB *to_bb, BB *from_bb, OP *op)
{
  Is_True(OP_bb(op) == from_bb, ("op not in from_bb"));
  OPS_Remove_Op(&from_bb->ops, op);
  append_ops(&to_bb->ops, op, op);
  to_bb->ops.length++;
  setup_ops(to_bb, op, op, 1);
}


void BB_Append_All(BB *to_bb, BB *from_bb)
{
  OPS the_ops;

  if (BB_length(from_bb) == 0) return;

  the_ops = from_bb->ops;
  BB_Remove_All(from_bb);
  BB_Append_Ops(to_bb, &the_ops);
}


void BB_Prepend_All (BB *to_bb, BB *from_bb)
{
  OPS the_ops;

  if (BB_length(from_bb) == 0) return;

  the_ops = from_bb->ops;
  BB_Remove_All (from_bb);
  BB_Prepend_Ops (to_bb, &the_ops);
}


OP *BB_Remove_Branch(BB *bb)
{
  OP *last_op;
  OP *br = BB_branch_op(bb);

  if (br) {
    last_op = BB_last_op(bb);
    if (OP_noop(last_op)) BB_Remove_Op(bb, last_op);
    BB_Remove_Op(bb, br);
  }

  return br;
}


void BB_Remove_Op(BB *bb, OP *op)
{
  OPS_Remove_Op(&bb->ops, op);
  op->bb = NULL;
}


void BB_Remove_Ops(BB *bb, OPS *ops)
{
  OP *op;

  if (OPS_first(ops) == NULL) return;

  OPS_Remove_Ops(&bb->ops, ops);

  FOR_ALL_OPS_OPs(ops, op) op->bb = NULL;
}


void BB_Remove_All(BB *bb)
{
  BB_Remove_Ops(bb, &bb->ops);
  BB_next_op_map_idx(bb) = 0;
}

/* ====================================================================
 *
 * Mk_OP / Mk_VarOP
 *
 * Make new OP records.
 *
 * ====================================================================
 */
OP *
Mk_OP(TOP opr, ...)
{
  va_list ap;
  INT i;
  INT results = TOP_fixed_results(opr);
  INT opnds = TOP_fixed_opnds(opr);
#ifdef TARG_IA64
  OP *op = New_OP(results, opnds, CGTARG_Max_Number_of_Hidden_Opnd(opr));
#else
  OP *op = New_OP(results, opnds);
#endif

  FmtAssert(!TOP_is_var_opnds(opr), ("Mk_OP not allowed with variable operands"));

  Set_OP_code(op, opr);

  va_start(ap, opr);

  for (i = 0; i < results; ++i) {
    TN *result = va_arg(ap, TN *);
    Set_OP_result(op, i, result);
  }
  if (TOP_is_defs_fpu_int(opr)) Set_TN_is_fpu_int(OP_result(op, 0));

  for (i = 0; i < opnds; ++i) {
    TN *opnd = va_arg(ap, TN *);
    Set_OP_opnd(op, i, opnd);
  }

  va_end(ap);

  CGTARG_Init_OP_cond_def_kind(op);

#if Is_True_On
#ifdef TARG_X8664
  // Make sure no 64-bit int operations for n32 will be generated.
  if( Is_Target_32bit() &&
      !OP_dummy( op )   &&
      !OP_simulated(op) &&
      !OP_cond_move(op) &&
      OP_code(op) != TOP_leave ){

    for( int i = 0; i < OP_results(op); i++ ){
      TN* tn = OP_result( op, i );
      if( tn != NULL && 
	  OP_result_size( op, i ) > 32 &&
	  TN_register_class(tn) == ISA_REGISTER_CLASS_integer ){
	FmtAssert( FALSE, ("i386 does not support 64-bit operation -- %s",
			   TOP_Name(opr) ) );
      }
    }

    const int base_idx = OP_find_opnd_use( op, OU_base );
    const int index_idx = OP_find_opnd_use( op, OU_index );
    const int target_idx = OP_find_opnd_use( op, OU_target );

    for( int i = 0; i < OP_opnds(op); i++ ){
      TN* tn = OP_opnd( op, i );
      if( tn != NULL     &&
	  i != base_idx  &&
	  i != index_idx &&
	  i != target_idx&&
	  OP_opnd_size( op, i ) > 32 &&
	  TN_register_class(tn) == ISA_REGISTER_CLASS_integer ){
	FmtAssert( FALSE, ("i386 does not support 64-bit operation -- %s",
			   TOP_Name(opr) ) );
      }
    }
  }
#endif // TARG_X8664
#endif // Is_True_On

#ifdef TARG_X8664
  if ( TOP_is_vector_high_loadstore ( OP_code ( op ) ) )
    Set_OP_cond_def_kind(op, OP_ALWAYS_COND_DEF);
#endif
  return op;
}

OP *
Mk_VarOP(TOP opr, INT results, INT opnds, TN **res_tn, TN **opnd_tn)
{
  if (results != TOP_fixed_results(opr)) {
    FmtAssert(TOP_is_var_opnds(opr) && results > TOP_fixed_results(opr),
	      ("%d is not enough results for %s", results, TOP_Name(opr)));
  }
  if (opnds != TOP_fixed_opnds(opr)) {
    FmtAssert(TOP_is_var_opnds(opr) && opnds > TOP_fixed_opnds(opr),
	      ("%d is not enough operands for %s", opnds, TOP_Name(opr)));
  }

  INT i;
#ifdef TARG_IA64
  OP *op = New_OP(results, opnds, CGTARG_Max_Number_of_Hidden_Opnd(opr));
#else
  OP *op = New_OP(results, opnds);
#endif
  Set_OP_code(op, opr);

  for (i = 0; i < results; ++i) Set_OP_result(op, i, res_tn[i]);
  if (TOP_is_defs_fpu_int(opr)) Set_TN_is_fpu_int(res_tn[0]);

  for (i = 0; i < opnds; ++i) Set_OP_opnd(op, i, opnd_tn[i]);

  CGTARG_Init_OP_cond_def_kind(op);

  return op;
}

/* ====================================================================
 *
 * Print_OP / Print_OP_No_SrcLine / Print_OPs / Print_OPS
 *
 * Print an OP (or OP list) to the trace file.  These shouldn't be
 * inlined since they're useful for debugging and don't affect user
 * compile-time performance.
 *
 * ====================================================================
 */

void Print_OP_No_SrcLine(const OP *op)
{
  INT16 i;
  WN *wn;
  BOOL cg_loop_op = Is_CG_LOOP_Op(op);
#ifdef TARG_IA64
  if (OP_start_bundle(op)) fprintf( TFile, " }\n{\n");
  fprintf (TFile, "[%3d] ", OP_map_idx(op));
#endif
#ifdef TARG_X8664
  fprintf (TFile, "[%4d] ", OP_scycle(op) );
#endif
  fprintf (TFile, "[%4d] ", Srcpos_To_Line(OP_srcpos(op)));
  if (OP_has_tag(op)) {
	LABEL_IDX tag = Get_OP_Tag(op);
	fprintf (TFile, "<tag %s>: ", LABEL_name(tag));
  }
  for (i = 0; i < OP_results(op); i++) {
    Print_TN(OP_result(op,i),FALSE);
    fprintf(TFile, " ");
  }
  fprintf(TFile, ":- ");
  fprintf(TFile, "%s ", TOP_Name(OP_code(op)));
#ifdef TARG_IA64
  if ( OP_variant(op) != 0 ) {
    fprintf ( TFile, "(%x) ", OP_variant(op));
  }
#endif
  for (i=0; i<OP_opnds(op); i++) {
    TN *tn = OP_opnd(op,i);
    Print_TN(tn,FALSE);
    if ( cg_loop_op ) {
      INT omega = TN_is_symbol(tn) ? OP_restore_omega(op) : OP_omega(op,i);
      if (omega) fprintf(TFile, "[%d]", omega);
    }
    if (OP_Defs_TN(op, tn)) fprintf(TFile, "<defopnd>");
    fprintf(TFile, " ");
  }

  fprintf(TFile, ";");

  /* print flags */
  // fprintf(TFile," flags 0x%08x ",OP_flags(op));
  if (OP_glue(op)) fprintf (TFile, " glue");
  if (OP_no_alias(op)) fprintf (TFile, " noalias");
  if (OP_copy(op)) fprintf (TFile, " copy");
  if (OP_volatile(op)) fprintf (TFile, " volatile");
  if (OP_side_effects(op)) fprintf (TFile, " side_effects");
  if (OP_hoisted(op)) fprintf (TFile, " hoisted");
  if (OP_cond_def(op)) fprintf (TFile, " cond_def");
  if (OP_end_group(op)) fprintf (TFile, " end_group");
  if (OP_tail_call(op)) fprintf (TFile, " tail_call");
  if (OP_no_move_before_gra(op)) fprintf (TFile, " no_move");
  if (OP_spadjust_plus(op)) fprintf (TFile, " spadjust_plus");
  if (OP_spadjust_minus(op)) fprintf (TFile, " spadjust_minus");
#ifdef TARG_IA64
  if (OP_Scheduled(op)) fprintf (TFile, " scheduled");
  if (OP_start_bundle(op)) fprintf (TFile, " start_bundle");
  if (OP_safe_load(op)) fprintf (TFile, " safe_load");
#endif

  if (wn = Get_WN_From_Memory_OP(op)) {
    char buf[500];
    buf[0] = '\0';
    if (Alias_Manager) Print_alias_info (buf, Alias_Manager, wn);
#ifdef TARG_X8664
    fprintf(TFile, " WN %s", buf);
#else
    fprintf(TFile, " WN=0x%p %s", wn, buf);
#endif
  }
  if (OP_unrolling(op)) {
    UINT16 unr = OP_unrolling(op);
    fprintf(TFile, " %d%s unrolling", unr,
	    unr == 1 ? "st" : unr == 2 ? "nd" : unr == 3 ? "rd" : "th");
  }
  fprintf(TFile, "\n");
}

void Print_OP( const OP *op )
{
  Print_Src_Line (OP_srcpos(op), TFile);
  Print_OP_No_SrcLine(op);
}

void Print_OPs( const OP *op )
{
  for ( ; op; op = OP_next(op))
    Print_OP(op);
}

void Print_OPS( const OPS *ops )
{
  OP *op;
  FOR_ALL_OPS_OPs_FWD(ops, op)
    Print_OP(op);
}

void Print_OPs_No_SrcLines( const OP *op )
{
  for ( ; op; op = OP_next(op))
    Print_OP_No_SrcLine(op);
}

void Print_OPS_No_SrcLines( const OPS *ops )
{
  OP *op;
  FOR_ALL_OPS_OPs_FWD(ops, op)
    Print_OP_No_SrcLine(op);
}



/* ====================================================================
 *
 * OP_Defs_Reg
 *
 * See interface description.
 *
 * ====================================================================
 */

BOOL
OP_Defs_Reg(const OP *op, ISA_REGISTER_CLASS cl, REGISTER reg)
{
  register INT num;

  for ( num = 0; num < OP_results(op); num++ ) {
    TN *res_tn = OP_result(op,num);
    if (TN_is_register(res_tn)) {
      if (TN_register_class(res_tn) == cl && TN_register(res_tn) == reg ) {
	return TRUE;
      }
    }
  }

  /* if we made it here, we must not have found it */
  return FALSE;
}

/* ====================================================================
 *
 * OP_Refs_Reg
 *
 * See interface description.
 *
 * ====================================================================
 */

BOOL
OP_Refs_Reg(const OP *op, ISA_REGISTER_CLASS cl, REGISTER reg)
{
  register INT num;

  for ( num = 0; num < OP_opnds(op); num++ ) {
    TN *opnd_tn = OP_opnd(op,num);
    if (TN_is_register(opnd_tn)) {
      if (TN_register_class(opnd_tn) == cl && TN_register(opnd_tn) == reg ) {
	return TRUE;
      }
    }
  }

#ifdef KEY
  if( OP_cond_def( op ) ){
    for ( num = 0; num < OP_results(op); num++ ) {
      TN* result_tn = OP_result( op, num );
      if (TN_is_register(result_tn)          &&
	  TN_register_class(result_tn) == cl &&
	  TN_register(result_tn) == reg ) {
	return TRUE;
      }      
    }
  }
#endif

  /* if we made it here, we must not have found it */
  return FALSE;
}


/* ====================================================================
 *
 * OP_Defs_TN
 *
 * See interface description.
 *
 * ====================================================================
 */

BOOL
OP_Defs_TN(const OP *op, const struct tn *res)
{
  register INT num;

  for ( num = 0; num < OP_results(op); num++ ) {
    if ( OP_result(op,num) == res ) {
      return( TRUE );
    }
  }

  /* if we made it here, we must not have found it */
  return( FALSE );
}


/* ====================================================================
 *
 * OP_Refs_TN
 *
 * See interface description.
 *
 * ====================================================================
 */

BOOL
OP_Refs_TN( const OP *op, const struct tn *opnd )
{
  register INT16 num;

  for ( num = 0; num < OP_opnds(op); num++ ) {
    if ( OP_opnd(op,num) == opnd ) {
      return( TRUE );
    }
  }

#ifdef KEY
  if( OP_cond_def( op ) ){
    for ( num = 0; num < OP_results(op); num++ ) {
      if( OP_result( op, num ) == opnd )
	return TRUE;
    }
  }
#endif

  /* if we made it here, we must not have found it */
  return( FALSE );
}


/* ====================================================================
 *
 * OP_Real_Ops - How many ops does this op really represent, i.e. will
 * be emitted.
 *
 * ====================================================================
 */

INT16
OP_Real_Ops( const OP *op )
{
  if ( op == NULL || OP_dummy(op) ) {
    return 0;
  }
  else if ( OP_simulated(op) ) {
    return Simulated_Op_Real_Ops (op);
  }
  return 1;
}


/* ====================================================================
 *
 * OP_Real_Inst_Words - How many instruction words does this op really 
 * represent, i.e. will be emitted.
 *
 * ====================================================================
 */

INT
OP_Real_Inst_Words( const OP *op )
{
  if ( op == NULL || OP_dummy(op) ) {
    return 0;
  }
  else if ( OP_simulated(op) ) {
    return Simulated_Op_Real_Inst_Words (op);
  }
  return OP_inst_words(op);
}


/* ====================================================================
 *
 * OP_Is_Float_Mem - Is OP a floating point memory operation?
 *
 * ====================================================================
 */

BOOL
OP_Is_Float_Mem( const OP *op )
{
  return (OP_load(op) && TN_is_float(OP_result(op, 0))) ||
	 (OP_store(op) && TN_is_float(OP_opnd(op, 0)));
}

/* ====================================================================
 *
 * OP_Alloca_Barrier - Is OP a alloca barrier node with alias info?
 *
 * ====================================================================
 */

BOOL
OP_Alloca_Barrier(OP *op )
{
  return (OP_code(op) == TOP_spadjust && Get_WN_From_Memory_OP(op));
}

// =======================================================================
// Is_Delay_Slot_Op
// Return TRUE if the <op> is the right type to put into a delay slot of
// <xfer_op>.
// =======================================================================
BOOL 
Is_Delay_Slot_Op (OP *xfer_op, OP *op)
{
  if (op == NULL || OP_xfer(op) || OP_Real_Ops(op) != 1) return FALSE;

  // R10k chip bug workaround: Avoid placing integer mult/div in delay 
  // slots of unconditional branches. (see pv516598) for more details.
  if (xfer_op && OP_uncond(xfer_op) &&
      (OP_imul(op) || OP_idiv(op))) return FALSE;
  
  // TODO: do we need the following restriction ?
  if (OP_has_hazard(op) || OP_has_implicit_interactions(op))
    return FALSE;
  return TRUE;
}



// Debugging routine
void dump_op(const OP *op)
{
   FILE *f;
   f = TFile;
   Set_Trace_File_internal(stdout);
   Print_OP_No_SrcLine(op);
   Set_Trace_File_internal(f);
}


/* ====================================================================
 *
 * OP_cond_def
 *
 * Return TRUE if the OP conditionally modifies some of the results.
 *
 * ====================================================================
 */

BOOL OP_cond_def(const OP *op) 
{
  return OP_cond_def_kind(op) == OP_ALWAYS_COND_DEF ||
    ((OP_cond_def_kind(op) == OP_PREDICATED_DEF) && 
     !TN_is_true_pred(OP_opnd(op, OP_PREDICATE_OPND)));
}

/* ====================================================================
 *
 * OP_has_implicit_interactions
 *
 * Return TRUE if the OP has some implicit interaction properties with
 * other OPs in a non-obvious way.
 *
 * ====================================================================
 */

BOOL OP_has_implicit_interactions(OP *op) 
{
  if (OP_volatile(op) || OP_side_effects(op))
    return TRUE;

  INT i;
  for (i = 0; i < OP_opnds(op); i++) {
    TN *opnd_tn = OP_opnd(op, i);
    if (TN_is_tag(opnd_tn)) return TRUE;
  }

  return FALSE;
}

/* ====================================================================
 *
 * OP_Base_Offset_TNs
 *
 * Return the base and offset TNs for the given memory OP.
 *
 * ====================================================================
 */
void OP_Base_Offset_TNs(OP *memop, TN **base_tn, TN **offset_tn)
{
#ifdef TARG_X8664
  Is_True(OP_load(memop) || OP_load_exe(memop) || OP_store(memop), ("not a load or store"));
#else
  Is_True(OP_load(memop) || OP_store(memop), ("not a load or store"));
#endif

  INT offset_num = OP_find_opnd_use (memop, OU_offset);
  INT base_num   = OP_find_opnd_use (memop, OU_base);

  *offset_tn = NULL;

  *base_tn = base_num >= 0 ? OP_opnd(memop, base_num) : NULL;

  // <offset> TNs are not part of <memop>. Find the definining OP_iadd
  // instruction which sets the offset and matches the base_tn.

  if (offset_num < 0) {

    DEF_KIND kind;
    OP *defop = TN_Reaching_Value_At_Op(*base_tn, memop, &kind, TRUE);
    if (defop && OP_iadd(defop) && kind == VAL_KNOWN) {
      TN *defop_offset_tn = OP_opnd(defop, 1);
      TN *defop_base_tn = OP_opnd(defop, 2);
      if (defop_base_tn == *base_tn && TN_has_value(defop_base_tn)) {
	*offset_tn = defop_offset_tn;
      }
    }
  } else {
    *offset_tn = OP_opnd(memop, offset_num);
  }
}

#ifdef TARG_IA64
/* ====================================================================
 * OP_ld_st_unat
 *
 * return TRUE if op load/store a unat bit 
 *
 * ====================================================================
 */
BOOL OP_ld_st_unat(OP *op)
{
    mTOP opcode = OP_code(op);
    if(opcode == TOP_mov_f_ar || opcode == TOP_mov_t_ar_r  ||
       opcode == TOP_mov_f_ar_m || opcode == TOP_mov_t_ar_r_m)
    {
        for(INT i=0; i<OP_results(op); i++)
        {
            if(OP_result(op,i) == 
               Build_Dedicated_TN(ISA_REGISTER_CLASS_application,(REGISTER)(REGISTER_MIN + 36),0))
                 return TRUE;
        }  
        for(INT i=0;i<OP_opnds(op); i++)
        {
            if(OP_opnd(op,i) == 
               Build_Dedicated_TN(ISA_REGISTER_CLASS_application,(REGISTER)(REGISTER_MIN + 36),0))
                return TRUE;
        }
    }
    return FALSE;
}

BOOL OP_def_return_value(OP* op)
{
    for (INT i = OP_results(op) - 1 ; i >= 0 ; i--) {
        mTN_NUM n = TN_number(OP_result(op,i));
        if ((n >= First_Int_Preg_Return_Offset && 
             n <= Last_Int_Preg_Return_Offset)       ||
            (n >= First_Float_Preg_Return_Offset &&
             n <= Last_Float_Preg_Return_Offset)) {
            return TRUE;    
        }
    }         
    return FALSE;
}        

BOOL OP_use_return_value (OP* op) {
    for (INT i = 0; i < OP_opnds (op); i++) {
        TN* opnd = OP_opnd(op, i);
        if (TN_is_constant(opnd)) { continue; }

        mTN_NUM n = TN_number(opnd);
        if ((n >= First_Int_Preg_Return_Offset && 
             n <= Last_Int_Preg_Return_Offset)       ||
            (n >= First_Float_Preg_Return_Offset &&
             n <= Last_Float_Preg_Return_Offset)) {
            return TRUE;    
        }
    }         
    return FALSE;
}

/* Add hidden operands to given op. All hidden operands should be added at one time.
 */
void
Add_Hidden_Operands (OP* op, const vector<TN*>& hopnds) {
  if (hopnds.size () == 0) return;

  INT t = CGTARG_Max_Number_of_Hidden_Opnd (OP_code(op));
  Is_True (t > 0,  ("Op does not have hidden openrands"));
  Is_True (hopnds.size() <= t, ("Expected at most %d hidden operands"));
  Is_True (OP_hidden_opnds(op) == 0, ("Hidden operands are added once"));

  // leave room for hidden operands   
  if (OP_results(op) != 0) {
    INT32 from_idx = op->opnds+op->results - 1;
    INT32 to_idx = from_idx + hopnds.size();
    for (INT32 count = OP_results(op); count > 0; count--) {
      op->res_opnd[to_idx--] = op->res_opnd[from_idx--];
    }
  }

  // now interpose the hidden operands between operands and results.
  for (INT32 i = 0; i < hopnds.size (); ++i) {
    op->res_opnd[op->opnds+i] = hopnds[i];
  }

  op->hidden_opnds = hopnds.size();
  op->opnds += hopnds.size ();
}

#endif // TARG_IA64

#ifdef KEY
/* ====================================================================
 *
 * TN_Pair_In_OP
 *
 * See interface description.
 *
 * ====================================================================
 */

BOOL 
TN_Pair_In_OP(OP* op, struct tn *tn_res, struct tn *tn_opnd) 
{
  INT i;
  for (i = 0; i < OP_results(op); i++) {
    if (tn_res == OP_result(op,i)) {
      break; 
    }
  }
  if (i == OP_results(op)) {
    // If tn_res has an assigned register, check if it matches a result.  (This
    // changes the semantics of TN_Pair_In_OP, but that's ok since the only
    // user of TN_Pair_In_OP is LRA, which wants this check.)  Bug 9489.
    BOOL result_match = FALSE;
    if (TN_register(tn_res) != REGISTER_UNDEFINED) {
      for (int j = 0; j < OP_results(op); j++) {
        TN *res = OP_result(op, j);
	if (TN_register_and_class(res) == TN_register_and_class(tn_res)) {
	  result_match = TRUE;
	}
      }
    }
    if (!result_match)
      return FALSE;
  }
  for (i = 0; i < OP_opnds(op); i++) {
    if (tn_opnd == OP_opnd(op,i)) {
      return TRUE; 
    }
  }
  return FALSE;
}

/* ====================================================================
 *
 * TN_Resnum_In_OP
 *
 * See interface description.
 *
 * ====================================================================
 */

INT 
TN_Resnum_In_OP (OP* op, struct tn *tn, BOOL match_assigned_reg) 
{
  for (INT i = 0; i < OP_results(op); i++) {
    TN *res = OP_result(op, i);
    if (tn == res) {
      return i;
    }

    if (match_assigned_reg &&
	TN_register(res) != REGISTER_UNDEFINED &&
	TN_register_and_class(res) == TN_register_and_class(tn)) {
      return i;
    }
  }
  FmtAssert (FALSE,
             ("TN_resnum_in_OP: Could not find <tn> in results list\n"));
  return -1;
}
#endif

/* Is <op> a copy from a callee-saves register into its save-TN?
 */
BOOL
OP_Is_Copy_To_Save_TN(const OP* op)
{
  INT i;

  for ( i = OP_results(op) - 1; i >= 0; --i ) {
    TN* tn = OP_result(op,i);
    if ( TN_is_save_reg(tn)) return TRUE;
  }

  return FALSE;
}

/*  Is <op> a copy to a callee-saves register from its save-TN?
 */
BOOL
OP_Is_Copy_From_Save_TN( const OP* op )
{
  INT i;

  // You'd think there'd be a better way than groveling through the operands,
  // but short of marking these when we make them, this seems to be the most
  // bullet-proof

  for ( i = OP_results(op) - 1; i >= 0; --i ) {
    if ( TN_is_dedicated(OP_result(op,i)) ) break;
  }
  if ( i < 0 ) return FALSE;

  for ( i = OP_opnds(op) - 1; i >= 0; --i ) {
    TN* tn = OP_opnd(op,i);
    if ( TN_Is_Allocatable(tn) && TN_is_save_reg(tn))
      return TRUE;
  }

  return FALSE;
}

