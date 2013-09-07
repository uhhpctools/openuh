/**
 * Author: Rengan Xu
 * University of Houston
 */

#include "acc_kernel.h"

int gangs[3];
int vectors[3];
/*global kernel file name*/
//char cu_filename[512];
unsigned int shared_size = 0;

acc_hashmap* __accr_file_map = NULL;

/*global kernel module and function*/
//CUmodule cu_module;
//CUfunction cu_function;

void __accr_set_gangs(int x, int y, int z)
{
	gangs[0] = x;
	gangs[1] = y;
	gangs[2] = z;
}

void __accr_set_vectors(int x, int y, int z)
{
	vectors[0] = x;
    vectors[1] = y;
    vectors[2] = z;
}

void __accr_set_gang_num_x(int x)
{
	if(x==0)
		gangs[0] = 1;
	else
		gangs[0] = x;
}

void __accr_set_gang_num_y(int y)
{
	if(y==0)
		gangs[1] = 1;
	else
		gangs[1] = y;
}

void __accr_set_gang_num_z(int z)
{
	if(z==0)
		gangs[2] = 1;
	else
		gangs[2] = z;
}

void __accr_set_vector_num_x(int x)
{
	if(x<=0 || x>1024)
	{
		if(vectors[1]<=0)
			vectors[0] = 32;
		else
			vectors[0] = 1024/vectors[1];
	}
	else
		vectors[0] = x;
}

void __accr_set_vector_num_y(int y)
{
	if(y <= 0 || y>1024)
		vectors[1] = 1;
	else
	{
		vectors[1] = y;
		if(vectors[0] * vectors[1] > 1024)			
			vectors[0] = 1024/vectors[1];
	}
}

void __accr_set_vector_num_z(int z)
{
	if(z == 0)
		vectors[2] = 1;
	else
		vectors[2] = z;
}

void __accr_set_default_gang_vector(void)
{
	gangs[0] = 192;
	gangs[1] = 1;
	gangs[2] = 1;

	vectors[0] = 128;
	vectors[1] = 1;
	vectors[2] = 1;
	
	DEBUG(("Set gangs: %d, %d, %d", gangs[0], gangs[1], gangs[2]));
	DEBUG(("Set vectors: %d, %d, %d", vectors[0], vectors[1], vectors[2]));
}


/* used in parallel region */
int __accr_get_num_workers()
{
    return vectors[1];
}

/* used in parallel region */
int __accr_get_num_vectors()
{
    return vectors[0];
}

int __accr_get_total_num_gangs(void)
{
    return gangs[0]*gangs[1]*gangs[2];
}

int __accr_get_total_gangs_workers()
{
    return (__accr_get_total_num_gangs()*__accr_get_num_workers());
}

int __accr_get_total_num_vectors()
{
    return (__accr_get_total_num_gangs()*__accr_get_num_workers()*__accr_get_num_vectors());
}

void __accr_reset_default_gang_vector(void)
{
	gangs[0] = 0;
    gangs[1] = 0;
    gangs[2] = 0;

    vectors[0] = 0;
    vectors[1] = 0;
    vectors[2] = 0;
}

void __accr_set_shared_mem_size(unsigned int size)
{
    shared_size = size;
}

void __accr_set_default_shared_mem_size()
{
    shared_size = __acc_gpu_config->max_threads_per_block*sizeof(double); 
}


/*
 *szKernelName: kernel function name
 *szKernelLib: kernel file name
 */
void __accr_launchkernel(char* szKernelName, char* szKernelLib, int async_expr)
{
	void **args;
	FILE *fp;
	int file_size;
    CUmodule cu_module;
    CUfunction cu_function;
	CUresult ret;
	int i, args_count;
	param_t *param;


    if(__accr_file_map == NULL)
        __accr_file_map = acc_hashmap_create();

       
    cu_module =  (char*)acc_hashmap_get_string(__accr_file_map, szKernelLib);
    if(cu_module == NULL)
    {
        /* 
         * this file is the first time to appear, add it in the hashmap
         * key is the file name, value is the module in this file
         */
		char *ptx_source;
		fp = fopen(szKernelLib, "rb");
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		ptx_source = (char*)malloc((file_size+1)*sizeof(char));
		fseek(fp, 0, SEEK_SET);
		fread(ptx_source, sizeof(char), file_size, fp);
		fclose(fp);
		ptx_source[file_size] = '\0';
	
		//printf("Loading module %s\n", ptx_source);
		ret = cuModuleLoadData(&cu_module, ptx_source);
		CUDA_CHECK(ret);
		free(ptx_source);
        acc_hashmap_put_string(__accr_file_map, szKernelLib, cu_module);
    }
        
	ret = cuModuleGetFunction(&cu_function, cu_module, szKernelName);	
	CUDA_CHECK(ret);
    
#if 0
	/*
	 *if the kernel to be launched is different from the previous
	 *kernel, then update the previous kernel as current kernel
	 */
	if(strcmp(cu_filename, szKernelLib) != 0)
	{
		char *ptx_source;
		strncpy(cu_filename, szKernelLib, 512);
		
		fp = fopen(szKernelLib, "rb");
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		ptx_source = (char*)malloc((file_size+1)*sizeof(char));
		fseek(fp, 0, SEEK_SET);
		fread(ptx_source, sizeof(char), file_size, fp);
		fclose(fp);
		ptx_source[file_size] = '\0';
	
		//printf("Loading module %s\n", ptx_source);
		ret = cuModuleLoadData(&cu_module, ptx_source);
		CUDA_CHECK(ret);
		free(ptx_source);

		//printf("Kernel name: %s\n", szKernelName);	
		//printf("Kernel file name: %s\n", szKernelLib);	
		ret = cuModuleGetFunction(&cu_function, cu_module, szKernelName);	
		CUDA_CHECK(ret);
	}else
    {
        /*the file name are the same, but the kernel function may be different*/
		ret = cuModuleGetFunction(&cu_function, cu_module, szKernelName);	
		CUDA_CHECK(ret);
    }
#endif	
	args_count = vector_length(param_list);
	args = (void**)malloc(args_count*sizeof(void*));
	
	for(i = args_count-1; i >=0; i--)
	{
		param = (param_t*)malloc(sizeof(param_t));
		vector_popback(param_list, param);
		if(param->device_addr != NULL)
		{
			args[i] = &(param->device_addr);
			DEBUG(("args[%d] device address: %p", i, param->device_addr));
		}
		else
		{
			args[i] = param->host_addr;
			DEBUG(("args[%d] host address: %p", i, param->host_addr));
		}

	}

	DEBUG(("Arguments added successfully"));
	DEBUG(("gang_x: %d, gang_y: %d, gang_z: %d", gangs[0], gangs[1], gangs[2]));
	DEBUG(("vector_x: %d, vector_y: %d, vector_z: %d", vectors[0], vectors[1], vectors[2]));

	if(gangs[0] > __acc_gpu_config->max_grid_dim[0] ||
	   gangs[1] > __acc_gpu_config->max_grid_dim[1] ||
	   gangs[2] > __acc_gpu_config->max_grid_dim[2] )
	   ERROR(("Gang number exceeds the limit"));

	if(vectors[0] > __acc_gpu_config->max_block_dim[0] ||
	   vectors[1] > __acc_gpu_config->max_block_dim[1] ||
	   vectors[2] > __acc_gpu_config->max_block_dim[2] ||
	   vectors[0]*vectors[1]*vectors[2] > __acc_gpu_config->max_threads_per_block)
	   ERROR(("Vector number exceeds the limit"));
	

	/* the asynchronous scalar expression do not accept negative value now*/
	if(async_expr < 0)
	{
		ret = cuLaunchKernel(cu_function, gangs[0], gangs[1], gangs[2], 
						     vectors[0], vectors[1], vectors[2], 
						     shared_size, 
						     NULL, args, NULL);
		CUDA_CHECK(ret);
	}else if(async_expr == 0)
	{
		if(async_streams[MODULE_BASE] == NULL)
		{
			CUDA_CHECK( cuStreamCreate(&async_streams[MODULE_BASE], 0) );	
		}
		
		ret = cuLaunchKernel(cu_function, gangs[0], gangs[1], gangs[2], 
						     vectors[0], vectors[1], vectors[2], 
						     shared_size, 
						     async_streams[MODULE_BASE], args, NULL);
		CUDA_CHECK(ret);
	}else
	{
		int stream_pos;
		stream_pos = async_expr % MODULE_BASE;
		
		if(async_streams[stream_pos] == NULL)
		{
			CUDA_CHECK( cuStreamCreate(&async_streams[stream_pos], 0) );	
		}
		
		ret = cuLaunchKernel(cu_function, gangs[0], gangs[1], gangs[2], 
						     vectors[0], vectors[1], vectors[2], 
						     shared_size, 
						     async_streams[stream_pos], args, NULL);
		CUDA_CHECK(ret);
	}

	DEBUG(("The kernel is launched successfully"));
	/* free the memory of kernel parameter */
	free(args);

}
