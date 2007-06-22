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

#include "anl_common.h"
#include "anl_diagnostics.h"  // For warnings and errors
#include "anl_file_mngr.h"    // For managing files
#include "w2cf_translator.h"  // For translating WHIRL into high-level-language
#include "anl_func_entry.h"
#include "anl_pragma_attribute.h"

extern ANL_DIAGNOSTICS *Anl_Diag; // Defined in anl_driver.cxx


#define MAX_DISTRIBUTION_PRAGMAS 32


// =============== Class Utilities (hidden) ===============
// ========================================================

void
ANL_PRAGMA_ATTRIBUTE::_Append_Arg_Numbers(ANL_CBUF *cbuf,
					  INT32     val1,
					  INT32     val2)
{
   cbuf->Write_Char('(');
   if (val1 != -1)
      cbuf->Write_Int(val1);

   if (val2 != -1)
   {
      cbuf->Write_String(", ");
      cbuf->Write_Int(val2);
   }
   cbuf->Write_Char(')');
} /* ANL_PRAGMA_ATTRIBUTE::_Append_Arg_Numbers */


// =============== Class Utilities (public) ===============
// ========================================================

BOOL 
ANL_PRAGMA_ATTRIBUTE::Is_ProMpf_Pragma_Attribute(WN *stmt)
{
   BOOL predicate = (stmt != NULL && 
		     (WN_operator(stmt) == OPR_PRAGMA ||
		      WN_operator(stmt) == OPR_XPRAGMA));

   if (predicate)
      switch (WN_pragma(stmt))
      {
      case WN_PRAGMA_PREFETCH_REF:
	 predicate = (WN_next(stmt) != NULL && 
		      WN_operator(WN_next(stmt)) == OPR_PREFETCH);
	 break;
      case WN_PRAGMA_DISTRIBUTE:
      case WN_PRAGMA_REDISTRIBUTE:
      case WN_PRAGMA_DISTRIBUTE_RESHAPE:
	 predicate = (WN_pragma_index(stmt) == 0);
	 break;
      case WN_PRAGMA_DYNAMIC:
      case WN_PRAGMA_COPYIN:
      case WN_PRAGMA_NUMTHREADS:
      case WN_PRAGMA_PAGE_PLACE:
      case WN_PRAGMA_KAP_CONCURRENTIZE:
      case WN_PRAGMA_KAP_NOCONCURRENTIZE:
      case WN_PRAGMA_KAP_ASSERT_PERMUTATION:
      case WN_PRAGMA_CRI_CNCALL:
      case WN_PRAGMA_KAP_ASSERT_CONCURRENT_CALL:
      case WN_PRAGMA_KAP_ASSERT_DO:
      case WN_PRAGMA_KAP_ASSERT_DOPREFER:
      case WN_PRAGMA_IVDEP:
	 break;
      default:
	 predicate = FALSE;
	 break;
      }
   return predicate;
} // ANL_PRAGMA_ATTRIBUTE::Is_ProMpf_Pragma_attribute


// =============== Private Member Functions ===============
// ========================================================


BOOL 
ANL_PRAGMA_ATTRIBUTE::_Is_Assertion()
{
   BOOL predicate;

   switch(_pragma_kind)
   {
   case ANL_ASSERT_PERMUTATION:
   case ANL_ASSERT_CONCURRENT_CALL:
   case ANL_ASSERT_DO:
   case ANL_ASSERT_DOPREFER:
      predicate = TRUE;
      break;
   default:
      predicate = FALSE;
      break;
   }
   return predicate;   
} // ANL_PRAGMA_ATTRIBUTE::_Is_Assertion


void 
ANL_PRAGMA_ATTRIBUTE::_Write_Distribution(ANL_CBUF *cbuf, WN **next)
{
   // Write the symbol to be distributed and the distribution parameters,
   // then return the next statement, following the last distribution
   // parameter, which where any applicable clauses will begin.  The
   // *next parameter is only written in this routine.
   //
   INT32        num_dims;
   WN          *distr_pragma[MAX_DISTRIBUTION_PRAGMAS];
   WN          *distr_cyclic[MAX_DISTRIBUTION_PRAGMAS];
   WN          *distr_bound[MAX_DISTRIBUTION_PRAGMAS];
   WN_PRAGMA_ID id = (WN_PRAGMA_ID)WN_pragma(_apragma);

   if (WN_operator(_apragma) != OPR_PRAGMA) 
      cbuf->Write_String("<ERROR: unexpected opcode in _Write_Distribution>");

   // Write the distribution symbol to file
   //
   _func_entry->Pu_Translator()->
      Original_Symname_To_String(cbuf, WN_st(_apragma));

   // Determine the number of dimensions and accumulate the distribution
   // kind for each dimension.
   //
   WN *wn = _apragma; 
   for (num_dims = 0; 
	(wn != NULL                        &&
	 WN_operator(wn) == OPR_PRAGMA &&
	 WN_pragma(wn) == id               &&
	 num_dims == WN_pragma_index(wn));
	num_dims++)
   {
      distr_pragma[num_dims] = wn;
      if (WN_pragma_distr_type(wn) == DISTRIBUTE_CYCLIC_EXPR)
      {
	 wn = WN_next(wn);
         distr_cyclic[num_dims] = wn;
      }
      wn = WN_next(wn);
      distr_bound[num_dims] = wn;
      wn = WN_next(wn);
   }

   /* Skip two stores, which are generated purely for dependency analysis
    * purposes.
    */
   if (WN_operator(wn)==OPR_STID && ST_class(WN_st(wn))==CLASS_PREG)
   {
      wn = WN_next(wn);
      if (WN_operator(wn)==OPR_STID && ST_class(WN_st(wn))==CLASS_PREG)
         wn = WN_next(wn);
   }
   *next = wn;

   // Translate the sequence of distribution kinds, in Fortran order, i.e.
   // in reverse order from the given WHIRL representation.
   //
   cbuf->Write_Char('(');
   for (INT32 dim = num_dims-1; dim >= 0; dim--)
   {
      switch (WN_pragma_distr_type(distr_pragma[dim]))
      {
      case DISTRIBUTE_STAR:
	 cbuf->Write_Char('*');
         break;

      case DISTRIBUTE_BLOCK:
	 cbuf->Write_String("block");
         break;

      case DISTRIBUTE_CYCLIC_EXPR:
	 cbuf->Write_String("cyclic");
	 cbuf->Write_Char('(');
	 _func_entry->Pu_Translator()->
	    Expr_To_String(cbuf, WN_kid0(distr_cyclic[dim]));
	 cbuf->Write_Char(')');
         break;

      case DISTRIBUTE_CYCLIC_CONST:
	 cbuf->Write_String("cyclic");
	 _Append_Arg_Numbers(cbuf, WN_pragma_preg(distr_pragma[dim]), -1);
         break;

      default:
	 cbuf->Write_String("unknown_distribution");
         break;
      }

      if (dim > 0)
	 cbuf->Write_Char(',');

   } // For each dimension

   cbuf->Write_Char(')');

} // ANL_PRAGMA_ATTRIBUTE::_Write_Distribution


void
ANL_PRAGMA_ATTRIBUTE::_Write_Pragma_Arguments(ANL_CBUF *cbuf)
{
   // Write out a sequence of symbols or expressions, as specified by the
   // pragma, provided they where grouped together as one pragma in the
   // original source.
   //
   WN          *next = Next_Stmt();
   ANL_SRCPOS   pos(_apragma);
   WN_PRAGMA_ID id = (WN_PRAGMA_ID)WN_pragma(_apragma);

   cbuf->Write_Char('(');
   for (WN *wn = _apragma; wn != next; wn = WN_next(wn))
   {
      if (wn != _apragma)
	 cbuf->Write_Char(',');

      if (WN_operator(wn) == OPR_XPRAGMA)
      {
	 _func_entry->Pu_Translator()->A_Pragma_Expr_To_String(cbuf, wn);
      }
      else
      {
         // A common symbol?
	 //
	 if (ST_IS_COMMON_BLOCK(WN_st(wn)))
	    cbuf->Write_Char('/');

	 _func_entry->
            Pu_Translator()->Original_Symname_To_String(cbuf, WN_st(wn));

	 if (ST_IS_COMMON_BLOCK(WN_st(wn)))
	    cbuf->Write_Char('/');
      }
   }
   cbuf->Write_Char(')');
} // ANL_PRAGMA_ATTRIBUTE::_Write_Pragma_Arguments


void 
ANL_PRAGMA_ATTRIBUTE::_Write_Pragma(ANL_CBUF *cbuf)
{
   WN *next = _apragma;

   switch (_pragma_kind)
   {
   case ANL_PREFETCH_REF_ATTR:
      cbuf->Write_String("PREFETCH_REF \"");
      _func_entry->Pu_Translator()->
	 Prefetch_Attributes_To_String(cbuf, 
				       WN_next(_apragma),
				       WN_pragma_arg2(_apragma));
      cbuf->Write_String("\"");
      break;
   case ANL_DISTRIBUTE_ATTR:
      cbuf->Write_String("DISTRIBUTE \"");
      _Write_Distribution(cbuf, &next);
      cbuf->Write_Char(' ');
      _func_entry->Pu_Translator()->ClauseList_To_String(cbuf, &next);
      cbuf->Write_String("\"");
      break;
   case ANL_REDISTRIBUTE_ATTR:
      cbuf->Write_String("REDISTRIBUTE \"");
      _Write_Distribution(cbuf, &next);
      cbuf->Write_Char(' ');
      _func_entry->Pu_Translator()->ClauseList_To_String(cbuf, &next);
      cbuf->Write_String("\"");
      break;
   case ANL_DISTRIBUTE_RESHAPE_ATTR:
      cbuf->Write_String("DISTRIBUTE RESHAPE \"");
      _Write_Distribution(cbuf, &next);
      cbuf->Write_Char(' ');
      _func_entry->Pu_Translator()->ClauseList_To_String(cbuf, &next);
      cbuf->Write_String("\"");
      break;
   case ANL_DYNAMIC_ATTR:
      cbuf->Write_String("DYNAMIC \"");
      _func_entry->Pu_Translator()->
	 Original_Symname_To_String(cbuf, WN_st(_apragma));
      cbuf->Write_String("\"");
      break;
   case ANL_COPYIN_ATTR:
      cbuf->Write_String("COPYIN ");
      _Write_Pragma_Arguments(cbuf);
      break;
   case ANL_NUMTHREADS_ATTR:
      cbuf->Write_String("NUMTHREADS ");
      _Write_Pragma_Arguments(cbuf);
      break;
   case ANL_PAGE_PLACE_ATTR:
      cbuf->Write_String("PAGE_PLACE ");
      _Write_Pragma_Arguments(cbuf);
      break;
   case ANL_CONCURRENTIZE:
      cbuf->Write_String("CONCURRENTIZE");
      break;
   case ANL_NOCONCURRENTIZE:
      cbuf->Write_String("NO CONCURRENTIZE");
      break;
   case ANL_ASSERT_PERMUTATION:
      cbuf->Write_String("ASSERT PERMUTATION ");
      _Write_Pragma_Arguments(cbuf);
      break;
   case ANL_ASSERT_CONCURRENT_CALL:
      cbuf->Write_String("ASSERT CONCURRENT CALL");
      break;
   case ANL_ASSERT_DO:
      if (WN_pragma_arg1(next) == ASSERT_DO_CONCURRENT)
	 cbuf->Write_String("ASSERT DO (CONCURRENT)");
      else
	 cbuf->Write_String("ASSERT DO (SERIAL)");
      break;
   case ANL_ASSERT_DOPREFER:
      if (WN_pragma_arg1(next) == ASSERT_DO_CONCURRENT)
	 cbuf->Write_String("ASSERT DO PREFER (CONCURRENT)");
      else
	 cbuf->Write_String("ASSERT DO PREFER (SERIAL)");
      break;
   case ANL_IVDEP:
      cbuf->Write_String("IVDEP");
      break;
   default:
      cbuf->Write_String("UNKNOWN ");
      break;
   }
} // ANL_PRAGMA_ATTRIBUTE::_Write_Pragma


// =============== Public Member Functions ================
// ========================================================


ANL_PRAGMA_ATTRIBUTE::ANL_PRAGMA_ATTRIBUTE(WN             *apragma,
					   INT32    enclosing_construct_level,
					   ANL_FUNC_ENTRY *func_entry,
					   MEM_POOL       *pool):
   _apragma(apragma),
   _func_entry(func_entry),
   _enclosing_construct_level(enclosing_construct_level),
   _pool(pool)
{
   if (WN_operator(apragma) == OPR_PRAGMA ||
       WN_operator(apragma) == OPR_XPRAGMA)
   {
      switch (WN_pragma(apragma))
      {
      case WN_PRAGMA_PREFETCH_REF:
	 _pragma_kind = ANL_PREFETCH_REF_ATTR;
	 break;
      case WN_PRAGMA_DISTRIBUTE:
	 _pragma_kind = ANL_DISTRIBUTE_ATTR;
	 break;
      case WN_PRAGMA_REDISTRIBUTE:
	 _pragma_kind = ANL_REDISTRIBUTE_ATTR;
	 break;
      case WN_PRAGMA_DISTRIBUTE_RESHAPE:
	 _pragma_kind = ANL_DISTRIBUTE_RESHAPE_ATTR;
	 break;
      case WN_PRAGMA_DYNAMIC:
	 _pragma_kind = ANL_DYNAMIC_ATTR;
	 break;
      case WN_PRAGMA_COPYIN:
	 _pragma_kind = ANL_COPYIN_ATTR;
	 break;
      case WN_PRAGMA_NUMTHREADS:
	 _pragma_kind = ANL_NUMTHREADS_ATTR;
	 break;
      case WN_PRAGMA_PAGE_PLACE:
	 _pragma_kind = ANL_PAGE_PLACE_ATTR;
	 break;
      case WN_PRAGMA_KAP_CONCURRENTIZE:
	 _pragma_kind = ANL_CONCURRENTIZE;
	 break;
      case WN_PRAGMA_KAP_NOCONCURRENTIZE:
	 _pragma_kind = ANL_NOCONCURRENTIZE;
	 break;
      case WN_PRAGMA_KAP_ASSERT_PERMUTATION:
	 _pragma_kind = ANL_ASSERT_PERMUTATION;
	 break;
      case WN_PRAGMA_CRI_CNCALL:
      case WN_PRAGMA_KAP_ASSERT_CONCURRENT_CALL:
	 _pragma_kind = ANL_ASSERT_CONCURRENT_CALL;
	 break;
      case WN_PRAGMA_KAP_ASSERT_DO:
	 _pragma_kind = ANL_ASSERT_DO;
	 break;
      case WN_PRAGMA_KAP_ASSERT_DOPREFER:
	 _pragma_kind = ANL_ASSERT_DOPREFER;
	 break;
      case WN_PRAGMA_IVDEP:
	 _pragma_kind = ANL_IVDEP;
	 break;
      default:
	 _pragma_kind = ANL_UNKNOWN_ATTR;
	 break;
      }
   }
   else
   {
      _pragma_kind = ANL_UNKNOWN_ATTR;
   }
} // ANL_PRAGMA_ATTRIBUTE::ANL_PRAGMA_ATTRIBUTE


BOOL
ANL_PRAGMA_ATTRIBUTE::Is_Pragma_Construct_Attribute(INT32 construct_level)
{
   BOOL predicate;
   
   switch (_pragma_kind)
   {
   case ANL_DISTRIBUTE_ATTR:
   case ANL_DISTRIBUTE_RESHAPE_ATTR:
   case ANL_DYNAMIC_ATTR:
   case ANL_CONCURRENTIZE:
   case ANL_NOCONCURRENTIZE:
   case ANL_ASSERT_PERMUTATION:
   case ANL_ASSERT_CONCURRENT_CALL:
   case ANL_ASSERT_DO:
   case ANL_ASSERT_DOPREFER:
   case ANL_IVDEP:
   case ANL_UNKNOWN_ATTR:
      predicate = FALSE;
      break;
   default:
      // True if the directive is nested within the construct.
      //
      predicate = (construct_level <= _enclosing_construct_level);
      break;
   }
   return predicate;
} // ANL_PRAGMA_ATTRIBUTE::Is_Pragma_Construct_Attribute


BOOL 
ANL_PRAGMA_ATTRIBUTE::Is_Region_Construct_Attribute(INT32 construct_level)
{
   return Is_Pragma_Construct_Attribute(construct_level);
} // ANL_PRAGMA_ATTRIBUTE::Is_Region_Construct_Attribute


BOOL 
ANL_PRAGMA_ATTRIBUTE::Is_Loop_Construct_Attribute(INT32 construct_level)
{
   BOOL predicate;
   
   switch (_pragma_kind)
   {
   case ANL_CONCURRENTIZE:
   case ANL_NOCONCURRENTIZE:
   case ANL_ASSERT_PERMUTATION:
      predicate = FALSE;
      break;
   case ANL_ASSERT_CONCURRENT_CALL:
   case ANL_ASSERT_DO:
   case ANL_ASSERT_DOPREFER:
   case ANL_IVDEP:
      predicate = (construct_level == _enclosing_construct_level+1);
      break;
   default:
      predicate = Is_Pragma_Construct_Attribute(construct_level);
      break;
   }
   return predicate;
} // ANL_PRAGMA_ATTRIBUTE::Is_Loop_Construct_Attribute


WN *
ANL_PRAGMA_ATTRIBUTE::Next_Stmt()
{
   ANL_SRCPOS   pos(_apragma);
   WN_PRAGMA_ID id = (WN_PRAGMA_ID)WN_pragma(_apragma);
   WN          *next = WN_next(_apragma);
   INT32        num_dims;

   switch (_pragma_kind)
   {
   case ANL_COPYIN_ATTR:
   case ANL_NUMTHREADS_ATTR:
   case ANL_PAGE_PLACE_ATTR:
   case ANL_CONCURRENTIZE:
   case ANL_NOCONCURRENTIZE:
   case ANL_ASSERT_PERMUTATION:
   case ANL_ASSERT_CONCURRENT_CALL:
   case ANL_ASSERT_DO:
   case ANL_ASSERT_DOPREFER:
   case ANL_IVDEP:
      while (next != NULL                          &&
	     (WN_operator(next) == OPR_XPRAGMA || 
	      WN_operator(next) == OPR_PRAGMA) &&
	     WN_pragma(next) == id                 &&
	     ANL_SRCPOS(next).Linenum() == pos.Linenum())
      {
	 next = WN_next(next);
      }
      break;
   case ANL_DISTRIBUTE_ATTR:
   case ANL_REDISTRIBUTE_ATTR:
   case ANL_DISTRIBUTE_RESHAPE_ATTR:
      // See _Write_Distribution for details on what we need to do
      // here.
      //
      next = _apragma;
      for (num_dims = 0; 
	   (next != NULL                        &&
	    WN_operator(next) == OPR_PRAGMA &&
	    WN_pragma(next) == id               &&
	    num_dims == WN_pragma_index(next));
	   num_dims++)
      {
	 if (WN_pragma_distr_type(next) == DISTRIBUTE_CYCLIC_EXPR)
	    next = WN_next(next);
	 next = WN_next(next); // skip this OPR_PRAGMA
	 next = WN_next(next); // skip the following OPR_XPRAGMA
      }

      /* Skip two stores, which are generated purely for dependency analysis
       * purposes.
       */
      if (WN_operator(next)==OPR_STID && ST_class(WN_st(next))==CLASS_PREG)
      {
	 next = WN_next(next);
	 if (WN_operator(next)==OPR_STID && 
	     ST_class(WN_st(next))==CLASS_PREG)
	    next = WN_next(next);
      }
      break;
   default:
      break;
   }
   while (next != NULL && 
	  _func_entry->Pu_Translator()->Is_A_Pragma_Clause(next))
   {
      next = WN_next(next);
   }

   return next;
} // ANL_PRAGMA_ATTRIBUTE::Next_Stmt


void
ANL_PRAGMA_ATTRIBUTE::Write(ANL_CBUF *cbuf, INT32 id)
{
   if (_pragma_kind != ANL_UNKNOWN_ATTR)
   {
      WN         *next_stmt = _apragma;
      ANL_SRCPOS  begin_pos;
      ANL_SRCPOS  end_pos;

      // Some preliminiaries.
      //
      _func_entry->Get_Pragma_Srcpos_Range(_apragma, &begin_pos, &end_pos);

      // The dir clause.
      //
      if (_Is_Assertion())
	 cbuf->Write_String("asrtn ");
      else
	 cbuf->Write_String("dir ");
      cbuf->Write_Int(id);
      cbuf->Write_Char(' ');
      begin_pos.Write(cbuf);
      cbuf->Write_Char('-');
      end_pos.Write(cbuf);
      cbuf->Write_Char(' ');
      _Write_Pragma(cbuf);
      cbuf->Write_String("\n");
   }
} // ANL_PRAGMA_ATTRIBUTE::Write

