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


#pragma ident "@(#) libfi/mathlb/idnint.c	92.1	07/09/99 11:00:36"


#include <fortran.h>
#include <liberrno.h>

extern _f_int8 _IDNINT_(_f_real16 *x);

/*
 * IDNINT:  Nearest integer(kind=8) in real(kind=16)
 *        - pass by address
 */

_f_int8
_IDNINT_(_f_real16 *x)
{
	_f_real16 _DINT(_f_real16 arg);
        _f_real16 y = *x;
        _f_real16 result;
        if (y < 0.0)
                y -= (_f_real16) 0.5;
        else
                y += (_f_real16) 0.5;
        result	= _DINT(y);

	/* hardware will raise invalid operation exception and
	 * return HUGE if float value too large for integer.
	 */
        return ((_f_int8) result);
}
