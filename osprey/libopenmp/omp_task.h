/*
 Task Implementation for Open64's OpenMP runtime library

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


#ifndef __OMP_TASK_H__
#define __OMP_TASK_H__

#include "omp_rtl.h"


/* note: it is caller's responsibility to enqueue or dequeue tasks as
 * appropriate. __ompc_task_switch will only enable switching from one task to
 * another
 */
void inline __ompc_task_switch(omp_task_t *old_task, omp_task_t *new_task)
{
  Is_True(new_task != NULL && new_task->coro != NULL && new_task->desc != NULL,
      ("__ompc_task_switch: new_task is uninitialized"));

#ifdef TASK_DEBUG
  printf("%d: __ompc_task_switch: switching from %X to %X\n", __omp_myid, old_task->coro,
      new_task->coro);
#endif

  __omp_current_task = new_task;
  /* if firstprivate was captured in parent task and passed in via struct, we
   * can "start" the new task as soon as we switch to it. otherwise, it only
   * "starts" in __ompc_task_body_start
   */
#if 1
  if (__omp_current_task->desc->started == 0) {
  __omp_tasks_started++;
  __omp_current_task->desc->started = 1;
  __omp_current_task->desc->threadid = __omp_myid;
  }
#endif

  new_task->desc->safe_to_enqueue = 0;
  new_task->desc->context_flag = 1;
  old_task->desc->safe_to_enqueue = 1;

  co_call(new_task->coro);
}

static void
inline __ompc_task_exit_to(omp_task_t *current, omp_task_t *new_task)
{
  Is_True(current != NULL && current->coro != NULL && current->desc != NULL,
      ("__ompc_task_exit_to: current is uninitialized"));
  Is_True(new_task != NULL && new_task->coro != NULL && new_task->desc != NULL,
      ("__ompc_task_exit_to: new_task is uninitialized"));

#ifdef TASK_DEBUG
  printf("%d: __ompc_task_exit_to: switching from %X to %X\n", __omp_myid, current->coro,
      new_task->coro);
#endif

  __omp_current_task = new_task;
  /* if firstprivate was captured in parent task and passed in via struct, we
   * can "start" the new task as soon as we switch to it. otherwise, it only
   * starts in __ompc_task_body_start
   */
#if 1
  if (__omp_current_task->desc->started == 0) {
  __omp_tasks_started++;
  __omp_current_task->desc->started = 1;
  __omp_current_task->desc->threadid = __omp_myid;
  }
#endif

  new_task->desc->context_flag = 1;
  new_task->desc->safe_to_enqueue = 0;
  /*current->desc->safe_to_enqueue = 1;*/
  pthread_mutex_destroy(&current->lock);
  co_exit_to(new_task->coro);
}

void inline __ompc_task_inc_depth()
{
  Is_True(__omp_current_task != NULL && __omp_current_task->desc != NULL,
      ("__ompc_task_inc_depth: __omp_current_task is uninitialized"));
  __omp_tasks_skipped++;
  __omp_current_task->desc->pdepth++;
}

void inline __ompc_task_dec_depth()
{
  Is_True(__omp_current_task != NULL && __omp_current_task->desc != NULL,
      ("__ompc_task_dec_depth: __omp_current_task is uninitialized"));
  __omp_current_task->desc->pdepth--;
}

static inline omp_task_t* __ompc_task_get(omp_task_func func, frame_pointer_t fp, 
                        void *args, int stacksize)
{
  omp_task_t *new_task = malloc(sizeof(omp_task_t));
  Is_True(new_task != NULL,
      ("__ompc_task_get: new_task could not be allocated"));
  new_task->desc = malloc(sizeof(omp_task_desc_t));
  Is_True(new_task->desc != NULL,
      ("__ompc_task_get: new_task->desc could not be allocated"));
#ifdef UH_PCL
  new_task->coro = co_create(func, args, fp, NULL, stacksize);
#else
  new_task->coro = co_create(func, fp, NULL, stacksize);
#endif
  Is_True(new_task->coro != NULL,
      ("__ompc_task_get: returned coroutine is NULL"));

  return new_task;
}

inline void __ompc_task_delete(omp_task_t *task)
{
  static long cnt = 0;
  Is_True(task != NULL && task->coro != NULL,
      ("__ompc_task_delete: task is uninitialized"));
#ifdef TASK_DEBUG
  printf("%d. %d: __ompc_task_delete: delete task %X\n", ++cnt, __omp_myid, task->coro );
#endif
  __omp_tasks_deleted++;
  co_delete(task->coro);
}

#ifdef USE_OLD_TASKS
inline void __ompc_init_vp()
{
  co_vp_init();
}
#endif

#endif

