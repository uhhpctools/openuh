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


#ifndef CAF_RTL_H
#define CAF_RTL_H

#include <ctype.h>
#include "lock.h"
#include "team.h"
#include "dopevec.h"

#define LOAD_STORE_FENCE() __sync_synchronize()
#define SYNC_FETCH_AND_ADD(t,v) __sync_fetch_and_add(t,v)
#define SYNC_FETCH_AND_AND(t,v) __sync_fetch_and_and(t,v)
#define SYNC_FETCH_AND_OR(t,v) __sync_fetch_and_or(t,v)
#define SYNC_FETCH_AND_XOR(t,v) __sync_fetch_and_xor(t,v)
#define SYNC_SWAP(t,v) __sync_lock_test_and_set(t,v)
#define SYNC_CSWAP(t,u,v) __sync_val_compare_and_swap(t,u,v)

typedef int8_t INT1;
typedef int16_t INT2;
typedef int32_t INT4;
typedef int64_t INT8;

typedef INT4 event_t;
typedef INT4 atomic_t;
typedef INT4 atomic4_t;
typedef INT8 atomic8_t;

typedef void *comm_handle_t;


/* COMPILER BACK-END INTERFACE */

void __caf_init();

void __caf_finalize(int exit_code);

void __caf_exit(int status);

/* ensures local/remote completion of communication */
void __coarray_wait_all();
void __coarray_wait(comm_handle_t *hdl);

void __target_alloc(unsigned long buf_size, void **ptr);
void __target_dealloc(void **ptr);
void *__target_alloc2(unsigned long buf_size, void *orig_ptr);

/* management of local communication buffers */
void __acquire_lcb(unsigned long buf_size, void **ptr);
void __release_lcb(void **ptr);

/* non-strided (contiguous) read and write operations */
void __coarray_nbread(size_t image, void *src, void *dest, size_t nbytes,
                      comm_handle_t * hdl);
void __coarray_read(size_t image, void *src, void *dest, size_t nbytes);

void __coarray_write_from_lcb(size_t image, void *dest, void *src,
                              size_t nbytes, int ordered,
                              comm_handle_t * hdl);
void __coarray_write(size_t image, void *dest, void *src, size_t nbytes,
                     int ordered, comm_handle_t * hdl);

/* strided, non-contiguous read and write operations */
void __coarray_strided_nbread(size_t image,
                              void *src, const size_t src_strides[],
                              void *dest, const size_t dest_strides[],
                              const size_t count[], int stride_levels,
                              comm_handle_t * hdl);

void __coarray_strided_read(size_t image,
                            void *src, const size_t src_strides[],
                            void *dest, const size_t dest_strides[],
                            const size_t count[], int stride_levels);

void __coarray_strided_write_from_lcb(size_t image,
                                      void *dest,
                                      const size_t dest_strides[],
                                      void *src,
                                      const size_t src_strides[],
                                      const size_t count[],
                                      int stride_levels,
                                      int ordered, comm_handle_t * hdl);

void __coarray_strided_write(size_t image,
                             void *dest,
                             const size_t dest_strides[],
                             void *src,
                             const size_t src_strides[],
                             const size_t count[],
                             int stride_levels, int ordered,
                             comm_handle_t * hdl);

/* TODO: vector, non-contiguous read and write operations  */

/* COMPILER FRONT-END INTERFACE */

/* SYNCHRONIZATION INTRINSICS */
void _SYNC_ALL(int *status, int stat_len, char *errmsg, int errmsg_len);
void _SYNC_MEMORY(int *status, int stat_len, char *errmsg, int errmsg_len);
void _SYNC_IMAGES(int images[], int image_count, int *status, int stat_len,
                  char *errmsg, int errmsg_len);
void _SYNC_IMAGES_ALL(int *status, int stat_len, char *errmsg,
                      int errmsg_len);
void _SYNC_TEAM(team_type *team_p, int *status, int stat_len, char *errmsg,
                int errmsg_len);

/* IMAGE INQUIRY INTRINSICS */
int _NUM_IMAGES1(team_type *team);
int _NUM_IMAGES2(int *team_id);

int _IMAGE_INDEX(DopeVectorType * diminfo, DopeVectorType * sub);

int _IMAGE_INDEX1(DopeVectorType * diminfo, DopeVectorType * sub,
                 team_type *team);

int _IMAGE_INDEX2(DopeVectorType * diminfo, DopeVectorType * sub,
                  int *team_id);

int _THIS_IMAGE0(team_type *team);

void _THIS_IMAGE1(DopeVectorType *ret, DopeVectorType *diminfo, team_type *team);

int _THIS_IMAGE2(DopeVectorType * diminfo, int *sub, team_type *team);

void _LCOBOUND_1(DopeVectorType * ret, DopeVectorType * diminfo);
int _LCOBOUND_2(DopeVectorType * diminfo, int *sub);
void _UCOBOUND_1(DopeVectorType * ret, DopeVectorType * diminfo);
int _UCOBOUND_2(DopeVectorType * diminfo, int *sub);

/* LOCKS SUPPORT */
void _COARRAY_LOCK(lock_t * lock, const int *image, char *success,
                   int success_len, int *status, int stat_len,
                   char *errmsg, int errmsg_len);
void _COARRAY_UNLOCK(lock_t * lock, const int *image, int *status,
                     int stat_len, char *errmsg, int errmsg_len);

/* ATOMIC INTRINSICS */
void _ATOMIC_DEFINE_4_1(atomic4_t * atom, INT1 * value, int *image);
void _ATOMIC_DEFINE_4_2(atomic4_t * atom, INT2 * value, int *image);
void _ATOMIC_DEFINE_4_4(atomic4_t * atom, INT4 * value, int *image);
void _ATOMIC_DEFINE_4_8(atomic4_t * atom, INT8 * value, int *image);
void _ATOMIC_DEFINE_8_1(atomic8_t * atom, INT1 * value, int *image);
void _ATOMIC_DEFINE_8_2(atomic8_t * atom, INT2 * value, int *image);
void _ATOMIC_DEFINE_8_4(atomic8_t * atom, INT4 * value, int *image);
void _ATOMIC_DEFINE_8_8(atomic8_t * atom, INT8 * value, int *image);

void _ATOMIC_REF_4_1(INT1 * value, atomic4_t * atom, int *image);
void _ATOMIC_REF_4_2(INT2 * value, atomic4_t * atom, int *image);
void _ATOMIC_REF_4_4(INT4 * value, atomic4_t * atom, int *image);
void _ATOMIC_REF_4_8(INT8 * value, atomic4_t * atom, int *image);
void _ATOMIC_REF_8_1(INT1 * value, atomic8_t * atom, int *image);
void _ATOMIC_REF_8_2(INT2 * value, atomic8_t * atom, int *image);
void _ATOMIC_REF_8_4(INT4 * value, atomic8_t * atom, int *image);
void _ATOMIC_REF_8_8(INT8 * value, atomic8_t * atom, int *image);

void _ATOMIC_ADD_4_1(atomic4_t * atom, INT1 * value, atomic4_t *old, int *image);
void _ATOMIC_ADD_4_2(atomic4_t * atom, INT2 * value, atomic4_t *old, int *image);
void _ATOMIC_ADD_4_4(atomic4_t * atom, INT4 * value, atomic4_t *old, int *image);
void _ATOMIC_ADD_4_8(atomic4_t * atom, INT8 * value, atomic4_t *old, int *image);
void _ATOMIC_ADD_8_1(atomic8_t * atom, INT1 * value, atomic8_t *old, int *image);
void _ATOMIC_ADD_8_2(atomic8_t * atom, INT2 * value, atomic8_t *old, int *image);
void _ATOMIC_ADD_8_4(atomic8_t * atom, INT4 * value, atomic8_t *old, int *image);
void _ATOMIC_ADD_8_8(atomic8_t * atom, INT8 * value, atomic8_t *old, int *image);

void _ATOMIC_AND_4_1(atomic4_t * atom, INT1 * value, atomic4_t *old, int *image);
void _ATOMIC_AND_4_2(atomic4_t * atom, INT2 * value, atomic4_t *old, int *image);
void _ATOMIC_AND_4_4(atomic4_t * atom, INT4 * value, atomic4_t *old, int *image);
void _ATOMIC_AND_4_8(atomic4_t * atom, INT8 * value, atomic4_t *old, int *image);
void _ATOMIC_AND_8_1(atomic8_t * atom, INT1 * value, atomic8_t *old, int *image);
void _ATOMIC_AND_8_2(atomic8_t * atom, INT2 * value, atomic8_t *old, int *image);
void _ATOMIC_AND_8_4(atomic8_t * atom, INT4 * value, atomic8_t *old, int *image);
void _ATOMIC_AND_8_8(atomic8_t * atom, INT8 * value, atomic8_t *old, int *image);

void _ATOMIC_OR_4_1(atomic4_t * atom, INT1 * value, atomic4_t *old, int *image);
void _ATOMIC_OR_4_2(atomic4_t * atom, INT2 * value, atomic4_t *old, int *image);
void _ATOMIC_OR_4_4(atomic4_t * atom, INT4 * value, atomic4_t *old, int *image);
void _ATOMIC_OR_4_8(atomic4_t * atom, INT8 * value, atomic4_t *old, int *image);
void _ATOMIC_OR_8_1(atomic8_t * atom, INT1 * value, atomic8_t *old, int *image);
void _ATOMIC_OR_8_2(atomic8_t * atom, INT2 * value, atomic8_t *old, int *image);
void _ATOMIC_OR_8_4(atomic8_t * atom, INT4 * value, atomic8_t *old, int *image);
void _ATOMIC_OR_8_8(atomic8_t * atom, INT8 * value, atomic8_t *old, int *image);

void _ATOMIC_XOR_4_1(atomic4_t * atom, INT1 * value, atomic4_t *old, int *image);
void _ATOMIC_XOR_4_2(atomic4_t * atom, INT2 * value, atomic4_t *old, int *image);
void _ATOMIC_XOR_4_4(atomic4_t * atom, INT4 * value, atomic4_t *old, int *image);
void _ATOMIC_XOR_4_8(atomic4_t * atom, INT8 * value, atomic4_t *old, int *image);
void _ATOMIC_XOR_8_1(atomic8_t * atom, INT1 * value, atomic8_t *old, int *image);
void _ATOMIC_XOR_8_2(atomic8_t * atom, INT2 * value, atomic8_t *old, int *image);
void _ATOMIC_XOR_8_4(atomic8_t * atom, INT4 * value, atomic8_t *old, int *image);
void _ATOMIC_XOR_8_8(atomic8_t * atom, INT8 * value, atomic8_t *old, int *image);


void _ATOMIC_CAS(atomic_t * atom, atomic_t * oldval, atomic_t *compare,
                 atomic_t *newval, int *image);

/* EVENTS SUPPORT */
void _EVENT_POST(event_t * event, int *image);
void _EVENT_QUERY(event_t * event, int *image, char *state, int state_len);
void _EVENT_WAIT(event_t * event, int *image);

/* critical construct support */
void _CRITICAL();
void _END_CRITICAL();

/* OTHER PUBLIC INTERFACES */
void coarray_translate_remote_addr(void **remote_addr, int image);

/* runtime checks */
int check_remote_address(size_t, void *);
int check_remote_image(size_t);
int check_remote_image_initial_team(size_t);

/* UHCAF library routines */
void uhcaf_check_comms(void);

#endif
