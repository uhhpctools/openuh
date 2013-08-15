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
