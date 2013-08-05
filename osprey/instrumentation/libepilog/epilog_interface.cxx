//-*-c++-*-

/*
 * Copyright 2003, 2004, 2005 PathScale, Inc.  All Rights Reserved.
 */

// ====================================================================
// ====================================================================
//
// Module: profile_interface.cxx
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
// During instrumentation, calls to the following procedures are
// inserted into the WHIRL code.  When invoked, these procedures
// initialize, perform, and finalize frequency counts.
//
// ====================================================================
// ====================================================================


#include <stdio.h>
#include <vector>
#include <Profile/Profiler.h>
// using namespace tau;
//#include <omp.h>
using std::vector;

#include "profile.h"
#include "epilog_interface.h"
#include "profile_errors.h"
#include "dump.h"

//omp_lock_t tau_regdescr_lock;
// ====================================================================


static char *output_filename = NULL;
static BOOL unique_output_filename = FALSE; 


// ====================================================================


// One time initialization

/* void tau_create_top_level_timer_if_necessary__() {
Tau_create_top_level_timer_if_necessary();
}*/
void __profile_init(struct profile_init_struct *d)
{
  static bool first_called=TRUE;

  if(first_called) {

    first_called=FALSE;

 //    omp_init_lock(&tau_regdescr_lock);
    //TAU_PROFILE_SET_NODE(0);
  }
  

/*
  char *fname = d->output_filename;
  int phase_num = d->phase_num;
  BOOL unique_name = d->unique_name;


  PROFILE_PHASE curr_phase_num;

  void (*pf)() = __profile_finish;
  static bool first_call_to_profile_init = true;
  if (first_call_to_profile_init) {

     atexit(pf);

     output_filename = new char[strlen(fname) + 7 + 1];
     strcpy(output_filename, fname); 
     if (unique_name)
	 strcat (output_filename, ".XXXXXX");
     unique_output_filename = unique_name;
     first_call_to_profile_init = false;
  }

  curr_phase_num = Instrumentation_Phase_Num();

  if (curr_phase_num == PROFILE_PHASE_NONE) {
     Set_Instrumentation_Phase_Num((PROFILE_PHASE) phase_num);
  } else if(curr_phase_num != (PROFILE_PHASE) phase_num) {
    profile_warn("Phase Number already set to a different value in: %s",
		 output_filename);
  }
  */
}


// PU level initialization to gather profile information for the PU.
// We call atexit during the first call to this routine to ensure
// that at exit we remember to destroy the data structures and dump
// profile information.
// Also, during the first call, a profile handle for the PU is created.
// During subsequent calls, teh PC address of the PU is used to access
// a hash table and return the profile handle that was created during the 
// first call.

void *
__profile_pu_init(char *file_name, char* pu_name, long current_pc,
		  INT32 pusize, INT32 checksum)
{
 
}


// For a PU, initialize the data structures that maintain 
// invokation profile information.

void
__profile_invoke_init(void *pu_handle, INT32 num_invokes)
{
 
}


// Gather profile information for a conditional invoke


void __profile_invoke_exit(struct profile_gen_struct *d)
{

 /* void *pu_handle = d->pu_handle; 
  INT32 invoke_exit_id = d->id; 
  char *pu_name = d->pu_name;
  INT32 linenum = d->linenum; 
  char *file_name = d->file_name; */
 // TAU_GLOBAL_TIMER_STOP();  
   Tau_stop_timer(d->data,Tau_get_tid());
}
void
__profile_invoke(struct profile_gen_struct *d)
{


  void *pu_handle = d->pu_handle; 
  INT32 invoke_id = d->id;
 char *pu_name = d->pu_name;
 INT32 linenum = d->linenum; 
 INT32 endline = d->endline; 
 char *file_name = d->file_name;
 INT32 taken = d->taken; 
 INT32 target = d->target;
 INT32 num_targets = d->num_targets;
 void **data = &(d->data);
 void *called_fun_address = d->called_fun_address;

 Tau_profile_c_timer(data, d->pu_name, "", TAU_DEFAULT, "TAU_DEFAULT");  
 Tau_start_timer(d->data, 0,Tau_get_tid()); 
/* 
printf("%d,%d,%d,%d,%d,%d",invoke_id,linenum,endline,taken,target,num_targets);
 if (pu_handle==NULL) printf("pu_handle=NULL,");
 else printf("pu_handle=NOT_NULL,");
 
 if(data==NULL && called_fun_address==NULL) printf("data=fun=NULL");
		    else printf("data=fun=NOTNULL");
 
 */
   
 //   Tau_create_top_level_timer_if_necessary();
 
  
 //   omp_set_lock(&tau_regdescr_lock);
 /*    if(d->data!=NULL)
    { 
       Profiler *p = new Profiler ((FunctionInfo *)d->data, TAU_DEFAULT, true, RtsLayer::myThread());
       p->Start();

     }
     else
    {
    char rname[256], rtype[1024];
    sprintf(rname, "%s", pu_name);
    sprintf(rtype, "[file:%s <%d, %d>]",
        file_name, linenum, endline);
    
    FunctionInfo *f = new FunctionInfo(rname, rtype, TAU_DEFAULT, "TAU_DEFAULT");
    d->data = (void *) f;
    Profiler *p = new Profiler (f, TAU_DEFAULT, true, RtsLayer::myThread());
    p->Start();
    }
  //  omp_unset_lock(&tau_regdescr_lock);
   */
 
}


// For a PU, initialize the data structures that maintain 
// conditional branch profile information.

void
__profile_branch_init(void *pu_handle, INT32 num_branches)
{
 
}


// Gather profile information for a conditional branch
void __profile_branch_exit(struct profile_gen_struct *d)
{
  void *pu_handle = d->pu_handle;
  INT32 branch_id = d->id;
  bool taken = d->taken;
  BranchSubType subtype = (BranchSubType) d->subtype;
  INT32 linenum = d->linenum;
  INT32 endline = d->endline;
  char *file_name = d->file_name;
 
}


void __profile_branch(struct profile_gen_struct *d)
{
  void *pu_handle = d->pu_handle;
  INT32 branch_id = d->id;
  bool taken = d->taken;
  BranchSubType subtype = (BranchSubType) d->subtype;
  INT32 linenum = d->linenum;
  INT32 endline = d->endline;
  char *file_name = d->file_name;

 
}


// For a PU, initialize the data structures that maintain 
// switch profile information.

void
__profile_switch_init(void *pu_handle,
		      INT32 num_switches,    INT32 *switch_num_targets,
		      INT32 num_case_values, INT64 *case_values)
{
 
}


// Gather profile information for an Switch

void
__profile_switch(struct profile_gen_struct *d)
{

   void *pu_handle = d->pu_handle;
  INT32 switch_id = d->id;
  INT32 num_targets = d->num_targets;
  INT32 target = d->target;
  INT32 linenum = d->linenum;
  INT32 endline = d->endline;
  char *file_name = d->file_name;

 }

void
__profile_switch_exit(struct profile_gen_struct *d)
{
  void *pu_handle = d->pu_handle;
  INT32 switch_id = d->id;
  INT32 linenum = d->linenum;
  INT32 endline = d->endline;
  char *file_name = d->file_name;

}
// For a PU, initialize the data structures that maintain 
// compgoto profile information.

void
__profile_compgoto_init(void *pu_handle, INT32 num_compgotos,
			INT32 *compgoto_num_targets)
{

 
}


// Gather profile information for an Compgoto

void
__profile_compgoto(struct profile_gen_struct *d)
{
    void *pu_handle = d->pu_handle;
  INT32 compgoto_id = d->id;
  INT32 num_targets = d->num_targets;
  INT32 target = d->target;
  INT32 linenum = d->linenum;
  INT32 endline = d->endline;
  char *file_name = d->file_name;
 
}

#ifdef KEY
// For a PU, initialize the data structures that maintain 
// value profile information.

void __profile_value_init( void *pu_handle, INT32 num_values )
{
 
}


// Gather profile information for a Value

void
__profile_value( void *pu_handle, INT32 inst_id, INT64 value )
{
 
}

// For a PU, initialize the data structures that maintain value (FP) profile 
// information for both the operands of a binary operator.

void __profile_value_fp_bin_init( void *pu_handle, INT32 num_values )
{
 
}


// Gather profile information for Values (FP)

void
__profile_value_fp_bin( void *pu_handle, INT32 inst_id, 
			double value_fp_0, double value_fp_1 )
{
 
}
#endif

// For a PU, initialize the data structures that maintain 
// loop profile information.

void 
__profile_loop_init(void *pu_handle, INT32 num_loops)
{
}


// Gather profile information for a loop entry

void __profile_loop(struct profile_gen_struct *d)
{
  /*
  //   omp_set_lock(&tau_regdescr_lock);
      if(d->data!=NULL)
    { 
       Profiler *p = new Profiler ((FunctionInfo *)d->data, TAU_DEFAULT, true, RtsLayer::myThread());
       p->Start();

     }
       else

   {
    INT32 loop_id = d->id;
    INT32 linenum = d->linenum;
    INT32 endline = d->endline;
    char *file_name = d->file_name;
    char rname[256], rtype[1024];
    sprintf(rname, "%s%d", "LOOP #",loop_id);
    sprintf(rtype, "[file:%s <%d, %d>]",
        file_name, linenum, endline);
    
    FunctionInfo *f = new FunctionInfo(rname, rtype, TAU_DEFAULT, "TAU_DEFAULT");
    d->data = (void *) f;
    Profiler *p = new Profiler (f, TAU_DEFAULT, true, RtsLayer::myThread());
    p->Start();
   
    }
  // omp_unset_lock(&tau_regdescr_lock);
  */
   
    char rname[256], rtype[1024];
    sprintf(rname, "%s%d", "LOOP #",d->id);
    sprintf(rtype, "[file:%s <%d, %d>]",
        d->file_name, d->linenum, d->endline);
   void **data = &(d->data);
   Tau_profile_c_timer(data, rname, rtype, TAU_DEFAULT, "TAU_DEFAULT");
   Tau_start_timer(d->data, 0,Tau_get_tid());
  
}

void
__profile_loop_exit(struct profile_gen_struct *d)
{
 /*
  void *pu_handle = d->pu_handle;
  INT32 loop_id = d->id;
  INT32 linenum = d->linenum;
  INT32 endline = d->endline;
  char *file_name = d->file_name;
 */
 // TAU_GLOBAL_TIMER_STOP(); 
 //Tau_stop_timer(var);
  Tau_stop_timer(d->data,Tau_get_tid());
}

// Gather profile information from a Loop Iteration

void
__profile_loop_iter(struct profile_gen_struct *d) 
{
  void *pu_handle = d->pu_handle;
  INT32 loop_id = d->id;
  INT32 linenum = d->linenum;
  INT32 endline = d->endline;
  char *file_name = d->file_name;
 
}


// For a PU, initialize the data structures that maintain
// short circuit profile information.

void 
__profile_short_circuit_init(void *pu_handle, INT32 num_short_circuit_ops)
{

}


// Gather profile information for the right operand of a short circuit op

void 
__profile_short_circuit(struct profile_gen_struct *d)
{
  void *pu_handle = d->pu_handle;
  INT32 short_circuit_id = d->id;
  bool taken = d->taken;
  char *file_name = d->file_name;
}


// For a PU, initialize the data structures that maintain
// call profiles.

void 
__profile_call_init(void *pu_handle, int num_calls)
{
}

// For a PU, initialize the data structures that maintain
// icall profiles.

void 
__profile_icall_init(void *pu_handle, int num_icalls)
{
}

// Gather the entry count for this call id

void 
 __profile_call_entry(struct profile_gen_struct *d)
{
  void *pu_handle = d->pu_handle;
  INT32 call_id = d->id;
}


// Gather the exit count for this call id

void 
__profile_call_exit(struct profile_gen_struct *d)
{  
   void *pu_handle = d->pu_handle;
  INT32 call_id = d->id;

}

void
__profile_icall_entry(struct profile_gen_struct *d)
{
  void *pu_handle = d->pu_handle;
  INT32 icall_id = d->id;
  void *called_fun_address = d->called_fun_address;

}
void
__profile_icall_exit(struct profile_gen_struct *d)
{

}

// At exit processing to destroy data structures and dump profile
// information.

void __profile_finish(void)
{
  /*
  FILE *fp;
  HASH_MAP::iterator i;

  if (unique_output_filename) {
      int file_id = mkstemp (output_filename);
      fp = fdopen (file_id, "w+");
  } else
      fp = fopen (output_filename, "w+");

  if (fp == NULL) {
     profile_error("Unable to open file: %s", output_filename);
  } 

  Dump_all(fp, output_filename);
  
#if 0  // so the fini routine won't core dump if it access instrumented code
  // This fix is a patch so that the mongoose compiler can instrument itself
  // The problem is that C++ code appears to result in other __profile
  // procedures being called after __profile_finish.  For 7.4, we should
  // find out why this is happening, and then restore this code.
  for(i = PU_Profile_Handle_Table.begin();
      i != PU_Profile_Handle_Table.end(); i++) {
    PU_PROFILE_HANDLE pu_handle = (*i).second;
    delete pu_handle;
  }
#endif

  fclose(fp);

  delete [] output_filename;

  */
}
