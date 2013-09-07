/**
 * Author: Rengan Xu
 * University of Houston
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CL/cl.h>

#include "acc_util.h"
#include "acc_log.h"
#include "vector.h"
#include "acc_hashmap.h"

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))


typedef enum
{
  acc_device_none     = 0, //< no device
  acc_device_host     = 1, //< host device
  acc_device_not_host = 2, //< not host device
  acc_device_default  = 3, //< default device type
  acc_device_nvidia  = 4,
  acc_device_radeon  = 5,
  acc_device_xeonphi  = 6,
  //acc_device_cuda     = 4, //< CUDA device
  //acc_device_opencl   = 5  //< OpenCL device 
} acc_device_t;


typedef enum
{
	ACC_KDATA_UNKNOWN = 0,
	//MTYPE_I1 = 2        /* 8-bit integer */
	ACC_KDATA_UINT8,      
	//MTYPE_I2 = 3        /* 16-bit integer */
	ACC_KDATA_UINT16,
	//MTYPE_I4 = 4        /* 32-bit integer */
	ACC_KDATA_UINT32,
	//MTYPE_I8 = 5        /* 64-bit integer */
	ACC_KDATA_UINT64,
	//MTYPE_U1 = 6        /* 8-bit unsigned integer */
	ACC_KDATA_INT8,
	//MTYPE_U2 = 7        /* 16-bit unsigned integer */
	ACC_KDATA_INT16,
	//MTYPE_U4 = 8        /* 32-bit unsigned interger */
	ACC_KDATA_INT32,
	//MTYPE_U8 = 9        /* 64-bit unsigned integer */
	ACC_KDATA_INT64,
	//MTYPE_F4 = 10       /* 32-bit IEEE floating point */
	ACC_KDATA_FLOAT,
	//MTYPE_F8 = 11       /* 64-bit IEEE floating point */
	ACC_KDATA_DOUBLE
} ACC_KERNEL_DATA_TYPE;


typedef enum
{
	IN = 0,    /* cop in */
	OUT,       /* copy out */
	INOUT,     /* copy in and copy out */
	NONE       /* neither copy in or copy out*/
} TRANSFER_TYPE;

#define ACC_OK 0

#define acc_host_alloc(size) __acc_malloc_handler((size))
#define acc_host_alloc_zero(size) memset(acc_host_alloc(size), 0, (size))
#define acc_host_free(p) __acc_free_handler(p)


#endif
