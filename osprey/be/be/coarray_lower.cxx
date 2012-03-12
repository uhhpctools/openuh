/*
  Copyright (C) 2010-2012 University of Houston.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement
  or the like.  Any license provided herein, whether implied or
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with
  other software, or any other product whatsoever.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston MA 02111-1307, USA.

  Contact information:
  http://ww.cs.uh.edu/~hpctools

*/


#include <stdint.h>
#include <sys/types.h>
#if ! defined(BUILD_OS_DARWIN)
#include <elf.h>		    /* for wn.h */
#endif /* ! defined(BUILD_OS_DARWIN) */

#include "defs.h"
#include "wn.h"
#include "erbe.h"

#include "symtab.h"

#include "mtypes.h"
#include "wn_util.h"
#include "wn_tree_util.h"
#include "config_targ.h"
#include "const.h"
#include "cxx_template.h"
#include "cxx_hash.h"
#include "pu_info.h"
#include "coarray_lower.h"
#include "srcpos.h"
#include "tracing.h"
#include "lnopt_main.h"
#include "wn_simp.h"
#include "strtab.h"
#include "region_util.h"
#include "config.h"
#include "prompf.h"
#include "anl_driver.h"
#include "cxx_memory.h"
#include "wb_buffer.h"
#include "wb_carray.h"
#include "wb_browser.h"
#include "wb.h"
#include "targ_const.h"
#include "dra_export.h"
#include "be_symtab.h"

/***********************************************************************
 * Local constants, types, etc.
 ***********************************************************************/

typedef enum {
    READ_TO_LCB = 0,    /* read remote array, store data to LCB */
    WRITE_FROM_LCB = 1, /* load data from LCB, write remote array */
    READ_DIRECT = 2,    /* read remote array, store data directly to
                           local object */
    WRITE_DIRECT = 3    /* load data directly from local object, write
                           remote array */
} ACCESS_TYPE;


typedef enum  {
    NONE = 0,
    DV_BASE_PTR, DV_BASE_LEN, DV_ASSOC, DV_NUM_DIM, DV_TYPE_LEN,
    DV_ORIG_BASE , DV_ORIG_SIZE , DV_DIM1_LB   , DV_DIM1_EXT  ,
    DV_DIM1_STR  , DV_DIM2_LB   , DV_DIM2_EXT  , DV_DIM2_STR  ,
    DV_DIM3_LB   , DV_DIM3_EXT  , DV_DIM3_STR  , DV_DIM4_LB   ,
    DV_DIM4_EXT  , DV_DIM4_STR  , DV_DIM5_LB   , DV_DIM5_EXT  ,
    DV_DIM5_STR  , DV_DIM6_LB   , DV_DIM6_EXT  , DV_DIM6_STR  ,
    DV_DIM7_LB   , DV_DIM7_EXT  , DV_DIM7_STR  , DV_LAST
} DOPEVEC_FIELDS;

typedef struct {
  INT16 ofst32;
  INT16 type32;
  INT16 ofst64;
  INT16 type64;
  const char  *name;
} DOPEVEC_FIELD_INFO;


typedef struct {
  WN *stmt; /* a CAF statement node */
  WN *blk;  /* a block enclosing stmt */
} CAF_STMT_NODE;


/***********************************************************************
 * Local variable definitions
 ***********************************************************************/

static std::vector<CAF_STMT_NODE> caf_delete_list;

static ST *this_image_st = NULL;
static ST *num_images_st = NULL;

static DOPEVEC_FIELD_INFO dopevec_fldinfo[DV_LAST+1] = {
     0,         0,      0,         0,     "",
							/* FIOSTRUCT_NONE */
     0, MTYPE_U4,      0, MTYPE_U8,	 "base_addr",
							/* DV_BASE_PTR */
     4, MTYPE_I4,      8, MTYPE_I8,	 "base_len",
							/* DV_BASE_LEN */
#if 0
     8, MTYPE_U8,      16, MTYPE_U8,	 "flag_info",
							/* DV_FLAG_INFO */
#endif
     8, MTYPE_U4,      16, MTYPE_U4,     "assoc",
							/* DV_ASSOC */
     12, MTYPE_U4,     20, MTYPE_U4,     "num_dims",
							/* DV_NUM_DIM */
     16, MTYPE_U8,     24, MTYPE_U8,	 "type_len",
							/* DV_TYPE_LEN */
     24, MTYPE_U4,     32, MTYPE_U8,	 "orig_base",
							/* DV_ORIG_BASE */
     28, MTYPE_I4,     40, MTYPE_I8,	 "orig_size",
							/* DV_ORIG_SIZE */
     32, MTYPE_I4,     48, MTYPE_I8,	 "dim1_lb",
							/* DV_DIM1_LB */
     36, MTYPE_I4,     56, MTYPE_I8,	 "dim1_extent",
							/* DV_DIM1_EXTENT */
     40, MTYPE_I4,     64, MTYPE_I8,	 "dim1_stride",
							/* DV_DIM1_STRIDE */
     44, MTYPE_I4,     72, MTYPE_I8,	 "dim2_lb",
							/* DV_DIM2_LB */
     48, MTYPE_I4,     80, MTYPE_I8,	 "dim2_extent",
							/* DV_DIM2_EXT */
     52, MTYPE_I4,     88, MTYPE_I8,	 "dim2_stride",
							/* DV_DIM2_STR */
     56, MTYPE_I4,     96, MTYPE_I8,	 "dim3_lb",
							/* DV_DIM3_LB */
     60, MTYPE_I4,     104, MTYPE_I8,	 "dim3_extent",
							/* DV_DIM3_EXT */
     64, MTYPE_I4,     112, MTYPE_I8,	 "dim3_stride",
							/* DV_DIM3_STR */
     68, MTYPE_I4,     120, MTYPE_I8,	 "dim4_lb",
							/* DV_DIM4_LB */
     72, MTYPE_I4,     128, MTYPE_I8,	 "dim4_extent",
							/* DV_DIM4_EXT */
     76, MTYPE_I4,     136, MTYPE_I8,	 "dim4_stride",
							/* DV_DIM4_STR */
     80, MTYPE_I4,     144, MTYPE_I8,	 "dim5_lb",
							/* DV_DIM5_LB */
     84, MTYPE_I4,     152, MTYPE_I8,	 "dim5_extent",
							/* DV_DIM5_EXT */
     88, MTYPE_I4,     160, MTYPE_I8,	 "dim5_stride",
							/* DV_DIM5_STR */
     92, MTYPE_I4,     168, MTYPE_I8,	 "dim6_lb",
							/* DV_DIM6_LB */
     96, MTYPE_I4,     176, MTYPE_I8,	 "dim6_extent",
							/* DV_DIM6_EXT */
     100, MTYPE_I4,    184, MTYPE_I8,	 "dim6_stride",
							/* DV_DIM6_STR */
     104, MTYPE_I4,    192, MTYPE_I8,	 "dim7_lb",
							/* DV_DIM7_LB */
     108, MTYPE_I4,    200, MTYPE_I8,	 "dim7_extent",
							/* DV_DIM7_EXT */
     112, MTYPE_I4,    208, MTYPE_I8,	 "dim7_stride",
							/* DV_DIM7_STR */
};

/***********************************************************************
 * Local macros
 ***********************************************************************/

#define Set_Parent(wn, p) (WN_MAP_Set(Caf_Parent_Map, wn, (void*)  p))
#define Get_Parent(wn) ((WN*) WN_MAP_Get(Caf_Parent_Map, (WN*) wn))

#define NAME_IS( st, name ) \
        strlen( &Str_Table[(st)->u1.name_idx]) == strlen(name) \
        && !strncmp( &Str_Table[(st)->u1.name_idx], name, strlen(name))


/***********************************************************************
 * Local function declarations
 ***********************************************************************/

static BOOL is_dope(const TY_IDX tyi);
static BOOL currentpu_ismain();
static BOOL is_coarray_type(const TY_IDX tyi);
static void set_coarray_tsize(TY_IDX coarray_type);
static INT get_1darray_size(const TY_IDX tyi);
static INT get_coarray_rank(const TY_IDX tyi);
static INT get_array_rank(const TY_IDX tyi);
static INT get_coarray_corank(const TY_IDX tyi);
static TY_IDX get_array_type(const ST * array_st);
static BOOL is_contiguous_access(WN *remote_access);
static TY_IDX create_arr1_type(TYPE_ID elem_type, INT16 ne);

static ST* gen_lcbptr_symbol(TY_IDX tyi, const char *rootname);

static WN* gen_coarray_access_stmt(WN *remote_access, WN *local_access,
                                   ST *lcbtemp, WN *xfer_size,
                                   ACCESS_TYPE access);
static void substitute_lcb(WN *remote_access, ST *lcbtemp,
                           WN *wn_arrayexp,  WN **replace_wn);

static BOOL stmt_rhs_is_addressable(WN *stmt_node);
static BOOL array_ref_is_coindexed(WN *arr);

static int add_caf_stmt_to_delete_list(WN *stmt, WN *blk);
static void delete_caf_stmts_in_delete_list();

static WN * Generate_Call_acquire_lcb(WN *, WN *);
static WN * Generate_Call_release_lcb(WN *);
static WN * Generate_Call_coarray_read(WN *coarray, WN *lcb_ptr,
                                       WN *xfer_size, WN *image);
static WN * Generate_Call_coarray_read_src_str(WN *coarray, WN *lcb_ptr, WN *ndim,
                                    WN *str_mults, WN *extents, WN *strides,
                                    WN *image);
static WN * Generate_Call_coarray_read_full_str(WN *coarray, WN *local,
                                    WN *src_ndim, WN *src_str_mults,
                                    WN *src_extents, WN *src_strides,
                                    WN *dest_ndim, WN *dest_str_mults,
                                    WN *dest_extents, WN *dest_strides,
                                    WN *image);
static WN * Generate_Call_coarray_write(WN *coarray, WN *lcb_ptr,
                                       WN *xfer_size, WN *image);
static WN * Generate_Call_coarray_write_dest_str(WN *coarray, WN *lcb_ptr, WN *ndim,
                                    WN *str_mults, WN *extents, WN *strides,
                                    WN *image);
static WN * Generate_Call_coarray_write_full_str(WN *coarray, WN *local,
                                    WN *dest_ndim, WN *dest_str_mults,
                                    WN *dest_extents, WN *dest_strides,
                                    WN *src_ndim, WN *src_str_mults,
                                    WN *src_extents, WN *src_strides,
                                    WN *image);


// ====================================================================
//
// Utility functions to generate function calls
//
// TODO: move these to their own file in common/com
//
// ====================================================================

static void
My_Get_Return_Pregs(PREG_NUM *rreg1, PREG_NUM *rreg2, mTYPE_ID type,
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
} // My_Get_Return_Pregs()

#define MYGET_RETURN_PREGS(rreg1, rreg2, type) \
  My_Get_Return_Pregs(&rreg1, &rreg2, type, __FILE__, __LINE__)

ST *
Create_VarSym( char *symName, TYPE_ID vtype = MTYPE_I4,
                    SYMTAB_IDX symTab = CURRENT_SYMTAB )
{
  Is_True( symName != NULL, ("Create_VarSym: symName (arg1) is empty") );
  ST *newSym = New_ST ( symTab );
  ST_Init(newSym, Save_Str ( symName ), CLASS_VAR, SCLASS_AUTO,
          EXPORT_LOCAL, MTYPE_To_TY(vtype));

  return newSym;
}

WN *
Generate_Call_Shell( const char *name, TYPE_ID rtype, INT32 argc )
{
  TY_IDX  ty = Make_Function_Type( MTYPE_To_TY( rtype ) );
  ST     *st = Gen_Intrinsic_Function( ty, name );

  Clear_PU_no_side_effects( Pu_Table[ST_pu( st )] );
  Clear_PU_is_pure( Pu_Table[ST_pu( st )] );
  Set_PU_no_delete( Pu_Table[ST_pu( st )] );

  WN *wn_call = WN_Call( rtype, MTYPE_V, argc, st );

  WN_Set_Call_Default_Flags(  wn_call );
  // WN_Reset_Call_Non_Parm_Mod( wn_call );
  // WN_Reset_Call_Non_Parm_Ref( wn_call );

  return wn_call;
}


WN *
Generate_Call( const char *name, TYPE_ID rtype = MTYPE_V )
{
  WN *call = Generate_Call_Shell( name, rtype, 0 );
  return call;
}


inline WN *
Generate_Param( WN *arg, UINT32 flag )
{
  return WN_CreateParm( WN_rtype( arg ), arg,
			MTYPE_To_TY( WN_rtype( arg ) ), flag );
}


// ====================================================================
//
// Routines for carrying out coarray lowering.
//
// ====================================================================

/*
 * Coarray_Prelower
 *
 * Coarray Prelowering will carry out the following tasks:
 *  - replace coarray intrinsics
 *  - standardize remote coarray accesses.
 *    Read:   t = A(...)[...]
 *    Write:  A(...)[...] = t
 *
 */
WN * Coarray_Prelower(PU_Info *current_pu, WN *pu)
{
    BOOL is_main;
    WN *func_body;

    is_main  = currentpu_ismain();

    func_body = WN_func_body( pu );
    if ( is_main ) {
        WN *first_wn = WN_first(func_body);
        WN *call_caf_init = Generate_Call( CAF_INIT );
        WN_INSERT_BlockFirst( func_body, call_caf_init );
    }

    /* create extern symbols _this_image and _num_images. Should be
     * initialized by runtime library
     */

    if (this_image_st == NULL) {
        this_image_st = New_ST( GLOBAL_SYMTAB );
        ST_Init( this_image_st, Save_Str( "_this_image" ),
                CLASS_VAR, SCLASS_EXTERN, EXPORT_PREEMPTIBLE,
                MTYPE_To_TY(MTYPE_U8));
    }

    if (num_images_st == NULL) {
        num_images_st = New_ST( GLOBAL_SYMTAB );
        ST_Init( num_images_st, Save_Str( "_num_images" ),
                CLASS_VAR, SCLASS_EXTERN, EXPORT_PREEMPTIBLE,
                MTYPE_To_TY(MTYPE_U8));
    }

    WN_TREE_CONTAINER<PRE_ORDER> wcpre(func_body);
    WN_TREE_CONTAINER<PRE_ORDER> ::iterator wipre, curr_wipre=NULL, temp_wipre = NULL;

    /* for support for character coarrays
     **/
    WN *lhs_ref_param_wn = NULL;
    WN *rhs_ref_param_wn = NULL;
    WN *lhs_size_param_wn = NULL;
    WN *rhs_size_param_wn = NULL;

    for (wipre = wcpre.begin(); wipre != wcpre.end(); ++wipre) {
        WN *insert_blk;
        WN *wn = wipre.Wn();
        WN *wn_next;
        WN *insert_wnx;
        WN *blk_node;
        WN *stmt_node;
        WN *parent;
        ST *func_st;
        INT8 rank, corank;
        ST *array_st;
        WN *wn_arrayexp;

        ST *lcbptr_st;
        WN *transfer_size_wn;
        ST *st1;
        TY_IDX ty1, ty2, ty3;
        WN *replace_wn = NULL;
        
        parent = wipre.Get_parent_wn();

        /* if its a statement, set stmt_node and set blk_node to
         * parent */
        if ((OPCODE_is_stmt(WN_opcode(wn)) ||
            OPCODE_is_scf(WN_opcode(wn)) &&
             WN_operator(wn) != OPR_BLOCK &&
             WN_operator(wn) != OPR_FUNC_ENTRY)) {
          stmt_node = wn;
          wn_arrayexp = NULL;
          if (WN_operator(parent) == OPR_BLOCK) blk_node = parent;
        }

        /* stores most recently encountered ARRAYEXP in wn_arrayexp */
        if (WN_operator(wn) == OPR_ARRAYEXP)
          wn_arrayexp = wn;

        switch (WN_operator(wn)) {
            case OPR_CALL:
                func_st = &St_Table[ WN_st_idx(wn) ];
                if ( NAME_IS(func_st, "_THIS_IMAGE0") ) {
                    /* IR looks like:
                     *   wn:          CALL _THIS_IMAGE0
                     *   WN_next(wn): STID t$n
                     *                   LDID .preg_return_val
                     * We replace with:
                     *   WN_next(wn): STID t$n
                     *                   LDID _this_image
                     */
                    wn_next = WN_next(wn);
                    wipre.Delete();
                    Is_True( WN_operator(wn_next) == OPR_STID,
                     ("Expected STID in IR after _THIS_IMAGE0() call"));
                    WN_Delete( WN_kid0(wn_next) );
                    WN_kid0(wn_next) = WN_Ldid(MTYPE_U8, 0,
                            this_image_st, ST_type(this_image_st));
                } else if ( NAME_IS(func_st, "num_images") ) {
                    wn_next = WN_next(wn);
                    wipre.Delete();
                    Is_True( WN_operator(wn_next) == OPR_STID,
                     ("Expected STID in IR after _num_images() call"));
                    WN_Delete( WN_kid0(wn_next) );
                    WN_kid0(wn_next) = WN_Ldid(MTYPE_U8, 0,
                            num_images_st, ST_type(num_images_st));
                } else if ( NAME_IS(func_st, "sync_images_") ) {
                    wn_next = WN_next(wn);

                    Is_True( WN_num_actuals(wn) == 2,
                            ("Expected 2 args for sync_images from FE"));

                    //WN_Delete( WN_kid0(WN_actual(wn,1)));
                    st1 = WN_st(WN_kid0(WN_actual(wn,0)));
                    ty1 = ST_type(st1);
                    if (TY_kind(ty1) == KIND_POINTER)
                        ty1 = TY_pointed(ty1);
                    Is_True( is_dope(ty1),
                            ("Expected sync_images arg 1 to be a dope from FE"));

                    /* args: DV, 1 -> array_list, #array_list */
                    WN_Delete(WN_kid0(WN_actual(wn,0)));
                    WN_Delete(WN_kid0(WN_actual(wn,1)));
                    WN_kid0(WN_actual(wn,0)) =
                        WN_Ldid(Pointer_type, 0 /* DV_BASE_PTR ofst */,
                                st1, MTYPE_To_TY(Pointer_type));
                    if (TY_size(ty1) > ( Pointer_Size == 4?
                                dopevec_fldinfo[DV_DIM1_LB].ofst32 :
                                dopevec_fldinfo[DV_DIM1_LB].ofst64)) {
                        WN_kid0(WN_actual(wn,1)) =
                            WN_Ldid(Integer_type,
                                    Pointer_Size == 4 ?
                                    dopevec_fldinfo[DV_DIM1_EXT].ofst32 :
                                    dopevec_fldinfo[DV_DIM1_EXT].ofst64,
                                    WN_st(WN_kid0(WN_actual(wn,0))),
                                    MTYPE_To_TY(Integer_type));
                    } else {
                        WN_kid0(WN_actual(wn,1)) =
                            WN_Intconst(Integer_type, 1);
                    }
                }

                /* check for call to _END */
                if ( NAME_IS(func_st, "_END") ) {
                    insert_wnx = Generate_Call( CAF_FINALIZE );
                    WN_INSERT_BlockBefore(blk_node, wn, insert_wnx);
                }
                break;
            case OPR_INTRINSIC_CALL:
                if (WN_opcode(wn) == OPC_VINTRINSIC_CALL) {
                  if (WN_intrinsic(wn) == INTRN_STOP_F90) {
                    insert_wnx = Generate_Call( CAF_FINALIZE );
                    WN_INSERT_BlockBefore(blk_node, wn, insert_wnx);
                  
                  } else if (WN_intrinsic(wn) == INTRN_CASSIGNSTMT) {
                    /*for character coarrays support*/ 
                    lhs_ref_param_wn = WN_kid0(wn);
                    rhs_ref_param_wn = WN_kid1(wn);
                    lhs_size_param_wn = WN_kid2(wn);
                    rhs_size_param_wn = WN_kid3(wn);
                  }
                } 
                break;
            case OPR_ARRAY:
            case OPR_ARRSECTION:
                if (WN_operator(WN_kid0(wn)) == OPR_ILOAD) break; /* not a coarray */
                /*keep iterating till LDA or LDID is reached....
                 * for char coarray support*/
                curr_wipre = wipre;
                while( WN_kid0(curr_wipre.Wn()) &&
                       WN_operator(WN_kid0(curr_wipre.Wn())) != OPR_LDA &&
                       WN_operator(WN_kid0(curr_wipre.Wn())) != OPR_LDID) {
                  ++curr_wipre;
                }
                wn = curr_wipre.Wn();
                temp_wipre = wipre;
                wipre = curr_wipre;
                
               /*generic coarray syntax check for ints and chars*/ 
                array_st = WN_st(WN_kid0(wn));
                ty1 = get_array_type(array_st);
                if ( is_dope(ty1) ) { /*check if allocatable type*/
                    ty1 = TY_pointed(FLD_type(TY_fld(ty1)));
                    /*
                    if (TY_kind(ty2) == KIND_POINTER)
                        ty2 = TY_pointed(ty2);
                        */
                    if (!is_coarray_type(ty1))
                        break;
                    rank = get_coarray_rank(ty1);
                    corank = get_coarray_corank(ty1);
                } else  {             /*check if save type*/
                    /* break if not coarray */
                    ty1 = get_array_type(array_st);
                    if (!is_coarray_type(ty1))  /* break if not coarray */
                        break;

                    rank = get_coarray_rank(ty1);
                    corank = get_coarray_corank(ty1);
                }

                /* break if not cosubscripted */
                if (WN_kid_count(wn) == (1+2*rank))
                    break;

                if (WN_operator(parent) == OPR_ILOAD) {
                    /* this is a coarray read */

                    WN *local_access = NULL;
                    lcbptr_st = NULL;
                    transfer_size_wn = NULL;

                    lcbptr_st = gen_lcbptr_symbol( MTYPE_To_TY(Pointer_type),
                                "lcbptr");

                    /* LCB only used if RHS is not addressable */
                    if (stmt_rhs_is_addressable(stmt_node) &&
                        WN_operator(stmt_node) == OPR_ISTORE &&
                        WN_operator(WN_kid1(stmt_node)) == OPR_ARRAYEXP &&
                        !array_ref_is_coindexed(WN_kid0(WN_kid1(stmt_node)))) {
                      /* TODO: what about offset field? */
                      Is_True(WN_offset(stmt_node) == 0,
                          ("Unhandled non-zero offset in stmt_node"));
                      WN *lhs = WN_COPY_Tree(WN_kid1(stmt_node));
                      if (WN_operator(lhs) == OPR_ARRAYEXP) {
                        local_access = WN_kid0(lhs);
                        Is_True(WN_operator(local_access) == OPR_ARRSECTION,
                            ("expecting the operator for local_access "
                             "to be ARRSECTION"));
                      } else {
                        /* should not reach */
                        Is_True(0, ("bad WHIRL node encountered in LHS"));
                      }

                      //fdump_tree(stdout, stmt_node);

                      /* TODO: move coarray read generation to subsequent
                       * lowering phase  (after analysis/optimization)
                       */
                      insert_blk = gen_coarray_access_stmt( wn, local_access, lcbptr_st,
                          transfer_size_wn, READ_DIRECT);

                      insert_wnx = WN_first(insert_blk);
                      while (insert_wnx) {
                        insert_wnx = WN_EXTRACT_FromBlock(insert_blk,
                                                          insert_wnx);
                        WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);
                        insert_wnx = WN_first(insert_blk);
                      }

                      add_caf_stmt_to_delete_list(stmt_node, blk_node);
                      WN_Delete(insert_blk);
                    } else {
                      /* get size of transfer data */
                      transfer_size_wn =  WN_Intconst(MTYPE_U8,
                                                      TY_size( Ty_Table[ty1].u2.etype )
                                                      );
                      if (WN_operator(wn) == OPR_ARRSECTION) {
                        for (INT8 i = 1; i < WN_kid_count(wn_arrayexp); i++) {
                          transfer_size_wn = WN_Mpy(MTYPE_U8,
                              WN_COPY_Tree(WN_kid(wn_arrayexp,i)),
                              transfer_size_wn);
                        }
                      }
                      insert_wnx = Generate_Call_acquire_lcb(transfer_size_wn,
                          WN_Lda(Pointer_type, 0, lcbptr_st));

                      WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);

                      /* TODO: move coarray read generation to subsequent
                       * lowering phase  (after analysis/optimization)
                       */
                      insert_blk = gen_coarray_access_stmt( wn, NULL, lcbptr_st,
                          transfer_size_wn, READ_TO_LCB);

                      /* replace temp into coarray reference */
                      substitute_lcb(wn, lcbptr_st, wn_arrayexp, &replace_wn); 

                      WN_Delete(wn);
                      
                      wipre.Replace(replace_wn);
                      insert_wnx = WN_first(insert_blk);
                      while (insert_wnx) {
                        insert_wnx = WN_EXTRACT_FromBlock(insert_blk,
                                                          insert_wnx);
                        WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);
                        insert_wnx = WN_first(insert_blk);
                      }

                      WN_Delete(insert_blk);
                    }

                } else if (WN_operator(parent) == OPR_ISTORE ||
                           WN_operator(parent) == OPR_ARRAYEXP) {
                    WN *local_access = NULL;
                    lcbptr_st = NULL;
                    transfer_size_wn = NULL;

                    /* LCB only used if RHS is not addressable */
                    if (stmt_rhs_is_addressable(stmt_node) &&
                        !array_ref_is_coindexed(WN_kid0(WN_kid0(
                                                WN_kid0(stmt_node))))) {
                      WN *rhs = WN_COPY_Tree(WN_kid0(stmt_node));
                      if (WN_operator(rhs) == OPR_ARRAYEXP &&
                          WN_operator(WN_kid0(rhs)) == OPR_ILOAD) {
                        local_access = WN_kid0(WN_kid0(rhs));
                        Is_True(WN_operator(local_access) == OPR_ARRSECTION,
                            ("expecting the operator for local_access "
                             "to be ARRSECTION"));
                      } else {
                        /* should not reach */
                        Is_True(0, ("bad WHIRL node encountered in RHS"));
                      }

                      /* TODO: move coarray write generation to subsequent
                       * lowering phase  (after analysis/optimization)
                       */
                      insert_blk = gen_coarray_access_stmt( wn, local_access,
                          lcbptr_st, transfer_size_wn, WRITE_DIRECT);

                      insert_wnx = WN_last(insert_blk);
                      while (insert_wnx) {
                        insert_wnx = WN_EXTRACT_FromBlock(insert_blk, insert_wnx);
                        WN_INSERT_BlockAfter(blk_node, stmt_node, insert_wnx);
                        insert_wnx = WN_last(insert_blk);
                      }

                      add_caf_stmt_to_delete_list(stmt_node, blk_node);

                      WN_Delete(insert_blk);
                    } else {
                      /* this is coarray write. Parent operator is OPR_ISTORE
                       * if indexing a single element, and it will be
                       * OPR_ARRAYEXP if indexing multiple elements with
                       * OPR_ARRSECTION
                       */
                      lcbptr_st = gen_lcbptr_symbol( MTYPE_To_TY(Pointer_type),
                              "lcbptr");

                      /* get size of transfer data */
                      transfer_size_wn =  WN_Intconst(MTYPE_U8,
                          TY_size( Ty_Table[ty1].u2.etype ));

                      if (WN_operator(wn) == OPR_ARRSECTION) {
                          for (INT8 i = 1;
                               i < WN_kid_count(wn_arrayexp); i++) {
                              transfer_size_wn = WN_Mpy(MTYPE_U8,
                                    WN_COPY_Tree(WN_kid(wn_arrayexp,i)),
                                    transfer_size_wn);
                          }
                      }
                      insert_wnx = Generate_Call_acquire_lcb(transfer_size_wn,
                              WN_Lda(Pointer_type, 0, lcbptr_st));

                      WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);

                      /* replace temp into coarray reference */
                      substitute_lcb(wn, lcbptr_st, wn_arrayexp, &replace_wn);

                      /* TODO: move coarray write generation to subsequent
                       * lowering phase  (after analysis/optimization)
                       */
                      insert_blk = gen_coarray_access_stmt( wn, local_access,
                          lcbptr_st, transfer_size_wn, WRITE_FROM_LCB);
                      insert_wnx = WN_last(insert_blk);
                      while (insert_wnx) {
                        insert_wnx = WN_EXTRACT_FromBlock(insert_blk, insert_wnx);
                        WN_INSERT_BlockAfter(blk_node, stmt_node, insert_wnx);
                        insert_wnx = WN_last(insert_blk);
                      }

                      WN_Delete(wn);
                      
                      wipre.Replace(replace_wn);
                      WN_Delete(insert_blk);
                    }
                }
                else if (WN_operator(parent) == OPR_PARM) {
                      if(WN_operator(wipre.Get_parent_wn()) == OPR_PARM )
                        break;
                      WN *local_access = NULL;
                      lcbptr_st = NULL;
                      transfer_size_wn = NULL;
                      lcbptr_st = gen_lcbptr_symbol( MTYPE_To_TY(Pointer_type),
                                                     "lcbptr");
                      transfer_size_wn =  WN_Intconst(MTYPE_U8,
                                          TY_size( Ty_Table[ty1].u2.etype ));
                      if (WN_operator(wn) == OPR_ARRSECTION) {
                          for (INT8 i = 1; i < WN_kid_count(wn_arrayexp); i++) {
                              transfer_size_wn = WN_Mpy(MTYPE_U8,
                                                 WN_COPY_Tree(WN_kid(wn_arrayexp,i)),
                                                 transfer_size_wn);                                 
                          }
                      }
                      insert_wnx = Generate_Call_acquire_lcb(transfer_size_wn,
                                                             WN_Lda(Pointer_type, 
                                                             0,
                                                             lcbptr_st)
                                                             );
                      WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);
                      if (parent == rhs_ref_param_wn) {
                          /*coarray read*/
                          insert_blk = gen_coarray_access_stmt( wn, NULL, lcbptr_st,
                                                      transfer_size_wn, READ_TO_LCB);      
                          /* get LDID node in replace_wn that substitues wn
                           * pointed to by wipre*/
                          substitute_lcb(wn, lcbptr_st, wn_arrayexp, &replace_wn);
                          wipre.Replace(replace_wn);
                          insert_wnx = WN_first(insert_blk);
                          while (insert_wnx) {
                                 insert_wnx = WN_EXTRACT_FromBlock(insert_blk,
                                                                  insert_wnx);
                                 WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);
                                 insert_wnx = WN_first(insert_blk);
                          }
                          WN_Delete(wn); 
                          wipre.Replace(replace_wn);
                          WN_Delete(insert_blk);
                      } else if (parent == lhs_ref_param_wn) {
                           /*corray write*/
                           
                           /* get LDID node in replace_wn that substitues wn 
                            * pointed to by wipre*/
                           substitute_lcb(wn, lcbptr_st, wn_arrayexp, &replace_wn);
                           insert_blk = gen_coarray_access_stmt( wn, local_access,
                                        lcbptr_st, transfer_size_wn, WRITE_FROM_LCB);
                           insert_wnx = WN_last(insert_blk);
                           while (insert_wnx) {
                                 insert_wnx = WN_EXTRACT_FromBlock(insert_blk, insert_wnx);
                                 WN_INSERT_BlockAfter(blk_node, stmt_node, insert_wnx);
                                 insert_wnx = WN_last(insert_blk);
                            }
                            WN_Delete(wn);
                            wipre.Replace(replace_wn);
                            WN_Delete(insert_blk);
                      }          
                }
                else if (WN_operator(parent) == OPR_STID) {
                /* IR looks something like this:
                 * U8STID 0 <2,7,_temp_.dope.0> T<69,anon_ptr.,8>
                 *                                     {line: 1/12}
                 *  U8ARRAY 3 4
                 *    U8LDA 0 <2,1,A> T<59,anon_ptr.,8>
                 *    I4INTCONST 1 (0x1)
                 *    I8INTCONST 5 (0x5)
                 *    I8INTCONST 5 (0x5)
                 *    I4INTCONST 1 (0x1)
                 *    I4INTCONST 0 (0x0)
                 *    I4INTCONST 0 (0x0)
                 *
                 *  The back-end may encounter this if a co-indexed object
                 *  expression is converted into a dope vector by front-end.
                 *  E.g. print *, a(:)[p]
                 *
                 */
                }
                temp_wipre = NULL;
                break;
        }
    }

    /* remove statements in caf_delete_list and clear the list */
    delete_caf_stmts_in_delete_list();

    /* resize coarrays.
     * TODO: This should be fixed in front-end, actually.
     *       not tested on deferred-size / allocatables.
     * */
    ST *sym;
    UINT32 i;
    FOREACH_SYMBOL(CURRENT_SYMTAB, sym, i) {
        if (is_coarray_type(ST_type(sym))) {
            set_coarray_tsize(ST_type(sym));
        }
    }


    return pu;
} /* Coarray_Prelower */



static const char * const dope_str_prefix = ".dope." ;
static const INT dope_str_prefix_len = 6;

static BOOL is_dope(const TY_IDX tyi)
{
  if (TY_kind(tyi) == KIND_STRUCT &&
      strncmp(TY_name(tyi), dope_str_prefix, dope_str_prefix_len) == 0)
    return TRUE;
  else
      return FALSE;
}


/*
 * currentpu_ismain
 *
 * Checks if the current PU is the main entry point of program
 */
static BOOL currentpu_ismain()
{
	ST *pu_st = Get_Current_PU_ST();
	char *pu_name = ST_name(pu_st);

	switch (PU_src_lang(Get_Current_PU())) {
		case PU_C_LANG:
		case PU_CXX_LANG:
			return strcmp( pu_name, "main") == 0;
			break;
		case PU_F90_LANG:
		case PU_F77_LANG:
			return strcmp( pu_name, "MAIN__") == 0;
			break;
		default:
			FmtAssert (FALSE, ("Unknown source language type"));
	}
} /* currentpu_ismain */


/*
 * is_coarray_type
 *
 * Checks if array type is coarray (has codimensions)
 */
static BOOL is_coarray_type(const TY_IDX tyi)
{
    if (TY_kind(tyi) == KIND_POINTER)
      return TY_is_coarray(TY_pointed(tyi));
    else
      return TY_is_coarray(tyi);
} /* is_coarray_type */

/*
 * set_coarray_tsize
 *
 * Determines size of (local) coarray. Set TY_size for coarray type
 * accordingly.
 *
 * TODO: not tested on deferred-size / allocatables.
 */
static void set_coarray_tsize(TY_IDX coarray_type)
{
    UINT16 i;
    TY_IDX real_coarray_type = coarray_type;
    TY_IDX base_type;
    if (TY_kind(coarray_type) == KIND_POINTER) {
        real_coarray_type = TY_pointed(coarray_type);
    }
    base_type = Ty_Table[real_coarray_type].u2.etype;
    ARB_HANDLE bounds(Ty_Table[real_coarray_type].Arb());
    INT ndim, co_dim, array_dim;
    ndim = ARB_dimension(bounds);
    co_dim = ARB_codimension(bounds);
    array_dim = ndim - co_dim;
    INT32 coarray_size =  TY_size(base_type);

    if (array_dim != 0)
        for (i = co_dim; i < ndim; i++) {
            INT16 ub = bounds[i].Entry()->u2.ubnd_val;
            INT16 lb = bounds[i].Entry()->u1.lbnd_val;
            coarray_size *= ub - lb + 1;
        }

    Set_TY_size(real_coarray_type, coarray_size);
} /* set_coarray_tsize */

static INT get_1darray_size(const TY_IDX tyi)
{
    ARB_HANDLE arb(Ty_Table[tyi].Arb());
    return (ARB_ubnd_val(arb) - ARB_lbnd_val(arb)+1);
}

static INT get_array_rank(const TY_IDX tyi)
{
    ARB_HANDLE arb(Ty_Table[tyi].Arb());
    return (ARB_dimension(arb));
}

static INT get_coarray_rank(const TY_IDX tyi)
{
    ARB_HANDLE arb(Ty_Table[tyi].Arb());
    return (ARB_dimension(arb) - ARB_codimension(arb));
}

static INT get_coarray_corank(const TY_IDX tyi)
{
    ARB_HANDLE arb(Ty_Table[tyi].Arb());
    return (ARB_codimension(arb));
}

static TY_IDX get_array_type(const ST *array_st)
{
    TY_IDX ty = ST_type(array_st);
    if (TY_kind(ty) == KIND_POINTER)
        return TY_pointed(ty);
    return ty;
}

static ST* gen_lcbptr_symbol(TY_IDX tyi, const char *rootname)
{
    ST *st = Gen_Temp_Named_Symbol(tyi, rootname, CLASS_VAR, SCLASS_AUTO);
    Set_ST_is_temp_var(st);
    Set_ST_is_lcb_ptr(st);
    return st;
}

/*
 * gen_coarray_access_stmt:
 *
 * Assumptions:
 *  - operator for remote_read is either OPR_ARRAY or OPR_ARRSECTION.
 *  - lcbtemp has been allocated with sufficient space to receive entire
 *    transfer.
 *  - the read may be noncontiguous, but it must be a continuous block-wise
 *    shape. So, no strides on OPR_TRIPLET indexes allowed.
 *  - won't work with allocatable or assumed-shape coarrays
 *
 *  TODO:
 *  - handle allocatable or assumed-shape coarrays (dim size is not
 *    OPR_INTCONST)
 *  - other aggregate types for indexes other than OPR_TRIPLET?
 *  - handle strides on OPR_TRIPLET indexes
 *  - probably more ...
 */
static WN* gen_coarray_access_stmt(WN *remote_access, WN *local_access,
                                   ST *lcbtemp, WN *xfer_size,
                                   ACCESS_TYPE access)
{
    WN *return_blk;
    WN *call;
    WN *coarray_wn; /* base address for remote access */
    WN *local_wn;   /* base address for local buffer (if addressable) */
    WN *temp_wn;    /* LCB base address */
    WN *image;
    TY_IDX ty1, ty2;
    BOOL strided_access = FALSE;
    BOOL local_strided_access = FALSE;

    return_blk = WN_CreateBlock();

    ST *coarray_st = WN_st(WN_kid0(remote_access));
    ty1 = get_array_type(coarray_st);
    if ( is_dope(ty1) )
        ty1 = TY_pointed(FLD_type(TY_fld(ty1)));

    TY_IDX elem_type = Ty_Table[ty1].u2.etype;
    INT8 rank = get_coarray_rank(ty1);
    INT8 corank = get_coarray_corank(ty1);
    INT8 totalrank = rank +  corank;


    /* calculate base address for coarray access */
    WN** wn_str_m = (WN **)malloc(rank*sizeof(WN*));
    WN** wn_costr_m = (WN **)malloc(corank*sizeof(WN*));
    WN *offset;

    wn_str_m[0] = WN_Intconst(Integer_type, TY_size(elem_type));
    BOOL noncontig = (WN_element_size(remote_access) < 0);
    for (INT8 i=1; i < rank; i++) {
        if (noncontig)
          wn_str_m[i] = WN_COPY_Tree(WN_kid(remote_access,totalrank-i));
        else
          wn_str_m[i] = WN_Mpy( Integer_type,
              WN_COPY_Tree(wn_str_m[i-1]),
              WN_COPY_Tree(WN_kid(remote_access,totalrank+1-i)));
    }

    offset = WN_Intconst(Integer_type, 0);

    for (INT8 i=0; i < rank; i++) {
        WN *dim_offset;
        WN *sub = WN_array_index(remote_access, totalrank-i-1);
        /* TODO: handle other aggregate operators? */
        if (WN_operator(sub) == OPR_TRIPLET) {
            dim_offset = WN_COPY_Tree( WN_kid0( sub));
            /* if stride is > 1 or not a const, then this is a strided
             * access */
            if ( WN_operator(WN_kid1(sub)) != OPR_INTCONST ||
                 WN_const_val(WN_kid1(sub)) > 1)
              strided_access = TRUE;
        } else {
            dim_offset = WN_COPY_Tree( sub );
        }

        offset = WN_Add(MTYPE_I8,
                WN_Mpy(MTYPE_I8, dim_offset,
                WN_COPY_Tree(wn_str_m[i]) ),
                        offset);
    }

    coarray_wn =  WN_Add(Pointer_type,
                    WN_COPY_Tree(WN_kid0(remote_access)),
                    offset);

    /* base address for buffer */
    WN** local_wn_str_m = NULL;
    WN*  local_offset = NULL;
    INT8 local_rank = 0;
    if (access == READ_TO_LCB || access == WRITE_FROM_LCB) {
      temp_wn =  WN_Ldid(Pointer_type, 0, lcbtemp,
                          MTYPE_To_TY(Pointer_type));
      local_wn = WN_Intconst(Pointer_type, 0);
    } else {
      /* access == READ_DIRECT || access == WRITE_DIRECT */
      Is_True(local_access != NULL,
          ("local_access is NULL for DIRECT access type"));
      temp_wn = WN_Intconst(Pointer_type, 0);

      /* assuming local_access is an ARRSECTION node */

      TY_IDX etype;
      ST *local_array_st = WN_st(WN_kid0(local_access));
      TY_IDX ty = get_array_type(local_array_st);
      if ( is_dope(ty) || TY_kind(ty) == KIND_POINTER)
        ty = TY_pointed(FLD_type(TY_fld(ty)));

      if (TY_kind(ty) == KIND_ARRAY) {
        etype = Ty_Table[ty].u2.etype;
        local_rank = get_coarray_rank(ty);
        local_wn_str_m = (WN **)malloc(local_rank*sizeof(WN*));
        local_wn_str_m[0] = WN_Intconst(Integer_type, TY_size(etype));
      } else {
        /* assumes rank 1 */
        local_rank = 1;
        local_wn_str_m = (WN **)malloc(local_rank*sizeof(WN*));
        local_wn_str_m[0] = WN_Intconst(Integer_type,
                                         (INT)WN_element_size(local_access));
      }
      BOOL local_noncontig = (WN_element_size(local_access) < 0);

      for (INT8 i=1; i < local_rank; i++) {
          if (local_noncontig)
            local_wn_str_m[i] = WN_COPY_Tree(WN_kid(local_access,
                                                    local_rank-i));
          else
            local_wn_str_m[i] = WN_Mpy( Integer_type,
                WN_COPY_Tree(local_wn_str_m[i-1]),
                WN_COPY_Tree(WN_kid(local_access,local_rank+1-i)));
      }

      local_offset = WN_Intconst(Integer_type, 0);
      for (INT8 i=0; i < local_rank; i++) {
        WN *dim_offset;
        WN *sub = WN_array_index(local_access, (local_rank-i-1));
        /* TODO: handle other aggregate operators? */
        if (WN_operator(sub) == OPR_TRIPLET) {
          dim_offset = WN_COPY_Tree( WN_kid0( sub));
          /* if stride is > 1 or not a const, then this is a strided
           * access */
          if ( WN_operator(WN_kid1(sub)) != OPR_INTCONST ||
              WN_const_val(WN_kid1(sub)) > 1)
            local_strided_access = TRUE;
        } else {
          dim_offset = WN_COPY_Tree( sub );
        }

        local_offset = WN_Add(MTYPE_I8,
            WN_Mpy(MTYPE_I8, dim_offset,
              WN_COPY_Tree(local_wn_str_m[i]) ),
            local_offset);
      }

      local_wn =  WN_Add(Pointer_type, WN_COPY_Tree(WN_kid0(local_access)),
                         local_offset);
    }

    /* calculate remote image */
    wn_costr_m[0] = WN_Intconst(Integer_type, 1);
    for (INT8 i=1; i < corank; i++) {
        if (noncontig)
          wn_costr_m[i] = WN_COPY_Tree(WN_kid(remote_access,corank-i));
        else
          wn_costr_m[i] = WN_Mpy( MTYPE_I8,
              WN_COPY_Tree(wn_costr_m[i-1]),
              WN_COPY_Tree(WN_kid(remote_access,corank+1-i)));
    }

    image = WN_Intconst(MTYPE_U8, 1);
    for (INT8 i=0; i < corank; i++) {
        image = WN_Add(MTYPE_U8,
                WN_Mpy(MTYPE_I8,
                WN_COPY_Tree(WN_kid(remote_access, 2*totalrank-rank-i)),
                WN_COPY_Tree(wn_costr_m[i]) ),
                image);
    }

    /* if contiguous transfer, generate call to coarray_access */
    if (is_contiguous_access(remote_access) &&
        (access == READ_TO_LCB || access == WRITE_FROM_LCB)) {
        if (access == READ_TO_LCB) {
            call = Generate_Call_coarray_read(coarray_wn, temp_wn,
                                        WN_COPY_Tree(xfer_size),
                                        image);
        } else if (access == WRITE_FROM_LCB) {
            call = Generate_Call_coarray_write(coarray_wn, temp_wn,
                                        WN_COPY_Tree(xfer_size),
                                        image);
        }
        WN_INSERT_BlockFirst( return_blk, call);
    } else {
        TY_IDX arr_ty = create_arr1_type(MTYPE_U8, rank );
        ST *strides;
        ST *str_mults = Gen_Temp_Named_Symbol( arr_ty, "str_mults", CLASS_VAR,
                SCLASS_AUTO);
        Set_ST_is_temp_var(str_mults);
        ST *extents = Gen_Temp_Named_Symbol( arr_ty, "extents", CLASS_VAR,
                SCLASS_AUTO);
        Set_ST_is_temp_var(extents);

        if (strided_access) {
          strides = Gen_Temp_Named_Symbol( arr_ty, "strides", CLASS_VAR,
              SCLASS_AUTO);
          Set_ST_is_temp_var(strides);
        }

        OPCODE op_array = OPCODE_make_op( OPR_ARRAY,
                                        Pointer_type, MTYPE_V );
        for (INT8 i = 0; i  < rank; i++) {
          WN *sm_array = WN_Create( op_array, 3 );
          WN_element_size( sm_array ) =
              TY_size( TY_AR_etype(arr_ty));
          WN_array_base( sm_array ) =
              WN_Lda(Pointer_type, 0, str_mults);
          WN_array_index( sm_array, 0 ) =
              WN_Intconst(MTYPE_U8,i);
          WN_array_dim( sm_array, 0 ) =
              WN_Intconst(MTYPE_U8,rank);

          WN *sm_store = WN_Istore(MTYPE_U8, 0,
                  Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8),false),
                  sm_array,
                  WN_COPY_Tree(wn_str_m[i]));
          WN_INSERT_BlockLast( return_blk, sm_store);

          WN *extent_array = WN_Create( op_array, 3 );
          WN_element_size( extent_array ) =
              TY_size( TY_AR_etype(arr_ty));
          WN_array_base( extent_array ) =
              WN_Lda(Pointer_type, 0, extents);
          WN_array_index( extent_array, 0 ) =
              WN_Intconst(MTYPE_U8,i);
          WN_array_dim( extent_array, 0 ) =
              WN_Intconst(MTYPE_U8,rank);

          WN *sub = WN_kid(remote_access, 2*totalrank-i);
          INT ext;
          WN *wn_ext;
          /* TODO: handle other aggregate operators as well */
          if (WN_operator(sub) == OPR_TRIPLET) {
              ext = WN_const_val(WN_kid2(sub));
              wn_ext = WN_COPY_Tree(WN_kid2(sub));
          } else {
              ext = 1;
              wn_ext = WN_Intconst(Integer_type, 1);
          }

          WN *extent_store = WN_Istore(MTYPE_U8, 0,
                  Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8),false),
                   extent_array, wn_ext
                  /*WN_Intconst(Integer_type, ext)*/);
          WN_INSERT_BlockLast( return_blk, extent_store);

          if (strided_access) {
            WN *stride_array = WN_Create( op_array, 3 );
            WN_element_size( stride_array ) =
                TY_size( TY_AR_etype(arr_ty));
            WN_array_base( stride_array ) =
                WN_Lda(Pointer_type, 0, strides);
            WN_array_index( stride_array, 0 ) =
                WN_Intconst(MTYPE_U8,i);
            WN_array_dim( stride_array, 0 ) =
                WN_Intconst(MTYPE_U8,rank);

            WN *wn_str;
            /* TODO: handle other aggregate operators as well */
            if (WN_operator(sub) == OPR_TRIPLET) {
                wn_str = WN_COPY_Tree(WN_kid1(sub));
            } else {
                wn_str = WN_Intconst(Integer_type, 1);
            }

            WN *stride_store = WN_Istore(MTYPE_U8, 0,
                    Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8),false),
                     stride_array, wn_str );
            WN_INSERT_BlockLast( return_blk, stride_store);
          }

        }

        TY_IDX local_arr_ty;
        ST *local_strides;
        ST *local_str_mults;
        ST *local_extents;
        OPCODE op_local_array;
        if (access == READ_DIRECT || access == WRITE_DIRECT) {
          local_arr_ty = create_arr1_type(MTYPE_U8, local_rank );
          local_str_mults = Gen_Temp_Named_Symbol(local_arr_ty,
                                                "local_str_mults", CLASS_VAR,
                                                SCLASS_AUTO);
          Set_ST_is_temp_var(local_str_mults);
          local_extents = Gen_Temp_Named_Symbol(local_arr_ty,
                                                "local_extents", CLASS_VAR,
                                                 SCLASS_AUTO);
          Set_ST_is_temp_var(local_extents);

          if (local_strided_access) {
            local_strides = Gen_Temp_Named_Symbol( local_arr_ty, "local_strides",
                                CLASS_VAR, SCLASS_AUTO);
            Set_ST_is_temp_var(local_strides);
          }


          op_local_array = OPCODE_make_op( OPR_ARRAY, Pointer_type, MTYPE_V );
          for (INT8 i = 0; i  < local_rank; i++) {
            WN *local_sm_array = WN_Create( op_local_array, 3 );
            WN_element_size( local_sm_array ) =
              TY_size( TY_AR_etype(local_arr_ty));
            WN_array_base( local_sm_array ) =
              WN_Lda(Pointer_type, 0, local_str_mults);
            WN_array_index( local_sm_array, 0 ) =
              WN_Intconst(MTYPE_U8,i);
            WN_array_dim( local_sm_array, 0 ) =
              WN_Intconst(MTYPE_U8,local_rank);

            WN *sm_store = WN_Istore(MTYPE_U8, 0,
                Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8),false),
                local_sm_array,
                WN_COPY_Tree(local_wn_str_m[i]));
            WN_INSERT_BlockLast( return_blk, sm_store);

            WN *local_extent_array = WN_Create( op_local_array, 3 );
            WN_element_size( local_extent_array ) =
              TY_size( TY_AR_etype(local_arr_ty));
            WN_array_base( local_extent_array ) =
              WN_Lda(Pointer_type, 0, local_extents);
            WN_array_index( local_extent_array, 0 ) =
              WN_Intconst(MTYPE_U8,i);
            WN_array_dim( local_extent_array, 0 ) =
              WN_Intconst(MTYPE_U8,local_rank);

            WN *sub = WN_kid(local_access, 2*local_rank-i);
            INT ext;
            WN *wn_ext;
            /* TODO: handle other aggregate operators as well */
            if (WN_operator(sub) == OPR_TRIPLET) {
              ext = WN_const_val(WN_kid2(sub));
              wn_ext = WN_COPY_Tree(WN_kid2(sub));
            } else {
              ext = 1;
              wn_ext = WN_Intconst(Integer_type, 1);
            }

            WN *extent_store = WN_Istore(MTYPE_U8, 0,
                Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8),false),
                local_extent_array, wn_ext
                /*WN_Intconst(Integer_type, ext)*/);
            WN_INSERT_BlockLast( return_blk, extent_store);

            if (local_strided_access) {
              WN *local_stride_array = WN_Create( op_local_array, 3 );
              WN_element_size( local_stride_array ) =
                  TY_size( TY_AR_etype(local_arr_ty));
              WN_array_base( local_stride_array ) =
                  WN_Lda(Pointer_type, 0, local_strides);
              WN_array_index( local_stride_array, 0 ) =
                  WN_Intconst(MTYPE_U8,i);
              WN_array_dim( local_stride_array, 0 ) =
                  WN_Intconst(MTYPE_U8,local_rank);

              WN *wn_str;
              /* TODO: handle other aggregate operators as well */
              if (WN_operator(sub) == OPR_TRIPLET) {
                  wn_str = WN_COPY_Tree(WN_kid1(sub));
              } else {
                  wn_str = WN_Intconst(Integer_type, 1);
              }

              WN *stride_store = WN_Istore(MTYPE_U8, 0,
                      Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8),false),
                       local_stride_array, wn_str );
              WN_INSERT_BlockLast( return_blk, stride_store);
            }
          }
        }


        if (access == READ_TO_LCB) {
            call = Generate_Call_coarray_read_src_str(coarray_wn, temp_wn,
                    WN_Intconst(Integer_type, rank),
                    WN_Lda(Pointer_type, 0, str_mults),
                    WN_Lda(Pointer_type, 0, extents),
                    strided_access ? WN_Lda(Pointer_type, 0, strides) :
                                     WN_Intconst(Pointer_type, 0),
                    image);
        } else if (access == WRITE_FROM_LCB) {
            call = Generate_Call_coarray_write_dest_str(coarray_wn, temp_wn,
                    WN_Intconst(Integer_type, rank),
                    WN_Lda(Pointer_type, 0, str_mults),
                    WN_Lda(Pointer_type, 0, extents),
                    strided_access ? WN_Lda(Pointer_type, 0, strides) :
                                     WN_Intconst(Pointer_type, 0),
                    image);
        } else if (access == READ_DIRECT) {
            call = Generate_Call_coarray_read_full_str(coarray_wn, local_wn,
                    WN_Intconst(Integer_type, rank),
                    WN_Lda(Pointer_type, 0, str_mults),
                    WN_Lda(Pointer_type, 0, extents),
                    strided_access ? WN_Lda(Pointer_type, 0, strides) :
                                     WN_Intconst(Pointer_type, 0),
                    WN_Intconst(Integer_type, local_rank),
                    WN_Lda(Pointer_type, 0, local_str_mults),
                    WN_Lda(Pointer_type, 0, local_extents),
                    local_strided_access ?
                          WN_Lda(Pointer_type, 0, local_strides) :
                          WN_Intconst(Pointer_type, 0),
                    image);
        } else if (access == WRITE_DIRECT) {
            call = Generate_Call_coarray_write_full_str(coarray_wn, local_wn,
                    WN_Intconst(Integer_type, rank),
                    WN_Lda(Pointer_type, 0, str_mults),
                    WN_Lda(Pointer_type, 0, extents),
                    strided_access ? WN_Lda(Pointer_type, 0, strides) :
                                     WN_Intconst(Pointer_type, 0),
                    WN_Intconst(Integer_type, local_rank),
                    WN_Lda(Pointer_type, 0, local_str_mults),
                    WN_Lda(Pointer_type, 0, local_extents),
                    local_strided_access ?
                          WN_Lda(Pointer_type, 0, local_strides) :
                          WN_Intconst(Pointer_type, 0),
                    image);
        }
        WN_INSERT_BlockLast( return_blk, call);
    }

    for (INT8 i = 0; i < local_rank; i++) {
      WN_Delete(local_wn_str_m[i]);
    }
    for (INT8 i = 0; i < rank; i++) {
      WN_Delete(wn_str_m[i]);
    }
    for (INT8 i = 0; i < corank; i++) {
      WN_Delete(wn_costr_m[i]);
    }

    free(local_wn_str_m);
    free(wn_str_m);
    free(wn_costr_m);

    return return_blk;
} /* gen_coarray_access_stmt */


/*
 * substitute_lcb:
 *
 *   replaces a remote access reference with dereferenced or indexed reference
 *   to a temporary local coarray buffer.
 *
 *   returns replaced node in replace_wn. Also may modify wn_arrayexp if its
 *   an OPR_ARRSECTION operations.
 *
 */
static void substitute_lcb(WN *remote_access, ST *lcbtemp, WN *wn_arrayexp,
                                 WN **replace_wn)
{
    TY_IDX ty1, ty2;
    ST *coarray_st = WN_st(WN_kid0(remote_access));
    ty1 = get_array_type(coarray_st);
    if ( is_dope(ty1) )
        ty1 = TY_pointed(FLD_type(TY_fld(ty1)));

    TY_IDX elem_type = Ty_Table[ty1].u2.etype;
    INT8 rank = get_coarray_rank(ty1);
    INT8 corank = get_coarray_corank(ty1);
    INT8 totalrank = rank +  corank;

    /* substitute lcbtemp into original remote_access node.
     * TODO: assuming OPR_ARRAY for simplicity right now ... */

    if (WN_operator(remote_access) == OPR_ARRAY) {
        *replace_wn = WN_Ldid(Pointer_type, 0, lcbtemp,
                            MTYPE_To_TY(Pointer_type));
    } else if (WN_operator(remote_access) == OPR_ARRSECTION) {
        /* TODO: this won't work */

        *replace_wn  = WN_Create( OPCODE_make_op(OPR_ARRSECTION,
                    WN_rtype(remote_access),
                    WN_desc(remote_access)), 1+2*rank);

        /* assume contiguous access in LCB */
        WN_element_size(*replace_wn) = abs(WN_element_size(remote_access));

        WN_array_base(*replace_wn) = WN_Ldid(Pointer_type, 0, lcbtemp,
                                           MTYPE_To_TY(Pointer_type));

        /* set sizes and index for each dimension of tmp buff */
        INT8 j = 1;
        for (INT8 i = 1; i <= rank; i++) {
            WN *wn_ext;
            /* TODO: handle other aggregate operators as well */
            if (WN_operator(WN_array_index(remote_access, corank+i-1)) == OPR_TRIPLET) {
              wn_ext = WN_COPY_Tree(WN_kid2(WN_array_index(remote_access,corank+i-1)));
            } else  {
              wn_ext = WN_Intconst(Integer_type, 1);
            }

            WN_array_dim(*replace_wn,i-1) = wn_ext;

            /* set subscript for dimension i */
            WN_array_index(*replace_wn,i-1) =
                WN_COPY_Tree(WN_array_index(remote_access, corank+i-1));

            if (WN_operator(WN_array_index(*replace_wn,i-1)) == OPR_TRIPLET) {
                WN_Delete(WN_kid0(WN_array_index(*replace_wn,i-1)));
                WN_kid0(WN_array_index(*replace_wn,i-1)) =
                    WN_Intconst(Integer_type, 0);
                WN_Delete(WN_kid1(WN_array_index(*replace_wn,i-1)));
                WN_kid1(WN_array_index(*replace_wn,i-1)) =
                    WN_Intconst(Integer_type, 1);
                WN_Delete(WN_kid2(WN_array_index(*replace_wn,i-1)));
                WN_kid2(WN_array_index(*replace_wn,i-1)) =
                    WN_COPY_Tree(WN_array_dim(*replace_wn, i-1));

                /* adjust enclosing arrayexp node */
                /*
                WN_Delete(WN_kid( wn_arrayexp, j));
                WN_kid( wn_arrayexp, j) =
                    WN_COPY_Tree(WN_array_dim(*replace_wn, i-1));
                    */

                j++;
            } else {
                WN_Delete(WN_array_index(*replace_wn,i-1));
                WN_array_index(*replace_wn,i-1) =
                    WN_Intconst(Integer_type,0);
            }
        }

    }

} /* substitute_lcb */



/*
 * is_contiguous_access : checks if an array access operation is accessing a
 * contiguous slice of the array.
 *
 * Algorithm:
 * requires_complete_access = FALSE
 * for d = last dim to first dim
 *    if access_range(d) < extent(d) && requires_complete_access
 *       return false
 *    else if access_range(d) == 1
 *       continue
 *    else if access_range(d) > 1
 *       requires_complete_access = TRUE
 *    fi
 *
 * return TRUE
 *
 */
static BOOL is_contiguous_access(WN *remote_access)
{
    TY_IDX ty1, ty2;
    FmtAssert(WN_operator(remote_access) != OPR_ARRAY ||
            WN_operator(remote_access) != OPR_ARRSECTION,
            ("Unexpected operator for remote coarray access"));

    ST *coarray_st = WN_st(WN_kid0(remote_access));
    ty1 = get_array_type(coarray_st);
    if ( is_dope(ty1) )
        ty1 = TY_pointed(FLD_type(TY_fld(ty1)));

    TY_IDX elem_type = Ty_Table[ty1].u2.etype;
    INT8 rank = get_coarray_rank(ty1);
    INT8 corank = get_coarray_corank(ty1);
    INT8 totalrank = rank +  corank;

    INT* access_range = (INT *)malloc(rank*sizeof(INT));
    INT* extent = (INT *)malloc(rank*sizeof(INT));

    BOOL requires_complete_access = FALSE;

    if (WN_operator(remote_access) == OPR_ARRSECTION) {
        for (INT8 i = rank-1; i >= 0; i--) {
            WN *sub = WN_kid(remote_access, 2*totalrank-i);
            /* TODO: handle other aggregate operators as well */
            if (WN_operator(sub) == OPR_TRIPLET) {
                /* no static bounds, assume non-contiguous */
                if (WN_operator(WN_kid2(sub)) != OPR_INTCONST ||
                    WN_operator(WN_kid1(sub)) != OPR_INTCONST)
                    return FALSE;
                /*
                FmtAssert(WN_operator(WN_kid2(sub)) == OPR_INTCONST,
                        ("Expected extent in coarray subscript to be constant"));
                FmtAssert(WN_operator(WN_kid1(sub)) == OPR_INTCONST &&
                        WN_const_val(WN_kid1(sub)) == 1,
                        ("Expected stride in coarray subscript to be 1"));
                        */
                access_range[i] = WN_const_val(WN_kid2(sub));
            } else {
                access_range[i] = 1;
            }

            /* calculate extents */
            WN *size = WN_kid(remote_access, totalrank-i);
            if (WN_operator(size) == OPR_INTCONST) {
                extent[i] = WN_const_val(size);
            } else {
                /* assume non-contiguous if array shape is not statically
                 * known */
                return FALSE;
            }

            if ((access_range[i] < extent[i]) && requires_complete_access)
                return FALSE;
            else if (access_range[i] > 1)  {
                requires_complete_access = TRUE;
            }
            else continue;
        }
    }

    free(access_range);
    free(extent);

    return TRUE;
}

static TY_IDX create_arr1_type(TYPE_ID elem_type, INT16 ne)
{
    TY_IDX arr_ty = TY_IDX_ZERO;
    TY_IDX elem_ty_idx= Be_Type_Tbl(elem_type);
    TY& ty = New_TY (arr_ty);
    TY_Init (ty, TY_size(elem_ty_idx)*ne, KIND_ARRAY, MTYPE_UNKNOWN,
            Save_Str2i("arrtype.", TY_name(elem_type), ne ));
    Set_TY_etype(ty, Be_Type_Tbl(elem_type));

    ARB_HANDLE arb = New_ARB ();
    ARB_Init (arb, 0, ne-1, TY_size(elem_ty_idx));
    Set_ARB_dimension(arb, 1);
    Set_ARB_first_dimen(arb);
    Set_ARB_last_dimen(arb);
    Set_ARB_const_lbnd(arb);
    Set_ARB_const_ubnd(arb);
    Set_ARB_const_stride(arb);

    Set_TY_arb(ty, arb);
    Set_TY_align (arr_ty, TY_size(elem_ty_idx));

    return arr_ty;
}

/*
 * stmt_rhs_is_addressable
 *
 * Returns TRUE if the RHS is identified as addressable. This includes:
 *     - LDID  (loading of a scalar value)
 *     - ILOAD (derefereced pointer, or loading of an array element
 *     - ARRAYEXP where kid0 is ILOAD
 *
 * TODO: For simplicity, currently only handles 3rd case from above.
 *       What about non-unit stride?
 */
static BOOL stmt_rhs_is_addressable(WN *stmt_node)
{
  WN *rhs;
  Is_True(stmt_node && WN_kid0(stmt_node),
          ("stmt_node not a value statement with kid 0"));

  rhs = WN_kid0(stmt_node);

  ///////// DEBUG ////////////////
  //fdump_tree(stdout, stmt_node);
  ////////////////////////////////

  return (WN_operator(rhs) == OPR_ARRAYEXP &&
          WN_operator(WN_kid0(rhs)) == OPR_ILOAD);
}

static BOOL array_ref_is_coindexed(WN *arr)
{
  ST *array_st;
  TY_IDX ty;
  INT8 rank, corank;

  if ((WN_operator(arr) != OPR_ARRAY) &&
      (WN_operator(arr) != OPR_ARRSECTION))
    return 0;

  array_st = WN_st(WN_kid0(arr));
  ty = get_array_type(array_st);
  if ( is_dope(ty) ) {
    ty = TY_pointed(FLD_type(TY_fld(ty)));
    if (!is_coarray_type(ty))
      return 0;
    rank = get_coarray_rank(ty);
    corank = get_coarray_corank(ty);
  } else  {
    /* break if not coarray */
    ty = get_array_type(array_st);
    if (!is_coarray_type(ty))
      return 0;

    rank = get_coarray_rank(ty);
    corank = get_coarray_corank(ty);
  }

  /* break if not cosubscripted */
  if (WN_kid_count(arr) == (1+2*rank))
    return 0;

  return 1;
}


/*
 * adds the <stmt, blk> pair to the delete list
 *   return 1 if stmt was not already in delete list
 *   return 0 if stmt was already in delete list (no add)
 */
static int add_caf_stmt_to_delete_list(WN *stmt, WN *blk)
{
  int i;
  CAF_STMT_NODE stmt_to_delete;
  int ok_to_delete = 1;

  /* check if statement is already in the delete list */
  for (i = 0; i != caf_delete_list.size(); ++i) {
    if (caf_delete_list[i].stmt == stmt) {
      ok_to_delete = 0;
      break;
    }
  }

  if (ok_to_delete) {
    stmt_to_delete.stmt = stmt;
    stmt_to_delete.blk = blk;
    caf_delete_list.push_back(stmt_to_delete);
    return 1;
  } else {
    return 0;
  }
}

static void delete_caf_stmts_in_delete_list()
{
  for (int i = 0; i != caf_delete_list.size(); ++i) {
    CAF_STMT_NODE stmt_to_delete = caf_delete_list[i];
    Is_True(stmt_to_delete.stmt && stmt_to_delete.blk,
        ("invalid CAF_STMT_NODE object in caf_delete_list"));
    WN *stmt = WN_EXTRACT_FromBlock(stmt_to_delete.blk, stmt_to_delete.stmt);
    WN_DELETE_Tree(stmt);
  }

  caf_delete_list.clear();
}



/* routines for generating specific runtime calls
 */

static WN *
Generate_Call_acquire_lcb(WN *xfer_size, WN *lcb_ptr)
{
    WN *call = Generate_Call_Shell( ACQUIRE_LCB, MTYPE_V, 2);
    WN_actual( call, 0 ) =
        Generate_Param( xfer_size, WN_PARM_BY_VALUE);
    WN_actual( call, 1 ) =
        Generate_Param( lcb_ptr, WN_PARM_BY_REFERENCE);

    return call;
}

static WN *
Generate_Call_release_lcb(WN *lcb_ptr)
{
    WN *call = Generate_Call_Shell( RELEASE_LCB, MTYPE_V, 1);
    WN_actual( call, 0 ) =
        Generate_Param( lcb_ptr, WN_PARM_BY_REFERENCE);

    return call;
}

static WN *
Generate_Call_coarray_read(WN *coarray, WN *lcb_ptr, WN *xfer_size,
                            WN *image)
{
    WN *call = Generate_Call_Shell( COARRAY_READ, MTYPE_V, 4);
    WN_actual( call, 0 ) =
        Generate_Param( coarray, WN_PARM_BY_REFERENCE);
    WN_actual( call, 1 ) =
        Generate_Param( lcb_ptr, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( xfer_size, WN_PARM_BY_VALUE);
    WN_actual( call, 3 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);

    return call;
}

static WN *
Generate_Call_coarray_read_src_str(WN *coarray, WN *lcb_ptr, WN *ndim,
                                    WN *str_mults, WN *extents, WN *strides,
                                    WN *image)
{
    WN *call = Generate_Call_Shell( COARRAY_READ_SRC_STR, MTYPE_V, 7);
    WN_actual( call, 0 ) =
        Generate_Param( coarray, WN_PARM_BY_REFERENCE);
    WN_actual( call, 1 ) =
        Generate_Param( lcb_ptr, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( ndim, WN_PARM_BY_VALUE);
    WN_actual( call, 3 ) =
        Generate_Param( str_mults, WN_PARM_BY_REFERENCE);
    WN_actual( call, 4 ) =
        Generate_Param( extents, WN_PARM_BY_REFERENCE);
    WN_actual( call, 5 ) =
        Generate_Param( strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 6 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);

    return call;
}

static WN * Generate_Call_coarray_read_full_str(WN *coarray, WN *local,
                                    WN *src_ndim, WN *src_str_mults,
                                    WN *src_extents, WN *src_strides,
                                    WN *dest_ndim, WN *dest_str_mults,
                                    WN *dest_extents, WN *dest_strides,
                                    WN *image)
{
    WN *call = Generate_Call_Shell( COARRAY_READ_FULL_STR, MTYPE_V, 11);
    WN_actual( call, 0 ) =
        Generate_Param( coarray, WN_PARM_BY_REFERENCE);
    WN_actual( call, 1 ) =
        Generate_Param( local, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( src_ndim, WN_PARM_BY_VALUE);
    WN_actual( call, 3 ) =
        Generate_Param( src_str_mults, WN_PARM_BY_REFERENCE);
    WN_actual( call, 4 ) =
        Generate_Param( src_extents, WN_PARM_BY_REFERENCE);
    WN_actual( call, 5 ) =
        Generate_Param( src_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 6 ) =
        Generate_Param( dest_ndim, WN_PARM_BY_VALUE);
    WN_actual( call, 7 ) =
        Generate_Param( dest_str_mults, WN_PARM_BY_REFERENCE);
    WN_actual( call, 8 ) =
        Generate_Param( dest_extents, WN_PARM_BY_REFERENCE);
    WN_actual( call, 9 ) =
        Generate_Param( dest_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 10 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);

    return call;
}

static WN *
Generate_Call_coarray_write(WN *coarray, WN *lcb_ptr, WN *xfer_size,
                            WN *image)
{
    WN *call = Generate_Call_Shell( COARRAY_WRITE, MTYPE_V, 4);
    WN_actual( call, 0 ) =
        Generate_Param( coarray, WN_PARM_BY_REFERENCE);
    WN_actual( call, 1 ) =
        Generate_Param( lcb_ptr, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( xfer_size, WN_PARM_BY_VALUE);
    WN_actual( call, 3 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);

    return call;
}

static WN *
Generate_Call_coarray_write_dest_str(WN *coarray, WN *lcb_ptr, WN *ndim,
                                    WN *str_mults, WN *extents, WN *strides,
                                    WN *image)
{
    WN *call = Generate_Call_Shell( COARRAY_WRITE_DEST_STR, MTYPE_V, 7);
    WN_actual( call, 0 ) =
        Generate_Param( coarray, WN_PARM_BY_REFERENCE);
    WN_actual( call, 1 ) =
        Generate_Param( lcb_ptr, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( ndim, WN_PARM_BY_VALUE);
    WN_actual( call, 3 ) =
        Generate_Param( str_mults, WN_PARM_BY_REFERENCE);
    WN_actual( call, 4 ) =
        Generate_Param( extents, WN_PARM_BY_REFERENCE);
    WN_actual( call, 5 ) =
        Generate_Param( strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 6 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);

    return call;
}

static WN * Generate_Call_coarray_write_full_str(WN *coarray, WN *local,
                                    WN *dest_ndim, WN *dest_str_mults,
                                    WN *dest_extents, WN *dest_strides,
                                    WN *src_ndim, WN *src_str_mults,
                                    WN *src_extents, WN *src_strides,
                                    WN *image)
{
    WN *call = Generate_Call_Shell( COARRAY_WRITE_FULL_STR, MTYPE_V, 11);
    WN_actual( call, 0 ) =
        Generate_Param( coarray, WN_PARM_BY_REFERENCE);
    WN_actual( call, 1 ) =
        Generate_Param( local, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( dest_ndim, WN_PARM_BY_VALUE);
    WN_actual( call, 3 ) =
        Generate_Param( dest_str_mults, WN_PARM_BY_REFERENCE);
    WN_actual( call, 4 ) =
        Generate_Param( dest_extents, WN_PARM_BY_REFERENCE);
    WN_actual( call, 5 ) =
        Generate_Param( dest_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 6 ) =
        Generate_Param( src_ndim, WN_PARM_BY_VALUE);
    WN_actual( call, 7 ) =
        Generate_Param( src_str_mults, WN_PARM_BY_REFERENCE);
    WN_actual( call, 8 ) =
        Generate_Param( src_extents, WN_PARM_BY_REFERENCE);
    WN_actual( call, 9 ) =
        Generate_Param( src_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 10 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);

    return call;
}
