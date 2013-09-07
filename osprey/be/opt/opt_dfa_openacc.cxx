/***********************************************************************************************************/
/***********************************************************************************************************/
/***********************************************************************************************************/
/***********************************************************************************************************/
/***********************************************************************************************************/
/*
Data flow analysis for OpenACC offload region which require compiler to identify the private/in/out/inout variables.
Author: Xiaonan(Daniel) Tian, Computer Science Department of University of Houston
Email: xtian2@uh.edu
Copyright Reserved  by the author
*/
/***********************************************************************************************************/
/***********************************************************************************************************/
/***********************************************************************************************************/
/***********************************************************************************************************/

#ifdef USE_PCH
#include "opt_pch.h"
#endif // USE_PCH
#pragma hdrstop

#include <stdint.h>
#define USE_STANDARD_TYPES
#include <alloca.h>
#include "pu_info.h"		/* for PU_Info_state and related things */

#include "unistd.h"

#include "defs.h"
#include "erbe.h"		/* Error messages		*/
#include "glob.h"		/* for Feedback_File_Name	*/
#include "config_targ.h"
#include "config_ipa.h"         /* IPA_Enable_Alias_Class       */
#include "config.h"		/* Query_Skiplist		*/
#include "wn.h"
#include "wn_tree_util.h"
#include "wn_simp.h"
#include "wn_lower.h"
// Remove the following line
#include "wn_util.h"

#include "ir_reader.h"
#include "tracing.h"
#include "be_util.h"
#include "region_util.h"
#include "fb_whirl.h"		// for FB_Annotate_whirl

#include "optimizer.h"
#include "opt_alias_class.h"
#include "opt_cfg.h"
#include "opt_main.h"
#include "opt_fb.h"
#include "opt_exc.h"
#include "opt_sym.h"
#include "opt_ssa.h"
#include "opt_emit.h"
#include "opt_du.h"

#include "opt_dbg.h"
#include "opt_goto.h"
#include "opt_rvi.h"
#include "opt_util.h"
#include "opt_alias_mgr.h"
#include "opt_alias_interface.h"	/* for Verify_alias() */
#include "opt_vn.h"                     /* Global value numbering (gvn) */

#include "config_lno.h"
#include "config_opt.h"			// for Delay_U64_Lowering

#include "dep_graph.h"			/* for tracing Current_Dep_Graph */
#include "wb_ipl.h"			/* whirl browser for ipl */ 
#include "opt_du.h"

#ifndef __MINGW32__
#include "regex.h"                      // For regcomp and regexec
#endif /* __MINGW32__ */


#include "xstats.h"                     // For PU_WN_BB_Cnt
#include "opt_wovp.h"     // for write once variable promotion
#include "opt_misc.h"
#include "opt_lmv.h"
#include "opt_peel_unroll.h"

#if defined(TARG_SL)
#include "opt_lclsc.h"
#endif

#include "wssa_utils.h"   // WHIRL SSA
#include <vector>
#include <map>
using namespace std;
//#include "opt_emit_template_openacc.h"

//#include "../opt/init.cxx"          // force include of Wopt_Initializer

static CFG *g_cfg = NULL;
extern BOOL run_autoConstCacheOpt;

static void DFA_BB_Traverse(BB_NODE *bb);
static const char *BB_kind_name[] = {
  "UNKNOWN",
  "GOTO",
  "LOGIF",
  "VARGOTO",
  "ENTRY",
  "EXIT",
  "DOSTART",
  "DOEND",
  "DOSTEP",
  "DOHEAD",
  "DOTAIL",
  "IO",
  "WHILEEND",
  "REGIONSTART",
  "REGIONEXIT",
  "REPEATBODY",
  "REPEATEND",
  "SUMMARY"
};

// data structure for definitions table
typedef struct DEF {
    size_t     def_idx;
    WN        *def_wn;
    size_t     def_num;
    ST        *var_st;
    DEF       *next;
} DEF_T;

static int num_defs = 0;
static map<size_t, DEF_T*> acc_def_table;

// data structure for variables table
typedef struct {
    size_t  var_idx;
    ST      *var_st;
    DEF_T   *first_def;
    DEF_T   *last_def;
} VAR_T;

static size_t num_vars = 0;
static map<size_t, VAR_T*> acc_var_table;

// hash map for indexing into VAR table using ST *
static map<ST*, size_t> acc_vars;
static map<ST*, size_t> acc_prescan_vars;
static size_t	acc_total_num_vars;

//static map<UINT32,UINT32> acc_label_bbid;	// region exit belongs to which bbid
//static map<UINT32,UINT32> acc_start_end_bbid;//for parallel/kenerls region

// current output file for WHIRL DFA
static FILE *curr_output_fp = stdout;

//----------------------------------------------------------------------
// WHIRL annotation structures

// annotate for statement definition
typedef struct STMT_DEF {
    size_t def_idx;
} STMT_DEF_T;

static WN_MAP WDFA_Def_Map;

// annotate for statement "may-defs"
typedef struct STMT_MAYDEFS {
    vector<size_t> maydef_list;
} STMT_MAYDEFS_T;

static WN_MAP WDFA_MayDefs_Map;

// annotate for statement uses
typedef struct STMT_USES {
    vector<size_t> use_list;
} STMT_USES_T;

static WN_MAP WDFA_Uses_Map;

// annotate for statement "may-uses"
typedef struct STMT_MAYUSES {
    vector<size_t> mayuse_list;
} STMT_MAYUSES_T;

static WN_MAP WDFA_MayUses_Map;
static WN_MAP WDFA_Parent_Map;

static MEM_POOL wdfa_pool;
static MEM_POOL Wdfa_Local_Pool;

//----------------------------------------------------------------------

//----------------------------------------------------------------------
// WHIRL annotation structures

typedef struct BB_DATAFLOW_INFO {
    vector<bool> gen;
    vector<bool> kill;
    vector<bool> in;
    vector<bool> out;
} BB_DATAFLOW_INFO_T;

static map<INT32, BB_DATAFLOW_INFO_T*>  acc_dataflow_info;

//This is for parallel/kernels regions
typedef struct BB_DATAFLOW_ACC_REGION_INFO
{
    vector<bool> used;
    vector<bool> changed;
    vector<bool> scalar_private;
    vector<bool> parameters;
    vector<bool> first_private;	//in
    vector<bool> last_private;	//out    
    vector<bool> inout;			//shared
    vector<bool> array_pointer;	//pointer or array
    vector<bool> global;
    vector<bool> write_ptr;
    vector<bool> read_ptr;
    vector<bool> readonly_ptr;
	//this is used for scalar replacement
    vector<bool> readonly_scalar;
}BB_DATAFLOW_ACC_REGION_INFO;
//////////////////////////////////////////////////////////////
static map<UINT32, BB_DATAFLOW_ACC_REGION_INFO*> acc_used_data_classify;

//static functions
static void Print_Def_and_Var_Tables();
static void print_sets(vector<bool>* pBitvector);

/***********************************************************************
 * Local macros
 ***********************************************************************/

#define NEW_WDFA(x) CXX_NEW(x,&wdfa_pool);
#define NEW_Local_WDFA(x) CXX_NEW(x, &Wdfa_Local_Pool);

#define DELETE_WDFA(x) CXX_DELETE(x,&wdfa_pool);
#define DELETE_Local_WDFA(x) CXX_DELETE(x, &Wdfa_Local_Pool);

#define Set_Def(wn,d)  (WN_MAP_Set(WDFA_Def_Map, wn, (STMT_DEF_T*) d))
#define Get_Def(wn)    ((STMT_DEF_T*) WN_MAP_Get(WDFA_Def_Map, (WN*) wn))

#define Set_MayDefs(wn,d)  (WN_MAP_Set(WDFA_MayDefs_Map, wn, (STMT_MAYDEFS_T*) d))
#define Get_MayDefs(wn)    ((STMT_MAYDEFS_T*) WN_MAP_Get(WDFA_MayDefs_Map, (WN*) wn))

#define Set_Uses(wn,u)  (WN_MAP_Set(WDFA_Uses_Map, wn, (STMT_USES_T*) u))
#define Get_Uses(wn)    ((STMT_USES_T*) WN_MAP_Get(WDFA_Uses_Map, (WN*) wn))

#define Set_MayUses(wn,u)  (WN_MAP_Set(WDFA_MayUses_Map, wn, (STMT_MAYUSES_T*) u))
#define Get_MayUses(wn)    ((STMT_MAYUSES_T*) WN_MAP_Get(WDFA_MayUses_Map, (WN*) wn))


#define Set_Parent(wn,u)  (WN_MAP_Set(WDFA_Parent_Map, wn, (WN*) u))
#define Get_Parent(wn)    ((WN*) WN_MAP_Get(WDFA_Parent_Map, (WN*) wn))



/**
Initialize the basic resource for OpenACC DFA
**/
void init_dfa_openacc()
{
    MEM_POOL_Initialize(&wdfa_pool, "WHIRL DFA Pool", FALSE);
    MEM_POOL_Initialize(&Wdfa_Local_Pool, "WHIRL DFA Local Pool", FALSE);

    // initialize WN maps
    WDFA_Def_Map    = WN_MAP_Create(&wdfa_pool);
    WDFA_MayDefs_Map = WN_MAP_Create(&wdfa_pool);
    WDFA_Uses_Map    = WN_MAP_Create(&wdfa_pool);
    WDFA_MayUses_Map = WN_MAP_Create(&wdfa_pool);
    WDFA_Parent_Map = WN_MAP_Create(&wdfa_pool);
	//acc_label_bbid.clear();	// region exit belongs to which bbid
    //acc_start_end_bbid.clear();
	num_vars = 0;
	num_defs = 0;
}

void free_dfa_resource_openacc()
{
	int i=0;
	//release def table 
	//for(i=0; i<acc_def_table.size(); i++)
	//{
	//	delete acc_def_table[i];
	//}
	acc_def_table.clear();
	//release var table
	//for(i=0; i<acc_var_table.size(); i++)
	//{
	//	delete acc_var_table[i];
	//}
	acc_var_table.clear();
        acc_vars.clear();
	acc_prescan_vars.clear();
	//acc_label_bbid.clear();	// region exit belongs to which bbid
    //acc_start_end_bbid.clear();
	acc_used_data_classify.clear();
	acc_dataflow_info.clear();
    // delete WN maps
    WN_MAP_Delete(WDFA_Def_Map);
    WN_MAP_Delete(WDFA_MayDefs_Map);
    WN_MAP_Delete(WDFA_Uses_Map);
    WN_MAP_Delete(WDFA_MayUses_Map);
    WN_MAP_Delete(WDFA_Parent_Map);

    MEM_POOL_Delete(&wdfa_pool);
    MEM_POOL_Delete(&Wdfa_Local_Pool);
}

/*
removed unecessary variable in liveness analysis.
for example, .PREG, compiler generated temp variables, unused data, and const string.
*/
static BOOL dfa_scan_is_uncounted_var(ST* st_var)
{
    if(ST_sym_class(st_var) == CLASS_PREG && ST_sclass(st_var)==SCLASS_REG)
		return TRUE;
	//if(ST_is_temp_var(st_var))
	//	return TRUE;
	if(ST_is_not_used(st_var))
		return TRUE;
	//to avoid string
	//if((ST_sym_class(st_var) == CLASS_CONST) && (ST_sclass(st_var)==SCLASS_FSTATIC)
	//	&& (ST_is_initialized(st_var)) && (TY_kind(ST_type(st_var))==KIND_ARRAY))
	//	return TRUE;
	return FALSE;
}
/*
Global variable 
*/
static BOOL dfa_scan_is_global_var(ST* st_var)
{
    if(ST_sclass(st_var)==SCLASS_FSTATIC
		|| ST_sclass(st_var)==SCLASS_EXTERN
		|| ST_sclass(st_var)==SCLASS_UGLOBAL
		|| ST_sclass(st_var)==SCLASS_DGLOBAL)
		return TRUE;
	return FALSE;
}

//=========================================================================
//
// Function: add_def
// Input:
//    stmt: a WHIRL statement which defines some variable
//    var: the ST* for a variable defined by a statement
//
// Description: Creates a new entry in the DEF table for the defined variable,
// updating DEF and VAR tables accordingly. Also, annotate the statement with
// the def idx for the newly added DEF entry.
//
//acc_var_tab: how many different var, there will be how many entries in acc_var_tab.
//=========================================================================
static void add_def( WN *stmt, ST *var)
{
    size_t var_idx;
    size_t def_idx;

    // currently only handles local variables
    //if (!Is_Local_Symbol(var)) return;
    if(dfa_scan_is_uncounted_var(var))
		return;

    if (acc_vars.find(var) == acc_vars.end()) 
	{
        // encountered variable first time, so initialize entry in
        // VAR table
        var_idx = num_vars;
        acc_vars[var] = var_idx;

		VAR_T* pnew_var = NEW_WDFA(VAR_T);
		acc_var_table[var_idx] = pnew_var;
			
        acc_var_table[var_idx]->var_idx = var_idx;
        acc_var_table[var_idx]->var_st = var;

        num_vars++;

        // add "default" definition
        size_t def_idx = num_defs;
		DEF_T* pnew_def = NEW_WDFA(DEF_T);
		acc_def_table[def_idx] = pnew_def;
		
        acc_def_table[def_idx]->def_idx = def_idx;
        acc_def_table[def_idx]->def_wn = NULL;
        acc_def_table[def_idx]->def_num = 0;
        acc_def_table[def_idx]->var_st = var;
        acc_def_table[def_idx]->next = NULL;
        num_defs++;

        acc_var_table[var_idx]->first_def = acc_def_table[def_idx];
        acc_var_table[var_idx]->last_def = acc_def_table[def_idx];
    } 
	else 
	{
        var_idx = acc_vars[var];
    }

    def_idx = num_defs;
	DEF_T* pnew_def = NEW_WDFA(DEF_T);
	acc_def_table[def_idx] = pnew_def;
    // update DEF table
    acc_def_table[def_idx]->def_idx = def_idx;
    acc_def_table[def_idx]->def_wn = stmt;
    acc_def_table[def_idx]->var_st = var;
    acc_def_table[def_idx]->next = NULL;
    num_defs++;

    // update last def pointers in VAR table
    if (acc_var_table[var_idx]->last_def) 
	{
        // update next pointer for previous definition of this
        // variable.
        (acc_var_table[var_idx]->last_def)->next =
            acc_def_table[def_idx];

        // set new def_num
        acc_def_table[def_idx]->def_num =
            acc_var_table[var_idx]->last_def->def_num + 1;
    }
    acc_var_table[var_idx]->last_def = acc_def_table[def_idx];

    // annotate WHIRL node with def_idx
    STMT_DEF_T *stmt_def_info = NEW_WDFA(STMT_DEF_T);
    stmt_def_info->def_idx = def_idx;
    Set_Def(stmt, stmt_def_info);
}

//=========================================================================
//
// Function: add_use
// Input:
//    stmt: a WHIRL statement which uses some variable
//    var: the ST* for a variable used by a statement
//
// Description: If var is a newly encountered variable, create new entry for
// it in the VAR table and DEF table. Annotate the statement by adding var
// idx to its use list.
//
//=========================================================================
static void add_use( WN *stmt, ST *var)
{
    size_t var_idx;
    size_t def_idx;

    // currently only handles local variables
    //if (!Is_Local_Symbol(var)) return;
    if(dfa_scan_is_uncounted_var(var))
		return;

    // update VAR table
    if (acc_vars.find(var) == acc_vars.end()) 
	{
        // encountered variable first time, so initialize entry in
        // VAR table
        var_idx = num_vars;
        acc_vars[var] = var_idx;

		VAR_T* pnew_var = NEW_WDFA(VAR_T);
		acc_var_table[var_idx] = pnew_var;

        acc_var_table[var_idx]->var_idx = var_idx;
        acc_var_table[var_idx]->var_st = var;

        num_vars++;
		
		DEF_T* pnew_def = NEW_WDFA(DEF_T);

        // add "default" definition
        size_t def_idx = num_defs;
		acc_def_table[def_idx] = pnew_def;
        acc_def_table[def_idx]->def_idx = def_idx;
        acc_def_table[def_idx]->def_wn = NULL;
        acc_def_table[def_idx]->var_st = var;
        acc_def_table[def_idx]->next = NULL;
        num_defs++;

        acc_var_table[var_idx]->first_def = acc_def_table[def_idx];
        acc_var_table[var_idx]->last_def = acc_def_table[def_idx];
    }

    var_idx = acc_vars[var];

    // annotate WHIRL node with var_idx张择

    STMT_USES_T *stmt_uses_info = Get_Uses(stmt);

    if (stmt_uses_info == NULL) 
	{
        stmt_uses_info = NEW_WDFA(STMT_USES_T);
        stmt_uses_info->use_list.clear();
    }

    stmt_uses_info->use_list.push_back(var_idx);

    Set_Uses(stmt, stmt_uses_info);
}

/**
any other statement
**/
static void update_tables_from_other_stmt(WN *wn_stmt)
{	
	WN_TREE_ITER<POST_ORDER> tree_iter(wn_stmt);
	WN* wn;
	if((WN_operator(wn_stmt)==OPR_PRAGMA) 
			|| (WN_operator(wn_stmt)==OPR_XPRAGMA)
			|| (WN_operator(wn_stmt)==OPR_REGION_EXIT)
			|| (WN_operator(wn_stmt)==OPR_LABEL))
		return;
    // update VAR table by visiting every WN in stmt tree and recording
    // variable uses
    while (tree_iter != LAST_POST_ORDER_ITER)
	{
        wn = tree_iter.Wn();
        switch (WN_operator(wn)) 
		{
            case OPR_LDID:
			case OPR_LDA:
                ST *used_var = WN_st(wn);
                add_use(wn_stmt, used_var);
                break;
        }
		tree_iter ++;
    }
	/*while (tree_iter != LAST_PRE_ORDER_ITER)
	{
		fdump_tree(stdout, tree_iter.Wn());
		tree_iter ++;
	}*/
}


/**
assignment statement
**/
static void update_tables_from_asg_stmt(WN *wn_stmt)
{	
	WN_TREE_ITER<POST_ORDER> tree_iter(wn_stmt);
	WN* wn;
    if (!OPERATOR_is_store( WN_operator(wn_stmt)))
        return;
	
    // update VAR table by visiting every WN in stmt tree and recording
    // variable uses
    while (tree_iter != LAST_POST_ORDER_ITER)
	{
        wn = tree_iter.Wn();
        switch (WN_operator(wn)) 
		{
            case OPR_LDID:
			case OPR_LDA:
                ST *used_var = WN_st(wn);
                add_use(wn_stmt, used_var);
                break;
        }
		tree_iter ++;
    }

    // update DEF table
    switch (WN_operator(wn_stmt)) 
	{
        case OPR_STID:
              ST *defined_var = WN_st(wn_stmt);
              add_def(wn_stmt, defined_var);
              break;
    }
	/*while (tree_iter != LAST_PRE_ORDER_ITER)
	{
		fdump_tree(stdout, tree_iter.Wn());
		tree_iter ++;
	}*/
}


/**
flow control statement
**/
static void update_tables_from_cf_stmt(WN *wn_stmt)
{	
	WN_TREE_ITER<POST_ORDER> tree_iter(wn_stmt);
	WN* wn;
	while (tree_iter != LAST_POST_ORDER_ITER)
	{
        wn = tree_iter.Wn();
        switch (WN_operator(wn)) {
            case OPR_LDID:
            case OPR_LDA:
                ST *used_var = WN_st(wn);
                add_use(wn_stmt, used_var);
                break;
        }
		//fdump_tree(stdout, tree_iter.Wn());
		tree_iter ++;
	}
}

/**
call statement
**/
static void update_tables_from_call_stmt(WN *wn_stmt)
{	
	WN_TREE_ITER<POST_ORDER> tree_iter(wn_stmt);
	WN* wn;
	while (tree_iter != LAST_POST_ORDER_ITER)
	{
        wn = tree_iter.Wn();
        switch (WN_operator(wn)) 
		{
            case OPR_LDID:
            case OPR_LDA:
                ST *used_var = WN_st(wn);
                add_use(wn_stmt, used_var);
                break;
        }
		//fdump_tree(stdout, tree_iter.Wn());
		tree_iter ++;
	}
}

//=========================================================================
//
// Function: is_assignment_stmt
// Input:
//  stmt: WHIRL statement
//
// Description: returns TRUE if stmt is an assignment statement
//
//=========================================================================
static BOOL is_assignment_stmt(WN *stmt)
{
    return OPERATOR_is_store( WN_operator(stmt) );
}

//=========================================================================
//
// Function: is_control_flow_stmt
// Input:
//  stmt: WHIRL statement
//
// Description: Returns TRUE if it is a basic block ending control flow
// statement
//
//=========================================================================
static BOOL is_control_flow_stmt(WN *stmt)
{
    BOOL is_cf = FALSE;;

    switch (WN_operator(stmt)) {
      case OPR_DO_LOOP:
      case OPR_DO_WHILE:
      case OPR_WHILE_DO:
      case OPR_IF:
      case OPR_GOTO:
      case OPR_SWITCH:
      case OPR_COMPGOTO:
      case OPR_AGOTO:
      case OPR_TRUEBR:
      case OPR_FALSEBR:
      case OPR_RETURN:
      case OPR_RETURN_VAL:
          is_cf = TRUE;
    }

    return is_cf;
}

//=========================================================================
//
// Function: is_call_stmt
// Input:
//  stmt: WHIRL statement
//
// Description: Returns TRUE if it is a call statement.
//
//=========================================================================
static BOOL is_call_stmt(WN *stmt)
{
    return OPERATOR_is_call( WN_operator(stmt) );
}

static void padding_bitvector_if_necessary(INT32 bb_id, INT32 cur_bit_pos)
{
	BB_DATAFLOW_INFO_T *bb_info = acc_dataflow_info[bb_id];
	INT32 cur_size = bb_info->gen.size();
	if(cur_size >= (cur_bit_pos))
		return;
	else if(cur_size < (cur_bit_pos))
	{
		for(INT32 bit_index = cur_size; bit_index < (cur_bit_pos); bit_index++)
		{
			bb_info->gen.push_back(FALSE);
			bb_info->kill.push_back(FALSE);
			bb_info->in.push_back(FALSE);
			bb_info->out.push_back(FALSE);
		}
	}
}
//=========================================================================
//
// Function: add_bb_info_from_stmt
// Input:
//    stmt : a statement WHIRL node
//    bb_id: a numeric identifer for a basic block (>= 1)
//    wdfa_type: type of data flow analysis we are doing
//
// Description: Processes statement node stmt, and incorporates its data flow
// information to the basic block given by bb_id
//
//=========================================================================
static void add_bb_info_from_stmt(INT32 bb_id, WN *stmt)
{
/*#ifdef DEBUG_WDFA_PRINT
    print_stmt(stmt);
    print_def(stmt);
    print_uses(stmt);
#endif*/

    Is_True(acc_dataflow_info.find(bb_id) != acc_dataflow_info.end(),
            ("data flow info record for basic block not yet created"));

    STMT_DEF_T *stmt_def  = Get_Def(stmt);
    STMT_USES_T *stmt_uses = Get_Uses(stmt);
    BB_DATAFLOW_INFO_T *bb_info = acc_dataflow_info[bb_id];

    //switch (wdfa_type)
    {
        /*case WDFA_REACHING_DEF:
            if (stmt_def != NULL) {
              size_t def_idx = stmt_def->def_idx;
              ST *var_st = def_table[def_idx].var_st;
              size_t var_idx = vars[var_st];

              for (DEF_T *d = var_table[var_idx].first_def;
                   d != NULL;
                   d = d->next) {
                // GEN: unset any prior generated definitions to this variable
                bb_info->gen.reset(d->def_idx);

                // KILL: set all definitions to this variable
                bb_info->kill.set(d->def_idx);
              }

              // GEN: set the definition generated here
              bb_info->gen.set(def_idx);
            }
            break;*/

        //case WDFA_LIVE_VAR:
            // definition would occur after the uses, so we look at it first
            if (stmt_def != NULL) 
			{
				size_t def_idx = stmt_def->def_idx;
				ST *var_st = acc_def_table[def_idx]->var_st;
				size_t var_idx = acc_vars[var_st];

				// GEN: unset the variable who's liveness is blocked by this
				// definition
				bb_info->gen[var_idx] = 0;//.reset(var_idx);

				// KILL: set the variable who's liveness is killed by this
				// definition
				bb_info->kill[var_idx] = 1; //.set(var_idx);
            }

            if (stmt_uses != NULL)
			{
                for (size_t i = 0; i < stmt_uses->use_list.size(); i++) 
				{
                    size_t use_idx = stmt_uses->use_list[i];

                    // GEN: set any variable that is used
                    bb_info->gen[use_idx] = 1; //.set(use_idx);
                }
            }
            //break;

        //default:
        //    break;
    }

}

//=========================================================================
//
// Function: Initialize_BB_DFA_LocalInfo
// Input:
//    bb_id: basic block ID
//    wdfa_type: data flow analysis type
//
// Description: This iterates through each statement in the basic block given
// by bb_id, performing a local data flow analysis
//
//=========================================================================
static void initialize_bb_dfa_localInfo(BB_NODE *bb)
{
    STMT_ITER wn_iter(bb->Firststmt(), bb->Laststmt());
	//////////////////////////////////////////////////////////////
	//go to the last stmt
	for (wn_iter.First(); !wn_iter.Is_Empty(); wn_iter.Next())
	{
		if(wn_iter.Cur()==NULL||wn_iter.Cur()==bb->Laststmt())
			break;
	}
	//////////////////////////////////////////////////////////////
    WN *wn_stmt;
	INT32 bb_id = bb->Id();
	for (wn_iter.Cur(); !wn_iter.Is_Empty(); wn_iter.Prev())
	{
		wn_stmt = wn_iter.Cur();
		add_bb_info_from_stmt(bb_id, wn_stmt);
	}
    // traverse through statements to compute GEN and KILL sets
    //wn_stmt = bb->Laststmt();
    //while (stmt != NULL) {
    //    add_bb_info_from_stmt(bb_id, stmt, wdfa_type);
    //    stmt = BB_next_stmt(node, stmt, wdfa_type);
    //}

    // print GEN and KILL sets
}


/**
Retrieve def/use information for each basic block
**/
void dfa_def_use_info_bb_openacc(BB_NODE *bb)
{
	STMT_ITER wn_iter(bb->Firststmt(), bb->Laststmt());
	//printf("BB KIND = %s \n", BB_kind_name[bb->Kind()]);
	//printf("Cur ID = %d \n", bb->Id());
    INT32 bbid = bb->Id();
    //fprintf(curr_output_fp, "BB %d, Kind=%s.\n", bbid, BB_kind_name[bb->Kind()]);
	//bb->Print_wn(stdout);
    for (wn_iter.First(); !wn_iter.Is_Empty(); wn_iter.Next())
	{
		WN* wn_stmt = wn_iter.Cur();
		//fdump_tree(stdout, wn_stmt);
    	if ( is_assignment_stmt(wn_stmt) )  
		{
            update_tables_from_asg_stmt(wn_stmt);
        } else if ( is_control_flow_stmt(wn_stmt) ) 
        {
            update_tables_from_cf_stmt(wn_stmt);
        } else if ( is_call_stmt(wn_stmt) ) 
        {
            update_tables_from_call_stmt(wn_stmt);
        }
		else
			update_tables_from_other_stmt(wn_stmt);
		//fdump_tree()
		//if(bbid == 11)
		//fdump_tree(stdout, wn_iter.Cur());	
	}
	
    // allocate bit vectors for GEN, KILL, IN, and OUT sets
    acc_dataflow_info[bbid] = NEW_WDFA(BB_DATAFLOW_INFO_T);
    BB_DATAFLOW_INFO_T *info = acc_dataflow_info[bbid];
	
	padding_bitvector_if_necessary(bbid, acc_total_num_vars);
	/*UINT32 isize_test1 = info->gen.size(); 
	UINT32 isize_test2 = info->kill.size();
	UINT32 isize_test3 = info->in.size();
	UINT32 isize_test4 = info->out.size();
	if(isize_test3 != acc_total_num_vars)
	{	
		printf("Error1\n");
	}*/
	//Generate Local Information
	initialize_bb_dfa_localInfo(bb);
    //fprintf(curr_output_fp, "GEN: ");
	//print_sets(&acc_dataflow_info[bbid]->gen);
    //   fprintf(curr_output_fp, "KILL: ");
	//print_sets(&acc_dataflow_info[bbid]->kill);
}

void dfa_scan_all_var_st(WN* entry)
{
	WN_TREE_ITER<POST_ORDER> tree_iter(entry);
	WN* wn;
	ST *defined_or_used_var = NULL;
	acc_total_num_vars = 0;
	
	while (tree_iter != LAST_POST_ORDER_ITER)
	{
		wn = tree_iter.Wn();
        switch (WN_operator(wn)) 
		{
            case OPR_LDID:
			case OPR_LDA:
			case OPR_STID:				
                defined_or_used_var = WN_st(wn);
			    if(dfa_scan_is_uncounted_var(defined_or_used_var))
					break;
				//to avoid string
				//if((ST_sym_class(defined_or_used_var) == CLASS_CONST) && (ST_sclass(defined_or_used_var)==SCLASS_FSTATIC)
				//	&& (ST_is_initialized(defined_or_used_var)) && (TY_kind(ST_type(defined_or_used_var))==KIND_ARRAY))
				//	break;
    			if (acc_prescan_vars.find(defined_or_used_var) == acc_prescan_vars.end())
				{
					acc_prescan_vars[defined_or_used_var] = acc_total_num_vars;
					acc_total_num_vars ++;
				}
                break;
        }
		tree_iter ++;
	}
	
}

void dfa_attach_pragma_tree_openacc()
{
}


//=========================================================================
//
// Function: set_col
// Input:
//  s1 : destination string
//  s2 : source string
//  w  : total width for padding
//
// Description: Copies s2 into s1, with sufficient padding to make the new
// length w.
//
//=========================================================================
static inline void set_col( char *s1, const char *s2, size_t w)
{
    strncpy(s1, s2, w);

    for (size_t i = strlen(s1); i < w-1; i++) {
      s1[i] = ' ';
    }
    s1[w-1] = '\0';
}


//=========================================================================
//
// Function: Print_Def_and_Var_Tables
// Input:
//
// Description: Print contents of the DEF and VAR tables.
//
//=========================================================================
static void Print_Def_and_Var_Tables()
{
    const int width = 20;
    char c1[width], c2[width], c3[width], c4[width], c5[width], tmp[width];
    char bar[width*6];

    memset(bar, 0, width*6);

    set_col(c1, "index", width);
    set_col(c2, "WN*",   width);
    set_col(c3, "var",   width);
    set_col(c4, "next", width);
    strncpy(c5, "def_num", width);

    memset(bar, '-', 5+strlen(c1)+strlen(c2)+strlen(c3)+strlen(c4)+strlen(c5));
    fprintf(curr_output_fp, "%s\n", bar);
    fprintf(curr_output_fp, "DEF TABLE\n");
    fprintf(curr_output_fp, "%s\n", bar);
    fprintf(curr_output_fp, "%s %s %s %s %s\n", c1, c2, c3, c4, c5);
    fprintf(curr_output_fp, "%s\n", bar);

    for (size_t i = 0; i < num_defs; i++) {
        snprintf(tmp, width, "%lu", (unsigned long)acc_def_table[i]->def_idx);
        set_col(c1, tmp, width);
        snprintf(tmp, width, "%p", acc_def_table[i]->def_wn);
        set_col(c2, tmp, width);
        snprintf(tmp, width, "%s", ST_name(acc_def_table[i]->var_st));
        set_col(c3, tmp, width);
        snprintf(tmp, width, "%d",
                 (acc_def_table[i]->next) ?  acc_def_table[i]->next->def_idx : -1);
        set_col(c4, tmp, width);
        snprintf(c5, width, "%d", acc_def_table[i]->def_num);

        fprintf(curr_output_fp, "%s %s %s %s %s\n", c1, c2, c3, c4, c5);
    }

    fprintf(curr_output_fp, "%s\n\n\n", bar);

    memset(bar, 0, width*4);
    set_col(c1, "index", width);
    set_col(c2, "var",   width);
    set_col(c3, "first", width);
    strncpy(c4, "last", width);

    memset(bar, '-', 5+strlen(c1)+strlen(c2)+strlen(c3)+strlen(c4));
    fprintf(curr_output_fp, "%s\n", bar);
    fprintf(curr_output_fp, "VAR TABLE\n");
    fprintf(curr_output_fp, "%s\n", bar);
    fprintf(curr_output_fp, "%s %s %s %s\n", c1, c2, c3, c4);
    fprintf(curr_output_fp, "%s\n", bar);
    for (size_t i = 0; i < num_vars; i++) {
        snprintf(tmp, width, "%lu", (unsigned long)acc_var_table[i]->var_idx);
        set_col(c1, tmp, width);
        snprintf(tmp, width, "%s", ST_name(acc_var_table[i]->var_st));
        set_col(c2, tmp, width);
        snprintf(tmp, width, "%d", (acc_var_table[i]->first_def) ?
                 acc_var_table[i]->first_def->def_idx : -1);
        set_col(c3, tmp, width);
        snprintf(c4, width, "%d", (acc_var_table[i]->last_def)  ?
                 acc_var_table[i]->last_def->def_idx : -1);

        fprintf(curr_output_fp, "%s %s %s %s\n", c1, c2, c3, c4);
    }

    fprintf(curr_output_fp, "%s\n\n\n", bar);
}


static void dfa_reset_bitvector(vector<bool>* pbitvector)
{
	UINT32 i;
	for(i=0; i<pbitvector->size(); i++)
	{
		(*pbitvector)[i] = FALSE;
	}
}

static vector<bool> dfa_or_operation_bitvector(vector<bool>* pbitvector1,
											vector<bool>* pbitvector2)
{
	UINT32 i;
	vector<bool> bv_results = *pbitvector1;
	UINT32 isize1= pbitvector1->size();
	UINT32 isize2= pbitvector2->size();
	for(i=0; i<pbitvector1->size(); i++)
	{
		bool bt1 = (*pbitvector1)[i];
		bool bt2 = (*pbitvector2)[i];
		bool bt3 = bt1 | bt2;
		bv_results[i] =  bt3;
	}
	return bv_results;	
}

static vector<bool> dfa_and_operation_bitvector(vector<bool>* pbitvector1,
											vector<bool>* pbitvector2)
{
	UINT32 i;
	vector<bool> bv_results = *pbitvector1;
	for(i=0; i<pbitvector1->size(); i++)
	{
		bv_results[i] = (*pbitvector1)[i] & (*pbitvector2)[i] ;
	}
	return bv_results;	
}

//~, bitwise NOT 
static vector<bool> dfa_not_operation_bitvector(vector<bool>* pbitvector1)
{
	UINT32 i;
	vector<bool> bv_results = *pbitvector1;
	bool intermediate = 0;
	for(i=0; i<pbitvector1->size(); i++)
	{
		//intermediate = (*pbitvector1)[i] ;
		//intermediate = ~intermediate ;
		bv_results[i] = (*pbitvector1)[i] ? 0:1;
	}
	return bv_results;	
}


static BOOL dfa_is_same_operation_bitvector(vector<bool>* pbitvector1,
											vector<bool>* pbitvector2)
{
	UINT32 i;
	vector<bool> bv_results = *pbitvector1;
	for(i=0; i<pbitvector1->size(); i++)
	{
		bv_results[i] = (*pbitvector1)[i] ^ (*pbitvector2)[i] ;
		if(bv_results[i])
			return FALSE;
	}
	return TRUE;	
}

//xor operation, it means sets minus operation.
static vector<bool> dfa_xor_operation_bitvector(vector<bool>* pbitvector1,
											vector<bool>* pbitvector2)
{
	UINT32 i;
	vector<bool> bv_results = *pbitvector1;
	for(i=0; i<pbitvector1->size(); i++)
	{
		bv_results[i] = (*pbitvector1)[i] ^ (*pbitvector2)[i] ;
	}
	return bv_results;	
}

static void print_sets(vector<bool>* pBitvector)
{
	size_t bit_len = acc_total_num_vars;
    char *bits;
    bits = (char *)malloc((bit_len+1) * sizeof(char));
    memset(bits, 0, bit_len+1);
	
    for (size_t i = 0; i < bit_len; i++) 
	{
        if ((*pBitvector)[i]) 
		{
            bits[i] = '1';
            fprintf(curr_output_fp, " %s",
                    ST_name(acc_var_table[i]->var_st));
        } 
		else 
        {
            bits[i] = '0';
        }
    }
    //fprintf(curr_output_fp, " (%s)\n", bits);
    fprintf(curr_output_fp, "\n");

    free(bits);
}

static void print_sets_no_st(vector<bool>* pBitvector)
{
	size_t bit_len = acc_total_num_vars;
    char *bits;
    bits = (char *)malloc((bit_len+1) * sizeof(char));
    memset(bits, 0, bit_len+1);
	
    for (size_t i = 0; i < bit_len; i++) 
	{
        if ((*pBitvector)[i]) 
		{
            bits[i] = '1';
            //fprintf(curr_output_fp, " %s",
            //        ST_name(acc_var_table[i]->var_st));
        } 
		else 
        {
            bits[i] = '0';
        }
    }
    fprintf(curr_output_fp, " (%s)\n", bits);

    free(bits);
}

//=========================================================================
//
// Function: print_in_out_sets
// Input:
//    bb_id: a numeric identifer for a basic block (>= 1)
//    wdfa_type: type of data flow analysis we are doing
//
// Description: Prints the IN and OUT sets associated with the basic block
// given by bb_id
//
//=========================================================================
static void print_in_out_sets(INT32 bb_id)
{
    size_t bit_len = 0;
    char *bits;
    BB_DATAFLOW_INFO_T *info = acc_dataflow_info[bb_id];
    fprintf(curr_output_fp, "\n*************************\n");

	bit_len = acc_total_num_vars;

    bits = (char *)malloc((bit_len+1) * sizeof(char));
    memset(bits, 0, bit_len+1);

    // print IN set
    fprintf(curr_output_fp, "IN: ");
    for (size_t i = 0; i < bit_len; i++) {
        if (info->in[i]) {
            bits[i] = '1';
            fprintf(curr_output_fp, " %s",
                    ST_name(acc_var_table[i]->var_st));
        } else {
            bits[i] = '0';
        }
    }
    fprintf(curr_output_fp, " (%s)\n", bits);

    // print OUT set
    fprintf(curr_output_fp, "OUT: ");
    for (size_t i = 0; i < bit_len; i++) {
        if (info->out[i]) {
            bits[i] = '1';
            fprintf(curr_output_fp, " %s",
                    ST_name(acc_var_table[i]->var_st));
        } else {
            bits[i] = '0';
        }
    }
    fprintf(curr_output_fp, " (%s)\n", bits);

    free(bits);
	
    fprintf(curr_output_fp, "*************************\n\n");
}

static WN* DFA_create_acc_data_clause_for_offload_region(WN_PRAGMA_ID pragma_name, ST* st_var, WN* wn_replace_block)
{
	ST * st = NULL;
	WN * wn_Clause = NULL;
	st = st_var;
	WN* wn_kid_num;		
	//WN* wnEnd;
    //WGEN_ACC_Set_Cflag(acc_clause_copy);   //set clause flag for check
    wn_Clause = WN_CreateXpragma(pragma_name, st, 1);
	WN_set_pragma_acc(wn_Clause);  
	{
		//which mean this is an array
		wn_kid_num = WN_Intconst(MTYPE_I4, 0);		
		//wnEnd = WN_Intconst(MTYPE_I4, 0);
		//WN* kid0 = WN_Relational (OPR_LT, MTYPE_I4, wnStart, wnEnd);
		//WN_kid1(wn) = NULL;
	}
    //WN* wn_pragmaLength = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_DATA_LENGTH, st, 1);
	//WN_set_pragma_acc(wn_pragmaLength);  
	WN_kid0(wn_Clause) = wn_kid_num;
	//WN_kid0(wn_pragmaLength) = wnEnd;
	WN_INSERT_BlockLast(wn_replace_block, wn_Clause);
	//WN_INSERT_BlockLast(wn_replace_block, wn_pragmaLength);
	//WN_next(wn) = pragmaLength;	
	//WN_prev(pragmaLength) = wn;
    //WN* pragmaStart = WN_CreateXpragma(WN_PRAGMA_ACC_CLAUSE_DATA_START, st, 1);
  
	return wn_Clause;
}


static WN* DFA_const_clause_for_offload_region_nvidia_kepler(ST* st_var, WN* wn_replace_block)
{
	ST * st = NULL;
	WN * wn_start = NULL;
	st = st_var;
    //WGEN_ACC_Set_Cflag(acc_clause_copy);   //set clause flag for check
    wn_start = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_CONST, st, 0, 0);
	WN_INSERT_BlockLast(wn_replace_block, wn_start);
  
	return wn_start;
}


static void DFA_Create_acc_const_varlist_nvidia_kepler(vector<bool>* pBitvector, WN* wn_replace_block)
{
	size_t bit_len = pBitvector->size();
	
    for (size_t i = 0; i < bit_len; i++) 
	{
        if ((*pBitvector)[i]) 
		{
            ST* st_var = acc_var_table[i]->var_st;
            WN* wn_const = DFA_const_clause_for_offload_region_nvidia_kepler(st_var, wn_replace_block);
        } 
    }
}


static WN* DFA_create_acc_parameter_clause_for_offload_region(ST* st_var, WN* wn_replace_block)
{
	ST * st = NULL;
	WN * wn_start = NULL;
	st = st_var;
    //WGEN_ACC_Set_Cflag(acc_clause_copy);   //set clause flag for check
    wn_start = WN_CreatePragma(WN_PRAGMA_ACC_CLAUSE_PARM, st, 0, 0);
	WN_INSERT_BlockLast(wn_replace_block, wn_start);
  
	return wn_start;
}


static void DFA_Create_acc_parameters_varlist(vector<bool>* pBitvector, WN* wn_replace_block)
{
	size_t bit_len = pBitvector->size();
	
    for (size_t i = 0; i < bit_len; i++) 
	{
        if ((*pBitvector)[i]) 
		{
            ST* st_var = acc_var_table[i]->var_st;
            WN* wn_parameters = DFA_create_acc_parameter_clause_for_offload_region(st_var, wn_replace_block);
			//WN_INSERT_BlockLast(wn_replace_block, wn_parameters);
        } 
    }
}


static void DFA_Create_acc_private_varlist(vector<bool>* pBitvector, WN* wn_replace_block)
{
	size_t bit_len = pBitvector->size();
	
    for (size_t i = 0; i < bit_len; i++) 
	{
        if ((*pBitvector)[i]) 
		{
            ST* st_var = acc_var_table[i]->var_st;
            WN* wn_private = DFA_create_acc_data_clause_for_offload_region(WN_PRAGMA_ACC_CLAUSE_PRIVATE, st_var, wn_replace_block);
			//WN_INSERT_BlockLast(wn_replace_block, wn_private);
        } 
    }
}


static void DFA_Create_acc_first_private_varlist(vector<bool>* pBitvector, WN* wn_replace_block)
{
	size_t bit_len = pBitvector->size();
	
    for (size_t i = 0; i < bit_len; i++) 
	{
        if ((*pBitvector)[i]) 
		{
            ST* st_var = acc_var_table[i]->var_st;
            WN* wn_private = DFA_create_acc_data_clause_for_offload_region(WN_PRAGMA_ACC_CLAUSE_FIRST_PRIVATE, st_var, wn_replace_block);
			//WN_INSERT_BlockLast(wn_replace_block, wn_private);
        } 
    }
}

static void DFA_Create_acc_last_private_varlist(vector<bool>* pBitvector, WN* wn_replace_block)
{
	size_t bit_len = pBitvector->size();
	
    for (size_t i = 0; i < bit_len; i++) 
	{
        if ((*pBitvector)[i]) 
		{
            ST* st_var = acc_var_table[i]->var_st;
            WN* wn_private = DFA_create_acc_data_clause_for_offload_region(WN_PRAGMA_ACC_CLAUSE_LAST_PRIVATE, st_var, wn_replace_block);
			//WN_INSERT_BlockLast(wn_replace_block, wn_private);
        } 
    }
}

static void DFA_Create_acc_inout_varlist(vector<bool>* pBitvector, WN* wn_replace_block)
{
	size_t bit_len = pBitvector->size();
	
    for (size_t i = 0; i < bit_len; i++) 
	{
        if ((*pBitvector)[i]) 
		{
            ST* st_var = acc_var_table[i]->var_st;
            WN* wn_private = DFA_create_acc_data_clause_for_offload_region(WN_PRAGMA_ACC_CLAUSE_INOUT_SCALAR, st_var, wn_replace_block);
			//WN_INSERT_BlockLast(wn_replace_block, wn_private);
        } 
    }
}


void Print_local_info(UINT32 bbid)
{
	
		BB_DATAFLOW_INFO_T*  pBB_Info = acc_dataflow_info[bbid];
		if(pBB_Info != NULL)
		{
			UINT32 isize_test1 = pBB_Info->gen.size(); 
			UINT32 isize_test2 = pBB_Info->kill.size();
			UINT32 isize_test3 = pBB_Info->in.size();
			UINT32 isize_test4 = pBB_Info->out.size();
			if(isize_test3 != acc_total_num_vars)
			{	
				printf("Error3\n");
			}
		}
}

//=========================================================================
//
// Function: Perform_Global_DFA
// Input:
//    wdfa_type: data flow analysis type
//
// Description: This carries out iterative, bit-vector data-flow analysis,
// computing IN and OUT sets for each basic block and printing the results.
//
//=========================================================================
void perform_global_dfa(CFG* cfg)//WDFA_TYPE wdfa_type)
{
    INT32 iteration = 1;
    BOOL finished = FALSE;
    //bitset<MAX_TBL_SIZE> boundary_info(0);
	vector<bool> boundary_info(0);
	INT32 i;
		
  	DFSBB_ITER dfs_iter(cfg);
	BB_NODE *bb;	
	INT32 bbid;
	BB_DATAFLOW_INFO_T* pBB_Info;
	//There is some bbs which haven't created bb info yet.
	CFG_ITER cfg_iter;
	///////////////////////////////////////////////////////////////////////
	//FOR_ALL_ELEM (bb, cfg_iter, Init(cfg)) 
	FOR_ALL_ELEM (bb, dfs_iter, Init())
	{
		bbid = bb->Id();
		pBB_Info = acc_dataflow_info[bbid];
		if(pBB_Info == NULL)
		{
		    acc_dataflow_info[bbid] = NEW_WDFA(BB_DATAFLOW_INFO_T);
		    pBB_Info = acc_dataflow_info[bbid];			
			padding_bitvector_if_necessary(bbid, acc_total_num_vars);
		}
		//Print_local_info(bbid);
	}
	//////////////////////////////////////////////////////////////////////
	/*FOR_ALL_ELEM (bb, cfg_iter, Init(cfg)) 
	{
		bbid = bb->Id();
		pBB_Info = acc_dataflow_info[bbid];
		printf("BB ID=%d, BB KIND = %s \n", bbid , 
						BB_kind_name[cfg->Get_bb(bbid )->Kind()]);
		
		printf("Succ :  ");
	    for ( BB_LIST *succ = bb->Succ(); succ != NULL; succ = succ->Next()) 
    	{
			BB_NODE* pNode = succ->Node();
            UINT32 succ_id = pNode->Id();
			printf("%d  ", succ_id);
    	}
		printf("\n");
	    // KILL 
	    fprintf(curr_output_fp, "Kill: ");
		print_sets(&acc_dataflow_info[bbid]->kill);	
	    // GEN 
	    fprintf(curr_output_fp, "Gen: ");
		print_sets(&acc_dataflow_info[bbid]->gen);
	}*/
	//////////////////////////////////////////////////////////////////////////	
	for(i=0; i<acc_total_num_vars; i++)
	{
		boundary_info.push_back(FALSE);
	}
	///////////////////////////////////////////////////////////////////////////
	/*FOR_ALL_ELEM (bb, dfs_iter, Init())
	{
		bbid = bb->Id();
		//printf("BB ID=%d, BB KIND = %s \n", bbid, BB_kind_name[bb->Kind()]);
		if(bb->Kind() == BB_REGIONSTART)
		{
			//fprintf(curr_output_fp, "BB %d\n", bbid);
        	//print_in_out_sets(bbid);
        	BB_REGION* pRegionInfo = bb->Regioninfo();
			if((WN_opcode(bb->Firststmt()) == OPC_REGION))
			{
				WN* node  = bb->Firststmt();
			    WN *wtmp = WN_first(WN_region_pragmas(node));
			    WN_PRAGMA_ID wid = (WN_PRAGMA_ID) WN_pragma(wtmp);
				if(wid == WN_PRAGMA_ACC_KERNELS_BEGIN ||
					wid == WN_PRAGMA_ACC_PARALLEL_BEGIN)
				{
					WN* wn_exit = WN_last(WN_region_body(node));
					UINT32 LabelNumber = WN_label_number(wn_exit);
					acc_label_bbid[LabelNumber] = bbid;	// region exit belongs to which bbid
    				//acc_start_end_bbid.clear();
					//fdump_tree(stdout, wn_exit);
				}
			}
			
		}
	}*/
	///////////////////////////////////////////////////////////////////////////////
	//Find the Parallel/Kernel region exit block
	/*FOR_ALL_ELEM (bb, dfs_iter, Init())
	{
		bbid = bb->Id();
		//printf("BB ID=%d, BB KIND = %s \n", bbid, BB_kind_name[bb->Kind()]);
		if(bb->Kind() == BB_REGIONEXIT)
		{	
			STMT_ITER wn_iter(bb->Firststmt(), bb->Laststmt());
			//WN* wn_exit = bb->Laststmt(); 
			for (wn_iter.First(); !wn_iter.Is_Empty(); wn_iter.Next())
			{
				if(WN_opcode(wn_iter.Cur()) == OPC_REGION_EXIT)
				{					
					UINT32 LabelNumber = WN_label_number(wn_iter.Cur());
					//found
					if(acc_label_bbid.find(LabelNumber) != acc_label_bbid.end())
					{
						UINT32 startid = acc_label_bbid[LabelNumber];
						acc_start_end_bbid[startid] = bb->Id();
					}
				}
				//fdump_tree(stdout, wn_iter.Cur());
			}
		}
	}*/
	/////////////////////////////////////////////////////////////////////////////
	//Match the parallel/kernels exit block
	/*map<UINT32,UINT32>::iterator itor = acc_start_end_bbid.begin();
	for(; itor!=acc_start_end_bbid.end(); itor++)
	{
		UINT32 startID = itor->first;
		UINT32 exitID = itor->second;
		
		//printf("start BB ID=%d, BB KIND = %s \n", startID, 
		//				BB_kind_name[cfg->Get_bb(startID)->Kind()]);
		//printf("end BB ID=%d, BB KIND = %s \n", exitID, 
		//				BB_kind_name[cfg->Get_bb(exitID)->Kind()]);
		//printf("test\n");
	}*/
	///////////////////////////////////////////////////////////////////////////
	//scan the Parallel/Kernels region to generate all the used var bit vector
	FOR_ALL_ELEM (bb, dfs_iter, Init())
	{	
		WN* wn_region  = bb->Firststmt();
		if((bb->Kind() == BB_REGIONSTART && bb->Firststmt())
			&& (WN_opcode(bb->Firststmt()) == OPC_REGION)
			&& ((WN_pragma(WN_first(WN_region_pragmas(wn_region)))==WN_PRAGMA_ACC_KERNELS_BEGIN)
			|| (WN_pragma(WN_first(WN_region_pragmas(wn_region)))==WN_PRAGMA_ACC_PARALLEL_BEGIN)))
		{
			WN* wn_region_body = WN_region_body(wn_region);
			
			WN_TREE_ITER<POST_ORDER> tree_iter(wn_region_body);
			WN* wn;
			ST *defined_var = NULL;
			ST *used_var = NULL;
			UINT32 bitvector_index;
			BB_DATAFLOW_ACC_REGION_INFO* pRegionInfo = NEW_WDFA(BB_DATAFLOW_ACC_REGION_INFO);
			bbid = bb->Id();
		    acc_used_data_classify[bbid] = pRegionInfo;
			//padding
			for(INT32 bit_index = 0; bit_index < acc_total_num_vars; bit_index++)
			{
				pRegionInfo->used.push_back(FALSE);
				pRegionInfo->changed.push_back(FALSE);
				pRegionInfo->scalar_private.push_back(FALSE);
				pRegionInfo->parameters.push_back(FALSE);
				pRegionInfo->first_private.push_back(FALSE);
				pRegionInfo->last_private.push_back(FALSE);
				pRegionInfo->inout.push_back(FALSE);
				pRegionInfo->array_pointer.push_back(FALSE);
				pRegionInfo->global.push_back(FALSE);
				pRegionInfo->write_ptr.push_back(FALSE);
				pRegionInfo->read_ptr.push_back(FALSE);
				pRegionInfo->readonly_ptr.push_back(FALSE);
				pRegionInfo->readonly_scalar.push_back(FALSE);				
			}
						
			while (tree_iter != LAST_POST_ORDER_ITER)
			{
				wn = tree_iter.Wn();
		        switch (WN_operator(wn)) 
				{
		            case OPR_LDID:
		                used_var = WN_st(wn);
						if(dfa_scan_is_uncounted_var(used_var))
							break;
						bitvector_index = acc_vars[used_var];
						pRegionInfo->used[bitvector_index] = TRUE;
		                break;
					case OPR_LDA:
		                used_var = WN_st(wn);
						if(dfa_scan_is_uncounted_var(used_var))
							break;
						bitvector_index = acc_vars[used_var];
						pRegionInfo->used[bitvector_index] = TRUE;
						//may be changed
						if(TY_kind(ST_type(used_var)) == KIND_SCALAR)
						{
							pRegionInfo->changed[bitvector_index] = TRUE;
							if(dfa_scan_is_global_var(used_var))
								pRegionInfo->global[bitvector_index] = TRUE;
						}
		                break;
			        case OPR_STID:
		                used_var = WN_st(wn);
						if(dfa_scan_is_uncounted_var(used_var))
							break;
						bitvector_index = acc_vars[used_var];
						pRegionInfo->used[bitvector_index] = TRUE;
						pRegionInfo->changed[bitvector_index] = TRUE;
						if(dfa_scan_is_global_var(used_var))
							pRegionInfo->global[bitvector_index] = TRUE;
						break;
					case OPR_ISTORE:
						{
							WN* wn_addr = WN_kid1(wn);
							OPERATOR opr = WN_operator(wn_addr);
							WN* wn_base;
							if(opr==OPR_ADD)
							{
								wn_base = WN_kid0(wn_addr);
							}
							else if(opr==OPR_ARRAY)
							{							
								wn_base = WN_array_base(wn_addr) ;
							}
							else
								Fail_FmtAssertion("Unhandled address operator in ISTORE. opt_dfa_openacc.cxx.");
							if(WN_operator(wn_base) == OPR_LDID
								|| WN_operator(wn_base) == OPR_LDA)
							{
								ST* st_addr = WN_st(wn_base);
								TY_IDX ty_addr = ST_type(st_addr);
								if(TY_kind(ty_addr) == KIND_POINTER
										|| TY_kind(ty_addr) == KIND_ARRAY)
								{
									bitvector_index = acc_vars[st_addr];
									//ST* st_var_table = acc_var_table[bitvector_index]->var_st;
									pRegionInfo->write_ptr[bitvector_index] = TRUE;
								}
							}
							
						}
						break;
					case OPR_ILOAD:
						{
							WN* wn_addr = WN_kid0(wn);
							OPERATOR opr = WN_operator(wn_addr);
							WN* wn_base;
							if(opr==OPR_ADD)
							{
								wn_base = WN_kid0(wn_addr);
							}
							else if(opr==OPR_ARRAY)
							{							
								wn_base = WN_array_base(wn_addr);
							}
							else
								Fail_FmtAssertion("Unhandled address operator in ILOAD. opt_dfa_openacc.cxx.");
							if(WN_operator(wn_base) == OPR_LDID
								|| WN_operator(wn_base) == OPR_LDA)
							{
								ST* st_addr = WN_st(wn_base);
								TY_IDX ty_addr = ST_type(st_addr);
								if(TY_kind(ty_addr) == KIND_POINTER
										|| TY_kind(ty_addr) == KIND_ARRAY)
								{
									bitvector_index = acc_vars[st_addr];
									//ST* st_var_table = acc_var_table[bitvector_index]->var_st;
									pRegionInfo->read_ptr[bitvector_index] = TRUE;
								}
							}
						}
						break;
		        }				
				tree_iter ++;
			}
			//Find out the pointer/array variables that are used in this 
			for(INT32 bit_index = 0; bit_index < acc_total_num_vars; bit_index++)
			{
				if(pRegionInfo->used[bit_index])
				{
					ST* st_var = acc_var_table[bit_index]->var_st;
					if(TY_kind(ST_type(st_var)) == KIND_POINTER 
						|| TY_kind(ST_type(st_var)) == KIND_ARRAY
						|| F90_ST_Has_Dope_Vector(st_var))
						pRegionInfo->array_pointer[bit_index] = TRUE;
				}
			}
		}
	}
	///////////////////////////////////////////////////////////////////////////
    while (!finished) 
	{
        //bitset<MAX_TBL_SIZE> old_in, old_out;
		vector<bool> old_in, old_out;
		
        //fprintf(curr_output_fp, "==========================================\n");
        //fprintf(curr_output_fp, "ITERATION %d\n", iteration);
        //fprintf(curr_output_fp, "==========================================\n");


        finished = TRUE;
		FOR_ALL_ELEM (bb, dfs_iter, Init())
    	//for (WN_CFG::po_dfs_iterator it = cfg->Po_dfs_begin();
        // it != cfg->Po_dfs_end();
        //++it) 
		{
	        //INT32 v = it->Get_id();			
			bbid = bb->Id();
	        BB_DATAFLOW_INFO_T *info = acc_dataflow_info[bbid];

	        //fprintf(curr_output_fp, "BB %d, Kind=%s.\n", bbid, BB_kind_name[cfg->Get_bb(bbid)->Kind()]);						

	        old_in = info->in;
	        old_out = info->out;

			// compute OUT
	        if (bb->Succ()->Len() == 0) //it->Succ_begin() == it->Succ_end()) 
			{
				// end node
	            info->out = boundary_info;
	        } 
			else 
			{
	            //info->out.reset();
	            dfa_reset_bitvector(&info->out);
				/*for ( BB_LIST *succ = bb->Succ(); succ != NULL; succ = succ->Next()) 
				{
					BB_NODE* pNode = succ->Node();
					printf("%d \t", pNode->Id());
				}*/
				
	            //for (BB_NODE_BASE<WN_STMT_CONTAINER>::
	            //        bb_iterator sit = it->Succ_begin();
	            //    sit != it->Succ_end();
	            //    ++sit) 
	            for ( BB_LIST *succ = bb->Succ(); succ != NULL; succ = succ->Next()) 
				{
					BB_NODE* pNode = succ->Node();
	                UINT32 succ_id = pNode->Id();
					pBB_Info = acc_dataflow_info[succ_id];
	                //info->out |= acc_dataflow_info[succ_id]->in;
	                info->out = dfa_or_operation_bitvector(&info->out,
	                				&pBB_Info->in);
	            }
	        }

	        // compute IN
	        //info->in = (info->out & ~(info->kill)) | info->gen;
	        vector<bool> bv_results = dfa_not_operation_bitvector(&info->kill);
		    //fprintf(curr_output_fp, "bv_results: ");
			//print_sets_no_st(&bv_results);	
			bv_results = dfa_and_operation_bitvector(&info->out, &bv_results);
	        info->in = dfa_or_operation_bitvector(&bv_results, &info->gen);

	        //finished &= (old_in == info->in) && (old_out == info->out);
	        finished &= dfa_is_same_operation_bitvector(&old_in, &info->in)
	        				&& dfa_is_same_operation_bitvector(&old_out, &info->out);
			
	        //print_in_out_sets(bbid);			
    	}
        iteration++;
    }
	//////////////////////////////////////////////////////////////////
	BOOL bOutputInfo = (Enable_UHACCInfoFlag&(1<<UHACC_INFO_OUTPUT_DFA))>>UHACC_INFO_OUTPUT_DFA;
	//generate first private, last private, shared, private variables list
	map<UINT32, BB_DATAFLOW_ACC_REGION_INFO*>::iterator itor2 = acc_used_data_classify.begin();
	for(; itor2!=acc_used_data_classify.end(); itor2 ++)
	{
		UINT32 istart_bbid = itor2->first;
		UINT32 iend_bbid; // = ;
		BB_DATAFLOW_ACC_REGION_INFO* pRegionInfo = acc_used_data_classify[istart_bbid];
		//if(acc_start_end_bbid.find(istart_bbid) == acc_start_end_bbid.end())
		//	Fail_FmtAssertion("Error: Cannot find the parallel/kernel region id in acc_start_end_bbid pair.");		
        BB_REGION* pBB_RegionInfo = cfg->Get_bb(istart_bbid)->Regioninfo();
		//iend_bbid = acc_start_end_bbid[istart_bbid];
		iend_bbid = pBB_RegionInfo->Region_end()->Id();
		BB_DATAFLOW_INFO_T* pStartBB_Info = acc_dataflow_info[istart_bbid];
		BB_DATAFLOW_INFO_T* pEndBB_Info = acc_dataflow_info[iend_bbid];
		///////////////////////////////////////////////////////////////////////////////////
	    // In in start region
	    //fprintf(curr_output_fp, "IN: ");
		//print_sets(&acc_dataflow_info[istart_bbid]->in);	
	    // Out in end region
	    //fprintf(curr_output_fp, "OUT: ");
		//print_sets(&acc_dataflow_info[iend_bbid]->out);
	    //fprintf(curr_output_fp, "USED: ");
		//print_sets(&pRegionInfo->used);
		///////////////////////////////////////////////////////////////////////////////////
		//Now need IN set from start BB, and OUT set from End BB
		vector<bool>* pBitvectorIN = &pStartBB_Info->in;
		vector<bool>* pBitvectorOUT = &pEndBB_Info->out;
		//filter out all the unused data in IN/OUT sets. The unused var maybe used the succ nodes, so they appear in both IN/OUT.
		vector<bool> filterIN = dfa_and_operation_bitvector(pBitvectorIN, &pRegionInfo->used);
		vector<bool> filterOUT = dfa_and_operation_bitvector(pBitvectorOUT, &pRegionInfo->used);
		//if the data is not changed in this offload region, ignore. 
		filterOUT = dfa_and_operation_bitvector(&filterOUT, &pRegionInfo->changed);		
		//in case some global data is changed and ignored in this region, put them in here
		filterOUT = dfa_or_operation_bitvector(&filterOUT, &pRegionInfo->global);
		if(bOutputInfo)
		{
			fprintf(curr_output_fp, "*****************************************************************\n");
			fprintf(curr_output_fp, "GLOBAL Changed: ");
			print_sets(&pRegionInfo->global);
		}
	    //fprintf(curr_output_fp, "filterIN: ");
		//print_sets(&filterIN);
	    //fprintf(curr_output_fp, "filterOUT: ");
		//print_sets(&filterOUT);
		/////////////////////////////////////////////////////////////
		vector<bool> tmpResults1 = dfa_not_operation_bitvector(&pRegionInfo->array_pointer);
		vector<bool> tmpResults2;
		pRegionInfo->parameters = dfa_or_operation_bitvector(&filterIN, &filterOUT);
	    // print PARAMETER set
		if(bOutputInfo)
		{
		    fprintf(curr_output_fp, "PARAMETERS: ");
			print_sets(&pRegionInfo->parameters);
		}
		//////////////////////////////////////////////////////////////////////////////////////
		//Only get the scalar variable
		//private section
		tmpResults2 = dfa_or_operation_bitvector(&filterIN, &filterOUT);
		tmpResults2 = dfa_xor_operation_bitvector(&tmpResults2, &pRegionInfo->used);
		pRegionInfo->scalar_private = dfa_and_operation_bitvector(&tmpResults2, &tmpResults1);
	    // print private set
		if(bOutputInfo)
		{
		    fprintf(curr_output_fp, "PRIVATE: ");
			print_sets(&pRegionInfo->scalar_private);
		}
		//////////////////////////////////////////////////////////////////////////////////////
		//inout section
		tmpResults2 = dfa_and_operation_bitvector(&filterIN, &filterOUT);
		pRegionInfo->inout = dfa_and_operation_bitvector(&tmpResults2, &tmpResults1);
		//INOUT have to be removed from FIRST PRIVATE and LAST PRIVATE
		tmpResults2 = dfa_not_operation_bitvector(&pRegionInfo->inout);;
	    // print INOUT set
		if(bOutputInfo)
		{
		    fprintf(curr_output_fp, "INOUT: ");
			print_sets(&pRegionInfo->inout);
		}
		//////////////////////////////////////////////////////////////////////////////////////
		//first private section
		pRegionInfo->first_private = dfa_and_operation_bitvector(&filterIN, &tmpResults1);
		pRegionInfo->first_private = dfa_and_operation_bitvector(&pRegionInfo->first_private, &tmpResults2);
	    // print first private set
		if(bOutputInfo)
		{
		    fprintf(curr_output_fp, "FIRST PRIVATE: ");
			print_sets(&pRegionInfo->first_private);
		}
		//////////////////////////////////////////////////////////////////////////////////////
		//last private section
		pRegionInfo->last_private = dfa_and_operation_bitvector(&filterOUT, &tmpResults1);
		pRegionInfo->last_private = dfa_and_operation_bitvector(&pRegionInfo->last_private, &tmpResults2);
	    // print last private set
		if(bOutputInfo)
		{
		    fprintf(curr_output_fp, "LAST PRIVATE: ");
			print_sets(&pRegionInfo->last_private);
		}
		//////////////////////////////////////////////////////////////////////////////////////
		//read-only scalar
		//all the read-only scalar must in first private list
		// readonly-scalar = firstprivate & ^changed
		tmpResults2 = dfa_not_operation_bitvector(&pRegionInfo->changed);
		pRegionInfo->readonly_scalar = dfa_and_operation_bitvector(&pRegionInfo->first_private,
										&tmpResults2);
		if(bOutputInfo)
		{
		    fprintf(curr_output_fp, "READONLY-SCALAR: ");
			print_sets(&pRegionInfo->readonly_scalar);
		}
		//////////////////////////////////////////////////////////////////////////////////////
		//const pointer
		tmpResults2 = dfa_and_operation_bitvector(&pRegionInfo->write_ptr, 
									&pRegionInfo->read_ptr);
		//pRegionInfo->write_ptr;
		pRegionInfo->readonly_ptr = dfa_xor_operation_bitvector(&pRegionInfo->read_ptr, 
									&tmpResults2);		// print last private set
		if(run_autoConstCacheOpt && bOutputInfo)
	    {
		    fprintf(curr_output_fp, "WRITE array or pointer: ");
			print_sets(&pRegionInfo->write_ptr);
		    fprintf(curr_output_fp, "READ array or pointer: ");
			print_sets(&pRegionInfo->read_ptr);
		    fprintf(curr_output_fp, "READONLY array or pointer: ");
			print_sets(&pRegionInfo->readonly_ptr);
		}
		//////////////////////////////////////////////////////////////////////////////////////
		//Attached the WN pragma to region pragma block
		bb = cfg->Get_bb(istart_bbid);
		Is_True((bb->Kind()==BB_REGIONSTART), ("start bbid is not region start basic block"));
		WN* wn_pragma_block = WN_region_pragmas(bb->Firststmt());
		DFA_Create_acc_private_varlist(&pRegionInfo->scalar_private, wn_pragma_block);
		DFA_Create_acc_first_private_varlist(&pRegionInfo->first_private, wn_pragma_block);
		DFA_Create_acc_last_private_varlist(&pRegionInfo->last_private, wn_pragma_block);
		DFA_Create_acc_inout_varlist(&pRegionInfo->inout, wn_pragma_block);
		DFA_Create_acc_parameters_varlist(&pRegionInfo->parameters, wn_pragma_block);		
		//this is the read-only scalar and they will be used for scalar replacement algorithms
		DFA_Create_acc_const_varlist_nvidia_kepler(&pRegionInfo->readonly_scalar, wn_pragma_block);
		if(run_autoConstCacheOpt)
			DFA_Create_acc_const_varlist_nvidia_kepler(&pRegionInfo->readonly_ptr, wn_pragma_block);
	}
}

//This is for debug
void TraverseAndCheckEachBBStmt(CFG* cfg)
{
	STMT_ITER     stmt_iter;
	WN           *wn;
	DFSBB_ITER dfs_iter(cfg);
	BB_NODE *bb;	
	INT32 bbid;
	MEM_POOL ssa_debug_pool;
	
    MEM_POOL_Initialize(&ssa_debug_pool, "SSA DEBUG Pool", FALSE);
	
	FOR_ALL_ELEM (bb, dfs_iter, Init())
	{		
		bbid = bb->Id();
	    fprintf(stdout, "BB %d, Kind=%s.\n", bbid, BB_kind_name[cfg->Get_bb(bbid)->Kind()]);	
		FOR_ALL_ELEM (wn, stmt_iter, Init(bb->Firststmt(), bb->Laststmt())) 
		{
			//stmt = bb->Add_stmtnode(wn, ssa_debug_pool);
			STMTREP *stmt = CXX_NEW(STMTREP(WN_opcode(wn)), &ssa_debug_pool);
			stmt->Set_wn(wn);
			// note: this code should be nearly identical to Append_stmtrep,
			// except for checks to see if it's above a branch if any in the
			// block.  This method should only be called for quickly converting
			// from WNs to STMTREP.
			stmt->Print(stdout);
		}
	}
    
    MEM_POOL_Delete(&ssa_debug_pool);
}
