/*
 * Copyright (C) 2007. QLogic Corporation. All Rights Reserved.
 */
/*
 * Copyright 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

#ifndef _GCCFE_WGEN_OMP_CHECK_STACK_H_
#define _GCCFE_WGEN_OMP_CHECK_STACK_H_

typedef enum {
	wgen_unknown,
	wgen_cscf,
	wgen_other_ompscope,
	wgen_omp_parallel,
	wgen_omp_task,
	wgen_omp_for,
	wgen_omp_single,
	wgen_omp_sections,
	wgen_omp_parallel_sections,
	wgen_omp_parallel_for,
	wgen_omp_section,
	wgen_omp_master,
	wgen_omp_critical,
	wgen_omp_ordered,
	wgen_omp_barrier,
	wgen_omp_taskwait,
	wgen_omp_flush,
	wgen_omp_atomic,
	wgen_omp_threadprivate,
	//OpenACC clause
	wgen_acc_wait,
	wgen_acc_update,
	wgen_acc_declare,
	wgen_acc_cache,
	wgen_acc_parallel,
	wgen_acc_kernels,
	wgen_acc_data,
	wgen_acc_hostdata,
	wgen_acc_loop
}  WGEN_CHECK_KIND;
typedef enum{
   clause_none          =0x0 ,
   clause_default        =0x1 ,
   clause_firstprivate    =0x2 ,
   clause_if             =0x4 ,     
   clause_lastprivate    =0x8 ,
   clause_copyin         =0x10 ,
   clause_copyprivate    =0x20,
   clause_ordered        =0x40,
   clause_private        =0x80,
   clause_reduction      =0x100,
   clause_schedule      =0x200,
   clause_shared        =0x400,
   clause_untied        =0x800,
   clause_in            =0x1000,
   clause_out           =0x2000,
   clause_target        =0x4000

}WGEN_CLAUSE_KIND;

typedef enum{
	acc_clause_none          		=0x00000000 ,
	acc_clause_if        			=0x00000001 ,
	acc_clause_async        		=0x00000002 ,
	acc_clause_num_gangs        	=0x00000004 ,
	acc_clause_num_workers      	=0x00000008 ,
	acc_clause_vector_length    	=0x00000010 ,
	acc_clause_copy        			=0x00000020 ,
	acc_clause_copyin        		=0x00000040 ,
	acc_clause_copyout        		=0x00000080 ,
	acc_clause_create        		=0x00000100 ,
	acc_clause_present        		=0x00000200 ,
	acc_clause_present_or_copy  	=0x00000400 ,
	acc_clause_present_or_copyin	=0x00000800 ,
	acc_clause_present_or_copyout	=0x00001000 ,  
	acc_clause_present_or_create    =0x00002000 ,
	acc_clause_acc_resident         =0x00004000 , 
	acc_clause_use_device        	=0x00008000 ,
	acc_clause_deviceptr        	=0x00010000 ,
	acc_clause_collapse        		=0x00020000 ,
	acc_clause_seq        			=0x00040000 ,
	acc_clause_private        		=0x00080000 ,  
	acc_clause_first_private        =0x00100000 ,  
	acc_clause_varlist        		=0x00200000 ,  
	acc_clause_intexp        		=0x00400000 ,  
	acc_clause_reduction        	=0x00800000 ,  
	acc_clause_gang        			=0x01000000 ,  
	acc_clause_worker        		=0x02000000 ,  
	acc_clause_vector        		=0x04000000 ,  
	acc_clause_independent       	=0x08000000 ,  
	acc_clause_host        			=0x10000000 ,  
	acc_clause_device        		=0x20000000 ,
	acc_clause_parm				    =0x40000000 
}WGEN_ACC_CLAUSE_KIND;

typedef struct check_stmt
{
   WGEN_CHECK_KIND kind;     
   char* name;        // for critical directive
   int cflag;
   unsigned int accflag; //for ACC clauses 
   int linenum;         
   int filenum;
   WN *wn_prag;
   WN *region;        // for a stack of regions    
}  CHECK_STMT;

static CHECK_STMT *omp_check_stack; //check stack     
static CHECK_STMT *omp_check_sp;   
static CHECK_STMT *omp_check_last;  //point to the last one of the check stack
static int omp_check_size;      //size of the check stack now
static int omp_check_num;      //number of stmt in check stack

#define ENLARGE(x) (x + (x >> 1))
#define OMP_CHECK_STACK_SIZE 32

extern  void WGEN_CS_Init();
extern void WGEN_CS_push( WGEN_CHECK_KIND kind,int lnum, int fnum);
extern void SEtd(SRCPOS srcpos);
extern CHECK_STMT *WGEN_CS_top(void);
extern CHECK_STMT *WGEN_CS_enclose(void);
extern WGEN_CHECK_KIND WGEN_CS_pop(WGEN_CHECK_KIND kind);
extern void WGEN_Set_Prag(WN *omp_prag); 
extern void WGEN_Set_Nameflag(char* name);
extern void WGEN_Set_Cflag(WGEN_CLAUSE_KIND flag);
extern void WGEN_Set_LFnum(CHECK_STMT *cs,int lnum,int fnum);
extern bool WGEN_Check_Cflag(CHECK_STMT* cs, WGEN_CLAUSE_KIND flag);
extern void WGEN_Set_Region (WN *);
/*For OpenACC, by daniel. tian*/
extern void WGEN_ACC_Set_Cflag(WGEN_ACC_CLAUSE_KIND flag);

//search from the inside level to outside level, return inmost find level
extern int WGEN_CS_Find(WGEN_CHECK_KIND kind); 
extern CHECK_STMT* WGEN_CS_Find_Rtn(WGEN_CHECK_KIND kind);
extern int WGEN_CS_Find_Cflag(WGEN_CHECK_KIND kind,WGEN_CLAUSE_KIND flag);
extern int WGEN_CS_Find_fgname(WGEN_CHECK_KIND kind,char* name); 
//utilities:
//search in the name

extern bool WGEN_is_top(WGEN_CHECK_KIND kind);
extern bool WGEN_is_top_next(WGEN_CHECK_KIND kind);
//test if sub1 and sub2 bind to the same bindkind:
extern bool WGEN_bind_to_same(WGEN_CHECK_KIND sub1,WGEN_CHECK_KIND sub2,WGEN_CHECK_KIND bind);

#endif

