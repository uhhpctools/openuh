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


/* -*-Mode: c++;-*- (Tell emacs to use c++ mode) */
/* ====================================================================
 *
 * Module: DaVinci.h
 * $Revision: 1.1.1.1 $
 * $Date: 2005/10/21 19:00:00 $
 * $Author: marcel $
 * $Source: /proj/osprey/CVS/open64/osprey1.0/be/com/com_whirlview.cxx,v $
 * ====================================================================
 */

#include <stdio.h>
#include <string.h>
#include <vector>

#include "defs.h"
#include "errors.h"
#include "srcpos.h"
#include "opcode.h"
#include "wn.h"		/* Whirl Node descriptor */
#include "wn_simp.h"    /* Whirl simplifier (can be stubs) */

// #include "stab.h"
// #include "const.h"
// #include "targ_const.h"
// #include "strtab.h"
// #include "wio.h"
// #include "wintrinsic.h"
// #include "wn_pragmas.h"

#include "DaVinci.h"
#include "wb_util.h"
#include "com_whirlview.h"
#include "fb_whirl.h"

#define MAXEXPR 5000

static MEM_POOL DV_wv_mempool;
static BOOL     DV_wv_mempool_init = FALSE;
static DaVinci *DV      = NULL;
static WN      *Func_wn = NULL;

static void draw_expr(WN *);
static void draw_stmt(WN *);

static char *
id_str(WN *wn)
{
  static char dv_id[64];
  INT         len;

 if ( wn ) {
   // don't need unique labels for display - node_id is unique.
   len = sprintf( dv_id, "%d:%d",
		  OPCODE_mapcat(WN_opcode(wn)), WN_map_id(wn) );
 } else {
   strcpy( dv_id, "NULL-WN" );
 }

 if ( Cur_PU_Feedback ) {
   FB_FREQ freq = Cur_PU_Feedback->Query_total_out( wn );
   freq.Sprintf( dv_id + len );
 }

 return dv_id;
}

static void
draw_expr(WN *wn)

{
  NODE_TYPE nt;
  EDGE_TYPE et;
  INT       i;

  DV->Node_Begin( NODE_ID(wn), id_str(wn), nt );

  for (i = 0; i < WN_kid_count(wn); i++) {
    WN *wn2 = WN_kid(wn,i);
    DV->Out_Edge( EDGE_ID(wn, wn2), et, NODE_ID(wn2) );
  }
  DV->Node_End();

  for (i = 0; i < WN_kid_count(wn); i++) {
    draw_expr( WN_kid(wn,i) );
  }
}

static void
draw_stmt(WN *wn, BOOL show_expr)
{
  NODE_TYPE nt;
  EDGE_TYPE et;

  vector<WN*> kids;

  switch (WN_opcode(wn)) {
  case OPC_BLOCK: {
    WN *wn2 = WN_first(wn);
    while (wn2) {
      kids.push_back( wn2 );
      wn2 = WN_next(wn2);
    }
    break;
  }
  case OPC_IF:
    nt.Shape( NS_RHOMBUS );
    if ( WN_then(wn) ) {
      kids.push_back( WN_then(wn) );  // THEN.
    }
    if ( show_expr ) {
      kids.push_back( WN_if_test(wn) ); // condition-expr.
    }
    if ( WN_else(wn) ) {
      kids.push_back( WN_else(wn) );  // ELSE.
    }
    break;

  case OPC_DO_LOOP:
    nt.Shape( NS_ELLIPSE );
    if ( show_expr ) kids.push_back( WN_index(wn) );   // INDX-expr.
    kids.push_back( WN_start(wn) );   // INIT.
    if ( show_expr ) kids.push_back( WN_end(wn) );     // COMP-expr.
    kids.push_back( WN_do_body(wn) ); // BODY.
    kids.push_back( WN_step(wn) );    // INCR.
    break;

  case OPC_COMPGOTO:
    if ( show_expr ) kids.push_back( WN_kid(wn,0) );   // SWCH-expr.
    kids.push_back( WN_kid(wn,1) );   // JMPS.
    if ( WN_kid_count(wn) > 2 ) {
      kids.push_back( WN_kid(wn,2) ); // DFLT.
    }
    break;

  case OPC_XGOTO:
    if ( show_expr ) kids.push_back( WN_kid(wn,0) );   // SWCH-expr.
    kids.push_back( WN_kid(wn,1) );   // JMPS.
    break;

  default: 
    {
      for (INT i = 0; i < WN_kid_count(wn); ++i) {
	WN* wn2 = WN_kid(wn,i);
	FmtAssert(wn2, ("Null kid in draw_stmt"));
	OPCODE opc2 = WN_opcode(wn2);
	if ( show_expr && OPCODE_is_expression(opc2) ) {
	  kids.push_back( wn2 );
	} else if ( OPCODE_is_stmt(opc2) || OPCODE_is_scf(opc2) ) {
	  kids.push_back( wn2 );
	}
      }
    }
  }


  if ( Cur_PU_Feedback ) {
    FB_FREQ freq = Cur_PU_Feedback->Query_total_out( wn );
    if ( freq.Known() ) {
      nt.Color( "wheat1" );
    }
  }

  DV->Node_Begin( NODE_ID(wn), id_str(wn), nt );

  vector<WN*>::iterator wn_iter;

  for (wn_iter = kids.begin(); wn_iter != kids.end(); ++wn_iter) {
    WN *wn2 = *wn_iter;

    DV->Out_Edge( EDGE_ID(wn,wn2), et, NODE_ID(wn2) );
  }
  DV->Node_End();

  for (wn_iter = kids.begin(); wn_iter != kids.end(); ++wn_iter) {
    WN *wn2 = *wn_iter;

    draw_stmt( wn2, show_expr );
  }
}

static void
draw_whirl_tree(WN *wn, BOOL show_expr)
{
  DV->Graph_Begin();

  if ( OPCODE_is_expression(WN_opcode(wn)) ) {
    draw_expr( wn );
  } else if ( OPCODE_is_stmt(WN_opcode(wn))
	      || OPCODE_is_scf(WN_opcode(wn)) ) {
    draw_stmt( wn, show_expr );
  } else {
    FmtAssert( FALSE, ("opcode of unknown type") );
  }
  DV->Graph_End();
}

class Callback : public DaVinci_Callback {
  WN *node_sel;  // last node selected (or NULL).
public:
  virtual void Node_Select(const INT n_ids, const NODE_ID id_array[]);
  virtual void Edge_Select(const EDGE_ID& id);
  virtual void Menu_Select(const char *menu_id);
};

void
Callback::Node_Select(const INT n_ids, const NODE_ID id_array[])
{
  char buf[ MAXEXPR ]; // more: better storage management.

  for (INT i = 0; i < n_ids; ++i) {
    WN *wn      = (WN *)id_array[i];
    WN *head_wn = ( Func_wn ? Func_wn : wn );
    INT end = WB_Dump_Whirl_Expr(head_wn, wn, buf, 0);
    buf[end] = '\0';
    printf("%p: %s\n", wn, buf);
  }
  node_sel = (n_ids > 0 ? (WN *)id_array[n_ids - 1] : NULL);
}

void
Callback::Edge_Select(const EDGE_ID& id)
{
  EDGE_TYPE et;

  et.Color( "red" );
  DV->Change_Attr( id, et );  // just to ack edge select event ..
}

void
Callback::Menu_Select(const char *menu_id)
{
  if ( strcmp( menu_id, "EXPAND" ) == 0 && node_sel ) {
    printf("selecting EXPAND for node %s\n", id_str(node_sel));
    // MORE: use graph update capability to expand tree.
    //       need to track which nodes have already been plotted.
  }
}


static MENU_INFO DV_Menu[] = {
  { "EXPAND", "Expand Subtree", true, 0, NULL }
  // more: add other useful queries here.
};
#define N_DV_MENU   ( sizeof(DV_Menu) / sizeof(DV_Menu[0]) )

// more: consider du_mgr ..?

void
dV_view_whirl(WN *wn, const char *title, BOOL show_expr, FILE *trace_fp)
{
  if ( ! DaVinci::enabled(true) ) return;

  Func_wn = (WN_operator(wn) == OPR_FUNC_ENTRY ? wn : NULL);

  const char *trace_fname = getenv("DV_TRACE_FILE");
  bool        local_trace = false;

  if ( trace_fp == NULL && trace_fname ) {
    if ( (trace_fp = fopen(trace_fname, "w")) != NULL ) {
      local_trace = true;
    } else {
      fprintf(stderr, "DV_TRACE_FILE not writeable\n");
      perror( trace_fname );
    }
  }
  FmtAssert( DV == NULL, ("dV_view_fb_cfg: DV is null"));
  if ( ! DV_wv_mempool_init ) {
    MEM_POOL_Initialize(&DV_wv_mempool, "DV_wv_mempool", FALSE);
    DV_wv_mempool_init = TRUE;
  }
  DV = CXX_NEW(DaVinci(&DV_wv_mempool, trace_fp), &DV_wv_mempool);

  const char *window_title = (title ? title : "com_whirlview tree display");
  DV->Title( window_title  );
  draw_whirl_tree( wn, show_expr );
  DV->Menu_Create( N_DV_MENU, DV_Menu );

  Callback callback;
  DV->Event_Loop( &callback );

  CXX_DELETE(DV, &DV_wv_mempool);
  DV      = NULL;
  Func_wn = NULL;

  if ( local_trace ) {
    (void)fclose( trace_fp );
  }
}
