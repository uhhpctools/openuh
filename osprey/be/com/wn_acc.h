/***************************************************************************
  This file is created by Xiaonan(Daniel) Tian from HPCTools, University of Houston
  (daniel.xntian@gmail.com) for OpenUH OpenACC compiler.
  It is intended to lower the OpenACC pragma.
  It is free to use. However, please keep the original author.
  http://www2.cs.uh.edu/~xntian2/
*/



/**
***		OPENACC Lowering Support
***		---------------------------
***
*** Description:
***
***	This interface describes all the declarations and data needed to
***     perform and support ACC lowering.
***
*** Exported functions:
***
***  These functions are from wn_acc.cxx:
***
***	lower_acc	called when first ACC pragma is encountered, identifies,
***			extracts, converts and replaces an entire parallel
***			region in the whirl tree
***
***
**/

#ifndef wnacc_INCLUDED
#define wnacc_INCLUDED "wn_acc.h"


#include <map>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>

using namespace std;


#if (! defined(__ELF_H__)) && (! defined(BUILD_OS_DARWIN))
#include <elf.h>            /* pu_info.h can't compile without this */
#endif

#ifndef dwarf_DST_INCLUDED
#include "dwarf_DST.h"      /* for DST_IDX and DST_INFO_IDX */
#endif

#ifndef pu_info_INCLUDED    /* for PU_Info */
#include "pu_info.h"
#endif

#ifndef cxx_template_INCLUDED
#include "cxx_template.h"   /* for DYN_ARRAY */
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define WN_ACC_Compare_Trees(x,y)	(WN_Simp_Compare_Trees(x,y))  

inline WN_OFFSET WN_offsetx ( WN *wn )
{
  OPERATOR opr;
  opr = WN_operator(wn);
  if ((opr == OPR_PRAGMA) || (opr == OPR_XPRAGMA)) {
    return (WN_pragma_arg1(wn));
  } else {
    return (WN_offset(wn));
  }
}

extern WN * lower_acc_nvidia (WN * block, WN * node, INT64 actions);
extern WN * lower_acc_apu (WN * block, WN * node, INT64 actions);
extern INT32 acc_region_num;	 // MP region number within parent PU
extern INT32 acc_construct_num; // construct number within MP region
extern UINT32 Enable_UHACCFlag;


typedef enum {
  ACCP_UNKNOWN,
  ACCP_PARALLEL_REGION,
  ACCP_LOOP_REGION,
  ACCP_HOST_DATA_REGION,
  ACCP_DATA_REGION,
  ACCP_KERNEL_REGION,
  ACCP_UPDATE_REGION,
  ACCP_CACHE_REGION,
  ACCP_DECLARE_REGION,
  ACCP_WAIT_REGION, 
  ACCP_ENTER_DATA_REGION, 
  ACCP_EXIT_DATA_REGION,  
  ACCP_ATOMIC_REGION
} ACCP_process_type;

typedef enum 
{
	ACC_NONE_SPECIFIED,
	ACC_VECTOR = 1,
	ACC_GANG_VECTOR,
	ACC_WORKER,
	ACC_GANG,
	ACC_GANG_WORKER,
	ACC_WORKER_VECTOR,
	ACC_GANG_WORKER_VECTOR
}ACC_LOOP_TYPE;

typedef enum ACC_LOOP_LEVEL
{
	ACC_OUTTER_LOOP,
	ACC_MIDDER_LOOP,
	ACC_INNER_LOOP,
	ACC_PARALLEL_REDUCTION
}ACC_LOOP_LEVEL;

typedef struct ACC_ReductionMap
{
	ST* hostName;//host reduction ST
	ST* deviceName; //device memory ST, allocated in host side
	ST* st_Inkernel; //st in the kernel. For each reduction, there is a respective st* in the kernel, usually as kernel parameter. this is a buffer
	ST* st_inout_used;	//the variable init/output buffer before/after reduction
	ST* reduction_kenels; //an independent kernel carry the final reduction
	ST* local_reduction_fun; //for the top level reduction, the kernel may call another device function to do the local reduction first, then launch final reduction kernel
	ST* st_private_var; //private var in kernel to store reduction result
	WN* wn_private_var;
	ST* st_local_array;//local memory buffer for reduction in the block
	OPERATOR ReductionOpr; 
	//ACC_LOOP_LEVEL acc_looplevel;
	//ACC_LOOP_LEVEL acc_stmt_location; //where is the reduction stmt
	ACC_LOOP_TYPE acc_stmt_location_new; //another version, under evaluation
	ACC_LOOP_TYPE looptype;
	WN* wn_localIndexOpr; //for local reduction used inner gang
	WN* wn_assignment2localArray; //this is used in kernel
	WN* wn_IndexOpr;
	WN* wn_assignment2Array; //this is used in kernel
	WN* wn_initialAssign; //this is used in kernel
	WN* wn_assignBack2PrivateVar; //this is used in kernel
	ST* st_num_of_element;	
	ST* st_backupValue;		
	WN* wn_backupValue;	
	WN* wn_backupStmt;	
}ACC_ReductionMap;

extern WN* VH_OpenACC_Lower(WN * node, INT64 actions);

/****************************************************************/
//for offload region transformation
extern char acc_tmp_name_prefix[];
extern WN_MAP_TAB *acc_pmaptab;	/* Parent map table */
extern WN_MAP_TAB *acc_cmaptab;	/* Child map table */

extern PU_Info *acc_ppuinfo;	/* Parent PU info structure */
extern SYMTAB_IDX acc_psymtab;	/* Parent symbol table */
extern SYMTAB_IDX acc_csymtab;	/* Child symbol table */
extern INT32 acc_func_level;	/* Parallel function stab level */
extern UINT32 kernel_tmp_variable_count;

/****************************************************************/
extern UINT32 acc_collapse_count;

/****************************************************************/
//for scalar replacement algorithm
extern BOOL acc_scalarization_enabled;
extern BOOL acc_scalarization_level2_enabled;
extern BOOL acc_scalarization_level3_enabled;
extern BOOL acc_ptr_restrict_enabled;
extern map<ST*, BOOL> acc_const_offload_scalar;
extern vector<ST*> acc_loop_index_var;
extern UINT32 kernel_tmp_licm_count;
extern INT32 MAX_REGISTERS_ALLOWED_PER_KERNEL;
extern TY_IDX ACC_Get_ElementTYForMultiArray(ST* stArr);
extern void ACC_Scalar_Replacement_Algorithm(WN* tree, ST* st_kernel);

/*****************************************************************/
//Misc variables and functions
extern BOOL ACC_Identical_Pragmas ( WN * wn1, WN * wn2 );
extern  WN_OFFSET ACC_WN_offsetx ( WN *wn );
extern  void ACC_WN_set_offsetx ( WN *wn, WN_OFFSET ofst );
extern void ACC_Host_Create_Preg_or_Temp ( TYPE_ID mtype, 
											const char *name, ST **st);
extern void ACC_Device_Create_Preg_or_Temp ( TYPE_ID mtype, const char *name, ST **st);
extern WN* ACC_WN_Integer_Cast(WN* tree, TYPE_ID to, TYPE_ID from);
extern void acc_my_Get_Return_Pregs(PREG_NUM *rreg1, PREG_NUM *rreg2, mTYPE_ID type,
                    const char *file, INT line);
extern int acc_get_lineno(WN *wn);
extern TY_IDX F90_ST_Get_Dope_Vector_etype(ST *st);
extern WN* Gen_ACC_Load( ST * st, WN_OFFSET offset, BOOL scalar_only );
extern WN* Gen_ACC_Store( ST * st, WN_OFFSET offset, WN * value, BOOL scalar_only);

#define WN_ACC_Compare_Trees(x,y)	(WN_Simp_Compare_Trees(x,y))  
extern UINT32 acc_reg_tmp_count; 
extern INT64 acc_line_number;	/* Line number of acc parallel/kernel region */
extern INT64 acc_line_no;

//For forloop analysis
extern WN * acc_test_stmt;
extern vector<WN *> acc_base_node;		  /* Parallel forloop base */
extern vector<WN *> acc_limit_node;		/* Parallel forloop limit */
extern WN *acc_ntrip_node;		/* Parallel forloop trip count, iterations in this loop */
extern vector<WN *> acc_stride_node;		/* Parallel forloop stride */
extern WN *acc_doloop_body;
extern UINT32 acc_loopnest_depth;
extern vector<ST *> acc_forloop_index_st;		/* User forloop index variable ST */
extern vector<TYPE_ID> acc_forloop_index_type;	/* User forloop index variable type */
extern INT32 acc_reduction_count; 

/*
* Extract do info for acc info (init, limit, step, etc). 
*/
void ACC_Extract_Do_Info ( WN * do_tree );
/*extract indices information*/
void ACC_Extract_Index_Info ( WN* pdo_node );
extern ACCP_process_type acc_t;

extern void ACC_Extract_ACC_LoopNest_Info( WN * tree );
extern WN* ACC_Walk_and_Replace_ACC_Loop_Seq (WN * tree);
/********************************************************************************************************
***************************Data Handle Structure*********************************************************
*********************************************************************************************************/	
typedef enum ACC_SCALAR_TYPE
{
	ACC_SCALAR_VAR_IN,
	ACC_SCALAR_VAR_OUT,
	ACC_SCALAR_VAR_INOUT,
	ACC_SCALAR_VAR_PRIVATE, 
	//The difference between ACC_SCALAR_VAR_OUT and 
	//ACC_SCALAR_VAR_CREATE is that CREATE do not need copyout
	ACC_SCALAR_VAR_CREATE,	
	ACC_SCALAR_VAR_PRESENT 
}ACC_SCALAR_TYPE;

typedef struct ACC_DREGION__ENTRY
{
	WN* acc_data_clauses;
	//This is for .dope structure
	WN* acc_data_start_addr;
	//may be multi-dimension segment
	vector<WN*> acc_data_start;
	vector<WN*> acc_data_length;
	ACC_SCALAR_TYPE acc_scalar_type;
}ACC_DREGION__ENTRY;

/*
*Run time Generation
*/
typedef enum {

	ACCRUNTIME_NONE = 0,
	ACCRUNTIME_FIRST = 1,

	ACCR_SETUP              = 1,
	ACCR_CLEANUP            = 2,

	ACCR_SYNC				= 3,	/* Not really needed? to be deleted*/	
	ACCR_DEVICEMEMMALLOC			= 4,	
	ACCR_DEVICEMEMIN			= 5,
	ACCR_DEVICEMEMOUT			= 6,
	ACCR_LAUNCHKERNEL		= 7,
	ACCR_KERNELPARAMPUSH_POINTER	= 8,
	ACCR_PRESENT_COPY		= 9,
	ACCR_PRESENT_COPYIN		= 10,
	ACCR_PRESENT_COPYOUT	= 11,
	ACCR_PRESENT_CREATE		= 12,
	ACCR_SET_GANG_NUM_X		= 13,
	ACCR_SET_GANG_NUM_Y		= 14,
	ACCR_SET_GANG_NUM_Z		= 15,
	ACCR_SET_VECTOR_NUM_X	= 16,
	ACCR_SET_VECTOR_NUM_Y	= 17,
	ACCR_SET_VECTOR_NUM_Z	= 18,
	ACCR_DMEM_RELEASE		= 19,
	ACCR_MAP_DREGION		= 20,
	ACCR_KERNELPARAMPUSH_SCALAR	= 21,
	ACCR_KERNELPARAMPUSH_DOUBLE = 22,
	ACCR_REDUCTION_BUFF_MALLOC = 23,
	ACCR_FINAL_REDUCTION_ALGORITHM = 24,
	ACCR_FREE_ON_DEVICE = 25,
	ACCR_SETUP_DEFAULT_TOLOGY = 26,
	ACCR_SETUP_GANG_TOLOGY 	  = 27,
	ACCR_SETUP_VECTOR_TOLOGY 	  = 28,
	ACCR_RESET_DEFAULT_TOLOGY = 29,
	ACCR_GET_DEVICE_ADDR = 30,
	ACCR_UPDATE_HOST_VARIABLE = 31,
	ACCR_UPDATE_DEVICE_VARIABLE 	= 32,
	ACCR_WAIT_SOME_OR_ALL_STREAM	= 33,
	ACCR_SYNCTHREADS				= 34, //this one is only used in kernel functions
	ACCR_PRINTF_DBG				= 35,
	ACCR_GET_NUM_GANGS			= 36,
	ACCR_GET_NUM_WORKERS		= 37,
	ACCR_GET_NUM_VECTORS		= 38,	
	ACCR_GET_TOTAL_VECTORS		= 39,	
	ACCR_GET_TOTAL_GANGS		= 40,
	ACCR_GET_TOTAL_GANGS_WORKERS = 41,
	ACCR_CALL_LOCAL_REDUCTION	= 42,
	ACCR_DYNAMIC_LAUNCH_KERNEL	= 43,
	ACCR_STACK_PUSH			= 44,
	ACCR_STACK_POP			= 45,
	ACCR_STACK_PENDING_TO_CURRENT_STACK = 46,
	ACCR_STACK_CLEAR_DEVICE_PTR_IN_CURRENT_STACK = 47,
	ACCR_DATA_EXIT_COPYOUT = 48,
	ACCR_DATA_EXIT_DELETE = 49,
	ACCR_FREE_REDUCTION_BUFF = 50,
	ACCR_INIT_LAUNCH_PARAMS = 51,
	ACCR_INIT_LAUNCH_RED_PARAMS = 52,	//for reduction kernel launch
	ACCR_LAUNCHKERNEL_EX = 53,	//for CUDA launch with options
	ACCRUNTIME_LAST 		= ACCR_LAUNCHKERNEL_EX
} OACCRUNTIME;

typedef enum {
	PAR_FUNC_ACC_NONE = 0,
	PAR_FUNC_ACC_KERNEL,
	PAR_FUNC_ACC_DEVICE,
	PAR_FUNC_ACC_LAST = PAR_FUNC_ACC_DEVICE
} PAR_FUNC_ACC_TYPE;


typedef struct {
	ST* hostName;
	ST* deviceName;
	WN* wnSize;
	WN* wnStart;
	unsigned int type_size;
	ST* st_is_pcreate_tmp;
}ACC_DATA_ST_MAP;


typedef enum 
{
	ACC_KDATA_UNKOWN = 0,
//MTYPE_I1=2, 		 /*   8-bit integer */
	ACC_KDATA_UINT8,
//MTYPE_I2=3, 		 /*  16-bit integer */
	ACC_KDATA_UINT16,
//MTYPE_I4=4, 		 /*  32-bit integer */
	ACC_KDATA_UINT32,
//MTYPE_I8=5, 		 /*  64-bit integer */
	ACC_KDATA_UINT64,
//MTYPE_U1=6, 		 /*   8-bit unsigned integer */
	ACC_KDATA_INT8,
//MTYPE_U2=7, 		 /*  16-bit unsigned integer */
	ACC_KDATA_INT16,
//MTYPE_U4=8, 		 /*  32-bit unsigned integer */
	ACC_KDATA_INT32,
//MTYPE_U8=9, 		 /*  64-bit unsigned integer */
	ACC_KDATA_INT64,
//MTYPE_F4=10,		 /*  32-bit IEEE floating point */
	ACC_KDATA_FLOAT,
//MTYPE_F8=11,		 /*  64-bit IEEE floating point */
	ACC_KDATA_DOUBLE	
}ACC_KERNEL_DATA_TYPE;

extern  ST_IDX Make_ACCRuntime_ST ( OACCRUNTIME rop );

#define GET_ACCRUNTIME_ST(x) (accr_sts[x] == ST_IDX_ZERO ? \
                             Make_ACCRuntime_ST(x) : accr_sts[x])

extern const char *accr_names [ACCRUNTIME_LAST + 1];
extern ST_IDX accr_sts [ACCRUNTIME_LAST + 1];
extern TY_IDX accruntime_ty;
extern WN* ACC_GetArraySize(ACC_DREGION__ENTRY dEntry);
extern WN* ACC_GetArrayElementSize(ACC_DREGION__ENTRY dEntry);
extern TY_IDX ACC_GetArrayElemType(TY_IDX ty);
extern WN* ACC_GetArrayStart(ACC_DREGION__ENTRY dEntry);
extern WN* ACC_GetArraySizeInUnit(ACC_DREGION__ENTRY dEntry);
extern TY_IDX ACC_GetDopeElementType(ST* st_host);
extern INT64 GetKernelParamType(ST* pParamST);
/********************************************************************************************************
********************************************Off load Region *********************************************/
extern ACC_ARCH_TYPE acc_target_arch;

extern ST *glbl_threadIdx_x;
extern ST *glbl_threadIdx_y;
extern ST *glbl_threadIdx_z;
extern ST *glbl_blockIdx_x;
extern ST *glbl_blockIdx_y;
extern ST *glbl_blockIdx_z;
extern ST *glbl_blockDim_x;
extern ST *glbl_blockDim_y;
extern ST *glbl_blockDim_z;
extern ST *glbl_gridDim_x;
extern ST *glbl_gridDim_y;
extern ST *glbl_gridDim_z;
extern ST *st_glbl_threadIdx_gid_x;
extern ST *st_glbl_threadDim_gbl_x;

extern WN* threadidx;
extern WN* threadidy;
extern WN* threadidz;

extern WN* blockidx;
extern WN* blockidy;
extern WN* blockidz;

extern WN* blockdimx;
extern WN* blockdimy;
extern WN* blockdimz;

extern WN* griddimx;
extern WN* griddimy;
extern WN* griddimz;

extern WN* wn_threadid_gid_x;
extern WN* wn_thread_global_width;


extern ST *acc_parallel_proc;	/* Extracted parallel/kernels process */
extern vector<ST*> acc_kernel_functions_st; //ST list of kernel functions created
extern ST* acc_st_shared_memory;

//structure for preserve data region info
typedef struct
{
	vector<ACC_DREGION__ENTRY> acc_dregion_pcreate;
	vector<ACC_DREGION__ENTRY> acc_dregion_pcopy;
	vector<ACC_DREGION__ENTRY> acc_dregion_pcopyin;
	vector<ACC_DREGION__ENTRY> acc_dregion_pcopyout;
	vector<ACC_DREGION__ENTRY> acc_dregion_present;
	vector<ACC_DREGION__ENTRY> acc_dregion_private;
	vector<ACC_DREGION__ENTRY> acc_dregion_fprivate;
	WN * acc_if_node;
	WN * acc_copy_nodes;		/* Points to (optional) copy nodes */
	WN * acc_copyin_nodes;	/* Points to (optional) copyin nodes */
	WN * acc_copyout_nodes; /* Points to (optional) copyout nodes */	
	WN * acc_create_nodes;	/*copy/copyin/copyout/create will merge into pcopy/pcopyin/pcopyout/pcreate*/
	/////////////////////////////////////////////////////////
	WN * acc_present_nodes;
	vector<ACC_DATA_ST_MAP*> presentMap;
	WN * acc_present_or_copy_nodes;
	vector<ACC_DATA_ST_MAP*> pcopyMap;
	WN * acc_present_or_copyin_nodes;
	vector<ACC_DATA_ST_MAP*> pcopyinMap;
	WN * acc_present_or_copyout_nodes;
	vector<ACC_DATA_ST_MAP*> pcopyoutMap;
	WN * acc_present_or_create_nodes;
	vector<ACC_DATA_ST_MAP*> pcreateMap;
	WN * acc_deviceptr_nodes;
	vector<ST*> dptrList;
	WN* wn_cont_nodes;
	WN* wn_stmt_block;
}SingleDRegionInfo;

typedef struct 
{
	UINT32 Depth;
	vector<SingleDRegionInfo> DRegionInfo;
}DataRegion;

extern DataRegion acc_nested_dregion_info;

//structure for preserve kernels region info
typedef struct
{
	vector<ACC_DREGION__ENTRY> acc_dregion_pcreate;
	vector<ACC_DREGION__ENTRY> acc_dregion_pcopy;
	vector<ACC_DREGION__ENTRY> acc_dregion_pcopyin;
	vector<ACC_DREGION__ENTRY> acc_dregion_pcopyout;
	vector<ACC_DREGION__ENTRY> acc_dregion_present;
	vector<ACC_DREGION__ENTRY> acc_dregion_private;
	vector<ACC_DREGION__ENTRY> acc_dregion_fprivate;
	vector<ACC_DREGION__ENTRY> acc_dregion_lprivate;
	vector<ACC_DREGION__ENTRY> acc_dregion_inout_scalar;
	//////////////////////////////////////////////////////
	WN * acc_if_node;
	WN * acc_copy_nodes;		/* Points to (optional) copy nodes */
	WN * acc_copyin_nodes;	/* Points to (optional) copyin nodes */
	WN * acc_copyout_nodes; /* Points to (optional) copyout nodes */	
	WN * acc_create_nodes;	/*copy/copyin/copyout/create will merge into pcopy/pcopyin/pcopyout/pcreate*/
	/////////////////////////////////////////////////////////
	WN * acc_present_nodes;
	vector<ACC_DATA_ST_MAP*> presentMap;
	WN * acc_present_or_copy_nodes;
	vector<ACC_DATA_ST_MAP*> pcopyMap;
	WN * acc_present_or_copyin_nodes;
	vector<ACC_DATA_ST_MAP*> pcopyinMap;
	WN * acc_present_or_copyout_nodes;
	vector<ACC_DATA_ST_MAP*> pcopyoutMap;
	WN * acc_present_or_create_nodes;
	vector<ACC_DATA_ST_MAP*> pcreateMap;
	WN * acc_deviceptr_nodes;
	vector<ST*> dptrList;
	WN* acc_async;
	//kernel launch parameters, not in spec, 
	//when the def/use module done, it will be removed.
	WN* acc_param; 
}KernelsRegionInfo;


//structure for preserve parallel region info
typedef struct
{
	vector<ACC_DREGION__ENTRY> acc_dregion_pcreate;
	vector<ACC_DREGION__ENTRY> acc_dregion_pcopy;
	vector<ACC_DREGION__ENTRY> acc_dregion_pcopyin;
	vector<ACC_DREGION__ENTRY> acc_dregion_pcopyout;
	vector<ACC_DREGION__ENTRY> acc_dregion_present;
	vector<ACC_DREGION__ENTRY> acc_dregion_private;
	vector<ACC_DREGION__ENTRY> acc_dregion_fprivate;
	vector<ACC_DREGION__ENTRY> acc_dregion_lprivate;
	vector<ACC_DREGION__ENTRY> acc_dregion_inout_scalar;
	////////////////////////////////////////////////////////////////
	WN * acc_if_node;
	WN * acc_copy_nodes;		/* Points to (optional) copy nodes */
	WN * acc_copyin_nodes;	/* Points to (optional) copyin nodes */
	WN * acc_copyout_nodes; /* Points to (optional) copyout nodes */	
	WN * acc_create_nodes;	/*copy/copyin/copyout/create will merge into pcopy/pcopyin/pcopyout/pcreate*/
	/////////////////////////////////////////////////////////
	WN * acc_present_nodes;
	vector<ACC_DATA_ST_MAP*> presentMap;
	WN * acc_present_or_copy_nodes;
	vector<ACC_DATA_ST_MAP*> pcopyMap;
	WN * acc_present_or_copyin_nodes;
	vector<ACC_DATA_ST_MAP*> pcopyinMap;
	WN * acc_present_or_copyout_nodes;
	vector<ACC_DATA_ST_MAP*> pcopyoutMap;
	WN * acc_present_or_create_nodes;
	vector<ACC_DATA_ST_MAP*> pcreateMap;
	WN * acc_deviceptr_nodes;
	vector<ST*> dptrList;
	WN* acc_async;
	WN* acc_num_gangs;
	WN* acc_num_workers;
	WN* acc_vector_length;
	WN* acc_reduction_nodes;
	//this is used to process the reduction operations in the top parallel construct level, not in the loop
	vector<ACC_ReductionMap> reductionmap;
	//vector<ACC_DATA_ST_MAP*> reductionMap;
	WN* acc_private;
	vector<ACC_DATA_ST_MAP*> privateMap;
	WN* acc_firstprivate;
	vector<ACC_DATA_ST_MAP*> firstPrivateMap;	
	WN* acc_param; 
}ParallelRegionInfo;

typedef enum {
  ACC_VAR_NONE        = 0,
  ACC_VAR_LOCAL       = 1,	//declare in the kernel function
  ACC_VAR_COPY        = 2,
  ACC_VAR_COPYIN      = 3,
  ACC_VAR_COPYOUT     = 4,
  ACC_VAR_REDUCTION   = 5,  
  ACC_VAR_VECTOR_REDUCTION   = 6
} ACC_VAR_TYPE;

typedef struct {
  ACC_VAR_TYPE   vtype;
  TYPE_ID    mtype;
  BOOL       has_offset;
  
  TY_IDX     ty;
  
  ST        *orig_st;
  WN_OFFSET  orig_offset;
  ST        *new_st;
  WN_OFFSET  new_offset;
  OPERATOR   reduction_opr; /* specified in REDUCTION pragma */
} ACC_VAR_TABLE;

//This table is used to localize the ST  which is used in kernel/parallel region.
//static ACC_VAR_TABLE* acc_local_var_table;		//All the data in the kernel function
extern map<ST*, ACC_VAR_TABLE> acc_local_new_var_map;     //used to replace var st in kernel function
//static map<ST*, ST*> acc_local_new_var_map;     //used to replace var st in kernel function
extern map<ST*, ACC_VAR_TABLE> acc_local_new_var_map_host_data;     //used to replace var st in host_data region

typedef struct ACC_SCALAR_VAR_INFO
{
	ST* st_var;
	ACC_SCALAR_TYPE acc_scalar_type;
	//no value  when acc_scalar_type is ACC_SCALAR_VAR_IN and ACC_SCALAR_VAR_PRIVATE
	ST* st_device_in_host;
	//no value  when acc_scalar_type is ACC_SCALAR_VAR_PRIVATE
	//pointer type when acc_scalar_type is ACC_SCALAR_VAR_OUT, ACC_SCALAR_VAR_INOUT,ACC_SCALAR_VAR_REDUCTION
	//scalar value when acc_scalar_type is ACC_SCALAR_VAR_IN
	ST* st_device_in_kparameters; 
	//scalar type
	ST* st_device_in_klocal;
	BOOL is_reduction; //reduction variable can also be one of ACC_SCALAR_TYPE.
	BOOL is_across_gangs; //if this reduction is across gangs, it requires another reduction kernel launch.
	OPERATOR opr_reduction;
	UINT32 isize; //size in bytes, transfer between host and device
	//if the scalar var is specified the copy/copyin/copyout/create, then bcreate_by_previous=TRUE
	//it is set in ACC_Find_Scalar_Var_Inclose_Data_Clause
	BOOL bcreate_by_previous;

	ACC_SCALAR_VAR_INFO()
	{
		bcreate_by_previous = FALSE;
		is_reduction = FALSE;
		is_across_gangs = FALSE;
	}
}ACC_SCALAR_VAR_INFO;

#define ACC_GET_RETURN_PREGS(rreg1, rreg2, type) \
  acc_my_Get_Return_Pregs(&rreg1, &rreg2, type, __FILE__, __LINE__)

//this one is to manage the scalar variable automatically generated by compiler DFA.
extern map<ST*, ACC_SCALAR_VAR_INFO*> acc_offload_scalar_management_tab;

extern map<ST*, ACC_SCALAR_VAR_INFO*> acc_offload_params_management_tab;

extern vector<ACC_DREGION__ENTRY> acc_unspecified_dregion_pcopy;

extern vector<ACC_DATA_ST_MAP*> acc_unspecified_pcopyMap;

extern BOOL acc_kernel_launch_debug_mode;
extern BOOL acc_dfa_enabled;
extern ST* st_shared_array_4parallelRegion ;
extern TY_IDX ty_shared_array_in_parallel;
extern DST_INFO_IDX  acc_nested_dst;

extern WN *acc_pragma_block;	/* Parallel funciton pragma block */
extern WN *acc_reference_block;	/* Parallel funciton reference block */
extern WN *acc_parallel_func;	/* General Kernels function */

typedef struct KernelParameter
{
	ST* st_host;  //host memory space
	ST*	st_device;//still on host side Symbol table which points to device memory space
	//ACC_DU_Liveness acc_inout;
	
}KernelParameter;
//used to create kernel parameters when outline kernel function.
extern vector<KernelParameter> acc_kernelLaunchParamList;
//main for reduction
extern vector<KernelParameter> acc_additionalKernelLaunchParamList; 

extern ST * st_is_pcreate;
extern WN_OFFSET ofst_st_is_pcreate;
extern BOOL is_pcreate_tmp_created;

extern vector<ST *>  kernel_param;
extern map<ST*, BOOL> acc_const_offload_ptr;

extern vector<ACC_DREGION__ENTRY> acc_dregion_pcreate;
extern vector<ACC_DREGION__ENTRY> acc_dregion_pcopy;
extern vector<ACC_DREGION__ENTRY> acc_dregion_pcopyin;
extern vector<ACC_DREGION__ENTRY> acc_dregion_pcopyout;
extern vector<ACC_DREGION__ENTRY> acc_dregion_present;
extern vector<ACC_DREGION__ENTRY> acc_dregion_host;
extern vector<ACC_DREGION__ENTRY> acc_dregion_device;
extern vector<ACC_DREGION__ENTRY> acc_dregion_private;
extern vector<ACC_DREGION__ENTRY> acc_dregion_fprivate;//first private
extern vector<ACC_DREGION__ENTRY> acc_dregion_lprivate;//last private
extern vector<ACC_DREGION__ENTRY> acc_dregion_inout_scalar;
extern vector<ACC_DREGION__ENTRY> acc_dregion_delete;
extern vector<ACC_DREGION__ENTRY> acc_host_data_use_device;	//for host_data region


//Function Declare
extern void ACC_Push_Some_Globals( );
extern void ACC_Pop_Some_Globals( );
extern void ACC_Create_Func_DST ( char * st_name );

extern void acc_dump_scalar_management_tab();
extern WN* ACC_Make_Array_ref(ST *base, WN* wn_offset, WN* wn_dim);
extern WN * ACC_Walk_and_Localize (WN * tree);
extern void ACC_Process_scalar_variable_for_offload_region(WN* wn_block, 
				BOOL bIsKernels);
extern BOOL ACC_Check_Pointer_Exist_inClause(ST* st_param, void* pRegionInfo, BOOL isKernelRegion);

void ACC_Process_Clause_Pragma(WN * tree);
extern WN* ACC_Process_KernelsRegion( WN * tree, WN* wn_cont);
extern WN* ACC_Process_ParallelRegion( WN * tree, WN* wn_cont);
extern WN* ACC_Process_HostDataRegion( WN * tree );
extern WN* ACC_Process_WaitRegion( WN * tree );
extern WN* ACC_Process_UpdateRegion( WN * tree );
extern WN* ACC_Process_CacheRegion( WN * tree );
extern WN* ACC_Process_DeclareRegion( WN * tree );
extern WN* ACC_Process_ExitData_Region( WN * tree );
extern WN* ACC_Process_EnterData_Region( WN * tree );
extern WN* ACC_Process_DataRegion( WN * tree );


/*********************************************************************************************************
***********************************************PRAGMA NODES********************************************
**********************************************************************************************************/
/* async int expression */
extern WN* acc_AsyncExpr;
extern WN* acc_async_nodes;   
extern WN* acc_clause_intnum;
extern WN *acc_host_nodes;
extern WN *acc_device_nodes;
extern WN *acc_stmt_block;		/* Original statement nodes */
extern WN *acc_cont_nodes;		/* Statement nodes after acc code */
extern WN *acc_if_node;		/* Points to (optional) if node */
extern WN *acc_num_gangs_node;
extern WN *acc_num_workers_node;
extern WN *acc_vector_length_node;
extern WN *acc_collapse_node;
extern WN *acc_gang_node;
extern WN *acc_worker_node;
extern WN *acc_vector_node;
extern WN *acc_seq_node;
extern WN *acc_independent_node;

extern WN *acc_reduction_nodes;	/* Points to (optional) reduction nodes */
extern vector<WN*> acc_wait_list;
extern WN *acc_copy_nodes;		/* Points to (optional) shared nodes */
extern WN *acc_copyin_nodes;	/* Points to (optional) copyin nodes */
extern WN *acc_copyout_nodes;	/* Points to (optional) copyout nodes */
extern WN *acc_wait_nodes;	/* Points to (optional) acc wait pragma nodes */
extern WN *acc_parms_nodes;	/* Points to (optional) parmeter nodes */
extern WN *acc_create_nodes;
extern WN *acc_present_nodes;
extern WN *acc_present_or_copy_nodes;
extern WN *acc_present_or_copyin_nodes;
extern WN *acc_present_or_copyout_nodes;
extern WN *acc_present_or_create_nodes;
extern WN *acc_deviceptr_nodes;
extern WN *acc_private_nodes;
extern WN *acc_firstprivate_nodes;
extern WN *acc_lastprivate_nodes;//basically, it is used for copyout
extern WN *acc_inout_nodes;
extern WN *acc_delete_nodes;
extern WN *acc_use_device_nodes;

extern BOOL acc_set_gangs;
extern BOOL acc_set_workers;
extern BOOL acc_set_vector_length;

/*********************************************************************************************************
***********************************************runtime gen API********************************************
**********************************************************************************************************/
extern WN * Gen_DeviceMalloc( ST* st_hmem, ST *st_dmem, WN* wnSize) ;
extern WN * Gen_DataD2H (ST *Src, ST *Dst, WN* wnSize, WN* wnStart);
extern WN * Gen_DataH2D (ST *Src, ST *Dst, WN* wnSize, WN* wnStart);
extern WN* Gen_ACC_Stack_Push();
extern WN* Gen_ACC_Stack_Pop();
extern WN* Gen_ACC_Free_Device_Ptr_InCurrentStack();
extern WN* Gen_ACC_Pending_DevicePtr_To_Current_Stack(ST *st_dmem, BOOL bIsReduction=FALSE);
extern WN* ACC_Remove_All_ACC_Pragma(WN* node);
extern ACC_DATA_ST_MAP* ACC_GenSingleCreateAndMallocDeviceMem(ACC_DREGION__ENTRY dEntry, 
						 vector<ACC_DATA_ST_MAP*>* pDMap, WN* ReplacementBlock);
extern WN* ACC_GenIsPCreate(ACC_DREGION__ENTRY dEntry, ST* st_device, unsigned int type_size);
extern ST* ACC_GenDeclareSingleDeviceMem(ACC_DREGION__ENTRY dentry, 
						 vector<ACC_DATA_ST_MAP*>* pDMap);
extern WN* ACC_Gen_GetDeviceAddr(ACC_DREGION__ENTRY dentry, ST* st_device, unsigned int type_size);
extern void ACC_GenWaitNodes(vector<WN*> *pWaitlist, WN* ReplacementBlock);
extern void ACC_GenPresentNode(vector<ACC_DREGION__ENTRY>* pDREntries, vector<ACC_DATA_ST_MAP*>* pDMap, 
								 WN* ReplacementBlock);
extern void ACC_GenDevicePtr(WN* nodes, vector<ST*>* pDMap);

extern void ACC_GenDeviceCreateCopyInOut(vector<ACC_DREGION__ENTRY>* pDREntries, 
								 vector<ACC_DATA_ST_MAP*>* pDMap, 
								 WN* ReplacementBlock, BOOL MemIn, BOOL isStructure = TRUE);
extern WN* ACC_GenFreeDeviceMemory(ST* st_device_mem);
extern void ACC_GenDeviceMemFreeInBatch(vector<ACC_DATA_ST_MAP*>* pDMap, WN* ReplacementBlock);
extern void ACC_GenDataCopyOut(vector<ACC_DATA_ST_MAP*>* pDMap, WN* ReplacementBlock);
extern WN* ACC_GenDataExitDelete(ST* st_Var, WN* wn_start, 
							 WN* wn_length, unsigned int type_size);
extern WN* ACC_GenUpdateHostVar(ST* st_Var, WN* wn_start, 
							 WN* wn_length, unsigned int type_size);
extern WN* ACC_GenUpdateDeviceVar(ST* st_Var, WN* wn_start,
						 WN* wn_length, unsigned int type_size);
extern WN* ACC_GenWaitStream(WN* wn_int_expr);
extern void ACC_Process_scalar_variable_for_offload_region(WN* wn_block=NULL, 
				BOOL bIsKernels=FALSE);
extern WN* ACC_Find_Scalar_Var_Inclose_Data_Clause(void* pRegionInfo, BOOL isKernelRegion);
extern void Finalize_Kernel_Parameters();
extern WN* ACC_GenGetTotalNumGangsWorkers();
extern WN* ACC_GenGetTotalNumGangs();
extern WN* ACC_GenGetTotalNumVectors();
extern void ACC_Setup_GPU_toplogy_kernels(WN* replace_block);
extern void ACC_Setup_GPU_toplogy_parallel(ParallelRegionInfo* pPRInfo, WN* replace_block);
extern void ACC_Setup_GPU_toplogy_1block_1thread(WN* replace_block);
extern WN* Gen_Reset_Default_Toplogy();
extern WN* Gen_Set_Default_Toplogy();
extern WN* Gen_Set_Vector_Num_Z(WN* wn_num);
extern WN* Gen_Set_Vector_Num_Y(WN* wn_num);
extern WN* Gen_Set_Vector_Num_X(WN* wn_num);
extern WN* Gen_Set_Gangs_Num_Y(WN* wn_num);
extern WN* Gen_Set_Gangs_Num_Z(WN* wn_num);
extern WN* Gen_Set_Gangs_Num_X(WN* wn_num);
extern WN* ACC_Gen_Dim_Init_Call(ST* st_dim);
extern WN* Gen_Sync_Threads();
extern WN* ACC_Gen_Call_Local_Reduction(ST* st_device_func, ST* st_inputdata);
extern WN* ACC_Gen_Reduction_Dim_Init_Call(ST* st_dim, ST* st_blocksize);
extern WN* GenReductionMalloc(ST* st_device, WN* wnSize);
extern void Transform_ACC_Parallel_Block ( WN * tree, ParallelRegionInfo* pPRInfo, WN* wn_replace_block);
extern void Transform_ACC_Parallel_Block_New ( WN * tree, WN* wn_replace_block, 
										WN* wn_num_gangs, WN* wn_num_workers, WN* wn_vector_length);
extern void Transform_ACC_Kernel_Block_New ( WN * tree, WN* wn_replace_block);
extern void Transform_ACC_Kernel_Block ( WN * tree, KernelsRegionInfo* pKRInfo, WN* wn_replace_block);
extern void ACC_Fix_Dependence_Graph(PU_Info *parent_pu_info, PU_Info *child_pu_info, WN *child_wn);
extern  WN* ACC_Launch_HSA_Kernel(int index, WN* wn_replace_block);
extern  WN* ACC_LaunchKernel_nvidia (int index, WN* wn_replace_block, BOOL bParallel);
extern  WN* ACC_LaunchKernelEx_nvidia (int index, WN* wn_replace_block, BOOL bParallel);


/*********************************************************************************************************
***********************************************Reduction Operations***************************************
**********************************************************************************************************/
typedef enum ReductionUsingMem
{
	ACC_RD_SHARED_MEM=0,
	ACC_RD_GLOBAL_MEM
}ReductionUsingMem;

typedef enum ReductionRolling
{
	ACC_RD_UNROLLING=0,
	ACC_RD_ROLLING
}ReductionRolling;

typedef enum ReductionExeMode
{
	ACC_RD_LAUNCH_KERNEL,
	ACC_RD_DYNAMICPARAL_EXE	
}ReductionExeMode;
extern map<TYPE_ID, ST*> acc_global_memory_for_reduction_host;		//used in host side
extern map<TYPE_ID, ST*> acc_global_memory_for_reduction_device;	//used in host side
extern map<TYPE_ID, ST*> acc_global_memory_for_reduction_param;	//used in kernel parameters
extern map<TYPE_ID, ST*> acc_global_memory_for_reduction_block;	//used in a single block

//The following are using for shared memory 
extern map<TYPE_ID, ST*> acc_shared_memory_for_reduction_block;	//used in device side

//////////////////////////////////////////////////////////////////////////
extern map<ST*, ACC_ReductionMap> acc_reduction_tab_map; //ST in Host side


extern ReductionUsingMem acc_reduction_mem;
extern ReductionRolling acc_reduction_rolling;
extern ReductionExeMode acc_reduction_exemode;

typedef struct acc_reduction_kernels_pair
{
	ST* st_kernels_fun;
	OPERATOR ReductionOpr; 
	TY_IDX opr_ty; //double, float, int, short, etc. It's scalar variables.
	ACC_LOOP_TYPE looptype;
}acc_reduction_kernels_pair;

extern vector<acc_reduction_kernels_pair> acc_reduction_kernels_maps;
extern vector<acc_reduction_kernels_pair> acc_reduction_devices_maps; //devices function for reduction
extern map<ST*, ST*> acc_reduction_device_reduction_call;

extern ST *acc_reduction_proc;	/* reduction for ACC process */
extern WN *acc_reduction_func;	/* reduction kernel function */
extern WN *acc_replace_block;	/* Replacement nodes to be returned */

extern void GenFinalReductionAlgorithm_APU(ST* st_dbuffer, ST* st_dhost, 
				ST* st_reduction_kernel_name, ST* st_num_of_element, UINT32 iTypesize, 
				WN* wn_replace_block);
extern void ACC_ProcessReduction_Parallel(ParallelRegionInfo* pPRInfo, WN* wn_replace_block);
extern WN* GenFinalReductionAlgorithm_nvidia(ST* st_dbuffer, ST* st_dhost, 
				ST* st_reduction_kernel_name, ST* st_num_of_element, UINT32 iTypesize);

extern WN* ACC_Get_Init_Value_of_Reduction(OPERATOR ReductionOpr, TYPE_ID rtype);
extern void ACC_Reduction4ParallelConstruct(ParallelRegionInfo* pPRInfo);
extern void ACC_ProcessReduction_Inside_offloadRegion(WN* wn_replace_block, 
										ACC_ReductionMap* pReductionmap);
extern void ACC_Parallel_Loop_Reduction_Extract_Public (WN * tree, WN* wn_replace_block);
extern ACC_ReductionMap* ACC_Get_ReductionMap(WN* wn);

//preprocess the reductions appearing in top-level loop
//the reduction buff will join the kernel launch parameters.
typedef struct FOR_LOOP_RDC_INFO
{
	ACC_LOOP_TYPE looptype;
	vector<ACC_ReductionMap*> reductionmap;
}FOR_LOOP_RDC_INFO;

void ACC_Parallel_Loop_Reduction_Extract_Public (WN * tree, WN* wn_replace_block);
extern vector<FOR_LOOP_RDC_INFO> acc_top_level_loop_rdc;


/*********************************************************************************************************
********************************************Loop Scheduling Definition************************************
**********************************************************************************************************/
extern WN * ACC_Transform_MultiForLoop(KernelsRegionInfo* pKRInfo);
extern WN * ACC_Linear_Stmts_Transformation(WN* wn_stmt, KernelsRegionInfo* pKRInfo);
extern WN * ACC_Transform_SingleForLoop(ParallelRegionInfo* pPRInfo, WN* wn_replace_block);
extern void ACC_Handle_Parallel_Loops( WN* stmt_block, ParallelRegionInfo* pPRInfo, WN* wn_replace_block);
extern void ACC_Handle_Kernels_Loops( WN* stmt_block, KernelsRegionInfo* pKRInfo, WN* wn_replace_block);
extern void ACC_Global_Shared_Memory_Reduction(WN* wn_replace_block);
extern void Identify_Loop_Scheduling(WN* tree, WN* replace_block, BOOL bIsKernels);
extern WN* ACC_Loop_Scheduling_Transformation_Gpu(WN* tree, WN* wn_replace_block);
extern void ACC_Setup_GPU_toplogy_for_Parallel(WN* wn_gangs, WN* wn_workers, 
 									WN* wn_vector_length, WN* replace_block);
extern void ACC_Rewrite_Collapsed_Do (WN* wn_start_block, WN* wn_indices_init_block);

#ifdef __cplusplus
}
#endif

#endif

