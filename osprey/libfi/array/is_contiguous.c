/*
  Copyright (C) 2014 University of Houston. All Rights Reserved.

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
