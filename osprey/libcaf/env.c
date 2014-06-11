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

#include <stdlib.h>
#include <string.h>
#include "trace.h"

int get_env_flag(const char *var_name, int default_val)
{
    char *val;
    if (var_name == NULL)
        return 0;

    val = getenv(var_name);

    if (val == NULL)
        return default_val;

    if (strcasecmp(val, "1") == 0 ||
        strcasecmp(val, "y") == 0 ||
        strcasecmp(val, "on") == 0 || strcasecmp(val, "yes") == 0) {
        return 1;
    } else if (strcasecmp(val, "0") == 0 ||
               strcasecmp(val, "n") == 0 ||
               strcasecmp(val, "off") == 0 || strcasecmp(val, "no") == 0) {
        return 0;
    } else {
        LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
                     "Bad val for %s: %s", var_name, val);
        return default_val;
    }
}

size_t get_env_size(const char *var_name, size_t default_size)
{
    char *p;
    char *val;

    if (var_name == NULL)
        return default_size;

    val = getenv(var_name);

    if (val == NULL)
        return default_size;

    double dummy = strtod(val, &p);

    if (*p != '\0') {
        LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
                     "Bad val for %s: %s", var_name, val);
        return default_size;
    }

    return (size_t) atoll(val);
}

size_t get_env_size_with_unit(const char *var_name, size_t default_size)
{
    char *val;
    size_t ret_val;
    char *u;

    if (var_name == NULL)
        return default_size;

    val = getenv(var_name);

    if (val == NULL)
        return default_size;

    u = alloca(strlen(val));

    ret_val = 0;
    sscanf(val, "%ld", (long *)&ret_val);
    sprintf(u, "%ld", (long) ret_val);
    if (strlen(u) != strlen(val)) {
        sscanf(val, "%ld%s", (unsigned long *)&ret_val, u);
        if (strlen(u) != 0 && strlen(u) != 1) {
            LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
                    "Bad val for %s: %s", var_name, val);
            ret_val = default_size;
        } else if (strlen(u) != 0) {

            if (strncasecmp(u, "k", strlen(u)) == 0) {
                ret_val *= 1024L;
            } else if (strncasecmp(u, "m", strlen(u)) == 0) {
                ret_val *= 1024L*1024L;
            } else if (strncasecmp(u, "g", strlen(u)) == 0) {
                ret_val *= 1024L*1024L*1024L;
            } else {
                LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
                        "Bad val for %s: %s", var_name, val);
                ret_val = default_size;
            }
        }

    }

    if (ret_val == 0)
        ret_val = default_size;

    return ret_val;
}
