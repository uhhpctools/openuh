/*
 ARMCI Communication Layer for supporting Coarray Fortran

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


#ifndef ARMCI_COMM_LAYER_H
#define ARMCI_COMM_LAYER_H

#include "mpi.h"
#include "armci.h"

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
    armci_hdl_t *handle;
    int rmaid;
    void *address;
    void *local_buf;
    unsigned long size;
    unsigned long proc;
    handle_state_t state;
    access_type_t access_type;
    struct handle_list *prev;
    struct handle_list *next;
};

typedef struct armci_handle_x {
    armci_hdl_t handle;
    struct armci_handle_x *next;
} armci_handle_x_t;

struct nb_handle_manager {
    struct handle_list **handles;
    unsigned long num_handles;
    void **min_nb_address;
    void **max_nb_address;
    armci_handle_x_t *free_armci_handles;
};


/* GET CACHE OPTIMIZATION */
struct cache {
    void *remote_address;
    void *cache_line_address;
    armci_hdl_t *handle;
};


#endif
