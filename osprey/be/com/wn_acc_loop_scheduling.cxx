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

//Recognize all loop scheduling clause
typedef struct ACC_Loop_Scheduling_Identifier
{
	ACC_LOOP_TYPE looptype;
  	WN* wn_gang;
	BOOL bCreateGang;
	WN* wn_worker;
	BOOL bCreateWorker;
  	WN* wn_vector;	
	BOOL bCreateVector;
	WN* pragma_block;
	WN* num_gangs;
	WN* num_vectors;
	BOOL bIdentified;
}ACC_Loop_Scheduling_Identifier;

static vector<ACC_Loop_Scheduling_Identifier> acc_loop_recog_info;
static UINT32 acc_nested_loop_depth = 0;
static UINT32 acc_aligned_depth = 0;
static vector<ACC_Loop_Scheduling_Identifier> acc_toplogy_setup;

static void ACC_Copy_Loop_Scheduling_Idenfitier(ACC_Loop_Scheduling_Identifier* pDst, ACC_Loop_Scheduling_Identifier* pSrc)
{
	//level 1
	if(pSrc->wn_gang)
	{
		if(pDst->wn_gang)
		{
			WN_pragma(pDst->wn_gang) = WN_pragma(pSrc->wn_gang);
			pDst->bCreateGang = FALSE;
		}
		else
		{
			WN_PRAGMA_ID pragma_id = (WN_PRAGMA_ID)WN_pragma(pSrc->wn_gang);
			pDst->wn_gang = WN_CreatePragma(pragma_id, (ST_IDX)NULL, 0, 0);
			pDst->bCreateGang = TRUE;
		}						
	}
	else
		pDst->wn_gang = NULL;

	if(pSrc->wn_vector)
	{
		if(pDst->wn_vector)
		{						
			pDst->bCreateVector = FALSE;
			WN_pragma(pDst->wn_vector) = WN_pragma(pSrc->wn_vector);
		}
		else
		{
			WN_PRAGMA_ID pragma_id = (WN_PRAGMA_ID)WN_pragma(pSrc->wn_vector);
			pDst->wn_vector = WN_CreatePragma(pragma_id, (ST_IDX)NULL, 0, 0);
			pDst->bCreateVector = TRUE;						
		}					
	}
	else
		pDst->wn_vector = NULL;
}

/*****************************************************************
kernels loop follows this pattern, everytime, it only includes one top-level loop
for()
{
	...
	for(...){}
	...
	for(...){}
	...
}
//the flow control is allowed inside the for loop
for()
{
	...
	if(...)
	{
		...
		for(...){}		//after parsing this loop, the topology(GPU threads arch) of GPUs is finished, 
					//the following nested loops have to follow this topologyx.
		...
	}
	else [if(...)]
	{
		...
		for(...){}
		...
	}
	...
}
******************************************************************/
static BOOL Identify_ACC_Kernels_Loop_Scheduling ( WN * tree, WN* replace_block)
{
  OPCODE op;
  OPERATOR opr;
  INT32 i;
  WN *r;
  WN *temp;
  ST *old_sym;
  WN_OFFSET old_offset;
  ACC_LOOP_TYPE looptype = ACC_NONE_SPECIFIED;
  WN* wn_gang = NULL;
  WN* wn_vector = NULL;
  BOOL hasLoopInside = FALSE;
  BOOL temp_hasLoopInside = FALSE;

  /* Ignore NULL subtrees. */

  if (tree == NULL)
    return FALSE;

  /* Initialization. */

  op = WN_opcode(tree);
  opr = OPCODE_operator(op);

  if ((WN_opcode(tree) == OPC_REGION &&
			(WN_region_kind(tree) == REGION_KIND_ACC) &&
						   WN_first(WN_region_pragmas(tree)) &&
						   (WN_opcode(WN_first(WN_region_pragmas(tree))) == OPC_PRAGMA
						   || WN_opcode(WN_first(WN_region_pragmas(tree))) == OPC_XPRAGMA) ) 
			  && WN_pragma(WN_first(WN_region_pragmas(tree))) == WN_PRAGMA_ACC_LOOP_BEGIN)
  {
  	WN* pragma_block = WN_region_pragmas(tree);
    WN* cur_node = WN_first(pragma_block);
  	WN* next_node = WN_next(cur_node);
	BOOL isSeq = FALSE;
	WN* wn_region_bdy = WN_region_body(tree);
	WN* num_gangs = NULL;
	WN* num_vectors = NULL;
	WN* wn_gang = NULL;
	WN* wn_vector = NULL;
	//in default, assume there is no loop scheduling clauses
	BOOL bCreateGang = TRUE;
	BOOL bCreateVector = TRUE;
	acc_nested_loop_depth ++;

  	while ((cur_node = next_node)) 
  	{
    	next_node = WN_next(cur_node);

    	if (((WN_opcode(cur_node) == OPC_PRAGMA) ||
         (WN_opcode(cur_node) == OPC_XPRAGMA)) &&
        (WN_pragmas[WN_pragma(cur_node)].users & PUSER_ACC)) 
    	{
			switch (WN_pragma(cur_node)) 
			{

			  case WN_PRAGMA_ACC_CLAUSE_GANG:
				if(looptype == ACC_VECTOR)
					looptype = ACC_GANG_VECTOR;
				else
					looptype = ACC_GANG;
				wn_gang = cur_node;
				num_gangs = WN_kid0(cur_node);
				bCreateGang = FALSE;
			    break;  

			  case WN_PRAGMA_ACC_CLAUSE_VECTOR:
				if(looptype == ACC_GANG)
					looptype = ACC_GANG_VECTOR;
				else
					looptype = ACC_VECTOR;
				wn_vector = cur_node;
				num_vectors = WN_kid0(cur_node);
				bCreateVector = FALSE;
			    break;  
				
			  default:
		        break;
			}
  		} 
 	 }  
	 ACC_Loop_Scheduling_Identifier acc_loop_scheduling_identifier;
	 acc_loop_scheduling_identifier.looptype = looptype;
	 acc_loop_scheduling_identifier.wn_gang = wn_gang;
	 acc_loop_scheduling_identifier.wn_vector = wn_vector;
	 acc_loop_scheduling_identifier.num_gangs = num_gangs;
	 acc_loop_scheduling_identifier.num_vectors = num_vectors;
	 acc_loop_scheduling_identifier.bCreateGang = bCreateGang;
	 acc_loop_scheduling_identifier.bCreateVector = bCreateVector;
	 acc_loop_scheduling_identifier.pragma_block = pragma_block;
	 acc_loop_scheduling_identifier.bIdentified = FALSE;
	 acc_loop_recog_info.push_back(acc_loop_scheduling_identifier);

	 if(acc_loop_recog_info.size()>=3)
 	 {
 	 	//no necessary to search more
 	 	ACC_Loop_Scheduling_Identifier acc_loop_identifier0 = acc_loop_recog_info[0];
		ACC_Loop_Scheduling_Identifier acc_loop_identifier1 = acc_loop_recog_info[1];
		ACC_Loop_Scheduling_Identifier acc_loop_identifier2 = acc_loop_recog_info[2];
		//different topology setup in one offload region. This is not allowed
		if(acc_aligned_depth != 0 && acc_aligned_depth != 3)
			Fail_FmtAssertion ("All of the nested loops have to be with same depth");
		if(acc_aligned_depth == 0)
			acc_aligned_depth = 3;
		//WN_PRAGMA_ACC_CLAUSE_TOPLEVEL_LOOP
		WN* wn_innest_loop_pragma = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_INNEST_LOOP, 
											(ST_IDX)NULL, 0, 0);
		WN_INSERT_BlockLast(WN_region_pragmas(tree), wn_innest_loop_pragma);
		
		if(!acc_toplogy_setup.empty())
		{
			//the topology is already setup. So there is no necessary to be identified one by one.
			//just copy the previous one.
			if(acc_loop_identifier0.bIdentified == FALSE)
			{
				ACC_Loop_Scheduling_Identifier acc_loop_identifier0_setup = acc_toplogy_setup[0];
				ACC_Copy_Loop_Scheduling_Idenfitier(&acc_loop_identifier0, &acc_loop_identifier0_setup);				
			}
			if(acc_loop_identifier1.bIdentified == FALSE)
			{
				ACC_Loop_Scheduling_Identifier acc_loop_identifier1_setup = acc_toplogy_setup[1];
				ACC_Copy_Loop_Scheduling_Idenfitier(&acc_loop_identifier1, &acc_loop_identifier1_setup);				
			}
			if(acc_loop_identifier2.bIdentified == FALSE)
			{
				ACC_Loop_Scheduling_Identifier acc_loop_identifier2_setup = acc_toplogy_setup[2];
				ACC_Copy_Loop_Scheduling_Idenfitier(&acc_loop_identifier2, &acc_loop_identifier2_setup);
				if(acc_loop_identifier2.bCreateGang && acc_loop_identifier2.wn_gang)
					WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier2.wn_gang);
				if(acc_loop_identifier2.bCreateVector && acc_loop_identifier2.wn_vector)
					WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier2.wn_vector);
			}
		}
		else
		{
			if(acc_loop_identifier0.looptype == ACC_GANG_VECTOR)
			{
				// level0: gangz vectorz / gangy vectory	/	gangx  vectorx	/
				// level1: gangy vectory / gangx vectorx	/	gangy  vectory	/
				// level2: gangx vectorx /	vectorz			/	gangz			/
				WN* wn_node;
				if(acc_loop_identifier1.looptype == ACC_GANG_VECTOR && acc_loop_identifier2.looptype == ACC_GANG_VECTOR)
				{
					WN_pragma(acc_loop_identifier0.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_Z;
					WN_pragma(acc_loop_identifier0.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Z;
					WN_pragma(acc_loop_identifier1.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_Y;
					WN_pragma(acc_loop_identifier1.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;
					WN_pragma(acc_loop_identifier2.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
					WN_pragma(acc_loop_identifier2.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
					//setup topology
					//set gangs
					WN* wnGangsNumExpr = WN_kid0(acc_loop_identifier0.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_Z(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					wnGangsNumExpr = WN_kid0(acc_loop_identifier1.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_Y(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					wnGangsNumExpr = WN_kid0(acc_loop_identifier2.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_X(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					//set vector
					WN* wnVectorsNumExpr = WN_kid0(acc_loop_identifier0.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_Z(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
					wnVectorsNumExpr = WN_kid0(acc_loop_identifier1.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_Y(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
					wnVectorsNumExpr = WN_kid0(acc_loop_identifier2.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_X(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
				}
				else if(acc_loop_identifier1.looptype == ACC_GANG_VECTOR && acc_loop_identifier2.looptype == ACC_VECTOR)
				{
					WN_pragma(acc_loop_identifier0.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_Y;
					WN_pragma(acc_loop_identifier0.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;
					WN_pragma(acc_loop_identifier1.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
					WN_pragma(acc_loop_identifier1.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
					WN_pragma(acc_loop_identifier2.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Z;
					//setup topology
					//set gangs
					WN* wnGangsNumExpr = WN_kid0(acc_loop_identifier0.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_Y(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					wnGangsNumExpr = WN_kid0(acc_loop_identifier1.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_X(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					//set vector
					WN* wnVectorsNumExpr = WN_kid0(acc_loop_identifier0.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_Y(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
					wnVectorsNumExpr = WN_kid0(acc_loop_identifier1.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_X(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
					wnVectorsNumExpr = WN_kid0(acc_loop_identifier2.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_Z(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
				}
				else //if(acc_loop_identifier1.looptype == ACC_GANG_VECTOR && acc_loop_identifier2.looptype == ACC_GANG)
				{			
					//level 0
					WN_pragma(acc_loop_identifier0.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
					WN_pragma(acc_loop_identifier0.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
					//level 1
					acc_loop_identifier1.looptype = ACC_GANG_VECTOR;
					if(acc_loop_identifier1.wn_gang)
						WN_pragma(acc_loop_identifier1.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_Y;
					else
					{						
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_GANG_Y, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);			
						acc_loop_identifier1.wn_gang = wn_node;
					}
					if(acc_loop_identifier1.wn_vector)
						WN_pragma(acc_loop_identifier1.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;
					else
					{								
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_Y, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);	
						acc_loop_identifier1.wn_vector = wn_node;
					}
					//level 2
					acc_loop_identifier2.looptype = ACC_GANG;
					if(acc_loop_identifier2.wn_gang)
					{
						WN_pragma(acc_loop_identifier2.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_Z;
					}
					else
					{				
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_GANG_Z, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);					
						acc_loop_identifier2.wn_gang = wn_node;
						WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier2.wn_gang);
					}				
					if(acc_loop_identifier2.wn_vector)
						acc_loop_identifier2.wn_vector = NULL;
					//setup topology
					//set gangs
					WN* wnGangsNumExpr = WN_kid0(acc_loop_identifier0.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_X(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					wnGangsNumExpr = WN_kid0(acc_loop_identifier1.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_Y(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					wnGangsNumExpr = WN_kid0(acc_loop_identifier2.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_Z(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					//set vector
					WN* wnVectorsNumExpr = WN_kid0(acc_loop_identifier0.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_X(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
					wnVectorsNumExpr = WN_kid0(acc_loop_identifier1.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_Y(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
					//WN_pragma(acc_loop_identifier2.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
				} 
			}
			if(acc_loop_identifier0.looptype == ACC_VECTOR)
			{
				// level0: vectorx
				// level1: gangy vectory / gangx
				// level2: gangx		 / gangy vectory
				WN* wn_node;
				//level 0
				WN_pragma(acc_loop_identifier0.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
				if(acc_loop_identifier1.looptype == ACC_GANG_VECTOR && acc_loop_identifier2.looptype == ACC_GANG)
				{
					WN_pragma(acc_loop_identifier1.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_Y;
					WN_pragma(acc_loop_identifier1.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;
					WN_pragma(acc_loop_identifier2.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
					//setup topology
					//set gangs
					WN* wnGangsNumExpr = WN_kid0(acc_loop_identifier1.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_Y(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					wnGangsNumExpr = WN_kid0(acc_loop_identifier2.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_X(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					//set vector
					WN* wnVectorsNumExpr = WN_kid0(acc_loop_identifier0.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_X(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
					wnVectorsNumExpr = WN_kid0(acc_loop_identifier1.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_Y(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
				}
				else //if(acc_loop_identifier1.looptype == ACC_GANG && acc_loop_identifier2.looptype == ACC_GANG_VECTOR)
				{			
					//level 1
					acc_loop_identifier1.looptype = ACC_GANG;
					if(acc_loop_identifier1.wn_gang)
						WN_pragma(acc_loop_identifier1.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
					else
					{
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_GANG_X, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);			
						acc_loop_identifier1.wn_gang = wn_node;
					}
					//level 2
					acc_loop_identifier2.looptype = ACC_GANG_VECTOR;
					if(acc_loop_identifier2.wn_gang)
					{
						WN_pragma(acc_loop_identifier2.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_Y;
					}
					else
					{		
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_GANG_Y, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);				
						acc_loop_identifier2.wn_gang = wn_node;
						WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier2.wn_gang);
					}
					if(acc_loop_identifier2.wn_vector)
						WN_pragma(acc_loop_identifier2.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;
					else
					{			
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_Y, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);						
						acc_loop_identifier2.wn_vector = wn_node;
						WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier2.wn_vector);
					}
					//setup topology
					//set gangs
					WN* wnGangsNumExpr = WN_kid0(acc_loop_identifier1.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_X(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					wnGangsNumExpr = WN_kid0(acc_loop_identifier2.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_Y(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					//set vector
					WN* wnVectorsNumExpr = WN_kid0(acc_loop_identifier0.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_X(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
					wnVectorsNumExpr = WN_kid0(acc_loop_identifier2.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_Y(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
					//WN_pragma(acc_loop_identifier2.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
				} 				
			}
			else
			{
				// level0: gangx
				// level1: gangy vectory
				// level2: vectorx
				WN* wn_node;
				//level 0
				acc_loop_identifier0.looptype = ACC_GANG;
				if(acc_loop_identifier0.wn_gang)
					WN_pragma(acc_loop_identifier0.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
				else
				{
					wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_GANG_X, (ST*)NULL, 1);	
					WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);
					acc_loop_identifier0.wn_gang = wn_node; 
				}
				if(acc_loop_identifier0.wn_vector)
					acc_loop_identifier0.wn_vector = NULL;
				/////////////////////////////////////////////////////////////////////////////
				//level 1
				acc_loop_identifier1.looptype = ACC_GANG_VECTOR;
				if(acc_loop_identifier1.wn_gang)
					WN_pragma(acc_loop_identifier1.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_Y;
				else
				{
					wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_GANG_Y, (ST*)NULL, 1);	
					WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);
					acc_loop_identifier1.wn_gang = wn_node; 	
				}

				if(acc_loop_identifier1.wn_vector)
					WN_pragma(acc_loop_identifier1.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;
				else
				{
					wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_Y, (ST*)NULL, 1);	
					WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);	
					acc_loop_identifier1.wn_vector = wn_node;
				}
				
				/////////////////////////////////////////////////////////////////////////////
				//level 2
				acc_loop_identifier1.looptype = ACC_VECTOR;
				if(acc_loop_identifier2.wn_gang)
					acc_loop_identifier2.wn_gang = NULL;
				if(acc_loop_identifier2.wn_vector)
					WN_pragma(acc_loop_identifier2.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
				else
				{		
					wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_X, (ST*)NULL, 1);	
					WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 128);
					acc_loop_identifier2.wn_vector = wn_node;
					WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier2.wn_vector);
				}
				//setup topology
				//set gangs
				WN* wnGangsNumExpr = WN_kid0(acc_loop_identifier0.wn_gang);
				if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
						&& WN_const_val(wnGangsNumExpr)>1) 
						|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
				{
					WN* wn_gangs_set = Gen_Set_Gangs_Num_X(wnGangsNumExpr);
					WN_INSERT_BlockLast(replace_block, wn_gangs_set);
				}
				wnGangsNumExpr = WN_kid0(acc_loop_identifier1.wn_gang);
				if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
						&& WN_const_val(wnGangsNumExpr)>1) 
						|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
				{
					WN* wn_gangs_set = Gen_Set_Gangs_Num_Y(wnGangsNumExpr);
					WN_INSERT_BlockLast(replace_block, wn_gangs_set);
				}
				//set vector
				WN* wnVectorsNumExpr = WN_kid0(acc_loop_identifier1.wn_vector);
				if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
						&& WN_const_val(wnVectorsNumExpr)>1) 
						|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
				{
					WN* wn_vector_sets = Gen_Set_Vector_Num_Y(wnVectorsNumExpr);
					WN_INSERT_BlockLast(replace_block, wn_vector_sets);
				}
				wnVectorsNumExpr = WN_kid0(acc_loop_identifier2.wn_vector);
				if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
						&& WN_const_val(wnVectorsNumExpr)>1) 
						|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
				{
					WN* wn_vector_sets = Gen_Set_Vector_Num_X(wnVectorsNumExpr);
					WN_INSERT_BlockLast(replace_block, wn_vector_sets);
				}		
			}
		}
		acc_loop_identifier0.bIdentified =TRUE;
		acc_loop_identifier1.bIdentified =TRUE;
		acc_loop_identifier2.bIdentified =TRUE;
		acc_loop_recog_info[0] = acc_loop_identifier0;
		acc_loop_recog_info[1] = acc_loop_identifier1;
		acc_loop_recog_info[2] = acc_loop_identifier2;
		if(acc_toplogy_setup.empty())
			acc_toplogy_setup = acc_loop_recog_info;
 	 }
	 else
	 {
	 	//for the nested loop depth are less or equal to 2, keep seeking more loopnest level
	 	//hasLoopInside = hasLoopInside || 
	 	//				Identify_ACC_Kernels_Loop_Scheduling (wn_region_bdy, replace_block);
	 	temp_hasLoopInside = Identify_ACC_Kernels_Loop_Scheduling (wn_region_bdy, replace_block);
        hasLoopInside = temp_hasLoopInside || hasLoopInside;
		//if there is no more loops
		if(hasLoopInside == FALSE)
		{
			//identify the innest loop
			WN* wn_innest_loop_pragma = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_INNEST_LOOP, 
												(ST_IDX)NULL, 0, 0);
			WN_INSERT_BlockLast(WN_region_pragmas(tree), wn_innest_loop_pragma);
			if(acc_loop_recog_info.size() == 1)
	 		{
	 			ACC_Loop_Scheduling_Identifier acc_loop_identifier0 = acc_loop_recog_info[0];
				//different topology setup in one offload region. This is not allowed
				if(acc_aligned_depth != 0 && acc_aligned_depth != 1)
					Fail_FmtAssertion ("All of the nested loops have to be with same depth");
				if(acc_aligned_depth == 0)
					acc_aligned_depth = 1;
				WN* wn_node;
				if(!acc_toplogy_setup.empty())
				{
					if(acc_loop_identifier0.bIdentified == FALSE)
					{
						ACC_Loop_Scheduling_Identifier acc_loop_identifier0_setup = acc_toplogy_setup[0];
						ACC_Copy_Loop_Scheduling_Idenfitier(&acc_loop_identifier0, &acc_loop_identifier0_setup);
						if(acc_loop_identifier0.bCreateGang && acc_loop_identifier0.wn_gang)
							WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier0.wn_gang);
						if(acc_loop_identifier0.bCreateVector && acc_loop_identifier0.wn_vector)
							WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier0.wn_vector);
					}
				}
				else
				{
					if(acc_loop_identifier0.wn_gang)
						WN_pragma(acc_loop_identifier0.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
					else
					{						
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_GANG_X, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);
						acc_loop_identifier0.wn_gang = wn_node;
						WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier0.wn_gang);
						acc_loop_identifier0.bCreateGang = TRUE;
					}
					///////////////////////////////////////////////////////////////////////////////
					if(acc_loop_identifier0.wn_vector)
						WN_pragma(acc_loop_identifier0.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
					else
					{						
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_X, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 128);
						acc_loop_identifier0.wn_vector = wn_node;
						WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier0.wn_vector);
						acc_loop_identifier0.bCreateVector = TRUE;
					}		
					acc_loop_identifier0.looptype = ACC_GANG_VECTOR;
					
					//setup topology
					//set gangs
					WN* wnGangsNumExpr = WN_kid0(acc_loop_identifier0.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_X(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					//set vector
					WN* wnVectorsNumExpr = WN_kid0(acc_loop_identifier0.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_X(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
				}

				
				WN* wn_outer_loop_pragma = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_TOPLEVEL_LOOP, 
													(ST_IDX)NULL, 0, 0);
				WN_INSERT_BlockLast(WN_region_pragmas(tree), wn_outer_loop_pragma);
					
				acc_loop_identifier0.bIdentified =TRUE;
				acc_loop_recog_info[0] = acc_loop_identifier0;
				if(acc_toplogy_setup.empty())
					acc_toplogy_setup = acc_loop_recog_info;
	 		}
			else if(acc_loop_recog_info.size() == 2)
			{
	 			ACC_Loop_Scheduling_Identifier acc_loop_identifier0 = acc_loop_recog_info[0];
	 			ACC_Loop_Scheduling_Identifier acc_loop_identifier1 = acc_loop_recog_info[1];
				//different topology setup in one offload region. This is not allowed
				if(acc_aligned_depth != 0 && acc_aligned_depth != 2)
					Fail_FmtAssertion ("All of the nested loops have to be with same depth");
				if(acc_aligned_depth == 0)
					acc_aligned_depth = 2;

				//in case, the previous top-level loop has been set
				//check the previous loop type
				if(!acc_toplogy_setup.empty())
				{
					if(acc_loop_identifier0.bIdentified == FALSE)
					{
						ACC_Loop_Scheduling_Identifier acc_loop_identifier0_setup = acc_toplogy_setup[0];
						ACC_Copy_Loop_Scheduling_Idenfitier(&acc_loop_identifier0, &acc_loop_identifier0_setup);
						ACC_Loop_Scheduling_Identifier acc_loop_identifier1_setup = acc_toplogy_setup[1];
						ACC_Copy_Loop_Scheduling_Idenfitier(&acc_loop_identifier1, &acc_loop_identifier1_setup);
						if(acc_loop_identifier1.bCreateGang && acc_loop_identifier1.wn_gang)
							WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier1.wn_gang);
						if(acc_loop_identifier1.bCreateVector && acc_loop_identifier1.wn_vector)
							WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier1.wn_vector);
					}
				}
				else
				{
					WN* wn_node;
					//level 0
					if(acc_loop_identifier0.wn_gang)
						WN_pragma(acc_loop_identifier0.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_Y;
					else
					{
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_GANG_Y, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);
						acc_loop_identifier0.wn_gang = wn_node;
						acc_loop_identifier0.bCreateGang = TRUE;
					}

					if(acc_loop_identifier0.wn_vector)
						WN_pragma(acc_loop_identifier0.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;
					else
					{
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_Y, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);
						acc_loop_identifier0.wn_vector = wn_node;
						acc_loop_identifier0.bCreateVector = TRUE;
					}
					//level 1
					if(acc_loop_identifier1.wn_gang)
						WN_pragma(acc_loop_identifier1.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
					else
					{
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_GANG_X, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 1);
						acc_loop_identifier1.wn_gang = wn_node;
						WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier1.wn_gang);
						acc_loop_identifier1.bCreateGang = TRUE;
					}

					if(acc_loop_identifier1.wn_vector)
						WN_pragma(acc_loop_identifier1.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
					else
					{
						wn_node = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_X, (ST*)NULL, 1);	
						WN_kid0(wn_node) = WN_Intconst(MTYPE_U4, 64);
						acc_loop_identifier1.wn_vector = wn_node;
						WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier1.wn_vector);
						acc_loop_identifier1.bCreateVector = TRUE;
					}
					
					//setup topology
					//set gangs
					WN* wnGangsNumExpr = WN_kid0(acc_loop_identifier0.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_Y(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					wnGangsNumExpr = WN_kid0(acc_loop_identifier1.wn_gang);
					if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnGangsNumExpr)>1) 
							|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
					{
						WN* wn_gangs_set = Gen_Set_Gangs_Num_X(wnGangsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_gangs_set);
					}
					//set vector
					WN* wnVectorsNumExpr = WN_kid0(acc_loop_identifier0.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_Y(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
					wnVectorsNumExpr = WN_kid0(acc_loop_identifier1.wn_vector);
					if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
							&& WN_const_val(wnVectorsNumExpr)>1) 
							|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
					{
						WN* wn_vector_sets = Gen_Set_Vector_Num_X(wnVectorsNumExpr);
						WN_INSERT_BlockLast(replace_block, wn_vector_sets);
					}
				}
				acc_loop_identifier0.bIdentified =TRUE;
				acc_loop_identifier1.bIdentified =TRUE;
				acc_loop_recog_info[0] = acc_loop_identifier0;
	 			acc_loop_recog_info[1] = acc_loop_identifier1;
				if(acc_toplogy_setup.empty())
					acc_toplogy_setup = acc_loop_recog_info;
			}
		}
		else //if there is some loop inside, the loop is identified inside.
		{
			ACC_Loop_Scheduling_Identifier acc_loop_identifier = 
									acc_loop_recog_info[acc_nested_loop_depth-1];
			if(acc_loop_identifier.bCreateGang && acc_loop_identifier.wn_gang)
					WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier.wn_gang);
			if(acc_loop_identifier.bCreateVector && acc_loop_identifier.wn_vector)
					WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier.wn_vector);

			if(acc_nested_loop_depth == 1)
			{
				WN* wn_outer_loop_pragma = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_TOPLEVEL_LOOP, (ST_IDX)NULL, 0, 0);
				WN_INSERT_BlockLast(WN_region_pragmas(tree), wn_outer_loop_pragma);
			}
		}
	 }
	 
	 acc_loop_recog_info.pop_back();
	 acc_nested_loop_depth --;
	 return TRUE;
  }  
  else if (op == OPC_BLOCK) 
  {
    r = WN_first(tree);
    while (r) 
	{ // localize each node in block
      //hasLoopInside = hasLoopInside || Identify_ACC_Kernels_Loop_Scheduling (r, replace_block);
      temp_hasLoopInside = Identify_ACC_Kernels_Loop_Scheduling (r, replace_block);
      hasLoopInside = temp_hasLoopInside || hasLoopInside;
      r = WN_next(r);
      
   }
  }
  else 
  {
    for (i=0; i < WN_kid_count(tree); i++)
    {
      //hasLoopInside = hasLoopInside || Identify_ACC_Kernels_Loop_Scheduling( WN_kid(tree, i), replace_block);
      temp_hasLoopInside = Identify_ACC_Kernels_Loop_Scheduling (WN_kid(tree, i), replace_block);
      hasLoopInside = temp_hasLoopInside || hasLoopInside;
    }
  }
  return (hasLoopInside);
}


static BOOL Identify_ACC_Parallel_Loop_Scheduling ( WN * tree)
{
  OPCODE op;
  OPERATOR opr;
  INT32 i;
  WN *r;
  WN *temp;
  ST *old_sym;
  WN_OFFSET old_offset;
  ACC_LOOP_TYPE looptype = ACC_NONE_SPECIFIED;
  WN* wn_gang = NULL;
  WN* wn_worker = NULL;
  WN* wn_vector = NULL;
  //in default, assume there is no loop scheduling clauses
  BOOL bCreateGang = TRUE;
  BOOL bCreateWorker = TRUE;
  BOOL bCreateVector = TRUE;
  ////////////////////////////////////////////////////////
  BOOL hasLoopInside = FALSE;
  BOOL temp_hasLoopInside = FALSE;

  /* Ignore NULL subtrees. */

  if (tree == NULL)
    return FALSE;

  /* Initialization. */

  op = WN_opcode(tree);
  opr = OPCODE_operator(op);

  if ((WN_opcode(tree) == OPC_REGION &&
			(WN_region_kind(tree) == REGION_KIND_ACC) &&
						   WN_first(WN_region_pragmas(tree)) &&
						   (WN_opcode(WN_first(WN_region_pragmas(tree))) == OPC_PRAGMA
						   || WN_opcode(WN_first(WN_region_pragmas(tree))) == OPC_XPRAGMA) ) 
			  && WN_pragma(WN_first(WN_region_pragmas(tree))) == WN_PRAGMA_ACC_LOOP_BEGIN)
  {
  	WN* pragma_block = WN_region_pragmas(tree);
    WN* cur_node = WN_first(pragma_block);
  	WN* next_node = WN_next(cur_node);
	BOOL isSeq = FALSE;
	WN* wn_region_bdy = WN_region_body(tree);
	acc_nested_loop_depth ++;

  	while ((cur_node = next_node)) 
  	{
    	next_node = WN_next(cur_node);

    	if (((WN_opcode(cur_node) == OPC_PRAGMA) ||
         (WN_opcode(cur_node) == OPC_XPRAGMA)) &&
        (WN_pragmas[WN_pragma(cur_node)].users & PUSER_ACC)) 
    	{
			switch (WN_pragma(cur_node)) 
			{

			  case WN_PRAGMA_ACC_CLAUSE_GANG:
				if(looptype == ACC_VECTOR)
					looptype = ACC_GANG_VECTOR;
				
				else if(looptype == ACC_WORKER)				
					looptype = ACC_GANG_WORKER;
				
				else if(looptype == ACC_WORKER_VECTOR)				
					looptype = ACC_GANG_WORKER_VECTOR;
				
				else
					looptype = ACC_GANG;
				wn_gang = cur_node;
				//there is no necessary to recreate 
				//new WN* node for gang level
				bCreateGang = FALSE;
			    break;  
				
			  case WN_PRAGMA_ACC_CLAUSE_WORKER:	
				if(looptype == ACC_VECTOR)				
					looptype = ACC_WORKER_VECTOR;
				else if(looptype == ACC_GANG)				
					looptype = ACC_GANG_WORKER;
				else if(looptype == ACC_GANG_VECTOR)				
					looptype = ACC_GANG_WORKER_VECTOR;
				else
					looptype = ACC_WORKER;
				wn_worker = cur_node;
				bCreateWorker = FALSE;
				break;

			  case WN_PRAGMA_ACC_CLAUSE_VECTOR:				
				if(looptype == ACC_GANG)
					looptype = ACC_GANG_VECTOR;
				else if(looptype == ACC_GANG_WORKER)				
					looptype = ACC_GANG_WORKER_VECTOR;
				else if(looptype == ACC_WORKER)				
					looptype = ACC_WORKER_VECTOR;
				else
					looptype = ACC_VECTOR;
				wn_vector = cur_node;
				bCreateVector = FALSE;
			    break;  
				
			  default:
		        break;
			}
  		} 
 	 }  
	 ACC_Loop_Scheduling_Identifier acc_loop_scheduling_identifier;
	 acc_loop_scheduling_identifier.looptype = looptype;
	 acc_loop_scheduling_identifier.wn_gang = wn_gang;
	 acc_loop_scheduling_identifier.wn_worker = wn_worker;
	 acc_loop_scheduling_identifier.wn_vector = wn_vector;
	 
	 acc_loop_scheduling_identifier.bCreateGang = bCreateGang;
	 acc_loop_scheduling_identifier.bCreateWorker = bCreateWorker;
	 acc_loop_scheduling_identifier.bCreateVector = bCreateVector;
	 
	 acc_loop_scheduling_identifier.pragma_block = pragma_block;
	 acc_loop_recog_info.push_back(acc_loop_scheduling_identifier);

	 if(acc_loop_recog_info.size()>=3)
 	 {
 	 	//no necessary to search more
 	 	ACC_Loop_Scheduling_Identifier acc_loop_identifier0 = acc_loop_recog_info[0];
		ACC_Loop_Scheduling_Identifier acc_loop_identifier1 = acc_loop_recog_info[1];
		ACC_Loop_Scheduling_Identifier acc_loop_identifier2 = acc_loop_recog_info[2];
		//different topology setup in one offload region. This is not allowed
		if(acc_aligned_depth != 0 && acc_aligned_depth != 3)
			Fail_FmtAssertion ("All of the nested loops have to be with same depth");
		/////////////////////////////////////////////////////////////////////////////
		if(acc_aligned_depth == 0)
			acc_aligned_depth = 3;
		WN* wn_node;
		if(acc_loop_identifier0.wn_gang)
		{
			WN_pragma(acc_loop_identifier0.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
		}
		else
		{
			wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_GANG_X, (ST_IDX)NULL, 0, 0);
			acc_loop_identifier0.wn_gang = wn_node;			
		}

		if(acc_loop_identifier1.wn_worker)
		{
			WN_pragma(acc_loop_identifier1.wn_worker) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;
		}
		else			
		{
			wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_Y, (ST_IDX)NULL, 0, 0);
			acc_loop_identifier1.wn_worker = wn_node;
		}
					
		if(acc_loop_identifier2.wn_vector)
			WN_pragma(acc_loop_identifier2.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
		else
		{
			wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_X, (ST_IDX)NULL, 0, 0);
			acc_loop_identifier2.wn_vector = wn_node;
			WN_INSERT_BlockLast(pragma_block, acc_loop_identifier2.wn_vector);
		}
		
		//WN_INSERT_BlockLast(pragma_block, acc_loop_identifier2.wn_vector);
		//WN_PRAGMA_ACC_CLAUSE_TOPLEVEL_LOOP
		WN* wn_innest_loop_pragma = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_INNEST_LOOP, 
											(ST_IDX)NULL, 0, 0);
		WN_INSERT_BlockLast(pragma_block, wn_innest_loop_pragma);
		acc_loop_recog_info[0] = acc_loop_identifier0;
		acc_loop_recog_info[1] = acc_loop_identifier1;
		acc_loop_recog_info[2] = acc_loop_identifier2;
 	 }
	 else
	 {
	 	//for the nested loop depth are less or equal to 2, keep seeking more loopnest level
	 	//hasLoopInside = hasLoopInside || Identify_ACC_Parallel_Loop_Scheduling (wn_region_bdy);
	    temp_hasLoopInside = Identify_ACC_Parallel_Loop_Scheduling (wn_region_bdy);  
	 	hasLoopInside = temp_hasLoopInside || hasLoopInside ;
		//if there is no more loops
		if(hasLoopInside == FALSE)
		{
			WN* wn_innest_loop_pragma = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_INNEST_LOOP, 
												(ST_IDX)NULL, 0, 0);
			WN_INSERT_BlockLast(pragma_block, wn_innest_loop_pragma);
			if(acc_loop_recog_info.size() == 1)
	 		{
	 			ACC_Loop_Scheduling_Identifier acc_loop_identifier0 = acc_loop_recog_info[0];
				//different topology setup in one offload region. This is not allowed
				if(acc_aligned_depth != 0 && acc_aligned_depth != 1)
					Fail_FmtAssertion ("All of the nested loops have to be with same depth");
				if(acc_aligned_depth == 0)
					acc_aligned_depth = 1;
				WN* wn_node;// = 
				if(acc_loop_identifier0.wn_gang)
				{
					WN_pragma(acc_loop_identifier0.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
				}
				else
				{
					wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_GANG_X, (ST_IDX)NULL, 0, 0);
					acc_loop_identifier0.wn_gang = wn_node;
					WN_INSERT_BlockLast(pragma_block, acc_loop_identifier0.wn_gang);
				}
				
				if(acc_loop_identifier0.wn_worker)
				{
					WN_pragma(acc_loop_identifier0.wn_worker) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;
				}
				else
				{
					wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_Y, (ST_IDX)NULL, 0, 0);
					acc_loop_identifier0.wn_worker = wn_node;
					WN_INSERT_BlockLast(pragma_block, acc_loop_identifier0.wn_worker);
				}

				if(acc_loop_identifier0.wn_vector)
				{
					WN_pragma(acc_loop_identifier0.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
				}
				else
				{
					wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_X, (ST_IDX)NULL, 0, 0);
					acc_loop_identifier0.wn_vector = wn_node;			
					WN_INSERT_BlockLast(pragma_block, acc_loop_identifier0.wn_vector);
				}
				
				acc_loop_recog_info[0] = acc_loop_identifier0;

				wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_Y, (ST_IDX)NULL, 0, 0);
				WN* wn_toplevel_loop_pragma = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_TOPLEVEL_LOOP, 
												(ST_IDX)NULL, 0, 0);
				WN_INSERT_BlockLast(pragma_block, wn_toplevel_loop_pragma);
	 		}
			else if(acc_loop_recog_info.size() == 2)
			{
	 			ACC_Loop_Scheduling_Identifier acc_loop_identifier0 = acc_loop_recog_info[0];
	 			ACC_Loop_Scheduling_Identifier acc_loop_identifier1 = acc_loop_recog_info[1];
				//different topology setup in one offload region. This is not allowed
				if(acc_aligned_depth != 0 && acc_aligned_depth != 2)
					Fail_FmtAssertion ("All of the nested loops have to be with same depth");
				if(acc_aligned_depth == 0)
					acc_aligned_depth = 2;
				if(acc_loop_identifier0.looptype == ACC_GANG_WORKER)
				{
					WN* wn_node;

					WN_pragma(acc_loop_identifier0.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
					WN_pragma(acc_loop_identifier0.wn_worker) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;

					if(acc_loop_identifier1.wn_vector)
						WN_pragma(acc_loop_identifier1.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
					else
					{
						wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_X, (ST_IDX)NULL, 0, 0);
						acc_loop_identifier1.wn_vector = wn_node;
						WN_INSERT_BlockLast(pragma_block, acc_loop_identifier1.wn_vector);
					}				
				}
				else //if(acc_loop_identifier0.looptype == ACC_GANG) 
					//&& acc_loop_identifier1.looptype == ACC_WORKER_VECTOR)
				{
					WN* wn_node;
					//top loop scheduling
					if(acc_loop_identifier0.wn_gang)
					{
						WN_pragma(acc_loop_identifier0.wn_gang) = WN_PRAGMA_ACC_CLAUSE_GANG_X;
					}
					else
					{						
						wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_GANG_X, (ST_IDX)NULL, 0, 0);
						acc_loop_identifier0.wn_gang = wn_node;
					}

					//second loop scheduling
					if(acc_loop_identifier1.wn_worker)
					{
						WN_pragma(acc_loop_identifier1.wn_worker) = WN_PRAGMA_ACC_CLAUSE_VECTOR_Y;
					}
					else
					{
						wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_Y, (ST_IDX)NULL, 0, 0);
						acc_loop_identifier1.wn_worker = wn_node;				
						WN_INSERT_BlockLast(pragma_block, acc_loop_identifier1.wn_worker);	
					}
					
					if(acc_loop_identifier1.wn_vector)
					{
						WN_pragma(acc_loop_identifier1.wn_vector) = WN_PRAGMA_ACC_CLAUSE_VECTOR_X;
					}
					else
					{
						wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_X, (ST_IDX)NULL, 0, 0);
						acc_loop_identifier1.wn_vector = wn_node;					
						WN_INSERT_BlockLast(pragma_block, acc_loop_identifier1.wn_vector);
					}			
					
				}
				/*else //if(acc_loop_identifier0.looptype == ACC_GANG_VECTOR&&
					 //	acc_loop_identifier1.looptype == ACC_GANG_VECTOR)
				{
					WN* wn_node;
					wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_GANG_X, (ST_IDX)NULL, 0, 0);
					acc_loop_identifier0.wn_gang = wn_node;
					
					wn_node = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_VECTOR_X, (ST_IDX)NULL, 0, 0);
					acc_loop_identifier1.wn_vector = wn_node;
					
					WN_INSERT_BlockLast(pragma_block, 
														acc_loop_identifier1.wn_vector);
				}*/
				acc_loop_recog_info[0] = acc_loop_identifier0;
	 			acc_loop_recog_info[1] = acc_loop_identifier1;
			}
		}	
		else //if there is some loop inside, the loop is identified inside.
		{
			ACC_Loop_Scheduling_Identifier acc_loop_identifier = 
									acc_loop_recog_info[acc_nested_loop_depth-1];
			if(acc_loop_identifier.bCreateGang && acc_loop_identifier.wn_gang)
				WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier.wn_gang);
			if(acc_loop_identifier.bCreateWorker && acc_loop_identifier.wn_worker)
				WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier.wn_worker);
			//There is no need to put this vector node which was already done
			//if(acc_loop_identifier.bCreateVector)
			//	WN_INSERT_BlockLast(WN_region_pragmas(tree), acc_loop_identifier.wn_vector);
			
			if(acc_nested_loop_depth==1)
			{	
				WN* wn_toplevel_loop_pragma = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_TOPLEVEL_LOOP, 
													(ST_IDX)NULL, 0, 0);
				WN_INSERT_BlockLast(WN_region_pragmas(tree), wn_toplevel_loop_pragma);
			}
		}	
	 }
	 
	 acc_loop_recog_info.pop_back();
	 acc_nested_loop_depth --;
	 return TRUE;
  }  
  else if (op == OPC_BLOCK) 
  {
    r = WN_first(tree);
    while (r) 
	{ // traverse each node in block
      //hasLoopInside = hasLoopInside || Identify_ACC_Parallel_Loop_Scheduling (r);
      temp_hasLoopInside = Identify_ACC_Parallel_Loop_Scheduling (r);  
      hasLoopInside = temp_hasLoopInside || hasLoopInside ;
      r = WN_next(r);      
    }
  }
  else 
  {
    for (i=0; i < WN_kid_count(tree); i++)
    {
      //hasLoopInside = hasLoopInside || Identify_ACC_Parallel_Loop_Scheduling( WN_kid(tree, i));
      temp_hasLoopInside = Identify_ACC_Parallel_Loop_Scheduling( WN_kid(tree, i));  
      hasLoopInside = temp_hasLoopInside || hasLoopInside; 
    }
  }
  return (hasLoopInside);
}

//this function does two things:
//1. identify the gang/work/vector and map them into the x/y/z dimensions of thread block and grid
//2. setup the threads topology when the loop scheduling is determined.
void Identify_Loop_Scheduling(WN* tree, WN* replace_block, BOOL bIsKernels)
{
	acc_loop_recog_info.clear();
	acc_toplogy_setup.clear();
	acc_nested_loop_depth = 0;
	acc_aligned_depth = 0;

	if(bIsKernels)
		Identify_ACC_Kernels_Loop_Scheduling(tree, replace_block);
	else
		Identify_ACC_Parallel_Loop_Scheduling(tree);
}

BOOL acc_set_gangs = FALSE;
BOOL acc_set_workers = FALSE;
BOOL acc_set_vector_length = FALSE;

void ACC_Setup_GPU_toplogy_for_Parallel(WN* wn_gangs, WN* wn_workers, 
 									WN* wn_vector_length, WN* replace_block)
{
	WN* toplogy = Gen_Set_Default_Toplogy();
	WN_INSERT_BlockLast(replace_block, toplogy);
	WN* gangs_num = wn_gangs;
	WN* workers_num = wn_workers;
	WN* vectors_length = wn_vector_length;
	WN* wnGangsNumExpr;
	WN* wnWorkersNumExpr;
	WN* wnVectorsNumExpr;
	if(gangs_num)
	{
		wnGangsNumExpr = WN_kid0(gangs_num);			
		if((WN_operator(wnGangsNumExpr) == OPR_INTCONST 
						&& WN_const_val(wnGangsNumExpr) != 0) 
						|| (WN_operator(wnGangsNumExpr) != OPR_INTCONST))
		{
			WN* wn_gangs_set = Gen_Set_Gangs_Num_X(wnGangsNumExpr);
			WN_INSERT_BlockLast(replace_block, wn_gangs_set);
			acc_set_gangs = TRUE;
		}
	}
	if(workers_num)
	{
		wnWorkersNumExpr = WN_kid0(workers_num);
		if((WN_operator(wnWorkersNumExpr) == OPR_INTCONST 
						&& WN_const_val(wnWorkersNumExpr) != 0 ) 
						|| (WN_operator(wnWorkersNumExpr) != OPR_INTCONST))
		{
			WN* wn_workers_set = Gen_Set_Vector_Num_Y(wnWorkersNumExpr);
			WN_INSERT_BlockLast(replace_block, wn_workers_set);
			acc_set_workers = TRUE;
		}		
	}
	if(vectors_length)
	{
		wnVectorsNumExpr = WN_kid0(vectors_length);
		if((WN_operator(wnVectorsNumExpr) == OPR_INTCONST 
						&& WN_const_val(wnVectorsNumExpr) >= 1 ) 
						|| (WN_operator(wnVectorsNumExpr) != OPR_INTCONST))
		{
			WN* wn_vectors_set = Gen_Set_Vector_Num_X(wnVectorsNumExpr);
			WN_INSERT_BlockLast(replace_block, wn_vectors_set);
			acc_set_vector_length = TRUE;
		}
	}
}


typedef enum acc_loop_gang_level
{
	ACC_LOOP_GANG_NONE,
	ACC_LOOP_GANG_X,
	ACC_LOOP_GANG_Y,
	ACC_LOOP_GANG_Z		
}acc_loop_gang_level;

typedef enum acc_loop_vector_level
{
	ACC_LOOP_VECTOR_NONE,
	ACC_LOOP_VECTOR_X,
	ACC_LOOP_VECTOR_Y,
	ACC_LOOP_VECTOR_Z,	
	ACC_LOOP_VECTOR_X_Y //only for parallel region: worker vector
}acc_loop_vector_level;

WN* ACC_Loop_Scheduling_Transformation_Gpu(WN* tree, WN* wn_replace_block)
{
  OPCODE op;
  OPERATOR opr;
  INT32 i;
  WN *r;
  WN *temp;
  acc_loop_gang_level gang_type = ACC_LOOP_GANG_NONE;
  acc_loop_vector_level vector_type = ACC_LOOP_VECTOR_NONE;
  vector<ACC_ReductionMap*> reductionmap;

  /* Ignore NULL subtrees. */

  if (tree == NULL)
    return (tree);

  /* Initialization. */

  op = WN_opcode(tree);
  opr = OPCODE_operator(op);
  acc_collapse_count = 1;
  
  if ((WN_opcode(tree) == OPC_REGION &&
			(WN_region_kind(tree) == REGION_KIND_ACC) &&
						   WN_first(WN_region_pragmas(tree)) &&
						   (WN_opcode(WN_first(WN_region_pragmas(tree))) == OPC_PRAGMA
						   || WN_opcode(WN_first(WN_region_pragmas(tree))) == OPC_XPRAGMA) ) 
			  && WN_pragma(WN_first(WN_region_pragmas(tree))) == WN_PRAGMA_ACC_LOOP_BEGIN)
  {
		WN* pragma_block = WN_region_pragmas(tree);
		WN* cur_node = WN_first(pragma_block);
		WN* next_node = WN_next(cur_node);
		WN* wn_region_bdy = WN_region_body(tree);
		BOOL bIsInnestLoop = FALSE;
		BOOL bIsTopLoop = FALSE;

	  	while ((cur_node = next_node)) 
	  	{
	    	next_node = WN_next(cur_node);

	    	if (((WN_opcode(cur_node) == OPC_PRAGMA) ||
	         (WN_opcode(cur_node) == OPC_XPRAGMA)) &&
	        (WN_pragmas[WN_pragma(cur_node)].users & PUSER_ACC)) 
	    	{
				switch (WN_pragma(cur_node)) 
				{				
				  case WN_PRAGMA_ACC_CLAUSE_COLLAPSE:
					 acc_collapse_count = WN_const_val(WN_kid0(cur_node));
					 break;
 
				  case WN_PRAGMA_ACC_CLAUSE_GANG_X:
				  	gang_type = ACC_LOOP_GANG_X;
				    break;  
					
				  case WN_PRAGMA_ACC_CLAUSE_GANG_Y:
				  	gang_type = ACC_LOOP_GANG_Y;
				    break;  
					
				  case WN_PRAGMA_ACC_CLAUSE_GANG_Z:
				  	gang_type = ACC_LOOP_GANG_Z;
				    break;  
					
				  case WN_PRAGMA_ACC_CLAUSE_VECTOR_X:
				  	if(vector_type == ACC_LOOP_VECTOR_Y)
						vector_type = ACC_LOOP_VECTOR_X_Y;
					else
						vector_type = ACC_LOOP_VECTOR_X;
					break;

				  case WN_PRAGMA_ACC_CLAUSE_VECTOR_Y:
				  	if(vector_type == ACC_LOOP_VECTOR_X)
						vector_type = ACC_LOOP_VECTOR_X_Y;
					else
						vector_type = ACC_LOOP_VECTOR_Y;
				    break;  

				  case WN_PRAGMA_ACC_CLAUSE_VECTOR_Z:
				  	vector_type = ACC_LOOP_VECTOR_Z;
				    break;  

				  case WN_PRAGMA_ACC_CLAUSE_REDUCTION:
				  {
					ACC_ReductionMap* pReductionMap = ACC_Get_ReductionMap(cur_node);

					ACC_ProcessReduction_Inside_offloadRegion(wn_replace_block, 
																pReductionMap);
					reductionmap.push_back(pReductionMap);
				  }
					break;
				  case WN_PRAGMA_ACC_CLAUSE_INNEST_LOOP:
				  	bIsInnestLoop = TRUE;
					break;
				  case WN_PRAGMA_ACC_CLAUSE_TOPLEVEL_LOOP:
				  	bIsTopLoop = TRUE;
					break;
					
				  default:
			        break;
				}
	  		} 
	 	 }  
		//////////////////////////////////////////////////////////////////////
		WN_DELETE_FromBlock (WN_region_pragmas(tree), cur_node);
		//////////////////////////////////////////////////////////////////////
		
		WN* wn_pdo_node;
		WN* wn_first_node = WN_first(wn_region_bdy);
		wn_pdo_node = wn_first_node;
		while(wn_pdo_node && WN_operator(wn_pdo_node) != OPR_DO_LOOP)
		{
			wn_pdo_node = WN_next(wn_pdo_node);
		}

		if(wn_pdo_node == NULL)
		{
			Fail_FmtAssertion ("Cannot find the DOLOOP statement in ACC LOOP.");
		}
	    WN* wn_prev_node = WN_prev(wn_pdo_node);//At this time, it's not necessary 
	    WN *code_before_pdo = NULL;
	    if (wn_prev_node) {
	        // add synchronization to sandwich code before PDO
	      code_before_pdo = WN_CreateBlock();
	      WN_EXTRACT_ItemsFromBlock(wn_region_bdy, wn_first_node, wn_prev_node);
	      WN_first(code_before_pdo) = wn_first_node;
	      WN_last(code_before_pdo) = wn_prev_node;
	      //WN_INSERT_BlockBefore(wn_region_bdy, wn_pdo_node, code_before_pdo);
	      //wn_prev_node = WN_prev(wn_pdo_node);
	    }
		
	    WN* wn_next_node = WN_next(wn_pdo_node);//At this time, it's not necessary
	    WN* code_after_pdo = NULL;
	    if (wn_next_node) {
	      WN* wn_last_node = WN_last(wn_region_bdy);
	      code_after_pdo = WN_CreateBlock();
	      WN_EXTRACT_ItemsFromBlock(wn_region_bdy, wn_next_node, wn_last_node);
	      WN_first(code_after_pdo) = next_node;
	      WN_last(code_after_pdo) = wn_last_node;
	    }
	    
		WN_EXTRACT_FromBlock(wn_region_bdy, wn_pdo_node);
		//////////////////////////////////////////////////////////////////////		
		//////////////////////////////////////////////////////////////////////
		ACC_Extract_Index_Info(wn_pdo_node);
		
		ACC_Extract_Do_Info ( wn_pdo_node );
		
		WN* wn_init;
		WN* wn_condition;
		WN* wn_incr;
		WN* wn_loopbody;
		WN* wn_precollapse_block = NULL;
		WN* wn_collapse_init_idx_blk = NULL;
		UINT32 this_acc_collapse_count = acc_collapse_count;
		ST * acc_index_st;
		if(acc_collapse_count > 1)
		{
			for(int i=0; i<acc_collapse_count; i++)
			{
				ST* st_tmp_idx = acc_forloop_index_st[i];
				acc_loop_index_var.push_back(st_tmp_idx);
			}
			wn_precollapse_block = WN_CreateBlock();
			wn_collapse_init_idx_blk = WN_CreateBlock();
			ACC_Rewrite_Collapsed_Do(wn_precollapse_block, wn_collapse_init_idx_blk);
			acc_index_st = acc_forloop_index_st[0];
		}
		else
		{			
			//it is for scalar replacement
			acc_index_st = acc_forloop_index_st[0];
			acc_loop_index_var.push_back(acc_index_st);
		}
		
		TYPE_ID acc_index_type = acc_forloop_index_type[0];
		wn_init = acc_base_node[0];
		wn_condition = acc_limit_node[0];
		wn_incr = acc_stride_node[0];
		wn_loopbody = acc_doloop_body;
		/////////////////////////////////////////////////////////////////
		//WN* wn_acc_loop_translated_body = WN_CreateBlock();
		//begin transformation
		/////////////////////////////////////////////////////////////////////
		//check the gang-level
		WN* wn_gang_id = NULL;
		WN* wn_gang_width = NULL;
	    WN* wn_index_init0 = NULL;
	    WN* wn_index_init1 = NULL;
	    WN* wn_index_init2 = NULL;
	    WN* wn_index_step0 = NULL;
	    WN* wn_index_step1 = NULL;
	    WN* wn_init_index_block = WN_CreateBlock();
		if(gang_type == ACC_LOOP_GANG_X)
		{
			wn_gang_id = WN_COPY_Tree(blockidx);
			wn_gang_width = WN_COPY_Tree(griddimx);
		}
		else if(gang_type == ACC_LOOP_GANG_Y)
		{
			wn_gang_id = WN_COPY_Tree(blockidy);
			wn_gang_width = WN_COPY_Tree(griddimy);
		}
		else if(gang_type == ACC_LOOP_GANG_Z)
		{
			wn_gang_id = WN_COPY_Tree(blockidz);
			wn_gang_width = WN_COPY_Tree(griddimz);
		}
		
		//check the vector level
		WN* wn_vector_id = NULL;
		WN* wn_vector_width = NULL;
		if(vector_type == ACC_LOOP_VECTOR_X)
		{
			wn_vector_id = WN_COPY_Tree(threadidx);
			wn_vector_width = WN_COPY_Tree(blockdimx);
		}
		else if(vector_type == ACC_LOOP_VECTOR_Y)
		{
			wn_vector_id = WN_COPY_Tree(threadidy);
			wn_vector_width = WN_COPY_Tree(blockdimy);
		}
		else if(vector_type == ACC_LOOP_VECTOR_Z)
		{
			wn_vector_id = WN_COPY_Tree(threadidz);
			wn_vector_width = WN_COPY_Tree(blockdimz);
		}
		else if(vector_type == ACC_LOOP_VECTOR_X_Y)
		{
			//all the expression is in 3 address code
			//vector id first
			char tmp_localname[256];
			{
				ST* st_new_tmp_1 = New_ST( CURRENT_SYMTAB );
		   		// = (char *) alloca(strlen(ST_name(acc_tmp_name_prefix))+10);
		   
		   		sprintf ( tmp_localname, "%s_vector_width_%d", acc_tmp_name_prefix, 
												kernel_tmp_variable_count);
		   		kernel_tmp_variable_count ++;

		   		ST_Init(st_new_tmp_1, Save_Str( tmp_localname), CLASS_VAR, 
		   				SCLASS_AUTO, EXPORT_LOCAL, Be_Type_Tbl(MTYPE_U4));
				///////////////////////////////////////////////////////////////////////
				wn_vector_id = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockDim_x)), 
		   									WN_COPY_Tree(threadidy), 
		   									WN_COPY_Tree(blockdimx));
				wn_vector_id = WN_Stid(TY_mtype(ST_type(st_new_tmp_1)), 0, 
		   									st_new_tmp_1, ST_type(st_new_tmp_1), 
		   									wn_vector_id);
				WN_INSERT_BlockLast(wn_init_index_block, wn_vector_id);
				wn_vector_id = WN_Ldid(TY_mtype(ST_type(st_new_tmp_1)), 0, 
		   										st_new_tmp_1, 
		   										ST_type(st_new_tmp_1));
				wn_vector_id = WN_Binary(OPR_ADD, 
		   									TY_mtype(ST_type(glbl_threadIdx_x)), 
		   									wn_vector_id, 
		   									WN_COPY_Tree(threadidx));
				wn_vector_id = WN_Stid(TY_mtype(ST_type(st_new_tmp_1)), 0, 
		   									st_new_tmp_1, ST_type(st_new_tmp_1), 
		   									wn_vector_id);
				WN_INSERT_BlockLast(wn_init_index_block, wn_vector_id);
				///////////////////////////////////////////////////////////////////////
				wn_vector_id = WN_Ldid(TY_mtype(ST_type(st_new_tmp_1)), 0, 
		   										st_new_tmp_1, 
		   										ST_type(st_new_tmp_1));
			}
			///////////////////////////////////////////////////////////////////////
			//setup the vector width
			{
				ST* st_new_tmp_2 = New_ST( CURRENT_SYMTAB );
		   
		   		sprintf ( tmp_localname, "%s_vector_width_%d", acc_tmp_name_prefix, 
												kernel_tmp_variable_count);
		   		kernel_tmp_variable_count ++;

		   		ST_Init(st_new_tmp_2, Save_Str( tmp_localname), CLASS_VAR, 
		   				SCLASS_AUTO, EXPORT_LOCAL, Be_Type_Tbl(MTYPE_U4));
				wn_vector_width = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockDim_x)), 
		   									WN_COPY_Tree(blockdimy), 
		   									WN_COPY_Tree(blockdimx));
				wn_vector_width = WN_Stid(TY_mtype(ST_type(st_new_tmp_2)), 0, 
		   									st_new_tmp_2, ST_type(st_new_tmp_2), 
		   									wn_vector_width);
				WN_INSERT_BlockLast(wn_init_index_block, wn_vector_width);
				///////////////////////////////////////////////////////////////////////
				wn_vector_width = WN_Ldid(TY_mtype(ST_type(st_new_tmp_2)), 0, 
		   										st_new_tmp_2, 
		   										ST_type(st_new_tmp_2));
			}
		}

		//inital value: gang_id * vector_width + vector_id
		//stride value: gang_width * vector_width
		//glbl_st_blockDim_x glbl_wn_blockIdx_x
	   //new temp variable for stride
	   ST* st_new_tmp = New_ST( CURRENT_SYMTAB );
	   char tmp_localname[256];// = (char *) alloca(strlen(ST_name(acc_tmp_name_prefix))+10);
	   
	   sprintf ( tmp_localname, "%s%d", acc_tmp_name_prefix, kernel_tmp_variable_count);
	   kernel_tmp_variable_count ++;

	   ST_Init(st_new_tmp, Save_Str( tmp_localname), CLASS_VAR, 
	   				SCLASS_AUTO, EXPORT_LOCAL, Be_Type_Tbl(MTYPE_U4));
	   WN* wn_index = WN_Ldid(TY_mtype(ST_type(acc_index_st)), 0, 
									acc_index_st, ST_type(acc_index_st));
	   //there is a special case in AMD platform
	   //when there is only one level loop 
	   //num_worker is not specified
	   //no reduction, because reduction algorithm uses local memory which is using local id to identify the data affinity
	   //we will use get_global_id to get the thread identification
	   if(reductionmap.size()==0 && acc_target_arch==ACC_ARCH_TYPE_APU && acc_set_workers==FALSE && bIsInnestLoop && bIsTopLoop)
	   {	   	
   			//init value			
			wn_index_init0 = WN_COPY_Tree(wn_threadid_gid_x);
			wn_index_init0 = WN_Binary(OPR_ADD, 
	   									TY_mtype(ST_type(acc_index_st)), 
	   									WN_COPY_Tree(wn_init), 
	   									wn_index_init0);
			wn_index_init0 = WN_Stid(TY_mtype(ST_type(acc_index_st)), 0, 
   										acc_index_st, ST_type(acc_index_st), 
   										wn_index_init0);
			//stride
			wn_index_step1 = WN_COPY_Tree(wn_thread_global_width);
			wn_index_step1 = WN_Binary(OPR_ADD, TY_mtype(ST_type(acc_index_st)), 
								WN_COPY_Tree(wn_index), wn_index_step1);
		    wn_index_step0 = WN_Stid(TY_mtype(ST_type(acc_index_st)), 0, 
							acc_index_st, ST_type(acc_index_st), wn_index_step1);
	   }
	   //if gang clause appears
	   else if(wn_gang_id)
	   {
	   		if(wn_vector_id)
	   		{
	   			//init value
	   			wn_index_init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockDim_x)), 
	   									WN_COPY_Tree(wn_gang_id), 
	   									WN_COPY_Tree(wn_vector_width));
				wn_index_init0 = WN_Stid(TY_mtype(ST_type(acc_index_st)), 0, 
	   									acc_index_st, ST_type(acc_index_st), 
	   									wn_index_init0);
				WN_INSERT_BlockLast(wn_init_index_block, wn_index_init0);
				///////////////////////////////////////////////////////////////////////////
				wn_index_init1 = WN_Binary(OPR_ADD, 
	   									TY_mtype(ST_type(acc_index_st)), 
	   									WN_COPY_Tree(wn_init), 
	   									WN_COPY_Tree(wn_vector_id));
				wn_index_init1 = WN_Binary(OPR_ADD, 
	   									TY_mtype(ST_type(acc_index_st)), 
	   									wn_index_init1, 
	   									WN_COPY_Tree(wn_index));
				wn_index_init1 = WN_Stid(TY_mtype(ST_type(acc_index_st)), 0, 
										acc_index_st, 
	   									ST_type(acc_index_st), wn_index_init1);
				//put it back to init0
				wn_index_init0 = wn_index_init1;
				//stride
				wn_index_step0 = WN_Binary(OPR_MPY, 
												TY_mtype(ST_type(glbl_blockDim_x)), 
												WN_COPY_Tree(wn_gang_width), 
												WN_COPY_Tree(wn_vector_width));
			    wn_index_step0 = WN_Stid(TY_mtype(ST_type(st_new_tmp)), 0, 
												st_new_tmp, ST_type(st_new_tmp), 
												wn_index_step0);
			    WN_INSERT_BlockLast( wn_init_index_block,  wn_index_step0);
				//step add
				wn_index_step1 = WN_Ldid(TY_mtype(ST_type(st_new_tmp)), 0, 
								st_new_tmp, ST_type(st_new_tmp));
			    wn_index_step1 = WN_Binary(OPR_ADD, TY_mtype(ST_type(acc_index_st)), 
								WN_COPY_Tree(wn_index), wn_index_step1);
			    wn_index_step0 = WN_Stid(TY_mtype(ST_type(acc_index_st)), 0, 
								acc_index_st, ST_type(acc_index_st), wn_index_step1);

	   		}
			else
			{
				//init value
				wn_index_init0 = WN_COPY_Tree(wn_gang_id);
				wn_index_init0 = WN_Binary(OPR_ADD, 
	   									TY_mtype(ST_type(acc_index_st)), 
	   									WN_COPY_Tree(wn_init), 
	   									wn_index_init0);
				wn_index_init0 = WN_Stid(TY_mtype(ST_type(acc_index_st)), 0, 
	   									acc_index_st, ST_type(acc_index_st), 
	   									wn_index_init0);
				//stride
				wn_index_step1 = wn_gang_width;
				wn_index_step1 = WN_Binary(OPR_ADD, TY_mtype(ST_type(acc_index_st)), 
								WN_COPY_Tree(wn_index), wn_index_step1);
			    wn_index_step0 = WN_Stid(TY_mtype(ST_type(acc_index_st)), 0, 
								acc_index_st, ST_type(acc_index_st), wn_index_step1);
			}
	   }
	   //then only vector clause appears
	   else
	   {
   			//init value
			wn_index_init0 = WN_COPY_Tree(wn_vector_id);
			wn_index_init0 = WN_Binary(OPR_ADD, 
	   									TY_mtype(ST_type(acc_index_st)), 
	   									WN_COPY_Tree(wn_init), 
	   									wn_index_init0);
			wn_index_init0 = WN_Stid(TY_mtype(ST_type(acc_index_st)), 0, 
   										acc_index_st, ST_type(acc_index_st), 
   										wn_index_init0);
			//stride
			wn_index_step1 = wn_vector_width;
			wn_index_step1 = WN_Binary(OPR_ADD, TY_mtype(ST_type(acc_index_st)), 
								WN_COPY_Tree(wn_index), wn_index_step1);
		    wn_index_step0 = WN_Stid(TY_mtype(ST_type(acc_index_st)), 0, 
							acc_index_st, ST_type(acc_index_st), wn_index_step1);
	   }
	   
	    WN* wn_for_test = WN_COPY_Tree(acc_test_stmt);
	    WN* wn_loopidame = WN_CreateIdname(0,acc_index_st);
		
	    if(this_acc_collapse_count > 1)
    	{
    		WN_INSERT_BlockFirst(wn_loopbody, wn_collapse_init_idx_blk);
    	}
	   
	    wn_pdo_node = WN_CreateDO(wn_loopidame, 
	   				wn_index_init0, wn_for_test, wn_index_step0, 
	   				wn_loopbody, NULL);
		/////////////////////////////////////////////////////////////////////
		//Traverse the loop body if it is not the innest loop
		if(bIsInnestLoop == FALSE)
			WN_do_body(wn_pdo_node) = ACC_Loop_Scheduling_Transformation_Gpu (
													WN_do_body(wn_pdo_node), 
													wn_replace_block);
		else
		{
			//Only take the replacement in the innest parallel loop
			//Scan the loop invariable first 
			ACC_Scalar_Replacement_Algorithm(wn_loopbody, acc_parallel_proc);
		}
		////////////////////////////////////////////////////////////////////
		//a new block
		wn_region_bdy = WN_CreateBlock();
		WN_INSERT_BlockLast(wn_region_bdy, wn_init_index_block);
		
		if(code_before_pdo)
			WN_INSERT_BlockLast(wn_region_bdy, code_before_pdo);

		//reduction pre process
		UINT32 RDIdx = 0;
		while(RDIdx < reductionmap.size())
		{
			if(bIsTopLoop == FALSE)
			{
				//no process, everything is left at the beginning of the 
				//scalar processing
				//here we only take care the non-top-level loop
				ACC_ReductionMap* pReductionMap = reductionmap[RDIdx];
				WN_INSERT_BlockLast( wn_region_bdy,  pReductionMap->wn_backupStmt);
				WN_INSERT_BlockLast( wn_region_bdy,  pReductionMap->wn_initialAssign);
			}
			RDIdx ++;
		}

		//taking care of pre-collapse stmts
		if(this_acc_collapse_count > 1)
			WN_INSERT_BlockLast(wn_region_bdy, wn_precollapse_block);
		////////////////////////////////////////////////////////////////////////////////
		WN_INSERT_BlockLast(wn_region_bdy, wn_pdo_node);
		////////////////////////////////////////////////////////////////////////////////
		//reduction post process
		RDIdx = 0;
		while(RDIdx < reductionmap.size())
		{
			ACC_ReductionMap* pReductionMap = reductionmap[RDIdx];
			//Call inner local reduction
			////////////////////////////////////////////////////////////////////////////////
			if(pReductionMap->local_reduction_fun)
			{
				//assign to an local array statement is only necessary
				//when the local reduction is required.
				//it means the local reduction function is not NULL.
				WN_INSERT_BlockLast( wn_region_bdy,  pReductionMap->wn_assignment2localArray);
				WN* wn_call = Gen_Sync_Threads();
				WN_INSERT_BlockLast( wn_region_bdy,  wn_call);
				wn_call = ACC_Gen_Call_Local_Reduction(pReductionMap->local_reduction_fun, 
															pReductionMap->st_local_array);
				WN_INSERT_BlockLast( wn_region_bdy,  wn_call);
			}
			if(bIsTopLoop == FALSE)
				WN_INSERT_BlockLast( wn_region_bdy,  pReductionMap->wn_assignBack2PrivateVar);
			else
				//for final reduction uses
				WN_INSERT_BlockLast( wn_region_bdy,  pReductionMap->wn_assignment2Array);
			
			RDIdx ++;
		}
		if(code_after_pdo)
			WN_INSERT_BlockLast(wn_region_bdy, code_after_pdo);

        WN *tree_prev_node = WN_prev(tree);
        WN *tree_next_node = WN_next(tree);
		
        if(tree_prev_node)
        {
            WN_prev(wn_region_bdy) = tree_prev_node;
            WN_next(tree_prev_node) = wn_region_bdy;
        }
        if(tree_next_node)
        {
            WN_next(wn_region_bdy) =  tree_next_node;
            WN_prev(tree_next_node) = wn_region_bdy;
        }
        tree = wn_region_bdy;

		//the original acc_collapse_count was changed 
		//in the recursive Loop_Scheduking_Transformation
		for(int i=0; i<acc_collapse_count; i++)
		{
			acc_loop_index_var.pop_back();
		}
  }

  else if (op == OPC_BLOCK) 
  {
    r = WN_first(tree);
    while (r) 
	{ // localize each node in block
      r = ACC_Loop_Scheduling_Transformation_Gpu (r, wn_replace_block);
      if (WN_prev(r) == NULL)
        WN_first(tree) = r;
      if (WN_next(r) == NULL)
        WN_last(tree) = r;

      r = WN_next(r);
      
   }
  }
  else 
  {
    for (i=0; i < WN_kid_count(tree); i++)
    {
      WN_kid(tree, i) = ACC_Loop_Scheduling_Transformation_Gpu( WN_kid(tree, i), wn_replace_block);
    }
  }
  
  return (tree);
}

