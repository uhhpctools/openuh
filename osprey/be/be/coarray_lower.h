/*
  Copyright (C) 2010 Universty of Houston. 

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if 
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston MA 02111-1307, USA.

  Contact information:
  http://www.cs.uh.edu/~hpctools

*/

#ifndef caf_lower_h_included
#define caf_lower_h_included

#ifdef __cplusplus
extern "C" {
#endif


#ifndef wn_INCLUDED
#include "wn.h"
#endif

#ifndef pu_info_INCLUDED
#include "pu_info.h"
#endif

extern WN* Coarray_Prelower(PU_Info *current_pu, WN* pu);

#ifdef __cplusplus
}
#endif

#ifndef symtab_INCLUDED
#include "symtab.h"
#endif

#define CAF_INIT                    "caf_init_"
#define CAF_FINALIZE                "caf_finalize_"
#define ACQUIRE_LCB                 "acquire_lcb_"
#define RELEASE_LCB                 "release_lcb_"
#define COARRAY_READ                "coarray_read_"
#define COARRAY_WRITE               "coarray_write_"
#define COARRAY_STRIDED_READ        "coarray_strided_read_"
#define COARRAY_STRIDED_WRITE       "coarray_strided_write_"
#define COARRAY_READ_SRC_STR        "coarray_read_src_str_"
#define COARRAY_READ_FULL_STR       "coarray_read_full_str_"
#define COARRAY_WRITE_DEST_STR      "coarray_write_dest_str_"
#define COARRAY_WRITE_FULL_STR      "coarray_write_full_str_"

#endif /* caf_lower_h_included */

