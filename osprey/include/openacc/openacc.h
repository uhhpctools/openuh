#ifndef OPENACC_OPENUH_RT_H
#define OPENACC_OPENUH_RT_H

typedef enum
{
  acc_device_none     = 0, //< no device
  acc_device_host     = 1, //< host device
  acc_device_not_host = 2, //< not host device
  acc_device_default  = 3, //< default device type
  acc_device_nvidia  = 4,
  acc_device_radeon  = 5,
  acc_device_xeonphi  = 6,
  acc_device_cpu  = 7
  //acc_device_cuda     = 4, //< CUDA device
  //acc_device_opencl   = 5  //< OpenCL device 
} acc_device_t;


extern void _w2c_mstore(void* src, int src_offset, void* dst, int dst_offset, int ilength);

extern int acc_async_test(int);

extern int acc_async_test_all(void);

extern void acc_async_wait(int);

extern void acc_async_wait_all(void);

extern void acc_init(acc_device_t);

extern void acc_shutdown(acc_device_t);

extern void* acc_malloc(unsigned int);

extern void acc_free(void*);

#endif
