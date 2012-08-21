/*
 Communication Layer Interface for supporting Coarray Fortran

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


#ifndef _COMM_H
#define _COMM_H

#define ENABLE_LOCAL_MEMCPY
#define MAX_DIMS 15

/* environment */

#define ENV_GETCACHE                  "UHCAF_GETCACHE"
#define ENV_NBPUT                     "UHCAF_NBPUT"
#define ENV_PROGRESS_THREAD           "UHCAF_PROGRESS_THREAD"
#define ENV_PROGRESS_THREAD_INTERVAL  "UHCAF_PROGRESS_THREAD_INTERVAL"
#define ENV_GETCACHE_LINE_SIZE        "UHCAF_GETCACHE_LINE_SIZE"
#define ENV_SHARED_MEMORY_SIZE        "UHCAF_IMAGE_HEAP_SIZE"

#define DEFAULT_ENABLE_GETCACHE           0
#define DEFAULT_ENABLE_NBPUT              0
#define DEFAULT_ENABLE_PROGRESS_THREAD    0
#define DEFAULT_PROGRESS_THREAD_INTERVAL  1000L /* ns */
/* these will be overridden by the defaults in cafrun script */
#define DEFAULT_GETCACHE_LINE_SIZE        65536L
#define DEFAULT_SHARED_MEMORY_SIZE        31457280L

#define MAX_NUM_IMAGES                    0x100000
#define MAX_IMAGE_HEAP_SIZE               0x1000000000

/* init */
void comm_init(struct shared_memory_slot *common_shared_memory_slot);


/* critical support */
void comm_get_signal(int **signal);
void comm_critical();
void comm_end_critical();


/* proc query functions */
size_t comm_get_proc_id();
size_t comm_get_num_procs();

/* non-strided (contiguous) read and write operations */
void comm_read(size_t proc, void *src, void *dest, size_t nbytes);
void comm_write(size_t proc, void *dest, void *src, size_t nbytes);

/* strided, non-contiguous read and write operations */
void comm_strided_read(size_t proc,
                       void *src, const size_t src_strides[],
                       void *dest, const size_t dest_strides[],
                       const size_t count[], size_t stride_levels);

void comm_strided_write(size_t proc,
                        void *dest, const size_t dest_strides[],
                        void *src, const size_t src_strides[],
                        const size_t count[], size_t stride_levels);

/* TODO: vector, non-contiguous read and write operations  */


/* shared memory management */
unsigned long allocate_static_coarrays();

/* returns addresses ranges for shared heap */
void comm_translate_remote_addr(void **remote_addr, int proc);
ssize_t comm_address_translation_offset(size_t proc);
void *comm_start_heap(size_t proc);
void *comm_end_heap(size_t proc);
void *comm_start_symmetric_heap(size_t proc);
void *comm_end_symmetric_heap(size_t proc);
void *comm_start_asymmetric_heap(size_t proc);
void *comm_end_asymmetric_heap(size_t proc);
void *comm_start_static_heap(size_t proc);
void *comm_end_static_heap(size_t proc);
void *comm_start_allocatable_heap(size_t proc);
void *comm_end_allocatable_heap(size_t proc);

/* malloc & free */
void *comm_malloc(size_t size);
void comm_free(void *ptr);
void comm_free_lcb(void *ptr);

/* barriers */
void comm_barrier_all();
void comm_sync_images(int *image_list, int image_count);

/* atomics */
void comm_swap_request(void *target, void *value, size_t nbytes,
                       int proc, void *retval);
void comm_cswap_request(void *target, void *cond, void *value,
                        size_t nbytes, int proc, void *retval);
void comm_fstore_request(void *target, void *value, size_t nbytes,
                         int proc, void *retval);
void comm_fadd_request(void *target, void *value, size_t nbytes, int proc,
                       void *retval);

/* progress */
void comm_service();

/* exit */
void comm_memory_free();
void comm_exit(int status);
void comm_finalize();

#endif                          /* _COMM_H */
