//===----------------------------------------------------------------------===//
//
//     KernelGen -- A prototype of LLVM-based auto-parallelizing Fortran/C
//        compiler for NVIDIA GPUs, targeting numerical modeling code.
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <openacc.h>

#define _A(array, ix, iy, is) (array[(ix) + nx * (iy) + nx * ny * (is)])
#define real double

int laplacian(int nx, int ny, int ns,
	const real alpha, const real beta,
	real* w0, real* w1)
{
	int i,j,k;
	unsigned int szarray = (unsigned int)nx * ny * ns;

#pragma acc data present(w0[0:szarray], w1[0:szarray])
{
  #pragma acc kernels
  {
    #pragma acc loop independent vector(2)
	for (k = 1; k < ns - 1; k++)
	{
		#pragma acc loop independent gang(1) vector(128)
		for (j = 1; j < ny - 1; j++)
		{
			#pragma acc loop independent gang(126)
			for (i = 1; i < nx - 1; i++)
			{
				_A(w1, i, j, k) = alpha * _A(w0, i, j, k) + beta * (
				
					_A(w0, i+1, j, k) + _A(w0, i-1, j, k) +
					_A(w0, i, j+1, k) + _A(w0, i, j-1, k) +
					_A(w0, i, j, k+1) + _A(w0, i, j, k-1));
			}
		}
	}
  }
}
	return 0;
}

#define parse_arg(name, arg) \
	int name = atoi(arg); \
	if (name < 0) \
	{ \
		printf("Value for " #name " is invalid: %d\n", name); \
		exit(1); \
	}

//#define real_rand() (((real)(rand() / (double)RAND_MAX) - 0.5) * 2)
#define real_rand() (real)(rand() / (double)RAND_MAX)

int main(int argc, char* argv[])
{
	int i, j, k, it;
    //FILE *fp;
	//int fd;
	real mean;
	struct timeval tim;
	double start, end;

	if (argc != 5)
	{
		printf("Usage: %s <nx> <ny> <ns> <nt>\n", argv[0]);
		exit(1);
	}

	srand(17);
	parse_arg(nx, argv[1]);
	parse_arg(ny, argv[2]);
	parse_arg(ns, argv[3]);
	parse_arg(nt, argv[4]);

	real alpha = real_rand();
	real beta = real_rand();

	printf("alpha = %f, beta = %f\n", alpha, beta);

	unsigned int szarray = (unsigned int)nx * ny * ns;
	unsigned int szarrayb = szarray * sizeof(real);

	real* w0 = (real*)malloc(szarrayb);
	real* w1 = (real*)malloc(szarrayb);

	if (!w0 || !w1)
	{
		printf("Error allocating memory for arrays: %p, %p\n", w0, w1);
		exit(1);
	}

	for (i = 0; i < szarray; i++)
	{
		w0[i] = real_rand();
		w1[i] = real_rand();
	}

	// 1) Perform an empty offload, that should strip
	// the initialization time from further offloads.
	acc_init(acc_device_default);
	
	gettimeofday(&tim, NULL);
	start = tim.tv_sec + (tim.tv_usec/1000000.0);

	// 2) Allocate data on device, but do not copy anything.
	#pragma acc data create (w0[0:szarray], w1[0:szarray])
	{
		// 3) Transfer data from host to device and leave it there,
		// i.e. do not allocate deivce memory buffers.
		#pragma acc update device(w0[0:szarray], w1[0:szarray])
		// 4) Perform data processing iterations, keeping all data
		// on device.
		{
			for (it = 0; it < nt; it++)
			{
				laplacian(nx, ny, ns, alpha, beta, w0, w1);
				real* w = w0; w0 = w1; w1 = w;
			}
		}

		// 5) Transfer output data back from device to host.
		#pragma acc update host (w0[0:szarray])
	}
	
	gettimeofday(&tim, NULL);
    end = tim.tv_sec + (tim.tv_usec/1000000.0);
#if 0	
	fd = creat(argv[5], 00666);
	fd = open(argv[5], O_WRONLY);
	write(fd, w0, szarrayb);
	close(fd);
    fp = fopen(argv[5], "w");
    fprintf(fp, "%d %d %d\n", ns, ny, nx);
	for(k = 0; k < ns; k++){
    	for (j = 0; j < ny; j++) {
        	for (i = 0; i < nx; i++) {
            	fprintf(fp, "%d %d %d %f\n", k, j, i, w0[k*nx*ny + j*nx + i]);
        	}
    	}
	}
    fclose(fp);
#endif
	
	mean = 0.0f;
	for (i = 0; i < szarray; i++)
		mean += w0[i];

	printf("Final mean = %f\n", mean/szarray);
	printf("Time for computing: %.2f s\n",end-start);

	free(w0);
	free(w1);

	return 0;
}

