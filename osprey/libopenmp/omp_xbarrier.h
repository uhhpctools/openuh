/*
 Extended Barrier Implementation for OpenUH's OpenMP runtime library

 Copyright (C) 2014 University of Houston.

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


#ifndef __OMP_XBARRIER_H__
#define __OMP_XBARRIER_H__

#include <string.h>
#include "omp_sys.h"
#include "omp_lock.h"

/* Global structures for barrier */ 
typedef unsigned char boolean;
#define False ((boolean) 0)
#define True ((boolean) 1)
typedef unsigned char role_enum;
#define LOSER ((role_enum) 1)
#define WINNER ((role_enum) 2)
#define CHAMPION ((role_enum) 4)
#define NOOP ((role_enum) 8)
#define DROPOUT ((role_enum) 16)

typedef enum {
  LINEAR_XBARRIER = 0,
  SIMPLE_XBARRIER,
  TOUR_XBARRIER,
  TREE_XBARRIER,
  DISSEM_XBARRIER
} omp_xbarrier_t;

/* dissemination barrier */
struct localnode {
  volatile boolean flag[2];
  struct localnode *partner;
} __attribute__ ((__aligned__(CACHE_LINE_SIZE)));
typedef struct localnode omp_localnode_t;

/* tree barrier */
typedef union {
  volatile long whole;
  boolean parts[4];
} whole_and_parts;

struct treenode {
  volatile boolean *parentflag;
  volatile boolean *child_notify[2];
  whole_and_parts havechild;
  whole_and_parts childnotready;
  volatile boolean wakeup_sense;
  boolean dummy;
  int pad[4];
} __attribute__ ((__aligned__(CACHE_LINE_SIZE)));
typedef struct treenode omp_treenode_t;

/* tournament barrier */
struct round_t {
  volatile boolean *opponent;
  role_enum role;
  volatile boolean flag;
} __attribute__ ((__aligned__(CACHE_LINE_SIZE)));
typedef struct round_t omp_round_t;

typedef union {
  omp_localnode_t **nodes;      /* dissemination barrier shared data */
  omp_treenode_t *shared_array;  /* tree barrier shared data */
  omp_round_t **rounds;       /* tournament barrier shared data */
} omp_xbarrier_info_t;


struct omp_xbarrier_local_info {
  volatile int parity;             /* any barrier */
  volatile boolean sense;          /* any barrier */
  union {
  omp_treenode_t *mynode;     /* tree barrier */
  omp_round_t  *myrounds;   /* tournament barrier */
  } u;
}; // __attribute__ ((__aligned__(CACHE_LINE_SIZE)));
typedef struct omp_xbarrier_local_info omp_xbarrier_local_info_t;

/* global variables extern declarations */
extern omp_xbarrier_t __omp_xbarrier_type;
extern long int __omp_spin_count; // defined in omp_thread.c

extern void __ompc_set_xbarrier_wait();


// extern void __ompc_xbarrier_select(int needevent);


#endif  /* __OMP_XBARRIER_H */

