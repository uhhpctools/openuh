/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2013 University of Houston.

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

#include <stdio.h>
#include "env.h"

#ifndef _PROFILE_H
#define _PROFILE_H


/* needs to be consistent with parameters declared in caf_profiles
 * (include/fort/caf-extra.caf) */
typedef enum caf_prof_groups {
     CAFPROF_STARTUP               = 0X00000001,
     CAFPROF_STOPPED               = 0X00000002,
     CAFPROF_COARRAY_ALLOC_DEALLOC = 0X00000004,
     CAFPROF_TARGET_ALLOC_DEALLOC  = 0X00000008,

     CAFPROF_LCB                   = 0X00000010,
     CAFPROF_GET                   = 0X00000020,
     CAFPROF_PUT                   = 0X00000040,
     CAFPROF_WAIT                  = 0X00000080,

     CAFPROF_SYNC_STATEMENTS       = 0X00000100,
     CAFPROF_MUTEX                 = 0X00000200,
     CAFPROF_EVENTS                = 0X00000400,
     CAFPROF_ATOMICS               = 0X00000800,

     CAFPROF_BCAST                 = 0X00001000,
     CAFPROF_REDUCE                = 0X00002000,

     CAFPROF_TEAM                  = 0X00010000,

     /* group collections */
     CAFPROF_NONE                  = 0x00000000,
     CAFPROF_MEM_ALLOC             = 0x0000001C,
     CAFPROF_ONESIDED_COMM         = 0X000000F0,
     CAFPROF_SYNC                  = 0X00000F80,
     CAFPROF_DEFAULT               = 0x00000FFF,
     CAFPROF_COLLECTIVES           = 0X00003000,
     CAFPROF_ALL                   = 0XFFFFFFFF
} caf_prof_groups_t;


#ifndef PCAF_INSTRUMENT

/* existence of esd_open means episode and epilog library should be available
 * */
extern void esd_open();
#pragma weak esd_open

#define PROFILE_REGION_ENTRY(rname,grp,rtype)                        ((void) 1)
#define PROFILE_FUNC_ENTRY(grp)                                      ((void) 1)

#define PROFILE_REGION_EXIT(rname)                                   ((void) 1)
#define PROFILE_FUNC_EXIT(grp)                                       ((void) 1)



#define PROFILE_INIT()                                               ((void) 1)

#define PROFILE_STATS_INIT()   {\
    if (_this_image == 1 && (esd_open != NULL || getenv(ENV_STATS) != NULL)) \
      Warning("Profiling support is not enabled"); \
}

#define PROFILE_STATS_DUMP()                                         ((void) 1)

#define PROFILE_RMA_STORE_STRIDED_BEGIN(proc, stride_levels, count)  ((void) 1)
#define PROFILE_RMA_STORE_BEGIN(proc, nelem)                         ((void) 1)
#define PROFILE_RMA_STORE_END(proc)                                  ((void) 1)
#define PROFILE_RMA_STORE_DEFERRED_END(proc)                         ((void) 1)
#define PROFILE_RMA_STORE(proc, nelem)                               ((void) 1)

#define PROFILE_RMA_LOAD_STRIDED_BEGIN(proc, stride_levels, count)   ((void) 1)
#define PROFILE_RMA_LOAD_BEGIN(proc, nelem)                          ((void) 1)
#define PROFILE_RMA_LOAD_END(proc)                                   ((void) 1)
#define PROFILE_RMA_LOAD_DEFERRED_END(proc)                          ((void) 1)
#define PROFILE_RMA_LOAD(proc, nelem)                                ((void) 1)

#define PROFILE_COMM_HANDLE_END(hdl)                                 ((void) 1)

#define PROFILE_RMA_END_ALL()                                        ((void) 1)
#define PROFILE_RMA_END_ALL_STORES()                                 ((void) 1)
#define PROFILE_RMA_END_ALL_STORES_TO_PROC(proc)                     ((void) 1)
#define PROFILE_RMA_END_ALL_LOADS()                                  ((void) 1)
#define PROFILE_RMA_END_ALL_LOADS_TO_PROC(proc)                      ((void) 1)

#define PROFILE_GET_CACHE_HIT(proc)                                  ((void) 1)
#define PROFILE_GET_CACHE_MISS(proc)                                 ((void) 1)
#define PROFILE_GET_CACHE_WRITE_THROUGH(proc)                        ((void) 1)

#else

/* EPIK Dummy Interfaces */

static int esd_def_file_(FILE* a1)
{
    /* do nothing */
    return 0;
}

static int esd_def_region_(const char* a1, int a2, int a3, int a4,
                    const char* a5, unsigned char a6)
{
    /* do nothing */
    return 0;
}

static void esd_enter_(int a1)
{
    /* do nothing */
}

static void esd_exit_(int a1)
{
    /* do nothing */
}


static void elg_put_1ts_(int a1, int a2, int a3)
{
    /* do nothing */
}

static void elg_put_1te_(int a1, int a2)
{
    /* do nothing */
}

static void elg_put_1te_remote_(int a1, int a2)
{
    /* do nothing */
}

static void elg_get_1ts_(int a1, int a2, int a3)
{
    /* do nothing */
}

static void elg_get_1te_remote_(int a1, int a2)
{
    /* do nothing */
}

static void elg_get_1te_(int a1, int a2)
{
    /* do nothing */
}

/* existence of esd_open means episode and epilog library should be available
 * */
#pragma weak esd_open
#pragma weak esd_def_file             = esd_def_file_
#pragma weak esd_def_region           = esd_def_region_
#pragma weak esd_enter                = esd_enter_
#pragma weak esd_exit                 = esd_exit_

#pragma weak elg_put_1ts              = elg_put_1ts_
#pragma weak elg_put_1te              = elg_put_1te_
#pragma weak elg_put_1te_remote       = elg_put_1te_remote_

#pragma weak elg_get_1ts              = elg_put_1ts_
#pragma weak elg_get_1ts_remote       = elg_get_1te_remote_
#pragma weak elg_get_1te              = elg_get_1te_

extern int in_rma_region;
extern int rma_prof_rid;
extern int profiling_enabled;
extern int epik_enabled;

extern caf_prof_groups_t prof_groups;

extern const char *CAFPROF_GRP;
extern const char *CAFPROF_GRP_MEM;
extern const char *CAFPROF_GRP_COMM;
extern const char *CAFPROF_GRP_SYNC;
extern const char *CAFPROF_GRP_COLL;

extern int esd_open();
extern void esd_enter(INT4);
extern void esd_exit(INT4);

#define PROFILE_INIT() { profile_init();  }

#define PROFILE_STATS_INIT() { profile_stats_init(); }

#define PROFILE_STATS_DUMP() { profile_stats_dump(); }

#define PROFILE_RMA_STORE_STRIDED_BEGIN(proc, stride_levels, count)  { \
    int i; int nbytes = 1;  \
    if (profiling_enabled) \
    for (i = 0; i <= stride_levels; i++) nbytes = nbytes * count[i]; \
    profile_rma_store_begin( proc, nbytes); \
}

#define PROFILE_RMA_STORE_BEGIN(proc, nelem ) { \
    profile_rma_store_begin(proc, nelem ); \
}

#define PROFILE_RMA_STORE_END(proc)  { \
    profile_rma_store_end(proc); \
}

#define PROFILE_RMA_STORE_DEFERRED_END(proc)  { \
    profile_save_nbstore(proc); \
    profile_unset_in_prof_region(); \
}

#define PROFILE_RMA_STORE(proc, nelem) { \
    profile_rma_store(proc, nelem); \
}

#define PROFILE_RMA_LOAD_STRIDED_BEGIN(proc, stride_levels, count)  { \
    int i; int nbytes = 1;  \
    if (profiling_enabled) \
    for (i = 0; i <= stride_levels; i++) nbytes = nbytes * count[i]; \
    profile_rma_load_begin( proc, nbytes); \
}

#define PROFILE_RMA_LOAD_BEGIN(proc, nelem ) { \
    profile_rma_load_begin(proc, nelem); \
}

#define PROFILE_RMA_LOAD_END(proc)  { \
    profile_rma_load_end(proc); \
}

#define PROFILE_RMA_LOAD_DEFERRED_END(proc)  { \
    profile_save_nbload(proc); \
    profile_unset_in_prof_region(); \
}

#define PROFILE_RMA_LOAD(proc, nelem) { \
    profile_rma_load(proc, nelem); \
}

#define PROFILE_COMM_HANDLE_END(hdl) {\
    profile_comm_handle_end(hdl); \
}

#define PROFILE_RMA_END_ALL        profile_rma_end_all

#define PROFILE_RMA_END_ALL_STORES profile_rma_end_all_nbstores

#define PROFILE_RMA_END_ALL_STORES_TO_PROC \
                                   profile_rma_end_all_nbstores_to_proc

#define PROFILE_RMA_END_ALL_LOADS  profile_rma_end_all_nbloads

#define PROFILE_RMA_END_ALL_LOADS_TO_PROC(proc) \
                                   profile_rma_end_all_nbloads_to_proc

#define PROFILE_GET_CACHE_HIT      profile_record_get_cache_hit
#define PROFILE_GET_CACHE_MISS     profile_record_get_cache_miss

#define PROFILE_GET_CACHE_WRITE_THROUGH \
                                   profile_record_get_cache_write_through


#ifdef EPIK

#include <epik_user.h>
#include <elg_trc.h>


#define PROFILE_REGION_ENTRY(rname,grp,rtype)    \
    static int _##rname##_init = 0; \
    static elg_ui4 _##rname##_rid; \
    if (epik_enabled && (prof_groups&(grp))) { \
        if (!_##rname##_init) { \
            int fid = esd_def_file(__FILE__); \
            _##rname##_rid = esd_def_region(#rname , fid, __LINE__, ELG_NO_LNO, \
                    "CAF" , rtype); \
            _##rname##_init = 1; \
        } \
        esd_enter(_##rname##_rid); \
    }

#define PROFILE_FUNC_ENTRY(grp) \
    static int _prof_func_init = 0; \
    static elg_ui4 _prof_func_rid; \
    if (epik_enabled && (prof_groups&(grp))) { \
        if (!_prof_func_init) { \
            int fid = esd_def_file(__FILE__); \
            const char *group_name; \
            if (CAFPROF_MEM_ALLOC & grp) \
                group_name = CAFPROF_GRP_MEM; \
            else if (CAFPROF_ONESIDED_COMM & grp) \
                group_name = CAFPROF_GRP_COMM; \
            else if (CAFPROF_SYNC & grp) \
                group_name = CAFPROF_GRP_SYNC; \
            else if (CAFPROF_COLLECTIVES & grp) \
                group_name = CAFPROF_GRP_COLL; \
            else \
                group_name = CAFPROF_GRP; \
            _prof_func_rid = esd_def_region(__func__ , fid, __LINE__, ELG_NO_LNO, \
                    group_name , ELG_FUNCTION); \
            _prof_func_init = 1; \
        } \
        esd_enter(_prof_func_rid); \
    }

#define PROFILE_REGION_EXIT(rname,grp)  \
    if (epik_enabled && (prof_groups&(grp))) { \
        esd_exit(_##rname##_rid); \
    }

#define PROFILE_FUNC_EXIT(grp)  \
    if (epik_enabled && (prof_groups&(grp))) { \
        esd_exit(_prof_func_rid); \
    }

#define PROFILE_PUT_START(proc, rma_prof_rid, nelem) { \
    if (epik_enabled) { \
        elg_put_1ts(proc, rma_prof_rid, nelem); \
    } \
}

#define PROFILE_PUT_END_REMOTE(proc, rma_prof_rid) { \
    if (epik_enabled) { \
        elg_put_1te_remote(proc, rma_prof_rid); \
    } \
}

#define PROFILE_GET_START_REMOTE(proc, rma_prof_rid, nelem) { \
    if (epik_enabled) { \
        elg_get_1ts_remote(proc, rma_prof_rid, nelem); \
    } \
}

#define PROFILE_GET_END(proc, rma_prof_rid) { \
    if (epik_enabled) { \
        elg_get_1te(proc, rma_prof_rid); \
    } \
}

#else

#define PROFILE_REGION_ENTRY(rname,grp,rtype)               ((void) 1)

#define PROFILE_FUNC_ENTRY(grp)                             ((void) 1)

#define PROFILE_REGION_EXIT(rname,grp)                      ((void) 1)

#define PROFILE_FUNC_EXIT(grp)                              ((void) 1)

#define PROFILE_PUT_START(proc, rma_prof_rid, nelem)        ((void) 1)

#define PROFILE_PUT_END_REMOTE(proc, rma_prof_rid)          ((void) 1)

#define PROFILE_GET_START_REMOTE(proc, rma_prof_rid, nelem) ((void) 1)

#define PROFILE_GET_END(proc, rma_prof_rid)                 ((void) 1)

#endif /* defined(EPIK) */

void profile_init();
void profile_stats_init();
void profile_stats_dump();

void profile_rma_store_begin(int proc, int nelem);
void profile_rma_store_end(int proc);
void profile_rma_store(int proc, int nelem);
void profile_rma_load_begin(int proc, int nelem);
void profile_rma_load_end(int proc);
void profile_rma_load(int proc, int nelem);

void profile_rma_nbstore_end(int proc, int rid);
void profile_save_nbstore(int proc);
void profile_save_nbstore_rmaid(int proc, int rid);

void profile_rma_nbload_end(int proc, int rid);
void profile_save_nbload(int proc);
void profile_save_nbload_rmaid(int proc, int rid);

void profile_rma_end_all();
void profile_rma_end_all_nbstores();
void profile_rma_end_all_nbstores_to_proc(int proc);
void profile_rma_end_all_nbloads();
void profile_rma_end_all_nbloads_to_proc(int proc);

void profile_set_in_prof_region();
void profile_unset_in_prof_region();

void profile_record_get_cache_hit(int proc);
void profile_record_get_cache_miss(int proc);
void profile_record_get_cache_write_through(int proc);

void uhcaf_profile_start();
void uhcaf_profile_stop();

void uhcaf_profile_set_events(caf_prof_groups_t *groups);
void uhcaf_profile_reset_events();
void uhcaf_profile_add_events(caf_prof_groups_t *groups);
void uhcaf_profile_remove_events(caf_prof_groups_t *groups);

#endif                          /* PCAF_INSTRUMENT */


#endif                          /* _PROFILE_H */
