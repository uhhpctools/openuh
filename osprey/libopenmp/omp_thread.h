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
#include "pcl.h"
#include <time.h>
/* Interfaces have already been defined in omp_rtl.h.
 * Since implementations are included here, not include
 * this file anymore. Use omp_rtl.h instead.
 */

extern pthread_mutex_t __omp_hash_table_lock;
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

inline int __ompc_get_max_threads(void)
{
  /*Could be called in 1. sequential part or 2. parallel region

  1.  for sequential part invoking: 
    return the value of OMP_NUM_THREADS or
		    number of available processors
    cannot use the internal var because the RTL may not yet been initialized!!
  2. for a parallel region: return the initialized internal var.
    By Liao. 8/30/2006 bug 157
  */
        char *env_var_str;
        int  env_var_val;

  if (__omp_rtl_initialized == 1)
    return __omp_nthreads_var;
  else {
       env_var_str = getenv("OMP_NUM_THREADS");
       if (env_var_str != NULL)
          {
              sscanf(env_var_str, "%d", &env_var_val);
            Is_Valid(env_var_val > 0, ("OMP_NUM_THREAD should > 0"));
            if (env_var_val > __omp_max_num_threads)
                env_var_val = __omp_max_num_threads;
            return env_var_val;
           } 
       else
          return Get_SMP_CPU_num();
  }
}

inline int __ompc_get_num_procs(void)
{
  if (__omp_rtl_initialized == 1)
    return __omp_num_processors;
  else
    return Get_SMP_CPU_num();
}

/* The caller must ensure the validity of __num_threads*/
inline void __ompc_set_num_threads(const int __num_threads)
{
  /* The logic is perhaps wrong*/
  Is_Valid( __num_threads > 0,
	    (" number of threads must be possitive!"));
  if (__num_threads > __omp_max_num_threads) {
    Warning(" Exceed the threads number limit.");
  }
  /* Not try to modify the setting, adjust it at fork*/
  __omp_nthreads_var = __num_threads;
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
    return __ompc_get_current_v_thread();
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
/* Should not be called directly, use __ompc_barrier instead*/
inline void __ompc_barrier_wait(omp_team_t *team)
{
  /*Warning: This implementation may cause cache problems*/
  int barrier_flag;
  int reset_barrier = 0;
  int new_count;
  int i, j;

/*
    omp_task_t *next;

    __ompc_atomic_dec(&__omp_level_1_team_manager.num_tasks);

    while(__omp_level_1_team_manager.num_tasks != 0)
    {
	__ompc_task_schedule(&next);
	if(next != NULL) {
	  __ompc_task_switch(__omp_level_1_team_tasks[__omp_myid], next);
	}

    }

*/

  barrier_flag = team->barrier_flag;
  new_count = __ompc_atomic_inc(&team->barrier_count);

  if (new_count == team->team_size) {
    /* The last one reset flags*/
    team->barrier_count = 0;
    team->barrier_count2 = 1;
    team->barrier_flag = barrier_flag ^ 1; /* Xor: toggle*/
    for (i = 0; i < 300; i++)
      if (team->barrier_count2 == team->team_size) {
	goto barrier_exit;
      }
    pthread_mutex_lock(&(team->barrier_lock));
    pthread_mutex_unlock(&(team->barrier_lock));
    pthread_cond_broadcast(&(team->barrier_cond));
  } else {
    /* Wait for the last to reset te barrier*/
    /* We must make sure that every waiting thread get this
     * signal */
    for (i = 0; i < 5000; i++)
      if (team->barrier_flag != barrier_flag) {
	__ompc_atomic_inc(&team->barrier_count2);
	goto barrier_exit;
      }
    pthread_mutex_lock(&(team->barrier_lock));
    while (team->barrier_flag == barrier_flag)
      pthread_cond_wait(&(team->barrier_cond), &(team->barrier_lock));
    pthread_mutex_unlock(&(team->barrier_lock));
  }

 barrier_exit:
  __ompc_atomic_inc(&__omp_level_1_team_manager.num_tasks);

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
    //		if (__omp_level_1_team_size != 1)
    //		{
    __ompc_barrier_wait(&__omp_level_1_team_manager);
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

inline void __ompc_ebarrier(void)
{
  omp_v_thread_t *temp_v_thread;
   omp_v_thread_t *p_vthread = __ompc_get_v_thread_by_num( __omp_myid);
    p_vthread->thr_ebar_state_id++;
  __ompc_set_state(THR_EBAR_STATE);
  __ompc_event_callback(OMP_EVENT_THR_BEGIN_EBAR);
  if (__omp_exe_mode & OMP_EXE_MODE_NORMAL) {
    //          if (__omp_level_1_team_size != 1)
    //          {
    __ompc_barrier_wait(&__omp_level_1_team_manager);
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
    __ompc_event_callback(OMP_EVENT_THR_END_EBAR);
     __ompc_set_state(THR_WORK_STATE);
    return;
    }
  else {
    __ompc_barrier_wait(temp_v_thread->team);
  }
   __ompc_set_state(THR_WORK_STATE);
  __ompc_event_callback(OMP_EVENT_THR_END_EBAR);
}

/* Check the _num_threads against __omp_max_num_threads*/
int
__ompc_check_num_threads(const int _num_threads)
{
  int num_threads = _num_threads;
  int request_threads;
  /* How about _num_threads == 1*/
  Is_Valid( num_threads > 0,
	    (" number of threads must be possitive!"));
  if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
    /* request for level 1 threads*/
    request_threads = num_threads - __omp_level_1_team_alloc_size;
    if (request_threads <= __omp_max_num_threads) {
      /* OK. we can fulfill your request*/
    } else {
      /* Exceed current limitat*/
      Warning(" Exceed the thread number limit: Reduce to Max");
      num_threads = __omp_level_1_team_alloc_size + __omp_max_num_threads;
    }
  } else {/* Request for nest team*/
    if ((num_threads - 1) > __omp_max_num_threads) {
      /* Exceed current limit*/
      /* The master is already there, need not to be allocated*/
      num_threads = __omp_max_num_threads + 1; 
      Warning(" Exceed the thread number limit: Reduce to Max");
    } else {
      /* OK. we can fulfill your request*/
    }
  }
  return num_threads;
}

/* Exposed API should be moved to somewhere else, instead of been inlined*/
/* flush needs to do nothing on IA64 based platforms?*/
inline void __ompc_flush(void *p)
{

}

/* stuff function. Required by legacy code for Guide*/
inline int __ompc_can_fork(void)
{
  /* always return true currently*/
  return 1;
}

inline void __ompc_begin(void)
{
  /* do nothing */
}

inline void __ompc_end(void)
{
  /* do nothing */
}


#endif /* __omp_rtl_thread_included */
