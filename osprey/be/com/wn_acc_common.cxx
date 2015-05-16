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

INT64 acc_line_number;	/* Line number of acc parallel/kernel region */

//For forloop analysis
WN * acc_test_stmt;
vector<WN *> acc_base_node;		  /* Parallel forloop base */
vector<WN *> acc_limit_node;		/* Parallel forloop limit */
WN *acc_ntrip_node;		/* Parallel forloop trip count, iterations in this loop */
vector<WN *> acc_stride_node;		/* Parallel forloop stride */
WN *acc_doloop_body;
UINT32 acc_loopnest_depth;
vector<ST *> acc_forloop_index_st;		/* User forloop index variable ST */
vector<TYPE_ID> acc_forloop_index_type;	/* User forloop index variable type */
ACCP_process_type acc_t;
INT32 acc_reduction_count; 


//it is for fortran info
typedef struct dope_array_info
{
	WN* wn_original;
	WN* wn_replace;
	WN* wn_4_assignment;
}dope_array_info;

typedef struct dope_array_info_map
{
	//WN* wn_startaddr;
	map<INT32, dope_array_info> array_stmt_map;
}dope_array_info_map;

static map<ST*, dope_array_info_map> acc_dope_array;


int acc_get_lineno(WN *wn)
{ 
  USRCPOS srcpos;
  USRCPOS_srcpos(srcpos) = WN_Get_Linenum(wn);
  if (USRCPOS_srcpos(srcpos) != 0)
  {
    return USRCPOS_linenum(srcpos);
  }
  return 0; 
} 


/*  Create either a preg or a temp depending on presence of C++ exception
    handling.  */

void ACC_Device_Create_Preg_or_Temp ( TYPE_ID mtype, const char *name, ST **st)
{
	ST *new_st;
	char szname[256];
	sprintf(szname, "__accd_tmp_%s_%d", name, kernel_tmp_variable_count);
	kernel_tmp_variable_count++;
  
	new_st = New_ST (CURRENT_SYMTAB);
	ST_Init (new_st,
	         Save_Str (szname),
	         CLASS_VAR,
	         SCLASS_AUTO,
	         EXPORT_LOCAL,
	         MTYPE_To_TY (mtype));
	Set_ST_is_temp_var ( new_st );
	*st = new_st;
}

UINT32 acc_reg_tmp_count = 0; 

void ACC_Host_Create_Preg_or_Temp ( TYPE_ID mtype, const char *name, ST **st)
{
	ST *new_st;
	char szTmp[256];
	sprintf(szTmp, "__acch_tmp_%s_%d", name, acc_reg_tmp_count);
    acc_reg_tmp_count++;
	
	new_st = New_ST (CURRENT_SYMTAB);
	ST_Init (new_st,
	         Save_Str (szTmp),
	         CLASS_VAR,
	         SCLASS_AUTO,
	         EXPORT_LOCAL,
	         MTYPE_To_TY (mtype));
	Set_ST_is_temp_var ( new_st );
	*st = new_st;
}


/*
Hack for determining if an ST has an associated F90 dope vector (this is
true for F90 pointers, allocatable arrays, and arrays that may be
non-contiguous).
*/

/*
from dphillim:
so  if it has a dope TY and
    the dope_TY points to a KIND_ARRAY,
    and its not an f90 pointer 
    and not an argument (SCALAR_FORMAL_REF), 
    Then it's an allocatable because there's nothing left.
*/

//(PU_src_lang(Get_Current_PU()) == PU_F77_LANG || PU_src_lang(Get_Current_PU()) == PU_F90_LANG)
TY_IDX
F90_ST_Get_Dope_Vector_etype(ST *st)
{
	FLD_HANDLE  fli ;
	TY_IDX base_pointer_ty;
	fli = TY_fld(Ty_Table[ST_type(st)]);
	base_pointer_ty = FLD_type(fli);
	TY_IDX e_ty = TY_etype(TY_pointed(base_pointer_ty));
	return e_ty;
}


void
acc_my_Get_Return_Pregs(PREG_NUM *rreg1, PREG_NUM *rreg2, mTYPE_ID type,
                    const char *file, INT line)
{
  if (WHIRL_Return_Info_On) {
    RETURN_INFO return_info = Get_Return_Info(Be_Type_Tbl(type),
                                              Use_Simulated);
    if (RETURN_INFO_count(return_info) <= 2) {
      *rreg1 = RETURN_INFO_preg(return_info, 0);
      *rreg2 = RETURN_INFO_preg(return_info, 1);
    } else
      Fail_FmtAssertion("file %s, line %d: more than 2 return registers",
                        file, line);

  } else
    Get_Return_Pregs(type, MTYPE_UNKNOWN, rreg1, rreg2);

  FmtAssert(*rreg1 != 0 && *rreg2 == 0, ("bad return pregs"));
} // my_Get_Return_Pregs()


// Return a non-structure field from offset, if there's multiple field
// with the same offset, return the first.
// This routine can return empty fld handler.
static FLD_HANDLE 
ACC_Get_FLD_From_Offset_r(const TY_IDX ty_idx, const UINT64 offset, UINT* field_id)
{
  Is_True(Is_Structure_Type(ty_idx), ("need to be a structure type"));

  UINT64 cur_offset = 0;

  FLD_ITER fld_iter = Make_fld_iter(TY_fld(ty_idx));
  do {
    if ( field_id != NULL )
      (*field_id) ++;
    FLD_HANDLE fld(fld_iter);       

    // we assume that we will not see bit-fields here.
    cur_offset = FLD_ofst(fld);

    if (cur_offset == offset)
    {
      // check type
      TY_IDX cur_fld_idx = FLD_type(fld);
      if (!Is_Structure_Type(cur_fld_idx))
        return fld;
    }

    TY_IDX cur_fld_idx = FLD_type(fld);
    if (TY_kind(cur_fld_idx) == KIND_STRUCT &&
        TY_fld(cur_fld_idx) != FLD_HANDLE())
    {
      // it's possible that the new_offset becomes negative
      // because of unions. 
      INT64 new_offset = offset - cur_offset;
      if (new_offset < 0) 
	  	return FLD_HANDLE();
      FLD_HANDLE fld1 = ACC_Get_FLD_From_Offset_r(cur_fld_idx, new_offset, field_id);
      if (!fld1.Is_Null()) return fld1;
    }

  } while (!FLD_last_field(fld_iter++));

  return FLD_HANDLE();
}

// Return a non-structure field from offset, if there's multiple field
// with the same offset, return the first.
// This routine will assert if it cannot find a valid field.
static FLD_HANDLE 
ACC_Get_FLD_From_Offset(const TY_IDX ty_idx, const UINT64 offset, UINT *field_id= NULL)
{
  if (field_id != NULL)
	  *field_id= 0;
  FLD_HANDLE fld = ACC_Get_FLD_From_Offset_r(ty_idx, offset, field_id);
  FmtAssert(!fld.Is_Null(),("cannot find field from offset"));
  return fld;
}

static void
Gen_ACC_LS_get_fld_id_and_ty(ST *st, WN_OFFSET offset, BOOL scalar_only, UINT &field_id, TY_IDX &ty, TY_IDX &result_ty)
{
  ty = ST_type(st);
  result_ty = ty;
  if (scalar_only && TY_kind(ty) == KIND_STRUCT )
  {
    FLD_HANDLE fld = ACC_Get_FLD_From_Offset(ty, offset, &field_id);
    result_ty = FLD_type(fld);
  }
  if (scalar_only && TY_kind(ty) == KIND_ARRAY)
    ty = TY_etype(ty);
  return; 
}
/*  Generate an appropriate load WN based on an ST.  */

WN *
Gen_ACC_Load( ST * st, WN_OFFSET offset, BOOL scalar_only )
{
  UINT field_id = 0;
  WN *wn;
  TY_IDX ty;
  TY_IDX result_ty;
  
  Gen_ACC_LS_get_fld_id_and_ty(st, offset, scalar_only, field_id, ty, result_ty);

  wn = WN_Ldid ( TY_mtype(result_ty), offset, st, ty ,field_id);

  return (wn);
}

/*  Generate an appropriate store WN based on an ST.  */

WN *
Gen_ACC_Store( ST * st, WN_OFFSET offset, WN * value, BOOL scalar_only)
{
  UINT  field_id = 0;
  WN *wn;
  TY_IDX ty;
  TY_IDX result_ty;

  Gen_ACC_LS_get_fld_id_and_ty(st, offset, scalar_only, field_id, ty, result_ty);
  
  wn = WN_Stid ( TY_mtype(result_ty), offset, st, ty, value, field_id );
  WN_linenum(wn) = acc_line_number;

  return (wn);
}


/*inline WN_OFFSET WN_offsetx ( WN *wn )
{
  OPERATOR opr;
  opr = WN_operator(wn);
  if ((opr == OPR_PRAGMA) || (opr == OPR_XPRAGMA)) {
    return (WN_pragma_arg1(wn));
  } else {
    return (WN_offset(wn));
  }
}*/
	
	
	
/****************************************************/
/*this following  two functions will be used in */
void ACC_WN_set_offsetx ( WN *wn, WN_OFFSET ofst )
{
  OPERATOR opr;
  opr = WN_operator(wn);
  if ((opr == OPR_PRAGMA) || (opr == OPR_XPRAGMA)) {
	WN_pragma_arg1(wn) = ofst;
  } else {
	WN_offset(wn) = ofst;
  }
}



/*  Compare two PRAGMA nodes or XPRAGMA trees for equality.  */

BOOL ACC_Identical_Pragmas ( WN * wn1, WN * wn2 )
{
  INT32 i;

  if ((WN_operator(wn1) != WN_operator(wn2)) ||
      (WN_pragma(wn1) != WN_pragma(wn2)) ||
      (WN_st(wn1) != WN_st(wn2)) ||
      (WN_pragma_flags(wn1) != WN_pragma_flags(wn2)) ||
      ((WN_operator(wn1) == OPR_PRAGMA) &&
       (WN_pragma_arg1(wn1) != WN_pragma_arg1(wn2))) || 
       (WN_pragma_arg2(wn1) != WN_pragma_arg2(wn2)) ||
      (WN_kid_count(wn1) != WN_kid_count(wn2)))
    return (FALSE);

  for (i = 0; i < WN_kid_count(wn1); i++)
    if (WN_ACC_Compare_Trees(WN_kid(wn1, i), WN_kid(wn2, i)) != 0)
      return (FALSE);

  return (TRUE);
}

 WN_OFFSET ACC_WN_offsetx ( WN *wn )
{
  OPERATOR opr;
  opr = WN_operator(wn);
  if ((opr == OPR_PRAGMA) || (opr == OPR_XPRAGMA)) {
    return (WN_pragma_arg1(wn));
  } else {
    return (WN_offset(wn));
  }
}
 
 
 WN* ACC_WN_Integer_Cast(WN* tree, TYPE_ID to, TYPE_ID from)
 {
   if (from != to)
	 return WN_CreateExp1(OPCODE_make_op(OPR_CVT, to, from), tree);
   else
	 return tree;
 }
 
 /* standardize do comp operations.
  * required by RTL.
  * LT -> LE, GT -> LE
  * csc.
  * must be called before Extract_Do_Info
  */
 
 static void
 ACC_Standardize_ForLoop (WN* do_tree)
 {
   if (WN_operator(WN_end(do_tree)) == OPR_GE 
	   || WN_operator(WN_end(do_tree)) == OPR_LE )
   {
	 // need to do nothing.
	 return;
   }
   else
   {
	 WN_Upper_Bound_Standardize(do_tree, WN_end(do_tree), TRUE);
   }
 }
 
 /*
 * Extract do info for acc scheduling. 
 */
 
 static void 
 ACC_Extract_Per_Level_Do_Info ( WN * do_tree, UINT32 level_id )
 {
   // standardize do tree.
   acc_test_stmt = WN_COPY_Tree(WN_end(do_tree));
   ACC_Standardize_ForLoop(do_tree);
 
   WN		 *do_idname  = WN_index(do_tree);
   ST		 *do_id_st	 = WN_st(do_idname);
   WN_OFFSET  do_id_ofst = WN_offsetx(do_idname);
   WN		 *do_init;
   WN		 *do_limit;
   WN		 *do_stride;
   WN		 *doloop_body;
   BOOL 	 was_kid0 = FALSE;
 
   /* Extract acc scheduling info from do */
 
   do_init = WN_kid0(WN_start(do_tree));
   //WN_kid0(WN_start(do_tree)) = NULL;
 
#ifdef KEY
   {
	 // bug 5767: handle cvt
	 WN * kid0 = WN_kid0 (WN_end (do_tree));
	 if (WN_operator (kid0) == OPR_CVT)
	   kid0 = WN_kid0 (kid0);
 
	 WN * kid1 = WN_kid1 (WN_end (do_tree));
	 if (WN_operator (kid1) == OPR_CVT)
	   kid1 = WN_kid0 (kid1);
 
	 if (WN_operator (kid0) == OPR_LDID &&
		 WN_st (kid0) == do_id_st &&
		 WN_offsetx (kid0) == do_id_ofst)
	 { // kid0
	   was_kid0 = TRUE;
	   do_limit = WN_kid1 (WN_end (do_tree));
	   WN_kid1 (WN_end (do_tree)) = NULL;
	 }
	 else if (WN_operator (kid1) == OPR_LDID &&
			  WN_st (kid1) == do_id_st &&
			  WN_offsetx (kid1) == do_id_ofst)
	 { // kid1
	   do_limit = WN_kid0 (WN_end (do_tree));
	   WN_kid0 (WN_end (do_tree)) = NULL;
	 }
	 else
	 { // try again
	   WN_Upper_Bound_Standardize ( do_tree, WN_end(do_tree), TRUE );
	   // handle cvt
	   kid0 = WN_kid0 (WN_end (do_tree));
	   if (WN_operator (kid0) == OPR_CVT)
		 kid0 = WN_kid0 (kid0);
 
	   kid1 = WN_kid1 (WN_end (do_tree));
	   if (WN_operator (kid1) == OPR_CVT)
		 kid1 = WN_kid0 (kid1);
 
	   if (WN_operator (kid0) == OPR_LDID &&
		   WN_st (kid0) == do_id_st &&
		   WN_offsetx (kid0) == do_id_ofst)
	   { // kid0
		 was_kid0 = TRUE;
		 do_limit = WN_kid1 (WN_end (do_tree));
		 WN_kid1 (WN_end (do_tree)) = NULL;
	   }
	   else if (WN_operator (kid1) == OPR_LDID &&
				WN_st (kid1) == do_id_st &&
				WN_offsetx (kid1) == do_id_ofst)
	   { // kid1
		 do_limit = WN_kid0 (WN_end (do_tree));
		 WN_kid0 (WN_end (do_tree)) = NULL;
	   }
	   else // fail
		 Fail_FmtAssertion ( "malformed limit test in ACC forloop processing" );
	 }
   }
#else
   if ((WN_operator(WN_kid0(WN_end(do_tree))) == OPR_LDID) &&
	   (WN_st(WN_kid0(WN_end(do_tree))) == do_id_st) &&
	   (WN_offsetx(WN_kid0(WN_end(do_tree))) == do_id_ofst)) {
	 was_kid0 = TRUE;
	 do_limit = WN_kid1(WN_end(do_tree));
	 WN_kid1(WN_end(do_tree)) = NULL;
   } else if ((WN_operator(WN_kid1(WN_end(do_tree))) == OPR_LDID) &&
		  (WN_st(WN_kid1(WN_end(do_tree))) == do_id_st) &&
		  (WN_offsetx(WN_kid1(WN_end(do_tree))) == do_id_ofst)) {
	 do_limit = WN_kid0(WN_end(do_tree));
	 WN_kid0(WN_end(do_tree)) = NULL;
   } else {
	 WN_Upper_Bound_Standardize ( do_tree, WN_end(do_tree), TRUE );
	 if ((WN_operator(WN_kid0(WN_end(do_tree))) == OPR_LDID) &&
	 (WN_st(WN_kid0(WN_end(do_tree))) == do_id_st) &&
	 (WN_offsetx(WN_kid0(WN_end(do_tree))) == do_id_ofst)) {
	   was_kid0 = TRUE;
	   do_limit = WN_kid1(WN_end(do_tree));
	   WN_kid1(WN_end(do_tree)) = NULL;
	 } else if ((WN_operator(WN_kid1(WN_end(do_tree))) == OPR_LDID) &&
			(WN_st(WN_kid1(WN_end(do_tree))) == do_id_st) &&
			(WN_offsetx(WN_kid1(WN_end(do_tree))) == do_id_ofst)) {
	   do_limit = WN_kid0(WN_end(do_tree));
	   WN_kid0(WN_end(do_tree)) = NULL;
	 } else {
	   Fail_FmtAssertion ( "malformed limit test in ACC processing" );
	 }
   }
#endif
 
   if ((WN_operator(WN_kid0(WN_kid0(WN_step(do_tree)))) == OPR_LDID) &&
	   (WN_st(WN_kid0(WN_kid0(WN_step(do_tree)))) == do_id_st) &&
	   (WN_offsetx(WN_kid0(WN_kid0(WN_step(do_tree)))) == do_id_ofst))
   {
	 do_stride = WN_COPY_Tree ( WN_kid1(WN_kid0(WN_step(do_tree))) );
#ifdef KEY
	 if (WN_operator (WN_kid0 (WN_step (do_tree))) == OPR_SUB)
	 { // the loop goes down, don't miss '-' in (- non-const-stride)
	   OPCODE negop = OPCODE_make_op (OPR_NEG, WN_rtype (do_stride), MTYPE_V);
	   do_stride = WN_CreateExp1 (negop, do_stride);
	 }
#endif // KEY
   }
   else
	 do_stride = WN_COPY_Tree ( WN_kid0(WN_kid0(WN_step(do_tree))) );
 
   /* Generate mp scheduling expressions */
   doloop_body = WN_do_body(do_tree);
   acc_base_node.push_back(do_init);
	 // used by Rewrite_Do, need to be copied ?
   acc_limit_node.push_back(WN_COPY_Tree( do_limit ));
   acc_stride_node.push_back(do_stride);
   acc_doloop_body = WN_do_body(do_tree);
 
   TYPE_ID current_index_type = acc_forloop_index_type[level_id];
   if (acc_collapse_count == 1) {
	 if (((WN_operator(WN_end(do_tree)) == OPR_LT) && was_kid0) ||
		 ((WN_operator(WN_end(do_tree)) == OPR_GT) && !was_kid0)) { 
	   WN* wn_exp0 = WN_Sub(current_index_type, do_limit, WN_COPY_Tree(do_init));
	   wn_exp0 = ACC_WN_Integer_Cast(wn_exp0, current_index_type, WN_rtype(wn_exp0));
	   WN* wn_exp1 = WN_Add(current_index_type, wn_exp0, WN_COPY_Tree(do_stride));
	   wn_exp1 = ACC_WN_Integer_Cast(wn_exp1, current_index_type, WN_rtype(wn_exp1));
	   WN* wn_exp2 = WN_Sub(current_index_type, wn_exp1, WN_Intconst(current_index_type, 1));
	   wn_exp2 = ACC_WN_Integer_Cast(wn_exp2, current_index_type, WN_rtype(wn_exp2));
	   WN* wn_exp3 = WN_Div(current_index_type, wn_exp2, WN_COPY_Tree(do_stride));
	   acc_ntrip_node = wn_exp3; 
	 } else if (((WN_operator(WN_end(do_tree)) == OPR_GT) && was_kid0) ||
				((WN_operator(WN_end(do_tree)) == OPR_LT) && !was_kid0)) { 
	   WN* wn_exp0 = WN_Sub(current_index_type, do_limit, WN_COPY_Tree(do_init));
	   wn_exp0 = ACC_WN_Integer_Cast(wn_exp0, current_index_type, WN_rtype(wn_exp0));
	   WN* wn_exp1 = WN_Add(current_index_type, wn_exp0, WN_Intconst(current_index_type, 1));
	   wn_exp1 = ACC_WN_Integer_Cast(wn_exp1, current_index_type, WN_rtype(wn_exp1));
	   WN* wn_exp2 = WN_Add(current_index_type, wn_exp1, WN_COPY_Tree(do_stride));
	   wn_exp2 = ACC_WN_Integer_Cast(wn_exp2, current_index_type, WN_rtype(wn_exp2));
	   WN* wn_exp3 = WN_Div(current_index_type, wn_exp2, WN_COPY_Tree(do_stride));
	   acc_ntrip_node = wn_exp3; 
	 } else { 
	   WN* wn_exp0 = WN_Sub(current_index_type, do_limit, WN_COPY_Tree(do_init));
	   wn_exp0 = ACC_WN_Integer_Cast(wn_exp0, current_index_type, WN_rtype(wn_exp0));
	   WN* wn_exp1 = WN_Add(current_index_type, wn_exp0, WN_COPY_Tree(do_stride));
	   wn_exp1 = ACC_WN_Integer_Cast(wn_exp1, current_index_type, WN_rtype(wn_exp1));
	   WN* wn_exp2 = WN_Div(current_index_type, wn_exp1, WN_COPY_Tree(do_stride));
	   acc_ntrip_node = wn_exp2; 
	 } 
   }
 
 }
 
 /*
 * Extract do info for acc info (init, limit, step, etc). 
 */
 
 void 
 ACC_Extract_Do_Info ( WN * do_tree )
 {
   acc_base_node.clear();
   acc_limit_node.clear();
   acc_stride_node.clear();
   acc_doloop_body = NULL;
   for (UINT32 i = 0; i < acc_collapse_count; i++) {
	 ACC_Extract_Per_Level_Do_Info(do_tree, i);
	 do_tree = WN_first(WN_do_body(do_tree));
   }
 }

 /*extract indices information*/
 void
 ACC_Extract_Index_Info ( WN* pdo_node )
 {
   WN *prev_pdo = NULL;
   acc_forloop_index_st.clear();
   acc_forloop_index_type.clear();
   for (UINT32 i = 0; i < acc_collapse_count; i++) {
	 if (WN_operator(pdo_node) != OPR_DO_LOOP) {
	   /* in case collapse count exceeds number of do loops */
	   acc_collapse_count = i;
	   pdo_node = prev_pdo;
	   break;
	 }
	 ST * tmp_do_index_st = WN_st(WN_index(pdo_node));
	 TYPE_ID tmp_do_index_type = TY_mtype(ST_type(tmp_do_index_st));
	 if (tmp_do_index_type == MTYPE_I1 || tmp_do_index_type == MTYPE_I2)
	   tmp_do_index_type = MTYPE_I4;
	 else if (tmp_do_index_type == MTYPE_U1 || tmp_do_index_type == MTYPE_U2)
	   tmp_do_index_type = MTYPE_U4;
	 acc_forloop_index_st.push_back(tmp_do_index_st);
	 acc_forloop_index_type.push_back(tmp_do_index_type);
	 prev_pdo = pdo_node;
	 pdo_node = WN_first(WN_do_body(pdo_node));
   }
 }
 
static WN* ACC_Extract_Seq_Loops_Info(WN * tree )
{
	
	WN        *cur_node;
	WN        *first_node;
	WN        *prev_node;
	WN        *next_node;
  	BOOL 	  is_region;
	for (cur_node = WN_first(tree); cur_node; cur_node = next_node) 
	{

		prev_node = WN_prev(cur_node);
		next_node = WN_next(cur_node);
		//Process the loop region which is inside the kernel region
		if ((is_region = (WN_opcode(cur_node) == OPC_REGION &&
	                     WN_first(WN_region_pragmas(cur_node)) &&
	                     WN_opcode(WN_first(
		        WN_region_pragmas(cur_node))) == OPC_PRAGMA) ) &&
	   		WN_pragma(WN_first(WN_region_pragmas(cur_node))) ==
					WN_PRAGMA_ACC_LOOP_BEGIN) 
		{				
		  //this for loop is seq, ignore any clauses
		  //stupid fortran will generate some trash code right before do-loop		  
		  WN* wn_region_bdy = WN_COPY_Tree(WN_region_body(cur_node));
		  WN* sequential_list = NULL;
		  WN* pdo_node = WN_first(wn_region_bdy);
		  //WN* sequential_tmp = NULL;
		  while(WN_operator(pdo_node) != OPR_DO_LOOP)
		  {
		  	if(sequential_list == NULL)
				sequential_list = WN_CreateBlock();
		    WN_INSERT_BlockLast(sequential_list, WN_COPY_Tree(pdo_node));
			pdo_node = WN_next(pdo_node);
		  }
		  WN* wn_doloop_body = ACC_Extract_Seq_Loops_Info(WN_do_body(pdo_node));
		  WN_do_body(pdo_node) = wn_doloop_body;
		  if(sequential_list != NULL)
		  	WN_INSERT_BlockLast(sequential_list, pdo_node);
		  else
		  	sequential_list = pdo_node;
	
		  //WN* wn_new_body = ACC_Extract_Seq_Loops_Info(WN_do_body(wn_forstmt));
		  //WN_do_body(wn_forstmt) = wn_new_body;
		  WN_prev(sequential_list) = prev_node;
		  WN_next(sequential_list) = next_node;
		  
		  if (WN_prev(cur_node) == NULL)
		    WN_first(tree) = sequential_list;
		  if (WN_next(cur_node) == NULL)
		    WN_last(tree) = sequential_list;

		  RID_Delete( Current_Map_Tab, cur_node );
		  WN_DELETE_Tree(cur_node);
		}
	}

	return tree;
}
 
 WN * 
 ACC_Walk_and_Replace_ACC_Loop_Seq (WN * tree)
 {
   OPCODE op;
   OPERATOR opr;
   INT32 i;
   WN *r;
   WN *temp;
   ST *old_sym;
   WN_OFFSET old_offset;
 
   /* Ignore NULL subtrees. */
 
   if (tree == NULL)
	 return (tree);
 
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
	 WN* pragma_block = WN_first(WN_region_pragmas(tree));
	 WN* cur_node = pragma_block;
	 WN* next_node = WN_next(cur_node);
	 BOOL isSeq = FALSE;
 
	 while ((cur_node = next_node)) 
	 {
		 next_node = WN_next(cur_node);
 
		 if (((WN_opcode(cur_node) == OPC_PRAGMA) ||
		  (WN_opcode(cur_node) == OPC_XPRAGMA)) &&
		 (WN_pragmas[WN_pragma(cur_node)].users & PUSER_ACC)) 
		 {
			 switch (WN_pragma(cur_node)) 
			 {
 
			   case WN_PRAGMA_ACC_CLAUSE_SEQ:
				 isSeq = TRUE;
				 {
					 WN* wn_region_bdy = WN_COPY_Tree(WN_region_body(tree));
					 wn_region_bdy = ACC_Walk_and_Replace_ACC_Loop_Seq(wn_region_bdy);
					 //RID_Delete( Current_Map_Tab, tree );
					 WN* acc_do_node = WN_first(wn_region_bdy);
					 //WN* sequential_tmp = NULL;
					 if(WN_operator(acc_do_node) != OPR_DO_LOOP)
					 {
						 Fail_FmtAssertion("ACC DO LOOP Region must include do loop(ACC_Walk_and_Replace_ACC_Loop_Seq)");
					 }
					 WN_prev(wn_region_bdy) = WN_prev(tree);
					 if(WN_prev(tree))
						 WN_next(WN_prev(tree)) = wn_region_bdy;
					 WN_next(wn_region_bdy) = WN_next(tree);
					 if(WN_next(tree))
						 WN_prev(WN_next(tree)) = wn_region_bdy;
					 
					 //WN_DELETE_Tree(tree);
					 tree = wn_region_bdy;	 
					 return tree;
					 //op = WN_opcode(tree);
					 //opr = OPCODE_operator(op);
				 }
				 break;  
				 
			   default:
				 break;
			 }
		 } 
	  }  
   }
 
   if (op == OPC_BLOCK) 
   {
	 r = WN_first(tree);
	 while (r) 
	 { // localize each node in block
	   r = ACC_Walk_and_Replace_ACC_Loop_Seq (r);
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
	   WN_kid(tree, i) = ACC_Walk_and_Replace_ACC_Loop_Seq( WN_kid(tree, i));
	 }
   }
   return (tree);
 }	 
  
 INT64 GetKernelParamType(ST* pParamST)
 {
	 
	 TY_IDX ty = ST_type(pParamST);
	 TY_KIND kind = TY_kind(ty);//ST_name(old_st)
	 TYPE_ID typeID;
	 if(kind == KIND_POINTER)
	 {		 
		 TY_IDX pty = TY_pointed(ty);
		 //check if it is dynamic array
		 if(TY_kind(pty) == KIND_ARRAY)
		 {
			 pty = TY_etype(pty);
		 }
		 typeID = TY_mtype(pty);
	 }
	 else if(kind == KIND_SCALAR)
	 {
		 typeID = TY_mtype(ty);
	 }
	 else
	 {
		 Is_True(FALSE, ("Wrong Kernel Parameter Kind Type in GetKernelParamType 1."));
	 }
 
	 switch(typeID)
	 {
	 case MTYPE_I1: 	  /*   8-bit integer */
		 return ACC_KDATA_UINT8;
	 case MTYPE_I2: 		  /*  16-bit integer */
		 return ACC_KDATA_UINT16;
	 case MTYPE_I4: 		  /*  32-bit integer */
		 return ACC_KDATA_UINT32;
	 case MTYPE_I8: 		  /*  64-bit integer */
		 return ACC_KDATA_UINT64;
	 case MTYPE_U1: 		  /*   8-bit unsigned integer */
		 return ACC_KDATA_INT8;
	 case MTYPE_U2: 		  /*  16-bit unsigned integer */
		 return ACC_KDATA_INT16;
	 case MTYPE_U4: 		  /*  32-bit unsigned integer */
		 return ACC_KDATA_INT32;
	 case MTYPE_U8: 		  /*  64-bit unsigned integer */
		 return ACC_KDATA_INT64;
	 case MTYPE_F4: 	  /*  32-bit IEEE floating point */
		 return ACC_KDATA_FLOAT;
	 case MTYPE_F8: 	  /*  64-bit IEEE floating point */
		 return ACC_KDATA_DOUBLE;
	 default:
		 Is_True(FALSE, ("Wrong Kernel Parameter Type ID in GetKernelParamType 2."));
	 }
	 return ACC_KDATA_UNKOWN;
 }
 
 //WN node must have a kid which includes buffer region
 //wnArr is a pragma wn node which includes variable declaration
 WN* ACC_GetArraySizeInUnit(ACC_DREGION__ENTRY dEntry)
 {
	 WN* wn_array = dEntry.acc_data_clauses;
	 WN* wn_start = dEntry.acc_data_start[0];
	 WN* wn_length = dEntry.acc_data_length[0];
	 
	 ST* st_array = WN_st(wn_array);
	 TY_IDX ty = ST_type(st_array);
	 TY_KIND kind = TY_kind(ty);
	 
	 WN* wnUpper = wn_length;
	 OPCODE opupper = WN_opcode(wnUpper);
	 //Two cases: array with no region limite which mean the entire array; buffer with region declaration
	 WN* wnBoundary = WN_COPY_Tree(wnUpper);
	 return wnBoundary;
 }
 
 int ACC_Get_Array_TotalDim(WN* wnArr)
 {	 
	 ST* stArr = WN_st(wnArr);
	 TY_IDX ty = ST_type(stArr);
	 TY_KIND kind = TY_kind(ty);
	 int idim = 1;
	 if(TY_kind(ty) == KIND_POINTER)
		 return 1;
	 if(TY_kind(TY_etype(ty)) == KIND_SCALAR)
		 return 1;
	 else if(TY_kind(TY_etype(ty)) != KIND_SCALAR 
				 && TY_kind(TY_etype(ty)) == KIND_ARRAY)
	 {
		 //multi-dimensional array
		 //ty = TY_etype(ty);
		 while(TY_kind(TY_etype(ty)) != KIND_SCALAR 
				 && TY_kind(TY_etype(ty)) == KIND_ARRAY)
		 {
			 ty = TY_etype(ty);
			 idim ++;
		 }
	 }
	 return idim;
 }
 
 UINT32 ACC_Get_ElementSizeForMultiArray(WN* wnArr)
 {
	 ST* stArr = WN_st(wnArr);
	 TY_IDX ty = ST_type(stArr);
	 TY_KIND kind = TY_kind(ty);
	 int idim = 1;
	 if(TY_kind(TY_etype(ty)) != KIND_SCALAR 
				 && TY_kind(TY_etype(ty)) == KIND_ARRAY)
	 {
		 //multi-dimensional array
		 //ty = TY_etype(ty);
		 while(TY_kind(TY_etype(ty)) != KIND_SCALAR 
				 && TY_kind(TY_etype(ty)) == KIND_ARRAY)
		 {
			 ty = TY_etype(ty);
			 idim ++;
		 }
	 }
	 
	 //if(TY_kind(TY_etype(ty)) == KIND_SCALAR)
	 return TY_size(TY_etype(ty));
	 //return idim;
 }
 
 
 
 TY_IDX ACC_Get_ElementTYForMultiArray_Fortran(ST* stArr)
 {
	 //ST* stArr = WN_st(wnArr);
	 TY_IDX ty = ST_type(stArr);
	 TY_KIND kind = TY_kind(ty);
	 int idim = 1;
	 
	 //if the st is dynamic array, then ST kind is pointer, 
	 //TY_pointed will be an array
	 if(TY_kind(ty) == KIND_ARRAY)
	 {
		 ty = TY_etype(ty);
	 }
	 else if(TY_kind(ty) == KIND_STRUCT && F90_ST_Has_Dope_Vector(stArr))
	 {
		 TY_IDX etype;
		 FLD_HANDLE  fli ;
		 TY_IDX base_pointer_ty;
		 fli = TY_fld(ty);
		 base_pointer_ty = FLD_type(fli);
		 ty = TY_etype(TY_pointed(base_pointer_ty));
		 return ty;
	 }
	 
	 //if(TY_kind(TY_etype(ty)) == KIND_SCALAR)
	 return ty;
	 //return idim;
 }
 
 
 TY_IDX ACC_Get_ElementTYForMultiArray(ST* stArr)
 {
	 //ST* stArr = WN_st(wnArr);
	 TY_IDX ty = ST_type(stArr);
	 TY_KIND kind = TY_kind(ty);
	 int idim = 1;
	 
	 if (PU_f77_lang(Current_PU_Info_pu()) || PU_f90_lang(Current_PU_Info_pu()))
	 {
		 return ACC_Get_ElementTYForMultiArray_Fortran(stArr);
	 }
	 //if the st is dynamic array, then ST kind is pointer, 
	 //TY_pointed will be an array
	 if(TY_kind(ty) == KIND_POINTER)
	 {		 
		 ty = TY_pointed(ty);
	 }
	 if(TY_kind(TY_etype(ty)) != KIND_SCALAR 
				 && TY_kind(TY_etype(ty)) == KIND_ARRAY)
	 {
		 //multi-dimensional array
		 //ty = TY_etype(ty);
		 while(TY_kind(TY_etype(ty)) != KIND_SCALAR 
				 && TY_kind(TY_etype(ty)) == KIND_ARRAY)
		 {
			 ty = TY_etype(ty);
			 idim ++;
		 }
	 }
	 
	 //if(TY_kind(TY_etype(ty)) == KIND_SCALAR)
	 return TY_etype(ty);
	 //return idim;
 }
 
 
 WN* ACC_Get_Specified_Dim(WN* wnArr, int n)
 {
 
	 ST* stArr = WN_st(wnArr);
	 TY_IDX ty = ST_type(stArr);
	 TY_KIND kind = TY_kind(ty);
	 int idim = 1;
	 int upper_bound;
	 int lower_bound;
	 
	 if(TY_kind(TY_etype(ty)) != KIND_SCALAR 
				 && TY_kind(TY_etype(ty)) == KIND_ARRAY && n > 0)
	 {
		 //multi-dimensional array
		 //ty = TY_etype(ty);
		 while(TY_kind(TY_etype(ty)) != KIND_SCALAR 
				 && TY_kind(TY_etype(ty)) == KIND_ARRAY)
		 {
			 if(idim == n)
			 {
				 if(ARB_const_stride(TY_arb(ty))
						 && ARB_const_lbnd(TY_arb(ty)) && ARB_const_ubnd(TY_arb(ty)))
				 {			 
					 upper_bound = ARB_ubnd_val(TY_arb(ty));
					 lower_bound = ARB_lbnd_val(TY_arb(ty));
					 break;
				 }
				 else
					 Fail_FmtAssertion(("%s is not a static array in ACC_Get_Specified_Dim."), ST_name(stArr));
			 }
			 ty = TY_etype(ty);
			 idim ++;
		 }
	 }
	 
	 if(TY_kind(ty) == KIND_ARRAY)
	 {
		 if(ARB_const_stride(TY_arb(ty))
				 && ARB_const_lbnd(TY_arb(ty)) && ARB_const_ubnd(TY_arb(ty)))
		 {			 
			 upper_bound = ARB_ubnd_val(TY_arb(ty));
			 lower_bound = ARB_lbnd_val(TY_arb(ty));
		 }
		 else
			 Fail_FmtAssertion(("%s is not a static array in ACC_Get_Specified_Dim."), ST_name(stArr));
	 }
	 
	 WN* wn_dim = WN_Intconst(MTYPE_U4, (upper_bound - lower_bound + 1));
	 return wn_dim;
 }
 
 
 WN* ACC_Load_MultiDimArray_StartAddr(WN* wnArr)
 {
	 ST* base = WN_st(wnArr);
	 int idim = ACC_Get_Array_TotalDim(wnArr);
	 WN *arr_ref = WN_Create( OPCODE_make_op(OPR_ARRAY, Pointer_Mtype,MTYPE_V), 1+2*idim);
	 WN_element_size(arr_ref) = ACC_Get_ElementSizeForMultiArray(wnArr);
	 WN_array_base(arr_ref) = WN_Lda(Pointer_type, 0, base);
 
						   
	 for( int j=0; j<idim; j++ )
	 {
		  //assume the index type to be I8. also assume the dim-size I8 type.
		  //TODO: make it more adaptive. csc
		  WN_array_index( arr_ref, j ) = WN_Intconst(MTYPE_U4, 0);
		  WN_array_dim( arr_ref, j ) = ACC_Get_Specified_Dim(wnArr, j);;
	 }
 
	 //WN_array_index(arr_ref,0) = WN_Intconst(MTYPE_U4, 0);
	 //WN_array_dim(arr_ref,0) = WN_Intconst(MTYPE_U4, idim);
	 return arr_ref;
 }
 
 TY_IDX ACC_GetArrayElemType(TY_IDX ty)
 {
	 TY_IDX tyElement = TY_etype(ty);
	 while(TY_kind(tyElement) == KIND_ARRAY)
		 tyElement = TY_etype(tyElement);
	 return tyElement;
 }
 
 //WN node must have a kid which includes buffer region
 //wnArr is a pragma wn node which includes variable declaration
 //return the offset of the array buffer. It is in byte.
 WN* ACC_GetArrayStart(ACC_DREGION__ENTRY dEntry)
 {
	 //now we only support 1 segment array region for all dimensional arrays
	 WN* wn_array = dEntry.acc_data_clauses;
	 WN* wn_start = dEntry.acc_data_start[0];
	 WN* wn_length = dEntry.acc_data_length[0];
	 
	 ST* st_array = WN_st(wn_array);
	 TY_IDX ty = ST_type(st_array);
	 TY_KIND kind = TY_kind(ty);
	 
	 WN* wnLower = wn_start;
	 //OPCODE oplower = WN_opcode(wnLower);
	 OPERATOR oplower = WN_operator(wnLower);
	 WN* wnUpper = wn_length;
	 //OPCODE opupper = WN_opcode(wnUpper);
	 OPERATOR opupper = WN_operator(wnLower);
	 
	 WN* wnStart;
	 WN* wnElementsize = ACC_GetArrayElementSize(dEntry);
	 wnStart = WN_Binary(OPR_MPY, MTYPE_U4, 
						 wnElementsize, WN_COPY_Tree(wnLower));
	 return wnStart;
 }
 
 WN* ACC_GetArrayElementSize(ACC_DREGION__ENTRY dEntry)
 {
	 ST* st_array;
	 WN* wn_length;
	 UINT32 idims = dEntry.acc_data_length.size();	 
	 WN* wn_array = dEntry.acc_data_clauses;
	 st_array = WN_st(wn_array);
	 TY_IDX ty = ST_type(st_array);
	 TY_KIND kind = TY_kind(ty);
	 int i;
 
	 WN* wn_TotalSize = WN_Intconst(MTYPE_U4, 1);
	 WN* wnElementsize = NULL;
 
	 if(kind == KIND_ARRAY)
	 {
		 TY_IDX tyElement = TY_etype(ty);
		 //for C, multi-dimensional array
		 //it array of array of array ...
		 if(TY_kind(tyElement) == KIND_ARRAY)
			 tyElement = ACC_GetArrayElemType(tyElement);
		 if(TY_kind(tyElement) != KIND_SCALAR)
			 Fail_FmtAssertion("Only support KIND_SCALAR for fortran array, while array :%s is not.", ST_name(st_array));
		 INT32 elemSize = TY_size(tyElement);
		 wnElementsize = WN_Intconst(MTYPE_U4, elemSize);
	 }
	 else if(kind == KIND_POINTER)
	 {
		 TY_IDX tyPointed = TY_pointed(ty);
		 
		 if(TY_kind(tyPointed)==KIND_ARRAY)
			 tyPointed = ACC_GetArrayElemType(tyPointed);
 
		 if(TY_kind(tyPointed) != KIND_SCALAR)
							 Fail_FmtAssertion("Pointer %s is not scalar pointer.", ST_name(st_array));
		 INT32 elemSize = TY_size(tyPointed);
		 wnElementsize = WN_Intconst(MTYPE_U4, elemSize);
	 }
	 else if(F90_ST_Has_Dope_Vector(st_array))
	 {
		 FLD_HANDLE  fli ;
		 TY_IDX base_pointer_ty;
		 fli = TY_fld(Ty_Table[ST_type(st_array)]);
		 base_pointer_ty = FLD_type(fli);
		 TY_IDX e_ty = TY_etype(TY_pointed(base_pointer_ty));
		 INT32 elemSize = TY_size(e_ty);
		 wnElementsize = WN_Intconst(MTYPE_U4, elemSize);
	 }
	 else
	 {
		 Fail_FmtAssertion("unsupported array %s type, in lower acc section (ACC_GetArraySize_Fortran).", ST_name(st_array));
	 }
	 return wnElementsize;
 }
 
 //WN node must have a kid which includes buffer region
 //return the size of the array buffer. It is in byte.
 WN* ACC_GetArraySize(ACC_DREGION__ENTRY dEntry)
 {
	 //now we only support 1 segment array region for all dimensional arrays
	 UINT32 idims = dEntry.acc_data_length.size();	 
	 WN* wn_array = dEntry.acc_data_clauses;
	 WN* wn_TotalSize = WN_Intconst(MTYPE_U4, 1);
	 WN* wnElementsize = NULL;	 
	 ST* st_array = WN_st(wn_array);
	 TY_IDX ty = ST_type(st_array);
	 TY_KIND kind = TY_kind(ty);
	 int i=0;
			 
	 wnElementsize = ACC_GetArrayElementSize(dEntry);
	 
	 for(i=0; i<idims; i++)
	 {
		 WN* wn_length = dEntry.acc_data_length[i];
		 WN* wnBoundary = WN_COPY_Tree(wn_length);
		 WN* wnWholeSize;
		 wn_TotalSize = WN_Binary(OPR_MPY, MTYPE_U4, 
							 wn_TotalSize, wnBoundary);
	 }
	 wn_TotalSize = WN_Binary(OPR_MPY, MTYPE_U4, 
							 wn_TotalSize, wnElementsize);
	 return wn_TotalSize;
 }

 
TY_IDX ACC_GetDopeElementType(ST* st_host)
{
	TY_IDX ty = ST_type(st_host);
	TY_KIND kind = TY_kind(ty);
	FLD_HANDLE  fli ;
	TY_IDX base_pointer_ty;
	fli = TY_fld(Ty_Table[ty]);
	base_pointer_ty = FLD_type(fli);
	TY_IDX e_ty = TY_etype(TY_pointed(base_pointer_ty));
	return e_ty;
}

static WN * 
ACC_Walk_and_Replace_Dope (WN * tree, BOOL bOffloadRegion, BOOL bInArray)
{
  OPCODE op;
  OPERATOR opr;
  INT32 i;
  WN *r;
  WN *temp;
  ST *old_sym;
  WN_OFFSET old_offset;

  /* Ignore NULL subtrees. */

  if (tree == NULL)
    return (tree);

  /* Initialization. */

  op = WN_opcode(tree);
  opr = OPCODE_operator(op);

  /* Look for and replace any nodes referencing localized symbols */
  if(opr == OPR_LDID && bOffloadRegion == TRUE && bInArray == TRUE)
  {
  		ST* st_name = WN_st(tree);
		if(F90_ST_Has_Dope_Vector(st_name) == FALSE)
			return tree;
		old_offset = WN_offsetx(tree);
		//not necessary to replace offset 0 node, leave to acc lower
		if(old_offset == 0)
			return tree;
		map<ST*, dope_array_info_map>::iterator itor = acc_dope_array.find(st_name);
		if(itor == acc_dope_array.end())
		{
			//didn't find it
			//create temp variable
			ST* st_temp = NULL;
			dope_array_info_map darray_info_map;
			dope_array_info darray_info;
			darray_info.wn_original = tree;
			TYPE_ID type_id = WN_rtype(tree);
			ACC_Host_Create_Preg_or_Temp(type_id, "bound", &st_temp);
				
			WN* wn_init_temp = WN_Stid(TY_mtype(ST_type(st_temp)), 0, st_temp, 
								ST_type(st_temp), WN_COPY_Tree(tree));
			
			WN* wn_load_temp = WN_Ldid(TY_mtype(ST_type(st_temp)), 0, st_temp, ST_type(st_temp));
			darray_info.wn_replace = wn_load_temp;
			darray_info.wn_4_assignment = wn_init_temp;
			//put in the map info
			darray_info_map.array_stmt_map[old_offset] = darray_info;
			acc_dope_array[st_name] = darray_info_map;
			
			WN_Delete(tree);
			tree = WN_COPY_Tree(darray_info.wn_replace);
			return tree;
		}
		else
		{
			//find it.
			dope_array_info_map darray_info_map = acc_dope_array[st_name];
			map<INT32, dope_array_info>::iterator itor1 = darray_info_map.array_stmt_map.find(old_offset);
			if(itor1 == darray_info_map.array_stmt_map.end())
			{
				//didn't find the respective offset node
				ST* st_temp = NULL;
				dope_array_info darray_info;
				darray_info.wn_original = tree;
				TYPE_ID type_id = WN_rtype(tree);
				ACC_Host_Create_Preg_or_Temp(type_id, "bound", &st_temp);
				
				WN* wn_init_temp = WN_Stid(TY_mtype(ST_type(st_temp)), 0, st_temp, 
									ST_type(st_temp), WN_COPY_Tree(tree));
				
				WN* wn_load_temp = WN_Ldid(TY_mtype(ST_type(st_temp)), 0, st_temp, ST_type(st_temp));
				darray_info.wn_replace = wn_load_temp;
				darray_info.wn_4_assignment = wn_init_temp;
				//put in the map info
				darray_info_map.array_stmt_map[old_offset] = darray_info;
				acc_dope_array[st_name] = darray_info_map;
			
				WN_Delete(tree);
				tree = WN_COPY_Tree(darray_info.wn_replace);
				return tree;
			}
			else
			{
				//find the respective offset node
				dope_array_info darray_info = darray_info_map.array_stmt_map[old_offset];
				WN_Delete(tree);
				tree = WN_COPY_Tree(darray_info.wn_replace);
				return tree;
			}
		}
		
  }
  else if (opr == OPR_ARRAY && bOffloadRegion == TRUE) 
  {
#define DOPE_DIM_OFFSET (48)
#define DOPE_DIM_SIZE	(8)

	WN* wn_base = WN_array_base(tree) ;
    	ST* array_sym = WN_st(wn_base);
	if(F90_ST_Has_Dope_Vector(array_sym)==TRUE)
	{
		int num_dim = WN_num_dim(tree);
		for(int idim=0; idim<num_dim; idim++)
		{				
			WN* wn_dim = WN_array_dim(tree, idim);
			ST* st_name = WN_st(wn_dim);
			//double check
			if(F90_ST_Has_Dope_Vector(st_name) == FALSE || array_sym != st_name)
				Fail_FmtAssertion("Dope Structure includes unknow symbol!");
			WN_OFFSET old_offset = WN_offset(wn_dim);
			WN_OFFSET suppose_offset = DOPE_DIM_OFFSET + (num_dim - idim -1) 
						* 3 * DOPE_DIM_SIZE + DOPE_DIM_SIZE;
			if(old_offset != suppose_offset)
			{//reset
				WN_offset(wn_dim) = suppose_offset;
			}
		}
	}

  	bInArray = TRUE;
  }
  

  /* Walk all children */
  if(op == OPC_REGION)
  {
	  if ((WN_opcode(tree) == OPC_REGION) && 
			(WN_region_kind(tree) == REGION_KIND_ACC) &&
		     WN_first(WN_region_pragmas(tree)) &&
		     ((WN_opcode(WN_first(WN_region_pragmas(tree))) == OPC_PRAGMA) ||
		      (WN_opcode(WN_first(WN_region_pragmas(tree))) == OPC_XPRAGMA))) 
	  {

		    WN *wtmp = WN_first(WN_region_pragmas(tree));
		    WN_PRAGMA_ID wid = (WN_PRAGMA_ID) WN_pragma(wtmp);

			WN * wn_stmt_block = NULL;
		    switch (wid) 
			{
		    case WN_PRAGMA_ACC_KERNELS_BEGIN:
		    case WN_PRAGMA_ACC_PARALLEL_BEGIN:
			  bOffloadRegion = TRUE;
			  wn_stmt_block = WN_region_body(tree);
			  WN_region_body(tree) = ACC_Walk_and_Replace_Dope(wn_stmt_block, bOffloadRegion, bInArray);
			  //if the dope array info is not empty, then the WHIRL tree is updated.
			  if(!acc_dope_array.empty())
			  {
			  	map<ST*, dope_array_info_map>::iterator itor;
			    	WN* wn_bound_info = WN_CreateBlock();
				WN* wn_new_block = WN_CreateBlock();
				WN* wn_prev = WN_prev(tree);
				WN* wn_next = WN_next(tree);
				for(itor=acc_dope_array.begin(); itor != acc_dope_array.end(); itor ++)
				{
					int j;
					dope_array_info_map darray_info_map = itor->second;
					map<INT32, dope_array_info>::iterator itor1;
					for(itor1=darray_info_map.array_stmt_map.begin(); 
										itor1 != darray_info_map.array_stmt_map.end(); itor1++)
					{
						dope_array_info darray_info = itor1->second;
						WN* wn_assignment = darray_info.wn_4_assignment;
						WN_INSERT_BlockLast(wn_bound_info, wn_assignment);
					}
				}

				WN_INSERT_BlockLast(wn_new_block, wn_bound_info);
				WN_INSERT_BlockLast(wn_new_block, tree);
				tree = wn_new_block;
				WN_prev(tree) = wn_prev;
				if(wn_prev)
					WN_next(wn_prev) = tree;
				WN_next(tree) = wn_next;
				if(wn_next)
					WN_prev(wn_next) = tree;
				acc_dope_array.clear();
			  }
			  return tree;
		      break;

		    default:
		      break;
		    }

		}
  }  
  
  if (op == OPC_BLOCK) 
  {
    r = WN_first(tree);
    while (r) 
	{ // localize each node in block
      r = ACC_Walk_and_Replace_Dope (r, bOffloadRegion, bInArray);
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
      WN_kid(tree, i) = ACC_Walk_and_Replace_Dope ( WN_kid(tree, i), bOffloadRegion, bInArray);
    }
  }
  return (tree);
}   


/*This function must be called before OPENACC OFFLOAD DATA FLOW ANALYSIS*/
WN* VH_OpenACC_Lower(WN * node, INT64 actions)
{	
  /* Validate input arguments. */
  
  //case	WN_PRAGMA_ACC_CLAUSE_DOPE:
	// break;
  //case	WN_PRAGMA_ACC_CLAUSE_DOPE_START_ADDR:
	// break;
  //case	WN_PRAGMA_ACC_CLAUSE_DOPE_DIM:
	 //break;

  Is_True(actions & LOWER_ACC_VH,
	  ("actions does not contain LOWER_ACC_VH"));
  node = ACC_Walk_and_Replace_Dope(node, FALSE, FALSE);

}

