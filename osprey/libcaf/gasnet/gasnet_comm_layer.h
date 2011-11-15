/*
 GASNet Communication Layer for supporting Coarray Fortran

 Copyright (C) 2009-2011 University of Houston.

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

#define GASNET_PAR 1
#ifdef IB
#define GASNET_CONDUIT_IBV 1
#else
#define GASNET_CONDUIT_MPI 1
#endif
#define GASNET_VIS_AMPIPE 1
#define GASNET_VIS_MAXCHUNK 512
#define GASNET_VIS_REMOTECONTIG 1

#define ENABLE_LOCAL_MEMCPY
#define MAX_DIMS 15
//#define ENABLE_NONBLOCKING_PUT

#define GASNET_HANDLER_SYNC_REQUEST 128
#define DEFAULT_SHARED_MEMORY_SIZE 30000000L

#ifndef GASNET_COMM_LAYER_H
#define GASNET_COMM_LAYER_H

#include "gasnet.h"
#include "gasnet_tools.h"
#include "gasnet_vis.h"
#include "gasnet_coll.h"

typedef enum {
    static_coarray,
    allocatable_coarray,
    nonsymmetric_coarray,
} memory_segment_type;

/*init*/
void comm_init();


/* inline functions */
unsigned long comm_get_proc_id();
unsigned long comm_get_num_procs();


/* Coarray read/write */
void comm_read(void * src, void *dest, unsigned long xfer_size, unsigned long proc);
void comm_write(void *dest, void *src, unsigned long xfer_size, unsigned long proc);
void comm_read_src_str(void *src, void *dest, unsigned int ndim,
                    unsigned long *src_strides, unsigned long *src_extents,
                    unsigned long proc);
void comm_write_src_str(void *dest, void *src, unsigned int ndim,
                    unsigned long *dest_strides, unsigned long *dest_extents,
                    unsigned long proc);
void comm_read_full_str (void * src, void *dest, unsigned int src_ndim,
        unsigned long *src_strides, unsigned long *src_extents, 
        unsigned int dest_ndim, unsigned long *dest_strides,
        unsigned long *dest_extents, unsigned long proc);
void comm_write_full_str (void * dest, void *src, unsigned int dest_ndim,
        unsigned long *dest_strides, unsigned long *dest_extents, 
        unsigned int src_ndim, unsigned long *src_strides,
        unsigned long *src_extents, unsigned long proc);


/* shared memory management */
static void *get_remote_address(void *src, unsigned long img);
unsigned long allocate_static_coarrays(); /* TBD */

#if defined (ENABLE_NONBLOCKING_PUT)
/* non-blocking put */
struct write_handle_list
{
    gasnet_handle_t handle;
    struct write_handle_list *next;
};
static struct write_handle_list* get_next_handle(unsigned long proc);
static void put_sync(unsigned long proc);
#endif


/* active msg handler for sync images */
void handler_sync_request(gasnet_token_t token, int imageIndex);


/* interrupt safe malloc and free */
void* comm_malloc(size_t size);
void comm_free(void *ptr);


/* Barriers */
void comm_barrier_all();
void comm_sync_images(int *image_list, int image_count);


/* exit */
void comm_memory_free();
void comm_exit(int status);
void comm_finalize();

#endif
