#include <stdlib.h>
#include <assert.h>
#include "omp_rtl.h"
#include "omp_sys.h"

/* maximum allowed task switching depth */
#define MAX_SDEPTH 20
#define MAX_CHILDREN 10

cond_func __ompc_etask_skip_cond;

/* use libhoard instead? */
void __ompc_task_alloc_args(void **args, int size)
{
  *args = aligned_malloc( size, CACHE_LINE_SIZE );
}

void __ompc_task_free_args(void *args)
{
  aligned_free( args );
}


int __ompc_etask_skip_cond_default()
{
  return ((__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) ||
          __ompc_get_num_threads() == 1              ||
          __omp_current_v_thread->sdepth == MAX_SDEPTH  ||
          __omp_current_etask == NULL);
}

int __ompc_etask_skip_cond_num_children()
{
  return ((__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) ||
          __ompc_get_num_threads() == 1              ||
          __omp_current_v_thread->sdepth == MAX_SDEPTH  ||
          __omp_current_etask == NULL ||
          __omp_current_etask->num_children > MAX_CHILDREN);
}

int __ompc_etask_skip_cond_queue_load()
{
  return ((__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) ||
          __ompc_get_num_threads() == 1              ||
          __omp_current_v_thread->sdepth == MAX_SDEPTH  ||
          __omp_current_etask == NULL ||
          __ompc_get_current_team()->etask_q[__omp_myid].reject);
}

int __ompc_task_deferred_cond(int may_delay)
{
  return may_delay && !__ompc_etask_skip_cond();
}

int __ompc_task_create(omp_task_func taskfunc, frame_pointer_t fp, void *args,
                       int may_delay, int is_tied, int blocks_parent)
{
  omp_team_t *team = __ompc_get_current_team();
  int *has_tied_tasks;

  if (__ompc_etask_skip_cond()) {
    omp_etask_t *orig_task = __omp_current_etask;
    __omp_current_etask = NULL;
    taskfunc(args, fp);
    __omp_current_etask = orig_task;
    return 0; /* what should this return? */
  }

  /* maybe save this somewhere in a __thread variable for quicker access */
  has_tied_tasks = &__ompc_get_current_v_thread(__omp_myid)->has_tied_tasks;

  if (may_delay) {
    omp_etask_t *new_task = malloc(sizeof(omp_etask_t));
    Is_True( new_task != NULL, ("could not allocate new_task"));

    bzero(new_task, sizeof(omp_etask_t));
    new_task->state = OMP_TASK_UNSCHEDULED;
    new_task->t.func = taskfunc;
    new_task->fp = fp;
    new_task->args = args;
    new_task->is_tied = is_tied;
    new_task->may_delay = may_delay;
    new_task->blocks_parent = blocks_parent;
    new_task->depth = __omp_current_etask->depth + 1;
    new_task->parent = __omp_current_etask;

    /* can this be done without an atomic? */
    __ompc_atomic_inc(&team->num_tasks);

    __ompc_atomic_inc(&__omp_current_etask->num_children);

    if(blocks_parent)
      __ompc_atomic_inc(&__omp_current_etask->num_blocking_children);

    /* do we want to track children or descendants? */
    /*
    __omp_current_etask->last_child->right_sib = new_task;
    new_task->left_sib = __omp_current_etask->last_child;
    __omp_current_etask->last_child = new_task;
    */

    /* add new task to task queue for this thread in current team */
    __ompc_etask_q_push_tail(&team->etask_q[__omp_myid], new_task);

  } else {
    omp_etask_t new_immediate_task;
    omp_etask_t *new_task = &new_immediate_task;

    bzero(new_task, sizeof(omp_etask_t));
    new_task->state = OMP_TASK_RUNNING;
    new_task->t.func = taskfunc;
    new_task->fp = fp;
    new_task->args = args;
    new_task->is_tied = is_tied;
    new_task->may_delay = may_delay;
    new_task->blocks_parent = 0;
    new_task->depth = __omp_current_etask->depth + 1;
    new_task->parent = __omp_current_etask;

    /* not necessary if task is executed immediately, right? */
    //__ompc_atomic_inc(&team->num_tasks);

    /* execute the task */
    if (__omp_current_etask->is_tied) {
      omp_etask_t *orig_task = __omp_current_etask;
      (*has_tied_tasks)++;
      __omp_current_etask = new_task;
      taskfunc(args, fp);
      (*has_tied_tasks)--;
      __omp_current_etask = orig_task;
    } else {
      omp_etask_t *orig_task = __omp_current_etask;
      __omp_current_etask = new_task;
      taskfunc(args, fp);
      __omp_current_etask = orig_task;
    }
  }

}

void __ompc_task_body_start()
{
  /*
   * Actually, looking forward, this function isn't necessary because we don't
   * need to signal the "start" of the task body (i.e. the point at which
   * firstprivate variables are initialized), since firstprivate variables
   * should be passed in by value.
   */
}

void __ompc_task_exit()
{
  omp_etask_t *current_task;
  omp_etask_t *next_task = NULL;
  omp_team_t *team;
  /* maybe save this somewhere in a __thread variable for quicker access */
  int *has_tied_tasks = &__ompc_get_current_v_thread(__omp_myid)->has_tied_tasks;

  current_task = __omp_current_etask;

  /* if no etask assigned for current task, just return */
  if (current_task == NULL) return;

  team = __ompc_get_current_team();
  current_task->state = OMP_TASK_EXITING;

  /* only decrement for explicit tasks, and only explicit tasks have a
   * parent */
  if (current_task->parent && current_task->may_delay) {
    __ompc_atomic_dec(&team->num_tasks);
    __ompc_atomic_dec(&current_task->parent->num_children);
  }

  /* only try to free parent or put it back on queue if it was a deferred task
   */
  if (current_task->parent && current_task->parent->may_delay) {
    /* acquire lock on parent. decrement parent's num_children. if
     * num_children is 0 and parent's state is TASK_FINISHED, then go ahead
     * and deallocate the task for the parent. if num_children is 0 and parent
     * is in a taskwait state and parent is a coroutine, then place it on to
     * the queue in a TASK_READY state so that another thread could
     * potentially pick it up. release the lock.
     *
     * would like to avoid using locks if possible, but not sure if possible */
    __ompc_lock(&current_task->parent->lock);
    if (current_task->parent->num_children == 0 &&
        current_task->parent->state == OMP_TASK_FINISHED) {
      __ompc_unlock(&current_task->parent->lock);
      free(current_task->parent);
    } else if ((current_task->parent->is_coroutine) &&
        (current_task->parent->num_children == 0) &&
        (current_task->parent->state == OMP_TASK_WAITING)) {
      /* if the parent is a coroutine, and current task was its last child,
       * and the parent in a taskwait, then allow it to switch to another thread
       * by placing it back into the queue
       */
      current_task->parent->state = OMP_TASK_READY;
      __ompc_etask_q_push_tail(&team->etask_q[__omp_myid], current_task->parent);
      __ompc_unlock(&current_task->parent->lock);
    } else  {
      __ompc_unlock(&current_task->parent->lock);
    }
  }

  /* should not immediately return if descendant tasks may potentially still
   * need access to current call stack. instead, we look for other tasks to
   * execute from this point.  */
  while ( current_task->num_blocking_children) {
    /* only allow stealing of ready tied tasks if this thread has no tasks
     * that are tied to it.  */
    next_task = __ompc_etask_schedule(!(*has_tied_tasks));
    if (next_task != NULL)
      __ompc_etask_switch(next_task);
  }

  /* need to decrement num_blocking_children for parent if this is a deferred
   * task. We put it at the end, to ensure all blocking child tasks have first
   * completed.
   */
  if (current_task->may_delay && current_task->blocks_parent)
    __ompc_atomic_dec(&current_task->parent->num_blocking_children);
}

void __ompc_task_wait()
{
  omp_etask_t *current_task;
  omp_etask_t *next_task = NULL;
  omp_team_t *team;
  int is_tied;
  /* maybe save this somewhere in a __thread variable for quicker access */
  int *has_tied_tasks = &__ompc_get_current_v_thread(__omp_myid)->has_tied_tasks;

  /* if no etask assigned for current task, just return */
  if (__omp_current_etask == NULL) return;

  team = __ompc_get_current_team();
  current_task = __omp_current_etask;
  is_tied = current_task->is_tied;

  current_task->state = OMP_TASK_WAITING;

  /* while there are still children, look for available work in task queues */
  while ( current_task->num_children) {
    /* only allow stealing of ready tied tasks if this thread has no tasks
     * that are tied to it.  */
    next_task = __ompc_etask_schedule(!((*has_tied_tasks)+is_tied));
    if (next_task != NULL) {
        if (is_tied) {
          (*has_tied_tasks)++;
          __ompc_etask_switch(next_task);
          (*has_tied_tasks)--;
        } else {
          __ompc_etask_switch(next_task);
        }
    }
  }

  current_task->state = OMP_TASK_RUNNING;
}

