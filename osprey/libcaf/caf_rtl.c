/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2009-2011 University of Houston.

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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "dopevec.h"

#if defined(ARMCI)
#include "armci_comm_layer.h"
#elif defined(GASNET)
#include "gasnet_comm_layer.h"
#endif

#include "caf_rtl.h"
#include "trace.h"

const int DEBUG = 1;

/* initialized in comm_init() */
unsigned long _this_image;
unsigned long _num_images;

/* common_slot is a node in the shared memory link-list that keeps track
 * of available memory that can used for both allocatable coarrays and
 * asymmetric data. It is the only handle to access the link-list.*/
struct shared_memory_slot *common_slot;

void caf_init_()
{
    LIBCAF_TRACE_INIT();

    common_slot = (struct shared_memory_slot *) malloc (
                            sizeof(struct shared_memory_slot));
    START_TIMER();
    comm_init(common_slot); /* common slot is initialized in comm_init */
    STOP_TIMER(INIT);

    _this_image = comm_get_proc_id() + 1;
    _num_images = comm_get_num_procs();

    LIBCAF_TRACE( LIBCAF_LOG_INIT, "caf_rtl.c:caf_init_->initialized,"
            " num_images = %lu", _num_images);
    LIBCAF_TRACE( LIBCAF_LOG_TIME, "comm_init ");
}

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
 *  void* coarray_allocatable_allocate_(unsigned long var_size);
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
 *                          =========
 *                          | Common|
 *                          |  slot |
 *                          =========
 *                          | asymm |
 *                          | heap  |
 *                          |_______|
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
static struct shared_memory_slot* find_empty_shared_memory_slot_above
             (struct shared_memory_slot *slot, unsigned long var_size)
{
    while (slot)
    {
        if(slot->feb==0 && slot->size>=var_size)
            return slot;
        slot = slot->prev;
    }
    return 0;
}

/* Static function used to find empty memory slots while reserving
 * memory for assymetric coarrays */
static struct shared_memory_slot* find_empty_shared_memory_slot_below
             (struct shared_memory_slot *slot, unsigned long var_size)
{
    while (slot)
    {
        if(slot->feb==0 && slot->size>=var_size)
            return slot;
        slot = slot->next;
    }
    return 0;
}

/* Static function used to reserve top part of an empty memory slot
 * for allocatable coarrays. Returns the memory address allocated */
static void* split_empty_shared_memory_slot_from_top
    (struct shared_memory_slot *slot, unsigned long var_size)
{
    struct shared_memory_slot *new_empty_slot;
    new_empty_slot = (struct shared_memory_slot *) malloc
                            (sizeof(struct shared_memory_slot));
    new_empty_slot->addr = slot->addr + var_size;
    new_empty_slot->size = slot->size - var_size;
    new_empty_slot->feb = 0;
    new_empty_slot->next = slot->next;
    new_empty_slot->prev = slot;
    slot->size = var_size;
    slot->feb = 1;
    slot->next = new_empty_slot;
    if(common_slot == slot)
        common_slot = new_empty_slot;
    return slot->addr;
}

/* Static function used to reserve bottom part of an empty memory slot
 * for asymmetric data. Returns the memory address allocated*/
static void* split_empty_shared_memory_slot_from_bottom
    (struct shared_memory_slot *slot, unsigned long var_size)
{
    struct shared_memory_slot *new_full_slot;
    new_full_slot = (struct shared_memory_slot *) malloc
                        (sizeof(struct shared_memory_slot));
    new_full_slot->addr = slot->addr + slot->size - var_size;
    new_full_slot->size = var_size;
    new_full_slot->feb = 1;
    new_full_slot->next = slot->next;
    new_full_slot->prev = slot;
    slot->size = slot->size - var_size;
    slot->next = new_full_slot;
    return new_full_slot->addr;
}

/* Memory allocation function for allocatable coarrays. It is invoked
 * from fortran allocation function _ALLOCATE in
 * osprey/libf/fort/allocation.c
 * It finds empty slot from the shared memory list (common_slot & above)
 * and then splits the slot from top
 * Note: there is barrier as it is a collective operation*/
void* coarray_allocatable_allocate_(unsigned long var_size)
{
    struct shared_memory_slot *empty_slot;
    empty_slot = find_empty_shared_memory_slot_above(common_slot,
                                                      var_size);
    if(empty_slot == 0)
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
        "No More Shared Memory Space available for allocatable coarray.\n"
        "Set env variable UHCAF_IMAGE_HEAP_SIZE or cafrun option "
        "for more space");

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY,"caf_rtl.c:coarray_coarray_allocate"
        "-> Found empty slot %p. About to split it from top."
        ,empty_slot->addr);

    comm_barrier_all(); // implicit barrier in case of allocatable.
    if ( empty_slot!=common_slot && empty_slot->size == var_size )
    {
        empty_slot->feb=1;
        return empty_slot->addr;
    }
    return split_empty_shared_memory_slot_from_top(empty_slot, var_size);
}

/* Memory allocation function for asymmetric data. It is invoked
 * from fortran allocation function _ALLOCATE in
 * osprey/libf/fort/allocation.c
 * It finds empty slot from the shared memory list (common_slot & below)
 * and then splits the slot from bottom */
void* coarray_asymmetric_allocate_(unsigned long var_size)
{
    struct shared_memory_slot *empty_slot;
    empty_slot = find_empty_shared_memory_slot_below(common_slot,
                                                        var_size);
    if(empty_slot == 0)
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
        "No More Shared Memory Space available for asymmetric data.\n"
        "Set env variable UHCAF_IMAGE_HEAP_SIZE or cafrun option "
        "for more space");

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY,"caf_rtl.c:coarray_asymmetric_allocate"
        "-> Found empty slot %p. About to split it from bottom. "
        , empty_slot->addr);

    if ( empty_slot!=common_slot && empty_slot->size == var_size )
    {
        empty_slot->feb=1;
        return empty_slot->addr;
    }
    return split_empty_shared_memory_slot_from_bottom(empty_slot,
                                                        var_size);
}

/* Static function called from coarray_deallocate.
 * It finds the slot with the address (passed in param) by searching
 * the shared memory link-list starting from the slot(passed in param)
 * and above it. Used for finding slots containing allocatable coarrays*/
static struct shared_memory_slot* find_shared_memory_slot_above
                    (struct shared_memory_slot *slot, void *address)
{
    while (slot)
    {
        if(slot->feb==1 && slot->addr==address)
            return slot;
        slot=slot->prev;
    }
    return 0;
}

/* Static function called from coarray_deallocate.
 * It finds the slot with the address (passed in param) by searching
 * the shared memory link-list starting from the slot(passed in param)
 * and below it. Used for finding slots containing asymmetric data*/
static struct shared_memory_slot* find_shared_memory_slot_below
                    (struct shared_memory_slot *slot, void *address)
{
    while (slot)
    {
        if(slot->feb==1 && slot->addr==address)
            return slot;
        slot=slot->next;
    }
    return 0;
}

/* Static function called from empty_shared_memory_slot (used in dealloc).
 * Merge slot with the slot above & below it. If any of these slots is the
 * common-slot, the common-slot points to the merged slot */
static void join_3_shared_memory_slots(struct shared_memory_slot *slot)
{
    slot->prev->size = slot->prev->size + slot->size + slot->next->size;
    slot->prev->next = slot->next->next;
    if(slot->next->next)
        slot->next->next->prev = slot->prev;
    if(common_slot == slot || common_slot == slot->next)
        common_slot=slot->prev;
    comm_free(slot->next);
    comm_free(slot);
}

/* Static function called from empty_shared_memory_slot (used in dealloc).
 * Merge slot with the slot above it. If any of these slots is the
 * common-slot, the common-slot points to the merged slot */
static void join_with_prev_shared_memory_slot
                (struct shared_memory_slot *slot)
{
    slot->prev->size += slot->size;
    slot->prev->next = slot->next;
    if(slot->next)
        slot->next->prev = slot->prev;
    if(common_slot == slot)
        common_slot = slot->prev;
    comm_free(slot);
}

/* Static function called from empty_shared_memory_slot (used in dealloc).
 * Merge slot with the slot below it. If any of these slots is the
 * common-slot, the common-slot points to the merged slot */
static void join_with_next_shared_memory_slot
                (struct shared_memory_slot *slot)
{
    struct shared_memory_slot *tmp;
    tmp = slot->next;
    slot->size += slot->next->size;
    if(slot->next->next)
        slot->next->next->prev = slot;
    slot->next = slot->next->next;
    if(common_slot == tmp)
        common_slot = slot;
    comm_free(tmp);
}

/* Static function called from coarray_deallocate.
 * Empties the slot passed in parameter:
 * 1) set full-empty-bit to 0
 * 2) merge the slot with neighboring empty slots (if found) */
static void empty_shared_memory_slot(struct shared_memory_slot *slot)
{
   slot->feb=0;
   if(slot->prev && (slot->prev->feb==0) && slot->next
                                           && (slot->next->feb==0) )
       join_3_shared_memory_slots(slot);
   else if (slot->prev && (slot->prev->feb==0))
       join_with_prev_shared_memory_slot(slot);
   else if (slot->next && (slot->next->feb==0))
       join_with_next_shared_memory_slot(slot);
}

/* Memory deallocation function for allocatable coarrays and asymmetric
 * data. It is invoked from fortran allocation function _DEALLOCATE in
 * osprey/libf/fort/allocation.c
 * It finds the slot from the shared memory list, set full-empty-bit to 0,
 * and then merge the slot with neighboring empty slots (if found)
 * Note: there is implicit barrier for allocatable coarrays*/
void coarray_deallocate_(void *var_address)
{
    struct shared_memory_slot *slot;
    slot = find_shared_memory_slot_above(common_slot, var_address);
    if (slot)
        comm_barrier_all(); //implicit barrier for allocatable
    else
        slot = find_shared_memory_slot_below(common_slot, var_address);
    if (slot == 0)
    {
        LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
            "caf_rtl.c:coarray_deallocate_->Address%p not coarray."
            ,var_address);
        return;
    }
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY,
            "caf_rtl.c:coarray_deallocate_->before deallocating %p.", var_address);
    empty_shared_memory_slot(slot);

}

/* Static function called from coarray_free_all_shared_memory_slots()
 * during exit from program.
 * It recursively frees slots in the shared memory link-list starting
 * from slot passed in param and all slots above(previous) it. */
static void free_prev_slots_recursively( struct shared_memory_slot *slot )
{
    if(slot)
    {
        free_prev_slots_recursively(slot->prev);
        comm_free(slot);
    }
}

/* Static function called from coarray_free_all_shared_memory_slots()
 * during exit from program.
 * It recursively frees slots in the shared memory link-list starting
 * from slot passed in param and all slots below(next) it. */
static void free_next_slots_recursively( struct shared_memory_slot *slot )
{
    if(slot)
    {
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
    free_prev_slots_recursively(common_slot->prev);
    free_next_slots_recursively(common_slot);
}

/* end shared memory management functions*/

void caf_exit_(int status)
{
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "caf_rtl.c:caf_exit_->Exiting with error code %d",status);
    comm_exit(status);
}

void caf_finalize_()
{
    LIBCAF_TRACE( LIBCAF_LOG_TIME_SUMMARY, "Accumulated Time:");
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "caf_rtl.c:caf_finalize_->Before call to comm_finalize");
    comm_finalize();
}

void acquire_lcb_(unsigned long buf_size, void **ptr)
{
    *ptr = comm_malloc(buf_size);
    LIBCAF_TRACE( LIBCAF_LOG_DEBUG, "caf_rtl.c:acquire_lcb->"
            " acquired lcb %p of size %lu", *ptr, buf_size);
}

void release_lcb_(void **ptr)
{
    comm_free_lcb(*ptr);
    LIBCAF_TRACE( LIBCAF_LOG_DEBUG, "caf_rtl.c:release_lcb_->"
            "freed lcb %p", *ptr);
}

void sync_all_()
{
   LIBCAF_TRACE( LIBCAF_LOG_BARRIER, "caf_rtl.c:sync_all_->"
           "before call to comm_barrier_all");
   START_TIMER();
   comm_barrier_all();
   STOP_TIMER(SYNC);
   LIBCAF_TRACE( LIBCAF_LOG_TIME, "comm_sync_all ");

}

void sync_memory_()
{
   LIBCAF_TRACE( LIBCAF_LOG_BARRIER, "caf_rtl.c:sync_memory->"
           "in sync memory");
}

void sync_images_( int *imageList, int imageCount)
{
    int i;
    for ( i=0; i<imageCount ; i++)
    {
        LIBCAF_TRACE( LIBCAF_LOG_BARRIER,"caf_rtl.c:sync_images_->Before"
        " call to comm_syncimages for sync with img%d",imageList[i]);
        imageList[i]--;
    }
    START_TIMER();
    comm_sync_images(imageList,imageCount);
    STOP_TIMER(SYNC);
    LIBCAF_TRACE( LIBCAF_LOG_TIME, "comm_sync_images ");
}

void sync_images_all_()
{
    int i;
    int imageCount=_num_images;
    int *imageList;
    LIBCAF_TRACE( LIBCAF_LOG_BARRIER, "caf_rtl.c:sync_images_all_->"
        "before call to comm_sync_images for sync with all images");
    imageList = (int *)comm_malloc(_num_images*sizeof(int));
    for (i=0; i<imageCount ; i++)
        imageList[i]=i;
    START_TIMER();
    comm_sync_images(imageList,imageCount);
    STOP_TIMER(SYNC);
    LIBCAF_TRACE( LIBCAF_LOG_TIME, "comm_sync_image_all ");

    comm_free(imageList);
}

int image_index_(DopeVectorType *diminfo, DopeVectorType *sub)
{
    if ( diminfo == NULL || sub == NULL )
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
           "caf_rtl.c:image_index_-> image_index failed for "
           "&diminfo=%p, &codim=%p", diminfo,sub);
    }

    int i;
    int rank = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int image = 0;
    int lb_codim, ub_codim;
    int *codim = (int *)sub->base_addr.a.ptr;
    int str_m = 1;


    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "caf_rtl.c:image_index_->rank: %d, corank %d", rank, corank);
    if (sub->dimension[0].extent != corank)
        return 0;

    for (i = 0; i < corank; i++) {
        int extent;
        str_m = diminfo->dimension[rank+i].stride_mult;
        if (i == (corank-1))
            extent = (_num_images-1) / str_m + 1;
        else
            extent = diminfo->dimension[rank+i].extent;
        lb_codim = diminfo->dimension[rank+i].low_bound;
        ub_codim = diminfo->dimension[rank+i].low_bound +
                    extent - 1;
        if (codim[i]>=lb_codim && (ub_codim==0 || codim[i]<=ub_codim)) {
            image += str_m * (codim[i] - lb_codim);
        }
        else{
            return 0;
        }
    }

    if( _num_images > image )
        return image+1;
    else
        return 0;
}

int this_image3_(DopeVectorType *diminfo, int* sub)
{
    int img = _this_image-1;
    int rank = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int dim = *sub;
    int str_m = 1;
    int lb_codim=0;
    int ub_codim=0;
    int extent,i;

    if ( diminfo == NULL )
    {
       LIBCAF_TRACE(LIBCAF_LOG_FATAL,
       "caf_rtl.c:this_image3_ ->this_image failed for &diminfo=%p",
       diminfo);
    }
    if(dim < 1 || dim > corank)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
           "caf_rtl.c:this_image3_->this_image failed as %d dim"
           " is not present", dim);
    }

    lb_codim = diminfo->dimension[rank+dim-1].low_bound;
    str_m = diminfo->dimension[rank+dim-1].stride_mult;
    if (dim == corank)
      extent = (_num_images-1) / str_m + 1;
    else
      extent = diminfo->dimension[rank+dim-1].extent;
    ub_codim = lb_codim + extent - 1;
    if(ub_codim > 0){
        return (((img/str_m)%extent)+lb_codim);
    }
    else{
        return ((img/str_m)+lb_codim);
    }
}

void this_image2_(DopeVectorType *ret, DopeVectorType *diminfo)
{
    int i;
    int corank = diminfo->n_codim;
    int *ret_int;
    if ( diminfo == NULL || ret==NULL)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
           "caf_rtl.c:this_image2_ ->this_image failed for "
           "&diminfo:%p and &ret:%p",diminfo, ret);
    }
    ret->base_addr.a.ptr = comm_malloc(sizeof(int)*corank);
    ret->dimension[0].low_bound = 1;
    ret->dimension[0].extent = corank;
    ret->dimension[0].stride_mult = 1;
    ret_int = (int*)ret->base_addr.a.ptr;
    for (i=1; i<=corank; i++)
    {
        ret_int[i-1] = this_image3_(diminfo, &i);
    }
}

int lcobound2_(DopeVectorType *diminfo, int *sub)
{
    int rank   = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int dim=*sub;
    if ( diminfo == NULL )
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
           "caf_rtl.c:lcobound2 ->lcobound failed for &diminfo:%p",
           diminfo);
    }
    if(dim < 1 || dim > corank)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
           "caf_rtl.c:lcobound2 ->lcobound failed as dim %d not present",
            dim);
    }
    return diminfo->dimension[rank+dim-1].low_bound;
}

void lcobound_(DopeVectorType *ret, DopeVectorType *diminfo)
{
    int i;
    int rank   = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int *ret_int;
    if ( diminfo == NULL || ret==NULL)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
       "caf_rtl.c:lcobound ->lcobound failed for diminfo:%p and ret:%p",
        diminfo, ret);
    }
    ret->base_addr.a.ptr = comm_malloc(sizeof(int)*corank);
    ret->dimension[0].low_bound = 1;
    ret->dimension[0].extent = corank;
    ret->dimension[0].stride_mult = 1;
    ret_int = (int*)ret->base_addr.a.ptr;
    for (i=0; i<corank; i++)
    {
        ret_int[i] = diminfo->dimension[rank+i].low_bound;
    }
}

int ucobound2_(DopeVectorType *diminfo, int *sub)
{
    int rank   = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int dim=*sub;
    int extent;
    if ( diminfo == NULL )
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
       "caf_rtl.c:ucobound2 ->ucobound failed for &diminfo:%p",diminfo);
    }
    if(dim < 1 || dim > corank)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
       "caf_rtl.c:ucobound2 ->ucobound failed as dim %d not present",dim);
    }

    if (dim == corank)
      extent = (_num_images-1) /
          diminfo->dimension[rank+dim-1].stride_mult + 1;
    else
      extent = diminfo->dimension[rank+dim-1].extent;

    return (diminfo->dimension[rank+dim-1].low_bound +
                extent - 1);
}

void ucobound_(DopeVectorType *ret, DopeVectorType *diminfo)
{
    int i;
    int rank   = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int *ret_int;
    int extent;
    if ( diminfo == NULL || ret==NULL)
    {
      LIBCAF_TRACE(LIBCAF_LOG_FATAL,
      "caf_rtl.c:ucobound ->ucobound failed for diminfo:%p and ret:%p",
       diminfo, ret);
    }
    ret->base_addr.a.ptr = comm_malloc(sizeof(int)*corank);
    ret->dimension[0].low_bound = 1;
    ret->dimension[0].extent = corank;
    ret->dimension[0].stride_mult = 1;
    ret_int = (int*)ret->base_addr.a.ptr;
    for (i=0; i<corank; i++)
    {
      if (i == (corank-1))
        extent = (_num_images-1) /
            diminfo->dimension[rank+i].stride_mult + 1;
      else
        extent = diminfo->dimension[rank+i].extent;

      ret_int[i] = diminfo->dimension[rank+i].low_bound +
                    extent - 1;
    }
}

void coarray_read_full_str_(void * src, void *dest, unsigned int src_ndim,
        unsigned long *src_strides, unsigned long *src_extents,
        unsigned int dest_ndim, unsigned long *dest_strides,
        unsigned long *dest_extents, unsigned long img)
{
    int i, is_contig = 1;

    for (i = 1; i < src_ndim; i++) {
      if (src_strides[i] != (src_strides[i-1]*src_extents[i-1])) {
        is_contig = 0;
        break;
      }
    }

    if (is_contig) {
      for (i = 1; i < dest_ndim; i++) {
        if (dest_strides[i] != (dest_strides[i-1]*dest_extents[i-1])) {
          is_contig = 0;
          break;
        }
      }
    }

    if (is_contig) {
      unsigned long xfer_size = src_strides[0]*src_extents[0];
      for (i = 1; i < src_ndim; i++) {
        xfer_size *= src_extents[i];
      }
      if (DEBUG) {
        unsigned long dest_xfer_size = dest_strides[0]*dest_extents[0];
        for (i = 1; i < dest_ndim; i++) {
          dest_xfer_size *= dest_extents[i];
        }
        if (dest_xfer_size != xfer_size) {
          LIBCAF_TRACE(LIBCAF_LOG_FATAL,
              "caf_rtl.c:coarray_read_full_str->dest and src xfer_size must"
              " be same. xfer_size=%d, dest_xfer_size=%d",
              xfer_size, dest_xfer_size);
        }
      }
      coarray_read_(src, dest, xfer_size, img);
      return;
      /* not reached */
    }

   START_TIMER();
   comm_read_full_str(src, dest, src_ndim, src_strides, src_extents,
                   dest_ndim, dest_strides, dest_extents, img-1);
   STOP_TIMER(READ);
   LIBCAF_TRACE( LIBCAF_LOG_TIME, "comm_read_strided ");

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "caf_rtl.c:coarray_read_full_str->Finished read(strided) "
            "from %p on Img %lu to %p using dim %d ",
            src, img, dest, src_ndim);
}


void coarray_read_src_str_(void * src, void *dest, unsigned int ndim,
        unsigned long *src_strides, unsigned long *src_extents,
        unsigned long img)
{
    int i, is_contig = 1;
    for (i = 1; i < ndim; i++) {
      if (src_strides[i] != (src_strides[i-1]*src_extents[i-1])) {
        is_contig = 0;
        break;
      }
    }

    if (is_contig) {
      unsigned long xfer_size = src_strides[0]*src_extents[0];
      for (i = 1; i < ndim; i++) {
        xfer_size *= src_extents[i];
      }
      coarray_read_(src, dest, xfer_size, img);
      return;
      /* not reached */
    }

   START_TIMER();
   comm_read_src_str(src, dest, ndim, src_strides, src_extents, img-1);
   STOP_TIMER(READ);
   LIBCAF_TRACE( LIBCAF_LOG_TIME, "comm_read_strided ");

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "caf_rtl.c:coarray_read_src_str->Finished read(strided) "
            "from %p on Img %lu to %p using dim %d ",
            src, img, dest, ndim);
}

void coarray_read_(void * src, void * dest, unsigned long xfer_size,
        unsigned long img)
{
    START_TIMER();
    comm_read(src, dest, xfer_size, img-1);//reads from src on img to dest
    STOP_TIMER(READ);
    LIBCAF_TRACE( LIBCAF_LOG_TIME, "comm_read ");

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "caf_rtl.c:coarray_read->Finished read from %p on Img %lu to %p"
        " data of size %lu ", src, img, dest, xfer_size);
}

void coarray_write_dest_str_(void * dest, void *src, unsigned int ndim,
        unsigned long *dest_strides, unsigned long *dest_extents,
        unsigned long img)
{
    int i, is_contig = 1;
    for (i = 1; i < ndim; i++) {
      if (dest_strides[i] != (dest_strides[i-1]*dest_extents[i-1])) {
        is_contig = 0;
        break;
      }
    }
    if (is_contig) {
      unsigned long xfer_size = dest_strides[0]*dest_extents[0];
      for (i = 1; i < ndim; i++) {
        xfer_size *= dest_extents[i];
      }
      coarray_write_(dest, src, xfer_size, img);
      return;
      /* not reached */
    }

    START_TIMER();
    comm_write_dest_str(dest, src, ndim,dest_strides,dest_extents, img-1);
    STOP_TIMER(WRITE);
    LIBCAF_TRACE( LIBCAF_LOG_TIME, "comm_write_strided ");

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "caf_rtl.c:coarray_write_dest_str->Finished write(strided) to %p"
        " on Img %lu from %p using dim %d ", dest, img, src, ndim);
}

void coarray_write_(void * dest, void * src, unsigned long xfer_size, unsigned long img)
{
    START_TIMER();
    comm_write(dest, src, xfer_size, img-1);//write to dest in img
    STOP_TIMER(WRITE);
    LIBCAF_TRACE( LIBCAF_LOG_TIME, "comm_write ");

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "caf_rtl.c:coarray_write->Wrote to %p on Img %lu from %p data of"
        " size %lu ", dest, img, src, xfer_size);
}

void coarray_write_full_str_(void * dest, void *src,
        unsigned int dest_ndim,
        unsigned long *dest_strides, unsigned long *dest_extents,
        unsigned int src_ndim,
        unsigned long *src_strides, unsigned long *src_extents,
        unsigned long img)
{
    int i, is_contig = 1;

    for (i = 1; i < dest_ndim; i++) {
      if (dest_strides[i] != (dest_strides[i-1]*dest_extents[i-1])) {
        is_contig = 0;
        break;
      }
    }

    if (is_contig) {
      for (i = 1; i < src_ndim; i++) {
        if (src_strides[i] != (src_strides[i-1]*src_extents[i-1])) {
          is_contig = 0;
          break;
        }
      }
    }

    if (is_contig) {
      unsigned long xfer_size = dest_strides[0]*dest_extents[0];
      for (i = 1; i < dest_ndim; i++) {
        xfer_size *= dest_extents[i];
      }
      if (DEBUG) {
        unsigned long src_xfer_size = src_strides[0]*src_extents[0];
        for (i = 1; i < src_ndim; i++) {
          src_xfer_size *= src_extents[i];
        }
        if (src_xfer_size != xfer_size) {
          LIBCAF_TRACE(LIBCAF_LOG_FATAL,
              "caf_rtl.c:coarray_write_full_str->dest and src xfer_size must"
              " be same. xfer_size=%d, src_xfer_size=%d",
              xfer_size, src_xfer_size);
        }
      }
      coarray_write_(dest, src, xfer_size, img);
      return;
      /* not reached */
    }

    START_TIMER();
    comm_write_full_str(dest, src, dest_ndim, dest_strides, dest_extents,
                    src_ndim, src_strides, src_extents, img-1);
    STOP_TIMER(WRITE);
    LIBCAF_TRACE( LIBCAF_LOG_TIME, "comm_write_strided ");

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "caf_rtl.c:coarray_write_full_str->Finished write(strided) to %p"
        " on Img %lu from %p using dim %d ", dest, img, src, dest_ndim);
}


/* COLLECTIVES */

/* supplemental functions for collective subroutines
 * Borrowed from Joon's code */

static int my_pow2(int exp)
{
    int result=1;
    result <<= exp ;
    return result ;
}

static int is_even()
{
    return (_num_images % 2) ? 0:1 ;
}

static double mylog2(double exp)
{
    return log10(exp)/log10(2);
}

static double myceillog2(double exp)
{
    return ceil(log10(exp)/log10(2));
}

/* Add the value in buf to dope vector dst_dv */
static void dope_add( void *buf, DopeVectorType *dst_dv,
                        int total_bytes )
{
    int el_type  = dst_dv->type_lens.type;
    void *dst_ptr = dst_dv->base_addr.a.ptr;
    int i;
    unsigned int el_len;
    el_len = dst_dv->base_addr.a.el_len >>3; // convert bits to bytes

    switch (el_type)
    {
        case  DVTYPE_INTEGER: 
        {
            for(i=0; i< total_bytes/el_len;i++)
            {
                *((int*)dst_ptr + i) += *((int*)buf + i);
            }
            break;
        }
        case  DVTYPE_REAL:
        {
            for(i=0; i< total_bytes/el_len;i++)
            {
                *((float*)dst_ptr + i) += *((float*)buf + i);
            } 
            break;
        }
        default :
        { break; }
    }
} 

/* COSUM  (modified Joon's armci_comaxval function) */
/* Accumulates the value of src_dv on all images and stores it into sum_dv
 * of root */
void comm_cosum(DopeVectorType *src_dv, DopeVectorType *sum_dv,int root)
{    
    int i,iter;
    int total_iter = (int) myceillog2(_num_images) ;
    unsigned int el_len;
    unsigned int target;
    void *local_buf;
    int total_bytes =1;

    // initialization
    el_len = src_dv->base_addr.a.el_len >>3; // convert bits to bytes
    for(i=0; i<src_dv->n_dim ; i++)
      total_bytes *= src_dv->dimension[i].extent;
    local_buf = malloc(total_bytes);
    memset(local_buf, 0 , total_bytes);
    total_bytes *=el_len; 
    // copy content of dopevector from src to sum locally
    memcpy(sum_dv->base_addr.a.ptr, src_dv->base_addr.a.ptr, total_bytes);

    // swap processed ID between 0 and root (non zero process ID)
    int vPID = (_this_image == root ) ? 
      0 : (_this_image == 0) ? root : _this_image;

    // do reduction                                                       
    for(iter=0; iter<total_iter; iter++)
    {
      if( (vPID % my_pow2(iter+1)) == 0 )
      {
        if( (vPID + my_pow2(iter)) < _num_images)
        {
          // compute target process IDs for data transfer
          target = vPID + my_pow2(iter);

          //swap back for process Id 0 and root process(non-zero) 
          if(target == root) target=0;

          comm_read(src_dv->base_addr.a.ptr, local_buf, total_bytes, target);

          dope_add(local_buf, sum_dv, total_bytes);
        }
      }
      comm_barrier_all();
    }    

    free(local_buf);
    //Broadcast for all to all
}
