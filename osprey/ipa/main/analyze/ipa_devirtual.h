/*
 * Copyright (C) 2009 Advanced Micro Devices, Inc.  All Rights Reserved.
 */

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

#ifndef cxx_ipa_devirtual_INCLUDED
#define cxx_ipa_devirtual_INCLUDED

/*
 * Main function of devirtualization in IPA phase
 */
extern void
IPA_devirtualization(); // earlier version

extern void
IPA_Fast_Static_Analysis_VF ();

#include <list>
#include <ext/hash_set>
#include <ext/hash_map>
#include <string>
using std::list;
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;

class IPA_VIRTUAL_FUNCTION_TRANSFORM {
    public:
// After analysis, an object of the following type is available
// for each transformable virtual function       
    class VIRTUAL_FUNCTION_CANDIDATE {
        public:
            // Replace icall with call
            BOOL Single_Callee;
// The virtual function's call site summary
            SUMMARY_CALLSITE *Virtual_Call_Site;
// Dummy call site summary that will now be set to fun_st_idx
            SUMMARY_CALLSITE *Dummy_Call_Site;
// Caller node containing the virtual function call site
            IPA_NODE *Caller;
// The virtual table of the instance attached to callee
            WN* Virtual_Table;
// The ST_IDX of the resolved function contained in 
// above WN* irtual_table
            ST_IDX Transform_Function_ST_IDX;
    };
// mapping NODE_INDEX indices against 
// a list of transform candidates 
    hash_map <NODE_INDEX, list<VIRTUAL_FUNCTION_CANDIDATE> > 
        Node_Transform_Lists;

// optimization controlling 
    bool Class_Hierarchy_Analysis, Class_Type_Analysis;
// transformation initializing and finalizing
    void Initialize_Virtual_Function_Transform ();
    void Finalize_Virtual_Function_Transform ();

// fixup
    void Fixup_Virtual_Function_Callsites ();
    void Fixup_Virtual_Function_Callsites_Per_Node (IPA_NODE *method);

// prepare for optimization by obtaining constructors
// and building class hierarchy graph
    void Prepare_Virtual_Function_Transform ();
    
// main function for transform 
    void Transform_Virtual_Functions ();
    
// analyze and transform
    void Transform_Virtual_Functions_Per_Node (IPA_NODE* method);
    void Apply_Virtual_Function_Transform (VIRTUAL_FUNCTION_CANDIDATE);

// collect constructor calls from call graph
    hash_set <TY_INDEX> Constructed_Types;
    hash_map <TY_INDEX, PU_IDX> Constructor_Map;
    TY_INDEX Get_Constructor_Type (SUMMARY_SYMBOL *func_sym);
    void Identify_Constructors ();

// A mapping between PUs and NODE_INDEX indices. 
// The other way mapping is available directly 
// but this is not   
    hash_map <PU_IDX, NODE_INDEX> pu_node_index_map;
    void Build_PU_NODE_INDEX_Map();

// utils
    int Get_Callsite_Count (IPA_NODE* method);

    typedef hash_map <WN_MAP_ID, WN *> WN_IPA_MAP;
    hash_map <NODE_INDEX, WN_IPA_MAP> Node_Virtual_Function_Whirl_Map;
    void Build_Virtual_Function_Whirl_Map (
            IPA_NODE* method, 
            WN_IPA_MAP& a_wn_ipa_map);

    hash_set<TY_INDEX> Identify_Instances_From_Subclass_Hierarchy (
            TY_INDEX declared_class);

    void Identify_Virtual_Function (
            hash_set<TY_INDEX> constructed_types_set, 
            SUMMARY_CALLSITE *callsite,
            VIRTUAL_FUNCTION_CANDIDATE& vcand);

    void Locate_Virtual_Function_In_Virtual_Table (
            IPA_NODE *constructor,
            WN* vtab, size_t func_offset, 
            VIRTUAL_FUNCTION_CANDIDATE& vcand);

// debug 
    bool Enable_Debug;
    FILE *Virtual_Whirls;
    FILE *Transformed_Whirls;
    void Dump_Constructors ();
    typedef hash_map <NODE_INDEX, hash_map <WN_MAP_ID, hash_set<TY_INDEX> > > VIRTUAL_FUNCTION_DEBUG_DATA;
    VIRTUAL_FUNCTION_DEBUG_DATA Transform_Debug_Data;
    void Dump_Virtual_Function_Transform_Candidates ();
    hash_map <NODE_INDEX, IPA_NODE*> Optimized_Methods_By_NODE_INDEX;

// statistics
    bool Enable_Statistics;
    int Num_VFs_Count;
    int Class_Hierarchy_Transform_Count;
    int Class_Instance_Transform_Count;
    void Print_Statistics ();
    void Miss_Hit_Profile ();
    void Histogram_Statistics ();
    hash_map <int, int > Num_Instances;
    hash_map <int, int > Class_Hierarchy_Depth;
    hash_map <int, list<std::string> > Miss_Hit_Tag;
    void Update_Class_Hierarchy_Depth (int index);
    void Update_Instances (int index);

// profiling
    bool Enable_Profile;
    ST_IDX Miss_ST_IDX, Hit_ST_IDX;
};

#endif

