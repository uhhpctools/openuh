/**
 * Author: Rengan Xu
 * University of Houston
 */

#include "acc_reduction.h"
#include "acc_kernel.h"

unsigned int nextPow2( unsigned int x ) 
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return ++x;
}

int isPow2(unsigned int x)
{
	if((x&(x-1)) == 0)
		return 1;
	else
		return 0;
}


void getNumBlocksAndThreads(int whichKernel, int n, int maxBlocks, int maxThreads, int* blocks, int* threads)
{
    if (whichKernel < 3)
    {
        *threads = (n < maxThreads) ? nextPow2(n) : maxThreads;
        *blocks = (n + *threads - 1) / (*threads);
    }
    else
    {
        *threads = (n < maxThreads*2) ? nextPow2((n + 1)/ 2) : maxThreads;
        *blocks = (n + ((*threads) * 2 - 1)) / ((*threads) * 2);
    }
        
    if (whichKernel == 6)
        *blocks = min(maxBlocks, *blocks);
	
}


void reduce(int size, int threads, int blocks, int whichKernel, double* d_idata, double* d_odata)
{
/*
	__accr_set_gang_num_x(blocks);
	__accr_set_gang_num_y(1);
	__accr_set_gang_num_z(1);

	__accr_set_vector_num_x(threads);
	__accr_set_vector_num_y(1);
	__accr_set_vector_num_z(1);
*/	
	CUmodule cu_module;
	CUfunction cu_function;
	int is_pow2;
	unsigned int blockSize;
	
    int smem_size = (threads <= 32) ? 2 * threads * sizeof(double) : threads * sizeof(double);

	void *args[] = {&d_idata, &d_odata, &size};
	void *args1[] = {&d_idata, &d_odata, &size, &blockSize};
	void *args2[] = {&d_idata, &d_odata, &size, &blockSize, &is_pow2};
	CUDA_CHECK( cuModuleLoad(&cu_module, "/home/rengan/openacc/uh_libopenacc/reduce.ptx") );
		
	/*remember to change the launch kernel runtime*/
	switch(whichKernel)
	{
	case 0:
		CUDA_CHECK( cuModuleGetFunction(&cu_function, cu_module, "reduce0") );
		CUDA_CHECK( cuLaunchKernel(cu_function, blocks, 1, 1,
					                     threads, 1, 1,
					                     smem_size, 
					                     NULL, args, NULL) );
		break;
		//__accr_launchkernel("reduce0", "reduce.ptx", smem_size);	
	case 1:
		CUDA_CHECK( cuModuleGetFunction(&cu_function, cu_module, "reduce1") );
		CUDA_CHECK( cuLaunchKernel(cu_function, blocks, 1, 1,
					                     threads, 1, 1,
					                     smem_size, 
					                     NULL, args, NULL) );
		break;
	case 2:
		CUDA_CHECK( cuModuleGetFunction(&cu_function, cu_module, "reduce2") );
		CUDA_CHECK( cuLaunchKernel(cu_function, blocks, 1, 1,
					                     threads, 1, 1,
					                     smem_size, 
					                     NULL, args, NULL) );
		break;
	case 3:
		CUDA_CHECK( cuModuleGetFunction(&cu_function, cu_module, "reduce3") );
		CUDA_CHECK( cuLaunchKernel(cu_function, blocks, 1, 1,
					                     threads, 1, 1,
					                     smem_size, 
					                     NULL, args, NULL) );
		break;
	case 4:
		switch(threads)
		{
			case 512:
				blockSize = 512;
				break;
			case 256:
				blockSize = 256;
				break;
			case 128:
				blockSize = 128;
				break;
			case 64:
				blockSize = 64;
				break;
			case 32:
				blockSize = 32;
				break;
			case 16:
				blockSize = 16;
				break;
			case 8:
				blockSize = 8;
				break;
			case 4:
				blockSize = 4;
				break;
			case 2:
				blockSize = 2;
				break;
			case 1:
				blockSize = 1;
				break;
			
		}
		CUDA_CHECK( cuModuleGetFunction(&cu_function, cu_module, "reduce4") );
		CUDA_CHECK( cuLaunchKernel(cu_function, blocks, 1, 1,
			   			                 threads, 1, 1,
			   			                 smem_size, 
			   			                 NULL, args1, NULL) );
		break;
	case 5:
		switch(threads)
		{
			case 512:
				blockSize = 512;
				break;
			case 256:
				blockSize = 256;
				break;
			case 128:
				blockSize = 128;
				break;
			case 64:
				blockSize = 64;
				break;
			case 32:
				blockSize = 32;
				break;
			case 16:
				blockSize = 16;
				break;
			case 8:
				blockSize = 8;
				break;
			case 4:
				blockSize = 4;
				break;
			case 2:
				blockSize = 2;
				break;
			case 1:
				blockSize = 1;
				break;
			
		}
		CUDA_CHECK( cuModuleGetFunction(&cu_function, cu_module, "reduce5") );
		CUDA_CHECK( cuLaunchKernel(cu_function, blocks, 1, 1,
			   			                 threads, 1, 1,
			   			                 smem_size, 
			   			                 NULL, args1, NULL) );
		break;
	case 6:
	default:
		is_pow2 = isPow2(size);
		//void *args1[] = {&d_idata, &d_odata, &size, &blockSize, &is_pow2};
	
		switch(threads)
		{
			case 512:
				blockSize = 512;
				break;
			case 256:
				blockSize = 256;
				break;
			case 128:
				blockSize = 128;
				break;
			case 64:
				blockSize = 64;
				break;
			case 32:
				blockSize = 32;
				break;
			case 16:
				blockSize = 16;
				break;
			case 8:
				blockSize = 8;
				break;
			case 4:
				blockSize = 4;
				break;
			case 2:
				blockSize = 2;
				break;
			case 1:
				blockSize = 1;
				break;
				
		}
		CUDA_CHECK( cuModuleGetFunction(&cu_function, cu_module, "reduce6") );
		CUDA_CHECK( cuLaunchKernel(cu_function, blocks, 1, 1,
			   			                 threads, 1, 1,
			   			                 smem_size, 
			   			                 NULL, args2, NULL) );
		
	}
	 
}

double benchmarkReduce(int n, int numThreads, int numBlocks, 
					   int maxThreads, int maxBlocks, 
					   int whichKernel, int cpuFinalThreshold, 
					   double* h_odata, double* d_idata, double* d_odata)
{
	int i;
	double gpu_result = 0;
	int needReadBack;

	needReadBack = 1;

	reduce(n, numThreads, numBlocks, whichKernel, d_idata, d_odata);

	int s = numBlocks;
	int kernel = whichKernel;
	while(s > cpuFinalThreshold)
	{
		int threads = 0, blocks = 0;
		getNumBlocksAndThreads(kernel, s, maxBlocks, maxThreads, &blocks, &threads);
		reduce(s, threads, blocks, kernel, d_odata, d_odata);
		
		if(kernel < 3)
			s = (s + threads - 1)/threads;
		else
			s = (s + (threads*2-1))/(threads*2);
	}

	if(s > 1)
	{
		cutilRTSafeCall( cudaMemcpy(h_odata, d_odata, s*sizeof(double), cudaMemcpyDeviceToHost) );

		for(i=0; i<s; i++)
			gpu_result += h_odata[i];

		needReadBack = 0;
	}

	if(needReadBack)
		cutilRTSafeCall( cudaMemcpy(&gpu_result, d_odata, sizeof(double), cudaMemcpyDeviceToHost) );

	return gpu_result;
}

#if 0
void __accr_final_reduction_algorithm(double* result, double *d_idata, int type)
{
	int maxThreads, maxBlocks;
    int cpuFinalThreshold = 1;
	int whichKernel;
	int size;

	maxBlocks = 64;
	maxThreads = 256;
	whichKernel = 6; //default kernel is 6th

	/*the number of elements for the reduction data*/
	size = gangs[0]*gangs[1]*gangs[2]*vectors[0]*vectors[1]*vectors[2];
//	unsigned int bytes = size*sizeof(double);
	DEBUG(("The size for the reduction data: %d", size));
	
	int numBlocks = 0;
	int numThreads = 0;
	
	getNumBlocksAndThreads(whichKernel, size, maxBlocks, maxThreads, &numBlocks, &numThreads);
    if (numBlocks == 1) 
		cpuFinalThreshold = 1;
	
    // allocate mem for the result on host side
    double* h_odata = (double*) malloc(numBlocks*sizeof(double));
	//double* d_idata = NULL;
	double* d_odata = NULL;

//	cudaMalloc((void**)&d_idata, bytes);
	cutilRTSafeCall( cudaMalloc((void**)&d_odata, numBlocks*sizeof(double)) );

//	cudaMemcpy(d_idata, h_idata, bytes, cudaMemcpyHostToDevice);
	//cudaMemcpy(d_odata, h_idata, numBlocks*sizeof(double), cudaMemcpyHostToDevice);
	cutilRTSafeCall( cudaMemcpy(d_odata, d_idata, numBlocks*sizeof(double), cudaMemcpyDeviceToDevice) );

	reduce(size, numThreads, numBlocks, whichKernel, d_idata, d_odata);

	*result = benchmarkReduce(size, numThreads, numBlocks, 
								 maxThreads, maxBlocks, 
								 whichKernel, cpuFinalThreshold, 
								 h_odata, d_idata, d_odata);

}
#endif

void __accr_final_reduction_algorithm(void* device_result, void *d_idata, char* kernel_name, 
                                char* szKernelLib, char* szKernelPtx, unsigned int size, 
                                unsigned int type_size)
{
    unsigned int block_size;
    //void *__device_result;

    block_size = 128;

    __accr_set_gangs(1, 1, 1);
    __accr_set_vectors(block_size, 1, 1);
    //__accr_malloc_on_device(result, &__device_result, type_size);
    __accr_push_kernel_param_pointer(&d_idata);
    __accr_push_kernel_param_pointer(&device_result);
    __accr_push_kernel_param_scalar(&size);
    __accr_push_kernel_param_scalar(&block_size);
    //__accr_set_shared_mem_size(block_size*type_size);
    __accr_launchkernelex(kernel_name, szKernelLib, szKernelPtx, 64, -2);
    
    //__accr_memout_d2h(__device_result, result, type_size, 0, -2); 
    //__accr_free_on_device(__device_result);
}
