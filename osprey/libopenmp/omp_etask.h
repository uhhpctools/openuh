#ifndef __OMP_ETASK_H__
#define __OMP_ETASK_H__

#include "pcl.h"
#include "omp_rtl.h"

int __ompc_etask_skip_cond_default();
int __ompc_etask_skip_cond_num_children();
int __ompc_etask_skip_cond_queue_load();

int __ompc_etask_create(omp_task_func taskfunc, frame_pointer_t fp, void *args,
                       int may_delay, int is_tied, int blocks_parent);

void __ompc_etask_body_start(); /* not used */

void __ompc_etask_exit();

void __ompc_etask_wait();

inline void
__ompc_etask_switch(omp_etask_t *new_task)
{
  omp_etask_t *orig_task = __omp_current_etask;

  new_task->state = OMP_TASK_RUNNING;
  __omp_current_etask = new_task;

  (__omp_current_v_thread->sdepth)++;

  if (new_task->is_coroutine)
    co_call(new_task->t.coro);
  else
    __omp_current_etask->t.func(__omp_current_etask->args,
                                __omp_current_etask->fp);

  (__omp_current_v_thread->sdepth)--;

  Is_True(__omp_current_etask->state == OMP_TASK_EXITING,
      ("__omp_current_etask returns but is not in EXITING state"));

  /* would like to avoid using lock if possible */
  __ompc_lock(&__omp_current_etask->lock);
  if (__omp_current_etask->num_children == 0) {
    __ompc_unlock(&__omp_current_etask->lock);
    free(__omp_current_etask);
  } else {
    __omp_current_etask->state = OMP_TASK_FINISHED;
    __ompc_unlock(&__omp_current_etask->lock);
  }
  __omp_current_etask = orig_task;
}


#endif
