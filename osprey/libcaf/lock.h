/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2012-2013 University of Houston.

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

#ifndef LOCK_H
#define LOCK_H

/* lock structure */
/* Note: This limits us to 1M images, and 64 GB heap size per
 * image. */

struct coarray_lock {
    volatile char locked:8;
    volatile int image:20;
    volatile long long ofst:36;
};
typedef struct coarray_lock lock_t;


void comm_lock(lock_t * lock, int image, char *errmsg, int errmsg_len);
void comm_lock_stat(lock_t * lock, int image, char *success,
                    int success_len, int *status, int stat_len,
                    char *errmsg, int errmsg_len);

void comm_unlock(lock_t * lock, int image, char *errmsg, int errmsg_len);
void comm_unlock_stat(lock_t * lock, int image, int *status,
                      int stat_len, char *errmsg, int errmsg_len);

void comm_unlock2(lock_t * lock, int image, char *errmsg, int errmsg_len);
void comm_unlock2_stat(lock_t * lock, int image, int *status,
                       int stat_len, char *errmsg, int errmsg_len);

#endif
