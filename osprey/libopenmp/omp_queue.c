#include <stdlib.h>
#include <assert.h>
#include "omp_rtl.h"

/* only used in array version */
#define OMP_TASK_Q_SIZE (__omp_task_q_upper_limit * 4)
#define  __ompc_dec_mod(x)                      \
  do {                                          \
    if(*x == 0) *x = OMP_TASK_Q_SIZE - 1;       \
    else *x = *x - 1;                           \
  } while(0)


/*
  default is to use doubly linked list version of queues
  change to 0 to use the circular buffer implementation
  must also change the definition of a task queue in omp_rtl.h
*/

/* create a dummy head and tail nodes and set each one to point to the other */

void __ompc_task_q_init(omp_task_q_t *tq)
{
  tq->head = co_create(NULL, NULL, NULL, 4096);
  if(tq->head == NULL)
    printf("head = NULL\n");

  tq->tail = co_create(NULL, NULL, NULL, 4096);

  if(tq->tail == NULL)
    printf("tail = NULL\n");


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
}

void __ompc_task_q_put_head(omp_task_q_t *tq, omp_task_t *task)
{
  omp_task_t *temp;

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

    __ompc_lock(&tq->lock);
#ifdef TASK_DEBUG
    printf("%d: putting %X on quueue %lu\n", __omp_myid, task, tq);
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



/* old task queue implementation */

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

