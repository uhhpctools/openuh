#include <stdlib.h>
#include <assert.h>
#include "omp_rtl.h"
#include "pcl.h"
#include "omp_task.h"
#include "omp_sys.h"

#define SCHED6

__thread int breadth = 1;


__thread unsigned int __omp_tasks_started;
__thread unsigned int __omp_tasks_skipped;
__thread unsigned int __omp_tasks_created;
__thread unsigned int __omp_tasks_stolen;
__thread unsigned int __omp_tasks_deleted;

int __ompc_task_depth_cond()
{
  Is_True(__omp_current_task != NULL && __omp_current_task->desc != NULL,
      ("__ompc_task_depth_cond: __omp_current_task is uninitialized"));
  return (__omp_current_task->desc->depth < __omp_task_level_limit);
}

int __ompc_task_depthmod_cond()
{
  Is_True(__omp_current_task != NULL && __omp_current_task->desc != NULL,
      ("__ompc_task_depthmod_cond: __omp_current_task is uninitialized"));
  return __omp_current_task->desc->pdepth % __omp_task_mod_level == 0;
}

int __ompc_task_queue_cond()
{
  omp_team_t *team = __ompc_get_current_team();

  if(breadth && team->public_task_q[__omp_myid].size > __omp_task_q_upper_limit)
    breadth = 0;
  else if(!breadth && team->public_task_q[__omp_myid].size < __omp_task_q_lower_limit)
    breadth = 1;

  return breadth;
}

int __ompc_task_true_cond()
{
  return 1;
}

int __ompc_task_false_cond()
{
  return 0;
}

/* use libhoard instead? */
void __ompc_task_alloc_args(void **args, int size)
{
  *args = aligned_malloc( size, CACHE_LINE_SIZE );
}

void __ompc_task_free_args(void *args)
{
  aligned_free( args );
}

int __ompc_task_numtasks_cond()
{
  return (__ompc_get_current_team()->num_tasks < __omp_task_limit);
}

int __ompc_task_deferred_cond(int may_delay)
{
  return may_delay && __ompc_task_create_cond();
}


int __ompc_task_create(omp_task_func taskfunc, void* fp, void *args,
                      int may_delay, int is_tied, int blocks_parent)
{
  omp_team_t *team;
  omp_task_t *newtask;
  __omp_tasks_created++;

  team = __ompc_get_current_team();
  newtask = __ompc_task_get(taskfunc, fp, args, __omp_task_stack_size);

#if defined(TASK_DEBUG)
  fprintf(stdout,"%d: __ompc_task_create: %lX created %lX\n", __omp_myid, __omp_current_task->coro,
            newtask->coro);
#endif

  if(newtask == NULL || newtask->coro == NULL || newtask->desc == NULL ) {
    fprintf(stderr, "%d: not able to create new tasks\n", __omp_myid);
    exit(1);
  }
  newtask->desc->num_children = 0;
  newtask->desc->is_parallel_task = 0;
  newtask->desc->is_tied = is_tied;
  newtask->desc->may_delay = may_delay;
  newtask->desc->started = 0;
  newtask->desc->safe_to_enqueue = 0;
  newtask->desc->depth = __omp_current_task->desc->depth + 1;
  newtask->desc->pdepth = newtask->desc->depth;
  newtask->desc->blocks_parent = blocks_parent;
  newtask->creator = __omp_current_task;
  pthread_mutex_init(&newtask->lock, NULL);

  /* update number of children - use atomic operation if possible */
  int x;
  x = __ompc_atomic_inc(&__omp_current_task->desc->num_children);
  __ompc_atomic_inc(&(team->num_tasks));

  if (blocks_parent)
    __ompc_atomic_inc(&__omp_current_task->desc->num_blocking_children);


  if (may_delay & ! (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL)) {
     /* only switch here if newtask will switch back upon initializing its
      * firstprivates. in that case, when current task resumes it will enqueue
      * the new task into public queue.
      */
    // __ompc_task_switch(__omp_current_task, newtask);
#ifdef SCHED1
    newtask->desc->threadid = friend;
    __sync_bool_compare_and_swap(&__omp_empty_flags[friend], 1, 0);
    __ompc_task_q_put_tail(&(team->public_task_q[friend]), newtask);
#else
    __ompc_task_q_put_tail(&(team->public_task_q[__omp_myid]), newtask);
#endif
  } else if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
    /* don't enqueue tasks in sequential mode */
    __ompc_task_switch(__omp_current_task, newtask);
  } else {
    if (__omp_current_task->desc->is_tied) {
      __ompc_task_q_put_tail(&(team->private_task_q[__omp_myid]),
          __omp_current_task);
    } else {
      __ompc_task_q_put_tail(&(team->public_task_q[__omp_myid]),
          __omp_current_task);
    }
    __ompc_task_switch(__omp_current_task, newtask);
  }

  return 0;
}

void __ompc_task_body_start()
{

  /* this part is only necessary if firstprivate wasn't captured in parent
   * task and it was to be initialized before __ompc_task_body_start in the
   * task. Otherwise, we can assume the task already "started" before
   * __ompc_task_body_start. If this is enabled, also enable the immediate
   * task switch in __ompc_task_create above.
   */
#if 0
  Is_True(__omp_current_task != NULL && __omp_current_task->desc != NULL,
    ("__ompc_task_body_start: __omp_current_task is uninitialized")); if
  (__omp_current_task->desc->may_delay && !(__omp_exe_mode &
                                            OMP_EXE_MODE_SEQUENTIAL))
  __ompc_task_switch(__omp_current_task, __omp_current_task->creator);

  Is_True(__omp_current_task->desc->started == 0,
      ("already started task called __ompc_task_body_start"));

  __omp_tasks_started++;
  __omp_current_task->desc->started = 1;
  __omp_current_task->desc->threadid = __omp_myid;

#endif
}


void __ompc_task_exit()
{
  omp_task_t *creator;
  omp_task_t *next;
  omp_team_t *team;

  team = __ompc_get_current_team();
  next = NULL;
  Is_True(__omp_current_task != NULL && __omp_current_task->desc != NULL,
      ("__ompc_task_exit: __omp_current_task is uninitialized"));
  creator = __omp_current_task->creator;

  /* decrement num_children of parent*/
  if(creator != NULL) {
    Is_True(creator->desc != NULL,
    ("__ompc_task_exit: creator->desc is uninitialized"));

#ifdef TASK_DEBUG
    printf("%d: task_exit: task = %X: %X->num_children = %d\n", __omp_myid,
        __omp_current_task->coro, creator->coro, creator->desc->num_children);
#endif 
    pthread_mutex_lock(&creator->lock);

    int num_children;

    num_children = __ompc_atomic_dec(&creator->desc->num_children);

#ifdef TASK_DEBUG
    printf("%d: task_exit: parent = %X: task = %X: num_children_left = %d; state = %d\n",
        __omp_myid, creator->coro, __omp_current_task->coro, num_children,
        creator->desc->state);
#endif

    assert(num_children >= 0);

    if ( (num_children > 0) || creator->desc->is_parallel_task) {

      pthread_mutex_unlock(&creator->lock);

    } else if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {

      pthread_mutex_unlock(&creator->lock);

    } else if (creator->desc->state == OMP_TASK_SUSPENDED) {

      int threadid;
      omp_task_q_t *q;
      creator->desc->state = OMP_TASK_DEFAULT;


      while(!creator->desc->safe_to_enqueue) {};

#ifdef TASK_DEBUG
      printf("%d: task_exit: %X placing %X on queue ", __omp_myid,
          __omp_current_task->coro, creator->coro);
#endif
#ifdef SCHED1
      threadid = creator->desc->threadid;
      __sync_bool_compare_and_swap(&__omp_empty_flags[threadid], 1, 0);

#else
      if(creator->desc->is_tied && creator->desc->started) {
        threadid = creator->desc->threadid;
        q = team->private_task_q;
      } else {
        threadid = __omp_myid;
        q = team->public_task_q;
      }
#endif
      __ompc_task_q_put_tail(&q[threadid], creator);

      pthread_mutex_unlock(&creator->lock);

    } else if (creator->desc->state == OMP_TASK_EXIT) {
      __ompc_task_delete(creator);
    } else {
#ifdef TASK_DEBUG
        printf("%d: taskexit: %X state = %d\n", __omp_myid, creator,
            creator->desc->state);
#endif
      pthread_mutex_unlock(&creator->lock);
    }
  }

  if (!__omp_current_task->desc->is_parallel_task)
    __ompc_atomic_dec(&(team->num_tasks));
  __omp_current_task->desc->state = OMP_TASK_EXIT;


  /* before we delete anything we need to wait for all children to complete */
  __ompc_task_wait2(OMP_TASK_EXIT);
}

void __ompc_task_wait()
{
  __ompc_task_wait2(OMP_TASK_SUSPENDED);
}

void __ompc_task_wait2(omp_task_state_t state)
{
  /* tasks calling this function are not in a ready queue, set state to
   * blocked and find another task to execute
   */

  /* if task still has outstanding children, it must wait until its
   * num_children value is zero, when this happens, the last child to complete
   * will either a) add it to the queue if state == OMP_TASK_SUSPENDED or b)
   * reclaim/free the parent v_thread for later use
   */

  Is_True(__omp_current_task != NULL && __omp_current_task->desc != NULL
          && __omp_current_task->coro != NULL,
      ("__ompc_task_exit: __omp_current_task is uninitialized"));

  assert(state == OMP_TASK_SUSPENDED || state == OMP_TASK_EXIT);

  omp_task_t *next;
  omp_task_t *old, *new;
  omp_task_t *current_task = __omp_current_task;
  omp_team_t *team = __ompc_get_current_team();


#ifdef TASK_DEBUG
  fprintf(stdout,"%d: taskwait2: %X num_children = %d; state = %d\n", __omp_myid,
      __omp_current_task->coro, __omp_current_task->desc->num_children, state);
#endif

  while(1) {
    pthread_mutex_lock(&current_task->lock);

    if (current_task->desc->num_children == 0 ||
        (state == OMP_TASK_EXIT && current_task->desc->num_blocking_children == 0)) {
      /* all children have completed */
      /* if state == OMP_TASK_EXIT, we need to delete ourselves and schedule 
         the next task
      */
#ifdef TASK_DEBUG
      printf("%d: taskwait2: %X num_children = 0; state = %d\n", __omp_myid,
          current_task->coro, state);
#endif

      if(state == OMP_TASK_EXIT) {
		    
		    
        current_task->desc->state = OMP_TASK_EXIT;
        if (current_task->desc->blocks_parent)
          __ompc_atomic_dec(&current_task->creator->desc->num_blocking_children);

        pthread_mutex_unlock(&current_task->lock);

        current_task->desc->state = OMP_TASK_DONE;

        if (current_task->desc->is_parallel_task)
          return;

        __ompc_task_schedule(&next);

        Is_True(next == NULL || next->desc->state != OMP_TASK_EXIT,
            ("received task in exit state from scheduler"));

        if (next == NULL)
          next =  __omp_current_v_thread->implicit_task;

        __ompc_task_exit_to(current_task, next);

      } else if(state == OMP_TASK_SUSPENDED) {
        pthread_mutex_unlock(&current_task->lock);
        return;
      }

    } else {

#ifdef TASK_DEBUG
      printf("%d: taskwait2: %X num_children = %d\n", __omp_myid,
          current_task->coro, current_task->desc->num_children);
#endif
      current_task->desc->state = state;
      __ompc_task_schedule(&next);
	
      if(next != NULL || !current_task->desc->is_parallel_task) {
        if (next == NULL)
          next = __ompc_get_v_thread_by_num(__omp_myid)->implicit_task;

        pthread_mutex_unlock(&current_task->lock);
        __ompc_task_switch(current_task, next);

      } else {
        pthread_mutex_unlock(&current_task->lock);
      }
    }
  }
}

void __ompc_task_schedule(omp_task_t **next)
{
  omp_team_t *team = __ompc_get_current_team();

  if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
    *next = __omp_current_task->creator;
    Is_True(*next != NULL,
        ("__ompc_task_schedule: NULL creator in sequential region"));
    return;
  }


  Is_True(team->private_task_q != NULL,
      ("__ompc_task_schedule: private task queue is NULL"));
  Is_True(team->public_task_q != NULL,
      ("__ompc_task_schedule: public task queue is NULL"));

  __ompc_task_q_get_tail(&(team->private_task_q[__omp_myid]), next);
     
  if(*next == NULL) {
    __ompc_task_q_get_tail(&(team->public_task_q[__omp_myid]), next);
  }

#ifdef SCHED1
  if(*next == NULL) {
    __sync_bool_compare_and_swap(&__omp_empty_flags[__omp_myid], 0, 1);
  }
#else
  if(*next == NULL) {

    Is_True(team != NULL, ("_ompc_task_schedule: NULL team"));

    int victim = (rand_r(&__omp_seed) % team->team_size);
	  
    if(__omp_myid != victim) {
      __ompc_task_q_get_head(&(team->public_task_q[victim]), next);

      if(*next != NULL)
        __omp_tasks_stolen++;
    }

  }
#endif
}

inline void __ompc_enqueue_parent()
{
}
