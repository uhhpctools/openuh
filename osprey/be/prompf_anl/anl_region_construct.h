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
#ifndef anl_region_construct_INCLUDED
#define anl_region_construct_INCLUDED

// ==============================================================
// ==============================================================
//
// Module: anl_region_construct.h
// $Revision: 1.1.1.1 $
// $Date: 2005/10/21 19:00:00 $
// $Author: marcel $
// $Source: /proj/osprey/CVS/open64/osprey1.0/be/prompf_anl/anl_region_construct.h,v $
//
// Description:
//
//    This handles constructs represented by a region.  There is
//    only one such region at the moment (PARALLEL) region, although
//    we here implement a class of object that can handle various
//    kinds.
//
// ==============================================================
// ==============================================================


class ANL_FUNC_ENTRY;
class ANL_PRAGMA_ATTRIBUTE;


class ANL_REGION_CONSTRUCT
{
private:

   enum ANL_REGION_KIND
   {
      ANL_UNKNOWN_REGION,
      ANL_PARALLEL_REGION,
      ANL_PSECTION_REGION,
      ANL_SINGLE_PROCESS_REGION,
      ANL_MASTER_PROCESS_REGION
   };

   INT64            _id;
   ANL_REGION_KIND  _region_kind;
   WN              *_region;
   INT32            _construct_level;
   BOOL             _is_nowait;
   BOOL             _is_omp;
   ANL_FUNC_ENTRY  *_func_entry;
   MEM_POOL        *_pool;

   WN *_First_Region_Stmt();
   WN *_Last_Region_Stmt();

   void _Region_Srcpos_Range(ANL_SRCPOS *min, ANL_SRCPOS *max);
   void _Region_Body_Srcpos_Range(ANL_SRCPOS *min, ANL_SRCPOS *max);

   WN* _userEndParallel();

   void _Write_Region_Directive(ANL_CBUF *cbuf);

public:

   // =================== Class Utilities ====================

   static BOOL Is_ProMpf_Region_Construct(WN *stmt);
   static BOOL Is_Valid_Dir(ANL_PRAGMA_ATTRIBUTE *dir,
			    INT32                 construct_level);


   // ============== Constructors & Destructors ==============

   ANL_REGION_CONSTRUCT(WN             *region,
			INT32           construct_level,
			ANL_FUNC_ENTRY *func_entry,
			MEM_POOL       *pool);


   // ============== Analysis and Output =============

   WN  *Next_Stmt();

   void Write(ANL_CBUF *cbuf);


}; // class ANL_REGION_CONSTRUCT

#endif /* anl_region_construct_INCLUDED */
