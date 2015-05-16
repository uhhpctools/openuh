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

#define SNK_ORDERED 1
#define SNK_UNORDERED 0

#include <stdint.h>
typedef struct hsa_signal_s { uint64_t handle; } hsa_signal_t;

typedef struct snk_task_s snk_task_t;
struct snk_task_s {
   hsa_signal_t signal ;
   snk_task_t* next;
};

typedef struct snk_lparm_s snk_lparm_t;
struct snk_lparm_s {
   int ndim;                  /* default = 1 */
   size_t gdims[3];           /* NUMBER OF THREADS TO EXECUTE MUST BE SPECIFIED */
   size_t ldims[3];           /* Default = {64} , e.g. 1 of 8 CU on Kaveri */
   int stream;                /* default = -1 , synchrnous */
   int barrier;               /* default = SNK_UNORDERED */
   int acquire_fence_scope;   /* default = 2 */
   int release_fence_scope;   /* default = 2 */
   snk_task_t *requires ;     /* Linked list of required parent tasks, default = NULL  */
   snk_task_t *needs ;        /* Linked list of parent tasks where only one must complete, default=NULL */
} ;

/* This string macro is used to declare launch parameters set default values  */
//#define SNK_INIT_LPARM(X,Y) snk_lparm_t * X ; snk_lparm_t  _ ## X ={.ndim=1,.gdims={Y},.ldims={64},.stream=-1,.barrier=SNK_UNORDERED,.acquire_fence_scope=2,.release_fence_scope=2,.requires=NULL,.needs=NULL} ; X = &_ ## X ;


static snk_lparm_t launch_params;

void __accr_init_launch_params(void** plaunch_params)
{
        launch_params.ndim = 3;
        launch_params.gdims[0] = gangs[0] * vectors[0];
        launch_params.gdims[1] = gangs[1] * vectors[1];
        launch_params.gdims[2] = gangs[2] * vectors[2];

        launch_params.ldims[0] = vectors[0];
        launch_params.ldims[1] = vectors[1];
        launch_params.ldims[2] = vectors[2];
	launch_params.stream=-1;
	launch_params.barrier=SNK_UNORDERED;
	launch_params.acquire_fence_scope=2;
	launch_params.release_fence_scope=2;
	launch_params.requires=NULL;
	launch_params.needs=NULL;

        *plaunch_params = &launch_params;
}

void __accr_init_launch_reduction_params(void** plaunch_params, unsigned int* pblock_size)
{
        launch_params.ndim = 3;
		gangs[0] = 1;
		gangs[1] = 1;
		gangs[2] = 1;
		vectors[0] = 256;
		vectors[1] = 1;
		vectors[2] = 1;
        launch_params.gdims[0] = gangs[0] * vectors[0];
        launch_params.gdims[1] = gangs[1] * vectors[1];
        launch_params.gdims[2] = gangs[2] * vectors[2];

        launch_params.ldims[0] = vectors[0];
        launch_params.ldims[1] = vectors[1];
        launch_params.ldims[2] = vectors[2];
	launch_params.stream=-1;
	launch_params.barrier=SNK_UNORDERED;
	launch_params.acquire_fence_scope=2;
	launch_params.release_fence_scope=2;
	launch_params.requires=NULL;
	launch_params.needs=NULL;

        *plaunch_params = &launch_params;
		*pblock_size = (unsigned int)vectors[0];
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
	if(x<=0)
		gangs[0] = 1;
	else
		gangs[0] = x;
}

void __accr_set_gang_num_y(int y)
{
	if(y<=0)
		gangs[1] = 1;
	else
		gangs[1] = y;
}

void __accr_set_gang_num_z(int z)
{
	if(z<=0)
		gangs[2] = 1;
	else
		gangs[2] = z;
}

void __accr_set_vector_num_x(int x)
{
	if(x<=0 || x>512)
	{
		if(vectors[1]<=0)
			vectors[0] = 32;
		else
			vectors[0] = 512/vectors[1];
	}
	else
		vectors[0] = x;
}

void __accr_set_vector_num_y(int y)
{
	if(y <= 0 || y>512)
		vectors[1] = 1;
	else
	{
		vectors[1] = y;
		if(vectors[0] * vectors[1] > 512)			
			vectors[0] = 512/vectors[1];
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
