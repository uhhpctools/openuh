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
#include "anl_file_mngr.h"    // For writing to output file
#include "w2cf_translator.h"  // For translating WHIRL into high-level-language
#include "anl_func_entry.h"   // For func entry related utilities
#include "anl_varlist.h"

extern ANL_DIAGNOSTICS *Anl_Diag; // Defined in anl_driver.cxx

// ================== Internal utilities ==================
// ========================================================

// This is necessary until we consistently use SCLASS_FORMAL_REF ro
// represent reference parameters.
//
#define ANL_IS_REF_PARM_ADDR(st) \
(ST_sclass(st) == SCLASS_FORMAL && \
 TY_IS_POINTER(ST_type(st)) && \
 !ST_is_value_parm(st))

static BOOL 
St_Belongs_In_Varlist(ST *st)
{
   return ((ST_sym_class(st) == CLASS_VAR) &&
	   (!ST_is_temp_var(st)) &&
	   (!Has_Base_Block(st) || St_Belongs_In_Varlist(ST_base(st))));
} // St_Belongs_In_Varlist


static void
Write_Sclass(ANL_CBUF *cbuf, ST *st)
{

  if (Has_Base_Block(st))
    st = ST_base(st);

   switch (ST_sclass(st))
   {
   case SCLASS_AUTO:
      cbuf->Write_Char('A');
      break;

   case SCLASS_FORMAL:
      if (ANL_IS_REF_PARM_ADDR(st))
	 cbuf->Write_Char('R'); // Reference parameter
      else
	 cbuf->Write_Char('V'); // Value parameter
      break;

   case SCLASS_PSTATIC:
   case SCLASS_FSTATIC:
      cbuf->Write_Char('S');
      break;

   case SCLASS_COMMON:
   case SCLASS_EXTERN:
   case SCLASS_UGLOBAL:
   case SCLASS_DGLOBAL:
      cbuf->Write_Char('G');
      break;


   case SCLASS_FORMAL_REF:
      cbuf->Write_Char('R'); // Reference parameter
      break;

   default:
      cbuf->Write_Char('U'); // We do not expect this in a varlist!
      break;
   }
} // Write_Sclass


// =============== Private Member Functions ===============
// ========================================================

mUINT32 
ANL_VARLIST::_Binary_Search(INT32 id, mUINT32 from, mUINT32 till)
{
   // Return the approximate insertion point, if the desired item
   // is not found.  Note that the actual insertion point will be
   // either immediately before or after the returned index.
   //
   mUINT32 found_idx;

   if (from >= till)
      found_idx = from;
   else
   {
      const mUINT32 halfway = (till + from) / 2;
      const INT32   id2 = _vlist.Indexed_Get(halfway)->Id();

      if (id == id2)
	 found_idx = halfway;
      else if (id < id2)
      {
	 if (halfway == 0)
	    found_idx = 0;
	 else
	    found_idx = _Binary_Search(id, from, halfway-1);
      }
      else
	 found_idx = _Binary_Search(id, halfway+1, till);
   }
   return found_idx;
} // ANL_VARLIST::_Binary_Search


UINT32 
ANL_VARLIST::_Get_Io_Item_Lda_Access_Status(WN *io_item)
{
   UINT32 status;

   // Assume all LDAs under io items that are not in an IO list are
   // read but not written.
   //
   switch (WN_io_item(io_item))
   {
   case IOL_ARRAY:
   case IOL_CHAR:
   case IOL_CHAR_ARRAY:
   case IOL_EXPR:
   case IOL_IMPLIED_DO:
   case IOL_IMPLIED_DO_1TRIP:
   case IOL_LOGICAL:
   case IOL_RECORD:
   case IOL_VAR:
   case IOL_DOPE:
      status = 0;
      break;
   default:
      status = ANL_VAR_READ;
      break;
   }

   if (status == 0)
   {
      // First determine whether this is an input or output IO statement
      // for the given IO list item.
      //
      WN *io_stmt = io_item;
      while (WN_operator(io_stmt) != OPR_IO)
	 io_stmt = LWN_Get_Parent(io_stmt);

      switch (WN_io_statement(io_stmt))
      {
      case IOS_PRINT:
      case IOS_TYPE:
      case IOS_REWRITE:
      case IOS_WRITE:
	 status = ANL_VAR_READ;
	 break;

      case IOS_READ:
      case IOS_ACCEPT:
	 status = ANL_VAR_WRITTEN;
	 break;

      case IOS_ENCODE:
	 status = ANL_VAR_READ;     // Reads from the IOL item
	 break;

      case IOS_DECODE:
	 status = ANL_VAR_WRITTEN;  // Writes to the IOL item
	 break;

      default:
	 status = ANL_VAR_READ | ANL_VAR_WRITTEN;
	 break;
      }
   }
   return status;
} // ANL_VARLIST::_Get_Io_Item_Lda_Access_Status


UINT32 
ANL_VARLIST::_Get_Lda_Access_Status(WN *lda)
{
   UINT32 status;
   WN    *parent = LWN_Get_Parent(lda);

   switch (WN_operator(parent))
   {
   case OPR_ARRAY:
      if (lda == WN_kid0(parent))
	 status = _Get_Lda_Access_Status(parent);
      else
	 status = ANL_VAR_READ | ANL_VAR_WRITTEN; // Whatever this means!
      break;

   case OPR_ILOAD:
      status = ANL_VAR_READ;
      break;

   case OPR_MLOAD:
      if (lda == WN_kid0(parent))
	 status = ANL_VAR_READ;
      else
	 status = ANL_VAR_READ | ANL_VAR_WRITTEN; // Whatever this means!
      break;

   case OPR_ISTORE:
      if (lda == WN_kid1(parent))
	 status = ANL_VAR_WRITTEN;
      else
	 status = ANL_VAR_READ | ANL_VAR_WRITTEN; // Whatever this means!
      break;

   case OPR_MSTORE:
      if (lda == WN_kid1(parent))
	 status = ANL_VAR_READ;
      else
	 status = ANL_VAR_READ | ANL_VAR_WRITTEN; // Whatever this means!
      break;

   case OPR_SUB:
   case OPR_ADD:
      status = _Get_Lda_Access_Status(parent); // Pointer arithmetics??
      break;

   case OPR_PARM:
      // Base this on flag values associated with the PARM node, instead
      // of specially recognizing intrinsics while feeling hopeless about
      // non-instrinsic calls.
      //
      if (WN_Parm_In(parent) || WN_Parm_Out(parent))
      {
	 if (WN_Parm_In(parent))
	    status = ANL_VAR_READ;
	 if (WN_Parm_Out(parent))
	    status = ANL_VAR_WRITTEN;
      }
      else
      {
	 status = ANL_VAR_READ | ANL_VAR_WRITTEN; // We have no idea
      }
      break;

   case OPR_IO_ITEM:
      // Base this on the kind of IO item.
      //
      status = _Get_Io_Item_Lda_Access_Status(parent);
      break;

   case OPR_XPRAGMA:
      if (WN_pragma(parent) == WN_PRAGMA_CRITICAL_SECTION_BEGIN)
         status = ANL_VAR_READ;
      else
         status = ANL_VAR_READ | ANL_VAR_WRITTEN;
      break;

   default:
      // We have no idea, so just assume it is both read and written.
      //
      status = ANL_VAR_READ | ANL_VAR_WRITTEN;
      break;
   }
   return status;
} // ANL_VARLIST::_Get_Lda_Access_Status


// =============== Public Member Functions ================
// ========================================================


void
ANL_VAR::Set_Name_Alias(ANL_VAR *var)
{
   if (this != var)
   {
      ANL_VAR *alias;

      // See if var is already in the alias list for this
      //
      for (alias = _alias; 
	   alias != var && alias != this; 
	   alias = alias->_alias);

      if (alias == this)
      {
	 // Combine the two disjoint alias cycles into one cycle.
	 //
	 ANL_VAR *next = _alias;

	 _alias = var;  // Open and connect this cycle to var cycle
	 for (alias = var->_alias; 
	      alias->_alias != var; 
	      alias = alias->_alias); // Gets to the end of the var cycle
	 alias->_alias = next; // Connect the var cycle with this cycle.
      }
   }
} // ANL_VAR::Set_Name_Alias


void 
ANL_VAR::Reset_References()
{
   _status = 0;
   for (ANL_VAR *var = _alias; var != this; var = var->_alias)
      var->_status = 0;
} // ANL_VAR::Reset_References


void 
ANL_VAR::Write(ANL_CBUF *cbuf, ANL_FUNC_ENTRY *func_entry)
{
   BOOL     read = Is_Read();
   BOOL     written = Is_Written();
   ANL_VAR *var;

   for (var = _alias; var != this; var = var->_alias)
   {
      read = read || var->Is_Read();
      written = written || var->Is_Written();
   }
   
   cbuf->Write_String(" \"");
   func_entry->Pu_Translator()->Original_Symname_To_String(cbuf, _st);
   cbuf->Write_String("\"(");
   Write_Sclass(cbuf, _st);
   cbuf->Write_String("):");
   if (read)
      cbuf->Write_Char('r');
   if (written)
      cbuf->Write_Char('w');
} // ANL_VAR::Write


ANL_VAR *
ANL_VARLIST::Find(ST *st)
{
   // Return NULL if not found.
   //
   ANL_VAR    *found;
   const INT32 id = ST_st_idx(st);
   mUINT32     idx = _Binary_Search(id, 0, _vlist.Size());
   if (idx < _vlist.Size() && id == _vlist.Indexed_Get(idx)->Id())
      found = _vlist.Indexed_Get(idx);
   else
      found = NULL;

   return found;
} // ANL_VARLIST::Find


ANL_VAR *
ANL_VARLIST::Find_or_Insert(ST *st)
{
   // Insert new ANL_VAR if not found.
   //
   ANL_VAR    *found;
   const INT32 id = ST_st_idx(st);

   mUINT32     idx = _Binary_Search(id, 0, _vlist.Size());
   if (idx < _vlist.Size() && id == _vlist.Indexed_Get(idx)->Id())
      found = _vlist.Indexed_Get(idx);
   else
   {
      ANL_VAR *var = CXX_NEW(ANL_VAR(st), _pool);
      BOOL     added;

      // Insert the new item
      //
      if (idx >= _vlist.Size())
      {
	 _vlist.Insert_Last(var, &added);
	 found = _vlist.Indexed_Get(_vlist.Size()-1);
      }
      else if (id < _vlist.Indexed_Get(idx)->Id())
      {
	 _vlist.Insert_Before(var, idx, &added);
	 found = _vlist.Indexed_Get(idx);
      }
      else
      {
	 _vlist.Insert_After(var, idx, &added);
	 found = _vlist.Indexed_Get(idx+1);
      }
      if (!added)
	 Anl_Diag->Error("Cannot insert element in variable list!!");
   }
   return found;
} // ANL_VARLIST::Find_or_Insert


void 
ANL_VARLIST::Insert_Var_Refs(WN *subtree)
{ 
   // Traverse every expression subtree in the PU to determine what
   // variables are referenced and how they are referenced.
   //
   for (WN_ITER *tree_iter = WN_WALK_TreeIter(subtree);
	tree_iter != NULL;
	tree_iter = WN_WALK_TreeNext(tree_iter))
   {
      UINT32   status;
      ANL_VAR *var;
      WN      *wn = WN_ITER_wn(tree_iter);

      switch (WN_operator(wn))
      {
      case OPR_LDID:
	 if (St_Belongs_In_Varlist(WN_st(wn)))
	 {
	    var = Find_or_Insert(WN_st(wn));

	    if (ANL_IS_REF_PARM_ADDR(WN_st(wn)) ||
		ST_pt_to_unique_mem(WN_st(wn)))
	    {
	       // Either a reference parameter or an adjustable array,
	       // where references are indirect.
	       //
	       status = _Get_Lda_Access_Status(wn);
	       if ((status & ANL_VAR_READ) != 0)
		  var->Set_Read();
	       if ((status & ANL_VAR_WRITTEN) != 0)
		  var->Set_Written();
	    }
	    else
	    {
	       var->Set_Read();
	    }
	 }
	 break;

      case OPR_STID:
	 if (St_Belongs_In_Varlist(WN_st(wn)) &&
	     !ST_pt_to_unique_mem(WN_st(wn)))
	 {
	    // Typically a unique memory pointer is due to an adjustable
	    // automatic variable.  We do not record the initial assignment
	    // to such a variable.
	    //
	    BOOL record_write = TRUE;

	    var = Find_or_Insert(WN_st(wn));

	    if (WN_operator(WN_kid0(wn)) == OPR_LDID)
	    {
	       // Detect compiler temporaries used as aliases for user-declared
	       // scalar variables (e.g. for parameters used in adjustable
	       // array bounds).
	       //
	       ANL_CBUF lhs(_pool);
	       ANL_CBUF rhs(_pool);
	       _func_entry->Pu_Translator()->
		  Original_Symname_To_String(&lhs, WN_st(wn));
	       _func_entry->Pu_Translator()->
		  Original_Symname_To_String(&rhs, WN_st(WN_kid0(wn)));

	       if (strcmp(lhs.Chars(), rhs.Chars()) == 0)
	       {
		  record_write = FALSE; // Copy-in of params into temporary
		  var->Set_Name_Alias(Find_or_Insert(WN_st(WN_kid0(wn))));
	       }
	    }
	    if (record_write)
	       var->Set_Written();
	 }
	 break;

      case OPR_LDA:
	 if (St_Belongs_In_Varlist(WN_st(wn)))
	 {
	    status = _Get_Lda_Access_Status(wn);
	    var = Find_or_Insert(WN_st(wn));
	    if ((status & ANL_VAR_READ) != 0)
	       var->Set_Read();
	    if ((status & ANL_VAR_WRITTEN) != 0)
	       var->Set_Written();
	 }
	 break;

      case OPR_PRAGMA:
	 if (WN_operator(wn) == OPR_PRAGMA)
	 {
	    switch (WN_pragma(wn))
	    {
	    case WN_PRAGMA_LOCAL:
	    case WN_PRAGMA_LASTLOCAL:
	    case WN_PRAGMA_SHARED:
	    case WN_PRAGMA_FIRSTPRIVATE:
	       break;  // No need to register a reference for these?

	    case WN_PRAGMA_COPYIN:
	       if (St_Belongs_In_Varlist(WN_st(wn)))
	       {
		  var = Find_or_Insert(WN_st(wn));
		  var->Set_Read();
	       }
	       break;
	    default:
	       break;
	    }
	 }
	 break;

      default:
	 break; // Nothing to do
      }
   }
} // ANL_VARLIST::Insert_Var_Refs


void 
ANL_VARLIST::Write(ANL_CBUF *cbuf, INT64 construct_id)
{
   const INT32 NUM_CHARS_PER_LINE = 72;
   BOOL     first_var_list_item = TRUE;
   INT      max_idx;
   ANL_CBUF tmpbuf(_pool);

   tmpbuf.Write_String("varlist ");
   tmpbuf.Write_Int(construct_id);
   for (INT var_idx = 0; var_idx < _vlist.Size(); var_idx++)
   {
      ANL_VAR *var = _vlist.Indexed_Get(var_idx);
      
      if (tmpbuf.Size() >= NUM_CHARS_PER_LINE)
      {
	 // Start a new "varlist"
	 //
	 tmpbuf.Write_Char('\n');
	 cbuf->Write_String(tmpbuf.Chars());
	 tmpbuf.Reset();
	 tmpbuf.Write_String("varlist ");
	 tmpbuf.Write_Int(construct_id);
	 first_var_list_item = TRUE;
      }

      // Write the next varlist item
      //
      if (var->Is_Read() || var->Is_Written())
      {
	 if (first_var_list_item)
	 {
	    first_var_list_item = FALSE;
	 }
	 else
	 {
	    tmpbuf.Write_String(",");
	 }
	 var->Write(&tmpbuf, _func_entry);
	 var->Reset_References();
      }
   }
   if (tmpbuf.Size() > 0)
      cbuf->Write_String(tmpbuf.Chars());
} // ANL_VARLIST::Write

