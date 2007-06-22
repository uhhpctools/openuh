/*
  Copyright (c) 2001, Institute of Computing Technology, Chinese Academy of Sciences
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:
  
  Redistributions of source code must retain the above copyright notice, this list
  of conditions and the following disclaimer. 

  Redistributions in binary form must reproduce the above copyright notice, this list
  of conditions and the following disclaimer in the documentation and/or other materials
  provided with the distribution. 

  Neither the name of the owner nor the names of its contributors may be used to endorse or
  promote products derived from this software without specific prior written permission. 

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
  FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
 
//-*-c++-*-

#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <stack>
#include <list>
#include <set>

#include "bb.h"
#include "defs.h"
#include "cg_region.h"
#include "label_util.h"
#include "symtab.h"
#include "data_layout.h"
#include "symtab_access.h"
#include "cg_flags.h"
#include "vt_region.h"
#include "tracing.h"
#include "instr_reader.h"
#include "dump_feedback.h"
#include "freq.h"
#include "ipfec_defs.h"
#include "gra_live.h"
#include "region.h"
#include "region_bb_util.h"
#include "region_update.h"
#include "region_verify.h"
#include "ipfec_options.h"

#include "val_prof.h"

#define GEN_NEWBB_FOR_RESTORE___Should_disable_ebo
//#define VALUE_PROFILE_INTERNAL_DEBUG		
//#define _maybe_wrong_test
//#define _delete_but_not_fully_tested_yet
#define	VALUE_PROFILE_DETAIL

#define My_OP_to_split_entry1 TOP_mov_f_pr
#define My_OP_to_split_entry2 TOP_mov_f_ar
#define My_OP_to_split_entry3 TOP_mov_f_br
#define My_OP_to_split_entry4 TOP_spadjust
#define TNV_N 10

INST2PROFLIST inst2prof_list;

//OP_TNV_MAP op_tnv_map;
OP_MAP op_tnv_map; 
OP_MAP op_stride_tnv_map;

/* The FB_Info_Value definition in ORC conflicit with its def
 * in current compiler. Turn off val-prof for the time-being.
 */
#if 0
extern INT32 current_PU_handle;
extern BOOL PU_Has_Calls;
extern SAVE_REG *Return_Address_Reg;

extern char * Instrumentation_File_Name;
static INT32   pu_num_stride = 0;  

void Handle_All_Hazards (BB *bb);
BB * Divide_BB(BB *bb, OP *point);
BB * RGN_Divide_BB(BB *bb, OP *point);
void Exp_COPY (TN *tgt_tn, TN *src_tn, OPS *ops);

char * get_cat_str(char * str1, char * str2, MEM_POOL * mempool=NULL)
{
	char * str;
	if (!mempool)
	{
		DevWarn("Not using memory pool when alloc memory to get_cat_str!");
		str = (char *)malloc(strlen(str1)+strlen(str2)+2);
	}
	else str = (char *)CXX_NEW_ARRAY(char, strlen(str1)+strlen(str2)+2, mempool); 

	strcpy(str, str1);
	strcat(str,"/");
	strcat(str,str2);
	return str;
}

class CG_VALUE_INSTRUMENT_WALKER {
private:
  // ------------------------------------------------------------------
  // Private members of CG_VALUE_INSTRUMENT_WALKER should be accessed only
  // through the private methods provided further below.
  // ------------------------------------------------------------------
		
    MEM_POOL * _mempool;	// the local memory pool
    UINT32 _ld_count;		// counter for type of instruction we are instrumenting//here
    UINT32 _st_count;		// counter for type of instruction we are instrumenting//here
	UINT32 _other_count;   // all other count except load/restore count
	UINT32 _instrument_count;	// checksum of all inst. instrumented
	UINT32 _stride_instr_count;	// checksum of all  stride inst. instrumented
    UINT32 _qulified_count;	// counter of all inst. which are qulified to be instrumented
    BOOL _trace; // if true, print trace information into the trace file ( .t file ).
    PROFILE_PHASE _phase; //in which phase the instrumentation occurs
    BOOL _prefetch; //if true,do stride profiling in value profiling
    BOOL _do_value; //if true ,do value 
    TN * _pu_hdr_return_tn; //store the value of pu init call 
    
public: 
    CG_VALUE_INSTRUMENT_WALKER(MEM_POOL *m, UINT32 lc, UINT32 sc, UINT32 total, PROFILE_PHASE phase, BOOL stride_profiling_flag, BOOL do_value) :
	_mempool(m), _ld_count(0), _st_count(0), _other_count(0), _instrument_count(0), _stride_instr_count(0), _qulified_count(0),
		_trace( Get_Trace(TP_A_PROF, TT_VALUE_PROF) ),
		_phase(phase),
		_prefetch(stride_profiling_flag), _do_value(do_value), _pu_hdr_return_tn(NULL)
	{}

	~CG_VALUE_INSTRUMENT_WALKER(){}

    void CFG_Walk(CGRIN *rin);

protected:
    BB * Val_Prof_Divide_BB(BB *bb, OP *point);
	void Val_Prof_Insert_BB(BB *bb, BB* after);
	inline void Try_to_set_op_copy(OP * op, TN * tn1, TN * tn2);
	TN * Find_original_TN( OPS *save_ops, TN * tn );
    TN * Find_TN( TOP opc,INT bb_num,INT TN_num, TN* t1, INT index );
    OP* Find_OP( TOP opc, BB * bb);
	inline OP * Gen_Param(ISA_REGISTER_CLASS rclass, REGISTER reg, INT size, TN* tn );
	inline OPS * Gen_Param(ISA_REGISTER_CLASS rclass, REGISTER reg, INT size, char * str_arg );
	inline void Gen_Param_append_ops(ISA_REGISTER_CLASS rclass, REGISTER reg, INT size, TN* tn , OPS * ops);
	inline OP * Gen_Call_Shell( char *name, TYPE_ID rtype);
	inline OP * Gen_Call_Shell_append_ops( char *name, TYPE_ID rtype, OPS * ops);
	inline void Gen_Save_Restore_Input_Reg_append_ops( ISA_REGISTER_CLASS rclass, INT size, OPS * save_ops, OPS * restore_ops, BOOL no_move);
	inline void Gen_Save_Restore_Output_Reg_append_ops( ISA_REGISTER_CLASS rclass, INT size, OPS * save_ops, OPS * restore_ops, BOOL no_move);
	inline void Gen_Save_Restore_Dedicated_Reg_append_ops( ISA_REGISTER_CLASS rclass, REGISTER reg, INT size, OPS * save_ops, OPS * restore_ops, BOOL no_move);
	void Gen_Save_Restore_Return_Addr_Reg_append_ops(OPS * save_ops, OPS * restore_ops);
	inline void Gen_Save_Restore_Return_Value_Reg_append_ops(OPS * save_ops, OPS * restore_ops);
	inline void Gen_Save_Restore_GP_Reg_append_ops(OPS * save_ops, OPS * restore_ops);
	inline void Gen_Call_Init_Return_ops(OPS * restore_ops);
	void Adjust_Save_Restore_ARPFS();
	BB * CG_Gen_Call(char * func_name, char * str_arg, INT int_arg1, INT int_arg2);
	BB * CG_Gen_Call(char * func_name, char * str_arg1, char * str_arg2, INT int_arg1, INT int_arg2);
	BB * CG_Gen_Call(char * func_name, char * str_arg, INT int_arg);
	OP * Find_OP_to_split_entry_exit(BB *bb);
	void Prepare_Insert_BBs_for_call(BB* bb);
	void Prepare_Insert_BBs_for_call_init(BB* bb);	
	void Insert_BBs_for_call(BB* bb, OPS * save_ops, BB * call_bb, OPS * restore_ops);
	void Insert_BBs_for_call(BB* bb, OPS * save_ops, BB * call_bb, BB * srd_call_bb, OPS * restore_ops);
	void Insert_BBs_for_call_init(BB* bb, OPS * save_ops, BB * call_bb, OPS * restore_ops);	
    void Insert_BBs_for_call_init(BB* bb, OPS * save_ops, BB * call_bb, BB * srd_call_bb, OPS * restore_ops);	
    void Update_CFG_around_BB(BB * bb, BB * new_inserted_bb);
   
	void Request_Output_Registers();

    BB * CG_Gen_Call(char *func_name, TN * PU_Handle, INT id, TN * res_tn);
    BB * CG_Gen_Call(char *func_name, char * srcfile_pu_name, INT id, TN * res_tn);
    void Do_instrument(INST_TO_PROFILE *p, BB *bb, OP * op, BOOL instr_before);
    void BB_Walk(BB *bb);
public:
	inline WN *Gen_Param( WN *arg, UINT32 flag )
	{
		return WN_CreateParm( WN_rtype( arg ), arg, MTYPE_To_TY( WN_rtype( arg ) ), flag );
	}

	WN * Gen_Call_Shell( char *name, TYPE_ID rtype, INT32 argc )
	{
		TY_IDX  ty = Make_Function_Type( MTYPE_To_TY( rtype ) );
		ST     *st = Gen_Intrinsic_Function( ty, name );

		Clear_PU_no_side_effects( Pu_Table[ST_pu( st )] );
		Clear_PU_is_pure( Pu_Table[ST_pu( st )] );
		Set_PU_no_delete( Pu_Table[ST_pu( st )] );

		WN *wn_call = WN_Call( rtype, MTYPE_V, argc, st );
		WN_Set_Call_Default_Flags(  wn_call );
		return wn_call;
	}  

	WN * Gen_Call( char *name, WN *arg1, WN *arg2, TYPE_ID rtype = MTYPE_V )
	{
		WN *call = Gen_Call_Shell( name, rtype, 2 );
		WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
		WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
		return call;
	}

	WN * Gen_Call( char *name, WN *arg1, WN *arg2, WN *arg3, TYPE_ID rtype = MTYPE_V )
	{
		WN *call = Gen_Call_Shell( name, rtype, 3 );
		WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
		WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
		WN_actual( call, 2 ) = Gen_Param( arg3, WN_PARM_BY_VALUE );
		return call;
	}

	WN * Gen_Call( char *name, WN *arg1, WN *arg2, WN *arg3, WN * arg4, TYPE_ID rtype = MTYPE_V )
	{
		WN *call = Gen_Call_Shell( name, rtype, 4 );
		WN_actual( call, 0 ) = Gen_Param( arg1, WN_PARM_BY_VALUE );
		WN_actual( call, 1 ) = Gen_Param( arg2, WN_PARM_BY_VALUE );
		WN_actual( call, 2 ) = Gen_Param( arg3, WN_PARM_BY_VALUE );
		WN_actual( call, 3 ) = Gen_Param( arg4, WN_PARM_BY_VALUE );
		return call;
	}

};


BB * CG_VALUE_INSTRUMENT_WALKER::Val_Prof_Divide_BB(BB *bb, OP *point)
{
	if (IPFEC_Enable_Region_Formation && 
		_phase != PROFILE_PHASE_BEFORE_REGION)
	{
		BB * newbb;
		newbb = RGN_Divide_BB(bb, point);

		if (newbb)
		{
        	BB_freq(newbb) = BB_freq(bb);
        	Set_Freq(bb,newbb, BB_freq(bb));

        	Set_BB_profile_splitted(bb);
        	Set_BB_profile_splitted(newbb);
        	if ( BB_chk_split(bb) || BB_chk_split_head(bb) )
        		Set_BB_chk_split(newbb);
			//Maybe this should not be like this, should check with Gange.
			//But in scheduler.cxx:4598, it use BB_chk_split_head(bb) to guard the Handle_Chk_Split_Bunch(bb)
			// if it can also use BB_chk_split(bb) to trigger Handle_Chk_Split_Bunch, the next statement is not needed.
        	if ( BB_chk_split_head(bb) )
        		Set_BB_chk_split_head(newbb);
        	newbb->id_before_profile = BB_id_before_profile(bb);
		}
		return newbb;
	}
	else
	{
		BB * newbb;
		newbb = Divide_BB(bb, point);
		if (newbb)
		{
	        BB_freq(newbb) = BB_freq(bb);
    	        Set_Freq(bb,newbb, BB_freq(bb));

        	Set_BB_profile_splitted(bb);
        	Set_BB_profile_splitted(newbb);
        	newbb->id_before_profile = BB_id_before_profile(bb);
		Update_CFG_around_BB(bb, newbb);
		}
		return newbb;
	}
}

void CG_VALUE_INSTRUMENT_WALKER::Val_Prof_Insert_BB(BB *bb, BB* after)
{
	REGIONAL_CFG * regional_cfg;
	Insert_BB(bb, after);
	if (IPFEC_Enable_Region_Formation && 
		_phase != PROFILE_PHASE_BEFORE_REGION)
	{
		regional_cfg = Home_Region(after)->Regional_Cfg();
		regional_cfg->Add_Node(bb);
	}
	Update_CFG_around_BB(after, bb);
}

//This function only deals such case: after <new_inserted_bb> is inserted after <bb>, it update the CFG to reflect this.
void CG_VALUE_INSTRUMENT_WALKER::Update_CFG_around_BB(BB * bb, BB * new_inserted_bb)
{
    BBLIST* nxt;
    BBLIST* succ;
    for(succ = BB_succs(bb); succ; succ = nxt)
    {
        BB* bb_succ = BBLIST_item(succ);
        nxt = BBLIST_next(succ);
        if (IPFEC_Enable_Region_Formation && 
               _phase != PROFILE_PHASE_BEFORE_REGION)
        {
            RGN_Link_Pred_Succ_With_Prob( new_inserted_bb, bb_succ, BBLIST_prob(succ) );
            RGN_Unlink_Pred_Succ( bb, bb_succ);
        }
        else
        {
	        Link_Pred_Succ_with_Prob(new_inserted_bb, bb_succ, BBLIST_prob(succ));
    	    Unlink_Pred_Succ(bb, bb_succ);
        }
        //update frequency information
        Set_Freq(new_inserted_bb, bb_succ, BBLIST_freq(succ));
        
    }
        if (IPFEC_Enable_Region_Formation && 
               _phase != PROFILE_PHASE_BEFORE_REGION)
        {
    		RGN_Link_Pred_Succ_With_Prob(bb, new_inserted_bb, 1.0);
        }
        else Link_Pred_Succ_with_Prob(bb, new_inserted_bb, 1.0);

        //update frequency information
        BB_freq(new_inserted_bb) = BB_freq(bb);
        Set_Freq(bb,new_inserted_bb, BB_freq(bb));
}

TN* CG_VALUE_INSTRUMENT_WALKER::Find_original_TN( OPS *save_ops, TN * tn )
{
	Is_True(tn!=NULL, ("tn should not be NULL!"));

	if (save_ops == NULL)
		return NULL;
	for (OP * op=OPS_first(save_ops); op !=NULL; op = OP_next(op))
	{
		if (OP_opnd(op,1) == tn)
			return OP_result(op,0);
	}
	return NULL;
}

// Find the TN which opcode is opc,return the No index TN
TN* CG_VALUE_INSTRUMENT_WALKER::Find_TN( TOP opc,INT bb_num,INT TN_num, TN* t1, INT index)
{
  Is_True( bb_num>0,( "the BB num is not positive" ) );
  Is_True( TN_num>=0,( " the TN num is less than 0" ) );
  Is_True( index>=0,( " the TN num is less than 0" ) );
  Is_True( index<=9,( " the TN num is not less than 9" ) );
  BB* bb= REGION_First_BB;
  for( INT i=1; i<bb_num; i++ )
    if( bb != NULL )
      bb = BB_next( bb );
  
  if ( bb == NULL )
    return NULL;
  
  OP* op = BB_first_op(bb);
  for( ; op != NULL; op = OP_next( op ) )
  {
    if ( OP_code( op ) != opc )
      continue;
      
    if ( OP_opnd( op,TN_num ) != t1 )
      continue;
    else
      return OP_opnd( op,index );
  }
  
  if ( op == NULL )
    return NULL;
}

// Find the OP which opcode is opc,return the pointer of OP
OP* CG_VALUE_INSTRUMENT_WALKER::Find_OP( TOP opc, BB * bb)
{
	Is_True( bb!=NULL,( "Find_OP: <bb> should not be NULL!! " ) );

	for( OP* op = BB_first_op(bb); op != NULL; op = OP_next( op ) )
	{
		if ( OP_code( op ) == opc )
			return op;
	}
	return NULL;
}

inline void CG_VALUE_INSTRUMENT_WALKER::Try_to_set_op_copy(OP * op, TN * tn1, TN * tn2)
{
	if ( TN_is_register(tn1) && TN_is_register(tn2) )
		Set_OP_copy(op);
}

inline OP * CG_VALUE_INSTRUMENT_WALKER::Gen_Param(ISA_REGISTER_CLASS rclass, REGISTER reg, INT size, TN* tn )
{
	Is_True(tn!=NULL, ("ERROR: Try to use NULL tn to generate parameter!"));
	TN* TN_output_reg = Build_Dedicated_TN( rclass, reg, size );
	OP* mov_op;
	if ( rclass == ISA_REGISTER_CLASS_integer )
	{
		if (TN_is_register(tn))
		{
			mov_op = Mk_OP( TOP_mov, TN_output_reg, True_TN, tn ); 
			Try_to_set_op_copy(mov_op, TN_output_reg, tn );
		}
		else if (TN_is_constant(tn))
		{
			mov_op = Mk_OP( TOP_mov_i, TN_output_reg, True_TN, tn );
			Try_to_set_op_copy(mov_op, TN_output_reg, tn );
		}
		else Is_True(FALSE, ("ERROR: tn is neither register nor constant!!"));
	}
	else
	{
		if (TN_is_register(tn))
		{
			mov_op = Mk_OP( TOP_mov_f, TN_output_reg, True_TN, tn ); 
			Try_to_set_op_copy(mov_op, TN_output_reg, tn );
		}
		else Is_True(FALSE, ("I do not know which opr is for mov floating constant to a register!!"));
	}

	return mov_op;
}

inline OPS * CG_VALUE_INSTRUMENT_WALKER::Gen_Param(ISA_REGISTER_CLASS rclass, REGISTER reg, INT size, char * str_arg )
{
	OPS * newops;
	newops = OPS_Create();
	
	Is_True(rclass == ISA_REGISTER_CLASS_integer, ("ERROR: pointer should only be passed to ISA_REGISTER_CLASS_integer!"));

	TN* save_gp_TN = NULL;
	save_gp_TN = Find_TN( TOP_mov,1,1,GP_TN,2 );
	if ( save_gp_TN != NULL )
	{
		OP* mov_op = Mk_OP( TOP_mov,GP_TN,True_TN,save_gp_TN );
		Try_to_set_op_copy(mov_op, GP_TN, save_gp_TN );
		OPS_Append_Op( newops, mov_op );
	}  
  
	TN* str_arg_tgt_TN = Gen_Register_TN( ISA_REGISTER_CLASS_integer,8 );
	// create the string symtal constant
	TCON tcon = Host_To_Targ_String ( MTYPE_STRING, str_arg,strlen( str_arg )+1 );
	TY_IDX ty = MTYPE_To_TY( MTYPE_STRING );
	ST* st = Gen_String_Sym ( &tcon, ty, FALSE );
	Allocate_Object( st );  
	INT64 offset = 0;
	INT32 relocs = TN_RELOC_IA_LTOFF22;
	TN* var_name_tn = Gen_Symbol_TN ( st , offset, relocs );
	OP* addl_op = Mk_OP( TOP_addl, str_arg_tgt_TN, True_TN, var_name_tn, GP_TN );
	OPS_Append_Op( newops, addl_op );

	TN* TN_output_reg = Build_Dedicated_TN( rclass, reg, size );
	TN* enum_ldtype = Gen_Enum_TN( ECV_ldtype );
	TN* enum_ldhint = Gen_Enum_TN( ECV_ldhint );
	OP* ld8_op = Mk_OP( TOP_ld8,TN_output_reg,True_TN, enum_ldtype,enum_ldhint,str_arg_tgt_TN );
	OPS_Append_Op( newops, ld8_op );
  
	return newops;
}

inline void CG_VALUE_INSTRUMENT_WALKER::Gen_Param_append_ops(ISA_REGISTER_CLASS rclass, REGISTER reg, INT size, TN* tn , OPS * ops)
{
	OPS_Append_Op(ops, Gen_Param(rclass, reg, size, tn));
}

inline OP * CG_VALUE_INSTRUMENT_WALKER::Gen_Call_Shell( char *name, TYPE_ID rtype)
{
	TN* enum_sptk = Gen_Enum_TN( ECV_bwh_sptk );
	TN* enum_many = Gen_Enum_TN( ECV_ph_many );
	TN* enum_dh = Gen_Enum_TN( ECV_dh );

	TN *ar_ec = Build_Dedicated_TN ( ISA_REGISTER_CLASS_application,
				   ( REGISTER )( REGISTER_MIN + 66 ), 
				   8 );
	TY_IDX ty = Make_Function_Type( MTYPE_To_TY( rtype ) );
	ST *call_st = Gen_Intrinsic_Function( ty, name );
	Clear_PU_no_side_effects( Pu_Table[ST_pu( call_st )] );
	Clear_PU_is_pure( Pu_Table[ST_pu( call_st )] );
	Set_PU_no_delete( Pu_Table[ST_pu( call_st )] );
	TN * func_name_tn = Gen_Symbol_TN( call_st,0,0 );
	OP* call_op =Mk_OP( TOP_br_call,RA_TN,True_TN,enum_sptk,enum_many,enum_dh,func_name_tn,ar_ec );

	return call_op;
}

inline OP * CG_VALUE_INSTRUMENT_WALKER::Gen_Call_Shell_append_ops( char *name, TYPE_ID rtype, OPS * ops)
{
	OPS_Append_Op(ops, Gen_Call_Shell( name, rtype ));
}

inline void CG_VALUE_INSTRUMENT_WALKER::Gen_Save_Restore_Output_Reg_append_ops( ISA_REGISTER_CLASS rclass, INT size, OPS * save_ops, OPS * restore_ops, BOOL no_move=FALSE)
{
	Is_True(rclass == ISA_REGISTER_CLASS_integer, ("I am not sure if other type than ISA_REGISTER_CLASS_integer is right!! Check this procedure please!!"));
	TN * save_tn, *reg_tn;
	OP * save_op, *restore_op;
	for (INT i = 0; i<REGISTER_Number_Stacked_Output(rclass); i++)
	{
		Gen_Save_Restore_Dedicated_Reg_append_ops( rclass, FIRST_OUTPUT_REG - i , size, save_ops, restore_ops, no_move);
	}
}

inline void CG_VALUE_INSTRUMENT_WALKER::Gen_Save_Restore_Input_Reg_append_ops( ISA_REGISTER_CLASS rclass, INT size, OPS * save_ops, OPS * restore_ops, BOOL no_move=FALSE)
{
	Is_True(rclass == ISA_REGISTER_CLASS_integer, ("I am not sure if other type than ISA_REGISTER_CLASS_integer is right!! Check this procedure please!!"));
	TN * save_tn, *reg_tn;
	OP * save_op, *restore_op;
	for (INT i = 0; i<REGISTER_Number_Stacked_Local(rclass); i++)
	{
		if (i>=8)
			break;
		Gen_Save_Restore_Dedicated_Reg_append_ops( rclass, FIRST_INPUT_REG + i , size, save_ops, restore_ops, no_move);
	}
}

inline void CG_VALUE_INSTRUMENT_WALKER::Gen_Save_Restore_Dedicated_Reg_append_ops( ISA_REGISTER_CLASS rclass, REGISTER reg, INT size, OPS * save_ops, OPS * restore_ops, BOOL no_move=FALSE)
{
	TN * save_tn, * reg_tn;
	OP * save_op, * restore_op;
	TOP the_mov_opr;

	if (rclass == ISA_REGISTER_CLASS_integer)
		the_mov_opr = TOP_mov;
	else if (rclass == ISA_REGISTER_CLASS_float)
		the_mov_opr = TOP_mov_f;
	else 
	{
		the_mov_opr = TOP_mov;
		DevWarn("Not sure if non ISA_REGISTER_CLASS_integer(float) is right here!");
	}
	
	save_tn = Gen_Register_TN( rclass, size );
#ifndef _maybe_wrong_test
	Set_TN_is_global_reg( save_tn );
#endif
	reg_tn = Build_Dedicated_TN( rclass, reg , size );
#ifdef  _maybe_wrong_test
	Reset_TN_is_global_reg(reg_tn);
	if (TN_is_global_reg(reg_tn))
		fprintf(stderr, "Tn is global!\n");
	else fprintf(stderr, "Tn is local!\n"); 
#endif
	save_op = Mk_OP(the_mov_opr, save_tn, True_TN, reg_tn);
	Try_to_set_op_copy(save_op, save_tn, reg_tn);
	if (no_move)
		Set_OP_no_move_before_gra(save_op);
	OPS_Append_Op( save_ops, save_op);

	restore_op = Mk_OP(the_mov_opr, reg_tn, True_TN, save_tn);
        if( !IPFEC_Enable_Edge_Profile )
	Try_to_set_op_copy(restore_op, reg_tn, save_tn);
	if (no_move)
		Set_OP_no_move_before_gra(restore_op);
#ifndef _maybe_wrong_test
	OPS_Prepend_Op( restore_ops, restore_op );
#endif
}

inline void CG_VALUE_INSTRUMENT_WALKER::Gen_Save_Restore_Return_Value_Reg_append_ops(OPS * save_ops, OPS * restore_ops)
{
	//save&restore r8 -- r11
    for ( INT i = 0; i < 4; i++ )  
    {
    	Gen_Save_Restore_Dedicated_Reg_append_ops( ISA_REGISTER_CLASS_integer, 
    		RETURN_REG + i , 8,  save_ops, restore_ops);
    }  

	//save&resotre f8 -- f15
    for ( INT i = 0; i < 8; i++ )  
    {
    	Gen_Save_Restore_Dedicated_Reg_append_ops( ISA_REGISTER_CLASS_float, 
    		FLOAT_RETURN_REG + i , 8, save_ops, restore_ops);
    }  
}

void CG_VALUE_INSTRUMENT_WALKER::Gen_Save_Restore_Return_Addr_Reg_append_ops(OPS * save_ops, OPS * restore_ops)
{
	TN * save_tn;
	if ( TN_register_class(RA_TN) != ISA_REGISTER_CLASS_integer )
	{
		save_tn = Build_RCLASS_TN (ISA_REGISTER_CLASS_integer);
		Set_TN_save_creg (save_tn, TN_class_reg(RA_TN));
		Exp_COPY (save_tn, RA_TN, save_ops );
		//Set_TN_is_global_reg( save_tn );
	} 
	else 
	{
		Exp_COPY (SAVE_tn(Return_Address_Reg), RA_TN, save_ops );
		//Set_TN_is_global_reg( SAVE_tn(Return_Address_Reg) );
	}
	if ( TN_register_class(RA_TN) != ISA_REGISTER_CLASS_integer )
	{
		Exp_COPY (RA_TN, save_tn, restore_ops );
	}
	else
	{
        // Copy back the return address register from the save_tn. 
      	Exp_COPY ( RA_TN, SAVE_tn(Return_Address_Reg), restore_ops );
	}
	
}


inline void CG_VALUE_INSTRUMENT_WALKER::Gen_Save_Restore_GP_Reg_append_ops(OPS * save_ops, OPS * restore_ops)
{
	TN * save_tn, * reg_tn;
	OP * save_op, * restore_op;
	save_tn = Gen_Register_TN( ISA_REGISTER_CLASS_integer, 8 );
#ifndef _maybe_wrong_test
	Set_TN_is_global_reg( save_tn );
#endif

	reg_tn = GP_TN;
        Set_TN_is_global_reg( save_tn );
	save_op = Mk_OP(TOP_mov, save_tn, True_TN, reg_tn);
	Try_to_set_op_copy(save_op, save_tn, reg_tn);
	OPS_Append_Op( save_ops, save_op);

	restore_op = Mk_OP(TOP_mov, reg_tn, True_TN, save_tn);
	Try_to_set_op_copy(restore_op, reg_tn, save_tn);
	OPS_Prepend_Op( restore_ops, restore_op );
}

inline void CG_VALUE_INSTRUMENT_WALKER::Gen_Call_Init_Return_ops(OPS * restore_ops)
{
    TN * return_reg_tn;
    OP * save_op;
    return_reg_tn = Build_Dedicated_TN(ISA_REGISTER_CLASS_integer, RETURN_REG, 8);
    save_op = Mk_OP(TOP_mov, _pu_hdr_return_tn, True_TN, return_reg_tn);
    Try_to_set_op_copy(save_op, _pu_hdr_return_tn, return_reg_tn);
    OPS_Insert_Op_After(restore_ops, OPS_first(restore_ops), save_op );
}

void CG_VALUE_INSTRUMENT_WALKER::Adjust_Save_Restore_ARPFS()
{
  	OP* save_ar_pfs = BB_first_op(REGION_First_BB);
  	Is_True( save_ar_pfs !=NULL && OP_code(save_ar_pfs) == TOP_alloc, ("I assume the first op is always alloc op!"));
  	TN* save_ar_pfs_TN = OP_result( save_ar_pfs,0 );
  	Is_True( save_ar_pfs_TN != NULL,( "there should be a IR that alloc pfs" ) );

  	//more assertion on entry_bb
  	{
  		INT i=0;
		BB_LIST * bblist;
	 	Is_True(Entry_BB_Head !=NULL, ("Entry should not be NULL!"));
	    for (bblist = Entry_BB_Head; bblist; bblist = BB_LIST_rest(bblist) )
	    {
	    	i++;
	    }
    	if (i>1)
    		Is_True(FALSE, ("Assumption of only one Entry_BB for each PU failed!"));
    	else if (i==0)
    		Is_True(FALSE, ("Entry_BB_Head has no BB!"));
  	}

    if ( !PU_Has_Calls )
    {
    	OP* mov_t_ar_r_op;
	 	BB_LIST * bblist;
    	for ( bblist = Exit_BB_Head; bblist; bblist = BB_LIST_rest( bblist ) )
    	{
    		BB * bb = BB_LIST_first( bblist );
    		// see if there is already one <mov_t_ar_r_op> -- restore of ar.pfs. Since PU has no calls, it should
    		// not find such OP except such case: some other phase such as "edge profiling" inserted <mov_t_ar_r_op> but it 
    		// forget to mark PU_Has_Call.
    		mov_t_ar_r_op = Find_OP(TOP_mov_t_ar_r_i, bb);
    		Is_True( mov_t_ar_r_op ==NULL, ("Since PU has no call, now should not exist restore ar.pfs op!!"));

    		mov_t_ar_r_op = Mk_OP( TOP_mov_t_ar_r_i,Pfs_TN,True_TN,save_ar_pfs_TN );
    		Is_True(bb->ops.last != NULL, ("NULL Exit_BB!"));

	        BB_Insert_Op_Before( bb, bb->ops.last,mov_t_ar_r_op );
    	}
  	}  

}

BB * CG_VALUE_INSTRUMENT_WALKER::CG_Gen_Call(char *func_name, char * str_arg1, INT id, TN * res_tn)
{
	OPS * new_ops;
	new_ops = OPS_Create();
	
	OPS_Append_Ops(new_ops, Gen_Param(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG, 8, str_arg1) );
	Gen_Param_append_ops(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG-1, 8, Gen_Literal_TN(id, 8), new_ops );
	Gen_Param_append_ops(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG-2, 8, res_tn, new_ops );
	Gen_Call_Shell_append_ops( func_name, MTYPE_V, new_ops );

	BB * bb;
	bb = Gen_BB();
	Set_BB_call(bb);
	Set_BB_profile_added(bb);
	BB_Append_Ops(bb, new_ops);
  // add annotations about callinfo 
  WN *str_arg_tn1 = WN_LdaString( str_arg1, 0, strlen( str_arg1 ) + 1 );
  WN * arg2 = WN_Intconst( MTYPE_I8, id );
  WN * arg3 = WN_Intconst( MTYPE_I8, 0 );
  WN* call = Gen_Call( func_name, str_arg_tn1, arg2, arg3 );
  ST * call_st = WN_st( call );
  CALLINFO* call_info = TYPE_PU_ALLOC ( CALLINFO );
  CALLINFO_call_st( call_info ) = call_st;
  CALLINFO_call_wn( call_info ) = call;
  BB_Add_Annotation ( bb, ANNOT_CALLINFO, call_info );

	return bb;
}

BB * CG_VALUE_INSTRUMENT_WALKER::CG_Gen_Call(char *func_name, TN * PU_Handle, INT id, TN * res_tn)
{
	OPS * new_ops;
	new_ops = OPS_Create();
	
	// Step I:generate arg1 -- pu_handle -- here this value is in parameter "PU_Handle"
	// The Generate opcode is like:
	//    TN126( r127 ) :- mov TN257( p0 ) TN713 //TN713 contains the pointer of pu_handle
	Gen_Param_append_ops(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG, 8, PU_Handle, new_ops );

	// Step II:generate arg2
	// The Generate opcode is like:
	//    TN127( r126 ) :- mov_i TN257( p0 ) ( 0xj ) //0xi is parameter "id"
	Gen_Param_append_ops(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG-1, 8, Gen_Literal_TN(id, 8), new_ops );

	// Step III:generate arg3 -- the real value pass to instr lib. --here it's value is in parameter "res_tn".
	// The Generate opcode is like:
	//    TN128( r125 ) :- mov TN257( p0 ) TN714  //TN714 contains the value to be profiled
	Gen_Param_append_ops(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG-2, 8, res_tn, new_ops );

	// Step IV: Generate call
	// The Generated opcode is like:
	//  GTN321(b0) :- br.call TN257(p0)(enum:.sptk) (enum:.many) (enum:)
	//        (sym:__value_profile_call+0) GTN395(ar.ec) ; WN=0x809cf60
	Gen_Call_Shell_append_ops( func_name, MTYPE_V, new_ops );

	BB * bb;
	bb = Gen_BB();
	Set_BB_call(bb);
	Set_BB_profile_added(bb);
	BB_Append_Ops(bb, new_ops);
  // add annotations about callinfo 
  WN * arg1 = WN_Intconst( MTYPE_I8, 0 );
  WN * arg2 = WN_Intconst( MTYPE_I8, id );
  WN * arg3 = WN_Intconst( MTYPE_I8, 0 );
  WN* call = Gen_Call( func_name, arg1, arg2, arg3 );
  ST * call_st = WN_st( call );
  CALLINFO* call_info = TYPE_PU_ALLOC ( CALLINFO );
  CALLINFO_call_st( call_info ) = call_st;
  CALLINFO_call_wn( call_info ) = call;
  BB_Add_Annotation ( bb, ANNOT_CALLINFO, call_info );

	return bb;
}

BB * CG_VALUE_INSTRUMENT_WALKER::CG_Gen_Call(char * func_name, char * str_arg1, char * str_arg2, INT int_arg1, INT int_arg2)
{
	OPS * new_ops;
	new_ops = OPS_Create();

	OPS_Append_Ops(new_ops, Gen_Param(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG, 8, str_arg1) );
	OPS_Append_Ops(new_ops, Gen_Param(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG-1, 8, str_arg2) );
	Gen_Param_append_ops(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG-2, 8, Gen_Literal_TN(int_arg1, 8), new_ops );
	Gen_Param_append_ops(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG-3, 8, Gen_Literal_TN(int_arg2, 8), new_ops );
	Gen_Call_Shell_append_ops( func_name, MTYPE_V, new_ops );

	BB * bb;
	bb = Gen_BB();
	Set_BB_call(bb);
	Set_BB_profile_added(bb);
	BB_Append_Ops(bb, new_ops);
  // add annotations about callinfo 
  WN * arg1 = WN_Intconst( MTYPE_I8, int_arg1 );
  WN * arg2 = WN_Intconst( MTYPE_I8, int_arg2 );
  WN *str_arg_tn1 = WN_LdaString( str_arg1, 0, strlen( str_arg1 ) + 1 );
  WN *str_arg_tn2 = WN_LdaString( str_arg2, 0, strlen( str_arg2 ) + 1 );
  WN* call = Gen_Call( func_name, str_arg_tn1, str_arg_tn2 , arg1, arg2 );
  ST * call_st = WN_st( call );
  CALLINFO* call_info = TYPE_PU_ALLOC ( CALLINFO );
  CALLINFO_call_st( call_info ) = call_st;
  CALLINFO_call_wn( call_info ) = call;
  BB_Add_Annotation ( bb, ANNOT_CALLINFO, call_info );

  return bb;
}
	

BB * CG_VALUE_INSTRUMENT_WALKER::CG_Gen_Call(char * func_name, char * str_arg, INT int_arg1, INT int_arg2)
{
	OPS * new_ops;
	new_ops = OPS_Create();

	OPS_Append_Ops(new_ops, Gen_Param(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG, 8, str_arg) );
	Gen_Param_append_ops(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG-1, 8, Gen_Literal_TN(int_arg1, 8), new_ops );
	Gen_Param_append_ops(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG-2, 8, Gen_Literal_TN(int_arg2, 8), new_ops );
	Gen_Call_Shell_append_ops( func_name, MTYPE_V, new_ops );

	BB * bb;
	bb = Gen_BB();
	Set_BB_call(bb);
	Set_BB_profile_added(bb);
	BB_Append_Ops(bb, new_ops);
  // add annotations about callinfo 
  WN * arg1 = WN_Intconst( MTYPE_I8, int_arg1 );
  WN * arg2 = WN_Intconst( MTYPE_I8, int_arg2 );
  WN *str_arg_tn = WN_LdaString( str_arg, 0, strlen( str_arg ) + 1 );
  WN* call = Gen_Call( func_name, str_arg_tn , arg1, arg2 );
  ST * call_st = WN_st( call );
  CALLINFO* call_info = TYPE_PU_ALLOC ( CALLINFO );
  CALLINFO_call_st( call_info ) = call_st;
  CALLINFO_call_wn( call_info ) = call;
  BB_Add_Annotation ( bb, ANNOT_CALLINFO, call_info );

  return bb;
}

BB * CG_VALUE_INSTRUMENT_WALKER::CG_Gen_Call(char * func_name, char * str_arg, INT int_arg)
{
	OPS * new_ops;
	new_ops = OPS_Create();

	OPS_Append_Ops(new_ops, Gen_Param(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG, 8, str_arg) );
	Gen_Param_append_ops(ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG-1, 8, Gen_Literal_TN(int_arg, 8), new_ops );
	Gen_Call_Shell_append_ops( func_name, MTYPE_U8, new_ops );

	BB * bb;
	bb = Gen_BB();
	Set_BB_call(bb);
	Set_BB_profile_added(bb);
	BB_Append_Ops(bb, new_ops);
  // add annotations about callinfo 
  WN * arg = WN_Intconst( MTYPE_I8, int_arg );
  WN *str_arg_tn = WN_LdaString( str_arg, 0, strlen( str_arg ) + 1 );
  WN* call = Gen_Call( func_name, str_arg_tn , arg, MTYPE_U8 );
  ST * call_st = WN_st( call );
  CALLINFO* call_info = TYPE_PU_ALLOC ( CALLINFO );
  CALLINFO_call_st( call_info ) = call_st;
  CALLINFO_call_wn( call_info ) = call;
  BB_Add_Annotation ( bb, ANNOT_CALLINFO, call_info );

  return bb;
}

OP * CG_VALUE_INSTRUMENT_WALKER::Find_OP_to_split_entry_exit( BB *bb )
{
	OP * op;
	op = Find_OP(My_OP_to_split_entry1,bb);
	if ( op == NULL )
	{
		DevWarn("Did not find predicate register save at entry!");
		op = Find_OP(My_OP_to_split_entry2,bb);
	}
	if ( op == NULL )
	{
		DevWarn("Did not find ar.lc register save at entry!");
		op = Find_OP(My_OP_to_split_entry3,bb);
	}
	if ( op == NULL )
	{
		DevWarn("Did not find branch register save at entry!");
		op = Find_OP(My_OP_to_split_entry4,bb);
	}
	FmtAssert( op != NULL, ("Failed to find expected OP in entry_exit BB for splitting the BB!") );
	return op;
}

void CG_VALUE_INSTRUMENT_WALKER::Prepare_Insert_BBs_for_call(BB* bb)
{
   	Is_True(bb!=NULL,("Prepare_Insert_BBs_for_call:<bb> should not be NULL!"));
	Is_True(!(BB_entry(bb)&&BB_exit(bb)),("NON-Init call! Entry-BB and Exit_BB should have been seperated by init_call instru!!"));

	if ( BB_exit(bb) && BB_entry(bb) )
	{
		OP * op;
		op = Find_OP_to_split_entry_exit(bb);
		Is_True(op !=NULL, ("Prepare_Insert_BBs_for_call:Do not know where to split the BB!"));
	   	Val_Prof_Divide_BB(bb, op);
	}
}

void CG_VALUE_INSTRUMENT_WALKER::Insert_BBs_for_call(BB* bb, OPS * save_ops, BB * call_bb, OPS * restore_ops)
{
   	Is_True(call_bb !=NULL, ("Insert_BBs_for_call:<call_ops> should not be NULL!"));

	BB_Prepend_Ops(call_bb, save_ops);

	Val_Prof_Insert_BB(call_bb, bb);
//	Update_CFG_around_BB(bb, call_bb);

#ifdef GEN_NEWBB_FOR_RESTORE___Should_disable_ebo
   	BB * tmpbb;
	tmpbb = Gen_BB();
	Set_BB_profile_added(tmpbb);
	BB_Append_Ops(tmpbb, restore_ops);
	Val_Prof_Insert_BB(tmpbb, call_bb);
//	Update_CFG_around_BB(call_bb, tmpbb);
#else //now it seems this is right.
	FmtAssert(FALSE, ("Please define GEN_NEWBB_FOR_RESTORE___Should_disable_ebo")); 
	Is_True(BB_next(call_bb)!=NULL,("NULL????? should not"));
 	BB_Prepend_Ops(BB_next(call_bb), restore_ops);
//	Handle_All_Hazards(BB_next(call_bb));
#endif 
	
}

void CG_VALUE_INSTRUMENT_WALKER::Insert_BBs_for_call(BB* bb, OPS * save_ops, BB * call_bb, BB * srd_call_bb, OPS * restore_ops)
{
   	Is_True(call_bb !=NULL, ("Insert_BBs_for_call:<call_ops> should not be NULL!"));

	BB_Prepend_Ops(call_bb, save_ops);

	Val_Prof_Insert_BB(call_bb, bb);
	Val_Prof_Insert_BB(srd_call_bb, call_bb);
//	Update_CFG_around_BB(bb, call_bb);

#ifdef GEN_NEWBB_FOR_RESTORE___Should_disable_ebo
   	BB * tmpbb;
	tmpbb = Gen_BB();
	Set_BB_profile_added(tmpbb);
	BB_Append_Ops(tmpbb, restore_ops);
	Val_Prof_Insert_BB(tmpbb, srd_call_bb);
//	Update_CFG_around_BB(call_bb, tmpbb);
#else //now it seems this is right.
	FmtAssert(FALSE, ("Please define GEN_NEWBB_FOR_RESTORE___Should_disable_ebo")); 
	Is_True(BB_next(srd_call_bb)!=NULL,("NULL????? should not"));
 	BB_Prepend_Ops(BB_next(srd_call_bb), restore_ops);
//	Handle_All_Hazards(BB_next(call_bb));
#endif 
	
}

void CG_VALUE_INSTRUMENT_WALKER::Prepare_Insert_BBs_for_call_init(BB* bb)
{
   	Is_True(bb!=NULL,("Prepare_Insert_BBs_for_call_init:<bb> should not be NULL!"));

	Is_True(BB_entry(bb),("Init call should always be directly after entry BB!!"));
	
	if ( (BB_exit(bb)) || (BB_branch_op(bb)!=NULL)  || (BB_call(bb)) )
	{

            OP * op ;
	  //  op = BB_xfer_op( bb );
           // op = OP_prev(op);
	    op = Find_OP_to_split_entry_exit(bb);
	    Is_True(op !=NULL, ("Prepare_Insert_BBs_for_call_init: do not know where to split the BB"));
	    Val_Prof_Divide_BB(bb, op);
	}
}

void CG_VALUE_INSTRUMENT_WALKER::Insert_BBs_for_call_init(BB* bb, OPS * save_ops, BB * call_bb, OPS * restore_ops)
{
   	Is_True(call_bb !=NULL, ("Insert_BBs_for_call:<call_ops> should not be NULL!"));

	BB_Prepend_Ops(call_bb, save_ops);
	
	Val_Prof_Insert_BB(call_bb, bb);
//	Update_CFG_around_BB(bb, call_bb);

#ifdef GEN_NEWBB_FOR_RESTORE___Should_disable_ebo
   	BB * tmpbb;
	tmpbb = Gen_BB();
	Set_BB_profile_added(tmpbb);
	BB_Append_Ops(tmpbb, restore_ops);
	Val_Prof_Insert_BB(tmpbb, call_bb);
//	Update_CFG_around_BB(call_bb, tmpbb);
#else //now it seems this is right.
	FmtAssert(FALSE, ("Please define GEN_NEWBB_FOR_RESTORE___Should_disable_ebo")); 
	Is_True(BB_next(call_bb)!=NULL,("NULL????? should not"));
 	BB_Prepend_Ops(BB_next(call_bb), restore_ops);
#endif 
	
}

void CG_VALUE_INSTRUMENT_WALKER::Insert_BBs_for_call_init(BB* bb, OPS * save_ops, BB * call_bb, BB * srd_call_bb, OPS * restore_ops)
{
   	Is_True(call_bb !=NULL, ("Insert_BBs_for_call:<call_ops> should not be NULL!"));

	BB_Prepend_Ops(call_bb, save_ops);
	
	Val_Prof_Insert_BB(call_bb, bb);
	Val_Prof_Insert_BB(srd_call_bb, call_bb);
//	Update_CFG_around_BB(bb, call_bb);

#ifdef GEN_NEWBB_FOR_RESTORE___Should_disable_ebo
   	BB * tmpbb;
	tmpbb = Gen_BB();
	Set_BB_profile_added(tmpbb);
	BB_Append_Ops(tmpbb, restore_ops);
	Val_Prof_Insert_BB(tmpbb, srd_call_bb);
//	Update_CFG_around_BB(call_bb, tmpbb);
#else //now it seems this is right.
	FmtAssert(FALSE, ("Please define GEN_NEWBB_FOR_RESTORE___Should_disable_ebo")); 
	Is_True(BB_next(srd_call_bb)!=NULL,("NULL????? should not"));
 	BB_Prepend_Ops(BB_next(srd_call_bb), restore_ops);
#endif 
	
}


//instrument the <op> according to the information in <p>, for integer load, the profile target is the first result operand of <op>.
//if <instr_before> is true, then take the value before <op> executes to do profiling; otherwise the opsite.
void CG_VALUE_INSTRUMENT_WALKER::Do_instrument(INST_TO_PROFILE *p, BB *bb, OP * op, BOOL instr_before)
{       
	UINT32 Min_Instr_Point, Max_Instr_Point;
	Min_Instr_Point = Value_Instr_Range >> 16;
	Max_Instr_Point = Value_Instr_Range & 0xffff;
	BOOL do_value_instr = _do_value;
	if (_qulified_count < Min_Instr_Point || _qulified_count > Max_Instr_Point )
	{
		_qulified_count++;
		return;
	}
	_qulified_count++;
	if (_qulified_count > 300)
		_do_value =FALSE;
	if ( (_qulified_count > 300 ) && (_stride_instr_count >300)) 
	{
        	return;
	} 
	else if ( (!_prefetch) && (_qulified_count > 100))
	{
		if (_qulified_count % 50 == 0)
		{
			DevWarn("Too many OP(%d) qulified to instrument! exceed limit(500), so ignore!", _qulified_count);
		}
		return;
	}
	
   	Is_True (op!=NULL,("NULL op?? Can not believe!"));
   	
    OPS *save_ops, *restore_ops;
    BB * call_bb;
    BB * srd_call_bb;
    if (_trace) {
	fprintf(TFile, "Instrumentation inside BB%d\n",BB_id(bb));
    }

	//split old <bb> at <op> according to <instr_before>
    BB * newbb;
    if ( instr_before )
    {
    	if ( BB_first_op(bb) == op )
    	{
    		OP * nop_op;
    		nop_op = Mk_OP (TOP_nop_i, True_TN, Gen_Literal_TN(0, 4));;
    		BB_Prepend_Op(bb, nop_op);
    	}
    	Is_True (OP_prev(op)!=NULL,("NULL op?? Can not believe!"));
    	newbb = Val_Prof_Divide_BB(bb, OP_prev(op));
    }
    else
    {
    	newbb = Val_Prof_Divide_BB(bb, op);
    }
    Prepare_Insert_BBs_for_call(bb);
    BOOL ld_flag = _prefetch;
	switch (OP_code(op)) {
    case TOP_ld1:
    case TOP_ld4:
    case TOP_ld2:
    case TOP_ld8:
    	_ld_count++;
    	break;
    case TOP_st1:
    case TOP_st2:
    case TOP_st4:
    case TOP_st8:
    	_st_count++;
    	_prefetch = FALSE;  //prefech
    	break;
    default:
        _other_count++;   
        _prefetch = FALSE;  //prefetch
    	break;
    }

    //Generate save/restore ops, call bb (incluing parameters)
    save_ops = OPS_Create();
    restore_ops = OPS_Create();
    if(BB_call( bb )){
	//save & restore Output registers ( pseudo name: r127, r126 ... )
	Gen_Save_Restore_Output_Reg_append_ops( ISA_REGISTER_CLASS_integer, 8, save_ops, restore_ops);
	} else
		{
     BBLIST* edge = BB_succs( bb );
     INT32 bblist_len =BBlist_Len( edge );
       while ( edge != NULL && bblist_len-- )
      {
    BBLIST* nedge = edge;
    edge = BBLIST_next( edge );

    BB* target_bb = BBLIST_item( nedge );
      if(BB_call( target_bb )){
      	Gen_Save_Restore_Output_Reg_append_ops( ISA_REGISTER_CLASS_integer, 8, save_ops, restore_ops);
      break;
      	  }
        }     
       }
        //save & restore Input registers ( r32, r33 ... , r39 ) 
   	Gen_Save_Restore_Input_Reg_append_ops(ISA_REGISTER_CLASS_integer, 8, save_ops, restore_ops);
	//save & restore Return address register ( b0 ) 
	if( !IPFEC_Enable_Edge_Profile )
	Gen_Save_Restore_Return_Addr_Reg_append_ops(save_ops, restore_ops);
	//save & restore GP register ( gp ) 
        Gen_Save_Restore_GP_Reg_append_ops(save_ops, restore_ops);
	//save & restore Return value register ( r8-r11, f8-f15 ) 
	//Gen_Save_Restore_Return_Value_Reg_append_ops(save_ops, restore_ops);
        TN * profile_target_tn;
        TN * prefetch_target_tn;
	profile_target_tn = Find_original_TN( save_ops, OP_result(op, 0) );
	if (profile_target_tn == NULL)
	    profile_target_tn = OP_result(op,0);
	if (_prefetch){  
	    prefetch_target_tn = Find_original_TN( save_ops, OP_opnd(op, 3) );
		if (prefetch_target_tn == NULL)
		    prefetch_target_tn = OP_opnd(op, 3); //prefetch
		}
	//Other information attached to OP 
	OP_flags_val_prof(op) = VAL_PROF_FLAG;
	OP_val_prof_id(op) = _instrument_count;
	if (_prefetch){
	    OP_flags_srd_prof(op) = SRD_PROF_FLAG;
        OP_srd_prof_id(op) = _ld_count;     
	    }
	OP_exec_count(op) = -1;

   	call_bb = CG_Gen_Call(INVOKE_VALUE_INSTRUMENT_NAME, _pu_hdr_return_tn, _instrument_count, profile_target_tn);
   	_instrument_count ++;
   	Is_True(call_bb != NULL, ("Failed to generate call BB during instrument"));
   	if (_prefetch){
   	   _stride_instr_count++;
   	   srd_call_bb = CG_Gen_Call(INVOKE_STRIDE_INSTRUMENT_NAME, _pu_hdr_return_tn, _stride_instr_count, prefetch_target_tn);
   	   Is_True(srd_call_bb != NULL, ("Failed to generate stride call BB during instrument"));
       if (!_do_value)
           Insert_BBs_for_call(bb, save_ops, srd_call_bb, restore_ops);
   	   else
   	       Insert_BBs_for_call(bb, save_ops, call_bb, srd_call_bb, restore_ops);
   		}
    else		
    Insert_BBs_for_call(bb, save_ops, call_bb, restore_ops);
    _prefetch = ld_flag ;
    _do_value = do_value_instr;
}

//traverse the whole <bb>, examine each OP in <bb>, if one op should be instrumented, do it.
void CG_VALUE_INSTRUMENT_WALKER::BB_Walk(BB *bb) 
{

    if(!BB_nest_level(bb))        
        return;
    INST2PROFLIST::iterator i;
   	OP *op, *tmp_op;
    struct INST_TO_PROFILE *p;
	for (op = BB_first_op(bb); op != NULL; op = tmp_op) 
	{
   		tmp_op = OP_next(op);
    	//check if the same instruction to instru
	    for (i = inst2prof_list.begin(); i != inst2prof_list.end(); i++) 
    	{
	    	p = *i;
	    	if ( OP_code(op) == p->Opcode() )
	    	{
	 			//because BB will be splitted during instrumentation, so 
	 			//we can not assume <op> belongs to <bb> anymore
	    		Do_instrument(p,OP_bb(op),op,p->Is_instr_before());
	 			break;
	    	}
    	}
    }
}



//traverse the whole CFG, instrument all BBs, also add initialization code for instrumentation.
//Actually this functions do the complete work of instrumentation we intend to do.
void CG_VALUE_INSTRUMENT_WALKER::CFG_Walk(CGRIN *rin)
{
    //request output stack registers for the purpose of pass parameters.
    Request_Output_Registers();

	_pu_hdr_return_tn = Gen_Register_TN( ISA_REGISTER_CLASS_integer, 8 );
	Set_TN_is_global_reg( _pu_hdr_return_tn );
    //traverse all BBs in current PU, process them one by one
   	BB * bb, * tmp_bb;
        BB* lbb;
 	for (bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) 
 	{
 		if(!BB_profile_added(bb) && !BB_profile_changed(bb) && !BB_profile_splitted(bb) )
 		bb->id_before_profile = BB_id(bb);
        }
 	for (bb = REGION_First_BB; bb != NULL; bb = tmp_bb) 
         {
		tmp_bb = BB_next(bb); 
		Is_True(!BB_profile_changed(bb), ("profile changed bb appears in later traverse!"));
		if (BB_entry(bb) || BB_exit(bb))
		{
			DevWarn("Value profile: Entry/exit bb, ignored!");
			continue;
		}
		if (BB_scheduled(bb))
		{
			DevWarn("BB is scheduled by SWP/CFLOW/LOCS already, ignored!");
			continue;
		}
		if (BB_profile_added(bb))
		{
#ifdef VALUE_PROFILE_INTERNAL_DEBUG		
			DevWarn("BB is added by other profiler, ignored!");
#endif
			continue;
		}
		if (BB_scheduled_hbs(bb))
		{
			DevWarn("BB is scheduled by hbs already, ignored!");
			continue;
		}
		if (BB_recovery(bb))
		{
			DevWarn("BB is scheduled by speculation, ignored to walk around bugs in speculation!");
			continue;
		}
    	BB_Walk(bb); 
    }

#ifdef	VALUE_PROFILE_DETAIL
	DevWarn(" %d OPs is instrumented.\n",_instrument_count);
#endif
	Is_True(_instrument_count>=0, ("instrument count is negative!"));
	if (_instrument_count == 0)
	{
#ifdef	VALUE_PROFILE_DETAIL
		DevWarn("No OP need instrumentation in current PU. Skip initialization!\n");
#endif
		return;
	}
		
	//NOTE: First traverse the CFG and do instrumentation, then change Entry_BB_Head. 
	//So as to avoid instrumentation for new inserted instructions.
	//Another reason is: after instrumentaion, the _instrument_count, _ld_count and _st_count are known.
	//In other words, the Value table size is know. This information is used during running time to alloc mem for value table
	
    //Do initianlization work in each of the Entry_BB
   	BB_LIST * bblist;
    for (bblist = Entry_BB_Head; bblist; bblist = BB_LIST_rest(bblist) )
    {
    	BB * bb = BB_LIST_first(bblist);
		Prepare_Insert_BBs_for_call_init(bb);
		
    	// Insert the call bb into current Entry_bb to do global initialization work for the whole program && for this PU.
    	OPS * save_ops, *restore_ops;
    	BB * call_bb;
    	BB * srd_call_bb;
	save_ops = OPS_Create();
	restore_ops = OPS_Create();
	//save & restore Output registers ( pseudo name: r127, r126 ... )
	Gen_Save_Restore_Output_Reg_append_ops( ISA_REGISTER_CLASS_integer, 8, save_ops, restore_ops);
	//save & restore Input registers ( r32, r33 ... , r39 ) 
    	Gen_Save_Restore_Input_Reg_append_ops(ISA_REGISTER_CLASS_integer, 8, save_ops, restore_ops);
        //save & restore Return address register ( b0 ) 
    	Gen_Save_Restore_Return_Addr_Reg_append_ops(save_ops, restore_ops);
	//save & restore GP register ( gp ) 
	Gen_Save_Restore_GP_Reg_append_ops(save_ops, restore_ops);
	//save & restore Return value register ( r8-r11, f8-f15 ) 
        Gen_Save_Restore_Return_Value_Reg_append_ops(save_ops, restore_ops);
	Gen_Call_Init_Return_ops(restore_ops);
	call_bb = CG_Gen_Call(INVOKE_VALUE_INSTRUMENT_INIT_NAME, Value_Instru_File_Name, get_cat_str(Src_File_Name, Cur_PU_Name, _mempool), _phase, _instrument_count);
        if (_prefetch)
        {   
            srd_call_bb = CG_Gen_Call(INVOKE_STRIDE_INSTRUMENT_INIT_NAME, Stride_Instru_File_Name, get_cat_str(Src_File_Name, Cur_PU_Name, _mempool), _phase, _stride_instr_count);
          
            if (!_do_value)
            	{
                Insert_BBs_for_call_init(bb, save_ops, srd_call_bb, restore_ops);
            	}
            else 
                Insert_BBs_for_call_init(bb, save_ops, call_bb, srd_call_bb, restore_ops);
 	 }
 	 else
 	    Insert_BBs_for_call_init(bb, save_ops, call_bb, restore_ops);
    } 
    Adjust_Save_Restore_ARPFS();
   	PU_Has_Calls = TRUE;
    //Now the modification work of CGIR finished, it is the time to update the liveness information 
#ifndef _delete_but_not_fully_tested_yet
    GRA_LIVE_Recalc_Liveness(NULL);
#endif
}

void CG_VALUE_INSTRUMENT_WALKER::Request_Output_Registers()
{
  REGISTER_Allocate_Stacked_Register( ABI_PROPERTY_caller,
                  ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG );
  REGISTER_Allocate_Stacked_Register( ABI_PROPERTY_caller,
                  ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG - 1 );
  REGISTER_Allocate_Stacked_Register( ABI_PROPERTY_caller,
                  ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG - 2 );  
  REGISTER_Allocate_Stacked_Register( ABI_PROPERTY_caller,
                  ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG - 3 );  
  REGISTER_Allocate_Stacked_Register( ABI_PROPERTY_caller,
                  ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG - 4 );  
  REGISTER_Allocate_Stacked_Register( ABI_PROPERTY_caller,
                  ISA_REGISTER_CLASS_integer, FIRST_OUTPUT_REG - 5 );  
}

//external interface,  perform instrumentation 
void CG_VALUE_Instrument(CGRIN *rin, PROFILE_PHASE phase,BOOL stride_profiling_flag, BOOL do_value)
{

  // Create and initialize local memory pool
  MEM_POOL local_mempool;
  MEM_POOL_Initialize( &local_mempool, "CG_VALUE_INSTRUMENT_WALKER_Pool", FALSE );
  MEM_POOL_Push( &local_mempool );
  {
	if(Instrumentation_File_Name != NULL)
	{
    	Value_Instru_File_Name = Instrumentation_File_Name;
    	Stride_Instru_File_Name = Instrumentation_File_Name;
    }
	if ( Value_Instru_File_Name == NULL )
	{
		DevWarn( "not specify the feedback file!use default feedback file\n" );
		Value_Instru_File_Name = Src_File_Name;
	}
    if ( Stride_Instru_File_Name == NULL )
	{
		DevWarn( "not specify the feedback file!use default feedback file\n" );
		Stride_Instru_File_Name = Src_File_Name;
	}

    // Walk the CFG -- instrument
    FREQ_Compute_BB_Frequencies( );
    CG_VALUE_INSTRUMENT_WALKER cg_value_instrument_walker(&local_mempool,0,0,0,phase,stride_profiling_flag, do_value);
    cg_value_instrument_walker.CFG_Walk(rin);

  }
  // Dispose of local memory pool
  MEM_POOL_Pop( &local_mempool );
  MEM_POOL_Delete( &local_mempool );
}

//annotation part start here


class CG_VALUE_ANNOTATE_WALKER {
private:
    MEM_POOL * _mempool;	// the local memory pool
    PROFILE_PHASE _phase; //in which phase the instrumentation occurs
    PU_PROFILE_HANDLES _fb_handles; //feedback info here.
	PU_PROFILE_HANDLE _fb_handle_merged; //merged feedback info here.
    UINT32 _instrument_count; //how many OP really instrumented
    UINT32 _stride_count;       //how many OP really stride instrumented
    UINT32 _annotation_count;  //how many OP really annotated
    UINT32 _stride_annotation_count; //how many OP really stride annontated
    UINT32 _qulified_count;  //how many OP is qualified to annotation
    UINT32 _srd_qulified_count;  //how many OP is qualified to stride annotation
    UINT32 _pu_inconsistent_count;  //how many OP whose execution count != BB_freq
//    static UINT32 _file_inconsistent_count; // how many OP whose execution count != BB_freq in the whole file
   
public:
	CG_VALUE_ANNOTATE_WALKER(MEM_POOL *m, PROFILE_PHASE phase, PU_PROFILE_HANDLES fb_handles)
		:_mempool(m),_phase(phase),_fb_handles(fb_handles),_fb_handle_merged(NULL),
		_instrument_count(0), _stride_count(0), _annotation_count(0), _stride_annotation_count(0), _qulified_count(0), _srd_qulified_count(0), _pu_inconsistent_count(0)
	{
	}
	~CG_VALUE_ANNOTATE_WALKER(){}

	void CFG_Walk(CGRIN * rin);

protected:
	void Merge_feedback_data();
    void Merge_stride_feedback_data();
	void BB_Walk(BB *bb);
	void Do_annotation(BB* bb, OP * op);
	void Do_stride_annotation(BB* bb, OP * op);
	
};

void CG_VALUE_ANNOTATE_WALKER::Merge_feedback_data()
{
	if (_fb_handle_merged)
	{
		DevWarn("value feecback data already merged!\n");
		return;
	}
	if (_fb_handles.size() == 0)
	{
		DevWarn("no feedback data for current PU.\n");
		_fb_handle_merged = NULL;
		return; 
	}

	//Now merge the data together.
	_fb_handle_merged = CXX_NEW(PU_Profile_Handle(NULL, 0), _mempool);
	FB_Value_Vector & fb_merged_value_vector = _fb_handle_merged->Get_Value_Table();
	INT fb_handle_num;
	fb_handle_num = _fb_handles.size();
	PU_PROFILE_HANDLE the_largest_fb;
	PU_PROFILE_ITERATOR pu_prof_itr = _fb_handles.begin();
	the_largest_fb = *pu_prof_itr;
	_instrument_count = the_largest_fb->Get_Value_Table().size();
	for (pu_prof_itr= ( _fb_handles.begin() ); pu_prof_itr != _fb_handles.end (); ++pu_prof_itr)
	{
    	PU_Profile_Handle * handle=*pu_prof_itr;
    	if ( _instrument_count != handle->Value_Profile_Table.size() )
    		DevWarn("Value_Profile_Table.size() differ in feedback files!");
    	if ( _instrument_count < handle->Value_Profile_Table.size() )
    	{
    		the_largest_fb = handle;
    		_instrument_count = the_largest_fb->Value_Profile_Table.size();
    	}
	}

	fb_merged_value_vector.resize(_instrument_count);
		if (_instrument_count==0)
		{
		return;
		}

	UINT64 * values = TYPE_MEM_POOL_ALLOC_N(UINT64, _mempool, TNV_N*fb_handle_num);
	UINT64 * counters = TYPE_MEM_POOL_ALLOC_N(UINT64, _mempool, TNV_N*fb_handle_num);
	INT i, j, k, m, n;
	for ( i=0; i<_instrument_count; i++ )
	{
		memset(values, 0, TNV_N*fb_handle_num*sizeof(UINT64));
		memset(counters, 0, TNV_N*fb_handle_num*sizeof(UINT64));
		INT32 cur_id = the_largest_fb->Value_Profile_Table[i].tnv._id;
		UINT64 cur_exec_counter = 0;
		INT cur_flag = the_largest_fb->Value_Profile_Table[i].tnv._flag;
		for (pu_prof_itr = _fb_handles.begin(); pu_prof_itr != _fb_handles.end (); ++pu_prof_itr)
		{
			if ( i >= (*pu_prof_itr)->Get_Value_Table().size() )
				continue;
			FB_Info_Value & fb_info_value = Get_Value_Profile( *pu_prof_itr, i );
			if((fb_info_value.tnv._id != NULL) && (cur_id!=NULL))
                                Is_True(fb_info_value.tnv._id == cur_id,("_id not consitent between feedback files"));
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

void CG_VALUE_ANNOTATE_WALKER::Merge_stride_feedback_data()
{
    
	if (_fb_handles.size() == 0)
	{
		DevWarn("no feedback data for current PU.\n");
		_fb_handle_merged = NULL;
		return; 
	}

	//Now merge the stride data together.
	FB_Value_Vector & fb_merged_stride_vector = _fb_handle_merged->Get_Stride_Table();
	INT fb_handle_num;
	fb_handle_num = _fb_handles.size();
	PU_PROFILE_HANDLE the_largest_fb;
	PU_PROFILE_ITERATOR pu_prof_itr = _fb_handles.begin();
	the_largest_fb = *pu_prof_itr;
	_stride_count = the_largest_fb->Get_Stride_Table().size();
	for (pu_prof_itr= ( _fb_handles.begin() ); pu_prof_itr != _fb_handles.end (); ++pu_prof_itr)
	{
    	PU_Profile_Handle * handle=*pu_prof_itr;
    	if ( _stride_count != handle->Stride_Profile_Table.size() )
    		DevWarn("Stride_Profile_Table.size() differ in feedback files!");
    	if ( _stride_count < handle->Stride_Profile_Table.size() )
    	{
    		the_largest_fb = handle;
    		_stride_count = the_largest_fb->Stride_Profile_Table.size();
    	}
	}

	fb_merged_stride_vector.resize(_stride_count);
		if (_stride_count==0)
		{
		return;
		}

	UINT64 * values = TYPE_MEM_POOL_ALLOC_N(UINT64, _mempool, TNV_N*fb_handle_num);
	UINT64 * counters = TYPE_MEM_POOL_ALLOC_N(UINT64, _mempool, TNV_N*fb_handle_num);
	INT i, j, k, m, n;
	for ( i=0; i<_stride_count; i++ )
	{
		memset(values, 0, TNV_N*fb_handle_num*sizeof(UINT64));
		memset(counters, 0, TNV_N*fb_handle_num*sizeof(UINT64));
		INT32 cur_id = the_largest_fb->Stride_Profile_Table[i].tnv._id;
		UINT64 cur_exec_counter = 0;
		UINT64 cur_zeroes_counter = 0;
		INT cur_flag = the_largest_fb->Stride_Profile_Table[i].tnv._flag;
		for (pu_prof_itr = _fb_handles.begin(); pu_prof_itr != _fb_handles.end (); ++pu_prof_itr)
		{
			if ( i >= (*pu_prof_itr)->Get_Stride_Table().size() )
				continue;
			FB_Info_Value & fb_info_stride = Get_Stride_Profile( *pu_prof_itr, i );
                        if((fb_info_stride.tnv._id != NULL) && (cur_id!=NULL))
			        Is_True(fb_info_stride.tnv._id == cur_id,("_id not consitent between feedback files"));
			cur_exec_counter += fb_info_stride.tnv._exec_counter;
			cur_zeroes_counter += fb_info_stride.tnv._zero_std_counter;
			Is_True(fb_info_stride.tnv._flag == cur_flag,("_flag not consitent between feedback files"));
			for ( j=0; j<TNV_N; j++ )
			{
				if ( fb_info_stride.tnv._counters[j] == 0 )
					break;
				for ( m=0;m<TNV_N*fb_handle_num;m++)
				{
					if (counters[m] == 0)
					{
						values[m] = fb_info_stride.tnv._values[j];
						counters[m] += fb_info_stride.tnv._counters[j];
						break;
					}
					else if ( fb_info_stride.tnv._values[j] == values[m] )
					{
						counters[m] += fb_info_stride.tnv._counters[j];
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
		fb_merged_stride_vector[i].tnv._id = cur_id;
		fb_merged_stride_vector[i].tnv._exec_counter = cur_exec_counter;
		fb_merged_stride_vector[i].tnv._zero_std_counter = cur_zeroes_counter;
		fb_merged_stride_vector[i].tnv._flag = cur_flag;
		for ( m=0; m<TNV_N; m++ )
		{
			fb_merged_stride_vector[i].tnv._values[m] = values[m];
			fb_merged_stride_vector[i].tnv._counters[m] = counters[m];
		}
	}
}

void CG_VALUE_ANNOTATE_WALKER::CFG_Walk(CGRIN *rin)
{
	Is_True( _fb_handle_merged == NULL, ("_fb_handle_merged != NULL before merge feedback data.") );
	Merge_feedback_data();
	Merge_stride_feedback_data();
	if (_fb_handle_merged == NULL)
	{
		DevWarn("There is no feedback data.\n");
	   // OP_MAP_Delete(op_stride_tnv_map);
		return;
	}
    //traverse all BBs in current PU, process them one by one
   	BB * bb, * tmp_bb;
 	for (bb = REGION_First_BB; bb != NULL; bb = tmp_bb) 
 	{
		tmp_bb = BB_next(bb); 
		Is_True(!BB_profile_changed(bb), ("profile changed bb appears in later traverse!"));
		if (BB_entry(bb) || BB_exit(bb))
		{
			DevWarn("Value profile: Entry/exit bb, ignored!");
			continue;
		}
		if (BB_scheduled(bb))
		{
			DevWarn("BB is scheduled by SWP/CFLOW/LOCS already, ignored!");
			continue;
		}
		if (BB_profile_added(bb))
		{
#ifdef VALUE_PROFILE_INTERNAL_DEBUG		
			DevWarn("BB is added by other profiler, ignored!");
#endif
			continue;
		}
		if (BB_scheduled_hbs(bb))
		{
			DevWarn("BB is scheduled by hbs already, ignored!");
			continue;
		}

             BB_Walk(bb);
    }

 	Is_True(_annotation_count == _instrument_count, ("_annotation_count != _instrument_count _annotation_count= %d _instrument_count=%d",_annotation_count,_instrument_count));

    if ( Get_Trace( TP_A_PROF,TT_PROF_FEEDBACK_DUMP ) )
    {
    	fprintf(TFile,"================================================\n");
    	fprintf(TFile, "Value profile annotation information for PU: %s \n", Cur_PU_Name);
    	fprintf(TFile," %d OP annotated. (already merged if there are multi files):\n", _annotation_count);
    	fprintf(TFile,"--------------------------------------------------------------\n");
    	for ( INT i=0; i<_instrument_count; i++)
    	{
			FB_Value_Vector& value_table = _fb_handle_merged->Get_Value_Table();
			value_table[i].tnv.Print(TFile);
    	}
    	fprintf(TFile,"\n================================================\n");
    }

}

//traverse the whole <bb>, examine each OP in <bb>.
void CG_VALUE_ANNOTATE_WALKER::BB_Walk(BB *bb) 
{
    if(!BB_nest_level(bb))
       return;
    INST2PROFLIST::iterator i;
   	OP *op, *tmp_op;
    struct INST_TO_PROFILE *p;
	for (op = BB_first_op(bb); op != NULL; op = tmp_op) 
	{
   		tmp_op = OP_next(op);
    	//check if the same instruction to instru
	    for (i = inst2prof_list.begin(); i != inst2prof_list.end(); i++) 
    	{
	    	p = *i;
	    	if ( OP_code(op) == p->Opcode() )
	    	{
	 			//because BB will be splitted during instrumentation, so 
	 			//we can not assume <op> belongs to <bb> anymore
	    		Do_annotation(OP_bb(op),op);
	 		    switch (OP_code(op)) {
                    case TOP_ld1:
                    case TOP_ld4:
                    case TOP_ld2:
                    case TOP_ld8:
                    Do_stride_annotation(OP_bb(op),op);
                    default:
                       ; //nothing need to do
	 			}
            	break;
	    	}
    	}
    }
}

void CG_VALUE_ANNOTATE_WALKER::Do_annotation(BB* bb, OP * op) 
{
	FB_Value_Vector& value_table = _fb_handle_merged->Get_Value_Table();
	_qulified_count ++;
	Is_True(_instrument_count == _fb_handle_merged->Get_Value_Table().size(), ("_instrument_count != merged value table."));
	if (_qulified_count > _instrument_count)
		return;

	OP_flags_val_prof(op) = VAL_PROF_FLAG;
	OP_val_prof_id(op) = _annotation_count;
	OP_exec_count(op) = value_table[_annotation_count].tnv._exec_counter;
	OP_MAP_Set( op_tnv_map, op, &(value_table[_annotation_count].tnv));
	if ( 1 )
	{
		float op_exec_count = (float)OP_exec_count(op);
		fprintf(TFile, "BBid=%d, OP_exec_count(op)=%llu(%f), bb->freq=%f (%s) \n", 
					BB_id(bb), OP_exec_count(op), op_exec_count, bb->freq, (BB_freq_fb_based(bb)?"feedback":"heuristic") );
		if (op_exec_count != bb->freq)
		{
			_pu_inconsistent_count++;
			DevWarn("ERROR: BB frequency != op exec count(%f). Total unmatched = %u", 
				op_exec_count, _pu_inconsistent_count);
//			Is_True(0,("ERROR: BB frequency != op exec count. Total unmatched = %u", _pu_inconsistent_count));
		}
	}
	this->_annotation_count++;
}

void CG_VALUE_ANNOTATE_WALKER::Do_stride_annotation(BB* bb, OP * op) 
{
	FB_Value_Vector& stride_table = _fb_handle_merged->Get_Stride_Table();
	_srd_qulified_count++;
	Is_True(_stride_count == _fb_handle_merged->Get_Stride_Table().size(), ("_stride_count != merged stride table."));
	if (_qulified_count > _stride_count)
		return;

	OP_flags_srd_prof(op) |= SRD_PROF_FLAG;
	OP_srd_prof_id(op) = _stride_annotation_count;
	OP_exec_count(op) = stride_table[_stride_annotation_count].tnv._exec_counter;
	OP_MAP_Set( op_stride_tnv_map, op, &(stride_table[_stride_annotation_count].tnv));
	this->_stride_annotation_count++;
}

void CG_VALUE_Annotate(CGRIN * rin, PROFILE_PHASE phase)
{
  // Create and initialize local memory pool
  MEM_POOL local_mempool;
  op_tnv_map = OP_MAP_Create();
  op_stride_tnv_map = OP_MAP_Create();
  MEM_POOL_Initialize( &local_mempool, "CG_VALUE_INSTRUMENT_WALKER_Pool", FALSE );
  MEM_POOL_Push( &local_mempool );
  {
  	PU_PROFILE_HANDLES fb_handles
      = Get_CG_PU_Value_Profile( get_cat_str(Src_File_Name, Cur_PU_Name, &local_mempool), Feedback_File_Info[phase]);

  	if( Get_Trace( TP_A_PROF,TT_PROF_FEEDBACK_DUMP ) )
  	{
    	Dump_Fb_Data( fb_handles );
  	}
  	
    CG_VALUE_ANNOTATE_WALKER cg_value_annotate_walker(&local_mempool,phase,fb_handles);
	// Walk the CFG -- do annotation
    cg_value_annotate_walker.CFG_Walk(rin);
  }
  // Dispose of local memory pool
  MEM_POOL_Pop( &local_mempool );
  MEM_POOL_Delete( &local_mempool );
}
#endif
