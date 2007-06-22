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


/* -*-Mode: c++;-*- (Tell emacs to use c++ mode) */
// TODO:
//   *  remove unnecessary includes
//
//
#include <time.h>
#include "anl_common.h"
#include "stamp.h"           // For INCLUDE_STAMP
#include "config_promp.h"    // For Current_PROMP
#include "w2c_driver.h"      // For whirl2c.so utilities
#include "w2f_driver.h"      // For whirl2f.so utilities
#include "anl_file_mngr.h"   // For managing an analysis file
#include "w2cf_translator.h" // For C++ interface to whirl2[fc] utilities
#include "anl_func_entry.h"  // For processing FUNC_ENTRY nodes
#include "anl_driver.h" 
#include "wb_anl.h" 
#include "prompf.h" 

// Avoid errors due to uses of "int" in stdio.h macros.
//
#undef int

#pragma weak W2F_Get_Transformed_Src_Path
#pragma weak W2C_Get_Transformed_Src_Path


// ================== Define the global objects =============
// ==========================================================

// Macros that dispatch on the language in wich the extracted
// sources should be expressed (Fortran or C).
//
#define USE_C_TRANSFORMED_SRC() \
   (Language == LANG_KR_C  || Language == LANG_ANSI_C || \
    Language == LANG_CPLUS || Language == LANG_DELTA)
#define USE_F77_TRANSFORMED_SRC() \
   (Language == LANG_F77 || Language == LANG_F90)


// Imported objects
extern const char *Fe_Version;  // From be.so


// Global C++ objects
//
MEM_POOL          Anl_Default_Pool; // Pool used for temporary anl memory alloc
WN_MAP            Parent_Map;       // Used by LWN_Set_Parent/LWN_Get_Parent
ANL_DIAGNOSTICS  *Anl_Diag;         // Diagnostics handler


// Local C++ objects
//
static ANL_FILE_MNGR *Anl_File_Mngr;        // File handler
static COUNTER        Next_Construct_Id(1); // Next id to use for wn-->id map
static BOOL           Pool_Initialized = FALSE;

// Command-line options variables
//
BOOL               Anl_Owhile = FALSE;           // Generate owhile descriptors
static BOOL        Anl_Enabled = TRUE;           // Generate analysis file?
static BOOL        Anl_Verbose = FALSE;          // Show progress
static const char *Anl_Filename = NULL;          // Output file name
static const char *Anl_OrignSrc_Filename = NULL; // Original src file name
static const char *Anl_Version = NULL;           // Compiler version
static       char *Anl_Progname = NULL;          // Name of this executable


static void
Anl_Help_Page(void)
{
   fprintf(stderr,
     "\n"
     "-PROMPF<opts> will produce a \".anl\" file, and may be invoked with\n"
     "any of the following <opts>:\n"
     "\n"
     "\t\":show\" Will show .anl generation progress.\n"
     "\t\":owhile\" Will generate owhile descriptors for while loops, and\n"
       "\t\twill also try to identify while loops as for loops for C code.\n"
     "\t\":anl=<filename>\" Explicitly defines the name and path of the\n"
       "\t\toutput file to be <filename>.  The default will be a file with\n"
       "\t\tthe same name as the original, in the current working directory,\n"
       "\t\tand with the suffix \".anl\".\n"
     "\t\":src=<filename>\" Explicitly defines the name and path of the\n"
       "\t\toriginal source file to be <filename>.  The default will be\n"
       "\t\tsupplied to the compiler back-end by the driver.\n"
     );
} // Anl_Help_Page


static void
Anl_Validate_Options(ANL_DIAGNOSTICS *diag)
{
   // The option assumptions, are specified below.  When any of them
   // are violated, this routine will terminate execution with a
   // fatal assertion failure.
   //
   //   1) Anl_OrignSrc_Filename != NULL || Src_File_Name != NULL
   //
   // Postcondition: The above condition is TRUE,
   //                or else Anl_Enabled == FALSE.
   //
   if (Anl_OrignSrc_Filename == NULL)
   {
      // The original source-file must be explicitly given, either 
      // as an optional argument or as a back-end (be) argument.
      //
      if (Src_File_Name == NULL) // (2)
      {
	 Anl_Help_Page();
	 diag->Error("Missing source file name");
	 Anl_Enabled = FALSE;
      }
      else
      {
	 Anl_OrignSrc_Filename = Src_File_Name; // Our default
      }
   } // if
} // Anl_Validate_Options


static void
Derive_Anl_Filename(void)
{
   // Unless explicitly provided, we need to derive the name to be
   // used for the anl file from the Anl_OrignSrc_Filename.
   //
   char *anl_name;

   if (Anl_Filename == NULL)
   {
      if (Anl_OrignSrc_Filename != NULL)
      {
	 // Derive Anl_Filename from Anl_OrignSrc_Filename, but remove
	 // any pathnames, since output-files should reside in the 
	 // current (working) directory.
	 //
	 anl_name = Last_Pathname_Component((char *)Anl_OrignSrc_Filename);
	 Anl_Filename = New_Extension(anl_name, ".anl");
      }
   } // if
} // Derive_Anl_Filename


// ========= Define the external interface routines =========
// ==========================================================

extern "C" void
Anl_Process_Command_Line (INT phase_argc, char *phase_argv[],
			  INT argc, char *argv[])
{
   // Note that the Anl_Default_Pool cannot be used while
   // executing this subroutine, since it will not be initialized
   // until Prp_Init() is called (after a call to this routine).
   //
   if (argv[0] != NULL)
      Anl_Progname = argv[0];

   // The processing of the PROMP group was moved out to the back-end.
   // Instead of directly using Current_PROMP, we initiate
   // prompf_anl specific variables to hold the values.
   //
   Anl_Enabled = (PROMP_enabled ||
		  PROMP_owhile || 
		  PROMP_show ||
		  PROMP_anl_filename!=NULL ||
		  PROMP_src_filename!=NULL);
   Anl_Owhile = PROMP_owhile;
   Anl_Verbose = PROMP_show;
   Anl_Filename = PROMP_anl_filename;
   Anl_OrignSrc_Filename = PROMP_src_filename;
   Next_Construct_Id.Reset(PROMP_next_id);
} // Anl_Process_Command_Line


extern "C" BOOL
Anl_Needs_Whirl2c()
{
   // Precondition: Must have done Anl_Process_Command_Line()
   //
   return USE_C_TRANSFORMED_SRC();
} // Prp_Needs_Whirl2c


extern "C" BOOL
Anl_Needs_Whirl2f()
{
   // Precondition: Must have done Anl_Process_Command_Line()
   //
   return !USE_C_TRANSFORMED_SRC();
} // Anl_Needs_Whirl2f


extern "C" void 
Anl_Init()
{
   // Precondition: Must have done Anl_Process_Command_Line() and
   // loaded whirl2c.so or whirl2f.so, as is appropriate for the
   // language to be processed.  Note that the options passed on
   // to whirl2c or whirl2f are independent of options passed to
   // prompf_anl.so.
   //
   Set_Error_Phase ("ProMpf Analysis File Generation");

   // Create new mempool.
   //
   MEM_POOL_Initialize(&Anl_Default_Pool, "Anl_Default_Pool", FALSE);
   MEM_POOL_Push(&Anl_Default_Pool);
   Pool_Initialized = TRUE;
   Anl_Diag = CXX_NEW(ANL_DIAGNOSTICS(stderr), &Anl_Default_Pool);

   // Infer information missing from explicit options.
   //
   Anl_Validate_Options(Anl_Diag);
   Derive_Anl_Filename();

   if (Anl_Enabled)
   {      
      // Open the analysis file (overwrites an existing file)
      //
      Anl_File_Mngr = CXX_NEW(ANL_FILE_MNGR(Anl_Diag), &Anl_Default_Pool);
      Anl_File_Mngr->Open_Create(Anl_Filename);
      if (Anl_Diag->Error_Was_Reported())
	 Anl_Diag->Fatal("Cannot open .anl file for static analysis output!!");

      // Emit time and date information
      //
      //   systime = time(NULL);
      //   Anl_File_Mngr->Write_String("Timestamp: \"");
      //   if ((systime != (time_t)-1))
      //      Anl_File_Mngr->Write_String(ctime);
      //   else
      //      Anl_File_Mngr->Write_String("unknown time");
      //   Anl_File_Mngr->Write_String("\"\n");

      // Emit product information
      //	 
      Anl_File_Mngr->Write_String("product \"Mongoose\"\n");

      // Emit version information
      //
      Anl_File_Mngr->Write_String("version \"");
      if (Fe_Version != NULL)
	 Anl_File_Mngr->Write_String(Fe_Version);
      else
	 Anl_File_Mngr->Write_String(INCLUDE_STAMP);
      Anl_File_Mngr->Write_String("\"\n");

      // Emit language information
      //
      Anl_File_Mngr->Write_String("language \"");
      switch (Language)
      {
      case LANG_F77:
	 Anl_File_Mngr->Write_String("f77\"\n");
	 break;
      case LANG_F90:
	 Anl_File_Mngr->Write_String("f90\"\n");
	 break;
      case LANG_KR_C:    // Kernighan & Richie C
	 Anl_File_Mngr->Write_String("kr-c\"\n");
	 break;
      case LANG_ANSI_C:  // ANSI standard C
	 Anl_File_Mngr->Write_String("ansi-c\"\n");
	 break;
      case LANG_CPLUS:   // Regular C++
	 Anl_File_Mngr->Write_String("c++\"\n");
	 break;
      case LANG_DELTA:   // Delta C++
	 Anl_File_Mngr->Write_String("delta-c++\"\n");
	 break;
      case LANG_UNKNOWN:
      default:
	 Anl_File_Mngr->Write_String("unknown\"\n");
	 break;
      }

      // Emit information about the path to the flist/clist file:
      //
      if (USE_C_TRANSFORMED_SRC())
      {
	 Anl_File_Mngr->Write_String("whirl2c \"");
	 Anl_File_Mngr->Write_String(W2C_Get_Transformed_Src_Path());
      }
      else // (USE_F77_TRANSFORMED_SRC())
      {
	 Anl_File_Mngr->Write_String("whirl2f \"");
	 Anl_File_Mngr->Write_String(W2F_Get_Transformed_Src_Path());
      }
      Anl_File_Mngr->Write_String("\"\n\n");
      Anl_File_Mngr->Close_File();
   }
} // Anl_Init


extern "C" WN_MAP
Anl_Init_Map(MEM_POOL *id_map_pool)
{
  return WN_MAP32_Create(id_map_pool);
} 

extern "C" void 
Anl_Static_Analysis(WN *pu, WN_MAP id_map)
{
   // Precondition: Must have done Anl_Init()
   //
   // Note: Any memory allocations using Anl_Default_Pool (including 
   // uses of ANL_CBUFs) will be freed up upon exit from this 
   // subroutine, so *never* let such allocated objects be accessible
   // beyond the lifetime of this invokation.
   //
   // Setup the construct id mapping.  This mapping must survive until
   // after LNO and whirl2[cf] processing.
   //
   ANL_FUNC_ENTRY  *anl_func;
   W2CF_TRANSLATOR *translator;

   Set_Error_Phase ("ProMpf Analysis File Generation");
   if (Anl_Enabled)
   {
      // Cleanup from previous run!
      //
      if (Prompf_Info != NULL) { 
	Prompf_Info = NULL;
	MEM_POOL_Pop(&PROMPF_pool);
	MEM_POOL_Delete(&PROMPF_pool);
      } 

      MEM_POOL_Push_Freeze(&Anl_Default_Pool);
      Anl_File_Mngr->Open_Append(Anl_Filename);
      if (Anl_Diag->Error_Was_Reported())
	 Anl_Diag->Fatal("Cannot open .anl file for static analysis output!!");

      // Setup the parent mapping.  We only need this mapping until we
      // pop the Anl_Default_Pool.
      //
      Parent_Map = WN_MAP_Create(&Anl_Default_Pool);
      LWN_Parentize(pu);
      
      // Generate ANL original source construct information.
      //
      translator = 
	 CXX_NEW(W2CF_TRANSLATOR(pu,
				 &Anl_Default_Pool,
				 USE_C_TRANSFORMED_SRC()),
		 &Anl_Default_Pool);
      anl_func = 
	 CXX_NEW(ANL_FUNC_ENTRY(pu, 
				&Anl_Default_Pool, 
				translator,
				id_map,
				&Next_Construct_Id),
		 &Anl_Default_Pool);
      anl_func->Emit_Original_Construct(Anl_File_Mngr);
      CXX_DELETE(anl_func, &Anl_Default_Pool);
      CXX_DELETE(translator, &Anl_Default_Pool);

      // Free up the parent mapping before freeing up the mempools
      //
      WN_MAP_Delete(Parent_Map);
      Parent_Map = NULL;

      Anl_File_Mngr->Close_File();

      // Pop the mempool segment used
      //
      MEM_POOL_Pop_Unfreeze(&Anl_Default_Pool);

      // Initialize transaction log structures
      MEM_POOL_Initialize(&PROMPF_pool, "PROMPF_pool", FALSE);
      MEM_POOL_Push(&PROMPF_pool);
      Prompf_Info = CXX_NEW(PROMPF_INFO(pu, &PROMPF_pool), &PROMPF_pool);

   } // if (Anl_Enabled)

} // Anl_Static_Analysis


extern "C" INT64 
Get_Next_Construct_Id()
{
   return Next_Construct_Id.Value();
} // Get_Next_Construct_Id


extern "C" INT64
New_Construct_Id()
{
   return Next_Construct_Id.Post_Incr();
} // New_Construct_Id


extern "C" const char *
Anl_File_Path()
{
   return Anl_Filename;
} // Anl_File_Path


extern "C" void
Anl_Fini()
{
   // Precondition: Must have done Anl_Init()
   //
   if (Anl_Enabled)
   {
      Set_Error_Phase ("ProMpf Analysis File Generation");

      // Cleanup Prompf_Info
      if (Prompf_Info != NULL) { 
	Prompf_Info = NULL;
	MEM_POOL_Pop(&PROMPF_pool);
	MEM_POOL_Delete(&PROMPF_pool);
      } 
      WN_MAP_Delete(Prompf_Id_Map);

      // Reset the prp state to what it was initially.
      Anl_Verbose = FALSE;          // Show progress
      Anl_Filename = NULL;          // Output file name
      Anl_OrignSrc_Filename = NULL; // Original src file name
      Anl_Version = NULL;           // Compiler version
      Anl_Enabled = FALSE;          // Generate analysis file?

      CXX_DELETE(Anl_File_Mngr, &Anl_Default_Pool);
      Anl_File_Mngr = NULL;
   } // if (Anl_Enabled)

   // Delete diagnostic system and mempool
   //
   CXX_DELETE(Anl_Diag, &Anl_Default_Pool);
   MEM_POOL_Delete(&Anl_Default_Pool);
   Pool_Initialized = FALSE;
} // Anl_Fini


extern "C" void
Anl_Cleanup()
{
   // Cleanup in case of error condition (or a forgotten call to Anl_Fini())
   //
   if (Anl_File_Mngr != NULL && Anl_File_Mngr->File_Is_Open())
       Anl_File_Mngr->Close_File();
} // Anl_Cleanup
