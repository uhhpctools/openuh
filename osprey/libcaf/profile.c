/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2013 University of Houston.

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

#include <stdlib.h>
#include <stdio.h>
#include "utlist.h"
#include "caf_rtl.h"
#include "util.h"
#include "profile.h"


typedef struct rma_node_t {
    int rmaid;
    int target;
    struct rma_node_t *next, *prev;
} rma_node_t;

extern unsigned long _this_image;

rma_node_t *saved_store_rma_list = NULL;
rma_node_t *saved_load_rma_list = NULL;

int profiling_enabled = 0;
int in_rma_region = 0;
int rma_prof_rid = 0;

caf_prof_groups_t prof_groups = CAFPROF_DEFAULT;

const char* CAFPROF_GRP       = "CAF";
const char* CAFPROF_GRP_MEM   = "CAF_MEM";
const char* CAFPROF_GRP_COMM  = "CAF_COMM";
const char* CAFPROF_GRP_SYNC  = "CAF_SYNC";
const char* CAFPROF_GRP_COLL  = "CAF_COLL";

static int rmaid_cmp(rma_node_t * a, rma_node_t * b)
{
    return (a->rmaid - b->rmaid);
}


void profile_init()
{
    if (esd_open) {
        profiling_enabled = 1;
        esd_open();
    }
}

void profile_rma_store_begin(int proc, int nelem)
{
    if (!profiling_enabled || !(prof_groups & CAFPROF_PUT))
        return;
    if (in_rma_region)
        Error("profile_rma_store_begin called within active rma region");

    rma_prof_rid = rma_prof_rid + 1;

    elg_put_1ts(proc, rma_prof_rid, nelem);

    in_rma_region = 1;
}

void profile_rma_store_end(int proc)
{
    if (!profiling_enabled || !(prof_groups & CAFPROF_PUT))
        return;
    if (!in_rma_region)
        Error("profile_rma_load_end called outside of active rma region");

    elg_put_1te_remote(proc, rma_prof_rid);
    in_rma_region = 0;
}

void profile_rma_store(int proc, int nelem)
{
}


void profile_rma_load_begin(int proc, int nelem)
{
    if (!profiling_enabled || !(prof_groups & CAFPROF_GET))
        return;
    if (in_rma_region)
        Error("profile_rma_load_begin called within active rma region");

    rma_prof_rid = rma_prof_rid + 1;

    elg_get_1ts_remote(proc, rma_prof_rid, nelem);
    in_rma_region = 1;
}

void profile_rma_load_end(int proc)
{
    if (!profiling_enabled || !(prof_groups & CAFPROF_GET))
        return;
    if (!in_rma_region)
        Error("profile_rma_load_end called outside of active rma region");

    elg_get_1te(proc, rma_prof_rid);
    in_rma_region = 0;
}

void profile_rma_load(int proc, int nelem)
{
}

/* compiler and internal API */

void profile_rma_nbstore_end(int proc, int rid)
{
    rma_node_t *rma_node, tmp;
    if (!profiling_enabled || !(prof_groups & CAFPROF_PUT))
        return;

    elg_put_1te_remote(proc, rid);

    /* remove this RMA from the save list */
    tmp.rmaid = rid;
    rma_node = NULL;
    DL_SEARCH(saved_store_rma_list, rma_node, &tmp, rmaid_cmp);
    if (rma_node)
        DL_DELETE(saved_store_rma_list, rma_node);

}

void profile_save_nbstore(int proc)
{
    if (!profiling_enabled || !(prof_groups & CAFPROF_PUT))
        return;
    profile_save_nbstore_rmaid(proc, rma_prof_rid);
}

void profile_save_nbstore_rmaid(int proc, int rid)
{
    if (!profiling_enabled || !(prof_groups & CAFPROF_PUT))
        return;
    rma_node_t *new_rma;
    if ((new_rma = malloc(sizeof(*new_rma))) == NULL)
        Error("malloc failed to allocate memory");

    new_rma->rmaid = rid;
    new_rma->target = proc;

    DL_APPEND(saved_store_rma_list, new_rma);
}


void profile_rma_nbload_end(int proc, int rid)
{
    if (!profiling_enabled || !(prof_groups & CAFPROF_GET))
        return;
    rma_node_t *rma_node, tmp;

    elg_get_1te(proc, rid);

    /* remove this RMA from the save list */
    tmp.rmaid = rid;
    rma_node = NULL;
    DL_SEARCH(saved_load_rma_list, rma_node, &tmp, rmaid_cmp);
    if (rma_node)
        DL_DELETE(saved_load_rma_list, rma_node);

}

void profile_save_nbload(int proc)
{
    if (!profiling_enabled || !(prof_groups & CAFPROF_GET))
        return;
    profile_save_nbload_rmaid(proc, rma_prof_rid);
}

void profile_save_nbload_rmaid(int proc, int rid)
{
    if (!profiling_enabled || !(prof_groups & CAFPROF_GET))
        return;
    rma_node_t *new_rma;
    if ((new_rma = malloc(sizeof(*new_rma))) == NULL)
        Error("malloc failed to allocate memory");

    new_rma->rmaid = rid;
    new_rma->target = proc;

    DL_APPEND(saved_load_rma_list, new_rma);
}


void profile_rma_end_all()
{
    if (!profiling_enabled)
        return;
    profile_rma_end_all_nbstores();
    profile_rma_end_all_nbloads();
}

void profile_rma_end_all_nbstores()
{
    rma_node_t *rma_node, *tmp;
    if (!profiling_enabled || !(prof_groups & CAFPROF_PUT))
        return;

    DL_FOREACH_SAFE(saved_store_rma_list, rma_node, tmp) {
        elg_put_1te_remote(rma_node->target, rma_node->rmaid);
        DL_DELETE(saved_store_rma_list, rma_node);
    }
}

void profile_rma_end_all_nbstores_to_proc(int proc)
{
    rma_node_t *rma_node, *tmp;
    if (!profiling_enabled || !(prof_groups & CAFPROF_PUT))
        return;

    DL_FOREACH_SAFE(saved_store_rma_list, rma_node, tmp) {
        if (rma_node->target == proc) {
            elg_put_1te_remote(proc, rma_node->rmaid);
            DL_DELETE(saved_store_rma_list, rma_node);
        }
    }
}

void profile_rma_end_all_nbloads()
{
    rma_node_t *rma_node, *tmp;
    if (!profiling_enabled || !(prof_groups & CAFPROF_GET))
        return;

    DL_FOREACH_SAFE(saved_load_rma_list, rma_node, tmp) {
        elg_put_1te(rma_node->target, rma_node->rmaid);
        DL_DELETE(saved_load_rma_list, rma_node);
    }
}

void profile_rma_end_all_nbloads_to_proc(int proc)
{
    rma_node_t *rma_node, *tmp;
    if (!profiling_enabled || !(prof_groups & CAFPROF_GET))
        return;

    DL_FOREACH_SAFE(saved_load_rma_list, rma_node, tmp) {
        if (rma_node->target == proc) {
            elg_put_1te(proc, rma_node->rmaid);
            DL_DELETE(saved_load_rma_list, rma_node);
        }
    }
}


void profile_set_in_prof_region()
{
    in_rma_region = 1;
}

void profile_unset_in_prof_region()
{
    in_rma_region = 0;
}

#pragma weak uhcaf_profile_start_ = uhcaf_profile_start
void uhcaf_profile_start()
{
    profiling_enabled = 1;
}

#pragma weak uhcaf_profile_stop_ = uhcaf_profile_stop
void uhcaf_profile_stop()
{
    profiling_enabled = 0;
}

#pragma weak uhcaf_profile_set_events_ = uhcaf_profile_set_events
void uhcaf_profile_set_events(caf_prof_groups_t *groups)
{
    prof_groups = *groups;
}

#pragma weak uhcaf_profile_reset_events_ = uhcaf_profile_reset_events
void uhcaf_profile_reset_events()
{
    prof_groups = CAFPROF_DEFAULT;
}

#pragma weak uhcaf_profile_add_events_ = uhcaf_profile_add_events
void uhcaf_profile_add_events(caf_prof_groups_t *groups)
{
    prof_groups |= *groups;
}

#pragma weak uhcaf_profile_remove_events_ = uhcaf_profile_remove_events
void uhcaf_profile_remove_events(caf_prof_groups_t *groups)
{
    prof_groups &= ~(*groups);
}
