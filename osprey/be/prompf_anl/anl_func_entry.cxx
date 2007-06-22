/*
 * Copyright 2004 PathScale, Inc.  All Rights Reserved.
 */

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

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "anl_common.h"
#include "anl_diagnostics.h"  // For warnings and errors
#include "anl_file_mngr.h"    // For managing files
#include "w2cf_translator.h"  // For translating WHIRL into high-level-language
#include "anl_varlist.h"      // For emitting attributes of symbol references
#include "anl_pragma_attribute.h" // For <dir> entries
#include "anl_loop_construct.h"
#include "anl_pragma_construct.h"
#include "anl_region_construct.h"
#include "anl_func_entry.h"

extern ANL_DIAGNOSTICS *Anl_Diag; // Defined in anl_driver.cxx
extern BOOL             Anl_Owhile; // Defined in anl_driver.cxx


// ================== External Functions ==================
// ========================================================

void 
Adjust_Srcpos_Range(WN         *stmt,
		    ANL_SRCPOS *min,
		    ANL_SRCPOS *max)
{
   WN_ITER *stmt_iter;

   // Traverse all statements and pragmas in the stmt, and increase the given
   // srcpos range to encompass the srcpos of the stmts encountered in the
   // walk.
   //
   for (stmt_iter = WN_WALK_StmtIter(stmt);
	stmt_iter != NULL;
	stmt_iter = WN_WALK_StmtNext(stmt_iter))
   {
      ANL_SRCPOS srcpos(WN_ITER_wn(stmt_iter));

      if (srcpos > *max)
	 *max = srcpos;
      else if (srcpos < *min)
	 *min = srcpos;
   }
} // Adjust_Srcpos_Range


// =============== Private Member Functions ===============
// ========================================================

void
ANL_FUNC_ENTRY::_Push_Construct_Level(ANL_CBUF *cbuf)
{
   _construct_level++;
   if (_construct_level == 1)
      cbuf->Write_Char('\n');
} // ANL_FUNC_ENTRY::_Push_Construct_Level


void
ANL_FUNC_ENTRY::_Pop_Construct_Level()
{
   _construct_level--;
} // ANL_FUNC_ENTRY::_Pop_Construct_Level


// =============== Public Member Functions ================
// ========================================================

ANL_FUNC_ENTRY::~ANL_FUNC_ENTRY()
{
   for (INT i = _dir_entries.Size()-1; i >= 0; i--)
      CXX_DELETE(_dir_entries.Indexed_Get(i), _pool);
} // ANL_FUNC_ENTRY::~ANL_FUNC_ENTRY


void 
ANL_FUNC_ENTRY::Get_Pragma_Srcpos_Range(WN         *apragma,
					ANL_SRCPOS *min,
					ANL_SRCPOS *max)
{
   ANL_SRCPOS   pos(apragma);
   WN_PRAGMA_ID id = (WN_PRAGMA_ID)WN_pragma(apragma);
   WN          *next = WN_next(apragma);
   INT32        num_dims;

   *min = pos;
   *max = pos;

   switch (id)
   {
   case WN_PRAGMA_PREFETCH_REF:
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
      while (next != NULL                          &&
	     (WN_operator(next) == OPR_XPRAGMA || 
	      WN_operator(next) == OPR_PRAGMA)     &&
	     WN_pragma(next) == id                 &&
	     ANL_SRCPOS(next).Linenum() == pos.Linenum())
      {
	 ANL_SRCPOS srcpos(next);
	 if (srcpos > *max)
	    *max = srcpos;
	 next = WN_next(next);
      }
      break;
   case WN_PRAGMA_DISTRIBUTE:
   case WN_PRAGMA_REDISTRIBUTE:
   case WN_PRAGMA_DISTRIBUTE_RESHAPE:
      for (num_dims = 1; 
	   (next != NULL                        &&
	    WN_operator(next) == OPR_PRAGMA     &&
	    WN_pragma(next) == id               &&
	    num_dims == WN_pragma_index(next));
	   num_dims++, next = WN_next(next))
      {
	 ANL_SRCPOS srcpos(next);
	 if (srcpos > *max)
	    *max = srcpos;
      }

      /* Skip two stores, which are generated purely for dependency analysis
       * purposes.
       */
      if (WN_operator(next)==OPR_STID && 
	  ST_class(WN_st(next))==CLASS_PREG)
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
   while (_w2cf->Is_A_Pragma_Clause(next))
   {
      if (!WN_pragma_compiler_generated(next))
      {
	 ANL_SRCPOS srcpos(next);
	 if (srcpos > *max)
	    *max = srcpos;
      }
      next = WN_next(next);
   }
} // ANL_FUNC_ENTRY::Get_Pragma_Srcpos_Range


void 
ANL_FUNC_ENTRY::Set_Construct_Id(WN *construct, INT64 id)
{
   if (id > INT32_MAX)
      Anl_Diag->Error("Too many constructs for PROMPF!!");
   else
      WN_MAP32_Set(_id_map, construct, id);
} // ANL_FUNC_ENTRY::Set_Construct_Id


void
ANL_FUNC_ENTRY::Emit_Nested_Original_Constructs(ANL_CBUF *cbuf,
						WN            *from_stmt, 
						WN            *to_stmt)
{
   // Look for nested original constructs in the range of statements
   // on the list from "from_stmt" to "to_stmt" inclusively.  We assume
   // "from_stmt!=NULL" implies "to_stmt!=NULL".
   //
   BOOL                  flag;
   ANL_PRAGMA_ATTRIBUTE *dir_attribute = NULL;
   ANL_PRAGMA_CONSTRUCT *pragma_construct = NULL;
   ANL_REGION_CONSTRUCT *region_construct = NULL;
   ANL_LOOP_CONSTRUCT   *loop_construct = NULL;
   WN                   *stmt = from_stmt;

   while (stmt != NULL && stmt != WN_next(to_stmt))
   {
      switch (WN_operator(stmt))
      {
      case OPR_IF:
	 Emit_Nested_Original_Constructs(cbuf,
					 WN_kid1(stmt),
					 WN_kid1(stmt));
	 Emit_Nested_Original_Constructs(cbuf,
					 WN_kid2(stmt),
					 WN_kid2(stmt));
	 stmt = WN_next(stmt);
	 break;

      case OPR_BLOCK:
	 Emit_Nested_Original_Constructs(cbuf,
					 WN_first(stmt),
					 WN_last(stmt));
	 stmt = WN_next(stmt);
	 break;

      case OPR_XPRAGMA:
      case OPR_PRAGMA:
	 if (ANL_PRAGMA_CONSTRUCT::Is_ProMpf_Pragma_Construct(stmt))
	 {
	    _Push_Construct_Level(cbuf); 
	    pragma_construct =
	       CXX_NEW(ANL_PRAGMA_CONSTRUCT(stmt, 
					    _construct_level,
					    this, 
					    _pool), _pool);
	    pragma_construct->Write(cbuf);
	    stmt = pragma_construct->Next_Stmt();
	    CXX_DELETE(pragma_construct, _pool);
	    _Pop_Construct_Level(); 
	 }
	 else if (ANL_PRAGMA_ATTRIBUTE::Is_ProMpf_Pragma_Attribute(stmt))
	 {
	    dir_attribute = 
	       CXX_NEW(ANL_PRAGMA_ATTRIBUTE(stmt,
					    _construct_level,
					    this, 
					    _pool), _pool);
	    _dir_entries.Insert_Last(dir_attribute, &flag);
	    stmt = dir_attribute->Next_Stmt();
	 }
	 else
	    stmt = WN_next(stmt);
	 break;

      case OPR_REGION:
	 if (ANL_REGION_CONSTRUCT::Is_ProMpf_Region_Construct(stmt))
	 {
	    _Push_Construct_Level(cbuf);
	    region_construct = 
	       CXX_NEW(ANL_REGION_CONSTRUCT(stmt, 
					    _construct_level,
					    this, 
					    _pool), _pool);
	    region_construct->Write(cbuf);
	    stmt = region_construct->Next_Stmt();
	    CXX_DELETE(region_construct, _pool);
	    _Pop_Construct_Level();
	 }
	 else
	 {
	    Emit_Nested_Original_Constructs(cbuf,
					    WN_first(WN_region_body(stmt)),
					    WN_last(WN_region_body(stmt)));
	    stmt = WN_next(stmt);
	 }
	 break;

      case OPR_DO_WHILE:
      case OPR_WHILE_DO:
	 if (Anl_Owhile)
	 {
	    // Emit original construct for a while-loop.
	    //
	    _Push_Construct_Level(cbuf);
	    loop_construct = 
	       CXX_NEW(ANL_LOOP_CONSTRUCT(stmt, 
					  _construct_level,
					  this,
					  _pool), _pool);
	    loop_construct->Write(cbuf);
	    stmt = loop_construct->Next_Stmt();
	    CXX_DELETE(loop_construct, _pool);
	    _Pop_Construct_Level();
	 }
	 else
	 {
	    // Do not treat while-loops as original constructs.
	    //
	    Emit_Nested_Original_Constructs(cbuf,
					    WN_kid1(stmt),
					    WN_kid1(stmt));
	    stmt = WN_next(stmt);
	 } 
	 break;

      case OPR_DO_LOOP:
	 _Push_Construct_Level(cbuf);
	 loop_construct = 
	    CXX_NEW(ANL_LOOP_CONSTRUCT(stmt, 
				       _construct_level,
				       this,
				       _pool), _pool);
	 loop_construct->Write(cbuf);
	 stmt = loop_construct->Next_Stmt();
	 CXX_DELETE(loop_construct, _pool);
	 _Pop_Construct_Level();
	 break;

      default:
	 stmt = WN_next(stmt);
	 break;
      }
   }
} // ANL_FUNC_ENTRY::Emit_Nested_Original_Constructs


void 
ANL_FUNC_ENTRY::Emit_Dir_Entries(ANL_CBUF *cbuf,
				 INT64     for_construct_id,
				 INT32     for_construct_level,
				 BOOL (*do_emit)(ANL_PRAGMA_ATTRIBUTE *dir,
						 INT32      construct_level))
{
   for (INT i = _dir_entries.Size() - 1; i >= 0; i--)
   {
      ANL_PRAGMA_ATTRIBUTE *dir = _dir_entries.Indexed_Get(i);

      if (do_emit(dir, for_construct_level))
      {
	 dir->Write(cbuf, for_construct_id);
	 _dir_entries.Indexed_Remove(i); // Changes _dir_entries.Size()!
	 CXX_DELETE(dir, _pool);
      }
   }
} // ANL_FUNC_ENTRY::Emit_Dir_Entries


void 
ANL_FUNC_ENTRY::Emit_Original_Construct(ANL_FILE_MNGR *outp_file)
{
   const INT64 id = _next_id->Post_Incr();
   ANL_CBUF    cbuf(_pool);
   ANL_CBUF    varlist_cbuf(_pool);
   ANL_CBUF    nested_cbuf(_pool);
   ANL_SRCPOS  min_srcpos(_pu);
   ANL_SRCPOS  max_srcpos(_pu);
   ANL_VARLIST varlist(_pool, this);

   Set_Construct_Id(_pu, id);

   // Start writing the function descriptor to file
   //
   Adjust_Srcpos_Range(_pu, &min_srcpos, &max_srcpos);
   cbuf.Write_String("function ");
   cbuf.Write_Int(id);
   cbuf.Write_String(" \"");
   _w2cf->Original_Symname_To_String(&cbuf, &St_Table[WN_entry_name(_pu)]);
   cbuf.Write_String("\" range ");
   min_srcpos.Write(&cbuf);
   cbuf.Write_Char('-');
   max_srcpos.Write(&cbuf);
   cbuf.Write_Char('\n');
   outp_file->Write_String(cbuf.Chars());

   // Determine variable references within the function body, and
   // write them out to a temporary buffer.
   //
   varlist.Insert_Var_Refs(WN_func_body(_pu));
   varlist.Write(&varlist_cbuf, id);
   varlist_cbuf.Write_Char('\n');

   // Write nested constructs to a temporary buffer
   //
   Emit_Nested_Original_Constructs(&nested_cbuf, 
				   WN_first(WN_func_body(_pu)), 
				   WN_last(WN_func_body(_pu)));

   // Write out any remaining <dir> entries to file (i.e. those that were
   // not attributed to nested constructs.
   //
   cbuf.Reset();
   for (INT i = _dir_entries.Size()-1; i >= 0; i--)
   {
      ANL_PRAGMA_ATTRIBUTE *dir = _dir_entries.Indexed_Get(i);
      dir->Write(&cbuf, id);
      _dir_entries.Indexed_Remove(i); // Changes _dir_entries.Size()
      CXX_DELETE(dir, _pool);
   }

   // Write the varlist and nested constructs to file.
   //
   if (cbuf.Size() > 0)
      outp_file->Write_String(cbuf.Chars());
   if (varlist_cbuf.Size() > 0)
      outp_file->Write_String(varlist_cbuf.Chars());
   if (nested_cbuf.Size() > 0)
      outp_file->Write_String(nested_cbuf.Chars());

   // Finish writing the function descriptor to file
   //
   cbuf.Reset();
   cbuf.Write_String("end_function ");
   cbuf.Write_Int(id);
   cbuf.Write_String("\n\n\n"); // Seperate function entries by two empty lines
   outp_file->Write_String(cbuf.Chars());

} // ANL_FUNC_ENTRY::Emit_Original_Construct
