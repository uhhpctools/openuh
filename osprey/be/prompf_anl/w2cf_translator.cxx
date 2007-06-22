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

#include "wn_tree_util.h"
#include "anl_common.h"
#include "anl_diagnostics.h" // For warnings and errors
#include "w2c_driver.h"      // For whirl2c.so utilities
#include "w2f_driver.h"      // For whirl2f.so utilities
#include "anl_file_mngr.h"   // For managing files
#include "w2cf_translator.h"

extern ANL_DIAGNOSTICS *Anl_Diag; // Defined in anl_driver.cxx

// Remove any direct dependence on whirl2f.so and whirl2c.so
//
#pragma weak    W2C_Push_PU
#pragma weak    W2C_Pop_PU
#pragma weak    W2C_Translate_Wn
#pragma weak    W2C_Translate_Stid_Lhs
#pragma weak    W2C_Translate_Istore_Lhs
#pragma weak    W2C_Process_Command_Line
#pragma weak    W2C_Init
#pragma weak    W2C_Translate_Wn_Str
#pragma weak    W2C_Object_Name
#pragma weak    W2F_Push_PU
#pragma weak    W2F_Pop_PU
#pragma weak    W2F_Translate_Wn
#pragma weak    W2F_Translate_Stid_Lhs
#pragma weak    W2F_Translate_Istore_Lhs
#pragma weak    W2F_Process_Command_Line
#pragma weak    W2F_Init
#pragma weak    W2F_Translate_Wn_Str
#pragma weak    W2F_Object_Name

// ============== General Purpose Utilities ===============
// ========================================================

#define NEXT_ANL_POINTER(p) TY_pointer(p)


// Class used in combination with TREE_WALK to determine number
// of WN_st references to a given ST from WHIRL nodes of the given
// operator.
//
class NUM_ST_REFS
{
private:
   const OPERATOR _opr;
   ST * const     _st;
   INT            _num_nodes;
public:
   NUM_ST_REFS(OPERATOR opr, ST *st): _opr(opr), _st(st), _num_nodes(0) {}
   INT num_nodes() const {return _num_nodes;}

   void  operator()(WN* wn)
   {
      if (WN_operator(wn) == _opr && WN_st(wn) == _st)
	 ++_num_nodes;
   }
}; // NUM_ST_REFS


// ==================== Class Members =====================
// ========================================================

const INT W2CF_TRANSLATOR::_Max_W2cf_String_Size = 1024*5;


void
W2CF_TRANSLATOR::_Get_Ftn_Name(ANL_CBUF *cbuf, const ST *st)
{
   // PRECONDITION:  ST_name(st) != NULL
   //
   const char *name_ptr = ST_name(st);

   if (ST_EXTERNAL_LINKAGE(st) && 
       !ST_IS_BASED_AT_COMMON_OR_EQUIVALENCE(st) &&
       !(ST_sym_class(st) == CLASS_VAR && ST_is_namelist(st)))
   {
      // Here we deal with a curiosity of the Fortran naming scheme for
      // external names:
      //
      //    + If the name ends with a '_', the name was without the '_'
      //      in the original Fortran source.
      //
      //    + If the name ends without a '_', the name was with a '$'
      //      suffix in the original Fortran source.
      //
      //    + If the name begins with a '_', always emit a trailing '$'.
      //      
      const char first_char = name_ptr[0];

      while (*name_ptr != '\0')
      {
	 if (first_char != '_' && name_ptr[0] == '_')
	 {
	    if (name_ptr[1] == '\0')
	    {
	       // Ignore ending underscore.
	       name_ptr += 1;
	    }
	    else if (name_ptr[1] == '_' && name_ptr[2] == '\0')
	    {
	       // Ignore ending two underscores.
	       name_ptr += 2;
	    }
	    else
	    {
	       // make underscore part of the name
	       //
	       cbuf->Write_Char(name_ptr[0]);
	       name_ptr += 1;
	    }
	 }
	 else
	 {
	    cbuf->Write_Char(name_ptr[0]);
	    name_ptr += 1;
	    if (name_ptr[0] == '\0')
	       cbuf->Write_Char('$'); // Ends with '$'
	 }
      } // While more characters
   } 
   else if (name_ptr != NULL)
   {
      cbuf->Write_String(name_ptr);
   }
} // W2CF_TRANSLATOR::_Get_Ftn_Name


BOOL 
W2CF_TRANSLATOR::_Is_Ptr_Expr(WN *wn)
{
   BOOL predicate;

   switch (WN_operator(wn))
   {
   case OPR_LDID:
   case OPR_ILOAD:
      predicate = TY_IS_POINTER(WN_ty(wn));
      break;

   case OPR_ARRAY:
      predicate = TRUE;
      break;

   case OPR_LDA:
      predicate = TRUE;
      break;

   case OPR_ADD:
      predicate = _Is_Ptr_Expr(WN_kid0(wn)) || _Is_Ptr_Expr(WN_kid1(wn));
      break;

   default:
      predicate = FALSE;
      break;
   }
   return predicate;
} // W2CF_TRANSLATOR::_Is_Ptr_Expr


TY_IDX 
W2CF_TRANSLATOR::_Get_Expr_Pointed_Ty(WN *wn)
{
   // PRECONDITION: _Is_Ptr_Expr(wn) == TRUE
   //
   // Returns the type pointed to by the given pointer expression
   //
   TY_IDX ty;

   switch (WN_operator(wn))
   {
   case OPR_LDID:
   case OPR_ILOAD:
      ty = TY_pointed(WN_ty(wn));
      break;

   case OPR_ARRAY:
      ty = _Get_Expr_Pointed_Ty(WN_kid0(wn));
      if (TY_IS_ARRAY(ty))
	 ty = TY_AR_etype(ty);
      break;

   case OPR_LDA:
      ty = TY_pointed(WN_ty(wn));
      break;

   case OPR_ADD:
      if (_Is_Ptr_Expr(WN_kid0(wn)))
	 ty = _Get_Expr_Pointed_Ty(WN_kid0(wn));
      else if (_Is_Ptr_Expr(WN_kid1(wn)))
	 ty = _Get_Expr_Pointed_Ty(WN_kid1(wn));
      else
	 ty = NULL;
      break;

   default:
      ty = NULL;
      break;
   }
   return ty;
} // W2CF_TRANSLATOR::_Get_Expr_Pointed_Ty


// ==================== Hidden Members ====================
// ========================================================
//
// In the 7.2 version of the symtab, pseudo-TY entries were
// created & linked together. They were created in a pool local
// to ANL. As this can't be done in the new symbol table, we do
// not reuse pointers anymore and just add new pointers to the
// symbol table as appropriate.  As only character
// objects (strings?) get these pointer TYs, overhead shouldn't be
// too bad.  See _Reuse_Ptr in revision 1.18 of this file and
// revision 1.9 of w2cf_translator.h.
//==========================================================

void
W2CF_TRANSLATOR::_Istore_Lhs_To_String(ANL_CBUF   *cbuf, 
				       WN         *lhs,
				       STAB_OFFSET ofst,
				       TY_IDX      ty, 
				       MTYPE       mtype)

{
   if (_translate_to_c)
   {
      W2C_Push_PU(_pu, lhs);
      W2C_Translate_Istore_Lhs(_strbuf, _Max_W2cf_String_Size,
			       lhs, ofst, ty, mtype);
      W2C_Pop_PU();
   }
   else
   {
      W2F_Push_PU(_pu, lhs);
      W2F_Translate_Istore_Lhs(_strbuf, _Max_W2cf_String_Size,
			       lhs, ofst, ty, mtype);
      W2F_Pop_PU();
   }
   cbuf->Write_String(_strbuf);
} // W2CF_TRANSLATOR::_Istore_Lhs_To_String


void 
W2CF_TRANSLATOR::_Mp_Schedtype_To_String(ANL_CBUF                *cbuf,
					 WN_PRAGMA_SCHEDTYPE_KIND kind)
{
   switch (kind)
   {
   case WN_PRAGMA_SCHEDTYPE_RUNTIME:
      cbuf->Write_String("runtime");
      break;
   case WN_PRAGMA_SCHEDTYPE_SIMPLE:
      cbuf->Write_String("simple");
      break;
   case WN_PRAGMA_SCHEDTYPE_INTERLEAVE:
      cbuf->Write_String("interleaved");
      break;
   case WN_PRAGMA_SCHEDTYPE_DYNAMIC:
      cbuf->Write_String("dynamic");
      break;
   case WN_PRAGMA_SCHEDTYPE_GSS:
      cbuf->Write_String("gss");
      break;
   case WN_PRAGMA_SCHEDTYPE_PSEUDOLOWERED:
      cbuf->Write_String("pseudolowered");
      break;
   default:
      cbuf->Write_String("<mpschedtype??>");
      break;
   }
} // W2CF_TRANSLATOR::_Mp_Schedtype_To_String



void 
W2CF_TRANSLATOR::_Default_Kind_To_String (ANL_CBUF             *cbuf,
					  WN_PRAGMA_DEFAULT_KIND kind)
{
  // print a string associated with a WN_PRAGMA_DEFAULT

  switch (kind)
    {
    case WN_PRAGMA_DEFAULT_NONE:
      cbuf->Write_String(" none ") ;
      break;
    case WN_PRAGMA_DEFAULT_SHARED:
      cbuf->Write_String(" shared ") ;
      break;
    case WN_PRAGMA_DEFAULT_PRIVATE:
      cbuf->Write_String(" private ") ;
      break;
    default:
      cbuf->Write_String("<default kind??>");
      break;
    }

} // W2CF_TRANSLATOR::_Default_Kind_To_String



void
W2CF_TRANSLATOR::_Clause_Exprs_To_String(ANL_CBUF    *cbuf, 
					 WN_PRAGMA_ID id,
					 WN         **next_clause)
{
   BOOL first_clause = TRUE;
   WN  *clause;

   cbuf->Write_Char('(');
   for (clause = *next_clause;
	(clause != NULL && 
	 WN_operator(clause) == OPR_XPRAGMA && 
	 WN_pragma(clause) == id);
	clause = WN_next(clause))
   {
      if (first_clause)
	 first_clause = FALSE;
      else
	 cbuf->Write_String(", ");
      
      A_Pragma_Expr_To_String(cbuf, clause);
   }
   cbuf->Write_Char(')');
   *next_clause = clause;
} // W2CF_TRANSLATOR::_Clause_Exprs_To_String


void
W2CF_TRANSLATOR::_Rev_Clause_Exprs_To_String(ANL_CBUF    *cbuf, 
					     WN_PRAGMA_ID id,
					     WN         **next_clause)
{
   WN  *first_clause = *next_clause;
   WN  *last_clause = NULL;
   WN  *clause;

   for (clause = first_clause;
	(clause != NULL &&
	 WN_operator(clause) == OPR_XPRAGMA && 
	 WN_pragma(clause) == id);
	clause = WN_next(clause))
   {
      last_clause = clause;
   }

   cbuf->Write_Char('(');
   for (clause = last_clause;
	clause != NULL && clause != WN_prev(first_clause);
	clause = WN_prev(clause))
   {
      if (clause != last_clause)
	 cbuf->Write_String(", ");
      
      if (id == WN_PRAGMA_ONTO && 
	  WN_operator(WN_kid0(clause)) == OPR_INTCONST &&
	  WN_const_val(WN_kid0(clause)) == 0)
      {
	 cbuf->Write_Char('*'); // Special case!
      }
      else
	 A_Pragma_Expr_To_String(cbuf, clause);
   }
   cbuf->Write_Char(')');
   *next_clause = WN_next(last_clause);
} // W2CF_TRANSLATOR::_Rev_Clause_Exprs_To_String


void
W2CF_TRANSLATOR::_Clause_Symbols_To_String(ANL_CBUF    *cbuf, 
					   WN_PRAGMA_ID id,
					   WN         **next_clause)
{
   BOOL first_clause = TRUE;
   WN  *clause;

   cbuf->Write_Char('(');
   for (clause = *next_clause;
	(clause != NULL && 
	 WN_operator(clause) == OPR_PRAGMA && 
	 WN_pragma(clause) == id);
	clause = WN_next(clause))
   {
      ST * const st = WN_st(clause);

      if (ST_class(st) != CLASS_PREG) // Pregs are not in original source
      {
	 if (first_clause)
	    first_clause = FALSE;
	 else
	    cbuf->Write_String(", ");

	 Original_Symname_To_String(cbuf, st); 
      }
   }
   cbuf->Write_Char(')');
   *next_clause = clause;
} // W2CF_TRANSLATOR::_Clause_Symbols_To_String


void
W2CF_TRANSLATOR::_Array_Segment_To_String(ANL_CBUF    *cbuf, 
					  WN_PRAGMA_ID id,
					  WN         **next_clause)
{
   BOOL first_clause = TRUE;
   WN  *clause;

   cbuf->Write_Char('(');
   for (clause = *next_clause;
	(clause != NULL && 
	 WN_operator(clause) == OPR_XPRAGMA && 
	 WN_pragma(clause) == id);
	clause = WN_next(clause))
   {
      ST * const st = WN_st(clause);

      if (ST_class(st) != CLASS_PREG) // Pregs are not in original source
      {
	 if (first_clause)
	    first_clause = FALSE;
	 else
	    cbuf->Write_String(", ");

	 Original_Symname_To_String(cbuf, st); 
	 cbuf->Write_Char('(');
	 if (_translate_to_c)
	    cbuf->Write_Int(0);
	 else
	    cbuf->Write_Int(1);
	 cbuf->Write_Char(':');
	 A_Pragma_Expr_To_String(cbuf, clause);
	 if (_translate_to_c)
	 {
	    cbuf->Write_Char('-');
	    cbuf->Write_Int(1);
	 }
	 cbuf->Write_Char(')');
      }
   }
   cbuf->Write_Char(')');
   *next_clause = clause;
} // W2CF_TRANSLATOR::_Array_Segment_To_String


void
W2CF_TRANSLATOR::_Skip_Ignored_Clauses(WN **next_clause)
{
   BOOL skipped = TRUE;

   while (skipped && Is_A_Pragma_Clause(*next_clause))
   {
      if (WN_pragma_compiler_generated(*next_clause))
      {
	 *next_clause = WN_next(*next_clause);
      }
      else
      {
	 switch (WN_pragma(*next_clause))
	 {
	 case WN_PRAGMA_DATA_AFFINITY:
	 case WN_PRAGMA_THREAD_AFFINITY:
	 case WN_PRAGMA_MPNUM:
	 case WN_PRAGMA_SYNC_DOACROSS:
	 case WN_PRAGMA_DEFAULT:
	    *next_clause = WN_next(*next_clause);
	    break;
	 default:
	    skipped = FALSE;
	    break;
	 }
      }
   }
} // W2CF_TRANSLATOR::_Skip_Ignored_Clauses


// =============== Public Member Functions ================
// ========================================================

W2CF_TRANSLATOR::W2CF_TRANSLATOR(WN       *pu, 
				 MEM_POOL *pool,
				 BOOL      translate_to_c):
   _pu(pu),
   _pool(pool),
   _translate_to_c(translate_to_c)
{
   _strbuf = CXX_NEW_ARRAY(char, _Max_W2cf_String_Size, _pool);
} // W2CF_TRANSLATOR::W2CF_TRANSLATOR


W2CF_TRANSLATOR::~W2CF_TRANSLATOR()
{
   TY *ptr;

   CXX_DELETE_ARRAY(_strbuf, _pool);
} // W2CF_TRANSLATOR::~W2CF_TRANSLATOR


BOOL
W2CF_TRANSLATOR::Is_A_Pragma_Clause(WN *clause) const
{
   // Always keep this in synch with ClauseList_To_String().
   //
   BOOL predicate = (clause != NULL && 
		     (WN_operator(clause) == OPR_PRAGMA ||
		      WN_operator(clause) == OPR_XPRAGMA));

   if (predicate)
   {
      switch (WN_pragma(clause))
      {
      case WN_PRAGMA_AFFINITY:
      case WN_PRAGMA_DATA_AFFINITY:
      case WN_PRAGMA_THREAD_AFFINITY:
      case WN_PRAGMA_CHUNKSIZE:
      case WN_PRAGMA_IF:
      case WN_PRAGMA_LASTLOCAL:
      case WN_PRAGMA_LOCAL:
      case WN_PRAGMA_MPSCHEDTYPE:
      case WN_PRAGMA_ORDERED:
      case WN_PRAGMA_REDUCTION:
      case WN_PRAGMA_SHARED:
      case WN_PRAGMA_ONTO:
      case WN_PRAGMA_LASTTHREAD:
      case WN_PRAGMA_MPNUM:
      case WN_PRAGMA_SYNC_DOACROSS:
      case WN_PRAGMA_FIRSTPRIVATE:
      case WN_PRAGMA_DEFAULT:
         break;

      case WN_PRAGMA_NOWAIT:
	 predicate = _translate_to_c;
	 break;
	 
      default:
	 predicate = FALSE;
	 break;
      }
   }
   return predicate;
} // W2CF_TRANSLATOR::Is_A_Pragma_Clause


TY_IDX
W2CF_TRANSLATOR::Get_Pointer_To(TY_IDX pointed_ty)
{
   TY_IDX ptr = TY_pointer(pointed_ty);

   if (ptr == NULL)
   {
     ptr = Make_Pointer_Type(pointed_ty,FALSE);
   }
   return ptr;
} // W2CF_TRANSLATOR::Get_Pointer_To


BOOL
W2CF_TRANSLATOR::Whileloop_Looks_Like_Forloop(WN *stmt) const
{
   BOOL pred = FALSE;
   WN * const prev_stmt = WN_prev(stmt);
   
   if (_translate_to_c                     &&
       (WN_operator(stmt) == OPR_DO_WHILE || 
	WN_operator(stmt) == OPR_WHILE_DO) &&
       prev_stmt != NULL                   &&
       WN_operator(prev_stmt) == OPR_STID)
   {
      ST * const  st = WN_st(prev_stmt);
      ANL_SRCPOS  stid_srcpos(prev_stmt);
      ANL_SRCPOS  while_srcpos(stmt);
      NUM_ST_REFS loads_in_test(OPR_LDID, st);
      NUM_ST_REFS stores_in_body(OPR_STID, st);
      
      if (ST_sym_class(st) != CLASS_PREG                          &&
	  stid_srcpos >= while_srcpos                             &&
	  WN_TREE_walk_pre_order(WN_while_test(stmt), 
				 loads_in_test).num_nodes() == 1  &&
	  WN_TREE_walk_pre_order(WN_while_body(stmt), 
				 stores_in_body).num_nodes() >= 1)
      {
	 pred = TRUE;
      }
   }
   return pred;
} // W2CF_TRANSLATOR::Whileloop_Looks_Like_Forloop


void 
W2CF_TRANSLATOR::Expr_To_File(ANL_FILE_MNGR *file_mngr, WN *expr)
{
   if (_translate_to_c)
   {
      W2C_Push_PU(_pu, expr);
      W2C_Translate_Wn(file_mngr->File(), expr);
      W2C_Pop_PU();
   }
   else
   {
      W2F_Push_PU(_pu, expr);
      W2F_Translate_Wn(file_mngr->File(), expr);
      W2F_Pop_PU();
   }
} // W2CF_TRANSLATOR::Expr_To_File


void
W2CF_TRANSLATOR::Expr_To_String(ANL_CBUF *cbuf, WN *expr)
{
   if (_translate_to_c)
   {
      W2C_Push_PU(_pu, expr);
      W2C_Translate_Wn_Str(_strbuf, _Max_W2cf_String_Size, expr);
      W2C_Pop_PU();
   }
   else
   {
      W2F_Push_PU(_pu, expr);
      W2F_Translate_Wn_Str(_strbuf, _Max_W2cf_String_Size, expr);
      W2F_Pop_PU();
   }
   cbuf->Write_String(_strbuf);
} // W2CF_TRANSLATOR::Expr_To_String


void
W2CF_TRANSLATOR::Stid_Lhs_To_String(ANL_CBUF   *cbuf, 
				    ST         *st, 
				    STAB_OFFSET ofst,
				    TY_IDX      ty, 
				    MTYPE       mtype)
{
   if (_translate_to_c)
   {
      W2C_Push_PU(_pu, WN_func_body(_pu));
      W2C_Translate_Stid_Lhs(_strbuf, _Max_W2cf_String_Size,
			     st, ofst, ty, mtype);
      W2C_Pop_PU();
   }
   else
   {
      W2F_Push_PU(_pu, WN_func_body(_pu));
      W2F_Translate_Stid_Lhs(_strbuf, _Max_W2cf_String_Size,
			     st, ofst, ty, mtype);
      W2F_Pop_PU();
   }
   cbuf->Write_String(_strbuf);
} // W2CF_TRANSLATOR::Stid_Lhs_To_String


void 
W2CF_TRANSLATOR::Original_Symname_To_String(ANL_CBUF *cbuf, ST *st)
{
   // Do not use whirl2c.so or whirl2f.so, since they will not give correct
   // names for aliases.  Aliases will be resolved to unique names with
   // numeric suffixes by flist and clist, but we wish to avoid that here.
   //
   if (ST_name(st) == NULL)
      cbuf->Write_String("<????>");
   else if (_translate_to_c)
      cbuf->Write_String(ST_name(st));
   else
      _Get_Ftn_Name(cbuf, st);
} // W2CF_TRANSLATOR::Original_Symname_To_String


void 
W2CF_TRANSLATOR::Transformed_Symname_To_String(ANL_CBUF *cbuf, ST *st)
{
   // Express the names as whirl2f understand them
   //
   if (_translate_to_c)
   {
      W2C_Push_PU(_pu, WN_func_body(_pu));
      cbuf->Write_String(W2C_Object_Name(st));
      W2C_Pop_PU();
   }
   else
   {
      W2F_Push_PU(_pu, WN_func_body(_pu));
      cbuf->Write_String(W2F_Object_Name(st));
      W2F_Pop_PU();
   }
} // W2CF_TRANSLATOR::Transformed_Symname_To_String

void
W2CF_TRANSLATOR::A_Pragma_Expr_To_String(ANL_CBUF *cbuf, WN *apragma)
{
   if (_Is_Ptr_Expr(WN_kid0(apragma)))
   {
      TY_IDX pointed = _Get_Expr_Pointed_Ty(WN_kid0(apragma));
      TY_IDX pointer = Get_Pointer_To(pointed);
      _Istore_Lhs_To_String(cbuf, WN_kid0(apragma), 0 /*offset*/,
			    pointer, TY_mtype(pointed));
   }
   else
   {
      Expr_To_String(cbuf, WN_kid0(apragma));
   }
} // W2CF_TRANSLATOR::A_Pragma_Expr_To_String

void 
W2CF_TRANSLATOR::ClauseList_To_String(ANL_CBUF *cbuf, WN **clause_list)
{
   // PRECONDITION: clause == OPR_PRAGMA or OPR_XPRAGMA node.
   //
   // Translate the given sequence of clauses into C or Fortran 
   // (modelled after wn2f_pragma.c).
   //
   //
   WN  *this_clause;
   WN  *next_clause = *clause_list;

   _Skip_Ignored_Clauses(&next_clause);
   while (Is_A_Pragma_Clause(next_clause))
   {
      this_clause = next_clause;

      // Should have been skipped by _Skip_Ignored_Clauses()
      //
      Is_True(!WN_pragma_compiler_generated(next_clause),
	      ("Attempt to emit compiler generated clause in "
	       "W2CF_TRANSLATOR::ClauseList_To_String"));

      switch (WN_pragma(next_clause))
      {
      case WN_PRAGMA_DATA_AFFINITY:
      case WN_PRAGMA_THREAD_AFFINITY:
      case WN_PRAGMA_MPNUM:
      case WN_PRAGMA_SYNC_DOACROSS:
      case WN_PRAGMA_DEFAULT:
	 // Should have been skipped by _Skip_Ignored_Clauses().
	 //
	 Is_True(FALSE,
		 ("Unexpected case in "
		  "W2CF_TRANSLATOR::ClauseList_To_String"));
	 break;

      case WN_PRAGMA_NOWAIT:
	 cbuf->Write_String("nowait");
	 next_clause = WN_next(next_clause);
	 break;

      case WN_PRAGMA_AFFINITY:
	 cbuf->Write_String("affinity");
	 _Clause_Exprs_To_String(cbuf, WN_PRAGMA_AFFINITY, &next_clause);
	 cbuf->Write_String(" = ");
	 if (WN_pragma(next_clause) == WN_PRAGMA_DATA_AFFINITY)
	    cbuf->Write_String("data");
	 else if (WN_pragma(next_clause) == WN_PRAGMA_THREAD_AFFINITY)
	    cbuf->Write_String("thread");
	 else
	    cbuf->Write_String("<unknown_affinity??>");

	 // Process the expression associated with the thread/data affinity
	 // pragma.
	 //
	 cbuf->Write_String("\"(");
	 A_Pragma_Expr_To_String(cbuf, next_clause);
	 cbuf->Write_String(")\"");
	 next_clause = WN_next(next_clause);
	 break;

      case WN_PRAGMA_CHUNKSIZE:
	 cbuf->Write_String("chunk=");
	 _Clause_Exprs_To_String(cbuf, 
				 (WN_PRAGMA_ID)WN_pragma(next_clause),
				 &next_clause);
	 break;
	 
      case WN_PRAGMA_IF:
	 cbuf->Write_String("if");
	 _Clause_Exprs_To_String(cbuf, 
				 (WN_PRAGMA_ID)WN_pragma(next_clause),
				 &next_clause);
	 break;

      case WN_PRAGMA_LASTLOCAL:
	 cbuf->Write_String("lastlocal");
	 _Clause_Symbols_To_String(cbuf,
				   (WN_PRAGMA_ID)WN_pragma(next_clause),
				   &next_clause);
	 break;

      case WN_PRAGMA_LOCAL:
	 cbuf->Write_String("local");
	 if (WN_operator(next_clause) == OPR_XPRAGMA)
	 {
	    _Array_Segment_To_String(cbuf, 
				     (WN_PRAGMA_ID)WN_pragma(next_clause),
				     &next_clause);
	 }
	 else
	 {
	    _Clause_Symbols_To_String(cbuf,
				      (WN_PRAGMA_ID)WN_pragma(next_clause),
				      &next_clause);
	 }
	 break;

      case WN_PRAGMA_MPSCHEDTYPE:
	 /* Can be both a clause and a pragma */
	 cbuf->Write_String("mp_schedtype = ");
	 _Mp_Schedtype_To_String(cbuf, 
				 (WN_PRAGMA_SCHEDTYPE_KIND)WN_pragma_arg1(next_clause));
	 break;

      case WN_PRAGMA_ORDERED:
	 cbuf->Write_String("(ordered)");
	 break;

      case WN_PRAGMA_REDUCTION:
	 cbuf->Write_String("reduction");
	 if (WN_operator(next_clause) == OPR_XPRAGMA)
	    _Clause_Exprs_To_String(cbuf,
				    (WN_PRAGMA_ID)WN_pragma(next_clause),
				    &next_clause);
	 else
	    _Clause_Symbols_To_String(cbuf, 
				      (WN_PRAGMA_ID)WN_pragma(next_clause),
				      &next_clause);
	 break;

      case WN_PRAGMA_SHARED:
	 cbuf->Write_String("shared");
	 _Clause_Symbols_To_String(cbuf, 
				   (WN_PRAGMA_ID)WN_pragma(next_clause),
				   &next_clause);
	 break;

      case WN_PRAGMA_ONTO:
	 cbuf->Write_String("onto");
	 _Rev_Clause_Exprs_To_String(cbuf, 
				     (WN_PRAGMA_ID)WN_pragma(next_clause),
				     &next_clause);
	 break;

      case WN_PRAGMA_LASTTHREAD:
	 cbuf->Write_String("lastthread");
	 _Clause_Symbols_To_String(cbuf,
				   (WN_PRAGMA_ID)WN_pragma(next_clause), 
				   &next_clause);
	 break;

      case WN_PRAGMA_FIRSTPRIVATE:
	 cbuf->Write_String("firstprivate");
	 _Clause_Symbols_To_String(cbuf,
				   (WN_PRAGMA_ID)WN_pragma(next_clause), 
				   &next_clause);
	 break;

//       case WN_PRAGMA_DEFAULT:
//         the .anl file doesn't need the string just now.
//
//	 cbuf->Write_String("default");
//	 _Default_Kind_To_String(cbuf,
//				 (WN_PRAGMA_DEFAULT_KIND)WN_pragma_arg1(next_clause));
//	 break;

      default:
	 // This should never occur!
	 //
	 break;

      } // switch

      if (this_clause == next_clause)
	 next_clause = WN_next(this_clause); // Avoid inifinite loop

      _Skip_Ignored_Clauses(&next_clause);
      if (Is_A_Pragma_Clause(next_clause))
	 cbuf->Write_String(", "); // separate by commas
   } // while there are more pragma clauses

   *clause_list = next_clause; // Indicate how many clauses we have processed

} // W2CF_TRANSLATOR::ClauseList_To_String


void
W2CF_TRANSLATOR::Nest_Clauses_To_String(ANL_CBUF *cbuf, 
					WN       *nest_region, 
					INT       nest_levels)
{
   // We need to get index variables for "nest_levels" number
   // of DO loops, and emit the corresponding "nest" clause.
   //
   WN          *next_stmt = nest_region;
   WN_PRAGMA_ID nest_kind = WN_PRAGMA_UNDEFINED;

   if (next_stmt == NULL                        ||
       WN_operator(next_stmt) != OPR_REGION ||
       WN_first(WN_region_pragmas(next_stmt)) == NULL)
      Anl_Diag->Error("Unexpected do-nest in PROMPF!!");
   else
      nest_kind = 
	 (WN_PRAGMA_ID)WN_pragma(WN_first(WN_region_pragmas(next_stmt)));

   cbuf->Write_String("nest(");
   for (INT nest = 1; nest <= nest_levels; nest++)
   {
      if (WN_operator(next_stmt) != OPR_REGION       ||
	  WN_first(WN_region_pragmas(next_stmt)) == NULL ||
	  WN_pragma(WN_first(WN_region_pragmas(next_stmt))) != nest_kind)
	 Anl_Diag->Error("Unexpected do-nest in PROMPF!!");

      // Get the next nested loop, assuming next_stmt at this point
      // refers to a region.
      //
      next_stmt = WN_first(WN_region_body(next_stmt));
      while (WN_operator(next_stmt) != OPR_DO_LOOP)
	 next_stmt = WN_next(next_stmt);

      // Write out the index variable.
      //
      Original_Symname_To_String(cbuf, WN_st(WN_index(next_stmt)));

      // Emit separator, and search for the next nested region, if 
      // any is expected.
      //
      if (nest < nest_levels)
      {
	 cbuf->Write_Char(',');

	 next_stmt = WN_first(WN_do_body(next_stmt));
	 while (next_stmt != NULL && WN_operator(next_stmt) != OPR_REGION)
	    next_stmt = WN_next(next_stmt);

	 if (next_stmt == NULL                              ||
	     WN_operator(next_stmt) != OPR_REGION       ||
	     WN_first(WN_region_pragmas(next_stmt)) == NULL ||
	     WN_pragma(WN_first(WN_region_pragmas(next_stmt))) != nest_kind)
	    Anl_Diag->Error("Unexpected do-nest in PROMPF!!");
      }
   }
   cbuf->Write_Char(')');
} // W2CF_TRANSLATOR::ClauseList_To_String


void 
W2CF_TRANSLATOR::Prefetch_Attributes_To_String(ANL_CBUF    *cbuf, 
					       WN          *prefetch,
					       INT32        size)
{
   INT pflag = WN_prefetch_flag(prefetch);

   // Emit memory reference
   //
   cbuf->Write_Char('=');
   if (_Is_Ptr_Expr(WN_kid0(prefetch)))
   {
     TY_IDX pointed = _Get_Expr_Pointed_Ty(WN_kid0(prefetch));
     TY_IDX pointer = Get_Pointer_To(pointed);

      _Istore_Lhs_To_String(cbuf, WN_kid0(prefetch), 0 /*offset*/, 
			    pointer, TY_mtype(pointed));
   }
   else
      Expr_To_String(cbuf, WN_kid0(prefetch));

   // Emit stride and level clauses
   //
   cbuf->Write_Char(',');
   if (PF_GET_STRIDE_1L(pflag) > 0)
   {
      if (PF_GET_STRIDE_2L(pflag) > 0)
      {
	 cbuf->Write_String("stride=");
	 cbuf->Write_Int(PF_GET_STRIDE_1L(pflag));
	 cbuf->Write_Char(',');
	 cbuf->Write_Int(PF_GET_STRIDE_2L(pflag));
	 cbuf->Write_Char(',');
	 cbuf->Write_String("level=1,2");
      }
      else
      {
	 cbuf->Write_String("stride=");
	 cbuf->Write_Int(PF_GET_STRIDE_1L(pflag));
	 cbuf->Write_Char(',');
	 cbuf->Write_String("level=1");
      }
   }
   else if (PF_GET_STRIDE_2L(pflag) > 0)
   {
      cbuf->Write_String("stride=");
      cbuf->Write_Int(PF_GET_STRIDE_2L(pflag));
      cbuf->Write_Char(',');
      cbuf->Write_String("level=,2");
   }
   else
   {
      cbuf->Write_String("stride=,level=");
   }

   // Emit a kind clause
   //
   cbuf->Write_Char(',');
   if (PF_GET_READ(pflag))
      cbuf->Write_String("kind=rd");
   else
      cbuf->Write_String("kind=wr");

   // Emit a size clause
   //
   if (size > 0)
   {
      cbuf->Write_Char(',');
      cbuf->Write_String("size=");
      cbuf->Write_Int(size);
   }
} // W2CF_TRANSLATOR::Prefetch_Attributes_To_String

