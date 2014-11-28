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

#include "collectives.h"
#include "caf_rtl.h"

#include "comm.h"
#include "alloc.h"
#include "util.h"
#include "trace.h"
#include "profile.h"
#include "team.h"

#ifdef MPI_AVAIL
#include "mpi.h"
#endif


co_reduce_t co_reduce_algorithm;

/* initialized in comm_init() */
extern unsigned long _this_image;
extern unsigned long _num_images;

void *collectives_buffer;
size_t collectives_bufsize;

int collectives_max_workbufs = 0;

int enable_collectives_1sided;
int enable_collectives_use_canary = 0;

int enable_collectives_2level = 0;
int mpi_collectives_available = 0;

extern team_type current_team;

static int get_proc_id(team_type team, int image_id);


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


#define MIN(x,y) ( (x<y) ? (x) : (y) )

void sum_reduce_int1__(void *, void *, int);
void sum_reduce_int2__(void *, void *, int);
void sum_reduce_int4__(void *, void *, int);
void sum_reduce_int8__(void *, void *, int);
void sum_reduce_real4__(void *, void *, int);
void sum_reduce_real8__(void *, void *, int);
void sum_reduce_complex4__(void *, void *, int);
void sum_reduce_complex8__(void *, void *, int);

void max_reduce_int1__(void *, void *, int);
void max_reduce_int2__(void *, void *, int);
void max_reduce_int4__(void *, void *, int);
void max_reduce_int8__(void *, void *, int);
void max_reduce_real4__(void *, void *, int);
void max_reduce_real8__(void *, void *, int);
void max_reduce_char__(void *, void *, int, int, int);


void min_reduce_int1__(void *, void *, int);
void min_reduce_int2__(void *, void *, int);
void min_reduce_int4__(void *, void *, int);
void min_reduce_int8__(void *, void *, int);
void min_reduce_real4__(void *, void *, int);
void min_reduce_real8__(void *, void *, int);
void min_reduce_char__(void *, void *, int, int, int);

void prod_reduce_int1__(void *, void *, int);
void prod_reduce_int2__(void *, void *, int);
void prod_reduce_int4__(void *, void *, int);
void prod_reduce_int8__(void *, void *, int);
void prod_reduce_real4__(void *, void *, int);
void prod_reduce_real8__(void *, void *, int);
void prod_reduce_complex4__(void *, void *, int);
void prod_reduce_complex8__(void *, void *, int);

static size_t inline get_reduction_type_size(caf_reduction_type_t type, int charlen)
{
    size_t elem_size;

    switch (type) {
        case CAF_LOGICAL1:
        case CAF_INT1:
            elem_size = 1;
            break;
        case CAF_LOGICAL2:
        case CAF_INT2:
            elem_size = 2;
            break;
        case CAF_LOGICAL4:
        case CAF_INT4:
        case CAF_REAL4:
        case CAF_COMPLEX4:
            elem_size = 4;
            break;
        case CAF_LOGICAL8:
        case CAF_INT8:
        case CAF_REAL8:
        case CAF_COMPLEX8:
            elem_size = 8;
            break;
        case CAF_REAL16:
        case CAF_COMPLEX16:
            elem_size = 16;
            break;
        case CAF_CHAR:
            elem_size =  charlen;
            break;
        default:
            Error("unexpected element type");
    }

    return elem_size;
}

static void inline perform_reduce(caf_reduction_op_t op, caf_reduction_type_t type,
                           void *res, void *b, int s, int charlen)
{
    if (op == CAF_SUM) {
        switch (type) {
            case CAF_INT1:
                sum_reduce_int1__(res, b, s);
                break;
            case CAF_INT2:
                sum_reduce_int2__(res, b, s);
                break;
            case CAF_INT4:
                sum_reduce_int4__(res, b, s);
                break;
            case CAF_INT8:
                sum_reduce_int8__(res, b, s);
                break;
            case CAF_REAL4:
                sum_reduce_real4__(res, b, s);
                break;
            case CAF_REAL8:
                sum_reduce_real8__(res, b, s);
                break;
            case CAF_COMPLEX4:
                sum_reduce_complex4__(res, b, s);
                break;
            case CAF_COMPLEX8:
                sum_reduce_complex8__(res, b, s);
                break;
            default:
                Error("unexpected element type (%d) for CO_SUM", type);
        }
    } else if (op == CAF_MAX) {
        switch (type) {
            case CAF_INT1:
                max_reduce_int1__(res, b, s);
                break;
            case CAF_INT2:
                max_reduce_int2__(res, b, s);
                break;
            case CAF_INT4:
                max_reduce_int4__(res, b, s);
                break;
            case CAF_INT8:
                max_reduce_int8__(res, b, s);
                break;
            case CAF_REAL4:
                max_reduce_real4__(res, b, s);
                break;
            case CAF_REAL8:
                max_reduce_real8__(res, b, s);
                break;
            case CAF_CHAR:
                max_reduce_char__(res, b, s, charlen, charlen);
                break;
            default:
                Error("unexpected element type (%d) for CO_MAX", type);
        }
    } else if (op == CAF_MIN) {
        switch (type) {
            case CAF_INT1:
                min_reduce_int1__(res, b, s);
                break;
            case CAF_INT2:
                min_reduce_int2__(res, b, s);
                break;
            case CAF_INT4:
                min_reduce_int4__(res, b, s);
                break;
            case CAF_INT8:
                min_reduce_int8__(res, b, s);
                break;
            case CAF_REAL4:
                min_reduce_real4__(res, b, s);
                break;
            case CAF_REAL8:
                min_reduce_real8__(res, b, s);
                break;
            case CAF_CHAR:
                min_reduce_char__(res, b, s, charlen, charlen);
                break;
            default:
                Error("unexpected element type (%d) for CO_MIN", type);
        }
    } else if (op == CAF_PROD) {
        switch (type) {
            case CAF_INT1:
                prod_reduce_int1__(res, b, s);
                break;
            case CAF_INT2:
                prod_reduce_int2__(res, b, s);
                break;
            case CAF_INT4:
                prod_reduce_int4__(res, b, s);
                break;
            case CAF_INT8:
                prod_reduce_int8__(res, b, s);
                break;
            case CAF_REAL4:
                prod_reduce_real4__(res, b, s);
                break;
            case CAF_REAL8:
                prod_reduce_real8__(res, b, s);
                break;
            case CAF_COMPLEX4:
                prod_reduce_complex4__(res, b, s);
                break;
            case CAF_COMPLEX8:
                prod_reduce_complex8__(res, b, s);
                break;
            default:
                Error("unexpected element type (%d) for CO_PRODUCT", type);
        }
    } else {
        Error("unexpected reduction type (%d)", op);
    }
}

static void (*char_opr)(void *, int, void *, void *, int, int);
static INTEGER1 (*int1_opr)(void *, void *);
static INTEGER2 (*int2_opr)(void *, void *);
static INTEGER4 (*int4_opr)(void *, void *);
static INTEGER8 (*int8_opr)(void *, void *);
static BYTE16 (*byte16_opr)(void *, void *);
static REAL4 (*real4_opr)(void *, void *);
static REAL8 (*real8_opr)(void *, void *);
static REAL16 (*real16_opr)(void *, void *);
static COMPLEX8 (*complex8_opr)(void *, void *);
static COMPLEX16 (*complex16_opr)(void *, void *);
static void (*dt_opr)(void *, void *, void *);

static void perform_udr(void *opr, char *res, char *b, int s, int elem_size,
                        int charlen, int type)
{
    int j, k;
    int char_type = 0;
    int integer_type = 0;
    int real_type = 0;
    int complex_type = 0;
    int other_type = 0;

    if (type == DVTYPE_ASCII)
        char_type = 1;
    else if (type == DVTYPE_INTEGER || type == DVTYPE_LOGICAL)
        integer_type = 1;
    else if (type == DVTYPE_REAL)
        real_type = 1;
    else if (type == DVTYPE_COMPLEX)
        complex_type = 1;
    else
        other_type = 1;

    if (char_type) {
        char_opr = opr;
        /* reduction for character string type */
        for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++)
            (char_opr)(&res[j], charlen, &res[j], &b[j], charlen, charlen);
    } else if (integer_type || other_type) {
        if (elem_size == 1) {
            int1_opr = opr;
            /* reduction for 1 byte type */
            for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++)
                ((INTEGER1*)res)[k] = (int1_opr)(&res[j], &b[j]);
        } else if (elem_size == 2) {
            int2_opr = opr;
            /* reduction for 2 bytes type */
            for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++)
                ((INT2*)res)[k] = (int2_opr)(&res[j], &b[j]);
        } else if (elem_size == 4) {
            int4_opr = opr;
            /* reduction for 4 bytes type */
            for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++)
                ((INTEGER4*)res)[k] = (int4_opr)(&res[j], &b[j]);
        } else if (elem_size == 8) {
            int8_opr = opr;
            /* reduction for 8 bytes type */
            for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++) {
                ((INTEGER8*)res)[k] = (int8_opr)(&res[j], &b[j]);
            }
        } else if (elem_size == 16) {
            byte16_opr = opr;
            /* reduction for 16 bytes type */
            for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++)
                ((BYTE16*)res)[k] = (byte16_opr)(&res[j], &b[j]);
        } else if (elem_size == 3) {
            int4_opr = opr;
            /* reduction for 4 byte type */
            for (j = 0; j < s*elem_size; j += elem_size) {
                INTEGER4 t = (int4_opr)(&res[j], &b[j]);
                memcpy(&res[j], &t, elem_size);
            }
        } else if (elem_size > 4 && elem_size < 8) {
            int8_opr = opr;
            /* reduction for 8 byte type */
            for (j = 0; j < s*elem_size; j += elem_size) {
                INTEGER8 t = (int8_opr)(&res[j], &b[j]);
                memcpy(&res[j], &t, elem_size);
            }
        } else if (elem_size > 8 && elem_size < 16) {
            byte16_opr = opr;
            /* reduction for 16 byte type */
            for (j = 0; j < s*elem_size; j += elem_size) {
                BYTE16 t = (byte16_opr)(&res[j], &b[j]);
                memcpy(&res[j], &t, elem_size);
            }
        } else {
            dt_opr = opr;
            for (j = 0; j < s*elem_size; j += elem_size)
                dt_opr(&res[j], &res[j], &b[j]);
        }
    } else if (real_type) {
        if (elem_size == 4) {
            real4_opr = opr;
            /* reduction for 4 bytes type */
            for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++)
                ((REAL4*)res)[k] = (real4_opr)(&res[j], &b[j]);
        } else if (elem_size == 8) {
            real8_opr = opr;
            /* reduction for 4 bytes type */
            for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++) {
                ((REAL8*)res)[k] = (real8_opr)(&res[j], &b[j]);
            }
        } else if (elem_size == 16) {
            real16_opr = opr;
            /* reduction for 8 bytes type */
            for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++)
                ((REAL16*)res)[k] = (real16_opr)(&res[j], &b[j]);
        }
    } else if (complex_type) {
        if (elem_size == 8) {
            complex8_opr = opr;
            /* reduction for 4 bytes type */
            for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++) {
                ((COMPLEX8*)res)[k] = (complex8_opr)(&res[j], &b[j]);
            }
        } else if (elem_size == 16) {
            complex16_opr = opr;
            /* reduction for 8 bytes type */
            for (j = 0, k = 0; j < s*elem_size; j += elem_size, k++)
                ((COMPLEX16*)res)[k] = (complex16_opr)(&res[j], &b[j]);
        }
    }
}

#ifdef MPI_AVAIL

void do_mpi_reduce(void *source, int size, MPI_Datatype dtype, MPI_Op op,
                   int root)
{
    if (_this_image == root+1) {
        MPI_Reduce(MPI_IN_PLACE, source, size, dtype, op, root,
                     MPI_COMM_WORLD);
    } else {
        MPI_Reduce(source, source, size, dtype, op, root,
                     MPI_COMM_WORLD);
    }
}

void do_mpi_char_reduce(void *source, int size, caf_reduction_type_t dtype,
                        caf_reduction_op_t op, int root, int charlen)
{
    int i;
    int log2_p;
    int q, r, partner;
    int me, p;
    int step;
    void *work;
    MPI_Request req;
    MPI_Status stat;

    me = _this_image;
    p = _num_images;

    work = malloc(size * charlen);

    /* find greatest power of 2 */
    q = 1;
    log2_p = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_p = log2_p + 1;
    }
    if (q < p) {
        log2_p = log2_p + 1;
    }

    step = 1;
    for (i = 1; i <= log2_p; i++) {
        if (((me-1)%(2*step)) < step) {
            partner = me + step;
            if (partner <= p) {
                MPI_Send(source, size*charlen, MPI_BYTE, partner-1, i,
                         MPI_COMM_WORLD);
            }
        } else {
            partner = me - step;
            if (partner >= 1) {
                MPI_Recv(work, size*charlen, MPI_BYTE, partner-1, i,
                         MPI_COMM_WORLD, &stat);

                perform_reduce(op, dtype, source, work, size, charlen);
            }
        }

        step = step * 2;
    }

    if (me ==  root+1) {
        MPI_Recv(source, size*charlen, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &stat);
    } else if (me == 1) {
        MPI_Send(source, size*charlen, MPI_BYTE, root, 0, MPI_COMM_WORLD);
    }

    free(work);
}

void do_mpi_char_allreduce(void *source, int size, caf_reduction_type_t dtype,
                        caf_reduction_op_t op, int charlen)
{
    int i;
    int log2_q;
    int q, r, partner;
    int me, p;
    int step;
    void *work;
    MPI_Request req;
    MPI_Status stat;

    me = _this_image;
    p = _num_images;

    work = malloc(size * charlen);

    /* find greatest power of 2 */
    q = 1;
    log2_q = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_q = log2_q + 1;
    }

    /* r is the number of remaining processes, after q */
    r = p - q;

    /* last r processes put values to first r processes */
    if (me > q) {
        partner = me - q;
        MPI_Send(source, size*charlen, MPI_BYTE, partner, 0, MPI_COMM_WORLD);
    } else if (me <= r) {
        partner = me + q;
        MPI_Recv(work, size*charlen, MPI_BYTE, partner, 0, MPI_COMM_WORLD,
                 &stat);
        perform_reduce(op, dtype, source, work, size, charlen);
    }

    /* first q processes do recursive doubling algorithm */
    if (me <= q) {
        step = 1;
        for (i = 1; i <= log2_q; i++) {
            if (((me-1)%(2*step)) < step) {
                partner = me + step;
                if (partner <= p) {
                    MPI_Irecv(work, size*charlen, MPI_BYTE, partner-1, i,
                              MPI_COMM_WORLD, &req);
                    MPI_Send(source, size*charlen, MPI_BYTE, partner-1, i,
                             MPI_COMM_WORLD);
                    MPI_Wait(&req, &stat);

                    perform_reduce(op, dtype, source, work, size, charlen);
                }
            } else {
                partner = me - step;
                if (partner >= 1) {
                    MPI_Irecv(work, size*charlen, MPI_BYTE, partner-1, i,
                              MPI_COMM_WORLD, &req);
                    MPI_Send(source, size*charlen, MPI_BYTE, partner-1, i,
                             MPI_COMM_WORLD);
                    MPI_Wait(&req, &stat);

                    perform_reduce(op, dtype, source, work, size, charlen);
                }
            }
            step = step * 2;
        }
    }

    /* first r processes put values to last r processes */
    if (me <= r) {
        partner = me + q;
        MPI_Send(source, size*charlen, MPI_BYTE, partner, 0, MPI_COMM_WORLD);
    } else if (me > q) {
        partner = me - q;
        MPI_Recv(source, size*charlen, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &stat);
    }

    free(work);
}
#endif /* defined(MPI_AVAIL) */

#pragma weak MPI_Initialized

void co_reduce_predef_to_image__( void *source, int *result_image, int *size,
                                 int *charlen, caf_reduction_type_t *elem_type,
                                 caf_reduction_op_t *op)
{
    int k;
    int p, q, r, me, partner, proc_id;
    int i, j, step, log2_p, val;
    int k1, k2;
    int num_bufs;
    int *pot_partners, *partners;
    size_t elem_size;
    int sz;
    void *base_buffer;
    void *work_buffers;
    int base_buffer_alloc = 0;
    int work_buffers_alloc = 0;
    int do_mpi;
    int ierr;
#ifdef MPI_AVAIL
    MPI_Datatype dtype;
    MPI_Op mpi_op;
#endif

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    sz = *size;

#ifdef MPI_AVAIL
    if (!mpi_collectives_available && !enable_collectives_1sided) {
        /* check if MPI was initialized */
        if (MPI_Initialized &&
            MPI_Initialized(&mpi_collectives_available) != MPI_SUCCESS) {
            Error("MPI_Initialized check failed");
        }
    }

    if (mpi_collectives_available && !enable_collectives_1sided &&
        (current_team == NULL || current_team->depth == 0)) {
        switch (*elem_type) {
            case CAF_LOGICAL1:
            case CAF_INT1:
                dtype = MPI_INT8_T;
                break;
            case CAF_LOGICAL2:
            case CAF_INT2:
                dtype = MPI_INT16_T;
                break;
            case CAF_LOGICAL4:
            case CAF_INT4:
                dtype = MPI_INT32_T;
                break;
            case CAF_LOGICAL8:
            case CAF_INT8:
                dtype = MPI_INT64_T;
                break;
            case CAF_REAL4:
                dtype = MPI_REAL;
                break;
            case CAF_REAL8:
                dtype = MPI_DOUBLE;
                break;
            case CAF_REAL16:
                dtype = MPI_LONG_DOUBLE;
                break;
            case CAF_COMPLEX4:
                dtype = MPI_C_FLOAT_COMPLEX;
                break;
            case CAF_COMPLEX8:
                dtype = MPI_C_DOUBLE_COMPLEX;
                break;
            case CAF_COMPLEX16:
                dtype = MPI_C_LONG_DOUBLE_COMPLEX;
                break;
            case CAF_CHAR:
                dtype = MPI_CHARACTER;
                break;
            default:
                Error("unexpected mpi type (%d)", op);
        }

        switch (*op) {
            case CAF_SUM:
                mpi_op = MPI_SUM;
                break;
            case CAF_PROD:
                mpi_op = MPI_PROD;
                break;
            case CAF_MIN:
                mpi_op = MPI_MIN;
                break;
            case CAF_MAX:
                mpi_op = MPI_MAX;
                break;
            default:
                Error("unexpected reduction op (%d)", op);
        }

        /* adding barrier here to ensure communication progress before
         * entering MPI reduce routine */
//        comm_barrier_all();
        comm_sync_all(NULL, 0, NULL, 0);
        if (dtype != MPI_CHARACTER)
            do_mpi_reduce(source, *size, dtype, mpi_op, *result_image-1);
        else {
            do_mpi_char_reduce(source, *size, *elem_type, *op,
                               *result_image-1, *charlen);
        }

        return;

        /* does not reach */
    }
#endif

    me = _this_image;
    p = _num_images;

    /* find greatest power of 2 */
    q = 1;
    log2_p = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_p = log2_p + 1;
    }
    if (q < p) {
        log2_p = log2_p + 1;
    }

    elem_size = get_reduction_type_size(*elem_type, *charlen);

    k = (sz+1)*elem_size;
    if (collectives_bufsize < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((log2_p+1)*(sz+1)*elem_size);

        num_bufs = MIN(log2_p,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size,NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;
    } else if (collectives_bufsize < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(log2_p*(sz+1)*elem_size);

        num_bufs = MIN(log2_p,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = collectives_buffer;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;
    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buffer;
        work_buffers = &((char*)collectives_buffer)[(sz+1)*elem_size];
        num_bufs = ((int)collectives_bufsize-k)/k;
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }
    }


    pot_partners = malloc(log2_p*sizeof(int));
    partners = malloc(log2_p*sizeof(int));

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;

    step = 1;
    for (i = 1; i <= log2_p; i += num_bufs) {
        k = 0;
        for (j = 1; j <= MIN(num_bufs,log2_p-i+1); j++) {
            k2 = j*(sz+1);
            ((char *)work_buffers)[(k2-1)*elem_size] = 0;
            if (((me-1)%(2*step)) < step) {
                pot_partners[j-1] = me+step;
                if ((me+step) <= p) {
                    k = k + 1;
                    partners[k-1] = me+step;
                }
            } else {
                pot_partners[j-1] = me-step;
                if ((me-step) >= 1) {
                    k = k + 1;
                    partners[k-1] = me-step;
                }
            }
            step = step * 2;
        }

        for (j = 1; j <= MIN(num_bufs,log2_p-i+1); j++) {
            if (j == 1 && k > 0) {
                _SYNC_IMAGES(partners, k, NULL, 0, NULL, 0);
            }
            partner = pot_partners[j-1];
            if (partner < 1 || partner > p) continue;

            k1 = (j-1)*(sz+1)+1;
            k2 = j*(sz+1);

            if (me > partner) {
	        proc_id = get_proc_id(current_team, partner);

	        if (enable_collectives_use_canary) {

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                &((char*)base_buffer)[0],
                                sz*elem_size+1 );
                } else {

                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );
                }

            } else {

                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_reduce(*op, *elem_type, base_buffer,
                               &((char*)work_buffers)[(k1-1)*elem_size],
                               sz, *charlen);
            }
        }
    }

    if (me == *result_image) {
        ((char*)base_buffer)[sz*elem_size] = 0;
        if (me != 1) {
            int root = 1;
            _SYNC_IMAGES(&root, 1, NULL, 0, NULL, 0);

            /* poll on flag */
            comm_poll_char_while_zero(&((char*)base_buffer)[sz*elem_size]);
        }
        memcpy(source, base_buffer, sz*elem_size);
    } else if (me == 1) {
        _SYNC_IMAGES(result_image, 1, NULL, 0, NULL, 0);
        proc_id = get_proc_id(current_team, *result_image);
        if (enable_collectives_use_canary) {

            comm_nbi_write( proc_id,
                            &((char*)base_buffer)[0],
                            &((char*)base_buffer)[0],
                            sz*elem_size+1 );

        } else {

            comm_write_x( proc_id,
                          &((char*)base_buffer)[0],
                          &((char*)base_buffer)[0],
                          sz*elem_size );

            comm_nbi_write( proc_id,
                            &((char*)base_buffer)[sz*elem_size],
                            &((char*)base_buffer)[sz*elem_size],
                            1 );

        }


    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else if (work_buffers_alloc) {
        coarray_deallocate_(work_buffers, NULL);
    }

    free(pot_partners);
    free(partners);

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}


void co_reduce_predef_to_all_2level__( void *source, int *size, int *charlen,
                               caf_reduction_type_t *elem_type,
                               caf_reduction_op_t *op);

void co_reduce_predef_to_all__( void *source, int *size, int *charlen,
                               caf_reduction_type_t *elem_type,
                               caf_reduction_op_t *op)
{
    int k;
    int p, q, r, me, partner, proc_id;
    int i, j, step, log2_q, val;
    int k1, k2;
    int num_bufs;
    int *partners;
    size_t elem_size;
    int sz;
    void *base_buffer;
    void *work_buffers;
    int base_buffer_alloc = 0;
    int work_buffers_alloc = 0;
    int do_mpi;
    int ierr;
#ifdef MPI_AVAIL
    MPI_Datatype dtype;
    MPI_Op mpi_op;
#endif

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    sz = *size;

#ifdef MPI_AVAIL
    if (!mpi_collectives_available && !enable_collectives_1sided) {
        /* check if MPI was initialized */
        if (MPI_Initialized &&
            MPI_Initialized(&mpi_collectives_available) != MPI_SUCCESS) {
            Error("MPI_Initialized check failed");
        }
    }

    if (mpi_collectives_available && !enable_collectives_1sided &&
        (current_team == NULL || current_team->depth == 0)) {
        switch (*elem_type) {
            case CAF_LOGICAL1:
            case CAF_INT1:
                dtype = MPI_INT8_T;
                break;
            case CAF_LOGICAL2:
            case CAF_INT2:
                dtype = MPI_INT16_T;
                break;
            case CAF_LOGICAL4:
            case CAF_INT4:
                dtype = MPI_INT32_T;
                break;
            case CAF_LOGICAL8:
            case CAF_INT8:
                dtype = MPI_INT64_T;
                break;
            case CAF_REAL4:
                dtype = MPI_REAL;
                break;
            case CAF_REAL8:
                dtype = MPI_DOUBLE;
                break;
            case CAF_REAL16:
                dtype = MPI_LONG_DOUBLE;
                break;
            case CAF_COMPLEX4:
                dtype = MPI_C_FLOAT_COMPLEX;
                break;
            case CAF_COMPLEX8:
                dtype = MPI_C_DOUBLE_COMPLEX;
                break;
            case CAF_COMPLEX16:
                dtype = MPI_C_LONG_DOUBLE_COMPLEX;
                break;
            case CAF_CHAR:
                dtype = MPI_UNSIGNED_CHAR;
                break;
            default:
                Error("unexpected mpi type (%d)", op);
        }

        switch (*op) {
            case CAF_SUM:
                mpi_op = MPI_SUM;
                break;
            case CAF_PROD:
                mpi_op = MPI_PROD;
                break;
            case CAF_MIN:
                mpi_op = MPI_MIN;
                break;
            case CAF_MAX:
                mpi_op = MPI_MAX;
                break;
            default:
                Error("unexpected reduction op (%d)", op);
        }

        /* adding barrier here to ensure communication progress before
         * entering MPI reduce routine */
        comm_sync_all(NULL, 0, NULL, 0);
        if (dtype != MPI_CHARACTER) {
            MPI_Allreduce(MPI_IN_PLACE, source, *size, dtype, mpi_op,
                          MPI_COMM_WORLD);
        } else {
            do_mpi_char_allreduce(source, *size, *elem_type, *op, *charlen);
        }

        return;
    }
#endif

    if (enable_collectives_2level && current_team &&
        (current_team->leaders_count > 1  &&
         current_team->leaders_count < _num_images) ) {
        co_reduce_predef_to_all_2level__(source, size, charlen, elem_type, op);
        PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
        return;

        /* does not reach */
    }

    me = _this_image;
    p = _num_images;

    /* find greatest power of 2 less than p */
    q = 1;
    log2_q = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_q = log2_q + 1;
    }

    elem_size = get_reduction_type_size(*elem_type, *charlen);

    k = (sz+1)*elem_size;
    if (collectives_bufsize < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((log2_q+1)*(sz+1)*elem_size);

        num_bufs = MIN(log2_q,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;
    } else if (collectives_bufsize < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(log2_q*(sz+1)*elem_size);

        num_bufs = MIN(log2_q,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = collectives_buffer;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;
    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buffer;
        work_buffers = &((char*)collectives_buffer)[(sz+1)*elem_size];
        num_bufs = ((int)collectives_bufsize-k)/k;
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }
    }

    partners = malloc(log2_q*sizeof(int));

    /* r is the number of remaining processes, after q */
    r = p - q;

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;

    /* last r processes put values to first r processes */
    if (me > q) {
        partner = me - q;
        proc_id = get_proc_id(current_team, partner);

        _SYNC_IMAGES (&partner, 1, NULL, 0, NULL, 0);


        if (enable_collectives_use_canary) {

            comm_nbi_write( proc_id,
                            &((char*)work_buffers)[0],
                            &((char*)base_buffer)[0],
                            sz*elem_size+1 );

        } else {


            comm_write_x( proc_id,
                          &((char*)work_buffers)[0],
                          &((char*)base_buffer)[0],
                          sz*elem_size );

            comm_nbi_write( proc_id,
                            &((char*)work_buffers)[sz*elem_size],
                            &((char*)base_buffer)[sz*elem_size],
                            1 );

        }

    } else if (me <= r) {
        partner = me + q;
        ((char*)work_buffers)[sz*elem_size] = 0;
        _SYNC_IMAGES (&partner, 1, NULL, 0, NULL, 0);

        /* poll on flag */
        comm_poll_char_while_zero(&((char*)work_buffers)[sz*elem_size]);

        /* reduce:
         *   work_buf(1:sz) = work_buf(1:sz) + work_buf(sz+2:2*(sz+1)-1)
         */

        perform_reduce(*op, *elem_type, base_buffer,
                       &((char*)work_buffers)[0],
                       sz, *charlen);
    }


    /* first q processes do recursive doubling algorithm */
    if (me <= q) {
        step = 1;
        for (i = 1; i <= log2_q; i += num_bufs) {
            for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {
                k2 = j*(sz+1);
                ((char *)work_buffers)[(k2-1)*elem_size] = 0;
                if (((me-1)%(2*step)) < step) {
                    partners[j-1] = me+step;
                } else {
                    partners[j-1] = me-step;
                }
                step = step*2;
            }
            for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {

                if (j == 1) {
                    _SYNC_IMAGES(partners, MIN(num_bufs, log2_q), NULL, 0,
                                 NULL, 0);
                }
                k1 = (j-1)*(sz+1)+1;
                k2 = j*(sz+1);
                partner = partners[j-1];
                proc_id = get_proc_id(current_team, partner);

                if (enable_collectives_use_canary) {

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                &((char*)base_buffer)[0],
                                sz*elem_size+1 );

                } else {

                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );

                }


                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_reduce(*op, *elem_type, base_buffer,
                               &((char*)work_buffers)[(k1-1)*elem_size],
                               sz, *charlen);
            }
        }
    }


    /* first r processes put values to last r processes */
    if (me <= r) {
        partner = me + q;
        proc_id = get_proc_id(current_team, partner);

        _SYNC_IMAGES (&partner, 1, NULL, 0, NULL, 0);

        if (enable_collectives_use_canary) {

            comm_nbi_write( proc_id,
                            &((char*)base_buffer)[0],
                            &((char*)base_buffer)[0],
                            sz*elem_size+1 );

        } else {

            comm_write_x( proc_id,
                          &((char*)base_buffer)[0],
                          &((char*)base_buffer)[0],
                          sz*elem_size );

            comm_nbi_write( proc_id,
                            &((char*)base_buffer)[sz*elem_size],
                            &((char*)base_buffer)[sz*elem_size],
                            1 );

        }

    } else if (me > q) {
        partner = me - q;
        ((char*)base_buffer)[sz*elem_size] = 0;
        _SYNC_IMAGES (&partner, 1, NULL, 0, NULL, 0);

        /* poll on flag */
        comm_poll_char_while_zero(&((char*)base_buffer)[sz*elem_size]);
    }

    memcpy(source, base_buffer, sz*elem_size);

    free(partners);

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else if (work_buffers_alloc) {
        coarray_deallocate_(work_buffers, NULL);
    }

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

void co_reduce_predef_to_all_2level__( void *source, int *size, int *charlen,
                               caf_reduction_type_t *elem_type,
                               caf_reduction_op_t *op)
{
    int k;
    int p, q, r, me, partner, proc_id;
    int i, j, step, log2_q, val;
    int k1, k2;
    int num_bufs;
    int *partners;
    size_t elem_size;
    int sz;
    void *base_buffer;
    void *work_buffers;
    int base_buffer_alloc = 0;
    int work_buffers_alloc = 0;
    int do_mpi;
    int ierr;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    sz = *size;

    const int my_proc = current_team->codimension_mapping[_this_image - 1];
    const int is_leader = (current_team->intranode_set[1] == my_proc);
    const long int *leader_set = current_team->leader_set;
    const int leaders_count = current_team->leaders_count;
    const int intranode_count = current_team->intranode_set[0];

    /* find leader index into leaders_set */
    int leader_index = -1;
    if (is_leader) {
        int i;
        for (i = 0; i < leaders_count; i++) {
            if (leader_set[i] == my_proc) {
                leader_index = i;
                break;
            }
        }
    }

    /* compute image indices for other images on same node */
    int *local_images;
    if (is_leader) {
        local_images = malloc((intranode_count-1) * sizeof(*local_images));
        int i;
        for (i = 0; i < intranode_count-1; i++) {
            /* non-leaders start at intranode_set[2] ... */
            local_images[i] = current_team->intranode_set[i+2]+1;
        }
    }

    me = leader_index+1;
    p = leaders_count;

    /* find greatest power of 2 less than p */
    q = 1;
    log2_q = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_q = log2_q + 1;
    }

    elem_size = get_reduction_type_size(*elem_type, *charlen);

    k = (sz+1)*elem_size;
    if (collectives_bufsize < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((log2_q+1)*(sz+1)*elem_size);

        num_bufs = MIN(log2_q,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;
    } else if (collectives_bufsize < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(log2_q*(sz+1)*elem_size);

        num_bufs = MIN(log2_q,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = collectives_buffer;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;
    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buffer;
        work_buffers = &((char*)collectives_buffer)[(sz+1)*elem_size];
        num_bufs = ((int)collectives_bufsize-k)/k;
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }
    }

    if (is_leader) {
      partners = malloc(log2_q*sizeof(int));
    }

    /* r is the number of remaining processes, after q */
    r = p - q;

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;


    /* Phase 1: Reduce to Leader */

    if (is_leader) {
        /* leader reads and performs reduction operation for each non-leader
         * */
        int i;
        for (i = 1; i < intranode_count; i++) {
             /* NOTE: assumes that SYNC_IMAGES_HASH is NOT defined  */
            comm_sync_images(&local_images[i-1], 1, NULL, 0, NULL, 0);
            comm_read(current_team->intranode_set[i+1], &((char*)base_buffer)[0],
                      &((char*)work_buffers)[0], sz*elem_size);
            perform_reduce(*op, *elem_type, base_buffer,
                           &((char*)work_buffers)[0], sz, *charlen);
        }

    } else {
        int my_leader_image = current_team->intranode_set[1] + 1;
        comm_sync_images(&my_leader_image, 1, NULL, 0, NULL, 0);
    }

    /* Phase 2: Leaders perform reduction using recursive-doubling */

    if (is_leader) {

        /* last r processes put values to first r processes */
        if (me > q) {
            partner = me - q;
            //proc_id = get_proc_id(current_team, partner);
            proc_id = leader_set[partner-1];

            int image_id = proc_id+1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            if (enable_collectives_use_canary) {

                comm_nbi_write( proc_id,
                        &((char*)work_buffers)[0],
                        &((char*)base_buffer)[0],
                        sz*elem_size+1 );

            } else {


                comm_write_x( proc_id,
                        &((char*)work_buffers)[0],
                        &((char*)base_buffer)[0],
                        sz*elem_size );

                comm_nbi_write( proc_id,
                        &((char*)work_buffers)[sz*elem_size],
                        &((char*)base_buffer)[sz*elem_size],
                        1 );

            }

        } else if (me <= r) {
            partner = me + q;
            proc_id = leader_set[partner-1];

            ((char*)work_buffers)[sz*elem_size] = 0;
            int image_id = proc_id+1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            /* poll on flag */
            comm_poll_char_while_zero(&((char*)work_buffers)[sz*elem_size]);

            /* reduce:
             *   work_buf(1:sz) = work_buf(1:sz) + work_buf(sz+2:2*(sz+1)-1)
             */

            perform_reduce(*op, *elem_type, base_buffer,
                    &((char*)work_buffers)[0],
                    sz, *charlen);
        }

        /* first q processes do recursive doubling algorithm */
        if (me <= q) {
            step = 1;
            for (i = 1; i <= log2_q; i += num_bufs) {
                for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {
                    k2 = j*(sz+1);
                    ((char *)work_buffers)[(k2-1)*elem_size] = 0;
                    if (((me-1)%(2*step)) < step) {
                        partners[j-1] = leader_set[me+step-1] + 1;
                    } else {
                        partners[j-1] = leader_set[me-step-1] + 1;
                    }
                    step = step*2;
                }
                for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {

                    if (j == 1) {
                        comm_sync_images(partners, MIN(num_bufs, log2_q), NULL, 0,
                                NULL, 0);
                    }
                    k1 = (j-1)*(sz+1)+1;
                    k2 = j*(sz+1);
                    proc_id = partners[j-1] - 1;

                    if (enable_collectives_use_canary) {

                        comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                &((char*)base_buffer)[0],
                                sz*elem_size+1 );

                    } else {

                        comm_write_x( proc_id,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                &((char*)base_buffer)[0],
                                sz*elem_size );

                        comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );

                    }


                    /* poll on flag */
                    comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_reduce(*op, *elem_type, base_buffer,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            sz, *charlen);
                }
            }
        }


        /* first r processes put values to last r processes */
        if (me <= r) {
            partner = me + q;
            proc_id = leader_set[partner-1];

            int image_id = proc_id+1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            if (enable_collectives_use_canary) {

                comm_nbi_write( proc_id,
                        &((char*)base_buffer)[0],
                        &((char*)base_buffer)[0],
                        sz*elem_size+1 );

            } else {

                comm_write_x( proc_id,
                        &((char*)base_buffer)[0],
                        &((char*)base_buffer)[0],
                        sz*elem_size );

                comm_nbi_write( proc_id,
                        &((char*)base_buffer)[sz*elem_size],
                        &((char*)base_buffer)[sz*elem_size],
                        1 );

            }

        } else if (me > q) {
            partner = me - q;
            proc_id = leader_set[partner-1];
            ((char*)base_buffer)[sz*elem_size] = 0;
            int image_id = proc_id+1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            /* poll on flag */
            comm_poll_char_while_zero(&((char*)base_buffer)[sz*elem_size]);
        }

    }

    /* Phase 3: Leader writes values back to its non-leaders */

    if (is_leader) {
        int i;
        for (i = 1; i < intranode_count; i++) {
            comm_write_x(current_team->intranode_set[i+1], &((char*)base_buffer)[0],
                      &((char*)base_buffer)[0], sz*elem_size);
             /* NOTE: assumes that SYNC_IMAGES_HASH is NOT defined  */
            comm_sync_images(&local_images[i-1], 1, NULL, 0, NULL, 0);
        }
    } else {
        int my_leader_image = current_team->intranode_set[1] + 1;
        comm_sync_images(&my_leader_image, 1, NULL, 0, NULL, 0);
    }


    memcpy(source, base_buffer, sz*elem_size);

    if (is_leader) {
      free(partners);
      free(local_images);
    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else if (work_buffers_alloc) {
        coarray_deallocate_(work_buffers, NULL);
    }

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

int _IS_CONTIGUOUS(DopeVectorType *source);

INT8 _SIZEOF_8(DopeVectorType *source);

void co_broadcast_from_root_2level(void *source, size_t sz, int source_image);

void co_broadcast_from_root(void *source, size_t sz, int source_image)
{
    int k;
    int i,j, step, val, log2_p;
    int p, q, r, me, partner, proc_id;
    int k1, k2;
    int num_bufs;
    int *pot_partners, *partners;
    void *base_buffer;
    int base_buffer_alloc = 0;

    if (enable_collectives_2level && current_team &&
        (current_team->leaders_count > 1  &&
          current_team->leaders_count < _num_images)) {
        co_broadcast_from_root_2level(source, sz, source_image);
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
        return;

        /* does not reach */
    }

    me = _this_image;
    p = _num_images;

    k = sz;
    if (collectives_bufsize < k) {
        base_buffer = coarray_allocatable_allocate_(sz, NULL, NULL);
        base_buffer_alloc = 1;
    } else {
        base_buffer = collectives_buffer;
    }

    /* find greatest power of 2 */
    q = 1;
    log2_p = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_p = log2_p + 1;
    }
    if (q < p) {
        log2_p = log2_p + 1;
    }

    if (me == source_image) {
        memcpy(base_buffer, source, sz);

        if (me != 1) {
            /* copy from source image to image 1 */
            int root = 1;
            proc_id = get_proc_id(current_team, root);

            comm_nbi_write( proc_id,
                    &((char*)base_buffer)[0],
                    &((char*)base_buffer)[0],
                    sz );
            _SYNC_IMAGES(&root, 1, NULL, 0, NULL, 0);
        }

    } else {
        if (me == 1) {
            _SYNC_IMAGES(&source_image, 1, NULL, 0, NULL, 0);

            /* received from source image */
        }
    }

    step = q;
    while (step > 0) {
        if ((me-1)%step == 0) {
            if (((me-1)%(2*step)) == 0) {
                if ((me+step) <= p) {
                    int p = me+step;
                    proc_id = get_proc_id(current_team, p);
                    comm_nbi_write( proc_id,
                                    &((char*)base_buffer)[0],
                                    &((char*)base_buffer)[0],
                                    sz );
                    _SYNC_IMAGES(&p, 1, NULL, 0, NULL, 0);
                }
            } else {
                if ((me-step) >= 1) {
                    int p = me-step;
                    _SYNC_IMAGES(&p, 1, NULL, 0, NULL, 0);
                }
            }
        }
        step = step / 2;
    }

    /* all images except for source image needs to copy from base_buffer to
     * source */
    if (me != source_image) {
        memcpy(source, base_buffer, sz);
    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    }

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

void co_broadcast_from_root_2level(void *source, size_t sz, int source_image)
{
    int k;
    int i,j, step, val, log2_p;
    int p, q, r, me, partner, proc_id;
    int k1, k2;
    int num_bufs;
    int *pot_partners, *partners;
    void *base_buffer;
    int base_buffer_alloc = 0;

    const int my_proc = current_team->codimension_mapping[_this_image - 1];
    const int is_leader = (current_team->intranode_set[1] == my_proc);
    const long int *leader_set = current_team->leader_set;
    const int leaders_count = current_team->leaders_count;
    const int intranode_count = current_team->intranode_set[0];

    /* find leader index into leaders_set */
    int leader_index = -1;
    if (is_leader) {
        int i;
        for (i = 0; i < leaders_count; i++) {
            if (leader_set[i] == my_proc) {
                leader_index = i;
                break;
            }
        }
    }

    /* compute image indices for other images on same node */
    int *local_images;
    if (is_leader) {
        local_images = malloc((intranode_count-1) * sizeof(*local_images));
        int i;
        for (i = 0; i < intranode_count-1; i++) {
            /* non-leaders start at intranode_set[2] ... */
            local_images[i] = current_team->intranode_set[i+2]+1;
        }
    }

    me = leader_index + 1;
    p = leaders_count;

    k = sz;
    if (collectives_bufsize < k) {
        base_buffer = coarray_allocatable_allocate_(sz, NULL, NULL);
        base_buffer_alloc = 1;
    } else {
        base_buffer = collectives_buffer;
    }

    /* find greatest power of 2 */
    q = 1;
    log2_p = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_p = log2_p + 1;
    }
    if (q < p) {
        log2_p = log2_p + 1;
    }

    if (me == source_image) {
        memcpy(base_buffer, source, sz);

        if (me != 1) {
            /* copy from source image to image 1 */
            int root = 1;
            proc_id = get_proc_id(current_team, root);

            comm_nbi_write( proc_id,
                    &((char*)base_buffer)[0],
                    &((char*)base_buffer)[0],
                    sz );

            _SYNC_IMAGES(&root, 1, NULL, 0, NULL, 0);
        }

    } else {
        if (me == 1) {
            _SYNC_IMAGES(&source_image, 1, NULL, 0, NULL, 0);

            /* received from source image */
        }
    }

    /* Phase 1: Broadcast to other leaders */

    step = q;
    while (step > 0) {
        if ((me-1)%step == 0) {
            if (((me-1)%(2*step)) == 0) {
                if ((me+step) <= p) {
                    int p = me+step;
                    proc_id = get_proc_id(current_team, p);
                    comm_nbi_write( proc_id,
                                    &((char*)base_buffer)[0],
                                    &((char*)base_buffer)[0],
                                    sz );
                    _SYNC_IMAGES(&p, 1, NULL, 0, NULL, 0);
                }
            } else {
                if ((me-step) >= 1) {
                    int p = me-step;
                    _SYNC_IMAGES(&p, 1, NULL, 0, NULL, 0);
                }
            }
        }
        step = step / 2;
    }

    /* Phase 2: Leaders broadcast to their non-leaders */

    if (is_leader) {
        int i;
        for (i = 1; i < intranode_count; i++) {
            comm_write_x(current_team->intranode_set[i+1], &((char*)base_buffer)[0],
                      &((char*)base_buffer)[0], sz);
             /* NOTE: assumes that SYNC_IMAGES_HASH is NOT defined  */
            comm_sync_images(&local_images[i-1], 1, NULL, 0, NULL, 0);
        }
    } else {
        int my_leader_image = current_team->intranode_set[1] + 1;
        comm_sync_images(&my_leader_image, 1, NULL, 0, NULL, 0);
    }

    /* all images except for source image needs to copy from base_buffer to
     * source */
    if (me != source_image) {
        memcpy(source, base_buffer, sz);
    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    }

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}


void CO_BROADCAST__(void *source, INTEGER4 * source_image,
                    INTEGER4 * stat, char * errmsg, DopeVectorType *source_dv,
                    int charlen)
{
    INTEGER8 source_size;
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_BCAST);

#ifdef MPI_AVAIL
    if (!mpi_collectives_available && !enable_collectives_1sided) {
        /* check if MPI was initialized */
        if (MPI_Initialized &&
            MPI_Initialized(&mpi_collectives_available) != MPI_SUCCESS) {
            Error("MPI_Initialized check failed");
        }
    }
#endif

    /* determine if source is contiguous */
    source_size = _SIZEOF_8(source_dv);

#ifdef MPI_AVAIL
    if (mpi_collectives_available && !enable_collectives_1sided &&
        (current_team == NULL || current_team->depth == 0)) {
        /* adding barrier here to ensure communication progress before
         * entering MPI Bcast routine */
        comm_barrier_all();
        MPI_Bcast(source, source_size, MPI_BYTE,
                  *source_image-1, MPI_COMM_WORLD);
    } else
#endif
    {
        co_broadcast_from_root(source, source_size, *source_image);
    }

    PROFILE_FUNC_EXIT(CAFPROF_BCAST);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}



/* translating from image id in 'team' to proc ID */
static int get_proc_id(team_type team, int image_id)
{
    int proc_id = image_id-1;

    if (team!= NULL || team->codimension_mapping != NULL) {
            proc_id = team->codimension_mapping[image_id-1];
    }

    return proc_id;
}


void co_reduce_to_image__(void *source, int *result_image, int *size,
                          int *elem_size_p, int *charlen, int type,
                          void *opr)
{
    int k;
    int p, q, r, me, partner, proc_id;
    int i, j, step, log2_p, val;
    int k1, k2;
    int num_bufs;
    int *pot_partners, *partners;
    size_t elem_size;
    int sz;
    void *base_buffer;
    void *work_buffers;
    int base_buffer_alloc = 0;
    int work_buffers_alloc = 0;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    sz = *size;

    me = _this_image;
    p = _num_images;

    /* find greatest power of 2 */
    q = 1;
    log2_p = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_p = log2_p + 1;
    }
    if (q < p) {
        log2_p = log2_p + 1;
    }

    elem_size = *elem_size_p;

    k = (sz+1)*elem_size;
    if (collectives_bufsize < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((log2_p+1)*(sz+1)*elem_size);

        num_bufs = MIN(log2_p,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size,NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;
    } else if (collectives_bufsize < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(log2_p*(sz+1)*elem_size);

        num_bufs = MIN(log2_p,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = collectives_buffer;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;
    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buffer;
        work_buffers = &((char*)collectives_buffer)[(sz+1)*elem_size];
        num_bufs = ((int)collectives_bufsize-k)/k;
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }
    }


    pot_partners = malloc(log2_p*sizeof(int));
    partners = malloc(log2_p*sizeof(int));

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;

    step = 1;
    for (i = 1; i <= log2_p; i += num_bufs) {
        k = 0;
        for (j = 1; j <= MIN(num_bufs,log2_p-i+1); j++) {
            k2 = j*(sz+1);
            ((char *)work_buffers)[(k2-1)*elem_size] = 0;
            if (((me-1)%(2*step)) < step) {
                pot_partners[j-1] = me+step;
                if ((me+step) <= p) {
                    k = k + 1;
                    partners[k-1] = me+step;
                }
            } else {
                pot_partners[j-1] = me-step;
                if ((me-step) >= 1) {
                    k = k + 1;
                    partners[k-1] = me-step;
                }
            }
            step = step * 2;
        }

        for (j = 1; j <= MIN(num_bufs,log2_p-i+1); j++) {
            if (j == 1 && k > 0) {
                _SYNC_IMAGES(partners, k, NULL, 0, NULL, 0);
            }
            partner = pot_partners[j-1];
            if (partner < 1 || partner > p) continue;

            k1 = (j-1)*(sz+1)+1;
            k2 = j*(sz+1);

            if (me > partner) {
	        proc_id = get_proc_id(current_team, partner);

	        if (enable_collectives_use_canary) {

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                &((char*)base_buffer)[0],
                                sz*elem_size+1 );
                } else {

                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );
                }

            } else {

                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_udr(opr, base_buffer,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            sz, elem_size, *charlen, type);
            }
        }
    }

    if (me == *result_image) {
        ((char*)base_buffer)[sz*elem_size] = 0;
        if (me != 1) {
            int root = 1;
            _SYNC_IMAGES(&root, 1, NULL, 0, NULL, 0);

            /* poll on flag */
            comm_poll_char_while_zero(&((char*)base_buffer)[sz*elem_size]);
        }
        memcpy(source, base_buffer, sz*elem_size);
    } else if (me == 1) {
        _SYNC_IMAGES(result_image, 1, NULL, 0, NULL, 0);
        proc_id = get_proc_id(current_team, *result_image);
        if (enable_collectives_use_canary) {

            comm_nbi_write( proc_id,
                            &((char*)base_buffer)[0],
                            &((char*)base_buffer)[0],
                            sz*elem_size+1 );

        } else {

            comm_write_x( proc_id,
                          &((char*)base_buffer)[0],
                          &((char*)base_buffer)[0],
                          sz*elem_size );

            comm_nbi_write( proc_id,
                            &((char*)base_buffer)[sz*elem_size],
                            &((char*)base_buffer)[sz*elem_size],
                            1 );

        }


    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else if (work_buffers_alloc) {
        coarray_deallocate_(work_buffers, NULL);
    }

    free(pot_partners);
    free(partners);

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

void co_reduce_to_all_2level__(void *source, int *size, int *elem_size_p,
                               int *charlen, int type,
                               void *opr);

void co_reduce_to_all__(void *source, int *size, int *elem_size_p,
                        int *charlen, int type, void *opr)
{
    int k;
    int p, q, r, me, partner, proc_id;
    int i, j, step, log2_q, val;
    int k1, k2;
    int num_bufs;
    int *partners;
    size_t elem_size;
    int sz;
    void *base_buffer;
    void *work_buffers;
    int base_buffer_alloc = 0;
    int work_buffers_alloc = 0;
    int do_mpi;
    int ierr;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    sz = *size;

    if (enable_collectives_2level && current_team &&
        (current_team->leaders_count > 1  &&
          current_team->leaders_count < _num_images) ) {
        co_reduce_to_all_2level__(source, size, elem_size_p, charlen, type, opr);
        PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
        return;

        /* does not reach */
    }

    me = _this_image;
    p = _num_images;

    /* find greatest power of 2 less than p */
    q = 1;
    log2_q = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_q = log2_q + 1;
    }

    elem_size = *elem_size_p;

    k = (sz+1)*elem_size;
    if (collectives_bufsize < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((log2_q+1)*(sz+1)*elem_size);

        num_bufs = MIN(log2_q,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;
    } else if (collectives_bufsize < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(log2_q*(sz+1)*elem_size);

        num_bufs = MIN(log2_q,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = collectives_buffer;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;
    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buffer;
        work_buffers = &((char*)collectives_buffer)[(sz+1)*elem_size];
        num_bufs = ((int)collectives_bufsize-k)/k;
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }
    }

    partners = malloc(log2_q*sizeof(int));

    /* r is the number of remaining processes, after q */
    r = p - q;

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;

    /* last r processes put values to first r processes */
    if (me > q) {
        partner = me - q;
        proc_id = get_proc_id(current_team, partner);

        _SYNC_IMAGES (&partner, 1, NULL, 0, NULL, 0);


        if (enable_collectives_use_canary) {

            comm_nbi_write( proc_id,
                            &((char*)work_buffers)[0],
                            &((char*)base_buffer)[0],
                            sz*elem_size+1 );

        } else {


            comm_write_x( proc_id,
                          &((char*)work_buffers)[0],
                          &((char*)base_buffer)[0],
                          sz*elem_size );

            comm_nbi_write( proc_id,
                            &((char*)work_buffers)[sz*elem_size],
                            &((char*)base_buffer)[sz*elem_size],
                            1 );

        }

    } else if (me <= r) {
        partner = me + q;
        ((char*)work_buffers)[sz*elem_size] = 0;
        _SYNC_IMAGES (&partner, 1, NULL, 0, NULL, 0);

        /* poll on flag */
        comm_poll_char_while_zero(&((char*)work_buffers)[sz*elem_size]);

        /* reduce:
         *   work_buf(1:sz) = work_buf(1:sz) + work_buf(sz+2:2*(sz+1)-1)
         */

        perform_udr(opr, base_buffer,
                &((char*)work_buffers)[0],
                sz, elem_size, *charlen, type);
    }


    /* first q processes do recursive doubling algorithm */
    if (me <= q) {
        step = 1;
        for (i = 1; i <= log2_q; i += num_bufs) {
            for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {
                k2 = j*(sz+1);
                ((char *)work_buffers)[(k2-1)*elem_size] = 0;
                if (((me-1)%(2*step)) < step) {
                    partners[j-1] = me+step;
                } else {
                    partners[j-1] = me-step;
                }
                step = step*2;
            }
            for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {

                if (j == 1) {
                    _SYNC_IMAGES(partners, MIN(num_bufs, log2_q), NULL, 0,
                                 NULL, 0);
                }
                k1 = (j-1)*(sz+1)+1;
                k2 = j*(sz+1);
                partner = partners[j-1];
                proc_id = get_proc_id(current_team, partner);

                if (enable_collectives_use_canary) {

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                &((char*)base_buffer)[0],
                                sz*elem_size+1 );

                } else {

                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );

                }


                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */


                perform_udr(opr, base_buffer,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            sz, elem_size, *charlen, type);
            }
        }
    }


    /* first r processes put values to last r processes */
    if (me <= r) {
        partner = me + q;
        proc_id = get_proc_id(current_team, partner);

        _SYNC_IMAGES (&partner, 1, NULL, 0, NULL, 0);

        if (enable_collectives_use_canary) {

            comm_nbi_write( proc_id,
                            &((char*)base_buffer)[0],
                            &((char*)base_buffer)[0],
                            sz*elem_size+1 );

        } else {

            comm_write_x( proc_id,
                          &((char*)base_buffer)[0],
                          &((char*)base_buffer)[0],
                          sz*elem_size );

            comm_nbi_write( proc_id,
                            &((char*)base_buffer)[sz*elem_size],
                            &((char*)base_buffer)[sz*elem_size],
                            1 );

        }

    } else if (me > q) {
        partner = me - q;
        ((char*)base_buffer)[sz*elem_size] = 0;
        _SYNC_IMAGES (&partner, 1, NULL, 0, NULL, 0);

        /* poll on flag */
        comm_poll_char_while_zero(&((char*)base_buffer)[sz*elem_size]);
    }

    memcpy(source, base_buffer, sz*elem_size);

    free(partners);

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else if (work_buffers_alloc) {
        coarray_deallocate_(work_buffers, NULL);
    }

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

void co_reduce_to_all_2level__(void *source, int *size, int *elem_size_p,
                               int *charlen, int type, void *opr)
{
    int k;
    int p, q, r, me, partner, proc_id;
    int i, j, step, log2_q, val;
    int k1, k2;
    int num_bufs;
    int *partners;
    size_t elem_size;
    int sz;
    void *base_buffer;
    void *work_buffers;
    int base_buffer_alloc = 0;
    int work_buffers_alloc = 0;
    int do_mpi;
    int ierr;


    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    sz = *size;

    const int my_proc = current_team->codimension_mapping[_this_image - 1];
    const int is_leader = (current_team->intranode_set[1] == my_proc);
    const long int *leader_set = current_team->leader_set;
    const int leaders_count = current_team->leaders_count;
    const int intranode_count = current_team->intranode_set[0];

    /* find leader index into leaders_set */
    int leader_index = -1;
    if (is_leader) {
        int i;
        for (i = 0; i < leaders_count; i++) {
            if (leader_set[i] == my_proc) {
                leader_index = i;
                break;
            }
        }
    }

    /* compute image indices for other images on same node */
    int *local_images;
    if (is_leader) {
        local_images = malloc((intranode_count-1) * sizeof(*local_images));
        int i;
        for (i = 0; i < intranode_count-1; i++) {
            /* non-leaders start at intranode_set[2] ... */
            local_images[i] = current_team->intranode_set[i+2]+1;
        }
    }

    me = leader_index+1;
    p = leaders_count;

    /* find greatest power of 2 less than p */
    q = 1;
    log2_q = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_q = log2_q + 1;
    }

    elem_size = *elem_size_p;

    k = (sz+1)*elem_size;
    if (collectives_bufsize < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((log2_q+1)*(sz+1)*elem_size);

        num_bufs = MIN(log2_q,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;
    } else if (collectives_bufsize < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(log2_q*(sz+1)*elem_size);

        num_bufs = MIN(log2_q,((int)largest_slot-k)/k);

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        base_buffer = collectives_buffer;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;
    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buffer;
        work_buffers = &((char*)collectives_buffer)[(sz+1)*elem_size];
        num_bufs = ((int)collectives_bufsize-k)/k;
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }
    }

    if (is_leader) {
      partners = malloc(log2_q*sizeof(int));
    }

    /* r is the number of remaining processes, after q */
    r = p - q;

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;


    /* Phase 1: Reduce to Leader */

    if (is_leader) {
        /* leader reads and performs reduction operation for each non-leader
         * */
        int i;
        for (i = 1; i < intranode_count; i++) {
             /* NOTE: assumes that SYNC_IMAGES_HASH is NOT defined  */
            comm_sync_images(&local_images[i-1], 1, NULL, 0, NULL, 0);
            comm_read(current_team->intranode_set[i+1], &((char*)base_buffer)[0],
                      &((char*)work_buffers)[0], sz*elem_size);

            perform_udr(opr, base_buffer,
                    &((char*)work_buffers)[0],
                    sz, elem_size, *charlen, type);

        }

    } else {
        int my_leader_image = current_team->intranode_set[1] + 1;
        comm_sync_images(&my_leader_image, 1, NULL, 0, NULL, 0);
    }

    /* Phase 2: Leaders perform reduction using recursive-doubling */

    if (is_leader) {

        /* last r processes put values to first r processes */
        if (me > q) {
            partner = me - q;
            //proc_id = get_proc_id(current_team, partner);
            proc_id = leader_set[partner-1];

            int image_id = proc_id+1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            if (enable_collectives_use_canary) {

                comm_nbi_write( proc_id,
                        &((char*)work_buffers)[0],
                        &((char*)base_buffer)[0],
                        sz*elem_size+1 );

            } else {


                comm_write_x( proc_id,
                        &((char*)work_buffers)[0],
                        &((char*)base_buffer)[0],
                        sz*elem_size );

                comm_nbi_write( proc_id,
                        &((char*)work_buffers)[sz*elem_size],
                        &((char*)base_buffer)[sz*elem_size],
                        1 );

            }

        } else if (me <= r) {
            partner = me + q;
            proc_id = leader_set[partner-1];

            ((char*)work_buffers)[sz*elem_size] = 0;
            int image_id = proc_id+1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            /* poll on flag */
            comm_poll_char_while_zero(&((char*)work_buffers)[sz*elem_size]);

            /* reduce:
             *   work_buf(1:sz) = work_buf(1:sz) + work_buf(sz+2:2*(sz+1)-1)
             */

            perform_udr(opr, base_buffer,
                    &((char*)work_buffers)[0],
                    sz, elem_size, *charlen, type);

        }

        /* first q processes do recursive doubling algorithm */
        if (me <= q) {
            step = 1;
            for (i = 1; i <= log2_q; i += num_bufs) {
                for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {
                    k2 = j*(sz+1);
                    ((char *)work_buffers)[(k2-1)*elem_size] = 0;
                    if (((me-1)%(2*step)) < step) {
                        partners[j-1] = leader_set[me+step-1] + 1;
                    } else {
                        partners[j-1] = leader_set[me-step-1] + 1;
                    }
                    step = step*2;
                }
                for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {

                    if (j == 1) {
                        comm_sync_images(partners, MIN(num_bufs, log2_q), NULL, 0,
                                NULL, 0);
                    }
                    k1 = (j-1)*(sz+1)+1;
                    k2 = j*(sz+1);
                    proc_id = partners[j-1] - 1;

                    if (enable_collectives_use_canary) {

                        comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                &((char*)base_buffer)[0],
                                sz*elem_size+1 );

                    } else {

                        comm_write_x( proc_id,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                &((char*)base_buffer)[0],
                                sz*elem_size );

                        comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );

                    }


                    /* poll on flag */
                    comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */


                    perform_udr(opr, base_buffer,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            sz, elem_size, *charlen, type);

                }
            }
        }


        /* first r processes put values to last r processes */
        if (me <= r) {
            partner = me + q;
            //proc_id = get_proc_id(current_team, partner);
            proc_id = leader_set[partner-1];

            int image_id = proc_id+1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            if (enable_collectives_use_canary) {

                comm_nbi_write( proc_id,
                        &((char*)base_buffer)[0],
                        &((char*)base_buffer)[0],
                        sz*elem_size+1 );

            } else {

                comm_write_x( proc_id,
                        &((char*)base_buffer)[0],
                        &((char*)base_buffer)[0],
                        sz*elem_size );

                comm_nbi_write( proc_id,
                        &((char*)base_buffer)[sz*elem_size],
                        &((char*)base_buffer)[sz*elem_size],
                        1 );

            }

        } else if (me > q) {
            partner = me - q;
            proc_id = leader_set[partner-1];
            ((char*)base_buffer)[sz*elem_size] = 0;
            int image_id = proc_id+1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            /* poll on flag */
            comm_poll_char_while_zero(&((char*)base_buffer)[sz*elem_size]);
        }

    }

    /* Phase 3: Leader writes values back to its non-leaders */

    if (is_leader) {
        int i;
        for (i = 1; i < intranode_count; i++) {
            comm_write_x(current_team->intranode_set[i+1], &((char*)base_buffer)[0],
                      &((char*)base_buffer)[0], sz*elem_size);
             /* NOTE: assumes that SYNC_IMAGES_HASH is NOT defined  */
            comm_sync_images(&local_images[i-1], 1, NULL, 0, NULL, 0);
        }
    } else {
        int my_leader_image = current_team->intranode_set[1] + 1;
        comm_sync_images(&my_leader_image, 1, NULL, 0, NULL, 0);
    }


    memcpy(source, base_buffer, sz*elem_size);

    if (is_leader) {
      free(partners);
      free(local_images);
    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else if (work_buffers_alloc) {
        coarray_deallocate_(work_buffers, NULL);
    }

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}


void CO_REDUCE__(void *source, void *opr, INTEGER4 *result_image,
                 INTEGER4 * stat, char * errmsg, DopeVectorType *source_dv,
                 int charlen)
{
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    INTEGER8 nbytes = _SIZEOF_8(source_dv);
    int elem_size;
    int type = source_dv->type_lens.type;

    if (source_dv->type_lens.type == DVTYPE_ASCII) {
        elem_size = source_dv->base_addr.charptr.byte_len;
    } else {
        elem_size = source_dv->base_addr.a.el_len >> 3;
    }

    int nelems = nbytes / elem_size;

    if (result_image == NULL) {
        co_reduce_to_all__(source, &nelems, &elem_size, &charlen, type, opr);
    } else {
        co_reduce_to_image__(source, result_image, &nelems, &elem_size, &charlen,
                             type, opr);
    }

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}
