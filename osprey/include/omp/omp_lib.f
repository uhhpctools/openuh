!  Copyright (C) 2005. PathScale Inc. All rights reserved.
!                                                                               
!  Copied from the OpenMP Application Program Interface Version 2.5 Public
!  Draft, November 2004.
!                                                                               
!  Copyright (C) 1997-2004 OpenMP Architecture Review Board.
!  Permission to copy without fee all or part of this material is granted,
!  provided the OpenMP Architecture Review Board copyright notice and the
!  titled of this document appear. Notice is given that copying is by
!  permission of OpenMP Architecture Review Board.

        module omp_lib_kinds
        integer, parameter :: omp_integer_kind   = 4
        integer, parameter :: omp_logical_kind   = 4
        integer, parameter :: omp_lock_kind      = 8
        integer, parameter :: omp_nest_lock_kind = 8
        end module omp_lib_kinds

        module omp_lib
          use omp_lib_kinds
          integer, parameter :: openmp_version = 199910

          interface
            subroutine omp_destroy_lock (var)
              use omp_lib_kinds
              integer (kind=omp_lock_kind), intent(inout) :: var
            end subroutine omp_destroy_lock
          end interface

          interface
            subroutine omp_destroy_nest_lock (var)
              use omp_lib_kinds
              integer (kind=omp_nest_lock_kind), intent(inout) :: var
            end subroutine omp_destroy_nest_lock
          end interface

          interface
            function omp_get_dynamic ()
              use omp_lib_kinds
              logical (kind=omp_logical_kind) :: omp_get_dynamic
            end function omp_get_dynamic
          end interface

          interface
            function omp_get_max_threads ()
              use omp_lib_kinds
              integer (kind=omp_integer_kind) :: omp_get_max_threads
            end function omp_get_max_threads
          end interface

          interface
            function omp_get_nested ()
              use omp_lib_kinds
              logical (kind=omp_logical_kind) :: omp_get_nested
            end function omp_get_nested
          end interface

          interface
            function omp_get_num_procs ()
              use omp_lib_kinds
              integer (kind=omp_integer_kind) :: omp_get_num_procs
            end function omp_get_num_procs
          end interface

          interface
            function omp_get_num_threads ()
              use omp_lib_kinds
              integer (kind=omp_integer_kind) :: omp_get_num_threads
            end function omp_get_num_threads
          end interface

          interface
            function omp_get_thread_num ()
              use omp_lib_kinds
              integer (kind=omp_integer_kind) :: omp_get_thread_num
            end function omp_get_thread_num
          end interface

          interface
            function omp_get_wtick ()
              use omp_lib_kinds
              double precision :: omp_get_wtick
            end function omp_get_wtick
          end interface

          interface
            function omp_get_wtime ()
              use omp_lib_kinds
              double precision :: omp_get_wtime
            end function omp_get_wtime
          end interface

          interface
            subroutine omp_init_lock (var)
              use omp_lib_kinds
              integer (kind=omp_lock_kind), intent(out) :: var
            end subroutine omp_init_lock
          end interface

          interface
            subroutine omp_init_nest_lock (var)
              use omp_lib_kinds
              integer (kind=omp_nest_lock_kind), intent(out) :: var
            end subroutine omp_init_nest_lock
          end interface

          interface
            function omp_in_parallel ()
              use omp_lib_kinds
              logical (kind=omp_logical_kind) :: omp_in_parallel
            end function omp_in_parallel
          end interface

          interface
            subroutine omp_set_dynamic (enable)
              use omp_lib_kinds
              logical (kind=omp_logical_kind), intent(in) :: enable
            end subroutine omp_set_dynamic
          end interface

          interface
            subroutine omp_set_lock (var)
              use omp_lib_kinds
              integer (kind=omp_lock_kind), intent(inout) :: var
            end subroutine omp_set_lock
          end interface

          interface
            subroutine omp_set_nest_lock (var)
              use omp_lib_kinds
              integer (kind=omp_nest_lock_kind), intent(inout) :: var
            end subroutine omp_set_nest_lock
          end interface

          interface
            subroutine omp_set_nested (enable)
              use omp_lib_kinds
              logical (kind=omp_logical_kind), intent(in) :: enable
            end subroutine omp_set_nested
          end interface

          interface
            subroutine omp_set_num_threads (nthreads)
              use omp_lib_kinds
              integer (kind=omp_integer_kind), intent(in) :: nthreads
            end subroutine omp_set_num_threads
          end interface

          interface
            function omp_test_lock (var)
              use omp_lib_kinds
              logical (kind=omp_logical_kind) :: omp_test_lock
              integer (kind=omp_lock_kind), intent(inout) :: var
            end function omp_test_lock
          end interface

          interface
            function omp_test_nest_lock (var)
              use omp_lib_kinds
              integer (kind=omp_integer_kind) :: omp_test_nest_lock
              integer (kind=omp_nest_lock_kind), intent(inout) :: var
            end function omp_test_nest_lock
          end interface

          interface
            subroutine omp_unset_lock (var)
              use omp_lib_kinds
              integer (kind=omp_lock_kind), intent(inout) :: var
            end subroutine omp_unset_lock
          end interface

          interface
            subroutine omp_unset_nest_lock (var)
              use omp_lib_kinds
              integer (kind=omp_nest_lock_kind), intent(inout) :: var
            end subroutine omp_unset_nest_lock
          end interface
        end module omp_lib
