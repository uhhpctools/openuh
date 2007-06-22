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
#include "anl_varlist.h"      // For emitting attributes of symbol references
#include "anl_pragma_attribute.h" // For <dir> entries
#include "anl_func_entry.h"
#include "anl_pragma_construct.h"

extern ANL_DIAGNOSTICS *Anl_Diag; // Defined in anl_driver.cxx


// ================ Class Utility Functions ===============
// ========================================================

BOOL 
ANL_PRAGMA_CONSTRUCT::Is_ProMpf_Pragma_Construct(WN *stmt)
{
   BOOL predicate = (stmt != NULL && 
		     (WN_operator(stmt) == OPR_PRAGMA ||
		      WN_operator(stmt) == OPR_XPRAGMA));

   if (predicate)
      switch (WN_pragma(stmt))
      {

      case WN_PRAGMA_SECTION:
      case WN_PRAGMA_BARRIER:
      case WN_PRAGMA_CRITICAL_SECTION_BEGIN:
      case WN_PRAGMA_ORDERED_BEGIN:
      case WN_PRAGMA_ATOMIC:
	 break;
	 
      default:
	 predicate = FALSE;
	 break;
      }
   return predicate;
} // ANL_PRAGMA_CONSTRUCT::Is_ProMpf_Pragma_Construct


BOOL 
ANL_PRAGMA_CONSTRUCT::Is_Valid_Dir(ANL_PRAGMA_ATTRIBUTE *dir,
				   INT32                 construct_level)
{
   return dir->Is_Pragma_Construct_Attribute(construct_level);
}


// =============== Private Member Functions ===============
// ========================================================


// Is there an end of construct marker ie: a pragma, available?
// if called with a context of 'for_id', it implies _pragma_end
// may be given an ID & must be an end of construct marker to get
// a T result. If it's just for evaluating a line number, then 
// for_id is FALSE and T may be returned on the grounds _pragma_end 
// is just a line marker. for_id matters only for ATOMIC, just now.

BOOL
ANL_PRAGMA_CONSTRUCT::_End_Is_Part_Of_Construct(BOOL for_id)
{
   // PRECONDITION: Must have done _Find_Pragma_End_Stmt()
   //
   BOOL predicate;
   
   if (_pragma_end != NULL)
   {
      switch (_pragma_kind)
      {
      case ANL_SECTION_PRAGMA:
	 predicate = FALSE;
	 break;

      case ANL_BARRIER_PRAGMA:
	 predicate = TRUE;  // _pragma_end == _pragma_begin
	 break;

      case ANL_ATOMIC_PRAGMA:
	 predicate = !for_id;
	 break;

      case ANL_CRITICAL_SECTION_PRAGMA:
	 predicate = 
	    (WN_pragma(_pragma_end) == WN_PRAGMA_CRITICAL_SECTION_END);
	 break;

      case ANL_ORDERED_PRAGMA:
	 predicate = 
	    (WN_pragma(_pragma_end) == WN_PRAGMA_ORDERED_END);
	 break;

      default:
	 predicate = FALSE;
	 break;
      }
   }
   else // (_pragma_end == NULL)
   {
      predicate = FALSE;
   }
   return predicate;
} // ANL_PRAGMA_CONSTRUCT::_End_Is_Part_Of_Construct


void
ANL_PRAGMA_CONSTRUCT::_Find_Pragma_End_Stmt()
{
   // We allow two ways of ending a pragma delimited region
   // of statements.  This function finds any such statement, returns
   // "pragma_begin" if not applicable (e.g. for BARRIER), and NULL if
   // none is found.  The exact values returned here have consequences
   // for the algorithms used elsewhere (w.g. for _Get_Max_Construct_Srcpos).
   //
   WN_PRAGMA_ID end_id[2] = {WN_PRAGMA_UNDEFINED, 
			     WN_PRAGMA_UNDEFINED};

   BOOL search_loop =  (_pragma_end != _pragma_begin) ;

   switch (_pragma_kind)
   {

   case ANL_SECTION_PRAGMA:
      end_id[0] = WN_PRAGMA_SECTION;
      end_id[1] = WN_PRAGMA_PSECTION_END;
      break;

   case ANL_BARRIER_PRAGMA:
      _pragma_end = _pragma_begin; // Does not bracket any statements.
      search_loop = FALSE;
      break;

   case ANL_ATOMIC_PRAGMA:
      _pragma_end = WN_next(_pragma_begin);
      _pr_end_part_of_body = TRUE;
      search_loop = FALSE;
      break ;

   case ANL_CRITICAL_SECTION_PRAGMA:
      end_id[0] = WN_PRAGMA_CRITICAL_SECTION_END;
      break;

   case ANL_ORDERED_PRAGMA:
      end_id[0] = WN_PRAGMA_ORDERED_END;
      break;

   default:
      Anl_Diag->Error("Unexpected pragma in _Find_Pragma_End_Stmt()!!");
      break;
   }

   if (search_loop)
     {
       // Search for a pragma that matches the expected END pragma,
       // or end up with (pragma_end == NULL) when none is found.
       //
       WN *stmt;

       for (stmt = WN_next(_pragma_begin);
	    (stmt != NULL &&
	     !((WN_operator(stmt) == OPR_PRAGMA ||
		WN_operator(stmt) == OPR_XPRAGMA) &&
	       (WN_pragma(stmt) == end_id[0] ||
		WN_pragma(stmt) == end_id[1])));
	    stmt = WN_next(stmt));
       _pragma_end = stmt;
     } 
} // ANL_PRAGMA_CONSTRUCT::_Find_Pragma_End_Stmt

   
void 
ANL_PRAGMA_CONSTRUCT::_Get_Max_Construct_Srcpos(ANL_SRCPOS *max)
{
   ANL_SRCPOS min(_pragma_begin);

   *max = ANL_SRCPOS(_pragma_begin);
   for (WN *stmt = _pragma_begin; 
	stmt != _pragma_end;
	stmt = WN_next(stmt))
   {
      Adjust_Srcpos_Range(stmt, &min, max);
   }

   if (_End_Is_Part_Of_Construct(FALSE))
      Adjust_Srcpos_Range(_pragma_end, &min, max);
} // ANL_PRAGMA_CONSTRUCT::_Get_Max_Construct_Srcpos


void 
ANL_PRAGMA_CONSTRUCT::_Write_Pragma_Directives(ANL_CBUF *cbuf)
{
   WN         *next_stmt = WN_next(_pragma_begin);
   ANL_CBUF    clause_list(_pool);
   ANL_SRCPOS  begin_pos;
   ANL_SRCPOS  begin_extent;
   ANL_SRCPOS  end_pos;

   // Some preliminiaries.
   //
   _Get_Max_Construct_Srcpos(&end_pos);
   _func_entry->Get_Pragma_Srcpos_Range(_pragma_begin,
					&begin_pos, &begin_extent);

   clause_list.Write_Char(' ');
   _func_entry->Pu_Translator()->
      ClauseList_To_String(&clause_list, &next_stmt);

   // The idir clause.
   //
   cbuf->Write_String("idir ");
   cbuf->Write_Int(_id);
   cbuf->Write_Char(' ');
   begin_pos.Write(cbuf);
   cbuf->Write_Char('-');
   begin_extent.Write(cbuf);
   
   cbuf->Append_Pragma_Preamble((BOOL) WN_pragma_omp(_pragma_begin),FALSE);

   switch (_pragma_kind)
   {
   case ANL_SECTION_PRAGMA:
      cbuf->Write_String("SECTION ");
      break;
   case ANL_BARRIER_PRAGMA:
      cbuf->Write_String("BARRIER ");
      break;
   case ANL_CRITICAL_SECTION_PRAGMA:
      cbuf->Write_String("CRITICAL SECTION ");
      if (WN_operator(_pragma_begin) == OPR_XPRAGMA)
	{
	 cbuf->Write_Char('(');
	 _func_entry->Pu_Translator()->
	    A_Pragma_Expr_To_String(cbuf, _pragma_begin);
	 cbuf->Write_Char(')');
        } 
      else if (WN_st(_pragma_begin) != NULL)
	{
	  /* 
           * In OMP code the pragma has a string const ST
           * whose value is the name of the locking varbl.
           */

	  ST * st = WN_st( _pragma_begin);
	  if (ST_class(st) == CLASS_CONST)
	    {
	     cbuf->Write_Char('(');
	     cbuf->Write_String(Targ_String_Address(STC_val(st)));
	     cbuf->Write_Char(')');
	    }
	}
      cbuf->Write_Char(' ');
      break;
    case ANL_ORDERED_PRAGMA:
      cbuf->Write_String("ORDERED ");
      break;
    case ANL_ATOMIC_PRAGMA:
      cbuf->Write_String("ATOMIC ");
      break;
   default:
      cbuf->Write_String("<WHATPRAGMA??> ");
      break;
   }
   cbuf->Write_String(clause_list.Chars());
   cbuf->Write_String("\n");

   if (_pragma_end != _pragma_begin && 
       _End_Is_Part_Of_Construct(TRUE) &&
       !_func_entry->Pu_Translator()->Language_is_C())
   {
      cbuf->Write_String("edir ");
      cbuf->Write_Int(_id);
      cbuf->Write_String(" ");
      end_pos.Write(cbuf);

      cbuf->Append_Pragma_Preamble((BOOL) WN_pragma_omp(_pragma_begin),FALSE);

      switch (_pragma_kind)
      {
      case ANL_CRITICAL_SECTION_PRAGMA:
	 cbuf->Write_String("END CRITICAL SECTION\n");
	 break;
      case ANL_ORDERED_PRAGMA:
	 cbuf->Write_String("END_ORDERED\n");
	 break;
      case ANL_ATOMIC_PRAGMA:
	 cbuf->Write_String("END ATOMIC\n");
	 break;

      default:
	 cbuf->Write_String("<WHAT_PRAGMA_END??>\n");
	 break;
      }
   }
} // ANL_PRAGMA_CONSTRUCT::_Write_Pragma_Directives


// =============== Public Member Functions ================
// ========================================================


ANL_PRAGMA_CONSTRUCT::ANL_PRAGMA_CONSTRUCT(WN             *pragma_begin,
					   INT32           construct_level,
					   ANL_FUNC_ENTRY *func_entry,
					   MEM_POOL       *pool):
   _pragma_begin(pragma_begin),
   _construct_level(construct_level),
   _pragma_end((WN *)-1),  // To catch errors
   _pr_end_part_of_body(FALSE),
   _func_entry(func_entry),
   _pool(pool)
{
  _is_omp = FALSE ;

   if (WN_operator(pragma_begin) == OPR_PRAGMA ||
       WN_operator(pragma_begin) == OPR_XPRAGMA)
   {
      _is_omp = WN_pragma_omp(pragma_begin);

      switch (WN_pragma(pragma_begin))
      {
      case WN_PRAGMA_SECTION:
	 _pragma_kind = ANL_SECTION_PRAGMA;
	 break;
      case WN_PRAGMA_BARRIER:
	 _pragma_kind = ANL_BARRIER_PRAGMA;
	 break;
      case WN_PRAGMA_CRITICAL_SECTION_BEGIN:
	 _pragma_kind = ANL_CRITICAL_SECTION_PRAGMA;
	 break;
      case WN_PRAGMA_ORDERED_BEGIN:
	 _pragma_kind = ANL_ORDERED_PRAGMA;
	 break;
      case WN_PRAGMA_ATOMIC:
	 _pragma_kind = ANL_ATOMIC_PRAGMA;
	 break;
      default:
	 _pragma_kind = ANL_UNKNOWN_PRAGMA;
	 break;
      }
      _id = _func_entry->Next_Construct_Id()->Post_Incr();
      func_entry->Set_Construct_Id(pragma_begin, _id);

      _Find_Pragma_End_Stmt();
      if (_End_Is_Part_Of_Construct(TRUE))
	 func_entry->Set_Construct_Id(_pragma_end, _id);
   }
   else
   {
      _pragma_kind = ANL_UNKNOWN_PRAGMA;
      _id = -1;
   }
} // ANL_PRAGMA_CONSTRUCT::ANL_PRAGMA_CONSTRUCT


WN *
ANL_PRAGMA_CONSTRUCT::Next_Stmt()
{
   WN *next;

   if (_End_Is_Part_Of_Construct(TRUE))
      next = WN_next(_pragma_end);
   else
      next = _pragma_end;
   return next;
} // ANL_PRAGMA_CONSTRUCT::Next_Stmt


void
ANL_PRAGMA_CONSTRUCT::Write(ANL_CBUF *cbuf)
{
   if (_pragma_kind != ANL_UNKNOWN_PRAGMA)
   {
      ANL_CBUF    varlist_cbuf(_pool);
      ANL_CBUF    nested_cbuf(_pool);
      ANL_SRCPOS  min_srcpos(_pragma_begin);
      ANL_SRCPOS  max_srcpos;
      ANL_VARLIST varlist(_pool, _func_entry);
      WN         *stmt;

      // Write out the pragma construct header
      //
      _Get_Max_Construct_Srcpos(&max_srcpos);
      cbuf->Append_Pragma_Preamble(_is_omp,TRUE);

      switch (_pragma_kind)
      {
      case ANL_SECTION_PRAGMA:
	 cbuf->Write_String("section ");
	 cbuf->Write_Int(_id);
	 cbuf->Write_String(" \"parallel subsection\"");
	 break;
      case ANL_BARRIER_PRAGMA:
	 cbuf->Write_String("barrier ");
	 cbuf->Write_Int(_id);
	 cbuf->Write_String(" \"barrier\"");
	 break;
      case ANL_CRITICAL_SECTION_PRAGMA:
	 cbuf->Write_String("critsect ");
	 cbuf->Write_Int(_id);
	 cbuf->Write_String(" \"");
	 if (WN_operator(_pragma_begin) == OPR_XPRAGMA)
	 {
	    _func_entry->Pu_Translator()->
	       A_Pragma_Expr_To_String(cbuf, _pragma_begin);
	 }
	 cbuf->Write_Char('\"');
	 break;
      case ANL_ORDERED_PRAGMA:
	 cbuf->Write_String("ordered ");
	 cbuf->Write_Int(_id);
	 cbuf->Write_String(" \"sequential section\"");
	 break;
      case ANL_ATOMIC_PRAGMA:
	 cbuf->Write_String("atomic ");
	 cbuf->Write_Int(_id);
	 cbuf->Write_String(" \"atomic store\"");
	 break;
      default:
	 cbuf->Write_String("<WHATPRAGMA??> ");
	 cbuf->Write_Int(_id);
	 cbuf->Write_String(" \"???\"");
	 break;
      }
      cbuf->Write_String(" range ");
      min_srcpos.Write(cbuf);
      cbuf->Write_Char('-');
      max_srcpos.Write(cbuf);
      cbuf->Write_Char('\n');

      // Write out the region directives
      //
      _Write_Pragma_Directives(cbuf);

      // Determine variable references within the statements belonging to the
      // loop, and write them out to a temporary buffer. Sometimes (atomic)
      // need to look at _pragma_end too.

      for (stmt = _pragma_begin; stmt != _pragma_end; stmt = WN_next(stmt))
      {
	 varlist.Insert_Var_Refs(stmt);
      }
      if (_pr_end_part_of_body)
	 varlist.Insert_Var_Refs(stmt);

      varlist.Write(&varlist_cbuf, _id);
      varlist_cbuf.Write_Char('\n');

      // Write nested constructs to temporary buffer
      //
      if (_End_Is_Part_Of_Construct(FALSE))
	 stmt = _pragma_end;
      else
	 for (stmt = _pragma_begin; 
	      WN_next(stmt) != _pragma_end; 
	      stmt = WN_next(stmt));
      _func_entry->Emit_Nested_Original_Constructs(&nested_cbuf,
						   WN_next(_pragma_begin),
						   stmt);

      // Write out any applicable <dir> entries (i.e. those that were
      // not attributed to nested constructs).
      //
      _func_entry->
	 Emit_Dir_Entries(cbuf, _id, _construct_level,
			  &ANL_PRAGMA_CONSTRUCT::Is_Valid_Dir);

      // Write the varlist and nested constructs.
      //
      if (varlist_cbuf.Size() > 0)
	 cbuf->Write_String(varlist_cbuf.Chars());
      if (nested_cbuf.Size() > 0)
	 cbuf->Write_String(nested_cbuf.Chars());

      // Finish writing the construct descriptor
      //
      cbuf->Append_Pragma_Preamble(_is_omp,TRUE);
      switch (_pragma_kind)
      {
      case ANL_SECTION_PRAGMA:
	 cbuf->Write_String("end_section ");
	 break;
      case ANL_BARRIER_PRAGMA:
	 cbuf->Write_String("end_barrier ");
	 break;
      case ANL_CRITICAL_SECTION_PRAGMA:
	 cbuf->Write_String("end_critsect ");
	 break;
      case ANL_ORDERED_PRAGMA:
	 cbuf->Write_String("end_ordered ");
	 break;
      case ANL_ATOMIC_PRAGMA:
	 cbuf->Write_String("end_atomic ");
	 break;
      default:
	 cbuf->Write_String("<ENDWHATPRAGMA??> \"???\" ");
	 break;
      }
      cbuf->Write_Int(_id);
      cbuf->Write_String("\n"); // Start next construct on a new line
   }
} // ANL_PRAGMA_CONSTRUCT::Write

