/*
 * Copyright (C) 2008-2010 Advanced Micro Devices, Inc.  All Rights Reserved.
 */
// This program is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it would be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// Further, this software is distributed without any warranty that it
// is free of the rightful claim of any third person regarding
// infringement  or the like.  Any license provided herein, whether
// implied or otherwise, applies only to this software file.  Patent
// licenses, if any, provided herein do not apply to combinations of
// this program with other software, or any other product whatsoever.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.

// ===============================================================================================
//
// Description:
//
//                   Structured  Component Nodes
//                   --------------------------
//      This is the structured component node data structure that represents if-region, loop-region
//      and other structured control flow components in the source program. 
//
// Reserved prefix:
// ----------------
//
//     SC         for structured component node
//
// Exported types:
// ---------------
//     SC_NODE
//
//          A structured component node.  Every structured component node contains the
//          following fields.
//
//              IDTYPE     _id
//                    A unique identifier for a structured component node.
//
//              SC_TYPE    type
//                    The type of a SC_NODE.  See SC_TYPE.
//
//              MEM_POOL * pool
//                    Allocation memory pool.
//
//
//              BB_NODE * bb_rep
//                    A pointer to representing BB_NODE.  
//
//              BB_LIST * bbs
//                    A list of BB_NODEs contained in this SC_NODE.
//
//              SC_NODE * parent
//                    A pointer to the parent node.
//
//              SC_LIST * kids
//                    A pointer to the list of child nodes.  The list is ordered
//                    according to order of occurrence in the procedure.      
//
//              IDTYPE _class_id
//                    ID of the loop class this SC_NODE belongs.  Scratch field. 
//                    Loops are classified according to their path symmetricities in the SC tree.
//
//              int _depth
//                    Depth of this SC_NODE in the SC tree. Scratch field.
//
//              int flag
//                    Flag of this SC_NODE.  see SC_NODE_FLAG.
//
//     SC_LIST :SLIST_NODE
//
//          A singly linked list of SC_NODEs.  We use this class to build the children node list.
//
//     SC_LIST_CONTAINER : SLIST
//     SC_LIST_ITER : SLIST_ITER
//
//          The standard container and iterator for singly linked list.
//     
//
// ==============================================================================================

#ifndef opt_proactive_INCLUDED
#define opt_proactive_INCLUDED "opt_proactive.h"
#include <iterator>
#include <set>
#include <map>


#ifndef opt_bb_INCLUDED
#include "opt_bb.h"
#endif
#ifndef opt_base_INCLUDED
#include "opt_base.h"
#endif
#ifndef opt_main_INCLUDED
#include "opt_main.h"
#endif

extern "C" {
#include "bitset.h"
}

// Type of SC nodes
enum SC_TYPE {
  SC_NONE = 0,
  SC_IF,     // if-region
  SC_THEN,   // then-path of a if-region
  SC_ELSE,   // else-path of a if-region
  SC_LOOP,   // loop region
  SC_BLOCK,  // blocks of straight-line codes
  SC_FUNC,   // func entry
  SC_LP_START, // H-WHIRL loop start
  SC_LP_COND,  // H-WHIRL loop cond
  SC_LP_STEP,  // H-WHIRL loop step
  SC_LP_BACKEDGE,  // H-WHIRL loop backedge
  SC_LP_BODY,  // loop body
  SC_COMPGOTO, // COMPGOTO
  SC_OTHER   // other structured control flows
};

extern BOOL SC_type_has_rep(SC_TYPE type);
extern BOOL SC_type_has_bbs(SC_TYPE type);
extern BOOL SC_type_is_marker(SC_TYPE type);

static const char * sc_type_name[] =
  {"NONE", "IF", "THEN", "ELSE", "LOOP", "BLOCK", "FUNC",
   "LP_START", "LP_COND", "LP_STEP", "LP_BACKEDGE", "LP_BODY", "COMPGOTO", "OTHER"};

// bit mask.
enum SC_NODE_FLAG
{
  HAS_SYMM_LOOP = 0x1
};


// Structure component nodes.
class SC_NODE {
private:
  SC_TYPE type;
  IDTYPE _id;
  MEM_POOL * pool;
  union {
    BB_NODE * bb_rep;  // Pointer to this SC_NODE's representing BB_NODE. Valid for SC_IF.
    BB_LIST * bbs;     // A list of BB_NODEs. Valid for SC_BLOCK only.
  } u1;
  SC_NODE * parent;
  SC_LIST * kids;
  IDTYPE _class_id;
  int _depth;
  int _flag;

private:
  BOOL Is_member(BB_NODE *);

public:
  IDTYPE       Id(void)          const  { return _id; }
  void         Set_id(IDTYPE i )        { _id = i; }
  IDTYPE       Class_id(void)    const  { return _class_id; }
  void         Set_class_id(IDTYPE i)   { _class_id = i; }
  int          Depth(void)       const  { return _depth; }
  void         Set_depth(int i)         { _depth = i; }
  int          Flag()            const  { return _flag; }
  void         Set_flag(int i)          { _flag = i; }
  void         Remove_flag(int i);
  BOOL         Has_flag(int i)          { return ((_flag & i) != 0); }
  void         Add_flag(int i)          { if (!Has_flag(i)) {_flag += i; } }
  SC_TYPE      Type(void)        const { return type; }
  void         Set_type(SC_TYPE i)     { type = i; }
  const char * Type_name(void) const   { return sc_type_name[type]; }
  BB_NODE *    Get_bb_rep()    const   { return (SC_type_has_rep(type) ? u1.bb_rep : NULL); }
  void         Set_bb_rep(BB_NODE * i) 
  { 
    FmtAssert(SC_type_has_rep(type), ("Unexpected SC_NODE"));
    u1.bb_rep = i; 
  }

  BB_LIST *       Get_bbs()   const { return (SC_type_has_bbs(type) ? u1.bbs : NULL); }
  void            Append_bbs(BB_NODE *i)
  {
    FmtAssert(SC_type_has_bbs(type), ("Unexpected SC_NODE"));
    if (u1.bbs == NULL)
      u1.bbs = CXX_NEW(BB_LIST(i), pool);
    else
      u1.bbs = u1.bbs->Append(i, pool);
  }

  void            Prepend_bbs(BB_NODE *i)
  {
    FmtAssert(SC_type_has_bbs(type), ("Unexpected SC_NODE"));
    if (u1.bbs == NULL)
      u1.bbs = CXX_NEW(BB_LIST(i), pool);
    else
      u1.bbs = u1.bbs->Prepend(i, pool);
  }

  void           Set_bbs(BB_LIST * i)
  { 
    FmtAssert(SC_type_has_bbs(type), ("Unexpected SC_NODE"));
    u1.bbs = i; 
  }

  MEM_POOL * Get_pool()                { return pool; }
  void       Set_pool(MEM_POOL * i)    { pool = i; }

  SC_NODE *  Parent(void)      const   { return parent; }
  void       Set_parent(SC_NODE * i)   { parent = i; }
  void       Set_kids(SC_LIST * i)     { kids = i; }
  SC_LIST *  Kids(void)        const   { return kids; }
  void       Clear(void);
  BOOL       Is_empty_block();
  
  SC_NODE(void)          { Clear(); }
  SC_NODE(const SC_NODE&);
  ~SC_NODE(void)         {}

  void Print(FILE *fp=stderr, BOOL = TRUE) const;

  void Append_kid(SC_NODE * sc);
  void Prepend_kid(SC_NODE * sc);
  void Remove_kid(SC_NODE * sc);
  void Insert_before(SC_NODE * sc);
  void Insert_after(SC_NODE * sc);
  SC_NODE * Last_kid();
  SC_NODE * First_kid();
  SC_NODE * Next_sibling();
  SC_NODE * Prev_sibling();
  SC_NODE * Next_sibling_of_type(SC_TYPE);
  SC_NODE * Next_in_tree();
  SC_NODE * Get_nesting_if(SC_NODE *);
  SC_NODE * First_kid_of_type(SC_TYPE);
  BOOL Contains(BB_NODE *);
  BB_NODE * Then();
  BB_NODE * Else();
  BB_NODE * Merge();
  void      Set_merge(BB_NODE *);
  BB_NODE * Head();
  BB_NODE * Then_end();
  BB_NODE * Else_end();
  BB_NODE * Exit();
  BB_LOOP * Loopinfo();
  WN *      Index();
  SC_NODE * Find_kid_of_type(SC_TYPE);
  void Unlink();
  void Convert(SC_TYPE);
  void Delete();
  BOOL Is_well_behaved();
  BOOL Is_sese();
  BOOL Has_same_loop_struct(SC_NODE *);
  BOOL Has_symmetric_path(SC_NODE *, BOOL);
  SC_NODE * Find_lcp(SC_NODE *);
  BB_NODE * First_bb();
  BB_NODE * Last_bb();
  // Find first executable statement in this SC_NODE.
  WN * First_executable_stmt();
  BOOL Is_pred_in_tree(SC_NODE *);
  int Num_of_loops(SC_NODE *, BOOL, BOOL);
  int Executable_stmt_count();
  BOOL Has_loop();
  SC_NODE * Get_real_parent();
};

class SC_LIST : public SLIST_NODE {
private:
  SC_NODE * node;
  SC_LIST(const SC_LIST &);
  SC_LIST &operator = (const SC_LIST&);

public:
  SC_LIST(void)         { Clear(); }
  SC_LIST(SC_NODE * nd) { Clear(); node = nd;}
  ~SC_LIST(void)        {};
  
  DECLARE_SLIST_NODE_CLASS( SC_LIST )
  SC_LIST *Append (SC_NODE *bb, MEM_POOL *pool);  

  SC_LIST *Prepend(SC_NODE *bb, MEM_POOL *pool)
  {
    SC_LIST * new_sclst = (SC_LIST*) CXX_NEW(SC_LIST(bb), pool);
    new_sclst->Set_Next(this);
    return new_sclst;
  }

  SC_LIST *Remove(SC_NODE *sc, MEM_POOL *pool);
  BOOL Contains(SC_NODE *sc) const;
  void Print (FILE *fp = stderr) const;
  SC_NODE * Last_elem();
  SC_NODE * First_elem();
  
  void Init(SC_NODE *nd)      { node = nd; }
  void Clear(void)            { node = NULL; }

  SC_NODE *Node(void)  const { return node;}
  void Set_node(SC_NODE *sc)  { node = sc; }
};

class SC_LIST_CONTAINER : public SLIST {
private:
  DECLARE_SLIST_CLASS( SC_LIST_CONTAINER, SC_LIST )  

  SC_LIST_CONTAINER(const SC_LIST_CONTAINER&);
  SC_LIST_CONTAINER& operator = (const SC_LIST_CONTAINER&);

public:
  ~SC_LIST_CONTAINER(void) {};

  void Append (SC_NODE *sc, MEM_POOL *pool);
  void Prepend (SC_NODE *sc, MEM_POOL *pool);
  void Remove(SC_NODE *sc, MEM_POOL *pool);
  SC_NODE *Remove_head(MEM_POOL *pool);
  BOOL Contains(SC_NODE *sc) const;
};

class SC_LIST_ITER : public SLIST_ITER {
  DECLARE_SLIST_CONST_ITER_CLASS( SC_LIST_ITER, SC_LIST, SC_LIST_CONTAINER )
public:
  void     Init(void)       {}
  void Validate_unique(FILE *fp=stderr);
  SC_NODE *First_sc(void)   { return (First()) ? Cur()->Node():NULL; }
  SC_NODE *Next_sc(void)    { return (Next())  ? Cur()->Node():NULL; }
  SC_NODE *Cur_sc(void)     { return (Cur())   ? Cur()->Node():NULL; }
  SC_NODE *First_elem(void) { return (First()) ? Cur()->Node():NULL; }
  SC_NODE *Next_elem(void)  { return (Next())  ? Cur()->Node():NULL; }
};

// Generic CFG transformations.
class CFG_TRANS {
private:
  BS * _true_val;   // a bit set of TRUE values, scratch field.
  MAP * _val_map;   // map from an interger to a value number.
  WN_MAP _low_map;  // map from WN * to a constant that gives value's low boundary.
  WN_MAP _high_map; // map from WN * to a constant that gives value's high boundary.
  MAP * _def_wn_map;   // map AUX_ID to definition WN *.
  MAP * _const_wn_map; // map an integer constant to WHIRL.

protected:
  COMP_UNIT * _cu;
  BOOL _trace;
  BOOL _dump;
  int _transform_count;
  MEM_POOL * _pool;
  INT32 _code_bloat_count;
  MAP * _invar_map;  // hash BB_NODE Id to SC_NODE *

private:
  void Infer_val_range(WN *, BOOL, BOOL);
  void Match_def(WN *);
  STACK<WN *> * Collect_operands(WN *);
  void Set_map(WN_MAP &, WN *, WN *);

protected:
  void Inc_transform_count() { _transform_count++; }
  void Delete_val_map();
  void Init_val_map(WN *, BOOL);
  void Infer_val_range(SC_NODE *, SC_NODE *);
  void Delete_val_range_maps();
  BOOL Val_mod(SC_NODE *, WN *, BOOL);
  BOOL Val_match(WN *);
  BOOL Is_trackable_var(AUX_ID);
  BOOL Is_trackable_expr(WN *);
  void Track_val(BB_NODE *, BB_NODE *, WN *);
  void Track_val(SC_NODE *, BB_NODE *, WN *);
  AUX_ID Get_val(AUX_ID);
  void Set_val(AUX_ID, AUX_ID);
  void Remove_val(WN *, WN *);
  void Fix_parent_info(SC_NODE *, SC_NODE *);
  void Insert_region(BB_NODE *, BB_NODE *, BB_NODE *, BB_NODE *, MEM_POOL *);
  BOOL Has_side_effect(WN *);
  BOOL Is_aliased(WN *, WN *);
  BOOL Maybe_assigned_expr(WN *, WN *);
  BOOL Maybe_assigned_expr(SC_NODE *, WN *, BOOL);
  BOOL Maybe_assigned_expr(BB_NODE *, WN *);
  BOOL Maybe_assigned_expr(SC_NODE *, BB_NODE *);
  BOOL Maybe_assigned_expr(SC_NODE *, SC_NODE *);
  BOOL Maybe_assigned_expr(BB_NODE *, BB_NODE *);  
  BOOL Maybe_assigned_expr(BB_NODE *, SC_NODE *);
  BOOL Has_dependency(SC_NODE *, SC_NODE *);
  BOOL Has_dependency(SC_NODE *, BB_NODE *);
  BOOL Has_dependency(BB_NODE *, BB_NODE *);
  BOOL Has_dependency(SC_NODE * , WN *);
  BOOL Has_dependency(BB_NODE *, WN *);
  BOOL Is_invariant(SC_NODE *, SC_NODE *, AUX_ID);
  BOOL Is_invariant(SC_NODE *, BB_NODE *, AUX_ID);
  BOOL Is_invariant(BB_NODE *, BB_NODE *, AUX_ID);
  BOOL Is_invariant(BB_NODE *, WN * wn, AUX_ID);
  BOOL Is_invariant(SC_NODE *, WN * wn, AUX_ID);
  BOOL Can_be_speculative(SC_NODE *);
  BOOL Can_be_speculative(BB_NODE *);
  BOOL Can_be_speculative(WN *);
  ST * Get_st(WN *);
  void Delete_branch(BB_NODE *);
  MAP * Get_invar_map() { return _invar_map ; }
  void New_invar_map();
  void Delete_invar_map();
  void Hash_invar(BB_NODE *, SC_NODE *);
  void Clear();
  void Delete();
  void Set_cu(COMP_UNIT * i) { _cu = i; }
  COMP_UNIT * Get_cu();
  WN * Get_const_wn(long long);

public:
  void Set_trace(BOOL i) { _trace = i; }
  void Set_dump(BOOL i)  { _dump = i; }
  void Set_pool(MEM_POOL * i) { _pool = i; }
  int Transform_count() { return _transform_count; }
  void Do_code_motion(SC_NODE *, SC_NODE *);
  void Do_head_duplication(SC_NODE *, SC_NODE *);
  void Do_tail_duplication(SC_NODE *, SC_NODE *);
};

// bit mask for if-merging actions.
typedef enum IF_MERGE_ACTION {
  DO_NONE = 0x0,
  DO_IFMERGE = 0x1,
  DO_IFCOLLAPSE = 0x2,
  DO_IFFLIP = 0x4
};

// bit mask for if-merging pass
typedef enum IF_MERGE_PASS {
  PASS_NONE = 0x0,
  PASS_GLOBAL = 0x1,
  PASS_LOCAL = 0x2
};

// If-merging transformation.
class IF_MERGE_TRANS : public CFG_TRANS {
private:
  IF_MERGE_ACTION _action;

protected:
  IF_MERGE_PASS _pass;
  int _region_id;    // Id of currently processed region.

private:
  void Merge_CFG(SC_NODE *, SC_NODE *);
  void Merge_SC(SC_NODE *, SC_NODE *);
  BOOL Is_if_collapse_cand(SC_NODE * sc1, SC_NODE * sc2);

protected:
  void Set_region_id(int i) { _region_id = i; }
  SC_NODE * Do_merge(SC_NODE *, SC_NODE *);
  BOOL      Is_candidate(SC_NODE *, SC_NODE *, BOOL);
  void Clear(void);

public:
  void Top_down_trans(SC_NODE * sc);
  IF_MERGE_TRANS(void) { Clear(); }
  IF_MERGE_TRANS(COMP_UNIT * i) { Clear(); Set_cu(i); }
  IF_MERGE_TRANS(const IF_MERGE_TRANS&);
  ~IF_MERGE_TRANS(void) {};
  void Set_pass(IF_MERGE_PASS i) { _pass = i; }
};

// Proactive loop fusion transformation.
class PRO_LOOP_FUSION_TRANS : virtual public IF_MERGE_TRANS {
private:
  MAP  *_loop_depth_to_loop_map;  // map from SC tree depth to a list of SC_LOOPs, scratch field
  SC_LIST * _loop_list;          // a list of SC_LOOPs, scratch field
  int _last_class_id;
  BOOL _edit_loop_class;

private:
  void Reset_loop_class(SC_NODE *, int);
  void Find_loop_class(SC_NODE *);
  void Collect_classified_loops(SC_NODE *);
  BOOL Is_cand_type(SC_TYPE type) { return ((type == SC_IF) || (type == SC_LOOP)); }
  void Find_cand(SC_NODE *, SC_NODE **, SC_NODE **, SC_NODE *);
  BOOL Traverse_trans(SC_NODE *, SC_NODE *);
  BOOL Is_delayed(SC_NODE *, SC_NODE *);
  BOOL Is_worthy(SC_NODE *);
  void Nonrecursive_trans(SC_NODE *, BOOL);
  int New_class_id() { _last_class_id ++; return _last_class_id; }

protected:
  void Clear();

public:
  PRO_LOOP_FUSION_TRANS(void) { Clear(); }
  PRO_LOOP_FUSION_TRANS(COMP_UNIT * i) { Clear(); Set_cu(i); }
  void Top_down_trans(SC_NODE *);
  void Classify_loops(SC_NODE *);
};

// bit mask for proactive loop interchange actions.
typedef enum PRO_LOOP_INTERCHANGE_ACTION {
    DO_INTERCHANGE_NONE = 0x0,
    DO_TREE_HEIGHT_RED = 0x1,  // do if-condition tree height reduction
    DO_IF_COND_DIS = 0x2,  // do if-condition distribution
    DO_REV_LOOP_UNS = 0x4,  // do reverse loop-unswitching
    DO_LOOP_UNS = 0x8  // do loop-unswitching
};

typedef enum CANON_ACTION {
    CANON_NONE = 0x0,
    SPLIT_IF_HEAD = 0x1, // Split head of SC_IF so that it only contains one statement.
    HEAD_DUP = 0x2, // Head duplicate preceding siblings of SC_IF's head.
    TAIL_DUP = 0x4  // Tail duplicate SC_IF's merge.
};

// Proactive loop interchange transformation.
class PRO_LOOP_INTERCHANGE_TRANS : virtual public IF_MERGE_TRANS {
private:
    MAP * _def_cnt_map; // hash aux id to def count
    STACK<SC_NODE *> * _outer_stack; // a stack of outer SC_LOOPs. 
    STACK<SC_NODE *> * _inner_stack; // a stack of inner SC_LOOPs. 
    STACK<SC_NODE *> * _local_stack; // scratch field.
    STACK<SC_NODE *> * _restart_stack; // a stack of SC_NODEs where restarting occurs.
    STACK<SC_NODE *> * _tmp_stack; // scratch field.
    int _action;
    MAP * _def_map; //  Map symbol auxiliary Id to definition WN *.
    STACK<SC_NODE *> * _unlink_sc; // A stack of unlinked SC_NODE *.

private:
    int Nonrecursive_trans(SC_NODE *, SC_NODE *);
    BOOL Is_candidate(SC_NODE *, SC_NODE *);
    WN * Get_cond(SC_NODE *, BOOL);
    WN * Merge_cond(WN *, WN *, OPERATOR);
    BOOL Do_if_cond_tree_height_reduction(SC_NODE *, SC_NODE *);
    BOOL Do_loop_unswitching(SC_NODE *, SC_NODE *);
    BOOL Do_reverse_loop_unswitching(SC_NODE *, SC_NODE *, SC_NODE *);
    SC_NODE * Do_loop_dist(SC_NODE *, BOOL);
    BOOL Do_if_cond_dist(SC_NODE *);
    BOOL Can_reorder_cond(WN *, WN *);
    void Do_hoist(SC_NODE * sc1, SC_NODE * sc2);
    void Do_if_cond_wrap(BB_NODE *, SC_NODE *, BOOL);
    BOOL Check_sibling(SC_NODE *, SC_NODE *);
    void Do_canon(SC_NODE *, SC_NODE *, int);
    void Remove_block(SC_NODE *);
    void Remove_block(BB_NODE *);
    void Invalidate_invar(SC_NODE *);
    void Invalidate_invar(BB_NODE *);
    SC_NODE * Split(SC_NODE *, SC_NODE *);
    SC_NODE * Do_pre_dist(SC_NODE *, SC_NODE *);
    SC_NODE * Do_partition(SC_NODE *);
    SC_NODE * Do_loop_fusion(SC_NODE *);
    void Do_lock_step_fusion(SC_NODE *, SC_NODE *);
    BOOL Do_loop_interchange(SC_NODE *, SC_NODE *);
    void Swap_stmt(BB_NODE *, BB_NODE *);
    void Add_def_map(AUX_ID, WN *);
    void Copy_prop(SC_NODE * sc);
    void Copy_prop(BB_NODE * bb);
    void Copy_prop(WN *);
    BOOL Get_unique_ref(SC_NODE *, SC_NODE *, WN **);
    BOOL Get_unique_ref(BB_NODE *, SC_NODE *, WN **);
    BOOL Get_unique_ref(WN *, SC_NODE *, WN **);
    BOOL Compare_trees(WN *, SC_NODE *, WN *, SC_NODE *);
    BOOL Hoist_succ_blocks(SC_NODE *);
    WN * Get_index_load(SC_NODE *);
    WN * Get_index_load(WN *, ST *);
    BOOL Is_perfect_loop_nest(SC_NODE *);
    BOOL Check_iteration(SC_NODE *, SC_TYPE, SC_NODE *);
    BOOL Check_index(SC_NODE *);
    SC_NODE * Find_fusion_buddy(SC_NODE *, STACK<SC_NODE *> *);
    void Do_split_if_head(SC_NODE *);
    SC_NODE * Find_dist_cand(SC_NODE *);
    BOOL Can_interchange(SC_NODE *, SC_NODE *);
    BOOL Can_fuse(SC_NODE *);
    unsigned long Get_def_cnt(AUX_ID i) 
        { return (unsigned long) _def_cnt_map->Get_val((POINTER) i); }

protected:
    void Clear();
    void Delete();

public:
    PRO_LOOP_INTERCHANGE_TRANS(void) { Clear(); }
    PRO_LOOP_INTERCHANGE_TRANS(COMP_UNIT * i) { Clear(); Set_cu(i); }
    BOOL Top_down_trans(SC_NODE *, BOOL);
    void Hash_def_cnt_map(SC_NODE *);
    void Hash_def_cnt_map(BB_NODE *);
};

// Proactive loop transformations.
class PRO_LOOP_TRANS :  public PRO_LOOP_FUSION_TRANS, 
                        public PRO_LOOP_INTERCHANGE_TRANS {
private:
    void Clear();

public:
    PRO_LOOP_TRANS(void) { Clear(); }
    PRO_LOOP_TRANS(COMP_UNIT * i) { Clear(); Set_cu(i); }
    void Delete();
};

#endif /*opt_proactive_INCLUDED*/
