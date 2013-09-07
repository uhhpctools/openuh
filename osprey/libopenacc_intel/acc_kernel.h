/**
 * Author: Rengan Xu
 * University of Houston
 */

#ifndef __KERNEL_H__
#define __KERNEL_H__
#include "acc_common.h"
#include "acc_data.h"
#include "acc_context.h"

extern int gangs[3];
extern int vectors[3];

extern cl_kernel current_cl_kernel_handle;
//extern char cu_filename[512];
//extern CUmodule cu_module;
//extern CUfunction cu_function;
extern unsigned int shared_size;

extern void __accr_set_gangs(int x, int y, int z);

extern void __accr_set_vectors(int x, int y, int z);

extern void __accr_set_default_gang_vector(void);

extern void __accr_reset_default_gang_vector(void);

extern void __accr_set_shared_mem_size(unsigned int size);

extern void __accr_set_default_shared_mem_size(void);

extern void __accr_set_gang_num_x(int x);

extern void __accr_set_gang_num_y(int y);

extern void __accr_set_gang_num_z(int z);

extern void __accr_set_vector_num_x(int x);

extern void __accr_set_vector_num_y(int y);

extern void __accr_set_vector_num_z(int z);

extern int __accr_get_num_workers();

extern int __accr_get_num_vectors();

extern int __accr_get_total_num_gangs(void);

extern int __accr_get_total_gangs_workers();

extern int __accr_get_total_num_vectors();

extern void __accr_launchkernel(char* szKernelName, char* szKernelLib, int async_expr);

#endif
