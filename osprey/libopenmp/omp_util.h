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
 * File: omp_util.h
 * Abstract: some utilities of the OpenMP Runtime Library
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 * 
 */
#ifndef __omp_rtl_utility_included
#define __omp_rtl_utility_included
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <sched.h>

#define Is_True_On
/* Assertion stuff */
#ifdef Is_True_On
#define Is_True(Cond, ParmList)\
    ( Cond ? (void) 1 : \
    ( fprintf(stderr, "Assertion Failed at %s:%d ", __FILE__, __LINE__), \
      fprintf(stderr, ParmList), \
      fprintf(stderr, "\n"), \
      fflush(stderr), \
      abort(), 0))

#define DevWarning(Cond, ParmList)\
    ( Cond ? (void) 1 : \
    ( fprintf(stderr, "DevWaring: at %s: %d", __FILE__, __LINE__), \
      fprintf(stderr, ParmList), \
      fprintf(stderr, "\n"), \
      fflush(stderr), 0)) 

#define DebugLog(ParmList) \
	fprintf(stderr, "Debug Log at %s:%d", __FILE__, __LINE__); \
	fprintf(stderr, ParmList);
#else

#define Is_True(Cond, ParmList) ()
#define Dev_Warning(Cond, ParmList) ()
#define DebugLog(ParmList) ()

#endif
	
#define Is_Valid(Cond, ParmList)\
    ( Cond ? (void) 1 : \
    ( fprintf(stderr, "Invalid setting :"), \
      fprintf(stderr, ParmList), \
      fprintf(stderr, "\n"), \
      fflush(stderr), \
      abort(), 0))

void
Not_Valid (char * error_message) __attribute__ ((__noreturn__));

void
Warning (char * warning_message);

/* Waiting while condition is true */
#define MAX_COUNT 50000
#define OMPC_WAIT_WHILE(condition) \
      { \
          if (condition) { \
              int count = 0; \
              while (condition) { \
                   if (count > MAX_COUNT) { \
	                sleep(0); \
	                count = 0; \
                   } \
		   count++; \
              } \
          } \
      }

/* Hash stuff*/
#define UTHREAD_HASH_SIZE  0x100L
#define UTHREAD_HASH_MASK (UTHREAD_HASH_SIZE-1)
#define HASH_IDX(ID) ((int)((unsigned long int)(ID) & (UTHREAD_HASH_MASK)))

/* string stuff*/
char *
Trim_Leading_Spaces (char *);

/* system stuff*/
unsigned long int
Get_System_Stack_Limit(void);

int
Get_SMP_CPU_num(void);

void __ompc_do_nothing(void);
#endif
