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

#define filename "output.dat"
#define Index2D(nx, ix, iy) ((ix) + (nx)*iy)
#define real double

void jacobi_(int nx, int ny,
	real c0, real c1, real c2, real* w0, real* w1);

#define parse_arg(name, arg) \
	int name = atoi(arg); \
	if (name < 0) \
	{ \
		printf("Value for " #name " is invalid: %d\n", name); \
		exit(1); \
	}

//#define real_rand() (((real)(rand() / (double)RAND_MAX) - 0.5) * 2)
#define real_rand() (real)(rand() / (double)RAND_MAX)

void jacobi_(int nx, int ny,
	real c0, real c1, real c2, real* w0, real* w1)
{
	int i, j;
	int i00, im10, ip10, i0m1, i0p1;
	int im1m1, im1p1, ip1m1, ip1p1;

 #pragma acc data present(w0[0:nx*ny], w1[0:nx*ny])
 {
  #pragma acc kernels
  {
    #pragma acc loop independent gang(1023) vector(2)
	for(j=1; j<ny-1; j++)
	{
	    #pragma acc loop independent gang(16) vector(128)
		for(i=1; i<nx-1; i++)
		{
			i00   = Index2D(nx, i, j);
			im10  = Index2D(nx, i-1, j);
			ip10  = Index2D(nx, i+1, j);
			i0m1  = Index2D(nx, i, j-1);
			i0p1  = Index2D(nx, i, j+1);
			
			im1m1 = Index2D(nx, i-1, j-1);
			im1p1 = Index2D(nx, i-1, j+1);
			ip1m1 = Index2D(nx, i+1, j-1);
			ip1p1 = Index2D(nx, i+1, j+1);

			w1[i00] = c0*w0[i00] + 
					  c1*(w0[im10] + w0[i0m1] + w0[ip10] + w0[i0p1]) +
					  c2*(w0[im1m1] + w0[im1p1] + w0[ip1m1] + w0[ip1p1]);
		}
	}
   }
 }
}


int main(int argc, char* argv[])
{
	int i, it;
	//int fd;
	real mean;
	struct timeval tim;
	double start, end;
	if (argc != 4)
	{
		printf("Usage: %s <nx> <ny> <nt>\n", argv[0]);
		exit(1);
	}

	parse_arg(nx, argv[1]);
	parse_arg(ny, argv[2]);
	parse_arg(nt, argv[3]);

	srand(17);
	real c0 = real_rand();
	real c1 = real_rand() / 4.;
	real c2 = real_rand() / 4.;

	printf("c0 = %f, c1 = %f, c2 = %f\n", c0, c1, c2);

	int szarray = nx * ny;
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
	{
	// 4) Perform data processing iterations, keeping all data
	// on device.
		for (it = 0; it < nt; it++)
		{
			jacobi_(nx, ny, c0, c1, c2, w0, w1);
			real* w = w0; w0 = w1; w1 = w;
		}
	}

	// 5) Transfer output data back from device to host.
	#pragma acc update host (w0[0:szarray])
	}
	
	gettimeofday(&tim, NULL);
    end = tim.tv_sec + (tim.tv_usec/1000000.0);
/*
	fd = creat(filename, 00666);
	fd = open(filename, O_WRONLY);
	write(fd, w0, szarrayb);
	close(fd);
*/  
	mean = 0.0f;
	for (i = 0; i < szarray; i++)
		mean += w0[i];
	printf("Final mean = %f\n", mean/szarray);
	printf("Time for computing: %.2f s\n",end-start);

	free(w0);
	free(w1);

	return 0;
}

