/***********************************************************************************************************/
/***********************************************************************************************************/
/***********************************************************************************************************/
/***********************************************************************************************************/
/***********************************************************************************************************/
/*
Data flow analysis for OpenACC offload region which require compiler to identify the private/in/out/inout variables.
Author: Xiaonan(Daniel) Tian, Computer Science Department of University of Houston
Email: xtian2@uh.edu
Copyright Reserved  by the author
*/
/***********************************************************************************************************/
/***********************************************************************************************************/
/***********************************************************************************************************/
/***********************************************************************************************************/
#ifndef OPT_DFA_LIVENSS_FOR_OPENACC_PARALLEL_REGION
#define OPT_DFA_LIVENSS_FOR_OPENACC_PARALLEL_REGION "opt_dfa_openacc.h"
#include "opt_cfg.h"

void init_dfa_openacc();
void free_dfa_resource_openacc();
void dfa_def_use_info_bb_openacc(BB_NODE *bb);
void dfa_liveness_analysis_openacc();
void dfa_attach_pragma_tree_openacc();
void dfa_scan_all_var_st(WN* entry);
void perform_global_dfa(CFG* cfg);//WDFA_TYPE wdfa_type)
void TraverseAndCheckEachBBStmt(CFG* cfg);

#endif

