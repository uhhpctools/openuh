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
#include "../omp_rtl.h"
#include "../omp_sys.h"

/* level ids */
#define PER_THREAD 0
#define COMMUNITY 1


/* __ompc_init_task_pool_simple_2level:
 * Initializes a task pool, for which tasks may be added and taken.
 */
omp_task_pool_t * __ompc_create_task_pool_simple_2level(int team_size)
{
  int i;
  omp_task_pool_t *new_pool;
  omp_task_queue_level_t *per_thread;
  omp_task_queue_level_t *community;

  new_pool = (omp_task_pool_t *) aligned_malloc(sizeof(omp_task_pool_t),
                                                CACHE_LINE_SIZE);
  Is_True(new_pool != NULL, ("__ompc_create_task_pool: couldn't malloc new_pool"));

  new_pool->team_size = team_size;
  new_pool->num_levels = 2;
  new_pool->num_pending_tasks = 0;
  new_pool->level = aligned_malloc(sizeof(omp_task_queue_level_t)*2,
                                   CACHE_LINE_SIZE);
  pthread_mutex_init(&(new_pool->pool_lock), NULL);
  pthread_cond_init(&(new_pool->pool_cond), NULL);

  Is_True(new_pool->level != NULL,
      ("__ompc_create_task_pool: couldn't malloc level"));

  per_thread = &new_pool->level[PER_THREAD];
  community = &new_pool->level[COMMUNITY];

  per_thread->num_queues = team_size;
  per_thread->task_queue = aligned_malloc(sizeof(omp_queue_t) * team_size,
                                          CACHE_LINE_SIZE);
  community->num_queues = 1;
  community->task_queue = aligned_malloc(sizeof(omp_queue_t), CACHE_LINE_SIZE);

  Is_True(per_thread->task_queue != NULL,
      ("__ompc_create_task_pool: couldn't malloc per-thread task queues"));
  Is_True(community->task_queue != NULL,
      ("__ompc_create_task_pool: couldn't malloc community task queue"));

  for (i = 0; i < team_size; i++)
    __ompc_queue_init(&per_thread->task_queue[i], __omp_task_queue_num_slots);

  /* what's a good size for the community queue, as a function of the local queue
   * sizes and the team size? Just going to make it 2 * local queue size for
   * now.
   */
  __ompc_queue_init(community->task_queue, __omp_task_queue_num_slots*2);

  return new_pool;
}

/* __ompc_expand_task_pool_simple_2level
 * Expand the task pool for a new team size. We add an extra task queue per
 * extra thread.
 *
 * Note: may consider resizing the community queue as well, but not doing that
 * presently.
 */
omp_task_pool_t * __ompc_expand_task_pool_simple_2level(omp_task_pool_t *pool, int
    new_team_size) { int i; int old_team_size; omp_task_queue_level_t
  *per_thread;

  if (pool == NULL)
    return __ompc_create_task_pool(new_team_size);

  per_thread = &pool->level[PER_THREAD];

  old_team_size = pool->team_size;

  per_thread->num_queues = new_team_size;
  per_thread->task_queue = aligned_realloc(
                              (void *) per_thread->task_queue,
                              sizeof(omp_queue_t) * old_team_size,
                              sizeof(omp_queue_t) * new_team_size,
                              CACHE_LINE_SIZE);
  Is_True(per_thread->task_queue != NULL,
      ("__ompc_expand_task_pool: couldn't expand the task pool"));

  for (i = old_team_size; i < new_team_size; i++)
    __ompc_queue_init(&per_thread->task_queue[i],
                      __omp_task_queue_num_slots);

  return pool;
}

/* __ompc_add_task_to_pool_simple_2level:
 * Adds a task to the task pool. The task will be added to the current
 * thread's queue.
 */
int __ompc_add_task_to_pool_simple_2level(omp_task_pool_t *pool, omp_task_t *task)
{
  int success;
  int myid = __omp_myid;
  omp_task_queue_level_t *per_thread;
  omp_task_queue_level_t *community;

  Is_True(pool != NULL, ("__ompc_add_task_to_pool: task pool is uninitialized"));
  Is_True(task != NULL,
      ("__ompc_add_task_to_pool: tried to add NULL task to pool"));

  success = 0;
  per_thread = &pool->level[PER_THREAD];
  community = &pool->level[COMMUNITY];

  /* num_pending_tasks track not just tasks entered into the task pool, but
   * also tasks marked as deferred that could not fit into the task pool
   */
  if (__ompc_atomic_inc(&pool->num_pending_tasks) == 1) {
    pthread_mutex_lock(&pool->pool_lock);
    pthread_cond_broadcast(&pool->pool_cond);
    pthread_mutex_unlock(&pool->pool_lock);
  }

  /* don't try to place it in per-thread queue if it looks to be full, because
   * we have the community queue to use instead   */
  if (!__ompc_task_queue_is_full(&per_thread->task_queue[myid]))
    success = __ompc_task_queue_put(&pool->level[PER_THREAD].task_queue[myid],
                                    task);

  if (!success)
    success = __ompc_task_queue_donate(pool->level[COMMUNITY].task_queue, task);

  return success;
}

/* __ompc_remove_task_from_pool_simple_2level:
 * Takes a task from the task pool. First tries to get a task from the current
 * thread's task queue. If that doesn't work, then it will look for work in
 * the community queue. If that's also empty, then it will attempt to steal a
 * task from another task queue (so long as there are no other tasks, not in a
 * barrier, that are tied to the current thread).
 *
 * Note: The restriction on stealing is overly conservative. Even if there are
 * tasks tied to the current thread and not in a barrier ([*]), we should be
 * able to steal any untied tasks, or tied tasks that descend from all tasks
 * in [*]. But this implementation does not separate untied tasks from tied
 * tasks, and also does not track descendants in the task pool.
 */
omp_task_t *__ompc_remove_task_from_pool_simple_2level(omp_task_pool_t *pool)
{
  omp_task_t *task, *current_task;
  omp_team_t *team;
  omp_v_thread_t *current_thread;
  omp_queue_t *my_queue;
  omp_queue_t *victim_queue;
  omp_task_queue_level_t *per_thread, *community;
  int myid = __omp_myid;

  Is_True(pool != NULL, ("__ompc_remove_task_from_pool: task pool is uninitialized"));

  current_task = __omp_current_task;
  current_thread = __omp_current_v_thread;
  per_thread = &pool->level[PER_THREAD];
  community = &pool->level[COMMUNITY];

  task = __ompc_task_queue_get(&per_thread->task_queue[myid]);

  /* if no task in local queue, we will look in another queue so long as there
   * are no suspended tasks tied to thread and the current task is either in a
   * barrier or its not tied
   */
  if (task == NULL && !current_thread->num_suspended_tied_tasks &&
      (__ompc_task_state_is_in_barrier(current_task) ||
       !__ompc_task_is_tied(current_task))) {

    if (__omp_task_chunk_size > 1) {
      /* this will steal a chunk of tasks, instead of just 1, from the
       * community
       * queue */
      task = __ompc_task_queue_steal_chunk(community->task_queue,
          &per_thread->task_queue[myid],
          __omp_task_chunk_size);
    } else {
      task = __ompc_task_queue_steal(community->task_queue);
    }

    if (task == NULL) {
      int first_victim, victim = 0;
      int team_size = pool->team_size;
      victim = (rand_r(&__omp_seed) % (team_size - 1));
      if (victim >= myid) victim++;
      /* cycle through to find a queue with work to steal */
      first_victim = victim;
      while (1) {
        while (__ompc_queue_is_empty(&per_thread->task_queue[victim])) {
          victim++;
          if (victim == myid)
            victim++;
          if (victim == team_size)
            victim = 0;
          if (victim == first_victim)
            return NULL;
        }
        task = __ompc_task_queue_steal(&per_thread->task_queue[victim]);
        if ( task != NULL ) return task;
      }
    }
  }

  return task;
}

/* __ompc_destroy_task_pool_simple_2level:
 */
void __ompc_destroy_task_pool_simple_2level(omp_task_pool_t *pool)
{
  int i;
  omp_task_queue_level_t *per_thread;
  omp_task_queue_level_t *community;

  Is_True(pool != NULL, ("__ompc_destroy_task_pool; pool is NULL"));

  per_thread = &pool->level[PER_THREAD];
  community = &pool->level[COMMUNITY];

  for (i = 0; i < pool->team_size; i++) {
    __ompc_queue_free_slots(&per_thread->task_queue[i]);
  }
  __ompc_queue_free_slots(community->task_queue);

  pthread_mutex_destroy(&pool->pool_lock);

  aligned_free(per_thread->task_queue); /* free queues in level 0 */
  aligned_free(community->task_queue); /* free queues in level 1 */
  aligned_free(pool->level); /* free the level array */
  aligned_free(pool); /* free the pool itself */
}
