#include <stdlib.h>
#include <assert.h>
#include "omp_rtl.h"
#include "omp_sys.h"

/* maximum allowed task switching depth */
#define MAX_SDEPTH 20

/* use libhoard instead? */
void __ompc_task_alloc_args(void **args, int size)
{
  *args = aligned_malloc( size, CACHE_LINE_SIZE );
}

void __ompc_task_free_args(void *args)
{
  aligned_free( args );
}


/* this will find the next task that's ready for scheduling, remove
 * it from the queue, and then return it
 *
 * if allow_stealing is 1, then will search for work in a victim task queue
 * (not itself) if there is no work in its own queue. this can be improved,
 * potentially, to seek out untied tasks belonging to other threads. maybe we
 * use a dedicated global queue for untied tasks? or 1 queue for new tied
 * tasks and 1 queue for untied tasks per thread is another option.
 */
omp_etask_t *__ompc_etask_schedule(int allow_stealing)
{
  omp_etask_t *next_task;
  omp_team_t *team;
  omp_etask_q_t *my_queue;
  omp_etask_q_t *other_queue;

  if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) return NULL;

  team = __ompc_get_current_team();
  my_queue = &team->etask_q[__omp_myid];

  if ((next_task = __ompc_etask_q_pop_head(my_queue)) == NULL && allow_stealing) {
    int ts = team->team_size;
    /* find a first victim */
    int first_victim, victim = (rand_r(&__omp_seed) % (ts-1));
    if (victim >= __omp_myid) victim++;
    /* cycle through to find a queue with work to steal */
    first_victim = victim;
    while (1) {
      while (team->etask_q[victim].size == 0) {
        victim++;
        if (victim == ts) victim = 0;
        if (victim == first_victim) return NULL;
      }
      other_queue = &team->etask_q[victim];
      if ((next_task = __ompc_etask_q_pop_tail(other_queue)) != NULL)
        return next_task;
    }
  }

  /* look for untied tasks in other queues? ...  */

  return next_task;
}


/* called from omp_task.c */

int __ompc_task_create(omp_task_func taskfunc, frame_pointer_t fp, void *args,
                       int may_delay, int is_tied, int blocks_parent)
{
  omp_team_t *team = __ompc_get_current_team();
  int *has_tied_tasks;

  if ((__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) ||
      __ompc_get_num_threads() == 1 ||
      (__omp_current_etask == NULL || __omp_current_etask->sdepth == MAX_SDEPTH)) {
    omp_etask_t *orig_task = __omp_current_etask;
    __omp_current_etask = NULL;
    taskfunc(args, fp);
    __omp_current_etask = orig_task;
    return 0; /* what should this return? */
  }

  omp_etask_t *new_task = malloc(sizeof(omp_etask_t));
  Is_True( new_task != NULL, ("could not allocate new_task"));

  /* maybe save this somewhere in a __thread variable for quicker access */
  has_tied_tasks = &__ompc_get_current_v_thread(__omp_myid)->has_tied_tasks;

  bzero(new_task, sizeof(omp_etask_t));
  if (is_tied) {
    new_task->state = OMP_TASK_UNSCHEDULED;
    new_task->t.func = taskfunc;
    new_task->fp = fp;
    new_task->args = args;
    new_task->is_tied = 1;
    new_task->may_delay = may_delay;
    new_task->blocks_parent = blocks_parent;
    new_task->depth = __omp_current_etask->depth + 1;
    new_task->parent = __omp_current_etask;
  } else {
    /* treat untied task identically for now */
    new_task->state = OMP_TASK_UNSCHEDULED;
    new_task->t.func = taskfunc;
    new_task->fp = fp;
    new_task->args = args;
    new_task->is_tied = 0;
    new_task->may_delay = may_delay;
    new_task->blocks_parent = blocks_parent;
    new_task->depth = __omp_current_etask->depth + 1;
    new_task->parent = __omp_current_etask;
    /*
    Is_True( 0, ("untied tasks not yet suported."));
    assert(0);
    */
  }

  __ompc_atomic_inc(&team->num_tasks);

  __ompc_atomic_inc(&__omp_current_etask->num_children);

  if (blocks_parent)
    __ompc_atomic_inc(&__omp_current_etask->num_blocking_children);

  /* do we want to track children or descendants? */
  /*
  __omp_current_etask->last_child->right_sib = new_task;
  new_task->left_sib = __omp_current_etask->last_child;
  __omp_current_etask->last_child = new_task;
  */

  if ( may_delay == 0 ) {
    /* execute the task */
    if (__omp_current_etask->is_tied) {
      (*has_tied_tasks)++;
      __ompc_etask_switch(new_task);
      (*has_tied_tasks)--;
    } else {
      __ompc_etask_switch(new_task);
    }
  } else {
    /* add new task to task queue for this thread in current team */
    __ompc_etask_q_push_tail(&team->etask_q[__omp_myid], new_task);
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
  omp_etask_t *next_task = NULL;
  omp_team_t *team;
  /* maybe save this somewhere in a __thread variable for quicker access */
  int *has_tied_tasks = &__ompc_get_current_v_thread(__omp_myid)->has_tied_tasks;

  /* if no etask assigned for current task, just return */
  if (__omp_current_etask == NULL) return;

  team = __ompc_get_current_team();
  __omp_current_etask->state = OMP_TASK_EXITING;

  if (__omp_current_etask->parent) {
    /* only decrement for implicit tasks, and only explicit tasks have a
     * parent */
    __ompc_atomic_dec(&team->num_tasks);

    /* acquire lock on parent. decrement parent's num_children. if
     * num_children is 0 and parent's state is TASK_FINISHED, then go ahead
     * and deallocate the task for the parent. if num_children is 0 and parent
     * is in a taskwait state and parent is a coroutine, then place it on to
     * the queue in a TASK_READY state so that another thread could
     * potentially pick it up. release the lock.
     *
     * would like to avoid using locks if possible, but not sure if possible */
    __ompc_lock(&__omp_current_etask->parent->lock);
    if (__ompc_atomic_dec(&__omp_current_etask->parent->num_children) == 0 &&
        __omp_current_etask->parent->state == OMP_TASK_FINISHED) {
      __ompc_unlock(&__omp_current_etask->parent->lock);
      free(__omp_current_etask->parent);
    } else if ((__omp_current_etask->parent->is_coroutine) &&
        (__omp_current_etask->parent->num_children == 0) &&
        (__omp_current_etask->parent->state == OMP_TASK_WAITING)) {
      /* if the parent is a coroutine, and current task was its last child,
       * and the parent in a taskwait, then allow it to switch to another thread
       * by placing it back into the queue
       */
      __omp_current_etask->parent->state = OMP_TASK_READY;
      __ompc_etask_q_push_tail(&team->etask_q[__omp_myid], __omp_current_etask->parent);
      __ompc_unlock(&__omp_current_etask->parent->lock);
    } else  {
      __ompc_unlock(&__omp_current_etask->parent->lock);
    }
  }

  /* should not immediately return if descendant tasks may potentially still
   * need access to current call stack. instead, we look for other tasks to
   * execute from this point.  */
  while ( __omp_current_etask->num_blocking_children) {
    /* only allow stealing of ready tied tasks if this thread has no tasks
     * that are tied to it.  */
    next_task = __ompc_etask_schedule(!(*has_tied_tasks));
    if (next_task != NULL)
      __ompc_etask_switch(next_task);
  }

  /* need to decrement num_blocking_children for parent. We put it at the end, to
   * ensure all blocking child tasks have first completed.
   */
  if (__omp_current_etask->blocks_parent)
    __ompc_atomic_dec(&__omp_current_etask->parent->num_blocking_children);
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
