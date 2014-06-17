/*
  Copyright UT-Battelle, LLC.  All Rights Reserved. 2014
  Oak Ridge National Laboratory
*/

/*
  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  UT-BATTELLE, LLC AND THE GOVERNMENT MAKE NO REPRESENTATIONS AND DISCLAIM ALL
  WARRANTIES, BOTH EXPRESSED AND IMPLIED.  THERE ARE NO EXPRESS OR IMPLIED
  WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, OR THAT
  THE USE OF THE SOFTWARE WILL NOT INFRINGE ANY PATENT, COPYRIGHT, TRADEMARK,
  OR OTHER PROPRIETARY RIGHTS, OR THAT THE SOFTWARE WILL ACCOMPLISH THE
  INTENDED RESULTS OR THAT THE SOFTWARE OR ITS USE WILL NOT RESULT IN INJURY
  OR DAMAGE.  THE USER ASSUMES RESPONSIBILITY FOR ALL LIABILITIES, PENALTIES,
  FINES, CLAIMS, CAUSES OF ACTION, AND COSTS AND EXPENSES, CAUSED BY,
  RESULTING FROM OR ARISING OUT OF, IN WHOLE OR IN PART THE USE, STORAGE OR
  DISPOSAL OF THE SOFTWARE.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston MA 02111-1307, USA.

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan
*/

#ifndef _SHMEM_H
#define _SHMEM_H 1

#define SHMEM_VERSION 1.0

#include <sys/types.h>
#include <complex.h>
#include <stdio.h>
/*
 * not all compilers support this annotation
 *
 * Yes: GNU, PGI, Intel   (others?)
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

extern void   start_pes(int npes){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_init(void){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_finalize(void){printf("\nOpenSHMEM Analyzer\n");};

extern int    shmem_my_pe(void) {printf("\nOpenSHMEM Analyzer\n");};
extern int    my_pe(void) {printf("\nOpenSHMEM Analyzer\n");};
extern int    _my_pe(void) {printf("\nOpenSHMEM Analyzer\n");};

extern int    shmem_num_pes(void) {printf("\nOpenSHMEM Analyzer\n");};
extern int    shmem_n_pes(void) {printf("\nOpenSHMEM Analyzer\n");};
extern int    num_pes(void) {printf("\nOpenSHMEM Analyzer\n");};
extern int    _num_pes(void) {printf("\nOpenSHMEM Analyzer\n");};

extern char * shmem_nodename(void) {printf("\nOpenSHMEM Analyzer\n");}

extern char * shmem_version(void) {printf("\nOpenSHMEM Analyzer\n");};

/*
 * I/O
 */

extern void   shmem_short_put(short *dest, const short *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_int_put(int *dest, const int *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_long_put(long *dest, const long *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_longlong_put(long long *dest, const long long *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_longdouble_put(long double *dest, const long double *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_double_put(double *dest, const double *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_float_put(float *dest, const float *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_putmem(void *dest, const void *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_put32(void *dest, const void *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_put64(void *dest, const void *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_put128(void *dest, const void *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void   shmem_short_get(short *dest, const short *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_int_get(int *dest, const int *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_long_get(long *dest, const long *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_longlong_get(long long *dest, const long long *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_longdouble_get(long double *dest, const long double *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_double_get(double *dest, const double *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_float_get(float *dest, const float *src, size_t len, int pe){
               int *a=&pe; 
               printf("\nOpenSHMEM Analyzer\n %d %d",len,a);

};
extern void   shmem_getmem(void *dest, const void *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_get32(void *dest, const void *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_get64(void *dest, const void *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_get128(void *dest, const void *src, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void   shmem_char_p(char *addr, char value, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_short_p(short *addr, short value, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_int_p(int *addr, int value, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_long_p(long *addr, long value, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_longlong_p(long long *addr, long long value, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_float_p(float *addr, float value, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_double_p(double *addr, double value, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_longdouble_p(long double *addr, long double value, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern char        shmem_char_g(char *addr, int pe) {return *addr;};
extern short       shmem_short_g(short *addr, int pe) {return *addr;};
extern int         shmem_int_g(int *addr, int pe) {return *addr;};
extern long        shmem_long_g(long *addr, int pe) {return *addr;};
extern long long   shmem_longlong_g(long long *addr, int pe) {return *addr;};
extern float       shmem_float_g(float *addr, int pe) {return *addr;};
extern double      shmem_double_g(double *addr, int pe) {return *addr;};
extern long double shmem_longdouble_g(long double *addr, int pe) {return *addr;};


#if 0
/*
 * non-blocking I/O
 */

extern void * shmem_short_put_nb(short *dest, const short *src, size_t len, int pe) ();
extern void * shmem_int_put_nb(int *dest, const int *src, size_t len, int pe) ();
extern void * shmem_long_put_nb(long *dest, const long *src, size_t len, int pe) ();
extern void * shmem_longlong_put_nb(long long *dest, const long long *src, size_t len, int pe) ();
extern void * shmem_longdouble_put_nb(long double *dest, const long double *src, size_t len, int pe) ();
extern void * shmem_double_put_nb(double *dest, const double *src, size_t len, int pe) ();
extern void * shmem_float_put_nb(float *dest, const float *src, size_t len, int pe) ();
extern void * shmem_putmem_nb(void *dest, const void *src, size_t len, int pe) ();
extern void * shmem_put32_nb(void *dest, const void *src, size_t len, int pe) ();
extern void * shmem_put64_nb(void *dest, const void *src, size_t len, int pe) ();
extern void * shmem_put128_nb(void *dest, const void *src, size_t len, int pe) ();

#endif /* NOTYET */

/*
 * strided I/O
 */

typedef long ptrdiff_t;

extern void  shmem_double_iput(double *target, const double *source,
			       ptrdiff_t tst, ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_float_iput(float *target, const float *source,
			      ptrdiff_t tst, ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_int_iput(int *target, const int *source,  ptrdiff_t  tst,
			    ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_iput32(void  *target, const void *source, ptrdiff_t tst,
			  ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_iput64(void *target, const void *source,  ptrdiff_t  tst,
			  ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_iput128(void *target, const void *source, ptrdiff_t tst,
			   ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_long_iput(long *target, const long *source, ptrdiff_t tst,
			     ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_longdouble_iput(long double *target, const long double *source,
				   ptrdiff_t tst, ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_longlong_iput(long long *target, const long long *source,
				ptrdiff_t tst, ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_short_iput(short *target, const short *source, ptrdiff_t tst,
			      ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};


extern void  shmem_double_iget(double *target, const double *source,
			       ptrdiff_t tst, ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_float_iget(float *target, const float *source,
			      ptrdiff_t tst, ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_int_iget(int *target, const int *source,  ptrdiff_t  tst,
			    ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_iget32(void  *target, const void *source, ptrdiff_t tst,
			  ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_iget64(void *target, const void *source,  ptrdiff_t  tst,
			  ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_iget128(void *target, const void *source, ptrdiff_t tst,
			   ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_long_iget(long *target, const long *source, ptrdiff_t tst,
			     ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_longdouble_iget(long double *target, const long double *source,
				   ptrdiff_t tst, ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_longlong_iget(long long *target, const long long *source,
				ptrdiff_t tst, ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};

extern void  shmem_short_iget(short *target, const short *source, ptrdiff_t tst,
			      ptrdiff_t sst, size_t len, int pe){printf("\nOpenSHMEM Analyzer\n");};


/*
 * barriers
 */

extern void   shmem_barrier_all(void){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_barrier(int PE_start, int logPE_stride, int PE_size,
                            long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_fence(void){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_quiet(void){printf("\nOpenSHMEM Analyzer\n");};

/*
 * accessibility
 */

extern int shmem_pe_accessible(int pe) {return 1;};
extern int shmem_addr_accessible(void *addr, int pe) {return 1;};

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

extern long   malloc_error;

extern void * shmalloc(size_t size) {printf("\nOpenSHMEM Analyzer\n");};
extern void   shfree(void *ptr){printf("\nOpenSHMEM Analyzer\n");};
extern void * shrealloc(void *ptr, size_t size) {printf("\nOpenSHMEM Analyzer\n");};
extern void * shmemalign(size_t alignment, size_t size) {printf("\nOpenSHMEM Analyzer\n");};

extern void * shmem_malloc(size_t size) {printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_free(void *ptr){printf("\nOpenSHMEM Analyzer\n");};
extern void * shmem_realloc(void *ptr, size_t size) {printf("\nOpenSHMEM Analyzer\n");};
extern void * shmem_memalign(size_t alignment, size_t size) {printf("\nOpenSHMEM Analyzer\n");};

extern char * sherror(void) {printf("\nOpenSHMEM Analyzer\n");};
extern char * shmem_error(void) {printf("\nOpenSHMEM Analyzer\n");};

/*
 * wait operations
 */

/*
 * values aren't important
 */
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

extern void   shmem_short_wait_until(short *ivar, int cmp, short cmp_value){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_int_wait_until(int *ivar, int cmp, int cmp_value){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_long_wait_until(long *ivar, int cmp, long cmp_value){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_longlong_wait_until(long long *ivar, int cmp, long long cmp_value){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_wait_until(long *ivar, int cmp, long cmp_value){printf("\nOpenSHMEM Analyzer\n");};

extern void   shmem_short_wait(short *ivar, short cmp_value){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_int_wait(int *ivar, int cmp_value){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_long_wait(long *ivar, long cmp_value){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_longlong_wait(long long *ivar, long long cmp_value){printf("\nOpenSHMEM Analyzer\n");};
extern void   shmem_wait(long *ivar, long cmp_value){printf("\nOpenSHMEM Analyzer\n");};

/*
 * atomic swaps
 */

extern int         shmem_int_swap(int *target, int value, int pe) {printf("\nOpenSHMEM Analyzer\n");};
extern long        shmem_long_swap(long *target, long value, int pe) {printf("\nOpenSHMEM Analyzer\n");};
extern long long   shmem_longlong_swap(long long *target, long long value, int pe) {printf("\nOpenSHMEM Analyzer\n");};
extern float       shmem_float_swap(float *target, float value, int pe) {printf("\nOpenSHMEM Analyzer\n");};
extern double      shmem_double_swap(double *target, double value, int pe) {printf("\nOpenSHMEM Analyzer\n");};
extern long        shmem_swap(long *target, long value, int pe) {printf("\nOpenSHMEM Analyzer\n");};

extern int         shmem_int_cswap(int *target, int cond, int value, int pe) {printf("\nOpenSHMEM Analyzer\n");};
extern long        shmem_long_cswap(long *target, long cond, long value, int pe) {printf("\nOpenSHMEM Analyzer\n");};
extern long long   shmem_longlong_cswap(long long *target,
					long long cond, long long value, int pe) {printf("\nOpenSHMEM Analyzer\n");};

/*
 * atomic fetch-{add,inc} & add,inc
 */

extern int       shmem_int_fadd(int *target, int value, int pe) {return *target;};
extern long      shmem_long_fadd(long *target, long value, int pe) {return *target;};
extern long long shmem_longlong_fadd(long long *target, long long value, int pe) {return *target;};
extern int       shmem_int_finc(int *target, int pe) {return *target;};
extern long      shmem_long_finc(long *target, int pe) {return *target;};
extern long long shmem_longlong_finc(long long *target, int pe) {return *target;};

extern void      shmem_int_add(int *target, int value, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void      shmem_long_add(long *target, long value, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void      shmem_longlong_add(long long *target, long long value, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void      shmem_int_inc(int *target, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void      shmem_long_inc(long *target, int pe){printf("\nOpenSHMEM Analyzer\n");};
extern void      shmem_longlong_inc(long long *target, int pe){printf("\nOpenSHMEM Analyzer\n");};

/*
 * cache flushing
 */

extern void         shmem_clear_cache_inv(void){printf("\nOpenSHMEM Analyzer\n");};
extern void         shmem_set_cache_inv(void){printf("\nOpenSHMEM Analyzer\n");};
extern void         shmem_clear_cache_line_inv(void *target){printf("\nOpenSHMEM Analyzer\n");};
extern void         shmem_set_cache_line_inv(void *target){printf("\nOpenSHMEM Analyzer\n");};
extern void         shmem_udcflush(void){printf("\nOpenSHMEM Analyzer\n");};
extern void         shmem_udcflush_line(void *target){printf("\nOpenSHMEM Analyzer\n");};

/*
 * reductions
 */

#define SHMEM_BCAST_SYNC_SIZE 1024
#define SHMEM_SYNC_VALUE 0
#define SHMEM_REDUCE_SYNC_SIZE 128

#define _SHMEM_BCAST_SYNC_SIZE SHMEM_BCAST_SYNC_SIZE
#define _SHMEM_SYNC_VALUE SHMEM_SYNC_VALUE
#define _SHMEM_REDUCE_SYNC_SIZE SHMEM_REDUCE_SYNC_SIZE

extern void shmem_complexd_sum_to_all(double complex *target,
				      double complex *source,
				      int nreduce,
				      int PE_start, int logPE_stride, int PE_size,
				      double complex *pWrk,
				      long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_complexf_sum_to_all(float complex *target,
				      float complex *source,
				      int nreduce,
				      int PE_start, int logPE_stride, int PE_size,
				      float complex *pWrk,
				      long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_double_sum_to_all(double *target,
				    double *source,
				    int nreduce,
				    int PE_start, int logPE_stride, int PE_size,
				    double *pWrk,
				    long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_float_sum_to_all(float *target,
				   float *source,
				   int nreduce,
				   int PE_start, int logPE_stride, int PE_size,
				   float *pWrk,
				   long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_int_sum_to_all(int *target,
				 int *source,
				 int nreduce,
				 int PE_start, int logPE_stride, int PE_size,
				 int *pWrk,
				 long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_long_sum_to_all(long *target,
				  long *source,
				  int nreduce,
				  int PE_start, int logPE_stride, int PE_size,
				  long *pWrk,
				  long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longdouble_sum_to_all(long double *target,
					long double *source,
					int nreduce,
					int PE_start, int logPE_stride, int PE_size,
					long double *pWrk,
					long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longlong_sum_to_all(long long *target,
				      long long *source,
				      int nreduce,
				      int PE_start, int logPE_stride, int PE_size,
				      long long *pWrk,
				      long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_short_sum_to_all(short *target,
				   short *source,
				   int nreduce,
				   int PE_start, int logPE_stride, int PE_size,
				   short *pWrk,
				   long *pSync){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_complexd_prod_to_all(double complex *target,
				       double complex *source,
				       int nreduce,
				       int PE_start, int logPE_stride, int PE_size,
				       double complex *pWrk,
				       long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_complexf_prod_to_all(float complex *target,
				       float complex *source,
				       int nreduce,
				       int PE_start, int logPE_stride, int PE_size,
				       float complex *pWrk,
				       long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_double_prod_to_all(double *target,
				     double *source,
				     int nreduce,
				     int PE_start, int logPE_stride, int PE_size,
				     double *pWrk,
				     long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_float_prod_to_all(float *target,
				    float *source,
				    int nreduce,
				    int PE_start, int logPE_stride, int PE_size,
				    float *pWrk,
				    long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_int_prod_to_all(int *target,
				  int *source,
				  int nreduce,
				  int PE_start, int logPE_stride, int PE_size,
				  int *pWrk,
				  long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_long_prod_to_all(long *target,
				   long *source,
				   int nreduce,
				   int PE_start, int logPE_stride, int PE_size,
				   long *pWrk,
				   long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longdouble_prod_to_all(long double *target,
					 long double *source,
					 int nreduce,
					 int PE_start, int logPE_stride, int PE_size,
					 long double *pWrk,
					 long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longlong_prod_to_all(long long *target,
				       long long *source,
				       int nreduce,
				       int PE_start, int logPE_stride, int PE_size,
				       long long *pWrk,
				       long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_short_prod_to_all(short *target,
				    short *source,
				    int nreduce,
				    int PE_start, int logPE_stride, int PE_size,
				    short *pWrk,
				    long *pSync){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_int_and_to_all(int *target,
				 int *source,
				 int nreduce,
				 int PE_start, int logPE_stride, int PE_size,
				 int *pWrk,
				 long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_long_and_to_all(long *target,
				  long *source,
				  int nreduce,
				  int PE_start, int logPE_stride, int PE_size,
				  long  *pWrk,
				  long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longlong_and_to_all(long long *target,
				      long long *source,
				      int nreduce,
				      int PE_start, int logPE_stride, int PE_size,
				      long long *pWrk,
				      long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_short_and_to_all(short *target,
				   short *source,
				   int nreduce,
				   int PE_start, int logPE_stride, int PE_size,
				   short *pWrk,
				   long *pSync){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_int_or_to_all(int *target,
				int *source,
				int nreduce,
				int PE_start, int logPE_stride, int PE_size,
				int *pWrk,
				long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_long_or_to_all(long *target,
				 long *source,
				 int nreduce,
				 int PE_start, int logPE_stride, int PE_size,
				 long *pWrk,
				 long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longlong_or_to_all(long long *target,
				     long long *source,
				     int nreduce,
				     int PE_start, int logPE_stride, int PE_size,
				     long long *pWrk,
				     long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_short_or_to_all(short *target,
				  short *source,
				  int nreduce,
				  int PE_start, int logPE_stride, int PE_size,
				  short *pWrk,
				  long *pSync){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_int_xor_to_all(int *target,
				 int *source,
				 int nreduce,
				 int PE_start, int logPE_stride, int PE_size,
				 int *pWrk,
				 long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_long_xor_to_all(long *target,
				  long *source,
				  int nreduce,
				  int PE_start, int logPE_stride, int PE_size,
				  long *pWrk,
				  long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longlong_xor_to_all(long long *target,
				      long long *source,
				      int nreduce,
				      int PE_start, int logPE_stride, int PE_size,
				      long long *pWrk,
				      long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_short_xor_to_all(short *target,
				   short *source,
				   int nreduce,
				   int PE_start, int logPE_stride, int PE_size,
				   short *pWrk,
				   long *pSync){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_int_max_to_all(int *target,
				 int *source,
				 int nreduce,
				 int PE_start, int logPE_stride, int PE_size,
				 int *pWrk,
				 long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_long_max_to_all(long *target,
				  long *source,
				  int nreduce,
				  int PE_start, int logPE_stride, int PE_size,
				  long *pWrk,
				  long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longlong_max_to_all(long long *target,
				      long long *source,
				      int nreduce,
				      int PE_start, int logPE_stride, int PE_size,
				      long long *pWrk,
				      long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_short_max_to_all(short *target,
				   short *source,
				   int nreduce,
				   int PE_start, int logPE_stride, int PE_size,
				   short *pWrk,
				   long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longdouble_max_to_all(long double *target,
					long double *source,
					int nreduce,
					int PE_start, int logPE_stride, int PE_size,
					long double *pWrk,
					long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_float_max_to_all(float *target,
				   float *source,
				   int nreduce,
				   int PE_start, int logPE_stride, int PE_size,
				   float *pWrk,
				   long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_double_max_to_all(double *target,
				    double *source,
				    int nreduce,
				    int PE_start, int logPE_stride, int PE_size,
				    double *pWrk,
				    long *pSync){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_int_min_to_all(int *target,
				 int *source,
				 int nreduce,
				 int PE_start, int logPE_stride, int PE_size,
				 int *pWrk,
				 long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_long_min_to_all(long *target,
				  long *source,
				  int nreduce,
				  int PE_start, int logPE_stride, int PE_size,
				  long *pWrk,
				  long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longlong_min_to_all(long long *target,
				      long long *source,
				      int nreduce,
				      int PE_start, int logPE_stride, int PE_size,
				      long long *pWrk,
				      long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_short_min_to_all(short *target,
				   short *source,
				   int nreduce,
				   int PE_start, int logPE_stride, int PE_size,
				   short *pWrk,
				   long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_longdouble_min_to_all(long double *target,
					long double *source,
					int nreduce,
					int PE_start, int logPE_stride, int PE_size,
					long double *pWrk,
					long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_float_min_to_all(float *target,
				   float *source,
				   int nreduce,
				   int PE_start, int logPE_stride, int PE_size,
				   float *pWrk,
				   long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_double_min_to_all(double *target,
				    double *source,
				    int nreduce,
				    int PE_start, int logPE_stride, int PE_size,
				    double *pWrk,
				    long *pSync){printf("\nOpenSHMEM Analyzer\n");};

/*
 * broadcasts
 */

extern void shmem_broadcast32(void *target, const void *source, size_t nlong,
                              int PE_root, int PE_start, int logPE_stride, int PE_size,
                              long *pSync){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_broadcast64(void *target, const void *source, size_t nlong,
                              int PE_root, int PE_start, int logPE_stride, int PE_size,
                              long *pSync){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_sync_init(long *pSync){printf("\nOpenSHMEM Analyzer\n");};

/*
 * collects
 */

extern void shmem_fcollect32(void *target, const void *source, size_t nlong,
			     int PE_start, int logPE_stride, int PE_size,
			     long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_fcollect64(void *target, const void *source, size_t nlong,
			     int PE_start, int logPE_stride, int PE_size,
			     long *pSync){printf("\nOpenSHMEM Analyzer\n");};

extern void shmem_collect32(void *target, const void *source, size_t nlong,
			    int PE_start, int logPE_stride, int PE_size,
			    long *pSync){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_collect64(void *target, const void *source, size_t nlong,
			    int PE_start, int logPE_stride, int PE_size,
			    long *pSync){printf("\nOpenSHMEM Analyzer\n");};

/*
 * locks/critical section
 */

extern void shmem_set_lock(long *lock){printf("\nOpenSHMEM Analyzer\n");};
extern void shmem_clear_lock(long *lock){printf("\nOpenSHMEM Analyzer\n");};
extern int  shmem_test_lock(long *lock) {return 1;};


#endif /* _SHMEM_H */
