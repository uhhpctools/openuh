#ifndef ACC_STACK_IMPLEMENTATION
#define ACC_STACK_IMPLEMENTATION
typedef struct __acc_device_pointer
{
	struct __acc_device_pointer *pnext;
	int bisreductionbuffer;
	void* pdevice;
}__acc_device_pointer;

typedef struct __acc_stack_impl
{
	struct __acc_stack_impl *pnext;
	__acc_device_pointer* pheader;
}__acc_stack_impl;

void __accr_stack_push();
void __accr_stack_pending_to_current_stack();
void __accr_stack_pop();
void __accr_stack_init();
void __accr_stream_clear_device_ptr(int async);
void __accr_stack_pending_to_current_stack(void* pdevice, int bisReductionBuffer, int async);



#endif
