/*
 Runtime library for supporting Coarray Fortran 

 Copyright (C) 2011-2012 University of Houston.

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

void Warning (char *warning_message);
void Error (char *error_message);

/* debug utility functions */

extern void debug_print_array_int(char *name, int *arr, int n);
extern void debug_print_array_long(char *name, long *arr, int n);
#endif /* _UTIL_H */
