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
#include "wn_mp.h"        /* for wn_mp_dg.cxx's extern functions */
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
#include "dwarf_DST.h"
#include "dwarf_DST_producer.h"
#include "dwarf_DST_mem.h"
#include "config.h"
#include "standardize.h"
#include "irbdata.h"
#include "omp_lower.h"
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


/*****************************************/
static void Load_Kernel_Parameters(WN* wn_kernel_call)
{
	WN* wnx, *wn;
	int parm_id = 0;
	int i=0;
	
	int iarg_no = 0;
	
	//WN_Set_Call_IS_KERNEL_LAUNCH(wn);
	//first copyin, then copyout, then copy, then param pragma
	//
	for(i=0; i<acc_kernelLaunchParamList.size(); i++)
	{
		ST* hostST = acc_kernelLaunchParamList[i].st_host;
		//in case of deviceptr, the host st is NULL.
		TY_IDX ty = ST_type(hostST);
		TY_KIND kind = TY_kind(ty);//ST_name(old_st)
		TYPE_ID typeID;
		if(kind == KIND_POINTER || 
			kind == KIND_ARRAY ||
			(kind == KIND_STRUCT && F90_ST_Has_Dope_Vector(hostST)))
		{					
		   if(F90_ST_Has_Dope_Vector(hostST))
		   {
			   TY_IDX ty_element = ACC_GetDopeElementType(hostST);
			   TY_IDX pty = MTYPE_To_TY(TY_mtype(ty_element));			   
		   	   TY_IDX ty_p = Make_Pointer_Type(pty);
			   
			   wnx = WN_Ldid(Pointer_type, 0, hostST, ty_p);
		   }
		   else if(kind == KIND_ARRAY)
		   {		   	   
			   wnx = WN_Lda( Pointer_type, 0, hostST);
		   }
		   else if(kind == KIND_POINTER)
		   {		   	   
			   TY_IDX pty = TY_pointed(ty);
			   BOOL isArray = FALSE;
			   if(TY_kind(pty) == KIND_ARRAY)
			   {
					//it is an dynamic array
					pty = ACC_GetArrayElemType(pty);
					isArray = TRUE;
			   }
			   pty = MTYPE_To_TY(TY_mtype(pty));
		   	   TY_IDX ty_p = Make_Pointer_Type(pty);
			   wnx = WN_Ldid(Pointer_type, 0, hostST, ty_p);
		   }			

		   //First, pointer
		   WN_kid(wn_kernel_call, iarg_no) = WN_CreateParm(Pointer_type, wnx, 
		                       WN_ty(wnx), WN_PARM_BY_REFERENCE);
			
		}
		else if(kind == KIND_SCALAR)
		{
			ST* old_st = hostST;
			if(acc_offload_scalar_management_tab.find(old_st) == acc_offload_scalar_management_tab.end())
										 Fail_FmtAssertion("cannot find var in acc_offload_scalar_management_tab.");
			ACC_SCALAR_VAR_INFO* pVarInfo = acc_offload_scalar_management_tab[old_st];
			//new_ty, new_st are used to local variable
			//ty_param, st_param are used for parameter.
			//They may be different in ACC_SCALAR_VAR_INOUT and ACC_SCALAR_VAR_OUT cases			
			if(!pVarInfo)
				Fail_FmtAssertion("cannot find var in acc_offload_scalar_management_tab.");
			else if(pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_PRIVATE)
				Fail_FmtAssertion("A private var should not appear in kernel parameters.");
			else if(pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_IN)
			{
				//wnx = WN_Lda( Pointer_type, 0, old_st);
				wnx = WN_Ldid(TY_mtype(ST_type(old_st)), 
						0, old_st, ST_type(old_st));
				//First, pointer
			    WN_kid(wn_kernel_call, iarg_no) = WN_CreateParm(TY_mtype(ST_type(old_st)), wnx, 
			                       WN_ty(wnx), WN_PARM_BY_VALUE);
			}
			else if(pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_INOUT
				|| pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_OUT
				||	pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_CREATE
				||	pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_PRESENT)
			{
				wnx = WN_Lda( Pointer_type, 0, old_st);
				//First, pointer
			    WN_kid(wn_kernel_call, iarg_no) = WN_CreateParm(Pointer_type, wnx, 
			                       WN_ty(wnx), WN_PARM_BY_REFERENCE);
			}
			else
				Fail_FmtAssertion("Unclassified variables appears in kernel parameters.");
		}
		else
		{
			Is_True(FALSE, ("Wrong Kernel Parameter Kind Type. in Push_Kernel_Parameters 1"));
		}
		iarg_no ++;
	}
	//launch the reduction parameters
	i=0;
	//Only parallel region doing the reduction this way
	for(i=0; i<acc_additionalKernelLaunchParamList.size(); i++)
	{
		KernelParameter parmList = acc_additionalKernelLaunchParamList[i];
		////////////////////////////////////////////////////////////////////////////
		ST* st_device = parmList.st_device;
		TY_IDX ty = ST_type(st_device);
		TY_KIND kind = TY_kind(ty);//ST_name(old_st)
		TYPE_ID typeID;
		if(kind == KIND_POINTER)
		{			
	  		//wnx = WN_Lda(Pointer_type, 0, st_device);
			wnx = WN_Ldid(Pointer_type, 0, st_device, ST_type(st_device));
			//First, pointer
			WN_kid(wn_kernel_call, iarg_no) = WN_CreateParm(Pointer_type, wnx, 
							 		WN_ty(wnx), WN_PARM_BY_VALUE);
		}
		else
		{
			Is_True(FALSE, ("Wrong Kernel Parameter Kind Type in Push_Kernel_Parameters 2."));
		}
		iarg_no ++;
	}
}

WN* ACC_Launch_HSA_Kernel(int index, WN* wn_replace_block)
{
	WN * wn;
	WN * wnx;
	WN * l;
	//int iParm = device_copyout.size() + device_copyin.size() + acc_parms_count;
	UINT32 i = 0;
	int parm_id = 0;
	int arg_num = acc_kernelLaunchParamList.size() 
						+ acc_additionalKernelLaunchParamList.size() + 1;
	///////////////////////////////////////////////////////////////////////////
	
	char* localname = (char *) alloca(256);
	sprintf ( localname, "__accr_dim_%d", acc_reg_tmp_count);
	acc_reg_tmp_count++;
	TY_IDX ty_p = Make_Pointer_Type(Be_Type_Tbl(MTYPE_V));
	ST* st_dim = New_ST( CURRENT_SYMTAB );
	ST_Init(st_dim,
		Save_Str( localname ),
		CLASS_VAR,
		SCLASS_AUTO,
		EXPORT_LOCAL,
		ty_p);
	WN* wn_init_dim = ACC_Gen_Dim_Init_Call(st_dim);
	WN_INSERT_BlockLast(wn_replace_block, wn_init_dim);
	///////////////////////////////////////////////////////////////////////////
	
	wn = WN_Create(OPC_VCALL, arg_num);
	WN_st_idx(wn) = ST_st_idx(*(acc_kernel_functions_st[index]));
	//make the kernels parameters ready first
	Load_Kernel_Parameters(wn);
	//setup one more parameter for launch kernel

	
	wnx = WN_Ldid( Pointer_type, 0, st_dim, ST_type(st_dim));
	WN_kid(wn, arg_num-1) = WN_CreateParm(Pointer_type, wnx, 
                       WN_ty(wnx), WN_PARM_BY_REFERENCE);
	
	WN_INSERT_BlockLast(wn_replace_block, wn);
	return wn;
}


static void ACC_Delete_All_RID(RID* p_rid)
{
  	RID *rtmp;
 	rtmp=RID_first_kid(p_rid);
 
	while (rtmp) 
	{
		RID* rtmp_next = RID_next(rtmp);
		ACC_Delete_All_RID(rtmp);
		rtmp = rtmp_next;
	}
	WN* wh_node = p_rid->rwn;
	RID_Delete ( Current_Map_Tab, wh_node);
}


WN * 
lower_acc_apu ( WN * block, WN * node, LOWER_ACTIONS actions )
{
  INT32 i;			/* Temporary index */
  INT32 vsize;			/* Var_table size */
  INT32 lsize;			/* label_info_table size */
  INT32 ssize;			/* Shared_table size */
  INT32 msize;			/* Mpnum_table size */
  BOOL cont;			/* Loop control */
  WN   *wn;			/* Temporary node */
  WN   *temp_node;		/* Temporary node */
  WN   *stmt1_block;		/* If true statement block */
  WN   *stmt2_block;		/* If false statement block */
  WN   *cur_node;		/* Current node within original nodes */
  WN   *next_node;		/* Next node in sequence */
  WN   *return_nodes;		/* Nodes to be returned */
  WN   *fp;			/* Frame pointer uplink */
  WN   *mpsched_wn;		/* Real wn for mpsched node */
  WN   *chunk_wn;		/* Real wn for chunk node */
  WN   *acc_call_wn;		/* Real wn for mp call */
  WN   *if_cond_wn;		/* Real wn for if condition */
  ST   *lock_st;		/* ST for critical section lock */
  ST   *ntrip_st;		/* ST for loop trip count */
  ST   *return_st;		/* ST for mp status return */
  WN_OFFSET ntrip_ofst;		/* Offset for loop trip count */
  WN_OFFSET return_ofst;	/* Offset for mp status return */
  PREG_NUM rreg1, rreg2;	/* Pregs with I4 return values */
  INT32 num_criticals;		/* Number of nested critical sections */
  BOOL  while_seen;		/* While seen where do should be */
  WN   *task_fpsetup_blk; /* sets up firstprivate struct for task */
  
  WN* wn_cont_nodes = NULL;
  WN* wn_stmt_block = NULL;
  WN* wn_replace_block = NULL;  
  ACCP_process_type acc_process_type;


  /* Validate input arguments. */
  acc_target_arch = ACC_ARCH_TYPE_APU;

  Is_True(actions & LOWER_ACC,
	  ("actions does not contain LOWER_ACC"));
  Is_True(((WN_opcode(node) == OPC_PRAGMA) ||
	   (WN_opcode(node) == OPC_XPRAGMA) ||
	   (WN_opcode(node) == OPC_IF) ||
	   (WN_opcode(node) == OPC_REGION)),
	  ("invalid acc node"));

	//Is_True(PU_Info_proc_sym(Current_PU_Info) == last_pu_proc_sym,
	//        ("LowerMP_PU_Init() not called for this PU"));
	/* Determine processing required based on first node. */
	acc_t = ACCP_UNKNOWN;
	UINT32 tmp_flag;
	
	tmp_flag = 1 << UHACC_REDUCTION_USING_GLOBAL_MEMORY;
    acc_reduction_mem = (ReductionUsingMem)(Enable_UHACCFlag & tmp_flag);
	
	tmp_flag = 1 << UHACC_REDUCTION_USING_UNROLLING;
    acc_reduction_rolling = (ReductionRolling)((Enable_UHACCFlag & tmp_flag)
				>>UHACC_REDUCTION_USING_UNROLLING);
	
	tmp_flag = 1 << UHACC_ENABLE_DFA_OFFLOAD_REGION;
	acc_dfa_enabled = (Enable_UHACCFlag & tmp_flag) 
				>> UHACC_ENABLE_DFA_OFFLOAD_REGION;
	
	tmp_flag = 1 << UHACC_ENABLE_SCALARIZATION_OFFLOAD_LEVEL1;
	acc_scalarization_enabled = (Enable_UHACCFlag & tmp_flag) 
				>> UHACC_ENABLE_SCALARIZATION_OFFLOAD_LEVEL1;
	
	tmp_flag = 1 << UHACC_ENABLE_SCALARIZATION_OFFLOAD_LEVEL2;
	acc_scalarization_level2_enabled = (Enable_UHACCFlag & tmp_flag) 
				>> UHACC_ENABLE_SCALARIZATION_OFFLOAD_LEVEL2;
	
	tmp_flag = 1 << UHACC_ENABLE_SCALARIZATION_OFFLOAD_LEVEL3;
	acc_scalarization_level3_enabled = (Enable_UHACCFlag & tmp_flag) 
				>> UHACC_ENABLE_SCALARIZATION_OFFLOAD_LEVEL3;
	
	tmp_flag = 1 << UHACC_ENABLE_RESTRICT_PTR_OFFLOAD;
	acc_ptr_restrict_enabled = (Enable_UHACCFlag & tmp_flag) 
				>> UHACC_ENABLE_RESTRICT_PTR_OFFLOAD;
	
	acc_stmt_block = NULL;	  /* Original statement nodes */
	acc_cont_nodes = NULL;	  /* Statement nodes after acc code */
	acc_if_node = NULL;	  /* Points to (optional) if node */
	acc_device_nodes = NULL;
	acc_host_nodes = NULL;
	acc_replace_block = NULL;
	acc_reduction_nodes = NULL; /* Points to (optional) reduction nodes */
	acc_copy_nodes = NULL;	  /* Points to (optional) shared nodes */
	acc_copyin_nodes = NULL;	  /* Points to (optional) copyin nodes */
	acc_copyout_nodes = NULL;   /* Points to (optional) copyout nodes */
	acc_wait_nodes = NULL;  /* Points to (optional) acc wait pragma nodes */
	acc_parms_nodes = NULL;
	acc_num_gangs_node = NULL;
	acc_num_workers_node = NULL;
	acc_vector_length_node = NULL;
	acc_collapse_node = NULL;
	acc_gang_node = NULL;
	acc_worker_node = NULL;
	acc_vector_node = NULL;
	acc_seq_node = NULL;
	acc_independent_node = NULL;


	acc_create_nodes = NULL;
	acc_present_nodes = NULL;
	acc_present_or_copy_nodes = NULL;
	acc_present_or_copyin_nodes = NULL;
	acc_present_or_copyout_nodes = NULL;
	acc_present_or_create_nodes = NULL;
	acc_deviceptr_nodes = NULL;
	acc_private_nodes = NULL;
	acc_wait_nodes = NULL;
	acc_use_device_nodes = NULL;

	acc_dregion_pcreate.clear();
	acc_dregion_pcopy.clear();
	acc_dregion_pcopyin.clear();
	acc_dregion_pcopyout.clear();
	acc_dregion_present.clear();
	acc_dregion_host.clear();
	acc_dregion_device.clear();
	acc_dregion_private.clear();
	acc_dregion_fprivate.clear();
	acc_wait_list.clear();	
	acc_host_data_use_device.clear();
	/*acc_create_count = 0;
	acc_present_count = 0;
	acc_present_or_copy_count = 0;
	acc_present_or_copyin_count = 0;
	acc_present_or_copyout_count = 0;
	acc_present_or_create_count = 0;
	acc_deviceptr_count = 0;
	acc_private_count = 0;
	acc_firstprivate_count = 0;*/

	is_pcreate_tmp_created = FALSE;

  acc_collapse_count = 0;   /* collapse count */
  acc_clause_intnum = 0;	  /*Int expression, it's for integer expression, e.g, in wait pragma*/
  //acc_copyin_count = 0;  /* Count of copyins */
  //acc_copyout_count = 0; /* Count of copyouts */
  //acc_copy_count = 0;	  /* Count of copys */
  acc_async_nodes = 0;
  //acc_parms_count = 0;
  acc_reduction_count = 0;
  acc_kernel_functions_st.clear();
  //nested data region info reset
  acc_nested_dregion_info.Depth = 0;
  acc_nested_dregion_info.DRegionInfo.clear();
  
  //ssize = 4096 * sizeof(ACC_SHARED_TABLE);
  //acc_copyin_table = (ACC_SHARED_TABLE *) alloca ( ssize );
  //BZERO ( acc_copyin_table, ssize );
  
  //ssize = 4096 * sizeof(ACC_SHARED_TABLE);
  //acc_copyout_table = (ACC_SHARED_TABLE *) alloca ( ssize );
  //BZERO ( acc_copyout_table, ssize );
  
  //ssize = 4096 * sizeof(ACC_SHARED_TABLE);
  //acc_copy_table = (ACC_SHARED_TABLE *) alloca ( ssize );
  //BZERO ( acc_copy_table, ssize );
  	
  //ssize = 4096 * sizeof(ACC_SHARED_TABLE);
  //acc_parm_table = (ACC_SHARED_TABLE *) alloca ( ssize );
  //BZERO ( acc_parm_table, ssize );

  start_processing:

  if ((WN_opcode(node) == OPC_REGION) &&
	     WN_first(WN_region_pragmas(node)) &&
	     ((WN_opcode(WN_first(WN_region_pragmas(node))) == OPC_PRAGMA) ||
	      (WN_opcode(WN_first(WN_region_pragmas(node))) == OPC_XPRAGMA))) {

    WN *wtmp = WN_first(WN_region_pragmas(node));
    WN_PRAGMA_ID wid = (WN_PRAGMA_ID) WN_pragma(wtmp);


    switch (wid) {
      /* orphaned SINGLE region: process it now and return */
    case WN_PRAGMA_ACC_KERNELS_BEGIN:
      acc_process_type = ACCP_KERNEL_REGION;
      acc_line_no = acc_get_lineno(wtmp);
      break;
    case WN_PRAGMA_ACC_PARALLEL_BEGIN:
      acc_process_type = ACCP_PARALLEL_REGION;
      acc_line_no = acc_get_lineno(wtmp);
      break;
    case WN_PRAGMA_ACC_DATA_BEGIN:
      acc_process_type = ACCP_DATA_REGION;
      break;
    case WN_PRAGMA_ACC_HOSTDATA_BEGIN:
      acc_process_type = ACCP_HOST_DATA_REGION;
      break;	  
	case WN_PRAGMA_ACC_WAIT:
	  acc_process_type = ACCP_WAIT_REGION;
	  break;
    case WN_PRAGMA_ACC_UPDATE:
      acc_process_type = ACCP_UPDATE_REGION;
      break;
    case WN_PRAGMA_ACC_DECLARE:
      acc_process_type = ACCP_DECLARE_REGION;
      break;
    case WN_PRAGMA_ACC_CACHE:
      acc_process_type = ACCP_CACHE_REGION;
      break;
    case WN_PRAGMA_ACC_ENTER_DATA:
      acc_process_type = ACCP_ENTER_DATA_REGION;
      break;
    case WN_PRAGMA_ACC_EXIT_DATA:
      acc_process_type = ACCP_EXIT_DATA_REGION;
      break;
	  

    default:
      printf("pragma value = %d", (int)wid); 
      Fail_FmtAssertion (
		 "out of context pragma (%s) in acc {primary pragma} processing",
		 WN_pragmas[wid].name);
    }

    next_node = WN_next(wtmp);
    wn_cont_nodes = WN_next(node);
    wn_stmt_block = WN_region_body(node);

    //if (mpt != MPP_ORPHANED_PDO) 
    {
      WN_Delete ( wtmp );
      WN_Delete ( WN_region_pragmas(node) );
      WN_DELETE_Tree ( WN_region_exits(node) );
    }

  }

  
  ACC_Process_Clause_Pragma(next_node);

  if(wn_cont_nodes)
	WN_prev(wn_cont_nodes) = NULL;

	wn_replace_block = WN_CreateBlock();

	if (acc_process_type == ACCP_DATA_REGION)
	{
		//it will include any other constructs.
		//Get the information and move to the next stage
		SingleDRegionInfo sDRegionInfo;
		sDRegionInfo.acc_if_node = acc_if_node;
		//sDRegionInfo.acc_copy_nodes = acc_copy_nodes;		/* Points to (optional) copy nodes */
		//sDRegionInfo.acc_copyin_nodes = acc_copyin_nodes;	/* Points to (optional) copyin nodes */
		//sDRegionInfo.acc_copyout_nodes = acc_copyout_nodes;	/* Points to (optional) copyout nodes */	
		//sDRegionInfo.acc_create_nodes = acc_create_nodes;
		sDRegionInfo.acc_present_nodes = acc_present_nodes;
		sDRegionInfo.acc_present_or_copy_nodes = acc_present_or_copy_nodes;
		sDRegionInfo.acc_present_or_copyin_nodes = acc_present_or_copyin_nodes;
		sDRegionInfo.acc_present_or_copyout_nodes = acc_present_or_copyout_nodes;
		sDRegionInfo.acc_present_or_create_nodes = acc_present_or_create_nodes;
		sDRegionInfo.acc_deviceptr_nodes = acc_deviceptr_nodes;

		sDRegionInfo.acc_dregion_pcreate = acc_dregion_pcreate;		
		sDRegionInfo.acc_dregion_pcopy = acc_dregion_pcopy;		
		sDRegionInfo.acc_dregion_pcopyin = acc_dregion_pcopyin;		
		sDRegionInfo.acc_dregion_pcopyout = acc_dregion_pcopyout;		
		sDRegionInfo.acc_dregion_present = acc_dregion_present;		
		sDRegionInfo.acc_dregion_private = acc_dregion_private;		
		sDRegionInfo.acc_dregion_fprivate = acc_dregion_fprivate;
		
	    sDRegionInfo.wn_cont_nodes = wn_cont_nodes;
	    sDRegionInfo.wn_stmt_block = wn_stmt_block;
		
		  //////////////////////////////////////////////////////////////////////////
		  
  		acc_nested_dregion_info.DRegionInfo.push_back(sDRegionInfo);
		acc_nested_dregion_info.Depth ++;
		
		//reset them
		acc_if_node = NULL;
		acc_copy_nodes = NULL;		/* Points to (optional) copy nodes */
		acc_copyin_nodes = NULL;	/* Points to (optional) copyin nodes */
		acc_copyout_nodes = NULL;	/* Points to (optional) copyout nodes */	
		acc_create_nodes = NULL;
		acc_present_nodes = NULL;
		acc_present_or_copy_nodes = NULL;
		acc_present_or_copyin_nodes = NULL;
		acc_present_or_copyout_nodes = NULL;
		acc_present_or_create_nodes = NULL;
		acc_deviceptr_nodes = NULL;
		
		acc_dregion_pcreate.clear();
		acc_dregion_pcopy.clear();
		acc_dregion_pcopyin.clear();
		acc_dregion_pcopyout.clear();
		acc_dregion_present.clear();
		acc_dregion_host.clear();
		acc_dregion_device.clear();
		acc_dregion_private.clear();
		acc_dregion_fprivate.clear();
		
		wn_replace_block = ACC_Process_DataRegion(wn_stmt_block);
  		acc_nested_dregion_info.DRegionInfo.pop_back();
		acc_nested_dregion_info.Depth --;
	}
	else if (acc_process_type == ACCP_KERNEL_REGION) 
	{
	  //generate kernel and launch the kernel
	  wn_replace_block = ACC_Process_KernelsRegion(wn_stmt_block, wn_cont_nodes);
	} 	 
	else if(acc_process_type == ACCP_HOST_DATA_REGION)
	{
	  //TODO for host construct region
	  wn_replace_block = ACC_Process_HostDataRegion(wn_stmt_block);
	}
	else if(acc_process_type == ACCP_PARALLEL_REGION)
	{
	  //It will include LOOP constructs
	  wn_replace_block = ACC_Process_ParallelRegion(wn_stmt_block, wn_cont_nodes);
	}		  	   	  
	else if (acc_process_type == ACCP_WAIT_REGION) 
	{
		  //Wait
	  wn_replace_block = ACC_Process_WaitRegion(wn_stmt_block);
	} 	  	  
	else if (acc_process_type == ACCP_DECLARE_REGION) 
	{
		  //Declare
	  wn_replace_block = ACC_Process_DeclareRegion(wn_stmt_block);
	} 	  	  
	else if (acc_process_type == ACCP_CACHE_REGION) 
	{
		  //Cache
	  wn_replace_block = ACC_Process_CacheRegion(wn_stmt_block);
	} 	  	  
	else if (acc_process_type == ACCP_UPDATE_REGION) 
	{
		  //Update
	  wn_replace_block = ACC_Process_UpdateRegion(wn_stmt_block);
	}   	  
	else if (acc_process_type == ACCP_ENTER_DATA_REGION) 
	{
		  //Enter Data
	  wn_replace_block = ACC_Process_EnterData_Region(wn_stmt_block);
	}   	  
	else if (acc_process_type == ACCP_EXIT_DATA_REGION) 
	{
		  //Exit Data
	  wn_replace_block = ACC_Process_ExitData_Region(wn_stmt_block);
	}  

  
  /* For all other parallel nodes return the replacement code chained with
       any code following the original parallel constructs. */

  if (WN_first(wn_replace_block)) 
  {
	  return_nodes = WN_first(wn_replace_block);
	  WN_next(WN_last(wn_replace_block)) = wn_cont_nodes;
	  if (wn_cont_nodes)
	  	WN_prev(wn_cont_nodes) = WN_last(wn_replace_block);
  } 
  else
	  return_nodes = wn_cont_nodes;

	WN_Delete (wn_replace_block );    
    ACC_Delete_All_RID ( REGION_get_rid(node) );
    return (return_nodes);
}



static void ACC_Blank()
{
}
