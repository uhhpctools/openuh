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
#ifndef anl_loop_construct_INCLUDED
#define anl_loop_construct_INCLUDED

// ==============================================================
// ==============================================================
//
// Module: anl_loop_construct.h
// $Revision: 1.1 $
// $Date: 2005/07/27 02:13:44 $
// $Author: kevinlo $
// $Source: /depot/CVSROOT/javi/src/sw/cmplr/be/prompf_anl/anl_loop_construct.h,v $
//
// Description:
//
//
// ==============================================================
// ==============================================================


class ANL_FUNC_ENTRY;
class ANL_PRAGMA_ATTRIBUTE;

class ANL_LOOP_CONSTRUCT
{
private:

   enum ANL_LOOP_KIND
   {
      ANL_DO_LOOP,
      ANL_WHILE_LOOP,
      ANL_WHILE_AS_DO_LOOP,
      ANL_DOACROSS_LOOP,
      ANL_PARALLELDO_LOOP,
      ANL_PDO_LOOP
   };

   INT64           _id;
   ANL_LOOP_KIND   _loop_kind;
   WN             *_loop_region;
   WN             *_loop;
   INT32           _construct_level;
   ANL_FUNC_ENTRY *_func_entry;
   MEM_POOL       *_pool;

   WN *_First_Loop_Stmt();
   WN *_Last_Loop_Stmt();

   static BOOL Is_Valid_Dir(ANL_PRAGMA_ATTRIBUTE *dir, 
			    INT32                 construct_level);
   static BOOL Is_End_Of_Loop_Comment(WN *end_stmt);
   static void Remove_Stmt_In_Block(WN *stmt);

   BOOL _Is_Parallel_Loop() const 
   {
      return (_loop_kind == ANL_DOACROSS_LOOP   ||
	      _loop_kind == ANL_PARALLELDO_LOOP ||
	      _loop_kind == ANL_PDO_LOOP);
   }
   
   void _Loop_Srcpos_Range(ANL_SRCPOS *min, ANL_SRCPOS *max);
   void _Write_Loop_Header(ANL_CBUF *cbuf);
   void _Write_Loop_Directive(ANL_CBUF *cbuf);

public:

   // ============== Constructors & Destructors ==============

   ANL_LOOP_CONSTRUCT(WN             *loop,
		      INT32           construct_level,
		      ANL_FUNC_ENTRY *func_entry,
		      MEM_POOL       *pool);


   // ============== Analysis and Output =============

   WN *Next_Stmt();

   void Write(ANL_CBUF *cbuf);


}; // class ANL_LOOP_CONSTRUCT

#endif /* anl_loop_construct_INCLUDED */
