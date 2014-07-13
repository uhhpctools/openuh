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

extern WN* Coarray_Lower(PU_Info *current_pu, WN* pu);
extern WN* Coarray_Prelower(PU_Info *current_pu, WN* pu);
extern WN* Coarray_Symbols_Lower(PU_Info *current_pu, WN *pu);
extern void Coarray_Global_Symbols_Remove();


#ifdef __cplusplus
}
#endif

#ifndef symtab_INCLUDED
#include "symtab.h"
#endif

#define CAF_INIT                            "__caf_init"
#define CAF_FINALIZE                        "__caf_finalize"
#define TARGET_ALLOC                        "__target_alloc"
#define TARGET_DEALLOC                      "__target_dealloc"
#define ACQUIRE_LCB                         "__acquire_lcb"
#define RELEASE_LCB                         "__release_lcb"
#define COARRAY_NBREAD                      "__coarray_nbread"
#define COARRAY_READ                        "__coarray_read"
#define COARRAY_WRITE_FROM_LCB              "__coarray_write_from_lcb"
#define COARRAY_WRITE                       "__coarray_write"
#define COARRAY_STRIDED_NBREAD              "__coarray_strided_nbread"
#define COARRAY_STRIDED_READ                "__coarray_strided_read"
#define COARRAY_STRIDED_WRITE_FROM_LCB      "__coarray_strided_write_from_lcb"
#define COARRAY_STRIDED_WRITE               "__coarray_strided_write"
#define COARRAY_WAIT                        "__coarray_wait"
#define COARRAY_WAIT_ALL                    "__coarray_wait_all"

#endif /* caf_lower_h_included */

