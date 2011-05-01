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

int __ompc_task_depth_cond()
{
  return (__omp_current_task->depth < __omp_task_level_limit);
}

int __ompc_task_depthmod_cond()
{
  return __omp_current_task->pdepth % __omp_task_mod_level == 0;
}

int __ompc_task_queue_cond()
{

  if(breadth && __omp_local_task_q[__omp_myid].size > __omp_task_q_upper_limit)
    breadth = 0;
  else if(!breadth && __omp_local_task_q[__omp_myid].size < __omp_task_q_lower_limit)
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

int __ompc_task_numtasks_cond()
{
  return (__omp_level_1_team_manager.num_tasks < __omp_task_limit);
}


int __ompc_task_create(omp_task_func taskfunc, void* fp, int may_delay, int is_tied)
{

  __omp_tasks_created++;
  omp_task_t *newtask = __ompc_task_get(taskfunc, fp, __omp_task_stack_size);

#if defined(TASK_DEBUG)
  fprintf(stdout,"%d: %lX created %lX\n", __omp_myid, __omp_current_task, newtask);
#endif

  if(newtask == NULL) {
    fprintf(stderr, "%d: not able to create new tasks\n", __omp_myid);
    exit(1);
  }
  newtask->num_children = 0;
  newtask->is_parallel_task = 0;
  newtask->is_tied = is_tied;
  newtask->started = 0;
  newtask->creator = __omp_current_task;
  newtask->safe_to_enqueue = 0;
  newtask->depth = __omp_current_task->depth + 1;
  newtask->pdepth = newtask->depth;
  pthread_mutex_init(&newtask->lock, NULL);

  /* update number of children - use atomic operation if possible */
  int x;
  x = __ompc_atomic_inc(&__omp_current_task->num_children);
  __ompc_atomic_inc(&__omp_level_1_team_manager.num_tasks);

  __ompc_task_switch(__omp_current_task, newtask);

  if (may_delay) {
#ifdef SCHED1
    newtask->threadid = friend;
    __sync_bool_compare_and_swap(&__omp_empty_flags[friend], 1, 0);
    __ompc_task_q_put_tail(&__omp_local_task_q[friend], newtask);
#else
    __ompc_task_q_put_tail(&__omp_local_task_q[__omp_myid], newtask);
#endif
  } else {
    __ompc_task_switch(__omp_current_task, newtask);
  }

  return 0;
}

void __ompc_task_body_start()
{
  __ompc_task_switch(__omp_current_task, __omp_current_task->creator);
}


void __ompc_task_exit()
{

  omp_task_t *next;
  next = NULL;
  /* decrement num_children of parent*/
  if(__omp_current_task->creator != NULL) {

#ifdef TASK_DEBUG
    printf("%X: %X->num_children = %d\n", __omp_current_task, __omp_current_task->creator,__omp_current_task->creator->num_children);
#endif 
    pthread_mutex_lock(&__omp_current_task->creator->lock);

    int x;

    x = __ompc_atomic_dec(&__omp_current_task->creator->num_children);

#ifdef TASK_DEBUG
    printf("parent = %X: task = %X: num_children_left = %d; state = %d\n", __omp_current_task->creator, __omp_current_task,x, __omp_current_task->creator->state);
#endif

    assert(x >= 0);
    if((x) == 0 && !__omp_current_task->creator->is_parallel_task ) {
      if(__omp_current_task->creator->state == OMP_TASK_SUSPENDED) {
        __omp_current_task->creator->state = OMP_TASK_DEFAULT;
        while(!__omp_current_task->creator->safe_to_enqueue){};

#ifdef TASK_DEBUG
        printf("%d: task_exit: %X placing %X on queue ", __omp_myid, __omp_current_task, __omp_current_task->creator);
#endif
        int threadid;
        omp_task_q_t *q;
#ifdef SCHED1
        threadid = __omp_current_task->creator->threadid;
        __sync_bool_compare_and_swap(&__omp_empty_flags[threadid], 1, 0);

#else
        if(__omp_current_task->creator->is_tied &&
           __omp_current_task->creator->started) {
          threadid = __omp_current_task->creator->threadid;
          q = __omp_private_task_q;
        } else {
          threadid = __omp_myid;
          q = __omp_local_task_q;
        }
#endif
        __ompc_task_q_put_tail(&q[threadid], __omp_current_task->creator);

      }
#ifdef TASK_DEBUG
      else
        printf("%d: taskexit: %X state = %d\n", __omp_myid, __omp_current_task->creator, __omp_current_task->creator->state);
#endif

    }
    pthread_mutex_unlock(&__omp_current_task->creator->lock);
  }

  __ompc_atomic_dec(&__omp_level_1_team_manager.num_tasks);
  __omp_current_task->state = OMP_TASK_EXIT;


  /* before we delete anything we need to wait for all children to complete */

  __ompc_task_wait2(OMP_TASK_EXIT);
  
}

void __ompc_task_wait()
{
  __ompc_task_wait2(OMP_TASK_SUSPENDED);
}

void __ompc_task_wait2(omp_task_state_t state)
{
  /* tasks calling this function are not in a ready queue, set state to blocked and find another task to execute */

  /* if task still has outstanding children, it must wait until its num_children value is zero, when this happens,
     the last child to complete will either a) add it to the queue if state == OMP_TASK_SUSPENDED or b) reclaim/free the parent v_thread for later use
  */

  assert(state == OMP_TASK_SUSPENDED || state == OMP_TASK_EXIT);

  omp_task_t *next;
  omp_task_t *old, *new;
  omp_task_t *current_task = __omp_current_task;


#ifdef TASK_DEBUG
  fprintf(stdout,"%d: taskwait: %X num_children = %d; state = %d\n", __omp_myid, __omp_current_task, __omp_current_task->num_children, state);
#endif


  while(1) {
    pthread_mutex_lock(&current_task->lock);

    if(current_task->num_children == 0) {
      /* all children have completed */
      /* if state == OMP_TASK_EXIT, we need to delete ourselves and schedule 
         the next task
      */
#ifdef TASK_DEBUG
      printf("%d: taskwait2: %X num_children = 0; state = %d\n", __omp_myid, current_task, state);
#endif

      if(state == OMP_TASK_EXIT) {
		    
        __ompc_task_schedule(&next);
		    
        current_task->state = OMP_TASK_EXIT;
        if(next == NULL)
          next = __omp_level_1_team_tasks[__omp_myid];

        old = NULL;

        pthread_mutex_unlock(&current_task->lock);
        __ompc_task_exit_to(current_task, next);
      } else if(state == OMP_TASK_SUSPENDED) {
        pthread_mutex_unlock(&current_task->lock);
        return;
      }

    } else {

#ifdef TASK_DEBUG
      printf("%d: taskwait: %X num_children = %d\n", __omp_myid, current_task, current_task->num_children);
#endif
      current_task->state = state;
      __ompc_task_schedule(&next);
	
      if(next != NULL || !current_task->is_parallel_task) {
        if(next == NULL)
          next = __omp_level_1_team_tasks[__omp_myid];

        if(state == OMP_TASK_EXIT) {
          old = NULL;
        } else if(state == OMP_TASK_SUSPENDED) {
          old = current_task;
        }
        pthread_mutex_unlock(&current_task->lock);
        __ompc_task_switch(old, next);

      } else
        pthread_mutex_unlock(&current_task->lock);

    }

  }

}

void __ompc_task_schedule(omp_task_t **next)
{
  __ompc_task_q_get_tail(&__omp_private_task_q[__omp_myid], next);
     
  if(*next == NULL) {
    __ompc_task_q_get_tail(&__omp_local_task_q[__omp_myid], next);
  }

#ifdef SCHED1
  if(*next == NULL) {
    __sync_bool_compare_and_swap(&__omp_empty_flags[__omp_myid], 0, 1);
  }
#else
  if(*next == NULL) {

    int victim = (rand_r(&__omp_seed) % __omp_level_1_team_size);
	  
    if(__omp_myid != victim) {
      __ompc_task_q_get_head(&__omp_local_task_q[victim], next);

      if(*next != NULL)
        __omp_tasks_stolen++;
    }

  }
#endif
}

inline void __ompc_enqueue_parent()
{
}
