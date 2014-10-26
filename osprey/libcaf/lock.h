/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2012-2014 University of Houston.

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
