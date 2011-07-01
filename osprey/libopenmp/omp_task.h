#ifndef __OMP_TASK_H__
#define __OMP_TASK_H__

#include "omp_rtl.h"


void inline __ompc_task_switch(omp_task_t *old_task, omp_task_t *new_task)
{
  Is_True(new_task != NULL && new_task->coro != NULL && new_task->desc != NULL,
      ("__ompc_task_switch: new_task is uninitialized"));

#ifdef TASK_DEBUG
  printf("%d: __ompc_task_switch: switching from %X to %X\n", __omp_myid, old_task->coro,
      new_task->coro);
#endif

  __omp_current_task = new_task;

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

inline omp_task_t* __ompc_task_get(omp_task_func func, void *args, int stacksize)
{
  omp_task_t *new_task = malloc(sizeof(omp_task_t));
  Is_True(new_task != NULL,
      ("__ompc_task_get: new_task could not be allocated"));
  new_task->desc = malloc(sizeof(omp_task_desc_t));
  Is_True(new_task->desc != NULL,
      ("__ompc_task_get: new_task->desc could not be allocated"));
  new_task->coro = co_create(func, args, NULL, stacksize);
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

inline void __ompc_init_vp()
{
  co_vp_init();
}
#endif

