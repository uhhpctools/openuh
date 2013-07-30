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

typedef char INTEGER1;
typedef short INTEGER2;
typedef int INTEGER4;
typedef size_t INTEGER8;

/* Collectives */
//void comm_cosum(DopeVectorType *src_dv, DopeVectorType *sum_dv,int root);

/* CO_BCAST */
void _CO_BCAST_I1(DopeVectorType * source, INTEGER1 * src_img_p);
void _CO_BCAST_I2(DopeVectorType * source, INTEGER2 * src_img_p);
void _CO_BCAST_I4(DopeVectorType * source, INTEGER4 * src_img_p);
void _CO_BCAST_I8(DopeVectorType * source, INTEGER8 * src_img_p);

/* CO_MAXVAL */

void _CO_MAXVAL_INT1_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT1(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT2_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT2(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT4(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_INT8(DopeVectorType * source, DopeVectorType * result);

void _CO_MAXVAL_REAL4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_REAL4(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_REAL8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_REAL8(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_REAL16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_REAL16(DopeVectorType * source, DopeVectorType * result);

void _CO_MAXVAL_C4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_C4(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_C8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_C8(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_C16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MAXVAL_C16(DopeVectorType * source, DopeVectorType * result);

/* CO_MINVAL */

void _CO_MINVAL_INT1_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT1(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT2_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT2(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT4(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_INT8(DopeVectorType * source, DopeVectorType * result);

void _CO_MINVAL_REAL4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_REAL4(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_REAL8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_REAL8(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_REAL16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_REAL16(DopeVectorType * source, DopeVectorType * result);

void _CO_MINVAL_C4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_C4(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_C8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_C8(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_C16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_MINVAL_C16(DopeVectorType * source, DopeVectorType * result);

/* CO_SUM */

void _CO_SUM_INT1_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT1(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT2_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT2(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT4(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_INT8(DopeVectorType * source, DopeVectorType * result);

void _CO_SUM_REAL4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_REAL4(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_REAL8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_REAL8(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_REAL16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_REAL16(DopeVectorType * source, DopeVectorType * result);

void _CO_SUM_C4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_C4(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_C8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_C8(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_C16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_SUM_C16(DopeVectorType * source, DopeVectorType * result);

/* CO_PRODUCT */

void _CO_PRODUCT_INT1_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT1(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT2_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT2(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT4(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_INT8(DopeVectorType * source, DopeVectorType * result);

void _CO_PRODUCT_REAL4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_REAL4(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_REAL8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_REAL8(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_REAL16_0(DopeVectorType * source,
                          DopeVectorType * result);
void _CO_PRODUCT_REAL16(DopeVectorType * source, DopeVectorType * result);

void _CO_PRODUCT_C4_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_C4(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_C8_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_C8(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_C16_0(DopeVectorType * source, DopeVectorType * result);
void _CO_PRODUCT_C16(DopeVectorType * source, DopeVectorType * result);

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
#define FORTRAN_CO_REDUCE_(op, type) \
    extern void co_##op##_##type##_0__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##type##_1__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##type##_2__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##type##_3__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##type##_4__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##type##_5__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##type##_6__(DopeVectorType *source,  \
                DopeVectorType *result); \
    extern void co_##op##_##type##_7__(DopeVectorType *source,  \
                DopeVectorType *result);
    FORTRAN_CO_REDUCE_(maxval, int1)
    FORTRAN_CO_REDUCE_(maxval, int2)
    FORTRAN_CO_REDUCE_(maxval, int4)
    FORTRAN_CO_REDUCE_(maxval, int8)
    FORTRAN_CO_REDUCE_(maxval, real4)
    FORTRAN_CO_REDUCE_(maxval, real8)
// FORTRAN_CO_REDUCE_(maxval, real16)
// FORTRAN_CO_REDUCE_(maxval, c4)
// FORTRAN_CO_REDUCE_(maxval, c8)
// FORTRAN_CO_REDUCE_(maxval, c16)
    FORTRAN_CO_REDUCE_(minval, int1)
    FORTRAN_CO_REDUCE_(minval, int2)
    FORTRAN_CO_REDUCE_(minval, int4)
    FORTRAN_CO_REDUCE_(minval, int8)
    FORTRAN_CO_REDUCE_(minval, real4)
    FORTRAN_CO_REDUCE_(minval, real8)
// FORTRAN_CO_REDUCE_(minval, real16)
// FORTRAN_CO_REDUCE_(minval, c4)
// FORTRAN_CO_REDUCE_(minval, c8)
// FORTRAN_CO_REDUCE_(minval, c16)
    FORTRAN_CO_REDUCE_(sum, int1)
    FORTRAN_CO_REDUCE_(sum, int2)
    FORTRAN_CO_REDUCE_(sum, int4)
    FORTRAN_CO_REDUCE_(sum, int8)
    FORTRAN_CO_REDUCE_(sum, real4)
    FORTRAN_CO_REDUCE_(sum, real8)
// FORTRAN_CO_REDUCE_(sum, real16)
    FORTRAN_CO_REDUCE_(sum, c4)
    FORTRAN_CO_REDUCE_(sum, c8)
// FORTRAN_CO_REDUCE_(sum, c16)
    FORTRAN_CO_REDUCE_(product, int1)
    FORTRAN_CO_REDUCE_(product, int2)
    FORTRAN_CO_REDUCE_(product, int4)
    FORTRAN_CO_REDUCE_(product, int8)
    FORTRAN_CO_REDUCE_(product, real4)
    FORTRAN_CO_REDUCE_(product, real8)
// FORTRAN_CO_REDUCE_(product, real16)
    FORTRAN_CO_REDUCE_(product, c4)
    FORTRAN_CO_REDUCE_(product, c8)
// FORTRAN_CO_REDUCE_(product, c16)
#endif                          /* CAF_COLLECTIVES_H */
