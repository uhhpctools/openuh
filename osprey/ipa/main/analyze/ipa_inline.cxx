/*
 * Copyright 2003, 2004, 2005 PathScale, Inc.  All Rights Reserved.
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
// ====================================================================
// ====================================================================
//
// Module: ipa_inline.cxx
// $Source: /proj/osprey/CVS/open64/osprey1.0/ipa/main/analyze/ipa_inline.cxx,v $
//
// Revision history:
//  16-Nov-96 - Original Version
//
// Description:
//
// Inlining analysis.
//
// ====================================================================
// ====================================================================

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "defs.h"
#include "tracing.h"			// trace flags
#include "errors.h"			// error handling

#include "ipa_option.h"			// ipa options

#include "ipa_inline.h"
#include "ipa_nested_pu.h"
#include "ipa_summary.h"
#include "ipc_symtab_merge.h"		// IPC_GLOBAL_IDX_MAP
#if 0
#include "ipo_defs.h"			// IPA_NODE_CONTEXT
#endif

#include "symtab_access.h"

//^INLINING_TUNING
#define MINI_APPLICATION	10000
#define LARGE_APPLICATION	118000   //for GAP benchmark

#define MINI_HOTNESS_THRESHOLD		1
#define MEDIAN_HOTNESS_THRESHOLD	10
#define LARGE_HOTNESS_THRESHOLD		120
//INLINING_TUNIN$
#define TINY_SIZE 10

INT Total_Prog_Size = 0;	// Size of the final program
INT Total_Inlined = 0;
INT Total_Not_Inlined = 0;
static UINT32 Max_Total_Prog_Size; // max. program size allowed
static INT Real_Orig_Prog_Weight; // Orig_Prog_Weight - dead code
static UINT32 non_aggr_callee_limit;
static UINT32 Real_Orig_WN_Count; // Orig_Prog_Weight - dead code

FILE *N_inlining;
FILE *Y_inlining;
FILE *e_weight;
FILE *Verbose_inlining;

#define BASETYPE TY_mtype

static OPCODE OPC_UNKNOWN = (OPCODE)0;

static
OPCODE Stid_Opcode [MTYPE_LAST + 1] = {
  OPC_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPC_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPC_I1STID,     /* MTYPE_I1 */
  OPC_I2STID,     /* MTYPE_I2 */
  OPC_I4STID,     /* MTYPE_I4 */
  OPC_I8STID,     /* MTYPE_I8 */
  OPC_U1STID,     /* MTYPE_U1 */
  OPC_U2STID,     /* MTYPE_U2 */
  OPC_U4STID,     /* MTYPE_U4 */
  OPC_U8STID,     /* MTYPE_U8 */
  OPC_F4STID,     /* MTYPE_F4 */
  OPC_F8STID,     /* MTYPE_F8 */
#ifdef TARG_IA64
  OPC_F10STID,    /* MTYPE_F10 */
  OPC_F16STID,    /* MTYPE_F16 */
#elif TARG_X8664
  OPC_UNKNOWN,    /* MTYPE_F10 */ 
  OPC_UNKNOWN,    /* MTYPE_F16 */ 
#endif
  OPC_UNKNOWN,    /* MTYPE_STR */
  OPC_FQSTID,     /* MTYPE_FQ */
  OPC_UNKNOWN,    /* MTYPE_M */
  OPC_C4STID,     /* MTYPE_C4 */
  OPC_C8STID,     /* MTYPE_C8 */
  OPC_CQSTID,     /* MTYPE_CQ */
  OPC_UNKNOWN     /* MTYPE_V */
#ifdef KEY
  ,OPC_BSSTID,    /* MTYPE_BS */
  OPC_A4STID,     /* MTYPE_A4 */
  OPC_A8STID,     /* MTYPE_A8 */
  OPC_UNKNOWN,    /* MTYPE_C10 */
  OPC_UNKNOWN,    /* MTYPE_C16 */
  OPC_UNKNOWN,    /* MTYPE_I16 */
  OPC_UNKNOWN     /* MTYPE_U16 */
#ifdef TARG_X8664
  ,OPC_V16C4STID, /* MTYPE_V16C4 */
  OPC_V16C8STID,  /* MTYPE_V16C8 */
  OPC_V16I1STID,  /* MTYPE_V16I1 */
  OPC_V16I2STID,  /* MTYPE_V16I2 */
  OPC_V16I4STID,  /* MTYPE_V16I4 */
  OPC_V16I8STID,  /* MTYPE_V16I8 */
  OPC_V16F4STID,  /* MTYPE_V16F4 */
  OPC_V16F8STID,  /* MTYPE_V16F8 */
  OPC_V8I1STID,   /* MTYPE_V8I1 */
  OPC_V8I2STID,   /* MTYPE_V8I2 */
  OPC_V8I4STID,   /* MTYPE_V8I4 */
  OPC_V8F4STID,   /* MTYPE_V8F4 */
  OPC_M8I1STID,   /* MTYPE_M8I1 */
  OPC_M8I2STID,   /* MTYPE_M8I2 */
  OPC_M8I4STID,   /* MTYPE_M8I4 */
  OPC_M8F4STID    /* MTYPE_M8F4 */
#endif // TARG_X8664
#endif // KEY
};

#ifdef TODO
// IPA Feedback - need inline/noinline identifiers
extern IPA_FEEDBACK_STRINGS * IPA_Fbk_Strings;
#endif

// ====================================================================
//
// Report_Reason / Report_Limit_Reason
//
// We're not inlining a call.  Report why.  For Report_Reason, the
// parameter reason is a simple string.  For Report_Limit_Reason, it
// is a printf format string for printing one (limit1) or two (limit1,
// limit2) integer limits.
//
// WARNING:  The DEMANGLE routine always returns the same static
// buffer, so the caller and callee names must not be live
// simultaneously.
//
// ====================================================================

void
Report_Reason (const IPA_NODE *callee, const IPA_NODE *caller,
	       const char *reason , const IPA_EDGE *edge)
{
  INT32 IPA_idx = 0;

  if ( ! ( INLINE_List_Actions || Trace_IPA || Trace_Perf ) ) return;

  char *callee_name = DEMANGLE (callee->Name());

  if ( INLINE_List_Actions ) {
    fprintf ( stderr, "%s not inlined into ", callee_name );
  }
  if ( Trace_IPA || Trace_Perf ) {
    fprintf ( TFile, "%s not inlined into ", callee_name );
  }

  char *caller_name = DEMANGLE (caller->Name());
  INT32 caller_index = caller->Node_Index();

  if ( INLINE_List_Actions ) {
    fprintf ( stderr, "%s", caller_name );
    if ( IPA_Skip_Report ) {
      fprintf ( stderr, " (%d)", caller_index );
    }
    fprintf ( stderr, ": %s    (edge# %d) \n", reason, edge->Edge_Index ()  );
  }
  if ( Trace_IPA || Trace_Perf ) {
    fprintf ( TFile, "%s", caller_name );
    if ( IPA_Skip_Report ) {
      fprintf ( TFile, " (%d)", caller_index );
    }
    fprintf ( TFile, ": %s    (edge# %d) \n", reason, edge->Edge_Index ()  );
    fflush ( TFile );
  }
if(Get_Trace ( TP_IPA, IPA_TRACE_TUNING))
{
  	
 	fprintf(N_inlining, "Report_Reason:[%s] not inlined into [%s]  (edge# %d):\n", callee_name, caller_name,edge->Edge_Index () ); 
	fprintf ( N_inlining, "\t {reason: %s}\n", reason  );
}
#ifdef TODO
#ifndef _STANDALONE_INLINER
  if( IPA_Enable_Feedback ) {
    /* need pragma plus the reason */
    IPA_idx = IPA_Fbk_Strings->Emit_id_string( callee_name);
    if ( IPA_idx > 0 ) {
      fprintf(IPA_Feedback_prg_fd, 
              "#pragma noinline %s  /* why: %s */\n", callee_name, reason );
    }
  } /* if (IPA_Enable_Feedback) */
#endif  // _STANDALONE_INLINER
#endif // TODO
}

void
Report_Limit_Reason (const IPA_NODE *callee, const IPA_NODE *caller,
		     const IPA_EDGE *edge,
		     const char *reason, float limit1, float limit2)
{
  INT32 IPA_idx = 0;
  if ( ! ( INLINE_List_Actions || Trace_IPA || Trace_Perf ) ) return;

  char *callee_name = DEMANGLE (callee->Name());

  if ( INLINE_List_Actions ) {
    fprintf ( stderr, "%s not inlined into ", callee_name );
  }
  if ( Trace_IPA || Trace_Perf ) {
    fprintf ( TFile, "%s not inlined into ", callee_name );
  }
  char *caller_name = DEMANGLE (caller->Name());
  INT32 caller_index = caller->Node_Index();

  if ( INLINE_List_Actions ) {
    fprintf ( stderr, "%s", caller_name );
    if ( IPA_Skip_Report ) {
      fprintf ( stderr, " (%d): ", caller_index );
    } else {
      fprintf ( stderr, ": " );
    }
    fprintf ( stderr, reason, limit1, limit2 );
    fprintf ( stderr, "   (edge# %d)\n" , edge->Edge_Index () );
  }
  if ( Trace_IPA || Trace_Perf ) {
    fprintf ( TFile, "%s", caller_name );
    if ( IPA_Skip_Report ) {
      fprintf ( TFile, " (%d): ", caller_index );
    } else {
      fprintf ( TFile, ": " );
    }
    fprintf ( TFile, reason, limit1, limit2 );
    fprintf ( TFile, "   (edge# %d)\n", edge->Edge_Index ()  );
    fflush ( TFile );
  }
if(Get_Trace ( TP_IPA, IPA_TRACE_TUNING))
{
	fprintf ( N_inlining, "Report_Limit_Reason: [%s] not inlIned into [%s]  (edge# %d):\n", callee_name, caller_name, edge->Edge_Index () );
	fprintf(N_inlining, "\t{reason: " );
	fprintf ( N_inlining, reason, limit1, limit2 );
	fprintf(N_inlining, "}\n");
}
#ifdef TODO
#ifndef _STANDALONE_INLINER
  if( IPA_Enable_Feedback ) {
    /* need pragma plus the reason */
    IPA_idx = IPA_Fbk_Strings->Emit_id_string( callee_name);
    if ( IPA_idx > 0 ) {
      fprintf(IPA_Feedback_prg_fd, 
              "#pragma noinline %s  /* why: ", callee_name );
      fprintf(IPA_Feedback_prg_fd, reason, limit1, limit2 );
      fprintf(IPA_Feedback_prg_fd, " */\n"); 

    }
  } /* if( IPA_Enable_Feedback ) */
#endif  // _STANDALONE_INLINER
#endif // TODO
}

/* the combined weight after inlining two procedures */
/* we need to subtract the bb count and call count by 1 to reflect the
   removal of a call.  We add the number of copy stmt created for copying
   the formal parameters */
UINT32
Get_combined_weight (PU_SIZE s1, PU_SIZE s2, IPA_NODE *callee)
{
    s1 += s2;
    /* 1 less bb and 1 less callfrom removing the call, add copy stmt for
       formals */ 
#ifdef KEY
    s1.Inc_PU_Size (-1, 0, -1);
#else
    s1.Inc_PU_Size (-1, callee->Num_Formals(), -1);
#endif
    return s1.Weight ();
}

UINT32
Get_combined_olimit (PU_SIZE s1, PU_SIZE s2, IPA_NODE *callee)
{
    s1 += s2;
    /* 1 less bb and 1 less callfrom removing the call, add copy stmt for
       formals */ 
#ifdef KEY
    s1.Inc_PU_Size (-1, 0, -1);
#else
    s1.Inc_PU_Size (-1, callee->Num_Formals(), -1);
#endif
    return s1.Olimit ();
}



#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))

typedef AUX_IPA_NODE<UINT32> INLINE_COUNTER_ARRAY;

// For each PU, keep track of the number of calls (to this PU) that are
// inlined.  If all calls to this PU are inlined, the PU might be deleted.
static INLINE_COUNTER_ARRAY* inline_count;

static inline BOOL
All_Calls_Inlined (const IPA_NODE* node, const IPA_CALL_GRAPH* cg)
{
    Is_True (IPA_Enable_DFE, ("Expecting -IPA:dfe=on"));

    return cg->Num_In_Edges (node) == (*inline_count)[node];
}

#endif // (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))

static void
Init_inline_parameters (void)
{
    UINT64 bloat_size;
    UINT64 current_size = Real_Orig_Prog_Weight =
	MIN (Total_Prog_Size, Orig_Prog_Weight);

	Real_Orig_WN_Count = Orig_Prog_WN_Count; //INLINING_TUNING

#ifdef KEY
    if (INLINE_Ignore_Bloat && ! IPA_Bloat_Factor_Set)
      IPA_Bloat_Factor = UINT32_MAX;
#endif // KEY

    bloat_size = current_size * (UINT64) IPA_Bloat_Factor;

    if (bloat_size > UINT32_MAX || IPA_Bloat_Factor == UINT32_MAX)
	Max_Total_Prog_Size = UINT32_MAX; // possible mult overflow
    else
	Max_Total_Prog_Size = current_size + bloat_size / 100;

    non_aggr_callee_limit = IPA_PU_Minimum_Size + (IPA_PU_Minimum_Size / 2);

    //Adaptive inlining here, pengzhao
    if( IPA_Min_Hotness == 10) //DEFAULT_MIN_HOTNESS
    {
      if(Real_Orig_Prog_Weight< MINI_APPLICATION)
      {
	    IPA_Min_Hotness = MINI_HOTNESS_THRESHOLD;	
      }else if(Real_Orig_Prog_Weight< LARGE_APPLICATION)
      {
	    IPA_Min_Hotness = MEDIAN_HOTNESS_THRESHOLD;	
      }else
      {
	    IPA_Min_Hotness = LARGE_HOTNESS_THRESHOLD;	
      }
    }

    if (Total_cycle_count.Known() && (Trace_IPA || Trace_Perf|| Get_Trace(TP_IPA, IPA_TRACE_TUNING_NEW))) {
	fprintf (TFile, "\tTotal number of calls = ");
        Total_call_freq.Print(TFile);
	fprintf (TFile, "\n\tTotal cycle count = ");
        Total_cycle_count.Print(TFile);
	fprintf (TFile, "\n");
// INLINING_TUNING^
	fprintf(TFile, "Total cycle_count_2 = "); 
		Total_cycle_count_2.Print(TFile); 
	fprintf(TFile, "\nTotal WN_count = %d",Orig_Prog_WN_Count); 
	fprintf (TFile, "\n");
//INLINING_TUNING$
	fprintf(TFile, "\t Real_Orig_Prog_Weight=%d\n",Real_Orig_Prog_Weight);
	fprintf(TFile, "\t current_size=%lld\n",current_size);
	fprintf(TFile, "\t Max_Total_Prog_Size=%d\n",Max_Total_Prog_Size);
    }

    if (Trace_IPA || Get_Trace(TP_IPA, IPA_TRACE_TUNING_NEW)) {
	fprintf(TFile, "Bloat factor = %u%% \n",  IPA_Bloat_Factor);
	fprintf(TFile, "PU Limit = %u \n", IPA_PU_Limit);
	fprintf(TFile, "PU Hard Limit = %u\n", IPA_PU_Hard_Limit);
	fprintf(TFile, "Depth Level = %u \n", IPA_Max_Depth);
	if (IPA_Bloat_Factor_Set)
	    fprintf(TFile, "Bloat Factor set = TRUE \n");
	else
	    fprintf(TFile, "Bloat Factor set = FALSE \n");

	if (IPA_PU_Limit_Set)
	    fprintf(TFile, "PU Limit Set = TRUE \n");
	else
	    fprintf(TFile, "PU Limit Set = FALSE \n");

	fprintf(TFile, "IPA_PU_Minimum_Size = %d\n",IPA_PU_Minimum_Size );
	fprintf(TFile, "non_aggr_callee_limit = %d\n",non_aggr_callee_limit );
	fprintf(TFile, "IPA_Min_Hotness = %d\n",IPA_Min_Hotness );
    }

} // Init_inline_parameters


#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
static void
Update_Total_Prog_Size (const IPA_NODE *caller, IPA_NODE *callee,
			const IPA_CALL_GRAPH *cg)
{
    ++((*inline_count)[callee]);

    if (IPA_Enable_Cloning && caller->Is_Clone_Candidate()) {
	callee->Set_Undeletable ();
	return;
    }
	
    if (! callee->Is_Undeletable () &&
	! callee->Should_Be_Skipped() &&
	All_Calls_Inlined (callee, cg) &&
	! callee->Is_Externally_Callable ()) {

	callee->Set_Deletable ();
	Total_Prog_Size -= callee->Weight();
    }
} // Update_Total_Prog_Size
#endif // (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))


float
compute_hotness (IPA_EDGE *edge, IPA_NODE *callee, INT callee_size)
{
    FB_FREQ cycle_ratio = (edge->Get_frequency () / callee->Get_frequency () *
                           callee->Get_cycle_count ()) / Total_cycle_count;
    float cycle_ratio_float = cycle_ratio.Value();
    float size_ratio = (float) callee_size / (float) Real_Orig_Prog_Weight;
    float result_float = (cycle_ratio_float / size_ratio * 100.0);
    return (result_float);
}

UINT32 // KEY
Effective_weight (const IPA_NODE* node)  {
#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
    if (IPA_Use_Effective_Size && node->Has_frequency ()) {
	SUMMARY_FEEDBACK *fb = node->Get_feedback ();
	return PU_Weight (fb->Get_effective_bb_count (),
			  fb->Get_effective_stmt_count (),
			  node->PU_Size().Call_Count ());
    } else
#endif // _STANDALONE_INLINER
	return node->Weight ();
}

#ifdef KEY
void inline_do_it (IPA_EDGE * ed, IPA_NODE * caller, IPA_NODE * callee,
                   const IPA_CALL_GRAPH * cg)
{
    UINT32 caller_weight = caller->Weight();
    UINT32 combined_weight = Get_combined_weight (caller->PU_Size(),
                                                  callee->PU_Size(),
						  callee);
    if (Trace_IPA || Trace_Perf) {
        UINT32 callee_weight = Effective_weight (callee);
	fprintf (stderr, "%s inlined into %s", callee->Name(),caller->Name());
	fprintf (TFile, "%s inlined into ", DEMANGLE (callee->Name()));
	fprintf (TFile, "%s (size: %d + %d = %d)   (edge# %d) \n", DEMANGLE (caller->Name()), callee_weight, caller_weight, combined_weight, ed->Edge_Index());
    }
    
#ifdef TODO
    if( IPA_Enable_Feedback ) {
	/* check for cross-file inlining */
	if( callee->File_Index() !=  caller->File_Index()) {
	    (callee->Get_fbk_ptr() )->Set_Cross_File_Fnd();
	}
    }
#endif
    
    Total_Prog_Size += (combined_weight - caller_weight);
    caller->UpdateSize (callee, ed);
    
#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
    if (IPA_Enable_DFE)
	Update_Total_Prog_Size (caller, callee, cg);
#endif // _STANDALONE_INLINER
    
    if (callee->Summary_Proc()->Has_var_dim_array()) {  // propagate the bit up
	caller->Summary_Proc()->Set_has_var_dim_array();
    }
}

// Return true if a callee (actually any callee) can be inlined into
// "node" without checking for additional constraints.
static BOOL
trivially_ok_to_inline (const IPA_NODE * node, const IPA_CALL_GRAPH * cg)
{
  Is_True (node->Summary_Proc()->Get_bb_count() > 0,
           ("How can there be a call without any BB?"));

  // An approximate estimate to decide if the only thing "node" does is
  // call another function -- a popular category of functions in C++.
  return node->Summary_Proc()->Get_callsite_count() == 1 &&
         cg->Num_Out_Edges (node) == 1 &&
         node->Summary_Proc()->Get_bb_count() == 1;
}
#endif // KEY

static BOOL
check_size_and_freq (IPA_EDGE *ed, IPA_NODE *caller,
		     IPA_NODE *callee, const IPA_CALL_GRAPH *cg)
{
    BOOL inline_it = FALSE;
    char reason[300] = "{reason: ";
    char tmp_decision[300] = "";
    char tmp_reason[100]="";
    INT32 IPA_idx = 0;
    UINT32 caller_weight = caller->Weight ();
    UINT32 callee_weight = Effective_weight (callee);
    UINT32 combined_weight = Get_combined_weight (caller->PU_Size(), callee->PU_Size(), callee);

    if (PU_is_operator(callee->Get_PU()) && ed->Num_Actuals() <= 2)
      combined_weight = 1;

#ifdef KEY
    float hotness = callee->Get_feedback() == NULL ? 0.0 :
      compute_hotness (ed, callee, callee_weight);       
#else
    float  hotness = compute_hotness (ed, callee, callee_weight); 
#endif

    //pengzhao
    if(Get_Trace ( TP_IPA, IPA_TRACE_TUNING)) {
	
	SUMMARY_FEEDBACK *fb = callee->Get_feedback();
	INT e_bb_cnt, e_stmt_cnt;
	e_bb_cnt= e_stmt_cnt = -1;
	
	if(callee->Has_frequency ()) {
	    e_bb_cnt = (fb==NULL)? unsigned(-1) : fb->Get_effective_bb_count ();
	    e_stmt_cnt = (fb==NULL)? unsigned(-1) : fb->Get_effective_stmt_count ();
	}
	fprintf(e_weight, "%-8d%-8d%-8d%-8d%-8d%-8d%-8d%s \n", e_bb_cnt, e_stmt_cnt, callee->PU_Size().Call_Count (),callee_weight, callee->PU_Size().Bb_count(), callee->PU_Size().Stmt_count(),callee->Weight() , callee->Name() );
    }

     if ( caller->Summary_Proc()->Is_Never_Invoked() == FALSE && 
         callee->Summary_Proc()->Is_Never_Invoked() == FALSE) {

        if (callee_weight <= TINY_SIZE
#ifdef KEY
	    || (callee_weight < INLINE_Callee_Limit && PU_is_marked_inline(callee->Get_PU()))
	    || trivially_ok_to_inline (caller, cg) // ok to inline into caller?
#endif
	   )
	{
#ifdef KEY
	    // don't use goto
	    inline_do_it(ed, caller, callee, cg);
	    return TRUE;
#else
            goto inline_do_it;
#endif
        }             
     } 
    
    if (IPA_Force_Depth_Set) {
	if (!callee->Has_Noinline_Attrib() && cg->Node_Depth(callee) <= IPA_Force_Depth
#ifdef TODO
	&& !ed->IsICall()
#endif
	) {
	    if ( Trace_IPA || Trace_Perf ) {
		fprintf ( TFile, "%s inlined into ", DEMANGLE (callee->Name()) );
		fprintf (TFile, "%s:  because of force depth = (%d)\n", DEMANGLE (caller->Name()), IPA_Force_Depth);
	    }
	    
	    //pengzhao
	    if (Get_Trace ( TP_IPA, IPA_TRACE_TUNING)) {
		inline_it = TRUE;
		sprintf ( tmp_decision, "*[%s] will be Inlined into [%s] (edge=%d)", DEMANGLE (callee->Name()),DEMANGLE (caller->Name()),ed->Edge_Index() );
		sprintf(tmp_reason, " because of force depth = (%d)}",IPA_Force_Depth); 
		strcat (reason, tmp_reason);
	    }
	    
	    if ( INLINE_List_Actions ) {
		fprintf ( stderr, "%s inlined into ", DEMANGLE (callee->Name()) );
		fprintf ( stderr, "%s: because of force depth =  (%d)\n", DEMANGLE (caller->Name()), IPA_Force_Depth );
		Total_Prog_Size += (combined_weight - caller_weight);
		caller->UpdateSize (callee, ed);
#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
		if (IPA_Enable_DFE)
		    Update_Total_Prog_Size (caller, callee, cg);
#endif // _STANDALONE_INLINER
	    }
	    ed->Set_Must_Inline_Attrib();
	    
	    if (callee->Summary_Proc()->Has_var_dim_array()) {  // propagate the bit up
		caller->Summary_Proc()->Set_has_var_dim_array();
	    }
	    return TRUE;
	}
    }
    
    // We must inline (regardless of size considerations) if edge is
    // set inline (by a pragma) or if callee is set inline (by being on a
    // must list, or by default)
    if (!ed->Has_Must_Inline_Attrib() && !callee->Has_Must_Inline_Attrib() && !INLINE_All
#if (defined(_STANDALONE_INLINER) || defined(_LIGHTWEIGHT_INLINER))
	// callee has __inline set
	&& (!callee->Summary_Proc()->Is_must_inline())
#endif // _STANDALONE_INLINER
    ) {
	
	if (Total_Prog_Size >= Max_Total_Prog_Size) {
#ifndef KEY
	    static BOOL reported = FALSE;
	    
	    if ( ! reported ) {
#endif // !KEY
		if ( Trace_IPA || Trace_Perf ) {
		    fprintf ( TFile, "Inlining stopped because total " "program size limit exceeded\n" );
		}
#ifdef KEY
                Report_Reason (callee, caller, 
		               "Total program size limit exceeded", ed);
#else
		if ( INLINE_List_Actions ) {
		    fprintf ( stderr, "Inlining stopped because total " "program size limit exceeded\n" );
		}
		reported = TRUE;
	    }
#endif
	    return FALSE;
	}// if (Total_Prog_Size >= Max_Total_Prog_Size) 
	
	INT loopnest = ed->Summary_Callsite ()->Get_loopnest ();
	
	if (Trace_IPA) {
	    fprintf (TFile, "\tcaller: %s (%u) loopnest = %u", DEMANGLE (caller->Name()), caller_weight, loopnest); 
	    fprintf (TFile, ", callee: %s ", DEMANGLE (callee->Name()));
	    if (callee->PU_Size().Call_Count () == 0)
		fprintf (TFile, "(leaf) ");
	    fprintf (TFile, "(%u)\n", callee_weight);
	}
	
	if (ed->Has_frequency ()) {
#ifdef KEY
	    if(ed->Get_frequency ().Value() == 0.0f)
#else
	    if(ed->Get_frequency ()._value == 0.0f)
#endif
	    {
		ed->Set_reason_id(32);
		Report_Reason (callee, caller, "Edge is never invoked", ed);
		return FALSE;
	    }
	}
	
	if ( caller->Summary_Proc()->Is_Never_Invoked() ) {// there is no fdbk info for this edge
	    ed->Set_reason_id(32);
	    Report_Reason (callee, caller, "Edge is never invoked", ed);
	    return FALSE;
	}
	
	if ( callee->Summary_Proc()->Is_Never_Invoked() ) {// there is no fdbk info for this edge
	    ed->Set_reason_id(32);
	    Report_Reason (callee, caller, "Edge is never invoked", ed);
	    return FALSE;
	}

	if (callee_weight >= non_aggr_callee_limit && 
	    hotness < (float)IPA_Min_Hotness &&
	    ed->Summary_Callsite ()->Is_in_case_clause ()) {
	    ed->Set_reason_id(34);
	    Report_Reason (callee, caller, "Infrequent callee in switch statements", ed);
	    return FALSE;
	}

        if (callee_weight > IPA_PU_Minimum_Size) {
            if (combined_weight > IPA_PU_Limit) {
		ed->Set_reason_id(26);
		ed->Set_reason_data((float)combined_weight);
		Report_Limit_Reason (callee, caller, ed, "combined size(%f) exceeds -IPA:plimit=%f", 
	                             combined_weight,IPA_PU_Limit);
		return FALSE;
#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
	    } 
	    
	    if (ed->Has_frequency () && callee->Has_frequency () && ed->Get_frequency().Known() && callee->Get_frequency ().Known()) {
		float min_hotness = (float)IPA_Min_Hotness;
		
		//following codes deal with the PUs that are invoked 
		//infrequently but contain very hot codes (i.e. loops etc)
		float density = (float) callee->Get_cycle_count().Value() / ((float)callee_weight * (float)callee->Get_frequency().Value());
		
		// if there is only one callsite, we will inline it anyway
		if ( cg->Num_In_Edges(callee) > 1) {
		    if(hotness < min_hotness) {
                        ed->Set_reason_id(27);
                        ed->Set_reason_data(hotness);
                        Report_Limit_Reason (callee, caller, ed, "hotness (%f) < -IPA:min_hotness (%.1f)", hotness, min_hotness);

                        // Add the vector registration here
                        // if we have more budget, we will lower the hotness threshhold
                        return FALSE;
                    }
                    if(density > IPA_Max_Density) {
                        ed->Set_reason_id(33);
                        ed->Set_reason_data(density);
			// KEY: use %f for IPA_Max_Density
                        Report_Limit_Reason (callee, caller, ed, "Density (%f) > Max_density (%f)", density, (float)IPA_Max_Density);
                        return FALSE;
		    }
		}else{
		    if(hotness < (min_hotness/2)) {
			ed->Set_reason_id(27);
			ed->Set_reason_data(hotness);
			Report_Limit_Reason (callee, caller, ed, "hotness (%f) < -IPA:min_hotness (%.1f)", hotness, min_hotness);
			return FALSE;
		    }
		}
	    } else if ( callee->Summary_Proc()->Is_Never_Invoked() ) { // there is no fdbk info for this edge
		ed->Set_reason_id(32);
		Report_Reason (callee, caller, "Edge is never invoked", ed);
		return FALSE;
#endif // _STANDALONE_INLINER
	    }else{ // 1.
#ifndef PATHSCALE_MERGE_ZHC
                if (callee_weight > IPA_Small_Callee_Limit && cg->Num_In_Edges(callee) > 2) {
#else
		if (callee_weight > IPA_Small_Callee_Limit && cg->Num_In_Edges(callee) > 1) {
#endif
                    /* We try to screen out callees that are too large, but
                     * accept those that have only one caller:
                     */
                    if (loopnest == 0 || callee->PU_Size().Call_Count () > 0) {
                        ed->Set_reason_id(28);
                        ed->Set_reason_data((float)callee_weight);
                        Report_Limit_Reason (callee, caller, ed, "callee size" " (%f) > -IPA:callee_limit=%f", callee_weight, IPA_Small_Callee_Limit ); 
                        return FALSE;
                    }
                }

#ifndef PATHSCALE_MERGE_ZHC
                if (!INLINE_Aggressive && loopnest == 0 && callee->PU_Size().Call_Count() > 1 && 
                    callee_weight > non_aggr_callee_limit) {
#else
		if (!INLINE_Aggressive && loopnest == 0 && callee->PU_Size().Call_Count() > 0 && callee_weight > non_aggr_callee_limit) {
#endif
                    /* Less aggressive inlining: don't inline unless it is
                     * either small, leaf, or called from a loop. 
                     */
                    ed->Set_reason_id(29);
                    ed->Set_reason_data((float)callee_weight);
                    Report_Limit_Reason (callee, caller, ed, "callee_size (%.1f) > -INLINE:aggressive=off callee limit (%.1f)", callee_weight, non_aggr_callee_limit );
                    return FALSE;
                }
	    }// 1.
	} else {
	    if (combined_weight > IPA_PU_Hard_Limit) {
		ed->Set_reason_id(30);
		ed->Set_reason_data((float)combined_weight);
		Report_Limit_Reason ( callee, caller, ed, "small, but size (%f) " "exceeds hard function size limit (%f)", combined_weight, IPA_PU_Hard_Limit );
		return FALSE;
	    } else {
		// inlining since pu size is less than minimum pu size
		
#ifdef TODO
		if (ed->IsICall () && (IPA_Enable_Cloning && callee->Is_Clone_Candidate()))
		// until cprop handles indirect call, we can never
		// inline a cloned PU
		    callee->Clear_Clone_Candidate ();
#endif
		
		if ( Trace_IPA || Trace_Perf ) {
		    fprintf ( TFile, "%s inlined into ", DEMANGLE (callee->Name()) );
		    fprintf (TFile, "%s: forced because of small size (%d)  (edge# %d)\n", DEMANGLE (caller->Name()), callee_weight, ed->Edge_Index() );
		}
		
		//pengzhao
		if (Get_Trace ( TP_IPA, IPA_TRACE_TUNING)) {
		    inline_it = TRUE;
		    sprintf ( tmp_decision, "*[%s] will be Inlined into [%s] (edge=%d): ", DEMANGLE (callee->Name()) ,DEMANGLE (caller->Name()),ed->Edge_Index() );
		    sprintf(tmp_reason, " forced because of small size (%d) ",callee_weight);
		    strcat (reason, tmp_reason);
		}
		
		if ( INLINE_List_Actions ) {
		    fprintf ( stderr, "%s inlined into ", DEMANGLE (callee->Name()) );
		    fprintf ( stderr, "%s: forced because of small size (%d)  (edge# %d)\n", DEMANGLE (caller->Name()), callee_weight, ed->Edge_Index() );
		}
	    }
	}
	
	if (Get_combined_olimit (caller->PU_Size(), callee->PU_Size(), callee) > Olimit) {
	    ed->Set_reason_id(31);
	    ed->Set_reason_data((float)Get_combined_olimit (caller->PU_Size(), callee->PU_Size(), callee));
	    Report_Limit_Reason (callee, caller, ed, "Olimit (%f) exceeds -OPT:Olimit=%f", Get_combined_olimit (caller->PU_Size(), callee->PU_Size(), callee), Olimit );
	    return FALSE;
	}

#ifdef KEY
	if (IPA_Enable_Branch_Heuristic)
	{
	    // ********** REMOVE THIS ************* 
	    if (getenv ("branch"))
	    	IPA_Min_Branch_Prob = atof (getenv("branch"));
	    float branch_p = ed->Summary_Callsite()->Get_probability();
	    if (branch_p >= 0 && branch_p < IPA_Min_Branch_Prob)
	    {
	    	fprintf (stderr, "%s not inlined into %s because branch probability %f < minimum probability %f\n", callee->Name(), caller->Name(), branch_p, IPA_Min_Branch_Prob);
		return FALSE;
	    }
	}
#endif
    }//
    
#ifdef TODO
    if (ed->IsICall () && (IPA_Enable_Cloning && callee->Is_Clone_Candidate())) {
	// if we decide to clone, we cannot inline
	Report_Reason (callee, caller, "cannot inline indirect call to cloned PU", ed);
	return FALSE;
    }
#endif
    
    /* Finally, we decide to inline this call */
#ifdef KEY
    inline_do_it (ed, caller, callee, cg);
#else
    inline_do_it: if (Trace_IPA || Trace_Perf) {
	fprintf (stderr, "%s inlined into %s", callee->Name(),caller->Name());
	fprintf (TFile, "%s inlined into ", DEMANGLE (callee->Name()));
	fprintf (TFile, "%s (size: %d + %d = %d)   (edge# %d) \n", DEMANGLE (caller->Name()), callee_weight, caller_weight, combined_weight, ed->Edge_Index());
    }
    
    //pengzhao
    if(Get_Trace ( TP_IPA, IPA_TRACE_TUNING)) {
	if (inline_it==FALSE)
	    sprintf (tmp_decision, "*[%s] will be Inlined into [%s] (edge=%d)", DEMANGLE (callee->Name()), DEMANGLE (caller->Name()),ed->Edge_Index() );
	sprintf(tmp_reason, " and the limits donot filter it out},(size: %d + %d = %d) ",callee_weight, caller_weight, combined_weight );
	strcat (reason, tmp_reason);
	strcat (reason, "\n");
	fprintf(Y_inlining, tmp_decision);
	fprintf(Y_inlining, reason);
    }
    
#ifdef TODO
    if( IPA_Enable_Feedback ) {
	/* check for cross-file inlining */
	if( callee->File_Index() !=  caller->File_Index()) {
	    (callee->Get_fbk_ptr() )->Set_Cross_File_Fnd();
	}
    }
#endif
    
    Total_Prog_Size += (combined_weight - caller_weight);
    caller->UpdateSize (callee, ed);
    
#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
    if (IPA_Enable_DFE)
	Update_Total_Prog_Size (caller, callee, cg);
#endif // _STANDALONE_INLINER
    
    if (callee->Summary_Proc()->Has_var_dim_array()) {  // propagate the bit up
	caller->Summary_Proc()->Set_has_var_dim_array();
    }
#endif // KEY
    return TRUE;
} // check-size-and-freq

//--------------------------------------------------------------------------
// check if return types are okay
//--------------------------------------------------------------------------
static BOOL
return_types_are_compatible (IPA_NODE* callee_node, IPA_EDGE *ed)
{
    if (ed->Summary_Callsite()->Get_return_type() == MTYPE_V)
        return TRUE;                    // caller ignoring the return type

    ST* callee = callee_node->Func_ST();

    Is_True (ST_sym_class (callee) == CLASS_FUNC,
             ("Expecting a function ST"));

    TY_IDX ty_idx = ST_pu_type (callee);
    TY& ty = Ty_Table[ty_idx];

    Is_True (TY_kind (ty) == KIND_FUNCTION, ("Expecting a function ST")
);

    TY_IDX ret_ty_idx = TYLIST_type (Tylist_Table[TY_tylist (ty)]);

    if (ret_ty_idx == 0)
        return FALSE;

    TY& ret_ty = Ty_Table[ret_ty_idx];

    if (TY_kind (ret_ty) == KIND_VOID)
        return FALSE;
    else {
        TYPE_ID callee_mtype = BASETYPE(ret_ty);
        TYPE_ID caller_mtype = ed->Summary_Callsite()->Get_return_type();

        // check if types are the same or the return type is
        // a structure return

        if ((callee_mtype == caller_mtype) || (callee_mtype == MTYPE_M)
)
            return TRUE;
    }

    return FALSE;
} // return_types_are_compatible

static TY_IDX
base_type_of_array(TY_IDX array_type)
{
    if (TY_kind(TY_AR_etype(array_type)) != KIND_ARRAY)
	return (TY_AR_etype(array_type));
    else
	return (base_type_of_array(TY_AR_etype(array_type)));
}

//--------------------------------------------------------------------------
// check if the types are compatible. If they are then we are fine,
// else we don't inline
//--------------------------------------------------------------------------
static BOOL
types_are_compatible (TY_IDX ty_actual, TY_IDX ty_formal, BOOL lang)
{

    // if it is not a value parameter then check to see
    // if it is of type KIND_SCALAR, if true then return
    // FALSE

    TYPE_ID formal_element_size, actual_element_size;

    if ((ty_actual == 0) || (ty_formal == 0))
	return FALSE;		// No type info

    if (lang) {
        BOOL formal_is_array, actual_is_array;
    
        if (TY_kind(ty_formal) == KIND_POINTER) {
    	    ty_formal = TY_pointed(ty_formal);
    	    formal_is_array = (TY_kind(ty_formal) == KIND_ARRAY);
	}
        else
    	    formal_is_array = (TY_kind(ty_formal) == KIND_ARRAY);
    
        if (TY_kind(ty_actual) == KIND_POINTER) {
    	    ty_actual = TY_pointed(ty_actual);
    	    actual_is_array = (TY_kind(ty_actual) == KIND_ARRAY);
        }
        else
    	    actual_is_array = (TY_kind(ty_actual) == KIND_ARRAY);
    
#ifdef KEY
	UINT64 formal_type_size, actual_type_size;

	if (INLINE_Check_Compatibility == RELAXED_CHECK)
	{
	  formal_type_size = TY_size (ty_formal);
	  actual_type_size = TY_size (ty_actual);

	  if (!actual_is_array && formal_is_array && 
	      (!formal_type_size /* variable-sized formal array */ ||
	      actual_type_size == formal_type_size))
	  {
	    // check if actual is a complex and formal is array of 2 floats
	    TY_IDX formal = base_type_of_array (ty_formal);
	    TYPE_ID actual_type = BASETYPE (ty_actual);
	    TYPE_ID formal_type = BASETYPE (formal);
	    if ((actual_type == MTYPE_C10 && formal_type == MTYPE_F10) ||
		(actual_type == MTYPE_C8 && formal_type == MTYPE_F8) ||
		(actual_type == MTYPE_C4 && formal_type == MTYPE_F4))
	      return TRUE;

	    // Actual can be complex, and formal can be array of 1 formal or
	    // of variable size where size should be 1.
	    if ((actual_type == MTYPE_C10 && formal_type == MTYPE_C10) ||
		(actual_type == MTYPE_C8 && formal_type == MTYPE_C8) ||
		(actual_type == MTYPE_C4 && formal_type == MTYPE_C4))
	      return TRUE;
	  }
	}
#endif // KEY

        // PV 374125, don't inline in this case
        // where one of the parameters is an array and the
        // other is a scalar
        if ((!actual_is_array && formal_is_array))
    	    return FALSE;
    
        if (actual_is_array) {
	    ty_actual = base_type_of_array(ty_actual);
    	    actual_element_size = BASETYPE(ty_actual);
	}
        else 
    	    actual_element_size = BASETYPE(ty_actual);
    
        if (formal_is_array)  {
	    ty_formal = base_type_of_array(ty_formal);
    	    formal_element_size = BASETYPE(ty_formal);
	}
        else 
    	    formal_element_size = BASETYPE(ty_formal);
        
        if (formal_element_size == actual_element_size)
    	    return TRUE;

#ifdef KEY
	if (INLINE_Check_Compatibility == RELAXED_CHECK)
	{
	  if (actual_is_array && formal_is_array &&
	      ((actual_element_size==MTYPE_C10 &&
		formal_element_size==MTYPE_F10) ||
	       (actual_element_size==MTYPE_C8 &&
		formal_element_size==MTYPE_F8) ||
	       (actual_element_size==MTYPE_C4 && 
	        formal_element_size==MTYPE_F4)))
	  {
	    if (!formal_type_size || !actual_type_size ||
	        formal_type_size == actual_type_size)
	      return TRUE;
	  }
	  // the other way
	  if (actual_is_array && formal_is_array &&
	      ((actual_element_size==MTYPE_F10 && 
	        formal_element_size==MTYPE_C10) ||
	       (actual_element_size==MTYPE_F8 && 
	        formal_element_size==MTYPE_C8) ||
	       (actual_element_size==MTYPE_F4 && 
	        formal_element_size==MTYPE_C4)))
	  {
	    if (!formal_type_size || !actual_type_size ||
	        formal_type_size == actual_type_size)
	      return TRUE;
	  }
	}
#endif // KEY

#if 0
        else
    	    return FALSE;
#endif
    }
#if 0
    else { 	
#endif
	
        TYPE_ID desc = BASETYPE(ty_formal);
	if ((desc == 0) && (TY_kind (ty_formal) == KIND_FUNCTION)) {
    	    TY_IDX ret_ty_idx = TYLIST_type (Tylist_Table[TY_tylist (ty_formal)]);

    	    if (ret_ty_idx == 0)
        	return FALSE;
	    else
		desc = BASETYPE(ret_ty_idx);
	}

	if (desc == 0)
	    return FALSE;	// Don't know what the basetype is

        OPCODE stid  = Stid_Opcode[desc];

        if (desc == MTYPE_M)
	    /* we just check the size of the formal and actual */
	    return (TY_size(ty_formal) == TY_size(ty_actual));
    
        if (stid == OPC_UNKNOWN)
	    return FALSE;

	TYPE_ID rtype = BASETYPE(ty_actual);
	TYPE_ID ltype = OPCODE_desc(stid);
#ifdef KEY
        if (INLINE_Type_Mismatch || IPO_Types_Are_Compatible(ltype, rtype))
#else
        if (IPO_Types_Are_Compatible(ltype, rtype))
#endif
	    return TRUE;
#if 0
    }
#endif

    return FALSE;
}

//--------------------------------------------------------------------------
// check if return types are okay
//--------------------------------------------------------------------------
static BOOL
param_types_are_compatible (IPA_NODE* caller_node, IPA_NODE* callee_node, IPA_EDGE *ed)
{
#ifdef KEY
    // num_formals is actually the lesser of the # of actual parameters and
    // # of formal parameters.
    INT num_formals = ed->Num_Actuals() < callee_node->Num_Formals() ? 
    			ed->Num_Actuals() : callee_node->Num_Formals();
#else
    INT num_formals = callee_node->Num_Formals();
#endif

    if (!num_formals) // No types to check
	return TRUE;

    SUMMARY_FORMAL* callee_formal = IPA_get_formal_array(callee_node);
    SUMMARY_ACTUAL* call_actual = IPA_get_actual_array(caller_node);
    SUMMARY_SYMBOL* caller_symbols = IPA_get_symbol_array(caller_node);
    SUMMARY_SYMBOL* callee_symbols = IPA_get_symbol_array(callee_node);

    SUMMARY_ACTUAL *actuals = &call_actual[ed->Summary_Callsite()->Get_actual_index()];
    SUMMARY_FORMAL *formals = &callee_formal[callee_node->Summary_Proc()->Get_formal_index()];

#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
    const IPC_GLOBAL_IDX_MAP* callee_idx_maps = IP_FILE_HDR_idx_maps(callee_node->File_Header());
    const IPC_GLOBAL_IDX_MAP* caller_idx_maps = IP_FILE_HDR_idx_maps(caller_node->File_Header());
    Is_True(callee_idx_maps && caller_idx_maps, ("idx_maps for caller and callee are not set up\n"));
#endif // _STANDALONE_INLINER

    BOOL lang = ((callee_node->Summary_Proc()->Get_lang() == LANG_F77) || 
    		(callee_node->Summary_Proc()->Get_lang() == LANG_F90));

#if 0
    IPA_NODE_CONTEXT context(callee_node);
#endif

    for (INT i=0; i<num_formals; ++i) {
	TY_IDX ty_formal = formals[i].Get_ty();
	TY_IDX ty_actual = actuals[i].Get_ty();

	if (!IPA_Enable_Inline_Char_Array) {
            if (((TY_kind(ty_formal) == KIND_SCALAR) && 
			(formals[i].Is_ref_parm() || lang)) &&
	    		actuals[i].Is_value_parm())
	        return FALSE;   // formal is decl. as a scalar formal reference parm and 
			        // actual is passed-by-value, don't match
	}

	if (!IPA_Enable_Inline_Var_Dim_Array && formals[i].Is_var_dim_array())
	    return FALSE;	// Don't inline var-dim array

        // turn off inlining when the formal is of
        // sclass: scalar_formal_ref and its kind is STRUCT
        // We can only inline scalar or array FORMAL_REFS
        if (formals[i].Is_ref_parm() && (TY_kind(ty_formal) == KIND_STRUCT)) {
	    if (!IPA_Enable_Inline_Struct) 
                return FALSE;
	    else {
		if (TY_kind(ty_actual) == KIND_POINTER) {
    	    	    if (TY_kind(TY_pointed(ty_actual)) == KIND_ARRAY) 
		  	if (!IPA_Enable_Inline_Struct_Array_Actual)
			    return FALSE;
                }
                else {
    	            if (TY_kind(ty_actual) == KIND_ARRAY)
		  	if (!IPA_Enable_Inline_Struct_Array_Actual)
			    return FALSE;
	        }
	    }
	}

	SUMMARY_SYMBOL* s = callee_symbols + formals[i].Get_symbol_index();

	if (actuals[i].Get_symbol_index () >= 0) {
            SUMMARY_SYMBOL* caller_sym =
            	caller_symbols + actuals[i].Get_symbol_index ();

            if (s->Is_addr_f90_target () != caller_sym->Is_addr_f90_target ())
            	return FALSE;
	} else if (s->Is_addr_f90_target ())
	    return FALSE;

	if (IPA_Enable_Inline_Optional_Arg && s->Is_optional() &&
		(ty_actual == 0))  // Skip over optional argument
	    continue;

	if (!types_are_compatible(ty_actual, ty_formal, lang))
	    return FALSE;
    }
    return TRUE;
}

void
IPA_NODE::UpdateSize (IPA_NODE *callee, IPA_EDGE *ed)
{
    _pu_size += callee->PU_Size();
#ifdef KEY
    _pu_size.Inc_PU_Size (-1, 0, -1);
#else
    _pu_size.Inc_PU_Size (-1, callee->Num_Formals(), -1);
#endif

#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
    if (IPA_Use_Effective_Size && Has_frequency() &&
	callee->Has_frequency () && ed->Summary_Callsite()->Get_frequency_count().Known()) {
	SUMMARY_FEEDBACK *fb = Get_feedback ();
	SUMMARY_FEEDBACK *callee_fb = callee->Get_feedback ();
	fb->Inc_effective_bb_count (callee_fb->Get_effective_bb_count () - 1);
	fb->Inc_effective_stmt_count (callee_fb->Get_effective_stmt_count () + 
				      callee->Num_Formals());
	fb->Inc_cycle_count ((ed->Get_frequency () / callee->Get_frequency ()) * callee_fb->Get_cycle_count ());
    }
#endif // _STANDALONE_INLINER
    
} // IPA_NODE::UpdateSize


//--------------------------------------------------------------
// now update the call graph, i.e. simply increment the inline
// count, set the edge to no_inline, adjust the total program
// size
//--------------------------------------------------------------
void
Update_Call_Graph (IPA_NODE *n)
{

    /* by removing a call, we decrease the number of basic block by 1 */
    PU_SIZE size = n->PU_Size ();
    size.Inc_PU_Size (-1, 0, -1);
    n->Set_PU_Size (size);

    size.Set_PU_Size (1, 0, 1);
    Total_Prog_Size -= size.Weight ();

    n->Summary_Proc()->Decr_call_count ();
    n->Summary_Proc()->Decr_callsite_count ();
    // Don't update Total_Succ as the edge is still present.

} // Update_Call_Graph 


/*-------------------------------------------------------------*/
/* check to see if the callee, being procedure with nested PU, */
/* can be inlined only if all its nested PU are inlined        */
/*-------------------------------------------------------------*/
static BOOL
no_inline_pu_with_nested_pus(IPA_NODE* caller, IPA_GRAPH* cg)
{
    const PU_Info* pu = caller->PU_Info ();
    if (pu == NULL) 			// alt.entry
	return TRUE;
    for (pu = PU_Info_child (pu); pu; pu = PU_Info_next (pu)) {

	const AUX_PU& aux_pu =
	    Aux_Pu_Table [ST_pu (St_Table [PU_Info_proc_sym (pu)])];
	const IPA_NODE* child = cg->Node_User (AUX_PU_node (aux_pu));
#if (defined(_STANDALONE_INLINER) || defined(_LIGHTWEIGHT_INLINER))
	if (child && (!(child->Has_Inline_Attrib() || 
                  child->Has_Must_Inline_Attrib())))
#else // _STANDALONE_INLINER
	if (child && !child->Is_Deletable ())
#endif // _STANDALONE_INLINER
	    return TRUE;
    }
    return FALSE;
}

#ifdef KEY
#include "ir_bread.h"		// for WN_get_section_base
#include "sys/elf_whirl.h"	// for WT_COMP_FLAGS
// Fills in the vector with options
static void
get_command_options (IPA_NODE * node, vector<char *> &v)
{
    PU_IDX pu = ST_pu (node->Func_ST ());
    const IP_FILE_HDR& hdr = *AUX_PU_file_hdr (Aux_Pu_Table [pu]);

    char * base_addr = (char *) 
    	WN_get_section_base (IP_FILE_HDR_input_map_addr (hdr), WT_COMP_FLAGS);
    if (base_addr == (char *) -1)
    	ErrMsg (EC_IR_Scn_Read, "command line", IP_FILE_HDR_file_name (hdr));

    Elf64_Word argc = *((Elf64_Word *) base_addr);
    Elf64_Word * args = (Elf64_Word *) (base_addr + sizeof (Elf64_Word));

    v.reserve (sizeof (char *) * (argc - 1));
    for (INT i=1; i<argc; ++i)
    	v.push_back (base_addr + args[i]);
}

static BOOL
different_options (IPA_NODE * caller, IPA_NODE * callee)
{
    vector<char *> caller_opt;
    get_command_options (caller, caller_opt);

    vector<char *> callee_opt;
    get_command_options (callee, callee_opt);

    if (caller_opt.size() != callee_opt.size())
    	return TRUE;

    sort (caller_opt.begin(), caller_opt.end(), option_cmp());
    sort (callee_opt.begin(), callee_opt.end(), option_cmp());

    UINT size = caller_opt.size();

    for (UINT i=0; i<size; ++i)
    	if (strcmp (caller_opt[i], callee_opt[i]))
	  return TRUE;

    return FALSE;
}

#if !defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER)
// Return TRUE if a call to this PU is suitable for pure-call optimization
static bool check_node (IPA_NODE * node)
{
    WN * func = node->Whirl_Tree(FALSE);
    Is_True (WN_operator(func) == OPR_FUNC_ENTRY, ("Unexpected WN node"));

    WN * body = WN_func_body (func);
    WN * first = WN_first (body);
    Is_True (first, ("No code in function body"));

    WN * last = WN_last (body);

    if (WN_operator (first) == OPR_PRAGMA)
      first = WN_next (first);
    Is_True (first, ("No code in function body"));

    Is_True (WN_operator (first) != OPR_PRAGMA, ("Unexpected pragma"));

    // returns an expression
    if (first == last && WN_operator (first) == OPR_RETURN_VAL)
      return FALSE;

    // returns void
    if (WN_operator (last) == OPR_RETURN)
      return FALSE;

    // no control-flow statements?
    for (; first != last; first = WN_next (first))
    {
       if (OPERATOR_is_scf (WN_operator (first)) ||
           OPERATOR_is_non_scf (WN_operator (first)))
         return TRUE;
    }

    return FALSE;
}

// Return true if a fortran reference parameter is used as a do-loop index
// and the corresponding actual is an array element. We should be able to
// inline in such scenarios. One option is to use something like COPY_IN
// and COPY_OUT immediately when the temporary is written to.
// Bugs 110, 9925
static BOOL
formal_is_loop_index (IPA_NODE * caller_node, IPA_NODE * callee_node, IPA_EDGE * ed)
{
    // num_formals is actually the lesser of the # of actual parameters and
    // # of formal parameters.
    INT num_formals = ed->Num_Actuals() < callee_node->Num_Formals() ? 
    			ed->Num_Actuals() : callee_node->Num_Formals();

    if (!num_formals) // Nothing to check
	return FALSE;

    BOOL lang = ((callee_node->Summary_Proc()->Get_lang() == LANG_F77) || 
    		(callee_node->Summary_Proc()->Get_lang() == LANG_F90));

    if (!lang)
	return FALSE;

    SUMMARY_FORMAL* callee_formal = IPA_get_formal_array(callee_node);
    SUMMARY_ACTUAL* call_actual = IPA_get_actual_array(caller_node);

    SUMMARY_ACTUAL *actuals = &call_actual[ed->Summary_Callsite()->Get_actual_index()];
    SUMMARY_FORMAL *formals = &callee_formal[callee_node->Summary_Proc()->Get_formal_index()];

    for (INT i=0; i<num_formals; ++i) {
      if (formals[i].Is_loop_index() && actuals[i].Get_pass_type() == PASS_ARRAY)
        return TRUE;
    }
    return FALSE;
}
#endif // ! _STANDALONE_INLINER && ! _LIGHTWEIGHT_INLINER
#endif // KEY

/*-------------------------------------------------------------*/
/* check to see if we should be inlining                       */
/*-------------------------------------------------------------*/
static BOOL
do_inline (IPA_EDGE *ed, IPA_NODE *caller,
	   IPA_NODE *callee, const IPA_CALL_GRAPH *cg)
{
    BOOL result = TRUE;
    char *reason = 0;

#ifdef KEY
    if (cg->Graph()->Is_Recursive_Edge (ed->Edge_Index ())) {
#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
	if (caller == callee)
	{
	// we may inline it, but that doesn't mean we can delete it.
	    caller->Set_Undeletable ();
	    caller->Set_Recursive ();
	}
	BOOL set_recursive_in_edge = TRUE;
	if (ed->Has_frequency () && callee->Has_frequency () &&
		 ed->Get_frequency().Known() && callee->Get_frequency ().Known()) {
	    if (compute_hotness (ed, callee, Effective_weight (callee))
		< IPA_Min_Hotness)
		set_recursive_in_edge = FALSE;
	}

	if (set_recursive_in_edge)
#endif //  _STANDALONE_INLINER
	    callee->Set_Recursive_In_Edge();
    }
#endif
#if 0
    // caller and callee are not from the same partition AND the
    // callee is not from the COMMON partition (partiton with value 0)
    // or the caller is from the COMMON partition but not the callee
    if ((IPA_Enable_GP_Partition ||
	 (IPA_Enable_SP_Partition && (IPA_Space_Access_Mode == SAVE_SPACE_MODE))) &&
	    ((caller->Get_partition_group() == COMMON_PARTITION) || 
	    (callee->Get_partition_group() != COMMON_PARTITION)) && 
	    (caller->Get_partition_group() != callee->Get_partition_group())) 
	return FALSE;
#endif
//;;printf("######## (0x%x)%s->(0x%x)%s (%.1f)\n", caller,caller->Name(), callee,callee->Name(),callee->Get_cycle_count()._value );
//;;fflush(stdout);

    if (callee->Should_Be_Skipped()) {
	reason = "callee is skipped";
	ed->Set_reason_id (0);
	result = FALSE;
    }
    else if (ed->Has_Noinline_Attrib()) {
	reason = "edge is skipped";
	ed->Set_reason_id (1);
	result = FALSE;
    }
    else if (IPA_Enable_DCE && ed->Is_Deletable ()) {
        // call deleted by DCE
	reason = "call deleted by DCE";
	ed->Set_reason_id (2);
	result = FALSE;
    }
    else if (!IPA_Enable_Inline_Nested_PU && caller->Is_Nested_PU ()) {
        // Check for nested PU
	result = FALSE;
	reason = "caller is a nested procedure";
	ed->Set_reason_id (3);
    } else if ( PU_uplevel (callee->Get_PU ()) &&
		((!IPA_Enable_Inline_Nested_PU) ||
		no_inline_pu_with_nested_pus(callee, cg->Graph ()))) {
	if (callee->Has_Must_Inline_Attrib()) {
	    callee->Clear_Must_Inline_Attrib ();
	    reason = "callee has nested procedure(s) so ignore user MUST inline request";
	ed->Set_reason_id (4);
	}
	else 
	    reason = "callee has nested procedure(s)";
	ed->Set_reason_id (5);
	callee->Set_Noinline_Attrib ();
	result = FALSE;
#ifdef KEY
// Do recursive inlining only under ipa
#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
    } else if (cg->Graph()->Is_Recursive_Edge (ed->Edge_Index ()) &&
    	       (caller != callee || !INLINE_Recursive)) {
#else
    } else if (cg->Graph()->Is_Recursive_Edge (ed->Edge_Index ())) {
#endif
#else	// for KEY
    } else if (cg->Graph()->Is_Recursive_Edge (ed->Edge_Index ())) {
#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
	BOOL set_recursive_in_edge = TRUE;
	if (ed->Has_frequency () && callee->Has_frequency () &&
		 ed->Get_frequency().Known() && callee->Get_frequency ().Known()) {
	    if (compute_hotness (ed, callee, Effective_weight (callee))
		< IPA_Min_Hotness)
		set_recursive_in_edge = FALSE;
	}

	if (set_recursive_in_edge)
#endif //  _STANDALONE_INLINER
	    callee->Set_Recursive_In_Edge ();
#endif	// KEY
	result = FALSE;
	reason = "callee is recursive";
	ed->Set_reason_id (6);
    } else if (callee->Has_Varargs()) {
	result = FALSE;
	reason = "callee is varargs";
	ed->Set_reason_id (7);
    } else if (callee->Summary_Proc()->Is_alt_entry() ||
	       callee->Summary_Proc()->Has_alt_entry() || 
	       caller->Summary_Proc()->Is_alt_entry()) {
	result = FALSE;
	reason = "function with alternate entry point";
	ed->Set_reason_id (8);
    }
#ifdef KEY
    else if (!INLINE_Param_Mismatch 
    	     && ed->Num_Actuals() < callee->Num_Formals())
#else
    else if (ed->Num_Actuals() < callee->Num_Formals())
#endif
    {
	result = FALSE;
	reason = "number of parameters mismatched";
	ed->Set_reason_id (9);
    } else if (callee->Summary_Proc()->Has_formal_pragma()) {
	result = FALSE;
	reason = "callee has pragmas which are associated with formals";
	ed->Set_reason_id (10);
    } else if (callee->Summary_Proc()->Has_mp_needs_lno()) {
	result = FALSE;
	reason = "callee has flag that suggested that it should be MP'ed";
	ed->Set_reason_id (11);
    } else if (callee->Summary_Proc()->Has_noinline_parallel_pragma()) {
	result = FALSE;
	reason = "callee has parallel pragmas that suggest turning off inlining";
	ed->Set_reason_id (12);
    } else if ((caller->Summary_Proc()->Has_parallel_pragma() ||
	       caller->Summary_Proc()->Has_parallel_region_pragma()) &&
	       callee->Summary_Proc()->Has_var_dim_array()) {
	result = FALSE;
	reason = "callee has VLAs and caller has parallel_pragma"; 
	ed->Set_reason_id (13);
    } else if (caller->Summary_Proc()->Has_parallel_region_pragma() &&
	       callee->Summary_Proc()->Has_pdo_pragma()) {
	result = FALSE;
	reason = "callee has PDO pramgas and caller has parallel_pragma"; 
	ed->Set_reason_id (14);
    } else if (ed->Summary_Callsite()->Is_no_inline())  {

#if (defined(_STANDALONE_INLINER) || defined(_LIGHTWEIGHT_INLINER))
        // check for pragmas and command line options before setting this
  	// call to no inline
	if ( !ed->Has_Must_Inline_Attrib() && !callee->Has_Must_Inline_Attrib()) {
#endif // _STANDALONE_INLINER 

            result = FALSE;
	    reason = "callsite pragma requested not to inline";
	ed->Set_reason_id (15);
#if (defined(_STANDALONE_INLINER) || defined(_LIGHTWEIGHT_INLINER))
	}
#endif // _STANDALONE_INLINER 

    } else if (ed->Summary_Callsite()->Is_must_inline() &&
	     !callee->Has_Noinline_Attrib())  {
        // Pragmas override commandline options
        // set the MustInline bit so that we inline regardless
        // of size
        ed->Set_Must_Inline_Attrib();

#if (defined(_STANDALONE_INLINER) || defined(_LIGHTWEIGHT_INLINER))
    } else if (callee->Summary_Proc()->Is_exc_inline() && !INLINE_Exceptions) {
#else // _STANDALONE_INLINER
    } else if (callee->Summary_Proc()->Is_exc_inline() && !IPA_Enable_Exc) {
#endif // _STANDALONE_INLINER
	result = FALSE;
	reason = "exception handling function";
	ed->Set_reason_id (16);
    } else if (callee->Summary_Proc()->Is_exc_inline() &&
	     callee->Summary_Proc()->Has_pstatic()) {
	result = FALSE;
	reason = "exception handling code with pstatics";
	ed->Set_reason_id (17);
    } else if ((UINT) cg->Node_Depth(callee) > IPA_Max_Depth) {
	result = FALSE;
	reason = "depth in call graph exceeds specified maximum";
	ed->Set_reason_id (18);
    } else if (!ed->Has_Must_Inline_Attrib() &&
	     (callee->Has_Noinline_Attrib() ||
	      (callee->Summary_Proc()->Is_no_inline() && result) ||
	      (!callee->Has_Must_Inline_Attrib() && INLINE_None ))) {
	result = FALSE;
	reason = "user requested not to inline";
	ed->Set_reason_id (19);
#if (defined(_STANDALONE_INLINER) || defined(_LIGHTWEIGHT_INLINER))
    // if an inline function has local statics the front end marks the function
    // pre-emptible (in an attempt to not inline it) and weak
    // The inliner doesn't inline this fn and should emit a message
    // that distinguishes it from the case where a function was not inlined
    // because it was NOT marked inline
    } else if ( ( (!callee->Summary_Proc()->Is_may_inline() &&
	    !callee->Summary_Proc()->Is_must_inline()) && 
	    !INLINE_Preemptible ) && 
	    ( !callee->Has_Must_Inline_Attrib() ) && 
	    !ed->Summary_Callsite()->Is_must_inline() && 
	    !ed->Has_Must_Inline_Attrib()) {
	result = FALSE;
	if ( callee->Summary_Proc()->Has_fstatic()) 
	{
            reason = "function has local fstatics and is set preemptible";
	        ed->Set_reason_id (20);
	}
	else
	{
            reason = "function is preemptible and has not been set to mustinline";
	        ed->Set_reason_id (21);
	}

#endif // _STANDALONE_INLINER
    }
    else if (!return_types_are_compatible(callee, ed)) {
	reason = "incompatible return types";
	        ed->Set_reason_id (22);
	result = FALSE;
    }
    else if (INLINE_Check_Compatibility != AGGRESSIVE && // KEY
             !param_types_are_compatible(caller, callee, ed)) {
	reason = "incompatible parameter types";
	        ed->Set_reason_id (23);
	result = FALSE;
    } 
#if defined(KEY) && !defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER)
    else if (Opt_Options_Inconsistent && 
    	     caller->File_Id() != callee->File_Id()) {
    // The caller and callee come from different files, check if they
    // are to be compiled with different options
    	if (different_options (caller, callee)) {
	    result = FALSE;
	    reason = "optimization options are different for caller and callee";
	    ed->Set_reason_id (34);
	}
    }
    else if (IPA_Enable_Pure_Call_Opt && 
             !callee->Summary_Proc()->Has_side_effect() &&
	     !callee->Summary_Proc()->Is_must_inline() &&
	     !ed->Has_Must_Inline_Attrib() &&
	     !callee->Has_Must_Inline_Attrib() &&
	     // Check several heuristics to determine if it is a good
	     // candidate.
	     check_node (callee)) {
	    result = FALSE;
	    reason = "Trying to do pure-call-optimization for this callsite";
	    ed->Set_reason_id (35);
    }
    // This can only arise with ipa
    else if (callee->Is_Lang_CXX() && !caller->Is_Lang_CXX() &&
             PU_has_exc_scopes (callee->Get_PU())) {
            result = FALSE;
            reason = "not inlining C++ with exceptions into non-C++";
            ed->Set_reason_id (36);
    }
    else if (formal_is_loop_index(caller, callee, ed)) {
            result = FALSE;
            reason = "formal parameter is a loop index";
            ed->Set_reason_id (37);
    }
#endif // KEY && !_STANDALONE_INLINER && !_LIGHTWEIGHT_INLINER
    // The following else-if must be last
    else if (!IPA_Enable_Lang) {
	if ((callee->Summary_Proc()->Get_lang() == LANG_F77) || 
	    (caller->Summary_Proc()->Get_lang() == LANG_F77)) {
	    if ((callee->Summary_Proc()->Get_lang() != LANG_F77) || 
		(caller->Summary_Proc()->Get_lang() != LANG_F77)) {
		result = FALSE;
		reason = "not inlining across language boundaries";
	        ed->Set_reason_id (24);
	    }
	}
        else if ((callee->Summary_Proc()->Get_lang() == LANG_F90) || 
                 (caller->Summary_Proc()->Get_lang() == LANG_F90)) {
	    if ((callee->Summary_Proc()->Get_lang() != LANG_F90) || 
		(caller->Summary_Proc()->Get_lang() != LANG_F90)) {
		result = FALSE;
		reason = "not inlining across language boundaries";
	        ed->Set_reason_id (25);
	    }
	}
    }

    if ( result == FALSE ) {
	Report_Reason ( callee, caller, reason , ed);
	return FALSE;
    } 

#if 0 // def _STANDALONE_INLINER
    return result;
#else
    return check_size_and_freq (ed, caller, callee, cg);
#endif // _STANDALONE_INLINER
} // do_inline



// invocation cost for each IPA_EDGE during inline analysis
typedef AUX_IPA_EDGE<INT32> INVOCATION_COST;

// Assign to each call a "cost", which is used to determine the priority of
// inlining.  There are three factors, loopnest of the call, number of calls
// from the callee (0 means leaf), and size of the callee.  Also, we isolated
// out several boundary cases which we give higher priority:
//
// 1) loopnest > 0 && call_count == 0
// 2) loopnest == 0 && call_count == 0
// 3) loopnest > 0 && call_count > 0
// 4) loopnest == 0 && call_count > 0
//
// In the first 3 cases, the value of call_count does not matter,
// all we care is > or == 0.  We just sort by size.  In the 4th cases, we sort
// first by call_count, and then by the size.
static INT32
Estimated_Invocation_Cost (IPA_EDGE* edge, const IPA_CALL_GRAPH* cg)
{
    IPA_NODE* callee = cg->Callee (edge);

    INT loopnest = edge->Summary_Callsite ()->Get_loopnest ();

#if (defined(_STANDALONE_INLINER) || defined(_LIGHTWEIGHT_INLINER))
    INT32 cost = callee->Weight ();
#else
    INT32 cost = Effective_weight (callee);

    if (edge->Has_frequency () && callee->Has_frequency () &&
	    edge->Get_frequency().Known() && callee->Get_frequency ().Known()) {
	// if feedback information is available, ignore the heuristics and
	// use the "hotness" of the callee instead
	return INT32_MAX - (INT32)compute_hotness (edge, callee, cost);
    }
#endif // _STANDALONE_INLINER

    if (loopnest < 100)
	/* assume we never have loopnest > 100 */
	cost += ((100 - loopnest) << 11);
    
    if (callee->PU_Size().Call_Count () != 0) {
	if (loopnest > 0)
	    cost += (1 << 22);
	else
	    cost += (callee->PU_Size().Call_Count () << 22);
    }

    return cost;
    
} // Estimated_Invocation_Cost


// comparision function object for sorting the callsites
struct INVOCATION_COST_COMP
{
    const INVOCATION_COST& cost_vector;

    INVOCATION_COST_COMP (const INVOCATION_COST& c) : cost_vector (c) {}

    BOOL operator() (IPA_EDGE_INDEX e1, IPA_EDGE_INDEX e2) const {
	return cost_vector[e1] < cost_vector[e2];
    }
};

// For each node, create a list of call sites and sort them based on the
// cost function defined in Estimated_Invocation_Cost so that more
// desirable callees are inlined first.   
typedef vector<IPA_EDGE_INDEX> EDGE_INDEX_VECTOR;

void
Get_Sorted_Callsite_List (IPA_NODE *n, IPA_CALL_GRAPH *cg,
			  INVOCATION_COST& cost_vector,
			  EDGE_INDEX_VECTOR& callsite_list)
{
    if (cg->Num_Out_Edges(n) == 0)
	return;

    Is_True (callsite_list.empty (), ("Uninitialized callsite list"));

    IPA_SUCC_ITER edge_iter (cg, n);
    for (edge_iter.First (); !edge_iter.Is_Empty (); edge_iter.Next ()) {
	IPA_EDGE *edge = edge_iter.Current_Edge ();

	if (edge) {
	    IPA_EDGE_INDEX idx = edge->Array_Index ();
	    cost_vector[idx] = Estimated_Invocation_Cost (edge, cg);
	    callsite_list.push_back (idx);
	}
    }   

    sort (callsite_list.begin (), callsite_list.end (),
	  INVOCATION_COST_COMP (cost_vector));
} // Get_Sorted_Callsite_List


// decide if the given call could be deleted or inlined
static void
Analyze_call (IPA_NODE* caller, IPA_EDGE* edge, const IPA_CALL_GRAPH* cg)
{
    IPA_NODE* callee = cg->Callee (edge);

#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
	    
    if (IPA_Enable_DCE) {
	// Do dead call elimination analysis
		
	if (!callee->Summary_Proc()->Has_pragma_side_effect() && // KEY
	    ((edge->Is_Deletable () || // set by const. propagation

	      (
#ifdef KEY
	       callee->Summary_Proc()->Get_callsite_count() == 0 &&
#else
	       cg->Node_Depth (callee) == 0 &&
#endif
	       !callee->Has_Direct_Mod_Ref() &&
	       !callee->Summary_Proc()->Is_alt_entry () &&
	       !callee->Summary_Proc()->Has_alt_entry () &&
	       !caller->Summary_Proc()->Is_alt_entry () &&
	       return_types_are_compatible (callee, edge))))) {

	    // KEY
	    Is_True (callee->Summary_Proc()->Get_callsite_count() ||
	             !callee->Summary_Proc()->Get_call_count(),
		     ("Callsite and call counts don't match"));

	    edge->Set_Deletable();
	    if (Trace_IPA || Trace_Perf) {
		fprintf (TFile, "%s called from ",
			 DEMANGLE (callee->Name()));
		fprintf(TFile, "%s deleted\n",
			DEMANGLE (caller->Name())); 
	    }

	    Update_Call_Graph (caller);
			
	    if (IPA_Enable_DFE)
		Update_Total_Prog_Size (caller, callee, cg);

	    return;		// edge deleted, skip the inline analysis
	}
    }

    if (! IPA_Enable_Inline)
	return;
    
#endif // _STANDALONE_INLINER

    if (do_inline (edge, caller, callee, cg)) {
	    
        if(Get_Trace ( TP_IPA, IPA_TRACE_TUNING)) {
            fprintf(Verbose_inlining,"YYY\t(%p)%-20s -------<%p>--------> (%p)%-20s\n",caller,caller->Name(),edge,callee,callee->Name());
	}

	edge->Set_Inline_Attrib ();
	Total_Inlined++;

    } else {
	edge->Clear_All_Inline_Attrib ();

        if(Get_Trace ( TP_IPA, IPA_TRACE_TUNING)) {
            fprintf(Verbose_inlining,"NNN\t(%p)%-20s -------<%p>--------> (%p)%-20s\n",caller,caller->Name(),edge,callee,callee->Name());
	}
#if 0
	if (callee->Has_Must_Inline_Attrib())
	    callee->Clear_Must_Inline_Attrib ();
#endif
	if (callee->Has_Inline_Attrib())
	    callee->Clear_Inline_Attrib ();
	Total_Not_Inlined++;
    }
} // Analyze_call


/*-------------------------------------------------------------------------*/
/* Solve the interprocedural analysis phase of inlining.                   */
/*-------------------------------------------------------------------------*/

#ifdef KEY

// invocation freq for each IPA_EDGE during inline analysis
typedef AUX_IPA_EDGE<float> INVOCATION_FREQ;

// comparision function object for sorting the callsites
struct INVOCATION_FREQ_COMP
{
  const INVOCATION_FREQ& freq_vector;

  INVOCATION_FREQ_COMP( const INVOCATION_FREQ& c ) : freq_vector (c) {}

  BOOL operator() (IPA_EDGE_INDEX e1, IPA_EDGE_INDEX e2) const {
    return freq_vector[e1] > freq_vector[e2];
  }
};


void Perform_Inline_Analysis2( IPA_CALL_GRAPH* cg, MEM_POOL* pool )
{
  INVOCATION_FREQ freq_vector( cg, pool );
    
  if( Get_Trace ( TP_IPA, IPA_TRACE_TUNING) ){
    Verbose_inlining = fopen ("Verbose_inlining.log", "w");
    N_inlining = fopen ("N_inlining.log", "w");
    Y_inlining = fopen ("Y_inlining.log", "w");
    e_weight = fopen ("callee_wght.log","w");
  }

  Init_inline_parameters ();

#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
  if( IPA_Enable_DFE ){
    inline_count = CXX_NEW (INLINE_COUNTER_ARRAY (cg, pool), pool);
  }
#endif // !_STANDALONE_INLINER

  EDGE_INDEX_VECTOR callsite_list;

  IPA_NODE_ITER cg_iter( cg, PREORDER, pool );

  /* traverse all nodes at PREORDER */
  for( cg_iter.First(); !cg_iter.Is_Empty(); cg_iter.Next() ){
    IPA_NODE* caller = cg_iter.Current();

    if( caller == NULL ||
	caller->Should_Be_Skipped() ||
	cg->Num_Out_Edges(caller) == 0 ){
      Total_Not_Inlined++;
      continue;
    }

    IPA_SUCC_ITER edge_iter( cg, caller );

    for( edge_iter.First (); !edge_iter.Is_Empty (); edge_iter.Next() ){
      IPA_EDGE *edge = edge_iter.Current_Edge ();

      if( edge != NULL &&
	  edge->Has_frequency() ){
	const FB_FREQ freq = edge->Get_frequency();

	if( freq.Known() &&
	    freq.Value() > 0 ){
	  const IPA_EDGE_INDEX idx = edge->Array_Index();
	  freq_vector[idx] = freq.Value();
	  callsite_list.push_back (idx);
	}
      }
    }   
  }    

  sort( callsite_list.begin(),
	callsite_list.end(),
	INVOCATION_FREQ_COMP( freq_vector ) );

  for( EDGE_INDEX_VECTOR::iterator first = callsite_list.begin();
       first != callsite_list.end();
       first++ ){
    IPA_EDGE* edge = cg->Edge(*first);
    Analyze_call( cg->Caller(edge), edge, cg );
  }

#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))
  if( IPA_Enable_DFE ){
    CXX_DELETE (inline_count, pool);
    inline_count = NULL;
  }
#endif  // !_STANDALONE_INLINER

  if( Get_Trace ( TP_IPA, IPA_TRACE_TUNING) ){
    fclose(e_weight);
    fclose(Y_inlining);
    fclose(N_inlining);
    fclose(Verbose_inlining);
  }

  //return Perform_Inline_Analysis( cg, pool );
}
#endif // KEY

void
Perform_Inline_Analysis (IPA_CALL_GRAPH* cg, MEM_POOL* pool)
{
    INVOCATION_COST cost_vector (cg, pool);
    
    if(Get_Trace ( TP_IPA, IPA_TRACE_TUNING)) {
        Verbose_inlining = fopen ("Verbose_inlining.log", "w");
	N_inlining = fopen ("N_inlining.log", "w");
	Y_inlining = fopen ("Y_inlining.log", "w");
	e_weight = fopen ("callee_wght.log","w");
    }
    Init_inline_parameters ();

#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))

    if (IPA_Enable_DFE)
	inline_count = CXX_NEW (INLINE_COUNTER_ARRAY (cg, pool), pool);

#endif // _STANDALONE_INLINER

    EDGE_INDEX_VECTOR callsite_list;
    IPA_NODE_ITER cg_iter (cg, LEVELORDER, pool);


    /* traverse all nodes at levelorder */
    for (cg_iter.First(); !cg_iter.Is_Empty(); cg_iter.Next()) {
	IPA_NODE* caller = cg_iter.Current();

	if (caller == NULL)
	    continue;

	if (caller->Should_Be_Skipped()) { 
	    Total_Inlined++;
	    continue;
	}

	callsite_list.clear ();
	Get_Sorted_Callsite_List (caller, cg, cost_vector, callsite_list);

	EDGE_INDEX_VECTOR::const_iterator last = callsite_list.end ();
	for (EDGE_INDEX_VECTOR::iterator first = callsite_list.begin ();
	     first != last; ++first) {

	    Analyze_call (caller, cg->Edge (*first), cg);

	}
    }

#if (!defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER))

#ifdef TODO
    /* Feedback needs inline counters, so use before cleaned up */
    if( IPA_Enable_Feedback ) {
	fprintf(IPA_Feedback_prg_fd,"\nINLINING SUCCESS INFO\n\n");
    	IPA_NODE_ITER cg_iter (cg, PREORDER);
	/* process INLINE nodes for #pragma inline info */
    	for (cg_iter.First(); !cg_iter.Is_Empty(); cg_iter.Next()) {
	    IPA_NODE* n = (IPA_NODE*)cg_iter.Current();
	    if (n == NULL) continue;
	    if ((*inline_count)[caller]) {
		/* check for cross-file inlining */
		if((caller->Get_fbk_ptr() )->Get_Cross_File_Fnd() ) {
		    fprintf( IPA_Feedback_prg_fd, "#pragma inline %s /* Cross-file - %d inlined - %d calls */\n",
			     DEMANGLE (caller->Name()),
			     (*inline_count)[caller],
			     cg->Num_In_Edges(n));
		} else {
		    fprintf(IPA_Feedback_prg_fd, "#pragma inline %s /* %d inlined - %d calls */\n",
			    DEMANGLE (caller->Name()),
			    (*inline_count)[caller],
			    cg->Num_In_Edges(n));
		}
	    }
	}
    } /* if (IPA_Enable_Feedback) */
#endif // TODO

    if (IPA_Enable_DFE) {
	CXX_DELETE (inline_count, pool);
	inline_count = 0;
    }
    
#endif  // _STANDALONE_INLINER

    if(Get_Trace ( TP_IPA, IPA_TRACE_TUNING)) {
        fclose(e_weight);
        fclose(Y_inlining);
        fclose(N_inlining);
        fclose(Verbose_inlining);
    }
} // Perform_Inline_Analysis
