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

#include "comm.h"
#include "lock.h"
#include "trace.h"
#include "profile.h"
#include "alloc.h"


extern int __ompc_init_rtl(int num_threads);

#pragma weak __ompc_init_rtl

/* initialized in comm_init() */
unsigned long _this_image;
unsigned long _num_images;


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
    PROFILE_FUNC_ENTRY(0);

    /* common slot is initialized in comm_init */
    CALLSITE_TIMED_TRACE(INIT, INIT, comm_init);

    /* initialize the openmp runtime library, if it exists */
    if (__ompc_init_rtl)
        __ompc_init_rtl(0);

    PROFILE_FUNC_EXIT();

    LIBCAF_TRACE(LIBCAF_LOG_INIT, "exit");
}

void __caf_finalize(int exit_code)
{
    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "entry");
    PROFILE_FUNC_ENTRY(0);

    LIBCAF_TRACE(LIBCAF_LOG_TIME_SUMMARY, "Accumulated Time:");
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY_SUMMARY, "\n\tHEAP USAGE: ");

    CALLSITE_TRACE(EXIT, comm_finalize, exit_code);

    PROFILE_FUNC_EXIT();

    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "exit");

    /* does not reach */
}

void __target_alloc(unsigned long buf_size, void **ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    *ptr = coarray_asymmetric_allocate_(buf_size);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "allocated target %p of size %lu",
                 *ptr, buf_size);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

void *__target_alloc2(unsigned long buf_size, void *orig_ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    void *ret_ptr;

    if (comm_address_in_shared_mem(orig_ptr))
        return orig_ptr;

    ret_ptr = coarray_asymmetric_allocate_(buf_size);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "allocated target %p of size %lu",
                 ret_ptr, buf_size);

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
    return ret_ptr;
}

void __target_dealloc(void **ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    coarray_asymmetric_deallocate_(*ptr);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "freed target %p", *ptr);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}


void __acquire_lcb(unsigned long buf_size, void **ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    *ptr = comm_lcb_malloc(buf_size);

    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "acquired lcb %p of size %lu",
                 *ptr, buf_size);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

void __release_lcb(void **ptr)
{
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "entry");
    comm_lcb_free(*ptr);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "freed lcb %p", *ptr);
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY, "exit");
}

void __coarray_sync(comm_handle_t hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "before call to comm_sync with hdl=%p",
                 hdl);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync, hdl);

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void __coarray_nbread(size_t image, void *src, void *dest, size_t nbytes,
                      comm_handle_t * hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    PROFILE_FUNC_ENTRY(0);

    check_remote_image(image);

    /* initialize to NULL */
    if (hdl)
        *hdl = NULL;

    /* reads nbytes from src on proc 'image-1' into local dest */
    CALLSITE_TIMED_TRACE(COMM, READ, comm_nbread, image - 1, src, dest,
                         nbytes, hdl);


    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void __coarray_read(size_t image, void *src, void *dest, size_t nbytes)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");

    PROFILE_FUNC_ENTRY(0);

    check_remote_image(image);

    /* reads nbytes from src on proc 'image-1' into local dest */
    CALLSITE_TIMED_TRACE(COMM, READ, comm_read, image - 1, src, dest,
                         nbytes);


    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void __coarray_write_from_lcb(size_t image, void *dest, void *src,
                              size_t nbytes, int ordered,
                              comm_handle_t * hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(0);

    check_remote_image(image);

    /* initialize to NULL */
    if (hdl && hdl != (void *) -1)
        *hdl = NULL;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_write_from_lcb, image - 1, dest,
                         src, nbytes, ordered, hdl);

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void __coarray_write(size_t image, void *dest, void *src,
                     size_t nbytes, int ordered, comm_handle_t * hdl)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(0);

    check_remote_image(image);

    /* initialize to NULL */
    if (hdl && hdl != (void *) -1)
        *hdl = NULL;

    CALLSITE_TIMED_TRACE(COMM, WRITE, comm_write, image - 1, dest, src,
                         nbytes, ordered, hdl);

    PROFILE_FUNC_EXIT();
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

    PROFILE_FUNC_ENTRY(0);

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
            PROFILE_FUNC_EXIT();
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
            PROFILE_FUNC_EXIT();
            LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
            return;
            /* not reached */
        }
    }

    CALLSITE_TIMED_TRACE(COMM, READ, comm_strided_nbread, image - 1, src,
                         src_strides, dest, dest_strides, count,
                         stride_levels, hdl);

    PROFILE_FUNC_EXIT();
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

    PROFILE_FUNC_ENTRY(0);

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

        PROFILE_FUNC_EXIT();
        return;
        /* not reached */
    }

    CALLSITE_TIMED_TRACE(COMM, READ, comm_strided_read, image - 1, src,
                         src_strides, dest, dest_strides, count,
                         stride_levels);

    PROFILE_FUNC_EXIT();
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

    PROFILE_FUNC_ENTRY(0);

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

        PROFILE_FUNC_EXIT();
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

    PROFILE_FUNC_EXIT();
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

    PROFILE_FUNC_ENTRY(0);

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
            Error
                ("local buffer for coarray_strided_write "
                 "should be contiguous");
            /* should not reach ... */
        }

        PROFILE_FUNC_EXIT();
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

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}


#pragma weak uhcaf_check_comms_ = uhcaf_check_comms
void uhcaf_check_comms(void)
{
    LIBCAF_TRACE(LIBCAF_LOG_SERVICE, "entry");
    PROFILE_FUNC_ENTRY(0);

    comm_service();

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SERVICE, "exit");
}



void __caf_exit(int status)
{
    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "entry");

    LIBCAF_TRACE(LIBCAF_LOG_TIME_SUMMARY, "Accumulated Time: ");
    LIBCAF_TRACE(LIBCAF_LOG_MEMORY_SUMMARY, "\n\tHEAP USAGE: ");
    LIBCAF_TRACE(LIBCAF_LOG_EXIT, "Exiting with error code %d", status);

    CALLSITE_TRACE(EXIT, comm_exit, status);

    /* does not reach */
}


void _SYNC_ALL(int *status, int stat_len, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_all, status, stat_len,
                         errmsg, errmsg_len);

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

/*************CRITICAL SUPPORT **************/

void _CRITICAL()
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_critical);

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _END_CRITICAL()
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_end_critical);

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

/*************END CRITICAL SUPPORT **************/


void _COARRAY_LOCK(lock_t * lock, const int *image, char *success,
                   int success_len, int *status, int stat_len,
                   char *errmsg, int errmsg_len)
{
    int img;

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);

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

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _COARRAY_UNLOCK(lock_t * lock, const int *image, int *status,
                     int stat_len, char *errmsg, int errmsg_len)
{
    int img;
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);

    if (*image == 0)
        img = _this_image;
    else
        img = *image;

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

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _ATOMIC_DEFINE_1(atomic_t * atom, INT1 * value, int *image)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(0);

    if (*image == 0) {
        /* local reference */
        *atom = (atomic_t) * value;
    } else {
        atomic_t t = (atomic_t) * value;
        check_remote_image(*image);
        check_remote_address(*image, atom);

        /* atomic variables are always of size sizeof(atomic_t) bytes. */
        CALLSITE_TRACE(COMM, comm_write, *image - 1, atom, &t,
                       sizeof(atomic_t), 1, (void *) -1);
    }

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_DEFINE_2(atomic_t * atom, INT2 * value, int *image)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(0);

    if (*image == 0) {
        /* local reference */
        *atom = (atomic_t) * value;
    } else {
        atomic_t t = (atomic_t) * value;
        check_remote_image(*image);
        check_remote_address(*image, atom);

        /* atomic variables are always of size sizeof(atomic_t) bytes. */
        CALLSITE_TRACE(COMM, comm_write, *image - 1, atom, &t,
                       sizeof(atomic_t), 1, (void *) -1);
    }

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_DEFINE_4(atomic_t * atom, INT4 * value, int *image)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(0);

    if (*image == 0) {
        /* local reference */
        *atom = (atomic_t) * value;
    } else {
        atomic_t t = (atomic_t) * value;
        check_remote_image(*image);
        check_remote_address(*image, atom);

        /* atomic variables are always of size sizeof(atomic_t) bytes. */
        CALLSITE_TRACE(COMM, comm_write, *image - 1, atom, &t,
                       sizeof(atomic_t), 1, (void *) -1);
    }

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_DEFINE_8(atomic_t * atom, INT8 * value, int *image)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(0);

    if (*image == 0) {
        /* local reference */
        *atom = (atomic_t) * value;
    } else {
        atomic_t t = (atomic_t) * value;
        check_remote_image(*image);
        check_remote_address(*image, atom);

        /* atomic variables are always of size sizeof(atomic_t) bytes. */
        CALLSITE_TRACE(COMM, comm_write, *image - 1, atom, &t,
                       sizeof(atomic_t), 1, (void *) -1);
    }

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_1(INT1 * value, atomic_t * atom, int *image)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(0);

    if (*image == 0) {
        /* local reference */
        *value = (INT1) * atom;
    } else {
        atomic_t t;
        check_remote_image(*image);
        check_remote_address(*image, atom);

        /* atomic variables are always of size sizeof(atomic_t) bytes. */
        CALLSITE_TRACE(COMM, comm_read, *image - 1, atom, &t,
                       sizeof(atomic_t));

        *value = (INT1) t;
    }

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_2(INT2 * value, atomic_t * atom, int *image)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(0);

    if (*image == 0) {
        /* local reference */
        *value = (INT2) * atom;
    } else {
        atomic_t t;
        check_remote_image(*image);
        check_remote_address(*image, atom);

        /* atomic variables are always of size sizeof(atomic_t) bytes. */
        CALLSITE_TRACE(COMM, comm_read, *image - 1, atom, &t,
                       sizeof(atomic_t));

        *value = (INT2) t;
    }

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_4(INT4 * value, atomic_t * atom, int *image)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(0);

    if (*image == 0) {
        /* local reference */
        *value = (atomic_t) * atom;
    } else {
        atomic_t t;
        check_remote_image(*image);
        check_remote_address(*image, atom);

        /* atomic variables are always of size sizeof(atomic_t) bytes. */
        CALLSITE_TRACE(COMM, comm_read, *image - 1, atom, &t,
                       sizeof(atomic_t));

        *value = (INT4) t;
    }

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _ATOMIC_REF_8(INT8 * value, atomic_t * atom, int *image)
{
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "entry");
    PROFILE_FUNC_ENTRY(0);

    if (*image == 0) {
        /* local reference */
        *value = (INT8) * atom;
    } else {
        atomic_t t;
        check_remote_image(*image);
        check_remote_address(*image, atom);

        /* atomic variables are always of size sizeof(atomic_t) bytes. */
        CALLSITE_TRACE(COMM, comm_read, *image - 1, atom, &t,
                       sizeof(atomic_t));

        *value = (INT8) t;
    }

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_COMM, "exit");
}

void _EVENT_POST(event_t * event, int *image)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);

    if (*image == 0) {
        /* local reference */
        event_t result, inc = 1;
        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_fadd_request, event, &inc,
                             sizeof(event_t), _this_image - 1, &result);
    } else {
        event_t result, inc = 1;
        check_remote_image(*image);
        check_remote_address(*image, event);

        CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_fadd_request, event, &inc,
                             sizeof(event_t), *image - 1, &result);

    }

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _EVENT_QUERY(event_t * event, int *image, char *state, int state_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);

    memset(state, 0, state_len);
    if (*image == 0) {
        *state = (int) (*event) != 0;
    } else {
        check_remote_image(*image);
        check_remote_address(*image, event);
        switch (state_len) {
        case 1:
            CALLSITE_TIMED_TRACE(SYNC, SYNC, _ATOMIC_REF_1,
                                 (INT1 *) & state, event, image);
            break;
        case 2:
            CALLSITE_TIMED_TRACE(SYNC, SYNC, _ATOMIC_REF_2,
                                 (INT2 *) & state, event, image);
            break;
        case 4:
            CALLSITE_TIMED_TRACE(SYNC, SYNC, _ATOMIC_REF_4,
                                 (INT4 *) & state, event, image);
            break;
        case 8:
            CALLSITE_TIMED_TRACE(SYNC, SYNC, _ATOMIC_REF_8,
                                 (INT8 *) & state, event, image);
            break;
        default:
            LIBCAF_TRACE(LIBCAF_LOG_FATAL, "_EVENT_QUERY called "
                         "using state variable with invalid length %d",
                         state_len);
        }
    }

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _EVENT_WAIT(event_t * event, int *image)
{
    event_t state;
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);

    START_TIMER();

    if (*image == 0) {
        int done = 0;
        do {
            state = *event;
            if (state > 0) {
                state = SYNC_FETCH_AND_ADD(event, -1);
                if (state > 0) {
                    /* event variable successfully modified */
                    return;
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
                    return;
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

    STOP_TIMER(SYNC);

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_TIME, "_EVENT_WAIT");
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}


void _SYNC_MEMORY(int *status, int stat_len, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);
    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_memory, status, stat_len,
                         errmsg, errmsg_len);
    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _SYNC_IMAGES(int images[], int image_count, int *status, int stat_len,
                  char *errmsg, int errmsg_len)
{
    int i;
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);
    for (i = 0; i < image_count; i++) {
        check_remote_image(images[i]);
        images[i];
    }

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_images, images, image_count,
                         status, stat_len, errmsg, errmsg_len);

    PROFILE_FUNC_EXIT();
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}

void _SYNC_IMAGES_ALL(int *status, int stat_len, char *errmsg,
                      int errmsg_len)
{
    int i;
    int image_count = _num_images;
    int *images;

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    PROFILE_FUNC_ENTRY(0);

    images = (int *) comm_malloc(_num_images * sizeof(int));
    for (i = 0; i < image_count; i++)
        images[i] = i + 1;

    CALLSITE_TIMED_TRACE(SYNC, SYNC, comm_sync_images, images, image_count,
                         status, stat_len, errmsg, errmsg_len);

    comm_free(images);

    PROFILE_FUNC_EXIT();
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

    if (sub->dimension[0].extent != corank)
        return 0;

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
