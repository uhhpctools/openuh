/*
 Extended Barrier Implementation for OpenUH's OpenMP runtime library

 Copyright (C) 2011 University of Houston.

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

