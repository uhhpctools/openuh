/*
 SHMEM Communication Layer for supporting Coarray Fortran

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include "caf_rtl.h"
#include "comm.h"
#include "alloc.h"
#include "env.h"
#include "shmem_comm_layer.h"
#include "trace.h"
#include "util.h"
#include "collectives.h"
#include "profile.h"
#include "utlist.h"
#include "team.h"

const size_t LARGE_COMM_BUF_SIZE = 120 * 1024;

const size_t SMALL_XFER_SIZE = 200;     /* size for which local mem copy would be
                                           advantageous */

extern co_reduce_t co_reduce_algorithm;

extern unsigned long _this_image;
extern unsigned long _num_images;
extern unsigned long _log2_images;
extern unsigned long _rem_images;
extern team_type     initial_team;
extern mem_usage_info_t * mem_info;
extern mem_usage_info_t * teams_mem_info;
extern size_t alloc_byte_alignment;

extern int rma_prof_rid;

/* init_common_slot and child_common_slot is a node in the shared memory
 * link-list that keeps track of available memory that can used for both
 * allocatable coarrays and asymmetric data.
 *
 * TEAM_SUPPORT: separate the common_slot into two.
 *
 * One is init_common_slot. Initialized in comm_init(). Keep tracking of
 * shared memory of initial team.
 *
 * Another is child_comm_slot. Keep tracking the
 * comm_slot of teams other than team_world comm_slot.
 */
extern shared_memory_slot_t *init_common_slot;
extern shared_memory_slot_t * child_common_slot;

extern int enable_collectives_2level;
extern int enable_reduction_2level;
extern int enable_broadcast_2level;
extern int enable_collectives_mpi;
extern int enable_collectives_use_canary;
extern int mpi_collectives_available;
extern void *collectives_buffer[2];
extern size_t collectives_bufsize;
extern void *allreduce_buffer[2];
extern size_t allreduce_bufsize;
extern void *reduce_buffer[2];
extern size_t reduce_bufsize;
extern void *broadcast_buffer[2];
extern size_t broadcast_bufsize;
extern int collectives_max_workbufs;

/*
 * team_info_t list, used for exhanging team_info structure within form_team.
 * Allocated in comm_init.
 */
extern team_info_t * exchange_teaminfo_buf;

/*
 * Global team stack, initialized ini comm_init()
 */
extern team_stack_t * global_team_stack;

/*
 * Static Variable declarations
 */
static unsigned long my_proc;
static unsigned long num_procs;

static unsigned long static_symm_data_total_size = 0;

/* for critical section lock */
static long *critical_naive_lock;

/* sync images */
static sync_images_t sync_images_algorithm;
typedef union {
    int value; /* used for counter and ping-pong */
    struct {
        short sense;
        short val;
    } t; /* used for sense reversing */
} sync_flag_t;
static sync_flag_t *sync_flags = NULL;

team_barrier_t team_barrier_algorithm;

/* rma ordering */
static rma_ordering_t rma_ordering;

/*Tracking num of leaders, proc_id of leaders */
int total_num_supernodes;

/* Shared memory management:
 * coarray_start_all_images stores the shared memory start address of all
 * images */
static void **coarray_start_all_images = NULL;  /* local address space */
static void **start_shared_mem_address = NULL;  /* different address spaces */

/* image stoppage info */
static int *error_stopped_image_exists = NULL;
static int *this_image_stopped = NULL;
static char *stopped_image_exists = NULL;
static int in_error_termination = 0;
static int in_normal_termination = 0;

static size_t nb_xfer_limit;

/* strided algorithms selection */
const char *str_alg;
const static char *stride_algorithms[2] = {
  "DEFAULT",
  "2DIM"
};


/* GET CACHE OPTIMIZATION */
static int enable_get_cache;    /* set by env variable */
static int get_cache_sync_refetch; /* set by env variable */
static unsigned long getcache_block_size;        /* set by env var. */
/* Instead of making the cache_all_image an array of struct cache, I
 * make it an array of pointer to struct cache. This will make it easy
 * to add more cache lines in the future by making it 2D array */
static struct cache **cache_all_images;
static unsigned long shared_memory_size;

/* Mutexes */
static int critical_mutex;


/* Local function declarations */
void *get_remote_address(void *src, size_t img);

static inline void check_for_error_stop();
static void clear_all_cache();
static void clear_cache(size_t node);
static int cache_check_and_get(size_t node, void *remote_address,
                                size_t nbytes, void *local_address);
static int cache_check_and_get_strided(void *remote_src,
                                        int src_strides[],
                                        void *local_dest,
                                        int dest_strides[], int count[],
                                        size_t stride_levels, size_t node);
static void update_cache(size_t node, void *remote_address, size_t nbytes,
                         void *local_address);

static void local_strided_copy(void *src, const size_t src_strides[],
                               void *dest, const size_t dest_strides[],
                               const size_t count[], size_t stride_levels);

static void remote_strided_get(void *src, const size_t src_strides[],
                               void *dest, const size_t dest_strides[],
                               const size_t count[], size_t stride_levels,
                               int proc);

static void remote_strided_put(void *src, const size_t src_strides[],
                               void *dest, const size_t dest_strides[],
                               const size_t count[], size_t stride_levels,
                               int proc);

static void strided_get(void *remote_src, const size_t src_strides[],
                        void *dest, const size_t dest_strides[],
                        const size_t count[], size_t stride_levels,
                        int proc);

static void strided_put(void *src, const size_t src_strides[],
                        void *remote_dest, const size_t dest_strides[],
                        const size_t count[], size_t stride_levels,
                        int proc);

static struct handle_list *get_next_handle(unsigned long proc,
                                           void *remote_address,
                                           void *local_buf,
                                           unsigned long size,
                                           access_type_t access_type);
static void reset_min_nb_address(unsigned long proc,
                                 access_type_t access_type);
static void reset_max_nb_address(unsigned long proc,
                                 access_type_t access_type);
static void delete_node(unsigned long proc, struct handle_list *node,
                        access_type_t access_type);
static int address_in_handle(struct handle_list *handle_node,
                             void *address, unsigned long size);
static int address_in_nb_address_block(void *remote_addr, size_t proc,
                                       size_t size,
                                       access_type_t access_type);
static void update_nb_address_block(void *remote_addr, size_t proc,
                                    size_t size,
                                    access_type_t access_type);
static void wait_on_pending_accesses(size_t proc,
                                     void *remote_address,
                                     size_t size,
                                     access_type_t access_type);
static void wait_on_all_pending_accesses_to_proc(unsigned long proc);
static void wait_on_all_pending_accesses();
static void wait_on_all_pending_puts_to_proc(unsigned long proc);
static void wait_on_all_pending_puts();


#ifdef SYNC_IMAGES_HASHED
static void sync_images_counter(hashed_image_list_t *image_list,
                      int image_count, int *status, int stat_len,
                      char *errmsg, int errmsg_len);
static void sync_images_ping_pong(hashed_image_list_t *image_list,
                      int image_count, int *status, int stat_len,
                      char *errmsg, int errmsg_len);
static void sync_images_sense_rev(hashed_image_list_t *image_list,
                      int image_count, int *status, int stat_len,
                      char *errmsg, int errmsg_len);
#else
static void sync_images_counter(int *image_list,
                      int image_count, int *status, int stat_len,
                      char *errmsg, int errmsg_len);
static void sync_images_ping_pong(int *image_list,
                      int image_count, int *status, int stat_len,
                      char *errmsg, int errmsg_len);
static void sync_images_sense_rev(int *image_list,
                      int image_count, int *status, int stat_len,
                      char *errmsg, int errmsg_len);
#endif

static void sync_all_dissemination_mcs(int *stat, int stat_len,
		char * errmsg, int errmsg_len, team_type_t *team);

void set_static_symm_data(void *base_address, size_t alignment);
unsigned long get_static_symm_size(size_t alignment,
                                   size_t collectives_bufsize);


/* must call comm_init() first */
inline size_t comm_get_proc_id()
{
    return my_proc;
}

/* must call comm_init() first */
inline size_t comm_get_num_procs()
{
    return num_procs;
}

/* must call comm_init() first */
size_t comm_get_node_id(size_t proc)
{
    return (size_t) proc;
}

void *comm_get_sharedptr(void *addr, size_t proc)
{
  if (proc == my_proc)
    return addr;

  return NULL;
}


static int mpi_initialized_by_shmem = 0;

#pragma weak PMPI_Init

int MPI_Init(int *argc, char ***argv)
{
    int ret = MPI_SUCCESS;

    if (!mpi_initialized_by_shmem) {
        ret = PMPI_Init(argc, argv);
    }

    return ret;
}

void mpi_init_(int *ierr)
{
    int argc;
    char **argv;
    int flag;
    *ierr = MPI_SUCCESS;

    if (!mpi_initialized_by_shmem) {
        argc = ARGC;
        argv = ARGV;
        *ierr = PMPI_Init(&argc, &argv);
    }
}

#pragma weak PMPI_Finalize

int MPI_Finalize()
{
    int ret = MPI_SUCCESS;
    if (!mpi_initialized_by_shmem ||
        in_error_termination || in_normal_termination) {
        ret = PMPI_Finalize();
    }

    return ret;
}

void mpi_finalize_(int *ierr)
{
    int flag;
    *ierr = MPI_SUCCESS;

    if (!mpi_initialized_by_shmem ||
        in_error_termination || in_normal_termination) {
        *ierr = PMPI_Finalize();
    }
}


/**************************************************************
 *       Shared (RMA) Memory Segment Address Ranges
 **************************************************************/

inline void *comm_start_shared_mem(size_t proc)
{
    return get_remote_address(coarray_start_all_images[my_proc], proc);
}

inline void *comm_start_symmetric_mem(size_t proc)
{
    return comm_start_shared_mem(proc);
}

inline void *comm_start_static_data(size_t proc)
{
    return comm_start_shared_mem(proc);
}

inline void *comm_end_static_data(size_t proc)
{
    return (char *) comm_start_shared_mem(proc) +
        static_symm_data_total_size;
}

inline void *comm_start_allocatable_heap(size_t proc)
{
    return comm_end_static_data(proc);
}

inline void *comm_end_allocatable_heap(size_t proc)
{
    return comm_end_symmetric_mem(proc);
}

inline void *comm_end_symmetric_mem(size_t proc)
{
    /* we can't directly use common_slot->addr as the argument, because the
     * translation algorithm will treat it as part of the asymmetric memory
     * space */
    return get_remote_address(init_common_slot->addr - 1, proc) + 1;
}

inline void *comm_start_asymmetric_heap(size_t proc)
{
    if (proc != my_proc) {
        return comm_end_symmetric_mem(proc);
    } else {
        return (char *) init_common_slot->addr + init_common_slot->size;
    }
}

inline void *comm_end_asymmetric_heap(size_t proc)
{
    return comm_end_shared_mem(proc);
}

inline void *comm_end_shared_mem(size_t proc)
{
    return comm_start_shared_mem(proc) + shared_memory_size;
}

static inline int address_in_symmetric_mem(void *addr)
{
    void *start_symm_mem;
    void *end_symm_mem;

    start_symm_mem = coarray_start_all_images[my_proc];
    end_symm_mem = init_common_slot->addr;

    return (addr >= start_symm_mem && addr < end_symm_mem);
}

static inline int address_in_shared_mem(void *addr)
{
    void *start_shared_mem;
    void *end_shared_mem;

    start_shared_mem = coarray_start_all_images[my_proc];
    end_shared_mem = start_shared_mem + shared_memory_size;

    return (addr >= start_shared_mem && addr < end_shared_mem);
}

static inline int remote_address_in_shared_mem(size_t proc, void *address)
{
    return !((address < comm_start_symmetric_mem(my_proc) ||
              address >= comm_end_symmetric_mem(my_proc)) &&
             (address < comm_start_asymmetric_heap(proc) ||
              address >= comm_end_asymmetric_heap(proc)));
}

static inline long get_leader( )
{
	if(current_team != NULL)
		return current_team->intranode_set[1];
	else
		return -1;
}

int comm_address_in_shared_mem(void *addr)
{
    void *start_shared_mem;
    void *end_shared_mem;

    start_shared_mem = coarray_start_all_images[my_proc];
    end_shared_mem = start_shared_mem + shared_memory_size;

    return (addr >= start_shared_mem && addr < end_shared_mem);
}


/**************************************************************
 *                Memory Copy Helper Routines
 **************************************************************/

/* naive implementation of strided copy
 * TODO: improve by finding maximal blocksize
 */
static void local_strided_copy(void *src, const size_t src_strides[],
                               void *dest, const size_t dest_strides[],
                               const size_t count[], size_t stride_levels)
{
    int i, j;
    size_t num_blks;
    size_t cnt_strides[stride_levels + 1];
    size_t blockdim_size[stride_levels];
    void *dest_ptr = dest;
    void *src_ptr = src;

    /* assuming src_elem_size=dst_elem_size */
    size_t blk_size = count[0];
    num_blks = 1;
    cnt_strides[0] = 1;
    blockdim_size[0] = count[0];
    for (i = 1; i <= stride_levels; i++) {
        cnt_strides[i] = cnt_strides[i - 1] * count[i];
        blockdim_size[i] = blk_size * cnt_strides[i];
        num_blks *= count[i];
    }

    for (i = 1; i <= num_blks; i++) {
        memcpy(dest_ptr, src_ptr, blk_size);
        for (j = 1; j <= stride_levels; j++) {
            if (i % cnt_strides[j])
                break;
            src_ptr -= (count[j] - 1) * src_strides[j - 1];
            dest_ptr -= (count[j] - 1) * dest_strides[j - 1];
        }
        src_ptr += src_strides[j - 1];
        dest_ptr += dest_strides[j - 1];
    }
}

static void remote_strided_get(void *src, const size_t src_strides[],
                        void *dest, const size_t dest_strides[],
                        const size_t count[], size_t stride_levels,
                        int proc)
{
    int i, j;
    size_t num_blks;
    size_t cnt_strides[stride_levels + 1];
    size_t blockdim_size[stride_levels];
    void *dest_ptr = dest;
    void *src_ptr = src;

    /* assuming src_elem_size=dst_elem_size */
    size_t blk_size = count[0];
    num_blks = 1;
    cnt_strides[0] = 1;
    blockdim_size[0] = count[0];
    for (i = 1; i <= stride_levels; i++) {
        cnt_strides[i] = cnt_strides[i - 1] * count[i];
        blockdim_size[i] = blk_size * cnt_strides[i];
        num_blks *= count[i];
    }

    for (i = 1; i <= num_blks; i++) {
        shmem_getmem(dest_ptr, src_ptr, blk_size, proc);
        for (j = 1; j <= stride_levels; j++) {
            if (i % cnt_strides[j])
                break;
            src_ptr -= (count[j] - 1) * src_strides[j - 1];
            dest_ptr -= (count[j] - 1) * dest_strides[j - 1];
        }
        src_ptr += src_strides[j - 1];
        dest_ptr += dest_strides[j - 1];
    }
}

static void remote_strided_put(void *src, const size_t src_strides[],
                        void *dest, const size_t dest_strides[],
                        const size_t count[], size_t stride_levels,
                        int proc)
{
    int i, j;
    size_t num_blks;
    size_t cnt_strides[stride_levels + 1];
    size_t blockdim_size[stride_levels];
    void *dest_ptr = dest;
    void *src_ptr = src;

    /* assuming src_elem_size=dst_elem_size */
    size_t blk_size = count[0];
    num_blks = 1;
    cnt_strides[0] = 1;
    blockdim_size[0] = count[0];
    for (i = 1; i <= stride_levels; i++) {
        cnt_strides[i] = cnt_strides[i - 1] * count[i];
        blockdim_size[i] = blk_size * cnt_strides[i];
        num_blks *= count[i];
    }

    for (i = 1; i <= num_blks; i++) {
        shmem_putmem(dest_ptr, src_ptr, blk_size, proc);
        for (j = 1; j <= stride_levels; j++) {
            if (i % cnt_strides[j])
                break;
            src_ptr -= (count[j] - 1) * src_strides[j - 1];
            dest_ptr -= (count[j] - 1) * dest_strides[j - 1];
        }
        src_ptr += src_strides[j - 1];
        dest_ptr += dest_strides[j - 1];
    }
}

static void strided_get(void *remote_src, const size_t src_strides[],
                        void *dest, const size_t dest_strides[],
                        const size_t count[], size_t stride_levels,
                        int proc)
{
    int i, j;
    size_t num_blks;
    size_t cnt_strides[stride_levels + 1];
    size_t blockdim_size[stride_levels];
    void *dest_ptr = dest;
    void *src_ptr = remote_src;
    int base_dimension, buffer_dimension;

    /* assuming src_elem_size=dst_elem_size */
    size_t blk_size = count[0];
    num_blks = 1;
    cnt_strides[0] = 1;
    blockdim_size[0] = count[0];
    for (i = 1; i <= stride_levels; i++) {
        cnt_strides[i] = cnt_strides[i - 1] * count[i];
        blockdim_size[i] = blk_size * cnt_strides[i];
        num_blks *= count[i];
    }

    if (str_alg != NULL) {
        if (strcasecmp(stride_algorithms[1], str_alg) == 0) {
            if (stride_levels >= 2) {
                if (count[1] > count[2]) {
                    base_dimension = 1;
                    buffer_dimension = 2;
                } else {
                    base_dimension = 2;
                    buffer_dimension = 1;
                }

                int shmem_sst = src_strides[base_dimension - 1]/count[0];
                int shmem_tst = dest_strides[base_dimension - 1]/count[0];
                int shmem_nelems = count[base_dimension];
                int base_blk_size = count[1] * count[2];
                void *s_ptr, *d_ptr;

                for (i = 1; i <= num_blks; i++) {
                    if (i % base_blk_size == 1) {
                        s_ptr = src_ptr;
                        d_ptr = dest_ptr;
                        for (j = 0; j < count[buffer_dimension]; j++) {
                            if (count[0] == 4) {
                                shmem_iget32(d_ptr, s_ptr, shmem_tst, shmem_sst,
                                        shmem_nelems, proc);
                            } else if (count[1] == 8) {
                                shmem_iget64(d_ptr, s_ptr, shmem_tst, shmem_sst,
                                        shmem_nelems, proc);
                            } else if (count[1] == 16){
                                shmem_iget128(d_ptr, s_ptr, shmem_tst, shmem_sst,
                                        shmem_nelems, proc);
                            }
                            s_ptr += src_strides[buffer_dimension - 1];
                            d_ptr += dest_strides[buffer_dimension - 1];
                        }
                    }

                    for (j = 1; j <= stride_levels; j++) {
                        if (i % cnt_strides[j])
                            break;
                        src_ptr -= (count[j] - 1) * src_strides[j - 1];
                        dest_ptr -= (count[j] - 1) * dest_strides[j - 1];
                    }

                    src_ptr += src_strides[j - 1];
                    dest_ptr += dest_strides[j - 1];
                }
            } else {
                if (count[0] == 4) {
                    shmem_iget32(dest_ptr, src_ptr, dest_strides[0]/count[0],
                            src_strides[0]/count[0], count[1], proc);
                } else if (count[1] == 8) {
                    shmem_iget64(dest_ptr, src_ptr, dest_strides[0]/count[0],
                            src_strides[0]/count[0], count[1], proc);
                } else if (count[1] == 16){
                    shmem_iget128(dest_ptr, src_ptr, dest_strides[0]/count[0],
                            src_strides[0]/count[0], count[1], proc);
                } else {
                    remote_strided_get(remote_src, src_strides,
                                       dest, dest_strides,
                                       count, stride_levels,
                                       proc);
                }
            }
        } else {
            remote_strided_get(remote_src, src_strides,
                    dest, dest_strides,
                    count, stride_levels,
                    proc);
        }
    } else {
        remote_strided_get(remote_src, src_strides,
                dest, dest_strides,
                count, stride_levels,
                proc);
    }
}

static void strided_put(void *src, const size_t src_strides[],
                        void *remote_dest, const size_t dest_strides[],
                        const size_t count[], size_t stride_levels,
                        int proc)
{
    int i, j;
    size_t num_blks;
    size_t cnt_strides[stride_levels + 1];
    size_t blockdim_size[stride_levels];
    void *dest_ptr = remote_dest;
    void *src_ptr = src;
    int base_dimension, buffer_dimension;

    /* assuming src_elem_size=dst_elem_size */
    size_t blk_size = count[0];
    num_blks = 1;
    cnt_strides[0] = 1;
    blockdim_size[0] = count[0];
    for (i = 1; i <= stride_levels; i++) {
        cnt_strides[i] = cnt_strides[i - 1] * count[i];
        blockdim_size[i] = blk_size * cnt_strides[i];
        num_blks *= count[i];
    }

    if (str_alg != NULL) {
        if (strcasecmp(stride_algorithms[1], str_alg) == 0) {
            if (stride_levels >= 2) {
                if (count[1] > count[2]) {
                    base_dimension = 1;
                    buffer_dimension = 2;
                } else {
                    base_dimension = 2;
                    buffer_dimension = 1;
                }

                int shmem_sst = src_strides[base_dimension - 1]/count[0];
                int shmem_tst = dest_strides[base_dimension - 1]/count[0];
                int shmem_nelems = count[base_dimension];
                int base_blk_size = count[1] * count[2];
                void *s_ptr, *d_ptr;

                for (i = 1; i <= num_blks; i++) {
                    if (i % base_blk_size == 1) {
                        s_ptr = src_ptr;
                        d_ptr = dest_ptr;
                        for (j = 0; j < count[buffer_dimension]; j++) {
                            if (count[0] == 4) {
                                shmem_iput32(d_ptr, s_ptr, shmem_tst, shmem_sst,
                                        shmem_nelems, proc);
                            } else if (count[1] == 8) {
                                shmem_iput64(d_ptr, s_ptr, shmem_tst, shmem_sst,
                                        shmem_nelems, proc);
                            } else if (count[1] == 16){
                                shmem_iput128(d_ptr, s_ptr, shmem_tst, shmem_sst,
                                        shmem_nelems, proc);
                            }
                            s_ptr += src_strides[buffer_dimension - 1];
                            d_ptr += dest_strides[buffer_dimension - 1];
                        }
                    }

                    for (j = 1; j <= stride_levels; j++) {
                        if (i % cnt_strides[j])
                            break;
                        src_ptr -= (count[j] - 1) * src_strides[j - 1];
                        dest_ptr -= (count[j] - 1) * dest_strides[j - 1];
                    }

                    src_ptr += src_strides[j - 1];
                    dest_ptr += dest_strides[j - 1];
                }
            } else {
                if (count[0] == 4) {
                    shmem_iput32(dest_ptr, src_ptr, dest_strides[0]/count[0],
                            src_strides[0]/count[0], count[1], proc);
                } else if (count[1] == 8) {
                    shmem_iput64(dest_ptr, src_ptr, dest_strides[0]/count[0],
                            src_strides[0]/count[0], count[1], proc);
                } else if (count[1] == 16){
                    shmem_iput128(dest_ptr, src_ptr, dest_strides[0]/count[0],
                            src_strides[0]/count[0], count[1], proc);
                } else {
                    remote_strided_put(src, src_strides,
                            remote_dest, dest_strides,
                            count, stride_levels,
                            proc);
                }
            }
        } else {
            remote_strided_put(src, src_strides,
                    remote_dest, dest_strides,
                    count, stride_levels,
                    proc);
        }
    } else {
        remote_strided_put(src, src_strides,
                remote_dest, dest_strides,
                count, stride_levels,
                proc);
    }

}



/*****************************************************************
 *                      INITIALIZATION
 *****************************************************************/

/*
 * comm_init:
 * 1) Initialize SHMEM
 * 2) Create flags and mutexes for sync_images
 * 3) Create pinned memory and populates coarray_start_all_images.
 * 4) Populates init_common_slot and child_common_slot with the pinned memory
 *    data which is used by alloc.c to allocate/deallocate coarrays.
 */

void comm_init()
{
    int ret, i;
    int argc;
    char **argv;
    unsigned long caf_shared_memory_size;
    unsigned long image_heap_size;
    size_t collectives_offset;
    unsigned long  teams_heap_size;

    extern mem_usage_info_t * mem_info;
    extern mem_usage_info_t * teams_mem_info;

    size_t static_align;


    alloc_byte_alignment = get_env_size(ENV_ALLOC_BYTE_ALIGNMENT,
                                   DEFAULT_ALLOC_BYTE_ALIGNMENT);

    /* static coarrays must be 16-byte aligned */
    static_align = ((alloc_byte_alignment-1)/16+1)*16;

    /* get size for collectives buffer */
    collectives_bufsize = get_env_size_with_unit(ENV_COLLECTIVES_BUFSIZE,
                                                 DEFAULT_COLLECTIVES_BUFSIZE);

    /* Get total shared memory size, per image, requested by program (enough
     * space for save coarrays + heap).
     */

    static_symm_data_total_size = get_static_symm_size(static_align,
                                                       collectives_bufsize);

    collectives_offset = static_symm_data_total_size -
        ((collectives_bufsize-1)/static_align+1)*static_align;

    if (static_symm_data_total_size % static_align) {
        static_symm_data_total_size =
            ((static_symm_data_total_size-1)/static_align+1)*static_align;
    }

    caf_shared_memory_size = static_symm_data_total_size;
    image_heap_size = get_env_size_with_unit(ENV_IMAGE_HEAP_SIZE,
                                             DEFAULT_IMAGE_HEAP_SIZE);
    teams_heap_size = get_env_size_with_unit(ENV_TEAMS_HEAP_SIZE,
                                            DEFAULT_TEAMS_HEAP_SIZE);
    caf_shared_memory_size += image_heap_size;

    /*
    argc = ARGC;
    argv = ARGV;
    MPI_Init(&argc, &argv);
    __f90_set_args(argc, argv);
    */

    /* selection strided algorithm from env variable */
    str_alg = getenv("UHCAF_STRIDE_ALGORITHM");

    start_pes(0);

    mpi_initialized_by_shmem = 0;

    my_proc = _my_pe();
    num_procs = _num_pes();

    /* compute log2_images and rem_images */
    int log2_procs = 0;
    long n = num_procs;
    long m = 1;
    while (n > 0) {
        static int first = 1;
        if (first) {
            first = 0;
        } else {
            log2_procs++;
            m <<= 1;
        }
        n >>= 1;
    }
    long rem_procs = num_procs - m;

    /* set extern symbols used for THIS_IMAGE and NUM_IMAGES intrinsics */
    _this_image = my_proc + 1;
    _num_images = num_procs;
    _log2_images = log2_procs;
    _rem_images = rem_procs;

    LIBCAF_TRACE_INIT();
    LIBCAF_TRACE(LIBCAF_LOG_INIT, "after start_pes");

    if (_num_images >= MAX_NUM_IMAGES) {
        if (my_proc == 0) {
            Error("Number of images may not exceed %lu", MAX_NUM_IMAGES);
        }
    }

    /* which reduction algorithm to use */
    char *alg;
    alg = getenv(ENV_CO_REDUCE_ALGORITHM);
    co_reduce_algorithm = CO_REDUCE_DEFAULT;

    if (alg != NULL) {
        if (strncasecmp(alg, "all2all", 7) == 0) {
            co_reduce_algorithm = CO_REDUCE_ALL2ALL;
        } else if (strncasecmp(alg, "2tree_syncall", 13) == 0) {
            co_reduce_algorithm = CO_REDUCE_2TREE_SYNCALL;
        } else if (strncasecmp(alg, "2tree_syncimages", 16) == 0) {
            co_reduce_algorithm = CO_REDUCE_2TREE_SYNCIMAGES;
        } else if (strncasecmp(alg, "2tree_events", 12) == 0) {
            co_reduce_algorithm = CO_REDUCE_2TREE_EVENTS;
        } else {
            if (my_proc == 0) {
                Warning("CO_REDUCE_ALGORITHM %s is not supported. "
                        "Using default", alg);
            }
        }
    }

    /* which sync images to use */
    char *si_alg;
    si_alg = getenv(ENV_SYNC_IMAGES_ALGORITHM);
    sync_images_algorithm = SYNC_SENSE_REV;

    if (si_alg != NULL) {
        if (strncasecmp(si_alg, "counter", 7) == 0) {
            sync_images_algorithm = SYNC_COUNTER;
        } else if (strncasecmp(si_alg, "ping_pong", 9) == 0) {
            sync_images_algorithm = SYNC_PING_PONG;
        } else if (strncasecmp(si_alg, "sense_reversing", 15 ) == 0) {
            sync_images_algorithm = SYNC_SENSE_REV;
        } else if (strncasecmp(si_alg, "default", 7 ) == 0) {
            sync_images_algorithm = SYNC_SENSE_REV;
        } else {
            if (my_proc == 0) {
                Warning("SYNC_IMAGES_ALGORITHM %s is not supported. "
                        "Using default", si_alg);
            }
        }
    }

    /* which team barrier algorithm is used */
    char *team_barrier_alg;
    team_barrier_alg = getenv(ENV_TEAM_BARRIER_ALGORITHM);
    team_barrier_algorithm = BAR_DISSEM;

    if(team_barrier_alg != NULL){
        if (strncasecmp(team_barrier_alg, "dissemination", 13) == 0) {
            team_barrier_algorithm = BAR_DISSEM;
        } else {
            if(my_proc == 0) {
                Warning("%s=%s is not supported."
                        " Using default", ENV_TEAM_BARRIER_ALGORITHM,
                        team_barrier_alg);
            }
        }
    }

    /* which rma ordering strategy to use */
    char *ordering_val;
    ordering_val = getenv(ENV_RMA_ORDERING);
    rma_ordering = RMA_ORDERING_DEFAULT;

    if (ordering_val != NULL) {
        if (strncasecmp(ordering_val, "blocking", 8) == 0) {
            rma_ordering = RMA_BLOCKING;
        } else if (strncasecmp(ordering_val, "put_ordered", 11) == 0) {
            rma_ordering = RMA_PUT_ORDERED;
        } else if (strncasecmp(ordering_val, "put_image_ordered", 17) == 0) {
            rma_ordering = RMA_PUT_IMAGE_ORDERED;
        } else if (strncasecmp(ordering_val, "put_address_ordered", 19) == 0) {
            rma_ordering = RMA_PUT_ADDRESS_ORDERED;
        } else if (strncasecmp(ordering_val, "relaxed", 11) == 0) {
            rma_ordering = RMA_RELAXED;
        } else if (strncasecmp(ordering_val, "default", 7) == 0) {
            rma_ordering = RMA_ORDERING_DEFAULT;
        } else {
            if (my_proc == 0) {
                Warning("rma_ordering %s is not supported. "
                        "Using default", ordering_val);
            }
        }
    }


    /* Check if optimizations are enabled */
    enable_get_cache = get_env_flag(ENV_GETCACHE, DEFAULT_ENABLE_GETCACHE);
    getcache_block_size = get_env_size(ENV_GETCACHE_BLOCK_SIZE,
                                      DEFAULT_GETCACHE_BLOCK_SIZE);
    get_cache_sync_refetch = get_env_flag(ENV_GETCACHE_SYNC_REFETCH,
                                      DEFAULT_ENABLE_GETCACHE_SYNC_REFETCH);

    nb_xfer_limit = get_env_size(ENV_NB_XFER_LIMIT, DEFAULT_NB_XFER_LIMIT);

    if (nb_xfer_limit < 1) {
        if (my_proc == 0) {
            Error("%s needs to be set to at least 1", ENV_NB_XFER_LIMIT);
        }
    }

    if (caf_shared_memory_size / 1024 >= MAX_SHARED_MEMORY_SIZE / 1024) {
        if (my_proc == 0) {
            Error("Image shared memory size must not exceed %lu GB",
                  MAX_SHARED_MEMORY_SIZE / (1024 * 1024 * 1024));
        }
    }

    /* Create pinned/registered memory (Shared Memory) */
    coarray_start_all_images = shmalloc(num_procs * sizeof(void *));
    //start_shared_mem_address = malloc(num_procs * sizeof(void *));

    {
        static void *start_addr;
        int i;
        long *psync_collect = shmalloc(_SHMEM_COLLECT_SYNC_SIZE *
                sizeof *psync_collect);

        for (i = 0; i < _SHMEM_COLLECT_SYNC_SIZE; i++) {
            psync_collect[i] = _SHMEM_SYNC_VALUE;
        }

        shmem_barrier_all();

        start_addr = shmemalign(static_align, caf_shared_memory_size);
        *((void **)start_addr) = start_addr;

        shmem_fcollect64(coarray_start_all_images, start_addr, 1, 0, 0, num_procs,
                psync_collect);

        shfree(psync_collect);
    }

    /* adjust teams_heap_size */
    if ( (teams_heap_size + MIN_HEAP_SIZE + static_symm_data_total_size) >
          caf_shared_memory_size) {
        /* let the first process issue a warning to the user */
        teams_heap_size = caf_shared_memory_size -
                (MIN_HEAP_SIZE + static_symm_data_total_size);
        if (teams_heap_size < 0) teams_heap_size = 0;

        if (my_proc == 0) {
            Warning("teams_heap_size is too big. Reducing to %lu bytes.",
                    teams_heap_size);
        }
    }


    shmem_barrier_all();

    allocate_static_symm_data((char *) coarray_start_all_images[my_proc]);

    /* set collectives buffer */
    collectives_buffer[0] = coarray_start_all_images[my_proc] +
                            collectives_offset;
    collectives_buffer[1] = (char *)collectives_buffer[0] +
                             collectives_bufsize/2;

    memset(collectives_buffer[0], 0, collectives_bufsize);

    /* initialize the child common shared memory slot */
    if (child_common_slot != NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "child_common_slot has already been initialized");
    }

    child_common_slot = malloc(sizeof(shared_memory_slot_t));
    child_common_slot->addr = coarray_start_all_images[my_proc]
        + static_symm_data_total_size;
    child_common_slot->size = teams_heap_size;
    child_common_slot->feb = 0;
    child_common_slot->next = 0;
    child_common_slot->prev = 0;
    child_common_slot->next_empty = 0;
    child_common_slot->prev_empty = 0;

    shared_memory_size = caf_shared_memory_size;

    /*Init the team stack*/
    global_team_stack = (team_stack_t *) malloc(sizeof(team_stack_t));
    global_team_stack->count = 0;


     /*Add team support here*/

    initial_team = (team_type) malloc(sizeof(team_type_t));
    initial_team->codimension_mapping = (long *)malloc(num_procs*sizeof(long));
    for (i = 0; i < num_procs; i++) {
        initial_team->codimension_mapping[i] = i;
    }

    /* Add intranode_set array
     *
     * Currently, treating each process as residing on its own node.
     * TODO: get node info from SHMEM
     */

    {
	    initial_team->intranode_set = (long *)malloc(sizeof(long)*2);
	    initial_team->intranode_set[0] = 1;
	    initial_team->intranode_set[1] = my_proc;
    }

    /*Forming the leader set */
    {
	    initial_team->leader_set =
            (long *)malloc(sizeof(long) * num_procs);

        initial_team->leaders_count = num_procs;

	    for (i = 0; i < num_procs; i++) {
		    initial_team->leader_set[i] = i;
	    }

        total_num_supernodes = num_procs;
    }

    initial_team->current_this_image    = my_proc + 1;
    initial_team->current_num_images    = num_procs;
    initial_team->current_log2_images   = log2_procs;
    initial_team->current_rem_images    = rem_procs;
    initial_team->depth                 = 0;
    initial_team->parent                = NULL;
    initial_team->team_id               = -1;
    initial_team->allreduce_bufid       = 0;
    initial_team->reduce_bufid          = 0;
    initial_team->bcast_bufid           = 0;
    initial_team->defined               = 1;
    initial_team->activated             = 1;
    initial_team->barrier.parity        = 0;
    initial_team->barrier.sense         = 0;
    initial_team->barrier.bstep         = NULL;

    initial_team->allocated_list = NULL;

    /*Init the child_common_slot*/
    if (init_common_slot == NULL) {
        init_common_slot       = (shared_memory_slot_t *)
                                   malloc(sizeof(shared_memory_slot_t));
        init_common_slot->addr = child_common_slot->addr + teams_heap_size;
        init_common_slot->size = image_heap_size - teams_heap_size;
        init_common_slot->feb  = 0;
        init_common_slot->next = NULL;
        init_common_slot->prev = NULL;
        init_common_slot->next_empty = NULL;
        init_common_slot->prev_empty = NULL;
     }


    shared_memory_slot_t * tmp_slot;
    tmp_slot = init_common_slot;
    while(tmp_slot->prev != NULL)
        tmp_slot = tmp_slot->prev;
    initial_team->symm_mem_slot.start_addr = tmp_slot->addr;
    initial_team->symm_mem_slot.end_addr = init_common_slot->addr;

    current_team = initial_team;

    /* update the init_mem_info*/
    mem_info = (mem_usage_info_t *)
        coarray_allocatable_allocate_(sizeof(*mem_info), NULL, NULL);

    /* allocate space in symmetric heap for memory usage info */
    mem_info->current_heap_usage = teams_heap_size + sizeof(*mem_info);
    mem_info->max_heap_usage = mem_info->current_heap_usage;
    mem_info->reserved_heap_usage = image_heap_size;
    teams_mem_info = (mem_usage_info_t *)
        coarray_allocatable_allocate_(sizeof(*teams_mem_info), NULL, NULL);
    teams_mem_info->current_heap_usage = 0;
    teams_mem_info->max_heap_usage = 0;
    teams_mem_info->reserved_heap_usage = teams_heap_size;

    /* allocate space for recording image termination */
    error_stopped_image_exists =
        (int *) coarray_allocatable_allocate_(sizeof(int), NULL, NULL);
    this_image_stopped =
        (int *) coarray_allocatable_allocate_(sizeof(int), NULL, NULL);
    *error_stopped_image_exists = 0;
    *this_image_stopped = 0;

    stopped_image_exists = (char *) coarray_allocatable_allocate_(
                      (num_procs+1) * sizeof(char), NULL, NULL);

    memset(stopped_image_exists, 0, (num_procs+1)*sizeof(char));

    /* allocate flags for p2p synchronization via sync images */
    sync_flags = (sync_flag_t *) coarray_allocatable_allocate_(
            num_procs * sizeof(sync_flag_t), NULL, NULL);
    memset(sync_flags, 0, num_procs*sizeof(sync_flag_t));

    /* allocate a bunch of flags for supporting synchronization in ollectives.
     *
     * TODO: needs to be done more efficiently. A single allocatable, and
     * internode_syncflags should probably be proportional to
     * log2(leaders_count) rather than leaders_count.
     * */
    {
        size_t flags_offset = 0;

        int log2_leaders = 0;
        {
            int p = 1;
            while ( (2*p) <= initial_team->leaders_count) {
                p = p*2;
                log2_leaders += 1;
            }
            if (p < initial_team->leaders_count) {
                log2_leaders += 1;
            }
        }

        initial_team->intranode_barflags = malloc(initial_team->intranode_set[0] *
                                        sizeof(*initial_team->intranode_barflags));

        initial_team->coll_syncflags =
            coarray_allocatable_allocate_(
                sizeof(*initial_team->intranode_barflags[0]) +
                sizeof(*initial_team->bcast_flag) +
                sizeof(*initial_team->allreduce_flag) +
                2 * sizeof(*initial_team->reduce_go) +
                2 * log2_procs * sizeof(*initial_team->reduce_go) +
                2 * log2_procs * sizeof(*initial_team->allreduce_sync),
                NULL, NULL);

        initial_team->intranode_barflags[0] = &initial_team->coll_syncflags[flags_offset];
        flags_offset += sizeof(*initial_team->intranode_barflags[0]);

        initial_team->bcast_flag = &initial_team->coll_syncflags[flags_offset];
        flags_offset += sizeof(*initial_team->bcast_flag);

        initial_team->reduce_flag = &initial_team->coll_syncflags[flags_offset];
        flags_offset += sizeof(*initial_team->reduce_flag);

        initial_team->allreduce_flag = &initial_team->coll_syncflags[flags_offset];
        flags_offset += sizeof(*initial_team->allreduce_flag);

        initial_team->reduce_go = &initial_team->coll_syncflags[flags_offset];
        flags_offset += 2 * sizeof(*initial_team->reduce_go);
        initial_team->reduce_go[0] = 1;
        initial_team->reduce_go[1] = 1;

        initial_team->bcast_go = &initial_team->coll_syncflags[flags_offset];
        flags_offset += 2 * log2_procs * sizeof(*initial_team->bcast_go);
        for (i = 0; i < 2*log2_procs; i++) {
            initial_team->bcast_go[i] = 1;
        }

        initial_team->allreduce_sync = &initial_team->coll_syncflags[flags_offset];

    }

    /* Initializing remainder of intranode barrier flags */
    {
        int num_nonleaders = initial_team->intranode_set[0] - 1;
        memset(&initial_team->intranode_barflags[1], 0,
                num_nonleaders * sizeof(*initial_team->intranode_barflags));

        long leader = initial_team->intranode_set[1];
        int intranode_count = initial_team->intranode_set[0];
        /* set intranode flags for synchronization */
        if (my_proc == leader) {
          int i;
          for (i = 1; i < intranode_count; i++) {
            int target = initial_team->intranode_set[i + 1];
            initial_team->intranode_barflags[i] =
              comm_get_sharedptr(initial_team->intranode_barflags[0],
                  target);
          }
        } else {
          int leader = initial_team->intranode_set[1];
          initial_team->intranode_barflags[1] =
            comm_get_sharedptr(initial_team->intranode_barflags[0],
                leader);
        }
    }


    /* check whether to use 1-sided collectives implementation */
    enable_collectives_mpi = get_env_flag(ENV_COLLECTIVES_MPI,
                                    DEFAULT_ENABLE_COLLECTIVES_MPI);

    /* check whether to enable use of canary protocol for some collectives */
    enable_collectives_use_canary = get_env_flag(ENV_COLLECTIVES_USE_CANARY,
                                    DEFAULT_ENABLE_COLLECTIVES_USE_CANARY);

    /* determine if there is a maximum number of work buffers to use for
     * collectives */
    collectives_max_workbufs = get_env_flag(ENV_COLLECTIVES_MAX_WORKBUFS,
                                            DEFAULT_COLLECTIVES_MAX_WORKBUFS);

    enable_collectives_2level = get_env_flag(ENV_COLLECTIVES_2LEVEL,
            DEFAULT_ENABLE_COLLECTIVES_2LEVEL);

    enable_reduction_2level = get_env_flag(ENV_REDUCTION_2LEVEL,
            DEFAULT_ENABLE_REDUCTION_2LEVEL);

    enable_broadcast_2level = get_env_flag(ENV_BROADCAST_2LEVEL,
            DEFAULT_ENABLE_BROADCAST_2LEVEL);

    mpi_collectives_available = 1;


    /*allocate exhange buffer for form_team here*/
    exchange_teaminfo_buf = (team_info_t *)
        coarray_allocatable_allocate_(sizeof(team_info_t)* num_procs, NULL, NULL);

    /* partition collectives buffer into all-reduce, reduce, and broadcast siloes */
    {
        int q = 1;
        int log2_q = 0;
        while ( (2*q) <=  initial_team->leaders_count) {
            q = 2 * q;
            log2_q = log2_q + 1;
        }
        if (q < initial_team->leaders_count)
            log2_q = log2_q + 1;

        allreduce_bufsize = (collectives_bufsize/4)*(1+log2_q)/(2+log2_q);
        reduce_bufsize    = (collectives_bufsize/4)*(1+log2_q)/(2+log2_q);
        broadcast_bufsize = (collectives_bufsize/2) -
                            (allreduce_bufsize + reduce_bufsize);

        allreduce_buffer[0] = collectives_buffer[0];
        reduce_buffer[0]    = (char*)allreduce_buffer[0] + allreduce_bufsize;
        broadcast_buffer[0] = (char*)reduce_buffer[0] + reduce_bufsize;

        allreduce_buffer[1] = collectives_buffer[1];
        reduce_buffer[1]    = (char*)allreduce_buffer[1] + allreduce_bufsize;
        broadcast_buffer[1] = (char*)reduce_buffer[1] + reduce_bufsize;
    }

    /*Push the first team into stack*/
    global_team_stack->stack[global_team_stack->count] = initial_team;
    global_team_stack->count += 1;

    /* critical section lock */
    critical_naive_lock = (long *) shmalloc(sizeof *critical_naive_lock);
    *critical_naive_lock = 0;

    LIBCAF_TRACE(LIBCAF_LOG_INIT, "Finished. Waiting for global barrier."
                 "common_slot->addr=%p, common_slot->size=%lu",
                 init_common_slot->addr, init_common_slot->size);

    shmem_barrier_all();

    LIBCAF_TRACE(LIBCAF_LOG_INIT, "exit");
}



/*****************************************************************
 *                  Shared Memory Management
 ****************************************************************/

/* It should allocate memory to all static coarrays/targets from the
 * pinned-down memory created during init */
void set_static_symm_data_(void *base_address, size_t alignment)
{
    /* do nothing */
}

#pragma weak set_static_symm_data = set_static_symm_data_

unsigned long get_static_symm_size_(size_t alignment,
                                    size_t collectives_bufsize)
{
    return ((collectives_bufsize-1)/alignment+1)*alignment;
}

#pragma weak get_static_symm_size = get_static_symm_size_

void allocate_static_symm_data(void *base_address)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    size_t align = ((alloc_byte_alignment-1)/16+1)*16;
    set_static_symm_data(base_address, align);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

inline ssize_t comm_address_translation_offset(size_t proc)
{
    char *remote_base_address = coarray_start_all_images[proc];
    return remote_base_address -
        (char *) coarray_start_all_images[my_proc];
}


void comm_translate_remote_addr(void **remote_addr, int proc)
{
    ssize_t offset;
    void *local_address;

    /* TODO: Check this ... is start_shared_mem_address[proc] ever set? */

    //offset = start_shared_mem_address[my_proc] - start_shared_mem_address[proc];
    offset = coarray_start_all_images[my_proc] - coarray_start_all_images[proc];
    local_address = *remote_addr + offset;

#if 0
    if (!address_in_symmetric_mem(local_address)) {
        local_address = (char *) local_address +
            comm_address_translation_offset(proc);
    }
#endif
    *remote_addr = local_address;
}



/* Calculate the address on another image corresponding to a local address
 * This is possible as all images must have the same coarrays, i.e the
 * memory is symmetric. Since we know the start address of all images
 * from coarray_start_all_images, remote_address = start+offset
 * NOTE: remote_address on this image might be different from the actual
 * address on that image. So don't rely on it while debugging*/
void *get_remote_address(void *src, size_t img)
{
    size_t offset;
    void *remote_address;

    return src;

    /* does not reach */

#if 0
    if ((img == my_proc) || !address_in_symmetric_mem(src))
        return src;
    offset = src - coarray_start_all_images[my_proc];
    remote_address = coarray_start_all_images[img] + offset;
    return remote_address;
#endif
}

/****************************************************************
 *                         FINALIZATION
 ****************************************************************/

void comm_memory_free()
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    if (coarray_start_all_images) {
        coarray_free_all_shared_memory_slots(); /* in caf_rtl.c */
        shfree(coarray_start_all_images);
    }

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

void comm_exit(int status)
{
    int p;

    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "entry");

    /*
     * ERROR TERMINATION
     *
     * Notify other images that this image has started error termination
     */

    in_error_termination = 1;

    if (status == 0)
        Warning("Image %d is exiting without a set error code", my_proc+1);

    *this_image_stopped = 2; /* 2 means error stopped */

    if (*error_stopped_image_exists == 0 && status != 0) {
        *error_stopped_image_exists = status;

        /* broadcast to every image that this image has stopped.
         * TODO: Other images should be able to detect this image has stopped
         * when they try to synchronize with it. Requires custom
         * implementation of barriers.
         */
        for (p = 0; p < num_procs; p++) {
            if (p == my_proc) continue;
            comm_write(p, error_stopped_image_exists,
                       error_stopped_image_exists,
                       sizeof(*error_stopped_image_exists),
                       1, NULL);
        }
    }

    LOAD_STORE_FENCE();
    comm_memory_free();


    /* TODO: can SHMEM provide a better way to exit than this? */
    exit(1);

    /* does not reach */
}

static inline void check_for_error_stop()
{
    if (!in_error_termination && error_stopped_image_exists != NULL &&
        *error_stopped_image_exists != 0)
        comm_exit(*error_stopped_image_exists);
}

void comm_finalize(int exit_code)
{
    int p;

    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "entry");

    /*
     * NORMAL TERMINATION STEPS:
     * (1) Initiation
     * (2) Synchronization
     * (3) Completion
     */

    /* Initiation */

    in_normal_termination = 1;

    int stopped_image_already_exists = stopped_image_exists[num_procs];

    *this_image_stopped = 1; /* 1 means normal stopped */
    stopped_image_exists[my_proc] = 1;
    stopped_image_exists[num_procs] = 1;

    /* wait on all pendings remote accesses to complete */
    shmem_quiet();

    /* broadcast to every image that this image has stopped.
     * TODO: Other images should be able to detect this image has stopped when
     * they try to synchronize with it. Requires custom implementation of
     * barriers.
     */
    for (p = 0; p < num_procs; p++) {
        if (p == my_proc) continue;

        if (!stopped_image_already_exists) {
            /* this indicates at least one image has stopped, allowing for
             * quicker check for barrier synchronization */
            char stopped = 1;
            comm_write(p, &stopped_image_exists[num_procs],
                       &stopped, sizeof(char), 1, NULL);
        }

        comm_write(p, &stopped_image_exists[my_proc],
                   &stopped_image_exists[my_proc], sizeof(char), 1, NULL);
    }

    /* Synchronization */

    comm_barrier_all();

    /* Completion */

    comm_memory_free();

    /*
    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "Before call to MPI_Finalize");
    MPI_Finalize();
    */

    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "exit");
    exit(exit_code);

    /* does not reach */
}

/***************************************************************
 *                      SYNCHRONIZATION
 ***************************************************************/


/* TODO: Right now, for every critical section we simply acquire a mutex on
 * process 0. Instead, we should define a mutex for each critical section in
 * order to allow execution of different critical sections at the same time.
 */

void comm_critical()
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    check_for_error_stop();

    shmem_set_lock(critical_naive_lock);
    comm_new_exec_segment();

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void comm_end_critical()
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    check_for_error_stop();

    shmem_quiet();

    shmem_clear_lock(critical_naive_lock);

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

/* wait for any pending communication to specified proc to complete. */
void comm_fence(size_t proc)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    check_for_error_stop();

    /* TODO: see if we can refine this to something more relaxed; for example
     * consider using a shmem_fence instead of shmem_quiet */
    shmem_quiet();

    LOAD_STORE_FENCE();

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void comm_fence_all()
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    check_for_error_stop();

    shmem_quiet();

    LOAD_STORE_FENCE();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

/* start of a new execution segment. All we do here is ensure any cached
 * copies of remote data is either thrown away or refreshed */
void comm_new_exec_segment()
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    /* TODO: anything else to do here with SHMEM? */

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}


void comm_barrier_all()
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    check_for_error_stop();

    shmem_barrier_all();

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void comm_sync(comm_handle_t hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    check_for_error_stop();

    if (hdl == (comm_handle_t) - 1) {
        shmem_quiet();
    } else if (hdl != NULL) {   /* wait on specified handle */

        /* TODO: when non-blocking interface for SHMEM is available;
         * for now, just shmem_quiet (could perhaps be shmem_fence instead )
         * */

        shmem_quiet();
    }

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void comm_sync_all(int *status, int stat_len, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    check_for_error_stop();

    if (status != NULL) {
        memset(status, 0, (size_t) stat_len);
        *((INT2 *) status) = STAT_SUCCESS;
    }
    if (errmsg != NULL && errmsg_len) {
        memset(errmsg, 0, (size_t) errmsg_len);
    }

    shmem_quiet();

    LOAD_STORE_FENCE();

	if(current_team == NULL || current_team == initial_team ||
       current_team->codimension_mapping == NULL) {
	    if (stopped_image_exists != NULL && stopped_image_exists[num_procs]) {
            if (status != NULL) {
                *((INT2 *) status) = STAT_STOPPED_IMAGE;
            } else {
                Error("Image %d attempted to synchronize with stopped image",
                      _this_image);
                /* doesn't reach */
            }
        } else {
            shmem_barrier_all();
        }
	} else {
		//determine which algorithm we are going to use
		switch(team_barrier_algorithm) {
            case BAR_DISSEM:
				sync_all_dissemination_mcs(status, stat_len, errmsg, errmsg_len,
                                           current_team);
				break;
			default:
				sync_all_dissemination_mcs(status, stat_len, errmsg, errmsg_len,
                                           current_team);
		}

	}

    comm_new_exec_segment();

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

static void sync_all_dissemination_mcs(int *status, int stat_len,
                                       char *errmsg, int errmsg_len,
                                       team_type_t *team)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG, "Using dissemination algorithm");
    /*Phase 1, init*/
    long nums = team->current_num_images;
    long rank = team->current_this_image - 1;
    char bar_parity;
    char bar_sense;
    barrier_round_t *bstep;
    int i;

    bar_parity = team->barrier.parity;
    bar_sense = 1 - team->barrier.sense;

    /*Phase 2*/
    long round = 0;
    int sendpeer_idx, recvpeer_idx;
    long round_bound = ceil(log2((double)nums));

    for(round = 0;round < round_bound; round++){
        bstep = &team->barrier.bstep[round];
        unsigned long send_dest;
        unsigned long recv_peer;

        send_dest = bstep->target;
        recv_peer = bstep->source;

        shmem_putmem(&bstep->remote[bar_parity], &bar_sense,
                     sizeof bstep->remote[bar_parity],
                     send_dest);

        while (bstep->local[bar_parity] != bar_sense &&
               !(*error_stopped_image_exists)) {
            comm_service();
            LOAD_STORE_FENCE();

            if (stopped_image_exists[recv_peer] &&
                bstep->local[bar_parity] != bar_sense) {
                if (status != NULL) {
                    *((INT2 *) status) = STAT_STOPPED_IMAGE;
                    LOAD_STORE_FENCE();
                    return;
                } else {
                    /* error termination */
                    Error("Image %d attempted to synchronize with "
                          "stopped image %d.", my_proc+1, recv_peer+1);
                    /* doesn't reach */
                }
            }
        }

        check_for_error_stop();

        if (stopped_image_exists[recv_peer] &&
            bstep->local[bar_parity] != bar_sense) {
            /* q has stopped */
            if (status != NULL) {
                *((INT2 *) status) = STAT_STOPPED_IMAGE;
                LOAD_STORE_FENCE();
                return;
            } else {
                /* error termination */
                Error("Image %d attempted to synchronize with stopped "
                      "image %d.", my_proc+1, recv_peer+1);
                /* doesn't reach */
            }
        }

        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                     "value %d from peer %d received in round %d\n",
                     bar_sense, recv_peer, round);

        LIBCAF_TRACE(LIBCAF_LOG_DEBUG, "ending round %d\n", round);
    }

    team->barrier.parity = 1 - bar_parity;
    if (bar_parity == 1) team->barrier.sense = bar_sense;

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void comm_sync_team(team_type_t *team, int *status, int stat_len, char *errmsg,
                    int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    check_for_error_stop();

    if (status != NULL) {
        memset(status, 0, (size_t) stat_len);
        *((INT2 *) status) = STAT_SUCCESS;
    }
    if (errmsg != NULL && errmsg_len) {
        memset(errmsg, 0, (size_t) errmsg_len);
    }

    shmem_quiet();

    LOAD_STORE_FENCE();

    /* TODO: need to check for stopped image during the execution of the
     * barrier, instead of just at the beginning */
    if (stopped_image_exists != NULL && stopped_image_exists[num_procs]) {
        if (status != NULL) {
            *((INT2 *) status) = STAT_STOPPED_IMAGE;
        } else {
            Error("Image %d attempted to synchronize with stopped image",
                  _this_image);
            /* doesn't reach */
        }
    } else {
        shmem_barrier_all();
    }

    /* determine which algorithm we are going to use */
    switch(team_barrier_algorithm) {
        case BAR_DISSEM:
            sync_all_dissemination_mcs(status, stat_len, errmsg, errmsg_len, team);
            break;
        default:
            sync_all_dissemination_mcs(status, stat_len, errmsg, errmsg_len, team);
    }

    comm_new_exec_segment();

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void comm_sync_memory(int *status, int stat_len, char *errmsg,
                      int errmsg_len)
{
    unsigned long i;

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    check_for_error_stop();

    if (status != NULL) {
        memset(status, 0, (size_t) stat_len);
        *((INT2 *) status) = STAT_SUCCESS;
    }
    if (errmsg != NULL && errmsg_len) {
        memset(errmsg, 0, (size_t) errmsg_len);
    }

    shmem_quiet();

    comm_new_exec_segment();

    LOAD_STORE_FENCE();

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

#ifdef SYNC_IMAGES_HASHED
void comm_sync_images(hashed_image_list_t *image_list, int image_count,
                      int *status, int stat_len, char *errmsg, int errmsg_len)
#else
void comm_sync_images(int *image_list, int image_count,
                      int *status, int stat_len, char *errmsg, int errmsg_len)
#endif
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");

    check_for_error_stop();

    if (status != NULL) {
        memset(status, 0, (size_t) stat_len);
        *((INT2 *) status) = STAT_SUCCESS;
    }
    if (errmsg != NULL && errmsg_len) {
        memset(errmsg, 0, (size_t) errmsg_len);
    }

    shmem_quiet();

    switch (sync_images_algorithm) {
        case SYNC_COUNTER:
            sync_images_counter(image_list, image_count, status,
                                stat_len, errmsg, errmsg_len);
            break;
        case SYNC_PING_PONG:
            sync_images_ping_pong(image_list, image_count, status,
                                  stat_len, errmsg, errmsg_len);
            break;
        case SYNC_SENSE_REV:
            sync_images_sense_rev(image_list, image_count, status,
                                  stat_len, errmsg, errmsg_len);
            break;
        default:
            sync_images_sense_rev(image_list, image_count, status,
                                  stat_len, errmsg, errmsg_len);
            break;
    }

    comm_new_exec_segment();

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

#ifdef SYNC_IMAGES_HASHED
static void sync_images_counter(hashed_image_list_t *image_list,
                                int image_count, int *status,
                                int stat_len, char *errmsg,
                                int errmsg_len)
#else
static void sync_images_counter(int *image_list,
                                int image_count, int *status,
                                int stat_len, char *errmsg,
                                int errmsg_len)
#endif
{
    int i;
#ifdef SYNC_IMAGES_HASHED
    hashed_image_list_t *list_item;
    for (list_item = image_list, i = 0; i < image_count; i++) {
        int q;
        /* if image_list is NULL, we sync with all images */
        if (list_item != NULL) {
            q = list_item->image_id - 1;
            list_item = list_item->hh.next;
        } else {
            q = i;
        }
#else
    for (i = 0; i < image_count; i++) {
        int q;
        /* if image_list is NULL, we sync with all images */
        if (image_list != NULL) {
            q = image_list[i] - 1;
        } else {
            q = i;
        }
#endif

        if (my_proc != q) {
            int inc = 1;
            /* increment counter */
            comm_add_request(&sync_flags[my_proc].value, &inc,
                             sizeof(inc), q);
        }
    }

#ifdef SYNC_IMAGES_HASHED
    for (list_item = image_list, i = 0; i < image_count; i++) {
        int q;
        /* if image_list is NULL, we sync with all images */
        if (list_item != NULL) {
            q = list_item->image_id - 1;
            list_item = list_item->hh.next;
        } else {
            q = i;
        }
#else
    for (i = 0; i < image_count; i++) {
        int q;
        /* if image_list is NULL, we sync with all images */
        if (image_list != NULL) {
            q = image_list[i] - 1;
        } else {
            q = i;
        }
#endif

        if (q == my_proc)
            continue;

        check_for_error_stop();

        while (sync_flags[q].value == 0) {
            comm_service();
            LOAD_STORE_FENCE();

            if (stopped_image_exists[q] && !sync_flags[q].value) {
                /* q has stopped */
                if (status != NULL) {
                    *((INT2 *) status) = STAT_STOPPED_IMAGE;
                    LOAD_STORE_FENCE();
                    return;
                } else {
                    /* error termination */
                    Error("Image %d attempted to synchronize with "
                          "stopped image %d.", _this_image, q+1);
                    /* doesn't reach */
                }
            }
        }

        /* atomically decrement counter */
        SYNC_FETCH_AND_ADD((int *)&sync_flags[q].value, (int)-1);

    }
}

#ifdef SYNC_IMAGES_HASHED
static void sync_images_ping_pong(hashed_image_list_t *image_list,
                                  int image_count, int *status,
                                  int stat_len, char *errmsg,
                                  int errmsg_len)
#else
static void sync_images_ping_pong(int *image_list,
                                  int image_count, int *status,
                                  int stat_len, char *errmsg,
                                  int errmsg_len)
#endif
{
    int i;
    int images_to_check;
    char check_images[image_count];

    images_to_check = image_count;

#ifdef SYNC_IMAGES_HASHED
    hashed_image_list_t *list_item;
    for (list_item = image_list, i = 0; i < image_count; i++) {
        int q;
        /* if image_list is NULL, we sync with all images */
        if (list_item != NULL) {
            q = list_item->image_id - 1;
            list_item = list_item->hh.next;
        } else {
            q = i;
        }
#else
    for (i = 0; i < image_count; i++) {
        int q;
        /* if image_list is NULL, we sync with all images */
        if (image_list != NULL) {
            q = image_list[i] - 1;
        } else {
            q = i;
        }
#endif

        check_images[i] = 1;
        if (my_proc == q) {
            images_to_check--;
            check_images[i] = 0;
        }
        if (my_proc < q) {
            int flag_set = 1;
            comm_write(q, &sync_flags[my_proc].value, &flag_set,
                       sizeof(flag_set), 1, NULL);
        }
    }

    while (images_to_check != 0) {
#ifdef SYNC_IMAGES_HASHED
        for (list_item = image_list, i = 0; i < image_count; i++) {
            int q;
            /* if image_list is NULL, we sync with all images */
            if (list_item != NULL) {
                q = list_item->image_id - 1;
                list_item = list_item->hh.next;
            } else {
                q = i;
            }
#else
        for (i = 0; i < image_count; i++) {
            int q;
            /* if image_list is NULL, we sync with all images */
            if (image_list != NULL) {
                q = image_list[i] - 1;
            } else {
                q = i;
            }
#endif

            if (check_images[i] == 0) continue;

            check_for_error_stop();

            if (stopped_image_exists[q] && !sync_flags[q].value) {
                /* q has stopped */
                if (status != NULL) {
                    *((INT2 *) status) = STAT_STOPPED_IMAGE;
                    LOAD_STORE_FENCE();
                    return;
                } else {
                    /* error termination */
                    Error("Image %d attempted to synchronize with stopped "
                          "image %d.", _this_image, q+1);
                    /* doesn't reach */
                }
            }

            if (sync_flags[q].value == 1) {
                sync_flags[q].value = 0;
                check_images[i] = 0;
                images_to_check--;
                if (q < my_proc) {
                    int flag_set = 1;
                    comm_write(q, &sync_flags[my_proc].value, &flag_set,
                            sizeof(flag_set), 1, NULL);
                }
            }
        }
        comm_service();
    }
}

#ifdef SYNC_IMAGES_HASHED
static void sync_images_sense_rev(hashed_image_list_t *image_list,
                              int image_count, int *status, int stat_len,
                              char *errmsg, int errmsg_len)
#else
static void sync_images_sense_rev(int *image_list,
                              int image_count, int *status, int stat_len,
                              char *errmsg, int errmsg_len)
#endif
{
    int i;

#ifdef SYNC_IMAGES_HASHED
    hashed_image_list_t *list_item;
    for (list_item = image_list, i = 0; i < image_count; i++) {
        int q;
        /* if image_list is NULL, we sync with all images */
        if (list_item != NULL) {
            q = list_item->image_id - 1;
            list_item = list_item->hh.next;
        } else {
            q = i;
        }
#else
    for (i = 0; i < image_count; i++) {
        int q;
        /* if image_list is NULL, we sync with all images */
        if (image_list != NULL) {
            q = image_list[i] - 1;
        } else {
            q = i;
        }
#endif

        if (my_proc != q) {
            char sense = sync_flags[q].t.sense % 2 + 1;
            sync_flags[q].t.sense = sense;
            comm_nbi_write(q, &sync_flags[my_proc].t.val, &sense,
                           sizeof(sense));
        }
    }

#ifdef SYNC_IMAGES_HASHED
    for (list_item = image_list, i = 0; i < image_count; i++) {
        int q;
        /* if image_list is NULL, we sync with all images */
        if (list_item != NULL) {
            q = list_item->image_id - 1;
            list_item = list_item->hh.next;
        } else {
            q = i;
        }
#else
    for (i = 0; i < image_count; i++) {
        int q;
        /* if image_list is NULL, we sync with all images */
        if (image_list != NULL) {
            q = image_list[i] - 1;
        } else {
            q = i;
        }
#endif

        if (my_proc == q)
            continue;

        check_for_error_stop();


        while (!sync_flags[q].t.val) {
            comm_service();
            LOAD_STORE_FENCE();

            if (stopped_image_exists[q] && !sync_flags[q].t.val) {
                /* q has stopped */
                if (status != NULL) {
                    *((INT2 *) status) = STAT_STOPPED_IMAGE;
                    LOAD_STORE_FENCE();
                    return;
                } else {
                    /* error termination */
                    Error("Image %d attempted to synchronize with "
                          "stopped image %d.", _this_image, q+1);
                    /* doesn't reach */
                }
            }
        }

        char x;
        x = (char) SYNC_SWAP((char *)&sync_flags[q].t.val, 0);
        if (x != sync_flags[q].t.sense) {
            /* already received the next notification */
            sync_flags[q].t.val = x;
        }
    }
}

/***************************************************************
 *                        ATOMICS
 ***************************************************************/

void comm_swap_request(void *target, void *value, size_t nbytes,
                       int proc, void *retval)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

    check_remote_address(proc + 1, target);
    if (nbytes == sizeof(int)) {
        int t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_int_swap(remote_address, *((int *)value), proc);
        memmove(retval, &t, nbytes);
    } else if (nbytes == sizeof(long)) {
        long t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_long_swap(remote_address, *((long *)value), proc);
        memmove(retval, &t, nbytes);
    } else if (nbytes == sizeof(long long)) {
        long long t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_longlong_swap(remote_address, *((long long *)value), proc);
        memmove(retval, &t, nbytes);
    } else {
        char msg[100];
        sprintf(msg, "unsupported nbytes (%lu) in comm_swap_request",
                (unsigned long) nbytes);
        Error(msg);
    }
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void
comm_cswap_request(void *target, void *cond, void *value,
                   size_t nbytes, int proc, void *retval)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

    check_remote_address(proc + 1, target);
    if (nbytes == sizeof(int)) {
        int t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_int_cswap(remote_address, *((int *)cond), *((int *)value), proc);
        memmove(retval, &t, nbytes);
    } else if (nbytes == sizeof(long)) {
        long t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_long_cswap(remote_address, *((long *)cond),
                *((long *)value), proc);
        memmove(retval, &t, nbytes);
    } else if (nbytes == sizeof(long long)) {
        long long t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_longlong_cswap(remote_address, *((long long *)cond),
                *((long long *)value), proc);
        memmove(retval, &t, nbytes);
    } else {
        char msg[100];
        sprintf(msg, "unsupported nbytes (%lu) in comm_cswap_request",
                (unsigned long) nbytes);
        Error(msg);
    }
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_fadd_request(void *target, void *value, size_t nbytes, int proc,
                       void *retval)
{
    long long old;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

    check_remote_address(proc + 1, target);
    if (nbytes == sizeof(int)) {
        int ret;
        void *remote_address = get_remote_address(target, proc);
        if (*((int*)value) == 1) {
            ret = shmem_int_finc(target, proc);
        } else {
            ret = shmem_int_fadd(target, *((int *)value), proc);
        }
        memmove(retval, &ret, nbytes);
    } else if (nbytes == sizeof(long)) {
        long ret;
        void *remote_address = get_remote_address(target, proc);
        if (*((long*)value) == 1) {
            ret = shmem_long_finc(target, proc);
        } else {
            ret = shmem_long_fadd(target, *((long *)value), proc);
        }
        memmove(retval, &ret, nbytes);
    } else if (nbytes == sizeof(long long)) {
        long long ret;
        void *remote_address = get_remote_address(target, proc);
        if (*((long long*)value) == 1) {
            ret = shmem_longlong_finc(target, proc);
        } else {
            ret = shmem_longlong_fadd(target, *((long long *)value), proc);
        }
        memmove(retval, &ret, nbytes);
    } else {
        char msg[100];
        sprintf(msg, "unsupported nbytes (%lu) in comm_fadd_request",
                (unsigned long) nbytes);
        Error(msg);
    }
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_add_request(void *target, void *value, size_t nbytes, int proc)
{
    long long old;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

    check_remote_address(proc + 1, target);
    if (nbytes == sizeof(int)) {
        void *remote_address = get_remote_address(target, proc);
        if (*((int*)value) == 1) {
            shmem_int_inc(target, proc);
        } else {
            shmem_int_add(target, *((int *)value), proc);
        }
    } else if (nbytes == sizeof(long)) {
        void *remote_address = get_remote_address(target, proc);
        if (*((long*)value) == 1) {
            shmem_long_inc(target, proc);
        } else {
            shmem_long_add(target, *((long *)value), proc);
        }
    } else if (nbytes == sizeof(long long)) {
        void *remote_address = get_remote_address(target, proc);
        if (*((long long*)value) == 1) {
            shmem_longlong_inc(target, proc);
        } else {
            shmem_longlong_add(target, *((long long *)value), proc);
        }
    } else {
        char msg[100];
        sprintf(msg, "unsupported nbytes (%lu) in comm_add_request",
                (unsigned long) nbytes);
        Error(msg);
    }
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}


void comm_fand_request(void *target, void *value, size_t nbytes, int proc,
                       void *retval)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    /* TODO */
    Error("comm_fand_request not implemented for SHMEM comm layer");

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_and_request(void *target, void *value, size_t nbytes, int proc)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    /* TODO */
    Error("comm_and_request not implemented for SHMEM comm layer");

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}
void comm_for_request(void *target, void *value, size_t nbytes, int proc,
                       void *retval)
{
    long long old;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    /* TODO */
    Error("comm_for_request not implemented for SHMEM comm layer");

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_or_request(void *target, void *value, size_t nbytes, int proc)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    /* TODO */
    Error("comm_or_request not implemented for SHMEM comm layer");

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}
void comm_fxor_request(void *target, void *value, size_t nbytes, int proc,
                       void *retval)
{
    long long old;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    /* TODO */
    Error("comm_fxor_request not implemented for SHMEM comm layer");

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_xor_request(void *target, void *value, size_t nbytes, int proc)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    /* TODO */
    Error("comm_xor_request not implemented for SHMEM comm layer");

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}


void comm_fstore_request(void *target, void *value, size_t nbytes,
                         int proc, void *retval)
{
    long long old;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

    check_remote_address(proc + 1, target);
    if (nbytes == sizeof(int)) {
        int t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_int_swap(remote_address, *((int *)value), proc);
        memmove(retval, &t, nbytes);
    } else if (nbytes == sizeof(long)) {
        long t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_long_swap(remote_address, *((long *)value), proc);
        memmove(retval, &t, nbytes);
    } else if (nbytes == sizeof(long long)) {
        long long t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_longlong_swap(remote_address, *((long long *)value), proc);
        memmove(retval, &t, nbytes);
    } else {
        char msg[100];
        sprintf(msg, "unsupported nbytes (%lu) in comm_fstore_request",
                (unsigned long) nbytes);
        Error(msg);
    }
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_atomic_store_request(void *target, void *value, size_t nbytes,
                               int proc)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

    check_remote_address(proc + 1, target);
    if (nbytes == sizeof(int)) {
        int t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_int_swap(remote_address, *((int *)value), proc);
    } else if (nbytes == sizeof(long)) {
        long t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_long_swap(remote_address, *((long *)value), proc);
    } else if (nbytes == sizeof(long long)) {
        long long t;
        void *remote_address = get_remote_address(target, proc);
        t = shmem_longlong_swap(remote_address, *((long long *)value), proc);
    } else {
        char msg[100];
        sprintf(msg, "unsupported nbytes (%lu) in comm_atomic_store_request",
                (unsigned long) nbytes);
        Error(msg);
    }
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

/***************************************************************
 *                    Local memory allocations
 ***************************************************************/

void *comm_lcb_malloc(size_t size)
{
    void *ptr;

    check_for_error_stop();

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    /* allocate out of asymmetric heap if there is sufficient enough room */
    ptr = coarray_asymmetric_allocate_if_possible_(size);
    if (!ptr) {
        ptr = malloc(size);
    }

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
    return ptr;
}

void comm_lcb_free(void *ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");

    check_for_error_stop();

    if (!ptr) {
        LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
        return;
    }

    if (ptr < coarray_start_all_images[my_proc] ||
        ptr >= (coarray_start_all_images[my_proc] + shared_memory_size))
        free(ptr);
    else {
        /* in shared memory segment, which means it was allocated using
           coarray_asymmetric_allocate_ */
        coarray_asymmetric_deallocate_(ptr);
    }
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

void *comm_malloc(size_t size)  //To make it sync with gasnet
{
    return malloc(size);
}

void comm_free(void *ptr)       //To make it sync with gasnet
{
    if (!ptr)
        return;
    free(ptr);
}

/***************************************************************
 *                  Read/Write Communication
 ***************************************************************/

void comm_service()
{
    /* does nothing much right now */

    /* in case this is call in a spin-loop, sleep every SLEEP_INTERVAL calls
     */
    const unsigned long  SLEEP_INTERVAL = 1000000;
    static unsigned long count = 0;

    check_for_error_stop();

    //if (count % SLEEP_INTERVAL == 0) usleep(1);
    if (count % SLEEP_INTERVAL == 0) shmem_fence();
    count++;
}

void comm_poll_char_while_nonzero(char *c)
{
  char val;

  val = *c;
  while (val != 0) {
    comm_service();
    val = *c;
  }
}

void comm_poll_char_while_zero(char *c)
{
  char val;

  val = *c;
  while (val == 0) {
    comm_service();
    val = *c;
  }
}

void comm_atomic_define(size_t proc, INT4 *atom, INT4 val)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    comm_atomic_store_request(atom, &val, sizeof val, proc);

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_atomic8_define(size_t proc, INT8 *atom, INT8 val)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    comm_atomic_store_request(atom, &val, sizeof val, proc);

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_atomic_ref(INT4 *val, size_t proc, INT4 *atom)
{
    INT8 x;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    x = 0;
    comm_fadd_request(atom, &x, sizeof *val, proc, val);

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_atomic8_ref(INT8 *val, size_t proc, INT8 *atom)
{
    INT8 x;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    x = 0;
    comm_fadd_request(atom, &x, sizeof *val, proc, val);

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_nbread(size_t proc, void *src, void *dest, size_t nbytes,
                 comm_handle_t * hdl)
{
    void *remote_src;

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

#if defined(ENABLE_LOCAL_MEMCPY)
    if (my_proc == proc && nbytes <= SMALL_XFER_SIZE) {
        memcpy(dest, src, nbytes);
        if (hdl != NULL)
            *hdl = NULL;

        LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
        return;
        /* does not reach */
    }
#endif

    remote_src = get_remote_address(src, proc);
    LIBCAF_TRACE(LIBCAF_LOG_COMM,
                 "src: %p, remote_src: %p, offset: %p "
                 "start_all_images[proc]: %p, start_all_images[my_proc]: %p",
                 src, remote_src, comm_address_translation_offset(proc),
                 coarray_start_all_images[proc],
                 coarray_start_all_images[my_proc]);

    /* TODO: see if we can use shmem_fence() or something even more relaxed
     * instead? */
    shmem_quiet();

    PROFILE_RMA_LOAD_BEGIN(proc, nbytes);

    shmem_getmem(dest, remote_src, nbytes, proc);

    /* get has completed */

    if (hdl != NULL) {
        *hdl = NULL;
    }

    PROFILE_RMA_LOAD_END(proc);

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_read(size_t proc, void *src, void *dest, size_t nbytes)
{
    void *remote_src;

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

#if defined(ENABLE_LOCAL_MEMCPY)
    if (my_proc == proc) {
        memcpy(dest, src, nbytes);

        LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
        return;
        /* does not reach */
    }
#endif

    remote_src = get_remote_address(src, proc);
    LIBCAF_TRACE(LIBCAF_LOG_COMM,
                 "src: %p, remote_src: %p, offset: %p "
                 "start_all_images[proc]: %p, start_all_images[my_proc]: %p",
                 src, remote_src, comm_address_translation_offset(proc),
                 coarray_start_all_images[proc],
                 coarray_start_all_images[my_proc]);

    /* TODO: see if we can use shmem_fence() or something even more relaxed
     * instead? */
    shmem_quiet();

    PROFILE_RMA_LOAD_BEGIN(proc, nbytes);
    shmem_getmem(dest, remote_src, nbytes, proc);
    PROFILE_RMA_LOAD_END(proc);

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_write_from_lcb(size_t proc, void *dest, void *src, size_t nbytes,
                         int ordered, comm_handle_t * hdl)
{
    void *remote_dest;

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

#if defined(ENABLE_LOCAL_MEMCPY)
    if (my_proc == proc && nbytes <= SMALL_XFER_SIZE) {
        memcpy(dest, src, nbytes);
        comm_lcb_free(src);

        if (hdl != NULL && hdl != (void *) -1)
            *hdl = NULL;

        LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
        return;
        /* does not reach */
    }
#endif

    remote_dest = get_remote_address(dest, proc);

    if (ordered) {
        /* TODO: see if we can use shmem_fence() or something even more relaxed
         * instead? */
        if (rma_ordering == RMA_PUT_ORDERED) {
            shmem_quiet();
        } else if (rma_ordering == RMA_PUT_IMAGE_ORDERED) {
            shmem_fence();
        } else if (rma_ordering == RMA_PUT_ADDRESS_ORDERED) {
            shmem_fence();
        }

        PROFILE_RMA_STORE_BEGIN(proc, nbytes);

        if (hdl != (void *) -1 && rma_ordering != RMA_BLOCKING) {
            shmem_putmem(remote_dest, src, nbytes, proc);
            comm_lcb_free(src);

            PROFILE_RMA_STORE_END(proc);
        } else {
            /* block until remote completion */
            shmem_putmem(remote_dest, src, nbytes, proc);
            comm_lcb_free(src);

            shmem_quiet();

            PROFILE_RMA_STORE_END(proc);
        }

    } else {                    /* ordered == 0 */
        /* TODO: see if we can use shmem_fence() or something even more relaxed
         * instead? */
        if (rma_ordering == RMA_PUT_ORDERED) {
            shmem_quiet();
        } else if (rma_ordering == RMA_PUT_IMAGE_ORDERED) {
            shmem_fence();
        } else if (rma_ordering == RMA_PUT_ADDRESS_ORDERED) {
            shmem_fence();
        }

        PROFILE_RMA_STORE_BEGIN(proc, nbytes);

        if (hdl != (void *) -1 && rma_ordering != RMA_BLOCKING) {
            shmem_putmem(remote_dest, src, nbytes, proc);
            comm_lcb_free(src);

            PROFILE_RMA_STORE_END(proc);
        } else {
            /* block until remote completion */
            shmem_putmem(remote_dest, src, nbytes, proc);
            comm_lcb_free(src);

            shmem_quiet();

            PROFILE_RMA_STORE_END(proc);
        }
    }

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

/* like comm_write_from_lcb, except we don't need to "release" the source
 * buffer upon completion and guarantees local completion
 */
void comm_write(size_t proc, void *dest, void *src,
                size_t nbytes, int ordered, comm_handle_t * hdl)
{
    void *remote_dest;

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

#if defined(ENABLE_LOCAL_MEMCPY)
    if (my_proc == proc && nbytes <= SMALL_XFER_SIZE) {
        memcpy(dest, src, nbytes);

        if (hdl != NULL && hdl != (void *) -1)
            *hdl = NULL;
        LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
        return;
        /* does not reach */
    }
#endif

    remote_dest = get_remote_address(dest, proc);
    if (ordered && rma_ordering != RMA_RELAXED) {
        if (rma_ordering == RMA_PUT_ORDERED) {
            shmem_quiet();
        } else if (rma_ordering == RMA_PUT_IMAGE_ORDERED) {
            shmem_fence();
        } else if (rma_ordering == RMA_PUT_ADDRESS_ORDERED) {
            shmem_fence();
        }

        PROFILE_RMA_STORE_BEGIN(proc, nbytes);

        /* guarantees local completion */
        shmem_putmem(remote_dest, src, nbytes, proc);

        if (hdl != (void *) -1 && rma_ordering != RMA_BLOCKING) {
            PROFILE_RMA_STORE_END(proc);
        } else {
            /* block until remote completion */
            shmem_quiet();

            PROFILE_RMA_STORE_END(proc);
        }

    } else {                    /* ordered == 0 */
        if (rma_ordering == RMA_PUT_ORDERED) {
            shmem_quiet();
        } else if (rma_ordering == RMA_PUT_IMAGE_ORDERED) {
            shmem_fence();
        } else if (rma_ordering == RMA_PUT_ADDRESS_ORDERED) {
            shmem_fence();
        }

        PROFILE_RMA_STORE_BEGIN(proc, nbytes);

        shmem_putmem(remote_dest, src, nbytes, proc);

        if (hdl != (void *)-1) {
            PROFILE_RMA_STORE_END(proc);
        } else { /* hdl == -1 */
            /* block until it remotely completes */
            shmem_quiet();

            PROFILE_RMA_STORE_END(proc);
        }

    }

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

/* just a direct call into SHMEM which blocks only until local completion
 */
void comm_nbi_write(size_t proc, void *dest, void *src, size_t nbytes)
{
    void *remote_dest;

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    remote_dest = get_remote_address(dest, proc);

    PROFILE_RMA_STORE_BEGIN(proc, nbytes);
    shmem_putmem(remote_dest, src, nbytes, proc);
    PROFILE_RMA_STORE_END(proc);

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

/* same as comm_nbi_write */
void comm_write_x(size_t proc, void *dest, void *src, size_t nbytes)
{
    void *remote_dest;

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    remote_dest = get_remote_address(dest, proc);

    PROFILE_RMA_STORE_BEGIN(proc, nbytes);
    shmem_putmem(remote_dest, src, nbytes, proc);
    PROFILE_RMA_STORE_END(proc);

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}


void comm_strided_nbread(size_t proc,
                         void *src, const size_t src_strides[],
                         void *dest, const size_t dest_strides[],
                         const size_t count[], size_t stride_levels,
                         comm_handle_t * hdl)
{
    void *remote_src;

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();

#if defined(ENABLE_LOCAL_MEMCPY)
    size_t nbytes = 1;
    if (my_proc == proc) {
        /* local copy */
        local_strided_copy(src, src_strides, dest, dest_strides,
                count, stride_levels);
        if (hdl != NULL)
            *hdl = NULL;

        LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
        return;
        /* does not reach */
    }
#endif
    {
        size_t size;
        /* calculate max size (very conservative!) */
        size = src_strides[stride_levels - 1] * (count[stride_levels] - 1)
            + count[0];
        remote_src = get_remote_address(src, proc);

        if (rma_ordering == RMA_PUT_ORDERED) {
            shmem_quiet();
        } else if (rma_ordering == RMA_PUT_IMAGE_ORDERED) {
            shmem_fence();
        } else if (rma_ordering == RMA_PUT_ADDRESS_ORDERED) {
            shmem_fence();
        }

        PROFILE_RMA_LOAD_STRIDED_BEGIN(proc, stride_levels, count);

        /*
        ARMCI_GetS(remote_src, src_strides, dest, dest_strides,
                   count, stride_levels, proc);
                   */
        strided_get(remote_src, src_strides, dest, dest_strides, count,
                    stride_levels, proc);


        /* completed */
        if (hdl != NULL) {
            *hdl = NULL;
        }

        PROFILE_RMA_LOAD_END(proc);

    }

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void comm_strided_read(size_t proc,
                       void *src, const size_t src_strides[],
                       void *dest, const size_t dest_strides[],
                       const size_t count[], size_t stride_levels)
{
    void *remote_src;

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();


#if defined(ENABLE_LOCAL_MEMCPY)
    if (my_proc == proc) {
        /* local copy */
        local_strided_copy(src, src_strides, dest, dest_strides,
                           count, stride_levels);

        LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
        return;
        /* does not reach */
    }
#endif
    {
        size_t size;
        /* calculate max size (very conservative!) */
        size = src_strides[stride_levels - 1] * (count[stride_levels] - 1)
            + count[0];
        remote_src = get_remote_address(src, proc);

        if (rma_ordering == RMA_PUT_ORDERED) {
            shmem_quiet();
        } else if (rma_ordering == RMA_PUT_IMAGE_ORDERED) {
            shmem_fence();
        } else if (rma_ordering == RMA_PUT_ADDRESS_ORDERED) {
            shmem_fence();
        }


        PROFILE_RMA_LOAD_STRIDED_BEGIN(proc, stride_levels, count);

        /*
        ARMCI_GetS(remote_src, src_strides, dest, dest_strides, count,
                   stride_levels, proc);
                   */
        strided_get(remote_src, src_strides, dest, dest_strides, count,
                    stride_levels, proc);

        PROFILE_RMA_LOAD_END(proc);
    }

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}


void comm_strided_write_from_lcb(size_t proc,
                                 void *dest, const size_t dest_strides[],
                                 void *src, const size_t src_strides[],
                                 const size_t count[],
                                 size_t stride_levels, int ordered,
                                 comm_handle_t * hdl)
{
    void *remote_dest;
    int i;

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    check_for_error_stop();


#if defined(ENABLE_LOCAL_MEMCPY)
    size_t nbytes = 1;
    if (my_proc == proc) {
        for (i = 0; i <= stride_levels; i++) {
            nbytes = nbytes * count[i];
        }
        if (nbytes <= SMALL_XFER_SIZE) {
            /* local copy */
            local_strided_copy(src, src_strides, dest, dest_strides,
                               count, stride_levels);
            comm_lcb_free(src);

            if (hdl != NULL && hdl != (void *) -1)
                *hdl = NULL;

            LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
            return;
            /* does not reach */
        }
    }
#endif
    {
        remote_dest = get_remote_address(dest, proc);

        if (ordered) {
            size_t size;
            /* calculate max size (very conservative!) */
            size = (src_strides[stride_levels - 1] * count[stride_levels])
                + count[0];

            if (rma_ordering == RMA_PUT_ORDERED) {
                shmem_quiet();
            } else if (rma_ordering == RMA_PUT_IMAGE_ORDERED) {
                shmem_fence();
            } else if (rma_ordering == RMA_PUT_ADDRESS_ORDERED) {
                shmem_fence();
            }

            PROFILE_RMA_STORE_STRIDED_BEGIN(proc, stride_levels, count);

            if (hdl != (void *) -1) {
                /*
                ARMCI_NbPutS(src, src_strides, remote_dest, dest_strides,
                             count, stride_levels, proc,
                             handle_node->handle);
                             */
                strided_put(src, src_strides, remote_dest, dest_strides,
                            count, stride_levels, proc);
                comm_lcb_free(src);

                PROFILE_RMA_STORE_END(proc);
            } else {
                /* block until remote completion */
                /*
                ARMCI_PutS(src, src_strides, remote_dest, dest_strides,
                           count, stride_levels, proc);
                           */
                strided_put(src, src_strides, remote_dest, dest_strides,
                            count, stride_levels, proc);
                comm_lcb_free(src);

                shmem_quiet();

                PROFILE_RMA_STORE_END(proc);
            }

        } else {                /* ordered == 0 */
            size_t size;
            /* calculate max size (very conservative!) */
            size = (src_strides[stride_levels - 1] * count[stride_levels])
                + count[0];

            if (rma_ordering == RMA_PUT_ORDERED) {
                shmem_quiet();
            } else if (rma_ordering == RMA_PUT_IMAGE_ORDERED) {
                shmem_fence();
            } else if (rma_ordering == RMA_PUT_ADDRESS_ORDERED) {
                shmem_fence();
            }

            PROFILE_RMA_STORE_STRIDED_BEGIN(proc, stride_levels, count);


            /*
            ARMCI_NbPutS(src, src_strides, remote_dest, dest_strides,
                         count, stride_levels, proc, handle);
                         */
            strided_put(src, src_strides, remote_dest, dest_strides,
                        count, stride_levels, proc);

            comm_lcb_free(src);

            if (hdl != (void *)-1) {
                PROFILE_RMA_STORE_END(proc);
            } else { /* hdl == -1 */
                /* block until it remotely completes */
                shmem_quiet();

                PROFILE_RMA_STORE_END(proc);
            }
        }
    }

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

/* like comm_strided_write_from_lcb, except we don't need to "release" the
 * source buffer upon completion and local completion guaranteed
 */
void comm_strided_write(size_t proc,
                        void *dest,
                        const size_t dest_strides[], void *src,
                        const size_t src_strides[],
                        const size_t count[],
                        size_t stride_levels, int ordered,
                        comm_handle_t * hdl)
{
    void *remote_dest;
    int i;

    check_for_error_stop();

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");


#if defined(ENABLE_LOCAL_MEMCPY)
    size_t nbytes = 1;
    if (my_proc == proc) {
        for (i = 0; i <= stride_levels; i++) {
            nbytes = nbytes * count[i];
        }
        if (nbytes <= SMALL_XFER_SIZE) {
            /* local copy */
            local_strided_copy(src, src_strides, dest, dest_strides,
                               count, stride_levels);

            if (hdl != NULL && hdl != (void *) -1)
                *hdl = NULL;

            LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
            return;
            /* does not reach */
        }
    }
#endif
    {
        remote_dest = get_remote_address(dest, proc);

        if (ordered && rma_ordering != RMA_RELAXED) {
            size_t size;
            /* calculate max size (very conservative!) */
            size = (src_strides[stride_levels - 1] * count[stride_levels])
                + count[0];

            if (rma_ordering == RMA_PUT_ORDERED) {
                shmem_quiet();
            } else if (rma_ordering == RMA_PUT_IMAGE_ORDERED) {
                shmem_fence();
            } else if (rma_ordering == RMA_PUT_ADDRESS_ORDERED) {
                shmem_fence();
            }

            PROFILE_RMA_STORE_STRIDED_BEGIN(proc, stride_levels, count);

            /* guarantees local completion */
            /*
            ARMCI_PutS(src, src_strides, remote_dest, dest_strides,
                       count, stride_levels, proc);
                       */
            strided_put(src, src_strides, remote_dest, dest_strides,
                        count, stride_levels, proc);

            if (hdl != (void *) -1 && rma_ordering != RMA_BLOCKING) {
                PROFILE_RMA_STORE_END(proc);
            } else {
                /* block until it remotely completes */
                shmem_quiet();

                PROFILE_RMA_STORE_END(proc);
            }

        } else {                /* ordered == 0 */
            size_t size;
            /* calculate max size (very conservative!) */
            size = (src_strides[stride_levels - 1] * count[stride_levels])
                + count[0];

            if (rma_ordering == RMA_PUT_ORDERED) {
                shmem_quiet();
            } else if (rma_ordering == RMA_PUT_IMAGE_ORDERED) {
                shmem_fence();
            } else if (rma_ordering == RMA_PUT_ADDRESS_ORDERED) {
                shmem_fence();
            }

            PROFILE_RMA_STORE_STRIDED_BEGIN(proc, stride_levels, count);

            /*
            ARMCI_NbPutS(src, src_strides, remote_dest, dest_strides,
                         count, stride_levels, proc, handle);
                         */
            strided_put(src, src_strides, remote_dest, dest_strides,
                        count, stride_levels, proc);

            if (hdl != (void *)-1) {

                PROFILE_RMA_STORE_END(proc);

            } else { /* hdl == -1 */
                /* block until it remotely completes */
                shmem_quiet();

                PROFILE_RMA_STORE_END(proc);
            }

        }

    }

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

#ifdef PCAF_INSTRUMENT

/***************************************************************
 *                      Profiling Support
 ***************************************************************/

void profile_comm_handle_end(comm_handle_t hdl)
{
    if (hdl == (comm_handle_t) - 1) {
        PROFILE_RMA_END_ALL();
    } else if (hdl != NULL) {
        struct handle_list *handle_node = hdl;
        if (handle_node->access_type == PUTS) {
            profile_rma_nbstore_end(handle_node->proc,
                                    handle_node->rmaid);
        } else {
            profile_rma_nbload_end(handle_node->proc,
                                   handle_node->rmaid);
        }
    }
}
#endif
