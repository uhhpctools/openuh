/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2009-2013 University of Houston.

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
 http://www.cs.uh.edu/~hpctools
*/


#ifndef ALLOC_H
#define ALLOC_H

#include <ctype.h>
#include "lock.h"
#include "dopevec.h"


/* if an required asymmetric allocation will result in this percentage or
 * greater of the heap space consumed, then allocate out of default system
 * memory instead. Only used when out_of_segment_rma_enabed is set to 1. */
static const double ASYMM_ALLOC_RESTRICT_FACTOR = 0.70;

/* SHARED MEMORY MANAGEMENT */
struct shared_memory_slot {
    void *addr;
    unsigned long size;
    unsigned short feb;         //full empty bit. 1=full
    struct shared_memory_slot *next;
    struct shared_memory_slot *prev;
};
typedef struct shared_memory_slot shared_memory_slot_t;


typedef struct {
    size_t current_heap_usage;
    size_t max_heap_usage;
    size_t reserved_heap_usage;
} mem_usage_info_t;

/* SHARED MEMORY MANAGEMENT */

void *coarray_allocatable_allocate_(unsigned long var_size, int* statvar);
void *coarray_asymmetric_allocate_(unsigned long var_size);
void *coarray_asymmetric_allocate_if_possible_(unsigned long var_size);
void coarray_asymmetric_deallocate_(void *var_address);
void coarray_deallocate_(void *var_address, int* statvar);
void coarray_free_all_shared_memory_slots();


#endif
