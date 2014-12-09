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
#include "cxx_memory.h"
#include "wb_buffer.h"
#include "wb_carray.h"
#include "wb_browser.h"
#include "wb.h"
#include "targ_const.h"
#include "dra_export.h"
#include "be_symtab.h"
#include "f90_utils.h"
#include "limits.h"
#include "data_layout.h"

#include <vector>
#include <map>

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

typedef struct {
    int rank;
    int corank;
    BOOL first_strided_subscript;
    WN *array_ref;
    vector<WN *> stride_mult;
    vector<WN *> subscript;
    vector<WN *> subscript_stride;
} ARRAY_ACCESS_DESC;

typedef struct {
  ST *handle_st;
  BOOL deferred;
  WN *prior_stmt_dep; /* points to statement after which wait on
                         handle should be inserted if !deferred,
                         otherwise it is NULL -- currently not used */
} ACCESS_HDL;

typedef struct {
  BOOL is_cor;
  int  cor_depth;
} COR_INFO;


/***********************************************************************
 * Local variable definitions
 ***********************************************************************/

static BOOL caf_prelower_initialized = FALSE;
static MEM_POOL caf_pool;
static WN_MAP Caf_COR_Info_Map;
static WN_MAP Caf_Parent_Map;
static WN_MAP Caf_LCB_Map;
static WN_MAP Caf_Visited_Map;
static WN_MAP Caf_Coarray_Sync_Map;

static std::vector<CAF_STMT_NODE> caf_initializer_list;
static std::vector<CAF_STMT_NODE> caf_delete_list;
static std::map<ST *, ST *> save_coarray_symbol_map;
static std::map<ST *, ST *> save_target_symbol_map;
static std::map<ST *, ST *> auto_target_symbol_map;
static std::map<ST *, ST *> common_save_coarray_symbol_map;
static std::map<ST *, ST *> common_save_target_symbol_map;

static ST *this_image_st = NULL;
static ST *num_images_st = NULL;
static ST *log2_images_st = NULL;
static ST *rem_images_st = NULL;

static TY_IDX null_coarray_type;
static TY_IDX null_array_type;

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

#define Set_LCB_Stmt(wn) (WN_MAP_Set(Caf_LCB_Map, wn, (void*)  1))
#define Is_LCB_Stmt(wn) ((void *) WN_MAP_Get(Caf_LCB_Map, (WN*) wn))

#define Set_Visited(wn) (WN_MAP_Set(Caf_Visited_Map, wn, (void*)  1))
#define Was_Visited(wn) ((void *) WN_MAP_Get(Caf_Visited_Map, (WN*) wn))

#define Set_Coarray_Sync(wn, access_handle) \
        (WN_MAP_Set(Caf_Coarray_Sync_Map, wn, (ACCESS_HDL*) access_handle))
#define Coarray_Sync(wn) \
        ((ACCESS_HDL*) WN_MAP_Get(Caf_Coarray_Sync_Map, (WN*) wn))

#define NAME_IS( st, name ) \
        strlen( &Str_Table[(st)->u1.name_idx]) == strlen(name) \
        && !strncmp( &Str_Table[(st)->u1.name_idx], name, strlen(name))

static void Set_COR_Info(WN *wn, COR_INFO *info)
{
    int dat = info->is_cor;
    dat = dat << 16;
    dat = dat | info->cor_depth;
    WN_MAP_Set(Caf_COR_Info_Map, wn, (void *)dat);
}

static void Get_COR_Info(WN *wn, COR_INFO *info)
{
    int dat = (long int) WN_MAP_Get(Caf_COR_Info_Map, (WN *)wn);
    info->is_cor = dat >> 16;
    info->cor_depth = dat & 0xFFFF;
}


/***********************************************************************
 * Local function declarations
 ***********************************************************************/

static void Set_COR_Info(WN *wn, COR_INFO *info);
static void Get_COR_Info(WN *wn, COR_INFO *info);

static void unfold_nested_cors_in_block(WN *block);
static void unfold_nested_cors_in_stmt(WN *node, WN *stmt, WN *block, BOOL is_nested=FALSE);
static inline void set_and_update_cor_depth(WN *w, BOOL c, INT d);
static inline void update_cor_depth(WN *w);
static int find_kid_num(WN *parent, WN *kid);
static BOOL is_load_operation(WN *node);
static BOOL is_convert_operation(WN *node);
static void gen_auto_target_symbol(ST *sym);
static inline void gen_save_coarray_symbol(ST *sym);
static inline void gen_save_target_symbol(ST *sym);
static inline void gen_global_save_coarray_symbol(ST *sym);
static inline void gen_global_save_target_symbol(ST *sym);
static ST* gen_save_symm_symbol(ST *sym);
static WN* gen_array1_ref( OPCODE op_array, TY_IDX array_type,
                               ST *array_st, INT8 index, INT8 dim);
static ST *get_lcb_sym(WN *access);
static WN *get_lcb_assignment(WN *coarray_deref, INT num_codim,
                              ST *LCB_st);
static BOOL array_ref_on_LHS(WN *wn, TY_IDX *ty);
static BOOL array_ref_on_RHS(WN *wn, TY_IDX *ty);
static BOOL array_ref_in_parm(WN *wn, TY_IDX *ty);
static BOOL is_lvalue(WN *expr);
static WN* get_transfer_size(WN *arr_ref, TY_IDX elem_type);
static void replace_RHS_with_LCB( WN *stmt_node, ST *LCB_st);
static WN* get_enclosing_direct_arr_ref(WN *arr);
static WN* get_inner_arrsection(WN *arr);
static WN* get_innermost_array(WN *ref);
static TY_IDX get_assign_stmt_datatype(WN *stmt);
static WN* get_load_parent(WN *start);
static WN* get_store_parent(WN *start);
static TY_IDX get_array_type_from_tree(WN *array_ref,
                                       TY_IDX *arrsection_type = NULL);
static int subscript_is_strided(WN *array, INT8 i);
static WN* subscript_extent(WN *array, INT8 i);
static WN* subscript_stride(WN *array, INT8 i);
static int subscript_is_scalar(WN *array, INT8 i);
static inline WN* WN_array_subscript(WN *array, INT8 i);

static WN *find_outer_array(WN *start, WN *end);
static TY_IDX get_type_at_offset (TY_IDX ty, WN_OFFSET offset,
                                  BOOL stop_at_array = FALSE,
                                  BOOL stop_at_coarray = FALSE);
static void Coarray_Prelower_Init();
static void Parentize(WN *wn);
static void init_caf_extern_symbols();
static void handle_caf_call_stmts(WN_TREE_CONTAINER<PRE_ORDER>::iterator wipre,
                                  WN **wn_next_p);
static WN *expr_is_coindexed(WN *expr, WN **image, TY_IDX *coarray_type,
                             WN **direct_coarray_ref = NULL);
static void uncoindex_expr(WN *expr);
static WN *get_containing_arrayexp(WN *wn);
static BOOL is_dope(const TY_IDX tyi);
static BOOL currentpu_ismain();
static BOOL is_coarray_type(const TY_IDX tyi);
static void set_coarray_tsize(TY_IDX coarray_type);
static INT get_1darray_size(const TY_IDX tyi);
static INT get_coarray_rank(const TY_IDX tyi);
static INT get_array_rank(const TY_IDX tyi);
static INT get_coarray_corank(const TY_IDX tyi);
static TY_IDX get_array_type(const ST * array_st);
static BOOL is_contiguous_access(WN *remote_access, INT8 rank);
static BOOL is_vector_access(WN *remote_access);
static TY_IDX create_arr1_type(TYPE_ID elem_type, INT16 ne);
static BOOL is_assignment_stmt(WN *stmt);

static ST* gen_lcbptr_symbol(TY_IDX tyi, const char *rootname);

static WN* gen_coarray_access_stmt(WN *remote_access, WN *local_access,
                                   ST *lcbtemp, WN *xfer_size,
                                   ACCESS_TYPE access,
                                   ACCESS_HDL *access_handle = NULL,
                                   ST *image_idx_st = NULL);
static void substitute_lcb(WN *remote_access, ST *lcbtemp,
                           WN *wn_arrayexp, WN **replace_wn);

static BOOL stmt_rhs_is_addressable(WN *stmt_node);
static WN * array_ref_is_coindexed(WN *arr, TY_IDX ty);
static void array_ref_remove_cosubscripts(WN *arr_parent, WN *arr, TY_IDX ty);

static int add_caf_stmt_to_delete_list(WN *stmt, WN *blk);
static int stmt_in_delete_list(WN *stmt);
static void delete_caf_stmts_in_delete_list();

static int add_caf_stmt_to_initializer_list(WN *stmt, WN *blk);
static int stmt_in_initializer_list(WN *stmt);
static void move_stmts_from_initializer_list(WN *wn, WN *blk);

static WN* make_array_ref(ST *base);
static WN* make_array_ref(WN *base);

static WN* gen_local_image_condition(WN *image, WN *orig_stmt_copy, BOOL is_write);

static WN * Generate_Call_target_alloc(WN *, WN *);
static WN * Generate_Call_target_dealloc(WN *);
static WN * Generate_Call_acquire_lcb(WN *, WN *);
static WN * Generate_Call_release_lcb(WN *);
static WN * Generate_Call_coarray_wait(WN *, BOOL with_guard = FALSE);
static WN * Generate_Call_coarray_wait_all();
static WN * Generate_Call_coarray_nbread(WN *image, WN *src, WN *dest,
                                       WN *nbytes, WN *hdl);
static WN * Generate_Call_coarray_read(WN *image, WN *src, WN *dest,
                                       WN *nbytes);
static WN * Generate_Call_coarray_write_from_lcb(WN *image, WN *dest, WN *src,
                                                 WN *nbytes, WN *ordered,
                                                 WN *hdl);
static WN * Generate_Call_coarray_write(WN *image, WN *dest,
                                        WN *src, WN *nbytes,
                                        WN *ordered, WN *hdl);
static WN * Generate_Call_coarray_strided_nbread(WN *image, WN *src,
                                    WN *src_strides, WN *dest,
                                    WN *dest_strides, WN *count,
                                    WN *stride_levels, WN *hdl);
static WN * Generate_Call_coarray_strided_read(WN *image, WN *src,
                                    WN *src_strides, WN *dest,
                                    WN *dest_strides, WN *count,
                                    WN *stride_levels);
static WN * Generate_Call_coarray_strided_write_from_lcb(WN *image, WN *dest,
                                    WN *dest_strides, WN *src,
                                    WN *src_strides, WN *count,
                                    WN *stride_levels,
                                    WN *ordered,
                                    WN *hdl);
static WN * Generate_Call_coarray_strided_write(WN *image,
                                    WN *dest,
                                    WN *dest_strides, WN *src,
                                    WN *src_strides, WN *count,
                                    WN *stride_levels,
                                    WN *ordered,
                                    WN *hdl);


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
 * The Coarray_Prelower phase is a preprocessing pass which handles certain CAF
 * instrincs and also standardizes statements containing remote coarray
 * accesses such that both the left and right hand sides contain l-values.
 *
 * For example, a statement such as this:
 *      y(:) = a(:)[i] + a(:)[j]
 * becomes:
 *      lcb1(:) = a(:)[i]
 *      lcb2(:) = a(:)[j]
 *      y(:) = lcb1(:) + lcb2(:)
 *
 * As another example:
 *      a(:)[i] = y(:) + z(:)
 * becomes:
 *      lcb3(:) = y(:) + z(:)
 *      a(:)[i] = lcb3(:)
 *
 * The prelowering phase will generate temporary buffers, referred to as Local
 * Communication Buffers (LCBs) from which data may be transmitted or to which
 * data may be received. If a statement with a remote coarray access already
 * contains l-values on both the left and right side, then no LCB will be
 * created in order to avoid an unnecessary extra copy.
 *
 * A subsequent back-end phase should handle lowering co-subscripted array
 * references into 1-sided GET/PUT calls. However, for the time being we will
 * handle this translation right here.
 */

WN * Coarray_Prelower(PU_Info *current_pu, WN *pu)
{
    BOOL is_main = FALSE;
    static BOOL global_coarrays_processed = FALSE;

    WN *func_body, *func_exit_stmts;

    if (!caf_prelower_initialized) {
      Coarray_Prelower_Init();

      /* create NULL coarray and array types */
      null_coarray_type = create_arr1_type(MTYPE_V, 0);
      Set_TY_is_coarray(null_coarray_type);
      Set_ARB_dimension(TY_arb(null_coarray_type), 1);
      Set_ARB_codimension(TY_arb(null_coarray_type), 1);

      null_array_type = create_arr1_type(MTYPE_V, 0);

      caf_prelower_initialized = TRUE;
    }

    /* insert call to caf_init if this is the main PU */
    is_main  = currentpu_ismain();
    func_body = WN_func_body( pu );
    func_exit_stmts = WN_CreateBlock();

    /* create extern symbols _this_image and _num_images. Should be
     * initialized by runtime library
     */
    init_caf_extern_symbols();

    /* Create Parent Map for WHIRL tree */
    Caf_COR_Info_Map = WN_MAP_Create(&caf_pool);
    Caf_Parent_Map = WN_MAP_Create(&caf_pool);
    Caf_LCB_Map = WN_MAP_Create(&caf_pool);
    Caf_Visited_Map = WN_MAP_Create(&caf_pool);
    Caf_Coarray_Sync_Map = WN_MAP_Create(&caf_pool);

    Parentize(func_body);

    /* resize coarrays
     * TODO: This should be fixed in front-end, actually.
     *       not tested on deferred-size / allocatables.
     * */
    ST *sym;
    UINT32 i;
    FOREACH_SYMBOL(CURRENT_SYMTAB, sym, i) {
        if (sym->sym_class == CLASS_VAR && is_coarray_type(ST_type(sym))) {
            if (ST_sclass(sym) == SCLASS_PSTATIC) {
                set_coarray_tsize(ST_type(sym));
            }
        }
    }

    if (global_coarrays_processed == FALSE) {
        FOREACH_SYMBOL(GLOBAL_SYMTAB, sym, i) {
            if (sym->sym_class == CLASS_VAR &&
                is_coarray_type(ST_type(sym))) {
                set_coarray_tsize(ST_type(sym));
            }
        }
        global_coarrays_processed = TRUE;
    }

    WN_TREE_CONTAINER<PRE_ORDER> wcpre(func_body);
    WN_TREE_CONTAINER<PRE_ORDER> ::iterator wipre, curr_wipre=NULL, temp_wipre = NULL;

    for (wipre = wcpre.begin(); wipre != wcpre.end(); ++wipre) {
        ST *st1;
        TY_IDX ty1, ty2, ty3;
        TY_IDX coarray_type;
        TY_IDX elem_type;
        WN *replace_wn = NULL;
        WN *image;
        WN *coindexed_arr_ref;
        WN *direct_coarray_ref;
        WN *wn = wipre.Wn();
        WN *wn_next;

        switch (WN_operator(wn)) {
            case OPR_ARRAY:
            case OPR_ARRSECTION:

                coindexed_arr_ref =
                    expr_is_coindexed(wn, &image, &coarray_type,
                                      &direct_coarray_ref);

                if (image == NULL || is_vector_access(coindexed_arr_ref)
                        //|| Was_Visited(coindexed_arr_ref)
                    )
                    break;

                set_and_update_cor_depth(coindexed_arr_ref, TRUE, 1);

                /* skip ahead */
                while ( wipre.Wn() != direct_coarray_ref) wipre++;

                //Set_Visited(coindexed_arr_ref);

                break;
        }
    }

    unfold_nested_cors_in_block(func_body);


    /* Pass 1: Traverse PU, searching for:
     *   (1) CAF intrinsics (this_image, num_images, sync_images)
     *   (2) calls to STOP or _END, before which insert caf_finalize() call
     *   (3) co-subscripted array section references: "standardize" and, for now, go
     *       ahead and generate 1-sided GET/PUT calls
     */

    /* for support for character coarrays
     **/
    WN *lhs_ref_param_wn = NULL;
    WN *rhs_ref_param_wn = NULL;

    BOOL defer_sync_just_seen = FALSE;
    ST *handle_var = NULL;
    BOOL pragma_preamble_done = FALSE;
    BOOL do_loop_start_node = FALSE;
    WN *do_loop_stmt_node = NULL;
    WN *sync_blk = WN_CreateBlock();
    BOOL stmt_is_assignment = FALSE;

    for (wipre = wcpre.begin(); wipre != wcpre.end(); ++wipre) {
        WN *insert_blk;
        WN *wn = wipre.Wn();
        WN *wn_next;
        WN *insert_wnx;
        WN *insert_sync;
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
        TY_IDX coarray_type;
        TY_IDX elem_type;
        WN *replace_wn = NULL;
        WN *image;
        WN *coindexed_arr_ref;
        WN *direct_coarray_ref;

        parent = wipre.Get_parent_wn();

        /* if its a statement, set stmt_node and also set blk_node to
         * parent if the parent is a block */
        if ((OPCODE_is_stmt(WN_opcode(wn)) ||
            OPCODE_is_scf(WN_opcode(wn)) &&
             WN_operator(wn) != OPR_BLOCK &&
             WN_operator(wn) != OPR_FUNC_ENTRY)) {

          /* if sync_blk is not empty, then insert it sync before prior
           * stmt_node */
          if (sync_blk && WN_first(sync_blk)) {
              WN *sync_stmt = WN_first(sync_blk);
              while (sync_stmt) {
                  sync_stmt = WN_EXTRACT_FromBlock(sync_blk, sync_stmt);
                  if (!do_loop_start_node) {
                      WN_INSERT_BlockBefore(blk_node, stmt_node, sync_stmt);
                  } else {
                      WN_INSERT_BlockBefore(blk_node, do_loop_stmt_node,
                                            sync_stmt);
                  }
                  sync_stmt = WN_first(sync_blk);
              }
          }

          stmt_node = wn;
          stmt_is_assignment = is_assignment_stmt(stmt_node);

          wn_arrayexp = NULL;
          if (WN_operator(parent) == OPR_BLOCK) {
              blk_node = parent;
          } else if (WN_operator(parent) == OPR_DO_LOOP &&
              WN_start(parent) == stmt_node) {
              /* working on initialization statement of a do-loop. */
              do_loop_start_node = TRUE;
              do_loop_stmt_node = parent;
          } else if (WN_operator(parent) == OPR_DO_LOOP) {
              /* no longer working on initialization statement of do-loop */
              do_loop_start_node = FALSE;
          }
        }

        /* need to move static initializers to coarray data or F90 targets
         * past the call to caf_init(). */
        if (is_main && !pragma_preamble_done) {
            if (WN_operator(wn) == OPR_STID) {
                if ( is_coarray_type(ST_type(WN_st(wn))) ||
                     ST_is_f90_target(WN_st(wn)) ) {
                    add_caf_stmt_to_initializer_list(wn, func_body);
                }
            } else if (WN_operator(wn) == OPR_ISTORE ||
                    WN_operator(wn) == OPR_MSTORE) {
                WN *find_lda = WN_kid1(wn);
                while (find_lda && WN_operator(find_lda) != OPR_LDA) {
                    find_lda = WN_kid0(find_lda);
                }
                if ( find_lda &&
                     (is_coarray_type(ST_type(WN_st(find_lda))) ||
                      ST_is_f90_target(WN_st(find_lda))) ) {
                    add_caf_stmt_to_initializer_list(wn, func_body);
                }

            }
        }


        /* stores most recently encountered ARRAYEXP in wn_arrayexp */
        if (WN_operator(wn) == OPR_ARRAYEXP)
          wn_arrayexp = wn;

        switch (WN_operator(wn)) {
            case OPR_PRAGMA:
                if (WN_pragma(wn) == WN_PRAGMA_DEFER_SYNC) {
                    defer_sync_just_seen = TRUE;
                    handle_var = WN_st(wn);
                } else if (WN_pragma(wn) == WN_PRAGMA_SYNC) {
                    WN *hdl;
                    if (WN_st(wn) == NULL) {
                        insert_wnx = Generate_Call_coarray_wait_all();
                    } else {
                        hdl = WN_Lda(Pointer_type, 0, WN_st(wn));
                        insert_wnx = Generate_Call_coarray_wait(hdl, TRUE);
                    }
                    WN_INSERT_BlockAfter(blk_node, wn, insert_wnx);
                } else if (is_main && WN_pragma(wn) == WN_PRAGMA_PREAMBLE_END) {
                    pragma_preamble_done = TRUE;
                    insert_wnx = Generate_Call( CAF_INIT );
                    WN_INSERT_BlockAfter(blk_node, wn, insert_wnx);

                    /* move initializers for static coarrays here */
                    move_stmts_from_initializer_list(insert_wnx, blk_node);
                }
                break;
            case OPR_CALL:
                handle_caf_call_stmts(wipre, &wn_next);

                /* check for call to _END */
                if ( NAME_IS(WN_st(wn), "_END") ) {
                    insert_wnx = Generate_Call_Shell(CAF_FINALIZE, MTYPE_V, 1);
                    WN_actual(insert_wnx,0) =
                        Generate_Param( WN_Intconst(Integer_type, 0),
                                        WN_PARM_BY_VALUE);
                    WN_INSERT_BlockBefore(blk_node, wn, insert_wnx);
                }

                /* all library calls with implicit sync memory semantics
                 * should include an optimization  barrier to prevent unsafe
                 * optimization.:
                 *      SYNC ALL: forward and backward barrier
                 *      SYNC IMAGES: forward and backward barrier
                 *      SYNC MEMORY: forward and backward barrier
                 *      LOCK: backward barrier
                 *      UNLOCK: forward barrier
                 *      CRITICAL: backward barrier
                 *      END CRITICAL: forward barrier
                 */

                if ( NAME_IS(WN_st(wn), "_SYNC_ALL") ||
                     NAME_IS(WN_st(wn), "_SYNC_IMAGES") ||
                     NAME_IS(WN_st(wn), "_SYNC_MEMORY") ) {
                    WN_INSERT_BlockBefore(blk_node, wn,
                                          WN_CreateBarrier(TRUE,0));
                    WN_INSERT_BlockAfter(blk_node, wn,
                                          WN_CreateBarrier(FALSE,0));
                }

                if ( NAME_IS(WN_st(wn), "_COARRAY_LOCK") ||
                     NAME_IS(WN_st(wn), "_CRITICAL") ) {
                    WN_INSERT_BlockAfter(blk_node, wn,
                                          WN_CreateBarrier(FALSE,0));
                }

                if ( NAME_IS(WN_st(wn), "_COARRAY_UNLOCK") ||
                     NAME_IS(WN_st(wn), "_END_CRITICAL") ) {
                    WN_INSERT_BlockBefore(blk_node, wn,
                                          WN_CreateBarrier(TRUE,0));
                }

                break;
            case OPR_INTRINSIC_CALL:
                if (WN_opcode(wn) == OPC_VINTRINSIC_CALL) {
                   if (WN_intrinsic(wn) == INTRN_CASSIGNSTMT) {
                    /*for character coarrays support*/
                    lhs_ref_param_wn = WN_kid0(wn);
                    rhs_ref_param_wn = WN_kid1(wn);
                  }
                }

                break;

            case OPR_ARRAY:
            case OPR_ARRSECTION:

                coindexed_arr_ref = expr_is_coindexed(wn, &image, &coarray_type,
                                                      &direct_coarray_ref);
                if (image == NULL) break;

                /* co-indexed expressions with vector subscripts are handled
                 * in Coarray Lowering */
                if (is_vector_access(coindexed_arr_ref)) break;

                if (Was_Visited(coindexed_arr_ref)) break;

                if (defer_sync_just_seen) {
                    ACCESS_HDL *access_handle = (ACCESS_HDL*)malloc(
                            sizeof(ACCESS_HDL));
                    access_handle->handle_st = handle_var;
                    access_handle->deferred = TRUE;
                    Set_Coarray_Sync(stmt_node, (ACCESS_HDL*)access_handle);
                    handle_var = NULL;
                    defer_sync_just_seen = FALSE;
                }

                temp_wipre = wipre;
                while ( temp_wipre.Wn() != coindexed_arr_ref )
                    temp_wipre++;

                Set_Visited(coindexed_arr_ref);

                if ( stmt_is_assignment &&
                     array_ref_on_LHS(coindexed_arr_ref, &elem_type) ) {
                    /* coarray write ... */

                    if (elem_type == TY_IDX_ZERO) {
                        /* if elem_type being accessed still can't be resolved,
                         * just use the coarray etype */
                        elem_type = Ty_Table[coarray_type].u2.etype;
                    }
                    WN *RHS_wn = WN_kid0(stmt_node);
                    BOOL RHS_is_preg = FALSE;

                    if (WN_operator(RHS_wn) == OPR_LDID &&
                        ST_sclass(WN_st(RHS_wn)) == SCLASS_REG) {
                        RHS_is_preg = TRUE;
                    }

                    if ( is_lvalue(RHS_wn) && !is_convert_operation(RHS_wn) &&
                        !RHS_is_preg &&
                        (!get_inner_arrsection(coindexed_arr_ref) ||
                         get_inner_arrsection(RHS_wn)) &&
                          (is_vector_access(coindexed_arr_ref) ||
                           !is_vector_access(RHS_wn))) {

                        WN *if_local_wn = gen_local_image_condition(
                                                  image,
                                                  WN_COPY_Tree(stmt_node),
                                                  TRUE);


                        /* setup else block for if-local statement */
                        WN *new_stmt_node = WN_COPY_Tree(stmt_node);
                        Set_Coarray_Sync(new_stmt_node, Coarray_Sync(stmt_node));
                        WN_INSERT_BlockFirst( WN_else(if_local_wn),
                                              new_stmt_node);

                        WN_INSERT_BlockBefore(blk_node, stmt_node, if_local_wn);

                        /* defer deletion of the old statment so that we can continue
                         * to traverse the tree */
                        add_caf_stmt_to_delete_list(stmt_node, blk_node);


                    } else {
                        WN *new_stmt_node;
                        ST *LCB_st;
                        WN *xfer_sz_node;


                        WN *if_local_wn = gen_local_image_condition(
                                                image,
                                                WN_COPY_Tree(stmt_node),
                                                TRUE);


                        /* setup else block for storing to remote image ... */

                        /* create source LCB for co-indexed term */
                        LCB_st = gen_lcbptr_symbol(
                               Make_Pointer_Type(elem_type,FALSE),
                               "LCB" );
                        xfer_sz_node = get_transfer_size(coindexed_arr_ref,
                                                         elem_type);
                        insert_wnx = Generate_Call_acquire_lcb(
                                xfer_sz_node,
                                WN_Lda(Pointer_type, 0, LCB_st));
                        WN_INSERT_BlockFirst(WN_else(if_local_wn),
                                             insert_wnx);

                        /* create "normalized" assignment to remote coarray */
                        new_stmt_node = WN_COPY_Tree(stmt_node);
                        replace_RHS_with_LCB(new_stmt_node,
                                             LCB_st);

                        Set_LCB_Stmt(new_stmt_node);
                        Set_Coarray_Sync(new_stmt_node, Coarray_Sync(stmt_node));

                        WN_INSERT_BlockLast( WN_else(if_local_wn),
                                              new_stmt_node);

                        WN_INSERT_BlockAfter(blk_node, stmt_node, if_local_wn);

                        /* replace LHS */
                        WN *store = get_store_parent(coindexed_arr_ref);
                        if (store) WN_offset(store) = 0;
                        substitute_lcb(coindexed_arr_ref, LCB_st,
                                       wn_arrayexp, &replace_wn);
                        WN_Delete(coindexed_arr_ref);
                        temp_wipre.Replace(replace_wn);

                        /* move copy of original statment into else block */
                        WN_INSERT_BlockAfter( WN_else(if_local_wn),
                                              WN_first(WN_else(if_local_wn)),
                                              WN_COPY_Tree(stmt_node) );

                        wipre = temp_wipre;

                        /* defer deletion of the old statment so that we can continue
                         * to traverse the tree */
                        add_caf_stmt_to_delete_list(stmt_node, blk_node);
                    }

                } else if ( array_ref_on_RHS(coindexed_arr_ref, &elem_type) ) {
                    /* coarray read ... */

                    if (elem_type == TY_IDX_ZERO) {
                        /* if elem_type being accessed still can't be resolved,
                         * just use the coarray etype */
                        elem_type = Ty_Table[coarray_type].u2.etype;
                    }
                    WN *RHS_wn = WN_kid0(stmt_node);
                    WN *LHS_img;
                    TY_IDX LHS_coarray_type;
                    BOOL LHS_is_coindexed  = FALSE;
                    BOOL LHS_has_arrsection = FALSE;
                    BOOL LHS_is_vector_access = FALSE;
                    BOOL LHS_is_preg = FALSE;

                    if (WN_operator(stmt_node) == OPR_ISTORE ||
                        WN_operator(stmt_node) == OPR_MSTORE) {
                        WN *LHS_wn = WN_kid1(stmt_node);
                        if (expr_is_coindexed(LHS_wn, &LHS_img,
                            &LHS_coarray_type)) {
                            LHS_is_coindexed = TRUE;
                        }
                        LHS_has_arrsection = get_inner_arrsection(LHS_wn) != NULL;
                        LHS_is_vector_access = (BOOL) is_vector_access(LHS_wn);
                    } else if (WN_operator(stmt_node) == OPR_STID &&
                            ST_sclass(WN_st(stmt_node)) == SCLASS_REG) {
                        LHS_is_preg = TRUE;
                    }

                    BOOL coindexed_arr_is_RHS = stmt_is_assignment &&
                                               is_lvalue(RHS_wn) &&
                                               !is_convert_operation(RHS_wn) &&
                                               !do_loop_start_node;
                    WN *node = coindexed_arr_ref;

                    while (coindexed_arr_is_RHS && node != RHS_wn) {
                        node = Get_Parent(node);
                        if (WN_operator(node) == OPR_ARRAY ||
                                WN_operator(node) == OPR_ARRSECTION) {
                            coindexed_arr_is_RHS = FALSE;
                        }
                    }

                    if ( coindexed_arr_is_RHS && (!LHS_has_arrsection ||
                          get_inner_arrsection(coindexed_arr_ref)) &&
                         !LHS_is_coindexed &&
                         !LHS_is_preg &&
                         (is_vector_access(coindexed_arr_ref) ||
                          !LHS_is_vector_access) ) {
                        /* no LCB created */

                        WN *if_local_wn = gen_local_image_condition(
                                                  image,
                                                  WN_COPY_Tree(stmt_node),
                                                  FALSE);

                        /* if no sync handle specified for this read, then
                         * create a temporary one */
                        if (!Coarray_Sync(stmt_node)) {
                            ACCESS_HDL *access_handle = (ACCESS_HDL*)malloc(
                                    sizeof(ACCESS_HDL));
                            access_handle->handle_st =
                                Gen_Temp_Named_Symbol(MTYPE_To_TY(MTYPE_U8),
                                        "hdl", CLASS_VAR, SCLASS_AUTO);
                            access_handle->deferred = FALSE;
                            Set_ST_is_temp_var(access_handle->handle_st);
                            Set_Coarray_Sync(stmt_node, (ACCESS_HDL*)access_handle);
                        }

                        /* setup else block for if-local statement */
                        WN *new_stmt_node = WN_COPY_Tree(stmt_node);
                        Set_Coarray_Sync(new_stmt_node, Coarray_Sync(stmt_node));

                        WN_INSERT_BlockFirst( WN_else(if_local_wn),
                                new_stmt_node);

                        if (!do_loop_start_node) {
                            WN_INSERT_BlockBefore(blk_node, stmt_node,
                                                  if_local_wn);
                        } else {
                            WN_INSERT_BlockBefore(blk_node, do_loop_stmt_node,
                                                  if_local_wn);
                        }


                        /* insert sync for remote read if not deferred*/
                        if (Coarray_Sync(stmt_node)->deferred == FALSE) {
                            WN *sync_hdl;
                            ST *handle_st;
                            handle_st =
                                Coarray_Sync(stmt_node)->handle_st;
                            if (handle_st == NULL) {
                                Is_True(0, ("handle_st not set for non-deferred sync"));
                            } else {
                                sync_hdl = WN_Lda( Pointer_type, 0, handle_st);
                            }
                            insert_sync =
                                Generate_Call_coarray_wait(sync_hdl, TRUE);

                            WN *insert_sync_init = WN_Stid(Pointer_type, 0,
                                                    handle_st,
                                                    ST_type(handle_st),
                                                    WN_Intconst(Pointer_type,0));
                            WN_INSERT_BlockBefore(blk_node, if_local_wn,
                                                  insert_sync_init);
                            WN_INSERT_BlockLast(sync_blk, insert_sync);
                        } else {
                            /* for deferred sync, need to initialize the
                             * handle, if available, to 0 */
                            ST *handle_st;
                            handle_st =
                                Coarray_Sync(stmt_node)->handle_st;
                            if (handle_st != NULL) {
                                WN *insert_sync_init = WN_Stid(Pointer_type, 0,
                                        handle_st,
                                        ST_type(handle_st),
                                        WN_Intconst(Pointer_type,0));
                                WN_INSERT_BlockBefore(blk_node, if_local_wn,
                                        insert_sync_init);
                            }
                        }


                        /* defer deletion of the old statment so that we can continue
                         * to traverse the tree */
                        add_caf_stmt_to_delete_list(stmt_node, blk_node);
                    } else {
                        /* create destination LCB for co-indexed term */

                        WN *new_stmt_node;
                        ST *LCB_st;
                        WN *xfer_sz_node;
                        INT num_codim;

                        /* create LCB for coindexed array ref */
                        LCB_st = gen_lcbptr_symbol(
                               Make_Pointer_Type(elem_type,FALSE),
                               "LCB" );
                        xfer_sz_node = get_transfer_size(coindexed_arr_ref,
                                                         elem_type);
                        insert_wnx = Generate_Call_acquire_lcb(
                                xfer_sz_node,
                                WN_Lda(Pointer_type, 0, LCB_st));

                        if (!do_loop_start_node) {
                            WN_INSERT_BlockBefore(blk_node, stmt_node,
                                                  insert_wnx);
                        } else {
                            WN_INSERT_BlockBefore(blk_node, do_loop_stmt_node,
                                                  insert_wnx);
                        }

                        /* create "normalized" assignment from remote coarray */
                        num_codim = coindexed_arr_ref == direct_coarray_ref ?
                                    get_coarray_corank(coarray_type) : 0;
                        new_stmt_node = get_lcb_assignment(
                                WN_COPY_Tree(Get_Parent(wn)),
                                num_codim,
                                LCB_st);

                        WN *if_local_wn = gen_local_image_condition(
                                                image,
                                                WN_COPY_Tree(new_stmt_node),
                                                FALSE);

                        WN_INSERT_BlockFirst( WN_else(if_local_wn),
                                              new_stmt_node);

                        if (!do_loop_start_node) {
                            WN_INSERT_BlockBefore(blk_node, stmt_node,
                                                  if_local_wn);
                        } else {
                            WN_INSERT_BlockBefore(blk_node, do_loop_stmt_node,
                                                  if_local_wn);
                        }

                        /* if no sync handle specified for this read, then
                         * create a temporary one */
                        if (!Coarray_Sync(stmt_node)) {
                            ACCESS_HDL *access_handle = (ACCESS_HDL*)malloc(
                                    sizeof(ACCESS_HDL));
                            access_handle->handle_st =
                                Gen_Temp_Named_Symbol(MTYPE_To_TY(MTYPE_U8),
                                        "hdl", CLASS_VAR, SCLASS_AUTO);
                            access_handle->deferred = FALSE;
                            Set_ST_is_temp_var(access_handle->handle_st);
                            Set_Coarray_Sync(new_stmt_node, (ACCESS_HDL*)access_handle);
                        } else {
                            Set_Coarray_Sync(new_stmt_node, Coarray_Sync(stmt_node));
                        }


                        /* replace term in RHS */
                        WN *load = get_load_parent(coindexed_arr_ref);
                        if (load) WN_offset(load) = 0;
                        substitute_lcb(coindexed_arr_ref, LCB_st,
                                       wn_arrayexp, &replace_wn);
                        WN_Delete(coindexed_arr_ref);
                        temp_wipre.Replace(replace_wn);
                        wipre = temp_wipre;

                        /* insert sync for remote read if not deferred*/
                        if (Coarray_Sync(new_stmt_node)->deferred == FALSE) {
                            WN *sync_hdl;
                            ST *handle_st;
                            handle_st =
                                Coarray_Sync(new_stmt_node)->handle_st;
                            if (handle_st == NULL) {
                                Is_True(0, ("handle_st not set for non-deferred sync"));
                            } else {
                                sync_hdl = WN_Lda( Pointer_type, 0, handle_st );
                            }
                            insert_sync =
                                Generate_Call_coarray_wait(sync_hdl, TRUE);

                            WN *insert_sync_init =
                                            WN_Stid(Pointer_type, 0,
                                                    handle_st,
                                                    ST_type(handle_st),
                                                    WN_Intconst(Pointer_type,0));
                            WN_INSERT_BlockBefore(blk_node, if_local_wn, insert_sync_init);
                            WN_INSERT_BlockLast(sync_blk, insert_sync);
                        } else {
                            /* for deferred sync, need to initialize the
                             * handle, if available, to 0 */
                            ST *handle_st;
                            handle_st =
                                Coarray_Sync(stmt_node)->handle_st;
                            if (handle_st != NULL) {
                                WN *insert_sync_init = WN_Stid(Pointer_type, 0,
                                        handle_st,
                                        ST_type(handle_st),
                                        WN_Intconst(Pointer_type,0));
                                WN_INSERT_BlockBefore(blk_node, if_local_wn,
                                        insert_sync_init);
                            }
                        }

                        /* call to release LCB */
                        insert_wnx = Generate_Call_release_lcb(
                                WN_Lda(Pointer_type, 0, LCB_st));

                        if (!do_loop_start_node) {
                            WN_INSERT_BlockAfter(blk_node, stmt_node,
                                                 insert_wnx);
                        } else {
                            WN_INSERT_BlockAfter(blk_node, do_loop_stmt_node,
                                                 insert_wnx);
                        }

                    }

                } else if ( array_ref_in_parm(coindexed_arr_ref, &elem_type) ) {
                    WN *new_stmt_node;
                    ST *LCB_st;
                    WN *xfer_sz_node;

                    //elem_type = Ty_Table[coarray_type].u2.etype;

                    /* create LCB for coindexed array ref */
                    LCB_st = gen_lcbptr_symbol(
                            Make_Pointer_Type(elem_type,FALSE),
                            "LCB" );
                    xfer_sz_node = get_transfer_size(coindexed_arr_ref,
                            elem_type);
                    insert_wnx = Generate_Call_acquire_lcb(
                            xfer_sz_node,
                            WN_Lda(Pointer_type, 0, LCB_st));
                    WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);

                    if (Get_Parent(coindexed_arr_ref) == rhs_ref_param_wn) {
                        /*coarray read*/

                        /* if no sync handle specified for this read statement, then
                         * create a temporary one */
                        if (!Coarray_Sync(stmt_node)) {
                            ACCESS_HDL *access_handle = (ACCESS_HDL*)malloc(
                                    sizeof(ACCESS_HDL));
                            access_handle->handle_st =
                                Gen_Temp_Named_Symbol(MTYPE_To_TY(MTYPE_U8),
                                        "hdl", CLASS_VAR, SCLASS_AUTO);
                            access_handle->deferred = FALSE;
                            Set_ST_is_temp_var(access_handle->handle_st);
                            Set_Coarray_Sync(stmt_node, (ACCESS_HDL*)access_handle);
                        }

                        insert_blk = gen_coarray_access_stmt( wn,
                                NULL, LCB_st, xfer_sz_node, READ_TO_LCB,
                                Coarray_Sync(stmt_node));

                        /* insert sync for remote reads if not deferred*/
                        if (Coarray_Sync(stmt_node)) {
                            if (Coarray_Sync(stmt_node)->deferred == FALSE) {
                                WN *sync_hdl;
                                ST *handle_st;
                                handle_st =
                                    Coarray_Sync(stmt_node)->handle_st;
                                if (handle_st == NULL) {
                                    sync_hdl = WN_Intconst(Pointer_type,0);
                                } else {
                                    sync_hdl = WN_Lda( Pointer_type, 0, handle_st );
                                }
                                insert_sync =
                                    Generate_Call_coarray_wait(sync_hdl, TRUE);
                                WN_INSERT_BlockLast(insert_blk, insert_sync);
                            }

                            free(Coarray_Sync(stmt_node));
                        } else {
                            /* should not reach */

                            Is_True( 0,
                            ("Coarray_Sync not created for coarray read statement"));
                            insert_sync = Generate_Call_coarray_wait_all();
                            WN_INSERT_BlockLast(insert_blk, insert_sync);
                        }

                        /* get LDID node in replace_wn that substitues wn
                         * pointed to by wipre*/
                        WN *load = get_load_parent(coindexed_arr_ref);
                        if (load) WN_offset(load) = 0;
                        substitute_lcb(coindexed_arr_ref, LCB_st,
                                       wn_arrayexp, &replace_wn);
                        WN_Delete(coindexed_arr_ref);
                        temp_wipre.Replace(replace_wn);
                        wipre = temp_wipre;

                        insert_wnx = WN_first(insert_blk);
                        while (insert_wnx) {
                            insert_wnx = WN_EXTRACT_FromBlock(insert_blk,
                                    insert_wnx);
                            WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);
                            insert_wnx = WN_first(insert_blk);
                        }
                        WN_Delete(insert_blk);
                        insert_blk = NULL;
                        rhs_ref_param_wn = NULL;
                    } else if (Get_Parent(coindexed_arr_ref) == lhs_ref_param_wn) {
                        /*corray write*/

                        /* if no sync handle specified for this write statement, then
                         * create a temporary one */
                        if (!Coarray_Sync(stmt_node)) {
                            ACCESS_HDL *access_handle = (ACCESS_HDL*)malloc(
                                    sizeof(ACCESS_HDL));
                            access_handle->handle_st = NULL;
                            access_handle->deferred = FALSE;
                            Set_Coarray_Sync(stmt_node, (ACCESS_HDL*)access_handle);
                        }

                        insert_blk = gen_coarray_access_stmt( wn, NULL,
                                LCB_st, xfer_sz_node, WRITE_FROM_LCB,
                                Coarray_Sync(stmt_node));

                        if (Coarray_Sync(stmt_node))
                            free(Coarray_Sync(stmt_node));

                        /* get LDID node in replace_wn that substitues wn
                         * pointed to by wipre*/
                        WN *store = get_store_parent(coindexed_arr_ref);
                        if (store) WN_offset(store) = 0;
                        substitute_lcb(coindexed_arr_ref, LCB_st,
                                       wn_arrayexp, &replace_wn);
                        WN_Delete(coindexed_arr_ref);
                        temp_wipre.Replace(replace_wn);
                        wipre = temp_wipre;

                        insert_wnx = WN_last(insert_blk);
                        while (insert_wnx) {
                            insert_wnx = WN_EXTRACT_FromBlock(insert_blk, insert_wnx);
                            WN_INSERT_BlockAfter(blk_node, stmt_node, insert_wnx);
                            insert_wnx = WN_last(insert_blk);
                        }
                        WN_Delete(insert_blk);
                        insert_blk = NULL;
                        lhs_ref_param_wn = NULL;
                    }
                }

                break;
        }
    }
    Is_True(WN_first(sync_blk) == NULL, ("sync_blk not empty after Pass 1"));
    WN_Delete(sync_blk);

    /* remove statements in caf_delete_list and clear the list */
    delete_caf_stmts_in_delete_list();


    /* reconstruct parent and COR_Info map */

    Parentize(func_body);

    COR_INFO cleared_cor_info;
    cleared_cor_info.is_cor = FALSE;
    cleared_cor_info.cor_depth = 0;
    for (wipre = wcpre.begin(); wipre != wcpre.end(); ++wipre) {
        ST *st1;
        TY_IDX ty1, ty2, ty3;
        TY_IDX coarray_type;
        TY_IDX elem_type;
        WN *replace_wn = NULL;
        WN *image;
        WN *coindexed_arr_ref;
        WN *direct_coarray_ref;
        WN *wn = wipre.Wn();
        WN *wn_next;

        Set_COR_Info(wn, &cleared_cor_info);

        switch (WN_operator(wn)) {
            case OPR_ARRAY:
            case OPR_ARRSECTION:

                coindexed_arr_ref =
                    expr_is_coindexed(wn, &image, &coarray_type,
                                      &direct_coarray_ref);

                if (image == NULL || is_vector_access(coindexed_arr_ref))
                    break;

                set_and_update_cor_depth(coindexed_arr_ref, TRUE, 1);

                /* skip ahead */
                while ( wipre.Wn() != direct_coarray_ref) wipre++;

                break;
        }
    }

    /* Pass 2: Generate Communication for coindexed array section accesses
     * TODO: Move this to later back-end phase?
     */
    curr_wipre = NULL;
    temp_wipre = NULL;
    for (wipre = wcpre.begin(); wipre != wcpre.end(); ++wipre) {
        COR_INFO wn_cor_info;
        ST *image_st = NULL;
        WN *insert_blk = NULL;
        WN *wn = wipre.Wn();
        WN *insert_sync;
        WN *insert_wnx;
        WN *blk_node;
        WN *stmt_node;
        WN *parent;
        WN *wn_arrayexp;

        TY_IDX coarray_type;
        TY_IDX elem_type;
        WN *replace_wn = NULL;
        WN *image;
        WN *coindexed_arr_ref;
        WN *direct_coarray_ref;

        Get_COR_Info(wn, &wn_cor_info);
        if (wn_cor_info.cor_depth == 0)
            continue;

        parent = wipre.Get_parent_wn();

        /* if its a statement, set stmt_node and also set blk_node to
         * parent if the parent is a block */
        if ((OPCODE_is_stmt(WN_opcode(wn)) ||
            OPCODE_is_scf(WN_opcode(wn)) &&
             WN_operator(wn) != OPR_BLOCK &&
             WN_operator(wn) != OPR_FUNC_ENTRY)) {
          stmt_node = wn;
          stmt_is_assignment = is_assignment_stmt(stmt_node);
          wn_arrayexp = NULL;
          if (WN_operator(parent) == OPR_BLOCK) blk_node = parent;
        }

        /* stores most recently encountered ARRAYEXP in wn_arrayexp */
        if (WN_operator(wn) == OPR_ARRAYEXP)
          wn_arrayexp = wn;

        if (stmt_in_delete_list(stmt_node))
            continue;

        switch (WN_operator(wn)) {
            case OPR_ARRAY:
            case OPR_ARRSECTION:
                coindexed_arr_ref = expr_is_coindexed(wn, &image, &coarray_type,
                                                      &direct_coarray_ref);
                if (image == NULL) break;

                if (is_vector_access(coindexed_arr_ref)) break;

                if ( stmt_is_assignment &&
                     array_ref_on_LHS(coindexed_arr_ref, &elem_type) ) {

                    /* coarray write */

                    WN *RHS_wn = WN_kid0(stmt_node);
                    if (elem_type == TY_IDX_ZERO) {
                        /* if elem_type being accessed still can't be resolved,
                         * just use the coarray etype */
                        elem_type = Ty_Table[coarray_type].u2.etype;
                    }
                    Is_True(is_lvalue(RHS_wn),
                            ("Unexpected coarray ref found in RHS"));

                    /* if no sync handle specified for this write statement, then
                     * create a temporary one */
                    if (!Coarray_Sync(stmt_node)) {
                        ACCESS_HDL *access_handle = (ACCESS_HDL*)malloc(
                                sizeof(ACCESS_HDL));
                        access_handle->handle_st = NULL;
                        access_handle->deferred = FALSE;
                        Set_Coarray_Sync(stmt_node, (ACCESS_HDL*)access_handle);
                    }


                    if (Is_LCB_Stmt(stmt_node)) {
                        ST *LCB_st;
                        WN *xfer_sz_node;

                        LCB_st = get_lcb_sym(RHS_wn);
                        xfer_sz_node = get_transfer_size(RHS_wn, elem_type);
                        insert_blk = gen_coarray_access_stmt( wn,
                                            NULL, LCB_st,
                                            xfer_sz_node, WRITE_FROM_LCB,
                                            Coarray_Sync(stmt_node));
                    } else {
                        WN *xfer_sz_node;
                        WN *local_access;

                        while (RHS_wn && WN_operator(RHS_wn) == OPR_CVTL) {
                            RHS_wn = WN_kid0(RHS_wn);
                        }

                        if (is_vector_access(RHS_wn)) break;

                        /* if RHS is not an array, temporarily turn it into
                         * one */
                        if (get_innermost_array(RHS_wn) == NULL) {
                            if (WN_operator(RHS_wn) == OPR_LDID) {
                                WN *new_RHS_wn;
                                /* replace the LDID expression with an Iload
                                 * to an array ref */
                                TY_IDX elem_ty = WN_ty(RHS_wn);
                                new_RHS_wn = WN_Iload( TY_mtype(elem_ty),
                                        WN_offset(RHS_wn),
                                        Make_Pointer_Type(elem_ty, FALSE),
                                        make_array_ref(WN_st(RHS_wn)));
                                WN_Delete(RHS_wn);
                                WN_kid0(stmt_node) = new_RHS_wn;
                                RHS_wn = new_RHS_wn;
                            } else if (WN_operator(RHS_wn) == OPR_ILOAD) {
                                WN *new_RHS_wn;
                                /* replace the ILOAD expression with an Iload
                                 * to an array ref */
                                TY_IDX elem_ty = WN_ty(RHS_wn);
                                new_RHS_wn = WN_Iload( TY_mtype(elem_ty),
                                        WN_offset(RHS_wn),
                                        Make_Pointer_Type(elem_ty, FALSE),
                                        make_array_ref(WN_kid0(RHS_wn)) );
                                WN_Delete(RHS_wn);
                                WN_kid0(stmt_node) = new_RHS_wn;
                                RHS_wn = new_RHS_wn;
                            } else if (WN_operator(RHS_wn) == OPR_MLOAD) {
                                WN_kid0(RHS_wn) = make_array_ref(WN_kid0(RHS_wn));
                            }

                            Parentize(RHS_wn);
                        }


                        if (WN_operator(RHS_wn) == OPR_ARRAYEXP &&
                            (WN_operator(WN_kid0(RHS_wn)) == OPR_ILOAD) ||
                            (WN_operator(WN_kid0(RHS_wn)) == OPR_MLOAD)) {
                          local_access = WN_kid0(WN_kid0(RHS_wn));
                          Is_True(WN_operator(local_access) == OPR_ARRSECTION,
                              ("expecting the operator for local_access "
                               "to be ARRSECTION"));
                        } else if (WN_operator(RHS_wn) == OPR_ILOAD ||
                                   WN_operator(RHS_wn) == OPR_MLOAD) {
                          local_access = WN_kid0(RHS_wn);
                          Is_True(WN_operator(local_access) == OPR_ARRAY,
                              ("expecting the operator for local_access "
                               "to be ARRAY"));
                        } else {
                          /* should not reach */
                          Is_True(0, ("bad WHIRL node encountered in RHS"));
                        }


                        xfer_sz_node = get_transfer_size(RHS_wn, elem_type);

                        insert_blk = gen_coarray_access_stmt( wn, local_access,
                                             NULL, xfer_sz_node, WRITE_DIRECT,
                                             Coarray_Sync(stmt_node));
                    }

                    if (Coarray_Sync(stmt_node))
                        free(Coarray_Sync(stmt_node));


                } else  if ( array_ref_on_RHS(coindexed_arr_ref, &elem_type) ) {
                    WN *new_stmt_node = NULL;

                    /* coarray read */

                    if (elem_type == TY_IDX_ZERO) {
                        /* if elem_type being accessed still can't be resolved,
                         * just use the coarray etype */
                        elem_type = Ty_Table[coarray_type].u2.etype;
                    }
                    WN *RHS_wn = WN_kid0(stmt_node);
                    Is_True(is_lvalue(RHS_wn),
                            ("Unexpected coarray ref found in RHS"));


                    if (Is_LCB_Stmt(stmt_node)) {
                        ST *LCB_st;
                        WN *LHS_wn = WN_kid1(stmt_node);
                        WN *xfer_sz_node;

                        LCB_st = get_lcb_sym(LHS_wn);
                        xfer_sz_node = get_transfer_size(wn, elem_type);
                        insert_blk = gen_coarray_access_stmt( wn,
                                            NULL, LCB_st,
                                            xfer_sz_node, READ_TO_LCB,
                                            Coarray_Sync(stmt_node));

                    } else {
                        WN *xfer_sz_node;
                        WN *LHS_wn;

                        if (is_load_operation(RHS_wn)) {
                            if (WN_operator(stmt_node) == OPR_STID) {
                                /* replace the STID statement with an Istore to an
                                 * array ref */
                                TY_IDX elem_ty = WN_ty(stmt_node);
                                new_stmt_node = WN_Istore( TY_mtype(elem_ty),
                                        WN_offset(stmt_node),
                                        Make_Pointer_Type(elem_ty, FALSE),
                                        make_array_ref(WN_st(stmt_node)),
                                        WN_COPY_Tree(RHS_wn));

                            } else if ((WN_operator(stmt_node) == OPR_ISTORE ||
                                       WN_operator(stmt_node) == OPR_MSTORE) &&
                                       get_innermost_array(WN_kid1(stmt_node)) == NULL) {
                                new_stmt_node = WN_COPY_Tree(stmt_node);
                                WN_kid1(new_stmt_node) =
                                    make_array_ref(WN_kid1(new_stmt_node));
                            } else {
                                new_stmt_node = WN_COPY_Tree(stmt_node);
                            }
                        } else {
                            new_stmt_node = WN_COPY_Tree(stmt_node);
                        }
                        LHS_wn = WN_kid1(new_stmt_node);

                        if (is_vector_access(LHS_wn)) {
                            WN_DELETE_Tree(new_stmt_node);
                            /* done with this statement for this pass, so free
                             * the sync map */
                            free(Coarray_Sync(stmt_node));
                            break;
                        }

                        Parentize(new_stmt_node);
                        Set_Coarray_Sync(new_stmt_node, Coarray_Sync(stmt_node));

                        xfer_sz_node = get_transfer_size(wn, elem_type);
                        insert_blk = gen_coarray_access_stmt( wn,
                                            WN_operator(LHS_wn)==OPR_ARRAYEXP?
                                             WN_kid0(LHS_wn) : LHS_wn,
                                            NULL,
                                            xfer_sz_node, READ_DIRECT,
                                            Coarray_Sync(new_stmt_node));
                        WN_DELETE_Tree(new_stmt_node);
                    }

                    if (Coarray_Sync(stmt_node)) {
                        free(Coarray_Sync(stmt_node));
                    } else {
                        /* should not reach */

                        Is_True( 0,
                                ("Coarray_Sync not created for coarray read statement"));
                        insert_sync = Generate_Call_coarray_wait_all();
                        WN_INSERT_BlockLast(insert_blk, insert_sync);
                    }

                }


                /* replace with runtime call for remote coarray access, if
                 * coarray access statement has been lowered  */
                if (insert_blk) {
                    insert_wnx = WN_first(insert_blk);
                    while (insert_wnx) {
                        insert_wnx = WN_EXTRACT_FromBlock(insert_blk, insert_wnx);
                        WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);
                        insert_wnx = WN_first(insert_blk);
                    }
                    /* defer deletion of the old statment so that we can continue
                     * to traverse the tree */
                    add_caf_stmt_to_delete_list(stmt_node, blk_node);
                    WN_Delete(insert_blk);
                    insert_blk = NULL;
                }
        }
    }



    /* remove statements in caf_delete_list and clear the list */
    delete_caf_stmts_in_delete_list();

    /* remove statements in func_exit_stmts */
    WN *wnx = WN_first(func_exit_stmts);
    while (wnx) {
      WN_DELETE_Tree(WN_EXTRACT_FromBlock(func_exit_stmts, wnx));
      wnx = WN_first(func_exit_stmts);
    }
    WN_Delete(func_exit_stmts);
    func_exit_stmts = NULL;

    save_coarray_symbol_map.clear();
    save_target_symbol_map.clear();
    auto_target_symbol_map.clear();


    WN_MAP_Delete(Caf_COR_Info_Map);
    WN_MAP_Delete(Caf_Parent_Map);
    WN_MAP_Delete(Caf_LCB_Map);
    WN_MAP_Delete(Caf_Visited_Map);
    WN_MAP_Delete(Caf_Coarray_Sync_Map);

    return pu;
} /* Coarray_Prelower */

/*
 * Lowering phase which must be called after VHO lowering
 */
WN * Coarray_Lower(PU_Info *current_pu, WN *pu)
{
    WN *func_body;

    /* Create Parent Map for WHIRL tree */
    Caf_COR_Info_Map = WN_MAP_Create(&caf_pool);
    Caf_Parent_Map = WN_MAP_Create(&caf_pool);
    Caf_LCB_Map = WN_MAP_Create(&caf_pool);
    Caf_Visited_Map = WN_MAP_Create(&caf_pool);
    Caf_Coarray_Sync_Map = WN_MAP_Create(&caf_pool);

    func_body = WN_func_body( pu );


    /* for support for character coarrays
     **/
    WN *lhs_ref_param_wn = NULL;
    WN *rhs_ref_param_wn = NULL;

    BOOL stmt_is_assignment = FALSE;

    /*
     *  Find co-subscripted array references, and generate LCBs.
     */
    WN_TREE_CONTAINER<PRE_ORDER> wcpre(func_body);
    WN_TREE_CONTAINER<PRE_ORDER> ::iterator wipre, curr_wipre=NULL, temp_wipre = NULL;
    Parentize(func_body);
    for (wipre = wcpre.begin(); wipre != wcpre.end(); ++wipre) {
        WN *insert_blk;
        WN *wn = wipre.Wn();
        WN *wn_next;
        WN *insert_wnx;
        WN *insert_sync;
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
        TY_IDX coarray_type;
        TY_IDX elem_type;
        WN *replace_wn = NULL;
        WN *image;
        WN *coindexed_arr_ref;
        WN *direct_coarray_ref;

        parent = wipre.Get_parent_wn();

        /* if its a statement, set stmt_node and also set blk_node to
         * parent if the parent is a block */
        if ((OPCODE_is_stmt(WN_opcode(wn)) ||
            OPCODE_is_scf(WN_opcode(wn)) &&
             WN_operator(wn) != OPR_BLOCK &&
             WN_operator(wn) != OPR_FUNC_ENTRY)) {
          stmt_node = wn;
          stmt_is_assignment = is_assignment_stmt(stmt_node);
          wn_arrayexp = NULL;
          if (WN_operator(parent) == OPR_BLOCK) blk_node = parent;
        }

        /* stores most recently encountered ARRAYEXP in wn_arrayexp */
        if (WN_operator(wn) == OPR_ARRAYEXP)
          wn_arrayexp = wn;

        switch (WN_operator(wn)) {
            case OPR_INTRINSIC_CALL:
                if (WN_opcode(wn) == OPC_VINTRINSIC_CALL) {
                   if (WN_intrinsic(wn) == INTRN_CASSIGNSTMT) {
                    /*for character coarrays support*/
                    lhs_ref_param_wn = WN_kid0(wn);
                    rhs_ref_param_wn = WN_kid1(wn);
                  }
                }

                break;

            case OPR_ARRAY:

                coindexed_arr_ref = expr_is_coindexed(wn, &image, &coarray_type,
                                                      &direct_coarray_ref);
                if (image == NULL) break;
                //if (is_vector_access(coindexed_arr_ref)) break;
                if (Was_Visited(coindexed_arr_ref)) break;

                temp_wipre = wipre;
                while ( temp_wipre.Wn() != coindexed_arr_ref )
                    temp_wipre++;

                Set_Visited(coindexed_arr_ref);

                if ( stmt_is_assignment &&
                     array_ref_on_LHS(coindexed_arr_ref, &elem_type) ) {
                    /* handle remote write */

                    if (elem_type == TY_IDX_ZERO) {
                        /* if elem_type being accessed still can't be resolved,
                         * just use the coarray etype */
                        elem_type = Ty_Table[coarray_type].u2.etype;
                    }
                    WN *RHS_wn = WN_kid0(stmt_node);
                    BOOL RHS_is_preg = FALSE;

                    if (WN_operator(RHS_wn) == OPR_LDID &&
                        ST_sclass(WN_st(RHS_wn)) == SCLASS_REG) {
                        RHS_is_preg = TRUE;
                    }

                    if ( !is_lvalue(RHS_wn)
                         || RHS_is_preg || is_convert_operation(RHS_wn)) {
                        WN *new_stmt_node;
                        ST *LCB_st;
                        WN *xfer_sz_node;

                        /* create LCB for RHS */
                        LCB_st = gen_lcbptr_symbol(
                               Make_Pointer_Type(elem_type,FALSE),
                               "LCB" );
                        xfer_sz_node = get_transfer_size(coindexed_arr_ref,
                                                         elem_type);
                        insert_wnx = Generate_Call_acquire_lcb(
                                xfer_sz_node,
                                WN_Lda(Pointer_type, 0, LCB_st));
                        WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);

                        /* create "normalized" assignment to remote coarray */
                        new_stmt_node = WN_COPY_Tree(stmt_node);
                        replace_RHS_with_LCB(new_stmt_node,
                                             LCB_st);
                        Set_LCB_Stmt(new_stmt_node);
                        WN_INSERT_BlockAfter(blk_node, stmt_node, new_stmt_node);

                        /* replace LHS */
                        WN *store = get_store_parent(coindexed_arr_ref);
                        if (store) WN_offset(store) = 0;
                        substitute_lcb(coindexed_arr_ref, LCB_st,
                                       wn_arrayexp, &replace_wn);
                        WN_Delete(coindexed_arr_ref);
                        temp_wipre.Replace(replace_wn);
                        wipre = temp_wipre;

                    } else if (is_load_operation(RHS_wn)) {
                        while (RHS_wn && WN_operator(RHS_wn) == OPR_CVTL) {
                            RHS_wn = WN_kid0(RHS_wn);
                        }
                        if (WN_operator(RHS_wn) == OPR_LDID) {
                            WN *new_RHS_wn;
                            /* replace the LDID expression with an Iload to an
                             * array ref */
                            TY_IDX elem_ty = WN_ty(RHS_wn);
                            new_RHS_wn = WN_Iload( TY_mtype(elem_ty),
                                    WN_offset(RHS_wn),
                                    Make_Pointer_Type(elem_ty, FALSE),
                                    make_array_ref(WN_st(RHS_wn)));
                            WN_Delete(RHS_wn);
                            WN_kid0(stmt_node) = new_RHS_wn;
                        }
                    } else {
                    /* handle indirection ... */
                    }


                } else if ( array_ref_on_RHS(coindexed_arr_ref, &elem_type) ) {
                    /* handle remote read */

                    if (elem_type == TY_IDX_ZERO) {
                        /* if elem_type being accessed still can't be resolved,
                         * just use the coarray etype */
                        elem_type = Ty_Table[coarray_type].u2.etype;
                    }
                    WN *RHS_wn = WN_kid0(stmt_node);
                    WN *LHS_img;
                    TY_IDX LHS_coarray_type;
                    BOOL LHS_is_coindexed  = FALSE;
                    BOOL LHS_is_preg = FALSE;

                    if (WN_operator(stmt_node) == OPR_ISTORE ||
                        WN_operator(stmt_node) == OPR_MSTORE) {
                        WN *LHS_wn = WN_kid1(stmt_node);
                        if (expr_is_coindexed(LHS_wn, &LHS_img,
                            &LHS_coarray_type)) {
                            LHS_is_coindexed = TRUE;
                        }
                    } else if (WN_operator(stmt_node) == OPR_STID &&
                            ST_sclass(WN_st(stmt_node)) == SCLASS_REG) {
                        LHS_is_preg = TRUE;
                    }

                    if ( !is_lvalue(RHS_wn) || LHS_is_coindexed
                         || LHS_is_preg
                         || is_convert_operation(RHS_wn)) {
                        WN *new_stmt_node;
                        ST *LCB_st;
                        WN *xfer_sz_node;
                        INT num_codim;

                        /* create LCB for coindexed array ref */
                        LCB_st = gen_lcbptr_symbol(
                               Make_Pointer_Type(elem_type,FALSE),
                               "LCB" );
                        xfer_sz_node = get_transfer_size(coindexed_arr_ref,
                                                         elem_type);
                        insert_wnx = Generate_Call_acquire_lcb(
                                xfer_sz_node,
                                WN_Lda(Pointer_type, 0, LCB_st));
                        WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);

                        /* create "normalized" assignment from remote coarray */
                        num_codim = coindexed_arr_ref == direct_coarray_ref ?
                                    get_coarray_corank(coarray_type) : 0;
                        new_stmt_node = get_lcb_assignment(
                                WN_COPY_Tree(Get_Parent(wn)),
                                num_codim,
                                LCB_st);

                        WN_INSERT_BlockBefore(blk_node, stmt_node, new_stmt_node);

                        /* replace term in RHS */
                        WN *load = get_load_parent(coindexed_arr_ref);
                        if (load) WN_offset(load) = 0;
                        substitute_lcb(coindexed_arr_ref, LCB_st,
                                       wn_arrayexp, &replace_wn);
                        WN_Delete(coindexed_arr_ref);
                        temp_wipre.Replace(replace_wn);
                        wipre = temp_wipre;

                        /* call to release LCB */
                        insert_wnx = Generate_Call_release_lcb(
                                WN_Lda(Pointer_type, 0, LCB_st));
                        WN_INSERT_BlockAfter(blk_node, stmt_node, insert_wnx);

                    } else if (is_load_operation(RHS_wn)) {

                        if (WN_operator(stmt_node) == OPR_STID) {
                            /* replace the STID statement with an Istore to an
                             * array ref */
                            TY_IDX elem_ty = WN_ty(stmt_node);
                            insert_wnx = WN_Istore( TY_mtype(elem_ty),
                                    WN_offset(stmt_node),
                                    Make_Pointer_Type(elem_ty, FALSE),
                                    make_array_ref(WN_st(stmt_node)),
                                    WN_COPY_Tree(RHS_wn));
                            WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);
                            Set_Coarray_Sync(insert_wnx, Coarray_Sync(stmt_node));

                            add_caf_stmt_to_delete_list(stmt_node, blk_node);
                        }
                    } else {
                    /* handle indirection ... */
                    }
                } else if ( array_ref_in_parm(coindexed_arr_ref, &elem_type) ) {
                    WN *new_stmt_node;
                    ST *LCB_st;
                    WN *xfer_sz_node;

                    /* create LCB for coindexed array ref */
                    LCB_st = gen_lcbptr_symbol(
                            Make_Pointer_Type(elem_type,FALSE),
                            "LCB" );
                    xfer_sz_node = get_transfer_size(coindexed_arr_ref,
                            elem_type);
                    insert_wnx = Generate_Call_acquire_lcb(
                            xfer_sz_node,
                            WN_Lda(Pointer_type, 0, LCB_st));
                    WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);

                    if (Get_Parent(coindexed_arr_ref) == rhs_ref_param_wn) {
                        /*coarray read*/

                        /* if no sync handle specified for this read statement, then
                         * create a temporary one */
                        if (!Coarray_Sync(stmt_node)) {
                            ACCESS_HDL *access_handle = (ACCESS_HDL*)malloc(
                                    sizeof(ACCESS_HDL));
                            access_handle->handle_st =
                                Gen_Temp_Named_Symbol(MTYPE_To_TY(MTYPE_U8),
                                        "hdl", CLASS_VAR, SCLASS_AUTO);
                            access_handle->deferred = FALSE;
                            Set_ST_is_temp_var(access_handle->handle_st);
                            Set_Coarray_Sync(stmt_node, (ACCESS_HDL*)access_handle);
                        }

                        insert_blk = gen_coarray_access_stmt( wn,
                                NULL, LCB_st, xfer_sz_node, READ_TO_LCB,
                                Coarray_Sync(stmt_node));

                        /* insert sync for remote reads if not deferred*/
                        if (Coarray_Sync(stmt_node)) {
                            if (Coarray_Sync(stmt_node)->deferred == FALSE) {
                                WN *sync_hdl;
                                ST *handle_st;
                                handle_st =
                                    Coarray_Sync(stmt_node)->handle_st;
                                if (handle_st == NULL) {
                                    sync_hdl = WN_Intconst(Pointer_type,0);
                                } else {
                                    sync_hdl = WN_Lda( Pointer_type, 0, handle_st );
                                }
                                insert_sync =
                                    Generate_Call_coarray_wait(sync_hdl, TRUE);
                                WN_INSERT_BlockLast(insert_blk, insert_sync);
                            }

                            free(Coarray_Sync(stmt_node));
                        } else {
                            insert_sync = Generate_Call_coarray_wait_all();
                            WN_INSERT_BlockLast(insert_blk, insert_sync);
                        }

                        /* get LDID node in replace_wn that substitues wn
                         * pointed to by wipre*/
                        WN *load = get_load_parent(coindexed_arr_ref);
                        if (load) WN_offset(load) = 0;
                        substitute_lcb(coindexed_arr_ref, LCB_st,
                                       wn_arrayexp, &replace_wn);
                        WN_Delete(coindexed_arr_ref);
                        temp_wipre.Replace(replace_wn);
                        wipre = temp_wipre;

                        insert_wnx = WN_first(insert_blk);
                        while (insert_wnx) {
                            insert_wnx = WN_EXTRACT_FromBlock(insert_blk,
                                    insert_wnx);
                            WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);
                            insert_wnx = WN_first(insert_blk);
                        }
                        WN_Delete(insert_blk);
                        insert_blk = NULL;
                        rhs_ref_param_wn = NULL;
                    } else if (Get_Parent(coindexed_arr_ref) == lhs_ref_param_wn) {
                        /*corray write*/

                        /* if no sync handle specified for this write statement, then
                         * create a temporary one */
                        if (!Coarray_Sync(stmt_node)) {
                            ACCESS_HDL *access_handle = (ACCESS_HDL*)malloc(
                                    sizeof(ACCESS_HDL));
                            access_handle->handle_st = NULL;
                            access_handle->deferred = FALSE;
                            Set_Coarray_Sync(stmt_node, (ACCESS_HDL*)access_handle);
                        }

                        insert_blk = gen_coarray_access_stmt( wn, NULL,
                                LCB_st, xfer_sz_node, WRITE_FROM_LCB,
                                Coarray_Sync(stmt_node));

                        if (Coarray_Sync(stmt_node))
                            free(Coarray_Sync(stmt_node));

                        /* get LDID node in replace_wn that substitues wn
                         * pointed to by wipre*/
                        WN *store = get_store_parent(coindexed_arr_ref);
                        if (store) WN_offset(store) = 0;
                        substitute_lcb(coindexed_arr_ref, LCB_st,
                                       wn_arrayexp, &replace_wn);
                        WN_Delete(coindexed_arr_ref);
                        temp_wipre.Replace(replace_wn);
                        wipre = temp_wipre;

                        insert_wnx = WN_last(insert_blk);
                        while (insert_wnx) {
                            insert_wnx = WN_EXTRACT_FromBlock(insert_blk, insert_wnx);
                            WN_INSERT_BlockAfter(blk_node, stmt_node, insert_wnx);
                            insert_wnx = WN_last(insert_blk);
                        }
                        WN_Delete(insert_blk);
                        insert_blk = NULL;
                        lhs_ref_param_wn = NULL;
                    }
                }

                break;
        }
    }


    /*
     * Lower to CAF Runtime calls for 1-sided communication.
     */
    Parentize(func_body);
    for (wipre = wcpre.begin(); wipre != wcpre.end(); ++wipre) {
        WN *insert_blk = NULL;
        WN *wn = wipre.Wn();
        WN *insert_wnx;
        WN *insert_sync;
        WN *blk_node;
        WN *stmt_node;
        WN *parent;
        WN *wn_arrayexp;

        TY_IDX coarray_type;
        TY_IDX elem_type;
        WN *replace_wn = NULL;
        WN *image;
        WN *coindexed_arr_ref;
        WN *direct_coarray_ref;

        parent = wipre.Get_parent_wn();

        /* if its a statement, set stmt_node and also set blk_node to
         * parent if the parent is a block */
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

        if (stmt_in_delete_list(stmt_node))
            continue;

        switch (WN_operator(wn)) {
            case OPR_ARRAY:
                coindexed_arr_ref = expr_is_coindexed(wn, &image, &coarray_type,
                                                      &direct_coarray_ref);
                if (image == NULL) break;

                //if (is_vector_access(coindexed_arr_ref)) break;

                if ( array_ref_on_LHS(coindexed_arr_ref, &elem_type) ) {
                    /* coarray write */

                    WN *RHS_wn = WN_kid0(stmt_node);
                    if (elem_type == TY_IDX_ZERO) {
                        /* if elem_type being accessed still can't be resolved,
                         * just use the coarray etype */
                        elem_type = Ty_Table[coarray_type].u2.etype;
                    }
                    Is_True(is_lvalue(RHS_wn),
                            ("Unexpected coarray ref found in RHS"));

                    /* if no sync handle specified for this write statement, then
                     * create a temporary one */
                    if (!Coarray_Sync(stmt_node)) {
                        ACCESS_HDL *access_handle = (ACCESS_HDL*)malloc(
                                sizeof(ACCESS_HDL));
                        access_handle->handle_st = NULL;
                        access_handle->deferred = FALSE;
                        Set_Coarray_Sync(stmt_node, (ACCESS_HDL*)access_handle);
                    }

                    if (Is_LCB_Stmt(stmt_node)) {
                        ST *LCB_st;
                        WN *xfer_sz_node;

                        LCB_st = get_lcb_sym(RHS_wn);
                        xfer_sz_node = get_transfer_size(RHS_wn, elem_type);
                        insert_blk = gen_coarray_access_stmt( wn,
                                NULL, LCB_st,
                                xfer_sz_node, WRITE_FROM_LCB,
                                Coarray_Sync(stmt_node));
                    } else {
                        WN *xfer_sz_node;
                        WN *local_access;

                        while (RHS_wn && WN_operator(RHS_wn) == OPR_CVTL) {
                            RHS_wn = WN_kid0(RHS_wn);
                        }

                        if (WN_operator(RHS_wn) == OPR_ILOAD ||
                                   WN_operator(RHS_wn) == OPR_MLOAD) {
                          local_access = WN_kid0(RHS_wn);
                          Is_True(WN_operator(local_access) == OPR_ARRAY,
                              ("expecting the operator for local_access "
                               "to be ARRAY"));
                        } else {
                          /* should not reach */
                          Is_True(0, ("bad WHIRL node encountered in RHS"));
                        }

                        xfer_sz_node = get_transfer_size(RHS_wn, elem_type);

                        insert_blk = gen_coarray_access_stmt( wn, local_access,
                                             NULL, xfer_sz_node, WRITE_DIRECT,
                                             Coarray_Sync(stmt_node));
                    }

                    if (Coarray_Sync(stmt_node))
                        free(Coarray_Sync(stmt_node));

                } else  if ( array_ref_on_RHS(coindexed_arr_ref, &elem_type) ) {
                    /* coarray read */

                    if (elem_type == TY_IDX_ZERO) {
                        /* if elem_type being accessed still can't be resolved,
                         * just use the coarray etype */
                        elem_type = Ty_Table[coarray_type].u2.etype;
                    }
                    WN *RHS_wn = WN_kid0(stmt_node);
                    Is_True(is_lvalue(RHS_wn),
                            ("Unexpected coarray ref found in RHS"));

                    /* if no sync handle specified for this read statement, then
                     * create a temporary one */
                    if (!Coarray_Sync(stmt_node)) {
                        ACCESS_HDL *access_handle = (ACCESS_HDL*)malloc(
                                sizeof(ACCESS_HDL));
                        access_handle->handle_st =
                          Gen_Temp_Named_Symbol(MTYPE_To_TY(MTYPE_U8),
                                              "hdl", CLASS_VAR, SCLASS_AUTO);
                        access_handle->deferred = FALSE;
                        Set_ST_is_temp_var(access_handle->handle_st);
                        Set_Coarray_Sync(stmt_node, (ACCESS_HDL*)access_handle);
                    }

                    if (Is_LCB_Stmt(stmt_node)) {
                        ST *LCB_st;
                        WN *LHS_wn = WN_kid1(stmt_node);
                        WN *xfer_sz_node;

                        LCB_st = get_lcb_sym(LHS_wn);
                        xfer_sz_node = get_transfer_size(wn, elem_type);
                        insert_blk = gen_coarray_access_stmt( wn,
                                NULL, LCB_st,
                                xfer_sz_node, READ_TO_LCB,
                                Coarray_Sync(stmt_node));
                    } else {
                        WN *xfer_sz_node;
                        WN *LHS_wn = WN_kid1(stmt_node);

                        xfer_sz_node = get_transfer_size(wn, elem_type);
                        insert_blk = gen_coarray_access_stmt( wn,
                                            WN_operator(LHS_wn)==OPR_ARRAYEXP?
                                             WN_kid0(LHS_wn) : LHS_wn,
                                            NULL,
                                            xfer_sz_node, READ_DIRECT,
                                            Coarray_Sync(stmt_node));
                    }

                    /* insert sync for remote reads if not deferred*/
                    if (Coarray_Sync(stmt_node)) {
                        if (Coarray_Sync(stmt_node)->deferred == FALSE) {
                            WN *sync_hdl;
                            ST *handle_st;
                            handle_st =
                                Coarray_Sync(stmt_node)->handle_st;
                            if (handle_st == NULL) {
                                sync_hdl = WN_Intconst(Pointer_type,0);
                            } else {
                                sync_hdl = WN_Lda( Pointer_type, 0, handle_st );
                            }
                            insert_sync =
                                Generate_Call_coarray_wait(sync_hdl, TRUE);
                            WN_INSERT_BlockLast(insert_blk, insert_sync);
                        }

                        free(Coarray_Sync(stmt_node));
                    } else {
                        /* should not reach */

                        Is_True( 0,
                         ("Coarray_Sync not created for coarray read statement"));
                        insert_sync = Generate_Call_coarray_wait_all();
                        WN_INSERT_BlockLast(insert_blk, insert_sync);
                    }
                }

                /* replace with runtime call for remote coarray access */
                if (insert_blk) {
                    insert_wnx = WN_first(insert_blk);
                    while (insert_wnx) {
                        insert_wnx = WN_EXTRACT_FromBlock(insert_blk, insert_wnx);
                        WN_INSERT_BlockBefore(blk_node, stmt_node, insert_wnx);
                        insert_wnx = WN_first(insert_blk);
                    }
                    /* defer deletion of the old statment so that we can continue
                     * to traverse the tree */
                    add_caf_stmt_to_delete_list(stmt_node, blk_node);
                    WN_Delete(insert_blk);
                    insert_blk = NULL;
                }
        }
    }


    /* remove statements in caf_delete_list and clear the list */
    delete_caf_stmts_in_delete_list();

    WN_MAP_Delete(Caf_COR_Info_Map);
    WN_MAP_Delete(Caf_Parent_Map);
    WN_MAP_Delete(Caf_LCB_Map);
    WN_MAP_Delete(Caf_Visited_Map);
    WN_MAP_Delete(Caf_Coarray_Sync_Map);

    return pu;
} /* Coarray_Lower */

WN * Coarray_Symbols_Lower(PU_Info *current_pu, WN *pu)
{
    BOOL is_main = FALSE;
    static BOOL global_coarrays_processed = FALSE;

    WN *func_body, *func_exit_stmts;

    is_main  = currentpu_ismain();

    /* insert call to caf_init if this is the main PU */
    is_main  = currentpu_ismain();
    func_body = WN_func_body( pu );
    func_exit_stmts = WN_CreateBlock();

    /* generate new coarray and target symbols,
     * nullify local coarray symbols, which should disappear from the AST by
     * the end of this routine */
    ST *sym;
    UINT32 i;
    FOREACH_SYMBOL(CURRENT_SYMTAB, sym, i) {
        if (sym->sym_class == CLASS_VAR && is_coarray_type(ST_type(sym))) {
            if (ST_sclass(sym) == SCLASS_PSTATIC) {
                gen_save_coarray_symbol(sym);

                /* don't allot space for this symbol in global memory, if
                 * uninitialized */
                if (!ST_is_initialized(sym)) {
                    Set_ST_type(sym, null_coarray_type);
                    Set_ST_is_not_used(sym);
                }
            }
        } else if (sym->sym_class == CLASS_VAR && ST_is_f90_target(sym)) {
            if (ST_sclass(sym) == SCLASS_PSTATIC || is_main) {
                gen_save_target_symbol(sym);
                /* don't allot space for this symbol in global memory, if
                 * uninitialized */
                if (!ST_is_initialized(sym)) {
                    Set_ST_type(sym, null_array_type);
                    Set_ST_is_not_used(sym);
                }
            } else if (ST_sclass(sym) == SCLASS_AUTO) {
                gen_auto_target_symbol(sym);
                ST *targ_ptr = auto_target_symbol_map[sym];
                WN *insert_wnx = Generate_Call_target_alloc(
                        WN_Intconst(MTYPE_U8, TY_size(ST_type(sym))),
                        WN_Lda(Pointer_type, 0, targ_ptr));
                WN_INSERT_BlockFirst( func_body, insert_wnx);
                insert_wnx = Generate_Call_target_dealloc(
                    WN_Lda(Pointer_type, 0, targ_ptr));
                WN_INSERT_BlockLast( func_exit_stmts, insert_wnx);

                /* don't allot space for this symbol in stack */
                Set_ST_type(sym, null_array_type);
                Set_ST_is_not_used(sym);
            }
        }
    }


    if (global_coarrays_processed == FALSE) {
        FOREACH_SYMBOL(GLOBAL_SYMTAB, sym, i) {
            if (sym->sym_class == CLASS_VAR &&
                is_coarray_type(ST_type(sym))) {
                if (ST_sclass(sym) == SCLASS_COMMON ||
                    ST_sclass(sym) == SCLASS_DGLOBAL) {
                    gen_global_save_coarray_symbol(sym);
                }
            } else if (sym->sym_class == CLASS_VAR && ST_is_f90_target(sym)) {
                if (ST_sclass(sym) == SCLASS_COMMON ||
                    ST_sclass(sym) == SCLASS_DGLOBAL) {
                    gen_global_save_target_symbol(sym);
                }
            }
        }
        global_coarrays_processed = TRUE;
    }

    /* Replace Save Coarray and Target Symbols
     */
    if (!save_coarray_symbol_map.empty() ||
        !common_save_coarray_symbol_map.empty() ||
        !save_target_symbol_map.empty() ||
        !common_save_target_symbol_map.empty() ||
        !auto_target_symbol_map.empty() ) {
        WN_TREE_CONTAINER<POST_ORDER> wcpost(func_body);
        WN_TREE_CONTAINER<POST_ORDER> ::iterator wipost;
        for (wipost = wcpost.begin(); wipost != wcpost.end(); ++wipost) {
            WN *blk_node, *stmt_node;
            WN *wn = wipost.Wn();
            ST *st1;
            TY_IDX ty;
            WN_OFFSET offset = 0;
            switch (WN_operator(wn)) {
            case OPR_RETURN:
                if (func_exit_stmts) {
                    WN *insert_wnx;
                    insert_wnx = WN_first(func_exit_stmts);
                    while (insert_wnx) {
                        WN_INSERT_BlockBefore(blk_node, stmt_node, WN_COPY_Tree(insert_wnx));
                        insert_wnx = WN_next(insert_wnx);
                    }
                }
                break;
              case OPR_LDA:
                  st1 = WN_st(wn);
                  ty  = WN_ty(wn);
                  offset = WN_offset(wn);
                  if (st1 && is_coarray_type(ST_type(st1))) {
                      ST *save_coarray_replace;
                      if (ST_sclass(st1) == SCLASS_COMMON ||
                          ST_sclass(st1) == SCLASS_DGLOBAL) {
                          save_coarray_replace =
                              common_save_coarray_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_PSTATIC) {
                          save_coarray_replace =
                              save_coarray_symbol_map[st1];
                      } else {
                          continue;
                      }
                      Is_True(save_coarray_replace,
                        ("New symbol for save coarray was not created yet"));

                      wipost.Replace(
                              WN_Add( MTYPE_U8,
                                 WN_Ldid(TY_mtype(ST_type(save_coarray_replace)),
                                      0, save_coarray_replace, ty),
                                 WN_Intconst(MTYPE_U8, offset))
                              );
                      WN_Delete(wn);
                  } else if (st1 && ST_is_f90_target(st1)) {
                      ST *targptr_replace;
                      if (ST_sclass(st1) == SCLASS_COMMON ||
                          ST_sclass(st1) == SCLASS_DGLOBAL) {
                          targptr_replace =
                              common_save_target_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_PSTATIC || is_main) {
                          targptr_replace =
                              save_target_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_AUTO) {
                          targptr_replace =
                              auto_target_symbol_map[st1];
                      } else {
                          continue;
                      }
                      Is_True(targptr_replace,
                        ("New symbol for target ptr was not created yet"));

                      wipost.Replace(
                              WN_Add( MTYPE_U8,
                                 WN_Ldid(TY_mtype(ST_type(targptr_replace)),
                                      0, targptr_replace, ty),
                                 WN_Intconst(MTYPE_U8, offset))
                              );
                      WN_Delete(wn);
                  }
                  break;

              case OPR_LDID:
                  st1 = WN_st(wn);
                  ty  = WN_ty(wn);
                  offset = WN_offset(wn);
                  if (st1 && is_coarray_type(ST_type(st1))) {
                      ST *save_coarray_replace;
                      if (ST_sclass(st1) == SCLASS_COMMON ||
                          ST_sclass(st1) == SCLASS_DGLOBAL) {
                          save_coarray_replace =
                              common_save_coarray_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_PSTATIC) {
                          save_coarray_replace =
                              save_coarray_symbol_map[st1];
                      } else {
                          continue;
                      }
                      Is_True(save_coarray_replace,
                        ("New symbol for save coarray was not created yet"));
                      wipost.Replace(
                              WN_IloadLdid(WN_desc(wn),
                                  offset, ty,
                                  save_coarray_replace, 0)
                              );
                      WN_Delete(wn);
                  } else if (st1 && ST_is_f90_target(st1)) {
                      ST *targptr_replace;
                      if (ST_sclass(st1) == SCLASS_COMMON ||
                          ST_sclass(st1) == SCLASS_DGLOBAL) {
                          targptr_replace =
                              common_save_target_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_PSTATIC || is_main) {
                          targptr_replace =
                              save_target_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_AUTO) {
                          targptr_replace =
                              auto_target_symbol_map[st1];
                      } else {
                          continue;
                      }
                      Is_True(targptr_replace,
                        ("New symbol for target ptr was not created yet"));

                      wipost.Replace(
                              WN_IloadLdid(WN_desc(wn),
                                  offset, ty, targptr_replace, 0)
                              );
                      WN_Delete(wn);
                  }

                  break;

              case OPR_STID:
                  st1 = WN_st(wn);
                  ty  = WN_ty(wn);
                  offset = WN_offset(wn);
                  if (st1 && is_coarray_type(ST_type(st1))) {
                      ST *save_coarray_replace;
                      if (ST_sclass(st1) == SCLASS_COMMON ||
                          ST_sclass(st1) == SCLASS_DGLOBAL) {
                          save_coarray_replace =
                              common_save_coarray_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_PSTATIC) {
                          save_coarray_replace =
                              save_coarray_symbol_map[st1];
                      } else {
                          continue;
                      }
                      Is_True(save_coarray_replace,
                        ("New symbol for save coarray was not created yet"));
                      wipost.Replace(
                              WN_Istore(WN_rtype(WN_kid0(wn)),
                                  offset, Make_Pointer_Type(ty),
                                  WN_Ldid(TY_mtype(ST_type(save_coarray_replace)),
                                      0, save_coarray_replace,
                                      ST_type(save_coarray_replace)),
                                  WN_COPY_Tree(WN_kid0(wn)), 0)
                              );
                      WN_Delete(wn);
                  } else if (st1 && ST_is_f90_target(st1)) {
                      ST *targptr_replace;
                      if (ST_sclass(st1) == SCLASS_COMMON ||
                          ST_sclass(st1) == SCLASS_DGLOBAL) {
                          targptr_replace =
                              common_save_target_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_PSTATIC || is_main) {
                          targptr_replace =
                              save_target_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_AUTO) {
                          targptr_replace =
                              auto_target_symbol_map[st1];
                      } else {
                          continue;
                      }
                      Is_True(targptr_replace,
                        ("New symbol for target ptr was not created yet"));

                      wipost.Replace(
                              WN_Istore(WN_rtype(WN_kid0(wn)),
                                  offset, Make_Pointer_Type(ty),
                                  WN_Ldid(TY_mtype(ST_type(targptr_replace)),
                                      0, targptr_replace,
                                      ST_type(targptr_replace)),
                                  WN_COPY_Tree(WN_kid0(wn)), 0)
                              );
                      WN_Delete(wn);
                  }
                  break;

              default:
                  st1 = WN_has_sym(wn) ? WN_st(wn) :  NULL;
                  if (st1 && is_coarray_type(ST_type(st1))) {
                      ST *save_coarray_replace;
                      if (ST_sclass(st1) == SCLASS_COMMON ||
                          ST_sclass(st1) == SCLASS_DGLOBAL) {
                          save_coarray_replace =
                              common_save_coarray_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_PSTATIC) {
                          save_coarray_replace =
                              save_coarray_symbol_map[st1];
                      } else {
                          continue;
                      }
                      Fail_FmtAssertion
                        ("Encountered unexpected save coarray symbol in whirl tree.");
                  } else if (st1 && ST_is_f90_target(st1)) {
                      ST *targptr_replace;
                      if (ST_sclass(st1) == SCLASS_COMMON ||
                          ST_sclass(st1) == SCLASS_DGLOBAL) {
                          targptr_replace =
                              common_save_target_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_PSTATIC || is_main) {
                          targptr_replace =
                              save_target_symbol_map[st1];
                      } else if (ST_sclass(st1) == SCLASS_AUTO) {
                          targptr_replace =
                              auto_target_symbol_map[st1];
                      } else {
                          continue;
                      }
                      Fail_FmtAssertion
                        ("Encountered unexpected target symbol in whirl tree.");
                  }
                  break;
            }
        }
    }

    /* remove statements in func_exit_stmts */
    WN *wnx = WN_first(func_exit_stmts);
    while (wnx) {
      WN_DELETE_Tree(WN_EXTRACT_FromBlock(func_exit_stmts, wnx));
      wnx = WN_first(func_exit_stmts);
    }
    WN_Delete(func_exit_stmts);

    func_exit_stmts = NULL;

    save_coarray_symbol_map.clear();
    save_target_symbol_map.clear();
    auto_target_symbol_map.clear();

    return pu;
}

void Coarray_Global_Symbols_Remove()
{
    ST *sym;
    UINT32 i;

    /* nullify global coarray symbols, which should by now be removed from all
     * PUs in the AST.
     * TODO: double-check that space is in fact not being allocated for these
     * symbols
     * */
    FOREACH_SYMBOL(GLOBAL_SYMTAB, sym, i) {
        if (sym->sym_class == CLASS_VAR &&
                is_coarray_type(ST_type(sym))) {
            if (ST_sclass(sym) == SCLASS_COMMON ||
                    ST_sclass(sym) == SCLASS_DGLOBAL) {
                /* don't allot space for this symbol in global memory, if
                 * uninitialized */
                if (!ST_is_initialized(sym)) {
                    Set_ST_type(sym, null_coarray_type);
                    Set_ST_is_not_used(sym);
                }
            }
        } else if (sym->sym_class == CLASS_VAR && ST_is_f90_target(sym)) {
            if (ST_sclass(sym) == SCLASS_COMMON ||
                    ST_sclass(sym) == SCLASS_DGLOBAL) {
                /* don't allot space for this symbol in global memory, if
                 * uninitialized */
                if (!ST_is_initialized(sym)) {
                    Set_ST_type(sym, null_array_type);
                    Set_ST_is_not_used(sym);
                }
            }
        }
    }
}


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
 *  - operator for remote_access is either OPR_ARRAY or OPR_ARRSECTION, and
 *    the offset of its parent ILOAD is 0
 *  - if access is READ_DIRECT or WRITE_DIRECT, local_ref will be non-NULL
 *    and LCB_st will be NULL
 *  - coarray_ref (and local_ref, if defined) are part of the PU tree
 *
 */
static WN* gen_coarray_access_stmt(WN *coarray_ref, WN *local_ref,
                                   ST *LCB_st, WN *xfer_size,
                                   ACCESS_TYPE access,
                                   ACCESS_HDL *access_handle,
                                   ST *image_st)
{
    WN *return_blk;
    TY_IDX remote_array_type;
    TY_IDX coarray_type, elem_type;
    WN *image;
    WN *coarray_base_address;
    WN *local_base_address;

    WN *direct_coarray_ref;
    vector<ARRAY_ACCESS_DESC> remote_access;
    ARRAY_ACCESS_DESC* remote_arrsection = NULL;

    int remote_num_array_cpnts = 0;
    int remote_arrsection_idx = -1;
    int remote_access_elemsize = 0;
    int remote_arrsection_first_sm = 0;

    vector<ARRAY_ACCESS_DESC> local_access;
    int local_num_array_cpnts = 0;
    int local_arrsection_idx = -1;
    WN_OFFSET offset = 0;
    ARRAY_ACCESS_DESC* local_arrsection = NULL;
    int local_access_elemsize = 0;
    int local_arrsection_first_sm = 0;

    return_blk = WN_CreateBlock();

    coarray_ref = expr_is_coindexed(coarray_ref, &image, &coarray_type,
                                    &direct_coarray_ref);


    if (image_st) {
        WN_DELETE_Tree(image);
        image = WN_Ldid(MTYPE_U8, 0, image_st, ST_type(image_st));
    }


    /* get enclosing assignment stmt node */
    WN *stmt_node = coarray_ref;
    while (stmt_node && !OPCODE_is_stmt(WN_opcode(stmt_node)))
        stmt_node = Get_Parent(stmt_node);

    Is_True( stmt_node != NULL,
            ("couldn't find enclosing statement for coarray reference!"));

    elem_type = get_assign_stmt_datatype(stmt_node);
    if (array_ref_on_LHS(coarray_ref, &elem_type)) ;
    else if (array_ref_on_RHS(coarray_ref, &elem_type)) ;

    WN *wp = direct_coarray_ref;
    while (wp) {
        if (WN_operator(wp) == OPR_ARRAY) {
            remote_num_array_cpnts++;
        } else if (WN_operator(wp) == OPR_ARRSECTION) {
            remote_arrsection_idx = remote_num_array_cpnts;
            remote_num_array_cpnts++;
        }
        if (wp == coarray_ref) break;
        wp = Get_Parent(wp);
    }
    remote_access.resize(remote_num_array_cpnts);
    if (remote_arrsection_idx != -1) {
        remote_arrsection = &remote_access[remote_arrsection_idx];
    }

    /* fill in remote_access structure */
    wp = direct_coarray_ref;
    int i = 0;
    while (wp) {
        if (WN_operator(wp) == OPR_ARRAY ||
            WN_operator(wp) == OPR_ARRSECTION) {
            int rank, corank, totalrank;
            if (wp == direct_coarray_ref) {
                rank = get_coarray_rank(coarray_type);
                corank = get_coarray_corank(coarray_type);
            } else {
                rank = WN_num_dim(wp);
                corank = 0;
            }

            totalrank = rank + corank;
            remote_access[i].array_ref = wp;
            remote_access[i].rank = rank;
            remote_access[i].corank = corank;
            remote_access[i].first_strided_subscript = FALSE;
            remote_access[i].stride_mult.resize(rank);
            remote_access[i].subscript.resize(rank);
            remote_access[i].subscript_stride.resize(rank);

            WN *stride_mult, *subscript;

            int elem_size = WN_element_size(wp);
            if (elem_size < 0) elem_size = -1*elem_size;

            /* subscripts */
            for (int j = 0; j < rank; j++) {
                remote_access[i].subscript[j] =
                    WN_COPY_Tree(WN_kid(wp, 2*totalrank-j));
            }

            /* stride multipliers */
            BOOL noncontig = (WN_element_size(wp) < 0);
            if (noncontig) {
                for (int j = 0; j < rank; j++) {
                    stride_mult = WN_Mpy( MTYPE_U8,
                                    WN_Intconst(Integer_type, elem_size),
                                    WN_COPY_Tree(WN_kid(wp, totalrank-j)) );
                    remote_access[i].stride_mult[j] = stride_mult;
                }
            } else if (rank > 0) {
                stride_mult = WN_Intconst(Integer_type, elem_size);
                remote_access[i].stride_mult[0] = stride_mult;
                for (int j = 1; j < rank; j++) {
                    stride_mult = WN_Mpy( MTYPE_U8,
                                    WN_COPY_Tree(stride_mult),
                                    WN_COPY_Tree(WN_kid(wp, totalrank+1-j)) );
                    remote_access[i].stride_mult[j] = stride_mult;
                }
            }

            i++;
        }

        if (wp == coarray_ref) break;
        wp = Get_Parent(wp);
    }

    /* compute base address for coarray_ref */
    WN *coarray_ref_offset = WN_Intconst(Integer_type, 0);

    /* offset for coarray_ref */
    if (access == READ_TO_LCB || access == READ_DIRECT) {
        WN *parent_load = direct_coarray_ref;
        while (parent_load &&
               WN_operator(parent_load) != OPR_LDID  &&
               WN_operator(parent_load) != OPR_ILOAD &&
               WN_operator(parent_load) != OPR_MLOAD) {
            parent_load = Get_Parent(parent_load);
            if (parent_load && WN_operator(parent_load) == OPR_ADD) {
                coarray_ref_offset = WN_Add(Integer_type,
                        coarray_ref_offset,
                        WN_COPY_Tree(WN_kid1(parent_load)));
            }
        }
        if (parent_load) {
            TY_IDX t = WN_ty(parent_load);
            while (TY_kind(t) == KIND_POINTER) {
                t = TY_pointed(t);
            }
            remote_access_elemsize = TY_size(t);

            offset = WN_offset(parent_load);
            if (offset != 0) {
                coarray_ref_offset = WN_Add(Integer_type,
                        coarray_ref_offset,
                        WN_Intconst(Integer_type, offset));
            }
        }
    } else {
        WN *parent_store = direct_coarray_ref;
        while (parent_store &&
               WN_operator(parent_store) != OPR_STID &&
               WN_operator(parent_store) != OPR_MSTORE &&
               WN_operator(parent_store) != OPR_ISTORE) {
            parent_store = Get_Parent(parent_store);
            if (parent_store && WN_operator(parent_store) == OPR_ADD) {
                coarray_ref_offset = WN_Add(Integer_type,
                        coarray_ref_offset,
                        WN_COPY_Tree(WN_kid1(parent_store)));
            }
        }
        if (parent_store) {
            TY_IDX t = WN_ty(parent_store);
            while (TY_kind(t) == KIND_POINTER) {
                t = TY_pointed(t);
            }
            remote_access_elemsize = TY_size(t);

            offset = WN_offset(parent_store);
            if (offset != 0) {
                coarray_ref_offset = WN_Add(Integer_type,
                        coarray_ref_offset,
                        WN_Intconst(Integer_type, offset));
            }
        }
    }

    for (int i = 0; i < remote_num_array_cpnts; i++) {
        for (int j = 0; j < remote_access[i].rank; j++) {
            WN *dim_offset;
            WN *sub = remote_access[i].subscript[j];
            if (WN_operator(sub) == OPR_TRIPLET) {
                dim_offset = WN_COPY_Tree(WN_kid0(sub));
            } else if (WN_operator(sub) == OPR_ARRAYEXP) {
                /* TODO: add support for this */
                dim_offset = NULL;
            } else {
                dim_offset = WN_COPY_Tree(sub);
            }

            coarray_ref_offset = WN_Add(MTYPE_U8,
                    WN_Mpy(MTYPE_U8, dim_offset,
                           WN_COPY_Tree(remote_access[i].stride_mult[j])),
                           coarray_ref_offset);
        }
    }

    coarray_base_address = WN_Add(Pointer_type,
                          WN_COPY_Tree(WN_kid0(direct_coarray_ref)),
                          coarray_ref_offset);

    /* fill in subscript strides */
    for (int i = 0; i < remote_num_array_cpnts; i++) {
        for (int j = 0; j < remote_access[i].rank; j++) {
            WN *dim_offset;
            WN *sub = remote_access[i].subscript[j];

            if (remote_arrsection_idx != j) {
                remote_access[i].subscript_stride[j] =
                    WN_Intconst(Integer_type, 1);
            } else {
                if (WN_operator(sub) == OPR_TRIPLET) {
                    remote_access[i].subscript_stride[j] =
                                        WN_COPY_Tree(WN_kid1(sub));
                    BOOL stride_is_const =
                        (WN_operator(WN_kid1(sub)) == OPR_INTCONST);
                    if (j == 0 && (!stride_is_const ||
                        stride_is_const && WN_const_val(WN_kid1(sub)) > 1)) {
                        remote_access[i].first_strided_subscript = TRUE;
                    }
                } else {
                    remote_access[i].subscript_stride[j] =
                                        WN_Intconst(Integer_type, 1);
                }
            }
        }
    }


    WN *local_inner_array = get_innermost_array(local_ref);
    if (access == READ_DIRECT || access == WRITE_DIRECT) {
        WN *wp = local_inner_array;
        i = 0;
        while (wp) {
            if (WN_operator(wp) == OPR_ARRAY) {
                local_num_array_cpnts++;
            } else if (WN_operator(wp) == OPR_ARRSECTION) {
                local_arrsection_idx = local_num_array_cpnts;
                local_num_array_cpnts++;
            }
            if (wp == local_ref) break;
            wp = Get_Parent(wp);
        }
        local_access.resize(local_num_array_cpnts);
        if (local_arrsection_idx != -1)
            local_arrsection = &local_access[local_arrsection_idx];

        /* fill in local_access structure */
        wp = local_inner_array;
        i = 0;
        while (wp) {
            if (WN_operator(wp) == OPR_ARRAY ||
                WN_operator(wp) == OPR_ARRSECTION) {
                int rank = WN_num_dim(wp);
                local_access[i].array_ref = wp;
                local_access[i].rank = rank;
                local_access[i].corank = 0;
                local_access[i].stride_mult.resize(rank);
                local_access[i].subscript.resize(rank);
                local_access[i].subscript_stride.resize(rank);

                WN *stride_mult;
                int elem_size = WN_element_size(wp);
                if (elem_size < 0) elem_size = -1*elem_size;

                /* subscripts */
                for (int j = 0; j < rank; j++) {
                    local_access[i].subscript[j] =
                        WN_COPY_Tree(WN_kid(wp, 2*rank-j));
                }

                /* stride multipliers */
                BOOL noncontig = (WN_element_size(wp) < 0);
                if (noncontig) {
                    for (int j = 0; j < rank; j++) {
                        stride_mult = WN_Mpy( MTYPE_U8,
                                WN_Intconst(Integer_type, elem_size),
                                WN_COPY_Tree(WN_kid(wp, rank-j)) );
                        local_access[i].stride_mult[j] = stride_mult;
                    }
                } else if (rank > 0) {
                    stride_mult = WN_Intconst(Integer_type, elem_size);
                    local_access[i].stride_mult[0] = stride_mult;
                    for (int j = 1; j < rank; j++) {
                        stride_mult = WN_Mpy( MTYPE_U8,
                                WN_COPY_Tree(stride_mult),
                                WN_COPY_Tree(WN_kid(wp, rank+1-j)) );
                        local_access[i].stride_mult[j] = stride_mult;
                    }
                }

                i++;
            }

            if (wp == local_ref) break;
            wp = Get_Parent(wp);
        }
    }

    /* compute base address for local ref */
    local_access_elemsize = remote_access_elemsize;
    BOOL local_array_is_lcb = FALSE;
    if (access == READ_TO_LCB || access == WRITE_FROM_LCB) {
        /* local_ref == NULL */
        local_base_address = WN_Ldid(Pointer_type, 0, LCB_st,
                                     ST_type(LCB_st));
    } else {
        /* local_ref != NULL */

        /* READ_DIRECT or WRITE_DIRECT access:
         * assume the local_ref operator is OPR_ARRSECTION if in
         * pre-lowering phase. */

        Is_True(local_ref != NULL,
                ("local_ref is NULL for DIRECT access type"));

        WN *local_ref_offset = WN_Intconst(Integer_type, 0);

        if (WN_operator(WN_kid0(local_inner_array)) == OPR_LDA ||
            WN_operator(WN_kid0(local_inner_array)) == OPR_LDID) {
            ST* local_array_st = WN_st(WN_kid0(local_inner_array));
            local_array_is_lcb = ST_is_lcb_ptr(local_array_st);
        }

        /* offset for local_ref */
        WN *wp = local_inner_array;
        WN *load_parent = get_load_parent(local_inner_array);
        WN *store_parent = get_store_parent(local_inner_array);

        if (access == WRITE_DIRECT) {
            while (wp && wp != load_parent && wp != store_parent) {
                wp = Get_Parent(wp);
                if (wp && WN_operator(wp) == OPR_ADD) {
                    local_ref_offset = WN_Add(Integer_type,
                            local_ref_offset,
                            WN_COPY_Tree(WN_kid1(wp)));
                }
            }
            if (wp == load_parent) {
                offset = WN_offset(wp);
                if (offset != 0) {
                    local_ref_offset = WN_Add(Integer_type,
                            local_ref_offset,
                            WN_Intconst(Integer_type, offset));
                }
            }
        } else { /* READ_DIRECT */
            while (wp && wp != load_parent && wp != store_parent) {
                wp = Get_Parent(wp);
                if (wp && WN_operator(wp) == OPR_ADD) {
                    local_ref_offset = WN_Add(Integer_type,
                            local_ref_offset,
                            WN_COPY_Tree(WN_kid1(wp)));
                }
            }
            if (wp == store_parent) {
                offset = WN_offset(wp);
                if (offset != 0) {
                    local_ref_offset = WN_Add(Integer_type,
                            local_ref_offset,
                            WN_Intconst(Integer_type, offset));
                }
            }
        }

        for (int i = 0; i < local_num_array_cpnts; i++) {
            for (int j = 0; j < local_access[i].rank; j++) {
                WN *dim_offset;
                WN *sub = local_access[i].subscript[j];
                if (WN_operator(sub) == OPR_TRIPLET) {
                    dim_offset = WN_COPY_Tree(WN_kid0(sub));
                } else if (WN_operator(sub) == OPR_ARRAYEXP) {
                    /* TODO: add support for this */
                    dim_offset = NULL;
                } else {
                    dim_offset = WN_COPY_Tree(sub);
                }

                local_ref_offset = WN_Add(MTYPE_U8,
                        WN_Mpy(MTYPE_U8, dim_offset,
                            WN_COPY_Tree(local_access[i].stride_mult[j])),
                        local_ref_offset);
            }
        }

        local_base_address = WN_Add(Pointer_type,
                              WN_COPY_Tree(WN_kid0(local_inner_array)),
                              local_ref_offset);

        /* fill in subscript strides */
        for (int i = 0; i < local_num_array_cpnts; i++) {
            for (int j = 0; j < local_access[i].rank; j++) {
                WN *dim_offset;
                WN *sub = local_access[i].subscript[j];

                if (local_arrsection_idx != j) {
                    local_access[i].subscript_stride[j] =
                        WN_Intconst(Integer_type, 1);
                } else {
                    if (WN_operator(sub) == OPR_TRIPLET) {
                        local_access[i].subscript_stride[j] =
                            WN_COPY_Tree(WN_kid1(sub));
                        BOOL stride_is_const =
                            (WN_operator(WN_kid1(sub)) == OPR_INTCONST);
                        if (j == 0 && (!stride_is_const ||
                                    stride_is_const &&
                                    WN_const_val(WN_kid1(sub)) > 1)) {
                            local_access[i].first_strided_subscript = TRUE;
                        }
                    } else {
                        local_access[i].subscript_stride[j] =
                            WN_Intconst(Integer_type, 1);
                    }
                }
            }
        }

    }

    BOOL coarray_ref_is_contig = TRUE;
    if (remote_arrsection) {
        if (WN_operator(remote_arrsection->stride_mult[0]) == OPR_INTCONST) {
            remote_arrsection_first_sm = WN_const_val(
                                   remote_arrsection->stride_mult[0]);
        } else {
            remote_arrsection_first_sm = INT_MAX;
        }
        coarray_ref_is_contig =
               (remote_arrsection_first_sm == remote_access_elemsize) &&
                                            is_contiguous_access(
                                                remote_arrsection->array_ref,
                                                remote_arrsection->rank);
    }

    BOOL local_ref_is_contig = TRUE;

    if (access != READ_TO_LCB && access != WRITE_FROM_LCB &&
        !local_array_is_lcb) {
        if (local_arrsection) {
            if (WN_operator(local_arrsection->stride_mult[0]) == OPR_INTCONST) {
                local_arrsection_first_sm = WN_const_val(
                                    local_arrsection->stride_mult[0]);
            } else {
                local_arrsection_first_sm = INT_MAX;
            }
            local_ref_is_contig =
                (local_arrsection_first_sm == local_access_elemsize) &&
                                          is_contiguous_access(
                                            local_arrsection->array_ref,
                                            local_arrsection->rank);
        }
    }


    if (coarray_ref_is_contig &&
        (access == READ_TO_LCB ||
        (local_ref_is_contig && access == READ_DIRECT))) {
        WN *hdl;
        if (access_handle == NULL || access_handle->handle_st == NULL)
            hdl = WN_Intconst(Pointer_type, 0);
        else
            hdl = WN_Lda(Pointer_type, 0, access_handle->handle_st);

        WN * call = Generate_Call_coarray_nbread(image,
                                       coarray_base_address,
                                       local_base_address,
                                       WN_COPY_Tree(xfer_size),
                                       hdl);
        WN_INSERT_BlockFirst( return_blk, call);
    } else if (coarray_ref_is_contig && access == WRITE_FROM_LCB) {
        WN *ordered, *hdl;

        if (access_handle != NULL && access_handle->deferred == TRUE)
            ordered = WN_Intconst(Integer_type, 0);
        else
            ordered = WN_Intconst(Integer_type, 1);

        if (access_handle == NULL || access_handle->handle_st == NULL)
            hdl = WN_Intconst(Pointer_type, 0);
        else
            hdl = WN_Lda(Pointer_type, 0, access_handle->handle_st);

        WN * call = Generate_Call_coarray_write_from_lcb(image,
                                          coarray_base_address,
                                          local_base_address,
                                          WN_COPY_Tree(xfer_size),
                                          ordered, hdl);
        WN_INSERT_BlockFirst( return_blk, call);
    } else if (coarray_ref_is_contig && local_ref_is_contig &&
               access == WRITE_DIRECT) {
        WN *ordered, *hdl;
        if (access_handle != NULL && access_handle->deferred == TRUE)
            ordered = WN_Intconst(Integer_type, 0);
        else
            ordered = WN_Intconst(Integer_type, 1);

        if (access_handle == NULL || access_handle->handle_st == NULL)
            hdl = WN_Intconst(Pointer_type, 0);
        else
            hdl = WN_Lda(Pointer_type, 0, access_handle->handle_st);
        WN * call = Generate_Call_coarray_write(image,
                                          coarray_base_address,
                                          local_base_address,
                                          WN_COPY_Tree(xfer_size),
                                          ordered, hdl);
        WN_INSERT_BlockFirst( return_blk, call);
    } else {
        int remote_rank = 0;
        vector<WN*> remote_dim_sm;
        vector<WN*> remote_subscript_strides;

        int local_rank = 0;
        vector<WN*> local_dim_sm;
        vector<WN*> local_subscript_strides;

        if (remote_arrsection) {
            remote_rank = remote_arrsection->rank;
            remote_dim_sm = remote_arrsection->stride_mult;
            remote_subscript_strides = remote_arrsection->subscript_stride;
        }

        if (local_arrsection) {
            local_rank = local_arrsection->rank;
            local_dim_sm = local_arrsection->stride_mult;
            local_subscript_strides = local_arrsection->subscript_stride;
        }

        /* generate a strided access
         * TODO: vector access for ARRAYEXP subscripts */

        TY_IDX dim_array_type = create_arr1_type(MTYPE_U8, remote_rank+1);
        ST *count_st = Gen_Temp_Named_Symbol(
                dim_array_type, "count", CLASS_VAR, SCLASS_AUTO);
        Set_ST_is_temp_var(count_st);
        ST *coarray_strides_st = Gen_Temp_Named_Symbol(
                dim_array_type, "coarray_strides", CLASS_VAR, SCLASS_AUTO);
        Set_ST_is_temp_var(coarray_strides_st);

        ST *local_strides_st = Gen_Temp_Named_Symbol(
                dim_array_type, "local_strides", CLASS_VAR, SCLASS_AUTO);
        Set_ST_is_temp_var(local_strides_st);

        OPCODE op_array = OPCODE_make_op( OPR_ARRAY, Pointer_type, MTYPE_V );

        WN *count_store = NULL;
        WN *stride_store = NULL;

        int count_idx = 0;
        int count_idx2 = 0;
        int coarray_stride_idx = 0;
        int local_stride_idx = 0;
        int coarray_ref_idx = 0;
        int local_ref_idx = 0;

        int first_sm = remote_arrsection_first_sm;
        WN *local_arrsection_ref;
        WN *count_array;

        WN *count_val = NULL;
        WN *coarray_stride_val = NULL;
        WN *local_stride_val = NULL;
        int extra_count = 0;
        if (local_arrsection != NULL)
            local_arrsection_ref = local_arrsection->array_ref;
        if (subscript_is_scalar(remote_arrsection->array_ref, coarray_ref_idx) ) {
            count_array = gen_array1_ref(op_array, dim_array_type,
                                count_st, count_idx, remote_rank+1);
            count_val = WN_Intconst(MTYPE_U8, remote_access_elemsize);
            count_idx++;
            coarray_ref_idx++;
            count_store = WN_Istore( MTYPE_U8, 0,
                    Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                    count_array, count_val );
        } else if ( subscript_is_strided(remote_arrsection->array_ref, coarray_ref_idx) ) {
            count_array = gen_array1_ref(op_array, dim_array_type,
                                count_st, count_idx, remote_rank+1);
            count_val = WN_Intconst(MTYPE_U8, remote_access_elemsize);
            count_idx++;
            count_store = WN_Istore( MTYPE_U8, 0,
                    Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                    count_array, count_val );
        } else if (local_ref != NULL ) {
            /* so first dimension of coarray reference is neither scalar or
             * strided. Check if the same is true for the local reference (not
             * an LCB). */
            if (subscript_is_scalar(local_arrsection_ref, local_ref_idx) ) {
                count_array = gen_array1_ref(op_array, dim_array_type,
                                    count_st, count_idx, remote_rank+1);
                count_val = WN_Intconst(MTYPE_U8, remote_access_elemsize);
                count_idx++;
                local_ref_idx++;
                count_store = WN_Istore( MTYPE_U8, 0,
                        Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                        count_array, count_val );
            } else if ( subscript_is_strided(local_arrsection_ref, local_ref_idx) ) {
                count_array = gen_array1_ref(op_array, dim_array_type,
                                    count_st, count_idx, remote_rank+1);
                count_val = WN_Intconst(MTYPE_U8, remote_access_elemsize);
                count_idx++;
                count_store = WN_Istore( MTYPE_U8, 0,
                        Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                        count_array, count_val );
            }
        }
        if (count_store) {
            extra_count = 1;
            WN_INSERT_BlockLast(return_blk, count_store);
        }

        /* set counts and coarray strides */
        while (coarray_ref_idx < remote_rank) {
            WN *coarray_stride_array;
            WN *stride_factor = NULL;
            count_val = NULL;
            coarray_stride_val = NULL;

            count_store = NULL;
            stride_store = NULL;


            if ( subscript_is_scalar(remote_arrsection->array_ref, coarray_ref_idx) ) {
                coarray_ref_idx++;
            } else if ( subscript_is_strided(remote_arrsection->array_ref, coarray_ref_idx) ) {
                /* setup count and coarray stride array reference */
                count_array = gen_array1_ref(op_array, dim_array_type,
                                    count_st, count_idx, remote_rank+1);

                if (count_idx == 0) {
                    if (remote_access_elemsize < first_sm) {
                        count_val =
                            WN_Intconst(MTYPE_U8, remote_access_elemsize);
                        count_store = WN_Istore( MTYPE_U8, 0,
                                Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                                count_array, count_val );
                        WN_INSERT_BlockLast(return_blk, count_store);
                        extra_count = 1;
                        count_idx++;
                        count_array = gen_array1_ref(op_array, dim_array_type,
                                count_st, count_idx, remote_rank+1);
                        count_val =
                            subscript_extent(remote_arrsection->array_ref, coarray_ref_idx);
                    } else {
                        count_val =
                            subscript_extent(remote_arrsection->array_ref, coarray_ref_idx);
                        count_val = WN_Mpy(MTYPE_U8,
                                WN_Intconst(MTYPE_U8, remote_access_elemsize),
                                count_val);
                    }
                } else {
                    count_val =
                        subscript_extent(remote_arrsection->array_ref, coarray_ref_idx);
                }

                count_store = WN_Istore( MTYPE_U8, 0,
                        Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                        count_array, count_val );

                if (count_idx > 0) {
                    coarray_stride_array = gen_array1_ref(op_array, dim_array_type,
                                        coarray_strides_st, coarray_stride_idx,
                                        remote_rank);
                    stride_factor = subscript_stride(remote_arrsection->array_ref, coarray_ref_idx);
                    coarray_stride_val = WN_Mpy( Integer_type,
                            WN_COPY_Tree(remote_dim_sm[coarray_ref_idx]),
                            stride_factor);

                    stride_store = WN_Istore( MTYPE_U8, 0,
                            Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                            coarray_stride_array, coarray_stride_val );
                    coarray_stride_idx++;
                }

                count_idx++;
                coarray_ref_idx++;
            } else {
                /* setup count and coarray stride array reference */
                count_array = gen_array1_ref(op_array, dim_array_type,
                                    count_st, count_idx, remote_rank+1);
                coarray_stride_array = gen_array1_ref(op_array, dim_array_type,
                                    coarray_strides_st, coarray_stride_idx,
                                    remote_rank);

                if (count_idx == 0) {
                    if (remote_access_elemsize < first_sm) {
                        count_val =
                            WN_Intconst(MTYPE_U8, remote_access_elemsize);
                        count_store = WN_Istore( MTYPE_U8, 0,
                                Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                                count_array, count_val );
                        WN_INSERT_BlockLast(return_blk, count_store);
                        extra_count = 1;
                        count_idx++;
                        count_array = gen_array1_ref(op_array, dim_array_type,
                                count_st, count_idx, remote_rank+1);
                        count_val =
                            subscript_extent(remote_arrsection->array_ref, coarray_ref_idx);
                    } else {
                        count_val =
                            subscript_extent(remote_arrsection->array_ref, coarray_ref_idx);
                        count_val = WN_Mpy(MTYPE_U8,
                                WN_Intconst(MTYPE_U8, remote_access_elemsize),
                                count_val);
                    }
                } else {
                    count_val =
                        subscript_extent(remote_arrsection->array_ref, coarray_ref_idx);
                }

                count_store = WN_Istore( MTYPE_U8, 0,
                        Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                        count_array, count_val );

                if (count_idx > 0) {
                    coarray_stride_val =
                        WN_COPY_Tree(remote_dim_sm[coarray_ref_idx]);

                    stride_store = WN_Istore( MTYPE_U8, 0,
                            Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                            coarray_stride_array, coarray_stride_val );
                    coarray_stride_idx++;
                }

                count_idx++;
                coarray_ref_idx++;
            }

            if (count_store) {
                WN_INSERT_BlockLast(return_blk, count_store);
                if (stride_store)
                    WN_INSERT_BlockLast(return_blk, stride_store);
            }
        }


        if (local_ref == NULL)
            local_rank = coarray_ref_idx;

        coarray_ref_idx = 0;
        count_idx = extra_count;
        local_stride_val = NULL;
        while (local_ref_idx < local_rank) {
            WN *local_stride_array;
            WN *stride_factor = NULL;
            count_array = NULL;

            stride_store = NULL;

            if (local_ref != NULL) {
                if ( subscript_is_scalar(local_arrsection_ref, local_ref_idx) ) {
                    local_ref_idx++;
                } else if ( subscript_is_strided(local_arrsection_ref, local_ref_idx) ) {
                    if (count_idx > 0) {
                        local_stride_array = gen_array1_ref(op_array,
                                        dim_array_type, local_strides_st,
                                        local_stride_idx, remote_rank);

                        stride_factor = subscript_stride(local_arrsection_ref,
                                                         local_ref_idx);
                        local_stride_val = WN_Mpy( Integer_type,
                                WN_COPY_Tree(local_dim_sm[local_ref_idx]),
                                stride_factor);

                        stride_store = WN_Istore( MTYPE_U8, 0,
                                Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                                local_stride_array, local_stride_val );

                        local_stride_idx++;
                    }

                    count_idx++;
                    local_ref_idx++;
                } else {
                    if (count_idx > 0) {
                        local_stride_array = gen_array1_ref(op_array,
                                dim_array_type, local_strides_st,
                                local_stride_idx, remote_rank);

                        local_stride_val =
                            WN_COPY_Tree(local_dim_sm[local_ref_idx]);

                        stride_store = WN_Istore( MTYPE_U8, 0,
                                Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                                local_stride_array, local_stride_val );

                        local_stride_idx++;
                    }

                    count_idx++;
                    local_ref_idx++;
                }
            } else {
                /* using LCB, so strides are based on the extents of the
                 * coarray reference */
                if ( subscript_is_scalar(remote_arrsection->array_ref, local_ref_idx) ) {
                    local_ref_idx++;
                } else {
                    if (count_idx > 0) {
                        local_stride_array = gen_array1_ref(op_array,
                                        dim_array_type, local_strides_st,
                                        local_stride_idx, remote_rank);
                        count_array = gen_array1_ref(op_array,
                                dim_array_type, count_st, count_idx2,
                                remote_rank+1);

                        if (local_stride_idx == 0) {
                            local_stride_val = WN_Iload( MTYPE_U8, 0,
                                          Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                                          count_array);
                        } else {
                            local_stride_val =
                                WN_Mpy( Integer_type,
                                        WN_COPY_Tree(local_stride_val),
                                        WN_Iload(MTYPE_U8, 0,
                                            Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                                            count_array));
                        }

                        stride_store = WN_Istore( MTYPE_U8, 0,
                                Make_Pointer_Type(MTYPE_To_TY(MTYPE_U8), FALSE),
                                local_stride_array, local_stride_val );

                        local_stride_idx++;
                        count_idx2++;
                    }

                    count_idx++;
                    local_ref_idx++;
                }
            }

            if (stride_store) {
                WN_INSERT_BlockLast(return_blk, stride_store);
            }
        }


        if (access == READ_TO_LCB || access == READ_DIRECT) {
            WN *hdl;
            if (access_handle == NULL || access_handle->handle_st == NULL)
                hdl = WN_Intconst(Pointer_type, 0);
            else
                hdl = WN_Lda(Pointer_type, 0, access_handle->handle_st);
            WN *call = Generate_Call_coarray_strided_nbread( image,
                    coarray_base_address,
                    WN_Lda(Pointer_type, 0, coarray_strides_st),
                    local_base_address,
                    WN_Lda(Pointer_type, 0, local_strides_st),
                    WN_Lda(Pointer_type, 0, count_st),
                    WN_Intconst(Integer_type, count_idx-1),
                    hdl
                   );
            WN_INSERT_BlockLast( return_blk, call );
        } else if (access == WRITE_FROM_LCB) {
            WN *ordered, *hdl;
            if (access_handle != NULL && access_handle->deferred == TRUE)
                ordered = WN_Intconst(Integer_type, 0);
            else
                ordered = WN_Intconst(Integer_type, 1);

            if (access_handle == NULL || access_handle->handle_st == NULL)
                hdl = WN_Intconst(Pointer_type, 0);
            else
                hdl = WN_Lda(Pointer_type, 0, access_handle->handle_st);
            WN *call = Generate_Call_coarray_strided_write_from_lcb( image,
                    coarray_base_address,
                    WN_Lda(Pointer_type, 0, coarray_strides_st),
                    local_base_address,
                    WN_Lda(Pointer_type, 0, local_strides_st),
                    WN_Lda(Pointer_type, 0, count_st),
                    WN_Intconst(Integer_type, count_idx-1),
                    ordered, hdl
                   );
            WN_INSERT_BlockLast( return_blk, call );
        } else if (access == WRITE_DIRECT) {
            WN *ordered, *hdl;
            if (access_handle != NULL && access_handle->deferred == TRUE)
                ordered = WN_Intconst(Integer_type, 0);
            else
                ordered = WN_Intconst(Integer_type, 1);

            if (access_handle == NULL || access_handle->handle_st == NULL)
                hdl = WN_Intconst(Pointer_type, 0);
            else
                hdl = WN_Lda(Pointer_type, 0, access_handle->handle_st);
            WN *call = Generate_Call_coarray_strided_write( image,
                    coarray_base_address,
                    WN_Lda(Pointer_type, 0, coarray_strides_st),
                    local_base_address,
                    WN_Lda(Pointer_type, 0, local_strides_st),
                    WN_Lda(Pointer_type, 0, count_st),
                    WN_Intconst(Integer_type, count_idx-1),
                    ordered, hdl
                   );
            WN_INSERT_BlockLast( return_blk, call );
        } else {
            /* shouldn't reach */
        }

        remote_dim_sm.clear();
        remote_subscript_strides.clear();
        local_dim_sm.clear();
        local_subscript_strides.clear();
    }

    for (int i = 0; i < remote_access.size(); i++) {
        for (int j = 0; j < remote_access[i].rank; j++) {
            if (remote_access[i].stride_mult[j])
                WN_Delete(remote_access[i].stride_mult[j]);
            if (remote_access[i].subscript[j])
                WN_Delete(remote_access[i].subscript[j]);
            if (remote_access[i].stride_mult[j])
                WN_Delete(remote_access[i].subscript_stride[j]);
        }
        remote_access[i].stride_mult.clear();
        remote_access[i].subscript.clear();
        remote_access[i].subscript_stride.clear();
    }
    remote_access.clear();

    if (local_ref) {
        for (int i = 0; i < local_access.size(); i++) {
            for (int j = 0; j < local_access[i].rank; j++) {
                if (local_access[i].stride_mult[j])
                    WN_Delete(local_access[i].stride_mult[j]);
                if (local_access[i].subscript[j])
                    WN_Delete(local_access[i].subscript[j]);
                if (local_access[i].stride_mult[j])
                    WN_Delete(local_access[i].subscript_stride[j]);
            }
            local_access[i].stride_mult.clear();
            local_access[i].subscript.clear();
            local_access[i].subscript_stride.clear();
        }
        local_access.clear();
    }

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

static void substitute_lcb(WN *remote_access,
                           ST *lcbtemp, WN *wn_arrayexp,
                           WN **replace_wn)
{
    TY_IDX ty1, ty2;
    TY_IDX elem_type;
    INT elem_size;
    ty1 = get_array_type_from_tree(remote_access, &ty2);

    Is_True( ty1 != TY_IDX_ZERO,
            (" couldn't find array type in substitute_lcb "));

    if ( is_dope(ty1) )
        ty1 = TY_pointed(FLD_type(TY_fld(ty1)));

    elem_type = TY_pointed(ST_type(lcbtemp));
    elem_size = TY_size(elem_type);

    /* resolve to a base element type */
    while ( TY_kind(elem_type) == KIND_ARRAY ) {
        elem_type = TY_etype(elem_type);
        elem_size = TY_size(elem_type);
        if (TY_kind(elem_type) == KIND_STRUCT) {
            elem_type = MTYPE_To_TY(MTYPE_U1);
            break;
        }
    }

    /* substitute lcbtemp into original remote_access node. */

    WN *arrsection = get_inner_arrsection(remote_access);
    if (arrsection == NULL) {
        *replace_wn =   WN_Ldid(Pointer_type, 0,
                                lcbtemp, ST_type(lcbtemp));
    } else  {

        INT8 rank, corank, totalrank;
        if (is_coarray_type(ty2)) {
            rank = get_coarray_rank(ty2);
            corank = get_coarray_corank(ty2);
            totalrank = rank + corank;
        } else {
            rank = get_array_rank(ty2);
            corank = 0;
            totalrank = rank;
        }

        *replace_wn  = WN_Create( OPCODE_make_op(OPR_ARRSECTION,
                    WN_rtype(arrsection),
                    WN_desc(arrsection)), 1+2*rank);

        /* assume contiguous access in LCB */
        WN_element_size(*replace_wn) = elem_size;

        WN_array_base(*replace_wn) =  WN_Ldid(Pointer_type, 0, lcbtemp,
                                              ST_type(lcbtemp));

        /* set sizes and index for each dimension of tmp buff */
        INT8 j = 1;
        for (INT8 i = 1; i <= rank; i++) {
            WN *wn_ext;
            /* TODO: handle other aggregate operators as well */
            if (WN_operator(WN_array_index(
                     arrsection, corank+i-1)) == OPR_TRIPLET) {
              wn_ext = WN_COPY_Tree(WN_kid2(
                           WN_array_index(arrsection,corank+i-1)));
            } else  {
              wn_ext = WN_Intconst(Integer_type, 1);
            }

            WN_array_dim(*replace_wn,i-1) = wn_ext;

            /* set subscript for dimension i */
            WN_array_index(*replace_wn,i-1) =
                WN_COPY_Tree(WN_array_index(arrsection, corank+i-1));

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
static BOOL is_contiguous_access(WN *array_ref, INT8 rank)
{
    INT8 num_dim;
    TY_IDX coarray_rank;
    FmtAssert(WN_operator(array_ref) != OPR_ARRAY ||
            WN_operator(array_ref) != OPR_ARRSECTION,
            ("Unexpected operator for array reference"));

    num_dim = WN_num_dim(array_ref);

    INT* access_range = (INT *)malloc(num_dim*sizeof(INT));
    INT* extent = (INT *)malloc(num_dim*sizeof(INT));

    BOOL requires_complete_access = FALSE;

    if (WN_operator(array_ref) == OPR_ARRSECTION) {
        for (INT8 i = rank-1; i >= 0; i--) {
            WN *sub = WN_array_index(array_ref, num_dim-i-1);
            /* TODO: handle other aggregate operators as well */
            if (WN_operator(sub) == OPR_TRIPLET) {
                if (WN_operator(WN_kid1(sub)) != OPR_INTCONST ||
                    (WN_operator(WN_kid1(sub)) == OPR_INTCONST &&
                    WN_const_val(WN_kid1(sub)) != 1))
                    return FALSE;

                if (WN_operator(WN_kid2(sub)) != OPR_INTCONST ||
                    WN_operator(WN_kid0(sub)) != OPR_INTCONST) {
                    /* always less than extent for this dim */
                    access_range[i] = -1;
                } else {
                    access_range[i] = WN_const_val(WN_kid2(sub));
                }
            } else {
                access_range[i] = 1;
            }

            /* calculate extents */
            WN *size = WN_array_dim(array_ref, num_dim-i-1);
            if (WN_operator(size) == OPR_INTCONST) {
                extent[i] = WN_const_val(size);
            } else {
                /* conservatively set extent to maximum size if bounds aren't
                 * statically known */
                extent[i] = INT_MAX;
            }

            if ((access_range[i] < extent[i]) && requires_complete_access)
                return FALSE;
            else if (access_range[i] != 1)  {
                requires_complete_access = TRUE;
            }
            else continue;
        }
    }

    free(access_range);
    free(extent);

    return TRUE;
}

static BOOL is_vector_access(WN *array_ref)
{
    WN_TREE_CONTAINER<POST_ORDER> wcpost(array_ref);
    WN *wn;
    INT8 num_dim, rank;
    BOOL is_vector = FALSE;


    /* find inner-most ARRSECTION node, if it exists */

    wn = wcpost.begin().Wn();
    while (wn && WN_operator(wn) != OPR_ARRSECTION) {
        wn = Get_Parent(wn);
    }

    if (wn == NULL) return FALSE;
    array_ref = wn;

    TY_IDX arr_type = get_array_type_from_tree(array_ref);
    if ( is_dope(arr_type) || TY_kind(arr_type) == KIND_POINTER)
        arr_type = TY_pointed(FLD_type(TY_fld(arr_type)));

    if (TY_kind(arr_type) != KIND_ARRAY)
        return FALSE;

    Is_True(TY_kind(arr_type) == KIND_ARRAY,
       ("arr_type for local_ref is not KIND_ARRAY for DIRECT access type"));

    if (is_coarray_type(arr_type))
        rank = get_coarray_rank(arr_type);
    else
        rank = get_array_rank(arr_type);

    num_dim = WN_num_dim(array_ref);

    /* its a vector access if one of the subscript nodes is ARRAYEXP */
    if (WN_operator(array_ref) == OPR_ARRSECTION) {
        for (INT8 i = rank-1; i >= 0; i--) {
            WN *sub = WN_array_index(array_ref, num_dim-i-1);
            if (WN_operator(sub) == OPR_ARRAYEXP) {
                return TRUE;
            }
        }
    }

    return FALSE;
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

    if (TY_size(elem_ty_idx))
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
          (WN_operator(WN_kid0(rhs)) == OPR_ILOAD ||
          WN_operator(WN_kid0(rhs)) == OPR_MLOAD));
}

static void array_ref_remove_cosubscripts(WN *arr_parent, WN *arr, TY_IDX ty)
{
  ST *array_st;
  INT8 rank, corank, totalrank;

  if ((WN_operator(arr) != OPR_ARRAY) &&
      (WN_operator(arr) != OPR_ARRSECTION))
    return;

  if ( is_dope(ty) ) {
    ty = TY_pointed(FLD_type(TY_fld(ty)));
    if (!is_coarray_type(ty))
      return;
    rank = get_coarray_rank(ty);
    corank = get_coarray_corank(ty);
  } else  {
    /* break if not coarray */
    if (!is_coarray_type(ty))
      return;

    rank = get_coarray_rank(ty);
    corank = get_coarray_corank(ty);
  }

  /* check array depth matches in case its a coscalar of array type */
  if (rank == 0)  {
      TY_IDX ty1 = Ty_Table[ty].u2.etype;
      WN *p = Get_Parent(arr);
      while (TY_kind(ty1) == KIND_ARRAY) {
          ty1 = Ty_Table[ty1].u2.etype;
          if (!p || (WN_operator(p) != OPR_ARRAY &&
              WN_operator(p) != OPR_ARRSECTION)) {
              return;
          }
          p = Get_Parent(p);
      }
  }


  /* break if not cosubscripted */
  if (WN_kid_count(arr) == (1+2*rank))
    return;

  /* if it is rank 0, then replace the array reference with kid 0 */
  if (rank == 0) {
      WN *tmp;
      tmp = WN_COPY_Tree(WN_kid0(arr));

      if ((WN_operator(arr_parent) == OPR_ARRAY ||
          WN_operator(arr_parent) == OPR_ARRSECTION) &&
          (WN_operator(tmp) == OPR_LDA ||
           WN_operator(tmp) == OPR_LDID)) {

          TY_IDX ty = WN_ty(tmp);
          if (TY_kind(ty) == KIND_POINTER) {
              ty = TY_pointed(ty);
          }
          if (TY_kind(ty) == KIND_ARRAY) {
              ty = Ty_Table[ty].u2.etype;
          }
          ty = get_type_at_offset(ty, WN_offset(tmp), TRUE, TRUE);
          ty = Make_Pointer_Type(ty);
          WN_set_ty(tmp, ty);

      }
      if (WN_kid1(arr_parent) == arr) {
          WN_kid1(arr_parent) = WN_COPY_Tree(tmp);
      } else if (WN_kid0(arr_parent) == arr) {
          WN_kid0(arr_parent) = WN_COPY_Tree(tmp);
      } else {
          Is_True(0, ("unexpected parent tree in array_ref_remove_cosubscripts"));
      }
      WN_DELETE_Tree(arr);
      return;
  }

  /* confirmed that this is a image-selecting array reference. now remove the
   * cosubscripts. Remove kids 1...corank and totalrank+1...totalrank+corank */
  totalrank = rank + corank;
  for (INT8 i=0; i < rank; i++) {
      WN_kid(arr,i+1) = WN_kid(arr,i+corank+1);
  }
  for (INT8 i=0; i < rank; i++) {
      WN_kid(arr,i+rank+1) = WN_kid(arr,i+totalrank+1+corank);
  }
  WN_set_kid_count(arr,1+2*rank);
  for (INT8 i=1+2*rank; i < 1+2*totalrank; i++) {
      WN_kid(arr,i) = NULL;
  }
}

static WN * array_ref_is_coindexed(WN *arr, TY_IDX ty)
{
  WN *base_addr;
  ST *array_st;
  INT8 rank, corank, totalrank;

  if ((WN_operator(arr) != OPR_ARRAY) &&
      (WN_operator(arr) != OPR_ARRSECTION))
    return 0;

  if ( is_dope(ty) ) {
    ty = TY_pointed(FLD_type(TY_fld(ty)));
    if (!is_coarray_type(ty))
      return 0;
    rank = get_coarray_rank(ty);
    corank = get_coarray_corank(ty);
  } else  {
    /* break if not coarray */
    if (!is_coarray_type(ty))
      return 0;

    rank = get_coarray_rank(ty);
    corank = get_coarray_corank(ty);
  }

  /* check array depth matches in case its a coscalar of array type */
  if (rank == 0)  {
      TY_IDX ty1 = Ty_Table[ty].u2.etype;
      WN *p = Get_Parent(arr);
      while (TY_kind(ty1) == KIND_ARRAY) {
          ty1 = Ty_Table[ty1].u2.etype;
          if (!p || (WN_operator(p) != OPR_ARRAY &&
              WN_operator(p) != OPR_ARRSECTION)) {
              return 0;
          }
          p = Get_Parent(p);
      }
  }


  /* break if not cosubscripted */
  if (WN_kid_count(arr) == (1+2*rank))
    return 0;

  /* this is a image-selecting array reference. return expression that
   * computes selected image */

  totalrank = rank + corank;

  WN **wn_costr_m = (WN **)malloc(corank*sizeof(WN*));
  wn_costr_m[0] = WN_Intconst(Integer_type, 1);
  BOOL noncontig = (WN_element_size(arr) < 0);
  for (INT8 i=1; i < corank; i++) {
      if (noncontig)
          wn_costr_m[i] = WN_COPY_Tree(WN_kid(arr,corank-i));
      else
          wn_costr_m[i] = WN_Mpy( MTYPE_I8,
                  WN_COPY_Tree(wn_costr_m[i-1]),
                  WN_COPY_Tree(WN_kid(arr,corank+1-i)));
  }

  WN *image = WN_Intconst(MTYPE_U8, 1);
  for (INT8 i=0; i < corank; i++) {
      image = WN_Add(MTYPE_U8,
              WN_Mpy(MTYPE_I8,
                  WN_COPY_Tree(WN_kid(arr, 2*totalrank-rank-i)),
                  WN_COPY_Tree(wn_costr_m[i]) ),
              image);
  }


  free(wn_costr_m);
  return image;
}

static int stmt_in_initializer_list(WN *stmt)
{
    int i;

    /* check if statement is already in the delete list */
    for (i = 0; i != caf_initializer_list.size(); ++i) {
        if (caf_initializer_list[i].stmt == stmt) {
            return 1;
        }
    }

    return 0;
}

static void move_stmts_from_initializer_list(WN *wn, WN *blk)
{
  for (int i = 0; i != caf_initializer_list.size(); ++i) {
    CAF_STMT_NODE stmt_to_init = caf_initializer_list[i];
    Is_True(stmt_to_init.stmt && stmt_to_init.blk,
        ("invalid CAF_STMT_NODE object in caf_initializer_list"));
    WN *stmt = WN_EXTRACT_FromBlock(stmt_to_init.blk, stmt_to_init.stmt);
    WN_INSERT_BlockAfter(blk, wn, stmt);
    wn = stmt;
  }

  caf_initializer_list.clear();
}

static int add_caf_stmt_to_initializer_list(WN *stmt, WN *blk)
{
  int i;
  CAF_STMT_NODE init_stmt;
  int ok = 1;

  /* check if statement is already in the initializer list */
  ok = ! (stmt_in_initializer_list(stmt));

  /* traverse up tree until we find node who's parent is blk */
  WN *p = stmt;
  while (p && Get_Parent(p) != blk) {
      p = Get_Parent(p);
  }

  Is_True(p && Get_Parent(p) == blk,
          ("can't figure out what to do with this statement"));

  if (p == NULL) return 0;

  /* check (again) if statement is already in the initializer list */
  ok = ! (stmt_in_initializer_list(p));

  if (ok) {
    init_stmt.stmt = p;
    init_stmt.blk = blk;
    caf_initializer_list.push_back(init_stmt);
    return 1;
  } else {
    return 0;
  }
}


static int stmt_in_delete_list(WN *stmt)
{
    int i;

    /* check if statement is already in the delete list */
    for (i = 0; i != caf_delete_list.size(); ++i) {
        if (caf_delete_list[i].stmt == stmt) {
            return 1;
        }
    }

    return 0;
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
  ok_to_delete = ! (stmt_in_delete_list(stmt));

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
Generate_Call_target_alloc(WN *xfer_size, WN *target_ptr)
{
    WN *call = Generate_Call_Shell( TARGET_ALLOC, MTYPE_V, 2);
    WN_actual( call, 0 ) =
        Generate_Param( xfer_size, WN_PARM_BY_VALUE);
    WN_actual( call, 1 ) =
        Generate_Param( target_ptr, WN_PARM_BY_REFERENCE);

    return call;
}

static WN *
Generate_Call_target_dealloc(WN *target_ptr)
{
    WN *call = Generate_Call_Shell( TARGET_DEALLOC, MTYPE_V, 1);
    WN_actual( call, 0 ) =
        Generate_Param( target_ptr, WN_PARM_BY_REFERENCE);

    return call;
}

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

static WN * Generate_Call_coarray_wait_all()
{
    WN *call = Generate_Call_Shell( COARRAY_WAIT_ALL, MTYPE_V, 0);

    return call;
}

static WN * Generate_Call_coarray_wait(WN *hdl, BOOL with_guard)
{
    WN *test, *if_wn;
    WN *call = Generate_Call_Shell( COARRAY_WAIT, MTYPE_V, 1);
    WN_actual( call, 0 ) =
        Generate_Param( hdl, WN_PARM_BY_REFERENCE);

    if (with_guard) {
        WN *deref_hdl = WN_Iload(Pointer_type, 0, WN_type(hdl),
                                 WN_COPY_Tree(hdl), 0);
        test = WN_NE(MTYPE_U8, deref_hdl, WN_Intconst(MTYPE_U8, 0));
        if_wn = WN_CreateIf(test, WN_CreateBlock(), WN_CreateBlock());
        WN_INSERT_BlockLast( WN_then(if_wn), call);
        return if_wn;
    }

    return call;
}

static WN *
Generate_Call_coarray_nbread(WN *image, WN *src, WN *dest, WN *nbytes, WN *hdl)
{
    WN *call = Generate_Call_Shell( COARRAY_NBREAD, MTYPE_V, 5);
    WN_actual( call, 0 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);
    WN_actual( call, 1 ) =
        Generate_Param( src, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( dest, WN_PARM_BY_REFERENCE);
    WN_actual( call, 3 ) =
        Generate_Param( nbytes, WN_PARM_BY_VALUE);
    WN_actual( call, 4 ) =
        Generate_Param( hdl, WN_PARM_BY_REFERENCE);

    return call;
}

static WN *
Generate_Call_coarray_read(WN *image, WN *src, WN *dest, WN *nbytes)
{
    WN *call = Generate_Call_Shell( COARRAY_READ, MTYPE_V, 4);
    WN_actual( call, 0 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);
    WN_actual( call, 1 ) =
        Generate_Param( src, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( dest, WN_PARM_BY_REFERENCE);
    WN_actual( call, 3 ) =
        Generate_Param( nbytes, WN_PARM_BY_VALUE);

    return call;
}

static WN *
Generate_Call_coarray_write_from_lcb(WN *image, WN *dest, WN *src, WN *nbytes,
                                     WN *ordered, WN *hdl)
{
    WN *call = Generate_Call_Shell( COARRAY_WRITE_FROM_LCB, MTYPE_V, 6);
    WN_actual( call, 0 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);
    WN_actual( call, 1 ) =
        Generate_Param( dest, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( src, WN_PARM_BY_REFERENCE);
    WN_actual( call, 3 ) =
        Generate_Param( nbytes, WN_PARM_BY_VALUE);
    WN_actual( call, 4 ) =
        Generate_Param( ordered, WN_PARM_BY_VALUE);
    WN_actual( call, 5 ) =
        Generate_Param( hdl, WN_PARM_BY_REFERENCE);

    return call;
}

static WN *
Generate_Call_coarray_write(WN *image, WN *dest, WN *src,
                            WN *nbytes, WN *ordered, WN *hdl)
{
    WN *call = Generate_Call_Shell( COARRAY_WRITE, MTYPE_V, 6);
    WN_actual( call, 0 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);
    WN_actual( call, 1 ) =
        Generate_Param( dest, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( src, WN_PARM_BY_REFERENCE);
    WN_actual( call, 3 ) =
        Generate_Param( nbytes, WN_PARM_BY_VALUE);
    WN_actual( call, 4 ) =
        Generate_Param( ordered, WN_PARM_BY_VALUE);
    WN_actual( call, 5 ) =
        Generate_Param( hdl, WN_PARM_BY_REFERENCE);

    return call;
}

static WN * Generate_Call_coarray_strided_nbread(WN *image, WN *src,
                                    WN *src_strides, WN *dest,
                                    WN *dest_strides, WN *count,
                                    WN *stride_levels, WN *hdl)
{
    WN *call = Generate_Call_Shell( COARRAY_STRIDED_NBREAD, MTYPE_V, 8);
    WN_actual( call, 0 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);
    WN_actual( call, 1 ) =
        Generate_Param( src, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( src_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 3 ) =
        Generate_Param( dest, WN_PARM_BY_REFERENCE);
    WN_actual( call, 4 ) =
        Generate_Param( dest_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 5 ) =
        Generate_Param( count, WN_PARM_BY_REFERENCE);
    WN_actual( call, 6 ) =
        Generate_Param( stride_levels, WN_PARM_BY_VALUE);
    WN_actual( call, 7 ) =
        Generate_Param( hdl, WN_PARM_BY_REFERENCE);

    return call;
}

static WN * Generate_Call_coarray_strided_read(WN *image, WN *src,
                                    WN *src_strides, WN *dest,
                                    WN *dest_strides, WN *count,
                                    WN *stride_levels)
{
    WN *call = Generate_Call_Shell( COARRAY_STRIDED_READ, MTYPE_V, 7);
    WN_actual( call, 0 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);
    WN_actual( call, 1 ) =
        Generate_Param( src, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( src_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 3 ) =
        Generate_Param( dest, WN_PARM_BY_REFERENCE);
    WN_actual( call, 4 ) =
        Generate_Param( dest_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 5 ) =
        Generate_Param( count, WN_PARM_BY_REFERENCE);
    WN_actual( call, 6 ) =
        Generate_Param( stride_levels, WN_PARM_BY_VALUE);

    return call;
}

static WN * Generate_Call_coarray_strided_write_from_lcb(WN *image, WN *dest,
                                    WN *dest_strides, WN *src,
                                    WN *src_strides, WN *count,
                                    WN *stride_levels,
                                    WN *ordered, WN *hdl)
{
    WN *call = Generate_Call_Shell( COARRAY_STRIDED_WRITE_FROM_LCB, MTYPE_V, 9);
    WN_actual( call, 0 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);
    WN_actual( call, 1 ) =
        Generate_Param( dest, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( dest_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 3 ) =
        Generate_Param( src, WN_PARM_BY_REFERENCE);
    WN_actual( call, 4 ) =
        Generate_Param( src_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 5 ) =
        Generate_Param( count, WN_PARM_BY_REFERENCE);
    WN_actual( call, 6 ) =
        Generate_Param( stride_levels, WN_PARM_BY_VALUE);
    WN_actual( call, 7 ) =
        Generate_Param( ordered, WN_PARM_BY_VALUE);
    WN_actual( call, 8 ) =
        Generate_Param( hdl, WN_PARM_BY_REFERENCE);

    return call;
}

static WN * Generate_Call_coarray_strided_write(WN *image, WN *dest,
                                    WN *dest_strides, WN *src,
                                    WN *src_strides, WN *count,
                                    WN *stride_levels, WN *ordered,
                                    WN *hdl)
{
    WN *call = Generate_Call_Shell( COARRAY_STRIDED_WRITE, MTYPE_V, 9);
    WN_actual( call, 0 ) =
        Generate_Param( image, WN_PARM_BY_VALUE);
    WN_actual( call, 1 ) =
        Generate_Param( dest, WN_PARM_BY_REFERENCE);
    WN_actual( call, 2 ) =
        Generate_Param( dest_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 3 ) =
        Generate_Param( src, WN_PARM_BY_REFERENCE);
    WN_actual( call, 4 ) =
        Generate_Param( src_strides, WN_PARM_BY_REFERENCE);
    WN_actual( call, 5 ) =
        Generate_Param( count, WN_PARM_BY_REFERENCE);
    WN_actual( call, 6 ) =
        Generate_Param( stride_levels, WN_PARM_BY_VALUE);
    WN_actual( call, 7 ) =
        Generate_Param( ordered, WN_PARM_BY_VALUE);
    WN_actual( call, 8 ) =
        Generate_Param( hdl, WN_PARM_BY_REFERENCE);

    return call;
}

/*
 * init_caf_extern_symbols
 *
 * Generates global extern symbols for:
 *    _this_image (this_image_st)
 *    _num_images (num_images_st)
 *    _log2_images (num_images_st)
 *    _rem_images (num_images_st)
 *
 * if they have not already been created.
 */
void init_caf_extern_symbols()
{

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

    if (log2_images_st == NULL) {
        log2_images_st = New_ST( GLOBAL_SYMTAB );
        ST_Init( log2_images_st, Save_Str( "_log2_images" ),
                CLASS_VAR, SCLASS_EXTERN, EXPORT_PREEMPTIBLE,
                MTYPE_To_TY(MTYPE_U8));
    }

    if (rem_images_st == NULL) {
        rem_images_st = New_ST( GLOBAL_SYMTAB );
        ST_Init( rem_images_st, Save_Str( "_rem_images" ),
                CLASS_VAR, SCLASS_EXTERN, EXPORT_PREEMPTIBLE,
                MTYPE_To_TY(MTYPE_U8));
    }
}

/*
 * handle_caf_call_stmts
 *
 * Certain call statements generated in front-end need to be further processed
 * in back-end. This includes, currently:
 *
 *    _THIS_IMAGE0: replace with global symbol _this_image
 *    _NUM_IMAGES: replace with global symbol _num_images
 *    _LOG2_IMAGES: replace with global symbol _log2_images
 *    _REM_IMAGES: replace with global symbol _rem_images
 *    _SYNC_IMAGES: replace arguments with (array-list, #array-list)
 *
 */
static void handle_caf_call_stmts(
    WN_TREE_CONTAINER<PRE_ORDER>::iterator
    wipre, WN **wn_next_p)
{
    ST *func_st;
    WN *wn_next, *wn;

    wn_next = *wn_next_p;
    wn = wipre.Wn();

    func_st = WN_st(wn);

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
    } else if ( NAME_IS(func_st, "_NUM_IMAGES") ) {
        wn_next = WN_next(wn);
        wipre.Delete();
        Is_True( WN_operator(wn_next) == OPR_STID,
                ("Expected STID in IR after _num_images() call"));
        WN_Delete( WN_kid0(wn_next) );
        WN_kid0(wn_next) = WN_Ldid(MTYPE_U8, 0,
                num_images_st, ST_type(num_images_st));
    } else if ( NAME_IS(func_st, "_LOG2_IMAGES") ) {
        wn_next = WN_next(wn);
        wipre.Delete();
        Is_True( WN_operator(wn_next) == OPR_STID,
                ("Expected STID in IR after _log2_images() call"));
        WN_Delete( WN_kid0(wn_next) );
        WN_kid0(wn_next) = WN_Ldid(MTYPE_U8, 0,
                log2_images_st, ST_type(log2_images_st));
    } else if ( NAME_IS(func_st, "_REM_IMAGES") ) {
        wn_next = WN_next(wn);
        wipre.Delete();
        Is_True( WN_operator(wn_next) == OPR_STID,
                ("Expected STID in IR after _rem_images() call"));
        WN_Delete( WN_kid0(wn_next) );
        WN_kid0(wn_next) = WN_Ldid(MTYPE_U8, 0,
                rem_images_st, ST_type(rem_images_st));
    } else if ( NAME_IS(func_st, "_SYNC_IMAGES") ) {
        wn_next = WN_next(wn);

        Is_True( WN_num_actuals(wn) == 6,
                ("Expected 6 args for sync_images from FE"));

        ST *st1 = WN_st(WN_kid0(WN_actual(wn,0)));
        TY_IDX ty1 = ST_type(st1);
        if (TY_kind(ty1) == KIND_POINTER)
            ty1 = TY_pointed(ty1);

        /* type of first argument may not be a dope vector if
         * Coarray_Prelower phase is run a second time on this PU
         * (e.g. after ipa-link) */
        /*
        Is_True( is_dope(ty1),
                ("Expected sync_images arg 1 to be a dope from FE"));
                */
        if (!is_dope(ty1)) return;

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
}

static void
dump_ty (TY_IDX ty_idx)
{
	TY& ty = Ty_Table[ty_idx];
	ty.Print(stdout);
}

static void uncoindex_expr(WN *expr)
{
    WN_TREE_CONTAINER<POST_ORDER> wcpost(expr);
    WN *uncoindexed_expr = NULL;
    WN *wn;
    TY_IDX type;
    WN_OFFSET ofst = 0;
    WN *img = NULL;

    /* start from left most leaf and search up the tree for a coarray access
     * */
    wn = wcpost.begin().Wn();
    type = TY_IDX_ZERO;
    while (wn) {
        WN *parent = Get_Parent(wn);

        switch (WN_operator(wn)) {
            case OPR_ARRAY:
            case OPR_ARRSECTION:
                if (type == TY_IDX_ZERO)
                    break;

                type = get_type_at_offset(type, ofst, TRUE, TRUE);

                while (TY_kind(type) == KIND_POINTER)
                    type = TY_pointed(type);

                if ( is_coarray_type(type) ) {
                    array_ref_remove_cosubscripts(parent, wn, type);
                    return;
                }

                break;

            case OPR_LDA:
            case OPR_LDID:
                type = WN_ty(wn);
                while (TY_kind(type) == KIND_POINTER)
                    type = TY_pointed(type);

                ofst = 0;
                break;

            case OPR_ILOAD:
                type = WN_ty(wn);
                while (TY_kind(type) == KIND_POINTER)
                    type = TY_pointed(type);

                ofst = 0;
                break;

            case OPR_ADD:
                ofst += WN_const_val(WN_kid1(wn));
                break;
        }
        /* if back at original expression node, break */
        if (wn == expr) break;

        wn = parent;
    }

}

/*
 * expr_is_coindexed
 *
 * input: expr - a WHIRL expression tree
 * output:
 *        image - WHIRL node for selected image
 *        coarray_type - type for coarray, if found
 *        direct_coarray_ref (optional) - the ARRAY/ARRSECTION
 *                            node for the coarray (symbol at
 *                            kid0 will be the coarray symbol)
 *        return value: coindexed array expression within input parameter
 *                      expr
 */
static WN *expr_is_coindexed(WN *expr, WN **image, TY_IDX *coarray_type,
                             WN **direct_coarray_ref)
{
    WN_TREE_CONTAINER<POST_ORDER> wcpost(expr);
    WN *wn;
    TY_IDX type;
    WN_OFFSET ofst = 0;
    WN *img = NULL;

    *image = NULL;
    *coarray_type = TY_IDX_ZERO;

    /* start from left most leaf and search up the tree for a coarray access
     * */
    wn = wcpost.begin().Wn();
    type = TY_IDX_ZERO;
    while (wn) {
        WN *parent = Get_Parent(wn);

        switch (WN_operator(wn)) {
            case OPR_ARRAY:
            case OPR_ARRSECTION:
                if (type == TY_IDX_ZERO)
                    break;

                type = get_type_at_offset(type, ofst, TRUE, TRUE);

                while (TY_kind(type) == KIND_POINTER)
                    type = TY_pointed(type);

                if ( is_coarray_type(type) ) {
                    *image = array_ref_is_coindexed(wn, type);
                    /* TODO: double check that we want to return if wn is not
                     * coindexed */
                    if (*image) {
                        WN *enclosing_ref = get_enclosing_direct_arr_ref(wn);
                        *coarray_type = type;
                        if (direct_coarray_ref != NULL)
                            *direct_coarray_ref = wn;
                        return enclosing_ref;
                    } else {
                        return NULL;
                    }
                }

                break;

            case OPR_LDA:
            case OPR_LDID:
                type = WN_ty(wn);
                while (TY_kind(type) == KIND_POINTER)
                    type = TY_pointed(type);

                ofst = 0;
                break;

            case OPR_ILOAD:
                //type = get_type_at_offset(type, ofst, FALSE, FALSE);
                type = WN_ty(wn);
                while (TY_kind(type) == KIND_POINTER)
                    type = TY_pointed(type);

                ofst = 0;
                break;

            case OPR_ADD:
                ofst += WN_const_val(WN_kid1(wn));
                break;
        }
        /* if back at original expression node, break */
        if (wn == expr) break;

        wn = parent;
    }


    return NULL;
}

static void Parentize(WN *wn)
{
  if (!OPCODE_is_leaf (WN_opcode (wn))) {
    if (WN_opcode(wn) == OPC_BLOCK) {
      WN* kid = WN_first (wn);
      while (kid) {
        Set_Parent (kid, wn);
        Parentize (kid);
        kid = WN_next (kid);
      }
    }
    else {
      INT kidno;
      WN* kid;
      for (kidno=0; kidno<WN_kid_count(wn); kidno++) {
        kid = WN_kid (wn, kidno);
        if (kid) {
          Set_Parent (kid, wn);
          Parentize (kid);
        }
      }
    }
  }
}

static void Coarray_Prelower_Init()
{
    MEM_POOL_Initialize(&caf_pool, "CAF Prelowering Pool", FALSE);
}

static TY_IDX get_type_at_offset (TY_IDX ty, WN_OFFSET offset, BOOL stop_at_array,
                                  BOOL stop_at_coarray)
{
  switch (TY_kind(ty)) {
    case KIND_STRUCT:
      {
        // return type of field
        FLD_ITER fld_iter = Make_fld_iter(TY_fld(ty));
        do {
          FLD_HANDLE fld(fld_iter);
          if (Is_Composite_Type(FLD_type(fld))
              && offset > FLD_ofst(fld)
              && offset < FLD_ofst(fld) + TY_size(FLD_type(fld))) {
            return get_type_at_offset (FLD_type(fld), offset - FLD_ofst(fld),
                                      stop_at_array, stop_at_coarray);
          }
          if (FLD_ofst(fld) == offset) {
            return get_type_at_offset (FLD_type(fld), offset - FLD_ofst(fld),
                                      stop_at_array, stop_at_coarray);
          }
        } while (!FLD_last_field(fld_iter++));
        FmtAssert(FALSE, ("couldn't find matching field"));
      }
        break;

    case KIND_ARRAY:
      // return type of elements, recursing in case array of structs
      if (offset == 0 ) {
          if (stop_at_coarray && is_coarray_type(ty)) {
              return ty;
          } else if (stop_at_array) {
              return ty;
          }
      }

      return get_type_at_offset (TY_etype(ty),
              offset % TY_size(TY_etype(ty)), stop_at_array,
              stop_at_coarray);

    case KIND_POINTER:
        return get_type_at_offset(TY_pointed(ty),
                offset, stop_at_array, stop_at_coarray);

    default:
      return ty;
  }
}

static WN *find_outer_array(WN *start, WN *end)
{
    WN *wn;
    wn = start;
    while (wn != end) {
        if (WN_operator(wn) == OPR_ARRAY || WN_operator(wn) == OPR_ARRSECTION)
            return wn;

        wn = Get_Parent(wn);
    }

    if (WN_operator(wn) == OPR_ARRAY || WN_operator(wn) == OPR_ARRSECTION)
        return wn;
    else
        return NULL;
}

/*
 * array_ref_on_LHS
 *
 * Input: wn, an ARRAY or ARRSECTION node.
 * Output: return value says if the input wn is on LHS of its parent
 * which is an assignment statement.
 */
static BOOL array_ref_on_LHS(WN *wn, TY_IDX *ty)
{
    WN *parent;
    Is_True(WN_operator(wn) != OPR_ARRAY ||
            WN_operator(wn) != OPR_ARRSECTION,
            ("Unexpected operator for array_ref_on_LHS arg"));

    parent = Get_Parent(wn);

    if (WN_operator(parent) == OPR_ISTORE) {
        TYPE_ID desc = WN_desc(parent);
        if (desc == MTYPE_M) {
            // *ty = TY_IDX_ZERO;
            *ty = TY_pointed(WN_ty(parent));
        } else {
            *ty = MTYPE_To_TY(desc);
        }
        return TRUE;
    } else if (WN_operator(parent) == OPR_MSTORE) {
        //*ty = TY_IDX_ZERO;
        *ty = TY_pointed(WN_ty(parent));
        return TRUE;
    } else {
        WN *node = parent;
        while (WN_operator(node) == OPR_ARRAYEXP ||
               WN_operator(node) == OPR_ADD) {
            node = Get_Parent(parent);
        }
        if (WN_operator(node) == OPR_ISTORE) {
            TYPE_ID desc = WN_desc(node);
            if (desc == MTYPE_M) {
                //*ty = TY_IDX_ZERO;
                //*ty = TY_pointed(WN_ty(parent));
                *ty = TY_pointed(WN_ty(node));
            } else {
                *ty = MTYPE_To_TY(desc);
            }
            return TRUE;

        } else if (WN_operator(node) == OPR_MSTORE) {
            //*ty = TY_IDX_ZERO;
            //*ty = TY_pointed(WN_ty(parent));
            *ty = TY_pointed(WN_ty(node));
            return TRUE;

        }
        /* in this case, we fall through and return FALSE */
    }

    return FALSE;
}

/*
 * array_ref_on_RHS
 *
 * Input: wn, an ARRAY or ARRSECTION node.
 * Output: return value says if the input wn is on RHS of its parent
 * statement.
 */
static BOOL array_ref_on_RHS(WN *wn, TY_IDX *ty)
{
    WN *parent;
    Is_True(WN_operator(wn) != OPR_ARRAY ||
            WN_operator(wn) != OPR_ARRSECTION,
            ("Unexpected operator for array_ref_on_RHS arg"));

    parent = Get_Parent(wn);

    if (WN_operator(parent) == OPR_ILOAD) {
        TYPE_ID desc = WN_desc(parent);
        if (desc == MTYPE_M) {
            //*ty = TY_IDX_ZERO;
            *ty = TY_pointed(WN_ty(parent));
        } else {
            *ty = MTYPE_To_TY(desc);
        }
        return TRUE;
    } else if (WN_operator(parent) == OPR_MLOAD) {
        //*ty = TY_IDX_ZERO;
        *ty = TY_pointed(WN_ty(parent));
        return TRUE;
    }

    return FALSE;
}

/*
 * array_ref_in_parm
 *
 * Input: wn, an ARRAY or ARRSECTION node.
 * Output: return value says if the input wn is in a PARM node of its parent
 * statement.
 */
static BOOL array_ref_in_parm(WN *wn, TY_IDX *ty)
{
    WN *parent;
    Is_True(WN_operator(wn) != OPR_ARRAY ||
            WN_operator(wn) != OPR_ARRSECTION,
            ("Unexpected operator for array_ref_in_parm arg"));

    parent = Get_Parent(wn);

    if (WN_operator(parent) == OPR_PARM) {
        TY_IDX tid = WN_ty(parent);
        if (TY_kind(tid) == KIND_POINTER)
            *ty = TY_pointed(tid);
    }

    return WN_operator(parent) == OPR_PARM;
}

/*
 * is_lvalue
 *
 * Input: wn, a whirl expression node
 * Output: return value says if the input wn is an lvalue (i.e. has an
 * address)
 */
static BOOL is_lvalue(WN *expr)
{
    BOOL ret;

    if (WN_operator(expr) == OPR_CVT || WN_operator(expr) == OPR_CVTL) {
        ret = is_lvalue(WN_kid0(expr));
    } else {
        ret = (WN_operator(expr) == OPR_ARRAYEXP &&
                (WN_operator(WN_kid0(expr)) == OPR_MLOAD ||
                 WN_operator(WN_kid0(expr)) == OPR_ILOAD)) ||
            WN_operator(expr) == OPR_LDID  ||
            WN_operator(expr) == OPR_ILOAD ||
            WN_operator(expr) == OPR_MLOAD;
    }

    return ret;
}

/*
 * get_transfer_size
 *
 * Input: arr_ref, WN for an array reference
 *        elem_type, type for base element
 * Output: returns the size in bytes for the array reference arr_ref
 */
static WN* get_transfer_size(WN *arr_ref, TY_IDX elem_type)
{
    WN *size_wn = WN_Intconst(MTYPE_U8,
                              TY_size( elem_type) );

    if (WN_operator(arr_ref) == OPR_ARRSECTION) {
        WN *arrayexp = get_containing_arrayexp(arr_ref);

        if (arrayexp != NULL) {
            /* case 1 */
            for (INT8 i = 1; i < WN_kid_count(arrayexp); i++) {
                size_wn = WN_Mpy(MTYPE_U8, WN_COPY_Tree(WN_kid(arrayexp,i)),
                                 size_wn);
            }
        } else {
            /* case 2 */

            /* no ARRAYEXP found. So determine full size of its kids */
            INT8 rank = (WN_kid_count(arr_ref) - 1) / 2;
            for (INT8 i = 0; i < rank; i++) {
                WN* sub = WN_array_index(arr_ref, i);

                if (WN_operator(sub) == OPR_TRIPLET) {
                    /* multiply by number of values in progression */
                    size_wn = WN_Mpy(MTYPE_U8, WN_COPY_Tree(WN_kid(sub,2)),
                            size_wn);
                } else if (WN_operator(sub) == OPR_ARRAYEXP) {
                    /* multiply by size of first (and only) dimension */
                    size_wn = WN_Mpy(MTYPE_U8, WN_COPY_Tree(WN_kid(sub,1)),
                            size_wn);
                }
            }
        }
    } else if (WN_operator(arr_ref) == OPR_ARRAYEXP) {
        /* case 3 */
        for (INT8 i = 1; i < WN_kid_count(arr_ref); i++) {
            size_wn = WN_Mpy(MTYPE_U8, WN_COPY_Tree(WN_kid(arr_ref,i)),
                             size_wn);
        }
    } else if (WN_operator(arr_ref) == OPR_ARRAY) {
        WN *arrayexp = get_containing_arrayexp(arr_ref);

        if (arrayexp != NULL) {
            /* case 4 */
            for (INT8 i = 1; i < WN_kid_count(arrayexp); i++) {
                size_wn = WN_Mpy(MTYPE_U8, WN_COPY_Tree(WN_kid(arrayexp,i)),
                                 size_wn);
            }
        }
    }

    return size_wn;
}

/*
 * replace_RHS_with_LCB
 *
 * Input: stmt_node, an assignment statement of the form LHS = RHS
 *        LCB_st, a symbol for a generated LCB (local communication buffer)
 *
 * Action:
 *    This routine will substitute the LCB into RHS of assignment statement,
 *    so that it becomes:
 *          LHS = LCB(...)
 */
static void replace_RHS_with_LCB( WN *stmt_node, ST *LCB_st)
{
    WN *old_RHS;
    WN *new_RHS;
    WN *new_RHS_addr;
    TY_IDX elem_type;
    INT elem_size;
    TYPE_ID desc, rtype;

    Is_True(WN_operator(stmt_node) == OPR_ISTORE ||
            WN_operator(stmt_node) == OPR_MSTORE,
           ("Unexpected operator for replace_RHS_with_LCB"));

    elem_type = TY_pointed(ST_type(LCB_st));
    elem_size = TY_size(elem_type);

    /* resolve to a base element type */
    while ( TY_kind(elem_type) == KIND_ARRAY ) {
        elem_type = TY_etype(elem_type);
        elem_size = TY_size(elem_type);
        if (TY_kind(elem_type) == KIND_STRUCT) {
            elem_type = MTYPE_To_TY(MTYPE_U1);
            break;
        }
    }

    rtype = TY_mtype(elem_type);
    desc = TY_mtype(elem_type);
    if (desc == MTYPE_I1 || desc == MTYPE_I2)
        rtype = MTYPE_I4;
    else if (desc == MTYPE_U1 || desc == MTYPE_U2)
        rtype = MTYPE_U4;

    old_RHS = WN_kid0(stmt_node);

    /* check if RHS or LHS is an array expression, otherwise its just a simple
     * indirect load of the LCB pointer */
    if (WN_operator(old_RHS) == OPR_ARRAYEXP) {
        WN *LHS_arrayexp = WN_kid1(stmt_node);
        WN *LHS_arrsection = get_inner_arrsection(LHS_arrayexp);
        INT LHS_section_ndim, LHS_corank;

        new_RHS_addr = WN_Create( OPCODE_make_op(OPR_ARRSECTION,
                    WN_rtype(LHS_arrsection),
                    WN_desc(LHS_arrsection)),
                    1+2*(WN_kid_count(old_RHS)-1));

        /* use elem_size since WN_element_size may not be true element size
         * when original array ref was non-contiguous (e.g. for assume shape
         * array of 8-byte integers, WN_element_size will be -4)
         */
        WN_element_size(new_RHS_addr) = elem_size;
            /* abs(WN_element_size(LHS_arrsection)); */

        WN_array_base(new_RHS_addr) =
            WN_CreateLdid(OPR_LDID, TY_mtype(ST_type(LCB_st)),
                        TY_mtype(ST_type(LCB_st)), 0, LCB_st,
                        ST_type(LCB_st));

        INT ndim = WN_kid_count(old_RHS) - 1;
        for (INT8 i = 1; i <= ndim; i++) {
            WN_array_dim(new_RHS_addr, i-1) =
                WN_COPY_Tree(WN_kid(old_RHS,i));

            OPCODE opc_triplet =
                Pointer_Size == 4 ? OPC_I4TRIPLET : OPC_I8TRIPLET;
            WN_array_index(new_RHS_addr, i-1) =
                WN_Create(opc_triplet, 3);
            /* lb */
            WN_kid0(WN_array_index(new_RHS_addr,i-1)) =
                WN_Intconst(Integer_type, 0);
            /* extent */
            WN_kid2(WN_array_index(new_RHS_addr,i-1)) =
                WN_COPY_Tree(WN_kid(old_RHS,i));
            /* stride */
            WN_kid1(WN_array_index(new_RHS_addr,i-1)) =
                WN_Intconst(Integer_type, 1);
        }
    } else if (WN_operator(stmt_node) == OPR_ISTORE &&
           WN_operator(WN_kid1(stmt_node)) == OPR_ARRAYEXP) {
        /* LHS is an array expression, so RHS should be one also */
        WN *LHS_arrayexp = WN_kid1(stmt_node);
        WN *LHS_arrsection = get_inner_arrsection(LHS_arrayexp);
        INT LHS_section_ndim, LHS_corank;

        new_RHS_addr = WN_Create( OPCODE_make_op(OPR_ARRSECTION,
                    WN_rtype(LHS_arrsection),
                    WN_desc(LHS_arrsection)),
                    1+2*(WN_kid_count(LHS_arrayexp)-1));

        /* use elem_size since WN_element_size may not be true element size
         * when original array ref was non-contiguous (e.g. for assume shape
         * array of 8-byte integers, WN_element_size will be -4)
         */
        WN_element_size(new_RHS_addr) = elem_size;
           /* abs(WN_element_size(LHS_arrsection)); */

        WN_array_base(new_RHS_addr) =
            WN_CreateLdid(OPR_LDID, TY_mtype(ST_type(LCB_st)),
                        TY_mtype(ST_type(LCB_st)), 0, LCB_st,
                        ST_type(LCB_st));

        INT ndim = WN_kid_count(LHS_arrayexp)-1;
        for (INT8 i = 1; i <= ndim; i++) {
            WN_array_dim(new_RHS_addr, i-1) =
                WN_COPY_Tree(WN_kid(LHS_arrayexp,i));

            OPCODE opc_triplet =
                Pointer_Size == 4 ? OPC_I4TRIPLET : OPC_I8TRIPLET;
            WN_array_index(new_RHS_addr, i-1) =
                WN_Create(opc_triplet, 3);
            /* lb */
            WN_kid0(WN_array_index(new_RHS_addr,i-1)) =
                WN_Intconst(Integer_type, 0);
            /* extent */
            WN_kid2(WN_array_index(new_RHS_addr,i-1)) =
                WN_COPY_Tree(WN_kid(LHS_arrayexp,i));
            /* stride */
            WN_kid1(WN_array_index(new_RHS_addr,i-1)) =
                WN_Intconst(Integer_type, 1);
        }
    } else {
        new_RHS_addr =
            WN_CreateLdid(OPR_LDID, TY_mtype(ST_type(LCB_st)),
                        TY_mtype(ST_type(LCB_st)), 0, LCB_st,
                        ST_type(LCB_st));
    }

    switch (desc) {
        case MTYPE_I1:
            new_RHS_addr = WN_CreateCvtl(OPC_I4CVTL, 8, new_RHS_addr);
            break;
        case MTYPE_I2:
            new_RHS_addr = WN_CreateCvtl(OPC_I4CVTL, 16, new_RHS_addr);
            break;
        case MTYPE_U1:
            new_RHS_addr = WN_CreateCvtl(OPC_U4CVTL, 8, new_RHS_addr);
            break;
        case MTYPE_U2:
            new_RHS_addr = WN_CreateCvtl(OPC_U4CVTL, 16, new_RHS_addr);
            break;
    }

    new_RHS = WN_CreateIload(OPR_ILOAD, rtype, desc, 0,
            elem_type, ST_type(LCB_st), new_RHS_addr);

    WN_Delete(old_RHS);

    WN_kid0(stmt_node) = F90_Wrap_ARREXP(new_RHS);
}

/*
 * get_lcb_assignment
 *
 * Input: coarray_ref, a remote coarray reference
 *
 * Action:
 *    This routine will generate an assignment statement of the form:
 *         LCB(...) = coarray_ref(...)
 */
static WN *get_lcb_assignment(WN *coarray_deref, INT num_codim,
                              ST *LCB_st)
{
    WN *new_stmt;
    WN *addr = NULL;
    TY_IDX elem_type;
    INT elem_size;
    TYPE_ID desc, rtype;
    WN *coarray_ref = WN_kid0(coarray_deref);

    Is_True(WN_operator(coarray_ref) == OPR_ARRSECTION ||
            WN_operator(coarray_ref) == OPR_ARRAY,
           ("Unexpected operator for get_lcb_assignment"));

    elem_type = TY_pointed(ST_type(LCB_st));
    elem_size = TY_size(elem_type);

    /* resolve to a base element type */
    while ( TY_kind(elem_type) == KIND_ARRAY ) {
        elem_type = TY_etype(elem_type);
        elem_size = TY_size(elem_type);
        if (TY_kind(elem_type) == KIND_STRUCT) {
            elem_type = MTYPE_To_TY(MTYPE_U1);
            break;
        }
    }

    rtype = TY_mtype(elem_type);
    desc = rtype;

    if (desc == MTYPE_I1 || desc == MTYPE_I2)
        desc = MTYPE_I4;
    else if (desc == MTYPE_U1 || desc == MTYPE_U2)
        desc = MTYPE_U4;

    if (WN_operator(coarray_ref) == OPR_ARRSECTION) {
        WN *new_arrsection;
        INT ndim, section_ndim;
        WN *new_arrexp = F90_Wrap_ARREXP(WN_COPY_Tree(coarray_ref));

        section_ndim = (WN_kid_count(coarray_ref) - 1) / 2 - num_codim;
        ndim = WN_kid_count(new_arrexp) - 1;

        new_arrsection = WN_Create( OPCODE_make_op(OPR_ARRSECTION,
                             WN_rtype(coarray_ref),
                             WN_desc(coarray_ref)), 1+2*ndim);

        /* use elem_size since WN_element_size may not be true element size
         * when original array ref was non-contiguous (e.g. for assume shape
         * array of 8-byte integers, WN_element_size will be -4)
         */
        WN_element_size(new_arrsection) = elem_size;
          /*  abs(WN_element_size(coarray_ref)); */

        WN_array_base(new_arrsection) =
            WN_CreateLdid(OPR_LDID, TY_mtype(ST_type(LCB_st)),
                        TY_mtype(ST_type(LCB_st)), 0, LCB_st,
                        ST_type(LCB_st));

        INT8 j = 1;
        for (INT8 i = 1; i <= section_ndim; i++) {
            WN *orig_index = WN_array_index(coarray_ref, num_codim+i-1);
            if ( (WN_operator(orig_index) == OPR_TRIPLET) ||
                 (WN_operator(orig_index) == OPR_ARRAYEXP) )  {
              WN_array_dim(new_arrsection, j-1) =
                  WN_COPY_Tree(WN_kid(new_arrexp, j));

              WN_array_index(new_arrsection, j-1) =
                  WN_Create( OPCODE_make_op(OPR_TRIPLET,
                    WN_rtype(orig_index), WN_desc(orig_index)), 3);
              /* lb */
              WN_kid0(WN_array_index(new_arrsection,j-1)) =
                  WN_Intconst(Integer_type, 0);
              /* extent */
              WN_kid2(WN_array_index(new_arrsection,j-1)) =
                  WN_COPY_Tree(WN_kid(new_arrexp, j));
              /* stride */
              WN_kid1(WN_array_index(new_arrsection,j-1)) =
                  WN_Intconst(Integer_type, 1);
              j++;
            }
        }

        /* replace old ARRSECTION node with new one  */
        WN_Delete(WN_kid0(new_arrexp));
        WN_kid0(new_arrexp) = new_arrsection;
        addr = new_arrexp;
    } else {
        addr =
            WN_CreateLdid(OPR_LDID, TY_mtype(ST_type(LCB_st)),
                        TY_mtype(ST_type(LCB_st)), 0, LCB_st,
                        ST_type(LCB_st));
    }

    WN *val = F90_Wrap_ARREXP(coarray_deref);
    switch (rtype) {
        case MTYPE_I1:
            val = WN_CreateCvtl(OPC_I4CVTL, 8, val);
            break;
        case MTYPE_I2:
            val = WN_CreateCvtl(OPC_I4CVTL, 16, val);
            break;
        case MTYPE_U1:
            val = WN_CreateCvtl(OPC_U4CVTL, 8, val);
            break;
        case MTYPE_U2:
            val = WN_CreateCvtl(OPC_U4CVTL, 16, val);
            break;
    }

    TYPE_ID val_rtype = WN_rtype(val);

    if (val_rtype != desc) {
        val = WN_Cvt(val_rtype, desc, val);
    }

    new_stmt = WN_Istore(rtype, 0, ST_type(LCB_st),
                       addr, val);

    Set_LCB_Stmt(new_stmt);

    return new_stmt;
}

/*
 * get_lcb_sym
 *
 * input: access, WHIRL node for a memory access. It is assumed that the base
 * address for the access is given by an LCB pointer
 *
 * output: return the LCB sym
 */
static ST *get_lcb_sym(WN *access)
{
    ST *LCB_st;
    WN *base;
    Is_True( access, ("get_lcb_sym called for NULL access"));
    if (access == NULL) return NULL;

    base = access;
    while (WN_kid0(base) != NULL)
        base = WN_kid0(base);

    LCB_st = WN_st(base);

    if (! ST_is_lcb_ptr(LCB_st) )
        return NULL;
    else
        return LCB_st;
}

/*
 * get_containing_arrayexp
 *
 * input: wn, a WHIRL node
 * output: containing array expression node
 *
 * action: search parent until we find a node with operator OPR_ARRAYEXP
 */
static WN *get_containing_arrayexp(WN *wn)
{
    WN *parent;

    parent = Get_Parent(wn);
    while (parent && WN_operator(parent) != OPR_ARRAYEXP)
        parent = Get_Parent(parent);

    if (parent == NULL) {
        WN *arrayexp = F90_Wrap_ARREXP( WN_COPY_Tree(wn) );
        if (WN_operator(arrayexp) != OPR_ARRAYEXP) {
            return NULL;
        } else {
            return arrayexp;
        }
    }

    return parent;
}

/*
 * get_assign_stmt_datatype
 *
 * input: a stmt WHIRL node
 * output: the type of the object being assigned
 *
 */
static TY_IDX get_assign_stmt_datatype(WN *stmt)
{
    if (WN_desc(stmt) != MTYPE_M &&
        (WN_operator(stmt) == OPR_STID ||
        WN_operator(stmt) == OPR_ISTORE)) {
        return MTYPE_To_TY(WN_desc(stmt));
    } else if (WN_operator(stmt) == OPR_ISTORE) {
        return TY_pointed(WN_ty(stmt));
    } else if ( WN_operator(stmt) == OPR_STID ||
                WN_operator(stmt) == OPR_MSTORE) {
        return WN_ty(stmt);
    }
    return TY_IDX_ZERO;
}

static WN* get_load_parent(WN *start)
{
    WN *load = start;
    while (load &&
            WN_operator(load) != OPR_LDID  &&
            WN_operator(load) != OPR_ILOAD &&
            WN_operator(load) != OPR_MLOAD) {
        load = Get_Parent(load);
    }

    return load;
}

static WN* get_store_parent(WN *start)
{
    WN *store = start;
    while (store &&
            WN_operator(store) != OPR_STID  &&
            WN_operator(store) != OPR_MSTORE  &&
            WN_operator(store) != OPR_ISTORE) {
        store = Get_Parent(store);
    }

    return store;
}

/*
 * get_enclosing_direct_arr_ref
 *
 * input: an array reference node
 * output: enclosing array node that does not contain any indirection (i.e. no
 * ILOAD or MLOAD.
 */
static WN* get_enclosing_direct_arr_ref(WN *arr)
{
    WN *enclosing_arr;
    Is_True(WN_operator(arr) == OPR_ARRAY ||
            WN_operator(arr) == OPR_ARRSECTION,
          ("expected an array reference whirl node"));

    enclosing_arr = arr;

    WN *p = Get_Parent(enclosing_arr);
    while (p && !OPCODE_is_stmt(WN_opcode(p)) && WN_operator(p) != OPR_ILOAD &&
          WN_operator(p) != OPR_MLOAD) {
        if (WN_operator(p) == OPR_ARRAY || WN_operator(p) == OPR_ARRSECTION)
            enclosing_arr = p;
        p = Get_Parent(p);
    }

    return enclosing_arr;
} /* gen_enclosing_direct_arr_ref */

/*
 * get_enclosing_direct_arr_ref
 *
 * input: arr, a WHIRL node enclosing an arrsection node
 * output: an ARRSECTION node found within arr
 *
 * Assumes that the ARRSECTION node will found in the "base" address kid (kid
 * 0)
 */
static WN* get_inner_arrsection(WN *arr)
{
    WN *wn = arr;

    while (wn && WN_operator(wn) != OPR_ARRSECTION) {
        wn = WN_kid0(wn);
    }

    return wn;
} /* get_inner_arrsection */

/*
 * get_array_type_from_tree
 *
 * input: an WHIRL tree that is OPR_ARRAY or OPR_ARRSECTION
 * output: the TY_IDX for the array type accessed
 *
 * Action: start at left-most leaf
 */
static TY_IDX get_array_type_from_tree(WN *tree, TY_IDX *arrsection_type)
{
    WN_TREE_CONTAINER<POST_ORDER> wcpost(tree);
    WN *wn;
    TY_IDX type;
    WN_OFFSET ofst = 0;
    WN *img = NULL;

    /* start from left most leaf and search up the tree for an array access
     * */
    wn = wcpost.begin().Wn();
    type = TY_IDX_ZERO;
    while (wn) {
        WN *parent = Get_Parent(wn);

        switch (WN_operator(wn)) {
            case OPR_ARRAY:
            case OPR_ARRSECTION:
                if (type == TY_IDX_ZERO)
                    break;

                type = get_type_at_offset(type, ofst, TRUE, TRUE);

                while (TY_kind(type) == KIND_POINTER)
                    type = TY_pointed(type);

                if (arrsection_type != NULL &&
                    WN_operator(wn) == OPR_ARRSECTION) {
                    *arrsection_type = type;
                }

                if (TY_kind(type) != KIND_ARRAY)
                    return TY_IDX_ZERO;

                if (wn == tree || TY_kind(TY_etype(type)) == KIND_ARRAY)
                    return type;

                type = TY_etype(type);

                break;

            case OPR_LDA:
            case OPR_LDID:
                type = WN_ty(wn);

                ofst = 0;
                break;

            case OPR_ILOAD:
                //type = get_type_at_offset(type, ofst, FALSE, FALSE);
                type = WN_ty(wn);
                while (TY_kind(type) == KIND_POINTER)
                    type = TY_pointed(type);

                ofst = 0;

                break;

            case OPR_ADD:
                ofst += WN_const_val(WN_kid1(wn));
                break;
        }

        wn = parent;
    }

    /* should not reach */

    return TY_IDX_ZERO;
}


/* Fortran is column-major, meaning array subscripts in Fortran should be
 * ordered from the opposite end.
 */
static inline WN* WN_array_subscript(WN *array, INT8 i)
{
    return WN_array_index( array, WN_num_dim(array)-i-1 );
}

static int subscript_is_scalar(WN *array, INT8 i)
{
    WN *subscript = WN_array_subscript(array, i);

    return ( WN_operator(subscript) != OPR_TRIPLET &&
             WN_operator(subscript) != OPR_ARRAYEXP );
}

static int subscript_is_strided(WN *array, INT8 i)
{
    int is_strided = 0;
    WN *subscript = WN_array_subscript(array, i);

    if (WN_operator(subscript) == OPR_TRIPLET) {
        if (WN_operator(WN_kid1(subscript)) != OPR_INTCONST ||
            WN_const_val(WN_kid1(subscript)) != 1) {
            is_strided = 1;
        }
    } else if (WN_operator(subscript) == OPR_ARRAYEXP) {
        is_strided = 1;
    }

    return is_strided;
}

static WN* subscript_extent(WN *array, INT8 i)
{
    WN *extent;
    WN* subscript = WN_array_subscript(array, i);

    if (WN_operator(subscript) == OPR_TRIPLET) {
        extent = WN_COPY_Tree(WN_kid2(subscript));
    } else if (WN_operator(subscript) == OPR_ARRAYEXP) {
        /* TODO: not handled currently */
        extent = NULL;
    } else {
        Is_True(0, ("subscript extent called on scalar subscript"));
        extent = WN_Intconst(Integer_type, 1);
    }

    return extent;
}

static WN* subscript_stride(WN *array, INT8 i)
{
    WN *stride;
    WN* subscript = WN_array_subscript(array, i);

    if (WN_operator(subscript) == OPR_TRIPLET) {
        stride = WN_COPY_Tree(WN_kid1(subscript));
    } else if (WN_operator(subscript) == OPR_ARRAYEXP) {
        /* TODO: not handled currently */
        stride = WN_Intconst(Integer_type, 1);
    } else {
        Is_True(0, ("subscript extent called on scalar subscript"));
        stride = WN_Intconst(Integer_type, 1);
    }

    return stride;
}

static WN* gen_array1_ref( OPCODE op_array, TY_IDX array_type,
                               ST *array_st, INT8 index, INT8 dim)
{
    WN *array = WN_Create( op_array, 3 );
    WN_element_size(array) = TY_size(TY_AR_etype(array_type));
    WN_array_base(array) = WN_Lda(Pointer_type, 0, array_st);
    WN_array_index(array,0) = WN_Intconst(MTYPE_U8, index);
    WN_array_dim(array,0) = WN_Intconst(MTYPE_U8, dim);

    return array;
}

static void gen_auto_target_symbol(ST *sym)
{
    char *new_sym_str = (char *) alloca(strlen("__targptr_") +
                strlen(ST_name(sym)));

    sprintf( new_sym_str, "__targptr_%s", ST_name(sym));

    /* make symbol name a legal variable identifier */
    char *s = new_sym_str;
    for (int i = 0; i < strlen(new_sym_str); i++) {
        if (s[i] == '.') s[i] = '_';
    }

    ST *new_sym = New_ST( CURRENT_SYMTAB );
    ST_Init( new_sym, Save_Str(new_sym_str), CLASS_VAR, SCLASS_AUTO,
            EXPORT_LOCAL, Make_Pointer_Type(ST_type(sym)) );

    auto_target_symbol_map[sym] = new_sym;
}

static inline void gen_save_coarray_symbol(ST *sym)
{
    Is_True( ST_sclass(sym) == SCLASS_PSTATIC,
            ("sym storage class should be SCLASS_PSTATIC"));

    save_coarray_symbol_map[sym] = gen_save_symm_symbol(sym);
}

static inline void gen_save_target_symbol(ST *sym)
{
    Is_True( ST_sclass(sym) == SCLASS_PSTATIC || currentpu_ismain(),
            ("sym storage class should be SCLASS_PSTATIC"));

    save_target_symbol_map[sym] = gen_save_symm_symbol(sym);
}

static inline void gen_global_save_coarray_symbol(ST *sym)
{
    Is_True( ST_sclass(sym) == SCLASS_COMMON ||
             ST_sclass(sym) == SCLASS_DGLOBAL,
            ("sym storage class should be SCLASS_COMMON or SCLASS_DGLOBAL"));

    common_save_coarray_symbol_map[sym] = gen_save_symm_symbol(sym);
}

static inline void gen_global_save_target_symbol(ST *sym)
{
    Is_True( ST_sclass(sym) == SCLASS_COMMON ||
             ST_sclass(sym) == SCLASS_DGLOBAL,
            ("sym storage class should be SCLASS_COMMON or SCLASS_DGLOBAL"));

    common_save_target_symbol_map[sym] = gen_save_symm_symbol(sym);
}

static ST* gen_save_symm_symbol(ST *sym)
{
    char *new_sym_str;

    if (ST_is_initialized(sym)) {
        new_sym_str = (char *) alloca(strlen("__SAVE_INIT_SYMM") +
                strlen(ST_name(sym)) + strlen(ST_name(Get_Current_PU_ST()))
                + 20);

        sprintf( new_sym_str, "__SAVE_INIT_SYMM_%s_%s_%lu",
                ST_name(Get_Current_PU_ST()), ST_name(sym),
                (unsigned long) TY_size(ST_type(sym)));
    } else {
        new_sym_str = (char *) alloca(strlen("__SAVE_SYMM_") +
                strlen(ST_name(sym)) + strlen(ST_name(Get_Current_PU_ST()))
                + 20);

        sprintf( new_sym_str, "__SAVE_SYMM_%s_%s_%lu",
                ST_name(Get_Current_PU_ST()), ST_name(sym),
                (unsigned long) TY_size(ST_type(sym)));
    }

    /* make symbol name a legal variable identifier */
    char *s = new_sym_str;
    for (int i = 0; i < strlen(new_sym_str); i++) {
        if (s[i] == '.') s[i] = '_';
    }

    ST *new_sym = New_ST( GLOBAL_SYMTAB );
    ST_Init( new_sym, Save_Str(new_sym_str), CLASS_VAR, SCLASS_EXTERN,
            EXPORT_PREEMPTIBLE, Make_Pointer_Type(ST_type(sym)) );

    /* indicate this pointer type is restricted (i.e. doesn't alias) */
    TY_IDX ty = ST_type(new_sym);
    Set_TY_is_restrict(ty);
    Set_TY_is_const(ty);
    Set_ST_type(new_sym, ty);

    /* if symbol is initialized, then we also generate a global pointer symbol
     * to it */
    if (ST_is_initialized(sym)) {
        char *str = (char *) alloca(8 + strlen(ST_name(sym)) +
                     strlen(ST_name(Get_Current_PU_ST())));

        sprintf( str, "__%s_%s_ptr", ST_name(Get_Current_PU_ST()),
                 ST_name(sym));

        /* make symbol name a legal variable identifier */
        char *s = str;
        for (int i = 0; i < strlen(str); i++) {
            if (s[i] == '.') s[i] = '_';
        }

        ST *new_sym_ptr = New_ST( GLOBAL_SYMTAB );

        ST_Init( new_sym_ptr, Save_Str(str), CLASS_VAR, SCLASS_DGLOBAL,
                EXPORT_PREEMPTIBLE, MTYPE_To_TY(Pointer_type) );
        Set_ST_is_initialized(new_sym_ptr);

        Allocate_Object(new_sym_ptr);

        INITO_IDX new_inito = New_INITO (new_sym_ptr);
        INITV_IDX inv = New_INITV();
        INITV_Init_Symoff(inv, ST_base(sym), sym->offset);
        Set_ST_initv_in_other_st(sym);
        Set_INITV_next(inv, 0);
        Set_INITO_val(new_inito, inv);
    }

    return new_sym;
}

/*
 * is_load_operation
 *
 * simply return TRUE if its an ldid, iload, or mload operation
 */
static BOOL is_load_operation(WN *node)
{
    if (WN_operator(node) == OPR_CVT || WN_operator(node) == OPR_CVTL)
        return is_load_operation(WN_kid0(node));

    return WN_operator(node) == OPR_LDID ||
           WN_operator(node) == OPR_ILOAD ||
           WN_operator(node) == OPR_MLOAD;
}

/*
 * is_convert_operation
 *
 * simply return TRUE if its a type conversion operation.
 */
static BOOL is_convert_operation(WN *node)
{
    return WN_operator(node) == OPR_CVT;
}

/*
 * get_innermost_array
 *
 */
static WN* get_innermost_array(WN *ref)
{
    WN *wnx, *wi;

    wnx = NULL;
    wi = ref;
    while (wi) {
        if (WN_operator(wi) == OPR_ARRAY ||
            WN_operator(wi) == OPR_ARRSECTION) {
            wnx = wi;
        }

        wi = WN_kid0(wi);
    }

    return wnx;
} /* get_innermost_array */

/*
 * make_array_ref
 *
 * takes a symbol to be loaded, and creates an scalar ARRAY reference with
 * the address for that symbol as the base address
 */
static WN* make_array_ref(ST *base)
{
    WN *arr_ref = WN_Create( OPCODE_make_op(OPR_ARRAY,Pointer_Mtype,MTYPE_V),3);
    WN_element_size(arr_ref) = TY_size(ST_type(base));
    WN_array_base(arr_ref) = WN_Lda(Pointer_type, 0, base);
    WN_array_index(arr_ref,0) = WN_Intconst(MTYPE_U4, 0);
    WN_array_dim(arr_ref,0) = WN_Intconst(MTYPE_U4, 1);
    return arr_ref;
} /* make_array_ref */

static WN* make_array_ref(WN *base)
{
    WN *arr_ref = WN_Create( OPCODE_make_op(OPR_ARRAY,Pointer_Mtype,MTYPE_V),3);
    WN_element_size(arr_ref) = TY_size(WN_ty(base));
    WN_array_base(arr_ref) = base;
    WN_array_index(arr_ref,0) = WN_Intconst(MTYPE_U4, 0);
    WN_array_dim(arr_ref,0) = WN_Intconst(MTYPE_U4, 1);
    return arr_ref;
} /* make_array_ref */

/* generates an if statement, where an "uncoindexed" version of orig_stmt_copy
 * is inserted into the then block. */
static WN* gen_local_image_condition(WN *image, WN *orig_stmt_copy, BOOL is_write)
{
    Parentize(orig_stmt_copy);

    uncoindex_expr( WN_kid(orig_stmt_copy,is_write?1:0) );

    WN *if_local_wn, *test;
    test = WN_EQ(MTYPE_U8,
            WN_COPY_Tree(image),
            WN_Ldid(MTYPE_U8, 0, this_image_st,
                ST_type(this_image_st)));

    if_local_wn = WN_CreateIf(test,
            WN_CreateBlock(),
            WN_CreateBlock());
    WN_INSERT_BlockLast( WN_then(if_local_wn), orig_stmt_copy );

    return if_local_wn;
} /* gen_local_image_condition */

static BOOL is_assignment_stmt(WN *stmt)
{
    return stmt && OPCODE_is_store(WN_opcode(stmt));
}

static int find_kid_num(WN *parent, WN *kid)
{
    Is_True(parent == Get_Parent(kid), ("parent is not the parent"));

    int kid_count = WN_kid_count(parent);
    int i;
    for (i = 0; i < kid_count; i++) {
        if (kid == WN_kid(parent, i)) break;
    }

    Is_True(i < kid_count, ("kid not found in parent tree"));

    return i;
}

static inline void set_and_update_cor_depth(WN *w, BOOL c, INT d)
{
    COR_INFO info;
    info.is_cor = c;
    info.cor_depth = d;

    Set_COR_Info(w, &info);
    update_cor_depth(w);
}

static inline void update_cor_depth(WN *w)
{
    WN *wp;
    int curr_depth;

    COR_INFO info;
    Get_COR_Info(w, &info);

    curr_depth = info.cor_depth;

    wp = Get_Parent(w);
    while (wp) {
        Get_COR_Info(wp, &info);
        if (info.is_cor) {
            info.cor_depth += curr_depth;
            curr_depth = info.cor_depth;
        } else if (info.cor_depth < curr_depth) {
            info.cor_depth = curr_depth;
        } else {
            curr_depth = info.cor_depth;
        }
        Set_COR_Info(wp, &info);
        wp = Get_Parent(wp);
    }
}

static void unfold_nested_cors_in_block(WN *block)
{
    COR_INFO node_core_info;
    WN *s, *s_next;

    Get_COR_Info(block, &node_core_info);

    if (node_core_info.cor_depth < 2) return;

    for (s = WN_first(block); s; s = s_next) {
        s_next = WN_next(s);
        Get_COR_Info(s, &node_core_info);
        if (node_core_info.cor_depth < 2) continue;

        unfold_nested_cors_in_stmt(s, s, block);
    }
}

static void unfold_nested_cors_in_stmt(WN *node, WN *stmt, WN *block, BOOL is_nested)
{
    static WN *wn_arrayexp = NULL;

    COR_INFO node_cor_info;

    ST *lcbptr_st;
    WN *transfer_size_wn;
    ST *st1;
    TY_IDX ty1, ty2, ty3;
    TY_IDX coarray_type;
    TY_IDX elem_type;
    WN *replace_wn = NULL;
    WN *image;
    WN *coindexed_arr_ref;
    WN *direct_coarray_ref;
    WN *insert_wnx;

    if (WN_operator(node) == OPR_BLOCK) {
        unfold_nested_cors_in_block(node);
        return;
        /* does not reach */
    }

    Get_COR_Info(node, &node_cor_info);
    if (node_cor_info.cor_depth == 0) return;

    if (WN_operator(node) == OPR_ARRAYEXP) {
        wn_arrayexp = node;
    }

    if (is_nested && node_cor_info.is_cor) {
        if (WN_operator(node) == OPR_ARRAY ||
            WN_operator(node) == OPR_ARRSECTION) {

            coindexed_arr_ref = expr_is_coindexed(node, &image, &coarray_type,
                                                  &direct_coarray_ref);

            /* TODO: rename this function to something else. It doesn't have
             * to be RHS of an assignment statement */
            if (!array_ref_on_RHS(coindexed_arr_ref, &elem_type) ||
                image == NULL || is_vector_access(coindexed_arr_ref)) {
                goto recurse;
            }

            Is_True(image != NULL && !is_vector_access(coindexed_arr_ref),
                    ("cor info for this node is incorrect"));

            /* create destination LCB for co-indexed term */

            WN *new_stmt_node;
            ST *LCB_st;
            WN *xfer_sz_node;
            INT num_codim;

            /* create LCB for coindexed array ref */
            LCB_st = gen_lcbptr_symbol(
                    Make_Pointer_Type(elem_type,FALSE),
                    "LCB" );
            xfer_sz_node = get_transfer_size(coindexed_arr_ref,
                    elem_type);
            insert_wnx = Generate_Call_acquire_lcb(
                    xfer_sz_node,
                    WN_Lda(Pointer_type, 0, LCB_st));

            WN_INSERT_BlockBefore(block, stmt, insert_wnx);
            Parentize(insert_wnx);
            Set_Parent(insert_wnx, block);

            /* create "normalized" assignment from remote coarray */
            num_codim = coindexed_arr_ref == direct_coarray_ref ?
                get_coarray_corank(coarray_type) : 0;
            WN *nested_cor_parent_copy =
                WN_COPY_Tree_With_Map(Get_Parent(node));
            new_stmt_node = get_lcb_assignment(
                    nested_cor_parent_copy,
                    num_codim,
                    LCB_st);

            WN_INSERT_BlockBefore(block, stmt, new_stmt_node);
            Parentize(new_stmt_node);
            Set_Parent(new_stmt_node, block);
            update_cor_depth(nested_cor_parent_copy);

            /* recursively unfold nested CORs in new statement */
            unfold_nested_cors_in_stmt(new_stmt_node, new_stmt_node, block);

            /* replace term in RHS */
            WN *load = get_load_parent(coindexed_arr_ref);
            if (load) WN_offset(load) = 0;
            substitute_lcb(coindexed_arr_ref, LCB_st,
                    wn_arrayexp, &replace_wn);
            WN *parent = Get_Parent(coindexed_arr_ref);
            int kid_num = find_kid_num(parent, coindexed_arr_ref);
            WN_kid(parent, kid_num) = replace_wn;
            WN_DELETE_Tree(coindexed_arr_ref);
            Parentize(replace_wn);
            Set_Parent(replace_wn, parent);
            node = replace_wn;

            /* walk up tree, updating COR info of AST */
            set_and_update_cor_depth(replace_wn, FALSE, 0);

            /* call to release LCB */
            insert_wnx = Generate_Call_release_lcb(
                    WN_Lda(Pointer_type, 0, LCB_st));

            WN_INSERT_BlockAfter(block, stmt, insert_wnx);
            Parentize(insert_wnx);
            Set_Parent(insert_wnx, block);

        }
    }

recurse:
    /* not an array, so check kids */
    int kid_count = WN_kid_count(node);
    for (int i = 0; i < kid_count; i++) {
        WN *k = WN_kid(node, i);
        COR_INFO kid_cor_info;
        Get_COR_Info(k, &kid_cor_info);

        is_nested = is_nested || node_cor_info.is_cor;
        if (kid_cor_info.cor_depth > 1 ||
            (is_nested && kid_cor_info.cor_depth == 1)) {
            unfold_nested_cors_in_stmt(k, stmt, block, is_nested);
            /* get updated node COR info */
            Get_COR_Info(node, &node_cor_info);
        }

    }
}

