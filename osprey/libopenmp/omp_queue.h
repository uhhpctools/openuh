/*
 OpenMP Queue Implementation for Open64's OpenMP runtime library

 Copyright (C) 2011 University of Houston.

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

#ifndef __omp_queue_included
#define __omp_queue_included

#include "omp_lock.h"
#include "omp_sys.h"

typedef void * omp_queue_item_t;

struct omp_queue_slot {
  omp_queue_item_t item;

  /* for lists */
  struct omp_queue_slot *next;
  struct omp_queue_slot *prev;
  int used;
};
typedef struct omp_queue_slot omp_queue_slot_t;

struct omp_queue {
  omp_queue_slot_t *slots;

  int num_slots;
  volatile unsigned int head_index;
  volatile unsigned int tail_index;

  /* for lists */
  volatile omp_queue_slot_t *head;
  volatile omp_queue_slot_t *tail;

  /* internal use only */
  ompc_lock_t lock1; /* global or head lock */
  ompc_lock_t lock2; /* tail lock */
  volatile int reject;
  volatile int is_empty;
  volatile int used_slots;
} __attribute__ ((__aligned__(CACHE_LINE_SIZE)));
typedef struct omp_queue omp_queue_t;

/* inline functions */
static inline int __ompc_queue_is_empty(omp_queue_t *q)
{
  return q->is_empty;
}

static inline int __ompc_queue_num_slots(omp_queue_t *q)
{
  return (q->num_slots);
}

/* external interface */

/* function pointers */
extern void
(*__ompc_queue_init)(omp_queue_t *q, int num_slots);
extern void
(*__ompc_queue_free_slots)(omp_queue_t *q);
extern int
(*__ompc_queue_is_full)(omp_queue_t *q);
extern int
(*__ompc_queue_num_used_slots)(omp_queue_t *q);
extern omp_queue_item_t
(*__ompc_queue_get_head)(omp_queue_t *q);
extern omp_queue_item_t
(*__ompc_queue_get_tail)(omp_queue_t *q);
extern int
(*__ompc_queue_put_tail)(omp_queue_t *q, omp_queue_item_t item);
extern int
(*__ompc_queue_put_head)(omp_queue_t *q, omp_queue_item_t item);
extern omp_queue_item_t
(*__ompc_queue_transfer_chunk_from_head)(omp_queue_t *src, omp_queue_t *dst,
                                         int chunk_size);
extern int
(*__ompc_queue_cfifo_is_full)(omp_queue_t *q);
extern int
(*__ompc_queue_cfifo_num_used_slots)(omp_queue_t *q);
extern int
(*__ompc_queue_cfifo_put)(omp_queue_t *q, omp_queue_item_t item);
extern omp_queue_item_t
(*__ompc_queue_cfifo_get)(omp_queue_t *q);
extern omp_queue_item_t
(*__ompc_queue_cfifo_transfer_chunk)(omp_queue_t *src, omp_queue_t *dst,
                                     int chunk_size);

/* implementation */

/* array implementation */
extern void __ompc_queue_array_init(omp_queue_t *q, int num_slots);
extern void __ompc_queue_array_free_slots(omp_queue_t *q);
extern int __ompc_queue_array_is_full(omp_queue_t *q);
extern int __ompc_queue_array_num_used_slots(omp_queue_t *q);
extern omp_queue_item_t __ompc_queue_array_get_head(omp_queue_t *q);
extern omp_queue_item_t __ompc_queue_array_get_tail(omp_queue_t *q);
extern int __ompc_queue_array_put_tail(omp_queue_t *q, omp_queue_item_t item);
extern int __ompc_queue_array_put_head(omp_queue_t *q, omp_queue_item_t item);
extern omp_queue_item_t
__ompc_queue_array_transfer_chunk_from_head_to_empty(omp_queue_t *src,
                                                     omp_queue_t *dst,
                                                     int chunk_size);
extern int __ompc_queue_cfifo_array_is_full(omp_queue_t *q);
extern int __ompc_queue_cfifo_array_num_used_slots(omp_queue_t *q);
extern int __ompc_queue_cfifo_array_put(omp_queue_t *q, omp_queue_item_t item);
extern omp_queue_item_t __ompc_queue_cfifo_array_get(omp_queue_t *q);
extern omp_queue_item_t
__ompc_queue_cfifo_array_transfer_chunk_to_empty(omp_queue_t *src,
                                                 omp_queue_t *dst,
                                                 int chunk_size);

/* dyn_array implementation */

extern int
__ompc_queue_dyn_array_put_tail(omp_queue_t *q, omp_queue_item_t item);

extern int
__ompc_queue_dyn_array_put_head(omp_queue_t *q, omp_queue_item_t item);

extern int
__ompc_queue_cfifo_dyn_array_put(omp_queue_t *q, omp_queue_item_t item);


#endif /* __omp_queue_included */
