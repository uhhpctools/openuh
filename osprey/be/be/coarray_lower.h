/*
  Copyright (C) 2010-2014 Universty of Houston.

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

