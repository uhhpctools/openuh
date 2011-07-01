#include <stdlib.h>
#include <assert.h>
#include "omp_rtl.h"
#include "omp_sys.h"

omp_etask_t *(*__ompc_etask_schedule)(int);
omp_etask_t *(*__ompc_etask_local_get)(omp_etask_q_t *);
omp_etask_t *(*__ompc_etask_victim_get)(omp_etask_q_t *);
void (*__ompc_etask_enqueue)(omp_etask_t *);


/*
 * __ompc_etask_schedule_default
 *
 * Default, simple scheduler.
 *
 * This will find the next task that's ready for scheduling, remove
 * it from the queue, and then return it.
 *
 * If allow_stealing is 1, then will search for work in a victim task queue
 * (not itself) if there is no work in its own queue. this can be improved,
 * potentially, To seek out untied tasks belonging to other threads. maybe we
 * use a dedicated global queue for untied tasks? Or 1 queue for new tied
 * tasks and 1 queue for untied tasks per thread is another option.
 */
omp_etask_t *__ompc_etask_schedule_default(int allow_stealing)
{
  omp_etask_t *next_task;
  omp_team_t *team;
  omp_etask_q_t *my_queue;
  omp_etask_q_t *other_queue;

  if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) return NULL;

  team = __ompc_get_current_team();
  my_queue = &team->etask_q[__omp_myid];

  if ((next_task = __ompc_etask_local_get(my_queue)) == NULL && allow_stealing) {
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
      if ((next_task = __ompc_etask_victim_get(other_queue)) != NULL)
        return next_task;
    }
  }

  /* look for untied tasks in other queues? ...  */

  return next_task;
}
