/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

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

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/


// -*-C++-*-

/**
*** Description:
***
***	This file contains classes which implement a transaction log for 
***	PROMPF. The transaction log is used to map original construct ids 
***     to transformed construct ids.  Currently the transaction log is 
***	only implemented for preopt and LNO up through automatic parallel-
***	ization, but excluding tiling. 
***
*** Exported types and functions: 
***
***	class PROMPF_LINES
***
***	  Implements a collection of line number ranges. Each range is 
***	  stored in Low(i)..High(i).  Line numbers must be non-negative 
***	  integers.  In every case, Low(i) <= High(i) and High(i) < 
***	  Low(i+1).  There are a total of Sections() collections of ranges.
***	      
***	class PROMPF_TRANS_TYPE
***
*** 	    Implements transaction types for the transaction log.  The 
***	    following transactions are supported: 
***
***		MPF_UNKNOWN: Unknown transaction type. 
***		MPF_MARK_F90_LOWER: Statrt recording trans for F90 lowering
***		MPF_MARK_OMP: Start recording trans for OMP prelowering
***		MPF_MARK_PREOPT: Start recording trans for preopt
***		MPF_MARK_PRELNO: Start recording trans for LNO before 
***		  and including parallelization
***		MPF_MARK_POSTLNO: Start recording trans for LNO after 
***		  parallelization
***		MPF_ELIMINATION: Eliminate a loop during LNO 
***	       	MPF_FUSION: Fuse together two loops 
***	        MPF_FISSION: Perform fission on a loop nest 
***		MPF_DISTRIBUTION: Apply distribution to an SNL
***	        MPF_INTERCHANGE: Permute perfectly nested set of loops
***		MPF_PRE_PEEL: Peel a loop nest and place peeled portion 
***		    before the loop nest 
***		MPF_POST_PEEL: Peel a loop nest and place peeled portion
***		    after the loop nest. 
***		MPF_MP_TILE: Processor tile a loop for parallelization
***		MPF_DSM_TILE: Processor tile a loop with affinity for 
***		    some lego reshaped array 
***		MPF_DONEST_OUTER_TILE: Create a processor tile while  
***		    converting a doacross nest to a doacross 
***		MPF_DONEST_MIDDLE_TILE: Create a middle tile loop while
***		    converting a doacross nest to a doacross
***		MPF_DSM_LOCAL: Create a loop to copy out an array which 
***		    is a lego reshaped array appearing in a LASTLOCAL 
*** 		    clause on an MP loop 
***		MPF_DSM_IO: Create a loop to copy a lego reshaped array 
***		    into a temp array or vice versa. 
***	        MPF_SINGLE_PROCESS: Create a single process. 
***		MPF_MP_VERSION: Make serial and parallel versions of an
***		    doacross loop or parallel region.
***		MPF_PARALLEL_REGION: Create a parallel region. 
***		MPF_HOIST_MESSY_BOUNDS: Create clones while hoisting 
***		    messy bounds
***		MPF_DOACROSS_SYNC: Create a synchronization loop for a 
***		    doacross with automatically generated synchronization
***		MPF_DOACROSS_OUTER_TILE: Create a parallel outer tile loop 
***		    for doacross with synchronization  
***		MPF_DOACROSS_INNER_TILE: Create a serial inner tile loop 
***		    for doacross with synchronization
***		MPF_REMOVE_UNITY_TRIP: Remove a unity trip loop.
***		MPF_CACHE_WINDDOWN: Create a windown version of the cache
***		    tiled loops. 
***		MPF_INTERLEAVED_WINDDOWN: Create a windown version of the 
***		    middle loop of an MP tiled loop with interleaved schedule
***		MPF_GENERAL_VERSION: Version a loop nest so that we can 
***		    apply a general SNL transformation.  
***		MPF_CACHE_TILE: Tile a loop for the cache. 
***		MPF_REGISTER_WINDDOWN: Create a winddown version of 
***		    register tiled loop. 
***		MPF_REGISTER_SSTRIP: Indicate that a small strip version 
***		    of a nest of register tiled loops had been created. 
***		MPF_REGISTER_TILE: Indicate that a loop has been register 
***		    tiled.  
***		MPF_REGISTER_STARTUP: Create a startup loop nest for 
***		    general register tiling. 
***		MPF_REGISTER_SHUTDOWN: Create a shutdown loop nest for 
***		    general register tiling. 
***		MPF_SE_TILE: Indicate that a loop has been scalar expansion
***		    tiled. 
***		MPF_SE_CACHE_TILE: Indicate that a loop has been tiled for 
***		    scalar expansion and cache. 
***		MPF_INNER_FISSION: Indicate that an inner loop has been 
***		    fissioned into a set of inner loops
***		MPF_GATHER_SCATTER: Indicate that an inner loop has been 
***		    fissioned into a set of inner loops during gather-scatter
***		MPF_VINTR_FISSION: Indicate that an inner loop has been
***		    fissioned into a set of inner loops during vector 
***		    intrinsic fission 
***		MPF_PREFETCH_VERSION: Version a loop nest for prefetching
***	 	MPF_OMPL_SECTIONS_LOOP: Lower an OMP SECTIONS construct to 
***		    a loop 
***		MPF_OMPL_ELIM_SECTION: Eliminate a SECTION because the 
***		    corresponding SECTIONS clause was converted into a loop
***		MPF_OMPL_ATOMIC_CSECTION: Lower an OMP ATOMIC directive to 
***		    a CRITICAL SECTION 
***		MPF_OMPL_ATOMIC_SWAP: Lower an OMP ATOMIC directive to a 
***		    compare and swap operation 
***		MPF_OMPL_ATOMIC_FETCHOP: Lower an OMP ATOMIC directive to 
***		    a fetch and op intrinsic. 
***		MPF_OMPL_MASTER_IF: Lower an OMP MASTER directive to an if. 
***		MPF_OMPL_FETCHOP_ATOMIC: Create an atomic directive for 
***		    an unsupported fetch and op intrinsic. 
***		MPF_F90_ARRAY_STMT: Create a loop from an F90 array statement 
***		MPF_OUTER_SHACKLE: Create an outer shackle loop
***		MPF_INNER_SHACKLE: Create an inner shackle loop from an 
***		    original loop 
***		MPF_PREOPT_CREATE: Create a DO_LOOP by raising a WHILE loop 
***		    or unstructured control flow loop.
***
***	class PROMPF_CHAIN_TYPE 
***
*** 	    For each enumeration value of PROMPF_TRANS_TYPE, assigns a 
***	    value: 
***		MPF_CHAIN_INVALID: For MPF_UNKNOWN. 
***	        MPF_CHAIN_TRANSFORM: Indicates that loops are created 
***		  or destroyed during the transformation
***		MPF_CHAIN_TRANSMIT: Indicates that loops are moved around
***		  during the transformation. 
*** 
***	PROMPF_CHAIN_TYPE prompf_chain[]
***
***	   Maintains the mapping from PROMPF_TRANS_TYPE to PROMPF_CHAIN_
***	   TYPE.
*** 
***	class PROMPF_TRANS
***
***	   Represents a transaction in the transformation log. 
***
***	class PROMPF_ID_TYPE
***
***	   Represents an original or transformed id in the transaction log.
***
***	enum PROMPF_TRANS_LOG
***
***	   One value for each of five transaction logs we currently 
***	   generate in PROMPF: 
***	     PTL_F90_LOWER: Transformations done during F90 lowering 
***	     PTL_OMP: Transformations done during OMP prelowering 
***	     PTL_PREOPT: Transformations done duting Pre-optimizer
*** 	     PTL_PRELNO: Transformations done in LNO before parallelization
***	     PTL_POSTLNO: Transformations done in LNO after parallelization 
***
***	class PROMPF_INFO
***
***	   Represents a transformation transaction log. 
***
*** 	WN_MAP Prompf_Id_Map; 
***
***	   The Id Map which folds the current valid of the id for each 
***	   of the PROMPF constructs. 
***
***     PROMPF_INFO* Prompf_Info; 
***
***	   Pointer to the global PROMPF_INFO used to store LNO's transfor- 
***	   mation transaction log.
***
***	PROMPF_ID_TYPE Prompf_Id_Type(WN* wn_ref, WN* wn_region, BOOL* is_first)
***
***	  Returns the PROMPF_ID_TYPE corresponding to the node 'wn_ref'.
***	  Returns MPID_UNKNOWN if this node is not one for which we should
***	  construct a PROMPF_ID.  Returns TRUE in 'is_first' if 'wn' should 
***	  be the first node with its PROMP map id, FALSE otherwise.  The 
***	  'wn_region' is the closest enclosing OPC_REGION node, if there 
***	  is one. 
***
***	void Prompf_Assign_Ids(WN* wn_old, WN* wn_new, STACK<INT>* old_stack,
***	  STACK<INT>* new_stack, BOOL copy_ids, INT max_ids=INT32_MAX);
***
***	  Traverse the old code 'wn_old' and the new code 'wn_new'
***	  simulanteously, and assign new PROMPF ids to nodes in the new code
***	  which don't have them, but which correspond to nodes in the old code.
***	  When a new id is assigned in the new code, push the old WN* on the
***	  'old_stack' and the new WN* on the 'new_stack'.  Assign a maximum of
***	  'max_ids' new ids in the new code.   If 'copy_ids', then duplicate 
***	  the ids in the new code rather than assigning new ids.
**/

#ifndef prompf_INCLUDED
#define prompf_INCLUDED "prompf.h"
	
enum PROMPF_TRANS_TYPE {
  MPF_UNKNOWN, 
  MPF_MARK_F90_LOWER, 
  MPF_MARK_OMP, 
  MPF_MARK_PREOPT, 
  MPF_MARK_PRELNO, 
  MPF_MARK_POSTLNO, 
  MPF_ELIMINATION, 
  MPF_FUSION, 
  MPF_FISSION, 
  MPF_DISTRIBUTION, 
  MPF_INTERCHANGE, 
  MPF_PRE_PEEL,
  MPF_POST_PEEL, 
  MPF_MP_TILE, 
  MPF_DSM_TILE, 
  MPF_DONEST_OUTER_TILE, 
  MPF_DONEST_MIDDLE_TILE,
  MPF_DSM_LOCAL, 
  MPF_DSM_IO,
  MPF_SINGLE_PROCESS,
  MPF_MP_VERSION, 
  MPF_PARALLEL_REGION,
  MPF_HOIST_MESSY_BOUNDS,
  MPF_DOACROSS_SYNC, 
  MPF_DOACROSS_OUTER_TILE, 
  MPF_DOACROSS_INNER_TILE,
  MPF_REMOVE_UNITY_TRIP,
  MPF_CACHE_WINDDOWN,
  MPF_INTERLEAVED_WINDDOWN,
  MPF_GENERAL_VERSION,
  MPF_CACHE_TILE,
  MPF_REGISTER_WINDDOWN,
  MPF_REGISTER_SSTRIP, 
  MPF_REGISTER_TILE, 
  MPF_REGISTER_STARTUP, 
  MPF_REGISTER_SHUTDOWN,
  MPF_SE_TILE, 
  MPF_SE_CACHE_TILE,
  MPF_INNER_FISSION,
  MPF_GATHER_SCATTER,
  MPF_VINTR_FISSION,
  MPF_PREFETCH_VERSION,
  MPF_OMPL_SECTIONS_LOOP,
  MPF_OMPL_ELIM_SECTION,
  MPF_OMPL_ATOMIC_CSECTION,
  MPF_OMPL_ATOMIC_SWAP,
  MPF_OMPL_ATOMIC_FETCHOP,
  MPF_OMPL_MASTER_IF,
  MPF_OMPL_FETCHOP_ATOMIC,
  MPF_F90_ARRAY_STMT,
  MPF_OUTER_SHACKLE, 
  MPF_INNER_SHACKLE,
  MPF_PREOPT_CREATE
}; 

enum PROMPF_CHAIN_TYPE { 
  MPF_CHAIN_INVALID, 
  MPF_CHAIN_TRANSFORM, 
  MPF_CHAIN_TRANSMIT
}; 

extern PROMPF_CHAIN_TYPE prompf_chain[]; 

class PROMPF_LINES { 
private: 
  STACK<INT> _low; 
  STACK<INT> _high; 
  MEM_POOL* _pool; 
public: 
  PROMPF_LINES(MEM_POOL* pool): _pool(pool), _low(pool), _high(pool) {}
  PROMPF_LINES(WN* wn_tree, MEM_POOL* pool); 
  INT Sections(); 
  INT Low(INT i);
  INT High(INT i);  
  void Print(FILE *fp); 
  INT Print_Compact(FILE *fp, BOOL print_brackets=TRUE); 
  void Add_Line(INT line); 
  void Add_Lines(WN* wn_tree);
}; 

class PROMPF_TRANS {
private: 
  PROMPF_TRANS_TYPE _type; 
  STACK<INT> _old_loops; 
  STACK<INT> _new_loops; 
  STACK<INT> _prev_loops; 
  STACK<PROMPF_LINES*> _old_lines; 
  STACK<PROMPF_LINES*> _new_lines; 
  char* _index_name; 
  MEM_POOL* _pool; 
public: 
  PROMPF_TRANS(MEM_POOL* pool): _pool(pool), _type(MPF_UNKNOWN), 
    _old_loops(pool), _new_loops(pool), _prev_loops(pool), 
    _old_lines(pool), _new_lines(pool), _index_name(NULL) {} 
  PROMPF_TRANS(STACK<INT>* old_loops, STACK<INT>* new_loops,
    STACK<INT>* prev_loops, STACK<PROMPF_LINES>* new_lines, 
    char* index_name, MEM_POOL* pool); 
  PROMPF_TRANS_TYPE Type() {return _type;}
  INT Old_Loop_Count() {return _old_loops.Elements();}
  INT New_Loop_Count() {return _new_loops.Elements();}
  INT Prev_Loop_Count() {return _prev_loops.Elements();}
  INT Old_Loop(INT i); 
  INT New_Loop(INT i); 
  INT Prev_Loop(INT i); 
  void Set_Type(PROMPF_TRANS_TYPE type) {_type = type;}
  void Add_Old_Loop(INT id) {_old_loops.Push(id);}
  void Add_New_Loop(INT id) {_new_loops.Push(id);} 
  void Add_Prev_Loop(INT id) {_prev_loops.Push(id);} 
  void Add_Old_Lines(PROMPF_LINES* pl) {_old_lines.Push(pl);}
  void Add_New_Lines(PROMPF_LINES* pl) {_new_lines.Push(pl);}
  void Add_Index_Name(char* index_name);
  void Print(FILE* fp); 
  void Print_Compact(FILE* fp); 
}; 

enum PROMPF_ID_TYPE { 
  MPID_UNKNOWN, 
  MPID_FUNC_ENTRY, 
  MPID_DO_LOOP, 
  MPID_PAR_REGION, 
  MPID_PAR_SECTION, 
  MPID_SECTION, 
  MPID_BARRIER, 
  MPID_SINGLE_PROCESS,
  MPID_CRITICAL_SECTION,
  MPID_MASTER,
  MPID_ORDERED, 
  MPID_PAR_SECTIONS, 
  MPID_ATOMIC
}; 

class PROMPF_ID { 
private: 
  PROMPF_ID_TYPE _type; 
  BOOL _valid; 
  INT _last_trans; 
  MEM_POOL* _pool; 
public: 
  PROMPF_ID(PROMPF_ID_TYPE type, BOOL valid, INT last_trans, MEM_POOL* pool): 
    _pool(pool), _type(type), _valid(valid), _last_trans(last_trans) {}
  BOOL Is_Valid() {return _valid;}
  BOOL Last_Trans() {return _last_trans;}
  void Invalidate() {_valid = FALSE;}
  void Validate() {_valid = TRUE;}
  void Set_Last_Trans(INT v) {_last_trans = v;} 
  void Print(FILE* fp, INT entry=0); 
};

enum PROMPF_TRANS_LOG {
  PTL_F90_LOWER, 
  PTL_OMP, 
  PTL_PREOPT, 
  PTL_PRELNO, 
  PTL_POSTLNO
}; 

class PROMPF_INFO { 
private: 
  BOOL _enabled; 
  INT _first_id; 
  STACK<PROMPF_TRANS*> _trans_stack; 
  STACK<PROMPF_ID*> _id_stack;   
  INT _trans_checkpoint; 
  PROMPF_TRANS* Trans(INT i) {return _trans_stack.Bottom_nth(i);}
  INT Last_Trans() {return _trans_stack.Elements() - 1;}
  void Reset_Last_Trans(INT old_id);
  PROMPF_ID* Id(INT i) {return _id_stack.Bottom_nth(i - _first_id);}
  INT First_Id() {return _first_id;}
  INT Last_Id() {return _first_id + _id_stack.Elements() - 1;}
  PROMPF_ID* Remove_Id() {return _id_stack.Pop();}
  PROMPF_INFO(MEM_POOL* pool): _pool(pool), _enabled(FALSE), 
    _trans_stack(pool), _id_stack(pool), _trans_checkpoint(-1)
    {_trans_stack.Clear(); _id_stack.Clear();}
  void Add_Trans(PROMPF_TRANS* pt);
  PROMPF_TRANS* Remove_Trans();
  void Add_Id(PROMPF_ID* pi) {_id_stack.Push(pi);} 
  void Update_Id(INT i, INT trans_index) 
    { _id_stack.Bottom_nth(i - _first_id)->Set_Last_Trans(trans_index);} 
  void Push_Original_Loops(INT id_trans, INT last_trans, 
    STACK<INT>* oloop_stack);
  BOOL Check_Old_Ids(INT old_ids[], INT new_ids[], INT nloops); 
  BOOL Check_New_Ids(INT new_ids[], INT nloops); 
  void Prompf_Info_Traverse(WN* wn_tree, WN* wn_region); 
  void Check_Traverse(FILE* fp, WN* wn_tree, BOOL ids[], WN* wn_region); 
  MEM_POOL* _pool; 
public: 
  PROMPF_INFO(WN* wn_func, MEM_POOL* pool);
  BOOL Is_Enabled() {return _enabled;} 
  void Disable() {_enabled = FALSE;}
  void Enable() {_enabled = TRUE;}
  void Mark_F90_Lower(); 
  void Mark_Omp(); 
  void Mark_Preopt(); 
  void Mark_Prelno(); 
  void Mark_Postlno(); 
  void Elimination(INT old_loop); 
  void Undo_Elimination();
  void Fusion(INT old_loops[], INT new_loop);
  void Undo_Fusion();
  void Fission(INT old_loops[], PROMPF_LINES* old_lines[], INT new_loops[], 
    PROMPF_LINES* new_lines[],  INT nloops); 
  void Distribution(INT old_loops[], PROMPF_LINES* old_lines[], 
    INT new_loops[], PROMPF_LINES* new_lines[],  INT nloops); 
  void Interchange(INT old_loops[], INT new_loops[], INT nloops); 
  void Pre_Peel(INT old_loops[], INT new_loops[], INT nloops); 
  void Undo_Pre_Peel();
  void Post_Peel(INT old_loops[], INT new_loops[], INT nloops); 
  void Undo_Post_Peel();
  void Mp_Tile(INT old_loop, INT new_loops[], INT nloops); 
  void Dsm_Tile(INT old_loop, INT new_loops[], INT nloops); 
  void Donest_Outer_Tile(INT old_loops[], INT new_loop, INT nloops);
  void Donest_Middle_Tile(INT old_loop, INT new_loop); 
  void Dsm_Local(INT new_loop, PROMPF_LINES* pl, char* index_name); 
  void Dsm_Io(INT new_loop, INT linenum, char* index_name); 
  void Single_Process(INT new_id, PROMPF_LINES* pl); 
  void Mp_Version(INT old_loops[], INT new_loops[], PROMPF_ID_TYPE id_type[], 
    INT nloops); 
  void Parallel_Region(INT old_loop, INT new_loop); 
  void Hoist_Messy_Bounds(INT old_loop[], INT new_loop[], INT nloops); 
  void Doacross_Sync(INT old_loop, INT new_loop); 
  void Doacross_Outer_Tile(INT old_loop, INT new_loop); 
  void Doacross_Inner_Tile(INT old_loop, INT new_loop); 
  void Remove_Unity_Trip(INT old_loop); 
  void Cache_Winddown(INT old_loops[], INT new_loops[], INT nloops); 
  void Interleaved_Winddown(INT old_loops[], INT new_loops[], INT nloops); 
  void General_Version(INT old_loops[], INT new_loops[], INT nloops); 
  void Cache_Tile(INT old_loop, INT new_loop); 
  void Register_Winddown(INT old_loops[], INT new_loops[], INT nloops); 
  void Register_SStrip(INT old_loops[], INT new_loops[], INT nloops); 
  void Register_Tile(INT loop); 
  void Register_Startup(INT old_loops[], INT new_loops[], INT nloops); 
  void Register_Shutdown(INT old_loops[], INT new_loops[], INT nloops); 
  void Se_Tile(INT old_loop, INT new_loop); 
  void Se_Cache_Tile(INT old_loop, INT new_loop); 
  void Inner_Fission(INT old_loop, PROMPF_LINES* old_lines, INT new_loops[],
    PROMPF_LINES* new_lines[], INT nloops);
  void Gather_Scatter(INT old_loop, PROMPF_LINES* old_lines, INT new_loops[],
    PROMPF_LINES* new_lines[], INT nloops);
  void Vintr_Fission(INT old_loop, PROMPF_LINES* old_lines, INT new_loops[],
    PROMPF_LINES* new_lines[], INT nloops);
  void Prefetch_Version(INT old_loops[], INT new_loops[], INT nloops); 
  void OMPL_Sections_To_Loop(INT old_id); 
  void OMPL_Eliminate_Section(INT old_id); 
  void OMPL_Atomic_To_Critical_Section(INT old_id); 
  void OMPL_Atomic_To_Swap(INT old_id); 
  void OMPL_Atomic_To_FetchAndOp(INT old_id); 
  void OMPL_Master_To_If(INT old_id); 
  void OMPL_Fetchop_Atomic(INT new_id, PROMPF_LINES* pl); 
  void F90_Array_Stmt(INT new_loop, PROMPF_LINES* pl, char* index_name);
  void Outer_Shackle(INT new_loop, PROMPF_LINES* pl, char* index_name);
  void Inner_Shackle(INT old_loop, INT new_loop);
  void Preopt_Create(INT new_loop, PROMPF_LINES* pl, char* index_name); 
  void Clear();
  void Save();
  void Restore();
  void Print(FILE* fp); 
  INT Check(FILE* fp, WN* wn_func);
  void Print_Compact(FILE* fp, PROMPF_TRANS_LOG ptl); 
};

extern WN_MAP Prompf_Id_Map; 
extern PROMPF_INFO* Prompf_Info; 
extern MEM_POOL PROMPF_pool;

extern PROMPF_ID_TYPE Prompf_Id_Type(WN* wn_ref, WN* wn_region, 
  BOOL* is_first=NULL); 

extern void Prompf_Assign_Ids(WN* wn_old, WN* wn_new, STACK<WN*>* old_stack, 
  STACK<WN*>* new_stack, BOOL copy_ids, INT max_ids=INT32_MAX);

#endif /* prompf_INCLUDED */ 
