/*
 Runtime library for supporting Coarray Fortran

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


#ifndef CAF_RTL_H
#define CAF_RTL_H

#include <ctype.h>
#include "lock.h"
#include "dopevec.h"

/* SHARED MEMORY MANAGEMENT */
struct shared_memory_slot {
    void *addr;
    unsigned long size;
    unsigned short feb;         //full empty bit. 1=full
    struct shared_memory_slot *next;
    struct shared_memory_slot *prev;
};

#define LOAD_STORE_FENCE() __sync_synchronize()
#define SYNC_FETCH_AND_ADD(t,v) __sync_fetch_and_add(t,v)
#define SYNC_SWAP(t,v) __sync_lock_test_and_set(t,v)
#define SYNC_CSWAP(t,u,v) __sync_val_compare_and_swap(t,u,v)

typedef int8_t INT1;
typedef int16_t INT2;
typedef int32_t INT4;
typedef int64_t INT8;

typedef INT4 event_t;


/* COMPILER BACK-END INTERFACE */

void __caf_init();

void __caf_finalize();

void __caf_exit(int status);

/* management of local communication buffers */
void __acquire_lcb(unsigned long buf_size, void **ptr);
void __release_lcb(void **ptr);

/* non-strided (contiguous) read and write operations */
void __coarray_read(size_t image, void *src, void *dest, size_t nbytes);
void __coarray_write(size_t image, void *dest, void *src, size_t nbytes);

/* strided, non-contiguous read and write operations */
void __coarray_strided_read(size_t image,
                            void *src, const size_t src_strides[],
                            void *dest, const size_t dest_strides[],
                            const size_t count[], int stride_levels);

void __coarray_strided_write(size_t image,
                             void *dest, const size_t dest_strides[],
                             void *src, const size_t src_strides[],
                             const size_t count[], int stride_levels);

/* TODO: vector, non-contiguous read and write operations  */

/* COMPILER FRONT-END INTERFACE */

/* SYNCHRONIZATION INTRINSICS */
void _SYNC_ALL();
void _SYNC_MEMORY();
void _SYNC_IMAGES(int *imageList, int imageCount);
void _SYNC_IMAGES_ALL();

/* IMAGE INQUIRY INTRINSICS */
int _IMAGE_INDEX(DopeVectorType * diminfo, DopeVectorType * sub);
void _THIS_IMAGE1(DopeVectorType * ret, DopeVectorType * diminfo);
int _THIS_IMAGE2(DopeVectorType * diminfo, int *sub);

void _LCOBOUND_1(DopeVectorType * ret, DopeVectorType * diminfo);
int _LCOBOUND_2(DopeVectorType * diminfo, int *sub);
void _UCOBOUND_1(DopeVectorType * ret, DopeVectorType * diminfo);
int _UCOBOUND_2(DopeVectorType * diminfo, int *sub);

/* LOCKS SUPPORT */
void _COARRAY_LOCK(lock_t * lock, int *image, char *success,
                   int success_len);
void _COARRAY_UNLOCK(lock_t * lock, int *image);

/* ATOMIC INTRINSICS */
void _ATOMIC_DEFINE_1(INT4 * atom, INT1 * value, int *image);
void _ATOMIC_DEFINE_2(INT4 * atom, INT2 * value, int *image);
void _ATOMIC_DEFINE_4(INT4 * atom, INT4 * value, int *image);
void _ATOMIC_DEFINE_8(INT4 * atom, INT8 * value, int *image);
void _ATOMIC_REF_1(INT1 * value, INT4 * atom, int *image);
void _ATOMIC_REF_2(INT2 * value, INT4 * atom, int *image);
void _ATOMIC_REF_4(INT4 * value, INT4 * atom, int *image);
void _ATOMIC_REF_8(INT8 * value, INT4 * atom, int *image);

/* EVENTS SUPPORT */
void _EVENT_POST(event_t * event, int *image);
void _EVENT_QUERY(event_t * event, int *image, char *state, int state_len);
void _EVENT_WAIT(event_t * event, int *image);

/* critical construct support */
void _CRITICAL();
void _END_CRITICAL();

/* OTHER PUBLIC INTERFACES */

/* shared memory management */
void *coarray_allocatable_allocate_(unsigned long var_size);
void *coarray_asymmetric_allocate_(unsigned long var_size);
void coarray_deallocate_(void *var_address);
void coarray_free_all_shared_memory_slots();
void coarray_translate_remote_addr(void **remote_addr, int image);

/* runtime checks */
int check_remote_address(size_t, void *);
int check_remote_image(size_t);

/* UHCAF library routines */
void uhcaf_print_heap_map(char *str);
void uhcaf_check_comms(void);

#endif
