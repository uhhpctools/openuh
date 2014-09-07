/*
  Copyright UT-Battelle, LLC.  All Rights Reserved. 2014
  Oak Ridge National Laboratory
*/

/*
 * Copyright (C) 2010-2011 Advanced Micro Devices, Inc.  All Rights Reserved.
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

  UT-BATTELLE, LLC AND THE GOVERNMENT MAKE NO REPRESENTATIONS AND DISCLAIM ALL
  WARRANTIES, BOTH EXPRESSED AND IMPLIED.  THERE ARE NO EXPRESS OR IMPLIED
  WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, OR THAT
  THE USE OF THE SOFTWARE WILL NOT INFRINGE ANY PATENT, COPYRIGHT, TRADEMARK,
  OR OTHER PROPRIETARY RIGHTS, OR THAT THE SOFTWARE WILL ACCOMPLISH THE
  INTENDED RESULTS OR THAT THE SOFTWARE OR ITS USE WILL NOT RESULT IN INJURY
  OR DAMAGE.  THE USER ASSUMES RESPONSIBILITY FOR ALL LIABILITIES, PENALTIES,
  FINES, CLAIMS, CAUSES OF ACTION, AND COSTS AND EXPENSES, CAUSED BY,
  RESULTING FROM OR ARISING OUT OF, IN WHOLE OR IN PART THE USE, STORAGE OR
  DISPOSAL OF THE SOFTWARE.

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
 * ====================================================================
 *
 * Module: ipl_main.c
 *
 * Revision history:
 *  20-Sep-94 - Original Version 
 *
 * Description:
 *
 * The local phase driver
 *
 * ====================================================================
 * ====================================================================
 */

/*----------------------------------------------------------
 *          Summary Phase
 * This phase reads in the IR files and collects summary 
 * information
 *----------------------------------------------------------*/
#include <stdint.h>
#if defined(BUILD_OS_DARWIN)
#include <darwin_elf.h>
#else /* defined(BUILD_OS_DARWIN) */
#include <elf.h>
#endif /* defined(BUILD_OS_DARWIN) */
#include <sys/elf_whirl.h>	    // for WHIRL revision number
#include <sys/types.h>		    // for ir_bwrite.h
#if defined(__MINGW32__)
#include <WINDOWS.h>
#endif
#include "defs.h"

#define BACK_END		    // needed by config.h
#include "glob.h"		    // for Show_Progress
#include "flags.h"		    // for Process_Command_Line_Group ()
#include "wn.h"			    // for WHIRL related operations
#include "tracing.h"		    // for Get_Trace()
#include "ir_reader.h"		    // for IR_reader_init()
#include "const.h"		    // for CONST_TAB_SIZE
#include "pu_info.h"		    // for ir_bwrite.h
#include "ir_bwrite.h"		    // for IPA_write_summary ()
#include "cxx_memory.h"		    // for CXX_NEW

#include "ipl_summary.h"	    // for summary info data structures
#include "ipl_summarize.h"	    // for summarization info
#include "ipl_bread_write.h"	    // for IPA_irb_write_summary()
#include "ipl_array_bread_write.h"  // for Init_write_asections
#include "optimizer.h"		    // for struct DU_MANAGER ...
#include "ipl_driver.h"		    // for extern "C" declaration
#include "config.h"		    // for Run_preopt
#include "config_debug.h"
#include "config_opt.h"
#include "config_ipa.h"
#include "ipl_main.h"
#include "ipl_summarize_template.h" // put these two template files
#include "ipl_analyze_template.h"   // last in the include list
#include "ipl_cost_template.h" 	    // execution cost analysis 
#include "ipl_outline.h"	    // outline analysis
#include "wb_ipl.h" 
#include "ipa_section_main.h" 	    // utilities
#include "ipl_elfsym.h"		    // for IPL_Write_Elf_Symtab
#include "../local/init.cxx"        // force include of Ipl_Initializer

#ifdef OPENSHMEM_ANALYZER
#include "opt_alias_class.h"
/* General progress trace: */
#include <stdarg.h>
#include <fstream>
#include <queue>
using namespace std;
#ifndef opt_OSA_INCLUDED
#include "opt_OSA.h"
ofstream fout1;
#endif
FILE* iplmessagesout;
#endif

/* General progress trace: */
BOOL Trace_IPA = FALSE;
BOOL Trace_Perf = FALSE;

BOOL Debug_On = FALSE;
BOOL DoPreopt = FALSE;
BOOL Do_Const = FALSE;
BOOL Do_Par   = FALSE;
BOOL Do_Split_Commons = TRUE;
BOOL Do_Split_Commons_Set = FALSE;
BOOL Do_Common_Const = FALSE;
BOOL IPL_Enable_Outline = FALSE;
BOOL IPL_Enable_Unknown_Frequency = FALSE;
#if defined(__linux__) || defined(BUILD_OS_DARWIN)
BOOL IPL_Generate_Elf_Symtab = TRUE;
#else
BOOL IPL_Generate_Elf_Symtab = FALSE;
#endif // __linux__
#ifdef KEY
UINT32 IPL_Ignore_Small_Loops = 0;
#endif

// temporary placeholder until feedback is fixed
// BOOL FB_PU_Has_Feedback = FALSE;

mUINT8 Optlevel = 0;

static INT driver_argc = 0;
static char **driver_argv;

static OPTION_DESC Options_IPL[] = {
    { OVK_BOOL,	OV_INTERNAL,	FALSE, "debug",	"",
      0, 0, 0,	&Debug_On,	NULL},
    { OVK_BOOL,	OV_INTERNAL,	FALSE, "const",	"",
      0, 0, 0,	&Do_Const,	NULL},
    { OVK_BOOL,	OV_INTERNAL,	FALSE, "par",	"",
      0, 0, 0,	&Do_Par,	NULL},
    { OVK_BOOL, OV_INTERNAL,	FALSE, "outline", "",
      0, 0, 0,  &IPL_Enable_Outline, NULL},
    { OVK_BOOL, OV_INTERNAL,	FALSE, "unknown", "",
      0, 0, 0,  &IPL_Enable_Unknown_Frequency, NULL},
    { OVK_BOOL, OV_INTERNAL,	FALSE, "elf_symtab", "",
      0, 0, 0,	&IPL_Generate_Elf_Symtab, NULL},
#ifdef KEY
    { OVK_UINT32, OV_INTERNAL,	FALSE, "ignore_small_loops", "",
      0, 0, UINT32_MAX, &IPL_Ignore_Small_Loops, NULL},
#endif
    { OVK_COUNT }		    // List terminator -- must be last
};


OPTION_GROUP IPL_Option_Groups[] = {
    { "IPL", ':', '=', Options_IPL },
    { NULL }			    // List terminator -- must be last
};


INT    ICALL_MAX_PROMOTE_PER_CALLSITE = 2;

SUMMARY *Summary;			// class for all the summary work
#ifdef SHARED_BUILD
WN_MAP Parent_Map;
#endif
WN_MAP Summary_Map;
WN_MAP Stmt_Map;

DYN_ARRAY<char*>* Ipl_Symbol_Names = NULL; 
DYN_ARRAY<char*>* Ipl_Function_Names = NULL; 

#ifdef OPENSHMEM_ANALYZER

typedef enum {
    OSA_WARNING_MSG = 0,
    OSA_ERROR_MSG =  1
} OSA_msg_t;

char ipl_current_source[500];

void IPL_WriteLinkHTML(BOOL link, int line=0, BOOL br=TRUE)
{

   if(br)
     vfprintf (iplmessagesout,"<br>\n",NULL);

   if(link) {
     fprintf (iplmessagesout,"<a href=\"file:");
     fprintf (iplmessagesout,ipl_current_source);
     fprintf  (iplmessagesout,"#line%d",line);
     fprintf (iplmessagesout,"\">[view]</a>");
   }
}

void IPL_WriteHTML (const char * format, ...)
{
  va_list args;
  va_start (args, format);
  vfprintf (iplmessagesout, format, args);
  va_end (args);
}


// Need to keep this consistent with Print_OSA_Msg routines defined in
// ipa_cg.cxx and opt_alias_mgr.cxx
// TODO: Move these print routines into one common place.

#define OSA_MSG_BUFSIZE 512

static void Print_OSA_Msg(OSA_msg_t msg_type, const char *filename,
                   int line, int col, const char *msg, va_list argp)
{
    char tmp[OSA_MSG_BUFSIZE];
    char tmp2[OSA_MSG_BUFSIZE];
    va_list ap;

    vsnprintf(tmp, OSA_MSG_BUFSIZE, msg, argp);

    if (filename != NULL) {
        snprintf(tmp2, OSA_MSG_BUFSIZE, "\n%s:%d: %s: %s\n", filename, line,
                msg_type == OSA_ERROR_MSG ? "error" : "warning",
                tmp);
        fprintf(stderr, "%s", tmp2);
        IPL_WriteLinkHTML(TRUE, line);
        IPL_WriteHTML(tmp2);
    } else {
        snprintf(tmp2, OSA_MSG_BUFSIZE, "\n%s: %s\n",
                msg_type == OSA_ERROR_MSG ? "error" : "warning",
                tmp);
        fprintf(stderr, "%s", tmp2);
        IPL_WriteLinkHTML(FALSE);
        IPL_WriteHTML(tmp2);
    }
}

static void Print_OSA_Warning(const char *filename, int line, int col,
                              const char *msg, ...)
{
    va_list argp;
    va_start(argp, msg);
    Print_OSA_Msg(OSA_WARNING_MSG, filename, line, col, msg, argp);
    va_end(argp);
}

static void Print_OSA_Error(const char *filename, int line, int col,
                            const char *msg, ...)
{
    va_list argp;
    va_start(argp, msg);
    Print_OSA_Msg(OSA_ERROR_MSG, filename, line, col, msg, argp);
    va_end(argp);
}


//SP: Adding function similar to Parentize to annotate the WN with
//    attributes required for building a barrier tree and navigating it.

static  WN_MAP OSA_Lineno_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_MultiV_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_PEstart_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_Lstride_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_Npes_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_Parent_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_B_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_BA_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_Barrier_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_Operator_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_Label_Map = WN_MAP_UNDEFINED;
static  WN_MAP OSA_Discovered_Map = WN_MAP_UNDEFINED;

#define Set_num_B(wn, num_barriers) (WN_MAP_Set(OSA_B_Map, wn, (void*) num_barriers))
#define Get_num_B(wn) ((unsigned long) WN_MAP_Get(OSA_B_Map,(WN*) wn))
#define Set_num_BA(wn, num_barriers) (WN_MAP_Set(OSA_BA_Map, wn, (void*) num_barriers))
#define Get_num_BA(wn) ((unsigned long) WN_MAP_Get(OSA_BA_Map,(WN*) wn))
#define Set_attributes(wn, num_barriers) (WN_MAP_Set(OSA_Barrier_Map, wn, (void*) num_barriers))
#define Get_attributes(wn) ((unsigned long) WN_MAP_Get(OSA_Barrier_Map,(WN*) wn))
#define Set_Parent(wn, p) (WN_MAP_Set(OSA_Parent_Map, wn, (void*)  p))
#define Get_Parent(wn) ((WN*) WN_MAP_Get(OSA_Parent_Map, (WN*) wn))

// 0=no operator, 1=concatination, 2=alternation, 3=quantification
#define Set_Operator(wn, p) (WN_MAP_Set(OSA_Operator_Map, wn, (void*)  p))
#define Get_Operator(wn) ((unsigned long) WN_MAP_Get(OSA_Operator_Map, (WN*) wn))

// Numeric value to indicate sequence of operators and barriers
#define Set_Label(wn, p) (WN_MAP_Set(OSA_Label_Map, wn, (void*)  p))
#define Get_Label(wn) ((unsigned long) WN_MAP_Get(OSA_Label_Map, (WN*) wn))

//For recording the parameters of Barrier
#define Set_PEstart(wn, pestart) (WN_MAP_Set(OSA_PEstart_Map, wn, (void*) pestart))
#define Get_PEstart(wn) ((long) WN_MAP_Get(OSA_PEstart_Map,(WN*) wn))
#define Set_Lstride(wn, lstride) (WN_MAP_Set(OSA_Lstride_Map, wn, (void*) lstride))
#define Get_Lstride(wn) ((long) WN_MAP_Get(OSA_Lstride_Map,(WN*) wn))
#define Set_Npes(wn, npes) (WN_MAP_Set(OSA_Npes_Map, wn, (void*) npes))
#define Get_Npes(wn) ((unsigned long) WN_MAP_Get(OSA_Npes_Map,(WN*) wn))

#define Set_MultiV(wn, multiv) (WN_MAP_Set(OSA_MultiV_Map, wn, (void*) multiv))
#define Get_MultiV(wn) ((unsigned long) WN_MAP_Get(OSA_MultiV_Map,(WN*) wn))

#define Set_Lineno(wn, lineno) (WN_MAP_Set(OSA_Lineno_Map, wn, (void*) lineno))
#define Get_Lineno(wn) ((unsigned long) WN_MAP_Get(OSA_Lineno_Map,(WN*) wn))

//For matching barriers
//Initial value is -1, after every branch is discovered we increment
#define Set_Discovered(wn, edges) (WN_MAP_Set(OSA_Discovered_Map, wn, (void*) edges))
#define Get_Discovered(wn) ((long) WN_MAP_Get(OSA_Discovered_Map,(WN*) wn))

static void Annotate_btree (WN* wn)
{
    int debug = 0;
    int skip_wn, revisit_wn, visited_wn, multiv_wn;
    unsigned long init_tonegative = -1;
    unsigned long init_tozero = 0;
    static unsigned long lab=1;
    USRCPOS pos;
    INT64 line;

    if (!OPCODE_is_leaf(WN_opcode(wn))) {
        if (WN_opcode(wn) == OPC_BLOCK) {
            if (debug == 1) {
                printf("WN_block: ");
                fdump_wn(stdout, wn);
            }

            WN *kid = WN_first(wn);
            while (kid) {
                if(debug == 1){
                    printf("WN_first: ");
                    fdump_wn(stdout, kid);
                }
                Set_Parent(kid, wn);
                Set_num_B(kid, init_tozero);
                Set_num_BA(kid, init_tozero);
                Set_attributes(kid, init_tozero);
                Set_Operator(kid, init_tozero);
                Set_Label(kid, lab);
                Set_PEstart(kid,init_tonegative);
                Set_Lstride(kid,init_tonegative);
                Set_Npes(kid,init_tozero);
                Set_MultiV(kid,init_tonegative);
                Set_Discovered(kid,init_tonegative);

                USRCPOS_srcpos(pos) = WN_Get_Linenum(kid);
                line =  USRCPOS_linenum(pos);
                Set_Lineno(kid,(unsigned long)line);

                lab++;
                Annotate_btree(kid);
                kid = WN_next(kid);
            }
        } else {
            INT kidno;
            for (kidno = 0; kidno < WN_kid_count(wn); kidno++) {
                WN *kid = WN_kid(wn, kidno);
                if (kid) {
                    if(debug == 3) {
                        printf("\n Kid%d: ", kidno);
                        fdump_wn(stdout, kid);
                    }
                    Set_Parent(kid, wn);
                    Set_num_B(kid, init_tozero);
                    Set_num_BA(kid, init_tozero);
                    Set_attributes(kid, init_tozero);
                    Set_Operator(kid, init_tozero);
                    Set_Label(kid, lab);
                    Set_PEstart(kid,init_tonegative);
                    Set_Lstride(kid,init_tonegative);
                    Set_Npes(kid,init_tozero);
                    Set_MultiV(kid,init_tonegative);
                    Set_Discovered(kid,init_tonegative);

                    Set_Lineno(kid,init_tozero);
                    lab++;
                    Annotate_btree(kid);
                }
            }
        }
    }//end-if !leaf
}


static void Mark_barriers (WN* wn, struct DU_MANAGER *du_mgr)
{
  extern INT64 *cfgnode_id_ptr;
  extern INT64 *cfgedge_id_ptr;
  extern OSAedge *cfged_ptr;
  extern OSAnode *cfgentrynode_ptr, *cfgnode_ptr;
  extern OSAgraph *cfgraph_ptr;

  int debug = 0, is_barrier=-1;
  static unsigned long bar_no=1, op_num=1;
  WN *stmt, *tmp_wn, *parent_wn;
  char *varnm;
  unsigned long num_barriers, op, lab, num_B=0, num_BA=0;
  unsigned long concat = 1, alter =2, quant = 3, alterC=4, quantC=5;

  //SP: For recording barrier parameters
  unsigned long pestart= -1, lstride= 99, numpes=0;
  int num_defs=0;
  const DU_NODE *tmp;

  //SP: Collecting barriers in queues (Logic from Match barriers)
  std::queue<int> q0, q1, q2, q3, q4;
  static int noCond = 0;

  if (!OPCODE_is_leaf(WN_opcode(wn))) {
      if (WN_opcode(wn) == OPC_BLOCK) {
          unsigned long isMV=-1;
          stmt = WN_first(wn);
          while (stmt) {
              if (WN_has_sym(stmt)){
                  if(WN_st(stmt)){
                      varnm = ST_name(WN_st(stmt));
                      if(strcmp(varnm,"shmem_barrier") == 0){
                          is_barrier = 1;
                          //SP: Now to analyze the arguments for start, stride and npes
                          // case1: Local Constants (values and variables)
                          if(WN_operator(WN_kid(WN_kid(stmt,0),0))== OPR_INTCONST) {
                              Set_PEstart(stmt,(long)WN_get_const_val(WN_kid(WN_kid(stmt,0),0)));
                              pestart = (unsigned long)WN_get_const_val(WN_kid(WN_kid(stmt,0),0));
                          }
                          if(debug == 3)
                              fprintf(stdout,"Value of start PE:%lu\n",pestart);
                          //case1: Local Constant
                          if(WN_operator(WN_kid(WN_kid(stmt,1),0))== OPR_INTCONST) {
                              Set_Lstride(stmt,(long)WN_get_const_val(WN_kid(WN_kid(stmt,1),0)));
                              lstride = WN_get_const_val(WN_kid(WN_kid(stmt,1),0));
                          }
                          if(debug == 3)
                              fprintf(stdout,"Value of log_stride:%d\n",lstride);
                          //case1: Local Constant
                          if(WN_operator(WN_kid(WN_kid(stmt,2),0))== OPR_INTCONST) {
                              Set_Npes(stmt,(unsigned long)WN_get_const_val(WN_kid(WN_kid(stmt,2),0)));
                              numpes = WN_get_const_val(WN_kid(WN_kid(stmt,2),0));
                          }
                          if(debug == 3)
                              fprintf(stdout,"Value of npes:%d\n",numpes);
                      }
                      if(strcmp(varnm,"shmem_barrier_all") == 0)
                          is_barrier = 2;
                      if(is_barrier > 0) {
                          Set_Label(stmt,bar_no);
                          bar_no++;
                          if(debug == 1){
                              printf("\nBarrier found(1):");
                              fdump_wn(stdout, stmt);
                          }
                          tmp_wn = stmt;
                          int parent_level = 1;
                          parent_wn = Get_Parent(tmp_wn);
                          while (1) {
                              if (parent_wn == NULL)  //FUN-Entry
                                  break;
                              else{
                                  if (debug == 1) {
                                      printf("Parent%d: ",parent_level);
                                      fdump_wn(stdout, parent_wn);
                                  }
                                  if (is_barrier == 1) {
                                      num_B = Get_num_B(parent_wn);
                                      num_B++;
                                      Set_num_B(parent_wn,num_B);
                                  }
                                  else{
                                      num_BA = Get_num_BA(parent_wn);
                                      num_BA++;
                                      Set_num_BA(parent_wn,num_BA);
                                  }
                                  num_barriers = Get_attributes(parent_wn);
                                  op = Get_Operator(parent_wn);
                                  if(debug == 1)
                                      printf("OLD: bars=%d,operator=%d \n",
                                              num_barriers, op);
                                  num_barriers++;
                                  Set_attributes(parent_wn, num_barriers);

                                  if (WN_opcode(parent_wn) == OPC_FUNC_ENTRY) {
                                      Set_Operator(parent_wn,concat);
                                  }
                                  else if(WN_opcode(parent_wn) == OPC_IF){
                                      //for every node
                                      for(int i=0; i< cfgraph_ptr->nodes.size();i++) {
                                          if (cfgraph_ptr->nodes[i].is_mVal == 1 &&
                                              strcmp(cfgraph_ptr->nodes[i].opcode,
                                                     "OPC_IF") == 0) {
                                              if(cfgraph_ptr->nodes[i].wn_self ==
                                                 parent_wn) {
                                                  isMV=1;
                                                  Set_MultiV(parent_wn,isMV);
                                                  //Set_Lineno(parent_wn,(unsigned long)cfgraph_ptr->nodes[i].line);
                                                  break;
                                              }
                                          }
                                      }//end-for
                                      if (isMV == 1)
                                          Set_Operator(parent_wn,alterC);
                                      else
                                          Set_Operator(parent_wn,alter);
                                      isMV = -1;
                                  }//end-if(WN_opcode(parent_wn) == OPC_IF)
                                  else if (WN_opcode(parent_wn) == OPC_BLOCK){
                                      if (WN_opcode(Get_Parent(parent_wn)) == OPC_IF ||
                                          WN_opcode(Get_Parent(parent_wn)) == OPC_WHILE_DO ||
                                          WN_opcode(Get_Parent(parent_wn)) == OPC_DO_WHILE) {
                                          if(WN_kid(Get_Parent(parent_wn), 2) != NULL) {
                                              Set_Operator(parent_wn,concat);
                                          } else {
                                              //printf("No else block.\n");
                                              Set_Parent(WN_kid(wn,1),Get_Parent(parent_wn));
                                          }
                                      } else if (WN_opcode(Get_Parent(parent_wn)) == OPC_DO_LOOP) {
                                          if(WN_kid(Get_Parent(parent_wn), 4) != NULL) {
                                              Set_Operator(parent_wn,concat);
                                          } else{
                                              //printf("No else block.\n");
                                              Set_Parent(WN_kid(wn,1),Get_Parent(parent_wn));
                                          }
                                      } else {
                                          Set_Operator(parent_wn,concat);
                                      }
                                  }
                                  else if(WN_opcode(parent_wn) == OPC_DO_LOOP ||
                                          WN_opcode(parent_wn) == OPC_WHILE_DO ||
                                          WN_opcode(parent_wn) == OPC_DO_WHILE) {
                                      //for every node
                                      for(int i=0; i< cfgraph_ptr->nodes.size();i++) {
                                          if (cfgraph_ptr->nodes[i].is_mVal == 1 &&
                                        (strcmp(cfgraph_ptr->nodes[i].opcode, "OPC_DO_LOOP") == 0 ||
                                        strcmp(cfgraph_ptr->nodes[i].opcode, "OPC_DO_WHILE") == 0 ||
                                        strcmp(cfgraph_ptr->nodes[i].opcode, "OPC_WHILE_DO") ==0) ) {
                                              if(cfgraph_ptr->nodes[i].wn_self == parent_wn) {
                                                  isMV=2;
                                                  Set_MultiV(parent_wn,isMV);
                                                  //Set_Lineno(parent_wn,(unsigned long)cfgraph_ptr->nodes[i].line);
                                                  break;
                                              }
                                          }
                                      }//end-for
                                      if(isMV == 2)
                                          Set_Operator(parent_wn,quantC);
                                      else
                                          Set_Operator(parent_wn,quant);
                                      isMV = -1;
                                  }//end-if(WN_opcode(parent_wn) == OPC_DO*)

                                  num_barriers = Get_attributes(parent_wn);
                                  op = Get_Operator(parent_wn);
                                  if(debug == 1) {
                                      printf("NEW: bars=%d,operator=%d label=%d\n",
                                              num_barriers, op, lab);
                                  }
                              }//end-else
                              tmp_wn = parent_wn;
                              parent_wn = Get_Parent(tmp_wn);
                              parent_level++;
                          }//end-while(1)
                      }//end-if barrier is found
                      is_barrier = -1;
                  }
              }
              Mark_barriers(stmt,du_mgr);
              stmt = WN_next(stmt);
          }
      } else {
          INT kidno;
          int isMV=-1;
          for (kidno = 0; kidno < WN_kid_count(wn); kidno++) {
              stmt = WN_kid(wn, kidno);
              if(stmt){
                  if (WN_has_sym(stmt)){
                      if(WN_st(stmt)){
                          varnm = ST_name(WN_st(stmt));
                          if(strcmp(varnm,"shmem_barrier") == 0) {
                              is_barrier = 1;
                              printf("In else!\n");
                              if(WN_operator(WN_kid(WN_kid(stmt,0),0))== OPR_CONST) {
                                  fprintf(stdout,"Value of start PE is a constant\n");
                              }
                              if(WN_operator(WN_kid(WN_kid(stmt,1),0))== OPR_CONST) {
                                  fprintf(stdout,"Value of stride is a constant\n");
                              }
                              if(WN_operator(WN_kid(WN_kid(stmt,2),0))== OPR_CONST) {
                                  fprintf(stdout,"Value of numpes is a constant\n");
                              }
                          }
                          if(strcmp(varnm,"shmem_barrier_all") == 0)
                              is_barrier = 2;
                          if(is_barrier > 0){
                              if(debug == 1){
                                  printf("\nBarrier found(1):");
                                  fdump_wn(stdout, stmt);
                              }
                              Set_Label(stmt,bar_no);
                              bar_no++;
                              tmp_wn = stmt;
                              int parent_level = 1;
                              parent_wn = Get_Parent(tmp_wn);
                              while(1){
                                  if(parent_wn == NULL){  //FUN-Entry
                                      break;
                                  }
                                  else{
                                      if(debug == 1){
                                          printf("Parent%d: ",parent_level);
                                          fdump_wn(stdout, parent_wn);
                                      }
                                      if(is_barrier == 1){
                                          num_B = Get_num_B(parent_wn);
                                          num_B++;
                                          Set_num_B(parent_wn, num_B);
                                      }
                                      else{
                                          num_BA = Get_num_BA(parent_wn);
                                          num_BA++;
                                          Set_num_BA(parent_wn,num_BA);
                                      }
                                      num_barriers = Get_attributes(parent_wn);
                                      op = Get_Operator(parent_wn);
                                      if(debug == 1) {
                                          printf("OLD: bars=%d,operator=%d label=%d\n",
                                                  num_barriers, op, lab);
                                      }
                                      num_barriers++;
                                      Set_attributes(parent_wn, num_barriers);
                                      if(WN_opcode(parent_wn) == OPC_FUNC_ENTRY)
                                          Set_Operator(parent_wn,concat);
                                      else if(WN_opcode(parent_wn) == OPC_IF){
                                          //printf("Number of kids in IF:%d\n",WN_kid_count(tmp_wn));
                                          for(int i=0; i< cfgraph_ptr->nodes.size();
                                              i++) {//for every node
                                              if(cfgraph_ptr->nodes[i].is_mVal == 1 &&
                                                 strcmp(cfgraph_ptr->nodes[i].opcode, "OPC_IF" == 0)) {
                                                  if(cfgraph_ptr->nodes[i].wn_self == parent_wn) {
                                                      isMV=1;
                                                      Set_MultiV(parent_wn,isMV);
                                                      //Set_Lineno(parent_wn,(unsigned long)cfgraph_ptr->nodes[i].line);
                                                      break;
                                                  }
                                              }
                                          }
                                          if(isMV == 1)
                                              Set_Operator(parent_wn,alterC);
                                          else
                                              Set_Operator(parent_wn,alter);
                                      }//end-if(WN_opcode(parent_wn) == OPC_IF)
                                      else if (WN_opcode(parent_wn) == OPC_BLOCK){
                                          if(WN_opcode(Get_Parent(parent_wn)) == OPC_IF ||
                                             WN_opcode(Get_Parent(parent_wn)) == OPC_WHILE_DO ||
                                             WN_opcode(Get_Parent(parent_wn)) == OPC_DO_WHILE) {
                                              if(WN_kid(Get_Parent(parent_wn), 2) != NULL)
                                                  Set_Operator(parent_wn,concat);
                                              else{
                                                  //printf("No else block.\n");
                                                  Set_Parent(WN_kid(wn,1),Get_Parent(parent_wn));
                                              }
                                          }
                                          else if (WN_opcode(Get_Parent(parent_wn)) == OPC_DO_LOOP) {
                                              if (WN_kid(Get_Parent(parent_wn), 4) != NULL)
                                                  Set_Operator(parent_wn,concat);
                                              else {
                                                  //printf("No else block.\n");
                                                  Set_Parent(WN_kid(wn,1),Get_Parent(parent_wn));
                                              }
                                          }
                                          else
                                              Set_Operator(parent_wn,concat);
                                      }
                                      else if (WN_opcode(parent_wn) == OPC_DO_LOOP ||
                                               WN_opcode(parent_wn) == OPC_WHILE_DO ||
                                               WN_opcode(parent_wn) == OPC_DO_WHILE) {
                                          // for every node
                                          for(int i=0; i< cfgraph_ptr->nodes.size();i++) {
                                              if(cfgraph_ptr->nodes[i].is_mVal == 1 &&
                                           (strcmp(cfgraph_ptr->nodes[i].opcode, "OPC_DO_LOOP") == 0 ||
                                            strcmp(cfgraph_ptr->nodes[i].opcode, "OPC_DO_WHILE") == 0 ||
                                            strcmp(cfgraph_ptr->nodes[i].opcode, "OPC_WHILE_DO") ==0)) {
                                                  if(cfgraph_ptr->nodes[i].wn_self == parent_wn) {
                                                      isMV=2;
                                                      Set_MultiV(parent_wn,isMV);
                                                      //Set_Lineno(parent_wn,(unsigned long)cfgraph_ptr->nodes[i].line);
                                                      break;
                                                  }
                                              }
                                          }//end-for
                                          if(isMV == 2)
                                              Set_Operator(parent_wn,quantC);
                                          else
                                              Set_Operator(parent_wn,quant);
                                          isMV = -1;
                                      }//end-if(WN_opcode(parent_wn) == OPC_DO*)

                                      num_barriers = Get_attributes(parent_wn);
                                      op = Get_Operator(parent_wn);
                                      if(debug == 1)
                                          printf("NEW: bars=%d,operator=%d \n", num_barriers,op);
                                  }//end-else
                                  tmp_wn = parent_wn;
                                  parent_wn = Get_Parent(tmp_wn);
                                  parent_level++;
                              }//end-while(1)
                          }
                          is_barrier = -1;
                      }//end if(WN_st(stmt))
                  }
                  Mark_barriers(stmt,du_mgr);
              }
          }//end-for
      }//end-else
  }//end-if Opcode != leaf


}

//SP: Implementing the soution with arrays for saving all barrier sequences.
//We assume that all barriers are matched.
static int Match_barriers (WN* wn, struct DU_MANAGER *du_mgr)
{
    int debug = 0;
    USRCPOS pos;
    INT64 line;


    if (!OPCODE_is_leaf(WN_opcode(wn))) {
        if (WN_opcode(wn) == OPC_BLOCK) {
            if(debug == 1){
                printf("WN_block: ");
                fdump_wn(stdout, wn);
            }
            WN *kid = WN_first(wn);
            while (kid) {
                if(debug == 1){
                    printf("WN_first: ");
                    fdump_wn(stdout, kid);
                }
                //Set_Parent(kid, wn);

                USRCPOS_srcpos(pos) = WN_Get_Linenum(kid);
                line =  USRCPOS_linenum(pos);
                //Set_Lineno(kid,(unsigned long)line);

                Match_barriers(kid, du_mgr);
                kid = WN_next(kid);
            }
        } else {
            INT kidno;
            for (kidno = 0; kidno < WN_kid_count(wn); kidno++) {
                WN *kid = WN_kid(wn, kidno);
                if (kid) {
                    if(debug == 1){
                        printf("\n\t Kid%d: ", kidno);
                        fdump_wn(stdout, kid);
                    }
                    //Set_Parent(kid, wn);

                    //Set_Lineno(kid,init_tozero);
                    Match_barriers(kid, du_mgr);
                }
            }
        }
    }//end-if !leaf

    /*
    {
        printf("Inside match barriers\n");
        int debug = 1;

        int all_explored = 1, is_barrier=0;
        // Array of Arrays,300 barrier sequences of upto 100 barriers in each.
        int all_barrier_comb[300][100];
        // Every time I reach the leaf, increment bseq, everytime I add a
        // barrier increment bno.
        static int bseq=0, bno=0;
        char *varnm;
        WN* w, wn_lastB, wninelse;
        w = wn;
        static int chkpt=0;
        //printf("besq:%d,bno:%d\n",bseq,bno);
        // while(all_explored = 0){
        if (!OPCODE_is_leaf(WN_opcode(wn))) {
            if (WN_opcode(wn) == OPC_BLOCK ) {
                WN *kid;
                kid = WN_first(wn);
                while (kid) {
                    WN *pwn, *gwn;
                    printf("Checkpt %d\n",chkpt);
                    chkpt++;
                    printf("IF, FIRST KID:\t");
                    fdump_wn(stdout, kid);

                    //if (WN_has_sym(kid)){
                    if(WN_st(kid)){
                        varnm = ST_name(WN_st(kid));
                        if(strcmp(varnm,"shmem_barrier") == 0){
                            is_barrier = 1;
                            printf("IF: Found barrier\n");
                        }
                        else{
                            if(strcmp(varnm,"shmem_barrier_all") == 0){
                                is_barrier = 2;
                                printf("IF: Found barrier all\n");
                            }
                        }//end-else
                        //if(is_barrier > 0){
                        if(Get_Parent(kid)){
                            pwn = Get_Parent(kid);
                            if(Get_Parent(pwn)){
                                gwn = Get_Parent(pwn);
                                printf("Grandparent:\t");
                                fdump_wn(stdout, gwn);
                                if(WN_opcode(gwn) == OPC_FUNC_ENTRY && is_barrier > 0){
                                    //SP: Eg. 2014 indicates that there it is a shmem_barrier_all
                                    //(2014/1000=2) with label 14 (2014%1000)
                                    all_barrier_comb[bseq][bno] = is_barrier * 1000 + Get_Label(wn);
                                    if(debug == 1)
                                        printf("all_barrier_comb[%d][%d]=%d\n",bseq,bno,all_barrier_comb[bseq][bno]);
                                    wn_lastB = wn;
                                    bno++;

                                }//end-if gwn =ENTRY
                                else {
                                    long isDis=-2;
                                    int kidNo=-1;
                                    if(WN_opcode(gwn) == OPC_IF){
                                        int kidno=-1;
                                        if(pwn == WN_kid1(gwn))
                                            kidno = 1;
                                        else if (pwn == WN_kid2(gwn))
                                            kidno = 2;
                                        isDis = Get_Discovered(gwn);
                                        //Both THEN and ELSE are done
                                        if(isDis == 1 || (isDis == 0 && kidno == 1)){
                                            //Don't add the barrier
                                        }
                                        else{
                                            //Not explored THEN yet or not explored ELSE
                                            if((isDis == -1 && kidno == 1) ||
                                                (isDis == 0 && kidno == 2)){
                                                //SP: Eg. 2014 indicates that there it is a shmem_barrier_all
                                                //(2014/1000=2) with label 14 (2014%1000)
                                                all_barrier_comb[bseq][bno] = is_barrier * 1000 + Get_Label(wn);
                                                if(debug == 1)
                                                    printf("all_barrier_comb[%d][%d]=%d\n",bseq,bno,all_barrier_comb[bseq][bno]);
                                                wn_lastB = wn;
                                                isDis++;
                                                Set_Discovered(gwn,isDis);
                                            }
                                            else {
                                                // should never reach here.
                                                printf("Un-handled combination reached: isDis = %d, "
                                                       " kidno = %d\n",isDis,kidno);
                                                //WN *kid = WN_kid(wn, kidno);
                                                //Match_barriers(kid, du_mgr);
                                            }
                                        }//end-else
                                    }//end-if pcode==IF
                                }
                            }
                            else
                                printf("No Grandparent\n");
                        }
                        //}//end-if(is_barrier >0)
                    }
                    //}//end-if has sym
                    Match_barriers(kid, du_mgr);
                    kid = WN_next(kid);
                }

            }
            else {
                INT kidno;
                printf("ELSE, WN:\t");
                fdump_wn(stdout, wn);
                for (kidno; kidno < WN_kid_count(wn); kidno++) {
                    WN *kid = WN_kid(wn, kidno);
                    if(kid){
                        WN *pwn, *gwn;
                        pwn = wn;
                        printf("ELSE, KID:\t");
                        fdump_wn(stdout, kid);

                        //if (WN_has_sym(kid)){
                        if(WN_st(kid)){
                            varnm = ST_name(WN_st(kid));
                            if(strcmp(varnm,"shmem_barrier") == 0){
                                is_barrier = 1;
                                printf("Found barrier\n");
                            }
                            else{
                                if(strcmp(varnm,"shmem_barrier_all") == 0){
                                    is_barrier = 2;
                                    printf("Found barrier all\n");
                                }
                            }//end-else
                            if(is_barrier > 0){
                                if(Get_Parent(wn)){
                                    gwn = Get_Parent(wn);
                                    printf("Grandparent:\t");
                                    fdump_wn(stdout, gwn);
                                    if(WN_opcode(gwn) == OPC_FUNC_ENTRY){
                                        //SP: Eg. 2014 indicates that there it is a shmem_barrier_all
                                        //(2014/1000=2) with label 14 (2014%1000)
                                        all_barrier_comb[bseq][bno] =
                                            is_barrier * 1000 + Get_Label(wn);
                                        if(debug == 1)
                                            printf("all_barrier_comb[%d][%d]=%d\n",bseq,bno,
                                                    all_barrier_comb[bseq][bno]);
                                        wn_lastB = wn;
                                        bno++;

                                    }//end-if gwn =ENTRY
                                    else {
                                        long isDis=-2;
                                        int kidNo=-1;
                                        if(WN_opcode(gwn) == OPC_IF){

                                            isDis = Get_Discovered(gwn);
                                            //Both THEN and ELSE are done
                                            if(isDis == 1 || (isDis == 0 && kidno == 1)){
                                                //Don't add the barrier
                                            }
                                            else{
                                                // Not explored THEN yet or not explored ELSE
                                                if((isDis == -1 && kidno == 1) ||
                                                    (isDis == 0 && kidno == 2)) {
                                                    //SP: Eg. 2014 indicates that there it is a
                                                    //shmem_barrier_all (2014/1000=2) with label 14 (2014%1000)
                                                    all_barrier_comb[bseq][bno] = is_barrier * 1000 + Get_Label(wn);
                                                    if(debug == 1)
                                                        printf("all_barrier_comb[%d][%d]=%d\n",bseq,bno,
                                                                all_barrier_comb[bseq][bno]);
                                                    wn_lastB = wn;
                                                    isDis++;
                                                    Set_Discovered(gwn,isDis);
                                                }
                                                else{
                                                    //should never reach here.
                                                    printf("Un-handled combination reached: isDis = %d, "
                                                           " kidno = %d\n",isDis,kidno);
                                                    //WN *kid = WN_kid(wn, kidno);
                                                    //Match_barriers(kid, du_mgr);
                                                }
                                            }//end-else
                                        }//end-if pcode==IF
                                    }
                                }
                                else
                                    printf("No Grandparent\n");
                            }
                        }
                        // }
                        Match_barriers(kid, du_mgr);

                    }//end-if(kid)
                }
            }
        }//end-opcode leaf
        else{
            //bseq++;
            //wn = w;
            //TODO:Set end condition for while=>iterate through the tree and
            //check that all Discovered values ==1

        }
        //  }//end-while
    }
    */
}

//SP: To match the barriers recorded in the barrier tree: Has sound logic but does not account for nested barriers
static int Check_barriers(WN* wn, struct DU_MANAGER *du_mgr)
{
    int debug = 0;
    WN *kid, *wn_BA[4][10];
    int bseq[4][10];
    int i,j;
    char *varnm;
    int unmatch=-1;

    if (!OPCODE_is_leaf(WN_opcode(wn))) {
        if (WN_opcode(wn) == OPC_BLOCK ) {
            kid = WN_first(wn);
            while (kid) {
                if(Get_attributes(kid) > 0 && Get_MultiV(kid) > 0){
                    i=j=0; //each IF starts with

                    //SP: if number of B+BA associated with IF WN is > 00 barriers.
                    if(WN_opcode(kid) == OPC_IF){
                        int nestL=0;
                        //SP:Find if there are/is nested conditionals

                        if(debug == 1)
                            printf("Get_attributes(kid):%lu, Get_MultiV(kid):%lu\n",
                                    Get_attributes(kid), Get_MultiV(kid));

                        if(Get_num_BA(WN_kid(kid,1)) != Get_num_BA(WN_kid(kid,2))){
			  //DK: If conditional has wrong line number
			  //printf("IF conditional on line %lu has unmatched "
			  //	 "barrier_all call.\n" /*Get_Lineno(kid)*/, Get_Lineno(WN_kid(WN_kid(kid,1),0))-1);
			  Print_OSA_Warning(Src_File_Name,  Get_Lineno(WN_kid(WN_kid(kid,1),0))-1, 0,"IF conditional has unmatched barrier_all call");                               
			  unmatch = 1;
                        }

                        //SP: This should be inside nested conditional analysis for better accuracy
                        if(Get_num_B(WN_kid(kid,1)) != Get_num_B(WN_kid(kid,2))){
			  //printf("IF conditional on line %lu MAY have unmatched"
			  //       " barrier call.\n",/*Get_Lineno(kid)*/ Get_Lineno(WN_kid(WN_kid(kid,1),0))-1);
			    Print_OSA_Warning(Src_File_Name,  Get_Lineno(WN_kid(WN_kid(kid,1),0))-1, 0,"IF conditional MAY have unmatched barrier call");                       
                            unmatch = 1;
                        }
                        //SP: Find if the THEN or ELSE blocks have any nested IFs with barriers
                        WN *kinthen, *kinelse;

                        //initialized to the first kid statement in the then block
                        kinthen = WN_kid(WN_kid(kid,1),0);

                        //initialized to the first kid statement in the else block
                        kinelse = WN_kid(WN_kid(kid,2),0);

                        int numB=0;
                        //must make sure that Bs exist
                        if(Get_num_B(WN_kid(kid,1)) == Get_num_B(WN_kid(kid,2))
                           && Get_num_B(WN_kid(kid,1)) != 0){
                            if(Get_num_BA(WN_kid(kid,1)) == Get_num_BA(WN_kid(kid,2))) {
                                // there may exist multiple Barriers(both B & BA) per block,
                                // ordering matters!
                                numB = Get_attributes(WN_kid(kid,1));
                                WN *Binthen, *Binelse;
                                // initialized to the first kid statement in the then block
                                Binthen = WN_kid(WN_kid(kid,1),0);
                                // initialized to the first kid statement in the else block
                                Binelse = WN_kid(WN_kid(kid,2),0);

                                for(int i=0;i<numB;i++){
                                    while(Binthen){
                                        if(strcmp(ST_name(WN_st(Binthen)),"shmem_barrier") == 0 ||
                                           strcmp(ST_name(WN_st(Binthen)),"shmem_barrier_all") == 0){
                                            break;
                                        }
				    }
                                    while(Binelse){
                                        if(strcmp(ST_name(WN_st(Binelse)),"shmem_barrier") == 0 ||
                                           strcmp(ST_name(WN_st(Binelse)),"shmem_barrier_all") == 0){
                                            break;
                                        }
                                        else{
                                            Binelse = WN_next(Binelse);
                                        }
                                    }
                                    if(strcmp(ST_name(WN_st(Binthen)),ST_name(WN_st(Binelse))) == 0
                                       && strcmp(ST_name(WN_st(Binelse)),"shmem_barrier") == 0){

                                        if((Get_PEstart(Binthen) != -1 &&
                                            Get_Lstride(Binthen) != -1 &&
                                            Get_Npes(Binthen) != 0) ||
                                            (Get_PEstart(Binelse) != -1 &&
                                             Get_Lstride(Binelse) != -1 &&
                                             Get_Npes(Binelse) != 0)) {
                                            if(Get_PEstart(Binthen) == Get_PEstart(Binelse) &&
                                               Get_Lstride(Binthen) == Get_Lstride(Binelse) &&
                                               Get_Npes(Binthen) == Get_Npes(Binelse)) {
                                                if(debug == 1) {
                                                    // barrier parameters are constants and they match
                                                    printf("Barrier statements in line %lu and "
                                                           " %lu match!\n",Get_Lineno(Binthen),
                                                           Get_Lineno(Binelse));
                                                }
                                            }
                                            else {
                                                // barrier parameters are constants and they match
                                                printf("Barrier statements in line %lu and %lu "
                                                       " have different active sets.\n",
                                                       Get_Lineno(Binthen),
                                                       Get_Lineno(Binelse));
                                                unmatch = 1;
                                            }
                                        }
                                        else { //barrier parameteres are not constants
                                            printf("Barrier statements in line %lu and %lu "
                                                   "cannot be matched through static analysis.\n",
                                                   Get_Lineno(Binthen), Get_Lineno(Binelse));
                                            unmatch = 1;
                                        }

                                        Binthen = WN_next(Binthen);
                                        Binelse = WN_next(Binelse);
                                    }
                                    else {
                                        if(strcmp(ST_name(WN_st(Binthen)),
                                                  ST_name(WN_st(Binelse))) == 0 &&
                                           strcmp(ST_name(WN_st(Binelse)),
                                                  "shmem_barrier_all") == 0) {
                                            if(debug == 1) {
                                                // barrier parameters are constants and they match
                                                printf("Barrier all statements in line %lu and "
                                                       " %lu match!\n",Get_Lineno(Binthen),
                                                       Get_Lineno(Binelse));
                                            }
                                        }
                                        else {
                                            // barrier parameters are constants and they match
                                            printf("The application may have incorrect ordering "
                                                   " of barrier statements (check lines %lu and %lu).\n",
                                                   Get_Lineno(Binthen),
                                                   Get_Lineno(Binelse));
                                        }
                                        Binthen = WN_next(Binthen);
                                        Binelse = WN_next(Binelse);
                                    }

                                }//end-for
                            }//end-if BA in then == BA in else
                        }//end-if B in then == B in else
                    }//end-if(WN_opcode(parent_wn) == OPC_IF)

                    //SP: if number of B+BA associated with DO WN is > 0
                    if(WN_opcode(kid) == OPC_DO_LOOP ||
                       WN_opcode(kid) == OPC_WHILE_DO ||
                       WN_opcode(kid) == OPC_DO_WHILE){
		      //printf("There  MAY be unmatched barrier calls due to DO "
		      //       " loop on line %lu.\n", Get_Lineno(kid));
			Print_OSA_Warning(Src_File_Name,  Get_Lineno(kid), 0,"There  MAY be unmatched barrier calls due to DO loop");  
                    }//end-if

                }//end-if barriers exist
                Check_barriers(kid, du_mgr);
                kid = WN_next(kid);
            }
        } else {
            INT kidno;
            for (kidno = 0; kidno < WN_kid_count(wn); kidno++) {
                WN *kid = WN_kid(wn, kidno);
                Check_barriers(kid, du_mgr);
            }
        }
    }//end-if !leaf
}

//SP: Print barrier tree to dot file
void Print_IR_maps(WN* w)
{
    char *varnm;
    unsigned long nbar, op, lab;
    static unsigned long nlab=1;
    int debug = -1;

    if (!OPCODE_is_leaf(WN_opcode(w))) {
        if (WN_opcode(w) == OPC_BLOCK) {
            WN *kid = WN_first(w);
            while (kid) {
                nbar = Get_attributes(kid);
                if(nbar>0){
                    op = Get_Operator(kid);
                    Set_Label(kid, nlab);
                    nlab++;
                    lab = Get_Label(kid);
                    if(debug ==0){
                        if(op == 1) {
                            printf("Barriers:%lu, op:CONCAT, Label:%lu\n",
                                    nbar,lab);
                        } else if(op == 2) {
                            printf("Barriers:%lu, op:ALT, Label:%lu\n",
                                    nbar,lab);
                        } else if (op == 3) {
                            printf("Barriers:%lu, op:QUANT, Label:%lu\n",
                                    nbar,lab);
                        } else if (op == 4) {
                            printf("Barriers:%lu, op:ALT-CON, Label:%lu\n",
                                    nbar,lab);
                        } else if (op == 5) {
                            printf("Barriers:%lu, op:QUANT-CON, Label:%lu\n",
                                    nbar,lab);
                        } else {
                            printf("Something ain't right!!\n");
                        }
                    }
                } else {
                    int which_bar=-1;
                    if (WN_has_sym(kid)) {
                        if(WN_st(kid)) {
                            varnm = ST_name(WN_st(kid));
                            lab = Get_Label(kid);
                            if(strcmp(varnm,"shmem_barrier") == 0) {
                                which_bar = 1;
                                if(debug ==1) fprintf(stdout,"B%d\n",lab);
                            }
                            if(strcmp(varnm,"shmem_barrier_all") == 0) {
                                which_bar = 2;
                                if(debug ==1) fprintf(stdout,"BA%d\n",lab);
                            }
                            if(which_bar > 0) {
                                static int p_cnt=1;
                                WN *tmp = kid, *pwn;

                                pwn = Get_Parent(tmp);

                                while(1) {
                                    unsigned long op = Get_Operator(tmp);
                                    char opr[20], opr1[20];
                                    unsigned long op1;
                                    if(pwn != NULL) {
                                        if(debug ==1) {
                                            printf("\nParent: ");
                                            fdump_wn(stdout, pwn);
                                        }
                                        op1 = Get_Operator(pwn);
                                        if(op1 == 1) {
                                            strcpy(opr1,"C");
                                            fout1<<"\tC"<<Get_Label(pwn)<<
                                                "[shape=circle color=blue "
                                                "style=clear height=.25 width=.25];"
                                                <<endl;
                                        } else if(op1 == 2) {
                                            strcpy(opr1,"A");
                                            fout1<<"\tA"<<Get_Label(pwn)<<
                                                "[shape=circle color=red style=clear"
                                                " height=.25 width=.25];" <<endl;
                                        } else if(op1 == 3) {
                                            strcpy(opr1,"Q");
                                            fout1<<"\tQ"<<Get_Label(pwn)<<
                                                "[shape=octagon color=black "
                                                "style=clear height=.25 width=.25];"
                                                <<endl;
                                        } else if(op1 == 4) {
                                            strcpy(opr1,"AC");
                                            fout1<<"\tAC"<<Get_Label(pwn)<<
                                                "[shape=doublecircle color=red "
                                                "style=clear height=.25 width=.25];"
                                                <<endl;
                                        } else if(op1 == 5) {
                                            strcpy(opr1,"QC");
                                            fout1<<"\tQC"<<Get_Label(pwn)<<
                                                "[shape=doubleoctagon color=red "
                                                "style=clear height=.25 width=.25];"
                                                <<endl;
                                        } else {
                                            strcpy(opr1,"Blah");//Never reached!
                                        }
                                    }
                                    if(op == 1) {
                                        strcpy(opr,"C");
                                        fout1<<"\tC"<<Get_Label(tmp)<<
                                            "[shape=circle color=blue style=clear "
                                            "height=.25 width=.25];"<<endl;
                                    } else if(op == 2) {
                                        strcpy(opr,"A");
                                        fout1<<"\tA"<<Get_Label(tmp)<<
                                            "[shape=circle color=red style=clear "
                                            "height=.25 width=.25];"<<endl;
                                    } else if(op == 3) {
                                        strcpy(opr,"Q");
                                        fout1<<"\tQ"<<Get_Label(tmp)<<
                                            "[shape=octagon color=black style=clear "
                                            "height=.25 width=.25];"<<endl;
                                    } else if(op == 4) {
                                        strcpy(opr,"AC");
                                        fout1<<"\tAC"<<Get_Label(tmp)<<
                                            "[shape=doublecircle color=red "
                                            "style=clear height=.25 width=.25];"<<endl;
                                    } else if(op == 5) {
                                        strcpy(opr,"QC");
                                        fout1<<"\tQC"<<Get_Label(tmp)<<
                                            "[shape=doubleoctagon color=red "
                                            "style=clear height=.25 width=.25];"<<endl;
                                    } else {
                                        strcpy(opr,"Blah");
                                    }

                                    if(pwn == NULL) {  //FUN-Entry
                                        if(p_cnt == 1) {
                                            fout1<<"\tC0 [shape=circle color=blue "
                                                "style=clear height=.25 width=.25];"
                                                <<endl;
                                            if(which_bar == 1) {
                                                fout1<<"\tB"<<lab<<"[shape=triangle "
                                                   "color=pink style=clear height=.25"
                                                   " width=.25];"<<endl;
                                                fout1<<"\tC0"<<" -> "<<"B"
                                                    <<lab<<";"<<endl;
                                                if(debug ==2) {
                                                    fprintf(stdout,"\tC0 -> B%lu",lab);
                                                    fprintf(stdout,";\n");
                                                }
                                            }
                                            else {
                                                fout1<<"\tBA"<<lab<<"[shape=triangle "
                                                   "color=pink style=clear height=.25 "
                                                   "width=.25];"<<endl;
                                                fout1<<"\tC0"<<" -> "<<"BA"<<lab<<";"
                                                    <<endl;
                                                if(debug ==2) {
                                                    fprintf(stdout,"\tC0 -> BA%lu",lab);
                                                    fprintf(stdout,";\n");
                                                }
                                            }
                                        }
                                        break;
                                    }//end-if(pwn==NULL)
                                    else {
                                        if(p_cnt == 1) {
                                            if (which_bar == 1) {
                                                fout1<<"\tB"<<lab<<"[shape=triangle "
                                                   "color=pink style=clear height=.25 "
                                                   "width=.25];"<<endl;
                                                fout1<<"\t"<<opr1<<Get_Label(pwn)<<" -> "
                                                    <<"B"<<lab<<";"<<endl;
                                                if(debug ==2) {
                                                    fprintf(stdout,"\t%s%lu -> B%lu",
                                                            opr1,Get_Label(pwn),lab);
                                                    fprintf(stdout,";\n");
                                                }
                                            } else {
                                                fout1<<"\tBA"<<lab<<"[shape=triangle "
                                                    "color=pink style=clear height=.25 "
                                                    "width=.25];"<<endl;
                                                fout1<<"\t"<<opr1<<Get_Label(pwn)<<
                                                    " -> "<<"BA"<<lab<<";"<<endl;
                                                if (debug ==2) {
                                                    fprintf(stdout,"\t%s%lu -> BA%lu",
                                                            opr1,Get_Label(pwn),lab);
                                                    fprintf(stdout,";\n");
                                                }
                                            }
                                        } else {
                                            fout1<<"\t"<<opr1<<Get_Label(pwn)<<
                                                " -> "<<opr<<Get_Label(tmp)<<";"<< endl;
                                            if (debug ==2) {
                                                fprintf(stdout,"\t%s%lu -> %s%lu",
                                                        opr1,Get_Label(pwn),opr,Get_Label(tmp));
                                                fprintf(stdout,";\n");
                                            }
                                        }
                                        p_cnt++;
                                    }//end-else
                                    tmp = pwn;
                                    pwn = Get_Parent(tmp);
                                }//end while(1)
                                p_cnt=1;
                            }//end-if(which_bar>0)
                        }
                    }
                }//end else
                Print_IR_maps(kid);
                kid = WN_next(kid);
            }
        } else {
            debug = 0;
            INT kidno;
            for (kidno = 0; kidno < WN_kid_count(w); kidno++) {
                WN *kid = WN_kid(w, kidno);
                if (kid) {
                    nbar = Get_attributes(kid);
                    if(nbar>0) {
                        op = Get_Operator(kid);
                        Set_Label(kid, nlab);
                        nlab++;
                        lab = Get_Label(kid);
                        if(debug ==1) {
                            if(op == 1) {
                                printf("Barriers:%lu, op:CONCAT, Label:%lu\n",nbar,lab);
                            } else if(op == 2) {
                                printf("Barriers:%lu, op:ALT, Label:%lu\n",nbar,lab);
                            } else if (op == 3) {
                                printf("Barriers:%lu, op:QUANT, Label:%lu\n",nbar,lab);
                            } else if (op == 4) {
                                printf("Barriers:%lu, op:ALT-CON, Label:%lu\n",nbar,lab);
                            } else if (op == 5) {
                                printf("Barriers:%lu, op:QUANT-CON, Label:%lu\n",nbar,lab);
                            } else {
                                printf("Something ain't right!!\n");
                            }
                        }
                    } else {
                        int which_bar=-1;
                        if (WN_has_sym(kid)) {
                            if(WN_st(kid)) {
                                varnm = ST_name(WN_st(kid));
                                lab = Get_Label(kid);
                                if (strcmp(varnm,"shmem_barrier") == 0) {
                                    which_bar = 1;
                                    if(debug ==1) fprintf(stdout,"B%d\n",lab);
                                }
                                if (strcmp(varnm,"shmem_barrier_all") == 0) {
                                    which_bar = 2;
                                    if(debug ==1) fprintf(stdout,"BA%d\n",lab);
                                }
                                if (which_bar > 0) {
                                    static int p_cnt=1;
                                    WN *tmp = kid, *pwn;

                                    pwn = Get_Parent(tmp);

                                    while(1) {
                                        unsigned long op = Get_Operator(tmp);
                                        char opr[20], opr1[20];
                                        unsigned long op1;
                                        if (pwn != NULL) {

                                            if (debug ==1) {
                                                printf("\nParent: ");
                                                fdump_wn(stdout, pwn);
                                            }
                                            op1 = Get_Operator(pwn);
                                            if(op1 == 1) {
                                                strcpy(opr1,"C");
                                                fout1<<"\tC"<<Get_Label(pwn)<<
                                                    "[shape=circle color=blue "
                                                    "style=clear height=.25 width=.25];"
                                                    <<endl;
                                            } else if (op1 == 2) {
                                                strcpy(opr1,"A");
                                                fout1<<"\tA"<<Get_Label(pwn)<<
                                                    "[shape=circle color=red "
                                                    "style=clear height=.25 width=.25];"
                                                    <<endl;
                                            } else if (op1 == 3) {
                                                strcpy(opr1,"Q");
                                                fout1<<"\tQ"<<Get_Label(pwn)<<
                                                    "[shape=octagon color=black "
                                                    "style=clear height=.25 width=.25];"
                                                    <<endl;
                                            } else if (op1 == 4) {
                                                strcpy(opr1,"AC");
                                                fout1<<"\tAC"<<Get_Label(pwn)<<
                                                    "[shape=doublecircle color=red "
                                                    "style=clear height=.25 width=.25];"
                                                    <<endl;
                                            } else if (op1 == 5) {
                                                strcpy(opr1,"QC");
                                                fout1<<"\tQC"<<Get_Label(pwn)<<
                                                    "[shape=doubleoctagon color=red "
                                                    "style=clear height=.25 width=.25];"
                                                    <<endl;
                                            } else {
                                                strcpy(opr1,"Blah");//Never reached!
                                            }
                                        }
                                        if(op == 1) {
                                            strcpy(opr,"C");
                                            fout1<<"\tC"<<Get_Label(tmp)<<
                                                "[shape=circle color=blue style=clear "
                                                "height=.25 width=.25];"<<endl;
                                        } else if(op == 2) {
                                            strcpy(opr,"A");
                                            fout1<<"\tA"<<Get_Label(tmp)<<
                                                "[shape=circle color=red style=clear "
                                                "height=.25 width=.25];"<<endl;
                                        } else if(op == 3) {
                                            strcpy(opr,"Q");
                                            fout1<<"\tQ"<<Get_Label(tmp)<<"[shape=octagon"
                                               " color=black style=clear height=.25 "
                                               "width=.25];"<<endl;
                                        } else if(op == 4) {
                                            strcpy(opr,"AC");
                                            fout1<<"\tAC"<<Get_Label(tmp)<<
                                                "[shape=doublecircle color=red "
                                                "style=clear height=.25 width=.25];"
                                                <<endl;
                                        } else if(op == 5) {
                                            strcpy(opr,"QC");
                                            fout1<<"\tQC"<<Get_Label(tmp)<<
                                                "[shape=doubleoctagon color=red "
                                                "style=clear height=.25 width=.25];"
                                                <<endl;
                                        } else {
                                            strcpy(opr,"Blah");
                                        }
                                        if(pwn == NULL) {  //FUN-Entry
                                            if(p_cnt == 1) {
                                                fout1<<"\tC0 [shape=circle color=blue "
                                                    "style=clear height=.25 width=.25];"
                                                    <<endl;
                                                if(which_bar == 1){
                                                    fout1<<"\tB"<<lab<<"[shape=triangle "
                                                        "color=pink style=clear height=.25"
                                                        " width=.25];"<<endl;
                                                    fout1<<"\tC0"<<" -> "<<"B"<<lab<<";"<<endl;
                                                    if(debug ==2){
                                                        fprintf(stdout,"\tC0 -> B%lu",lab);
                                                        fprintf(stdout,";\n");
                                                    }
                                                } else {
                                                    fout1<<"\tBA"<<lab<<"[shape=triangle "
                                                        "color=pink style=clear height=.25 "
                                                        "width=.25];"<<endl;
                                                    fout1<<"\tC0"<<" -> "<<"BA"<<lab<<";"<<endl;
                                                    if(debug ==2) {
                                                        fprintf(stdout,"\tC0 -> BA%lu",lab);
                                                        fprintf(stdout,";\n");
                                                    }
                                                }
                                            }
                                            break;
                                        }//end-if(pwn==NULL)
                                        else {
                                            if(p_cnt == 1) {
                                                if(which_bar == 1) {
                                                    fout1<<"\tB"<<lab<<"[shape=triangle "
                                                        "color=pink style=clear height=.25 "
                                                        "width=.25];"<<endl;
                                                    fout1<<"\t"<<opr1<<Get_Label(pwn)
                                                        <<" -> "<<"B"<<lab<<";"<<endl;
                                                    if(debug ==2) {
                                                        fprintf(stdout,"\t%s%lu -> B%lu",
                                                                opr1,
                                                                Get_Label(pwn),
                                                                lab);
                                                        fprintf(stdout,";\n");
                                                    }
                                                } else {
                                                    fout1<<"\tBA"<<lab<<"[shape=triangle "
                                                        "color=pink style=clear height=.25 "
                                                        "width=.25];"<<endl;
                                                    fout1<<"\t"<<opr1<<Get_Label(pwn)
                                                        <<" -> "<<"BA"<<lab<<";"<<endl;
                                                    if(debug ==2) {
                                                        fprintf(stdout,"\t%s%lu -> BA%lu",
                                                                opr1,
                                                                Get_Label(pwn),
                                                                lab);
                                                        fprintf(stdout,";\n");
                                                    }
                                                }
                                            } else {
                                                fout1<<"\t"<<opr1<<Get_Label(pwn)<<" -> "
                                                    <<opr<<Get_Label(tmp)<<";"<< endl;
                                                if(debug ==2) {
                                                    fprintf(stdout,"\t%s%lu -> %s%lu",
                                                            opr1,
                                                            Get_Label(pwn),
                                                            opr,
                                                            Get_Label(tmp));
                                                    fprintf(stdout,";\n");
                                                }
                                            }
                                            p_cnt++;
                                        }//end-else
                                        tmp = pwn;
                                        pwn = Get_Parent(tmp);
                                    }//end while(1)
                                    p_cnt=1;
                                }//end-if(which_bar>0)
                            }
                        }
                    }//end else
                    Print_IR_maps(kid);
                }
            }//end-for
        }//end-else
    }
}

//SP-end


int IPL_IsOpenSHMEM(char *input, int begin, int end) {
  int debug=0;
  char shmem_name[190][50] ={
 /*
  * Initialization & rtl  // 0
 */
    "first_name",
    "start_pes",   // 1
    "shmem_init",
    "shmem_finalize",
    "shmem_my_pe",  // 4
    "my_pe",
    "_my_pe",
    "shmem_num_pes",
    "shmem_n_pes",
    "num_pes",
    "_num_pes",
    "shmem_nodename",
    "shmem_version",
/*
 * I/O  // 13
 */
     "shmem_short_put",
     "shmem_int_put",
     "shmem_long_put",
     "shmem_longlong_put",
     "shmem_longdouble_put",
     "shmem_double_put",
     "shmem_float_put",
     "shmem_putmem",
     "shmem_put32",
     "shmem_put64",
     "shmem_put128",

      // 24

     "shmem_short_get",
     "shmem_int_get",
     "shmem_long_get",
     "shmem_longlong_get",
     "shmem_longdouble_get",
     "shmem_double_get",
     "shmem_float_get",
     "shmem_getmem",
     "shmem_get32",
     "shmem_get64",
     "shmem_get128",
    // 35
     "shmem_char_p",
     "shmem_short_p",
     "shmem_int_p",
     "shmem_long_p",
     "shmem_longlong_p",
     "shmem_float_p",
     "shmem_double_p",
     "shmem_longdouble_p",
    //43
     "shmem_char_g",
     "shmem_short_g",
     "shmem_int_g",
     "shmem_long_g",
     "shmem_longlong_g",
     "shmem_float_g",
     "shmem_double_g",
     "shmem_longdouble_g",

/*
 * non-blocking I/O
 */  //51
     "shmem_short_put_nb",
     "shmem_int_put_nb",
     "shmem_long_put_nb",
     "shmem_longlong_put_nb",
     "shmem_longdouble_put_nb",
     "shmem_double_put_nb",
     "shmem_float_put_nb",
     "shmem_putmem_nb",
     "shmem_put32_nb",
     "shmem_put64_nb",
     "shmem_put128_nb",
/*
 * strided I/O
 */  //62
     "shmem_double_iput",
     "shmem_float_iput",
     "shmem_int_iput",
     "shmem_iput32",
     "shmem_iput64",
     "shmem_iput128",
     "shmem_long_iput",
     "shmem_longdouble_iput",
     "shmem_longlong_iput",
     "shmem_short_iput",
     "shmem_double_iget",
     "shmem_float_iget",
     "shmem_int_iget",
     "shmem_iget32",
     "shmem_iget64",
     "shmem_iget128",
     "shmem_long_iget",
     "shmem_longdouble_iget",
     "shmem_longlong_iget",
     "shmem_short_iget",
/*
 * barriers
 */ //82
     "shmem_barrier_all",
     "shmem_barrier",
     "shmem_fence",
     "shmem_quiet",
/*
 * accessibility
 */  //86
     "shmem_pe_accessible",
     "shmem_addr_accessible",
/*
 * symmetric memory management
 */  // 88
     "shmalloc",
     "shfree",
     "shrealloc",
     "shmemalign",
     "shmem_malloc",
     "shmem_free",
     "shmem_realloc",
     "shmem_memalign",
     "sherror",
     "shmem_error",
/*
 * wait operations
 *///98
     "shmem_short_wait_until",
     "shmem_int_wait_until",
     "shmem_long_wait_until",
     "shmem_longlong_wait_until",
     "shmem_wait_until",
     "shmem_short_wait",
     "shmem_int_wait",
     "shmem_long_wait",
     "shmem_longlong_wait",
     "shmem_wait",
/*
 * atomic swaps
 *///108
     "shmem_int_swap",
     "shmem_long_swap",
     "shmem_longlong_swap",
     "shmem_float_swap",
     "shmem_double_swap",
     "shmem_swap",
     "shmem_int_cswap",
     "shmem_long_cswap",
     "shmem_longlong_cswap",
/*
 * atomic fetch-{add,inc} & add,inc
 */
    //117
     "shmem_int_fadd",
     "shmem_long_fadd",
     "shmem_longlong_fadd",
     "shmem_int_finc",
     "shmem_long_finc",
     "shmem_longlong_finc",
     "shmem_int_add",
     "shmem_long_add",
     "shmem_longlong_add",
     "shmem_int_inc",
     "shmem_long_inc",
     "shmem_longlong_inc",
/*
 * cache flushing
 *///129
     "shmem_clear_cache_inv",
     "shmem_set_cache_inv",
     "shmem_clear_cache_line_inv",
     "shmem_set_cache_line_inv",
     "shmem_udcflush",
     "shmem_udcflush_line",
/*
 * reductions
 */
//135
"shmem_complexd_sum_to_all",
"shmem_complexf_sum_to_all",
"shmem_double_sum_to_all",
"shmem_float_sum_to_all",
"shmem_int_sum_to_all",
"shmem_long_sum_to_all",
"shmem_longdouble_sum_to_all",
"shmem_longlong_sum_to_all",
"shmem_short_sum_to_all",
"shmem_complexd_prod_to_all",
"shmem_complexf_prod_to_all",
"shmem_double_prod_to_all",
"shmem_float_prod_to_all",
"shmem_int_prod_to_all",
"shmem_long_prod_to_all",
"shmem_longdouble_prod_to_all",
"shmem_longlong_prod_to_all",
"shmem_short_prod_to_all",
"shmem_int_and_to_all",
"shmem_long_and_to_all",
"shmem_longlong_and_to_all",
"shmem_short_and_to_all",
"shmem_int_or_to_all",
"shmem_long_or_to_all",
"shmem_longlong_or_to_all",
"shmem_short_or_to_all",
"shmem_int_xor_to_all",
"shmem_long_xor_to_all",
"shmem_longlong_xor_to_all",
"shmem_short_xor_to_all",
"shmem_int_max_to_all",
"shmem_long_max_to_all",
"shmem_longlong_max_to_all",
"shmem_short_max_to_all",
"shmem_longdouble_max_to_all",
"shmem_float_max_to_all",
"shmem_double_max_to_all",
"shmem_int_min_to_all",
"shmem_long_min_to_all",
"shmem_longlong_min_to_all",
"shmem_short_min_to_all",
"shmem_longdouble_min_to_all",
"shmem_float_min_to_all",
"shmem_double_min_to_all",
/*
 * broadcasts
 */
//179
     "shmem_broadcast32",
     "shmem_broadcast64",
     "shmem_sync_init",
/*
 * collects
 */
//182
     "shmem_fcollect32",
     "shmem_fcollect64",
     "shmem_collect32",
     "shmem_collect64",
/*
 * locks/critical section
 */
//186
     "shmem_set_lock",
     "shmem_clear_lock",
     "shmem_test_lock"
     };

  for(int i=begin;i<=end;i++) {
      if(strcmp(shmem_name[i],input)==0) {
          if(debug) printf("\n*** OpenSHMEM call found: %s ***\n", shmem_name[i]);
          return 1;
      }
  }
  //  if(debug)
  // printf("\nThis is the last SHMEM call: %s\n", shmem_name[188]);

  return 0;
}

#endif /* defined(OPENSHMEM_ANALYZER) */

/* ====================================================================
 *
 * Process_Command_Line
 *
 * Process the command line arguments.  Evaluate all flags except per-
 * source file control flags and set up global options.
 *
 * ====================================================================
 */
static void
Process_Command_Line (INT argc, char **argv)
{
    INT i;

    for (i = 0; i < argc; i++) {
	if (argv[i] != NULL && *(argv[i]) == '-') {

	    char *arg_str = argv[i];
	    if (Process_Command_Line_Group (arg_str+1,
					    IPL_Option_Groups))
	      {
		continue;
	      }
	    if (strcmp (arg_str, "-cmds") == 0) {
		driver_argc = argc - i - 1;
		if (driver_argc > 0)
		    driver_argv = argv + i + 1;
		i = argc;	    /* stop processing */
	    }
	}		   
    }

    if (OPT_Reorg_Common_Set)
      Do_Split_Commons = OPT_Reorg_Common;

    Do_Split_Commons_Set = OPT_Reorg_Common_Set && OPT_Reorg_Common;

    Do_Par = IPA_Enable_Array_Summary && Run_preopt;

    if (Do_Par)
      WOPT_Enable_Generate_DU = TRUE;

    Do_Common_Const = IPA_Enable_Common_Const && Run_preopt;

} // Process_Command_Line

void
ipl_main (INT ipl_argc, char **ipl_argv)
{
    extern char *Whirl_Revision;

    if (strcmp (Whirl_Revision, WHIRL_REVISION) != 0)
	FmtAssert (!DEBUG_Ir_Version_Check,
		   ("WHIRL revision mismatch between be.so (%s) and ipl.so (%s)", 
		    Whirl_Revision, WHIRL_REVISION));
    
    /* Perform preliminary command line processing: */
    Process_Command_Line (ipl_argc, ipl_argv);
    Optlevel = Opt_Level;
} // ipl_main


/* Initialization that needs to be done after the global symtab is read */
void
Ipl_Init (void)
{
    Set_Error_Phase ( "Ipl Initialization" );

    Summary = CXX_NEW (SUMMARY(Malloc_Mem_Pool), Malloc_Mem_Pool);

    // information for writing out array sections
    Init_write_asections (Malloc_Mem_Pool);

} /* Ipl_Init */


/* Initialization of IPL when it's called from IPA */
void
Ipl_Init_From_Ipa (MEM_POOL* pool)
{
  Summary = CXX_NEW (SUMMARY(pool), pool);
  Init_write_asections (pool);
} /* Ipl_Init_From_Ipa */

#ifdef OPENSHMEM_ANALYZER

void IPL_Nested_Checks(WN *lda, struct DU_MANAGER *du_mgr, int line,
                       int arg,ST *stm, int first)
{

    static long counter = 0;
    if(first) counter =0;

    counter ++;
    for (WN_ITER* wni = WN_WALK_TreeIter(lda); wni != NULL; wni = WN_WALK_TreeNext(wni)) {
                      WN *wn = WN_ITER_wn(wni);
		      if(wn!=lda) {
			// fdump_wn(stdout,wn);
                        DEF_LIST *def_list = du_mgr->Ud_Get_Def(wn);

                        if ( def_list != NULL) {
			  // printf("before incomplete");
			  if (def_list->Incomplete()) {
			    if(WN_operator(wn)==OPR_LDID) {
			      if (ST_sym_class(WN_st(wn)) == CLASS_PREG) {/* printf("***exception***"); */}
                              else return;
                            }
                            else return;
                          }
			  // printf("after incomplete");
                                DEF_LIST_ITER def_lst_iter;
                                const DU_NODE *tmp;
                                FOR_ALL_NODE(tmp, def_lst_iter, Init(def_list))
	                  {
			     WN *wn2 = tmp->Wn();
			     OPERATOR opr = WN_operator(wn2);
                                switch (opr) {
                                   case OPR_PICCALL:
                                   case OPR_CALL:
                                   case OPR_INTRINSIC_CALL:
                                   case OPR_IO:
                                   {
				       int check =0;
                                       if ( WN_has_sym( wn2 ) ) {
                                       char *name = ST_name( WN_st( wn2 ) );

                                       if(strcmp(name,"malloc")==0 ||
                                          strcmp(name,"calloc")==0 ||
                                          strcmp(name,"valloc")==0 ||
                                          strcmp(name,"realloc")==0 ||
                                          strcmp(name,"pvalloc")==0 ) {

                                           if(ST_sym_class(stm) == CLASS_PREG) {
                                             Print_OSA_Warning(Src_File_Name, line, 0,
                                                               "Local pointer in arg %d of "
                                                               "OpenSHMEM call initialized with %s",
                                                               arg, name);
	                                       }
                                          else {

                                             Print_OSA_Warning(Src_File_Name, line, 0,
                                                               "Local pointer in arg %d: %s of "
                                                               "OpenSHMEM call initialized with %s",
                                                               arg, ST_name(stm), name);
	                                  }

			               }


			          } // end of WN_has_sym
		                 } // end of case CALL
				   break;
				case OPR_FUNC_ENTRY:
                   if(ST_sym_class(stm) == CLASS_PREG) {
                       Print_OSA_Warning(Src_File_Name, line, 0,
                               "Uninitialized variable affecting arg %d of OpenSHMEM call",
                               arg);
                   }
                   else {

                       Print_OSA_Warning(Src_File_Name, line, 0,
                               "Uninitialized variable %s affecting arg %d of OpenSHMEM call",
                               ST_name(stm), arg);

                   }
				break;
				default:
				  //  fdump_wn(stdout,wn2);
                                  if(wn2!=lda && (counter < 500)) {
				    IPL_Nested_Checks(wn2, du_mgr, line, arg, stm,0);
				    // fdump_wn(stdout,wn2);
				    // printf("\nThis is the pointer: %p",wn2);
				  }
                               } // end of switch


			         //  fdump_wn(stdout,wn2);
			  }  // FOR_ALL

			}  // def_list != NULL
		      } // !=lda
    }  // end of for

}


void IPL_Check_OpenSHMEM_Initvars(WN *lda, int line, int arg,
                                  struct DU_MANAGER *du_mgr )
{

    // printf("enter initivars\n");
    if(WN_operator(lda) == OPR_LDID ) {
        // fdump_tree(stdout,lda);
        ST *st = WN_st(lda);
        ST_IDX st_idx = ST_st_idx(st);

        /* don't do analysis if data is global until IPA-link */
        if (ST_sclass(st) == SCLASS_UGLOBAL || ST_sclass(st) == SCLASS_DGLOBAL ||
                ST_sclass(st) == SCLASS_COMMON  || ST_sclass(st) == SCLASS_EXTERN ||
                ST_sclass(st) == SCLASS_FSTATIC || ST_sclass(st) == SCLASS_DGLOBAL ||
                ST_sclass(st) == SCLASS_FORMAL ) {
            return;
        }

        /* check for uninitializations */

        DEF_LIST *def_list = du_mgr->Ud_Get_Def(lda);
        if ( def_list != NULL) {

            if (def_list->Incomplete()) return;

            DEF_LIST_ITER def_lst_iter;
            const DU_NODE *tmp;
            FOR_ALL_NODE(tmp, def_lst_iter, Init(def_list))
            {
                if( WN_operator(tmp->Wn()) == OPR_FUNC_ENTRY) {

                    if(ST_sym_class(st) == CLASS_PREG) {
                       Print_OSA_Warning(Src_File_Name, line, 0,
                               "Uninitialized variable in arg %d of OpenSHMEM call",
                               arg);
                    }
                    else {
                       Print_OSA_Warning(Src_File_Name, line, 0,
                               "Uninitialized variable in arg %d: %s in OpenSHMEM call",
                               arg, ST_name(st));

                    }

                    return;
                }  // end of if OPR_FUNC_ENTRY


                IPL_Nested_Checks(tmp->Wn(),du_mgr, line, arg, st,1);

                /*
                   for (WN_ITER* wni = WN_WALK_TreeIter(tmp->Wn()); wni != NULL; wni = WN_WALK_TreeNext(wni)) {
                   WN *wn = WN_ITER_wn(wni);
                   if(wn!=tmp->Wn()) {
                   fdump_wn(stdout,wn);
                   }
                   } */

            } // end of FORALL

        }

        //  du_mgr->Print_Ud(lda,stdout);

    } // end of OPR_LDID

    // printf("exiting initvars\n");


}


void IPL_Check_Alias(WN *func_body, ALIAS_MANAGER* alias_mgr, WN* memopr_wn,
              WN* shmem_stmt, int arg_idx = 0, int line = 0, BOOL is_blocking = FALSE)
{
      USRCPOS target_linepos;

      int i, target_line;
      const int LEN_MAX=100;

      WN* arr_alias_wn[LEN_MAX];
      WN* arr_stmt[LEN_MAX];

      OPERATOR op = WN_operator(memopr_wn);

      /* Only mem opr like LDID, STID, ILOAD and ISTORE can be candidates for aliasing */
      if ( ( op==OPR_LDID || op==OPR_STID || op==OPR_ILOAD || op==OPR_ISTORE)) {

             for(i = 0 ; i < LEN_MAX; i++) {
                      arr_alias_wn[i]=NULL;
             }

             if(Find_all_alias(alias_mgr, memopr_wn, func_body,arr_alias_wn,
                               arr_stmt, LEN_MAX, is_blocking)){
                    for(i = 0 ; i < LEN_MAX && arr_alias_wn[i]!=NULL; i++) {
                          USRCPOS_srcpos(target_linepos)=WN_Get_Linenum(arr_stmt[i]);

                          /*get the innermost child to get the symbol name. This
                            is useful in case of multiple levels of indirection
                            e.g.:
                                 U8U8LDID 0 <1,78,z> T<86,anon_ptr.,8> [alias_id: 6,fixed]
                              U8U8ILOAD 0 T<77,anon_ptr.,8> T<86,anon_ptr.,8> [alias_id: 10]
                            U8PARM 2 T<77,anon_ptr.,8> #  by_value  [alias_id: 0]
                          */
                          WN* temp_wn=WN_kid0(memopr_wn);
                          OPERATOR temp_op = WN_operator(memopr_wn);
                          while(temp_wn!=NULL && ( temp_op==OPR_LDID || temp_op==OPR_STID ||
                                                   temp_op==OPR_ILOAD || temp_op==OPR_ISTORE)) {
                            memopr_wn = temp_wn;
                            temp_wn=WN_kid0(memopr_wn);
                            if(temp_wn==NULL){break;}
                            temp_op = WN_operator(temp_wn);
                          }

                          Print_OSA_Warning(Src_File_Name, line, 0,
                                  "Symmetric Variable named %s in arg %d of OpenSHMEM call "
                                  "may be aliased with the pointer in line %d",
                                  ST_name(WN_st_idx(memopr_wn)), arg_idx,
                                  USRCPOS_linenum(target_linepos));
                          IPL_WriteLinkHTML(TRUE,USRCPOS_linenum(target_linepos),FALSE);

                     }
/*                     printf("\n*** OpenSHMEM Warning: Symmetric Variable named %s in arg%d of OpenSHMEM call (line=%d, file=%s)  may be aliased unsafely with the following: *** \n",
                            ST_name(WN_st_idx(memopr_wn)), arg_idx, line, Src_File_Name
                           );

                     for(i = 0 ; i < LEN_MAX && arr_alias_wn[i]!=NULL; i++) {
                          USRCPOS_srcpos(target_linepos)=WN_Get_Linenum(arr_stmt[i]);
                          printf("\t\t %d: at line %d with ",
                                 i+1,
                                 USRCPOS_linenum(target_linepos)
                                );
                          Print_Info(alias_mgr, arr_alias_wn[i]);
                          printf(" memory reference\n");
                     } */
             }
      }
}

void IPL_Check_OpenSHMEM_Get(WN *wn,struct DU_MANAGER *du_mgr, ALIAS_MANAGER*
alias_mgr , WN* func_body)
{
  int const_length = 0;
  SRCPOS srcpos = WN_Get_Linenum(wn);
  USRCPOS linepos;
  USRCPOS_srcpos(linepos) = srcpos;
  int line = USRCPOS_linenum(linepos);
  int arg=2;

  WN *param =  WN_kid(wn,arg-1); // Second param operator
  WN *ldid = WN_kid(param,0); // LDID 2

  IPL_Check_Alias(func_body, alias_mgr, ldid, wn, arg, line, TRUE);

  /*
  WN *param2 = WN_kid(wn,0); // Second parameter
  WN *lda2 = WN_kid(param2,0); // LDA 1

  WN *param3 = WN_kid(wn,2);
  WN *u8const = WN_kid(param3,0);
  */

  for (WN_ITER* wni = WN_WALK_TreeIter(ldid); wni != NULL;
       wni = WN_WALK_TreeNext(wni)) {
      WN *wn = WN_ITER_wn(wni);
      IPL_Check_OpenSHMEM_Initvars(wn,line,2,du_mgr );
  }
}

void IPL_Check_OpenSHMEM_Put_General(WN *wn, struct DU_MANAGER *du_mgr,
                    ALIAS_MANAGER* alias_mgr, WN* func_body )
{
  SRCPOS srcpos = WN_Get_Linenum(wn);
  USRCPOS linepos;
  USRCPOS_srcpos(linepos) = srcpos;
  int line = USRCPOS_linenum(linepos);
  int arg=1;

  WN *param =  WN_kid(wn,arg-1); // First param operator
  WN *ldid = WN_kid(param,0); // LDA
  OPERATOR op = WN_operator(ldid);

  IPL_Check_Alias(func_body, alias_mgr, ldid, wn, arg, line, FALSE);
  /*
  WN *param2 = WN_kid(wn,1); // Second parameter
  WN *lda2 = WN_kid(param2,0); // LDA 2

  WN *param3 = WN_kid(wn,2);
  WN *u8const = WN_kid(param3,0);
  // printf("\nline number=%d",line);
  */

   for (WN_ITER* wni = WN_WALK_TreeIter(ldid); wni != NULL; wni = WN_WALK_TreeNext(wni)) {
        WN *wn = WN_ITER_wn(wni);
        IPL_Check_OpenSHMEM_Initvars(wn,line,1,du_mgr );
   }
}


void OpenSHMEM_Local_IO(WN *entry, struct DU_MANAGER *du_mgr, ALIAS_MANAGER* alias_mgr)
{

     for (WN_ITER* wni = WN_WALK_TreeIter(entry);
            wni != NULL;
            wni = WN_WALK_TreeNext(wni))
            {
             WN *wn = WN_ITER_wn(wni);
             OPERATOR opr = WN_operator(wn);
             switch (opr) {
                  case OPR_PICCALL:
                  case OPR_CALL:
                  case OPR_INTRINSIC_CALL:
                  case OPR_IO:
                      {
                          if ( WN_has_sym( wn ) ) {
                			   char *name = ST_name( WN_st( wn ) );

			                   if(IPL_IsOpenSHMEM(name,13,23) ||
                                  IPL_IsOpenSHMEM(name,51,61) ||
                                  IPL_IsOpenSHMEM(name,72,81) ||
                                  IPL_IsOpenSHMEM(name,35,50) || /* g p */
                                  IPL_IsOpenSHMEM(name,87,87) || /* accessibility */
                                  IPL_IsOpenSHMEM(name,98,128) || /* atomics */
                                  IPL_IsOpenSHMEM(name,131,132) || /* cache inv */
			                      IPL_IsOpenSHMEM(name,134,134)) /* cache flush */   {
                    			     //  printf("\nEntering SHMEMCALL=%s\n",name);
			                        IPL_Check_OpenSHMEM_Put_General(wn,du_mgr,alias_mgr, entry);
			                   }
			                   if(IPL_IsOpenSHMEM(name,24,34) || IPL_IsOpenSHMEM(name,72,81)) {
                                    IPL_Check_OpenSHMEM_Get(wn,du_mgr, alias_mgr, entry);
		                  	   }

		            	 } // end of WN_has_sym
		              } // end of case CALL
               } // end of switch
              } // end of traversal
}

#endif /* defined(OPENSHMEM_ANALYZER) */

/*-----------------------------------------------------------------*/
/* entry point into the local phase                                */
/*-----------------------------------------------------------------*/
void
Perform_Procedure_Summary_Phase (WN* w, struct DU_MANAGER *du_mgr,
				 struct ALIAS_MANAGER *alias_mgr,
				 void *emitter)
{
    Trace_IPA = Get_Trace (TP_IPL, TT_IPL_IPA);

    if ( Debug_On )
	IR_reader_init();

    if (IPL_Enable_Outline) {
	const WN* wn = Outline_Split_Point (w, IPA_PU_Minimum_Size,
					    IPA_Small_Callee_Limit / 2);
	if (wn) {
	    fprintf (TFile, "Splitting %s:\n", ST_name (WN_st (w)));
	    fdump_tree (TFile, const_cast<WN*> (wn));
	}
    }

    if (Trace_IPA) {
	fprintf ( TFile, "Summarizing procedure %s \n", ST_name(WN_st(w)) );
    }

    DoPreopt = Run_preopt;
    if (Run_preopt && Cur_PU_Feedback) {
	BOOL not_pass = Cur_PU_Feedback->Verify ("IPL");
	if (not_pass) //FB_VERIFY_CONSISTENT = 0 
	    DevWarn ("Feedback verify fails after preopt");
    }
    
    WB_IPL_Set_Scalar_Summary(Summary);
    WB_IPL_Set_Array_Summary(NULL);
    Summary->Set_du_mgr (du_mgr);
    Summary->Set_alias_mgr (alias_mgr);
    Summary->Set_emitter ((EMITTER *) emitter);
    Summary->Summarize (w);

#ifdef OPENSHMEM_ANALYZER

    if (OSA_Flag) {

        char message_filename[500];
        strcpy(message_filename,Src_File_Name);
        strcpy(ipl_current_source,Src_File_Name);
        strcat(message_filename,".msg");
        strcat(ipl_current_source,".html");

        iplmessagesout  = fopen (message_filename,"w");

        // barrier matching code here
        {

            //Moved here from opt_main.cxx
            extern INT64 *cfgnode_id_ptr;
            extern INT64 *cfgedge_id_ptr;

            extern OSAedge *cfged_ptr;
            extern OSAnode *cfgentrynode_ptr, *cfgnode_ptr;
            extern OSAgraph *cfgraph_ptr;

            //SP: Don't need these for changed logic.
            extern INT64 osa_bb_start[100];
            extern INT64 osa_bb_end[100];
            int i,j;

            //SP: Controls what to print to stdout, for debugging
            int printtree=0;
            if (w) {
                WN* stmt;
                INT64 tmp_defid = -1;
                INT64 def_id, line, multiV=-1, is_cond=-1;
                char *opcode, *varnm, *fname, *vid, *line1;
                char opcode_str[20];

                if (printtree==1) {
                    fprintf(stdout, "WN Tree Dump\n");
                    fdump_tree(stdout, w);
                }

                WN *body = WN_func_body(w);
                if (body) {
                    *cfgnode_id_ptr = *cfgnode_id_ptr + 1;

                    if (WN_opcode(w)) {
                        opcode = OPCODE_name(WN_opcode(w));
                        strcpy(opcode_str,OPCODE_name(WN_opcode(w)));
                    }

                    varnm = "ROOT";
                    USRCPOS pos;
                    USRCPOS_srcpos(pos) = WN_Get_Linenum(w);
                    line =  USRCPOS_linenum(pos);
                    fname = Src_File_Name;

                    cfged_ptr->wn_def = w;
                    cfgentrynode_ptr->set_values(*cfgnode_id_ptr,
                                                 tmp_defid,
                                                 opcode_str,
                                                 varnm,line,
                                                 w,
                                                 NULL,
                                                 fname,
                                                 multiV,
                                                 is_cond);

                    if (printtree == 3) {
                        printf("Entry: id: %d,opcode: %s, varnm: %s, "
                               "line: %d, wn: %p \n", *cfgnode_id_ptr,
                               opcode, varnm, line, w);
                    }

                    //SP: Here def_id refers to the Dominator
                    cfged_ptr->def_id = *cfgnode_id_ptr;
                    //tmp_defid = *cfgnode_id_ptr;

                    if (printtree==1) {
                        fprintf(stdout, "Body exists \n");
                        fdump_wn(stdout,body);
                    }
                    if (printtree==2) {
                        printf("Statements depending on ENTRY:\n");
                    }

                    for (stmt = WN_first(body);  stmt != NULL;
                         stmt = WN_next(stmt)) {
                        // fprintf(stdout, "INSIDE if(w) and for()\n\n");
                        *cfgnode_id_ptr = *cfgnode_id_ptr + 1;
                        *cfgedge_id_ptr = *cfgedge_id_ptr + 1;
                        //SP: Here use_id refers to the the dominated
                        cfged_ptr->use_id = *cfgnode_id_ptr;
                        cfged_ptr->wn_use = stmt;
                        cfged_ptr->wn_parent = NULL;
                        cfged_ptr->id = *cfgedge_id_ptr;
                        cfgentrynode_ptr->edges.push_back(*cfged_ptr);
                        if (printtree==2) {
                            fdump_wn(stdout, stmt);
                        }

                        opcode  = OPCODE_name(WN_opcode(stmt));
                        char opcode_str[20];
                        strcpy(opcode_str,OPCODE_name(WN_opcode(stmt)));

                        /*
                        if (WN_has_sym(stmt)) { //this may not work
                            if(ST_name(WN_st(stmt))) {
                                varnm = ST_name(WN_st(stmt));
                                char *tmp = varnm;
                                if(strstr(tmp,"."))
                                    varnm = strstr(varnm,".") + 1;
                            }
                        } else */
                        //SP: Statements don't have symbols?
                        varnm = "NULL";
                        USRCPOS pos;
                        USRCPOS_srcpos(pos) = WN_Get_Linenum(stmt);
                        line =  USRCPOS_linenum(pos);
                        fname = Src_File_Name;

                        unsigned long int  stmt_off = stmt - w;
                        cfgnode_ptr->set_values(*cfgnode_id_ptr,
                                                tmp_defid,
                                                opcode_str,
                                                varnm,
                                                line,
                                                stmt,
                                                w,
                                                fname,
                                                multiV,
                                                is_cond);
                        cfgraph_ptr->nodes.push_back(*cfgnode_ptr);

                        if (printtree == 2) {
                            printf("CFnode: id: %d,opcode: %s, line: %d,"
                                   "wn_self:%p \n",*cfgnode_id_ptr,opcode,
                                   line,stmt);
                        }//end-if printcfg
                    }//end-for
                    cfgraph_ptr->nodes.push_back(*cfgentrynode_ptr);
                }//end-if body

                //  printf("Value of *cfgnode_id_ptr = %d\n\n\n", *cfgnode_id_ptr);
                if (printtree == 3) {
                    for(int i=0; i< cfgraph_ptr->nodes.size();i++) {
                        if(cfgraph_ptr->nodes[i].is_mVal == 1) {
                            printf("Node: id: %d, def_id: %d, var_nm: %s,"
                                   "opcode: %s, line: %d, wn_offest:%lu "
                                   "multivalued:%d, is_conditional: %d\n",
                                   cfgraph_ptr->nodes[i].id,
                                   cfgraph_ptr->nodes[i].def_id,
                                   cfgraph_ptr->nodes[i].var_nm,
                                   cfgraph_ptr->nodes[i].opcode,
                                   cfgraph_ptr->nodes[i].line,
                                   (cfgraph_ptr->nodes[i].wn_parent-
                                    cfgraph_ptr->nodes[i].wn_self),
                                   cfgraph_ptr->nodes[i].is_mVal,
                                   cfgraph_ptr->nodes[i].is_conditional);
                        }
                    }//end-for
                }//end-if
            }//end-if(w)

            //printf("\n\n");

            // Post-procesing, here we:

            // 1. Propagate multivalued seed ----> Done in opt_du.cxx,
            //    hence commented out

            /*
               printf("\n\nAfter propagation\n");
               for(int i=0; i< cfgraph_ptr->nodes.size();i++) {
                   if(cfgraph_ptr->nodes[i].is_mVal == 1) {
                       printf(" Node: id: %d, def_id: %d, var_nm: %s,"
                              " opcode: %s, line: %d,mVAL: %d\n",
                              cfgraph_ptr->nodes[i].id,
                              cfgraph_ptr->nodes[i].def_id,
                              cfgraph_ptr->nodes[i].var_nm,
                              cfgraph_ptr->nodes[i].opcode,
                              cfgraph_ptr->nodes[i].line,
                              cfgraph_ptr->nodes[i].is_mVal);
                   }//end-if
               }//end-for
            */

            // 2. Merge the nodes with the same wn_self

            // for every node
            for(int i=0; i< cfgraph_ptr->nodes.size();i++) {
                // compare with all nodes
                for(int j=i+1; j< cfgraph_ptr->nodes.size();j++) {
                    // found repeat
                    if(cfgraph_ptr->nodes[i].wn_self ==
                       cfgraph_ptr->nodes[j].wn_self) {
                        INT64 tmp_repeat = cfgraph_ptr->nodes[j].id;
                        cfgraph_ptr->nodes[j].id = cfgraph_ptr->nodes[i].id;
                        // if either of the nodes rep the same WN are marked
                        // as MV, mark all copies as MV
                        if(cfgraph_ptr->nodes[i].is_mVal==1 ||
                           cfgraph_ptr->nodes[j].is_mVal == 0) {
                            cfgraph_ptr->nodes[i].is_mVal = 1;
                            cfgraph_ptr->nodes[j].is_mVal = 1;
                        }
                    }//end-if

                }//end-for
            }//end-for

            //SP: Now that all the information is combined we traverse the IR
            //to get barrier information
            MEM_POOL local_mempool;
            MEM_POOL_Initialize( &local_mempool, "WN_OSA_BMatch_Pool", FALSE );
            MEM_POOL_Push( &local_mempool);

            OSA_Parent_Map = WN_MAP_Create(&local_mempool);
            OSA_B_Map = WN_MAP_Create(&local_mempool);
            OSA_BA_Map = WN_MAP_Create(&local_mempool);
            OSA_Barrier_Map = WN_MAP_Create(&local_mempool);
            OSA_Operator_Map = WN_MAP_Create(&local_mempool);
            OSA_Label_Map = WN_MAP_Create(&local_mempool);
            OSA_PEstart_Map = WN_MAP_Create(&local_mempool);
            OSA_Lstride_Map = WN_MAP_Create(&local_mempool);
            OSA_Npes_Map = WN_MAP_Create(&local_mempool);
            OSA_MultiV_Map = WN_MAP_Create(&local_mempool);
            OSA_Lineno_Map = WN_MAP_Create(&local_mempool);
            OSA_Discovered_Map = WN_MAP_Create(&local_mempool);

            Set_Parent(w,NULL);
            //printf("Before Annotate\n");
            Annotate_btree(w);
            //printf("After Annotate, before Mark\n");
            Mark_barriers(w,du_mgr);
            if (Check_barriers(w, du_mgr) == 0)
                Match_barriers(w,du_mgr);
            //printf("After Match barriers\n");
            //else
            //  printf("There IS barrier mismatch, please make corrections.\n");

            unsigned long nBar,nB,nBA;
            nBar=Get_attributes(w);
            nB=Get_num_B(w);
            nBA=Get_num_BA(w);

            //printf("Total barriers:%lu, B:%lu, BA:%lu\n",nBar,nB,nBA);

            //SP:TO:Print only marked IR
            //printf("After Mark, before IR maps\n");
            string pu_file_name="BarrierTree.dot";
            fout1.open(pu_file_name.c_str(),ios::out);
            fout1 <<"strict digraph system{\n";
            fout1 <<"size=\"10,8\"\n";
            fout1 << "\t node [shape = circle, color=blue, "
                     "style=clear, size=\"30,30\"];\n";

            Print_IR_maps(w);

            fout1 << "}\n";
            fout1.close();

            MEM_POOL_Pop( &local_mempool);
            MEM_POOL_Delete( &local_mempool);

            //SP-end
        }

        // Dump_alias_mgr(alias_mgr, w, stdout);
        OpenSHMEM_Local_IO(w,du_mgr, alias_mgr);

    }

#endif /* defined(OPENSHMEM_ANALYZER) */

    WB_IPL_Set_Array_Summary(NULL);
    WB_IPL_Set_Scalar_Summary(NULL);

#ifdef OPENSHMEM_ANALYZER
    if (OSA_Flag) {
        fclose (iplmessagesout);
    }
#endif
 
} // Perform_Procedure_Summary_Phase

void
Ipl_Fini (void)
{
    Summary->Set_global_addr_taken_attrib ();
    return;
} // Ipl_Fini


void
Ipl_Extra_Output (Output_File *ir_output)
{
#ifndef KEY
	if(IPA_Enable_Reorder)/*&&Feedback_Enabled[PROFILE_PHASE_BEFORE_VHO] )*/
#endif // !KEY
		Summary->Finish_collect_struct_access();// reorder, free pointers, set hot_flds
    IPA_write_summary(IPA_irb_write_summary, ir_output);

    if ( Get_Trace ( TKIND_IR, TP_IPL ) )
	IPA_Trace_Summary_File ( TFile, ir_output, TRUE, 
	  Ipl_Symbol_Names, Ipl_Function_Names );	

    if (driver_argc > 0)
	WN_write_flags (driver_argc, driver_argv, ir_output);

#if defined(__linux__) || defined(BUILD_OS_DARWIN)
    // write out the Elf version of global symtab
    IPL_Write_Elf_Symtab (ir_output);
#endif // __linux__
    
} // Ipl_Extra_Output
