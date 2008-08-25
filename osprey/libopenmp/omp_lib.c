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
 * File: omp_lib.c
 * Abstract: implementation of OpenMP run-time library subroutines
 *          for OpenMP programmer
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 * 
 */

#include <stdlib.h>

#include "omp_rtl.h"
#include "omp_lock.h" 

#include <sys/time.h>

void
__omp_fatal(char *msg)
{
  fprintf(stderr, "OpenMP Run-time Library FATAL error:\n %s\n", msg);
  exit(-1);
}


/*
 * OpenMP standard library function
 */

inline void
omp_set_num_threads(omp_int_t num)
{
  __ompc_set_num_threads(num);
}


void omp_set_num_threads_(omp_int_t);

#pragma weak omp_set_num_threads_ = omp_set_num_threads

inline omp_int_t
omp_get_num_threads(void)
{
  return (omp_int_t)__ompc_get_num_threads();
}


omp_int_t omp_get_num_threads_(void);
#pragma weak omp_get_num_threads_ = omp_get_num_threads

inline omp_int_t
omp_get_max_threads(void)
{
  return (omp_int_t)__ompc_get_max_threads();
}


omp_int_t omp_get_max_threads_(void);
#pragma weak omp_get_max_threads_ = omp_get_max_threads

inline omp_int_t
omp_get_thread_num(void)
{
  return (omp_int_t)__ompc_get_local_thread_num();
}


omp_int_t omp_get_thread_num_(void);
#pragma weak omp_get_thread_num_ = omp_get_thread_num

inline omp_int_t
omp_get_num_procs(void)
{
  return (omp_int_t)__ompc_get_num_procs();
}


omp_int_t omp_get_num_procs_(void);
#pragma weak omp_get_num_procs_ = omp_get_num_procs
inline omp_int_t
omp_in_parallel(void)
{
  return (omp_int_t)__ompc_in_parallel();
}


omp_int_t omp_in_parallel_(void);
#pragma weak omp_in_parallel_ = omp_in_parallel

inline void
omp_set_dynamic(omp_int_t dynamic)
{
  __ompc_set_dynamic(dynamic);
}


void omp_set_dynamic_(omp_int_t);
#pragma weak omp_set_dynamic_ = omp_set_dynamic

inline omp_int_t
omp_get_dynamic(void)
{
  return (omp_int_t)__ompc_get_dynamic();
}


omp_int_t omp_get_dynamic_(void);
#pragma weak omp_get_dynamic_ = omp_get_dynamic

inline void
omp_set_nested(omp_int_t nested)
{
  __ompc_set_nested(nested);
}


void omp_set_nested_(omp_int_t);
#pragma weak omp_set_nested_ = omp_set_nested


inline omp_int_t
omp_get_nested(void)
{
  return (omp_int_t)__ompc_get_nested();
}


omp_int_t omp_get_nested_(void);
#pragma weak omp_get_nested_ = omp_get_nested
/*
 * Lock Functions
 */
inline void
omp_init_lock(volatile omp_lock_t *lock)
{
  __ompc_init_lock_s(lock);
}


void omp_init_lock_(volatile omp_lock_t *);
#pragma weak omp_init_lock_ = omp_init_lock

inline void
omp_init_nest_lock(volatile omp_nest_lock_t *lock)
{
  __ompc_init_nest_lock(lock);
}


void omp_init_nest_lock_(volatile omp_nest_lock_t *);
#pragma weak omp_init_nest_lock_ = omp_init_nest_lock

inline void
omp_destroy_lock(volatile omp_lock_t *lock)
{
  __ompc_destroy_lock(lock);
}


void omp_destroy_lock_(volatile omp_lock_t *);
#pragma weak omp_destroy_lock_ = omp_destroy_lock
inline void
omp_destroy_nest_lock(volatile omp_nest_lock_t *lock)
{
  __ompc_destroy_nest_lock(lock);
}


void omp_destroy_nest_lock_(volatile omp_nest_lock_t *);
#pragma weak omp_destroy_nest_lock_ = omp_destroy_nest_lock
inline void
omp_set_lock(volatile omp_lock_t *lock)
{
  __ompc_lock_s(lock);
}


void omp_set_lock_(volatile omp_lock_t *);
#pragma weak omp_set_lock_ = omp_set_lock
inline void
omp_set_nest_lock(volatile omp_nest_lock_t *lock)
{
  __ompc_nest_lock_s(lock);
}


void omp_set_nest_lock_(volatile omp_nest_lock_t *);
#pragma weak omp_set_nest_lock_ = omp_set_nest_lock
inline void
omp_unset_lock(volatile omp_lock_t *lock)
{
  __ompc_unlock_s(lock);
}


void omp_unset_lock_(volatile omp_lock_t *);
#pragma weak omp_unset_lock_ = omp_unset_lock
inline void
omp_unset_nest_lock(volatile omp_nest_lock_t *lock)
{
  __ompc_nest_unlock_s(lock);
}


void omp_unset_nest_lock_(volatile omp_nest_lock_t *);
#pragma weak omp_unset_nest_lock_ = omp_unset_nest_lock
inline int
omp_test_lock(volatile omp_lock_t *lock)
{
  return __ompc_test_lock(lock);
}


int omp_test_lock_(volatile omp_lock_t *);
#pragma weak omp_test_lock_ = omp_test_lock
inline int
omp_test_nest_lock(volatile omp_nest_lock_t *lock){
  return __ompc_test_nest_lock(lock);
}
	

int omp_test_nest_lock_(volatile omp_nest_lock_t *);
#pragma weak omp_test_nest_lock_ = omp_test_nest_lock
/*
 * Timer function
 */
omp_wtime_t
omp_get_wtime(void)
{
  struct timeval tval;

  gettimeofday(&tval, NULL);
  return (omp_wtime_t)( (double)tval.tv_sec + 1.0e-6 * (double)tval.tv_usec );
}

omp_wtime_t omp_get_wtime_(void);
#pragma weak omp_get_wtime_ = omp_get_wtime


omp_wtime_t omp_get_wtime__(void);
#pragma weak omp_get_wtime__ = omp_get_wtime

omp_wtime_t
omp_get_wtick(void)
{
  double t1, t2;
	
  t1 = omp_get_wtime();
  do {
    t2 = omp_get_wtime();
  } while(t1 == t2);

  return (omp_wtime_t)(t2 - t1);
}


omp_wtime_t omp_get_wtick_(void);
#pragma weak omp_get_wtick_ = omp_get_wtick


omp_wtime_t omp_get_wtick__(void);
#pragma weak omp_get_wtick__ = omp_get_wtick
