/*
 *  Copyright (C) 2000, 2001 HPC,Tsinghua Univ.,China .  All Rights Reserved.
 *
 *      This program is free software; you can redistribute it and/or modify it
 *  under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *      This program is distributed in the hope that it would be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *      Further, this software is distributed without any warranty that it is
 *  free of the rightful claim of any third person regarding infringement
 *  or the like.  Any license provided herein, whether implied or
 *  otherwise, applies only to this software file.  Patent licenses, if
 *  any, provided herein do not apply to combinations of this program with
 *  other software, or any other product whatsoever.
 *
 *      You should have received a copy of the GNU General Public License along
 *  with this program; if not, write the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */

/*
 * File: omp_type.h
 * Abstract: basic type definitions
 * History: 04/23/2003, built by Jiang Hongshan, Tsinghua Univ.
 * 
 */
#ifndef __omp_rtl_type_included
#define __omp_rtl_type_included

/* This header file defines all the type alias used in
 * the RTL.
 */

typedef int 		omp_int32;
typedef long long 	omp_int64;
typedef float		omp_real32;
typedef double		omp_real64;
typedef int		omp_bool;

typedef int omp_int_t;
typedef double omp_wtime_t;

#define TRUE	1
#define FALSE	0

#endif /* end __omp_rtl_type_included*/
