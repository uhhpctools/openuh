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
 * File: omp_sys.h
 * Abstract: architecture dependent routines
 * History: 06/20/2007, built by He Jiangzhou, Tsinghua Univ.
 * 
 */
#ifndef __omp_sys_included
#define __omp_sys_included

#  ifndef TARG_IA64
//#  ifdef TARG_X8664 // IA32 will fall through to the Itanium branch!

static inline void
__ompc_spin_lock(volatile int* lock)
{
  int result = 1;
  do {
    __asm__ __volatile__ ("xchgl %0,(%1)":"=r"(result):"r"(lock),"0"(result));
  } while (result);
}

static inline void
__ompc_spin_unlock(volatile int* lock)
{
  *lock = 0;
}


//TODO implement the following two functions as atomic operation
static inline int
__ompc_atomic_inc(volatile int* value)
{
  int result;
  static int lock = 0;
  __ompc_spin_lock(&lock);
  result = ++(*value);
  __ompc_spin_unlock(&lock);
  return result;
}

static inline int
__ompc_atomic_dec(volatile int* value)
{
  int result;
  static int lock = 0;
  __ompc_spin_lock(&lock);
  result = --(*value);
  __ompc_spin_unlock(&lock);
  return result;
}


#  else

static inline int
__ompc_atomic_inc(volatile int* value)
{
  int result;
  __asm__ __volatile__ ("fetchadd4.rel %0=[%1],1":"=r"(result):"r"(value));
  return result + 1;
}

static inline int
__ompc_atomic_dec(volatile int* value)
{
  int result;
  __asm__ __volatile__ ("fetchadd4.rel %0=[%1],-1":"=r"(result):"r"(value));
  return result - 1;
}

static inline void
__ompc_spin_lock(volatile int* lock)
{
  int result = 1;
  do {
    __asm__ __volatile__ ("xchg4 %0=[%1],%2":"=r"(result):"r"(lock),"0"(result));
  } while (result);
}

static inline void
__ompc_spin_unlock(volatile int* lock)
{
  *lock = 0;
}

#  endif

#endif
