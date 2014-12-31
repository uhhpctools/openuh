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

void co_gather_to_all__(void *source, void *dest, int size, int elem_size_p);

void co_broadcast_from_root(void *source, size_t sz, int source_image);

/* call from outside caf-runtime */


void co_reduce_predef_to_image__( void *source, int *result_image, int *size,
                                 int *charlen, caf_reduction_type_t *elem_type,
                                 caf_reduction_op_t *op, INTEGER4 *stat,
                                 char *errmsg, int errmsg_len);

void co_reduce_predef_to_all__( void *source, int *size, int *charlen,
                               caf_reduction_type_t *elem_type,
                               caf_reduction_op_t *op, INTEGER4 *stat,
                               char *errmsg, int errmsg_len);

void CO_BROADCAST__(void *source, INTEGER4 * source_image,
                    INTEGER4 * stat, char * errmsg, DopeVectorType *source_dv,
                    int errmsg_len);

void CO_REDUCE__(void *source, void *opr, INTEGER4 *result_image,
                 INTEGER4 * stat, char * errmsg, DopeVectorType *source_dv,
                 int charlen);


#endif                          /* CAF_COLLECTIVES_H */
