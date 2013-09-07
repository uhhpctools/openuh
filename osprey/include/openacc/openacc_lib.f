 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  !This file is created by Xiaonan(Daniel) Tian from HPCTools, University of Houston
  !(daniel.xntian@gmail.com) for OpenUH OpenACC compiler.
  !It is intended to lower the OpenACC pragma.
  !It is free to use. However, please keep the original author.
  !http://www2.cs.uh.edu/~xntian2/
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
       
        module acc_lib_kinds
		integer, parameter :: acc_device_kind   = 4
        	integer, parameter :: acc_handle_kind   = 4
        	integer, parameter :: acc_logical_kind    = 4
        end module acc_lib_kinds
        
	module openacc     
	use acc_lib_kinds
        !integer, parameter :: acc_device_kind   = 4
        !integer, parameter :: acc_handle_kind   = 4
        !integer, parameter :: acc_logical_kind    = 4
        
        integer(kind=acc_device_kind), parameter :: acc_device_none     &
     >                                                    = 0
        integer(kind=acc_device_kind), parameter :: acc_device_host     &
     >                                                   = 1
        integer(kind=acc_device_kind), parameter :: acc_device_not_host &
     >                                                     = 2
        integer(kind=acc_device_kind), parameter :: acc_device_default  &
     >                                                    = 3
        integer(kind=acc_device_kind), parameter :: acc_device_nvidia   &
     >                                                   = 4
        integer(kind=acc_device_kind), parameter :: acc_device_radeon   &
     >                                                   = 5
        integer(kind=acc_device_kind), parameter :: acc_device_xeonphi  &
     >                                                    = 6

	integer, parameter :: openacc_version = 201311

	interface
            function acc_get_num_devices( devicetype )
              use acc_lib_kinds
            	integer (kind=acc_device_kind):: devicetype
            	integer :: acc_get_num_devices
            end function acc_get_num_devices
       end interface

	
	interface
            subroutine acc_set_device_type( devicetype )
              use acc_lib_kinds
            	integer (kind=acc_device_kind):: devicetype
            end subroutine acc_set_device_type
       end interface
	
	interface
            function acc_get_device_type()
              use acc_lib_kinds
            	integer (kind=acc_device_kind):: acc_get_device_type
            end function acc_get_device_type
       end interface
	
	interface
            subroutine acc_set_device_num( devicenum, devicetype )
              use acc_lib_kinds
            	integer :: devicenum
            	integer (kind=acc_device_kind) :: devicetype
            end subroutine acc_set_device_num
       end interface
	
	interface
            function acc_get_device_num( devicetype )
              use acc_lib_kinds
            	integer (kind=acc_device_kind) :: devicetype
              integer :: acc_get_device_num
            end function acc_get_device_num
       end interface
	
	interface
            function acc_async_test( expression )
              use acc_lib_kinds
            	integer (kind=acc_handle_kind) :: expression
              logical (kind=acc_logical_kind) :: acc_async_test
            end function acc_async_test
       end interface
	
	interface
            function acc_async_test_all()
              use acc_lib_kinds
              logical (kind=acc_logical_kind) :: acc_async_test_all
            end function acc_async_test_all
       end interface
	
	interface
            subroutine acc_async_wait( expression )
              use acc_lib_kinds
            	integer (kind=acc_handle_kind) :: expression
            end subroutine acc_async_wait
       end interface
	
	interface
            subroutine acc_async_wait_all()
            end subroutine acc_async_wait_all
       end interface
	
	interface
            subroutine acc_init( devicetype )
              use acc_lib_kinds
            	integer (kind=acc_device_kind) :: devicetype
            end subroutine acc_init
       end interface
	
	interface
            subroutine acc_shutdown( devicetype )
              use acc_lib_kinds
            	integer (kind=acc_device_kind) :: devicetype
            end subroutine acc_shutdown
       end interface
	
	interface
            function acc_on_device( devicetype )
              use acc_lib_kinds
            	integer (kind=acc_device_kind) :: devicetype
              logical (kind=acc_logical_kind) :: acc_on_device
            end function acc_on_device
       end interface	     
       
       end module openacc
