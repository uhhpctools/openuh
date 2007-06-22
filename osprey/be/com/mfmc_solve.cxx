/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

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

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/


/* ----------------------------------------------------------------------
 * This code adapted by Robert Kennedy from a Push-Relabel
 * implementation developed in 1994 at the Stanford University
 * department of Computer Science by Boris Cherkassky and Andrew
 * Goldberg (goldberg@cs.stanford.edu).
 *
 * The program implements the "highest level" node selection strategy
 * with the global relabeling and gap relabeling heuristics.
 * ----------------------------------------------------------------------
 */
#if 0

#include <sys/types.h>
#include <sys/times.h>

#include "mfmc_defs.h"

#define GLOB_UPDT_FREQ 1.0

/* global variables */

static INT32            n;                   /* number of nodes */
static MFMC_NODE       *nodes;               /* array of nodes */
static MFMC_ARC        *arcs;                /* array of arcs */
static MFMC_LAYER      *layers;              /* array of layers */
static INT64	       *cap;                 /* array of capacities */
static MFMC_NODE       *source;              /* origin */
static MFMC_NODE       *sink;                /* destination */
static MFMC_STATS      *stats;		     /* statistics */
static MFMC_NODE      **queue;               /* queue for storing nodes */
static MFMC_NODE      **q_read, **q_write;   /* queue pointers */

static INT32	       lmax;             /* maximal layer */
static INT32           lmax_push;        /* maximal layer with excess node */
static INT32           lmin_push;        /* minimal layer with excess node */

static INT64           biggest_flow;     /* upper bound on the flow value */
/*--- initialization */

/*********************************************************************/
/*                                                                   */
/* current processor time in seconds                                 */
/* difference between two calls is processor time spent by your code */
/* needs: <sys/types.h>, <sys/times.h>                               */
/* depends on compiler and OS                                        */
/*                                                                   */
/*********************************************************************/

float timer(void)
{
  struct tms hold;

  times(&hold);
  return (float) hold.tms_utime / 60.0;
}

static BOOL
pr_init(INT32        n_p,		/* number of nodes */
	MFMC_NODE   *nodes_p,		/* array of nodes */
	MFMC_ARC    *arcs_p,		/* array of arcs */
	INT64       *cap_p,		/* array of given capacities */
	MFMC_NODE   *source_p,		/* the source node */
	MFMC_NODE   *sink_p,		/* the sink node */
	MFMC_NODE  **queue_p,		/* the BFS node queue */
	MFMC_LAYER  *layers_p,		/* distance level structure */
	MFMC_STATS  *stats_p,		/* statistics block */
	INT64        flow_upper_bound)	/* flow value upper bound */

{
  MFMC_NODE  *i;        /* current node */

  n      = n_p;
  nodes  = nodes_p;
  arcs   = arcs_p;
  cap    = cap_p;
  source = source_p;
  sink   = sink_p;
  stats  = stats_p;
  queue  = queue_p;
  layers = layers_p;

  for (i = nodes; i < nodes + n; i++) {
    i->excess = 0;
  }

  source->excess = biggest_flow = flow_upper_bound;

  lmax = n - 1;

  return MFMC_NO_ERROR;
} /* end of initialization */


/*--- global rank update - breadth first search */

static void
def_ranks (void)

{
  MFMC_NODE  *i, *j, *jn;  /* current nodes */
  MFMC_ARC   *a;           /* current arc   */
  MFMC_LAYER *l;           /* current layer */
  INT32       j_rank;       /* rank of node j */

  stats->n_up++; /* statistics */

  /* initialization */
  for (i = nodes; i < nodes + n; i++) {
    i->rank = n;
  }

  sink->rank = 0;

  *queue = sink;

  for (l = layers; l <= layers + lmax; l++) {
    l->push_first   = NULL;
    l->trans_first  = NULL;
  }

  lmax = lmax_push = 0;
  lmin_push = n;

  /* breadth first search */
  for (q_read = queue, q_write = queue + 1; 
       q_read != q_write;
       q_read++) {
    /* scanning arcs incident to node i */
    i = *q_read;
    j_rank = i->rank + 1;

    for (a = i->first; a != NULL; a = a->next) {
      j = a->head;

      if (j->rank == n) {
	/* j is not labelled */
	if (a->reverse->r_cap > 0 ) {
	  /* arc (j, i) is not saturated */
    	  j->rank    = j_rank;
    	  j->current = j->first;

	  l = layers + j_rank;
	  if (j_rank > lmax) lmax = j_rank;

	  if (j->excess > 0) {
	    j->nl_next     = l->push_first;
	    l->push_first  = j;
	    if (j_rank > lmax_push) lmax_push = j_rank;
	    if (j_rank < lmin_push) lmin_push = j_rank;
	  }
	  else {
	    /* j->excess == 0 */
	    jn = l->trans_first;
	    j->nl_next     = jn;
	    if (jn != NULL) jn->nl_prev = j;
	    l->trans_first  = j; 
	    }

	  /* put j  to scanning queue */
    	  *q_write = j;
	  q_write++;
    	}
      }
    } /* node "i" is scanned */ 
  } /* end of scanning queue */
} /* end of global update */

/*--- removing excessive flow - second phase of PR-algorithm */

static void
prefl_to_flow (void)

{
  MFMC_NODE  *i,*j, *ir, *is, *it, *front;       /* current nodes */
  MFMC_ARC   *a;                                 /* current arc   */
  INT64       path_cap;                          /* path capacity */
  BOOL	      path, out_of_path;                 /* flags         */

  /* initialization */
  for (i = nodes + n; i >= nodes; i--) {
    i->current = i->first;
    i->nl_next = NULL;
  }

  /* removing flow from excessive nodes */
  for (i = nodes; i < nodes + n; i ++) {
    if (i->excess <= 0 || i == source || i == sink) continue;

    /* i - has positive excess */
    for (front = i; i->excess > 0;) {
      /* looking for path from i to the source
	 (cycle may occur) */

      while (front != source) {
	for (a = front->current; ; a = a->next) {
	  if (cap[a - arcs] == 0 &&
	      a->r_cap > 0)
		   break;
	}
	front->current = a;

	j = a->head;

	front->nl_next = j;
	front = j;

	if (j->nl_next != NULL)
	  /* cycle is found */
	  break;
      } /* while (front != source) */

      /* either path from i to the source or cycle is found */

      if (front == source) {
	path = TRUE;
	is = i;
	path_cap = i->excess;
      }
      else {
	path = FALSE;
	is = j;
	path_cap = biggest_flow;
      }

      /* finding capacity of the path (cycle) */

      front = ir = is;

      do {
	a = ir->current;

	if (a->r_cap < path_cap) {
	  front    = ir;
	  path_cap =  a ->r_cap;
	}
	ir = ir->nl_next;

      } while (ir != j);

      if (path) i->excess -= path_cap;

      /* reducing flows along the path */
	
      ir = is;
      out_of_path = 0;
	
      do {
	a = ir->current;
	a->r_cap -= path_cap;
	a->reverse->r_cap += path_cap;

	it = ir->nl_next;

	if (ir == front) out_of_path = 1;
	if (out_of_path) ir->nl_next = NULL;
	    
	ir = it;

      } while (ir != j);

    } /* now excess of i is 0 */
  }
} /* end of prefl_to_flow */

/*--- cleaning beyond the gap */

static BOOL
gap (MFMC_LAYER *le)	/* pointer to the empty layer */

{
  MFMC_LAYER *l;          /* current layer */
  MFMC_NODE  *i;          /* current nodes */
  INT32	      r;          /* rank of the layer before l  */
  BOOL        cc;         /* cc = TRUE - no nodes with positive excess before
			     the gap */

  stats->n_gap++; /* statistics */

  r = (le - layers) - 1;

  /* putting ranks beyond the gap to "infinity" */

  for (l = le + 1; l <= layers + lmax; l++) {
    for (i = l->push_first; i != NULL; i = i->nl_next) {
      i->rank = n;
      stats->n_gnode ++; /* statistics */     
    }

    for (i = l->trans_first; i != NULL; i = i->nl_next) {
      i->rank = n;
      stats->n_gnode ++; /* statistics */     
    }

    l->push_first = l->trans_first = NULL;
  }

  cc = (lmin_push > r) ? TRUE : FALSE;

  lmax = r;
  lmax_push = r;

  return cc;
} /* end of gap */


/*--- pushing flow from node  i  */

static BOOL
push (MFMC_NODE *i)

{
  MFMC_NODE  *j;                /* sucsessor of i */
  MFMC_NODE  *j_next, *j_prev;  /* j's sucsessor and predecessor in layer list */
  INT32       j_rank;           /* rank of the next layer */
  MFMC_LAYER *lj;               /* j's layer */
  MFMC_ARC   *a;                /* current arc (i,j) */
  INT64	      fl;               /* flow to push through the arc */

  j_rank = i->rank - 1;

  /* scanning arcs outgoing from  i  */

  for (a = i->current; a != NULL; a = a->next) {
    if (a->r_cap > 0) {
      /* "a" is not saturated */
      
      j = a->head;
      if (j->rank == j_rank) {
	/* j belongs to the next layer */

	fl = MIN(i->excess, a->r_cap);

	a->r_cap -= fl;
	a->reverse->r_cap += fl;
	stats->n_push ++; /* statistics */

	if (j_rank > 0) {
	  lj = layers + j_rank;

	  if (j->excess == 0) {
	    /* before current push  j  had zero excess */

	    /* remove  j  from the list of transit nodes */
	    j_next = j->nl_next;
		
	    if (lj->trans_first == j)
	      /* j  starts the list */
	      lj->trans_first = j_next;
	    else {
	      /* j  is not the first */
	      j_prev = j->nl_prev;
	      j_prev->nl_next = j_next;
	      if (j_next != NULL)
		j_next->nl_prev = j_prev;
	    }

	    /* put  j  to the push-list */
	    j->nl_next = lj->push_first;
	    lj->push_first = j;

	    if (j_rank < lmin_push)
	      lmin_push = j_rank;
	  } /* j->excess == 0 */
	} /* j->rank > 0 */

	j->excess += fl;
	i->excess -= fl;

	if (i->excess == 0) break;

      } /* j belongs to the next layer */
    } /* a  is not saturated */
  } /* end of scanning arcs from  i */

  i->current = a;

  return a == NULL;
} /* end of push */

/*--- relabeling node i */

static INT32
relabel (MFMC_NODE *i)

{
  MFMC_NODE  *j;        /* sucsessor of i */
  INT32       j_rank;   /* minimal rank of a node available from j */
  MFMC_ARC   *a;        /* current arc */
  MFMC_ARC   *a_j;      /* an arc which leads to the node with minimal rank */
  MFMC_LAYER *l;        /* layer for node i */

  stats->n_rel++; /* statistics */

  i->rank = j_rank = n;

  /* looking for a node with minimal rank available from i */

  for (a = i->first; a != NULL; a = a->next) {
    if (a->r_cap > 0) {
      j = a->head;

	if (j->rank < j_rank)
	  {
	    j_rank = j->rank;
	    a_j    = a;
	  }
      }
  }
      
  if (j_rank < n) {
    i->rank = ++j_rank;
    i->current = a_j;

    l = layers + j_rank;

    if (i->excess > 0) {
      i->nl_next = l->push_first;
      l->push_first = i;
      if (j_rank > lmax_push) lmax_push = j_rank;
      if (j_rank < lmin_push) lmin_push = j_rank;
    }
    else {
      j = l->trans_first;
      i->nl_next = j;
      if (j != 0) j->nl_prev = i;
      l->trans_first = i;
    }

    if (j_rank > lmax) lmax = j_rank;
  } /* end of j_rank < n */
      
  return j_rank;
} /* end of relabel */


/* entry point */

BOOL
MFMC_Find_max_flow_min_cut(INT32        n_p,
			   MFMC_NODE   *nodes_p,
			   MFMC_ARC    *arcs_p,
			   INT64       *cap_p,
			   MFMC_NODE   *source_p,
			   MFMC_NODE   *sink_p,
			   MFMC_NODE  **queue_p,
			   MFMC_LAYER  *layers_p,
			   INT64        flow_upper_bound,
			   MFMC_STATS  *stats_p,
			   INT64        *fl)

{
  MFMC_NODE   *i;           /* current node */
  MFMC_NODE   *j;           /* i-sucsessor in layer list */
  INT32        i_rank;      /* rank of  i */
  MFMC_LAYER  *l;           /* current layer */
  INT32        n_r;         /* the number of recent relabels */
  BOOL         cc;          /* condition code */
  MFMC_STATS   stat_buf;    /* local buffer in case user doesn't care */

  if (stats_p != NULL) {
    stats_p->time = timer();
  }
  cc = pr_init(n_p, nodes_p, arcs_p, cap_p, source_p, sink_p,
	       queue_p, layers_p,
	       (stats_p != NULL ? stats_p : &stat_buf),
	       flow_upper_bound);

  if (cc != MFMC_NO_ERROR) return cc;

  def_ranks();

  n_r = 0;

  /* highest level method */

  while (lmax_push >= lmin_push) {
    /* main loop */
    l = layers + lmax_push;

    i = l->push_first;

    if (i == NULL) {
      /* nothing to push from this level */ 
      lmax_push --;
    }
    else {
      l->push_first = i->nl_next;

      cc = push(i);

      if (cc) { /* i must be relabeled */

	relabel(i);
	n_r ++;

	if (l->push_first == NULL && 
	    l->trans_first == NULL) {
	  /* gap is found */
	  gap ( l );
	}

	/* checking the necessity of global update */
	if (n_r > GLOB_UPDT_FREQ * (float) n) {
	  /* it is time for global update */
	  def_ranks ();
	  n_r = 0;
	}
      }
      else { /* excess is pushed out of  i  */
	j = l->trans_first;
	i->nl_next = j;
	l->trans_first = i;
	if (j != NULL) j->nl_prev = i;
      }
    }
  } /* end of the main loop */
    
#if 1
  *fl = sink->excess;
#else
  *fl += sink->excess;
#endif

  prefl_to_flow();

  if (stats_p != NULL) {
    stats_p->time = timer() - stats_p->time;
  }

  return MFMC_NO_ERROR;
} /* end of constructing flow */

#endif
