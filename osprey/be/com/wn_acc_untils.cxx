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

typedef INT64 OFFSET_64 ; 
static TY_IDX  acc_utils_types_new_TY(INT32 align);

static FLD_HANDLE
acc_utils_fld_util(const char* name_string, TY_IDX fld_ty,  OFFSET_64 offset)
{

  FLD_HANDLE fld;

  if (fld_ty == 0)
    return(fld);

  fld = New_FLD ();
  FLD_Init (fld, Save_Str(name_string), fld_ty, offset);
  Set_FLD_bofst(fld, 0);
  Set_FLD_bsize(fld, 0);

  return(fld);
}

static TY_IDX
acc_utils_types_mk_struct(INT64 size, INT32 align, FLD_HANDLE list, const char *name)
{
  TY_IDX ty_idx ;

  ty_idx = acc_utils_types_new_TY(align) ;
  TY& ty = Ty_Table[ty_idx];

  TY_Init (ty, size, KIND_STRUCT, MTYPE_M, Save_Str(name));

  Set_TY_fld(ty, list);
  return (ty_idx);

}

static TY_IDX 
acc_utils_types_new_TY(INT32 align) 
{
  TY_IDX idx;

  TY& ty = New_TY(idx);

  Set_TY_align(idx,align);

  return idx ;
}


//this is for AMD HSA Platform
// structure struct transfer_t { int nargs ; size_t* rsrvd1; size_t* rsrvd2 ; size_t* rsrvd3 ; }
static TY_IDX 
acc_utils_transfer_t_TY()
{
  TY_IDX ty ;
  FLD_HANDLE f1 ;
  FLD_HANDLE f2 ;
  FLD_HANDLE f3 ;
  FLD_HANDLE f4 ;
  UINT32 elem_alignment = Pointer_Size;

  TY_IDX tp = Make_Pointer_Type(Be_Type_Tbl(MTYPE_U4));
  TY_IDX int_ty = Be_Type_Tbl(MTYPE_I4); 
  
  f1 = acc_utils_fld_util("nargs", int_ty, 0);
  f2 = acc_utils_fld_util("rsrvd1",tp, elem_alignment);
  f3 = acc_utils_fld_util("rsrvd2",tp, elem_alignment*2);
  f4 = acc_utils_fld_util("rsrvd3",tp, elem_alignment*3);
  Set_FLD_last_field(f4);

  ty = acc_utils_types_mk_struct(elem_alignment * 4,
				elem_alignment,
				f1,
				"transfer_t");
  return ty ;
}



static TY_IDX
acc_utils_types_array_util(INT16 rank, TY_IDX ety_idx, INT32 align, INT64 arr_size,
							const char * name)
{
	TY_IDX  ty_arr_idx ;
	INT16 i ;

	ty_arr_idx = acc_utils_types_new_TY(align);
	TY &ty_arr = Ty_Table[ty_arr_idx];
	TY_Init (ty_arr, arr_size, KIND_ARRAY, MTYPE_UNKNOWN, Save_Str(name));

	Set_TY_etype(ty_arr, ety_idx);

	for (i = 0 ; i < rank ; i++) 
	{

		ARB_HANDLE arb = New_ARB();
		ARB_Init (arb, 1, 1, 1);

		if (i == 0) {
		   Set_ARB_first_dimen(arb);
		   Set_TY_arb (ty_arr, arb);
		}

		Set_ARB_dimension (arb, rank - i );
		if (i == rank - 1)
		   Set_ARB_last_dimen (arb);


		Set_ARB_const_lbnd (arb);
		Set_ARB_lbnd_val (arb, 0);

		Set_ARB_const_stride (arb);
		Set_ARB_stride_val (arb, 0);

		Set_ARB_const_ubnd (arb);
		Set_ARB_ubnd_val (arb, 0);

	}
   
  return (ty_arr_idx);
}
  
//
//create typedef struct lparm_t Launch_params_t;
//struct lparm_t { int ndim; size_t gdims[3]; size_t ldims[3]; Transfer_t transfer ; } ;

static UINT32 param_t_count = 0;

ST* acc_utils_mk_lparm_t_st(void)
{
	TY_IDX ty_elem, ty;
	TY_IDX ty_array_gdims, ty_array_ldims ;
	TY_IDX ty1, ty4;
	FLD_HANDLE f1 ;
	FLD_HANDLE f2 ;
	FLD_HANDLE f3 ;
	FLD_HANDLE f4 ;
	UINT32 isize = 0;

    ty1 = Be_Type_Tbl(MTYPE_I4); 
    ty_elem = Be_Type_Tbl(MTYPE_U4); 
    ty_array_gdims = acc_utils_types_array_util(1,
			    ty_elem,
			    TY_size(ty_elem),
			    TY_size(ty_elem)*3,
			    "gdims");
    ty_array_ldims = acc_utils_types_array_util(1,
			    ty_elem,
			    TY_size(ty_elem),
			    TY_size(ty_elem)*3,
			    "ldims");

	//TY& ta = Ty_Table[ta_idx];
	Set_TY_AR_lbnd_val (ty_array_gdims, 0, 2);
	Set_TY_AR_ubnd_val(ty_array_gdims, 0, 2);
	Set_TY_AR_stride_val(ty_array_gdims, 0, TY_size(ty_elem));

	Set_TY_AR_lbnd_val (ty_array_ldims, 0, 2);
	Set_TY_AR_ubnd_val(ty_array_ldims, 0, 2);
	Set_TY_AR_stride_val(ty_array_ldims, 0, TY_size(ty_elem));

	ty4 = acc_utils_transfer_t_TY();

	f1 = acc_utils_fld_util("ndim", ty1, 0);
	f2 = acc_utils_fld_util("gdims", ty_array_gdims, TY_size(ty_elem));
	f3 = acc_utils_fld_util("ldims", ty_array_gdims, TY_size(ty_elem)*4);
	f4 = acc_utils_fld_util("transfer",ty4,  ((TY_size(ty_elem)*7 + Pointer_Size - 1)%Pointer_Size) * Pointer_Size);
	Set_FLD_last_field(f4);

	//padding is necessary if it is 64bit machine
	//sizeof(ndim + gdims[3] + dims[3]) + padding + sizeof(transfer_t)
	isize = ((TY_size(ty_elem)*7 + Pointer_Size - 1)%Pointer_Size) * Pointer_Size + Pointer_Size * 4;
	
  	ty = acc_utils_types_mk_struct(isize, Pointer_Size, f1, "lparm_t");

	ST* st = New_ST(CURRENT_SYMTAB);
	char szname[256];
	sprintf(szname, "lparam_t_%d", param_t_count);
	param_t_count ++;
  	ST_Init(st, Save_Str(szname), CLASS_VAR, SCLASS_AUTO, EXPORT_LOCAL, ty);
  	return st ;		  
}


