/*
  Copyright (C) 2014 University of Houston.

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


#pragma ident "@(#) libfi/array/sizeof.c	92.2	07/07/99 15:52:02"
#include <liberrno.h>
#include <stddef.h>
#include <cray/dopevec.h>

/*
 */

#if defined (_UNICOS)
#pragma duplicate _SIZE_4 as _SIZE_2
#pragma duplicate _SIZE_4 as _SIZE_1
#endif

_f_int4
_SIZEOF_4   (DopeVectorType * source)
{
        _f_int4 iresult;
        int elem_size;
        int dim;
        int rank;
        int loopj;

	/* If source is a pointer/allocatable array, it must be
	 * associated/allocated. */
	if (source->p_or_a  &&  !source->assoc)
		_lerror (_LELVL_ABORT, FENMPTAR, "SIZE");

    /* determine the total size of the array */
    if (source->type_lens.type == DVTYPE_ASCII) {
        elem_size = source->base_addr.charptr.byte_len;
    } else {
        elem_size = source->base_addr.a.el_len >> 3;
    }

	rank = source->n_dim;
    iresult = elem_size;

    /* Retrieve product of extents */
    for (loopj = 0; loopj < rank; loopj++)
        iresult = iresult * source->dimension[loopj].extent;

    return(iresult);
}

/*
 */

_f_int8
_SIZEOF_8   (DopeVectorType * source,
	_f_int *dimptr)
{
        _f_int8 iresult;
        int elem_size;
        int dim;
        int rank;
        int loopj;

	/* If source is a pointer/allocatable array, it must be
	 * associated/allocated. */
	if (source->p_or_a  &&  !source->assoc)
		_lerror (_LELVL_ABORT, FENMPTAR, "SIZE");

    /* determine the total size of the array */
    if (source->type_lens.type == DVTYPE_ASCII) {
        elem_size = source->base_addr.charptr.byte_len;
    } else {
        elem_size = source->base_addr.a.el_len >> 3;
    }

	rank = source->n_dim;
    iresult = elem_size;

    /* Retrieve product of extents */
    for (loopj = 0; loopj < rank; loopj++)
        iresult = iresult * source->dimension[loopj].extent;

    return(iresult);
}
