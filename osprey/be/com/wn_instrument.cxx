//-*-c++-*-


/*
 * Copyright 2003, 2004, 2005 PathScale, Inc.  All Rights Reserved.
 */

// ====================================================================
// ====================================================================
//
// Module: wn_instrument.cxx
// $Author: oscar $
//
// ====================================================================
//
// Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it would be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// Further, this software is distributed without any warranty that it
// is free of the rightful claim of any third person regarding
// infringement  or the like.  Any license provided herein, whether
// implied or otherwise, applies only to this software file.  Patent
// licenses, if any, provided herein do not apply to combinations of
// this program with other software, or any other product whatsoever.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
//
// Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
// Mountain View, CA 94043, or:
//
// http://www.sgi.com
//
// For further information regarding this notice, see:
//
// http://oss.sgi.com/projects/GenInfo/NoticeExplan
//
// ====================================================================
//
// Description:
//
// The procedures WN_Instrument and WN_Annotate, declared in
// wn_instrument.h, invoke the instrumentation and annotation phases of
// feedback collection.
//
// The class WN_INSTRUMENT_WALKER implements the instrumentation and
// annotation phases of feedback collection.  Only WN_Instrument and
// WN_Annotate should need to reference the WN_INSTRUMENT_WALKER class.
//
// Interface of WN_INSTRUMENT_WALKER to WN_Instrument and WN_Annotate:
//
// WN_INSTRUMENT_WALKER( BOOL instrumenting, PROFILE_PHASE phase );
//
//   If instrumenting is TRUE, insert instrumentation.
//   If instrumenting is FALSE, annotate the WHIRL with feedback data.
//   In either case, set phase number that the instrumenter is in.
//
// ~WN_INSTRUMENT_WALKER();
//
// void Tree_Walk( WN *wn );
//
//   Walk the tree and perform instrumentation or annotate feedback.
//
// ====================================================================
// ====================================================================



#ifdef USE_PCH
#include "be_com_pch.h"
#endif /* USE_PCH */

#pragma hdrstop
#define USE_STANDARD_TYPES

#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <stack>
#include <list>
#include <set>
#include <map>

// #include "stab.h"
#include "mempool.h"
#include "cxx_memory.h"
#include "tracing.h"
#include "config_opt.h"         // for Instrumentation_Enabled
#include "defs.h"
#include "wn.h"
#include "wn_map.h"
#include "wn_util.h"
#include "profile_com.h"
#include "instr_reader.h"
#include "targ_sim.h"
#include "wn_pragmas.h"
#include "ir_reader.h"		// fdump_tree
#include "glob.h"
#include "errors.h"
#include "be_symtab.h"
#include "symtab_utils.h"
#include "vho_lower.h"
#include "fb_whirl.h"
#include "wn_instrument.h"
#include "opt_du.h"
#include "opt_alias_mgr.h"
#include "dep_graph.h"
#include "cxx_template.h"
#include "prompf.h"
#include "ir_reader.h"
#include "wb_util.h"
#include "wb_buffer.h"
#include "wb_carray.h"
#include "const.h"
/* #include "wb_browser.h"
   #include "wb.h" */
#include "wb_ipl.h"
extern BOOL Epilog_Flag;
//BOOL Epilog_Flag=TRUE;
BOOL HasGotos;
BOOL InitTypes=FALSE;
TY_IDX profile_init_struct_ty_idx;
TY_IDX profile_gen_struct_ty_idx;
TY_IDX I4_ty;
TY_IDX I1_ty;
TY_IDX V_ty;
TY_IDX Ptr_ty;
TY_IDX VPtr_ty;
// Use this switch to turn on/off additional debugging messages

#define Instrumenter_DEBUG 0


// ALSO, SEE:
//   common/com/profile_com.h
//   common/com/instr_reader.h
//   common/instrument/instr_reader.cxx

// Invokes instrumentation:
//   be/be/driver.cxx (through WN_Instrument and wiw_wopt.Tree_Walk)



// ====================================================================
//
// WN_INSTRUMENT_WALKER class declaration
//
// ====================================================================
#define Set_Parent(wn, p) (WN_MAP_Set(Inst_Parent_Map, wn, (void*)  p))
#define Get_Parent(wn) ((WN*) WN_MAP_Get(Inst_Parent_Map, (WN*) wn))
BOOL is_mpi_or_pomp(char *s);

#ifdef KEY
static WN* pu_wn = NULL;
#endif

#define TNV_N 10

class WN_INSTRUMENT_WALKER {
private:

  // ------------------------------------------------------------------
  // Private members of WN_INSTRUMENT_WALKER should be accessed only
  // through the private methods provided further below.
  // ------------------------------------------------------------------

  // _mempool is the local memory pool

  MEM_POOL *         _mempool;
  WN_MAP             Inst_Parent_Map;
  // Phase at which instrumentation/annotation is occuring
  //  (PROFILE_PHASE declared in common/com/profile_com.h)

  PROFILE_PHASE      _phase;

  // _instrumenting is TRUE  if inserting instrumentation
  // _instrumenting is FALSE if annotating WHIRL with feedback data
  // _vho_lower is TRUE iff introduced instrumentation code requires
  //   VHO Lowering after tree walk finishes (to lower commas)
  // _in_preamble is TRUE iff the tree walker is currently within a
  //   PU's preamble (after FUNCT_ENTRY or ALTENTRY and before the
  //   WN_PRAGMA_PREAMBLE_END pragma.  The tree walker should skip all
  //   WHIRL nodes within a preamble.

  BOOL               _instrumenting;
  BOOL               _vho_lower;
  BOOL               _in_preamble;
  // Counter for each type of instrumentation

  UINT32             _count_return;
  UINT32             _count_invoke;
  UINT32             _count_branch;
  UINT32             _count_loop;
  UINT32             _count_circuit;
  UINT32             _count_call;
  UINT32             _count_icall;
  UINT32             _count_switch;
  UINT32             _count_compgoto;
  UINT32             _fb_count_icall;
#ifdef KEY
  UINT32             _count_value;
  UINT32             _count_value_fp_bin;
#endif

  // _instrument_count is total number of WHIRL nodes that have been
  //   instrumented/annotated; used as a checksum

  UINT32             _instrument_count;

  // _pu_handle is the temp storing the Handle for all profile calls in PU
  // _fb_handle is a list of handles for feedback info for this PU

  ST                *_pu_handle;
  PU_PROFILE_HANDLES _fb_handle;
  //_fb_handle_merged is a handle only for merged icall feedback info for this PU.
  PU_PROFILE_HANDLE _fb_handle_merged;
  ST               *_profile_init_struct_st;
  ST               *_profile_gen_struct_st; 
  // Instrumentation initialization code will be inserted just before
  // the WN_PRAGMA_PREAMBLE_END pragma that occurs after each entry's
  // preamble.  During the tree walk:
  // _entry_pragma_stmt and _entry_pragma_block are NULL if no
  //   WN_PRAGMA_PREAMBLE_END pragma have been encountered yet.
  //   Otherwise _entry_pragma_stmt holds one WN_PRAGMA_PREAMBLE_END
  //   pragma, and _entry_pragma_block is the BLOCK in which it appears.
  // _other_entry_pragmas contains (stmt, block) pairs for all other
  //   WN_PRAGMA_PREAMBLE_END pragmas that have been encountered.
  // These three members should only be accessed through the private
  // methods provided further below.

  typedef mempool_allocator<WN *>  ALLOC_TYPE;
  typedef std::deque<WN *, ALLOC_TYPE > DEQUE_TYPE;
  typedef std::stack<WN *, DEQUE_TYPE > STACK_TYPE;

  WN *               _entry_pragma_stmt;
  WN *               _entry_pragma_block;
  STACK_TYPE         _other_entry_pragmas;

  // _instrumentation_nodes contains all instrumentation WHIRL nodes
  //   that were inserted into the WHIRL tree but have not yet been
  //   passed by the tree walker.  Instrumentation nodes should not
  //   be instrumented or annotated.
  // This vector should only be accessed through the private methods
  //   provided further below.

  vector<WN *, mempool_allocator<WN *> >   _instrumentation_nodes;

  // SWITCH and COMPGOTO case information

  vector<INT32, mempool_allocator<INT32> > _switch_num_targets;
  vector<INT64, mempool_allocator<INT64> > _switch_case_values;
  vector<INT32, mempool_allocator<INT32> > _compgoto_num_targets;

  // ------------------------------------------------------------------
  // Undefined default methods made private to detect errors
  // ------------------------------------------------------------------

  WN_INSTRUMENT_WALKER(void);
  WN_INSTRUMENT_WALKER(const WN_INSTRUMENT_WALKER&);
  WN_INSTRUMENT_WALKER& operator=(const WN_INSTRUMENT_WALKER&);

  void Create_Struct_Types()
 {
     I4_ty = Be_Type_Tbl(MTYPE_I4);
     I1_ty  = Be_Type_Tbl(MTYPE_I1);
     V_ty = Be_Type_Tbl(MTYPE_V);
     Ptr_ty = Make_Pointer_Type(I1_ty);
     VPtr_ty = Make_Pointer_Type(V_ty);

   // profile_init_struct declaration
   {
     FLD_HANDLE first_field = New_FLD ();
     FLD_Init (first_field, Save_Str("output_filename"),
               Ptr_ty , 0);
   
     FLD_HANDLE field = New_FLD ();
     FLD_Init (field, Save_Str("phase_num"),I4_ty,8);

     field = New_FLD ();
     FLD_Init (field, Save_Str("unique_name"),I4_ty,12);
   
     Set_FLD_last_field(field);


   /* TY_IDX feedback_struct_ty_idx; */
     TY& profile_init_struct_ty = New_TY(profile_init_struct_ty_idx);
     TY_Init (profile_init_struct_ty, 2*TY_size(I4_ty)+TY_size(Ptr_ty),
               KIND_STRUCT,MTYPE_M, Save_Str("PROFILE_INIT_STRUCT_TY"));

     Set_TY_fld(profile_init_struct_ty, first_field);
     Set_TY_align (profile_init_struct_ty_idx,8); 
   }  
   // profile_invoke_struct
    {
      int field_num =0;
     FLD_HANDLE first_field = New_FLD ();
     FLD_Init (first_field, Save_Str("pu_handle"),
               VPtr_ty , field_num);
     field_num+=8;
   
     FLD_HANDLE field = New_FLD ();
     FLD_Init (field, Save_Str("file_name"),Ptr_ty,field_num);
     field_num+=8;

     field = New_FLD ();
     FLD_Init (field, Save_Str("pu_name"),Ptr_ty,field_num);
     field_num+=8;   
 
    field = New_FLD ();
     FLD_Init (field, Save_Str("id"),I4_ty,field_num);
     field_num+=4;


     field = New_FLD ();
     FLD_Init (field, Save_Str("linenum"),I4_ty,field_num);
     field_num+=4;

     field = New_FLD ();
     FLD_Init (field, Save_Str("endline"),I4_ty,field_num);
     field_num+=4;

     field = New_FLD ();
     FLD_Init (field, Save_Str("subtype"),I4_ty,field_num);
     field_num+=4;

      field = New_FLD ();
     FLD_Init (field, Save_Str("taken"),I4_ty,field_num);
     field_num+=4;

      field = New_FLD ();
     FLD_Init (field, Save_Str("callee"),Ptr_ty,field_num);
     field_num+=8;

     field = New_FLD ();
     FLD_Init (field, Save_Str("target"),I4_ty,field_num);
     field_num+=4;

     field = New_FLD ();
     FLD_Init (field, Save_Str("num_target"),I4_ty,field_num);
     field_num+=4;

     field = New_FLD();
     FLD_Init (field, Save_Str("called_fun_address"),VPtr_ty,field_num);
     field_num+=8;

     field = New_FLD();
     FLD_Init (field, Save_Str("data"),VPtr_ty,field_num);
     field_num+=8;

     //field = New_FLD();
     //FLD_Init (field, Save_Str("region_id"),I4_ty,field_num);
     //field_num+=4;

     Set_FLD_last_field(field);

     TY& profile_gen_struct_ty = New_TY(profile_gen_struct_ty_idx);
     TY_Init (profile_gen_struct_ty, 7*TY_size(I4_ty)+3*TY_size(Ptr_ty)
              + 3*TY_size(VPtr_ty)/*+4 *alignment */, KIND_STRUCT,MTYPE_M, Save_Str("PROFILE_GEN_STRUCT_TY"));

     Set_TY_fld(profile_gen_struct_ty, first_field);
     Set_TY_align (profile_gen_struct_ty_idx,8); 
   }

   InitTypes=TRUE;
 }

 WN * Load_Struct(ST *feedback_struct_st)
  {
    TYPE_ID Struct_type = MTYPE_M;
    WN *load_wn = WN_Lda( Pointer_type, 0, feedback_struct_st);
    return load_wn; 
  }

WN * Init_Struct(ST *feedback_struct_st,  WN *field, TY_IDX type, int field_offset )
 {
   WN *store_wn;  
  
     if (type == Ptr_ty || type == VPtr_ty ) {
       OPCODE store_op = OPCODE_make_op(OPR_STID, MTYPE_M, MTYPE_U8);
       store_wn = WN_CreateStid (store_op,
                                     field_offset, /* field offset */
			             feedback_struct_st,
			             type,
                                     field);
     }
       else if(type == I4_ty) {
    
      OPCODE store_op = OPCODE_make_op(OPR_STID, MTYPE_M, MTYPE_I4);
      store_wn = WN_CreateStid (store_op,
                                     field_offset, /* field offset */
			             feedback_struct_st,
			             type,
                                     field);
      
   }
    

   return store_wn;
 }
 void Parentize (WN* wn)
{
  if (!OPCODE_is_leaf(WN_opcode(wn))) { 
    if (WN_opcode(wn) == OPC_BLOCK) {
      WN *kid = WN_first(wn);
      while (kid) {
        Set_Parent(kid, wn);
        Parentize(kid);
        kid = WN_next(kid);
      }
    } else {
      INT kidno;
      for (kidno = 0; kidno < WN_kid_count(wn); kidno++) {
        WN *kid = WN_kid(wn, kidno);
        if (kid) { 
          Set_Parent(kid, wn);
          Parentize(kid);
        }
      }
    }
  }
}

void find_endline (WN *wn, INT32 &endline)
{

  // sort line# of procedures
  // check pos is the sanme as the procedure filename
  // check that it is in range.
 
   endline = 0;
   WN_ITER *wn_iter;
   for (wn_iter = WN_WALK_TreeIter(wn);
        wn_iter != NULL;
        wn_iter = WN_WALK_TreeNext(wn_iter))
   {
           
      SRCPOS srcpos = WN_Get_Linenum(WN_ITER_wn(wn_iter));
      USRCPOS linepos;
      USRCPOS_srcpos(linepos) = srcpos;
      INT32 line = USRCPOS_linenum(linepos);
      if (line>endline)
        endline = line; 

   }


}

BOOL meets_nesting_level(WN *wn2)
{
   BOOL meets;
   unsigned int looplevel = 1;
   
   if( Instrumentation_Nesting_Level==-1) 
       return TRUE; //Early exit when Nesting level is disabled.

   while(Get_Parent(wn2)!=NULL)
      {
        wn2 = Get_Parent(wn2);
        OPERATOR opr = WN_operator( wn2 );
        if (opr==OPR_DO_LOOP || opr== OPR_WHILE_DO || opr == OPR_DO_WHILE)
         looplevel++;
      }

   if(looplevel <= Instrumentation_Nesting_Level)
       meets=TRUE;
   else
       meets=FALSE;
     
   return meets;  
}
BOOL selective_loops(int id, WN *wn2)
{
    BOOL select=meets_nesting_level(wn2);
    switch (id)
   {
/*     case 133:
     case 57:
     case 94:
     case 104:
     case 48:
     case 8:
     case 14:
     case 5:
     case 2:
     case 63:
     case 20:
     case 60:
     case 66:
     case 18:
     case 101:
     case 16: 
     case 98:
     select = TRUE;
     break;
 */  
   }
    
  return select;
}

void region_subtype(WN *wn, WN *&region_type, int &subtype)
{

   typedef enum 
   { 
     IF_THEN_SUBTYPE=1,
     IF_THEN_ELSE_SUBTYPE=2,
     IF_UNK_SUBTYPE=3,
     TRUEBR_SUBTYPE=4,
     FALSEBR_SUBTYPE=5,
     CSELECT_SUBTYPE=6
   } BranchSubType;

  typedef enum
  {   
    DO_LOOP_SUBTYPE=1,
    WHILE_DO_SUBTYPE=2,
    DO_WHILE_SUBTYPE=3

  } LoopSubType;

  typedef enum
  {
    PICCALL_SUBTYPE=1,
    CALL_SUBTYPE=2,
    INTRINSIC_CALL_SUBTYPE=3,
    IO_SUBTYPE=4

  } CallSubType;

  typedef enum
  {
    CAND_SUBTYPE=1,   
    CIOR_SUBTYPE=2

  } CircuiteSubType;

  typedef enum
  { 
    COMPGOTO_SUBTYPE=1,
    XGOTO_SUBTYPE=2
  } GotoSubType; 

      
    OPERATOR op = WN_operator(wn);
    subtype=-1;
    BOOL assigned = FALSE;
    switch (op)
    {
      case OPR_IF:
	if (WN_else(wn))
	{   
          subtype = IF_THEN_ELSE_SUBTYPE;
	   char name[]="IF_THEN_ELSE"; 
	   // region_type = WN_LdaString(name, 0, strlen(name) + 1 );

         }
	  else if (WN_then(wn))
	 {
          

           subtype = IF_THEN_SUBTYPE; 
	  char name[]="IF_THEN";
          //region_type = WN_LdaString(name, 0, strlen(name) + 1 );

         }
	else
        {
          subtype = IF_UNK_SUBTYPE;
	  char name[]="IF_UNK"; 
          // region_type = WN_LdaString(name, 0, strlen(name) + 1 );

	}
	assigned = TRUE;
      break;
     
   
    case OPR_TRUEBR: 
    subtype = TRUEBR_SUBTYPE;
    break;
    case OPR_FALSEBR:
      subtype = FALSEBR_SUBTYPE;
    break;

    case OPR_CSELECT:
      subtype = CSELECT_SUBTYPE; 
    break; 

    case OPR_DO_LOOP:
      subtype = DO_LOOP_SUBTYPE;
    break;

    case OPR_WHILE_DO:
      subtype = WHILE_DO_SUBTYPE;
    break; 

    case OPR_DO_WHILE:
      subtype = DO_WHILE_SUBTYPE; 
    break;

    case OPR_CAND:
      subtype = CAND_SUBTYPE;
    break;

    case OPR_CIOR:
      subtype = CIOR_SUBTYPE;
    break;

    case OPR_PICCALL:
      subtype = PICCALL_SUBTYPE; 
    break;

    case OPR_CALL:
      subtype = CALL_SUBTYPE;
    break;

    case OPR_INTRINSIC_CALL:
      subtype = INTRINSIC_CALL_SUBTYPE; 
    break;

    case OPR_IO:
      subtype = IO_SUBTYPE;
    break;

    case OPR_COMPGOTO:
      subtype = COMPGOTO_SUBTYPE; 
    break;

    case OPR_XGOTO:
      subtype = XGOTO_SUBTYPE;
    break;
    
    default:
    break;
    }
    /*
    if(!assigned)
      {  

        char *val = NULL; 
        char *name = NULL;   
        val  = new char[strlen(OPERATOR_name(op))+1];
        strcpy(val,OPERATOR_name(op));

        if(strlen(val)>4)
       { 
         name = &val[4];
       } 
        else
       {
         name = val; 
        }
        region_type = WN_LdaString(name, 0, strlen(name) + 1 );
        if(val != NULL)
        delete [] val;
     
	} */


} 

INITV_IDX INITO_to_Integer(INITV_IDX next_inv, TYPE_ID mytype, INT64 value)
{
    INITV_IDX inv_temp = next_inv;   
    next_inv = New_INITV();
    //    INITV_Init_Pad(next_inv,TY_size(Be_Type_Tbl(mytype)));
    INITV_Init_Integer(next_inv,mytype,value);   // id //
   
    Set_INITV_next(inv_temp,next_inv);
    return next_inv;

}

INITV_IDX INITO_to_String(INITV_IDX next_inv, char *s)
{
    int size = strlen(s)+1;
    INITV_IDX inv_temp = next_inv;
    next_inv = New_INITV();
    INITV_Init_Sym_String(next_inv,s,size);
    Set_INITV_next(inv_temp,next_inv);
    return next_inv;

}

  
void INITV_Init_Sym_String(INITV_IDX next_inv, char *s, int size)
{
  TCON tc = Host_To_Targ_String (MTYPE_STR, s, size);
  ST *st = New_Const_Sym(Enter_tcon(tc),MTYPE_To_TY(MTYPE_STR));

  // ST *st = Gen_String_Sym (&tc, MTYPE_To_TY(MTYPE_STR), FALSE );
  INITV_Init_Symoff(next_inv,st,0);

}

ST * create_struct_st(TY_IDX temp_ty_idx)
{
  ST *temp_st;
  if (temp_ty_idx == profile_init_struct_ty_idx)
  {
    temp_st = Gen_Temp_Named_Symbol(profile_init_struct_ty_idx, "profile_init_struct", 
                               CLASS_VAR,SCLASS_PSTATIC);

  }
  else if (temp_ty_idx == profile_gen_struct_ty_idx) {
 
    temp_st = Gen_Temp_Named_Symbol(profile_gen_struct_ty_idx, "profile_gen_struct",
                                CLASS_VAR,SCLASS_PSTATIC);
  }

    Set_ST_is_temp_var(temp_st);
  
    return temp_st;

}


void location(WN *wn,WN *&source_file, INT32 &line, char *&fullpath, BOOL gen_wn=TRUE)
{
  /* this procedure needs to be cleaned badly... */
  SRCPOS srcpos = WN_Get_Linenum(wn);
  USRCPOS linepos;
  USRCPOS_srcpos(linepos) = srcpos;
  line = USRCPOS_linenum(linepos);
   char *filename=NULL, *dirname=NULL;
  IPA_IR_Filename_Dirname(srcpos,filename,dirname);
 
  if(filename !=NULL && dirname !=NULL)
  {
    if(gen_wn) {
    fullpath = new char[strlen(filename)+strlen(dirname)+2];
    strcpy(fullpath,dirname);
    strcat(fullpath,"/");
    strcat(fullpath,filename);
    // fullpath = new char[strlen(filename)+1];
    // strcpy(fullpath,filename);
    }
    //source_file = WN_LdaString(fullpath, 0, strlen(fullpath ) + 1 );
    //delete [] fullpath;
   }
   else
   {  
     if(gen_wn) { 
     char message[]="filename information not available";
     fullpath = new char[strlen(message)+1];
     strcpy(fullpath,message);
     } 
     
     //  source_file = WN_LdaString(fullpath, 0, strlen(fullpath ) + 1 );

    }
   if(filename!=NULL)
  delete [] filename;
  if(dirname!=NULL)
  delete [] dirname;


}
  // ------------------------------------------------------------------

  // Get the PU handle.
  WN *PU_Handle() const {
    return WN_Ldid( Pointer_type, 0, _pu_handle, MTYPE_To_TY(Pointer_type));
  }

  // Get the feedback handle.
  PU_PROFILE_HANDLES& FB_Handle() { return _fb_handle; }

  // Is the feedback handle empty?
public:
  BOOL FB_Handle_Empty() { return _fb_handle.empty(); }
private:

  // Perform VHO_Lower on the tree after finishing tree walk
  void Set_VHO_Lower_Tree() { _vho_lower = TRUE; }

  // ------------------------------------------------------------------
  // List of WN_PRAGMA_PREAMBLE_END pragma WHIRL nodes and BLOCKs
  // ------------------------------------------------------------------

  // During the instrumention tree walk, the WN_PRAGMA_PREAMBLE_END
  // pragma WHIRL node for each PU entry (along with the BLOCK that
  // contains it) is stored in a list.  After the tree walk,
  // instrumentation initialization code is inserted at the end of each
  // entry's preamble.
  //
  // Implementation: The top pragma statement and block are kept in
  // _entry_pragma_stmt and _entry_pragma_block.  Any other pragmas are
  // stored in _other_entry_pragmas.  Since most PUs only have one
  // entry point, _other_entry_pragmas is usually not used.
  //
  // The methods below maintain the list of pragma nodes and blocks:
  //
  // Entry_List_Empty returns TRUE iff the list is empty.
  // Entry_Pragma returns the pragma at the top of the list.
  // Entry_Block returns the block which contains the Entry_Pragma.
  //   (If the list is empty, Entry_Pragma and Entry_Block return NULL)
  // Pop_Entry_Pragma discards the top pragma and block from the list.
  // Push_Entry_Pragma inserts a pragma and its block into the list.

  BOOL Entry_List_Empty() const { return _entry_pragma_stmt == NULL; }
  WN * Entry_Pragma()     const { return _entry_pragma_stmt; }
  WN * Entry_Block()      const { return _entry_pragma_block; }

  void Pop_Entry_Pragma() {
    if ( _other_entry_pragmas.empty() )
      _entry_pragma_stmt  = _entry_pragma_block = NULL;
    else {
      _entry_pragma_stmt  = _other_entry_pragmas.top();
      _other_entry_pragmas.pop();
      _entry_pragma_block = _other_entry_pragmas.top();
      _other_entry_pragmas.pop();
    }
  }

  void Push_Entry_Pragma( WN *stmt, WN *block ) {
    Is_True( stmt != NULL, ( "WN_INSTRUMENT_WALKER::Push_Entry_Pragma"
			     " stmt is NULL" ) );
    if ( _entry_pragma_stmt == NULL ) {
      _entry_pragma_stmt  = stmt;
      _entry_pragma_block = block;
    } else {
      _other_entry_pragmas.push( block );
      _other_entry_pragmas.push( stmt );
    }
  }

  // ------------------------------------------------------------------
  // List of instrumentation nodes that have not yet been passed
  // ------------------------------------------------------------------

  // _instrumentation_nodes contains all instrumentation WHIRL nodes
  // that were inserted into the WHIRL tree but have not yet been
  // passed by the tree walker.  Instrumentation nodes should not be
  // instrumented or annotated.  The following methods search and
  // update that list:

  // Record_Instrument_Node appends wn to the list of not-yet-passed
  //   instrumentation nodes.  It should be invoked on any node
  //   inserted _after_ the tree walker's current position.
  // Test_Instrument_Node returns TRUE iff wn is in the list of WHIRL
  //   not-yet-passed instrumentation nodes.  If Test_Instrument_Node
  //   does find a match, it removes that node from the list, so it
  //   should only be called once per node.

  void Record_Instrument_Node( WN *wn ) {
    _instrumentation_nodes.push_back( wn );
  }

  BOOL Test_Instrument_Node( WN *wn ) {
    INT t, last = _instrumentation_nodes.size() - 1;
    // Search from back to front, since next match is usually at end
    // reverse order
    for ( t = last; t >= 0; --t )
      if ( _instrumentation_nodes[t] == wn ) {
	_instrumentation_nodes[t] = _instrumentation_nodes[last];
	_instrumentation_nodes.pop_back();
	return TRUE;
      }
    return FALSE;
  }

  // ------------------------------------------------------------------
  // Methods to insert the instrumentation code into the WHIRL tree
  // ------------------------------------------------------------------

  // Is_Return_Store_Stmt returns TRUE iff the statement wn (after a
  //   call) is saving the return value to a pseudo-register.
  //   Instrumentation cannot be placed between a call and such a
  //   statement.
  // Is_Return_Store_Comma returns TRUE iff the statement wn (after a
  //   call) is a comma returning the return value of the last call in
  //   the comma's block.  Instrumentation cannot be placed after the
  //   call in the block.
  // Test_Dedicated_Reg returns TRUE iff the tree rooted at wn refers
  //   to a hardware register that instrumentation may overwrite.
  // Instrument_Before inserts an instrumentation WHIRL node in the
  //   current block before the current statement.
  // Instrument_After inserts an instrumentation WHIRL node in the
  //   current block before the current statement.
  // Instrument_Entry inserts an instrumentation WHIRL node at the end
  //   of the preamble of each of the program unit's entry points.
  // Create_Comma_Kid ensures that WN_kid( wn, kid_idx ) is a COMMA
  //   WHIRL node, so that instrumentation code can be inserted into
  //   it.  Create_Comma_Kid returns a pointer to the COMMA node.

  BOOL Is_Return_Store_Stmt( WN *wn );
  BOOL Is_Return_Store_Comma( WN *wn );
  BOOL Test_Dedicated_Reg( WN *wn );

  void Instrument_Before( WN *call, WN *current_stmt, WN *block );
  void Instrument_After( WN *call, WN *current_stmt, WN *block );
  void Instrument_Entry( WN *call );

  WN *Create_Comma_Kid( WN *wn, INT kid_idx );

  // ------------------------------------------------------------------
  // Instrumentation and Annotation of each type of WHIRL node
  // ------------------------------------------------------------------
  void Instrument_With_Gen_Struct( int initialized,int mode, WN *instr, WN *wn, WN *block, char *file_name, 
                                  char *pu_name, int id, int linenum, int endline, int subtype, WN *taken, char *callee,
                                  WN *target, int num_targets, WN *called_fun_address);
  void Instrument_Special_Returns(WN *wn, WN *stmt, WN *block, INT32 id, INT32 enum_subtype, INT32 line, INT32 endline, WN *source_file, BOOL branch);
  void IsGotos(WN *wn, WN *stmt, WN *block);
  void Instrument_Invoke_Exit( WN *wn, INT32 id, WN *block, int after = 0 );
  void Instrument_Invoke( WN *wn, INT32 id, WN *block );
  void Initialize_Instrumenter_Invoke( INT32 count );
  void Annotate_Invoke( WN *wn, INT32 id );
  void Instrument_Branch( WN *wn, INT32 id, WN *block, WN *stmt );
  void Instrument_Cselect( WN *wn, INT32 id, WN *block );
  void Initialize_Instrumenter_Branch( INT32 count );
  void Annotate_Branch( WN *wn, INT32 id );
  void Instrument_Loop( WN *wn, INT32 id, WN *block, WN *stmt );
  void Initialize_Instrumenter_Loop( INT32 count );
  void Annotate_Loop( WN *wn, INT32 id );
  void Instrument_Circuit( WN *wn, INT32 id );
  void Initialize_Instrumenter_Circuit( INT32 count );
  void Annotate_Circuit( WN *wn, INT32 id );
  void Instrument_Call( WN *wn, INT32 id, WN *block );
  void Initialize_Instrumenter_Call( INT32 count );
  void Annotate_Call( WN *wn, INT32 id );
  void Instrument_Icall( WN *wn, INT32 id, WN *block );
  void Initialize_Instrumenter_Icall( INT32 count );
  void Annotate_Icall( WN *wn, INT32 id );
  void Instrument_Switch( WN *wn, INT32 id, WN *block );
  void Initialize_Instrumenter_Switch( INT32 count );
  void Annotate_Switch( WN *wn, INT32 id );
  void Instrument_Compgoto( WN *wn, INT32 id, WN *block );
  void Initialize_Instrumenter_Compgoto( INT32 count );
  void Annotate_Compgoto( WN *wn, INT32 id );
#ifdef KEY
  void Initialize_Instrumenter_Value( INT32 count );
  void Instrument_Value( WN *wn, INT32 id, WN *block );
  void Annotate_Value( WN *wn, INT32 id );
  void Initialize_Instrumenter_Value_FP_Bin( INT32 count );
  void Instrument_Value_FP_Bin( WN *wn, INT32 id, WN *block, WN *parent, 
				WN *stmt );
  void Annotate_Value_FP_Bin( WN *wn, INT32 id );
#endif

  // ------------------------------------------------------------------

  // Walk the tree and perform instrumentation or annotate feedback
#ifndef KEY
  void Tree_Walk_Node( WN *wn, WN *stmt, WN *block );
#else
  void Tree_Walk_Node( WN *wn, WN *stmt, WN *block, WN* parent = NULL );
#endif
   
protected:
  void Merge_Icall_Feedback();

  // ------------------------------------------------------------------
  // Public interface to WN_Instrument and WN_Annotate
  // ------------------------------------------------------------------

public:

  // If instrumenting is TRUE, insert instrumentation.
  // If instrumenting is FALSE, annotate the WHIRL with feedbackd data.
  // In either case, set phase number that the instrumenter is in.

  WN_INSTRUMENT_WALKER( BOOL instrumenting, PROFILE_PHASE phase,
			MEM_POOL *local_mempool,
			PU_PROFILE_HANDLES fb_handles );
  ~WN_INSTRUMENT_WALKER() {}

  // Walk the tree and perform instrumentation or annotate feedback.

  void Tree_Walk( WN *wn );
};


// ====================================================================
//
// Utility functions to generate calls to instrumentation functions.
//
// ====================================================================


WN *
Gen_Call_Shell( char *name, TYPE_ID rtype, INT32 argc )
{
  TY_IDX  ty = Make_Function_Type( MTYPE_To_TY( rtype ) );
  ST     *st = Gen_Intrinsic_Function( ty, name );

  Clear_PU_no_side_effects( Pu_Table[ST_pu( st )] );
  Clear_PU_is_pure( Pu_Table[ST_pu( st )] );
  Set_PU_no_delete( Pu_Table[ST_pu( st )] );

  WN *wn_call = WN_Call( rtype, MTYPE_V, argc, st );

  WN_Set_Call_Default_Flags(  wn_call );
  // WN_Reset_Call_Non_Parm_Mod( wn_call );
  // WN_Reset_Call_Non_Parm_Ref( wn_call );
  
  return wn_call;
}  


WN *
Gen_Call( char *name, TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 0 );
  return call;
}


inline WN *
Gen_Param( WN *arg, UINT32 flag )
{
  return WN_CreateParm( WN_rtype( arg ), arg,
			MTYPE_To_TY( WN_rtype( arg ) ), flag );
}


WN *
Gen_Call( char *name, WN *arg1, TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 1 );
  WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
  return call;
}


WN *
Gen_Call( char *name, WN *arg1, WN *arg2, TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 2 );
  WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
  WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
  return call;
}


WN *
Gen_Call( char *name, WN *arg1, WN *arg2, WN *arg3, TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 3 );
  WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
  WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
  WN_actual( call, 2 ) = Gen_Param( arg3, WN_PARM_BY_VALUE );
  return call;
}


WN *
Gen_Call( char *name, WN *arg1, WN *arg2, WN *arg3, WN *arg4,
	  TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 4 );
  WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
  WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
  WN_actual( call, 2 ) = Gen_Param( arg3, WN_PARM_BY_VALUE );
  WN_actual( call, 3 ) = Gen_Param( arg4, WN_PARM_BY_VALUE );
  return call;
}


WN *
Gen_Call( char *name, WN *arg1, WN *arg2, WN *arg3, WN *arg4,
	  WN *arg5, TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 5 );
  WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
  WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
  WN_actual( call, 2 ) = Gen_Param( arg3, WN_PARM_BY_VALUE );
  WN_actual( call, 3 ) = Gen_Param( arg4, WN_PARM_BY_VALUE );
  WN_actual( call, 4 ) = Gen_Param( arg5, WN_PARM_BY_VALUE );
  return call;
}


WN *
Gen_Call( char *name, WN *arg1, WN *arg2, WN *arg3, WN *arg4,
	  WN *arg5, WN *arg6, TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 6 );
  WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
  WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
  WN_actual( call, 2 ) = Gen_Param( arg3, WN_PARM_BY_VALUE );
  WN_actual( call, 3 ) = Gen_Param( arg4, WN_PARM_BY_VALUE );
  WN_actual( call, 4 ) = Gen_Param( arg5, WN_PARM_BY_VALUE );
  WN_actual( call, 5 ) = Gen_Param( arg6, WN_PARM_BY_VALUE );
  return call;
}

WN *
Gen_Call( char *name, WN *arg1, WN *arg2, WN *arg3, WN *arg4,
	  WN *arg5, WN *arg6, WN *arg7, TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 7 );
  WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
  WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
  WN_actual( call, 2 ) = Gen_Param( arg3, WN_PARM_BY_VALUE );
  WN_actual( call, 3 ) = Gen_Param( arg4, WN_PARM_BY_VALUE );
  WN_actual( call, 4 ) = Gen_Param( arg5, WN_PARM_BY_VALUE );
  WN_actual( call, 5 ) = Gen_Param( arg6, WN_PARM_BY_VALUE );
  WN_actual( call, 6 ) = Gen_Param( arg7, WN_PARM_BY_VALUE );
  return call;
}

WN *
Gen_Call( char *name, WN *arg1, WN *arg2, WN *arg3, WN *arg4,
	  WN *arg5, WN *arg6, WN *arg7, WN *arg8, TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 8 );
  WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
  WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
  WN_actual( call, 2 ) = Gen_Param( arg3, WN_PARM_BY_VALUE );
  WN_actual( call, 3 ) = Gen_Param( arg4, WN_PARM_BY_VALUE );
  WN_actual( call, 4 ) = Gen_Param( arg5, WN_PARM_BY_VALUE );
  WN_actual( call, 5 ) = Gen_Param( arg6, WN_PARM_BY_VALUE );
  WN_actual( call, 6 ) = Gen_Param( arg7, WN_PARM_BY_VALUE );
  WN_actual( call, 7 ) = Gen_Param( arg8, WN_PARM_BY_VALUE );
  return call;
}

// Some parameters are by reference, not by value


WN *
Gen_Call_ref3( char *name, WN *arg1, WN *arg2, WN *arg3, 
	       TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 3 );
  WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
  WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
  WN_actual( call, 2 ) = Gen_Param( arg3, WN_PARM_BY_REFERENCE );
  return call;
}


WN *
Gen_Call_ref35( char *name, WN *arg1, WN *arg2, WN *arg3, WN *arg4,
		WN *arg5, TYPE_ID rtype = MTYPE_V )
{
  WN *call = Gen_Call_Shell( name, rtype, 5 );
  WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
  WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
  WN_actual( call, 2 ) = Gen_Param( arg3, WN_PARM_BY_REFERENCE );
  WN_actual( call, 3 ) = Gen_Param( arg4, WN_PARM_BY_VALUE );
  WN_actual( call, 4 ) = Gen_Param( arg5, WN_PARM_BY_REFERENCE );
  return call;
}


// ====================================================================


WN_INSTRUMENT_WALKER::WN_INSTRUMENT_WALKER( BOOL instrumenting,
					    PROFILE_PHASE phase,
					    MEM_POOL *local_mempool,
					    PU_PROFILE_HANDLES fb_handles )
  : _mempool( local_mempool ),
    _phase( phase ),
    _instrumenting( instrumenting ),
    _vho_lower( FALSE ),
    _in_preamble( FALSE ),
    _count_return( 0 ),
    _count_invoke( 0 ),
    _count_branch( 0 ),
    _count_loop( 0 ),
    _count_circuit( 0 ),
    _count_call( 0 ),
    _count_icall( 0 ),
    _count_switch( 0 ),
    _count_compgoto( 0 ),
#ifdef KEY
    _count_value( 0 ),
    _count_value_fp_bin( 0 ),
#endif
    _instrument_count( 0 ),
    _pu_handle( NULL ),
    _fb_handle( fb_handles ),
    _fb_handle_merged( NULL ),
    _entry_pragma_stmt( NULL ),
    _entry_pragma_block( NULL ),
    _other_entry_pragmas( DEQUE_TYPE( ALLOC_TYPE( local_mempool ) ) ),
    _instrumentation_nodes( local_mempool ),
    _switch_num_targets( local_mempool ),
    _switch_case_values( local_mempool ),
    _compgoto_num_targets( local_mempool ),
    _profile_init_struct_st( NULL),
     _profile_gen_struct_st(NULL),
    Inst_Parent_Map(WN_MAP_UNDEFINED)
{
  if ( _instrumenting ) {
    _pu_handle = Gen_Temp_Symbol(MTYPE_To_TY(Pointer_type), "pu_instrument_handle");
    if (!InitTypes) 
         Create_Struct_Types();
   
  }

  /*
    _pu_handle = Gen_Temp_Named_Symbol(MTYPE_To_TY(Pointer_type), "pu_instrument_handle", CLASS_VAR, SCLASS_PSTATIC);
    Set_ST_is_temp_var(_pu_handle);
    Set_ST_is_initialized(_pu_handle);
    INITO_IDX inito = New_INITO(_pu_handle);
    INITV_IDX inv = New_INITV();
    INITV_Init_Integer(inv, MTYPE_To_TY(Pointer_type), 0);
    Set_INITO_val(inito, inv);


  */
}


// ====================================================================
//
// Procedures to insert the instrumentation code into the WHIRL tree
//
// ====================================================================


BOOL 
WN_INSTRUMENT_WALKER::Is_Return_Store_Stmt( WN *wn )
{
  if ( wn && WN_operator( wn ) == OPR_STID ) {
    WN *val = WN_kid( wn, 0 );
    if ( WN_operator( val ) == OPR_LDID ) {
      ST *st = WN_st( val );
      if ( ST_sym_class( st ) == CLASS_PREG
	   && ( Is_Return_Preg( WN_offset( val ) )
		|| st == Return_Val_Preg ) )
	return TRUE;
    }
  }
  
  return FALSE;
}


BOOL 
WN_INSTRUMENT_WALKER::Is_Return_Store_Comma( WN *wn )
{
  if ( wn && WN_operator( wn ) == OPR_COMMA ) {
    WN *val = WN_kid( wn, 1 );
    if ( WN_operator( val ) == OPR_LDID ) {
      ST *st = WN_st( val );
      if ( ST_sym_class( st ) == CLASS_PREG
	   && ( Is_Return_Preg( WN_offset( val ) )
		|| st == Return_Val_Preg ) )
	return TRUE;
    }
  }
  
  return FALSE;
}


BOOL
WN_INSTRUMENT_WALKER::Test_Dedicated_Reg( WN *wn )
{
  if ( wn == NULL )
    return FALSE;

  OPERATOR opr = WN_operator( wn );

  if ( opr == OPR_LDID ) {
    ST *st = WN_st( wn );
    if ( ST_sym_class( st ) == CLASS_PREG
	 && Preg_Is_Dedicated( WN_offset( wn ) ) )
      return TRUE;
  }
  
  // traverse the tree starting with this node.
  if ( opr == OPR_BLOCK ) {
    // Special traversal case for BLOCK structure
    WN *node;
    for ( node = WN_first( wn ); node; node = WN_next( node ) )
      if ( Test_Dedicated_Reg( node ) )
	return TRUE;
  }
  else { // Traverse the kids
    for ( INT32 i = 0; i < WN_kid_count( wn ); i++ )
      if ( Test_Dedicated_Reg( WN_kid( wn, i ) ) )
	return TRUE;
  }
  return FALSE;
}


void
WN_INSTRUMENT_WALKER::Instrument_Before( WN *wn, WN *current_stmt,
					 WN *block )
{
  if ( Test_Dedicated_Reg( current_stmt ) ) {
    DevWarn( "Instrumenter Warning: Hardware registers used in "
	     "instrumented node - program may behave differently!" );
    // fdump_tree( TFile, current_stmt );
  }

  WN *stmt_prev = WN_prev( current_stmt );
  WN_INSERT_BlockAfter( block, stmt_prev, wn );
}


void
WN_INSTRUMENT_WALKER::Instrument_After( WN *wn, WN *current_stmt,
					WN *block )
{
  WN *stmt = WN_next( current_stmt );

  // If at a call, place instrumentation after return args saved.
  // if ( _phase != PROFILE_PHASE_BEFORE_VHO )
  int i = 0;
  if ( OPCODE_is_call( WN_opcode( current_stmt ) ) )
    while ( stmt && Is_Return_Store_Stmt( stmt ) ) {
      if ( WN_rtype( current_stmt ) == MTYPE_V ) {
	DevWarn( "Instrumenter Warning: Should NOT have skipped!" );
	// fdump_tree( TFile, current_stmt );
	// fdump_tree( TFile, stmt );
      }
      i++;
      stmt = WN_next( stmt );
    }

  if ( WN_rtype( current_stmt ) != MTYPE_V && i == 0 ) {
    DevWarn( "Instrumenter Warning: Should have skipped!" );
    // fdump_tree( TFile, current_stmt );
    // fdump_tree( TFile, stmt );
  }

  WN_INSERT_BlockBefore( block, stmt, wn );
  
  Record_Instrument_Node( wn );
}


void
WN_INSTRUMENT_WALKER::Instrument_Entry( WN *wn )
{
  WN_INSERT_BlockBefore( Entry_Block(), Entry_Pragma(), wn );
}


WN *
WN_INSTRUMENT_WALKER::Create_Comma_Kid( WN *wn, INT kid_idx ) {
  WN *wn_comma;
  WN *wn_kid = WN_kid( wn, kid_idx );
  OPERATOR opr_kid = OPCODE_operator( WN_opcode( wn_kid ) );
  if ( opr_kid == OPR_COMMA ) {
    wn_comma = wn_kid;
  } else {
    wn_comma = WN_Create( OPR_COMMA, WN_rtype( wn_kid ), MTYPE_V, 2 );
    WN_kid( wn_comma, 0 ) = WN_CreateBlock();
    WN_kid( wn_comma, 1 ) = wn_kid;
    WN_kid( wn, kid_idx ) = wn_comma;
  }
 /*  _vho_lower = TRUE; */
  return wn_comma;
}


// ====================================================================
//
// Below are instrumentation and annotation procedures for each type
// of instrumentation:
// -> Invoke
// -> Branch
// -> Loop
// -> Circuit
// -> Call
// -> Switch
// -> Compgoto
//
// Three procedures are defined for each type or subtype:
//
// Instrument_*
// Initialize_Instrumenter_*
// Annotate_*
//
// ====================================================================

void WN_INSTRUMENT_WALKER::Instrument_With_Gen_Struct(int initialized,
                                                      int mode,
                                                      WN *instr,
                                                      WN *wn,
                                                      WN *block,            
                                                      char *file_name,
						      char *pu_name,
						      int id,
						      int linenum,
						      int endline,
						      int subtype,
						      WN *taken,
						      char *callee,
                                                      WN *target,
                                                      int num_targets,
                                                      WN *called_fun_address )

{
 
  if(!ST_is_initialized(_profile_gen_struct_st) && initialized)
   
    {
    INITO_IDX inito = New_INITO(_profile_gen_struct_st);
    INITV_IDX inv_temp;
    INITV_IDX inv = New_INITV();
    INITV_IDX next_inv= New_INITV();
    INITV_Init_Block(inv, next_inv/*INITV_Next_Idx()*/);
    
    
    INITV_Init_Integer(next_inv, MTYPE_I8/*MTYPE_To_TY(Pointer_type)*/, 0); //pu_handle// runtime/ let it be 0
    
  
    if(file_name==NULL) {
    next_inv = INITO_to_String(next_inv,""); //filename *fix*
    } else {
    
    next_inv = INITO_to_String(next_inv,file_name);
    }
  
    if(pu_name==NULL) {
         next_inv = INITO_to_String(next_inv,""); //pu_name 
    }
    else {
        next_inv = INITO_to_String(next_inv,pu_name); 
    }
    
    next_inv = INITO_to_Integer(next_inv,MTYPE_I4,id);  // id
    next_inv = INITO_to_Integer(next_inv,MTYPE_I4,linenum);  // linenum
    next_inv = INITO_to_Integer(next_inv,MTYPE_I4,endline);  // endline
    next_inv = INITO_to_Integer(next_inv,MTYPE_I4,subtype);  // subtype
    next_inv = INITO_to_Integer(next_inv,MTYPE_I4,0);  // taken  *fix* runtime

    if(callee==NULL) {
    next_inv = INITO_to_String(next_inv,""); //callee 
    }
    else {
     next_inv = INITO_to_String(next_inv,callee);
    }


    next_inv = INITO_to_Integer(next_inv,MTYPE_I4,0);  // target *fix* runtime
    next_inv = INITO_to_Integer(next_inv,MTYPE_I4,num_targets);  // num_target
    next_inv = INITO_to_Integer(next_inv,MTYPE_I8/*MTYPE_To_TY(Pointer_type)*/,0);  // fun_address *fix* runtime
    next_inv = INITO_to_Integer(next_inv,MTYPE_I8,0); // data
   // next_inv = INITO_to_Integer(next_inv,MTYPE_I4,0); 
   
    Set_INITO_val(inito, inv);
     Set_ST_is_initialized(_profile_gen_struct_st);
    }



  if (mode==1) {
   
    int field_num =0;
  Instrument_After(instr,wn,block);
    
  if(!Epilog_Flag) {
    Instrument_After(Init_Struct(_profile_gen_struct_st,
                                PU_Handle(),VPtr_ty,field_num ),
                    wn,block);
  }
    field_num+=8;
      
    field_num+=8; //file_name
    field_num+=8; //pu_name 
    field_num+=4; //id 
    field_num+=4; // linenum 
    field_num+=4; //endline
    field_num+=4; //subtype
    
    if(taken!=NULL && !Epilog_Flag)
      Instrument_After(Init_Struct(_profile_gen_struct_st,
				   taken ,I4_ty,field_num ),
                    wn,block);
   
    field_num+=4; // taken
    
   
    field_num+=8; // callee
   
       if(target!=NULL && !Epilog_Flag)
          Instrument_After(Init_Struct(_profile_gen_struct_st,
                                target,I4_ty,field_num ),
                    wn,block);
    
    field_num+=4; // target

    
       field_num+=4; //num_targets

       
       if(called_fun_address!=NULL && !Epilog_Flag)
        Instrument_After(Init_Struct(_profile_gen_struct_st,
                                called_fun_address,VPtr_ty,field_num ),
                    wn,block);
       
       field_num+=8; //fun_address
   }
  else if (mode==0){
    
    int field_num=0;
    if (!Epilog_Flag) {
    Instrument_Before(Init_Struct(_profile_gen_struct_st,
                                PU_Handle(),VPtr_ty,field_num ),
                    wn,block);
    }
    field_num+=8;

    
    field_num+=8; // file_name
    field_num+=8; //pu_name
    field_num+=4; //id 
    field_num+=4; // linenum
    field_num+=4; //endline
    field_num+=4; //subtype
    
    if(taken!=NULL && !Epilog_Flag)
      Instrument_Before(Init_Struct(_profile_gen_struct_st,
				    taken ,I4_ty,field_num ),
                    wn,block);
    field_num+=4; // taken
 
    field_num+=8; //callee
    
     if(target!=NULL && !Epilog_Flag)
          Instrument_Before(Init_Struct(_profile_gen_struct_st,
                                target,I4_ty,field_num ),
			    wn,block); 


    field_num+=4; // target

    field_num+=4; //num_targets

     if(called_fun_address!=NULL && !Epilog_Flag)
        Instrument_Before(Init_Struct(_profile_gen_struct_st,
                                called_fun_address,VPtr_ty,field_num ),
                    wn,block);
     field_num+=8; //fun_address
    
      Instrument_Before(instr,wn,block);

  } else if (mode==2)
  {
       int field_num=0;
       WN_INSERT_BlockFirst(block,instr);
       Record_Instrument_Node( instr );
     
       WN *temp=NULL;
       if (!Epilog_Flag) {
       WN_INSERT_BlockFirst(block,temp=Init_Struct(_profile_gen_struct_st,
                                PU_Handle(),VPtr_ty,field_num ));       
       Record_Instrument_Node( temp ); 
       }        
       field_num+=8;

        field_num+=8; // file_name
        field_num+=8; //pu_name                
	field_num+=4; // id  
	field_num+=4; //linenum          
	field_num+=4; //endline 
	field_num+=4; //subtype         

    if(taken!=NULL && !Epilog_Flag){
      WN_INSERT_BlockFirst(block,temp=Init_Struct(_profile_gen_struct_st,
				       taken ,I4_ty,field_num ));
            Record_Instrument_Node( temp );         
    }
    field_num+=4; //taken

  
    field_num+=8; //callee               
    
    if(target!=NULL && !Epilog_Flag) {

          WN_INSERT_BlockFirst(block,temp=Init_Struct(_profile_gen_struct_st,
					   target,I4_ty,field_num ));
         Record_Instrument_Node( temp );
    } 
    
    field_num+=4; //target
  
     field_num+=4; //num_targets

       if(called_fun_address!=NULL && !Epilog_Flag) {
         WN_INSERT_BlockFirst(block,temp=Init_Struct(_profile_gen_struct_st,
						     called_fun_address,VPtr_ty,field_num ));
          Record_Instrument_Node( temp );          
    }            
       field_num+=8; // fun_address

  } else if (mode==3)
  {
    
    int field_num = 0;
    if (!Epilog_Flag) {
    WN_INSERT_BlockLast(wn, Init_Struct(_profile_gen_struct_st,
				    PU_Handle(),VPtr_ty,field_num ));
    }
    field_num+=8; 
               
    field_num+=8; // file_name
    field_num+=8; //pu_name;
    field_num+=4; //id
    field_num+=4; //linenum
    field_num+=4; //endline                   
    field_num+=4; //subtype
                    
    if(taken!=NULL && !Epilog_Flag)
      WN_INSERT_BlockLast( wn,Init_Struct(_profile_gen_struct_st,
				   taken ,I4_ty,field_num ));
    field_num+=4; // taken                

  
    field_num+=8; //callee                
   
       if(target!=NULL && !Epilog_Flag)
          WN_INSERT_BlockLast( wn,Init_Struct(_profile_gen_struct_st,
                                  target,I4_ty,field_num ));
     
       field_num+=4; // target                
       field_num+=4; //num_targets
              
       if(called_fun_address!=NULL && !Epilog_Flag) 
          WN_INSERT_BlockLast(wn,Init_Struct(_profile_gen_struct_st,
						     called_fun_address,VPtr_ty,field_num ));
       field_num+=8; // fun_address                
                 
    WN_INSERT_BlockLast( wn,instr );
  }
  
}
void
WN_INSTRUMENT_WALKER::Instrument_Invoke_Exit( WN *wn, INT32 id, WN *block, int mode)
{
  WN *wn2 = wn, *source_file=NULL, *subtype=NULL, *func_name;
  char *name=NULL, *filename=NULL;
  INT32 line = 0, endline=0;
  
    while(Get_Parent(wn2)!=NULL)
      wn2 = Get_Parent(wn2);
  

   if ( WN_has_sym( wn2 ) ) {
    name = ST_name( WN_st( wn2 ) );
    //    func_name = WN_LdaString( name, 0, strlen( name ) + 1 );
    // } else
    //func_name = WN_Zerocon( Pointer_type );
   }

  location(wn,source_file,line,filename,FALSE); 
   if(line==0) {
   location(wn2,source_file,line,filename);
   find_endline(wn2,line);
   line =0;
  } else {

   location(wn,source_file,line,filename);
  }
  
  // find_endline(wn2,endline);
   
   _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
   WN *instr = Gen_Call( INVOKE_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
                      
    Instrument_With_Gen_Struct(1,mode,
                             instr, 
                             wn,
                             block,            
                             filename,
			     name,
			     id,
			     line,
			     line,
			     0,  
			       NULL,
			       NULL,
                              NULL,
                              0,
                              NULL); 

    if(filename!=NULL) delete [] filename;
 }

void WN_INSTRUMENT_WALKER::Instrument_Invoke( WN *wn, INT32 id, WN *block )
{

  WN *wn2 = wn, *source_file=NULL, *subtype=NULL, *func_name;
  char *name=NULL,*filename=NULL; 
 INT32 line = 0, endline=0;
  
  while(Get_Parent(wn2)!=NULL)
    wn2 = Get_Parent(wn2);
  

   if ( WN_has_sym( wn2 ) ) {
    name = ST_name( WN_st( wn2 ) );
    //  func_name = WN_LdaString( name, 0, strlen( name ) + 1 );
    // } else
    //func_name = WN_Zerocon( Pointer_type );
   }

  location(wn2,source_file,line,filename); 
  find_endline(wn2,endline);

      _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
      WN *instr = Gen_Call(INVOKE_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
  WN_Set_Linenum(instr,WN_Get_Linenum(wn)); 
  Instrument_With_Gen_Struct(1, 1, /*after? */
                             instr, 
                             wn,
                             block,            
                             filename,
			     name,
			     id,
                             line,
			     endline,
			     0,
			     NULL,
			     NULL,
                             NULL,
                             0,
                             NULL);  
  if(filename!=NULL)
  delete [] filename;
}
void
WN_INSTRUMENT_WALKER::Initialize_Instrumenter_Invoke( INT32 count )
{
  if ( count == 0 ) return;

  WN *total_invokes = WN_Intconst( MTYPE_I4, count );
  // __profile_gen_init( handle, total_invokes )
  Instrument_Entry( Gen_Call( INVOKE_INIT_NAME,
			      PU_Handle(), total_invokes ) );
}


void
WN_INSTRUMENT_WALKER::Annotate_Invoke( WN *wn, INT32 id )
{
  // Sum profile frequency counts
  PU_PROFILE_HANDLES& handles = FB_Handle();
  FB_Info_Invoke info_invoke( FB_FREQ_ZERO );
  for ( PU_PROFILE_ITERATOR i( handles.begin() );
	i != handles.end (); ++i ) {
    FB_Info_Invoke& info = Get_Invoke_Profile( *i, id );
    info_invoke.freq_invoke += info.freq_invoke;
  }

  // Attach profile information to node.
  Cur_PU_Feedback->Annot_invoke( wn, info_invoke );
}


// ====================================================================

void WN_INSTRUMENT_WALKER::IsGotos(WN *wn, WN *stmt, WN *block)
{
   
   OPERATOR opr = WN_operator(wn);
   if(opr == OPR_BLOCK)
   {
     WN *node;
     for(node=WN_first(wn); node; node= WN_next(node))
           IsGotos(node,node,wn);

   }
   else
   {
     for (INT32 i=0; i<WN_kid_count(wn);i++)
           IsGotos(WN_kid(wn,i),stmt,block);
   }

   switch (opr)
   {
     case OPR_XGOTO:
     case OPR_COMPGOTO:
     case OPR_GOTO:
     case OPR_TRUEBR:
     case OPR_FALSEBR:
     HasGotos = TRUE;
     break;

    }

}
void WN_INSTRUMENT_WALKER::Instrument_Special_Returns(WN *wn, WN *stmt, WN *block, INT32 id, INT32 enum_subtype, INT32 line, INT32 endline, WN *source_file, BOOL branch)
{
   OPERATOR opr = WN_operator(wn);
   if(opr == OPR_BLOCK)
   {
     WN *node;
     for(node=WN_first(wn); node; node= WN_next(node))
        Instrument_Special_Returns(node,node,wn,id,enum_subtype, line, endline, source_file,branch);
     
   } 
   else
   {
     for (INT32 i=0; i<WN_kid_count(wn);i++)
	    Instrument_Special_Returns(WN_kid(wn,i),stmt,block,id,enum_subtype, line, endline, source_file,branch);          
   }

    switch(opr)
        {
           case OPR_RETURN:
           case OPR_RETURN_VAL:
           {
	     char *filename=NULL;
              location(wn,source_file,endline,filename); 

	     if (branch) {
	               _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
                       WN *instr3 = Gen_Call(BRANCH_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
                       WN_Set_Linenum(instr3,WN_Get_Linenum(wn)); 

                       Instrument_With_Gen_Struct(1,0, /*after? */
                             instr3, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     endline,
			     enum_subtype,
			     NULL,
			     NULL,
                             NULL,
			     0,
			     NULL); 
	

 	     }
	     else
	     { 
               _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
              WN *instr4 = Gen_Call(LOOP_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
              WN_Set_Linenum(instr4,WN_Get_Linenum(wn)); 

              Instrument_With_Gen_Struct(1,0, /*after? */
                             instr4, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     endline,
			     enum_subtype,
			     NULL,
			     NULL,
                             NULL,
			     0,
			     NULL);  




             } 
             	if(filename!=NULL)
		  delete [] filename;  
           }
           break;

           case OPR_PICCALL:
           case OPR_CALL:
           case OPR_INTRINSIC_CALL:
           case OPR_IO:
            { 
               
              if ( WN_has_sym( wn ) ) {
                   char *name = ST_name( WN_st( wn ) );
                  if (strcmp(name,"_END")==0)
	             {
                       // do instrumentation
                       char *filename = NULL; 
                       location(wn,source_file,endline,filename); 
		       if(branch) {
                       _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
                         WN *instr3 = Gen_Call(BRANCH_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
                       WN_Set_Linenum(instr3,WN_Get_Linenum(wn)); 

                       Instrument_With_Gen_Struct(1,0, /*after? */
                             instr3, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     endline,
			     enum_subtype,
			     NULL,
			     NULL,
                             NULL,
                             0,NULL); 
        
		       }
                        else
	               {     _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
		               
                               WN *instr4 = Gen_Call(LOOP_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
                               WN_Set_Linenum(instr4,WN_Get_Linenum(wn)); 

                              Instrument_With_Gen_Struct(1,0, /*after? */
                                                         instr4, 
							 wn,
							 block,            
							 filename,
							 NULL,
							 id,
							 line,
							 endline,
							 enum_subtype,
							 NULL,
							 NULL,
							 NULL,
							 0,NULL);  
                              
                       }
                       if(filename!=NULL) delete [] filename;     
		     }             
              
	      }

              else if(opr == OPR_INTRINSIC_CALL)
	      {
	         switch (WN_intrinsic(wn))
                  {
	              case INTRN_STOP:
	              case INTRN_STOP_F90:
			char *filename=NULL;
                      location(wn,source_file,endline,filename); 
			if (branch) {
			  _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
                           WN *instr3 = Gen_Call(BRANCH_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
                       WN_Set_Linenum(instr3,WN_Get_Linenum(wn)); 
                       Instrument_With_Gen_Struct(1,0, /*after? */
                             instr3, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     endline,
			     enum_subtype,
			     NULL,
			     NULL,
                             NULL,
                             0,NULL);
			}
                       else
	                {   _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
	                         WN *instr4 = Gen_Call(LOOP_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
                                 WN_Set_Linenum(instr4,WN_Get_Linenum(wn)); 

                                 Instrument_With_Gen_Struct(1,0, /*after? */
							    instr4, 
							    wn,
							    block,            
							    filename,
							    NULL,
							    id,
							    line,
							    endline,
							    enum_subtype,
							    NULL,
							    NULL,
							    NULL,
							    0,NULL);
                             
                        }
                        if(filename!=NULL) delete [] filename;
	              break;
	          }
	       }
        
	    }
            break;
	}// end switch
}

void
WN_INSTRUMENT_WALKER::Instrument_Branch( WN *wn, INT32 id, WN *block, WN *stmt )
{

  WN *source_file=NULL;
  INT32 line=0, endline=0;
  char *filename = NULL;
  location(wn,source_file,line,filename);
  find_endline(wn,endline);
  WN *str_subtype=NULL;
  INT32 enum_subtype=-1;
  region_subtype(wn,str_subtype,enum_subtype);

  // Compute condition once and save to preg.

  TYPE_ID cond_type = WN_rtype( WN_kid0( wn ) );
  PREG_NUM cond = Create_Preg( cond_type, "__branch_cond" );
  Instrument_Before( WN_StidIntoPreg( cond_type, cond,
				      MTYPE_To_PREG( cond_type ),
				      WN_kid0( wn ) ),
		     wn, block );

  // Replace condition by preg
  WN_kid0( wn ) = WN_LdidPreg( cond_type, cond );


  // profile_branch( handle, b_id, condition, taken_if_true );
  OPERATOR opr = OPCODE_operator( WN_opcode( wn ) );
  WN *taken = WN_Relational( ( opr == OPR_FALSEBR ) ? OPR_EQ : OPR_NE,
			     MTYPE_I4, WN_LdidPreg( cond_type, cond ),
			     WN_Intconst( MTYPE_I4, 0 ) );

     _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
    WN *instr = Gen_Call(BRANCH_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
  WN_Set_Linenum(instr,WN_Get_Linenum(wn)); 

  Instrument_With_Gen_Struct(1,0, /*after? */
                             instr, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     endline,
			     enum_subtype,
			     taken,
			     NULL,
                             NULL,
                             0,NULL);  
  


  if (Epilog_Flag)
  {
    
  WN *instr2 = Gen_Call(BRANCH_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
  WN_Set_Linenum(instr2,WN_Get_Linenum(wn)); 

  Instrument_With_Gen_Struct(0,1,  //after
                             instr2, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     endline,
			     enum_subtype,
			     NULL,
			     NULL,
                             NULL,
                             0,NULL); 
  }

  if(filename!=NULL) delete [] filename;

  if (Epilog_Flag) {
    for (INT32 i=0; i<WN_kid_count(wn); i++)
    Instrument_Special_Returns(WN_kid(wn,i),stmt,block, id, enum_subtype, line, endline, source_file,TRUE); 
  }
}


void
WN_INSTRUMENT_WALKER::Instrument_Cselect( WN *wn, INT32 id, WN *block )
{
  WN *source_file=NULL;
  char *filename = NULL;
  INT32 line=0, endline=0;
  location(wn,source_file,line,filename);
  find_endline(wn,endline);
  WN *str_subtype=NULL;
  INT32 enum_subtype=-1;
  region_subtype(wn,str_subtype,enum_subtype);

  // Create comma for kid0
  WN *comma = Create_Comma_Kid( wn, 0 );

  // Compute condition once and save to preg.
  TYPE_ID cond_type = WN_rtype( WN_kid( comma, 1 ) );
  PREG_NUM cond = Create_Preg( cond_type, "__cselect_cond" );
  WN *stid = WN_StidIntoPreg( cond_type, cond, MTYPE_To_PREG( cond_type ),
			      WN_kid( comma, 1 ) );
  WN_INSERT_BlockLast( WN_kid( comma, 0 ), stid );

  // Replace condition by preg
  WN_kid( comma, 1 ) = WN_LdidPreg( cond_type, cond );

  // Insert instrumentation call
  WN *taken = WN_Relational( OPR_NE, MTYPE_I4,
			     WN_LdidPreg( cond_type, cond ),
			     WN_Intconst( MTYPE_I4, 0 ) );
  WN *instr = Gen_Call( BRANCH_INSTRUMENT_NAME, PU_Handle(),
			WN_Intconst( MTYPE_I4, id ), taken, WN_Intconst(MTYPE_I4,enum_subtype) ,WN_Intconst(MTYPE_I4,line),WN_Intconst(MTYPE_I4,endline),source_file);
  WN_INSERT_BlockLast( WN_kid( comma, 0 ), instr );

 if (Epilog_Flag)
  {
    
    WN *instr2 = Gen_Call( BRANCH_EXIT_INSTRUMENT_NAME, PU_Handle(),
			WN_Intconst( MTYPE_I4, id ),  WN_Intconst(MTYPE_I4,enum_subtype) ,WN_Intconst(MTYPE_I4,line),WN_Intconst(MTYPE_I4,endline),
     WN_COPY_Tree(source_file));
    WN_Set_Linenum(instr2,WN_Get_Linenum(wn));
    Instrument_After( instr2, wn, block );
    
  }
  
 if(filename!=NULL) delete [] filename;
}


void
WN_INSTRUMENT_WALKER::Initialize_Instrumenter_Branch( INT32 count )
{
  if ( count == 0 ) return;

  // __profile_branch_init(handle, total_branches)
  WN *total_branches = WN_Intconst( MTYPE_I4, count );
  Instrument_Entry( Gen_Call( BRANCH_INIT_NAME,
			      PU_Handle(), total_branches ) );
}


void
WN_INSTRUMENT_WALKER::Annotate_Branch(WN *wn, INT32 id)
{
  PU_PROFILE_HANDLES& handles = FB_Handle();
  FB_Info_Branch info_branch( FB_FREQ_ZERO, FB_FREQ_ZERO );
  for (PU_PROFILE_ITERATOR i( handles.begin() ); i != handles.end(); ++i ) {
    FB_Info_Branch& info = Get_Branch_Profile( *i, id );
    info_branch.freq_taken += info.freq_taken;
    info_branch.freq_not_taken += info.freq_not_taken;
  }
  // Attach profile information to node.
  Cur_PU_Feedback->Annot_branch( wn, info_branch );
}


// ====================================================================


void
WN_INSTRUMENT_WALKER::Instrument_Loop( WN *wn, INT32 id, WN *block, WN *stmt )
{
   WN *source_file=NULL, *subtype=NULL;
   char *filename = NULL;
  INT32 line=0, endline=0;
  INT omp_found = 0;
  WN *pregion = Get_Parent(Get_Parent(wn));
   if(WN_operator(pregion)==OPR_REGION)
  {
    WN *pregion_pragmas = WN_first(WN_region_pragmas(pregion)); 
    switch(WN_pragma(pregion_pragmas))
    {
      case WN_PRAGMA_PARALLEL_BEGIN:
      case WN_PRAGMA_PARALLEL_SECTIONS:
      case WN_PRAGMA_PARALLEL_DO:
      case WN_PRAGMA_DOACROSS:
      case WN_PRAGMA_PDO_BEGIN:
      omp_found = 1;
      

      break;
    }
  } 

 
  location(wn,source_file,line,filename);
  find_endline(wn,endline);
   WN *str_subtype=NULL;
  INT32 enum_subtype=-1;
  region_subtype(wn,str_subtype,enum_subtype);
  // region_subtype(wn,subtype);  

  OPERATOR opr = OPCODE_operator( WN_opcode( wn ) );

  // profile_loop( handle, id ) before loop.
 
   _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
    WN *instr2 = Gen_Call(LOOP_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
  WN_Set_Linenum(instr2,WN_Get_Linenum(wn)); 

  Instrument_With_Gen_Struct(1,0, /*after? */
                             instr2, 
                             omp_found? pregion : wn,
                             omp_found? Get_Parent(pregion) : block,            
                             filename,
			     NULL,
			     id,
                             line,
			     endline,
			     enum_subtype,
			     NULL,
			     NULL,
                             NULL,
                             0,NULL);  
  


  if(Epilog_Flag) 
    {  
 
        WN *instr3 = Gen_Call(LOOP_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
        WN_Set_Linenum(instr3,WN_Get_Linenum(wn)); 

        Instrument_With_Gen_Struct(0,1, /*after? */
                             instr3, 
                             omp_found? pregion : wn,
                             omp_found? Get_Parent(pregion) : block,            
                             filename,
			     NULL,
			     id,
                             line,
			     endline,
			     enum_subtype,
			     NULL,
			     NULL,
                             NULL,
                             0,NULL);  


    } 
  if(filename!=NULL)
    delete [] filename;
 if (Epilog_Flag) {
    for (INT32 i=0; i<WN_kid_count(wn); i++)
     Instrument_Special_Returns(WN_kid(wn,i),stmt,block, id, enum_subtype, line, endline, source_file,FALSE); 
  }


  // profile_loop_iter( handle, id ) at beginning of loop iter.
  WN *body = ( opr == OPR_DO_LOOP
	       ? WN_do_body( wn ) : WN_while_body( wn ) );

   WN *source_file2=NULL;
   char *filename2=NULL;
  INT32 line2=0, endline2=0;
  location(body,source_file2,line2,filename2);
  find_endline(body,endline2);
    WN *str_subtype2=NULL;
  INT32 enum_subtype2=-1;
  region_subtype(wn,str_subtype2,enum_subtype2); //leave the original wn, for the loop type.


 /*
        _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
       WN *iter_call = Gen_Call(LOOP_INST_ITER_NAME,Load_Struct(_profile_gen_struct_st));
        WN_Set_Linenum(iter_call,WN_Get_Linenum(body)); 

        Instrument_With_Gen_Struct(1,2, 
                             iter_call, 
                             wn,
                             body,            
                             filename2,
			     NULL,
			     id,
                             line2,
			     endline2,
			     enum_subtype2,
			     NULL,
			     NULL,
                             NULL,
                             0,NULL);  */

  if(filename2!=NULL) delete [] filename2;
}


void
WN_INSTRUMENT_WALKER::Initialize_Instrumenter_Loop( INT32 count )
{
  if ( count == 0 ) return;

  WN *total_loops = WN_Intconst( MTYPE_I4, count );
  // __profile_loop_init( handle, total_loops )
  Instrument_Entry( Gen_Call( LOOP_INIT_NAME,
				   PU_Handle(), total_loops ) );
}


void
WN_INSTRUMENT_WALKER::Annotate_Loop( WN *wn, INT32 id )
{
  PU_PROFILE_HANDLES& handles = FB_Handle();
  FB_Info_Loop info_loop( FB_FREQ_ZERO, FB_FREQ_ZERO, FB_FREQ_ZERO,
			  FB_FREQ_ZERO, FB_FREQ_ZERO, FB_FREQ_ZERO );
  for ( PU_PROFILE_ITERATOR i( handles.begin() ); i != handles.end(); ++i ) {
    FB_Info_Loop& info = Get_Loop_Profile ( *i, id ); 
    info_loop.freq_zero += info.freq_zero;
    info_loop.freq_positive += info.freq_positive;
    info_loop.freq_out += info.freq_out;
    info_loop.freq_back += info.freq_back;
    info_loop.freq_exit += info.freq_exit;
    info_loop.freq_iterate += info.freq_iterate;
  }
  // Attach profile information to node.
  Cur_PU_Feedback->Annot_loop( wn, info_loop );
}


// ====================================================================


// WN * 
// SHORT_CIRCUIT_INSTRUMENTER::Instrument_Clause(WN *clause, WN *call)
// {
//   OPERATOR opr = OPCODE_operator(WN_opcode(clause));
//   WN *comma;
//
//   if (opr == OPR_COMMA) {
//     comma = clause;
//   } else {
//     comma = WN_Create(OPR_COMMA, WN_rtype(clause), MTYPE_V, 2);
//     WN_kid(comma, 0) = WN_CreateBlock();
//     WN_kid(comma, 1) = clause;
//   }
//
//   WN_INSERT_BlockFirst(WN_kid(comma, 0), call);
//   return comma;
// }


void
WN_INSTRUMENT_WALKER::Instrument_Circuit( WN *wn, INT32 id )
{
  // No need to instrument left branch (kid 0)
  char *filename = NULL;
  WN *parent = wn;
     WN *str_subtype=NULL;
  INT32 enum_subtype=-1;
  region_subtype(wn,str_subtype,enum_subtype);

  while(!OPCODE_is_scf(WN_opcode(parent)) && Get_Parent(parent)!=NULL)
  {
    parent = Get_Parent(parent);  
  }

  WN *source_file=NULL;
  INT32 line=0;
  location(parent,source_file,line,filename);

  // Create comma for right branch (kid 1)

  WN *comma = Create_Comma_Kid( wn, 1 );

  // Compute condition once and save to preg
  TYPE_ID cond_type = WN_rtype( WN_kid( comma, 1 ) );
  PREG_NUM cond = Create_Preg( cond_type, "__circuit_cond" );
  WN *stid = WN_StidIntoPreg( cond_type, cond, MTYPE_To_PREG( cond_type ),
			      WN_kid( comma, 1 ) );
  WN_INSERT_BlockLast( WN_kid( comma, 0 ), stid );

  // Replace condition in left branch by preg
  WN_kid( comma, 1 ) = WN_LdidPreg( cond_type, cond );

  // Insert instrumentation call
  //   IDEA: Use BRANCH instead of SHORT_CIRCUIT
  OPERATOR opr = OPCODE_operator( WN_opcode( wn ) );
  WN *taken = WN_Relational( opr == OPR_CAND ? OPR_EQ : OPR_NE,
			     MTYPE_I4, WN_LdidPreg( cond_type, cond ),
			     WN_Intconst( MTYPE_I4, 0 ) );
     _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
    WN *instr = Gen_Call(SHORT_CIRCUIT_INST_NAME,Load_Struct(_profile_gen_struct_st));
    WN_Set_Linenum(instr,WN_Get_Linenum(wn)); 

  Instrument_With_Gen_Struct(1,3, /*after? */
                             instr, 
                             WN_kid(comma,0),
                             NULL,            
                             filename,
			     NULL,
			     id,
                             line,
			     line,
			     enum_subtype,
			     taken,
			     NULL,
                             NULL,
                             0,NULL); 


  if(filename!=NULL) delete [] filename;
}


void
WN_INSTRUMENT_WALKER::Initialize_Instrumenter_Circuit( INT32 count )
{
  if ( count == 0 ) return;

  WN *total_short_circuits = WN_Intconst( MTYPE_I4, count );
  // __profile_short_circuit_init( handle, total_short_circuits )
  Instrument_Entry( Gen_Call( SHORT_CIRCUIT_INIT_NAME,
				   PU_Handle(), total_short_circuits ) );
}


void
WN_INSTRUMENT_WALKER::Annotate_Circuit( WN *wn, INT32 id )
{
  PU_PROFILE_HANDLES& handles = FB_Handle();
  FB_Info_Circuit info_circuit( FB_FREQ_ZERO, FB_FREQ_ZERO, FB_FREQ_ZERO );
  for (PU_PROFILE_ITERATOR i( handles.begin () ); i != handles.end(); ++i ) {
    FB_Info_Circuit& info = Get_Short_Circuit_Profile( *i, id );
    info_circuit.freq_left += info.freq_left;
    info_circuit.freq_right += info.freq_right;
    info_circuit.freq_neither += info.freq_neither;
  }
  // Attach profile information to node.
  Cur_PU_Feedback->Annot_circuit( wn, info_circuit );
}


// ====================================================================


void
WN_INSTRUMENT_WALKER::Instrument_Call( WN *wn, INT32 id, WN *block )
{
  // Get the name of the called function.
  WN *called_func_name;
  WN *source_file=NULL;
  char *name=NULL, *filename=NULL;
  
  INT32 line=0;
  location(wn,source_file,line,filename);
    WN *str_subtype=NULL;
  INT32 enum_subtype=-1;
  region_subtype(wn,str_subtype,enum_subtype);


  if ( WN_has_sym( wn ) ) {
    name = ST_name( WN_st( wn ) );
    called_func_name = WN_LdaString( name, 0, strlen( name ) + 1 );
  } else
    called_func_name = WN_Zerocon( Pointer_type );

  _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
  WN *instr = Gen_Call(CALL_INST_ENTRY_NAME,Load_Struct(_profile_gen_struct_st));
  WN_Set_Linenum(instr,WN_Get_Linenum(wn)); 
  Instrument_With_Gen_Struct(1,0, /*after? */
                             instr, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     line,
			     enum_subtype,
			     NULL,
			     name,
                             NULL,
                             0,NULL);  

    
 
  if(Epilog_Flag) {

      WN *instr = Gen_Call(CALL_INST_EXIT_NAME,Load_Struct(_profile_gen_struct_st));
      WN_Set_Linenum(instr,WN_Get_Linenum(wn)); 
      Instrument_With_Gen_Struct(0,1, /*after? */
                             instr, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     line,
			     enum_subtype,
			     NULL,
			     name,
                             NULL,
                             0,NULL); 

  }
  if(filename!=NULL) delete [] filename;
}



void
WN_INSTRUMENT_WALKER::Instrument_Icall( WN *wn, INT32 id, WN *block )
{
  // Get the address of the called function.
  char *filename = NULL;
  WN *called_func_address;
   WN *source_file=NULL;
  INT32 line=0;
  location(wn,source_file,line,filename);

  called_func_address = WN_COPY_Tree(WN_kid(wn,WN_kid_count(wn)-1));

  
  

      _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);

   WN *instr = Gen_Call(ICALL_ENTRY_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
   WN_Set_Linenum(instr,WN_Get_Linenum(wn)); 
   Instrument_With_Gen_Struct(1,0, /*after? */
                             instr, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     line,
			     0,
			     NULL,
			     NULL,
                             NULL,
                             0, 
			     called_func_address
                             );  

    if(Epilog_Flag)
    {

       WN *instr = Gen_Call(ICALL_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st));
      WN_Set_Linenum(instr,WN_Get_Linenum(wn)); 
      Instrument_With_Gen_Struct(0,1, /*after? */
                             instr, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     line,
			     0,
			     NULL,
			     NULL,
                             NULL,
                             0,
			     WN_COPY_Tree(called_func_address)); 

  }
    if(filename!=NULL) delete [] filename;
}

void
WN_INSTRUMENT_WALKER::Initialize_Instrumenter_Call( INT32 count )
{
  if ( count == 0 ) return;

  WN *total_calls = WN_Intconst( MTYPE_I4, count );
  // __profile_call_init( handle, total_calls )
  Instrument_Entry( Gen_Call( CALL_INIT_NAME,
			      PU_Handle(), total_calls ) );
}

void
WN_INSTRUMENT_WALKER::Initialize_Instrumenter_Icall( INT32 count )
{
  if ( count == 0 ) return;

  WN *total_icalls = WN_Intconst( MTYPE_I4, count );
  // __profile_icall_init( handle, total_calls )
  Instrument_Entry( Gen_Call( ICALL_INIT_NAME,
			      PU_Handle(), total_icalls ) );
}

#ifdef KEY
void WN_INSTRUMENT_WALKER::Initialize_Instrumenter_Value_FP_Bin( INT32 count )
{
  if ( count == 0 ) return;

  WN* total_values = WN_Intconst( MTYPE_I4, count );
  // __profile_loop_init( handle, total_values )
  Instrument_Entry( Gen_Call( VALUE_FP_BIN_INIT_NAME,
			      PU_Handle(), total_values ) );
}


static BOOL Is_Array_Exp ( WN *wn ) 
{
  if (WN_operator(wn) == OPR_ARRAYEXP)
    return TRUE;
  
  for (INT kid = 0; kid < WN_kid_count(wn); kid ++)
    if (Is_Array_Exp(WN_kid(wn, kid))) return TRUE;

  return FALSE;
}

void WN_INSTRUMENT_WALKER::Instrument_Value_FP_Bin( WN *wn, INT32 id, 
						    WN *block, WN *parent, 
						    WN *stmt )
{
  if ( !stmt ||
       /* To work around a bug in the optimizer. */
       WN_operator(stmt) == OPR_IO )
    return;

  INT kid_num = -1;
  for ( INT kid = 0; kid < WN_kid_count(parent); kid ++ )
    if ( WN_kid(parent, kid) == wn ) {
      kid_num = kid; break; 
    }  
  if (kid_num == -1) return;

  // F90 lowerer is called after instrumentation. So we should not instrument
  // MPYs with F90 ARRAYEXP operands.
  if (Is_Array_Exp(WN_kid0(wn)) || Is_Array_Exp(WN_kid1(wn)))
    return;

  WN* comma = Create_Comma_Kid ( parent, kid_num );
  WN* expr0 = WN_kid0(wn);
  WN* expr1 = WN_kid1(wn);
  
  const TYPE_ID opnd_type = MTYPE_F8;
  PREG_NUM preg0 = Create_Preg( opnd_type, "__value_fp_0_prof" );
  PREG_NUM preg1 = Create_Preg( opnd_type, "__value_fp_1_prof" );
  WN* stid0 = WN_StidIntoPreg( opnd_type, preg0, MTYPE_To_PREG( opnd_type ),
			       expr0 );
  WN* stid1 = WN_StidIntoPreg( opnd_type, preg1, MTYPE_To_PREG( opnd_type ),
			       expr1 );
  WN* instr = Gen_Call( VALUE_FP_BIN_INSTRUMENT_NAME, PU_Handle(),
			WN_Intconst( MTYPE_I4, id ),
			WN_LdidPreg( opnd_type, preg0 ),
			WN_LdidPreg( opnd_type, preg1 ) );

  WN_INSERT_BlockLast( WN_kid( comma, 0 ), stid0 );
  WN_INSERT_BlockLast( WN_kid( comma, 0 ), stid1 );
  WN_INSERT_BlockLast( WN_kid( comma, 0 ), instr );

  WN_kid0( wn ) = WN_LdidPreg( opnd_type, preg0 );
  WN_kid1( wn ) = WN_LdidPreg( opnd_type, preg1 );
}

void WN_INSTRUMENT_WALKER::Annotate_Value_FP_Bin( WN *wn, INT32 id )
{
  PU_PROFILE_HANDLES& handles = FB_Handle();

  if( handles.size() == 1 ){
    Cur_PU_Feedback->Annot_value_fp_bin(wn, 
				     Get_Value_FP_Bin_Profile(handles[0],id));
    return;
  }
}


void WN_INSTRUMENT_WALKER::Initialize_Instrumenter_Value( INT32 count )
{
  if ( count == 0 ) return;

  WN* total_values = WN_Intconst( MTYPE_I4, count );
  // __profile_loop_init( handle, total_values )
  Instrument_Entry( Gen_Call( VALUE_INIT_NAME,
			      PU_Handle(), total_values ) );
}


void WN_INSTRUMENT_WALKER::Instrument_Value( WN *wn, INT32 id, WN *block )
{
  // Create comma for right branch (kid 1)
  WN *comma = Create_Comma_Kid( wn, 1 );

  const TYPE_ID divisor_type = MTYPE_I8;
  const TYPE_ID to_type = Mtype_TransferSize( divisor_type,
					      WN_rtype( WN_kid(comma,1) ) );
  WN* kid1 = WN_Type_Conversion( WN_kid(comma,1), to_type );

  PREG_NUM divisor = Create_Preg( divisor_type, "__value_prof" );

  WN* stid = WN_StidIntoPreg( divisor_type, divisor, MTYPE_To_PREG( divisor_type ),
			      kid1 );
  WN_INSERT_BlockLast( WN_kid( comma, 0 ), stid );

  // Replace kid1 by preg
  WN_kid( comma, 1 ) = WN_LdidPreg( divisor_type, divisor );

  // Insert instrumentation call

  WN* instr = Gen_Call( VALUE_INSTRUMENT_NAME, PU_Handle(),
			WN_Intconst( MTYPE_I4, id ),
			WN_LdidPreg( divisor_type, divisor ) );

  WN_INSERT_BlockLast( WN_kid( comma, 0 ), instr );
}

typedef std::map<INT64, FB_FREQ> V2F_MAP;
typedef V2F_MAP::iterator V2F_ITERATOR;
static V2F_MAP v2f_map;

struct Sort_Value_by_Freq {
  const V2F_MAP& v2f;
  bool operator()(INT64 i, INT64 j) { 
    //return v2f[i] > v2f[j];
    return v2f_map[i] > v2f_map[j];
  }
  Sort_Value_by_Freq( const V2F_MAP& v2f_map ) : v2f(v2f_map) {}
};


void WN_INSTRUMENT_WALKER::Annotate_Value( WN *wn, INT32 id )
{
  PU_PROFILE_HANDLES& handles = FB_Handle();

  if( handles.size() == 1 ){
    Cur_PU_Feedback->Annot_value( wn, Get_Value_Profile( handles[0], id ) );
    return;
  }

  //V2F_MAP v2f_map;
  FB_FREQ exe_counter(0.0);
  Is_True( v2f_map.empty(), ("v2f_map is not empty") );

  // Collect all the data and put them to v2f_map.

  for( PU_PROFILE_ITERATOR i( handles.begin () ); i != handles.end(); ++i ){
    const FB_Info_Value& src_info = Get_Value_Profile( *i, id );

    exe_counter += src_info.exe_counter;

    for( int i = 0; i < src_info.num_values; i++ ){
      const INT64   new_value = src_info.value[i];
      const FB_FREQ new_freq  = src_info.freq[i];

      if( v2f_map.find( new_value ) == v2f_map.end() ){
	v2f_map[new_value] = new_freq;

      } else {
	v2f_map[new_value] = v2f_map[new_value] + new_freq;
      }
    }
  }

  // Sort v2f_map by freq.
  vector<INT64> sorted_value;
  for( V2F_ITERATOR i = v2f_map.begin(); i != v2f_map.end(); i++ ){
    sorted_value.push_back( i->first );
  }

  sort( sorted_value.begin(), sorted_value.end(), Sort_Value_by_Freq( v2f_map ) );

  // Insert the first TNV elements of v2f_map to dest_info.
  FB_Info_Value dest_info;

  dest_info.num_values = MIN( TNV, sorted_value.size() );
  dest_info.exe_counter = exe_counter;

  for( int i = 0; i < dest_info.num_values; i++ ){
    const INT64 value = sorted_value[i];
    dest_info.value[i] = value;
    dest_info.freq[i]  = v2f_map[value];
  }


  // Attach profile information to node.
  Cur_PU_Feedback->Annot_value( wn, dest_info );

  v2f_map.clear();
}
#endif

void
WN_INSTRUMENT_WALKER::Annotate_Call( WN *wn, INT32 id )
{
  PU_PROFILE_HANDLES& handles = FB_Handle();
  FB_Info_Call info_call( FB_FREQ_ZERO );
  for (PU_PROFILE_ITERATOR i( handles.begin() ); i != handles.end (); ++i) {
    FB_Info_Call& info = Get_Call_Profile( *i, id );
    info_call.freq_entry += info.freq_entry;
    info_call.freq_exit += info.freq_exit;
  }

  info_call.in_out_same = ( info_call.freq_entry == info_call.freq_exit );

  // Attach profile information to node.
  Cur_PU_Feedback->Annot_call( wn, info_call );
}

void
WN_INSTRUMENT_WALKER::Annotate_Icall( WN *wn, INT32 id )
{
  FB_Icall_Vector icall_table =  _fb_handle_merged->Get_Icall_Table();
  FB_Info_Icall & info_icall = icall_table[id];

  // Attach profile information to node.
  Cur_PU_Feedback->Annot_icall( wn, info_icall );
}

// ====================================================================


void
WN_INSTRUMENT_WALKER::Instrument_Switch( WN *wn, INT32 id, WN *block )
{

  WN *source_file=NULL;
  INT32 line=0, endline=0;
  char *filename=NULL;
  location(wn,source_file,line,filename);
  find_endline(wn,endline);
  // Record number of targets
  _switch_num_targets.push_back( WN_num_entries( wn ) );

  // Record case values (one for each target)
  for ( WN *wn_casegoto = WN_first( WN_kid1( wn ) );
	wn_casegoto != NULL;
	wn_casegoto = WN_next( wn_casegoto ) ) {
    _switch_case_values.push_back( WN_const_val( wn_casegoto ) );
  }

  // Compute target once and save to preg.
  TYPE_ID cond_type = WN_rtype( WN_kid0( wn ) );
  PREG_NUM cond = Create_Preg( cond_type, "__switch_cond" );

  Instrument_Before( WN_StidIntoPreg( cond_type, cond,
				      MTYPE_To_PREG( cond_type ),
				      WN_kid0( wn ) ),
		     wn, block );
  WN_kid0( wn ) = WN_LdidPreg( cond_type, cond );


  _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);
  WN *instr = Gen_Call(SWITCH_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st)); 
  WN_Set_Linenum(instr,WN_Get_Linenum(wn)); 
   Instrument_With_Gen_Struct(1,0, /*after? */
                             instr, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     endline,
			     0,
			     NULL,
			     NULL,
			     WN_LdidPreg( cond_type, cond ),
			      WN_num_entries( wn ),
                             NULL 
                             ); 

 
  
  if(Epilog_Flag) {


    WN *instr = Gen_Call(SWITCH_EXIT_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st)); 
    WN_Set_Linenum(instr,WN_Get_Linenum(wn)); 
    Instrument_With_Gen_Struct(0,1, //after? 
                             instr, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     endline,
			     0,
			     NULL,
			     NULL,
			     NULL,
                             0,
                             NULL
                             ); 


  }

   if(filename!=NULL) delete [] filename;
}


void
WN_INSTRUMENT_WALKER::Initialize_Instrumenter_Switch( INT32 count )
{
  if ( count == 0 ) return;

  // Build switch length array from vector.
  INT32 num_switches = count;
      
  TY_IDX arrayTY = Make_Array_Type( MTYPE_I4, 1, num_switches );
  ST *arrayST = New_ST( CURRENT_SYMTAB );
  ST_Init( arrayST, Save_Str( "switch_num_targets" ),
	   CLASS_VAR, SCLASS_PSTATIC, EXPORT_LOCAL, arrayTY );
      
  // This should be only initialized once - instead of every time.
  // How to do this?
  for ( INT32 i = 0; i < num_switches; i++ ) {
    WN *st = WN_Stid(MTYPE_I4, i * MTYPE_RegisterSize( MTYPE_I4 ),
		     arrayST, arrayTY,
		     WN_Intconst( MTYPE_I4, _switch_num_targets[i] ) );
    Instrument_Entry( st );
  }

  WN *total_switches = WN_Intconst( MTYPE_I4, num_switches );
  WN *switch_num_targets = WN_Lda( Pointer_type, 0, arrayST );


  // Build case value array from vector.
  INT32 num_case_values = _switch_case_values.size();

  arrayTY = Make_Array_Type( MTYPE_I8, 1, num_case_values );
  arrayST = New_ST( CURRENT_SYMTAB );
  ST_Init( arrayST, Save_Str( "switch_case_values" ),
	   CLASS_VAR, SCLASS_PSTATIC, EXPORT_LOCAL, arrayTY );
      
  // This should be only initialized once - instead of every time.
  // How to do this?
  for ( INT32 j = 0; j < num_case_values; j++ ) {
    WN *st = WN_Stid( MTYPE_I8, j * MTYPE_RegisterSize( MTYPE_I8 ),
		      arrayST, arrayTY,
		      WN_Intconst( MTYPE_I8, _switch_case_values[j] ) );
    Instrument_Entry( st );
  }

  WN *total_case_values = WN_Intconst( MTYPE_I4, num_case_values );
  WN *switch_case_values = WN_Lda( Pointer_type, 0, arrayST );

  // __profile_switch_init(handle, total_switches, target_count_array)
  WN *instr = Gen_Call_ref35( SWITCH_INIT_NAME, PU_Handle(),
			      total_switches, switch_num_targets,
			      total_case_values, switch_case_values );
  Instrument_Entry( instr );
}


static inline void
Handle_Switch_Profile( PU_PROFILE_HANDLES& handles, WN* wn, INT32 id,
		       FB_Info_Switch& (*get_profile) ( PU_PROFILE_HANDLE,
							INT32 ) )
{
  FB_Info_Switch& info = (*get_profile) ( handles[0], id );
  if ( handles.size() == 1 ) {
    // Attach profile information to node.
    Cur_PU_Feedback->Annot_switch( wn, info );
  } else {
    FB_Info_Switch info_switch;
    info_switch.freq_targets.insert( info_switch.freq_targets.begin(),
				     info.freq_targets.begin(),
				     info.freq_targets.end() );
    PU_PROFILE_ITERATOR i (handles.begin ());
    for (++i; i != handles.end (); ++i) {
      FB_Info_Switch& info = (*get_profile) (*i, id);
      FmtAssert( info.size () == info_switch.size (),
		 ("Inconsistent profile data from different files"));
      transform( info.freq_targets.begin(),
		 info.freq_targets.end(),
		 info_switch.freq_targets.begin(),
		 info_switch.freq_targets.begin(),
		 std::plus<FB_FREQ>() );
    }
    Cur_PU_Feedback->Annot_switch( wn, info_switch );
  }
}


void
WN_INSTRUMENT_WALKER::Annotate_Switch( WN *wn, INT32 id )
{
  Handle_Switch_Profile( FB_Handle(), wn, id, Get_Switch_Profile );
}


// ====================================================================


void
WN_INSTRUMENT_WALKER::Instrument_Compgoto( WN *wn, INT32 id, WN *block )
{
   WN *source_file=NULL;
   char *filename = NULL;
  INT32 line=0;
  location(wn,source_file,line,filename);
     WN *str_subtype=NULL;
  INT32 enum_subtype=-1;
  region_subtype(wn,str_subtype,enum_subtype);
  _compgoto_num_targets.push_back( WN_num_entries( wn ) );
  
  // Compute target once and save to preg.
  TYPE_ID cond_type = WN_rtype( WN_kid0( wn ) );
  PREG_NUM cond = Create_Preg( cond_type, "__compgoto_cond" );

  WN *target = WN_StidIntoPreg( cond_type, cond,
				MTYPE_To_PREG( cond_type ),
				WN_kid0( wn ) );
  Instrument_Before( target, wn, block );
  WN_kid0( wn ) = WN_LdidPreg( cond_type, cond );

   _profile_gen_struct_st = create_struct_st(profile_gen_struct_ty_idx);

  WN *instr = Gen_Call(COMPGOTO_INSTRUMENT_NAME,Load_Struct(_profile_gen_struct_st)); 
  WN_Set_Linenum(instr,WN_Get_Linenum(wn)); 
  Instrument_With_Gen_Struct(1,0, /*after? */
                             instr, 
                             wn,
                             block,            
                             filename,
			     NULL,
			     id,
                             line,
			     line,
			     enum_subtype,
			     NULL,
			     NULL,
			     WN_LdidPreg( cond_type, cond ),
                             WN_num_entries( wn ),
                             NULL
                             ); 

  if (filename!=NULL) delete [] filename;
}


void
WN_INSTRUMENT_WALKER::Initialize_Instrumenter_Compgoto( INT32 count )
{
  if ( count == 0 ) return;

  // Build compgoto length array from vector.
  INT32 num_compgotos = count;

  TY_IDX arrayTY = Make_Array_Type( MTYPE_I4, 1, num_compgotos );
  ST *arrayST = New_ST( CURRENT_SYMTAB );
  ST_Init( arrayST, Save_Str( "compgoto_num_targets" ),
	   CLASS_VAR, SCLASS_PSTATIC, EXPORT_LOCAL, arrayTY );

  // This should be only initialized once - instead of every time.
  // How to do this?
  for ( INT32 i = 0; i < num_compgotos; i++ ) {
    WN *st = WN_Stid( MTYPE_I4, i * MTYPE_RegisterSize( MTYPE_I4 ),
		      arrayST, arrayTY, 
		      WN_Intconst( MTYPE_I4, _compgoto_num_targets[i] ) );
    Instrument_Entry( st );
  }

  WN *total_compgotos = WN_Intconst( MTYPE_I4, num_compgotos );
  WN *compgoto_num_targets = WN_Lda( Pointer_type, 0, arrayST );

  // __profile_compgoto_init(handle, total_compgotos, target_count_array)
  Instrument_Entry( Gen_Call_ref3( COMPGOTO_INIT_NAME, PU_Handle(),
				   total_compgotos,
				   compgoto_num_targets ) );
}


void
WN_INSTRUMENT_WALKER::Annotate_Compgoto( WN *wn, INT32 id )
{
  Handle_Switch_Profile( FB_Handle(), wn, id, Get_Compgoto_Profile );
}


// ====================================================================
//
// This is the main driver of the instrumenter. It traverses the tree
// calling appropriate instrumentation procedures when needed.
//
// Tree_Walk_Node must traverse the WHIRL tree in the same order
// during annotation as during instrumentation.
//
// Tree_Walk_Node and Tree_Walk must proceed FORWARD through the
// statements within a BLOCK (so that _instrumentation_nodes only need
// record upcoming WHIRL nodes).
//
// ====================================================================


#ifndef KEY
void
WN_INSTRUMENT_WALKER::Tree_Walk_Node( WN *wn, WN *stmt, WN *block )
#else
void
WN_INSTRUMENT_WALKER::Tree_Walk_Node( WN *wn, WN *stmt, WN *block,
				      WN* parent )
#endif
{
  OPERATOR opr = WN_operator( wn );

  // Pay special attention to ALT_ENTRY, PRAGMA, and REGION

  // ALT_ENTRY indicates we are entering preamble
  if ( opr == OPR_ALTENTRY ) {
    Is_True( ! _in_preamble, ( "WN_INSTRUMENT_WALKER::Tree_Walk_Node found"
			       " no WN_PRAGMA_PREAMBLE_END pragma" ) );
    _in_preamble = TRUE;
  }

  // WN_PRAGMA_PREAMBLE_END pragma indicates the end of the preamble;
  // Record the PRAGMA for later insertion of instrumentation
  //   initialization code.
  else if ( opr == OPR_PRAGMA
	    && WN_pragma( wn ) == WN_PRAGMA_PREAMBLE_END ) {
    Is_True( _in_preamble, ( "WN_INSTRUMENT_WALKER::Tree_Walk_Node found"
			     " extra WN_PRAGMA_PREAMBLE_END pragma" ) );
    _in_preamble = FALSE;
    Push_Entry_Pragma( wn, block );
  }

  else if ( opr == OPR_REGION ) {

    // temp for _pu_handle must be scoped SHARED within PARALLEL regions
    WN *regn_prag = WN_first( WN_region_pragmas( wn ) );
    if ( regn_prag ) {
      switch ( WN_pragma( regn_prag ) ) {
      case WN_PRAGMA_PARALLEL_BEGIN:
      case WN_PRAGMA_PARALLEL_SECTIONS:
      case WN_PRAGMA_PARALLEL_DO:
      case WN_PRAGMA_DOACROSS:
	{
	  WN *prag = WN_CreatePragma( WN_PRAGMA_SHARED, _pu_handle, 0, 0 );
	  WN_set_pragma_compiler_generated( prag );
	  WN_INSERT_BlockLast( WN_region_pragmas ( wn ), prag );
	}
	break;
      default:
	break;  // not a PARALLEL region
      }
    }
  }

  // Do not instrument or annotate code within the preamble
  // Skip over any nodes added by instrumentation
  if ( _in_preamble || Test_Instrument_Node( wn ) )
    return;

  // Traverse WHIRL subtree.
  if ( opr == OPR_BLOCK ) {

    // Special traversal case for BLOCK structure
    WN *node;
    for ( node = WN_first( wn ); node; node = WN_next( node ) )
      Tree_Walk_Node( node, node, wn );

    Is_True( ! _in_preamble, ( "WN_INSTRUMENT_WALKER::Tree_Walk_Node found"
			       " no WN_PRAGMA_PREAMBLE_END pragma" ) );

  } else if ( OPERATOR_is_expression( opr ) ) {

    // Watch out for special case:
    if ( Is_Return_Store_Comma( wn ) ) {

      // Handle COMMA holding CALL with return value --- convert:
      //   BLOCK                   --->      BLOCK
      //    ... PARAMs ...         --->       ... PARAMs ...
      //   CALL                    --->      CALL
      //                           --->       LDID preg_return_val
      //                           --->      STID temp
      //   END_BLOCK               --->      END_BLOCK
      //   LDID preg_return_val    --->     LDID temp
      //  COMMA                    --->     COMMA
      // This convertion allow the CALL to be instrumented correctly.

      // Store return value into preg
      TYPE_ID val_type = WN_rtype( WN_kid( wn, 1 ) );
      PREG_NUM val = Create_Preg( val_type, "__call_comma" );
      WN *stid = WN_StidIntoPreg( val_type, val, MTYPE_To_PREG( val_type ),
				  WN_kid( wn, 1 ) );
      WN_INSERT_BlockLast( WN_kid( wn, 0 ), stid );
      
      // Comma now returns value of preg
      WN_kid( wn, 1 ) = WN_LdidPreg( val_type, val );
    }

    // Traverse the kids of the current expression
    for ( INT32 i = 0; i < WN_kid_count( wn ); i++ )
#ifndef KEY
      Tree_Walk_Node( WN_kid( wn, i ), stmt, block );
#else
      Tree_Walk_Node( WN_kid( wn, i ), stmt, block, wn );
#endif

  } else {

    // Traverse the kids of the current statement
    for ( INT32 i = 0; i < WN_kid_count( wn ); i++ )
#ifndef KEY
      Tree_Walk_Node( WN_kid( wn, i ), wn, block );
#else
      Tree_Walk_Node( WN_kid( wn, i ), wn, block, wn );
#endif
  }

  // Perform the instrumentation or annotation of the current node
  switch ( opr ) {

  case OPR_RETURN:
  case OPR_RETURN_VAL:
    {
            //  _instrument_count++;
   if(Epilog_Flag && _instrumenting) {
        if (Instrumentation_Procedures || Instrumentation_All)
	{     INT32 id = _count_return++;
              Instrument_Invoke_Exit(wn,id,block);
        }
      }
     } 
    
  break;

#ifdef KEY

  case OPR_MPY:
    if( OPCODE_rtype(WN_opcode(wn)) == MTYPE_F8 && ( Instrumentation_All || Instrumentation_Multiplies) &&meets_nesting_level(wn)) {      _instrument_count++;
      const INT32 id = _count_value_fp_bin++;
      if( _instrumenting ) {	 
             Instrument_Value_FP_Bin( wn, id, block, parent, stmt );
      }
      else
        Annotate_Value_FP_Bin( wn, id );
    }   
    break;
  case OPR_REM:
  case OPR_DIV:
  case OPR_MOD:
    if( !WN_operator_is( WN_kid1(wn), OPR_INTCONST ) &&
	MTYPE_is_integral( OPCODE_rtype(WN_opcode(wn) ) ) && ( Instrumentation_All 
        || Instrumentation_Reminder_Division_Modulus) &&meets_nesting_level(wn) ){
      _instrument_count++;
      const INT32 id = _count_value++;
      if( _instrumenting ) {
	  Instrument_Value( wn, id, block );
      }
      else
	Annotate_Value( wn, id );
    }
    break;
#endif

  case OPR_PRAGMA:
    if ( WN_pragma( wn ) != WN_PRAGMA_PREAMBLE_END )
      break;
     if (Instrumentation_Procedures || Instrumentation_All)
    {
      _instrument_count++;
      INT32 id = _count_invoke++;
      if ( _instrumenting) {         
	 Instrument_Invoke( wn, id, block );
      }
      else
	Annotate_Invoke( wn, id );
    }
    break;
   
  case OPR_TRUEBR:
  case OPR_FALSEBR:
  case OPR_IF:
  if((Instrumentation_Branches || Instrumentation_All) &&meets_nesting_level(wn))   
    { HasGotos = FALSE;
      IsGotos(wn,stmt,block);
      if(Epilog_Flag) {
       if(HasGotos==FALSE) {
       _instrument_count++;
      INT32 id = _count_branch++;
      if ( _instrumenting ) {
	  Instrument_Branch( wn, id,block,stmt );
      }
      else
	Annotate_Branch( wn, id );
      }
     }
     else {
      _instrument_count++;
      INT32 id = _count_branch++;
      if ( _instrumenting ) {
        Instrument_Branch( wn, id,block,stmt );
      } 
      else
        Annotate_Branch( wn, id );

     }
    }
    break; 

    
  case OPR_CSELECT:
    if((Instrumentation_Cselects || Instrumentation_All) &&meets_nesting_level(wn)) 
    {
      _instrument_count++;
      INT32 id = _count_branch++;
      if ( _instrumenting ) {
	
	     Instrument_Cselect( wn, id,block );
      }
      else
	Annotate_Branch( wn, id );
    }
    break;
    
 
  case OPR_DO_LOOP:
  case OPR_WHILE_DO:
  case OPR_DO_WHILE:
    if((Instrumentation_All || Instrumentation_Loops) && meets_nesting_level(wn) ) 
    { 
      HasGotos=FALSE;
      IsGotos(wn,stmt,block);
      if (Epilog_Flag)
      {
       if (HasGotos==FALSE) {
      _instrument_count++;
      INT32 id = _count_loop++;
      if ( _instrumenting)
      { 
 	  Instrument_Loop( wn, id, block,stmt );
	  
      }
      else
	Annotate_Loop( wn, id );
       }
      }
      else
      {
       _instrument_count++;
      INT32 id = _count_loop++;
      if ( _instrumenting )
      { 
        Instrument_Loop( wn, id, block,stmt );
      }
      else
        Annotate_Loop( wn, id );
      }
    }
    break; 

   
  case OPR_CAND:
  case OPR_CIOR:
  if((Instrumentation_All || Instrumentation_Shortcircuits) &&meets_nesting_level(wn))
    {
      _instrument_count++;
      INT32 id = _count_circuit++;
      if ( _instrumenting )
	   {
	     Instrument_Circuit( wn, id );
	  }
      else
	Annotate_Circuit( wn, id );
    }
    break;
  
 
  case OPR_PICCALL:
  case OPR_CALL:
  case OPR_INTRINSIC_CALL:
  case OPR_IO:
   { 
     char mpistr[] = "mpi_";
     char pompstr[] = "pomp_";
     BOOL skip_func = FALSE;

     if ( WN_has_sym( wn ) ) {
         char *name = ST_name( WN_st( wn ) );
         char *tmp_name = new char[strlen(name)+1];
         strcpy(tmp_name,name);
         for (int i=0; i<strlen(name);i++)
             tmp_name[i] = tolower(tmp_name[i]); 
          
         if (is_mpi_or_pomp(tmp_name))
           skip_func = TRUE;

         if (strcmp(name,"_END")==0)
	   {
             skip_func = TRUE;
             if(Epilog_Flag) {                     
                        if(_instrumenting)
			{  if (Instrumentation_Procedures || Instrumentation_All)
			{    _count_return++;
                            INT32 id = _count_return;
                            Instrument_Invoke_Exit(wn,id,block);
                        }
                        }
		     }             
           } 
        
     
        delete [] tmp_name;       
     }

     else if(opr == OPR_INTRINSIC_CALL)
	   {
	     switch (WN_intrinsic(wn))
             {
	        case INTRN_STOP:
	        case INTRN_STOP_F90:
		  skip_func = TRUE;
                   if(Epilog_Flag) {
                 
		      if(_instrumenting) {
			  if (Instrumentation_Procedures || Instrumentation_All) {
                           _count_return++;
                            INT32 id = _count_return;  
                            Instrument_Invoke_Exit(wn,id,block);
			  } 
		      }
		     } 
	     
	        break;

	     }
	   }
        
      
     if (!skip_func && (Instrumentation_Callsites || Instrumentation_All) && meets_nesting_level(wn) ) {
      _instrument_count++;
      INT32 id = _count_call++;
      if ( _instrumenting ) {
	   Instrument_Call( wn, id, block );
      }
      else
	Annotate_Call( wn, id );
    } 
    }
    break;
 
  case OPR_ICALL:
   if((Instrumentation_Callsites || Instrumentation_All) &&meets_nesting_level(wn))
    {
      _instrument_count++;
	  INT32 idcall = _count_call++;
	  INT32 idicall = _count_icall++;
	  if (_instrumenting)
	  {
	         
		Instrument_Call( wn, idcall, block);
		Instrument_Icall( wn, idicall, block);
              
	  }
	  else
	  {
	    Annotate_Call( wn, idcall);
	    Annotate_Icall( wn, idicall);
	  }
    }
    break; 
  
  case OPR_SWITCH:
   if ((Instrumentation_Switches || Instrumentation_All) &&meets_nesting_level(wn)) 
    {
      _instrument_count++;
      INT32 id = _count_switch++;
      if ( _instrumenting ) {	  
	      Instrument_Switch( wn, id, block );
      }
      else
	Annotate_Switch( wn, id );
    }
    break;

  case OPR_COMPGOTO:
  case OPR_XGOTO:
   if ((Instrumentation_Gotos || Instrumentation_All) &&meets_nesting_level(wn)) 
    {
      _instrument_count++;
      INT32 id = _count_compgoto++;
      if ( _instrumenting ) {              
	Instrument_Compgoto( wn, id, block );       
      }
      else
	Annotate_Compgoto( wn, id );
    }
    break;
    
  }

  // This shouldn't be necessary, but....
  if ( opr == OPR_REGION ) {
    if ( _vho_lower ) {
      WN_region_body( wn ) = VHO_Lower( WN_region_body( wn ) );
 // #ifndef KEY 
      _vho_lower = FALSE;
// #endif
    }
  }
}

void
WN_INSTRUMENT_WALKER::Merge_Icall_Feedback()
{
	if (_fb_handle_merged)
	{
		DevWarn("Icall feecback data already merged!\n");
		return;
	}
	if (_fb_handle.size() == 0)
	{
		DevWarn("no Icall feedback data for current PU.\n");
		_fb_handle_merged = NULL;
		return; 
	}

	//Now merge the data together.
	_fb_handle_merged = CXX_NEW(PU_Profile_Handle(NULL, 0), _mempool);
	FB_Icall_Vector & fb_merged_value_vector = _fb_handle_merged->Get_Icall_Table();
	INT fb_handle_num;
	fb_handle_num = _fb_handle.size();
	PU_PROFILE_HANDLE the_largest_fb;
	PU_PROFILE_ITERATOR pu_prof_itr = _fb_handle.begin();
	the_largest_fb = *pu_prof_itr;
	_fb_count_icall = the_largest_fb->Get_Icall_Table().size();
	for (pu_prof_itr= ( _fb_handle.begin() ); pu_prof_itr != _fb_handle.end (); ++pu_prof_itr)
	{
    	PU_Profile_Handle * handle=*pu_prof_itr;
    	if ( _fb_count_icall != handle->Icall_Profile_Table.size() )
    		DevWarn("Icall_Profile_Table.size() differ in feedback files!");
    	if ( _fb_count_icall < handle->Icall_Profile_Table.size() )
    	{
    		the_largest_fb = handle;
    		_fb_count_icall = the_largest_fb->Icall_Profile_Table.size();
    	}
	}

	fb_merged_value_vector.resize(_fb_count_icall);
		if (_fb_count_icall==0)
		{
		return;
		}

	UINT64 * values = TYPE_MEM_POOL_ALLOC_N(UINT64, _mempool, TNV_N*fb_handle_num);
	UINT64 * counters = TYPE_MEM_POOL_ALLOC_N(UINT64, _mempool, TNV_N*fb_handle_num);
	INT i, j, k, m, n;
	for ( i=0; i<_fb_count_icall; i++ )
	{
		memset(values, 0, TNV_N*fb_handle_num*sizeof(UINT64));
		memset(counters, 0, TNV_N*fb_handle_num*sizeof(UINT64));
		INT32 cur_id = the_largest_fb->Icall_Profile_Table[i].tnv._id;
		if (cur_id != i)
		{
			Is_True( cur_id == 0, ("cur_id should either be 0(means not executed) or i"));
			cur_id = i;
		}
		UINT64 cur_exec_counter = 0;
		INT cur_flag = the_largest_fb->Icall_Profile_Table[i].tnv._flag;
		for (pu_prof_itr = _fb_handle.begin(); pu_prof_itr != _fb_handle.end (); ++pu_prof_itr)
		{
			if ( i >= (*pu_prof_itr)->Get_Icall_Table().size() )
				continue;
			FB_Info_Icall & fb_info_value = Get_Icall_Profile( *pu_prof_itr, i );
			if (fb_info_value.tnv._id != cur_id)
			{
				//Note: if _id not consistent, the only valid case is "in some feedback file this icall is not executed at all"
				// in such case, the values of fb_info_value.tnv should all be 0 (we should make sure we write 0 into the file)
				Is_True( fb_info_value.tnv._id == 0 ,("_id not consitent between feedback files"));
				continue;
			}	
			cur_exec_counter += fb_info_value.tnv._exec_counter;
			Is_True(fb_info_value.tnv._flag == cur_flag,("_flag not consitent between feedback files"));
			for ( j=0; j<TNV_N; j++ )
			{
				if ( fb_info_value.tnv._counters[j] == 0 )
					break;
				for ( m=0;m<TNV_N*fb_handle_num;m++)
				{
					if (counters[m] == 0)
					{
						values[m] = fb_info_value.tnv._values[j];
						counters[m] += fb_info_value.tnv._counters[j];
						break;
					}
					else if ( fb_info_value.tnv._values[j] == values[m] )
					{
						counters[m] += fb_info_value.tnv._counters[j];
						break;
					}
				}
			}
		}
		for ( m=0; m<TNV_N*fb_handle_num; m++ )
			for ( n=m+1; n<TNV_N*fb_handle_num; n++)
			{
				if (counters[m] < counters[n])
				{
					INT tmp;
					tmp = counters[m];
					counters[m] = counters[n];
					counters[n] = tmp;
				}
			}
		fb_merged_value_vector[i].tnv._id = cur_id;
		fb_merged_value_vector[i].tnv._exec_counter = cur_exec_counter;
		fb_merged_value_vector[i].tnv._flag = cur_flag;
		for ( m=0; m<TNV_N; m++ )
		{
			fb_merged_value_vector[i].tnv._values[m] = values[m];
			fb_merged_value_vector[i].tnv._counters[m] = counters[m];
		}
	}
}

INT32 
WN_node_count(WN * wn)
{
	INT32 count = 0;
	for (WN_ITER * wni=WN_WALK_TreeIter(wn); wni; wni=WN_WALK_TreeNext(wni))
	{
		count++;
	}
	return count;
}

void
WN_INSTRUMENT_WALKER::Tree_Walk( WN *root ) 
{
  // Root must be a func entry!!
  Is_True( WN_operator( root ) == OPR_FUNC_ENTRY,
	   ( "WN_INSTRUMENT_WALKER::Tree_Walk:"
	     " OPR_FUNC_ENTRY expected at top of PU tree." ) );

  // Cur_PU_Feedback should not be NULL if annotating
  Is_True( _instrumenting || Cur_PU_Feedback,
	   ( "WN_INSTRUMENT_WALKER::Tree_Walk:"
	     " NULL Cur_PU_Feedbackduring annotation" ) );

  // Tree_Walk should not be called if annotating with empty _fb_handle
  Is_True( _instrumenting || ! _fb_handle.empty (),
	   ( "WN_INSTRUMENT_WALKER::Tree_Walk: No feedback info for"
	     " program unit %s in file %s.", Cur_PU_Name, Src_File_Name ) );

  // Don't instrument twice!
  // Can we come up with an Is_True to check for multiple instrumentation?
  // Is_True( ! _entry_is_instrumented,
  //	   ( "Instrumenter Error: Whirl tree already instrumented "
  //	     "(initialization done)!" ) );

#if Instrumenter_DEBUG
  fdump_tree( TFile, root );
#endif

  if ( !_instrumenting ) //if annotation, need to merge feedback file for Icall information.
  {
    Is_True(_fb_handle_merged == NULL, ("Merged Feedback data for Icall should be NULL before anyone merge it!"));
    Merge_Icall_Feedback();
    if (_fb_handle_merged == NULL)
    {
	   DevWarn("There is no Icall feedback data for current PU!");
    }
  }
#ifdef KEY
  pu_wn = root;
#endif
   
  Inst_Parent_Map = WN_MAP_Create(_mempool);
  Set_Parent(root,NULL);
  Parentize(root);  
  // Instrument all statements after (and including) the preamble end;
  // Do not instrument statements in the preamble
  _in_preamble = TRUE;  // will be set FALSE at WN_PRAGMA_PREAMBLE_END
  WN* body = WN_func_body( root );
  INT32 pusize_est = WN_node_count(body);
  WN* stmt;
  for ( stmt = WN_first( body ); stmt; stmt = WN_next( stmt ) )
    Tree_Walk_Node( stmt, stmt, body );

  if(_count_invoke>0 && _count_return==0 && Epilog_Flag)
   { 
     WN *last = WN_last(body); 
      if (last) {
     _count_return++;
     Parentize(root);
     Instrument_Invoke_Exit(last,_count_return,body, 1);
     } 
   }    
  Is_True( ! _in_preamble, ( "WN_INSTRUMENT_WALKER::Tree_Walk found"
			     " no WN_PRAGMA_PREAMBLE_END pragma" ) );

  // Is there any instrumentation to be initialized?
  if ( _instrumenting && _instrument_count > 0 ) {
      
    // Add initialization to each entry point.
    while ( ! Entry_List_Empty() ) {
      // Initialize the instrumentation.
      // Note: this only needs to be done in the entire program.
      // For now, I'm calling this at every PU.
      WN *output_file_name
	= WN_LdaString( Instrumentation_File_Name, 0,
			strlen( Instrumentation_File_Name ) + 1 );
      WN *phasenum = WN_Intconst( MTYPE_I4, _phase );

      WN *unique_output = WN_Intconst( MTYPE_I4,
				       Instrumentation_Unique_Output );

       _profile_init_struct_st = create_struct_st(profile_init_struct_ty_idx);
     
    {
    INITO_IDX inito = New_INITO(_profile_init_struct_st);
    INITV_IDX inv_temp;
    INITV_IDX inv = New_INITV();
    INITV_IDX next_inv= New_INITV();
    INITV_Init_Block(inv, /*next_inv*/ INITV_Next_Idx());
         
    if(Instrumentation_File_Name==NULL) {
    next_inv = INITO_to_String(next_inv,""); //filename *fix*
    } else {
    
    next_inv = INITO_to_String(next_inv,Instrumentation_File_Name);
    }
    
    // next_inv = INITO_to_Integer(next_inv,MTYPE_I8,0);
    next_inv = INITO_to_Integer(next_inv,MTYPE_I4,_phase);  // id
    next_inv = INITO_to_Integer(next_inv,MTYPE_I4,Instrumentation_Unique_Output);  // linenum
    Set_INITO_val(inito, inv);
     Set_ST_is_initialized(_profile_init_struct_st);
    }


    /*
      Instrument_Entry(Init_Struct(_profile_init_struct_st,
                                   output_file_name,Ptr_ty,0 ));
       Instrument_Entry(Init_Struct(_profile_init_struct_st,
                                   phasenum,I4_ty,8 ));
       Instrument_Entry(Init_Struct(_profile_init_struct_st,
                                     unique_output,I4_ty,12 ));
    */
       
      Instrument_Entry( Gen_Call( INST_INIT_NAME,
                                  Load_Struct(_profile_init_struct_st) ) );
	  
      // Initialize the PU instrumentation.
     if (!Epilog_Flag) { 
     WN *src_file_name = WN_LdaString ( Src_File_Name, 0,
					 strlen( Src_File_Name ) + 1 );
      WN *pu_name = WN_LdaString ( Cur_PU_Name, 0, strlen( Cur_PU_Name ) + 1 );
      WN *pc = WN_Lda( Pointer_type, 0, WN_st( root ) );
      WN *pusize = WN_Intconst( MTYPE_I4, pusize_est );
      WN *checksum = WN_Intconst( MTYPE_I4, _instrument_count );
      // r2 = __profile_pu_init( src_file_name, pu_name, pc, pusize, checksum )

#ifdef KEY
      /* The body of an "extern __inline__" function will not be emitted.
	 So don't bother to mention its name in the instrumented assemble
	 file (bug#2255).
      */
      if( PU_is_extern_inline(Get_Current_PU()) ){
	pc = WN_Zerocon( Pointer_type );
      }
#endif
      Instrument_Entry( Gen_Call( PU_INIT_NAME, src_file_name,
				  pu_name, pc, pusize, checksum, Pointer_type ) );
	  
      // Get current handle.
      PREG_NUM rreg1, rreg2;

      if ( WHIRL_Return_Info_On ) {

	RETURN_INFO return_info
	  = Get_Return_Info( Be_Type_Tbl( Pointer_type ), Use_Simulated
#ifdef TARG_X8664
	  		     , PU_ff2c_abi(Get_Current_PU())
#endif
	  		   );

	if ( RETURN_INFO_count( return_info ) <= 2 ) {
	  rreg1 = RETURN_INFO_preg( return_info, 0 );
	  rreg2 = RETURN_INFO_preg( return_info, 1 );
	} else
	  Fail_FmtAssertion( "WN_Instrumenter::WN_tree_init:"
			     " more than 2 return registers");
      } else {
	Get_Return_Pregs( Pointer_type, MTYPE_UNKNOWN, &rreg1, &rreg2 );
      }
	  
      // handle = r2;
      Instrument_Entry( WN_Stid(Pointer_type, 0, _pu_handle, 
      				MTYPE_To_TY(Pointer_type),
				WN_LdidPreg( Pointer_type, rreg1 ) ) );
      // Initialize specific instrumentation.
      
     
      Initialize_Instrumenter_Invoke(   _count_invoke   );
      Initialize_Instrumenter_Branch(   _count_branch   );
      Initialize_Instrumenter_Loop(     _count_loop     );
      Initialize_Instrumenter_Circuit(  _count_circuit  );
      Initialize_Instrumenter_Call(     _count_call     );
      Initialize_Instrumenter_Icall(    _count_icall     );
      Initialize_Instrumenter_Switch(   _count_switch   );
      Initialize_Instrumenter_Compgoto( _count_compgoto );
#ifdef KEY
      Initialize_Instrumenter_Value( _count_value );
      Initialize_Instrumenter_Value_FP_Bin( _count_value_fp_bin );
#endif
    }
      Pop_Entry_Pragma();
    }

    // Perform additional lowering - if necessary.
    if ( _vho_lower ) {
      WN_func_body( root ) = VHO_Lower( WN_func_body( root ) );
      _vho_lower = FALSE;
    }

  } else if ( ! _instrumenting ) { // feedback

    // Compare checksums!
    for ( PU_PROFILE_ITERATOR i( _fb_handle.begin() );
	  i != _fb_handle.end(); ++i ) {
	  
      UINT32 checksum = Get_PU_Checksum( *i );
	
      // KEY
      FmtAssert( _instrument_count == checksum,
		 ( "Instrumenter Error: (Phase %d) Feedback file %s has "
		   "invalid checksum for program unit %s in file %s. "
		   "Computed = %d, In file = %d.",
		   _phase, (*i)->fb_name, Cur_PU_Name, Src_File_Name,
		   _instrument_count, checksum ) );
    }
  }
  WN_MAP_Delete(Inst_Parent_Map);
#if Instrumenter_DEBUG
  fdump_tree(TFile, root);
#endif
//  fdump_tree(TFile, root);
//  Print_symtab(TFile,CURRENT_SYMTAB);
//  Print_symtab(TFile,GLOBAL_SYMTAB);
}


// ====================================================================
//
// Interface to the rest of the compiler backend -- see fuller
// descriptions in the header file wn_instrument.h.
//
// WN_Instrument performs feedback instrumentation on the WHIRL tree
// rooted at wn.
//
// WN_Annotate reads frequency counts from one or more previously
// generated feedback data files and stores the feedback data into
// the FEEDBACK object Cur_PU_Feedback.
//
// Set_Instrumentation_File_Name records the prefix for the names of
// the feedback data files.
//
// ====================================================================


void
WN_Instrument( WN *wn, PROFILE_PHASE phase )
{
  Set_Error_Phase( "WN_Instrument" );
  if ( Instrumenter_DEBUG )
    DevWarn( "WN_Instrument, phase == %d", phase );

  // Create and initialize local memory pool
  MEM_POOL local_mempool;
  MEM_POOL_Initialize( &local_mempool, "WN_INSTRUMENT_WALKER_Pool", FALSE );
  MEM_POOL_Push( &local_mempool );
  {
    // Walk the WHIRL tree -- instrument
    PU_PROFILE_HANDLES fb_handles; // empty for instrumentation
    WN_INSTRUMENT_WALKER wiw( TRUE, phase, &local_mempool, fb_handles );
    wiw.Tree_Walk( wn );
  }

  // Dispose of local memory pool
  MEM_POOL_Pop( &local_mempool );
  MEM_POOL_Delete( &local_mempool );
}

void 
WN_Clean_Mapid_for_Calls(WN * wn)
{
	for (WN_ITER * wni=WN_WALK_TreeIter(wn); wni; wni=WN_WALK_TreeNext(wni))
	{
		WN * wntmp = WN_ITER_wn(wni);
		OPCODE opcode;
		OPERATOR opr;
		opcode = WN_opcode (wntmp);
  		opr = WN_operator(wntmp);

		if ( opr == OPR_CALL || opr == OPR_ICALL )
		{
			if (WN_map_id(wntmp) != -1)
			{
			  WN_set_map_id(wntmp, -1);
			}
		}
	}
}

void
WN_Annotate( WN *wn, PROFILE_PHASE phase, MEM_POOL *MEM_pu_pool )
{
  Set_Error_Phase( "WN_Annotate" );
  if ( Instrumenter_DEBUG )
    DevWarn( "WN_Annotate, phase == %d", phase );
  
  WN_Clean_Mapid_for_Calls(wn);

  // Prepare Cur_PU_Feedback, allocating a new FEEDBACK object if
  // necessary.  Note that feedback info might not be always available
  // (e.g., function never called)
  PU_PROFILE_HANDLES fb_handles
    = Get_PU_Profile( Cur_PU_Name, Src_File_Name,
		      Feedback_File_Info[phase] );
  if ( fb_handles.empty() ) {
#ifdef KEY
    DevWarn( "Function %s() is not called in the training run", Cur_PU_Name );
#else
    DevWarn( "Cannot find expected feedback data - function not called?" );
#endif
    return;
  } else {
    // Use CXX_DELETE( Cur_PU_Feedback, MEM_pu_pool ); first if not NULL??
    if ( Cur_PU_Feedback == NULL )
      Cur_PU_Feedback = CXX_NEW( FEEDBACK( wn, MEM_pu_pool ), MEM_pu_pool );
  }

  // Create and initialize local memory pool
  MEM_POOL local_mempool;
  MEM_POOL_Initialize( &local_mempool, "WN_INSTRUMENT_WALKER_Pool", FALSE );
  MEM_POOL_Push( &local_mempool );
  {
    // Walk the WHIRL tree -- annotate
    WN_INSTRUMENT_WALKER wiw( FALSE, phase, &local_mempool, fb_handles );
    wiw.Tree_Walk( wn );
  }
  // Dispose of local memory pool
  MEM_POOL_Pop( &local_mempool );
  MEM_POOL_Delete( &local_mempool );

#ifdef KEY
  Cur_PU_Feedback->Set_Runtime_Func_Addr( 0 );  

  for( PU_PROFILE_ITERATOR i( fb_handles.begin() );
       i != fb_handles.end(); ++i ){
    const PU_PROFILE_HANDLE h = *i;
    const UINT64 addr = Cur_PU_Feedback->Get_Runtime_Func_Addr();

    if( addr == 0 ){
      Cur_PU_Feedback->Set_Runtime_Func_Addr( h->runtime_fun_address );

    } else if( addr != h->runtime_fun_address ){
      Cur_PU_Feedback->Set_Runtime_Func_Addr( 0 );
      break;
    }
  }
#endif
  Cur_PU_Feedback->Verify("after annotation");
}


void
Set_Instrumentation_File_Name( char *fname ) 
{
  if ( fname ) {
    Instrumentation_File_Name
      = (char *) malloc( sizeof( char ) *
			 ( strlen(fname)
			   + Instrumentation_Phase_Num / 10 + 2 ) );
    sprintf( Instrumentation_File_Name, "%s%d", fname,
	     Instrumentation_Phase_Num );
    // Instrumentation_File_Name = fname;
  } else {
    DevWarn( "Instrumenter Warning: Invalid instrumentation file name." );
    Instrumentation_File_Name = "";
  }
}

void
Set_Selective_Instrumentation_File_Name( char *fname ) 
{
  if ( fname ) {
    Selective_Instrumentation_File_Name
      = (char *) malloc( sizeof( char ) *
			 ( strlen(fname)+ 1 ) );
    sprintf( Selective_Instrumentation_File_Name, "%s", fname);
    // Instrumentation_File_Name = fname;
  } else {
    DevWarn( "Instrumenter Warning: Invalid instrumentation file name." );
    Selective_Instrumentation_File_Name = "";
  }
//  printf(Selective_Instrumentation_File_Name);
}
const int CALLS_LEN = 40;
const int NUM_MPI_CALLS = 559;
const int NUM_POMP_CALLS = 46;
const int NUM_OMP_CALLS = 21;
char mpi_calls[NUM_MPI_CALLS][CALLS_LEN] ={
"mpi_send",
"pmpi_send",
"mpi_recv", 
"pmpi_recv",
"mpi_get_count",
"pmpi_get_count",
"mpi_bsend",
"pmpi_bsend",
"mpi_ssend",
"pmpi_ssend",
"mpi_rsend",
"pmpi_rsend",
"mpi_buffer_attach",
"pmpi_buffer_attach",
"mpi_buffer_detach",
"pmpi_buffer_detach",
"mpi_isend",
"pmpi_isend",
"mpi_ibsend",
"pmpi_ibsend",
"mpi_issend",
"pmpi_issend",
"mpi_irsend",
"pmpi_irsend",
"mpi_irecv",
"pmpi_irecv",
"mpi_wait",
"pmpi_wait",
"mpi_test",
"pmpi_test",
"mpi_request_free",
"pmpi_request_free",
"mpi_waitany",
"pmpi_waitany",
"mpi_testany",
"pmpi_testany",
"mpi_waitall",
"pmpi_waitall",
"mpi_testall",
"pmpi_testall",
"mpi_waitsome",
"pmpi_waitsome",
"mpi_testsome",
"pmpi_testsome",
"mpi_iprobe",
"pmpi_iprobe",
"mpi_probe",
"pmpi_probe",
"mpi_cancel",
"pmpi_cancel",
"mpi_test_cancelled",
"pmpi_test_cancelled",
"mpi_send_init",
"pmpi_send_init",
"mpi_bsend_init",
"pmpi_bsend_init",
"mpi_ssend_init",
"pmpi_ssend_init",
"mpi_rsend_init",
"pmpi_rsend_init",
"mpi_recv_init",
"pmpi_recv_init",
"mpi_start",
"pmpi_start",
"mpi_startall",
"pmpi_startall",            
"pmpi_irsend",
"mpi_irecv",
"pmpi_irecv",
"mpi_wait",
"pmpi_wait",
"mpi_test",
"pmpi_test",
"mpi_request_free",
"pmpi_request_free",
"mpi_waitany",
"pmpi_waitany",
"mpi_testany",
"pmpi_testany",
"mpi_waitall",
"pmpi_waitall",
"mpi_testall",
"pmpi_testall",
"mpi_waitsome",
"pmpi_waitsome",
"mpi_testsome",
"pmpi_testsome",
"mpi_iprobe",
"pmpi_iprobe",
"mpi_sendrecv",
"pmpi_sendrecv",
"mpi_sendrecv_replace",
"pmpi_sendrecv_replace",
"mpi_type_contiguous",
"pmpi_type_contiguous",
"mpi_type_vector",
"pmpi_type_vector",
"mpi_type_hvector",
"pmpi_type_hvector",
"mpi_type_indexed",
"pmpi_type_indexed",
"mpi_type_hindexed",
"pmpi_type_hindexed",
"mpi_type_struct",
"pmpi_type_struct",
"mpi_address",
"pmpi_address",
"mpi_type_extent",
"pmpi_type_extent",
"mpi_type_size",
"pmpi_type_size",
"mpi_type_lb",
"pmpi_type_lb",
"mpi_type_ub",
"pmpi_type_ub",
"mpi_type_commit",
"pmpi_type_commit",
"mpi_type_free",
"pmpi_type_free",
"mpi_get_elements",
"pmpi_get_elements",
"mpi_pack",
"pmpi_pack",
"mpi_unpack",
"pmpi_unpack",
"mpi_pack_size",
"pmpi_pack_size",
"mpi_barrier",
"pmpi_barrier",
"mpi_bcast",
"pmpi_bcast",
"mpi_gather",
"pmpi_gather",
"mpi_gatherv",
"pmpi_gatherv",
"mpi_scatter",
"pmpi_scatter",
"mpi_scatterv",
"pmpi_scatterv",
"mpi_allgather",
"pmpi_allgather",
"mpi_allgatherv",
"pmpi_allgatherv",
"mpi_alltoall",
"pmpi_alltoall",
"mpi_alltoallv",
"pmpi_alltoallv",
"mpi_reduce",
"pmpi_reduce",
"mpi_op_create",
"pmpi_op_create",
"mpi_op_free",
"pmpi_op_free",
"mpi_allreduce",
"pmpi_allreduce",
"mpi_reduce_scatter",
"pmpi_reduce_scatter",
"mpi_scan",
"pmpi_scan",
"mpi_group_size",
"pmpi_group_size",
"mpi_group_rank",
"pmpi_group_rank",
"mpi_group_translate_ranks",
"pmpi_group_translate_ranks",
"mpi_group_compare",
"pmpi_group_compare",
"mpi_comm_group",
"pmpi_comm_group",
"mpi_group_union",
"pmpi_group_union",
"mpi_group_intersection",
"pmpi_group_intersection",
"mpi_group_difference",
"pmpi_group_difference",
"mpi_group_incl",
"pmpi_group_incl",
"mpi_group_excl",
"pmpi_group_excl",
"mpi_group_range_incl",
"pmpi_group_range_incl",
"mpi_group_range_excl",
"pmpi_group_range_excl",
"mpi_group_free",
"pmpi_group_free",
"mpi_comm_size",
"pmpi_comm_size",
"mpi_comm_rank",
"pmpi_comm_rank",
"mpi_comm_compare",
"pmpi_comm_compare",
"mpi_comm_dup",
"pmpi_comm_dup",
"mpi_comm_create",
"pmpi_comm_create",
"mpi_comm_split",
"pmpi_comm_split",
"mpi_comm_free",
"pmpi_comm_free",
"mpi_comm_test_inter",
"pmpi_comm_test_inter",
"mpi_comm_remote_size",
"pmpi_comm_remote_size",
"mpi_comm_remote_group",
"pmpi_comm_remote_group",
"mpi_intercomm_create",
"pmpi_intercomm_create",
"mpi_intercomm_merge",
"pmpi_intercomm_merge",
"mpi_keyval_create",
"pmpi_keyval_create",
"mpi_keyval_free",
"pmpi_keyval_free",
"mpi_attr_put",
"pmpi_attr_put",
"mpi_attr_get",
"pmpi_attr_get",
"mpi_attr_delete",
"pmpi_attr_delete",
"mpi_cart_create",
"pmpi_cart_create",
"mpi_dims_create",
"pmpi_dims_create",
"mpi_graph_create",
"pmpi_graph_create",
"mpi_topo_test",
"pmpi_topo_test",
"mpi_graphdims_get",
"pmpi_graphdims_get",
"mpi_graph_get",
"pmpi_graph_get",
"mpi_cartdim_get",
"pmpi_cartdim_get",
"mpi_cart_get",
"pmpi_cart_get",
"mpi_cart_rank",
"pmpi_cart_rank",
"mpi_cart_coords",
"pmpi_cart_coords",
"mpi_graph_neighbors_count",
"pmpi_graph_neighbors_count",
"mpi_graph_neighbors",
"pmpi_graph_neighbors",
"mpi_cart_shift",
"pmpi_cart_shift",
"mpi_cart_sub",
"pmpi_cart_sub",
"mpi_cart_map",
"pmpi_cart_map",
"mpi_graph_map",
"pmpi_graph_map",
"mpi_get_processor_name",
"pmpi_get_processor_name",
"mpi_errhandler_create",
"pmpi_errhandler_create",
"mpi_errhandler_set",
"pmpi_errhandler_set",
"mpi_errhandler_get",
"pmpi_errhandler_get",
"mpi_errhandler_free",
"pmpi_errhandler_free",
"mpi_error_string",
"pmpi_error_string",
"mpi_error_class",
"pmpi_error_class",
"mpi_wtime",
"pmpi_wtime",
"mpi_wtick",
"pmpi_wtick",
"mpi_init",
"pmpi_init",
"mpi_finalize",
"pmpi_finalize",
"mpi_initialized",
"pmpi_initialized",
"mpi_abort",
"pmpi_abort",
"mpi_pcontrol",
"pmpi_pcontrol",
"mpi_get_version",
"pmpi_get_version",
"mpi_request_get_status",
"pmpi_request_get_status",
"mpi_info_create",
"pmpi_info_create",
"mpi_info_delete",
"pmpi_info_delete",
"mpi_info_dup",
"pmpi_info_dup",
"mpi_info_free",
"pmpi_info_free",
"mpi_info_get",
"pmpi_info_get",
"mpi_info_get_nkeys",
"pmpi_info_get_nkeys",
"mpi_info_get_nthkey",
"pmpi_info_get_nthkey",
"mpi_info_get_valuelen",
"pmpi_info_get_valuelen",
"mpi_info_set",
"pmpi_info_set",
"mpi_alloc_mem",
"pmpi_alloc_mem",
"mpi_free_mem",
"pmpi_free_mem",
"mpi_info_c2f",
"pmpi_info_c2f",
"mpi_info_f2c",
"pmpi_info_f2c",
"mpi_comm_c2f",
"pmpi_comm_c2f",
"mpi_comm_f2c",
"pmpi_comm_f2c",
"mpi_type_c2f",
"pmpi_type_c2f",
"mpi_type_f2c",
"pmpi_type_f2c",
"mpi_group_c2f",
"pmpi_group_c2f",
"mpi_group_f2c",
"pmpi_group_f2c",
"mpi_request_c2f",
"pmpi_request_c2f",
"mpi_request_f2c",
"pmpi_request_f2c",
"mpi_op_c2f",
"pmpi_op_c2f",
"mpi_op_f2c",
"pmpi_op_f2c",
"mpi_type_create_hvector",
"pmpi_type_create_hvector",
"mpi_type_create_hindexed",
"pmpi_type_create_hindexed",
"mpi_type_create_struct",
"pmpi_type_create_struct",
"mpi_get_address",
"pmpi_get_address",
"mpi_comm_spawn",
"pmpi_comm_spawn",
"mpi_comm_spawn_multiple",
"pmpi_comm_spawn_multiple",
"mpi_comm_get_parent",
"pmpi_comm_get_parent",
"mpi_win_c2f",
"pmpi_win_c2f",
"mpi_win_f2c",
"pmpi_win_f2c",
"mpi_win_create",
"pmpi_win_create",
"mpi_win_fence",
"pmpi_win_fence",
"mpi_win_free",
"pmpi_win_free",
"mpi_win_lock",
"pmpi_win_lock",
"mpi_win_unlock",
"pmpi_win_unlock",
"mpi_put",
"pmpi_put",
"mpi_get",
"pmpi_get",
"mpi_accumulate",
"pmpi_accumulate",
"mpi_type_get_envelope",
"pmpi_type_get_envelope",
"mpi_type_get_contents",
"pmpi_type_get_contents",
"mpi_type_dup",
"pmpi_type_dup",
"mpi_grequest_complete",
"pmpi_grequest_complete",
"mpi_grequest_start",
"pmpi_grequest_start",
"mpi_status_set_cancelled",
"pmpi_status_set_cancelled",
"mpi_status_set_elements",
"pmpi_status_set_elements",
"mpi_init_thread",
"pmpi_init_thread",
"mpi_query_thread",
"pmpi_query_thread",
"mpi_is_thread_main",
"pmpi_is_thread_main",
"mpi_comm_create_keyval",
"pmpi_comm_create_keyval",
"mpi_comm_free_keyval",
"pmpi_comm_free_keyval",
"mpi_comm_set_attr",
"pmpi_comm_set_attr",
"mpi_comm_get_attr",
"pmpi_comm_get_attr",
"mpi_comm_delete_attr",
"pmpi_comm_delete_attr",
"mpi_win_create_keyval",
"pmpi_win_create_keyval",
"mpi_win_free_keyval",
"pmpi_win_free_keyval",
"mpi_win_set_attr",
"pmpi_win_set_attr",
"mpi_win_get_attr",
"pmpi_win_get_attr",
"mpi_win_delete_attr",
"pmpi_win_delete_attr",
"mpi_type_create_keyval",
"pmpi_type_create_keyval",
"mpi_type_free_keyval",
"pmpi_type_free_keyval",
"mpi_type_set_attr",
"pmpi_type_set_attr",
"mpi_type_get_attr",
"pmpi_type_get_attr",
"mpi_type_delete_attr",
"pmpi_type_delete_attr",
"mpi_finalized",
"pmpi_finalized",
"mpi_file_open",
"mpi_file_close",
"mpi_file_delete",
"mpi_file_set_size",
"mpi_file_preallocate",
"mpi_file_get_size",
"mpi_file_get_group",
"mpi_file_get_amode",
"mpi_file_set_info",
"mpi_file_get_info",
"mpi_file_set_view",
"mpi_file_get_view",
"mpi_file_read_at",
"mpi_file_read_at_all",
"mpi_file_write_at",
"mpi_file_write_at_all",
"mpi_file_iread_at",
"mpi_file_iwrite_at",
"mpi_file_read",
"mpi_file_read_all",
"mpi_file_write",
"mpi_file_write_all",
"mpi_file_iread",
"mpi_file_iwrite",
"mpi_file_seek",
"mpi_file_get_position",
"mpi_file_get_byte_offset",
"mpi_file_read_shared",
"mpi_file_write_shared",
"mpi_file_iread_shared",
"mpi_file_iwrite_shared",
"mpi_file_read_ordered",
"mpi_file_write_ordered",
"mpi_file_seek_shared",
"mpi_file_get_position_shared",
"mpi_file_read_at_all_begin",
"mpi_file_read_at_all_end",
"mpi_file_write_at_all_begin",
"mpi_file_write_at_all_end",
"mpi_file_read_all_begin",
"mpi_file_read_all_end",
"mpi_file_write_all_begin",
"mpi_file_write_all_end",
"mpi_file_read_ordered_begin",
"mpi_file_read_ordered_end",
"mpi_file_write_ordered_begin",
"mpi_file_write_ordered_end",
"mpi_file_get_type_extent",
"mpi_file_set_atomicity",
"mpi_file_get_atomicity",
"mpi_file_sync",
"mpi_file_set_errhandler",
"mpi_file_get_errhandler",
"mpi_type_create_subarray",
"mpi_type_create_darray",
"mpi_file_f2c",
"mpi_file_c2f",
"mpio_test",
"mpio_wait",
"mpi_request_c2f",
"mpi_request_f2c",
"mpi_info_create",
"mpi_info_set",
"mpi_info_delete",
"mpi_info_get",
"mpi_info_get_valuelen",
"mpi_info_get_nkeys",
"mpi_info_get_nthkey",
"mpi_info_dup",
"mpi_info_free",
"mpi_info_c2f",
"mpi_info_f2c",
"pmpi_file_open",
"pmpi_file_close",
"pmpi_file_delete",
"pmpi_file_set_size",
"pmpi_file_preallocate",
"pmpi_file_get_size",
"pmpi_file_get_group",
"pmpi_file_get_amode",
"pmpi_file_set_info",
"pmpi_file_get_info",
"pmpi_file_set_view",
"pmpi_file_get_view",
"pmpi_file_read_at",
"pmpi_file_read_at_all",
"pmpi_file_write_at",
"pmpi_file_write_at_all",
"pmpi_file_iread_at",
"pmpi_file_iwrite_at",
"pmpi_file_read",
"pmpi_file_read_all",
"pmpi_file_write",
"pmpi_file_write_all",
"pmpi_file_iread",
"pmpi_file_iwrite",
"pmpi_file_seek",
"pmpi_file_get_position",
"pmpi_file_get_byte_offset",
"pmpi_file_read_shared",
"pmpi_file_write_shared",
"pmpi_file_iread_shared",
"pmpi_file_iwrite_shared",
"pmpi_file_read_ordered",
"pmpi_file_write_ordered",
"pmpi_file_seek_shared",
"pmpi_file_get_position_shared",
"pmpi_file_read_at_all_begin",
"pmpi_file_read_at_all_end",
"pmpi_file_write_at_all_begin",
"pmpi_file_write_at_all_end",
"pmpi_file_read_all_begin",
"pmpi_file_read_all_end",
"pmpi_file_write_all_begin",
"pmpi_file_write_all_end",
"pmpi_file_read_ordered_begin",
"pmpi_file_read_ordered_end",
"pmpi_file_write_ordered_begin",
"pmpi_file_write_ordered_end",
"pmpi_file_get_type_extent",
"pmpi_file_set_atomicity",
"pmpi_file_get_atomicity",
"pmpi_file_sync",
"pmpi_file_set_errhandler",
"pmpi_file_get_errhandler",
"pmpi_type_create_subarray",
"pmpi_type_create_darray",
"pmpi_file_f2c",
"pmpi_file_c2f",
"pmpio_test",
"pmpio_wait",
"pmpi_request_c2f",
"pmpi_request_f2c",
"pmpi_info_create",
"pmpi_info_set",
"pmpi_info_delete",
"pmpi_info_get",
"pmpi_info_get_valuelen",
"pmpi_info_get_nkeys",
"pmpi_info_get_nthkey",
"pmpi_info_dup",
"pmpi_info_free",
"pmpi_info_c2f",
"pmpi_info_f2c"
};


char omp_calls[NUM_OMP_CALLS][CALLS_LEN] = {
"omp_set_num_threads",
"omp_get_num_threads",
"omp_get_max_threads",
"omp_get_thread_num",
"omp_in_parallel",
"omp_set_dynamic",
"omp_get_dynamic",
"omp_set_nested",
"omp_get_nested",
"omp_init_lock",
"omp_init_nest_lock",
"omp_destroy_lock",
"omp_destroy_next_lock",
"omp_set_lock",
"omp_set_nest_lock",
"omp_unset_lock",
"omp_unset_nest_lock",
"omp_test_lock",
"omp_test_nest_lock",
"omp_get_wtick",
"omp_get_wtime"
};



char pomp_calls[NUM_POMP_CALLS][CALLS_LEN]= {
"pomp_finalize",
"pomp_init",
"pomp_off",
"pomp_on",
"pomp_begin",
"pomp_end",
"pomp_atomic_enter",
"pomp_atomic_exit",
"pomp_barrier_enter",
"pomp_barrier_exit",
"pomp_flush_enter",
"pomp_flush_exit",
"pomp_critical_begin",
"pomp_critical_end",
"pomp_critical_enter",
"pomp_critical_exit",
"pomp_for_enter",
"pomp_for_exit",
"pomp_do_enter",
"pomp_do_exit",
"pomp_master_begin",
"pomp_master_end",
"pomp_parallel_begin",
"pomp_parallel_end",
"pomp_parallel_fork",
"pomp_parallel_join",
"pomp_section_begin",
"pomp_section_end",
"pomp_sections_enter",
"pomp_sections_exit",
"pomp_single_begin",
"pomp_single_end",
"pomp_single_enter",
"pomp_single_exit",
"pomp_workshare_enter",
"pomp_workshare_exit",
"pomp_init_lock",
"pomp_destroy_lock",
"pomp_set_lock",
"pomp_unset_lock",
"pomp_test_lock",
"pomp_init_nest_lock",
"pomp_destroy_nest_lock",
"pomp_set_nest_lock",
"pomp_unset_nest_lock",
"pomp_test_nest_lock"
};


BOOL debug_copper = FALSE;
BOOL is_mpi_or_pomp(char *s)
{

   BOOL res = FALSE;
   for(int i=0; i<NUM_MPI_CALLS;i++)
    {
      char *substring =  strstr(s,mpi_calls[i]);

      if (substring !=NULL)
       {

         if(substring[0]==s[0])
          {
           if(debug_copper) {
           printf("\nmhit: ");
           printf(s);
           printf(":");
           printf(substring);
           printf(":");
           printf(mpi_calls[i]);
           printf(":%d:",i);
           printf(mpi_calls[NUM_MPI_CALLS-1]); }
           res = TRUE;
           break;
          }
         substring = NULL;  
       }

    }
  if (!res) {
   for(int i=0; i<NUM_POMP_CALLS;i++)
    {
      char *substring =  strstr(s,pomp_calls[i]);

      if (substring !=NULL)
       {

         if(substring[0]==s[0])
          {
           if(debug_copper) {
           printf("\nphit: ");
           printf(s);
           printf(":");
           printf(substring);
           printf(":");
           printf(pomp_calls[i]);
           printf(":%d",i); }

           res = TRUE;
           break;
          }
         substring = NULL;
       }
    

    }
  }

   
   if (!res) {
   for(int i=0; i<NUM_OMP_CALLS;i++)
    {
      char *substring =  strstr(s,omp_calls[i]);

      if (substring !=NULL)
       {

         if(substring[0]==s[0])
          {
           if(debug_copper) {
           printf("\nphit: ");
           printf(s);
           printf(":");
           printf(substring);
           printf(":");
           printf(pomp_calls[i]);
           printf(":%d",i); }

           res = TRUE;
           break;
          }
         substring = NULL;
       }
    

    }
  }

   return res;
}

// ====================================================================
