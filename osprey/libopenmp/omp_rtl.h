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
 * File: omp_rtl.h
 * Abstract: implementation of OpenMP run-time library subroutines
 *          for OpenMP programmer
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 *          06/09/2005, updated by Liao Chunhua, Univ. of Houston
 *          06/20/2007, updated by He Jiangzhou, Tsinghua Univ.
 * 
 */
#ifndef __omp_rtl_basic_included
#define __omp_rtl_basic_included

#include "omp_util.h"
#include "omp_type.h"
#include <pthread.h>
#include "omp_lock.h"

/* machine dependent values*/
/* parameters for Itanium2 */
#define CACHE_LINE_SIZE		64	/* L1D size */
#define CACHE_LINE_SIZE_L2L3	128	/* L2,L3 size */

/* default setting values*/
#define OMP_NESTED_DEFAULT	0	
#define OMP_DYNAMIC_DEFAULT	0
#define OMP_NUM_THREADS_DEFAULT 4  	
/* The limit of current pThread lib imposed.
 * Will be eliminated using NPTL
 */
#define OMP_MAX_NUM_THREADS 	256
#define OMP_STACK_SIZE_DEFAULT	0x400000L /* 4MB*/

#define OMP_POINTER_SIZE	8

/* Maybe the pthread_creation should be done in a unit way,
 * currently not implemented*/
/* unit used to allocate date structure */
/*
  #define OMP_ALLOC_UNIT		CACHE_LINE_SIZE_L2L3 / 8 
*/

typedef enum {
  OMP_SCHED_UNKNOWN 	= 0,
  OMP_SCHED_STATIC 	= 1,
  OMP_SCHED_STATIC_EVEN 	= 2,
  OMP_SCHED_DYNAMIC 	= 3,
  OMP_SCHED_GUIDED 	= 4,
  OMP_SCHED_RUNTIME 	= 5,

  OMP_SCHED_ORDERED_UNKNOWN	= 32,
  OMP_SCHED_ORDERED_STATIC 	= 33,
  OMP_SCHED_ORDERED_STATIC_EVEN 	= 34,
  OMP_SCHED_ORDERED_DYNAMIC 	= 35,
  OMP_SCHED_ORDERED_GUIDED 	= 36,
  OMP_SCHED_ORDERED_RUNTIME 	= 37,

  OMP_SCHED_DEFAULT = OMP_SCHED_STATIC_EVEN
} omp_sched_t;

#define OMP_SCHEDULE_DEFAULT	2
#define OMP_CHUNK_SIZE_DEFAULT	1
#define OMP_SCHED_ORDERED_GAP	30


/*  micro_task prototype.
 *  omp_micro(*gtid, NULL, frame_pointer)
 *  Maybe need to revise.
 */
#define frame_pointer_t char*
/* The entry function prototype is nolonger 
 * the seem as GUIDE*/
typedef void (*omp_micro)(int, frame_pointer_t);
typedef void* (*pthread_entry)(void *);

/* global variables used in RTL, declared in omp_thread.c */
extern volatile int __omp_nested;	  /* nested enable/disable */
extern volatile int __omp_dynamic;	  /* dynamic enable/disable */
/* max num of thread available*/
extern volatile int __omp_max_num_threads;
/* stores the number of threads requested for future parallel regions. */
extern volatile int __omp_nthreads_var;
/* num of processors available*/
extern int 	    __omp_num_processors; 
/* default schedule type and chunk size of runtime schedule*/
extern omp_sched_t  __omp_rt_sched_type;
extern int  	    __omp_rt_sched_size;
/* flag, whether the RTL data structure has been initializaed yet*/
extern int 	    __omp_rtl_initialized;

/* Can only be set through environment variable OMP_sTACK_SIZE*/
extern volatile unsigned long int __omp_stack_size;

/* a system level lock, used for malloc in __ompc_get_thdprv ,by Liao*/
extern omp_lock_t _ompc_thread_lock;

/* The OMP_EXE_MODE_NESTED_SEQUENTIAL is of no use any longer*/
typedef enum {
  OMP_EXE_MODE_SEQUENTIAL 	= 1,
  OMP_EXE_MODE_NORMAL		= 2,
  OMP_EXE_MODE_NESTED		= 4,
  OMP_EXE_MODE_NESTED_SEQUENTIAL	= 8, /* should equal to 0?*/
	
  OMP_EXE_MODE_DEFAULT = OMP_EXE_MODE_SEQUENTIAL,
  OMP_EXE_MODE_EXE_SEQUENTIAL 	= 9,
  OMP_EXE_MODE_EXE_PARALLEL 	= 6,
  OMP_EXE_MODE_IN_PARALLEL 	= 14
} omp_exe_mode_t;

/* current execution mode*/
extern volatile omp_exe_mode_t __omp_exe_mode;

typedef struct omp_u_thread omp_u_thread_t;
typedef struct omp_v_thread omp_v_thread_t;
typedef struct omp_team	    omp_team_t;

/* kernel thread*/
/* Should be 64 Bytes, currently 24B*/
struct omp_u_thread{
  pthread_t uthread_id;		/* pthread id*/
  omp_u_thread_t *hash_next;	/* hash link*/
  omp_v_thread_t *task;		/* task(vthread)*/
  /* Maybe a few more bytes should be here for alignment.*/
  /* TODO: stuff bytes*/
  int stuff_byte[10];
}; __attribute__ ((__aligned__(CACHE_LINE_SIZE)))

/* team*/
/* Should be 256 Bytes, currently 208B*/
struct omp_team{
  //	int	team_id;
  int	team_size;
  int	team_level;	/* team_level is not currently used yet*/
  int	is_nested;	
  /* not used yet */
  //	omp_v_thread_t **team_list;

  /* for loop schedule*/

  omp_lock_t	schedule_lock;
  volatile long loop_lower_bound;
  long	loop_upper_bound;
  long	loop_increament;
	
  int	schedule_type;
  long	chunk_size;
  /* For static schedule*/
  //	long	loop_stride;
  /* For ordered dynamic schedule*/
  volatile long schedule_count;
  /* We still need a semphore for scheduler initialization */
  volatile int loop_count;
  /* For scheduler initialization count. */
  //	volatile int schedule_in_count;

  /* for ordered schedule*/
  /* Using schedule_lock as the ordered lock*/
  volatile long	ordered_count;
  pthread_cond_t ordered_cond;

  /* for single*/
  omp_lock_t	single_lock;
  volatile int	single_count; 
  //	volatile int	single_open; /* Single section protector*/
  /* for copyprivate*/
  volatile int	cppriv_counter;
  void * cppriv;

  /* for team barrier*/
  /* TODO: optimize the barrier implementation, test the performance */
  pthread_mutex_t barrier_lock;
  pthread_cond_t barrier_cond;
  volatile int barrier_count;
  volatile int barrier_count2;
  volatile int barrier_flag; /* To indicate all arrived */

  /* Still need a flag to indicate there are new tasks for level_1 team,
   * To avoid pthread allowed spurious wake up, and for nested teams,
   * use this as a semphore to synchronize all thread before they really start*/
  volatile int new_task;
  /* Maybe a few more bytes should be here for alignment.*/
  /* TODO: stuff bytes*/
  int stuff_byte[7];
}; __attribute__ ((__aligned__(CACHE_LINE_SIZE_L2L3)))

/* user thread*/
/* Should be 64 Byte, currently 64 Byte*/
struct omp_v_thread {
  int	vthread_id;
  int	team_size;	/* redundant with team->team_size */
  
  omp_u_thread_t *executor;	/* needed? used anywhere?*/
  //	omp_v_thread_t *creator;	/* needed? used anywhere?*/
  omp_team_t     *team;
	
  volatile omp_micro entry_func;
  volatile frame_pointer_t frame_pointer;

  /* For RUNTIME assigned STATIC schedule only*/
  long schedule_count;
  /* For ordered schedule*/
  long ordered_count;
  long rest_iter_count;
  /* For single sections*/
  int	single_count;
  /* For Dynamic scheduler initialization*/
  int	loop_count;
  /* for 'lastprivate'? used ?*/
  //	int is_last;
  /* Maybe a few more bytes should be here for alignment.*/
  /* TODO: stuff bytes*/
}; __attribute__ ((__aligned__(CACHE_LINE_SIZE)))

/* The array for level 1 thread team, 
 * using vthread_id to index them
 */
extern omp_v_thread_t *  __omp_level_1_team; 
extern omp_u_thread_t *  __omp_level_1_pthread;
extern int		 __omp_level_1_team_size;
extern omp_team_t	 __omp_level_1_team_manager;
extern int		 __omp_level_1_team_alloc_size;
extern omp_u_thread_t *  __omp_uthread_hash_table[UTHREAD_HASH_SIZE]; 

/* Where do they should be initialized? */
extern pthread_t 	 __omp_root_thread_id;
extern omp_v_thread_t	 __omp_root_v_thread; /* necessary?*/
extern omp_u_thread_t *	 __omp_root_u_thread; /* Where do theyshould be initialized somewhere */
extern omp_team_t	 __omp_root_team;     /* hold information for sequential part*/

/* prototypes, implementations are defined in omp_thread.h */
extern void __ompc_set_nested(const int __nested);
extern void __ompc_set_dynamic(const int __dynamic);
extern int __ompc_get_dynamic(void);
extern int __ompc_get_nested(void);
extern int __ompc_get_max_threads(void);
extern int __ompc_get_num_procs(void);
extern void __ompc_set_num_threads(const int __num_threads);
extern int __ompc_in_parallel(void);

extern omp_u_thread_t * __ompc_get_current_u_thread();
extern omp_v_thread_t * __ompc_get_current_v_thread();
extern omp_v_thread_t * __ompc_get_v_thread_by_num(int vthread_id);
extern int __ompc_get_local_thread_num(void);
extern int __ompc_get_num_threads(void);
extern omp_team_t * __ompc_get_current_team(void);
extern void __ompc_barrier_wait(omp_team_t *team);
extern void __ompc_barrier(void);
extern void __ompc_flush(void *p);
extern int __ompc_ok_to_fork(void);
extern void __ompc_begin(void);
extern void __ompc_end(void);

extern void __ompc_fork(const int num_threads, omp_micro micro_task,
			frame_pointer_t frame_pointer);
/* copied from omp_lock.h*/
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

/* TODO:Not implemented yet*/
extern void __ompc_serialized_parallel(int vthread_id);
extern void __ompc_end_serialized_parallel(int vthread_id);
/* Other stuff fuctions*/

/* Maybe we should use libhoard for Dynamic memory 
 * allocation and deallocation,
 * The APIs should lie here. NEED TEST */

/* Support Pathscale OpenMP lowering, CWG */
extern int __ompc_sug_numthreads;

#endif /* __omp_rtl_basic_included */
