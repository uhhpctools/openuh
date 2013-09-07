/***************************************************************************
  This file is created by Xiaonan(Daniel) Tian from HPCTools, University of Houston
  (daniel.xntian@gmail.com) for OpenUH OpenACC compiler.
  It is intended to lower the OpenACC pragma.
  It is free to use. However, please keep the original author.
  http://www2.cs.uh.edu/~xntian2/
*/



/**
***		OPENACC Lowering Support
***		---------------------------
***
*** Description:
***
***	This interface describes all the declarations and data needed to
***     perform and support ACC lowering.
***
*** Exported functions:
***
***  These functions are from wn_acc.cxx:
***
***	lower_acc	called when first ACC pragma is encountered, identifies,
***			extracts, converts and replaces an entire parallel
***			region in the whirl tree
***
***
**/

#ifndef wnacc_INCLUDED
#define wnacc_INCLUDED "wn_acc.h"


#include <map>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>

using namespace std;


#if (! defined(__ELF_H__)) && (! defined(BUILD_OS_DARWIN))
#include <elf.h>            /* pu_info.h can't compile without this */
#endif

#ifndef dwarf_DST_INCLUDED
#include "dwarf_DST.h"      /* for DST_IDX and DST_INFO_IDX */
#endif

#ifndef pu_info_INCLUDED    /* for PU_Info */
#include "pu_info.h"
#endif

#ifndef cxx_template_INCLUDED
#include "cxx_template.h"   /* for DYN_ARRAY */
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern WN * lower_acc_nvidia (WN * block, WN * node, INT64 actions);
extern WN * lower_acc_apu (WN * block, WN * node, INT64 actions);
extern INT32 acc_region_num;	 // MP region number within parent PU
extern INT32 acc_construct_num; // construct number within MP region


typedef enum {
  ACCP_UNKNOWN,
  ACCP_PARALLEL_REGION,
  ACCP_LOOP_REGION,
  ACCP_HOST_DATA_REGION,
  ACCP_DATA_REGION,
  ACCP_KERNEL_REGION,
  ACCP_UPDATE_REGION,
  ACCP_CACHE_REGION,
  ACCP_DECLARE_REGION,
  ACCP_WAIT_REGION, 
  ACCP_ENTER_DATA_REGION, 
  ACCP_EXIT_DATA_REGION,  
  ACCP_ATOMIC_REGION
} ACCP_process_type;

extern WN* VH_OpenACC_Lower(WN * node, INT64 actions);

extern BOOL acc_scalarization_enabled;
extern BOOL acc_scalarization_level2_enabled;
extern BOOL acc_scalarization_level3_enabled;
extern BOOL acc_ptr_restrict_enabled;
extern map<ST*, BOOL> acc_const_offload_scalar;
extern vector<ST*> acc_loop_index_var;
extern UINT32 kernel_tmp_licm_count;
extern TY_IDX ACC_Get_ElementTYForMultiArray(ST* stArr);
extern void ACC_Scalar_Replacement_Algorithm(WN* tree, ST* st_kernel);

#ifdef __cplusplus
}
#endif

#endif

