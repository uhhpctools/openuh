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

void IPL_WriteHTML (char * format, ...)
{
  va_list args;
  va_start (args, format);
  vfprintf (iplmessagesout, format, args);
  va_end (args);
}

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
					     printf("\n*** OpenSHMEM Warning: Local pointer in arg%d of OpenSHMEM call (line=%d, file=%s) initialized with %s ***\n", arg,line,Src_File_Name,name);
                                             IPL_WriteLinkHTML(TRUE,line);
                                             IPL_WriteHTML("\n*** OpenSHMEM Warning: Local pointer in arg%d of OpenSHMEM call (line=%d, file=%s) initialized with %s ***\n", arg,line,Src_File_Name,name);
	                                       }
                                          else {
		                             printf("\n*** OpenSHMEM Warning: Local pointer in arg%d: %s of OpenSHMEM call (line=%d, file=%s) initialized with %s ***\n", arg,ST_name(stm),line,Src_File_Name,name);
                                             IPL_WriteLinkHTML(TRUE,line);
                                             IPL_WriteHTML("\n*** OpenSHMEM Warning: Local pointer in arg%d: %s of OpenSHMEM call (line=%d, file=%s) initialized with %s ***\n", arg,ST_name(stm),line,Src_File_Name,name);
	                                  }

			               }


			          } // end of WN_has_sym
		                 } // end of case CALL
				   break;
				case OPR_FUNC_ENTRY:
                                    if(ST_sym_class(stm) == CLASS_PREG) {
                   printf("\n*** OpenSHMEM Warning: Uninitialized variable affecting arg%d of OpenSHMEM call (line=%d, file=%s) ***\n", arg,line,Src_File_Name);
                   IPL_WriteLinkHTML(TRUE,line);
                   IPL_WriteHTML("\n*** OpenSHMEM Warning: Uninitialized variable affecting arg%d of OpenSHMEM call (line=%d, file=%s) ***\n", arg,line,Src_File_Name);
	      }
              else {
		printf("\n*** OpenSHMEM Warning: Uninitialized variable %s affecting arg%d: in OpenSHMEM call (line=%d, file=%s) ***\n", ST_name(stm),arg,line,Src_File_Name);
                IPL_WriteLinkHTML(TRUE,line);
	        IPL_WriteHTML("\n*** OpenSHMEM Warning: Uninitialized variable %s affecting arg%d: in OpenSHMEM call (line=%d, file=%s) ***\n", ST_name(stm),arg,line,Src_File_Name);

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
                        printf("\n*** OpenSHMEM Warning: Uninitialized variable "
                               "in arg%d of OpenSHMEM call (line=%d, file=%s) ***\n",
                               arg,line,Src_File_Name);
                        IPL_WriteLinkHTML(TRUE,line);
                        IPL_WriteHTML("\n*** OpenSHMEM Warning: Uninitialized variable "
                                      "in arg%d of OpenSHMEM call (line=%d, file=%s) ***\n",
                                       arg,line,Src_File_Name);
                    }
                    else {
                        printf("\n*** OpenSHMEM Warning: Uninitialized variable  "
                               "in arg%d: %s in OpenSHMEM call (line=%d, file=%s) ***\n",
                               arg,ST_name(st),line,Src_File_Name);
                        IPL_WriteLinkHTML(TRUE,line);
                        IPL_WriteHTML("\n*** OpenSHMEM Warning: Uninitialized "
                                "variable in arg%d: %s in OpenSHMEM call "
                                "(line=%d, file=%s) ***\n",
                                arg,ST_name(st),line,Src_File_Name);

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
                          while(temp_wn!=NULL && ( op==OPR_LDID || op==OPR_STID ||
                                                   op==OPR_ILOAD || op==OPR_ISTORE)) {
                            memopr_wn = temp_wn;
                            WN* temp_wn=WN_kid0(memopr_wn);
                            if(temp_wn==NULL){break;}
                            OPERATOR temp_op = WN_operator(temp_wn);
                          }

                          printf("\n*** OpenSHMEM Warning: Symmetric Variable "
                                 "named %s in arg%d of OpenSHMEM call "
                                 "(line=%d, file=%s) may be aliased with the "
                                 "pointer in line %d*** \n",
                                ST_name(WN_st_idx(memopr_wn)),
                                arg_idx,
                                line,
                                Src_File_Name,
                                USRCPOS_linenum(target_linepos)
                                );
                          IPL_WriteLinkHTML(TRUE,line);
                          IPL_WriteHTML("\n*** OpenSHMEM Warning: Symmetric "
                                 "Variable named %s in arg%d of OpenSHMEM call  "
                                 "(line=%d, file=%s) may be aliased with the "
                                 "pointer in line %d*** \n",
                                ST_name(WN_st_idx(memopr_wn)),
                                arg_idx,
                                line,
                                Src_File_Name,
                                USRCPOS_linenum(target_linepos)
                                );
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
