/*
 ARMCI Communication Layer for supporting Coarray Fortran

 Copyright (C) 2009-2012 University of Houston.

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
#include "caf_rtl.h"
#include "comm.h"
#include "env.h"
#include "armci_comm_layer.h"
#include "trace.h"
#include "util.h"

extern unsigned long _this_image;
extern unsigned long _num_images;

/* common_slot is a node in the shared memory link-list that keeps track
 * of available memory that can used for both allocatable coarrays and
 * asymmetric data. It is the only handle to access the link-list.*/
extern struct shared_memory_slot *common_slot;

/*
 * Static Variable declarations
 */
static unsigned long my_proc;
static unsigned long num_procs;

/* sync images */
static void **syncptr; /* sync flags */

/* Shared memory management:
 * coarray_start_all_images stores the shared memory start address of all
 * images */
static void **coarray_start_all_images;

/* NON-BLOCKING PUT: ARMCI non-blocking operations does not ensure local
 * completion on its return. ARMCI_Wait(handle) provides local completion,
 * as opposed to GASNET where it means remote completion. For remote
 * completion, ARMCI_Fence(remote_proc) needs to be invoked. However, we
 * can not FENCE for a specific handle. Hence we wait for all remote
 * writes to finish when a new read/write adrress overlaps any one of
 * them.  */
static int enable_nbput; /* 0=disabled. set by env var UHCAF_NBPUT */
static void **min_nbwrite_address;
static void **max_nbwrite_address;

/* GET CACHE OPTIMIZATION */
static int enable_get_cache; /* set by env variable */
static unsigned long getCache_line_size; /* set by env var. */
/* Instead of making the cache_all_image an array of struct cache, I
 * make it an array of pointer to struct cache. This will make it easy
 * to add more cache lines in the future by making it 2D array */
static struct cache **cache_all_images;
static unsigned long shared_memory_size;
static unsigned long static_heap_size;

/* Mutexes */
static int critical_mutex;


/* Local function declarations */
static void *get_remote_address(void *src, size_t img);
static void clear_all_cache();
static void clear_cache(size_t node);
static void cache_check_and_get(size_t node, void *remote_address,
                            size_t nbytes, void *local_address);
static void cache_check_and_get_strided(
        void *remote_src, int src_strides[],
        void *local_dest, int dest_strides[],
        int count[], size_t stride_levels, size_t node);
static void update_cache(size_t node, void *remote_address,
                        size_t nbytes, void *local_address);

/* NONBLOCKING PUT OPTIMIZATION */
static int address_in_nbwrite_address_block(void *remote_addr,
       size_t proc, size_t size);
static void update_nbwrite_address_block(void *remote_addr,
        size_t proc, size_t size);
static void check_wait_on_pending_puts(size_t proc, void* remote_address,
                                size_t size);
static void wait_on_pending_puts(size_t proc);
static void wait_on_all_pending_puts();



/*
 * Inline functions
 */
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

static inline int address_on_heap(void *addr)
{
    void *start_heap;
    void *end_heap;

    start_heap = coarray_start_all_images[my_proc];
    end_heap = common_slot->addr;

    return (addr >= start_heap && addr <= end_heap);
}


/* TODO: Right now, for every critical section we simply acquire a mutex on
 * process 0. Instead, we should define a mutex for each critical section in
 * order to allow execution of different critical sections at the same time.
 */
void comm_critical()
{
    ARMCI_Lock(critical_mutex, 0);
}

void comm_end_critical()
{
    ARMCI_Unlock(critical_mutex, 0);
}

void comm_service()
{
    /* TODO */
}

/*
 * INIT:
 * 1) Initialize ARMCI
 * 2) Create flags and mutexes for sync_images
 * 3) Create pinned memory and populates coarray_start_all_images.
 * 4) Populates common_shared_memory_slot with the pinned memory data
 *    which is used by caf_rtl.c to allocate/deallocate coarrays.
 */
void comm_init(struct shared_memory_slot *common_shared_memory_slot)
{
    int ret,i;
    int   argc = 1;
    char  **argv;
    unsigned long caf_shared_memory_size;
    unsigned long max_size=powl(2,(sizeof(unsigned long)*8))-1;

    argv = (char **) malloc(argc * sizeof(*argv));
    argv[0] = "caf";

    MPI_Init(&argc, &argv);

    ret = ARMCI_Init();
    if (ret != 0) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL, "ARMCI init error");
    }

    MPI_Comm_rank (MPI_COMM_WORLD, (int *)&my_proc);
    MPI_Comm_size (MPI_COMM_WORLD, (int *)&num_procs);

    /* set extern symbols used for THIS_IMAGE and NUM_IMAGES intrinsics */
    _this_image = my_proc+1;
    _num_images = num_procs;

    /* Check if optimizations are enabled */
    enable_get_cache = get_env_flag(ENV_GETCACHE,
                                    DEFAULT_ENABLE_GETCACHE);
    getCache_line_size = get_env_size(ENV_GETCACHE_LINE_SIZE,
                                      DEFAULT_GETCACHE_LINE_SIZE);
    enable_nbput = get_env_flag(ENV_NBPUT,
                                DEFAULT_ENABLE_NBPUT);

    /* Create flags and mutex for sync_images and critical regions
     * For every image, we create num_procs mutexes and one additional
     * mutex for the critical region. This needs to be changed to
     * multiple mutexes based depending on the number of critical regions.
     * The important point to note here is that ARMCI_Create_mutexes can be
     * called only once per image
     */

    ARMCI_Create_mutexes(num_procs+1);
    syncptr = malloc (num_procs*sizeof(void *));
    ARMCI_Malloc ((void**)syncptr, num_procs*sizeof(int));
    for ( i=0; i<num_procs; i++ )
        ((int*)(syncptr[my_proc]))[i]=0;

    critical_mutex = num_procs; /* last mutex reserved for critical sections */

    /* Create pinned/registered memory (Shared Memory) */
    coarray_start_all_images = malloc (num_procs*sizeof(void *));

    caf_shared_memory_size = get_env_size(ENV_SHARED_MEMORY_SIZE,
                              DEFAULT_SHARED_MEMORY_SIZE);
    if (caf_shared_memory_size>=max_size) /*overflow check*/
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                "Shared memory size must be less than %lu bytes",max_size);
    }


    ret = ARMCI_Malloc ((void**)coarray_start_all_images,
            caf_shared_memory_size);
    if(ret != 0)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
        "ARMCI_Malloc failed when allocating %lu (%luMB)"
        ,caf_shared_memory_size, caf_shared_memory_size/1000000L );
    }

    static_heap_size =
        allocate_static_coarrays(coarray_start_all_images[my_proc]);

    if(enable_nbput)
    {
        min_nbwrite_address= malloc(num_procs*sizeof(void*));
        max_nbwrite_address= malloc(num_procs*sizeof(void*));
        /* initialize data structures to 0 */
        for(i=0; i<num_procs; i++){
            min_nbwrite_address[i]=0;
            max_nbwrite_address[i]=0;
        }
    }
    if (enable_get_cache)
    {
        cache_all_images =
            (struct cache **)malloc(num_procs* sizeof(struct cache *));
        /* initialize data structures to 0 */
        for(i=0; i<num_procs; i++){
            cache_all_images[i]=(struct cache *)malloc(sizeof(struct cache));
            cache_all_images[i]->remote_address=0;
            cache_all_images[i]->handle=0;
            cache_all_images[i]->cache_line_address=malloc(getCache_line_size);
        }
    }

    /* initialize common shared memory slot */
    common_shared_memory_slot->addr = coarray_start_all_images[my_proc]
                                                + static_heap_size;
    common_shared_memory_slot->size = caf_shared_memory_size
                                                - static_heap_size;
    common_shared_memory_slot->feb = 0;
    common_shared_memory_slot->next =0;
    common_shared_memory_slot->prev =0;

    shared_memory_size = caf_shared_memory_size;

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"armci_comm_layer.c:comm_init-> Img%lu"
        "Finished. Waiting for global barrier."
        "common_slot->addr=%p, common_slot->size=%lu",
        my_proc+1, common_shared_memory_slot->addr,
        common_shared_memory_slot->size);

    ARMCI_Barrier();
}

/*
 * Static Functions for GET CACHE
 */

/* naive implementation of strided copy
 * TODO: improve by finding maximal blocksize
 */
void local_strided_copy(void *src, const int src_strides[],
        void *dest, const int dest_strides[],
        const int count[], size_t stride_levels)
{
    int i,j;
    size_t num_blks;
    size_t cnt_strides[stride_levels+1];
    size_t blockdim_size[stride_levels];
    void *dest_ptr = dest;
    void *src_ptr = src;

    /* assuming src_elem_size=dst_elem_size */
    size_t blk_size = count[0];
    num_blks = 1;
    cnt_strides[0] = 1;
    blockdim_size[0] = count[0];
    for (i = 1; i <= stride_levels; i++)
    {
        cnt_strides[i] = cnt_strides[i-1] * count[i];
        blockdim_size[i] = blk_size * cnt_strides[i];
        num_blks *= count[i];
    }

    for (i = 1; i <= num_blks; i++)
    {
        memcpy(dest_ptr, src_ptr, blk_size);
        for (j=1; j<=stride_levels; j++) {
            if (i%cnt_strides[j]) break;
            src_ptr -= (count[j]-1) * src_strides[j-1];
            dest_ptr -= (count[j]-1) * dest_strides[j-1];
        }
        src_ptr += src_strides[j-1];
        dest_ptr += dest_strides[j-1];
    }
}


static void refetch_all_cache()
{
    int i;
    for ( i=0; i<num_procs; i++ )
    {
        if(cache_all_images[i]->remote_address)
        {
            ARMCI_NbGet(cache_all_images[i]->remote_address,
                    cache_all_images[i]->cache_line_address,
                    getCache_line_size, i, cache_all_images[i]->handle);
        }
    }
    LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "armci_comm_layer.c:refetch_all_cache-> Finished nb get");
}

static void refetch_cache(unsigned long node)
{
    if(cache_all_images[node]->remote_address)
    {
        ARMCI_NbGet(cache_all_images[node]->remote_address,
                cache_all_images[node]->cache_line_address,
                getCache_line_size, node, cache_all_images[node]->handle);
    }
    LIBCAF_TRACE(LIBCAF_LOG_CACHE,
    "armci_comm_layer.c:refetch_cache-> Finished nb get from image %lu",
            node+1);
}

static void cache_check_and_get(size_t node, void *remote_address,
                            size_t nbytes, void *local_address)
{
    void *cache_address = cache_all_images[node]->remote_address;
    size_t start_offset;
    void *cache_line_address = cache_all_images[node]->cache_line_address;
    /* data in cache */
    if(cache_address>0 && remote_address >= cache_address &&
           remote_address+nbytes <= cache_address+getCache_line_size)
    {
        start_offset=remote_address-cache_address;
        if(cache_all_images[node]->handle)
        {
            ARMCI_Wait(cache_all_images[node]->handle);
            cache_all_images[node]->handle = 0;
        }
        memcpy(local_address, cache_line_address+start_offset, nbytes);
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
                "armci_comm_layer.c:cache_check_and_get-> Address %p on"
                " image %lu found in cache.", remote_address, node+1);
    }
    else /*data not in cache*/
    {
        /* data NOT from end of shared segment OR bigger than cacheline*/
        if(((remote_address+getCache_line_size) <=
                (coarray_start_all_images[node]+shared_memory_size))
            && (nbytes <= getCache_line_size))
        {
            ARMCI_Get(remote_address, cache_line_address,
                    getCache_line_size, node);
            cache_all_images[node]->remote_address=remote_address;
            cache_all_images[node]->handle = 0;
            memcpy(local_address, cache_line_address, nbytes);
        }
        else{
            ARMCI_Get(remote_address, local_address, nbytes, node);
        }
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
                "armci_comm_layer.c:cache_check_and_get-> Address %p on"
                " image %lu NOT found in cache.", remote_address, node+1);
    }
}



static void cache_check_and_get_strided(
        void *remote_src, int src_strides[],
        void *local_dest, int dest_strides[],
        int count[], size_t stride_levels, size_t node)
{
    void *cache_address = cache_all_images[node]->remote_address;
    size_t start_offset;
    void *cache_line_address =cache_all_images[node]->cache_line_address;
    size_t size;
    int i,j;

    size = src_strides[stride_levels-1] * (count[stride_levels]-1)
           + count[0];

    /* data in cache */
    if(cache_address>0 && remote_src >= cache_address &&
           remote_src+size <= cache_address+getCache_line_size)
    {
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
                "armci_comm_layer.c:cache_check_and_get_strided->"
                "Address %p on image %lu found in cache.",
                remote_src, node+1);
        start_offset=remote_src-cache_address;
        if(cache_all_images[node]->handle)
        {
            ARMCI_Wait(cache_all_images[node]->handle);
            cache_all_images[node]->handle = 0;
        }

        local_strided_copy(cache_line_address+start_offset,
                src_strides, local_dest, dest_strides,
                count, stride_levels);


    }
    else /*data not in cache*/
    {
        /* data NOT from end of shared segment OR bigger than cacheline*/
        if(((remote_src+getCache_line_size) <=
            (coarray_start_all_images[node]+shared_memory_size))
            && (size <= getCache_line_size))
        {
            LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "armci_comm_layer.c:cache_check_and_get_strided->"
            " Data for Address %p on image %lu NOT found in cache.",
            remote_src, node+1);

            ARMCI_Get(remote_src, cache_line_address, getCache_line_size,
                      node);
            cache_all_images[node]->remote_address=remote_src;
            cache_all_images[node]->handle = 0;

            local_strided_copy(cache_line_address,
                    src_strides, local_dest, dest_strides,
                    count, stride_levels);
        }
        else{
            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                "armci_comm_layer.c:cache_check_and_get_strided>gasnet_gets_bulk from"
                " %p on image %lu to %p (stride_levels= %u)",
                remote_src, node+1, local_dest, stride_levels);

            ARMCI_GetS(remote_src, src_strides,
                    local_dest, dest_strides,
                    count,  stride_levels, node );
        }
    }
}


/* Update cache if remote write overlap -- like writethrough cache */
static void update_cache(size_t node, void *remote_address,
                        size_t nbytes, void *local_address)
{
    void *cache_address = cache_all_images[node]->remote_address;
    size_t start_offset;
    void *cache_line_address = cache_all_images[node]->cache_line_address;
    if(cache_address>0 && remote_address >= cache_address &&
           remote_address+nbytes <= cache_address+getCache_line_size)
    {
        start_offset = remote_address - cache_address;
        memcpy(cache_line_address+start_offset,local_address, nbytes);
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "armci_comm_layer.c:update_cache-> Value of address %p on"
            " image %lu updated in cache due to write conflict.",
            remote_address, node+1);
    }
    else if (cache_address>0 &&
             remote_address >= cache_address &&
             remote_address <= cache_address + getCache_line_size)
    {
        start_offset = remote_address - cache_address;
        nbytes = getCache_line_size - start_offset;
        memcpy(cache_line_address+start_offset,local_address, nbytes);
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "armci_comm_layer.c:update_cache-> Value of address %p on"
            " image %lu partially updated in cache (write conflict).",
            remote_address, node+1);
    }
    else if (cache_address>0 &&
             remote_address+nbytes >= cache_address &&
             remote_address+nbytes<=cache_address+getCache_line_size)
    {
        start_offset = cache_address-remote_address;
        nbytes = nbytes - start_offset;
        memcpy(cache_line_address,local_address+start_offset,nbytes);
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "armci_comm_layer.c:update_cache-> Value of address %p on"
            " image %lu partially updated in cache (write conflict).",
            remote_address, node+1);
    }
}


static void update_cache_strided(
        void* remote_dest_address, const int dest_strides[],
        void* local_src_address, const int src_strides[],
        const int count[], size_t stride_levels,
        size_t node)
{
    void *cache_address = cache_all_images[node]->remote_address;
    size_t start_offset;
    void *cache_line_address =cache_all_images[node]->cache_line_address;
    size_t size;

    /* calculate max size (very conservative!) */
    size = (dest_strides[stride_levels-1]*(count[stride_levels]-1))
            + count[0];
    
    /* New data completely fit into cache */
    if(cache_address>0 && remote_dest_address >= cache_address &&
           remote_dest_address+size <= cache_address+getCache_line_size)
    {
        start_offset = remote_dest_address - cache_address;

        local_strided_copy( local_src_address, src_strides,
                cache_line_address+start_offset, dest_strides,
                count, stride_levels);

        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
        "armci_comm_layer.c:update_cache_full_strided->"
        " Value of address %p on"
        " image %lu updated in cache due to write conflict.",
            remote_dest_address, node+1);
    }
    /* Some memory overlap */
    else if (cache_address>0 && 
            ((remote_dest_address+size > cache_address &&
             remote_dest_address+size < cache_address+getCache_line_size)
             ||
             ( remote_dest_address > cache_address &&
               remote_dest_address < cache_address+getCache_line_size)
            ))
    {
        //make it invalid
        cache_all_images[node]->remote_address=0;
    }
}


/*
 * static functions for non-blocking put
 */
static int address_in_nbwrite_address_block(void *remote_addr,
       size_t proc, size_t size)
{
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"armci_comm_layer.c:"
        "address_in_nbwrite_address_block-> Img%lu "
        "remote address:%p, min:%p, max:%p, addr+size:%p",
        proc+1, remote_addr, min_nbwrite_address[proc],
        max_nbwrite_address[proc], remote_addr+size);
    if(min_nbwrite_address[proc]==0)
        return 0;
    if(((remote_addr+size) >= min_nbwrite_address[proc])
      && (remote_addr <= max_nbwrite_address[proc]))
        return 1;
    else
        return 0;
}

static void update_nbwrite_address_block(void *remote_addr,
        size_t proc, size_t size)
{
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"armci_comm_layer.c:"
        "update_nbwrite_address_block-> Img%lu "
        "remote address:%p, min:%p, max:%p, addr+size:%p",
        proc+1, remote_addr, min_nbwrite_address[proc],
        max_nbwrite_address[proc], remote_addr+size);
    if(min_nbwrite_address[proc]==0)
    {
        min_nbwrite_address[proc]=remote_addr;
        max_nbwrite_address[proc]=remote_addr+size;
        return;
    }
    if (remote_addr < min_nbwrite_address[proc])
        min_nbwrite_address[proc]=remote_addr;
    if ((remote_addr+size) > max_nbwrite_address[proc])
        max_nbwrite_address[proc]=remote_addr+size;
}

static void check_wait_on_pending_puts(size_t proc, void* remote_address,
                                size_t size)
{
    if(address_in_nbwrite_address_block(remote_address, proc, size))
    {
        ARMCI_Fence(proc);
        min_nbwrite_address[proc]=0;
        max_nbwrite_address[proc]=0;
    }
}

static void wait_on_pending_puts(size_t proc)
{
    ARMCI_Fence(proc);
    min_nbwrite_address[proc]=0;
    max_nbwrite_address[proc]=0;
}

static void wait_on_all_pending_puts()
{
    int i;
    ARMCI_AllFence();
    for ( i=0; i<num_procs; i++)
    {
        min_nbwrite_address[i]=0;
        max_nbwrite_address[i]=0;
    }
}

/*
 * End of static functions for non-blocking put
 */

/*
 * Shared Memory Management
 */

/* It should allocate memory to all static coarrays from the pinned-down
 * memory created during init */
unsigned long set_save_coarrays_(void *base_address)
{
    return 0;
}
#pragma weak set_save_coarrays = set_save_coarrays_
unsigned long set_save_coarrays(void *base_address);

unsigned long allocate_static_coarrays(void *base_address)
{
    return set_save_coarrays(base_address);
}

/* returns addresses ranges for shared heap */

inline void* comm_start_heap(size_t proc)
{
    return get_remote_address(coarray_start_all_images[my_proc],
            proc);
}

inline void* comm_end_heap(size_t proc)
{
    return get_remote_address(
            (char *)coarray_start_all_images[my_proc]+ shared_memory_size,
            proc);
}

inline void* comm_start_symmetric_heap(size_t proc)
{
    return comm_start_heap(proc);
}

inline void *comm_end_symmetric_heap(size_t proc)
{
    return get_remote_address(common_slot->addr,proc);
}

inline void *comm_start_asymmetric_heap(size_t proc)
{
    if (proc != my_proc) {
        return comm_end_symmetric_heap(proc);
    } else {
        return (char *)common_slot->addr + common_slot->size;
    }
}

inline void *comm_end_asymmetric_heap(size_t proc)
{
    return get_remote_address(comm_end_heap(proc), proc);
}

inline void *comm_start_static_heap(size_t proc)
{
    return get_remote_address(comm_start_heap(proc), proc);
}

inline void *comm_end_static_heap(size_t proc)
{
    return (char *)comm_start_heap(proc) + static_heap_size;
}

inline void *comm_start_allocatable_heap(size_t proc)
{
    return comm_end_static_heap(proc);
}

inline void *comm_end_allocatable_heap(size_t proc)
{
    return comm_end_symmetric_heap(proc);
}


/* Calculate the address on another image corresponding to a local address
 * This is possible as all images must have the same coarrays, i.e the
 * memory is symmetric. Since we know the start address of all images
 * from coarray_start_all_images, remote_address = start+offset
 * NOTE: remote_address on this image might be different from the actual
 * address on that image. So don't rely on it while debugging*/
static void *get_remote_address(void *src, size_t img)
{
    size_t offset;
    void *remote_address;
    if ( (img == my_proc) || !address_on_heap(src) )
        return src;
    offset = src - coarray_start_all_images[my_proc];
    remote_address = coarray_start_all_images[img]+offset;
    return remote_address;
}

/*
 * End Shared Memory Management
 */

void comm_memory_free()
{
    coarray_free_all_shared_memory_slots(); /* in caf_rtl.c */
    comm_free(syncptr);
    ARMCI_Free(coarray_start_all_images[my_proc]);
    comm_free(coarray_start_all_images);
    if(enable_get_cache)
    {
        int i;
        for(i=0; i<num_procs; i++)
        {
            comm_free(cache_all_images[i]->cache_line_address);
            comm_free(cache_all_images[i]);
        }
        comm_free(cache_all_images);
    }
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_memory_free-> Finished.");
}

void comm_exit(int status)
{
    comm_memory_free();
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "armci_comm_layer.c:comm_exit-> Before call to ARMCI_Error"
        " with status %d." ,status);

    //ARMCI_Error("ARMCI error",status);

    MPI_Finalize();
    exit (status);

    /* does not reach */
}

void comm_finalize()
{
    comm_barrier_all();
    comm_memory_free();
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_exit-> Before call to ARMCI_Finalize"
            );
    ARMCI_Finalize();
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_exit-> Before call to MPI_Finalize");
    MPI_Finalize();
    exit(0);

    /* does not reach */
}

void comm_barrier_all()
{
    if(enable_nbput)
        wait_on_all_pending_puts();
    ARMCI_WaitAll();
    ARMCI_Barrier();
    if(enable_get_cache)
        refetch_all_cache();
}

void comm_sync_images(int *image_list, int image_count)
{
    int i, remote_img;
    int *dest_flag; /* remote flag to set */
    volatile int *check_flag; /* flag to wait on locally */
    int whatever; /* to store remote value for ARMCI_Rmw */

    LIBCAF_TRACE(LIBCAF_LOG_BARRIER,
            "armci_comm_layer.c:comm_sync_images-> Syncing with"
            " %d images",image_count);
    for (i=0; i<image_count; i++)
    {
        if(my_proc == image_list[i])
        {
            continue;
        }
        remote_img = image_list[i];

        if(enable_nbput)
        {
            wait_on_pending_puts(remote_img);
            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                "armci_comm_layer.c:comm_sync_images->Finished waiting for"
                " all pending puts on image %lu. Min:%p, Max:%p", remote_img,
                min_nbwrite_address[remote_img], max_nbwrite_address[remote_img]);
        }

        if (remote_img < 0 || remote_img >= num_procs)
        {
            LIBCAF_TRACE(LIBCAF_LOG_FATAL,"sync_images called with "
                    "invalid remote image %d\n", remote_img);
        }

        /* complete any blocking communications to remote image first */
        ARMCI_Fence(remote_img);

        dest_flag = ((int *)syncptr[remote_img]) + my_proc;

        ARMCI_Lock(my_proc, remote_img);
        ARMCI_Rmw
            (ARMCI_FETCH_AND_ADD,(void *)&whatever, (void*)dest_flag, 1, remote_img);
        ARMCI_Unlock(my_proc, remote_img);
    }

    for (i=0; i<image_count; i++)
    {
        if(my_proc == image_list[i])
        {
            continue;
        }
        remote_img = image_list[i];

        check_flag = ((int *)syncptr[my_proc])+remote_img;

        LIBCAF_TRACE(LIBCAF_LOG_BARRIER,
            "armci_comm_layer.c:comm_sync_images-> Waiting on"
            " image %lu.", remote_img+1);

      /* user usleep to wait at least 1 OS time slice before checking
       * flag again  */
        while (!(*check_flag)) usleep(50);

        LIBCAF_TRACE(LIBCAF_LOG_BARRIER,
            "armci_comm_layer.c:comm_sync_images-> Waiting over on"
            " image %lu. About to decrement %d",
            remote_img+1, *check_flag);

        ARMCI_Lock(remote_img, my_proc);
           (*check_flag)--  ;
           /* dont just make it 0, maybe more than 1 sync_images
            * are present back to back
            * */
        ARMCI_Unlock(remote_img, my_proc);

        if(enable_get_cache)
            refetch_cache(image_list[i]);

        LIBCAF_TRACE(LIBCAF_LOG_BARRIER,
            "armci_comm_layer.c:comm_sync_images-> Sync image over");
    }

}

/* atomics */
void comm_swap_request (void *target, void *value, size_t nbytes,
			    int proc, void *retval)
{
    check_remote_address(proc+1, target);
    if (nbytes == sizeof(int) ) {
        void *remote_address = get_remote_address(target, proc);
        ARMCI_Rmw( ARMCI_SWAP, value, remote_address, 0, proc);
        (void) ARMCI_Rmw( ARMCI_SWAP, value, remote_address, 0, proc);
        memmove( retval, value, nbytes);
    } else if (nbytes == sizeof(long) ) {
        void *remote_address = get_remote_address(target, proc);
        (void) ARMCI_Rmw( ARMCI_SWAP_LONG, value, remote_address, 0, proc);
        memmove( retval, value, nbytes);
    }
}

void
comm_cswap_request (void *target, void *cond, void *value,
			     size_t nbytes, int proc, void *retval)
{
    check_remote_address(proc+1, target);
    /* TODO */
    Error( "comm_cswap_request not implemented for ARMCI conduit" );
}

void comm_fadd_request (void *target, void *value, size_t nbytes, int proc,
			    void *retval)
{
    long long old;
    check_remote_address(proc+1, target);
    memmove(&old,value,nbytes);
    if (nbytes == sizeof(int) ) {
        void *remote_address = get_remote_address(target, proc);
        *(int*)retval =
            ARMCI_Rmw( ARMCI_FETCH_AND_ADD, 0, remote_address,*(int*)value,
                    proc);
        memmove(value, &old, nbytes);
    } else if (nbytes == sizeof(long) ) {
        void *remote_address = get_remote_address(target, proc);
        *(long*)retval =
            ARMCI_Rmw( ARMCI_FETCH_AND_ADD_LONG, 0, remote_address,*(int*)value,
                    proc);
        memmove(value, &old, nbytes);
    }
}

void comm_fstore_request (void *target, void *value, size_t nbytes, int proc,
			    void *retval)
{
    long long old;
    check_remote_address(proc+1, target);
    memmove(&old,value,nbytes);
    if (nbytes == sizeof(int) ) {
        void *remote_address = get_remote_address(target, proc);
        ARMCI_Rmw( ARMCI_SWAP, value, remote_address, 0, proc);
        memmove(retval, value, nbytes);
        memmove(value, &old, nbytes);
    } else if (nbytes == sizeof(long) ) {
        void *remote_address = get_remote_address(target, proc);
        ARMCI_Rmw( ARMCI_SWAP_LONG, value, remote_address, 0,
                   proc);
        memmove(retval, value, nbytes);
        memmove(value, &old, nbytes);
    }
}

void* comm_malloc(size_t size) //To make it sync with gasnet
{
    return malloc(size);
}

void comm_free ( void *ptr) //To make it sync with gasnet
{
    free(ptr);
}

void comm_free_lcb ( void *ptr) //To make it sync with gasnet
{
    free(ptr);
}

void comm_read( size_t proc, void *src, void *dest, size_t nbytes)
{
    void *remote_src;
    remote_src = get_remote_address(src,proc);
    if(enable_nbput)
    {
        check_wait_on_pending_puts(proc, remote_src, nbytes);
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "armci_comm_layer.c:comm_read->Finished waiting for"
        " pending puts on %p on image %lu. Min:%p, Max:%p", remote_src,
        proc+1, min_nbwrite_address[proc], max_nbwrite_address[proc] );
    }
    if(enable_get_cache)
        cache_check_and_get(proc, remote_src, nbytes, dest);
    else{
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_read->Before ARMCI_Get from %p on"
            " image %lu to %p size %lu",
            remote_src, proc+1, dest, nbytes);
        ARMCI_Get(remote_src, dest, (int)nbytes, (int)proc);
    }
}

void comm_write( size_t proc, void *dest, void *src, size_t nbytes)
{
    void *remote_dest;
    remote_dest = get_remote_address(dest,proc);
    if(enable_nbput)
    {
        armci_hdl_t handle;
        ARMCI_INIT_HANDLE(&handle);
        check_wait_on_pending_puts(proc, remote_dest, nbytes);
        ARMCI_NbPut(src, remote_dest, nbytes, proc, &handle);
        ARMCI_Wait(&handle); // This ensures local completion only
        update_nbwrite_address_block(remote_dest, proc, nbytes);
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_write->After ARMCI_NbPut"
            " to %p on image %lu. Min:%p, Max:%p", remote_dest, proc+1,
            min_nbwrite_address[proc], max_nbwrite_address[proc] );
    }
    else
    {
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                "armci_comm_layer.c:comm_write->Before ARMCI_Put to %p on"
                " image %lu from %p size %lu",
                remote_dest, proc+1, src, nbytes);
        ARMCI_Put(src, remote_dest, (int)nbytes, (int)proc);
    }
    if(enable_get_cache)
        update_cache(proc, remote_dest, nbytes, src);
}


void comm_strided_read ( size_t proc,
        void *src, const size_t src_strides_[],
        void *dest, const size_t dest_strides_[],
        const size_t count_[], size_t stride_levels)
{
    void *remote_src;
    int src_strides[stride_levels];
    int dest_strides[stride_levels];
    int count[stride_levels+1];

    /* ARMCI uses int for strides and count arrays */
    int i;
    for (i = 0; i < stride_levels; i++) {
        src_strides[i] = src_strides_[i];
        dest_strides[i] = dest_strides_[i];
        count[i] = count_[i];
    }
    count[stride_levels] = count_[stride_levels];

#if defined(ENABLE_LOCAL_MEMCPY)
    if (  my_proc == proc  ) {
        /* local copy */
        local_strided_copy( src, src_strides, dest, dest_strides,
                            count, stride_levels );
    } else
#endif
    {
        remote_src = get_remote_address(src,proc);
        if(enable_nbput) {
            size_t size;
            /* calculate max size (very conservative!) */
            size = src_strides[stride_levels-1] * (count[stride_levels]-1)
                   + count[0];
            check_wait_on_pending_puts(proc, remote_src, size);
            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_strided_read->Finished waiting for"
            " pending puts on %p on image %lu. Min:%p,Max:%p",remote_src,
            proc+1, min_nbwrite_address[proc],max_nbwrite_address[proc] );
        }

        if(enable_get_cache) {

            cache_check_and_get_strided(remote_src, src_strides,
                   dest, dest_strides, count, stride_levels, proc);

        } else {

            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                "gasnet_comm_layer.c:comm_strided_read->gasnet_gets_bulk from"
                " %p on image %lu to %p (stride_levels= %u)",
                remote_src, proc+1, dest, stride_levels);
            ARMCI_GetS (remote_src, src_strides, dest, dest_strides, count,
                       stride_levels, proc);
        }
    }
}


void comm_strided_write ( size_t proc,
        void *dest, const size_t dest_strides_[],
        void *src, const size_t src_strides_[],
        const size_t count_[], size_t stride_levels)
{
    void *remote_dest;
    int src_strides[stride_levels];
    int dest_strides[stride_levels];
    int count[stride_levels+1];

    /* ARMCI uses int for strides and count arrays */
    int i;
    for (i = 0; i < stride_levels; i++) {
        src_strides[i] = src_strides_[i];
        dest_strides[i] = dest_strides_[i];
        count[i] = count_[i];
    }
    count[stride_levels] = count_[stride_levels];

#if defined(ENABLE_LOCAL_MEMCPY)
    if (  my_proc == proc  ) {
        /* local copy */
        local_strided_copy( src, src_strides, dest, dest_strides,
                            count, stride_levels );
    } else
#endif
    {
        remote_dest = get_remote_address(dest,proc);

        if(enable_nbput) {
            armci_hdl_t handle;
            ARMCI_INIT_HANDLE(&handle);
            size_t size;
            /* calculate max size (very conservative!) */
            size = (src_strides[stride_levels-1] * count[stride_levels])
                    + count[0];

            check_wait_on_pending_puts(proc, remote_dest, size);
            ARMCI_NbPutS(src, src_strides, remote_dest, dest_strides,
                             count,  stride_levels, proc, &handle );
            ARMCI_Wait(&handle); // This ensures local completion only
            update_nbwrite_address_block(remote_dest, proc, size);
            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_strided_write->After ARMCI_NbPutS"
            " to %p on image %lu. Min:%p, Max:%p", remote_dest, proc+1,
            min_nbwrite_address[proc], max_nbwrite_address[proc] );
        } else {

            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                "armci_comm_layer.c:comm_strided_write->before ARMCI_PutS"
                " to %p on image %lu from %p (stride_levels= %u)",
                remote_dest, proc+1, src, stride_levels);
            ARMCI_PutS(src, src_strides, remote_dest, dest_strides,
                       count, stride_levels, proc );
        }

        if(enable_get_cache) {
            update_cache_strided(remote_dest, dest_strides,
                   src, src_strides, count, stride_levels, proc);
        }
    }
}
