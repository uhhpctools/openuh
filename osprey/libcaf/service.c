/*
 *
 * Copyright (c) 2011-2013
 *   University of Houston System and Oak Ridge National Laboratory.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * o Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * o Neither the name of the University of Houston System, Oak Ridge
 *   National Laboratory nor the names of its contributors may be used to
 *   endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 This provides a service thread for ensuring progress. This has been adapted
 from the UH OpenSHMEM implementation.
*/

/* #defines are in the header file */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <error.h>
#include <string.h>
#include "caf_rtl.h"
#include "env.h"
#include "comm.h"
#include "trace.h"


/*
 * for hi-res timer
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309
#endif                          /* _POSIX_C_SOURCE */

extern unsigned long _this_image;
extern unsigned long _num_images;

static struct timespec delayspec;

static pthread_t thr;

static volatile int done = 0;

static int enable_progress_thread;
static size_t progress_thread_interval; /* ns */

extern void comm_service();

/*
 * does comms. service until told not to
 */

static void *start_service(void *unused)
{
    do {
        comm_service();
        pthread_yield();
        nanosleep(&delayspec, NULL);    /* back off */
    }
    while (!done);
}

/*
 * start the servicer
 */

void comm_service_init(void)
{
    int s;

    enable_progress_thread = get_env_flag(ENV_PROGRESS_THREAD,
                                          DEFAULT_ENABLE_PROGRESS_THREAD);

    /* don't spawn a progress thread here if the conduit is ibv or vapi, since
     * GASNet's receive thread (GASNET_RCV_THREAD) will be enabled.
     */
#if !defined(GASNET_CONDUIT_IBV) && !defined(GASNET_CONDUIT_VAPI)
    if (enable_progress_thread == 0)
#endif
        return;

    progress_thread_interval = get_env_size(ENV_PROGRESS_THREAD_INTERVAL,
                                            DEFAULT_PROGRESS_THREAD_INTERVAL);

    delayspec.tv_sec = (time_t) 0;
    delayspec.tv_nsec = progress_thread_interval;

    s = pthread_create(&thr, NULL, start_service, (void *) 0);
    if (s != 0) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "service thread creation failed (%s)", strerror(s));
        /* NOT REACHED */
    }

    LIBCAF_TRACE(LIBCAF_LOG_SERVICE, "started progress thread");
}

/*
 * stop the servicer
 */

void comm_service_finalize(void)
{
    int s;

    /* don't spawn a progress thread here if the conduit is ibv or vapi, since
     * GASNet's receive thread (GASNET_RCV_THREAD) will be enabled.
     */
#if !defined(GASNET_CONDUIT_IBV) && !defined(GASNET_CONDUIT_VAPI)
    if (enable_progress_thread == 0)
#endif
        return;

    done = 1;

    s = pthread_join(thr, NULL);
    if (s != 0) {
        LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                     "service thread termination failed (%s)",
                     strerror(s));
        /* NOT REACHED */
    }

    LIBCAF_TRACE(LIBCAF_LOG_SERVICE, "stopped progress thread");
}
