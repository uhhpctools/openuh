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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "dopevec.h"
#include "caf_rtl.h"
#include "util.h"

#include "uthash.h"
#include "comm.h"
#include "lock.h"
#include "trace.h"
#include "profile.h"
#include "alloc.h"
#include "collectives.h"


extern int __ompc_init_rtl(int num_threads);

#pragma weak __ompc_init_rtl

/* initialized in comm_init() */
unsigned long _this_image;
unsigned long _num_images;
unsigned long _log2_images;
unsigned long _rem_images;

static int is_contiguous_access(const size_t strides[],
                                const size_t count[],
                                size_t stride_levels);
static void local_src_strided_copy(void *src, const size_t src_strides[],
                                   void *dest, const size_t count[],
                                   size_t stride_levels);

static void local_dest_strided_copy(void *src, void *dest,
                                    const size_t dest_strides[],
                                    const size_t count[],
                                    size_t stride_levels);


/* COMPILER BACK-END INTERFACE */

void __caf_init()
{
    LIBCAF_TRACE(LIBCAF_LOG_INIT, "entry");

    static int libcaf_initialized = 0;
    if (libcaf_initialized == 0)
        libcaf_initialized = 1;
    else
        return;

    PROFILE_INIT();
    PROFILE_FUNC_ENTRY(CAFPROF_STARTUP);

    /* common slot is initialized in comm_init */
    CALLSITE_TIMED_TRACE(INIT, INIT, comm_init);

    PROFILE_STATS_INIT();

    /* initialize the openmp runtime library, if it exists */
    if (__ompc_init_rtl)
        __ompc_init_rtl(0);

    PROFILE_FUNC_EXIT(CAFPROF_STARTUP);

    LIBCAF_TRACE(LIBCAF_LOG_INIT, "exit");
}

void __caf_finalize(int exit_code)
{
    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_STOPPED);

    LIBCAF_TRACE(LIBCAF_LOG_TIME_SUMMARY, "Accumulated Time:");
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY_SUMMARY, "\n\tHEAP USAGE: ");

    PROFILE_STATS_DUMP();

    CALLSITE_TRACE(EXIT, comm_finalize, exit_code);

    PROFILE_FUNC_EXIT(CAFPROF_STOPPED);

    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "exit");

    /* does not reach */
}

void __target_alloc(unsigned long buf_size, void **ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_TARGET_ALLOC_DEALLOC);

    *ptr = coarray_asymmetric_allocate_(buf_size);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "allocated target %p of size %lu",
                 *ptr, buf_size);

    PROFILE_FUNC_EXIT(CAFPROF_TARGET_ALLOC_DEALLOC);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

void *__target_alloc2(unsigned long buf_size, void *orig_ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_TARGET_ALLOC_DEALLOC);
    void *ret_ptr;

    if (comm_address_in_shared_mem(orig_ptr))
        return orig_ptr;

    ret_ptr = coarray_asymmetric_allocate_(buf_size);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "allocated target %p of size %lu",
                 ret_ptr, buf_size);

    PROFILE_FUNC_EXIT(CAFPROF_TARGET_ALLOC_DEALLOC);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
    return ret_ptr;
}

void __target_dealloc(void **ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_TARGET_ALLOC_DEALLOC);

    coarray_asymmetric_deallocate_(*ptr);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "freed target %p", *ptr);

    PROFILE_FUNC_EXIT(CAFPROF_TARGET_ALLOC_DEALLOC);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}


void __acquire_lcb(unsigned long buf_size, void **ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_LCB);

    *ptr = comm_lcb_malloc(buf_size);

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "acquired lcb %p of size %lu",
                 *ptr, buf_size);

    PROFILE_FUNC_EXIT(CAFPROF_LCB);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

void __release_lcb(void **ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_LCB);

    comm_lcb_free(*ptr);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "freed lcb %p", *ptr);

    PROFILE_FUNC_EXIT(CAFPROF_LCB);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

void __coarray_wait_all()
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_WAIT);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync, (comm_handle_t) -1);

    PROFILE_FUNC_EXIT(CAFPROF_WAIT);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void __coarray_wait(comm_handle_t *hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_WAIT);

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "before call to comm_sync with hdl=%p",
                 hdl);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync, *hdl);
    *hdl = NULL;

    PROFILE_FUNC_EXIT(CAFPROF_WAIT);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void __coarray_nbread(size_t image, void *src, void *dest, size_t nbytes,
                      comm_handle_t * hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    PROFILE_FUNC_ENTRY(CAFPROF_GET);

    check_remote_image(image);

    /* initialize to NULL */
    if (hdl)
        *hdl = NULL;

    /* reads nbytes from src on proc 'image-1' into local dest */
    CALLSITE_TIMED_TRACE(COMM, READ, comm_nbread, image - 1, src, dest,
                         nbytes, hdl);


    PROFILE_FUNC_EXIT(CAFPROF_GET);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void __coarray_read(size_t image, void *src, void *dest, size_t nbytes)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    PROFILE_FUNC_ENTRY(CAFPROF_GET);

    check_remote_image(image);

    /* reads nbytes from src on proc 'image-1' into local dest */
    CALLSITE_TIMED_TRACE(COMM, READ, comm_read, image - 1, src, dest,
                         nbytes);


    PROFILE_FUNC_EXIT(CAFPROF_GET);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void __coarray_write_from_lcb(size_t image, void *dest, void *src,
                              size_t nbytes, int ordered,
                              comm_handle_t * hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_PUT);

    check_remote_image(image);

    /* initialize to NULL */
    if (hdl && hdl != (void *) -1)
        *hdl = NULL;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_write_from_lcb, image - 1, dest,
                         src, nbytes, ordered, hdl);

    PROFILE_FUNC_EXIT(CAFPROF_PUT);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void __coarray_write(size_t image, void *dest, void *src,
                     size_t nbytes, int ordered, comm_handle_t * hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_PUT);

    check_remote_image(image);

    /* initialize to NULL */
    if (hdl && hdl != (void *) -1)
        *hdl = NULL;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_write, image - 1, dest, src,
                         nbytes, ordered, hdl);

    PROFILE_FUNC_EXIT(CAFPROF_PUT);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void __coarray_strided_nbread(size_t image,
                              void *src, const size_t src_strides[],
                              void *dest, const size_t dest_strides[],
                              const size_t count[], int stride_levels,
                              comm_handle_t * hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    int remote_is_contig = 0;
    int local_is_contig = 0;
    int i;

    PROFILE_FUNC_ENTRY(CAFPROF_GET);

    check_remote_image(image);

    /* initialize to NULL */
    if (hdl)
        *hdl = NULL;

    /* runtime check if it is contiguous transfer */
    remote_is_contig =
        is_contiguous_access(src_strides, count, stride_levels);
    if (remote_is_contig) {
        local_is_contig =
            is_contiguous_access(dest_strides, count, stride_levels);
        size_t nbytes = 1;
        for (i = 0; i <= stride_levels; i++)
            nbytes = nbytes * count[i];

        if (local_is_contig) {
            CALLSITE_TIMED_TRACE(COMM, READ, comm_nbread, image - 1, src,
                                 dest, nbytes, hdl);
            PROFILE_FUNC_EXIT(CAFPROF_GET);
            LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
            return;
            /* not reached */
        } else {
            /* We use a blocking comm_read, rather than than comm_nbread. For
             * the non-blocking caes, it would require saving the actual
             * destination and its stride info, and doing a local strided mem
             * copy upon detecting its completion in the future.  Its unclear
             * whether there is a significant performance advantage to doing
             * this, versus just using a blocking read in this case.
             * Alternatively, we could use the non-blocking strided
             * interfaces, for which we simply can skip the code in this else
             * block. */
            void *buf;
            __acquire_lcb(nbytes, &buf);
            CALLSITE_TIMED_TRACE(COMM, READ, comm_read, image - 1, src,
                                 buf, nbytes);
            local_dest_strided_copy(buf, dest, dest_strides, count,
                                    stride_levels);
            __release_lcb(&buf);
            PROFILE_FUNC_EXIT(CAFPROF_GET);
            LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
            return;
            /* not reached */
        }
    }

    CALLSITE_TIMED_TRACE(COMM, READ, comm_strided_nbread, image - 1, src,
                         src_strides, dest, dest_strides, count,
                         stride_levels, hdl);

    PROFILE_FUNC_EXIT(CAFPROF_GET);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void __coarray_strided_read(size_t image,
                            void *src, const size_t src_strides[],
                            void *dest, const size_t dest_strides[],
                            const size_t count[], int stride_levels)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    int remote_is_contig = 0;
    int local_is_contig = 0;
    int i;

    PROFILE_FUNC_ENTRY(CAFPROF_GET);

    check_remote_image(image);

    /* runtime check if it is contiguous transfer */
    remote_is_contig =
        is_contiguous_access(src_strides, count, stride_levels);
    if (remote_is_contig) {
        local_is_contig =
            is_contiguous_access(dest_strides, count, stride_levels);
        size_t nbytes = 1;
        for (i = 0; i <= stride_levels; i++)
            nbytes = nbytes * count[i];

        if (local_is_contig) {
            CALLSITE_TIMED_TRACE(COMM, READ, comm_read, image - 1, src,
                                 dest, nbytes);
        } else {
            void *buf;
            __acquire_lcb(nbytes, &buf);
            CALLSITE_TIMED_TRACE(COMM, READ, comm_read, image - 1, src,
                                 buf, nbytes);
            local_dest_strided_copy(buf, dest, dest_strides, count,
                                    stride_levels);
            __release_lcb(&buf);
        }

        PROFILE_FUNC_EXIT(CAFPROF_GET);
        return;
        /* not reached */
    }

    CALLSITE_TIMED_TRACE(COMM, READ, comm_strided_read, image - 1, src,
                         src_strides, dest, dest_strides, count,
                         stride_levels);

    PROFILE_FUNC_EXIT(CAFPROF_GET);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void __coarray_strided_write_from_lcb(size_t image,
                                      void *dest,
                                      const size_t dest_strides[],
                                      void *src,
                                      const size_t src_strides[],
                                      const size_t count[],
                                      int stride_levels, int ordered,
                                      comm_handle_t * hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    int remote_is_contig = 0;
    int local_is_contig = 0;
    int i;

    PROFILE_FUNC_ENTRY(CAFPROF_PUT);

    check_remote_image(image);

    /* initialize to NULL */
    if (hdl && hdl != (void *) -1)
        *hdl = NULL;

    /* runtime check if it is contiguous transfer */
    remote_is_contig =
        is_contiguous_access(dest_strides, count, stride_levels);
    if (remote_is_contig) {
        local_is_contig =
            is_contiguous_access(src_strides, count, stride_levels);
        size_t nbytes = 1;
        for (i = 0; i <= stride_levels; i++)
            nbytes = nbytes * count[i];

        if (local_is_contig) {
            CALLSITE_TIMED_TRACE(COMM, WRITE, comm_write_from_lcb,
                                 image - 1, dest, src, nbytes, ordered,
                                 hdl);
        } else {
            Error
                ("local buffer for coarray_strided_write_from_lcb should be contiguous");
            /* should not reach */
        }

        PROFILE_FUNC_EXIT(CAFPROF_PUT);
        LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
        return;
        /* not reached */
    }

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_strided_write_from_lcb,
                         image - 1, dest, dest_strides, src, src_strides,
                         count, stride_levels, ordered, hdl);

    LIBCAF_TRACE(LIBCAF_LOG_COMM,
                 "Finished write(strided) to %p"
                 " on Img %lu from %p using stride_levels %d, ordered=%d",
                 dest, image, src, stride_levels, ordered);

    PROFILE_FUNC_EXIT(CAFPROF_PUT);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void __coarray_strided_write(size_t image,
                             void *dest,
                             const size_t dest_strides[],
                             void *src,
                             const size_t src_strides[],
                             const size_t count[],
                             int stride_levels, int ordered,
                             comm_handle_t * hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    int remote_is_contig = 0;
    int local_is_contig = 0;
    int i;

    PROFILE_FUNC_ENTRY(CAFPROF_PUT);

    check_remote_image(image);

    /* initialize to NULL */
    if (hdl && hdl != (void *) -1)
        *hdl = NULL;

    /* runtime check if it is contiguous transfer */
    remote_is_contig =
        is_contiguous_access(dest_strides, count, stride_levels);
    if (remote_is_contig) {
        local_is_contig =
            is_contiguous_access(src_strides, count, stride_levels);
        size_t nbytes = 1;
        for (i = 0; i <= stride_levels; i++)
            nbytes = nbytes * count[i];

        if (local_is_contig) {
            CALLSITE_TIMED_TRACE(COMM, WRITE, comm_write, image - 1, dest,
                                 src, nbytes, ordered, hdl);
        } else {
            void *buf;
            __acquire_lcb(nbytes, &buf);
            local_src_strided_copy(src, src_strides, buf, count, stride_levels);
            CALLSITE_TIMED_TRACE(COMM, WRITE, comm_write_from_lcb,
                                 image - 1, dest, buf, nbytes, ordered, hdl);
        }

        PROFILE_FUNC_EXIT(CAFPROF_PUT);
        LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
        return;
        /* not reached */
    }

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_strided_write, image - 1, dest,
                         dest_strides, src, src_strides, count,
                         stride_levels, ordered, hdl);

    LIBCAF_TRACE(LIBCAF_LOG_COMM,
                 "Finished write(strided) to %p"
                 " on Img %lu from %p using stride_levels %d, ordered=%d ",
                 dest, image, src, stride_levels, ordered);

    PROFILE_FUNC_EXIT(CAFPROF_PUT);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}


#pragma weak uhcaf_check_comms_ = uhcaf_check_comms
void uhcaf_check_comms(void)
{
    LIBCAF_TRACE(LIBCAF_LOG_SERVICE, "entry");

    comm_service();

    LIBCAF_TRACE(LIBCAF_LOG_SERVICE, "exit");
}



void __caf_exit(int status)
{
    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_STOPPED);

    LIBCAF_TRACE(LIBCAF_LOG_TIME_SUMMARY, "Accumulated Time: ");
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY_SUMMARY, "\n\tHEAP USAGE: ");
    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "Exiting with error code %d", status);

    CALLSITE_TRACE(EXIT, comm_exit, status);

    PROFILE_FUNC_EXIT(CAFPROF_STOPPED);
    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "exit");
    /* does not reach */
}


void _SYNC_ALL(int *status, int stat_len, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_SYNC_STATEMENTS);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_all, status, stat_len,
                         errmsg, errmsg_len);

    PROFILE_FUNC_EXIT(CAFPROF_SYNC_STATEMENTS);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

/*************CRITICAL SUPPORT **************/

void _CRITICAL()
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_MUTEX);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_critical);

    PROFILE_FUNC_EXIT(CAFPROF_MUTEX);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _END_CRITICAL()
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_MUTEX);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_end_critical);

    PROFILE_FUNC_EXIT(CAFPROF_MUTEX);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

/*************END CRITICAL SUPPORT **************/


void _COARRAY_LOCK(lock_t * lock, const int *image, char *success,
                   int success_len, int *status, int stat_len,
                   char *errmsg, int errmsg_len)
{
    int img;

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_MUTEX);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    if (success == NULL && status == NULL) {
        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_lock, lock, img, errmsg,
                             errmsg_len);
    } else {
        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_lock_stat, lock, img,
                             success, success_len, status, stat_len,
                             errmsg, errmsg_len);
    }

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_new_exec_segment);

    PROFILE_FUNC_EXIT(CAFPROF_MUTEX);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _COARRAY_UNLOCK(lock_t * lock, const int *image, int *status,
                     int stat_len, char *errmsg, int errmsg_len)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_MUTEX);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_fence_all);

#if defined(GASNET)
    if (status == NULL) {
        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_unlock, lock, img, errmsg,
                             errmsg_len);
    } else {
        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_unlock_stat, lock, img,
                             status, stat_len, errmsg, errmsg_len);
    }
#elif defined(ARMCI)
    /* this version uses fetch-and-store instead of compare-and-swap */
    if (status == NULL) {
        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_unlock2, lock, img, errmsg,
                             errmsg_len);
    } else {
        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_unlock2_stat, lock, img,
                             status, stat_len, errmsg, errmsg_len);
    }
#endif

    PROFILE_FUNC_EXIT(CAFPROF_MUTEX);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _ATOMIC_DEFINE_4_1(atomic4_t * atom, INT1 * value, int *image)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_atomic_define, img-1,
                         atom, *value);

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_DEFINE_4_2(atomic4_t * atom, INT2 * value, int *image)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_atomic_define, img-1,
                         atom, *value);

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_DEFINE_4_4(atomic4_t * atom, INT4 * value, int *image)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_atomic_define, img-1,
                         atom, *value);

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_DEFINE_4_8(atomic4_t * atom, INT8 * value, int *image)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_atomic_define, img-1,
                         atom, *value);

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_DEFINE_8_1(atomic8_t * atom, INT1 * value, int *image)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_atomic8_define, img-1,
                         atom, *value);

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_DEFINE_8_2(atomic8_t * atom, INT2 * value, int *image)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_atomic8_define, img-1,
                         atom, *value);

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_DEFINE_8_4(atomic8_t * atom, INT4 * value, int *image)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_atomic8_define, img-1,
                         atom, *value);

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_DEFINE_8_8(atomic8_t * atom, INT8 * value, int *image)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_atomic8_define, img-1,
                         atom, *value);

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_4_1(INT1 * value, atomic4_t * atom, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, READ, comm_atomic_ref, &val, img-1,
                         atom);

    *value = (INT1) val;

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_4_2(INT2 * value, atomic4_t * atom, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, READ, comm_atomic_ref, &val, img-1,
                         atom);

    *value = (INT2) val;

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_4_4(INT4 * value, atomic4_t * atom, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, READ, comm_atomic_ref, &val, img-1,
                         atom);

    *value = (INT4) val;

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_4_8(INT8 * value, atomic4_t * atom, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, READ, comm_atomic_ref, &val, img-1,
                         atom);

    *value = (INT8) val;

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_8_1(INT1 * value, atomic8_t * atom, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, READ, comm_atomic8_ref, &val, img-1,
                         atom);

    *value = (INT1) val;

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_8_2(INT2 * value, atomic8_t * atom, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, READ, comm_atomic8_ref, &val, img-1,
                         atom);

    *value = (INT2) val;

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_8_4(INT4 * value, atomic8_t * atom, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, READ, comm_atomic8_ref, &val, img-1,
                         atom);

    *value = (INT4) val;

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_8_8(INT8 * value, atomic8_t * atom, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, READ, comm_atomic8_ref, &val, img-1,
                         atom);

    *value = (INT8) val;

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_ADD_4_1(atomic_t * atom, INT1 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;
    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_add_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fadd_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_ADD_4_2(atomic_t * atom, INT2 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;
    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_add_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fadd_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_ADD_4_4(atomic_t * atom, INT4 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_add_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fadd_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_ADD_4_8(atomic_t * atom, INT8 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_add_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fadd_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_ADD_8_1(atomic8_t * atom, INT1 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;
    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_add_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fadd_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_ADD_8_2(atomic8_t * atom, INT2 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;
    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_add_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fadd_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_ADD_8_4(atomic8_t * atom, INT4 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_add_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fadd_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_ADD_8_8(atomic8_t * atom, INT8 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_add_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fadd_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_AND_4_1(atomic_t * atom, INT1 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_and_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fand_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_AND_4_2(atomic_t * atom, INT2 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_and_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fand_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_AND_4_4(atomic_t * atom, INT4 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_and_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fand_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_AND_4_8(atomic_t * atom, INT8 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_and_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fand_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_AND_8_1(atomic8_t * atom, INT1 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_and_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fand_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_AND_8_2(atomic8_t * atom, INT2 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_and_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fand_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_AND_8_4(atomic8_t * atom, INT4 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_and_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fand_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_AND_8_8(atomic8_t * atom, INT8 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_and_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fand_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_OR_4_1(atomic_t * atom, INT1 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_or_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_for_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_OR_4_2(atomic_t * atom, INT2 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_or_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_for_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_OR_4_4(atomic_t * atom, INT4 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_or_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_for_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_OR_4_8(atomic_t * atom, INT8 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_or_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_for_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_OR_8_1(atomic8_t * atom, INT1 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_or_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_for_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_OR_8_2(atomic8_t * atom, INT2 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_or_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_for_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_OR_8_4(atomic8_t * atom, INT4 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_or_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_for_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_OR_8_8(atomic8_t * atom, INT8 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_or_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_for_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_XOR_4_1(atomic_t * atom, INT1 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_xor_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fxor_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_XOR_4_2(atomic_t * atom, INT2 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_xor_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fxor_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_XOR_4_4(atomic_t * atom, INT4 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_xor_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fxor_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_XOR_4_8(atomic_t * atom, INT8 * value, atomic_t * old, int *image)
{
    INT4 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT4) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_xor_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fxor_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_XOR_8_1(atomic8_t * atom, INT1 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_xor_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fxor_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_XOR_8_2(atomic8_t * atom, INT2 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_xor_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fxor_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_XOR_8_4(atomic8_t * atom, INT4 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_xor_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fxor_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_XOR_8_8(atomic8_t * atom, INT8 * value, atomic8_t * old, int *image)
{
    INT8 val;
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    val = (INT8) *value;

    if (old == NULL) {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_xor_request, atom,
                             &val, sizeof *atom, img-1);
    } else {
        CALLSITE_TIMED_TRACE(COMM, SYNC, comm_fxor_request, atom,
                             &val, sizeof *atom, img-1, old);
    }

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_CAS_4(atomic_t * atom, atomic_t * oldval, atomic_t *compare,
                   atomic_t *newval, int *image)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, SYNC, comm_cswap_request, atom,
                         compare, newval, sizeof *newval, img-1, oldval);

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_CAS_8(atomic8_t * atom, atomic8_t * oldval, atomic8_t *compare,
                   atomic8_t *newval, int *image)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_ATOMICS);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

    CALLSITE_TIMED_TRACE(COMM, SYNC, comm_cswap_request, atom,
                         compare, newval, sizeof *newval, img-1, oldval);

    PROFILE_FUNC_EXIT(CAFPROF_ATOMICS);
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _EVENT_POST(event_t * event, int *image)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_EVENTS);

    if (*image == 0) {
        /* local reference */
        event_t result, inc = 1;
        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_add_request, event, &inc,
                             sizeof(event_t), _this_image - 1);
    } else {
        event_t result, inc = 1;
        check_remote_image(*image);
        check_remote_address(*image, event);

        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_fence_all);

        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_add_request, event, &inc,
                             sizeof(event_t), *image - 1);

    }

    PROFILE_FUNC_EXIT(CAFPROF_EVENTS);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _EVENT_QUERY(event_t * event, int *image, char *state, int state_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_EVENTS);

    memset(state, 0, state_len);
    if (*image == 0) {
        *state = (int) (*event) != 0;
    } else {
        check_remote_image(*image);
        check_remote_address(*image, event);
        switch (state_len) {
        case 1:
            CALLSITE_TIMED_TRACE(SYNC, SYNC, _ATOMIC_REF_4_1,
                                 (INT1 *) & state, event, image);
            break;
        case 2:
            CALLSITE_TIMED_TRACE(SYNC, SYNC, _ATOMIC_REF_4_2,
                                 (INT2 *) & state, event, image);
            break;
        case 4:
            CALLSITE_TIMED_TRACE(SYNC, SYNC, _ATOMIC_REF_4_4,
                                 (INT4 *) & state, event, image);
            break;
        case 8:
            CALLSITE_TIMED_TRACE(SYNC, SYNC, _ATOMIC_REF_4_8,
                                 (INT8 *) & state, event, image);
            break;
        default:
            LIBCAF_TRACE(LIBCAF_LOG_FATAL, "_EVENT_QUERY called "
                         "using state variable with invalid length %d",
                         state_len);
        }
    }

    PROFILE_FUNC_EXIT(CAFPROF_EVENTS);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _EVENT_WAIT(event_t * event, int *image)
{
    event_t state;
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_EVENTS);

    START_TIMER();

    if (*image == 0) {
        int done = 0;
        do {
            state = *event;
            if (state > 0) {
                state = SYNC_FETCH_AND_ADD(event, -1);
                if (state > 0) {
                    /* event variable successfully modified */
                    break;
                } else {
                    /* shouldn't have decremented, so add 1 back */
                    state = SYNC_FETCH_AND_ADD(event, 1);
                }
            }
            comm_service();
        } while (1);
    } else {
        check_remote_image(*image);
        check_remote_address(*image, event);

        LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
                     "suspending tracing while polling remote event");
        LIBCAF_TRACE_SUSPEND();
        do {
            comm_read(*image - 1, event, &state, sizeof(event_t));
            if (state > 0) {
                INT4 dec = -1;
                INT4 inc = 1;
                comm_fadd_request(event, &dec, sizeof(event_t), *image - 1,
                                  &state);
                if (state > 0) {
                    /* event variable successfully modified */
                    break;
                } else {
                    /* shouldn't have decremented, so add 1 back */
                    comm_fadd_request(event, &inc, sizeof(event_t),
                                      *image - 1, &state);
                }
            }
            comm_service();
        } while (1);
        LIBCAF_TRACE_RESUME();
        LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
                     "resuming tracing after polling remote event");
    }

    comm_new_exec_segment;

    STOP_TIMER(SYNC);

    PROFILE_FUNC_EXIT(CAFPROF_EVENTS);
    LIBCAF_TRACE(LIBCAF_LOG_TIME, "_EVENT_WAIT");
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}


void _SYNC_MEMORY(int *status, int stat_len, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_SYNC_STATEMENTS);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_memory, status, stat_len,
                         errmsg, errmsg_len);

    PROFILE_FUNC_EXIT(CAFPROF_SYNC_STATEMENTS);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _SYNC_IMAGES(int images[], int image_count, int *status, int stat_len,
                  char *errmsg, int errmsg_len)
{
#ifdef SYNC_IMAGES_HASHED
    hashed_image_list_t hashed_images[image_count];
    hashed_image_list_t *image_list = NULL;
#endif
    int new_image_count = image_count;
    int i;
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_SYNC_STATEMENTS);
#ifdef SYNC_IMAGES_HASHED
    for (i = 0; i < image_count; i++) {
        hashed_image_list_t *check_duplicate;
        check_remote_image(images[i]);
        HASH_FIND_INT(image_list, &images[i], check_duplicate);
        if (check_duplicate != NULL) {
            new_image_count--;
            continue;
        }
        hashed_images[i].image_id = images[i];
        HASH_ADD_INT(image_list, image_id, (&hashed_images[i]));
    }

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_images, image_list,
                         new_image_count, status, stat_len, errmsg, errmsg_len);
#else
    for (i = 0; i < image_count; i++) {
        check_remote_image(images[i]);
    }
    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_images, images,
                         new_image_count, status, stat_len, errmsg, errmsg_len);
#endif

    PROFILE_FUNC_EXIT(CAFPROF_SYNC_STATEMENTS);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _SYNC_IMAGES_ALL(int *status, int stat_len, char *errmsg,
                      int errmsg_len)
{
    int i;

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_SYNC_STATEMENTS);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_images,
                         NULL, _num_images,
                         status, stat_len, errmsg, errmsg_len);

    PROFILE_FUNC_EXIT(CAFPROF_SYNC_STATEMENTS);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

int _IMAGE_INDEX(DopeVectorType * diminfo, DopeVectorType * sub)
{
    if (diminfo == NULL || sub == NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "image_index failed for "
                     "&diminfo=%p, &codim=%p", diminfo, sub);
    }

    int i;
    int rank = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int image = 0;
    int lb_codim, ub_codim;
    int *codim = (int *) sub->base_addr.a.ptr;
    int str_m = 1;

    if (sub->dimension[0].extent != corank) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "image_index failed due to corank mismatch "
                     " (was %d, should be %d)",
                      corank, sub->dimension[0].extent);
        exit(1);
    }

    for (i = 0; i < corank; i++) {
        int extent;
        str_m = diminfo->dimension[rank + i].stride_mult;
        if (i == (corank - 1))
            extent = (_num_images - 1) / str_m + 1;
        else
            extent = diminfo->dimension[rank + i].extent;
        lb_codim = diminfo->dimension[rank + i].low_bound;
        ub_codim = diminfo->dimension[rank + i].low_bound + extent - 1;
        if (codim[i] >= lb_codim
            && (ub_codim == 0 || codim[i] <= ub_codim)) {
            image += str_m * (codim[i] - lb_codim);
        } else {
            return 0;
        }
    }

    if (_num_images > image)
        return image + 1;
    else
        return 0;
}

void _THIS_IMAGE1(DopeVectorType * ret, DopeVectorType * diminfo)
{
    int i;
    int corank = diminfo->n_codim;
    int *ret_int;
    if (diminfo == NULL || ret == NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "this_image failed for "
                     "&diminfo:%p and &ret:%p", diminfo, ret);
    }
    ret->base_addr.a.ptr = comm_malloc(sizeof(int) * corank);
    ret->dimension[0].low_bound = 1;
    ret->dimension[0].extent = corank;
    ret->dimension[0].stride_mult = 1;
    ret_int = (int *) ret->base_addr.a.ptr;
    for (i = 1; i <= corank; i++) {
        ret_int[i - 1] = _THIS_IMAGE2(diminfo, &i);
    }
}

int _THIS_IMAGE2(DopeVectorType * diminfo, int *sub)
{
    int img = _this_image - 1;
    int rank = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int dim = *sub;
    int str_m = 1;
    int lb_codim = 0;
    int ub_codim = 0;
    int extent, i;

    if (diminfo == NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "this_image failed for &diminfo=%p", diminfo);
    }
    if (dim < 1 || dim > corank) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "this_image failed as %d dim" " is not present", dim);
    }

    lb_codim = diminfo->dimension[rank + dim - 1].low_bound;
    str_m = diminfo->dimension[rank + dim - 1].stride_mult;
    if (dim == corank)
        extent = (_num_images - 1) / str_m + 1;
    else
        extent = diminfo->dimension[rank + dim - 1].extent;
    ub_codim = lb_codim + extent - 1;
    if (ub_codim > 0) {
        return (((img / str_m) % extent) + lb_codim);
    } else {
        return ((img / str_m) + lb_codim);
    }
}

int _LCOBOUND_2(DopeVectorType * diminfo, int *sub)
{
    int rank = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int dim = *sub;
    if (diminfo == NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "lcobound failed for &diminfo:%p", diminfo);
    }
    if (dim < 1 || dim > corank) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "lcobound failed as dim %d not present", dim);
    }
    return diminfo->dimension[rank + dim - 1].low_bound;
}

void _LCOBOUND_1(DopeVectorType * ret, DopeVectorType * diminfo)
{
    int i;
    int rank = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int *ret_int;
    if (diminfo == NULL || ret == NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "lcobound failed for diminfo:%p and ret:%p",
                     diminfo, ret);
    }
    ret->base_addr.a.ptr = comm_malloc(sizeof(int) * corank);
    ret->dimension[0].low_bound = 1;
    ret->dimension[0].extent = corank;
    ret->dimension[0].stride_mult = 1;
    ret_int = (int *) ret->base_addr.a.ptr;
    for (i = 0; i < corank; i++) {
        ret_int[i] = diminfo->dimension[rank + i].low_bound;
    }
}

int _UCOBOUND_2(DopeVectorType * diminfo, int *sub)
{
    int rank = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int dim = *sub;
    int extent;
    if (diminfo == NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "ucobound failed for &diminfo:%p", diminfo);
    }
    if (dim < 1 || dim > corank) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "ucobound failed as dim %d not present", dim);
    }

    if (dim == corank)
        extent = (_num_images - 1) /
            diminfo->dimension[rank + dim - 1].stride_mult + 1;
    else
        extent = diminfo->dimension[rank + dim - 1].extent;

    return (diminfo->dimension[rank + dim - 1].low_bound + extent - 1);
}

void _UCOBOUND_1(DopeVectorType * ret, DopeVectorType * diminfo)
{
    int i;
    int rank = diminfo->n_dim;
    int corank = diminfo->n_codim;
    int *ret_int;
    int extent;
    if (diminfo == NULL || ret == NULL) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "ucobound failed for diminfo:%p and ret:%p",
                     diminfo, ret);
    }
    ret->base_addr.a.ptr = comm_malloc(sizeof(int) * corank);
    ret->dimension[0].low_bound = 1;
    ret->dimension[0].extent = corank;
    ret->dimension[0].stride_mult = 1;
    ret_int = (int *) ret->base_addr.a.ptr;
    for (i = 0; i < corank; i++) {
        if (i == (corank - 1))
            extent = (_num_images - 1) /
                diminfo->dimension[rank + i].stride_mult + 1;
        else
            extent = diminfo->dimension[rank + i].extent;

        ret_int[i] = diminfo->dimension[rank + i].low_bound + extent - 1;
    }
}


static int is_contiguous_access(const size_t strides[],
                                const size_t count[], size_t stride_levels)
{
    int block_size = 1;
    int is_contig = 1;
    int i;

    block_size = count[0];
    for (int i = 1; i <= stride_levels; i++) {
        if (count[i] == 1)
            continue;
        else if (block_size == strides[i - 1])
            block_size = block_size * count[i];
        else
            return 0;
    }
    return 1;
}


/* Translate a remote address from a given image to the local address space
 */
void coarray_translate_remote_addr(void **remote_addr, int image)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    LIBCAF_TRACE(LIBCAF_LOG_COMM,
                 "(start) - "
                 "remote_addr: %p, image: %d ", *remote_addr, image);

    CALLSITE_TRACE(COMM, comm_translate_remote_addr, remote_addr,
                   image - 1);

    LIBCAF_TRACE(LIBCAF_LOG_COMM,
                 "(end) - "
                 "remote_addr: %p, image: %d ", *remote_addr, image);

    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

static void local_src_strided_copy(void *src, const size_t src_strides[],
                                   void *dest, const size_t count[],
                                   size_t stride_levels)
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
        if (src_strides[i] == blockdim_size[i])
            blk_size = src_strides[i];
        else
            num_blks *= count[i];
    }

    for (i = 1; i <= num_blks; i++) {
        memcpy(dest_ptr, src_ptr, blk_size);
        dest_ptr += blk_size;
        for (j = 1; j <= stride_levels; j++) {
            if (i % cnt_strides[j])
                break;
            src_ptr -= (count[j] - 1) * src_strides[j - 1];
        }
        src_ptr += src_strides[j - 1];
    }
}

static void local_dest_strided_copy(void *src, void *dest,
                                    const size_t dest_strides[],
                                    const size_t count[],
                                    size_t stride_levels)
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
        if (dest_strides[i] == blockdim_size[i])
            blk_size = dest_strides[i];
        else
            num_blks *= count[i];
    }

    for (i = 1; i <= num_blks; i++) {
        memcpy(dest_ptr, src_ptr, blk_size);
        src_ptr += blk_size;
        for (j = 1; j <= stride_levels; j++) {
            if (i % cnt_strides[j])
                break;
            dest_ptr -= (count[j] - 1) * dest_strides[j - 1];
        }
        dest_ptr += dest_strides[j - 1];
    }
}

/*
 * image should be between 1 .. NUM_IMAGES
 */
int check_remote_image(size_t image)
{
    const int error_len = 255;
    char error_msg[error_len];
    if (image < 1 || image > _num_images) {
        memset(error_msg, 0, error_len);
        sprintf(error_msg,
                "Image %lu is out of range. Should be in [ %u ... %lu ].",
                (unsigned long) image, 1, (unsigned long) _num_images);
        Error(error_msg);
        /* should not reach */
    }
}

/*
 * address is either the address of the local symmetric variable that must be
 * translated to the remote image, or it is a remote address on the remote
 * image
 */
int check_remote_address(size_t image, void *address)
{
    const int error_len = 255;
    char error_msg[error_len];

    if ((address < comm_start_symmetric_mem(_this_image - 1) ||
         address > comm_end_symmetric_mem(_this_image - 1)) &&
        (address < comm_start_asymmetric_heap(image - 1) ||
         address > comm_end_asymmetric_heap(image - 1))) {
        memset(error_msg, 0, error_len);
        sprintf(error_msg,
                "Address %p (translates to %p) is out of range. "
                "Should fall within [ %p ... %p ] "
                "on remote image %lu.",
                address,
                (char *) address + comm_address_translation_offset(image -
                                                                   1),
                comm_start_shared_mem(image - 1),
                comm_end_shared_mem(image - 1), (unsigned long) image);
        Error(error_msg);
        /* should not reach */
    }
}
