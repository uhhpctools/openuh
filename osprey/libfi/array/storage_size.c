/*
  Copyright (C) 2014 University of Houston.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  Contact information:
http://www.cs.uh.edu/~hpctools

*/


#pragma ident "@(#) libfi/array/storage_size.c	92.2	07/07/99 15:52:02"
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
_STORAGE_SIZE_4   (DopeVectorType * source)
{
        _f_int4 iresult;
        int elem_size;
        int dim;
        int rank;
        int loopj;

	/* If source is a pointer/allocatable array, it must be
	 * associated/allocated. */
	if (source->p_or_a  &&  !source->assoc)
		_lerror (_LELVL_ABORT, FENMPTAR, "STORAGE_SIZE");
    /* determine the total size of the array */

    if (source->type_lens.type == DVTYPE_ASCII) {
        elem_size = source->base_addr.charptr.byte_len << 3;
    } else {
        elem_size = source->base_addr.a.el_len;
    }

	rank = source->n_dim;
    iresult = elem_size;

    /* Retrieve product of extents */
/*    for (loopj = 0; loopj < rank; loopj++)
        iresult = iresult * source->dimension[loopj].extent;
*/
    return(iresult);
}

/*
 */

_f_int8
_STORAGE_SIZE_8   (DopeVectorType * source)
{
        _f_int8 iresult;
        int elem_size;
        int dim;
        int rank;
        int loopj;

	/* If source is a pointer/allocatable array, it must be
	 * associated/allocated. */
	if (source->p_or_a  &&  !source->assoc)
		_lerror (_LELVL_ABORT, FENMPTAR, "STORAGE_SIZE");

    /* determine the total size of the array */
    if (source->type_lens.type == DVTYPE_ASCII) {
        elem_size = source->base_addr.charptr.byte_len << 3;
    } else {
        elem_size = source->base_addr.a.el_len;
    }

	rank = source->n_dim;
    iresult = elem_size;

    /* Retrieve product of extents */
/*    for (loopj = 0; loopj < rank; loopj++)
        iresult = iresult * source->dimension[loopj].extent;
*/
    return(iresult);
}
