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

#include "dopevec.h"

void caf_init_();

/* shared memory management */
struct shared_memory_slot{
    void *addr;
    unsigned long size;
    unsigned short feb; //full empty bit. 1=full
    struct shared_memory_slot *next;
    struct shared_memory_slot *prev;
};
void* coarray_allocatable_allocate_(unsigned long var_size);
void* coarray_asymmetric_allocate_(unsigned long var_size);
void coarray_deallocate_(void *var_address);
void coarray_free_all_shared_memory_slots();

/* synchronization */
void sync_all_();
void sync_memory_();
void sync_images_( int *imageList, int imageCount);
void sync_images_all_();

/* critical construct support */
void caf_critical_();
void caf_end_critical_();

/* image inquiry */
int image_index_(DopeVectorType *diminfo, DopeVectorType *sub);
int this_image3_(DopeVectorType *diminfo, int* sub);
void this_image2_(DopeVectorType *ret, DopeVectorType *diminfo);
int lcobound2_(DopeVectorType *diminfo, int *sub);
void lcobound_(DopeVectorType *ret, DopeVectorType *diminfo);
int ucobound2_(DopeVectorType *diminfo, int *sub);
void ucobound_(DopeVectorType *ret, DopeVectorType *diminfo);

/* management of local communication buffers */
void acquire_lcb_(unsigned long buf_size, void **ptr);
void release_lcb_(void **ptr);

/* non-strided (contiguous) read and write operations */
void coarray_read_( size_t image, void *src, void *dest, size_t nbytes);
void coarray_write_( size_t image, void *dest, void *src, size_t nbytes);

/* strided, non-contiguous read and write operations */
void coarray_strided_read_ ( size_t image,
        void *src, const size_t src_strides[],
        void *dest, const size_t dest_strides[],
        const size_t count[], int stride_levels);

void coarray_strided_write_ ( size_t image,
        void *dest, const size_t dest_strides[],
        void *src, const size_t src_strides[],
        const size_t count[], int stride_levels);

/* TODO: vector, non-contiguous read and write operations  */

/* Collectives */
void comm_cosum(DopeVectorType *src_dv, DopeVectorType *sum_dv,int root);

void caf_exit_(int status);
void caf_finalize_();

#endif
