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


/* TODO: Tracing
 */

#if 0

#include "errors.h"
#include "mfmc.h"
#include "mfmc_defs.h"

MFMC_HANDLE
MFMC_Init_problem(MEM_POOL *mem_pool,
		  BOOL      trace,
		  INT32     n,
		  INT32     m,
		  INT32     n_sources,
		  INT32     n_sinks)
{
  INT32 i;

  /* allocating memory for  'nodes', 'arcs'  and internal arrays */
  MFMC_HANDLE handle = (MFMC_HANDLE) MEM_POOL_Alloc(mem_pool,
						    sizeof(*handle));

  if (handle == NULL) {
    return NULL;
  }

  handle->mem_pool = mem_pool;
  handle->trace = trace;

  handle->n = handle->user_n = n;
  handle->m = handle->user_m = m;

  handle->n_sources = n_sources;
  handle->n_sinks   = n_sinks;

  handle->pos_current = 0;

  if (n_sources > 1) {
    /* We will introduce a source. It will be nodes[handle->user_n].
     */
    handle->n++;
    handle->m += n_sources;
  }

  if (n_sinks > 1) {
    /* We will introduce a sink. It will be nodes[handle->n - 1].
     */
    handle->n++;
    handle->m += n_sinks;
  }

  /* Allocate memory that will live beyond the final query. These
   * resources will be freed by the user when s/he pops or deletes the
   * MEM_POOL.
   */
  handle->nodes     = TYPE_MEM_POOL_ALLOC_N(MFMC_NODE,
					    mem_pool,
					    handle->n + 2);
  handle->arcs      = TYPE_MEM_POOL_ALLOC_N(MFMC_ARC,
					    mem_pool,
					    2 * handle->m + 1);
  handle->acap      = TYPE_MEM_POOL_ALLOC_N(INT64,
					    mem_pool,
					    2 * handle->m);

  /* Allocate memory that will live through the end of the
   * push-relabel algorithm, but that need not stay around for the
   * first query.
   */
  MEM_POOL_Push(mem_pool);

  handle->queue     = TYPE_MEM_POOL_ALLOC_N(MFMC_NODE *,
					    mem_pool,
					    handle->n);
  handle->layers    = TYPE_MEM_POOL_ALLOC_N(MFMC_LAYER,
					    mem_pool,
					    handle->n + 2);

  /* Allocate memory that will live only as long as it takes us to set
   * up the problem instance.
   */
  MEM_POOL_Push(handle->mem_pool);

  handle->arc_tail  = TYPE_MEM_POOL_ALLOC_N(INT32,
					    mem_pool,
					    2 * handle->m);
  handle->arc_first = TYPE_MEM_POOL_ALLOC_N(INT32,
					    mem_pool,
					    handle->n + 2);

  if (handle->nodes == NULL || handle->arcs == NULL || 
      handle->arc_first == NULL || handle->arc_tail == NULL) {
    return NULL;
  }

  for (i = 0; i < n; i++) {
    handle->nodes[i].node_type = TRANSSHIPMENT;
  }

  for (i = 0; i < m; i++) {
    handle->arc_first[i] = 0;
  }

  if (n_sources > 1) {
    handle->the_source = &handle->nodes[handle->user_n];
  }
  else {
    handle->the_source = NULL;
  }

  if (n_sinks > 1) {
    handle->the_sink = &handle->nodes[handle->n - 1];
  }
  else {
    handle->the_sink = NULL;
  }

  /* setting pointer to the first arc */
  handle->arc_current = handle->arcs;

  handle->n_sources_set = 0;
  handle->n_sinks_set = 0;
  handle->max_cap = 0;

  return handle;
}

MFMC_EC
MFMC_Place_arc(MFMC_HANDLE      handle,
	       INT32            tail,
	       INT32            head,
	       INT64            lower_cap,
	       INT64            upper_cap,
	       MFMC_ARC_HANDLE *arc_handle)

{
  FmtAssert(tail >= 0 && tail < handle->n,
	    ("Tail node out of range"));
  FmtAssert(head >= 0 && head < handle->n,
	    ("Head node out of range"));

  if (upper_cap < lower_cap) {
    return MFMC_INFEASIBLE;
  }

  if (upper_cap < 0 || lower_cap > 0) {
    return MFMC_ZERO_INFEASIBLE;
  }

  /* For the sake of the following check, user_m gets modified so that
   * user_m == m, just prior to the construction of auxiliary source
   * and/or sink node(s).
   */
  if (handle->pos_current + 1 >= 2 * handle->user_m) {
    return MFMC_BAD_ARC_COUNT;
  }

  /* no of arcs incident to node i is stored in arc_first[i+1] */
  handle->arc_first[tail + 1] ++; 
  handle->arc_first[head + 1] ++;

  /* storing information about the arc */
  handle->arc_tail[handle->pos_current]   = tail;
  handle->arc_tail[handle->pos_current+1] = head;

  handle->arc_current->head = handle->nodes + head;
  handle->arc_current->r_cap = upper_cap;
  handle->arc_current->reverse = handle->arc_current + 1;

  (handle->arc_current + 1)->head = handle->nodes + tail;
  (handle->arc_current + 1)->r_cap = -lower_cap;
  (handle->arc_current + 1)->reverse = handle->arc_current;

  /* searching minimum and maximum node */
  handle->arc_current += 2;
  handle->pos_current += 2;

  if (upper_cap > handle->max_cap) {
    handle->max_cap = upper_cap;
  }
  if (-lower_cap > handle->max_cap) {
    handle->max_cap = -lower_cap;
  }

  if (arc_handle != NULL) {
    /* Arc handles not implemented yet */
    return MFMC_NOT_IMPLEMENTED;
  }

  return MFMC_NO_ERROR;
}

MFMC_EC
MFMC_Set_source(MFMC_HANDLE handle, INT32 s)
{
  FmtAssert(s >= 0 && s < handle->n,
	    ("Source node out of range"));

  if (handle->nodes[s].node_type == TRANSSHIPMENT) {
    if (handle->n_sources == 1) {
      handle->nodes[s].node_type = SOURCE;
      handle->the_source = &handle->nodes[s];
    }
    else {
      handle->nodes[s].node_type = USER_SOURCE;
      /* We cannot place the introduced arc now because we don't yet
       * know the largest capacity in the problem.
       */
    }
    handle->n_sources_set++;
    return MFMC_NO_ERROR;
  }
  else {
    return MFMC_S_T_OVERLAP;
  }
}

MFMC_EC
MFMC_Set_sink(MFMC_HANDLE handle, INT32 t)
{
  FmtAssert(t >= 0 && t < handle->n,
	    ("Sink node out of range"));

  if (handle->nodes[t].node_type != SOURCE) {
    if (handle->n_sinks == 1) {
      handle->nodes[t].node_type = SINK;
      handle->the_sink = &handle->nodes[t];
    }
    else {
      handle->nodes[t].node_type = USER_SINK;
      /* We cannot place the introduced arc now because we don't yet
       * know the largest capacity in the problem.
       */
    }
    handle->n_sinks_set++;
    return MFMC_NO_ERROR;
  }
  else {
    return MFMC_S_T_OVERLAP;
  }
}

/* User interface to code to solve the flow/cut problem. Included in
 * mfmc_setup.c because we do the tail end of setup here before
 * passing the problem off to the solver.
 */

MFMC_EC
MFMC_Solve_problem(MFMC_HANDLE handle)
{
  INT64		big_cap = handle->max_cap * handle->m;
  INT32		i;
  INT32		tail,
		last,
		arc_num,
		arc_new_num;
  INT64		cap;
  MFMC_ARC     *arc_current,
	       *arc_new,
	       *arc_tmp;
  MFMC_NODE    *head_p,
	       *ndp;

  if (handle->n_sources != handle->n_sources_set ||
      handle->n_sinks != handle->n_sinks_set) {
    return MFMC_UNSEEN_S_T;
  }

  if (handle->pos_current != 2 * handle->user_m) {
    return MFMC_BAD_ARC_COUNT;
  }

  /* The following cheat is to pacify the arc count check in
   * MFMC_Place_arc.
   */
  handle->user_m = handle->m;

  if (handle->n_sources > 1) {
    /* Place a high capacity arc out of the source to each of the
     * user's sources.
     */
    for (i = 0; i < handle->user_n; i++) {
      if (handle->nodes[i].node_type == USER_SOURCE) {
	MFMC_Place_arc(handle, handle->the_source - handle->nodes, i,
		       0, big_cap, NULL);
      }
    }
  }

  if (handle->n_sinks > 1) {
    /* Make an artificial sink and place a high capacity arc into it
     * from each of the user's sinks.
     */
    for (i = handle->user_n; i >= 0; i--) {
      if (handle->nodes[i].node_type == USER_SINK) {
	MFMC_Place_arc(handle, i, handle->the_sink - handle->nodes,
		       0, big_cap, NULL);
      }
    }
  }

  FmtAssert(handle->pos_current == 2 * handle->m,
	    ("Arc count mismatch in auxiliary source/sink placement"));

  /* Now that there are no more arcs to place, build the ordered arc
   * and list and connect the node list to it.
   */
  /********** ordering arcs - linear time algorithm ***********/

  /* first arc from the first node */
  handle->nodes[0].first = handle->arcs;

  /* before below loop arc_first[i+1] is the number of arcs outgoing
   * from i; after this loop arc_first[i] is the position of the first
   * outgoing from node i arcs after they would be ordered; this value
   * is transformed to pointer and written to node. 
   */
 
  for (i = 1; i <= handle->n + 1; i++) {
    handle->arc_first[i] += handle->arc_first[i - 1];
    handle->nodes[i].first = handle->arcs + handle->arc_first[i];
  }

  /* scanning all the nodes except the last */
  for ( i = 0; i < handle->n; i++) {
    last = handle->nodes[i + 1].first - handle->arcs;
    /* arcs outgoing from i must be sited
     * from position arc_first[i] to the position
     * equal to initial value of arc_first[i+1]-1
     */

    for (arc_num = handle->arc_first[i];
	 arc_num < last;
	 arc_num++) {
      tail = handle->arc_tail[arc_num];

      /* the arc no  arc_num  is not in place because arc sited here
	 must go out from i;
	 we'll put it to its place and continue this process
	 until an arc in this position would go out from i */

      while ( tail != i ) {
	arc_new_num  = handle->arc_first[tail];
	arc_current  = handle->arcs + arc_num;
	arc_new      = handle->arcs + arc_new_num;
	    
	/* arc_current must be sited in the position arc_new    
	   swapping these arcs:                                 */

	head_p = arc_new->head;
	arc_new->head = arc_current->head;
	arc_current->head = head_p;

	cap = arc_new->r_cap;
	arc_new->r_cap = arc_current->r_cap;
	arc_current->r_cap = cap;

	if (arc_new != arc_current->reverse) {
	  arc_tmp = arc_new->reverse;
	  arc_new->reverse = arc_current->reverse;
	  arc_current->reverse = arc_tmp;

	  (arc_current->reverse)->reverse = arc_current;
	  (arc_new->reverse)->reverse = arc_new;
	}

	handle->arc_tail[arc_num] = handle->arc_tail[arc_new_num];
	handle->arc_tail[arc_new_num] = tail;

	/* we increase arc_first[tail]  */
	handle->arc_first[tail] ++ ;

	tail = handle->arc_tail[arc_num];
      }
    }
    /* all arcs outgoing from  i  are in place */
  }

  /* -----------------------  arcs are ordered  ------------------------- */

  /*----------- constructing lists ---------------*/


  for (ndp = handle->nodes;
       ndp <= handle->nodes + handle->n;
       ndp++) {
    ndp->first = NULL;
  }

  /* Why do we maintain the "next" field for arcs? It would seem
   * always to be the case that (a->next == a + 1).
   */
  for (arc_current = handle->arcs + (2 * handle->m - 1);
       arc_current >= handle->arcs;
       arc_current--) {
    arc_num = arc_current - handle->arcs;
    handle->acap[arc_num] = arc_current->r_cap;
    tail = handle->arc_tail[arc_num];
    ndp = handle->nodes + tail;
    arc_current->next = ndp->first;
    ndp->first = arc_current;
  }

  /* Free the arc_first and arc_tail memory */
  MEM_POOL_Pop(handle->mem_pool);

  /* Now solve the problem. */
  MFMC_Find_max_flow_min_cut(handle->n,
			     handle->nodes,
			     handle->arcs,
			     handle->acap,
			     handle->the_source,
			     handle->the_sink,
			     handle->queue,
			     handle->layers,
			     big_cap,
			     &handle->stats,
			     &handle->flow_value);

  /* Free the queue and layers memory used internally by the algorithm */
  MEM_POOL_Pop(handle->mem_pool);

  return MFMC_NO_ERROR;
}

#endif
