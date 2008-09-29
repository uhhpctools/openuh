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
 * File: omp_lock.c
 * Abstract: implementation of OpenMP lock and critical sections
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 *          06/09/2005, updated by Liao Chunhua, Univ. of Houston
 * 
 */
 
#include <stdlib.h>
#include "omp_lock.h"
#include "omp_rtl.h"
#include "omp_sys.h"

static omp_lock_t atomic_lock;


omp_int_t omp_get_thread_num(void);
inline void 
__ompc_init_lock (volatile omp_lock_t *lp)
{
  pthread_mutex_init((pthread_mutex_t *)lp, NULL);
}

inline void
__ompc_init_lock_s(volatile omp_lock_t *lp)
{
  pthread_mutex_init((pthread_mutex_t *)lp, NULL);
}


inline void 
__ompc_lock_s(volatile omp_lock_t *lp)
{
   int status = pthread_mutex_trylock((pthread_mutex_t *)lp);
   if(status!=0) {
      __ompc_set_state(THR_LKWT_STATE);
      __ompc_event_callback(OMP_EVENT_THR_BEGIN_LKWT);
      pthread_mutex_lock((pthread_mutex_t *)lp);
      __ompc_event_callback(OMP_EVENT_THR_END_LKWT);
  }

   __ompc_set_state(THR_WORK_STATE);
}


inline void
__ompc_lock(volatile omp_lock_t *lp)
{
  pthread_mutex_lock((pthread_mutex_t *)lp);
}



inline void 
__ompc_unlock (volatile omp_lock_t *lp)
{
  pthread_mutex_unlock((pthread_mutex_t *)lp);


}

inline void
__ompc_unlock_s(volatile omp_lock_t *lp)
{
  pthread_mutex_unlock((pthread_mutex_t *)lp);
}


inline void 
__ompc_destroy_lock (volatile omp_lock_t *lp)
{
  pthread_mutex_destroy((pthread_mutex_t *)lp);
}


inline int 
__ompc_test_lock (volatile omp_lock_t *lp)
{
  return (pthread_mutex_trylock((pthread_mutex_t *)lp) == 0);
}


void 
__ompc_init_nest_lock (volatile omp_nest_lock_t *lp)
{
  __ompc_init_lock (&lp->lock);
  __ompc_init_lock (&lp->wait);
  lp->count = 0;
}

void
__ompc_init_nest_lock_s (volatile omp_nest_lock_t *lp)
{
  __ompc_init_lock (&lp->lock);
  __ompc_init_lock (&lp->wait);
  lp->count = 0;
}


void 
__ompc_nest_lock (volatile omp_nest_lock_t *lp)
{
  pthread_t id = pthread_self();
  int nest;

  if( (lp->count > 0) && (lp->thread_id == id) ) {
    nest = 1;
  } else {
  wait_nest_lock:
    __ompc_lock(&lp->wait); /* be blocked here */
    nest = 0;
  }
  __ompc_lock(&lp->lock);
  if(nest) {
    if( lp->count == 0 ) { /* the 'wait' lock be released */
      if(!__ompc_test_lock(&lp->wait)) {
	__ompc_unlock(&lp->lock);
	goto wait_nest_lock;
      }
      lp->thread_id = id;
    } else { /* lp->count > 0 */
      if(lp->thread_id != id) {
	__ompc_unlock(&lp->lock);
	goto wait_nest_lock;
      }
    }
    lp->count++;
  }  else { /* get the 'wait' lock. Assert:( lp->count == 0 ) */
    lp->thread_id = id;
    lp->count = 1;
  }
  __ompc_unlock(&lp->lock);
}

void
__ompc_nest_lock_s (volatile omp_nest_lock_t *lp)
{
  pthread_t id = pthread_self();
  int nest;

  if( (lp->count > 0) && (lp->thread_id == id) ) {
    nest = 1;
  } else {
  wait_nest_lock:
    __ompc_lock_s(&lp->wait); /* be blocked here */
    nest = 0;
  }
  __ompc_lock(&lp->lock);
  if(nest) {
    if( lp->count == 0 ) { /* the 'wait' lock be released */
      if(!__ompc_test_lock(&lp->wait)) {
        __ompc_unlock(&lp->lock);
        goto wait_nest_lock;
      }
      lp->thread_id = id;
    } else { /* lp->count > 0 */
      if(lp->thread_id != id) {
        __ompc_unlock(&lp->lock);
        goto wait_nest_lock;
      }
    }
    lp->count++;
  }  else { /* get the 'wait' lock. Assert:( lp->count == 0 ) */
    lp->thread_id = id;
    lp->count = 1;
  }
  __ompc_unlock(&lp->lock);
}


void 
__ompc_nest_unlock (volatile omp_nest_lock_t *lp)
{
  __ompc_lock (&lp->lock);
  if(lp->count > 0){
    lp->count--;
    if(lp->count == 0){
      __ompc_unlock_s(&lp->wait);
    }
  }
  __ompc_unlock (&lp->lock);
}

void
__ompc_nest_unlock_s (volatile omp_nest_lock_t *lp)
{
  __ompc_lock (&lp->lock);
  if(lp->count > 0){
    lp->count--;
    if(lp->count == 0){
      __ompc_unlock(&lp->wait);
    }
  }
  __ompc_unlock (&lp->lock);
}


void 
__ompc_destroy_nest_lock (volatile omp_nest_lock_t *lp)
{
  __ompc_destroy_lock (&lp->wait);
  __ompc_destroy_lock (&lp->lock);
}


int 
__ompc_test_nest_lock (volatile omp_nest_lock_t *lp)
{
  pthread_t id = pthread_self();
    
  __ompc_lock(&lp->lock);
  if(lp->count > 0){
    if(lp->thread_id == id){
      lp->count++;
    }
    else{
      __ompc_unlock(&lp->lock);
      return 0;
    }
  }
  else{
    if(!__ompc_test_lock(&lp->wait)){
      __ompc_unlock(&lp->lock);
      return 0;
    }
    lp->thread_id = id;
    lp->count = 1;
  }
  __ompc_unlock(&lp->lock);
  return lp->count;
}

/* for Critical directive */
/*Changed by Liao, the work of init lock has been moved to runtime */

inline void
__ompc_critical(int gtid, volatile omp_lock_t **lck)
{
  __ompc_set_state(THR_OVHD_STATE);
  if (*lck ==NULL) {
    __ompc_lock(&_ompc_thread_lock);
    if ((omp_lock_t*)*lck == NULL){
      *lck = (omp_lock_t *)malloc(sizeof(omp_lock_t));
      Is_True(*lck!=NULL, 
	      ("Cannot allocate lock memory for critical"));
    }
    __ompc_init_lock (*lck);
    __ompc_unlock(&_ompc_thread_lock);
  }
  // __ompc_lock((volatile omp_lock_t *)*lck);

   int status = pthread_mutex_trylock((pthread_mutex_t *)*lck);
   if(status!=0) {
      __ompc_set_state(THR_CTWT_STATE);
      __ompc_event_callback(OMP_EVENT_THR_BEGIN_CTWT);
      pthread_mutex_lock((pthread_mutex_t *)*lck);
       __ompc_event_callback(OMP_EVENT_THR_END_CTWT);
  }
 
   __ompc_set_state(THR_WORK_STATE);

}

inline void
__ompc_end_critical(int gtid, volatile omp_lock_t **lck)
{
  __ompc_unlock((volatile omp_lock_t *)*lck);
  __ompc_set_state(THR_WORK_STATE);
}
inline void
__ompc_reduction(int gtid, volatile omp_lock_t **lck)
{
  /* __ompc_critical(gtid,lck); */

    __ompc_set_state(THR_OVHD_STATE);
  if (*lck ==NULL) {
    __ompc_lock(&_ompc_thread_lock);
    if ((omp_lock_t*)*lck == NULL){
      *lck = (omp_lock_t *)malloc(sizeof(omp_lock_t));
      Is_True(*lck!=NULL,
              ("Cannot allocate lock memory for critical"));
    }
    __ompc_init_lock (*lck);
    __ompc_unlock (&_ompc_thread_lock);
  }
  __ompc_lock((volatile omp_lock_t *)*lck);

  __ompc_set_state(THR_REDUC_STATE);

}
inline void
__ompc_end_reduction(int gtid, volatile omp_lock_t **lck)
{
 /* __ompc_end_critical(gtid,lck); */

  __ompc_unlock((volatile omp_lock_t *)*lck);
  __ompc_set_state(THR_WORK_STATE);
}


inline void
__ompc_critical_light(int gtid, volatile omp_lock_t **lck)
{
  __ompc_spin_lock_s((void*)lck);
}

inline void
__ompc_end_critical_light(int gtid, volatile omp_lock_t **lck)
{
  __ompc_spin_unlock((void*)lck);
 __ompc_set_state(THR_WORK_STATE);
}
