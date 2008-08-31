#ifndef __OMP_TASK_H__
#define __OMP_TASK_H__

#include "omp_rtl.h"


//#define TASK_DEBUG

void __ompc_task_switch(omp_task_t *old, omp_task_t *new)
{

#ifdef TASK_DEBUG
  printf("%d: switching from %X to %X\n", __omp_myid, old, new);
#endif

  if(new->started == 0)
    {
      __omp_tasks_started++;
      new->started = 1;
      new->threadid = __omp_myid;
    }
  __omp_current_task = new;

  /*
  uth_switchto_ex(__omp_myid, old, new);
  */
  new->context_flag=1;
  co_call(new);
}

/*
void inline __ompc_task_enqueue(omp_task_t *task)
{
  if(!task->is_parallel_task)
    __ompc_task_q_put_tail(&__omp_local_task_q[__omp_myid], task);
}
*/

void inline __ompc_task_inc_depth()
{
    __omp_tasks_skipped++;
  (__omp_current_task->pdepth)++;
}

void inline __ompc_task_dec_depth()
{
  (__omp_current_task->pdepth)--;
}

inline omp_task_t* __ompc_task_get(omp_task_func func, void *args, int stacksize)
{
  return co_create(func, args, NULL, stacksize);
}

inline void __ompc_task_delete(omp_task_t *task)
{
  co_delete(task);
}

inline void __ompc_init_vp()
{
  co_vp_init();
}
#endif
