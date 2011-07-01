#include <stdlib.h>
#include <assert.h>
#include "omp_rtl.h"
#include "omp_sys.h"

/* only used in array version */
#define OMP_TASK_Q_SIZE (__omp_task_q_upper_limit * 4)
#define  __ompc_dec_mod(x)                      \
  do {                                          \
    if(*x == 0) *x = OMP_TASK_Q_SIZE - 1;       \
    else *x = *x - 1;                           \
  } while(0)


#ifndef USE_OLD_TASKS

/*
 * etask queue stuff
 */

void
__ompc_etask_q_init(omp_etask_q_t *tq)
{
  Is_True(tq != NULL, ("tq is NULL"));
  bzero(tq, sizeof(omp_etask_q_t));
}

omp_etask_t *
__ompc_etask_q_pop_head(omp_etask_q_t *tq)
{
  omp_etask_t *head_task;

  __ompc_lock(&tq->head_lock);
  if (tq->size == 0)  {
    __ompc_unlock(&tq->head_lock);
    return NULL;
  }

  if (tq->size > 2) {
    head_task = tq->head;
    tq->head = head_task->next;
    tq->head->prev = NULL;
    __ompc_atomic_dec(&tq->size);
  } else {
    __ompc_lock(&tq->tail_lock);
    head_task = tq->head;
    if (tq->size == 2) {
      tq->head = tq->tail;
      tq->head->prev = NULL;
      __ompc_atomic_dec(&tq->size);
    } else {
      tq->head = tq->tail = NULL;
      __ompc_atomic_dec(&tq->size);
    }
    Is_True(head_task != NULL, ("head_task is NULL, size was %d\n", tq->size));
    __ompc_unlock(&tq->tail_lock);
  }
  __ompc_unlock(&tq->head_lock);

  head_task->prev = head_task->next = NULL;
  return head_task;
}

omp_etask_t*
__ompc_etask_q_pop_tail(omp_etask_q_t *tq)
{
  omp_etask_t *tail_task;

  __ompc_lock(&tq->head_lock);
  __ompc_lock(&tq->tail_lock);
  if (tq->size == 0) {
    __ompc_unlock(&tq->head_lock);
    __ompc_unlock(&tq->tail_lock);
    return NULL;
  }

  if (tq->size > 2) {
    __ompc_unlock(&tq->head_lock);
    tail_task = tq->tail;
    tq->tail = tail_task->prev;
    tq->tail->next = NULL;
    __ompc_atomic_dec(&tq->size);
  } else {
    tail_task = tq->tail;
    if (tq->size == 2) {
      tq->tail = tq->head;
      tq->head->next = NULL;
      __ompc_atomic_dec(&tq->size);
    } else {
      tq->tail = tq->head = NULL;
      __ompc_atomic_dec(&tq->size);
    }
    Is_True(tail_task != NULL, ("tail_task is NULL, size was %d\n", tq->size));
    __ompc_unlock(&tq->head_lock);
  }
  __ompc_unlock(&tq->tail_lock);

  tail_task->prev = tail_task->next = NULL;
  return tail_task;
}

void
__ompc_etask_q_push_head(omp_etask_q_t *tq, omp_etask_t *head_task)
{
  Is_True(head_task != NULL, ("trying to push an empty task into queue head"));

  if (head_task == NULL) return;

  __ompc_lock(&tq->head_lock);
  if (tq->size > 2) {
    head_task->next = tq->head;
    tq->head->prev = head_task;
    tq->head = head_task;
    __ompc_atomic_inc(&tq->size);
  } else {
    __ompc_lock(&tq->tail_lock);

    if (tq->size != 0) {
      head_task->next = tq->head;
      tq->head->prev = head_task;
      tq->head = head_task;
    } else {
      tq->head = tq->tail = head_task;
    }

    __ompc_atomic_inc(&tq->size);
    __ompc_unlock(&tq->tail_lock);
  }
  __ompc_unlock(&tq->head_lock);
}

void
__ompc_etask_q_push_tail(omp_etask_q_t *tq, omp_etask_t *tail_task)
{
  Is_True(tail_task != NULL, ("trying to push an empty task into queue tail"));

  if (tail_task == NULL) return;

  __ompc_lock(&tq->head_lock);
  __ompc_lock(&tq->tail_lock);
  if (tq->size > 2) {
    __ompc_unlock(&tq->head_lock);
    tail_task->prev = tq->tail;
    tq->tail->next = tail_task;
    tq->tail = tail_task;
    __ompc_atomic_inc(&tq->size);
  } else {
    if (tq->size != 0) {
      tail_task->prev = tq->tail;
      tq->tail->next = tail_task;
      tq->tail = tail_task;
    } else {
      tq->tail = tq->head = tail_task;
    }

    __ompc_atomic_inc(&tq->size);
    __ompc_unlock(&tq->head_lock);
  }
  __ompc_unlock(&tq->tail_lock);
}

#else /* USE_OLD_TASKS */


/*
  default is to use doubly linked list version of queues
  change to 0 to use the circular buffer implementation
  must also change the definition of a task queue in omp_rtl.h
*/

/* create a dummy head and tail nodes and set each one to point to the other */

void __ompc_task_q_init(omp_task_q_t *tq)
{
  /*
  tq->head = co_create(NULL, NULL, NULL, 4096);
  Is_True(tq->head != NULL,
	  ("Could not initialize tq->head with dummy coroutine"));

  tq->tail = co_create(NULL, NULL, NULL, 4096);
  Is_True(tq->tail != NULL,
	  ("Could not initialize tq->tail with dummy coroutine"));
      */

  tq->head = malloc(sizeof(omp_task_t));
  Is_True(tq->head != NULL,
	  ("Could not initialize tq->head"));
  tq->head->coro = NULL;
  tq->head->desc = NULL;

  tq->tail = malloc(sizeof(omp_task_t));
  Is_True(tq->tail != NULL,
	  ("Could not initialize tq->tail"));
  tq->tail->coro = NULL;
  tq->tail->desc = NULL;


  tq->size = 0;
  tq->head->next = tq->tail;
  tq->tail->prev = tq->head;

  __ompc_init_lock(&(tq->lock));
}

void __ompc_task_q_get_head(omp_task_q_t *tq, omp_task_t **task)
{
  __ompc_lock(&tq->lock);
  if(tq->head->next == tq->tail) {
    *task = NULL;
  } else {
    *task = tq->head->next;
    tq->head->next = (*task)->next;
    tq->head->next->prev = tq->head;
    tq->size = tq->size - 1;
  }
  __ompc_unlock(&tq->lock);
  Is_True(*task == NULL || (*task)->desc->state != OMP_TASK_EXIT, ("getting a task in exit state from head"));
#ifdef TASK_DEBUG
  if (*task != NULL) {
  printf("%d: getting %X from head of queue %X\n",  *task, tq);
  printf("%d: head = %X; head->next = %X;\n", __omp_myid, tq->head, tq->head->next);
  }
#endif
}

void __ompc_task_q_get_tail(omp_task_q_t *tq, omp_task_t **task)
{
  __ompc_lock(&tq->lock);

  if(tq->head->next == tq->tail) {
    *task = NULL;
  } else {
    *task = tq->tail->prev;
    tq->tail->prev = (*task)->prev;
    tq->tail->prev->next = tq->tail;
    tq->size = tq->size - 1;
  }
  __ompc_unlock(&tq->lock);
  Is_True(*task == NULL || (*task)->desc->state != OMP_TASK_EXIT, ("getting a task in exit state from tail"));
#ifdef TASK_DEBUG
  if (*task != NULL) {
  printf("%d: getting %X from tail of queue %X\n",  *task, tq);
  printf("%d: tail = %X; tail->prev = %X;\n", __omp_myid, tq->tail, tq->tail->prev);
  }
#endif
}

void __ompc_task_q_put_head(omp_task_q_t *tq, omp_task_t *task)
{
  omp_task_t *temp;

  Is_True(task->desc->state != OMP_TASK_EXIT, ("putting a task in exit state to head"));

  __ompc_lock(&tq->lock);

  temp = tq->head->next;
  tq->head->next = task;
  task->prev = tq->head;
  task->next = temp;
  temp->prev = task;

  tq->size = tq->size + 1;
  __ompc_unlock(&tq->lock);
}

void __ompc_task_q_put_tail(omp_task_q_t *tq, omp_task_t *task)
{
  omp_task_t *temp;

  Is_True(task->desc->state != OMP_TASK_EXIT, ("putting a task in exit state to tail"));

  __ompc_lock(&tq->lock);
#ifdef TASK_DEBUG
  printf("%d: putting %X on queue %X\n", __omp_myid, task, tq);
  printf("%d: tail = %X; tail->prev = %X;\n", __omp_myid, tq->tail, tq->tail->prev);
#endif
  temp = tq->tail->prev;
  tq->tail->prev = task;
  task->next = tq->tail;
  task->prev = temp;
  temp->next = task;
#ifdef TASK_DEBUG
  printf("%d: task->prev = %X; task->next = %X\n", __omp_myid, task->prev, task->next);
#endif
  tq->size = tq->size + 1;
  __ompc_unlock(&tq->lock);
}


/* dump routines */
static char* __dump_task_state(omp_task_state_t state)
{
  switch(state) {
    case OMP_TASK_DEFAULT: return "D";
    case OMP_TASK_SUSPENDED: return "S";
    case OMP_TASK_EXIT: return "X";
  }

  return "?";
}

void __dump_task_q(omp_task_q_t *tq)
{
  int i=1;
  omp_task_t *tp;
  Is_True( tq != NULL, ("__ompc_dump_task_q: tq is NULL"));
  fprintf(stderr, "   \tTASKID\tIMP\tTIED\tSTARTED\t#CHILD\tSTATE\tPARENT\n");

  __ompc_lock(&tq->lock);

  tp = tq->head;
  while (tp != NULL) {
    if (tp->desc != NULL)
      fprintf(stderr, "(%d)\t%x\t%d\t%d\t%d\t%d\t%s\t%x\n",
          i++, tp->coro, tp->desc->is_parallel_task, tp->desc->is_tied,
          tp->desc->started, tp->desc->num_children,
          __dump_task_state(tp->desc->state), tp->creator->coro);
    else
      fprintf(stderr, "(%d)\n", i++);
    tp = tp->next;
  }

  __ompc_unlock(&tq->lock);
}

#endif /* USE_OLD_TASKS */


/* older task queue implementation */

/*
void __ompc_task_q_init(omp_task_q_t *tq)
{
  tq->queue = malloc(sizeof(omp_task_t *) * (OMP_TASK_Q_SIZE));
  tq->head = tq->tail = 0;
  tq->size = 0;
  __ompc_init_lock(&(tq->lock));
}


void __ompc_task_q_put_head(omp_task_q_t *tq, omp_task_t *task)
{
  if(tq->size >= OMP_TASK_Q_SIZE)
    {
      fprintf(stderr, "size >= __omp_task_q_limit\n");
      exit(1);
    }

  __ompc_lock(&(tq->lock));
  tq->head = (tq->head + 1) % OMP_TASK_Q_SIZE;
  tq->queue[tq->head] = task;
  tq->size++;
  __ompc_unlock(&(tq->lock));

}

void __ompc_task_q_put_tail(omp_task_q_t *tq, omp_task_t *task)
{

  if(tq->size >= OMP_TASK_Q_SIZE)
    {
      fprintf(stderr, "size >= __omp_task_q_limit\n");
      exit(1);
    }

  __ompc_lock(&(tq->lock));
  tq->queue[tq->tail] = task;
  __ompc_dec_mod(&tq->tail);
  tq->size++;
  __ompc_unlock(&(tq->lock));

}

void __ompc_task_q_get_head(omp_task_q_t *tq, omp_task_t **task)
{
  __ompc_lock(&(tq->lock));
  if(tq->size <= 0)
    *task = NULL;
  else
    {
      *task = tq->queue[tq->head];
      __ompc_dec_mod(&tq->head);
      tq->size--;
    }
  __ompc_unlock(&(tq->lock));

}

void __ompc_task_q_get_tail(omp_task_q_t *tq, omp_task_t **task)
{
  __ompc_lock(&(tq->lock));
  if(tq->size <= 0)
    *task = NULL;
  else
    {
      tq->tail = (tq->tail + 1) % OMP_TASK_Q_SIZE;
      *task = tq->queue[tq->tail];
      tq->size--;
    }
  __ompc_unlock(&(tq->lock));
}

*/

