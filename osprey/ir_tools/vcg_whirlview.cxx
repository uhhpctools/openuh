/*
 * Copyright 2013 University of Houston.
 */

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

#include <stdio.h>
#include <string.h>
#include <vector>

#include "defs.h"
#include "errors.h"
#include "srcpos.h"
#include "opcode.h"
#include "wn.h"		/* Whirl Node descriptor */
#include "wn_simp.h"    /* Whirl simplifier (can be stubs) */
#include "pu_info.h"
#include "ir_bread.h"		    /* for WN_open_input(), etc. */
#include "cxx_memory.h"

#include "com_whirlview.h"
//#include "fb_whirl.h"

#include "vcg.h"

#define MAXEXPR 5000

static WN      *Func_wn = NULL;

// Memory allocation for VCG graphs
static MEM_POOL VCG_pool;
static BOOL VCG_pool_init = FALSE;
static int vcg_node_count = 0;

static PU_Info *Current_PU = NULL;

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

 /*
 if ( Cur_PU_Feedback ) {
   FB_FREQ freq = Cur_PU_Feedback->Query_total_out( wn );
   freq.Sprintf( dv_id + len );
 }
 */

 return dv_id;
}

#include <sstream>
#include <string>
using namespace std;

// method to dump the vcg graph of a function.

#define NEW_VCG(x)  CXX_NEW(x,&VCG_pool)

// external methods

extern char *image_st(MEM_POOL *pool, ST_IDX st_idx);
extern char *image_ty(MEM_POOL *pool, ST_IDX st_idx);
extern char *image_wn(MEM_POOL *pool, WN *wn);
extern char *image_stmt(MEM_POOL *pool, WN *wn);
extern char *image_WN_TREE_stmt(MEM_POOL *pool, WN *wn);
extern char *image_expr(MEM_POOL *pool, WN *wn);
extern char *image_WN_TREE_expr(MEM_POOL *pool, WN *wn);
extern char *image_lineno(MEM_POOL *pool, WN *wn);

extern void help_image_lineno(stringstream &ss, WN *wn);
extern void help_image_st (stringstream &ss, ST_IDX st_idx);
extern void help_image_ty(stringstream &ss, TY_IDX ty);
extern void help_image_expr(stringstream &ss, WN * wn, INT indent);
extern void help_image_wn(stringstream &ss, WN *wn, INT indent);
extern void help_image_stmt(stringstream &ss, WN * wn, INT indent);
extern void help_WN_TREE_image_stmt(stringstream &ss, WN *wn, int indent);
extern void help_WN_TREE_image_expr(stringstream &ss, WN * wn, INT indent);

VCGNode *vcg_whirl_tree(VCGGraph &vcg, WN *wn);
static VCGNode *vcg_stmt(VCGGraph &vcg, WN *wn);

static
const char *
get_unique_name(const char *prefix)
{
  stringstream ss;
  if (prefix)
    ss << prefix;
  ss << " (" << vcg_node_count << ")";
  vcg_node_count++;
  char *str  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(ss.str().data()) + 1);

  // copy string into mempool.
  strcpy(str, ss.str().data());
  return str;
}

static
const char *
get_unique_name2(const char *suffix)
{
  stringstream ss;
  ss << " " << vcg_node_count << ": ";
  if (suffix)
    ss << suffix << " ";
  vcg_node_count++;
  char *str  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(ss.str().data()) + 1);

  // copy string into mempool.
  strcpy(str, ss.str().data());
  return str;
}



static
const char *lineno(WN *wn, char *suffix = NULL)
{
  stringstream ss;
  ss << "line ";
  help_image_lineno(ss,wn);
  if (suffix)
    ss << suffix;
  char *str  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(ss.str().data()) + 1);
  // copy string into mempool.
  strcpy(str, ss.str().data());
  return str;
}


static
const char *node_title(WN *wn)
{
  char *title = image_wn(&VCG_pool,wn);

  /* replace '"' with '\"' in the returned string */
  stringstream ss;
  string string_title(title);

  size_t found = string_title.find("\"");
  while (found != string::npos) {
      size_t pos;
      size_t len;
      ss << string_title.substr(0, found-1);
      ss << "\\\"";
      pos = found+1;
      len = strlen(string_title.data()) - pos;
      string_title = string_title.substr(pos, len);
      found = string_title.find("\"");
  }
  ss << string_title;

  MEM_POOL_FREE(&VCG_pool, title);
  title = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(ss.str().data())+1);
  strcpy(title, ss.str().data());
  return title;
}

static
const char *node_name(WN *wn, const char *prefix = NULL)
{
  stringstream ss;


  WN *func_wn = (WN_operator(wn) == OPR_FUNC_ENTRY ? wn : NULL);
  if (func_wn)
  {
     return  image_st(&VCG_pool, WN_entry_name(func_wn));
  }

  if (WN_opcode(wn) == OPC_LABEL)
  {
      ss << "L";
      ss << WN_label_number(wn);
      char *str  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(ss.str().data()) + 1);

      // copy string into mempool.
      strcpy(str, ss.str().data());
      return str;
  }

  if (prefix)
    ss << prefix;

  {
      ss << "@" << (WN *)wn;
      char *str  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(ss.str().data())+1);
      strcpy(str, ss.str().data());
      return str;
  }

  switch (WN_opcode(wn)) {
    case OPC_BLOCK:
      ss << "B ";
      break;
    case OPC_IF:
      ss << "IF ";
      break;
    case OPC_DO_WHILE:
      ss << "DO WHILE";
      break;;
    case OPC_WHILE_DO:
      ss << "WHILE DO";
      break;
    case OPC_DO_LOOP:
      ss << "DO_LOOP";
      break;
    case OPC_COMPGOTO:
      ss << "COMPGOTO ";
      break;
    case OPC_XGOTO:
      ss << "XGOTO ";
      break;
    default:
      if (OPCODE_is_stmt(WN_opcode(wn)))
         ss << "S ";
      else if (OPCODE_is_expression(WN_opcode(wn)))
         ss << "E ";
      else
         ss << "WN ";
      break;
  }

  ss << id_str(wn);
  char *str  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(ss.str().data()) + 1);

  // copy string into mempool.
  strcpy(str, ss.str().data());
  return str;
}


// return info associated with a loop, such as index variable, init, step and compare expressions.

char *image_loopinfo(WN *wn)
{
  stringstream ss;

  if (WN_opcode(wn) == OPC_DO_LOOP)
  {

     ss << "INDEX:\n";
     help_image_expr(ss, WN_index(wn),1);
     ss << "INIT:\n";
     help_image_stmt(ss, WN_start(wn),1);
     ss << "COMP:";
     help_image_expr(ss, WN_end(wn), 1);
     ss << "INCR:";
     help_image_stmt(ss, WN_step(wn),1);
     if ( WN_do_loop_info(wn) != NULL ) {
       help_image_stmt(ss,WN_do_loop_info(wn), 1);
     }
  }
  else
  {
     ss << "not a loop";
  }


  char *str  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(ss.str().data()) + 1);

  // copy string into mempool.
  strcpy(str, ss.str().data());
  return str;
}

static
const char *get_last_node_title(VCGNode *node, WN *wn)
{
  if (wn == NULL)
    return node->title();
  switch(WN_opcode(wn))
  {
    case OPC_BLOCK:
      return node_name(wn, "END");
    case OPC_IF:
      return node_name(wn, "ENDIF");
    case OPC_DO_LOOP:
      return node_name(wn, "ENDLOOP");
  default:
    if (OPCODE_is_scf(WN_opcode(wn)))
    {
      return node_name(wn, "ENDSCF");
    }
    return node->title();
  }
}

const char *image_funcinfo(WN *wn)
{
  WN *func_wn = (WN_operator(wn) == OPR_FUNC_ENTRY ? wn : NULL);
  if (!func_wn) return "null";
  stringstream ss;
  ss << "FUNC NAME: ";
  help_image_st(ss, WN_entry_name(func_wn));
  ss << endl;
  INT num_formals = WN_num_formals(func_wn);
  if (num_formals > 0)
  {
     ss << "FORMALS:" << endl;
     INT i;
     for (i = 0; i < num_formals; i++) {
        ss << " ";
        help_image_wn(ss, WN_formal(func_wn, i), 0);
     }
     ss << endl;
  }
  ss << "PRAGMAS: ";
  help_image_wn(ss,WN_func_pragmas(func_wn),1);
  ss << endl;

  char *str  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(ss.str().data()) + 1);

  // copy string into mempool.
  strcpy(str, ss.str().data());
  return str;
}


extern BOOL follow_st;

void
vcg_wn(VCGGraph &vcg, WN *wn)
{
  follow_st = TRUE;
  WN *func_wn = (WN_operator(wn) == OPR_FUNC_ENTRY ? wn : NULL);
  const char *vcg_pu_name = get_unique_name2(ST_name(WN_st(func_wn)));
  VCGGraph *vcg_pu = NEW_VCG(VCGGraph(vcg_pu_name, MaxDepth));
  vcg_pu->color(LightBlue);
  vcg_pu->textColor(Black);
  vcg_pu->shape(Box);
  vcg_pu->folding(1);
  vcg.addSubGraph(*vcg_pu);

  const char *name = node_name(wn);
  VCGNode *funcnode = NEW_VCG(VCGNode(name, node_title(wn), Ellipse));
  vcg_pu->addNode(*funcnode);
  const char *funcinfo = image_funcinfo(wn);
  funcnode->info(1,funcinfo);
  VCGNode *body = vcg_whirl_tree(*vcg_pu, WN_func_body(wn));
  VCGEdge *edge = NEW_VCG(VCGEdge(funcnode->title(), body->title()));
  edge->label("BODY");
  vcg_pu->addEdge(*edge);
}

void
vcg_pus(PU_Info *pu_tree, FILE *vcgfile, const char *pu_name)
{
  PU_Info *pu;
  MEM_POOL_Initialize(&VCG_pool, "VCG_mempool", FALSE);
  VCG_pool_init = TRUE;
  VCGGraph vcg("VCG of whirl");
  vcg.infoName(1, "Whirl IR");
  vcg.fineTuning(1);

  for (pu = pu_tree; pu != NULL; pu = PU_Info_next (pu)) {
      MEM_POOL_Push(MEM_pu_nz_pool_ptr);
      MEM_POOL_Push(MEM_pu_pool_ptr);
      Read_Local_Info (MEM_pu_nz_pool_ptr, pu);

      WN *wn = PU_Info_tree_ptr(pu);

      if (WN_opcode(wn) == OPC_FUNC_ENTRY &&
          (!pu_name || (strcmp(pu_name, ST_name(WN_st(wn))) == 0))) {
          Current_PU = pu;

          /* generate graph for the PU here ... */
          vcg_wn(vcg, wn);

          Free_Local_Info (pu);
          MEM_POOL_Pop(MEM_pu_nz_pool_ptr);
          MEM_POOL_Pop(MEM_pu_pool_ptr);

          if (pu_name) {
              /* only generating VCG for this PU, so we break */
              Free_Local_Info (pu);
              MEM_POOL_Pop(MEM_pu_nz_pool_ptr);
              MEM_POOL_Pop(MEM_pu_pool_ptr);
              break;
          }
      }

      Free_Local_Info (pu);
      MEM_POOL_Pop(MEM_pu_nz_pool_ptr);
      MEM_POOL_Pop(MEM_pu_pool_ptr);
  }

  Is_True(vcgfile != NULL, ("Couldn't open vcgfile for writing"));
  vcg.emit(vcgfile);

  MEM_POOL_Delete(&VCG_pool);
}

static void
vcg_whirl_expr_tree(VCGGraph &vcg, VCGNode *node, WN *wn)
{
      INT i;
      WN *wn2;
      for (i = 0; i < WN_kid_count(wn); i++) {
          wn2 = WN_kid(wn, i);
          if (OPCODE_is_stmt(WN_opcode(wn2)) ||
              OPCODE_is_scf(WN_opcode(wn2))) {
              VCGNode *kid = vcg_stmt(vcg, wn2);
              VCGEdge *edge = NEW_VCG(VCGEdge(node->title(), kid->title()));
              char str[32];
              sprintf(str, "kid %d", i);
              char *kid_label  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(str) + 1);
              strcpy(kid_label, str);
              edge->label(kid_label);
              vcg.addEdge(*edge);
          } else {
              const char *kid_name = node_name(wn2);
              VCGNode *kid = NEW_VCG(VCGNode(kid_name, node_title(wn2)));
              kid->shape(Ellipse);
              kid->backGroundColor(LightGreen);
              kid->textColor(Black);
              VCGEdge *edge = NEW_VCG(VCGEdge(node->title(), kid->title()));
              char str[32];
              sprintf(str, "kid %d", i);
              char *kid_label  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(str) + 1);
              strcpy(kid_label, str);
              edge->label(kid_label);
              vcg.addNode(*kid);
              vcg.addEdge(*edge);
              if (WN_kid_count(wn2) > 0) {
                  vcg_whirl_expr_tree(vcg, kid, wn2);
              }
          }
      }
}

VCGNode *
vcg_whirl_tree(VCGGraph &vcg, WN *wn)
{
  if ( OPCODE_is_expression(WN_opcode(wn)) ) {
    //Is_True( FALSE, ("expr vcg not supported ") );
    const char *name = node_name(wn);
    VCGNode *node = NEW_VCG(VCGNode(name, node_title(wn)));
    vcg_whirl_expr_tree(vcg, node, wn);
    vcg.addNode(*node);
    return node;
  } else if ( OPCODE_is_stmt(WN_opcode(wn))
	      || OPCODE_is_scf(WN_opcode(wn)) ) {
    return vcg_stmt( vcg, wn);
  } else {
    Is_True( FALSE, ("opcode of unknown type") );
  }
  return NULL;
}

BOOL
is_uncond_branch(WN *wn)
{
  return (wn && (WN_opcode(wn) == OPC_GOTO));
}

static VCGNode *
vcg_if_stmt(VCGGraph *if_vcg, WN *wn)
{
    const char *if_test_title = get_unique_name("IF-TEST ");
    VCGGraph *if_test_vcg = NEW_VCG(VCGGraph(if_test_title));
    if_vcg->addSubGraph(*if_test_vcg);
    if_test_vcg->folding(1);

    const char *name = node_name(wn, "IF");
    VCGNode *ifn = NEW_VCG(VCGNode(name, node_title(wn),Rhombus));
    if_vcg->addNode(*ifn);

    const char *end = node_name(wn, "ENDIF");
    VCGNode *endifn = NEW_VCG(VCGNode(end, "end if",Rhombus));
    if_vcg->addNode(*endifn);

    if ( WN_if_test(wn) ) {
      const char *test_name = node_name(WN_if_test(wn), "TEST");
      VCGNode *iftestn = NEW_VCG(VCGNode(test_name, node_title(WN_if_test(wn))));
      if_test_vcg->addNode(*iftestn);
      VCGEdge *edge = NEW_VCG(VCGEdge(ifn->title(), iftestn->title()));
      if_vcg->addEdge(*edge);
      edge->label("test");
      vcg_whirl_expr_tree(*if_test_vcg, iftestn, WN_if_test(wn));
    }
    if ( WN_then(wn) ) {
      VCGNode *then_vcg = vcg_stmt(*if_vcg, WN_then(wn));
      const char *then_last_title = get_last_node_title(then_vcg, WN_then(wn));
      VCGEdge *edge = NEW_VCG(VCGEdge(ifn->title(),then_vcg->title()));
      if_vcg->addEdge(*edge);
      edge->label("then");
      // add an edge between then block and endif
      VCGEdge *edge2 = NEW_VCG(VCGEdge(then_last_title,endifn->title()));
      if_vcg->addEdge(*edge2);
      edge2->label("end-then");
    }
    if ( WN_else(wn) ) {
      VCGNode *else_vcg = vcg_stmt(*if_vcg, WN_else(wn));
      const char *else_last_title = get_last_node_title(else_vcg, WN_else(wn));
      VCGEdge *edge = NEW_VCG(VCGEdge(ifn->title(),else_vcg->title()));
      if_vcg->addEdge(*edge);
      edge->label("else");
      // add an edge between else block and endif
      VCGEdge *edge2 = NEW_VCG(VCGEdge(else_last_title,endifn->title()));
      if_vcg->addEdge(*edge2);
      edge2->label("end-else");
    }
    else
    {
      // add an edge between if block and endif
      VCGEdge *edge2 = NEW_VCG(VCGEdge(ifn->title(),endifn->title()));
      if_vcg->addEdge(*edge2);
      edge2->label("fall-thru");
    }

    return ifn;
}

static VCGNode *
vcg_do_loop_stmt(VCGGraph *loop_vcg, WN *wn)
{
    const char *name = node_name(wn, "LOOP");
    VCGNode *loopn = NEW_VCG(VCGNode(name, "LOOP", Ellipse));
    loop_vcg->addNode(*loopn);

    if (WN_index(wn))
    {
        const char *loop_index_title = get_unique_name("INDEX ");
        VCGGraph *loop_index_vcg = NEW_VCG(VCGGraph(loop_index_title));
        loop_vcg->addSubGraph(*loop_index_vcg);
        loop_index_vcg->folding(1);
        VCGNode *indexn = vcg_whirl_tree(*loop_index_vcg, WN_index(wn));
        VCGEdge *edge = NEW_VCG(VCGEdge(loopn->title(), indexn->title()));
        edge->label("index");
        loop_vcg->addEdge(*edge);
    }


    if (WN_start(wn))
    {
        const char *loop_init_title = get_unique_name("INIT ");
        VCGGraph *loop_init_vcg = NEW_VCG(VCGGraph(loop_init_title));
        loop_vcg->addSubGraph(*loop_init_vcg);
        loop_init_vcg->folding(1);
        VCGNode *initn = vcg_whirl_tree(*loop_init_vcg, WN_start(wn));
        VCGEdge *edge = NEW_VCG(VCGEdge(loopn->title(), initn->title()));
        edge->label("init");
        loop_vcg->addEdge(*edge);
    }

    if (WN_end(wn))
    {
        const char *loop_comp_title = get_unique_name("COMP ");
        VCGGraph *loop_comp_vcg = NEW_VCG(VCGGraph(loop_comp_title));
        loop_vcg->addSubGraph(*loop_comp_vcg);
        loop_comp_vcg->folding(1);
        VCGNode *compn = vcg_whirl_tree(*loop_comp_vcg, WN_end(wn));
        VCGEdge *edge = NEW_VCG(VCGEdge(loopn->title(), compn->title()));
        edge->label("comp");
        loop_vcg->addEdge(*edge);
    }

    if (WN_step(wn))
    {
        const char *loop_incr_title = get_unique_name("INCR ");
        VCGGraph *loop_incr_vcg = NEW_VCG(VCGGraph(loop_incr_title));
        loop_vcg->addSubGraph(*loop_incr_vcg);
        loop_incr_vcg->folding(1);
        VCGNode *incrn = vcg_whirl_tree(*loop_incr_vcg, WN_step(wn));
        VCGEdge *edge = NEW_VCG(VCGEdge(loopn->title(), incrn->title()));
        edge->label("step");
        loop_vcg->addEdge(*edge);
    }

    const char *endname = node_name(wn, "ENDLOOP");
    VCGNode *endloop = NEW_VCG(VCGNode(endname, "END LOOP", Ellipse));
    loop_vcg->addNode(*endloop);

    VCGNode *body = vcg_stmt(*loop_vcg, WN_do_body(wn));
    char *data = image_loopinfo(wn);
    VCGEdge *edge = NEW_VCG(VCGEdge(loopn->title(),body->title()));
    edge->label("loop-body");
    loop_vcg->addEdge(*edge);
    loopn->info(1, data);

    VCGEdge *endedge = NEW_VCG(VCGEdge(
                get_last_node_title(body, WN_do_body(wn)),
                endloop->title()));
    loop_vcg->addEdge(*endedge);
    endedge->label("end-loop-body");
    return loopn;
}

static
VCGNode *
vcg_stmt(VCGGraph &vcg, WN *wn)
{
  static int block_level = 0;
  vector<WN*> kids;
  stringstream ss;
  switch (WN_opcode(wn)) {
  case OPC_BLOCK:
  {
    const char *vcg_blk_name = get_unique_name("BLOCK ");
    VCGGraph *vcg_blk = NEW_VCG(VCGGraph(vcg_blk_name));
    vcg_blk->folding(1);
    if (block_level % 2) {
        vcg_blk->color(DarkGreen);
        vcg_blk->textColor(White);
    } else {
        vcg_blk->color(LightGreen);
        vcg_blk->textColor(Black);
    }
    vcg_blk->shape(Box);
    const char *name = node_name(wn, "BEGIN");
    VCGNode *blk = NEW_VCG(VCGNode(name, "BEGIN BLOCK", Ellipse));
    vcg.addSubGraph(*vcg_blk);
    vcg_blk->addNode(*blk);
    vcg_blk->folding(1);
    WN *wn2 = WN_first(wn);
    VCGNode *prev = blk;
    WN *prev_wn = NULL;
    block_level++;
    while (wn2) {
      VCGNode *node = vcg_stmt(*vcg_blk,wn2);
      if (!prev_wn || !is_uncond_branch(prev_wn))
      {
        VCGEdge *edge = NEW_VCG(VCGEdge(get_last_node_title(prev, prev_wn),
                                      node->title()));
        if (!prev_wn)
            edge->label("first");
        else
            edge->label("next");

        vcg_blk->addEdge(*edge);
      }
      prev_wn = wn2;
      wn2 = WN_next(wn2);
      prev = node;
    }
    block_level--;

    const char *endname = node_name(wn, "END");
    VCGNode *endblk = NEW_VCG(VCGNode(endname, "END BLOCK", Ellipse));
    vcg_blk->addNode(*endblk);
    VCGEdge *edge = NEW_VCG(VCGEdge(prev->title(),endblk->title()));
    edge->label("end-of-block");
    vcg_blk->addEdge(*edge);

    return blk;
    break;
  }
  case OPC_IF:
  {
    // create a new subgraph for the IF_ELSE tree and
    // keep it folded.

    const char *if_title = get_unique_name("IF-THEN-ELSE ");
    VCGGraph *if_vcg = NEW_VCG(VCGGraph(if_title));
    if_vcg->color(LightGreen);
    if_vcg->textColor(Blue);
    vcg.addSubGraph(*if_vcg);

    VCGNode *ifn = vcg_if_stmt(if_vcg, wn);

    return ifn;
    break;
  }

  case OPC_LABEL:
  {
     const char *name = node_name(wn);
     char str[32];
     sprintf(str, "%s(%p)", name, Current_PU);
     char *title  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(str) + 1);
     // copy string into mempool.
     strcpy(title, str);
     VCGNode *label = NEW_VCG(VCGNode(title, name));
     vcg.addNode(*label);
     return label;
  }
  case OPC_TRUEBR:
  case OPC_FALSEBR:
  case OPC_GOTO:
  {
     const char *name = node_name(wn, "BR");
     VCGNode *br = NEW_VCG(VCGNode(name, node_title(wn), Rhombus));
     if (WN_opcode(wn) != OPC_GOTO)
     {
       char *test_expr = image_expr(&VCG_pool, WN_kid(wn,0));
       br->info(1,test_expr);
     }
     vcg.addNode(*br);
     char str[32];
     sprintf(str, "L%d(%p)", WN_label_number(wn), Current_PU);
     char *mem  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(str) + 1);
     // copy string into mempool.
     strcpy(mem, str);

     VCGEdge *edge = NEW_VCG(VCGEdge(br->title(), mem));
     edge->setPriority(0);
     vcg.addEdge(*edge);
     if (WN_opcode(wn) == OPC_TRUEBR)
         edge->label("true");
     if (WN_opcode(wn) == OPC_FALSEBR)
         edge->label("false");
     if (WN_opcode(wn) == OPC_GOTO)
         edge->label("uncond");

     return br;
     break;
  }

  case OPC_DO_LOOP:
  {
    const char *loop_title = get_unique_name("LOOP SUBGRAPH ");
    VCGGraph *loop_vcg = NEW_VCG(VCGGraph(loop_title));
    loop_vcg->color(LightGreen);
    loop_vcg->textColor(Blue);
    vcg.addSubGraph(*loop_vcg);

    VCGNode *loopn = vcg_do_loop_stmt(loop_vcg, wn);

    return loopn;
    break;
  }
  case OPC_COMPGOTO:
  {
     const char *name = node_name(wn);
     VCGNode *compgoto = NEW_VCG(VCGNode(name, node_title(wn), Rhombus));
     vcg.addNode(*compgoto);
     char *data = image_wn(&VCG_pool, WN_kid(wn,0));
     compgoto->info(1, data);
     VCGNode *target = vcg_stmt(vcg, WN_kid(wn,1));
     VCGEdge *edge = NEW_VCG(VCGEdge(compgoto->title(), target->title()));
     edge->label("goto-target");
     if ( WN_kid_count(wn) > 2 ) {
        VCGNode *deftarget = vcg_stmt(vcg, WN_kid(wn,2));
        VCGEdge *edge = NEW_VCG(VCGEdge(compgoto->title(), deftarget->title()));
        edge->label("default-target");
        vcg.addEdge(*edge);
     }
     return compgoto;
     break;
  }
  case OPC_XGOTO:
  {
    const char *name = node_name(wn);
    VCGNode *xgoto = NEW_VCG(VCGNode(name, node_title(wn), Rhombus));
    vcg.addNode(*xgoto);
    char *data = image_wn(&VCG_pool, WN_kid(wn,0) );
    VCGNode *target = vcg_stmt(vcg, WN_kid(wn,1));
    VCGEdge *edge = NEW_VCG(VCGEdge(xgoto->title(), target->title()));
    edge->label("goto-target");
    return xgoto;
    break;
  }

  default:
  {
    if (OPCODE_is_stmt(WN_opcode(wn)))
    {
      const char *name = node_name(wn);
      const char *vcg_stmt_name = get_unique_name(node_title(wn));
      VCGGraph *vcg_stmt = NEW_VCG(VCGGraph(vcg_stmt_name,Tree));
      vcg.addSubGraph(*vcg_stmt);
      vcg_stmt->color(LightRed);
      vcg_stmt->textColor(Black);
      vcg_stmt->shape(Box);
      vcg_stmt->folding(1);

      VCGNode *node = NEW_VCG(VCGNode(name, node_title(wn)));
      vcg_stmt->addNode(*node);
      char *data = image_stmt(&VCG_pool, wn);
      node->info(1,data);

      vcg_whirl_expr_tree(*vcg_stmt, node, wn);
      return node;
    }

    if (OPCODE_is_scf(WN_opcode(wn)))
    {
      const char *scfname = node_name(wn, "SCF");
      const char *scf_title = get_unique_name(node_title(wn));
      VCGGraph *scf_vcg = NEW_VCG(VCGGraph(scf_title));
      scf_vcg->color(LightGreen);
      scf_vcg->textColor(Blue);
      vcg.addSubGraph(*scf_vcg);
      VCGNode *vcgnode = NEW_VCG(VCGNode(scfname, scf_title, Ellipse));
      scf_vcg->addNode(*vcgnode);
      const char *endscfname = node_name(wn, "ENDSCF");
      VCGNode *endnode = NEW_VCG(VCGNode(endscfname, "END SCF", Ellipse));
      scf_vcg->addNode(*endnode);

      vcgnode->backGroundColor(Green);
      endnode->backGroundColor(Green);

      stringstream ss;
      help_image_wn(ss,wn,0);
      OPCODE opc2;
      INT i;
      for (i = 0; i < WN_kid_count(wn); i++) {
        char str[32];
        sprintf(str, "kid %d", i);
        WN *wn2 = WN_kid(wn,i);
        if (wn2) {
	    opc2 = WN_opcode(wn2);
	    if (opc2 == 0) {
	      ss << "### error: WN opcode 0\n";
	    } else if (OPCODE_is_expression(opc2)) {
	      //help_image_expr(ss,wn2, 1);
          const char *scf_kid_title = get_unique_name("SCF Kid Expr ");
          VCGGraph *scf_kid_vcg = NEW_VCG(VCGGraph(scf_kid_title));
          scf_vcg->addSubGraph(*scf_kid_vcg);
          VCGNode *kidn = vcg_whirl_tree(*scf_kid_vcg, wn2);
          VCGEdge *edge = NEW_VCG(VCGEdge(vcgnode->title(), kidn->title()));
          char *kid_label = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(str)+1);
          strcpy(kid_label, str);
          edge->label(kid_label);
          scf_vcg->addEdge(*edge);
	    }
	    else if (OPCODE_is_stmt(opc2) || OPCODE_is_scf(opc2)) {

		VCGNode *stmt_vcg = vcg_stmt(*scf_vcg,wn2);
                // add an edge from wn to stmt.
                VCGEdge *edge = NEW_VCG(VCGEdge(vcgnode->title(), stmt_vcg->title()));
                char *kid_label = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(str)+1);
                strcpy(kid_label, str);
                edge->label(kid_label);
                // add an edge from last node of wn2 to the scf end node.
                VCGEdge *endedge = NEW_VCG(VCGEdge(get_last_node_title(stmt_vcg,wn2), endnode->title()));
                scf_vcg->addEdge(*edge);
                scf_vcg->addEdge(*endedge);
	    }
	}
      }

      // attach the expr string.
      char *str  = (char *) MEM_POOL_Alloc(&VCG_pool, strlen(ss.str().data()) + 1);
      // copy string into mempool.
      strcpy(str, ss.str().data());
      vcgnode->info(1,str);
      return vcgnode;
    }

  }
  }
}
