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
#include <openacc.h>
#define real double

#define _A(array, is, iy, ix) (array[(ix) + nx * (iy) + nx * ny * (is)])


int wave13pt(int nx, int ny, int ns,
	const real c0, const real c1, const real c2,
	real* w0, real* w1, real* w2)
{
	int i, j, k;
	unsigned int szarray = (unsigned int)nx * ny * ns;
#pragma acc data present(w0[0:szarray], w1[0:szarray], w2[0:szarray])
{
  #pragma acc kernels
  {
	#pragma acc loop independent vector(2)
	for (k = 2; k < ns - 2; k++)
	{
		#pragma acc loop independent gang(31) vector(4)
		for (j = 2; j < ny - 2; j++)
		{
			#pragma acc loop independent gang(2) vector(64)
			for (i = 2; i < nx - 2; i++)
			{
				_A(w2, k, j, i) =  c0 * _A(w1, k, j, i) - _A(w0, k, j, i) +

					c1 * (
						_A(w1, k, j, i+1) + _A(w1, k, j, i-1)  +
						_A(w1, k, j+1, i) + _A(w1, k, j-1, i)  +
						_A(w1, k+1, j, i) + _A(w1, k-1, j, i)) +
					c2 * (
						_A(w1, k, j, i+2) + _A(w1, k, j, i-2)  +
						_A(w1, k, j+2, i) + _A(w1, k, j-2, i)  +
						_A(w1, k+2, j, i) + _A(w1, k-2, j, i));
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
	int i, it;
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

	real c0 = real_rand();
	real c1 = real_rand() / 6.;
	real c2 = real_rand() / 6.;

	printf("c0 = %f, c1 = %f, c2 = %f\n", c0, c1, c2);

    unsigned int szarray = (unsigned int)nx * ny * ns;
	unsigned int szarrayb = szarray * sizeof(real);

	real* w0 = (real*)malloc(szarrayb);
	real* w1 = (real*)malloc(szarrayb);
	real* w2 = (real*)malloc(szarrayb);

	if (!w0 || !w1 || !w2)
	{
		printf("Error allocating memory for arrays: %p, %p, %p\n", w0, w1, w2);
		exit(1);
	}

	real mean = 0.0f;
	for (i = 0; i < szarray; i++)
	{
		w0[i] = real_rand();
		w1[i] = real_rand();
		w2[i] = real_rand();
		mean += w0[i] + w1[i] + w2[i];
	}
	printf("initial mean = %f\n", mean / szarray / 3);

	// 1) Perform an empty offload, that should strip
	// the initialization time from further offloads.
	acc_init(acc_device_default);
	
	gettimeofday(&tim, NULL);
	start = tim.tv_sec + (tim.tv_usec/1000000.0);

	// 2) Allocate data on device, but do not copy anything.
	#pragma acc data create (w0[0:szarray], w1[0:szarray], w2[0:szarray])
	{
		// 3) Transfer data from host to device and leave it there,
		// i.e. do not allocate deivce memory buffers.
		#pragma acc update device(w0[0:szarray], w1[0:szarray], w2[0:szarray])
		// 4) Perform data processing iterations, keeping all data
		// on device.
		for (it = 0; it < nt; it++)
		{
			wave13pt(nx, ny, ns, c0, c1, c2, w0, w1, w2);
			real* w = w0; w0 = w1; w1 = w2; w2 = w;
		}

		// 5) Transfer output data back from device to host.
		#pragma acc update host (w1[0:szarray])
	}
	
	gettimeofday(&tim, NULL);
    end = tim.tv_sec + (tim.tv_usec/1000000.0);

	acc_shutdown(acc_device_default);
	// For the final mean - account only the norm of the top
	// most level (tracked by swapping idxs array of indexes).
	mean = 0.0f;
	for (i = 0; i < szarray; i++)
		mean += w1[i];
	printf("final mean = %f\n", mean / szarray);
	printf("Time for computing: %.2f s\n",end-start);

	free(w0);
	free(w1);
	free(w2);

	fflush(stdout);

	return 0;
}

