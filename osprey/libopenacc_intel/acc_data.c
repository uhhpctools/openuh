/**
 * Author: Rengan Xu
 * University of Houston
 */
#include "acc_data.h"

vector param_list;
acc_hashmap* map = NULL;
cudaStream_t async_streams[12] = {NULL};
int MODULE_BASE;
BOOL bIsZeroCopyEnabled = FALSE;

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
	//void *ptr;
	cl_mem* cl_ptr;
	cl_ptr = (cl_mem*)malloc(sizeof(cl_mem));
	/*the data allocated by this routine no need to put in hashmap*/

	*cl_ptr = clCreateBuffer(context->opencl_context, CL_MEM_READ_WRITE, size, NULL, NULL);
	
	return cl_ptr;
}

void acc_free(void* ptr)
{
	/* release the device memory */ 
	clReleaseMemObject((cl_mem)(*((cl_mem*)ptr)));
	free(ptr);
}

/* allocate memory on device */
void __accr_malloc_on_device(void* pHost, void** pDevice, 
			unsigned int size, char* strVarname)
{
	cl_int error;
	cl_mem* cl_ptr;
	cl_ptr = (cl_mem*)malloc(sizeof(cl_mem));
	*(cl_mem**)pDevice = cl_ptr;
	
	if(bIsZeroCopyEnabled == TRUE)
		*cl_ptr = clCreateBuffer(context->opencl_context, CL_MEM_USE_HOST_PTR, size, pHost, &error);;
	else
		*cl_ptr = clCreateBuffer(context->opencl_context, CL_MEM_READ_WRITE, size, NULL, &error);

	
	*pDevice = cl_ptr;
	/*add the data in the hashmap*/
	param_t *param = (param_t*)malloc(sizeof(param_t));
	param->host_addr = pHost;
	param->device_addr = *pDevice; //cl_mem*
	param->size = size;
    param->pinned = 0;
	if(map == NULL)
		map = acc_hashmap_create();
	acc_hashmap_put(map, param->host_addr, param);
}

void __accr_free_on_device(void* pDevice, char* strVarname)
{
	if(pDevice)
    {
        /* since it will no longer be on the device, 
         * it should be removed from the hash table */
        __accr_remove_device_from_hashmap(pDevice, strVarname);
        
        /* release the device memory */ 
		clReleaseMemObject((cl_mem)(*((cl_mem*)pDevice)));
		free(pDevice);
    }
}

void __accr_data_exit_delete(void* pHost,
                             unsigned int offset,
                             unsigned int size,
                             unsigned int type_size,
                             int async_expr, 
			     char* strVarname)
{
        void *pDevice = NULL;
        __accr_get_device_addr(pHost, &pDevice, offset, size, type_size, strVarname);
	if(pDevice)
        	__accr_free_on_device(pDevice, strVarname);
}

void __accr_memin_h2d(void* pHost, 
					  void* pDevice, 
					  unsigned int size, 
					  unsigned int offset,
					  int async_expr, char* strVarname)
{
	int stream_pos;
    param_t *param;
    acc_hm_entry *e;
    int index;

	if(pHost == NULL || pDevice == NULL)
		ERROR(("Error in uploading data from host to device"));
	
	if(async_expr < 0)	
    {		
		cl_int error = clEnqueueWriteBuffer(context->cl_cq, *((cl_mem*)pDevice), CL_FALSE, 
									offset, size, pHost, 0, NULL, NULL);
		if(error  != CL_SUCCESS)
	    {
	        INFO(("acc_init error: %s", acc_get_error_string(error)));
	    }
        
	}else
    {
    }
}

void __accr_memout_d2h(void* pDevice, 
					   void* pHost, 
					   unsigned int size, 
					   unsigned int offset,
					   int async_expr, char* strVarname)
{
	int stream_pos;
    param_t *param;
    acc_hm_entry *e;
    int index;

	if(pHost == NULL || pDevice == NULL)
		ERROR(("Error in downloading data from device to host"));

	if(async_expr < 0)
    {		
		cl_int error = clEnqueueReadBuffer(context->cl_cq, *((cl_mem*)pDevice), CL_FALSE, 
									offset, size, pHost, 0, NULL, NULL);
		if(error  != CL_SUCCESS)
	    {
	        INFO(("acc_init error: %s", acc_get_error_string(error)));
	    }
	}
	else
    {
        
    }
}

void __accr_init_param_list()
{
	param_list = vector_create(sizeof(param_t));
}

/* 
 * Add a parameter to a kernel's parameter space 
 * This parameter is an array
 */
void __accr_push_kernel_param_pointer(void** pParam, int inum, char* strVarname)
{
	cl_int cl_error = clSetKernelArg(current_cl_kernel_handle, inum, sizeof(cl_mem), *pParam);

	if(cl_error  != CL_SUCCESS)
    {
        INFO(("acc_init error: %s", acc_get_error_string(error)));
    }
}

/* 
 * Add a parameter to a kernel's parameter space 
 * This parameter is a scalar
 */
void __accr_push_kernel_param_scalar(void* pValue, int inum, unsigned int isize, char* strVarname)
{
	cl_int cl_error = clSetKernelArg(current_cl_kernel_handle, inum, isize, (void *)pValue);
	if(cl_error  != CL_SUCCESS)
    {
        INFO(("acc_init error: %s", acc_get_error_string(error)));
    }
}

/* given a host address, find the device address in the hash map */
int __accr_get_device_addr(void* pHostAddr, void** pDeviceAddr, unsigned int istart, 
						unsigned int isize,   unsigned int type_size, char* strVarname)
{
	int key, map_length;
    int index;
    acc_hm_entry* e;
    param_t *param;
    
	/*if the map has not created yet*/
    if(map == NULL)
	{
		WARN(("WARN: The data map has not created yet"));
    	return 1;
	}

    map_length = map->capacity;

    /* the program passes the whole array */
    param = (param_t*)acc_hashmap_get(map, pHostAddr);
    if((param != NULL) && (param->host_addr == pHostAddr))
    {
        *pDeviceAddr = param->device_addr;
        return 1;
    }
	else
    {
        /* the program passes the partial array, we cannot find the entry
         * directly from the host address since the passed address is not
         * the start of the host address (usually adds an offset), so we
         * need to iterate over all entries of the hashmap
         **/
        for(index=0; index<map_length; index++)
        {
            for(e = map->table[index]; e != NULL; e=e->next)
            {
                param = e->value;
                if((param != NULL) && ((pHostAddr) >= param->host_addr)
		            && (((pHostAddr+istart+isize) <= param->host_addr + param->size)))
	            {
    	            //*pDeviceAddr = param->device_addr + (pHostAddr - param->host_addr);
    	            //we find it
    	            cl_int errNum;
					param_t *newparam;
					cl_mem* cl_ptr = (cl_mem*)malloc(sizeof(cl_mem));
    	            cl_buffer_region region = {pHostAddr - param->host_addr, isize};
    	            *cl_ptr = clCreateSubBuffer(
				            *((cl_mem*)param->device_addr),
				            CL_MEM_READ_WRITE,
				            CL_BUFFER_CREATE_TYPE_REGION,
				            &region,
				            &errNum);
					*pDeviceAddr = cl_ptr;
					
					if(bIsZeroCopyEnabled == TRUE)
					{						
	    	            *cl_ptr = clCreateSubBuffer(
					            *((cl_mem*)param->device_addr),
					            CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,
					            CL_BUFFER_CREATE_TYPE_REGION,
					            &region,
					            &errNum);
					}
					else
					{						
	    	            *cl_ptr = clCreateSubBuffer(
					            *((cl_mem*)param->device_addr),
					            CL_MEM_READ_WRITE,
					            CL_BUFFER_CREATE_TYPE_REGION,
					            &region,
					            &errNum);
					}

					
					*pDevice = cl_ptr;
					/*add the data in the hashmap*/
					newparam = (param_t*)malloc(sizeof(param_t));
					newparam->host_addr = pHostAddr;
					newparam->device_addr = cl_ptr; //cl_mem*
					newparam->size = isize;
				    newparam->pinned = 0;
					
					acc_hashmap_put(map, newparam->host_addr, newparam);

					
					__accr_stack_pending_to_current_stack(cl_ptr);
                    return 1;
	            }
            }
        }
    }

    ERROR(("ERROR: The device address for host address %p is not found", pHostAddr));
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
	int key, map_length;
	int bIsExist;
	param_t *param;
	/*if the map has not created yet*/
	if(map == NULL)
	{
		WARN(("WARN: The data map has not created yet"));
		return 0;
	}

	bIsExist = __accr_get_device_addr(pBuffer, pDeviceAddr, start, 
						length,   type_size, strVarname);
	
	return bIsExist;
}

/*
 * given a device address
 * determine whether it is already in the hashmap
 */
int __accr_device_addr_present(void* pDevice, char* strVarname)
{
	int index, map_length;
	param_t *param;

	/*if the map has not created yet*/
	if(map == NULL)
	{
		ERROR(("ERROR: The data map has not created yet"));
		return 0;
	}
	
	map_length = map->capacity;
	for(index=0; index<map_length; index++)
	{
        param = (param_t*)acc_hashmap_get_from_index(map, index, pDevice);
        if(param != NULL)
            return 1;
	}

	ERROR(("ERROR: The given address %p is not present on the device", pDevice));
	return 0;
}

/*
 * given a device address, remove it from the hashmap
 *
 */
static int __accr_remove_device_from_hashmap(void* pDevice, char* strVarname)
{
	int index, map_length;
	param_t *param;
    
	/*if the map has not created yet*/
	if(map == NULL)
	{
		ERROR(("ERROR: The data map has not created yet"));
		return 0;
	}
	
	map_length = map->capacity;
	for(index=0; index<map_length; index++)
	{
        param = (param_t*)acc_hashmap_get_from_index(map, index, pDevice);
		/*
        if((param != NULL) && param->device_addr == pDevice)
        {
            DEBUG(("param device addr: %p, pDevice: %p", param->device_addr, pDevice));
            acc_hashmap_remove(map, param->host_addr);
            return 1;
        } 
		*/
		if(param != NULL)
		{
            DEBUG(("param device addr: %p, pDevice: %p", param->device_addr, pDevice));
           
            acc_hashmap_remove(map, param->host_addr);
			return 1;
		}
	}

	ERROR(("ERROR: The given address %p is not present on the device", pDevice));
    return 0;
}

void __accr_reduction_buff_malloc(void** pDevice, int type_size)
{
	param_t *newparam;
	cl_mem* cl_ptr = (cl_mem*)malloc(sizeof(cl_mem));
	*cl_ptr = acc_malloc(type_size);
	*pDevice = cl_ptr;
}

void __accr_free_reduction_buff(void* pDevice)
{	
	clReleaseMemObject((cl_mem)(*((cl_mem*)pDevice)));
	free(pDevice);
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
	//int stream_pos;
	void *pDevice;
	__accr_get_device_addr(pHost, &pDevice, offset, size, type_size, strVarname);

	__accr_memin_h2d(pHost, pDevice, size, offset, async_expr, strVarname);
	DEBUG(("Updating %u bytes data from host to device", size));
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
	//int stream_pos;
	void *pDevice;
	__accr_get_device_addr(pHost, &pDevice, offset, size, type_size, strVarname);
	__accr_memout_d2h(pDevice, pHost, size, offset, async_expr, strVarname);
	DEBUG(("Updating %u bytes data from device to host", size));
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
	void *pDevice;
	/*the position in the stream array*/
	/*int stream_pos;
	__accr_get_device_addr(pHost, &pDevice, offset, size, type_size, strVarname);

	CUDART_CHECK( cudaHostRegister(pHost + offset, size, cudaHostRegisterMapped) );

	stream_pos = scalar_expr % MODULE_BASE;
	if(async_streams[stream_pos] == NULL)
	{
		CUDA_CHECK( cuStreamCreate(&async_streams[stream_pos], 0) );
	}	
		
	CUDART_CHECK( cudaMemcpyAsync(pDevice + offset, 
								     pHost + offset, 
								     size, 
								 	 cudaMemcpyHostToDevice, 
								 	 async_streams[stream_pos]) );*/
	DEBUG(("Updating %u bytes data from host to device", size));
	
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
	void *pDevice;
	/*the position in the stream array*/
	/*int stream_pos;
	__accr_get_device_addr(pHost, &pDevice, offset, size, type_size, strVarname);

	CUDART_CHECK( cudaHostRegister(pHost + offset, size, cudaHostRegisterMapped) );

	stream_pos = scalar_expr % MODULE_BASE;
	if(async_streams[stream_pos] == NULL)
	{
		CUDA_CHECK( cuStreamCreate(&async_streams[stream_pos], 0) );	
	}
	 
	CUDART_CHECK( cudaMemcpyAsync(pHost + offset, 
	     						 	 pDevice + offset, 
								 	 size, 
									 cudaMemcpyDeviceToHost, 
									 async_streams[stream_pos]) );*/
	
	DEBUG(("Updating %u bytes data from device to host", size));
	
}

void __accr_wait_stream(int async_expr)
{
	/*int stream_pos;
	stream_pos = async_expr % MODULE_BASE;
	if(async_streams[stream_pos] != NULL)
	{
		CUDA_CHECK( cuStreamSynchronize(async_streams[stream_pos]) );
		
		DEBUG(("Waiting for stream %d", stream_pos));
	}*/
}

void __accr_wait_all_streams(void)
{
	/*int pos;
	for(pos=0; pos<MODULE_BASE+1; pos++)
	{
		if(async_streams[pos] != NULL)
		{
			CUDA_CHECK( cuStreamSynchronize(async_streams[pos]) );
		}
	}*/
		
	DEBUG(("Waiting for all streams"));
}

void __accr_wait_some_or_all_stream(int async_expr)
{
	/*if(async_expr <= 0)
		__accr_wait_all_streams();
	else
		__accr_wait_stream(async_expr);*/
}

void __accr_destroy_all_streams(void)
{
	/*int pos;
	for(pos=0; pos<MODULE_BASE+1; pos++)
	{
		if(async_streams[pos] != NULL)
		{
			CUDA_CHECK( cuStreamDestroy(async_streams[pos]) );
		}
	}
	
	DEBUG(("Destroyed all streams"));*/
}

int acc_async_test_(int* scalar_expr);

int acc_async_test_(int* scalar_expr)
{
    //acc_async_test(*scalar_expr);
}
//#pragma weak acc_async_test_ = acc_async_test

int acc_async_test(int scalar_expr)
{
	return 0;
}

int acc_async_test_all_(void);
#pragma weak acc_async_test_all_ = acc_async_test_all

int acc_async_test_all(void)
{
	return 0;
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
