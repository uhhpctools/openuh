/*
 CAF runtime library to be used with OpenUH

 Copyright (C) 2009-2012 University of Houston.

 This program is free software; you can redistribute it and/or modify it
 under the terms of version 2 of the GNU General Public License as
 published by the Free Software Foundation.

 This program is distributed in the hope that it would be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 Further, this software is distributed without any warranty that it is
 free of the rightful claim of any third person regarding infringement
 or the like.  Any license provided herein, whether implied or
 otherwise, applies only to this software file.  Patent licenses, if
 any, provided herein do not apply to combinations of this program with
 other software, or any other product whatsoever.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston MA 02111-1307, USA.

 Contact information:
 http://www.cs.uh.edu/~hpctools
*/

/* #defines are in the header file */

#include <stdio.h>
#include <pthread.h>
#include <error.h>

/*
 * for hi-res timer
 */

#ifndef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE 199309
#endif /* _POSIX_C_SOURCE */

#include "caf_rtl.h"
#include "trace.h"

extern unsigned long _this_image;
extern unsigned long _num_images;

static long delay = 1000L; /* ns */
static struct timespec delayspec;

static pthread_t thr;

static volatile int done = 0;

/*
 * does comms. service until told not to
 */

static void *
start_service (void *unused)
{
  do
    {
      comm_service();
      pthread_yield ();
      nanosleep (&delayspec, NULL); /* back off */
    }
  while (! done);
}

/*
 * start the servicer
 */

void
comm_service_init (void)
{
  int s;

  delayspec.tv_sec = (time_t) 0;
  delayspec.tv_nsec = delay;

  s = pthread_create (&thr, NULL, start_service, (void *) 0);
  if (s != 0) {
      LIBCAF_TRACE( LIBCAF_LOG_FATAL,
              "service thread creation failed (%s)", 
              strerror(s) );
      /* NOT REACHED */
  }

  LIBCAF_TRACE( LIBCAF_LOG_SERVICE, "started progress thread");
}

/*
 * stop the servicer
 */

void
comm_service_finalize (void)
{
  int s;

  done = 1;

  s = pthread_join (thr, NULL);
  if (s != 0) {
      LIBCAF_TRACE( LIBCAF_LOG_FATAL,
              "service thread termination failed (%s)", 
              strerror(s) );
      /* NOT REACHED */
  }

  LIBCAF_TRACE( LIBCAF_LOG_SERVICE, "stopped progress thread");
}
