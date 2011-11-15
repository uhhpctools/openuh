/*
 GASNet Communication runtime library to be used with OpenUH

 Copyright (C) 2009-2010 University of Houston.

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
void * everything_allocatable_start;

/*sync images*/
static unsigned short *sync_images_flag;
static gasnet_hsl_t sync_lock = GASNET_HSL_INITIALIZER;

/*non-blocking put*/
#if defined(ENABLE_NONBLOCKING_PUT)
static struct write_handle_list **write_handles_per_node;
#endif


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


/*
 * start of handlers
 */

/* handler funtion for  sync images */
void handler_sync_request(gasnet_token_t token, int imageIndex)
{
    gasnet_hsl_lock( &sync_lock );
    sync_images_flag[imageIndex]++;
    gasnet_hsl_unlock( &sync_lock );
}

static gasnet_handlerentry_t handlers[] =
  {
    { GASNET_HANDLER_SYNC_REQUEST, handler_sync_request }
  };
static const int nhandlers = sizeof(handlers) / sizeof(handlers[0]);

/*
 * end of handlers
 */

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
    unsigned long caf_shared_memory_size;
    unsigned long caf_shared_memory_pages;
    unsigned long static_coarray_size;
    unsigned long max_size=powl(2,(sizeof(unsigned long)*8))-1;

    argv = (char **) malloc(argc * sizeof(*argv));
    argv[0] = "caf";

    ret = gasnet_init(&argc, &argv);
    if (ret != GASNET_OK) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL, "GASNet init error");
    }

    uintptr_t max_local = gasnet_getMaxLocalSegmentSize();
    if(max_local == -1)
        gasnet_everything = 1;

    my_proc = gasnet_mynode();
    num_procs = gasnet_nodes();

    /* malloc dataStructures for sync_images, nb-put */
    sync_images_flag = (unsigned short *)malloc
        (num_procs*sizeof(unsigned short));
#if defined(ENABLE_NONBLOCKING_PUT)
    write_handles_per_node = (struct write_handle_list **)malloc
        (num_procs*sizeof(struct write_handle_list *));
#endif

    /* initialize data structures to 0 */
    for(i=0; i<num_procs; i++){
        sync_images_flag[i]=0;
#if defined(ENABLE_NONBLOCKING_PUT)
        write_handles_per_node[i]=0;
#endif
    }

    /* create pinned-down/registered memory  and populate
     * coarray_start_all_images */
    coarray_start_all_images = (gasnet_seginfo_t *)malloc
        (num_procs*sizeof(gasnet_seginfo_t));
    caf_shared_memory_size_str = getenv("UHCAF_SHARED_MEMORY_SIZE");
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

#if defined(ENABLE_NONBLOCKING_PUT)
/*
 * static functions for non-blocking put
 */
static struct write_handle_list* get_next_handle(unsigned long proc)
{
    struct write_handle_list* handle_node;
    if(write_handles_per_node[proc]==0)
    {
        write_handles_per_node[proc] = (struct write_handle_list *)
            comm_malloc(sizeof(struct write_handle_list));
        handle_node = write_handles_per_node[proc];
    }
    else{
        handle_node = write_handles_per_node[proc];
        while(handle_node->next)
        {
            handle_node = handle_node->next;
        }
        handle_node->next = (struct write_handle_list *)
            comm_malloc(sizeof(struct write_handle_list));
        handle_node = handle_node->next;
    }
    handle_node->handle=GASNET_INVALID_HANDLE;
    handle_node->next=0; //Just in case there is a sync before the put
    return handle_node;
}

static void put_sync(unsigned long proc)
{
    struct write_handle_list *handle_node, *delete_node;
    handle_node = write_handles_per_node[proc];
    while (handle_node)
    {
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "gasnet_comm_layer.c:put_sync->Waiting for "
            "non-blocking put on node%p" , handle_node);
        gasnet_wait_syncnb (handle_node->handle);
        delete_node = handle_node;
        handle_node = handle_node->next;
        comm_free(delete_node);
    }
    write_handles_per_node[proc]=0;
}
/*
 * End of static functions for non-blocking put
 */
#endif

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
static void *get_remote_address(void *src, unsigned long img)
{
    unsigned long offset;
    void * remote_start_address;
    if (img == my_proc)
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


void comm_memory_free()
{
    coarray_free_all_shared_memory_slots(); /* in caf_rtl.c */
    if (gasnet_everything)
        comm_free(coarray_start_all_images[my_proc].addr);
    comm_free(coarray_start_all_images);
    comm_free(sync_images_flag);
#if defined(ENABLE_NONBLOCKING_PUT)
    comm_free(write_handles_per_node);
#endif

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
}


void comm_barrier_all()
{
    unsigned long i;
#if defined(ENABLE_NONBLOCKING_PUT)
    for (i=0; i<num_procs; i++)
       put_sync(i);
#endif
    gasnet_wait_syncnbi_all();
    gasnet_barrier_notify(barcount, barflag);
    gasnet_barrier_wait(barcount, barflag);

    barcount += 1;
}


void comm_sync_images(int *image_list, int image_count)
{
    int i,ret;
    gasnet_wait_syncnbi_all();
    for(i=0; i<image_count; i++){
        if(my_proc != image_list[i]){
#if defined(ENABLE_NONBLOCKING_PUT)
            put_sync(image_list[i]);
#endif
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
    }
    for(i=0; i<image_count; i++){
        GASNET_BLOCKUNTIL(sync_images_flag[image_list[i]]);
        gasnet_hsl_lock( &sync_lock );
        sync_images_flag[image_list[i]]--;
        gasnet_hsl_unlock( &sync_lock );
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


void comm_read(void *src, void *dest, unsigned long xfer_size,
                unsigned long proc)
{
    void *remote_src;
    remote_src = get_remote_address(src,proc);
#if defined(ENABLE_NONBLOCKING_PUT)
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_read->waiting for pending puts "
        "on image%lu before start of get", proc+1);
    put_sync(proc);
#endif
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_read->gasnet_get from %p on image %lu"
        " to %p size %lu", remote_src, proc+1, dest, xfer_size);
    gasnet_get( dest, proc, remote_src, xfer_size);
}


void comm_write(void *dest, void *src, unsigned long xfer_size,
                unsigned long proc)
{
    void *remote_dest;
    remote_dest = get_remote_address(dest,proc);
#if defined(ENABLE_NONBLOCKING_PUT)
    struct write_handle_list* handle_node = get_next_handle(proc);
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_write->Got next handle%p. About to get"
        " remote allocatable address for dest%p on image%lu",
        handle_node, dest,proc+1);
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_write->Before gasnet_put_nb to %p on "
        "image %lu from %p size %lu",
        remote_dest, proc+1, src, xfer_size);
    handle_node->handle =
        gasnet_put_nb ( proc, remote_dest, src, xfer_size);
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_write->Got a nb-put handle for node%p"
        " to img%lu" ,handle_node,proc+1);
    handle_node->next = 0;
#else
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_read->gasnet_put to %p on image %lu"
        " from %p size %lu", remote_dest, proc+1, src, xfer_size);
    gasnet_put( proc, remote_dest, src, xfer_size);
#endif
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
        size_t dst_stride_ar[MAX_DIMS];
        size_t src_stride_ar[MAX_DIMS];
        size_t count[MAX_DIMS];

        dst_stride_ar[0] = src_extents[0]*elem_size;
        src_stride_ar[0] = src_strides[1];
        for (i = 1; i < ndim-1; i++) {
            dst_stride_ar[i] = dst_stride_ar[i-1]*src_extents[i];
            src_stride_ar[i] = src_strides[i+1];
            count[i] = src_extents[i];
        }
        count[ndim-1] = src_extents[ndim-1];
        count[0] = src_extents[0]*elem_size;

        remote_src = get_remote_address(src,proc);
#if defined(ENABLE_NONBLOCKING_PUT)
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "gasnet_comm_layer.c:comm_read_src_str->waiting for pending"
            " puts on image%lu before start of strided get", proc+1);
        put_sync(proc);
#endif
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "gasnet_comm_layer.c:comm_read_src_str->gasnet_gets_bulk from"
            " %p on image %lu to %p ndim %u",
            remote_src, proc+1, dest, ndim-1);
        gasnet_gets_bulk (dest, dst_stride_ar, proc, remote_src,
                src_stride_ar, count,  ndim-1);
    }
}


void comm_read_full_str (void * src, void *dest, unsigned int src_ndim,
        unsigned long *src_strides, unsigned long *src_extents, 
        unsigned int dest_ndim, unsigned long *dest_strides,
        unsigned long *dest_extents, unsigned long proc)
{
    int i,j;
    unsigned long elem_size = src_strides[0];
    void *remote_src;
    size_t src_stride_ar[MAX_DIMS];
    size_t dest_stride_ar[MAX_DIMS];
    size_t count[MAX_DIMS];

    if(src_ndim!=dest_ndim)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
        "gasnet_comm_layer.c:comm_read_full_str->src and dest dim must"
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
            "gasnet_comm_layer.c:comm_read_full_str->src and dest extent"
            " must be same. src_extents[%d]=%d, dest_extents[%d]=%d",
            i, src_extents[i],i, dest_extents[i]);
        }
    }
    count[src_ndim-1] = src_extents[src_ndim-1];
    count[0] = src_extents[0]*elem_size;

    remote_src = get_remote_address(src,proc);
#if defined(ENABLE_NONBLOCKING_PUT)
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "gasnet_comm_layer.c:comm_read_full_str->waiting for pending"
            " puts on image%lu before start of strided get", proc+1);
        put_sync(proc);
#endif
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_read_full_str->Before ARMCI_GetS"
        " from %p on image %lu to %p ndim %lu",
        remote_src, proc+1, dest, src_ndim-1);

    gasnet_gets_bulk (dest, dest_stride_ar, proc, remote_src,
            src_stride_ar, count,  src_ndim-1);
}


void comm_write_dest_str(void *dest, void *src, unsigned int ndim,
                    unsigned long *dest_strides,
                    unsigned long *dest_extents,
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


        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "gasnet_comm_layer.c:comm_write_dest_str-> Local Strided "
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
        size_t src_stride_ar[MAX_DIMS];
        size_t dest_stride_ar[MAX_DIMS];
        size_t count[MAX_DIMS];

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

#if defined(ENABLE_NONBLOCKING_PUT)
        struct write_handle_list* handle_node = get_next_handle(proc);
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "gasnet_comm_layer.c:comm_write_src_str->gasnet_put_nb_bulk"
            " to %p on image %lu from %p ndim %u",
            remote_dest, proc+1, src, ndim-1);
        handle_node->handle = gasnet_puts_nb_bulk(proc, remote_dest,
                dest_stride_ar, src, src_stride_ar, count,  ndim-1);
        handle_node->next = 0;
#else
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "gasnet_comm_layer.c:comm_write_src_str->gasnet_put_bulk"
            " to %p on image %lu from %p ndim %u",
            remote_dest, proc+1, src, ndim-1);
        gasnet_puts_bulk(proc, remote_dest, dest_stride_ar, src,
                src_stride_ar, count,  ndim-1);
#endif

    }
}


void comm_write_full_str (void * dest, void *src, unsigned int dest_ndim,
        unsigned long *dest_strides, unsigned long *dest_extents, 
        unsigned int src_ndim, unsigned long *src_strides,
        unsigned long *src_extents, unsigned long proc)
{
    int i,j;
    unsigned long elem_size = dest_strides[0];
    void *remote_dest;
    size_t src_stride_ar[MAX_DIMS];
    size_t dest_stride_ar[MAX_DIMS];
    size_t count[MAX_DIMS];

    if(src_ndim!=dest_ndim)
    {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
        "gasnet_comm_layer.c:comm_write_full_str->src and dest dim must"
        " be same. src_ndim=%d, dest_ndim=%d",
        src_ndim, dest_ndim);
    }

    src_stride_ar[0] = dest_extents[0]*elem_size;
    dest_stride_ar[0] = dest_strides[1];
    for (i = 0; i < dest_ndim-1; i++) {
        src_stride_ar[i] = src_strides[i+1];
        dest_stride_ar[i] = dest_strides[i+1];
        count[i] = dest_extents[i];
        if(dest_extents[i]!=src_extents[i])
        {
            LIBCAF_TRACE(LIBCAF_LOG_FATAL,
            "gasnet_comm_layer.c:comm_write_full_str->src and dest extent"
            " must be same. src_extents[%d]=%d, dest_extents[%d]=%d",
            i, src_extents[i],i, dest_extents[i]);
        }
    }
    count[dest_ndim-1] = dest_extents[dest_ndim-1];
    count[0] = dest_extents[0]*elem_size; //as it must be in bytes

    remote_dest = get_remote_address(dest,proc);

#if defined(ENABLE_NONBLOCKING_PUT)
    struct write_handle_list* handle_node = get_next_handle(proc);
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_write_src_str->gasnet_put_nb_bulk"
        " to %p on image %lu from %p ndim %u",
        remote_dest, proc+1, src, ndim-1);
    handle_node->handle = gasnet_puts_nb_bulk(proc, remote_dest,
            dest_stride_ar, src, src_stride_ar, count,  ndim-1);
    handle_node->next = 0;
#else
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "gasnet_comm_layer.c:comm_write_src_str->gasnet_put_bulk"
        " to %p on image %lu from %p ndim %u",
        remote_dest, proc+1, src, dest_ndim-1);
    gasnet_puts_bulk(proc, remote_dest, dest_stride_ar, src,
            src_stride_ar, count,  dest_ndim-1);
#endif

}
