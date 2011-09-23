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

#include <stdlib.h>
#include <assert.h>
#include "omp_rtl.h"
#include "omp_sys.h"

/*
 * Current implementations:
 *
 * default:       single-level, work-stealing task pool, 1 task queue per
 *                thread which holds tied tasks, and 1 task queue per thread
 *                which holds untied tasks; implementation in
 *                omp_task_pool.c.
 *
 * simple:        single-level, work-stealing task pool, 1 task queue per
 *                thread which holds both tied and untied tasks; implementation
 *                in other_taskpools directory.
 *
 * 2level:        two-level, workstealing task pool, 1 task queue per thread
 *                which holds tied tasks, 1 task queue per thread which holds
 *                untied tasks, 1 community queue for tied tasks and 1
 *                community queue for untied tasks; work may be taken from the
 *                untied community queue in chunks; not yet implemented
 *
 * simple_2level: two-level, work-stealing task pool, 1 task queue per thread,
 *                and 1 community queue for the team; each queue holds both
 *                tied and untied tasks; work may  be taken from the community
 *                queue in chunks; implementation in other_takspools directory
 *
 * public_private: implementation in other_taskpools directory
 *
 */

/* task pool size */
int __omp_task_queue_num_slots = 0;
int __omp_task_chunk_size = 1;

/* these will point to appropriate implementation at runtime */
omp_task_pool_t *(*__ompc_create_task_pool)(int team_size);
omp_task_pool_t *(*__ompc_expand_task_pool)(omp_task_pool_t *pool,
                                            int new_team_size);
int (*__ompc_add_task_to_pool)(omp_task_pool_t *pool, omp_task_t *new_task);
omp_task_t *(*__ompc_remove_task_from_pool)(omp_task_pool_t *pool);
void (*__ompc_destroy_task_pool)(omp_task_pool_t *pool);

omp_queue_item_t (*__ompc_task_queue_get)(omp_queue_t *q);
omp_queue_item_t (*__ompc_task_queue_steal)(omp_queue_t *q);
int (*__ompc_task_queue_put)(omp_queue_t *q, omp_queue_item_t item);
int (*__ompc_task_queue_donate)(omp_queue_t *q, omp_queue_item_t item);
int (*__ompc_task_queue_is_full)(omp_queue_t *q);
int (*__ompc_task_queue_num_used_slots)(omp_queue_t *q);
omp_queue_item_t (*__ompc_task_queue_steal_chunk)(omp_queue_t *src,
                                                  omp_queue_t *dst,
                                                  int chunk_size);


/* level ids */
#define PER_THREAD 0

#define TIED_IDX(q) (2*(q))
#define UNTIED_IDX(q) (2*(q)+1)

/* __ompc_init_task_pool_default:
 * Initializes a task pool, for which tasks may be added and taken.  The task
 * pool will be single-level, with 1 task queue allotted per thread.
 */
omp_task_pool_t * __ompc_create_task_pool_default(int team_size)
{
  int i;
  omp_task_pool_t *new_pool;
  omp_task_queue_level_t *per_thread;

  new_pool = (omp_task_pool_t *) aligned_malloc(sizeof(omp_task_pool_t),
                                                CACHE_LINE_SIZE);
  Is_True(new_pool != NULL, ("__ompc_create_task_pool: couldn't malloc new_pool"));

  new_pool->team_size = team_size;
  new_pool->num_levels = 1;
  new_pool->num_pending_tasks = 0;
  new_pool->level = aligned_malloc(sizeof(omp_task_queue_level_t),
                                   CACHE_LINE_SIZE);

  Is_True(new_pool->level != NULL,
      ("__ompc_create_task_pool: couldn't malloc level"));

  per_thread = &new_pool->level[PER_THREAD];

  per_thread->num_queues = team_size*2;
  per_thread->task_queue = aligned_malloc(sizeof(omp_queue_t) * team_size * 2,
                                          CACHE_LINE_SIZE);

  Is_True(per_thread->task_queue != NULL,
      ("__ompc_create_task_pool: couldn't malloc per-thread task queue"));

  for (i = 0; i < team_size; i++) {
    __ompc_queue_init(&per_thread->task_queue[TIED_IDX(i)],
                      __omp_task_queue_num_slots);
    __ompc_queue_init(&per_thread->task_queue[UNTIED_IDX(i)],
                      __omp_task_queue_num_slots);
  }

  return new_pool;
}

/* __ompc_expand_task_pool_default
 * Expand the task pool for a new team size. Simply a matter of add an extra
 * task queue per extra thread.
 */
omp_task_pool_t * __ompc_expand_task_pool_default(omp_task_pool_t *pool,
                                                      int new_team_size)
{
  int i;
  int old_team_size;
  omp_task_queue_level_t *per_thread;

  if (pool == NULL)
    return __ompc_create_task_pool(new_team_size);

  per_thread = &pool->level[PER_THREAD];

  old_team_size = pool->team_size;

  per_thread->num_queues = new_team_size * 2;
  per_thread->task_queue = aligned_realloc(
                              (void *) per_thread->task_queue,
                              sizeof(omp_queue_t) * old_team_size * 2,
                              sizeof(omp_queue_t) * new_team_size * 2,
                              CACHE_LINE_SIZE);
  Is_True(per_thread->task_queue != NULL,
      ("__ompc_expand_task_pool: couldn't expand the task pool"));

  for (i = old_team_size; i < new_team_size; i++) {
    __ompc_queue_init(&per_thread->task_queue[TIED_IDX(i)],
                      __omp_task_queue_num_slots);
    __ompc_queue_init(&per_thread->task_queue[UNTIED_IDX(i)],
                      __omp_task_queue_num_slots);
  }

  return pool;
}

/* __ompc_add_task_to_pool_default:
 * Adds a task to the task pool. The task will be added to the current
 * thread's queue.
 */
int __ompc_add_task_to_pool_default(omp_task_pool_t *pool, omp_task_t *task)
{
  int success;
  int myid = __omp_myid;
  omp_task_queue_level_t *per_thread;

  Is_True(pool != NULL, ("__ompc_add_task_to_pool: task pool is uninitialized"));
  Is_True(task != NULL,
      ("__ompc_add_task_to_pool: tried to add NULL task to pool"));

  /* num_pending_tasks track not just tasks entered into the task pool, but
   * also tasks marked as deferred that could not fit into the task pool
   */
  __ompc_atomic_inc(&pool->num_pending_tasks);

  per_thread = &pool->level[PER_THREAD];

  if (__ompc_task_is_tied(task))
    /* For tied tasks, we don't use the task_queue API. We explicitly put to
     * the tail */
    success = __ompc_queue_put_tail(
                        &per_thread->task_queue[TIED_IDX(myid)],
                        task);
  else
    success = __ompc_task_queue_put(
                        &per_thread->task_queue[UNTIED_IDX(myid)],
                        task);

  return success;
}

/* __ompc_remove_task_from_pool_default:
 *Takes task from the task pool.
 *
 * Takes a task from the task pool. First tries to get a task from the current
 * thread's task queue. If that doesn't work, then it will attempt to steal a
 * task from another task queue (so long as there are no other tasks, not in a
 * barrier, that are tied to the current thread).
 */
omp_task_t *__ompc_remove_task_from_pool_default(omp_task_pool_t *pool)
{
  omp_task_t *task, *current_task;
  omp_team_t *team;
  omp_v_thread_t *current_thread;
  omp_queue_t *my_queue;
  omp_queue_t *victim_queue;
  omp_task_queue_level_t *per_thread;
  int myid = __omp_myid;

  Is_True(pool != NULL, ("__ompc_remove_task_from_pool: task pool is uninitialized"));

  current_task = __omp_current_task;
  current_thread = __omp_current_v_thread;
  per_thread = &pool->level[PER_THREAD];

  /* We get only from the tail for tied tasks. This is necessary to guarantee
   * that tied tasks are only scheduled if they are descendants of every
   * suspended tied task not at a barrier */
  task = __ompc_queue_get_tail(&per_thread->task_queue[TIED_IDX(myid)]);

  /* for untied tasks, we can get from the head or tail, depending on what
   * O64_OMP_TASK_QUEUE is set to */
  if (task == NULL)
    task = __ompc_task_queue_get(&per_thread->task_queue[UNTIED_IDX(myid)]);


  /* check if there are any untied tasks available in the other task queues */
  if (task == NULL) {
    int first_victim, victim = 0;
    int team_size = pool->team_size;

    if (team_size < 2)
      return NULL;

    victim = (rand_r(&__omp_seed) % (team_size - 1));
    if (victim >= myid) victim++;
    /* cycle through to find a queue with work to steal */
    first_victim = victim;
    while (1) {
      while (__ompc_queue_lockless_is_empty(
                     &per_thread->task_queue[UNTIED_IDX(victim)])) {
        victim++;
        if (victim == myid)
          victim++;
        if (victim == team_size)
          victim = 0;
        if (victim == first_victim)
         goto CHECK_TIED_TASK_QUEUES;
      }
      task = __ompc_task_queue_steal(
                     &per_thread->task_queue[UNTIED_IDX(victim)]);
      if ( task != NULL ) {
        /*
        if (!__ompc_task_state_is_unscheduled(task)) {
          // Is_True(0, ("state of task from queue was not unscheduled"));
          printf("\n... (1) skipping over a task with state %s; queue size is %d \n",
                  __ompc_task_get_state_string(task),
         __ompc_queue_num_used_slots(&per_thread->task_queue[UNTIED_IDX(victim)]));
          task = NULL;
        }
        */
        return task;
      }
    }
  }

  /* if no task in local queue and no available untied tasks, we will look in
   * another queue so long as there are no suspended tasks tied to thread and
   * the current task is either in a barrier or its not tied
   */
CHECK_TIED_TASK_QUEUES:
  if (task == NULL && !current_thread->num_suspended_tied_tasks &&
      (__ompc_task_state_is_in_barrier(current_task) ||
       !__ompc_task_is_tied(current_task))) {
    int first_victim, victim = 0;
    int team_size = pool->team_size;

    victim = (rand_r(&__omp_seed) % (team_size - 1));
    if (victim >= myid) victim++;
    /* cycle through to find a queue with work to steal */
    first_victim = victim;
    while (1) {
      while (__ompc_queue_is_empty(
                         &per_thread->task_queue[TIED_IDX(victim)])) {
        victim++;
        if (victim == myid)
          victim++;
        if (victim == team_size)
          victim = 0;
        if (victim == first_victim)
          return NULL;
      }
      /* Always steal from the head for tied tasks. Note also that by not
       * using the task_queue API, CFIFO implementation will not be used */
      task = __ompc_queue_steal_head(
                         &per_thread->task_queue[TIED_IDX(victim)]);
      if ( task != NULL ) {
        /*
        if (!__ompc_task_state_is_unscheduled(task)) {
          // Is_True(0, ("state of task from queue was not unscheduled"));
          printf("\n... (2) skipping over a task with state %s; queue size is %d \n",
                  __ompc_task_get_state_string(task),
         __ompc_queue_num_used_slots(&per_thread->task_queue[TIED_IDX(victim)]));
          task = NULL;
        }
        */
        return task;
      }
    }
  }

  /*
  if ( task != NULL ) {
    if (!__ompc_task_state_is_unscheduled(task)) {
      // Is_True(0, ("state of task from queue was not unscheduled"));
      printf("\n... (3) skipping over a task with state %s; queue size is %d \n",
          __ompc_task_get_state_string(task),
          __ompc_queue_num_used_slots(&per_thread->task_queue[UNTIED_IDX(myid)]));
      task = NULL;
    }
  }
  */
  return task;
}

/* __ompc_destroy_task_pool_default:
 */
void __ompc_destroy_task_pool_default(omp_task_pool_t *pool)
{
  int i;
  omp_task_queue_level_t *per_thread;

  Is_True(pool != NULL, ("__ompc_destroy_task_pool; pool is NULL"));

  per_thread = &pool->level[PER_THREAD];

  for (i = 0; i < pool->team_size; i++) {
    __ompc_queue_free_slots(&per_thread->task_queue[TIED_IDX(i)]);
    __ompc_queue_free_slots(&per_thread->task_queue[UNTIED_IDX(i)]);
  }

  aligned_free(per_thread->task_queue); /* free queues in level 0 */
  aligned_free(pool->level); /* free the level array */
  aligned_free(pool); /* free the pool itself */
}
