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
 * File: omp_thread.c
 * Abstract: routines for thread management
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 *          06/09/2005, updated by Liao Chunhua, Univ. of Houston
 *          06/20/2007, updated by He Jiangzhou, Tsinghua Univ.
 * 
 */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include "omp_thread.h"

/*To align with the Pathscale OMP lowering, CWG */
int __ompc_sug_numthreads = 1;


#define MAX_COUNTER  20000
volatile int __omp_nested = OMP_NESTED_DEFAULT;          /* nested enable/disable */
volatile int __omp_dynamic = OMP_DYNAMIC_DEFAULT;         /* dynamic enable/disable */
/* max num of thread available*/
volatile int __omp_max_num_threads = OMP_MAX_NUM_THREADS - 1;
/* stores the number of threads requested for future parallel regions. */
volatile int __omp_nthreads_var;
/* num of processors available*/
int __omp_num_processors;
/* default schedule type and chunk size of runtime schedule*/
omp_sched_t  __omp_rt_sched_type = OMP_SCHED_DEFAULT;
int  __omp_rt_sched_size = OMP_CHUNK_SIZE_DEFAULT;

volatile unsigned long int __omp_stack_size = OMP_STACK_SIZE_DEFAULT;

volatile omp_exe_mode_t __omp_exe_mode = OMP_EXE_MODE_DEFAULT;

omp_v_thread_t * __omp_level_1_team = NULL;
omp_u_thread_t * __omp_level_1_pthread = NULL;
int		 __omp_level_1_team_size = 1;
int		 __omp_level_1_team_alloc_size = 1;
omp_team_t	 __omp_level_1_team_manager;
omp_u_thread_t * __omp_uthread_hash_table[UTHREAD_HASH_SIZE];

/* use once for pthread creation */
volatile int	 __omp_level_1_pthread_count = 1;
/* use for level_1 team end microtask synchronization */
volatile int	 __omp_level_1_exit_count = 0;

omp_team_t	 __omp_root_team;
omp_u_thread_t * __omp_root_u_thread;
omp_v_thread_t	 __omp_root_v_thread;
pthread_t	 __omp_root_thread_id = -1;

int		  __omp_rtl_initialized = 0;
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

/* maybe a separate attribute should be here for nested pthreads */

/* sysnem lock variables */

/* prototype */
void __ompc_environment_variables();
void __ompc_level_1_barrier(const int vthread_id);
void __ompc_exit_barrier(omp_v_thread_t *_v_thread);
void __ompc_fini_rtl();
int  __ompc_init_rtl(int num_threads);
int  __ompc_expand_level_1_team(int new_num_threads);
void *__ompc_level_1_slave(void *_u_thread_id);
void *__ompc_nested_slave(void *_v_thread); 

void 
__ompc_environment_variables()
{
  char *env_var_str;
  int  env_var_val;
	
  env_var_str = getenv("OMP_NUM_THREADS");
  if (env_var_str != NULL) {
    sscanf(env_var_str, "%d", &env_var_val);
    Is_Valid(env_var_val > 0, ("OMP_NUM_THREAD should > 0"));
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
	       ("An ',' is expected before the chunksize"));
      env_var_str = Trim_Leading_Spaces(++env_var_str);
      Is_Valid(isdigit((int)(*env_var_str)),
	       ("number expected for chunksize"));
      sscanf(env_var_str, "%d", &env_var_val);
      Is_Valid(env_var_val > 0, 
	       ("Positive number expected"));
      __omp_rt_sched_size = env_var_val;
    } else { /* no chunk size specified */
      if(__omp_rt_sched_type == OMP_SCHED_STATIC)
	__omp_rt_sched_type = OMP_SCHED_STATIC_EVEN;
    }

  }

  /* determine the stack size of slave threads. */
  env_var_str = getenv("OMP_SLAVE_STACK_SIZE");
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
}


/* Used for level_1 team end parallel barrier,
 * Not for Normal use. Using __ompc_barrier instead 
 * */
void
__ompc_level_1_barrier(const int vthread_id)
{
  pthread_mutex_lock(&__omp_level_1_barrier_mutex);
  __omp_level_1_exit_count++;
  if (vthread_id == 0) {
    while (__omp_level_1_exit_count != __omp_level_1_team_size)
      pthread_cond_wait(&__omp_level_1_barrier_cond, &__omp_level_1_barrier_mutex);
    // reset for next usage, Warning: No need to lock it.
    __omp_level_1_exit_count = 0;
    __omp_level_1_team_manager.barrier_flag = 0;
  } else if (__omp_level_1_exit_count == __omp_level_1_team_size)
    pthread_cond_signal(&__omp_level_1_barrier_cond);
  pthread_mutex_unlock(&__omp_level_1_barrier_mutex);
}

/* Used for Nested team end parallel barrier. Since
 * all slaves will exit.
 * */
void
__ompc_exit_barrier(omp_v_thread_t * vthread)
{
  // Assuming that vthread->team_size != 1 
  Is_True((vthread != NULL) && (vthread->team != NULL), 
	  ("bad vthread or vthread->team in nested groups"));

  pthread_mutex_lock(&(vthread->team->barrier_lock));
  vthread->team->barrier_count += 1;
  pthread_mutex_unlock(&(vthread->team->barrier_lock));

  // Master wait all slaves arrived
  if(vthread->vthread_id == 0) {
    OMPC_WAIT_WHILE(vthread->team->barrier_count != vthread->team->team_size);
  }
}

/* The thread function for level_1 slaves*/
void* 
__ompc_level_1_slave(void * _uthread_index)
{
  long uthread_index;
  int counter;
  int task_expect = 1;

  uthread_index = (long) _uthread_index;

  pthread_mutex_lock(&__omp_level_1_mutex);
  __omp_level_1_pthread_count++;
  pthread_mutex_unlock(&__omp_level_1_mutex);

  for(;;) {
    if (__omp_level_1_team_manager.new_task != task_expect) {
      for( counter = 0; __omp_level_1_team_manager.new_task != task_expect; counter++) {
	if (counter > MAX_COUNTER) {
	  pthread_mutex_lock(&__omp_level_1_mutex);
	  while (__omp_level_1_team_manager.new_task != task_expect) {
	    pthread_cond_wait(&__omp_level_1_cond, &__omp_level_1_mutex);
	  }
	  pthread_mutex_unlock(&__omp_level_1_mutex);
	  counter = 0;
	}
      }
    }

    task_expect = !task_expect;

    /* The program should exit now? */
    /*		if (__omp_exit_now == true) 
		break;
    */

    /* exe micro_task now */
    if ( __omp_level_1_team[uthread_index].entry_func != NULL) {
      __omp_level_1_team[uthread_index].entry_func(uthread_index,
						   (char *)__omp_level_1_team[uthread_index].frame_pointer);

      __omp_level_1_team[uthread_index].entry_func = NULL;
      __ompc_level_1_barrier(uthread_index);

    }
  }


  return NULL;
}

/* The thread function for nested slaves*/
void*
__ompc_nested_slave(void * _v_thread)
{
  omp_v_thread_t * my_vthread = (omp_v_thread_t *) _v_thread;
  /* need to wait for others ready? */

  OMPC_WAIT_WHILE(my_vthread->team->new_task != 1);
  /* The relationship between vthread, uthread, and team should be OK here*/

  my_vthread->entry_func(my_vthread->vthread_id,
			 (char *)my_vthread->frame_pointer);

  /*TODO: fix the barrier call for nested threads*/
  __ompc_exit_barrier(my_vthread);

  pthread_exit(NULL);

}

void
__ompc_fini_rtl(void) 
{
  /* clean up job*/
  if (__omp_level_1_team != NULL)
    free(__omp_level_1_team);
  if (__omp_level_1_pthread != NULL)
    free(__omp_level_1_pthread);

  /* Other mutex, conditions, locks , should be destroyed here*/

}

/* must be called when the first fork()*/
int 
__ompc_init_rtl(int num_threads)
{
  int threads_to_create;
  int i;
  int return_value;


  Is_True(__omp_rtl_initialized == 0, 
	  (" RTL has been initialized yet!"));


  /* set default thread number to processor number */
  __omp_num_processors = Get_SMP_CPU_num();
  __omp_nthreads_var = __omp_num_processors;

  /* parse OpenMP environment variables */
  __ompc_environment_variables();

  /* register the finalize function*/
  atexit(__ompc_fini_rtl);

  /* determine number of threads to create*/
  threads_to_create = num_threads == 0 ? __omp_nthreads_var : num_threads;
 /*keep it as nthreads-var suggested in spec. Liao*/
  __omp_nthreads_var = threads_to_create;
  /* setup pthread attributes */
  pthread_attr_init(&__omp_pthread_attr);
  pthread_attr_setscope(&__omp_pthread_attr, PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setstacksize(&__omp_pthread_attr, __omp_stack_size);
  /* need to set up barrier attributes */

  /* initial global locks*/
  pthread_mutex_init(&__omp_level_1_mutex, NULL);
  pthread_mutex_init(&__omp_level_1_barrier_mutex, NULL);
  pthread_mutex_init(&__omp_hash_table_lock, NULL);
  pthread_cond_init(&__omp_level_1_cond, NULL);
  pthread_cond_init(&__omp_level_1_barrier_cond, NULL);
  __ompc_init_lock(&_ompc_thread_lock);

  /* clean up uthread hash table */
  __ompc_clear_hash_table();

  /* create level 1 team */
  __omp_level_1_pthread = 
    (omp_u_thread_t *) malloc(sizeof(omp_u_thread_t) * threads_to_create);
  Is_True(__omp_level_1_pthread != NULL, 
	  ("Can't allocate __omp_level_1_pthread"));
  memset(__omp_level_1_pthread, 0, sizeof(omp_u_thread_t) * threads_to_create);

  __omp_level_1_team = 
    (omp_v_thread_t *) malloc(sizeof(omp_v_thread_t) * threads_to_create);
  Is_True(__omp_level_1_team != NULL, 
	  ("Can't allocate __omp_level_1_team"));
  memset(__omp_level_1_team, 0, sizeof(omp_v_thread_t) * threads_to_create);

  __omp_level_1_team_manager.team_size = threads_to_create;
  __omp_level_1_team_manager.team_level = 1;
  __omp_level_1_team_manager.is_nested = 0;
  __omp_level_1_team_manager.barrier_count = 0;
  __omp_level_1_team_manager.barrier_flag = 0;
  __omp_level_1_team_manager.single_count = 0;
  __omp_level_1_team_manager.new_task = 0;
  __omp_level_1_team_manager.loop_count = 0;
	
  __ompc_init_lock(&(__omp_level_1_team_manager.schedule_lock));
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

  __omp_root_v_thread.entry_func = NULL;
  __omp_root_v_thread.frame_pointer = NULL;

  __omp_root_thread_id = pthread_self();

  __omp_root_u_thread->uthread_id = __omp_root_thread_id;
  __omp_root_u_thread->hash_next = NULL;
  __ompc_insert_into_hash_table(__omp_root_u_thread);

  for (i=1; i< threads_to_create; i++) {
    return_value = pthread_create( &(__omp_level_1_pthread[i].uthread_id),
				   &__omp_pthread_attr, (pthread_entry) __ompc_level_1_slave, 
				   (void *)((unsigned long int)i));
    Is_True(return_value == 0, ("Can not create more pthread"));
    __ompc_insert_into_hash_table(&(__omp_level_1_pthread[i]));
  }

  OMPC_WAIT_WHILE(__omp_level_1_pthread_count != threads_to_create);	

  /* We still should make sure that all the slaves are ready*/
  /* TODO: wait for all slaves*/

  __omp_level_1_team_size = threads_to_create;
  __omp_level_1_team_alloc_size = threads_to_create;
  __omp_max_num_threads -= threads_to_create;
  __omp_rtl_initialized = 1;
  return threads_to_create;
}

/* Expand level_1_team to new_num_threads.
 * Original size: __omp_level_1_team_alloc_size.
 * return: the real allocated size( expect new_num_threads */
int
__ompc_expand_level_1_team(int new_num_threads)
{
  int i;
  int return_value;
  omp_u_thread_t *new_u_team;
  omp_v_thread_t *new_v_team;

  new_u_team = (omp_u_thread_t *) realloc((void *) __omp_level_1_pthread,
					  sizeof(omp_u_thread_t) * new_num_threads);

  Is_True(new_u_team != NULL, ("Can not realloc level 1 pthread data structure"));

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

  new_v_team = (omp_v_thread_t *) realloc((void *) __omp_level_1_team, 
					  sizeof(omp_v_thread_t) * new_num_threads);

  Is_True(new_v_team != NULL, ("Can not realloc level 1 team data structure"));

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
  __omp_level_1_team_manager.team_size = new_num_threads;

  for (i=__omp_level_1_team_alloc_size; i<new_num_threads; i++) {
    /* for v_thread */
    __omp_level_1_team[i].team = &__omp_level_1_team_manager;
    __omp_level_1_team[i].single_count = 0;
    __omp_level_1_team[i].loop_count = 0;
    __omp_level_1_team[i].vthread_id = i;
    __omp_level_1_team[i].executor = &__omp_level_1_pthread[i];
    __omp_level_1_pthread[i].task = &__omp_level_1_team[i];

    /* for u_thread */
    return_value = pthread_create( &(__omp_level_1_pthread[i].uthread_id),
				   &__omp_pthread_attr, (pthread_entry) __ompc_level_1_slave, 
				   (void *)((unsigned long int)i));
    Is_True(return_value == 0, ("Can not create more pthread"));
    __ompc_insert_into_hash_table(&(__omp_level_1_pthread[i]));
  }

  OMPC_WAIT_WHILE(__omp_level_1_pthread_count != new_num_threads);	
  /* We still should make sure that all the slaves are ready*/
  /* TODO: wait for all slaves*/

  __omp_max_num_threads -= new_num_threads - __omp_level_1_team_alloc_size;
  __omp_level_1_team_size = new_num_threads;
  __omp_level_1_team_alloc_size = new_num_threads;
  return new_num_threads;
}

/* The main fork API. at the first fork, initialize the RTL*/
void
__ompc_fork(const int _num_threads, omp_micro micro_task, 
	    frame_pointer_t frame_pointer)
{
  int i;
  int return_value;
  int num_threads = _num_threads;
  omp_team_t temp_team;
  omp_v_thread_t temp_v_thread;
  omp_v_thread_t *nest_v_thread_team;
  omp_u_thread_t *nest_u_thread_team;
  omp_u_thread_t *current_u_thread;
  omp_v_thread_t *original_v_thread;

  /* TODO: We still need to check the limitation before real fork?*/
  if ( num_threads !=0) {
    num_threads = __ompc_check_num_threads(num_threads);
  } else {
    /* no valid num_threads provided, using default, still
     * need to check the validity against __omp_max_num_threads*/
    /*		num_threads = __omp_num_threads;
		this is a bug causing 4 threads are always created by default, 
		let init_rtl to decide  right threads number and save it as __omp_num_threads
		Liao*/
  }


  if (__omp_rtl_initialized == 0 ) {
    /* first fork, initial rtl*/
    __ompc_init_rtl(num_threads);
  }

  if (__omp_exe_mode & OMP_EXE_MODE_SEQUENTIAL) {
    __omp_exe_mode = OMP_EXE_MODE_NORMAL;
    /* level 1 thread fork */
    /* How about num_threads < __omp_level_1_team_size */
    /* TODO: fix this condition*/
    if (num_threads > __omp_level_1_team_alloc_size) {
      Is_True( __omp_max_num_threads >0,
	       ("reach thread number limit, no more new threads"));
      __ompc_expand_level_1_team(num_threads);
    } else if(num_threads>0) {
      __omp_level_1_team_size = num_threads;
      __omp_level_1_team_manager.team_size = num_threads;
    } else if (num_threads==0) {
      /* use default thread number decided from processor number and environment variable*/
      __omp_level_1_team_size = __omp_nthreads_var;
      __omp_level_1_team_manager.team_size = __omp_nthreads_var;
    }

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
      __omp_level_1_team_manager.new_task = !__omp_level_1_team_manager.new_task;
      pthread_cond_broadcast(&__omp_level_1_cond);
      pthread_mutex_unlock(&__omp_level_1_mutex);
    }

    __omp_level_1_pthread[0].task = &(__omp_level_1_team[0]);

    micro_task(0, frame_pointer);

    __ompc_level_1_barrier(0);
    __omp_exe_mode = OMP_EXE_MODE_SEQUENTIAL;
    __omp_level_1_pthread[0].task = &__omp_root_v_thread;

  } else if (__omp_nested == 1) {
    /* OMP_EXE_MODE_IN_PARALLEL, with nested enable */
    /* nested fork */
    /* Maybe we should also ensure that teamsize != 1*/

    __omp_exe_mode = OMP_EXE_MODE_NESTED;

    current_u_thread = __ompc_get_current_u_thread();
    original_v_thread = current_u_thread->task;

    //inherit the thread number from the 1st level. Liao
    if (num_threads ==0 ) num_threads = __omp_level_1_team_size;
    temp_team.team_size = num_threads;
    temp_team.is_nested = 1;
    temp_team.team_level = original_v_thread->team->team_level + 1;
    temp_team.barrier_count = 0;
    temp_team.barrier_flag = 0;
    temp_team.new_task = 0;
    /* Used anywhere. obsoleted*/
    temp_team.loop_count = 0;
    temp_team.single_count = 0;

    __ompc_init_lock(&(temp_team.schedule_lock));
    pthread_cond_init(&(__omp_level_1_team_manager.ordered_cond), NULL);
    __ompc_init_lock(&(temp_team.single_lock));
    pthread_mutex_init(&(temp_team.barrier_lock), NULL);
    pthread_cond_init(&(temp_team.barrier_cond), NULL);
 
    nest_v_thread_team = (omp_v_thread_t *)malloc(
						  sizeof(omp_v_thread_t) * num_threads);
    Is_True(nest_v_thread_team != NULL, 
	    ("Can't allocate nested v_thread team"));

    /* nest_u_thread_team[0] is of no use currently*/
    nest_u_thread_team = (omp_u_thread_t *)malloc(
						  sizeof(omp_u_thread_t) * num_threads );
    Is_True(nest_u_thread_team != NULL,
	    ("Can't allocate nested u_thread team"));

    /* A lock is needed to protect global variables */
    /* TODO: need a global lock*/
    __omp_max_num_threads -= num_threads - 1;
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
      return_value = pthread_create(&(nest_u_thread_team[i].uthread_id),
				    &__omp_pthread_attr, (pthread_entry) __ompc_nested_slave, 
				    (void *)(&(nest_v_thread_team[i])));
      Is_True(return_value == 0, ("Can not create more pthread"));
      __ompc_insert_into_hash_table(&(nest_u_thread_team[i]));
    }

    temp_team.new_task = 1;

    nest_v_thread_team[0].vthread_id = 0;
    nest_v_thread_team[0].single_count = 0;
    nest_v_thread_team[0].loop_count = 0;
    nest_v_thread_team[0].team = &temp_team;
    nest_v_thread_team[0].team_size = num_threads;
    /* The following two maybe not important. */
    nest_v_thread_team[0].entry_func = micro_task;
    nest_v_thread_team[0].frame_pointer = frame_pointer;
    nest_v_thread_team[0].executor = current_u_thread;
    current_u_thread->task = &(nest_v_thread_team[0]);

    /* execution */
    /* A start barrier should also be presented here?*/

    micro_task(0, frame_pointer);

    __ompc_exit_barrier(&(nest_v_thread_team[0]));

    for (i=1; i<num_threads; i++) {
      __ompc_remove_from_hash_table(nest_u_thread_team[i].uthread_id);
    }

    free(nest_v_thread_team);
    free(nest_u_thread_team);

    current_u_thread->task = original_v_thread;
			
    /* TODO: __omp_exe_mode switch here*/
  } else { /* OMP_EXE_MODE_IN_PARALLEL and nested disabled*/
    current_u_thread = __ompc_get_current_u_thread();
    original_v_thread = current_u_thread->task;

    //bug 361, get_local_thread_num() return garbage value, Liao
    temp_v_thread.vthread_id = 0; 
    temp_v_thread.team_size = 1;
    temp_v_thread.single_count = 0;
    temp_v_thread.loop_count = 0;
    temp_v_thread.executor = current_u_thread;
    temp_v_thread.team = &temp_team;
    temp_v_thread.entry_func = micro_task;
    temp_v_thread.frame_pointer = frame_pointer;

    temp_team.team_size = 1;
    temp_team.team_level = original_v_thread->team->team_level + 1;
    temp_team.is_nested = 1;

    /* The lock can be eliminated, anyway */
    /* no need to use lock in this case*/
    temp_team.loop_count = 0;
    temp_team.single_count = 0;

    __ompc_init_lock(&(temp_team.schedule_lock));
    __ompc_init_lock(&(temp_team.single_lock));

    current_u_thread->task = &temp_v_thread;

    /* Do we really need to maintain such a global status */
    /* a lock should be added here */
    __omp_exe_mode = OMP_EXE_MODE_NESTED_SEQUENTIAL;
    /* execute the task */

    micro_task(0, frame_pointer);

    /* lock should be added here */
    /* The exe_mode switch is abandoned for this case.*/
    /*
      __omp_nested_team_count--;
      if (__omp_nested_team_count == 0)
      __omp_exe_mode = OMP_EXE_MODE_NORMAL;
    */

    __ompc_destroy_lock(&(temp_team.schedule_lock));
    __ompc_destroy_lock(&(temp_team.single_lock));
  }
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
