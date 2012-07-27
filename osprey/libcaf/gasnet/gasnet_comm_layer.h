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

#include "gasnet.h"
#include "gasnet_tools.h"
#include "gasnet_vis.h"
#include "gasnet_coll.h"


/* GASNET handler IDs */
enum {
  GASNET_HANDLER_SYNC_REQUEST = 128,
  GASNET_HANDLER_SWAP_REQUEST = 129,
  GASNET_HANDLER_SWAP_REPLY = 130,
  GASNET_HANDLER_CSWAP_REQUEST = 131,
  GASNET_HANDLER_CSWAP_REPLY = 132,
  GASNET_HANDLER_FADD_REQUEST = 133,
  GASNET_HANDLER_FADD_REPLY = 134
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

typedef enum {
    static_coarray,
    allocatable_coarray,
    nonsymmetric_coarray,
} memory_segment_type;


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

#endif
