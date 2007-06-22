#!/bin/csh -f
### ======================================================================
### ======================================================================
###
### Module: gen_st_list.csh
### $Revision: 1.1.1.1 $
### $Date: 2005/10/21 19:00:00 $
### $Author: marcel $
### $Source: /proj/osprey/CVS/open64/osprey1.0/be/cg/gen_st_list.csh,v $
### Revision history:
###   27-Feb-92 - Original version
###
### Usage:      gen_st_list MTP_BIN
###
###     Generate the st_list.[ch] module.  The argument is the MTP_BIN
###     directory.  We do this in a file so the make rule can depend on
###     and it can be rebuilt when the procedure changes
###
### ======================================================================
### ======================================================================



csh -f $1/gen_x_list.csh    'ST*'                                      \
                            'ST'                                       \
                            'defs.h'                                   \
			    'errors.h'				       \
                            'mempool.h'                                 \
                            'cgir.h'                                   \
                            'st_list.h'
