/*
 * Copyright (C) 2009 Advanced Micro Devices, Inc.  All Rights Reserved.
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

/* Copyright (C) 2006-2011 University of Houston. */


/*
 * File: omp_thread.c
 * Abstract: routines for thread management
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 *          06/09/2005, updated by Liao Chunhua, Univ. of Houston
 *          06/20/2007, updated by He Jiangzhou, Tsinghua Univ.
 * 
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <malloc.h>
#include "omp_thread.h"
#include "omp_sys.h"
#include "omp_xbarrier.h"

#define SPIN_COUNT_DEFAULT 20000
#define WAIT_TIME_DEFAULT 5000
#include "pcl.h"
#include "omp_collector_util.h"
#include "omp_collector_validation.h"
/*To align with the Pathscale OMP lowering, CWG */

unsigned long current_region_id = 0;
unsigned long current_parent_id = 0;


#define debug 0
__thread int __omp_myid;
__thread int __omp_seed;
volatile int __omp_verbose = 0;
volatile int __omp_nested = OMP_NESTED_DEFAULT;          /* nested enable/disable */
volatile int __omp_dynamic = OMP_DYNAMIC_DEFAULT;         /* dynamic enable/disable */
/* max num of thread available*/
volatile int __omp_max_num_threads = OMP_MAX_NUM_THREADS - 1;
/* stores the number of threads requested for future parallel regions. */
volatile int __omp_nthreads_var;

/* num of hardware processors */
int __omp_num_hardware_processors;

/* num of available processors */
int __omp_num_processors;

/* num of cores that used in affinity setting*/
int __omp_core_list_size;

/* list of the available processors */
int * __omp_list_processors;

/* default schedule type and chunk size of runtime schedule*/
omp_sched_t  __omp_rt_sched_type = OMP_SCHED_DEFAULT;
int  __omp_rt_sched_size = OMP_CHUNK_SIZE_DEFAULT;

volatile unsigned long int __omp_stack_size = OMP_STACK_SIZE_DEFAULT;
volatile unsigned long int __omp_task_stack_size = OMP_TASK_STACK_SIZE_DEFAULT;

__thread omp_exe_mode_t __omp_exe_mode = OMP_EXE_MODE_DEFAULT;

volatile int __omp_empty_flags[OMP_MAX_NUM_THREADS];

omp_v_thread_t * __omp_level_1_team = NULL;
omp_u_thread_t * __omp_level_1_pthread = NULL;
int		 __omp_level_1_team_size = 1;
int		 __omp_level_1_team_alloc_size = 1;
volatile omp_team_t	 __omp_level_1_team_manager;
// omp_team_t       temp_team;

omp_u_thread_t * __omp_uthread_hash_table[UTHREAD_HASH_SIZE];

__thread omp_v_thread_t *__omp_current_v_thread;

/* use once for pthread creation */
volatile int __attribute__ ((__aligned__(CACHE_LINE_SIZE)))__omp_level_1_pthread_count = 1;
/* use for level_1 team end microtask synchronization */
volatile int __attribute__ ((__aligned__(CACHE_LINE_SIZE)))__omp_level_1_exit_count = 0;

int __attribute__ ((__aligned__(CACHE_LINE_SIZE)))__omp_spin_user_lock = 0;

omp_team_t       __omp_root_team;
omp_u_thread_t * __omp_root_u_thread;
omp_v_thread_t	 __omp_root_v_thread={0,THR_SERIAL_STATE,0,NULL,NULL,0,NULL,0,0,0,0,0,0,0,0,0,0,0};

pthread_t	     __omp_root_thread_id = -1;

int              __omp_rtl_initialized = 0;

// control variable for spin lock, it can be set by O64_OMP_SPIN_COUNT
long int __omp_spin_count = SPIN_COUNT_DEFAULT;

long int __omp_wait_time = WAIT_TIME_DEFAULT;

// control variable for whether binding thread to cpu
// it can be reset by O64_OMP_SET_AFFINITY
int             __omp_set_affinity = 1;

unsigned int numtasks = 0;
//static volatile int __omp_global_team_count = 0;
//static volatile int __omp_nested_team_count = 0;

static pthread_attr_t	__omp_pthread_attr;
//static pthread_barrierattr_t __omp_pthread_barrierattr;
//static volatile int  __omp_exit_now = 0;

static pthread_mutex_t __omp_level_1_mutex;
static pthread_cond_t __omp_level_1_cond;

static pthread_mutex_t __omp_level_1_barrier_mutex;
static pthread_cond_t __omp_level_1_barrier_cond;

pthread_mutex_t __omp_hash_table_lock;
int ompc_req_start = 0;

/* maybe a separate attribute should be here for nested pthreads */

/* sysnem lock variables */

/* prototype */
void __ompc_environment_variables();
void __ompc_print_environment();
void __ompc_task_configure();
void __ompc_level_1_barrier(const int vthread_id);
void __ompc_exit_barrier(omp_v_thread_t *_v_thread);
void __ompc_fini_rtl();
int  __ompc_init_rtl(int num_threads);
void __ompc_expand_level_1_team(int new_num_threads);
void *__ompc_level_1_slave(void *_u_thread_id);
void *__ompc_nested_slave(void *_v_thread); 


void __ompc_set_state(OMP_COLLECTOR_API_THR_STATE state)
{
  omp_v_thread_t *p_vthread = __ompc_get_v_thread_by_num( __omp_myid);
  p_vthread->state = state;

  // omp_v_thread_t *p_vthread =  __ompc_get_current_v_thread();
  //  p_vthread->state = state;
  //  if(debug) printf("Thread %d STATE=%d\n",p_vthread->vthread_id,(int) state);
 
}
void __ompc_event_callback(OMP_COLLECTORAPI_EVENT event)
{
  //  omp_v_thread_t *p_vthread =  __ompc_get_current_v_thread();
  //  if(debug) 
  //      printf("Thread %d EVENT=%d STATE=%d\n",p_vthread->vthread_id,
  //                              (int) event, (int) p_vthread->state);

  if( __omp_level_1_team_manager.callbacks[event] && 
                  collector_initialized && (!collector_paused))
    __omp_level_1_team_manager.callbacks[event](event);
}

void 
__ompc_environment_variables()
{
  char *env_var_str;
  int  env_var_val;

  env_var_str = getenv("O64_OMP_VERBOSE");
  if (env_var_str != NULL) {
    env_var_val = strncasecmp(env_var_str, "true", 4);

    if (env_var_val == 0) {
      __omp_verbose = 1;
    } else {
      env_var_val = strncasecmp(env_var_str, "false", 4);
      if (env_var_val == 0)  {
        __omp_verbose = 0;
      } else {
	Not_Valid("O64_OMP_VERBOSE should be set to: true/false");
      }
    }
  }
	
  env_var_str = getenv("OMP_NUM_THREADS");
  if (env_var_str != NULL) {
    sscanf(env_var_str, "%d", &env_var_val);
    Is_Valid(env_var_val > 0, ("OMP_NUM_THREADS should be positive")); 
    if (env_var_val > __omp_max_num_threads)
      env_var_val = __omp_max_num_threads;
    __omp_nthreads_var = env_var_val;
  }


  env_var_str = getenv("OMP_DYNAMIC");
  if (env_var_str != NULL) {
    env_var_val = strncasecmp(env_var_str, "true", 4);
			      
    if (env_var_val == 0) {	
      __omp_dynamic = 1; 
    } else {
      env_var_val = strncasecmp(env_var_str, "false", 4);
      if (env_var_val == 0) {
	__omp_dynamic = 0;
      } else {
	Not_Valid("OMP_DYNAMIC should be set to: true/false");
      }
    }
  }

  env_var_str = getenv("OMP_NESTED");
  if (env_var_str != NULL) {
    env_var_val = strncasecmp(env_var_str, "true", 4);
			      
    if (env_var_val == 0) {	
      __omp_nested = 1; 
    } else {
      env_var_val = strncasecmp(env_var_str, "false", 4);
      if (env_var_val == 0)  {
	__omp_nested = 0;
      } else {
	Not_Valid("OMP_NESTED should be set to: true/false");
      }
    }
  }
	
  env_var_str = getenv("OMP_SCHEDULE");
  if (env_var_str != NULL) {
    env_var_str = Trim_Leading_Spaces(env_var_str);
    if (strncasecmp(env_var_str, "static", 6) == 0) {
      env_var_str += 6;
      __omp_rt_sched_type = OMP_SCHED_STATIC;
    } else if (strncasecmp(env_var_str,"dynamic",7) == 0) {
      env_var_str += 7;
      __omp_rt_sched_type = OMP_SCHED_DYNAMIC;
    } else if (strncasecmp(env_var_str,"guided",6) == 0) {
      env_var_str += 6;
      __omp_rt_sched_type = OMP_SCHED_GUIDED;
    } else {
      Not_Valid("using: OMP_SCHEDULE=\"_schedule_[, _chunk_]\"");
    }

    env_var_str = Trim_Leading_Spaces(env_var_str);
		
    if (*env_var_str != '\0') {
      Is_Valid(*env_var_str == ',', 
	       ("a ',' is expected before the chunksize"));
      env_var_str = Trim_Leading_Spaces(++env_var_str);
      Is_Valid(isdigit((int)(*env_var_str)),
	       ("positive number expected for chunksize"));
      sscanf(env_var_str, "%d", &env_var_val);
      Is_Valid(env_var_val > 0, 
	       ("positive number expected for chunksize"));
      __omp_rt_sched_size = env_var_val;
    } else { /* no chunk size specified */
      if(__omp_rt_sched_type == OMP_SCHED_STATIC)
	__omp_rt_sched_type = OMP_SCHED_STATIC_EVEN;
    }

  }

  /* determine the stack size of slave threads. */
  env_var_str = getenv("OMP_STACKSIZE");
  if (env_var_str != NULL) {
    char stack_size_unit;
    unsigned long int stack_size;
    sscanf(env_var_str, "%ld%c", &stack_size, &stack_size_unit);
    Is_Valid( stack_size > 0, ("stack size must be positive"));
    switch (stack_size_unit) {
    case 'k':
    case 'K':
      stack_size *= 1024;
      break;
    case 'm':
    case 'M':
      stack_size *= 1024 * 1024;
      break;
    case 'g':
    case 'G':
      stack_size *= 1024 * 1024 * 1024;
      break;
    default:
      Not_Valid("Using _stacksize_[kKmMgG]");
      break;
    }
    if (stack_size < OMP_STACK_SIZE_DEFAULT)
      {
	Warning("specified a slave stack size less than 4MB, using default.");
      }
    /* maybe we also need to check the upper limit? */
    Is_Valid( stack_size < Get_System_Stack_Limit(),
	      ("beyond current user stack limit, try using ulimit"));
    __omp_stack_size = stack_size;
  }

  env_var_str = getenv("O64_OMP_WAIT_TIME");
  if (env_var_str != NULL) {
    long int wait_time;
    sscanf(env_var_str, "%ld", &wait_time);
    Is_Valid(wait_time > 0, ("wait time must be positive"));
    __omp_wait_time = wait_time;
  }

  env_var_str = getenv("O64_OMP_SPIN_COUNT");
  if (env_var_str != NULL) {
    long int spin_count;
    sscanf(env_var_str, "%ld", &spin_count);
    Is_Valid(spin_count > 0, ("spin count must be positive"));
    __omp_spin_count = spin_count;
  }

  env_var_str = getenv("O64_OMP_SPIN_USER_LOCK");
  if (env_var_str != NULL) {
    env_var_val = strncasecmp(env_var_str, "true", 4);

    if (env_var_val == 0) {
      __omp_spin_user_lock = 1;
    } else {
      env_var_val = strncasecmp(env_var_str, "false", 4);
      if (env_var_val == 0) {
        __omp_spin_user_lock = 0;
      } else {
        Not_Valid("O64_OMP_SPIN_USER_LOCK should be set to: true/false");
      }
    }
  }
 
  env_var_str = getenv("O64_OMP_SET_AFFINITY");
  if (env_var_str != NULL) {
    env_var_val = strncasecmp(env_var_str, "true", 4);

    if (env_var_val == 0) {
      __omp_set_affinity = 1;
    } else {
      env_var_val = strncasecmp(env_var_str, "false", 4);
      if (env_var_val == 0) {
        __omp_set_affinity = 0;
      } else {
        Not_Valid("O64_OMP_SET_AFFINITY should be set to: true/false");
      }
    }
  }

  env_var_str = getenv("O64_OMP_XBARRIER_TYPE");
  if (env_var_str != NULL) {
    if (strncasecmp(env_var_str, "dissem", 6) == 0) {
      __omp_xbarrier_type = DISSEM_XBARRIER;
      Warning("O64_OMP_XBARRIER_TYPE=dissem will not "
              "work correctly with OpenMP 3.0 tasking model.");
    } else if (strncasecmp(env_var_str, "tree", 4) == 0) {
      __omp_xbarrier_type = TREE_XBARRIER;
      Warning("O64_OMP_XBARRIER_TYPE=tree will not "
              "work correctly with OpenMP 3.0 tasking model.");
    } else if (strncasecmp(env_var_str, "tour", 4) == 0) {
      __omp_xbarrier_type = TOUR_XBARRIER;
      Warning("O64_OMP_XBARRIER_TYPE=tour will not "
              "work correctly with OpenMP 3.0 tasking model.");
    } else if (strncasecmp(env_var_str, "simple", 6) == 0) {
      __omp_xbarrier_type = SIMPLE_XBARRIER;
      Warning("O64_OMP_XBARRIER_TYPE=simple will not "
              "work correctly with OpenMP 3.0 tasking model.");
    } else if (strncasecmp(env_var_str, "linear", 6) == 0) {
      __omp_xbarrier_type = LINEAR_XBARRIER;
    } else  {
        Not_Valid("O64_OMP_XBARRIER_TYPE should be "
                  "dissem|tree|tour|simple|linear or unset");
    }
  } else {
    __omp_xbarrier_type = LINEAR_XBARRIER;
  }
  __ompc_set_xbarrier_wait();

  __ompc_task_configure();

  if (__omp_verbose == 1) __ompc_print_environment();
}

static void __ompc_print_env_tag(char *env_name)
{
  int i,j;
  const int max_len = 36;
  char env_name_tag[max_len];
  snprintf(env_name_tag, max_len, "[%s]", env_name);
  j = strlen(env_name_tag);
  for (i = 0; i < (max_len-j); i++) strcat(env_name_tag, " ");
  fprintf(stderr, "%s", env_name_tag);
}

void __ompc_print_environment()
{
  /* OMP_NUM_THREADS */
  __ompc_print_env_tag("OMP_NUM_THREADS");
  fprintf(stderr, "__omp_nthreads_var = %d\n",
          __omp_nthreads_var);
  /* OMP_DYNAMIC */
  __ompc_print_env_tag("OMP_DYNAMIC");
  fprintf(stderr, "__omp_dynamic = %d\n",
          __omp_dynamic);
  /* OMP_NESTED */
  __ompc_print_env_tag("OMP_NESTED");
  fprintf(stderr, "__omp_nested = %d\n",
          __omp_nested);
  /* OMP_SCHEDULE */
  char chunk_size_str[10];
  sprintf(chunk_size_str, ",%d", __omp_rt_sched_size);
  __ompc_print_env_tag("OMP_SCHEDULE");
  fprintf(stderr, "__omp_rt_sched_type = %s%s\n",
          __omp_rt_sched_type == OMP_SCHED_STATIC ? "static" :
          __omp_rt_sched_type == OMP_SCHED_DYNAMIC ? "dynamic" :
          __omp_rt_sched_type == OMP_SCHED_GUIDED ? "guided" :
          __omp_rt_sched_type == OMP_SCHED_STATIC ? "static" :
          __omp_rt_sched_type == OMP_SCHED_STATIC_EVEN ? "static_even" : "unknown",
          __omp_rt_sched_type == OMP_SCHED_STATIC_EVEN ? "" : chunk_size_str);
  /* OMP_STACKSIZE */
  __ompc_print_env_tag("OMP_STACKSIZE");
  fprintf(stderr, "__omp_stack_size = %ld\n",
          __omp_stack_size);
  /* O64_OMP_WAIT_TIME */
  __ompc_print_env_tag("O64_OMP_WAIT_TIME");
  fprintf(stderr, "__omp_wait_time = %ld\n",
          __omp_wait_time);
  /* O64_OMP_SPIN_COUNT */
  __ompc_print_env_tag("O64_OMP_SPIN_COUNT");
  fprintf(stderr, "__omp_spin_count = %ld\n",
          __omp_spin_count);
  /* O64_OMP_SPIN_USER_LOCK */
  __ompc_print_env_tag("O64_OMP_SPIN_USER_LOCK");
  fprintf(stderr, "__omp_spin_user_lock = %d\n",
          __omp_spin_user_lock);
  /* O64_OMP_SET_AFFINITY */
  __ompc_print_env_tag("O64_OMP_SET_AFFINITY");
  fprintf(stderr, "__omp_set_affinity = %d\n",
          __omp_set_affinity);
  /* O64_OMP_XBARRIER_TYPE */
  __ompc_print_env_tag("O64_OMP_XBARRIER_TYPE");
  fprintf(stderr, "__omp_xbarrier_type = %s\n",
          __omp_xbarrier_type == DISSEM_XBARRIER ? "dissem" :
          __omp_xbarrier_type == TREE_XBARRIER ? "tree" :
          __omp_xbarrier_type == TOUR_XBARRIER ? "tour" :
          __omp_xbarrier_type == SIMPLE_XBARRIER ? "simple" :
          __omp_xbarrier_type == LINEAR_XBARRIER ? "linear" : "unknown");
  /* O64_OMP_QUEUE_STORAGE */
  __ompc_print_env_tag("O64_OMP_QUEUE_STORAGE");
  fprintf(stderr, "__omp_queue_storage = %s\n",
          __omp_queue_storage == LIST_QUEUE_STORAGE ? "list" :
          __omp_queue_storage == DYN_ARRAY_QUEUE_STORAGE ? "dyn_array" :
          __omp_queue_storage == ARRAY_QUEUE_STORAGE ? "array" :
          __omp_queue_storage == LOCKLESS_QUEUE_STORAGE ? "lockless" : "unknown");
  /* O64_OMP_TASK_QUEUE */
  __ompc_print_env_tag("O64_OMP_TASK_QUEUE");
  fprintf(stderr, "__omp_task_queue = %s\n",
          __omp_task_queue);
  /* O64_OMP_TASK_QUEUE_NUM_SLOTS */
  __ompc_print_env_tag("O64_OMP_TASK_QUEUE_NUM_SLOTS");
  fprintf(stderr, "__omp_task_queue_num_slots = %d\n",
          __omp_task_queue_num_slots);
  /* O64_OMP_TASK_CHUNK_SIZE */
  __ompc_print_env_tag("O64_OMP_TASK_CHUNK_SIZE");
  fprintf(stderr, "__omp_task_chunk_size = %d\n",
          __omp_task_chunk_size);
  /* O64_OMP_TASK_POOL_GREEDVAL */
  __ompc_print_env_tag("O64_OMP_TASK_POOL_GREEDVAL");
  fprintf(stderr, "__omp_task_pool_greedval = %d\n",
          __omp_task_pool_greedval);
  /* O64_OMP_TASK_POOL */
  __ompc_print_env_tag("O64_OMP_TASK_POOL");
  fprintf(stderr, "__omp_task_pool = %s\n",
          __omp_task_pool);
  /* O64_OMP_TASK_CUTOFF */
  char cutoff_settings[128];
  sprintf(cutoff_settings, "num_threads:%d,switch:%d,depth:%d,num_children:%d",
          __omp_task_cutoff_num_threads_min,
          __omp_task_cutoff_switch_max,
          __omp_task_cutoff_depth_max,
          __omp_task_cutoff_num_children_max);
  __ompc_print_env_tag("O64_OMP_TASK_CUTOFF");
  fprintf(stderr, "__omp_task_cutoff = %s\n",
          __ompc_task_cutoff == __ompc_task_cutoff_always ? "always" :
          __ompc_task_cutoff == __ompc_task_cutoff_never ? "never" :
          cutoff_settings);
}

/* configuration for OpenMP Tasking implementation */
void
__ompc_task_configure()
{
  char *env_var_str;
  int  env_var_val;
  char *queue_storage_str, *task_queue_str;
  int using_queue_list = 0;
  int using_queue_lockless = 0;

  /* not currently used */
  /*
  env_var_str = getenv("OMP_TASK_STACK_SIZE");
  if (env_var_str != NULL) {
    char stack_size_unit;
    unsigned long int stack_size;
    sscanf(env_var_str, "%ld%c", &stack_size, &stack_size_unit);
    Is_Valid( stack_size > 0, ("stack size must be positive"));
    switch (stack_size_unit) {
    case 'k':
    case 'K':
      stack_size *= 1024;
      break;
    case 'm':
    case 'M':
      stack_size *= 1024 * 1024;
      break;
    case 'g':
    case 'G':
      stack_size *= 1024 * 1024 * 1024;
      break;
    case 'b':
    case 'B':
      break;
    default:
      Not_Valid("Using _stacksize_[kKmMgG]");
      break;
    }
    __omp_task_stack_size = stack_size;
  }
  */

  /* control the implementation of the queue storage */
  queue_storage_str = getenv("O64_OMP_QUEUE_STORAGE");
  if (queue_storage_str != NULL) {
    if (strncasecmp(queue_storage_str, "LIST", 4) == 0) {
      __omp_queue_storage = LIST_QUEUE_STORAGE;
      __ompc_queue_is_empty = &__ompc_queue_check_is_empty;
      __ompc_queue_init = &__ompc_queue_list_init;
      __ompc_queue_free_slots = &__ompc_queue_list_free_slots;
      __ompc_queue_is_full = &__ompc_queue_list_is_full;
      __ompc_queue_num_used_slots = &__ompc_queue_list_num_used_slots;
      __ompc_queue_steal_head = &__ompc_queue_list_steal_head;
      __ompc_queue_steal_tail= &__ompc_queue_list_steal_tail;
      __ompc_queue_get_head = &__ompc_queue_list_get_head;
      __ompc_queue_get_tail= &__ompc_queue_list_get_tail;
      __ompc_queue_put_tail = &__ompc_queue_list_put_tail;
      __ompc_queue_put_head = &__ompc_queue_list_put_head;
      /* not yet supported for LIST */
      __ompc_queue_transfer_chunk_from_head = NULL;

      /* CFIFO not yet supported for LIST, so using the standard LIST
       * implementation */
      __ompc_queue_cfifo_is_full = &__ompc_queue_list_is_full;
      __ompc_queue_cfifo_num_used_slots =
        &__ompc_queue_list_num_used_slots;
      __ompc_queue_cfifo_put = &__ompc_queue_list_put_tail;
      __ompc_queue_cfifo_get = &__ompc_queue_list_get_head;
      /* not yet supported for LIST */
      __ompc_queue_cfifo_transfer_chunk = NULL;

      using_queue_list = 1;

    } else if (strncasecmp(queue_storage_str, "DYN_ARRAY", 9) == 0) {
      /* mostly the same as ARRAY implementation, except for functions that
       * add new items to the queue */
      __omp_queue_storage = DYN_ARRAY_QUEUE_STORAGE;
      __ompc_queue_is_empty = &__ompc_queue_check_is_empty;
      __ompc_queue_init = &__ompc_queue_array_init;
      __ompc_queue_free_slots = &__ompc_queue_array_free_slots;
      __ompc_queue_is_full = &__ompc_queue_array_is_full;
      __ompc_queue_num_used_slots = &__ompc_queue_array_num_used_slots;
      __ompc_queue_steal_head = &__ompc_queue_array_steal_head;
      __ompc_queue_steal_tail= &__ompc_queue_array_steal_tail;
      __ompc_queue_get_head = &__ompc_queue_array_get_head;
      __ompc_queue_get_tail= &__ompc_queue_array_get_tail;
      __ompc_queue_put_tail = &__ompc_queue_dyn_array_put_tail;
      __ompc_queue_put_head = &__ompc_queue_dyn_array_put_head;
      __ompc_queue_transfer_chunk_from_head =
                 &__ompc_queue_array_transfer_chunk_from_head_to_empty;
      __ompc_queue_cfifo_is_full = &__ompc_queue_cfifo_array_is_full;
      __ompc_queue_cfifo_num_used_slots =
        &__ompc_queue_cfifo_array_num_used_slots;
      __ompc_queue_cfifo_put = &__ompc_queue_cfifo_dyn_array_put;
      __ompc_queue_cfifo_get = &__ompc_queue_cfifo_array_get;
      __ompc_queue_cfifo_transfer_chunk =
                 &__ompc_queue_cfifo_array_transfer_chunk_to_empty;
    } else if (strncasecmp(queue_storage_str, "ARRAY", 5) == 0) {
      __omp_queue_storage = ARRAY_QUEUE_STORAGE;
      __ompc_queue_is_empty = &__ompc_queue_check_is_empty;
      __ompc_queue_init = &__ompc_queue_array_init;
      __ompc_queue_free_slots = &__ompc_queue_array_free_slots;
      __ompc_queue_is_full = &__ompc_queue_array_is_full;
      __ompc_queue_num_used_slots = &__ompc_queue_array_num_used_slots;
      __ompc_queue_steal_head = &__ompc_queue_array_steal_head;
      __ompc_queue_steal_tail= &__ompc_queue_array_steal_tail;
      __ompc_queue_get_head = &__ompc_queue_array_get_head;
      __ompc_queue_get_tail= &__ompc_queue_array_get_tail;
      __ompc_queue_put_tail = &__ompc_queue_array_put_tail;
      __ompc_queue_put_head = &__ompc_queue_array_put_head;
      __ompc_queue_transfer_chunk_from_head =
                 &__ompc_queue_array_transfer_chunk_from_head_to_empty;
      __ompc_queue_cfifo_is_full = &__ompc_queue_cfifo_array_is_full;
      __ompc_queue_cfifo_num_used_slots =
        &__ompc_queue_cfifo_array_num_used_slots;
      __ompc_queue_cfifo_put = &__ompc_queue_cfifo_array_put;
      __ompc_queue_cfifo_get = &__ompc_queue_cfifo_array_get;
      __ompc_queue_cfifo_transfer_chunk =
                 &__ompc_queue_cfifo_array_transfer_chunk_to_empty;
    } else if (strncasecmp(queue_storage_str, "LOCKLESS", 8) == 0) {
      __omp_queue_storage = LOCKLESS_QUEUE_STORAGE;
      using_queue_lockless = 1;
      __ompc_queue_is_empty = &__ompc_queue_lockless_is_empty;
      __ompc_queue_init = &__ompc_queue_lockless_init;
      __ompc_queue_free_slots = &__ompc_queue_lockless_free_slots;
      __ompc_queue_is_full = &__ompc_queue_lockless_is_full;
      __ompc_queue_num_used_slots = &__ompc_queue_lockless_num_used_slots;
      __ompc_queue_steal_head = &__ompc_queue_lockless_get_head;
      __ompc_queue_steal_tail= NULL;
      __ompc_queue_get_head = &__ompc_queue_lockless_get_head;
      __ompc_queue_get_tail= &__ompc_queue_lockless_get_tail;
      __ompc_queue_put_tail = &__ompc_queue_lockless_put_tail;
      __ompc_queue_put_head = NULL;
      __ompc_queue_transfer_chunk_from_head = NULL;
      __ompc_queue_cfifo_is_full = &__ompc_queue_lockless_is_full;
      __ompc_queue_cfifo_num_used_slots = &__ompc_queue_lockless_num_used_slots;
      __ompc_queue_cfifo_put = &__ompc_queue_lockless_put_tail;
      __ompc_queue_cfifo_get = &__ompc_queue_lockless_get_head;
      __ompc_queue_cfifo_transfer_chunk = NULL;
    }  else  {
      Not_Valid("O64_OMP_QUEUE_STORAGE should be "
                "ARRAY|LIST|DYN_ARRAY or unset");
    }
  } else {
    /* ARRAY */
      __omp_queue_storage = ARRAY_QUEUE_STORAGE;
      __ompc_queue_is_empty = &__ompc_queue_check_is_empty;
    __ompc_queue_init = &__ompc_queue_array_init;
    __ompc_queue_free_slots = &__ompc_queue_array_free_slots;
    __ompc_queue_is_full = &__ompc_queue_array_is_full;
    __ompc_queue_num_used_slots = &__ompc_queue_array_num_used_slots;
    __ompc_queue_steal_head = &__ompc_queue_array_steal_head;
    __ompc_queue_steal_tail= &__ompc_queue_array_steal_tail;
    __ompc_queue_get_head = &__ompc_queue_array_get_head;
    __ompc_queue_get_tail= &__ompc_queue_array_get_tail;
    __ompc_queue_put_tail = &__ompc_queue_array_put_tail;
    __ompc_queue_put_head = &__ompc_queue_array_put_head;
    __ompc_queue_transfer_chunk_from_head =
      &__ompc_queue_array_transfer_chunk_from_head_to_empty;
    __ompc_queue_cfifo_is_full = &__ompc_queue_cfifo_array_is_full;
    __ompc_queue_cfifo_num_used_slots =
      &__ompc_queue_cfifo_array_num_used_slots;
    __ompc_queue_cfifo_put = &__ompc_queue_cfifo_array_put;
    __ompc_queue_cfifo_get = &__ompc_queue_cfifo_array_get;
    __ompc_queue_cfifo_transfer_chunk =
      &__ompc_queue_cfifo_array_transfer_chunk_to_empty;
  }

  /* control the implementation of the task queues */
  task_queue_str = getenv("O64_OMP_TASK_QUEUE");
  __ompc_task_queue_is_full         = __ompc_queue_is_full;
  __ompc_task_queue_num_used_slots  = __ompc_queue_num_used_slots;
  __ompc_task_queue_steal_chunk     = __ompc_queue_transfer_chunk_from_head;
  if (task_queue_str != NULL) {
    if (strncasecmp(task_queue_str, "CFIFO", 5) == 0) {
      if (using_queue_list) {
        Warning("O64_OMP_TASK_QUEUE=CFIFO not supported if "
                "O64_OMP_QUEUE_STORAGE=LIST. Using FIFO instead.");
        /* same as FIFO if queue storage is LIST */
        strcpy(__omp_task_queue, "fifo");
        __ompc_task_queue_get    = __ompc_queue_get_head;
        __ompc_task_queue_put    = __ompc_queue_put_tail;
        __ompc_task_queue_steal  = __ompc_queue_steal_head;
        __ompc_task_queue_donate = __ompc_queue_put_tail;
      }  else {
        strcpy(__omp_task_queue, "cfifo");
        __ompc_task_queue_get    = __ompc_queue_cfifo_get;
        __ompc_task_queue_put    = __ompc_queue_cfifo_put;
        __ompc_task_queue_steal  = __ompc_queue_cfifo_get;
        __ompc_task_queue_donate = __ompc_queue_cfifo_put;
        __ompc_task_queue_is_full = __ompc_queue_cfifo_is_full;
        __ompc_task_queue_num_used_slots = __ompc_queue_cfifo_num_used_slots;
        __ompc_task_queue_steal_chunk = __ompc_queue_cfifo_transfer_chunk;
      }
    } else if (strncasecmp(task_queue_str, "FIFO", 4) == 0) {
      strcpy(__omp_task_queue, "fifo");
      __ompc_task_queue_get    = __ompc_queue_get_head;
      __ompc_task_queue_put    = __ompc_queue_put_tail;
      __ompc_task_queue_steal  = __ompc_queue_steal_head;
      __ompc_task_queue_donate = __ompc_queue_put_tail;
    } else if (strncasecmp(task_queue_str, "LIFO", 4) == 0) {
      if (using_queue_lockless) {
        Not_Valid("O64_OMP_TASK_QUEUE=LIFO not supported if "
            "O64_OMP_QUEUE_STORAGE=LOCKLESS.");
      }
      strcpy(__omp_task_queue, "lifo");
      __ompc_task_queue_get    = __ompc_queue_get_tail;
      __ompc_task_queue_put    = __ompc_queue_put_tail;
      __ompc_task_queue_steal  = __ompc_queue_steal_tail;
      __ompc_task_queue_donate = __ompc_queue_put_tail;
    } else if (strncasecmp(task_queue_str, "INV_DEQUE", 9) == 0) {
      if (using_queue_lockless) {
        Not_Valid("O64_OMP_TASK_QUEUE=INV_DEQUE not supported if "
            "O64_OMP_QUEUE_STORAGE=LOCKLESS.");
      }
      strcpy(__omp_task_queue, "inv_deque");
      __ompc_task_queue_get    = __ompc_queue_get_head;
      __ompc_task_queue_put    = __ompc_queue_put_tail;
      __ompc_task_queue_steal  = __ompc_queue_steal_tail;
      __ompc_task_queue_donate = __ompc_queue_put_head;
    } else if (strncasecmp(task_queue_str, "DEQUE", 5) == 0) {
      strcpy(__omp_task_queue, "deque");
      __ompc_task_queue_get    = __ompc_queue_get_tail;
      __ompc_task_queue_put    = __ompc_queue_put_tail;
      __ompc_task_queue_steal  = __ompc_queue_steal_head;
      __ompc_task_queue_donate = __ompc_queue_put_head;
    } else {
      Not_Valid("O64_OMP_TASK_QUEUE should be "
                "DEQUE|CFIFO|FIFO|LIFO|INV_DEQUE or unset");
    }
  } else {
    /* DEQUE */
    strcpy(__omp_task_queue, "deque");
    __ompc_task_queue_get    = __ompc_queue_get_tail;
    __ompc_task_queue_put    = __ompc_queue_put_tail;
    __ompc_task_queue_steal  = __ompc_queue_steal_head;
    __ompc_task_queue_donate = __ompc_queue_put_head;
  }

  env_var_str = getenv("O64_OMP_TASK_QUEUE_NUM_SLOTS");
  __omp_task_queue_num_slots = TASK_QUEUE_DEFAULT_NUM_SLOTS;
  if (env_var_str != NULL) {
    int num_slots;
    sscanf(env_var_str, "%d", &num_slots);
    if (num_slots < 1) {
      Warning("Value for O64_OMP_TASK_QUEUE_NUM_SLOTS is invalid, "
              " so using default setting");
    } else
      __omp_task_queue_num_slots = num_slots;
  }

  env_var_str = getenv("O64_OMP_TASK_CHUNK_SIZE");
  __omp_task_chunk_size = 1;
  if (env_var_str != NULL) {
    int chunk_size = 1;
    sscanf(env_var_str, "%d", &chunk_size);
    if (chunk_size < 1) {
      Warning("Value for O64_OMP_TASK_CHUNK_SIZE is invalid, "
              " so using default setting");
    } else if ((chunk_size > 1) && using_queue_lockless) {
      Warning("O64_OMP_TASK_CHUNK_SIZE > 1 not allowed if "
              "O64_OMP_QUEUE_STORAGE=LOCKLESS. Setting it to 1.");
      __omp_task_chunk_size = 1;
    } else {
      __omp_task_chunk_size = chunk_size;
    }
  }

  /* value for alternator in O64_OMP_TASK_POOL=PUBLIC_PRIVATE  */
  env_var_str = getenv("O64_OMP_TASK_POOL_GREEDVAL");
  __omp_task_pool_greedval = 4;
  if (env_var_str != NULL) {
    int alt_val;
    sscanf(env_var_str,"%d", &alt_val);
    if (alt_val < 1) {
      Warning("Value for O64_OMP_TASK_POOL_GREEDVAL is invalid, "
          "default setting will be used.");
    } else {
      __omp_task_pool_greedval = alt_val;
    }
  }

  /* control the task pool configuration */
  env_var_str = getenv("O64_OMP_TASK_POOL");
  if (env_var_str != NULL) {
    if (strncasecmp(env_var_str, "PUBLIC_PRIVATE", 14) == 0) {
      /* added 7/28/2011 Jim LaGrone */
      strcpy(__omp_task_pool, "public_private");
      __ompc_create_task_pool    = &__ompc_create_task_pool_public_private;
      __ompc_expand_task_pool    = &__ompc_expand_task_pool_public_private;
      __ompc_add_task_to_pool    = &__ompc_add_task_to_pool_public_private;
      __ompc_remove_task_from_pool = &__ompc_remove_task_from_pool_public_private;
      __ompc_destroy_task_pool   = &__ompc_destroy_task_pool_public_private;
    } else if (strncasecmp(env_var_str, "DEFAULT", 7) == 0) {
      strcpy(__omp_task_pool, "default");
      __ompc_create_task_pool    = &__ompc_create_task_pool_default;
      __ompc_expand_task_pool    = &__ompc_expand_task_pool_default;
      __ompc_add_task_to_pool    = &__ompc_add_task_to_pool_default;
      __ompc_remove_task_from_pool = &__ompc_remove_task_from_pool_default;
      __ompc_destroy_task_pool   = &__ompc_destroy_task_pool_default;
    } else if (strncasecmp(env_var_str, "SIMPLE_2LEVEL", 13) == 0) {
      if (__omp_task_chunk_size > 1) {
        fprintf(stderr, "Warning: task_chunk_size (%d) > 1 with SIMPLE_2LEVEL"
            "task pool may result in incorrect scheduling\n",
            __omp_task_chunk_size);
        fflush(stderr);
      }
      if (task_queue_str != NULL &&
          strncasecmp(task_queue_str, "DEQUE", 5) &&
          strncasecmp(task_queue_str, "LIFO", 4)) {
        fprintf(stderr, "Warning: Value for O64_OMP_TASK_QUEUE (%s) "
            "may result in incorrect scheduling when using SIMPLE_2LEVEL "
            "task pool\n", task_queue_str);
        fflush(stderr);
      }
      if (using_queue_list) {
        Not_Valid("O64_OMP_TASK_POOL=SIMPLE_2LEVEL not supported if "
                  "O64_OMP_QUEUE_STORAGE=LIST");
        /* not reached */
      }
      strcpy(__omp_task_pool, "simple_2level");
      /* each thread gets a queue, plus a global "community" queue */
      __ompc_create_task_pool    = &__ompc_create_task_pool_simple_2level;
      __ompc_expand_task_pool    = &__ompc_expand_task_pool_simple_2level;
      __ompc_add_task_to_pool    = &__ompc_add_task_to_pool_simple_2level;
      __ompc_remove_task_from_pool = &__ompc_remove_task_from_pool_simple_2level;
      __ompc_destroy_task_pool   = &__ompc_destroy_task_pool_simple_2level;
      /* for SIMPLE_2LEVEL, a "donate" is the same as a put */
      __ompc_task_queue_donate = __ompc_task_queue_put;
    } else if  (strncasecmp(env_var_str, "MULTILEVEL", 10) == 0) {
      strcpy(__omp_task_pool, "multilevel");
      Not_Valid("O64_OMP_TASK_POOL does not yet support MULTILEVEL, "
                "try DEFAULT|SIMPLE|SIMPLE_2LEVEL|PUBLIC_PRIVATE instead");
    } else if  (strncasecmp(env_var_str, "2LEVEL", 6) == 0) {
      strcpy(__omp_task_pool, "2level");
      Not_Valid("O64_OMP_TASK_POOL does not yet support 2LEVEL, "
                "try DEFAULT|SIMPLE|SIMPLE_2LEVEL|PUBLIC_PRIVATE instead");
    } else if (strncasecmp(env_var_str, "SIMPLE", 6) == 0) {
      strcpy(__omp_task_pool, "simple");
      if (task_queue_str != NULL &&
          strncasecmp(task_queue_str, "DEQUE", 5) &&
          strncasecmp(task_queue_str, "LIFO", 4)) {
        fprintf(stderr, "Warning: Value for O64_OMP_TASK_QUEUE (%s) "
            "may result in incorrect scheduling when using SIMPLE "
            "task pool\n", task_queue_str);
        fflush(stderr);
      }
      __ompc_create_task_pool    = &__ompc_create_task_pool_simple;
      __ompc_expand_task_pool    = &__ompc_expand_task_pool_simple;
      __ompc_add_task_to_pool    = &__ompc_add_task_to_pool_simple;
      __ompc_remove_task_from_pool = &__ompc_remove_task_from_pool_simple;
      __ompc_destroy_task_pool   = &__ompc_destroy_task_pool_simple;
    } else {
      Not_Valid("O64_OMP_TASK_POOL should be "
                "DEFAULT|SIMPLE|SIMPLE_2LEVEL|PUBLIC_PRIVATE or unset");
    }
  } else {
    /* DEFAULT */
    strcpy(__omp_task_pool, "default");
    __ompc_create_task_pool    = &__ompc_create_task_pool_default;
    __ompc_expand_task_pool    = &__ompc_expand_task_pool_default;
    __ompc_add_task_to_pool    = &__ompc_add_task_to_pool_default;
    __ompc_remove_task_from_pool = &__ompc_remove_task_from_pool_default;
    __ompc_destroy_task_pool   = &__ompc_destroy_task_pool_default;
  }

  /* control the task cutoff */
  __ompc_task_cutoff = &__ompc_task_cutoff_default;
  env_var_str = getenv("O64_OMP_TASK_CUTOFF");
  if (env_var_str != NULL) {
    int i, j;
    char *saveptr1, *saveptr2;
    char *str1, *str2;
    for (i = 0, str1=env_var_str; ; i++, str1=NULL) {
      char *tok, *subtok;
      int val;
      tok = strtok_r(str1, ",", &saveptr1);
      if (tok == NULL)
        break;

      subtok = strtok_r(tok, ":", &saveptr2);
      if (strncasecmp(subtok, "ALWAYS", 6) == 0) {
        subtok = strtok_r(NULL, ":", &saveptr2);
        val = 1;
        if (subtok != NULL) {
          sscanf(subtok, "%d", &val);
        }
        if (val > 0)  {
          __ompc_task_cutoff = &__ompc_task_cutoff_always;
          /* if always encountered and val > 0, then ignore the rest */
          break;
        }
      } else if (strncasecmp(subtok, "NEVER", 5) == 0) {
        subtok = strtok_r(NULL, ":", &saveptr2);
        val = 1;
        if (subtok != NULL) {
          sscanf(subtok, "%d", &val);
        }
        if (val > 0)  {
          __ompc_task_cutoff = &__ompc_task_cutoff_never;
          /* if never encountered and val > 0, then ignore the rest */
          break;
        }
      } else if (strncasecmp(subtok, "NUM_THREADS", 11) == 0) {
        subtok = strtok_r(NULL, ":", &saveptr2);
        val = __omp_task_cutoff_num_threads_min;
        if (subtok != NULL) {
          sscanf(subtok, "%d", &val);
        }
        if (val > 0)  {
          /* enable with new value */
          __omp_task_cutoff_num_threads = 1;
          __omp_task_cutoff_num_threads_min = val;
        } else {
          /* disable */
          __omp_task_cutoff_num_threads = 0;
        }
      } else if (strncasecmp(subtok, "SWITCH", 6) == 0) {
        subtok = strtok_r(NULL, ":", &saveptr2);
        val = __omp_task_cutoff_switch_max;
        if (subtok != NULL) {
          sscanf(subtok, "%d", &val);
        }
        if (val > 0)  {
          /* enable with new value */
          __omp_task_cutoff_switch = 1;
          __omp_task_cutoff_switch_max = val;
        } else {
          /* disable */
          __omp_task_cutoff_switch = 0;
        }
      } else if (strncasecmp(subtok, "DEPTH", 5) == 0) {
        subtok = strtok_r(NULL, ":", &saveptr2);
        val = __omp_task_cutoff_depth_max;
        if (subtok != NULL) {
          sscanf(subtok, "%d", &val);
        }
        if (val > 0)  {
          /* enable with new value */
          __omp_task_cutoff_depth = 1;
          __omp_task_cutoff_depth_max = val;
        } else {
          /* disable */
          __omp_task_cutoff_depth = 0;
        }
      } else if (strncasecmp(subtok, "NUM_CHILDREN", 12) == 0) {
        subtok = strtok_r(NULL, ":", &saveptr2);
        val = __omp_task_cutoff_num_children_max;
        if (subtok != NULL) {
          sscanf(subtok, "%d", &val);
        }
        if (val > 0)  {
          /* enable with new value */
          __omp_task_cutoff_num_children = 1;
          __omp_task_cutoff_num_children_max = val;
        } else {
          /* disable */
          __omp_task_cutoff_num_children = 0;
        }
      } else {
        Not_Valid("O64_OMP_TASK_CUTOFF should be "
            "cutoff:val[,cutoff:val[,...]] where valid cutoffs are "
            "ALWAYS|NEVER|SWITCH|DEPTH|NUM_CHILDREN and val = 0 disables "
            "the cutoff");
      }
    }
  }
}


/* Used for level_1 team end parallel barrier,
 * Not for Normal use. Using __ompc_barrier instead
 * */
void
__ompc_level_1_barrier(const int vthread_id)
{
  int *bar_count;
  long int counter;
  omp_v_thread_t *p_vthread;
  int myrank, team_size;
  long int max_count;
  omp_task_t *next, *current_task;
  omp_task_pool_t *pool;

  max_count = __omp_spin_count;
  p_vthread = __ompc_get_v_thread_by_num(__omp_myid);
  team_size = __omp_level_1_team_size;
  pool = __omp_level_1_team_manager.task_pool;

  current_task = __omp_current_task;
  __ompc_task_set_state(current_task, OMP_TASK_IN_BARRIER);

  p_vthread->thr_ebar_state_id++;
  __ompc_set_state(THR_IBAR_STATE);
  __ompc_event_callback(OMP_EVENT_THR_BEGIN_IBAR);

  bar_count = &__omp_level_1_team_manager.barrier_count;
  __ompc_atomic_inc(bar_count);

  for (counter = 0; (*bar_count < team_size) ||
          __ompc_task_pool_num_pending_tasks(pool); counter++) {

      while (__ompc_task_pool_num_pending_tasks(pool) &&
             (next = __ompc_remove_task_from_pool(pool))) {
          if (next != NULL)
              __ompc_task_switch(next);
      }
      if (counter > max_count) {
          pthread_mutex_lock(&pool->pool_lock);
          if (__ompc_task_pool_num_pending_tasks(pool) == 0 &&
                  *bar_count < team_size) {
              struct timespec ts;
              clock_gettime(CLOCK_REALTIME, &ts);
              ts.tv_nsec += __omp_wait_time;
              pthread_cond_timedwait(&pool->pool_cond,
                                     &pool->pool_lock,
                                     &ts);
          }
          pthread_mutex_unlock(&pool->pool_lock);
      }
  }

  /* delete implicit task and reset task state for level-1 user thread */
  __ompc_task_delete(__omp_level_1_team[vthread_id].implicit_task);
  __omp_level_1_team[vthread_id].implicit_task = NULL;
  __omp_level_1_team[vthread_id].num_suspended_tied_tasks = 0;

  myrank = __ompc_atomic_inc(&__omp_level_1_exit_count);

  if (vthread_id == 0) {
    if (myrank != team_size)
    {
      for( counter = 0; __omp_level_1_exit_count != team_size; counter++) {
        if (counter > max_count) {
          pthread_mutex_lock(&__omp_level_1_barrier_mutex);
          while (__omp_level_1_exit_count != team_size) {
            pthread_cond_wait(&__omp_level_1_barrier_cond, &__omp_level_1_barrier_mutex);
          }
          pthread_mutex_unlock(&__omp_level_1_barrier_mutex);
        }
      }
    }
    __omp_level_1_exit_count = 0;
    *bar_count = 0;
  } else if (myrank == team_size ) {
    // here we do need the mutex lock! otherwise,
    // Otherwise, it's possible that cond_signal may fail to wake up the master thread.
    pthread_mutex_lock(&__omp_level_1_barrier_mutex);
    pthread_cond_signal(&__omp_level_1_barrier_cond);
    pthread_mutex_unlock(&__omp_level_1_barrier_mutex);
  }

  __ompc_event_callback(OMP_EVENT_THR_END_IBAR);
}

/* Used for Nested team end parallel barrier. Since
 * all slaves will exit.
 * */
void
__ompc_exit_barrier(omp_v_thread_t * vthread)
{
  long int counter;
  int *bar_count;
  int *exit_count;
  int myrank;
  long int max_count;
  omp_task_pool_t *pool;
  omp_task_t *next, *current_task;
  int team_size = vthread->team->team_size;

  // Assuming that vthread->team_size != 1
  Is_True((vthread != NULL) && (vthread->team != NULL),
	  ("bad vthread or vthread->team in nested groups"));

  max_count = __omp_spin_count;
  vthread->thr_ibar_state_id++;
  __ompc_set_state(THR_IBAR_STATE);
  __ompc_event_callback(OMP_EVENT_THR_BEGIN_IBAR);

  pool = vthread->team->task_pool;

  current_task = __omp_current_task;
  __ompc_task_set_state(current_task, OMP_TASK_IN_BARRIER);

  bar_count = &vthread->team->barrier_count;
  __ompc_atomic_inc(bar_count);

  for (counter = 0; (*bar_count < team_size) ||
          __ompc_task_pool_num_pending_tasks(pool); counter++) {

      while (__ompc_task_pool_num_pending_tasks(pool) &&
             (next = __ompc_remove_task_from_pool(pool))) {
          if (next != NULL)
              __ompc_task_switch(next);
      }
      if (counter > max_count) {
          pthread_mutex_lock(&pool->pool_lock);
          if (__ompc_task_pool_num_pending_tasks(pool) == 0 &&
                  *bar_count < team_size) {
              struct timespec ts;
              clock_gettime(CLOCK_REALTIME, &ts);
              ts.tv_nsec += __omp_wait_time;
              pthread_cond_timedwait(&pool->pool_cond,
                                     &pool->pool_lock,
                                     &ts);
          }
          pthread_mutex_unlock(&pool->pool_lock);
      }
  }


  __ompc_task_delete(vthread->implicit_task);

  exit_count = &vthread->team->exit_count;
  myrank = __ompc_atomic_inc(exit_count);

  if (__omp_myid == 0) {
    if (myrank != team_size)
    {
      for( counter = 0; *exit_count != team_size; counter++) {
        if (counter > max_count) {
          pthread_mutex_lock(&vthread->team->barrier_lock);
          while (*exit_count != team_size)
            pthread_cond_wait(&vthread->team->barrier_cond,
                              &vthread->team->barrier_lock);
          pthread_mutex_unlock(&vthread->team->barrier_lock);
        }
      }
    }
  } else if (myrank == team_size ) {
    // here we do need the mutex lock! otherwise,
    // Otherwise, it's possible that cond_signal may fail to wake up the master thread.
    pthread_mutex_lock(&vthread->team->barrier_lock);
    pthread_cond_signal(&vthread->team->barrier_cond);
    pthread_mutex_unlock(&vthread->team->barrier_lock);
  }

  __ompc_event_callback(OMP_EVENT_THR_END_IBAR);
}

/* The thread function for level_1 slaves*/
void*
__ompc_level_1_slave(void * _uthread_index)
{
  long uthread_index;
  long int counter;
  int new_tasks = __omp_level_1_team_manager.new_task;
  __omp_seed = uthread_index;
  uthread_index = (long) _uthread_index;
  __omp_myid = uthread_index;

  __ompc_set_state(THR_IDLE_STATE);
  __ompc_event_callback(OMP_EVENT_THR_BEGIN_IDLE);

  __ompc_atomic_inc(&__omp_level_1_pthread_count);

  for(;;) {
    for( counter = 0; __omp_level_1_team_manager.new_task == new_tasks;
         counter++) {
      if (counter > __omp_spin_count) {
        pthread_mutex_lock(&__omp_level_1_mutex);
        while (__omp_level_1_team_manager.new_task == new_tasks) {
          pthread_cond_wait(&__omp_level_1_cond, &__omp_level_1_mutex);
        }
        pthread_mutex_unlock(&__omp_level_1_mutex);
      }
    }

    /* update new_tasks with current count */
    new_tasks = __omp_level_1_team_manager.new_task;

    __omp_exe_mode = OMP_EXE_MODE_NORMAL;

    /* in case level 1 team expanded and the user threads were allocated
     * elsewhere */
    __omp_current_v_thread = &__omp_level_1_team[uthread_index];

    /* The program should exit now? */
    /*		if (__omp_exit_now == true)
		break;
    */

    /* exe micro_task now */
    if ( __omp_level_1_team[uthread_index].entry_func != NULL) {

      /* initialize implicit task for slave thread */
      if (__omp_level_1_team[uthread_index].implicit_task == NULL) {
        __omp_level_1_team[uthread_index].implicit_task =
              __ompc_task_new_implicit();
      }
      __omp_current_task = __omp_level_1_team[uthread_index].implicit_task;

      __ompc_event_callback(OMP_EVENT_THR_END_IDLE);
      __ompc_set_state(THR_WORK_STATE);
      __omp_level_1_team[uthread_index].entry_func(
              uthread_index,
              (void *) __omp_level_1_team[uthread_index].frame_pointer);

      __omp_level_1_team[uthread_index].entry_func = NULL;
      __ompc_level_1_barrier(uthread_index);

      __ompc_set_state(THR_IDLE_STATE);
      __ompc_event_callback(OMP_EVENT_THR_BEGIN_IDLE);
      __omp_exe_mode = OMP_EXE_MODE_SEQUENTIAL;
    }
  }

  return NULL;
}

/* The thread function for nested slaves*/
void*
__ompc_nested_slave(void * _v_thread)
{
  int counter;
  omp_v_thread_t * my_vthread = (omp_v_thread_t *) _v_thread;
  /* need to wait for others ready? */

  __omp_current_v_thread = _v_thread;

  __omp_exe_mode = OMP_EXE_MODE_NESTED;

  __omp_myid = my_vthread->vthread_id;

  /* initialize implicit task for nested slave */
  if (my_vthread->implicit_task == NULL) {
    my_vthread->implicit_task = __ompc_task_new_implicit();
  }

  __omp_current_task = my_vthread->implicit_task;

  __ompc_set_state(THR_IDLE_STATE);
  /* printf("IDLE called from nested\n"); */

  counter = 0;
  for (counter = 0; my_vthread->team->new_task != 1; counter++) {
      if (counter > __omp_spin_count) {

          pthread_mutex_lock(&my_vthread->team->barrier_lock);
          while (my_vthread->team->new_task != 1) {
              pthread_cond_wait(&my_vthread->team->barrier_cond,
                      &my_vthread->team->barrier_lock);
          }
          pthread_mutex_unlock(&my_vthread->team->barrier_lock);
      }
  }

  /* The relationship between vthread, uthread, and team should be OK here*/

  __ompc_event_callback(OMP_EVENT_THR_END_IDLE);
  __ompc_set_state(THR_WORK_STATE);

  my_vthread->entry_func(my_vthread->vthread_id,
      (char *)my_vthread->frame_pointer);

  /*TODO: fix the barrier call for nested threads*/
  __ompc_exit_barrier(my_vthread);

  __omp_exe_mode = OMP_EXE_MODE_NORMAL;

  pthread_exit(NULL);
}

void
__ompc_fini_rtl(void) 
{
  __ompc_destroy_task_pool(__omp_level_1_team_manager.task_pool);
  __ompc_xbarrier_info_destroy(&__omp_level_1_team_manager);

  /* clean up job*/
  if (__omp_level_1_team != NULL)
    aligned_free(__omp_level_1_team);
  if (__omp_level_1_pthread != NULL)
    aligned_free(__omp_level_1_pthread);

  /* Other mutex, conditions, locks , should be destroyed here*/
  if (__omp_list_processors != NULL)
    aligned_free(__omp_list_processors);

}

/* must be called when the first fork()*/
int 
__ompc_init_rtl(int num_threads)
{
  int threads_to_create, log2_threads_to_create;
  int i, j, k, d; 

  int return_value;
  void *stack_pointer;

  Is_True(__omp_rtl_initialized == 0, 
	  (" RTL has been initialized yet!"));


  /* get the number of hardware processors */
  __omp_num_hardware_processors = Get_SMP_CPU_num();

  /* get the number of available processors. It can be smaller than
     __omp_num_hardware_processors, because user can use numactl to 
     control which processors to run. Set default thread number to
     the number of available processors */
  __omp_num_processors = __omp_get_cpu_num();
  __omp_nthreads_var = __omp_num_processors;

#ifndef TARG_LOONGSON
  /* get the list of available processors, later we can bind pthreads
     to the available processors */ 
  __omp_get_available_processors();
#endif //TARG_LOONGSON

  /* parse OpenMP environment variables */
  __ompc_environment_variables();
#ifndef TARG_LOONGSON
  __ompc_sug_numthreads = __omp_nthreads_var;
  __ompc_cur_numthreads = __omp_nthreads_var;
#endif //TARG_LOONGSON

  /* register the finalize function*/
  atexit(__ompc_fini_rtl);

  /* determine number of threads to create*/
  threads_to_create = num_threads == 0 ? __omp_nthreads_var : num_threads;

  log2_threads_to_create = 0;
  for(k = 1; k < threads_to_create; log2_threads_to_create++, k <<= 1);

  /* keep it as nthreads-var suggested in spec. Liao */
  __omp_nthreads_var = threads_to_create;

  /* setup pthread attributes */
  pthread_attr_init(&__omp_pthread_attr);
  pthread_attr_setscope(&__omp_pthread_attr, PTHREAD_SCOPE_SYSTEM);
  /* need to set up barrier attributes */

  /* initial global locks*/
  pthread_mutex_init(&__omp_level_1_mutex, NULL);
  pthread_mutex_init(&__omp_level_1_barrier_mutex, NULL);
  pthread_mutex_init(&__omp_hash_table_lock, NULL);
  pthread_cond_init(&__omp_level_1_cond, NULL);
  pthread_cond_init(&__omp_level_1_barrier_cond, NULL);
  __ompc_init_spinlock(&_ompc_thread_lock);

  __omp_myid = 0;
  __omp_seed = 0;

  /* clean up uthread hash table */
  __ompc_clear_hash_table();


  /* create level 1 team */
  __omp_level_1_pthread = aligned_malloc(sizeof(omp_u_thread_t) * threads_to_create, CACHE_LINE_SIZE);
  Is_True(__omp_level_1_pthread != NULL, 
	  ("Can't allocate __omp_level_1_pthread"));
  memset(__omp_level_1_pthread, 0, sizeof(omp_u_thread_t) * threads_to_create);

  __omp_level_1_team = aligned_malloc(sizeof(omp_v_thread_t) * threads_to_create, CACHE_LINE_SIZE);
  Is_True(__omp_level_1_team != NULL, 
	  ("Can't allocate __omp_level_1_team"));
  memset(__omp_level_1_team, 0, sizeof(omp_v_thread_t) * threads_to_create);

  __omp_level_1_team_manager.team_size = threads_to_create;
  __omp_level_1_team_manager.team_level = 1;
  __omp_level_1_team_manager.is_nested = 0;
  __omp_level_1_team_manager.barrier_count = 0;
  __omp_level_1_team_manager.barrier_count2 = 0;
  __omp_level_1_team_manager.exit_count = 0;
  __omp_level_1_team_manager.barrier_flag = 0;
  __omp_level_1_team_manager.single_count = 0;
  __omp_level_1_team_manager.new_task = 0;
  __omp_level_1_team_manager.loop_count = 0;
  __omp_level_1_team_manager.loop_info_size = 0;
  __omp_level_1_team_manager.loop_info = NULL;

  __omp_level_1_team_manager.log2_team_size = log2_threads_to_create;

  __omp_level_1_team_manager.task_pool = 
    __ompc_create_task_pool(threads_to_create);

  __ompc_xbarrier_info_create(&__omp_level_1_team_manager);

  __ompc_init_spinlock(&(__omp_level_1_team_manager.schedule_lock));
  pthread_cond_init(&(__omp_level_1_team_manager.ordered_cond), NULL);
  __ompc_init_lock(&(__omp_level_1_team_manager.single_lock));
  pthread_mutex_init(&(__omp_level_1_team_manager.barrier_lock), NULL);
  pthread_cond_init(&(__omp_level_1_team_manager.barrier_cond), NULL);
	
  /* setup the level one team data structure */
  for (i=0; i<threads_to_create; i++) {
    __omp_level_1_team[i].team = &__omp_level_1_team_manager;
    __omp_level_1_team[i].team_size = threads_to_create;
    __omp_level_1_team[i].vthread_id = i;
    __omp_level_1_team[i].single_count = 0;
    __omp_level_1_team[i].loop_count = 0;
    __omp_level_1_team[i].state = THR_OVHD_STATE;
    __omp_level_1_team[i].thr_lkwt_state_id=0;     
    __omp_level_1_team[i].thr_ctwt_state_id=0;
    __omp_level_1_team[i].thr_atwt_state_id=0;
    __omp_level_1_team[i].thr_ibar_state_id=0;
    __omp_level_1_team[i].thr_ebar_state_id=0;
    __omp_level_1_team[i].thr_odwt_state_id=0;

    __omp_level_1_team[i].implicit_task = NULL;
    __omp_level_1_team[i].num_suspended_tied_tasks = 0;

    __ompc_init_xbarrier_local_info(&__omp_level_1_team[i].xbarrier_local,
         i, &__omp_level_1_team_manager);

    /* the corresponding relationship is fixed*/
    __omp_level_1_team[i].executor = &(__omp_level_1_pthread[i]);
    __omp_level_1_pthread[i].task = &(__omp_level_1_team[i]);
  }

  /* handle root thread data structure */
  __omp_root_u_thread = __omp_level_1_pthread;

  __omp_root_v_thread.vthread_id = 0;
  __omp_root_v_thread.executor = __omp_root_u_thread;
  __omp_root_v_thread.team_size = 1;
  // The following are set default
  __omp_root_v_thread.team = &__omp_root_team; /* maybe we can set it to a real team */
  // need to initialize __omp_root_team here
  __omp_root_team.team_size = 1;
  __omp_root_team.team_level = 0;
  __omp_root_team.is_nested = 0;
  __omp_root_team.log2_team_size = 1;

  __omp_root_v_thread.entry_func = NULL;
  __omp_root_v_thread.frame_pointer = NULL;

  __omp_root_v_thread.implicit_task = NULL;

  __omp_current_v_thread = &__omp_root_v_thread;

  /* no need to create an implicit task for the root user thread */
  __omp_current_task = NULL;

  __omp_root_thread_id = pthread_self();

  __omp_root_u_thread->uthread_id = __omp_root_thread_id;
  __omp_root_u_thread->hash_next = NULL;
  __ompc_insert_into_hash_table(__omp_root_u_thread);

#ifndef TARG_LOONGSON
  if (__omp_set_affinity) {
    //bind the current thread to the first available cpu 
    __ompc_bind_pthread_to_cpu(__omp_root_thread_id);
  }
#endif //TARG_LOONGSON
  __ompc_event_callback(OMP_EVENT_FORK);

  /* why twice? */
  __ompc_event_callback(OMP_EVENT_FORK);

/*
 * This routine is called by dynamic loader, which is before
 * user-defined main() where various bits in mxcsr are set.
 * This means those bits are not set to the threads
 * created here.
 * Here I only do a paritial fix that sets flush-to-zero
 * bit in X86.
 * ToDo:
 * 1. fix other bits in X86 (like masks controlled by options)
 * 2. fix for other platforms
 * We should have checked if SSE is avaliable as
 * x87 uses a different register.
 */
#if defined(TARG_X8664) || defined(TARG_IA32)

#define MM_FLUSH_ZERO_ON     0x8000
  {
    unsigned int cr;
    __asm__  __volatile__("stmxcsr %0" : "=m" (*&cr));
    cr = cr | MM_FLUSH_ZERO_ON; 
    __asm__  __volatile__("ldmxcsr %0" : : "m" (*&cr));
  }
#endif

  for (i=1; i< threads_to_create; i++) {

#ifndef TARG_LOONGSON
    return_value = pthread_attr_setstacksize(&__omp_pthread_attr, __omp_stack_size);
    Is_True(return_value == 0, ("Cannot set stack size for thread"));
#endif //TARG_LOONGSON
    return_value = pthread_create( &(__omp_level_1_pthread[i].uthread_id),
				   &__omp_pthread_attr, (pthread_entry) __ompc_level_1_slave, 
				   (void *)((unsigned long int)i));
    Is_True(return_value == 0, ("Cannot create more pthreads"));

    __omp_level_1_pthread[i].stack_pointer = (char *)0;

#ifndef TARG_LOONGSON
    if (__omp_set_affinity) {
      // bind pthread to a specific cpu
      __ompc_bind_pthread_to_cpu(__omp_level_1_pthread[i].uthread_id);
    }
#endif //TARG_LOONGSON

    __ompc_insert_into_hash_table(&(__omp_level_1_pthread[i]));

  }

  OMPC_WAIT_WHILE(__omp_level_1_pthread_count != threads_to_create);	

  /* We still should make sure that all the slaves are ready*/
  /* TODO: wait for all slaves*/

  __omp_level_1_team_size = threads_to_create;
  __omp_level_1_team_alloc_size = threads_to_create;
  __omp_max_num_threads -= threads_to_create;

  __omp_collector_init();

  __omp_rtl_initialized = 1;
  return threads_to_create;
}

/* Expand level_1_team to new_num_threads.
   The caller must make sure the validity of new_num_threads. */

void
__ompc_expand_level_1_team(int new_num_threads)
{
  int i;
  int return_value;
  int k, new_log2_num_threads;
  omp_u_thread_t *new_u_team;
  omp_v_thread_t *new_v_team;

  void *stack_pointer;

  new_log2_num_threads = 0;
  for(k = 1; k < new_num_threads; new_log2_num_threads++, k <<= 1);

  new_u_team = (omp_u_thread_t *) aligned_realloc((void *) __omp_level_1_pthread,
                        sizeof(omp_u_thread_t) * __omp_level_1_team_alloc_size, 
                        sizeof(omp_u_thread_t) * new_num_threads,
                        CACHE_LINE_SIZE);
                        

  Is_True(new_u_team != NULL, ("Cannot realloc level 1 pthread data structure"));

  if (new_u_team != __omp_level_1_pthread) {
    /* squash hash_table */
    /* first clean it */
    __ompc_clear_hash_table();

    for (i=0; i<__omp_level_1_team_alloc_size; i++)
      __ompc_insert_into_hash_table(&(new_u_team[i]));

    /* Warning : the following assignment may fail because of alignment*/
    __omp_root_u_thread = new_u_team;
    __omp_root_v_thread.executor = __omp_root_u_thread;
    __omp_level_1_pthread = new_u_team;

    /* refreshing relationship between data structure */
    for (i=0; i<__omp_level_1_team_alloc_size; i++)
      __omp_level_1_team[i].executor = &(__omp_level_1_pthread[i]);

  }

  new_v_team = (omp_v_thread_t *) aligned_realloc((void *) __omp_level_1_team, 
                      sizeof(omp_v_thread_t) * __omp_level_1_team_alloc_size,
					  sizeof(omp_v_thread_t) * new_num_threads, 
                      CACHE_LINE_SIZE);

  Is_True(new_v_team != NULL, ("Cannot realloc level 1 team data structure"));

  if (new_v_team != __omp_level_1_team) {
    __omp_level_1_team = new_v_team;
    for (i=0; i<__omp_level_1_team_alloc_size; i++)
      __omp_level_1_pthread[i].task = &(__omp_level_1_team[i]);

  }

  for (i=0; i<new_num_threads; i++) {
    __omp_level_1_team[i].team_size = new_num_threads;
  }

  memset(&(__omp_level_1_team[__omp_level_1_team_alloc_size]), 0,
	 sizeof(omp_v_thread_t) * (new_num_threads - __omp_level_1_team_alloc_size));

  /* destroy old xbarrier info */
  __ompc_xbarrier_info_destroy(&__omp_level_1_team_manager);

  __omp_level_1_team_manager.team_size = new_num_threads;
  __omp_level_1_team_manager.log2_team_size = new_log2_num_threads;

  __ompc_event_callback(OMP_EVENT_FORK);

  __omp_level_1_team_manager.task_pool = 
                    __ompc_expand_task_pool(__omp_level_1_team_manager.task_pool,
                                            new_num_threads);

  /* init new xbarrier info with updated team_size */
  __ompc_xbarrier_info_create(&__omp_level_1_team_manager);

  /* need to reset per-thread xbarrier info for existing threads */
  for (i=0; i<__omp_level_1_team_alloc_size; i++) {
    __ompc_init_xbarrier_local_info(&__omp_level_1_team[i].xbarrier_local,
                                    i, &__omp_level_1_team_manager);
  }

  for (i=__omp_level_1_team_alloc_size; i<new_num_threads; i++) {
    /* for v_thread */
    __omp_level_1_team[i].team = &__omp_level_1_team_manager;
    __omp_level_1_team[i].single_count = 0;
    __omp_level_1_team[i].loop_count = 0;
    __omp_level_1_team[i].state = THR_OVHD_STATE;
    __omp_level_1_team[i].thr_lkwt_state_id=0;     
    __omp_level_1_team[i].thr_ctwt_state_id=0;
    __omp_level_1_team[i].thr_atwt_state_id=0;
    __omp_level_1_team[i].thr_ibar_state_id=0;
    __omp_level_1_team[i].thr_ebar_state_id=0;
    __omp_level_1_team[i].thr_odwt_state_id=0;
    __omp_level_1_team[i].vthread_id = i;
    __omp_level_1_team[i].executor = &__omp_level_1_pthread[i];
    __omp_level_1_pthread[i].task = &__omp_level_1_team[i];

    __omp_level_1_team[i].implicit_task = NULL;
    __omp_level_1_team[i].num_suspended_tied_tasks = 0;

    __ompc_init_xbarrier_local_info(&__omp_level_1_team[i].xbarrier_local,
                                    i, &__omp_level_1_team_manager);

    /* for u_thread */
    return_value = pthread_attr_setstacksize(&__omp_pthread_attr, __omp_stack_size);
    Is_True(return_value == 0, ("Cannot set stack size for thread"));
    return_value = pthread_create( &(__omp_level_1_pthread[i].uthread_id),
				   &__omp_pthread_attr, (pthread_entry) __ompc_level_1_slave, 
				   (void *)((unsigned long int)i));
    Is_True(return_value == 0, ("Cannot create more pthreads"));

    __omp_level_1_pthread[i].stack_pointer = (char *)0;

#ifndef TARG_LOONGSON
    if (__omp_set_affinity) {
      // bind pthread to a specific cpu
      __ompc_bind_pthread_to_cpu(__omp_level_1_pthread[i].uthread_id);
    }
#endif //TARG_LOONGSON

    __ompc_insert_into_hash_table(&(__omp_level_1_pthread[i]));
  }

  OMPC_WAIT_WHILE(__omp_level_1_pthread_count != new_num_threads);	
  /* We still should make sure that all the slaves are ready*/
  /* TODO: wait for all slaves*/

  __omp_max_num_threads -= new_num_threads - __omp_level_1_team_alloc_size;
  Is_True(__omp_max_num_threads >= 0, "Invalid number of thread to expand");

  __omp_level_1_team_size = new_num_threads;
  __omp_level_1_team_alloc_size = new_num_threads;
}

/* The main fork API. at the first fork, initialize the RTL*/
void
__ompc_fork(const int _num_threads, omp_micro micro_task, 
	    frame_pointer_t frame_pointer)
{
  int i;
  int return_value;
  int k, log2_num_threads;
  int num_threads = _num_threads;
  omp_team_t temp_team;
  omp_v_thread_t temp_v_thread;
  omp_v_thread_t *nest_v_thread_team;
  omp_u_thread_t *nest_u_thread_team;
  omp_u_thread_t *current_u_thread;
  omp_v_thread_t *original_v_thread;
  omp_task_t *original_task;
  void * stack_pointer;
  unsigned int region_used = 0; // TODO: make it one-bit.
  pthread_attr_t nested_pthread_attr;

#if  !(defined TARG_LOONGSON || defined _UH_COARRAYS)
  Is_True(__omp_rtl_initialized != 0,
          (" RTL should have been initialized!"));
#else
  if (!__omp_rtl_initialized) {
    __ompc_init_rtl(0);
  }
#endif //TARG_LOONGSON

  // check the validity of num_threads
  if (num_threads != 0) 
    num_threads = __ompc_check_num_threads(num_threads);

  if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
    __omp_exe_mode = OMP_EXE_MODE_NORMAL;
    /* level 1 thread fork */
    /* How about num_threads < __omp_level_1_team_size */
    /* TODO: fix this condition*/
    /*
    static int first_time = 0;
    if (first_time) {
      current_region_id++;
      __ompc_set_state(THR_OVHD_STATE);
      __ompc_event_callback(OMP_EVENT_FORK);
    } else {
      first_time=1;
    } 
    */
    current_region_id++;
    __ompc_set_state(THR_OVHD_STATE);
    __ompc_event_callback(OMP_EVENT_FORK);

    // Adjust the number of the number of thread in the team
    if (num_threads == 0) {
     /* use default thread number decided from processor number and environment variable*/
      __omp_level_1_team_size = __omp_nthreads_var;
      __omp_level_1_team_manager.team_size = __omp_nthreads_var;
    } else {
      // expand the team when there is not enough threads
      if (num_threads > __omp_level_1_team_alloc_size) {
        __ompc_expand_level_1_team(num_threads);
        __omp_current_v_thread = &__omp_level_1_team[0];
      }

      if (num_threads != __omp_level_1_team_size) {
        /* change in team size means xbarrier info needs to be reconstructed
         */
        //__ompc_xbarrier_info_destroy(&__omp_level_1_team_manager);

        __omp_level_1_team_size = num_threads;
        __omp_level_1_team_manager.team_size = num_threads;
        log2_num_threads = 0;
        for(k = 1; k < num_threads; log2_num_threads++, k <<= 1);
        __omp_level_1_team_manager.log2_team_size = log2_num_threads;

        /* init new xbarrier info with updated team_size */
        __ompc_xbarrier_info_init(&__omp_level_1_team_manager);

        /* need to reset per-thread xbarrier info for existing threads */
        for (i=0; i<__omp_level_1_team_size; i++) {
          __ompc_init_xbarrier_local_info(&__omp_level_1_team[i].xbarrier_local,
                                          i, &__omp_level_1_team_manager);
        }

      } else {
        __omp_level_1_team_size = num_threads;
        __omp_level_1_team_manager.team_size = num_threads;
        log2_num_threads = 0;
        for(k = 1; k < num_threads; log2_num_threads++, k <<= 1);
        __omp_level_1_team_manager.log2_team_size = log2_num_threads;
      }
    }

    __ompc_task_pool_set_team_size( __omp_level_1_team_manager.task_pool,
                                    __omp_level_1_team_manager.team_size);

    for (i=0; i<__omp_level_1_team_size; i++) {
      __omp_level_1_team[i].frame_pointer = frame_pointer;
      __omp_level_1_team[i].team_size = __omp_level_1_team_size;
      __omp_level_1_team[i].entry_func = micro_task;
    }
    
    /* TODO: the current team size is incorrect, fix it*/
    /* OK, now should start the group */

    if (__omp_level_1_team_size > 1) {
      /* Before signal, should make sure that all slaves are ready*/
      pthread_mutex_lock(&__omp_level_1_mutex);
      (__omp_level_1_team_manager.new_task)++;
      pthread_cond_broadcast(&__omp_level_1_cond);
      pthread_mutex_unlock(&__omp_level_1_mutex);
    }


    __omp_level_1_pthread[0].task = &(__omp_level_1_team[0]);

    __omp_current_v_thread = &__omp_level_1_team[0];

    /* initialize implicit task for master thread of level 1 team */
    if (__omp_level_1_team[0].implicit_task == NULL) {
      __omp_level_1_team[0].implicit_task = __ompc_task_new_implicit();
    }

    __omp_current_task = __omp_level_1_team[0].implicit_task;

    __ompc_set_state(THR_WORK_STATE);
    micro_task(0, frame_pointer);

    __ompc_level_1_barrier(0);

    /* the reason for the overhead state  is so that the collector 
       can query for a region id at join without returning 0 when the threads goes to 
       a serial state according to the white paper. 
       This is important for the callstack creation. */

     __ompc_set_state(THR_OVHD_STATE);
     
    __ompc_event_callback(OMP_EVENT_JOIN);
    __ompc_set_state(THR_SERIAL_STATE);
    __omp_root_v_thread.state=THR_SERIAL_STATE;

    __omp_exe_mode = OMP_EXE_MODE_SEQUENTIAL;
    __omp_level_1_pthread[0].task = &__omp_root_v_thread;
    __omp_current_v_thread = &__omp_root_v_thread;
    __omp_current_task = NULL;

  } else if (__omp_nested == 1) {
    /* OMP_EXE_MODE_IN_PARALLEL, with nested enable */
    /* nested fork */
    /* Maybe we should also ensure that teamsize != 1*/

    int k, log2_num_threads;
    int orig_omp_myid = __omp_myid;

    __omp_exe_mode = OMP_EXE_MODE_NESTED;

    current_u_thread = __ompc_get_current_u_thread();
    original_v_thread = current_u_thread->task;
    original_task = __omp_current_task;

    if (num_threads == 0) num_threads = __omp_nthreads_var;

    log2_num_threads = 0;
    for(k = 1; k < num_threads; log2_num_threads++, k <<= 1);

    temp_team.team_size = num_threads;
    temp_team.is_nested = 1;
    temp_team.team_level = original_v_thread->team->team_level + 1;
    temp_team.barrier_count = 0;
    temp_team.barrier_count2 = 0;
    temp_team.exit_count = 0;
    temp_team.barrier_flag = 0;
    temp_team.new_task = 0;
    /* Used anywhere. obsoleted*/
    temp_team.loop_count = 0;
    temp_team.loop_info_size = 0;
    temp_team.loop_info = NULL;
    temp_team.single_count = 0;

    temp_team.log2_team_size = log2_num_threads;

    /* create task pool for nested team */
    temp_team.task_pool = __ompc_create_task_pool(num_threads);

    __ompc_xbarrier_info_create(&temp_team);

    __ompc_init_spinlock(&(temp_team.schedule_lock));
    pthread_cond_init(&(__omp_level_1_team_manager.ordered_cond), NULL);
    __ompc_init_lock(&(temp_team.single_lock));
    pthread_mutex_init(&(temp_team.barrier_lock), NULL);
    pthread_cond_init(&(temp_team.barrier_cond), NULL);

    nest_v_thread_team = aligned_malloc(sizeof(omp_v_thread_t) * num_threads, CACHE_LINE_SIZE); 
    Is_True(nest_v_thread_team != NULL, 
	    ("Cannot allocate nested v_thread team"));

    /* nest_u_thread_team[0] is of no use currently*/
    nest_u_thread_team = aligned_malloc(sizeof(omp_u_thread_t) * num_threads, CACHE_LINE_SIZE);
    Is_True(nest_u_thread_team != NULL,
	    ("Cannot allocate nested u_thread team"));

    /* A lock is needed to protect global variables */
    /* TODO: need a global lock*/
    __omp_max_num_threads -= num_threads - 1;

    __ompc_set_state(THR_OVHD_STATE);
    __ompc_event_callback(OMP_EVENT_FORK);

    pthread_attr_init(&nested_pthread_attr);
    pthread_attr_setscope(&nested_pthread_attr, PTHREAD_SCOPE_SYSTEM);
    return_value = pthread_attr_setstacksize(&nested_pthread_attr,
            __omp_stack_size);
    Is_True(return_value == 0, ("Cannot set stack size for thread"));
    for (i=1; i<num_threads; i++) {
      nest_v_thread_team[i].vthread_id = i;
      nest_v_thread_team[i].single_count = 0;
      nest_v_thread_team[i].loop_count = 0;
      nest_v_thread_team[i].team = &temp_team;
      nest_v_thread_team[i].team_size = num_threads;
      nest_v_thread_team[i].entry_func = micro_task;
      nest_v_thread_team[i].frame_pointer = frame_pointer;
      nest_v_thread_team[i].executor = &(nest_u_thread_team[i]);

      nest_u_thread_team[i].hash_next = NULL;
      nest_u_thread_team[i].task = &(nest_v_thread_team[i]);

      /* implicit task is created in __ompc_nested_slave */
      nest_v_thread_team[i].implicit_task = NULL;
      nest_v_thread_team[i].num_suspended_tied_tasks = 0;

      __ompc_init_xbarrier_local_info(&nest_v_thread_team[i].xbarrier_local,
                                     i, &temp_team);

      return_value = pthread_create(&(nest_u_thread_team[i].uthread_id),
				    &nested_pthread_attr, (pthread_entry) __ompc_nested_slave,
				    (void *)(&(nest_v_thread_team[i])));
      Is_True(return_value == 0, ("Cannot create more pthreads"));

      nest_u_thread_team[i].stack_pointer = (char *)0;

      // TODO: may need to bind pthread to a specific cpu for nested threads

      /* hash table isn't really necessary if storing current v_thread in
       * __omp_current_v_thread.  */
      //__ompc_insert_into_hash_table(&(nest_u_thread_team[i]));

    }

    pthread_mutex_lock(&temp_team.barrier_lock);
    temp_team.new_task = 1;
    pthread_cond_broadcast(&temp_team.barrier_cond);
    pthread_mutex_unlock(&temp_team.barrier_lock);

    nest_v_thread_team[0].vthread_id = 0;
    nest_v_thread_team[0].single_count = 0;
    nest_v_thread_team[0].loop_count = 0;
    nest_v_thread_team[0].team = &temp_team;
    nest_v_thread_team[0].team_size = num_threads;
    /* The following two maybe not important. */
    nest_v_thread_team[0].entry_func = micro_task;
    nest_v_thread_team[0].frame_pointer = frame_pointer;
    nest_v_thread_team[0].executor = current_u_thread;

    nest_v_thread_team[0].implicit_task = NULL;
    nest_v_thread_team[0].num_suspended_tied_tasks = 0;

    __ompc_init_xbarrier_local_info(&nest_v_thread_team[0].xbarrier_local,
                                   0, &temp_team);

    current_u_thread->task = &(nest_v_thread_team[0]);

    __omp_current_v_thread = &nest_v_thread_team[0];

    /* initialize implicit task for master thread of nested team */
    if (nest_v_thread_team[0].implicit_task == NULL) {
      nest_v_thread_team[0].implicit_task = __ompc_task_new_implicit();
    }

    __omp_current_task = nest_v_thread_team[0].implicit_task;

    /* execution */
    /* A start barrier should also be presented here?*/
    __ompc_set_state(THR_WORK_STATE);

    /* set master thread id to 0 in  nested region */
    __omp_myid = 0;

    micro_task(0, frame_pointer);

    __ompc_exit_barrier(&(nest_v_thread_team[0]));

    /* restore original thread id */
    __omp_myid = orig_omp_myid;

    __ompc_set_state(THR_OVHD_STATE);


    for (i=1; i<num_threads; i++) {
      /* hash table isn't really necessary if storing current v_thread in
       * __omp_current_v_thread.  */
      //__ompc_remove_from_hash_table(nest_u_thread_team[i].uthread_id);
      return_value = pthread_detach(nest_u_thread_team[i].uthread_id);
      Is_True(return_value == 0, ("Could not detach pthread"));
    }

    __ompc_destroy_task_pool(temp_team.task_pool);

    /* destroy xbarrier info for team */
    __ompc_xbarrier_info_destroy(&temp_team);

    aligned_free(nest_v_thread_team);
    aligned_free(nest_u_thread_team);

    current_u_thread->task = original_v_thread;
    __omp_current_v_thread  = original_v_thread;
    __omp_current_task = original_task;

	__ompc_event_callback(OMP_EVENT_JOIN);
	__ompc_set_state(THR_WORK_STATE);
    __omp_exe_mode = OMP_EXE_MODE_NORMAL;

  } else { /* OMP_EXE_MODE_IN_PARALLEL and nested disabled*/
    current_u_thread = __ompc_get_current_u_thread();
    original_v_thread = current_u_thread->task;
    original_task = __omp_current_task;

    temp_team.team_size = 1;
    temp_team.team_level = original_v_thread->team->team_level + 1;
    temp_team.is_nested = 1;
    temp_team.log2_team_size = 1;

    //bug 361, get_local_thread_num() return garbage value, Liao
    temp_v_thread.vthread_id = 0; 
    temp_v_thread.team_size = 1;
    temp_v_thread.vthread_id = 0;
    temp_v_thread.single_count = 0;
    temp_v_thread.loop_count = 0;
    temp_v_thread.executor = current_u_thread;
    temp_v_thread.team = &temp_team;
    temp_v_thread.entry_func = micro_task;
    temp_v_thread.frame_pointer = frame_pointer;
    temp_v_thread.implicit_task = NULL;
    temp_v_thread.num_suspended_tied_tasks = 0;

    __ompc_init_xbarrier_local_info(&temp_v_thread.xbarrier_local,
                                   0, &temp_team);

    /* The lock can be eliminated, anyway */
    /* no need to use lock in this case*/
    temp_team.loop_count = 0;
    temp_team.loop_info_size = 0;
    temp_team.loop_info = NULL;
    temp_team.single_count = 0;

    temp_team.task_pool = NULL;

    /* FIX THIS */
    //__ompc_task_pool_set_team_size( __omp_level_1_team_manager.task_pool, 1);

    __ompc_init_spinlock(&(temp_team.schedule_lock));
    __ompc_init_lock(&(temp_team.single_lock));

    current_u_thread->task = &temp_v_thread;
    __omp_current_v_thread = &temp_v_thread;
    __omp_current_task = NULL;

    /* Do we really need to maintain such a global status */
    /* a lock should be added here */
    __omp_exe_mode = OMP_EXE_MODE_NESTED_SEQUENTIAL;
    /* execute the task */
    __ompc_set_state(THR_WORK_STATE);
    __ompc_set_state(THR_WORK_STATE);
    micro_task(0, frame_pointer);

    /* lock should be added here */
    /* The exe_mode switch is abandoned for this case.*/
    /*
      __omp_nested_team_count--;
      if (__omp_nested_team_count == 0)
      __omp_exe_mode = OMP_EXE_MODE_NORMAL;
    */

    __ompc_destroy_spinlock(&(temp_team.schedule_lock));
    __ompc_destroy_lock(&(temp_team.single_lock));
    current_u_thread->task = original_v_thread;
    __omp_current_v_thread = original_v_thread;
    __omp_current_task = original_task;
  }

}

/* Check the _num_threads against __omp_max_num_threads*/
int
__ompc_check_num_threads(const int _num_threads)
{
  int num_threads = _num_threads;
  int request_threads;
  /* How about _num_threads == 1*/
  Is_Valid( num_threads > 0,
	    ("number of threads must be possitive!"));
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

/* How about Critical/Atomic? */

/* TODO: handle critical/atomic affairs here*/

void
__ompc_serialized_parallel (int vthread_id)
{

}

void
__ompc_end_serialized_parallel (int vthread_id)
{

}

