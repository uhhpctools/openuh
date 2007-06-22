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
#include "anl_region_construct.h"

extern ANL_DIAGNOSTICS *Anl_Diag; // Defined in anl_driver.cxx


// ================ Class Utility Functions ===============
// ========================================================


// A region construct appears in a REGION_pragmas section.
// Thus the scope of the pragma is the region. There may be
// a NOWAIT or END_MARKER to provide the line number of the
// end construct in the source.

BOOL 
ANL_REGION_CONSTRUCT::Is_ProMpf_Region_Construct(WN *stmt)
{
   BOOL predicate = (stmt != NULL && WN_operator(stmt) == OPR_REGION);

   if (predicate)
   {
      WN *pragma = WN_first(WN_region_pragmas(stmt));
      predicate = pragma != NULL ; 

      if (predicate) 
        {
	  switch (WN_pragma(pragma)) 
	    {
	    case WN_PRAGMA_PARALLEL_BEGIN:
	    case WN_PRAGMA_MASTER_BEGIN:
	    case WN_PRAGMA_SINGLE_PROCESS_BEGIN:
	    case WN_PRAGMA_PSECTION_BEGIN:
	    case WN_PRAGMA_PARALLEL_SECTIONS:
	      break;

	    default:
	      predicate  = FALSE;
	      break ;
	    }
	}
    }
   return predicate;
} // ANL_REGION_CONSTRUCT::Is_ProMpf_Region_Construct


BOOL 
ANL_REGION_CONSTRUCT::Is_Valid_Dir(ANL_PRAGMA_ATTRIBUTE *dir,
				   INT32                 construct_level)
{
   return dir->Is_Region_Construct_Attribute(construct_level);
}


// =============== Private Member Functions ===============
// ========================================================

WN *
ANL_REGION_CONSTRUCT::_First_Region_Stmt()
{
   WN *base = _region;
   ANL_SRCPOS basepos(_region);

   while (WN_prev(base) != NULL && ANL_SRCPOS(WN_prev(base)) >= basepos)
      base = WN_prev(base);
   return base;
} // ANL_REGION_CONSTRUCT::_First_Region_Stmt


WN *
ANL_REGION_CONSTRUCT::_Last_Region_Stmt()
{
   WN *base = _region;
   ANL_SRCPOS basepos(_region);

   while (WN_next(base) != NULL && ANL_SRCPOS(WN_next(base)) <= basepos)
      base = WN_next(base);
   return base;
} //ANL_REGION_CONSTRUCT::_Last_Region_Stmt


void 
ANL_REGION_CONSTRUCT::_Region_Srcpos_Range(ANL_SRCPOS *min, ANL_SRCPOS *max)
{
   // Get the source-position range for the region, including associated
   // pragmas and loop initialization statements (which may have been moved
   // in front of the region by the compiler front-end).
   //
   // If this is an OMP region, there may be an NOWAIT/END_MARKER
   // which provides the line number of the end construct.  However,
   // we rely on the more general algorithm, which should also yield
   // correct results in the event of a WN_PRAGMA_PARALLEL_END,
   // WN_PRAGMA_END_MARKER or WN_PRAGMA_NOWAIT pragma.
   //
   WN        *end_stmt = NULL;
   WN        *first_stmt = _First_Region_Stmt();
   WN        *last_stmt = _Last_Region_Stmt();
   ANL_SRCPOS minpos(first_stmt);
   ANL_SRCPOS maxpos(last_stmt);

   // Start out with the top-level stmts (at same nesting level as the
   // region) as a first approximation, looking only at the stmts
   // within the region.
   //
   for (WN *stmt = first_stmt; 
	stmt != NULL && WN_prev(stmt) != last_stmt;
	stmt = WN_next(stmt))
   {
      Adjust_Srcpos_Range(stmt, &minpos, &maxpos);
   }

   // See if we have an end-marker.
   //
   for (end_stmt = WN_first(WN_region_pragmas(_region));
	(end_stmt != NULL                              && 
	 WN_pragma(end_stmt) != WN_PRAGMA_PARALLEL_END &&
	 WN_pragma(end_stmt) != WN_PRAGMA_END_MARKER   &&
	 (WN_pragma(end_stmt) != WN_PRAGMA_NOWAIT ||
	  _func_entry->Pu_Translator()->Language_is_C()));
	end_stmt = WN_next(end_stmt));

   // If the region does not have a clearly marked END, then consider
   // it to extend up till the next statement (if one is present).
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
	   ("Unexpected maxpos in "
	    "ANL_LOOP_CONSTRUCT::_Loop_Srcpos_Range"));

   *min = minpos;
   *max = maxpos;
 } // ANL_REGION_CONSTRUCT::_Region_Srcpos_Range


WN *
ANL_REGION_CONSTRUCT::_userEndParallel()
{
   WN  *directive;
   WN  *end_directive = NULL;

   if (!_func_entry->Pu_Translator()->Language_is_C())
   {
      for (directive = WN_first(WN_region_pragmas(_region));
	   end_directive == NULL && directive != NULL;
	   directive = WN_next(directive))
      {
	 if (WN_pragma(directive) == WN_PRAGMA_PARALLEL_END &&
	     !WN_pragma_compiler_generated(directive)) 
	    end_directive = directive;
	 else if (WN_pragma(directive) == WN_PRAGMA_NOWAIT)
	    end_directive = directive;
	 else if (WN_pragma(directive) == WN_PRAGMA_END_MARKER)
	    end_directive = directive;
      }
   }
   return end_directive;

} // ANL_REGION_CONSTRUCT::_userEndParallel


void
ANL_REGION_CONSTRUCT::_Write_Region_Directive(ANL_CBUF *cbuf)
{
  WN        *region_end_pragma = _userEndParallel();
  WN        *region_pragma = WN_first(WN_region_pragmas(_region));
  WN        *region_clause = WN_next(region_pragma);
  ANL_SRCPOS startpos, endpos;
  char      *p;

  _func_entry->Get_Pragma_Srcpos_Range(region_pragma, &startpos, &endpos);

  // The idir clause.
  //
  cbuf->Write_String("idir ");
  cbuf->Write_Int(_id);
  cbuf->Write_Char(' ');
  startpos.Write(cbuf);
  cbuf->Write_Char('-');
  endpos.Write(cbuf);

  cbuf->Append_Pragma_Preamble(_is_omp,FALSE);

  switch (_region_kind)
  {
  case ANL_PARALLEL_REGION:   
     cbuf->Write_String("PARALLEL ");
     break;
  case ANL_PSECTION_REGION:   
     if(_is_omp)
	p = "SECTIONS ";
     else
	p = "PSECTIONS ";
     cbuf->Write_String(p);
     break;
  case ANL_SINGLE_PROCESS_REGION:  
     if (_is_omp)
	p = "SINGLE "; 
     else
	p = "SINGLE PROCESS " ;
     cbuf->Write_String(p);
     break;
  case ANL_MASTER_PROCESS_REGION:  
     cbuf->Write_String("MASTER"); 
     break;
  default:
     cbuf->Write_String("<WHATREGION??> ");
     break;
  }

  // The list of clauses on an idir.
  //
  _func_entry->Pu_Translator()->ClauseList_To_String(cbuf, &region_clause);
  cbuf->Write_String("\n");

  if (region_end_pragma)
  {
     // Note: we should never get here for C language.
     //
     _func_entry->Get_Pragma_Srcpos_Range(region_end_pragma,
					  &startpos, &endpos);
     cbuf->Write_String("edir ");
     cbuf->Write_Int(_id);
     cbuf->Write_String(" ");
     startpos = ANL_SRCPOS(region_end_pragma);
     startpos.Write(cbuf);

     cbuf->Append_Pragma_Preamble(_is_omp,FALSE);

     switch(_region_kind) 
     {
     case ANL_PSECTION_REGION:
	if(_is_omp)
	   p = "END SECTIONS";
	else
	   p = "END PSECTIONS";
	cbuf->Write_String(p);
	break ;
	
     case ANL_SINGLE_PROCESS_REGION:
	if(_is_omp)
	   p = "END SINGLE";
	else
	   p = "END SINGLE PROCESS";
	
	cbuf->Write_String(p);
	break;
	 
     case ANL_MASTER_PROCESS_REGION:
	cbuf->Write_String("END MASTER");
	break;
	
     case ANL_PARALLEL_REGION:
	cbuf->Write_String("END PARALLEL");
	break;
	 
     default:
	cbuf->Write_String("<WHAT END PRAGMA>");
	break;
     }

      
     if (_is_omp && 
	 WN_pragma(region_end_pragma) == WN_PRAGMA_NOWAIT)
     {
	cbuf->Write_String(" NOWAIT ");
	_is_nowait = TRUE;
     }

     cbuf->Write_Char('\n');
  }
} // ANL_REGION_CONSTRUCT::_Write_Region_Directive


// =============== Public Member Functions ================
// ========================================================

ANL_REGION_CONSTRUCT::ANL_REGION_CONSTRUCT(WN             *region,
					   INT32           construct_level,
					   ANL_FUNC_ENTRY *func_entry,
					   MEM_POOL       *pool):
					   _region(region),
					   _construct_level(construct_level),
					   _func_entry(func_entry),
					   _pool(pool)
{
  WN * pragma;

  _region_kind = ANL_UNKNOWN_REGION;
  _id = -1;
  _is_nowait = FALSE ;
  _is_omp = FALSE ;

  // if this is a construct, give it a  Prompf_id. The Id is set
  // on the region, but as PSECTIONS,SINGLE_PROCESS etc, may
  // not be OMP directives and are considered (just) pragmas 
  // by w2f, the id goes on the pragma as well. PARALLEL_BEGIN 
  // is & was always a region, so doesn't need an id on the pragma.

  if (Is_ProMpf_Region_Construct(region))
    {
      pragma  = WN_first(WN_region_pragmas(region));
      _is_omp = WN_pragma_omp(pragma);
      _id     = _func_entry->Next_Construct_Id()->Post_Incr();
	 
      func_entry->Set_Construct_Id(region, _id);
      func_entry->Set_Construct_Id(pragma, _id);

      switch(WN_pragma(pragma))
	{
	case WN_PRAGMA_PARALLEL_BEGIN:
	  _region_kind = ANL_PARALLEL_REGION; 
	  break;
	  
	case WN_PRAGMA_PARALLEL_SECTIONS:
	case WN_PRAGMA_PSECTION_BEGIN:
	  _region_kind = ANL_PSECTION_REGION; 
	  break;

	case WN_PRAGMA_SINGLE_PROCESS_BEGIN:
	  _region_kind = ANL_SINGLE_PROCESS_REGION; 
	  break;

	case WN_PRAGMA_MASTER_BEGIN:
	  _region_kind = ANL_MASTER_PROCESS_REGION; 
	  break;
	}
    }
} // ANL_REGION_CONSTRUCT::ANL_REGION_CONSTRUCT


WN *
ANL_REGION_CONSTRUCT::Next_Stmt()
{
   WN *last_stmt = _Last_Region_Stmt();
   return WN_next(last_stmt);
} // ANL_REGION_CONSTRUCT::Next_Stmt


void
ANL_REGION_CONSTRUCT::Write(ANL_CBUF *cbuf)
{
  if (_region_kind != ANL_UNKNOWN_REGION)
    {
      ANL_CBUF    varlist_cbuf(_pool);
      ANL_CBUF    nested_cbuf(_pool);
      ANL_SRCPOS  min_srcpos;
      ANL_SRCPOS  max_srcpos;
      ANL_VARLIST varlist(_pool, _func_entry);
      WN         *first_stmt = _First_Region_Stmt();
      WN         *last_stmt = _Last_Region_Stmt();
      char       *p;

      // Write out the construct header
      //
      _Region_Srcpos_Range(&min_srcpos, &max_srcpos);

      cbuf->Append_Pragma_Preamble(_is_omp,TRUE);
      switch (_region_kind) 
	{
	case ANL_PARALLEL_REGION:
	  cbuf->Write_String("pregion ");
	  cbuf->Write_Int(_id);
	  cbuf->Write_String(" \"parallel region\"");
	  break;
	  
	case ANL_PSECTION_REGION:
	  cbuf->Write_String("psections ");
	  cbuf->Write_Int(_id);
	  cbuf->Write_String(" \"parallel sections\"");
	  break;

	case ANL_SINGLE_PROCESS_REGION:
	  if (_is_omp) 
	    p = "single ";
	  else
	    p = "single_proc ";
	  cbuf->Write_String(p);
	  cbuf->Write_Int(_id);
	  cbuf->Write_String(" \"single processor section\"");
	  break;

	case ANL_MASTER_PROCESS_REGION:
	  cbuf->Write_String("master ");
	  cbuf->Write_Int(_id);
	  cbuf->Write_String(" \"master processor section\"");
	  break;

	default:
	  cbuf->Write_String(" <WHATREGION??>");
	  break;
	}
      cbuf->Write_String(" range ");
      min_srcpos.Write(cbuf);
      cbuf->Write_Char('-');
      max_srcpos.Write(cbuf);
      cbuf->Write_Char('\n');

      // Write out the region directives
      //
      _Write_Region_Directive(cbuf);

      // Determine variable references within the statements belonging to the
      // loop, and write them out to a temporary buffer.
      //
      for (WN *stmt = first_stmt;
	   stmt != NULL && WN_prev(stmt) != last_stmt;
	   stmt = WN_next(stmt))
      {
	 varlist.Insert_Var_Refs(stmt);
      }
      varlist.Write(&varlist_cbuf, _id);
      varlist_cbuf.Write_Char('\n');

      // Write nested constructs to a temporary buffer.
      //
      _func_entry->
	 Emit_Nested_Original_Constructs(&nested_cbuf,
					 WN_first(WN_region_body(_region)),
					 WN_last(WN_region_body(_region)));

      // Write out any applicable <dir> entries (i.e. those that were
      // not attributed to nested constructs).
      //
      _func_entry->
	 Emit_Dir_Entries(cbuf, _id, _construct_level,
			  &ANL_REGION_CONSTRUCT::Is_Valid_Dir);

      // Write the varlist and nested constructs.
      //
      if (varlist_cbuf.Size() > 0)
	 cbuf->Write_String(varlist_cbuf.Chars());
      if (nested_cbuf.Size() > 0)
	 cbuf->Write_String(nested_cbuf.Chars());

      // Finish writing the construct descriptor to buffer
      //

      cbuf->Append_Pragma_Preamble(_is_omp,TRUE);
      switch (_region_kind) 
	{
	case ANL_PARALLEL_REGION:
	  cbuf->Write_String("end_pregion ");
	  break;
	  
	case ANL_PSECTION_REGION:
	  cbuf->Write_String("end_psections ");
	  break ;

	case ANL_SINGLE_PROCESS_REGION:
	  if (_is_omp)
	    p = "end_single ";
	  else
	    p = "end_single_proc ";
	  cbuf->Write_String(p);
	  break ;

	case ANL_MASTER_PROCESS_REGION:
	 cbuf->Write_String("end_master ");
	 break;

	default:
	  cbuf->Write_String("<what_end??> ");
	}

      if (_is_nowait)
	cbuf->Write_String("nowait ");

      cbuf->Write_Int(_id);
      cbuf->Write_String("\n"); // Start next construct on a new line
   }
} // ANL_REGION_CONSTRUCT::Write
