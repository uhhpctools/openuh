/*
 * Copyright (C) 2011 Advanced Micro Devices, Inc.  All Rights Reserved.
 */

/*
 * Copyright 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
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


#include <stdint.h>
#ifdef USE_PCH
#include "be_com_pch.h"
#endif /* USE_PCH */
#pragma hdrstop
#include <sys/types.h>
#if defined(BUILD_OS_DARWIN)
#include <darwin_elf.h>
#else /* defined(BUILD_OS_DARWIN) */
#include <elf.h>
#endif /* defined(BUILD_OS_DARWIN) */
#include <stdio.h> 

#define USE_STANDARD_TYPES
#include "wn.h"
#include "stab.h"
#include "cxx_template.h"
#include "cxx_memory.h" 
#include "anl_driver.h"
#include "prompf.h"
#include "ir_reader.h"
#include "targ_sim.h"

#ifndef BUILD_SKIP_PROMPF
#ifdef SHARED_BUILD
#pragma weak New_Construct_Id 
#pragma weak Get_Next_Construct_Id
#pragma weak Anl_File_Path
#endif //SHARED_BUILD
#else
extern "C" INT64 Get_Next_Construct_Id() 
{
	FmtAssert(FALSE, ("NYI"));
	return 0; 
}
extern "C" INT64 New_Construct_Id() 
{ 
	FmtAssert(FALSE, ("NYI"));
	return 0; 
}
extern "C" const char* Anl_File_Path()
{
	FmtAssert(FALSE, ("NYI"));
	return 0; 
}
#endif

PROMPF_INFO* Prompf_Info = NULL; 
MEM_POOL PROMPF_pool;

//-----------------------------------------------------------------------
// NAME: prompf_chain 
// FUNCTION: For each PROMPF_TRANS_TYPE, gives the corresponding 
//   MPF_CHAIN_TYPE.  
//-----------------------------------------------------------------------

PROMPF_CHAIN_TYPE prompf_chain[] = {
  /* MPF_UNKNOWN,	      */  MPF_CHAIN_INVALID,
  /* MPF_MARK_F90_LOWER,      */  MPF_CHAIN_TRANSMIT,
  /* MPF_MARK_OMP,	      */  MPF_CHAIN_TRANSMIT,
  /* MPF_MARK_PREOPT,	      */  MPF_CHAIN_TRANSMIT,
  /* MPF_MARK_PRELNO,	      */  MPF_CHAIN_TRANSMIT,
  /* MPF_MARK_POSTLNO,	      */  MPF_CHAIN_TRANSMIT,
  /* MPF_ELIMINATION,	      */  MPF_CHAIN_TRANSFORM,
  /* MPF_FUSION,	      */  MPF_CHAIN_TRANSFORM,
  /* MPF_FISSION,	      */  MPF_CHAIN_TRANSFORM,	
  /* MPF_DISTRIBUTION,	      */  MPF_CHAIN_TRANSFORM,
  /* MPF_INTERCHANGE	      */  MPF_CHAIN_TRANSMIT,
  /* MPF_PRE_PEEL             */  MPF_CHAIN_TRANSFORM,
  /* MPF_POST_PEEL            */  MPF_CHAIN_TRANSFORM, 
  /* MPF_MP_TILE              */  MPF_CHAIN_TRANSFORM,
  /* MPF_DSM_TILE             */  MPF_CHAIN_TRANSFORM,
  /* MPF_DONEST_OUTER_TILE    */  MPF_CHAIN_TRANSFORM, 
  /* MPF_DONEST_MIDDLE_TILE   */  MPF_CHAIN_TRANSFORM, 
  /* MPF_DSM_LOCAL            */  MPF_CHAIN_TRANSFORM,
  /* MPF_DSM_IO               */  MPF_CHAIN_TRANSFORM, 
  /* MPF_SINGLE_PROCESS       */  MPF_CHAIN_TRANSFORM,
  /* MPF_MP_VERSION	      */  MPF_CHAIN_TRANSFORM,
  /* MPF_PARALLEL_REGION      */  MPF_CHAIN_TRANSFORM,
  /* MPF_HOIST_MESSY_BOUNDS   */  MPF_CHAIN_TRANSFORM,
  /* MPF_DOACROSS_SYNC 	      */  MPF_CHAIN_TRANSFORM,
  /* MPF_DOACROSS_OUTER_TILE  */  MPF_CHAIN_TRANSFORM,
  /* MPF_DOACROSS_INNER_TILE  */  MPF_CHAIN_TRANSFORM,
  /* MPF_REMOVE_UNITY_TRIP    */  MPF_CHAIN_TRANSFORM,
  /* MPF_CACHE_WINDDOWN	      */  MPF_CHAIN_TRANSFORM, 
  /* MPF_INTERLEAVED_WINDDOWN */  MPF_CHAIN_TRANSFORM, 
  /* MPF_GENERAL_VERSION      */  MPF_CHAIN_TRANSFORM,
  /* MPF_CACHE_TILE           */  MPF_CHAIN_TRANSFORM,
  /* MPF_REGISTER_WINDDOWN    */  MPF_CHAIN_TRANSFORM,
  /* MPF_REGISTER_SSTRIP      */  MPF_CHAIN_TRANSFORM,
  /* MPF_REGISTER_TILE        */  MPF_CHAIN_TRANSMIT,
  /* MPF_REGISTER_STARTUP     */  MPF_CHAIN_TRANSFORM,
  /* MPF_REGISTER_SHUTDOWN    */  MPF_CHAIN_TRANSFORM,
  /* MPF_SE_TILE              */  MPF_CHAIN_TRANSFORM,
  /* MPF_SE_CACHE_TILE        */  MPF_CHAIN_TRANSFORM,
  /* MPF_INNER_FISSION        */  MPF_CHAIN_TRANSFORM,
  /* MPF_GATHER_SCATTER       */  MPF_CHAIN_TRANSFORM,
  /* MPF_VINTR_FISSION        */  MPF_CHAIN_TRANSFORM,
  /* MPF_PREFETCH_VERSION     */  MPF_CHAIN_TRANSFORM,
  /* MPF_OMPL_SECTIONS_LOOP   */  MPF_CHAIN_TRANSMIT,
  /* MPF_OMPL_ELIM_SECTION    */  MPF_CHAIN_TRANSFORM,
  /* MPF_OMPL_ATOMIC_CSECTION */  MPF_CHAIN_TRANSMIT, 
  /* MPF_OMPL_ATOMIC_SWAP     */  MPF_CHAIN_TRANSFORM,
  /* MPF_OMPL_ATOMIC_FETCHOP  */  MPF_CHAIN_TRANSFORM,
  /* MPF_OMPL_MASTER_IF	      */  MPF_CHAIN_TRANSFORM,
  /* MPF_OMPL_FETCHOP_ATOMIC  */  MPF_CHAIN_TRANSFORM,
  /* MPF_F90_ARRAY_STMT       */  MPF_CHAIN_TRANSFORM,
  /* MPF_OUTER_SHACKLE        */  MPF_CHAIN_TRANSFORM,
  /* MPF_INNER_SHACKLE        */  MPF_CHAIN_TRANSFORM,
  /* MPF_PREOPT_CREATE        */  MPF_CHAIN_TRANSFORM,
}; 

//-----------------------------------------------------------------------
// NAME: PROMPF_LINES::Add_Lines
// FUNCTION: Walk the tree rooted at 'wn_tree', adding the line numbers 
//   of all of the statements to the PROMPF_LINES. 
//-----------------------------------------------------------------------

void PROMPF_LINES::Add_Lines(WN* wn_tree)
{
   if (wn_tree == NULL) 
     return; 

   if (OPCODE_has_next_prev(WN_opcode(wn_tree)))
     Add_Line(WN_linenum(wn_tree)); 

   if (OPCODE_is_expression(WN_opcode(wn_tree)))  
     return; 

   if (WN_opcode(wn_tree) == OPC_BLOCK) { 
     for (WN* wn = WN_first(wn_tree); wn != NULL; wn = WN_next(wn)) 
       Add_Lines(wn); 
   } else { 
     INT i;
     for (i = 0; i < WN_kid_count(wn_tree); i++) 
       Add_Lines(WN_kid(wn_tree, i)); 
   } 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_LINES::PROMPF_LINES 
// FUNCTION: Create a PROMPF_LINES containing the line numbers of all of 
//   the statements in the tree 'wn_tree'.  Create the PROMPF_LINES out 
//   of memory from 'pool'. 
//-----------------------------------------------------------------------

PROMPF_LINES::PROMPF_LINES(WN* wn_tree, 
			   MEM_POOL* pool): 
  _pool(pool), _low(pool), _high(pool)
{
  Add_Lines(wn_tree);  
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_LINES::Sections 
// FUNCTION: Returns the number of sections in the set of line number 
//   ranges. 
// EXAMPLE: The PROMPF_LINES with line numbers 1-7,9-15 has 2 sections.
//-----------------------------------------------------------------------

INT PROMPF_LINES::Sections()
{
  FmtAssert(_low.Elements() == _high.Elements(), 
    ("PROMPF_LINES::Sections: high and low range counts do not match")); 
  return _low.Elements(); 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_LINES::Low 
// FUNCTION: Returns the low end of the 'i'-th line number range in the 
//   PROMPF_LINES. 
//-----------------------------------------------------------------------

INT PROMPF_LINES::Low(INT i)
{
  FmtAssert(i >= 0 && i <= _low.Elements(), 
    ("PROMPF_LINES::Low: Low part of section does not exist"));
  return _low.Bottom_nth(i); 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_LINES::High 
// FUNCTION: Returns the high end of the 'i'-th line number range in the 
//   PROMPF_LINES. 
//-----------------------------------------------------------------------

INT PROMPF_LINES::High(INT i)
{
  FmtAssert(i >= 0 && i <= _high.Elements(), 
    ("PROMPF_LINES::High: High part of section does not exist")); 
  return _high.Bottom_nth(i); 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_LINES::Print 
// FUNCTION: Print the PROMPF_LINES to the file 'fp'. 
//-----------------------------------------------------------------------

void PROMPF_LINES::Print(FILE* fp)
{
  INT i;
  for (i = 0; i < Sections(); i++) { 
    fprintf(fp, "<%d:%d>", Low(i), High(i)); 
    if (i < Sections() - 1) 
      fprintf(fp, ","); 
  }
}

//-----------------------------------------------------------------------
// NAME: PROMPF_LINES::Print_Compact 
// FUNCTION: Print the PROMPF_LINES to the file 'fp' in compact form. 
//   (This is the form used by the PROMPF anl file.)  
//-----------------------------------------------------------------------

INT PROMPF_LINES::Print_Compact(FILE* fp, 
				BOOL print_brackets) 
				
{
  INT field_count = 0; 
  field_count += fprintf(fp, "lines "); 
  if (print_brackets) 
    fprintf(fp, "["); 
  INT i;
  for (i = 0; i < Sections(); i++) { 
    if (Low(i) == High(i))
      field_count += fprintf(fp, "%d", Low(i)); 
    else 
      field_count += fprintf(fp, "%d-%d", Low(i), High(i)); 
    if (i < Sections() - 1) 
      field_count += fprintf(fp, ","); 
  }
  if (print_brackets) 
    fprintf(fp, "]"); 
  return field_count; 
}

//-----------------------------------------------------------------------
// NAME: PROMPF_LINES::Add_Line 
// FUNCTION: Add the linenumber 'line' to the PROMPF_LINES. 
//-----------------------------------------------------------------------

void PROMPF_LINES::Add_Line(INT line) 
{ 
  FmtAssert(line >= 0, 
    ("PROMPF_LINES::Add_Line: Line number should not be negative")); 
  if (line == 0)
    return; 
  if (Sections() == 0) { 
    _low.Push(line); 
    _high.Push(line); 
   return; 
  } 
  INT i;
  for (i = 0; i < Sections(); i++) {
    if (line < Low(i)) 
      break; 
    if (line >= Low(i) && line <= High(i)) { 
      return; 
    } 
  }
  BOOL adjacent_low = i < Sections() && line == Low(i) - 1;  
  BOOL adjacent_high = i > 0 && line == High(i-1) + 1;
  if (adjacent_low) { 
    if (!adjacent_high) { 
      _low.Bottom_nth(i)--; 
    } else {  
      _high.Bottom_nth(i-1) = High(i); 
      STACK<INT> new_low(_pool);
      STACK<INT> new_high(_pool);
      INT replace_count = Sections() - i - 1; 
      INT j;
      for (j = 0; j < replace_count; j++) {
        INT lowpop = _low.Pop(); 
	new_low.Push(lowpop);
        INT highpop = _high.Pop();  
	new_high.Push(highpop); 
      } 
      _low.Pop(); 
      _high.Pop(); 
      for (j = 0; j < replace_count; j++) {
	INT newlowpop = new_low.Pop(); 
	_low.Push(newlowpop); 
        INT newhighpop = new_high.Pop(); 
	_high.Push(newhighpop); 
      } 
    } 
  } else { 
    if (adjacent_high) { 
      _high.Bottom_nth(i-1)++; 
    } else { 
      STACK<INT> new_low(_pool);   
      STACK<INT> new_high(_pool);  
      INT replace_count = Sections() - i;  
      INT j;
      for (j = 0; j < replace_count; j++) {
        INT lowpop = _low.Pop(); 
	new_low.Push(lowpop);
        INT highpop = _high.Pop();  
	new_high.Push(highpop); 
      } 
      _low.Push(line); 
      _high.Push(line); 
      for (j = 0; j < replace_count; j++) {
	INT newlowpop = new_low.Pop(); 
	_low.Push(newlowpop); 
        INT newhighpop = new_high.Pop(); 
	_high.Push(newhighpop); 
      } 
    }
  } 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_TRANS::Old_Loop 
// FUNCTION: Returns the id of the PROMPF_TRANS's 'i'th old loop. 
//-----------------------------------------------------------------------

INT PROMPF_TRANS::Old_Loop(INT i)
{
  FmtAssert(i >= 0 && i < _old_loops.Elements(), 
    ("PROMPF_TRANS::Old_Loop() index out of range")); 
  return _old_loops.Bottom_nth(i); 
}

//-----------------------------------------------------------------------
// NAME: PROMPF_TRANS::New_Loop 
// FUNCTION: Returns the id of the PROMPF_TRANS's 'i'th new loop. 
//-----------------------------------------------------------------------

INT PROMPF_TRANS::New_Loop(INT i)
{
  FmtAssert(i >= 0 && i < _new_loops.Elements(), 
    ("PROMPF_TRANS::New_Loop() index out of range")); 
  return _new_loops.Bottom_nth(i); 
}

//-----------------------------------------------------------------------
// NAME: PROMPF_TRANS::Prev_Loop
// FUNCTION: Returns the transaction index of the previous instance of 
//   the PROMPF_TRANS's 'i'th old loop.  Returns -1 if there is no previous 
//   instance.   
//-----------------------------------------------------------------------

INT PROMPF_TRANS::Prev_Loop(INT i)
{
  FmtAssert(i >= 0 && i < _prev_loops.Elements(), 
    ("PROMPF_TRANS::Prev_Loop() index out of range")); 
  return _prev_loops.Bottom_nth(i); 
}

//-----------------------------------------------------------------------
// NAME: PROMPF_TRANS:: Add_Index_Name
// FUNCTION: Add the name of the newly created index variable 'index_name' 
//   to the PROMPF_TRANS. 
//-----------------------------------------------------------------------

void PROMPF_TRANS::Add_Index_Name(char* index_name)
{ 
  char* name = (char*) CXX_NEW_ARRAY(char, strlen(index_name) + 1, _pool);
  strcpy(name, index_name);
  _index_name = name; 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_TRANS::Print 
// FUNCTION: Print the PROMPF_TRANS to the file 'fp'.  
//-----------------------------------------------------------------------

void PROMPF_TRANS::Print(FILE* fp)
{
  const INT MIDDLE_FIELD_COUNT = 30; 
  switch (_type) {
  case MPF_UNKNOWN: 
    fprintf(fp, "UNKNOWN              "); 
    break;
  case MPF_MARK_OMP: 
    fprintf(fp, "MARK OMP             "); 
    break;
  case MPF_MARK_PREOPT: 
    fprintf(fp, "MARK PREOPT          "); 
    break;
  case MPF_MARK_PRELNO: 
    fprintf(fp, "MARK PRELNO          "); 
    break;
  case MPF_MARK_POSTLNO: 
    fprintf(fp, "MARK POSTLNO         "); 
    break;
  case MPF_ELIMINATION: 
    fprintf(fp, "ELIMINATION          ");  
    break;
  case MPF_FUSION:
    fprintf(fp, "FUSION               ");  
    break;
  case MPF_FISSION:
    fprintf(fp, "FISSION              ");  
    break;
  case MPF_DISTRIBUTION:
    fprintf(fp, "DISTRIBUTION         ");  
    break;
  case MPF_INTERCHANGE: 
    fprintf(fp, "INTERCHANGE          ");  
    break;
  case MPF_PRE_PEEL: 
    fprintf(fp, "PRE-LOOP PEELING     "); 
    break; 
  case MPF_POST_PEEL: 
    fprintf(fp, "POST-LOOP PEELING    "); 
    break; 
  case MPF_MP_TILE: 
    fprintf(fp, "MP TILE              ");
    break;
  case MPF_DSM_TILE: 
    fprintf(fp, "DSM TILE             ");
    break;
  case MPF_DONEST_OUTER_TILE: 
    fprintf(fp, "DONEST OUTER TILE    ");
    break;
  case MPF_DONEST_MIDDLE_TILE: 
    fprintf(fp, "DONEST MIDDLE TILE   ");
    break;
  case MPF_DSM_LOCAL: 
    fprintf(fp, "DSM LOCAL            "); 
    break; 
  case MPF_DSM_IO: 
    fprintf(fp, "DSM IO               "); 
    break; 
  case MPF_SINGLE_PROCESS: 
    fprintf(fp, "SINGLE PROCESS       "); 
    break; 
  case MPF_MP_VERSION: 
    fprintf(fp, "MP VERSION           "); 
    break; 
  case MPF_PARALLEL_REGION:
    fprintf(fp, "PARALLEL REGION      "); 
    break; 
  case MPF_HOIST_MESSY_BOUNDS: 
    fprintf(fp, "HOIST MESSY BOUNDS   ");
    break;
  case MPF_DOACROSS_SYNC: 
    fprintf(fp, "DOACROSS SYNC        "); 
    break;
  case MPF_DOACROSS_OUTER_TILE: 
    fprintf(fp, "DOACROSS OUTER TILE  ");
    break;
  case MPF_DOACROSS_INNER_TILE: 
    fprintf(fp, "DOACROSS INNER TILE  ");
    break;
  case MPF_REMOVE_UNITY_TRIP: 
    fprintf(fp, "REMOVE UNITY TRIP    ");
    break;
  case MPF_CACHE_WINDDOWN: 
    fprintf(fp, "CACHE WINDDOWN       ");
    break;
  case MPF_INTERLEAVED_WINDDOWN: 
    fprintf(fp, "INTERLEAVED WINDDOWN ");
    break;
  case MPF_GENERAL_VERSION: 
    fprintf(fp, "GENERAL VERSION      ");
    break;
  case MPF_CACHE_TILE: 
    fprintf(fp, "CACHE TILE           "); 
    break; 
  case MPF_REGISTER_WINDDOWN: 
    fprintf(fp, "REGISTER WINDDOWN    ");
    break;
  case MPF_REGISTER_SSTRIP: 
    fprintf(fp, "REGISTER SSTRIP      ");
    break;
  case MPF_REGISTER_TILE: 
    fprintf(fp, "REGISTER TILE        ");
    break;
  case MPF_REGISTER_STARTUP: 
    fprintf(fp, "REGISTER STARTUP     ");
    break;
  case MPF_REGISTER_SHUTDOWN: 
    fprintf(fp, "REGISTER SHUTDOWN    ");
    break;
  case MPF_SE_TILE: 
    fprintf(fp, "SE TILE              ");
    break;
  case MPF_SE_CACHE_TILE: 
    fprintf(fp, "SE CACHE TILE        ");
    break;
  case MPF_INNER_FISSION: 
    fprintf(fp, "INNER FISSION 	      "); 
    break; 
  case MPF_GATHER_SCATTER: 
    fprintf(fp, "GATHER SCATTER       "); 
    break; 
  case MPF_VINTR_FISSION: 
    fprintf(fp, "VINTR FISSION 	      "); 
    break; 
  case MPF_PREFETCH_VERSION: 
    fprintf(fp, "PREFETCH VERSION     "); 
    break; 
  case MPF_OMPL_SECTIONS_LOOP: 
    fprintf(fp, "OMPL SECTIONS LOOP   "); 
    break;
  case MPF_OMPL_ELIM_SECTION: 
    fprintf(fp, "OMPL ELIM SECTION    "); 
    break;
  case MPF_OMPL_ATOMIC_CSECTION: 
    fprintf(fp, "OMPL ATOMIC CSECTION "); 
    break; 
  case MPF_OMPL_ATOMIC_SWAP: 
    fprintf(fp, "OMPL ATOMIC SWAP     ");
    break;
  case MPF_OMPL_ATOMIC_FETCHOP: 
    fprintf(fp, "OMPL ATOMIC FETCHOP  "); 
    break;
  case MPF_OMPL_MASTER_IF: 
    fprintf(fp, "OMPL MASTER IF       ");
    break; 
  case MPF_OMPL_FETCHOP_ATOMIC:
    fprintf(fp, "OMPL FETCHOP ATOMIC  ");
    break;
  case MPF_F90_ARRAY_STMT: 
    fprintf(fp, "F90 ARRAY STMT       ");
    break;
  case MPF_OUTER_SHACKLE:
    fprintf(fp, "OUTER SHACKLE        ");
    break;
  case MPF_INNER_SHACKLE:
    fprintf(fp, "INNER SHACKLE        ");
    break;
  case MPF_PREOPT_CREATE: 
    fprintf(fp, "PREOPT CREATE	      ");
    break; 
  } 
  INT field_count = 0; 
  field_count += fprintf(fp, "("); 
  INT i;
  for (i = 0; i < _old_loops.Elements(); i++) {
    field_count += fprintf(fp, "%d", _old_loops.Bottom_nth(i));  
    if (i < _old_loops.Elements() - 1)
      field_count += fprintf(fp, ",");
  } 
  field_count += fprintf(fp, ") -> ("); 
  for (i = 0; i < _new_loops.Elements(); i++) {
    field_count += fprintf(fp, "%d", _new_loops.Bottom_nth(i));  
    if (i < _new_loops.Elements() - 1)
      field_count += fprintf(fp, ",");
  } 
  field_count += fprintf(fp, ") "); 
  for (i = field_count + 1; i < MIDDLE_FIELD_COUNT; i++) 
    fprintf(fp, " "); 
  fprintf(fp, "["); 
  for (i = 0; i < _prev_loops.Elements(); i++) {
    fprintf(fp, "%d", _prev_loops.Bottom_nth(i));
    if (i < _prev_loops.Elements() - 1)
      fprintf(fp, ",");
  } 
  fprintf(fp, "] "); 
  for (i = 0; i < _old_lines.Elements(); i++) {
    _old_lines.Bottom_nth(i)->Print(fp); 
    if (i < _old_lines.Elements() - 1)
      fprintf(fp, ",");
  } 
  fprintf(fp, " "); 
  for (i = 0; i < _new_lines.Elements(); i++) {
    _new_lines.Bottom_nth(i)->Print(fp); 
    if (i < _new_lines.Elements() - 1)
      fprintf(fp, ",");
  } 
  fprintf(fp, " "); 
  if (_index_name != NULL) 
    fprintf(fp, "\"%s\"", _index_name);
  fprintf(fp, "\n"); 
}

//-----------------------------------------------------------------------
// NAME: PROMPF_TRANS::Print_Compact 
// FUNCTION: Print the PROMPF_TRANS in compact form to the file 'fp'.  
//   (This is the form used by the PROMPF anl file.)  
//-----------------------------------------------------------------------

void PROMPF_TRANS::Print_Compact(FILE* fp)
{
  switch (_type) {
  case MPF_UNKNOWN: 
    fprintf(fp, "UNKNOWN              "); 
    break;
  case MPF_MARK_OMP: 
    fprintf(fp, "MARK OMP             "); 
    break;
  case MPF_MARK_PREOPT: 
    fprintf(fp, "MARK PREOPT          "); 
    break;
  case MPF_MARK_PRELNO: 
    fprintf(fp, "MARK PRELNO          "); 
    break;
  case MPF_MARK_POSTLNO: 
    fprintf(fp, "MARK POSTLNO         "); 
    break;
  case MPF_ELIMINATION: 
    fprintf(fp, "ELIMINATION          ");  
    break;
  case MPF_FUSION:
    fprintf(fp, "FUSION               ");  
    break;
  case MPF_FISSION:
    fprintf(fp, "FISSION              ");  
    break;
  case MPF_DISTRIBUTION:
    fprintf(fp, "DISTRIBUTION         ");  
    break;
  case MPF_INTERCHANGE: 
    fprintf(fp, "INTERCHANGE          ");  
    break;
  case MPF_PRE_PEEL: 
    fprintf(fp, "PRE_LOOP_PEELING     "); 
    break; 
  case MPF_POST_PEEL: 
    fprintf(fp, "POST_LOOP_PEELING    "); 
    break; 
  case MPF_MP_TILE: 
    fprintf(fp, "MP_TILE              ");
    break;
  case MPF_DSM_TILE: 
    fprintf(fp, "DSM_TILE             ");
    break;
  case MPF_DONEST_OUTER_TILE: 
    fprintf(fp, "DONEST_OUTER_TILE    ");
    break;
  case MPF_DONEST_MIDDLE_TILE: 
    fprintf(fp, "DONEST_MIDDLE_TILE   ");
    break;
  case MPF_DSM_LOCAL: 
    fprintf(fp, "DSM_LOCAL            "); 
    break; 
  case MPF_DSM_IO: 
    fprintf(fp, "DSM_IO               "); 
    break; 
  case MPF_SINGLE_PROCESS: 
    fprintf(fp, "SINGLE_PROCESS       "); 
    break; 
  case MPF_MP_VERSION: 
    fprintf(fp, "MP_VERSION           "); 
    break; 
  case MPF_PARALLEL_REGION: 
    fprintf(fp, "PARALLEL_REGION      "); 
    break; 
  case MPF_HOIST_MESSY_BOUNDS: 
    fprintf(fp, "HOIST_MESSY_BOUNDS   ");
    break;
  case MPF_DOACROSS_SYNC: 
    fprintf(fp, "DOACROSS_SYNC        ");
    break; 
  case MPF_DOACROSS_OUTER_TILE: 
    fprintf(fp, "DOACROSS_OUTER_TILE  ");
    break; 
  case MPF_DOACROSS_INNER_TILE: 
    fprintf(fp, "DOACROSS_INNER_TILE  ");
    break; 
  case MPF_REMOVE_UNITY_TRIP: 
    fprintf(fp, "REMOVE_UNITY_TRIP    ");
    break;
  case MPF_CACHE_WINDDOWN: 
    fprintf(fp, "CACHE_WINDDOWN       ");
    break;
  case MPF_INTERLEAVED_WINDDOWN: 
    fprintf(fp, "INTERLEAVED_WINDDOWN ");
    break;
  case MPF_GENERAL_VERSION: 
    fprintf(fp, "GENERAL_VERSION      ");
    break;
  case MPF_CACHE_TILE: 
    fprintf(fp, "CACHE_TILE           "); 
    break; 
  case MPF_REGISTER_WINDDOWN: 
    fprintf(fp, "REGISTER_WINDDOWN    ");
    break;
  case MPF_REGISTER_SSTRIP: 
    fprintf(fp, "REGISTER_SSTRIP      ");
    break;
  case MPF_REGISTER_TILE: 
    fprintf(fp, "REGISTER_TILE        ");
    break;
  case MPF_REGISTER_STARTUP: 
    fprintf(fp, "REGISTER_STARTUP     ");
    break;
  case MPF_REGISTER_SHUTDOWN: 
    fprintf(fp, "REGISTER_SHUTDOWN    ");
    break;
  case MPF_SE_TILE: 
    fprintf(fp, "SE_TILE              ");
    break;
  case MPF_SE_CACHE_TILE: 
    fprintf(fp, "SE_CACHE_TILE        ");
    break;
  case MPF_INNER_FISSION: 
    fprintf(fp, "INNER_FISSION 	      "); 
    break; 
  case MPF_GATHER_SCATTER: 
    fprintf(fp, "GATHER_SCATTER       "); 
    break; 
  case MPF_VINTR_FISSION: 
    fprintf(fp, "VINTR_FISSION 	      "); 
    break; 
  case MPF_PREFETCH_VERSION: 
    fprintf(fp, "PREFETCH_VERSION     "); 
    break; 
  case MPF_OMPL_SECTIONS_LOOP: 
    fprintf(fp, "OMPL_SECTIONS_LOOP   "); 
    break;
  case MPF_OMPL_ELIM_SECTION: 
    fprintf(fp, "OMPL_ELIM_SECTION    "); 
    break;
  case MPF_OMPL_ATOMIC_CSECTION: 
    fprintf(fp, "OMPL_ATOMIC_CSECTION "); 
    break; 
  case MPF_OMPL_ATOMIC_SWAP: 
    fprintf(fp, "OMPL_ATOMIC_SWAP     ");
    break;
  case MPF_OMPL_ATOMIC_FETCHOP: 
    fprintf(fp, "OMPL_ATOMIC_FETCHOP  "); 
    break;
  case MPF_OMPL_MASTER_IF: 
    fprintf(fp, "OMPL_MASTER_IF       ");
    break; 
  case MPF_OMPL_FETCHOP_ATOMIC:
    fprintf(fp, "OMPL_FETCHOP_ATOMIC  ");
    break;
  case MPF_F90_ARRAY_STMT:
    fprintf(fp, "F90_ARRAY_STMT       ");
    break;
  case MPF_OUTER_SHACKLE:
    fprintf(fp, "OUTER_SHACKLE        ");
    break;
  case MPF_INNER_SHACKLE:
    fprintf(fp, "INNER_SHACKLE        ");
    break;
  case MPF_PREOPT_CREATE: 
    fprintf(fp, "PREOPT_CREATE        ");
    break; 
  } 
  INT field_count = 0; 
  INT i;
  for (i = 0; i < _old_loops.Elements(); i++) {
    field_count += fprintf(fp, "%d", _old_loops.Bottom_nth(i));  
    if (_old_lines.Elements() > 0) {
      field_count += fprintf(fp, " ");  
      field_count += _old_lines.Bottom_nth(i)->Print_Compact(fp); 
    } 
    if (i < _old_loops.Elements() - 1)
      field_count += fprintf(fp, ",");
  } 
  if (_old_loops.Elements() > 0)
    field_count += fprintf(fp, " "); 
  for (i = 0; i < _new_loops.Elements(); i++) {
    field_count += fprintf(fp, "%d", _new_loops.Bottom_nth(i));  
    if (_new_lines.Elements() > 0) {
      field_count += fprintf(fp, " ");  
      field_count += _new_lines.Bottom_nth(i)->Print_Compact(fp); 
    } 
    if (i < _new_loops.Elements() - 1)
      field_count += fprintf(fp, ",");
  } 
  if (_index_name != NULL) 
    fprintf(fp, " \"%s\"", _index_name);
  field_count += fprintf(fp, "\n"); 
}

//-----------------------------------------------------------------------
// NAME: PROMPF_ID::Print  
// FUNCTION: Print the PROMPF_ID to the file 'fp'.  
//-----------------------------------------------------------------------

void PROMPF_ID::Print(FILE* fp, INT entry)
{
  switch (_type) {
  case MPID_FUNC_ENTRY:
    fprintf(fp, "FUNC ENTRY        "); 
    break;
  case MPID_DO_LOOP:
    fprintf(fp, "DO LOOP           "); 
    break;
  case MPID_PAR_REGION:
    fprintf(fp, "PARALLEL REGION   "); 
    break;
  case MPID_PAR_SECTION:
    fprintf(fp, "PARALLEL SECTION  ");
    break;
  case MPID_SECTION:
    fprintf(fp, "SECTION           ");  
    break;
  case MPID_BARRIER:
    fprintf(fp, "BARRIER           ");  
    break;
  case MPID_SINGLE_PROCESS:
    fprintf(fp, "SINGLE PROCESS    ");  
    break;
  case MPID_CRITICAL_SECTION:
    fprintf(fp, "CRITICAL SECTION  ");  
    break;
  case MPID_MASTER: 
    fprintf(fp, "MASTER            "); 
    break; 
  case MPID_ORDERED: 
    fprintf(fp, "ORDERED           "); 
    break; 
  case MPID_PAR_SECTIONS: 
    fprintf(fp, "PARALLEL SECTIONS "); 
    break;  
  case MPID_ATOMIC: 
    fprintf(fp, "ATOMIC            "); 
    break;  
  default: 
    fprintf(fp, "<UNKNOWN>         ");
    break; 
  } 
  fprintf(fp, "%s", _valid ? "OK " : "   ");  
  fprintf(fp, "(%d) [%d]\n", entry, _last_trans); 
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Add_Trans 
// FUNCTION: Add the transaction 'pt' to the PROMPF_INFO's transaction 
//   list, assigning values to its Prev_Loop() fields.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Add_Trans(PROMPF_TRANS* pt)
{
  INT j;
  for (j = 0; j < pt->Old_Loop_Count(); j++) {
    INT old_loop = pt->Old_Loop(j); 
    INT i;
    for (i = Last_Trans(); i >= 0; i--) {
      PROMPF_TRANS* ptt = Trans(i); 
      INT k;
      for (k = 0; k < ptt->New_Loop_Count(); k++) 
        if (ptt->New_Loop(k) == old_loop) 
           break;
      if (k < ptt->New_Loop_Count()) {
        pt->Add_Prev_Loop(i);  
        break; 
      }
    }
    if (i == -1) 
      pt->Add_Prev_Loop(-1); 
  } 
  _trans_stack.Push(pt); 
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Remove_Trans 
// FUNCTION: Remove the last transaction from the transaction list.
//-----------------------------------------------------------------------

PROMPF_TRANS* PROMPF_INFO::Remove_Trans()
{
  return _trans_stack.Pop();
} 

//-----------------------------------------------------------------------
// NAME: Is_Grandparent_Region
// FUNCTION: Returns TRUE if 'wn_node' is a child of the WN_region_body 
//   of the OPC_REGION node 'wn_region'; returns FALSE otherwise.  
//-----------------------------------------------------------------------

static BOOL Is_Grandparent_Region(WN* wn_node, 
				  WN* wn_region)
{ 
  if (wn_region == NULL)
    return FALSE; 
  FmtAssert(WN_opcode(wn_region) == OPC_REGION, 
    ("Is_Grandparent_Region: Expected a OPC_REGION")); 
  WN* wn_first = WN_first(WN_region_body(wn_region)); 
  for (WN* wn = wn_first; wn != NULL; wn = WN_next(wn)) 
    if (wn == wn_node) 
      return TRUE; 
  return FALSE; 
} 

//-----------------------------------------------------------------------
// NAME: Prompf_Id_Type 
// FUNCTION: Returns the PROMPF_ID_TYPE corresponding to the node 'wn_ref'.
//   Returns MPID_UNKNOWN if this node is not one for which we should 
//   construct a PROMPF_ID.  Returns TRUE in 'is_first' if 'wn' should be
//   the first node with its PROMP map id, FALSE otherwise.  The 'wn_region'
//   is the closest enclosing OPC_REGION node, if there is one. 
//-----------------------------------------------------------------------

extern PROMPF_ID_TYPE Prompf_Id_Type(WN* wn_ref,
				     WN* wn_region, 
			             BOOL* is_first)
{
  if (is_first != NULL) 
    *is_first = TRUE; 
  OPCODE opc = WN_opcode(wn_ref); 
  if (opc == OPC_FUNC_ENTRY) 
    return MPID_FUNC_ENTRY; 
  if (opc == OPC_REGION) { 
    WN* wn_pragma = WN_first(WN_region_pragmas(wn_ref)); 
    if (wn_pragma != NULL && WN_opcode(wn_pragma) == OPC_PRAGMA) {
      switch (WN_pragma(wn_pragma)) { 
      case WN_PRAGMA_DOACROSS:
      case WN_PRAGMA_PARALLEL_DO:
      case WN_PRAGMA_PDO_BEGIN: 
	if (WN_pragma_arg1(wn_pragma) == 0)
 	  return MPID_DO_LOOP; 
	break; 
      case WN_PRAGMA_PARALLEL_BEGIN: 
	return MPID_PAR_REGION; 
      case WN_PRAGMA_PSECTION_BEGIN: 
        return MPID_PAR_SECTION; 
      case WN_PRAGMA_SINGLE_PROCESS_BEGIN: 
        return MPID_SINGLE_PROCESS; 
      case WN_PRAGMA_MASTER_BEGIN: 
        return MPID_MASTER; 
      case WN_PRAGMA_PARALLEL_SECTIONS: 
	return MPID_PAR_SECTIONS; 
      }
    }
  } 	
  if (opc == OPC_DO_LOOP) {
    if (Is_Grandparent_Region(wn_ref, wn_region)) { 
      WN* wn_pragma = WN_first(WN_region_pragmas(wn_region));
      if (wn_pragma != NULL && WN_opcode(wn_pragma) == OPC_PRAGMA) {
	switch (WN_pragma(wn_pragma)) {
	case WN_PRAGMA_DOACROSS:
	case WN_PRAGMA_PARALLEL_DO:
	case WN_PRAGMA_PDO_BEGIN:
	  if (WN_pragma_arg1(wn_pragma) == 0)
	    if (is_first != NULL) 
	      *is_first = FALSE; 
	}
      }
    }
    return MPID_DO_LOOP;
  }   
  if (opc == OPC_PRAGMA || opc == OPC_XPRAGMA) {
    switch (WN_pragma(wn_ref)) {
    case WN_PRAGMA_PARALLEL_BEGIN:
      if (is_first != NULL)
        *is_first = FALSE;
      return MPID_PAR_REGION;
    case WN_PRAGMA_PSECTION_BEGIN:
      if (is_first != NULL)
        *is_first = FALSE;
      return MPID_PAR_SECTION;
    case WN_PRAGMA_SINGLE_PROCESS_BEGIN:
      if (is_first != NULL)
        *is_first = FALSE;
      return MPID_SINGLE_PROCESS;
    case WN_PRAGMA_MASTER_BEGIN:
      if (is_first != NULL)
        *is_first = FALSE;
      return MPID_MASTER;
    case WN_PRAGMA_PARALLEL_SECTIONS:
      if (is_first != NULL)
        *is_first = FALSE;
      return MPID_PAR_SECTIONS; 
    case WN_PRAGMA_SECTION:
      return MPID_PAR_SECTION;
    case WN_PRAGMA_BARRIER:
      return MPID_BARRIER;
    case WN_PRAGMA_CRITICAL_SECTION_BEGIN:
      return MPID_CRITICAL_SECTION;
    case WN_PRAGMA_CRITICAL_SECTION_END:
      if (is_first != NULL)
        *is_first = FALSE;
      return MPID_CRITICAL_SECTION;
    case WN_PRAGMA_ORDERED_BEGIN:
      return MPID_ORDERED;
    case WN_PRAGMA_ORDERED_END:
      if (is_first != NULL)
        *is_first = FALSE;
      return MPID_ORDERED;
    case WN_PRAGMA_ATOMIC: 
      return MPID_ATOMIC; 
    } 
  } 
  return MPID_UNKNOWN; 
}  

//-----------------------------------------------------------------------
// NAME: Whirl_Symbol_Type
// FUNCTION: Returns a pointer to a printable string of characters
//   describing the symbol of the node 'wn'.  For loads and stores,
//   the symbol is printed, if any.  For do loops, the symbol of the
//   do loop is printed.  For nodes with no symbol information, the
//   type is printed.
//-----------------------------------------------------------------------

static const char* Whirl_Symbol_Type(WN* wn)
{
  WN* wn_symbol = NULL;
  OPCODE opc = WN_opcode(wn);
  OPERATOR opr = OPCODE_operator(opc);
  if (opc == OPC_PRAGMA || opc == OPC_XPRAGMA)
    return WN_pragmas[WN_pragma(wn)].name;
  wn_symbol = (opc == OPC_DO_LOOP) ? WN_index(wn) : (OPCODE_has_sym(opc))
    ? wn : NULL;
  if (wn_symbol == NULL)
    return (char *) (OPCODE_name(WN_opcode(wn)) + 4);

  const ST* st = WN_st(wn_symbol);
  if (st == NULL)
      return NULL;

  if (ST_class (st) != CLASS_PREG)
      return ST_name (st);
  else if (WN_offset(wn_symbol) > Last_Dedicated_Preg_Offset)
      return Preg_Name(WN_offset(wn_symbol));
  else
      return "DEDICATED PREG";
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Prompf_Info_Traverse
// FUNCTION: Traverses the 'wn_tree' which is enclosed by the region 
//   'wn_region' and records the PROMPF ids in the PROMPF_INFO.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Prompf_Info_Traverse(WN* wn_tree,
				       WN* wn_region)
{ 
  INT32 map_id = WN_MAP32_Get(Prompf_Id_Map, wn_tree);
  BOOL is_first = FALSE; 
  PROMPF_ID_TYPE pit = Prompf_Id_Type(wn_tree, wn_region, &is_first); 
  if (map_id != 0) {
    INT i;
    for (i = Last_Id() + 1; i < map_id; i++) { 
      Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, FALSE, Last_Trans(), _pool), 
	_pool)); 
    } 
    if (is_first) {
      Add_Id(CXX_NEW(PROMPF_ID(pit, TRUE, -1, _pool), _pool)); 
      FmtAssert(map_id == Last_Id(), 
	("PROMPF_INFO: Prompf map ids not assigned consecutively")); 
    } else {
      FmtAssert(map_id <= Last_Id() && Id(map_id)->Is_Valid(), 
	("PROMPF_INFO: Expected id %d to be already in table", map_id)); 
    }  
  } else if (pit != MPID_UNKNOWN) {
    DevWarn("Missing Prompf Id for 0x%p %s", wn_tree, 
      Whirl_Symbol_Type(wn_tree)); 
  }

  if (WN_opcode(wn_tree) == OPC_BLOCK) { 
    for (WN* wn = WN_first(wn_tree); wn != NULL; wn = WN_next(wn))  
      Prompf_Info_Traverse(wn, wn_region); 
  } else if (WN_opcode(wn_tree) == OPC_REGION) { 
    INT i;
    for (i = 0; i < WN_kid_count(wn_tree); i++) 
      Prompf_Info_Traverse(WN_kid(wn_tree, i), wn_tree); 
  } else { 
    INT i;
    for (i = 0; i < WN_kid_count(wn_tree); i++) 
      Prompf_Info_Traverse(WN_kid(wn_tree, i), wn_region); 
  } 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::PROMPF_INFO 
// FUNCTION: Create a PROMPF_INFO for the function rooted at 'wn_func' 
//   using memory from the 'pool'.  
//-----------------------------------------------------------------------

PROMPF_INFO::PROMPF_INFO(WN* wn_func, MEM_POOL* pool):
  _pool(pool), 
  _enabled(FALSE), 
  _first_id(WN_MAP32_Get(Prompf_Id_Map, wn_func)), 
  _trans_stack(pool), 
  _id_stack(pool),
  _trans_checkpoint(-1)
{
  _trans_stack.Clear(); 
  _id_stack.Clear(); 
  Prompf_Info_Traverse(wn_func, NULL); 
  INT i;
  for (i = Last_Id() + 1; i < Get_Next_Construct_Id(); i++) {
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, FALSE, Last_Trans(), _pool), 
      _pool)); 
  }
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Check_Old_Ids 
// FUNCTION: Return TRUE if each the 'new_ids' array is a permutation of
//   the 'old_ids' array, FALSE otherwise.  Both of these arrays have 
//   'nloops' elements. 
//-----------------------------------------------------------------------

BOOL PROMPF_INFO::Check_Old_Ids(INT old_ids[], 
				INT new_ids[],
                                INT nloops)
{
  INT i;
  for (i = 0; i < nloops; i++) {
    INT j;
    for (j = 0; j < nloops; j++) 
      if (old_ids[i] == new_ids[j])
        break; 
    if (j == nloops) 
      return FALSE; 
  } 
  return TRUE; 
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Check_New_Ids 
// FUNCTION: Returns TRUE if the 'nloops' values in the array 'new_ids'  
//   are the next 'nloops' id values to be assigned.  Return FALSE 
//   otherwise. 
//-----------------------------------------------------------------------

BOOL PROMPF_INFO::Check_New_Ids(INT new_ids[], 
                                INT nloops) 
{
  INT last_id = Last_Id();  
  INT i;
  for (i = last_id + 1; i <= last_id + nloops; i++) { 
    INT j;
    for (j = 0; j < nloops; j++) 
      if (new_ids[j] == i) 
        break; 
    if (j == nloops) 
      return FALSE; 
  } 
  return TRUE; 
}     

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Mark_F90_Lower
// FUNCTION: Indicate that we are about to record transformations from 
//   F90 lowering. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Mark_F90_Lower()
{ 
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_MARK_F90_LOWER); 
  Add_Trans(pt); 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Mark_Omp
// FUNCTION: Indicate that we are about to record transformations from 
//   OMP prelowering. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Mark_Omp()
{ 
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_MARK_OMP); 
  Add_Trans(pt); 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Mark_Preopt
// FUNCTION: Indicate that we are about to record transformations from 
//   the preoptimizer. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Mark_Preopt()
{ 
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_MARK_PREOPT); 
  Add_Trans(pt); 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Mark_Prelno 
// FUNCTION: Indicate that we are about to record transformations from 
//   LNO before and including parallelization. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Mark_Prelno()
{ 
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_MARK_PRELNO); 
  Add_Trans(pt); 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Mark_Postlno 
// FUNCTION: Indicate that we are about to record transformations from 
//   LNO after parallelization.
//-----------------------------------------------------------------------

void PROMPF_INFO::Mark_Postlno()
{ 
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_MARK_POSTLNO); 
  Add_Trans(pt); 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Elimination 
// FUNCTION: Indicate that the construct with the id 'old_loop' has been 
//   eliminated from the program.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Elimination(INT old_loop) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool); 
  pt->Set_Type(MPF_ELIMINATION); 
  pt->Add_Old_Loop(old_loop); 
  Add_Trans(pt); 
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  Id(old_loop)->Invalidate();
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Undo_Elimination
// FUNCTION: Undo the last transformation, which must be an elimination.
//-----------------------------------------------------------------------

void PROMPF_INFO::Undo_Elimination()
{
  PROMPF_TRANS* pt = Remove_Trans();
  FmtAssert(pt->Type() == MPF_ELIMINATION,
    ("Undo_Elimination: Expected last transaction to be MPF_ELIMINATION"));
  Id(pt->Old_Loop(0))->Validate();
  Reset_Last_Trans(pt->Old_Loop(0));
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Fusion 
// FUNCTION: Indicate that the two loops with ids given in 'old_loops'
//   have been fused into the single loop with id 'new_loop'.   
//-----------------------------------------------------------------------

void PROMPF_INFO::Fusion(INT old_loops[],
			 INT new_loop)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_FUSION);
  INT i;
  for (i = 0; i < 2; i++) 
    pt->Add_Old_Loop(old_loops[i]); 
  pt->Add_New_Loop(new_loop); 
  Add_Trans(pt);
  Update_Id(new_loop, Last_Trans()); 
  for (i = 0; i < 2; i++) {
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
    if (old_loops[i] != new_loop) 
      Id(old_loops[i])->Invalidate(); 
  }
} 

void PROMPF_INFO::Reset_Last_Trans(INT old_id)
{
  INT previous_trans = -1; 
  INT j;
  for (j = Last_Trans() - 1; j >= 0; j--) { 
    PROMPF_TRANS* ptt = Trans(j); 
    INT k;
    for (k = 0; k < ptt->Old_Loop_Count(); k++)  
      if (ptt->Old_Loop(k) == old_id)
	break; 
    if (k < ptt->Old_Loop_Count()) {
      previous_trans = k; 
      break; 
    } 
    for (k = 0; k < ptt->New_Loop_Count(); k++)  
      if (ptt->New_Loop(k) == old_id)
	break; 
    if (k < ptt->New_Loop_Count()) {
      previous_trans = k;
      break;
    } 
  }
  Id(old_id)->Set_Last_Trans(previous_trans);
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Undo_Fusion
// FUNCTION: Undo the last transformation, which must be a fusion. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Undo_Fusion()
{
  PROMPF_TRANS* pt = Remove_Trans();
  FmtAssert(pt->Type() == MPF_FUSION, 
    ("Undo_fusion: Expected last transaction to be MPF_FUSION"));
  INT new_loop = pt->New_Loop(0);
  INT i;
  for (i = 0; i < pt->Old_Loop_Count(); i++) 
    if (pt->Old_Loop(i) != new_loop) 
      Id(pt->Old_Loop(i))->Validate();
  for (i = 0; i < pt->Old_Loop_Count(); i++)  
    Reset_Last_Trans(pt->Old_Loop(i));
}  

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Fission 
// FUNCTION: Indicate that the 'nloops' deep nest of loops with ids in 
//   'old_loops' have been fissioned to produce 'nloops' new loops, 
//   whose ids are given in 'new_loops'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Fission(INT old_loops[], 
			  PROMPF_LINES* old_lines[], 
                          INT new_loops[], 
			  PROMPF_LINES* new_lines[], 
                          INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_FISSION);
  INT i;
  for (i = 0; i < nloops; i++) {
    pt->Add_Old_Loop(old_loops[i]);
    pt->Add_Old_Lines(old_lines[i]); 
    pt->Add_New_Loop(new_loops[i]); 
    pt->Add_New_Lines(new_lines[i]); 
  } 
  Add_Trans(pt); 
  Check_New_Ids(new_loops, nloops); 
  for (i = 0; i < nloops; i++) 
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Distribution 
// FUNCTION: Indicate that the 'nloops' deep nest of loops with ids in 
//   'old_loops' have been distributed to produce 'nloops' new loops, 
//   whose ids are given in 'new_loops'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Distribution(INT old_loops[], 
			       PROMPF_LINES* old_lines[], 
			       INT new_loops[], 
			       PROMPF_LINES* new_lines[], 
                               INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_DISTRIBUTION); 
  INT i;
  for (i = 0; i < nloops; i++) {
    pt->Add_Old_Loop(old_loops[i]);
    pt->Add_Old_Lines(old_lines[i]); 
    pt->Add_New_Loop(new_loops[i]); 
    pt->Add_New_Lines(new_lines[i]); 
  } 
  Add_Trans(pt); 
  Check_New_Ids(new_loops, nloops); 
  for (i = 0; i < nloops; i++) 
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool)); 
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Interchange 
// FUNCTION: Indicate that the 'nloops' deep nest of loops whose ids are
//   given by 'old_loops' have been interchanged to the order given by 
//   'new_loops'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Interchange(INT old_loops[], 
			      INT new_loops[], 
                              INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_INTERCHANGE); 
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  for (i = 0; i < nloops; i++) 
    Update_Id(new_loops[i], Last_Trans());  
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Pre_Peel 
// FUNCTION: Indicate that the 'nloops' new loops, whose ids are given 
//   in 'new_loops' have been created from the 'nloops' old loops, whose
//   ids are given by 'old_loops', by peeling off one or more iterations
//   of a loop and placing those iterations before the loop.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Pre_Peel(INT old_loops[], 
			   INT new_loops[], 
			   INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_PRE_PEEL);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops); 
  for (i = 0; i < nloops; i++) 
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool)); 
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Undo_Pre_Peel
// FUNCTION: Undo the last transformation, which must be a pre loop peeling. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Undo_Pre_Peel()
{
  PROMPF_TRANS* pt = Remove_Trans();
  FmtAssert(pt->Type() == MPF_PRE_PEEL,
    ("Undo_Pre_Peel: Expected last transaction to be MPF_PRE_PEEL"));
  INT i;
  for (i = 0; i < pt->New_Loop_Count(); i++)
    Remove_Id();
  for (i = 0; i < pt->Old_Loop_Count(); i++) 
    Reset_Last_Trans(pt->Old_Loop(i));
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Post_Peel 
// FUNCTION: Indicate that the 'nloops' new loops, whose ids are given 
//   in 'new_loops' have been created from the 'nloops' old loops, whose
//   ids are given by 'old_loops', by peeling off one or more iterations
//   of a loop and placing those iterations after the loop.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Post_Peel(INT old_loops[], 
			    INT new_loops[], 
			    INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_POST_PEEL);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops); 
  for (i = 0; i < nloops; i++) 
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool)); 
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Undo_Post_Peel
// FUNCTION: Undo the last transformation, which must be a post loop peeling. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Undo_Post_Peel()
{
  PROMPF_TRANS* pt = Remove_Trans();
  FmtAssert(pt->Type() == MPF_POST_PEEL,
    ("Undo_Post_Peel: Expected last transaction to be MPF_POST_PEEL"));
  INT i;
  for (i = 0; i < pt->New_Loop_Count(); i++)
    Remove_Id();
  for (i = 0; i < pt->Old_Loop_Count(); i++) 
    Reset_Last_Trans(pt->Old_Loop(i));
} 


//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Mp_Tile  
// FUNCTION: Indicate that the 'nloops' new loops with ids in 'new_loops' 
//   have been created form the loop with id 'old_loop' by mp tiling.   
//-----------------------------------------------------------------------

void PROMPF_INFO::Mp_Tile(INT old_loop,
                          INT new_loops[], 
			  INT nloops)
{
  FmtAssert(nloops == 1 || nloops == 2, 
    ("PROMPF_INFO::Mp_Tile: Only support 2D and 3D MP Tiling"));  
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_MP_TILE); 
  pt->Add_Old_Loop(old_loop); 
  INT i;
  for (i = 0; i < nloops; i++) 
    pt->Add_New_Loop(new_loops[i]); 
  Add_Trans(pt); 
  Check_New_Ids(new_loops, nloops); 
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
} 
   
//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Dsm_Tile  
// FUNCTION: Indicate that the 'nloops' new loops with ids in 'new_loops' 
//   have been created form the loop with id 'old_loop' by mp tiling.   
//-----------------------------------------------------------------------

void PROMPF_INFO::Dsm_Tile(INT old_loop,
                           INT new_loops[], 
			   INT nloops)
{
  FmtAssert(nloops == 1 || nloops == 2, 
    ("PROMPF_INFO::Mp_Tile: Only support 2D and 3D MP Tiling"));  
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_DSM_TILE); 
  pt->Add_Old_Loop(old_loop); 
  INT i;
  for (i = 0; i < nloops; i++) 
    pt->Add_New_Loop(new_loops[i]); 
  Add_Trans(pt); 
  Check_New_Ids(new_loops, nloops); 
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Donest_Outer_Tile  
// FUNCTION: Indicate that the 'nloops' old loops whose ids are in 'old_
//   loops' are converted to a single doacross and the new loop whose id
//   is 'new_loop' is created to be the new single doacross loop.
//-----------------------------------------------------------------------

void PROMPF_INFO::Donest_Outer_Tile(INT old_loops[], 
				    INT new_loop, 
				    INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_DONEST_OUTER_TILE); 
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  pt->Add_New_Loop(new_loop);
  Add_Trans(pt); 
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}
   
//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Donest_Middle_Tile 
// FUNCTION: Indicate that the loop whose id is 'old_loop' has been          
//   stripped into a 3D loop, and that a new loop has been created 
//   for the middle of the three loops, whose id is 'new_loop'.
//   loops' are converted to a single doacross and the new loop whose id
//   is 'new_loop' is created to be the new single doacross loop.
//-----------------------------------------------------------------------

void PROMPF_INFO::Donest_Middle_Tile(INT old_loop, 
				     INT new_loop) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_DONEST_MIDDLE_TILE);
  pt->Add_Old_Loop(old_loop);
  pt->Add_New_Loop(new_loop);
  Add_Trans(pt); 
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}
   
//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Dsm_Local 
// FUNCTION: Indicate that the loop with id 'new_loop' has been created 
//   to copy back a temporary array to a lego reshaped array which was 
//   named in a LASTLOCAL clause.  The line numbers of the region on  
//   which the LASTLOCAL call appears are contained in 'pl'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Dsm_Local(INT new_loop, 
			    PROMPF_LINES* pl,
			    char* index_name) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_DSM_LOCAL); 
  pt->Add_New_Loop(new_loop);
  pt->Add_New_Lines(pl); 
  pt->Add_Index_Name(index_name);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Dsm_Io 
// FUNCTION: Indicate that the loop with id 'new_loop' has been created 
//   during an optimization of IO on lego (reshaped) arrays.  The loop has
//   been created for the IO statement on line 'linenum'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Dsm_Io(INT new_loop, 
			 INT linenum,
			 char* index_name) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_DSM_IO); 
  pt->Add_New_Loop(new_loop);
  PROMPF_LINES* pl =  CXX_NEW(PROMPF_LINES(_pool), _pool); 
  pl->Add_Line(linenum); 
  pt->Add_New_Lines(pl); 
  pt->Add_Index_Name(index_name);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Single_Process
// FUNCTION: Indicate that the single process with id 'new_loop' has been 
//   created.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Single_Process(INT new_loop, 
			         PROMPF_LINES* pl)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_SINGLE_PROCESS); 
  pt->Add_New_Loop(new_loop);
  pt->Add_New_Lines(pl); 
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_SINGLE_PROCESS, TRUE, Last_Trans(), _pool),  
    _pool));
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Mp_Version 
// FUNCTION: Indicate that the 'nloops' new loops, whose ids are given 
//   in 'new_loops' have been created from the 'nloops' old loops, whose
//   ids are given by 'old_loops', by versioning a doacross loop or par- 
//   allel region into serial and parallel versions.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Mp_Version(INT old_loops[], 
			     INT new_loops[], 
			     PROMPF_ID_TYPE id_type[], 
			     INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_MP_VERSION);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops); 
  for (i = 0; i < nloops; i++) {
    INT j;
    for (j = 0; j < i; j++) 
      if (new_loops[j] == new_loops[i])
	break; 
    if (j == i)
      Add_Id(CXX_NEW(PROMPF_ID(id_type[i], TRUE, Last_Trans(), _pool), 
        _pool)); 
  } 
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Parallel_Region
// FUNCTION: Indicate that the parallel region with id 'new_loop' has been 
//   created for the loop 'old_loop' (which should be converted into a 
//   PDO loop).   
//-----------------------------------------------------------------------

void PROMPF_INFO::Parallel_Region(INT old_loop, 
				  INT new_loop)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_PARALLEL_REGION); 
  pt->Add_Old_Loop(old_loop); 
  pt->Add_New_Loop(new_loop);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_PAR_REGION, TRUE, Last_Trans(), _pool), 
    _pool));
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Hoist_Messy_Bounds
// FUNCTION: Indicate that the 'nloops' with ids 'old_loops[]' have been 
//   cloned into 'nloops' new loops with ids 'new_loops[]' while promoting
//   messy bounds.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Hoist_Messy_Bounds(INT old_loops[], 
				     INT new_loops[], 
				     INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_HOIST_MESSY_BOUNDS);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops);
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Doacross_Sync
// FUNCTION: Indicate that the loop with id 'new_loop' has been created 
//   to initialize an array of synchronization variables for the true 
//   doacross loop 'old_loop'.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Doacross_Sync(INT old_loop, 
			        INT new_loop) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_DOACROSS_SYNC);
  pt->Add_Old_Loop(old_loop);
  pt->Add_New_Loop(new_loop);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Doacross_Outer_Tile
// FUNCTION: Indicate that an outer tile parallel loop with id 'new_loop'
//   has been created from the original loop 'old_loop' to create a do-
//   across with synchronization. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Doacross_Outer_Tile(INT old_loop, 
				      INT new_loop)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_DOACROSS_OUTER_TILE);
  pt->Add_Old_Loop(old_loop);
  pt->Add_New_Loop(new_loop);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  Id(new_loop)->Set_Last_Trans(Last_Trans());
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Doacross_Sync
// FUNCTION: Indicate that an inner tile serial loop with id 'new_loop'
//   has been created from the original loop 'old_loop' to create a do-
//   across with synchronization. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Doacross_Inner_Tile(INT old_loop, 
				      INT new_loop)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_DOACROSS_INNER_TILE);
  pt->Add_Old_Loop(old_loop);
  pt->Add_New_Loop(new_loop);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  Id(new_loop)->Set_Last_Trans(Last_Trans());
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Remove_Unity_Trip
// FUNCTION: Indicate that the loop with id 'old_loop' was removed (without
//   removing the enclosed statements) because it was a unity trip loop. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Remove_Unity_Trip(INT old_loop)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool); 
  pt->Set_Type(MPF_REMOVE_UNITY_TRIP); 
  pt->Add_Old_Loop(old_loop); 
  Add_Trans(pt); 
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  Id(old_loop)->Invalidate();
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Cache_Winddown 
// FUNCTION: Indicate that the nest of 'nloops' loops with ids 'old_loops[]'
//   have been cloned to form a cache winddown of 'nloops' loops with ids
//   'new_loops'.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Cache_Winddown(INT old_loops[], 
			         INT new_loops[], 
			         INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_CACHE_WINDDOWN);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops);
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Interleaved_Winddown 
// FUNCTION: Indicate that the nest of 'nloops' loops with ids 'old_loops[]'
//   have been cloned to form a winddown loop nest of 'nloops' loops with ids
//   'new_loops'.  This was done to optimize the inner loop of an MP tiled
//   loop with interleaved scheduling. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Interleaved_Winddown(INT old_loops[], 
			               INT new_loops[], 
			               INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_INTERLEAVED_WINDDOWN);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops);
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::General_Version
// FUNCTION: Indicate that the 'nloops' with ids 'old_loops[]' have been 
//   cloned into 'nloops' new loops with ids 'new_loops[]' so that a      
//   general SNL transformation could be performed on them.  
//-----------------------------------------------------------------------

void PROMPF_INFO::General_Version(INT old_loops[], 
			          INT new_loops[], 
			          INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_GENERAL_VERSION);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops);
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Cache_Tile
// FUNCTION: Indicate that the loop with id 'old_loop' has been cache tiled 
//   to create an outer tile loop with id 'new_loop'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Cache_Tile(INT old_loop, 
			     INT new_loop)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_CACHE_TILE);
  pt->Add_Old_Loop(old_loop);
  pt->Add_New_Loop(new_loop);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Register_Winddown
// FUNCTION: Indicate that the nest of 'nloops' loops with ids 'old_loops[]'
//   have been cloned to  a register winddown form a of 'nloops' loops with 
//   ids 'new_loops[]'.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Register_Winddown(INT old_loops[], 
			            INT new_loops[], 
			            INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_REGISTER_WINDDOWN);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops);
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Register_SStrip
// FUNCTION: Indicate that a small strip version of the 'nloops' loops with 
//   ids 'old_loops[]' has been created, with ids stored in 'new_loops[]'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Register_SStrip(INT old_loops[], 
			        INT new_loops[], 
			        INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_REGISTER_SSTRIP);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops);
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Register_Tile
// FUNCTION: Indicate that the loop with id 'loop' has been register tiled.
//-----------------------------------------------------------------------

void PROMPF_INFO::Register_Tile(INT loop) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_REGISTER_TILE);
  pt->Add_Old_Loop(loop);
  Add_Trans(pt);
  Id(loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Se_Tile
// FUNCTION: Indicate that the loop with id 'old_loop' has been scalar 
//   expansion tiled to produce the tile loop with id 'new_loop'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Se_Tile(INT old_loop, 
			  INT new_loop)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_SE_TILE);
  pt->Add_Old_Loop(old_loop);
  pt->Add_New_Loop(new_loop);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
    _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Se_Cache_Tile
// FUNCTION: Indicate that the loop with id 'old_loop' has been scalar 
//   expansion and cache tiled to produce the tile loop with id 'new_loop'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Se_Cache_Tile(INT old_loop, 
			        INT new_loop)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_SE_CACHE_TILE);
  pt->Add_Old_Loop(old_loop);
  pt->Add_New_Loop(new_loop);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Register_Startup
// FUNCTION: Indicate that a startup clone of the 'nloops' loops with 
//   ids 'old_loops[]' has been created, with ids stored in 'new_loops[]'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Register_Startup(INT old_loops[], 
			           INT new_loops[], 
			           INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_REGISTER_STARTUP);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops);
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Register_Shutdown
// FUNCTION: Indicate that a shutdown clone of the 'nloops' loops with 
//   ids 'old_loops[]' has been created, with ids stored in 'new_loops[]'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Register_Shutdown(INT old_loops[], 
			            INT new_loops[], 
			            INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_REGISTER_SHUTDOWN);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops);
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Inner_Fission 
// FUNCTION: Inficate that the loop with id 'old_loop', has been fissioned
//   into 'nloops' loops with ids 'new_loops' 
//-----------------------------------------------------------------------

void PROMPF_INFO::Inner_Fission(INT old_loop,
				PROMPF_LINES* old_lines, 
                                INT new_loops[], 
				PROMPF_LINES* new_lines[], 
			        INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_INNER_FISSION); 
  pt->Add_Old_Loop(old_loop); 
  pt->Add_Old_Lines(old_lines); 
  INT i;
  for (i = 0; i < nloops; i++) {
    pt->Add_New_Loop(new_loops[i]); 
    pt->Add_New_Lines(new_lines[i]); 
  } 
  Add_Trans(pt); 
  Check_New_Ids(new_loops, nloops); 
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Gather_Scatter
// FUNCTION: Inficate that the loop with id 'old_loop', has been fissioned
//   into 'nloops' loops with ids 'new_loops' during gather-scatter.
//-----------------------------------------------------------------------

void PROMPF_INFO::Gather_Scatter(INT old_loop,
				 PROMPF_LINES* old_lines, 
                                 INT new_loops[], 
				 PROMPF_LINES* new_lines[], 
			         INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_GATHER_SCATTER); 
  pt->Add_Old_Loop(old_loop); 
  pt->Add_Old_Lines(old_lines); 
  INT i;
  for (i = 0; i < nloops; i++) {
    pt->Add_New_Loop(new_loops[i]); 
    pt->Add_New_Lines(new_lines[i]); 
  } 
  Add_Trans(pt); 
  Check_New_Ids(new_loops, nloops); 
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Vintr_Fission 
// FUNCTION: Inficate that the loop with id 'old_loop', has been fissioned
//   into 'nloops' loops with ids 'new_loops' during vector intrinsic 
//   fission.
//-----------------------------------------------------------------------

void PROMPF_INFO::Vintr_Fission(INT old_loop,
				PROMPF_LINES* old_lines, 
                                INT new_loops[], 
				PROMPF_LINES* new_lines[], 
			        INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_VINTR_FISSION); 
  pt->Add_Old_Loop(old_loop); 
  pt->Add_Old_Lines(old_lines); 
  INT i;
  for (i = 0; i < nloops; i++) {
    pt->Add_New_Loop(new_loops[i]); 
    pt->Add_New_Lines(new_lines[i]); 
  } 
  Add_Trans(pt); 
  Check_New_Ids(new_loops, nloops); 
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Prefetch_Version
// FUNCTION: Indicate that a clone of the 'nloops' loops with ids 
//   'old_loops[]' has been created for prefetching, with ids stored in 
//   'new_loops[]'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Prefetch_Version(INT old_loops[], 
			           INT new_loops[], 
			           INT nloops)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_PREFETCH_VERSION);
  INT i;
  for (i = 0; i < nloops; i++)
    pt->Add_Old_Loop(old_loops[i]);
  for (i = 0; i < nloops; i++)
    pt->Add_New_Loop(new_loops[i]);
  Add_Trans(pt);
  Check_New_Ids(new_loops, nloops);
  for (i = 0; i < nloops; i++)
    Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), 
      _pool));
  for (i = 0; i < nloops; i++)
    Id(old_loops[i])->Set_Last_Trans(Last_Trans());
  for (i = 0; i < nloops; i++)
    Id(new_loops[i])->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Outer_Shackle
// FUNCTION: Indicate that the loop with id 'new_loop' is a newly 
//   created outer shackle loop.  The line numbers of the loop nest 
//   for which the outer shackle loop is created are contained in 'pl'.
//-----------------------------------------------------------------------

void PROMPF_INFO::Outer_Shackle(INT new_loop, 
			        PROMPF_LINES* pl,
				char* index_name) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_OUTER_SHACKLE); 
  pt->Add_New_Loop(new_loop);
  pt->Add_New_Lines(pl); 
  pt->Add_Index_Name(index_name);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Inner_Shackle
// FUNCTION: Indicate that the loop whose id is 'old_loop' has been cloned
//   to create a 'new_loop' which is an part of an loop nest nested within
//   a set of outer shackle loops.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Inner_Shackle(INT old_loop, 
				INT new_loop) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_INNER_SHACKLE);
  pt->Add_Old_Loop(old_loop);
  pt->Add_New_Loop(new_loop);
  Add_Trans(pt); 
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(old_loop)->Set_Last_Trans(Last_Trans());
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::OMPL_Sections_To_Loop
// FUNCTION: Indicate that the SECTIONS construct with the given 'old_id' 
//   has been converted into a loop
//-----------------------------------------------------------------------

void PROMPF_INFO::OMPL_Sections_To_Loop(INT old_id)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_OMPL_SECTIONS_LOOP);
  pt->Add_Old_Loop(old_id);
  Add_Trans(pt);
  Id(old_id)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::OMPL_Eliminate_Section
// FUNCTION: Indicate that the SECTION construct with the given 'old_id'  
//   has been eliminated from the program, because its corresponding 
//   SECTIONS construct has been converted into a loop.
//-----------------------------------------------------------------------

void PROMPF_INFO::OMPL_Eliminate_Section(INT old_id)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_OMPL_ELIM_SECTION);
  pt->Add_Old_Loop(old_id);
  Add_Trans(pt);
  Id(old_id)->Set_Last_Trans(Last_Trans());
  Id(old_id)->Invalidate();
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::OMPL_Atomic_To_Critical_Section
// FUNCTION: Indicate that the ATOMIC construct with the given 'old_id' 
//   has been converted into a CRIRICAL SECTION.
//-----------------------------------------------------------------------

void PROMPF_INFO::OMPL_Atomic_To_Critical_Section(INT old_id)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_OMPL_ATOMIC_CSECTION);
  pt->Add_Old_Loop(old_id);
  Add_Trans(pt);
  Id(old_id)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::OMPL_Atomic_To_Swap
// FUNCTION: Indicate that the ATOMIC construct with the given 'old_id'
//   has been converted into a compare and swap sequence. 
//-----------------------------------------------------------------------

void PROMPF_INFO::OMPL_Atomic_To_Swap(INT old_id)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_OMPL_ATOMIC_SWAP);
  pt->Add_Old_Loop(old_id);
  Add_Trans(pt);
  Id(old_id)->Set_Last_Trans(Last_Trans());
  Id(old_id)->Invalidate();
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::OMPL_Atomic_To_FetchAndOp
// FUNCTION: Indicate that the ATOMIC construct with the given 'old_id' 
//   has been converted into a fetch and op sequence. 
//-----------------------------------------------------------------------

void PROMPF_INFO::OMPL_Atomic_To_FetchAndOp(INT old_id)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_OMPL_ATOMIC_FETCHOP);
  pt->Add_Old_Loop(old_id);
  Add_Trans(pt);
  Id(old_id)->Set_Last_Trans(Last_Trans());
  Id(old_id)->Invalidate();
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::OMPL_Master_To_If
// FUNCTION: Indicate that the MASTER construct with the given 'old_id'  
//   has been converted into an IF test. 
//-----------------------------------------------------------------------

void PROMPF_INFO::OMPL_Master_To_If(INT old_id)
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_OMPL_MASTER_IF);
  pt->Add_Old_Loop(old_id);
  Add_Trans(pt);
  Id(old_id)->Set_Last_Trans(Last_Trans());
  Id(old_id)->Invalidate();
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::OMPL_Fetchop_Atomic
// FUNCTION: Indicate that an unsupported fetch and op intrinsic has been 
//   lowered to an ATOMIC directive. 
//-----------------------------------------------------------------------

void PROMPF_INFO::OMPL_Fetchop_Atomic(INT new_id,
			              PROMPF_LINES* pl) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_OMPL_FETCHOP_ATOMIC); 
  pt->Add_New_Loop(new_id);
  pt->Add_New_Lines(pl); 
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_ATOMIC, TRUE, Last_Trans(), _pool),  
    _pool));
  Id(new_id)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::F90_Array_Stmt 
// FUNCTION: Indicate that the loop with id 'new_loop' has been created 
//   from the F90 array statement which appears on the lines contained 
//   in 'pl'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::F90_Array_Stmt(INT new_loop, 
			         PROMPF_LINES* pl,
				 char* index_name) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_F90_ARRAY_STMT); 
  pt->Add_New_Loop(new_loop);
  pt->Add_New_Lines(pl); 
  pt->Add_Index_Name(index_name);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Preopt_Create
// FUNCTION: Indicate that the loop with id 'new_loop' has been created 
//   by raising a WHILE or unstructed control flow loop into a DO_LOOP.  
//   The line numbers of the region on which the LASTLOCAL call appears 
//   are contained in 'pl'. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Preopt_Create(INT new_loop, 
			        PROMPF_LINES* pl,
				char* index_name) 
{
  PROMPF_TRANS* pt = CXX_NEW(PROMPF_TRANS(_pool), _pool);
  pt->Set_Type(MPF_PREOPT_CREATE); 
  pt->Add_New_Loop(new_loop);
  pt->Add_New_Lines(pl); 
  pt->Add_Index_Name(index_name);
  Add_Trans(pt);
  Add_Id(CXX_NEW(PROMPF_ID(MPID_DO_LOOP, TRUE, Last_Trans(), _pool), _pool));
  Id(new_loop)->Set_Last_Trans(Last_Trans());
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Print 
// FUNCTION: Print the PROMPF_INFO to the file 'fp'.  
//-----------------------------------------------------------------------

void PROMPF_INFO::Print(FILE* fp) 
{
  fprintf(fp, "TRANSFORMATION LIST:\n"); 
  INT i;
  for (i = 0; i < _trans_stack.Elements(); i++) {
    fprintf(fp, "  %2d ", i); 
    _trans_stack.Bottom_nth(i)->Print(fp); 
  } 
  fprintf(fp, "ID LIST:\n"); 
  for (i = First_Id(); i <= Last_Id(); i++) {
    fprintf(fp, "  "); 
    Id(i)->Print(fp, i); 
  }
  if (_trans_checkpoint != -1)
    fprintf(fp, "CHECKPOINT: %d\n", _trans_checkpoint);
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Check_Traverse
// FUNCTION: Traverse the 'wn_tree' (whose closest enclosing region is 
//   'wn_region', and check the consistency of the PROMPF information in 
//   that tree. Here 'ids[]' is a BOOL array in which 'ids[i - First_Id()]' 
//   is TRUE if i-th id has been seen.  
//-----------------------------------------------------------------------

static INT check_error_count = 0; 

#define PM_ASSERT(a, b) \
  ((a) ? 0 : (fprintf b, fprintf(fp, "\n"), check_error_count++))

void PROMPF_INFO::Check_Traverse(FILE* fp,
			         WN* wn_tree, 
				 BOOL ids[], 
				 WN* wn_region)
{ 
  INT map_id = WN_MAP32_Get(Prompf_Id_Map, wn_tree); 
  if (map_id != 0) {
    PM_ASSERT(map_id >= First_Id() && map_id <= Last_Id(),
      (fp, "PROMPF_INFO: Id in program out of range %d", map_id)); 
    if (map_id >= First_Id() && map_id <= Last_Id()) 
      ids[map_id - First_Id()] = TRUE; 
  } else { 
    BOOL is_first = FALSE; 
    PM_ASSERT(Prompf_Id_Type(wn_tree, wn_region, &is_first) == MPID_UNKNOWN, 
      (fp, "PROMPF_INFO: Missing Prompf Id for 0x%p %s", 
      wn_tree, Whirl_Symbol_Type(wn_tree)));
  }

  if (WN_opcode(wn_tree) == OPC_BLOCK) { 
    for (WN* wn = WN_first(wn_tree); wn != NULL; wn = WN_next(wn))  
      Check_Traverse(fp, wn, ids, wn_region); 
  } else if (WN_opcode(wn_tree) == OPC_REGION) { 
    INT i;
    for (i = 0; i < WN_kid_count(wn_tree); i++) 
      Check_Traverse(fp, WN_kid(wn_tree, i), ids, wn_tree); 
  } else { 
    INT i;
    for (i = 0; i < WN_kid_count(wn_tree); i++) 
      Check_Traverse(fp, WN_kid(wn_tree, i), ids, wn_region); 
  } 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Check 
// FUNCTION: Return the number of errors encountered while checking the 
//   PROMPF_INFO for internal consistency and consistency with the func-
//   tion 'wn_func', and print a message to the file 'fp' for each error
//   encountered.   
//-----------------------------------------------------------------------

INT PROMPF_INFO::Check(FILE* fp,
		       WN* wn_func) 
{
  INT check_error_count = 0; 
  INT nids = Last_Id() - First_Id() + 1; 
  INT* ids = CXX_NEW_ARRAY(BOOL, nids, _pool);  
  INT i;
  for (i = 0; i < nids; i++) 
    ids[i] = FALSE; 
  Check_Traverse(fp, wn_func, ids, NULL); 
  for (i = First_Id(); i < Last_Id(); i++) {
    PM_ASSERT(!Id(i)->Is_Valid() || ids[i - First_Id()],  
      (fp, "PROMPF_INFO: Valid id %d not in program", i)); 
    PM_ASSERT(Id(i)->Is_Valid() || !ids[i - First_Id()],  
      (fp, "PROMPF_INFO: Invalid id %d in program", i)); 
  } 
  for (i = First_Id(); i < Last_Id(); i++) {
    INT j = Id(i)->Last_Trans();  
    if (j != -1) { 
      PM_ASSERT(j >= 0 && j <= Last_Trans(), 
        (fp, "PROMPF_INFO: Trans Index %d out of range", j));  
      PROMPF_TRANS* pt = Trans(j); 
      if (Id(i)->Is_Valid()) { 
	INT k;
	for (k = 0; k < pt->Old_Loop_Count(); k++) 
	  if (pt->Old_Loop(k) == i)
	    break; 
        if (k == pt->Old_Loop_Count()) {
	  for (k = 0; k < pt->New_Loop_Count(); k++) 
	    if (pt->New_Loop(k) == i)
	      break; 
	  PM_ASSERT(k != pt->New_Loop_Count(), 
	    (fp, "PROMPF_INFO: Trans Index %d does not match LHS or RHS", i)); 
        } 
      } else { 
	INT k;
	for (k = 0; k < pt->Old_Loop_Count(); k++) 
	  if (pt->Old_Loop(k) == i)
	    break; 
	PM_ASSERT(k != pt->Old_Loop_Count(), 
	  (fp, "PROMPF_INFO: Trans Index %d does not match LHS", i)); 
      }  
    }
  }
  for (i = 0; i < Last_Trans(); i++) { 
    PROMPF_TRANS* pt = Trans(i); 
    PM_ASSERT(pt->Old_Loop_Count() == pt->Prev_Loop_Count(), 
      (fp, "PROMPF_INFO: Old_Loop_Count != Prev_Loop_Count for trans %d", i));
    INT j;
    for (j = 0; j < pt->Prev_Loop_Count(); j++) {
      PM_ASSERT(pt->Prev_Loop(j) >= -1 && pt->Prev_Loop(j) <= Last_Trans(), 
        (fp, "PROMPF_INFO: Prev_Loop(%d) for trans %d out of range", j, i));
      if (pt->Prev_Loop(j) >= 0) {
        PROMPF_TRANS* ptt = Trans(pt->Prev_Loop(j));
	INT k;
        for (k = 0; k < ptt->New_Loop_Count(); k++) 
          if (ptt->New_Loop(k) == pt->Old_Loop(j))
	    break;
        PM_ASSERT(k != ptt->New_Loop_Count(), 
          (fp, "PROMPF_INFO: Prev_Loop(%d) for trans %d has no new loop match",
             j, i));
      }
    }
  }
  return check_error_count; 
}

//-----------------------------------------------------------------------
// NAME: Is_Mark_Type
// FUNCTION: Returns TRUE if 'ptt' one of the transformation log mark        
//   types, returns FALSE otherwise. 
//-----------------------------------------------------------------------

static BOOL Is_Mark_Type(PROMPF_TRANS_TYPE ptt)      
{
  switch (ptt) { 
  case MPF_MARK_OMP: 
  case MPF_MARK_PREOPT: 
  case MPF_MARK_PRELNO: 
  case MPF_MARK_POSTLNO: 
    return TRUE; 
  default: 
    return FALSE; 
  } 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Print_Compact 
// FUNCTION: Print the transaction log for PROMP_INFO in compact form 
//   to the file 'fp'. (This is the form used by the PROMPF anl file.)  
//-----------------------------------------------------------------------

void PROMPF_INFO::Print_Compact(FILE *fp, 
				PROMPF_TRANS_LOG ptl)
{
  if (Last_Trans() == -1)
    return; 
  INT i = 0; 
  switch (ptl) {
  case PTL_F90_LOWER:
    for (i = 0; i <= Last_Trans(); i++)
      if (Trans(i)->Type() == MPF_MARK_F90_LOWER)
	break; 
    if (i >= Last_Trans())
      break; 
    fprintf(fp, "F90_LOWER_TRANSFORMATION_LOG_BEGIN\n"); 
    for (i++ ; i <= Last_Trans(); i++) {
      if (Is_Mark_Type(Trans(i)->Type()))
	break; 
      Trans(i)->Print_Compact(fp); 
    } 
    fprintf(fp, "F90_LOWER_TRANSFORMATION_LOG_END\n\n"); 
    break; 
  case PTL_OMP:
    for (i = 0; i <= Last_Trans(); i++)
      if (Trans(i)->Type() == MPF_MARK_OMP)
	break; 
    if (i >= Last_Trans())
      break; 
    fprintf(fp, "OMP_TRANSFORMATION_LOG_BEGIN\n"); 
    for (i++ ; i <= Last_Trans(); i++) {
      if (Is_Mark_Type(Trans(i)->Type()))
	break; 
      Trans(i)->Print_Compact(fp); 
    } 
    fprintf(fp, "OMP_TRANSFORMATION_LOG_END\n\n"); 
    break; 
  case PTL_PREOPT:
    for (i = 0; i <= Last_Trans(); i++)
      if (Trans(i)->Type() == MPF_MARK_PREOPT)
	break; 
    if (i >= Last_Trans() || Is_Mark_Type(Trans(i+1)->Type()))
      break; 
    fprintf(fp, "PREOPT_TRANSFORMATION_LOG_BEGIN\n");
    for (i++; i <= Last_Trans(); i++) {
      if (Is_Mark_Type(Trans(i)->Type()))
	break; 
      Trans(i)->Print_Compact(fp); 
    } 
    fprintf(fp, "PREOPT_TRANSFORMATION_LOG_END\n\n");
    break;  
  case PTL_PRELNO: 
    for (i = 0; i <= Last_Trans(); i++)
      if (Trans(i)->Type() == MPF_MARK_PRELNO)
	break; 
    if (i >= Last_Trans() || Is_Mark_Type(Trans(i+1)->Type()))
      break; 
    fprintf(fp, "TRANSFORMATION_LOG_BEGIN\n"); 
    for (i++; i <= Last_Trans(); i++) {
      if (Is_Mark_Type(Trans(i)->Type()))
	break; 
      Trans(i)->Print_Compact(fp); 
    } 
    fprintf(fp, "TRANSFORMATION_LOG_END\n\n"); 
    break;  
  case PTL_POSTLNO: 
    for (i = 0; i <= Last_Trans(); i++)
      if (Trans(i)->Type() == MPF_MARK_POSTLNO)
	break; 
    if (i >= Last_Trans() || Is_Mark_Type(Trans(i+1)->Type()))
      break; 
    fprintf(fp, "POST_TRANSFORMATION_LOG_BEGIN\n"); 
    for (i++; i <= Last_Trans(); i++) {
      if (Is_Mark_Type(Trans(i)->Type()))
	break; 
      Trans(i)->Print_Compact(fp); 
    } 
    fprintf(fp, "POST_TRANSFORMATION_LOG_END\n\n"); 
    break;  
  } 
}

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Save
// FUNCTION: Set a checkpoint to which the PROMPF log may be restored. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Save()
{ 
  FmtAssert(_trans_checkpoint == -1, 
    ("PROMPF_INFO::Save: Transformation Checkpoint Already Saved"));
  _trans_checkpoint = Last_Trans();
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Restore
// FUNCTION: Restore the PROMPF log to the stored checkpoint. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Restore()
{ 
  FmtAssert(_trans_checkpoint != -1,
    ("PROMPF_INFO::Restore: Transformation Checkpoint Not Saved"));
  while (Last_Trans() > _trans_checkpoint) { 
    PROMPF_TRANS* pt = Trans(Last_Trans());
    switch(pt->Type()) { 
    case MPF_PRE_PEEL: 
      Undo_Pre_Peel();
      break;
    case MPF_POST_PEEL: 
      Undo_Post_Peel();
      break;
    case MPF_FUSION: 
      Undo_Fusion();
      break;
    case MPF_ELIMINATION: 
      Undo_Elimination();
      break; 
    default: 
      FmtAssert(FALSE, ("Restore: Cannot undo PROMPF transformation"));
    } 
  } 
  _trans_checkpoint = -1; 
} 

//-----------------------------------------------------------------------
// NAME: PROMPF_INFO::Clear
// FUNCTION: Reset the PROMPF stored checkpoint. 
//-----------------------------------------------------------------------

void PROMPF_INFO::Clear()
{
  _trans_checkpoint = -1;
} 

//-----------------------------------------------------------------------
// NAME: Prompf_Assign_Ids_Traverse 
// FUNCTION: Recursive traversal for the function Prompf_Assign_Ids().
//   (See below for full description, function arguments are the same.)   
//-----------------------------------------------------------------------

static void Prompf_Assign_Ids_Traverse(WN* wn_old, 
			               WN* wn_new, 
			               STACK<WN*>* old_stack, 
			               STACK<WN*>* new_stack, 
				       BOOL copy_ids,
				       INT max_ids)
{
  if (old_stack->Elements() == max_ids)
    return; 
  FmtAssert(old_stack->Elements() < max_ids, 
    ("Prompf_Assign_Ids: Too many ids assigned")); 
  FmtAssert(WN_opcode(wn_old) == WN_opcode(wn_new), 
    ("Prompf_Assign_Ids: Cloned node type does not match original"));
  if (!OPCODE_is_scf(WN_opcode(wn_old)))
    return; 

  INT old_id = WN_MAP32_Get(Prompf_Id_Map, wn_old); 
  INT new_id = WN_MAP32_Get(Prompf_Id_Map, wn_new); 
  if (old_id != 0 && new_id == 0) {
    if (copy_ids) { 
      WN_MAP32_Set(Prompf_Id_Map, wn_new, old_id);
    } else { 
      INT new_id = New_Construct_Id(); 
      WN_MAP32_Set(Prompf_Id_Map, wn_new, new_id); 
    } 
    old_stack->Push(wn_old);
    new_stack->Push(wn_new); 
  } 

  if (WN_opcode(wn_old) == OPC_BLOCK) {
    WN* wn2 = WN_first(wn_new); 
    for (WN* wn1 = WN_first(wn_old); wn1 != NULL; wn1 = WN_next(wn1)) { 
      Prompf_Assign_Ids_Traverse(wn1, wn2, old_stack, new_stack, 
	copy_ids, max_ids); 
      wn2 = WN_next(wn2);  
    }
  } else { 
    INT i;
    for (i = 0; i < WN_kid_count(wn_old); i++) { 
      WN* wn1 = WN_kid(wn_old, i);  
      WN* wn2 = WN_kid(wn_new, i);  
      Prompf_Assign_Ids_Traverse(wn1, wn2, old_stack, new_stack, 
	copy_ids, max_ids);
    }
  }

  FmtAssert(old_stack->Elements() == new_stack->Elements(), 
    ("Prompf_Assign_Ids: Old and new stacks must have same element count")); 
}

//-----------------------------------------------------------------------
// NAME: Prompf_Assign_Ids 
// FUNCTION: Traverse the old code 'wn_old' and the new code 'wn_new' 
//   simulanteously, and assign new PROMPF ids to nodes in the new code 
//   which don't have them, but which correspond to nodes in the old code.
//   When a new id is assigned in the new code, push the old WN* on the 
//   'old_stack' and the new WN* on the 'new_stack'.  Assign a maximum of
//   'max_ids' new ids in the new code. If 'copy_ids', then duplicate the
//   ids in the new code rather than assigning new ids. 
//-----------------------------------------------------------------------

extern void Prompf_Assign_Ids(WN* wn_old,
                              WN* wn_new,
                              STACK<WN*>* old_stack, 
                              STACK<WN*>* new_stack,
			      BOOL copy_ids,
			      INT max_ids)
{
  Prompf_Assign_Ids_Traverse(wn_old, wn_new, old_stack, new_stack, 
    copy_ids, max_ids); 
}
