/*
 ARMCI Communication Layer for supporting Coarray Fortran

 Copyright (C) 2009-2014 University of Houston.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

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
