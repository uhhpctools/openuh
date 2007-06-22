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


/* -*-Mode: c++;-*- (Tell emacs to use c++ mode) */
#ifndef anl_dir_attribute_INCLUDED
#define anl_dir_attribute_INCLUDED

// ==============================================================
// ==============================================================
//
// Module: anl_pragma_attribute.h
// $Revision: 1.1.1.1 $
// $Date: 2005/10/21 19:00:00 $
// $Author: marcel $
// $Source: /proj/osprey/CVS/open64/osprey1.0/be/prompf_anl/anl_pragma_attribute.h,v $
//
// Description:
//
//    This defines a class representing a directive attribute, which
//    should belong to some enclosing construct (function, loop, etc.).
//
// ==============================================================
// ==============================================================


class ANL_FUNC_ENTRY;
class ANL_SRCPOS;


class ANL_PRAGMA_ATTRIBUTE
{
private:

   enum ANL_PRAGMA_ATTR_KIND
   {
      ANL_UNKNOWN_ATTR,
      ANL_PREFETCH_REF_ATTR,
      ANL_DISTRIBUTE_ATTR,
      ANL_REDISTRIBUTE_ATTR,
      ANL_DISTRIBUTE_RESHAPE_ATTR,
      ANL_DYNAMIC_ATTR,
      ANL_COPYIN_ATTR,
      ANL_NUMTHREADS_ATTR,
      ANL_PAGE_PLACE_ATTR,
      ANL_CONCURRENTIZE,
      ANL_NOCONCURRENTIZE,
      ANL_ASSERT_PERMUTATION,
      ANL_ASSERT_CONCURRENT_CALL,
      ANL_ASSERT_DO,
      ANL_ASSERT_DOPREFER,
      ANL_IVDEP,
      ANL_FLUSH,
      ANL_NUM_ATTRS
   };

   ANL_PRAGMA_ATTR_KIND _pragma_kind;
   WN                  *_apragma;
   ANL_FUNC_ENTRY      *_func_entry;
   INT32                _enclosing_construct_level;
   MEM_POOL            *_pool;
   
   static void _Append_Arg_Numbers(ANL_CBUF *cbuf,
				   INT32     val1,
				   INT32     val2);

   BOOL _Is_Assertion();
   void _Write_Distribution(ANL_CBUF *cbuf, WN **next);
   void _Write_Pragma_Arguments(ANL_CBUF *cbuf);
   void _Write_Pragma(ANL_CBUF *cbuf);

public:

   // =================== Class Utilities ====================

   static BOOL Is_ProMpf_Pragma_Attribute(WN *stmt);


   // ============== Constructors & Destructors ==============

   ANL_PRAGMA_ATTRIBUTE(WN             *apragma,
			INT32           enclosing_construct_level,
			ANL_FUNC_ENTRY *func_entry,
			MEM_POOL       *pool);


   // ======================== Queries ========================
      
   BOOL Is_Pragma_Construct_Attribute(INT32 construct_level);
   BOOL Is_Region_Construct_Attribute(INT32 construct_level);
   BOOL Is_Loop_Construct_Attribute(INT32 construct_level);


   // =================== Analysis and Output =================

   WN *Next_Stmt(); // Stmt after last clause belonging to this directive

   void Write(ANL_CBUF *cbuf, INT32 id);


}; // class ANL_PRAGMA_ATTRIBUTE

#endif /* anl_pragma_attribute_INCLUDED */
