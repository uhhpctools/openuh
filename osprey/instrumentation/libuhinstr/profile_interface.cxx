//-*-c++-*-
//
/*
 * Copyright 2013, University of Houston.  All Rights Reserved.
 */

/*
 * Copyright 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

// ====================================================================
// ====================================================================
//
// Module: profile_interface.cxx
//
// ====================================================================
//
// Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.
//
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
//
// Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
// Mountain View, CA 94043, or:
//
// http://www.sgi.com
//
// For further information regarding this notice, see:
//
// http://oss.sgi.com/projects/GenInfo/NoticeExplan
//
// ====================================================================
//
// Description:
//
// During instrumentation, calls to the following procedures are
// inserted into the WHIRL code.  When invoked, these procedures
// initialize, perform, and finalize frequency counts.
//
// ====================================================================
// ====================================================================


#include <stdio.h>
#include <stdlib.h>
#include <Profile/Profiler.h> /* from TAU */

#include "vector.h"
#include "profile.h"
#include "profile_interface.h"
#include "profile_errors.h"
#include "dump.h"

//omp_lock_t tau_regdescr_lock;
// ====================================================================


static char *output_filename = NULL;
static BOOL unique_output_filename = FALSE;


// ====================================================================


// One time initialization

void __profile_init(struct profile_init_struct *d)
{
    static bool first_called = TRUE;

    if(first_called) {
        first_called=FALSE;
    }
}


// PU level initialization to gather profile information for the PU.
// We call atexit during the first call to this routine to ensure
// that at exit we remember to destroy the data structures and dump
// profile information.
// Also, during the first call, a profile handle for the PU is created.
// During subsequent calls, teh PC address of the PU is used to access
// a hash table and return the profile handle that was created during the
// first call.

void *
__profile_pu_init(char *file_name, char* pu_name, long current_pc,
		  INT32 pusize, INT32 checksum)
{
    /* currently do nothing */
}


// For a PU, initialize the data structures that maintain
// invokation profile information.

void
__profile_invoke_init(void *pu_handle, INT32 num_invokes)
{
    /* currently do nothing */
}


// Gather profile information for a conditional invoke

void __profile_invoke_exit(struct profile_gen_struct *d)
{
    Tau_stop_timer(d->data,Tau_get_tid());
}

void
__profile_invoke(struct profile_gen_struct *d)
{
    void **data = &(d->data);
    Tau_profile_c_timer(data, d->pu_name, "", TAU_DEFAULT, "TAU_DEFAULT");
    Tau_start_timer(d->data, 0,Tau_get_tid());
}


// For a PU, initialize the data structures that maintain
// conditional branch profile information.

void
__profile_branch_init(void *pu_handle, INT32 num_branches)
{
    /* currently do nothing */
}


// Gather profile information for a conditional branch

void __profile_branch_exit(struct profile_gen_struct *d)
{
    /* currently do nothing */
}


void __profile_branch(struct profile_gen_struct *d)
{
    /* currently do nothing */
}


// For a PU, initialize the data structures that maintain
// switch profile information.

void
__profile_switch_init(void *pu_handle,
		      INT32 num_switches,    INT32 *switch_num_targets,
		      INT32 num_case_values, INT64 *case_values)
{
    /* currently do nothing */
}


// Gather profile information for an Switch

void
__profile_switch(struct profile_gen_struct *d)
{
    /* currently do nothing */
}

void
__profile_switch_exit(struct profile_gen_struct *d)
{
    /* currently do nothing */
}

// For a PU, initialize the data structures that maintain
// compgoto profile information.

void
__profile_compgoto_init(void *pu_handle, INT32 num_compgotos,
			INT32 *compgoto_num_targets)
{
    /* currently do nothing */
}


// Gather profile information for an Compgoto

void
__profile_compgoto(struct profile_gen_struct *d)
{
    /* currently do nothing */
}

// For a PU, initialize the data structures that maintain
// value profile information.
void __profile_value_init( void *pu_handle, INT32 num_values )
{
    /* currently do nothing */
}


// Gather profile information for a Value

void
__profile_value( void *pu_handle, INT32 inst_id, INT64 value )
{
    /* currently do nothing */
}

// For a PU, initialize the data structures that maintain value (FP) profile
// information for both the operands of a binary operator.

void __profile_value_fp_bin_init( void *pu_handle, INT32 num_values )
{
    /* currently do nothing */
}


// Gather profile information for Values (FP)

void
__profile_value_fp_bin( void *pu_handle, INT32 inst_id,
			double value_fp_0, double value_fp_1 )
{
    /* currently do nothing */
}


// For a PU, initialize the data structures that maintain
// loop profile information.

void
__profile_loop_init(void *pu_handle, INT32 num_loops)
{
    /* currently do nothing */
}


// Gather profile information for a loop entry

void __profile_loop(struct profile_gen_struct *d)
{
    char rname[256], rtype[1024];
    void **data;

    sprintf(rname, "%s%d", "LOOP #",d->id);
    sprintf(rtype, "[file:%s <%d, %d>]",
            d->file_name, d->linenum, d->endline);

    data = &(d->data);
    Tau_profile_c_timer(data, rname, rtype, TAU_DEFAULT, "TAU_DEFAULT");
    Tau_start_timer(d->data, 0,Tau_get_tid());
}

void
__profile_loop_exit(struct profile_gen_struct *d)
{
    Tau_stop_timer(d->data,Tau_get_tid());
}

// Gather profile information from a Loop Iteration

void
__profile_loop_iter(struct profile_gen_struct *d)
{
    /* currently do nothing */
}


// For a PU, initialize the data structures that maintain
// short circuit profile information.

void
__profile_short_circuit_init(void *pu_handle, INT32 num_short_circuit_ops)
{
    /* currently do nothing */
}


// Gather profile information for the right operand of a short circuit op

void
__profile_short_circuit(struct profile_gen_struct *d)
{
    /* currently do nothing */
}


// For a PU, initialize the data structures that maintain
// call profiles.

void
__profile_call_init(void *pu_handle, int num_calls)
{
    /* currently do nothing */
}

// For a PU, initialize the data structures that maintain
// icall profiles.

void
__profile_icall_init(void *pu_handle, int num_icalls)
{
    /* currently do nothing */
}

// Gather the entry count for this call id

void
 __profile_call_entry(struct profile_gen_struct *d)
{
    /* currently do nothing */
}


// Gather the exit count for this call id

void
__profile_call_exit(struct profile_gen_struct *d)
{
    /* currently do nothing */
}

void
__profile_icall_entry(struct profile_gen_struct *d)
{
    /* currently do nothing */
}

void
__profile_icall_exit(struct profile_gen_struct *d)
{
    /* currently do nothing */
}

// At exit processing to destroy data structures and dump profile
// information.

void __profile_finish(void)
{
    /* currently do nothing */
}
