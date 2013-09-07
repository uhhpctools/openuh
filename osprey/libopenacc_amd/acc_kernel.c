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

typedef struct transfer_t { int nargs ; size_t* rsrvd1; size_t* rsrvd2 ; size_t* rsrvd3 ; } transfer_t;
struct lparm_t { int ndim; size_t gdims[3]; size_t ldims[3]; transfer_t transfer ; } ;

typedef struct lparm_t Launch_params_t;

static Launch_params_t launch_params;

void __accr_init_launch_params(void** plaunch_params)
{
        launch_params.ndim = 3;
        launch_params.gdims[0] = gangs[0] * vectors[0];
        launch_params.gdims[1] = gangs[1] * vectors[1];
        launch_params.gdims[2] = gangs[2] * vectors[2];

        launch_params.ldims[0] = vectors[0];
        launch_params.ldims[1] = vectors[1];
        launch_params.ldims[2] = vectors[2];

        *plaunch_params = &launch_params;
}

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
}


/*
 *szKernelName: kernel function name
 *szKernelLib: kernel file name
 */
void __accr_launchkernel(char* szKernelName, char* szKernelLib, int async_expr)
{
}
