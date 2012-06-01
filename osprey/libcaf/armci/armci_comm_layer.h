/*
 ARMCI Communication Layer for supporting Coarray Fortran

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


#ifndef ARMCI_COMM_LAYER_H
#define ARMCI_COMM_LAYER_H

#include "mpi.h"
#include "armci.h"
#include "trace.h"
#include "caf_rtl.h"

#define ENABLE_LOCAL_MEMCPY
#define MAX_DIMS 15

#define DEFAULT_SHARED_MEMORY_SIZE 31457280L
#define DEFAULT_GETCACHE_LINE_SIZE 65536L

/* init */
void comm_init();


/* critical support */
void comm_get_signal(int** signal);
void comm_critical();
void comm_end_critical();



/* inline functions */
unsigned long comm_get_proc_id();
unsigned long comm_get_num_procs();

/* non-strided (contiguous) read and write operations */
void comm_read( size_t proc, void *src, void *dest, size_t nbytes);
void comm_write( size_t proc, void *dest, void *src, size_t nbytes);

/* strided, non-contiguous read and write operations */
void comm_strided_read ( size_t proc,
        void *src, const size_t src_strides[],
        void *dest, const size_t dest_strides[],
        const size_t count[], size_t stride_levels);

void comm_strided_write ( size_t proc,
        void *dest, const size_t dest_strides[],
        void *src, const size_t src_strides[],
        const size_t count[], size_t stride_levels);

/* TODO: vector, non-contiguous read and write operations  */

/* shared memory management */
unsigned long allocate_static_coarrays(); /*TBD */

/* GET CACHE OPTIMIZATION */
struct cache
{
    void *remote_address;
    void *cache_line_address;
    armci_hdl_t *handle;
};

/* malloc & free */
void* comm_malloc(size_t size);
void comm_free(void* ptr);
void comm_free_lcb(void* ptr);

/* barriers */
void comm_barrier_all();
void comm_sync_images(int *image_list, int image_count);

/* exit */
void comm_memory_free();
void comm_exit(int status);
void comm_finalize();

#endif
