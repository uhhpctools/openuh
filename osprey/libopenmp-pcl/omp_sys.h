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

/*
 * File: omp_sys.h
 * Abstract: architecture dependent routines
 * History: 06/20/2007, built by He Jiangzhou, Tsinghua Univ.
 * 
 */
#ifndef __omp_sys_included
#define __omp_sys_included

#if defined(TARG_X8664) || defined(TARG_IA32) || defined(TARG_LOONGSON)

//TODO: should use __sync_add_and_fetch(), but it's
// not yet implemented in open64.

static inline int
__ompc_atomic_inc(volatile int* value)
{
#ifdef OMP_USE_GCC_ATOMIC_BUILTINS
  return __sync_add_and_fetch(value, 1);
#else
  return (__sync_fetch_and_add(value,1)+1);
#endif
}


static inline int
__ompc_atomic_dec(volatile int* value)
{
#ifdef OMP_USE_GCC_ATOMIC_BUILTINS
  return __sync_add_and_fetch(value, -1);
#else
  return (__sync_fetch_and_add(value,-1)-1);
#endif
}

#else

/* under observation not sure if next function works */
static inline unsigned long
__ompc_atomic_inc_unsigned_long(volatile unsigned long*value)
{
  unsigned long result;
  __asm__ __volatile__ ("fetchadd8.rel %0=[%1],1":"=r"(result):"r"(value));
  return result +1;
}

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

static inline void __ompc_spin_lock_s(volatile int* lock)
{
  int result = 1;
  do
  {
    __asm__ __volatile__ ("xchg4 %0=[%1],%2":"=r"(result):"r"(lock),"0"(result));
  } while(result);
}


static inline void
__ompc_spin_unlock(volatile int* lock)
{
  *lock = 0;
}

static inline int
__ompc_spin_trylock(volatile int* lock)
{
  int result = 1;
  __asm__ __volatile__ ("xchg4 %0=[%1],%2":"=r"(result):"r"(lock),"0"(result));
  return (result==0);
}


#  endif

#endif
