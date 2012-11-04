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


/*   History:  07/28/2011, created by Jim LaGrone, University of Houston

	  In this pool, each thread has 2 queues: 1 public, 1 private. Each thread
	  has a public and private queue. An environment variable,
	  O64_OMP_TASK_POOL_GREEDVAL, is used to control how many tasks are placed
	  in the private or public queue. A value of 1 is "generous" and all newly
	  created tasks are placed in the public queue. Higher values allow some
	  portion of tasks to be placed in the private queue for possible work
	  stealing for a "greedy" policy. Theoretically, if there is locality to
	  exploit, the greedy approach might increase the likelihood of this
	  locality. If a single region is creating the tasks, a generous approach
	  will help with work stealing to start.

	 * O64_OMP_TASK_POOL_GREEDVAL=1 creates a generous policy and all tasks
      are placed in the public queue. Every task in public queue and available
      for theft.
  	 * O64_OMP_TASK_POOL_GREEDVAL=2 creates an equal policy, half the created
      tasks are placed in each queue. Every other task in public queue.
  	 * O64_OMP_TASK_POOL_GREEDVAL=3 Every third task in public queue.
	 * O64_OMP_TASK_POOL_GREEDVAL=N Every Nth task in public queue.

		As the value of N increases, the policy becomes more greedy as more
        tasks are placed in a thread's private queue for possible theft.

		NOTE: This policy has nothing to do with a task being TIED or UNTIED. At
      the time of their creation, the task does not need to be bound to any
      particular thread since its execution has not begun.

		WARNING: If this pool is used with TIED tasks, it is possible to violate
      the OpenMP Task Scheduling Constraint #2 per the specification 3.1,
      found on p. 66.

 */
#include <stdlib.h>
#include <assert.h>
#include "../omp_rtl.h"
#include "../omp_sys.h"

/* level ids */

#define PRIVATE(q) (2*(q))
#define PUBLIC(q) (2*(q)+1)

const short LEVEL0 = 0;

/* used for modulo operand  */
short __omp_task_pool_greedval;

/* keeps track of how many tasks have been placed in queues */
static unsigned long int alternator = 0;

/* __ompc_init_task_pool_public_private:
 * Initializes a task pool, for which tasks may be added and taken.  The task
 * pool will be single-level, with 1 task queue allotted per thread.
 */
omp_task_pool_t * __ompc_create_task_pool_public_private(int team_size)
{
  int i;
  omp_task_pool_t *new_pool;
  omp_task_queue_level_t *level_one;

  new_pool = (omp_task_pool_t *) aligned_malloc(sizeof(omp_task_pool_t),
                                                CACHE_LINE_SIZE);
  Is_True(new_pool != NULL, ("__ompc_create_task_pool: couldn't malloc new_pool"));

  new_pool->team_size = team_size;
  new_pool->num_levels = 1;
  new_pool->num_pending_tasks = 0;
  new_pool->level = aligned_malloc(sizeof(omp_task_queue_level_t),
                                   CACHE_LINE_SIZE);
  pthread_mutex_init(&(new_pool->pool_lock), NULL);
  pthread_cond_init(&(new_pool->pool_cond), NULL);

  Is_True(new_pool->level != NULL,
      ("__ompc_create_task_pool: couldn't malloc level"));

  level_one = &new_pool->level[LEVEL0];

  level_one->num_queues = team_size*2;
  level_one->task_queue = aligned_malloc(sizeof(omp_queue_t) * team_size * 2,
                                          CACHE_LINE_SIZE);

  Is_True(level_one->task_queue != NULL,
      ("__ompc_create_task_pool: couldn't malloc per-thread task queue"));

  for (i = 0; i < team_size * 2; i++)
    __ompc_queue_init(&level_one->task_queue[i], __omp_task_queue_num_slots);

  return new_pool;
}

/* __ompc_expand_task_pool_public_private
 * Expand the task pool for a new team size. Simply a matter of add an extra
 * task queue per extra thread.
 */
omp_task_pool_t * __ompc_expand_task_pool_public_private(omp_task_pool_t *pool,
                                                      int new_team_size)
{
  int i;
  int old_team_size;
  omp_task_queue_level_t *level_one;

  if (pool == NULL)
    return __ompc_create_task_pool(new_team_size);

  level_one = &pool->level[LEVEL0];

  old_team_size = pool->team_size;

  level_one->num_queues = new_team_size * 2; 
  level_one->task_queue = aligned_realloc(
                              (void *) level_one->task_queue,
                              sizeof(omp_queue_t) * old_team_size * 2,
                              sizeof(omp_queue_t) * new_team_size * 2,
                              CACHE_LINE_SIZE);
  Is_True(level_one->task_queue != NULL,
      ("__ompc_expand_task_pool: couldn't expand the task pool"));

  for (i = old_team_size*2; i < new_team_size*2; i++)
    __ompc_queue_init(&level_one->task_queue[i],
                      __omp_task_queue_num_slots);

  return pool;
}

/* __ompc_add_task_to_pool_public_private:
 * Adds a task to the task pool. The task will be added to the current
 * thread's queue. 
 */
int __ompc_add_task_to_pool_public_private(omp_task_pool_t *pool, omp_task_t *task)
{
/*REMOVE AFTER DEBUGGING*/
  static int public_count = 0;
  static int private_count = 0;
/*REMOVE AFTER DEBUGGING*/

  int success;
  int myid = __omp_myid;
  omp_task_queue_level_t *level_one;

  Is_True(pool != NULL, ("__ompc_add_task_to_pool: task pool is uninitialized"));
  Is_True(task != NULL,
      ("__ompc_add_task_to_pool: tried to add NULL task to pool"));

  /* num_pending_tasks track not just tasks entered into the task pool, but
   * also tasks marked as deferred that could not fit into the task pool
   */
  if (__ompc_atomic_inc(&pool->num_pending_tasks) == 1) {
    pthread_mutex_lock(&pool->pool_lock);
    pthread_cond_broadcast(&pool->pool_cond);
    pthread_mutex_unlock(&pool->pool_lock);
  }

  level_one = &pool->level[LEVEL0];

  int greed_val = alternator%__omp_task_pool_greedval;

  //printf("greed_val = %d\n", greed_val); 
  if ( greed_val != 0 ){
      success = __ompc_task_queue_put( 
                        &level_one->task_queue[PRIVATE(myid)],
                        task);
/*REMOVE AFTER DEBUGGING*/
		private_count++;
  }
  else{
      success = __ompc_task_queue_put( 
                        &level_one->task_queue[PUBLIC(myid)],
                        task);
/*REMOVE AFTER DEBUGGING*/
      public_count++; 
  }
alternator++;     
//  alternator = (alternator%__omp_task_pool_greedval)+1;
/*REMOVE AFTER DEBUGGING*/
//printf("alternator = %d\n",alternator);
/*  if(alternator % 16000 == 0)
	  printf("\t\tCount counts\tpublic = %d, private = %d, \n",
						public_count, private_count ); 
*/
  return success;
}

/* __ompc_remove_task_from_pool_public_private:
 *Takes task from the task pool. 
 *
 * Takes a task from the task pool. First tries to get a task from the current
 * thread's task queue. If that doesn't work, then it will attempt to steal a
 * task from another task queue (so long as there are no other tasks, not in a
 * barrier, that are tied to the current thread).
 *
 * Note: The restriction on stealing is overly conservative. Even if there are
 * tasks tied to the current thread and not in a barrier ([*]), we should be
 * able to steal any untied tasks, or tied tasks that descend from all tasks
 * in [*]. But this implementation does not separate untied tasks from tied
 * tasks, and also does not track descendants in the task pool. 
 */
omp_task_t *__ompc_remove_task_from_pool_public_private(omp_task_pool_t *pool)
{
  omp_task_t *task, *current_task;
  omp_team_t *team;
  omp_v_thread_t *current_thread;
  omp_queue_t *my_queue;
  omp_queue_t *victim_queue;
  omp_task_queue_level_t *level_one;
  int myid = __omp_myid;

  Is_True(pool != NULL, ("__ompc_remove_task_from_pool: task pool is uninitialized"));

  current_task = __omp_current_task;
  current_thread = __omp_current_v_thread;
  level_one = &pool->level[LEVEL0];

  task = __ompc_task_queue_get(&level_one->task_queue[PRIVATE(myid)]);
  if (task == NULL)
    task = __ompc_task_queue_get(&level_one->task_queue[PUBLIC(myid)]);

  /* if no task in local queue, we will look in another queue so long as there
   * are no suspended tasks tied to thread and the current task is either in a
   * barrier or its not tied
   */
  if (task == NULL) {
    int first_victim, victim = 0;
    int team_size = pool->team_size;
    victim = (rand_r(&__omp_seed) % (team_size - 1));
    if (victim >= myid) victim++;
    /* cycle through to find a queue with work to steal */
    first_victim = victim;
    while (1) {
      while (__ompc_queue_is_empty(
                     &level_one->task_queue[PUBLIC(victim)])) {
        victim++;
        if (victim == myid)
          victim++;
        if (victim == team_size)
          victim = 0;
        if (victim == first_victim)
         goto DONE;
      }
      task = __ompc_task_queue_steal(
                     &level_one->task_queue[PUBLIC(victim)]);
      if ( task != NULL ) return task;
    }
  }

DONE:
  return task;
}

/* __ompc_destroy_task_pool_public_private:
 */
void __ompc_destroy_task_pool_public_private(omp_task_pool_t *pool)
{
  int i;
  omp_task_queue_level_t *level_one;

  Is_True(pool != NULL, ("__ompc_destroy_task_pool; pool is NULL"));

  level_one = &pool->level[LEVEL0];

  for (i = 0; i < pool->team_size*2; i++) {
    __ompc_queue_free_slots(&level_one->task_queue[i]);
  }

  pthread_mutex_destroy(&pool->pool_lock);

  aligned_free(level_one->task_queue); /* free queues in level 0 */
  aligned_free(pool->level); /* free the level array */
  aligned_free(pool); /* free the pool itself */
}
