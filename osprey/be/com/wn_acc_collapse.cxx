 /***************************************************************************
  This file is created by Xiaonan(Daniel) Tian from HPCTools, University of Houston
  (daniel.xntian@gmail.com) for OpenUH OpenACC compiler.
  It is intended to lower the OpenACC pragma.
  It is free to use. However, please keep the original author.
  http://www2.cs.uh.edu/~xntian2/
*/

#include <stdint.h>
#ifdef USE_PCH
#include "be_com_pch.h"
#endif /* USE_PCH */
#pragma hdrstop

/* Header of wn_mp_dg.cxx
*  csc.
*/
#include <sys/types.h>
#if defined(BUILD_OS_DARWIN)
#include <darwin_elf.h>
#else /* defined(BUILD_OS_DARWIN) */
#include <elf.h>
#endif /* defined(BUILD_OS_DARWIN) */

#define USE_STANDARD_TYPES          /* override unwanted defines in "defs.h" */

#include <bstring.h>
#include "wn.h"
#include "wn_util.h"
#include "erglob.h"
#include "errors.h"
#include "strtab.h"                 /* for strtab */
#include "symtab.h"                 /* for symtab */
#include "irbdata.h"                /* for inito */
#include "dwarf_DST_mem.h"          /* for DST */
#include "pu_info.h"
#ifdef __MINGW32__
#include <WINDOWS.h>
#endif /* __MINGW32__ */
#include "ir_bwrite.h"
#include "ir_reader.h"
#include "ir_bcom.h"
#include "region_util.h"            /* for RID */
#include "dep_graph.h"
#include "cxx_hash.h"
#include "file_util.h"      /* For Last_Pathname_Component */
#include "wn_tree_util.h"
/* wn_mp_dg.cxx header end.
*  csc.
*/

#include <string.h>

#if ! defined(BUILD_OS_DARWIN)
#include <elf.h>
#endif /* ! defined(BUILD_OS_DARWIN) */
#include "alloca.h"
#include "cxx_template.h"
#include "defs.h"
#include "glob.h"
#include "errors.h"
#include "erglob.h"
#include "erbe.h"
#include "tracing.h"
#include "strtab.h"

#include "symtab.h"


#include "wn_util.h"
#include "wn_simp.h"
#include "stblock.h"
#include "data_layout.h"
#include "targ_sim.h"
#include "targ_const.h"
#include "config_targ.h"
#include "config_asm.h"
#include "const.h"
#include "ttype.h"
#include "wn_pragmas.h"
#include "wn_lower.h"
#include "region_util.h"
#include "wutil.h"
#include "wn_map.h"
#include "pu_info.h"
#include "config.h"
#include "standardize.h"
#include "irbdata.h"
#include "privatize_common.h"
#include "cxx_hash.h"
#include "wn_acc.h"
#include "mempool.h"
#include "parmodel.h"	// for NOMINAL_PROCS
#include "fb_info.h"
#include "fb_whirl.h"
#include "be_symtab.h"
#ifdef KEY
#include "wn_lower.h"
#include "config_opt.h"
#endif
#include "alias_analyzer.h"

vector<WN*>	acc_loop_lower_bound;
vector<WN*> acc_loop_upper_bound;
vector<WN*> acc_loop_ntrips;
vector<WN*> acc_loop_step;
UINT32 acc_collapse_count;   /* collapse count */

static TYPE_ID acc_collapse_mtype;

static void ACC_Fill_Each_Stride( )
{
	//calculate stride	
	WN* wn_this_trips, *wn_newtrips;
	WN* wn_lower;
	WN* wn_upper;
	UINT32 collapse_count = acc_collapse_count;
	
	for (UINT32 i = 0; i < collapse_count; i++) 
	{		
		wn_lower = acc_loop_lower_bound[i];
		wn_upper = acc_loop_upper_bound[i];
		wn_newtrips = WN_Binary(OPR_SUB, acc_collapse_mtype, 
						WN_COPY_Tree(wn_upper), WN_COPY_Tree(wn_lower));
		for(UINT32 j=i+1; j<collapse_count; j++)
		{			
			wn_lower = acc_loop_lower_bound[j];
			wn_upper = acc_loop_upper_bound[j];
			wn_this_trips = WN_Binary(OPR_SUB, acc_collapse_mtype, 
						WN_COPY_Tree(wn_upper), WN_COPY_Tree(wn_lower));
			wn_this_trips = WN_Binary(OPR_ADD, acc_collapse_mtype, 
						wn_this_trips, WN_Intconst(acc_collapse_mtype, 1));
			wn_newtrips = WN_Binary(OPR_MPY, acc_collapse_mtype, 
							wn_newtrips, wn_this_trips);
		}
		acc_loop_ntrips.push_back(wn_newtrips);
	}
}

static void ACC_Fill_Upper_Lower_Bound( )
{
	UINT32 collapse_count = acc_collapse_count;
	
	for (UINT32 i = 0; i < collapse_count; i++) 
	{
		acc_loop_lower_bound.push_back(acc_base_node[i]);
		acc_loop_upper_bound.push_back(acc_limit_node[i]);
	}
	//Fill the stride
	ACC_Fill_Each_Stride( );
}

/**************************************************************************************
* after the Collapse transformation,  the original one piece of the code can be separated into three sections
* 
* Initial statements (start_block)
* for loop (new loop index)
* {
*	 intial original nested loop index (indices initial block)
*	 original loop body	(new doloop body)
* }
* When Collapse is used, the nest loop have to be perfect nested loop
* Return a new do loop tree
*
**************************************************************************************/

void ACC_Rewrite_Collapsed_Do (WN* wn_start_block, WN* wn_indices_init_block)
{
	//WN * start_block = WN_CreateBlock();
	//WN * init_indices_block = WN_CreateBlock();
	//WN * new_doloop_body = WN_CreateBlock();
	WN * lastcond = NULL;
	WN * cond = NULL;
	WN * incr_block = NULL;
	UINT32 collapse_count = acc_collapse_count;
	//regenerate a new do loop statement
	//loop trip counts = times all of the nested loop counts
	//step is plus 1, which is mandatory
	//the init value is from 0.

	acc_collapse_mtype = acc_forloop_index_type[0];

	WN* wn_newtrips = WN_Intconst(acc_collapse_mtype, 1);
	ST* st_newindex;
	ST* st_newtotaltrips;
	ST* st_nexttrip;
	WN* wn_this_trips;
	WN* wn_lower;
	WN* wn_upper;
	
	ACC_Device_Create_Preg_or_Temp(acc_collapse_mtype, "collapse_idx", &st_newindex);
	ACC_Device_Create_Preg_or_Temp(acc_collapse_mtype, "collapse_trips", &st_newtotaltrips);
	ACC_Device_Create_Preg_or_Temp(acc_collapse_mtype, "next_trips", &st_nexttrip);
	WN* wn_new_loop_idx = Gen_ACC_Load(st_newindex, 0, FALSE);
	acc_loop_lower_bound.clear();
	acc_loop_upper_bound.clear();
	acc_loop_ntrips.clear();
	ACC_Fill_Upper_Lower_Bound();
	
	//new trips
	wn_newtrips = acc_loop_ntrips[0];
		
	//WN* wn_store_trips = Gen_ACC_Store(st_newtotaltrips, 0, wn_newtrips, FALSE);
	//WN_INSERT_BlockLast(wn_start_block, wn_store_trips);
	//generate the new index value
	for (UINT32 i = 0; i < collapse_count; i++) 
	{	    		
		ST * acc_index_st = acc_forloop_index_st[i];
		//TYPE_ID acc_index_type = acc_forloop_index_type[i];
		WN* wn_org_index = Gen_ACC_Load(acc_index_st, 0, FALSE);
		WN* wn_next_trip = Gen_ACC_Load(st_nexttrip, 0, FALSE);
		//stride is the next loop trip
		WN* wn_loop_stride;
		if(i<(collapse_count-1))
			wn_loop_stride = acc_loop_ntrips[i+1];
		else
			wn_loop_stride = WN_Intconst(acc_collapse_mtype, 1);

		WN* wn_tmp_value, *wn_tmp_trips; 
		if(i==0)
			wn_tmp_trips = WN_COPY_Tree(wn_new_loop_idx);
		else
			wn_tmp_trips = wn_next_trip;
		
		wn_tmp_value = WN_Binary(OPR_DIV, acc_collapse_mtype, 
							wn_tmp_trips, WN_COPY_Tree(wn_loop_stride));
		
		wn_lower = acc_loop_lower_bound[i];
		wn_tmp_value = WN_Binary(OPR_ADD, acc_collapse_mtype, 
							wn_tmp_value, WN_COPY_Tree(wn_lower));
		WN* wn_store_org_index = Gen_ACC_Store(acc_index_st, 0, wn_tmp_value, FALSE);
		WN_INSERT_BlockLast(wn_indices_init_block, wn_store_org_index);
		if(i < (collapse_count-1))
		{
			wn_tmp_value = WN_Binary(OPR_MOD, acc_collapse_mtype, 
								WN_COPY_Tree(wn_tmp_trips), WN_COPY_Tree(wn_loop_stride));
			WN* wn_store_next_tmp_index = Gen_ACC_Store(st_nexttrip, 0, wn_tmp_value, FALSE);
			WN_INSERT_BlockLast(wn_indices_init_block, wn_store_next_tmp_index);
		}
	}

	WN* wn_newcond = WN_Relational (OPR_LT, TY_mtype(ST_type(st_newindex)), 
								WN_COPY_Tree(wn_new_loop_idx), 
								WN_COPY_Tree(wn_newtrips));
	WN* wn_newinit = Gen_ACC_Store(st_newindex, 0, WN_Intconst(acc_collapse_mtype, 0), FALSE);
	WN* wn_newincr = WN_Binary(OPR_ADD, acc_collapse_mtype, 
						WN_COPY_Tree(wn_new_loop_idx), WN_Intconst(acc_collapse_mtype, 1));
	wn_newincr = Gen_ACC_Store(st_newindex, 0, wn_newincr, FALSE);
	
	//WN* wn_loopidame = WN_CreateIdname(0, st_newindex);

	acc_loop_lower_bound.clear();
	acc_loop_upper_bound.clear();
	acc_loop_ntrips.clear();
	acc_base_node.clear();
	acc_limit_node.clear();
	acc_forloop_index_st.clear();
	acc_forloop_index_type.clear();
	
	acc_base_node.push_back(WN_Intconst(acc_collapse_mtype, 0));
	acc_limit_node.push_back(wn_newtrips);
	acc_forloop_index_st.push_back(st_newindex);
	acc_forloop_index_type.push_back(acc_collapse_mtype);
	acc_test_stmt = wn_newcond;

}

