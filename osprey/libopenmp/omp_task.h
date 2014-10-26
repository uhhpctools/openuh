/*
 Task Implementation for Open64's OpenMP runtime library

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


#ifndef __OMP_TASK_H__
#define __OMP_TASK_H__

#include <string.h>
#include "omp_sys.h"
#include "omp_lock.h"
#include "pcl.h"

#define OMP_TASK_STACK_SIZE_DEFAULT     0x010000L /* 64 KB */
#define OMP_TASK_Q_UPPER_LIMIT_DEFAULT 10
#define OMP_TASK_Q_LOWER_LIMIT_DEFAULT 1
#define OMP_TASK_LEVEL_LIMIT_DEFAULT 3
#define OMP_TASK_LIMIT_DEFAULT 2*__omp_level_1_team_size
#define OMP_TASK_MOD_LEVEL_DEFAULT 3
#define OMP_TASK_CREATE_COND_DEFAULT &__ompc_task_true_cond

typedef enum {
  OMP_TASK_UNSCHEDULED,
  OMP_TASK_READY,
  OMP_TASK_RUNNING,
  OMP_TASK_WAITING,
  OMP_TASK_IN_BARRIER,
  OMP_TASK_EXITING,
  OMP_TASK_FINISHED
} omp_task_state_t;

typedef enum {
  OMP_TASK_IS_TIED       = 0x0001,
  OMP_TASK_IS_DEFERRED   = 0x0002,
  OMP_TASK_IS_FINAL      = 0x0004,
  OMP_TASK_IS_COROUTINE  = 0x0008,
  OMP_TASK_BLOCKS_PARENT = 0x0010,
  OMP_TASK_IS_IMPLICIT   = 0x0020
} omp_task_flag_t;

typedef unsigned int omp_task_flags_t;

/* function pointer declarations*/
typedef void (*omp_task_func)(void *, void *);
typedef int (*cond_func)();

/* openmp explicit task */
struct omp_task {
  union {
  omp_task_func   func;
  coroutine_t coro;
  } t;

  void *          frame_pointer;
  void *          firstprivates;

  volatile omp_task_state_t state;

  /* number of children that aren't done */
  volatile int num_children;

  /* a child which blocks the exit of the task because it or its descendants
   * uses the task's stack frame */
  volatile int num_blocking_children;

  int creating_thread_id;
  struct omp_task *parent;

  int depth;  /* parent-child depth */
  int sdepth; /* switching depth: number of tasks for which there is an omp_task_t
                 object that are on the thread's call stack */

  /* prev and next for task queue */
  struct omp_task *prev;
  struct omp_task *next;

  omp_task_flags_t flags;
} __attribute__ ((__aligned__(CACHE_LINE_SIZE)));
typedef struct omp_task omp_task_t;

/* inline functions */

static inline omp_task_state_t
__ompc_task_get_state(omp_task_t *task)
{
  return task->state;
}

static inline void
__ompc_task_set_state(omp_task_t *task, omp_task_state_t state)
{
  task->state = state;
}

static inline const char*
__ompc_task_get_state_string(omp_task_t *task)
{
  switch (task->state)
  {
    case OMP_TASK_UNSCHEDULED: return "UNSCHEDULED"; break;
    case OMP_TASK_READY: return "READY"; break;
    case OMP_TASK_RUNNING: return "RUNNING"; break;
    case OMP_TASK_WAITING: return "WAITING"; break;
    case OMP_TASK_IN_BARRIER: return "IN_BARRIER"; break;
    case OMP_TASK_EXITING: return "EXITING"; break;
    case OMP_TASK_FINISHED: return "FINISEHD"; break;
  }
}

static inline int
__ompc_task_state_is_unscheduled(omp_task_t *task)
{
  return __ompc_task_get_state(task) == OMP_TASK_UNSCHEDULED;
}

static inline int
__ompc_task_state_is_ready(omp_task_t *task)
{
  return __ompc_task_get_state(task) == OMP_TASK_READY;
}

static inline int
__ompc_task_state_is_running(omp_task_t *task)
{
  return __ompc_task_get_state(task) == OMP_TASK_RUNNING;
}

static inline int
__ompc_task_state_is_waiting(omp_task_t *task)
{
  return __ompc_task_get_state(task) == OMP_TASK_WAITING;
}

static inline int
__ompc_task_state_is_in_barrier(omp_task_t *task)
{
  return __ompc_task_get_state(task) == OMP_TASK_IN_BARRIER;
}

static inline int
__ompc_task_state_is_exiting(omp_task_t *task)
{
  return __ompc_task_get_state(task) == OMP_TASK_EXITING;
}

static inline int
__ompc_task_state_is_finished(omp_task_t *task)
{
  return __ompc_task_get_state(task) == OMP_TASK_FINISHED;
}

static inline omp_task_flags_t
__ompc_task_set_flags(omp_task_t *task, omp_task_flags_t flags)
{
  task->flags |= flags;
  return task->flags;
}

static inline omp_task_flags_t
__ompc_task_get_flags(omp_task_t *task, omp_task_flags_t flags)
{
  return (task->flags & flags);
}

static inline int
__ompc_task_is_tied(omp_task_t *task)
{
  return __ompc_task_get_flags(task, OMP_TASK_IS_TIED) != 0;
}

static inline int
__ompc_task_is_deferred(omp_task_t *task)
{
  return __ompc_task_get_flags(task, OMP_TASK_IS_DEFERRED) != 0;
}

static inline int
__ompc_task_is_final(omp_task_t *task)
{
  return __ompc_task_get_flags(task, OMP_TASK_IS_FINAL) != 0;
}

static inline int
__ompc_task_is_coroutine(omp_task_t *task)
{
  return __ompc_task_get_flags(task, OMP_TASK_IS_COROUTINE) != 0;
}

static inline int
__ompc_task_blocks_parent(omp_task_t *task)
{
  return __ompc_task_get_flags(task, OMP_TASK_BLOCKS_PARENT) != 0;
}

static inline int
__ompc_task_is_implicit(omp_task_t *task)
{
  return __ompc_task_get_flags(task, OMP_TASK_IS_IMPLICIT) != 0;
}

static inline omp_task_t *__ompc_task_new_implicit(void)
{
  omp_task_t *new_task =
      (omp_task_t*)aligned_malloc(sizeof(omp_task_t),CACHE_LINE_SIZE);

  Is_True(new_task != NULL, ("couldn't create new task object"));
  memset(new_task, 0, sizeof(omp_task_t));

  __ompc_task_set_flags(new_task, OMP_TASK_IS_IMPLICIT);
  __ompc_task_set_flags(new_task, OMP_TASK_IS_TIED);
  new_task->state = OMP_TASK_RUNNING;

  return new_task;
}

static inline omp_task_t *__ompc_task_new(void)
{
  omp_task_t *new_task =
      (omp_task_t*)aligned_malloc(sizeof(omp_task_t),CACHE_LINE_SIZE);

  Is_True(new_task != NULL, ("couldn't create new task object"));
  memset(new_task, 0, sizeof(omp_task_t));

  new_task->state = OMP_TASK_UNSCHEDULED;

  return new_task;
}

static inline void __ompc_task_delete(omp_task_t *task)
{
  Is_True(task != NULL, ("tried to delete a NULL task"));

  aligned_free(task);
}

static inline void
__ompc_task_set_function(omp_task_t *task, omp_task_func func)
{
  task->t.func = func;
}

static inline void
__ompc_task_set_frame_pointer(omp_task_t *task, void *frame_pointer)
{
  task->frame_pointer = frame_pointer;
}

static inline void
__ompc_task_set_firstprivates(omp_task_t *task, void *firstprivates)
{
  task->firstprivates = firstprivates;
}


/* global variables extern declarations */
extern __thread omp_task_t *__omp_current_task;
extern int (*__ompc_task_cutoff)( void );

extern __thread unsigned long __omp_task_cutoffs;
extern int __omp_task_cutoff_num_threads;
extern int __omp_task_cutoff_switch;
extern int __omp_task_cutoff_depth;
extern int __omp_task_cutoff_num_children;

/* default values when corresponding cutoffs are enabled */
extern int __omp_task_cutoff_num_threads_min;
extern int __omp_task_cutoff_switch_max;
extern int __omp_task_cutoff_depth_max;
extern int __omp_task_cutoff_num_children_max;

/* external API for compiler */
extern int __ompc_task_will_defer(int may_delay);
extern void __ompc_task_create(omp_task_func taskfunc, void *frame_pointer,
              void *firstprivates, int may_delay, int is_tied, int blocks_parent);
extern void __ompc_task_wait();
extern void __ompc_task_exit();

extern void __ompc_task_firstprivates_alloc(void **firstprivates, int size);
extern void __ompc_task_firstprivates_free(void *firstprivates);

/* internal APIs */
extern int __ompc_task_cutoff_default();
extern int __ompc_task_cutoff_always();
extern int __ompc_task_cutoff_never();

extern void __ompc_task_switch(omp_task_t *new_task);


#endif  /* __OMP_TASK_H */
