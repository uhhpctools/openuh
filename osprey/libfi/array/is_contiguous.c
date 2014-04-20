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


#pragma ident "@(#) libfi/array/is_contiguous.c	92.2	07/07/99 15:52:02"
#include <liberrno.h>
#include <stddef.h>
#include <cray/dopevec.h>
#include <cray/portdefs.h>

#include <stdlib.h>

#define BITS_PER_BYTE   (BITS_PER_WORD / BYTES_PER_WORD)

/*
 *      IS_CONTIGUOUS
 */

int
_IS_CONTIGUOUS  (DopeVectorType * source)
{
    long int n_dim = source->n_dim;
    long int elem_size;
    size_t num_elems;
    int is_contig = 0;
    int i,j,k;

    // determine the total size of the array and create a rank 1 dope vector
    if (source->type_lens.type == DVTYPE_ASCII) {
        elem_size = source->base_addr.charptr.byte_len;
    } else {
        elem_size = source->base_addr.a.el_len >> 3;
    }

    // check if source is contiguous or not
    is_contig = 1;
    num_elems = 1;
    for (i = 0; i < n_dim-1; i++) {
        if (i == 0) {
            int first_stride;
            if (source->type_lens.type == DVTYPE_ASCII ||
                    source->type_lens.type == DVTYPE_DERIVEDBYTE) {
                /* first dim is strided if the first stride multipler /
                 * elem_size is greater than 1 */
                first_stride =
                    source->dimension[0].stride_mult / elem_size;
            } else if (elem_size > BYTES_PER_WORD) {
                first_stride = source->dimension[0].stride_mult /
                    (elem_size / BYTES_PER_WORD);
            } else {
                first_stride = source->dimension[0].stride_mult;
            }
            if (first_stride > 1) {
                is_contig = 0;
            }
        } else {
            if (num_elems < source->dimension[i].stride_mult) {
                is_contig = 0;
            }
        }
        num_elems = num_elems * source->dimension[i].extent;
    }
    if (num_elems < source->dimension[n_dim-1].stride_mult &&
            source->dimension[n_dim-1].extent > 1) {
        is_contig = 0;
    }

    return is_contig;
}
