#include <algorithm>
#include <string>
#include <map>
#include <iostream>
#include <fstream>

#include "wn.h"
#include "wb_util.h"
#include "ir_reader.h"
#include "srcpos.h"
#include "parmodel.h"
#include "uh_util.h"

#include "lwn_util.h"
#include "lnoutils.h"
#include "ara.h"
#include "ara_loop.h"

extern PU_Info *LNO_Current_PU;

// Gathering loop cost of this loop
// Laks 03.19.07
//-----------------------------------------------------
static double UH_GatherLoopCost(WN *wn, double &fLoop, double &fMachine, double &fCache, double &fCost, double &nIters) {
  double mach_cost_per_iter = 0, est_num_iters; 	// cost per iterations
  INT nloops = SNL_Loop_Count(wn); 		// WARNING: this only works for SNL !!

	// gathering parallel status from parmodel.cxx
  PAR_STAT::id_count = 0; 		// do we need this ?
  PAR_STAT* ps = CXX_NEW(PAR_STAT(wn, nloops, &LNO_local_pool), 
    &LNO_local_pool);			// PAR_STAT is needed for loop overhead cost
  fLoop = ps->Loop_Overhead_Cost();
  CXX_DELETE(ps, &LNO_local_pool);  	// this can be costly
					// we need to avoid alloc and dealloc

	// machine cost
  fMachine = SNL_Machine_Cost(wn, nloops, 0, NULL,
      &mach_cost_per_iter, TRUE);

	// cache cost
  INT *ident_perm = CXX_NEW_ARRAY(INT, nloops, &LNO_local_pool);
  for (INT i = 0; i < nloops; i++) 
    ident_perm[i] = i; 
  fCache = SNL_Cache_Cost(wn, ident_perm, nloops,
    Do_Loop_Depth(wn), -1, NULL, &est_num_iters);
  CXX_DELETE_ARRAY(ident_perm, &LNO_local_pool);
 
	// average cost per iterations
  fCost = Compute_Work_Estimate(mach_cost_per_iter, fCache/est_num_iters);
  nIters = est_num_iters;
  // return the total cycles
  return (fMachine + fLoop + fCache);
}

//-----------------------------------------------------------------------
// NAME: Has_Io
// FUNCTION: Returns TRUE if the tree rooted at 'wn_tree' has an IO
//   statement, FALSE otherwise.   All I/O whirl nodes are pushed on the
//   stack 'stk_io'.
//-----------------------------------------------------------------------

static BOOL Has_Io(WN* wn_tree, STACK<WN*>* stk_io)
{
  BOOL found_io = FALSE;

  if (WN_opcode(wn_tree) == OPC_IO) {
    stk_io->Push(wn_tree);
    found_io = TRUE;
  }

  if (WN_opcode(wn_tree) == OPC_BLOCK) {
    for (WN* wn = WN_first(wn_tree); wn != NULL; wn = WN_next(wn))
      if (Has_Io(wn, stk_io))
        found_io = TRUE;
  } else {
    for (INT i = 0; i < WN_kid_count(wn_tree); i++)
      if (Has_Io(WN_kid(wn_tree, i), stk_io))
        found_io = TRUE;
  }
  return found_io;
}

// ---------------------------------------------------------------------
// update the variable global containing the database of the loop
// PARAM: 	wn is the loop
// 	
// ---------------------------------------------------------------------
static void UH_UpdateLoopStat(WN *wn, PAR_LOOP_STATUS loopStat, DO_LOOP_INFO* dli)
{
  std::map<WN*, PAR_LOOP_STATUS>::iterator itr;
  double fLoop, fCache, fMachine, fCost;
  double nIters;
  double fTotal = UH_GatherLoopCost(wn, fLoop, fMachine, fCache, fCost, nIters);
  bool isItersSymbolic =   (dli->Num_Iterations_Symbolic &&
      dli->Est_Max_Iterations_Index != -1);

  if((itr=mapLoopStatus.find(wn))!=mapLoopStatus.end()) {
    // the loop exist !
    itr->second.iStatus = loopStat.iStatus;
    itr->second.sReason = loopStat.sReason;
    itr->second.fOverhead = fLoop;
    itr->second.fMachine= fMachine;
    itr->second.fCache = fCache;
    itr->second.fTotal = fTotal;
    itr->second.fCost = fCost;
    itr->second.nIterations = dli->Est_Num_Iterations;
    itr->second.iLine = UH_GetLineNumber(wn);
    itr->second.isIterationSymbolic = isItersSymbolic;
  } else {
	// create a new entry
    PAR_LOOP_STATUS objStat;
    objStat.iStatus = loopStat.iStatus;
    objStat.sReason = loopStat.sReason;
    objStat.fOverhead = fLoop;
    objStat.fMachine = fMachine;
    objStat.fCache = fCache;
    objStat.fTotal = fTotal;
    objStat.fCost = fCost;
    objStat.nIterations = dli->Est_Num_Iterations;
    objStat.iLine = UH_GetLineNumber(wn);
    objStat.isIterationSymbolic = isItersSymbolic;
    mapLoopStatus[wn] = loopStat;
  }
}

//-----------------------------------------------------------------------
// NAME: Print_Prompl_Msgs
// FUNCTION: Print information for the .l file for 'wn_loop' which is part
//   of the subprogram 'current_pu'.  The file pointer for the .l file is
//   'fp'.  The 'ffi' is a list of SNLs in the subprogram; it is used to
//   group SNLs together in the output of the .list file.
//-----------------------------------------------------------------------

extern void UH_Print_Prompl_Msgs(WN* wn_loop)
{
  ARRAY_DIRECTED_GRAPH16* dg = Array_Dependence_Graph;
  DO_LOOP_INFO* dli = Get_Do_Loop_Info(wn_loop);
  PAR_LOOP_STATUS loopStat;
  char fp[2048];

  loopStat.wnLoop = wn_loop;
  
  // Print a blank line to separate SNLs
  INT i;
  if (dli->Last_Value_Peeled) {
    sprintf(fp,
      "%5d: Created by peeling last iteration of parallel loop.",
        (INT) UH_GetLineNumber(wn_loop));
    loopStat.iStatus = PAR_AUTO;
    loopStat.sReason = fp;
    UH_UpdateLoopStat(wn_loop, loopStat, dli);
    return;
  }
  if (Do_Loop_Is_Mp(wn_loop) && !dli->Auto_Parallelized) {
    sprintf(fp, "%5d: PARALLEL (Manual) ", (INT) UH_GetLineNumber(wn_loop));
    loopStat.iStatus = PAR_MAN;
    loopStat.sReason = fp;
    UH_UpdateLoopStat(wn_loop, loopStat, dli);
    return;
  }
  if (dli->ARA_Info == NULL)
    return;
  if (dli->ARA_Info->Is_Parallel() && dli->Auto_Parallelized) {
    if (dli->Is_Doacross)
      sprintf(fp, "%5d: PARALLEL (Auto Synchronized) ",
        (INT) UH_GetLineNumber(wn_loop));
    else
      sprintf(fp, "%5d: PARALLEL (Auto) ", (INT) UH_GetLineNumber(wn_loop));
    loopStat.iStatus = PAR_AUTO;
    loopStat.sReason = fp;
    UH_UpdateLoopStat(wn_loop, loopStat, dli);
    //vecLoopStatus.push_back(loopStat);
    return;
  }
  INT found_problem = FALSE;
  loopStat.iStatus = NOPAR;
  sprintf(fp, "%5d: Not Parallel\n", (INT) UH_GetLineNumber(wn_loop));
  loopStat.sReason = fp;
  if (dli->ARA_Info->Is_Parallel()
      && dli->ARA_Info->Not_Enough_Parallel_Work()) {
    sprintf(fp, "Loop body does not contain enough work.");
    found_problem = TRUE;
    loopStat.sReason += fp;
  }
  if (dli->Inside_Critical_Section) {
    sprintf(fp, "Loop is inside CRITICAL section.");
    found_problem = TRUE;
    loopStat.sReason += fp;
  }
  if (dli->Has_Threadprivate) {
    sprintf(fp, "Loop has THREAD_PRIVATE variables.");
    found_problem = TRUE;
    loopStat.sReason += fp;
  }
  if (dli->Has_Exits) {
    sprintf(fp, "Loop has early exits on ");
    found_problem = TRUE;
    loopStat.sReason += fp;
  }
  if (dli->Pragma_Cannot_Concurrentize) {
    sprintf(fp, "Pragma on loop inhibits parallelization.");
    found_problem = TRUE;
    loopStat.sReason += fp;
  }
  if (dli->Serial_Version_of_Concurrent_Loop) {
    sprintf(fp, "Loop in serial version of parallel loop.");
    found_problem = TRUE;
    loopStat.sReason += fp;
  }
  if (!Upper_Bound_Standardize(WN_end(wn_loop), TRUE)) {
    sprintf(fp, "Loop bounds could not be put in standard form.");
    found_problem = TRUE;
    loopStat.sReason += fp;
  }
  if (Inside_Lego_Tiled_Loop(wn_loop)) {
    sprintf(fp, "Loop refers to distributed or reshaped arrays.");
    found_problem = TRUE;
    loopStat.sReason += fp;
  }
  if (dli->ARA_Info->Is_OK_Parallel()
      && dli->ARA_Info->Inner_Loop_Is_Suggested_Parallel()) {
    sprintf(fp, "Parallelizing innermore loop was deemed more profitable.");
    found_problem = TRUE;
    loopStat.sReason += fp;
  }
  if (Outermore_Parallel_Construct(wn_loop, FALSE)) {
    sprintf(fp, "%5d: NESTED PARALLEL (Auto) ", (INT) UH_GetLineNumber(wn_loop));
    loopStat.iStatus = PAR_AUTO;
    loopStat.sReason = fp;
    UH_UpdateLoopStat(wn_loop, loopStat, dli);
    return;
  }
  if (dli->Has_Unsummarized_Calls || !dli->Is_Concurrent_Call) {
    for (i = 0; i < dli->ARA_Info->Call_No_Dep_Vars().Elements(); i++) {
      char* call_name = dli->ARA_Info->Call_No_Dep_Vars().Bottom_nth(i);
      INT ln_call = dli->ARA_Info->Ln_Call_No_Dep_Vars().Bottom_nth(i);
      found_problem = TRUE;
      if (strlen(call_name) == 0)
        sprintf(fp, "Call on line %d.", ln_call);
      else
        sprintf(fp, "Call %s on line %d.", call_name, ln_call);
      loopStat.sReason += fp;
    }
  }
  STACK<WN*> stk_io(&ARA_memory_pool);
  if (Has_Io(wn_loop, &stk_io)) {
    for (INT i = 0; i < stk_io.Elements(); i++) {
      found_problem = TRUE;
      INT ln_node = UH_GetLineNumber(stk_io.Bottom_nth(i));
      sprintf(fp, "Has IO statement on line %d.", ln_node);
      loopStat.sReason += fp;
    }
  }
  if (found_problem) {
    loopStat.sReason = fp;
    UH_UpdateLoopStat(wn_loop, loopStat, dli);
    return;
  }
  for (i = 0; i < dli->ARA_Info->Scalar_Vars().Elements(); i++) {
    found_problem = TRUE;
    sprintf(fp, "Scalar dependence on %s.",
      dli->ARA_Info->Scalar_Vars().Bottom_nth(i).Prompf_Name());
    loopStat.sReason += fp;
  }
  for (i = 0; i < dli->ARA_Info->Scalar_Alias().Elements(); i++) {
    found_problem = TRUE;
    sprintf(fp, "Aliased scalar %s.",
      dli->ARA_Info->Scalar_Alias().Bottom_nth(i).Prompf_Name());
    loopStat.sReason += fp;
  }
  for (i = 0; i < dli->ARA_Info->Scalar_No_Final().Elements(); i++) {
    found_problem = TRUE;
    sprintf(fp, "Scalar %s without unique last value.",
      dli->ARA_Info->Scalar_No_Final().Bottom_nth(i).Prompf_Name());
    loopStat.sReason += fp;
  }
  for (i =0; i < dli->ARA_Info->Scalar_Bad_Peel().Elements(); i++) {
    found_problem = TRUE;
    SYMBOL sym_priv = dli->ARA_Info->Scalar_Bad_Peel().Bottom_nth(i);
    INT ln_priv = dli->ARA_Info->Ln_Scalar_Bad_Peel().Bottom_nth(i);
    sprintf(fp, "Last iteration may not write value of scalar %s on line %d.",
      sym_priv.Prompf_Name(), ln_priv);
    loopStat.sReason += fp;
  }
  for (i = 0; i < dli->ARA_Info->Array_No_Dep_Vars().Elements(); i++) {
    found_problem = TRUE;
    SYMBOL sym_node = dli->ARA_Info->Array_No_Dep_Vars().Bottom_nth(i);
    INT ln_node = dli->ARA_Info->Ln_Array_No_Dep_Vars().Bottom_nth(i);
    sprintf(fp, "Array %s without dependence information. ",
      sym_node.Prompf_Name());
    sprintf(fp, "%s on line %d.", fp, ln_node);
    loopStat.sReason += fp;
  }
  for (i = 0; i < dli->ARA_Info->Ln_Misc_No_Dep_Vars().Elements(); i++) {
    found_problem = TRUE;
    INT ln_node = dli->ARA_Info->Ln_Misc_No_Dep_Vars().Bottom_nth(i);
    sprintf(fp, "Reference without dependence information ");
    sprintf(fp, "%s on line %d.", fp, ln_node);
    loopStat.sReason += fp;
  }
  for (i = 0; i < dli->ARA_Info->Dep_Vars().Elements(); i++) {
    found_problem = TRUE;
    SYMBOL sym_source = dli->ARA_Info->Dep_Source().Bottom_nth(i);
    SYMBOL sym_sink = dli->ARA_Info->Dep_Sink().Bottom_nth(i);
    INT ln_source = dli->ARA_Info->Ln_Dep_Source().Bottom_nth(i);
    INT ln_sink = dli->ARA_Info->Ln_Dep_Sink().Bottom_nth(i);
    sprintf(fp, "Array dependence from %s on line %d ",
      sym_source.Prompf_Name(), ln_source);
    sprintf(fp, "%s to %s on line %d.", fp,
      sym_sink.Prompf_Name(), ln_sink);
    loopStat.sReason += fp;
  }
  for (i =0; i < dli->ARA_Info->Dep_Bad_Peel().Elements(); i++) {
    found_problem = TRUE;
    SYMBOL sym_priv = dli->ARA_Info->Dep_Bad_Peel().Bottom_nth(i);
    INT ln_priv = dli->ARA_Info->Ln_Dep_Bad_Peel().Bottom_nth(i);
    sprintf(fp, "Last iteration may not write value of array %s on line %d.",
      sym_priv.Prompf_Name(), ln_priv);
    loopStat.sReason += fp;
  }
  for (i =0; i < dli->ARA_Info->Partial_Array_Sec().Elements(); i++) {
    found_problem = TRUE;
    SYMBOL sym_priv = dli->ARA_Info->Partial_Array_Sec().Bottom_nth(i);
    sprintf(fp, "Cannot privatize %s. Requires partial array section.",
      sym_priv.Prompf_Name());
    loopStat.sReason += fp;
  }
  if (!found_problem && dli->Has_Bad_Mem) {
    sprintf(fp, "Indirect reference.");
    found_problem = TRUE;
    loopStat.sReason += fp;
  }
  if (!found_problem) {
    sprintf(fp, "Reason unknown.");
    loopStat.sReason += fp;
  }
  UH_UpdateLoopStat(wn_loop, loopStat, dli);
}

