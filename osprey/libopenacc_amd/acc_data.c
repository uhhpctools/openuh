/**
 * Author: Rengan Xu
 * University of Houston
 */
#include "acc_data.h"

vector param_list;
acc_hashmap* map = NULL;
int MODULE_BASE;

static int __accr_remove_device_from_hashmap(void* pDevice, char* strVarname);

extern void _w2c_mstore(void* src, int src_offset, void* dst, int dst_offset, int ilength)
{
    memcpy(dst+dst_offset, src+src_offset, ilength);
}

void* __acc_malloc_handler(unsigned int size)
{
	void *ptr;
    ptr = malloc(size);
    if(ptr == NULL)
		ERROR(("Cannot allocate %u bytes of memory", (size)));
	   
	return ptr;
}
		  
void __acc_free_handler(void* ptr)
{
	if(ptr)
		free(ptr);
}

/* 
 * the data allocated by acc_malloc does not have host address
 * i.e. the host address is NULL
 */
void* acc_malloc(unsigned int size)
{
	void *ptr;
	/*the data allocated by this routine no need to put in hashmap*/
	//__accr_malloc_on_device(NULL, &ptr, size);
	ptr = __acc_malloc_handler(size);
	return ptr;
}

void acc_free(void* ptr)
{
	/* release the device memory */ 
	__acc_free_handler(ptr);
}

/* allocate memory on device */
void __accr_malloc_on_device(void* pHost, void** pDevice, 
			unsigned int size, char* strVarname)
{
	printf("Should not come to this __accr_malloc_on_device in HSA system.\n");
	exit(-1);
}

void __accr_free_on_device(void* pDevice, char* strVarname)
{
	printf("Should not come to this __accr_free_on_device in HSA system.\n");
	exit(-1);
}

void __accr_data_exit_delete(void* pHost,
                             unsigned int offset,
                             unsigned int size,
                             unsigned int type_size,
                             int async_expr, 
			     char* strVarname)
{
        void *pDevice = NULL;
}

void __accr_memin_h2d(void* pHost, 
					  void* pDevice, 
					  unsigned int size, 
					  unsigned int offset,
					  int async_expr, char* strVarname)
{
}

void __accr_memout_d2h(void* pDevice, 
					   void* pHost, 
					   unsigned int size, 
					   unsigned int offset,
					   int async_expr, char* strVarname)
{

}

void __accr_init_param_list()
{
}

/* 
 * Add a parameter to a kernel's parameter space 
 * This parameter is an array
 */
void __accr_push_kernel_param_pointer(void** pParam, char* strVarname)
{
}

/* 
 * Add a parameter to a kernel's parameter space 
 * This parameter is a scalar
 */
void __accr_push_kernel_param_scalar(void* pValue, char* strVarname)
{
}

void __accr_push_kernel_param_int(int* iValue)
{
}

void __accr_push_kernel_param_float(float* ftValue)
{
}

void __accr_push_kernel_param_double(double* dbValue)
{
}

void __accr_clean_param_list()
{
	vector_destroy(param_list);	
}

/* given a host address, find the device address in the hash map */
int __accr_get_device_addr(void* pHostAddr, void** pDeviceAddr, unsigned int istart, 
						unsigned int isize,   unsigned int type_size, char* strVarname)
{
    	*pDeviceAddr = pHostAddr;
	return 0;
}

/* 
 *start, length, size not used now
 *pBuffer is host address
 * the length is not in bytes. for example, a[10], the length here is 10, not 60.
 *determine whehter a data whose host addr is pBuffer is in the hashmap
 */
int __accr_present_create(void* pBuffer, void** pDeviceAddr, unsigned int start, 
							int length, unsigned int type_size, char* strVarname)
{
	*pDeviceAddr = pBuffer;
	return 1; 
}

/*
 * given a device address
 * determine whether it is already in the hashmap
 */
int __accr_device_addr_present(void* pDevice, char* strVarname)
{
        return 1;
}

/*
 * given a device address, remove it from the hashmap
 *
 */
static int __accr_remove_device_from_hashmap(void* pDevice, char* strVarname)
{
    return 1;
}

void __accr_reduction_buff_malloc(void** pDevice, int type_size)
{
	*pDevice = __acc_malloc_handler(type_size);
}

void __accr_free_reduction_buff(void* pDevice)
{	
        /* release the device memory */ 
	__acc_free_handler(pDevice);
}
/*
 * update the data from host to device
 * async_expr < 0: no async operation
 * async_expr == 0: async operation with default stream
 * async_expr > 0: async operation with use-specified stream
 */
void __accr_update_device_variable(void* pHost, 
								   unsigned int offset, 
								   unsigned int size,
								   unsigned int type_size,
								   int async_expr, char* strVarname)
{
}

/*
 * update the data from device to host
 */
void __accr_update_host_variable(void* pHost, 
								 unsigned int offset, 
								 unsigned int size,
								 unsigned int type_size,
								 int async_expr, char* strVarname)
{
}

/*
 * update the data from host to device asynchronously
 */
void __accr_update_device_variable_async(void* pHost, 
										 unsigned int offset, 
										 unsigned int size,
								   		 unsigned int type_size,
										 int scalar_expr, char* strVarname)
{
	
}

/*
 * update the data from device to host asynchronously
 */
void __accr_update_host_variable_async(void* pHost, 
									   unsigned int offset, 
									   unsigned int size,
								       unsigned int type_size,
									   int scalar_expr, char* strVarname)
{
}

void __accr_wait_stream(int async_expr)
{
}

void __accr_wait_all_streams(void)
{
	DEBUG(("Waiting for all streams"));
}

void __accr_wait_some_or_all_stream(int async_expr)
{
}

void __accr_destroy_all_streams(void)
{
}

int acc_async_test_(int* scalar_expr);

int acc_async_test_(int* scalar_expr)
{
}
//#pragma weak acc_async_test_ = acc_async_test

int acc_async_test(int scalar_expr)
{
}

int acc_async_test_all_(void);
#pragma weak acc_async_test_all_ = acc_async_test_all

int acc_async_test_all(void)
{
	return 1;
}

void acc_async_wait_(int* scalar_expr);
void acc_async_wait_(int* scalar_expr)
{
    acc_async_wait(*scalar_expr);
}
//#pragma weak acc_async_wait_ = acc_async_wait

void acc_async_wait(int scalar_expr)
{
	__accr_wait_stream(scalar_expr);
}


void acc_async_wait_all_(void);
#pragma weak acc_async_wait_all_ = acc_async_wait_all

void acc_async_wait_all(void)
{
	__accr_wait_all_streams();
}
