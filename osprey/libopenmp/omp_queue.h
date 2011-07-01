#ifndef __omp_rtl_queue_included
#define __omp_rtl_queue_included

void __ompc_etask_q_init_default(omp_etask_q_t *tq);
void __ompc_etask_q_init_con(omp_etask_q_t *tq);

void __ompc_etask_q_con_enqueue(omp_etask_q_t *tq, omp_etask_t *tail_task);
omp_etask_t * __ompc_etask_q_con_dequeue(omp_etask_q_t *tq);

omp_etask_t * __ompc_etask_q_pop_head_slock(omp_etask_q_t *tq);
omp_etask_t * __ompc_etask_q_pop_head_thlock(omp_etask_q_t *tq);
omp_etask_t * __ompc_etask_q_pop_head_hlock(omp_etask_q_t *tq);

omp_etask_t* __ompc_etask_q_pop_tail_slock(omp_etask_q_t *tq);
omp_etask_t* __ompc_etask_q_pop_tail_htlock(omp_etask_q_t *tq);
omp_etask_t* __ompc_etask_q_pop_tail_tlock(omp_etask_q_t *tq);

void __ompc_etask_q_push_head_slock(omp_etask_q_t *tq, omp_etask_t *head_task);
void __ompc_etask_q_push_head_thlock(omp_etask_q_t *tq, omp_etask_t *head_task);
void __ompc_etask_q_push_head_hlock(omp_etask_q_t *tq, omp_etask_t *head_task);

void __ompc_etask_q_push_tail_slock(omp_etask_q_t *tq, omp_etask_t *tail_task);
void __ompc_etask_q_push_tail_htlock(omp_etask_q_t *tq, omp_etask_t *tail_task);
void __ompc_etask_q_push_tail_tlock(omp_etask_q_t *tq, omp_etask_t *tail_task);

#endif /* __omp_rtl_queue_included */

