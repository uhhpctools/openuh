/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2.1 of the GNU Lesser General Public License 
  as published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU Lesser General Public 
  License along with this program; if not, write the Free Software 
  Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, 
  USA.

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/


#pragma ident "@(#) libfi/mpp_reduce/minloc_s_5.c	92.1	07/09/99 17:07:39"


#include <stdlib.h>
#include <liberrno.h>
#include <fmath.h>
#include <cray/dopevec.h>
#include "f90_macros.h"

#define RANK	5

/*
 *	Compiler generated call: CALL _MINLOC_JS5(RESULT, SOURCE, MASK)
 *
 *	Purpose: Determine the location of the first element of SOURCE
 *	         having the minimum value of the elements identified
 *	         by MASK. This particular routine handles source arrays
 *	         of rank 5 with a data type of 64-bit integer or
 *	         64-bit floating point.
 *
 *	Arguments:
 *	        RESULT - Dope vector for result array
 *	        SOURCE - Dope vector for user source array
 *	        MASK   - Dope vector for logical mask array
 *
 *	Description:
 *		This is the MPP single PE version of MINLOC. This
 *	        routine checks the scope of the source and mask
 *	        arrays and if they are private, calls the current
 *	        Y-MP single processor version of the routine. If they
 *	        are shared, then it allocates a result array before
 *	        calling a Fortran routine which declares the source
 *	        and mask arguments to be UNKNOWN SHARED.
 *
 *		Include file minloc_s.h contains the rank independent
 *		source code for this routine.
 */

#include "minloc_s.h"
