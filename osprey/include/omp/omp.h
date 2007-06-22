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
 * File: omp.h
 * Abstract: the Application Programming Interface of OpenMP
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 *          06/09/2005, updated by Liao Chunhua, Univ. of Houston
 * 
 */

#ifndef _OMP_H
#define _OMP_H

#include <pthread.h>

typedef pthread_mutex_t omp_lock_t;

typedef struct {
   omp_lock_t      lock, wait;
   pthread_t       thread_id;
   int             count;
} omp_nest_lock_t;


typedef int omp_int_t;
typedef double omp_wtime_t;
/* 
typedef void *omp_lock_t;
typedef void * omp_nest_lock_t;

*/
#ifdef __cplusplus
extern "C"{
#endif

/*
 * Excution Environment Functions
 */
extern void omp_set_num_threads(omp_int_t num);
extern omp_int_t omp_get_num_threads(void);
extern omp_int_t omp_get_max_threads(void);
extern omp_int_t omp_get_thread_num(void);
extern omp_int_t omp_get_num_procs(void);

extern omp_int_t omp_in_parallel(void);

extern void omp_set_dynamic(omp_int_t dynamic);
extern omp_int_t omp_get_dynamic(void);

extern void omp_set_nested(omp_int_t nested);
extern omp_int_t omp_get_nested(void);

/*
 * Lock Functions
 */
extern void omp_init_lock(volatile omp_lock_t *lock);
extern void omp_init_nest_lock(volatile omp_nest_lock_t *lock);

extern void omp_destroy_lock(volatile omp_lock_t *lock);
extern void omp_destroy_nest_lock(volatile omp_nest_lock_t *lock);

extern void omp_set_lock(volatile omp_lock_t *lock);
extern void omp_set_nest_lock(volatile omp_nest_lock_t *lock);

extern void omp_unset_lock(volatile omp_lock_t *lock);
extern void omp_unset_nest_lock(volatile omp_nest_lock_t *lock);

extern int omp_test_lock(volatile omp_lock_t *lock);
extern int omp_test_nest_lock(volatile omp_nest_lock_t *lock);
	
extern omp_wtime_t omp_get_wtick(void);
extern omp_wtime_t omp_get_wtime(void);

#ifdef __cplusplus
}
#endif

#endif /* _OMP_H */
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
 * File: omp.h
 * Abstract: the Application Programming Interface of OpenMP
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 *          06/09/2005, updated by Liao Chunhua, Univ. of Houston
 * 
 */

#ifndef _OMP_H
#define _OMP_H

#include <pthread.h>

typedef pthread_mutex_t omp_lock_t;

typedef struct {
   omp_lock_t      lock, wait;
   pthread_t       thread_id;
   int             count;
} omp_nest_lock_t;


typedef int omp_int_t;
typedef double omp_wtime_t;
/* 
typedef void *omp_lock_t;
typedef void * omp_nest_lock_t;

*/
#ifdef __cplusplus
extern "C"{
#endif

/*
 * Excution Environment Functions
 */
extern void omp_set_num_threads(omp_int_t num);
extern omp_int_t omp_get_num_threads(void);
extern omp_int_t omp_get_max_threads(void);
extern omp_int_t omp_get_thread_num(void);
extern omp_int_t omp_get_num_procs(void);

extern omp_int_t omp_in_parallel(void);

extern void omp_set_dynamic(omp_int_t dynamic);
extern omp_int_t omp_get_dynamic(void);

extern void omp_set_nested(omp_int_t nested);
extern omp_int_t omp_get_nested(void);

/*
 * Lock Functions
 */
extern void omp_init_lock(volatile omp_lock_t *lock);
extern void omp_init_nest_lock(volatile omp_nest_lock_t *lock);

extern void omp_destroy_lock(volatile omp_lock_t *lock);
extern void omp_destroy_nest_lock(volatile omp_nest_lock_t *lock);

extern void omp_set_lock(volatile omp_lock_t *lock);
extern void omp_set_nest_lock(volatile omp_nest_lock_t *lock);

extern void omp_unset_lock(volatile omp_lock_t *lock);
extern void omp_unset_nest_lock(volatile omp_nest_lock_t *lock);

extern int omp_test_lock(volatile omp_lock_t *lock);
extern int omp_test_nest_lock(volatile omp_nest_lock_t *lock);
	
extern omp_wtime_t omp_get_wtick(void);
extern omp_wtime_t omp_get_wtime(void);

#ifdef __cplusplus
}
#endif

#endif /* _OMP_H */
