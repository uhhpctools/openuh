/*
 * Copyright 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
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


#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "linker.h"			/* linker headers */
#include "process.h"                    /* For create_tmpdir. */

#include "errors.h"			/* for Set_Error_Phase() */
#include "glob.h"			/* for Cleanup_Files() */
#include "config.h"			/* for Preconfigure() */
#include "config_list.h" 
#include "config_targ.h"		/* for Target_ABI */
#include "wn.h"				/* for Dont_Use_WN_Free_List() */

#include "ipc_defs.h"			/* IP_32_bit_ABI */
#include "ipa_option.h"			/* Process_IPA_Options () */
#include "ipo_main.h"		/* Perform_Interprocedural_Optimization */
#include "ipc_symtab_merge.h"		// Initialize_Auxiliary_Tables ()
#include "ipc_type_merge.h"		// for merging types
#include "ipc_main.h"
#include "ipc_pic.h"			// for Global_symbol_optimization ()
#include "ld_ipa_interface.h"		// for ld_for_all_ST ()

#include "ipc_weak.h"


/***************************************************************************/
/* gets the ABI type from the linker                */
/***************************************************************************/
static void 
IP_set_target(void)
{
#ifdef _TARG_MIPS
    switch (ld_ipa_opt[LD_IPA_TARGOS].flag) {
    case TOS_MIPS_O32:
	Target_ABI = ABI_32;
	break;
	
    case TOS_MIPS_N32:
	Target_ABI = ABI_N32;
	break;

    case TOS_MIPS_64:
	Target_ABI = ABI_64;
	break;
    default:
	Target_ABI = ABI_N32;
	break;
    }

    switch (ld_ipa_opt[LD_IPA_ISA].flag) {
    case 1:
	Target_ISA = TARGET_ISA_M1;
	break;
    case 2:
	Target_ISA = TARGET_ISA_M2;
	break;
    case 3:
	Target_ISA = TARGET_ISA_M3;
	break;
    case 4:
	Target_ISA = TARGET_ISA_M4;
	break;
    default:
	break;				// use default
    }

    Use_32_Bit_Pointers = (Target_ABI < ABI_64);
#endif

#ifdef TARG_IA64
    Target_ABI = ABI_I64;
    Target_ISA = TARGET_ISA_I1;
    Use_32_Bit_Pointers = FALSE;
#endif

#ifdef _TARG_IA32
    Target_ABI = ABI_I32;
    Target_ISA = TRUE;
#endif

#ifdef TARG_X8664
    Target_ABI = IPA_Target_Type == IP_64_bit_ABI ? ABI_n64 : ABI_n32;
    Target_ISA = TARGET_ISA_x86_64;
    Use_32_Bit_Pointers = IPA_Target_Type == IP_32_bit_ABI;
#endif

    IPA_Configure_Target ();
}


static MEM_POOL Type_Merge_Pool;

static BOOL ipa_dot_so_initialized = FALSE;

void
ipa_dot_so_init ()
{
    Temporary_Error_Phase ephase ("IPA initialization");

    ipa_dot_so_initialized = TRUE;

    MEM_Initialize();
    Preconfigure ();
    IP_set_target();
    Dont_Use_WN_Free_List ();

    Init_Operator_To_Opcode_Table ();
    Initialize_Symbol_Tables (TRUE);
    Initialize_Auxiliary_Tables ();
    
    MEM_POOL_Initialize (&Type_Merge_Pool, "TY Merge Pool", 0);
    Initialize_Type_Merging_Hash_Tables (&Type_Merge_Pool);
	
    Set_FILE_INFO_ipa (File_info);	// mark the symtab IPA-generated
    
    if (ld_ipa_opt[LD_IPA_SHARABLE].flag == F_CALL_SHARED_RELOC)
        IPA_Enable_Relocatable_Opt = TRUE;

} /* ipa_dot_so_init */

#ifdef KEY
// Returns number of processors on success, otherwise returns 0
static int get_num_procs (void)
{
  FILE * fp;
  char buf[256];
  int cpus = 0;
  
  if ((fp = fopen ("/proc/cpuinfo", "r")) == NULL)
    return 0;

  while (fgets (buf, 256, fp))
  {
    if (!strncasecmp (buf, "processor", 9))
      cpus += 1;
  }

  fclose (fp);
  return cpus;
}
#endif // KEY

void
ipa_driver (INT argc, char **argv)
{
    if (! ipa_dot_so_initialized)
	// not a single WHIRL object has been found.
	ipa_dot_so_init ();

    Verify_Common_Block_Layout ();
	
    Clear_Extra_Auxiliary_Tables ();
    
    MEM_POOL_Delete (&Type_Merge_Pool);

    // turn off these features until they are ported.
    IPA_Enable_Cloning = FALSE;
#ifdef TARG_X8664
    IPA_Enable_AutoGnum = FALSE;
#else
    IPA_Enable_AutoGnum = TRUE;
#endif
#if 1
    IPA_Enable_DST = FALSE;
#else
    IPA_Enable_DST = TRUE;
#endif

    Process_IPA_Options (argc, argv);

#ifdef KEY
    if (Annotation_Filename == NULL ) // no feedback
      IPA_Enable_PU_Reorder = REORDER_DISABLE;

    // Enable parallel backend build after ipa
    if (IPA_Max_Jobs == 0)
      IPA_Max_Jobs = get_num_procs ();
#endif // KEY

    create_tmpdir ( Tracing_Enabled || List_Cite );

#ifdef TODO
    if (IPA_Target_Type == IP_32_bit_ABI)
        IP_update_space_status32();
    else
        IP_update_space_status64();
#endif

    if (ld_ipa_opt[LD_IPA_SHARABLE].flag & F_STATIC) {
	IPA_Enable_Picopt = FALSE;
	IPA_Enable_AutoGnum = FALSE;
    }

    if (IPA_Enable_Picopt || IPA_Enable_Relocatable_Opt) {
	Pic_optimization ();
    } else {
	Fix_up_static_functions ();
    }

    Perform_Interprocedural_Optimization ();
   
} /* ipa_driver */



/* preempt the definition in be.so, so that we can call ld's cleanup
   routines */
/*ARGSUSED*/
void
Signal_Cleanup (INT sig)
{
    Cleanup_Files (FALSE, TRUE);

    /* now do the ld part */
    /* we fake a fatal error instead of copying all the cleanup code here */

#ifdef KEY
    // Let caller do exit(1) as necessary, because not all callers want
    // exit(1).  For example, if ipa_link calls ErrMsg to report a user error,
    // then the correct behavior is to have the error trickle down to
    // ErrMsg_Report_User which calls this Signal_Cleanup.  Execution should go
    // back to ErrMsg_Report_User which will then do
    // exit(RC_NORECOVER_USER_ERROR), which will tell the pathcc driver that
    // it's an user error.  On the other hand, if we always do exit(1), this
    // will mislead the pathcc driver to emit an "internal compiler error"
    // message even when it's user error.
    fprintf(stderr,"IPA processing aborted\n");
    return;
#endif

#if defined(TARG_IA64)
    fprintf(stderr,"IPA processing aborted");
    exit(1);
#else
    msg (ER_FATAL, ERN_MESSAGE, "IPA processing aborted");
#endif
} /* Signal_Cleanup */
