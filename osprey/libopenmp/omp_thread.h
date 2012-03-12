/*
 * Copyright (C) 2009-2010 Advanced Micro Devices, Inc.  All Rights Reserved.
 */

/*

  OpenMP runtime library to be used in conjunction with Open64 Compiler Suites.

  Copyright (C) 2003 - 2009 Tsinghua University.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
  
  Contact information: HPC Institute, Department of Computer Science and Technology,
  Tsinghua University, Beijing 100084, CHINA, or:

  http://hpc.cs.tsinghua.edu.cn
  
*/

/* Copyright (C) 2006-2011 University of Houston.  */

/*
 * File: omp_thread.h
 * Abstract: routines for thread management
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 * 
 */

#ifndef __omp_rtl_thread_included
#define __omp_rtl_thread_included

#include "omp_rtl.h"
#include "omp_sys.h"
#include "omp_util.h"
#include "omp_xbarrier.h"
#include "pcl.h"
#include <time.h>
#include <unistd.h>

/* Interfaces have already been defined in omp_rtl.h.
 * Since implementations are included here, not include
 * this file anymore. Use omp_rtl.h instead.
 */

extern pthread_mutex_t __omp_hash_table_lock;

extern int __ompc_init_rtl(int num_threads);

extern int __ompc_check_num_threads(const int _num_threads);
extern void __ompc_expand_level_1_team(int new_num_threads);

extern void (*__ompc_xbarrier_wait)(omp_team_t *team);


static inline void __ompc_check_rtl_init()
{
  if (__omp_rtl_initialized == 0)
    __ompc_init_rtl(0);
}

inline void __ompc_set_nested(const int __nested)
{
  /* A lock is needed here to protect it?*/
  __omp_nested = __nested;
}

inline void __ompc_set_dynamic(const int __dynamic)
{
  __omp_dynamic = __dynamic;
}

inline int __ompc_get_dynamic(void)
{
  return __omp_dynamic;
}

inline int __ompc_get_nested(void)
{
  return __omp_nested;
}

static inline int __omp_get_cpu_num()
{
#ifndef TARG_LOONGSON
  cpu_set_t cpuset;
  int return_val, i, cur_count = 0;

  return_val = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  Is_True(return_val == 0, ("Get affinity error"));

  for (i = 0; i < __omp_num_hardware_processors; i++)
    if (CPU_ISSET(i, &cpuset)) cur_count ++;

  return cur_count;
#else
  return (int) sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

inline int __ompc_get_max_threads(void)
{
  /*Could be called in 1. sequential part or 2. parallel region

  1.  for sequential part invoking: 
    return the value of OMP_NUM_THREADS or
		    number of available processors
    cannot use the internal var because the RTL may not yet been initialized!!
  2. for a parallel region: return the initialized internal var.
  */
  char *env_var_str;
  int  env_var_val;

  __ompc_check_rtl_init();
  return __omp_nthreads_var;
}

inline int __ompc_get_num_procs(void)
{
  if (__omp_rtl_initialized == 1)
    return __omp_num_processors;
  else
    return __omp_get_cpu_num();
}

inline void __ompc_set_num_threads(const int __num_threads)
{
  int num_threads;

  __ompc_check_rtl_init();

  // check the validity of _num_threads
  num_threads = __ompc_check_num_threads(__num_threads);

  // expand the team when the current threads are fewer
  if (num_threads > __omp_level_1_team_alloc_size)
     __ompc_expand_level_1_team(num_threads);

  __omp_nthreads_var = num_threads; 
 
}

inline int __ompc_in_parallel(void)
{
  return (__omp_exe_mode != OMP_EXE_MODE_SEQUENTIAL);
}

static inline void __ompc_clear_hash_table(void)
{
  memset(__omp_uthread_hash_table, 0, sizeof(__omp_uthread_hash_table));
}

static inline void __ompc_insert_into_hash_table(omp_u_thread_t * new_u_thread)
{
  omp_u_thread_t *u_thread_temp;
  int hash_index;
  pthread_t uthread_id = new_u_thread->uthread_id;

  hash_index = HASH_IDX(uthread_id);
	
  pthread_mutex_lock(&__omp_hash_table_lock);

  u_thread_temp = __omp_uthread_hash_table[hash_index];
  /* maybe NULL */
  __omp_uthread_hash_table[hash_index] = new_u_thread;
  new_u_thread->hash_next = u_thread_temp;
  pthread_mutex_unlock(&__omp_hash_table_lock);
}

static inline void __ompc_remove_from_hash_table(pthread_t uthread_id)
{
  omp_u_thread_t *uthread_temp;
  int hash_index;

  hash_index = HASH_IDX(uthread_id);
  pthread_mutex_lock(&__omp_hash_table_lock);

  uthread_temp = __omp_uthread_hash_table[hash_index];
  Is_True( uthread_temp != NULL, ("No such pthread in hash table"));
  if (uthread_temp->uthread_id == uthread_id)
    __omp_uthread_hash_table[hash_index] = uthread_temp->hash_next;
  else {
    omp_u_thread_t *uthread_next = uthread_temp->hash_next;
    while (uthread_next != NULL) {
      if (uthread_next->uthread_id == uthread_id) {
	uthread_temp->hash_next = uthread_next->hash_next;
        // release the lock before returning!! bug 359 Liao
        pthread_mutex_unlock(&__omp_hash_table_lock);
	return;
      } else {
	uthread_temp = uthread_next;
	uthread_next = uthread_next->hash_next;
      }
    }
    Is_True(0, ("No such pthread in hash table"));
  }
  pthread_mutex_unlock(&__omp_hash_table_lock);
}

inline omp_u_thread_t * __ompc_get_current_u_thread()
{
  omp_u_thread_t *uthread_temp;
  pthread_t current_uthread_id;
	
  Is_True(__omp_uthread_hash_table != NULL, 
	  ("RTL data structures haven't been initialized yet!"));

  current_uthread_id = pthread_self();

  uthread_temp = __omp_uthread_hash_table[HASH_IDX(current_uthread_id)];

  Is_True(uthread_temp != NULL, ("This pThread is not in hash table!"));

  if (uthread_temp->uthread_id == current_uthread_id)
    return uthread_temp;

  else {
    do {
      /* is mutual exclusion necessary here? */
      uthread_temp = uthread_temp->hash_next;
      Is_True(uthread_temp != NULL, 
	      ("This pThread is not in hash table!"));
    } while (uthread_temp->uthread_id != current_uthread_id);

    return uthread_temp;
  }
}

inline omp_v_thread_t * __ompc_get_current_v_thread()
{
  omp_v_thread_t *v_thread_temp;

  v_thread_temp = __ompc_get_current_u_thread()->task;
  Is_True(v_thread_temp != NULL,
	  ("task structure of u_thread not properly set!"));
  return v_thread_temp;
}

inline omp_v_thread_t * __ompc_get_v_thread_by_num( int vthread_id )
{
  omp_v_thread_t *v_thread_temp;

  /* maybe first we should make sure the vthread_id is right,
   * TODO: check the validity of vthread_id. csc
   */

  if (__omp_exe_mode & OMP_EXE_MODE_NORMAL) {
    v_thread_temp = &(__omp_level_1_team[vthread_id]);
    Is_True(v_thread_temp != NULL, 
	    ("something wrong with level_1_team!"));
    return v_thread_temp;
  } else if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
    return &__omp_root_v_thread;
  } else {
    /* use TLS variable here instead since current pthread id may not be in
     * hash table */
    return __omp_current_v_thread;
    /* return __ompc_get_current_v_thread(); */
  }
}

inline int __ompc_get_local_thread_num(void)
{
  if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
    return 0;
  } else {
    return __ompc_get_current_v_thread()->vthread_id;
  }
}
	
inline int __ompc_get_num_threads(void)
{
  if (__omp_exe_mode & OMP_EXE_MODE_NORMAL) {
    return __omp_level_1_team_size;
  } else if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
    return 1;
  } else {
    return __ompc_get_current_v_thread()->team_size;
  }
}

inline omp_team_t * __ompc_get_current_team(void)
{
  if (__omp_exe_mode & OMP_EXE_MODE_NORMAL)
    return &__omp_level_1_team_manager;
  else if(__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
    /* tempory solution. Maybe a valid team should be there*/
    /* Runtime library should make distinction between
     * SEQUENTIAL execution and other execution mode, Since
     * __omp_root_team may not be initialized yet*/
    return &__omp_root_team;
  } else {
    return __ompc_get_current_v_thread()->team;
  }
}

extern __thread int total_tasks;
extern long int __omp_spin_count; // defined in omp_thread.c
extern __thread int total_tasks;

/* Should not be called directly, use __ompc_barrier instead*/
void __ompc_barrier_wait(omp_team_t *team)
{
  /*Warning: This implementation may cause cache problems*/
  int barrier_flag;
  int reset_barrier;
  int new_count;
  int i;
  volatile int *barrier_flag_p;
  omp_task_t *next, *current_task;
  omp_task_pool_t *pool;

  reset_barrier = 0;
  barrier_flag_p = &(team->barrier_flag);
  barrier_flag = *barrier_flag_p;

  pool = team->task_pool;
  current_task = __omp_current_task;

  __ompc_task_set_state(current_task, OMP_TASK_IN_BARRIER);

  __ompc_atomic_inc(&team->barrier_count);

  /* why not use pthread_cond_wait instead of a busy wait? */
  while(__ompc_task_pool_num_pending_tasks(pool) ||
        team->barrier_count != team->team_size) {
    next = __ompc_remove_task_from_pool(pool);
    if(next != NULL) {
      __ompc_task_switch(next);
    }
  }

  new_count = __ompc_atomic_inc(&team->barrier_count2);

  if (new_count == team->team_size) {
    /* The last one reset flags*/
    team->barrier_flag = barrier_flag ^ 1; /* Xor: toggle*/
    team->barrier_count = 0;
    team->barrier_count2 = 0;

    pthread_mutex_lock(&(team->barrier_lock));
    pthread_cond_broadcast(&(team->barrier_cond));
    pthread_mutex_unlock(&(team->barrier_lock));

  } else {
    /* Wait for the last to reset te barrier*/
    /* We must make sure that every waiting thread get this
     * signal */
    for (i = 0; i < __omp_spin_count; i++)
      if ((*barrier_flag_p) != barrier_flag) {
        return;
      }
    pthread_mutex_lock(&(team->barrier_lock));
    while (team->barrier_flag == barrier_flag) {
      pthread_cond_wait(&(team->barrier_cond), &(team->barrier_lock));
    }
    pthread_mutex_unlock(&(team->barrier_lock));
  }

  __ompc_task_set_state(current_task, OMP_TASK_RUNNING);

}



/* vthread_id is of no use in this implementation*/
/* exposed to outer world, should be unified*/
inline void __ompc_barrier(void)
{
  omp_v_thread_t *temp_v_thread;
  omp_v_thread_t *p_vthread = __ompc_get_v_thread_by_num( __omp_myid);
  p_vthread->thr_ibar_state_id++;
  __ompc_set_state(THR_IBAR_STATE);
  __ompc_event_callback(OMP_EVENT_THR_BEGIN_IBAR); 

  if (__omp_exe_mode & OMP_EXE_MODE_NORMAL) {
    __ompc_xbarrier_wait(&__omp_level_1_team_manager);
    __ompc_event_callback(OMP_EVENT_THR_END_IBAR);
    __ompc_set_state(THR_WORK_STATE);
    return;
    //		}
    //		else
    //			return;
  } else if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
    __ompc_event_callback(OMP_EVENT_THR_END_IBAR);
    __ompc_set_state(THR_WORK_STATE); 
   return;
   }
  /* other situations*/
  temp_v_thread = __ompc_get_current_v_thread();
  if(temp_v_thread->team_size == 1) {
    __ompc_event_callback(OMP_EVENT_THR_END_IBAR);
    __ompc_set_state(THR_WORK_STATE);
    return;
  }
  else {
    __ompc_barrier_wait(temp_v_thread->team);
  }
__ompc_event_callback(OMP_EVENT_THR_END_IBAR);
__ompc_set_state(THR_WORK_STATE);
}

void __ompc_ebarrier(void)
{
    omp_v_thread_t *temp_v_thread;
   omp_v_thread_t *p_vthread = __ompc_get_v_thread_by_num( __omp_myid);
    p_vthread->thr_ebar_state_id++;
  __ompc_set_state(THR_EBAR_STATE);
  __ompc_event_callback(OMP_EVENT_THR_BEGIN_EBAR);
  if (__omp_exe_mode & OMP_EXE_MODE_NORMAL) {
    //          if (__omp_level_1_team_size != 1)
    //          {
    __ompc_xbarrier_wait(&__omp_level_1_team_manager);
    __ompc_event_callback(OMP_EVENT_THR_END_EBAR);
    __ompc_set_state(THR_WORK_STATE);
    return;
    //          }
    //          else 
    //                  return;
                     
  } else if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
     __ompc_event_callback(OMP_EVENT_THR_END_EBAR);
      __ompc_set_state(THR_WORK_STATE);
     return;
    }
  /* other situations*/
  temp_v_thread = __ompc_get_current_v_thread();
  if(temp_v_thread->team_size == 1) {
    __ompc_event_callback(OMP_EVENT_THR_END_IBAR);
    __ompc_set_state(THR_WORK_STATE);
    return;
  }
  else {
    __ompc_barrier_wait(temp_v_thread->team);
  }
  __ompc_event_callback(OMP_EVENT_THR_END_IBAR);
  __ompc_set_state(THR_WORK_STATE);
}

/* Exposed API should be moved to somewhere else, instead of been inlined*/
/* flush needs to do nothing on IA64 based platforms?*/
inline void __ompc_flush(void *p)
{

}

/* stuff function. Required by legacy code for Guide*/
inline int __ompc_can_fork(void)
{
	if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL)
		return 1;
	else if (__omp_exe_mode & OMP_EXE_MODE_NORMAL && __omp_nested == 1)
		return 1;
	else if (__omp_exe_mode & OMP_EXE_MODE_NESTED )
		return 1;
	else return 0;
}

inline void __ompc_begin(void)
{
  /* do nothing */
}

inline void __ompc_end(void)
{
  /* do nothing */
}

#ifndef TARG_LOONGSON
static inline void __omp_get_available_processors()
{
  cpu_set_t cpuset;
  int return_val, i, cur_count=0;
  int *ordered_core_list;
  int core_list_size;

  // Try to bind pthread to cores: Fist try user-specified mapping. 
  // If we cannot get one, we try to figure it out automatically.
  if ( (core_list_size = Get_Affinity_Map(
            &ordered_core_list, __omp_num_hardware_processors)) == 0)
  {
    // We try to bind pthreads to cores on one cpu first. Thus we need to know how
    // cores are ordered. For example, some machine assign 0,4,8,12 to the cores
    // on the first cpu/socket and 1,5,9,13 to the second cpu and etc. We get
    // this info from /proc/cpuinfo. For any reason that we could not find the order,
    // we will use the default order 0,1,2,3...  

    ordered_core_list = (int*) malloc(sizeof(int) * __omp_num_hardware_processors);
    Is_True(ordered_core_list!= NULL,
            ("Can't allocate ordered_core_list"));
    Get_Ordered_Corelist(ordered_core_list, __omp_num_hardware_processors);
    core_list_size = __omp_num_hardware_processors;
  }

  /* create the list to record available processors */
  __omp_list_processors = aligned_malloc(sizeof(int) * core_list_size, CACHE_LINE_SIZE);
  Is_True(__omp_list_processors != NULL,
          ("Can't allocate __omp_list_processors"));

  return_val = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  Is_True(return_val == 0, ("Get affinity error"));
 
  for (i = 0; i < core_list_size; i++) {
     if (CPU_ISSET(ordered_core_list[i], &cpuset))
       __omp_list_processors[cur_count++] = ordered_core_list[i];
  }
  Is_Valid(cur_count > 0, ("no processors were deemed available"));
  __omp_core_list_size = cur_count;

  if (ordered_core_list!= NULL)
    free(ordered_core_list);
}

static int cur_cpu_to_bind;

/* bind the pthread to a specific cpu */
static inline void __ompc_bind_pthread_to_cpu(pthread_t thread)
{
  cpu_set_t cpuset;
  int return_val;
  
  CPU_ZERO(&cpuset);
  CPU_SET(__omp_list_processors[cur_cpu_to_bind],&cpuset);

  return_val = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  Is_True(return_val == 0, ("Set affinity error"));

  // next cpu to bind
  cur_cpu_to_bind = (cur_cpu_to_bind + 1) % __omp_core_list_size; 

}
#endif //TARG_LOONGSON

#endif /* __omp_rtl_thread_included */
