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


// This may look like C code, but it is really -*- C++ -*-

#ifdef USE_PCH
#include "lno_pch.h"
#endif // USE_PCH
#pragma hdrstop

// laks: hack stupidity
#define INT32_MAX   0x7fffffff
// maximum we only support 2048 characters !! please modify this !
#define UH_MAX_CHARS 2048

#include <map>

#include <sys/types.h>
#include <ctype.h>
#include <limits.h>
#include <alloca.h>

#include "wn.h"
#include "wb_util.h"
#include "frac.h"
#include "ara.h"
#include "pu_info.h"
#include "opt_du.h"
#include "lnoutils.h"
#include "lwn_util.h"
#include "config.h" 
#include "config_lno.h"
//#include "anl_driver.h"
//#include "prompf.h"
#include "glob.h"
#include "parids.h"
#include "cond.h"
#include "ara_region.h"
#include "ara_loop.h"
#include "fiz_fuse.h"

// laks 2007.02.01 add utils 
#include "uh_util.h"
#pragma weak Anl_File_Path

//-------------------------------
extern MEM_POOL ARA_memory_pool;  // defined in ara_loop.h

//-------------------------------
static void UH_Print_Def_Use(WN *wn, FILE *fp, char *strBuf);
static void UH_Print_Defs(WN* wn, FILE* fp, char *strBuf);
static void UH_Print_Uses(OPERATOR opr, WN* wn, FILE* fp, char *strBuf);
static BOOL UH_Is_Lex_After(WN* wn1, WN* wn2);

//-------------------------------
static BOOL UH_IsVarUsedAfterRegion(WN* wnVar, WN* wnRegion);

//-------------------------------
//map<WN*,>
//-------------------------------
//-------------------------------


//------------------------------
// XML version of autoscope. Please call this function when the USE-DEF list has been created !!!:w
// For instance, in Perform_Ara_and_Parallelization.
// And please call this function BEFORE ARA_LOOP_INFO::generate_pragma (the latter will erase some info!)
void
ARA_LOOP_INFO::Autoscope()
{
  char sPrivate[UH_MAX_CHARS], sLastprivate[UH_MAX_CHARS], sShared[UH_MAX_CHARS], sReduction[UH_MAX_CHARS];
  char sFirstprivate[UH_MAX_CHARS];
  std::map<WN*, PAR_LOOP_STATUS>::iterator itr;
  int iPrivate = 0, iFirstprivate=0, iLastprivate=0, iShared=0, iReduction=0;

  // INITIALIZATION
  strcpy(sPrivate,"");
  strcpy(sFirstprivate,"");
  strcpy(sLastprivate,"");
  strcpy(sShared,"");
  strcpy(sReduction,"");

  

  // Adjust Suggested_Parallel for convexity considerations. 
  DO_LOOP_INFO* dli = Get_Do_Loop_Info(_loop); 

    // ------- Privatizable arrays: lastprivate, private or shared ?
    INT i, j;
    for (i = 0; i < _pri.Elements(); ++i) {
      if (_pri.Bottom_nth(i)->Is_Loop_Invariant() && 
	  !_pri.Bottom_nth(i)->Is_Unknown_Size()) {
	if (_pri.Bottom_nth(i)->Need_Last_Value()) {
	  sprintf(sLastprivate,"%s %s",sLastprivate, _pri.Bottom_nth(i)->Array().Prompf_Name());
	  iLastprivate++;
	} else { 
	  //if(ST_class((_pri.Bottom_nth(i)->Array().St()))==CLASS_PREG)
   	  char *ptr_c=_pri.Bottom_nth(i)->Array().Prompf_Name();
	  if(ptr_c[0]!='<')
	    sprintf(sPrivate,"%s %s",sPrivate, _pri.Bottom_nth(i)->Array().Prompf_Name());
  	  iPrivate++;
	} 
      } else if(_pri.Bottom_nth(i)->Need_Last_Value()) {
	sprintf(sShared,"%s %s",sShared, _pri.Bottom_nth(i)->Array().Prompf_Name());
	iShared++;
      } else { // no lastvalue is needed firstprivate
	sprintf(sFirstprivate,"%s %s",sFirstprivate, _pri.Bottom_nth(i)->Array().Prompf_Name());
	iFirstprivate++;
      }
    }
    
    // Privatizable scalars: lastprivate or private
    for (i = 0; i < _scalar_pri.Elements(); ++i) {
      if (_scalar_last_value.Bottom_nth(i)) {
	sprintf(sLastprivate,"%s %s",sLastprivate, _scalar_pri.Bottom_nth(i)->_scalar.Prompf_Name());
	  iLastprivate++;
      }
      else {
	//if(ST_class((_scalar_pri.Bottom_nth(i)->_scalar.St()))==CLASS_PREG)
   	char *ptr_c= _scalar_pri.Bottom_nth(i)->_scalar.Prompf_Name();
	if(ptr_c[0]!='<')
	  sprintf(sPrivate,"%s %s",sPrivate, _scalar_pri.Bottom_nth(i)->_scalar.Prompf_Name());
  	iPrivate++;
      }
    }

    // Use arrays: normally shared
    for (i = 0; i < _use.Elements(); ++i) {
      if (Overlap_Local_Array(_use.Bottom_nth(i)->Array(), _use.Bottom_nth(i)->Offset()))
        FmtAssert(FALSE,("ARA_LOOP_INFO::Autoscope, exposed use overlaps with local array, need renaming"));
      sprintf(sShared,"%s %s",sShared, _use.Bottom_nth(i)->Array().Prompf_Name());
	iShared++;
    }

    // Use scalars: normally shared
    for (i = 0; i < _scalar_use.Elements(); ++ i) {
      if (Overlap_Local_Scalar(_scalar_use.Bottom_nth(i)->_scalar))
	//fprintf(stderr,">Autoscope: Error overlap scalar: %s\n ",_scalar_use.Bottom_nth(i)->_scalar.Prompf_Name());
      if (!Overlap_Reduction_Scalar(_scalar_use.Bottom_nth(i)->_scalar)) {
	// laks added 08.24.06 to verify if there is firstprivate needed
	SCALAR_NODE *snUse = _scalar_use.Bottom_nth(i);
	for(j=0; j<snUse->Elements(); ++j) {
	  WN* wnUse = snUse->Bottom_nth(j)->Wn;
	  // verify if the variable is used after the parallel region
	  if (UH_IsVarUsedAfterRegion(wnUse, _loop)) {
		// Yes, it has been used
            sprintf(sShared,"%s %s",sShared, _scalar_use.Bottom_nth(i)->_scalar.Prompf_Name());
	    iShared++;
	    break;
	  }
 	}
	// No, it isn't used. Thus, it should be firstprivate
	if(j >= snUse->Elements()) {
	  char *sVar = _scalar_use.Bottom_nth(i)->_scalar.Prompf_Name();
	  // Laks 2007.03.09: for unknown reason, the compiler replace some variables with <DEDICATED PREG> taf
	  // 	this tag will cause error in XML file. Therefore, we have to eliminate it here.
	  //	(Apparently only appears here). 
	  if(sVar[0] != '<')
            sprintf(sFirstprivate,"%s %s",sFirstprivate, _scalar_use.Bottom_nth(i)->_scalar.Prompf_Name());
	iFirstprivate++;
	}
      }
    }

    // Defined arrays: lastprivate or shared
    for (i = 0; i < _def.Elements(); ++ i) {
      ARA_REF *cur = _def.Bottom_nth(i);
      if (!Overlap_Local_Array(cur->Array(), cur->Offset()) && 
	  !Overlap_Exposed_Array(cur->Array(), cur->Offset())) {
	if (cur->Is_Loop_Invariant() && !cur->Is_Unknown_Size()) {
	  sprintf(sLastprivate,"%s %s",sLastprivate, cur->Array().Prompf_Name());
	  iLastprivate++;
        } else if(cur->Is_Loop_Invariant() &&  !cur->Need_Last_Value()) {
          sprintf(sFirstprivate,"%s %s",sFirstprivate, cur->Array().Prompf_Name());
	} else {  // laks 08.24.06: no last value is needed: firstprivate
          sprintf(sShared,"%s %s",sShared, cur->Array().Prompf_Name());
	  iShared++;
	} 
      }
    }
	
    // defined scalars: lastprivate
    for (i = 0; i < _scalar_def.Elements(); ++ i) {
      if (!Overlap_Local_Scalar(_scalar_def.Bottom_nth(i)->_scalar) && 
	  !Overlap_Exposed_Scalar(_scalar_def.Bottom_nth(i)->_scalar)) {
	sprintf(sLastprivate,"%s %s",sLastprivate, _scalar_def.Bottom_nth(i)->_scalar.Prompf_Name());
	iLastprivate++;
      }
    }

    // may defined scalars: shared
    for (i = 0; i < _scalar_may_def.Elements(); ++ i) {
      if (!Overlap_Local_Scalar(_scalar_may_def.Bottom_nth(i)->_scalar) &&
	  !Overlap_Exposed_Scalar(_scalar_may_def.Bottom_nth(i)->_scalar) &&
	  !Overlap_Kill_Scalar(_scalar_may_def.Bottom_nth(i)->_scalar) &&
	  !Overlap_Reduction_Scalar(_scalar_may_def.Bottom_nth(i)->_scalar)) {
	SCALAR_NODE *snDef = _scalar_may_def.Bottom_nth(i);
	for(j=0; j<snDef->Elements(); ++j) {
	  WN* wnDef = snDef->Bottom_nth(j)->Wn;
	  // laks 08.29.06: attempt to identify the firstprivate
	  if (UH_IsVarUsedAfterRegion(wnDef, _loop)) {
            sprintf(sShared,"%s %s",sShared, _scalar_may_def.Bottom_nth(i)->_scalar.Prompf_Name());
	    iShared++;
	    break;
	  }
 	}
	if(j >= snDef->Elements())
          sprintf(sFirstprivate,"%s %s",sFirstprivate, _scalar_may_def.Bottom_nth(i)->_scalar.Prompf_Name());
	  iFirstprivate++;
      }
    }

    // reduction
    for (i = 0; i < _reduction.Elements(); ++i) {
      WN* cur_wn = _reduction.Bottom_nth(i);
      if(OPCODE_has_sym( WN_opcode(cur_wn))) {
        SYMBOL symb(cur_wn);
        sprintf(sReduction,"%s %s",sReduction, symb.Prompf_Name());
	iReduction++;
        if (WN_operator(cur_wn) == OPR_ISTORE) {
        } else {  
       } 
      }
    }
    if (dli->Is_Doacross) {
    }

  // let summarize the situation
  itr = mapLoopStatus.find(_loop);
  if(itr != mapLoopStatus.end()) {
    // the loop exists in the database 
    itr->second.sPrivate = sPrivate;
    itr->second.sFirstPrivate = sFirstprivate;
    itr->second.sLastPrivate = sLastprivate;
    itr->second.sShared = sShared;
    itr->second.sReduction = sReduction;
    
    itr->second.iPrivate = iPrivate;
    itr->second.iFirstPrivate = iFirstprivate;
    itr->second.iLastPrivate = iLastprivate;
    itr->second.iShared = iShared;
    itr->second.iReduction = iReduction;
  } else {
    // inexistant
    PAR_LOOP_STATUS objStatus;
    objStatus.sPrivate = (sPrivate);
    objStatus.sFirstPrivate = (sFirstprivate);
    objStatus.sLastPrivate = (sLastprivate);
    objStatus.sShared = (sShared);
    objStatus.sReduction =  (sReduction);

    objStatus.iPrivate = iPrivate;
    objStatus.iFirstPrivate = iFirstprivate;
    objStatus.iLastPrivate = iLastprivate;
    objStatus.iShared = iShared;
    objStatus.iReduction = iReduction;

    mapLoopStatus[_loop] = objStatus;
  }

    for (INT i = 0; i < _children.Elements(); ++i) 
      _children.Bottom_nth(i)->Autoscope();
}

//-----------------------------------------------------------------
static void UH_WorkEstimation(WN *wnLoop) {
  WN* wn_outer = wnLoop;
  DO_LOOP_INFO* dli = Get_Do_Loop_Info(wn_outer);
  if (dli->Suggested_Parallel && dli->Work_Estimate == 0)
    printf("Work Estimate for loop %s at %d is 0", WB_Whirl_Symbol(wn_outer),
      (INT) UH_GetLineNumber(wn_outer));
  if (!dli->Suggested_Parallel) {
      // This can only happen in the case that Robert's and Peng's
      // selections of which loop to parallelize are not in agreement.
      // This problem, hence the following code, should go away in the
      // future--DRK.

//    dli->Work_Estimate = Single_Iteration_Cost(wn_outer, TRUE);
    printf("Parallelizing Unexpected Loop: Using Work Estimate of %.2f",
      dli->Work_Estimate);
  }

  ARA_LOOP_INFO *ali = dli->ARA_Info;
  // Never enough work.
  if (ali->Not_Enough_Parallel_Work())
    return; //LWN_Make_Icon(MTYPE_I4, 0);

  // Always enough work.
  BOOL has_left_right = FALSE;
  INT left = -1;
  INT right = -1;
//  if (Always_Enough_Parallel_Work(&has_left_right, &left, &right))
//    return LWN_Make_Icon(MTYPE_I4, 1);


}

//-----------------------------------------------------------------------
// NAME: Is_Lex_Before 
// FUNCTION: Returns TRUE if 'wn1' is lexically before 'wn2', FALSE
//   otherwise. 
//-----------------------------------------------------------------------
extern BOOL UH_Is_Lex_After(WN* wn1, WN* wn2)
{
   return Is_Lex_Before(wn2, wn1);
} 

//-----------------------------------------------------------------
// Function to verify if the variable is used outside a region
// INPUT: 
//  - wnVar, a variale WN
//  - wnRegion, a region (such as a loop or a statemetn block 
// OUTPUT:
//  BOOL TRUE if the variable is used outside, FALSE otherwise
//-----------------------------------------------------------------
BOOL UH_IsVarUsedAfterRegion(WN* wnVar, WN* wnRegion)
{
  if(wnVar == NULL)
    return FALSE;
  USE_LIST *uses = Du_Mgr->Du_Get_Use(wnVar);
  if (uses == NULL) {
      return FALSE;
  }
  USE_LIST_ITER iter(uses);
  for(const DU_NODE *node=iter.First();!iter.Is_Empty();node=iter.Next()){
     WN *wnUse = (WN *) node->Wn();
     if(wnUse) {
	if(UH_Is_Lex_After(wnUse, wnRegion))
	  return TRUE;
     }
  }
  return FALSE;
}

//-----------------------------------------------------------------
// Print out the def-use chains for the whole procedure
//-----------------------------------------------------------------
void UH_Print_Def_Use(WN *wn, FILE *fp, char *strBuf)
{
  OPCODE opcode = WN_opcode(wn);

//  UH_Print_Defs(wn, stdout, strBuf);

  if (opcode == OPC_BLOCK) {
    WN *kid = WN_first (wn);
    strcat(strBuf, "  ");
    while (kid) {
      UH_Print_Def_Use(kid,fp, strBuf);
      kid = WN_next(kid);
    }
  } else {
    if (opcode != OPC_IO) {
      for (INT kidno=0; kidno<WN_kid_count(wn); kidno++) {
        WN *kid = WN_kid(wn,kidno);
        if (kid) UH_Print_Defs(kid, stdout, strBuf);
        if (kid) UH_Print_Def_Use(kid,fp, strBuf);
      }
    }

  }
}

//-----------------------------------------------------------------
// Print the use
//-----------------------------------------------------------------
void UH_Print_Defs(WN* wn, FILE* fp, char *strBuf)
{
   // List all defs
  DEF_LIST *defs = Du_Mgr->Ud_Get_Def(wn);
  OPERATOR opr = WN_operator(wn); 
  WN *loop_stmt = NULL;
  if (defs) {
    loop_stmt = defs->Loop_stmt();
  } else {
    if (opr == OPR_LDID) DevWarn("WARNING NO DEF LIST ");
  }
  if (loop_stmt) {
    if (WN_opcode(loop_stmt)==OPC_DO_LOOP) {
      fprintf(fp,"%s %d:LOOP\n", strBuf, Srcpos_To_Line(WN_linenum(wn)));
    } else {
       DevWarn("WARNING loop_stmt is not a DO_LOOP (0x%p,ls=0x%p)",
              wn, loop_stmt);
    }
  }
  if (defs && defs->Incomplete()) {
    fprintf(fp,"Its def list is incomplete \n");
  }
  DEF_LIST_ITER iter(defs);
  if (iter.Is_Empty() && (opr == OPR_LDID)) {
    DevWarn("WARNING Empty DEF LIST ");
  } else if (iter.Is_Empty())
    return;
  fprintf(fp,"%s %d:%s the defs are:\n", strBuf, UH_GetLineNumber(wn), OPERATOR_name(opr));
  for(const DU_NODE *node=iter.First();!iter.Is_Empty();node=iter.Next()){
    WN *def = (WN *) node->Wn();
    fprintf(fp, "%s %d:%s used by:\n", strBuf, UH_GetLineNumber(def), SYMBOL(def).Name()  );
    UH_Print_Uses(opr, def, stdout, strBuf);
  }
  fprintf(fp,"\n");
}

//-----------------------------------------------------------------
// Print the use
//-----------------------------------------------------------------
void UH_Print_Uses(OPERATOR opr, WN* wn, FILE* fp, char *strBuf)
{
  // List all uses
  USE_LIST *uses = Du_Mgr->Du_Get_Use(wn);
  if (uses == NULL) {
    if (opr == OPR_STID) DevWarn("WARNING NO USES LIST ");
  }
  USE_LIST_ITER iter(uses);
  if (iter.Is_Empty() && (opr == OPR_STID)) {
     DevWarn("WARNING Empty USE LIST ");
  }
  if (uses && uses->Incomplete()) {
    fprintf(fp,"%s     Its use list is incomplete \n", strBuf);
   }
  fprintf(fp,"%s    ", strBuf);
  for(const DU_NODE *node=iter.First();!iter.Is_Empty();node=iter.Next()){
     WN *use = (WN *) node->Wn();
     int iLine = UH_GetLineNumber(use);
     if (iLine>0) {
       fprintf(fp, " %d ", iLine );
      }
   }
   fprintf(fp,"\n");
}


//-----------------------------------------------------------------

