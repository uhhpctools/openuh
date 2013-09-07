/*
 * Rectangular matrix multiplication, started from MIT Cilk matmul.cilk example
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <openacc_rtl.h>
#ifdef _OPENACC
	#include <openacc.h>
#endif


void zero(double *A, int n)
{
    int i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            A[i * n + j] = 0.0;
        }
    }
}

void initA(double *A, int n)
{
    int i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            A[i * n + j] = 3*i*j/n/n;
        }
    }
}

void initB(double *B, int n)
{
    int i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            B[i * n + j] = 5*j*i/n/n;
        }
    }
}

void initC(double *C, int n)
{
    int i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            C[i * n + j] = 0.0f;
        }
    }
}

void iter_matmul(double *A, double *B, double *C, int n)
{
    int i, j, k;
	double c;

	#pragma acc data copyin(A[0:n*n], B[0:n*n])	copyout(C[0:n*n])
	{
	  #pragma acc kernels
	  {
	    #pragma acc loop independent gang(8192)
    	for (i = 0; i < n; i++)
		{
			c = i;
		    #pragma acc loop independent gang(64) vector(128)
        	for (j = 0; j < n; j++) 
			{
            	c = 0.0f;
            	for (k = 0; k < n; k++)
                	c += A[i * n + k] * B[k * n + j];
	            C[i * n + j] = c;
        	}
		}
	  } 
	}
}

void verify(double *C, int n)
{
	int i, j;
	double sum;
	sum = 0.0f;
	for(i=0; i<n; i++)
		for(j=0; j<n; j++)
			sum += C[i * n + j];

	printf("Sum of C is: %f\n", sum);
}

int main(int argc, char *argv[])
{
    int n;
    double *A, *B, *C;
    double start, end;
  	struct timeval tim;

    if (argc != 2) {
        fprintf(stderr, "Usage: matmul <n>\n");
        exit(1);
    }
    n = atoi(argv[1]);

    A = malloc(n * n * sizeof(double));
    B = malloc(n * n * sizeof(double));
    C = malloc(n * n * sizeof(double));

    initA(A, n);
    initB(B, n);
    initC(C, n);
    //verify(A, n);
    //verify(B, n);

	acc_init(acc_device_default);

    /* sequential run */
    gettimeofday(&tim, NULL);
    start = tim.tv_sec + (tim.tv_usec/1000000.0);
    iter_matmul(A, B, C, n);
    gettimeofday(&tim, NULL);
    end = tim.tv_sec + (tim.tv_usec/1000000.0);

	printf("Execution time is: %.2f s\n", end-start);
	
	verify(C, n);

    free(C);
    free(B);
    free(A);
    return 0;
}
