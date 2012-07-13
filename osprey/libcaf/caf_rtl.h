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

/* SHARED MEMORY MANAGEMENT */
struct shared_memory_slot{
    void *addr;
    unsigned long size;
    unsigned short feb; //full empty bit. 1=full
    struct shared_memory_slot *next;
    struct shared_memory_slot *prev;
};

/* COMPILER BACK-END INTERFACE */

void __caf_init();

void __caf_finalize();

void __caf_exit(int status);

/* management of local communication buffers */
void __acquire_lcb(unsigned long buf_size, void **ptr);
void __release_lcb(void **ptr);

/* non-strided (contiguous) read and write operations */
void __coarray_read( size_t image, void *src, void *dest, size_t nbytes);
void __coarray_write( size_t image, void *dest, void *src, size_t nbytes);

/* strided, non-contiguous read and write operations */
void __coarray_strided_read ( size_t image,
        void *src, const size_t src_strides[],
        void *dest, const size_t dest_strides[],
        const size_t count[], int stride_levels);

void __coarray_strided_write ( size_t image,
        void *dest, const size_t dest_strides[],
        void *src, const size_t src_strides[],
        const size_t count[], int stride_levels);

/* TODO: vector, non-contiguous read and write operations  */


/* SYNCHRONIZATION INTRINSICS */
void _SYNC_ALL();
void _SYNC_MEMORY();
void _SYNC_IMAGES( int *imageList, int imageCount);
void _SYNC_IMAGES_ALL();

/* IMAGE INQUIRY INTRINSICS */
int   _IMAGE_INDEX(DopeVectorType *diminfo, DopeVectorType *sub);
void  _THIS_IMAGE1(DopeVectorType *ret, DopeVectorType *diminfo);
int   _THIS_IMAGE2(DopeVectorType *diminfo, int* sub);

void  _LCOBOUND_1(DopeVectorType *ret, DopeVectorType *diminfo);
int   _LCOBOUND_2(DopeVectorType *diminfo, int *sub);
void  _UCOBOUND_1(DopeVectorType *ret, DopeVectorType *diminfo);
int   _UCOBOUND_2(DopeVectorType *diminfo, int *sub);

/* critical construct support */
void caf_critical_();
void caf_end_critical_();

void* coarray_allocatable_allocate_(unsigned long var_size);
void* coarray_asymmetric_allocate_(unsigned long var_size);
void coarray_deallocate_(void *var_address);
void coarray_free_all_shared_memory_slots();

#endif
