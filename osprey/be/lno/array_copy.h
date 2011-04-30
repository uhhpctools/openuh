/*

  Copyright (C) 2009 Advanced Micro Devices, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef array_copy_INCLUDED
#define array_copy_INCLUDED

#include "wn.h"

typedef struct SAC_FLD_INFO_
{
  bool is_read;
  bool is_written;
  int new_fld_id;
  int new_offset;
  ST_IDX st_idx;
} SAC_FLD_INFO;

typedef struct SAC_INFO_
{
  WN* func_entry;
  WN* wn_loop; // wn corresponding to the DO_LOOP
  SAC_FLD_INFO* fld_info;
  TY_IDX orig_ty;
  TY_IDX new_ty;
  int orig_num_fields;
  int new_num_fields;
  int new_struct_size;
  ST* old_stride_sym;
  ST* new_stride_sym;
  WN_OFFSET new_stride_preg;
  WN* array_copy_wn;
  WN* copy_insertion_block;
  WN* copy_insertion_wn;
  WN* saved_start_wn;
  WN* saved_end_wn;
  WN* saved_int_stride;
  WN* saved_struct_stride;
  WN* saved_num_chunks_wn;
  OPCODE end_comp_op;
  bool end_is_kid0;
} SAC_INFO;

extern void Perform_Structure_Array_Copy_Opt(WN* func_node);

void Delete_SAC_Info(SAC_INFO*& sac_info);
void Find_Struct_Array_Copy_Candidate(SAC_INFO*& sac_info,
                                      WN* wn,
                                      bool collect_field_info,
                                      int& depth);

bool Check_Candidate_Legality(WN* wn_tree, SAC_INFO* sac_info);
void Traverse_WN_Tree_For_SAC_Legality(WN* wn, 
                                       SAC_INFO* sac_info, 
                                       bool& is_legal);
bool Routine_Is_Inlined_And_Safe(WN* tree, ST* callee, SAC_INFO* sac_info);
void Check_For_Inlined_Routine_Safety(WN* wn, ST* callee, 
                                      SAC_INFO* sac_info,
                                      bool& is_inlined);

void Collect_Loop_Field_Refs(WN* loop_body, SAC_INFO*& sac_info);
void Check_WN_For_Field_Refs(WN* wn, SAC_INFO*& sac_info);
void Create_New_Struct_Type(SAC_INFO* sac_info);

void Setup_Common_Info(SAC_INFO*& sac_info, WN* copy_block);
void Allocate_Struct_Copy_Array(SAC_INFO* sac_info,
                                WN* insertion_block,
                                WN* insertion_wn);
void Free_Struct_Copy_Array(SAC_INFO* sac_info,
                            WN* insertion_block);

void Insert_Array_Copy_Code(SAC_INFO*& sac_info);
BOOL Find_Insertion_Point(SAC_INFO* sac_info);
void Find_Def_Block(WN* wn, SAC_INFO* sac_info, WN*& def_block, WN*& def_node);
void Copy_Bounds_Defs(WN* expr, SAC_INFO*& sac_info, WN* insertion_block, 
                      WN* insertion_wn, ST* old_sym, ST* new_sym);
WN* Find_Definition(ST* sym, WN* block, WN* stop);
WN* Create_Copy_Loop_Code(SAC_INFO* sac_info, WN* copy_block);
void Do_DU_Update(WN* wn);

void Traverse_WN_Tree_For_Struct_Copy_Opt(SAC_INFO* sac_info);

void Insert_Sync_Copy_Code(SAC_INFO* sac_info);
void Find_Writes_To_Struct_Type(WN* wn, 
                                SAC_INFO* sac_info, 
                                bool found_insertion_pt);
WN* Generate_Copy_Code_For_Write(WN* wn, SAC_INFO* sac_info);

void Walk_And_Replace_Refs(WN* wn, SAC_INFO* sac_info, WN* idx_expr,
                           WN* parent, int kidno);

#endif // array_Copy_INCLUDED
