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
 * File: omp_util.c
 * Abstract: some utilities of the OpenMP Runtime Library
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 * 
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <ctype.h>
#include <unistd.h>
#include "omp_util.h"

void
Not_Valid (char *error_message)
{
  fprintf(stderr, error_message);
  fprintf(stderr, "\n");
  fflush(stderr);
  abort();
}

void
Warning (char *warning_message)
{
  fprintf(stderr, "Warning: %s\n", warning_message);
  fflush(stderr);
}

char *
Trim_Leading_Spaces (char *input)
{
  char *output = input;
  DevWarning( output != NULL, ("input string is NULL!"));

  while ((*output != '\0') && isspace((int)(*output))) {
    output++;
  }

  return output;
}

unsigned long int
Get_System_Stack_Limit (void)
{
  struct rlimit cur_rlimit;
  int return_value;
  return_value = getrlimit(RLIMIT_STACK, &cur_rlimit);
  Is_True(return_value == 0, ("user stack limit setting wrong"));
  return cur_rlimit.rlim_cur;
}

int
Get_SMP_CPU_num (void)
{
  /* Maybe we still want to check it's supported by underlying system? */
  return (int) sysconf(_SC_NPROCESSORS_ONLN);
}

void
__ompc_do_nothing (void)
{
}

