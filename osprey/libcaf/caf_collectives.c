/*
 Runtime library for supporting Coarray Fortran

 Copyright (C) 2009-2012 University of Houston.

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

#include "dopevec.h"

#include "caf_collectives.h"
#include "caf_rtl.h"

#if defined(ARMCI)
#include "armci_comm_layer.h"
#elif defined(GASNET)
#include "gasnet_comm_layer.h"
#endif

#include "util.h"
#include "trace.h"

/* initialized in comm_init() */
extern unsigned long _this_image;
extern unsigned long _num_images;

#define WORD_SIZE 4

#define USE_CAF_IMPLEMENTATION

#define CAF_CO_(op, type, ndim) \
    switch (ndim) { \
        case 0: co_##op##_##type##_##0__(source,result);break;\
        case 1: co_##op##_##type##_##1__(source,result);break;\
        case 2: co_##op##_##type##_##2__(source,result);break;\
        case 3: co_##op##_##type##_##3__(source,result);break;\
        case 4: co_##op##_##type##_##4__(source,result);break;\
        case 5: co_##op##_##type##_##5__(source,result);break;\
        case 6: co_##op##_##type##_##6__(source,result);break;\
        case 7: co_##op##_##type##_##7__(source,result);break;\
        default: Error("rank > 7 not supported");\
    }

/* CO_BCAST */

void _CO_BCAST_I1(DopeVectorType * source, INTEGER1 * src_img_p)
{
    INTEGER8 s_image = *src_img_p;
    INTEGER8 *s_image_p = &s_image;
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "calling _CO_BCAST_I8");
    _CO_BCAST_I8(source, s_image_p);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "completed _CO_BCAST_I8");
}

void _CO_BCAST_I2(DopeVectorType * source, INTEGER2 * src_img_p)
{
    INTEGER8 s_image = *src_img_p;
    INTEGER8 *s_image_p = &s_image;
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "calling _CO_BCAST_I8");
    _CO_BCAST_I8(source, s_image_p);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "completed _CO_BCAST_I8");
}

void _CO_BCAST_I4(DopeVectorType * source, INTEGER4 * src_img_p)
{
    INTEGER8 s_image = *src_img_p;
    INTEGER8 *s_image_p = &s_image;
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "calling _CO_BCAST_I8");
    _CO_BCAST_I8(source, s_image_p);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "completed _CO_BCAST_I8");
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
            src_strides[i + k] = source->dimension[i + k].stride_mult;
            dest_strides[i + k] = source->dimension[i + k].stride_mult;
        }
        for (i = 1; i <= _num_images; i++) {
            if (_this_image != i) {
                /* non-blocking would make sense here */
                if (stride_levels > 0) {
                    __coarray_strided_write(i, dest, dest_strides,
                                            src, src_strides, count,
                                            stride_levels);
                } else {
                    __coarray_write(i, dest, src, count[0]);
                }

            }
        }
        _SYNC_IMAGES_ALL(NULL, 0, NULL, 0);
    } else {
        /* is destination */

        if (source_image < 1 || source_image > _num_images) {
            LIBCAF_TRACE(LIBCAF_LOG_FATAL,
                         "CO_BCAST called with invalid source_image.");
        }

        _SYNC_IMAGES((int *) &source_image, 1, NULL, 0, NULL, 0);
    }
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}


/* CO_MAXVAL */

void _CO_MAXVAL_INT1_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_maxval_int1_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_INT1(DopeVectorType * source, DopeVectorType * result)
{
    int n_dim = source->n_dim;
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(maxval, int1, n_dim)        /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_INT2_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_maxval_int2_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_INT2(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(maxval, int2, n_dim)        /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_INT4_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_maxval_int4_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_INT4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(maxval, int4, n_dim)        /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_INT8_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_maxval_int8_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_INT8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(maxval, int8, n_dim)        /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_REAL4_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_maxval_real4_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_REAL4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(maxval, real4, n_dim)       /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_REAL8_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_maxval_real8_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_REAL8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(maxval, real8, n_dim)       /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_REAL16_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    // co_maxval_real16_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_REAL16(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(maxval, real16, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_C4_0(DopeVectorType * source, DopeVectorType * result)
{
    Error("CO_MAXVAL not supported for complex types");
#ifdef USE_CAF_IMPLEMENTATION
    // co_maxval_c4_0__(source, result);
#else
#endif
}

void _CO_MAXVAL_C4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
    Error("CO_MAXVAL not supported for complex types");
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(maxval, c4, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_C8_0(DopeVectorType * source, DopeVectorType * result)
{
    Error("CO_MAXVAL not supported for complex types");
#ifdef USE_CAF_IMPLEMENTATION
    // co_maxval_c8_0__(source, result);
#else
#endif
}

void _CO_MAXVAL_C8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
    Error("CO_MAXVAL not supported for complex types");
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(maxval, c8, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_C16_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    // co_maxval_c16_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MAXVAL_C16(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(maxval, c16, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

/* CO_MINVAL */

void _CO_MINVAL_INT1_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_minval_int1_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_INT1(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(minval, int1, n_dim)        /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_INT2_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_minval_int2_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_INT2(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(minval, int2, n_dim)        /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_INT4_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_minval_int4_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_INT4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(minval, int4, n_dim)        /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_INT8_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_minval_int8_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_INT8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(minval, int8, n_dim)        /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_REAL4_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_minval_real4_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_REAL4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(minval, real4, n_dim)       /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_REAL8_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_minval_real8_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_REAL8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(minval, real8, n_dim)       /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_REAL16_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    // co_minval_real16_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_REAL16(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(minval, real16, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_C4_0(DopeVectorType * source, DopeVectorType * result)
{
    Error("CO_MINVAL not supported for complex types");
#ifdef USE_CAF_IMPLEMENTATION
    // co_minval_c4_0__(source, result);
#else
#endif
}

void _CO_MINVAL_C4(DopeVectorType * source, DopeVectorType * result)
{
    int n_dim = source->n_dim;
    Error("CO_MINVAL not supported for complex types");
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(minval, c4, n_dim) /* see macro definition at top */
#else
#endif
}

void _CO_MINVAL_C8_0(DopeVectorType * source, DopeVectorType * result)
{
    Error("CO_MINVAL not supported for complex types");
#ifdef USE_CAF_IMPLEMENTATION
    // co_minval_c8_0__(source, result);
#else
#endif
}

void _CO_MINVAL_C8(DopeVectorType * source, DopeVectorType * result)
{
    int n_dim = source->n_dim;
    Error("CO_MINVAL not supported for complex types");
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(minval, c8, n_dim) /* see macro definition at top */
#else
#endif
}

void _CO_MINVAL_C16_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    // co_minval_c16_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_MINVAL_C16(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(minval, c16, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

/* CO_SUM */

void _CO_SUM_INT1_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_sum_int1_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_INT1(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(sum, int1, n_dim)   /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_INT2_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_sum_int2_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_INT2(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(sum, int2, n_dim)   /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_INT4_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_sum_int4_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_INT4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(sum, int4, n_dim)   /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_INT8_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_sum_int8_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_INT8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(sum, int8, n_dim)   /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_REAL4_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_sum_real4_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_REAL4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(sum, real4, n_dim)  /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_REAL8_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_sum_real8_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_REAL8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(sum, real8, n_dim)  /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_REAL16_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    // co_sum_real16_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_REAL16(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(sum, real16, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_C4_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_sum_c4_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_C4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(sum, c4, n_dim)     /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_C8_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_sum_c8_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_C8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(sum, c8, n_dim)     /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_C16_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    // co_sum_c16_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_SUM_C16(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(sum, c16, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

/* CO_PRODUCT */

void _CO_PRODUCT_INT1_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_product_int1_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_INT1(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(product, int1, n_dim)       /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_INT2_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_product_int2_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_INT2(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(product, int2, n_dim)       /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_INT4_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_product_int4_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_INT4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(product, int4, n_dim)       /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_INT8_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_product_int8_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_INT8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(product, int8, n_dim)       /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_REAL4_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_product_real4_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_REAL4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(product, real4, n_dim)      /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_REAL8_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_product_real8_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_REAL8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(product, real8, n_dim)      /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_REAL16_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    // co_product_real16_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_REAL16(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(product, real16, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_C4_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_product_c4_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_C4(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(product, c4, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_C8_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    co_product_c8_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_C8(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    CAF_CO_(product, c8, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_C16_0(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
#ifdef USE_CAF_IMPLEMENTATION
    // co_product_c16_0__(source, result);
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

void _CO_PRODUCT_C16(DopeVectorType * source, DopeVectorType * result)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "start");
    int n_dim = source->n_dim;
#ifdef USE_CAF_IMPLEMENTATION
    // CAF_CO_(product, c16, n_dim) /* see macro definition at top */
#else
#endif
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "end");
}

#if 0
/* COSUM  (modified Joon's armci_comaxval function) */
/* Accumulates the value of src_dv on all images and stores it into sum_dv
 * of root */
void comm_cosum(DopeVectorType * src_dv, DopeVectorType * sum_dv, int root)
{
    int i, iter;
    int total_iter = (int) myceillog2(_num_images);
    unsigned int el_len;
    unsigned int target;
    void *local_buf;
    int total_bytes = 1;

    // initialization
    el_len = src_dv->base_addr.a.el_len >> 3;   // convert bits to bytes
    for (i = 0; i < src_dv->n_dim; i++)
        total_bytes *= src_dv->dimension[i].extent;
    local_buf = malloc(total_bytes);
    memset(local_buf, 0, total_bytes);
    total_bytes *= el_len;
    // copy content of dopevector from src to sum locally
    memcpy(sum_dv->base_addr.a.ptr, src_dv->base_addr.a.ptr, total_bytes);

    // swap processed ID between 0 and root (non zero process ID)
    int vPID = (_this_image == root) ?
        0 : (_this_image == 0) ? root : _this_image;

    // do reduction
    for (iter = 0; iter < total_iter; iter++) {
        if ((vPID % my_pow2(iter + 1)) == 0) {
            if ((vPID + my_pow2(iter)) < _num_images) {
                // compute target process IDs for data transfer
                target = vPID + my_pow2(iter);

                //swap back for process Id 0 and root process(non-zero)
                if (target == root)
                    target = 0;

                comm_read(target, src_dv->base_addr.a.ptr, local_buf,
                          total_bytes);

                dope_add(local_buf, sum_dv, total_bytes);
            }
        }
        comm_barrier_all();
    }

    free(local_buf);
    //Broadcast for all to all
}

#endif
