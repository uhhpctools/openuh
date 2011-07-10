/*
 OpenMP Queue Implementation for Open64's OpenMP runtime library

 Copyright (C) 2008-2011 University of Houston.

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

#include <string.h>
#include "omp_queue.h"

void (*__ompc_queue_init)(omp_queue_t *q, int num_slots);
void (*__ompc_queue_free_slots)(omp_queue_t *q);
int (*__ompc_queue_is_full)(omp_queue_t *q);
int (*__ompc_queue_num_used_slots)(omp_queue_t *q);
omp_queue_item_t (*__ompc_queue_get_head)(omp_queue_t *q);
omp_queue_item_t (*__ompc_queue_get_tail)(omp_queue_t *q);
int (*__ompc_queue_put_tail)(omp_queue_t *q, omp_queue_item_t item);
int (*__ompc_queue_put_head)(omp_queue_t *q, omp_queue_item_t item);
int (*__ompc_queue_cfifo_is_full)(omp_queue_t *q);
int (*__ompc_queue_cfifo_num_used_slots)(omp_queue_t *q);
int (*__ompc_queue_cfifo_put)(omp_queue_t *q, omp_queue_item_t item);
omp_queue_item_t (*__ompc_queue_cfifo_get)(omp_queue_t *q);

/* dequeue implementation using array  */

void __ompc_queue_array_init(omp_queue_t * q, int num_slots)
{
  q->slots = aligned_malloc(
      num_slots * sizeof(omp_queue_slot_t), CACHE_LINE_SIZE);
  Is_True(q->slots != NULL,
      ("__ompc_queue_init: couldn't malloc slots for queue"));
  memset(q->slots, 0, num_slots*sizeof(omp_queue_slot_t));
  q->head = q->tail = q->slots;
  q->num_slots = num_slots;
  q->is_empty = 1;
  q->head_index = q->tail_index = q->used_slots = q->reject =  0;
  __ompc_init_lock(&q->lock1);
  __ompc_init_lock(&q->lock2);
}

void __ompc_queue_array_free_slots(omp_queue_t *q)
{
  Is_True(q->slots != NULL,
      ("__ompc_queue_free_slots: slots already NULL"));
  aligned_free(q->slots);
}

inline int __ompc_queue_array_num_used_slots(omp_queue_t *q)
{
  return q->used_slots;
}


inline int __ompc_queue_array_is_full(omp_queue_t *q)
{
  return q->used_slots == (q->num_slots-1);
}

omp_queue_item_t __ompc_queue_array_get_head(omp_queue_t *q)
{
  omp_queue_item_t item;
  Is_True(q != NULL, ("tried to get head from NULL queue"));

  if (__ompc_queue_is_empty(q)) {
    return NULL;
  }

  __ompc_lock(&q->lock1);

  if (__ompc_queue_is_empty(q)) {
    __ompc_unlock(&q->lock1);
    return NULL;
  }

  q->head_index = (q->head_index + 1) % q->num_slots;
  item = q->slots[q->head_index].item;

  if (--q->used_slots == 0)
    q->is_empty = 1;

  if (q->used_slots != 0) {
    Is_True(q->head_index != q->tail_index, ("queue overflow"));
  }

  __ompc_unlock(&q->lock1);

  return item;
}

omp_queue_item_t __ompc_queue_array_get_tail(omp_queue_t *q)
{
  omp_queue_item_t item;
  Is_True(q != NULL, ("tried to get tail from NULL queue"));

  if (__ompc_queue_is_empty(q)) {
    return NULL;
  }

  __ompc_lock(&q->lock1);

  if (__ompc_queue_is_empty(q)) {
    __ompc_unlock(&q->lock1);
    return NULL;
  }

  item = q->slots[q->tail_index].item;

  q->tail_index = (q->tail_index - 1) % q->num_slots;

  if (--q->used_slots == 0)
    q->is_empty = 1;

  if (q->used_slots != 0) {
    Is_True(q->head_index != q->tail_index, ("queue overflow"));
  }
  __ompc_unlock(&q->lock1);

  return item;
}

int __ompc_queue_array_put_tail(omp_queue_t *q, omp_queue_item_t item)
{
  Is_True(q != NULL, ("tried to put to tail on NULL queue"));

  __ompc_lock(&q->lock1);

  if (__ompc_queue_array_is_full(q)) {
    __ompc_unlock(&q->lock1);
    return 0;
  }

  q->tail_index = (q->tail_index + 1) % q->num_slots;

  q->slots[q->tail_index].item = item;
  ++q->used_slots;
  q->is_empty = 0;

  __ompc_unlock(&q->lock1);

  return 1;
}

int __ompc_queue_array_put_head(omp_queue_t *q, omp_queue_item_t item)
{
  Is_True(q != NULL, ("tried to put to head on NULL queue"));

  __ompc_lock(&q->lock1);

  if (__ompc_queue_array_is_full(q)) {
    __ompc_unlock(&q->lock1);
    return 0;
  }

  q->slots[q->head_index].item = item;

  q->head_index = (q->tail_index - 1) % q->num_slots;
  ++q->used_slots;
  q->is_empty = 0;

  __ompc_unlock(&q->lock1);

  return 1;
}

/* current fifo implementation using array  */

inline int __ompc_queue_cfifo_array_num_used_slots(omp_queue_t *q)
{
  if (q->tail_index == q->head_index)
    return 0;
  else if (q->tail_index > q->head_index)
    return (q->tail_index - q->head_index);
  else
    return (q->num_slots - (q->head_index - q->tail_index));
}


inline int __ompc_queue_cfifo_array_is_full(omp_queue_t *q)
{
  return ((q->tail_index + 1) % q->num_slots) == q->head_index;
}

int __ompc_queue_cfifo_array_put(omp_queue_t *q, omp_queue_item_t item)
{
  int new_tail_index;
  Is_True(q != NULL, ("tried to put to tail on NULL queue"));

  __ompc_lock(&q->lock2);

  new_tail_index = (q->tail_index + 1) % q->num_slots;

  if (new_tail_index == q->head_index) {
    /* queue is full */
    __ompc_unlock(&q->lock2);
    return 0;
  }

  q->slots[new_tail_index].item = item;
  q->tail_index = new_tail_index;
  q->is_empty = 0;

  __ompc_unlock(&q->lock2);

  return 1;
}

omp_queue_item_t __ompc_queue_cfifo_array_get(omp_queue_t *q)
{
  int new_head_index, tail_index;
  omp_queue_item_t item;
  Is_True(q != NULL, ("tried to get head from NULL queue"));

  __ompc_lock(&q->lock1);

  tail_index = q->tail_index;
  if (__ompc_queue_is_empty(q)) {
    /* queue is empty */
    __ompc_unlock(&q->lock1);
    return NULL;
  }

  new_head_index = (q->head_index + 1) % q->num_slots;
  q->head_index = new_head_index;
  item = q->slots[q->head_index].item;

  if (new_head_index == tail_index)
    q->is_empty = 1;

  __ompc_unlock(&q->lock1);

  return item;
}
