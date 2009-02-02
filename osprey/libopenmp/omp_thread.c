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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>
// #include <Profile/Profiler.h>
#include "omp_thread.h"
//#include "uth.h"
#include "pcl.h"
/*To align with the Pathscale OMP lowering, CWG */
int __ompc_sug_numthreads = 1;
//int regionid =0;
#define debug 0
#define MAX_COUNTER  20000
__thread int __omp_myid;
__thread int __omp_seed;
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
volatile unsigned long int __omp_task_stack_size = OMP_TASK_STACK_SIZE_DEFAULT;
volatile int __omp_task_q_upper_limit = OMP_TASK_Q_UPPER_LIMIT_DEFAULT;
volatile int __omp_task_q_lower_limit = OMP_TASK_Q_LOWER_LIMIT_DEFAULT;
volatile int __omp_task_level_limit = OMP_TASK_LEVEL_LIMIT_DEFAULT;
volatile int __omp_task_limit; //initialize in __ompc_init_rtl
volatile omp_exe_mode_t __omp_exe_mode = OMP_EXE_MODE_DEFAULT;
volatile int __omp_task_mod_level = OMP_TASK_MOD_LEVEL_DEFAULT;

volatile int __omp_empty_flags[OMP_MAX_NUM_THREADS];

omp_v_thread_t * __omp_level_1_team = NULL;
omp_task_t **     __omp_level_1_team_tasks = NULL;
omp_u_thread_t * __omp_level_1_pthread = NULL;
int		 __omp_level_1_team_size = 1;
int		 __omp_level_1_team_alloc_size = 1;
omp_team_t	 __omp_level_1_team_manager;
// omp_team_t       temp_team;

omp_u_thread_t * __omp_uthread_hash_table[UTHREAD_HASH_SIZE];


/*Cody - Task Queues */
omp_task_q_t    __omp_global_task_q;
omp_task_q_t *__omp_private_task_q;
omp_task_q_t    *__omp_local_task_q;

__thread omp_task_t *__omp_current_task;

omp_task_stats_t __omp_task_stats[OMP_MAX_NUM_THREADS];
char *__omp_task_stats_filename = NULL;

cond_func __ompc_task_create_cond = OMP_TASK_CREATE_COND_DEFAULT;

/* use once for pthread creation */
volatile int	 __omp_level_1_pthread_count = 1;
/* use for level_1 team end microtask synchronization */
volatile int	 __omp_level_1_exit_count = 0;

omp_team_t	 __omp_root_team;
omp_u_thread_t * __omp_root_u_thread;
omp_v_thread_t	 __omp_root_v_thread;
pthread_t	 __omp_root_thread_id = -1;

int		  __omp_rtl_initialized = 0;

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
void __ompc_level_1_barrier(const int vthread_id);
void __ompc_exit_barrier(omp_v_thread_t *_v_thread);
void __ompc_fini_rtl();
int  __ompc_init_rtl(int num_threads);
int  __ompc_expand_level_1_team(int new_num_threads);
void *__ompc_level_1_slave(void *_u_thread_id);
void *__ompc_nested_slave(void *_v_thread); 



char OMP_EVENT_NAME[22][50]= {
        "OMP_EVENT_FORK",
        "OMP_EVENT_JOIN",
        "OMP_EVENT_THR_BEGIN_IDLE",
        "OMP_EVENT_THR_END_IDLE",
        "OMP_EVENT_THR_BEGIN_IBAR",
        "OMP_EVENT_THR_END_IBAR",
        "OMP_EVENT_THR_BEGIN_EBAR",
        "OMP_EVENT_THR_END_EBAR",
        "OMP_EVENT_THR_BEGIN_LKWT",
        "OMP_EVENT_THR_END_LKWT",
        "OMP_EVENT_THR_BEGIN_CTWT",
        "OMP_EVENT_THR_END_CTWT",
        "OMP_EVENT_THR_BEGIN_ODWT",
        "OMP_EVENT_THR_END_ODWT",
        "OMP_EVENT_THR_BEGIN_MASTER",
        "OMP_EVENT_THR_END_MASTER",
        "OMP_EVENT_THR_BEGIN_SINGLE",
        "OMP_EVENT_THR_END_SINGLE",
        "OMP_EVENT_THR_BEGIN_ORDERED",
        "OMP_EVENT_THR_END_ORDERED",
        "OMP_EVENT_THR_BEGIN_ATWT",
        "OMP_EVENT_THR_END_ATWT" };


char OMP_STATE_NAME[11][50]= {
    "THR_OVHD_STATE",          /* Overhead */
    "THR_WORK_STATE",          /* Useful work, excluding reduction, master, single, critical */
    "THR_IBAR_STATE",          /* In an implicit barrier */
    "THR_EBAR_STATE",          /* In an explicit barrier */
    "THR_IDLE_STATE",          /* Slave waiting for work */
    "THR_SERIAL_STATE",        /* thread not in any OMP parallel region (initial thread only) */
    "THR_REDUC_STATE",         /* Reduction */
    "THR_LKWT_STATE",          /* Waiting for lock */
    "THR_CTWT_STATE",          /* Waiting to enter critical region */
    "THR_ODWT_STATE",          /* Waiting to execute an ordered region */
    "THR_ATWT_STATE"};         /* Waiting to enter an atomic region */

static void *idletimer=NULL;
static void *forktimer=NULL;
static void *ibarriertimer=NULL;
static void *ebarriertimer=NULL;
static void *locktimer=NULL;
static void *criticaltimer=NULL;
static void *odwttimer=NULL;
static void *mastertimer=NULL;
static void *singletimer=NULL;
static void *orderedtimer=NULL;

/*
void forkevent(OMP_COLLECTORAPI_EVENT event)
{
  Tau_profile_c_timer(&forktimer, "FORK/JOIN: OMP_EVENT_FORK/OMP_EVENT_JOIN ", "", TAU_DEFAULT, "TAU_DEFAULT");
  Tau_start_timer(forktimer, 0);
}
void joinevent(OMP_COLLECTORAPI_EVENT event)
{
   Tau_stop_timer(forktimer);
}

void ibarrier(OMP_COLLECTORAPI_EVENT event)
{
  Tau_profile_c_timer(&ibarriertimer, "IMPLICIT BARRIER: OMP_EVENT_THR_BEGIN_IBAR / OMP_EVENT_THR_END_IBAR", "", TAU_DEFAULT, "TAU_DEFAULT");
  Tau_start_timer(ibarriertimer, 0);
}
void end_ibarrier(OMP_COLLECTORAPI_EVENT event)
{
   Tau_stop_timer(ibarriertimer);
}

void ebarrier(OMP_COLLECTORAPI_EVENT event)
{
  Tau_profile_c_timer(&ebarriertimer, "EXPLICIT BARRIER: OMP_EVENT_THR_BEGIN_EBAR / OMP_EVENT_THR_END_EBAR", "", TAU_DEFAULT, "TAU_DEFAULT");
  Tau_start_timer(ebarriertimer, 0);
}
void end_ebarrier(OMP_COLLECTORAPI_EVENT event)
{
   Tau_stop_timer(ebarriertimer);
}

void locke(OMP_COLLECTORAPI_EVENT event)
{
  Tau_profile_c_timer(&locktimer, "WAITING ON LOCK: OMP_EVENT_THR_BEGIN_LKWT / OMP_EVENT_THR_END_LKWT", "", TAU_DEFAULT, "TAU_DEFAULT");
  Tau_start_timer(locktimer, 0);
}


void end_locke(OMP_COLLECTORAPI_EVENT event)
{
   Tau_stop_timer(locktimer);
}

void criticale(OMP_COLLECTORAPI_EVENT event)
{
  Tau_profile_c_timer(&criticaltimer, "WAITING ON CRITICAL: OMP_EVENT_THR_BEGIN_CTWT / OMP_EVENT_THR_END_CTWT", "", TAU_DEFAULT, "TAU_DEFAULT");
  Tau_start_timer(criticaltimer, 0);
}

void end_criticale(OMP_COLLECTORAPI_EVENT event)
{
   Tau_stop_timer(criticaltimer);
}

void odwte(OMP_COLLECTORAPI_EVENT event)
{
  Tau_profile_c_timer(&odwttimer, "WAITING ON ORDERED: OMP_EVENT_THR_BEGIN_ODWT / OMP_EVENT_THR_END_ODWT", "", TAU_DEFAULT, "TAU_DEFAULT");
  Tau_start_timer(odwttimer, 0);
}

void end_odwte(OMP_COLLECTORAPI_EVENT event)
{
   Tau_stop_timer(odwttimer);
}
void mastere(OMP_COLLECTORAPI_EVENT event)
{
  Tau_profile_c_timer(&mastertimer, "MASTER: OMP_EVENT_THR_BEGIN_MASTER / OMP_EVENT_THR_END_MASTER", "", TAU_DEFAULT, "TAU_DEFAULT");
  Tau_start_timer(mastertimer, 0);
}

void end_mastere(OMP_COLLECTORAPI_EVENT event)
{
   Tau_stop_timer(mastertimer);
}

void singlee(OMP_COLLECTORAPI_EVENT event)
{
  Tau_profile_c_timer(&singletimer, "SINGLE: OMP_EVENT_THR_BEGIN_SINGLE / OMP_EVENT_THR_END_SINGLE", "", TAU_DEFAULT, "TAU_DEFAULT");
  Tau_start_timer(singletimer, 0);
}

void end_singlee(OMP_COLLECTORAPI_EVENT event)
{
   Tau_stop_timer(singletimer);
}
void orderede(OMP_COLLECTORAPI_EVENT event)
{
  Tau_profile_c_timer(&orderedtimer, "ORDERED: OMP_EVENT_THR_BEGIN_ORDERED / OMP_EVENT_THR_END_ORDERED", "", TAU_DEFAULT, "TAU_DEFAULT");
  Tau_start_timer(orderedtimer, 0);
}

void end_orderede(OMP_COLLECTORAPI_EVENT event)
{
   Tau_stop_timer(orderedtimer);
}

void idlee(OMP_COLLECTORAPI_EVENT event)
{
  Tau_profile_c_timer(&idletimer, "IDLE: OMP_EVENT_THR_BEGIN_IDLE / OMP_EVENT_THR_END_IDLE", "", TAU_DEFAULT, "TAU_DEFAULT");
  Tau_start_timer(idletimer, 0);
}

void end_idlee(OMP_COLLECTORAPI_EVENT event)
{
   Tau_stop_timer(idletimer);
}

*/

void dummyfunc(OMP_COLLECTORAPI_EVENT event)
{
  omp_v_thread_t *p_vthread =  __ompc_get_current_v_thread();
  printf("Thread %d EVENT=%s STATE=%s\n",p_vthread->vthread_id,OMP_EVENT_NAME[event-1], OMP_STATE_NAME[p_vthread->state-1]);  

}
OMP_COLLECTORAPI_EC __ompc_req_start(void)
{
  int i;
  if(ompc_req_start==0) {
  for (i=0; i< OMP_EVENT_THR_END_ATWT+1; i++)
  {
   __omp_level_1_team_manager.callbacks[i]= NULL;
 //  temp_team.callbacks[i] = &dummyfunc;
  } // note check callback boundaries.
/* 
 __omp_level_1_team_manager.callbacks[OMP_EVENT_FORK] = &forkevent; 
  __omp_level_1_team_manager.callbacks[OMP_EVENT_JOIN] = &joinevent;
   __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_BEGIN_IBAR] = &ibarrier;
   __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_END_IBAR] = &end_ibarrier;
   __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_BEGIN_EBAR] = &ebarrier;
   __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_END_EBAR] = &end_ebarrier;
   __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_BEGIN_LKWT] = &locke;
   __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_END_LKWT] = &end_locke; 
    __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_BEGIN_CTWT] = &criticale;
   __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_END_CTWT] = &end_criticale;
   __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_BEGIN_ODWT] = &odwte;
    __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_END_ODWT] = &end_odwte;
    __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_BEGIN_MASTER] = &mastere;
    __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_END_MASTER] = &end_mastere;
    __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_BEGIN_SINGLE] = &singlee;
    __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_END_SINGLE] = &end_singlee;
     __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_BEGIN_ORDERED] = &orderede;
    __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_END_ORDERED] = &end_orderede;
    __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_BEGIN_IDLE] = &idlee;
    __omp_level_1_team_manager.callbacks[OMP_EVENT_THR_END_IDLE] = &end_idlee;
*/
ompc_req_start = 1;
  return OMP_ERRCODE_OK;
  }
  else
  return OMP_ERRCODE_SEQUENCE_ERR;
}

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
//  if(debug) printf("Thread %d EVENT=%d STATE=%d\n",p_vthread->vthread_id,(int) event, (int) p_vthread->state);
  if( __omp_level_1_team_manager.callbacks[event])
     __omp_level_1_team_manager.callbacks[event](event);
}

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

  env_var_str = getenv("OMP_TASK_Q_UPPER_LIMIT");
  if(env_var_str != NULL) {
    sscanf(env_var_str, "%d", &__omp_task_q_upper_limit);
  }
  env_var_str = getenv("OMP_TASK_Q_LOWER_LIMIT");
  if(env_var_str != NULL) {
    sscanf(env_var_str, "%d", &__omp_task_q_lower_limit);
  }

  env_var_str = getenv("OMP_TASK_LEVEL_LIMIT");
  if(env_var_str != NULL) {
    sscanf(env_var_str, "%d", &__omp_task_level_limit);
  }

  env_var_str = getenv("OMP_TASK_LIMIT");
  if(env_var_str != NULL) {
      sscanf(env_var_str, "%d", &__omp_task_limit);
  }

  env_var_str = getenv("OMP_TASK_MOD_LEVEL");
  if(env_var_str != NULL) {
    sscanf(env_var_str, "%d", &__omp_task_mod_level);
  }

  env_var_str = getenv("OMP_TASK_STATS_FILE");
  if(env_var_str != NULL) {
    __omp_task_stats_filename = malloc(strlen(env_var_str));
    strcpy(__omp_task_stats_filename, env_var_str);
  }

  env_var_str = getenv("OMP_TASK_CREATE_COND");
  if(env_var_str != NULL)
    {
      if(!strcmp(env_var_str, "depth")) {
	fprintf(stderr, "TASK_CREATE_COND = DEPTH\n");
	__ompc_task_create_cond = __ompc_task_depth_cond;
      }
      else if(!strcmp(env_var_str, "numtasks")) {
	  fprintf(stderr, "TASK_CREATE_COND = NUMTASKS\n");
	  __ompc_task_create_cond = __ompc_task_numtasks_cond;
      }
      else if(!strcmp(env_var_str, "depthmod")) {
	fprintf(stderr, "TASK_CREATE_COND = DEPTHMOD\n");
	__ompc_task_create_cond = __ompc_task_depthmod_cond;
      }
      else if(!strcmp(env_var_str, "queue")){
	fprintf(stderr, "TASK_CREATE_COND = QUEUE\n");
	__ompc_task_create_cond = __ompc_task_queue_cond;
      }
      else if(!strcmp(env_var_str, "true")) {
	fprintf(stderr, "TASK_CREATE_COND = TRUE\n");
	__ompc_task_create_cond = __ompc_task_true_cond;
      }
      else if(!strcmp(env_var_str, "false")) { 
	fprintf(stderr, "TASK_CREATE_COND = FALSE\n");
	__ompc_task_create_cond = __ompc_task_false_cond;
      }
      else {
	Warning("OMP_TASK_CREATE_COND invalid using task create condition: true");
      }
	 
    }
}


/* Used for level_1 team end parallel barrier,
 * Not for Normal use. Using __ompc_barrier instead 
 * */
void
__ompc_level_1_barrier(const int vthread_id)
{


    __ompc_set_state(THR_IBAR_STATE);
    __ompc_event_callback(OMP_EVENT_THR_BEGIN_IBAR);

/*
    omp_task_t *next;



    __ompc_atomic_dec(&__omp_level_1_team_manager.num_tasks);
    

    while(__omp_level_1_team_manager.num_tasks != 0)
    {
	__ompc_task_schedule(&next);
	if(next != NULL) {
	  __ompc_task_switch(__omp_current_task, next);
	}
    }


    __omp_task_stats[__omp_myid].tasks_started += __omp_tasks_started;
    __omp_task_stats[__omp_myid].tasks_skipped += __omp_tasks_skipped;
    __omp_task_stats[__omp_myid].tasks_stolen += __omp_tasks_stolen;
    __omp_task_stats[__omp_myid].tasks_created += __omp_tasks_created;

*/

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

  __ompc_event_callback(OMP_EVENT_THR_END_IBAR);
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

   __ompc_set_state(THR_IBAR_STATE);
  __ompc_event_callback(OMP_EVENT_THR_BEGIN_IBAR);

  pthread_mutex_lock(&(vthread->team->barrier_lock));
  vthread->team->barrier_count += 1;
  pthread_mutex_unlock(&(vthread->team->barrier_lock));

  // Master wait all slaves arrived
  if(vthread->vthread_id == 0) {
    OMPC_WAIT_WHILE(vthread->team->barrier_count != vthread->team->team_size);
  }
 
   __ompc_event_callback(OMP_EVENT_THR_END_IBAR);
}

/* The thread function for level_1 slaves*/
void* 
__ompc_level_1_slave(void * _uthread_index)
{
  long uthread_index;
  int counter;
  int task_expect = 1;
  int initial_idle=1;
  __omp_seed = uthread_index;
  uthread_index = (long) _uthread_index;
  __omp_myid = uthread_index;

  __omp_tasks_started = 0;
  __omp_tasks_skipped = 0;
  __omp_tasks_created = 0;
  __omp_tasks_stolen = 0;

    __ompc_set_state(THR_IDLE_STATE);
//  if(initial_idle) {
   __ompc_event_callback(OMP_EVENT_THR_BEGIN_IDLE);
   //  printf("first level_1_slave\n");
   initial_idle = 0;
//  }

  pthread_mutex_lock(&__omp_level_1_mutex);
  __omp_level_1_pthread_count++;
  pthread_mutex_unlock(&__omp_level_1_mutex);

/*initialize virtual processor - Cody*/
  __ompc_init_vp();

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

    /*the implicit parallel task can use the underlying thread generated by 
      uth_vp_init.
      
      Other than setting a few more variables this is similar to the previous
      non-tasking implementation
    */

    if ( __omp_level_1_team[uthread_index].entry_func != NULL) {
         
	
      //	__omp_level_1_team_tasks[uthread_index] = uth_self();
      __omp_level_1_team_tasks[uthread_index] = co_current();
      //	printf("__omp_level_1_team_tasks[%d] = %X\n", uthread_index, __omp_level_1_team_tasks[uthread_index]);
	__omp_current_task = __omp_level_1_team_tasks[uthread_index];
	__omp_current_task->is_parallel_task = 1;
	__omp_current_task->creator = NULL;
	__omp_current_task->num_children = 0;
	__omp_current_task->depth = 0;
	__omp_current_task->threadid = __omp_myid;
	__ompc_init_lock(&__omp_current_task->lock);

      __ompc_event_callback(OMP_EVENT_THR_END_IDLE);
      __ompc_set_state(THR_WORK_STATE);
	__omp_level_1_team[uthread_index].entry_func(uthread_index, (void *) __omp_level_1_team[uthread_index].frame_pointer);

      __omp_level_1_team[uthread_index].entry_func = NULL;
      __ompc_level_1_barrier(uthread_index);

      __ompc_set_state(THR_IDLE_STATE);
     // idle_count++;
      __ompc_event_callback(OMP_EVENT_THR_BEGIN_IDLE);
     //  printf("IDLE count=%d\n",idle_count);


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

   __ompc_set_state(THR_IDLE_STATE);
  printf("IDLE called from nested\n");
//  __ompc_event_callback(OMP_EVENT_THR_BEGIN_IDLE);


  OMPC_WAIT_WHILE(my_vthread->team->new_task != 1);
  /* The relationship between vthread, uthread, and team should be OK here*/

  __ompc_event_callback(OMP_EVENT_THR_END_IDLE);
  __ompc_set_state(THR_WORK_STATE);
 
  my_vthread->entry_func(my_vthread->vthread_id,
			 (char *)my_vthread->frame_pointer);

  /*TODO: fix the barrier call for nested threads*/
  __ompc_exit_barrier(my_vthread);

  pthread_exit(NULL);

}

void
__ompc_fini_rtl(void) 
{

  /*print task stats*/

  int i;
  double avg_tasks_created=0;
  double avg_tasks_skipped=0;
  double avg_tasks_stolen=0;
  double avg_tasks_started=0;

  double total_tasks_created=0;
  double total_tasks_skipped=0;
  double total_tasks_stolen=0;
  double total_tasks_started=0;
  FILE *file;

  if(__omp_task_stats_filename != NULL)
    file = fopen(__omp_task_stats_filename, "w");
  else
    file = stdout;

  for(i=0; i<__omp_level_1_team_manager.team_size;i++)
    {


      double temp, size;
      size = (double) __omp_level_1_team_manager.team_size;

      temp = (double) __omp_task_stats[i].tasks_created;
      avg_tasks_created += temp/size;
      
      temp = (double) __omp_task_stats[i].tasks_skipped;
      avg_tasks_skipped += temp/size;

      temp = (double) __omp_task_stats[i].tasks_stolen;
      avg_tasks_stolen += temp/size;

      temp = (double) __omp_task_stats[i].tasks_started;
      avg_tasks_started += temp/size;
      /*
      total_tasks_created += __omp_task_stats[i].tasks_created;
      total_tasks_skipped += __omp_task_stats[i].tasks_skipped;
      total_tasks_stolen += __omp_task_stats[i].tasks_stolen;
      total_tasks_started += __omp_task_stats[i].tasks_started;
      */
/* commented by Oscar
      fprintf(file, "thread %d:\n", i);
      fprintf(file, "\ttasks_created = %u\n", 
	      __omp_task_stats[i].tasks_created);
      fprintf(file, "\ttasks_skipped = %u\n", 
	      __omp_task_stats[i].tasks_skipped);
      fprintf(file, "\ttasks_started = %u\n", 
	      __omp_task_stats[i].tasks_started);
      fprintf(file, "\ttasks_stolen = %u\n", 
	      __omp_task_stats[i].tasks_stolen);

*/
    }
/* commented  by Oscar
  fprintf(file, "Average of all threads:\n");
  fprintf(file, "\ttasks_created = %f\n", avg_tasks_created);
  fprintf(file, "\ttasks_skipped = %f\n", avg_tasks_skipped);
  fprintf(file, "\ttasks_started = %f\n", avg_tasks_started);
  fprintf(file, "\ttasks_stolen = %f\n", avg_tasks_stolen); */
  /*
  fprintf(file, "Total of all threads:\n");
  fprintf(file, "\ttasks_created = %f\n", total_tasks_created);
  fprintf(file, "\ttasks_skipped = %f\n", total_tasks_skipped);
  fprintf(file, "\ttasks_started = %f\n", total_tasks_started);
  fprintf(file, "\ttasks_stolen = %f\n", total_tasks_stolen);
  
  */
  if(file != stdout)
    fclose(file);

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

  if(threads_to_create == 1)
      __ompc_task_create_cond = __ompc_task_false_cond;

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


/*initialize uth library Cody */
//  fprintf(stderr,"task_stack_size = %ld\n", __omp_task_stack_size);

  /*
  for(i=0; i<threads_to_create; i++)
    __omp_empty_flags[i] = 1;
  */

  //  uth_init(__omp_task_stack_size);
  //  uth_vp_init(0, NULL);

  __omp_task_limit = OMP_TASK_LIMIT_DEFAULT;

  for(i=0; i<threads_to_create; i++)
    {
      __omp_task_stats[i].tasks_started = 0;
      __omp_task_stats[i].tasks_created = 0;
      __omp_task_stats[i].tasks_skipped = 0;
      __omp_task_stats[i].tasks_stolen = 0;
    }

  __ompc_init_vp();
  __omp_myid = 0;
  __omp_seed = 0;

  /*intialize task queues */

  __ompc_task_q_init(&__omp_global_task_q);

  /*
  __omp_local_task_q = malloc(sizeof(omp_task_q_t) * threads_to_create);
  */

    __omp_local_task_q = calloc(threads_to_create, sizeof(omp_task_q_t));
    __omp_private_task_q = calloc(threads_to_create, sizeof(omp_task_q_t));
  
  Is_True(__omp_local_task_q != NULL,
	  ("Can't allocate __omp_local_task_q"));

  Is_True(__omp_private_task_q != NULL,
	  ("Can't allocate __omp_private_task_q"));


  
  for(i=0; i<threads_to_create; i++)
    {
      __ompc_task_q_init(&__omp_local_task_q[i]);
      __ompc_task_q_init(&__omp_private_task_q[i]);
    }

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

  __omp_level_1_team_tasks =
    (omp_task_t **) malloc(sizeof(omp_task_t *) * threads_to_create);

  Is_True(__omp_level_1_team_tasks != NULL,
	  ("Can't allocatae __omp_level_1_team_tasks"));
  memset(__omp_level_1_team_tasks, 0, sizeof(omp_task_t*) * threads_to_create);

  __omp_level_1_team_manager.team_size = threads_to_create;
  __omp_level_1_team_manager.team_level = 1;
  __omp_level_1_team_manager.is_nested = 0;
  __omp_level_1_team_manager.barrier_count = 0;
  __omp_level_1_team_manager.barrier_flag = 0;
  __omp_level_1_team_manager.single_count = 0;
  __omp_level_1_team_manager.new_task = 0;
  __omp_level_1_team_manager.loop_count = 0;
/*Cody - initialize num tasks to 0 */
  __omp_level_1_team_manager.num_tasks = 0;
	
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
     __omp_level_1_team[i].state = THR_OVHD_STATE;
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

  __ompc_event_callback(OMP_EVENT_FORK);

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
  __ompc_event_callback(OMP_EVENT_FORK);
  for (i=__omp_level_1_team_alloc_size; i<new_num_threads; i++) {
    /* for v_thread */
    __omp_level_1_team[i].team = &__omp_level_1_team_manager;
    __omp_level_1_team[i].single_count = 0;
    __omp_level_1_team[i].loop_count = 0;
     __omp_level_1_team[i].state = THR_OVHD_STATE;
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

 // regionid++;
  __ompc_req_start();
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
    __omp_root_v_thread.state=THR_OVHD_STATE;
    __ompc_init_rtl(num_threads);
  }
  else
  {
    __ompc_set_state(THR_OVHD_STATE);
    __ompc_event_callback(OMP_EVENT_FORK);
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

        __omp_level_1_team_manager.num_tasks = __omp_level_1_team_size;
	//        __omp_level_1_team_manager.num_active_tasks = __omp_level_1_team_size;
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

    //    __omp_level_1_team_tasks[0] = uth_self();
    __omp_level_1_team_tasks[0] = co_current();
    __omp_current_task = __omp_level_1_team_tasks[0];
    __omp_current_task->is_parallel_task = 1;
    __omp_current_task->creator = NULL;
    __omp_current_task->num_children = 0;
    __omp_current_task->depth = 0;
    __omp_current_task->threadid = 0;
    __ompc_init_lock(&__omp_current_task->lock);


     __ompc_set_state(THR_WORK_STATE);
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
     __ompc_event_callback(OMP_EVENT_FORK);
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
    __ompc_set_state(THR_WORK_STATE);
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
    __ompc_set_state(THR_WORK_STATE);
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

  __ompc_set_state(THR_SERIAL_STATE);
  __ompc_event_callback(OMP_EVENT_JOIN);

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
