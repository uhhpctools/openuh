/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

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

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/


#ifndef anl_common_INCLUDED
#define anl_common_INCLUDED
/* ==============================================================
 * ==============================================================
 *
 * Module: anl_common.h
 * $Revision: 1.1.1.1 $
 * $Date: 2005/10/21 19:00:00 $
 * $Author: marcel $
 * $Source: /proj/osprey/CVS/open64/osprey1.0/be/prompf_anl/anl_common.h,v $
 *
 * Description:
 *
 *   Includes and defines stuff commonly used in prompf_anl.so
 *   C++ sources.
 *
 * ==============================================================
 * ==============================================================
 */

#include "defs.h"            // Basic types, such as INT32

#include "config.h"
#include "erglob.h"          // For errors.h and EC definitions
#include "glob.h"            // Irb_File_Name and Src_File_Name
#include "wn.h"              // WN nodes
#include "wn_util.h"         // WN nodes
#include "symtab.h"           // TY and ST nodes
#include "irbdata.h"         // includes targ_const.h
#include "wintrinsic.h"      // WN intrinsic codes
#include "wn_pragmas.h"      // WN pragma codes
#include "cxx_memory.h"      // CXX_NEW/CXX_DELETE

#include "lwn_util.h"        // For LWN_Parentize()
#include "flags.h"           // For OPTION_DESC
#include "file_util.h"       // For Same_File, Last_Pathname_Component
#include "anl_diagnostics.h" // Diagnostics utilities
#include "anl_cbuf.h"        // Abstraction for character string buffers
#include "anl_srcpos.h"      // For source position manipulation


typedef TYPE_ID MTYPE;
typedef mINT64  STAB_OFFSET;

#define TY_IS_POINTER(ty) (TY_kind(ty) == KIND_POINTER)
#define TY_IS_ARRAY(ty) (TY_kind(ty) == KIND_ARRAY)

#define ST_AT_FILE_SCOPE(st) Is_Global_Symbol(st)

#define ST_IS_VALID_BASE(st) \
   (ST_base(st) != NULL && \
    ST_base(st) != (st) && \
    ST_sym_class(ST_base(st)) != CLASS_BLOCK) /* cg generated */ 

#define ST_IS_COMMON_BLOCK(st) \
    (PU_ftn_lang(Get_Current_PU()) &&       \
     (ST_sclass(st) == SCLASS_COMMON ||     \
      ST_sclass(st) == SCLASS_DGLOBAL)   && \
     TY_kind(ST_type(st)) == KIND_STRUCT && \
     !ST_AT_FILE_SCOPE(st))

#define ST_IS_EQUIVALENCE_BLOCK(st) \
    (PU_ftn_lang(Get_Current_PU()) &&       \
     TY_kind(ST_type(st)) == KIND_STRUCT && \
     ST_sclass(st) != SCLASS_COMMON      && \
     ! TY_fld(Ty_Table[ST_type(st)]).Is_Null()  && \
     (FLD_equivalence(TY_fld(Ty_Table[ST_type(st)]))))

#define ST_IS_BASED_AT_COMMON_OR_EQUIVALENCE(st) \
   (ST_IS_VALID_BASE(st) &&                      \
    (ST_IS_COMMON_BLOCK(ST_base(st)) ||          \
     ST_IS_EQUIVALENCE_BLOCK(ST_base(st))))

#define ST_NO_LINKAGE(st) \
    (ST_export(st) == EXPORT_LOCAL ||   \
     ST_export(st) == EXPORT_LOCAL_INTERNAL)

#define ST_EXTERNAL_LINKAGE(st) \
    (ST_export(st) == EXPORT_INTERNAL  ||  \
     ST_export(st) == EXPORT_HIDDEN    ||  \
     ST_export(st) == EXPORT_PROTECTED ||  \
     ST_export(st) == EXPORT_OPTIONAL  ||  \
     ST_export(st) == EXPORT_PREEMPTIBLE)

      
void 
Adjust_Srcpos_Range(WN         *stmt,
		    ANL_SRCPOS *min,
		    ANL_SRCPOS *max);  // Defined in anl_func_entry.cxx


class COUNTER
{
private:
   INT64 _value;
public:
   COUNTER(INT64 start): _value(start) {}
   void  Reset(INT64 start) {_value = start;}
   INT64 Value() const {return _value;}
   INT64 Post_Incr() {return _value++;}
   INT64 Pre_Incr() {return ++_value;}
}; // COUNTER

#endif /* anl_common_INCLUDED */
