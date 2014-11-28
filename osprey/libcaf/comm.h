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


#include "team.h"
#include "uthash.h"

/* use this if we want the runtime to skip over duplicate images in the sync
 * images list, with a small performance penalty */
// #define SYNC_IMAGES_HASHED

#define ENABLE_LOCAL_MEMCPY
#define MAX_DIMS 15


#define MAX_NUM_IMAGES                    0x100000
#define MAX_SHARED_MEMORY_SIZE            0x1000000000


/* status codes for synchronization statements */

enum {
    STAT_SUCCESS = 0,
    /* these should correspond to the same values define in
     * ../libfi/mathlb/iso_fortran_env.F90 */
    STAT_LOCKED = 101,
    STAT_UNLOCKED = 102,
    STAT_LOCKED_OTHER_IMAGE = 103,
    STAT_STOPPED_IMAGE = 104
} status_codes;

/* different types of atomic operations */
typedef enum {
  ATOMIC_ADD = 0,
  ATOMIC_AND = 1,
  ATOMIC_OR = 2,
  ATOMIC_XOR = 3
} atomic_op_t;

/* different types of sync images algorithms */
typedef enum {
  SYNC_COUNTER = 0,
  SYNC_PING_PONG = 1,
  SYNC_SENSE_REV = 2,
  SYNC_CSR = 3,
  SYNC_IMAGES_DEFAULT = 2
} sync_images_t;

typedef enum {
  BAR_DISSEM = 0,
  BAR_2LEVEL_MULTIFLAG = 1,
  BAR_2LEVEL_SHAREDCOUNTER = 2,
  TEAM_BAR_DEFAULT = 0
} team_barrier_t;

/* different types of rma ordering strategies */
typedef enum {
  RMA_BLOCKING = 0,
  RMA_PUT_ORDERED = 1,
  RMA_PUT_IMAGE_ORDERED = 2,
  RMA_PUT_ADDRESS_ORDERED = 3,
  RMA_RELAXED = 4,
  RMA_ORDERING_DEFAULT = RMA_PUT_ADDRESS_ORDERED
} rma_ordering_t;

typedef struct {
    int image_id;
    UT_hash_handle hh;
} hashed_image_list_t;

/* init */
void comm_init();

/* critical support */
void comm_get_signal(int **signal);
void comm_critical();
void comm_end_critical();


/* proc query functions */
size_t comm_get_proc_id();
size_t comm_get_num_procs();

size_t comm_get_node_id(size_t proc);

void *comm_get_sharedptr(void *addr, size_t proc);

/* non-strided (contiguous) read and write operations */
void comm_nbread(size_t proc, void *src, void *dest, size_t nbytes,
                 comm_handle_t * hdl);
void comm_read(size_t proc, void *src, void *dest, size_t nbytes);
void comm_write_from_lcb(size_t proc, void *dest, void *src, size_t nbytes,
                         int ordered, comm_handle_t * hdl);
void comm_write(size_t proc, void *dest, void *src,
                size_t nbytes, int ordered, comm_handle_t * hdl);
void comm_nbi_write(size_t proc, void *dest, void *src, size_t nbytes);
void comm_write_x(size_t proc, void *dest, void *src, size_t nbytes);

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

void comm_atomic_define(size_t proc, INT4 *atom, INT4 val);
void comm_atomic8_define(size_t proc, INT8 *atom, INT8 val);

void comm_atomic_ref(INT4 *val, size_t proc, INT4 *atom);
void comm_atomic8_ref(INT8 *val, size_t proc, INT8 *atom);

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
void comm_sync_team(team_type_t *team, int *status, int stat_len, char *errmsg,
                   int errmsg_len);


#ifdef SYNC_IMAGES_HASHED
void comm_sync_images(hashed_image_list_t *image_list, int image_count,
                      int *status, int stat_len, char *errmsg,
                      int errmsg_len);
#else
void comm_sync_images(int *image_list, int image_count,
                      int *status, int stat_len, char *errmsg,
                      int errmsg_len);
#endif

void comm_sync_memory(int *status, int stat_len, char *errmsg,
                      int errmsg_len);

/* like comm_sync_all, but without the status return */
void comm_barrier_all();

void comm_sync(comm_handle_t hdl);

void comm_new_exec_segment();
void comm_fence(size_t proc);
void comm_fence_all();

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
void comm_fand_request(void *target, void *value, size_t nbytes, int proc,
                       void *retval);
void comm_and_request(void *target, void *value, size_t nbytes, int proc);
void comm_for_request(void *target, void *value, size_t nbytes, int proc,
                       void *retval);
void comm_or_request(void *target, void *value, size_t nbytes, int proc);
void comm_fxor_request(void *target, void *value, size_t nbytes, int proc,
                       void *retval);
void comm_xor_request(void *target, void *value, size_t nbytes, int proc);

/* progress */
void comm_service();

void comm_poll_char_while_nonzero(char *);
void comm_poll_char_while_zero(char *);

/* exit */
void comm_memory_free();
void comm_exit(int status);
void comm_finalize(int exit_code);

#ifdef PCAF_INSTRUMENT
void profile_comm_handle_end(comm_handle_t hdl);
#endif

#endif                          /* _COMM_H */
