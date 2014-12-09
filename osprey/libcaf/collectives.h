/*
 Runtime library for supporting Coarray Fortran Collectives

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


#ifndef CAF_COLLECTIVES_H
#define CAF_COLLECTIVES_H

#include "dopevec.h"
#include <stdint.h>

typedef int8_t   INTEGER1;
typedef int16_t  INTEGER2;
typedef int32_t  INTEGER4;
typedef int64_t  INTEGER8;

typedef union BYTE1_u {
    INTEGER1 as_INTEGER1;
    char as_char;
} BYTE1;

typedef union BYTE2_u {
    INTEGER2 as_INTEGER2;
} BYTE2;

typedef union BYTE4_u {
    INTEGER4 as_INTEGER4;
    float as_float;
} BYTE4;

typedef union BYTE8_u {
    double as_double;
    INTEGER8 as_INTEGER8;
} BYTE8;

typedef struct BYTE16_s {
    union BYTE16_u {
        BYTE1 as_BYTE1[16];
        BYTE2 as_BYTE2[8];
        BYTE4 as_BYTE4[4];
        BYTE8 as_BYTE8[2];
    } value;
} INTEGER16;

typedef INTEGER16 BYTE16;

typedef float REAL4;
typedef double REAL8;
typedef long double REAL16;

typedef struct COMPLEX8_s {
    REAL4 real;
    REAL4 imag;
} COMPLEX8;

typedef struct COMPLEX16_s {
    REAL8 real;
    REAL8 imag;
} COMPLEX16;


typedef enum {
    CO_REDUCE_ALL2ALL = 1,
    CO_REDUCE_2TREE_SYNCALL = 2,
    CO_REDUCE_2TREE_SYNCIMAGES = 3,
    CO_REDUCE_2TREE_EVENTS = 4,
    CO_REDUCE_DEFAULT = 3
} co_reduce_t;

typedef enum {
    CAF_UNKNOWN   = 0,
    CAF_INT1      = 1,
    CAF_INT2      = 2,
    CAF_INT4      = 3,
    CAF_INT8      = 4,
    CAF_REAL4     = 5,
    CAF_REAL8     = 6,
    CAF_REAL16    = 7,
    CAF_COMPLEX4  = 8,
    CAF_COMPLEX8  = 9,
    CAF_COMPLEX16 = 10,
    CAF_LOGICAL1  = 11,
    CAF_LOGICAL2  = 12,
    CAF_LOGICAL4  = 13,
    CAF_LOGICAL8  = 14,
    CAF_CHAR      = 15
} caf_reduction_type_t;

typedef enum {
    CAF_SUM       = 1,
    CAF_MIN       = 2,
    CAF_MAX       = 3,
    CAF_PROD      = 4,
} caf_reduction_op_t;

/* Collectives */

void co_reduce_predef_to_image__( void *source, int *result_image, int *size,
                                 int *charlen, caf_reduction_type_t *elem_type,
                                 caf_reduction_op_t *op);

void co_reduce_predef_to_all__( void *source, int *size, int *charlen,
                               caf_reduction_type_t *elem_type,
                               caf_reduction_op_t *op);


void CO_BROADCAST__(void *source, INTEGER4 * source_image,
                    INTEGER4 * stat, char * errmsg, DopeVectorType *source_dv,
                    int charlen);

void co_broadcast_from_root(void *source, size_t sz, int source_image);

/* CO_BCAST */

void _CO_BCAST_I1(DopeVectorType * source, INTEGER1 * src_img_p);
void _CO_BCAST_I2(DopeVectorType * source, INTEGER2 * src_img_p);
void _CO_BCAST_I4(DopeVectorType * source, INTEGER4 * src_img_p);
void _CO_BCAST_I8(DopeVectorType * source, INTEGER8 * src_img_p);

/* CO_MAXVAL */

void _CO_MAXVAL_INT1_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT2_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT1(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT2(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT4(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT8(DopeVectorType * source, DopeVectorType * result);

void _CO_MAXVAL_REAL4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_REAL8_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_MAXVAL_REAL16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_REAL4(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_REAL8(DopeVectorType * source, DopeVectorType * result);
//void _CO_MAXVAL_REAL16(DopeVectorType * source, DopeVectorType * result);

//void _CO_MAXVAL_C4_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_MAXVAL_C8_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_MAXVAL_C16_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_MAXVAL_C4(DopeVectorType * source, DopeVectorType * result);
//void _CO_MAXVAL_C8(DopeVectorType * source, DopeVectorType * result);
//void _CO_MAXVAL_C16(DopeVectorType * source, DopeVectorType * result);

/* CO_MINVAL */

void _CO_MINVAL_INT1_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT2_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT1(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT2(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT4(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT8(DopeVectorType * source, DopeVectorType * result);

void _CO_MINVAL_REAL4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_REAL8_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_MINVAL_REAL16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_REAL4(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_REAL8(DopeVectorType * source, DopeVectorType * result);
//void _CO_MINVAL_REAL16(DopeVectorType * source, DopeVectorType * result);

//void _CO_MINVAL_C4_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_MINVAL_C8_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_MINVAL_C16_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_MINVAL_C4(DopeVectorType * source, DopeVectorType * result);
//void _CO_MINVAL_C8(DopeVectorType * source, DopeVectorType * result);
//void _CO_MINVAL_C16(DopeVectorType * source, DopeVectorType * result);

/* CO_SUM */

void _CO_SUM_INT1_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT2_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT1(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT2(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT4(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT8(DopeVectorType * source, DopeVectorType * result);

void _CO_SUM_REAL4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_REAL8_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_SUM_REAL16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_REAL4(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_REAL8(DopeVectorType * source, DopeVectorType * result);
//void _CO_SUM_REAL16(DopeVectorType * source, DopeVectorType * result);

void _CO_SUM_C4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_C8_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_SUM_C16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_C4(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_C8(DopeVectorType * source, DopeVectorType * result);
//void _CO_SUM_C16(DopeVectorType * source, DopeVectorType * result);

/* CO_PRODUCT */

void _CO_PRODUCT_INT1_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT2_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT1(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT2(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT4(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT8(DopeVectorType * source, DopeVectorType * result);

void _CO_PRODUCT_REAL4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_REAL8_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_PRODUCT_REAL16_0(DopeVectorType * source, DopeVectorType* result);
void _CO_PRODUCT_REAL4(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_REAL8(DopeVectorType * source, DopeVectorType * result);
//void _CO_PRODUCT_REAL16(DopeVectorType * source, DopeVectorType * result);

void _CO_PRODUCT_C4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_C8_0(DopeVectorType * source, DopeVectorType * result);
//void _CO_PRODUCT_C16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_C4(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_C8(DopeVectorType * source, DopeVectorType * result);
//void _CO_PRODUCT_C16(DopeVectorType * source, DopeVectorType * result);

/* INTERFACES FOR EXTERNAL CAF IMPLEMENTATION */

#define FORTRAN_CO_BCAST_(type) \
    extern void co_bcast_##type##_0__(DopeVectorType *source,  \
                size_t source_image, size_t size); \
    extern void co_bcast_##type##_1__(DopeVectorType *source,  \
                size_t source_image, size_t size); \
    extern void co_bcast_##type##_2__(DopeVectorType *source,  \
                size_t source_image, size_t size); \
    extern void co_bcast_##type##_3__(DopeVectorType *source,  \
                size_t source_image, size_t size); \
    extern void co_bcast_##type##_4__(DopeVectorType *source,  \
                size_t source_image, size_t size); \
    extern void co_bcast_##type##_5__(DopeVectorType *source,  \
                size_t source_image, size_t size); \
    extern void co_bcast_##type##_6__(DopeVectorType *source,  \
                size_t source_image, size_t size); \
    extern void co_bcast_##type##_7__(DopeVectorType *source,  \
                size_t source_image, size_t size);

    FORTRAN_CO_BCAST_(int1)
    FORTRAN_CO_BCAST_(int2)
    FORTRAN_CO_BCAST_(int4)
    FORTRAN_CO_BCAST_(int8)

#define FORTRAN_CO_REDUCE_(op, alg, type) \
    extern void co_##op##_##alg##_##type##_0__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##alg##_##type##_1__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##alg##_##type##_2__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##alg##_##type##_3__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##alg##_##type##_4__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##alg##_##type##_5__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##alg##_##type##_6__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##alg##_##type##_7__(DopeVectorType *source,  \
                DopeVectorType *result);

    FORTRAN_CO_REDUCE_(maxval, all2all, int1)
    FORTRAN_CO_REDUCE_(maxval, all2all, int2)
    FORTRAN_CO_REDUCE_(maxval, all2all, int4)
    FORTRAN_CO_REDUCE_(maxval, all2all, int8)
    FORTRAN_CO_REDUCE_(maxval, all2all, real4)
    FORTRAN_CO_REDUCE_(maxval, all2all, real8)
    // FORTRAN_CO_REDUCE_(maxval, all2all, real16)
    // FORTRAN_CO_REDUCE_(maxval, all2all, c4)
    // FORTRAN_CO_REDUCE_(maxval, all2all, c8)
    // FORTRAN_CO_REDUCE_(maxval, all2all, c16)

    FORTRAN_CO_REDUCE_(minval, all2all, int1)
    FORTRAN_CO_REDUCE_(minval, all2all, int2)
    FORTRAN_CO_REDUCE_(minval, all2all, int4)
    FORTRAN_CO_REDUCE_(minval, all2all, int8)
    FORTRAN_CO_REDUCE_(minval, all2all, real4)
    FORTRAN_CO_REDUCE_(minval, all2all, real8)
    // FORTRAN_CO_REDUCE_(minval, all2all, real16)
    // FORTRAN_CO_REDUCE_(minval, all2all, c4)
    // FORTRAN_CO_REDUCE_(minval, all2all, c8)
    // FORTRAN_CO_REDUCE_(minval, all2all, c16)

    FORTRAN_CO_REDUCE_(sum, all2all, int1)
    FORTRAN_CO_REDUCE_(sum, all2all, int2)
    FORTRAN_CO_REDUCE_(sum, all2all, int4)
    FORTRAN_CO_REDUCE_(sum, all2all, int8)
    FORTRAN_CO_REDUCE_(sum, all2all, real4)
    FORTRAN_CO_REDUCE_(sum, all2all, real8)
    // FORTRAN_CO_REDUCE_(sum, all2all, real16)
    FORTRAN_CO_REDUCE_(sum, all2all, c4)
    FORTRAN_CO_REDUCE_(sum, all2all, c8)
    // FORTRAN_CO_REDUCE_(sum, all2all, c16)

    FORTRAN_CO_REDUCE_(product, all2all, int1)
    FORTRAN_CO_REDUCE_(product, all2all, int2)
    FORTRAN_CO_REDUCE_(product, all2all, int4)
    FORTRAN_CO_REDUCE_(product, all2all, int8)
    FORTRAN_CO_REDUCE_(product, all2all, real4)
    FORTRAN_CO_REDUCE_(product, all2all, real8)
    // FORTRAN_CO_REDUCE_(product, all2all, real16)
    FORTRAN_CO_REDUCE_(product, all2all, c4)
    FORTRAN_CO_REDUCE_(product, all2all, c8)
    // FORTRAN_CO_REDUCE_(product, all2all, c16)

    FORTRAN_CO_REDUCE_(maxval, 2tree_syncall, int1)
    FORTRAN_CO_REDUCE_(maxval, 2tree_syncall, int2)
    FORTRAN_CO_REDUCE_(maxval, 2tree_syncall, int4)
    FORTRAN_CO_REDUCE_(maxval, 2tree_syncall, int8)
    FORTRAN_CO_REDUCE_(maxval, 2tree_syncall, real4)
    FORTRAN_CO_REDUCE_(maxval, 2tree_syncall, real8)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_syncall, real16)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_syncall, c4)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_syncall, c8)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_syncall, c16)

    FORTRAN_CO_REDUCE_(minval, 2tree_syncall, int1)
    FORTRAN_CO_REDUCE_(minval, 2tree_syncall, int2)
    FORTRAN_CO_REDUCE_(minval, 2tree_syncall, int4)
    FORTRAN_CO_REDUCE_(minval, 2tree_syncall, int8)
    FORTRAN_CO_REDUCE_(minval, 2tree_syncall, real4)
    FORTRAN_CO_REDUCE_(minval, 2tree_syncall, real8)
    // FORTRAN_CO_REDUCE_(minval, 2tree_syncall, real16)
    // FORTRAN_CO_REDUCE_(minval, 2tree_syncall, c4)
    // FORTRAN_CO_REDUCE_(minval, 2tree_syncall, c8)
    // FORTRAN_CO_REDUCE_(minval, 2tree_syncall, c16)

    FORTRAN_CO_REDUCE_(sum, 2tree_syncall, int1)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncall, int2)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncall, int4)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncall, int8)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncall, real4)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncall, real8)
    // FORTRAN_CO_REDUCE_(sum, 2tree_syncall, real16)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncall, c4)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncall, c8)
    // FORTRAN_CO_REDUCE_(sum, 2tree_syncall, c16)

    FORTRAN_CO_REDUCE_(product, 2tree_syncall, int1)
    FORTRAN_CO_REDUCE_(product, 2tree_syncall, int2)
    FORTRAN_CO_REDUCE_(product, 2tree_syncall, int4)
    FORTRAN_CO_REDUCE_(product, 2tree_syncall, int8)
    FORTRAN_CO_REDUCE_(product, 2tree_syncall, real4)
    FORTRAN_CO_REDUCE_(product, 2tree_syncall, real8)
    // FORTRAN_CO_REDUCE_(product, 2tree_syncall, real16)
    FORTRAN_CO_REDUCE_(product, 2tree_syncall, c4)
    FORTRAN_CO_REDUCE_(product, 2tree_syncall, c8)
    // FORTRAN_CO_REDUCE_(product, 2tree_syncall, c16)

    FORTRAN_CO_REDUCE_(maxval, 2tree_syncimages, int1)
    FORTRAN_CO_REDUCE_(maxval, 2tree_syncimages, int2)
    FORTRAN_CO_REDUCE_(maxval, 2tree_syncimages, int4)
    FORTRAN_CO_REDUCE_(maxval, 2tree_syncimages, int8)
    FORTRAN_CO_REDUCE_(maxval, 2tree_syncimages, real4)
    FORTRAN_CO_REDUCE_(maxval, 2tree_syncimages, real8)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_syncimages, real16)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_syncimages, c4)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_syncimages, c8)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_syncimages, c16)

    FORTRAN_CO_REDUCE_(minval, 2tree_syncimages, int1)
    FORTRAN_CO_REDUCE_(minval, 2tree_syncimages, int2)
    FORTRAN_CO_REDUCE_(minval, 2tree_syncimages, int4)
    FORTRAN_CO_REDUCE_(minval, 2tree_syncimages, int8)
    FORTRAN_CO_REDUCE_(minval, 2tree_syncimages, real4)
    FORTRAN_CO_REDUCE_(minval, 2tree_syncimages, real8)
    // FORTRAN_CO_REDUCE_(minval, 2tree_syncimages, real16)
    // FORTRAN_CO_REDUCE_(minval, 2tree_syncimages, c4)
    // FORTRAN_CO_REDUCE_(minval, 2tree_syncimages, c8)
    // FORTRAN_CO_REDUCE_(minval, 2tree_syncimages, c16)

    FORTRAN_CO_REDUCE_(sum, 2tree_syncimages, int1)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncimages, int2)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncimages, int4)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncimages, int8)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncimages, real4)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncimages, real8)
    // FORTRAN_CO_REDUCE_(sum, 2tree_syncimages, real16)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncimages, c4)
    FORTRAN_CO_REDUCE_(sum, 2tree_syncimages, c8)
    // FORTRAN_CO_REDUCE_(sum, 2tree_syncimages, c16)

    FORTRAN_CO_REDUCE_(product, 2tree_syncimages, int1)
    FORTRAN_CO_REDUCE_(product, 2tree_syncimages, int2)
    FORTRAN_CO_REDUCE_(product, 2tree_syncimages, int4)
    FORTRAN_CO_REDUCE_(product, 2tree_syncimages, int8)
    FORTRAN_CO_REDUCE_(product, 2tree_syncimages, real4)
    FORTRAN_CO_REDUCE_(product, 2tree_syncimages, real8)
    // FORTRAN_CO_REDUCE_(product, 2tree_syncimages, real16)
    FORTRAN_CO_REDUCE_(product, 2tree_syncimages, c4)
    FORTRAN_CO_REDUCE_(product, 2tree_syncimages, c8)
    // FORTRAN_CO_REDUCE_(product, 2tree_syncimages, c16)

    FORTRAN_CO_REDUCE_(maxval, 2tree_events, int1)
    FORTRAN_CO_REDUCE_(maxval, 2tree_events, int2)
    FORTRAN_CO_REDUCE_(maxval, 2tree_events, int4)
    FORTRAN_CO_REDUCE_(maxval, 2tree_events, int8)
    FORTRAN_CO_REDUCE_(maxval, 2tree_events, real4)
    FORTRAN_CO_REDUCE_(maxval, 2tree_events, real8)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_events, real16)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_events, c4)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_events, c8)
    // FORTRAN_CO_REDUCE_(maxval, 2tree_events, c16)

    FORTRAN_CO_REDUCE_(minval, 2tree_events, int1)
    FORTRAN_CO_REDUCE_(minval, 2tree_events, int2)
    FORTRAN_CO_REDUCE_(minval, 2tree_events, int4)
    FORTRAN_CO_REDUCE_(minval, 2tree_events, int8)
    FORTRAN_CO_REDUCE_(minval, 2tree_events, real4)
    FORTRAN_CO_REDUCE_(minval, 2tree_events, real8)
    // FORTRAN_CO_REDUCE_(minval, 2tree_events, real16)
    // FORTRAN_CO_REDUCE_(minval, 2tree_events, c4)
    // FORTRAN_CO_REDUCE_(minval, 2tree_events, c8)
    // FORTRAN_CO_REDUCE_(minval, 2tree_events, c16)

    FORTRAN_CO_REDUCE_(sum, 2tree_events, int1)
    FORTRAN_CO_REDUCE_(sum, 2tree_events, int2)
    FORTRAN_CO_REDUCE_(sum, 2tree_events, int4)
    FORTRAN_CO_REDUCE_(sum, 2tree_events, int8)
    FORTRAN_CO_REDUCE_(sum, 2tree_events, real4)
    FORTRAN_CO_REDUCE_(sum, 2tree_events, real8)
    // FORTRAN_CO_REDUCE_(sum, 2tree_events, real16)
    FORTRAN_CO_REDUCE_(sum, 2tree_events, c4)
    FORTRAN_CO_REDUCE_(sum, 2tree_events, c8)
    // FORTRAN_CO_REDUCE_(sum, 2tree_events, c16)

    FORTRAN_CO_REDUCE_(product, 2tree_events, int1)
    FORTRAN_CO_REDUCE_(product, 2tree_events, int2)
    FORTRAN_CO_REDUCE_(product, 2tree_events, int4)
    FORTRAN_CO_REDUCE_(product, 2tree_events, int8)
    FORTRAN_CO_REDUCE_(product, 2tree_events, real4)
    FORTRAN_CO_REDUCE_(product, 2tree_events, real8)
    // FORTRAN_CO_REDUCE_(product, 2tree_events, real16)
    FORTRAN_CO_REDUCE_(product, 2tree_events, c4)
    FORTRAN_CO_REDUCE_(product, 2tree_events, c8)
    // FORTRAN_CO_REDUCE_(product, 2tree_events, c16)

#endif                          /* CAF_COLLECTIVES_H */
