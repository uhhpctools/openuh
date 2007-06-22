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
#include "anl_diagnostics.h"  // For warnings and errors
#include "anl_file_mngr.h"    // For managing files
#include "w2cf_translator.h"  // For translating WHIRL into high-level-language
#include "anl_varlist.h"      // For emitting attributes of symbol references
#include "anl_pragma_attribute.h" // For <dir> entries
#include "anl_func_entry.h"
#include "anl_loop_construct.h"

extern ANL_DIAGNOSTICS *Anl_Diag; // Defined in anl_driver.cxx


// =============== Static Helper Functions ===============
// =======================================================

#define WN_current_loop_nest(wn) WN_pragma_arg1(wn)
#define WN_max_loop_nest(wn) WN_pragma_arg2(wn)


// ================ Static Member Functions ===============
// ========================================================

BOOL 
ANL_LOOP_CONSTRUCT::Is_Valid_Dir(ANL_PRAGMA_ATTRIBUTE *dir,
				 INT32                 construct_level)
{
   return dir->Is_Loop_Construct_Attribute(construct_level);
}

BOOL 
ANL_LOOP_CONSTRUCT::Is_End_Of_Loop_Comment(WN *end_stmt)
{
   return (WN_operator(end_stmt) == OPR_COMMENT &&
	   strcmp(Index_To_Str(WN_GetComment(end_stmt)), "ENDLOOP") == 0);
}

void
ANL_LOOP_CONSTRUCT::Remove_Stmt_In_Block(WN *stmt)
{
   WN *block = LWN_Get_Parent(stmt);

   while (block != NULL && WN_operator(block) != OPR_BLOCK)
      block = LWN_Get_Parent(block);
   
   if (WN_operator(block) == OPR_BLOCK)
   {
      WN_DELETE_FromBlock(block, stmt);
   }
}


// =============== Private Member Functions ===============
// ========================================================

WN *
ANL_LOOP_CONSTRUCT::_First_Loop_Stmt()
{
   WN        *base = (_Is_Parallel_Loop()? _loop_region : _loop);
   ANL_SRCPOS basepos(base);

   while (WN_prev(base) != NULL && ANL_SRCPOS(WN_prev(base)) >= basepos)
      base = WN_prev(base);
   return base;
} // ANL_LOOP_CONSTRUCT::_First_Loop_Stmt


WN *
ANL_LOOP_CONSTRUCT::_Last_Loop_Stmt()
{
   WN        *base = (_Is_Parallel_Loop()? _loop_region : _loop);
   ANL_SRCPOS basepos(base);

   while (WN_next(base) != NULL && ANL_SRCPOS(WN_next(base)) <= basepos)
      base = WN_next(base);
   return base;
} //ANL_LOOP_CONSTRUCT::_Last_Loop_Stmt


void 
ANL_LOOP_CONSTRUCT::_Loop_Srcpos_Range(ANL_SRCPOS *min, ANL_SRCPOS *max)
{
   // Get the source-position range for the loop, including associated
   // pragmas and loop initialization statements (which may have been moved
   // in front of the loop by the compiler front-end).
   //
   WN        *end_stmt = NULL;
   WN        *end_comment = NULL;
   WN        *base = (_Is_Parallel_Loop()? _loop_region : _loop);
   WN        *first_stmt = _First_Loop_Stmt();
   WN        *last_stmt = _Last_Loop_Stmt();
   ANL_SRCPOS minpos(first_stmt);
   ANL_SRCPOS maxpos(last_stmt);

   // Start out with the top-level stmts (at same nesting level as the
   // region or loop) as a first approximation, looking only at the stmts
   // within the loopor region.
   //
   for (WN *stmt = first_stmt; 
	stmt != NULL && WN_prev(stmt) != last_stmt;
	stmt = WN_next(stmt))
   {
      Adjust_Srcpos_Range(stmt, &minpos, &maxpos);
   }

   // See if the maxpos we got from the above analysis is accurate,
   // or whether we should adjust it to extend up to the statement 
   // following this construct.
   //
   if (_Is_Parallel_Loop())
   {
      // Look for special pragma to end the loop.
      //
      for (end_stmt = WN_first(WN_region_pragmas(_loop_region));
           (end_stmt != NULL && 
	    WN_pragma(end_stmt) != WN_PRAGMA_PDO_END    &&
	    WN_pragma(end_stmt) != WN_PRAGMA_END_MARKER &&
	    (WN_pragma(end_stmt) != WN_PRAGMA_NOWAIT ||
	     _func_entry->Pu_Translator()->Language_is_C()));
           end_stmt = WN_next(end_stmt));
   }

   // Look for special comment to end the loop.  We always need to do this
   // such that we can remove it from WHIRL.
   //
   if (_Is_Parallel_Loop())
   {
      // Get to the do-loop
      //
      for (end_comment = WN_first(WN_region_body(_loop_region));
	   (end_comment != NULL && WN_operator(end_comment) != OPR_DO_LOOP);
	   end_comment = WN_next(end_comment));

      // Look for the special loop terminating comment within the body of
      // the loop.
      //
      if (end_comment != NULL)
	 for (end_comment = WN_first(WN_do_body(end_comment));
	      (end_comment != NULL && !Is_End_Of_Loop_Comment(end_comment));
	      end_comment = WN_next(end_comment));
   }
   else
   {
      WN *const body = (_loop_kind == ANL_DO_LOOP? 
			WN_do_body(_loop) : WN_while_body(_loop));
	 
      // Look for a special loop terminating comment in the body of
      // the loop.
      //
      for (end_comment = WN_first(body);
	   (end_comment != NULL && !Is_End_Of_Loop_Comment(end_comment));
	   end_comment = WN_next(end_comment));
   }
   if (end_comment != NULL)
   {
      if (end_stmt == NULL)
	 end_stmt = end_comment;
      Remove_Stmt_In_Block(end_comment);
   }

   // Adjust the max position, when there is no true indication of
   // the end of the construct.
   //
   if (end_stmt == NULL && WN_next(last_stmt) != NULL)
   {
      // Assume the end is just before the next statement.
      //
      ANL_SRCPOS next_min(WN_next(last_stmt));
      ANL_SRCPOS next_max(WN_next(last_stmt));

      Adjust_Srcpos_Range(WN_next(last_stmt), &next_min, &next_max);
      if (next_min > maxpos) // FALSE when the file-numbers are different!
      {
	 next_min -= 1;
	 maxpos = next_min;
      }
   }

   Is_True(end_stmt == NULL || maxpos == ANL_SRCPOS(end_stmt), 
	   ("Unexpected maxpos (maxpos=[%d,%d,%d],end_stmt_pos=[%d,%d,%d]) in "
	    "ANL_LOOP_CONSTRUCT::_Loop_Srcpos_Range",
	    maxpos.Filenum(), maxpos.Linenum(), maxpos.Column(),
	    ANL_SRCPOS(end_stmt).Filenum(),
	    ANL_SRCPOS(end_stmt).Linenum(),
	    ANL_SRCPOS(end_stmt).Column()));

   *min = minpos;
   *max = maxpos;
} // ANL_LOOP_CONSTRUCT::_Loop_Srcpos_Range


void
ANL_LOOP_CONSTRUCT::_Write_Loop_Header(ANL_CBUF *cbuf)
{
   ANL_SRCPOS min_srcpos;
   ANL_SRCPOS max_srcpos;

   _Loop_Srcpos_Range(&min_srcpos, &max_srcpos);
   switch (_loop_kind)
   {
   case ANL_WHILE_LOOP:
      cbuf->Write_String("owhile ");
      cbuf->Write_Int(_id);
      if (_func_entry->Pu_Translator()->Language_is_C())
	 cbuf->Write_String(" \"while ");
      else
	 cbuf->Write_String(" \"do while ");
      _func_entry->Pu_Translator()->Expr_To_String(cbuf, WN_while_test(_loop));
      break;

   case ANL_WHILE_AS_DO_LOOP:
      cbuf->Write_String("oloop ");
      cbuf->Write_Int(_id);
      if (_func_entry->Pu_Translator()->Language_is_C())
	 cbuf->Write_String(" \"for ");
      else
	 cbuf->Write_String(" \"do ");
      _func_entry->Pu_Translator()->
	 Original_Symname_To_String(cbuf, WN_st(WN_prev(_loop)));
      break;

   case ANL_DO_LOOP:
   case ANL_DOACROSS_LOOP:
   case ANL_PARALLELDO_LOOP:
   case ANL_PDO_LOOP:
      cbuf->Write_String("oloop ");
      cbuf->Write_Int(_id);
      if (_func_entry->Pu_Translator()->Language_is_C())
	 cbuf->Write_String(" \"for ");
      else
	 cbuf->Write_String(" \"do ");
      _func_entry->Pu_Translator()->
	 Original_Symname_To_String(cbuf, WN_st(WN_index(_loop)));
      break;
      
   default:
      break;
   }
   cbuf->Write_String("\" range ");
   min_srcpos.Write(cbuf);
   cbuf->Write_Char('-');
   max_srcpos.Write(cbuf);
   cbuf->Write_Char('\n');
} // ANL_LOOP_CONSTRUCT::_Write_Loop_Header


void
ANL_LOOP_CONSTRUCT::_Write_Loop_Directive(ANL_CBUF *cbuf)
{
   BOOL       omp_pragma;
   WN        *nowait = NULL;
   WN        *end_pdo = NULL;
   WN        *region_pragma = WN_first(WN_region_pragmas(_loop_region));
   WN        *next_clause = WN_next(region_pragma);
   ANL_SRCPOS startpos;
   ANL_SRCPOS endpos;
   
   // Some preliminiaries.
   //
   _func_entry->Get_Pragma_Srcpos_Range(region_pragma, &startpos, &endpos);

   // The idir clause.
   //
   cbuf->Write_String("idir ");
   cbuf->Write_Int(_id);
   cbuf->Write_Char(' ');
   startpos.Write(cbuf);
   cbuf->Write_Char('-');
   endpos.Write(cbuf);

   omp_pragma = WN_pragma_omp(region_pragma);
   cbuf->Append_Pragma_Preamble(omp_pragma,FALSE);

   switch (_loop_kind)
   {
   case ANL_DOACROSS_LOOP:   
      cbuf->Write_String("DOACROSS ");
      break;
   case ANL_PARALLELDO_LOOP:   
      cbuf->Write_String("PARALLEL DO ");
      break;
   case ANL_PDO_LOOP:
      if (omp_pragma)
	 cbuf->Write_String("DO ");
      else if (_func_entry->Pu_Translator()->Language_is_C())
	 cbuf->Write_String("PFOR ");
      else
	 cbuf->Write_String("PDO ");
      break;
   default:
      cbuf->Write_String("<WHATLOOP??> ");
      break;
   }

   // Emit the "nest" clause if relevant
   //
   if (WN_max_loop_nest(region_pragma) > 1)
   {
      _func_entry->Pu_Translator()->
	 Nest_Clauses_To_String(cbuf, 
				_loop_region, 
				WN_max_loop_nest(region_pragma));
      if ( _func_entry->Pu_Translator()->Is_A_Pragma_Clause(next_clause))
	cbuf->Write_String(", ");
   }

   // The list of clauses on an idir.
   //
   _func_entry->Pu_Translator()->ClauseList_To_String(cbuf, &next_clause);
   cbuf->Write_String("\n");

   // The edir clause.
   //
   if (next_clause != NULL &&
       !_func_entry->Pu_Translator()->Language_is_C())
   {
      // We never emit an edir for language C (sections of code are delimited
      // by a nested scope, not by BEGIN/END pragmas).
      //
      if (WN_pragma(next_clause) == WN_PRAGMA_PDO_END &&
	  !WN_pragma_compiler_generated(next_clause))
	 end_pdo = next_clause;
      else if (WN_pragma(next_clause) == WN_PRAGMA_NOWAIT &&
	       !WN_pragma_compiler_generated(next_clause))
         nowait = next_clause;
   }
   if (nowait != NULL || end_pdo != NULL)
   {
      if (end_pdo != NULL)
	 startpos = ANL_SRCPOS(end_pdo);
      else
	 startpos = ANL_SRCPOS(nowait);
      cbuf->Write_String("edir ");
      cbuf->Write_Int(_id);
      cbuf->Write_String(" ");
      startpos.Write(cbuf);

      cbuf->Append_Pragma_Preamble(omp_pragma,FALSE);
      if (WN_pragma_omp(next_clause))
	cbuf->Write_String("END DO");
      else
	cbuf->Write_String("END PDO");
      if (nowait)
	 cbuf->Write_String(" nowait\n");
      else
	 cbuf->Write_Char('\n');
   }
} // ANL_LOOP_CONSTRUCT::_Write_Loop_Directive


// =============== Public Member Functions ================
// ========================================================


ANL_LOOP_CONSTRUCT::ANL_LOOP_CONSTRUCT(WN             *loop,
				       INT32           construct_level,
				       ANL_FUNC_ENTRY *func_entry,
				       MEM_POOL       *pool):
   _loop_region(NULL),
   _loop(loop),
   _construct_level(construct_level),
   _func_entry(func_entry),
   _pool(pool)
{
   WN  *parent_region = NULL;
   BOOL in_region = FALSE;
   
   // Set tentative loop kind (we may later change an ANL_DO_LOOP if its
   // context shows it to be a parallel loop).
   //
   if (WN_operator(loop) == OPR_WHILE_DO ||
       WN_operator(loop) == OPR_DO_WHILE)
   {
      if (_func_entry->Pu_Translator()->Whileloop_Looks_Like_Forloop(loop))
	 _loop_kind = ANL_WHILE_AS_DO_LOOP;
      else
	 _loop_kind = ANL_WHILE_LOOP;
   }
   else // An OPR_DO_LOOP
   {
      _loop_kind = ANL_DO_LOOP;
      parent_region = LWN_Get_Parent(LWN_Get_Parent(loop));
      in_region = (WN_operator(parent_region) == OPR_REGION);
   }

   // Set the ID for this loop.
   //
   _id = _func_entry->Next_Construct_Id()->Post_Incr();
   func_entry->Set_Construct_Id(loop, _id);

   // See if it is a special parallel loop
   //
   if (in_region)
   {
      WN *pragma = WN_first(WN_region_pragmas(parent_region));

      if (pragma != NULL)
      {
	 if (WN_pragma(pragma) == WN_PRAGMA_DOACROSS &&
	     WN_current_loop_nest(pragma) == 0)
	 {
	    _loop_kind = ANL_DOACROSS_LOOP;
	    _loop_region = parent_region;
	    func_entry->Set_Construct_Id(parent_region, _id);
	 }
	 else if (WN_pragma(pragma) == WN_PRAGMA_PARALLEL_DO &&
		  WN_current_loop_nest(pragma) == 0)
	 {
	    _loop_kind = ANL_PARALLELDO_LOOP;
	    _loop_region = parent_region;
	    func_entry->Set_Construct_Id(parent_region, _id);
	 }
	 else if (WN_pragma(pragma) == WN_PRAGMA_PDO_BEGIN &&
		  WN_current_loop_nest(pragma) == 0)
	 {
	    _loop_kind = ANL_PDO_LOOP;
	    _loop_region = parent_region;
	    func_entry->Set_Construct_Id(parent_region, _id);
	 }
      } // if region pragma
   } // if in region
} // ANL_LOOP_CONSTRUCT::ANL_LOOP_CONSTRUCT

   
WN *
ANL_LOOP_CONSTRUCT::Next_Stmt()
{
   // The next statement must be in the same scope as the _loop, since
   // that is expected from ANL_FUNC_ENTRY::Emit_Nested_Original_Constructs().
   //
   return WN_next(_loop);
} // ANL_LOOP_CONSTRUCT::Next_Stmt


void
ANL_LOOP_CONSTRUCT::Write(ANL_CBUF *cbuf)
{
   ANL_CBUF    varlist_cbuf(_pool);
   ANL_CBUF    nested_cbuf(_pool);
   ANL_SRCPOS  min_srcpos;
   ANL_SRCPOS  max_srcpos;
   ANL_VARLIST varlist(_pool, _func_entry);
   WN         *first_stmt = _First_Loop_Stmt();
   WN         *last_stmt = _Last_Loop_Stmt();
   WN         *body = ((_loop_kind == ANL_WHILE_AS_DO_LOOP ||
			_loop_kind == ANL_WHILE_LOOP)?  
		       WN_while_body(_loop) :
		       WN_do_body(_loop));

   // Write out the loop header
   //
   _Write_Loop_Header(cbuf);

   // Write out the index variable.
   //
   if (_loop_kind != ANL_WHILE_LOOP)
   {
      ST *const st = (_loop_kind == ANL_WHILE_AS_DO_LOOP?
		      WN_st(WN_prev(_loop)) : WN_st(WN_index(_loop)));
      
      cbuf->Write_String("index \"");
      _func_entry->Pu_Translator()-> Original_Symname_To_String(cbuf, st);
      cbuf->Write_String("\"\n");
   }

   // Write out the loop related directives
   //
   if (_loop_kind == ANL_DOACROSS_LOOP   ||
       _loop_kind == ANL_PARALLELDO_LOOP ||
       _loop_kind == ANL_PDO_LOOP)
   {
      _Write_Loop_Directive(cbuf);
   }

   // Determine variable references within the statements belonging to the
   // loop, and write them to a temporary buffer.
   //
   for (WN *stmt = first_stmt;
	stmt != NULL && WN_prev(stmt) != last_stmt;
	stmt = WN_next(stmt))
   {
      varlist.Insert_Var_Refs(stmt);
   }
   varlist.Write(&varlist_cbuf, _id);
   varlist_cbuf.Write_Char('\n');

   // Write nested constructs to a temporary buffer
   //
   _func_entry->Emit_Nested_Original_Constructs(&nested_cbuf, 
						WN_first(body), WN_last(body));

   // Write out any applicable <dir> entries (i.e. those that were
   // not attributed to nested constructs).
   //
   _func_entry->
      Emit_Dir_Entries(cbuf, _id, _construct_level,
		       &ANL_LOOP_CONSTRUCT::Is_Valid_Dir);

   // Write the varlist and nested constructs.
   //
   if (varlist_cbuf.Size() > 0)
      cbuf->Write_String(varlist_cbuf.Chars());
   if (nested_cbuf.Size() > 0)
      cbuf->Write_String(nested_cbuf.Chars());

   // Finish writing the function descriptor.
   //
   if (_loop_kind == ANL_WHILE_LOOP)
      cbuf->Write_String("end_owhile ");
   else
      cbuf->Write_String("end_oloop ");
   cbuf->Write_Int(_id);
   cbuf->Write_String("\n"); // Start next construct on a new line
} // ANL_LOOP_CONSTRUCT::Write
