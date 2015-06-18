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


map<TYPE_ID, ST*> acc_global_memory_for_reduction_host;		//used in host side
map<TYPE_ID, ST*> acc_global_memory_for_reduction_device;	//used in host side
map<TYPE_ID, ST*> acc_global_memory_for_reduction_param;	//used in kernel parameters
map<TYPE_ID, ST*> acc_global_memory_for_reduction_block;	//used in a single block

//The following are using for shared memory 
map<TYPE_ID, ST*> acc_shared_memory_for_reduction_block;	//used in device side

//////////////////////////////////////////////////////////////////////////
map<ST*, ACC_ReductionMap> acc_reduction_tab_map; //ST in Host side
map<ST*, ACC_ReductionMap*> acc_reduction_tab_map_new; //ST in Host side


ReductionUsingMem acc_reduction_mem = ACC_RD_SHARED_MEM;
ReductionRolling acc_reduction_rolling = ACC_RD_UNROLLING;
ReductionExeMode acc_reduction_exemode;



vector<acc_reduction_kernels_pair> acc_reduction_kernels_maps;
vector<acc_reduction_kernels_pair> acc_reduction_devices_maps; //devices function for reduction
map<ST*, ST*> acc_reduction_device_reduction_call;

ST *acc_reduction_proc;	/* reduction for ACC process */
WN *acc_reduction_func;	/* reduction kernel function */
WN *acc_replace_block;	/* Replacement nodes to be returned */

static ST* ACC_GenerateReduction_Kernels_TopLoop(ACC_ReductionMap* pReduction_map);
static ST* ACC_GenerateWorkerVectorReduction_unrolling(ACC_ReductionMap* pReduction_map);
static ST* ACC_GenerateWorkerReduction_unrolling(ACC_ReductionMap* pReduction_map);
static ST* ACC_GenerateVectorReduction_unrolling(ACC_ReductionMap* pReduction_map);

//////////////////////////////////////////////////////////////////////////////////////////////////
vector<FOR_LOOP_RDC_INFO> acc_top_level_loop_rdc;

static WN_MAP ACC_Reduction_Map;
#define Set_WN_ReductionMap(wn, p) (WN_MAP_Set(ACC_Reduction_Map, wn, (void*)  p))
#define Get_WN_ReductionMap(wn) ((ACC_ReductionMap*) WN_MAP_Get(ACC_Reduction_Map, (WN*) wn))
//////////////////////////////////////////////////////////////////////////////////////////////////

static WN* ACC_GEN_Reduction_Binary_Op(OPERATOR opr, TYPE_ID rtype, WN *l, WN *r)
{
	WN* wn_operation = NULL;
	if(opr == OPR_EQ || opr == OPR_NE)
		wn_operation = WN_Relational (opr, rtype, l, r);
	else
		wn_operation = WN_Binary(opr, rtype, l, r);
	return wn_operation;
}


/*Init value of reduction*/
static char* ACC_Get_Name_of_Reduction(OPERATOR ReductionOpr)
{
	switch(ReductionOpr)
	{
	case OPR_ADD:
		return "ADD";
	case OPR_CIOR:
		return "CIOR";
	case OPR_BIOR:
		return "BIOR";
	case OPR_BXOR:
		return "BXOR";
	case OPR_MAX:
		return "MAX";
	case OPR_MIN:
		return "MIN";
	case OPR_BAND:
		return "BAND";
	case OPR_MPY:
		return "MPY";
	case OPR_CAND:
		return "CAND";
	case OPR_LAND:
		return "LAND";
	case OPR_LIOR:
		return "LIOR";
	case OPR_EQ:
		return "EQ";
	case OPR_NE:
		return "NE";
	default:
    	Fail_FmtAssertion("invalid reduction operator for OpenACC: ACC_Get_Name_of_Reduction");		
	}
}


/*Init value of reduction*/
static char* ACC_Get_ScalarName_of_Reduction(TYPE_ID typeID)
{
	switch(typeID)
	{
	case MTYPE_I1: 		 /*   8-bit integer */
		return "char";
	case MTYPE_I2:  		 /*  16-bit integer */
		return "short";
	case MTYPE_I4:  		 /*  32-bit integer */
		return "int32";
	case MTYPE_I8:  		 /*  64-bit integer */
		return "long";
	case MTYPE_U1:  		 /*   8-bit unsigned integer */
		return "uchar";
	case MTYPE_U2:  		 /*  16-bit unsigned integer */
		return "ushort";
	case MTYPE_U4:  		 /*  32-bit unsigned integer */
		return "uint32";
	case MTYPE_U8:  		 /*  64-bit unsigned integer */
		return "ulong";
	case MTYPE_F4: 		 /*  32-bit IEEE floating point */
		return "float";
	case MTYPE_F8: 		 /*  64-bit IEEE floating point */
		return "double";
	default:
		Is_True(FALSE, ("Wrong reduction data Type in ACC_Get_ScalarName_of_Reduction. It must be scalar data. "));
	}
}


/*Init value of reduction*/
static char* ACC_Get_LoopTypeName_of_Reduction(ACC_LOOP_TYPE looptype)
{
	switch(looptype)
	{
	case ACC_GANG:
		return "local";
	case ACC_VECTOR: 			
		return "vector";
	case ACC_WORKER_VECTOR:  	
		return "worker_vector";
	case ACC_NONE_SPECIFIED:
	case ACC_GANG_WORKER_VECTOR:
		if(acc_reduction_mem == ACC_RD_SHARED_MEM)	 	
			return "worker_vector";
		else
			Is_True(FALSE, ("Wrong reduction Loop Type."));
	case ACC_GANG_WORKER:
		if(acc_reduction_mem == ACC_RD_SHARED_MEM)	 	
			return "worker";
		else
			Is_True(FALSE, ("Wrong reduction Loop Type."));
	case ACC_WORKER:  		 	
		return "worker";
	default:
		Is_True(FALSE, ("Wrong reduction Loop Type."));
	}
}


void GenFinalReductionAlgorithm_APU(ST* st_dbuffer, ST* st_dhost, 
				ST* st_reduction_kernel_name, ST* st_num_of_element, UINT32 iTypesize, 
				WN* wn_replace_block)
{	
	WN * wn;
	WN * wnx;
	WN * l;
	//int iParm = device_copyout.size() + device_copyin.size() + acc_parms_count;
	UINT32 i = 0;
	int parm_id = 0;
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
	
	sprintf ( localname, "__accr_blocksize_%d", acc_reg_tmp_count);
	acc_reg_tmp_count++;
	ST* st_blocksize = New_ST( CURRENT_SYMTAB );
	ST_Init(st_blocksize,
		Save_Str( localname ),
		CLASS_VAR,
		SCLASS_AUTO,
		EXPORT_LOCAL,
		Be_Type_Tbl(MTYPE_U4));
	
	WN* wn_init_dim = ACC_Gen_Reduction_Dim_Init_Call(st_dim, st_blocksize);
	WN_INSERT_BlockLast(wn_replace_block, wn_init_dim);
	///////////////////////////////////////////////////////////////////////////
	
	wn = WN_Create(OPC_VCALL, 5);
	WN_st_idx(wn) = ST_st_idx(st_reduction_kernel_name);
	//make the kernels parameters ready first	  
  	wnx = WN_Ldid(Pointer_type, 0, st_dbuffer, ST_type(st_dbuffer));	
    WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
                       WN_ty(wnx), WN_PARM_BY_VALUE);
	
	wnx = WN_Ldid( Pointer_type, 0, st_dhost, ST_type(st_dhost));
    WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wnx, 
                       WN_ty(wnx), WN_PARM_BY_REFERENCE);
	
  	wnx = WN_Ldid(TY_mtype(ST_type(st_num_of_element)), 0, st_num_of_element, 
						ST_type(st_num_of_element));	
    WN_kid(wn, 2) = WN_CreateParm(TY_mtype(ST_type(st_num_of_element)), wnx, 
		  							ST_type(st_num_of_element), WN_PARM_BY_VALUE);
	
  	wnx = WN_Ldid(TY_mtype(ST_type(st_blocksize)), 0, st_blocksize, 
						ST_type(st_blocksize));	
    WN_kid(wn, 3) = WN_CreateParm(TY_mtype(ST_type(st_blocksize)), wnx, 
		  							ST_type(st_blocksize), WN_PARM_BY_VALUE);
	//setup one more parameter for launch kernel	
	wnx = WN_Ldid( Pointer_type, 0, st_dim, ST_type(st_dim));
	WN_kid(wn, 4) = WN_CreateParm(Pointer_type, wnx, 
                       WN_ty(wnx), WN_PARM_BY_REFERENCE);
	
	WN_INSERT_BlockLast(wn_replace_block, wn);
}


/* End the Content of wn_mp_dg.cxx.
*  csc.
*/

WN* GenFinalReductionAlgorithm_nvidia(ST* st_dbuffer, ST* st_dhost, 
				ST* st_reduction_kernel_name, ST* st_num_of_element, UINT32 iTypesize)
{
	//Set_ST_name_idx
	WN * wn;
	WN* wnx;
	wn = WN_Create(OPC_VCALL, 6);	
	WN_st_idx(wn) = GET_ACCRUNTIME_ST(ACCR_FINAL_REDUCTION_ALGORITHM);
  
	WN_Set_Call_Non_Data_Mod(wn);
	WN_Set_Call_Non_Data_Ref(wn);
	WN_Set_Call_Non_Parm_Mod(wn);
	WN_Set_Call_Non_Parm_Ref(wn);
	WN_Set_Call_Parm_Ref(wn);
		
	wnx = WN_Ldid( Pointer_type, 0, st_dhost, ST_type(st_dhost));
    WN_kid(wn, 0) = WN_CreateParm(Pointer_type, wnx, 
                       WN_ty(wnx), WN_PARM_BY_REFERENCE);
  
  	wnx = WN_Ldid(Pointer_type, 0, st_dbuffer, ST_type(st_dbuffer));	
    WN_kid(wn, 1) = WN_CreateParm(Pointer_type, wnx, 
                       WN_ty(wnx), WN_PARM_BY_VALUE);

	
	char* kernelname = ST_name(st_reduction_kernel_name);
	WN* wn_kernelname = WN_LdaString(kernelname,0, strlen(kernelname)+1);
	WN_kid(wn, 2) = WN_CreateParm(Pointer_type, wn_kernelname, 
						 		WN_ty(wn_kernelname), WN_PARM_BY_VALUE);
	//which PTX file	
    char* srcfname = Last_Pathname_Component(Src_File_Name);   
    char* ptxfname = New_Extension ( srcfname, ".w2c.ptx");
	WN* wn_ptxname = WN_LdaString(ptxfname,0, strlen(ptxfname)+1);
	WN_kid(wn, 3) = WN_CreateParm(Pointer_type, wn_ptxname, 
						 		WN_ty(wn_ptxname), WN_PARM_BY_VALUE);
	
  	wnx = WN_Ldid(TY_mtype(ST_type(st_num_of_element)), 0, st_num_of_element, ST_type(st_num_of_element));	
    WN_kid(wn, 4) = WN_CreateParm(TY_mtype(ST_type(st_num_of_element)), wnx, 
		  							ST_type(st_num_of_element), WN_PARM_BY_VALUE);
	
    WN_kid(wn, 5) = WN_CreateParm(MTYPE_U4, WN_Intconst(MTYPE_U4, iTypesize), 
		  							Be_Type_Tbl(MTYPE_U4), WN_PARM_BY_VALUE);
	
  	return wn;
}


/**
reduction kernel parameters
**/

static ST* st_input_data;
static ST* st_output_data;
static ST* st_num_elem;
static ST* st_blocksize;

//static ST* st_isPow2;
/*****************************************/
//This function is only called by ACC_Create_Reduction_Kernels  function.
static void Create_reduction_kernel_st_params(ST* st_reduction)
{	
	//reduction_kernel_param.clear();
	
	WN* wn;
	//this var may be deleted later.it is useless.
	ACC_VAR_TYPE vtype = ACC_VAR_COPYIN;			
	TY_IDX ty = ST_type(st_reduction);
	TY_KIND kind = TY_kind(ty);//ST_name(old_st)
	if (kind != KIND_SCALAR)
    	Fail_FmtAssertion("Create_reduction_kernel_st_params: invalid OpenACC reduction type. It must be scalar variables.");
	
	char* localname = (char *) alloca(strlen(ST_name(st_reduction))+20);

	//declare in buffer
	sprintf ( localname, "g_in_%s", ST_name(st_reduction) );

	//This is a pointer type
	//TY_IDX pty = TY_pointed(ty);
	TY_IDX ty_p = Make_Pointer_Type(MTYPE_To_TY(TY_mtype(ty)));
	ST *karg = NULL;
	karg = New_ST( CURRENT_SYMTAB );
	ST_Init(karg,
			Save_Str( localname ),
			CLASS_VAR,
			SCLASS_FORMAL,
			EXPORT_LOCAL,
			ty_p);
	Set_ST_is_value_parm( karg );
	st_input_data = karg;
	
  	if(acc_target_arch == ACC_ARCH_TYPE_APU)		 
		Set_ST_ACC_global_data(st_input_data);
	 
	//declare out buffer	
	sprintf ( localname, "g_out_%s", ST_name(st_reduction) );

	//This is a pointer type
	//TY_IDX pty = TY_pointed(ty);
	karg = New_ST( CURRENT_SYMTAB );
	ST_Init(karg,
			Save_Str( localname ),
			CLASS_VAR,
			SCLASS_FORMAL,
			EXPORT_LOCAL,
			ty_p);
	Set_ST_is_value_parm( karg );
	st_output_data = karg;
  	if(acc_target_arch == ACC_ARCH_TYPE_APU)
		Set_ST_ACC_global_data(st_output_data);

	//delcare "n", how many elements in this input buffer
	sprintf ( localname, "n_of_%s", ST_name(st_reduction) );
	karg = New_ST( CURRENT_SYMTAB );
	ST_Init(karg,
			Save_Str( localname ),
			CLASS_VAR,
			SCLASS_FORMAL,
			EXPORT_LOCAL,
			Be_Type_Tbl(MTYPE_U4));
	Set_ST_is_value_parm( karg );
	st_num_elem = karg;
	//delcare "blocksize", 
	sprintf ( localname, "blocksize");
	karg = New_ST( CURRENT_SYMTAB );
	ST_Init(karg,
			Save_Str( localname ),
			CLASS_VAR,
			SCLASS_FORMAL,
			EXPORT_LOCAL,
			Be_Type_Tbl(MTYPE_U4));
	Set_ST_is_value_parm( karg );
	st_blocksize = karg;

}


//This function is only called by ACC_Create_Reduction_vector/worker/workervector  function.
static void Create_reduction_device_st_params(ST* st_reduction)
{	
	//reduction_kernel_param.clear();
	
	WN* wn;
	//this var may be deleted later.it is useless.
	ACC_VAR_TYPE vtype = ACC_VAR_COPYIN;			
	TY_IDX ty = ST_type(st_reduction);
	TY_KIND kind = TY_kind(ty);//ST_name(old_st)
	if (kind != KIND_SCALAR)
    	Fail_FmtAssertion("Create_reduction_device_st_params: invalid OpenACC reduction type. It must be scalar variables.");
	
	char* localname = (char *) alloca(strlen(ST_name(st_reduction))+20);

	//declare in buffer
	sprintf ( localname, "sdata_%s", ST_name(st_reduction) );

	//This is a pointer type
	//TY_IDX pty = TY_pointed(ty);
	TY_IDX ty_p = Make_Pointer_Type(MTYPE_To_TY(TY_mtype(ty)));
	ST *karg = NULL;
	karg = New_ST( CURRENT_SYMTAB );
	ST_Init(karg,
			Save_Str( localname ),
			CLASS_VAR,
			SCLASS_FORMAL,
			EXPORT_LOCAL,
			ty_p);
	Set_ST_is_value_parm( karg );
	st_input_data = karg;	

}



static ST_IDX Make_ACC_CUDA_Runtime_ST ( ST* st_device_func)
{
  //Is_True(rop >= ACCRUNTIME_FIRST && rop <= ACCRUNTIME_LAST,
  //        ("Make_ACCRuntime_ST: bad rop == %d", (INT) rop));
  if(acc_reduction_device_reduction_call[st_device_func])
  	return ST_st_idx(*acc_reduction_device_reduction_call[st_device_func]);

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
  ST_Init(st, Save_Str ( ST_name(st_device_func) ), CLASS_FUNC, SCLASS_EXTERN,
    EXPORT_PREEMPTIBLE, pu_idx);

  Allocate_Object ( st );
  acc_reduction_device_reduction_call[st_device_func] = st;
  //accr_sts[rop] = ST_st_idx(*st);
  return ST_st_idx(*acc_reduction_device_reduction_call[st_device_func]);
}

//vector<acc_reduction_kernels_pair> acc_reduction_kernels_maps
static void ACC_Create_Reduction_Kernels ( PAR_FUNC_ACC_TYPE func_type, 
				ACC_ReductionMap* pReduction_map)
{
  // should be merged up after done. Currently reserved for Debug
  const char *construct_type_str = "accrg_reduction";
  char temp_str[64];
  char szReduction_name[64];
  char szReduction_datatype[64];
  OPERATOR ReductionOpr = pReduction_map->ReductionOpr;
  ST* st_reduction_var = pReduction_map->hostName;
  char* looptype;

  sprintf(szReduction_name, "%s", ACC_Get_Name_of_Reduction(ReductionOpr));
  sprintf(szReduction_datatype, "%s", ACC_Get_ScalarName_of_Reduction(TY_mtype(ST_type(st_reduction_var))));
  // generate new name for nested function

  // should PAR regions and PAR DO's be numbered separately? -- DRK
  const char *old_st_name = ST_name(PU_Info_proc_sym(Current_PU_Info));
  char *st_name = (char *) alloca(strlen(old_st_name) + 64);
  if(func_type == PAR_FUNC_ACC_KERNEL)
  	looptype = "kernel";
  else
  	looptype = ACC_Get_LoopTypeName_of_Reduction(pReduction_map->looptype);
  sprintf ( st_name, "__%s_%s_%s_%s_%d_%d", construct_type_str, looptype, szReduction_name, szReduction_datatype,
	      acc_region_num, acc_construct_num );
  
  acc_construct_num ++;
  // get function prototype

  TY_IDX func_ty_idx = TY_IDX_ZERO;

  if  (func_ty_idx == TY_IDX_ZERO) 
  {
    // create new type for function, and type for pointer to function

    TY& ty = New_TY(func_ty_idx);
    sprintf(temp_str, ".%s", construct_type_str);
    TY_Init(ty, 0, KIND_FUNCTION, MTYPE_UNKNOWN, Save_Str(temp_str));
    Set_TY_align(func_ty_idx, 1);
	
    Set_TY_has_prototype(func_ty_idx);
	

    TYLIST_IDX parm_idx;
    TYLIST& parm_list = New_TYLIST(parm_idx);	
    Set_TY_tylist(ty, parm_idx);
    Set_TYLIST_type(parm_list, Be_Type_Tbl(MTYPE_V));  // return type

    /* turn this off if don't want to use taskargs struct */
    //else if (0)

    Set_TYLIST_type(New_TYLIST(parm_idx), TY_IDX_ZERO); // end of parm list

    // now create a type for a pointer to this function
    TY_IDX ptr_ty_idx;
    TY &ptr_ty = New_TY(ptr_ty_idx);
    sprintf(temp_str, ".%s_ptr", construct_type_str);
    TY_Init(ptr_ty, Pointer_Size, KIND_POINTER, Pointer_Mtype,
            Save_Str(temp_str));
    Set_TY_pointed(ptr_ty, func_ty_idx);
  }


  // create new PU and ST for nested function

  PU_IDX pu_idx;
  PU& pu = New_PU(pu_idx);
  PU_Init(pu, func_ty_idx, CURRENT_SYMTAB);

/*
Many questions of DRK's about flags:

is_pure and no_side_effects shouldn't be set due to output ref. parms?
does no_delete matter?
have no idea: needs_fill_align_lowering, needs_t9, put_in_elf_section,
  has_return_address, has_inlines, calls_{set,long}jmp, namelist
has_very_high_whirl and mp_needs_lno should have been handled already
is inheriting pu_recursive OK?
*/

  Set_PU_no_inline(pu);
  Set_PU_is_nested_func(pu);
  Set_PU_acc(pu);
  if(acc_target_arch == ACC_ARCH_TYPE_APU)
  	Set_PU_acc_opencl(pu);
  Set_PU_has_acc(pu);
  
#ifdef KEY
  Set_PU_acc_lower_generated(pu);
#endif // KEY
    // child PU inherits language flags from parent
  if (PU_c_lang(Current_PU_Info_pu()))
    Set_PU_c_lang(pu);
  if (PU_cxx_lang(Current_PU_Info_pu()))
    Set_PU_cxx_lang(pu);
  if (PU_f77_lang(Current_PU_Info_pu()))
    Set_PU_f77_lang(pu);
  if (PU_f90_lang(Current_PU_Info_pu()))
    Set_PU_f90_lang(pu);
  if (PU_java_lang(Current_PU_Info_pu()))
    Set_PU_java_lang(pu);

  Set_FILE_INFO_has_acc(File_info);  // is this true after acc lowerer?--DRK
  
  //TY_IDX	   funtype = ST_pu_type(st);
  //BOOL		   has_prototype = TY_has_prototype(funtype);
  //acc_reduction_proc

  acc_reduction_proc = New_ST(GLOBAL_SYMTAB);
  ST_Init(acc_reduction_proc,
          Save_Str (st_name),
          CLASS_FUNC,
          SCLASS_TEXT,
          EXPORT_LOCAL,
          TY_IDX (pu_idx));
  Set_ST_addr_passed(acc_reduction_proc);  
  Set_ST_sfname_idx(acc_reduction_proc, Save_Str(Src_File_Name));
  
  if(func_type == PAR_FUNC_ACC_KERNEL)
  	Set_ST_ACC_kernels_func(acc_reduction_proc);
  else if(func_type == PAR_FUNC_ACC_DEVICE)
  	Set_ST_ACC_device_func(acc_reduction_proc);

  Allocate_Object ( acc_reduction_proc);

  //
  acc_reduction_kernels_pair reduction_pairs;
  reduction_pairs.opr_ty = ST_type(st_reduction_var);
  reduction_pairs.ReductionOpr = ReductionOpr;
  reduction_pairs.st_kernels_fun = acc_reduction_proc;
  reduction_pairs.looptype = pReduction_map->looptype;
  if(func_type == PAR_FUNC_ACC_KERNEL)
  	acc_reduction_kernels_maps.push_back(reduction_pairs);
  else
  	acc_reduction_devices_maps.push_back(reduction_pairs);


  // create nested symbol table for parallel function

  New_Scope(CURRENT_SYMTAB + 1,
            Malloc_Mem_Pool,  // find something more appropriate--DRK
            TRUE);
  acc_csymtab = CURRENT_SYMTAB;
  acc_func_level = CURRENT_SYMTAB;
  Scope_tab[acc_csymtab].st = acc_reduction_proc;

  Set_PU_lexical_level(pu, CURRENT_SYMTAB);

  ACC_Create_Func_DST ( st_name );


  // pre-allocate in child as many pregs as there are in the parent

  for (UINT32 i = 1; i < PREG_Table_Size(acc_psymtab); i++) {
    PREG_IDX preg_idx;
    PREG &preg = New_PREG(acc_csymtab, preg_idx);
      // share name with corresponding parent preg
    Set_PREG_name_idx(preg,
      PREG_name_idx((*Scope_tab[acc_psymtab].preg_tab)[preg_idx]));
  }

    // create ST's for parameters

  ST *arg_gtid = NULL;
  ST *task_args = NULL;
  //Create the local ST for  kernels parameters
  if(func_type == PAR_FUNC_ACC_KERNEL)
  	Create_reduction_kernel_st_params(st_reduction_var);
  else
  	Create_reduction_device_st_params(st_reduction_var);
 
  //////////////////////////////////////////////////////////////////////
  /* declare some global variables for threadIdx and blockIdx */
  
  glbl_threadIdx_x = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_threadIdx_x,
      Save_Str( "__nv50_threadIdx_x"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  glbl_threadIdx_y = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_threadIdx_y,
      Save_Str( "__nv50_threadIdx_y"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  glbl_threadIdx_z = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_threadIdx_z,
      Save_Str( "__nv50_threadIdx_z"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  glbl_blockIdx_x = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_blockIdx_x,
      Save_Str( "__nv50_blockIdx_x"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  glbl_blockIdx_y = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_blockIdx_y,
      Save_Str( "__nv50_blockIdx_y"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  glbl_blockIdx_z = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_blockIdx_z,
      Save_Str( "__nv50_blockIdx_z"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  glbl_blockDim_x = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_blockDim_x,
      Save_Str( "__nv50_blockdim_x"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  glbl_blockDim_y = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_blockDim_y,
      Save_Str( "__nv50_blockdim_y"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  glbl_blockDim_z = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_blockDim_z,
      Save_Str( "__nv50_blockdim_z"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));

	
  glbl_gridDim_x = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_gridDim_x,
      Save_Str( "__nv50_griddim_x"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  
  glbl_gridDim_y = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_gridDim_y,
      Save_Str( "__nv50_griddim_y"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  
  glbl_gridDim_z = New_ST(CURRENT_SYMTAB); 
  ST_Init(glbl_gridDim_z,
      Save_Str( "__nv50_griddim_z"),
      CLASS_VAR,
      SCLASS_FORMAL,
      EXPORT_LOCAL,
      Be_Type_Tbl(MTYPE_U4));
  

    // TODO: other procedure specific arguments should
    // be handled here.

    // create WHIRL tree for nested function

  acc_reduction_func = WN_CreateBlock ( );
  WN* reference_block = WN_CreateBlock ( );
  WN* pragma_block = WN_CreateBlock ( );
#ifdef KEY
  WN *current_pu_tree = PU_Info_tree_ptr(Current_PU_Info);
  //WN *thread_priv_prag = WN_first(WN_func_pragmas(PU_Info_tree_ptr(Current_PU_Info)));
  
#endif
 
  UINT ikid = 0;
  WN *func_entry;
  //vector<ST*>::iterator itor = kernel_param.begin();
  //while(ikid < reduction_kernel_param.size())
  if(func_type == PAR_FUNC_ACC_KERNEL)
  {
     func_entry = WN_CreateEntry ( 4, acc_reduction_proc,
                                    acc_reduction_func, pragma_block,
                                    reference_block );
     WN_kid(func_entry, 0) = WN_CreateIdname ( 0, ST_st_idx(st_input_data));
     WN_kid(func_entry, 1) = WN_CreateIdname ( 0, ST_st_idx(st_output_data));
     WN_kid(func_entry, 2) = WN_CreateIdname ( 0, ST_st_idx(st_num_elem));
     WN_kid(func_entry, 3) = WN_CreateIdname ( 0, ST_st_idx(st_blocksize));
     //WN_kid(func_entry, 4) = WN_CreateIdname ( 0, ST_st_idx(st_isPow2));
	 //ACC_Add_DST_variable ( reduction_kernel_param[ikid], acc_nested_dst, acc_line_number, DST_INVALID_IDX );
  	 //ikid ++;
  }
  else
  {
     func_entry = WN_CreateEntry ( 1, acc_reduction_proc,
                                    acc_reduction_func, pragma_block,
                                    reference_block );
     WN_kid(func_entry, 0) = WN_CreateIdname ( 0, ST_st_idx(st_input_data));
  }

     // TODO: variable arguments list should be added here.

  WN_linenum(func_entry) = acc_line_number;


  // create PU_Info for nested function
  
  PU_Info *reduction_kernels_pu = TYPE_MEM_POOL_ALLOC ( PU_Info, Malloc_Mem_Pool );
  PU_Info_init ( reduction_kernels_pu );
  Set_PU_Info_tree_ptr (reduction_kernels_pu, func_entry );

  PU_Info_proc_sym(reduction_kernels_pu) = ST_st_idx(acc_reduction_proc);
  PU_Info_maptab(reduction_kernels_pu) = acc_cmaptab = WN_MAP_TAB_Create(MEM_pu_pool_ptr);
  PU_Info_pu_dst(reduction_kernels_pu) = acc_nested_dst;
  Set_PU_Info_state(reduction_kernels_pu, WT_SYMTAB, Subsect_InMem);
  Set_PU_Info_state(reduction_kernels_pu, WT_TREE, Subsect_InMem);
  Set_PU_Info_state(reduction_kernels_pu, WT_PROC_SYM, Subsect_InMem);
  Set_PU_Info_flags(reduction_kernels_pu, PU_IS_COMPILER_GENERATED);

  // don't copy nystrom points to analysis, alias_tag map
  // mp function's points to analysis will be analyzed locally.
  //AliasAnalyzer *aa = AliasAnalyzer::aliasAnalyzer();
  //if (aa) 
  //{
  //  // Current_Map_Tab is update to PU_Info_maptab(parallel_pu) in PU_Info_maptab
  //  Is_True(PU_Info_maptab(reduction_kernels_pu) == Current_Map_Tab,
  //      ("parallel_pu's PU's maptab isn't parallel_pu\n"));
  //  Current_Map_Tab = acc_pmaptab;
  //  WN_MAP_Set_dont_copy(aa->aliasTagMap(), TRUE);
  //  WN_MAP_Set_dont_copy(WN_MAP_ALIAS_CGNODE, TRUE);
  //  Current_Map_Tab = PU_Info_maptab(reduction_kernels_pu);
  //}
  //else 
  //{
    Current_Map_Tab = acc_pmaptab;
    WN_MAP_Set_dont_copy(WN_MAP_ALIAS_CGNODE, TRUE);
    Current_Map_Tab = PU_Info_maptab(reduction_kernels_pu);
  //}

    // use hack to save csymtab using parallel_pu, so we can restore it
    // later when we lower parallel_pu; this is necessary because the
    // new symtab routines can't maintain more than one chain of symtabs
    // in memory at one time, and we lower the parent PU all the way to
    // CG before we lower any of the nested MP PUs
        // Save_Local_Symtab expects this
  Set_PU_Info_symtab_ptr(reduction_kernels_pu, NULL);
  Save_Local_Symtab(acc_csymtab, reduction_kernels_pu);

  Is_True(PU_Info_state(reduction_kernels_pu, WT_FEEDBACK) == Subsect_Missing,
          ("there should be no feedback for parallel_pu"));

  RID *root_rid = RID_Create ( 0, 0, func_entry );
  RID_type(root_rid) = RID_TYPE_func_entry;
  Set_PU_Info_regions_ptr ( reduction_kernels_pu, root_rid );
  Is_True(PU_Info_regions_ptr(reduction_kernels_pu) != NULL,
	 ("ACC_Create_Reduction_Kernels, NULL root RID"));

  PU_Info *tpu = PU_Info_child(Current_PU_Info);

    // add parallel_pu after last child MP PU_Info item in parent's list
  if (tpu && PU_Info_state(tpu, WT_SYMTAB) == Subsect_InMem &&
      PU_acc(PU_Info_pu(tpu)) ) 
  {
    PU_Info *npu;

    while ((npu = PU_Info_next(tpu)) &&
	   PU_Info_state(npu, WT_SYMTAB) == Subsect_InMem &&
	   PU_acc(PU_Info_pu(npu)) )
      tpu = npu;

    PU_Info_next(tpu) = reduction_kernels_pu;
    PU_Info_next(reduction_kernels_pu) = npu;
  } 
  else 
  {
    PU_Info_child(Current_PU_Info) = reduction_kernels_pu;
    PU_Info_next(reduction_kernels_pu) = tpu;
  }


  // change some global state; need to clean this up--DRK

  Current_PU_Info = reduction_kernels_pu;
  Current_pu = &Current_PU_Info_pu();
  Current_Map_Tab = acc_pmaptab;

  //if (has_gtid)
  //  Add_DST_variable ( arg_gtid, nested_dst, line_number, DST_INVALID_IDX );
  //Add_DST_variable ( arg_slink, nested_dst, line_number, DST_INVALID_IDX );

}


//kernel function for reductions
static ST* ACC_Get_Reduction_kernels(OPERATOR ReductionOpr, TY_IDX ty_reduction_opr)
{
	int i=0;
	while(i<acc_reduction_kernels_maps.size())
	{
		acc_reduction_kernels_pair kernelsPair = acc_reduction_kernels_maps[i];
		if(kernelsPair.opr_ty == ty_reduction_opr
				&& kernelsPair.ReductionOpr == ReductionOpr)
			{
				return kernelsPair.st_kernels_fun;
			}
		i++;
	}
	return NULL;
}

//device function for reductions
static ST* ACC_Get_Reduction_devices(ACC_ReductionMap* pReduction_map)
{
	int i=0;	
	OPERATOR ReductionOpr = pReduction_map->ReductionOpr;
    TY_IDX ty_reduction_opr = ST_type(pReduction_map->hostName);
	ACC_LOOP_TYPE looptype = pReduction_map->looptype;
	while(i<acc_reduction_devices_maps.size())
	{
		acc_reduction_kernels_pair kernelsPair = acc_reduction_devices_maps[i];
		if(kernelsPair.opr_ty == ty_reduction_opr
				&& kernelsPair.ReductionOpr == ReductionOpr
				&& kernelsPair.looptype == looptype)
			{
				return kernelsPair.st_kernels_fun;
			}
		i++;
	}
	return NULL;
}


static WN* ACC_LoadDeviceSharedArrayElem(WN* wn_array_offset, ST* st_array)
{
	
	//load shared array
	unsigned int isize;
	if(TY_kind(ST_type(st_array)) == KIND_ARRAY)
	{
		isize = TY_size(TY_etype(ST_type(st_array)));
	}
	else if(TY_kind(ST_type(st_array)) == KIND_POINTER)
	{
		isize = TY_size(TY_pointed(ST_type(st_array)));
	}
	WN* wn_index = WN_Binary(OPR_MPY, MTYPE_U4, wn_array_offset, WN_Intconst(MTYPE_U4, isize));
	WN* wn_LoadReductionArray = WN_Ldid(Pointer_type, 0, st_array, ST_type(st_array));
	WN* wn_ptrLoc = WN_Binary(OPR_ADD, MTYPE_U4, wn_index, wn_LoadReductionArray);

	return wn_ptrLoc;
	//WN* tree = WN_Istore(TY_mtype(TY_pointed(ST_type(w->new_st))), 0, ST_type(w->new_st), wn_ptrLoc, WN_kid(tree, 0));
	//WN* tree = WN_Iload(TY_mtype(TY_pointed(ST_type(w->new_st))), 0,  TY_pointed(ST_type(w->new_st)), wn_ptrLoc);
}

/*Init value of reduction*/
WN* ACC_Get_Init_Value_of_Reduction(OPERATOR ReductionOpr, TYPE_ID rtype)
{
	WN* wn_init = NULL;
	switch(ReductionOpr)
	{
	case OPR_ADD:
	case OPR_CIOR:
	case OPR_BIOR:
	case OPR_BXOR:
	case OPR_LIOR:
	case OPR_NE:
		//return 0
		wn_init = WN_Intconst(MTYPE_U4, 0);
		break;
	case OPR_MAX:
		//return min INT_MIN
		if(rtype>=MTYPE_I1 && rtype<MTYPE_I8)
			wn_init = WN_Intconst(MTYPE_I4, INT_MIN);
		else if(rtype>=MTYPE_U1 && rtype<MTYPE_U8)
			wn_init = WN_Intconst(MTYPE_U4, 0);
		else if(rtype>=MTYPE_F4 && rtype<MTYPE_F16)
			wn_init = WN_Floatconst(MTYPE_F4, INT_MIN);
		else 
			Fail_FmtAssertion("Unhandle data type for MAX reduction: ACC_Get_Init_Value_of_Reduction");
			
		break;
	case OPR_MIN:
		//return max INT_MAX
		if(rtype>=MTYPE_I1 && rtype<MTYPE_I8)
			wn_init = WN_Intconst(MTYPE_I4, INT_MAX);
		else if(rtype>=MTYPE_U1 && rtype<MTYPE_U8)
			wn_init = WN_Intconst(MTYPE_U4, UINT_MAX);
		else if(rtype>=MTYPE_F4 && rtype<MTYPE_F16)
			wn_init = WN_Floatconst(MTYPE_F4, INT_MAX);
		else 
			Fail_FmtAssertion("Unhandle data type for MIN reduction: ACC_Get_Init_Value_of_Reduction");
		
		break;
	case OPR_BAND:
		//return ~0
		wn_init = WN_Intconst(MTYPE_U4, (~0));
		break;
	case OPR_MPY:
	case OPR_CAND:
	case OPR_LAND:
	case OPR_EQ:
		//return 1
		wn_init = WN_Intconst(MTYPE_U4, 1);
		break;
	default:
    	Fail_FmtAssertion("invalid reduction operator for OpenACC: ACC_Get_Init_Value_of_Reduction");		
	}
	return wn_init;
}

static WN* GenRightShiftAndOrOperations(WN* wn_op, WN* wn_bitpos)
{
	TYPE_ID rtype = TY_mtype(ST_type(WN_st(wn_op)));
	WN* wn_rightShift = WN_Binary(OPR_ASHR, rtype, WN_COPY_Tree(wn_op), wn_bitpos);
	WN* wn_OrOpr = WN_Binary(OPR_BIOR, rtype, WN_COPY_Tree(wn_op), wn_rightShift);
	
	wn_OrOpr = WN_Stid(rtype, 0, WN_st(wn_op), ST_type(WN_st(wn_op)), wn_OrOpr);
	return wn_OrOpr;
}

static WN* Gen_Next_Pow2DeviceStmt(WN* wn_op, WN* wn_replace_block)
{
	TYPE_ID rtype = TY_mtype(ST_type(WN_st(wn_op)));//
	WN* wn_ShrAndOr;
	WN* wn_minusOne = WN_Binary(OPR_SUB, rtype, WN_COPY_Tree(wn_op), WN_Intconst(rtype, 1));
	wn_minusOne = WN_Stid(TY_mtype(ST_type(WN_st(wn_op))), 0, WN_st(wn_op),
								ST_type(WN_st(wn_op)), wn_minusOne);
	WN_INSERT_BlockLast(wn_replace_block,  wn_minusOne);

	wn_ShrAndOr = GenRightShiftAndOrOperations(wn_op, WN_Intconst(rtype, 1));
	WN_INSERT_BlockLast(wn_replace_block,  wn_ShrAndOr);
	wn_ShrAndOr = GenRightShiftAndOrOperations(wn_op, WN_Intconst(rtype, 2));
	WN_INSERT_BlockLast(wn_replace_block,  wn_ShrAndOr);
	wn_ShrAndOr = GenRightShiftAndOrOperations(wn_op, WN_Intconst(rtype, 4));
	WN_INSERT_BlockLast(wn_replace_block,  wn_ShrAndOr);
	wn_ShrAndOr = GenRightShiftAndOrOperations(wn_op, WN_Intconst(rtype, 8));
	WN_INSERT_BlockLast(wn_replace_block,  wn_ShrAndOr);
	wn_ShrAndOr = GenRightShiftAndOrOperations(wn_op, WN_Intconst(rtype, 16));
	WN_INSERT_BlockLast(wn_replace_block,  wn_ShrAndOr);
	
	WN* wn_plusOne = WN_Binary(OPR_ADD, rtype, WN_COPY_Tree(wn_op), WN_Intconst(rtype, 1));
	wn_plusOne = WN_Stid(TY_mtype(ST_type(WN_st(wn_op))), 0, WN_st(wn_op),
								ST_type(WN_st(wn_op)), wn_plusOne);
	WN_INSERT_BlockLast(wn_replace_block,  wn_plusOne);
}


/*static WN* Gen_ReductionIfElseBlock1_Vector(WN* wn_blocksize, WN* wn_tid, WN* wn_btid, ST* st_shared_array,
						WN* wn_mySum, unsigned int ilimit, OPERATOR ReductionOpr)
{
	WN* wn_IfOuterTest = WN_Relational (OPR_GE, MTYPE_U4, 
								WN_COPY_Tree(wn_blocksize), 
								WN_Intconst(MTYPE_U4, ilimit));
	WN* wn_IfInnerTest = WN_Relational (OPR_LT, MTYPE_U4, 
								WN_COPY_Tree(wn_tid), 
								WN_Intconst(MTYPE_U4, ilimit/2));;
	WN* wn_OuterBlock = WN_CreateBlock();
	WN* wn_InnerBlock = WN_CreateBlock();
	WN* wn_CallSyncThreads = Gen_Sync_Threads();
	
	WN* Init0 = WN_Binary(OPR_ADD, MTYPE_U4, 
					WN_COPY_Tree(wn_tid), WN_Intconst(MTYPE_U4, ilimit/2));
	TY_IDX elem_ty;
	if(TY_kind(ST_type(st_shared_array)) == KIND_ARRAY)
		elem_ty = TY_etype(ST_type(st_shared_array));
	else if(TY_kind(ST_type(st_shared_array)) == KIND_POINTER)
		elem_ty = TY_pointed(ST_type(st_shared_array));
	else
    	Fail_FmtAssertion("invalid TY_kind. It must be ARRAY or POINTER in Gen_ReductionIfElseBlock1");	
			
	Init0 = ACC_LoadDeviceSharedArrayElem(Init0, st_shared_array);	
	Init0 = WN_Iload(TY_mtype(elem_ty), 0,  elem_ty, Init0);
	
	Init0 = WN_Binary(ReductionOpr, TY_mtype(ST_type(WN_st(wn_mySum))), WN_COPY_Tree(wn_mySum), Init0);
	Init0 = WN_Stid(TY_mtype(ST_type(WN_st(wn_mySum))), 0, WN_st(wn_mySum),
								ST_type(WN_st(wn_mySum)), Init0);	
	WN_INSERT_BlockLast(wn_InnerBlock,  Init0);


	Init0 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_btid), st_shared_array);	
	Init0 = WN_Istore(TY_mtype(elem_ty), 0, Make_Pointer_Type(elem_ty), Init0, WN_COPY_Tree(wn_mySum));	
	WN_INSERT_BlockLast(wn_InnerBlock,  Init0);
	Init0 = WN_CreateIf(wn_IfInnerTest, wn_InnerBlock, WN_CreateBlock());
	WN_INSERT_BlockLast(wn_OuterBlock,  Init0);
	if(ilimit > 64)//same warp, no necessary 
		WN_INSERT_BlockLast(wn_OuterBlock,  wn_CallSyncThreads);
	Init0 = WN_CreateIf(wn_IfOuterTest, wn_OuterBlock, WN_CreateBlock());
	return Init0;
}*/


static WN* Gen_ReductionIfElseBlock1(WN* wn_blocksize, WN* wn_tid, ST* st_shared_array,
						WN* wn_mySum, unsigned int ilimit, OPERATOR ReductionOpr)
{
	WN* wn_IfOuterTest = WN_Relational (OPR_GE, MTYPE_U4, 
								WN_COPY_Tree(wn_blocksize), 
								WN_Intconst(MTYPE_U4, ilimit));
	WN* wn_IfInnerTest = WN_Relational (OPR_LT, MTYPE_U4, 
								WN_COPY_Tree(wn_tid), 
								WN_Intconst(MTYPE_U4, ilimit/2));;
	WN* wn_OuterBlock = WN_CreateBlock();
	WN* wn_InnerBlock = WN_CreateBlock();
	WN* wn_CallSyncThreads = Gen_Sync_Threads();
	
	WN* Init0 = WN_Binary(OPR_ADD, MTYPE_U4, 
					WN_COPY_Tree(wn_tid), WN_Intconst(MTYPE_U4, ilimit/2));
	TY_IDX elem_ty;
	if(TY_kind(ST_type(st_shared_array)) == KIND_ARRAY)
		elem_ty = TY_etype(ST_type(st_shared_array));
	else if(TY_kind(ST_type(st_shared_array)) == KIND_POINTER)
		elem_ty = TY_pointed(ST_type(st_shared_array));
	else
    	Fail_FmtAssertion("invalid TY_kind. It must be ARRAY or POINTER in Gen_ReductionIfElseBlock1");	
			
	Init0 = ACC_LoadDeviceSharedArrayElem(Init0, st_shared_array);	
	Init0 = WN_Iload(TY_mtype(elem_ty), 0,  elem_ty, Init0);
	
	Init0 = ACC_GEN_Reduction_Binary_Op(ReductionOpr, TY_mtype(ST_type(WN_st(wn_mySum))), WN_COPY_Tree(wn_mySum), Init0);
	Init0 = WN_Stid(TY_mtype(ST_type(WN_st(wn_mySum))), 0, WN_st(wn_mySum),
								ST_type(WN_st(wn_mySum)), Init0);	
	WN* Init1 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_shared_array);	
	Init1 = WN_Istore(TY_mtype(elem_ty), 0, Make_Pointer_Type(elem_ty), 
								Init1, WN_COPY_Tree(wn_mySum));	
	
	if(ilimit > 64)
	{
		WN_INSERT_BlockLast(wn_InnerBlock,  Init0);
		WN_INSERT_BlockLast(wn_InnerBlock,  Init1);
		Init0 = WN_CreateIf(wn_IfInnerTest, wn_InnerBlock, WN_CreateBlock());
		WN_INSERT_BlockLast(wn_OuterBlock,  Init0);
		WN_INSERT_BlockLast(wn_OuterBlock,  wn_CallSyncThreads);
		Init0 = WN_CreateIf(wn_IfOuterTest, wn_OuterBlock, WN_CreateBlock());
	}
	else
	{
		WN_INSERT_BlockLast(wn_OuterBlock,  Init0);
		WN_INSERT_BlockLast(wn_OuterBlock,  Init1);
		Init0 = WN_CreateIf(wn_IfOuterTest, wn_OuterBlock, WN_CreateBlock());
	}

	
	return Init0;
}


static WN* Gen_ReductionIfElseBlock2(WN* wn_blocksize, WN* wn_tid, ST* st_shared_array,
						WN* wn_mySum, unsigned int ilimit, OPERATOR ReductionOpr)
{
	WN* wn_IfOuterTest = WN_Relational (OPR_GE, MTYPE_U4, 
								WN_COPY_Tree(wn_blocksize), 
								WN_Intconst(MTYPE_U4, ilimit));
	//WN* wn_IfInnerTest = WN_Relational (OPR_LT, TY_mtype(ST_type(WN_st(wn_tid))), 
	//							WN_COPY_Tree(wn_tid), 
	//							WN_Intconst(MTYPE_U4, ilimit/2));;
	WN* wn_OuterBlock = WN_CreateBlock();
	//WN* wn_InnerBlock = WN_CreateBlock();
	//WN* wn_CallSyncThreads = Gen_Sync_Threads();
	TY_IDX elem_ty;
	if(TY_kind(ST_type(st_shared_array)) == KIND_ARRAY)
		elem_ty = TY_etype(ST_type(st_shared_array));
	else if(TY_kind(ST_type(st_shared_array)) == KIND_POINTER)
		elem_ty = TY_pointed(ST_type(st_shared_array));
	else
    	Fail_FmtAssertion("invalid TY_kind. It must be ARRAY or POINTER in Gen_ReductionIfElseBlock2");	
	
	WN* Init0 = WN_Binary(OPR_ADD, MTYPE_U4, 
					WN_COPY_Tree(wn_tid), WN_Intconst(MTYPE_U4, ilimit/2));
	Init0 = ACC_LoadDeviceSharedArrayElem(Init0, st_shared_array);	
	Init0 = WN_Iload(TY_mtype(elem_ty), 0,  
									elem_ty, Init0);
	
	Init0 = ACC_GEN_Reduction_Binary_Op(ReductionOpr, TY_mtype(ST_type(WN_st(wn_mySum))), WN_COPY_Tree(wn_mySum), Init0);
	Init0 = WN_Stid(TY_mtype(ST_type(WN_st(wn_mySum))), 0, WN_st(wn_mySum),
								ST_type(WN_st(wn_mySum)), Init0);	
	WN_INSERT_BlockLast(wn_OuterBlock,  Init0);


	Init0 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_shared_array);	
	Init0 = WN_Istore(TY_mtype(elem_ty), 0, 
					Make_Pointer_Type(elem_ty), Init0, WN_COPY_Tree(wn_mySum));	
	WN_INSERT_BlockLast(wn_OuterBlock,  Init0);
	//Init0 = WN_CreateIf(wn_IfInnerTest, wn_InnerBlock, WN_CreateBlock());
	//WN_INSERT_BlockLast(wn_OuterBlock,  Init0);
	//WN_INSERT_BlockLast(wn_OuterBlock,  wn_CallSyncThreads);
	Init0 = WN_CreateIf(wn_IfOuterTest, wn_OuterBlock, WN_CreateBlock());
	return Init0;
}

/*return the reduction device function name, every type of reduction will be return once.
For example,  if "+" was generated once, compiler won't generate another "+" kernel. 
It will just return the previous function name.
This function is only valid for Kernels outter Loop reduction.
*/
static ST* ACC_GenerateWorkerVectorReduction_unrolling(ACC_ReductionMap* pReduction_map)
{
	
	OPERATOR ReductionOpr = pReduction_map->ReductionOpr;
	WN* reduction_params = NULL;
	//reductionmap	
	ST *old_st = pReduction_map->hostName;
	INT64 acc_dtype = 
			GetKernelParamType(old_st);
	//WN_OFFSET old_offset = WN_offsetx(reduction_node);		
    TY_IDX ty = ST_type(old_st);
	ty = MTYPE_To_TY(TY_mtype(ty));
    TY_KIND kind = TY_kind(ty);//ST_name(old_st)
    //char* localname; //= (char *) alloca(strlen(ST_name(old_st))+10);
	//sprintf ( localname, "__device_reduction_%s", ST_name(old_st));
	ST* st_kernel = ACC_Get_Reduction_devices(pReduction_map);
	if(st_kernel)
		return st_kernel;
	
	//generate new reuction kernels for this type and this operator
	acc_psymtab = CURRENT_SYMTAB;
	acc_ppuinfo = Current_PU_Info;
	acc_pmaptab = Current_Map_Tab;
	ACC_Push_Some_Globals( );
	ACC_Create_Reduction_Kernels(PAR_FUNC_ACC_DEVICE, pReduction_map);
	//////////////////////////////////////////////////////
	//make local declaress
    char* localname = (char *) alloca(strlen(ST_name(old_st))+10);
	ST* st_shared_array = st_input_data; //this is actually a shared memory buffer pointer 
	
	
	//WN* threadidx = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_x)), 
	//				0, glbl_threadIdx_x, ST_type(glbl_threadIdx_x));
	
	ST* st_tid = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_tid,
	  Save_Str("tid"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));
	WN* wn_tid = WN_Ldid(TY_mtype(ST_type(st_tid)), 
					0, st_tid, ST_type(st_tid));

	//my sum
	ST* st_mySum = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_mySum,
	  Save_Str("mySum"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty);
	WN* wn_mySum = WN_Ldid(TY_mtype(ST_type(st_mySum)), 
					0, st_mySum, ST_type(st_mySum));
	//blocksize
	ST* st_blockSize = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_blockSize,
	  Save_Str("blockSize"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));
	WN* wn_blockSize = WN_Ldid(TY_mtype(ST_type(st_blockSize)), 
					0, st_blockSize, ST_type(st_blockSize));
	//nextpow2
	ST* st_nextpow2 = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_nextpow2,
	  Save_Str("nextpow2"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));	
	WN* wn_nextpow2 = WN_Ldid(TY_mtype(ST_type(st_nextpow2)), 
					0, st_nextpow2, ST_type(st_nextpow2));

	
	//prevpow2
	ST* st_prevpow2 = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_prevpow2,
	  Save_Str("prevpow2"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));	
	WN* wn_prevpow2 = WN_Ldid(TY_mtype(ST_type(st_prevpow2)), 
					0, st_prevpow2, ST_type(st_prevpow2));
	
	
	//Set up predefined variable in CUDA
	WN* wn_threadidx = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_x)), 
					0, glbl_threadIdx_x, ST_type(glbl_threadIdx_x));
	WN* wn_threadidy = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_y)), 
					0, glbl_threadIdx_y, ST_type(glbl_threadIdx_y));
	WN* wn_threadidz = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_z)), 
					0, glbl_threadIdx_z, ST_type(glbl_threadIdx_z));
	
	WN* wn_blockidx = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_x)), 
					0, glbl_blockIdx_x, ST_type(glbl_blockIdx_x));
	WN* wn_blockidy = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_y)), 
					0, glbl_blockIdx_y, ST_type(glbl_blockIdx_y));
	WN* wn_blockidz = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_z)), 
					0, glbl_blockIdx_z, ST_type(glbl_blockIdx_z));
	
	WN* wn_blockdimx = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_x)), 
					0, glbl_blockDim_x, ST_type(glbl_blockDim_x));
	WN* wn_blockdimy = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_y)), 
					0, glbl_blockDim_y, ST_type(glbl_blockDim_y));
	WN* wn_blockdimz = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_z)), 
					0, glbl_blockDim_z, ST_type(glbl_blockDim_z));
	
	WN* wn_griddimx = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_x)), 
					0, glbl_gridDim_x, ST_type(glbl_gridDim_x));
	WN* wn_griddimy = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_y)), 
					0, glbl_gridDim_y, ST_type(glbl_gridDim_y));
	WN* wn_griddimz = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_z)), 
					0, glbl_gridDim_z, ST_type(glbl_gridDim_z));


	WN* Init0 = NULL;
	//pow2 alignment first
	Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(st_nextpow2)), 
					WN_COPY_Tree(wn_blockdimx), WN_COPY_Tree(wn_blockdimy));
	Init0 = WN_Stid(TY_mtype(ST_type(st_nextpow2)), 0, 
					st_nextpow2, ST_type(st_nextpow2), Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	//gen aligment stmt
	Gen_Next_Pow2DeviceStmt(wn_nextpow2, acc_reduction_func);

	//prevpow2 = nextpow2 >> 1
	Init0 = WN_Binary(OPR_ASHR, TY_mtype(ST_type(st_nextpow2)), 
					WN_COPY_Tree(wn_nextpow2), 
					WN_Intconst(TY_mtype(ST_type(st_nextpow2)), 1));
	Init0 = WN_Stid(TY_mtype(ST_type(st_prevpow2)), 0, 
					st_prevpow2, ST_type(st_prevpow2), Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);


	//tid = threadIdx.y * blockdim.x + threadIdx.x
	Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockIdx_x)), 
						WN_COPY_Tree(wn_threadidy), WN_COPY_Tree(wn_blockdimx));
	Init0 = WN_Binary(OPR_ADD, TY_mtype(ST_type(glbl_blockIdx_x)), 
						Init0, WN_COPY_Tree(wn_threadidx));
	Init0 = WN_Stid(TY_mtype(ST_type(st_tid)), 0, st_tid, ST_type(st_tid), Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);	
	//blocksize = blockdimx*blockdimy
	Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockIdx_x)), 
									WN_COPY_Tree(wn_blockdimx), WN_COPY_Tree(wn_blockdimy));
	Init0 = WN_Stid(TY_mtype(ST_type(st_blockSize)), 0, st_blockSize, 
									ST_type(st_blockSize), Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);

	/////////////////////////////////////////////////////////////////////////////////	
	WN* wn_IfTest1 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_tid), 
								WN_COPY_Tree(wn_prevpow2));
	WN* wn_IfBody1 = WN_CreateBlock();
	/////////////////////////////////////////////////////////////////////////////////
	WN* wn_nextIndex = WN_Binary(OPR_ADD, TY_mtype(ST_type(glbl_blockIdx_x)), 
							WN_COPY_Tree(wn_tid), WN_COPY_Tree(wn_prevpow2));
	WN* wn_IfTest11 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_nextIndex), WN_COPY_Tree(wn_blockSize));
	WN* wn_IfBody11 = WN_CreateBlock();
	
	WN* wn_shArr1 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_nextIndex), st_shared_array);	
	wn_shArr1 = WN_Iload(TY_mtype(ty), 0,  ty, wn_shArr1);
	
	WN* wn_shArr2 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_shared_array);	
	wn_shArr2 = WN_Iload(TY_mtype(ty), 0,  ty, wn_shArr2);
	
	Init0 = ACC_GEN_Reduction_Binary_Op(ReductionOpr, TY_mtype(ty), wn_shArr2, wn_shArr1);

	
	WN* wn_shArr3 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_shared_array);
	wn_shArr3 = WN_Istore(TY_mtype(ty), 0, Make_Pointer_Type(ty), wn_shArr3, Init0);
	WN_INSERT_BlockLast( wn_IfBody11,  wn_shArr3);
	WN* wn_ifThenElse = WN_CreateIf(wn_IfTest11, wn_IfBody11, WN_CreateBlock());
	////////////////////////////////////////////////////////////////////////////////////////
	WN_INSERT_BlockLast( wn_IfBody1,  wn_ifThenElse);
	wn_ifThenElse = WN_CreateIf(wn_IfTest1, wn_IfBody1, WN_CreateBlock());
	WN_INSERT_BlockLast( acc_reduction_func,  wn_ifThenElse);
	//init sum
	wn_shArr1 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_shared_array);	
	wn_shArr1 = WN_Iload(TY_mtype(ty), 0,  ty, wn_shArr1);
	//Init0 = ACC_Get_Init_Value_of_Reduction(ReductionOpr);
	Init0 = WN_Stid(TY_mtype(ST_type(st_mySum)), 0, st_mySum, ST_type(st_mySum), wn_shArr1);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Gen_Sync_Threads());
	//mySum += g_idata[btid];
	/*Init0 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_btid), st_input_data);
	Init0 = WN_Iload(TY_mtype(TY_pointed(ST_type(st_input_data))), 0,  
									TY_pointed(ST_type(st_input_data)), Init0);
	Init0 = WN_Binary(ReductionOpr, TY_mtype(ST_type(st_mySum)), WN_COPY_Tree(wn_mySum), Init0);
	Init0 = WN_Stid(TY_mtype(ST_type(st_mySum)), 0, st_mySum, ST_type(st_mySum), Init0);*/


	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_shared_array, wn_mySum, 512, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_shared_array, wn_mySum, 256, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_shared_array, wn_mySum, 128, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);

	
	WN* wn_IfTest2 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_tid), 
								WN_Intconst(MTYPE_U4, 32));
	WN* wn_IfBody2 = WN_CreateBlock();
	
    localname = (char *) alloca(strlen(ST_name(old_st))+10);
	sprintf ( localname, "__smem_%s", ST_name(old_st));
	Set_TY_is_volatile(ty);
	TY_IDX ty_pointer = Make_Pointer_Type(ty);
	ST* st_smem_pointer = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_smem_pointer,
	  Save_Str(localname),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty_pointer);
	
	
    Init0 = WN_Ldid(Pointer_type, 0, st_shared_array, ST_type(st_shared_array));
	Init0 = WN_Stid(TY_mtype(ST_type(st_smem_pointer)), 0, 
					st_smem_pointer, ST_type(st_smem_pointer), Init0);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 64, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 32, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 16, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 8, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 4, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 2, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	WN* wn_ifThenElse2 = WN_CreateIf(wn_IfTest2, wn_IfBody2, WN_CreateBlock());
	WN_INSERT_BlockLast( acc_reduction_func,  wn_ifThenElse2);
    WN_INSERT_BlockLast( acc_reduction_func,  Gen_Sync_Threads());

	
	
	
	//Init0 = WN_Binary(OPR_ADD, TY_mtype(ST_type(WN_st(wn_tid))), 
	//				WN_COPY_Tree(wn_tid), WN_Intconst(MTYPE_U4, ilimit/2));
	/*Init0 = ACC_LoadDeviceSharedArrayElem(WN_Intconst(MTYPE_U4, 0), st_shared_array);	
	Init0 = WN_Iload(TY_mtype(TY_pointed(ST_type(st_shared_array))), 0,  
									TY_pointed(ST_type(st_shared_array)), Init0);
	
    WN* wn_return = WN_CreateReturn_Val (OPR_RETURN_VAL, TY_mtype(ST_type(old_st)), MTYPE_V, Init0);
	
	WN_INSERT_BlockLast( acc_reduction_func,  wn_return);//*/

	
	////////////////////////////////////////////////////////////////
	//restore info
	//ACC_Transfer_Maps ( acc_pmaptab, acc_cmaptab, acc_reduction_func, 
	//	  PU_Info_regions_ptr(Current_PU_Info) );

	/* Create a new dependence graph for the child  and move all the 
	 appropriate vertices from the parent to the child graph */

	//Current_Map_Tab = acc_cmaptab;
	//ACC_Fix_Dependence_Graph ( acc_ppuinfo, Current_PU_Info, acc_reduction_func); 
	//Current_Map_Tab = acc_pmaptab;

  
	/* Restore parent information. */

	CURRENT_SYMTAB = acc_psymtab;
	Current_PU_Info = acc_ppuinfo;
	Current_pu = &Current_PU_Info_pu();
	Current_Map_Tab = acc_pmaptab;
	ACC_Pop_Some_Globals( );

	return acc_reduction_proc;
}

/*return the reduction device function name, every type of reduction will be return once.
For example,  if "+" was generated once, compiler won't generate another "+" kernel. 
It will just return the previous function name.
This function is only valid for Kernels outter Loop reduction.
*/
static ST* ACC_GenerateWorkerReduction_unrolling(ACC_ReductionMap* pReduction_map)
{
	
	OPERATOR ReductionOpr = pReduction_map->ReductionOpr;
	WN* reduction_params = NULL;
	//reductionmap	
	ST *old_st = pReduction_map->hostName;
	INT64 acc_dtype = 
			GetKernelParamType(old_st);
	//WN_OFFSET old_offset = WN_offsetx(reduction_node);		
    TY_IDX ty = ST_type(old_st);
	ty = MTYPE_To_TY(TY_mtype(ty));
    TY_KIND kind = TY_kind(ty);//ST_name(old_st)
    //char* localname; //= (char *) alloca(strlen(ST_name(old_st))+10);
	//sprintf ( localname, "__device_reduction_%s", ST_name(old_st));
	ST* st_kernel = ACC_Get_Reduction_devices(pReduction_map);
	if(st_kernel)
		return st_kernel;
	
	//generate new reuction kernels for this type and this operator
	acc_psymtab = CURRENT_SYMTAB;
	acc_ppuinfo = Current_PU_Info;
	acc_pmaptab = Current_Map_Tab;
	ACC_Push_Some_Globals( );
	ACC_Create_Reduction_Kernels(PAR_FUNC_ACC_DEVICE, pReduction_map);
	//////////////////////////////////////////////////////
	//make local declaress
    char* localname = (char *) alloca(strlen(ST_name(old_st))+10);
	/*sprintf ( localname, "__sdata_%s", ST_name(old_st));
	TY_IDX ty_array = Make_Pointer_Type(ty);//Make_Array_Type(TY_mtype(ty), 1, 1024);
	ST* st_shared_array = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_shared_array,
	  Save_Str(localname),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty_array);
	Set_ST_ACC_shared_array(*st_shared_array);*/
	ST* st_shared_array = st_input_data;
	
	
	//WN* threadidx = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_x)), 
	//				0, glbl_threadIdx_x, ST_type(glbl_threadIdx_x));
	
	ST* st_tid = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_tid,
	  Save_Str("tid"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));
	WN* wn_tid = WN_Ldid(TY_mtype(ST_type(st_tid)), 
					0, st_tid, ST_type(st_tid));
	//i
	ST* st_loop_index = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_loop_index,
	  Save_Str("i"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));
	WN* wn_loop_index = WN_Ldid(TY_mtype(ST_type(st_loop_index)), 
					0, st_loop_index, ST_type(st_loop_index));
	//
	ST* st_gridSize = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_gridSize,
	  Save_Str("gridSize"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));
	WN* wn_gridSize = WN_Ldid(TY_mtype(ST_type(st_gridSize)), 
					0, st_gridSize, ST_type(st_gridSize));
	//my sum
	ST* st_mySum = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_mySum,
	  Save_Str("mySum"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty);
	WN* wn_mySum = WN_Ldid(TY_mtype(ST_type(st_mySum)), 
					0, st_mySum, ST_type(st_mySum));
	

	//nextpow2
	ST* st_nextpow2 = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_nextpow2,
	  Save_Str("nextpow2"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));	
	WN* wn_nextpow2 = WN_Ldid(TY_mtype(ST_type(st_nextpow2)), 
					0, st_nextpow2, ST_type(st_nextpow2));

	
	//prevpow2
	ST* st_prevpow2 = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_prevpow2,
	  Save_Str("prevpow2"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));	
	WN* wn_prevpow2 = WN_Ldid(TY_mtype(ST_type(st_prevpow2)), 
					0, st_prevpow2, ST_type(st_prevpow2));
	
	
	WN* wn_blocksize = WN_Ldid(TY_mtype(ST_type(st_gridSize)), 
					0, st_gridSize, ST_type(st_gridSize));
	//Set up predefined variable in CUDA
	WN* wn_threadidx = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_x)), 
					0, glbl_threadIdx_x, ST_type(glbl_threadIdx_x));
	WN* wn_threadidy = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_y)), 
					0, glbl_threadIdx_y, ST_type(glbl_threadIdx_y));
	WN* wn_threadidz = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_z)), 
					0, glbl_threadIdx_z, ST_type(glbl_threadIdx_z));
	
	WN* wn_blockidx = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_x)), 
					0, glbl_blockIdx_x, ST_type(glbl_blockIdx_x));
	WN* wn_blockidy = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_y)), 
					0, glbl_blockIdx_y, ST_type(glbl_blockIdx_y));
	WN* wn_blockidz = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_z)), 
					0, glbl_blockIdx_z, ST_type(glbl_blockIdx_z));
	
	WN* wn_blockdimx = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_x)), 
					0, glbl_blockDim_x, ST_type(glbl_blockDim_x));
	WN* wn_blockdimy = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_y)), 
					0, glbl_blockDim_y, ST_type(glbl_blockDim_y));
	WN* wn_blockdimz = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_z)), 
					0, glbl_blockDim_z, ST_type(glbl_blockDim_z));
	
	WN* wn_griddimx = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_x)), 
					0, glbl_gridDim_x, ST_type(glbl_gridDim_x));
	WN* wn_griddimy = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_y)), 
					0, glbl_gridDim_y, ST_type(glbl_gridDim_y));
	WN* wn_griddimz = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_z)), 
					0, glbl_gridDim_z, ST_type(glbl_gridDim_z));
	
	WN* Init0 = NULL;
	//pow2 alignment first
	//Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(st_nextpow2)), 
	//				WN_COPY_Tree(blockdimx), WN_COPY_Tree(blockdimy));
	Init0 = WN_Stid(TY_mtype(ST_type(st_nextpow2)), 0, 
					st_nextpow2, ST_type(st_nextpow2), WN_COPY_Tree(wn_blockdimy));
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	//gen aligment stmt
	Gen_Next_Pow2DeviceStmt(wn_nextpow2, acc_reduction_func);

	//prevpow2 = nextpow2 >> 1
	Init0 = WN_Binary(OPR_ASHR, TY_mtype(ST_type(st_nextpow2)), 
					WN_COPY_Tree(wn_nextpow2), 
					WN_Intconst(TY_mtype(ST_type(st_nextpow2)), 1));
	Init0 = WN_Stid(TY_mtype(ST_type(st_prevpow2)), 0, 
					st_prevpow2, ST_type(st_prevpow2), Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);	


	//tid = threadIdx.y * blockdim.x + threadIdx.x
	Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockIdx_x)), 
						WN_COPY_Tree(wn_threadidy), WN_COPY_Tree(wn_blockdimx));
	Init0 = WN_Binary(OPR_ADD, TY_mtype(ST_type(glbl_blockIdx_x)), 
						Init0, WN_COPY_Tree(wn_threadidx));
	Init0 = WN_Stid(TY_mtype(ST_type(st_tid)), 0, st_tid, ST_type(st_tid), Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);	
	
	//Init0 = ACC_Get_Init_Value_of_Reduction(ReductionOpr);
	//Init0 = WN_Stid(TY_mtype(ST_type(st_mySum)), 0, st_mySum, ST_type(st_mySum), Init0);
	//WN_INSERT_BlockLast( acc_reduction_func,  Init0);

	//////////////////////////////////////////////////////////////////////////////////	
	WN* wn_IfTest1 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_tid), 
								WN_COPY_Tree(wn_prevpow2));
	WN* wn_IfBody1 = WN_CreateBlock();
	/////////////////////////////////////////////////////////////////////////////////
	WN* wn_nextIndex = WN_Binary(OPR_ADD, TY_mtype(ST_type(glbl_blockIdx_x)), 
							WN_COPY_Tree(wn_tid), WN_COPY_Tree(wn_prevpow2));
	WN* wn_IfTest11 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_nextIndex), WN_COPY_Tree(wn_blockdimy));
	WN* wn_IfBody11 = WN_CreateBlock();
	
	WN* wn_shArr1 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_nextIndex), st_shared_array);	
	wn_shArr1 = WN_Iload(TY_mtype(ty), 0,  ty, wn_shArr1);
	
	WN* wn_shArr2 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_shared_array);	
	wn_shArr2 = WN_Iload(TY_mtype(ty), 0,  ty, wn_shArr2);
	
	Init0 = ACC_GEN_Reduction_Binary_Op(ReductionOpr, TY_mtype(ty), wn_shArr2, wn_shArr1);

	
	WN* wn_shArr3 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_shared_array);
	wn_shArr3 = WN_Istore(TY_mtype(ty), 0, Make_Pointer_Type(ty), wn_shArr3, Init0);
	WN_INSERT_BlockLast( wn_IfBody11,  wn_shArr3);
	WN* wn_ifThenElse = WN_CreateIf(wn_IfTest11, wn_IfBody11, WN_CreateBlock());
	////////////////////////////////////////////////////////////////////////////////////////
	WN_INSERT_BlockLast( wn_IfBody1,  wn_ifThenElse);
	wn_ifThenElse = WN_CreateIf(wn_IfTest1, wn_IfBody1, WN_CreateBlock());

	WN_INSERT_BlockLast( acc_reduction_func,  wn_ifThenElse);
	////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\	
	//init sum
	wn_shArr1 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_shared_array);	
	wn_shArr1 = WN_Iload(TY_mtype(ty), 0,  ty, wn_shArr1);
	Init0 = WN_Stid(TY_mtype(ST_type(st_mySum)), 0, st_mySum, ST_type(st_mySum), wn_shArr1);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Gen_Sync_Threads());	
	
	/////////////////////////////////////////////////////////////////////////////////
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_shared_array, wn_mySum, 512, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_shared_array, wn_mySum, 256, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_shared_array, wn_mySum, 128, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);

	
	WN* wn_IfTest2 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_tid), 
								WN_Intconst(MTYPE_U4, 32));
	WN* wn_IfBody2 = WN_CreateBlock();
	
    localname = (char *) alloca(strlen(ST_name(old_st))+10);
	sprintf ( localname, "__smem_%s", ST_name(old_st));
	Set_TY_is_volatile(ty);
	TY_IDX ty_pointer = Make_Pointer_Type(ty);
	ST* st_smem_pointer = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_smem_pointer,
	  Save_Str(localname),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty_pointer);
	
	
    Init0 = WN_Ldid(Pointer_type, 0, st_shared_array, ST_type(st_shared_array));
	Init0 = WN_Stid(TY_mtype(ST_type(st_smem_pointer)), 0, 
					st_smem_pointer, ST_type(st_smem_pointer), Init0);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 64, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 32, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 16, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 8, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 4, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 2, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	WN* wn_ifThenElse2 = WN_CreateIf(wn_IfTest2, wn_IfBody2, WN_CreateBlock());
	WN_INSERT_BlockLast( acc_reduction_func,  wn_ifThenElse2);
    WN_INSERT_BlockLast( acc_reduction_func,  Gen_Sync_Threads());

	
	
	
	//Init0 = WN_Binary(OPR_ADD, TY_mtype(ST_type(WN_st(wn_tid))), 
	//				WN_COPY_Tree(wn_tid), WN_Intconst(MTYPE_U4, ilimit/2));
	/*Init0 = ACC_LoadDeviceSharedArrayElem(WN_Intconst(MTYPE_U4, 0), st_shared_array);	
	Init0 = WN_Iload(TY_mtype(TY_pointed(ST_type(st_shared_array))), 0,  
									TY_pointed(ST_type(st_shared_array)), Init0);
	
    WN* wn_return = WN_CreateReturn_Val (OPR_RETURN_VAL, TY_mtype(ST_type(old_st)), MTYPE_V, Init0);
	
	WN_INSERT_BlockLast( acc_reduction_func,  wn_return);//*/
	/* Restore parent information. */

	CURRENT_SYMTAB = acc_psymtab;
	Current_PU_Info = acc_ppuinfo;
	Current_pu = &Current_PU_Info_pu();
	Current_Map_Tab = acc_pmaptab;
	ACC_Pop_Some_Globals( );

	return acc_reduction_proc;
}


/*return the reduction device function name, every type of reduction will be return once.
For example,  if "+" was generated once, compiler won't generate another "+" kernel. 
It will just return the previous function name.
This function is only valid for Kernels outter Loop reduction.
*/
static ST* ACC_GenerateVectorReduction_unrolling(ACC_ReductionMap* pReduction_map)
{
	
	OPERATOR ReductionOpr = pReduction_map->ReductionOpr;
	WN* reduction_params = NULL;
	//reductionmap	
	ST *old_st = pReduction_map->hostName;
	INT64 acc_dtype = 
			GetKernelParamType(old_st);
	//WN_OFFSET old_offset = WN_offsetx(reduction_node);		
    TY_IDX ty = ST_type(old_st);
	ty = MTYPE_To_TY(TY_mtype(ty));
    TY_KIND kind = TY_kind(ty);//ST_name(old_st)
    //char* localname; //= (char *) alloca(strlen(ST_name(old_st))+10);
	//sprintf ( localname, "__device_reduction_%s", ST_name(old_st));
	ST* st_kernel = ACC_Get_Reduction_devices(pReduction_map);
	if(st_kernel)
		return st_kernel;
	
	
	//generate new reuction kernels for this type and this operator
	acc_psymtab = CURRENT_SYMTAB;
	acc_ppuinfo = Current_PU_Info;
	acc_pmaptab = Current_Map_Tab;
	ACC_Push_Some_Globals( );
	ACC_Create_Reduction_Kernels(PAR_FUNC_ACC_DEVICE, pReduction_map);
	//////////////////////////////////////////////////////
	//make local declaress
    char* localname = (char *) alloca(strlen(ST_name(old_st))+10);
	ST* st_shared_array = st_input_data; //this is actually a shared memory buffer pointer 
	
	
	//WN* threadidx = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_x)), 
	//				0, glbl_threadIdx_x, ST_type(glbl_threadIdx_x));
	
	ST* st_tid = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_tid,
	  Save_Str("tid"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));
	WN* wn_tid = WN_Ldid(TY_mtype(ST_type(st_tid)), 
					0, st_tid, ST_type(st_tid));

	//block tid, because blockdim.y may larger than 1.	
	/*ST* st_btid = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_btid,
	  Save_Str("btid"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));
	WN* wn_btid = WN_Ldid(TY_mtype(ST_type(st_btid)), 
					0, st_btid, ST_type(st_btid));*/
	
	//my sum
	ST* st_mySum = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_mySum,
	  Save_Str("mySum"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty);
	WN* wn_mySum = WN_Ldid(TY_mtype(ST_type(st_mySum)), 
					0, st_mySum, ST_type(st_mySum));
	//nextpow2
	ST* st_nextpow2 = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_nextpow2,
	  Save_Str("nextpow2"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));	
	WN* wn_nextpow2 = WN_Ldid(TY_mtype(ST_type(st_nextpow2)), 
					0, st_nextpow2, ST_type(st_nextpow2));

	
	//prevpow2
	ST* st_prevpow2 = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_prevpow2,
	  Save_Str("prevpow2"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));	
	WN* wn_prevpow2 = WN_Ldid(TY_mtype(ST_type(st_prevpow2)), 
					0, st_prevpow2, ST_type(st_prevpow2));
	
    localname = (char *) alloca(strlen(ST_name(old_st))+30);
	sprintf ( localname, "__smem_local_%s", ST_name(old_st));
	TY_IDX ty_pointer = Make_Pointer_Type(ty);
	ST* st_smemlocal_pointer = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_smemlocal_pointer,
	  Save_Str(localname),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty_pointer);
	WN* wn_localMem = WN_Ldid(Pointer_type, 0, st_smemlocal_pointer, 
				ST_type(st_smemlocal_pointer));
	
	//Set up predefined variable in CUDA
	WN* wn_threadidx = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_x)), 
					0, glbl_threadIdx_x, ST_type(glbl_threadIdx_x));
	WN* wn_threadidy = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_y)), 
					0, glbl_threadIdx_y, ST_type(glbl_threadIdx_y));
	WN* wn_threadidz = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_z)), 
					0, glbl_threadIdx_z, ST_type(glbl_threadIdx_z));
	
	WN* wn_blockidx = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_x)), 
					0, glbl_blockIdx_x, ST_type(glbl_blockIdx_x));
	WN* wn_blockidy = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_y)), 
					0, glbl_blockIdx_y, ST_type(glbl_blockIdx_y));
	WN* wn_blockidz = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_z)), 
					0, glbl_blockIdx_z, ST_type(glbl_blockIdx_z));
	
	WN* wn_blockdimx = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_x)), 
					0, glbl_blockDim_x, ST_type(glbl_blockDim_x));
	WN* wn_blockdimy = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_y)), 
					0, glbl_blockDim_y, ST_type(glbl_blockDim_y));
	WN* wn_blockdimz = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_z)), 
					0, glbl_blockDim_z, ST_type(glbl_blockDim_z));
	
	WN* wn_griddimx = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_x)), 
					0, glbl_gridDim_x, ST_type(glbl_gridDim_x));
	WN* wn_griddimy = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_y)), 
					0, glbl_gridDim_y, ST_type(glbl_gridDim_y));
	WN* wn_griddimz = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_z)), 
					0, glbl_gridDim_z, ST_type(glbl_gridDim_z));

	WN* wn_offset = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockIdx_x)),
								WN_COPY_Tree(wn_threadidy), WN_COPY_Tree(wn_blockdimx));
	wn_offset = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockIdx_x)),
		wn_offset, WN_Intconst(TY_mtype(ST_type(glbl_blockDim_x)),	TY_size(ty)));
	WN* wn_base = WN_Ldid(Pointer_type, 0, st_shared_array, ST_type(st_shared_array));
	wn_offset = WN_Binary(OPR_ADD, TY_mtype(ST_type(glbl_blockDim_x)),
		wn_offset, wn_base);
	wn_offset = WN_Stid(Pointer_type, 0, st_smemlocal_pointer, 
				ST_type(st_smemlocal_pointer), wn_offset); 
	WN_INSERT_BlockLast( acc_reduction_func,  wn_offset);

	
	WN* Init0 = NULL;
	//pow2 alignment first
	Init0 = WN_Stid(TY_mtype(ST_type(st_nextpow2)), 0, 
					st_nextpow2, ST_type(st_nextpow2), WN_COPY_Tree(wn_blockdimx));
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	//gen aligment stmt
	Gen_Next_Pow2DeviceStmt(wn_nextpow2, acc_reduction_func);

	//prevpow2 = nextpow2 >> 1
	Init0 = WN_Binary(OPR_ASHR, TY_mtype(ST_type(st_nextpow2)), 
					WN_COPY_Tree(wn_nextpow2), 
					WN_Intconst(TY_mtype(ST_type(st_nextpow2)), 1));
	Init0 = WN_Stid(TY_mtype(ST_type(st_prevpow2)), 0, 
					st_prevpow2, ST_type(st_prevpow2), Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);


	//tid in the block, btid = threadIdx.y * blockdim.x + threadIdx.x
	//Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockIdx_x)), 
	//					WN_COPY_Tree(wn_threadidy), WN_COPY_Tree(wn_blockdimx));
	//Init0 = WN_Binary(OPR_ADD, TY_mtype(ST_type(glbl_blockIdx_x)), 
	//					Init0, WN_COPY_Tree(wn_threadidx));
	//Init0 = WN_Stid(TY_mtype(ST_type(st_btid)), 0, st_btid, ST_type(st_btid), Init0);
	//WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	
	//tid in single y direction	
	Init0 = WN_Stid(TY_mtype(ST_type(st_tid)), 0, st_tid, ST_type(st_tid), WN_COPY_Tree(wn_threadidx));
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	
	//blocksize = blockdimx*blockdimy
	//Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockIdx_x)), 
	//								WN_COPY_Tree(blockdimx), WN_COPY_Tree(blockdimy));
	//Init0 = WN_Stid(TY_mtype(ST_type(st_blockSize)), 0, st_blockSize, 
	//								ST_type(st_blockSize), Init0);
	//WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	//
	//Init0 = ACC_Get_Init_Value_of_Reduction(ReductionOpr);
	//Init0 = WN_Stid(TY_mtype(ST_type(st_mySum)), 0, st_mySum, ST_type(st_mySum), Init0);
	//WN_INSERT_BlockLast( acc_reduction_func,  Init0);

	//
	
	WN* wn_IfTest1 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_tid), 
								WN_COPY_Tree(wn_prevpow2));
	WN* wn_IfBody1 = WN_CreateBlock();
	/////////////////////////////////////////////////////////////////////////////////
	WN* wn_nextIndex = WN_Binary(OPR_ADD, TY_mtype(ST_type(glbl_blockIdx_x)), 
							WN_COPY_Tree(wn_tid), WN_COPY_Tree(wn_prevpow2));
	//WN* wn_btidnextIndex = WN_Binary(OPR_ADD, TY_mtype(ST_type(glbl_blockIdx_x)), 
	//						WN_COPY_Tree(wn_btid), WN_COPY_Tree(wn_prevpow2));
	WN* wn_IfTest11 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_nextIndex), WN_COPY_Tree(wn_blockdimx));
	WN* wn_IfBody11 = WN_CreateBlock();
	
	WN* wn_shArr1 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_nextIndex), st_smemlocal_pointer);	
	wn_shArr1 = WN_Iload(TY_mtype(ty), 0,  ty, wn_shArr1);
	
	WN* wn_shArr2 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_smemlocal_pointer);	
	wn_shArr2 = WN_Iload(TY_mtype(ty), 0,  ty, wn_shArr2);
	
	Init0 = ACC_GEN_Reduction_Binary_Op(ReductionOpr, TY_mtype(ty), wn_shArr2, wn_shArr1);

	
	WN* wn_shArr3 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_smemlocal_pointer);
	wn_shArr3 = WN_Istore(TY_mtype(ty), 0, Make_Pointer_Type(ty), wn_shArr3, Init0);
	WN_INSERT_BlockLast( wn_IfBody11,  wn_shArr3);
	WN* wn_ifThenElse = WN_CreateIf(wn_IfTest11, wn_IfBody11, WN_CreateBlock());
	////////////////////////////////////////////////////////////////////////////////////////
	WN_INSERT_BlockLast( wn_IfBody1,  wn_ifThenElse);
	wn_ifThenElse = WN_CreateIf(wn_IfTest1, wn_IfBody1, WN_CreateBlock());
	WN_INSERT_BlockLast( acc_reduction_func,  wn_ifThenElse);
	////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\	
	////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\	
	//init sum
	wn_shArr1 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_smemlocal_pointer);	
	wn_shArr1 = WN_Iload(TY_mtype(ty), 0,  ty, wn_shArr1);
	Init0 = WN_Stid(TY_mtype(ST_type(st_mySum)), 0, st_mySum, ST_type(st_mySum), wn_shArr1);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Gen_Sync_Threads());
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smemlocal_pointer, wn_mySum, 512, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smemlocal_pointer, wn_mySum, 256, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smemlocal_pointer, wn_mySum, 128, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);

	
	WN* wn_IfTest2 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_tid), 
								WN_Intconst(MTYPE_U4, 32));
	WN* wn_IfBody2 = WN_CreateBlock();
	
    localname = (char *) alloca(strlen(ST_name(old_st))+10);
	sprintf ( localname, "__smem_%s", ST_name(old_st));
	Set_TY_is_volatile(ty);
	ty_pointer = Make_Pointer_Type(ty);
	ST* st_smem_pointer = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_smem_pointer,
	  Save_Str(localname),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty_pointer);	
	
    Init0 = WN_Ldid(Pointer_type, 0, st_smemlocal_pointer, ST_type(st_smemlocal_pointer));
	Init0 = WN_Stid(TY_mtype(ST_type(st_smem_pointer)), 0, 
					st_smem_pointer, ST_type(st_smem_pointer), Init0);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 64, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 32, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 16, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 8, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 4, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_prevpow2, wn_tid, st_smem_pointer, wn_mySum, 2, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	WN* wn_ifThenElse2 = WN_CreateIf(wn_IfTest2, wn_IfBody2, WN_CreateBlock());
	WN_INSERT_BlockLast( acc_reduction_func,  wn_ifThenElse2);
    WN_INSERT_BlockLast( acc_reduction_func,  Gen_Sync_Threads());
	
	 
	/* Restore parent information. */

	CURRENT_SYMTAB = acc_psymtab;
	Current_PU_Info = acc_ppuinfo;
	Current_pu = &Current_PU_Info_pu();
	Current_Map_Tab = acc_pmaptab;
	ACC_Pop_Some_Globals( );

	return acc_reduction_proc;
}


/*return the reduction kernels name, every type of reduction will be return once.
For example,  if "+" was generated once, compiler won't generate another "+" kernel. 
It will just return the previous function name.
This function is only valid for Kernels outter Loop reduction.
*/
static ST* ACC_GenerateReduction_Kernels_TopLoop(ACC_ReductionMap* pReduction_map)
{
	
	OPERATOR ReductionOpr = pReduction_map->ReductionOpr;
	WN* reduction_params = NULL;
	//reductionmap	
	ST *old_st = pReduction_map->hostName;
	INT64 acc_dtype = 
			GetKernelParamType(old_st);
	//WN_OFFSET old_offset = WN_offsetx(reduction_node);		
    TY_IDX ty = ST_type(old_st);
	ty = MTYPE_To_TY(TY_mtype(ty));
    TY_KIND kind = TY_kind(ty);//ST_name(old_st)
    //char* localname; //= (char *) alloca(strlen(ST_name(old_st))+10);
	//sprintf ( localname, "__device_reduction_%s", ST_name(old_st));
	ST* st_kernel = ACC_Get_Reduction_kernels(ReductionOpr, ty);
	if(st_kernel)
		return st_kernel;
	
	//generate new reuction kernels for this type and this operator
	acc_psymtab = CURRENT_SYMTAB;
	acc_ppuinfo = Current_PU_Info;
	acc_pmaptab = Current_Map_Tab;
	ACC_Push_Some_Globals( );
	ACC_Create_Reduction_Kernels(PAR_FUNC_ACC_KERNEL, pReduction_map);
	//////////////////////////////////////////////////////
	//make local declaress
    char* localname = (char *) alloca(strlen(ST_name(old_st))+10);
	sprintf ( localname, "__sdata_%s", ST_name(old_st));
	TY_IDX ty_array = Make_Pointer_Type(ty);	
  	if(acc_target_arch == ACC_ARCH_TYPE_APU)							
  		Set_TY_is_shared_mem(ty_array);
	ST* st_shared_array = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_shared_array,
	  Save_Str(localname),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty_array);
	Set_ST_ACC_shared_array(*st_shared_array);
	
  	if(acc_target_arch == ACC_ARCH_TYPE_APU)
    	{
		Set_ST_acc_type_class(st_shared_array, ST_ACC_TYPE_SHARED_ARRAY_FIXED);
    		Set_ST_acc_shared_array_size(st_shared_array, 512);
	}
	
	
	//WN* threadidx = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_x)), 
	//				0, glbl_threadIdx_x, ST_type(glbl_threadIdx_x));
	
	ST* st_tid = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_tid,
	  Save_Str("tid"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));
	WN* wn_tid = WN_Ldid(TY_mtype(ST_type(st_tid)), 
					0, st_tid, ST_type(st_tid));
	//i
	ST* st_loop_index = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_loop_index,
	  Save_Str("i"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));
	WN* wn_loop_index = WN_Ldid(TY_mtype(ST_type(st_loop_index)), 
					0, st_loop_index, ST_type(st_loop_index));
	//
	ST* st_gridSize = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_gridSize,
	  Save_Str("gridSize"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  Be_Type_Tbl(MTYPE_U4));
	WN* wn_gridSize = WN_Ldid(TY_mtype(ST_type(st_gridSize)), 
					0, st_gridSize, ST_type(st_gridSize));
	//my sum
	ST* st_mySum = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_mySum,
	  Save_Str("mySum"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty);
	WN* wn_mySum = WN_Ldid(TY_mtype(ST_type(st_mySum)), 
					0, st_mySum, ST_type(st_mySum));

	
	/*sprintf ( localname, "is_power2");
	ST* st_IsPow2 = New_ST( CURRENT_SYMTAB );
	ST_Init(st_IsPow2,
			Save_Str("is_power2"),
			CLASS_VAR,
			SCLASS_AUTO,
			EXPORT_LOCAL,
			Be_Type_Tbl(MTYPE_U4));
	WN* wn_IsPow2 = WN_Ldid(TY_mtype(ST_type(st_IsPow2)), 
					0, st_IsPow2, ST_type(st_IsPow2));*/
	
	
	//Set up predefined variable in CUDA
	WN* wn_threadidx = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_x)), 
					0, glbl_threadIdx_x, ST_type(glbl_threadIdx_x));
	WN* wn_threadidy = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_y)), 
					0, glbl_threadIdx_y, ST_type(glbl_threadIdx_y));
	WN* wn_threadidz = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_z)), 
					0, glbl_threadIdx_z, ST_type(glbl_threadIdx_z));
	
	WN* wn_blockidx = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_x)), 
					0, glbl_blockIdx_x, ST_type(glbl_blockIdx_x));
	WN* wn_blockidy = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_y)), 
					0, glbl_blockIdx_y, ST_type(glbl_blockIdx_y));
	WN* wn_blockidz = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_z)), 
					0, glbl_blockIdx_z, ST_type(glbl_blockIdx_z));
	
	WN* wn_blockdimx = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_x)), 
					0, glbl_blockDim_x, ST_type(glbl_blockDim_x));
	WN* wn_blockdimy = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_y)), 
					0, glbl_blockDim_y, ST_type(glbl_blockDim_y));
	WN* wn_blockdimz = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_z)), 
					0, glbl_blockDim_z, ST_type(glbl_blockDim_z));
	
	WN* wn_griddimx = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_x)), 
					0, glbl_gridDim_x, ST_type(glbl_gridDim_x));
	WN* wn_griddimy = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_y)), 
					0, glbl_gridDim_y, ST_type(glbl_gridDim_y));
	WN* wn_griddimz = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_z)), 
					0, glbl_gridDim_z, ST_type(glbl_gridDim_z));
	
	//load parameter: num of element
	WN* wn_num_elem = WN_Ldid(TY_mtype(ST_type(st_num_elem)), 0, st_num_elem, ST_type(st_num_elem));
	//WN* InitIsPow2 = WN_Binary(OPR_SUB, TY_mtype(ST_type(st_num_elem)), WN_COPY_Tree(wn_num_elem), 
	//							WN_Intconst(TY_mtype(ST_type(st_num_elem)), 1));
	
	//InitIsPow2 = WN_Binary(OPR_BAND, TY_mtype(ST_type(st_IsPow2)), 
	//							WN_COPY_Tree(wn_num_elem), InitIsPow2);
	//InitIsPow2 = WN_Relational (OPR_EQ, TY_mtype(ST_type(st_IsPow2)), 
	//					InitIsPow2, WN_Intconst(TY_mtype(ST_type(st_IsPow2)),0));
	//InitIsPow2 = WN_Stid(TY_mtype(ST_type(st_IsPow2)), 0, st_IsPow2, 
	//								ST_type(st_IsPow2), InitIsPow2);	
	//WN_INSERT_BlockLast( acc_reduction_func,  InitIsPow2);

	
	WN* Init0 = WN_Stid(TY_mtype(ST_type(st_tid)), 0, st_tid, ST_type(st_tid), WN_COPY_Tree(wn_threadidx));
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	WN* wn_blocksize = WN_Ldid(TY_mtype(ST_type(st_blocksize)), 0, st_blocksize, ST_type(st_blocksize));
	Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockIdx_x)), WN_COPY_Tree(wn_blockidx), WN_COPY_Tree(wn_blocksize));
	Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockIdx_x)), Init0,  WN_Intconst(MTYPE_U4, 2));
	Init0 = WN_Binary(OPR_ADD, TY_mtype(ST_type(glbl_blockIdx_x)), Init0,  WN_COPY_Tree(wn_threadidx));
	Init0 = WN_Stid(TY_mtype(ST_type(st_loop_index)), 0, st_loop_index, ST_type(st_loop_index), Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);

	
	//WN* wn_gridSizes = WN_Ldid(TY_mtype(ST_type(st_blocksize)), 0, st_blocksize, ST_type(st_blocksize));
	Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockDim_x)), WN_COPY_Tree(wn_griddimx), WN_COPY_Tree(wn_blocksize));
	Init0 = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_blockDim_x)), Init0,  WN_Intconst(MTYPE_U4, 2));
	Init0 = WN_Stid(TY_mtype(ST_type(st_gridSize)), 0, st_gridSize, ST_type(st_gridSize), Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);

	Init0 = ACC_Get_Init_Value_of_Reduction(ReductionOpr, TY_mtype(ty));
	Init0 = WN_Stid(TY_mtype(ST_type(st_mySum)), 0, st_mySum, ST_type(st_mySum), Init0);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);

	//Begin calculation
	WN* wn_loadWhileBlock = WN_CreateBlock();

	WN* wn_whileTest = WN_Relational (OPR_LT, TY_mtype(ST_type(st_loop_index)), 
								WN_COPY_Tree(wn_loop_index), 
								WN_COPY_Tree(wn_num_elem));
	//while body
	//WN* wn_IfTest1 = WN_Relational (OPR_EQ, TY_mtype(ST_type(st_IsPow2)), 
	//							WN_COPY_Tree(wn_IsPow2), 
	//							WN_Intconst(MTYPE_U4, 1));
	Init0 = WN_Binary(OPR_ADD, TY_mtype(ST_type(st_loop_index)), 
						WN_COPY_Tree(wn_loop_index), WN_COPY_Tree(wn_blocksize));
	WN* wn_IfTest2 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_loop_index)), 
						Init0, WN_COPY_Tree(wn_num_elem));
	WN* wn_IfCombinedTest = wn_IfTest2;//WN_Binary (OPR_CIOR, Boolean_type, wn_IfTest1, wn_IfTest2);
	//WN_Relational (OPR_CIOR, TY_mtype(ST_type(st_loop_index)), WN_COPY_Tree(wn_loop_index), WN_COPY_Tree(wn_num_elem));
	//WN* tree = WN_Istore(TY_mtype(TY_pointed(ST_type(w->new_st))), 0, ST_type(w->new_st), wn_ptrLoc, WN_kid(tree, 0));
	//WN* tree = WN_Iload(TY_mtype(TY_pointed(ST_type(w->new_st))), 0,  TY_pointed(ST_type(w->new_st)), wn_ptrLoc);
	WN* wn_ifThenBody = WN_CreateBlock();
	//mySum += g_idata[i];
	Init0 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_loop_index), st_input_data);
	Init0 = WN_Iload(TY_mtype(TY_pointed(ST_type(st_input_data))), 0,  
									TY_pointed(ST_type(st_input_data)), Init0);
	Init0 = ACC_GEN_Reduction_Binary_Op(ReductionOpr, TY_mtype(ST_type(st_mySum)), WN_COPY_Tree(wn_mySum), Init0);
	Init0 = WN_Stid(TY_mtype(ST_type(st_mySum)), 0, st_mySum, ST_type(st_mySum), Init0);
	WN_INSERT_BlockLast( wn_loadWhileBlock,  Init0);
	//mySum += g_idata[i+blockSize]; 
	Init0 = WN_Binary(OPR_ADD, TY_mtype(ST_type(st_loop_index)), 
					WN_COPY_Tree(wn_loop_index), WN_COPY_Tree(wn_blocksize));
	Init0 = ACC_LoadDeviceSharedArrayElem(Init0, st_input_data);
	Init0 = WN_Iload(TY_mtype(TY_pointed(ST_type(st_input_data))), 0,  
									TY_pointed(ST_type(st_input_data)), Init0);
	Init0 = ACC_GEN_Reduction_Binary_Op(ReductionOpr, TY_mtype(ST_type(st_mySum)), WN_COPY_Tree(wn_mySum), Init0);
	Init0 = WN_Stid(TY_mtype(ST_type(st_mySum)), 0, st_mySum, ST_type(st_mySum), Init0);
	WN_INSERT_BlockLast( wn_ifThenBody,  Init0);
	WN* wn_ifThenElse = WN_CreateIf(wn_IfCombinedTest, wn_ifThenBody, WN_CreateBlock());
	WN_INSERT_BlockLast( wn_loadWhileBlock,  wn_ifThenElse);
	//i += gridSize;
	Init0 = WN_Binary(OPR_ADD, TY_mtype(ST_type(st_loop_index)), 
					WN_COPY_Tree(wn_loop_index), WN_COPY_Tree(wn_gridSize));
	Init0 = WN_Stid(TY_mtype(ST_type(st_loop_index)), 0, st_loop_index, ST_type(st_loop_index), Init0);
	WN_INSERT_BlockLast( wn_loadWhileBlock,  Init0);
    WN* wn_WhileDO = WN_CreateWhileDo(wn_whileTest, wn_loadWhileBlock);
	WN_INSERT_BlockLast( acc_reduction_func,  wn_WhileDO);

	//sdata[tid] = mySum;
	Init0 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_tid), st_shared_array);
	Init0 = WN_Istore(TY_mtype(TY_pointed(ST_type(st_shared_array))), 0, 
					Make_Pointer_Type(ty), Init0, WN_COPY_Tree(wn_mySum));
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	//__syncthreads();
	WN* wn_callSyncThreads = Gen_Sync_Threads();
	WN_INSERT_BlockLast( acc_reduction_func,  WN_COPY_Tree(wn_callSyncThreads));

	Init0 = Gen_ReductionIfElseBlock1(wn_blocksize, wn_tid, st_shared_array, wn_mySum, 512, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_blocksize, wn_tid, st_shared_array, wn_mySum, 256, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_blocksize, wn_tid, st_shared_array, wn_mySum, 128, ReductionOpr);
	WN_INSERT_BlockLast( acc_reduction_func,  Init0);

	
	wn_IfTest2 = WN_Relational (OPR_LT, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_tid), 
								WN_Intconst(MTYPE_U4, 32));
	WN* wn_IfBody2 = WN_CreateBlock();
	
    localname = (char *) alloca(strlen(ST_name(old_st))+10);
	sprintf ( localname, "__smem_%s", ST_name(old_st));
	Set_TY_is_volatile(ty);
	TY_IDX ty_pointer = Make_Pointer_Type(ty);	
  	if(acc_target_arch == ACC_ARCH_TYPE_APU)						
  		Set_TY_is_shared_mem(ty_pointer);
	ST* st_smem_pointer = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_smem_pointer,
	  Save_Str(localname),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty_pointer);	  
  	if(acc_target_arch == ACC_ARCH_TYPE_APU)
		Set_ST_ACC_shared_array(st_smem_pointer);
	
    Init0 = WN_Ldid(Pointer_type, 0, st_shared_array, ST_type(st_shared_array));
	Init0 = WN_Stid(TY_mtype(ST_type(st_smem_pointer)), 0, 
					st_smem_pointer, ST_type(st_smem_pointer), Init0);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_blocksize, wn_tid, st_smem_pointer, wn_mySum, 64, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_blocksize, wn_tid, st_smem_pointer, wn_mySum, 32, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_blocksize, wn_tid, st_smem_pointer, wn_mySum, 16, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_blocksize, wn_tid, st_smem_pointer, wn_mySum, 8, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_blocksize, wn_tid, st_smem_pointer, wn_mySum, 4, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	Init0 = Gen_ReductionIfElseBlock1(wn_blocksize, wn_tid, st_smem_pointer, wn_mySum, 2, ReductionOpr);
	WN_INSERT_BlockLast( wn_IfBody2,  Init0);
	WN* wn_ifThenElse2 = WN_CreateIf(wn_IfTest2, wn_IfBody2, WN_CreateBlock());
	WN_INSERT_BlockLast( acc_reduction_func,  wn_ifThenElse2);

	
	WN* wn_IfTest3 = WN_Relational (OPR_EQ, TY_mtype(ST_type(st_tid)), 
								WN_COPY_Tree(wn_tid), 
								WN_Intconst(MTYPE_U4, 0));
	WN* wn_IfBody3 = WN_CreateBlock();
	
	//Init0 = WN_Binary(OPR_ADD, TY_mtype(ST_type(WN_st(wn_tid))), 
	//				WN_COPY_Tree(wn_tid), WN_Intconst(MTYPE_U4, ilimit/2));
	Init0 = ACC_LoadDeviceSharedArrayElem(WN_Intconst(MTYPE_U4, 0), st_shared_array);	
	Init0 = WN_Iload(TY_mtype(TY_pointed(ST_type(st_shared_array))), 0,  
									TY_pointed(ST_type(st_shared_array)), Init0);
	
	WN* Init1 = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(wn_blockidx), st_output_data);	
	Init1 = WN_Istore(TY_mtype(TY_pointed(ST_type(st_output_data))), 0, 
					ST_type(st_output_data), Init1, Init0);
	WN_INSERT_BlockLast( wn_IfBody3,  Init1);
	WN* wn_ifThenElse3 = WN_CreateIf(wn_IfTest3, wn_IfBody3, WN_CreateBlock());
	WN_INSERT_BlockLast( acc_reduction_func,  wn_ifThenElse3);//*/

	
	////////////////////////////////////////////////////////////////
	//restore info
	//ACC_Transfer_Maps ( acc_pmaptab, acc_cmaptab, acc_reduction_func, 
	//	  PU_Info_regions_ptr(Current_PU_Info) );

	/* Create a new dependence graph for the child  and move all the 
	 appropriate vertices from the parent to the child graph */

	//Current_Map_Tab = acc_cmaptab;
	//ACC_Fix_Dependence_Graph ( acc_ppuinfo, Current_PU_Info, acc_reduction_func); 
	//Current_Map_Tab = acc_pmaptab;

  
	/* Restore parent information. */

	CURRENT_SYMTAB = acc_psymtab;
	Current_PU_Info = acc_ppuinfo;
	Current_pu = &Current_PU_Info_pu();
	Current_Map_Tab = acc_pmaptab;
	ACC_Pop_Some_Globals( );

	return acc_reduction_proc;
}

/*the reduction has to appear in parallel construct
It is only performed in global memory*/
void ACC_Reduction4ParallelConstruct(ParallelRegionInfo* pPRInfo)
{
	INT32 RDIdx = 0;
	while(RDIdx < pPRInfo->reductionmap.size())
	{
		//WN* reduction_Idx1;
		//WN* reduction_Idx2;
		WN* wn_If_stmt_test = NULL;
		WN* wn_local_If_stmt_test = NULL;
		ACC_ReductionMap reductionMap = pPRInfo->reductionmap[RDIdx];
		ST* st_reduction_private_var = reductionMap.st_private_var;
		reductionMap.wn_assignment2localArray = NULL;
		TY_IDX ty_red = ST_type(reductionMap.hostName);
		TYPE_ID typeID = TY_mtype(ty_red);
		
		//global memory
		reductionMap.wn_IndexOpr = blockidx;
		//
		WN* test1 = WN_Relational (OPR_EQ, TY_mtype(ST_type(glbl_threadIdx_x)), 
					WN_COPY_Tree(threadidx), 
					WN_Intconst(TY_mtype(ST_type(glbl_threadIdx_x)), 0));
		WN* test2 = WN_Relational (OPR_EQ, TY_mtype(ST_type(glbl_threadIdx_y)), 
					WN_COPY_Tree(threadidy), 
					WN_Intconst(TY_mtype(ST_type(glbl_threadIdx_y)), 0));
		wn_If_stmt_test = WN_Binary (OPR_CAND, Boolean_type, test1, test2);
		
		//if reduction is ADD, init value is 0;
		//private scalar variable init
		WN* wn_reduction_init_scalar;
		WN* wn_reduction_init_garray;
		WN* wn_array_loc, *wn_stmt;
		int initvalue = 0;
		//TY_IDX ty_red = ST_type(reductionMap.hostName);
		wn_reduction_init_scalar = ACC_Get_Init_Value_of_Reduction(reductionMap.ReductionOpr, TY_mtype(ty_red));
		wn_reduction_init_scalar = WN_Stid(TY_mtype(ST_type(reductionMap.st_private_var)), 0, 
					reductionMap.st_private_var, ST_type(reductionMap.st_private_var), wn_reduction_init_scalar); 
		reductionMap.wn_initialAssign = wn_reduction_init_scalar;
			//WN_INSERT_BlockLast( wn_replace_block,  wn_reduction_init);	
		//////////////////////////////////////////////////////////////////////////////////
		//global reduction array
		wn_array_loc = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(reductionMap.wn_IndexOpr),
								reductionMap.st_Inkernel);
		wn_reduction_init_garray = WN_Istore(TY_mtype(ty_red), 0, 
							Make_Pointer_Type(ty_red), wn_array_loc, 
							WN_COPY_Tree(reductionMap.wn_private_var));
		
		
		
		wn_stmt = WN_CreateBlock();
		WN_INSERT_BlockLast(wn_stmt,  wn_reduction_init_garray);
		reductionMap.wn_assignment2Array = WN_CreateIf(wn_If_stmt_test, wn_stmt, WN_CreateBlock());
		
		/////////////////////////////////////////////////////////////////
		pPRInfo->reductionmap[RDIdx] = reductionMap;
		/////////////////////////////////////////////////////////////////	
		RDIdx ++;
	}
}


ACC_ReductionMap* ACC_Get_ReductionMap(WN* wn)
{
	ACC_ReductionMap* pRdcMap = Get_WN_ReductionMap(wn);
	return pRdcMap;
}

static ACC_LOOP_TYPE ACC_Locate_Reduction(WN* tree, ST* st_rd, OPERATOR oprRD, ACC_LOOP_TYPE curlooptype)
{	
  OPCODE op;
  OPERATOR opr;
  INT32 i;
  WN *r;
  WN *temp;
  ST *old_sym;
  WN_OFFSET old_offset;
  ACC_VAR_TABLE *w;
  ACC_LOOP_TYPE locate_loop_type;
  BOOL bFound = FALSE;

  /* Ignore NULL subtrees. */

  if (tree == NULL)
    return ACC_NONE_SPECIFIED;

  /* Initialization. */

  op = WN_opcode(tree);
  opr = OPCODE_operator(op);

  /* Look for and replace any nodes referencing localized symbols */
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
	ACC_LOOP_TYPE looptype;

  	while ((cur_node = next_node)) 
  	{
    	next_node = WN_next(cur_node);

    	if (((WN_opcode(cur_node) == OPC_PRAGMA) ||
         (WN_opcode(cur_node) == OPC_XPRAGMA)) &&
        (WN_pragmas[WN_pragma(cur_node)].users & PUSER_ACC)) 
    	{
			switch (WN_pragma(cur_node)) 
			{

			  case WN_PRAGMA_ACC_CLAUSE_GANG_X:
				if(looptype == ACC_VECTOR)
					looptype = ACC_GANG_VECTOR;
				
				else if(looptype == ACC_WORKER)				
					looptype = ACC_GANG_WORKER;
				
				else if(looptype == ACC_WORKER_VECTOR)				
					looptype = ACC_GANG_WORKER_VECTOR;
				
				else
					looptype = ACC_GANG;
			    break;  
				
			  case WN_PRAGMA_ACC_CLAUSE_VECTOR_Y:	
				if(looptype == ACC_VECTOR)				
					looptype = ACC_WORKER_VECTOR;
				else if(looptype == ACC_GANG)				
					looptype = ACC_GANG_WORKER;
				else if(looptype == ACC_GANG_VECTOR)				
					looptype = ACC_GANG_WORKER_VECTOR;
				else
					looptype = ACC_WORKER;
				break;

			  case WN_PRAGMA_ACC_CLAUSE_VECTOR_X:				
				if(looptype == ACC_GANG)
					looptype = ACC_GANG_VECTOR;
				else if(looptype == ACC_GANG_WORKER)				
					looptype = ACC_GANG_WORKER_VECTOR;
				else if(looptype == ACC_WORKER)				
					looptype = ACC_WORKER_VECTOR;
				else
					looptype = ACC_VECTOR;
			    break;  
				
			  default:
		        break;
			}
  		} 
 	 }  
	 locate_loop_type = ACC_Locate_Reduction(wn_region_bdy, st_rd, oprRD, looptype);
	 return locate_loop_type;
  }  
  else if (opr == OPR_STID)
  {
    old_sym = WN_st(tree);
	WN* Kid0 = WN_kid0(tree);
	if(WN_kid_count(Kid0) == 1 && WN_operator(Kid0) == OPR_CVT)
	{
		while(WN_operator(Kid0) == OPR_CVT)
			Kid0 = WN_kid0(Kid0);
	}
	if(WN_kid_count(Kid0) == 2)
	{	
		WN* wn_tmp0 = WN_kid0(Kid0);
		WN* wn_tmp1 = WN_kid1(Kid0);
		while(WN_operator(wn_tmp0) == OPR_CVT)
			wn_tmp0 = WN_kid0(wn_tmp0);
		while(WN_operator(wn_tmp1) == OPR_CVT)
			wn_tmp1 = WN_kid0(wn_tmp1);
		
		ST* st_Kid0 = WN_has_sym(wn_tmp0)? WN_st(wn_tmp0) : NULL;
		ST* st_Kid1 = WN_has_sym(wn_tmp1)? WN_st(wn_tmp1) : NULL;
		OPCODE opc_kid0 = WN_opcode(Kid0);
		OPERATOR opr_kid0 = OPCODE_operator(opc_kid0);
		if(st_rd == old_sym && opr_kid0 == oprRD && ((st_Kid0==st_rd)|| (st_Kid1 == st_rd)))
			return curlooptype;
	}
  } 
  /* Walk all children */
  if (op == OPC_BLOCK) 
  {
    r = WN_first(tree);
    while (r) 
	{ // localize each node in block
      locate_loop_type = ACC_Locate_Reduction ( r, st_rd, oprRD, curlooptype);
	  if(locate_loop_type != ACC_NONE_SPECIFIED)
		return locate_loop_type;
      r = WN_next(r);
      
   }
  }
  else 
  {
    for (i=0; i < WN_kid_count(tree); i++)
    {
      locate_loop_type = ACC_Locate_Reduction ( WN_kid(tree, i), st_rd, oprRD, curlooptype);
	  //if we find it, then return immediate
	  if(locate_loop_type != ACC_NONE_SPECIFIED)
		return locate_loop_type;
    }
  }
  return ACC_NONE_SPECIFIED;
}

/**************************************************************************************************************************
//Generating Reduction related information, 
//this function should be called before CUDA/OpenCL kernel generation (CreateMicroTask)
//update three values
*for the reduction at the TOP level loop
//DeviceName: allocate a buffer to hold value for each thread. The allocation is done by host.
//st_num_of_element: number of elements after local reduction
//reduction_kenels: additional kernel is going to be launched to finalize the reduction
//local reduction device function
*for other level of reduction
//update the local reduction functions
**************************************************************************************************************************/
static void ACC_ProcessReduction_Parallel_Outside_OffloadRegion(WN* wn_replace_block, ACC_ReductionMap* pReductionmap)
{
	/////////////////////////////////////////////////////////////////////////
	//process top level
	if(pReductionmap->looptype == ACC_GANG ||
		pReductionmap->looptype == ACC_GANG_WORKER ||
		pReductionmap->looptype == ACC_GANG_WORKER_VECTOR)
	{
		ST *old_st = pReductionmap->hostName;	
		ST* st_num_gangs;
		PREG_NUM rreg1, rreg2;	/* Pregs with I4 return values */;//each gang carries one buffer unit
		WN_INSERT_BlockLast( wn_replace_block, ACC_GenGetTotalNumGangs());
		ACC_Host_Create_Preg_or_Temp(MTYPE_I4, "_total_num_of_gangs", &st_num_gangs);
		/////////////////////////////////////////////////////////////////////////////////////////////
		ACC_GET_RETURN_PREGS(rreg1, rreg2, MTYPE_I4);
		WN* wn_return = WN_Ldid(TY_mtype(ST_type(st_num_gangs)), rreg1, Return_Val_Preg, ST_type(st_num_gangs));
		WN* temp_node = WN_Stid(TY_mtype(ST_type(st_num_gangs)), 0, 
										st_num_gangs, ST_type(st_num_gangs), wn_return);
		WN_INSERT_BlockLast( wn_replace_block, temp_node);

		/////////////////////////////////////////////////////////////
		WN* vector_num = WN_Ldid(TY_mtype(ST_type(st_num_gangs)), 
					0, st_num_gangs, ST_type(st_num_gangs));

		///////////////////////////////////////////////////////////////////////
		WN* alloc_size = WN_Binary(OPR_MPY, MTYPE_I4, WN_COPY_Tree(vector_num),
								WN_Intconst(MTYPE_I4, TY_size(ST_type(pReductionmap->hostName))));
		//malloc device addr
					
		//reductionmap	
		//
		TY_IDX ty = ST_type(old_st);
		TY_KIND kind = TY_kind(ty);//ST_name(old_st)
		char* localname = (char *) alloca(strlen(ST_name(old_st))+128);
		sprintf ( localname, "__device_reduction_%s_%d", ST_name(old_st), acc_reg_tmp_count);
		acc_reg_tmp_count ++;
		TY_IDX ty_p = Make_Pointer_Type(ty);
		ST *st_device = NULL;
		//WN *device_addr = NULL;
		st_device = New_ST( CURRENT_SYMTAB );
		ST_Init(st_device,
				Save_Str( localname ),
				CLASS_VAR,
				SCLASS_AUTO,
				EXPORT_LOCAL,
				ty_p);		
		pReductionmap->deviceName = st_device;
		pReductionmap->st_num_of_element = st_num_gangs;
		//call the acc malloc
		//WN* wnx = WN_Ldid(Pointer_type, 0, st_device, ST_type(st_device));
		//WN* wn_testif = WN_Relational (OPR_EQ, WN_rtype(wnx), 
		//					wnx, WN_Intconst(WN_rtype(wnx), 0));
		//WN* wn_thenblock = WN_CreateBlock();		
		//WN* wn_ifstmt = WN_CreateIf(wn_testif, wn_thenblock, WN_CreateBlock());
		WN_INSERT_BlockLast( wn_replace_block, GenReductionMalloc(st_device, alloc_size));
		WN* wn_pending_new_ptr = Gen_ACC_Pending_DevicePtr_To_Current_Stack(st_device, TRUE);
		WN_INSERT_BlockLast(wn_replace_block, wn_pending_new_ptr);
			
		//WN_INSERT_BlockLast( wn_replace_block, wn_ifstmt);				

		pReductionmap->reduction_kenels = ACC_GenerateReduction_Kernels_TopLoop(pReductionmap);
		acc_reduction_tab_map_new[old_st] = pReductionmap;
	}

	if(pReductionmap->looptype == ACC_VECTOR
		 && pReductionmap->acc_stmt_location_new == ACC_VECTOR)
	{
		pReductionmap->local_reduction_fun = ACC_GenerateVectorReduction_unrolling(pReductionmap);
	}
	else if(pReductionmap->acc_stmt_location_new == ACC_VECTOR 
			|| pReductionmap->acc_stmt_location_new == ACC_WORKER_VECTOR
			|| pReductionmap->acc_stmt_location_new == ACC_GANG_WORKER_VECTOR)
	{
		//besides looptype of ACC_VECTOR, ACC_GANG/ACC_WORKER/ACC_GANG_WORKER all require work-vector reduction
		pReductionmap->local_reduction_fun = ACC_GenerateWorkerVectorReduction_unrolling(pReductionmap);
	}
	else if(pReductionmap->acc_stmt_location_new == ACC_WORKER )//count workers
	{
		pReductionmap->local_reduction_fun = ACC_GenerateWorkerReduction_unrolling(pReductionmap);
	}
	else //count gangs
	{
		//no necessary
		pReductionmap->local_reduction_fun = NULL;
	}	

}


static UINT32 acc_nested_loop_depth_for_rdc = 0;

/***********************************************************************************************************
Put the reduction of top level loops in the acc_top_level_loop_rdc
************************************************************************************************************/
static void ACC_Parallel_Loop_Reduction_Extract (WN * tree, WN* wn_replace_block)
{
  OPCODE op;
  OPERATOR opr;
  INT32 i;
  WN *r;
  WN *temp;
  ACC_LOOP_TYPE looptype;
  WN* wn_gang = NULL;
  WN* wn_vector = NULL;
  BOOL hasLoopInside = FALSE;

  /* Ignore NULL subtrees. */

  if (tree == NULL)
    return ;

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
	FOR_LOOP_RDC_INFO for_loop_rdc_info;
	acc_nested_loop_depth_for_rdc ++;
	UINT32 acc_reduction_count = 0;
	//determine the loop types
  	while ((cur_node = next_node)) 
  	{
    	next_node = WN_next(cur_node);

    	if (((WN_opcode(cur_node) == OPC_PRAGMA) ||
         (WN_opcode(cur_node) == OPC_XPRAGMA)) &&
        (WN_pragmas[WN_pragma(cur_node)].users & PUSER_ACC)) 
    	{
			switch (WN_pragma(cur_node)) 
			{

			  case WN_PRAGMA_ACC_CLAUSE_GANG_X:
			  	if(acc_nested_loop_depth_for_rdc > 1)
					Fail_FmtAssertion ("Gang Clause can only appear in the top level loop.");
				if(looptype == ACC_VECTOR)
					looptype = ACC_GANG_VECTOR;
				
				else if(looptype == ACC_WORKER)				
					looptype = ACC_GANG_WORKER;
				
				else if(looptype == ACC_WORKER_VECTOR)				
					looptype = ACC_GANG_WORKER_VECTOR;
				
				else
					looptype = ACC_GANG;
			    break;  
				
			  case WN_PRAGMA_ACC_CLAUSE_VECTOR_Y:	
				if(looptype == ACC_VECTOR)				
					looptype = ACC_WORKER_VECTOR;
				else if(looptype == ACC_GANG)				
					looptype = ACC_GANG_WORKER;
				else if(looptype == ACC_GANG_VECTOR)				
					looptype = ACC_GANG_WORKER_VECTOR;
				else
					looptype = ACC_WORKER;
				break;

			  case WN_PRAGMA_ACC_CLAUSE_VECTOR_X:				
				if(looptype == ACC_GANG)
					looptype = ACC_GANG_VECTOR;
				else if(looptype == ACC_GANG_WORKER)				
					looptype = ACC_GANG_WORKER_VECTOR;
				else if(looptype == ACC_WORKER)				
					looptype = ACC_WORKER_VECTOR;
				else
					looptype = ACC_VECTOR;
			    break;  
				
			  default:
		        break;
			}
  		} 
 	 }
	 //find out all the reduction clauses
	 cur_node = WN_first(pragma_block);
  	 next_node = WN_next(cur_node);
	 while ((cur_node = next_node)) 
  	 {
    	next_node = WN_next(cur_node);

    	if (((WN_opcode(cur_node) == OPC_PRAGMA) ||
         (WN_opcode(cur_node) == OPC_XPRAGMA)) &&
        (WN_pragmas[WN_pragma(cur_node)].users & PUSER_ACC)) 
    	{
			switch (WN_pragma(cur_node)) 
			{
			  	case WN_PRAGMA_ACC_CLAUSE_REDUCTION:
				{
					ACC_ReductionMap* pReductionMap = new ACC_ReductionMap;
					ST* st_host = WN_st(cur_node);
					pReductionMap->looptype = looptype;
					pReductionMap->hostName = st_host;
					pReductionMap->ReductionOpr = (OPERATOR)WN_pragma_arg2(cur_node);
					pReductionMap->acc_stmt_location_new = 
								ACC_Locate_Reduction(wn_region_bdy, pReductionMap->hostName, 
														pReductionMap->ReductionOpr, looptype);
					if(pReductionMap->acc_stmt_location_new == ACC_NONE_SPECIFIED)
						Fail_FmtAssertion ("Cannot Locate the Reduction Statement.");

					ACC_ProcessReduction_Parallel_Outside_OffloadRegion(wn_replace_block, pReductionMap);
					Set_WN_ReductionMap(cur_node, (void*)pReductionMap);
					//we only collect the top-level reductions which will be an additional kernel parameters
					if(acc_nested_loop_depth_for_rdc == 1)
						for_loop_rdc_info.reductionmap.push_back(pReductionMap);

					//Updated the offload scalar table
					ACC_SCALAR_VAR_INFO* pVarInfo = acc_offload_scalar_management_tab[st_host];
					pVarInfo->is_reduction = TRUE;
				}
			    break;
				
			  default:
		        break;
			}
  		} 
 	  }
	  //multi-loops may be included in a parallel region
	  if(acc_nested_loop_depth_for_rdc == 1)
	  {
	 	for_loop_rdc_info.looptype = looptype;
	 	acc_top_level_loop_rdc.push_back(for_loop_rdc_info);
	  }
	  //check the nested loop reduction clauses
	  ACC_Parallel_Loop_Reduction_Extract(wn_region_bdy, wn_replace_block);
	  acc_nested_loop_depth_for_rdc --;
  }  
  else if (op == OPC_BLOCK) 
  {
    r = WN_first(tree);
    while (r) 
	{ // localize each node in block
      ACC_Parallel_Loop_Reduction_Extract(r, wn_replace_block);
      r = WN_next(r);
      
   }
  }
  else 
  {
    for (i=0; i < WN_kid_count(tree); i++)
    {
      ACC_Parallel_Loop_Reduction_Extract(WN_kid(tree, i), wn_replace_block);
    }
  }
}

void ACC_Parallel_Loop_Reduction_Extract_Public (WN * tree, WN* wn_replace_block)
{
	acc_nested_loop_depth_for_rdc = 0;
	ACC_Parallel_Loop_Reduction_Extract(tree, wn_replace_block);
}

/***************************************************************
Local index generation for worker-level
return a local if-test-statement
****************************************************************/
static WN* ACC_Generate_Reduction_Index_for_Worker(ACC_ReductionMap* pReductionmap)
{
	//////////////////////////////////////////////////////////////////////////
	pReductionmap->wn_localIndexOpr = WN_COPY_Tree(threadidy);
	//wn_local_If_stmt
	WN* wn_local_If_stmt_test = WN_Relational (OPR_EQ, 
				TY_mtype(ST_type(glbl_threadIdx_x)), 
				WN_COPY_Tree(threadidx), 
				WN_Intconst(TY_mtype(ST_type(glbl_threadIdx_x)), 0));
	return wn_local_If_stmt_test;
}


/***************************************************************
Local index generation for worker-vector level
****************************************************************/
static void ACC_Generate_Reductin_Index_for_Vector(ACC_ReductionMap* pReductionmap)
{
	WN* wn_reduction_Idx = WN_Binary(OPR_MPY, 
					TY_mtype(ST_type(glbl_threadIdx_x)), 
					WN_COPY_Tree(threadidy), 
					WN_COPY_Tree(blockdimx));
	wn_reduction_Idx = WN_Binary(OPR_ADD, 
					TY_mtype(ST_type(glbl_threadIdx_x)), 
					wn_reduction_Idx, 
					WN_COPY_Tree(threadidx));
	//////////////////////////////////////////////////////////////////////////
	pReductionmap->wn_localIndexOpr = wn_reduction_Idx;
}

static void ACC_Save_Reduction_Backup_Value(ACC_ReductionMap* pReductionmap)
{	
	TY_IDX ty_red = ST_type(pReductionmap->hostName);
	TYPE_ID typeID = TY_mtype(ty_red);
	ST* st_backupValue = New_ST(CURRENT_SYMTAB); 
	ST_Init(st_backupValue,
	  Save_Str2("__private_backup_", ST_name(pReductionmap->hostName)),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  MTYPE_To_TY(TY_mtype(ty_red)));
	pReductionmap->st_backupValue = st_backupValue;
	pReductionmap->wn_backupValue = WN_Ldid(typeID, 
			0, st_backupValue, ST_type(st_backupValue));				
	pReductionmap->wn_backupStmt = WN_Stid(TY_mtype(ST_type(st_backupValue)), 0, 
				st_backupValue, ST_type(st_backupValue), pReductionmap->wn_private_var);
}

static void ACC_Init_Reduction_Var_Stmt(ACC_ReductionMap* pReductionmap)
{	
	TY_IDX ty_red = ST_type(pReductionmap->hostName);
	TYPE_ID typeID = TY_mtype(ty_red);
	WN* wn_reduction_init = ACC_Get_Init_Value_of_Reduction(pReductionmap->ReductionOpr, 
																TY_mtype(ty_red));
	wn_reduction_init = WN_Stid(TY_mtype(ST_type(pReductionmap->st_private_var)), 0, 
											pReductionmap->st_private_var, 
											ST_type(pReductionmap->st_private_var), 
											wn_reduction_init); 
	pReductionmap->wn_initialAssign = wn_reduction_init;	
}

static void ACC_Init_ShArray_From_Reduction_Var_Stmt(ACC_ReductionMap* pReductionmap,
								WN* wn_local_if_stmt_test)
{
	TY_IDX ty_red = ST_type(pReductionmap->hostName);
	TYPE_ID typeID = TY_mtype(ty_red);
	WN* wn_array_loc = ACC_LoadDeviceSharedArrayElem(
						WN_COPY_Tree(pReductionmap->wn_localIndexOpr),
						pReductionmap->st_local_array);
	WN* wn_reduction_init = WN_Istore(TY_mtype(ty_red), 0, 
						Make_Pointer_Type(ty_red), wn_array_loc,
						WN_COPY_Tree(pReductionmap->wn_private_var));

	//pReductionmap->wn_assignment2Array = wn_reduction_init;
	if(wn_local_if_stmt_test)
	{
		WN* wn_stmt = WN_CreateBlock();
		WN_INSERT_BlockLast(wn_stmt,  wn_reduction_init);
		pReductionmap->wn_assignment2localArray = 
						WN_CreateIf(wn_local_if_stmt_test, wn_stmt, WN_CreateBlock());
	}
	else
		pReductionmap->wn_assignment2localArray = wn_reduction_init;
}

void ACC_ProcessReduction_Inside_offloadRegion(WN* wn_replace_block, 
										ACC_ReductionMap* pReductionmap)
{
	WN* wn_If_stmt_test = NULL;
	WN* wn_local_If_stmt_test = NULL;
	ST* st_reduction_private_var = pReductionmap->st_private_var;
	pReductionmap->wn_assignment2localArray = NULL;
	TY_IDX ty_red = ST_type(pReductionmap->hostName);
	TYPE_ID typeID = TY_mtype(ty_red);
	//we only use the shared memory for local reduction operations
	//there is a double type of shared memory buffer
	//every other type, we use type cast convert from double to its type
	ST* local_RedArray = acc_shared_memory_for_reduction_block[typeID];
	if(!local_RedArray)
	{
		TY_IDX ty = Be_Type_Tbl(typeID);//Make_Array_Type(TY_mtype(ty), 1, 1024);
		TY_IDX ty_array = Make_Pointer_Type(MTYPE_To_TY(TY_mtype(ty)));
		ST* local_array_red = New_ST(CURRENT_SYMTAB); 
		ST_Init(local_array_red,
		  Save_Str2("__shdata_", ACC_Get_ScalarName_of_Reduction(typeID)),
		  CLASS_VAR,
		  SCLASS_AUTO,
		  EXPORT_LOCAL,
		  ty_array);
		//only the opencl kernel requires pointer address space identification
		if(acc_target_arch == ACC_ARCH_TYPE_APU)
			Set_ST_ACC_shared_array(local_array_red);

		WN* wn_base = WN_Ldid(Pointer_type, 0, acc_st_shared_memory, 
					ST_type(acc_st_shared_memory));
		WN* wn_typecast = WN_Stid(Pointer_type, 0, local_array_red, 
										ST_type(local_array_red), wn_base); 
		WN_INSERT_BlockLast( wn_replace_block,	wn_typecast);
		acc_shared_memory_for_reduction_block[typeID] = local_array_red;
		pReductionmap->st_local_array = local_array_red;
	}
	else
		pReductionmap->st_local_array = local_RedArray;
	/////////////////////////////////////////////////////////////////
	//different loop scheduling and different reduction locations will generate different statements
	if(pReductionmap->looptype == ACC_GANG 
		|| pReductionmap->looptype == ACC_GANG_WORKER
		|| pReductionmap->looptype == ACC_GANG_WORKER_VECTOR)
	{
		//setup the local reduction index
		if(pReductionmap->looptype == ACC_GANG )
		{
			if(pReductionmap->acc_stmt_location_new == ACC_GANG)
			{
				//no local reduction is required
				pReductionmap->wn_localIndexOpr = NULL;
			}
			else if(pReductionmap->acc_stmt_location_new == ACC_WORKER)
			{
				//wn_local_If_stmt
				wn_local_If_stmt_test = ACC_Generate_Reduction_Index_for_Worker(pReductionmap);
			}
			else //if(pReductionmap->acc_stmt_location == ACC_VECTOR || pReductionmap->acc_stmt_location == ACC_WORKER_VECTOR)
			{
				ACC_Generate_Reductin_Index_for_Vector(pReductionmap);
			}
		}
		else if(pReductionmap->looptype == ACC_GANG_WORKER)
		{	
			//only two level of work, not outside, it must be inside
			if(pReductionmap->acc_stmt_location_new == ACC_GANG_WORKER)
			{
				wn_local_If_stmt_test = ACC_Generate_Reduction_Index_for_Worker(pReductionmap);
			}
			else
			{
				ACC_Generate_Reductin_Index_for_Vector(pReductionmap);
			}
		}
		else // pReductionmap->looptype == ACC_GANG_WORKER_VECTOR
		{
			ACC_Generate_Reductin_Index_for_Vector(pReductionmap);
		}
		
		//it is used to store data which will be used to do final reduction by launching another kernel
		//after local reduction, each gang have one element left.
		pReductionmap->wn_IndexOpr = blockidx;

		//if reduction is ADD, init value is 0;
		//private scalar variable init
		WN* wn_reduction_init_scalar;
		WN* wn_reduction_init_garray;
		WN* wn_array_loc, *wn_stmt;
		int initvalue = 0;
		
		//Init reduction private data
		ACC_Init_Reduction_Var_Stmt(pReductionmap);
		//////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////
		//local reduction array
		//reductionMap.wn_localIndexOpr != NULL means reduction location is not in outter loop body
		//there is a local reduction operation which has to be finished before store to global memory
		//see if it needs perform local reduction
		if(pReductionmap->wn_localIndexOpr != NULL)
		{
			
			ACC_Init_ShArray_From_Reduction_Var_Stmt(pReductionmap, wn_local_If_stmt_test);
			//init global array with the data from share memory
			//the local reduction result is stored in the very first element
			wn_reduction_init_garray = ACC_LoadDeviceSharedArrayElem(
							WN_Intconst(TY_mtype(ST_type(glbl_threadIdx_x)), 0), 
							pReductionmap->st_local_array);
			
			wn_reduction_init_garray = WN_Iload(typeID, 0,  ty_red, wn_reduction_init_garray);
			wn_array_loc = ACC_LoadDeviceSharedArrayElem(
						WN_COPY_Tree(pReductionmap->wn_IndexOpr),
						pReductionmap->st_Inkernel);
			wn_reduction_init_garray = WN_Istore(TY_mtype(ty_red), 0, 
						Make_Pointer_Type(ty_red), wn_array_loc, 
						wn_reduction_init_garray);
			
		}
		else //in the outter loop body
		{
			wn_array_loc = ACC_LoadDeviceSharedArrayElem(WN_COPY_Tree(pReductionmap->wn_IndexOpr),
						pReductionmap->st_Inkernel);
			wn_reduction_init_garray = WN_Istore(TY_mtype(ty_red), 0, 
						Make_Pointer_Type(ty_red), wn_array_loc, 
						WN_COPY_Tree(pReductionmap->wn_private_var));
		}
		
		WN* test1 = WN_Relational (OPR_EQ, TY_mtype(ST_type(glbl_threadIdx_x)), 
					WN_COPY_Tree(threadidx), 
					WN_Intconst(TY_mtype(ST_type(glbl_threadIdx_x)), 0));
		WN* test2 = WN_Relational (OPR_EQ, TY_mtype(ST_type(glbl_threadIdx_y)), 
					WN_COPY_Tree(threadidy), 
					WN_Intconst(TY_mtype(ST_type(glbl_threadIdx_y)), 0));
		wn_If_stmt_test = WN_Binary (OPR_CAND, Boolean_type, test1, test2);
		wn_stmt = WN_CreateBlock();
		WN_INSERT_BlockLast(wn_stmt,  wn_reduction_init_garray);
		pReductionmap->wn_assignment2Array = WN_CreateIf(wn_If_stmt_test, 
											wn_stmt, WN_CreateBlock());
	}
	else if(pReductionmap->looptype == ACC_WORKER)
	{
		//definitely in 3-level nested loops
		//among workers, if reduction stmt is in inner body, the reduction will be in this gang/block
		//shared memory	
		ST* st_host = pReductionmap->hostName;
		if(acc_offload_scalar_management_tab.find(st_host) == acc_offload_scalar_management_tab.end())
			Fail_FmtAssertion("cannot find reduction var in acc_offload_scalar_management_tab.");
		ACC_SCALAR_VAR_INFO* pVarInfo = acc_offload_scalar_management_tab[st_host];
		pReductionmap->st_private_var = pVarInfo->st_device_in_klocal;
		ST* st_reduction_private_var = pReductionmap->st_private_var;
		//////////////////////////////////////////////////////////////////////////////////
		TY_IDX ty_red = ST_type(pReductionmap->hostName);
		TYPE_ID typeID = TY_mtype(ty_red);
		pReductionmap->wn_private_var = WN_Ldid(TY_mtype(ST_type(st_reduction_private_var)),
                                        0, st_reduction_private_var, ST_type(st_reduction_private_var));
		//////////////////////////////////////////////////////////////////////////////////
		ACC_Save_Reduction_Backup_Value(pReductionmap);
		///////////////////////////////////////////////////////////////////////////////////
		if(pReductionmap->acc_stmt_location_new == ACC_WORKER)
		{
			wn_local_If_stmt_test = ACC_Generate_Reduction_Index_for_Worker(pReductionmap);
		}
		else //if(reductionMap.acc_stmt_location == ACC_VECTOR)
		{
			ACC_Generate_Reductin_Index_for_Vector(pReductionmap);
		}
		//if reduction is ADD, init value is 0;
		WN* wn_reduction_init;
		WN* wn_array_loc, *wn_stmt;
		int initvalue = 0;
		//Init reduction private data
		ACC_Init_Reduction_Var_Stmt(pReductionmap);
		//////////////////////////////////////////////////////////////////////////////////
		ACC_Init_ShArray_From_Reduction_Var_Stmt(pReductionmap, wn_local_If_stmt_test);
		//////////////////////////////////////////////////////////////////////////////////
		//Since the reduction is in the worker level,
		//after the local reduction is done, only one value in 
		//the index 0 of shared memory array is available
		WN* wn_storeback2PrivateVar = ACC_LoadDeviceSharedArrayElem(
								WN_Intconst(TY_mtype(ST_type(glbl_threadIdx_x)), 0), 
								pReductionmap->st_local_array);	
		wn_storeback2PrivateVar = WN_Iload(typeID, 0,  ty_red, wn_storeback2PrivateVar);
		wn_storeback2PrivateVar = WN_Binary(pReductionmap->ReductionOpr, typeID, 
									wn_storeback2PrivateVar, pReductionmap->wn_backupValue);
		wn_storeback2PrivateVar = WN_Stid(typeID, 0, 
										pReductionmap->st_private_var, 
										ST_type(pReductionmap->st_private_var), 
										wn_storeback2PrivateVar);
		pReductionmap->wn_assignBack2PrivateVar = wn_storeback2PrivateVar;
	}
	else //if(pReductionmap->looptype == ACC_WORKER_VECTOR || pReductionmap->looptype == ACC_VECTOR)
	{
		//among workers, if reduction stmt is in inner body, the reduction will be in this gang/block
		//shared memory	
		ST* st_host = pReductionmap->hostName;
		if(acc_offload_scalar_management_tab.find(st_host) == acc_offload_scalar_management_tab.end())
			Fail_FmtAssertion("cannot find reduction var in acc_offload_scalar_management_tab.");
		ACC_SCALAR_VAR_INFO* pVarInfo = acc_offload_scalar_management_tab[st_host];
		pReductionmap->st_private_var = pVarInfo->st_device_in_klocal;
		ST* st_reduction_private_var = pReductionmap->st_private_var;
		//////////////////////////////////////////////////////////////////////////////////
		//acc_global_memory_for_reduction_block
		TY_IDX ty_red = ST_type(pReductionmap->hostName);
		TYPE_ID typeID = TY_mtype(ty_red);
		pReductionmap->wn_private_var = WN_Ldid(TY_mtype(ST_type(st_reduction_private_var)),
                                        0, st_reduction_private_var, ST_type(st_reduction_private_var));
		ACC_Save_Reduction_Backup_Value(pReductionmap);
		///////////////////////////////////////////////////////////////////////////////////	
		ACC_Generate_Reductin_Index_for_Vector(pReductionmap);
		//if reduction is ADD, init value is 0;
		WN* wn_reduction_init;
		WN* wn_array_loc, *wn_stmt1;
		int initvalue = 0;
		//TY_IDX ty_red = ST_type(reductionMap.hostName);
		//Init reduction private data
		ACC_Init_Reduction_Var_Stmt(pReductionmap);
		//////////////////////////////////////////////////////////////////////////////////
		ACC_Init_ShArray_From_Reduction_Var_Stmt(pReductionmap, NULL);
		//////////////////////////////////////////////////////////////////////////////////		
		WN* wn_storeBackIndex;
		if(pReductionmap->looptype == ACC_VECTOR)
			wn_storeBackIndex = WN_Binary(OPR_MPY, TY_mtype(ST_type(glbl_threadIdx_y)), 
								WN_COPY_Tree(threadidy), WN_COPY_Tree(blockdimx));
		else 
			wn_storeBackIndex = WN_Intconst(TY_mtype(ST_type(glbl_threadIdx_x)), 0);
		WN* wn_storeback2PrivateVar = ACC_LoadDeviceSharedArrayElem(wn_storeBackIndex, 
								pReductionmap->st_local_array);	
		wn_storeback2PrivateVar = WN_Iload(typeID, 0,  ty_red, wn_storeback2PrivateVar);
		wn_storeback2PrivateVar = WN_Binary(pReductionmap->ReductionOpr, typeID, 
									wn_storeback2PrivateVar, pReductionmap->wn_backupValue);
		wn_storeback2PrivateVar = WN_Stid(TY_mtype(ST_type(pReductionmap->st_private_var)), 0, 
										pReductionmap->st_private_var, 
										ST_type(pReductionmap->st_private_var), 
										wn_storeback2PrivateVar);
		pReductionmap->wn_assignBack2PrivateVar = wn_storeback2PrivateVar;
	}

}

