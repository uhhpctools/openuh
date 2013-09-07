/**
 * Author: Rengan Xu
 * University of Houston
 */

#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include "acc_common.h"
#include "acc_data.h"

typedef struct context_s{
	acc_device_t device_type;
	CUcontext cu_context;
	CUdevice cu_device;
//	char name[512];
} context_t;

struct acc_gpu_config_s{
	int num_devices;
    /* the current device */
    int device_id; 

	/* compute capability */
	int major;
	int minor;

	/* total size of global memory */
	unsigned int total_global_mem;

	/* total size of constant memory */
	int total_constant_mem;

	/* shared memory size per block */
	int shared_mem_size;

	/* registers per block */
	int regs_per_block;
	
	int max_threads_per_block;

	int max_block_dim[3];

	int max_grid_dim[3];
};

typedef struct acc_gpu_config_s acc_gpu_config_t;

extern acc_gpu_config_t *__acc_gpu_config;

extern context_t *context;

extern void __accr_setup(void);

extern void acc_init(acc_device_t);

extern void __accr_cleanup(void);

extern void acc_shutdown(acc_device_t);

extern int __acc_gpu_config_create(int device_id, acc_gpu_config_t* config);

extern void __acc_gpu_config_destroy(acc_gpu_config_t** config);
#endif
