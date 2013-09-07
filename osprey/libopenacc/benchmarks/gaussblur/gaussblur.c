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
#include <openacc_rtl.h>
#ifdef __OPENACC
	#include <openacc.h>
#endif

#define real double


#define _A(array, ix, iy) (array[(ix) + nx * (iy)])


int gaussblur(int nx, int ny,
	const real s0, const real s1, const real s2,
	const real s4, const real s5, const real s8,
	real* w0, real* w1)
{
	int j, i;

	real f = 1. / (s0 + 4 * (s1 + s2 + s4 + s8) + 8 * s5);
	unsigned int szarray = (unsigned int)nx * ny;
#pragma acc data present(w0[0:szarray], w1[0:szarray])
{
  //#pragma acc kernels param(w0,w1,nx,ny,s0,s1,s2,s4,s5,s8,f)
  #pragma acc kernels
  {
    #pragma acc loop independent gang(512) vector(2) private(j,i)
	for (j = 2; j < ny - 2; j++)
	{
		#pragma acc loop independent vector(128)
		for (i = 2; i < nx - 2; i++)
		{
			_A(w1, i, j) = f * (
				s0 *  _A(w0, i,   j  ) +
				s1 * (_A(w0, i-1, j  ) + _A(w0, i+1, j  ) + _A(w0, i  , j-1) + _A(w0, i  , j+1)) +
				s2 * (_A(w0, i-1, j-1) + _A(w0, i+1, j-1) + _A(w0, i-1, j+1) + _A(w0, i+1, j+1)) +
				s4 * (_A(w0, i-2, j  ) + _A(w0, i+2, j  ) + _A(w0, i  , j-2) + _A(w0, i  , j+2)) +
				s5 * (_A(w0, i-2, j-1) + _A(w0, i-1, j-2) + _A(w0, i+1, j-2) + _A(w0, i+2, j-1)  +
				      _A(w0, i-2, j+1) + _A(w0, i-1, j+2) + _A(w0, i+1, j+2) + _A(w0, i+2, j+1)) +
				s8 * (_A(w0, i-2, j-2) + _A(w0, i+2, j-2) + _A(w0, i-2, j+2) + _A(w0, i+2, j+2)));
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

	if (argc != 4)
	{
		printf("Usage: %s <nx> <ny> <nt>\n", argv[0]);
		exit(1);
	}


	srand(17);
	parse_arg(nx, argv[1]);
	parse_arg(ny, argv[2]);
	parse_arg(nt, argv[3]);

	real s0 = real_rand();
	real s1 = real_rand();
	real s2 = real_rand();
	real s4 = real_rand();
	real s5 = real_rand();
	real s8 = real_rand();

	printf("s0 = %f, s1 = %f, s2 = %f\n", s0, s1, s2);
	printf("s4 = %f, s5 = %f, s8 = %f\n", s4, s5, s8);

	unsigned int szarray = (unsigned int)nx * ny;
	unsigned int szarrayb = szarray * sizeof(real);

	real* w0 = (real*)malloc(szarrayb);
	real* w1 = (real*)malloc(szarrayb);

	if (!w0 || !w1)
	{
		printf("Error allocating memory for arrays: %p, %p\n", w0, w1);
		exit(1);
	}

	real mean = 0.0f;
	for (i = 0; i < szarray; i++)
	{
		w0[i] = real_rand();
		w1[i] = real_rand();
		mean += w0[i] + w1[i];
	}
	printf("initial mean = %f\n", mean / szarray / 2);

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
			gaussblur(nx, ny, s0, s1, s2, s4, s5, s8, w0, w1);
			real* w = w0; w0 = w1; w1 = w;
		}
	}

	// 5) Transfer output data back from device to host.
	#pragma acc update host (w0[0:szarray])
	}
	
	gettimeofday(&tim, NULL);
    end = tim.tv_sec + (tim.tv_usec/1000000.0);

	// For the final mean - account only the norm of the top
	// most level (tracked by swapping idxs array of indexes).
	mean = 0.0f;
	for (i = 0; i < szarray; i++)
		mean += w0[i];
	printf("final mean = %f\n", mean / szarray);
	printf("Time for computing: %.2f s\n",end-start);

	free(w0);
	free(w1);

	fflush(stdout);

	return 0;
}

