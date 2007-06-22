/*
 *  Copyright (C) 2000, 2001 HPC,Tsinghua Univ.,China .  All Rights Reserved.
 *
 *      This program is free software; you can redistribute it and/or modify it
 *  under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *      This program is distributed in the hope that it would be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *      Further, this software is distributed without any warranty that it is
 *  free of the rightful claim of any third person regarding infringement
 *  or the like.  Any license provided herein, whether implied or
 *  otherwise, applies only to this software file.  Patent licenses, if
 *  any, provided herein do not apply to combinations of this program with
 *  other software, or any other product whatsoever.
 *
 *      You should have received a copy of the GNU General Public License along
 *  with this program; if not, write the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */

/*
 * File: omp_lock.h
 * Abstract: implementation of OpenMP lock and critical sections
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 * 
 */

#ifndef __omp_lock_included
#define __omp_lock_included

#ifndef __OPENMP_LOCK_TYPE_DEFINED_
#define __OPENMP_LOCK_TYPE_DEFINED_

#include <pthread.h>

typedef pthread_mutex_t omp_lock_t;

typedef struct {
   omp_lock_t      lock, wait;
   pthread_t       thread_id;
   int             count;
} omp_nest_lock_t;

#endif

extern void __ompc_init_lock (volatile omp_lock_t *);
extern void __ompc_lock (volatile omp_lock_t *);
extern void __ompc_unlock (volatile omp_lock_t *);
extern void __ompc_destroy_lock (volatile omp_lock_t *);
extern int __ompc_test_lock (volatile omp_lock_t *);

extern void __ompc_init_nest_lock (volatile omp_nest_lock_t *);
extern void __ompc_nest_lock (volatile omp_nest_lock_t *);
extern void __ompc_nest_unlock (volatile omp_nest_lock_t *);
extern void __ompc_destroy_nest_lock (volatile omp_nest_lock_t *);
extern int __ompc_test_nest_lock (volatile omp_nest_lock_t *);

extern void __ompc_critical(int gtid, volatile omp_lock_t **lck);
extern void __ompc_end_critical(int gtid, volatile omp_lock_t **lck);

#endif

