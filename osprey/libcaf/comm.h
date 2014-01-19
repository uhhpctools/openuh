/*
 Communication Layer Interface for supporting Coarray Fortran

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


#ifndef _COMM_H
#define _COMM_H

#define ENABLE_LOCAL_MEMCPY
#define MAX_DIMS 15


#define MAX_NUM_IMAGES                    0x100000
#define MAX_SHARED_MEMORY_SIZE            0x1000000000


/* status codes for synchronization statements */

enum {
    STAT_SUCCESS = 0,
    /* these should correspond to the same values define in
     * ../libfi/mathlb/iso_fortran_env.F90 */
    STAT_LOCKED = 775,
    STAT_UNLOCKED = 776,
    STAT_LOCKED_OTHER_IMAGE = 777,
    STAT_STOPPED_IMAGE = 778
} status_codes;

/* different types of sync images algorithms */
typedef enum {
  SYNC_COUNTER = 0,
  SYNC_PING_PONG = 1,
  SYNC_SENSE_REV = 2,
  SYNC_IMAGES_DEFAULT = 2
} sync_images_t;

/* init */
void comm_init();

/* critical support */
void comm_get_signal(int **signal);
void comm_critical();
void comm_end_critical();


/* proc query functions */
size_t comm_get_proc_id();
size_t comm_get_num_procs();

/* non-strided (contiguous) read and write operations */
void comm_nbread(size_t proc, void *src, void *dest, size_t nbytes,
                 comm_handle_t * hdl);
void comm_read(size_t proc, void *src, void *dest, size_t nbytes);
void comm_write_from_lcb(size_t proc, void *dest, void *src, size_t nbytes,
                         int ordered, comm_handle_t * hdl);
void comm_write(size_t proc, void *dest, void *src,
                size_t nbytes, int ordered, comm_handle_t * hdl);
void comm_write_TEST(size_t proc, void *dest, void *src,
                     size_t nbytes, int ordered, comm_handle_t * hdl);

/* strided, non-contiguous read and write operations */
void comm_strided_nbread(size_t proc,
                         void *src, const size_t src_strides[],
                         void *dest, const size_t dest_strides[],
                         const size_t count[], size_t stride_levels,
                         comm_handle_t * hdl);

void comm_strided_read(size_t proc,
                       void *src, const size_t src_strides[],
                       void *dest, const size_t dest_strides[],
                       const size_t count[], size_t stride_levels);

void comm_strided_write_from_lcb(size_t proc,
                                 void *dest, const size_t dest_strides[],
                                 void *src, const size_t src_strides[],
                                 const size_t count[],
                                 size_t stride_levels, int ordered,
                                 comm_handle_t * hdl);

void comm_strided_write(size_t proc,
                        void *dest, const size_t dest_strides[],
                        void *src, const size_t src_strides[],
                        const size_t count[],
                        size_t stride_levels, int ordered,
                        comm_handle_t * hdl);

/* TODO: vector, non-contiguous read and write operations  */


/* shared memory management */
void allocate_static_symm_data(void *base_address);

/* returns addresses ranges for shared heap */
int comm_address_in_shared_mem(void *addr);
void comm_translate_remote_addr(void **remote_addr, int proc);
ssize_t comm_address_translation_offset(size_t proc);
void *comm_start_shared_mem(size_t proc);
void *comm_start_symmetric_mem(size_t proc);
void *comm_start_static_data(size_t proc);
void *comm_end_static_data(size_t proc);
void *comm_start_allocatable_heap(size_t proc);
void *comm_end_allocatable_heap(size_t proc);
void *comm_end_symmetric_mem(size_t proc);
void *comm_start_asymmetric_heap(size_t proc);
void *comm_end_asymmetric_heap(size_t proc);
void *comm_end_shared_mem(size_t proc);

/* malloc & free */
void *comm_malloc(size_t size);
void comm_free(void *ptr);

void *comm_lcb_malloc(size_t size);
void comm_lcb_free(void *ptr);

/* barriers */
void comm_sync_all(int *status, int stat_len, char *errmsg,
                   int errmsg_len);
void comm_sync_images(int *image_list, int image_count, int *status,
                      int stat_len, char *errmsg, int errmsg_len);
void comm_sync_memory(int *status, int stat_len, char *errmsg,
                      int errmsg_len);

/* like comm_sync_all, but without the status return */
void comm_barrier_all();

void comm_sync(comm_handle_t hdl);

void comm_fence(size_t proc);

/* atomics */
void comm_swap_request(void *target, void *value, size_t nbytes,
                       int proc, void *retval);
void comm_cswap_request(void *target, void *cond, void *value,
                        size_t nbytes, int proc, void *retval);
void comm_fstore_request(void *target, void *value, size_t nbytes,
                         int proc, void *retval);
void comm_fadd_request(void *target, void *value, size_t nbytes, int proc,
                       void *retval);
void comm_add_request(void *target, void *value, size_t nbytes, int proc);

/* progress */
void comm_service();

/* exit */
void comm_memory_free();
void comm_exit(int status);
void comm_finalize(int exit_code);

#ifdef PCAF_INSTRUMENT
void profile_comm_handle_end(comm_handle_t hdl);
#endif

#endif                          /* _COMM_H */
