 /***************************************************************************
  This file is created by Xiaonan(Daniel) Tian from HPCTools, University of Houston
  (daniel.xntian@gmail.com) for OpenUH OpenACC compiler.
  It is intended to lower the OpenACC pragma.
  It is free to use. However, please keep the original author.
  http://www2.cs.uh.edu/~xntian2/
*/

!C the "C" of this comment starts in column 1
	integer acc_device_kind
	parameter ( acc_device_kind = 4 )

	integer acc_handle_kind
	parameter ( acc_handle_kind = 4 )

	integer  acc_device_none
	parameter (acc_device_none = 0)
	integer  acc_device_host
	parameter  (acc_device_host = 1)
	integer  acc_device_not_host
	parameter  (acc_device_not_host = 2)
	integer  acc_device_default
	parameter  (acc_device_default = 3)
	integer  acc_device_nvidia
	parameter  (acc_device_nvidia = 4)
	integer  acc_device_radeon
	parameter  (acc_device_radeon = 5)
	integer  acc_device_xeonphi
	parameter  (acc_device_xeonphi = 6)
	integer  acc_device_cpu
	parameter  (acc_device_cpu = 7)

!C default integer type assumed below
!C default logical type assumed below
!C OpenACC Fortran API v2.0
	integer openacc_version
	parameter ( openacc_version = 201311 )

	external acc_set_device_type
	external acc_set_device_num
	external acc_async_wait
	external acc_async_wait_all
	external acc_init
	external acc_shutdown


	external acc_get_num_devices
	integer acc_get_num_devices
	
	external acc_get_device_type
	integer acc_get_device_type
	
	external acc_get_device_num
	integer acc_get_device_num
	
	external acc_async_test
	logical acc_async_test
	
	external acc_async_test_all
	logical acc_async_test_all
	
	external acc_on_device
	logical acc_on_device
	

