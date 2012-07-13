/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2011-2012 University of Houston.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "caf_rtl.h"

#if defined(ARMCI)
#include "armci_comm_layer.h"
#elif defined(GASNET)
#include "gasnet_comm_layer.h"
#endif

#include "uthash.h"
#include "trace.h"
#include "util.h"

extern unsigned long _this_image;
extern unsigned long _num_images;

typedef lock_t lock_request_t;

struct lock_req_tbl_item {
    lock_t l;               /* key */
    lock_request_t *req;
    UT_hash_handle hh;       /* make structure hashable */
};

typedef struct lock_req_tbl_item lock_req_tbl_item_t;

lock_req_tbl_item_t *req_table = NULL;

static inline void *get_heap_address_from_offset(unsigned long ofst,
                                                 size_t image)
{
    return comm_start_heap(image-1) + ofst;
}

static inline unsigned long get_offset_from_heap_address(void * addr)
{
    return addr - comm_start_heap(_this_image-1);
}

void comm_lock(lock_t *lock, int image)
{
    /*
     *  Check that lock isn't already being held. If so, just return.
     *
     *   (1) create request for lock, allocated within non-symmetric RMA heap.
     *   (2) atomically swap request node into lock@image.
     *          request -> predecessor
     *   (3) if predecessor is set (i.e. locked != 0), then set
     *       predecessor->next on image predecessor->locked
     */
    lock_req_tbl_item_t check, *new_item;
    lock_request_t p,q;
    lock_request_t *lock_req;
    size_t lock_ofst = get_offset_from_heap_address(lock);

    check.l.locked = 0;
    check.l.image = image;
    check.l.ofst = lock_ofst;
    HASH_FIND(hh, req_table, &check.l, sizeof(lock_t), new_item);

    if (new_item) {
        /* request item already is in the request table, which means the lock
         * must already be held by this image */
        Warning("LOCK request on a lock that is already held.");
        return;
    }

    /* add hash item for lock request */
    lock_req =
      (lock_request_t *) coarray_asymmetric_allocate_(sizeof(*lock_req));
    new_item = (lock_req_tbl_item_t *) malloc(sizeof(*new_item));
    new_item->l.locked = 0;
    new_item->l.image = image;
    new_item->l.ofst = lock_ofst;
    new_item->req = lock_req;
    HASH_ADD(hh, req_table, l, sizeof(lock_t), new_item);

    lock_req->locked = 0;
    lock_req->image = 0;
    lock_req->ofst = 0;

    q.locked = 1;
    q.image =_this_image;
    q.ofst = get_offset_from_heap_address(lock_req);

    LOAD_STORE_FENCE();

    /* lock_req = FETCH_and_STORE(lock, image-1, q) */
    comm_fstore_request( lock, &q, sizeof(q), image-1, &p);

    if ( p.locked != 0 ) {
        lock_req->locked = 1;
        /* p->address now points to predecessor's request descriptor */
        comm_write( p.image-1, get_heap_address_from_offset(p.ofst,p.image), &q,
                sizeof(q) );

        LOAD_STORE_FENCE();

        do {
            /* do something useful here! */
            comm_service();
        } while (lock_req->locked);
    }
}

void comm_unlock(lock_t *lock, int image)
{
    /*
     * check if lock has actually been acquired first. If not, its an error.
     *   if next on request descriptor is NULL, there is no known successor.
     *
     */
    lock_t key;
    lock_request_t *req, *s, q;
    lock_req_tbl_item_t *request_item;
    int i = 0;
    size_t lock_ofst = get_offset_from_heap_address(lock);
    key.locked = 0;
    key.image = image;
    key.ofst = lock_ofst;
    HASH_FIND(hh, req_table, &key, sizeof(key), request_item);


    if (request_item == NULL) {
        /* request item already is in the request table, which means the lock
         * must already be held by this image */
        Warning("UNLOCK request on a lock that is not already held.");
        return;
    }

    req = request_item->req;
    if (req->image == 0) {
        /* there doesn't appear to be a successor (yet) */
        lock_request_t reset;
        q.locked = 1;
        q.image = _this_image;
        q.ofst = get_offset_from_heap_address(req);
        reset.locked = 0;
        reset.image = 0;
        reset.ofst = 0;

        comm_cswap_request(lock, &q, &reset, sizeof(reset), image-1, &q);

        if (q.image == _this_image) {
            /* no successor, and lock was reset */
            HASH_DELETE(hh, req_table, request_item);
            coarray_deallocate_( request_item->req );
            free( request_item);
            return;
        }

        /* there was a successor, so wait for them to set next in our request
         * descriptor */
        while (req->image == 0) {
            /* do something useful here! */
            comm_service();
        }

        /* next should now point to the successor */
    }

    /* reset locked on successor */
    s = get_heap_address_from_offset(req->ofst, req->image);
    // comm_read(req->image-1, s, &i, sizeof(int) );
    // i &= ~ 0x1; /* unset the last bit */
    i = 0;
    comm_write(req->image-1, s, &i, 1 );

    /* delete request item from request table */
    HASH_DELETE(hh, req_table, request_item);
    coarray_deallocate_( request_item->req );
    free( request_item);
}

void comm_unlock2(lock_t *lock, int image)
{
    /*
     * check if lock has actually been acquired first. If not, its an error.
     *   if next on request descriptor is NULL, there is no known successor.
     *
     */
    lock_t key;
    lock_request_t *req, *s, q;
    lock_req_tbl_item_t *request_item;
    int i = 0;
    size_t lock_ofst = get_offset_from_heap_address(lock);
    key.locked = 0;
    key.image = image;
    key.ofst = lock_ofst;
    HASH_FIND(hh, req_table, &key, sizeof(key), request_item);


    if (request_item == NULL) {
        /* request item already is in the request table, which means the lock
         * must already be held by this image */
        Warning("UNLOCK request on a lock that is not already held.");
        return;
    }

    req = request_item->req;
    if (req->image == 0) {
        /* there doesn't appear to be a successor (yet) */
        lock_request_t u, v;
        u.locked = 0;
        u.image = 0;
        u.ofst = 0;

        comm_fstore_request(lock, &u, sizeof(u), image-1, &v);

        if (v.image == _this_image) {
            /* no successor, and lock was reset */
            HASH_DELETE(hh, req_table, request_item);
            coarray_deallocate_( request_item->req );
            free( request_item);
            return;
        }

        /* there was at least one successor, but they were removed from the
         * queue 
         */
        comm_fstore_request(lock, &v, sizeof(v), image-1, &u);

        while (req->image == 0) {
            /* do something useful here! */
            comm_service();
        }

        if (u.image != 0) {
            /* link victim(s) to the usurper(s) */
            comm_write(u.image-1,
                       get_heap_address_from_offset(u.ofst,u.image),
                       req, sizeof(*req) );
        } else {
            /* reset locked on successor */
            s = get_heap_address_from_offset(req->ofst, req->image);
            i = 0; 
            comm_write(req->image-1, s, &i, 1 );
        }

    } else {

        /* reset locked on successor */
        s = get_heap_address_from_offset(req->ofst, req->image);
        i = 0; 
        comm_write(req->image-1, s, &i, 1 );
    }

    /* delete request item from request table */
    HASH_DELETE(hh, req_table, request_item);
    coarray_deallocate_( request_item->req );
    free( request_item);
}