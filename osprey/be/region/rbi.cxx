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


//-*-c++-*-
//============================================================================
//
// Module: rbi.cxx
// $Revision: 1.1.1.1 $
// $Date: 2005/10/21 19:00:00 $
// $Author: marcel $
// $Source: /proj/osprey/CVS/open64/osprey1.0/be/region/rbi.cxx,v $
//
// Revision history:
//  31-AUG-95 dahl - Original Version
//
// Description:
//	Region Boundary Info (RBI). Using DU-chains and alias
//      information, calculate the boundary information for regions in
//      a PU.
//
// This code is linked in and called by lno.so
//
// TODO items:
// *) Write query routines to provide region boundary information from
//    the used_in, def_in_live_out, and def_in_dead_out sets to
//    consumers. This should be fairly simple, using
//    POINTS_TO::Overlap(POINTS_TO *) from
//    be/wopt[_dev]/opt_alias_analysis.cxx. Some wrapper code will be
//    required to iterate through the appropriate set of POINTS_TOs,
//    and do something reasonable when the base-ST of the given
//    POINTS_TO differs from that of one of the set elements.
//
// NOTE:
//  Any POINTS_TO nodes created by RBI must be allocated from the 
//  REGION_mem_pool so that they do not disappear when the alias
//  manager is freed.
//
//============================================================================

#define rbi_CXX	"rbi.cxx"
#ifdef _KEEP_RCS_ID
static char *rcs_id = rbi_CXX"$Revision$";
#endif /* _KEEP_RCS_ID */

#include "opt_wn.h"
#include "opt_base.h"
#include "wn_util.h"
#include "stab.h"
#include "cxx_base.h"
#include "cxx_memory.h"
#include "region_util.h"	/* for region trace flags, REGION_mem_pool */
#include "ir_reader.h"		/* for fdump_tree			*/
#include "tracing.h"		/* TFile				*/
#include "opt_du.h"		/* for DU_MANAGER class declaration	*/
#include "opt_points_to.h"      /* for POINTS_TO class declaration	*/
#include "opt_alias_mgr.h"      /* for ALIAS_MANAGER class declaration	*/
#include "rbi.h"		/* class RBI				*/

// ALIAS_MANAGER::Id(), USE_LIST::Print(), DEF_LIST::Print()
// defined in wopt.so and exported
// these pragmas only work for Mongoose (bootstrap), Ucode has no clue.
#ifdef AFTER_MR
#pragma weak Print__8USE_LISTGP8__file_s
#pragma weak Print__8DEF_LISTGP8__file_s
#pragma weak Id__13ALIAS_MANAGERCGPC2WN
#endif

#if 0
this whole file is out of date and needs to be upgraded

// NOTE: for Mongoose MR-1 put the calls to the above exported C++ routines
// inside #ifdef AFTER_MR so it won't hold up quickstart. Once Mongoose
// bootstraps itself we can put this code back in.

// -----------------------------------------------------------------
// Supporting classes and types for manipulating structures internal
// to RBI
// -----------------------------------------------------------------
typedef enum {
  RBI_DEAD,
  RBI_LIVE,
  RBI_NUM_LIVENESS
} RBI_LIVENESS_STATE;

// Forward declarations
BOOL RBI_WN_Aliasable( const WN *const );

// Classes to support sets of WN's with special properties. Private to
// RBI.

class RBI_WN_REF : public SLIST_NODE {
  DECLARE_SLIST_NODE_CLASS(RBI_WN_REF);
private:
  WN *const _wn;
public:
  RBI_WN_REF(WN *const wn) : _wn(wn)	{ }
  ~RBI_WN_REF(void)			{ }

  WN   *Wn( void ) const		{ return _wn; }
  BOOL  Aliasable( void ) const		{ return RBI_WN_Aliasable(_wn); }
  void Print(FILE *FD)
    { fprintf(FD,"%s ",(Aliasable()) ? "<aliasable>" : "<not aliasable>");
      fdump_wn(FD,_wn);
    }
};

class RBI_WN_SET : public SLIST {
  friend class RBI_WN_USED_SET;
  friend class RBI_WN_DEF_SET;

private:
  RBI  *_rbi;
  DECLARE_SLIST_CLASS( RBI_WN_SET, RBI_WN_REF );

public:
  RBI_WN_SET( RBI *const rbi )		{ _rbi = rbi; }

  ~RBI_WN_SET( void )
    { while ( !Is_Empty() ) {
        RBI_WN_REF *tmp = Remove_Headnode();
        CXX_DELETE(tmp, _rbi->Mem_Pool());
      }
    }

  // Compare, return TRUE if they are the same, recursive
  // if we make a mistake there will be duplicates on the list making 
  //   it longer - no biggie
  // (based on common/com/wn_simp_code.h, SIMPNODE_Simp_Compare_Trees)
  //
  // PPP This routine needs to be improved. ISTOREs are always duplicated
  // in the list because they are recursive and the default case doesn't
  // actually compare.
  BOOL Compare(WN *const wn1, WN *const wn2)
    {
      OPCODE op = WN_opcode(wn1);
      if (op == WN_opcode(wn2)) { // same opcode
	switch (OPCODE_operator(op)) {
	  case OPR_ILOAD:
	  case OPR_ISTORE:
	    if (WN_load_offset(wn1) != WN_load_offset(wn2))
	      return FALSE;
	    return Compare(WN_kid0(wn1),WN_kid0(wn1));
	  case OPR_MLOAD:
	  case OPR_MSTORE:
	  case OPR_ILOADX:
	  case OPR_ISTOREX:
	    if (WN_load_offset(wn1) != WN_load_offset(wn2))
	      return FALSE;
	    BOOL tmp = Compare(WN_kid0(wn1),WN_kid0(wn1));
	    if (tmp == FALSE)
	      return FALSE;
	    return Compare(WN_kid1(wn1),WN_kid1(wn1));
	  case OPR_LDID:
	  case OPR_STID:
	    if (WN_load_offset(wn1) != WN_load_offset(wn2))
	      return FALSE;
	    return WN_st(wn1) == WN_st(wn2);
	  default:
	    return FALSE;
	  }
/*
	OPR_ARRAY: SIMPNODE_num_dim
	  SIMPNODE_element_size
	  compare SIMPNODE_array_base
	  for (numdim)
	    compare SIMPNODE_array_index
	    compare SIMPNODE_array_dim
*/
      }
      return FALSE;
    }

  // Add a unique element
  void Add( WN *const wn )
    { // check for duplicates first
      // take advantage of the fact that wn is a load or store.
      RBI_WN_REF *tmp;
      BOOL found = FALSE;
      for (tmp = Head(); tmp != NULL; tmp = tmp->Next()) {
	if (Compare(wn,tmp->Wn())) {
	  found = TRUE;
	  break;
	}
      }
      if (!found)
	Prepend(wn);
    }

  // prepend even if there are duplicates
  void Prepend( WN *const wn )
    {
      RBI_WN_REF *tmp = CXX_NEW(RBI_WN_REF(wn), _rbi->Mem_Pool());
      SLIST::Prepend(tmp);
    }
};

class RBI_WN_SET_ITER : public SLIST_ITER {
  DECLARE_SLIST_ITER_CLASS( RBI_WN_SET_ITER, RBI_WN_REF, RBI_WN_SET );
};

class RBI_WN_USED_SET : public RBI_WN_SET {
private:
  BOOL _all_aliasable;	// Are all Aliasable items in the set?
			// See RBI_WN_Aliasable's comments below for
			// the meaning of "Aliasable."

public:
  RBI_WN_USED_SET( RBI *const rbi ) : RBI_WN_SET( rbi )
    { _all_aliasable = FALSE; }

  void Add( WN *const wn )
    { if (!_all_aliasable || !RBI_WN_Aliasable(wn))
	RBI_WN_SET::Add(wn);
    }

  void Set_All_Aliasable( void )
    {
      _all_aliasable = TRUE;

      // Iterate down the list, removing nodes that are subsumed by
      // _all_aliasable. May not be safe to use standard list iterator
      // when modifying the shape of the list, so we use low-level
      // member functions.
      RBI_WN_REF      *prev = NULL;
      RBI_WN_REF      *curr = Head();

      while (curr != NULL) {
	if (curr->Aliasable()) {
	  Remove(prev, curr);
	  curr = curr->Next();
	  CXX_DELETE( curr, _rbi->Mem_Pool() );
	}
	else {
	  prev = curr;
	  curr = curr->Next();
	}
      }
    }

  BOOL All_Aliasable( void )		{ return _all_aliasable; }

  void Print(FILE *FD)
    {
      if (All_Aliasable())
	fprintf(FD,"<All Aliasable>\n");
      else {
	RBI_WN_REF *tmp = Head();
	if (tmp == NULL)
	  fprintf(FD,"    NULL\n");
	else {
	  while (tmp != NULL) {
	    tmp->Print(FD);
	    tmp = tmp->Next();
	  }
	}
      }
    }
};

class RBI_WN_DEF_SET : public RBI_WN_SET {
private:
  // Are all Aliasable items live-out/dead-out? See RBI_WN_Aliasable's
  // comments below for the meaning of "Aliasable."
  BOOL _all_aliasable[RBI_NUM_LIVENESS];

public:
  RBI_WN_DEF_SET( RBI *const rbi ) : RBI_WN_SET( rbi )
    { _all_aliasable[RBI_LIVE] = FALSE;
      _all_aliasable[RBI_DEAD] = FALSE;
    }

  void Add( WN *const wn, const RBI_LIVENESS_STATE state )
    {
      // If the WN is dead and aliasable, we put it explicitly in the
      // set only if _all_aliasable[RBI_DEAD] is false;
      // If the WN is live and aliasable, we put it explicitly in the
      // set only if _all_aliasable[RBI_LIVE] is false.
      if (!((_all_aliasable[RBI_LIVE] && (state == RBI_LIVE)) &&
	    (_all_aliasable[RBI_DEAD] && (state == RBI_DEAD))) ||
	  !RBI_WN_Aliasable(wn)) {
	RBI_WN_SET::Add(wn);
      }
    }

  void Set_All_Aliasable( const RBI_LIVENESS_STATE state )
    {
      if (state == RBI_LIVE) {
	_all_aliasable[RBI_LIVE] = TRUE;
	_all_aliasable[RBI_DEAD] = FALSE;
      } else
	_all_aliasable[RBI_DEAD] = TRUE;

      // Iterate down the list, removing nodes that are subsumed by
      // _all_aliasable[]. May not be safe to use standard list
      // iterator when modifying the shape of the list, so we use
      // low-level member functions.
      RBI_WN_REF      *prev = NULL;
      RBI_WN_REF      *curr = Head();

      while (curr != NULL) {
	// If all aliasable items are dead out, there can still be
	// individually-named aliasable items that are live-out. We
	// keep such items in the set explicitly.
	if (curr->Aliasable() &&
	    (_all_aliasable[RBI_LIVE] ||
	     ((_rbi->Get_WN_Prop(curr->Wn()) & WN_PROP_LIVE_OUT) == 0))) {
	  Remove(prev, curr);
	  curr = curr->Next();
	  CXX_DELETE( curr, _rbi->Mem_Pool() );
	} else {
	  prev = curr;
	  curr = curr->Next();
	}
      }
    }

  BOOL All_Aliasable( const RBI_LIVENESS_STATE state )
    { return _all_aliasable[state]; }

  void Print(FILE *FD)
    {
      if (All_Aliasable(RBI_LIVE))
	fprintf(FD,"<All Aliasable>\n");
      else {
	RBI_WN_REF *tmp = Head();
	if (tmp == NULL)
	  fprintf(FD,"    NULL\n");
	else {
	  while (tmp != NULL) {
	    tmp->Print(FD);
	    tmp = tmp->Next();
	  }
	}
      }
    }
};

// -----------------------------------------------------------------
// Member functions for class RBI
// -----------------------------------------------------------------

// constructor
RBI::RBI(MEM_POOL *RBI_pool)
{
  if (Get_Trace(TP_REGION, TT_REGION_RBI_DEBUG)) {
    _trace_level = 2;	// just RBI debug set --> detailed debug
    if (Get_Trace(TP_REGION, ~TT_REGION_RBI_DEBUG))
      _trace_level = 1;	// other bits are set also --> no detailed debug
  } else
    _trace_level = 0;

  _pool = RBI_pool;

  _prop_map = WN_MAP32_Create( _pool );
  FmtAssert(_prop_map != (WN_MAP) -1,("RBI::RBI Cannot create RBI._prop_map"));

  _tag_map = WN_MAP32_Create( _pool );
  FmtAssert(_tag_map != (WN_MAP) -1,("RBI::RBI Cannot create RBI._tag_map"));

  MEM_POOL_Initialize( _pool, "Region Boundary Info pool", FALSE );
  MEM_POOL_Push( _pool );
}

// destructor
RBI::~RBI(void)
{
  WN_MAP_Delete( _prop_map );
  MEM_POOL_Pop( _pool );
  MEM_POOL_Delete( _pool );
}

// OR a mask into a WN property word.
void
RBI::Set_WN_Prop(WN *const wn, const INT32 prop)
{
  INT32 curprop = WN_MAP32_Get( _prop_map, wn );
  curprop |= (INT32) prop;
  WN_MAP32_Set( _prop_map, wn, curprop );
}

// -----------------------------------------------------------------
// WN analysis functions
// -----------------------------------------------------------------

// A WN is considered Aliasable if it refers to an item that could be
// referenced by a pointer with an unknown value. For example, WN's
// referring to globals and items that are address_taken_saved are
// aliasable. Those referring to PREGS and locals that are not
// address_taken are not aliasable.

static BOOL
RBI_WN_Aliasable(const WN *const wn)
{
  const OPCODE opc = WN_opcode(wn);
  const OPERATOR opr = OPCODE_operator(opc);

  if (opc == OPC_FUNC_ENTRY) // TODO: Is this correct for fortran?
      return FALSE;

  if (opr == OPR_LDID || opr == OPR_STID) {
    // Examine the storage class of the WN's ST and see whether it's
    // address-taken or not.
    // case 1: IPA is not run --> assume all globals are address taken
    // case 2: IPA is run --> look at address taken bit for globals
    if (ST_is_global(WN_st(wn)) && !SYMTAB_IPA_on(Global_Symtab))
      return FALSE;
    else
      return ST_addr_taken(WN_st(wn));
  }

  if (opc == OPC_RETURN || opr == OPR_CALL)
    return TRUE;

  if (opr == OPR_ILOAD || opr == OPR_ISTORE) {
    // Determine whether this is an array operation. If not, return
    // TRUE. If so, return FALSE.
    WN *kid = WN_kid0(wn);
    return WN_operator(kid) != OPR_ARRAY;
  }

  FmtAssert(FALSE, ("Unknown opcode in BOOL RBI_WN_Aliasable( WN* )"));
  return TRUE;
}

// -----------------------------------------------------------------
// Does this WN constitute a potential use of a variable?

inline static BOOL
OPCODE_is_use(OPCODE op)
{
  if (op == OPC_RETURN)
    return TRUE;

  OPERATOR opr = OPCODE_operator(op);
  return (opr == OPR_LDID || opr == OPR_ILOAD || opr == OPR_CALL);
}

// -----------------------------------------------------------------
// Does this WN constitute a potential def of a variable?

inline static BOOL
OPCODE_is_def(OPCODE op)
{
  if (op == OPC_FUNC_ENTRY)
    return TRUE;

  OPERATOR opr = OPCODE_operator(op);
  return (opr == OPR_STID || opr == OPR_ISTORE || opr == OPR_CALL);
}

// -----------------------------------------------------------------
// Does this ILOAD access an element of an array with known address?

static BOOL
Is_Array_ILOAD( const WN *const wn )
{
  Is_True(WN_operator(wn)==OPR_ILOAD,("Non-ILOAD passed to Is_Array_ILOAD"));
  return WN_operator(WN_kid0(wn)) == OPR_ARRAY;
}

// -----------------------------------------------------------------
// Does this ISTORE assign to an element of an array with known
// address?

static BOOL
Is_Array_ISTORE( const WN *const wn )
{
  Is_True(WN_operator(wn)==OPR_ISTORE,("Non-ILOAD passed to Is_Array_ISTORE"));
  return WN_operator(WN_kid0(wn)) == OPR_ARRAY;
}

// -----------------------------------------------------------------
// Functions to deal with PREG_LISTs and PREGs.
// -----------------------------------------------------------------

inline static BOOL
Is_PREG(ST *st)
{
  return ST_symclass(st) == CLASS_PREG;
}

inline static BOOL
Find_PREG(PREG_LIST *l, PREG_NUM p_num)
{
  for (PREG_LIST *ptmp = l; ptmp; ptmp = PREG_LIST_rest(ptmp)) {
    if (PREG_LIST_first(ptmp) == p_num)
      return TRUE;
  }
  return FALSE;
}

inline static void
Add_To_PREG_LIST(PREG_LIST **l, WN *wn)
{
  // search for duplicates first
  PREG_NUM p_num = WN_load_offset(wn);
  if (!Find_PREG(*l,p_num))
    *l = PREG_LIST_Push(p_num,*l,&REGION_mem_pool);
}

inline static void
Add_To_All_PREG_LISTs(PREG_LIST **l, WN *wn, INT32 n)
{
  for (INT32 i = 0; i < n; i++)
    Add_To_PREG_LIST(&l[i], wn);
}

// -----------------------------------------------------------------
// Functions to deal with POINTS_TOs and sets of POINTS_TOs
// -----------------------------------------------------------------

BOOL RBI_PT_Aliasable( const POINTS_TO *const pt )
{
  // Note: FALSE is the conservative answer to return here because it
  // forces the POINTS_TO_SET routines to keep all the POINTS_TO's in
  // the set in question.
  Is_True(pt != NULL,("RBI_PT_Aliasable, NULL pt"));
  return FALSE;
}

// -----------------------------------------------------------------
// Add an element to a set of POINTS_TOs.
// the POINTS_TO_REF and POINTS_TO must be allocated from REGION_mem_pool
void
RBI::Add_To_PT_SET(POINTS_TO_SET *pt_set, POINTS_TO *pt)
{
  POINTS_TO_REF *tmp;

  /* Make sure pt isn't already in the set. */
  for (tmp = pt_set->Elements; tmp != NULL; tmp = tmp->Next) {
    if (tmp->Pt == pt) // PPP need a pt comparison function
      return;
  }
  POINTS_TO_REF *ptr = TYPE_MEM_POOL_ALLOC(POINTS_TO_REF, &REGION_mem_pool);
  ptr->Next = pt_set->Elements;
  ptr->Pt = Points_to_copy(pt,&REGION_mem_pool);
  pt_set->Elements = ptr;
}

// -----------------------------------------------------------------
// Mark a POINTS_TO set as containing all aliasable items and remove
// its aliasable (redundant) singleton elements.
void
RBI::Set_PT_SET_All_Aliasable(POINTS_TO_SET *pt_set)
{
  pt_set->All_Aliasable = TRUE;

#if 0
// this loop doesn't do anything since RBI_PT_Aliasable always returns FALSE
  POINTS_TO_REF *tmp1, *tmp2;
  while (pt_set->Elements != NULL) {
    if (RBI_PT_Aliasable(pt_set->Elements->Pt)) {
      tmp1 = pt_set->Elements;
      pt_set->Elements = tmp1->Next;
      MEM_POOL_FREE(&REGION_mem_pool, (void *) tmp1);
    } else
      break;
  }
#endif

#if 0
// this loop doesn't do anything since RBI_PT_Aliasable always returns FALSE
  if (pt_set->Elements != NULL) {
    tmp1 = pt_set->Elements;
    tmp2 = pt_set->Elements->Next;

    while (tmp2 != NULL) {
      if (RBI_PT_Aliasable(tmp2->Pt)) {
	tmp1->Next = tmp2->Next;
	MEM_POOL_FREE(&REGION_mem_pool, (void *) tmp2);
      } else
	tmp1 = tmp2;
      tmp2 = tmp1->Next;
    }
  }
#endif
}

// Calculate region boundary information for the given RID node. This
// is where most of the work gets done.
//
// We make two passes through the WHIRL tree for the region:
//
// The first pass labels the nodes in the region so we can quickly
// tell whether any particular node lies in the region. Also in the
// first pass, we collect a list of the region's nodes that we care
// about. This enables the second pass to skip quickly over irrelevant
// nodes that lie within the region.
//
// The second pass examines the relevant nodes gathered in the first
// pass and classifies them and their uses and defs according to their
// liveness status across the region entry and region exit. Items that
// are used inside the region and potentially defined outside are
// considered Used In the region, and are placed in the set
// used_in. Items that are defined inside the region are considered
// Defined In the region, and are placed in the set def_in. Each WHIRL
// node in that set carries a mark according to whether or not its
// data item(s) is (are) potentially used outside the region. Those
// that are potentially used outside are considered Live Out; others
// are considered Dead Out.
//
// After we have classified the region's WHIRL nodes and the
// extra-region WHIRL nodes that use or define items defined or used
// inside the region, we "lower" the sets of WHIRL nodes to sets of
// PREGs and POINTS_TO structures. The POINTS_TO structure is a unit
// used in WOPT's alias manager to keep track of the set of memory
// locations that could be accessed by a given WHIRL node. Because
// POINTS_TOs are required to be small in the current implementation,
// the information they can represent is sometimes not very precise
// (and is therefore very conservative in some cases).
//
// TODO items for RBI_Calc_Rgn:
// *) Consider using data flow equations to calculate region boundary
//    information for nested regions. Presently, each WHIRL node is
//    examined once for each region in which it participates. If
//    regions are nested deeply, this can be expensive. However, if
//    region nesting is shallow, the current approach is cheaper than
//    the data-flow approach because data-flow equations require that
//    more boundary information be maintained.
// *) Make the code's assumptions less conservative. There are many,
//    many areas where the current implementation does not use all the
//    available information. In particular, when a D-U or U-D chain is
//    found to be incomplete, we essentially give up and make a
//    wide-ranging conservative assumption. Instead, we should consult
//    the alias manager to narrow down the sets of items that could be
//    used or defined by an operation with an incomplete chain. At
//    present, the alias manager is even more conservative in many
//    cases than this code, but if we use it, we will inherit for free
//    whatever future improvements are made there.
// *) The operators and opcodes handled here are almost certainly a
//    proper subset of the ones required for correctness, particularly
//    in the area of intrinsic and I/O intrinsic calls.
// *) Each region can have multiple exits, and the current declaration
//    of the output set pregs_out reflects this. The current structure
//    of the output set def_in_live_out does NOT reflect this,
//    however. Regardless that the pregs_out sets are represented in a
//    manner general enough to support independent live-out
//    information for each exit from the region, the current
//    implementation does not gather information on a per-exit
//    basis. Instead, every preg that is live out across any exit is
//    placed in the sets corresponding to ALL the exits. Ultimately,
//    liveness information should be computed independently for all
//    the exits from the region; this will require major changes in
//    the techniques used, since the D-U/U-D chains carry no
//    information about which control-flow path(s) each use or
//    definition is conditioned upon.
// *) RBI_Calc_Rgn should really be split up into smaller pieces,
//    perhaps one for each pass through the region's WHIRL, and one
//    more for building the final output sets that go in the RID.
// *) The code to build the final output sets is currently messy and
//    contains lots of deeply-nested if-then-else cases. This could be
//    cleaned up and made somewhat easier to read by writing a few
//    little functions to do the middle-level set manipulations.
// *) Code should be added to produce useful tracing information. This
//    addition will probably happen naturally as the implementation is
//    tested and debugged.

void
RBI::RBI_Calc_Rgn(RID *const rid_node, const DU_MANAGER *const du_mgr,
		  const ALIAS_MANAGER *const am)
{
  WN *const wn = RID_rwn(rid_node);
  Is_True(wn != NULL,("RBI::RBI_Calc_Rgn, NULL WHIRL tree"));
  if (RID_type(rid_node) & RID_TYPE_func_entry) // don't do PU rid
    return;
  RBI_WN_SET relevant_nodes(this);
  RID_level(rid_node) = RL_RBI; // RID has been processed by RBI
  RID_bounds_exist(rid_node) = REGION_BOUND_EXISTS; // boundaries now exist

  Is_True(REGION_get_rid(wn) == rid_node,
	  ("RBI::RBI_Calc_Rgn: RID refers to incorrect WHIRL node"));
  Is_True(RID_rwn(rid_node) == wn,
	  ("RBI::RBI_Calc_Rgn: inconsistency between RID and WHIRL"));
  Is_True(WN_operator(wn) == OPR_REGION,
	  ("RBI::RBI_Calc_Rgn: RID refers to non-REGION node"));

  //======================================================================
  /* First, we mark those WN's that participate in the current region,
   * so we can quickly distinguish "inside" from "outside" in the
   * coming analysis. In the same pass over the tree, we accumulate a
   * list of nodes we will have to look at again (uses and defs).
   */
  if (Trace())
    fprintf(TFile,"===== RBI::RBI_Calc_Rgn (RGN %d): marking nodes\n",
	    RID_id(rid_node));
  for (WN_ITER *wni=WN_WALK_TreeIter(wn); wni; wni = WN_WALK_TreeNext(wni)) {
    WN *wtmp = WN_ITER_wn(wni);
    Set_WN_Cur_Rgn(wtmp,RID_id(rid_node)); // mark this WN with region id
    OPCODE op = WN_opcode(wtmp);
    if (OPCODE_is_use(op) || OPCODE_is_def(op)) // mark loads and stores
      relevant_nodes.Prepend(wtmp);
  }

  // allocate space for preg_out sets in RID
  Is_True(RID_num_exits(rid_node) > 0,
	  ("RBI::RBI_Calc_Rgn, incorrect # exits"));
  Is_True(RID_pregs_out(rid_node) == NULL,
	  ("RBI::RBI_Calc_Rgn, preg out set exists"));
  RID_pregs_out(rid_node) = TYPE_MEM_POOL_ALLOC_N(PREG_LIST *,
				  &REGION_mem_pool,RID_num_exits(rid_node));

  //======================================================================
  // second pass, go through relevant nodes in region and fill in sets
  if (Trace())
    fprintf(TFile,"===== RBI::RBI_Calc_Rgn (RGN %d): use_list, def_list\n",
	    RID_id(rid_node));

  RBI_WN_USED_SET used_in(this);
  RBI_WN_DEF_SET def_in(this);

  RBI_WN_SET_ITER relevant_iter;
  RBI_WN_REF *wnr;

  FOR_ALL_NODE(wnr, relevant_iter, Init(&relevant_nodes)) {
    WN *const wn = wnr->Wn();
    USE_LIST *use_list;
    DEF_LIST *def_list;
    OPERATOR opr = WN_operator(wn);

    if (Trace2()) {
      fprintf(TFile,"wn: ");
      fdump_wn(TFile,wn);
    }

#if 0
    /* Can't use the following assertion because, for example, an
     * empty procedure that returns no value will have a OPC_RETURN
     * with no uses or defs even though OPCODE_is_use(OPC_RETURN) == TRUE. 
     */
    FmtAssert(use_list != NULL || def_list != NULL,
	      ("RBI_Calc_Rgn: Relevant WN with no uses and no defs\n"));
#endif

    if (opr == OPR_CALL) {
      /* A call inside the region is assumed to use and def all
       * address-taken items. Since the PU must return, all
       * address-taken items that survive the return from this PU must
       * be assumed to be live-out. For the moment, we make the
       * simplifying hack of including all address-taken items in the
       * live-out set. Later, we can distinguish based on storage
       * class, defining PU, etc., to place appropriate things (e.g.,
       * locals allocated on the stack) in the def_in_dead_out
       * set. */
      used_in.Set_All_Aliasable();
      def_in.Set_All_Aliasable(RBI_LIVE);
      continue;
    }

    // ISTOREs and ILOADs have no DU chain - must assume ILOADs
    // are live-in and ISTOREs are live-out
    if (opr == OPR_ISTORE) {
      def_in.Add(wn, RBI_LIVE);
      Set_WN_Prop(wn, WN_PROP_DEF_IN | WN_PROP_LIVE_OUT);
      goto bottom;
    } else if (opr == OPR_ILOAD) {
      used_in.Add(wn);
      Set_WN_Prop(wn, WN_PROP_USED_IN);
      goto bottom;
    }

    use_list = du_mgr->Du_Get_Use(wn);
    def_list = du_mgr->Ud_Get_Def(wn);
    if (Trace2()) {
      if (use_list) {
	fprintf(TFile,"use_list: ");
#ifdef AFTER_MR
	use_list->Print(TFile);
#endif
	fprintf(TFile,"\n");
      } else
	fprintf(TFile,"use_list: NULL\n");
      if (def_list) {
	fprintf(TFile,"def_list: ");
#ifdef AFTER_MR
	def_list->Print(TFile);
#endif
	fprintf(TFile,"\n");
      } else
	fprintf(TFile,"def_list: NULL\n");
    }

    Is_True(WN_opcode(wn) != OPC_RETURN || use_list == NULL,
	    ("RBI_Calc_Rgn: OPC_RETURN has non-NULL use-list"));

    /* It seems unlikely that there can be non-address-taken items
     * on the use- or def-list of a call, but even so, we go ahead
     * and execute the relevant code to handle them regardless of
     * operator.
     */
    if (use_list != NULL) {
      if (use_list->Incomplete()) {
	if (Trace2())
	  fprintf(TFile,"use_list incomplete\n");

	// For function entries, direct stores, and array element
	// assignments, we need only be pessimistic about the liveness
	// information since we know exactly what range of addresses
	// could be involved. For other defs, we must be pessimistic
	// about the set of addresses as well.
	if (WN_operator(wn) == OPR_STID ||
	    WN_operator(wn) == OPR_FUNC_ENTRY || Is_Array_ISTORE(wn)) {
	  def_in.Add(wn, RBI_LIVE);
	  Set_WN_Prop(wn, WN_PROP_DEF_IN | WN_PROP_LIVE_OUT);
	} else
	  def_in.Set_All_Aliasable( RBI_LIVE );
      }
      else {
	if (Trace2())
	  fprintf(TFile,"use_list complete\n");

	// use_list is a complete D-U chain for *wn.
	USE_LIST_CONST_ITER use_list_iter;
	const DU_NODE *tmp;

	// If *wn is a function entry or direct or array store, it
	// represents itself. Otherwise, if all its cross-boundary
	// uses are direct or array loads, they represent it. Finally,
	// if *wn is a non-array indirect or a CALL and at least one
	// of its cross-boundary uses is a non-array indirect, a CALL,
	// or a RETURN, we take all aliasable items to be in the
	// def-in-live-out set for this region.
	if (WN_operator(wn) == OPR_STID || WN_operator(wn) == OPR_FUNC_ENTRY ||
	    Is_Array_ISTORE(wn)) {
	  def_in.Add(wn, RBI_DEAD);
	  Set_WN_Prop(wn, WN_PROP_DEF_IN);

	  // Still need to determine whether the WN is live out.
	  FOR_ALL_NODE( tmp, use_list_iter, Init(use_list) ) {
	    if (Outside_Cur_Rgn(tmp->Wn(), RID_id(rid_node))) {
	      Set_WN_Prop(wn, WN_PROP_LIVE_OUT);
	      break;
	    }
	  }
	} else {
	  // *wn is not a function entry or a direct or array store.
	  RBI_LIVENESS_STATE state = RBI_DEAD;

	  FOR_ALL_NODE ( tmp, use_list_iter, Init(use_list) ) {
	    if (Outside_Cur_Rgn(tmp->Wn(), RID_id(rid_node))) {
	      state = RBI_LIVE;
	      if ((WN_operator(tmp->Wn()) == OPR_LDID) ||
		  Is_Array_ILOAD(tmp->Wn())) {
		def_in.Add(tmp->Wn(), RBI_LIVE);
		Set_WN_Prop(wn, WN_PROP_DEF_IN | WN_PROP_LIVE_OUT);
	      } else {
		def_in.Set_All_Aliasable( RBI_LIVE );
		break;
	      }
	    }
	  }
	  // If we didn't find a use outside the region, we take all
	  // aliasable items to be in the def-in-dead-out set for this
	  // region.
	  if (state == RBI_DEAD)
	    def_in.Set_All_Aliasable( RBI_DEAD );
	}
      }
    }

    if (def_list != NULL) {
      if (def_list->Incomplete()) {
	if (Trace2())
	  fprintf(TFile,"def_list incomplete\n");
	/* For direct loads and array element accesses, we need only
	 * be pessimistic about the liveness information since we know
	 * exactly what range of addresses could be involved. For
	 * other uses, we must be pessimistic about the set of
	 * addresses as well.
	 */
	if ((WN_operator(wn) == OPR_LDID) || Is_Array_ILOAD(wn)) {
	  used_in.Add(wn);
	  Set_WN_Prop(wn, WN_PROP_USED_IN);
	} else
	  used_in.Set_All_Aliasable();
      }
      else {
	if (Trace2())
	  fprintf(TFile,"def_list complete\n");

	// def_list is a complete U-D chain for *wn.
	DEF_LIST_CONST_ITER def_list_iter;
	const DU_NODE *tmp;

	// If *wn is a direct or array load, it represents
	// itself and gets included in used_in if it has any
	// cross-boundary def. If *wn is not a direct or array load,
	// if all its cross-boundary defs are function entries or
	// direct or array stores, they represent it. Finally, if *wn
	// is a non-array indirect, a CALL, or a RETURN, and at least
	// one of its cross-boundary defs is a non-array indirect or a
	// CALL, we take all aliasable items to be in the used-in set
	// for this region.
	if ((WN_operator(wn) == OPR_LDID) || Is_Array_ILOAD(wn)) {
	  FOR_ALL_NODE( tmp, def_list_iter, Init(def_list) ) {
	    if (Outside_Cur_Rgn(tmp->Wn(), RID_id(rid_node))) {
	      if (Trace2())
		fprintf(TFile,"def is outside region\n");
	      used_in.Add(wn);
	      Set_WN_Prop(wn, WN_PROP_USED_IN);
	      break;
	    }
	  }
	} else {
	  // *wn is not a direct or array load. If all the
	  // cross-boundary WN's on its def-list are function entries
	  // or direct or array stores, we place those WN's in (*wn)'s
	  // used-in set. Otherwise, we place take all aliasable
	  // objects to be in (*wn)'s used-in set.
	  FOR_ALL_NODE( tmp, def_list_iter, Init(def_list) ) {
	    if (Outside_Cur_Rgn(tmp->Wn(), RID_id(rid_node))) {
	      if ((WN_operator(tmp->Wn()) == OPR_STID) ||
		  (WN_operator(tmp->Wn()) == OPR_FUNC_ENTRY) ||
		  Is_Array_ISTORE(tmp->Wn())) {
		used_in.Add(tmp->Wn());
		Set_WN_Prop(tmp->Wn(), WN_PROP_USED_IN);
	      } else {
		used_in.Set_All_Aliasable();
		break;
	      }
	    }
	  }
	}
      }
    } // if (def_list != NULL)

bottom:
    if (Trace2()) {
      fprintf(TFile,"Bottom of loop, used_in:\n");
      used_in.Print(TFile);
      fprintf(TFile,"Bottom of loop, def_in:\n");
      def_in.Print(TFile);
      fprintf(TFile,"\n");
    }

  } // FOR_ALL_NODE(wnr, relevant_iter, Init(&relevant_nodes))

  // We have maintained the invariant that for each of the sets def_in
  // and used_in, either the set has been Set_All_Aliasable, or it
  // contains only WN's from which we can easily derive ST's (and
  // hence POINTS_TO's that aren't too conservative). Such WN's are
  // LDID/STID, ILOAD/ISTORE for array access, and FUNC_ENTRY. We are
  // guaranteed no RETURNs or CALLs in the sets.
  //
  // Taking advantage of this invariant now, we resolve the two sets
  // of WN's into three sets of POINTS_TO's that reflect used_in,
  // def_in_live_out, and def_in_dead_out for the region and two sets
  // of PREGs that reflect pregs_in (PREGs used-in) and pregs_out
  // (PREGS live-out).
  //
  // Calls to add members to sets of POINTS_TO's in the following are
  // unappealing in structure because region_util.h has to be written
  // in C (because lnodriver.c has to be able to include it). Maybe in
  // the future this relationship can be unwound to the degree
  // necessary to implement RID as a C++ class.

  if (Trace2()) {
    fprintf(TFile,"===== RBI::RBI_Calc_Rgn (RGN %d): used_in/pregs_in sets\n",
	    RID_id(rid_node));
    fprintf(TFile,"used_in:\n");
    used_in.Print(TFile);
  }

  RBI_WN_SET_ITER  use_def_iter;

  if (used_in.All_Aliasable())
    Set_PT_SET_All_Aliasable(&RID_used_in(rid_node));

  FOR_ALL_NODE( wnr, use_def_iter, Init(&used_in) ) {
    WN *wn = wnr->Wn();
    if (Trace2()) {
      fprintf(TFile,"used_in wn: ");
      fdump_wn(TFile,wn);
    }
    Is_True(!(used_in.All_Aliasable() && RBI_WN_Aliasable(wn)),
	    ("Aliasable WN in used-in set marked all aliasable"));

/*PPP is this code ever executed? */
    if (WN_opcode(wn) == OPC_FUNC_ENTRY) {
      INT i;
      for (i = WN_num_formals(wn) - 1; i >= 0; i--) {
	WN *kid = WN_kid(wn, i);
	FmtAssert(WN_opcode(kid) == OPC_IDNAME,
		  ("Non-IDNAME function parameter"));
	if (Is_PREG(WN_st(kid))) {
	  Add_To_PREG_LIST(&RID_pregs_in(rid_node), kid);
	} else {
	  // Put the POINTS_TO referred to by this OPC_IDNAME kid
	  // (through the alias manager) into this RID's used_in set.
#ifdef AFTER_MR
	  Add_To_PT_SET(&RID_used_in(rid_node), am->Pt(am->Id(kid)));
#endif
	}
      }
    } else {
      if (Is_PREG(WN_st(wn))) {
	Add_To_PREG_LIST(&RID_pregs_in(rid_node), wn);
	if (Trace2())
	  fprintf(TFile,"Adding to pregs_in list\n");
      } else {
	// Put the POINTS_TO referred to by this WN (through the alias
	// manager) into this RID's used_in set.
#ifdef AFTER_MR
	Add_To_PT_SET(&RID_used_in(rid_node), am->Pt(am->Id(wn)));
#endif
	if (Trace2())
	  fprintf(TFile,"Adding to used_in list\n");
      }
    }
  } // FOR_ALL_NODE( wnr, use_def_iter, Init(&used_in) )

  //------------------------------------------------------------------------
  if (Trace2()) {
//    fprintf(TFile,"pregs_in:\n");
//    Dump_preg_list(RID_pregs_in(rid_node));
//    Dump_points_to_list(RID_used_in(rid_node),"used_in:");
    fprintf(TFile,"===== RBI::RBI_Calc_Rgn (RGN %d): def_in_* sets\n",
	    RID_id(rid_node));
    fprintf(TFile,"def_in:\n");
    def_in.Print(TFile);
  }

  if (def_in.All_Aliasable(RBI_LIVE))
    Set_PT_SET_All_Aliasable(&RID_def_in_live_out(rid_node));
  else if (def_in.All_Aliasable(RBI_DEAD))
    Set_PT_SET_All_Aliasable(&RID_def_in_dead_out(rid_node));

  FOR_ALL_NODE( wnr, use_def_iter, Init(&def_in) ) {
    WN   *wn = wnr->Wn();
    BOOL  live_out = ((Get_WN_Prop(wn) & WN_PROP_LIVE_OUT) != 0);
    if (Trace2()) {
      fprintf(TFile,"def wn: <%s> <%s> ",live_out?"live_out":"dead_out",
	  def_in.All_Aliasable(RBI_LIVE)?"aliasable live":
	  (def_in.All_Aliasable(RBI_DEAD)?"aliasable dead":"not aliasable"));
      fdump_wn(TFile,wn);
    }

    Is_True(!(RBI_WN_Aliasable(wn) && live_out &&
	      def_in.All_Aliasable(RBI_LIVE)),
	    ("Live out aliasable WN in def_in when all aliasable(RBI_LIVE)"));
    Is_True(!(RBI_WN_Aliasable(wn) && !live_out &&
	      def_in.All_Aliasable(RBI_DEAD)),
	    ("Dead out aliasable WN in def_in when all aliasable(RBI_DEAD)"));

    if (live_out || !def_in.All_Aliasable(RBI_DEAD)) {
      // In the present implementation, FUNC_ENTRY nodes cannot appear
      // inside regions, but we handle them here anyway.
      if (WN_opcode(wn) == OPC_FUNC_ENTRY) {
	for (INT i = WN_num_formals(wn) - 1; i >= 0; i--) {
	  WN *kid = WN_kid(wn, i);
	  FmtAssert(WN_opcode(kid) == OPC_IDNAME,
		    ("Non-IDNAME function parameter"));

	  if (live_out) {
	    if (Is_PREG(WN_st(wn))) {
	      if (Trace2())
		fprintf(TFile,"Adding to pregs_out list\n");
	      // For now, a conservative approach.
	      Add_To_All_PREG_LISTs(RID_pregs_out(rid_node),kid,
				    RID_num_exits(rid_node));
	    } else {
	      if (Trace2())
		fprintf(TFile,"Adding to def_in_live_out list\n");
	      // Put the POINTS_TO referred to by this OPC_IDNAME kid
	      // (through the alias manager) into this RID's
	      // def_in_live_out set.
#ifdef AFTER_MR
	      Add_To_PT_SET(&RID_def_in_live_out(rid_node),
			    am->Pt(am->Id(kid)));
#endif
	    }
	  } else {
	    // No one cares about dead-out PREGs. Here is where we
	    // discard them.
	    if (!Is_PREG(WN_st(wn))) {
	      if (Trace2())
		fprintf(TFile,"Adding to def_in_dead_out list\n");
#ifdef AFTER_MR
	      Add_To_PT_SET(&RID_def_in_dead_out(rid_node),
			    am->Pt(am->Id(kid)));
#endif
	    }
	  }
	}
      } // if (WN_opcode(wn) == OPC_FUNC_ENTRY)
      else {
	if (live_out) {
	  if (Is_PREG(WN_st(wn))) {
	    if (Trace2())
	      fprintf(TFile,"Adding to pregs_out list (2)\n");
	    // For now, a conservative approach.
	    Add_To_All_PREG_LISTs(RID_pregs_out(rid_node),wn,
				  RID_num_exits(rid_node));
	  } else {
	    // Put the POINTS_TO referred to by this WN (through the alias
	    // manager) into this RID's def_in_{live|dead}_out set.
	    if (Trace2())
	      fprintf(TFile,"Adding to def_in_live_out list (2)\n");
#ifdef AFTER_MR
	    Add_To_PT_SET(&RID_def_in_live_out(rid_node),am->Pt(am->Id(wn)));
#endif
	  }
	} else {
	  // No one cares about dead-out PREGs. Here is where we
	  // discard them.
	  if (!Is_PREG(WN_st(wn))) {
	    if (Trace2())
	      fprintf(TFile,"Adding to def_in_dead_out list (2)\n");
#ifdef AFTER_MR
	    Add_To_PT_SET(&RID_def_in_dead_out(rid_node),
			  am->Pt(am->Id(wn)));
#endif
	  } else {
	    if (Trace2()) { 
	      fprintf(TFile,"Discarding Dead-out PREG\n");
	      fdump_wn_no_st(TFile,wn);
	    }
	  }
	}
      }
    } // if (live_out || !def_in.All_Aliasable(RBI_DEAD)) 
  } // FOR_ALL_NODE( wnr, use_def_iter, Init(&def_in) )
  if (Trace2())
    RID_set_print(TFile,REGION_get_rid(wn));
}

// -----------------------------------------------------------------
// Recursive function to process the children of a RID node
// -----------------------------------------------------------------
void
RBI::RBI_Calc_Kids(RID *root, const DU_MANAGER *du_mgr,
		   const ALIAS_MANAGER *am)
{
  for (RID *kid = RID_first_kid(root); kid != NULL; kid = RID_next(kid)) {
    RBI_Calc_Rgn(kid, du_mgr, am);
    RBI_Calc_Kids(kid, du_mgr, am);
  }
  RID_level(root) = RL_RBI;
  RID_bounds_exist(root) = REGION_BOUND_EXISTS;
}

#endif
// -----------------------------------------------------------------
// RBI interface to the outside world.
// -----------------------------------------------------------------

extern "C"	/* so lnodriver.c can call this entry point	*/
void
Region_Bound_Info(WN *tree, DU_MANAGER *du_mgr, ALIAS_MANAGER *am)
{
#if 0
  if (!SYMTAB_has_rgn(Current_Symtab))
    return;

  FmtAssert(WN_operator(tree) == OPR_FUNC_ENTRY ||
	    WN_operator(tree) == OPR_REGION,
	    ("Region_Bound_Info: Expected FUNC_ENTRY; Got %s instead.\n",
	     OPCODE_name(WN_opcode(tree))));

  // Find the RID for this region.
  RID *root = REGION_get_rid(tree); // PU rid created by Region_Initialize
  Is_True(root != NULL,("Region_Bound_Info, NULL RID"));

  // create mem_pool and RBI class
  MEM_POOL RBI_pool;
  RBI Cur_RBI(&RBI_pool);

  // Iterate through the tree, and for each RID node we
  // discover, generate boundary information for the corresponding
  // region. The root of the RID tree is only a placeholder to
  // access the genuine top-level RIDs for this PU, so we don't
  // calculate any boundary information for it.
#ifdef AFTER_MR
  Cur_RBI.RBI_Calc_Kids(root, du_mgr, am);
#endif

  if (Cur_RBI.Trace())
    RID_set_print(TFile,REGION_get_rid(tree));
//    Cur_RBI.Dump_sets(tree);
#endif
}

