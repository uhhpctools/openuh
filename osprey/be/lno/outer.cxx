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


#ifdef USE_PCH
#include "lno_pch.h"
#endif // USE_PCH
#pragma hdrstop

#ifdef _KEEP_RCS_ID
/*REFERENCED*/
static char *rcs_id = "$Source: /home/bos/bk/kpro64-pending/be/lno/SCCS/s.outer.cxx $ $Revision: 1.6 $";
#endif /* _KEEP_RCS_ID */

#include <sys/types.h>
#include <limits.h> 
#include "pu_info.h"
#include "fusion.h"
#include "fiz_fuse.h"
#include "name.h"
#include "lwn_util.h"
#include "lno_bv.h"
#include "ff_utils.h"
#include "wn_map.h"
#include "btree.h"
#include "glob.h"
#include "tlog.h"
#include "parallel.h"

typedef HASH_TABLE<WN*,UINT> WN2UINT;

typedef enum { NORMAL, SKIP } OUTER_FUSION_STATUS;

typedef enum { INFO, FAIL, SUCCEED } INFO_TYPE;

static void outer_fusion_verbose_info(
  SRCPOS        srcpos1,
  SRCPOS        srcpos2,
  char*         message)
{
  
  printf("#### Outer Fusion(%d+%d): %s\n",
    Srcpos_To_Line(srcpos1),
    Srcpos_To_Line(srcpos2),
    message);
}

static void outer_fusion_analysis_info(
  INFO_TYPE     info_type,
  SRCPOS        srcpos1,
  SRCPOS        srcpos2,
  UINT32	snl_level1,
  UINT32	snl_level2,
  char*         message)
{
  
  switch (info_type) {
    case INFO:
      fprintf(LNO_Analysis,"( LNO_Outer_Fusion_Info ");
      break;
    case FAIL:
      fprintf(LNO_Analysis,"( LNO_Outer_Fusion_Failure ");
      break;
    case SUCCEED:
      fprintf(LNO_Analysis,"( LNO_Outer_Fusion_Success ");
      break;
  }

  fprintf(LNO_Analysis,"(%s %d %d) (%s %d %d) \"%s\" )\n",
    Cur_PU_Name, Srcpos_To_Line(srcpos1), snl_level1,
    Cur_PU_Name, Srcpos_To_Line(srcpos2), snl_level2,
    message);
}

static void outer_fusion_tlog_info(
  INFO_TYPE     info_type,
  SRCPOS        srcpos1,
  SRCPOS        srcpos2,
  UINT32	snl_level1,
  UINT32	snl_level2,
  char*         message)
{
  
  char tmp_string[300];
  sprintf(tmp_string,"%d %d %d %d %d",
            info_type, Srcpos_To_Line(srcpos1), Srcpos_To_Line(srcpos2),
            snl_level1, snl_level2);
  Generate_Tlog("LNO","outer_loop_fusion", Srcpos_To_Line(srcpos1), "",
          tmp_string, "", message);
}

#ifndef KEY // moved to common/com/config_lno.* and controllable by a flag
#define OLF_size_upperbound 100		// max size allowed for fusion
#define OLF_size_lowerbound 15		// remove several restriction
					// if size is smaller than this
#endif

static BINARY_TREE<NAME2BIT> *mapping_dictionary;
static UINT Bit_Position_Count=0;
static MEM_POOL OLF_default_pool;

static UINT
Stride_One_Level(REF_LIST_STACK *writes, REF_LIST_STACK *reads, 
	UINT outer_level, UINT level) {

  INT stride_one_level;

  UINT* hist=CXX_NEW_ARRAY(UINT,level+1,&OLF_default_pool);
  INT i;
  for (i=0; i<=level; i++) {
    hist[i] = 0;
  }

  REF_LIST_STACK* ref_list_stack[2];
  ref_list_stack[0]=writes;
  ref_list_stack[1]=reads;
  
  for (INT list_count=0; list_count<2; list_count++)
  for (INT ii=0;ii<ref_list_stack[list_count]->Elements(); ii++) {
    REFERENCE_ITER l_iter(ref_list_stack[list_count]->Bottom_nth(ii));
    for (REFERENCE_NODE* node=l_iter.First(); !l_iter.Is_Empty();
         node=l_iter.Next()) {
      WN* array_node=node->Wn;
      if (OPCODE_is_load(WN_opcode(array_node))) {
        array_node = WN_kid0(array_node);
      } else {
        array_node = WN_kid1(array_node);
      }
      if (WN_operator(array_node) == OPR_ADD) {
        if (WN_operator(WN_kid0(array_node)) == OPR_ARRAY) {
          array_node = WN_kid0(array_node);
        } else {
          array_node = WN_kid1(array_node);
        }
      }

      ACCESS_ARRAY* aa;
      aa=(ACCESS_ARRAY*)WN_MAP_Get(LNO_Info_Map,array_node);

      if (aa->Too_Messy)
        continue;
  
      ACCESS_VECTOR* av=aa->Dim(aa->Num_Vec()-1);
      if (av->Too_Messy || av->Non_Lin_Symb)
        continue;
  
      stride_one_level= -1;
      for (INT i=outer_level; i<=level; i++)
        if (av->Loop_Coeff(i)==0)
          continue;
        else if (av->Loop_Coeff(i)==1 || av->Loop_Coeff(i)==-1)
          if (stride_one_level== -1)
            stride_one_level=i;
          else {
            stride_one_level= -2;
            break;
          }
        else {
          stride_one_level= -2;
          break;
        }
  
      if (stride_one_level>=0)
        hist[stride_one_level]++;
    }
  }

  UINT max_freq=0;
  stride_one_level=-1;
  for (i=outer_level; i<=level; i++)
   if (max_freq<hist[i]) {
     max_freq=hist[i];
     stride_one_level=i;
   }

  return stride_one_level;
}

static BIT_VECTOR*
Array_Names_In_Loop(REF_LIST_STACK *writes, REF_LIST_STACK *reads) {

  BIT_VECTOR* bv=CXX_NEW(BIT_VECTOR(256,&OLF_default_pool), &LNO_local_pool);

  REF_LIST_STACK* ref_list_stack[2];
  ref_list_stack[0]=writes;
  ref_list_stack[1]=reads;
  
  for (INT list_count=0; list_count<2; list_count++)
  for (INT ii=0;ii<ref_list_stack[list_count]->Elements(); ii++) {
    REFERENCE_ITER l_iter(ref_list_stack[list_count]->Bottom_nth(ii));
    for (REFERENCE_NODE* node=l_iter.First(); !l_iter.Is_Empty();
         node=l_iter.Next()) {

       WN* array_node = node->Wn;
       if (OPCODE_is_load(WN_opcode(array_node))) {
         array_node = WN_kid0(array_node);
       } else {
         array_node = WN_kid1(array_node);
       }
       if (WN_operator(array_node) == OPR_ADD) {
         if (WN_operator(WN_kid0(array_node)) == OPR_ARRAY) {
           array_node = WN_kid0(array_node);
         } else if (
           WN_operator(WN_kid1(array_node)) == OPR_ARRAY) {
           array_node = WN_kid1(array_node);
         } else
           continue;
       }

       if (!OPCODE_has_sym(WN_opcode(WN_array_base(array_node))))
         continue;

       NAME2BIT temp_map(WN_array_base(array_node));

       UINT bit_position;
       if ((mapping_dictionary->Find(temp_map))==NULL) {
         if (Bit_Position_Count==256) {
           CXX_DELETE(bv,&LNO_local_pool);
           return NULL;
         }
         bit_position=Bit_Position_Count++;
         temp_map.Set_Bit_Position(bit_position);
         mapping_dictionary->Enter(temp_map);
       } else
         bit_position=
           mapping_dictionary->Find(temp_map)->Get_Data()->Get_Bit_Position();

       bv->Set(bit_position);
    }
  }
  return bv;
}


// --------------------------------------------------
// Test if any of the loops in an SNL is parallizable
// --------------------------------------------------
static BOOL
Any_Loop_In_SNL_Parallelizable(WN* loop, INT depth)
{
  if (Get_Do_Loop_Info(loop)->Parallelizable) {
    return TRUE;
  }
  for (INT i = 1; i < depth; ++i) {
    loop = Get_Only_Loop_Inside(loop, FALSE);
    if (Get_Do_Loop_Info(loop)->Parallelizable) {
      return TRUE;
    }
  }
  return FALSE;
}
  

static FISSION_FUSION_STATUS Fuse_Outer_Loops(WN* loop1, WN* loop2,
                      FIZ_FUSE_INFO* ffi, WN2UINT *wn2ffi,
                      UINT* fusion_level_io) {

  Is_True(Do_Loop_Is_Good(loop1) && !Do_Loop_Has_Calls(loop1) &&
				    !Do_Loop_Has_Gotos(loop1), 
          ("Bad loop passed to Fuse_Outer_Loops()."));
  Is_True(Do_Loop_Is_Good(loop2) && !Do_Loop_Has_Calls(loop2) &&
				    !Do_Loop_Has_Gotos(loop2), 
          ("Bad loop passed to Fuse_Outer_Loops()."));

  char loop1_var_name[80];
  char loop2_var_name[80];
  if (strlen(ST_name(WN_st(WN_index(loop1))))>=80) {
    strcpy(loop1_var_name,"name_too_long");
  } else
    strcpy(loop1_var_name,ST_name(WN_st(WN_index(loop1))));
  if (strlen(ST_name(WN_st(WN_index(loop2))))>=80) {
    strcpy(loop2_var_name,"name_too_long");
  } else
    strcpy(loop2_var_name,ST_name(WN_st(WN_index(loop2))));

  SRCPOS srcpos1=WN_Get_Linenum(loop1);
  SRCPOS srcpos2=WN_Get_Linenum(loop2);

  DO_LOOP_INFO *dli1=Get_Do_Loop_Info(loop1);
  DO_LOOP_INFO *dli2=Get_Do_Loop_Info(loop2);

  if (dli1->No_Fusion || dli2->No_Fusion
      || !Cannot_Concurrentize(loop1) && Cannot_Concurrentize(loop2)
      || Cannot_Concurrentize(loop1) && !Cannot_Concurrentize(loop2)) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Loops with no_fusion pragmas cannot be outer fused.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,0,0,
        "Loops with no_fusion pragmas cannot be outer fused.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,0,0,
        "Loops with no_fusion pragmas cannot be outer fused.");
    return Failed;
  }
  if (dli1->LB->Too_Messy || dli1->UB->Too_Messy ||
      dli2->LB->Too_Messy || dli2->UB->Too_Messy) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Loops with messy bounds cannot be outer fused.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,0,0,
        "Loops with messy bounds cannot be outer fused.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,0,0,
        "Loops with messy bounds cannot be outer fused.");
    return Failed;
  }
  if (!Do_Loop_Is_Good(loop1) || !Do_Loop_Is_Good(loop2) ||
      Do_Loop_Has_Calls(loop1) || Do_Loop_Has_Calls(loop2) ||
      Do_Loop_Has_Gotos(loop1) || Do_Loop_Has_Gotos(loop2)) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Loops with calls, exits, or gotos cannot be outer fused.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,0,0,
        "Loops with calls, exits, or gotos cannot be outer fused.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,0,0,
        "Loops with calls, exits, or gotos cannot be outer fused.");
    return Failed;
  }

  UINT ffi_index=wn2ffi->Find(loop1);
  Is_True(ffi_index,("Missing SNL info for loop1 in Fuse_Outer_Loops()."));
  if (ffi_index == 0) {
    SNL_INFO snl_info(loop1);
    ffi_index=ffi->New_Snl(snl_info);
    wn2ffi->Enter(loop1,ffi_index);
  }
  UINT snl_level1=ffi->Get_Depth(ffi_index);
  SNL_TYPE type1=ffi->Get_Type(ffi_index);

  ffi_index=wn2ffi->Find(loop2);
  Is_True(ffi_index,("Missing SNL info for loop2 in Fuse_Outer_Loops()."));
  if (ffi_index == 0) {
    SNL_INFO snl_info(loop2);
    ffi_index=ffi->New_Snl(snl_info);
    wn2ffi->Enter(loop2,ffi_index);
  }
  UINT snl_level2=ffi->Get_Depth(ffi_index);
  SNL_TYPE type2=ffi->Get_Type(ffi_index);

  if (LNO_Verbose)
    outer_fusion_verbose_info(srcpos1,srcpos2,
      "Attempt to fuse outer loops to improve locality.");
  if (LNO_Analysis)
    outer_fusion_analysis_info(INFO,srcpos1,srcpos2,snl_level1,snl_level2,
      "Attempt to fuse outer loops to improve locality.");
  if (LNO_Tlog)
    outer_fusion_tlog_info(INFO,srcpos1,srcpos2,snl_level1,snl_level2,
      "Attempt to fuse outer loops to improve locality.");

  if (snl_level1!=snl_level2 && LNO_Fusion<2) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Unequal SNL levels.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Unequal SNL levels.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Unequal SNL levels.");
    *fusion_level_io=0;
    return Failed;
  }

  if (type1!=type2 && LNO_Fusion<2) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Fusing SNL with non-SNL.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Fusing SNL with non-SNL.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Fusing SNL with non-SNL.");
    *fusion_level_io=0;
    return Failed;
  }

  BOOL parallel1 = (Run_autopar && LNO_Run_AP > 0 &&
                    Any_Loop_In_SNL_Parallelizable(loop1, snl_level1));
  BOOL parallel2 = (Run_autopar && LNO_Run_AP > 0 &&
                    Any_Loop_In_SNL_Parallelizable(loop2, snl_level2));
  if (parallel1 != parallel2 && LNO_Fusion) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Do not fuse parallel with serial loop.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,0,0,
        "Do not fuse parallel with serial loop.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,0,0,
        "Do not fuse parallel with serial loop.");
    return Failed;
  }

  mapping_dictionary =
    CXX_NEW(BINARY_TREE<NAME2BIT>(&OLF_default_pool),&OLF_default_pool);
  Bit_Position_Count=0;

  REF_LIST_STACK *writes1 = CXX_NEW(REF_LIST_STACK(&OLF_default_pool),
                                    &OLF_default_pool);
  REF_LIST_STACK *reads1 = CXX_NEW(REF_LIST_STACK(&OLF_default_pool),
                                   &OLF_default_pool);
  SCALAR_STACK *scalar_writes1 = CXX_NEW(SCALAR_STACK(&OLF_default_pool),
                                         &OLF_default_pool);
  SCALAR_STACK *scalar_reads1 = CXX_NEW(SCALAR_STACK(&OLF_default_pool),
                                        &OLF_default_pool);
  SCALAR_REF_STACK *params1 = 
         CXX_NEW(SCALAR_REF_STACK(&OLF_default_pool), &OLF_default_pool);
  DOLOOP_STACK *stack1=CXX_NEW(DOLOOP_STACK(&OLF_default_pool),
                                &OLF_default_pool);
  Build_Doloop_Stack(loop1, stack1);
  Init_Ref_Stmt_Counter();
  UINT array_ref_count1=New_Gather_References(
	    WN_do_body(loop1),writes1,reads1,stack1,
            scalar_writes1,scalar_reads1,params1,&OLF_default_pool);

  if (array_ref_count1 == -1)
    return Failed;
  REF_LIST_STACK *writes2 = CXX_NEW(REF_LIST_STACK(&OLF_default_pool),
                                    &OLF_default_pool);
  REF_LIST_STACK *reads2 = CXX_NEW(REF_LIST_STACK(&OLF_default_pool),
                                   &OLF_default_pool);
  SCALAR_STACK *scalar_writes2 = CXX_NEW(SCALAR_STACK(&OLF_default_pool),
                                         &OLF_default_pool);
  SCALAR_STACK *scalar_reads2 = CXX_NEW(SCALAR_STACK(&OLF_default_pool),
                                        &OLF_default_pool);
  SCALAR_REF_STACK *params2 = 
         CXX_NEW(SCALAR_REF_STACK(&OLF_default_pool), &OLF_default_pool);
  DOLOOP_STACK *stack2=CXX_NEW(DOLOOP_STACK(&OLF_default_pool),
                                &OLF_default_pool);
  Build_Doloop_Stack(loop2, stack2);
  UINT array_ref_count2=New_Gather_References(
	    WN_do_body(loop2),writes2,reads2,stack2,
            scalar_writes2,scalar_reads2,params2,&OLF_default_pool);
  if (array_ref_count2 == -1)
    return Failed;

  if (array_ref_count1+array_ref_count2>OLF_size_upperbound && LNO_Fusion<2) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Number of array references after merge is too big (>100)!!.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Number of array references after merge is too big (>100)!!.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Number of array references after merge is too big (>100)!!.");
    CXX_DELETE(mapping_dictionary, &OLF_default_pool);
    *fusion_level_io=0;
    return Failed;
  }

  INT outer_level1 = dli1->Depth; 
  INT outer_level2 = dli2->Depth; 
  if (Stride_One_Level(writes1,reads1,outer_level1,snl_level1)
      !=Stride_One_Level(writes2,reads2,outer_level2,snl_level2) 
      && LNO_Fusion<2) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Stride-1 level differs.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Stride-1 level differs.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Stride-1 level differs.");
    CXX_DELETE(mapping_dictionary, &OLF_default_pool);
    *fusion_level_io=0;
    return Failed;
  }

  BIT_VECTOR* bv1=Array_Names_In_Loop(writes1,reads1);
  BIT_VECTOR* bv2=Array_Names_In_Loop(writes2,reads2);

  if (!bv1 || !bv2) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Too many (>256) array names in loops.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Too many (>256) array names in loops.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Too many (>256) array names in loops.");
    CXX_DELETE(mapping_dictionary, &OLF_default_pool);
    *fusion_level_io=0;
    return Failed;
  }

  if (LNO_Fusion<2)
  if (array_ref_count1+array_ref_count2>OLF_size_lowerbound &&
      2*((*bv1) & (*bv2)).Pop_Count() < (~(*bv1) & (*bv2)).Pop_Count() && 
      2*((*bv1) & (*bv2)).Pop_Count() < ((*bv1) & ~(*bv2)).Pop_Count()) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Too few common array names or too many new array names in loop2.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Too few common array names or too many new array names in loop2.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Too few common array names or too many new array names in loop2.");
    CXX_DELETE(mapping_dictionary, &OLF_default_pool);
    *fusion_level_io=0;
    return Failed;
  }

  UINT fusion_level=snl_level1;
  if (snl_level2<snl_level1)
    fusion_level=snl_level2;
  UINT fusion_level_tmp=fusion_level;
  UINT peeling_limit=LNO_Fusion_Peeling_Limit;
  if (dli1->Est_Num_Iterations>=0)
    peeling_limit=MIN(peeling_limit,dli1->Est_Num_Iterations);
  if (dli2->Est_Num_Iterations>=0)
    peeling_limit=MIN(peeling_limit,dli2->Est_Num_Iterations);
  if (snl_level1!=snl_level2 || type1!=Inner || type2!=Inner)
    peeling_limit=0;
  FISSION_FUSION_STATUS level_fusion_status;
  FISSION_FUSION_STATUS status=
    Fuse(loop1,loop2,fusion_level,peeling_limit,TRUE);
  if (status==Succeeded || status==Succeeded_and_Inner_Loop_Removed) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Successfully fused outer loops.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(SUCCEED,srcpos1,srcpos2,snl_level1,snl_level2,
        "Successfully fused outer loops.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(SUCCEED,srcpos1,srcpos2,snl_level1,snl_level2,
        "Successfully fused outer loops.");
    CXX_DELETE(mapping_dictionary, &OLF_default_pool);
    if (status==Succeeded_and_Inner_Loop_Removed)
      fusion_level--;
    *fusion_level_io=fusion_level;
    return status;
  } else if (((snl_level1>1 && status==Try_Level_By_Level) || LNO_Fusion==2) &&
             ((level_fusion_status=Fuse_Level_By_Level(loop1,loop2,
                       &fusion_level_tmp,peeling_limit,
                       LNO_Fusion==2,TRUE,ffi))==Succeeded ||
              (level_fusion_status==Partially_fused && LNO_Fusion==2) ||
              level_fusion_status==Succeeded_and_Inner_Loop_Removed)) {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Successfully fused outer loops.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(SUCCEED,srcpos1,srcpos2,snl_level1,snl_level2,
        "Successfully fused outer loops.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(SUCCEED,srcpos1,srcpos2,snl_level1,snl_level2,
        "Successfully fused outer loops.");
    CXX_DELETE(mapping_dictionary, &OLF_default_pool);
    *fusion_level_io=fusion_level_tmp;
    return level_fusion_status;
  } else if (status==Partially_fused || 
             (status==Try_Level_By_Level &&
              level_fusion_status==Partially_fused)) {
    DevWarn("Partially fused loop is not restored");
    *fusion_level_io=fusion_level_tmp;
    return Partially_fused;
  } else {
    if (LNO_Verbose)
      outer_fusion_verbose_info(srcpos1,srcpos2,
        "Failed to fuse outer loops.");
    if (LNO_Analysis)
      outer_fusion_analysis_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Failed to fuse outer loops.");
    if (LNO_Tlog)
      outer_fusion_tlog_info(FAIL,srcpos1,srcpos2,snl_level1,snl_level2,
        "Failed to fuse outer loops.");
    CXX_DELETE(mapping_dictionary, &OLF_default_pool);
    *fusion_level_io=0;
    return Failed;
  }
}

#ifdef KEY
// Bug 1025
static BOOL Same_Loop_Body ( WN* wn, WN* copy, 
			     SYMBOL loop_index, SYMBOL copy_loop_index )
{
  if (!wn || !copy)
    return FALSE;

  OPERATOR wn_opr = WN_operator(wn);
  OPERATOR copy_opr = WN_operator(copy);

  if (wn_opr != wn_opr || WN_kid_count(wn) != WN_kid_count(copy)) 
    return FALSE;

  if (wn_opr == OPR_LDID || wn_opr == OPR_STID) {
    SYMBOL wn_sym(wn);
    SYMBOL copy_sym(copy);
    if (wn_sym != copy_sym && 
	wn_sym != loop_index && copy_sym != copy_loop_index)
      return FALSE;
  }
  else if (wn_opr == OPR_BLOCK) {
    WN* stmt_wn;
    WN* stmt_copy;
    for (stmt_wn=WN_first(wn), stmt_copy = WN_first(copy); 
	 stmt_wn && stmt_copy; 
	 stmt_wn= WN_next(stmt_wn), stmt_copy = WN_next(stmt_copy))
      if (!Same_Loop_Body(stmt_wn, stmt_copy, loop_index, copy_loop_index))
	return FALSE;
    if (stmt_wn == NULL && stmt_copy != NULL ||
	stmt_wn != NULL && stmt_copy == NULL)
      return FALSE;
  } 
  else if (wn_opr == OPR_DO_LOOP) {
    return Same_Loop_Body(WN_do_body(wn), WN_do_body(copy), 
			  loop_index, copy_loop_index);
  } 

  for (INT kid = 0; kid < WN_kid_count(wn); kid ++)
    if (!Same_Loop_Body(WN_kid(wn, kid), WN_kid(copy, kid), 
			loop_index, copy_loop_index))
      return FALSE;
  
  return TRUE;
}
#endif
static OUTER_FUSION_STATUS Outer_Loop_Fusion_Walk(WN* wn,
       FIZ_FUSE_INFO* ffi, WN2UINT *wn2ffi) {
  OPCODE opc=WN_opcode(wn);

  if (!OPCODE_is_scf(opc)) 
    return NORMAL;
  else if (opc==OPC_DO_LOOP) {
    if (Do_Loop_Is_Good(wn) && !Do_Loop_Has_Calls(wn) && 
	!Do_Loop_Has_Gotos(wn)) {
      WN* next_wn=WN_next(wn);

      // hacked to remove pragmas produced by inlining
      while (next_wn && 
             ((WN_operator(next_wn) == OPR_PRAGMA &&
               (WN_pragma(next_wn) == WN_PRAGMA_INLINE_BODY_START ||
                WN_pragma(next_wn) == WN_PRAGMA_INLINE_BODY_END ||
                WN_pragma(next_wn) == WN_PRAGMA_CLIST_SKIP_BEGIN ||
                WN_pragma(next_wn) == WN_PRAGMA_FLIST_SKIP_BEGIN ||
                WN_pragma(next_wn) == WN_PRAGMA_CLIST_SKIP_END ||
                WN_pragma(next_wn) == WN_PRAGMA_FLIST_SKIP_END))
              ||
              (WN_operator(next_wn) == OPR_XPRAGMA &&
               WN_pragma(next_wn) == WN_PRAGMA_COPYIN_BOUND))) {
        LWN_Delete_Tree_From_Block(next_wn);
        next_wn=WN_next(wn);
      } 

      OUTER_FUSION_STATUS state=SKIP;
      while (next_wn && WN_opcode(next_wn)==OPC_DO_LOOP &&
          Do_Loop_Is_Good(next_wn) && !Do_Loop_Has_Calls(next_wn) &&
				      !Do_Loop_Has_Gotos(next_wn)) {
        INT wn_index=wn2ffi->Find(wn);
        if (wn_index == 0) {
          //DevWarn("Missing SNL info for loop1 in Outer_Loop_Fusion_Walk().");
          SNL_INFO snl_info(wn);
          wn_index=ffi->New_Snl(snl_info);
          wn2ffi->Enter(wn,wn_index);
        }
        INT next_wn_index=wn2ffi->Find(next_wn);
        if (next_wn_index == 0) {
          //DevWarn("Missing SNL info for loop2 in Outer_Loop_Fusion_Walk().");
          SNL_INFO snl_info(next_wn);
          next_wn_index=ffi->New_Snl(snl_info);
          wn2ffi->Enter(next_wn,next_wn_index);
        }
        INT d1=ffi->Get_Depth(wn_index);
        INT d2=ffi->Get_Depth(next_wn_index);
        UINT fused_level;
#ifdef KEY
	// Bug 1025
	WN* next_next_copy;
	if (LNO_Fusion == 2)
	  next_next_copy = 
	    LWN_Copy_Tree(WN_next(next_wn), TRUE, LNO_Info_Map);	
#endif
        FISSION_FUSION_STATUS fusion_status=
          Fuse_Outer_Loops(wn,next_wn,ffi,wn2ffi,&fused_level);
        if (fusion_status==Succeeded || fusion_status==Partially_fused) {
          WN* wn1=wn;
          WN* wn2=wn;
          INT level=1;
          while (wn1=Get_Only_Loop_Inside(wn1,FALSE)) {
            level++;
            wn2=wn1;
          }
          if (d1>level) {
            INT i=ffi->Copy_Snl(ffi,wn_index);
            wn1=WN_first(WN_do_body(wn2));
            while (WN_opcode(wn1)!=OPC_DO_LOOP) wn1=WN_next(wn1);
            ffi->Set_Depth(i,d1-level);
            ffi->Set_Wn(i,wn1);
            wn2ffi->Enter(wn1,i);
          }
          if (d2>level) {
            INT j=ffi->Copy_Snl(ffi,next_wn_index);
            wn1=WN_last(WN_do_body(wn2));
            while (WN_opcode(wn1)!=OPC_DO_LOOP) wn1=WN_prev(wn1);
            ffi->Set_Depth(j,d2-level);
            ffi->Set_Wn(j,wn1);
            wn2ffi->Enter(wn1,j);
          }
          if (ffi->Get_Type(wn_index)==Inner &&
              ffi->Get_Type(next_wn_index)==Inner &&
              fusion_status==Succeeded)
            ffi->Set_Type(wn_index,Inner);
          else
            ffi->Set_Type(wn_index,Not_Inner);
          ffi->Set_Depth(wn_index,level);
          ffi->Set_Type(next_wn_index,Invalid);
          wn2ffi->Enter(next_wn, 0);
        } else if (fusion_status==Succeeded_and_Inner_Loop_Removed) {
          ffi->Set_Type(wn2ffi->Find(next_wn),Invalid);
          wn2ffi->Enter(next_wn, 0);
          if (fused_level>0)
            ffi->Set_Depth(wn2ffi->Find(wn),fused_level);
          else {
            ffi->Set_Type(wn2ffi->Find(wn),Invalid);
            wn2ffi->Enter(wn, 0);
            return state; // the original loop has been removed
          }
        } else {
          WN* new_next_wn=WN_next(wn);
          if (next_wn!=new_next_wn) {
            ffi->Set_Wn(wn2ffi->Find(next_wn),new_next_wn);
            wn2ffi->Enter(new_next_wn,wn2ffi->Find(next_wn));
            wn2ffi->Enter(next_wn,0);
          }
          return state;
        }
        next_wn=WN_next(wn);
        state=NORMAL;
#ifdef KEY
	// Bug 1025
	if (LNO_Fusion == 2 && next_wn &&
	    WN_operator(next_wn) == OPR_DO_LOOP &&
	    (!next_next_copy || WN_operator(next_next_copy) != OPR_DO_LOOP ||
	    // Make sure the new next_wn is not a remnant (post-peel) from the 
	    // last fusion. If so, then we have already tried fusing these two
	    // adjacent loops and we had a remainder loop and there is no 
	    // point trying to fuse them again.
	     (!Same_Loop_Body(next_wn, next_next_copy, 
			      WN_index(next_wn), WN_index(next_next_copy)))))
	  return SKIP;
#endif

        // hacked to remove pragmas produced by inlining
        while (next_wn && 
               ((WN_operator(next_wn) == OPR_PRAGMA &&
                 (WN_pragma(next_wn) == WN_PRAGMA_INLINE_BODY_START ||
                  WN_pragma(next_wn) == WN_PRAGMA_INLINE_BODY_END ||
                  WN_pragma(next_wn) == WN_PRAGMA_CLIST_SKIP_BEGIN ||
                  WN_pragma(next_wn) == WN_PRAGMA_FLIST_SKIP_BEGIN ||
                  WN_pragma(next_wn) == WN_PRAGMA_CLIST_SKIP_END ||
                  WN_pragma(next_wn) == WN_PRAGMA_FLIST_SKIP_END))
                ||
                (WN_operator(next_wn) == OPR_XPRAGMA &&
                 WN_pragma(next_wn) == WN_PRAGMA_COPYIN_BOUND))) {
          LWN_Delete_Tree_From_Block(next_wn);
          next_wn=WN_next(wn);
        } 

      }
    } else
      (void)Outer_Loop_Fusion_Walk(WN_do_body(wn),ffi,wn2ffi);
  } else if (opc==OPC_BLOCK) {
    for (WN* stmt=WN_first(wn); stmt; ) {
      WN* prev_stmt=WN_prev(stmt);
      WN* next_stmt=WN_next(stmt);
      OUTER_FUSION_STATUS status=Outer_Loop_Fusion_Walk(stmt,ffi,wn2ffi);
      if (!prev_stmt)
        if (WN_first(wn)!=stmt && status==NORMAL)
          stmt=WN_first(wn);	// a new stmt (loop) is created and
				// should retry
        else if (status==SKIP)		// status is SKIP so no more retry
          stmt=WN_next(WN_first(wn));
        else 			// if (WN_first(wn)==stmt)
          if (WN_next(stmt)==next_stmt)	// no new next stmt (loop) is created
            stmt=next_stmt;
          else;		// a new next stmt (loop) is created and should retry
      else
        if (WN_next(prev_stmt)!=stmt && status==NORMAL)
          stmt=prev_stmt;	// a new stmt (loop) is created and
				// should retry
        // e.g. a new peeled loop has been created before the current stmt
        // we will re-start from the prev_stmt
        else if (status==SKIP)		// status is SKIP so no more retry
          stmt=WN_next(WN_next(prev_stmt));
        else 			// if (WN_next(prev_stmt)==stmt)
          if (WN_next(stmt)==next_stmt)	// no new next stmt (loop) is created
            stmt=next_stmt;
        else;		// a new next stmt (loop) is created and should retry
    }
  } else
    for (UINT kidno=0; kidno<WN_kid_count(wn); kidno++) {
      (void)Outer_Loop_Fusion_Walk(WN_kid(wn,kidno),ffi,wn2ffi);
    }

  return NORMAL;
}

void Outer_Loop_Fusion_Phase(WN* func_nd, FIZ_FUSE_INFO* ffi) {
  
  MEM_POOL_Initialize(&OLF_default_pool,"OLF_default_pool",FALSE);
  MEM_POOL_Push(&OLF_default_pool);

  WN2UINT* wn2ffi=CXX_NEW(WN2UINT(256,&OLF_default_pool),&OLF_default_pool);

  ffi->Copy_Snl(ffi,0);
  ffi->Set_Type(0,Invalid);
  // moved the record of 0th SNL to the end to avoid using 0 as an id
  // in hash table where 0 has special meaning (NULL)

  for (INT i=1; i<ffi->Num_Snl(); i++) {
    WN* loop=ffi->Get_Wn(i);
    wn2ffi->Enter(loop, i);
  }

  Outer_Loop_Fusion_Walk(func_nd,ffi,wn2ffi);

  CXX_DELETE(wn2ffi, &OLF_default_pool);

  MEM_POOL_Pop(&OLF_default_pool);
  MEM_POOL_Delete(&OLF_default_pool);

}


