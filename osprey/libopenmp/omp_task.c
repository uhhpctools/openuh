/*
 Task Implementation for Open64's OpenMP runtime library

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


#include "omp_rtl.h"
#include "omp_task.h"

/* Description: Implements a breadth-first task scheduler for OpenMP 3.0
 * tasks. When a task is created with may_delay != 0, it is added to the task
 * pool and the current task resumes execution. Task switching in this case
 * occurs at:
 *
 *    - when creating a task, if the task can not be added into the task pool
 *    - during a taskwait, if there are pending children
 *    - when the current task is exiting
 *    - at a barrier (see omp_thread.{c,h})
 *
 * When OpenMP 3.1 is supported, additionally will need to support task
 * switching at task-yield.
 *
 * This implementation will support a variety of task cutoff schemes. By
 * default, task generation is "cut off" if:
 *
 *    - currently in a sequential region
 *    - only 1 thread in the current team
 *    - if the current task object is set to NULL
 *    - if the "switching depth" has exceeded a certain threshold (MAX_SDEPTH)
 *
 * When a task is cut off, it will execute immediately in a work-first manner
 * (that is, any of its descendants will also be "cut off").
 *
 * Note: Current implementation does not make use of PCL (Portable Coroutine
 * Library). Once a task starts to execute, it will not be placed back into
 * the task pool or resume execution on another thread. This constrains what
 * the scheduler can do with untied tasks. We are exploring how to support
 * this in a more efficient manner.
 */

#define MAX_SDEPTH 20

/* global variables, function pointers */
__thread omp_task_t *__omp_current_task;

cond_func __ompc_task_cutoff;

__thread unsigned long __omp_task_cutoffs = 0;

int __omp_task_cutoff_num_threads   = 1;
int __omp_task_cutoff_switch        = 1;
int __omp_task_cutoff_depth         = 0;
int __omp_task_cutoff_num_children  = 0;

/* default values when corresponding cutoffs are enabled */
int __omp_task_cutoff_num_threads_min   = 2;
int __omp_task_cutoff_switch_max        = 100;
int __omp_task_cutoff_depth_max         = 100;
int __omp_task_cutoff_num_children_max  = 100;

/* implementation for external API */

int __ompc_task_will_defer(int may_delay)
{
  return may_delay && !__ompc_task_cutoff();
}

void __ompc_task_create(omp_task_func taskfunc, void *frame_pointer,
        void *firstprivates, int may_delay, int is_tied, int blocks_parent)
{
  int myid;
  omp_team_t *team;
  omp_task_t *current_task, *new_task, *orig_task;
  omp_v_thread_t *current_thread;

  current_task = __omp_current_task;

  if (__ompc_task_cutoff()) {
    //__omp_task_cutoffs++;
    orig_task = current_task;
    __omp_current_task = NULL;
    taskfunc(firstprivates, frame_pointer);
    __omp_current_task = orig_task;
    return;
    /* not reached */
  }

  myid = __omp_myid;
  current_thread = __omp_current_v_thread;
  team = current_thread->team;

  if (may_delay) {
    new_task = __ompc_task_new();
    __ompc_task_set_function(new_task, taskfunc);
    __ompc_task_set_frame_pointer(new_task, frame_pointer);
    __ompc_task_set_firstprivates(new_task, firstprivates);
    new_task->creating_thread_id = myid;
    new_task->parent = current_task;
    new_task->depth = current_task->depth + 1;

    __ompc_task_set_flags(new_task, OMP_TASK_IS_DEFERRED);

    if (is_tied)
      __ompc_task_set_flags(new_task, OMP_TASK_IS_TIED);

    __ompc_atomic_inc(&current_task->num_children);

    if (blocks_parent) {
      __ompc_task_set_flags(new_task, OMP_TASK_BLOCKS_PARENT);
      __ompc_atomic_inc(&current_task->num_blocking_children);
    }

    if (__ompc_add_task_to_pool(team->task_pool, new_task) == 0) {
      /* couldn't add to task pool, so execute it immediately */
      __ompc_task_set_state(current_task, OMP_TASK_READY);
      __ompc_task_switch(new_task);
      __ompc_task_set_state(current_task, OMP_TASK_RUNNING);
    }

  } else {
    omp_task_t new_immediate_task;
    new_task = &new_immediate_task;
    memset(new_task, 0, sizeof(omp_task_t));
    __ompc_task_set_function(new_task, taskfunc);
    __ompc_task_set_frame_pointer(new_task, frame_pointer);

    /* firstprivates will be NULL, so don't need to set it */
    Is_True(firstprivates == NULL, ("firstprivates should always be NULL"));

    new_task->creating_thread_id = myid;
    new_task->parent = current_task;
    new_task->depth = current_task->depth + 1;

    if (is_tied)
      __ompc_task_set_flags(new_task, OMP_TASK_IS_TIED);

    __ompc_task_set_state(current_task, OMP_TASK_READY);
    if (__ompc_task_is_tied(current_task)) {
      /* if current task is tied, it should not go back into task pool */
      orig_task = current_task;
      ++(current_thread->num_suspended_tied_tasks);
      __omp_current_task = new_task;
      taskfunc(NULL, frame_pointer);
      --(current_thread->num_suspended_tied_tasks);
    } else {
      /* if current task is untied, it can go back into task pool, but this
       * isn't currently supported. */
      orig_task = current_task;
      __omp_current_task = new_task;
      taskfunc(NULL, frame_pointer);
      __omp_current_task = orig_task;
    }
    __ompc_task_set_state(current_task, OMP_TASK_RUNNING);
  }

}

void __ompc_task_wait()
{
  int myid;
  omp_team_t *team;
  omp_task_t *current_task, *next_task;
  omp_v_thread_t *current_thread;

  current_thread = __omp_current_v_thread;
  current_task   = __omp_current_task;

  /* If no task object assigned for current task, we assume all descendants
   * were executed work-first order. */
  if (current_task == NULL) return;

  team = current_thread->team;

  __ompc_task_set_state(current_task, OMP_TASK_WAITING);

  /* while there are still children, look for available work in task queues */
  while ( current_task->num_children) {
    next_task = __ompc_remove_task_from_pool(team->task_pool);
    if (next_task != NULL) {
        __ompc_task_switch(next_task);
    }
  }

  __ompc_task_set_state(current_task, OMP_TASK_RUNNING);
}

void __ompc_task_exit()
{
  omp_task_flag_t flags;
  int myid;
  omp_team_t *team;
  omp_task_t *current_task, *next_task;
  omp_v_thread_t *current_thread;

  current_task = __omp_current_task;

  /* If no task object assigned for current task, we assume all descendants
   * were executed work-first order. */
  if (current_task == NULL) return;

  current_thread = __omp_current_v_thread;
  team = current_thread->team;

  __ompc_task_set_state(current_task, OMP_TASK_EXITING);

  if (__ompc_task_is_deferred(current_task)) {
    Is_True(current_task->parent != NULL,
            ("deferred task should has a NULL parent"));
    __ompc_atomic_dec(&team->task_pool->num_pending_tasks);
    __ompc_atomic_dec(&current_task->parent->num_children);
  }

  /* only try to free parent or put it back on queue if it was a deferred
   * task and it has no more children (since child tasks may attempt to update
   * num_children field of parent when they exit) */
  if (current_task->parent && __ompc_task_is_deferred(current_task->parent) &&
      current_task->parent->num_children == 0 &&
      __ompc_task_state_is_finished(current_task->parent)) {
      __ompc_task_delete(current_task->parent);
  }

  /* should not immediately return if descendant tasks may potentially still
   * need access to current call stack. instead, we look for other tasks to
   * execute from this point.  */
  while (current_task->num_blocking_children) {
    next_task = __ompc_remove_task_from_pool(team->task_pool);
    if (next_task != NULL) {
        __ompc_task_switch(next_task);
    }
  }

  /* need to decrement num_blocking_children for parent if this is a deferred
   * task. We put it at the end, to ensure all blocking child tasks have first
   * completed.  */

  flags = OMP_TASK_IS_DEFERRED | OMP_TASK_BLOCKS_PARENT;
  if (__ompc_task_get_flags(current_task, flags) == flags)
    __ompc_atomic_dec(&current_task->parent->num_blocking_children);
}

void __ompc_task_firstprivates_alloc(void **firstprivates, int size)
{
  *firstprivates = aligned_malloc(size, CACHE_LINE_SIZE);
}

void __ompc_task_firstprivates_free(void *firstprivates)
{
  aligned_free(firstprivates);
}


/* implementation for internal API */

void __ompc_task_switch(omp_task_t *new_task)
{
  omp_v_thread_t *current_thread = __omp_current_v_thread;
  omp_task_t *orig_task = __omp_current_task;

  __ompc_task_set_state(new_task, OMP_TASK_RUNNING);
  __omp_current_task = new_task;
  new_task->sdepth = orig_task->sdepth + 1;

  if (__ompc_task_is_tied(orig_task) &&
      !__ompc_task_state_is_in_barrier(orig_task)) {
    ++(current_thread->num_suspended_tied_tasks);
    new_task->t.func(new_task->firstprivates, new_task->frame_pointer);
    --(current_thread->num_suspended_tied_tasks);
  } else {
    new_task->t.func(new_task->firstprivates, new_task->frame_pointer);
  }

  Is_True(__ompc_task_state_is_exiting(new_task),
      ("__ompc_task_switch: task returned but not in EXITING state"));

  if (new_task->num_children == 0) {
    __ompc_task_delete(new_task);
  } else {
    __ompc_task_set_state(new_task, OMP_TASK_FINISHED);
    __ompc_unlock(&new_task->lock);
  }

  __omp_current_task = orig_task;
}

static inline int __ompc_task_cutoff_num_threads()
{
  return (__ompc_get_num_threads() < __omp_task_cutoff_num_threads_min);
}

static inline int __ompc_task_cutoff_switch(omp_task_t *current_task)
{
  return (current_task->sdepth == __omp_task_cutoff_switch_max);
}

static inline int __ompc_task_cutoff_depth(omp_task_t *current_task)
{
  return (current_task->depth == __omp_task_cutoff_depth_max);
}

static inline int __ompc_task_cutoff_num_children(omp_task_t *current_task)
{
  return (current_task->num_children == __omp_task_cutoff_num_children_max);
}

int __ompc_task_cutoff_default()
{
  omp_task_t *current_task = __omp_current_task;

  return ( current_task == NULL ||
           __omp_task_cutoff_num_threads &&
           __ompc_task_cutoff_num_threads() ||
           __omp_task_cutoff_switch &&
           __ompc_task_cutoff_switch(current_task) ||
           __omp_task_cutoff_depth &&
           __ompc_task_cutoff_depth(current_task) ||
           __omp_task_cutoff_num_children &&
           __ompc_task_cutoff_num_children(current_task));
}

int __ompc_task_cutoff_always()
{
  return 1;
}

int __ompc_task_cutoff_never()
{
  omp_task_t *current_task = __omp_current_task;
  /* never cut off a task, unless the current task is NULL (i.e. in sequential
   * region) */
  return current_task == NULL;
}
