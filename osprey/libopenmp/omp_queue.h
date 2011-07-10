/*
 Task Queue Implementation for Open64's OpenMP runtime library

 Copyright (C) 2008-2011 University of Houston.

 This program is free software; you can redistribute it and/or modify it
 under the terms of version 2 of the GNU General Public License as
 published by the Free Software Foundation.

 This program is distributed in the hope that it would be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 Further, this software is distributed without any warranty that it is
 free of the rightful claim of any third person regarding infringement
 or the like.  Any license provided herein, whether implied or
 otherwise, applies only to this software file.  Patent licenses, if
 any, provided herein do not apply to combinations of this program with
 other software, or any other product whatsoever.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston MA 02111-1307, USA.

 Contact information:
 http://www.cs.uh.edu/~hpctools
*/


#ifndef __omp_rtl_queue_included
#define __omp_rtl_queue_included

#ifndef USE_OLD_TASKS

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

#endif

#endif /* __omp_rtl_queue_included */

