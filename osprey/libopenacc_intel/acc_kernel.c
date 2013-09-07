/**
 * Author: Rengan Xu
 * University of Houston
 */

#include "acc_kernel.h"

size_t gangs[3];
size_t vectors[3];
/*global kernel file name*/
//char cu_filename[512];
unsigned int shared_size = 0;

acc_hashmap* __accr_file_map = NULL;

cl_kernel current_cl_kernel_handle;

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
 *this function is called before any parameter push
 */
void __accr_prelaunchkernel(char* szKernelName, char* szKernelLib, int async_expr)
{
	//CL kernel file
	cl_program program;
	cl_int cl_error_code;
	cl_kernel cl_kernel_handle;
	//////////////////////////////////////////////////////////////////////////////
	int ret_code;
	unsigned int length;
	char* psource = NULL;
	FILE * fp;
	//check if the source code is built
    if(__accr_file_map == NULL)
        __accr_file_map = acc_hashmap_create();

       
    program =  (char*)acc_hashmap_get_string(__accr_file_map, szKernelLib);
	//if the cl code is not compiled yet, compile it and put in the hash table
    if(program == NULL)
    {
		/* 
		* this file is the first time to appear, add it in the hashmap
		* key is the file name, value is the module in this file
		*/
		//get the source code
		fp = fopen(szKernelLib, "rb");
		fseek(fp, 0L, SEEK_END);
		//get the length
		length = ftell(fp);
		psource = malloc((length+1) * sizeof(char));
		fseek(fp, 0L, SEEK_SET);
		ret_code = fread(psource, sizeof(char), length, fp);
		fclose(fp);

		//dynamical compile the code
		program = clCreateProgramWithSource(context->opencl_context, 1, (const char**)&psource, NULL, &cl_error_code);
		cl_error_code = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
		
        acc_hashmap_put_string(__accr_file_map, szKernelLib, program);
    }
	
	
	cl_kernel_handle = clCreateKernel(program, szKernelName, &cl_error_code);
	
	current_cl_kernel_handle = cl_kernel_handle;	
}

static int cl_work_dim = 1;
static unsigned int bIsAuto_cl_local_work_partition = 0;
 
void __accr_launchkernel(char* szKernelName, char* szKernelLib, int async_expr)
{
	//CL kernel file
	cl_int cl_error_code;
	staic size_t global_work_items[3];

	global_work_items[0] = gangs[0] * vectors[0];
	global_work_items[1] = gangs[1] * vectors[1];
	global_work_items[2] = gangs[2] * vectors[2];
		
	if(bIsAuto_cl_local_work_partition)
		cl_error_code = clEnqueueNDRangeKernel(context->cl_cq, current_cl_kernel_handle, cl_work_dim,
										NULL, global_work_items, NULL, 0, NULL, NULL);
	else		
		cl_error_code = clEnqueueNDRangeKernel(context->cl_cq, current_cl_kernel_handle, cl_work_dim, 
										NULL, global_work_items, vectors, 0, NULL, NULL);
	if(async_expr < 0)
	{
		cl_error_code = clFlush(context->cl_cq);
	  	cl_error_code = clFinish(context->cl_cq);
	}
	else //async
	{
	}
}

