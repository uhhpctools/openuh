/*
 Runtime library for supporting Coarray Fortran 

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

    strtod(val, &p);

    if (*p != '\0') {
        LIBCAF_TRACE(LIBCAF_LOG_NOTICE,
                     "Bad val for %s: %s", var_name, val);
        return default_size;
    }

    return (size_t) atoll(val);
}
