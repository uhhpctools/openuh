!  Copyright (C) 2006. QLogic Corporation. All Rights Reserved.
!
!  Copyright (C) 2005, 2006 PathScale, Inc. All rights reserved.
!                                                                               
!  Copied from the OpenMP Application Program Interface Version 2.5 Public
!  Draft, November 2004.
!                                                                               
!  Copyright (C) 1997-2004 OpenMP Architecture Review Board.
!  Permission to copy without fee all or part of this material is granted,
!  provided the OpenMP Architecture Review Board copyright notice and the
!  titled of this document appear. Notice is given that copying is by
!  permission of OpenMP Architecture Review Board.

C the "C" of this comment starts in column 1
	integer omp_lock_kind
	parameter ( omp_lock_kind = 8 )

	integer omp_nest_lock_kind
	parameter ( omp_nest_lock_kind = 8 )

C default integer type assumed below
C default logical type assumed below
C OpenMP Fortran API v1.1
	integer openmp_version
	parameter ( openmp_version = 200011 )

	external omp_destroy_lock
	external omp_destroy_nest_lock

	external omp_get_dynamic
	logical omp_get_dynamic

	external omp_get_max_threads
	integer omp_get_max_threads

	external omp_get_nested
	logical omp_get_nested

	external omp_get_num_procs
	integer omp_get_num_procs

	external omp_get_num_threads
	integer omp_get_num_threads

	external omp_get_thread_num
	integer omp_get_thread_num

	external omp_get_wtick
	double precision omp_get_wtick

	external omp_get_wtime
	double precision omp_get_wtime

	external omp_init_lock
	external omp_init_nest_lock

	external omp_in_parallel
	logical omp_in_parallel

	external omp_set_dynamic

	external omp_set_lock	
	external omp_set_nest_lock

	external omp_set_nested

	external omp_set_num_threads

	external omp_test_lock
	logical omp_test_lock

	external omp_test_nest_lock
	integer omp_test_nest_lock

	external omp_unset_lock
	external omp_unset_nest_lock

