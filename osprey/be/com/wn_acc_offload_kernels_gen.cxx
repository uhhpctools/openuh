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
#include "dwarf_DST.h"
#include "dwarf_DST_producer.h"
#include "dwarf_DST_mem.h"
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
/*This file is used to generate CUDA/OpenCL Kernels from Parallel/Kernels Region*/

WN_MAP_TAB *acc_pmaptab;	/* Parent map table */
WN_MAP_TAB *acc_cmaptab;	/* Child map table */


PU_Info *acc_ppuinfo;	/* Parent PU info structure */
SYMTAB_IDX acc_psymtab;	/* Parent symbol table */
SYMTAB_IDX acc_csymtab;	/* Child symbol table */
INT32 acc_func_level;	/* Parallel function stab level */

UINT32 kernel_tmp_variable_count = 0; //not index, New ST, name didn't appear in host code.
char acc_tmp_name_prefix[] = "__acc_tmp_";

ACC_ARCH_TYPE acc_target_arch = ACC_ARCH_TYPE_NVIDIA;


/*********************************************************************************************************
***********************************************Directive Nodes********************************************
**********************************************************************************************************/	
WN* acc_AsyncExpr;
WN* acc_async_nodes;   
WN* acc_clause_intnum;
WN *acc_host_nodes;
WN *acc_device_nodes;
WN *acc_stmt_block; 	/* Original statement nodes */
WN *acc_cont_nodes; 	/* Statement nodes after acc code */
WN *acc_if_node;		/* Points to (optional) if node */
WN *acc_num_gangs_node;
WN *acc_num_workers_node;
WN *acc_vector_length_node;
WN *acc_collapse_node;
WN *acc_gang_node;
WN *acc_worker_node;
WN *acc_vector_node;
WN *acc_seq_node;
WN *acc_independent_node;

WN *acc_reduction_nodes;	/* Points to (optional) reduction nodes */
vector<WN*> acc_wait_list;
WN *acc_copy_nodes; 	/* Points to (optional) shared nodes */
WN *acc_copyin_nodes;	/* Points to (optional) copyin nodes */
WN *acc_copyout_nodes;	/* Points to (optional) copyout nodes */
WN *acc_wait_nodes; /* Points to (optional) acc wait pragma nodes */
WN *acc_parms_nodes;	/* Points to (optional) parmeter nodes */
WN *acc_create_nodes;
WN *acc_present_nodes;
WN *acc_present_or_copy_nodes;
WN *acc_present_or_copyin_nodes;
WN *acc_present_or_copyout_nodes;
WN *acc_present_or_create_nodes;
WN *acc_deviceptr_nodes;
WN *acc_private_nodes;
WN *acc_firstprivate_nodes;
WN *acc_lastprivate_nodes;//basically, it is used for copyout
WN *acc_inout_nodes;
WN *acc_delete_nodes;
WN *acc_use_device_nodes;


/*********************************************************************************************************
***********************************************Variables for offload region*******************************
**********************************************************************************************************/
ST *glbl_threadIdx_x;
ST *glbl_threadIdx_y;
ST *glbl_threadIdx_z;
ST *glbl_blockIdx_x;
ST *glbl_blockIdx_y;
ST *glbl_blockIdx_z;
ST *glbl_blockDim_x;
ST *glbl_blockDim_y;
ST *glbl_blockDim_z;
ST *glbl_gridDim_x;
ST *glbl_gridDim_y;
ST *glbl_gridDim_z;

WN* threadidx;
WN* threadidy;
WN* threadidz;

WN* blockidx;
WN* blockidy;
WN* blockidz;

WN* blockdimx;
WN* blockdimy;
WN* blockdimz;

WN* griddimx;
WN* griddimy;
WN* griddimz;


INT32 acc_region_num = 1;	 // MP region number within parent PU
INT32 acc_construct_num = 1; // construct number within MP region
INT64 acc_line_no; //for debug
ST *acc_parallel_proc;	/* Extracted parallel/kernels process */
vector<ST*> acc_kernel_functions_st; //ST list of kernel functions created
ST* acc_st_shared_memory;
//used to create kernel parameters when outline kernel function.
vector<KernelParameter> acc_kernelLaunchParamList;
//main for reduction
vector<KernelParameter> acc_additionalKernelLaunchParamList; 
vector<ST *>  kernel_param;
map<ST*, BOOL> acc_const_offload_ptr;

//this one is to manage the scalar variable automatically generated by compiler DFA.
map<ST*, ACC_SCALAR_VAR_INFO*> acc_offload_scalar_management_tab;

map<ST*, ACC_SCALAR_VAR_INFO*> acc_offload_params_management_tab;

vector<ACC_DREGION__ENTRY> acc_unspecified_dregion_pcopy;

vector<ACC_DATA_ST_MAP*> acc_unspecified_pcopyMap;

//This table is used to localize the ST  which is used in kernel/parallel region.
//static ACC_VAR_TABLE* acc_local_var_table;		//All the data in the kernel function
map<ST*, ACC_VAR_TABLE> acc_local_new_var_map;     //used to replace var st in kernel function
//static map<ST*, ST*> acc_local_new_var_map;     //used to replace var st in kernel function
map<ST*, ACC_VAR_TABLE> acc_local_new_var_map_host_data;     //used to replace var st in host_data region

BOOL acc_dfa_enabled = FALSE;
DST_INFO_IDX  acc_nested_dst;

ST* st_shared_array_4parallelRegion = NULL;
TY_IDX ty_shared_array_in_parallel = 0;


WN *acc_pragma_block;	/* Parallel funciton pragma block */
WN *acc_reference_block;	/* Parallel funciton reference block */
WN *acc_parallel_func;	/* General Kernels function */


static ST * acc_local_gtid;		/* Microtask local gtid */
static ST * acc_old_gtid_st = NULL;
static const mINT32 ACC_NUM_HASH_ELEMENTS = 1021;

vector<ACC_DREGION__ENTRY> acc_dregion_pcreate;
vector<ACC_DREGION__ENTRY> acc_dregion_pcopy;
vector<ACC_DREGION__ENTRY> acc_dregion_pcopyin;
vector<ACC_DREGION__ENTRY> acc_dregion_pcopyout;
vector<ACC_DREGION__ENTRY> acc_dregion_present;
vector<ACC_DREGION__ENTRY> acc_dregion_host;
vector<ACC_DREGION__ENTRY> acc_dregion_device;
vector<ACC_DREGION__ENTRY> acc_dregion_private;
vector<ACC_DREGION__ENTRY> acc_dregion_fprivate;//first private
vector<ACC_DREGION__ENTRY> acc_dregion_lprivate;//last private
vector<ACC_DREGION__ENTRY> acc_dregion_inout_scalar;
vector<ACC_DREGION__ENTRY> acc_dregion_delete;
vector<ACC_DREGION__ENTRY> acc_host_data_use_device;	//for host_data region

DataRegion acc_nested_dregion_info;

static void ACC_Create_Private_Variables();
static void ACC_Insert_Initial_Stmts(WN* replacement_block);
static void ACC_Gen_Scalar_Variables_CopyOut_InOffload(WN* wn_replace_block);

#define SET_P_MAP(x,t) WN_MAP_Set(f90_parent_map,(x),(void *) (t))
#define GET_P_MAP(x) ((WN *) WN_MAP_Get(f90_parent_map,(x)))
static MEM_POOL acc_f90_lower_pool;
static BOOL isFirstCall = TRUE;

static void acc_check_for_duplicates(WN *pu, const char *str)
{
   /* Set up a parent map */
   static WN_MAP f90_parent_map;
   WN_ITER *ti;
   WN *w, *k, *p;
   INT i;
   BOOL found_dup = FALSE;

   if(isFirstCall)
   {
		MEM_POOL_Initialize(&acc_f90_lower_pool,"ACC Offload Region Transformation", TRUE);
		isFirstCall = FALSE;
   }

   f90_parent_map = WN_MAP_Create(&acc_f90_lower_pool);
   
   /* Walk everything */
   ti = WN_WALK_TreeIter(pu);
   while (ti) {
      w = WN_ITER_wn(ti);
      /* look at all the kids */
      for (i=0; i < WN_kid_count(w) ; i++) {
	 k = WN_kid(w,i);
	 p = GET_P_MAP(k);
	 if ((p != NULL) && (p != w)) {
	    fprintf(TFile,"\n%s: Multiparented node p=%8p, w=%8p, k=%d\n",str,p,w,i);
	    fprintf(TFile,"parent:\n"); fdump_tree(TFile,p);
	    fprintf(TFile,"current:\n"); fdump_tree(TFile,w);
	    
	    // csc. 2002/11/14
	    WN * temp_csc = GET_P_MAP(w);
	    if( temp_csc != NULL ){
		    fprintf(TFile, "current's parent:\n");
		    fdump_tree(TFile, temp_csc );
	    }
	    
	    fprintf(TFile,"multichild:\n"); fdump_tree(TFile,k);
	    found_dup = TRUE;
	 } else {
	    SET_P_MAP(k,w);
	 }
      }
      ti = WN_WALK_TreeNext(ti);
   }
   
   WN_MAP_Delete(f90_parent_map);
   if (found_dup) {
      DevWarn(("Duplicate WHIRL nodes found %s\n"),str);
   }
}

/*
* When switching scope, use this call to save some global vars.
* When the scope is switched back, use Pop_Some_Globals to restore them.
* csc.
*/
void ACC_Push_Some_Globals( )
{

  acc_old_gtid_st = acc_local_gtid;

}

/*
* Restore globals.
* csc.
*/
void ACC_Pop_Some_Globals( )
{

	// TODO: when enable true nested-parallelism, 
	// a stack style pop/push should be implemented.
  acc_local_gtid = acc_old_gtid_st;
  acc_old_gtid_st = NULL;
}

/*******************************************************************/

/* I move the content of wn_mp_dg.cxx into here.
*  For Copy_Non_MP_Tree need some stuff of this file.
*  csc.
*/

typedef  HASH_TABLE<VINDEX16,VINDEX16> VV_HASH_TABLE;
typedef STACK<VINDEX16> V_STACK;
static MEM_POOL ACC_Dep_Pool;
//static data type is false in default, here, I just want to make it readable
static BOOL acc_dep_pool_initialized = false; 
static void ACC_Create_Vertices(WN *wn, VV_HASH_TABLE *parent_to_child,
                V_STACK *parent_vertices,
                ARRAY_DIRECTED_GRAPH16 *parent_graph,
                ARRAY_DIRECTED_GRAPH16 *child_graph);

// Fix up the array dependence graph during MP lowering
void ACC_Fix_Dependence_Graph(PU_Info *parent_pu_info,
                                PU_Info *child_pu_info, WN *child_wn)
{
  ARRAY_DIRECTED_GRAPH16 *parent_graph =
     (ARRAY_DIRECTED_GRAPH16 *) PU_Info_depgraph_ptr(parent_pu_info);
  if (!parent_graph) { // no parent, no child
    Set_PU_Info_depgraph_ptr(child_pu_info,NULL);
    return;
  }
  if (!acc_dep_pool_initialized) {
    MEM_POOL_Initialize(&ACC_Dep_Pool,"MP_Dep_Pool",FALSE);
    acc_dep_pool_initialized = TRUE;
  }
  MEM_POOL_Push(&ACC_Dep_Pool);

  // Create a new dependence graph for the child region
  ARRAY_DIRECTED_GRAPH16 *child_graph  =
            CXX_NEW(ARRAY_DIRECTED_GRAPH16(100, 500,
                WN_MAP_DEPGRAPH, DEP_ARRAY_GRAPH), Malloc_Mem_Pool);
  Set_PU_Info_depgraph_ptr(child_pu_info,child_graph);
  Set_PU_Info_state(child_pu_info,WT_DEPGRAPH,Subsect_InMem);

  // mapping from the vertices in the parent graph to the corresponding
  // ones in the child
  VV_HASH_TABLE *parent_to_child =
    CXX_NEW(VV_HASH_TABLE(200,&ACC_Dep_Pool),&ACC_Dep_Pool);
  // a list of the parent vertices in the region
  V_STACK *parent_vertices = CXX_NEW(V_STACK(&ACC_Dep_Pool),&ACC_Dep_Pool);
  ACC_Create_Vertices(child_wn,parent_to_child,parent_vertices,parent_graph,
                                                        child_graph);

  // copy the edges, erase them from the parent graph
  INT i;
  for (i=0; i<parent_vertices->Elements(); i++) {
    VINDEX16 parent_v = parent_vertices->Bottom_nth(i);
    VINDEX16 child_v = parent_to_child->Find(parent_v);
    Is_True(child_v,("child_v missing "));
    EINDEX16 e;
    while (e = parent_graph->Get_Out_Edge(parent_v)) {
      VINDEX16 parent_sink = parent_graph->Get_Sink(e);
      VINDEX16 child_sink = parent_to_child->Find(parent_sink);
      Is_True(child_sink,("child_sink missing "));
      child_graph->Add_Edge(child_v,child_sink,
                parent_graph->Dep(e),parent_graph->Is_Must(e));

      parent_graph->Remove_Edge(e);
    }
  }
  for (i=0; i<parent_vertices->Elements(); i++) {
    // remove the vertex from the parent graph
    // since removing the vertex cleans the wn map, reset it
    VINDEX16 parent_v = parent_vertices->Bottom_nth(i);
    VINDEX16 child_v = parent_to_child->Find(parent_v);
    WN *wn = parent_graph->Get_Wn(parent_v);
    parent_graph->Delete_Vertex(parent_v);
    child_graph->Set_Wn(child_v,wn);
  }
  CXX_DELETE(parent_to_child,&ACC_Dep_Pool);
  CXX_DELETE(parent_vertices,&ACC_Dep_Pool);
  MEM_POOL_Pop(&ACC_Dep_Pool);
}

// walk the child, find all the vertices, create corresponding vertices
// in the child graph, fill up the hash table and stack
void ACC_Create_Vertices(WN *wn, VV_HASH_TABLE *parent_to_child,
                V_STACK *parent_vertices,
                ARRAY_DIRECTED_GRAPH16 *parent_graph,
                ARRAY_DIRECTED_GRAPH16 *child_graph)
{
  OPCODE opcode = WN_opcode(wn);
  if (opcode == OPC_BLOCK) 
 {
    WN *kid = WN_first (wn);
    while (kid) 
	{
      ACC_Create_Vertices(kid,parent_to_child,parent_vertices,parent_graph,
                                                        child_graph);
      kid = WN_next(kid);
    }
    return;
  }
  if (OPCODE_is_load(opcode) || OPCODE_is_store(opcode)
      || OPCODE_is_call(opcode)) 
 {
    VINDEX16 parent_v = parent_graph->Get_Vertex(wn);
    if (parent_v) 
	{
      // can't overflow since parent graph has
      // at least the same number of vertices
      VINDEX16 child_v = child_graph->Add_Vertex(wn);
      parent_to_child->Enter(parent_v,child_v);
      parent_vertices->Push(parent_v);
    }
  }
  for (INT kidno=0; kidno<WN_kid_count(wn); kidno++) {
    ACC_Create_Vertices(WN_kid(wn,kidno),parent_to_child,parent_vertices,
                                        parent_graph,child_graph);
  }
}

/*
Transfer all maps (except WN_MAP_FEEDBACK) associated with each node in the
tree from the parent mapset to the kid's.
*/

void ACC_Transfer_Maps_R ( WN_MAP_TAB * parent, WN_MAP_TAB * child, WN * tree,
                  RID * root_rid );

void ACC_Transfer_Maps ( WN_MAP_TAB * parent, WN_MAP_TAB * child, WN * tree,
                RID * root_rid )
{
    // to preserve WN_MAP_FEEDBACK in child map table, copy its contents
    // to fb_map
  HASH_TABLE<WN *, INT32> fb_map(ACC_NUM_HASH_ELEMENTS, Malloc_Mem_Pool);
  WN_ITER *wni = WN_WALK_TreeIter(tree);

  for ( ; wni; wni = WN_WALK_TreeNext(wni)) {
    WN *wn = WN_ITER_wn(wni);

    fb_map.Enter(wn, IPA_WN_MAP32_Get(child, WN_MAP_FEEDBACK, wn));
  }

  ACC_Transfer_Maps_R(parent, child, tree, root_rid); // overwrites WN_MAP_FEEDBACK

    // now restore values for WN_MAP_FEEDBACK from fb_map
  HASH_TABLE_ITER<WN *, INT32> fb_map_iter(&fb_map);
  WN *wn;
  INT32 val;

  while (fb_map_iter.Step(&wn, &val))
    IPA_WN_MAP32_Set(child, WN_MAP_FEEDBACK, wn, val);

//  parent->_is_used[WN_MAP_FEEDBACK] = is_used;  // restore the flag
} // Transfer_Maps

// this function does the real work
void ACC_Transfer_Maps_R ( WN_MAP_TAB * parent, WN_MAP_TAB * child, WN * tree,
                  RID * root_rid )
{
  WN *node;
  INT32 i;

  if (tree) {
    if (WN_opcode(tree) == OPC_BLOCK) {
      for (node = WN_first(tree); node; node = WN_next(node))
	ACC_Transfer_Maps_R ( parent, child, node, root_rid );
    } else
      for (i = 0; i < WN_kid_count(tree); i++)
	ACC_Transfer_Maps_R ( parent, child, WN_kid(tree, i), root_rid );

    if (WN_map_id(tree) != -1) {
      RID *rid = REGION_get_rid ( tree );
      IPA_WN_Move_Maps_PU ( parent, child, tree );
      if (WN_opcode(tree) == OPC_REGION) {
	Is_True(root_rid != NULL, ("Transfer_Maps_R, NULL root RID"));
	RID_unlink ( rid );
	RID_Add_kid ( rid, root_rid );
      } 
    }
  }
} // Transfer_Maps_R

// A VLA that is scoped within a parallel construct
// will have its ALLOCA generated by the front end,
// and it doesn't need a new ALLOCA when localized.

static vector<ST*> acc_inner_scope_vla;

static void 
ACC_Gather_Inner_Scope_Vlas(WN *wn)
{
  if (WN_operator(wn) == OPR_STID && WN_operator(WN_kid0(wn)) == OPR_ALLOCA) {
    acc_inner_scope_vla.push_back(WN_st(wn));    
  }
  else if (WN_operator(wn) == OPR_BLOCK) {
    for (WN *kid = WN_first(wn); kid; kid = WN_next(kid)) {
      ACC_Gather_Inner_Scope_Vlas(kid);
    }
  }
  else {
    for (INT kidno = 0; kidno < WN_kid_count(wn); kidno++) {
      ACC_Gather_Inner_Scope_Vlas(WN_kid(wn, kidno));
    }
  }
}

/********************************************************************************************************
***********************************************Function Declare Here*******************************
**********************************************************************************************************/
	
/********************************************************************
 declare some global variables for threadIdx and blockIdx 
*********************************************************************/
void ACC_Gen_Predefined_Variables()
{			
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


	threadidx = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_x)),
	                                    0, glbl_threadIdx_x, ST_type(glbl_threadIdx_x));
	threadidy = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_y)),
	                                0, glbl_threadIdx_y, ST_type(glbl_threadIdx_y));
	threadidz = WN_Ldid(TY_mtype(ST_type(glbl_threadIdx_z)),
	                                0, glbl_threadIdx_z, ST_type(glbl_threadIdx_z));

	blockidx = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_x)),
	                                0, glbl_blockIdx_x, ST_type(glbl_blockIdx_x));
	blockidy = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_y)),
	                                0, glbl_blockIdx_y, ST_type(glbl_blockIdx_y));
	blockidz = WN_Ldid(TY_mtype(ST_type(glbl_blockIdx_z)),
	                                0, glbl_blockIdx_z, ST_type(glbl_blockIdx_z));

	blockdimx = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_x)),
	                                0, glbl_blockDim_x, ST_type(glbl_blockDim_x));
	blockdimy = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_y)),
	                                0, glbl_blockDim_y, ST_type(glbl_blockDim_y));
	blockdimz = WN_Ldid(TY_mtype(ST_type(glbl_blockDim_z)),
	                                0, glbl_blockDim_z, ST_type(glbl_blockDim_z));

	griddimx = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_x)),
	                                0, glbl_gridDim_x, ST_type(glbl_gridDim_x));
	griddimy = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_y)),
	                                0, glbl_gridDim_y, ST_type(glbl_gridDim_y));
	griddimz = WN_Ldid(TY_mtype(ST_type(glbl_gridDim_z)),
	                                0, glbl_gridDim_z, ST_type(glbl_gridDim_z));
}


void acc_dump_scalar_management_tab()
{
	printf("size : %d\n", acc_offload_scalar_management_tab.size());
	map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor = acc_offload_scalar_management_tab.begin();
	for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
	{
		ACC_SCALAR_VAR_INFO* pVarInfo = itor->second;
		ST* st_host = itor->first;
		printf("ST_name:%s. \n", ST_name(pVarInfo->st_var));
	}
}	
	
/*****************************************/
//This function is only called by ACC_Create_Outline_OffloadRegion function.	
static void Create_kernel_parameters_ST()
{		
	kernel_param.clear();
	int i;
	/* Do locals */
	for(i=0; i<acc_kernelLaunchParamList.size(); i++)
	{
		ST *old_st = acc_kernelLaunchParamList[i].st_host;
		//WN_OFFSET old_offset = WN_offsetx(l); 	
		TY_IDX ty = ST_type(old_st);
		TY_KIND kind = TY_kind(ty);//ST_name(old_st)
		char* localname = (char *) alloca(strlen(ST_name(old_st))+10);
		ST *new_st;
		char* old_st_name = ST_name(old_st);
		char *new_st_name = (char *) alloca(strlen(old_st_name));
		//remove the illegal symbol in the name, like '$'  
		int name_len = 0;
		for(int i=0; i<strlen(old_st_name); i++)
		{
			  if(*(old_st_name+i) == '$')
				continue;
			  new_st_name[name_len] = old_st_name[i];
			  name_len ++;
		}
		new_st_name[name_len] = '\0';
		sprintf ( localname, "__d_%s", new_st_name );
		
		if(kind == KIND_STRUCT && F90_ST_Has_Dope_Vector(old_st))
		{
				TY_IDX ty_element = ACC_GetDopeElementType(old_st);
			TY_IDX pty = MTYPE_To_TY(TY_mtype(ty_element));
			if(!acc_const_offload_ptr.empty() &&
								acc_const_offload_ptr.find(old_st)!=acc_const_offload_ptr.end())
								Set_TY_is_const(pty);
			
			TY_IDX ty_p = Make_Pointer_Type(pty);
			if(acc_ptr_restrict_enabled)
			{
				Set_TY_is_restrict(ty_p);
			}
			else if(TY_is_const(pty))
			{
				Set_TY_is_restrict(ty_p);
			}

			new_st = New_ST( CURRENT_SYMTAB );
			ST_Init(new_st,
					Save_Str( localname ),
					CLASS_VAR,
					SCLASS_FORMAL,
					EXPORT_LOCAL,
					ty_p);
			Set_ST_is_value_parm( new_st );
			if(acc_target_arch == ACC_ARCH_TYPE_APU)
				Set_ST_ACC_global_data(new_st);
			kernel_param.push_back(new_st); 
		}	
		else if (kind == KIND_POINTER)
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
			//const memory pointer
			if(!acc_const_offload_ptr.empty() && 
				acc_const_offload_ptr.find(old_st)!=acc_const_offload_ptr.end())
				Set_TY_is_const(pty);
			
			TY_IDX ty_p = Make_Pointer_Type(pty);
			
			//set restrict pointer, if it is an array
			//no necessary to perform alias analysis
			if(acc_ptr_restrict_enabled)
			{
				Set_TY_is_restrict(ty_p);
			}
			else if(TY_is_const(pty))
			{
				Set_TY_is_restrict(ty_p);
			}
			
			//in case of 64bit machine, the alignment becomes 8bytes
			//Set_TY_align(ty_p, 4);
			new_st = New_ST( CURRENT_SYMTAB );
			ST_Init(new_st,
					Save_Str( localname ),
					CLASS_VAR,
					SCLASS_FORMAL,
					EXPORT_LOCAL,
					ty_p);
			Set_ST_is_value_parm( new_st );
			if(acc_target_arch == ACC_ARCH_TYPE_APU)
				Set_ST_ACC_global_data(new_st);
		
			kernel_param.push_back(new_st);
		}
		else if (kind == KIND_ARRAY)
		{
			TY_IDX etype = ACC_GetArrayElemType(ty);
			
			etype = MTYPE_To_TY(TY_mtype(etype));
			//const memory pointer
			if(!acc_const_offload_ptr.empty() && 
				acc_const_offload_ptr.find(old_st)!= acc_const_offload_ptr.end())
				Set_TY_is_const(etype);
			TY_IDX ty_p = Make_Pointer_Type(etype);
			//set restrict pointer
			if(acc_ptr_restrict_enabled)
			{
				Set_TY_is_restrict(ty_p);
			}
			else if(TY_is_const(etype))
			{
				Set_TY_is_restrict(ty_p);
			}
			//Set_TY_align(ty_p, 4);
			new_st = New_ST( CURRENT_SYMTAB );
			ST_Init(new_st,
					Save_Str( localname ),
					CLASS_VAR,
					SCLASS_FORMAL,
					EXPORT_LOCAL,
					ty_p);
			Set_ST_is_value_parm( new_st );
			if(acc_target_arch == ACC_ARCH_TYPE_APU)
				Set_ST_ACC_global_data(new_st);

			kernel_param.push_back(new_st);
		}
		//Scalar variables in
		else if (kind == KIND_SCALAR)// && acc_kernelLaunchParamList[i].st_device == NULL)
		{
			if(acc_offload_scalar_management_tab.find(old_st) == acc_offload_scalar_management_tab.end())
										 Fail_FmtAssertion("cannot find var in acc_offload_scalar_management_tab.");
			ACC_SCALAR_VAR_INFO* pVarInfo = acc_offload_scalar_management_tab[old_st];
			TY_IDX ty_param;
			ST* st_param;
			//new_ty, new_st are used to local variable
			//ty_param, st_param are used for parameter.
			//They may be different in ACC_SCALAR_VAR_INOUT and ACC_SCALAR_VAR_OUT cases
			
			if(!pVarInfo)
				Fail_FmtAssertion("cannot find var in acc_offload_scalar_management_tab.");
			else if(pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_PRIVATE)
				Fail_FmtAssertion("A private var should not appear in kernel parameters.");
			else if(pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_IN)
			{
				ty_param = ty;
				new_st = New_ST( CURRENT_SYMTAB );
				ST_Init(new_st,
						Save_Str2("_d_", new_st_name),
						CLASS_VAR,
						SCLASS_FORMAL,
						EXPORT_LOCAL,
						MTYPE_To_TY(TY_mtype(ty)));
				Set_ST_is_value_parm( new_st );
				kernel_param.push_back(new_st);
				//they are the same in this case
				pVarInfo->st_device_in_kparameters = new_st;
				pVarInfo->st_device_in_klocal = new_st;
			}
			else if(pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_INOUT
				|| pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_OUT
				||	pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_CREATE
				||	pVarInfo->acc_scalar_type ==ACC_SCALAR_VAR_PRESENT)
			{
				//create parameter
				ty_param = Make_Pointer_Type(MTYPE_To_TY(TY_mtype(ty))); //ST_type(pVarInfo->st_device_in_host);
				st_param = New_ST( CURRENT_SYMTAB );
				if(acc_ptr_restrict_enabled)
					Set_TY_is_restrict(ty_param);
				ST_Init(st_param,
						Save_Str2("_dp_",  new_st_name),
						CLASS_VAR,
						SCLASS_FORMAL,
						EXPORT_LOCAL,
						ty_param);
				Set_ST_is_value_parm( st_param );
				if(acc_target_arch == ACC_ARCH_TYPE_APU)
					Set_ST_ACC_global_data(st_param);

				kernel_param.push_back(st_param);
				//create local				
				new_st = New_ST( CURRENT_SYMTAB );
				ST_Init(new_st,
						Save_Str2("_private_", new_st_name),
						CLASS_VAR,
						SCLASS_AUTO,
						EXPORT_LOCAL,
						MTYPE_To_TY(TY_mtype(ty)));
				//Set_ST_is_value_parm( new_st );
				//kernel_param.push_back(new_st);
				pVarInfo->st_device_in_kparameters = st_param;
				pVarInfo->st_device_in_klocal = new_st;
			}
			else
				Fail_FmtAssertion("Unclassified variables appears in kernel parameters.");
				
			/*new_st = New_ST( CURRENT_SYMTAB );
			ST_Init(new_st,
					Save_Str( localname ),
					CLASS_VAR,
					SCLASS_FORMAL,
					EXPORT_LOCAL,
					ty);
			Set_ST_is_value_parm( new_st );
			kernel_param.push_back(new_st);*/
		}
		
		ACC_VAR_TABLE var;
		var.has_offset = FALSE;
		var.orig_st = old_st;
		var.new_st = new_st;
		acc_local_new_var_map[old_st] = var;
	}
	i=0;
	//reduction @Parallel region
	//check if there is any nested loop, then traverse one by one
	//for ST name 
#define EXTRA_TEMP_BUFFER_SIZE	(32)
	for(i=0; i<acc_top_level_loop_rdc.size(); i++)
	{
		FOR_LOOP_RDC_INFO for_loop_rdc_info = acc_top_level_loop_rdc[i];
		int j=0;
		//traverse each reduction of top loop level
		for(j=0; j<for_loop_rdc_info.reductionmap.size(); j++)
		{
			ACC_ReductionMap* preductionmap = for_loop_rdc_info.reductionmap[j];
			ST* st_reduction_private_var;
			WN_OFFSET old_offset = 0;
			ST* st_host = preductionmap->hostName;
			if(acc_offload_scalar_management_tab.find(st_host) == acc_offload_scalar_management_tab.end())
				Fail_FmtAssertion("cannot find var in acc_offload_scalar_management_tab.");
			ACC_SCALAR_VAR_INFO* pVarInfo = acc_offload_scalar_management_tab[st_host];
			//if the scalar var performs the reduction, but nobody uses ther results. 
			//There must be something wrong, double check it.
			if(acc_dfa_enabled && pVarInfo->acc_scalar_type !=ACC_SCALAR_VAR_INOUT)
			{
				Fail_FmtAssertion("unecessary reduction in the top acc loop."); 
			}

			pVarInfo->is_reduction = TRUE;
			pVarInfo->is_across_gangs = TRUE; 
			pVarInfo->opr_reduction = preductionmap->ReductionOpr;
			//st_device_in_klocal must be created earlier in this function
			st_reduction_private_var = pVarInfo->st_device_in_klocal;
			//create parameter	
			TY_IDX ty_elem = ST_type(st_host);
			char* localname = (char *) alloca(strlen(ST_name(st_host))+EXTRA_TEMP_BUFFER_SIZE); 
			//reduction used buffer
			sprintf ( localname, "__acc_rbuff_%s", ST_name(st_host) );
			TY_IDX ty_p = Make_Pointer_Type(MTYPE_To_TY(TY_mtype(ty_elem)));
			ST *karg = NULL;
			karg = New_ST( CURRENT_SYMTAB );
			//restrict memory pointer
			if(acc_ptr_restrict_enabled)
				Set_TY_is_restrict(ty_p);
			ST_Init(karg,
					Save_Str( localname ),
					CLASS_VAR,
					SCLASS_FORMAL,
					EXPORT_LOCAL,
					ty_p);
			Set_ST_is_value_parm( karg );
			if(acc_target_arch == ACC_ARCH_TYPE_APU)
				Set_ST_ACC_global_data( karg );
			
			kernel_param.push_back(karg);
			KernelParameter kernelParameter;
			kernelParameter.st_host = st_host;
			kernelParameter.st_device = preductionmap->deviceName;
			acc_additionalKernelLaunchParamList.push_back(kernelParameter);
			preductionmap->st_Inkernel = karg;
			
			preductionmap->st_private_var = st_reduction_private_var;
			preductionmap->wn_private_var = WN_Ldid(TY_mtype(ST_type(st_reduction_private_var)), 
					0, st_reduction_private_var, ST_type(st_reduction_private_var));
			preductionmap->st_inout_used = pVarInfo->st_device_in_kparameters;

			
			ACC_VAR_TABLE var;
			if(old_offset)
			{
				var.has_offset = TRUE;
				var.orig_offset = old_offset;
			}
			else
				var.has_offset = FALSE;
			var.orig_st = st_host;
			var.new_st = st_reduction_private_var;
			acc_local_new_var_map[st_host] = var;	
		}
	}	
}

void ACC_Create_Func_DST ( char * st_name )
{
  DST_INFO_IDX	dst = PU_Info_pu_dst( Current_PU_Info );
  DST_INFO	*info = DST_INFO_IDX_TO_PTR(dst);
  DST_ASSOC_INFO *assoc;
  USRCPOS		srcpos;

  USRCPOS_srcpos(srcpos) = acc_line_number;
  acc_nested_dst =	DST_mk_subprogram( srcpos,
			st_name,
			DST_INVALID_IDX,	/* return type */
			DST_INVALID_IDX,	/* for weak symbols */
			ST_st_idx(acc_parallel_proc),
			DW_INL_not_inlined,
			DW_VIRTUALITY_none,
			0,
			FALSE,			/* declaration */
			FALSE,			/* prototype */
#ifdef KEY
						FALSE,					// is_artificial
#endif
			FALSE			/* external */
			);
  (void)DST_append_child( dst, acc_nested_dst );
}

/*
Create MicroTask for Working threads.  This includes creating the following:
the corresponding nested symbol table; entries for the TY, PU, and ST
tables; debugging information; PU_Info object; and Whirl tree.
Current_PU_Info is set to point to the new nested function, and the
parallel function's symtab becomes CURRENT_SYMTAB.
*/

void ACC_Create_Outline_OffloadRegion ( )
{
  // should be merged up after done. Currently reserved for Debug
  const char *construct_type_str = "accrg";
  char temp_str[64];

  // generate new name for nested function

  // should PAR regions and PAR DO's be numbered separately? -- DRK
  const char *old_st_name = ST_name(PU_Info_proc_sym(Current_PU_Info));
  char* new_st_name = strdup(old_st_name);

  char *st_name = (char *) alloca(strlen(old_st_name) + 64);
  //remove the illegal symbol in the name, like '.'  
  int name_len = 0;
  for(int i=0; i<strlen(old_st_name); i++)
  {
  	  if(*(old_st_name+i) == '.')
	  	continue;
	  new_st_name[name_len] = old_st_name[i];
	  name_len ++;
  }
  new_st_name[name_len] = '\0';

  sprintf ( st_name, "__%s_%s_%d_%d_l%d", construct_type_str, new_st_name,
	      acc_region_num, acc_construct_num, acc_line_no);
  
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
  //Set_PU_acc_lower_generated(pu);
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

  acc_parallel_proc = New_ST(GLOBAL_SYMTAB);
  ST_Init(acc_parallel_proc,
          Save_Str (st_name),
          CLASS_FUNC,
          SCLASS_TEXT,
          EXPORT_LOCAL,
          TY_IDX(pu_idx));
  //Set_ST_addr_passed(acc_parallel_proc);
  Set_ST_ACC_kernels_func(acc_parallel_proc);
  Set_ST_sfname_idx(acc_parallel_proc, Save_Str(Src_File_Name));

  Allocate_Object ( acc_parallel_proc );
  
  acc_kernel_functions_st.push_back(acc_parallel_proc);


  // create nested symbol table for parallel function

  New_Scope(CURRENT_SYMTAB + 1,
            Malloc_Mem_Pool,  // find something more appropriate--DRK
            TRUE);
  acc_csymtab = CURRENT_SYMTAB;
  acc_func_level = CURRENT_SYMTAB;
  Scope_tab[acc_csymtab].st = acc_parallel_proc;

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
	
  //create shared meory here
  acc_st_shared_memory = New_ST( CURRENT_SYMTAB);
  TY_IDX ty_sh_ptr = Make_Pointer_Type(Be_Type_Tbl(MTYPE_F8));
  Set_TY_is_shared_mem(ty_sh_ptr);
  ST_Init(acc_st_shared_memory,
	  Save_Str( "__device_shared_memory_reserved"),
	  CLASS_VAR,
	  SCLASS_AUTO,
	  EXPORT_LOCAL,
	  ty_sh_ptr);//Put this variables in local table
  Set_ST_ACC_shared_array(acc_st_shared_memory);  
  if(acc_target_arch == ACC_ARCH_TYPE_APU)
  {
 	 Set_ST_acc_type_class(acc_st_shared_memory, ST_ACC_TYPE_SHARED_ARRAY_FIXED);
  	 Set_ST_acc_shared_array_size(acc_st_shared_memory, 512);
  }
  // create ST's for parameters

  ST *arg_gtid = NULL;
  ST *task_args = NULL;
  Create_kernel_parameters_ST();
  

  //////////////////////////////////////////////////////////////////////
  /* declare some global variables for threadIdx and blockIdx */
  ACC_Gen_Predefined_Variables();
  

    // TODO: other procedure specific arguments should
    // be handled here.

    // create WHIRL tree for nested function

  acc_parallel_func = WN_CreateBlock ( );
  acc_reference_block = WN_CreateBlock ( );
  acc_pragma_block = WN_CreateBlock ( );
#ifdef KEY
  WN *current_pu_tree = PU_Info_tree_ptr(Current_PU_Info);
  WN *thread_priv_prag = WN_first(WN_func_pragmas(PU_Info_tree_ptr(Current_PU_Info)));
  
#endif
  // Currently, don't pass data via arguments.
  UINT arg_cnt = 1;
  /* turn this off if don't want to use taskargs struct */
  //if (is_task_region) arg_cnt = 2;

  //UINT slink_arg_pos = arg_cnt - 1;
  WN *func_entry = WN_CreateEntry ( kernel_param.size(), acc_parallel_proc,
                                    acc_parallel_func, acc_pragma_block,
                                    acc_reference_block );


  UINT ikid = 0;
  //vector<ST*>::iterator itor = kernel_param.begin();
  while(ikid < kernel_param.size())
  {
     WN_kid(func_entry, ikid) = WN_CreateIdname ( 0, kernel_param[ikid] );
	 //ACC_Add_DST_variable ( kernel_param[ikid], acc_nested_dst, acc_line_number, DST_INVALID_IDX );
  	 ikid ++;
  }

     // TODO: variable arguments list should be added here.

  WN_linenum(func_entry) = acc_line_number;


  // create PU_Info for nested function
  
  PU_Info *parallel_pu = TYPE_MEM_POOL_ALLOC ( PU_Info, Malloc_Mem_Pool );
  PU_Info_init ( parallel_pu );
  Set_PU_Info_tree_ptr (parallel_pu, func_entry );

  PU_Info_proc_sym(parallel_pu) = ST_st_idx(acc_parallel_proc);
  PU_Info_maptab(parallel_pu) = acc_cmaptab = WN_MAP_TAB_Create(MEM_pu_pool_ptr);
  PU_Info_pu_dst(parallel_pu) = acc_nested_dst;
  Set_PU_Info_state(parallel_pu, WT_SYMTAB, Subsect_InMem);
  Set_PU_Info_state(parallel_pu, WT_TREE, Subsect_InMem);
  Set_PU_Info_state(parallel_pu, WT_PROC_SYM, Subsect_InMem);
  Set_PU_Info_flags(parallel_pu, PU_IS_COMPILER_GENERATED);

  // don't copy nystrom points to analysis, alias_tag map
  // mp function's points to analysis will be analyzed locally.
  AliasAnalyzer *aa = AliasAnalyzer::aliasAnalyzer();
  if (aa) {
    // Current_Map_Tab is update to PU_Info_maptab(parallel_pu) in PU_Info_maptab
    Is_True(PU_Info_maptab(parallel_pu) == Current_Map_Tab,
        ("parallel_pu's PU's maptab isn't parallel_pu\n"));
    Current_Map_Tab = acc_pmaptab;
    WN_MAP_Set_dont_copy(aa->aliasTagMap(), TRUE);
    WN_MAP_Set_dont_copy(WN_MAP_ALIAS_CGNODE, TRUE);
    Current_Map_Tab = PU_Info_maptab(parallel_pu);
  }
  else {
    Current_Map_Tab = acc_pmaptab;
    WN_MAP_Set_dont_copy(WN_MAP_ALIAS_CGNODE, TRUE);
    Current_Map_Tab = PU_Info_maptab(parallel_pu);
  }

    // use hack to save csymtab using parallel_pu, so we can restore it
    // later when we lower parallel_pu; this is necessary because the
    // new symtab routines can't maintain more than one chain of symtabs
    // in memory at one time, and we lower the parent PU all the way to
    // CG before we lower any of the nested MP PUs
        // Save_Local_Symtab expects this
  Set_PU_Info_symtab_ptr(parallel_pu, NULL);
  Save_Local_Symtab(acc_csymtab, parallel_pu);

  Is_True(PU_Info_state(parallel_pu, WT_FEEDBACK) == Subsect_Missing,
          ("there should be no feedback for parallel_pu"));

  RID *root_rid = RID_Create ( 0, 0, func_entry );
  RID_type(root_rid) = RID_TYPE_func_entry;
  Set_PU_Info_regions_ptr ( parallel_pu, root_rid );
  Is_True(PU_Info_regions_ptr(parallel_pu) != NULL,
	 ("ACC_Create_Outline_OffloadRegion, NULL root RID"));

  PU_Info *tpu = PU_Info_child(Current_PU_Info);

    // add parallel_pu after last child MP PU_Info item in parent's list
  if (tpu && PU_Info_state(tpu, WT_SYMTAB) == Subsect_InMem &&
      PU_acc(PU_Info_pu(tpu)) ) {
    PU_Info *npu;

    while ((npu = PU_Info_next(tpu)) &&
	   PU_Info_state(npu, WT_SYMTAB) == Subsect_InMem &&
	   PU_acc(PU_Info_pu(npu)) )
      tpu = npu;

    PU_Info_next(tpu) = parallel_pu;
    PU_Info_next(parallel_pu) = npu;
  } else {
    PU_Info_child(Current_PU_Info) = parallel_pu;
    PU_Info_next(parallel_pu) = tpu;
  }


  // change some global state; need to clean this up--DRK

  Current_PU_Info = parallel_pu;
  Current_pu = &Current_PU_Info_pu();
  Current_Map_Tab = acc_pmaptab;

  //if (has_gtid)
  //  Add_DST_variable ( arg_gtid, nested_dst, line_number, DST_INVALID_IDX );
  //Add_DST_variable ( arg_slink, nested_dst, line_number, DST_INVALID_IDX );

}

void Finalize_Kernel_Parameters()
{
	map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor = acc_offload_scalar_management_tab.begin();
	for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
	{
		ST* st_scalar = itor->first;
		ACC_SCALAR_VAR_INFO* pvar_info = itor->second;
		if(pvar_info->acc_scalar_type == ACC_SCALAR_VAR_PRIVATE)
		{
			map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor1 = acc_offload_params_management_tab.find(st_scalar);
			acc_offload_params_management_tab.erase(itor1);
		}
	}

	WN* wn_params_list = NULL;
	//
	itor = acc_offload_params_management_tab.begin();
	for(; itor!=acc_offload_params_management_tab.end(); itor++)
	{		
		ST* st_scalar = itor->first;		
    	WN* wn_param = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_PARM, st_scalar, 0, 0);
		WN_next(wn_param) = wn_params_list;
		if(wn_params_list)
			WN_prev(wn_params_list) = wn_param;
		wn_params_list = wn_param;
	}
	acc_parms_nodes = wn_params_list;
	/*debug information output*/
	/////////////////////////////////////////////////////////////////////////
	//parameters output
	itor = acc_offload_params_management_tab.begin();
	printf("Offload Region Parameters:");
	for(; itor!=acc_offload_params_management_tab.end(); itor++)
	{		
		ST* st_scalar = itor->first;
		printf("%s\t", ST_name(st_scalar));	
	}
	printf("\n");
	/////////////////////////////////////////////////////////////////////////
	//first private
	printf("first private:");
	itor = acc_offload_scalar_management_tab.begin();
	for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
	{
		ST* st_scalar = itor->first;
		ACC_SCALAR_VAR_INFO* pvar_info = itor->second;
		if(pvar_info->acc_scalar_type == ACC_SCALAR_VAR_IN)
			printf("%s\t", ST_name(st_scalar));	
	}
	printf("\n");
	/////////////////////////////////////////////////////////////////////////
	//last private
	printf("last private:");
	itor = acc_offload_scalar_management_tab.begin();
	for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
	{
		ST* st_scalar = itor->first;
		ACC_SCALAR_VAR_INFO* pvar_info = itor->second;
		if(pvar_info->acc_scalar_type == ACC_SCALAR_VAR_OUT)
			printf("%s\t", ST_name(st_scalar));	
	}
	printf("\n");
	/////////////////////////////////////////////////////////////////////////
	//inout
	printf("inout:");
	itor = acc_offload_scalar_management_tab.begin();
	for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
	{
		ST* st_scalar = itor->first;
		ACC_SCALAR_VAR_INFO* pvar_info = itor->second;
		if(pvar_info->acc_scalar_type == ACC_SCALAR_VAR_INOUT)
			printf("%s\t", ST_name(st_scalar));	
	}
	printf("\n");
	/////////////////////////////////////////////////////////////////////////
	//private
	printf("private:");
	itor = acc_offload_scalar_management_tab.begin();
	for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
	{
		ST* st_scalar = itor->first;
		ACC_SCALAR_VAR_INFO* pvar_info = itor->second;
		if(pvar_info->acc_scalar_type == ACC_SCALAR_VAR_PRIVATE)
			printf("%s\t", ST_name(st_scalar));	
	}
	printf("\n");
	/////////////////////////////////////////////////////////////////////////
	//create
	printf("create:");
	itor = acc_offload_scalar_management_tab.begin();
	for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
	{
		ST* st_scalar = itor->first;
		ACC_SCALAR_VAR_INFO* pvar_info = itor->second;
		if(pvar_info->acc_scalar_type == ACC_SCALAR_VAR_CREATE)
			printf("%s\t", ST_name(st_scalar));	
	}
	printf("\n");
	/////////////////////////////////////////////////////////////////////////
	//present
	printf("present:");
	itor = acc_offload_scalar_management_tab.begin();
	for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
	{
		ST* st_scalar = itor->first;
		ACC_SCALAR_VAR_INFO* pvar_info = itor->second;
		if(pvar_info->acc_scalar_type == ACC_SCALAR_VAR_PRESENT)
			printf("%s\t", ST_name(st_scalar));	
	}
	printf("\n");
}

WN* ACC_Find_Scalar_Var_Inclose_Data_Clause(void* pRegionInfo, BOOL isKernelRegion)
{
	///////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////
	if(isKernelRegion)
	{
		KernelsRegionInfo* pKernelsRegionInfo;
		pKernelsRegionInfo = (KernelsRegionInfo*)pRegionInfo;
		map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor = acc_offload_scalar_management_tab.begin();
		//Get a pair list for kernel parameters.
		for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
		{
			ST* st_scalar = itor->first;
			ACC_SCALAR_VAR_INFO* pvar_info = itor->second;
			int i = 0;
			BOOL isFound = FALSE;
			int j = 0;
				
			TY_IDX ty = ST_type(st_scalar);
			TY_KIND kind = TY_kind(ty);//ST_name(old_st)			
			//if it is scalar var, skip it.
			//let's traverse all the tables
			while(j < acc_nested_dregion_info.Depth)
			{
				//acc_nested_dregion_info.DRegionInfo[j];
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->hostName == st_scalar)
					{
						pvar_info->bcreate_by_previous = TRUE;
						pvar_info->st_device_in_host = acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->deviceName;
						pvar_info->acc_scalar_type = ACC_SCALAR_VAR_INOUT;
						isFound = TRUE;
						break;
					}
					i ++;
				}
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyinMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->hostName == st_scalar)
					{
						pvar_info->bcreate_by_previous = TRUE;
						//no device name
						//pvar_info->st_device_in_host = acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->deviceName;
						pvar_info->acc_scalar_type = ACC_SCALAR_VAR_IN;
						isFound = TRUE;
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->hostName == st_scalar)
					{
						pvar_info->bcreate_by_previous = TRUE;
						pvar_info->st_device_in_host = acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->deviceName;
						pvar_info->acc_scalar_type = ACC_SCALAR_VAR_OUT;
						isFound = TRUE;
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcreateMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->hostName == st_scalar)
					{
						pvar_info->bcreate_by_previous = TRUE;
						pvar_info->st_device_in_host = acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->deviceName;						
						pvar_info->acc_scalar_type = ACC_SCALAR_VAR_CREATE;
						isFound = TRUE;
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].presentMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->hostName == st_scalar)
					{
						pvar_info->bcreate_by_previous = TRUE;
						pvar_info->st_device_in_host = acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->deviceName;
						pvar_info->acc_scalar_type = ACC_SCALAR_VAR_PRESENT;
						isFound = TRUE;
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				
				j++;
			}
			if(isFound == TRUE)
				continue;

			//Kernels Table
			i = 0;
			while(i < pKernelsRegionInfo->pcopyMap.size())
			{
				if(pKernelsRegionInfo->pcopyMap[i]->hostName == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					pvar_info->st_device_in_host = pKernelsRegionInfo->pcopyMap[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_INOUT;
					isFound = TRUE;
					break;
				}
				i ++;
			}
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pKernelsRegionInfo->pcopyinMap.size())
			{
				if(pKernelsRegionInfo->pcopyinMap[i]->hostName == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					//no device name
					//pvar_info->st_device_in_host = pKernelsRegionInfo->pcopyinMap[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_IN;
					isFound = TRUE;
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pKernelsRegionInfo->pcopyoutMap.size())
			{
				if(pKernelsRegionInfo->pcopyoutMap[i]->hostName == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					pvar_info->st_device_in_host = pKernelsRegionInfo->pcopyoutMap[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_OUT;
					isFound = TRUE;
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pKernelsRegionInfo->pcreateMap.size())
			{
				if(pKernelsRegionInfo->pcreateMap[i]->hostName == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					pvar_info->st_device_in_host = pKernelsRegionInfo->pcreateMap[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_CREATE;
					isFound = TRUE;
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pKernelsRegionInfo->presentMap.size())
			{
				if(pKernelsRegionInfo->presentMap[i]->hostName == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					pvar_info->st_device_in_host = pKernelsRegionInfo->presentMap[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_PRESENT;
					isFound = TRUE;
					break;
				}
				i ++;
			}			
		}
	}
	else
	{
		ParallelRegionInfo* pParallelRegionInfo;
		pParallelRegionInfo = (ParallelRegionInfo*)pRegionInfo;
		map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor = acc_offload_scalar_management_tab.begin();
		//Get a pair list for kernel parameters.
		for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
		{
			ST* st_scalar = itor->first;
			ACC_SCALAR_VAR_INFO* pvar_info = itor->second;
			int i = 0;
			BOOL isFound = FALSE;
			int j = 0;
				
			TY_IDX ty = ST_type(st_scalar);
			TY_KIND kind = TY_kind(ty);//ST_name(old_st)
			
			//let's traverse all the tables
			while(j < acc_nested_dregion_info.Depth)
			{
				//acc_nested_dregion_info.DRegionInfo[j];
				//
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->hostName == st_scalar)
					{
						pvar_info->bcreate_by_previous = TRUE;
						pvar_info->st_device_in_host = acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->deviceName;
						pvar_info->acc_scalar_type = ACC_SCALAR_VAR_INOUT;
						isFound = TRUE;
						break;
					}
					i ++;
				}
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyinMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->hostName == st_scalar)
					{
						pvar_info->bcreate_by_previous = TRUE;
						//no device name
						//pvar_info->st_device_in_host = acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->deviceName;
						pvar_info->acc_scalar_type = ACC_SCALAR_VAR_IN;
						isFound = TRUE;
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->hostName == st_scalar)
					{
						pvar_info->bcreate_by_previous = TRUE;
						pvar_info->st_device_in_host = acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->deviceName;
						pvar_info->acc_scalar_type = ACC_SCALAR_VAR_OUT;
						isFound = TRUE;
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcreateMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->hostName == st_scalar)
					{
						pvar_info->bcreate_by_previous = TRUE;
						pvar_info->st_device_in_host = acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->deviceName;
						pvar_info->acc_scalar_type = ACC_SCALAR_VAR_CREATE;
						isFound = TRUE;
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].presentMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->hostName == st_scalar)
					{
						pvar_info->bcreate_by_previous = TRUE;
						pvar_info->st_device_in_host = acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->deviceName;
						pvar_info->acc_scalar_type = ACC_SCALAR_VAR_PRESENT;
						isFound = TRUE;
						break;
					}
					i ++;
				}				
				if(isFound == TRUE)
					break;	
				
				j++;
			}
			if(isFound == TRUE)
				continue;

			//parallel region Table
			while(i < pParallelRegionInfo->pcopyMap.size())
			{
				if(pParallelRegionInfo->pcopyMap[i]->hostName == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					pvar_info->st_device_in_host = pParallelRegionInfo->pcopyMap[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_INOUT;
					isFound = TRUE;
					break;
				}
				i ++;
			}
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->pcopyinMap.size())
			{
				if(pParallelRegionInfo->pcopyinMap[i]->hostName == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					//no device name
					//pvar_info->st_device_in_host = pParallelRegionInfo->pcopyinMap[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_IN;
					isFound = TRUE;
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->pcopyoutMap.size())
			{
				if(pParallelRegionInfo->pcopyoutMap[i]->hostName == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					pvar_info->st_device_in_host = pParallelRegionInfo->pcopyoutMap[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_OUT;
					isFound = TRUE;
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->pcreateMap.size())
			{
				if(pParallelRegionInfo->pcreateMap[i]->hostName == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					pvar_info->st_device_in_host = pParallelRegionInfo->pcreateMap[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_CREATE;
					isFound = TRUE;
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->presentMap.size())
			{
				if(pParallelRegionInfo->presentMap[i]->hostName == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					pvar_info->st_device_in_host = pParallelRegionInfo->presentMap[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_PRESENT;
					isFound = TRUE;
					break;
				}
				i ++;
			}
			
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->acc_dregion_fprivate.size())
			{
				if(WN_st(pParallelRegionInfo->acc_dregion_fprivate[i].acc_data_clauses) == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					//pvar_info->st_device_in_host = pParallelRegionInfo->acc_dregion_fprivate[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_IN;
					isFound = TRUE;
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->acc_dregion_private.size())
			{
				if(WN_st(pParallelRegionInfo->acc_dregion_private[i].acc_data_clauses) == st_scalar)
				{
					pvar_info->bcreate_by_previous = TRUE;
					//pvar_info->st_device_in_host = pParallelRegionInfo->acc_dregion_fprivate[i]->deviceName;
					pvar_info->acc_scalar_type = ACC_SCALAR_VAR_PRIVATE;
					isFound = TRUE;
					break;
				}
				i ++;
			}
		}
	}
}

static void ACC_Add_Scalar_Variable_Info(ST* st_name, ACC_SCALAR_TYPE var_property)
{	
	//record as parameters, during the finalization the private vars will be removed.
	acc_offload_params_management_tab[st_name] = NULL;

	//now only take care of scalar variable	
	if(TY_kind(ST_type(st_name)) != KIND_SCALAR && TY_kind(ST_type(st_name)) != KIND_ARRAY)
	{
		return;
	}
	TY_IDX ty = ST_type(st_name);
	
	map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor;
	itor = acc_offload_scalar_management_tab.find(st_name);
	//if we didn't find it, new entry then.
	if(itor == acc_offload_scalar_management_tab.end())
	{
		ACC_SCALAR_VAR_INFO* varinfo = new ACC_SCALAR_VAR_INFO;
		varinfo->acc_scalar_type = var_property;
		varinfo->st_var = st_name;
		varinfo->is_reduction = FALSE;
		varinfo->isize = TY_size(ty);	
		//bcreate_by_previous is false
		varinfo->bcreate_by_previous = FALSE;
		acc_offload_scalar_management_tab[st_name] = varinfo;
	}
	//find it, however, the property is private, then updated.
	else if(itor != acc_offload_scalar_management_tab.end() 
			&& var_property == ACC_SCALAR_VAR_PRIVATE)
	{
		ACC_SCALAR_VAR_INFO* varinfo = acc_offload_scalar_management_tab[st_name];
		varinfo->acc_scalar_type = ACC_SCALAR_VAR_PRIVATE;
	}
}

/*
Scan the offload region, and find out all the scalar variables 
which are not explicitly identified in kernels/parallel region or 
any enclosing data construct
*/
static WN* ACC_Scan_Offload_Region(WN* wn_offload_region, BOOL bIsKernels)
{
	WN_TREE_ITER<POST_ORDER> tree_iter(wn_offload_region);
	ACC_SCALAR_TYPE default_property = ACC_SCALAR_VAR_IN;
	if(bIsKernels)
			default_property = ACC_SCALAR_VAR_INOUT;
	else 
			default_property = ACC_SCALAR_VAR_IN;
	while (tree_iter != LAST_POST_ORDER_ITER)
	{
		OPCODE opc;
		OPERATOR opr;
        WN* wn = tree_iter.Wn();
		opr = WN_operator(wn);
		opc = WN_opcode(wn);
		//find the index variable which should be private
		if(opc == OPC_DO_LOOP)
		{
			WN* wn_index = WN_index(wn);
			ST* st_index = WN_st(wn_index);
			ACC_Add_Scalar_Variable_Info(st_index, ACC_SCALAR_VAR_PRIVATE);
		}
        switch (WN_operator(wn)) 
		{
            case OPR_LDID:
			case OPR_STID:
			case OPR_LDA:
			{
                ST *used_var = WN_st(wn);
				if(ST_sym_class(used_var) == CLASS_PREG)					
					ACC_Add_Scalar_Variable_Info(used_var, ACC_SCALAR_VAR_PRIVATE);
				else
					ACC_Add_Scalar_Variable_Info(used_var, default_property);
			}
                break;
			default:
				//I don't care at this time.
				break;
        }
		tree_iter ++;
    }
}

void ACC_Process_scalar_variable_for_offload_region(WN* wn_block, 
				BOOL bIsKernels)
{
	//acc_offload_scalar_management_tab
	UINT32 vindex;
	if(acc_dfa_enabled)
	{
		for(vindex=0; vindex<acc_dregion_inout_scalar.size(); vindex++)
		{
			ACC_DREGION__ENTRY entry = acc_dregion_inout_scalar[vindex];
			//Now only scalar var is supported in private/first private/lastprivate/inout pragma
			//ignore length clause
			ST* st_var = WN_st(entry.acc_data_clauses);
			ACC_SCALAR_VAR_INFO* pVarInfo = acc_offload_scalar_management_tab[st_var];
			if(pVarInfo)
				delete pVarInfo;
			pVarInfo = new ACC_SCALAR_VAR_INFO;
			acc_offload_scalar_management_tab[st_var] = pVarInfo;
			pVarInfo->acc_scalar_type = ACC_SCALAR_VAR_INOUT;
			pVarInfo->st_var = st_var;
			pVarInfo->is_reduction = FALSE;
			pVarInfo->is_across_gangs = FALSE;
		}
		for(vindex=0; vindex<acc_dregion_lprivate.size(); vindex++)
		{
			ACC_DREGION__ENTRY entry = acc_dregion_lprivate[vindex];
			//Now only scalar var is supported in private/first private/lastprivate/inout pragma
			//ignore length clause
			ST* st_var = WN_st(entry.acc_data_clauses);
			ACC_SCALAR_VAR_INFO* pVarInfo = acc_offload_scalar_management_tab[st_var];
			if(pVarInfo)
				delete pVarInfo;
			pVarInfo = new ACC_SCALAR_VAR_INFO;
			acc_offload_scalar_management_tab[st_var] = pVarInfo;
			pVarInfo->acc_scalar_type = ACC_SCALAR_VAR_OUT;
			pVarInfo->st_var = st_var;
			pVarInfo->is_reduction = FALSE;
			pVarInfo->is_across_gangs = FALSE;
		}
		for(vindex=0; vindex<acc_dregion_fprivate.size(); vindex++)
		{
			ACC_DREGION__ENTRY entry = acc_dregion_fprivate[vindex];
			//Now only scalar var is supported in private/first private/lastprivate/inout pragma
			//ignore length clause
			ST* st_var = WN_st(entry.acc_data_clauses);
			ACC_SCALAR_VAR_INFO* pVarInfo = acc_offload_scalar_management_tab[st_var];
			if(pVarInfo)
				delete pVarInfo;
			pVarInfo = new ACC_SCALAR_VAR_INFO;
			acc_offload_scalar_management_tab[st_var] = pVarInfo;
			pVarInfo->acc_scalar_type = ACC_SCALAR_VAR_IN;
			pVarInfo->st_var = st_var;
			pVarInfo->is_reduction = FALSE;
			pVarInfo->is_across_gangs = FALSE;
		}
		for(vindex=0; vindex<acc_dregion_private.size(); vindex++)
		{
			ACC_DREGION__ENTRY entry = acc_dregion_private[vindex];
			//Now only scalar var is supported in private/first private/lastprivate/inout pragma
			//ignore length clause
			ST* st_var = WN_st(entry.acc_data_clauses);
			ACC_SCALAR_VAR_INFO* pVarInfo = acc_offload_scalar_management_tab[st_var];
			if(pVarInfo)
				delete pVarInfo;
			pVarInfo = new ACC_SCALAR_VAR_INFO;
			acc_offload_scalar_management_tab[st_var] = pVarInfo;
			pVarInfo->acc_scalar_type = ACC_SCALAR_VAR_PRIVATE;
			pVarInfo->st_var = st_var;
			pVarInfo->is_reduction = FALSE;
			pVarInfo->is_across_gangs = FALSE;
		}
	}
	else
	{
		ACC_Scan_Offload_Region(wn_block, bIsKernels);
		//now scan the enclose data clauses, and change the scalar property if necessary
	}
	//acc_dregion_private;		
	//acc_dregion_fprivate;	
	//acc_dregion_lprivate;
	//acc_dregion_inout_scalar;
}

/**********************************************************************************/
//Generate GPU kernels parameters from acc kernels/parallel block
//isKernelRegion, if it is acc kernels region, should be set as TRUE.
// 				if it is the acc parallel region, should be set as FALSE.
/**********************************************************************************/

BOOL ACC_Check_Pointer_Exist_inClause(ST* st_param, void* pRegionInfo, BOOL isKernelRegion)
{
	ParallelRegionInfo* pParallelRegionInfo;
	KernelsRegionInfo* pKernelsRegionInfo;
	WN* wn;
	if(isKernelRegion)
	{
		pKernelsRegionInfo = (KernelsRegionInfo*)pRegionInfo;
		//Get a pair list for kernel parameters.
		//for(wn=paramlist; wn; wn=WN_next(wn))
		{
			//ST* st_param = WN_st(wn);
			int i = 0;
			BOOL isFound = FALSE;
			int j = 0;
				
			TY_IDX ty = ST_type(st_param);
			TY_KIND kind = TY_kind(ty);//ST_name(old_st)
			//if it is scalar var, skip it.
			//let's traverse all the tables
			while(j < acc_nested_dregion_info.Depth)
			{
				//acc_nested_dregion_info.DRegionInfo[j];
				//
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->hostName == st_param)
					{
						return TRUE;
					}
					i ++;
				}
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyinMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->hostName == st_param)
					{
						return TRUE;
					}
					i ++;
				}
				
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->hostName == st_param)
					{
						return TRUE;
					}
					i ++;
				}				
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcreateMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->hostName == st_param)
					{
						return TRUE;
					}
					i ++;
				}
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].presentMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->hostName == st_param)
					{
						return TRUE;
					}
					i ++;
				}
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].dptrList.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].dptrList[i] == st_param)
					{
						return TRUE;
					}
					i ++;
				}				
				j++;
			}
			
			//Kernels Table
			while(i < pKernelsRegionInfo->pcopyMap.size())
			{
				if(pKernelsRegionInfo->pcopyMap[i]->hostName == st_param)
				{
					return TRUE;
				}
				i ++;
			}
			i = 0;
			while(i < pKernelsRegionInfo->pcopyinMap.size())
			{
				if(pKernelsRegionInfo->pcopyinMap[i]->hostName == st_param)
				{
						return TRUE;
				}
				i ++;
			}
			
			i = 0;
			while(i < pKernelsRegionInfo->pcopyoutMap.size())
			{
				if(pKernelsRegionInfo->pcopyoutMap[i]->hostName == st_param)
				{
						return TRUE;
				}
				i ++;
			}
			
			i = 0;
			while(i < pKernelsRegionInfo->pcreateMap.size())
			{
				if(pKernelsRegionInfo->pcreateMap[i]->hostName == st_param)
				{
						return TRUE;
				}
				i ++;
			}
			i = 0;
			while(i < pKernelsRegionInfo->presentMap.size())
			{
				if(pKernelsRegionInfo->presentMap[i]->hostName == st_param)
				{
						return TRUE;
				}
				i ++;
			}			
			
			i = 0;
			while(i < pKernelsRegionInfo->dptrList.size())
			{
				if(pKernelsRegionInfo->dptrList[i] == st_param)
				{
						return TRUE;
				}
				i ++;
			}
			return FALSE;
		}
	}
	else
	{
		pParallelRegionInfo = (ParallelRegionInfo*)pRegionInfo;
		//Get a pair list for kernel parameters.
		//for(wn=paramlist; wn; wn=WN_next(wn))
		{
			//ST* st_param = WN_st(wn);
			int i = 0;
			int j = 0;
				
			TY_IDX ty = ST_type(st_param);
			TY_KIND kind = TY_kind(ty);//ST_name(old_st)
			
			//let's traverse all the tables
			while(j < acc_nested_dregion_info.Depth)
			{
				//acc_nested_dregion_info.DRegionInfo[j];
				//
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->hostName == st_param)
					{
						return TRUE;
					}
					i ++;
				}

				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyinMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->hostName == st_param)
					{
						return TRUE;
					}
					i ++;
				}
				

				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->hostName == st_param)
					{
						return TRUE;
					}
					i ++;
				}
				

				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcreateMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->hostName == st_param)
					{
						return TRUE;
					}
					i ++;
				}
				

				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].presentMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->hostName == st_param)
					{
						return TRUE;
					}
					i ++;
				}
				

				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].dptrList.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].dptrList[i] == st_param)
					{
						return TRUE;
					}
					i ++;
				}
			
				
				j++;
			}


			//Kernels Table
			while(i < pParallelRegionInfo->pcopyMap.size())
			{
				if(pParallelRegionInfo->pcopyMap[i]->hostName == st_param)
				{
						return TRUE;
				}
				i ++;
			}
			i = 0;
			while(i < pParallelRegionInfo->pcopyinMap.size())
			{
				if(pParallelRegionInfo->pcopyinMap[i]->hostName == st_param)
				{
						return TRUE;
				}
				i ++;
			}
			i = 0;
			while(i < pParallelRegionInfo->pcopyoutMap.size())
			{
				if(pParallelRegionInfo->pcopyoutMap[i]->hostName == st_param)
				{
						return TRUE;
				}
				i ++;
			}
			i = 0;
			while(i < pParallelRegionInfo->pcreateMap.size())
			{
				if(pParallelRegionInfo->pcreateMap[i]->hostName == st_param)
				{
						return TRUE;
				}
				i ++;
			}
			
			i = 0;
			while(i < pParallelRegionInfo->presentMap.size())
			{
				if(pParallelRegionInfo->presentMap[i]->hostName == st_param)
				{
						return TRUE;
				}
				i ++;
			}
			
			i = 0;
			while(i < pParallelRegionInfo->dptrList.size())
			{
				if(pParallelRegionInfo->dptrList[i] == st_param)
				{
						return TRUE;
				}
				i ++;
			}
			return FALSE;
		}
	}
}

WN* ACC_Make_Array_ref(ST *base, WN* wn_offset, WN* wn_dim)
{
    WN *arr_ref = WN_Create( OPCODE_make_op(OPR_ARRAY,Pointer_Mtype,MTYPE_V),3);
	if(TY_kind(ST_type(base)) == KIND_POINTER)
    	WN_element_size(arr_ref) = TY_size(TY_pointed(ST_type(base)));
	else if(TY_kind(ST_type(base)) == KIND_ARRAY)
    	WN_element_size(arr_ref) = TY_size(TY_etype(ST_type(base)));
    WN_array_base(arr_ref) = WN_Lda(Pointer_type, 0, base);
    WN_array_index(arr_ref,0) = wn_offset;
    WN_array_dim(arr_ref,0) = wn_dim;
    return arr_ref;
} /* make_array_ref */


/*
Walk the tree, replacing global references with local ones.  Within
parallel regions, also translate label numbers from those of the parent PU
to those of the child, and generate new INITO/INITV structures (for e.g.
C++ exception handling blocks) for the child PU.

Argument is_par_region must be TRUE iff tree is an MP construct that's a
parallel region.

In a non-recursive call to this routine, output argument
non_pod_finalization must point to a NULL WN *. Upon return,
(*non_pod_finalization) points to the non-POD finalization IF node (if one
was found in the tree), and this IF node is removed from the tree; the IF
node cannot have been the "tree" argument in the non-recursive call.

Note that within orphaned worksharing constructs, non-POD variables have
been localized already by the frontend, so we don't rewrite references to
such variables that appear in vtab.

In a non-recursive call to this routine, it is guaranteed that if the root
node of tree is not a load or store (e.g. it's a DO_LOOP or block), that
root node will not be replaced.
*/
WN * ACC_Walk_and_Localize (WN * tree)
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

  if (opr == OPR_LDID) 
  {
    old_sym = WN_st(tree);
    old_offset = WN_offsetx(tree);
	map<ST*, ACC_VAR_TABLE>::iterator itor = acc_local_new_var_map.find(old_sym);
	if(itor != acc_local_new_var_map.end())	
    //for (w=vtab; w->orig_st; w++) 
	{
	  ACC_VAR_TABLE newVar = itor->second;
      if ((newVar.orig_st == old_sym) &&
	  (newVar.has_offset ? (newVar.orig_offset == old_offset) : TRUE )) 
	  {	
  		  WN_st_idx(tree) = ST_st_idx(newVar.new_st);
		  // for reduction of a field of STRUCT, the TY_kind would be different
		  // And, we need to fix the TY for the wn, the field_id, and offsetx
		  // As the local_xxx symbol is always .predef..., so field_id should be 0
		  if (TY_kind(ST_type(newVar.new_st)) != TY_kind(WN_ty(tree)))
		  {
	        WN_set_ty(tree, ST_type(newVar.new_st));
	    	WN_set_field_id(tree, 0);
		  }
		  if (newVar.has_offset)
		    ACC_WN_set_offsetx(tree, newVar.new_offset);
      }
    }
  }
  else if (opr == OPR_STID)
  {
    old_sym = WN_st(tree);
    old_offset = WN_offsetx(tree);
	map<ST*, ACC_VAR_TABLE>::iterator itor = acc_local_new_var_map.find(old_sym);
	if(itor != acc_local_new_var_map.end())	
    //for (w=vtab; w->orig_st; w++) 
	{
	  ACC_VAR_TABLE newVar = itor->second;
      if ((newVar.orig_st == old_sym) &&
	  		(newVar.has_offset ? (newVar.orig_offset == old_offset) : TRUE )) 
	  {
		WN_st_idx(tree) = ST_st_idx(newVar.new_st);
		if (TY_kind(ST_type(newVar.new_st)) != TY_kind(WN_ty(tree)))
		{
				WN_set_ty(tree, ST_type(newVar.new_st));
				WN_set_field_id(tree, 0);
		}
		if (newVar.has_offset)
				ACC_WN_set_offsetx(tree, newVar.new_offset);	
      }
    }
  } 
  else if (opr == OPR_ARRAY && WN_num_dim(tree)==1)
  {
	WN* wn_base = WN_array_base(tree) ;
    old_sym = WN_st(wn_base);
	ST* new_sym = NULL;
	UINT32 esize = WN_element_size(tree);
	WN* wn_index = WN_COPY_Tree(WN_kid(tree, 2));
	WN* wn_offset = WN_Binary(OPR_MPY, 
						MTYPE_U4, 
						wn_index, 
						WN_Intconst(MTYPE_U4, esize));
	
	map<ST*, ACC_VAR_TABLE>::iterator itor = acc_local_new_var_map.find(old_sym);
	
	if(itor != acc_local_new_var_map.end())	
	{
	  	ACC_VAR_TABLE newVar = itor->second;
		new_sym = newVar.new_st;
		WN* wn_ldidbase = WN_Ldid(Pointer_type, 0, new_sym, ST_type(new_sym));
		WN* newtree = WN_Binary(OPR_ADD, 
					MTYPE_U4, 
					wn_offset, 
					wn_ldidbase);
		WN_DELETE_Tree(tree);
		tree = (newtree);
	}
  }
  else if (opr == OPR_ARRAY && WN_num_dim(tree)>1) 
  {
  	int idim = WN_num_dim(tree);
	int ii = 0;
	WN* wn_offset = NULL;
	WN* wn_base = WN_array_base(tree) ;
	//WN* wn_dimInOne = NULL;;
    	old_sym = WN_st(wn_base);
	ST* new_sym = NULL;
	INT32 esize = WN_element_size(tree);
	
	if(esize < 0)
	{
		if(F90_ST_Has_Dope_Vector(old_sym))
		{
			FLD_HANDLE  fli ;
			TY_IDX base_pointer_ty;
			fli = TY_fld(Ty_Table[ST_type(old_sym)]);
			base_pointer_ty = FLD_type(fli);
			TY_IDX e_ty = TY_etype(TY_pointed(base_pointer_ty));
			esize = TY_size(e_ty);
		}
		else
			Fail_FmtAssertion ("Unknown array element in WalkandLocalize");
	}
	
	map<ST*, ACC_VAR_TABLE>::iterator itor = acc_local_new_var_map.find(old_sym);
	
	if(itor != acc_local_new_var_map.end())	
    //for (w=vtab; w->orig_st; w++) 
	{
	  	ACC_VAR_TABLE newVar = itor->second;
		new_sym = newVar.new_st;
		//WN_num_dim
		//WN_array_dim(x,i)
		//WN_array_index(x, i)
		if(new_sym)
		{
			WN* wn_ldidbase = WN_Ldid(Pointer_type, 0, new_sym, ST_type(new_sym));
			TYPE_ID mtype_base_id = WN_rtype(wn_ldidbase);
			///////////////////////////////////////////////////////////////////////
			//begin translation
			for(ii=0; ii<idim-1; ii++)
			{
				WN* wn_index = WN_array_index(tree, ii);
				//TY_IDX idx_ty = WN_rtype(wn_index);
				//if(WN_has_sym(wn_index))
				//	idx_ty = TY_mtype(ST_type(WN_st(wn_index)));
				//else
				//	Is_True(WN_has_sym(wn_index), ("WN_st: wn doesn't have ST field"));
				wn_index = WN_COPY_Tree(wn_index);
				int iii = ii;
				while(iii<idim-1)
				{
					WN* wn_dim = WN_array_dim(tree, iii+1);
					if(WN_rtype(wn_dim) != mtype_base_id)
					{
						wn_dim = ACC_WN_Integer_Cast(WN_COPY_Tree(wn_dim), mtype_base_id, WN_rtype(wn_dim));
					}
					if(WN_rtype(wn_index) != mtype_base_id)
                                        {
                                                wn_index = ACC_WN_Integer_Cast(WN_COPY_Tree(wn_index), mtype_base_id, WN_rtype(wn_index));
                                        }
			   		wn_index = WN_Binary(OPR_MPY, 
			   						mtype_base_id, 
			   						wn_index, 
			   						wn_dim);
					iii ++;
				}
				if(wn_offset)
		   			wn_offset = WN_Binary(OPR_ADD, 
			   					mtype_base_id, 
		   						wn_offset, 
		   						wn_index);
				else
					wn_offset = wn_index;
				/*if(wn_dimInOne == NULL)
					wn_dimInOne = WN_Binary(OPR_MPY, 
			   					WN_rtype(wn_index), 
			   						WN_COPY_Tree(WN_array_dim(tree, ii)), 
			   						WN_COPY_Tree(WN_array_dim(tree, ii+1)));
				else
					wn_dimInOne = WN_Binary(OPR_MPY, 
			   					WN_rtype(wn_index), 
			   						wn_dimInOne, 
			   						WN_COPY_Tree(WN_array_dim(tree, ii+1)));*/
			}
			WN* wn_index = WN_array_index(tree, ii);
			if(WN_rtype(wn_index) != mtype_base_id)
                        {
                               wn_index = ACC_WN_Integer_Cast(WN_COPY_Tree(wn_index), mtype_base_id, WN_rtype(wn_index));
                        }

			if(wn_offset)
				wn_offset = WN_Binary(OPR_ADD, 
			   				WN_rtype(wn_index), 
							wn_index, 
							wn_offset);
			else
				wn_offset = WN_COPY_Tree(wn_index);
			wn_offset = WN_Binary(OPR_MPY, 
						mtype_base_id, 
						wn_offset, 
						WN_Intconst(mtype_base_id, esize));
			//wnx = WN_Lda( Pointer_type, 0, new_sym);
			//Set_TY_align(ST_type(new_sym), 4);
			//WN* wn_ldidbase = WN_Ldid(Pointer_type, 0, new_sym, ST_type(new_sym));
			//WN_Lda( Pointer_type, 0, new_sym);
			//WN_Ldid(Pointer_type, 0, new_sym, ST_type(new_sym));
			WN* newtree = WN_Binary(OPR_ADD, 
						mtype_base_id, 
						wn_offset, 
						wn_ldidbase);
			//WN_prev(newtree) = WN_prev(tree);
			//WN_next(newtree) = WN_next(tree);
			//WN* newtree = ACC_Make_Array_ref(new_sym, wn_offset, wn_dimInOne);
			WN_DELETE_Tree(tree);
			tree = (newtree);
		}
   }
  }
  else if (OPCODE_has_sym(op) && WN_st(tree)) 
  {
    old_sym = WN_st(tree);
    old_offset = OPCODE_has_offset(op) ? WN_offsetx(tree) : 0;
	
	map<ST*, ACC_VAR_TABLE>::iterator itor = acc_local_new_var_map.find(old_sym);
	
	if(itor != acc_local_new_var_map.end())	
	{
	  	ACC_VAR_TABLE newVar = itor->second;
	    //for (w=vtab; w->orig_st; w++) 
	      if ((newVar.orig_st == old_sym) &&
		  		(newVar.has_offset ? (newVar.orig_offset == old_offset) : TRUE )) 
		  {
			WN_st_idx(tree) = ST_st_idx(newVar.new_st);
			if (OPCODE_has_offset(op) && newVar.has_offset)
			  ACC_WN_set_offsetx(tree, newVar.new_offset);
	      }
	}
  }

  /* Walk all children */

  if (op == OPC_BLOCK) 
  {
    r = WN_first(tree);
    while (r) 
	{ // localize each node in block
      r = ACC_Walk_and_Localize ( r);
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
      WN_kid(tree, i) = ACC_Walk_and_Localize ( WN_kid(tree, i));
    }
  }
  return (tree);
}   


void ACC_Process_Clause_Pragma(WN * tree)
{
	 // Process_PDO() parses clauses of orphanded PDO
	 //cont = (mpt != MPP_ORPHANED_PDO);
	 WN* wn_next_node = tree;
	 WN* wn_cur_node;
	 WN* wn;
	 WN* wn_tmp;
	 WN* wn_kid_num;
	 UINT32 ikid_num = 0;
	 WN *acc_dstart_node;
     WN *acc_dlength_node;
	 
	 while ((wn_cur_node = wn_next_node)) 
	 {
	
	   wn_next_node = WN_next(wn_cur_node);
	
	   if (((WN_opcode(wn_cur_node) == OPC_PRAGMA) ||
			(WN_opcode(wn_cur_node) == OPC_XPRAGMA)) &&
		   (WN_pragmas[WN_pragma(wn_cur_node)].users & PUSER_ACC)) 
	   {
	
		 {	
			   switch (WN_pragma(wn_cur_node)) 
			   {
			
				 case WN_PRAGMA_ACC_CLAUSE_IF:
				   if (acc_if_node)
					 WN_DELETE_Tree ( acc_if_node );
				   acc_if_node = WN_kid0(wn_cur_node);
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_REDUCTION:
				   for (wn = acc_reduction_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
					   break;
				   if (wn == NULL) 
				   {
						 if (WN_opcode(wn_cur_node) != OPC_PRAGMA &&
						   WN_operator(WN_kid0(wn_cur_node)) == OPR_ARRAY &&
						   OPCODE_has_sym(WN_opcode(WN_kid0(WN_kid0(wn_cur_node)))) == 0) 
						   {
							   WN_DELETE_Tree ( wn_cur_node );
						   } 
						   else 
						   {
							   WN_next(wn_cur_node) = acc_reduction_nodes;
							   acc_reduction_nodes = wn_cur_node;
							   ++acc_reduction_count;
						   }
				   } 
				   else
					 WN_DELETE_Tree ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_DATA_START:
				 case WN_PRAGMA_ACC_CLAUSE_DATA_LENGTH:
				 	continue;
				   
				 case WN_PRAGMA_ACC_CLAUSE_PRESENT:
				   for (wn = acc_present_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   		break;
					 
				   if (wn == NULL) 
				   {					 
						wn_kid_num = WN_COPY_Tree(WN_kid0(wn_cur_node));
						ikid_num = WN_const_val(wn_kid_num);
						wn_tmp = wn_next_node;
						ACC_DREGION__ENTRY entry;
						entry.acc_data_clauses = wn_cur_node;	
						ST* st_name = WN_st(wn_cur_node);
						if(TY_kind(ST_type(st_name)) == KIND_SCALAR)
						{
							entry.acc_scalar_type = ACC_SCALAR_VAR_PRESENT;
						}					
						/*ST *old_st = WN_st(wn_cur_node);
						if(F90_ST_Has_Dope_Vector(old_st))
						{
							//if it is the dope st, then the real start addr is in the next WHIRL node.
							entry.acc_data_start_addr = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
						}*/
						
						while(ikid_num)
						{					 	
							acc_dstart_node = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
							acc_dlength_node = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
							entry.acc_data_start.push_back(acc_dstart_node);
							entry.acc_data_length.push_back(acc_dlength_node);							
							ikid_num --;
						}
						
						acc_dregion_present.push_back(entry);
						/////////////////////////////////////////
						WN_next(wn_cur_node) = acc_present_nodes;
						acc_present_nodes = wn_cur_node;
						//++acc_present_count;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;	
				   
				 case WN_PRAGMA_ACC_CLAUSE_USE_DEVICE:
				   for (wn = acc_use_device_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   		break;
					 
				   if (wn == NULL) 
				   {					 
						wn_kid_num = WN_COPY_Tree(WN_kid0(wn_cur_node));
						ikid_num = WN_const_val(wn_kid_num);
						wn_tmp = wn_next_node;
						ACC_DREGION__ENTRY entry;
						entry.acc_data_clauses = wn_cur_node;	
						
						while(ikid_num)
						{					 	
							acc_dstart_node = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
							acc_dlength_node = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
							entry.acc_data_start.push_back(acc_dstart_node);
							entry.acc_data_length.push_back(acc_dlength_node);							
							ikid_num --;
						}
						
						acc_host_data_use_device.push_back(entry);
						/////////////////////////////////////////
						WN_next(wn_cur_node) = acc_use_device_nodes;
						acc_use_device_nodes = wn_cur_node;
						//++acc_present_count;
				   } 
				   else
				   	WN_Delete ( wn_cur_node );
				   break;	
				   
				 case WN_PRAGMA_ACC_CLAUSE_DELETE:
				   for (wn = acc_delete_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   		break;
					 
				   if (wn == NULL) 
				   {					 
						wn_kid_num = WN_COPY_Tree(WN_kid0(wn_cur_node));
						ikid_num = WN_const_val(wn_kid_num);
						wn_tmp = wn_next_node;
						ACC_DREGION__ENTRY entry;
						entry.acc_data_clauses = wn_cur_node;	
						
						while(ikid_num)
						{					 	
							acc_dstart_node = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
							acc_dlength_node = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
							entry.acc_data_start.push_back(acc_dstart_node);
							entry.acc_data_length.push_back(acc_dlength_node);							
							ikid_num --;
						}
						
						acc_dregion_delete.push_back(entry);
						/////////////////////////////////////////
						WN_next(wn_cur_node) = acc_delete_nodes;
						acc_delete_nodes = wn_cur_node;
						//++acc_present_count;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_COPY:
				 case WN_PRAGMA_ACC_CLAUSE_PRESENT_OR_COPY:
				   for (wn = acc_present_or_copy_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   			break;
					 
				   if (wn == NULL) 
				   {
				   	 	wn_kid_num = WN_COPY_Tree(WN_kid0(wn_cur_node));
						ikid_num = WN_const_val(wn_kid_num);
						wn_tmp = wn_next_node;
						ACC_DREGION__ENTRY entry;
						entry.acc_data_clauses = wn_cur_node;
						ST* st_name = WN_st(wn_cur_node);
						if(TY_kind(ST_type(st_name)) == KIND_SCALAR)
						{
							entry.acc_scalar_type = ACC_SCALAR_VAR_INOUT;
						}
						
						while(ikid_num)
						{					 	
							acc_dstart_node = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
							acc_dlength_node = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
							entry.acc_data_start.push_back(acc_dstart_node);
							entry.acc_data_length.push_back(acc_dlength_node);							
							ikid_num --;
						}
						
					 	acc_dregion_pcopy.push_back(entry);
					 	///////////////////////////////////////////
					 	WN_next(wn_cur_node) = acc_present_or_copy_nodes;
					 	acc_present_or_copy_nodes = wn_cur_node;
					 //++acc_present_or_copy_count;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_COPYIN:
				 case WN_PRAGMA_ACC_CLAUSE_PRESENT_OR_COPYIN:
				   for (wn = acc_present_or_copyin_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   			break;
				   
				   if (wn == NULL) 
				   {				   	 
				   	 	wn_kid_num = WN_COPY_Tree(WN_kid0(wn_cur_node));
						ikid_num = WN_const_val(wn_kid_num);
						wn_tmp = wn_next_node;
						ACC_DREGION__ENTRY entry;
						entry.acc_data_clauses = wn_cur_node;
						ST* st_name = WN_st(wn_cur_node);
						if(TY_kind(ST_type(st_name)) == KIND_SCALAR)
						{
							entry.acc_scalar_type = ACC_SCALAR_VAR_IN;
						}
						while(ikid_num)
						{					 	
							acc_dstart_node = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
							acc_dlength_node = WN_COPY_Tree(WN_kid0(wn_tmp));
							wn_tmp = WN_next(wn_tmp);
							entry.acc_data_start.push_back(acc_dstart_node);
							entry.acc_data_length.push_back(acc_dlength_node);							
							ikid_num --;
						}
					 	acc_dregion_pcopyin.push_back(entry);
					 	///////////////////////////////////////////
					 	WN_next(wn_cur_node) = acc_present_or_copyin_nodes;
					 	acc_present_or_copyin_nodes = wn_cur_node;
					 //++acc_present_or_copyin_count;
				   } 
				   else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_COPYOUT:
				 case WN_PRAGMA_ACC_CLAUSE_PRESENT_OR_COPYOUT:
				   for (wn = acc_present_or_copyout_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   			break;
				   
				   if (wn == NULL) 
				   {					   
					   wn_kid_num = WN_COPY_Tree(WN_kid0(wn_cur_node));
					   ikid_num = WN_const_val(wn_kid_num);
					   wn_tmp = wn_next_node;
					   ACC_DREGION__ENTRY entry;
					   entry.acc_data_clauses = wn_cur_node;
						ST* st_name = WN_st(wn_cur_node);
						if(TY_kind(ST_type(st_name)) == KIND_SCALAR)
						{
							entry.acc_scalar_type = ACC_SCALAR_VAR_OUT;
						}
					   
					   while(ikid_num)
					   {					   
						   acc_dstart_node = WN_COPY_Tree(WN_kid0(wn_tmp));
						   wn_tmp = WN_next(wn_tmp);
						   acc_dlength_node = WN_COPY_Tree(WN_kid0(wn_tmp));
						   wn_tmp = WN_next(wn_tmp);
						   entry.acc_data_start.push_back(acc_dstart_node);
						   entry.acc_data_length.push_back(acc_dlength_node);						   
						   ikid_num --;
					   }
					   
					   acc_dregion_pcopyout.push_back(entry);
					   ///////////////////////////////////////////
					   WN_next(wn_cur_node) = acc_present_or_copyout_nodes;
					   acc_present_or_copyout_nodes = wn_cur_node;
					 //++acc_present_or_copyout_count;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_HOST:
				   for (wn = acc_host_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   		break;
				   
				   if (wn == NULL) 
				   {				   	 			   
					   wn_kid_num = WN_COPY_Tree(WN_kid0(wn_cur_node));
					   ikid_num = WN_const_val(wn_kid_num);
					   wn_tmp = wn_next_node;
					   ACC_DREGION__ENTRY entry;
					   entry.acc_data_clauses = wn_cur_node;
					   
					   while(ikid_num)
					   {					   
						   acc_dstart_node = WN_COPY_Tree(WN_kid0(wn_tmp));
						   wn_tmp = WN_next(wn_tmp);
						   acc_dlength_node = WN_COPY_Tree(WN_kid0(wn_tmp));
						   wn_tmp = WN_next(wn_tmp);
						   entry.acc_data_start.push_back(acc_dstart_node);
						   entry.acc_data_length.push_back(acc_dlength_node);						   
						   ikid_num --;
					   }
					   acc_dregion_host.push_back(entry);
					   ///////////////////////////////////////////
					   WN_next(wn_cur_node) = acc_host_nodes;
					   acc_host_nodes = wn_cur_node;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_DEVICE:
				   for (wn = acc_device_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   		break;
				   
				   if (wn == NULL) 
				   {				   	 				   	 			   
					   wn_kid_num = WN_COPY_Tree(WN_kid0(wn_cur_node));
					   ikid_num = WN_const_val(wn_kid_num);
					   wn_tmp = wn_next_node;
					   ACC_DREGION__ENTRY entry;
					   entry.acc_data_clauses = wn_cur_node;	
					   
					   while(ikid_num)
					   {					   
						   acc_dstart_node = WN_COPY_Tree(WN_kid0(wn_tmp));
						   wn_tmp = WN_next(wn_tmp);
						   acc_dlength_node = WN_COPY_Tree(WN_kid0(wn_tmp));
						   wn_tmp = WN_next(wn_tmp);
						   entry.acc_data_start.push_back(acc_dstart_node);
						   entry.acc_data_length.push_back(acc_dlength_node);						   
						   ikid_num --;
					   }
					   acc_dregion_device.push_back(entry);
					   WN_next(wn_cur_node) = acc_device_nodes;
					   acc_device_nodes = wn_cur_node;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_CREATE:
				 case WN_PRAGMA_ACC_CLAUSE_PRESENT_OR_CREATE:
				   for (wn = acc_present_or_create_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   break;
				   
				   if (wn == NULL) 
				   {				   	 			   
					   wn_kid_num = WN_COPY_Tree(WN_kid0(wn_cur_node));
					   ikid_num = WN_const_val(wn_kid_num);
					   wn_tmp = wn_next_node;
					   ACC_DREGION__ENTRY entry;
					   entry.acc_data_clauses = wn_cur_node;
						ST* st_name = WN_st(wn_cur_node);
						if(TY_kind(ST_type(st_name)) == KIND_SCALAR)
						{
							entry.acc_scalar_type = ACC_SCALAR_VAR_CREATE;
						}
					   
					   while(ikid_num)
					   {					   
						   acc_dstart_node = WN_COPY_Tree(WN_kid0(wn_tmp));
						   wn_tmp = WN_next(wn_tmp);
						   acc_dlength_node = WN_COPY_Tree(WN_kid0(wn_tmp));
						   wn_tmp = WN_next(wn_tmp);
						   entry.acc_data_start.push_back(acc_dstart_node);
						   entry.acc_data_length.push_back(acc_dlength_node);						   
						   ikid_num --;
					   }
					   acc_dregion_pcreate.push_back(entry);
					   //////////////////////////////////////
					   WN_next(wn_cur_node) = acc_present_or_create_nodes;
					   acc_present_or_create_nodes = wn_cur_node;
					 //++acc_present_or_create_count;
				   } 
				   else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_DEVICEPTR:
				   for (wn = acc_deviceptr_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   break;
				   if (wn == NULL) {
					 WN_next(wn_cur_node) = acc_deviceptr_nodes;
					 acc_deviceptr_nodes = wn_cur_node;
					 //++acc_deviceptr_count;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_PRIVATE:
				   for (wn = acc_private_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   break;
				   
				   if (wn == NULL) {
				   	 ACC_DREGION__ENTRY entry;
					 entry.acc_data_clauses = wn_cur_node;
					 acc_dregion_private.push_back(entry);
					 //////////////////////////////////////
					 WN_next(wn_cur_node) = acc_private_nodes;
					 acc_private_nodes = wn_cur_node;
					 //++acc_private_count;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_FIRST_PRIVATE:
				   for (wn = acc_firstprivate_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   break;
				   
				   if (wn == NULL) {
				   	 ACC_DREGION__ENTRY entry;
					 entry.acc_data_clauses = wn_cur_node;
					 acc_dregion_fprivate.push_back(entry);
					 //////////////////////////////////////
					 WN_next(wn_cur_node) = acc_firstprivate_nodes;
					 acc_firstprivate_nodes = wn_cur_node;
					 //++acc_firstprivate_count;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_LAST_PRIVATE:
				   for (wn = acc_lastprivate_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   break;
				   
				   if (wn == NULL) {
				   	 ACC_DREGION__ENTRY entry;
					 entry.acc_data_clauses = wn_cur_node;
					 acc_dregion_lprivate.push_back(entry);
					 //////////////////////////////////////
					 WN_next(wn_cur_node) = acc_lastprivate_nodes;
					 acc_lastprivate_nodes = wn_cur_node;
					 //++acc_firstprivate_count;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_INOUT_SCALAR:
				   for (wn = acc_inout_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   break;
				   
				   if (wn == NULL) {
				   	 ACC_DREGION__ENTRY entry;
					 entry.acc_data_clauses = wn_cur_node;
					 acc_dregion_inout_scalar.push_back(entry);
					 //////////////////////////////////////
					 WN_next(wn_cur_node) = acc_inout_nodes;
					 acc_inout_nodes = wn_cur_node;
					 //++acc_firstprivate_count;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_NUM_GANGS:
				   wn = acc_num_gangs_node; //only one stmt allowed.
				   if (wn == NULL) {
					 WN_next(wn_cur_node) = acc_num_gangs_node;
					 acc_num_gangs_node = wn_cur_node;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_NUM_WORKERS:
				   wn = acc_num_workers_node; //only one stmt allowed.
				   if (wn == NULL) {
					 WN_next(wn_cur_node) = acc_num_workers_node;
					 acc_num_workers_node = wn_cur_node;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_VECTOR_LENGTH:
				   wn = acc_vector_length_node; //only one stmt allowed.
				   if (wn == NULL) {
					 WN_next(wn_cur_node) = acc_vector_length_node;
					 acc_vector_length_node = wn_cur_node;
				   } else
					 WN_Delete ( wn_cur_node );
				   break;

				 case WN_PRAGMA_ACC_CLAUSE_WAIT:	
				   {
					 WN_next(wn_cur_node) = acc_wait_nodes;
					 acc_wait_nodes = wn_cur_node;
					 acc_wait_list.push_back(wn_cur_node);
				   } 
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_PARM:
				   for (wn = acc_parms_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
				   break;
				   if (wn == NULL) 
				   {
					 WN_next(wn_cur_node) = acc_parms_nodes;
					 acc_parms_nodes = wn_cur_node;
					 //++acc_parms_count;
				   } 
				   else
					 WN_Delete ( wn_cur_node );
				   break;
			
				 /*case WN_PRAGMA_NUMTHREADS:
				   if (numthreads_node)
					 WN_DELETE_Tree ( numthreads_node );
				   numthreads_node = cur_node;
				   break;*/
				 case WN_PRAGMA_ACC_CLAUSE_ASYNC:
				   acc_async_nodes = wn_cur_node;
				   break;
				   
				 case WN_PRAGMA_ACC_CLAUSE_INTEXP:
				   //This is used in wait directive				
				   for (wn = acc_wait_nodes; wn; wn = WN_next(wn))
					 if (ACC_Identical_Pragmas(wn_cur_node, wn))
					 	
				   if (wn == NULL) 
				   {
					 WN_next(wn_cur_node) = acc_wait_nodes;
					 acc_wait_nodes = wn_cur_node;
					 acc_wait_list.push_back(wn_cur_node);
				   } 
				   else
					 WN_Delete ( wn_cur_node );
					break;
					
				 case WN_PRAGMA_ACC_CLAUSE_CONST:
				 {
					ST* st = WN_st(wn_cur_node);
					if(TY_kind(ST_type(st)) == KIND_SCALAR)
						acc_const_offload_scalar[st] = TRUE;
					else
						acc_const_offload_ptr[st] = TRUE;
				 }
					break;
			
				 default:
					Fail_FmtAssertion ("out of context pragma (%s) in ACC {top-level pragma} processing",
										WN_pragmas[WN_pragma(wn_cur_node)].name);
			
			   }	
	  }	
	 } 	
	}
}

/**********************************************************************************/
//Generate GPU kernels parameters from acc kernels/parallel block
//isKernelRegion, if it is acc kernels region, should be set as TRUE.
// 				if it is the acc parallel region, should be set as FALSE.
/**********************************************************************************/

static WN* ACC_Generate_KernelParameters(WN* paramlist, void* pRegionInfo, BOOL isKernelRegion)
{
	ParallelRegionInfo* pParallelRegionInfo;
	KernelsRegionInfo* pKernelsRegionInfo;
	WN* wn;
	//Process scalar in/out variables first.

	if(acc_target_arch == ACC_ARCH_TYPE_APU)
	{
		for(wn=paramlist; wn; wn=WN_next(wn))
		{
			ST* st_param = WN_st(wn);
			KernelParameter kparam;
			kparam.st_host = st_param;
			kparam.st_device = NULL;
			acc_kernelLaunchParamList.push_back(kparam);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////
	else if(isKernelRegion)
	{
		pKernelsRegionInfo = (KernelsRegionInfo*)pRegionInfo;
		//Get a pair list for kernel parameters.
		for(wn=paramlist; wn; wn=WN_next(wn))
		{
			ST* st_param = WN_st(wn);
			int i = 0;
			BOOL isFound = FALSE;
			int j = 0;
				
			TY_IDX ty = ST_type(st_param);
			TY_KIND kind = TY_kind(ty);//ST_name(old_st)
			if(kind == KIND_SCALAR)
			{
				KernelParameter kparam;
				isFound = TRUE;
				if(acc_offload_scalar_management_tab.find(st_param) == acc_offload_scalar_management_tab.end())
				{
					Fail_FmtAssertion("ACC_Generate_KernelParameters: illegal parameters, kernels param:%s undefined in scalar acc pragma(in/out/private).",
									ST_name(st_param));
				}
				kparam.st_host = st_param;
				ACC_SCALAR_VAR_INFO* pInfo = acc_offload_scalar_management_tab[st_param];
				if(pInfo->acc_scalar_type==ACC_SCALAR_VAR_IN)
					kparam.st_device = NULL;
				//private never appears in parameters' list
				else //if(pInfo->acc_scalar_type==ACC_SCALAR_VAR_INOUT || pInfo->acc_scalar_type==ACC_SCALAR_VAR_OUT) 
					kparam.st_device = pInfo->st_device_in_host;
				
				acc_kernelLaunchParamList.push_back(kparam);
				continue;
			}
			//if it is scalar var, skip it.
			//let's traverse all the tables
			while(j < acc_nested_dregion_info.Depth)
			{
				//acc_nested_dregion_info.DRegionInfo[j];
				//
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->hostName == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						kparam.st_host = acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->hostName;
						kparam.st_device = acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->deviceName;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyinMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->hostName == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						kparam.st_host = acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->hostName;
						kparam.st_device = acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->deviceName;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->hostName == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						kparam.st_host = acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->hostName;
						kparam.st_device = acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->deviceName;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcreateMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->hostName == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						kparam.st_host = acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->hostName;
						kparam.st_device = acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->deviceName;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].presentMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->hostName == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						kparam.st_host = acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->hostName;
						kparam.st_device = acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->deviceName;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].dptrList.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].dptrList[i] == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						//if device ptr, the st_host = st_device
						kparam.st_host = st_param;
						kparam.st_device = st_param;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				
				j++;
			}
			if(isFound == TRUE)
				continue;

			//Kernels Table
			i=0;
			while(i < pKernelsRegionInfo->pcopyMap.size())
			{
				if(pKernelsRegionInfo->pcopyMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = pKernelsRegionInfo->pcopyMap[i]->hostName;
					kparam.st_device = pKernelsRegionInfo->pcopyMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pKernelsRegionInfo->pcopyinMap.size())
			{
				if(pKernelsRegionInfo->pcopyinMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = pKernelsRegionInfo->pcopyinMap[i]->hostName;
					kparam.st_device = pKernelsRegionInfo->pcopyinMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pKernelsRegionInfo->pcopyoutMap.size())
			{
				if(pKernelsRegionInfo->pcopyoutMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = pKernelsRegionInfo->pcopyoutMap[i]->hostName;
					kparam.st_device = pKernelsRegionInfo->pcopyoutMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pKernelsRegionInfo->pcreateMap.size())
			{
				if(pKernelsRegionInfo->pcreateMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = pKernelsRegionInfo->pcreateMap[i]->hostName;
					kparam.st_device = pKernelsRegionInfo->pcreateMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pKernelsRegionInfo->presentMap.size())
			{
				if(pKernelsRegionInfo->presentMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = pKernelsRegionInfo->presentMap[i]->hostName;
					kparam.st_device = pKernelsRegionInfo->presentMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			
			i = 0;
			while(i < pKernelsRegionInfo->dptrList.size())
			{
				if(pKernelsRegionInfo->dptrList[i] == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					//if device ptr, the st_host = st_device
					kparam.st_host = st_param;
					kparam.st_device = st_param;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			//find in unspecified region
			i = 0;
			while(i < acc_unspecified_pcopyMap.size())
			{
				if(acc_unspecified_pcopyMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = acc_unspecified_pcopyMap[i]->hostName;
					kparam.st_device = acc_unspecified_pcopyMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			if(isFound == FALSE)
			  Fail_FmtAssertion("ACC_Generate_KernelParameters: illegal parameters, kernels param:%s undefined in previous acc pragma.",
									ST_name(st_param));
		}
	}
	else
	{
		pParallelRegionInfo = (ParallelRegionInfo*)pRegionInfo;
		//Get a pair list for kernel parameters.
		for(wn=paramlist; wn; wn=WN_next(wn))
		{
			ST* st_param = WN_st(wn);
			int i = 0;
			BOOL isFound = FALSE;
			int j = 0;
				
			TY_IDX ty = ST_type(st_param);
			TY_KIND kind = TY_kind(ty);//ST_name(old_st)
			if(kind == KIND_SCALAR)
			{
				KernelParameter kparam;
				isFound = TRUE;
				kparam.st_host = st_param;
				if(acc_offload_scalar_management_tab.find(st_param) == acc_offload_scalar_management_tab.end())
				{
					Fail_FmtAssertion("ACC_Generate_KernelParameters: illegal parameters, kernels param:%s undefined in scalar acc pragma(in/out/private).",
									ST_name(st_param));
				}
				kparam.st_host = st_param;
				ACC_SCALAR_VAR_INFO* pInfo = acc_offload_scalar_management_tab[st_param];
				if(pInfo->acc_scalar_type==ACC_SCALAR_VAR_IN)
					kparam.st_device = NULL;
				//private never appears in parameters' list
				else //if(pInfo->acc_scalar_type==ACC_SCALAR_VAR_INOUT || pInfo->acc_scalar_type==ACC_SCALAR_VAR_OUT) 
					kparam.st_device = pInfo->st_device_in_host;
				
				acc_kernelLaunchParamList.push_back(kparam);
				continue;
			}
			
			//let's traverse all the tables
			while(j < acc_nested_dregion_info.Depth)
			{
				//acc_nested_dregion_info.DRegionInfo[j];
				//
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->hostName == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						kparam.st_host = acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->hostName;
						kparam.st_device = acc_nested_dregion_info.DRegionInfo[j].pcopyMap[i]->deviceName;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyinMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->hostName == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						kparam.st_host = acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->hostName;
						kparam.st_device = acc_nested_dregion_info.DRegionInfo[j].pcopyinMap[i]->deviceName;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->hostName == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						kparam.st_host = acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->hostName;
						kparam.st_device = acc_nested_dregion_info.DRegionInfo[j].pcopyoutMap[i]->deviceName;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].pcreateMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->hostName == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						kparam.st_host = acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->hostName;
						kparam.st_device = acc_nested_dregion_info.DRegionInfo[j].pcreateMap[i]->deviceName;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].presentMap.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->hostName == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						kparam.st_host = acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->hostName;
						kparam.st_device = acc_nested_dregion_info.DRegionInfo[j].presentMap[i]->deviceName;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
				
				i = 0;
				while(i < acc_nested_dregion_info.DRegionInfo[j].dptrList.size())
				{
					if(acc_nested_dregion_info.DRegionInfo[j].dptrList[i] == st_param)
					{
						KernelParameter kparam;
						isFound = TRUE;
						//if device ptr, the st_host = st_device
						kparam.st_host = st_param;
						kparam.st_device = st_param;
						acc_kernelLaunchParamList.push_back(kparam);
						break;
					}
					i ++;
				}
				
				if(isFound == TRUE)
					break;
			
				
				j++;
			}
			if(isFound == TRUE)
				continue;

			//parallel Table
			i=0;
			while(i < pParallelRegionInfo->pcopyMap.size())
			{
				if(pParallelRegionInfo->pcopyMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = pParallelRegionInfo->pcopyMap[i]->hostName;
					kparam.st_device = pParallelRegionInfo->pcopyMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->pcopyinMap.size())
			{
				if(pParallelRegionInfo->pcopyinMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = pParallelRegionInfo->pcopyinMap[i]->hostName;
					kparam.st_device = pParallelRegionInfo->pcopyinMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->pcopyoutMap.size())
			{
				if(pParallelRegionInfo->pcopyoutMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = pParallelRegionInfo->pcopyoutMap[i]->hostName;
					kparam.st_device = pParallelRegionInfo->pcopyoutMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->pcreateMap.size())
			{
				if(pParallelRegionInfo->pcreateMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = pParallelRegionInfo->pcreateMap[i]->hostName;
					kparam.st_device = pParallelRegionInfo->pcreateMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->presentMap.size())
			{
				if(pParallelRegionInfo->presentMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = pParallelRegionInfo->presentMap[i]->hostName;
					kparam.st_device = pParallelRegionInfo->presentMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			
				
			if(isFound == TRUE)
				continue;
			i = 0;
			while(i < pParallelRegionInfo->dptrList.size())
			{
				if(pParallelRegionInfo->dptrList[i] == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					//if device ptr, the st_host = st_device
					kparam.st_host = st_param;
					kparam.st_device = st_param;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}			
			
			if(isFound == TRUE)
				continue;
			//find in unspecified region
			i = 0;
			while(i < acc_unspecified_pcopyMap.size())
			{
				if(acc_unspecified_pcopyMap[i]->hostName == st_param)
				{
					KernelParameter kparam;
					isFound = TRUE;
					kparam.st_host = acc_unspecified_pcopyMap[i]->hostName;
					kparam.st_device = acc_unspecified_pcopyMap[i]->deviceName;
					acc_kernelLaunchParamList.push_back(kparam);
					break;
				}
				i ++;
			}
			
			if(isFound == FALSE)
			  Fail_FmtAssertion("ACC_Generate_KernelParameters: illegal parameters, parallel param:%s undefined in previous acc pragma",
									ST_name(st_param));
		}
	}
}

//acc_map_scalar_inout
//acc_map_scalar_out
/*Create single scalar VAR for device memory from host side*/
static ST* ACC_Create_Single_Scalar_Variable(ST* st_var, ACC_SCALAR_VAR_INFO* pInfo)
{	
		//ST* st_var = acc_scalar_inout_nodes[i];
		TY_IDX ty_var = ST_type(st_var);
		UINT32 ty_size = TY_size(ty_var);
	    char* localname = (char *) alloca(strlen(ST_name(st_var))+35);
		sprintf ( localname, "__device_%s_%d", ST_name(st_var), acc_reg_tmp_count);
		acc_reg_tmp_count ++;
		
		TY_IDX ty_p = Make_Pointer_Type(ty_var);
		ST * karg = NULL;
		karg = New_ST( CURRENT_SYMTAB );
		ST_Init(karg,
				Save_Str( localname ),
				CLASS_VAR,
				SCLASS_FSTATIC,
				EXPORT_LOCAL,
				ty_p);
		//ACC_SCALAR_INOUT_INFO* pSTMap = new ACC_SCALAR_INOUT_INFO;
		//pInfo->st_host = st_var;
		pInfo->st_device_in_host = karg;
		//pInfo-> = karg;
}

/****************************************************************************/
/******************Scalar Variable Create and Generate Copyin**********************/
/****************************************************************************/
static void ACC_Scalar_Variable_CreateAndCopyInOut(WN* wn_replace_block)
{
	map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor_offload_info = acc_offload_scalar_management_tab.begin();
	for(; itor_offload_info!=acc_offload_scalar_management_tab.end(); itor_offload_info++)
	{
		ACC_SCALAR_VAR_INFO* pInfo = itor_offload_info->second;
		//skip the created data. Actually all of them should be taken care early
		if(pInfo->bcreate_by_previous == TRUE)
			continue;
		///////////////////////////////////////////////////////
		if(pInfo->acc_scalar_type == ACC_SCALAR_VAR_INOUT)
		{
			WN* wn_start, *wn_size;
			ST* st_host = pInfo->st_var;
			TY_IDX ty_host = ST_type(st_host);
			UINT32 ty_size = TY_size(ty_host);
			wn_start = WN_Intconst(MTYPE_U4, 0);
			wn_size = WN_Intconst(MTYPE_U4, ty_size);
			//ACC_SCALAR_INOUT_INFO* pSTMap = new ACC_SCALAR_INOUT_INFO;
			ACC_Create_Single_Scalar_Variable(st_host, pInfo);
			pInfo->isize = ty_size;
			//acc_map_scalar_inout[st_host] = pSTMap;
			//WN* wnx = WN_Ldid(Pointer_type, 0, pInfo->st_device_in_host, ST_type(pInfo->st_device_in_host));
			ACC_DREGION__ENTRY dentry;
			dentry.acc_data_clauses = WN_Ldid(TY_mtype(ty_host), 0, st_host, ty_host);
			WN* wn_pcreate = ACC_GenIsPCreate(dentry, pInfo->st_device_in_host, ty_size);
			WN_INSERT_BlockLast( wn_replace_block, wn_pcreate );
			PREG_NUM rreg1, rreg2;	/* Pregs with I4 return values */;
			ST* st_is_pcreate_tmp;
		    ACC_GET_RETURN_PREGS(rreg1, rreg2, MTYPE_I4);	  
			ACC_Host_Create_Preg_or_Temp( MTYPE_I4, "_is_pcreate",
			                  &st_is_pcreate_tmp);
			WN* wn_return = WN_Ldid(TY_mtype(ST_type(st_is_pcreate_tmp)), rreg1, Return_Val_Preg, ST_type(st_is_pcreate_tmp));
      		WN* temp_node = WN_Stid(TY_mtype(ST_type(st_is_pcreate_tmp)), 0, 
	  								st_is_pcreate_tmp, ST_type(st_is_pcreate_tmp), wn_return);			
      		WN_INSERT_BlockLast( wn_replace_block, temp_node );
			temp_node = WN_Ldid(TY_mtype(ST_type(st_is_pcreate_tmp)), 0, st_is_pcreate_tmp, ST_type(st_is_pcreate_tmp));	  	
	  
			WN* wn_testif = WN_Relational (OPR_EQ, WN_rtype(temp_node), 
  								temp_node, WN_Intconst(WN_rtype(temp_node), 0));
			WN* wn_thenblock = WN_CreateBlock();
			WN* wn_malloc = Gen_DeviceMalloc(st_host, pInfo->st_device_in_host, WN_COPY_Tree(wn_size));				
			WN* wn_H2D = Gen_DataH2D(st_host, pInfo->st_device_in_host, wn_size, wn_start);
			WN* wn_pending_new_ptr = Gen_ACC_Pending_DevicePtr_To_Current_Stack(pInfo->st_device_in_host);
			WN_INSERT_BlockLast( wn_thenblock, wn_malloc);		
			WN_INSERT_BlockLast( wn_thenblock, wn_H2D);			
			WN_INSERT_BlockLast( wn_thenblock, wn_pending_new_ptr);		
			WN* wn_ifstmt = WN_CreateIf(wn_testif, wn_thenblock, WN_CreateBlock());
				
			WN_INSERT_BlockLast( wn_replace_block, wn_ifstmt);			
			//WN_INSERT_BlockLast(wn_replace_block, wn_H2D);
		}
		else if(pInfo->acc_scalar_type == ACC_SCALAR_VAR_OUT)
		{
			WN* wn_start, *wn_size;
			ST* st_host = pInfo->st_var;
			TY_IDX ty_host = ST_type(st_host);
			UINT32 ty_size = TY_size(ty_host);
			wn_start = WN_Intconst(MTYPE_U4, 0);
			wn_size = WN_Intconst(MTYPE_U4, ty_size);
			//ACC_SCALAR_INOUT_INFO* pSTMap = new ACC_SCALAR_INOUT_INFO;
			ACC_Create_Single_Scalar_Variable(st_host, pInfo);
			pInfo->isize = ty_size;
			//if(acc_map_scalar_inout[st_host])
			//	delete acc_map_scalar_inout[st_host];
			//acc_map_scalar_inout[st_host] = pSTMap;
			WN* wn_malloc = Gen_DeviceMalloc(st_host, pInfo->st_device_in_host, WN_COPY_Tree(wn_size));
			//WN* wnx = WN_Ldid(Pointer_type, 0, pInfo->st_device_in_host, ST_type(pInfo->st_device_in_host));
			ACC_DREGION__ENTRY dentry;
			dentry.acc_data_clauses = WN_Ldid(TY_mtype(ty_host), 0, st_host, ty_host);
			WN* wn_pcreate = ACC_GenIsPCreate(dentry, pInfo->st_device_in_host, ty_size);
			WN_INSERT_BlockLast( wn_replace_block, wn_pcreate );
			PREG_NUM rreg1, rreg2;	/* Pregs with I4 return values */;
			ST* st_is_pcreate_tmp;
		    ACC_GET_RETURN_PREGS(rreg1, rreg2, MTYPE_I4);	  
			ACC_Host_Create_Preg_or_Temp( MTYPE_I4, "_is_pcreate",
			                  &st_is_pcreate_tmp);
			WN* wn_return = WN_Ldid(TY_mtype(ST_type(st_is_pcreate_tmp)), rreg1, Return_Val_Preg, ST_type(st_is_pcreate_tmp));
      		WN* temp_node = WN_Stid(TY_mtype(ST_type(st_is_pcreate_tmp)), 0, 
	  								st_is_pcreate_tmp, ST_type(st_is_pcreate_tmp), wn_return);			
      		WN_INSERT_BlockLast( wn_replace_block, temp_node );
			temp_node = WN_Ldid(TY_mtype(ST_type(st_is_pcreate_tmp)), 0, st_is_pcreate_tmp, ST_type(st_is_pcreate_tmp));	  	
	  
			WN* wn_testif = WN_Relational (OPR_EQ, WN_rtype(temp_node), 
  								temp_node, WN_Intconst(WN_rtype(temp_node), 0));
			WN* wn_thenblock = WN_CreateBlock();
			WN_INSERT_BlockLast( wn_thenblock, wn_malloc);		
			WN* wn_ifstmt = WN_CreateIf(wn_testif, wn_thenblock, WN_CreateBlock());
				
			WN_INSERT_BlockLast( wn_replace_block, wn_ifstmt);	
		}
	}
	/*for(i=0; i<acc_scalar_out_nodes.size(); i++)
	{
		WN* wn_start, *wn_size;
		ST* st_host = acc_scalar_out_nodes[i];
		TY_IDX ty_host = ST_type(st_host);
		UINT32 ty_size = TY_size(ty_host);
		wn_start = WN_Intconst(MTYPE_U4, 0);
		wn_size = WN_Intconst(MTYPE_U4, ty_size);
		ACC_SCALAR_INOUT_INFO* pSTMap = new ACC_SCALAR_INOUT_INFO;
		ACC_Create_Single_Scalar_Variable(st_host, pSTMap);
		pSTMap->isize = ty_size;
		if(acc_map_scalar_inout[st_host])
			delete acc_map_scalar_inout[st_host];
		acc_map_scalar_inout[st_host] = pSTMap;
		WN* wn_H2D = Gen_DeviceMalloc(st_host, pSTMap->st_device_ptr_on_host, WN_COPY_Tree(wn_size));
		WN_INSERT_BlockLast(wn_replace_block, wn_H2D);
		//WN* wn_H2D = Gen_DataH2D(st_host, pSTMap->st_device_ptr_on_host, wn_size, wn_start);
		//WN_INSERT_BlockLast(wn_replace_block, wn_H2D);		
	}*/
}

/*****************************************************************************/
/*Handling scalar variable copyout*/
/*****************************************************************************/
static void ACC_Scalar_Variable_Copyout(WN* wn_replace_block)
{
	map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor_offload_info = acc_offload_scalar_management_tab.begin();
	for(; itor_offload_info!=acc_offload_scalar_management_tab.end(); itor_offload_info++)
	{
		ACC_SCALAR_VAR_INFO* pInfo = itor_offload_info->second;
		//skip the created data. Actually all of them should be taken care early
		if(pInfo->bcreate_by_previous == TRUE)
			continue;
		if(pInfo->acc_scalar_type == ACC_SCALAR_VAR_INOUT)
		{
			WN* wn_start;
			WN* wn_size;
			ST* st_host = pInfo->st_var;
			ST* st_device = pInfo->st_device_in_host;
			TY_IDX ty_host = ST_type(st_host);
			UINT32 ty_size = TY_size(ty_host);
			wn_start = WN_Intconst(MTYPE_U4, 0);
			wn_size = WN_Intconst(MTYPE_U4, ty_size);
			WN* wn_D2H = Gen_DataD2H(st_device, st_host, wn_size, wn_start);
			WN_INSERT_BlockLast(wn_replace_block, wn_D2H);
			//free buffer
			//wn_D2H = ACC_GenFreeDeviceMemory(st_device);
			//WN_INSERT_BlockLast(wn_replace_block, wn_D2H);
			delete pInfo;
		}
		else if(pInfo->acc_scalar_type == ACC_SCALAR_VAR_OUT)
		{
			WN* wn_start, *wn_size;
			ST* st_host = pInfo->st_var;
			ST* st_device = pInfo->st_device_in_host;
			TY_IDX ty_host = ST_type(st_host);
			UINT32 ty_size = TY_size(ty_host);
			wn_start = WN_Intconst(MTYPE_U4, 0);
			wn_size = WN_Intconst(MTYPE_U4, ty_size);
			WN* wn_D2H = Gen_DataD2H(st_device, st_host, wn_size, wn_start);
			WN_INSERT_BlockLast(wn_replace_block, wn_D2H);
			//free buffer
			//wn_D2H = ACC_GenFreeDeviceMemory(st_device);
			//WN_INSERT_BlockLast(wn_replace_block, wn_D2H);	
			delete pInfo;
		}
		else
			delete pInfo;
		itor_offload_info->second =NULL;
	}
	/*map<ST*, ACC_SCALAR_INOUT_INFO*>::iterator itor;// = acc_scalar_inout_nodes.begin();
	for(itor = acc_map_scalar_inout.begin(); itor!=acc_map_scalar_inout.end(); itor++)
	{
		WN* wn_start;
		WN* wn_size;
		ST* st_host = itor->second->st_host;
		ST* st_device = itor->second->st_device_ptr_on_host;
		TY_IDX ty_host = ST_type(st_host);
		UINT32 ty_size = TY_size(ty_host);
		wn_start = WN_Intconst(MTYPE_U4, 0);
		wn_size = WN_Intconst(MTYPE_U4, ty_size);
		WN* wn_D2H = Gen_DataD2H(st_device, st_host, wn_size, wn_start);
		WN_INSERT_BlockLast(wn_replace_block, wn_D2H);
		//free buffer
		wn_D2H = ACC_GenFreeDeviceMemory(st_device);
		WN_INSERT_BlockLast(wn_replace_block, wn_D2H);
		delete itor->second;
	}
	
	for(itor = acc_map_scalar_out.begin(); itor != acc_map_scalar_out.end(); itor++)
	{
		WN* wn_start, *wn_size;
		ST* st_host = itor->second->st_host;
		ST* st_device = itor->second->st_device_ptr_on_host;
		TY_IDX ty_host = ST_type(st_host);
		UINT32 ty_size = TY_size(ty_host);
		wn_start = WN_Intconst(MTYPE_U4, 0);
		wn_size = WN_Intconst(MTYPE_U4, ty_size);
		WN* wn_D2H = Gen_DataD2H(st_device, st_host, wn_size, wn_start);
		WN_INSERT_BlockLast(wn_replace_block, wn_D2H);
		//free buffer
		wn_D2H = ACC_GenFreeDeviceMemory(st_device);
		WN_INSERT_BlockLast(wn_replace_block, wn_D2H);	
		delete itor->second;
	}*/
}

//for the stupid test case
static WN* ACC_Find_Unspecified_Array()
{
	
		map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor = acc_offload_scalar_management_tab.begin();
		//Get a pair list for kernel parameters.
		for(; itor!=acc_offload_scalar_management_tab.end(); itor++)
		{
			ST* st_scalar = itor->first;
			ACC_SCALAR_VAR_INFO* pvar_info = itor->second;
			int i = 0;
			BOOL isFound = FALSE;
			int j = 0;
				
			TY_IDX ty = ST_type(st_scalar);
			TY_KIND kind = TY_kind(ty);//ST_name(old_st)
			//remove the previous defined array region
			if(kind == KIND_ARRAY && pvar_info->bcreate_by_previous==TRUE)
			{
				acc_offload_scalar_management_tab.erase(itor);
			}
			//not find in previous deinition, so treat in default as copy
			else if(kind == KIND_ARRAY && pvar_info->bcreate_by_previous==FALSE)	
			{
				ACC_DREGION__ENTRY dentry;
				dentry.acc_data_clauses = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_PRESENT_OR_COPY, st_scalar, 1);
				WN_kid0(dentry.acc_data_clauses) = WN_Intconst(MTYPE_I4, 1);

				dentry.acc_data_start.push_back(WN_Intconst(MTYPE_I4, 0));
				//get the length then.
				int idims = ARB_dimension(TY_arb(ty));
		
				idims = TY_AR_ndims(ty);
				WN* wn_lower_bound, *wn_upper_bound, *wn_stride;
				TY_IDX ty_element = ACC_Get_ElementTYForMultiArray(st_scalar);
				
				if(ARB_const_stride(TY_arb(ty)))
				{
					int istride = ARB_stride_val(TY_arb(ty));
					wn_stride = WN_Intconst(MTYPE_I4, istride);
				}
				else
				{
					ST_IDX st_stride = ARB_stride_var(TY_arb(ty));
					wn_stride = WN_Ldid(TY_mtype(ST_type(st_stride)), 0, st_stride, ST_type(st_stride));
				}
				wn_stride = WN_Binary(OPR_DIV, WN_rtype(wn_stride),
			                                      wn_stride, WN_Intconst(MTYPE_I4, TY_size(ty_element)));
				//wn_stride = wn_stride
				/////////////////////////////////////////////////////////////////////////////////////////////////////////
				if(ARB_const_lbnd(TY_arb(ty)))
				{
					int ilower_bound = ARB_lbnd_val(TY_arb(ty));
					wn_lower_bound = WN_Intconst(MTYPE_I4, ilower_bound);
				}
				else //var lower bound
				{
					ST_IDX st_lower_bound = ARB_lbnd_var(TY_arb(ty));
					wn_lower_bound = WN_Ldid(TY_mtype(ST_type(st_lower_bound)), 0, st_lower_bound, ST_type(st_lower_bound));
				}
				
				if(ARB_const_ubnd(TY_arb(ty)))
				{
					int iupper_bound = ARB_ubnd_val(TY_arb(ty));
					wn_upper_bound = WN_Intconst(MTYPE_I4, iupper_bound);
				}
				else //var upper bound
				{
					ST_IDX st_upper_bound = ARB_ubnd_var(TY_arb(ty));
					wn_upper_bound = WN_Ldid(TY_mtype(ST_type(st_upper_bound)), 0, st_upper_bound, ST_type(st_upper_bound));
				}	
				//subtract the offset
				wn_upper_bound = WN_Binary(OPR_SUB, WN_rtype(wn_upper_bound), wn_upper_bound, wn_lower_bound);
				wn_upper_bound = WN_Binary(OPR_ADD, WN_rtype(wn_upper_bound), wn_upper_bound, WN_Intconst(MTYPE_I4, 1));
				
				wn_upper_bound = WN_Binary(OPR_MPY, WN_rtype(wn_stride), wn_stride, wn_upper_bound);
				dentry.acc_data_length.push_back(wn_upper_bound);
		
				acc_unspecified_dregion_pcopy.push_back(dentry);
				//remove it from here, now acc_offload_scalar_management_tab only holds scalar vars
				acc_offload_scalar_management_tab.erase(itor);
			}
		}
}


WN* ACC_Process_KernelsRegion( WN * tree, WN* wn_cont)
{
	KernelsRegionInfo kernelsRegionInfo;
	WN* wn;
	WN* wn_if_test = NULL;
	WN* wn_else_block = NULL;
	WN* wn_if_stmt = NULL;
	WN* kernelsBlock = WN_CreateBlock();
	acc_reduction_count = 0;
	//ignore if, present, deviceptr clauses
	WN* wn_acc_stack_op = Gen_ACC_Stack_Push();
	if(acc_if_node)
    {
		WN* wn_origial_block = WN_COPY_Tree(tree);
		wn_if_test = WN_COPY_Tree(acc_if_node);
		wn_else_block = ACC_Remove_All_ACC_Pragma(wn_origial_block);
    }
	WN_INSERT_BlockLast(kernelsBlock, wn_acc_stack_op);
	kernelsRegionInfo.acc_if_node = acc_if_node;
	//sDRegionInfo.acc_copy_nodes = acc_copy_nodes; 	  /* Points to (optional) copy nodes */
	//sDRegionInfo.acc_copyin_nodes = acc_copyin_nodes;   /* Points to (optional) copyin nodes */
	//sDRegionInfo.acc_copyout_nodes = acc_copyout_nodes; /* Points to (optional) copyout nodes */	  
	//sDRegionInfo.acc_create_nodes = acc_create_nodes;
	kernelsRegionInfo.acc_present_nodes = acc_present_nodes;
	kernelsRegionInfo.acc_present_or_copy_nodes = acc_present_or_copy_nodes;
	kernelsRegionInfo.acc_present_or_copyin_nodes = acc_present_or_copyin_nodes;
	kernelsRegionInfo.acc_present_or_copyout_nodes = acc_present_or_copyout_nodes;
	kernelsRegionInfo.acc_present_or_create_nodes = acc_present_or_create_nodes;
	kernelsRegionInfo.acc_deviceptr_nodes = acc_deviceptr_nodes;
	kernelsRegionInfo.acc_async = acc_async_nodes;
	kernelsRegionInfo.acc_param = acc_parms_nodes;

	kernelsRegionInfo.acc_dregion_pcreate = acc_dregion_pcreate;		
	kernelsRegionInfo.acc_dregion_pcopy = acc_dregion_pcopy;		
	kernelsRegionInfo.acc_dregion_pcopyin = acc_dregion_pcopyin;		
	kernelsRegionInfo.acc_dregion_pcopyout = acc_dregion_pcopyout;		
	kernelsRegionInfo.acc_dregion_present = acc_dregion_present;
	kernelsRegionInfo.acc_dregion_private = acc_dregion_private;		
	kernelsRegionInfo.acc_dregion_fprivate = acc_dregion_fprivate;
	kernelsRegionInfo.acc_dregion_lprivate = acc_dregion_lprivate;
	kernelsRegionInfo.acc_dregion_inout_scalar = acc_dregion_inout_scalar;
	
	if(acc_wait_nodes)
	{
		ACC_GenWaitNodes(&acc_wait_list, kernelsBlock);
		acc_wait_nodes = NULL;
		acc_wait_list.clear();
	}
	//async expr
	if(acc_async_nodes)
	{		
		acc_AsyncExpr = WN_COPY_Tree(WN_kid0(acc_async_nodes));
		WN_Delete(acc_async_nodes); 
	}
	//declare the data	
	if(acc_present_or_copy_nodes)
	{
		//declaration, cuda malloc 
		ACC_GenDeviceCreateCopyInOut(&kernelsRegionInfo.acc_dregion_pcopy, 
  										&kernelsRegionInfo.pcopyMap, 
  										kernelsBlock, TRUE);
	}
	if(acc_present_or_copyin_nodes)
	{
		//declaration, cuda malloc 
		ACC_GenDeviceCreateCopyInOut(&kernelsRegionInfo.acc_dregion_pcopyin, 
  										&kernelsRegionInfo.pcopyinMap, 
  										kernelsBlock, TRUE);
	}
	if(acc_present_or_copyout_nodes)
	{
		//Only declaration and cuda malloc, no data movement
		ACC_GenDeviceCreateCopyInOut(&kernelsRegionInfo.acc_dregion_pcopyout, 
  										&kernelsRegionInfo.pcopyoutMap, 
  										kernelsBlock, FALSE);
	}
	if(acc_present_or_create_nodes)
	{
		//Only declaration and cuda malloc, no data movement
		ACC_GenDeviceCreateCopyInOut(&kernelsRegionInfo.acc_dregion_pcreate, 
  										&kernelsRegionInfo.pcreateMap, 
  										kernelsBlock, FALSE);
	}
	if(acc_present_nodes)
	{
		//Only declaration and cuda malloc, no data movement
		ACC_GenPresentNode(&kernelsRegionInfo.acc_dregion_present, 
  										&kernelsRegionInfo.presentMap, 
  										kernelsBlock);
	}
	if(acc_deviceptr_nodes)
	{
		//Only declaration and cuda malloc, no data movement
		ACC_GenDevicePtr(acc_deviceptr_nodes, 
  										&kernelsRegionInfo.dptrList);
	}
	
	//process scalar variables
	ACC_Process_scalar_variable_for_offload_region(tree, TRUE);
	//Let's process scalar variable
	//finalize the scalar data property(IN/OUT/INOUT/PRIVATE/CREATE/PRESENT)
	ACC_Find_Scalar_Var_Inclose_Data_Clause((void*)&kernelsRegionInfo, TRUE);
	//find the agregate data that is not specified in the kernels/enclosed data region
	//they are treated as pcopy
	ACC_Find_Unspecified_Array();
	ACC_GenDeviceCreateCopyInOut(&acc_unspecified_dregion_pcopy, 
										&acc_unspecified_pcopyMap, 
										kernelsBlock, TRUE);
	if(acc_dfa_enabled == FALSE)
	{
		//finalize the parameters for device computation kernel function
		Finalize_Kernel_Parameters();
	}
	
	//reset them
	acc_if_node = NULL;
	acc_copy_nodes = NULL;	  /* Points to (optional) copy nodes */
	acc_copyin_nodes = NULL;	  /* Points to (optional) copyin nodes */
	acc_copyout_nodes = NULL;   /* Points to (optional) copyout nodes */	  
	acc_create_nodes = NULL;
	acc_present_nodes = NULL;
	acc_present_or_copy_nodes = NULL;
	acc_present_or_copyin_nodes = NULL;
	acc_present_or_copyout_nodes = NULL;
	acc_present_or_create_nodes = NULL;
	acc_deviceptr_nodes = NULL;
	acc_async_nodes = NULL;
	acc_private_nodes =NULL;
	acc_firstprivate_nodes =NULL;
	acc_lastprivate_nodes =NULL;
	acc_inout_nodes =NULL;
			  
	acc_dregion_pcreate.clear();
	acc_dregion_pcopy.clear();
	acc_dregion_pcopyin.clear();
	acc_dregion_pcopyout.clear();
	acc_dregion_present.clear();
	acc_dregion_host.clear();
	acc_dregion_device.clear();
	acc_dregion_private.clear();
	acc_dregion_fprivate.clear();
	acc_dregion_lprivate.clear();
	acc_dregion_inout_scalar.clear();
	acc_local_new_var_map.clear();
	//Handling the data analysis	
	
	/******************************************************************/
	//Generate scalar value in/out processing, from liveness analysis results
	ACC_Scalar_Variable_CreateAndCopyInOut(kernelsBlock);
	/******************************************************************/
	//Generate parameters
	acc_kernelLaunchParamList.clear();	
	acc_additionalKernelLaunchParamList.clear();
	ACC_Generate_KernelParameters(acc_parms_nodes, (void*)&kernelsRegionInfo, TRUE);
    //multi kernels and multi loopnest process
    Transform_ACC_Kernel_Block_New(tree, kernelsBlock);
    //ACC_Handle_Kernels_Loops(tree, &kernelsRegionInfo, kernelsBlock);
	acc_kernel_functions_st.clear();
	//Generate the kernel launch function.
	//WN_INSERT_BlockLast(acc_replace_block, LauchKernel(0));	
	//Generate the data copy back function
	if(kernelsRegionInfo.acc_present_or_copyout_nodes)
		ACC_GenDataCopyOut(&kernelsRegionInfo.pcopyoutMap, kernelsBlock);
	if(kernelsRegionInfo.acc_present_or_copy_nodes)
		ACC_GenDataCopyOut(&kernelsRegionInfo.pcopyMap, kernelsBlock);
	
	if(acc_AsyncExpr)
		WN_Delete(acc_AsyncExpr);
	acc_AsyncExpr = NULL;

	/****************************************************************************/
	/*Copyout the data and clear the vectors and map buffer*/
	ACC_Scalar_Variable_Copyout(kernelsBlock);
	acc_offload_scalar_management_tab.clear();	
	acc_offload_params_management_tab.clear();
	acc_unspecified_dregion_pcopy.clear();
	acc_unspecified_pcopyMap.clear();
	acc_const_offload_ptr.clear();	
	acc_const_offload_scalar.clear();
	
	while(acc_parms_nodes)
	{
		WN* next = WN_next(acc_parms_nodes);
		WN_Delete(acc_parms_nodes);
		acc_parms_nodes = next;
	}
	acc_parms_nodes = NULL;
	//acc_scalar_inout_nodes.clear();
	//acc_scalar_out_nodes.clear();
	//acc_map_scalar_inout.clear();
	//acc_map_scalar_out.clear();
	/****************************************************************************/
	//Free device memory
	/*if(kernelsRegionInfo.acc_present_or_copyin_nodes)
		ACC_GenDeviceMemFreeInBatch(&kernelsRegionInfo.pcopyinMap, kernelsBlock);
	if(kernelsRegionInfo.acc_present_or_copyout_nodes)
		ACC_GenDeviceMemFreeInBatch(&kernelsRegionInfo.pcopyoutMap, kernelsBlock);
	if(kernelsRegionInfo.acc_present_or_copy_nodes)
		ACC_GenDeviceMemFreeInBatch(&kernelsRegionInfo.pcopyMap, kernelsBlock);
	if(kernelsRegionInfo.acc_present_or_create_nodes)
		ACC_GenDeviceMemFreeInBatch(&kernelsRegionInfo.pcreateMap, kernelsBlock);*/
	wn_acc_stack_op = Gen_ACC_Free_Device_Ptr_InCurrentStack();
	WN_INSERT_BlockLast(kernelsBlock, wn_acc_stack_op);
        wn_acc_stack_op = Gen_ACC_Stack_Pop();
	WN_INSERT_BlockLast(kernelsBlock, wn_acc_stack_op);

	
  if(wn_if_test)
  {
  	wn_if_stmt = WN_CreateIf(wn_if_test, kernelsBlock, wn_else_block);
	WN* wn_return_block = WN_CreateBlock();
	WN_INSERT_BlockLast(wn_return_block, wn_if_stmt);
	kernelsBlock = wn_return_block;
  }
	return kernelsBlock;
}

static WN* ACC_Process_Block_Transform(WN * tree )
{
  WN* node = tree;
  ACCP_process_type acc_process_type;
  
  WN* wn_next_node; // = WN_next(wtmp);
  WN* wn_cont_nodes;// = WN_next(node);
  WN* wn_stmt_block;// = WN_region_body(node);
  WN* cur_node, *next_node, *prev_node;
  WN* replacement_block = WN_CreateBlock();
  
  for (cur_node = WN_first(tree); cur_node; cur_node = next_node)  
  {
    prev_node = WN_prev(cur_node);
    next_node = WN_next(cur_node);
	if ((WN_opcode(cur_node) == OPC_REGION) &&
			 WN_first(WN_region_pragmas(cur_node)) &&
			 ((WN_opcode(WN_first(WN_region_pragmas(cur_node))) == OPC_PRAGMA) ||
			  (WN_opcode(WN_first(WN_region_pragmas(cur_node))) == OPC_XPRAGMA)))
	{
	    WN *wtmp = WN_first(WN_region_pragmas(cur_node));
	    WN_PRAGMA_ID wid = (WN_PRAGMA_ID) WN_pragma(wtmp);
		WN* subBlock;

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

	    default:
	      printf("pragma value = %d", (int)wid); 
	      Fail_FmtAssertion (
			 "out of context pragma (%s) in acc {primary pragma} processing",
			 WN_pragmas[wid].name);
	    }
		
	    wn_next_node = WN_next(wtmp);
	    wn_cont_nodes = WN_next(cur_node);
	    wn_stmt_block = WN_region_body(cur_node);
		if (acc_process_type == ACCP_WAIT_REGION
			|| acc_process_type == ACCP_UPDATE_REGION
			|| acc_process_type == ACCP_DECLARE_REGION
			|| acc_process_type == ACCP_CACHE_REGION) 
	    {
	      WN_Delete ( wtmp );
	      WN_Delete ( WN_region_pragmas(cur_node) );
	      WN_DELETE_Tree ( WN_region_exits(cur_node) );
	    }
		
		///////////////////////////////////////////
		//process clauses
		ACC_Process_Clause_Pragma(wn_next_node);
		//Begin to process the body
		if (acc_process_type == ACCP_DATA_REGION)
		{
			  //it will include any other constructs.
			  //Get the information and move to the next stage
			  SingleDRegionInfo sDRegionInfo;
			  sDRegionInfo.acc_if_node = acc_if_node;
			  //sDRegionInfo.acc_copy_nodes = acc_copy_nodes; 	  /* Points to (optional) copy nodes */
			  //sDRegionInfo.acc_copyin_nodes = acc_copyin_nodes;   /* Points to (optional) copyin nodes */
			  //sDRegionInfo.acc_copyout_nodes = acc_copyout_nodes; /* Points to (optional) copyout nodes */	  
			  //sDRegionInfo.acc_create_nodes = acc_create_nodes;
			  sDRegionInfo.acc_present_nodes = acc_present_nodes;
			  sDRegionInfo.acc_present_or_copy_nodes = acc_present_or_copy_nodes;
			  sDRegionInfo.acc_present_or_copyin_nodes = acc_present_or_copyin_nodes;
			  sDRegionInfo.acc_present_or_copyout_nodes = acc_present_or_copyout_nodes;
			  sDRegionInfo.acc_present_or_create_nodes = acc_present_or_create_nodes;
			  sDRegionInfo.acc_deviceptr_nodes = acc_deviceptr_nodes;
			  acc_nested_dregion_info.DRegionInfo.push_back(sDRegionInfo);
			  acc_nested_dregion_info.Depth ++;
		
			  //reset them
			  acc_if_node = NULL;
			  acc_copy_nodes = NULL;	  /* Points to (optional) copy nodes */
			  acc_copyin_nodes = NULL;	  /* Points to (optional) copyin nodes */
			  acc_copyout_nodes = NULL;   /* Points to (optional) copyout nodes */	  
			  acc_create_nodes = NULL;
			  acc_present_nodes = NULL;
			  acc_present_or_copy_nodes = NULL;
			  acc_present_or_copyin_nodes = NULL;
			  acc_present_or_copyout_nodes = NULL;
			  acc_present_or_create_nodes = NULL;
			  acc_deviceptr_nodes = NULL;
			  //cotinue process the body
			  subBlock = ACC_Process_DataRegion(wn_stmt_block);
			  acc_nested_dregion_info.DRegionInfo.pop_back();
			  acc_nested_dregion_info.Depth --;
		  }
		  else if(acc_process_type == ACCP_HOST_DATA_REGION)
		  {
		      //TODO for host construct region
		      subBlock = ACC_Process_HostDataRegion(wn_stmt_block);
		  }
		  else if(acc_process_type == ACCP_PARALLEL_REGION)
		  {
			  //It will include LOOP constructs
		      subBlock = ACC_Process_ParallelRegion(wn_stmt_block, wn_cont_nodes);
		  }		  	  
		  else if (acc_process_type == ACCP_KERNEL_REGION) 
		  {
		  	  //generate kernel and launch the kernel
		      subBlock = ACC_Process_KernelsRegion(wn_stmt_block, wn_cont_nodes);
		  } 	  	  
		  else if (acc_process_type == ACCP_WAIT_REGION) 
		  {
		  	  //Wait
		      subBlock = ACC_Process_WaitRegion(wn_stmt_block);
		  }
		  else if (acc_process_type == ACCP_DECLARE_REGION) 
		  {
		  	  //Declare
		      subBlock = ACC_Process_DeclareRegion(wn_stmt_block);
		  } 	  	  
		  else if (acc_process_type == ACCP_CACHE_REGION) 
		  {
		  	  //Cache
		      subBlock = ACC_Process_CacheRegion(wn_stmt_block);
		  } 	  	  
		  else if (acc_process_type == ACCP_UPDATE_REGION) 
		  {
		  	  //Update
		      subBlock = ACC_Process_UpdateRegion(wn_stmt_block);
		  } 

		  WN_INSERT_BlockLast(replacement_block, subBlock);
		
	}
	else if(WN_opcode(cur_node) != OPC_REGION_EXIT)//normal statement
	{
		int ikid = 0;
		//in case of the ACC pragma in the while-do/if-stmt etc.
		for (ikid = 0; ikid < WN_kid_count(cur_node); ikid++)
        if (WN_kid(cur_node, ikid) &&
           (WN_opcode(WN_kid(cur_node, ikid)) == OPC_BLOCK))
              WN_kid(cur_node, ikid) = ACC_Process_Block_Transform( WN_kid(cur_node, ikid) );
		
		WN_INSERT_BlockLast(replacement_block, cur_node);
	}
  }
    
  return replacement_block;  
}


WN* ACC_Process_ParallelRegion( WN * tree, WN* wn_cont)
{
	ParallelRegionInfo parallelRegionInfo;
	WN* wn;
    WN* wn_if_test = NULL;
	WN* wn_else_block = NULL;	
	WN* wn_if_stmt = NULL;
	WN* parallelBlock = WN_CreateBlock();
	
	WN* wn_acc_stack_op = Gen_ACC_Stack_Push();
	WN_INSERT_BlockLast(parallelBlock, wn_acc_stack_op);
	//process if
	if(acc_if_node)
    {
		WN* wn_origial_block = WN_COPY_Tree(tree);
		wn_if_test = WN_COPY_Tree(acc_if_node);
		wn_else_block = ACC_Remove_All_ACC_Pragma(wn_origial_block);
    }
	parallelRegionInfo.acc_if_node = acc_if_node;
	//sDRegionInfo.acc_copy_nodes = acc_copy_nodes; 	  /* Points to (optional) copy nodes */
	//sDRegionInfo.acc_copyin_nodes = acc_copyin_nodes;   /* Points to (optional) copyin nodes */
	//sDRegionInfo.acc_copyout_nodes = acc_copyout_nodes; /* Points to (optional) copyout nodes */	  
	//sDRegionInfo.acc_create_nodes = acc_create_nodes;
	parallelRegionInfo.acc_present_nodes = acc_present_nodes;
	parallelRegionInfo.acc_present_or_copy_nodes = acc_present_or_copy_nodes;
	parallelRegionInfo.acc_present_or_copyin_nodes = acc_present_or_copyin_nodes;
	parallelRegionInfo.acc_present_or_copyout_nodes = acc_present_or_copyout_nodes;
	parallelRegionInfo.acc_present_or_create_nodes = acc_present_or_create_nodes;
	parallelRegionInfo.acc_deviceptr_nodes = acc_deviceptr_nodes;
	parallelRegionInfo.acc_async = acc_async_nodes;
	parallelRegionInfo.acc_param = acc_parms_nodes;
	parallelRegionInfo.acc_private = acc_private_nodes;
	parallelRegionInfo.acc_firstprivate = acc_firstprivate_nodes;
	parallelRegionInfo.acc_num_gangs = acc_num_gangs_node;
	parallelRegionInfo.acc_num_workers = acc_num_workers_node;
	parallelRegionInfo.acc_vector_length = acc_vector_length_node;
	parallelRegionInfo.acc_reduction_nodes = acc_reduction_nodes;

	parallelRegionInfo.acc_dregion_pcreate = acc_dregion_pcreate;		
	parallelRegionInfo.acc_dregion_pcopy = acc_dregion_pcopy;		
	parallelRegionInfo.acc_dregion_pcopyin = acc_dregion_pcopyin;		
	parallelRegionInfo.acc_dregion_pcopyout = acc_dregion_pcopyout;		
	parallelRegionInfo.acc_dregion_present = acc_dregion_present;
	parallelRegionInfo.acc_dregion_private = acc_dregion_private;		
	parallelRegionInfo.acc_dregion_fprivate = acc_dregion_fprivate;	
	parallelRegionInfo.acc_dregion_lprivate = acc_dregion_lprivate;
	parallelRegionInfo.acc_dregion_inout_scalar = acc_dregion_inout_scalar;

	
	while(acc_reduction_nodes)
	{
		ACC_ReductionMap acc_reductionMap;
		acc_reductionMap.hostName = WN_st(acc_reduction_nodes);
		acc_reductionMap.ReductionOpr = (OPERATOR)WN_pragma_arg2(acc_reduction_nodes);
		parallelRegionInfo.reductionmap.push_back(acc_reductionMap);
		WN* next_acc_reduction_nodes = WN_next(acc_reduction_nodes);
		WN_Delete(acc_reduction_nodes);
		acc_reduction_nodes = next_acc_reduction_nodes;
	}
	
	if(acc_wait_nodes)
	{
		ACC_GenWaitNodes(&acc_wait_list, parallelBlock);
		acc_wait_nodes = NULL;
		acc_wait_list.clear();
	}

	//async expr
	if(acc_async_nodes)
	{		
		acc_AsyncExpr = WN_COPY_Tree(WN_kid0(acc_async_nodes));
		WN_Delete(acc_async_nodes); 
	}
	//declare the data	
	if(acc_present_or_copy_nodes)
	{
		//declaration, cuda malloc 
		ACC_GenDeviceCreateCopyInOut(&parallelRegionInfo.acc_dregion_pcopy, 
  										&parallelRegionInfo.pcopyMap, 
  										parallelBlock, TRUE);
	}
	if(acc_present_or_copyin_nodes)
	{
		//declaration, cuda malloc 
		ACC_GenDeviceCreateCopyInOut(&parallelRegionInfo.acc_dregion_pcopyin, 
  										&parallelRegionInfo.pcopyinMap, 
  										parallelBlock, TRUE);
	}
	if(acc_present_or_copyout_nodes)
	{
		//Only declaration and cuda malloc, no data movement
		ACC_GenDeviceCreateCopyInOut(&parallelRegionInfo.acc_dregion_pcopyout, 
  										&parallelRegionInfo.pcopyoutMap, 
  										parallelBlock, FALSE);
	}
	if(acc_present_or_create_nodes)
	{
		//Only declaration and cuda malloc, no data movement
		ACC_GenDeviceCreateCopyInOut(&parallelRegionInfo.acc_dregion_pcreate, 
  										&parallelRegionInfo.pcreateMap, 
  										parallelBlock, FALSE);
	}
	if(acc_present_nodes)
	{
		//Only declaration and cuda malloc, no data movement
		ACC_GenPresentNode(&parallelRegionInfo.acc_dregion_present, 
  										&parallelRegionInfo.presentMap, 
  										parallelBlock);
	}
	if(acc_deviceptr_nodes)
	{
		//Only declaration and cuda malloc, no data movement
		ACC_GenDevicePtr(acc_deviceptr_nodes, 
  										&parallelRegionInfo.dptrList);
	}
	//process scalar variables
	ACC_Process_scalar_variable_for_offload_region(tree, FALSE);
	//finalize the scalar data property(IN/OUT/INOUT/PRIVATE/CREATE/PRESENT)
	ACC_Find_Scalar_Var_Inclose_Data_Clause((void*)&parallelRegionInfo, FALSE);
	
	//find the agregate data that is not specified in the kernels/enclosed data region
	//they are treated as pcopy
	ACC_Find_Unspecified_Array();
	ACC_GenDeviceCreateCopyInOut(&acc_unspecified_dregion_pcopy, 
										&acc_unspecified_pcopyMap, 
										parallelBlock, TRUE);
	int iRDIdx = 0;
	if(iRDIdx < parallelRegionInfo.reductionmap.size())
	{
		ACC_ReductionMap reductionMap = parallelRegionInfo.reductionmap[iRDIdx];
		ST* st_host = reductionMap.hostName;
		//set the reduction var as inout
		acc_offload_scalar_management_tab[st_host]->acc_scalar_type = ACC_SCALAR_VAR_INOUT;
		iRDIdx ++;
	}
	//Let's process scalar variable
	if(acc_dfa_enabled == FALSE)
	{
		//finalize the parameters for device computation kernel function
		Finalize_Kernel_Parameters();
	}
	//reset them
	acc_if_node = NULL;
	acc_copy_nodes = NULL;	  /* Points to (optional) copy nodes */
	acc_copyin_nodes = NULL;	  /* Points to (optional) copyin nodes */
	acc_copyout_nodes = NULL;   /* Points to (optional) copyout nodes */	  
	acc_create_nodes = NULL;
	acc_present_nodes = NULL;
	acc_present_or_copy_nodes = NULL;
	acc_present_or_copyin_nodes = NULL;
	acc_present_or_copyout_nodes = NULL;
	acc_present_or_create_nodes = NULL;
	acc_deviceptr_nodes = NULL;
	acc_async_nodes = NULL;
	acc_private_nodes = NULL;
	acc_firstprivate_nodes = NULL;
	acc_lastprivate_nodes = NULL;
	acc_inout_nodes = NULL;
	acc_num_gangs_node = NULL;
	acc_num_workers_node = NULL;
	acc_vector_length_node = NULL;
	acc_reduction_nodes = NULL;
			  
	acc_dregion_pcreate.clear();
	acc_dregion_pcopy.clear();
	acc_dregion_pcopyin.clear();
	acc_dregion_pcopyout.clear();
	acc_dregion_present.clear();
	acc_dregion_host.clear();
	acc_dregion_device.clear();
	acc_dregion_private.clear();
	acc_dregion_fprivate.clear();
	acc_dregion_lprivate.clear();
	acc_dregion_inout_scalar.clear();
	acc_local_new_var_map.clear();
	acc_reduction_count = 0;
	
    
	
	
	/******************************************************************/
	//Generate scalar value in/out processing, from liveness analysis results
	ACC_Scalar_Variable_CreateAndCopyInOut(parallelBlock);
	/******************************************************************/
	//generate parameters
	acc_kernelLaunchParamList.clear();
	ACC_Generate_KernelParameters(acc_parms_nodes, (void*)&parallelRegionInfo, FALSE);
    //multi kernels and multi loopnest process
    //ACC_Setup_GPU_toplogy_parallel(&parallelRegionInfo, parallelBlock);
    //ACC_Handle_Parallel_Loops(tree, &parallelRegionInfo, parallelBlock);
    Transform_ACC_Parallel_Block_New(tree, parallelBlock, parallelRegionInfo.acc_num_gangs,
    								parallelRegionInfo.acc_num_workers, 
    								parallelRegionInfo.acc_vector_length);
	acc_kernel_functions_st.clear();
	acc_top_level_loop_rdc.clear();
	acc_shared_memory_for_reduction_block.clear();
	//Generate the kernel launch function.
	//WN_INSERT_BlockLast(acc_replace_block, LauchKernel(0));	
	//Generate the data copy back function
	
	/****************************************************************************/
	/*Copyout the data and clear the vectors and map buffer*/
	ACC_Scalar_Variable_Copyout(parallelBlock);
	acc_offload_scalar_management_tab.clear();
	acc_offload_params_management_tab.clear();
	acc_unspecified_dregion_pcopy.clear();
	acc_unspecified_pcopyMap.clear();
	acc_const_offload_ptr.clear();
	acc_const_offload_scalar.clear();
	while(acc_parms_nodes)
	{
		WN* next = WN_next(acc_parms_nodes);
		WN_Delete(acc_parms_nodes);
		acc_parms_nodes = next;
	}
	acc_parms_nodes = NULL;
	/****************************************************************************/
	if(parallelRegionInfo.acc_present_or_copyout_nodes)
		ACC_GenDataCopyOut(&parallelRegionInfo.pcopyoutMap, parallelBlock);
	if(parallelRegionInfo.acc_present_or_copy_nodes)
		ACC_GenDataCopyOut(&parallelRegionInfo.pcopyMap, parallelBlock);
	
	if(acc_AsyncExpr)
		WN_Delete(acc_AsyncExpr);
	acc_AsyncExpr = NULL;
	
	/****************************************************************************/
	//Free device memory
	/*if(parallelRegionInfo.acc_present_or_copyin_nodes)
		ACC_GenDeviceMemFreeInBatch(&parallelRegionInfo.pcopyinMap, parallelBlock);
	if(parallelRegionInfo.acc_present_or_copyout_nodes)
		ACC_GenDeviceMemFreeInBatch(&parallelRegionInfo.pcopyoutMap, parallelBlock);
	if(parallelRegionInfo.acc_present_or_copy_nodes)
		ACC_GenDeviceMemFreeInBatch(&parallelRegionInfo.pcopyMap, parallelBlock);
	if(parallelRegionInfo.acc_present_or_create_nodes)
		ACC_GenDeviceMemFreeInBatch(&parallelRegionInfo.pcreateMap, parallelBlock);*/
	wn_acc_stack_op = Gen_ACC_Free_Device_Ptr_InCurrentStack();
	WN_INSERT_BlockLast(parallelBlock, wn_acc_stack_op);
	wn_acc_stack_op = Gen_ACC_Stack_Pop();
	WN_INSERT_BlockLast(parallelBlock, wn_acc_stack_op);

	if(wn_if_test)
	{
		wn_if_stmt = WN_CreateIf(wn_if_test, parallelBlock, wn_else_block);
		WN* wn_return_block = WN_CreateBlock();
		WN_INSERT_BlockLast(wn_return_block, wn_if_stmt);
		parallelBlock = wn_return_block;
	}
	return parallelBlock;
}

/*basically replace all the function call parameters with device variables which are specified by use_device clauses*/
static WN * ACC_Walk_and_Localize_Host_Data(WN * tree)
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

  /*if (opr == OPR_LDID) 
  {
    old_sym = WN_st(tree);
    old_offset = WN_offsetx(tree);
	map<ST*, ACC_VAR_TABLE>::iterator itor = acc_local_new_var_map_host_data.find(old_sym);
	if(itor != acc_local_new_var_map_host_data.end())	
    //for (w=vtab; w->orig_st; w++) 
	{
	  ACC_VAR_TABLE newVar = itor->second;
      if ((newVar.orig_st == old_sym) &&
	  (newVar.has_offset ? (newVar.orig_offset == old_offset) : TRUE )) 
	  {	
  		  WN_st_idx(tree) = ST_st_idx(newVar.new_st);
		  // for reduction of a field of STRUCT, the TY_kind would be different
		  // And, we need to fix the TY for the wn, the field_id, and offsetx
		  // As the local_xxx symbol is always .predef..., so field_id should be 0
		  if (TY_kind(ST_type(newVar.new_st)) != TY_kind(WN_ty(tree)))
		  {
	        WN_set_ty(tree, ST_type(newVar.new_st));
	    	WN_set_field_id(tree, 0);
		  }
		  if (newVar.has_offset)
		    ACC_WN_set_offsetx(tree, newVar.new_offset);
      }
    }
  }  
  else */
  if (opr == OPR_LDA) 
  {
    old_sym = WN_st(tree);
    old_offset = WN_offsetx(tree);
	map<ST*, ACC_VAR_TABLE>::iterator itor = acc_local_new_var_map_host_data.find(old_sym);
	if(itor != acc_local_new_var_map_host_data.end())	
    //for (w=vtab; w->orig_st; w++) 
	{
	  ACC_VAR_TABLE newVar = itor->second;
      if ((newVar.orig_st == old_sym)) 
	  {	
	  	  if(TY_kind(ST_type(old_sym)) == KIND_ARRAY && TY_kind(ST_type(newVar.new_st)) == KIND_POINTER)
	  	  {
	  	  	 
				tree = WN_Ldid(TY_mtype(ST_type(newVar.new_st)), 
									0, newVar.new_st, ST_type(newVar.new_st));
	  	  }
		  else
		 {
	  		  WN_st_idx(tree) = ST_st_idx(newVar.new_st);
			  // for reduction of a field of STRUCT, the TY_kind would be different
			  // And, we need to fix the TY for the wn, the field_id, and offsetx
			  // As the local_xxx symbol is always .predef..., so field_id should be 0
			  if (TY_kind(ST_type(newVar.new_st)) != TY_kind(WN_ty(tree)))
			  {
		        WN_set_ty(tree, ST_type(newVar.new_st));
		    	WN_set_field_id(tree, 0);
			  }
			  if (newVar.has_offset)
			    ACC_WN_set_offsetx(tree, newVar.new_offset);
		 }
      }
    }
  }
  else if (OPCODE_has_sym(op) && WN_st(tree)) 
  {
    old_sym = WN_st(tree);
    old_offset = OPCODE_has_offset(op) ? WN_offsetx(tree) : 0;
	
	map<ST*, ACC_VAR_TABLE>::iterator itor = acc_local_new_var_map_host_data.find(old_sym);
	
	if(itor != acc_local_new_var_map_host_data.end())	
	{
	  	ACC_VAR_TABLE newVar = itor->second;
	    //for (w=vtab; w->orig_st; w++) 
	      if ((newVar.orig_st == old_sym) &&
		  		(newVar.has_offset ? (newVar.orig_offset == old_offset) : TRUE )) 
		  {
			WN_st_idx(tree) = ST_st_idx(newVar.new_st);
			if (OPCODE_has_offset(op) && newVar.has_offset)
			  ACC_WN_set_offsetx(tree, newVar.new_offset);
	      }
	}
  }

  /* Walk all children */
  if (op == OPC_BLOCK) 
  {
    r = WN_first(tree);
    while (r) 
	{ // localize each node in block
      r = ACC_Walk_and_Localize_Host_Data ( r);
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
      WN_kid(tree, i) = ACC_Walk_and_Localize_Host_Data ( WN_kid(tree, i));
    }
  }
  return (tree);
}   

WN* ACC_Process_HostDataRegion( WN * tree )
{
	
	WN* wn_HostDataBlock = WN_CreateBlock();
	int i = 0;
	vector<ACC_DATA_ST_MAP*> HDMap;
	if(acc_use_device_nodes)
  		ACC_GenPresentNode(&acc_host_data_use_device,
  										&HDMap, wn_HostDataBlock);
	
	for (i=0; i<HDMap.size(); i++) 
	{
		ST* host_name = HDMap[i]->hostName;
		ACC_VAR_TABLE var;
		var.new_st = HDMap[i]->deviceName;
		var.orig_st = host_name;
		var.has_offset = FALSE;
		var.new_offset = 0;
		var.orig_offset = 0;
		acc_local_new_var_map_host_data[host_name] = var;
	}
	WN* newTree = ACC_Walk_and_Localize_Host_Data(tree);
	WN_INSERT_BlockLast(wn_HostDataBlock, newTree);

	i=0;
	for (i=0; i<HDMap.size(); i++) 
		delete HDMap[i];
	
	acc_local_new_var_map_host_data.clear();
	
    for (i=0; i<acc_host_data_use_device.size(); i++) 
	{
	    ACC_DREGION__ENTRY dentry = acc_host_data_use_device[i];
		int iter = 0;
		WN_Delete (dentry.acc_data_clauses);
		while(iter < dentry.acc_data_start.size())
		{
	    	WN_Delete (dentry.acc_data_start[iter]);
	    	WN_Delete (dentry.acc_data_length[iter]);
			iter ++;
		}
		dentry.acc_data_start.clear();
		dentry.acc_data_length.clear();
    }
		
	return wn_HostDataBlock;
}


WN* ACC_Process_WaitRegion( WN * tree )
{
	WN* wn_waitBlock = WN_CreateBlock();
	if(acc_wait_nodes)
	{
		ACC_GenWaitNodes(&acc_wait_list, wn_waitBlock);
		acc_wait_nodes = NULL;
		acc_wait_list.clear();
	}
	else
	{
		//wait for all stream
		WN_INSERT_BlockLast(wn_waitBlock, ACC_GenWaitStream(WN_Intconst(MTYPE_I4, 0)));
	}
	
	return wn_waitBlock; 
}


WN* ACC_Process_UpdateRegion( WN * tree )
{
	int i;
	WN* wn_updateBlock = WN_CreateBlock();
	WN* dClause;
	
	ACC_DREGION__ENTRY dentry;
	//async expr
	if(acc_async_nodes)
	{		
		acc_AsyncExpr = WN_COPY_Tree(WN_kid0(acc_async_nodes));
		WN_Delete(acc_async_nodes); 
		acc_async_nodes = NULL;
	}
	
	if(acc_wait_nodes)
	{
		ACC_GenWaitNodes(&acc_wait_list, wn_updateBlock);
		acc_wait_nodes = NULL;
		acc_wait_list.clear();
	}
	
	for (i=0; i<acc_dregion_host.size(); i++) 
	{
		ACC_DREGION__ENTRY dentry = acc_dregion_host[i];
		dClause = dentry.acc_data_clauses;

		WN* wnSize = ACC_GetArraySize(dentry);
		WN* wnStart = ACC_GetArrayStart(dentry);
		ST *old_st = WN_st(dClause);
		TY_IDX ty = ST_type(old_st);
		TY_IDX etype;
		TY_KIND kind = TY_kind(ty);
		if(F90_ST_Has_Dope_Vector(old_st))
	   	{
         		etype = ACC_GetDopeElementType(old_st);
		}
		else if(kind == KIND_ARRAY)
			etype = ACC_GetArrayElemType(ty);
		else
		{
			etype = TY_pointed(ty);
			if(TY_kind(etype) == KIND_ARRAY)
			{
		        etype = ACC_GetArrayElemType(etype);
			}
		}
		unsigned int type_size = TY_size(etype);
		WN_OFFSET old_offset = WN_offsetx(dClause);
		WN_INSERT_BlockLast(wn_updateBlock, 
					ACC_GenUpdateHostVar(old_st, wnStart, wnSize, type_size));
	}
	
	for (i=0; i<acc_dregion_device.size(); i++)  
	{	  
	  ACC_DREGION__ENTRY dentry = acc_dregion_device[i];
	  dClause = dentry.acc_data_clauses;
	  WN* wnSize = ACC_GetArraySize(dentry);
	  WN* wnStart = ACC_GetArrayStart(dentry);
	  ST *old_st = WN_st(dClause);
		TY_IDX ty = ST_type(old_st);
		TY_IDX etype;
		TY_KIND kind = TY_kind(ty);
		if(F90_ST_Has_Dope_Vector(old_st))
		{
			etype = ACC_GetDopeElementType(old_st);
		}
		else if(kind == KIND_ARRAY)
			etype = ACC_GetArrayElemType(ty);
		else
		{
			etype = TY_pointed(ty);
			if(TY_kind(etype) == KIND_ARRAY)
			{
		        etype = ACC_GetArrayElemType(etype);
			}
		}
		unsigned int type_size = TY_size(etype);
	  WN_OFFSET old_offset = WN_offsetx(dClause);
	  WN_INSERT_BlockLast(wn_updateBlock, 
	  			ACC_GenUpdateDeviceVar(old_st, wnStart, wnSize, type_size));
	}

	if(acc_AsyncExpr)
		WN_Delete(acc_AsyncExpr);
	acc_AsyncExpr = NULL;
	
    for (i=0; i<acc_dregion_host.size(); i++) 
	{
	    ACC_DREGION__ENTRY dentry = acc_dregion_host[i];
		int iter = 0;
		WN_Delete (dentry.acc_data_clauses);
		while(iter < dentry.acc_data_start.size())
		{
	    	WN_Delete (dentry.acc_data_start[iter]);
	    	WN_Delete (dentry.acc_data_length[iter]);
			iter ++;
		}
		dentry.acc_data_start.clear();
		dentry.acc_data_length.clear();
    }
	
    for (i=0; i<acc_dregion_device.size(); i++) 
	{
	    ACC_DREGION__ENTRY dentry = acc_dregion_device[i];
		int iter = 0;
		WN_Delete (dentry.acc_data_clauses);
		while(iter < dentry.acc_data_start.size())
		{
	    	WN_Delete (dentry.acc_data_start[iter]);
	    	WN_Delete (dentry.acc_data_length[iter]);
			iter ++;
		}
		dentry.acc_data_start.clear();
		dentry.acc_data_length.clear();
    }
	acc_dregion_host.clear();
	acc_dregion_device.clear();
	if(acc_if_node)
	{
		WN* wn_if_stmt = WN_CreateIf(WN_COPY_Tree(acc_if_node), wn_updateBlock, WN_CreateBlock());
		WN* wn_return_block = WN_CreateBlock();
		WN_INSERT_BlockLast(wn_return_block, wn_if_stmt);
		wn_updateBlock = wn_return_block;
		WN_Delete(acc_if_node);
		acc_if_node = NULL;
	}
	
	return wn_updateBlock;
}


WN* ACC_Process_CacheRegion( WN * tree )
{
	return NULL;
}


WN* ACC_Process_DeclareRegion( WN * tree )
{
	return NULL;
}

WN* ACC_Process_ExitData_Region( WN * tree )
{	
	int i;
	WN* wn_exitBlock = WN_CreateBlock();	
	WN* dClause;
					
	vector<ACC_DATA_ST_MAP*> CopyinMap; 					
	vector<ACC_DATA_ST_MAP*> CreateMap;
	
	if(acc_wait_nodes)
	{
		ACC_GenWaitNodes(&acc_wait_list, wn_exitBlock);
		acc_wait_nodes = NULL;
		acc_wait_list.clear();
	}
		
	//async expr
	if(acc_async_nodes)
	{		
		acc_AsyncExpr = WN_COPY_Tree(WN_kid0(acc_async_nodes));
		WN_Delete(acc_async_nodes); 
		acc_async_nodes = NULL;
	}
	
	if(acc_dregion_pcopyout.size() != 0)
	{
		for (i=0; i<acc_dregion_pcopyout.size(); i++)
		{
			ACC_DREGION__ENTRY dentry = acc_dregion_pcopyout[i];
			dClause = dentry.acc_data_clauses;

			WN* wnSize = ACC_GetArraySize(dentry);
			WN* wnStart = ACC_GetArrayStart(dentry);
			ST *old_st = WN_st(dClause);
			WN* wn_Elemsize = ACC_GetArrayElementSize(dentry);
                        unsigned int type_size = WN_const_val(wn_Elemsize);
			WN_INSERT_BlockLast(wn_exitBlock, 
						ACC_GenUpdateHostVar(old_st, wnStart, wnSize, type_size));
		}
	}
	if(acc_dregion_delete.size() != 0)
	{
		for (i=0; i<acc_dregion_delete.size(); i++)
		{
			ACC_DREGION__ENTRY dentry = acc_dregion_delete[i];
			dClause = dentry.acc_data_clauses;

			WN* wnSize = ACC_GetArraySize(dentry);
			WN* wnStart = ACC_GetArrayStart(dentry);
			ST *old_st = WN_st(dClause);
			WN* wn_Elemsize = ACC_GetArrayElementSize(dentry);
                        unsigned int type_size = WN_const_val(wn_Elemsize);
			WN_INSERT_BlockLast(wn_exitBlock, 
						ACC_GenDataExitDelete(old_st, wnStart, wnSize, type_size));
		}
	}
	
	if(acc_AsyncExpr)
		WN_Delete(acc_AsyncExpr);
	acc_AsyncExpr = NULL;
	
    for (i=0; i<acc_dregion_pcopyout.size(); i++) 
	{
	    ACC_DREGION__ENTRY dentry = acc_dregion_pcopyout[i];
		int iter = 0;
		WN_Delete (dentry.acc_data_clauses);
		while(iter < dentry.acc_data_start.size())
		{
	    	WN_Delete (dentry.acc_data_start[iter]);
	    	WN_Delete (dentry.acc_data_length[iter]);
			iter ++;
		}
		dentry.acc_data_start.clear();
		dentry.acc_data_length.clear();
    }
	
    for (i=0; i<acc_dregion_delete.size(); i++) 
	{
	    ACC_DREGION__ENTRY dentry = acc_dregion_delete[i];
		int iter = 0;
		WN_Delete (dentry.acc_data_clauses);
		while(iter < dentry.acc_data_start.size())
		{
	    	WN_Delete (dentry.acc_data_start[iter]);
	    	WN_Delete (dentry.acc_data_length[iter]);
			iter ++;
		}
		dentry.acc_data_start.clear();
		dentry.acc_data_length.clear();
    }
	acc_dregion_pcopyout.clear();
	acc_dregion_delete.clear();
	if(acc_if_node)
	{
		WN* wn_if_stmt = WN_CreateIf(WN_COPY_Tree(acc_if_node), wn_exitBlock, WN_CreateBlock());
		WN* wn_return_block = WN_CreateBlock();
		WN_INSERT_BlockLast(wn_return_block, wn_if_stmt);
		wn_exitBlock = wn_return_block;
		WN_Delete(acc_if_node);
		acc_if_node = NULL;
	}
	
	return wn_exitBlock;
}


WN* ACC_Process_EnterData_Region( WN * tree )
{
	int i;
	WN* wn_enterBlock = WN_CreateBlock();
					
	vector<ACC_DATA_ST_MAP*> CopyinMap; 					
	vector<ACC_DATA_ST_MAP*> CreateMap;
	
	if(acc_wait_nodes)
	{
		ACC_GenWaitNodes(&acc_wait_list, wn_enterBlock);
		acc_wait_nodes = NULL;
		acc_wait_list.clear();
	}
		
	//async expr
	if(acc_async_nodes)
	{		
		acc_AsyncExpr = WN_COPY_Tree(WN_kid0(acc_async_nodes));
		WN_Delete(acc_async_nodes); 
		acc_async_nodes = NULL;
	}
	
	if(acc_dregion_pcopyin.size() != 0)
	{
		ACC_GenDeviceCreateCopyInOut(&acc_dregion_pcopyin, 
  										&CopyinMap, wn_enterBlock, TRUE, FALSE);
	}
	if(acc_dregion_pcreate.size() != 0)
	{
		ACC_GenDeviceCreateCopyInOut(&acc_dregion_pcreate, 
  										&CreateMap, wn_enterBlock, FALSE, FALSE);
	}
	
	if(acc_AsyncExpr)
		WN_Delete(acc_AsyncExpr);
	acc_AsyncExpr = NULL;
	
    for (i=0; i<acc_dregion_pcopyin.size(); i++) 
	{
	    ACC_DREGION__ENTRY dentry = acc_dregion_pcopyin[i];
		int iter = 0;
		WN_Delete (dentry.acc_data_clauses);
		while(iter < dentry.acc_data_start.size())
		{
	    	WN_Delete (dentry.acc_data_start[iter]);
	    	WN_Delete (dentry.acc_data_length[iter]);
			iter ++;
		}
		dentry.acc_data_start.clear();
		dentry.acc_data_length.clear();
    }
	
    for (i=0; i<acc_dregion_pcreate.size(); i++) 
	{
	    ACC_DREGION__ENTRY dentry = acc_dregion_pcreate[i];
		int iter = 0;
		WN_Delete (dentry.acc_data_clauses);
		while(iter < dentry.acc_data_start.size())
		{
	    	WN_Delete (dentry.acc_data_start[iter]);
	    	WN_Delete (dentry.acc_data_length[iter]);
			iter ++;
		}
		dentry.acc_data_start.clear();
		dentry.acc_data_length.clear();
    }
	acc_dregion_pcopyin.clear();
	acc_dregion_pcreate.clear();
	if(acc_if_node)
	{		
		WN* wn_if_stmt = WN_CreateIf(WN_COPY_Tree(acc_if_node), wn_enterBlock, WN_CreateBlock());
		WN* wn_return_block = WN_CreateBlock();
		WN_INSERT_BlockLast(wn_return_block, wn_if_stmt);
		wn_enterBlock = wn_return_block;
		WN_Delete(acc_if_node);
		acc_if_node = NULL;
	}
	
	return wn_enterBlock;
}



WN* ACC_Process_DataRegion( WN * tree )
{
  WN* node = tree;
  int i = 0;
  ACCP_process_type acc_process_type;
  WN* wn_if_test = NULL;
  WN* wn_else_block = NULL;
  WN* wn_if_stmt = NULL;
  WN* wn_next_node; // = WN_next(wtmp);
  WN* wn_cont_nodes;// = WN_next(node);
  WN* wn_stmt_block;// = WN_region_body(node);
  WN* cur_node, *next_node, *prev_node;
  WN* DRegion_replacement_block = WN_CreateBlock();
  WN* wn_acc_stack_op = Gen_ACC_Stack_Push();
  WN_INSERT_BlockLast(DRegion_replacement_block, wn_acc_stack_op);
  //acc_loopinfo.acc_forloop
  SingleDRegionInfo sDRegionInfo = 
  					acc_nested_dregion_info.DRegionInfo[acc_nested_dregion_info.Depth - 1];
  if(sDRegionInfo.acc_if_node)
  {
	WN* wn_origial_block = WN_COPY_Tree(tree);
	wn_if_test = WN_COPY_Tree(sDRegionInfo.acc_if_node);
	wn_else_block = ACC_Remove_All_ACC_Pragma(wn_origial_block);
  }
  
  if(sDRegionInfo.acc_present_or_copyin_nodes)
  	ACC_GenDeviceCreateCopyInOut(&sDRegionInfo.acc_dregion_pcopyin, 
  										&sDRegionInfo.pcopyinMap, DRegion_replacement_block, TRUE);
  if(sDRegionInfo.acc_present_or_copyout_nodes)
  	ACC_GenDeviceCreateCopyInOut(&sDRegionInfo.acc_dregion_pcopyout,
  										&sDRegionInfo.pcopyoutMap, DRegion_replacement_block, FALSE);
  if(sDRegionInfo.acc_present_or_copy_nodes)
  	ACC_GenDeviceCreateCopyInOut(&sDRegionInfo.acc_dregion_pcopy,
  										&sDRegionInfo.pcopyMap, DRegion_replacement_block, TRUE);
  if(sDRegionInfo.acc_present_or_create_nodes)
  	ACC_GenDeviceCreateCopyInOut(&sDRegionInfo.acc_dregion_pcreate,
  										&sDRegionInfo.pcreateMap, DRegion_replacement_block, FALSE);
  if(sDRegionInfo.acc_present_nodes)
  	ACC_GenPresentNode(&sDRegionInfo.acc_dregion_present,
  										&sDRegionInfo.presentMap, DRegion_replacement_block);
  if(sDRegionInfo.acc_deviceptr_nodes)
  	ACC_GenDevicePtr(sDRegionInfo.acc_deviceptr_nodes, &sDRegionInfo.dptrList);
  
  acc_nested_dregion_info.DRegionInfo[acc_nested_dregion_info.Depth - 1] = sDRegionInfo;
  
  for (cur_node = WN_first(tree); cur_node; cur_node = next_node)  
  {
    prev_node = WN_prev(cur_node);
    next_node = WN_next(cur_node);
	if ((WN_opcode(cur_node) == OPC_REGION) &&
			 WN_first(WN_region_pragmas(cur_node)) &&
			 ((WN_opcode(WN_first(WN_region_pragmas(cur_node))) == OPC_PRAGMA) ||
			  (WN_opcode(WN_first(WN_region_pragmas(cur_node))) == OPC_XPRAGMA)))
	{
	    WN *wtmp = WN_first(WN_region_pragmas(cur_node));
	    WN_PRAGMA_ID wid = (WN_PRAGMA_ID) WN_pragma(wtmp);
		WN* subBlock;

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

	    default:
	      printf("pragma value = %d", (int)wid);
	      Fail_FmtAssertion (
			 "out of context pragma (%s) in acc {primary pragma} processing",
			 WN_pragmas[wid].name);
	    }
		
	    wn_next_node = WN_next(wtmp);
	    wn_cont_nodes = WN_next(cur_node);
	    wn_stmt_block = WN_region_body(cur_node);
		if (acc_process_type == ACCP_WAIT_REGION
			|| acc_process_type == ACCP_UPDATE_REGION
			|| acc_process_type == ACCP_DECLARE_REGION
			|| acc_process_type == ACCP_CACHE_REGION) 
	    {
	      WN_Delete ( wtmp );
	      WN_Delete ( WN_region_pragmas(cur_node) );
	      WN_DELETE_Tree ( WN_region_exits(cur_node) );
	    }
		
		///////////////////////////////////////////
		//process clauses
		ACC_Process_Clause_Pragma(wn_next_node);
		//Begin to process the body
		if (acc_process_type == ACCP_DATA_REGION)
		{
			  //it will include any other constructs.
			  //Get the information and move to the next stage
			  SingleDRegionInfo sDRegionInfo;
			  sDRegionInfo.acc_if_node = acc_if_node;
			  //sDRegionInfo.acc_copy_nodes = acc_copy_nodes; 	  /* Points to (optional) copy nodes */
			  //sDRegionInfo.acc_copyin_nodes = acc_copyin_nodes;   /* Points to (optional) copyin nodes */
			  //sDRegionInfo.acc_copyout_nodes = acc_copyout_nodes; /* Points to (optional) copyout nodes */	  
			  //sDRegionInfo.acc_create_nodes = acc_create_nodes;
			  sDRegionInfo.acc_present_nodes = acc_present_nodes;
			  sDRegionInfo.acc_present_or_copy_nodes = acc_present_or_copy_nodes;
			  sDRegionInfo.acc_present_or_copyin_nodes = acc_present_or_copyin_nodes;
			  sDRegionInfo.acc_present_or_copyout_nodes = acc_present_or_copyout_nodes;
			  sDRegionInfo.acc_present_or_create_nodes = acc_present_or_create_nodes;
			  sDRegionInfo.acc_deviceptr_nodes = acc_deviceptr_nodes;
			  sDRegionInfo.wn_cont_nodes = wn_cont_nodes;
			  sDRegionInfo.wn_stmt_block = wn_stmt_block;

				sDRegionInfo.acc_dregion_pcreate = acc_dregion_pcreate;		
				sDRegionInfo.acc_dregion_pcopy = acc_dregion_pcopy;		
				sDRegionInfo.acc_dregion_pcopyin = acc_dregion_pcopyin;		
				sDRegionInfo.acc_dregion_pcopyout = acc_dregion_pcopyout;		
				sDRegionInfo.acc_dregion_present = acc_dregion_present;	
				sDRegionInfo.acc_dregion_private = acc_dregion_private;		
				sDRegionInfo.acc_dregion_fprivate = acc_dregion_fprivate;
			  
			  acc_nested_dregion_info.DRegionInfo.push_back(sDRegionInfo);
			  acc_nested_dregion_info.Depth ++;
		
			  //reset them
			  acc_if_node = NULL;
			  acc_copy_nodes = NULL;	  /* Points to (optional) copy nodes */
			  acc_copyin_nodes = NULL;	  /* Points to (optional) copyin nodes */
			  acc_copyout_nodes = NULL;   /* Points to (optional) copyout nodes */	  
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
			  //cotinue process the body
	
			  subBlock = ACC_Process_DataRegion(wn_stmt_block);
			  acc_nested_dregion_info.DRegionInfo.pop_back();
			  acc_nested_dregion_info.Depth --;
		  }
		  else if(acc_process_type == ACCP_HOST_DATA_REGION)
		  {
		      //TODO for host construct region
		      subBlock = ACC_Process_HostDataRegion(wn_stmt_block);
		  }
		  else if(acc_process_type == ACCP_PARALLEL_REGION)
		  {		  	  
			  //It will include LOOP constructs
		      subBlock = ACC_Process_ParallelRegion(wn_stmt_block, wn_cont_nodes);
		  }		  	  
		  else if (acc_process_type == ACCP_KERNEL_REGION) 
		  {
			  /////////////////////////////////////////////////////////
		  	  //generate kernel and launch the kernel
		      subBlock = ACC_Process_KernelsRegion(wn_stmt_block, wn_cont_nodes);
		  } 	  	  
		  else if (acc_process_type == ACCP_WAIT_REGION) 
		  {
		  	  //Wait
		      subBlock = ACC_Process_WaitRegion(wn_stmt_block);
		  }
		  else if (acc_process_type == ACCP_DECLARE_REGION) 
		  {
		  	  //Declare
		      subBlock = ACC_Process_DeclareRegion(wn_stmt_block);
		  } 	  	  
		  else if (acc_process_type == ACCP_CACHE_REGION) 
		  {
		  	  //Cache
		      subBlock = ACC_Process_CacheRegion(wn_stmt_block);
		  } 	  	  
		  else if (acc_process_type == ACCP_UPDATE_REGION) 
		  {
		  	  //Update
		      subBlock = ACC_Process_UpdateRegion(wn_stmt_block);
		  } 

		  WN_INSERT_BlockLast(DRegion_replacement_block, subBlock);
		
	}
	else if(WN_opcode(cur_node) != OPC_REGION_EXIT)//normal statement
	{
		int ikid = 0;
		//in case of the ACC pragma in the while-do/if-stmt etc.
		for (ikid = 0; ikid < WN_kid_count(cur_node); ikid++)
        if (WN_kid(cur_node, ikid) &&
           (WN_opcode(WN_kid(cur_node, ikid)) == OPC_BLOCK))
              WN_kid(cur_node, ikid) = ACC_Process_Block_Transform( WN_kid(cur_node, ikid) );
		
		WN_INSERT_BlockLast(DRegion_replacement_block, cur_node);
	}
  }
  
  if(sDRegionInfo.acc_present_or_copyout_nodes)
  	ACC_GenDataCopyOut(&sDRegionInfo.pcopyoutMap, DRegion_replacement_block);
  if(sDRegionInfo.acc_present_or_copy_nodes)
  	ACC_GenDataCopyOut(&sDRegionInfo.pcopyMap, DRegion_replacement_block);
  /****************************************************************************/
  //Free device memory
  /*if(sDRegionInfo.acc_present_or_copyin_nodes)
  	ACC_GenDeviceMemFreeInBatch(&sDRegionInfo.pcopyinMap, DRegion_replacement_block);
  if(sDRegionInfo.acc_present_or_copyout_nodes)
  	ACC_GenDeviceMemFreeInBatch(&sDRegionInfo.pcopyoutMap, DRegion_replacement_block);
  if(sDRegionInfo.acc_present_or_copy_nodes)
  	ACC_GenDeviceMemFreeInBatch(&sDRegionInfo.pcopyMap, DRegion_replacement_block);
  if(sDRegionInfo.acc_present_or_create_nodes)
  	ACC_GenDeviceMemFreeInBatch(&sDRegionInfo.pcreateMap, DRegion_replacement_block);*/
  wn_acc_stack_op = Gen_ACC_Free_Device_Ptr_InCurrentStack();
  WN_INSERT_BlockLast(DRegion_replacement_block, wn_acc_stack_op);
  wn_acc_stack_op = Gen_ACC_Stack_Pop();
  WN_INSERT_BlockLast(DRegion_replacement_block, wn_acc_stack_op);
  acc_region_num ++;
  acc_construct_num = 0;

  if(wn_if_test)
  {
  	wn_if_stmt = WN_CreateIf(wn_if_test, DRegion_replacement_block, wn_else_block);
	WN* wn_return_block = WN_CreateBlock();
	WN_INSERT_BlockLast(wn_return_block, wn_if_stmt);
	DRegion_replacement_block = wn_return_block;
  }
  return DRegion_replacement_block;  
}

//no loops in the kernels region
static WN *
ACC_Linear_Stmts_Transformation_New(WN* wn_stmt, WN* wn_replacementlock)
{
	//Retrieve all the information need, then switch the sym table.
	WN* IndexGenerationBlock;
	int i;

	//swich the table
	/* Initialization. */	
	acc_psymtab = CURRENT_SYMTAB;
	acc_ppuinfo = Current_PU_Info;
	acc_pmaptab = Current_Map_Tab;
	//outline
    ACC_Create_Outline_OffloadRegion();
	ACC_Create_Private_Variables();
    WN* wn_offload_block = WN_CreateBlock ();
	ACC_Insert_Initial_Stmts(wn_offload_block);
	//////////////////////////////////////////////////////////////////
	//kernels body   
    WN_INSERT_BlockLast(wn_offload_block, wn_stmt);
    ACC_Walk_and_Localize(wn_offload_block);

	ACC_Gen_Scalar_Variables_CopyOut_InOffload(wn_offload_block);
   	WN_INSERT_BlockLast ( acc_parallel_func, wn_offload_block );
	/* Transfer any mappings for nodes moved from parent to parallel function */
	ACC_Transfer_Maps ( acc_pmaptab, acc_cmaptab, acc_parallel_func, 
		  PU_Info_regions_ptr(Current_PU_Info) );

	/* Create a new dependence graph for the child  and move all the 
	 appropriate vertices from the parent to the child graph */

	Current_Map_Tab = acc_cmaptab;
	//ACC_Fix_Dependence_Graph ( acc_ppuinfo, Current_PU_Info, acc_parallel_func ); 
	Current_Map_Tab = acc_pmaptab;

  
	/* Restore parent information. */

	CURRENT_SYMTAB = acc_psymtab;
	Current_PU_Info = acc_ppuinfo;
	Current_pu = &Current_PU_Info_pu();
	Current_Map_Tab = acc_pmaptab;
}


/***********************************************************************
1. the private variables are create here
2. the first private variables are created during the kernels parameters generation
3. inout/out variables are also create during the kernels parameters generation.
4. Reduction related variables are created in the reduction processing procedures.
All local variables created for CUDA/OpenCL kernels, are stored at  acc_local_new_var_map
************************************************************************/
static void ACC_Create_Private_Variables()
{
	map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor_offload_info = acc_offload_scalar_management_tab.begin();
	for(; itor_offload_info!=acc_offload_scalar_management_tab.end(); itor_offload_info++)
	{
		ACC_SCALAR_VAR_INFO* pVarInfo = itor_offload_info->second;
	    char szlocalname[256];	  
		//several cases:
		//1. ACC_SCALAR_VAR_IN, ACC_SCALAR_VAR_OUT and  ACC_SCALAR_VAR_INOUT
		//the new private var is created during kernel parameter generation
		//2. ACC_SCALAR_VAR_PRIVATE 2.1 the new private var is created during kernel parameter generation if it is reduction var
		//							 2.2  else the new private var is going to created.
		if(pVarInfo->acc_scalar_type == ACC_SCALAR_VAR_PRIVATE )
		{
			ST* st_private = pVarInfo->st_var;
			TY_IDX index_ty = ST_type(st_private);
		    ST* st_new_private = New_ST( CURRENT_SYMTAB );
			if(pVarInfo->is_reduction== FALSE)
	    		sprintf ( szlocalname, "%s", ST_name(st_private));
			else
				sprintf ( szlocalname, "_private_%s", ST_name(st_private));
			ST_Init(st_new_private,
		      Save_Str( szlocalname),
		      CLASS_VAR,
		      SCLASS_AUTO,
		      EXPORT_LOCAL,
		      MTYPE_To_TY(TY_mtype(index_ty)));//Put this variables in local table
			ACC_VAR_TABLE var;
			var.has_offset = FALSE;
			var.orig_st = st_private;
			var.new_st = st_new_private;
		    acc_local_new_var_map[st_private] = var;
			pVarInfo->st_device_in_klocal = st_new_private;
		}
	}
}

/**************************************************************************
init statements generation
e.g:
for all of the in/out/present/ previous create scalar variables
they all have pointers, so intial the scalar value from respective pointers.
***************************************************************************/
static void ACC_Insert_Initial_Stmts(WN* replacement_block)
{
	///////////////////////////////////////////////////////////////////////////////////////
	WN* wn_thenblock = WN_CreateBlock();
	WN* wn_elseblock = WN_CreateBlock();
	UINT32 reduction_across_gangs_counter = 0;
	///////////////////////////////////////////////////////////////////////////////////////
	map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor_offload_info;
	itor_offload_info = acc_offload_scalar_management_tab.begin();
	for(; itor_offload_info!=acc_offload_scalar_management_tab.end(); itor_offload_info++)
	{
		ACC_SCALAR_VAR_INFO* pVarInfo = itor_offload_info->second;
		//several cases:
		//1. ACC_SCALAR_VAR_IN, initialized in parameter
		//2. ACC_SCALAR_VAR_OUT : no init
		//3. ACC_SCALAR_VAR_INOUT: need init
		//2. ACC_SCALAR_VAR_PRIVATE no init
		//general inout/present put in the replacement_block
		//reduction variables put in the then/else block which will be further used in if-else stmt
		if(pVarInfo->acc_scalar_type == ACC_SCALAR_VAR_INOUT
			|| pVarInfo->acc_scalar_type == ACC_SCALAR_VAR_PRESENT
			|| pVarInfo->acc_scalar_type == ACC_SCALAR_VAR_CREATE)
		{
			ST* st_scalar = pVarInfo->st_device_in_klocal;
			ST* st_scalar_ptr = pVarInfo->st_device_in_kparameters;
			TY_IDX elem_ty = ST_type(st_scalar);			
			WN* wn_scalar_ptr = WN_Ldid(Pointer_type, 0, st_scalar_ptr, ST_type(st_scalar_ptr));
			WN* Init0 = WN_Iload(TY_mtype(elem_ty), 0,  elem_ty, wn_scalar_ptr);
			Init0 = WN_Stid(TY_mtype(elem_ty), 0, st_scalar, ST_type(st_scalar), Init0);
			//if this is a reduction across gangs
			if(pVarInfo->is_reduction == TRUE && pVarInfo->is_across_gangs == TRUE)
			{	
				OPERATOR ReductionOpr = pVarInfo->opr_reduction;
				TY_IDX ty = ST_type(pVarInfo->st_var);
				WN* Init1 = ACC_Get_Init_Value_of_Reduction(ReductionOpr, TY_mtype(ty));
				Init1 = WN_Stid(TY_mtype(elem_ty), 0, st_scalar, ST_type(st_scalar), Init1);
				WN_INSERT_BlockLast(wn_elseblock, Init1);
				WN_INSERT_BlockLast(wn_thenblock, Init0);
				reduction_across_gangs_counter ++;
				//Init0 = WN_CreateIf(wn_If_stmt_test, wn_thenblock, wn_elseblock);
				//WN_INSERT_BlockLast(wn_parallelBlock, Init0);
			}
			else
				WN_INSERT_BlockLast(replacement_block, Init0);
		}
	}
	if(reduction_across_gangs_counter)
	{
		TYPE_ID tid_type_id = TY_mtype(ST_type(glbl_threadIdx_x));
		WN* test1 = WN_Relational (OPR_EQ, tid_type_id, 
					WN_COPY_Tree(threadidx), 
					WN_Intconst(tid_type_id, 0));
		WN* test2 = WN_Relational (OPR_EQ, tid_type_id, 
					WN_COPY_Tree(threadidy), 
					WN_Intconst(tid_type_id, 0));
		WN* test3 = WN_Relational (OPR_EQ, tid_type_id, 
					WN_COPY_Tree(blockidx), 
					WN_Intconst(tid_type_id, 0));
		
		WN* wn_If_stmt_test = WN_Binary (OPR_CAND, Boolean_type, test1, test2);
		wn_If_stmt_test = WN_Binary (OPR_CAND, Boolean_type, wn_If_stmt_test, test3);
		WN* wn_ifthenelse = WN_CreateIf(wn_If_stmt_test, wn_thenblock, wn_elseblock);
		WN_INSERT_BlockLast(replacement_block, wn_ifthenelse);
	}
}


static void ACC_Gen_Scalar_Variables_CopyOut_InOffload(WN* wn_replace_block)
{
	//Init IN/OUT scalar variables
	///////////////////////////////////////////////////////////////////////////
	TYPE_ID predefined_type_id = TY_mtype(ST_type(glbl_threadIdx_x));
	
	WN* test1 = WN_Relational (OPR_EQ, predefined_type_id, 
					WN_COPY_Tree(threadidx), 
					WN_Intconst(predefined_type_id, 0));
	WN* test2 = WN_Relational (OPR_EQ, predefined_type_id, 
				WN_COPY_Tree(threadidy), 
				WN_Intconst(predefined_type_id, 0));
	WN* test3 = WN_Relational (OPR_EQ, predefined_type_id, 
				WN_COPY_Tree(blockidx), 
				WN_Intconst(predefined_type_id, 0));
	
	WN* wn_If_stmt_test = WN_Binary (OPR_CAND, Boolean_type, test1, test2);
	wn_If_stmt_test = WN_Binary (OPR_CAND, Boolean_type, wn_If_stmt_test, test3);
	WN* wn_thenblock = WN_CreateBlock();
	WN* wn_elseblock = WN_CreateBlock();
	
	UINT32 copyoutCount = 0;
	map<ST*, ACC_SCALAR_VAR_INFO*>::iterator itor_offload_info = acc_offload_scalar_management_tab.begin();
	for(; itor_offload_info!=acc_offload_scalar_management_tab.end(); itor_offload_info++)
	{
		ACC_SCALAR_VAR_INFO* pVarInfo = itor_offload_info->second;
		//several cases:
		//1. ACC_SCALAR_VAR_IN, initialized in parameter
		//2. ACC_SCALAR_VAR_OUT : no init
		//3. ACC_SCALAR_VAR_INOUT: need init
		//4. ACC_SCALAR_VAR_PRIVATE no init
		//5. if it is reduction across gang, there is no necessary to copyout.
		if((pVarInfo->acc_scalar_type == ACC_SCALAR_VAR_INOUT 
			|| pVarInfo->acc_scalar_type == ACC_SCALAR_VAR_OUT
			|| pVarInfo->acc_scalar_type == ACC_SCALAR_VAR_CREATE
			|| pVarInfo->acc_scalar_type == ACC_SCALAR_VAR_PRESENT)
			&& (pVarInfo->is_across_gangs == FALSE))
		{				
			ST* st_scalar = pVarInfo->st_device_in_klocal;
			ST* st_scalar_ptr = pVarInfo->st_device_in_kparameters;
			TY_IDX elem_ty = ST_type(st_scalar);
			WN* Init0 = WN_Ldid(TY_mtype(elem_ty), 0, st_scalar, ST_type(st_scalar));
			WN* wn_scalar_ptr = WN_Ldid(Pointer_type, 0, st_scalar_ptr, ST_type(st_scalar_ptr));
			Init0 = WN_Istore(TY_mtype(elem_ty), 0, Make_Pointer_Type(elem_ty), 
								wn_scalar_ptr, Init0);
			
			WN_INSERT_BlockLast(wn_thenblock, Init0);
			copyoutCount ++;
		}
	}
	if(copyoutCount != 0)
	{
		WN* wn_scalar_out = WN_CreateIf(wn_If_stmt_test, wn_thenblock, WN_CreateBlock());
		WN_INSERT_BlockLast(wn_replace_block, wn_scalar_out);
	}
}

/*Including Parallel Construct region*/
void Transform_ACC_Parallel_Block_New ( WN * tree, WN* wn_replace_block, 
										WN* wn_num_gangs, WN* wn_num_workers, WN* wn_vector_length)
{
	INT32 i;
		
	//acc_reduction_mem = ACC_RD_GLOBAL_MEM;
	//acc_reduction_rolling = ACC_RD_UNROLLING;
	//acc_reduction_exemode = ACC_RD_LAUNCH_KERNEL;
	//Scan and translate the loop region. Attaching the general nodes as well.
	//initialize the pre-process reduction variables
	//all the reduction clauses appearing in acc_top_level_loop_rdc have to be 
	//taken care during kernel_parameter generatation
	acc_top_level_loop_rdc.clear();	
	acc_additionalKernelLaunchParamList.clear();
	//here is the preprocess:
	//1. remove seq loop
	//2. identify loop schedulings
	//3. preprocess all the reduction clauses
	//////////////////////////////////////////////////////////////////////////////////////////////
	//step 1 remove seq loop
	tree = ACC_Walk_and_Replace_ACC_Loop_Seq(tree);
	//step 2: identify the loop scheduling
	Identify_Loop_Scheduling(tree, wn_replace_block, FALSE);
	//step 3: setup the toplogy, threads topology has to be setup.
	//the reduction need the number of gangs to allocate memory for global reduction	
	ACC_Setup_GPU_toplogy_for_Parallel(wn_num_gangs, wn_num_workers, 
											wn_vector_length, wn_replace_block);
	//step 4: extract the loop reduction inside the parallel region
	//the global device memory allocation for reduction will be insert into wn_replace_block 
	ACC_Parallel_Loop_Reduction_Extract_Public(tree, wn_replace_block);
	///////////////////////////////////////////////////////////////////////////////////////////////
	//create local var table map
    kernel_tmp_variable_count = 0;
	
	//kernel_parameters_count = acc_kernelLaunchParamList.size();

	acc_psymtab = CURRENT_SYMTAB;
	acc_ppuinfo = Current_PU_Info;
	acc_pmaptab = Current_Map_Tab;
	
	//outline
	ACC_Create_Outline_OffloadRegion();

	ACC_Create_Private_Variables();
    WN* wn_parallel_block = WN_CreateBlock ();
	ACC_Insert_Initial_Stmts(wn_parallel_block);
	//clear the array ref info for scalar replacement algorithm
	acc_loop_index_var.clear();
	//////////////////////////////////////////////////////////////////////////////////////////////	
	WN* wn_translated_offload_Region = ACC_Loop_Scheduling_Transformation_Gpu (tree, wn_parallel_block);
	acc_check_for_duplicates(wn_translated_offload_Region, "before localize");
	ACC_Walk_and_Localize(wn_translated_offload_Region);
	acc_check_for_duplicates(wn_translated_offload_Region, "after localize");
	WN_verifier(wn_translated_offload_Region);
	WN_INSERT_BlockLast ( wn_parallel_block, wn_translated_offload_Region);
	ACC_Gen_Scalar_Variables_CopyOut_InOffload(wn_parallel_block);
	//////////////////////////////////////////////////////////////////////////////////////////////	
	//////////////////////////////////////////////////////////////////////////////////////////////
   	WN_INSERT_BlockLast ( acc_parallel_func, wn_parallel_block );
	/* Transfer any mappings for nodes moved from parent to parallel function */
	ACC_Transfer_Maps ( acc_pmaptab, acc_cmaptab, acc_parallel_func, 
		  PU_Info_regions_ptr(Current_PU_Info) );
	
	/* Restore parent information. */

	CURRENT_SYMTAB = acc_psymtab;
	Current_PU_Info = acc_ppuinfo;
	Current_pu = &Current_PU_Info_pu();
	Current_Map_Tab = acc_pmaptab;

	//begin launch the kernel
	///////////////////////////////////////////////////////////////////////////////////////////////
	//launch kernels
	if(acc_target_arch == ACC_ARCH_TYPE_APU)
		ACC_Launch_HSA_Kernel(0, wn_replace_block);
	else
		ACC_LaunchKernel_nvidia(0, wn_replace_block, TRUE);
	//LaunchKernel(0, wn_replace_block, TRUE);
	///////////////////////////////////////////////////////////////////////////////////////////////
	//launch final reduction global reduction operations
	for(i=0; i<acc_top_level_loop_rdc.size(); i++)
	{
		FOR_LOOP_RDC_INFO loop_rdc_info = acc_top_level_loop_rdc[i];
		int irdc_idx = 0;
		for(irdc_idx=0; irdc_idx<loop_rdc_info.reductionmap.size(); irdc_idx ++)
		{
			ACC_ReductionMap* reductionmap = loop_rdc_info.reductionmap[irdc_idx]; 
			ST *st_old = reductionmap->hostName;
			ST *st_device = reductionmap->deviceName;
			ST *st_reductionKernel = reductionmap->reduction_kenels;	
			ACC_SCALAR_VAR_INFO* scalar_var_info = acc_offload_scalar_management_tab[st_old];
			ST* st_device_onhost = scalar_var_info->st_device_in_host;
			
			if(acc_target_arch == ACC_ARCH_TYPE_APU)
				GenFinalReductionAlgorithm_APU(st_device, st_device_onhost,
					st_reductionKernel, reductionmap->st_num_of_element, TY_size(ST_type(st_old)), 
					wn_replace_block);
			else
			{
				WN* wn_reduction_final_stmt = GenFinalReductionAlgorithm_nvidia(st_device, st_device_onhost,
					st_reductionKernel, reductionmap->st_num_of_element, TY_size(ST_type(st_old)));
				WN_INSERT_BlockLast(wn_replace_block, wn_reduction_final_stmt);
			}
		}
	}
}


/*
Transform for statement into GPU kernel statements.
written by daniel tian.
return a tree to replace for_tree, and for_tree should not be
contained in other trees..
*/
static WN *
ACC_Transform_Single_Nested_Loop_for_Kernels(WN* tree)
{
	//This tree must be OpenACC LOOP 
	BOOL is_loop_region = (WN_opcode(tree) == OPC_REGION 
							 && (WN_first(WN_region_pragmas(tree))) 
                             && (WN_opcode(WN_first(WN_region_pragmas(tree))) == OPC_PRAGMA)
                             && (WN_pragma(WN_first(WN_region_pragmas(tree)))==WN_PRAGMA_ACC_LOOP_BEGIN));
	
    if(is_loop_region == FALSE)
	{
		Fail_FmtAssertion ("Illegal input in the kernels transformation. ");
	}
	//Retrieve all the information need, then switch the sym table.
	WN* wn_replacement_block;
	int i;
	//swich the table
	/* Initialization. */	
	acc_psymtab = CURRENT_SYMTAB;
	acc_ppuinfo = Current_PU_Info;
	acc_pmaptab = Current_Map_Tab;
	//outline
	ACC_Create_Outline_OffloadRegion();
	//ACC_Push_Some_Globals( );
	//
	//int kernel_index_count = 0;

	ACC_Create_Private_Variables();
    wn_replacement_block = WN_CreateBlock ();
	ACC_Insert_Initial_Stmts(wn_replacement_block);
	//clear the array ref info for scalar replacement algorithm
	acc_loop_index_var.clear();
	//wn_translated_loop may not begin with DO_LOOP,
	//if there is some top-level reduction,
	//the reduction initialization is coming first.
	WN* wn_translated_loop = ACC_Loop_Scheduling_Transformation_Gpu (tree, wn_replacement_block);
	
	acc_check_for_duplicates(wn_translated_loop, "before localize");
	ACC_Walk_and_Localize(wn_translated_loop);	
	acc_check_for_duplicates(wn_translated_loop, "after localize");
	WN_verifier(wn_translated_loop);
	WN_INSERT_BlockLast ( wn_replacement_block, wn_translated_loop);
	ACC_Gen_Scalar_Variables_CopyOut_InOffload(wn_replacement_block);
	///////////////////////////////////////////////////////////////////////////	
   	WN_INSERT_BlockLast ( acc_parallel_func, wn_replacement_block );
	/* Transfer any mappings for nodes moved from parent to parallel function */
	ACC_Transfer_Maps ( acc_pmaptab, acc_cmaptab, acc_parallel_func, 
		  PU_Info_regions_ptr(Current_PU_Info) );

	/* Create a new dependence graph for the child  and move all the 
	 appropriate vertices from the parent to the child graph */

	//Current_Map_Tab = acc_cmaptab;
	//ACC_Fix_Dependence_Graph ( acc_ppuinfo, Current_PU_Info, acc_parallel_func ); 
	//Current_Map_Tab = acc_pmaptab;

  
	/* Restore parent information. */

	CURRENT_SYMTAB = acc_psymtab;
	Current_PU_Info = acc_ppuinfo;
	Current_pu = &Current_PU_Info_pu();
	Current_Map_Tab = acc_pmaptab;
	//ACC_Pop_Some_Globals( );
}

/*Including kernel construct region*/
void Transform_ACC_Kernel_Block_New ( WN * tree, WN* wn_replace_block)
{
	INT32 i;
	WN *wn;
	WN *wn2;
	WN *wn3;
	WN *wn4;
	WN *cur_node;
	WN *prev_node;
	WN *next_node;
	WN *sp_block;
	ST *lock_st;

	INT32 num_criticals;
	BOOL is_omp, is_region;
	WN_PRAGMA_ID cur_id, end_id;
	INT32 gate_construct_num;
	int iKernelsCount = 0;
	WN* wn_prehandblock = NULL;
	
	//kernel_parameters_count = acc_kernelLaunchParamList.size();
	//this function should be called before ACC_Create_Outline_OffloadRegion.
	acc_reduction_tab_map.clear();

	for (cur_node = WN_first(tree); cur_node; cur_node = next_node) 
	{

		prev_node = WN_prev(cur_node);
		next_node = WN_next(cur_node);

		/////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////
		//Process the loop region which is inside the kernel region
		if ((is_region = (WN_opcode(cur_node) == OPC_REGION &&
                             WN_first(WN_region_pragmas(cur_node)) &&
                             WN_opcode(WN_first(
			        WN_region_pragmas(cur_node))) == OPC_PRAGMA) ) &&
	       WN_pragma(WN_first(WN_region_pragmas(cur_node))) ==
						WN_PRAGMA_ACC_LOOP_BEGIN) 
		{
			WN* toplogy = Gen_Set_Default_Toplogy();
			WN_INSERT_BlockLast(wn_replace_block, toplogy);
			//++num_constructs;
			//generate kernel function
			//This should be the first ACC LOOP
			//if there is some statements before loop,
			//all of them are translated into another offload kernel
			//which will be executed by one gang in which only one vector is active.
			if(wn_prehandblock != NULL)
			{
				ACC_Setup_GPU_toplogy_1block_1thread(wn_replace_block);
				iKernelsCount ++;
				ACC_Linear_Stmts_Transformation_New(wn_prehandblock, wn_replace_block);
				if(acc_target_arch == ACC_ARCH_TYPE_APU)
					ACC_Launch_HSA_Kernel(iKernelsCount-1, wn_replace_block);
				else
					ACC_LaunchKernel_nvidia(iKernelsCount-1, wn_replace_block, FALSE);
				//LaunchKernel(iKernelsCount-1, wn_replace_block, FALSE);
				wn_prehandblock = NULL;
			}
			iKernelsCount ++;
			//remove the acc sequential loop first
			cur_node = ACC_Walk_and_Replace_ACC_Loop_Seq(cur_node);
			Identify_Loop_Scheduling(cur_node, wn_replace_block, TRUE);
			//Setup gangs and vectors before kernel launch
			//ACC_Setup_GPU_toplogy_for_Kernels(wn_replace_block);
			
			////////////////////////////////////////////////
			//if(wn_prehandblock)
			//	acc_loopinfo.wn_prehand_nodes = wn_prehandblock;
			//wn_prehandblock = NULL;
			
			kernel_tmp_licm_count = 0;
			ACC_Transform_Single_Nested_Loop_for_Kernels(cur_node);
			//launch kernels
			if(acc_target_arch == ACC_ARCH_TYPE_APU)
				ACC_Launch_HSA_Kernel(iKernelsCount-1, wn_replace_block);
			else
				ACC_LaunchKernel_nvidia(iKernelsCount-1, wn_replace_block, FALSE);
			//LaunchKernel(iKernelsCount-1, wn_replace_block, FALSE);

			WN_Delete ( WN_region_pragmas(cur_node) );
			WN_DELETE_Tree ( WN_region_exits(cur_node) );
			RID_Delete ( Current_Map_Tab, cur_node );
			WN_Delete ( cur_node );
			//WN_Delete ( wn );
    	} 
		else //non-loop stmts in the kernels
		{
			if(wn_prehandblock == NULL)
			{
				wn_prehandblock = WN_CreateBlock();
			}
			WN_INSERT_BlockLast(wn_prehandblock, WN_COPY_Tree(cur_node));
			WN_Delete ( cur_node );
		}
	}

	//they becomes an kernel which will be executed as 1 gang with 1 vector
	if(wn_prehandblock != NULL)
	{
		ACC_Setup_GPU_toplogy_1block_1thread(wn_replace_block);
		iKernelsCount ++;
		ACC_Linear_Stmts_Transformation_New(wn_prehandblock, wn_replace_block);
		if(acc_target_arch == ACC_ARCH_TYPE_APU)
			ACC_Launch_HSA_Kernel(iKernelsCount-1, wn_replace_block);
		else
			ACC_LaunchKernel_nvidia(iKernelsCount-1, wn_replace_block, FALSE);
		//LaunchKernel(iKernelsCount-1, wn_replace_block, FALSE);
		wn_prehandblock = NULL;
	}
}


