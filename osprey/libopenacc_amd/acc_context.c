/**
 * Author: Rengan Xu
 * Revised: Xiaonan Tian for AMD APU
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
	__accr_stack_init();
}

int acc_get_num_devices_(acc_device_t* device_type);

int acc_get_num_devices_(acc_device_t* device_type)
{
    return 0;
}
//#pragma weak acc_get_num_devices_ = acc_get_num_devices

int acc_get_num_devices(acc_device_t device_type)
{
    return 0;
}

static void device_type_check(acc_device_t device_type)
{
}

void acc_set_device_num_(int* device_id, acc_device_t* device_type);

void acc_set_device_num_(int* device_id, acc_device_t* device_type)
{
}
//#pragma weak acc_set_device_num_ = acc_set_device_num

void acc_set_device_num(int device_id, acc_device_t device_type)
{
}

int acc_get_device_num_(acc_device_t* device_type);

int acc_get_device_num_(acc_device_t* device_type)
{
    return 0;
}
//#pragma weak acc_get_device_num_ = acc_get_device_num

int acc_get_device_num(acc_device_t device_type)
{
    return 0; 
}

void acc_set_device_type_(acc_device_t* device_type);

void acc_set_device_type_(acc_device_t* device_type)
{
}
//#pragma weak acc_set_device_type_ = acc_set_device_type

void acc_set_device_type(acc_device_t device_type)
{
}

acc_device_t acc_get_device_type_(void);
#pragma weak acc_get_device_type_ = acc_get_device_type

acc_device_t acc_get_device_type(void)
{
    return 0;
}

void __accr_cleanup(void)
{
}

void acc_shutdown_(acc_device_t* device_type);

void acc_shutdown_(acc_device_t* device_type)
{
}

//#pragma weak acc_shutdown_ = acc_shutdown
void acc_shutdown(acc_device_t device_type)
{
}
