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

#ifndef _PROFILE_H
#define _PROFILE_H

#ifndef PCAF_INSTRUMENT

#define PROFILE_REGION_ENTRY(rname,grp,rtype)                        ((void) 1)
#define PROFILE_FUNC_ENTRY(grp)                                      ((void) 1)

#define PROFILE_REGION_EXIT(rname)                                   ((void) 1)
#define PROFILE_FUNC_EXIT()                                          ((void) 1)



#define PROFILE_INIT()                                               ((void) 1)

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

#else

#include <epik_user.h>
#include <elg_trc.h>

#pragma weak esd_open
#pragma weak esd_def_file
#pragma weak esd_def_region
#pragma weak esd_enter
#pragma weak esd_exit

#pragma weak elg_put_1ts
#pragma weak elg_put_1te
#pragma weak elg_put_1te_remote

#pragma weak elg_get_1ts
#pragma weak elg_get_1ts_remote
#pragma weak elg_get_1te

extern int in_rma_region;
extern int rma_prof_rid;
extern int profiling_enabled;

extern void esd_enter(elg_ui4);
extern void esd_exit(elg_ui4);

#define PROFILE_REGION_ENTRY(rname,grp,rtype)    \
    static int _##rname##_init = 0; \
    static elg_ui4 _##rname##_rid; \
    if (profiling_enabled) { \
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
    if (profiling_enabled) { \
        if (!_prof_func_init) { \
            int fid = esd_def_file(__FILE__); \
            _prof_func_rid = esd_def_region(__func__ , fid, __LINE__, ELG_NO_LNO, \
                    "CAF" , ELG_FUNCTION); \
            _prof_func_init = 1; \
        } \
        esd_enter(_prof_func_rid); \
    }

#define PROFILE_REGION_EXIT(rname)  \
    if (profiling_enabled) { \
        esd_exit(_##rname##_rid); \
    }

#define PROFILE_FUNC_EXIT()  \
    if (profiling_enabled) { \
        esd_exit(_prof_func_rid); \
    }

#define PROFILE_INIT() { profile_init();  }

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

#define PROFILE_RMA_END_ALL_STORES_TO_PROC(proc) \
                                   profile_rma_end_all_nbstores_to_proc

#define PROFILE_RMA_END_ALL_LOADS  profile_rma_end_all_nbloads

#define PROFILE_RMA_END_ALL_LOADS_TO_PROC(proc) \
                                   profile_rma_end_all_nbloads_to_proc

void profile_init();

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

void uhcaf_profile_suspend();
void uhcaf_profile_resume();

#endif                          /* PCAF_INSTRUMENT */


#endif                          /* _PROFILE_H */
