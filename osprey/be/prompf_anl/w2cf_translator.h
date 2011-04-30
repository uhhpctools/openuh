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
#ifndef w2cf_translator_INCLUDED
#define w2cf_translator_INCLUDED

// ==============================================================
// ==============================================================
//
// Module: w2cf_translator.h
// $Revision: 1.1 $
// $Date: 2005/07/27 02:13:44 $
// $Author: kevinlo $
// $Source: /depot/CVSROOT/javi/src/sw/cmplr/be/prompf_anl/w2cf_translator.h,v $
//
// Description:
//
//    A class used to translate subexpressions within a PU into
//    Fortran 77 or C.  The purpose of this class is merely to
//    function as an abstraction to the whirl2c/whirl2f shared
//    objects.
//
//    For translation into ANL_CBUFs, new ANL_CBUFs will be allocated
//    from the pool and memory reclamation of the cbuf is left to the
//    user of this abstraction.
//
//    Get_Pointer_To:
//       May allocate a TY entry, which will not be freed up until 
//       the destructor is called on the W2CF_TRANSLATOR.
//
// ==============================================================
// ==============================================================


class W2CF_TRANSLATOR
{
private:

   static const INT _Max_W2cf_String_Size;
   char            *_strbuf;
   WN              *_pu;
   BOOL             _translate_to_c;    
   MEM_POOL        *_pool;

   static void _Get_Ftn_Name(ANL_CBUF *cbuf, const ST *st);
   static BOOL _Is_Ptr_Expr(WN *wn);

   static TY_IDX _Get_Expr_Pointed_Ty(WN *wn);

   void _Reuse_Ptr(TY_IDX ptr);

   void _Istore_Lhs_To_String(ANL_CBUF   *cbuf,
			      WN         *lhs, 
			      STAB_OFFSET ofst,
			      TY_IDX      ty,
			      MTYPE       mtype);

   void _Mp_Schedtype_To_String(ANL_CBUF                *cbuf,
				WN_PRAGMA_SCHEDTYPE_KIND kind);

   void _Clause_Exprs_To_String(ANL_CBUF    *cbuf, 
				WN_PRAGMA_ID id,
				WN         **next_clause);

   void _Rev_Clause_Exprs_To_String(ANL_CBUF    *cbuf, 
				    WN_PRAGMA_ID id,
				    WN         **next_clause);

   void _Clause_Symbols_To_String(ANL_CBUF    *cbuf, 
				  WN_PRAGMA_ID id,
				  WN         **next_clause);

   void _Array_Segment_To_String(ANL_CBUF    *cbuf, 
				 WN_PRAGMA_ID id,
				 WN         **next_clause);

   void _Prefetch_Attributes_To_String(ANL_CBUF    *cbuf, 
				       WN          *prefetch,
				       INT32        size);

   void _Skip_Ignored_Clauses(WN **next_clause);

   void _Default_Kind_To_String(ANL_CBUF             *cbuf,
				WN_PRAGMA_DEFAULT_KIND kind);


public:

   // ============== Constructors & Destructors ==============

   W2CF_TRANSLATOR(WN *pu, MEM_POOL *pool, BOOL translate_to_c);

   ~W2CF_TRANSLATOR();


   // ============== Inquiries ==============

   TY_IDX Get_Pointer_To(TY_IDX pointed_ty);
   BOOL   Is_A_Pragma_Clause(WN *clause) const;
   BOOL   Whileloop_Looks_Like_Forloop(WN *stmt) const;
   BOOL   Language_is_C() const {return _translate_to_c;}
   

   // ============== Translation ==============

   void Expr_To_File(ANL_FILE_MNGR *file_mngr, WN *expr);

   void Expr_To_String(ANL_CBUF *cbuf, WN *expr);

   void Stid_Lhs_To_String(ANL_CBUF   *cbuf,
			   ST         *st, 
			   STAB_OFFSET ofst,
			   TY_IDX      ty,
			   MTYPE       mtype);

   void Original_Symname_To_String(ANL_CBUF *cbuf, ST *sym);
   void Transformed_Symname_To_String(ANL_CBUF *cbuf, ST *sym);

   void A_Pragma_Expr_To_String(ANL_CBUF *cbuf, WN *apragma);

   void ClauseList_To_String(ANL_CBUF *cbuf, WN **clause_list);

   void Nest_Clauses_To_String(ANL_CBUF *cbuf, 
			       WN       *nest_region, 
			       INT       nest_levels);

   void Prefetch_Attributes_To_String(ANL_CBUF    *cbuf, 
				      WN          *prefetch,
				      INT32        size);

}; // class W2CF_TRANSLATOR

#endif /* w2cf_translator_INCLUDED */
