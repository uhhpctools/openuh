/**
 * Author: Rengan Xu
 * University of Houston
 */

/**
 * For all APIs to be used by the user (not compiler) in Fortran code, 
 * if the routine has parameters, then we cannot use pragma weak, instead
 * we should pass by reference since C by default pass the
 * argument by value but Fortran pass by reference
 */

#include "acc_context.h"
#include "acc_kernel.h"
#define FALSE	(0)
#define TRUE	(1)


context_t *context;
acc_gpu_config_t *__acc_gpu_config = NULL;
static unsigned int __accr_isInitialized = FALSE;

static void device_type_check(acc_device_t);


/*void acc_init__(acc_device_t device_type)
{
	acc_init (device_type);
}*/

void acc_init_(acc_device_t* device_type);

void acc_init_(acc_device_t* device_type)
{
    acc_init(*device_type);
}
//#pragma weak acc_init_ = acc_init

void acc_init(acc_device_t device_type)
{
	/*To do: add the support for different accel types */
	if(__accr_isInitialized == FALSE)
		__accr_isInitialized = TRUE;
	else
		return;

    cuInit(0);
	context = (context_t*)malloc(sizeof(context_t));
    /* by default, we use the first cuda device */
    acc_set_device_num(0, acc_device_nvidia);
}

int __acc_gpu_config_create(int device_id, acc_gpu_config_t* config)
{
	
    config->device_id = device_id;
    if(device_id >= config->num_devices)
        ERROR(("Error in device number"));
	
	cuDeviceComputeCapability(&(config->major), &(config->minor), device_id);
	
	cuDeviceTotalMem(&(config->total_global_mem), device_id);

	cuDeviceGetAttribute(&(config->total_constant_mem), CU_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY, device_id);

	cuDeviceGetAttribute(&(config->shared_mem_size), CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK, device_id);

	cuDeviceGetAttribute(&(config->regs_per_block), CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK, device_id);
	
	cuDeviceGetAttribute(&(config->max_threads_per_block), CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK, device_id);

	cuDeviceGetAttribute(&(config->max_block_dim[0]), CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X, device_id);

	cuDeviceGetAttribute(&(config->max_block_dim[1]), CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y, device_id);
	
	cuDeviceGetAttribute(&(config->max_block_dim[2]), CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z, device_id);
	
	cuDeviceGetAttribute(&(config->max_grid_dim[0]), CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X, device_id);
	
	cuDeviceGetAttribute(&(config->max_grid_dim[1]), CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y, device_id);
	
	cuDeviceGetAttribute(&(config->max_grid_dim[2]), CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z, device_id);
	
	INFO(("Number of devices: %d", config->num_devices));
	INFO(("Total global memory size: %lu", config->total_global_mem));
	INFO(("Total constant memory size: %d", config->total_constant_mem));
	INFO(("Shared memory size per block: %d", config->shared_mem_size));
	INFO(("Number of registers per block: %d", config->regs_per_block));
	INFO(("Max threads per block: %d", config->max_threads_per_block));
	INFO(("Max sizes of each dimension of a block: %d, %d, %d", 
											config->max_block_dim[0], 
											config->max_block_dim[1], 
											config->max_block_dim[2]));
	INFO(("Max sizes of each dimension of a grid: %d, %d, %d", 
											config->max_grid_dim[0], 
											config->max_grid_dim[1], 
											config->max_grid_dim[2]));
	return ACC_OK;
}

void __acc_gpu_config_destroy(acc_gpu_config_t** config)
{
	//acc_host_free(config);
	if(*config != NULL)
    {
	    free(*config);
        *config = NULL;
    }
}

int acc_get_num_devices_(acc_device_t* device_type);

int acc_get_num_devices_(acc_device_t* device_type)
{
    acc_get_num_devices(*device_type);
}
//#pragma weak acc_get_num_devices_ = acc_get_num_devices

int acc_get_num_devices(acc_device_t device_type)
{
    if(device_type == acc_device_none)
        return 0;
    else if(device_type == acc_device_host)
            return (sysconf(_SC_NPROCESSORS_ONLN));
    else if((device_type == acc_device_default)||
            (device_type == acc_device_not_host)||
            (device_type == acc_device_nvidia))
            return (__acc_gpu_config->num_devices);
}

static void device_type_check(acc_device_t device_type)
{
    /* only allow supported device types */
    if((device_type != acc_device_default) &&
       (device_type != acc_device_host) &&
       (device_type != acc_device_not_host) &&
       (device_type != acc_device_nvidia) &&
       (device_type != acc_device_radeon) &&
       (device_type != acc_device_xeonphi))
        ERROR(("Error in device type"));
}

void acc_set_device_num_(int* device_id, acc_device_t* device_type);

void acc_set_device_num_(int* device_id, acc_device_t* device_type)
{
    acc_set_device_num(*device_id, *device_type);
}
//#pragma weak acc_set_device_num_ = acc_set_device_num

void acc_set_device_num(int device_id, acc_device_t device_type)
{
    /* so far we do not support CPU device */
    if(device_type == acc_device_host)
    {
        if(__acc_gpu_config == NULL)
	        __acc_gpu_config = acc_host_alloc_zero(sizeof(*__acc_gpu_config));
        __acc_gpu_config->device_id = device_id;
        return;
    }
   
    /* this routine is called first  */ 
    if(__acc_gpu_config == NULL)	
        __acc_gpu_config = acc_host_alloc_zero(sizeof(*__acc_gpu_config));
	cuDeviceGetCount(&(__acc_gpu_config->num_devices));
	if(__acc_gpu_config->num_devices == 0)
	{
		ERROR(("No device available, abort"));
	}
    __acc_gpu_config_create(device_id, __acc_gpu_config);

	context->device_type = device_type;
//	cuDeviceGetName(context->name, 512, 0);
	cuDeviceGet(&(context->cu_device), device_id);
	cuCtxCreate(&(context->cu_context), 0, context->cu_device);
	
    /*
	 * used for asynchronous streams, the stream array has
	 * 12 elements, the last is reserved for default stream
	 */
	MODULE_BASE = 11;

    /* set up the default shared memory size */
    __accr_set_default_shared_mem_size();
}

int acc_get_device_num_(acc_device_t* device_type);

int acc_get_device_num_(acc_device_t* device_type)
{
    acc_get_device_num(*device_type);
}
//#pragma weak acc_get_device_num_ = acc_get_device_num

int acc_get_device_num(acc_device_t device_type)
{
    device_type_check(device_type);
    
    return __acc_gpu_config->device_id; 
}

void acc_set_device_type_(acc_device_t* device_type);

void acc_set_device_type_(acc_device_t* device_type)
{
    acc_set_device_type(*device_type);
}
//#pragma weak acc_set_device_type_ = acc_set_device_type

void acc_set_device_type(acc_device_t device_type)
{
    device_type_check(device_type);
    context->device_type = device_type;
    if(device_type == acc_device_default)
        context->device_type = acc_device_nvidia;

    /* do nothing for CPU device */
    if(device_type == acc_device_host)
        return;

    /* this routine is called before acc_set_device_num() */
    if(__acc_gpu_config == NULL)	
        __acc_gpu_config = acc_host_alloc_zero(sizeof(*__acc_gpu_config));

	cuDeviceGetCount(&(__acc_gpu_config->num_devices));
	if(__acc_gpu_config->num_devices == 0)
	{
		ERROR(("No device available, abort"));
	}
}

acc_device_t acc_get_device_type_(void);
#pragma weak acc_get_device_type_ = acc_get_device_type

acc_device_t acc_get_device_type(void)
{
    return context->device_type;
}

void __accr_cleanup(void)
{
    if(context->cu_context != NULL)
    {
	    CUresult ret;
	    ret = cuCtxDestroy(context->cu_context);
	    CUDA_CHECK(ret);
        context->cu_context = NULL;
    }
	__acc_gpu_config_destroy(&__acc_gpu_config);
}

void acc_shutdown_(acc_device_t* device_type);

void acc_shutdown_(acc_device_t* device_type)
{
    acc_shutdown(*device_type);
}

//#pragma weak acc_shutdown_ = acc_shutdown
void acc_shutdown(acc_device_t device_type)
{
    /* if the device is host, then do nothing */
    if(device_type == acc_device_host)
        return;

	if(__accr_isInitialized == TRUE)
		__accr_isInitialized = FALSE;
	else
		return;
	__accr_destroy_all_streams();
	/*To do: add the support for different accel types*/
	__accr_cleanup();
}
