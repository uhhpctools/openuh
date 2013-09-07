/**
 * Author: Rengan Xu
 * University of Houston
 */

#include "acc_reduction.h"
#include "acc_kernel.h"

void __accr_final_reduction_algorithm(void* device_result, void *d_idata, char* kernel_name, 
                                char* kernel_filename, unsigned int size, unsigned int type_size)
{
    /*unsigned int block_size;
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
    __accr_launchkernel(kernel_name, kernel_filename, -2);
    
    //__accr_memout_d2h(__device_result, result, type_size, 0, -2); 
    //__accr_free_on_device(__device_result);i*/
}
