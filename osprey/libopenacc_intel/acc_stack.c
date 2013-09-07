#include "acc_common.h"
#include "acc_stack.h"
#include "acc_data.h"

static __acc_stack_impl* __pacc_stack = NULL;

void __accr_stack_init()
{
	__pacc_stack = malloc(sizeof(__acc_stack_impl));
	__pacc_stack->pnext = NULL;
	__pacc_stack->pheader = NULL;
}

void __accr_stack_destroy()
{
	if(__pacc_stack)
		free(__pacc_stack);
	__pacc_stack = NULL;
}

void __accr_stack_push()
{
	__acc_stack_impl* newstack = malloc(sizeof(__acc_stack_impl));
	newstack->pnext = __pacc_stack;
	newstack->pheader = NULL;
 	__pacc_stack = newstack;	
}


void __accr_stack_pop()
{
	__acc_stack_impl* oldtop = __pacc_stack;
	__pacc_stack = __pacc_stack->pnext;
	free(oldtop);
}

void __accr_stack_pending_to_current_stack(void* pdevice, int bisReductionBuffer)
{
	__acc_device_pointer* newdeviceptr = malloc(sizeof(__acc_device_pointer));
	newdeviceptr->pdevice = pdevice;
	newdeviceptr->bisreductionbuffer = bisReductionBuffer;
	newdeviceptr->pnext = __pacc_stack->pheader;
	__pacc_stack->pheader = newdeviceptr;
}

void __accr_stack_clear_device_ptr_in_current_stack()
{
	__acc_device_pointer* pheader = __pacc_stack->pheader;
	while(pheader!=NULL)
	{
		if(pheader->bisreductionbuffer)
			__accr_free_reduction_buff(pheader->pdevice);
		else
			__accr_free_on_device(pheader->pdevice);
		pheader = pheader->pnext;
	}
}
