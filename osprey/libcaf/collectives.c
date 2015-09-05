/*

 Runtime library for supporting Coarray Fortran

 Copyright (C) 2009-2014 University of Houston.

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


/* global variables */
void *collectives_buffer[2];
size_t collectives_bufsize;

void *allreduce_buffer[2];
size_t allreduce_bufsize;

void *reduce_buffer[2];
size_t reduce_bufsize;

void *broadcast_buffer[2];
size_t broadcast_bufsize;

int collectives_max_workbufs = 0;

int enable_collectives_mpi;
int enable_collectives_use_canary = 0;

int enable_collectives_2level = 0;
int enable_reduction_2level = 0;
int enable_broadcast_2level = 0;
int mpi_collectives_available = 0;


/* initialized in comm_init() */
extern unsigned long _this_image;
extern unsigned long _num_images;

/* from team.c */
extern team_type current_team;

/* these are defined in libcaf-extra/reduce.caf */

extern void sum_reduce_int1__(void *, void *, int);
extern void sum_reduce_int2__(void *, void *, int);
extern void sum_reduce_int4__(void *, void *, int);
extern void sum_reduce_int8__(void *, void *, int);
extern void sum_reduce_real4__(void *, void *, int);
extern void sum_reduce_real8__(void *, void *, int);
extern void sum_reduce_complex4__(void *, void *, int);
extern void sum_reduce_complex8__(void *, void *, int);

extern void max_reduce_int1__(void *, void *, int);
extern void max_reduce_int2__(void *, void *, int);
extern void max_reduce_int4__(void *, void *, int);
extern void max_reduce_int8__(void *, void *, int);
extern void max_reduce_real4__(void *, void *, int);
extern void max_reduce_real8__(void *, void *, int);
extern void max_reduce_char__(void *, void *, int, int, int);

extern void min_reduce_int1__(void *, void *, int);
extern void min_reduce_int2__(void *, void *, int);
extern void min_reduce_int4__(void *, void *, int);
extern void min_reduce_int8__(void *, void *, int);
extern void min_reduce_real4__(void *, void *, int);
extern void min_reduce_real8__(void *, void *, int);
extern void min_reduce_char__(void *, void *, int, int, int);

extern void prod_reduce_int1__(void *, void *, int);
extern void prod_reduce_int2__(void *, void *, int);
extern void prod_reduce_int4__(void *, void *, int);
extern void prod_reduce_int8__(void *, void *, int);
extern void prod_reduce_real4__(void *, void *, int);
extern void prod_reduce_real8__(void *, void *, int);
extern void prod_reduce_complex4__(void *, void *, int);
extern void prod_reduce_complex8__(void *, void *, int);

/* defined in libfortran runtime */
extern int _IS_CONTIGUOUS(DopeVectorType *source);
extern INT8 _SIZEOF_8(DopeVectorType *source);

/* local forward declarations */
static int inline get_proc_id(team_type team, int image_id);
static size_t inline get_reduction_type_size(caf_reduction_type_t type,
                           int charlen);
static void inline perform_reduce(caf_reduction_op_t op, caf_reduction_type_t type,
                           void *res, void *b, int s, int charlen);
static void perform_udr(void *opr, char *res, char *b, int s, int elem_size,
                        int charlen, int type);

#ifdef MPI_AVAIL
static void inline do_mpi_reduce(void *source, int size, MPI_Datatype dtype, MPI_Op op,
                   int root);
static void do_mpi_char_reduce(void *source, int size, caf_reduction_type_t dtype,
                        caf_reduction_op_t op, int root, int charlen);
static void do_mpi_char_allreduce(void *source, int size, caf_reduction_type_t dtype,
                        caf_reduction_op_t op, int charlen);
#endif

static void co_reduce_predef_to_image_2level__(void *source, int *result_image,
                               int *size, int *charlen,
                               caf_reduction_type_t *elem_type,
                               caf_reduction_op_t *op);

static void co_reduce_predef_to_all_2level__(void *source, int *size, int *charlen,
                               caf_reduction_type_t *elem_type,
                               caf_reduction_op_t *op);

static void co_broadcast_from_root_2level(void *source, size_t sz, int source_image);

static void co_reduce_to_image__(void *source, int *result_image, int *size,
                          int *elem_size_p, int *charlen, int type,
                          void *opr);
static void co_reduce_to_image_2level__(void *source, int *result_image, int *size,
                          int *elem_size_p, int *charlen, int type,
                          void *opr);
static void co_reduce_to_all__(void *source, int *size, int *elem_size_p,
                        int *charlen, int type, void *opr);
static void co_reduce_to_all_2level__(void *source, int *size, int *elem_size_p,
                               int *charlen, int type, void *opr);



#define MIN(x,y) ( (x<y) ? (x) : (y) )
#define MAX(x,y) ( (x>y) ? (x) : (y) )

#define NEW_ALLREDUCE
#define NEW_REDUCE
#define NEW_BCAST
#define BINOMIAL_INTRANODE_BCAST


/* translating from image id in 'team' to proc ID */
static int inline get_proc_id(team_type team, int image_id)
{
    int proc_id = image_id-1;

    if (team!= NULL || team->codimension_mapping != NULL) {
            proc_id = team->codimension_mapping[image_id-1];
    }

    return proc_id;
}

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
            elem_size = 4;
            break;
        case CAF_LOGICAL8:
        case CAF_INT8:
        case CAF_REAL8:
        case CAF_COMPLEX4:
            elem_size = 8;
            break;
        case CAF_REAL16:
        case CAF_COMPLEX8:
            elem_size = 16;
            break;
        case CAF_COMPLEX16:
            elem_size = 32;
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

static void inline do_mpi_reduce(void *source, int size, MPI_Datatype dtype, MPI_Op op,
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

static void do_mpi_char_reduce(void *source, int size, caf_reduction_type_t dtype,
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

static void do_mpi_char_allreduce(void *source, int size, caf_reduction_type_t dtype,
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
                                 caf_reduction_op_t *op, INTEGER4 *stat,
                                 char *errmsg, int errmsg_len)
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
    void *collectives_buf;
    size_t collectives_bufsz;
    int bufid;
    int pindex = 0;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    sz = *size;

#ifdef MPI_AVAIL
    if (!mpi_collectives_available && enable_collectives_mpi) {
        /* check if MPI was initialized */
        if (MPI_Initialized &&
            MPI_Initialized(&mpi_collectives_available) != MPI_SUCCESS) {
            Error("MPI_Initialized check failed");
        }
    }

    if (mpi_collectives_available && enable_collectives_mpi &&
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

    if ((enable_collectives_2level || enable_reduction_2level) && current_team) {
        co_reduce_predef_to_image_2level__(source, result_image, size, charlen,
                                           elem_type, op);
        PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
        return;

        /* does not reach */
    }

    if (current_team != NULL) {
        bufid = current_team->reduce_bufid;
        collectives_buf = reduce_buffer[bufid];
        collectives_bufsz = reduce_bufsize;
    } else {
        Error("current_team is not set!");
    }

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

    if (collectives_bufsz < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        const int num_steps = log2_p;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((num_steps+1)*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb-1;

        if (collectives_max_workbufs >= 1) {
              num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_p) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;

    } else if (collectives_bufsz < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const int num_steps = log2_p;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(num_steps*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_p) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = collectives_buf;

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;

    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buf;
        work_buffers = &((char*)collectives_buf)[(sz+1)*elem_size];
        num_bufs = MIN(((int)collectives_bufsz-k)/k, log2_p);
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }
    }

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;

#ifndef NEW_REDUCE
    pot_partners = malloc(log2_p*sizeof(int));
    partners = malloc(log2_p*sizeof(int));

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

                if (0 /*enable_collectives_use_canary*/) {

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

#else

    if (!base_buffer_alloc)
        partners = malloc(log2_p*sizeof(int));

    if (num_bufs > 1) {
        step = 1;
        for (i = 1; i <= log2_p; i += num_bufs) {
            /* send sync notifications prior to each round except the first
             * one */
            if (i > 1) {
                int s = step;
                for (j = 1; j <= MIN(num_bufs,log2_p-i+1); j++) {
                    partner = 0;
                    if (((me-1)%(2*s)) == 0) {
                        if ((me+s) <= p) {
                            partner = me+s;
                        }
                    } else if (((me-1)%(2*s)) == s) {
                        if ((me-s) >= 1) {
                            partner = me-s;
                        }
                    }

                    s = s * 2;
                    if (partner < 1 || partner > p) continue;

                    if (me < partner) {
                        /* notify partner I'm ready to receive data */
                        coll_flags_t val = 1;

                        proc_id = get_proc_id(current_team, partner);

                        comm_nbi_write(proc_id, current_team->reduce_flag, 
                                       &val, sizeof(val));
                    }
                }
            }

            for (j = 1; j <= MIN(num_bufs,log2_p-i+1); j++) {

                partner = 0;
                if (((me-1)%(2*step)) == 0) {
                    if ((me+step) <= p) {
                        partner = me+step;
                    }
                } else if (((me-1)%(2*step)) == step) {
                    if ((me-step) >= 1) {
                        partner = me-step;
                    }
                }

                step = step * 2;

                if (partner < 1 || partner > p) continue;

                k1 = (j-1)*(sz+1)+1;
                k2 = (j)*(sz+1);

                proc_id = get_proc_id(current_team, partner);

                if (me > partner) {

                    if (!base_buffer_alloc) {
                        /* wait for reduce-go to be set */
                        comm_poll_char_while_zero(&current_team->reduce_go[bufid]);
                        current_team->reduce_go[bufid] = 0;
                    }

                    if (i > 1) {
                        /* subsequent rounds, wait for notification signal
                         * from partner */
                        comm_poll_char_while_zero(current_team->reduce_flag);
                        *current_team->reduce_flag = 0;
                    }

                    comm_write_x( proc_id,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            &((char*)base_buffer)[0],
                            sz*elem_size );

                    comm_nbi_write( proc_id,
                            &((char*)work_buffers)[(k2-1)*elem_size],
                            &((char*)base_buffer)[sz*elem_size],
                            1 );

                } else {
                    /* poll on flag */
                    comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                    ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                    if (!base_buffer_alloc) partners[pindex++] = proc_id;

                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_reduce(*op, *elem_type, base_buffer,
                                   &((char*)work_buffers)[(k1-1)*elem_size],
                                   sz, *charlen);
                }
            }
        }
    } else {
        step = 1;
        for (i = 1; i <= log2_p; i += 1) {

            partner = 0;
            if (((me-1)%(2*step)) == 0) {
                if ((me+step) <= p) {
                    partner = me+step;
                }
            } else if (((me-1)%(2*step)) == step) {
                if ((me-step) >= 1) {
                    partner = me-step;
                }
            }

            step = step * 2;

            if (partner < 1 || partner > p) continue;

            int image_id = get_proc_id(current_team, partner) + 1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            k1 = 1;
            k2 = sz+1;
            proc_id = get_proc_id(current_team, partner);

            if (me > partner) {

                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );
            } else {

                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                if (!base_buffer_alloc) partners[pindex++] = proc_id;

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_reduce(*op, *elem_type, base_buffer,
                               &((char*)work_buffers)[(k1-1)*elem_size],
                               sz, *charlen);
            }
        }
    }

#endif

    /* TODO: can be a bit more clever here and avoid this extra copy, by
     * temporarily treating the result image as the "leader" this reduction.
     * */
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
        memset(base_buffer, 0, (sz+1)*elem_size);
        coarray_deallocate_(work_buffers, NULL);
    } else {
        memset(base_buffer, 0, (num_bufs+1)*(sz+1)*elem_size);
    }

#ifdef NEW_REDUCE
    if (!base_buffer_alloc) {
        /* notify partners that wrote to me for reduce that my collectives
         * buffers are free for next time */
        for (i = 0; i < pindex; i++) {
            coll_flags_t val = 1;
            comm_nbi_write(partners[i], &current_team->reduce_go[bufid],
                           &val, sizeof(val));
        }

        free(partners);
    }
#else

    free(pot_partners);
    free(partners);
#endif

    current_team->reduce_bufid = 1 - bufid;

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}


static void co_reduce_predef_to_image_2level__(void *source, int *result_image,
                               int *size, int *charlen,
                               caf_reduction_type_t *elem_type,
                               caf_reduction_op_t *op)
{
    int k;
    int p, pp, q, qq, r, me, partner, proc_id;
    int i, j, step, log2_q, log2_qq, val;
    int k1, k2;
    int num_bufs, nbufs1, nbufs2;
    size_t elem_size;
    int sz;
    void *base_buffer;
    void *work_buffers;
    int base_buffer_alloc = 0;
    int work_buffers_alloc = 0;
    int do_mpi;
    int ierr;
    void *collectives_buf;
    size_t collectives_bufsz;
    int bufid;
    int *partners;
    int pindex = 0;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    if (current_team != NULL) {
        bufid = current_team->reduce_bufid;
        collectives_buf = reduce_buffer[bufid];
        collectives_bufsz = reduce_bufsize;
    } else {
        Error("current_team is not set!");
    }

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

    /* find intranode index into intranode_set */
    int intranode_index = -1;
    {
        int i;
        for (i = 1; i <= intranode_count; i++) {
            if (current_team->intranode_set[i] == my_proc) {
                intranode_index = i;
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
    pp = intranode_count;

    /* find log2_qq, ceil( log2(pp) ) */
    qq = 1;
    log2_qq = 0;
    while ( (2*qq) <= pp) {
        qq = 2*qq;
        log2_qq = log2_qq + 1;
    }
    if (qq < pp)
        log2_qq = log2_qq + 1;

    /* find log2_q, ceil( log2(p) ) */
    q = 1;
    log2_q = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_q = log2_q + 1;
    }
    if (q < p)
        log2_q = log2_q + 1;

    elem_size = get_reduction_type_size(*elem_type, *charlen);

    k = (sz+1)*elem_size;
    if (collectives_bufsz < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        /* find largest number of work buffers that can be accomodated.
         *
         * Goal is to get log2_q + log2_qq buffers. If nb is the number of
         * buffers that can be allocated from the symmetric heap, it must be
         * at least 2 (otherwise we do not have space for both the base buffer
         * and one work buffer).
         */

        const int num_steps = log2_q + log2_qq;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((num_steps+1)*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb-1;

        if (collectives_max_workbufs >= 1) {
              num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q+log2_qq) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;

        /*
         * Now that we have the number of work buffers, we need to determine
         * how many of them are reserved for inter-node communication (nbufs1)
         * and how many for intra-node communication (nbufs2).
         * Since inter-node communication is more costly, we try to use as
         * many for this as we can. 
         *
         * */

        nbufs1 = 0;
        nbufs2 = 0;
        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }
    } else if (collectives_bufsz < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const int num_steps = log2_q + log2_qq;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(num_steps*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q+log2_qq) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = collectives_buf;

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;

        nbufs1 = 0;
        nbufs2 = 0;
        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }
    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buf;
        work_buffers = &((char*)collectives_buf)[(sz+1)*elem_size];
        num_bufs = MIN(((int)collectives_bufsz-k)/k, log2_q + log2_qq);
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        nbufs1 = 0;
        nbufs2 = 0;

        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }
    }

    if (!base_buffer_alloc)
        partners = malloc((log2_q+log2_qq)*sizeof(int));

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;

    /* Phase 1: Reduce to Leader */

    me = intranode_index;

    if (nbufs2 > 0) {
        step = 1;
        for (i = 1; i <= log2_qq; i += nbufs2) {
            /* send sync notifications prior to each round except the first
             * one */
            if (i > 1) {
                int s = step;
                for (j = 1; j <= MIN(nbufs2,log2_qq-i+1); j++) {
                    partner = 0;
                    if (((me-1)%(2*s)) == 0) {
                        if ((me+s) <= pp) {
                            partner = me+s;
                        }
                    } else if (((me-1)%(2*s)) == s) {
                        if ((me-s) >= 1) {
                            partner = me-s;
                        }
                    }

                    s = s * 2;
                    if (partner < 1 || partner > pp) continue;

                    if (me < partner) {
                        /* notify partner I'm ready to receive data */
                        coll_flags_t *flag;

                        proc_id = current_team->intranode_set[partner];

                        flag = comm_get_sharedptr(current_team->reduce_flag,
                                                  proc_id);

                        *flag = 1;
                    }
                }
            }

            for (j = 1; j <= MIN(nbufs2,log2_qq-i+1); j++) {

                partner = 0;
                if (((me-1)%(2*step)) == 0) {
                    if ((me+step) <= pp) {
                        partner = me+step;
                    }
                } else if (((me-1)%(2*step)) == step) {
                    if ((me-step) >= 1) {
                        partner = me-step;
                    }
                }

                step = step * 2;

                if (partner < 1 || partner > pp) continue;

                k1 = (nbufs1+j-1)*(sz+1)+1;
                k2 = (nbufs1+j)*(sz+1);

                proc_id = current_team->intranode_set[partner];

                if (me > partner) {

                    if (!base_buffer_alloc) {
                        /* wait for reduce-go to be set */
                        comm_poll_char_while_zero(&current_team->reduce_go[bufid]);
                        current_team->reduce_go[bufid] = 0;
                    }

                    if (i > 1) {
                        /* subsequent rounds, wait for notification signal
                         * from partner */
                        comm_poll_char_while_zero(current_team->reduce_flag);
                        *current_team->reduce_flag = 0;
                    }

                    comm_write_x( proc_id,
                                  &((char*)work_buffers)[(k1-1)*elem_size],
                                  &((char*)base_buffer)[0],
                                  sz*elem_size );

                    comm_nbi_write( proc_id,
                                    &((char*)work_buffers)[(k2-1)*elem_size],
                                    &((char*)base_buffer)[sz*elem_size],
                                    1 );
                } else {
                    /* poll on flag */
                    comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                    ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                    if (!base_buffer_alloc) partners[pindex++] = proc_id;

                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_reduce(*op, *elem_type, base_buffer,
                                   &((char*)work_buffers)[(k1-1)*elem_size],
                                   sz, *charlen);
                }
            }
        }
    } else {
        step = 1;
        for (i = 1; i <= log2_qq; i += 1) {

            partner = 0;
            if (((me-1)%(2*step)) == 0) {
                if ((me+step) <= pp) {
                    partner = me+step;
                }
            } else if (((me-1)%(2*step)) == step) {
                if ((me-step) >= 1) {
                    partner = me-step;
                }
            }

            step = step * 2;

            if (partner < 1 || partner > pp) continue;

            int image_id = current_team->intranode_set[partner] + 1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);


            k1 = (nbufs1)*(sz+1)+1;
            k2 = (nbufs1+1)*(sz+1);

            proc_id = current_team->intranode_set[partner];

            if (me > partner) {
                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );
            } else {

                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                if (!base_buffer_alloc) partners[pindex++] = proc_id;

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_reduce(*op, *elem_type, base_buffer,
                               &((char*)work_buffers)[(k1-1)*elem_size],
                               sz, *charlen);
            }
        }
    }


    /* Phase 2: Leaders reduce to the first leader */

    if (is_leader) {
        me = leader_index+1;

        if (nbufs1 > 0) {
            step = 1;
            for (i = 1; i <= log2_q; i += nbufs1) {
                /* send sync notifications prior to each round except the first
                 * one */
                if (i > 1) {
                    int s = step;
                    for (j = 1; j <= MIN(nbufs1,log2_q-i+1); j++) {
                        partner = 0;
                        if (((me-1)%(2*s)) == 0) {
                            if ((me+s) <= p) {
                                partner = me+s;
                            }
                        } else if (((me-1)%(2*s)) == s) {
                            if ((me-s) >= 1) {
                                partner = me-s;
                            }
                        }

                        s = s * 2;
                        if (partner < 1 || partner > p) continue;

                        if (me < partner) {
                            /* notify partner I'm ready to receive data */
                            coll_flags_t val = 1;

                            proc_id = leader_set[partner-1];

                            comm_nbi_write(proc_id, current_team->reduce_flag, 
                                           &val, sizeof(val));
                        }
                    }
                }

                for (j = 1; j <= MIN(nbufs1,log2_q-i+1); j++) {

                    partner = 0;
                    if (((me-1)%(2*step)) == 0) {
                        if ((me+step) <= p) {
                            partner = me+step;
                        }
                    } else if (((me-1)%(2*step)) == step) {
                        if ((me-step) >= 1) {
                            partner = me-step;
                        }
                    }

                    step = step * 2;

                    if (partner < 1 || partner > p) continue;

                    k1 = (j-1)*(sz+1)+1;
                    k2 = (j)*(sz+1);

                    proc_id = leader_set[partner-1];

                    if (me > partner) {

                        if (!base_buffer_alloc) {
                            /* wait for reduce-go to be set */
                            comm_poll_char_while_zero(&current_team->reduce_go[bufid]);
                            current_team->reduce_go[bufid] = 0;
                        }

                        if (i > 1) {
                            /* subsequent rounds, wait for notification signal
                             * from partner */
                            comm_poll_char_while_zero(current_team->reduce_flag);
                            *current_team->reduce_flag = 0;
                        }

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

                        ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                        if (!base_buffer_alloc) partners[pindex++] = proc_id;

                        /* reduce:
                         *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                         */

                        perform_reduce(*op, *elem_type, base_buffer,
                                       &((char*)work_buffers)[(k1-1)*elem_size],
                                       sz, *charlen);
                    }
                }
            }
        } else {
            step = 1;
            for (i = 1; i <= log2_q; i += 1) {

                partner = 0;
                if (((me-1)%(2*step)) == 0) {
                    if ((me+step) <= p) {
                        partner = me+step;
                    }
                } else if (((me-1)%(2*step)) == step) {
                    if ((me-step) >= 1) {
                        partner = me-step;
                    }
                }

                step = step * 2;

                if (partner < 1 || partner > p) continue;

                int image_id = leader_set[partner-1] + 1;
                comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

                k1 = 1;
                k2 = sz+1;
                proc_id = leader_set[partner-1];

                if (me > partner) {

                    comm_write_x( proc_id,
                                  &((char*)work_buffers)[(k1-1)*elem_size],
                                  &((char*)base_buffer)[0],
                                  sz*elem_size );

                    comm_nbi_write( proc_id,
                                    &((char*)work_buffers)[(k2-1)*elem_size],
                                    &((char*)base_buffer)[sz*elem_size],
                                    1 );
                } else {

                    /* poll on flag */
                    comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                    ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                    if (!base_buffer_alloc) partners[pindex++] = proc_id;

                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_reduce(*op, *elem_type, base_buffer,
                                   &((char*)work_buffers)[(k1-1)*elem_size],
                                   sz, *charlen);
                }
            }
        }

    }

    /* TODO: can be a bit more clever here and avoid this extra copy, by
     * temporarily treating the result image as the leader of its
     * node for this reduction. */
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

        comm_write_x( proc_id,
                &((char*)base_buffer)[0],
                &((char*)base_buffer)[0],
                sz*elem_size );

        comm_nbi_write( proc_id,
                &((char*)base_buffer)[sz*elem_size],
                &((char*)base_buffer)[sz*elem_size],
                1 );
    }

    if (is_leader) {
      free(local_images);
    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else if (work_buffers_alloc) {
        memset(base_buffer, 0, (sz+1)*elem_size);
        coarray_deallocate_(work_buffers, NULL);
    } else {
        memset(base_buffer, 0, (num_bufs+1)*(sz+1)*elem_size);
    }

    if (!base_buffer_alloc) {
        /* notify partners that wrote to me for reduce that my collectives
         * buffers are free for next time */
        for (i = 0; i < pindex; i++) {
            coll_flags_t val = 1;
            comm_nbi_write(partners[i], &current_team->reduce_go[bufid],
                           &val, sizeof(val));
        }
        free(partners);
    }

    current_team->reduce_bufid = 1 - bufid;

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}


void co_reduce_predef_to_all__( void *source, int *size, int *charlen,
                               caf_reduction_type_t *elem_type,
                               caf_reduction_op_t *op, INTEGER4 *stat,
                               char *errmsg, int errmsg_len)
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
    void *collectives_buf;
    size_t collectives_bufsz;
    int bufid;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    sz = *size;

#ifdef MPI_AVAIL
    if (!mpi_collectives_available && enable_collectives_mpi) {
        /* check if MPI was initialized */
        if (MPI_Initialized &&
            MPI_Initialized(&mpi_collectives_available) != MPI_SUCCESS) {
            Error("MPI_Initialized check failed");
        }
    }

    if (mpi_collectives_available && enable_collectives_mpi &&
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

    if ((enable_collectives_2level || enable_reduction_2level) &&
        current_team) {
        co_reduce_predef_to_all_2level__(source, size, charlen, elem_type, op);
        PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
        return;

        /* does not reach */
    }

    if (current_team != NULL) {
      bufid = current_team->allreduce_bufid;
      collectives_buf = allreduce_buffer[bufid];
      collectives_bufsz = allreduce_bufsize;
    } else {
      Error("current_team is not set!");
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

    if (collectives_bufsz < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        const int num_steps = log2_q;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((num_steps+1)*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb-1;

        if (collectives_max_workbufs >= 1) {
              num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;

    } else if (collectives_bufsz < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const int num_steps = log2_q;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(num_steps*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = collectives_buf;

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;

    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buf;
        work_buffers = &((char*)collectives_buf)[(sz+1)*elem_size];
        num_bufs = MIN(((int)collectives_bufsz-k)/k, log2_q);
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


        if (0 /*enable_collectives_use_canary*/) {

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

#ifndef NEW_ALLREDUCE

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

                if (0 /*enable_collectives_use_canary*/) {

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

#else

    /* first q processes do recursive doubling algorithm */
    if (me <= q && num_bufs > 1) {
        step = 1;
        for (i = 1; i <= log2_q; i += num_bufs) {

            /* send sync notifications prior to each round except the first
             * one */
            if (i > 1) {
                int s = step;
                for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {
                    coll_flags_t val = 1;
                    if (((me-1)%(2*s)) < s) {
                        proc_id = get_proc_id(current_team, me+s);
                    } else {
                        proc_id = get_proc_id(current_team, me-s);
                    }

                    comm_nbi_write(proc_id,
                        &current_team->allreduce_sync[2*(i+j-2)+bufid],
                        &val, sizeof(val));

                    s = s * 2;
                }
            }

            for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {
                int partner;
                k1 = (j-1)*(sz+1)+1;
                k2 = j*(sz+1);

                if (((me-1)%(2*step)) < step) {
                    partner = me+step;
                } else {
                    partner = me-step;
                }

                proc_id = get_proc_id(current_team, partner);

                if (i > 1) {
                    /* subsequent rounds, wait for notification signal
                     * from partner */

                    comm_poll_char_while_zero(
                        &current_team->allreduce_sync[2*(i+j-2)+bufid]);
                    current_team->allreduce_sync[2*(i+j-2)+bufid] = 0;
                }


                comm_write_x( proc_id,
                        &((char*)work_buffers)[(k1-1)*elem_size],
                        &((char*)base_buffer)[0],
                        sz*elem_size );

                comm_nbi_write( proc_id,
                        &((char*)work_buffers)[(k2-1)*elem_size],
                        &((char*)base_buffer)[sz*elem_size],
                        1 );


                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_reduce(*op, *elem_type, base_buffer,
                               &((char*)work_buffers)[(k1-1)*elem_size],
                               sz, *charlen);

                step = step * 2;
            }

        }
    } else if (me <= q && num_bufs == 1) {
        step = 1;
        for (i = 1; i <= log2_q; i += 1) {
            k1 = 1;
            k2 = sz+1;

            if (((me-1)%(2*step)) < step) {
                proc_id = get_proc_id(current_team, me+step);
            } else {
                proc_id = get_proc_id(current_team, me-step);
            }

            int image_id = proc_id + 1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);


            comm_write_x( proc_id,
                    &((char*)work_buffers)[(k1-1)*elem_size],
                    &((char*)base_buffer)[0],
                    sz*elem_size );

            comm_nbi_write( proc_id,
                    &((char*)work_buffers)[(k2-1)*elem_size],
                    &((char*)base_buffer)[sz*elem_size],
                    1 );


            /* poll on flag */
            comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

            ((char*)work_buffers)[(k2-1)*elem_size] = 0;

            /* reduce:
             *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
             */

            perform_reduce(*op, *elem_type, base_buffer,
                           &((char*)work_buffers)[(k1-1)*elem_size],
                           sz, *charlen);

            step = step * 2;
        }
    }

#endif


    /* first r processes put values to last r processes */
    if (me <= r) {
        partner = me + q;
        proc_id = get_proc_id(current_team, partner);

        _SYNC_IMAGES (&partner, 1, NULL, 0, NULL, 0);

        if (0 /*enable_collectives_use_canary*/) {

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
        memset(base_buffer, 0, (sz+1)*elem_size);
        coarray_deallocate_(work_buffers, NULL);
    } else {
        memset(base_buffer, 0, (num_bufs+1)*(sz+1)*elem_size);
    }

    current_team->allreduce_bufid = 1 - bufid;


    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

static void co_reduce_predef_to_all_2level__(void *source, int *size, int *charlen,
                               caf_reduction_type_t *elem_type,
                               caf_reduction_op_t *op)
{
    int k;
    int p, pp, q, qq, r, me, partner, proc_id;
    int i, j, step, log2_q, log2_qq, val;
    int k1, k2;
    int num_bufs, nbufs1, nbufs2;
    size_t elem_size;
    int sz;
    void *base_buffer;
    void *work_buffers;
    int base_buffer_alloc = 0;
    int work_buffers_alloc = 0;
    int do_mpi;
    int ierr;
    void *collectives_buf;
    size_t collectives_bufsz;
    int bufid;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    if (current_team != NULL) {
      bufid = current_team->allreduce_bufid;
      collectives_buf = allreduce_buffer[bufid];
      collectives_bufsz = allreduce_bufsize;
    } else {
      Error("current_team is not set!");
    }

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

    /* find intranode index into intranode_set */
    int intranode_index = -1;
    {
        int i;
        for (i = 1; i <= intranode_count; i++) {
            if (current_team->intranode_set[i] == my_proc) {
                intranode_index = i;
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
    pp = intranode_count;

    /* find log2_qq, ceil( log2(pp) ) */
    qq = 1;
    log2_qq = 0;
    while ( (2*qq) <= pp) {
        qq = 2*qq;
        log2_qq = log2_qq + 1;
    }
    if (qq < pp) {
        log2_qq = log2_qq + 1;
    }

    /* find greatest power of 2, q, less than p */
    q = 1;
    log2_q = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_q = log2_q + 1;
    }

    /* r is the number of remaining processes, after q */
    r = p - q;

    elem_size = get_reduction_type_size(*elem_type, *charlen);

    k = (sz+1)*elem_size;
    if (collectives_bufsz < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        /* find largest number of work buffers that can be accomodated.
         *
         * Goal is to get log2_q + log2_qq buffers. If nb is the number of
         * buffers that can be allocated from the symmetric heap, it must be
         * at least 2 (otherwise we do not have space for both the base buffer
         * and one work buffer).
         */

        const int num_steps = log2_q + log2_qq;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((num_steps+1)*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb - 1;

        if (collectives_max_workbufs >= 1) {
              num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q+log2_qq) != 0)) {
            Error("1. not enough buffer space for collectives!");
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;

        /*
         * Now that we have the number of work buffers, we need to determine
         * how many of them are reserved for inter-node communication (nbufs1)
         * and how many for intra-node communication (nbufs2).
         * Since inter-node communication is more costly, we try to use as
         * many for this as we can. 
         *
         * */

        nbufs1 = 0;
        nbufs2 = 0;
        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }
    } else if (collectives_bufsz < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const int num_steps = log2_q + log2_qq;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(num_steps*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q+log2_qq) != 0)) {
            Error("2. not enough buffer space for collectives!");
        }

        base_buffer = collectives_buf;

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;

        nbufs1 = 0;
        nbufs2 = 0;
        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }

    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buf;
        work_buffers = &((char*)collectives_buf)[(sz+1)*elem_size];
        num_bufs = MIN(((int)collectives_bufsz-k)/k, log2_q + log2_qq);
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        nbufs1 = 0;
        nbufs2 = 0;

        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }

    }

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;

    /* Phase 1: Reduce to Leader */

    me = intranode_index;

    if (nbufs2 > 0) {
        step = 1;
        for (i = 1; i <= log2_qq; i += nbufs2) {
            /* send sync notifications prior to each round except the first
             * one */
            if (i > 1) {
                int s = step;
                for (j = 1; j <= MIN(nbufs2,log2_qq-i+1); j++) {
                    partner = 0;
                    if (((me-1)%(2*s)) == 0) {
                        if ((me+s) <= pp) {
                            partner = me+s;
                        }
                    } else if (((me-1)%(2*s)) == s) {
                        if ((me-s) >= 1) {
                            partner = me-s;
                        }
                    }

                    s = s * 2;
                    if (partner < 1 || partner > pp) continue;

                    if (me < partner) {
                        /* notify partner I'm ready to receive data */
                        coll_flags_t *flag;

                        proc_id = current_team->intranode_set[partner];

                        flag = comm_get_sharedptr(current_team->reduce_flag,
                                                  proc_id);

                        *flag = 1;
                    }
                }
            }

            for (j = 1; j <= MIN(nbufs2,log2_qq-i+1); j++) {

                partner = 0;
                if (((me-1)%(2*step)) == 0) {
                    if ((me+step) <= pp) {
                        partner = me+step;
                    }
                } else if (((me-1)%(2*step)) == step) {
                    if ((me-step) >= 1) {
                        partner = me-step;
                    }
                }

                step = step * 2;

                if (partner < 1 || partner > pp) continue;

                k1 = (nbufs1+j-1)*(sz+1)+1;
                k2 = (nbufs1+j)*(sz+1);

                if (me > partner) {

#if 0
                    if (!base_buffer_alloc) {
                        /* wait for reduce-go to be set */
                        comm_poll_char_while_zero(&current_team->reduce_go[bufid]);

                        /* no need for reset since we'll be doing a broadcast
                         * in the last phase of the all-reduce */
                    }
#endif

                    if (i > 1) {
                        /* subsequent rounds, wait for notification signal
                         * from partner */
                        comm_poll_char_while_zero(current_team->reduce_flag);
                        *current_team->reduce_flag = 0;
                    }

                    proc_id = current_team->intranode_set[partner];

                    comm_write_x( proc_id,
                                  &((char*)work_buffers)[(k1-1)*elem_size],
                                  &((char*)base_buffer)[0],
                                  sz*elem_size );

                    comm_nbi_write( proc_id,
                                    &((char*)work_buffers)[(k2-1)*elem_size],
                                    &((char*)base_buffer)[sz*elem_size],
                                    1 );
                } else {
                    /* poll on flag */
                    comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                    ((char*)work_buffers)[(k2-1)*elem_size] = 0;


                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_reduce(*op, *elem_type, base_buffer,
                                   &((char*)work_buffers)[(k1-1)*elem_size],
                                   sz, *charlen);
                }
            }
        }
    } else {
        step = 1;
        for (i = 1; i <= log2_qq; i += 1) {

            partner = 0;
            if (((me-1)%(2*step)) == 0) {
                if ((me+step) <= pp) {
                    partner = me+step;
                }
            } else if (((me-1)%(2*step)) == step) {
                if ((me-step) >= 1) {
                    partner = me-step;
                }
            }

            step = step * 2;

            if (partner < 1 || partner > pp) continue;

            int image_id = current_team->intranode_set[partner] + 1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);


            k1 = (nbufs1)*(sz+1)+1;
            k2 = (nbufs1+1)*(sz+1);

            if (me > partner) {
                proc_id = current_team->intranode_set[partner];

                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );
            } else {

                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_reduce(*op, *elem_type, base_buffer,
                               &((char*)work_buffers)[(k1-1)*elem_size],
                               sz, *charlen);
            }
        }
    }


    /* Phase 2: Leaders perform reduction using recursive-doubling */

    if (is_leader) {
        me = leader_index+1;

        /* last r processes put values to first r processes */
        if (me > q) {
            partner = me - q;
            proc_id = leader_set[partner-1];

            comm_poll_char_while_zero(current_team->reduce_flag);
            *(current_team->reduce_flag) = 0;

            if (enable_collectives_use_canary) {
                int k1 = nbufs1*(sz+1)+1;

                comm_nbi_write( proc_id,
                        &((char*)work_buffers)[(k1-1)*elem_size],
                        &((char*)base_buffer)[0],
                        sz*elem_size+1 );

            } else {
                int k1 = nbufs1*(sz+1)+1;
                int k2 = (nbufs1+1)*(sz+1);

                comm_write_x( proc_id,
                        &((char*)work_buffers)[(k1-1)*elem_size],
                        &((char*)base_buffer)[0],
                        sz*elem_size );

                comm_nbi_write( proc_id,
                        &((char*)work_buffers)[(k2-1)*elem_size],
                        &((char*)base_buffer)[sz*elem_size],
                        1 );

            }

        } else if (me <= r) {
            int val = 1;
            partner = me + q;
            proc_id = leader_set[partner-1];
            int k1 = nbufs1*(sz+1)+1;
            int k2 = (nbufs1+1)*(sz+1);

            /* notify sender that I'm ready */
            comm_nbi_write(proc_id, current_team->reduce_flag, &val,
                           sizeof(val));

            /* poll on flag */
            comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);
            ((char *)work_buffers)[(k2-1)*elem_size] = 0;

            /* reduce:
             *   work_buf(1:sz) = work_buf(1:sz) + work_buf(sz+2:2*(sz+1)-1)
             */

            perform_reduce(*op, *elem_type, base_buffer,
                    &((char*)work_buffers)[(k1-1)*elem_size],
                    sz, *charlen);
        }

        /* first q processes do recursive doubling algorithm */
        if (me <= q && nbufs1 > 0) {
            step = 1;
            for (i = 1; i <= log2_q; i += nbufs1) {

                /* send sync notifications prior to each round except the first
                 * one */
                if (i > 1) {
                    int s = step;
                    for (j = 1; j <= MIN(nbufs1, log2_q-i+1); j++) {
                        coll_flags_t val = 1;
                        if (((me-1)%(2*s)) < s) {
                            proc_id = leader_set[me+s-1];
                        } else {
                            proc_id = leader_set[me-s-1];
                        }

                        comm_nbi_write(proc_id,
                            &current_team->allreduce_sync[2*(i+j-2)+bufid],
                            &val, sizeof(val));

                        s = s * 2;
                    }
                }

                for (j = 1; j <= MIN(nbufs1, log2_q-i+1); j++) {
                    int partner;
                    k1 = (j-1)*(sz+1)+1;
                    k2 = j*(sz+1);

                    if (((me-1)%(2*step)) < step) {
                        partner = me+step;
                    } else {
                        partner = me-step;
                    }

                    proc_id = leader_set[partner-1];

                    if (i > 1) {
                        /* subsequent rounds, wait for notification signal
                         * from partner */

                        comm_poll_char_while_zero(
                            &current_team->allreduce_sync[2*(i+j-2)+bufid]);
                        current_team->allreduce_sync[2*(i+j-2)+bufid] = 0;
                    }

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

                    ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_reduce(*op, *elem_type, base_buffer,
                                   &((char*)work_buffers)[(k1-1)*elem_size],
                                   sz, *charlen);

                    step = step * 2;
                }

            }
        } else if (me <= q && nbufs1 == 0) {
            step = 1;
            for (i = 1; i <= log2_q; i += 1) {
                k1 = 1;
                k2 = sz+1;

                if (((me-1)%(2*step)) < step) {
                    proc_id = leader_set[me+step-1];
                } else {
                    proc_id = leader_set[me-step-1];
                }

                int image_id = proc_id + 1;
                comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

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

                ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_reduce(*op, *elem_type, base_buffer,
                               &((char*)work_buffers)[(k1-1)*elem_size],
                               sz, *charlen);

                step = step * 2;
            }
        }

        /* first r processes put values to last r processes */
        if (me <= r) {
            partner = me + q;
            proc_id = leader_set[partner-1];

            comm_poll_char_while_zero(current_team->reduce_flag);
            *(current_team->reduce_flag) = 0;

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
            int val = 1;
            partner = me - q;
            proc_id = leader_set[partner-1];
            ((char*)base_buffer)[sz*elem_size] = 0;

            /* notify sender that I'm ready to receive in base buffer */
            comm_nbi_write(proc_id, current_team->reduce_flag, &val,
                           sizeof(val));

            /* poll on flag */
            comm_poll_char_while_zero(&((char*)base_buffer)[sz*elem_size]);
        }

    }


    /* Phase 3: Leader writes values back to its non-leaders */

#if defined(GASNET)
    if (is_leader) {
        int i;
        /* NOTE: assuming that intranode_count doesn't exceed 256! */
        *(current_team->bcast_flag) = intranode_count-1;
        for (i = 1; i < intranode_count; i++) {
            int proc_id = current_team->intranode_set[i+1];
            coll_flags_t *flag = comm_get_sharedptr(current_team->bcast_flag,
                                                    proc_id);
            *flag = 1;
        }

        /* wait for non-leaders to finish reading buffer */
        comm_poll_char_while_nonzero(current_team->bcast_flag);
    } else {
        int leader_proc_id = current_team->intranode_set[1];
        coll_flags_t *leader_flag = comm_get_sharedptr(current_team->bcast_flag,
                                                       leader_proc_id);

        /* poll on flag */
        comm_poll_char_while_zero(current_team->bcast_flag);

        comm_read(leader_proc_id, &((char*)base_buffer)[0],
                  &((char*)base_buffer)[0], sz*elem_size);

        *(current_team->bcast_flag) = 0;

        SYNC_FETCH_AND_ADD((coll_flags_t *)leader_flag, -1);
    }
#else
    if (is_leader) {
        int i;
        for (i = 1; i < intranode_count; i++) {
            comm_write_x(current_team->intranode_set[i+1], &((char*)base_buffer)[0],
                    &((char*)base_buffer)[0], sz*elem_size);
        }
        /* NOTE: assumes that SYNC_IMAGES_HASH is NOT defined  */
        comm_sync_images(local_images, intranode_count-1, NULL, 0, NULL, 0);
    } else {
        int my_leader_image = current_team->intranode_set[1] + 1;
        comm_sync_images(&my_leader_image, 1, NULL, 0, NULL, 0);
    }
#endif

    memcpy(source, base_buffer, sz*elem_size);


    if (is_leader) {
      free(local_images);
    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else if (work_buffers_alloc) {
        memset(base_buffer, 0, (sz+1)*elem_size);
        coarray_deallocate_(work_buffers, NULL);
    } else {
        memset(base_buffer, 0, (num_bufs+1)*(sz+1)*elem_size);
    }

    /* toggle allreduce_bufid for current team */
    if (current_team != NULL) {
      current_team->allreduce_bufid = 1 - bufid;
    } else {
      Error("current_team is not set!");
    }

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

void co_broadcast_from_root(void *source, size_t sz, int source_image)
{
    int k;
    int i,j, step, val, log2_p;
    int p, q, r, me, partner, proc_id;
    int k1, k2;
    int num_bufs;
    int *partners;
    void *base_buffer;
    int base_buffer_alloc = 0;
    void *collectives_buf;
    size_t collectives_bufsz;
    int bufid, pindex = 0;

    if ((enable_collectives_2level || enable_broadcast_2level) &&
         current_team) {
        co_broadcast_from_root_2level(source, sz, source_image);
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
        return;

        /* does not reach */
    }

    if (current_team != NULL) {
        bufid = current_team->bcast_bufid;
        collectives_buf = broadcast_buffer[bufid];
        collectives_bufsz = broadcast_bufsize;
    } else {
      Error("current_team is not set!");
    }

    me = _this_image;
    p = _num_images;

    k = sz;
    if (collectives_bufsz < k) {
        base_buffer = coarray_allocatable_allocate_(sz+sizeof(int), NULL, NULL);
        base_buffer_alloc = 1;
    } else {
        base_buffer = collectives_buf;
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
        } else {
            ((char*)base_buffer)[sz] = 1;
        }

    } else {
        if (me == 1) {
            _SYNC_IMAGES(&source_image, 1, NULL, 0, NULL, 0);

            /* received from source image */

            ((char*)base_buffer)[sz] = 1;
        }
    }

#ifndef NEW_BCAST
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
#else

        int pi = log2_p;
        step = q/2;
        if (!base_buffer_alloc)
            partners = malloc(sizeof(*partners) * log2_p);
        while (step > 0) {
            partner = 0;
            if ((me-1)%step == 0) {
                if ((me-1)%(2*step) == 0) {
                    partner = me+step;
                } else {
                    partner = me-step;
                }
            }

            step = step / 2;
            pi = pi - 1;

            if (partner < 1 || partner > p) continue;

            int proc_id = get_proc_id(current_team, partner);

            if (me < partner) {
                /* writing to partner */

                if (!base_buffer_alloc) {
                    /* wait for broadcast_go to be set */
                    if (!current_team->bcast_go[bufid*log2_p+pi]) {
                        comm_poll_char_while_zero(
                                &current_team->bcast_go[bufid*log2_p+pi]);
                    }

                    current_team->bcast_go[bufid*log2_p+pi] = 0;
                }

                comm_write_x( proc_id,
                        &((char*)base_buffer)[0],
                        &((char*)base_buffer)[0],
                        sz );

                comm_nbi_write( proc_id,
                        &((char*)base_buffer)[sz],
                        &((char*)base_buffer)[sz],
                        1 );

            } else {
                /* wait to receive in base buffer */
                comm_poll_char_while_zero(&((char*)base_buffer)[sz]);

                if (!base_buffer_alloc) {
                    partners[pindex++] = partner;
                }
            }
        }
#endif

    /* all images except for source image needs to copy from base_buffer to
     * source */
    if (me != source_image) {
        memcpy(source, base_buffer, sz);
    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else {
        memset(base_buffer, 0, sz+sizeof(int));
    }

#ifdef NEW_BCAST
    if (!base_buffer_alloc) {
        /* notify partner that wrote to me for broadcast that my collectives
         * buffer is free for next time */
        for (i = 0; i < pindex; i++) {
            coll_flags_t val = 1;
            int proc_id = get_proc_id(current_team, partners[i]);
            int x,y;

            /* find my bcast_go flag on partner */
            y = me - partners[i];
            x = 0;
            while (y/2 != 0) {
                y = y / 2;
                x += 1;
            }

            comm_nbi_write(proc_id, &current_team->bcast_go[bufid*log2_p+x],
                    &val, sizeof(val));
        }
        free(partners);
    }
#endif

    current_team->bcast_bufid = 1 - bufid;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

static void co_broadcast_from_root_2level(void *source, size_t sz, int source_image)
{
    int k;
    int i,j, step, val, log2_p, log2_qq, qq;
    int p, q, r, me, partner, proc_id;
    int k1, k2;
    int num_bufs;
    int *internode_partners, *intranode_partners;
    void *base_buffer;
    int base_buffer_alloc = 0;
    int internode_pindex = 0, intranode_pindex = 0;

    const int my_proc = current_team->codimension_mapping[_this_image - 1];
    const int is_leader = (current_team->intranode_set[1] == my_proc);
    const long int *leader_set = current_team->leader_set;
    const int leaders_count = current_team->leaders_count;
    const int intranode_count = current_team->intranode_set[0];

    void *collectives_buf;
    size_t collectives_bufsz;
    int bufid;

    if (current_team != NULL) {
        bufid = current_team->bcast_bufid;
        collectives_buf = broadcast_buffer[bufid];
        collectives_bufsz = broadcast_bufsize;
    } else {
        Error("current_team is not set!");
    }

    /* find leader index into leader_set */
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

    /* find intranode index into intranode_set */
    int intranode_index = -1;
    {
        int i;
        for (i = 1; i <= intranode_count; i++) {
            if (current_team->intranode_set[i] == my_proc) {
                intranode_index = i;
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

    k = sz + sizeof(int);
    if (collectives_bufsz < k) {
        base_buffer = coarray_allocatable_allocate_(sz+sizeof(int), NULL, NULL);
        base_buffer_alloc = 1;
    } else {
        base_buffer = collectives_buf;
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

    /* find greatest power of 2 */
    qq = 1;
    log2_qq = 0;
    while ( (2*qq) <= intranode_count) {
        qq = 2*qq;
        log2_qq = log2_qq + 1;
    }
    if (qq < intranode_count) {
        log2_qq = log2_qq + 1;
    }

    if (_this_image == source_image) {
        memcpy(base_buffer, source, sz);

        if (_this_image != 1) {
            /* copy from source image to image 1 */
            int root = 1;
            proc_id = get_proc_id(current_team, root);

            comm_nbi_write( proc_id,
                    &((char*)base_buffer)[0],
                    &((char*)base_buffer)[0],
                    sz );

            _SYNC_IMAGES(&root, 1, NULL, 0, NULL, 0);
        } else {
            ((char*)base_buffer)[sz] = 1;
        }
    } else {
        if (_this_image == 1) {
            _SYNC_IMAGES(&source_image, 1, NULL, 0, NULL, 0);

            /* received from source image */

            ((char*)base_buffer)[sz] = 1;
        }
    }

    /* Phase 1: Broadcast to other leaders */

    if (is_leader) {
        if (!base_buffer_alloc)
            internode_partners = malloc(sizeof(*internode_partners) * log2_p);
        int pi = log2_p;
        step = q/2;
        while (step > 0) {
            partner = 0;
            if ((me-1)%step == 0) {
                if ((me-1)%(2*step) == 0) {
                    partner = me+step;
                } else {
                    partner = me-step;
                }
            }

            step = step / 2;
            pi = pi - 1;

            if (partner < 1 || partner > p) continue;

            int proc_id = leader_set[partner-1];

            if (me < partner) {
                /* writing to partner */

                if (!base_buffer_alloc) {
                    /* wait for broadcast_go to be set */
                    if (!current_team->bcast_go[2*pi+bufid]) {
                        comm_poll_char_while_zero(
                                &current_team->bcast_go[2*pi+bufid]);
                    }

                    current_team->bcast_go[2*pi+bufid] = 0;
                }


                if (enable_collectives_use_canary) {

                    comm_nbi_write( proc_id,
                            &((char*)base_buffer)[0],
                            &((char*)base_buffer)[0],
                            sz + sizeof(int) );

                } else {

                    comm_write_x( proc_id,
                            &((char*)base_buffer)[0],
                            &((char*)base_buffer)[0],
                            sz );

                    comm_nbi_write( proc_id,
                            &((char*)base_buffer)[sz],
                            &((char*)base_buffer)[sz],
                            1 );

                }
            } else {

                /* wait to receive in base buffer */
                comm_poll_char_while_zero(&((char*)base_buffer)[sz]);

                if (!base_buffer_alloc) {
                    internode_partners[internode_pindex++] = partner;
                }
            }
        }
    }


    /* Phase 2: Leaders broadcast to their non-leaders */

#ifndef BINOMIAL_INTRANODE_BCAST

#if defined(GASNET)
    if (is_leader) {
        int i;
        /* NOTE: assuming that intranode_count doesn't exceed 256! */
        *(current_team->bcast_flag) = intranode_count-1;
        for (i = 1; i < intranode_count; i++) {
            int proc_id = current_team->intranode_set[i+1];
            coll_flags_t *flag = comm_get_sharedptr(current_team->bcast_flag,
                                                    proc_id);
            *flag = 1;
        }

        /* wait for non-leaders to finish reading buffer */
        comm_poll_char_while_nonzero(current_team->bcast_flag);
    } else {
        int leader_proc_id = current_team->intranode_set[1];
        coll_flags_t *leader_flag = comm_get_sharedptr(current_team->bcast_flag,
                                                       leader_proc_id);

        /* poll on flag */
        comm_poll_char_while_zero(current_team->bcast_flag);

        comm_read(leader_proc_id, &((char*)base_buffer)[0],
                  &((char*)base_buffer)[0], sz);

        *(current_team->bcast_flag) = 0;

        SYNC_FETCH_AND_ADD((coll_flags_t *)leader_flag, -1);
    }
#else
    if (is_leader) {
        int i;
        for (i = 1; i < intranode_count; i++) {
            comm_write_x(current_team->intranode_set[i+1], &((char*)base_buffer)[0],
                    &((char*)base_buffer)[0], sz);
        }
        /* NOTE: assumes that SYNC_IMAGES_HASH is NOT defined  */
        comm_sync_images(local_images, intranode_count-1, NULL, 0, NULL, 0);
    } else {
        int my_leader_image = current_team->intranode_set[1] + 1;
        comm_sync_images(&my_leader_image, 1, NULL, 0, NULL, 0);
    }
#endif

#else

        if (!base_buffer_alloc) {
            intranode_partners = malloc(sizeof(*intranode_partners) * log2_qq);
        }

        int pi = log2_qq;
        step = qq/2;
        me = intranode_index;
        while (step > 0) {
            partner = 0;
            if ((me-1)%step == 0) {
                if ((me-1)%(2*step) == 0) {
                    partner = me+step;
                } else {
                    partner = me-step;
                }
            }

            step = step / 2;
            pi = pi - 1;

            if (partner < 1 || partner > intranode_count) continue;

            int proc_id = current_team->intranode_set[partner];

            if (me < partner) {
                /* writing to partner */

                if (!base_buffer_alloc) {
                    /* wait for broadcast_go to be set */
                    if (!current_team->bcast_go[2*(log2_p+pi)+bufid]) {
                        comm_poll_char_while_zero(
                                &current_team->bcast_go[2*(log2_p+pi)+bufid]);
                    }

                    current_team->bcast_go[2*(log2_p+pi)+bufid] = 0;
                }

                if (enable_collectives_use_canary) {

                    comm_nbi_write( proc_id,
                            &((char*)base_buffer)[0],
                            &((char*)base_buffer)[0],
                            sz + sizeof(int) );

                } else {

                    comm_write_x( proc_id,
                            &((char*)base_buffer)[0],
                            &((char*)base_buffer)[0],
                            sz );

                    comm_nbi_write( proc_id,
                            &((char*)base_buffer)[sz],
                            &((char*)base_buffer)[sz],
                            1 );

                }
            } else {

                /* wait to receive in base buffer */
                comm_poll_char_while_zero(&((char*)base_buffer)[sz]);

                if (!base_buffer_alloc) {
                    intranode_partners[intranode_pindex++] = partner;
                }
            }
        }

#endif

    /* all images except for source image needs to copy from base_buffer to
     * source */
    if (_this_image != source_image) {
        memcpy(source, base_buffer, sz);
    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else {
        memset(base_buffer, 0, sz+sizeof(int));
    }

    if (!base_buffer_alloc && is_leader) {
        /* notify partner that wrote to me for broadcast that my collectives
         * buffer is free for next time */
        me = leader_index+1;
        for (i = 0; i < internode_pindex; i++) {
            coll_flags_t val = 1;
            int proc_id = leader_set[internode_partners[i]-1];
            int x,y;

            /* find my bcast_go flag on partner */
            y = me - internode_partners[i];
            x = 0;
            while (y/2 != 0) {
                y = y / 2;
                x += 1;
            }

            comm_nbi_write(proc_id, &current_team->bcast_go[2*x+bufid],
                    &val, sizeof(val));
        }
        free(internode_partners);
    }

#ifdef BINOMIAL_INTRANODE_BCAST
    if (!base_buffer_alloc) {
        me = intranode_index;
        for (i = 0; i < intranode_pindex; i++) {
            coll_flags_t val = 1;
            int proc_id = current_team->intranode_set[intranode_partners[i]];
            int x,y;

            /* find my bcast_go flag on partner */
            y = me - intranode_partners[i];
            x = 0;
            while (y/2 != 0) {
                y = y / 2;
                x += 1;
            }

            comm_nbi_write(proc_id, &current_team->bcast_go[2*(log2_p+x)+bufid],
                    &val, sizeof(val));
        }

        free(intranode_partners);
    }
#endif

    current_team->bcast_bufid = 1 - bufid;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}


void CO_BROADCAST__(void *source, INTEGER4 * source_image,
                    INTEGER4 * stat, char * errmsg, DopeVectorType *source_dv,
                    int errmsg_len)
{
    INTEGER8 source_size;
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_BCAST);

#ifdef MPI_AVAIL
    if (!mpi_collectives_available && enable_collectives_mpi) {
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
    if (mpi_collectives_available && enable_collectives_mpi &&
        (current_team == NULL || current_team->depth == 0)) {
        /* adding barrier here to ensure communication progress before
         * entering MPI Bcast routine */
        comm_sync_all(NULL, 0, NULL, 0);
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

static void co_reduce_to_image__(void *source, int *result_image, int *size,
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
    void *collectives_buf;
    size_t collectives_bufsz;
    int bufid;
    int pindex = 0;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    sz = *size;

    if ((enable_collectives_2level || enable_reduction_2level)
        && current_team) {
        co_reduce_to_image_2level__(source, result_image, size, elem_size_p,
                                   charlen, type, opr);
        PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
        return;

        /* does not reach */
    }

    if (current_team != NULL) {
        bufid = current_team->reduce_bufid;
        collectives_buf = reduce_buffer[bufid];
        collectives_bufsz = reduce_bufsize;
    } else {
        Error("current_team is not set!");
    }


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

    if (collectives_bufsz < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        const int num_steps = log2_p;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((num_steps+1)*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb-1;

        if (collectives_max_workbufs >= 1) {
              num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_p) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;

    } else if (collectives_bufsz < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const int num_steps = log2_p;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(num_steps*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_p) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = collectives_buf;

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;

    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buf;
        work_buffers = &((char*)collectives_buf)[(sz+1)*elem_size];
        num_bufs = MIN(((int)collectives_bufsz-k)/k, log2_p);
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }
    }


    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;

#ifndef NEW_REDUCE

    pot_partners = malloc(log2_p*sizeof(int));
    partners = malloc(log2_p*sizeof(int));

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

                if (0 /*enable_collectives_use_canary*/) {

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

#else

    if (!base_buffer_alloc)
        partners = malloc(log2_p*sizeof(int));

    if (num_bufs > 1) {
        step = 1;
        for (i = 1; i <= log2_p; i += num_bufs) {
            /* send sync notifications prior to each round except the first
             * one */
            if (i > 1) {
                int s = step;
                for (j = 1; j <= MIN(num_bufs,log2_p-i+1); j++) {
                    partner = 0;
                    if (((me-1)%(2*s)) == 0) {
                        if ((me+s) <= p) {
                            partner = me+s;
                        }
                    } else if (((me-1)%(2*s)) == s) {
                        if ((me-s) >= 1) {
                            partner = me-s;
                        }
                    }

                    s = s * 2;
                    if (partner < 1 || partner > p) continue;

                    if (me < partner) {
                        /* notify partner I'm ready to receive data */
                        coll_flags_t val = 1;

                        proc_id = get_proc_id(current_team, partner);

                        comm_nbi_write(proc_id, current_team->reduce_flag, 
                                       &val, sizeof(val));
                    }
                }
            }

            for (j = 1; j <= MIN(num_bufs,log2_p-i+1); j++) {

                partner = 0;
                if (((me-1)%(2*step)) == 0) {
                    if ((me+step) <= p) {
                        partner = me+step;
                    }
                } else if (((me-1)%(2*step)) == step) {
                    if ((me-step) >= 1) {
                        partner = me-step;
                    }
                }

                step = step * 2;

                if (partner < 1 || partner > p) continue;

                k1 = (j-1)*(sz+1)+1;
                k2 = (j)*(sz+1);

                proc_id = get_proc_id(current_team, partner);

                if (me > partner) {

                    if (!base_buffer_alloc) {
                        /* wait for reduce-go to be set */
                        comm_poll_char_while_zero(&current_team->reduce_go[bufid]);
                        current_team->reduce_go[bufid] = 0;
                    }

                    if (i > 1) {
                        /* subsequent rounds, wait for notification signal
                         * from partner */
                        comm_poll_char_while_zero(current_team->reduce_flag);
                        *current_team->reduce_flag = 0;
                    }

                    comm_write_x( proc_id,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            &((char*)base_buffer)[0],
                            sz*elem_size );

                    comm_nbi_write( proc_id,
                            &((char*)work_buffers)[(k2-1)*elem_size],
                            &((char*)base_buffer)[sz*elem_size],
                            1 );

                } else {
                    /* poll on flag */
                    comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                    ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                    if (!base_buffer_alloc) partners[pindex++] = proc_id;

                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_udr(opr, base_buffer,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                sz, elem_size, *charlen, type);
                }
            }
        }
    } else {
        step = 1;
        for (i = 1; i <= log2_p; i += 1) {

            partner = 0;
            if (((me-1)%(2*step)) == 0) {
                if ((me+step) <= p) {
                    partner = me+step;
                }
            } else if (((me-1)%(2*step)) == step) {
                if ((me-step) >= 1) {
                    partner = me-step;
                }
            }

            step = step * 2;

            if (partner < 1 || partner > p) continue;

            int image_id = get_proc_id(current_team, partner) + 1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            k1 = 1;
            k2 = sz+1;
            proc_id = get_proc_id(current_team, partner);

            if (me > partner) {

                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );
            } else {

                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                if (!base_buffer_alloc) partners[pindex++] = proc_id;

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_udr(opr, base_buffer,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            sz, elem_size, *charlen, type);
            }
        }
    }


#endif

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
        memset(base_buffer, 0, (sz+1)*elem_size);
        coarray_deallocate_(work_buffers, NULL);
    } else {
        memset(base_buffer, 0, (num_bufs+1)*(sz+1)*elem_size);
    }

#ifdef NEW_REDUCE
    if (!base_buffer_alloc) {
        /* notify partners that wrote to me for reduce that my collectives
         * buffers are free for next time */
        for (i = 0; i < pindex; i++) {
            coll_flags_t val = 1;
            comm_nbi_write(partners[i], &current_team->reduce_go[bufid],
                           &val, sizeof(val));
        }

        free(partners);
    }
#else

    free(pot_partners);
    free(partners);
#endif

    current_team->reduce_bufid = 1 - bufid;

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

static void co_reduce_to_image_2level__(void *source, int *result_image, int *size,
                          int *elem_size_p, int *charlen, int type,
                          void *opr)
{
    int k;
    int p, pp, q, qq, me, partner, proc_id;
    int i, j, step, log2_q, log2_qq, val;
    int k1, k2;
    int num_bufs, nbufs1, nbufs2;
    size_t elem_size;
    int sz;
    void *base_buffer;
    void *work_buffers;
    int base_buffer_alloc = 0;
    int work_buffers_alloc = 0;
    void *collectives_buf;
    size_t collectives_bufsz;
    int bufid;
    int *partners;
    int pindex = 0;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);


    if (current_team != NULL) {
        bufid = current_team->reduce_bufid;
        collectives_buf = reduce_buffer[bufid];
        collectives_bufsz = reduce_bufsize;
    } else {
        Error("current_team is not set!");
    }

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

    /* find intranode index into intranode_set */
    int intranode_index = -1;
    {
        int i;
        for (i = 1; i <= intranode_count; i++) {
            if (current_team->intranode_set[i] == my_proc) {
                intranode_index = i;
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
    pp = intranode_count;

    /* find log2_qq, ceil( log2(pp) ) */
    qq = 1;
    log2_qq = 0;
    while ( (2*qq) <= pp) {
        qq = 2*qq;
        log2_qq = log2_qq + 1;
    }
    if (qq < pp)
        log2_qq = log2_qq + 1;

    /* find log2_q, ceil( log2(p) ) */
    q = 1;
    log2_q = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_q = log2_q + 1;
    }
    if (q < p)
        log2_q = log2_q + 1;

    elem_size = *elem_size_p;


    k = (sz+1)*elem_size;
    if (collectives_bufsz < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        /* find largest number of work buffers that can be accomodated.
         *
         * Goal is to get log2_q + log2_qq buffers. If nb is the number of
         * buffers that can be allocated from the symmetric heap, it must be
         * at least 2 (otherwise we do not have space for both the base buffer
         * and one work buffer).
         */

        const int num_steps = log2_q + log2_qq;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((num_steps+1)*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb-1;

        if (collectives_max_workbufs >= 1) {
              num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q+log2_qq) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;

        /*
         * Now that we have the number of work buffers, we need to determine
         * how many of them are reserved for inter-node communication (nbufs1)
         * and how many for intra-node communication (nbufs2).
         * Since inter-node communication is more costly, we try to use as
         * many for this as we can. 
         *
         * */

        nbufs1 = 0;
        nbufs2 = 0;
        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }
    } else if (collectives_bufsz < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const int num_steps = log2_q + log2_qq;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(num_steps*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q+log2_qq) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = collectives_buf;

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;

        nbufs1 = 0;
        nbufs2 = 0;
        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }
    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buf;
        work_buffers = &((char*)collectives_buf)[(sz+1)*elem_size];
        num_bufs = MIN(((int)collectives_bufsz-k)/k, log2_q + log2_qq);
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        nbufs1 = 0;
        nbufs2 = 0;

        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }
    }

    if (!base_buffer_alloc)
        partners = malloc((log2_q+log2_qq)*sizeof(int));

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;


    /* Phase 1: Reduce to Leader */

    me = intranode_index;

    if (nbufs2 > 0) {
        step = 1;
        for (i = 1; i <= log2_qq; i += nbufs2) {
            /* send sync notifications prior to each round except the first
             * one */
            if (i > 1) {
                int s = step;
                for (j = 1; j <= MIN(nbufs2,log2_qq-i+1); j++) {
                    partner = 0;
                    if (((me-1)%(2*s)) == 0) {
                        if ((me+s) <= pp) {
                            partner = me+s;
                        }
                    } else if (((me-1)%(2*s)) == s) {
                        if ((me-s) >= 1) {
                            partner = me-s;
                        }
                    }

                    s = s * 2;
                    if (partner < 1 || partner > pp) continue;

                    if (me < partner) {
                        /* notify partner I'm ready to receive data */
                        coll_flags_t *flag;

                        proc_id = current_team->intranode_set[partner];

                        flag = comm_get_sharedptr(current_team->reduce_flag,
                                                  proc_id);

                        *flag = 1;
                    }
                }
            }

            for (j = 1; j <= MIN(nbufs2,log2_qq-i+1); j++) {

                partner = 0;
                if (((me-1)%(2*step)) == 0) {
                    if ((me+step) <= pp) {
                        partner = me+step;
                    }
                } else if (((me-1)%(2*step)) == step) {
                    if ((me-step) >= 1) {
                        partner = me-step;
                    }
                }

                step = step * 2;

                if (partner < 1 || partner > pp) continue;

                k1 = (nbufs1+j-1)*(sz+1)+1;
                k2 = (nbufs1+j)*(sz+1);

                proc_id = current_team->intranode_set[partner];

                if (me > partner) {

                    if (!base_buffer_alloc) {
                        /* wait for reduce-go to be set */
                        comm_poll_char_while_zero(&current_team->reduce_go[bufid]);
                        current_team->reduce_go[bufid] = 0;
                    }

                    if (i > 1) {
                        /* subsequent rounds, wait for notification signal
                         * from partner */
                        comm_poll_char_while_zero(current_team->reduce_flag);
                        *current_team->reduce_flag = 0;
                    }

                    comm_write_x( proc_id,
                                  &((char*)work_buffers)[(k1-1)*elem_size],
                                  &((char*)base_buffer)[0],
                                  sz*elem_size );

                    comm_nbi_write( proc_id,
                                    &((char*)work_buffers)[(k2-1)*elem_size],
                                    &((char*)base_buffer)[sz*elem_size],
                                    1 );
                } else {
                    /* poll on flag */
                    comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                    ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                    if (!base_buffer_alloc) partners[pindex++] = proc_id;

                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_udr(opr, base_buffer,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                sz, elem_size, *charlen, type);
                }
            }
        }
    } else {
        step = 1;
        for (i = 1; i <= log2_qq; i += 1) {

            partner = 0;
            if (((me-1)%(2*step)) == 0) {
                if ((me+step) <= pp) {
                    partner = me+step;
                }
            } else if (((me-1)%(2*step)) == step) {
                if ((me-step) >= 1) {
                    partner = me-step;
                }
            }

            step = step * 2;

            if (partner < 1 || partner > pp) continue;

            int image_id = current_team->intranode_set[partner] + 1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);


            k1 = (nbufs1)*(sz+1)+1;
            k2 = (nbufs1+1)*(sz+1);

            proc_id = current_team->intranode_set[partner];

            if (me > partner) {

                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );
            } else {

                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                if (!base_buffer_alloc) partners[pindex++] = proc_id;

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_udr(opr, base_buffer,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            sz, elem_size, *charlen, type);
            }
        }
    }


    /* Phase 2: Leaders reduce to the first leader */

    if (is_leader) {
        me = leader_index+1;

        if (nbufs1 > 0) {
            step = 1;
            for (i = 1; i <= log2_q; i += nbufs1) {
                /* send sync notifications prior to each round except the first
                 * one */
                if (i > 1) {
                    int s = step;
                    for (j = 1; j <= MIN(nbufs1,log2_q-i+1); j++) {
                        partner = 0;
                        if (((me-1)%(2*s)) == 0) {
                            if ((me+s) <= p) {
                                partner = me+s;
                            }
                        } else if (((me-1)%(2*s)) == s) {
                            if ((me-s) >= 1) {
                                partner = me-s;
                            }
                        }

                        s = s * 2;
                        if (partner < 1 || partner > p) continue;

                        if (me < partner) {
                            /* notify partner I'm ready to receive data */
                            coll_flags_t val = 1;

                            proc_id = leader_set[partner-1];

                            comm_nbi_write(proc_id, current_team->reduce_flag, 
                                           &val, sizeof(val));
                        }
                    }
                }

                for (j = 1; j <= MIN(nbufs1,log2_q-i+1); j++) {

                    partner = 0;
                    if (((me-1)%(2*step)) == 0) {
                        if ((me+step) <= p) {
                            partner = me+step;
                        }
                    } else if (((me-1)%(2*step)) == step) {
                        if ((me-step) >= 1) {
                            partner = me-step;
                        }
                    }

                    step = step * 2;

                    if (partner < 1 || partner > p) continue;

                    k1 = (j-1)*(sz+1)+1;
                    k2 = (j)*(sz+1);

                    proc_id = leader_set[partner-1];

                    if (me > partner) {

                        if (!base_buffer_alloc) {
                            /* wait for reduce-go to be set */
                            comm_poll_char_while_zero(&current_team->reduce_go[bufid]);
                            current_team->reduce_go[bufid] = 0;
                        }

                        if (i > 1) {
                            /* subsequent rounds, wait for notification signal
                             * from partner */
                            comm_poll_char_while_zero(current_team->reduce_flag);
                            *current_team->reduce_flag = 0;
                        }

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

                        ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                        if (!base_buffer_alloc) partners[pindex++] = proc_id;

                        /* reduce:
                         *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                         */

                        perform_udr(opr, base_buffer,
                                    &((char*)work_buffers)[(k1-1)*elem_size],
                                    sz, elem_size, *charlen, type);
                    }
                }
            }
        } else {
            step = 1;
            for (i = 1; i <= log2_q; i += 1) {

                partner = 0;
                if (((me-1)%(2*step)) == 0) {
                    if ((me+step) <= p) {
                        partner = me+step;
                    }
                } else if (((me-1)%(2*step)) == step) {
                    if ((me-step) >= 1) {
                        partner = me-step;
                    }
                }

                step = step * 2;

                if (partner < 1 || partner > p) continue;

                int image_id = leader_set[partner-1] + 1;
                comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);


                k1 = 1;
                k2 = sz+1;

                proc_id = leader_set[partner-1];

                if (me > partner) {
                    comm_write_x( proc_id,
                                  &((char*)work_buffers)[(k1-1)*elem_size],
                                  &((char*)base_buffer)[0],
                                  sz*elem_size );

                    comm_nbi_write( proc_id,
                                    &((char*)work_buffers)[(k2-1)*elem_size],
                                    &((char*)base_buffer)[sz*elem_size],
                                    1 );
                } else {

                    /* poll on flag */
                    comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                    ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                    if (!base_buffer_alloc) partners[pindex++] = proc_id;

                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_udr(opr, base_buffer,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                sz, elem_size, *charlen, type);
                }
            }
        }

    }


    /* TODO: can be a bit more clever here and avoid this extra copy, by
     * temporarily treating the result image as the leader of its
     * node for this reduction. */
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

        comm_write_x( proc_id,
                &((char*)base_buffer)[0],
                &((char*)base_buffer)[0],
                sz*elem_size );

        comm_nbi_write( proc_id,
                &((char*)base_buffer)[sz*elem_size],
                &((char*)base_buffer)[sz*elem_size],
                1 );
    }


    if (is_leader) {
      free(local_images);
    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else if (work_buffers_alloc) {
        memset(base_buffer, 0, (sz+1)*elem_size);
        coarray_deallocate_(work_buffers, NULL);
    } else {
        memset(base_buffer, 0, (num_bufs+1)*(sz+1)*elem_size);
    }

    if (!base_buffer_alloc) {
        /* notify partners that wrote to me for reduce that my collectives
         * buffers are free for next time */
        for (i = 0; i < pindex; i++) {
            coll_flags_t val = 1;
            comm_nbi_write(partners[i], &current_team->reduce_go[bufid],
                           &val, sizeof(val));
        }
        free(partners);
    }


    current_team->reduce_bufid = 1 - bufid;

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}


static void co_reduce_to_all__(void *source, int *size, int *elem_size_p,
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
    void *collectives_buf;
    size_t collectives_bufsz;
    int bufid;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    sz = *size;

    if ((enable_collectives_2level || enable_reduction_2level)
        && current_team) {
        co_reduce_to_all_2level__(source, size, elem_size_p, charlen, type, opr);
        PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
        LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
        return;

        /* does not reach */
    }

    if (current_team != NULL) {
        bufid = current_team->allreduce_bufid;
        collectives_buf = allreduce_buffer[bufid];
        collectives_bufsz = allreduce_bufsize;
    } else {
        Error("current_team is not set!");
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

    if (collectives_bufsz < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        const int num_steps = log2_q;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((num_steps+1)*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb-1;

        if (collectives_max_workbufs >= 1) {
              num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;

    } else if (collectives_bufsz < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const int num_steps = log2_q;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(num_steps*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = collectives_buf;

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;

    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buf;
        work_buffers = &((char*)collectives_buf)[(sz+1)*elem_size];
        num_bufs = MIN(((int)collectives_bufsz-k)/k, log2_q);
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


        if (0 /*enable_collectives_use_canary*/) {

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


#ifndef NEW_ALLREDUCE

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


                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );


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

#else

    /* first q processes do recursive doubling algorithm */
    if (me <= q && num_bufs > 1) {
        step = 1;
        for (i = 1; i <= log2_q; i += num_bufs) {

            /* send sync notifications prior to each round except the first
             * one */
            if (i > 1) {
                int s = step;
                for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {
                    coll_flags_t val = 1;
                    if (((me-1)%(2*s)) < s) {
                        proc_id = get_proc_id(current_team, me+s);
                    } else {
                        proc_id = get_proc_id(current_team, me-s);
                    }

                    comm_nbi_write(proc_id,
                        &current_team->allreduce_sync[2*(i+j-2)+bufid],
                        &val, sizeof(val));

                    s = s * 2;
                }
            }

            for (j = 1; j <= MIN(num_bufs, log2_q-i+1); j++) {
                int partner;
                k1 = (j-1)*(sz+1)+1;
                k2 = j*(sz+1);

                if (((me-1)%(2*step)) < step) {
                    partner = me+step;
                } else {
                    partner = me-step;
                }

                proc_id = get_proc_id(current_team, partner);

                if (i > 1) {
                    /* subsequent rounds, wait for notification signal
                     * from partner */

                    comm_poll_char_while_zero(
                        &current_team->allreduce_sync[2*(i+j-2)+bufid]);
                    current_team->allreduce_sync[2*(i+j-2)+bufid] = 0;
                }

                comm_write_x( proc_id,
                        &((char*)work_buffers)[(k1-1)*elem_size],
                        &((char*)base_buffer)[0],
                        sz*elem_size );

                comm_nbi_write( proc_id,
                        &((char*)work_buffers)[(k2-1)*elem_size],
                        &((char*)base_buffer)[sz*elem_size],
                        1 );


                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_udr(opr, base_buffer,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            sz, elem_size, *charlen, type);

                step = step * 2;
            }

        }
    } else if (me <= q && num_bufs == 1) {
        step = 1;
        for (i = 1; i <= log2_q; i += 1) {
            k1 = 1;
            k2 = sz+1;

            if (((me-1)%(2*step)) < step) {
                proc_id = get_proc_id(current_team, me+step);
            } else {
                proc_id = get_proc_id(current_team, me-step);
            }

            int image_id = proc_id + 1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

            comm_write_x( proc_id,
                    &((char*)work_buffers)[(k1-1)*elem_size],
                    &((char*)base_buffer)[0],
                    sz*elem_size );

            comm_nbi_write( proc_id,
                    &((char*)work_buffers)[(k2-1)*elem_size],
                    &((char*)base_buffer)[sz*elem_size],
                    1 );


            /* poll on flag */
            comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

            ((char*)work_buffers)[(k2-1)*elem_size] = 0;

            /* reduce:
             *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
             */

            perform_udr(opr, base_buffer,
                        &((char*)work_buffers)[(k1-1)*elem_size],
                        sz, elem_size, *charlen, type);

            step = step * 2;
        }
    }
#endif

    /* first r processes put values to last r processes */
    if (me <= r) {
        partner = me + q;
        proc_id = get_proc_id(current_team, partner);

        _SYNC_IMAGES (&partner, 1, NULL, 0, NULL, 0);

        if (0 /*enable_collectives_use_canary*/) {

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
        memset(base_buffer, 0, (sz+1)*elem_size);
        coarray_deallocate_(work_buffers, NULL);
    } else {
        memset(base_buffer, 0, (num_bufs+1)*(sz+1)*elem_size);
    }

    current_team->allreduce_bufid = 1 - bufid;

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}

static void co_reduce_to_all_2level__(void *source, int *size, int *elem_size_p,
                               int *charlen, int type, void *opr)
{
    int k;
    int p, pp, q, qq, r, me, partner, proc_id;
    int i, j, step, log2_q, log2_qq, val;
    int k1, k2;
    int num_bufs, nbufs1, nbufs2;
    size_t elem_size;
    int sz;
    void *base_buffer;
    void *work_buffers;
    int base_buffer_alloc = 0;
    int work_buffers_alloc = 0;
    int do_mpi;
    int ierr;
    void *collectives_buf;
    size_t collectives_bufsz;
    int bufid;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    if (current_team != NULL) {
        bufid = current_team->allreduce_bufid;
        collectives_buf = allreduce_buffer[bufid];
        collectives_bufsz = allreduce_bufsize;
    } else {
        Error("current_team is not set!");
    }

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

    /* find intranode index into intranode_set */
    int intranode_index = -1;
    {
        int i;
        for (i = 1; i <= intranode_count; i++) {
            if (current_team->intranode_set[i] == my_proc) {
                intranode_index = i;
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
    pp = intranode_count;

    /* find log2_qq, ceil( log2(pp) ) */
    qq = 1;
    log2_qq = 0;
    while ( (2*qq) <= pp) {
        qq = 2*qq;
        log2_qq = log2_qq + 1;
    }
    if (qq < pp)
        log2_qq = log2_qq + 1;

    /* find greatest power of 2, q, less than p */
    q = 1;
    log2_q = 0;
    while ( (2*q) <= p) {
        q = 2*q;
        log2_q = log2_q + 1;
    }

    /* r is the number of remaining processes, after q */
    r = p - q;

    elem_size = *elem_size_p;

    k = (sz+1)*elem_size;
    if (collectives_bufsz < k) {
        /* not enough room in collectives buffer for the base or work
         * buffer(s) */

        /* find largest number of work buffers that can be accomodated.
         *
         * Goal is to get log2_q + log2_qq buffers. If nb is the number of
         * buffers that can be allocated from the symmetric heap, it must be
         * at least 2 (otherwise we do not have space for both the base buffer
         * and one work buffer).
         */

        const int num_steps = log2_q + log2_qq;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail((num_steps+1)*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb-1;

        if (collectives_max_workbufs >= 1) {
              num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q+log2_qq) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = coarray_allocatable_allocate_(
                (num_bufs+1)*(sz+1)*elem_size, NULL, NULL);

        work_buffers = &((char*)base_buffer)[(sz+1)*elem_size];
        base_buffer_alloc = 1;

        /*
         * Now that we have the number of work buffers, we need to determine
         * how many of them are reserved for inter-node communication (nbufs1)
         * and how many for intra-node communication (nbufs2).
         * Since inter-node communication is more costly, we try to use as
         * many for this as we can. 
         *
         * */

        nbufs1 = 0;
        nbufs2 = 0;
        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }
    } else if (collectives_bufsz < 2*k) {
        /* not enough room in collectives buffer for the work buffer(s) */

        /* find largest number of work buffers that can be accomodated */

        const int num_steps = log2_q + log2_qq;

        const unsigned long largest_slot =
            largest_allocatable_slot_avail(num_steps*(sz+1)*elem_size);

        const unsigned long nb = ((int)largest_slot)/k;

        num_bufs = nb;

        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        if (num_bufs == 0 && ((log2_q+log2_qq) != 0)) {
            Error("not enough buffer space for collectives!");
        }

        base_buffer = collectives_buf;

        work_buffers = coarray_allocatable_allocate_(
                num_bufs*(sz+1)*elem_size, NULL, NULL);

        work_buffers_alloc = 1;

        nbufs1 = 0;
        nbufs2 = 0;
        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }
    } else {
        /* collectives buffer is large enough to accomodate at least one work
         * buffer, so use that */
        base_buffer = collectives_buf;
        work_buffers = &((char*)collectives_buf)[(sz+1)*elem_size];
        num_bufs = MIN(((int)collectives_bufsz-k)/k, log2_q + log2_qq);
        if (collectives_max_workbufs >= 1) {
          num_bufs = MIN(num_bufs, collectives_max_workbufs);
        }

        nbufs1 = 0;
        nbufs2 = 0;

        if (num_bufs > 1) {
            if (log2_q == 0) {
              nbufs2 = num_bufs;
            } else if (log2_qq == 0) {
              nbufs1 = num_bufs;
            } else if (num_bufs <= log2_q) {
              nbufs1 = num_bufs - 1;
              nbufs2 = 1;
            } else {
              nbufs1 = log2_q;
              nbufs2 = num_bufs - log2_q;
            }
        }
    }

    memcpy(base_buffer, source, sz*elem_size);
    ((char *)base_buffer)[sz*elem_size] = 1;

    /* Phase 1: Reduce to Leader */

    me = intranode_index;

    if (nbufs2 > 0) {
        step = 1;
        for (i = 1; i <= log2_qq; i += nbufs2) {
            /* send sync notifications prior to each round except the first
             * one */
            if (i > 1) {
                int s = step;
                for (j = 1; j <= MIN(nbufs2,log2_qq-i+1); j++) {
                    partner = 0;
                    if (((me-1)%(2*s)) == 0) {
                        if ((me+s) <= pp) {
                            partner = me+s;
                        }
                    } else if (((me-1)%(2*s)) == s) {
                        if ((me-s) >= 1) {
                            partner = me-s;
                        }
                    }

                    s = s * 2;
                    if (partner < 1 || partner > pp) continue;

                    if (me < partner) {
                        /* notify partner I'm ready to receive data */
                        coll_flags_t *flag;

                        proc_id = current_team->intranode_set[partner];

                        flag = comm_get_sharedptr(current_team->reduce_flag,
                                                  proc_id);

                        *flag = 1;
                    }
                }
            }

            for (j = 1; j <= MIN(nbufs2,log2_qq-i+1); j++) {

                partner = 0;
                if (((me-1)%(2*step)) == 0) {
                    if ((me+step) <= pp) {
                        partner = me+step;
                    }
                } else if (((me-1)%(2*step)) == step) {
                    if ((me-step) >= 1) {
                        partner = me-step;
                    }
                }

                step = step * 2;

                if (partner < 1 || partner > pp) continue;

                k1 = (nbufs1+j-1)*(sz+1)+1;
                k2 = (nbufs1+j)*(sz+1);

                if (me > partner) {

#if 0
                    if (!base_buffer_alloc) {
                        /* wait for reduce-go to be set */
                        comm_poll_char_while_zero(&current_team->reduce_go[bufid]);

                        /* no need for reset since we'll be doing a broadcast
                         * in the last phase of the all-reduce */
                    }
#endif

                    if (i > 1) {
                        /* subsequent rounds, wait for notification signal
                         * from partner */
                        comm_poll_char_while_zero(current_team->reduce_flag);
                        *current_team->reduce_flag = 0;
                    }

                    proc_id = current_team->intranode_set[partner];

                    comm_write_x( proc_id,
                                  &((char*)work_buffers)[(k1-1)*elem_size],
                                  &((char*)base_buffer)[0],
                                  sz*elem_size );

                    comm_nbi_write( proc_id,
                                    &((char*)work_buffers)[(k2-1)*elem_size],
                                    &((char*)base_buffer)[sz*elem_size],
                                    1 );
                } else {
                    /* poll on flag */
                    comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                    ((char*)work_buffers)[(k2-1)*elem_size] = 0;


                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_udr(opr, base_buffer,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                sz, elem_size, *charlen, type);
                }
            }
        }
    } else {
        step = 1;
        for (i = 1; i <= log2_qq; i += 1) {

            partner = 0;
            if (((me-1)%(2*step)) == 0) {
                if ((me+step) <= pp) {
                    partner = me+step;
                }
            } else if (((me-1)%(2*step)) == step) {
                if ((me-step) >= 1) {
                    partner = me-step;
                }
            }

            step = step * 2;

            if (partner < 1 || partner > pp) continue;

            int image_id = current_team->intranode_set[partner] + 1;
            comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);


            k1 = (nbufs1)*(sz+1)+1;
            k2 = (nbufs1+1)*(sz+1);

            if (me > partner) {
                proc_id = current_team->intranode_set[partner];

                comm_write_x( proc_id,
                              &((char*)work_buffers)[(k1-1)*elem_size],
                              &((char*)base_buffer)[0],
                              sz*elem_size );

                comm_nbi_write( proc_id,
                                &((char*)work_buffers)[(k2-1)*elem_size],
                                &((char*)base_buffer)[sz*elem_size],
                                1 );
            } else {

                /* poll on flag */
                comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);

                ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_udr(opr, base_buffer,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            sz, elem_size, *charlen, type);
            }
        }
    }


    /* Phase 2: Leaders perform reduction using recursive-doubling */

    if (is_leader) {
        me = leader_index+1;

        /* last r processes put values to first r processes */
        if (me > q) {
            partner = me - q;
            proc_id = leader_set[partner-1];

            comm_poll_char_while_zero(current_team->reduce_flag);
            *(current_team->reduce_flag) = 0;

            if (enable_collectives_use_canary) {
                int k1 = nbufs1*(sz+1)+1;

                comm_nbi_write( proc_id,
                        &((char*)work_buffers)[(k1-1)*elem_size],
                        &((char*)base_buffer)[0],
                        sz*elem_size+1 );

            } else {
                int k1 = nbufs1*(sz+1)+1;
                int k2 = (nbufs1+1)*(sz+1);

                comm_write_x( proc_id,
                        &((char*)work_buffers)[(k1-1)*elem_size],
                        &((char*)base_buffer)[0],
                        sz*elem_size );

                comm_nbi_write( proc_id,
                        &((char*)work_buffers)[(k2-1)*elem_size],
                        &((char*)base_buffer)[sz*elem_size],
                        1 );

            }

        } else if (me <= r) {
            partner = me + q;
            proc_id = leader_set[partner-1];
            int k1 = nbufs1*(sz+1)+1;
            int k2 = (nbufs1+1)*(sz+1);

            /* notify sender that I'm ready */
            comm_nbi_write(proc_id, current_team->reduce_flag, &val,
                           sizeof(val));

            /* poll on flag */
            comm_poll_char_while_zero(&((char*)work_buffers)[(k2-1)*elem_size]);
            ((char *)work_buffers)[(k2-1)*elem_size] = 0;

            /* reduce:
             *   work_buf(1:sz) = work_buf(1:sz) + work_buf(sz+2:2*(sz+1)-1)
             */

            perform_udr(opr, base_buffer,
                        &((char*)work_buffers)[(k1-1)*elem_size],
                        sz, elem_size, *charlen, type);
        }

        /* first q processes do recursive doubling algorithm */
        if (me <= q && nbufs1 > 0) {
            step = 1;
            for (i = 1; i <= log2_q; i += nbufs1) {

                /* send sync notifications prior to each round except the first
                 * one */
                if (i > 1) {
                    int s = step;
                    for (j = 1; j <= MIN(nbufs1, log2_q-i+1); j++) {
                        coll_flags_t val = 1;
                        if (((me-1)%(2*s)) < s) {
                            proc_id = leader_set[me+s-1];
                        } else {
                            proc_id = leader_set[me-s-1];
                        }

                        comm_nbi_write(proc_id,
                            &current_team->allreduce_sync[2*(i+j-2)+bufid],
                            &val, sizeof(val));

                        s = s * 2;
                    }
                }

                for (j = 1; j <= MIN(nbufs1, log2_q-i+1); j++) {
                    int partner;
                    k1 = (j-1)*(sz+1)+1;
                    k2 = j*(sz+1);

                    if (((me-1)%(2*step)) < step) {
                        partner = me+step;
                    } else {
                        partner = me-step;
                    }

                    proc_id = leader_set[partner-1];

                    if (i > 1) {
                        /* subsequent rounds, wait for notification signal
                         * from partner */

                        comm_poll_char_while_zero(
                            &current_team->allreduce_sync[2*(i+j-2)+bufid]);
                        current_team->allreduce_sync[2*(i+j-2)+bufid] = 0;
                    }

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

                    ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                    /* reduce:
                     *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                     */

                    perform_udr(opr, base_buffer,
                                &((char*)work_buffers)[(k1-1)*elem_size],
                                sz, elem_size, *charlen, type);

                    step = step * 2;
                }

            }
        } else if (me <= q && nbufs1 == 0) {
            step = 1;
            for (i = 1; i <= log2_q; i += 1) {
                k1 = 1;
                k2 = sz+1;

                if (((me-1)%(2*step)) < step) {
                    proc_id = leader_set[me+step-1];
                } else {
                    proc_id = leader_set[me-step-1];
                }

                int image_id = proc_id + 1;
                comm_sync_images(&image_id, 1, NULL, 0, NULL, 0);

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

                ((char*)work_buffers)[(k2-1)*elem_size] = 0;

                /* reduce:
                 *   work_buf(1:sz) = work_buf(1:sz) + work_buf(k1:k2-1)
                 */

                perform_udr(opr, base_buffer,
                            &((char*)work_buffers)[(k1-1)*elem_size],
                            sz, elem_size, *charlen, type);

                step = step * 2;
            }
        }

        /* first r processes put values to last r processes */
        if (me <= r) {
            partner = me + q;
            proc_id = leader_set[partner-1];

            comm_poll_char_while_zero(current_team->reduce_flag);
            *(current_team->reduce_flag) = 0;

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

            /* notify sender that I'm ready to receive in base buffer */
            comm_nbi_write(proc_id, current_team->reduce_flag, &val,
                           sizeof(val));

            /* poll on flag */
            comm_poll_char_while_zero(&((char*)base_buffer)[sz*elem_size]);
        }

    }

    /* Phase 3: Leader writes values back to its non-leaders */
#if defined(GASNET)
    if (is_leader) {
        int i;
        /* NOTE: assuming that intranode_count doesn't exceed 256! */
        *(current_team->allreduce_flag) = intranode_count-1;
        for (i = 1; i < intranode_count; i++) {
            int proc_id = current_team->intranode_set[i+1];
            coll_flags_t *flag = comm_get_sharedptr(current_team->allreduce_flag,
                                                    proc_id);
            *flag = 1;
        }

        /* wait for non-leaders to finish reading buffer */
        comm_poll_char_while_nonzero(current_team->allreduce_flag);
    } else {
        int leader_proc_id = current_team->intranode_set[1];
        coll_flags_t *leader_flag = comm_get_sharedptr(current_team->allreduce_flag,
                                                       leader_proc_id);

        /* poll on flag */
        comm_poll_char_while_zero(current_team->allreduce_flag);

        comm_read(leader_proc_id, &((char*)base_buffer)[0],
                  &((char*)base_buffer)[0], sz*elem_size);

        *(current_team->allreduce_flag) = 0;

        SYNC_FETCH_AND_ADD((coll_flags_t *)leader_flag, -1);
    }
#else
    if (is_leader) {
        int i;
        for (i = 1; i < intranode_count; i++) {
            comm_write_x(current_team->intranode_set[i+1], &((char*)base_buffer)[0],
                    &((char*)base_buffer)[0], sz);
        }
        /* NOTE: assumes that SYNC_IMAGES_HASH is NOT defined  */
        comm_sync_images(local_images, intranode_count-1, NULL, 0, NULL, 0);
    } else {
        int my_leader_image = current_team->intranode_set[1] + 1;
        comm_sync_images(&my_leader_image, 1, NULL, 0, NULL, 0);
    }
#endif

    memcpy(source, base_buffer, sz*elem_size);

    if (is_leader) {
      free(local_images);
    }

    if (base_buffer_alloc) {
        coarray_deallocate_(base_buffer, NULL);
    } else if (work_buffers_alloc) {
        memset(base_buffer, 0, (sz+1)*elem_size);
        coarray_deallocate_(work_buffers, NULL);
    } else {
        memset(base_buffer, 0, (num_bufs+1)*(sz+1)*elem_size);
    }

    current_team->allreduce_bufid = 1 - bufid;

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

/* assuming here that dest is remotely accessible */
void co_gather_to_all__(void *source, void *dest, int size, int elem_size)
{
    int p, q, me, proc_id;
    int i, j, step, log2_p, val;
    size_t block_size;
#ifdef MPI_AVAIL
    MPI_Datatype dtype;
#endif
    void *collectives_buf;
    size_t collectives_bufsz;

    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "entry");
    PROFILE_FUNC_ENTRY(CAFPROF_REDUCE);

    if (current_team != NULL) {
      collectives_buf = collectives_buffer[0];
      collectives_bufsz = collectives_bufsize;
    } else {
      Error("current_team is not set!");
    }

    me = _this_image;
    p = _num_images;

    /* find log2_p, ceil(log2(p)) */
    q = 1;
    log2_p = 0;
    while ( q < p) {
        q = 2*q;
        log2_p++;
    }

    block_size = elem_size * size;

#ifdef MPI_AVAIL
    if (!mpi_collectives_available && enable_collectives_mpi) {
        /* check if MPI was initialized */
        if (MPI_Initialized &&
            MPI_Initialized(&mpi_collectives_available) != MPI_SUCCESS) {
            Error("MPI_Initialized check failed");
        }
    }

    if (mpi_collectives_available && enable_collectives_mpi &&
        (current_team == NULL || current_team->depth == 0)) {

        /* adding barrier here to ensure communication progress before
         * entering MPI reduce routine */

        MPI_Type_contiguous(block_size, MPI_BYTE, &dtype);
        MPI_Type_commit(&dtype);

        comm_sync_all(NULL, 0, NULL, 0);

        MPI_Allgather(source,  1, dtype, dest, 1, dtype, MPI_COMM_WORLD);

        MPI_Type_free(&dtype);
        return;

        /* does not reach */
    }
#endif

    /* copy source into first block of dest */
    memcpy(dest, source, block_size);

    /* p processes perform bruck algorithm, 1st stage
     *
     * round i:
     *    write to: me - 2^(i-1), &dest[2^(i-1) * BLK_SIZE]
     *    nbytes = MIN(2^(i-1), p-2^(i-1)) * BLK_SIZE
     */
    step = 1;
    for (i = 1; i <= log2_p; i += 1) {
        int k;
        int partners[2];
        partners[0] = 1 + (((me-1) + p) - step) % p;
        partners[1] = 1 + (((me-1) + p) + step) % p;

        k = MIN(step, p-step);

        /*
        if (partners[0] != partners[1])
            _SYNC_IMAGES(partners, 2, NULL, 0, NULL, 0);
        else
            _SYNC_IMAGES(partners, 1, NULL, 0, NULL, 0);
            */

        proc_id = get_proc_id(current_team, partners[0]);

        comm_write_x( proc_id,
                      &((char*)dest)[step*block_size],
                      dest, k * block_size );

        if (partners[0] != partners[1])
            _SYNC_IMAGES(partners, 2, NULL, 0, NULL, 0);
        else
            _SYNC_IMAGES(partners, 1, NULL, 0, NULL, 0);

        step = step * 2;
    }

    /* now, each image should do a local circular shift in its dest buffer
     */

    char *b;
    if (collectives_bufsz >= (p*block_size)) {
        b = (char *)collectives_buf;
    } else {
        b = malloc(p*block_size);
    }

    /* create copy of dest */
    memcpy(b, dest, p*block_size);

    /* copy first p-(me-1) blocks from b into end of dest */
    memcpy(&((char *)dest)[(me-1)*block_size],
           b,
           (p-(me-1))*block_size);

    /* copy last me-1 blocks from b into dest */
    memcpy(dest,
           &b[(p-(me-1))*block_size],
           (me-1)*block_size);


    if (collectives_bufsz < (p*block_size)) {
        free(b);
    } else {
        memset(b, 0, p*block_size);
    }

    PROFILE_FUNC_EXIT(CAFPROF_REDUCE);
    LIBCAF_TRACE(LIBCAF_LOG_COLLECTIVE, "exit");
}
