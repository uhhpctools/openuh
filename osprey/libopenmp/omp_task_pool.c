/*
 Task Pool Implementation for Open64's OpenMP runtime library

 Copyright (C) 2011 University of Houston.

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

#include <stdlib.h>
#include <assert.h>
#include "omp_rtl.h"
#include "omp_sys.h"

/* Actual task pool implementations can be found in omp_task_pool_*.c
 *
 * Current implementations:
 *
 * per_thread1:  single-level, work-stealing task pool, 1 task queue per
 *               thread which holds both tied and untied tasks.
 */

/* task pool size */
int task_queue_num_slots = 0;

/* these will point to appropriate implementation at runtime */
omp_task_pool_t *(*__ompc_create_task_pool)(int team_size);
omp_task_pool_t *(*__ompc_expand_task_pool)(omp_task_pool_t *pool,
                                            int new_team_size);
int (*__ompc_add_task_to_pool)(omp_task_pool_t *pool, omp_task_t *new_task);
omp_task_t *(*__ompc_remove_task_from_pool)(omp_task_pool_t *pool);
void (*__ompc_destroy_task_pool)(omp_task_pool_t *pool);

omp_queue_item_t (*__ompc_task_queue_get)(omp_queue_t *q);
omp_queue_item_t (*__ompc_task_queue_steal)(omp_queue_t *q);
int (*__ompc_task_queue_put)(omp_queue_t *q, omp_queue_item_t item);
int (*__ompc_task_queue_donate)(omp_queue_t *q, omp_queue_item_t item);
