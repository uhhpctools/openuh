/*
 GASNet Communication Layer for supporting Coarray Fortran

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

#ifndef GASNET_COMM_LAYER_H
#define GASNET_COMM_LAYER_H

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
#define DEFAULT_SHARED_MEMORY_SIZE 31457280L
#define DEFAULT_GETCACHE_LINE_SIZE 65536L


/* GASNET handler IDs */
enum {
  GASNET_HANDLER_SYNC_REQUEST = 128,
  GASNET_HANDLER_CRITICAL_REQUEST = 129,
  GASNET_HANDLER_CRITICAL_REPLY = 130,
  GASNET_HANDLER_END_CRITICAL_REQUEST = 131
};

#define GASNET_Safe(fncall) do {                                      \
    int _retval;                                                        \
    if ((_retval = fncall) != GASNET_OK) {                              \
          fprintf(stderr, "ERROR calling: %s\n"                              \
                                  " at: %s:%i\n"                                    \
                                  " error: %s (%s)\n",                              \
                             #fncall, __FILE__, __LINE__,                           \
                             gasnet_ErrorName(_retval), gasnet_ErrorDesc(_retval));  \
           fflush(stderr);                                                  \
           gasnet_exit(_retval);                                            \
         }                                                                  \
   } while(0)

#define BARRIER() do {                                              \
    gasnet_barrier_notify(0,GASNET_BARRIERFLAG_ANONYMOUS);            \
    GASNET_Safe(gasnet_barrier_wait(0,GASNET_BARRIERFLAG_ANONYMOUS)); \
} while (0) 


#include "gasnet.h"
#include "gasnet_tools.h"
#include "gasnet_vis.h"
#include "gasnet_coll.h"

typedef enum {
    static_coarray,
    allocatable_coarray,
    nonsymmetric_coarray,
} memory_segment_type;

typedef struct {
  unsigned long remote_proc;
  unsigned int remote_ndim;
  void *remote_addr;
  unsigned long *remote_str_mults;
  unsigned long *remote_extents;
  unsigned long *remote_strides;
  unsigned long local_proc;
  unsigned int local_ndim;
  void *local_addr;
  unsigned long *local_str_mults;
  unsigned long *local_extents;
  unsigned long *local_strides;
} array_section_t;

/*init*/
void comm_init();


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
unsigned long allocate_static_coarrays(); /* TBD */

/* NON-BLOCKING PUT OPTIMIZATION */
struct write_handle_list
{
    gasnet_handle_t handle;
    void *address;
    unsigned long size;
    struct write_handle_list *prev;
    struct write_handle_list *next;
};
struct local_buffer
{
    void *addr;
    struct local_buffer *next;
};

/* GET CACHE OPTIMIZATION */
struct cache
{
    void *remote_address;
    void *cache_line_address;
    gasnet_handle_t handle;
};

/* interrupt safe malloc and free */
void* comm_malloc(size_t size);
void comm_free(void *ptr);
void comm_free_lcb(void *ptr);


/* Barriers */
void comm_barrier_all();
void comm_sync_images(int *image_list, int image_count);

void comm_critical();
void comm_end_critical();


/* exit */
void comm_memory_free();
void comm_exit(int status);
void comm_finalize();

#endif
