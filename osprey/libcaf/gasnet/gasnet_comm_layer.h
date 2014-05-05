/*
 GASNet Communication Layer for supporting Coarray Fortran

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

#ifndef GASNET_COMM_LAYER_H
#define GASNET_COMM_LAYER_H

#define GASNET_PAR 1
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
    GASNET_HANDLER_FOP_REPLY = 133,
    GASNET_HANDLER_FADD_REQUEST = 134,
    GASNET_HANDLER_ADD_REQUEST = 135,
    GASNET_HANDLER_FAND_REQUEST = 136,
    GASNET_HANDLER_AND_REQUEST = 137,
    GASNET_HANDLER_FOR_REQUEST = 138,
    GASNET_HANDLER_OR_REQUEST = 139,
    GASNET_HANDLER_FXOR_REQUEST = 140,
    GASNET_HANDLER_XOR_REQUEST = 141,
    GASNET_HANDLER_PUT_REQUEST = 142,
    GASNET_HANDLER_PUT_REPLY = 143,
    GASNET_HANDLER_GET_REQUEST = 144,
    GASNET_HANDLER_GET_REPLY = 145,
    GASNET_HANDLER_ATOMIC_STORE_REQUEST = 146
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

typedef enum {
    PUTS = 0,
    GETS = 1
} access_type_t;

typedef enum {
    INTERNAL = 0,
    EXPOSED = 1,
    STALE = 2
} handle_state_t;

struct handle_list {
    gasnet_handle_t handle;
    void *address;
    void *local_buf;
    unsigned long size;
    unsigned long proc;
    access_type_t access_type;
    void *final_dest;
    int rmaid;
    handle_state_t state;
    struct handle_list *prev;
    struct handle_list *next;
};

struct nb_handle_manager {
    struct handle_list **handles;
    unsigned long num_handles;
    void **min_nb_address;
    void **max_nb_address;
};

/* GET CACHE OPTIMIZATION */
struct cache {
    void *remote_address;
    void *cache_line_address;
    gasnet_handle_t handle;
};

#endif
