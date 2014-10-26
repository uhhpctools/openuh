/*
 OpenMP Queue Implementation for Open64's OpenMP runtime library

 Copyright (C) 2014 University of Houston.

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

#ifndef __omp_queue_included
#define __omp_queue_included

#include "omp_lock.h"
#include "omp_sys.h"

typedef enum {
  ARRAY_QUEUE_STORAGE,
  DYN_ARRAY_QUEUE_STORAGE,
  LIST_QUEUE_STORAGE,
  LOCKLESS_QUEUE_STORAGE
} queue_storage_t;

extern queue_storage_t __omp_queue_storage;

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
(*__ompc_queue_steal_head)(omp_queue_t *q);
extern omp_queue_item_t
(*__ompc_queue_steal_tail)(omp_queue_t *q);
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

extern int
(*__ompc_queue_is_empty)(omp_queue_t *q);

/* implementation */

/* used by array, dyn_array, and list implementations */
extern int __ompc_queue_check_is_empty(omp_queue_t *q);

/* used by lockless implementation */
extern int __ompc_queue_lockless_is_empty(omp_queue_t *q);

/* array implementation */
extern void __ompc_queue_array_init(omp_queue_t *q, int num_slots);
extern void __ompc_queue_array_free_slots(omp_queue_t *q);
extern int __ompc_queue_array_is_full(omp_queue_t *q);
extern int __ompc_queue_array_num_used_slots(omp_queue_t *q);
extern omp_queue_item_t __ompc_queue_array_steal_head(omp_queue_t *q);
extern omp_queue_item_t __ompc_queue_array_steal_tail(omp_queue_t *q);
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

/* list implementation */

extern void __ompc_queue_list_init(omp_queue_t *q, int num_slots);
extern void __ompc_queue_list_free_slots(omp_queue_t *q);
extern int __ompc_queue_list_is_full(omp_queue_t *q);
extern int __ompc_queue_list_num_used_slots(omp_queue_t *q);
extern omp_queue_item_t __ompc_queue_list_steal_head(omp_queue_t *q);
extern omp_queue_item_t __ompc_queue_list_steal_tail(omp_queue_t *q);
extern omp_queue_item_t __ompc_queue_list_get_head(omp_queue_t *q);
extern omp_queue_item_t __ompc_queue_list_get_tail(omp_queue_t *q);
extern int __ompc_queue_list_put_tail(omp_queue_t *q, omp_queue_item_t item);
extern int __ompc_queue_list_put_head(omp_queue_t *q, omp_queue_item_t item);

/* lockless implementation */
extern void __ompc_queue_lockless_init(omp_queue_t * q, int num_slots);
extern void __ompc_queue_lockless_free_slots(omp_queue_t *q);
extern int __ompc_queue_lockless_num_used_slots(omp_queue_t *q);
extern int __ompc_queue_lockless_is_full(omp_queue_t *q);
extern omp_queue_item_t __ompc_queue_lockless_get_head(omp_queue_t *q);
extern omp_queue_item_t __ompc_queue_lockless_get_tail(omp_queue_t *q);
extern int __ompc_queue_lockless_put_tail(omp_queue_t *q, omp_queue_item_t item);


#endif /* __omp_queue_included */
