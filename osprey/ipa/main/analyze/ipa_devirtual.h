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

#ifndef cxx_ipa_devirtual_INCLUDED
#define cxx_ipa_devirtual_INCLUDED

#include "symtab.h"
#include "ipc_symtab_merge.h"
#include "ipa_chg.h"

enum {
    NO_VFUNCTION_CANDIDATE = 0,
	MORE_THAN_ONE_VFUNCTION_CANDIDATE = 0xFFFFFFFF
};

// The detail infomation of a virtual function callsite
typedef struct {
    WN *wn;                      // WN of the callsite 
    IPA_NODE *method_node;       // the method which the callsite belongs to
    SUMMARY_CALLSITE *callsite;  // the summary of the callsite
    TY_INDEX static_ty;          // the static class type which the method belongs to 
    UINT32 offset;               // the offset of the method entry in virtual table
    ST_IDX target;               // direct call candidate that the calliste may be converted to
	                             // If there is only one candidate, target is the candidate ST
                                 // Otherwise, NO_VFUNCTION_CANDIDATE is for no candidate, 
								 // MORE_THAN_ONE_VFUNCTION_CANDIDATE is for more than one candidate
} callsite_targets_t;

/*
 * Insert a virtual function target into the target set
 * @origin : the original set
 * @target : the target to be inserted
 */
extern void
Insert_Vfunction_Target(ST_IDX &origin, ST_IDX target);

/*
 * Find the ST of a virtual function.
 * @class_type : the base class of the virtual function
 * @offset     : the offset of the method entry in virtual table
 */
extern ST_IDX 
IPA_class_virtual_function(TY_INDEX class_type, size_t offset);

/*
 * Main function of devirtualization in IPA phase
 */
extern void
IPA_devirtualization();

#endif

#endif
