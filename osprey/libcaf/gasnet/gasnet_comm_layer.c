/*
 GASNet Communication runtime library to be used with OpenUH

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

/* #defines are in the header file */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "gasnet_comm_layer.h"
#include "trace.h"
#include "caf_rtl.h"
#include "util.h"


/* common_slot is a node in the shared memory link-list that keeps track
 * of available memory that can used for both allocatable coarrays and
 * asymmetric data. It is the only handle to access the link-list.*/
extern struct shared_memory_slot *common_slot;


/*
 * Static variable declarations
 */

static unsigned long my_proc;
static unsigned long num_procs;

static unsigned short gasnet_everything = 0; /* flag */

/* Global barrier */
static unsigned int barcount = 0;
static unsigned int barflag = 0; // GASNET_BARRIERFLAG_ANONYMOUS

/* Shared memory management
 * coarray_start_all_images stores the shared memory start address and
 * size of all images.
 * gasnet_seginfo_t is a struct defined in gasnet.h:
 * typedef struct {
 *      void *addr;
 *      uintptr_t size;
 * }gasnet_seginfo_t
 */
static gasnet_seginfo_t *coarray_start_all_images;
/* For everything config, coarray_start_all_images has to be populated
 * by remote gets. But we can not pass the address of
 * coarray_start_all_images[img]->addr to gasnet_get as it is on heap.
 * Only static variables have same address across images. So we use a
 * static variable everything_allocatable_start to store
 * coarray_start_all_images[my_proc]->addr */
static void * everything_allocatable_start;

/*sync images*/
static unsigned short *sync_images_flag;
static gasnet_hsl_t sync_lock = GASNET_HSL_INITIALIZER;

/*non-blocking put*/
static int enable_nbput; /* 0-disabled, set by env var UHCAF_NBPUT */
static struct write_handle_list **write_handles;
static void **min_nbwrite_address;
static void **max_nbwrite_address;
static struct local_buffer *lcb_list;
static struct local_buffer *lcb_tail;

/* get cache */
static int enable_get_cache; /* set by env variable */
static unsigned long getCache_line_size; /* set by env var. */
/* Instead of making the cache_all_image an array of struct cache, I
 * make it an array of pointer to struct cache. This will make it easy
 * to add more cache lines in the future by making it 2D array */
static struct cache **cache_all_images;
static unsigned long shared_memory_size;

/* mutex for critical sections */
gasnet_hsl_t cs_mutex = GASNET_HSL_INITIALIZER;
static int cs_done=0;

/* forward declarations */

static inline int address_on_heap(void *addr);

static void handler_sync_request(gasnet_token_t token, int imageIndex);

static void handler_critical_request(gasnet_token_t token,
                         gasnet_handlerarg_t src_proc);

static void handler_critical_reply(gasnet_token_t token,
                         gasnet_handlerarg_t mutex_id);

static void handler_end_critical_request(gasnet_token_t token,
                           gasnet_handlerarg_t src_proc);


static void *get_remote_address(void *src, size_t img);
static int address_in_nbwrite_address_block(void *remote_addr,
        size_t proc, size_t size);
static void update_nbwrite_address_block(void *remote_addr,
        size_t proc, size_t size);
static struct write_handle_list* get_next_handle(unsigned long proc,
        void* remote_address, unsigned long size);
static void reset_min_nbwrite_address(unsigned long proc);
static void reset_max_nbwrite_address(unsigned long proc);
static void delete_node(unsigned long proc, struct write_handle_list *node);
static int address_in_handle(struct write_handle_list *handle_node,
                                void *address, unsigned long size);
static void wait_on_pending_puts(unsigned long proc, void* remote_address,
                                unsigned long size);
static void wait_on_all_pending_puts(unsigned long proc);

static void clear_all_cache();
static void clear_cache(unsigned long node);
static void cache_check_and_get(size_t node, void *remote_address,
                            size_t nbytes, void *local_address);
static void update_cache(size_t node, void *remote_address,
                        size_t nbytes, void *local_address);

static void local_strided_copy(void *src, const size_t src_strides[],
        void *dest, const size_t dest_strides[],
        const size_t count[], size_t stride_levels);


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

    if (gasnet_everything)
        return 1;

    start_heap = coarray_start_all_images[my_proc].addr;
    end_heap = common_slot->addr;

    return (addr >= start_heap && addr <= end_heap);
}


/*************************************************************/
/* start of handlers */

static gasnet_handlerentry_t handlers[] =
  {
    { GASNET_HANDLER_SYNC_REQUEST, handler_sync_request },
    { GASNET_HANDLER_CRITICAL_REQUEST, handler_critical_request },
    { GASNET_HANDLER_CRITICAL_REPLY, handler_critical_reply },
    { GASNET_HANDLER_END_CRITICAL_REQUEST, handler_end_critical_request }
  };

static const int nhandlers = sizeof(handlers) / sizeof(handlers[0]);

/* handler funtion for  sync images */
static void handler_sync_request(gasnet_token_t token, int imageIndex)
{
    gasnet_hsl_lock( &sync_lock );
    sync_images_flag[imageIndex]++;
    gasnet_hsl_unlock( &sync_lock );
}


/* handlers for critical section access */
static void handler_critical_request(gasnet_token_t token,
                         gasnet_handlerarg_t src_proc)
{
  LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
      "inside handler_critical_request (%d,%d)\n", my_proc, (int)src_proc);

  GASNET_BLOCKUNTIL(gasnet_hsl_trylock(&cs_mutex) == GASNET_OK);
  //gasnet_hsl_lock(&cs_mutex);

  LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
      "inside handler_critical_request (%d,%d) ... done\n", my_proc, (int)src_proc);

  GASNET_Safe(gasnet_AMReplyShort0(token,GASNET_HANDLER_CRITICAL_REPLY));
}

static void handler_critical_reply(gasnet_token_t token, gasnet_handlerarg_t mutex_id)
{
  LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
      "inside handler_critical_reply (%d)\n", my_proc);

  cs_done = 1;

  LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
      "inside handler_critical_reply (%d) ... done\n", my_proc);
}

static void handler_end_critical_request(gasnet_token_t token,
                           gasnet_handlerarg_t src_proc)
{
  LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
      "inside handler_end_critical_request (%d,%d)\n", my_proc, (int)src_proc);

  gasnet_hsl_unlock(&cs_mutex);

  LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
      "inside handler_end_critical_request (%d) ... done\n", my_proc);
}

/* end of handlers */
/*************************************************************/

/*
 * INIT:
 * 1) Initialize GASNet
 * 2) Allocate memory and initialize data-structures used for sync and
 *    non-blocking puts.
 * 3) Create pinned memory and populate coarray_start_all_images(except
 *    for EVERYTHING config.
 * 4) Populates common_shared_memory_slot with the pinned memory data
 *    which is used by caf_rtl.c to allocate/deallocate coarrays.
 */
void comm_init(struct shared_memory_slot *common_shared_memory_slot)
{
    int ret,i;
    int   argc = 1;
    char  **argv;
    char *caf_shared_memory_size_str;
    char *enable_get_cache_str, *getCache_line_size_str;
    char *enable_nbput_str;
    unsigned long caf_shared_memory_size;
    unsigned long caf_shared_memory_pages;
    unsigned long static_coarray_size;
    unsigned long max_size=powl(2,(sizeof(unsigned long)*8))-1;

    farg_init(&argc, &argv);
    ret = gasnet_init(&argc, &argv);

    if (ret != GASNET_OK) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL, "GASNet init error");
    }

    uintptr_t max_local = gasnet_getMaxLocalSegmentSize();
    if(max_local == -1)
        gasnet_everything = 1;

    my_proc = gasnet_mynode();
    num_procs = gasnet_nodes();

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

    /* malloc dataStructures for sync_images, nb-put, get-cache */
    sync_images_flag = (unsigned short *)malloc
        (num_procs*sizeof(unsigned short));
    if (enable_nbput)
    {
        write_handles= (struct write_handle_list **)malloc
            (num_procs*sizeof(struct write_handle_list *));
        min_nbwrite_address= malloc(num_procs*sizeof(void*));
        max_nbwrite_address= malloc(num_procs*sizeof(void*));
    }

    if (enable_get_cache)
    {
        cache_all_images =
            (struct cache **)malloc(num_procs* sizeof(struct cache *));
    }


    /* initialize data structures to 0 */
    for(i=0; i<num_procs; i++){
        sync_images_flag[i]=0;

        if(enable_nbput)
        {
            write_handles[i]=0;
            min_nbwrite_address[i]=0;
            max_nbwrite_address[i]=0;
            lcb_list = 0;
            lcb_tail = 0;
        }
        if(enable_get_cache)
        {
            cache_all_images[i]=(struct cache *)malloc(sizeof(struct cache));
            cache_all_images[i]->remote_address=0;
            cache_all_images[i]->handle=0;
            cache_all_images[i]->cache_line_address=malloc(getCache_line_size);
        }
    }

    /* create pinned-down/registered memory  and populate
     * coarray_start_all_images */
    coarray_start_all_images = (gasnet_seginfo_t *)malloc
        (num_procs*sizeof(gasnet_seginfo_t));
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

    /* pinned-down memory must be PAGESIZE alligned */
    caf_shared_memory_pages = caf_shared_memory_size/GASNET_PAGESIZE;
    if ( caf_shared_memory_size%GASNET_PAGESIZE )
        caf_shared_memory_pages++;

    if (!gasnet_everything)
    {
        /* check if so much memory can be pinned */
        if ( max_local < (caf_shared_memory_pages*GASNET_PAGESIZE))
            LIBCAF_TRACE(LIBCAF_LOG_FATAL,
            "GASNET cannot attach %lubytes. Maximum memory that can be "
            "attached on this image(%lu) = %lu", caf_shared_memory_size,
            my_proc+1, (unsigned long)max_local);
    }

    /* gasnet everything ignores the last 2 params
     * note that attach is a collective operation */
    ret = gasnet_attach(handlers, nhandlers,
           caf_shared_memory_pages*GASNET_PAGESIZE,0);
    if (ret != GASNET_OK)
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,"GASNET attach failed");

    /* Get start address from all images
     * For gasnet_everything,  it get initialized to null,
     * I don't fetch it now, do when required first
     * time during get_remote_address(lazy init).*/
    ret = gasnet_getSegmentInfo(coarray_start_all_images,num_procs);
    if (ret != GASNET_OK)
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,"GASNET getSegmentInfo failed");


    /* Do a simple malloc for EVERYTHING, as attach did not do it */
    if (gasnet_everything)
    {
        coarray_start_all_images[my_proc].addr =
            malloc (caf_shared_memory_pages*GASNET_PAGESIZE);
        coarray_start_all_images[my_proc].size =
            caf_shared_memory_pages*GASNET_PAGESIZE;
    }

    if(gasnet_everything)
    {
        static_coarray_size = 0;
        everything_allocatable_start =
            coarray_start_all_images[my_proc].addr;
    }
    else
        static_coarray_size = allocate_static_coarrays();

    /* initialize common shared memory slot */
    common_shared_memory_slot->addr =
        coarray_start_all_images[my_proc].addr + static_coarray_size;
    common_shared_memory_slot->size =
        caf_shared_memory_size - static_coarray_size;
    common_shared_memory_slot->feb = 0;
    common_shared_memory_slot->next =0;
    common_shared_memory_slot->prev =0;

    shared_memory_size = caf_shared_memory_size;

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"gasnet_comm_layer.c:comm_init-> Img%lu"
        "Finished. Waiting for global barrier. Gasnet_Everything is %d. "
        "common_slot->addr=%p, common_slot->size=%lu",
        my_proc+1, gasnet_everything, common_shared_memory_slot->addr,
        common_shared_memory_slot->size);

    comm_barrier_all();/* barrier */
}

/*
 * End Init
 */

/*
 * Static Functions for GET CACHE
 */

/* naive implementation of strided copy
 * TODO: improve by finding maximal blocksize
 */
static void local_strided_copy(void *src, const size_t src_strides[],
        void *dest, const size_t dest_strides[],
        const size_t count[], size_t stride_levels)
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
            cache_all_images[i]->handle = gasnet_get_nb_bulk (
                cache_all_images[i]->cache_line_address, i,
                cache_all_images[i]->remote_address, getCache_line_size );
        }
    }
    LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "gasnet_comm_layer.c:refetch_all_cache-> Finished nb get");
}

static void refetch_cache(size_t node)
{
    if(cache_all_images[node]->remote_address)
    {
        cache_all_images[node]->handle = gasnet_get_nb_bulk (
            cache_all_images[node]->cache_line_address, node,
            cache_all_images[node]->remote_address, getCache_line_size );
    }
    LIBCAF_TRACE(LIBCAF_LOG_CACHE,
    "gasnet_comm_layer.c:refetch_cache-> Finished nb get from image %lu",
            node+1);
}

static void cache_check_and_get(size_t node, void *remote_address,
                            size_t nbytes, void *local_address)
{
    void *cache_address = cache_all_images[node]->remote_address;
    size_t start_offset;
    void *cache_line_address =cache_all_images[node]->cache_line_address;
    /* data in cache */
    if(cache_address>0 && remote_address >= cache_address &&
           remote_address+nbytes <= cache_address+getCache_line_size)
    {
        start_offset=remote_address-cache_address;
        if(cache_all_images[node]->handle)
        {
            gasnet_wait_syncnb(cache_all_images[node]->handle);
            cache_all_images[node]->handle = 0;
        }
        memcpy(local_address,cache_line_address+start_offset, nbytes);
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "gasnet_comm_layer.c:cache_check_and_get-> Address %p on"
            " image %lu found in cache.", remote_address, node+1);
    }
    else /*data not in cache*/
    {
        /* data NOT from end of shared segment OR bigger than cacheline*/
        if(((remote_address+getCache_line_size) <=
                (coarray_start_all_images[node].addr+shared_memory_size))
            && (nbytes <= getCache_line_size))
        {
            gasnet_get(cache_line_address, node, remote_address,
                        getCache_line_size);
            cache_all_images[node]->remote_address=remote_address;
            cache_all_images[node]->handle = 0;
            memcpy(local_address, cache_line_address, nbytes);
        }
        else{
            gasnet_get( local_address, node, remote_address, nbytes);
        }
        LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "gasnet_comm_layer.c:cache_check_and_get-> Address %p on"
            " image %lu NOT found in cache.", remote_address, node+1);
    }
}



static void cache_check_and_get_strided(
        void *remote_src, const size_t src_strides[],
        void *local_dest, const size_t dest_strides[],
        const size_t count[], size_t stride_levels, size_t node)
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
                "gasnet_comm_layer.c:cache_check_and_get_strided->"
                "Address %p on image %lu found in cache.",
                remote_src, node+1);
        start_offset=remote_src-cache_address;
        if(cache_all_images[node]->handle)
        {
            gasnet_wait_syncnb(cache_all_images[node]->handle);
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
            (coarray_start_all_images[node].addr+shared_memory_size))
            && (size <= getCache_line_size))
        {
            LIBCAF_TRACE(LIBCAF_LOG_CACHE,
            "gasnet_comm_layer.c:cache_check_and_get_strided->"
            " Data for Address %p on image %lu NOT found in cache.",
            remote_src, node+1);

            gasnet_get(cache_line_address, node, remote_src,
                        getCache_line_size);
            cache_all_images[node]->remote_address=remote_src;
            cache_all_images[node]->handle = 0;

            local_strided_copy(cache_line_address,
                    src_strides, local_dest, dest_strides,
                    count, stride_levels);
        }
        else{
            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                "gasnet_comm_layer.c:cache_check_and_get_strided>gasnet_gets_bulk from"
                " %p on image %lu to %p (stride_levels= %u)",
                remote_src, node+1, local_dest, stride_levels);

            gasnet_gets_bulk (local_dest, dest_strides, node, remote_src,
                    src_strides, count,  stride_levels);
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
            "gasnet_comm_layer.c:update_cache-> Value of address %p on"
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
            "gasnet_comm_layer.c:update_cache-> Value of address %p on"
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
            "gasnet_comm_layer.c:update_cache-> Value of address %p on"
            " image %lu partially updated in cache (write conflict).",
            remote_address, node+1);
    }
}

static void update_cache_strided(
        void* remote_dest_address, const size_t dest_strides[],
        void* local_src_address, const size_t src_strides[],
        const size_t count[], unsigned int stride_levels,
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
        "gasnet_comm_layer.c:update_cache_full_strided->"
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
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"gasnet_comm_layer.c:"
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
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"gasnet_comm_layer.c:"
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

static struct write_handle_list* get_next_handle(unsigned long proc,
        void* remote_address, unsigned long size)
{
    struct write_handle_list* handle_node;
    if(write_handles[proc]==0)
    {
        write_handles[proc] = (struct write_handle_list *)
            comm_malloc(sizeof(struct write_handle_list));
        handle_node = write_handles[proc];
        handle_node->prev = 0;
    }
    else{
        handle_node = write_handles[proc];
        while(handle_node->next) {
            handle_node = handle_node->next;
        }
        handle_node->next = (struct write_handle_list *)
            comm_malloc(sizeof(struct write_handle_list));
        handle_node->next->prev = handle_node;
        handle_node = handle_node->next;
    }
    handle_node->handle=GASNET_INVALID_HANDLE;
    handle_node->address=remote_address;
    handle_node->size=size;
    handle_node->next=0; //Just in case there is a sync before the put
    return handle_node;
}

static void reset_min_nbwrite_address(unsigned long proc)
{
    struct write_handle_list *handle_node;
    handle_node = write_handles[proc];
    if(handle_node)
    {
        min_nbwrite_address[proc]=handle_node->address;
        handle_node = handle_node->next;
    }
    else
        min_nbwrite_address[proc]=0;
    while (handle_node)
    {
        if( handle_node->address < min_nbwrite_address[proc] )
            min_nbwrite_address[proc] = handle_node->address;
        handle_node = handle_node->next;
    }
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"gasnet_comm_layer.c:"
        "reset_min_nbwrite_address-> Img%lu min:%p, max:%p",
        proc+1, min_nbwrite_address[proc], max_nbwrite_address[proc]);
}

static void reset_max_nbwrite_address(unsigned long proc)
{
    struct write_handle_list *handle_node;
    void *end_address;
    handle_node = write_handles[proc];
    max_nbwrite_address[proc]=0;
    while (handle_node)
    {
        end_address = handle_node->address + handle_node->size;
        if( end_address > max_nbwrite_address[proc] )
            max_nbwrite_address[proc] = end_address;
        handle_node = handle_node->next;
    }
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"gasnet_comm_layer.c:"
        "reset_max_nbwrite_address-> Img%lu min:%p, max:%p",
        proc+1, min_nbwrite_address[proc], max_nbwrite_address[proc]);
}

static void delete_node(unsigned long proc,
                        struct write_handle_list *node)
{
    void *node_address;
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"gasnet_comm_layer.c:"
        "delete_node-> Img%lu min:%p, max:%p",
        proc+1, min_nbwrite_address[proc], max_nbwrite_address[proc]);
    if(node->prev)
    {
        if(node->next)
        {
            node->prev->next = node->next;
            node->next->prev = node->prev;
        }
        else // last node in the list
            node->prev->next = 0;
    }
    else if(node->next)// this is the first node in the list
    {
        write_handles[proc]=node->next;
        node->next->prev = 0;
    }
    else // this is the only node in the list
    {
        write_handles[proc]=0;
        min_nbwrite_address[proc]=0;
        max_nbwrite_address[proc]=0;
        comm_free(node);
        return;
    }
    node_address = node->address;
    comm_free(node);
    if(node_address == min_nbwrite_address[proc])
        reset_min_nbwrite_address(proc);
    if((node_address + node->size) == max_nbwrite_address[proc])
        reset_max_nbwrite_address(proc);
}

static int address_in_handle(struct write_handle_list *handle_node,
                                void *address, unsigned long size)
{
    if ( ((address+size) > handle_node->address)
        && (address < (handle_node->address+handle_node->size)) )
        return 1;
    else
        return 0;
}

static void wait_on_pending_puts(unsigned long proc, void* remote_address,
                                unsigned long size)
{
    if(address_in_nbwrite_address_block(remote_address, proc, size))
    {
        struct write_handle_list *handle_node, *node_to_delete;
        handle_node = write_handles[proc];
        while (handle_node)
        {
            if(address_in_handle(handle_node,remote_address,size))
            {
                gasnet_wait_syncnb(handle_node->handle);
                delete_node(proc, handle_node);
                return;
            }
            else if (gasnet_try_syncnb(handle_node->handle) == GASNET_OK)
            {
                node_to_delete = handle_node;
                handle_node = handle_node->next;
                delete_node(proc, node_to_delete);
            }
            else 
            {
                handle_node = handle_node->next;
            }
        }
    }
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"gasnet_comm_layer.c:"
        "wait_on_pending_puts-> Img%lu remote_address:%p, size:%lu "
        "min:%p, max:%p",
        proc+1, remote_address, size,
        min_nbwrite_address[proc], max_nbwrite_address[proc]);
}

static void wait_on_all_pending_puts(unsigned long proc)
{
    struct write_handle_list *handle_node, *node_to_delete;
    handle_node = write_handles[proc];
    while (handle_node)
    {
        gasnet_wait_syncnb (handle_node->handle);
        node_to_delete = handle_node;
        handle_node = handle_node->next;
        comm_free(node_to_delete);
    }
    write_handles[proc]=0;
    min_nbwrite_address[proc]=0;
    max_nbwrite_address[proc]=0;
}

static void free_lcb()
{
    struct local_buffer *lcb, *lcb_next;
    lcb = lcb_list;
    while(lcb)
    {
        lcb_next = lcb->next;
        comm_free(lcb->addr);
        comm_free(lcb);
        lcb = lcb_next;
    }
    lcb_list=0;
    lcb_tail=0;
}

/*
 * End of static functions for non-blocking put
 */

/*
 * Shared Memory Management
 */

/* TO BE DONE: (not required for EVERYTHING config)
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
 * For EVERYTHING config:
 * 1) Static coarrays have same address on all images
 * 2) coarray_start_all_images is not populated at init. It needs to be
 *    populated when accessing an image for 1st time (lazy init)*/
static void *get_remote_address(void *src, size_t img)
{
    size_t offset;
    void * remote_start_address;
    if ( (img == my_proc) || !address_on_heap(src) )
        return src;
    if (gasnet_everything)
    {
        if( src>=coarray_start_all_images[my_proc].addr && src<=
            (coarray_start_all_images[my_proc].addr +
             coarray_start_all_images[my_proc].size) )
        {
            offset = src-coarray_start_all_images[my_proc].addr;
            remote_start_address = coarray_start_all_images[img].addr;
            /* lazy fetch as gasnet_getSegmentInfo initialzed it to 0 */
            if(remote_start_address==(void*)0){
                gasnet_get(&remote_start_address,img,
                        &everything_allocatable_start,
                        sizeof(void *) );
                LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                    "gasnet_comm_layer.c:get_remote_address->"
                    "Read image%lu allocatable base address%p",
                        img+1,remote_start_address);
                coarray_start_all_images[img].addr = remote_start_address;
            }
            return remote_start_address+offset;
        }
        else
            return src;
    }
    offset = src - coarray_start_all_images[my_proc].addr;
    return coarray_start_all_images[img].addr+offset;
}

/*
 * End Shared Memory Management
 */

void comm_critical()
{
  GASNET_Safe(gasnet_AMRequestShort1(0,
              GASNET_HANDLER_CRITICAL_REQUEST,
              my_proc));
  GASNET_BLOCKUNTIL(cs_done == 1);
  cs_done = 0;
}

void comm_end_critical()
{
  LIBCAF_TRACE(LIBCAF_LOG_DEBUG, "in comm_end_critical ...");
  GASNET_Safe(gasnet_AMRequestShort1(0,
              GASNET_HANDLER_END_CRITICAL_REQUEST,
              my_proc));
}


void comm_memory_free()
{
    coarray_free_all_shared_memory_slots(); /* in caf_rtl.c */
    if (gasnet_everything)
        comm_free(coarray_start_all_images[my_proc].addr);
    comm_free(coarray_start_all_images);
    comm_free(sync_images_flag);
    if(enable_nbput)
    {
        comm_free(write_handles);
        comm_free(min_nbwrite_address);
        comm_free(max_nbwrite_address);
    }
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
            "gasnet_comm_layer.c:comm_memory_free-> Finished.");
}

void comm_exit(int status)
{
    comm_memory_free();
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_exit-> Before call to gasnet_exit"
        " with status %d." ,status);
    gasnet_exit(status);
}

void comm_finalize()
{
    comm_barrier_all();
    comm_memory_free();
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_finalize-> Before call to gasnet_exit"
        " with status 0.");
    gasnet_exit(0);

    farg_free();
}


void comm_barrier_all()
{
    unsigned long i;
    if(enable_nbput)
    {
        for (i=0; i<num_procs; i++)
           wait_on_all_pending_puts(i);
        free_lcb();
    }
    gasnet_wait_syncnbi_all();
    gasnet_barrier_notify(barcount, barflag);
    gasnet_barrier_wait(barcount, barflag);

    if(enable_get_cache)
        refetch_all_cache();

    barcount += 1;
}


void comm_sync_images(int *image_list, int image_count)
{
    int i,ret;
    gasnet_wait_syncnbi_all();
    for(i=0; i<image_count; i++){
        if(my_proc != image_list[i]){
            ret=gasnet_AMRequestShort1
                (image_list[i], GASNET_HANDLER_SYNC_REQUEST, my_proc);
            if (ret != GASNET_OK) {
                LIBCAF_TRACE(LIBCAF_LOG_FATAL, "sync_images->GASNet AM "
                        "request error");
            }
        }
        else{
            sync_images_flag[my_proc]=1;
        }
        if(enable_nbput)
            wait_on_all_pending_puts(image_list[i]);
    }
    for(i=0; i<image_count; i++){
        GASNET_BLOCKUNTIL(sync_images_flag[image_list[i]]);
        gasnet_hsl_lock( &sync_lock );
        sync_images_flag[image_list[i]]--;
        gasnet_hsl_unlock( &sync_lock );
        if(enable_get_cache)
            refetch_cache(image_list[i]);
    }
}


void* comm_malloc(size_t size)
{
    void* ptr;
    gasnet_hold_interrupts();
    ptr = malloc(size);
    gasnet_resume_interrupts();
    return ptr;
}


void comm_free (void *ptr)
{
    gasnet_hold_interrupts();
    free (ptr);
    gasnet_resume_interrupts();
}


void comm_free_lcb (void *ptr)
{
    if(enable_nbput)
    {
        /* Can not free the memory as strided nonblocking calls are
         * not locally complete. Will free during barrier_all */
        if(lcb_tail)
        {
            lcb_tail->next = (struct local_buffer*)comm_malloc
                        (sizeof(struct local_buffer));
            lcb_tail->next->addr = ptr;
            lcb_tail = lcb_tail->next;
            lcb_tail->next = 0;
        }
        else
        {
            lcb_tail = (struct local_buffer*)comm_malloc
                        (sizeof(struct local_buffer));
            lcb_tail->addr = ptr;
            lcb_tail->next = 0;
            lcb_list = lcb_tail;
        }
    }
    else
    {
        gasnet_hold_interrupts();
        free (ptr);
        gasnet_resume_interrupts();
    }
}

void comm_read( size_t proc, void *src, void *dest, size_t nbytes)
{
    void *remote_src;
    remote_src = get_remote_address(src,proc);
    if(enable_nbput)
        wait_on_pending_puts(proc, remote_src, nbytes);

    if(enable_get_cache) {
        cache_check_and_get(proc, remote_src, nbytes, dest);
    } else {
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "gasnet_comm_layer.c:comm_read->gasnet_get from %p on image %lu"
            " to %p size %lu", remote_src, proc+1, dest, nbytes);
        gasnet_get( dest, proc, remote_src, nbytes);
    }
}


void comm_write( size_t proc, void *dest, void *src, size_t nbytes)
{
    void *remote_dest;
    remote_dest = get_remote_address(dest,proc);
    if(enable_nbput)
    {
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "gasnet_comm_layer.c:comm_write->Before gasnet_put_nb to %p on "
            "image %lu from %p size %lu",
            remote_dest, proc+1, src, nbytes);
        wait_on_pending_puts(proc, remote_dest, nbytes);
        struct write_handle_list* handle_node =
            get_next_handle(proc, remote_dest, nbytes);
        handle_node->handle =
            gasnet_put_nb ( proc, remote_dest, src, nbytes);
        handle_node->next = 0;
        update_nbwrite_address_block(remote_dest, proc, nbytes);
    }
    else
    {
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "gasnet_comm_layer.c:comm_write->gasnet_put to %p on image %lu"
            " from %p size %lu", remote_dest, proc+1, src, nbytes);
        gasnet_put( proc, remote_dest, src, nbytes);
    }

    if(enable_get_cache)
        update_cache(proc, remote_dest, nbytes, src);
}


void comm_strided_read ( size_t proc,
        void *src, const size_t src_strides[],
        void *dest, const size_t dest_strides[],
        const size_t count[], size_t stride_levels)
{
    void *remote_src;

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
            wait_on_pending_puts(proc, remote_src, size);
        }

        if(enable_get_cache) {

            cache_check_and_get_strided(remote_src, src_strides,
                   dest, dest_strides, count, stride_levels, proc);

        } else {

            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                "gasnet_comm_layer.c:comm_strided_read->gasnet_gets_bulk from"
                " %p on image %lu to %p (stride_levels= %u)",
                remote_src, proc+1, dest, stride_levels);
            gasnet_gets_bulk (dest, dest_strides, proc, remote_src,
                    src_strides, count,  stride_levels);
        }
    }
}


void comm_strided_write ( size_t proc,
        void *dest, const size_t dest_strides[],
        void *src, const size_t src_strides[],
        const size_t count[], size_t stride_levels)
{
    void *remote_dest;

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
            size_t size;
            /* calculate max size (very conservative!) */
            size = (src_strides[stride_levels-1] * count[stride_levels])
                    + count[0];

            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                "gasnet_comm_layer.c:comm_write_dest_str->Before"
                " gasnet_puts_nb_bulk to %p on image %lu from %p size %lu",
                remote_dest, proc+1, src, size);
            wait_on_pending_puts(proc, remote_dest, size);

            struct write_handle_list* handle_node =
                get_next_handle(proc, remote_dest, size);

            handle_node->handle = gasnet_puts_nb_bulk(proc, remote_dest,
                    dest_strides, src, src_strides, count,  stride_levels);
            handle_node->next = 0;

            update_nbwrite_address_block(remote_dest, proc, size);
        } else {

            LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
                "gasnet_comm_layer.c:comm_write_dest_str->gasnet_put_bulk"
                " to %p on image %lu from %p (stride_levels= %u)",
                remote_dest, proc+1, src, stride_levels);
            gasnet_puts_bulk(proc, remote_dest, dest_strides, src,
                    src_strides, count,  stride_levels);
        }

        if(enable_get_cache) {
            update_cache_strided(remote_dest, dest_strides,
                   src, src_strides, count, stride_levels, proc);
        }
    }
}
