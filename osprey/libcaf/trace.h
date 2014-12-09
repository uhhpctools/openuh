/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2011-2013 University of Houston.

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

#include <stdio.h>
#include "env.h"

#ifndef _TRACE_H
#define _TRACE_H

#ifndef TRACE

#define LIBCAF_TRACE_INIT()   {\
    if (_this_image == 1 && getenv(ENV_DTRACE) != NULL) \
       Warning("Tracing support is not enabled"); \
}

#define LIBCAF_TRACE(arg1, arg2, ...) ((void) 1)
#define START_TIMER() ((void) 1)
#define STOP_TIMER(arg1) ((void) 1)
#define CALLSITE_TRACE(level, f, ...)  f(__VA_ARGS__);
#define CALLSITE_TIMED_TRACE(level, timer, f, ...)  f(__VA_ARGS__);

#define LIBCAF_TRACE_SUSPEND() ((void) 1)
#define LIBCAF_TRACE_RESUME() ((void) 1)

#else

extern int trace_callstack_level;

#define LIBCAF_TRACE_INIT __libcaf_tracers_init
#define LIBCAF_TRACE(...) __libcaf_trace(drop_path(__FILE__), __func__, __LINE__, __VA_ARGS__)

#define START_TIMER  __start_timer
#define STOP_TIMER  __stop_timer

#define CALLSITE_TRACE(level, f, ...) \
    LIBCAF_TRACE(LIBCAF_LOG_##level, "ENTERING " #f);\
    trace_callstack_level++; \
    f(__VA_ARGS__); \
    trace_callstack_level--; \
    LIBCAF_TRACE(LIBCAF_LOG_##level, "LEFT " #f);

#define CALLSITE_TIMED_TRACE(level, timer, f, ...) \
    LIBCAF_TRACE(LIBCAF_LOG_##level, "ENTERING (timed) " #f);\
    trace_callstack_level++; \
    START_TIMER(); \
    f(__VA_ARGS__); \
    STOP_TIMER(timer); \
    LIBCAF_TRACE(LIBCAF_LOG_TIME, "(" #f ") " ); \
    trace_callstack_level--; \
    LIBCAF_TRACE(LIBCAF_LOG_##level, "LEFT " #f);

#define LIBCAF_TRACE_SUSPEND  uhcaf_dtrace_suspend
#define LIBCAF_TRACE_RESUME   uhcaf_dtrace_resume

typedef enum {
    LIBCAF_LOG_FATAL = 0,       /* unrecoverable problem */
    LIBCAF_LOG_DEBUG,           /* debugging information */
    LIBCAF_LOG_TIME,            /* timing information */
    LIBCAF_LOG_NOTICE,          /* serious, but non-fatal */
    LIBCAF_LOG_TIME_SUMMARY,    /* print accumulated time */
    LIBCAF_LOG_INIT,            /* during LIBCAF initialization */
    LIBCAF_LOG_EXIT,            /* during LIBCAF exit */
    LIBCAF_LOG_COMM,            /* communication operations */
    LIBCAF_LOG_MEMORY,          /* memory allocation/deallocation operations */
    LIBCAF_LOG_CACHE,           /* cache operationss */
    LIBCAF_LOG_SYNC,            /* synchronization */
    LIBCAF_LOG_COLLECTIVE,      /* collective operations */
    LIBCAF_LOG_TEAM,            /* team operations */
    LIBCAF_LOG_SERVICE,         /* show progress service */
    LIBCAF_LOG_MEMORY_SUMMARY,  /* print summary of memory usage */
    NUM_TRACERS = LIBCAF_LOG_MEMORY_SUMMARY + 1
} libcaf_trace_t;

typedef enum {
    START,
    STOP,
    PRINT,
    PRINT_ROLLUP
} __timer_start_stop_t;         /* time recording start or stop */

typedef enum {
    INIT,
    READ,
    WRITE,
    SYNC,
    DUMMY
} __timer_type_t;               /* type of timer */

extern char *drop_path(char *s);        /* from util.h */

extern void __libcaf_tracers_init(void);
extern void __libcaf_trace(const char *file, const char *func, int line,
                           libcaf_trace_t msg_type, char *fmt, ...);
extern int __trace_is_enabled(libcaf_trace_t level);
void __start_timer();
void __stop_timer(__timer_type_t type);

FILE *__trace_log_stream();

void uhcaf_dtrace_print_rma_segment(char *str, int len);

void uhcaf_dtrace_suspend();
void uhcaf_dtrace_resume();

#endif                          /* TRACE */

#endif                          /* _TRACE_H */
