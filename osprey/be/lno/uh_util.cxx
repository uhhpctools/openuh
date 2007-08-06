/*

  Copyright (C) 2006, 2007 University of Houston.  All Rights Reserved.

*/
#include <stdio.h>
#include <algorithm>
#include <map>
#include <string>
#include <iostream>
#include <sstream>

#include "wn_util.h"
#include "lwn_util.h"
#include "glob.h"

#include "ir_reader.h"
#include "dwarf_DST.h"
#include "dwarf_DST_mem.h"

#include "uh_glob.h"

typedef struct StructFileDir
  {
    std::string name;
    int incl_index;
  } CFileDir;

const char *ParStatusStr[]={"Auto", "Manual", "Synch", "None"};

static int UH_Browse_Node_Dump(WN* wn_node, std::string strStart,
		      char *strParent);

std::vector<PAR_LOOP_STATUS> vecLoopStatus;
std::map<WN*, PAR_LOOP_STATUS> mapLoopStatus;
//-----------------------------------------------------------------
// Get the line number of the Whirl node
// If the node has no line number information, get the line number
//  of the parent
//-----------------------------------------------------------------
int WN_linenum_fixed(WN *wn)
{
   int line=0;
   SRCPOS srcpos = WN_Get_Linenum(wn);
   USRCPOS linepos;
   USRCPOS_srcpos(linepos) = srcpos;
   line = USRCPOS_linenum(linepos);

    return line;
}

extern int UH_GetLineNumber(WN *wn)
{
  int iLine = 0;
  if(!wn) return 0;
  for(;wn && iLine==0; wn=LWN_Get_Parent(wn)) {
    iLine = WN_linenum_fixed(wn);
  }
  if(wn)
    if(WN_operator(wn) == OPR_ISTORE || WN_operator(wn) == OPR_MPY)
        return 0;
  return iLine;
}

//-----------------------------------------------------------------
extern void UH_IPA_IR_Filename_Dirname(SRCPOS srcpos,        /* in */
			             char *&fname,   /* out */
			             char *&dirname) /* out */
{
  static std::vector<std::string>incl_table_v;	// list of directories
  static std::vector<CFileDir>file_table_v;  // list of files
  static bool bInitialized = false;
  USRCPOS usrcpos;

  USRCPOS_srcpos(usrcpos) = srcpos;
  if (USRCPOS_filenum(usrcpos) == 0)
   {
      fname = NULL;
      dirname = NULL;
   } else {
     DST_IDX idx;
     DST_INCLUDE_DIR *incl;
     DST_FILE_NAME *file;
     char *name;
 
     if(!bInitialized) {
	// save the directory names
  	for (idx = DST_get_include_dirs ();
       		!DST_IS_NULL(idx);
       		idx = DST_INCLUDE_DIR_next(incl))
    	{
    	  incl = DST_DIR_IDX_TO_PTR (idx);
   	  name = DST_STR_IDX_TO_PTR (DST_INCLUDE_DIR_path(incl));
    	  std::string name_s = name;
    	  incl_table_v.push_back(name_s);
//	  std::cout <<"DIR:"<<name<<std::endl;
        }
	// save the filename
  	for (idx = DST_get_file_names (); 
     	  !DST_IS_NULL(idx); 
    	   idx = DST_FILE_NAME_next(file))
    	{
    	  file = DST_FILE_IDX_TO_PTR (idx);
    	  if (DST_IS_NULL(DST_FILE_NAME_name(file))) {
    	    name = "NULLNAME";
    	  } 
   	  else {
   	     name = DST_STR_IDX_TO_PTR (DST_FILE_NAME_name(file));
    	  }
   	  CFileDir file_d;
    	  file_d.name  = name;
    	  file_d.incl_index = DST_FILE_NAME_dir(file);
    	  file_table_v.push_back(file_d);
//	  std::cout <<"FILE:"<<name<<"\tID:"<<file_d.incl_index <<std::endl;
    	}

	bInitialized = true;
     }
 
     int fileidx = USRCPOS_filenum(usrcpos) - 1 ; 
     fname = (char*)file_table_v[fileidx].name.c_str();
     int diridx = file_table_v[fileidx].incl_index - 1;
     dirname = (char*)incl_table_v[diridx].c_str();
//     std::cout <<"File:"<<fname<<"\t"<<"Id="<<fileidx<<std::endl;
  } 
}

//-----------------------------------------------------------------------
// NAME: UH_GetBaseExtName
// FUNCTION: retrieve the base of filename and its extension
//-----------------------------------------------------------------------
extern int UH_GetBaseExtName(std::string filename, std::string &filebase)
{
    const char cDot = '.';

    std::string::size_type pos = filename.find_last_of(cDot);

    if(pos++ != std::string::npos)
      std::copy(filename.begin(), filename.begin()+(pos-1),  std::back_inserter(filebase));

    return int (pos);
}

//-----------------------------------------------------------------------
// NAME: UH_WNexprToString
//-----------------------------------------------------------------------
extern std::string UH_WNexprToString(WN *wn)
{
  OPCODE   opc = WN_opcode(wn);
  OPERATOR opr = OPCODE_operator(opc);
  std::stringstream str;
  str << "";

    switch(opr) {
     case OPR_CONST:
      switch (OPCODE_rtype(opc)) {
       case MTYPE_F4:
   	str << STC_val(WN_st(wn)).vals.fval;
        break;
       case MTYPE_F8:
	str << STC_val(WN_st(wn)).vals.dval;
        break;
       default:
	str <<"Unknown";
        break;
      }
      break;
     case OPR_INTCONST:
      str << WN_const_val(wn);
      break;
     case OPR_LDID:
      str << (WN_has_sym(wn)? ST_name(WN_st(wn)):"LDID" );
      break;

     case OPR_ADD:
      str<< "+";
      break;
     case OPR_SUB:
      str << "-";
      break;
     case OPR_MPY:
      str << "*";
      break;
     case OPR_DIV:
      str << "/";
      break;
     default:
      str << (WN_has_sym(wn)? ST_name(WN_st(wn)): OPERATOR_name(opr) );
      break;
    }
   return str.str();
}


//-----------------------------------------------------------------------
// NAME: UH_Node_Dump
// FUNCTION: Return the node count of the tree rooted at 'wn_node'.  If
//    possible, dump into graphviz format
//-----------------------------------------------------------------------

extern int UH_Node_Dump(WN* wn_node)
{
  int count;
  std::string my = " ";
  count = UH_Browse_Node_Dump(wn_node, my, "Main");

}


//-----------------------------------------------------------------------
extern void UH_PrintNode(WN *wnRoot)
{
  LWN_ITER* itr = LWN_WALK_TreeIter(wnRoot);

  for (; itr != NULL; itr = LWN_WALK_TreeNext(itr)) {
    WN* wn = itr->wn;
    if(OPERATOR_is_stmt(WN_operator(wn)))
      printf("Line %d\tOpcode: %s\tOperator:%s\n", Srcpos_To_Line(WN_linenum(wn)), 
	OPCODE_name(WN_opcode(wn)), OPERATOR_name(WN_operator(wn)));
  }

}

//-----------------------------------------------------------------------
static const char* UH_CheckWN(WN *wn) 
{
  /* we only interested with some operators: IF, CALL, LOOPS*/
  switch(WN_operator(wn)) {
    case OPR_CALL:
    case OPR_CASEGOTO:
    case OPR_DO_LOOP:
    case OPR_DO_WHILE:
    case OPR_GOTO:
    case OPR_IF:
    case OPR_WHILE_DO:
    case OPR_SELECT:
    case OPR_XGOTO:
    case OPR_WHERE:
 	return OPERATOR_name(WN_operator(wn));
	break;
    default:
	break;
	// do nothing ?
  }
  return NULL;
}
//-----------------------------------------------------------------------
static int UH_Browse_Node_Dump(WN* wn_node, std::string strStart,
		      char *strParent)
{
  int count = 0;
  int level = 0;
  char *sName;

/*      sName = (char*) UH_CheckWN(wn);
      if(sName) {
	std::cout << myStart<<Srcpos_To_Line(WN_linenum(wn))<<":"<<sName<<"\n";
      }*/

  std::string myStart = strStart + "\t";
  if (WN_opcode(wn_node) == OPC_BLOCK) {
    for (WN* wn = WN_first(wn_node); wn != NULL; wn = WN_next(wn)) {
      sName = (char*) UH_CheckWN(wn);
      if(sName) {
	std::cout << myStart<<Srcpos_To_Line(WN_linenum(wn))<<":"<<sName<<"\n";
      }
      count += UH_Browse_Node_Dump(wn, myStart,  sName);
    }
  } else {
    myStart = myStart + "\t";
    for (int i = 0; i < WN_kid_count(wn_node); i++) {
      sName = (char*) UH_CheckWN(WN_kid(wn_node, i));
      if(sName) {
        std::cout <<myStart<<sName;
      }

      count += UH_Browse_Node_Dump(WN_kid(wn_node, i), myStart, sName);
    }
  }
  return count;
}

