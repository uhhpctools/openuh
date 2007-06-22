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
***	This file contains descriptions of functions used to assign MPNUMs
***	for the subroutines which will be created when the MP lowerer is 
***	invoked on DOACROSSes and PARALLEL REGIONs. 
***
***  Exported types and functions:
***
***	WN* WN_Array_Symbol(WN* wn_ref)
***
***	    For the ILOAD or ISTORE 'wn_ref', returns the node which
***	    contains the symbol of 'wn_ref'.
***
***	void Annotate_For_Mp_Lowering(PU_Info* current_pu, WN* func_nd)
***
***	    Annotate the function 'func_nd' with information about what
***	    functions will be created when the doacrosses and parallel regions
***         are lowered.
***
***	void Print_Prompf_Transaction_Log(BOOL after)
***
***	    Print the transaction log for PROMPF.  If 'after', only 
***	    print transactions after the mark.
***
***	void Print_Prompf_Parallelization_Log(WN* func_nd)
***
***	    Print the parallelization log for 'func_nd'.
***
***     void Print_Prompf_Doacross_Log(PU_Info* current_pu, WN* func_nd,
***	    BOOL after)
***
***	    Print the PROMPF doacross log for the program unit whose
***	    PU_Info is 'current_pu' and whose root ARA_LOOP_INFO is i
***	    'root_info'.  If 'after', print at the end of LNO.
***
***	void Print_Prompf_Parallel_Region_Log(PU_Info* current_pu, 
***	    WN* func_nd, BOOL after)
***
***	    Print the PROMPF parallel region log for the program unit 
***	    whose PU_Info is 'current_pu' and whose root ARA_LOOP_INFO is
***	    'root_info'.  If 'after', print at the end of LNO.
***
***	void Print_Prompl_Msgs(PU_Info* current_pu, WN* func_nd) 
***
***	    Print messages for prompf .l file in human readable form.
***
***	void Print_Prompf_Nest_Log(WN* func_nd, BOOL after)
***
***	    Print a log with a line for each do loop containing the
***	    transformed id of the loop and its nesting level.  If 
***	    'after', print at the end of LNO.
***
***	void Prompf_Assign_New_Ids(WN* wn_func)
***
***	    Assign new ids to the DO_LOOPs in 'wn_func' which do not 
***	    already have ids.
***
***	void Prompf_Collect_Ids(WN* wn_tree, STACK<WN*>* st_wn,  
***	    STACK<INT>* st_id)
***
***	    Collect all of the prompf ids in the subtree rooted at
***	    'wn_tree' and push them on the stack 'st_id' and push their 
*** 	    their WN*s on the stack 'st_wn'.
***
***	void Print_Prompf_Preopt_Transaction_Log(STACK<INT>* st_before,
***	    STACK<WN*>* st_after_wn, MEM_POOL* mem_pool)
***
***	    Given that 'st_before' is a list of prompf ids before preopt,
***	    and 'st_after_id' is a list of WN*s with prompf ids after preopt,
***	    print the transformation log for preopt.
**/ 

#ifndef parids_INCLUDED
#define parids_INCLUDED "parids.h"

#ifndef prompf_INCLUDED
#include "prompf.h"
#endif 

extern WN* WN_Array_Symbol(WN* wn_ref); 

extern void Annotate_For_Mp_Lowering(PU_Info* current_pu, WN* func_nd); 

extern void Print_Prompf_Transaction_Log(BOOL after); 

extern void Print_Prompf_Parallelization_Log(WN* func_nd);

extern void Print_Prompf_Doacross_Log(PU_Info* current_pu, WN* func_nd,
  BOOL after);
 
extern void Print_Prompf_Parallel_Region_Log(PU_Info* current_pu, 
  WN* func_nd, BOOL after);

extern void Print_Prompl_Msgs(PU_Info* current_pu, WN* func_nd); 

extern void Print_Prompf_Nest_Log(WN* func_nd, BOOL after);

extern void Prompf_Assign_New_Ids(WN* wn_func); 

extern void Prompf_Collect_Ids(WN* wn_tree, STACK<WN*>* st_wn, 
  STACK<INT>* st_id);

extern void Print_Prompf_Preopt_Transaction_Log(PROMPF_INFO* prompf_info,
  STACK<INT>* st_before_id, STACK<WN*>* st_after_wn, MEM_POOL* mem_pool);

#endif /* parids_INCLUDED */

