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

ST * st_is_pcreate;
WN_OFFSET ofst_st_is_pcreate;
BOOL is_pcreate_tmp_created = FALSE;

 /*  This table contains ST_IDX entries entries for each of the ACC
	 runtime routines.	These entries allow efficient sharing of all
	 calls to a particular runtime routine. */
 const char *accr_names [ACCRUNTIME_LAST + 1] = {
   "",				 /* MPRUNTIME_NONE */
   "__accr_setup",			 /* ACCR_SETUP */
   "__accr_cleanup",	 /* ACCR_CLEANUP */
   "__accr_sync",		 /* ACCR_SYNC */
   "__accr_malloc_on_device",	 /* ACCR_CUDAMALLOC */
   "__accr_memin_h2d",		 /*ACCR_CUDAMEMIN*/
   "__accr_memout_d2h", 	 /*ACCR_CUDAMEMOUT*/
   "__accr_launchkernel",	 /*ACCR_LAUNCHKERNEL*/
   "__accr_push_kernel_param_pointer", /*ACCR_KERNELPARAMPUSH*/
   "__accr_present_copy",		 /*ACCR_PRESENT_COPY*/
   "__accr_present_copyin", 	 /*ACCR_PRESENT_COPYIN*/
   "__accr_present_copyout",	 /*ACCR_PRESENT_COPYOUT*/
   "__accr_present_create", 	 /*ACCR_PRESENT_CREATE*/
   "__accr_set_gang_num_x", 	 /*ACCR_SET_GANG_NUM_X*/
   "__accr_set_gang_num_y", 	 /*ACCR_SET_GANG_NUM_Y*/
   "__accr_set_gang_num_z", 	 /*ACCR_SET_GANG_NUM_Z*/
   "__accr_set_vector_num_x",	 /*ACCR_SET_VECTOR_NUM_X*/
   "__accr_set_vector_num_y",	 /*ACCR_SET_VECTOR_NUM_Y*/
   "__accr_set_vector_num_z",	 /*ACCR_SET_VECTOR_NUM_Z*/
   "__accr_dmem_release",		 /*ACCR_DMEM_RELEASE*/
   "__accr_map_data_region",	 /*map data region*/  
   "__accr_push_kernel_param_scalar", 
   "__accr_push_kernel_param_double",
   "__accr_reduction_buff_malloc",
   "__accr_final_reduction_algorithm",
   "__accr_free_on_device",
   "__accr_set_default_gang_vector",
   "__accr_set_gangs",
   "__accr_set_vectors",
   "__accr_reset_default_gang_vector",
   "__accr_get_device_addr",
   "__accr_update_host_variable",
   "__accr_update_device_variable",
   "__accr_wait_some_or_all_stream",
   "__syncthreads",
   "printf",
   "__accr_get_num_gangs",
   "__accr_get_num_workers",
   "__accr_get_num_vectors",
   "__accr_get_total_num_vectors",
   "__accr_get_total_num_gangs",
   "__accr_get_total_gangs_workers",
   "__accr_call_local_reduction",
   "__accr_dynamic_launch_kernel",
   "__accr_stack_push",
   "__accr_stack_pop",
   "__accr_stack_pending_to_current_stack",
   "__accr_stack_clear_device_ptr_in_current_stack",
   "__accr_data_exit_copyout",
   "__accr_data_exit_delete",
   "__accr_free_reduction_buff",
   "__accr_init_launch_params",
   "__accr_init_launch_reduction_params"
 };
 
 
 /*  This table contains ST_IDX entries entries for each of the MP
	 runtime routines.	These entries allow efficient sharing of all
	 calls to a particular runtime routine. */
 
 ST_IDX accr_sts [ACCRUNTIME_LAST + 1] = {
   ST_IDX_ZERO,   /* ACCRUNTIME_NONE */
   ST_IDX_ZERO,   /* ACCR_SETUP */
   ST_IDX_ZERO,   /* ACCR_CLEANUP */
   ST_IDX_ZERO,   /* ACCR_SYNC */
   ST_IDX_ZERO,   /* ACCR_CUDAMALLOC */
   ST_IDX_ZERO,   /* ACCR_CUDAMEMIN */
   ST_IDX_ZERO,   /* ACCR_CUDAMEMOUT */
   ST_IDX_ZERO,   /*ACCR_LAUNCHKERNEL*/
   ST_IDX_ZERO,   /*ACCR_KERNELPARAMPUSH*/
   ST_IDX_ZERO,  /*ACCR_PRESENT_COPY*/
   ST_IDX_ZERO,  /*ACCR_PRESENT_COPYIN*/
   ST_IDX_ZERO,  /*ACCR_PRESENT_COPYOUT*/
   ST_IDX_ZERO,  /*ACCR_PRESENT_CREATE*/
   ST_IDX_ZERO,  /*ACCR_SET_GANG_NUM_X*/
   ST_IDX_ZERO,  /*ACCR_SET_GANG_NUM_Y*/
   ST_IDX_ZERO,  /*ACCR_SET_GANG_NUM_Z*/
   ST_IDX_ZERO,  /*ACCR_SET_VECTOR_NUM_X*/
   ST_IDX_ZERO,  /*ACCR_SET_VECTOR_NUM_Y*/
   ST_IDX_ZERO,  /*ACCR_SET_VECTOR_NUM_Z*/
   ST_IDX_ZERO,  /*ACCR_DMEM_RELEASE*/
   ST_IDX_ZERO,  /*MAP DATA REGION*/
   ST_IDX_ZERO,  /*ACCR_PUSH_KERNEL_PARAM_INT*/
   ST_IDX_ZERO,  /*ACCR_PUSH_KERNEL_PARAM_DOUBLE*/
   ST_IDX_ZERO,  /*ACCR_REDUCTION_BUFF_MALLOC*/
   ST_IDX_ZERO,  /*ACCR_FINAL_REDUCTION_ALGORITHM*/
   ST_IDX_ZERO,  /**ACCR_FREE_ON_DEVICE*/
   ST_IDX_ZERO,  /*ACCR_SETUP_DEFAULT_TOLOGY*/
   ST_IDX_ZERO,  /*ACCR_SETUP_GANG_TOLOGY*/
   ST_IDX_ZERO,  /*ACCR_SETUP_VECTOR_TOLOGY*/
   ST_IDX_ZERO,  /*ACCR_RESET_DEFAULT_TOLOGY*/
   ST_IDX_ZERO,  /*ACCR_GET_DEVICE_ADDR*/
   ST_IDX_ZERO,  /*ACCR_UPDATE_HOST_VARIABLE*/
   ST_IDX_ZERO,  /*ACCR_UPDATE_DEVICE_VARIABLE*/
   ST_IDX_ZERO,  /*ACCR_WAIT_SOME_OR_ALL_STREAM*/
   ST_IDX_ZERO,  /*ACCR_SYNCTHREADS*/
   ST_IDX_ZERO,  /*FOR DEBUG*/
   ST_IDX_ZERO,  /*ACCR_GET_NUM_GANGS*/
   ST_IDX_ZERO,  /*ACCR_GET_NUM_WORKERS*/
   ST_IDX_ZERO,  /*ACCR_GET_NUM_VECTORS*/
   ST_IDX_ZERO,  /*ACCR_GET_TOTAL_VECTORS*/
   ST_IDX_ZERO,   /*ACCR_GET_TOTAL_GANGS*/
   ST_IDX_ZERO,  /*ACCR_GET_TOTAL_GANGS_WORKERS*/
   ST_IDX_ZERO,  /*ACCR_CALL_LOCAL_REDUCTION*/
   ST_IDX_ZERO,  /*ACCR_DYNAMIC_LAUNCH_KERNEL*/
   ST_IDX_ZERO,  /*ACCR_STACK_PUSH*/
   ST_IDX_ZERO,  /*ACCR_STACK_POP*/
   ST_IDX_ZERO,  /*ACCR_STACK_PENDING_TO_CURRENT_STACK*/
   ST_IDX_ZERO,  /*ACCR_STACK_CLEAR_DEVICE_PTR_IN_CURRENT_STACK*/
   ST_IDX_ZERO,  /*ACCR_DATA_EXIT_COPYOUT*/
   ST_IDX_ZERO,  /*ACCR_DATA_EXIT_DELETE*/
   ST_IDX_ZERO,  /*ACCR_FREE_REDUCTION_BUFF*/
   ST_IDX_ZERO,  /*ACCR_INIT_LAUNCH_PARAMS*/
   ST_IDX_ZERO	 /*ACCR_INIT_LAUNCH_RED_PARAMS*/
 };
 
// Generic type for parallel runtime routines
TY_IDX accruntime_ty = TY_IDX_ZERO;
 ST_IDX Make_ACCRuntime_ST ( OACCRUNTIME rop )
 {
   Is_True(rop >= ACCRUNTIME_FIRST && rop <= ACCRUNTIME_LAST,
		   ("Make_ACCRuntime_ST: bad rop == %d", (INT) rop));
 
	 // If the global type doesn't exist, create it and its pointer type.
   if (accruntime_ty == TY_IDX_ZERO) {
	 TY &mpr_ty = New_TY ( accruntime_ty );
	 TY_Init(mpr_ty, 0, KIND_FUNCTION, MTYPE_UNKNOWN,
			 Save_Str(".accruntime"));
	 Set_TY_align(accruntime_ty, 1);
 
	 TYLIST_IDX parm_idx;
	 TYLIST& parm_list = New_TYLIST(parm_idx);
	 Set_TY_tylist(mpr_ty, parm_idx);
	 Set_TYLIST_type(parm_list, Be_Type_Tbl(MTYPE_I4));  // I4 return type
	   // are there really no parameters? -- DRK
	 Set_TYLIST_type(New_TYLIST(parm_idx), TY_IDX_ZERO); // end of parm list
 
	 TY_IDX ty_idx;
	 TY &ty = New_TY ( ty_idx );
	 TY_Init(ty, Pointer_Size, KIND_POINTER, Pointer_Mtype,
	   Save_Str ( ".accruntime_ptr" ));
	 Set_TY_pointed(ty, accruntime_ty);
 
	 Set_TY_align(ty_idx, Pointer_Size); // unnecessary? TY_Init does
										 // not set alignment -- DRK
   }
 
   PU_IDX pu_idx;
   PU& pu = New_PU(pu_idx);
   PU_Init(pu, accruntime_ty, CURRENT_SYMTAB);
 
   /*  Create the ST, fill in all appropriate fields and enter into the */
   /*  global symbol table.  */
 
   ST *st = New_ST ( GLOBAL_SYMTAB );
   ST_Init(st, Save_Str ( accr_names[rop] ), CLASS_FUNC, SCLASS_EXTERN,
	 EXPORT_PREEMPTIBLE, pu_idx);
 
   Allocate_Object ( st );
 
   accr_sts[rop] = ST_st_idx(*st);
   return accr_sts[rop];
 }
 
 /*device Malloc memory*/
 WN * Gen_DeviceMalloc( ST* st_hmem, ST *st_dmem, WN* wnSize) 
 {
	 WN * wn;
	 WN* wnx;
	 wn = WN_Create(OPC_VCALL, 4);	 
	 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_DEVICEMEMMALLOC);
   
	 WN_Set_Call_Non_Data_Mod(wn);
	 WN_Set_Call_Non_Data_Ref(wn);
	 WN_Set_Call_Non_Parm_Mod(wn);
	 WN_Set_Call_Non_Parm_Ref(wn);
	 WN_Set_Call_Parm_Ref(wn);
 
	 //Scalar/Array/Pointer
	 //if the host is multi dim array, it will be different    
	 if(F90_ST_Has_Dope_Vector(st_hmem))
		 wnx = WN_Ldid(Pointer_type, 0, st_hmem, ST_type(st_dmem));
	 else if(TY_kind(ST_type(st_hmem)) == KIND_ARRAY)
		 wnx = WN_Lda( Pointer_type, 0, st_hmem);
	 else if(TY_kind(ST_type(st_hmem)) == KIND_POINTER)
		 wnx = WN_Ldid(Pointer_type, 0, st_hmem, ST_type(st_hmem));  
	 else if(TY_kind(ST_type(st_hmem)) == KIND_SCALAR)
		 wnx = WN_Lda( Pointer_type, 0, st_hmem);
	 
	 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
						WN_ty(wnx), WN_PARM_BY_VALUE);
	 
	 wnx = WN_Lda( Pointer_type, 0, st_dmem);
	 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wnx, 
						WN_ty(wnx), WN_PARM_BY_REFERENCE);
	 //
	 WN_kid(wn, 2) = WN_CreateParm(MTYPE_U4, wnSize, 
		   Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE); 
 
	 
	 char* strname = (char*) alloca ( strlen(ST_name(st_hmem))+ strlen(ST_name(st_dmem)) + 10);
	 sprintf ( strname, "%s : %s \0", ST_name(st_hmem), ST_name(st_dmem));
	 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
	 WN_kid(wn, 3) = WN_CreateParm(Pointer_type, wn_strname, 
								 WN_ty(wn_strname), WN_PARM_BY_VALUE);
	 
	 return wn;
 }
 
 
 WN *
 Gen_DataD2H (ST *Src, ST *Dst, WN* wnSize, WN* wnStart) 
 {
   WN * wn;
   WN * wnx;
   wn = WN_Create(OPC_VCALL, 6 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_DEVICEMEMOUT);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
 
   wnx = WN_Ldid(Pointer_type, 0, Src, ST_type(Src));
 
 
   //WN* multiArrayT; //
 
   //wnx = WN_Lda( Pointer_type, 0, Src);
   WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
						WN_ty(wnx), WN_PARM_BY_REFERENCE);
 
   //if the host is multi dim array, it will be different	 
	 if(F90_ST_Has_Dope_Vector(Dst))
		 wnx = WN_Ldid(Pointer_type, 0, Dst, ST_type(Src));
	 else if(TY_kind(ST_type(Dst)) == KIND_ARRAY)
		 wnx = WN_Lda( Pointer_type, 0, Dst);
	 else if(TY_kind(ST_type(Dst)) == KIND_POINTER)
		 wnx = WN_Ldid(Pointer_type, 0, Dst, ST_type(Dst));
	 else if(TY_kind(ST_type(Dst)) == KIND_SCALAR)
		 wnx = WN_Lda( Pointer_type, 0, Dst);
   //if(ACC_Get_Array_TotalDim(wnx) > 1)
   //	 wnx = ACC_Load_MultiDimArray_StartAddr(wnx);
   //wnx = WN_Lda( Pointer_type, 0, Dst);
   WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wnx, 
						WN_ty(wnx), WN_PARM_BY_REFERENCE);
   
   WN_kid(wn, 2) = WN_CreateParm(MTYPE_U4, wnSize, 
		   Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
   
   WN_kid(wn, 3) = WN_CreateParm(MTYPE_U4, wnStart, 
		   Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
   
   if(acc_AsyncExpr)
   {
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(acc_AsyncExpr), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   }
   else 
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_I4, WN_Intconst(MTYPE_I4, -2), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
 
   
	 char* strname = (char *) alloca ( strlen(ST_name(Dst))+ strlen(ST_name(Src)) + 10);
	 sprintf ( strname, "%s : %s \0", ST_name(Dst), ST_name(Src));
	 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
	 WN_kid(wn, 5) = WN_CreateParm(Pointer_type, wn_strname, 
								 WN_ty(wn_strname), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
 WN *
 Gen_DataH2D (ST *Src, ST *Dst, WN* wnSize, WN* wnStart) 
 {
   WN * wn;
   WN * wnx;
   wn = WN_Create(OPC_VCALL, 6);
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_DEVICEMEMIN);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
 
   if(F90_ST_Has_Dope_Vector(Src))
	   wnx = WN_Ldid(Pointer_type, 0, Src, ST_type(Dst));
   else if(TY_kind(ST_type(Src)) == KIND_ARRAY)
	   wnx = WN_Lda( Pointer_type, 0, Src);
   else  if(TY_kind(ST_type(Src)) == KIND_POINTER)
	   wnx = WN_Ldid(Pointer_type, 0, Src, ST_type(Src));
   else if(TY_kind(ST_type(Src)) == KIND_SCALAR)
	   wnx = WN_Lda( Pointer_type, 0, Src);
   //WN* multiArrayT;
   //if(ACC_Get_Array_TotalDim(wnx) > 1)
   //	 multiArrayT = ACC_Load_MultiDimArray_StartAddr(wnx);
   //WN* multiArrayT = ACC_Load_MultiDimArray_StartAddr(wnx);
   //wnx = WN_Lda( Pointer_type, 0, Src);
   WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
						WN_ty(wnx), WN_PARM_BY_REFERENCE);
 
   
   wnx = WN_Ldid(Pointer_type, 0, Dst, ST_type(Dst));
 
   //wnx = WN_Lda( Pointer_type, 0, Dst);
   WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wnx, 
						WN_ty(wnx), WN_PARM_BY_REFERENCE);
   
   WN_kid(wn, 2) = WN_CreateParm(MTYPE_U4, wnSize, 
		   Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
   
   WN_kid(wn, 3) = WN_CreateParm(MTYPE_U4, wnStart, 
		   Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
   
   if(acc_AsyncExpr)
   {
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(acc_AsyncExpr), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   }
   else 
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_I4, WN_Intconst(MTYPE_I4, -2), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   
	 char* strname = (char *) alloca ( strlen(ST_name(Src))+ strlen(ST_name(Dst)) + 10);
	 sprintf ( strname, "%s : %s \0", ST_name(Src), ST_name(Dst));
	 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
	 WN_kid(wn, 5) = WN_CreateParm(Pointer_type, wn_strname, 
								 WN_ty(wn_strname), WN_PARM_BY_VALUE);
 
   return wn;
 }

 
 
 WN* Gen_Push_KernelParamPointer(WN* wn_param, WN* wn_str_var)
 {
	 WN* wn = WN_Create(OPC_VCALL, 2);
	 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_KERNELPARAMPUSH_POINTER);
	 
	 WN_Set_Call_Non_Data_Mod(wn);
	 WN_Set_Call_Non_Data_Ref(wn);
	 WN_Set_Call_Non_Parm_Mod(wn);
	 WN_Set_Call_Non_Parm_Ref(wn);
	 WN_Set_Call_Parm_Ref(wn);
	 WN_linenum(wn) = acc_line_number;
	 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wn_param, 
								WN_ty(wn_param), WN_PARM_BY_REFERENCE);
 
			 
	 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wn_str_var, 
								 WN_ty(wn_str_var), WN_PARM_BY_VALUE);
	 return wn;
 }
 
 
 WN* Gen_Push_KernelParamScalar(WN* wn_param, WN* wn_str_var)
 {
	 WN* wn = WN_Create(OPC_VCALL, 2);
	 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_KERNELPARAMPUSH_SCALAR);
	 
	 WN_Set_Call_Non_Data_Mod(wn);
	 WN_Set_Call_Non_Data_Ref(wn);
	 WN_Set_Call_Non_Parm_Mod(wn);
	 WN_Set_Call_Non_Parm_Ref(wn);
	 WN_Set_Call_Parm_Ref(wn);
	 WN_linenum(wn) = acc_line_number;
	 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wn_param, 
								WN_ty(wn_param), WN_PARM_BY_REFERENCE);
 
			 
	 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wn_str_var, 
								 WN_ty(wn_str_var), WN_PARM_BY_VALUE);
	 return wn;
 }
 
 WN* Gen_ACC_Stack_Push()
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 0 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_STACK_PUSH);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
 
   return wn;
 }
 
 WN* Gen_ACC_Stack_Pop()
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 0 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_STACK_POP);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
 
   return wn;
 }
 
 WN* Gen_ACC_Free_Device_Ptr_InCurrentStack()
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 0 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_STACK_CLEAR_DEVICE_PTR_IN_CURRENT_STACK);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
 
   return wn;
 }
 
 
 WN *
 Gen_ACC_Pending_DevicePtr_To_Current_Stack(ST *st_dmem, BOOL bIsReduction) 
 {
	 WN * wn;
	 WN* wnx;
	 wn = WN_Create(OPC_VCALL, 2);	 
	 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_STACK_PENDING_TO_CURRENT_STACK);
   
	 WN_Set_Call_Non_Data_Mod(wn);
	 WN_Set_Call_Non_Data_Ref(wn);
	 WN_Set_Call_Non_Parm_Mod(wn);
	 WN_Set_Call_Non_Parm_Ref(wn);
	 WN_Set_Call_Parm_Ref(wn);
	 
	 wnx = WN_Ldid(Pointer_type, 0, st_dmem, ST_type(st_dmem));
	 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
						  WN_ty(wnx), WN_PARM_BY_VALUE);
	 WN_kid(wn, 1) = WN_CreateParm(MTYPE_I4, WN_Intconst(MTYPE_I4, bIsReduction), 
			   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
 
	 
	 return wn;
 }
 
 //only take care of region
 WN* ACC_Remove_All_ACC_Pragma(WN* node)
 {
	 WN* r;
	 int i;
	 WN* newnode = NULL;
	 
	 /* Walk all children */
	 if (WN_opcode(node) == OPC_BLOCK) 
	 {
		 r = WN_first(node);
		 while (r) 
		 { 
		   
			 //remove the empty region
			 if ((WN_opcode(r) == OPC_REGION) && (WN_region_kind(r) == REGION_KIND_ACC)) 
			 {
				 WN *wtmp = WN_first(WN_region_pragmas(r));
				 WN_PRAGMA_ID wid = (WN_PRAGMA_ID) WN_pragma(wtmp);
				 if(wid == WN_PRAGMA_ACC_WAIT ||
					 wid == WN_PRAGMA_ACC_UPDATE ||
					 wid == WN_PRAGMA_ACC_CACHE ||
					 wid == WN_PRAGMA_ACC_ENTER_DATA ||
					 wid == WN_PRAGMA_ACC_EXIT_DATA)
				 {
					 WN* wn_next = WN_next(r);
					 WN_prev(wn_next) = WN_prev(r);
					 WN_DELETE_Tree(r);
					 r = wn_next;
					 continue;
				 }
			 }
			 r = ACC_Remove_All_ACC_Pragma ( r);
			 if (WN_prev(r) == NULL)
				 WN_first(node) = r;
			 if (WN_next(r) == NULL)
				 WN_last(node) = r;
 
			 r = WN_next(r);
		   
		 }
		 return node;
	 }	 
	 else if ((WN_opcode(node) == OPC_REGION) && (WN_region_kind(node) == REGION_KIND_ACC)) 
	 {
		 WN *wtmp = WN_first(WN_region_pragmas(node));
		 WN_PRAGMA_ID wid = (WN_PRAGMA_ID) WN_pragma(wtmp); 	 
 
		 WN* wn_cont_nodes = WN_next(node);
		 WN* wn_stmt_block = WN_region_body(node);
 
		 switch (wid) {
		   /* orphaned SINGLE region: process it now and return */
		 case WN_PRAGMA_ACC_LOOP_BEGIN:
		 case WN_PRAGMA_ACC_KERNELS_BEGIN:
		 case WN_PRAGMA_ACC_PARALLEL_BEGIN:
		 case WN_PRAGMA_ACC_DATA_BEGIN:
		 case WN_PRAGMA_ACC_HOSTDATA_BEGIN:
		 {
			 newnode = ACC_Remove_All_ACC_Pragma(wn_stmt_block);
			 WN_next(newnode) = wn_cont_nodes;
		 }
		   break;
 
		 default:
		   printf("pragma value = %d", (int)wid); 
		   Fail_FmtAssertion (
			  "out of context pragma (%s) in acc ACC_Remove_All_ACC_Pragma.",
			  WN_pragmas[wid].name);
		 }
 
		 WN_Delete ( wtmp );
		 WN_Delete ( WN_region_pragmas(node) );
		 WN_DELETE_Tree ( WN_region_exits(node) );
		 return newnode;
	 }
	 else 
	 {
		 for (i=0; i < WN_kid_count(node); i++)
		 {
		   WN_kid(node, i) = ACC_Remove_All_ACC_Pragma ( WN_kid(node, i));
		 }
		 return node;
	 }
 }
 
 ACC_DATA_ST_MAP* ACC_GenSingleCreateAndMallocDeviceMem(ACC_DREGION__ENTRY dEntry, 
						 vector<ACC_DATA_ST_MAP*>* pDMap, WN* ReplacementBlock)
 {
	 WN* dclause = dEntry.acc_data_clauses;
	 ST *old_st = WN_st(dclause);
	 WN_OFFSET old_offset = WN_offsetx(dclause);	 
	 TY_IDX ty = ST_type(old_st);
	 TY_KIND kind = TY_kind(ty);//ST_name(old_st)
	 char* localname = (char *) alloca(strlen(ST_name(old_st))+35);
	 sprintf ( localname, "__device_%s_%d", ST_name(old_st), acc_reg_tmp_count);
	 acc_reg_tmp_count++;
	 
	 if (kind == KIND_ARRAY|| kind == KIND_POINTER)
	 {
		 TY_IDX etype;
		 if(kind == KIND_ARRAY)
			 etype = ACC_GetArrayElemType(ty);
		 else
		 {
			 etype = TY_pointed(ty);
			 if(TY_kind(etype) == KIND_ARRAY)
			 {
					 //etype = TY_etype(etype);
					 etype = ACC_GetArrayElemType(etype);
			 }
		 }
		 TY_IDX ty_p = Make_Pointer_Type(etype);
		 ST *karg = NULL;
		 WN *device_addr = NULL;
		 WN* wnSize = ACC_GetArraySize(dEntry);
		 WN* wnStart = ACC_GetArrayStart(dEntry);
		 ACC_DATA_ST_MAP* pSTMap = new ACC_DATA_ST_MAP;
		 karg = New_ST( CURRENT_SYMTAB );
		 ST_Init(karg,
				 Save_Str( localname ),
				 CLASS_VAR,
				 SCLASS_AUTO,
				 EXPORT_LOCAL,
				 ty_p);
		 //Set_ST_is_value_parm( karg );
		 pSTMap->deviceName = karg;
		 pSTMap->hostName = old_st;
		 pSTMap->wnSize = wnSize;
		 pSTMap->type_size = TY_size(etype);
		 pSTMap->wnStart = wnStart;
		 pDMap->push_back(pSTMap);
		 WN* WN_mallocall = Gen_DeviceMalloc(old_st, karg, WN_COPY_Tree(wnSize));
		 WN_INSERT_BlockLast(ReplacementBlock, WN_mallocall);
		 return pSTMap;
	 }
	 else if(kind == KIND_SCALAR)
	 {
		 TY_IDX etype = ST_type(old_st);
		 TY_IDX ty_p = Make_Pointer_Type(etype);
		 ST *karg = NULL;
		 WN *device_addr = NULL;
		 WN* wnSize = WN_Intconst(MTYPE_U4, TY_size(etype));
		 WN* wnStart = WN_Intconst(MTYPE_U4, 0);
		 ACC_DATA_ST_MAP* pSTMap = new ACC_DATA_ST_MAP;
		 karg = New_ST( CURRENT_SYMTAB );
		 ST_Init(karg,
				 Save_Str( localname ),
				 CLASS_VAR,
				 SCLASS_AUTO,
				 EXPORT_LOCAL,
				 ty_p);
		 //Set_ST_is_value_parm( karg );
		 pSTMap->deviceName = karg;
		 pSTMap->hostName = old_st;
		 pSTMap->wnSize = wnSize;
		 pSTMap->type_size = TY_size(etype);
		 pSTMap->wnStart = wnStart;
		 pDMap->push_back(pSTMap);
		 WN* WN_mallocall = Gen_DeviceMalloc(old_st, karg, WN_COPY_Tree(wnSize));
		 WN_INSERT_BlockLast(ReplacementBlock, WN_mallocall);
		 return pSTMap;
	 }
	 else if(kind == KIND_STRUCT && F90_ST_Has_Dope_Vector(old_st))
	 {
		 TY_IDX etype;
		 FLD_HANDLE  fli ;
		 TY_IDX base_pointer_ty;
		 fli = TY_fld(ty);
		 base_pointer_ty = FLD_type(fli);
		 etype = TY_etype(TY_pointed(base_pointer_ty));
 
		 
		 TY_IDX ty_p = Make_Pointer_Type(etype);
		 ST *karg = NULL;
		 WN *device_addr = NULL;
		 WN* wnSize = ACC_GetArraySize(dEntry);
		 WN* wnStart = ACC_GetArrayStart(dEntry);
		 ACC_DATA_ST_MAP* pSTMap = new ACC_DATA_ST_MAP;
		 karg = New_ST( CURRENT_SYMTAB );
		 ST_Init(karg,
				 Save_Str( localname ),
				 CLASS_VAR,
				 SCLASS_AUTO,
				 EXPORT_LOCAL,
				 ty_p);
		 //Set_ST_is_value_parm( karg );
		 pSTMap->deviceName = karg;
		 pSTMap->hostName = old_st;
		 pSTMap->wnSize = wnSize;
		 pSTMap->type_size = TY_size(etype);
		 pSTMap->wnStart = wnStart;
		 pDMap->push_back(pSTMap);
		 WN* WN_mallocall = Gen_DeviceMalloc(old_st, karg, WN_COPY_Tree(wnSize));
		 WN_INSERT_BlockLast(ReplacementBlock, WN_mallocall);
		 return pSTMap;
	 }
	 else 
		 Fail_FmtAssertion (
	   "out of WN kind in ACC_GenDeviceCreateAndMalloc(...)");
	 return NULL;
 }
 
 WN* ACC_GenIsPCreate(ACC_DREGION__ENTRY dEntry, ST* st_device,
										 unsigned int type_size)
 {
	 WN* wnSize = NULL;  
	 WN* wnSizeInUnit = NULL;
	 WN* wnStart = NULL;
	 ST *old_st = WN_st(dEntry.acc_data_clauses);
	 TY_IDX ty = ST_type(old_st);
	 WN* wnAddr;
	 if(TY_kind(ty) == KIND_ARRAY)
	 {
		 wnSize = ACC_GetArraySize(dEntry);  
		 wnSizeInUnit = ACC_GetArraySizeInUnit(dEntry);
		 wnStart = ACC_GetArrayStart(dEntry);
		 wnAddr = WN_Lda( Pointer_type, 0, old_st);
	 }
	 else if(TY_kind(ty) == KIND_POINTER)
	 {
		 wnSize = ACC_GetArraySize(dEntry);  
		 wnSizeInUnit = ACC_GetArraySizeInUnit(dEntry);
		 wnStart = ACC_GetArrayStart(dEntry);
		 wnAddr = WN_Ldid(Pointer_type, 0, old_st, ST_type(old_st));
	 }
	 else if(TY_kind(ty) == KIND_SCALAR)
	 {
		 wnSize = WN_Intconst(MTYPE_U4, TY_size(ty));	 
		 wnSizeInUnit = WN_Intconst(MTYPE_U4, 1);
		 wnStart = WN_Intconst(MTYPE_U4, 0);
		 wnAddr = WN_Lda( Pointer_type, 0, old_st);
	 }
	 else if(TY_kind(ty) == KIND_STRUCT && F90_ST_Has_Dope_Vector(old_st))
	 {		 
		 TY_IDX etype;
		 FLD_HANDLE  fli ;
		 TY_IDX base_pointer_ty;
		 fli = TY_fld(ty);
		 base_pointer_ty = FLD_type(fli);
		 etype = TY_etype(TY_pointed(base_pointer_ty));
		 
		 TY_IDX ty_p = Make_Pointer_Type(etype);
		 
		 wnSize = ACC_GetArraySize(dEntry);  
		 wnSizeInUnit = ACC_GetArraySizeInUnit(dEntry);
		 wnStart = ACC_GetArrayStart(dEntry);
		 wnAddr = WN_Ldid(Pointer_type, 0, old_st, ty_p);
	 }
	 else
		 Fail_FmtAssertion ("Unhandle cases in ACC_GenIsPCreate");
	 
	 WN * wn;
	 wn = WN_Create(OPC_I4CALL, 6 );
	 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_PRESENT_CREATE);
 
	 WN_Set_Call_Non_Data_Mod(wn);
	 WN_Set_Call_Non_Data_Ref(wn);
	 WN_Set_Call_Non_Parm_Mod(wn);
	 WN_Set_Call_Non_Parm_Ref(wn);
	 WN_Set_Call_Parm_Ref(wn);
	 WN_linenum(wn) = acc_line_number;
 
	 //wnx = WN_Lda( Pointer_type, 0, Src);
	 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnAddr, WN_ty(wnAddr), WN_PARM_BY_VALUE);	
	 
	 WN* wnx = WN_Lda( Pointer_type, 0, st_device);
	 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wnx, WN_ty(wnx), WN_PARM_BY_REFERENCE);
	 
	 WN_kid(wn, 2) = WN_CreateParm(MTYPE_U4, wnStart, Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
 
	 WN_kid(wn, 3) = WN_CreateParm(MTYPE_U4, wnSizeInUnit, Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
	 
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_U4, WN_Intconst(MTYPE_U4, type_size),
								 Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
   
	 char* strname = (char *) alloca ( strlen(ST_name(old_st))+ strlen(ST_name(st_device)) + 10);
	 sprintf ( strname, "%s : %s \0", ST_name(old_st), ST_name(st_device));
	 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
	 WN_kid(wn, 5) = WN_CreateParm(Pointer_type, wn_strname, 
								 WN_ty(wn_strname), WN_PARM_BY_VALUE);
	 
	 return wn;
 }
 
 //ACCR_GET_DEVICE_ADDR
 //this one, there is no malloc call
 ST* ACC_GenDeclareSingleDeviceMem(ACC_DREGION__ENTRY dentry, 
						 vector<ACC_DATA_ST_MAP*>* pDMap)
 {
	 WN* l = dentry.acc_data_clauses;
	 ST *old_st = WN_st(l);
	 WN_OFFSET old_offset = WN_offsetx(l);		 
	 TY_IDX ty = ST_type(old_st);
	 TY_KIND kind = TY_kind(ty);//ST_name(old_st)
	 char* localname = (char *) alloca(strlen(ST_name(old_st))+35);
	 sprintf ( localname, "__device_%s_%d", ST_name(old_st), acc_reg_tmp_count);
	 acc_reg_tmp_count ++;
	 
	 if (kind == KIND_ARRAY|| kind == KIND_POINTER)
	 {
		 TY_IDX etype;
		 if(kind == KIND_ARRAY)
			 etype = ACC_GetArrayElemType(ty);
		 else
		 {
			 etype = TY_pointed(ty);
			 if(TY_kind(etype) == KIND_ARRAY)
				 etype = ACC_GetArrayElemType(etype);
		 }
		 TY_IDX ty_p = Make_Pointer_Type(etype);
		 ST *karg = NULL;
		 WN *device_addr = NULL;
		 WN* wnSize = ACC_GetArraySize(dentry);
		 WN* wnStart = ACC_GetArrayStart(dentry);
		 ACC_DATA_ST_MAP* pSTMap = new ACC_DATA_ST_MAP;
		 karg = New_ST( CURRENT_SYMTAB );
		 ST_Init(karg,
				 Save_Str( localname ),
				 CLASS_VAR,
				 SCLASS_AUTO,
				 EXPORT_LOCAL,
				 ty_p);
		 //Set_ST_is_value_parm( karg );
		 pSTMap->deviceName = karg;
		 pSTMap->hostName = old_st;
		 pSTMap->wnSize = wnSize;
		 pSTMap->wnStart = wnStart;
		 pDMap->push_back(pSTMap);
		 return karg;
		 //return WN_mallocall;
		 //Gen_DataH2D(old_st, karg, isize);
		 //device_addr = WN_Lda( Pointer_type, 0, karg);
	 }
	 else if(kind == KIND_SCALAR)
	 {
		 TY_IDX etype = ST_type(old_st);
		 TY_IDX ty_p = Make_Pointer_Type(etype);
		 ST *karg = NULL;
		 WN *device_addr = NULL;
		 WN* wnSize = WN_Intconst(MTYPE_U4, TY_size(etype));
		 WN* wnStart = WN_Intconst(MTYPE_U4, 0);
		 ACC_DATA_ST_MAP* pSTMap = new ACC_DATA_ST_MAP;
		 karg = New_ST( CURRENT_SYMTAB );
		 ST_Init(karg,
				 Save_Str( localname ),
				 CLASS_VAR,
				 SCLASS_AUTO,
				 EXPORT_LOCAL,
				 ty_p);
		 //Set_ST_is_value_parm( karg );
		 pSTMap->deviceName = karg;
		 pSTMap->hostName = old_st;
		 pSTMap->wnSize = wnSize;
		 pSTMap->type_size = TY_size(etype);
		 pSTMap->wnStart = wnStart;
		 pDMap->push_back(pSTMap);
		 return karg;
	 }
	 else if(kind == KIND_STRUCT && F90_ST_Has_Dope_Vector(old_st))
	 {
		 TY_IDX etype;
		 FLD_HANDLE  fli ;
		 TY_IDX base_pointer_ty;
		 fli = TY_fld(ty);
		 base_pointer_ty = FLD_type(fli);
		 etype = TY_etype(TY_pointed(base_pointer_ty));
 
		 
		 TY_IDX ty_p = Make_Pointer_Type(etype);
		 ST *karg = NULL;
		 WN *device_addr = NULL;
		 WN* wnSize = ACC_GetArraySize(dentry);
		 WN* wnStart = ACC_GetArrayStart(dentry);
		 ACC_DATA_ST_MAP* pSTMap = new ACC_DATA_ST_MAP;
		 karg = New_ST( CURRENT_SYMTAB );
		 ST_Init(karg,
				 Save_Str( localname ),
				 CLASS_VAR,
				 SCLASS_AUTO,
				 EXPORT_LOCAL,
				 ty_p);
		 //Set_ST_is_value_parm( karg );
		 pSTMap->deviceName = karg;
		 pSTMap->hostName = old_st;
		 pSTMap->wnSize = wnSize;
		 pSTMap->type_size = TY_size(etype);
		 pSTMap->wnStart = wnStart;
		 pDMap->push_back(pSTMap);
		 return karg;
	 }
	 else 
		 Fail_FmtAssertion (
	   "out of WN kind in ACC_GenDeviceCreateAndMalloc(...)");
	 return NULL;
 }
 
 
 WN* ACC_Gen_GetDeviceAddr(ACC_DREGION__ENTRY dentry, ST* st_device, unsigned int type_size)
 {
	 WN* wnSize = ACC_GetArraySize(dentry);
	 WN* wnStart = ACC_GetArrayStart(dentry);
	 ST *old_st = WN_st(dentry.acc_data_clauses);
	 
	 WN* wnAddr;
	 
		 if(F90_ST_Has_Dope_Vector(old_st))
		 wnAddr = WN_Ldid(Pointer_type, 0, old_st, ST_type(st_device));
	 else if(TY_kind(ST_type(old_st)) == KIND_ARRAY)
		 wnAddr = WN_Lda( Pointer_type, 0, old_st);
	 else
		 wnAddr = WN_Ldid(Pointer_type, 0, old_st, ST_type(old_st));
	 
	 WN * wn;
	 WN * wnx;
	 wn = WN_Create(OPC_VCALL, 6);
	 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_GET_DEVICE_ADDR);
 
	 WN_Set_Call_Non_Data_Mod(wn);
	 WN_Set_Call_Non_Data_Ref(wn);
	 WN_Set_Call_Non_Parm_Mod(wn);
	 WN_Set_Call_Non_Parm_Ref(wn);
	 WN_Set_Call_Parm_Ref(wn);
	 WN_linenum(wn) = acc_line_number;
 
	 //wnx = WN_Lda( Pointer_type, 0, Src);
	 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnAddr, WN_ty(wnAddr), WN_PARM_BY_VALUE);	
	 
	 wnx = WN_Lda( Pointer_type, 0, st_device);
	 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wnx, WN_ty(wnx), WN_PARM_BY_REFERENCE);
	 
	 WN_kid(wn, 2) = WN_CreateParm(MTYPE_U4, wnStart, Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
 
	 WN_kid(wn, 3) = WN_CreateParm(MTYPE_U4, wnSize, Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
 
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_U4, WN_Intconst(MTYPE_U4, type_size), Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
   
	 char* strname = (char *) alloca ( strlen(ST_name(old_st))+ strlen(ST_name(st_device)) + 10);
	 sprintf ( strname, "%s : %s \0", ST_name(old_st), ST_name(st_device));
	 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
	 WN_kid(wn, 5) = WN_CreateParm(Pointer_type, wn_strname, 
								 WN_ty(wn_strname), WN_PARM_BY_VALUE);
	 
	 return wn;
 }
 
 /* Generate Wait function call, and delete all the WN nodes, clear the wait list. */
 void ACC_GenWaitNodes(vector<WN*> *pWaitlist, WN* ReplacementBlock)
 {
	 int i;
	 for(i=0; i<pWaitlist->size(); i++)
	 {
		 WN* wn_wait = (*pWaitlist)[i];
		 WN* acc_int_expr = WN_COPY_Tree(WN_kid0(wn_wait));
		 WN_INSERT_BlockLast(ReplacementBlock, ACC_GenWaitStream(acc_int_expr));
		 WN_Delete(wn_wait);
	 }
	 pWaitlist->clear();
 }
 
 void ACC_GenPresentNode(vector<ACC_DREGION__ENTRY>* pDREntries, vector<ACC_DATA_ST_MAP*>* pDMap, 
								 WN* ReplacementBlock)
 {
	 //WN* l;	 
	 WN* dClause;
	 int i=0;
	 if(!is_pcreate_tmp_created)
	 {
		 ACC_Host_Create_Preg_or_Temp( MTYPE_I4, "_is_pcreate",
					   &st_is_pcreate);
		 is_pcreate_tmp_created = TRUE;
	 }
	 
	 //////////////////////////////////////
	 for (i=0; i<pDREntries->size(); i++) 
	 {	   
		 ACC_DREGION__ENTRY dentry = (*pDREntries)[i];
		 dClause = dentry.acc_data_clauses;
 
		 WN* wnSize = ACC_GetArraySize(dentry);
		 WN* wnStart = ACC_GetArrayStart(dentry);
		 ST *old_st = WN_st(dClause);
		 TY_IDX ty = ST_type(old_st);
		 TY_IDX etype;
		 TY_KIND kind = TY_kind(ty);
		 if(kind == KIND_ARRAY)
			 etype = ACC_GetArrayElemType(ty);
		 else if(kind == KIND_POINTER)
		 {
			 etype = TY_pointed(ty);
			 if(TY_kind(etype) == KIND_ARRAY)
			 {
				 etype = ACC_GetArrayElemType(etype);
			 }
		 }
		 else if(kind == KIND_SCALAR)
		 {			 
			 etype = ty;
		 }
		 else if(kind == KIND_STRUCT && F90_ST_Has_Dope_Vector(old_st))
		 {
			 FLD_HANDLE  fli ;
			 TY_IDX base_pointer_ty;
			 fli = TY_fld(ty);
			 base_pointer_ty = FLD_type(fli);
			 etype = TY_etype(TY_pointed(base_pointer_ty));
		 }
		 unsigned int type_size = TY_size(etype);
		 WN_OFFSET old_offset = WN_offsetx(dClause);
		 ST* st_device = ACC_GenDeclareSingleDeviceMem(dentry, pDMap);
 
		 WN* wn_gen_device_addr = ACC_Gen_GetDeviceAddr(dentry, st_device, type_size);
		 WN_INSERT_BlockLast(ReplacementBlock, wn_gen_device_addr);
	 }
 }
 
 void ACC_GenDevicePtr(WN* nodes, vector<ST*>* pDMap)
 {
	 WN* l; 	 
	 for (l = nodes; l; l = WN_next(l)) 
	 {	   
	   //WN* wnSize = ACC_GetArraySize(l);
	   //WN* wnStart = ACC_GetArrayStart(l);
	   ST *dptr_st = WN_st(l);
	   pDMap->push_back(dptr_st);
	   //WN_OFFSET old_offset = WN_offsetx(l);
	   //ST* st_device = ACC_GenDeclareSingleDeviceMem(l, pDMap);
 
	   //WN* wn_gen_device_addr = ACC_Gen_GetDeviceAddr(l, st_device);
	   //WN_INSERT_BlockLast(ReplacementBlock, wn_gen_device_addr);
	 }
 }
 
 //for create and copyout, MemIn should be false.
 //for copy and copyin, MemIn should be true.
 void ACC_GenDeviceCreateCopyInOut(vector<ACC_DREGION__ENTRY>* pDREntries, 
								 vector<ACC_DATA_ST_MAP*>* pDMap, 
								 WN* ReplacementBlock, BOOL MemIn, BOOL isStructure)
 {
	 WN* dClause;
	 //WN* wnLength;
	 int i=0;
	 //if(!is_pcreate_tmp_created)
	 //{
		 //ACC_Host_Create_Preg_or_Temp( MTYPE_I4, "_is_pcreate",
		//				 &st_is_pcreate);
		 //is_pcreate_tmp_created = TRUE;
	 //}
	 
	 //////////////////////////////////////
	 for (i=0; i<pDREntries->size(); i++) 
	 {
	   ACC_DREGION__ENTRY dentry = (*pDREntries)[i];
	   dClause = dentry.acc_data_clauses;
	   WN* wnSize = NULL;
	   WN* wnStart = NULL;
	   WN_OFFSET old_offset = WN_offsetx(dClause);
	   ST *old_st = WN_st(dClause);
	   ST *st_is_pcreate_tmp = NULL;
	   TY_IDX ty = ST_type(old_st);
	   if(TY_kind(ty) == KIND_ARRAY || TY_kind(ty) == KIND_POINTER)
	   {
		   wnSize = ACC_GetArraySize(dentry);
		   wnStart = ACC_GetArrayStart(dentry);
	   }
	   else if(TY_kind(ty) == KIND_SCALAR)
	   {
		   //if dfa is enabled, all the scalar vars are ignored.
		   if(acc_dfa_enabled == TRUE)
			 continue;
		   wnSize = WN_Intconst(MTYPE_U4, TY_size(ty));
		   wnStart = WN_Intconst(MTYPE_U4, 0);
	   }
	   else if(TY_kind(ty) == KIND_STRUCT && F90_ST_Has_Dope_Vector(old_st))
	   {	 
		   wnSize = ACC_GetArraySize(dentry);
		   wnStart = ACC_GetArrayStart(dentry);
	   }
	   else 
		   Fail_FmtAssertion ("Unhandle cases in ACC_GenDeviceCreateCopyInOut");
	   
	   WN* thenblock = WN_CreateBlock();
	   WN* elseblock = WN_CreateBlock();
	   WN* wn_h2d;
	   PREG_NUM rreg1, rreg2;	 /* Pregs with I4 return values */;
	   //call is present function to check whether it has already been created.
	   //then block
	   ACC_DATA_ST_MAP* pSTMap = ACC_GenSingleCreateAndMallocDeviceMem(dentry, pDMap, thenblock);
	   ST* st_dMem = pSTMap->deviceName;
	   unsigned int type_size = pSTMap->type_size;
	   ///////////////////////////////////////////////////////////////////////////////
	   WN_INSERT_BlockLast( ReplacementBlock, ACC_GenIsPCreate(dentry, st_dMem, type_size));
	   ACC_GET_RETURN_PREGS(rreg1, rreg2, MTYPE_I4);   
	   ACC_Host_Create_Preg_or_Temp( MTYPE_I4, "_is_pcreate",
					   &st_is_pcreate_tmp);
	   pSTMap->st_is_pcreate_tmp = st_is_pcreate_tmp;
	   WN* wn_return = WN_Ldid(TY_mtype(ST_type(st_is_pcreate_tmp)), rreg1, Return_Val_Preg, ST_type(st_is_pcreate_tmp));
	   WN* temp_node = WN_Stid(TY_mtype(ST_type(st_is_pcreate_tmp)), 0, 
									 st_is_pcreate_tmp, ST_type(st_is_pcreate_tmp), wn_return);
	   
	   //WN_linenum(temp_node) = acc_line_number;
	   
	   WN_INSERT_BlockLast( ReplacementBlock, temp_node );
	   //if it is not exist create it
	   temp_node = WN_Ldid(TY_mtype(ST_type(st_is_pcreate_tmp)), 0, st_is_pcreate_tmp, ST_type(st_is_pcreate_tmp)); 	 
	   WN* test = WN_Relational (OPR_EQ, TY_mtype(ST_type(st_is_pcreate_tmp)), 
									 temp_node, WN_Intconst(MTYPE_I4, 0));
	   if(isStructure == TRUE)
	   {
		 WN* wn_pending_new_ptr = Gen_ACC_Pending_DevicePtr_To_Current_Stack(st_dMem);
		 WN_INSERT_BlockLast(thenblock, wn_pending_new_ptr);
	   }
	   
	   if(MemIn)
	   {
			 wn_h2d = Gen_DataH2D(old_st, st_dMem, WN_COPY_Tree(wnSize), WN_COPY_Tree(wnStart));
			 WN_INSERT_BlockLast(thenblock, wn_h2d);
	   }
	   //else block
	   //we already get the device address from ACC_GenIsPCreate
	   //WN* wn_GetDeviceAddr = ACC_Gen_GetDeviceAddr(dentry, st_dMem);
	   //WN_INSERT_BlockLast(elseblock, wn_GetDeviceAddr);
	   WN* ifexp = WN_CreateIf(test, thenblock, elseblock);
	   
	   WN_INSERT_BlockLast(ReplacementBlock, ifexp);
	 }
 }
 
 /*static void ACC_GenDataCopyIn(vector<ACC_DATA_ST_MAP*>* pDMap, WN* ReplacementBlock)
 {	 
	 UINT32 i = 0;
	 while(i<(*pDMap).size())
	 {
		 ACC_DATA_ST_MAP* pSTMap = (*pDMap)[i];
		 WN* H2DCalls = Gen_DataH2D((*pDMap)[i]->hostName, 
									 (*pDMap)[i]->deviceName, 
									 WN_COPY_Tree((*pDMap)[i]->wnSize),
									 WN_COPY_Tree((*pDMap)[i]->wnStart));
		 WN_INSERT_BlockLast(ReplacementBlock, H2DCalls);
		 i++;
	 }
 }*/
 WN* ACC_GenFreeDeviceMemory(ST* st_device_mem)
 {
	 WN * wn;
	 WN * wnx;
	 wn = WN_Create(OPC_VCALL, 2);
	 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_FREE_ON_DEVICE);
 
	 WN_Set_Call_Non_Data_Mod(wn);
	 WN_Set_Call_Non_Data_Ref(wn);
	 WN_Set_Call_Non_Parm_Mod(wn);
	 WN_Set_Call_Non_Parm_Ref(wn);
	 WN_Set_Call_Parm_Ref(wn);
	 WN_linenum(wn) = acc_line_number;
 
	 wnx = WN_Ldid(Pointer_type, 0, st_device_mem, ST_type(st_device_mem));
 
	 //wnx = WN_Lda( Pointer_type, 0, Src);
	 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
						WN_ty(wnx), WN_PARM_BY_VALUE);
   
	 char* strname = (char *) alloca ( strlen(ST_name(st_device_mem)) + 10);
	 sprintf ( strname, "%s \0", ST_name(st_device_mem));
	 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
	 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wn_strname, 
								 WN_ty(wn_strname), WN_PARM_BY_VALUE);
   
   return wn;
 }
 
 void ACC_GenDeviceMemFreeInBatch(vector<ACC_DATA_ST_MAP*>* pDMap, WN* ReplacementBlock)
 {	 
	 UINT32 i = 0;
	 while(i<(*pDMap).size())
	 {
		 ACC_DATA_ST_MAP* pSTMap = (*pDMap)[i];
		 WN* FeeDMemCalls = ACC_GenFreeDeviceMemory((*pDMap)[i]->deviceName);
		 WN_INSERT_BlockLast(ReplacementBlock, FeeDMemCalls);
		 i++;
	 }
 }
 
 
 void ACC_GenDataCopyOut(vector<ACC_DATA_ST_MAP*>* pDMap, WN* ReplacementBlock)
 {	 
	 UINT32 i = 0;
	 while(i<(*pDMap).size())
	 {
		 ACC_DATA_ST_MAP* pSTMap = (*pDMap)[i];
		 WN* D2HCalls = Gen_DataD2H((*pDMap)[i]->deviceName, 
									 (*pDMap)[i]->hostName, 
									 WN_COPY_Tree((*pDMap)[i]->wnSize),
									 WN_COPY_Tree((*pDMap)[i]->wnStart));
		 ST* st_is_pcreate_tmp = pSTMap->st_is_pcreate_tmp;
		 WN* temp_node = WN_Ldid(TY_mtype(ST_type(st_is_pcreate_tmp)), 0, st_is_pcreate_tmp, ST_type(st_is_pcreate_tmp));		 
		 WN* test = WN_Relational (OPR_EQ, TY_mtype(ST_type(st_is_pcreate_tmp)), 
									 temp_node, WN_Intconst(MTYPE_I4, 0));
		 WN* wn_then = WN_CreateBlock();
		 WN_INSERT_BlockLast(wn_then, D2HCalls);  
	   
		 WN* ifexp = WN_CreateIf(test, wn_then, WN_CreateBlock());
		 WN_INSERT_BlockLast(ReplacementBlock, ifexp);
		 i++;
	 }
 }
 
 WN* ACC_GenDataExitDelete(ST* st_Var, WN* wn_start, 
							 WN* wn_length, unsigned int type_size)
 {
   WN * wn;
   WN * wnx;
   wn = WN_Create(OPC_VCALL, 5 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_DATA_EXIT_DELETE);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
 
   if(F90_ST_Has_Dope_Vector(st_Var))
   {
	 TY_IDX ty_element = ACC_GetDopeElementType(st_Var);
	 TY_IDX ty_p = Make_Pointer_Type(ty_element);
		 wnx = WN_Ldid(Pointer_type, 0, st_Var, ty_p);
   }
   else  if(TY_kind(ST_type(st_Var)) == KIND_ARRAY)
	 wnx = WN_Lda( Pointer_type, 0, st_Var);
   else
	 wnx = WN_Ldid(Pointer_type, 0, st_Var, ST_type(st_Var));
 
   //wnx = WN_Lda( Pointer_type, 0, Src);
   WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
						WN_ty(wnx), WN_PARM_BY_VALUE);
 
   //wnx = WN_Lda( Pointer_type, 0, Dst);
   WN_kid(wn, 1) = WN_CreateParm(MTYPE_I4, wn_start, 
						Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   
   WN_kid(wn, 2) = WN_CreateParm(MTYPE_I4, wn_length, 
						Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   
   WN_kid(wn, 3) = WN_CreateParm(MTYPE_I4, WN_Intconst(MTYPE_U4, type_size), 
						Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   if(acc_AsyncExpr)
   {
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(acc_AsyncExpr), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   }
   else 
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_I4, WN_Intconst(MTYPE_I4, -2), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
 
 
 WN* ACC_GenUpdateHostVar(ST* st_Var, WN* wn_start, 
							 WN* wn_length, unsigned int type_size)
 {	 
   WN * wn;
   WN * wnx;
   wn = WN_Create(OPC_VCALL, 6 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_UPDATE_HOST_VARIABLE);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
 
   if(F90_ST_Has_Dope_Vector(st_Var))
   {
		 TY_IDX ty_element = ACC_GetDopeElementType(st_Var);
		 TY_IDX ty_p = Make_Pointer_Type(ty_element);
		 wnx = WN_Ldid(Pointer_type, 0, st_Var, ty_p);
   }
   else if(TY_kind(ST_type(st_Var)) == KIND_ARRAY)
	 wnx = WN_Lda( Pointer_type, 0, st_Var);
   else
	 wnx = WN_Ldid(Pointer_type, 0, st_Var, ST_type(st_Var));
 
   //wnx = WN_Lda( Pointer_type, 0, Src);
   WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
						WN_ty(wnx), WN_PARM_BY_VALUE);
 
   //wnx = WN_Lda( Pointer_type, 0, Dst);
   WN_kid(wn, 1) = WN_CreateParm(MTYPE_I4, wn_start, 
						Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   
   WN_kid(wn, 2) = WN_CreateParm(MTYPE_I4, wn_length, 
						Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   
   WN_kid(wn, 3) = WN_CreateParm(MTYPE_I4, WN_Intconst(MTYPE_U4, type_size), 
						Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   if(acc_AsyncExpr)
   {
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(acc_AsyncExpr), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   }
   else 
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_I4, WN_Intconst(MTYPE_I4, -2), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   
	 char* strname = (char *) alloca ( strlen(ST_name(st_Var)) + 10);
	 sprintf ( strname, "%s \0", ST_name(st_Var));
	 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
	 WN_kid(wn, 5) = WN_CreateParm(Pointer_type, wn_strname, 
								 WN_ty(wn_strname), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
 
 WN* ACC_GenUpdateDeviceVar(ST* st_Var, WN* wn_start,
						 WN* wn_length, unsigned int type_size)
 {	 
   WN * wn;
   WN * wnx;
   wn = WN_Create(OPC_VCALL, 6 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_UPDATE_DEVICE_VARIABLE);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
 
   if(F90_ST_Has_Dope_Vector(st_Var))
   {
		 TY_IDX ty_element = ACC_GetDopeElementType(st_Var);
		 TY_IDX ty_p = Make_Pointer_Type(ty_element);
		 wnx = WN_Ldid(Pointer_type, 0, st_Var, ty_p);
   }
   else if(TY_kind(ST_type(st_Var)) == KIND_ARRAY)
	 wnx = WN_Lda( Pointer_type, 0, st_Var);
   else
	 wnx = WN_Ldid(Pointer_type, 0, st_Var, ST_type(st_Var));
 
   //wnx = WN_Lda( Pointer_type, 0, Src);
   WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
						WN_ty(wnx), WN_PARM_BY_VALUE);
 
   //wnx = WN_Lda( Pointer_type, 0, Dst);
   WN_kid(wn, 1) = WN_CreateParm(MTYPE_I4, wn_start, 
						Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   
   WN_kid(wn, 2) = WN_CreateParm(MTYPE_I4, wn_length, 
						Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   
   WN_kid(wn, 3) = WN_CreateParm(MTYPE_I4, WN_Intconst(MTYPE_U4, type_size), 
						Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   
   if(acc_AsyncExpr)
   {
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(acc_AsyncExpr), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   }
   else 
	 WN_kid(wn, 4) = WN_CreateParm(MTYPE_I4, WN_Intconst(MTYPE_I4, -2), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
   
	 char* strname = (char *) alloca ( strlen(ST_name(st_Var)) + 10);
	 sprintf ( strname, "%s \0", ST_name(st_Var));
	 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
	 WN_kid(wn, 5) = WN_CreateParm(Pointer_type, wn_strname, 
								 WN_ty(wn_strname), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
 /*if wn_int_expr is "0", wait for all stream.
 else, it will just wait for the specified stream.*/
 WN* ACC_GenWaitStream(WN* wn_int_expr)
 {	 
   WN * wn;
   WN * wnx;
   wn = WN_Create(OPC_VCALL, 1 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_WAIT_SOME_OR_ALL_STREAM);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
   
   WN_kid(wn, 0) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(wn_int_expr), 
		   Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
 /*****************************************/
 void Push_Kernel_Parameters(WN* wn_replaceBlock, BOOL bParallel)
 {
	 WN* wnx, *wn;
	 int parm_id = 0;
	 int i=0;
	 
	 
	 //WN_Set_Call_IS_KERNEL_LAUNCH(wn);
	 //first copyin, then copyout, then copy, then param pragma
	 //
	 for(i=0; i<acc_kernelLaunchParamList.size(); i++)
	 {
		 ST* hostST = acc_kernelLaunchParamList[i].st_host;
		 //in case of deviceptr, the host st is NULL.
		 hostST = hostST ? hostST : acc_kernelLaunchParamList[i].st_device;
		 TY_IDX ty = ST_type(hostST);
		 TY_KIND kind = TY_kind(ty);//ST_name(old_st)
		 TYPE_ID typeID;
		 if(kind == KIND_POINTER || kind == KIND_ARRAY)
		 {		 
			 INT64 acc_dtype = 
					 GetKernelParamType(acc_kernelLaunchParamList[i].st_device);
			 wn = WN_Create(OPC_VCALL, 2);
			 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_KERNELPARAMPUSH_POINTER);
			 
			 WN_Set_Call_Non_Data_Mod(wn);
			 WN_Set_Call_Non_Data_Ref(wn);
			 WN_Set_Call_Non_Parm_Mod(wn);
			 WN_Set_Call_Non_Parm_Ref(wn);
			 WN_Set_Call_Parm_Ref(wn);
			 WN_linenum(wn) = acc_line_number;
			 
			 //wnx = WN_Ldid(Pointer_type, 0, acc_kernelLaunchParamList[i].st_device, 
			 // 				 ST_type(acc_kernelLaunchParamList[i].st_device));
			 
			 wnx = WN_Lda( Pointer_type, 0, acc_kernelLaunchParamList[i].st_device);
			 //First, pointer
			 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
								WN_ty(wnx), WN_PARM_BY_REFERENCE);
 
			 
			 char* strname = (char *) alloca ( strlen(ST_name(acc_kernelLaunchParamList[i].st_device)) + 10);
			 sprintf ( strname, "%s \0", ST_name(acc_kernelLaunchParamList[i].st_device));
			 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
			 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wn_strname, 
										 WN_ty(wn_strname), WN_PARM_BY_VALUE);
			 
			 //Attach two the replacement block
			 WN_INSERT_BlockLast(wn_replaceBlock, wn);
		 }
		 else if(kind == KIND_STRUCT && F90_ST_Has_Dope_Vector(hostST))
		 {		 
			 wn = WN_Create(OPC_VCALL, 2);
			 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_KERNELPARAMPUSH_POINTER);
			 
			 WN_Set_Call_Non_Data_Mod(wn);
			 WN_Set_Call_Non_Data_Ref(wn);
			 WN_Set_Call_Non_Parm_Mod(wn);
			 WN_Set_Call_Non_Parm_Ref(wn);
			 WN_Set_Call_Parm_Ref(wn);
			 WN_linenum(wn) = acc_line_number;
			 
			 //wnx = WN_Ldid(Pointer_type, 0, acc_kernelLaunchParamList[i].st_device, 
			 // 				 ST_type(acc_kernelLaunchParamList[i].st_device));
			 //TY_IDX alignty = F90_ST_Get_Dope_Vector_etype(hostST);
			 wnx = WN_Lda( Pointer_type, 0, acc_kernelLaunchParamList[i].st_device);
			 
			 //First, pointer
			 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
								WN_ty(wnx), WN_PARM_BY_REFERENCE);
 
			 
			 char* strname = (char *) alloca ( strlen(ST_name(acc_kernelLaunchParamList[i].st_device)) + 10);
			 sprintf ( strname, "%s \0", ST_name(acc_kernelLaunchParamList[i].st_device));
			 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
			 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wn_strname, 
										 WN_ty(wn_strname), WN_PARM_BY_VALUE);
			 
			 //Attach two the replacement block
			 WN_INSERT_BlockLast(wn_replaceBlock, wn);
		 }
		 else if(kind == KIND_SCALAR && acc_kernelLaunchParamList[i].st_device != NULL)
		 {		 
			 INT64 acc_dtype = 
					 GetKernelParamType(acc_kernelLaunchParamList[i].st_device);
			 wn = WN_Create(OPC_VCALL, 2);
			 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_KERNELPARAMPUSH_POINTER);
			 
			 WN_Set_Call_Non_Data_Mod(wn);
			 WN_Set_Call_Non_Data_Ref(wn);
			 WN_Set_Call_Non_Parm_Mod(wn);
			 WN_Set_Call_Non_Parm_Ref(wn);
			 WN_Set_Call_Parm_Ref(wn);
			 WN_linenum(wn) = acc_line_number;
			 
			 //wnx = WN_Ldid(Pointer_type, 0, acc_kernelLaunchParamList[i].st_device, 
			 // 				 ST_type(acc_kernelLaunchParamList[i].st_device));
			 
			 wnx = WN_Lda( Pointer_type, 0, acc_kernelLaunchParamList[i].st_device);
			 //First, pointer
			 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
								WN_ty(wnx), WN_PARM_BY_REFERENCE);
			 
			 char* strname = (char *) alloca ( strlen(ST_name(acc_kernelLaunchParamList[i].st_device)) + 10);
			 sprintf ( strname, "%s \0", ST_name(acc_kernelLaunchParamList[i].st_device));
			 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
			 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wn_strname, 
										 WN_ty(wn_strname), WN_PARM_BY_VALUE);
			 //Attach two the replacement block
			 WN_INSERT_BlockLast(wn_replaceBlock, wn);
		 }
		 else if(kind == KIND_SCALAR && acc_kernelLaunchParamList[i].st_device == NULL)
		 {
			 ST* old_st = acc_kernelLaunchParamList[i].st_host;
			 map<ST*, ACC_ReductionMap>::iterator itor = acc_reduction_tab_map.find(old_st); 
			 //find the symbol in the reduction, then ignore it in the parameters.
			 if(itor != acc_reduction_tab_map.end())
				 continue;
			 //////////////////////////////////////////////////
			 wn = WN_Create(OPC_VCALL, 2);
			 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_KERNELPARAMPUSH_SCALAR);
			 
			 WN_Set_Call_Non_Data_Mod(wn);
			 WN_Set_Call_Non_Data_Ref(wn);
			 WN_Set_Call_Non_Parm_Mod(wn);
			 WN_Set_Call_Non_Parm_Ref(wn);
			 WN_Set_Call_Parm_Ref(wn);
			 WN_linenum(wn) = acc_line_number;
			 
			 //wnx = WN_Ldid(Pointer_type, 0, acc_kernelLaunchParamList[i].st_device, 
			 // 				 ST_type(acc_kernelLaunchParamList[i].st_device));
			 
			 wnx = WN_Lda( Pointer_type, 0, old_st);
			 //First, pointer
			 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
								WN_ty(wnx), WN_PARM_BY_REFERENCE);
			 
			 char* strname = (char *) alloca ( strlen(ST_name(acc_kernelLaunchParamList[i].st_host)) + 10);
			 sprintf ( strname, "%s \0", ST_name(acc_kernelLaunchParamList[i].st_host));
			 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
			 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wn_strname, 
										 WN_ty(wn_strname), WN_PARM_BY_VALUE);
			 
			 //Attach two the replacement block
			 WN_INSERT_BlockLast(wn_replaceBlock, wn);
		 }
		 else
		 {
			 Is_True(FALSE, ("Wrong Kernel Parameter Kind Type. in Push_Kernel_Parameters 1"));
		 }
	 }
	 //launch the reduction parameters
	 i=0;
	 //Only parallel region doing the reduction this way
	 for(i=0; (bParallel && (i<acc_additionalKernelLaunchParamList.size())); i++)
	 {
		 KernelParameter parmList = acc_additionalKernelLaunchParamList[i];
		 ////////////////////////////////////////////////////////////////////////////
		 ST* st_device = parmList.st_device;
		 TY_IDX ty = ST_type(st_device);
		 TY_KIND kind = TY_kind(ty);//ST_name(old_st)
		 TYPE_ID typeID;
		 if(kind == KIND_POINTER || kind == KIND_ARRAY)
		 {
			 wn = WN_Create(OPC_VCALL, 2 );
			 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_KERNELPARAMPUSH_POINTER);
			 
			 WN_Set_Call_Non_Data_Mod(wn);
			 WN_Set_Call_Non_Data_Ref(wn);
			 WN_Set_Call_Non_Parm_Mod(wn);
			 WN_Set_Call_Non_Parm_Ref(wn);
			 WN_Set_Call_Parm_Ref(wn);
			 WN_linenum(wn) = acc_line_number;
			 
			 wnx = WN_Lda(Pointer_type, 0, st_device);
			 //First, pointer
			 WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
									 WN_ty(wnx), WN_PARM_BY_VALUE);
			 
			 char* strname = (char *) alloca ( strlen(ST_name(st_device)) + 10);
			 sprintf ( strname, "%s \0", ST_name(st_device));
			 WN* wn_strname = WN_LdaString(strname,0, strlen(strname)+1);
			 WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wn_strname, 
										 WN_ty(wn_strname), WN_PARM_BY_VALUE);
			 //Attach two the replacement block
			 WN_INSERT_BlockLast(wn_replaceBlock, wn);
		 }
		 else
		 {
			 Is_True(FALSE, ("Wrong Kernel Parameter Kind Type in Push_Kernel_Parameters 2."));
		 }
	 }
 }
 
 
 /****************************************************************
 Setup the number of GANG and VECTOR, or maybe worker in the future
 *****************************************************************/
 //Set Gangs
 WN* Gen_Set_Gangs_Num_X(WN* wn_num)
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 1 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_SET_GANG_NUM_X);
   if(wn_num == NULL)
	 wn_num = WN_Intconst(MTYPE_I4, 0);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
   WN_kid(wn, 0) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(wn_num), Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
 WN* Gen_Set_Gangs_Num_Z(WN* wn_num)
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 1 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_SET_GANG_NUM_Z);
   if(wn_num == NULL)
	 wn_num = WN_Intconst(MTYPE_I4, 0);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
   WN_kid(wn, 0) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(wn_num), Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
 
 WN* Gen_Set_Gangs_Num_Y(WN* wn_num)
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 1 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_SET_GANG_NUM_Y);
   if(wn_num == NULL)
	 wn_num = WN_Intconst(MTYPE_I4, 0);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
   WN_kid(wn, 0) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(wn_num), Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
 //Set vectors
 WN* Gen_Set_Vector_Num_X(WN* wn_num)
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 1 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_SET_VECTOR_NUM_X);
   if(wn_num == NULL)
	 wn_num = WN_Intconst(MTYPE_I4, 0);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
   WN_kid(wn, 0) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(wn_num), Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
  WN* Gen_Set_Vector_Num_Y(WN* wn_num)
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 1 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_SET_VECTOR_NUM_Y);
   if(wn_num == NULL)
	 wn_num = WN_Intconst(MTYPE_I4, 0);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
   WN_kid(wn, 0) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(wn_num), Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
  WN* Gen_Set_Vector_Num_Z(WN* wn_num)
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 1 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_SET_VECTOR_NUM_Z);
   if(wn_num == NULL)
	 wn_num = WN_Intconst(MTYPE_I4, 0);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
   WN_kid(wn, 0) = WN_CreateParm(MTYPE_I4, WN_COPY_Tree(wn_num), Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);
 
   return wn;
 }
 
  WN* Gen_Set_Vectors_Toplogy()
 {
	 return NULL;
 }
 
  WN* Gen_Set_Default_Toplogy()
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 0 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_SETUP_DEFAULT_TOLOGY);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
 
   return wn;
 }
 
  WN* Gen_Reset_Default_Toplogy()
 {
   WN * wn;
   wn = WN_Create(OPC_VCALL, 0 );
   WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_RESET_DEFAULT_TOLOGY);
 
   WN_Set_Call_Non_Data_Mod(wn);
   WN_Set_Call_Non_Data_Ref(wn);
   WN_Set_Call_Non_Parm_Mod(wn);
   WN_Set_Call_Non_Parm_Ref(wn);
   WN_Set_Call_Parm_Ref(wn);
   WN_linenum(wn) = acc_line_number;
 
   return wn;
 }
 
  void ACC_Setup_GPU_toplogy_1block_1thread(WN* replace_block)
 {
	 //topology setup
	 WN* wn_gangs_set = Gen_Set_Gangs_Num_X(WN_Intconst(MTYPE_U4, 1));
	 WN_INSERT_BlockLast(replace_block, wn_gangs_set);
	 wn_gangs_set = Gen_Set_Gangs_Num_Y(WN_Intconst(MTYPE_U4, 1));
	 WN_INSERT_BlockLast(replace_block, wn_gangs_set);
	 wn_gangs_set = Gen_Set_Gangs_Num_Z(WN_Intconst(MTYPE_U4, 1));
	 WN_INSERT_BlockLast(replace_block, wn_gangs_set);
			 
	 WN* wn_vectors_set = Gen_Set_Vector_Num_X(WN_Intconst(MTYPE_U4, 1));
	 WN_INSERT_BlockLast(replace_block, wn_vectors_set);
	 wn_vectors_set = Gen_Set_Vector_Num_Y(WN_Intconst(MTYPE_U4, 1));
	 WN_INSERT_BlockLast(replace_block, wn_vectors_set);
	 wn_vectors_set = Gen_Set_Vector_Num_Z(WN_Intconst(MTYPE_U4, 1));
	 WN_INSERT_BlockLast(replace_block, wn_vectors_set);
 }
 
  WN* ACC_GenGetTotalNumVectors()
 {
	 WN * wn;
	 wn = WN_Create(OPC_I4CALL, 0 );
	 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_GET_TOTAL_VECTORS);
 
	 WN_Set_Call_Non_Data_Mod(wn);
	 WN_Set_Call_Non_Data_Ref(wn);
	 WN_Set_Call_Non_Parm_Mod(wn);
	 WN_Set_Call_Non_Parm_Ref(wn);
	 WN_Set_Call_Parm_Ref(wn);
	 WN_linenum(wn) = acc_line_number;
 
	 return wn;
 }
 
  WN* ACC_GenGetTotalNumGangs()
 {
	 WN * wn;
	 wn = WN_Create(OPC_I4CALL, 0 );
	 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_GET_TOTAL_GANGS);
 
	 WN_Set_Call_Non_Data_Mod(wn);
	 WN_Set_Call_Non_Data_Ref(wn);
	 WN_Set_Call_Non_Parm_Mod(wn);
	 WN_Set_Call_Non_Parm_Ref(wn);
	 WN_Set_Call_Parm_Ref(wn);
	 WN_linenum(wn) = acc_line_number;
 
	 return wn;
 }
 
  WN* ACC_GenGetTotalNumGangsWorkers()
 {
	 WN * wn;
	 wn = WN_Create(OPC_I4CALL, 0 );
	 WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_GET_TOTAL_GANGS_WORKERS);
 
	 WN_Set_Call_Non_Data_Mod(wn);
	 WN_Set_Call_Non_Data_Ref(wn);
	 WN_Set_Call_Non_Parm_Mod(wn);
	 WN_Set_Call_Non_Parm_Ref(wn);
	 WN_Set_Call_Parm_Ref(wn);
	 WN_linenum(wn) = acc_line_number;
 
	 return wn;
 }
  
WN* ACC_Gen_Dim_Init_Call(ST* st_dim)
{
	WN * wn;
	WN * wnx;
	//int iParm = device_copyout.size() + device_copyin.size() + acc_parms_count;
	UINT32 i = 0;
	int parm_id = 0;

	//Then launch the kernel module
	//create whirl CALL
	wn = WN_Create(OPC_VCALL, 1 );
	WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_INIT_LAUNCH_PARAMS);

	WN_Set_Call_Non_Data_Mod(wn);
	WN_Set_Call_Non_Data_Ref(wn);
	WN_Set_Call_Non_Parm_Mod(wn);
	WN_Set_Call_Non_Parm_Ref(wn);
	WN_Set_Call_Parm_Ref(wn);
	WN_linenum(wn) = acc_line_number;

	wnx = WN_Lda( Pointer_type, 0, st_dim);
	WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
					 WN_ty(wnx), WN_PARM_BY_REFERENCE);
	return wn;
}

//for reduction launch parameters
WN* ACC_Gen_Reduction_Dim_Init_Call(ST* st_dim, ST* st_blocksize)
{
	WN * wn;
	WN * wnx;
	//int iParm = device_copyout.size() + device_copyin.size() + acc_parms_count;
	UINT32 i = 0;
	int parm_id = 0;

	//Then launch the kernel module
	//create whirl CALL
	wn = WN_Create(OPC_VCALL, 2 );
	WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_INIT_LAUNCH_RED_PARAMS);

	WN_Set_Call_Non_Data_Mod(wn);
	WN_Set_Call_Non_Data_Ref(wn);
	WN_Set_Call_Non_Parm_Mod(wn);
	WN_Set_Call_Non_Parm_Ref(wn);
	WN_Set_Call_Parm_Ref(wn);
	WN_linenum(wn) = acc_line_number;

	wnx = WN_Lda( Pointer_type, 0, st_dim);
	WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
					 WN_ty(wnx), WN_PARM_BY_REFERENCE);


	wnx = WN_Lda( Pointer_type, 0, st_blocksize);
	WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wnx, 
					 WN_ty(wnx), WN_PARM_BY_REFERENCE);
	return wn;
}


WN* GenReductionMalloc(ST* st_device, WN* wnSize)
{
	WN * wn;
	WN* wnx;
	wn = WN_Create(OPC_VCALL, 2);   
	WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_REDUCTION_BUFF_MALLOC);

	WN_Set_Call_Non_Data_Mod(wn);
	WN_Set_Call_Non_Data_Ref(wn);
	WN_Set_Call_Non_Parm_Mod(wn);
	WN_Set_Call_Non_Parm_Ref(wn);
	WN_Set_Call_Parm_Ref(wn);
	  
	wnx = WN_Lda( Pointer_type, 0, st_device);
	WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
					 WN_ty(wnx), WN_PARM_BY_REFERENCE);

	WN_kid(wn, 1) = WN_CreateParm(MTYPE_I4, wnSize, Be_Type_Tbl(MTYPE_I4), WN_PARM_BY_VALUE);

	return wn;
}

WN* Gen_Sync_Threads()
{
  WN * wn;
  wn = WN_Create(OPC_VCALL, 0 );
  WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_SYNCTHREADS);

  WN_Set_Call_Non_Data_Mod(wn);
  WN_Set_Call_Non_Data_Ref(wn);
  WN_Set_Call_Non_Parm_Mod(wn);
  WN_Set_Call_Non_Parm_Ref(wn);
  WN_Set_Call_Parm_Ref(wn);
  WN_linenum(wn) = acc_line_number;

  return wn;
}

 WN* ACC_Gen_Call_Local_Reduction(ST* st_device_func, ST* st_inputdata)
{
  WN * wn;
  WN * wnx;
  wn = WN_Create(OPC_VCALL, 1);
  WN_st_idx(wn) = ST_st_idx(*st_device_func);
  ST* st_fun = WN_st(wn);
  //Set_ST_st_idx(st_fun, Save_Str(ST_name(st_device_func)));
  WN_Set_Call_Non_Data_Mod(wn);
  WN_Set_Call_Non_Data_Ref(wn);
  WN_Set_Call_Non_Parm_Mod(wn);
  WN_Set_Call_Non_Parm_Ref(wn);
  WN_Set_Call_Parm_Ref(wn);
  WN_linenum(wn) = acc_line_number;

  wnx = WN_Ldid(Pointer_type, 0, st_inputdata, ST_type(st_inputdata));


  //wnx = WN_Lda( Pointer_type, 0, Src);
  WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
                       WN_ty(wnx), WN_PARM_BY_VALUE);
  return wn;
}

