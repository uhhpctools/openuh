

/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.
  Copyright (C) 2006, 2007 University of Houston.  All Rights Reserved.

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


#include <sys/types.h>
#include <ctype.h>
#include <limits.h>
#include <alloca.h>
#include "pu_info.h"
#include "lwn_util.h"
#include "config.h"
#include "srcpos.h"
#include "wn_util.h"
#include "ara.h"
#include "ara_loop.h"
#include "lnopt_main.h"
#include "parmodel.h"
/*i Laks additional feature*/
#include "ir_reader.h"
#include "ir_bread.h"
#include "uh_ara.h"
#include <iostream>
#include <fstream>
#include "srcpos.h"
#include "glob.h"
#include "uh_util.h"

#include <map>
#include <algorithm>
#include <vector>

#include "file_util.h"
#include "uh_be.h"
/* ====================================================================
 * Module: uh_lno.cxx
 * $Revision: 1.1.2.3.2.26 $
 * $Date: 2007-04-02 14:01:08 $
 * $Author: laksono $
 * $Source: /usr/local/cvs/openuh-2.0/kpro64/be/lno/Attic/uh_lno.cxx,v $
 *
 * Revision history:
 *  08.14.06: created by laksono
 *
 * Description:
 * Functions for transforming WHIRL trees based on OpenMP pragmas.
 *
 * Exported functions:
 * ====================================================================*/

#define Set_Parent(wn, p) (WN_MAP_Set(Omp_Parent_Map, wn, (void*)  p))
#define Get_Parent(wn) ((WN*) WN_MAP_Get(Omp_Parent_Map, (WN*) wn))

/*===============================================*/
extern PU_Info *LNO_Current_PU;

/*===============================================*/
static void UH_Parentize (WN* wn);
static WN* UH_CreateDoLoop(WN* wnParent, WN* wnPragma); 
static void Print_Pragmas(WN *pragma_blk);
static void UH_BrowseWhirlNode(WN* pragma_blk);
static void UH_BrowseWhirlNode(WN* wnSrc, char *strBuf);
static void UH_Browse_Loop(ARA_LOOP_INFO *root);
static void UH_Browse_Node_Dump(WN* wn_node, std::string strStart,
                      char *strParent);
static const char* UH_CheckCallOperator(WN *wnStmt, std::string sStart); 
static const char *UH_GetNodeName(WN *wn); 
static const char* UH_PragmaToString(mUINT16 iPragma) ;
/*===============================================*/

static WN_MAP Omp_Parent_Map = WN_MAP_UNDEFINED;
static std::ofstream oFile;
//-----------------------------------------------------------------------
// Open and Writing into a file
// If the file is already created, then open for append
//-----------------------------------------------------------------------
static BOOL UH_WriteToFile(WN *wnNode)
{
   const std::string sEXT = ".xml";
   static std::string sFilebase;
   static BOOL bInit = TRUE;
  // char *fname = Last_Pathname_Component ( Src_File_Name );
   std::string sFilename(Src_File_Name); //(Irb_File_Name);
   //std::string sFilename (Obj_File_Name);

   if(bInit) { // file need to be created
     UH_GetBaseExtName(sFilename,  sFilebase);
     sFilebase = sFilebase + sEXT;
     oFile.open(sFilebase.c_str(), std::ofstream::out);
     bInit = FALSE;
     printf("File %s has been created\n", sFilebase.c_str());
     //std::cout <<"File "<<sFilebase.c_str()<<" has been created."<< std:endl;
     //printf("File %s %s %s %s has been created\n",Src_File_Name,Obj_File_Name,Irb_File_Name,fname);
     
     return TRUE;
   } else {
     oFile.open(sFilebase.c_str(), std::ofstream::app);
     return FALSE;
   }
}


//----------------------------------------------------------------
// Print the cost of the loop to the standard output
// Remark: we assume the argument wn is a loop !! Test by yourself !
//-----------------------------------------------------------------
static void UH_Print_LoopCost(WN *wn, std::string sStart) 
{
  static int iLoop=0;
  double dMachine, dCache, dNumIters;
  double fCost; // temporarity

  char *sOpcodeName = OPCODE_name(WN_opcode(wn));
  int iLine =  Srcpos_To_Line(WN_linenum(wn));
  std::map<WN*, PAR_LOOP_STATUS>::iterator itr;
  bool isDoLoop = ((itr=mapLoopStatus.find(wn))!=mapLoopStatus.end()); 

  if(iLine==0 && isDoLoop) {
    iLine = itr->second.iLine;
  }
  oFile <<sStart<<"<loop line=\""<< iLine <<"\" opcode=\""<< sOpcodeName <<"\"" ;
  if(isDoLoop)
	oFile << " SNL=\"1\" >\n";
  else
	oFile << " SNL=\"0\" >\n";
// ---- loop information
  oFile <<sStart<<"\t<header>\n";
  oFile <<sStart<<"\t\t<index>"<< UH_GetNodeName(WN_index(wn)) <<"</index>\n";
  
  if(WN_operator(wn) == OPR_DO_LOOP) {
    WN *wnLb = WN_LOOP_LowerBound( wn ); /* lower bound of loop */
    OPCODE ub_compare;		/*   and comparison op for ub */
    WN *wnUb = WN_LOOP_UpperBound( wn, &ub_compare ); /* upper bound of loop */
    BOOL is_incr;
    WN *wnIncr = WN_LOOP_Increment( wn, &is_incr );	/* loop increment */
    if (wnLb) {
      oFile <<sStart<<"\t\t<lowerbound>";
      oFile << UH_WNexprToString(wnLb);
      oFile <<"</lowerbound>\n";
    }
    if (wnUb) {
      oFile <<sStart<<"\t\t<upperbound>";
      oFile << UH_WNexprToString(wnUb);
      oFile <<"</upperbound>\n";
    }
    if (wnIncr) {
      oFile <<sStart<<"\t\t<increment>";
      oFile <<(is_incr?"+":"-") <<  UH_WNexprToString(wnIncr);
      oFile <<"</increment>\n";
    }
  } else {
  }
  oFile <<sStart<<"\t</header>\n";

 // ---- parallel status
// std::map<WN*, PAR_LOOP_STATUS>::iterator itr;
// if((itr=mapLoopStatus.find(wn))!=mapLoopStatus.end()) {
 if(isDoLoop) {
   // we find the loop in the database
   PAR_LOOP_STATUS loopStat=itr->second;
	// ---- loop cost
   oFile <<sStart<<"\t<cost>\n";
   oFile <<sStart<<"\t\t<iterations";
   if(loopStat.isIterationSymbolic){
      oFile << " sym=\"0\"";
   } else
      oFile << " sym=\"1\"";
   oFile << ">" << loopStat.nIterations << "</iterations>\n";
   oFile <<sStart<<"\t\t<average>" << loopStat.fCost << "</average>\n";
   oFile <<sStart<<"\t\t<machine>" << loopStat.fMachine << "</machine>\n";
   oFile <<sStart<<"\t\t<cache>" << loopStat.fCache << "</cache>\n";
   oFile <<sStart<<"\t\t<overhead>" << loopStat.fOverhead << "</overhead>\n";
   oFile <<sStart<<"\t\t<total>" << loopStat.fTotal << "</total>\n";
   oFile <<sStart<<"\t</cost>\n";

   oFile <<sStart<<"\t<parallel>"<<std::endl;
   oFile <<sStart<<"\t\t<status>"<<ParStatusStr[loopStat.iStatus]<<"</status>"<<std::endl;
   if(loopStat.iStatus == NOPAR)
	 oFile <<sStart<<"\t\t<reason>"<<loopStat.sReason <<"</reason>"<<std::endl;

   // let print the scope of the variables. Remarque: temp variables are also printed !!!
   oFile <<sStart<<"\t\t<scope>" << std::endl;
   if(loopStat.iPrivate>0)
     oFile <<sStart<<"\t\t\t<private>"<< loopStat.sPrivate <<"</private>" << std::endl;
   if(loopStat.iFirstPrivate>0)
     oFile <<sStart<<"\t\t\t<firstprivate>"<< loopStat.sFirstPrivate <<"</firstprivate>" << std::endl;
   if(loopStat.iLastPrivate>0)
     oFile <<sStart<<"\t\t\t<lastprivate>"<< loopStat.sLastPrivate <<"</lastprivate>" << std::endl;
   if(loopStat.iReduction>0)
     oFile <<sStart<<"\t\t\t<reduction>"<< loopStat.sReduction <<"</reduction>" << std::endl;
   if(loopStat.iShared>0)
     oFile <<sStart<<"\t\t\t<shared>"<< loopStat.sShared <<"</shared>" << std::endl;
   oFile <<sStart<<"\t\t</scope>" << std::endl;

   oFile <<sStart<<"\t</parallel>"<<std::endl;
 } else {
 // std::cout<<"No found loop "<<UH_GetLineNumber(wn)<<" size data:"<<mapLoopStatus.size()<<std::endl;
 }
}
//-----------------------------------------------------------------
// Walk through Openmp parallel region
// INPUT: 
//   - current_pu: current unit
//   - func_nd: WN* of the unit
//-----------------------------------------------------------------
extern WN* UH_PrintUnitInfo(PU_Info* current_pu, WN *func_nd)
{
  char *filename, *dirname;
  SRCPOS srcpos = WN_Get_Linenum(func_nd);
  USRCPOS linepos;
  USRCPOS_srcpos(linepos) = srcpos;
  UH_IPA_IR_Filename_Dirname(srcpos,filename,dirname);  // this is bugged !! it returns wrong filename !
  if (UH_WriteToFile(func_nd)) {
	// open file and prepare to write it down the header
    oFile << "<?xml version=\"1.0\"?>"<<std::endl <<"<source version=\"1.0\">"<<std::endl;
  }
  // ---- start
  oFile << "<unit name=\"" << Cur_PU_Name << "\" file=\""<<filename <<"\" path=\""<<dirname
        <<"\" source=\""<< Src_File_Name <<"\" line=\""<< USRCPOS_linenum(linepos)<<"\">\n";

  // ---- browse the body
  UH_Browse_Node_Dump(func_nd, "", "Main");

  // ---- end
  oFile << "</unit>\n";
  if(!current_pu->next) {
    oFile <<"</source>"; // end of PUs
  }
  oFile.close();
}

//-----------------------------------------------------------------
static const char *UH_GetNodeName(WN *wn) 
{
  if (WN_has_sym(wn)) {
    return ST_name(WN_st(wn));
  } else {
    return OPERATOR_name(OPCODE_operator(WN_opcode(wn)));
  }
} 

//-----------------------------------------------------------------
// Print out the argument of a call function
//-----------------------------------------------------------------
static void UH_SubArguments(WN *wn, int numFormals, std::string sStart)
{
    WN *act;
    ST *stKid;
    int i;
    oFile <<sStart<< "<arguments>\n";
    for (i=0;i<numFormals;i++) {
        act = WN_actual(wn,i);
        if(act) {
          WN *kid = NULL;
          if(WN_operator(act)==OPR_PARM)
            kid = WN_kid(act,0);
          else
            continue;
          if(!kid) {
	    oFile << sStart<< "\t<arg type=\"unknwn\" />\n";
	    continue;
	  }
	  oFile << sStart<< "\t<arg ";
          OPCODE   opc = WN_opcode(kid);
          OPERATOR opr = OPCODE_operator(opc);
          WN *leaf;
          switch(opr) {
            case OPR_ARRAY:
              leaf = WN_array_base(kid);
              if (WN_operator(leaf) == OPR_LDID ||
                WN_operator(leaf) == OPR_LDA) {
		oFile <<"type=\"ARRAY\" name=\""<< UH_GetNodeName(leaf) <<"\"/>\n";
              }
              break;
            case OPR_LDA:
		oFile << "type=\"SCALAR\" name=\""<< UH_GetNodeName(kid) <<"\"/>\n";
              break;
            case OPR_CONST:
              if(OPCODE_rtype(opc) == MTYPE_F4)
		oFile << "type=\"FCONST\" name=\""<< STC_val(WN_st(kid)).vals.fval <<"\"/>\n";
              else if(OPCODE_rtype(opc) == MTYPE_F8)
		oFile << "type=\"DCONST\" name=\""<< STC_val(WN_st(kid)).vals.dval <<"\"/>\n";
              else
	 	oFile << "type=\"UCONST\">";
              break;
            case OPR_INTCONST:
		oFile << "type=\"ICONST\" value=\""<< (int)WN_const_val(kid) << "\"/>\n";
              break;
            default:
		if(OPCODE_has_sym(opc)) {
		  oFile << "type=\"UNK\" name=\""<< UH_GetNodeName(kid) <<"\"/>\n";
		} else {
  		  oFile << "type=\"ARG\" opcode=\""<<OPCODE_name(opc) <<"\" operator=\""<<OPERATOR_name(opr) <<"\" />\n";
		}
              break;
          }
	
       }
    }
    oFile << sStart<< "</arguments>\n";

}

//-----------------------------------------------------------------
// Browse the loop, and print the cost of the loop
//-----------------------------------------------------------------
static void UH_Browse(ARA_LOOP_INFO *root, WN* wnUnit)
{
  std::string my = " ";
  UH_Browse_Node_Dump(wnUnit, my, "Main");
}

//-----------------------------------------------------------------------
static const char* UH_BrowseIf(WN *wn, std::string sStart)
{
  WN *wnIfThen = WN_then(wn);
  oFile <<sStart << "<if line=\""<<Srcpos_To_Line(WN_linenum(wn)) <<"\">" << std::endl;
  oFile <<sStart<<"<ifthen  line=\""<<Srcpos_To_Line(WN_linenum(wnIfThen)) <<"\">\n";
  std::string myStart = sStart + "\t";
  oFile <<sStart<<"\t<kids>"<<std::endl;
  UH_Browse_Node_Dump(wnIfThen, myStart, "ifthen");
  oFile <<sStart<<"\t</kids>"<<std::endl;
  oFile <<myStart<<"</ifthen>" << std::endl;
  if(!WN_else_is_empty(wn)) {
    WN *wnElse = WN_else(wn);
    oFile <<myStart<<"<else line=\""<<Srcpos_To_Line(WN_linenum(wnElse)) <<"\">\n";
    oFile <<myStart<<"\t<kids>"<<std::endl;
    UH_Browse_Node_Dump(wnElse, myStart, "else");
    oFile <<myStart<<"\t</kids>"<<std::endl;
    oFile <<myStart<<"</else>" << std::endl;
  }
  oFile <<sStart << "</if>" << std::endl;
  return NULL; // we need to mark that the WN has been treated and no further browsing is needed
}

//-----------------------------------------------------------------------
// Print an XML tag for any WN node
//-----------------------------------------------------------------------
static const char* UH_PrintXMLnode(WN *wn, std::string sStart) {
   const char *strOp = OPERATOR_name(WN_operator(wn));
   oFile <<sStart<<"<" <<strOp <<" line=\""<<Srcpos_To_Line(WN_linenum(wn)) <<
	 "\" label=\""<< WN_label_number(wn) <<"\">\n";
   return strOp;
}

//-----------------------------------------------------------------------
// Check the WN for further treatement
// return NULL if no treatement if needed (or we do not interest with the WN)
// Note: Please add new opcode treatement here ONLY
//-----------------------------------------------------------------------
static const char* UH_CheckWN(WN *wn, std::string sStart)
{
  /* we only interested with some operators: IF, CALL, LOOPS*/
  int iLine;
  char *sOpcodeName;
  switch(WN_operator(wn)) {
    case OPR_CALL:
	return UH_CheckCallOperator(wn, sStart);
	break;
    case OPR_DO_LOOP:
    case OPR_DO_WHILE:
    case OPR_WHILE_DO:
	UH_Print_LoopCost(wn, sStart);
	return "loop";
	break;
    case OPR_IF:
	return UH_BrowseIf(wn, sStart);
    case OPR_GOTO:
    case OPR_CASEGOTO:
    case OPR_FALSEBR:
    case OPR_SELECT:
    case OPR_XGOTO:
    case OPR_COMPGOTO:
    case OPR_WHERE:
	//oFile <<sStart<<"<" <<OPERATOR_name(WN_operator(wn)) <<" line=\""<<Srcpos_To_Line(WN_linenum(wn)) <<"\">\n";
        //return OPERATOR_name(WN_operator(wn));
   	return UH_PrintXMLnode(wn, sStart);
        break;
    default:
    //    std::cout <<Srcpos_To_Line(WN_linenum(wn))<<":"<<OPERATOR_name(WN_operator(wn))<<"\n";
        break;
  }
  return NULL;
}

//-----------------------------------------------------------------------
// Check if a call site is an MPI routine or not and then print to XML file 
// If it is MPI, then we need to call a more detailed information
//-----------------------------------------------------------------------
static const char* UH_CheckCallOperator(WN *wnStmt, std::string sStart) 
{
    if(WN_operator(wnStmt) == OPR_CALL) {
      int numFormals = WN_num_actuals(wnStmt);
      const char *stName = ST_name(WN_entry_name(wnStmt));

      // check if the call is MPI routine or not
      if ( (stName[0]=='M' || stName[0]=='m') &&
 	   (stName[1]=='P' || stName[1]=='p') &&
	   (stName[2]=='I' || stName[2]=='i') ) {
        oFile <<sStart<<"<mpicall name=\"" << ST_name(WN_entry_name(wnStmt)) << "\" line=\""<< Srcpos_To_Line(WN_linenum(wnStmt))
  	  <<"\" args=\""<< numFormals <<"\"" <<">\n";
        UH_SubArguments(wnStmt, numFormals, sStart + "\t");
	oFile <<sStart<<"</mpicall>"<< std::endl;
        return NULL;
      } else {
	// this is not MPI
        int iLine = Srcpos_To_Line(WN_linenum(wnStmt));
        oFile <<sStart<<"<call name=\""<<ST_name(WN_entry_name(wnStmt))
              <<"\" line=\""<< iLine <<"\"/>\n";
	//return "call";
        return NULL;
      }
    }
  return false;
}

//-----------------------------------------------------------------------
// Browsing the WN
// - wn_node: the WN to browse
// - strStart: the start column position
// - strPArent: this is for debugging purpose
//-----------------------------------------------------------------------
static void UH_Browse_Node_Dump(WN* wn_node, std::string strStart,
                      char *strParent)
{
  char *sName="";
  std::string myStart;

  if (WN_opcode(wn_node) == OPC_BLOCK) {
	// in case of the block
    for (WN* wn = WN_first(wn_node); wn != NULL; wn = WN_next(wn)) {
      sName = (char*) UH_CheckWN(wn, strStart);
      if(sName || (WN_opcode(wn) == OPC_BLOCK) ) {
    	UH_Browse_Node_Dump(wn, strStart,  sName);
      } else if(WN_opcode(wn) == OPC_REGION){
	// find out if this is a pragma region or not
        WN* wn_pragma = WN_first(WN_region_pragmas(wn));
    	if(wn_pragma) {
	  // yes, this is a pragma region !
          oFile <<strStart<<"<pragma line=\"" << Srcpos_To_Line(WN_linenum(wn_pragma)) << "\" >" 
		<< UH_PragmaToString(WN_pragma(wn_pragma)) << "</pragma>" <<std::endl;
	  WN *wn_body= WN_region_body(wn);
	  UH_Browse_Node_Dump(wn_body, strStart, strParent);
    	}
      }
      if(sName) {
	oFile <<strStart <<"</"<<sName <<">\n";
      }
    }

  } else {
  	//other cases
   if(WN_kid_count(wn_node)>0) {
	/* the node has at least a child ! */
    oFile <<strStart<<"\t<kids>\n";
    myStart = strStart + "\t\t";
    
    for (int i = 0; i < WN_kid_count(wn_node); i++) {
      sName = (char*) UH_CheckWN(WN_kid(wn_node, i), strStart);
      if((sName || (WN_opcode(WN_kid(wn_node, i)))==OPC_BLOCK)  ) {
	// do not treat IF here since it has been treated previously
          UH_Browse_Node_Dump(WN_kid(wn_node, i), myStart, sName);
      }
      if(sName ) {
	oFile <<myStart <<"</"<<sName <<">\n";
      } 
    }
    oFile <<strStart<<"\t</kids>\n";
    }
  }
}

/*---------------------------------------------------*/
static const char* UH_PragmaToString(mUINT16 iPragma) 
{
  char *sStr = NULL;
  switch(iPragma) {
    case WN_PRAGMA_MASTER_BEGIN:
	sStr = "master begin";
	break;
    case WN_PRAGMA_PDO_BEGIN:
	sStr = "parallel do begin";
	break;
    case WN_PRAGMA_PARALLEL_DO:
	sStr = "parallel do";
	break;
    case WN_PRAGMA_PARALLEL_BEGIN:
	sStr = "parallel begin";
	break;
    case WN_PRAGMA_SINGLE_PROCESS_BEGIN:
	sStr = "single begin";
	break;
    case WN_PRAGMA_PARALLEL_SECTIONS:
	sStr = "parallel section";
	break;
    case WN_PRAGMA_PSECTION_BEGIN:
	sStr = "section begin";
	break;
    default:
	sStr = "unknown";
  }

  return sStr;
}

/*---------------------------------------------------*/
static void Print_Pragmas(WN *pragma_blk)
{
  WN *prag = (WN_region_pragmas(pragma_blk));
  int iLastLine = 0;
  BOOL is_pragma_block = TRUE;

  if (!prag)
    return ;
  prag = WN_first(prag);
  if (!prag)
    return ;
  WN *wnLastRegion = WN_last((WN_region_pragmas(pragma_blk)));
  WN *wnDoLoop;
  WN *wnNext, *wnFirst, *wnLast;

  wnDoLoop = WN_last(WN_region_body(pragma_blk));
  if(wnLastRegion)
    iLastLine = Srcpos_To_Line(WN_linenum(wnLastRegion));
  switch (WN_pragma(prag)) {
  case WN_PRAGMA_PARALLEL_BEGIN:  // PARALLEL SECTIONS already lowered to PDO
     printf("Parallel in line %d - %d\n", Srcpos_To_Line(WN_linenum(prag)), iLastLine);
     wnFirst = WN_first(WN_region_body(pragma_blk));
     wnLast  = WN_last(WN_region_body(pragma_blk));
     printf("  Region line %d - %d \n", Srcpos_To_Line(WN_linenum(wnFirst)),
		 Srcpos_To_Line(WN_linenum(wnLast)) );
     //UH_CreateDoLoop(pragma_blk, prag);
     break;

  case WN_PRAGMA_PARALLEL_DO:
     if(wnDoLoop) iLastLine = iLastLine = Srcpos_To_Line(WN_linenum(wnDoLoop));
     printf("Parallel Do of loop %s in line %d - %d \n", ST_name(WN_st(WN_index(wnDoLoop))), 
		Srcpos_To_Line(WN_linenum(prag)), iLastLine);
     break;
  case WN_PRAGMA_DOACROSS:
     printf("Parallel do accross in line %d - %d\n", Srcpos_To_Line(WN_linenum(prag)), iLastLine);
     break;

  case WN_PRAGMA_PDO_BEGIN:
     printf("Do loop %s, in line %d - %d\n", ST_name(WN_st(WN_index(wnDoLoop))),
		Srcpos_To_Line(WN_linenum(prag)), iLastLine);
     break;

  case WN_PRAGMA_MASTER_BEGIN:
     is_pragma_block = FALSE;
     printf("Master in line %d - %d\n", Srcpos_To_Line(WN_linenum(prag)), iLastLine);
     break;

  case WN_PRAGMA_SINGLE_PROCESS_BEGIN:
     is_pragma_block = FALSE;
     printf("Single in line %d - %d\n", Srcpos_To_Line(WN_linenum(prag)), iLastLine);
     break;

  case WN_PRAGMA_PARALLEL_SECTIONS:
     printf("Parallel Section in line %d - %d\n", Srcpos_To_Line(WN_linenum(prag)), iLastLine);
     break;
  case WN_PRAGMA_PSECTION_BEGIN:
     printf("Section in line %d - %d\n", Srcpos_To_Line(WN_linenum(prag)), iLastLine);
     break;
  default:
    is_pragma_block = FALSE;
    break;
  }

  if(!is_pragma_block)
     return;

  /*-------------------------*/
  /* This is a pragma region */
  /*-------------------------*/
  BOOL bLocal=FALSE, bLastLocal=FALSE, bFirstPrivate=FALSE, bShared=FALSE, bReduction=FALSE;
  for (WN *prag_clause = WN_next(prag); prag_clause;
         prag_clause = WN_next(prag_clause)) {
      if(prag_clause) {
        WN_PRAGMA_ID prag_id = (WN_PRAGMA_ID) WN_pragma(prag_clause);
        ST *st_prag = WN_st(prag_clause);
        switch (prag_id) {
        case WN_PRAGMA_LOCAL:
          if(!bLocal) {
           printf("\n Private:");
           bLocal = TRUE;
          }
          break;
        case WN_PRAGMA_LASTLOCAL:
          printf("\n LastPrivate:");
          break;
        case WN_PRAGMA_FIRSTPRIVATE:
          printf("\n FirstPrivate:");
          break;
        case WN_PRAGMA_SHARED:
          printf("\n Shared:");
          break;
        case WN_PRAGMA_REDUCTION:
          printf("\n Reduction:");
          break;
        case WN_PRAGMA_DEFAULT:
          printf("\n Default:");
          switch(WN_pragma_arg1(prag_clause)) {
           case WN_PRAGMA_DEFAULT_PRIVATE:
                printf(" Private\n");
                break;
           case WN_PRAGMA_DEFAULT_SHARED:
                printf(" Shared\n");
                break;
           case WN_PRAGMA_DEFAULT_NONE:
                printf(" NONE\n");
                break;
          }
          break;
        //default:
          //printf("ERROR: unknown clause: %d\n ",prag_id);
        }
        if(st_prag && prag_id != WN_PRAGMA_DEFAULT) {
          if(ST_name(st_prag))
             printf("%s ",ST_name(st_prag));
        }
      }
  }
  printf("\n");
}

/* ------------------------------------------------
 * Create a surrounding DO LOOP
 *-------------------------------------------------*/
static WN* UH_CreateDoLoop(WN* wnParent, WN* wnPragma) 
{
  // create index
  ST *index_st = MTYPE_To_PREG(MTYPE_I4);
  WN_OFFSET index_offset = Create_Preg(MTYPE_I4,"uh_loop");
  WN *index = WN_CreateIdname(index_offset,index_st);

  // create lower bount
  WN *lb = WN_StidIntoPreg(MTYPE_I4,index_offset, index_st,
			WN_CreateIntconst(OPC_I4INTCONST, 0 ));

  // upper bound
  WN *ub = WN_LE (MTYPE_I4, WN_LdidPreg (MTYPE_I4,index_offset ),
                   WN_CreateIntconst(OPC_I4INTCONST,1));
  // stride
  WN *incr = WN_StidIntoPreg ( MTYPE_I4, index_offset, index_st,
		WN_Add(MTYPE_I4, WN_LdidPreg ( MTYPE_I4,index_offset),
			WN_CreateIntconst ( OPC_I4INTCONST,(INT64)1 )));

  // the new do loop WN
  WN *new_do = WN_CreateDO(index,lb,ub,incr,WN_CreateBlock(),NULL);

  // set the line number
  WN_Set_Linenum(lb,WN_Get_Linenum(wnParent));
  WN_Set_Linenum(incr,WN_Get_Linenum(wnParent));
  WN_Set_Linenum(new_do,WN_Get_Linenum(wnParent));

  UH_Parentize(new_do);
  char strBuf[1024];
  strBuf[0] = '\0';
  UH_BrowseWhirlNode(WN_region_body(wnParent),strBuf);
  //WN *region_body = WN_next(wnPragma);
  //WN *region_body = WN_first(WN_region_body(wnParent));
  if(WN_region_body(wnPragma)) {
    WN *region_body = WN_first(WN_region_body(wnPragma));
    WN *wnDoBody = NULL;
    if ((region_body)) {
      WN *wnNext = WN_next(region_body);
      WN *wn = WN_EXTRACT_FromBlock(WN_region_body(wnParent),(region_body));
      WN_INSERT_BlockAfter(WN_do_body(new_do),wnDoBody,wn);
      Set_Parent(wn,WN_do_body(new_do));
      //region_body = wnNext;
    } 
  }
  WN_INSERT_BlockAfter(WN_region_body(wnParent),NULL,new_do);
  Set_Parent(new_do,WN_region_body(wnParent));

  // Mark the do loop index variable as local
  WN *local_pragma = WN_CreatePragma(WN_PRAGMA_LOCAL, index_st,index_offset,0);
  WN_INSERT_BlockAfter(WN_region_pragmas(wnParent),wnPragma,local_pragma);
  Set_Parent(local_pragma,WN_region_pragmas(wnParent));


}


/* ------------------------------------------------
 * Create parent map
 *-------------------------------------------------*/
static void UH_Parentize (WN* wn)
{
  if (!OPCODE_is_leaf(WN_opcode(wn))) { 
    if (WN_opcode(wn) == OPC_BLOCK) {
      WN *kid = WN_first(wn);
      while (kid) {
        Set_Parent(kid, wn);
        UH_Parentize(kid);
        kid = WN_next(kid);
      }
    } else {
      INT kidno;
      for (kidno = 0; kidno < WN_kid_count(wn); kidno++) {
        WN *kid = WN_kid(wn, kidno);
        if (kid) { 
          Set_Parent(kid, wn);
          UH_Parentize(kid);
        }
      }
    }
  }
}

/* ------------------------------------------------
 * Debugger: browse whirl
 *-------------------------------------------------*/
static void UH_BrowseWhirlNode(WN* wnSrc, char *strBuf)
{
  if(wnSrc) {
    if(WN_opcode(wnSrc))
      printf("%s %d:%s\n", strBuf, Srcpos_To_Line(WN_Get_Linenum(wnSrc)), OPCODE_name(WN_opcode(wnSrc)));
  } else
    return;

  WN *wn = wnSrc;

  switch(WN_opcode(wnSrc)) {
   case OPC_BLOCK:
        strcat(strBuf,"  ");
	for(WN *kid = WN_first (wn);kid ;kid = WN_next(kid) ) {
	  UH_BrowseWhirlNode(kid, strBuf);
	}
	break;
    case OPC_DO_LOOP:
 	printf("%s Loop %s\n", strBuf, ST_name(WN_st(WN_index(wn))));
	break;
    default:
	for (INT k = 0; k < WN_kid_count(wn); k++) {
	  UH_BrowseWhirlNode(WN_kid(wn, k), strBuf);
   	}

  }
}

/* ------------------------------------------------
 * Debugger: browse whirl
 *-------------------------------------------------*/
static void UH_BrowseWhirlNode(WN* pragma_blk)
{
     WN *wnNext = WN_first(WN_region_body(pragma_blk));
     while(wnNext) {
        TYPE_ID type = OPCODE_desc(WN_opcode(wnNext));
	printf(" Next line %d: %d %d ", Srcpos_To_Line(WN_linenum(wnNext)), type, WN_opcode(wnNext));
	switch(WN_opcode(wnNext)) {
	case OPC_PRAGMA:
	  printf(" Pragma");
	  break;
	case OPC_DO_LOOP:
	  printf(" Loop");
	  break;
	case OPC_BLOCK:
	  printf(" Block");
	  break;
	case OPC_REGION:
	  printf(" Region");
	  break;
	default:
	  printf(" Unknown:" );
	  break;
	}
	printf("\n");
        wnNext = WN_next(wnNext);
     } 
}
