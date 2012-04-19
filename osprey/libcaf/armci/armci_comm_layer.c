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

#include "armci_comm_layer.h"
#include "util.h"

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

/* Mutexes */
static int critical_mutex;


/*
 * Inline functions
 */
/* must call comm_init() first */
inline unsigned long comm_get_proc_id()
{
    return my_proc;
}

/* must call comm_init() first */
inline unsigned long comm_get_num_procs()
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
    char *caf_shared_memory_size_str, *enable_nbput_str;
    char *enable_get_cache_str, *getCache_line_size_str;
    unsigned long caf_shared_memory_size;
    unsigned long static_coarray_size;
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

    /* Check if optimizations are enabled */
    enable_get_cache_str = getenv("UHCAF_GETCACHE");
    if(enable_get_cache_str != NULL)
    {
        sscanf(enable_get_cache_str, "%d",
                &enable_get_cache);
    }
    else
    {
        enable_get_cache = 0;
    }
    getCache_line_size_str = getenv("UHCAF_GETCACHE_LINE_SIZE");
    if(getCache_line_size_str != NULL)
    {
        sscanf(getCache_line_size_str, "%lu",
                &getCache_line_size);
    }
    else
    {
        getCache_line_size = DEFAULT_GETCACHE_LINE_SIZE;
    }
    enable_nbput_str = getenv("UHCAF_NBPUT");
    if(enable_nbput_str != NULL)
    {
        sscanf(enable_nbput_str, "%d",
                &enable_nbput);
    }
    else
    {
        enable_nbput = 0;
    }

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
    caf_shared_memory_size_str = getenv("UHCAF_IMAGE_HEAP_SIZE");
    if(caf_shared_memory_size_str != NULL)
    {
        sscanf(caf_shared_memory_size_str, "%lu",
                &caf_shared_memory_size);
        if (caf_shared_memory_size>=max_size) /*overflow check*/
        {
            LIBCAF_TRACE(LIBCAF_LOG_FATAL,
            "Shared memory size must be less than %lu bytes",max_size);
        }
    }
    else
    {
        caf_shared_memory_size = DEFAULT_SHARED_MEMORY_SIZE;
    }
    ret = ARMCI_Malloc ((void**)coarray_start_all_images,
            caf_shared_memory_size);
    if(ret != 0)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
        "ARMCI_Malloc failed when allocating %lu (%luMB)"
        ,caf_shared_memory_size, caf_shared_memory_size/1000000L );
    }

    static_coarray_size = allocate_static_coarrays();

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
                                                + static_coarray_size;
    common_shared_memory_slot->size = caf_shared_memory_size
                                                - static_coarray_size;
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

/* strided copy from src to dest */
void local_copy_full_strided(void *src, void *dest, unsigned int ndim,
        unsigned long *src_strides, unsigned long *dest_strides,
        unsigned long *extents)
{
    int i,j;
    int num_blks=1;
    void *dest_ptr = dest;
    void *src_ptr = src;
    unsigned long cnt_strides[ndim];
    cnt_strides[0]=1;
    //assuming src_elem_size=dst_elem_size
    unsigned long blk_size = src_strides[0]*extents[0];
    for (i=1; i<ndim; i++)
    {
        cnt_strides[i]=cnt_strides[i-1]*extents[i];
        num_blks *= extents[i];
    }
    for (i=1; i<=num_blks; i++)
    {
        memcpy(dest_ptr, src_ptr, blk_size);
        for (j=1; j<ndim; j++)
            if(i%cnt_strides[j]) break;
        dest_ptr += dest_strides[j];
        src_ptr += src_strides[j];
    }
}

/* copy from src to dest, where src is strided */
void local_copy_src_str(void *src, void *dest, unsigned int ndim,
                unsigned long *src_strides, unsigned long *src_extents)
{
    int i,j;
    unsigned long elem_size = src_strides[0];

    void *dest_ptr;
    unsigned long blk_size;
    unsigned long num_blks;
    unsigned long cnt_strides[MAX_DIMS];
    unsigned int contig_rank;

    /*
     * (1) find the largest contiguous block that we can read from. Given by
     *     contig_rank
     * (2) calculate offset from one contiguous block to the next, along each
     *     of the  dimensions.
     */

    LIBCAF_TRACE(LIBCAF_LOG_CACHE,
        "armci_comm_layer.c:local_copy_src_str-> Local Strided "
        "MemCopy from %p to %p",
        src, dest);

    i = 0;
    contig_rank = 1;
    blk_size = elem_size;
    num_blks = 1;
    cnt_strides[0] = 1;
    for (i = 1; i < ndim; i++) {
        if (src_strides[i]/src_strides[i-1] == src_extents[i-1])  {
            cnt_strides[i] = 1;
            contig_rank++;
            blk_size = src_strides[i];
        } else  {
            cnt_strides[i] = cnt_strides[i-1] * src_extents[i];
            num_blks *= src_extents[i];
        }
    }
    if (blk_size == elem_size)
        blk_size *= src_extents[0];

    void *blk = src;
    dest_ptr = dest;
    for (i = 1; i <= num_blks; i++) {
        memcpy(dest_ptr, blk, blk_size);

        dest_ptr += blk_size;
        for (j = contig_rank; j < ndim; j++) {
            if (i % cnt_strides[j]) break;
        }
        blk += src_strides[j];
    }
}

/* copy from src to dest where dest is strided */
void local_copy_dest_str(void *dest, void *src, unsigned int ndim,
                    unsigned long *dest_strides,
                    unsigned long *dest_extents)
{
    int i,j;
    unsigned long elem_size = dest_strides[0];
    void *src_ptr;
    unsigned long blk_size;
    unsigned long num_blks;
    unsigned long cnt_strides[MAX_DIMS];
    unsigned int contig_rank;


    LIBCAF_TRACE(LIBCAF_LOG_CACHE,
        "armci_comm_layer.c:local_copy_dest_str-> Local Strided "
        "MemCopy from %p to %p",
        src, dest);

    i = 0;
    contig_rank = 1;
    blk_size = elem_size;
    num_blks = 1;
    cnt_strides[0] = 1;
    for (i = 1; i < ndim; i++) {
        if (dest_strides[i]/dest_strides[i-1] == dest_extents[i-1])  {
            cnt_strides[i] = 1;
            contig_rank++;
            blk_size = dest_strides[i];
        } else  {
            cnt_strides[i] = cnt_strides[i-1] * dest_extents[i];
            num_blks *= dest_extents[i];
        }
    }
    if (blk_size == elem_size)
        blk_size *= dest_extents[0];

    void *blk = dest;
    src_ptr = src;
    for (i = 1; i <= num_blks; i++) {
        memcpy(blk, src_ptr, blk_size);

        src_ptr += blk_size;
        for (j = contig_rank; j < ndim; j++) {
            if (i % cnt_strides[j]) break;
        }
        blk += dest_strides[j];
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

static void cache_check_and_get(unsigned long node, void *remote_address,
                            unsigned long xfer_size, void *local_address)
{
    void *cache_address = cache_all_images[node]->remote_address;
    unsigned long start_offset;
    void *cache_line_address = cache_all_images[node]->cache_line_address;
    /* data in cache */
    if(cache_address>0 && remote_address >= cache_address &&
           remote_address+xfer_size <= cache_address+getCache_line_size)
    {
        start_offset=remote_address-cache_address;
        if(cache_all_images[node]->handle)
        {
            ARMCI_Wait(cache_all_images[node]->handle);
            cache_all_images[node]->handle = 0;
        }
        memcpy(local_address, cache_line_address+start_offset, xfer_size);
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
                "armci_comm_layer.c:cache_check_and_get-> Address %p on"
                " image %lu found in cache.", remote_address, node+1);
    }
    else /*data not in cache*/
    {
        /* data NOT from end of shared segment OR bigger than cacheline*/
        if(((remote_address+getCache_line_size) <=
                (coarray_start_all_images[node]+shared_memory_size))
            && (xfer_size <= getCache_line_size))
        {
            ARMCI_Get(remote_address, cache_line_address,
                    getCache_line_size, node);
            cache_all_images[node]->remote_address=remote_address;
            cache_all_images[node]->handle = 0;
            memcpy(local_address, cache_line_address, xfer_size);
        }
        else{
            ARMCI_Get(remote_address, local_address, xfer_size, node);
        }
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
                "armci_comm_layer.c:cache_check_and_get-> Address %p on"
                " image %lu NOT found in cache.", remote_address, node+1);
    }
}

static void cache_check_and_get_src_strided(
                        void *remote_src, void *local_dest,
                        unsigned int ndim, unsigned long *src_strides,
                        unsigned long *src_extents, unsigned long node)
{
    void *cache_address = cache_all_images[node]->remote_address;
    unsigned long start_offset;
    void *cache_line_address =cache_all_images[node]->cache_line_address;
    unsigned long size;
    int i,j;
    unsigned long elem_size = src_strides[0];
    //calculate max size-> stride of last dim* extent of last dim
    size = (src_strides[ndim-1]*(src_extents[ndim-1]-1))
                    +elem_size;
    /* data in cache */
    if(cache_address>0 && remote_src >= cache_address &&
           remote_src+size <= cache_address+getCache_line_size)
    {
        start_offset=remote_src-cache_address;
        if(cache_all_images[node]->handle)
        {
            ARMCI_Wait(cache_all_images[node]->handle);
            cache_all_images[node]->handle = 0;
        }
        local_copy_src_str(cache_line_address+start_offset,
                local_dest, ndim, src_strides, src_extents);
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
                "armci_comm_layer.c:cache_check_and_get_src_strided->"
                "Address %p on image %lu found in cache.",
                remote_src, node+1);
    }
    else /*data not in cache*/
    {
        /* data NOT from end of shared segment OR bigger than cacheline*/
        if(((remote_src+getCache_line_size) <=
                (coarray_start_all_images[node]+shared_memory_size))
            && (size <= getCache_line_size))
        {
            LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "armci_comm_layer.c:cache_check_and_get_src_strided->"
            " Data for Address %p on image %lu NOT found in cache.",
            remote_src, node+1);

            ARMCI_Get(remote_src, cache_line_address, getCache_line_size,
                    node);
            cache_all_images[node]->remote_address=remote_src;
            cache_all_images[node]->handle = 0;
            local_copy_src_str(cache_line_address, local_dest,
                    ndim, src_strides, src_extents);
        }
        else{
            int dst_stride_ar[MAX_DIMS];
            int src_stride_ar[MAX_DIMS];
            int count[MAX_DIMS];

            dst_stride_ar[0] = src_extents[0]*elem_size;
            src_stride_ar[0] = src_strides[1];
            for (i = 1; i < ndim-1; i++) {
                dst_stride_ar[i] = dst_stride_ar[i-1]*src_extents[i];
                src_stride_ar[i] = src_strides[i+1];
                count[i] = src_extents[i];
            }
            count[ndim-1] = src_extents[ndim-1];
            count[0] = src_extents[0]*elem_size;

            LIBCAF_TRACE(LIBCAF_LOG_CACHE,
                "armci_comm_layer.c:cache_check_and_get_src_strided->"
                "Too big for cache");
            ARMCI_GetS(remote_src, src_stride_ar, local_dest,
                    dst_stride_ar, count,  ndim-1, node);
        }
    }
}

static void cache_check_and_get_full_strided(
            void *remote_src, void *local_dest, unsigned int src_ndim,
            unsigned long *src_strides, unsigned long *src_extents,
            unsigned int dest_ndim, unsigned long *dest_strides,
            unsigned long *dest_extents, unsigned long node)
{
    void *cache_address = cache_all_images[node]->remote_address;
    unsigned long start_offset;
    void *cache_line_address =cache_all_images[node]->cache_line_address;
    unsigned long size;
    int i,j;
    unsigned long elem_size = src_strides[0];
    //calculate max size-> stride of last dim* extent of last dim
    size = (src_strides[src_ndim-1]*(src_extents[src_ndim-1]-1))
                    +elem_size;

    /* data in cache */
    if(cache_address>0 && remote_src >= cache_address &&
           remote_src+size <= cache_address+getCache_line_size)
    {
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
                "armci_comm_layer.c:cache_check_and_get_full_strided->"
                "Address %p on image %lu found in cache.",
                remote_src, node+1);
        start_offset=remote_src-cache_address;
        if(cache_all_images[node]->handle)
        {
            ARMCI_Wait(cache_all_images[node]->handle);
            cache_all_images[node]->handle = 0;
        }

        local_copy_full_strided(cache_line_address+start_offset,
                local_dest, src_ndim, src_strides, dest_strides,
                src_extents);

    }
    else /*data not in cache*/
    {
        /* data NOT from end of shared segment OR bigger than cacheline*/
        if(((remote_src+getCache_line_size) <=
            (coarray_start_all_images[node]+shared_memory_size))
            && (size <= getCache_line_size))
        {
            LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "armci_comm_layer.c:cache_check_and_get_src_strided->"
            " Data for Address %p on image %lu NOT found in cache.",
            remote_src, node+1);

            ARMCI_Get(remote_src, cache_line_address, getCache_line_size,
                    node);
            cache_all_images[node]->remote_address=remote_src;
            cache_all_images[node]->handle = 0;

            local_copy_full_strided(cache_line_address,
                local_dest, src_ndim, src_strides, dest_strides,
                src_extents);
        }
        else{
            int src_stride_ar[MAX_DIMS];
            int dest_stride_ar[MAX_DIMS];
            int count[MAX_DIMS];
            for (i = 0; i < dest_ndim-1; i++) {
                src_stride_ar[i] = src_strides[i+1];
                dest_stride_ar[i] = dest_strides[i+1];
                count[i] = dest_extents[i];
                if(dest_extents[i]!=src_extents[i])
                {
                    LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                    "armci_comm_layer.c:comm_read_full_str->src and dst"
                    " extent must be same. src_extents[%d]=%d,"
                    " dest_extents[%d]=%d", i, src_extents[i],i,
                    dest_extents[i]);
                }
            }
            count[src_ndim-1] = src_extents[src_ndim-1];
            count[0] = src_extents[0]*elem_size;

            ARMCI_GetS(remote_src, src_stride_ar, local_dest,
                    dest_stride_ar, count,  src_ndim-1, node );
        }
    }
}

/* Update cache if remote write overlap -- like writethrough cache */
static void update_cache(unsigned long node, void *remote_address,
                        unsigned long xfer_size, void *local_address)
{
    void *cache_address = cache_all_images[node]->remote_address;
    unsigned long start_offset;
    void *cache_line_address = cache_all_images[node]->cache_line_address;
    if(cache_address>0 && remote_address >= cache_address &&
           remote_address+xfer_size <= cache_address+getCache_line_size)
    {
        start_offset = remote_address - cache_address;
        memcpy(cache_line_address+start_offset,local_address, xfer_size);
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
        xfer_size = getCache_line_size - start_offset;
        memcpy(cache_line_address+start_offset,local_address, xfer_size);
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "armci_comm_layer.c:update_cache-> Value of address %p on"
            " image %lu partially updated in cache (write conflict).",
            remote_address, node+1);
    }
    else if (cache_address>0 &&
             remote_address+xfer_size >= cache_address &&
             remote_address+xfer_size<=cache_address+getCache_line_size)
    {
        start_offset = cache_address-remote_address;
        xfer_size = xfer_size - start_offset;
        memcpy(cache_line_address,local_address+start_offset,xfer_size);
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "armci_comm_layer.c:update_cache-> Value of address %p on"
            " image %lu partially updated in cache (write conflict).",
            remote_address, node+1);
    }
}

static void update_cache_full_strided(void * remote_dest_address,
        void *local_src_address, unsigned int dest_ndim,
        unsigned long *dest_strides, unsigned long *dest_extents, 
        unsigned int src_ndim, unsigned long *src_strides,
        unsigned long *src_extents, unsigned long node)
{
    void *cache_address = cache_all_images[node]->remote_address;
    unsigned long start_offset;
    void *cache_line_address =cache_all_images[node]->cache_line_address;
    unsigned long size;

    //calculate max size.. stride of last dim* extent of last dim
    size = (dest_strides[dest_ndim-1]*(dest_extents[dest_ndim-1]-1))
            +dest_strides[0];
    
    //New data completely fit into cache
    if(cache_address>0 && remote_dest_address >= cache_address &&
           remote_dest_address+size <= cache_address+getCache_line_size)
    {
        start_offset = remote_dest_address - cache_address;

        local_copy_full_strided(local_src_address,
                cache_line_address+start_offset, dest_ndim,
                src_strides, dest_strides, src_extents);

        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
        "armci_comm_layer.c:update_cache_full_strided->"
        " Value of address %p on"
        " image %lu updated in cache due to write conflict.",
        remote_dest_address, node+1);
    }
    //Some memory overlap
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

static void update_cache_dest_strided(void * remote_dest_address,
        void *local_src_address, unsigned int dest_ndim,
        unsigned long *dest_strides, unsigned long *dest_extents, 
        unsigned long node)
{
    void *cache_address = cache_all_images[node]->remote_address;
    unsigned long start_offset;
    void *cache_line_address =cache_all_images[node]->cache_line_address;
    unsigned long size;

    //calculate max size.. stride of last dim* extent of last dim
    size = (dest_strides[dest_ndim-1]*(dest_extents[dest_ndim-1]-1))
            +dest_strides[0];
    
    //New data completely fit into cache
    if(cache_address>0 && remote_dest_address >= cache_address &&
           remote_dest_address+size <= cache_address+getCache_line_size)
    {
        start_offset = remote_dest_address - cache_address;

        local_copy_dest_str( cache_line_address+start_offset,
                local_src_address, dest_ndim,
                dest_strides, dest_extents);

        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
        "armci_comm_layer.c:update_cache_dest_strided->"
        " Value of address %p on"
        " image %lu updated in cache due to write conflict.",
        remote_dest_address, node+1);
    }
    //Some memory overlap
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
        unsigned long proc, unsigned long size)
{
    if(min_nbwrite_address[proc]==0)
        return 0;
    if(((remote_addr+size) >= min_nbwrite_address[proc])
      && (remote_addr <= max_nbwrite_address[proc]))
        return 1;
    else
        return 0;
}

static void update_nbwrite_address_block(void *remote_addr,
        unsigned long proc, unsigned long size)
{
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

static void check_wait_on_pending_puts(unsigned long proc, void* remote_address,
                                unsigned long size)
{
    if(address_in_nbwrite_address_block(remote_address, proc, size))
    {
        ARMCI_Fence(proc);
        min_nbwrite_address[proc]=0;
        max_nbwrite_address[proc]=0;
    }
}

static void wait_on_pending_puts(unsigned long proc)
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

/* TO BE DONE:
 * It should allocate memory to all static coarrays from the pinned-down
 * memory created during init */
unsigned long allocate_static_coarrays()
{
    unsigned long static_coarray_size;
    static_coarray_size = 0L;
    return static_coarray_size;
}

/* Calculate the address on another image corresponding to a local address
 * This is possible as all images must have the same coarrays, i.e the
 * memory is symmetric. Since we know the start address of all images
 * from coarray_start_all_images, remote_address = start+offset
 * NOTE: remote_address on this image might be different from the actual
 * address on that image. So don't rely on it while debugging*/
static void *get_remote_address(void *src, unsigned long img)
{
    unsigned long offset;
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
    ARMCI_Error("ARMCI error",status);
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

void comm_read(void *src, void *dest, unsigned long xfer_size,
                unsigned long proc)
{
    void *remote_src;
    remote_src = get_remote_address(src,proc);
    if(enable_nbput)
    {
        check_wait_on_pending_puts(proc, remote_src, xfer_size);
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "armci_comm_layer.c:comm_read->Finished waiting for"
        " pending puts on %p on image %lu. Min:%p, Max:%p", remote_src,
        proc+1, min_nbwrite_address[proc], max_nbwrite_address[proc] );
    }
    if(enable_get_cache)
        cache_check_and_get(proc, remote_src, xfer_size, dest);
    else{
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_read->Before ARMCI_Get from %p on"
            " image %lu to %p size %lu",
            remote_src, proc+1, dest, xfer_size);
        ARMCI_Get(remote_src, dest, xfer_size, proc);
    }
}

void comm_write(void *dest, void *src, unsigned long xfer_size,
                unsigned long proc)
{
    void *remote_dest;
    remote_dest = get_remote_address(dest,proc);
    if(enable_nbput)
    {
        armci_hdl_t handle;
        ARMCI_INIT_HANDLE(&handle);
        check_wait_on_pending_puts(proc, remote_dest, xfer_size);
        ARMCI_NbPut(src, remote_dest, xfer_size, proc, &handle);
        ARMCI_Wait(&handle); // This ensures local completion only
        update_nbwrite_address_block(remote_dest, proc, xfer_size);
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
                remote_dest, proc+1, src, xfer_size);
        ARMCI_Put(src, remote_dest, xfer_size, proc);
    }
    if(enable_get_cache)
        update_cache(proc, remote_dest, xfer_size, src);
}

void comm_read_src_str(void *src, void *dest, unsigned int ndim,
                unsigned long *src_strides, unsigned long *src_extents,
                unsigned long proc)
{
    int i,j;
    unsigned long elem_size = src_strides[0];
    void *remote_src;

#if defined(ENABLE_LOCAL_MEMCPY)
    if (  my_proc == proc  ) { /* local read */
        void *dest_ptr;
        unsigned long blk_size;
        unsigned long num_blks;
        unsigned long cnt_strides[MAX_DIMS];
        unsigned int contig_rank;

        /*
         * (1) find the largest contiguous block that we can read from. Given by
         *     contig_rank
         * (2) calculate offset from one contiguous block to the next, along each
         *     of the  dimensions.
         */

        i = 0;
        contig_rank = 1;
        blk_size = elem_size;
        num_blks = 1;
        cnt_strides[0] = 1;
        for (i = 1; i < ndim; i++) {
            if (src_strides[i]/src_strides[i-1] == src_extents[i-1])  {
                cnt_strides[i] = 1;
                contig_rank++;
                blk_size = src_strides[i];
            } else  {
                cnt_strides[i] = cnt_strides[i-1] * src_extents[i];
                num_blks *= src_extents[i];
            }
        }
        if (blk_size == elem_size)
            blk_size *= src_extents[0];

        void *blk = src;
        dest_ptr = dest;
        for (i = 1; i <= num_blks; i++) {
            if (dest_ptr < blk || dest_ptr > (blk+blk_size))
                memcpy(dest_ptr, blk, blk_size);
            else /* use memmove for overlapping sections */
                memmove(dest_ptr, blk, blk_size);

            dest_ptr += blk_size;
            for (j = contig_rank; j < ndim; j++) {
                if (i % cnt_strides[j]) break;
            }
            blk += src_strides[j];
        }
    } else
#endif
    {
        remote_src = get_remote_address(src,proc);
        if(enable_nbput)
        {
            unsigned long size;
            //calculate maxsize->stride of last dim* extent of last dim
            size =(src_strides[ndim-1]*(src_extents[ndim-1]-1))
                +elem_size;
            check_wait_on_pending_puts(proc, remote_src, size);
            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_read_src_str->Finished waiting for"
            " pending puts on %p on image %lu. Min:%p,Max:%p",remote_src,
            proc+1, min_nbwrite_address[proc],max_nbwrite_address[proc] );
        }
        if(enable_get_cache)
        {
            cache_check_and_get_src_strided(remote_src, dest,
                    ndim, src_strides, src_extents, proc);
        }
        else
        {
            int dst_stride_ar[MAX_DIMS];
            int src_stride_ar[MAX_DIMS];
            int count[MAX_DIMS];

            dst_stride_ar[0] = src_extents[0]*elem_size;
            src_stride_ar[0] = src_strides[1];
            for (i = 1; i < ndim-1; i++) {
                dst_stride_ar[i] = dst_stride_ar[i-1]*src_extents[i];
                src_stride_ar[i] = src_strides[i+1];
                count[i] = src_extents[i];
            }
            count[ndim-1] = src_extents[ndim-1];
            count[0] = src_extents[0]*elem_size;

            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                "armci_comm_layer.c:comm_read_src_str->Before ARMCI_GetS"
                " from %p on image %lu to %p ndim %lu",
                remote_src, proc+1, dest, ndim-1);

            ARMCI_GetS(remote_src, src_stride_ar, dest, dst_stride_ar,
                             count,  ndim-1, proc);
        }
    }
}

void comm_read_src_str2(void *src, void *dest, unsigned int ndim,
                    unsigned long *src_str_mults, unsigned long *src_extents,
                    unsigned long *src_strides,
                    unsigned long proc)
{
    int i,j,k;
    unsigned long elem_size = src_str_mults[0];
    void *remote_src;

    remote_src = get_remote_address(src,proc);

    int dst_stride_ar[MAX_DIMS];
    int src_stride_ar[MAX_DIMS];
    int count[MAX_DIMS];

    j = k = 0;
    for (i = 0; i < ndim; i++) {
      if (i > 0) {
        src_stride_ar[k++] = src_str_mults[i];
      }

      if (src_strides[i] > 1) {
        src_stride_ar[k++] = src_str_mults[i]*src_strides[i];
        count[j++] = 1;
      }   
      count[j++] = src_extents[i];
    }
    count[0] = count[0] * elem_size;
    dst_stride_ar[0] = count[0];
    for (i = 1; i < j-1; i++) {
      dst_stride_ar[i] = dst_stride_ar[i-1]*count[i];
    }

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "armci_comm_layer.c:comm_read_src_str->Before ARMCI_GetS"
        " from %p on image %lu to %p ndim %lu",
        remote_src, proc+1, dest, ndim-1);

    ARMCI_GetS(remote_src, src_stride_ar, dest, dst_stride_ar,
                     count,  j-1, proc);
}

void comm_read_full_str (void * src, void *dest, unsigned int src_ndim,
        unsigned long *src_strides, unsigned long *src_extents, 
        unsigned int dest_ndim, unsigned long *dest_strides,
        unsigned long *dest_extents, unsigned long proc)
{
    int i,j;
    unsigned long elem_size = src_strides[0];
    void *remote_src;
    int src_stride_ar[MAX_DIMS];
    int dest_stride_ar[MAX_DIMS];
    int count[MAX_DIMS];

    if(src_ndim!=dest_ndim)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
        "armci_comm_layer.c:comm_read_full_str->src and dest dim must"
        " be same. src_ndim=%d, dest_ndim=%d",
        src_ndim, dest_ndim);
    }
    remote_src = get_remote_address(src,proc);
    if(enable_nbput)
    {
        unsigned long size;
        //calculate max size.. stride of last dim* extent of last dim
        size =(src_strides[src_ndim-1]*(src_extents[src_ndim-1]-1))
                +elem_size;
        check_wait_on_pending_puts(proc, remote_src, size);
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "armci_comm_layer.c:comm_read_full_str->Finished waiting for"
        " pending puts on %p on image %lu. Min:%p, Max:%p", remote_src,
        proc+1, min_nbwrite_address[proc], max_nbwrite_address[proc] );
    }

    if(enable_get_cache)
    {
        cache_check_and_get_full_strided(remote_src, dest,
                src_ndim, src_strides, src_extents,
                dest_ndim, dest_strides, dest_extents,
                proc);
    }
    else
    {
        for (i = 0; i < dest_ndim-1; i++) {
            src_stride_ar[i] = src_strides[i+1];
            dest_stride_ar[i] = dest_strides[i+1];
            count[i] = dest_extents[i];
            if(dest_extents[i]!=src_extents[i])
            {
                LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                "armci_comm_layer.c:comm_read_full_str->src and dest"
                " extent must be same. src_extents[%d]=%d,"
                " dest_extents[%d]=%d",
                i, src_extents[i],i, dest_extents[i]);
            }
        }
        count[src_ndim-1] = src_extents[src_ndim-1];
        count[0] = src_extents[0]*elem_size;

        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_read_full_str->Before ARMCI_GetS"
            " from %p on image %lu to %p ndim %lu",
            remote_src, proc+1, dest, src_ndim-1);

        ARMCI_GetS(remote_src, src_stride_ar, dest, dest_stride_ar,
                         count,  src_ndim-1, proc );
    }
}

void comm_read_full_str2 (void * src, void *dest, unsigned int src_ndim,
        unsigned long *src_str_mults, unsigned long *src_extents, 
        unsigned long *src_strides,
        unsigned int dest_ndim, unsigned long *dest_str_mults,
        unsigned long *dest_extents, unsigned long *dest_strides,
        unsigned long proc)
{
    int i,j,k1,k2;
    unsigned long elem_size = src_str_mults[0];
    void *remote_src;
    int src_stride_ar[MAX_DIMS];
    int dest_stride_ar[MAX_DIMS];
    int count[MAX_DIMS];

    if(src_ndim!=dest_ndim)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
        "armci_comm_layer.c:comm_read_full_str->src and dest dim must"
        " be same. src_ndim=%d, dest_ndim=%d",
        src_ndim, dest_ndim);
    }
    remote_src = get_remote_address(src,proc);

    j = k1 = k2 = 0;
    for (i = 0; i < src_ndim; i++) {
      if (i > 0) {
        src_stride_ar[k1++] = src_str_mults[i];
        dest_stride_ar[k2++] = dest_str_mults[i];
      }

      if ((src_strides && src_strides[i]>1) &&
          (dest_strides == NULL || dest_strides[i]==1)) {
        src_stride_ar[k1++] = src_str_mults[i]*src_strides[i];
        if (k2 == 0)
          dest_stride_ar[k2++] = dest_str_mults[0];
        else
          dest_stride_ar[k2++] = dest_stride_ar[k2-1];
        count[j++] = 1;
      } else if ((dest_strides && dest_strides[i]>1) &&
          (src_strides == NULL || src_strides[i]==1)) {
        dest_stride_ar[k2++] = dest_str_mults[i]*dest_strides[i];
        if (k1 == 0)
          src_stride_ar[k1++] = src_str_mults[0];
        else
          src_stride_ar[k1++] = src_stride_ar[k1-1];
        count[j++] = 1;
      } else if (dest_strides && src_strides &&
          dest_strides[i]>1 && src_strides[i]>1) {
        src_stride_ar[k1++] = src_str_mults[i]*src_strides[i];
        dest_stride_ar[k2++] = dest_str_mults[i]*dest_strides[i];
        count[j++] = 1;
      }  
      count[j++] = src_extents[i];
    }
    count[0] = count[0] * elem_size;

    debug_print_array_int("src_stride_ar", src_stride_ar, k1);
    debug_print_array_int("dest_stride_ar", dest_stride_ar, k2);
    debug_print_array_int("count", count, j);


    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "armci_comm_layer.c:comm_read_full_str->Before ARMCI_GetS"
        " from %p on image %lu to %p ndim %lu",
        remote_src, proc+1, dest, src_ndim-1);

    ARMCI_GetS(remote_src, src_stride_ar, dest, dest_stride_ar,
                     count,  j-1, proc );
}

void comm_write_dest_str(void *dest, void *src, unsigned int ndim,
                unsigned long *dest_strides, unsigned long *dest_extents,
                unsigned long proc)
{
    int i,j;
    unsigned long elem_size = dest_strides[0];
    void *remote_dest;

#if defined(ENABLE_LOCAL_MEMCPY)
    if (  my_proc == proc  ) { /* local write */
        void *src_ptr;
        unsigned long blk_size;
        unsigned long num_blks;
        unsigned long cnt_strides[MAX_DIMS];
        unsigned int contig_rank;


        i = 0;
        contig_rank = 1;
        blk_size = elem_size;
        num_blks = 1;
        cnt_strides[0] = 1;
        for (i = 1; i < ndim; i++) {
            if(dest_strides[i]/dest_strides[i-1] == dest_extents[i-1])
            {
                cnt_strides[i] = 1;
                contig_rank++;
                blk_size = dest_strides[i];
            } else  {
                cnt_strides[i] = cnt_strides[i-1] * dest_extents[i];
                num_blks *= dest_extents[i];
            }
        }
        if (blk_size == elem_size)
            blk_size *= dest_extents[0];

        void *blk = dest;
        src_ptr = src;
        for (i = 1; i <= num_blks; i++) {
            if (src_ptr < blk || src_ptr > (blk+blk_size))
                memcpy(blk, src_ptr, blk_size);
            else /* use memmove for overlapping sections */
                memmove(blk, src_ptr, blk_size);

            src_ptr += blk_size;
            for (j = contig_rank; j < ndim; j++) {
                if (i % cnt_strides[j]) break;
            }
            blk += dest_strides[j];
        }
    } else
#endif
    {
        int src_stride_ar[MAX_DIMS];
        int dest_stride_ar[MAX_DIMS];
        int count[MAX_DIMS];

        src_stride_ar[0] = dest_extents[0]*elem_size;
        dest_stride_ar[0] = dest_strides[1];
        for (i = 1; i < ndim-1; i++) {
            src_stride_ar[i] = src_stride_ar[i-1]*dest_extents[i];
            dest_stride_ar[i] = dest_strides[i+1];
            count[i] = dest_extents[i];
        }
        count[ndim-1] = dest_extents[ndim-1];
        count[0] = dest_extents[0]*elem_size;

        remote_dest = get_remote_address(dest,proc);

        if(enable_nbput)
        {
            armci_hdl_t handle;
            ARMCI_INIT_HANDLE(&handle);
            unsigned long size;
            //calculate max size.. stride of last dim*extent of last dim
            size =(dest_strides[ndim-1]*(dest_extents[ndim-1]-1))
                    +elem_size;
            check_wait_on_pending_puts(proc, remote_dest, size);
            ARMCI_NbPutS(src, src_stride_ar, remote_dest, dest_stride_ar,
                             count,  ndim-1, proc, &handle );
            ARMCI_Wait(&handle); // This ensures local completion only
            update_nbwrite_address_block(remote_dest, proc, size);
            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_write_dest_str->After ARMCI_NbPutS"
            " to %p on image %lu. Min:%p, Max:%p", remote_dest, proc+1,
            min_nbwrite_address[proc], max_nbwrite_address[proc] );
        }
        else
        {
            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_write_dest_str->Before ARMCI_PutS"
            " to %p on image %lu from %p ndim %lu",
            remote_dest, proc+1, src, ndim-1);
            ARMCI_PutS(src, src_stride_ar, remote_dest, dest_stride_ar,
                         count,  ndim-1, proc );
        }
        if(enable_get_cache)
        {
            update_cache_dest_strided(remote_dest, src, ndim,
                    dest_strides, dest_extents, proc);
        }
    }
}

void comm_write_dest_str2(void *dest, void *src, unsigned int ndim,
                    unsigned long *dest_str_mults,
                    unsigned long *dest_extents,
                    unsigned long *dest_strides,
                    unsigned long proc)
{
    int i,j,k;
    unsigned long elem_size = dest_str_mults[0];
    void *remote_dest;

    int src_stride_ar[MAX_DIMS];
    int dest_stride_ar[MAX_DIMS];
    int count[MAX_DIMS];

    j = k = 0;
    for (i = 0; i < ndim; i++) {
      if (i > 0) {
        dest_stride_ar[k++] = dest_str_mults[i];
      }

      if (dest_strides[i] > 1) {
        dest_stride_ar[k++] = dest_str_mults[i]*dest_strides[i];
        count[j++] = 1;
      }   
      count[j++] = dest_extents[i];
    }
    count[0] = count[0] * elem_size;
    src_stride_ar[0] = count[0];
    for (i = 1; i < j-1; i++) {
      src_stride_ar[i] = src_stride_ar[i-1]*count[i];
    }


    remote_dest = get_remote_address(dest,proc);

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
    "armci_comm_layer.c:comm_write_dest_str->Before ARMCI_PutS"
    " to %p on image %lu from %p ndim %lu",
    remote_dest, proc+1, src, ndim-1);
    ARMCI_PutS(src, src_stride_ar, remote_dest, dest_stride_ar,
                 count,  j-1, proc );
}


void comm_write_full_str (void * dest, void *src, unsigned int dest_ndim,
        unsigned long *dest_strides, unsigned long *dest_extents, 
        unsigned int src_ndim, unsigned long *src_strides,
        unsigned long *src_extents, unsigned long proc)
{
    int i,j;
    unsigned long elem_size = dest_strides[0];
    void *remote_dest;
    int src_stride_ar[MAX_DIMS];
    int dest_stride_ar[MAX_DIMS];
    int count[MAX_DIMS];

    if(src_ndim!=dest_ndim)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
        "armci_comm_layer.c:comm_write_full_str->src and dest dim must"
        " be same. src_ndim=%d, dest_ndim=%d",
        src_ndim, dest_ndim);
    }

    for (i = 0; i < dest_ndim-1; i++) {
        src_stride_ar[i] = src_strides[i+1];
        dest_stride_ar[i] = dest_strides[i+1];
        count[i] = dest_extents[i];
        if(dest_extents[i]!=src_extents[i])
        {
            LIBCAF_TRACE(LIBCAF_LOG_FATAL,
            "armci_comm_layer.c:comm_write_full_str->src and dest extent"
            " must be same. src_extents[%d]=%d, dest_extents[%d]=%d",
            i, src_extents[i],i, dest_extents[i]);
        }
    }
    count[dest_ndim-1] = dest_extents[dest_ndim-1];
    count[0] = dest_extents[0]*elem_size;

    remote_dest = get_remote_address(dest,proc);

    if(enable_nbput)
    {
        armci_hdl_t handle;
        ARMCI_INIT_HANDLE(&handle);
        unsigned long size;
        //calculate max size.. stride of last dimention * extent of last dim
        size = (dest_strides[dest_ndim-1]*(dest_extents[dest_ndim-1]-1))+elem_size;
        check_wait_on_pending_puts(proc, remote_dest, size);
        ARMCI_NbPutS(src, src_stride_ar, remote_dest, dest_stride_ar,
                         count,  dest_ndim-1, proc, &handle );
        ARMCI_Wait(&handle); // This ensures local completion only
        update_nbwrite_address_block(remote_dest, proc, size);
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_write_full_str->After ARMCI_NbPutS"
            " to %p on image %lu. Min:%p, Max:%p", remote_dest, proc+1,
            min_nbwrite_address[proc], max_nbwrite_address[proc] );
    }
    else
    {
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "armci_comm_layer.c:comm_write_full_str->Before ARMCI_PutS"
        " to %p on image %lu from %p ndim %lu",
        remote_dest, proc+1, src, dest_ndim-1);
        ARMCI_PutS(src, src_stride_ar, remote_dest, dest_stride_ar,
                     count,  dest_ndim-1, proc );
    }
    if(enable_get_cache)
    {
        update_cache_full_strided(remote_dest, src, dest_ndim,
                dest_strides, dest_extents, src_ndim, src_strides,
                src_extents, proc);
    }
}

void comm_write_full_str2 (void * dest, void *src, unsigned int dest_ndim,
        unsigned long *dest_str_mults, unsigned long *dest_extents, 
        unsigned long *dest_strides,
        unsigned int src_ndim, unsigned long *src_str_mults,
        unsigned long *src_extents, unsigned long *src_strides,
        unsigned long proc)
{
    int i,j,k1,k2;
    unsigned long elem_size = dest_str_mults[0];
    void *remote_dest;
    int src_stride_ar[MAX_DIMS];
    int dest_stride_ar[MAX_DIMS];
    int count[MAX_DIMS];

    if(src_ndim!=dest_ndim)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
        "armci_comm_layer.c:comm_write_full_str->src and dest dim must"
        " be same. src_ndim=%d, dest_ndim=%d",
        src_ndim, dest_ndim);
    }

    j = k1 = k2 = 0;
    for (i = 0; i < dest_ndim; i++) {
      if (i > 0) {
        src_stride_ar[k1++] = src_str_mults[i];
        dest_stride_ar[k2++] = dest_str_mults[i];
      }

      if ((src_strides && src_strides[i]>1) &&
          (dest_strides == NULL || dest_strides[i]==1)) {
        src_stride_ar[k1++] = src_str_mults[i]*src_strides[i];
        if (k2 == 0)
          dest_stride_ar[k2++] = dest_str_mults[0];
        else
          dest_stride_ar[k2++] = dest_stride_ar[k2-1];
        count[j++] = 1;
      } else if ((dest_strides && dest_strides[i]>1) &&
          (src_strides == NULL || src_strides[i]==1)) {
        dest_stride_ar[k2++] = dest_str_mults[i]*dest_strides[i];
        if (k1 == 0)
          src_stride_ar[k1++] = src_str_mults[0];
        else
          src_stride_ar[k1++] = src_stride_ar[k1-1];
        count[j++] = 1;
      } else if (dest_strides && src_strides &&
          dest_strides[i]>1 && src_strides[i]>1) {
        src_stride_ar[k1++] = src_str_mults[i]*src_strides[i];
        dest_stride_ar[k2++] = dest_str_mults[i]*dest_strides[i];
        count[j++] = 1;
      }  
      count[j++] = dest_extents[i];
    }
    count[0] = count[0] * elem_size;


    remote_dest = get_remote_address(dest,proc);

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
    "armci_comm_layer.c:comm_write_full_str->Before ARMCI_PutS"
    " to %p on image %lu from %p ndim %lu",
    remote_dest, proc+1, src, dest_ndim-1);
    ARMCI_PutS(src, src_stride_ar, remote_dest, dest_stride_ar,
                 count,  j-1, proc );
}
