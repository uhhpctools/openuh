/*
 *
 * Copyright (c) 2014, University of Houston System and Oak Ridge National
 * Laboratory.
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * o Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 
 * o Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 * o Neither the name of the University of Houston System, Oak Ridge
 *   National Laboratory nor the names of its contributors may be used to
 *   endorse or promote products derived from this software without specific
 *   prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */



#ifndef _SHMEM_H
#define _SHMEM_H 1

#include <sys/types.h>
#include <stddef.h>               /* ptrdiff_t */

/*
 * C and C++ do complex numbers differently
 *
 */

#ifdef __cplusplus
# include <complex>
# define COMPLEXIFY(T) std::complex<T>
#else /* _cplusplus */
# include <complex.h>
# define COMPLEXIFY(T) T complex
#endif /* __cplusplus */

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

  /*
   * "Orange Mocha Frappuccino" SHMEM release
   */

#define SHMEM_MAJOR_VERSION 1
#define SHMEM_MINOR_VERSION 0

  /*
   * not all compilers support this annotation
   *
   * Yes: GNU, PGI, Intel, Open64/UH   (others?)
   * No: Sun/Oracle Studio
   *
   */
#if defined(__GNUC__) || defined(__PGIC__) || defined(__INTEL_COMPILER) || defined(__OPEN64__)
# define _WUR __attribute__((__warn_unused_result__))
#else
# define _WUR
#endif

  /*
   * init & query
   */

  extern void start_pes (int npes);
  extern int _my_pe (void) _WUR;
  extern int _num_pes (void) _WUR;

  /*
   * I/O
   */

  extern void shmem_short_put (short *dest, const short *src, size_t nelems,
			       int pe);
  extern void shmem_int_put (int *dest, const int *src, size_t nelems,
			     int pe);
  extern void shmem_long_put (long *dest, const long *src, size_t nelems,
			      int pe);
  extern void shmem_longlong_put (long long *dest, const long long *src,
				  size_t nelems, int pe);
  extern void shmem_longdouble_put (long double *dest, const long double *src,
				    size_t nelems, int pe);
  extern void shmem_double_put (double *dest, const double *src,
				size_t nelems, int pe);
  extern void shmem_complexd_put (COMPLEXIFY (double) * dest,
				  const COMPLEXIFY (double) * src,
				  size_t nelems, int pe);
  extern void shmem_float_put (float *dest, const float *src, size_t nelems,
			       int pe);
  extern void shmem_putmem (void *dest, const void *src, size_t nelems,
			    int pe);
  extern void shmem_put32 (void *dest, const void *src, size_t nelems,
			   int pe);
  extern void shmem_put64 (void *dest, const void *src, size_t nelems,
			   int pe);
  extern void shmem_put128 (void *dest, const void *src, size_t nelems,
			    int pe);

  extern void shmem_short_get (short *dest, const short *src, size_t nelems,
			       int pe);
  extern void shmem_int_get (int *dest, const int *src, size_t nelems,
			     int pe);
  extern void shmem_long_get (long *dest, const long *src, size_t nelems,
			      int pe);
  extern void shmem_longlong_get (long long *dest, const long long *src,
				  size_t nelems, int pe);
  extern void shmem_longdouble_get (long double *dest, const long double *src,
				    size_t nelems, int pe);
  extern void shmem_double_get (double *dest, const double *src,
				size_t nelems, int pe);
  extern void shmem_float_get (float *dest, const float *src, size_t nelems,
			       int pe);
  extern void shmem_getmem (void *dest, const void *src, size_t nelems,
			    int pe);
  extern void shmem_get32 (void *dest, const void *src, size_t nelems,
			   int pe);
  extern void shmem_get64 (void *dest, const void *src, size_t nelems,
			   int pe);
  extern void shmem_get128 (void *dest, const void *src, size_t nelems,
			    int pe);

  extern void shmem_char_p (char *addr, char value, int pe);
  extern void shmem_short_p (short *addr, short value, int pe);
  extern void shmem_int_p (int *addr, int value, int pe);
  extern void shmem_long_p (long *addr, long value, int pe);
  extern void shmem_longlong_p (long long *addr, long long value, int pe);
  extern void shmem_float_p (float *addr, float value, int pe);
  extern void shmem_double_p (double *addr, double value, int pe);
  extern void shmem_longdouble_p (long double *addr, long double value,
				  int pe);

  extern char shmem_char_g (char *addr, int pe) _WUR;
  extern short shmem_short_g (short *addr, int pe) _WUR;
  extern int shmem_int_g (int *addr, int pe) _WUR;
  extern long shmem_long_g (long *addr, int pe) _WUR;
  extern long long shmem_longlong_g (long long *addr, int pe) _WUR;
  extern float shmem_float_g (float *addr, int pe) _WUR;
  extern double shmem_double_g (double *addr, int pe) _WUR;
  extern long double shmem_longdouble_g (long double *addr, int pe) _WUR;


  /*
   * strided I/O
   */

  extern void shmem_double_iput (double *target, const double *source,
				 ptrdiff_t tst, ptrdiff_t sst, size_t nelems,
				 int pe);

  extern void shmem_float_iput (float *target, const float *source,
				ptrdiff_t tst, ptrdiff_t sst, size_t nelems,
				int pe);

  extern void shmem_int_iput (int *target, const int *source, ptrdiff_t tst,
			      ptrdiff_t sst, size_t nelems, int pe);

  extern void shmem_iput32 (void *target, const void *source, ptrdiff_t tst,
			    ptrdiff_t sst, size_t nelems, int pe);

  extern void shmem_iput64 (void *target, const void *source, ptrdiff_t tst,
			    ptrdiff_t sst, size_t nelems, int pe);

  extern void shmem_iput128 (void *target, const void *source, ptrdiff_t tst,
			     ptrdiff_t sst, size_t nelems, int pe);

  extern void shmem_long_iput (long *target, const long *source,
			       ptrdiff_t tst, ptrdiff_t sst, size_t nelems,
			       int pe);

  extern void shmem_longdouble_iput (long double *target,
				     const long double *source, ptrdiff_t tst,
				     ptrdiff_t sst, size_t nelems, int pe);

  extern void shmem_longlong_iput (long long *target, const long long *source,
				   ptrdiff_t tst, ptrdiff_t sst,
				   size_t nelems, int pe);

  extern void shmem_short_iput (short *target, const short *source,
				ptrdiff_t tst, ptrdiff_t sst, size_t nelems,
				int pe);


  extern void shmem_double_iget (double *target, const double *source,
				 ptrdiff_t tst, ptrdiff_t sst, size_t nelems,
				 int pe);

  extern void shmem_float_iget (float *target, const float *source,
				ptrdiff_t tst, ptrdiff_t sst, size_t nelems,
				int pe);

  extern void shmem_int_iget (int *target, const int *source, ptrdiff_t tst,
			      ptrdiff_t sst, size_t nelems, int pe);

  extern void shmem_iget32 (void *target, const void *source, ptrdiff_t tst,
			    ptrdiff_t sst, size_t nelems, int pe);

  extern void shmem_iget64 (void *target, const void *source, ptrdiff_t tst,
			    ptrdiff_t sst, size_t nelems, int pe);

  extern void shmem_iget128 (void *target, const void *source, ptrdiff_t tst,
			     ptrdiff_t sst, size_t nelems, int pe);

  extern void shmem_long_iget (long *target, const long *source,
			       ptrdiff_t tst, ptrdiff_t sst, size_t nelems,
			       int pe);

  extern void shmem_longdouble_iget (long double *target,
				     const long double *source, ptrdiff_t tst,
				     ptrdiff_t sst, size_t nelems, int pe);

  extern void shmem_longlong_iget (long long *target, const long long *source,
				   ptrdiff_t tst, ptrdiff_t sst,
				   size_t nelems, int pe);

  extern void shmem_short_iget (short *target, const short *source,
				ptrdiff_t tst, ptrdiff_t sst, size_t nelems,
				int pe);


  /*
   * barriers
   */

  extern void shmem_barrier_all (void);
  extern void shmem_barrier (int PE_start, int logPE_stride, int PE_size,
			     long *pSync);
  extern void shmem_fence (void);
  extern void shmem_quiet (void);

  /*
   * accessibility
   */

  extern int shmem_pe_accessible (int pe) _WUR;
  extern int shmem_addr_accessible (void *addr, int pe) _WUR;
  extern void *shmem_ptr (void *target, int pe) _WUR;

  /*
   * symmetric memory management
   */

  /* lower numbers match Fortran return values */

#define SHMEM_MALLOC_OK                   (0L)
#define SHMEM_MALLOC_BAD_SIZE             (-1L)
#define SHMEM_MALLOC_FAIL                 (-2L)
#define SHMEM_MALLOC_NOT_IN_SYMM_HEAP     (-3L)
#define SHMEM_MALLOC_ALREADY_FREE         (-4L)
#define SHMEM_MALLOC_NOT_ALIGNED          (-5L)

#define SHMEM_MALLOC_MEMALIGN_FAILED      (-11L)
#define SHMEM_MALLOC_REALLOC_FAILED       (-12L)
#define SHMEM_MALLOC_SYMMSIZE_FAILED      (-10L)

#define	_SHMEM_MALLOC_OK                  SHMEM_MALLOC_OK
#define _SHMEM_MALLOC_BAD_SIZE            SHMEM_MALLOC_BAD_SIZE
#define	_SHMEM_MALLOC_FAIL                SHMEM_MALLOC_FAIL
#define _SHMEM_MALLOC_NOT_IN_SYMM_HEAP    SHMEM_MALLOC_NOT_IN_SYMM_HEAP
#define	_SHMEM_MALLOC_ALREADY_FREE        SHMEM_MALLOC_ALREADY_FREE
#define	_SHMEM_MALLOC_NOT_ALIGNED         SHMEM_MALLOC_NOT_ALIGNED

#define	_SHMEM_MALLOC_MEMALIGN_FAILED     SHMEM_MALLOC_MEMALIGN_FAILED
#define	_SHMEM_MALLOC_REALLOC_FAILED      SHMEM_MALLOC_REALLOC_FAILED
#define	_SHMEM_MALLOC_SYMMSIZE_FAILED     SHMEM_MALLOC_SYMMSIZE_FAILED

#if 0
  extern long malloc_error;
#endif				/* not present in SGI version */

  extern void *shmalloc (size_t size) _WUR;
  extern void shfree (void *ptr);
  extern void *shrealloc (void *ptr, size_t size) _WUR;
  extern void *shmemalign (size_t alignment, size_t size) _WUR;

  /*
   * wait operations
   */

  /*
   * values aren't important
   */
#if 0
#define SHMEM_CMP_EQ 0
#define SHMEM_CMP_NE 1
#define SHMEM_CMP_GT 2
#define SHMEM_CMP_LE 3
#define SHMEM_CMP_LT 4
#define SHMEM_CMP_GE 5

#define _SHMEM_CMP_EQ SHMEM_CMP_EQ
#define _SHMEM_CMP_NE SHMEM_CMP_NE
#define _SHMEM_CMP_GT SHMEM_CMP_GT
#define _SHMEM_CMP_LE SHMEM_CMP_LE
#define _SHMEM_CMP_LT SHMEM_CMP_LT
#define _SHMEM_CMP_GE SHMEM_CMP_GE
#endif

  typedef enum
  {
    SHMEM_CMP_EQ = 0,
    SHMEM_CMP_NE,
    SHMEM_CMP_GT,
    SHMEM_CMP_LE,
    SHMEM_CMP_LT,
    SHMEM_CMP_GE,
    _SHMEM_CMP_EQ,
    _SHMEM_CMP_NE,
    _SHMEM_CMP_GT,
    _SHMEM_CMP_LE,
    _SHMEM_CMP_LT,
    _SHMEM_CMP_GE,
  } shmem_cmp_t;

  extern void shmem_short_wait_until (short *ivar, int cmp, short cmp_value);
  extern void shmem_int_wait_until (int *ivar, int cmp, int cmp_value);
  extern void shmem_long_wait_until (long *ivar, int cmp, long cmp_value);
  extern void shmem_longlong_wait_until (long long *ivar, int cmp,
					 long long cmp_value);
  extern void shmem_wait_until (long *ivar, int cmp, long cmp_value);

  extern void shmem_short_wait (short *ivar, short cmp_value);
  extern void shmem_int_wait (int *ivar, int cmp_value);
  extern void shmem_long_wait (long *ivar, long cmp_value);
  extern void shmem_longlong_wait (long long *ivar, long long cmp_value);
  extern void shmem_wait (long *ivar, long cmp_value);

  /*
   * atomic swaps
   */

  extern int shmem_int_swap (int *target, int value, int pe) _WUR;
  extern long shmem_long_swap (long *target, long value, int pe) _WUR;
  extern long long shmem_longlong_swap (long long *target, long long value,
					int pe) _WUR;
  extern float shmem_float_swap (float *target, float value, int pe) _WUR;
  extern double shmem_double_swap (double *target, double value, int pe) _WUR;
  extern long shmem_swap (long *target, long value, int pe) _WUR;

  extern int shmem_int_cswap (int *target, int cond, int value, int pe) _WUR;
  extern long shmem_long_cswap (long *target, long cond, long value,
				int pe) _WUR;
  extern long long shmem_longlong_cswap (long long *target, long long cond,
					 long long value, int pe) _WUR;

  /*
   * atomic fetch-{add,inc} & add,inc
   */

  extern int shmem_int_fadd (int *target, int value, int pe) _WUR;
  extern long shmem_long_fadd (long *target, long value, int pe) _WUR;
  extern long long shmem_longlong_fadd (long long *target, long long value,
					int pe) _WUR;
  extern int shmem_int_finc (int *target, int pe) _WUR;
  extern long shmem_long_finc (long *target, int pe) _WUR;
  extern long long shmem_longlong_finc (long long *target, int pe) _WUR;

  extern void shmem_int_add (int *target, int value, int pe);
  extern void shmem_long_add (long *target, long value, int pe);
  extern void shmem_longlong_add (long long *target, long long value, int pe);
  extern void shmem_int_inc (int *target, int pe);
  extern void shmem_long_inc (long *target, int pe);
  extern void shmem_longlong_inc (long long *target, int pe);

  /*
   * cache flushing (deprecated)
   */

  extern void shmem_clear_cache_inv (void);
  extern void shmem_set_cache_inv (void);
  extern void shmem_clear_cache_line_inv (void *target);
  extern void shmem_set_cache_line_inv (void *target);
  extern void shmem_udcflush (void);
  extern void shmem_udcflush_line (void *target);

  /*
   * reductions
   */

#define SHMEM_BCAST_SYNC_SIZE 64
#define SHMEM_SYNC_VALUE (-1L)
#define SHMEM_REDUCE_SYNC_SIZE 128
#define SHMEM_REDUCE_MIN_WRKDATA_SIZE SHMEM_REDUCE_SYNC_SIZE

#define _SHMEM_BCAST_SYNC_SIZE SHMEM_BCAST_SYNC_SIZE
#define _SHMEM_SYNC_VALUE SHMEM_SYNC_VALUE
#define _SHMEM_REDUCE_SYNC_SIZE SHMEM_REDUCE_SYNC_SIZE
#define _SHMEM_REDUCE_MIN_WRKDATA_SIZE SHMEM_REDUCE_MIN_WRKDATA_SIZE

  extern void shmem_complexd_sum_to_all (COMPLEXIFY (double) * target,
					 COMPLEXIFY (double) * source,
					 int nreduce,
					 int PE_start, int logPE_stride,
					 int PE_size,
					 COMPLEXIFY (double) * pWrk,
					 long *pSync);
  extern void shmem_complexf_sum_to_all (COMPLEXIFY (float) * target,
					 COMPLEXIFY (float) * source,
					 int nreduce, int PE_start,
					 int logPE_stride, int PE_size,
					 COMPLEXIFY (float) * pWrk,
					 long *pSync);
  extern void shmem_double_sum_to_all (double *target, double *source,
				       int nreduce, int PE_start,
				       int logPE_stride, int PE_size,
				       double *pWrk, long *pSync);
  extern void shmem_float_sum_to_all (float *target, float *source,
				      int nreduce, int PE_start,
				      int logPE_stride, int PE_size,
				      float *pWrk, long *pSync);
  extern void shmem_int_sum_to_all (int *target, int *source, int nreduce,
				    int PE_start, int logPE_stride,
				    int PE_size, int *pWrk, long *pSync);
  extern void shmem_long_sum_to_all (long *target, long *source, int nreduce,
				     int PE_start, int logPE_stride,
				     int PE_size, long *pWrk, long *pSync);
  extern void shmem_longdouble_sum_to_all (long double *target,
					   long double *source, int nreduce,
					   int PE_start, int logPE_stride,
					   int PE_size, long double *pWrk,
					   long *pSync);
  extern void shmem_longlong_sum_to_all (long long *target, long long *source,
					 int nreduce, int PE_start,
					 int logPE_stride, int PE_size,
					 long long *pWrk, long *pSync);
  extern void shmem_short_sum_to_all (short *target, short *source,
				      int nreduce, int PE_start,
				      int logPE_stride, int PE_size,
				      short *pWrk, long *pSync);

  extern void shmem_complexd_prod_to_all (COMPLEXIFY (double) * target,
					  COMPLEXIFY (double) * source,
					  int nreduce,
					  int PE_start, int logPE_stride,
					  int PE_size,
					  COMPLEXIFY (double) * pWrk,
					  long *pSync);
  extern void shmem_complexf_prod_to_all (COMPLEXIFY (float) * target,
					  COMPLEXIFY (float) * source,
					  int nreduce, int PE_start,
					  int logPE_stride, int PE_size,
					  COMPLEXIFY (float) * pWrk,
					  long *pSync);
  extern void shmem_double_prod_to_all (double *target, double *source,
					int nreduce, int PE_start,
					int logPE_stride, int PE_size,
					double *pWrk, long *pSync);
  extern void shmem_float_prod_to_all (float *target, float *source,
				       int nreduce, int PE_start,
				       int logPE_stride, int PE_size,
				       float *pWrk, long *pSync);
  extern void shmem_int_prod_to_all (int *target, int *source, int nreduce,
				     int PE_start, int logPE_stride,
				     int PE_size, int *pWrk, long *pSync);
  extern void shmem_long_prod_to_all (long *target, long *source, int nreduce,
				      int PE_start, int logPE_stride,
				      int PE_size, long *pWrk, long *pSync);
  extern void shmem_longdouble_prod_to_all (long double *target,
					    long double *source, int nreduce,
					    int PE_start, int logPE_stride,
					    int PE_size, long double *pWrk,
					    long *pSync);
  extern void shmem_longlong_prod_to_all (long long *target,
					  long long *source, int nreduce,
					  int PE_start, int logPE_stride,
					  int PE_size, long long *pWrk,
					  long *pSync);
  extern void shmem_short_prod_to_all (short *target, short *source,
				       int nreduce, int PE_start,
				       int logPE_stride, int PE_size,
				       short *pWrk, long *pSync);

  extern void shmem_int_and_to_all (int *target,
				    int *source,
				    int nreduce,
				    int PE_start, int logPE_stride,
				    int PE_size, int *pWrk, long *pSync);
  extern void shmem_long_and_to_all (long *target, long *source, int nreduce,
				     int PE_start, int logPE_stride,
				     int PE_size, long *pWrk, long *pSync);
  extern void shmem_longlong_and_to_all (long long *target, long long *source,
					 int nreduce, int PE_start,
					 int logPE_stride, int PE_size,
					 long long *pWrk, long *pSync);
  extern void shmem_short_and_to_all (short *target, short *source,
				      int nreduce, int PE_start,
				      int logPE_stride, int PE_size,
				      short *pWrk, long *pSync);

  extern void shmem_int_or_to_all (int *target,
				   int *source,
				   int nreduce,
				   int PE_start, int logPE_stride,
				   int PE_size, int *pWrk, long *pSync);
  extern void shmem_long_or_to_all (long *target, long *source, int nreduce,
				    int PE_start, int logPE_stride,
				    int PE_size, long *pWrk, long *pSync);
  extern void shmem_longlong_or_to_all (long long *target, long long *source,
					int nreduce, int PE_start,
					int logPE_stride, int PE_size,
					long long *pWrk, long *pSync);
  extern void shmem_short_or_to_all (short *target, short *source,
				     int nreduce, int PE_start,
				     int logPE_stride, int PE_size,
				     short *pWrk, long *pSync);

  extern void shmem_int_xor_to_all (int *target,
				    int *source,
				    int nreduce,
				    int PE_start, int logPE_stride,
				    int PE_size, int *pWrk, long *pSync);
  extern void shmem_long_xor_to_all (long *target, long *source, int nreduce,
				     int PE_start, int logPE_stride,
				     int PE_size, long *pWrk, long *pSync);
  extern void shmem_longlong_xor_to_all (long long *target, long long *source,
					 int nreduce, int PE_start,
					 int logPE_stride, int PE_size,
					 long long *pWrk, long *pSync);
  extern void shmem_short_xor_to_all (short *target, short *source,
				      int nreduce, int PE_start,
				      int logPE_stride, int PE_size,
				      short *pWrk, long *pSync);

  extern void shmem_int_max_to_all (int *target,
				    int *source,
				    int nreduce,
				    int PE_start, int logPE_stride,
				    int PE_size, int *pWrk, long *pSync);
  extern void shmem_long_max_to_all (long *target, long *source, int nreduce,
				     int PE_start, int logPE_stride,
				     int PE_size, long *pWrk, long *pSync);
  extern void shmem_longlong_max_to_all (long long *target, long long *source,
					 int nreduce, int PE_start,
					 int logPE_stride, int PE_size,
					 long long *pWrk, long *pSync);
  extern void shmem_short_max_to_all (short *target, short *source,
				      int nreduce, int PE_start,
				      int logPE_stride, int PE_size,
				      short *pWrk, long *pSync);
  extern void shmem_longdouble_max_to_all (long double *target,
					   long double *source, int nreduce,
					   int PE_start, int logPE_stride,
					   int PE_size, long double *pWrk,
					   long *pSync);
  extern void shmem_float_max_to_all (float *target, float *source,
				      int nreduce, int PE_start,
				      int logPE_stride, int PE_size,
				      float *pWrk, long *pSync);
  extern void shmem_double_max_to_all (double *target, double *source,
				       int nreduce, int PE_start,
				       int logPE_stride, int PE_size,
				       double *pWrk, long *pSync);

  extern void shmem_int_min_to_all (int *target,
				    int *source,
				    int nreduce,
				    int PE_start, int logPE_stride,
				    int PE_size, int *pWrk, long *pSync);
  extern void shmem_long_min_to_all (long *target, long *source, int nreduce,
				     int PE_start, int logPE_stride,
				     int PE_size, long *pWrk, long *pSync);
  extern void shmem_longlong_min_to_all (long long *target, long long *source,
					 int nreduce, int PE_start,
					 int logPE_stride, int PE_size,
					 long long *pWrk, long *pSync);
  extern void shmem_short_min_to_all (short *target, short *source,
				      int nreduce, int PE_start,
				      int logPE_stride, int PE_size,
				      short *pWrk, long *pSync);
  extern void shmem_longdouble_min_to_all (long double *target,
					   long double *source, int nreduce,
					   int PE_start, int logPE_stride,
					   int PE_size, long double *pWrk,
					   long *pSync);
  extern void shmem_float_min_to_all (float *target, float *source,
				      int nreduce, int PE_start,
				      int logPE_stride, int PE_size,
				      float *pWrk, long *pSync);
  extern void shmem_double_min_to_all (double *target, double *source,
				       int nreduce, int PE_start,
				       int logPE_stride, int PE_size,
				       double *pWrk, long *pSync);

  /*
   * broadcasts
   */

  extern void shmem_broadcast32 (void *target, const void *source,
				 size_t nelems, int PE_root, int PE_start,
				 int logPE_stride, int PE_size, long *pSync);

  extern void shmem_broadcast64 (void *target, const void *source,
				 size_t nelems, int PE_root, int PE_start,
				 int logPE_stride, int PE_size, long *pSync);

  /*
   * collects
   */

#define SHMEM_COLLECT_SYNC_SIZE SHMEM_BCAST_SYNC_SIZE
#define _SHMEM_COLLECT_SYNC_SIZE SHMEM_COLLECT_SYNC_SIZE

  extern void shmem_fcollect32 (void *target, const void *source,
				size_t nelems, int PE_start, int logPE_stride,
				int PE_size, long *pSync);
  extern void shmem_fcollect64 (void *target, const void *source,
				size_t nelems, int PE_start, int logPE_stride,
				int PE_size, long *pSync);

  extern void shmem_collect32 (void *target, const void *source,
			       size_t nelems, int PE_start, int logPE_stride,
			       int PE_size, long *pSync);
  extern void shmem_collect64 (void *target, const void *source,
			       size_t nelems, int PE_start, int logPE_stride,
			       int PE_size, long *pSync);

  /*
   * locks/critical section
   */

  extern void shmem_set_lock (long *lock);
  extern void shmem_clear_lock (long *lock);
  extern int shmem_test_lock (long *lock) _WUR;

  /*
   * new ideas (not part of formal 1.0 API)
   */
  extern void shmem_init (void);
  extern void shmem_finalize (void);
  extern int shmem_my_pe (void) _WUR;
  extern int shmem_num_pes (void) _WUR;
  extern int shmem_n_pes (void) _WUR;
  extern char *shmem_nodename (void) _WUR;
  extern int shmem_version (int *major, int *minor) _WUR;
  extern void *shmem_malloc (size_t size) _WUR;
  extern void shmem_free (void *ptr);
  extern void *shmem_realloc (void *ptr, size_t size) _WUR;
  extern void *shmem_memalign (size_t alignment, size_t size) _WUR;
  extern char *sherror (void) _WUR;
  extern char *shmem_error (void) _WUR;
  extern void shmem_sync_init (long *pSync);

  /*
   * --end--
   */

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* _SHMEM_H */
