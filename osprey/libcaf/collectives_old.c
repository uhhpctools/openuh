/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2009-2013 University of Houston.

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
#include <math.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "dopevec.h"

#include "collectives_old.h"
#include "caf_rtl.h"

#include "comm.h"
#include "alloc.h"
#include "util.h"
#include "trace.h"
#include "profile.h"
#include "team.h"

co_reduce_t co_reduce_algorithm;

/* initialized in comm_init() */
extern unsigned long _this_image;
extern unsigned long _num_images;

#define WORD_SIZE 4

/* CO_BCAST */

void _CO_BCAST_I1(DopeVectorType * source, INTEGER1 * src_img_p)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_BCAST);
    INTEGER8 s_image = *src_img_p;
    INTEGER8 *s_image_p = &s_image;
    _CO_BCAST_I8(source, s_image_p);
    PROFILE_FUNC_EXIT(CAFPROF_BCAST);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

void _CO_BCAST_I2(DopeVectorType * source, INTEGER2 * src_img_p)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_BCAST);
    INTEGER8 s_image = *src_img_p;
    INTEGER8 *s_image_p = &s_image;
    _CO_BCAST_I8(source, s_image_p);
    PROFILE_FUNC_EXIT(CAFPROF_BCAST);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

void _CO_BCAST_I4(DopeVectorType * source, INTEGER4 * src_img_p)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_BCAST);
    INTEGER8 s_image = *src_img_p;
    INTEGER8 *s_image_p = &s_image;
    _CO_BCAST_I8(source, s_image_p);
    PROFILE_FUNC_EXIT(CAFPROF_BCAST);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

/* currently using a naive linear broadcast algorithm */
void _CO_BCAST_I8(DopeVectorType * source, INTEGER8 * src_img_p)
{
    void *dest, *src;
    size_t dest_strides[7], src_strides[7], count[8];
    int stride_levels;
    long int n_dim = source->n_dim;
    long int elem_size;
    INTEGER8 source_image = *src_img_p;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_BCAST);

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE,
                 "source = %p, *src_img_p = %lu", source, *src_img_p);

    if (source->type_lens.type == DVTYPE_ASCII) {
        elem_size = source->base_addr.charptr.byte_len;
    } else {
        elem_size = source->base_addr.a.el_len >> 3;
    }

    if (_this_image == source_image) {
        int i;
        int k = 0;
        /* is source */
        dest = src = source->base_addr.a.ptr;

        /* check if first dimension is strided */
        if (n_dim > 0) {
            int first_stride = 1;
            if (source->type_lens.type == DVTYPE_ASCII ||
                source->type_lens.type == DVTYPE_DERIVEDBYTE) {
                /* first dim is strided if the first stride multipler /
                 * elem_size is greater than 1 */
                first_stride =
                    source->dimension[0].stride_mult / elem_size;
            } else if (elem_size > WORD_SIZE) {
                first_stride = source->dimension[0].stride_mult /
                    (elem_size / WORD_SIZE);
            } else {
                first_stride = source->dimension[0].stride_mult;
            }

            if (first_stride > 1) {
                k = 1;
                count[0] = elem_size;
                count[1] = source->dimension[0].extent;
                src_strides[0] = elem_size * first_stride;
                dest_strides[0] = elem_size * first_stride;
            } else {
                k = 0;
                count[0] = elem_size * source->dimension[0].extent;
            }
        } else {
            count[0] = elem_size;
        }

        stride_levels = n_dim - 1 + k;
        for (i = 0; i < stride_levels; i++) {
            count[i + 1 + k] = source->dimension[i + 1].extent;
            src_strides[i + k] = elem_size*source->dimension[i + k].stride_mult;
            dest_strides[i + k] = elem_size*source->dimension[i + k].stride_mult;
        }
        comm_barrier_all();
        for (i = 1; i <= _num_images; i++) {
            if (_this_image != i) {
                /* non-blocking would make sense here */
                if (stride_levels > 0) {
                    __coarray_strided_write(i, dest, dest_strides,
                                            src, src_strides, count,
                                            stride_levels, 0, NULL);
                } else {
                    __coarray_write(i, dest, src, count[0], 0, NULL);
                }

            }
        }
    } else {
        /* is destination */

        if (source_image < 1 || source_image > _num_images) {
            LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                         "CO_BCAST called with invalid source_image.");
        }

        comm_barrier_all();
    }
    comm_barrier_all();

    PROFILE_FUNC_EXIT(CAFPROF_BCAST);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}


#define USE_CAF_IMPLEMENTATION

#define CAF_CO_(op, alg, type, ndim) \
    switch (ndim) { \
        case 0: co_##op##_##alg##_##type##_##0__(source,result);break;\
        case 1: co_##op##_##alg##_##type##_##1__(source,result);break;\
        case 2: co_##op##_##alg##_##type##_##2__(source,result);break;\
        case 3: co_##op##_##alg##_##type##_##3__(source,result);break;\
        case 4: co_##op##_##alg##_##type##_##4__(source,result);break;\
        case 5: co_##op##_##alg##_##type##_##5__(source,result);break;\
        case 6: co_##op##_##alg##_##type##_##6__(source,result);break;\
        case 7: co_##op##_##alg##_##type##_##7__(source,result);break;\
        default: Error("rank > 7 not supported");\
    }

#define _CO_REDUCE(OP, TYPE, op, type) \
    void _CO_##OP##_##TYPE##_0(DopeVectorType *source, \
                               DopeVectorType *result) \
    { \
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry"); \
        PROFILE_FUNC_ENTRY(CAFPROF_REDUCE); \
        switch (co_reduce_algorithm) { \
            case CO_REDUCE_ALL2ALL: \
                co_##op##_all2all_##type##_0__(source, result); \
                break; \
            case CO_REDUCE_2TREE_SYNCALL: \
                co_##op##_2tree_syncall_##type##_0__(source, result); \
                break; \
            case CO_REDUCE_2TREE_SYNCIMAGES: \
                co_##op##_2tree_syncimages_##type##_0__(source, result); \
                break; \
            case CO_REDUCE_2TREE_EVENTS: \
                co_##op##_2tree_events_##type##_0__(source, result); \
                break; \
        } \
        PROFILE_FUNC_EXIT(CAFPROF_REDUCE); \
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");\
    } \
    void _CO_##OP##_##TYPE (DopeVectorType *source, \
                            DopeVectorType *result) \
    { \
        int n_dim = source->n_dim; \
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry"); \
        PROFILE_FUNC_ENTRY(CAFPROF_REDUCE); \
        switch (co_reduce_algorithm) { \
            case CO_REDUCE_ALL2ALL: \
                CAF_CO_(op, all2all, type, n_dim) \
                break; \
            case CO_REDUCE_2TREE_SYNCALL: \
                CAF_CO_(op, 2tree_syncall, type, n_dim) \
                break; \
            case CO_REDUCE_2TREE_SYNCIMAGES: \
                CAF_CO_(op, 2tree_syncimages, type, n_dim) \
                break; \
            case CO_REDUCE_2TREE_EVENTS: \
                CAF_CO_(op, 2tree_events, type, n_dim) \
                break; \
        } \
        PROFILE_FUNC_EXIT(CAFPROF_REDUCE); \
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");\
    }


_CO_REDUCE(MAXVAL, INT1,    maxval, int1)
_CO_REDUCE(MAXVAL, INT2,    maxval, int2)
_CO_REDUCE(MAXVAL, INT4,    maxval, int4)
_CO_REDUCE(MAXVAL, INT8,    maxval, int8)
_CO_REDUCE(MAXVAL, REAL4,   maxval, real4)
_CO_REDUCE(MAXVAL, REAL8,   maxval, real8)
//_CO_REDUCE(MAXVAL, REAL16,  maxval, real16)
//_CO_REDUCE(MAXVAL, C4,      maxval, c4)
//_CO_REDUCE(MAXVAL, C8,      maxval, c8)
//_CO_REDUCE(MAXVAL, C16,     maxval, c16)

_CO_REDUCE(MINVAL, INT1,    minval, int1)
_CO_REDUCE(MINVAL, INT2,    minval, int2)
_CO_REDUCE(MINVAL, INT4,    minval, int4)
_CO_REDUCE(MINVAL, INT8,    minval, int8)
_CO_REDUCE(MINVAL, REAL4,   minval, real4)
_CO_REDUCE(MINVAL, REAL8,   minval, real8)
//_CO_REDUCE(MINVAL, REAL16,  minval, real16)
//_CO_REDUCE(MINVAL, C4,      minval, c4)
//_CO_REDUCE(MINVAL, C8,      minval, c8)
//_CO_REDUCE(MINVAL, C16,     minval, c16)

_CO_REDUCE(SUM, INT1,    sum, int1)
_CO_REDUCE(SUM, INT2,    sum, int2)
_CO_REDUCE(SUM, INT4,    sum, int4)
_CO_REDUCE(SUM, INT8,    sum, int8)
_CO_REDUCE(SUM, REAL4,   sum, real4)
_CO_REDUCE(SUM, REAL8,   sum, real8)
//_CO_REDUCE(SUM, REAL16,  sum, real16)
_CO_REDUCE(SUM, C4,      sum, c4)
_CO_REDUCE(SUM, C8,      sum, c8)
//_CO_REDUCE(SUM, C16,     sum, c16)

_CO_REDUCE(PRODUCT, INT1,    product, int1)
_CO_REDUCE(PRODUCT, INT2,    product, int2)
_CO_REDUCE(PRODUCT, INT4,    product, int4)
_CO_REDUCE(PRODUCT, INT8,    product, int8)
_CO_REDUCE(PRODUCT, REAL4,   product, real4)
_CO_REDUCE(PRODUCT, REAL8,   product, real8)
//_CO_REDUCE(PRODUCT, REAL16,  product, real16)
_CO_REDUCE(PRODUCT, C4,      product, c4)
_CO_REDUCE(PRODUCT, C8,      product, c8)
//_CO_REDUCE(PRODUCT, C16,     product, c16)

