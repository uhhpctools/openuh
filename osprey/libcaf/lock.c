/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2012-2013 University of Houston.

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

#include "comm.h"
#include "alloc.h"
#include "uthash.h"
#include "trace.h"
#include "util.h"
#include "profile.h"


extern unsigned long _this_image;
extern unsigned long _num_images;

struct lock_request {
    volatile int locked;
    volatile int image:20;
    volatile long long ofst:36;
    volatile int done;
};

typedef struct lock_request lock_request_t;

struct lock_req_tbl_item {
    lock_t l;                   /* key */
    lock_request_t *req;
    UT_hash_handle hh;          /* make structure hashable */
};

typedef struct lock_req_tbl_item lock_req_tbl_item_t;

lock_req_tbl_item_t *req_table = NULL;

static inline void *get_shared_mem_address_from_offset(long ofst,
                                                       size_t image)
{
    return comm_start_shared_mem(image - 1) + ofst;
}

static inline unsigned long get_offset_from_shared_mem_address(void *addr)
{
    return addr - comm_start_shared_mem(_this_image - 1);
}

/***********************************************************************
 * Routines which do not handle STAT= and SUCCESS= specifiers
 ***********************************************************************/

void comm_lock(lock_t * lock, int image, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    lock_req_tbl_item_t check, *new_item;
    lock_t p, q;
    lock_request_t *lock_req;
    size_t lock_ofst = get_offset_from_shared_mem_address(lock);

    if (errmsg != NULL && errmsg_len) {
        memset(errmsg, 0, (size_t) errmsg_len);
    }

    check.l.locked = 0;
    check.l.image = image;
    check.l.ofst = lock_ofst;
    HASH_FIND(hh, req_table, &check.l, sizeof(lock_t), new_item);

    if (new_item) {
        /* request item already is in the request table, which means the lock
         * must already be held by this image */
        LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
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
    lock_req->done = 0;

    q.locked = 1;
    q.image = _this_image;
    q.ofst = get_offset_from_shared_mem_address(lock_req);

    LOAD_STORE_FENCE();

    /* p = FETCH-and-STORE(lock, q, image-1) */
    comm_fstore_request(lock, &q, sizeof(q), image - 1, &p);

    if (p.locked != 0) {
        lock_request_t r;
        r.image = _this_image;
        r.ofst = q.ofst;
        r.done = 1;

        lock_req->locked = 1;

        LOAD_STORE_FENCE();

        /* store r.{done,ofst,image} into RMA_MEM[p.ofst+4] at image p.image */
        comm_write(p.image - 1, ((int *)
                                 get_shared_mem_address_from_offset(p.ofst,
                                                                    p.image))
                   + 1, ((int *) &r) + 1, sizeof(r) - sizeof(int), 1,
                   (comm_handle_t *) 0);

        /* wait for locked flag to be reset */
        do {
            comm_service();
        } while (lock_req->locked);

    }

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}                               /* comm_lock */

void comm_unlock(lock_t * lock, int image, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    /*
     * check if lock has actually been acquired first. If not, its an error.
     *   if next on request descriptor is NULL, there is no known successor.
     */
    lock_t key;
    lock_t q;
    lock_request_t *req;
    lock_request_t *s;
    lock_req_tbl_item_t *request_item;
    int i = 0;
    size_t lock_ofst = get_offset_from_shared_mem_address(lock);

    if (errmsg != NULL && errmsg_len) {
        memset(errmsg, 0, (size_t) errmsg_len);
    }

    key.locked = 0;
    key.image = image;
    key.ofst = lock_ofst;
    HASH_FIND(hh, req_table, &key, sizeof(key), request_item);

    if (request_item == NULL) {
        /* request item already is not the request table, which means the lock
         * isn't being held by this image */
        LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
        return;
    }

    req = request_item->req;

    /* if there's a successor, notify that lock is being released */
    if (req->done == 0) {
        /* there doesn't appear to be a successor (yet) */
        lock_t reset;
        q.locked = 1;
        q.image = _this_image;
        q.ofst = get_offset_from_shared_mem_address(req);
        reset.locked = 0;
        reset.image = 0;
        reset.ofst = 0;

        /* q = COMPARE-and-SWAP(lock, q, reset, image-1) */
        comm_cswap_request(lock, &q, &reset, sizeof(reset), image - 1, &q);

        /* confirm that there is no successor */
        if (q.image == _this_image) {
            /* no successor, and lock was reset */
            HASH_DELETE(hh, req_table, request_item);
            coarray_asymmetric_deallocate_(request_item->req);
            free(request_item);
            return;
        }

        /* there was a successor, so wait for them to set 'done' in our
         * request descriptor */
        while (req->done == 0) {
            /* do something useful here! */
            comm_service();
        }
    }

    /* ensure successor image index has been set */
    while (req->image == 0) {
        /* do something useful here! */
        comm_service();
    }


    /* unset 'locked' on successor */
    s = get_shared_mem_address_from_offset(req->ofst, req->image);
    i = 0;
    comm_write(((size_t) (req->image) - 1), s, &i, sizeof(i), 1, NULL);

    /* delete request item from request table */
    HASH_DELETE(hh, req_table, request_item);
    coarray_asymmetric_deallocate_(request_item->req);
    free(request_item);

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}                               /* comm_unlock */

void comm_unlock2(lock_t * lock, int image, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    /*
     * check if lock has actually been acquired first. If not, its an error.
     * if next on request descriptor is NULL, there is no known successor.
     */

    lock_t key;
    lock_t q;
    lock_request_t *req, *s;
    lock_req_tbl_item_t *request_item;
    int i = 0;
    size_t lock_ofst = get_offset_from_shared_mem_address(lock);

    if (errmsg != NULL && errmsg_len) {
        memset(errmsg, 0, (size_t) errmsg_len);
    }

    key.locked = 0;
    key.image = image;
    key.ofst = lock_ofst;
    HASH_FIND(hh, req_table, &key, sizeof(key), request_item);

    if (request_item == NULL) {
        /* request item already is not the request table, which means the lock
         * isn't being held by this image */
        LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
        return;
    }

    req = request_item->req;

    /* if there's a successor, notify that lock is being released */
    if (req->done == 0) {
        /* there doesn't appear to be a successor (yet) */
        lock_t u, v;
        u.locked = 0;
        u.image = 0;
        u.ofst = 0;

        /* v = FETCH-and-STORE(lock, u, image-1) */
        comm_fstore_request(lock, &u, sizeof(u), image - 1, &v);

        /* confirm that there is no successor */
        if (v.image == _this_image) {
            /* no successor, and lock was reset */
            HASH_DELETE(hh, req_table, request_item);
            coarray_asymmetric_deallocate_(request_item->req);
            free(request_item);
            return;
        }

        /* there was at least one successor, but they were removed from the
         * queue
         */

        /* u = FETCH-and-STORE(lock, v, image-1) */
        comm_fstore_request(lock, &v, sizeof(v), image - 1, &u);

        /* there was a successor, so wait for them to set 'done' in our
         * request descriptor */
        while (req->done == 0) {
            /* do something useful here! */
            comm_service();
        }

        /* ensure successor image index has been set */
        while (req->image == 0) {
            /* do something useful here! */
            comm_service();
        }

        if (u.image != 0) {
            /* link victim(s) to the usurper(s) */
            lock_request_t r;
            comm_write(u.image - 1, ((int *)
                                     get_shared_mem_address_from_offset
                                     (u.ofst, u.image)) + 1,
                       ((int *) req) + 1, sizeof(*req) - sizeof(int), 1,
                       (void *) 0);
        } else {

            /* unset locked on successor */
            s = get_shared_mem_address_from_offset(req->ofst, req->image);
            i = 0;
            comm_write(req->image - 1, s, &i, sizeof(i), 1, (void *) 0);
        }

    } else {

        /* ensure successor image index has been set */
        while (req->image == 0) {
            /* do something useful here! */
            comm_service();
        }

        /* req->image should now point to the successor */

        /* unset locked on successor */
        s = get_shared_mem_address_from_offset(req->ofst, req->image);
        i = 0;
        comm_write(req->image - 1, s, &i, sizeof(i), 1, (void *) 0);
    }

    /* delete request item from request table */
    HASH_DELETE(hh, req_table, request_item);
    coarray_asymmetric_deallocate_(request_item->req);
    free(request_item);

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}                               /* comm_unlock2 */


/***********************************************************************
 * Routines which handle STAT= and SUCCESS= specifiers
 ***********************************************************************/

void comm_lock_stat(lock_t * lock, int image, char *success,
                    int success_len, int *status, int stat_len,
                    char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    lock_req_tbl_item_t check, *new_item;
    lock_t p, q;
    lock_request_t *lock_req;
    size_t lock_ofst = get_offset_from_shared_mem_address(lock);

    if (status != NULL) {
        memset(status, 0, (size_t) stat_len);
        *((INT2 *) status) = STAT_SUCCESS;
    }
    if (errmsg != NULL && errmsg_len) {
        memset(errmsg, 0, (size_t) errmsg_len);
    }

    check.l.locked = 0;
    check.l.image = image;
    check.l.ofst = lock_ofst;
    HASH_FIND(hh, req_table, &check.l, sizeof(lock_t), new_item);

    if (new_item) {
        /* request item already is in the request table, which means the lock
         * must already be held by this image */
        if (status != NULL) {
            *((INT2 *) status) = STAT_LOCKED;
        }

        LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
        return;
    }

    if (success != NULL) {
        /* clear success flag */
        memset(success, 0, (size_t) success_len);
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
    lock_req->done = 0;

    q.locked = 1;
    q.image = _this_image;
    q.ofst = get_offset_from_shared_mem_address(lock_req);

    LOAD_STORE_FENCE();


    if (success != NULL) {
#if defined(ARMCI)
        /* ARMCI doesn't support compare-and-swap. So, for now we do an
         * initial check and if it isn't locked then we try to acquire the
         * lock as normal */
        comm_read(image - 1, lock, &p, sizeof(p));

        if (p.locked != 0) {
            *success = 0;
            HASH_DELETE(hh, req_table, new_item);
            coarray_asymmetric_deallocate_(new_item->req);
            free(new_item);
            return;
        }

        /* doesn't appear to be locked, so now try to acquire it */

        /* p = FETCH-and-STORE(lock, q, image-1) */
        comm_fstore_request(lock, &q, sizeof(q), image - 1, &p);
#else                           /* defined(GASNET) */
        lock_t unset;
        unset.locked = 0;
        unset.image = 0;
        unset.ofst = 0;

        /* p = COMPARE-and-SWAP(lock, unset, q, image-1) */
        comm_cswap_request(lock, &unset, &q, sizeof(q), image - 1, &p);

        if (p.locked != 0) {
            *success = 0;
            HASH_DELETE(hh, req_table, new_item);
            coarray_asymmetric_deallocate_(new_item->req);
            free(new_item);
            return;
        }
#endif
    } else {
        /* p = FETCH-and-STORE(lock, q, image-1) */
        comm_fstore_request(lock, &q, sizeof(q), image - 1, &p);
    }

    if (p.locked != 0) {
        lock_request_t r;
        r.image = _this_image;
        r.ofst = q.ofst;
        r.done = 1;

        lock_req->locked = 1;

        LOAD_STORE_FENCE();

        /* store r.{done,ofst,image} into RMA_MEM[p.ofst+4] at image p.image */
        comm_write(p.image - 1, ((int *)
                                 get_shared_mem_address_from_offset(p.ofst,
                                                                    p.image))
                   + 1, ((int *) &r) + 1, sizeof(r) - sizeof(int), 1,
                   (comm_handle_t *) NULL);

        /* wait for locked flag to be reset */
        do {
            comm_service();
        } while (lock_req->locked);

    }

    if (success != NULL)
        *success = 1;

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}                               /* comm_lock_stat */

void comm_unlock_stat(lock_t * lock, int image, int *status,
                      int stat_len, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    /*
     * check if lock has actually been acquired first. If not, its an error.
     *   if next on request descriptor is NULL, there is no known successor.
     */
    lock_t key;
    lock_t q;
    lock_request_t *req;
    lock_request_t *s;
    lock_req_tbl_item_t *request_item;
    int i = 0;
    size_t lock_ofst = get_offset_from_shared_mem_address(lock);

    if (status != NULL) {
        memset(status, 0, (size_t) stat_len);
        *((INT2 *) status) = STAT_SUCCESS;
    }
    if (errmsg != NULL && errmsg_len) {
        memset(errmsg, 0, (size_t) errmsg_len);
    }

    key.locked = 0;
    key.image = image;
    key.ofst = lock_ofst;
    HASH_FIND(hh, req_table, &key, sizeof(key), request_item);

    if (request_item == NULL) {
        /* request item already is not the request table, which means the lock
         * isn't being held by this image */
        if (status != NULL) {
            lock_t p;
            comm_read(image - 1, lock, &p, sizeof(p));

            if (p.locked != 0) {
                *((INT2 *) status) = STAT_LOCKED_OTHER_IMAGE;
            } else {
                *((INT2 *) status) = STAT_UNLOCKED;
            }
        }

        LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
        return;
    }

    req = request_item->req;

    /* if there's a successor, notify that lock is being released */
    if (req->done == 0) {
        /* there doesn't appear to be a successor (yet) */
        lock_t reset;
        q.locked = 1;
        q.image = _this_image;
        q.ofst = get_offset_from_shared_mem_address(req);
        reset.locked = 0;
        reset.image = 0;
        reset.ofst = 0;

        /* q = COMPARE-and-SWAP(lock, q, reset, image-1) */
        comm_cswap_request(lock, &q, &reset, sizeof(reset), image - 1, &q);

        /* confirm that there is no successor */
        if (q.image == _this_image) {
            /* no successor, and lock was reset */
            HASH_DELETE(hh, req_table, request_item);
            coarray_asymmetric_deallocate_(request_item->req);
            free(request_item);
            return;
        }

        /* there was a successor, so wait for them to set 'done' in our
         * request descriptor */
        while (req->done == 0) {
            /* do something useful here! */
            comm_service();
        }
    }

    /* ensure successor image index has been set */
    while (req->image == 0) {
        /* do something useful here! */
        comm_service();
    }

    /* req->image should now point to the successor */

    /* unset 'locked' on successor */
    s = get_shared_mem_address_from_offset(req->ofst, req->image);
    i = 0;
    comm_write(((size_t) (req->image) - 1), s, &i, sizeof(i), 1, NULL);

    /* delete request item from request table */
    HASH_DELETE(hh, req_table, request_item);
    coarray_asymmetric_deallocate_(request_item->req);
    free(request_item);

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}                               /* comm_unlock_stat */

void comm_unlock2_stat(lock_t * lock, int image, int *status,
                       int stat_len, char *errmsg, int errmsg_len)
{
    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "entry");
    /*
     * check if lock has actually been acquired first. If not, its an error.
     * if next on request descriptor is NULL, there is no known successor.
     */

    lock_t key;
    lock_t q;
    lock_request_t *req, *s;
    lock_req_tbl_item_t *request_item;
    int i = 0;
    size_t lock_ofst = get_offset_from_shared_mem_address(lock);

    if (status != NULL) {
        memset(status, 0, (size_t) stat_len);
        *((INT2 *) status) = STAT_SUCCESS;
    }
    if (errmsg != NULL && errmsg_len) {
        memset(errmsg, 0, (size_t) errmsg_len);
    }

    key.locked = 0;
    key.image = image;
    key.ofst = lock_ofst;
    HASH_FIND(hh, req_table, &key, sizeof(key), request_item);

    if (request_item == NULL) {
        /* request item already is not the request table, which means the lock
         * isn't being held by this image */
        if (status != NULL) {
            lock_t p;
            comm_read(image - 1, lock, &p, sizeof(p));

            if (p.locked != 0) {
                *((INT2 *) status) = STAT_LOCKED_OTHER_IMAGE;
            } else {
                *((INT2 *) status) = STAT_UNLOCKED;
            }
        }

        LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
        return;
    }

    req = request_item->req;

    /* if there's a successor, notify that lock is being released */
    if (req->done == 0) {
        /* there doesn't appear to be a successor (yet) */
        lock_t u, v;
        u.locked = 0;
        u.image = 0;
        u.ofst = 0;

        /* v = FETCH-and-STORE(lock, u, image-1) */
        comm_fstore_request(lock, &u, sizeof(u), image - 1, &v);

        /* confirm that there is no successor */
        if (v.image == _this_image) {
            /* no successor, and lock was reset */
            HASH_DELETE(hh, req_table, request_item);
            coarray_asymmetric_deallocate_(request_item->req);
            free(request_item);
            return;
        }

        /* there was at least one successor, but they were removed from the
         * queue
         */

        /* u = FETCH-and-STORE(lock, v, image-1) */
        comm_fstore_request(lock, &v, sizeof(v), image - 1, &u);

        /* there was a successor, so wait for them to set 'done' in our
         * request descriptor */
        while (req->done == 0) {
            /* do something useful here! */
            comm_service();
        }

        /* ensure successor image index has been set */
        while (req->image == 0) {
            /* do something useful here! */
            comm_service();
        }

        if (u.image != 0) {
            /* link victim(s) to the usurper(s) */
            lock_request_t r;
            comm_write(u.image - 1, ((int *)
                                     get_shared_mem_address_from_offset
                                     (u.ofst, u.image)) + 1,
                       ((int *) req) + 1, sizeof(*req) - sizeof(int), 1,
                       NULL);
        } else {
            /* unset locked on successor */
            s = get_shared_mem_address_from_offset(req->ofst, req->image);
            i = 0;
            comm_write(req->image - 1, s, &i, sizeof(i), 1, NULL);
        }

    } else {

        /* ensure successor image index has been set */
        while (req->image == 0) {
            /* do something useful here! */
            comm_service();
        }

        /* unset locked on successor */
        s = get_shared_mem_address_from_offset(req->ofst, req->image);
        i = 0;
        comm_write(req->image - 1, s, &i, sizeof(i), 1, NULL);
    }

    /* delete request item from request table */
    HASH_DELETE(hh, req_table, request_item);
    coarray_asymmetric_deallocate_(request_item->req);
    free(request_item);

    LIBCAF_TRACE(LIBCAF_LOG_SYNC, "exit");
}                               /* comm_unlock2_stat */
