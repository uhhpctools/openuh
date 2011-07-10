/*
 Task Implementation for Open64's OpenMP runtime library

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


#ifndef __OMP_ETASK_H__
#define __OMP_ETASK_H__

#include "pcl.h"
#include "omp_rtl.h"

int __ompc_etask_skip_cond_default();
int __ompc_etask_skip_cond_num_children();
int __ompc_etask_skip_cond_queue_load();
int __ompc_etask_skip_cond_depth();

int __ompc_etask_create(omp_task_func taskfunc, frame_pointer_t fp, void *args,
                       int may_delay, int is_tied, int blocks_parent);

void __ompc_etask_body_start(); /* not used */

void __ompc_etask_exit();

void __ompc_etask_wait();

inline void
__ompc_etask_switch(omp_etask_t *new_task)
{
  omp_etask_t *orig_task = __omp_current_etask;

  new_task->state = OMP_TASK_RUNNING;
  __omp_current_etask = new_task;

  (__omp_current_v_thread->sdepth)++;

  if (new_task->is_coroutine)
    co_call(new_task->t.coro);
  else
    __omp_current_etask->t.func(__omp_current_etask->args,
                                __omp_current_etask->fp);

  (__omp_current_v_thread->sdepth)--;

  Is_True(__omp_current_etask->state == OMP_TASK_EXITING,
      ("__omp_current_etask returns but is not in EXITING state"));

  if (__omp_current_etask->num_children == 0) {
    free(__omp_current_etask);
  } else {
    __omp_current_etask->state = OMP_TASK_FINISHED;
  }
  __omp_current_etask = orig_task;
}


#endif
