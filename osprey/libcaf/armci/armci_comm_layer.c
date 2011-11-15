/*
 ARMCI Communication Layer for supporting Coarray Fortran

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


#define ENABLE_LOCAL_MEMCPY
#define MAX_DIMS 15

#define DEFAULT_SHARED_MEMORY_SIZE 30000000L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include "mpi.h"
#include "armci.h"
#include "armci_comm_layer.h"
#include "trace.h"
#include "caf_rtl.h"

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
    char *caf_shared_memory_size_str;
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

    /* Create flags and mutex for sync_images */
    ARMCI_Create_mutexes(num_procs);
    syncptr = malloc (num_procs*sizeof(void *));
    ARMCI_Malloc ((void**)syncptr, num_procs*sizeof(int));
    for ( i=0; i<num_procs; i++ )
        ((int*)(syncptr[my_proc]))[i]=0;

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

    /* initialize common shared memory slot */
    common_shared_memory_slot->addr = coarray_start_all_images[my_proc]
                                                + static_coarray_size;
    common_shared_memory_slot->size = caf_shared_memory_size
                                                - static_coarray_size;
    common_shared_memory_slot->feb = 0;
    common_shared_memory_slot->next =0;
    common_shared_memory_slot->prev =0;

    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,"armci_comm_layer.c:comm_init-> Img%lu"
        "Finished. Waiting for global barrier."
        "common_slot->addr=%p, common_slot->size=%lu",
        my_proc+1, common_shared_memory_slot->addr,
        common_shared_memory_slot->size);

    ARMCI_Barrier();
}


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
    if (img == my_proc)
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
            "armci_comm_layer.c:comm_exit-> Before call to MPI_Finalize"
            );
    MPI_Finalize();
}

void comm_barrier_all()
{
    ARMCI_WaitAll();
    ARMCI_Barrier();
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

void comm_read(void *src, void *dest, unsigned long xfer_size,
                unsigned long proc)
{
    void *remote_src;
    remote_src = get_remote_address(src,proc);
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_read->Before ARMCI_Get from %p on"
            " image %lu to %p size %lu",
            remote_src, proc+1, dest, xfer_size);
    ARMCI_Get(remote_src, dest, xfer_size, proc);
}

void comm_write(void *dest, void *src, unsigned long xfer_size,
                unsigned long proc)
{
    void *remote_dest;
    remote_dest = get_remote_address(dest,proc);
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_read->Before ARMCI_Put to %p on"
            " image %lu from %p size %lu",
            remote_dest, proc+1, src, xfer_size);
    ARMCI_Put(src, remote_dest, xfer_size, proc);
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

        remote_src = get_remote_address(src,proc);
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_read_src_str->Before ARMCI_GetS"
            " from %p on image %lu to %p ndim %lu",
            remote_src, proc+1, dest, ndim-1);

        ARMCI_GetS(remote_src, src_stride_ar, dest, dst_stride_ar,
                         count,  ndim-1, proc);
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

    for (i = 0; i < dest_ndim-1; i++) {
        src_stride_ar[i] = src_strides[i+1];
        dest_stride_ar[i] = dest_strides[i+1];
        count[i] = dest_extents[i];
        if(dest_extents[i]!=src_extents[i])
        {
            LIBCAF_TRACE(LIBCAF_LOG_FATAL,
            "armci_comm_layer.c:comm_read_full_str->src and dest extent"
            " must be same. src_extents[%d]=%d, dest_extents[%d]=%d",
            i, src_extents[i],i, dest_extents[i]);
        }
    }
    count[src_ndim-1] = src_extents[src_ndim-1];
    count[0] = src_extents[0]*elem_size;

    remote_src = get_remote_address(src,proc);
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "armci_comm_layer.c:comm_read_full_str->Before ARMCI_GetS"
        " from %p on image %lu to %p ndim %lu",
        remote_src, proc+1, dest, src_ndim-1);

    ARMCI_GetS(remote_src, src_stride_ar, dest, dest_stride_ar,
                     count,  src_ndim-1, proc );
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
        LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
            "armci_comm_layer.c:comm_write_dest_str->Before ARMCI_PutS"
            " to %p on image %lu from %p ndim %lu",
            remote_dest, proc+1, src, ndim-1);

        ARMCI_PutS(src, src_stride_ar, remote_dest, dest_stride_ar,
                         count,  ndim-1, proc );
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
    LIBCAF_TRACE(LIBCAF_LOG_DEBUG,
        "armci_comm_layer.c:comm_write_full_str->Before ARMCI_PutS"
        " to %p on image %lu from %p ndim %lu",
        remote_dest, proc+1, src, dest_ndim-1);

    /*DEBUG
    printf("INput from caf_rtc:\n ndim=%u, remote_img=%lu\n", dest_ndim, proc);
    for(i=0; i<dest_ndim; i++){
        printf("src_strides[%d]=%lu, dest_srides[%d]=%lu, extents[%d]=%lu\n",
                i,src_strides[i],i,dest_strides[i],i, dest_extents[i]);
    }
    printf("INput to ARMCI_PutS:\n ndim=%u, remote_img=%lu\n", dest_ndim, proc);
    for(i=0; i<dest_ndim; i++){
        printf("src_stride_ar[%d]=%d, dest_sride_ar[%d]=%d, count[%d]=%d\n",
                i,src_stride_ar[i],i,dest_stride_ar[i],i, count[i]);
    }
   END DEBUG */

    ARMCI_PutS(src, src_stride_ar, remote_dest, dest_stride_ar,
                     count,  dest_ndim-1, proc );
}
