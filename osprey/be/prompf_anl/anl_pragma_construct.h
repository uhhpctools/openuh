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
#ifndef anl_pragma_construct_INCLUDED
#define anl_pragma_construct_INCLUDED

// ==============================================================
// ==============================================================
//
// Module: anl_pragma_construct.h
// $Revision: 1.1.1.1 $
// $Date: 2005/10/21 19:00:00 $
// $Author: marcel $
// $Source: /proj/osprey/CVS/open64/osprey1.0/be/prompf_anl/anl_pragma_construct.h,v $
//
// Description:
//
//
// ==============================================================
// ==============================================================


class ANL_FUNC_ENTRY;
class ANL_PRAGMA_ATTRIBUTE;

class ANL_PRAGMA_CONSTRUCT
{
private:

   enum ANL_PRAGMA_KIND
   {
      ANL_UNKNOWN_PRAGMA,
      ANL_SECTION_PRAGMA,
      ANL_BARRIER_PRAGMA,
      ANL_ORDERED_PRAGMA,
      ANL_ATOMIC_PRAGMA,
      ANL_CRITICAL_SECTION_PRAGMA
   };

   INT64            _id;
   ANL_PRAGMA_KIND  _pragma_kind;
   WN              *_pragma_begin;
   WN              *_pragma_end;           // If applicable; otherwise NULL
   BOOL             _pr_end_part_of_body ; // is pragma_end last part of construct body
   BOOL             _is_omp ;
   INT32            _construct_level;
   ANL_FUNC_ENTRY  *_func_entry;
   MEM_POOL        *_pool;

   BOOL _End_Is_Part_Of_Construct(BOOL for_id);
   void _Find_Pragma_End_Stmt();
   void _Get_Max_Construct_Srcpos(ANL_SRCPOS *max);
   void _Write_Pragma_Directives(ANL_CBUF *cbuf);

public:

   // =================== Class Utilities ====================

   static BOOL Is_ProMpf_Pragma_Construct(WN *stmt);
   static BOOL Is_Valid_Dir(ANL_PRAGMA_ATTRIBUTE *dir,
			    INT32                 construct_level);


   // ============== Constructors & Destructors ==============

   ANL_PRAGMA_CONSTRUCT(WN             *pragma_begin,
			INT32           construct_level,
			ANL_FUNC_ENTRY *func_entry,
			MEM_POOL       *pool);


   // ============== Analysis and Output =============

   WN *Next_Stmt();

   void Write(ANL_CBUF *cbuf);


}; // class ANL_PRAGMA_CONSTRUCT

#endif /* anl_pragma_construct_INCLUDED */
