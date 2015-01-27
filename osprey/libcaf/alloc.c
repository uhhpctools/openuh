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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "caf_rtl.h"
#include "alloc.h"
#include "comm.h"
#include "env.h"

#include "trace.h"
#include "profile.h"
#include "util.h"
#include "team.h"

#define MEMORY_SLOT_SELECT(common_slot, minfo) \
    if (current_team == NULL || current_team->depth == 0) {  \
        common_slot = init_common_slot; \
        minfo = mem_info; \
    } else { \
        common_slot = child_common_slot; \
        minfo = teams_mem_info; \
    }

#define MEMORY_SLOT_SAVE(common_slot) \
    if (current_team == NULL || current_team->depth == 0) { \
        init_common_slot = common_slot; \
    } else { \
        child_common_slot = common_slot; \
    }

extern team_type_t *current_team;
/* byte alignment for allocations */
size_t alloc_byte_alignment = DEFAULT_ALLOC_BYTE_ALIGNMENT;

/* flag for whether we are enabled out-of-segment rma accesses */
int out_of_segment_rma_enabled = 0;

/* describes memory usage status */
// mem_usage_info_t *mem_info;
mem_usage_info_t *mem_info;
mem_usage_info_t *teams_mem_info;

/* common_slot is a node in the shared memory link-list that keeps track
 * of available memory that can used for both allocatable coarrays and
 * asymmetric data. It is the only handle to access the link-list.
 *
 * TEAM_SUPPORT: seperate the node into two.
 * One is init_common_slot, which is tracking the initial comm_slot. Initlized
 * in comm_init().
 * Another is child_common_slot, stays null until team other than team_world
 * is formed
 */

// shared_memory_slot_t *common_slot;
shared_memory_slot_t *init_common_slot;
shared_memory_slot_t *child_common_slot;

/* LOCAL FUNCTION DECLARATIONIS */
static struct shared_memory_slot *find_empty_shared_memory_slot_above
    (struct shared_memory_slot *slot, unsigned long var_size);
static struct shared_memory_slot *find_empty_shared_memory_slot_below
    (struct shared_memory_slot *slot, unsigned long var_size);
static void *split_empty_shared_memory_slot_from_top
    (struct shared_memory_slot *slot, unsigned long var_size,
     struct shared_memory_slot **orig_slot);
static void *split_empty_shared_memory_slot_from_bottom
    (struct shared_memory_slot *slot, unsigned long var_size);

static void print_slots_below(struct shared_memory_slot *slot);

static struct shared_memory_slot *find_shared_memory_slot_above
    (struct shared_memory_slot *slot, void *address);
static struct shared_memory_slot *find_shared_memory_slot_below
    (struct shared_memory_slot *slot, void *address);

static void join_3_shared_memory_slots(struct shared_memory_slot *slot,
                                       struct shared_memory_slot
                                       **common_slot_p);
static void join_with_prev_shared_memory_slot(struct shared_memory_slot
                                              *slot,
                                              struct shared_memory_slot
                                              **common_slot_p);
static void join_with_next_shared_memory_slot(struct shared_memory_slot
                                              *slot,
                                              struct shared_memory_slot
                                              **common_slot_p);

static void empty_shared_memory_slot(struct shared_memory_slot *slot,
                                     struct shared_memory_slot
                                     **common_slot_p);

static void free_prev_slots_recursively(struct shared_memory_slot *slot);
static void free_next_slots_recursively(struct shared_memory_slot *slot);

static void store_slot_info(team_type_t * team, void *addr,
                            DopeVectorType * dp);
static void delete_slot_info(team_type_t * team, void *addr);
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * SHARED MEMORY MANAGEMENT FUNCTIONS
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Note: The term 'shared memory' is used in the PGAS sense, i.e. the
 * memory may not be physically shared. It can however be directly
 * accessed from any other image. This is done by the pinning/registering
 * memory by underlying communication layer (GASNet/ARMCI).
 *
 * During comm_init GASNet/ARMCI creates a big chunk of shared memory.
 * Static coarrays are allocated memory from this chunk. The remaining
 * memory is left for allocatable coarrays and pointers in coarrays of
 * derived datatype (henceforth referred as asymmetric data).
 * It returns the starting address and size of this remaining memory chunk
 * by populating the structure common_slot(explained later).
 *
 * Normal fortan allocation calls are intercepted to check whether they
 * are for coarrays or asymmetric data. If yes, the following functions
 * are called which are defined below.
 *  void* coarray_allocatable_allocate_(unsigned long var_size, int *statvar);
 *  void* coarray_asymmetric_allocate_(unsigned long var_size);
 * Since allocatable coarrays must have symmetric address, a seperate heap
 * must be created for asymmetric data. To avoid wasting memory space by
 * statically reserving it, we use the top of heap for allocatable
 * coarrays (which grows downward) and the bottom of heap for asymmetric
 * data(which grows up). A link-list of struct shared_memory_slot is
 * used to manage allocation and deallocation.
 *
 * common_slot is an empty slot which always lies in between allocatable
 * heap and asymmetric heap, and used by both to reserve memory.
 *                          _________
 *                          | Alloc |
 *                          | heap  |
 *                          ---------   => may change at runitme
 *                          | Init  |
 *                          | Empty |
 *                          | slot  |
 *                          =========
 *                          | Alloc |
 *                          | heap  |
 *                          ---------   => may change at runtime
 *                          | Common|
 *                          | slot  |
 *                          ---------   => may change at runtime
 *                          | asymm |
 *                          | heap  |
 *                          |_______|
 *
 * Allocatable heap comsumes common_slot from top, while assymetric heap
 * consumes from bottom. Each allocation address and size is stored in
 * a sperate slot (node in the list). Each slot has a full-empty bit(feb).
 * During deallocation (using function coarray_deallocate_) the feb is set
 * to 0 (empty). If any neighboring slot is empty, they are merged. Hence,
 * when a slot bordering common-slot is deallocated, the common-slot
 * grows.
 *
 * If there is no more space left in common slot, empty slots are used
 * from above for allocable coarrays or from below for asymmetric data.
 *
 * During exit, the function coarray_free_all_shared_memory_slots()
 * is used to free all nodes in the shared memory list.
 */

/* Static function used to find empty memory slots while reserving
 * memory for allocatable coarrays */
static struct shared_memory_slot *find_empty_shared_memory_slot_above
    (struct shared_memory_slot *slot, unsigned long var_size)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    /* start from the first empty slot  */
    while (slot->prev_empty) {
        slot = slot->prev_empty;
    }

    /* search for empty slot that is large enough */
    while (slot) {
        if (slot->feb == 0 && slot->size >= var_size) {
            LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
            return slot;
        }
        slot = slot->next_empty;
    }

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
    return 0;
}

/* Static function used to find empty memory slots while reserving
 * memory for assymetric coarrays */
static struct shared_memory_slot *find_empty_shared_memory_slot_below
    (struct shared_memory_slot *slot, unsigned long var_size)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    /* start from last empty slot */
    while (slot->next_empty) {
        slot = slot->next_empty;
    }

    /* search backwards for empty slot that is large enough */
    while (slot) {
        if (slot->feb == 0 && slot->size >= var_size) {
            LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
            return slot;
        }
        slot = slot->prev_empty;
    }
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
    return 0;
}

/* Static function used to reserve top part of an empty memory slot
 * for allocatable coarrays. Returns the memory address allocated */
static void *split_empty_shared_memory_slot_from_top
    (struct shared_memory_slot *slot, unsigned long var_size,
     struct shared_memory_slot **orig_slot)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    struct shared_memory_slot *new_empty_slot;

    new_empty_slot = (struct shared_memory_slot *) malloc
        (sizeof(struct shared_memory_slot));

    new_empty_slot->addr = slot->addr + var_size;
    new_empty_slot->size = slot->size - var_size;
    new_empty_slot->feb = 0;
    new_empty_slot->next = slot->next;
    new_empty_slot->prev = slot;
    new_empty_slot->next_empty = slot->next_empty;
    new_empty_slot->prev_empty = slot->prev_empty;
    if (new_empty_slot->prev_empty)
        new_empty_slot->prev_empty->next_empty = new_empty_slot;
    if (new_empty_slot->next_empty)
        new_empty_slot->next_empty->prev_empty = new_empty_slot;

    slot->size = var_size;
    slot->feb = 1;
    slot->next = new_empty_slot;
    slot->prev_empty = NULL;
    slot->next_empty = NULL;

    if (*orig_slot == slot)
        *orig_slot = new_empty_slot;

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
    return slot->addr;
}

/* Static function used to reserve bottom part of an empty memory slot
 * for asymmetric data. Returns the memory address allocated*/
static void *split_empty_shared_memory_slot_from_bottom
    (struct shared_memory_slot *slot, unsigned long var_size)
{
    struct shared_memory_slot *new_full_slot;
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    new_full_slot = (struct shared_memory_slot *) malloc
        (sizeof(struct shared_memory_slot));
    new_full_slot->addr = slot->addr + slot->size - var_size;
    new_full_slot->size = var_size;
    new_full_slot->feb = 1;
    new_full_slot->next = slot->next;
    new_full_slot->prev = slot;
    new_full_slot->next_empty = NULL;
    new_full_slot->prev_empty = NULL;

    slot->size = slot->size - var_size;

    if (slot->next)
        slot->next->prev = new_full_slot;

    slot->next = new_full_slot;

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
    return new_full_slot->addr;
}

/* Memory allocation function for allocatable coarrays. It is invoked
 * from fortran allocation function _ALLOCATE in
 * osprey/libf/fort/allocation.c
 * It finds empty slot from the shared memory list (common_slot & above)
 * and then splits the slot from top
 * Note: there is barrier as it is a collective operation*/
void *coarray_allocatable_allocate_(unsigned long var_size, DopeVectorType * dp,
                                    int *statvar)
{
    mem_usage_info_t *minfo;
    struct shared_memory_slot *common_slot;
    struct shared_memory_slot *empty_slot;
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_COARRAY_ALLOC_DEALLOC);

    if (var_size % alloc_byte_alignment != 0) {
        var_size =
            ((var_size - 1) / alloc_byte_alignment +
             1) * alloc_byte_alignment;
    }

    MEMORY_SLOT_SELECT(common_slot, minfo);

    empty_slot = find_empty_shared_memory_slot_above(common_slot,
                                                     var_size);
    if (empty_slot == 0)
        Error
            ("No more shared memory space available for allocatable coarray.\n"
             "Set environment variable %s or use cafrun option %s for more space.",
             (common_slot == init_common_slot) ? ENV_IMAGE_HEAP_SIZE :
              ENV_TEAMS_HEAP_SIZE,
             (common_slot == init_common_slot) ? "--image-heap" :
              "--teams-heap");

    if (minfo) {
        size_t current_size = minfo->current_heap_usage + var_size;
        minfo->current_heap_usage = current_size;
        if (minfo->max_heap_usage < current_size)
            minfo->max_heap_usage = current_size;
    }
    // implicit barrier in case of allocatable.
    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_all, statvar, sizeof(int),
                         NULL, 0);


    if (empty_slot != common_slot && empty_slot->size == var_size) {
        empty_slot->feb = 1;

        if (empty_slot->prev_empty)
            empty_slot->prev_empty->next_empty = empty_slot->next_empty;

        if (empty_slot->next_empty)
            empty_slot->next_empty->prev_empty = empty_slot->prev_empty;

        empty_slot->next_empty = NULL;
        empty_slot->prev_empty = NULL;

        store_slot_info(current_team, empty_slot->addr, dp);

        PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
        return empty_slot->addr;
    }

    void *retval =
        split_empty_shared_memory_slot_from_top(empty_slot, var_size,
                                                &common_slot);
    store_slot_info(current_team, retval, dp);

    MEMORY_SLOT_SAVE(common_slot);
    PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");

    return retval;
}

/* Memory allocation function for asymmetric data. It is invoked
 * from fortran allocation function _ALLOCATE in
 * osprey/libf/fort/allocation.c
 * It finds empty slot from the shared memory list (common_slot & below)
 * and then splits the slot from bottom */
void *coarray_asymmetric_allocate_(unsigned long var_size)
{
    struct shared_memory_slot *common_slot;
    struct shared_memory_slot *empty_slot;

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_COARRAY_ALLOC_DEALLOC);


    common_slot = init_common_slot;
    if (var_size % alloc_byte_alignment != 0) {
        var_size =
            (var_size / alloc_byte_alignment + 1) * alloc_byte_alignment;
    }

    empty_slot = find_empty_shared_memory_slot_below(common_slot,
                                                     var_size);

    if (empty_slot == 0) {
        if (!out_of_segment_rma_enabled) {
            /* out-of-segment accesses not supported */
            Error
                ("No more shared memory space available for asymmetric data."
                 " Set environment variable %s or cafrun option for more "
                 "space.", ENV_IMAGE_HEAP_SIZE);
        } else {
            /* it is assumed that out-of-segment accesses are supported here.
             * */
            LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
                         "Couldn't find space in shared memory segment for "
                         "asymmetric allocation, "
                         "so allocating out of normal system memory.");
            void *retval = comm_malloc(var_size);
            PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
            LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
            return retval;
        }
        /* does not reach */
    } else if (empty_slot != 0 &&
               (out_of_segment_rma_enabled == 2 ||
               (out_of_segment_rma_enabled == 1  &&
               (mem_info->current_heap_usage + var_size) >=
               ASYMM_ALLOC_RESTRICT_FACTOR *
               mem_info->reserved_heap_usage))) {
        LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
                     "Running out of space in shared memory segment for "
                     "asymmetric allocation (%ldMB out of %ldMB would "
                     "be used) so allocating out of normal system memory.",
                     (mem_info->current_heap_usage +
                      var_size) / (1024 * 1024),
                     (mem_info->reserved_heap_usage) / (1024 * 1024));
        void *retval = comm_malloc(var_size);
        PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
        return retval;
        /* does not reach */
    }

    /* update heap usage info */
    size_t current_size = mem_info->current_heap_usage + var_size;
    mem_info->current_heap_usage = current_size;
    if (mem_info->max_heap_usage < current_size)
        mem_info->max_heap_usage = current_size;

    if (empty_slot != common_slot && empty_slot->size == var_size) {
        empty_slot->feb = 1;
        if (empty_slot->prev_empty)
            empty_slot->prev_empty->next_empty = empty_slot->next_empty;

        if (empty_slot->next_empty)
            empty_slot->next_empty->prev_empty = empty_slot->prev_empty;

        empty_slot->next_empty = NULL;
        empty_slot->prev_empty = NULL;
        PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
        return empty_slot->addr;
    }

    void *retval = split_empty_shared_memory_slot_from_bottom(empty_slot,
                                                              var_size);

    //store_slot_info(current_team, retval, dp);
    PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");

    return retval;
}

/* Memory allocation function for asymmetric data. It is invoked
 * from fortran allocation function _ALLOCATE in
 * osprey/libf/fort/allocation.c
 * It finds empty slot from the shared memory list (common_slot & below)
 * and then splits the slot from bottom */
void *coarray_asymmetric_allocate_if_possible_(unsigned long var_size)
{
    struct shared_memory_slot *common_slot;
    struct shared_memory_slot *empty_slot;

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_COARRAY_ALLOC_DEALLOC);

    common_slot = init_common_slot;;

    if (var_size % alloc_byte_alignment != 0) {
        var_size =
            (var_size / alloc_byte_alignment + 1) * alloc_byte_alignment;
    }

    empty_slot = find_empty_shared_memory_slot_below(common_slot,
                                                     var_size);
    if (empty_slot == 0) {
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "Couldn't find empty slot.");

        PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
        return 0;
    }

    /* update heap usage info */
    size_t current_size = mem_info->current_heap_usage + var_size;
    mem_info->current_heap_usage = current_size;
    if (mem_info->max_heap_usage < current_size)
        mem_info->max_heap_usage = current_size;

    if (empty_slot != common_slot && empty_slot->size == var_size) {
        empty_slot->feb = 1;
        if (empty_slot->prev_empty)
            empty_slot->prev_empty->next_empty = empty_slot->next_empty;

        if (empty_slot->next_empty)
            empty_slot->next_empty->prev_empty = empty_slot->prev_empty;

        empty_slot->next_empty = NULL;
        empty_slot->prev_empty = NULL;
        PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
        return empty_slot->addr;
    }

    void *retval = split_empty_shared_memory_slot_from_bottom(empty_slot,
                                                              var_size);

    PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");

    return retval;
}

/* Static function called from coarray_deallocate.
 * It finds the slot with the address (passed in param) by searching
 * the shared memory link-list starting from the slot(passed in param)
 * and above it. Used for finding slots containing allocatable coarrays*/
static struct shared_memory_slot *find_shared_memory_slot_above
    (struct shared_memory_slot *slot, void *address) {
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    while (slot) {
        if (slot->addr < current_team->symm_mem_slot.start_addr) {
            Warning("beyond team heap scope");
            return NULL;
        }
        if (slot->feb == 1 && slot->addr == address)
            return slot;
        slot = slot->prev;
    }

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
    return 0;
}

/* Static function called from coarray_deallocate.
 * It finds the slot with the address (passed in param) by searching
 * the shared memory link-list starting from the slot(passed in param)
 * and below it. Used for finding slots containing asymmetric data*/
static struct shared_memory_slot *find_shared_memory_slot_below
    (struct shared_memory_slot *slot, void *address) {
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    while (slot) {
        /*       if(slot->addr >= current_team->symm_mem_slot.end_addr){
           Warning("Address exceeds team heap scope %p, %p \n", slot->addr, current_team->symm_mem_slot.end_addr);
           return NULL;
           }
         */
        if (slot->feb == 1 && slot->addr == address)
            return slot;
        slot = slot->next;
    }

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
    return 0;
}

/* Static function called from empty_shared_memory_slot (used in dealloc).
 * Merge slot with the slot above & below it. If any of these slots is the
 * common-slot, the common-slot points to the merged slot */
static void join_3_shared_memory_slots(struct shared_memory_slot *slot,
                                       struct shared_memory_slot
                                       **common_slot_p)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    struct shared_memory_slot *prev_slot, *next_slot;

    prev_slot = slot->prev;
    next_slot = slot->next;

    slot->addr = prev_slot->addr;
    slot->size = prev_slot->size + slot->size + next_slot->size;
    slot->next = next_slot->next;
    slot->prev = prev_slot->prev;
    slot->next_empty = next_slot->next_empty;
    slot->prev_empty = prev_slot->prev_empty;

    if (prev_slot->prev) {
        prev_slot->prev->next = slot;
    }
    if (prev_slot->prev_empty) {
        prev_slot->prev_empty->next_empty = slot;
    }
    if (next_slot->next) {
        next_slot->next->prev = slot;
    }
    if (next_slot->next_empty) {
        next_slot->next_empty->prev_empty = slot;
    }

    if (*common_slot_p == next_slot || *common_slot_p == prev_slot)
        *common_slot_p = slot;
    comm_free(prev_slot);
    comm_free(next_slot);

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

/* Static function called from empty_shared_memory_slot (used in dealloc).
 * Merge slot with the slot above it. If any of these slots is the
 * common-slot, the common-slot points to the merged slot */
static void join_with_prev_shared_memory_slot(struct shared_memory_slot
                                              *slot,
                                              struct shared_memory_slot
                                              **common_slot_p)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    struct shared_memory_slot *prev_slot;

    prev_slot = slot->prev;

    slot->addr = prev_slot->addr;
    slot->size = prev_slot->size + slot->size;
    slot->prev = prev_slot->prev;
    slot->prev_empty = prev_slot->prev_empty;

    if (prev_slot->prev) {
        prev_slot->prev->next = slot;
    }
    if (prev_slot->prev_empty) {
        prev_slot->prev_empty->next_empty = slot;
    }

    /* prev_slot->next_empty should be NULL or point to next empty slot */
    slot->next_empty = prev_slot->next_empty;
    if (slot->next_empty)
        slot->next_empty->prev_empty = slot;

    if (prev_slot == *common_slot_p) {
        *common_slot_p = slot;
    }

    comm_free(prev_slot);

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

/* Static function called from empty_shared_memory_slot (used in dealloc).
 * Merge slot with the slot below it. If any of these slots is the
 * common-slot, the common-slot points to the merged slot */
static void join_with_next_shared_memory_slot(struct shared_memory_slot
                                              *slot,
                                              struct shared_memory_slot
                                              **common_slot_p)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    struct shared_memory_slot *next_slot;
    next_slot = slot->next;
    slot->size += next_slot->size;
    if (next_slot->next) {
        next_slot->next->prev = slot;
    }
    if (next_slot->next_empty) {
        next_slot->next_empty->prev_empty = slot;
    }
    slot->next_empty = next_slot->next_empty;
    slot->next = next_slot->next;

    /* next_slot->prev_empty should be NULL or point to prior empty slot */
    slot->prev_empty = next_slot->prev_empty;
    if (slot->prev_empty)
        slot->prev_empty->next_empty = slot;

    if (*common_slot_p == next_slot)
        *common_slot_p = slot;

    comm_free(next_slot);

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

/* Static function called from coarray_deallocate.
 * Empties the slot passed in parameter:
 * 1) set full-empty-bit to 0
 * 2) merge the slot with neighboring empty slots (if found)
 *
 * Return value: is 1 if slot has been freed, otherwise 0.
 * */

static void empty_shared_memory_slot(struct shared_memory_slot *slot,
                                     struct shared_memory_slot
                                     **common_slot_p)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    /* set to empty, and zero out the memory */
    slot->feb = 0;
    memset(slot->addr, 0, slot->size);

    if (slot->prev && (slot->prev->feb == 0) && slot->next
        && (slot->next->feb == 0)) {
        join_3_shared_memory_slots(slot, common_slot_p);
    } else if (slot->prev && (slot->prev->feb == 0)) {
        join_with_prev_shared_memory_slot(slot, common_slot_p);
    } else if (slot->next && (slot->next->feb == 0)) {
        join_with_next_shared_memory_slot(slot, common_slot_p);
    } else {
        int done = 0;
        if ((*common_slot_p)->addr >= slot->addr) {
            /* search forward for next empty slot */
            struct shared_memory_slot *s;
            s = slot;
            while (s->next) {
                s = s->next;
                if (!s->feb) {
                    slot->next_empty = s;
                    slot->prev_empty = s->prev_empty;
                    s->prev_empty = slot;
                    done = 1;
                    break;
                }
            }
        }
        if (!done) {
            /* search backward for previous empty slot */
            struct shared_memory_slot *s;
            s = slot;
            while (s->prev) {
                s = s->prev;
                if (!s->feb) {
                    slot->prev_empty = s;
                    slot->next_empty = s->next_empty;
                    s->next_empty = slot;
                    done = 1;
                    break;
                }
            }
        }
    }

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

/* Memory deallocation function for allocatable coarrays and asymmetric
 * data. It is invoked from fortran allocation function _DEALLOCATE in
 * osprey/libf/fort/allocation.c
 * It finds the slot from the shared memory list, set full-empty-bit to 0,
 * and then merge the slot with neighboring empty slots (if found)
 * Note: there is implicit barrier for allocatable coarrays*/
void coarray_deallocate_(void *var_address, int *statvar)
{
    mem_usage_info_t *minfo;
    struct shared_memory_slot *common_slot;
    struct shared_memory_slot *slot;

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_COARRAY_ALLOC_DEALLOC);

    MEMORY_SLOT_SELECT(common_slot, minfo);
    slot = find_shared_memory_slot_above(common_slot, var_address);
    if (slot) {
        // implicit barrier for allocatable
        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_all, statvar,
                             sizeof(int), NULL, 0);
    } else
        slot = find_shared_memory_slot_below(common_slot, var_address);
    if (slot == 0) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "Address%p not in remote-access segment.",
                     var_address);

        PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
        return;
    }
    delete_slot_info(current_team, slot->addr);
    /* update heap usage info if MEMORY_SUMMARY trace is enabled */
    minfo->current_heap_usage -= slot->size;

    empty_shared_memory_slot(slot, &common_slot);

    MEMORY_SLOT_SAVE(common_slot);
    PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

/* function called when the end_change_team is called.
 * join the slots within [start_addr, end_addr) (for team_world)
 * or until meet child_comm_addr (for other teams)
 * */

void deallocate_team_all()
{
    team_type_t *team = current_team;

    mem_usage_info_t *minfo;
    struct shared_memory_slot *common_slot;
    struct shared_memory_slot *cur_slot;
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_COARRAY_ALLOC_DEALLOC);

    if (team->allocated_list == NULL) {
        PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
        return;
    }

    MEMORY_SLOT_SELECT(common_slot, minfo);

    alloc_dp_slot *current_item, *tmp;

    HASH_ITER(hh, team->allocated_list, current_item, tmp) {
        cur_slot =
            find_shared_memory_slot_above(common_slot, current_item->addr);
        /*TODO: Need to correct this */
        if (cur_slot == NULL) {
            Warning("Problem in deallocate team all");
        } else {
            //      coarray_deallocate_(current_item->addr, NULL);
            empty_shared_memory_slot(cur_slot, &common_slot);
        }

        delete_slot_info(team, current_item->addr);
    }

    MEMORY_SLOT_SAVE(common_slot);

    PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

void deallocate_within(void *start_addr, void *end_addr)
{
    mem_usage_info_t *minfo;
    struct shared_memory_slot *common_slot;
    struct shared_memory_slot *slot;
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_COARRAY_ALLOC_DEALLOC);

    if (start_addr == end_addr) {
        PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
        return;
    }

    MEMORY_SLOT_SELECT(common_slot, minfo);

    slot = common_slot;

    while (slot && slot->addr > start_addr) {
        slot = slot->prev;
    }

    if (slot == NULL) {
        Error("Could not locate starting allocation slot");
    }

    while (slot && slot->addr < end_addr && slot != common_slot) {
        minfo->current_heap_usage -= slot->size;
        empty_shared_memory_slot(slot, &common_slot);
        slot = slot->next;
    }

    MEMORY_SLOT_SAVE(common_slot);
    PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

void coarray_asymmetric_deallocate_(void *var_address)
{
    struct shared_memory_slot *common_slot;
    struct shared_memory_slot *slot;
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_COARRAY_ALLOC_DEALLOC);

    common_slot = init_common_slot;

    slot = find_shared_memory_slot_below(common_slot, var_address);
    if (slot == 0) {
        if (!out_of_segment_rma_enabled) {
            /* coarray_asymmetric_deallocate_ shouldn't be called unless the
             * address is in the shared memory segment in this case.
             */
            LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                         "Address%p not in remote-access segment.",
                         var_address);
        } else {
            LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
                         "Address%p not in remote-access segment, using "
                         "normal free", var_address);
        }

        comm_free(var_address);

        PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
        return;
    }

    /* update heap usage info if MEMORY_SUMMARY trace is enabled */
    mem_info->current_heap_usage -= slot->size;

    empty_shared_memory_slot(slot, &common_slot);

    init_common_slot = common_slot;

    PROFILE_FUNC_EXIT(CAFPROF_COARRAY_ALLOC_DEALLOC);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

/* Static function called from coarray_free_all_shared_memory_slots()
 * during exit from program.
 * It recursively frees slots in the shared memory link-list starting
 * from slot passed in param and all slots above(previous) it. */
static void free_prev_slots_recursively(struct shared_memory_slot *slot)
{
    if (slot) {
        free_prev_slots_recursively(slot->prev);
        comm_free(slot);
    }
}

/* Static function called from coarray_free_all_shared_memory_slots()
 * during exit from program.
 * It recursively frees slots in the shared memory link-list starting
 * from slot passed in param and all slots below(next) it. */
static void free_next_slots_recursively(struct shared_memory_slot *slot)
{
    if (slot) {
        free_next_slots_recursively(slot->next);
        comm_free(slot);
    }
}

/* Function to delete the shared memory link-list.
 * Called during exit from comm_exit in armci_comm_layer.c or
 * gasnet_comm_layer.c.
 */
void coarray_free_all_shared_memory_slots()
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    free_prev_slots_recursively(init_common_slot->prev);
    free_next_slots_recursively(init_common_slot);

    /* update heap usage info */
    mem_info->current_heap_usage = 0;

    free_prev_slots_recursively(child_common_slot->prev);
    free_next_slots_recursively(child_common_slot);

    /* update heap usage info */
    teams_mem_info->current_heap_usage = 0;

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

/* returns size of largest allocatable slot available that is no larger than size, if
 * size is greater than 0, and the largest available overall if size equals 0
 * */
unsigned long largest_allocatable_slot_avail(unsigned long size)
{
    unsigned long retval = 0;
    struct shared_memory_slot *slot;

    if (current_team == NULL || current_team->depth == 0) {
        slot = init_common_slot;
    } else {
        slot = child_common_slot;
    }

    if (size == 0) {
        while (slot) {
            if (slot->feb == 0 && slot->size > retval)
                retval = slot->size;
            slot = slot->prev;
        }
    } else {
        while (slot && retval < size) {
            if (slot->feb == 0 && slot->size > retval)
                retval = slot->size;
            slot = slot->prev;
        }
        if (retval > size)
            retval = size;
    }


    return retval;
}

/* Store the DopeVector with correspoding address into team->allocated_list */
static void store_slot_info(team_type_t * team, void *addr,
                            DopeVectorType * dp)
{
    if (team == NULL) {
        Error("Empty team type in allocate");
        return;
    }
    alloc_dp_slot *s;
    HASH_FIND_PTR(team->allocated_list, &addr, s);
    if (s == NULL) {
        s = (alloc_dp_slot *) malloc(sizeof(alloc_dp_slot));
        s->addr = addr;
        s->dp = dp;
        HASH_ADD_PTR(team->allocated_list, addr, s);
    } else {
        /* Trick here: the implicit coarrays cannot be explicitly deallocated */
        if (s->dp == NULL) {
            s->dp = dp;
        } else {
            Error("Duplicated usage of this slot:%p", s->addr);
        }
    }
}

static void delete_slot_info(team_type_t * team, void *addr)
{
    if (team == NULL) {
        Error("Empty team type in allocate");
        return;
    }
    alloc_dp_slot *s;
    HASH_FIND_PTR(team->allocated_list, &addr, s);
    if (s != NULL && s->dp != NULL) {
        DopeVectorType *dp = s->dp;
        /* As in fort/allocation.c:726~727 */
        dp->assoc = 0;
        dp->ptr_alloc = 0;
    } else if (s == NULL) {
        Error
            ("Cannot free the memory which not allocated in current team");
    } else {
        /*Implicit allocation */
    }

    HASH_DEL(team->allocated_list, s);
    free(s);
    return;
}

/* used for debugging to print memory slots below the specified slot */
#if defined (CAFRT_DEBUG)

static void print_slots_above(struct shared_memory_slot *slot)
{
    FILE *f = NULL;
    int i;
    struct shared_memory_slot *s;

#if defined (TRACE)
    f = __trace_log_stream();
#endif

    if (!f)
        f = stderr;

    fprintf(f,
            "=======================================================\n");
    fprintf(f, "    Allocated Slots at/above slot %p\n", slot->addr);
    fprintf(f,
            "-------------------------------------------------------\n");
    s = slot;
    i = 1;
    while (s) {
        fprintf(f, "\t%d \t %p \t %10ld \t %d\n",
                i, s->addr, s->size, (int) s->feb);
        s = s->prev;
        i++;
    }
    fprintf(f,
            "=======================================================\n");

    fflush(f);
}

static void print_slots_below(struct shared_memory_slot *slot)
{
    FILE *f = NULL;
    int i;
    struct shared_memory_slot *s;

#if defined (TRACE)
    f = __trace_log_stream();
#endif

    if (!f)
        f = stderr;

    fprintf(f,
            "=======================================================\n");
    fprintf(f, "    Allocated Slots at/below slot %p\n", slot->addr);
    fprintf(f,
            "-------------------------------------------------------\n");
    s = slot;
    i = 1;
    while (s) {
        fprintf(f, "\t%d \t %p \t %10ld \t %d\n",
                i, s->addr, s->size, (int) s->feb);
        s = s->next;
        i++;
    }
    fprintf(f,
            "=======================================================\n");

    fflush(f);
}

#pragma weak uhcaf_print_alloc_slots_ = uhcaf_print_alloc_slots
void uhcaf_print_alloc_slots()
{
    FILE *f = NULL;
    int i;
    struct shared_memory_slot *s;

#if defined (TRACE)
    f = __trace_log_stream();
#endif

    if (!f)
        f = stderr;

    fprintf(f, "\n\n");

    /* print all slots above init_common_slot */
    for (i = 0; i < 80; i++) fprintf(f, "=");
    fprintf(f, "\n");
    fprintf(f, "    Allocated Slots for Teams\n");
    for (i = 0; i < 80; i++) fprintf(f, "-");
    fprintf(f, "\n");
    fprintf(f,
            "%5s %20s %15s %5s %15s %15s\n",
            "idx", "address", "size", "FEB", "prev-empty", "next-empty");

    s = child_common_slot;
    while (s->prev) {
        s = s->prev;
    }
    i = 1;
    while (s) {
        fprintf(f, "%5d %20p %15ld %5d %15p %15p\n",
                i, s->addr, s->size, (int) s->feb,
                s->prev_empty ? s->prev_empty->addr : 0,
                s->next_empty ? s->next_empty->addr : 0);
        s = s->next;
        i++;
    }

    /* print all slots above child_common_slot */
    for (i = 0; i < 80; i++) fprintf(f, "=");
    fprintf(f, "\n");
    fprintf(f, "    Allocated Slots for Initial Team\n");
    for (i = 0; i < 80; i++) fprintf(f, "-");
    fprintf(f, "\n");
    fprintf(f,
            "%5s %20s %15s %5s %15s %15s\n",
            "idx", "address", "size", "FEB", "prev-empty", "next-empty");

    s = init_common_slot;
    while (s->prev) {
        s = s->prev;
    }
    i = 1;
    while (s && s != init_common_slot) {
        fprintf(f, "%5d %20p %15ld %5d %15p %15p\n",
                i, s->addr, s->size, (int) s->feb,
                s->prev_empty ? s->prev_empty->addr : 0,
                s->next_empty ? s->next_empty->addr : 0);
        s = s->next;
        i++;
    }
    if (s == init_common_slot) {
        fprintf(f, "%5d %20p %15ld %5d %15p %15p\n",
                i, s->addr, s->size, (int) s->feb,
                s->prev_empty ? s->prev_empty->addr : 0,
                s->next_empty ? s->next_empty->addr : 0);
    }

    /* print all slots below init_common_slot */
    for (i = 0; i < 80; i++) fprintf(f, "=");
    fprintf(f, "\n");
    fprintf(f, "    Allocated Slots for Asymmetric Data\n");
    for (i = 0; i < 80; i++) fprintf(f, "-");
    fprintf(f, "\n");
    fprintf(f,
            "%5s %20s %15s %5s %15s %15s\n",
            "idx", "address", "size", "FEB", "prev-empty", "next-empty");

    s = init_common_slot;
    i = 1;
    while (s) {
        fprintf(f, "%5d %20p %15ld %5d %15p %15p\n",
                i, s->addr, s->size, (int) s->feb,
                s->prev_empty ? s->prev_empty->addr : 0,
                s->next_empty ? s->next_empty->addr : 0);
        s = s->next;
        i++;
    }

    for (i = 0; i < 80; i++) fprintf(f, "=");
    fprintf(f, "\n");
    fprintf(f, "\n\n");

    fflush(f);
}


#pragma weak uhcaf_print_alloc_max_size_ = uhcaf_print_alloc_max_size
void uhcaf_print_alloc_max_size()
{
    FILE *f = NULL;
#if defined (TRACE)
    f = __trace_log_stream();
#endif

    if (!f)
        f = stderr;

    fprintf(f, "*** largest size = %ld\n", largest_allocatable_slot_avail(0));
}

#else

static inline void print_slots_below(struct shared_memory_slot *slot)
{
    return;
}

#endif
