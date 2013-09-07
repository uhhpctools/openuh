/**
 * Author: Rengan Xu
 * University of Houston
 */

#ifndef __ACC_REDUCE_H__
#define __ACC_REDUCE_H__
#include "acc_common.h"
#include "acc_kernel.h"

extern unsigned int nextPow2( unsigned int x );

extern int isPow2(unsigned int x);

extern void getNumBlocksAndThreads(int whichKernel, int n, int maxBlocks, int maxThreads, int* blocks, int* threads);

extern void reduce(int size, int threads, int blocks, int whichKernel, double* d_idata, double* d_odata);

extern double benchmarkReduce(int n, int numThreads, int numBlcoks, 
					   int maxThreads, int maxBlocks, 
					   int whichKernel, int cpuFinalThreshold, 
					   double* h_odata, double* d_idata, double* d_odata);

//extern double run_reduction(double *h_idata, int size, int whichKernel);
//extern void __accr_final_reduction_algorithm(double* result, double *d_idata, int type);
extern void __accr_final_reduction_algorithm(void* result, void *d_idata, char* kernel_name, char* kernel_filename, unsigned int size, unsigned int type_size);

#endif
