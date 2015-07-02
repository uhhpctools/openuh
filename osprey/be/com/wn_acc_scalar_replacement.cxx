 /***************************************************************************
  This file is created by Xiaonan(Daniel) Tian from HPCTools, University of Houston
  (daniel.xntian@gmail.com) for OpenUH OpenACC compiler.
  It is intended to lower the OpenACC pragma.
  It is free to use. However, please keep the original author.
  http://www2.cs.uh.edu/~xntian2/
*/

#include <stdint.h>
#ifdef USE_PCH
#include "be_com_pch.h"
#endif /* USE_PCH */
#pragma hdrstop

/* Header of wn_mp_dg.cxx
*  csc.
*/
#include <sys/types.h>
#if defined(BUILD_OS_DARWIN)
#include <darwin_elf.h>
#else /* defined(BUILD_OS_DARWIN) */
#include <elf.h>
#endif /* defined(BUILD_OS_DARWIN) */

#define USE_STANDARD_TYPES          /* override unwanted defines in "defs.h" */

#include <bstring.h>
#include "wn.h"
#include "wn_util.h"
#include "erglob.h"
#include "errors.h"
#include "strtab.h"                 /* for strtab */
#include "symtab.h"                 /* for symtab */
#include "irbdata.h"                /* for inito */
#include "dwarf_DST_mem.h"          /* for DST */
#include "pu_info.h"
#ifdef __MINGW32__
#include <WINDOWS.h>
#endif /* __MINGW32__ */
#include "ir_bwrite.h"
#include "ir_reader.h"
#include "ir_bcom.h"
#include "region_util.h"            /* for RID */
#include "dep_graph.h"
#include "cxx_hash.h"
#include "file_util.h"      /* For Last_Pathname_Component */
#include "wn_tree_util.h"
/* wn_mp_dg.cxx header end.
*  csc.
*/

#include <string.h>

#if ! defined(BUILD_OS_DARWIN)
#include <elf.h>
#endif /* ! defined(BUILD_OS_DARWIN) */
#include "alloca.h"
#include "cxx_template.h"
#include "defs.h"
#include "glob.h"
#include "errors.h"
#include "erglob.h"
#include "erbe.h"
#include "tracing.h"
#include "strtab.h"

#include "symtab.h"


#include "wn_util.h"
#include "wn_simp.h"
#include "stblock.h"
#include "data_layout.h"
#include "targ_sim.h"
#include "targ_const.h"
#include "config_targ.h"
#include "config_asm.h"
#include "const.h"
#include "ttype.h"
#include "wn_pragmas.h"
#include "wn_lower.h"
#include "region_util.h"
#include "wutil.h"
#include "wn_map.h"
#include "pu_info.h"
#include "config.h"
#include "standardize.h"
#include "irbdata.h"
#include "privatize_common.h"
#include "cxx_hash.h"
#include "wn_acc.h"
#include "mempool.h"
#include "parmodel.h"	// for NOMINAL_PROCS
#include "fb_info.h"
#include "fb_whirl.h"
#include "be_symtab.h"
#ifdef KEY
#include "wn_lower.h"
#include "config_opt.h"
#endif
#include "alias_analyzer.h"

/*This piece of code is used for loop invariant code motion*/
typedef struct acc_array_ref_info
{
	//UINT32 iref_count;
	WN* wn_array_ref;
	UINT32 iread_count;
	UINT32 iwrite_count;
	ST* st_scalar;
	WN* wn_load_into_scalar;
	WN* wn_store_from_scalar;
}acc_array_ref_info;

typedef struct acc_kernel_reg_info_feedback
{
	string strkernel;
	UINT32 iregister_num;
}acc_kernel_reg_info_feedback;

static vector<acc_kernel_reg_info_feedback> acc_registes_feedback_info;
static ST* st_current_kernel;

typedef vector<acc_array_ref_info> vector_array_ref_info;
static map<ST*, INT32> acc_disabled_array_scalar;
static map<ST*, vector_array_ref_info> acc_array_reference_information;
static char acc_tmp_loop_invariant_code_emotion[] = "__acc_licm_tmp_";
static WN* acc_scalaization_prehand;
static WN* acc_scalaization_afterhand;


vector<ST*> acc_loop_index_var;
BOOL acc_scalarization_enabled = FALSE;
BOOL acc_scalarization_level2_enabled = FALSE;
BOOL acc_scalarization_level3_enabled = FALSE;
BOOL acc_ptr_restrict_enabled = FALSE;
map<ST*, BOOL> acc_const_offload_scalar;
UINT32 kernel_tmp_licm_count = 0;



static void ACC_Update_Array_Ref_Info(WN*wn_array_addr, BOOL bRead)
{
	WN* wn_base = WN_array_base(wn_array_addr);
	ST* st_array = WN_st(wn_base);
	vector_array_ref_info Array_ref_info_vec;
	acc_array_ref_info Array_ref_info;
	
	map<ST*, vector_array_ref_info>::iterator mapitor = acc_array_reference_information.find(st_array);
	if(mapitor != acc_array_reference_information.end())
	{
		//make a new entry
		Array_ref_info_vec = acc_array_reference_information[st_array];
		vector<acc_array_ref_info>::iterator vector_itor = Array_ref_info_vec.begin();
		for(; vector_itor != Array_ref_info_vec.end(); vector_itor++)
		{
			acc_array_ref_info existing_info = *vector_itor;
			WN* wn_array_ref = existing_info.wn_array_ref;
			if(WN_Simp_Compare_Trees(wn_array_ref, wn_array_addr) == 0)
			{
				if(bRead)
					existing_info.iread_count ++;
				else
					existing_info.iwrite_count ++;
				*vector_itor = existing_info;
				acc_array_reference_information[st_array] = Array_ref_info_vec;
				return;
			}
		}
		//if didn't find it
		if(vector_itor == Array_ref_info_vec.end())
		{
			acc_array_ref_info new_info;
			new_info.iread_count = 0;
			new_info.iwrite_count = 0;
			new_info.st_scalar = NULL;
			new_info.wn_array_ref = WN_COPY_Tree(wn_array_addr);
			new_info.wn_load_into_scalar = NULL;			
			new_info.wn_store_from_scalar = NULL;
			if(bRead)
				new_info.iread_count ++;
			else
				new_info.iwrite_count ++;
			Array_ref_info_vec.push_back(new_info);
			acc_array_reference_information[st_array] = Array_ref_info_vec;
		}
	}
	else
	{
		Array_ref_info_vec.clear();
		acc_array_ref_info new_info;
		new_info.iread_count = 0;
		new_info.iwrite_count = 0;
		new_info.st_scalar = NULL;
		new_info.wn_array_ref = WN_COPY_Tree(wn_array_addr);
		new_info.wn_load_into_scalar = NULL;			
		new_info.wn_store_from_scalar = NULL;
		if(bRead)
			new_info.iread_count ++;
		else
			new_info.iwrite_count ++;
		Array_ref_info_vec.push_back(new_info);
		acc_array_reference_information[st_array] = Array_ref_info_vec;
	}
}


static void ACC_Delete_Array_Ref_Info(WN* wn_array_addr)
{
	WN* wn_base = WN_array_base(wn_array_addr);
	ST* st_array = WN_st(wn_base);
	vector_array_ref_info Array_ref_info_vec;
	acc_array_ref_info Array_ref_info;
	
	Array_ref_info_vec = acc_array_reference_information[st_array];
	vector<acc_array_ref_info>::iterator vector_itor = Array_ref_info_vec.begin();
	for(; vector_itor != Array_ref_info_vec.end(); vector_itor++)
	{
		acc_array_ref_info existing_info = *vector_itor;
		WN* wn_array_ref = existing_info.wn_array_ref;
		if(WN_Simp_Compare_Trees(wn_array_ref, wn_array_addr) == 0)
		{
			Array_ref_info_vec.erase(vector_itor);
			acc_array_reference_information[st_array] = Array_ref_info_vec;
			return;
		}
	}
	
  	Fail_FmtAssertion (
	 "The item is not found in array reference contai(ACC_Delete_Array_Ref_Info).");
}

static BOOL ACC_IsExpPredictable(WN* wn_exp)
{
	ST* st_name = NULL;
	int i=0;
	BOOL bResults = TRUE;
	if(WN_has_sym(wn_exp))
	{
		st_name = WN_st(wn_exp);
		for(i=0; i<acc_loop_index_var.size(); i++)
		{
			ST* st_loop_index = acc_loop_index_var[i];
			//find it, then it is predictable
			if(st_loop_index == st_name)
				return TRUE;
		}
		//if level2 is enabled, we need check additional information
		if(acc_scalarization_level3_enabled)
		{
			if(!acc_const_offload_scalar.empty() && 
				acc_const_offload_scalar.find(st_name)!=acc_const_offload_scalar.end())
				return TRUE;
		}
		return FALSE;
	}

	for (i=0; i < WN_kid_count(wn_exp); i++)
    {
      //every single kid should be predictable
      if(ACC_IsExpPredictable ( WN_kid(wn_exp, i)) == FALSE)
	  	return FALSE;
    }
	return TRUE;
}

static BOOL ACC_IsArrayValidScalarization(WN * tree)
{
	int ii;
	int idim = WN_num_dim(tree);
	BOOL bResults = TRUE;
	for(ii=0; ii<idim; ii++)
	{
		WN* wn_array_index_exp = WN_array_index(tree, ii);
      	if(ACC_IsExpPredictable (wn_array_index_exp) == FALSE)
	  		return FALSE;
	}
	return TRUE;
}

static void ACC_DisabledScalarizationArraySym(WN * tree)
{
	WN* wn_base = WN_array_base(tree);
	ST* st_array = WN_st(wn_base);
	acc_disabled_array_scalar[st_array] = TRUE;
}

static BOOL ACC_IsDisabledScalarizationArraySym(WN * tree)
{
	WN* wn_base = WN_array_base(tree);
	ST* st_array = WN_st(wn_base);
	//clear all the counted array element in acc_array_reference_information
	map<ST*, INT32>::iterator mapitor = 
				acc_disabled_array_scalar.find(st_array);
	if(mapitor != acc_disabled_array_scalar.end())
		return TRUE;
	else
		return FALSE;
}

static void ACC_ClearCountedArrayElement(WN* tree)
{
	WN* wn_base = WN_array_base(tree);
	ST* st_array = WN_st(wn_base);
	//clear all the counted array element in acc_array_reference_information
	map<ST*, vector_array_ref_info>::iterator mapitor = 
				acc_array_reference_information.find(st_array);
	//find it, and clear it
	if(mapitor != acc_array_reference_information.end())
	{
		mapitor->second.clear();
		acc_array_reference_information.erase(mapitor);
	}	
}

bool SortByArrayRefCount(const acc_array_ref_info &lhs, const acc_array_ref_info &rhs) 
{
	return (lhs.iread_count+lhs.iwrite_count) > (rhs.iread_count+rhs.iwrite_count); 
}

static vector_array_ref_info acc_array_ref_analysis_vector;
static FILE *curr_output_fp = stdout;
static INT32 ARRAY_ELEMENT_SCALARIZATION_THRESHOLD_DEFAULT=32;
static INT32 MAX_REGISTERS_ALLOWED_PER_KERNEL=128; //this value is used by feedback
static INT32 acc_aray_element_registers_num_left = 0;

static void ACC_Dump_Array_Ref_Index(WN* wn_index)
{
  	OPCODE op;
  	OPERATOR opr;
	op = WN_opcode(wn_index);
    opr = OPCODE_operator(op);
	if(opr == OPR_LDID)
	{
		ST* st_name = WN_st(wn_index);
		fprintf(curr_output_fp, "%s", ST_name(st_name));
	}
	else if(opr == OPR_ADD)
	{
		ACC_Dump_Array_Ref_Index(WN_kid0(wn_index));
		fprintf(curr_output_fp, "+");
		ACC_Dump_Array_Ref_Index(WN_kid1(wn_index));
	}
	else if(opr == OPR_SUB)
	{
		ACC_Dump_Array_Ref_Index(WN_kid0(wn_index));
		fprintf(curr_output_fp, "-");
		ACC_Dump_Array_Ref_Index(WN_kid1(wn_index));
	}
	else if(opr == OPR_MPY)
	{
		ACC_Dump_Array_Ref_Index(WN_kid0(wn_index));
		fprintf(curr_output_fp, "*");
		ACC_Dump_Array_Ref_Index(WN_kid1(wn_index));
	}
	else if(opr == OPR_DIV)
	{
		ACC_Dump_Array_Ref_Index(WN_kid0(wn_index));
		fprintf(curr_output_fp, "/");
		ACC_Dump_Array_Ref_Index(WN_kid1(wn_index));
	}
	else if(opr == OPR_INTCONST)
	{
		INT32 ivalue = WN_const_val(wn_index);
		fprintf(curr_output_fp, "%d", ivalue);
	}
	else if(opr == OPR_CVT)
	{
		ACC_Dump_Array_Ref_Index(WN_kid0(wn_index));
	}	
	else 
	{
		fprintf(curr_output_fp, "CMPX");
	}
}

static void ACC_Dump_Array_Ref(WN* wn_array_addr)
{
  	OPCODE op;
  	OPERATOR opr;
	WN* wn_base = WN_array_base(wn_array_addr);
	INT32 idim = WN_num_dim(wn_array_addr);
	ST* st_array = WN_st(wn_base);
	op = WN_opcode(wn_array_addr);
    opr = OPCODE_operator(op);
	if(opr !=OPR_ARRAY)
		Fail_FmtAssertion("ACC_Dump_Array_Ref only takes care array node", ST_name(st_array));

	fprintf(curr_output_fp, "%s", ST_name(st_array));
	int i = 0;
	for(i=0; i<idim; i++)
	{
		WN* wn_index = WN_array_index(wn_array_addr, i);
		fprintf(curr_output_fp, "[");
		ACC_Dump_Array_Ref_Index(wn_index);
		fprintf(curr_output_fp, "]");
	}

			
}

static void ACC_Parse_Feedback_Register_Info_nvidia()
{
	string line;
  	ifstream feedbackfile ("./.accfeedback.txt");
 	size_t found;

	acc_registes_feedback_info.clear();
	
  	if (feedbackfile.is_open())
  	{
    	while ( getline (feedbackfile,line) )
    	{
	      found = line.find("Compiling entry function");

	      if(found != string::npos)
	      {
	        size_t found1 = line.find("'", found);
	        size_t found2 = line.find("'", found1+1);
	        string strKernelname = line.substr(found1+1, found2-found1-1);
			acc_kernel_reg_info_feedback reg_info;
	        //cout << strKernelname << '\n';

	        getline (feedbackfile,line);
	        getline (feedbackfile,line);
	        getline (feedbackfile,line);
	        found1 = line.find("Used")+5;
	        found2 = line.find("registers");
	        string strRegisterNum = line.substr(found1, found2-found1);
			reg_info.strkernel = strKernelname;
			reg_info.iregister_num = strtol(strRegisterNum.c_str(), NULL, 10);
			acc_registes_feedback_info.push_back(reg_info);	        
	      }
    }
    feedbackfile.close();
  }
}

static void ACC_Parse_Feedback_Register_Info_apu()
{
	string line;
  	ifstream feedbackfile ("./.accfeedback.txt");
 	size_t found;

	acc_registes_feedback_info.clear();
	
  	if (feedbackfile.is_open())
  	{
    	while ( getline (feedbackfile,line) )
    	{
	      found = line.find("Post-finalization statistics for kernel");

	      if(found != string::npos)
	      {
	        size_t found1 = line.find(":", found);
	        size_t found2 = line.length()-1;
			//trim the space at the end
			while(isspace(line[found2]))
			{
				line[found2]='\000';
				found2 --;
			}
			//plus 2 means skip ':' and white space
			found1 += 2;
	        string strKernelname = line.substr(found1, found2-found1+1);
			acc_kernel_reg_info_feedback reg_info;
	        //cout << strKernelname << '\n';

			//scalar register information which is ignored at this time
	        getline (feedbackfile,line);
			//vector register information which we care at this moment
	        getline (feedbackfile,line);
	        found1 = line.find(":");
	        found2 = line.length();
	        string strRegisterNum = line.substr(found1+1, found2-found1);
			reg_info.strkernel = strKernelname;
			reg_info.iregister_num = strtol(strRegisterNum.c_str(), NULL, 10);
			acc_registes_feedback_info.push_back(reg_info);	        
	      }
    }
    feedbackfile.close();
  }
}

static UINT32 ACC_Kernels_Used_Register_Num(ST* stKernelname)
{
	UINT32 i;
	UINT32 isize = acc_registes_feedback_info.size();
	for(i=0; i<isize; i++)
	{
		acc_kernel_reg_info_feedback reg_info = acc_registes_feedback_info[i];
		if(reg_info.strkernel.compare(ST_name(stKernelname)) == 0)
		{
			UINT32 iregnum = reg_info.iregister_num;
			return iregnum;
		}
	}
	if(i==isize)
	{
		Fail_FmtAssertion("ACC_Kernels_Used_Register_Num didn't find kernel %s in feedback info.\n", ST_name(stKernelname));
	}
	return 0;
}


static void ACC_Array_Ref_Results_Analysis(UINT32 reg_num_threshold)
{	
	BOOL bOutputInfo = (Enable_UHACCInfoFlag&(1<<UHACC_INFO_OUTPUT_SCALARIZATION))>>
									UHACC_INFO_OUTPUT_SCALARIZATION;
	INT32 num_array_elem_scalarize = 0;
	acc_array_ref_analysis_vector.clear();
	
	map<ST*, vector_array_ref_info>::iterator mapitor = 
				acc_array_reference_information.begin();

	for(; mapitor != acc_array_reference_information.end(); mapitor++)
	{
		vector_array_ref_info Array_ref_info_vec = mapitor->second;
		
		vector<acc_array_ref_info>::iterator vector_itor;// = Array_ref_info_vec.begin();
		for(int i=Array_ref_info_vec.size(); i>0; i--)
		{
			acc_array_ref_info existing_info = Array_ref_info_vec[i-1];
			vector_itor = Array_ref_info_vec.begin()+i-1;
			//remove unecessary array element 
			if(((existing_info.iread_count+existing_info.iwrite_count) <= 1)
							|| (existing_info.iread_count == 1 && existing_info.iwrite_count == 1))
			{
				Array_ref_info_vec.erase(vector_itor);
			}
			else
			{
				//count the number
				num_array_elem_scalarize ++;
				acc_array_ref_analysis_vector.push_back(existing_info);
			}
		}
		mapitor->second = Array_ref_info_vec;
	}
	if(bOutputInfo)
	{
		fprintf(curr_output_fp, " %s array element scalarization num: %d\n",
                    ST_name(st_current_kernel), num_array_elem_scalarize);
	}
	std::sort(acc_array_ref_analysis_vector.begin(), acc_array_ref_analysis_vector.end(), SortByArrayRefCount);
	//vector<acc_array_ref_info>::iterator vector_itor = acc_array_ref_analysis_vector.begin();
	//for(; vector_itor != acc_array_ref_analysis_vector.end(); vector_itor++)
	//{
	//	acc_array_ref_info existing_info = *vector_itor;
	//	printf("%d\t\n", existing_info.iread_count+existing_info.iwrite_count);
	//}
	//may be less than 0, however it will be reset in the set if-stmt.
	acc_aray_element_registers_num_left = reg_num_threshold - acc_array_ref_analysis_vector.size();
	//if the registers are over used, the application may get worse performance or even crashes.
	//scalarization tailer. We need to develop a sery of method to tail the register usage
	//for example: 
	//1. remove the coalescing reference
	//2. remove the least used array elements
	//set an threshold, if the array element scalarization num exceeds 64, remove the least usage array reference 
	if(acc_array_ref_analysis_vector.size() > reg_num_threshold)
	{		
		INT32 total_refs = acc_array_ref_analysis_vector.size();
		INT32 scalargap = total_refs - reg_num_threshold;
		//it reaches the limitation, no register is available
		acc_aray_element_registers_num_left = 0;
		if(bOutputInfo)
			fprintf(curr_output_fp, 
					" %s performs array element scalarization num reduction: %d back to array ref.\n",
                    ST_name(st_current_kernel), scalargap);
		for(int i=total_refs-1; i>=reg_num_threshold&&i>=0; i--)
		{
			acc_array_ref_info existing_info = acc_array_ref_analysis_vector[i];
			acc_array_ref_analysis_vector.pop_back();
			ACC_Delete_Array_Ref_Info(existing_info.wn_array_ref);
		}
	}
	
	if(bOutputInfo)
	{
		INT32 register_replace_num = acc_array_ref_analysis_vector.size();
		for(int i=0; i<register_replace_num; i++)
		{
			acc_array_ref_info existing_info = acc_array_ref_analysis_vector[i];
			ACC_Dump_Array_Ref(existing_info.wn_array_ref);
			fprintf(curr_output_fp, 
					" read:%d, write:%d.\n",
                    existing_info.iread_count, existing_info.iwrite_count);
			
		}
		//this is for debug, check how many array refereces are still in the map table
		INT32 mapcount = 0;
		mapitor = acc_array_reference_information.begin();
		for(; mapitor != acc_array_reference_information.end(); mapitor++)
		{
			vector_array_ref_info Array_ref_info_vec = mapitor->second;
			mapcount += Array_ref_info_vec.size();
		}
		fprintf(curr_output_fp, 
					"Totally %d array references will be replaced with scalar variables.\n",
                    mapcount);
	}
	
}

static void ACC_ScanAndCount_Array_Ref (WN * tree)
{	
  OPCODE op;
  OPERATOR opr;
  INT32 i;
  WN *r;
  WN *temp;
  ST *old_sym;
  WN_OFFSET old_offset;
  /* Initialization. */

  op = WN_opcode(tree);
  opr = OPCODE_operator(op);

  if(opr == OPR_ISTORE)
  {  	  		
		WN* wn_addr = WN_kid1(tree);
		OPERATOR addr_opr = WN_operator(wn_addr);
		if(addr_opr == OPR_ARRAY)
		{		
			BOOL bValidScalar = ACC_IsArrayValidScalarization(wn_addr);		
			if(bValidScalar == FALSE)
			{
				//put the array symbol into DISABLED SCALARIZATION ARRAY SYMBOL queue
				ACC_DisabledScalarizationArraySym(wn_addr);
				ACC_ClearCountedArrayElement(wn_addr);
			}

			//only if the array exp is valid and the array does not belong to disabled scalarization array sym
			if(bValidScalar == TRUE 
					&& ACC_IsDisabledScalarizationArraySym(wn_addr) == FALSE)
				ACC_Update_Array_Ref_Info(wn_addr, FALSE);
		}
  }
  else if(opr == OPR_ILOAD)
  {  		
		WN* wn_addr = WN_kid0(tree);
		OPERATOR addr_opr = WN_operator(wn_addr);
		if(addr_opr == OPR_ARRAY)
		{							
			BOOL bValidScalar = ACC_IsArrayValidScalarization(wn_addr);
			if(bValidScalar == FALSE)
			{
				//put the array symbol into DISABLED SCALARIZATION ARRAY SYMBOL queue
				ACC_DisabledScalarizationArraySym(wn_addr);
				ACC_ClearCountedArrayElement(wn_addr);
			}

			if(bValidScalar == TRUE 
					&& ACC_IsDisabledScalarizationArraySym(wn_addr) == FALSE)
				ACC_Update_Array_Ref_Info(wn_addr, TRUE);
		}
		
  }
  /* Walk all children */

  if (op == OPC_BLOCK) 
  {
    r = WN_first(tree);
    while (r) 
	{ // localize each node in block
      ACC_ScanAndCount_Array_Ref(r);
      r = WN_next(r);
      
   }
  }
  else 
  {
    for (i=0; i < WN_kid_count(tree); i++)
    {
      ACC_ScanAndCount_Array_Ref ( WN_kid(tree, i));
    }
  }
}

static WN* ACC_Array_Ref_Scalar_Replacement (WN * tree)
{
  OPCODE op;
  OPERATOR opr;
  INT32 i;
  WN *r;
  WN *temp;
  ST *old_sym;
  WN_OFFSET old_offset;

  /* Ignore NULL subtrees. */

  if (tree == NULL)
    return (tree);

  /* Initialization. */

  op = WN_opcode(tree);
  opr = OPCODE_operator(op);
  
  if(opr == OPR_ISTORE)
  {  	  		
  		vector_array_ref_info Array_ref_info_vec;
		acc_array_ref_info Array_ref_info;
		WN* wn_array_addr = WN_kid1(tree);
		WN* wn_value = WN_kid0(tree);
		OPERATOR addr_opr = WN_operator(wn_array_addr);
		if(addr_opr==OPR_ARRAY)
		{		
			WN* wn_base = WN_array_base(wn_array_addr);
			ST* st_array = WN_st(wn_base);
			Array_ref_info_vec = acc_array_reference_information[st_array];
			vector<acc_array_ref_info>::iterator vector_itor = Array_ref_info_vec.begin();
			for(; vector_itor != Array_ref_info_vec.end(); vector_itor++)
			{
				acc_array_ref_info existing_info = *vector_itor;
				WN* wn_array_ref = existing_info.wn_array_ref;
				if(WN_Simp_Compare_Trees(wn_array_ref, wn_array_addr) == 0)
				{
					//if ref count is larger or equal than 2
					//(unless read and write each is one time ), 
					//do the replacement
					//if(((existing_info.iread_count+existing_info.iwrite_count) > 1)
					//	&& !(existing_info.iread_count == 1 && existing_info.iwrite_count == 1))
					{
						ST* st_scalar = NULL;
						TY_IDX elem_ty = ACC_Get_ElementTYForMultiArray(st_array);
						//if the scalar haven't been created yet, create it now.
						if(existing_info.st_scalar == NULL)
						{	
						    st_scalar = New_ST( CURRENT_SYMTAB );
							char szlocalname[256];
							sprintf ( szlocalname, "%s%s_%d", acc_tmp_loop_invariant_code_emotion, 
											ST_name(st_array), kernel_tmp_licm_count);
							kernel_tmp_licm_count ++;
							
							ST_Init(st_scalar,
						      Save_Str( szlocalname),
						      CLASS_VAR,
						      SCLASS_AUTO,
						      EXPORT_LOCAL,
						      MTYPE_To_TY(TY_mtype(elem_ty)));//Put this variables in local table
						      existing_info.st_scalar = st_scalar;
						}
						else
							st_scalar = existing_info.st_scalar;

						if(existing_info.wn_store_from_scalar== NULL)
						{							
							WN* wn_scalar_value = WN_Ldid(TY_mtype(elem_ty), 
															0, st_scalar, elem_ty);
							
							WN* wn_store_back = WN_COPY_Tree(tree);
							WN_kid0(wn_store_back) = wn_scalar_value;
							existing_info.wn_store_from_scalar = wn_store_back;
							if(acc_scalaization_afterhand == NULL)
								acc_scalaization_afterhand = WN_CreateBlock();
							WN_INSERT_BlockLast(acc_scalaization_afterhand, wn_store_back);
						}

						//replace the istore with scalar store stmt
						WN* wn_store = WN_Stid(TY_mtype(elem_ty), 
								0, st_scalar, ST_type(st_scalar), WN_COPY_Tree(wn_value));
					   //update
					   *vector_itor = existing_info;
					   acc_array_reference_information[st_array] = Array_ref_info_vec;
					   //delete the useless WN tree
					   WN_DELETE_Tree(tree);
					   tree = wn_store;
					}
					break;
				}
			}
		}
  }
  else if(opr == OPR_ILOAD)
  {  		
  		vector_array_ref_info Array_ref_info_vec;
		acc_array_ref_info Array_ref_info;
		WN* wn_array_addr = WN_kid0(tree);
		OPERATOR addr_opr = WN_operator(wn_array_addr);
		if(addr_opr==OPR_ARRAY)
		{		
			WN* wn_base = WN_array_base(wn_array_addr);
			ST* st_array = WN_st(wn_base);
			Array_ref_info_vec = acc_array_reference_information[st_array];
			vector<acc_array_ref_info>::iterator vector_itor = Array_ref_info_vec.begin();
			for(; vector_itor != Array_ref_info_vec.end(); vector_itor++)
			{
				acc_array_ref_info existing_info = *vector_itor;
				WN* wn_array_ref = existing_info.wn_array_ref;
				if(WN_Simp_Compare_Trees(wn_array_ref, wn_array_addr) == 0)
				{
					//if ref count is larger or equal than 2
					//(unless read and write each is one time ), 
					//do the replacement
					//if(((existing_info.iread_count+existing_info.iwrite_count) > 1)
					//	&& !(existing_info.iread_count == 1 && existing_info.iwrite_count == 1))
					{
						ST* st_scalar = NULL;
						TY_IDX elem_ty = ACC_Get_ElementTYForMultiArray(st_array);
						//if the scalar haven't been created yet, create it now.
						if(existing_info.st_scalar == NULL)
						{							
						    st_scalar = New_ST( CURRENT_SYMTAB );
							char szlocalname[256];
							sprintf ( szlocalname, "%s%s_%d", acc_tmp_loop_invariant_code_emotion, 
											ST_name(st_array), kernel_tmp_licm_count);
							kernel_tmp_licm_count ++;
							
							ST_Init(st_scalar,
						      Save_Str( szlocalname),
						      CLASS_VAR,
						      SCLASS_AUTO,
						      EXPORT_LOCAL,
						      MTYPE_To_TY(TY_mtype(elem_ty)));//Put this variables in local table
						      existing_info.st_scalar = st_scalar;
						}
						else
							st_scalar = existing_info.st_scalar;

						if(existing_info.wn_load_into_scalar == NULL)
						{							
							WN* wn_value_in_array = WN_COPY_Tree(tree);
							WN* wn_load_into_scalar = WN_Stid(TY_mtype(elem_ty), 
								0, st_scalar, ST_type(st_scalar), wn_value_in_array);
							
							existing_info.wn_load_into_scalar = wn_load_into_scalar;
							if(acc_scalaization_prehand == NULL)
								acc_scalaization_prehand = WN_CreateBlock();
							WN_INSERT_BlockLast(acc_scalaization_prehand, wn_load_into_scalar);
						}
						//replace the iload with scalar load
						WN* wn_load = WN_Ldid(TY_mtype(elem_ty), 
															0, st_scalar, elem_ty);
					   //update
					   *vector_itor = existing_info;
					   acc_array_reference_information[st_array] = Array_ref_info_vec;
					   //delete the useless WN tree
					   WN_DELETE_Tree(tree);
					   tree = wn_load;
					}
					break;
				}
			}
		}
  }

  /* Walk all children */

  if (op == OPC_BLOCK) 
  {
  	WN* wn_prev = NULL;
  	WN* wn_next = NULL;
    r = WN_first(tree);
    while (r) 
	{ // localize each node in block
	  wn_prev = WN_prev(r);
	  wn_next = WN_next(r);
      r = ACC_Array_Ref_Scalar_Replacement ( r);
	  //the return r may be changed
	  //reassign the prev and next
	  if(wn_next && r != WN_prev(wn_next))
	  {
	  	WN_next(r) = wn_next;
		WN_prev(wn_next) = r;
	  }
	  if(wn_prev && r != WN_next(wn_prev))
  	  {
	  	WN_prev(r) = wn_prev;
		WN_next(wn_prev) = r;
  	  }
	  //set the first/last stmt in the block
      if (WN_prev(r) == NULL)
        WN_first(tree) = r;
      if (WN_next(r) == NULL)
        WN_last(tree) = r;  
      r = WN_next(r);      
   }
  }
  else 
  {
    for (i=0; i < WN_kid_count(tree); i++)
    {
      WN_kid(tree, i) = ACC_Array_Ref_Scalar_Replacement ( WN_kid(tree, i));
    }
  }
  return (tree);
}


//Loop invariant code motion will be appied the sequental loops inside the parallel loops
static WN* ACC_Loop_Invariant_Code_Motion_Level1(WN* tree, UINT32 reg_num_threshold)
{	
	//UINT32 reg_num_threshold = 0;
	//ACC_Kernels_Used_Register_Num
	ACC_ScanAndCount_Array_Ref(tree);
	ACC_Array_Ref_Results_Analysis(reg_num_threshold);
	//Call the replacement function
	acc_scalaization_prehand = NULL;
	acc_scalaization_afterhand =NULL;
	ACC_Array_Ref_Scalar_Replacement(tree);
	//insert the prehand code: load the array elements into scalar variable
	if(acc_scalaization_prehand)
		  WN_INSERT_BlockFirst( tree,  acc_scalaization_prehand);
	//insert the afterhand code: store the scalar variable back to array elements if they are modified
	if(acc_scalaization_afterhand)
		  WN_INSERT_BlockLast( tree,  acc_scalaization_afterhand);
	return tree;
}

//Loop invariant code motion will be appied the sequental loops inside the parallel loops
static WN* ACC_Loop_Invariant_Code_Motion_Level2(WN* tree)
{	
  OPCODE op;
  OPERATOR opr;
  INT32 i;
  WN *r;
  WN *temp;
  ST *old_sym;
  WN_OFFSET old_offset;

  /* Initialization. */
  op = WN_opcode(tree);
  opr = OPCODE_operator(op);
  if(opr == OPR_DO_LOOP)
  {  		
		WN* wn_index = WN_index(tree);
		WN* wn_loop_body = WN_do_body(tree);
		UINT32 reg_num_threshold = acc_aray_element_registers_num_left;
		acc_loop_index_var.push_back(WN_st(wn_index));	
		acc_disabled_array_scalar.clear();
		acc_array_reference_information.clear();
		//Scan the loop body
		ACC_ScanAndCount_Array_Ref(wn_loop_body);
		//Analysis the results
		ACC_Array_Ref_Results_Analysis(reg_num_threshold);
		//Call the replacement function
		acc_scalaization_prehand = NULL;
		acc_scalaization_afterhand =NULL;
		ACC_Array_Ref_Scalar_Replacement(wn_loop_body);
		//insert the prehand code: load the array elements into scalar variable
		if(acc_scalaization_prehand)
			  WN_INSERT_BlockFirst( wn_loop_body,  acc_scalaization_prehand);
		//insert the afterhand code: store the scalar variable back to array elements if they are modified
		if(acc_scalaization_afterhand)
			  WN_INSERT_BlockLast( wn_loop_body,  acc_scalaization_afterhand);
		
		if(acc_aray_element_registers_num_left > 0)
			ACC_Loop_Invariant_Code_Motion_Level2(wn_loop_body);

		acc_loop_index_var.pop_back();
		
		return tree;
  }
  /* Walk all children */

  if (op == OPC_BLOCK) 
  {
    r = WN_first(tree);
    while (r) 
	{ // localize each node in block
      ACC_Loop_Invariant_Code_Motion_Level2(r);
      r = WN_next(r);
      
   }
  }
  else 
  {
    for (i=0; i < WN_kid_count(tree); i++)
    {
      ACC_Loop_Invariant_Code_Motion_Level2 ( WN_kid(tree, i));
    }
  }
}

void ACC_Scalar_Replacement_Algorithm(WN* tree, ST* st_kernel)
{	
	//if feedback is performed, the user specified scalar replacement will be ignored.
	st_current_kernel = st_kernel;
	if((acc_scalarization_enabled&&Enable_UHACCFeedback==ACC_REGISTER_FEEDBACK_NONE)
			|| Enable_UHACCFeedback>=ACC_REGISTER_FEEDBACK_PHASE1)
	{
		UINT32 reg_num_threshold;
		
		if(Enable_UHACCFeedback>=ACC_REGISTER_FEEDBACK_PHASE1)
		{
			if(acc_target_arch == ACC_ARCH_TYPE_NVIDIA)
				ACC_Parse_Feedback_Register_Info_nvidia();
			else if(acc_target_arch == ACC_ARCH_TYPE_APU)
				ACC_Parse_Feedback_Register_Info_apu();
			
			UINT32 kernel_register = ACC_Kernels_Used_Register_Num(st_kernel);
			if(kernel_register >= MAX_REGISTERS_ALLOWED_PER_KERNEL)
			{
				acc_aray_element_registers_num_left = 0;
			}
			else
			{
				reg_num_threshold = MAX_REGISTERS_ALLOWED_PER_KERNEL - kernel_register;
			}
		}
		else //if(Enable_UHACCFeedback == ACC_REGISTER_FEEDBACK_NONE)
		{
			if(Enable_UHACCRegNum < 0)
				reg_num_threshold = ARRAY_ELEMENT_SCALARIZATION_THRESHOLD_DEFAULT;
			else
				reg_num_threshold = Enable_UHACCRegNum;
		}
	   	if(reg_num_threshold > 0)
	   		ACC_Loop_Invariant_Code_Motion_Level1(tree, reg_num_threshold);
	   	if(acc_scalarization_level2_enabled && acc_aray_element_registers_num_left > 0)
	   		ACC_Loop_Invariant_Code_Motion_Level2(tree);

	   
		acc_array_reference_information.clear();
		acc_disabled_array_scalar.clear();
	}
}

