/*
 *
 * Copyright (C) 2006, 2007, Tsinghua University.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.  
 *
 * For further information regarding this notice, see:
 * http://hpc.cs.tsinghua.edu.cn
 *
 */

#ifdef OSP_OPT

#define __STDC_LIMIT_MACROS

#include <queue>
#include <ext/hash_map>
#include <ext/hash_set>
#include "wn_util.h"
#include "ipo_parent.h"
#include "ipa_devirtual.h"
#include "ipc_ty_hash.h"
#include "ipl_summarize.h"
#include "ipl_summarize_util.h"
#include "ir_reader.h"

using std::queue;
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;

static hash_set <PU_IDX> pu_of_node;
static hash_set <TY_INDEX> visited_class;
static hash_set <TY_INDEX> live_class;
static hash_map <PU_IDX, NODE_INDEX> pu_node_index_table;
static queue <NODE_INDEX> list_method;

void
Insert_Vfunction_Target(ST_IDX &origin, ST_IDX target) {
    FmtAssert(target > ST_IDX_ZERO,
              ("Invalid ST_IDX for finding virtual function target."));
    switch (origin) {
        case MORE_THAN_ONE_VFUNCTION_CANDIDATE:
            return;
        case NO_VFUNCTION_CANDIDATE:
            origin = target;
            return;
        default:
            if (origin != target)
                origin = MORE_THAN_ONE_VFUNCTION_CANDIDATE;
        return;
    }
};

// Add the corresponding function ST of all live subclasses 
// into a target set of a callsite. 
static void  
IPA_set_virtual_call_targets(TY_INDEX class_type, 
                             ST_IDX &target,
                             size_t offset)
{
    visited_class.insert(class_type);

    // If the static type is instantiated, 
    // set it as a target and push it into list_method.
	// Otherwise, do not care it.
    if (IN_SET(live_class, class_type)) {
        // will visit the target, push it into list_method
        ST_IDX st_idx = IPA_class_virtual_function(class_type, offset);
        // st_idx may be zero. Do not process the ST under the condition. 
        // A case is that ST is an exception and its vptr may not have INITO.
        if (st_idx > ST_IDX_ZERO && !ST_is_pure_vfunc(&St_Table[st_idx])) {
            Insert_Vfunction_Target(target, st_idx);
            PU_IDX pui = ST_pu(St_Table[st_idx]);
            if (IN_SET(pu_of_node, pui))
                list_method.push(pu_node_index_table[pui]);
        }
    }

    // search all the subclasses of class_type
    int num_subclass = IPA_Class_Hierarchy->Get_Num_Sub_Classes(class_type);
    for (int i = 0; i < num_subclass; i++) {
        TY_INDEX subclass = IPA_Class_Hierarchy->Get_Sub_Class(class_type, i);
        // Only visit the unvisited classes
        if (NOT_IN_SET(visited_class, subclass)) 
            IPA_set_virtual_call_targets(subclass, target, offset);
    }
    return;
}

// Find the ST_IDX of a virtual function 
// by the class type and its offset in virtual table.
ST_IDX
IPA_class_virtual_function(TY_INDEX class_type, size_t offset) {
    INITV_IDX initv_idx = TY_vtable(make_TY_IDX(class_type));
    if (initv_idx <= INITV_IDX_ZERO)
        return ST_IDX_ZERO;
    do {
#ifndef TARG_IA64
        FmtAssert(INITV_kind(initv_idx) == INITVKIND_SYMOFF,
                  ("Not INITVKIND_SYMOFF."));
#else
        FmtAssert(INITV_kind(initv_idx) == INITVKIND_SYMIPLT,
                  ("Not INITVKIND_SYMIPLT."));
#endif
        if (offset == 0)
            return INITV_st(initv_idx);
	offset -= (Pointer_Size << 1);
        initv_idx = INITV_next(initv_idx);  
    } while (initv_idx > INITV_IDX_ZERO);
    FmtAssert(FALSE, ("Initv entry not found."));
    return ST_IDX_ZERO;
}

// Main function of devirtualizaton. 
void
IPA_devirtualization() {

    hash_set <NODE_INDEX> node_visited;
    vector <callsite_targets_t> live_callsite;

    // Set up pu->node table, and find out the main function entry.
    // If a pu has no corresponding ipa node, it must be an external function,
    // such as "new". Do not deal with these external functions.
    IPA_NODE_ITER iter(IPA_Call_Graph, DONTCARE);
    for (iter.First(); !iter.Is_Empty(); iter.Next()) {
        IPA_NODE *node = iter.Current();
        if (node) {
            PU_IDX pui = ST_pu(node->Func_ST());
            pu_node_index_table[pui] = node->Node_Index();
            pu_of_node.insert(pui);
            if (PU_is_mainpu(pui))
                list_method.push(node->Node_Index());
        }
    }

    while (!list_method.empty()) {

        NODE_INDEX method_idx = list_method.front();
        list_method.pop();

        if (IN_SET(node_visited, method_idx))
            continue;
        else
            node_visited.insert(method_idx);

        IPA_NODE *method = IPA_Call_Graph->Graph()->Node_User(method_idx);
        SUMMARY_PROCEDURE* method_summary = method->Summary_Proc();
        SUMMARY_CALLSITE* callsite_array = 
            IPA_get_callsite_array(method) + method_summary->Get_callsite_index();

        WN *wn_start = method->Whirl_Tree(FALSE);
        // For the simplest function, WHIRL tree may be empty.
        if (!wn_start)
            continue;
        for(WN_ITER *wni = WN_WALK_TreeIter(wn_start);
            wni != NULL; wni = WN_WALK_TreeNext(wni))
        {
            WN *wn = WN_ITER_wn(wni);

            // VFCALL and PICCALL should not appear.
            FmtAssert(!WN_operator_is(wn, OPR_VFCALL), 
                    ("VFCALL is found in devirtualization."));
            FmtAssert(!WN_operator_is(wn, OPR_PICCALL),
                    ("PICCALL is found in devirtualization"));

            // Only process call and icall, intrinsic call can be omitted
            if (!(WN_operator_is(wn, OPR_CALL) || WN_operator_is(wn, OPR_ICALL)))
                continue;

            if (WN_Call_Is_Virtual(wn)) {
                callsite_targets_t cst;
                for(int i = 0; i < method_summary->Get_callsite_count(); i++) {
                    if (callsite_array[i].Get_map_id() == WN_map_id(wn)) {
                        cst.callsite = &callsite_array[i];
                        break;
                    }
                }
                cst.wn = wn;
                WN *last = WN_kid(wn, WN_kid_count(wn)-1);
                if (WN_operator_is(last, OPR_ADD)) {
                    FmtAssert(WN_kid_count(last) == 2,
                                 ("Incorrect virtual call site."));
                    WN *addr = WN_kid0(last);
                    WN *ofst = WN_kid1(last);
                    FmtAssert(WN_operator_is(addr, OPR_ILOAD) || WN_operator_is(addr, OPR_LDID),
                               ("Virtual function call does not use ILOAD or LDID."));
                    FmtAssert(WN_operator_is(ofst, OPR_INTCONST),
                               ("Virtual table offset is not INTCONST."));
                    cst.static_ty = TY_IDX_index(WN_ty(addr));
                    cst.offset = WN_const_val(ofst);
                }
                else {
                    // Original WN generated by front end must be OPR_ILOAD.
                    // The OPR_ILOAD may be optimized to OPR_LDID by WOPT.
                    FmtAssert(WN_operator_is(last, OPR_ILOAD) || WN_operator_is(last, OPR_LDID),
                        ("Virtual function call does not use ILOAD or LDID."));
                    cst.static_ty = TY_IDX_index(WN_ty(last));
                    cst.offset = WN_load_offset(last);
                }
                cst.method_node = method;
                cst.target = NO_VFUNCTION_CANDIDATE;
                visited_class.clear();
                IPA_set_virtual_call_targets(cst.static_ty, cst.target, cst.offset);
                live_callsite.push_back(cst);
            }
            else {
                if (WN_operator_is(wn, OPR_ICALL)) {
                    // If the call is indirect call and not virtual function call,
                    // visit all PUs have the same prototype as the type of call.
                    TY_IDX tyi = WN_ty(wn);
                    for (int i = 1; i < PU_Table_Size(); i++) 
                        if (TY_IDX_index(tyi) == TY_IDX_index(PU_prototype(Pu_Table[i]))) 
                           if (IN_SET(pu_of_node, i))
                               list_method.push(pu_node_index_table[i]);
                }
                else {
                    ST_IDX func_st_idx = WN_st_idx(wn);
                    ST *func_st = &St_Table[func_st_idx];
                    PU_IDX pui = ST_pu(func_st);
                    if (PU_is_constructor(Pu_Table[pui])) {
                        // If new a class, add its corresponding method 
                        // into the target sets of the live callsites.
                        TY_INDEX class_ty = TY_IDX_index(TY_baseclass(ST_pu_type(func_st)));
                        if (NOT_IN_SET(live_class, class_ty)) {
                            live_class.insert(class_ty);
                            for (vector <callsite_targets_t>::iterator i = live_callsite.begin();
                                i != live_callsite.end(); i++) {
                                if (IPA_Class_Hierarchy->Is_Ancestor(i->static_ty, class_ty)) {
                                    // check if it is a pure virtual function
                                    ST_IDX st_idx = IPA_class_virtual_function(class_ty, i->offset);
                                    if (st_idx > ST_IDX_ZERO && !ST_is_pure_vfunc(&St_Table[st_idx]))
                                        Insert_Vfunction_Target(i->target, st_idx);
                                }
                            }
					    }
				    }
                    if (IN_SET(pu_of_node, pui))
                        list_method.push(pu_node_index_table[pui]);
                }
            }
        }
    }

    for (size_t i = 0; i < live_callsite.size(); i++) {
        // Convert the virtual function call to direct call
        // if the number of targets is 1.
        if (live_callsite[i].target != NO_VFUNCTION_CANDIDATE &&
            live_callsite[i].target != MORE_THAN_ONE_VFUNCTION_CANDIDATE) {
            WN *wn = live_callsite[i].wn;
            IPA_NODE *method = live_callsite[i].method_node;
            ST *st_callee = &St_Table[live_callsite[i].target];
            TY_INDEX ty_callee = ST_pu_type(st_callee);

            FmtAssert(!ST_is_pure_vfunc(st_callee), 
                    ("Convert indirect callsite to pure virtual function."));
            DevWarn("Convert indirect call to direct call %s", 
                    &Str_Table[ST_name_idx(*st_callee)]);
	    
            WN *new_wn = WN_generic_call(OPR_CALL, WN_rtype(wn), 
                                         WN_desc(wn), WN_kid_count(wn)-1, st_callee);
            for(size_t j = 0; j < WN_kid_count(new_wn); j++ ){
                WN_kid(new_wn, j) = WN_kid(wn, j);
            }
            WN_set_flag(new_wn, WN_flag(wn));
            WN_Reset_Call_Is_Virtual(new_wn);
	    
            WN *parent = WN_Get_Parent(wn, method->Parent_Map(), method->Map_Table());
            WN_Set_Parent(new_wn, parent, method->Parent_Map(), method->Map_Table());
            WN_INSERT_BlockAfter(parent, wn, new_wn);
            WN_EXTRACT_FromBlock(parent, wn);

            SUMMARY_CALLSITE *callsite = live_callsite[i].callsite;
            for (IPA_ICALL_LIST::iterator iter = method->Icall_List().begin();
                 iter != method->Icall_List().end(); iter++) 
            {
                if ((*iter)->Callsite() == callsite) {
                    method->Icall_List().erase(iter);
                    break;
                }
            }
            callsite->Reset_icall_slot();
            callsite->Reset_func_ptr();
            callsite->Set_param_count(WN_num_actuals(new_wn));
            callsite->Set_return_type(WN_rtype(new_wn));
            callsite->Set_callsite_freq();
            callsite->Set_probability(-1);
            callsite->Set_symbol_index(0); 

            IPA_EDGE* edge = IPA_Call_Graph->Add_New_Edge(callsite, 
                                live_callsite[i].method_node->Node_Index(),
                                pu_node_index_table[ST_pu(st_callee)]);
        }
    }
}

#endif

