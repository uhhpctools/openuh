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


/* $Header: /usr/local/cvs/openuh-2.0/kpro64/libF77/rand_.c,v 1.1.1.1 2006-10-19 18:05:20 oscar Exp $ */
/*
Uniform random number generator. 
Linear congruential generator, suitable for 32 bit machines;
multiplication is mod 2**31
*/
#include "cmplrs/host.h"

static	int32	randx = 1;

void
srand_(int32 *x)	/* subroutine to set seed */
{
randx = *x;
}

int32
irand_(void)
{
	return(((randx = randx * 1103515245L + 12345)>>16) & 0x7fff);
}

double rand_(void)
{
    
	return(irand_()/32768.0);
}

