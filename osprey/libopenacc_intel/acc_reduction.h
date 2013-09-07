/**
 * Author: Rengan Xu
 * University of Houston
 */

#ifndef __ACC_REDUCE_H__
#define __ACC_REDUCE_H__
#include "acc_common.h"
#include "acc_kernel.h"

extern void __accr_final_reduction_algorithm(void* result, void *d_idata, char* kernel_name, char* kernel_filename, unsigned int size, unsigned int type_size);

#endif
