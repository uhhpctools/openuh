/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2011-2014 University of Houston.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

 Contact information:
 http://www.cs.uh.edu/~hpctools
*/

#ifndef _UTIL_H
#define _UTIL_H

/* declared in osprey/libf/fio/xarg.c */
extern int f__xargc;
extern char **f__xargv;
extern void __f90_set_args(int argc, char **argv);

/* from osprey/libU77, assumes platform is __linux */
#define ARGC f__xargc
#define ARGV f__xargv

#define Warning(...)  __libcaf_warning(drop_path(__FILE__),__func__,__LINE__,__VA_ARGS__)
#define Error(...)    __libcaf_error(drop_path(__FILE__),__func__,__LINE__,__VA_ARGS__)

#if defined(CAFRT_DEBUG)
#define DEBUG_PRINT_ARR_INT(name,arr,n)  __libcaf_debug_print_array_int(name,arr,n)
#define DEBUG_PRINT_ARR_LONG(name,arr,n) __libcaf_debug_print_array_long(name,arr,n)
#else
#define DEBUG_PRINT_ARR_INT(name,arr,n)  ((void) 1)
#define DEBUG_PRINT_ARR_LONG(name,arr,n) ((void) 1)
#endif

void __libcaf_warning(const char *file, const char *func, int line,
                      char *warning_msg, ...);
void __libcaf_error(const char *file, const char *func, int line,
                    char *error_msg, ...);

/* file utils */
char *drop_path(char *s);

/* debug utility functions */

#if defined(CAFRT_DEBUG)
extern void uhcaf_debug_dope(DopeVectorType *dopev);
extern void __libcaf_debug_print_array_int(char *name, int *arr, int n);
extern void __libcaf_debug_print_array_long(char *name, long *arr, int n);
#endif

#endif                          /* _UTIL_H */
