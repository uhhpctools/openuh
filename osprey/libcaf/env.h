/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2011-2013 University of Houston.

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

#ifndef _ENV_H
#define _ENV_H

/* environment */

#define ENV_DTRACE                    "UHCAF_DTRACE"
#define ENV_DTRACE_DIR                "UHCAF_DTRACE_DIR"
#define ENV_PROFILE                   "UHCAF_PROFILE"
#define ENV_STATS                     "UHCAF_STATS"
#define ENV_GETCACHE                  "UHCAF_GETCACHE"
#define ENV_GETCACHE_SYNC_REFETCH     "UHCAF_GETCACHE_SYNC_REFETCH"
#define ENV_PROGRESS_THREAD           "UHCAF_PROGRESS_THREAD"
#define ENV_PROGRESS_THREAD_INTERVAL  "UHCAF_PROGRESS_THREAD_INTERVAL"
#define ENV_GETCACHE_BLOCK_SIZE       "UHCAF_GETCACHE_BLOCK_SIZE"
#define ENV_IMAGE_HEAP_SIZE           "UHCAF_IMAGE_HEAP_SIZE"
#define ENV_INIT_TEAM_HEAP_SIZE       "UHCAF_INIT_TEAM_HEAP_SIZE"
#define ENV_NB_XFER_LIMIT             "UHCAF_NB_XFER_LIMIT"
#define ENV_CO_REDUCE_ALGORITHM       "UHCAF_CO_REDUCE_ALGORITHM"
#define ENV_SYNC_IMAGES_ALGORITHM     "UHCAF_SYNC_IMAGES_ALGORITHM"
#define ENV_TEAM_BARRIER_ALGORITHM    "UHCAF_TEAM_BARRIER_ALGORITHM"
#define ENV_OUT_OF_SEGMENT_RMA        "UHCAF_OUT_OF_SEGMENT_RMA"
#define ENV_ALLOC_BYTE_ALIGNMENT      "UHCAF_ALLOC_BYTE_ALIGNMENT"
#define ENV_LOCAL_PACK_NONCONTIG_PUT  "UHCAF_LOCAL_PACK_NONCONTIG_PUT"
#define ENV_SHARED_MEM_RMA_BYPASS     "UHCAF_SHARED_MEM_RMA_BYPASS"
#define ENV_RMA_ORDERING              "UHCAF_RMA_ORDERING"
#define ENV_COLLECTIVES_BUFSIZE       "UHCAF_COLLECTIVES_BUFSIZE"
#define ENV_COLLECTIVES_MAX_WORKBUFS  "UHCAF_COLLECTIVES_MAX_WORKBUFS"
#define ENV_COLLECTIVES_1SIDED        "UHCAF_COLLECTIVES_1SIDED"
#define ENV_COLLECTIVES_USE_CANARY    "UHCAF_COLLECTIVES_USE_CANARY"
#define ENV_COLLECTIVES_2LEVEL        "UHCAF_COLLECTIVES_2LEVEL"

#define DEFAULT_ENABLE_PROFILE                  0
#define DEFAULT_ENABLE_STATS                    0
#define DEFAULT_ENABLE_GETCACHE                 0
#define DEFAULT_ENABLE_GETCACHE_SYNC_REFETCH    1
#define DEFAULT_ENABLE_PROGRESS_THREAD          0
#define DEFAULT_PROGRESS_THREAD_INTERVAL        1000L /* ns */
#define DEFAULT_GETCACHE_BLOCK_SIZE             65536L
#define DEFAULT_NB_XFER_LIMIT                   16
#define DEFAULT_ENABLE_OUT_OF_SEGMENT_RMA       0
#define DEFAULT_ALLOC_BYTE_ALIGNMENT            8
#define DEFAULT_ENABLE_LOCAL_PACK_NONCONTIG_PUT 0
#define DEFAULT_ENABLE_SHARED_MEM_RMA_BYPASS    1
#define DEFAULT_COLLECTIVES_BUFSIZE             4194304 /* 4 MB */
#define DEFAULT_COLLECTIVES_MAX_WORKBUFS        0
#define DEFAULT_ENABLE_COLLECTIVES_1SIDED       0
#define DEFAULT_ENABLE_COLLECTIVES_USE_CANARY   0
#define DEFAULT_ENABLE_COLLECTIVES_2LEVEL       0

/* these should be overridden by the defaults in cafrun script */
#define DEFAULT_IMAGE_HEAP_SIZE                 31457280L
#define DEFAULT_INIT_TEAM_HEAP_SIZE             20971520L

int get_env_flag(const char *var_name, int default_val);
size_t get_env_size(const char *var_name, size_t default_size);
size_t get_env_size_with_unit(const char *var_name, size_t default_size);

#endif                          /* _ENV_H */
