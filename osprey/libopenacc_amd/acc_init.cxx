/* 
 * This file is going to init OpenACC execution model environment 
 * Copyright@Daniel Tian, HPCTools Group, University of Houston
 */

typedef enum
{
  acc_device_none     = 0, //< no device
  acc_device_default  = 1, //< default device type
  acc_device_nvidia  = 2,
  acc_device_radeon  = 3,
  acc_device_xeonphi  = 4,
  acc_device_cpu  = 5
  /*acc_device_host     = 2, //< host device
  acc_device_not_host = 3, //< not host device
  acc_device_cuda     = 4, //< CUDA device
  acc_device_opencl   = 5  //< OpenCL device */
} acc_device_t;


extern "C" void acc_init(acc_device_t device_type);

extern "C" void acc_shutdown(acc_device_t device_type);


/*
 * class __acc_rtl_initializer
 * initialize the libopenacc by a static instance.
 */
class __acc_rtl_initializer {
private:
    __acc_rtl_initializer() {
        acc_init(acc_device_default);
    }
    ~__acc_rtl_initializer() {
	acc_shutdown(acc_device_default);
    }
    static __acc_rtl_initializer inst_;
};
__acc_rtl_initializer __acc_rtl_initializer::inst_;

