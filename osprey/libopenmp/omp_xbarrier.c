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


#include "omp_rtl.h"
#include "omp_xbarrier.h"

omp_xbarrier_t __omp_xbarrier_type;
void (*__ompc_xbarrier_wait)(omp_team_t *team);

extern inline void __ompc_barrier_wait(omp_team_t *team);

void __ompc_xbarrier_linear_wait(omp_team_t *team);
void __ompc_xbarrier_simple_wait(omp_team_t *team);
void __ompc_xbarrier_binary_wait(omp_team_t *team);
void __ompc_xbarrier_dissem_wait(omp_team_t *team);
void __ompc_xbarrier_tour_wait(omp_team_t *team);
void __ompc_xbarrier_tree_wait(omp_team_t *team);

void tour_xbarrier_init(int vpid, omp_round_t **myrounds, omp_team_t *team);
void tree_xbarrier_init(int vpid, omp_treenode_t **mynode, omp_team_t *team);

void __ompc_set_xbarrier_wait()
{
  switch (__omp_xbarrier_type) {
    case DISSEM_XBARRIER:
      __ompc_xbarrier_wait = &__ompc_xbarrier_dissem_wait;
      break;
    case TOUR_XBARRIER:
      __ompc_xbarrier_wait = &__ompc_xbarrier_tour_wait;
      break;
    case TREE_XBARRIER:
      __ompc_xbarrier_wait = &__ompc_xbarrier_tree_wait;
      break;
    case SIMPLE_XBARRIER:
      __ompc_xbarrier_wait = &__ompc_xbarrier_simple_wait;
      break;
    default:
      __ompc_xbarrier_wait = &__ompc_barrier_wait;
      break;
  }
}

void __ompc_xbarrier_info_init(omp_team_t *team)
{
  int i,j;
  omp_xbarrier_info_t *info;
  int team_size, log2_team_size;

  team_size = team->team_size;
  log2_team_size = team->log2_team_size;

  info = &(team->xbarrier_info);

  switch(__omp_xbarrier_type) {
    case LINEAR_XBARRIER:
    case SIMPLE_XBARRIER:
      break;
    case TOUR_XBARRIER:
      if (team_size == 1) {
        info->rounds = NULL;
      } else {
        for(i = 0;i < team_size; i++) {
          memset(info->rounds[i], 0, sizeof(omp_round_t) * log2_team_size);
        }
      }
      break;
    case TREE_XBARRIER:
      memset(info->shared_array, 0, sizeof(omp_treenode_t) * team_size);
      break;
    case DISSEM_XBARRIER:
      if (team_size == 1) {
        info->nodes = NULL;
      } else {
        for (i = 0; i < team_size ; i++) {
          int d = 1;
          for (j = 0; j < log2_team_size; j++) {
            info->nodes[i][j].flag[0] = False;
            info->nodes[i][j].flag[1] = False;
            info->nodes[i][j].partner =
              &(info->nodes[(i+d) % team_size][j]);
            d = 2*d;
          }
        }
      }
      break;
  }
}

void __ompc_xbarrier_info_create(omp_team_t *team)
{
  int i,j;
  omp_xbarrier_info_t *info;
  int team_size, log2_team_size;

  team_size = team->team_size;
  log2_team_size = team->log2_team_size;

  info = &(team->xbarrier_info);

  switch(__omp_xbarrier_type) {
    case LINEAR_XBARRIER:
    case SIMPLE_XBARRIER:
      break;
    case TOUR_XBARRIER:
      if (team_size == 1) {
        info->rounds = NULL;
      } else {
        info->rounds =
          (omp_round_t **) malloc(sizeof(omp_round_t *) * team_size);
        for(i = 0;i < team_size; i++) {
          info->rounds[i] =
            (omp_round_t *) aligned_malloc(sizeof(omp_round_t) * log2_team_size,
                                       CACHE_LINE_SIZE);
          memset(info->rounds[i], 0, sizeof(omp_round_t) * log2_team_size);
        }
      }
      break;
    case TREE_XBARRIER:
      info->shared_array =
        (omp_treenode_t *) aligned_malloc(sizeof(omp_treenode_t ) * team_size,
                                    CACHE_LINE_SIZE );
      memset(info->shared_array, 0, sizeof(omp_treenode_t) * team_size);
      break;
    case DISSEM_XBARRIER:
      if (team_size == 1) {
        info->nodes = NULL;
      } else {
        info->nodes =
          (omp_localnode_t **) malloc(sizeof(omp_localnode_t *) * team_size );
        for(i = 0;i < team_size; i++) {
          info->nodes[i] = (omp_localnode_t *)
                          aligned_malloc(sizeof(omp_localnode_t) * log2_team_size,
                                      CACHE_LINE_SIZE);
        }

        for (i = 0; i < team_size ; i++) {
          int d = 1;
          for (j = 0; j < log2_team_size; j++) {
            info->nodes[i][j].flag[0] = False;
            info->nodes[i][j].flag[1] = False;
            info->nodes[i][j].partner =
              &(info->nodes[(i+d) % team_size][j]);
            d = 2*d;
          }
        }
      }
      break;
  }
}

void __ompc_xbarrier_info_destroy(omp_team_t *team)
{
  int i,j,k;
  omp_xbarrier_info_t info;
  int team_size;

  team_size = team->team_size;
  info = team->xbarrier_info;

  switch(__omp_xbarrier_type) {
    case LINEAR_XBARRIER:
    case SIMPLE_XBARRIER:
      break;
    case TOUR_XBARRIER:
      if (team_size > 1) {
        for(i = 0;i < team_size; i++) {
          aligned_free(info.rounds[i]);
        }
        free(info.rounds);
      }
      break;
    case TREE_XBARRIER:
      aligned_free(info.shared_array);
      break;
    case DISSEM_XBARRIER:
      if (team_size > 1) {
        for(i = 0; i < team_size; i++) {
          aligned_free(info.nodes[i]);
        }
        free(info.nodes);
      }
      break;
  }
}

void __ompc_init_xbarrier_local_info( omp_xbarrier_local_info_t *local,
                                      int vpid, omp_team_t *team)
{
  Is_True(local != NULL,
      ("__ompc_init_xbarrier_local_info: local info is NULL"));

  local->parity = 0;
  local->sense = True;

  if (__omp_xbarrier_type == TREE_XBARRIER) {
     tree_xbarrier_init(vpid, &(local->u.mynode), team);
  } else if (__omp_xbarrier_type == TOUR_XBARRIER) {
     tour_xbarrier_init(vpid, &(local->u.myrounds), team);
  }
}



void tree_xbarrier_init(int vpid, omp_treenode_t **mynode, omp_team_t *team)
{
  int i,child_id;
  int team_size;
  omp_xbarrier_info_t team_xbarrier_info;

  team_xbarrier_info = team->xbarrier_info;
  team_size  = team->team_size;

  if (team_size == 1) return;

  *mynode = &(team_xbarrier_info.shared_array[vpid]);

  if (vpid == 0) {
    (*mynode)->parentflag = &(*mynode)->dummy;
  } else {
    int parentid = (vpid -1)/4;
    int my_index = vpid - (parentid * 4) - 1;
    (*mynode)->parentflag =
      &(team_xbarrier_info.shared_array[parentid].childnotready.parts[my_index]);
  }

  for (i = 0, child_id = 2*vpid+1; i < 2; i++, child_id++) {
    if (team_size > child_id)  /* have child i in wakeup tree */
      (*mynode)->child_notify[i] =
        &(team_xbarrier_info.shared_array[child_id].wakeup_sense);
    else  /* don't have child i in wakeup tree */
      (*mynode)->child_notify[i] = &(*mynode)->dummy;
  }
  for(i = 0, child_id = 4*vpid+1; i < 4; i++, child_id++) {
         /* have child i in arrival tree */
    (*mynode)->havechild.parts[i] = (team_size > child_id);
    (*mynode)->childnotready.whole = (*mynode)->havechild.whole;
    (*mynode)->wakeup_sense = False;
  }
}

void tour_xbarrier_init(int vpid, omp_round_t **myrounds, omp_team_t *team)
{
  int k;
  int team_size, log2_team_size;
  omp_xbarrier_info_t team_xbarrier_info;

  team_xbarrier_info = team->xbarrier_info;
  team_size  = team->team_size;
  log2_team_size  = team->log2_team_size;

  if (team_size == 1) return;

  (*myrounds) = &(team_xbarrier_info.rounds[vpid][0]);

  for(k = 0; k < log2_team_size; k++)
    (*myrounds)[k].role = DROPOUT;

  for(k = 0; k < log2_team_size; k++) {
     int vpid_mod_2_sup_k_plus_1 = vpid % (1 << (k+1));
     if (vpid_mod_2_sup_k_plus_1 == 0) {
        int partner = vpid + (1 << k);
        if (partner < team_size) (*myrounds)[k].role = WINNER;
        else (*myrounds)[k].role = NOOP;
        (*myrounds)[k].opponent = (boolean *) NULL;
     }
     else if (vpid_mod_2_sup_k_plus_1 == (1 << k)) {
        (*myrounds)[k].role = LOSER;
        (*myrounds)[k].opponent =
          &(team_xbarrier_info.rounds[vpid - (1<<k)][k].flag);
         break;
     }
  }

  if (vpid == 0)
    (*myrounds)[k-1].role = CHAMPION;
  for(k = 0; k < log2_team_size; k++)
    (*myrounds)[k].flag = False;
}


/* Implementation of different algorithms */

void __ompc_xbarrier_simple_wait(omp_team_t *team)
{
  int barrier_flag;
  int new_count;

  if (team->team_size == 1)
    return;

  barrier_flag = team->barrier_flag;
  new_count = __ompc_atomic_inc(&team->barrier_count);

  if (new_count == team->team_size) {
    /* The last one reset flags*/
    team->barrier_count = 0;
    team->barrier_flag = barrier_flag ^ 1; /* Xor: toggle*/
  }
  else {
    while (team->barrier_flag == barrier_flag) {
      /* keep checking */
    }
  }
}

void __ompc_xbarrier_dissem_wait(omp_team_t *team)
{
  int thread_id, r, d ;
  int log2_team_size;
  omp_xbarrier_local_info_t *xbarrier_local;
  omp_localnode_t **nodes;

  if (team->team_size == 1)
    return;

  thread_id = __omp_myid;
  log2_team_size = team->log2_team_size;
  xbarrier_local = &(__omp_current_v_thread->xbarrier_local);
  nodes = team->xbarrier_info.nodes;
  d = 1;

  for (r = 0; r < log2_team_size; r++) {
    nodes[thread_id][r].partner->flag[xbarrier_local->parity] =
      xbarrier_local->sense;
    while (nodes[thread_id][r].flag[xbarrier_local->parity] !=
           xbarrier_local->sense) {
      /* keep checking */
    }
    d = 2*d;
  }

  if (xbarrier_local->parity == 1)
    xbarrier_local->sense = xbarrier_local->sense ^ 1;
  xbarrier_local->parity = 1 - xbarrier_local->parity;
}

volatile boolean champion_sense = False;

void __ompc_xbarrier_tour_wait(omp_team_t *team)
{
  int thread_id;
  omp_xbarrier_local_info_t *xbarrier_local;
  omp_round_t *round;

  if (team->team_size == 1)
    return;

  thread_id = __omp_myid;
  xbarrier_local = &(__omp_current_v_thread->xbarrier_local);
  round = xbarrier_local->u.myrounds;
  
  for(;;) {
     if(round->role & LOSER) {
       *(round->opponent) = xbarrier_local->sense;
       while (champion_sense != xbarrier_local->sense) {
         /* keep checking */
       }
       break;
     }
     else if(round->role & WINNER) {
       while (round->flag != xbarrier_local->sense) {
         /* keep checking */
       }
       /* continue */
     } else if (round->role & CHAMPION) {
       while (round->flag != xbarrier_local->sense) {
         /* keep checking */
       }
       champion_sense = xbarrier_local->sense;
       break;
     }
     round++;
  }
  xbarrier_local->sense ^= True;
}

void __ompc_xbarrier_tree_wait(omp_team_t *team)
{
  int thread_id;
  omp_xbarrier_local_info_t *xbarrier_local;
  omp_treenode_t *mynode_reg;

  if (team->team_size == 1)
    return;

  thread_id = __omp_myid;
  xbarrier_local = &(__omp_current_v_thread->xbarrier_local);
  mynode_reg = xbarrier_local->u.mynode;

  while (mynode_reg->childnotready.whole) {
    /* keep checking */
  }

  mynode_reg->childnotready.whole = mynode_reg->havechild.whole;
  *(mynode_reg->parentflag) = False;

  if (thread_id != 0)
      while (mynode_reg->wakeup_sense != xbarrier_local->sense) {
        /* keep checking */
      }

  *mynode_reg->child_notify[0] = xbarrier_local->sense;
  *mynode_reg->child_notify[1] = xbarrier_local->sense;
  xbarrier_local->sense ^= True;
}

#if 0
/* Function to select a barrier algorithm based on BARRIER_TYPE */
void __ompc_barrier_wait_select(omp_team_t *team, int needevent)
{
  if(needevent) {
    __ompc_set_state(THR_IBAR_STATE);
    __ompc_event_callback(OMP_EVENT_THR_BEGIN_IBAR);
  }

  (*barrier_func_ptr)(team);

  if(needevent) {
    __ompc_event_callback(OMP_EVENT_THR_END_IBAR);
    __ompc_set_state(THR_WORK_STATE);
  }
}
#endif
