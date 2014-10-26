/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2009-2014 University of Houston.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

 Contact information:
 http://www.cs.uh.edu/~hpctools
*/


#ifndef ALLOC_H
#define ALLOC_H

#include <ctype.h>
#include "lock.h"
#include "dopevec.h"
#include "uthash.h"

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
    struct shared_memory_slot *next_empty;
    struct shared_memory_slot *prev_empty;
};
typedef struct shared_memory_slot shared_memory_slot_t;

typedef struct alloc_dp_slot {
    void *addr;
    DopeVectorType *dp;
    UT_hash_handle hh;
} alloc_dp_slot;
/*
 * mem_block_t keeps track of shared memory slot for each team.
 */
typedef struct {
    void *start_addr;
    void *end_addr;
} mem_block_t;

typedef struct {
    size_t current_heap_usage;
    size_t max_heap_usage;
    size_t reserved_heap_usage;
} mem_usage_info_t;

/* SHARED MEMORY MANAGEMENT */

void *coarray_allocatable_allocate_(unsigned long var_size, DopeVectorType * dp,
                                    int *statvar);
void *coarray_asymmetric_allocate_(unsigned long var_size);
void *coarray_asymmetric_allocate_if_possible_(unsigned long var_size);
void coarray_asymmetric_deallocate_(void *var_address);
void coarray_deallocate_(void *var_address, int *statvar);
void coarray_free_all_shared_memory_slots();
void deallocate_within(void *start_addr, void *end_addr);

unsigned long largest_allocatable_slot_avail(unsigned long size);

void deallocate_team_all();

#if defined(CAFRT_DEBUG)
extern void uhcaf_print_alloc_max_size();
extern void uhcaf_print_alloc_slots();
#endif
#endif
