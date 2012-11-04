/*
 Task Pool Implementation for Open64's OpenMP runtime library

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

#ifndef __omp_task_scheduler_included
#define __omp_task_scheduler_included

#include "omp_sys.h"
#include "omp_queue.h"

#define TASK_QUEUE_DEFAULT_NUM_SLOTS 128

/* Each task queue level in the task pool has a bank of available
 * task queues. */
struct omp_task_queue_level {
  omp_queue_t   *task_queue;
  int           num_queues;
} __attribute__ ((__aligned__(CACHE_LINE_SIZE)));
typedef struct omp_task_queue_level omp_task_queue_level_t;

struct omp_task_pool {
  omp_task_queue_level_t *level;

  /* Number of deferred tasks that are pending (i.e. have not yet exited) */
  volatile int num_pending_tasks;
  pthread_mutex_t pool_lock;
  pthread_cond_t pool_cond;

  int num_levels;
  int team_size;
} __attribute__ ((__aligned__(CACHE_LINE_SIZE)));
typedef struct omp_task_pool omp_task_pool_t;

/* inline functions */

static inline int __ompc_task_pool_num_pending_tasks(omp_task_pool_t *pool)
{
  return pool->num_pending_tasks;
}

static inline void __ompc_task_pool_set_team_size(omp_task_pool_t *pool,
                                                  int team_size)
{
  pool->team_size = team_size;
}


/* external interface */
extern int __omp_task_queue_num_slots;
extern int __omp_task_chunk_size;
extern short __omp_task_pool_greedval;

extern omp_task_pool_t *(*__ompc_create_task_pool)(int team_size);
extern omp_task_pool_t *(*__ompc_expand_task_pool)(omp_task_pool_t *pool,
                                            int new_team_size);
extern int (*__ompc_add_task_to_pool)(omp_task_pool_t *pool,
                                      omp_task_t *new_task);
extern omp_task_t *(*__ompc_remove_task_from_pool)(omp_task_pool_t *pool);
extern void (*__ompc_destroy_task_pool)(omp_task_pool_t *pool);

extern omp_queue_item_t (*__ompc_task_queue_get)(omp_queue_t *q);
extern omp_queue_item_t (*__ompc_task_queue_steal)(omp_queue_t *q);
extern int (*__ompc_task_queue_put)(omp_queue_t *q, omp_queue_item_t item);
extern int (*__ompc_task_queue_donate)(omp_queue_t *q, omp_queue_item_t item);
extern int (*__ompc_task_queue_is_full)(omp_queue_t *q);
extern int (*__ompc_task_queue_num_used_slots)(omp_queue_t *q);
extern omp_queue_item_t (*__ompc_task_queue_steal_chunk)(omp_queue_t *src,
                                                         omp_queue_t *dst,
                                                         int chunk_size);


/* default */
extern omp_task_pool_t *
__ompc_create_task_pool_default(int team_size);
extern omp_task_pool_t *
__ompc_expand_task_pool_default( omp_task_pool_t *pool, int new_team_size);
extern int
__ompc_add_task_to_pool_default(omp_task_pool_t *pool, omp_task_t *task);
extern omp_task_t *
__ompc_remove_task_from_pool_default(omp_task_pool_t *pool);
extern void
__ompc_destroy_task_pool_default(omp_task_pool_t *pool);

/* simple */
extern omp_task_pool_t *
__ompc_create_task_pool_simple(int team_size);
extern omp_task_pool_t *
__ompc_expand_task_pool_simple( omp_task_pool_t *pool, int new_team_size);
extern int
__ompc_add_task_to_pool_simple(omp_task_pool_t *pool, omp_task_t *task);
extern omp_task_t *
__ompc_remove_task_from_pool_simple(omp_task_pool_t *pool);
extern void
__ompc_destroy_task_pool_simple(omp_task_pool_t *pool);


/* simple_2level */
extern omp_task_pool_t *
__ompc_create_task_pool_simple_2level(int team_size);
extern omp_task_pool_t *
__ompc_expand_task_pool_simple_2level( omp_task_pool_t *pool, int new_team_size);
extern int
__ompc_add_task_to_pool_simple_2level(omp_task_pool_t *pool, omp_task_t *task);
extern omp_task_t *
__ompc_remove_task_from_pool_simple_2level(omp_task_pool_t *pool);
extern void
__ompc_destroy_task_pool_simple_2level(omp_task_pool_t *pool);


/* public_private Jim LaGrone 7/28/2011 */
extern omp_task_pool_t *
__ompc_create_task_pool_public_private(int team_size);
extern omp_task_pool_t *
__ompc_expand_task_pool_public_private( omp_task_pool_t *pool, int new_team_size);
extern int
__ompc_add_task_to_pool_public_private(omp_task_pool_t *pool, omp_task_t *task);
extern omp_task_t *
__ompc_remove_task_from_pool_public_private(omp_task_pool_t *pool);
extern void
__ompc_destroy_task_pool_public_private(omp_task_pool_t *pool);

#endif /* __omp_task_scheduler_included */
