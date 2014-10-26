/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2011-2014 University of Houston.

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
