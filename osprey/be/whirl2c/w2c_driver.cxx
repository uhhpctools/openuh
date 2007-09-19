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


/* ====================================================================
 * ====================================================================
 *
 * Module: w2c_driver.c
 * $Revision: 1.1.1.1 $
 * $Date: 2005/10/21 19:00:00 $
 * $Author: marcel $
 * $Source: /proj/osprey/CVS/open64/osprey1.0/be/whirl2c/w2c_driver.cxx,v $
 *
 * Revision history:
 *  07-Oct-95 - Original Version
 *
 * Description:
 *
 *  Defines an interface for a DSO version of whirl2c, with facilities
 *  for translating a PU at a time or arbitrary subexpressions.
 *
 *  Note that before any of these routines can be called, general
 *  WHIRL accessor routine initiation must have occurred. 
 *  Furthermore, there is no longer any concept of a .B input file
 *  (although we assume there may be a .B input file-name), only a
 *  WN* input tree and SYMTABs.
 *
 * ====================================================================
 * ====================================================================
 */

#ifdef _KEEP_RCS_ID
static char *rcs_id = "$Source: /proj/osprey/CVS/open64/osprey1.0/be/whirl2c/w2c_driver.cxx,v $ $Revision: 1.1.1.1 $";
#endif /* _KEEP_RCS_ID */

#include <sys/elf_whirl.h>  /* for WHIRL_REVISION */
#include <time.h>
#include "whirl2c_common.h" /* For defs.h, config.h, erglob.h, etc. */
#include "config_clist.h"   /* For CLIST command line parameters */
#include "config_list.h"    /* For List_Cite */
#include "w2cf_parentize.h" /* For W2CF_Parent_Map and W2FC_Parentize */
#include "file_util.h"      /* For Last_Pathname_Component */
#include "flags.h"          /* for OPTION_GROUP */
#include "timing.h"         /* Start/Stop Timer */
#include "wn_lower.h"       /* For lowering Fortran specific operations */

#include "PUinfo.h"
#include "ty2c.h"
#include "st2c.h"
#include "tcon2c.h"
#include "wn2c.h"
#include "w2c_driver.h"

/*----------------------------------------------------*/
//-----------------------------------------------------------
//  This class is intended to move text blocks in a c source file
//  particularly to move nested PUs back to their parent functions'
//  scopes for the whirl2c output file
//
// Several important assumptions
// 1. a nest PU starts with "void __ompdo_xxxn" , "void __ompregion_xxxn" ,
//             ends with "} /* xxx */"
// 2. its parent PU has a landmark for the last variable
//   declaration statement
//
// It needs some enhancement to be more flexible to handle any nested PUs.
//
// Typical usage: 
//  1. read source file into member variable list <string>
//      TextProcessor tproc ("test1.c");
//  2. process file
//      tproc.process ();
//  3. output the change source to a file
//      tproc.Output ("result.c");
//
//  Author: Chunhua Liao, University of Houston, 
//  May, 17 2005
//  Modified: June, 9
//  This source file is formated using 'indent' command.
//-----------------------------------------------------------

#include <iostream>
#include <string>
#include <list>
#include <algorithm>
#include <iterator>
#include <fstream>
#include <stdio.h>

using namespace std;

// A class to store source files, move text blocks and write it back.

/*
class TextProcessor
{
public:
  string mfilename;
  int lineCount;
    list<string> srcLines;
    TextProcessor ()
  {
  };
  // Read file into list of strings
  TextProcessor (string filename)
  {
    string s;
    ifstream in (filename.c_str ());

    srcLines.clear ();
    mfilename = filename;
    while (getline (in, s))
      srcLines.push_back (s);
    in.close ();
  }

  int getLineCount ()
  {
    return srcLines.size ();
  }

  // move a text block from f to l to position after target in the srcLines list
  bool moveBlock (list < string >::iterator f,
		  list < string >::iterator l,
		  list < string >::iterator target);

  //move all nested pus back to their parents
  bool process ();

  // write result to a file
  void Output (const string & filename)
  {
    ofstream out (filename.c_str ());
    copy (srcLines.begin (), srcLines.end (), ostream_iterator < string >
	  (out, "\n"));
    out.close ();
  };

// search function for a pattern from iter_begin in normal or reverse order
// return the iterator for the first match
  list < string >::iterator find_pattern (const char *ipattern,
			   list <string >::iterator iter_begin, 
		           list <string >::iterator iter_end,
                             bool forder = true)
  {
    list < string >::iterator iter;
     
 //   if (iter_begin == srcLines.end())
	//  iter_begin = srcLines.begin (); 
//
//    if (iter_end == srcLines.end()) {
//          iter_end = srcLines.end ();
//    } 
//
    if (forder)
      {				// normal search
	for (iter = iter_begin; iter != iter_end; iter++)
	  {
	    string::size_type pos = (*iter).find (ipattern);
	    if (pos != string::npos)
	      return iter;
	  }
      }
    else			// reverse search
      {
  
        Is_True(iter_end!=srcLines.end(),"w2c_driver: iterator is illegal"); 
          
        
	for (iter=iter_end; iter != iter_begin; iter--)
	  {
	    string::size_type pos = (*iter).find (ipattern);
	    if (pos != string::npos)
	      return iter;
	  }
      }

    return srcLines.end();
  }

};

// move a text block from f to l to position after target in the srcLines list
bool
  TextProcessor::moveBlock (list < string >::iterator f,
			    list < string >::iterator l,
			    list < string >::iterator target)
{
  list < string > tempList;
//  if ((f == NULL) || (l == NULL) || (target == NULL))
//    return false;
  tempList.clear ();
  tempList.splice (tempList.begin (), srcLines, f, ++l);	// ++l to include l, original is [f,l)
  srcLines.splice (++target, tempList);	//insert after target
  return true;

}

bool TextProcessor::process ()
{

//1. search for the start  of a nested pu 

  list < string >::iterator iter1, iter2, iter3,iter4, iter_next, tmp;
  iter1 = find_pattern ("void __omp",iter2,iter3);

//  if (iter1 != NULL)
//    {
      do
	{
//2. search for the end iterators of a nested pu
	  iter2 = find_pattern ("}//  __omp", iter1,tmp);
	  iter_next = iter2;	// mark start point for next search
	  iter_next++;
//2.5 change the pattern inside the nested pu just in case,add one more space
          iter4 = find_pattern ("  // Begin_of_nested_PU(s)", iter1, iter2, false);
//          if (iter4) 
          (*iter4).insert(9," ");
//3. find its parent pu via traverse back from the begining of the block
          iter3 = find_pattern ("  // Begin_of_nested_PU(s)", tmp, iter1, false);

//4. move the nested pu back to its parent pu

	  moveBlock (iter1, iter2, iter3);
//5. search for the next one
	  iter1 = find_pattern ("void __omp", iter_next,tmp);
	}
      while (iter1);
 //   }				//end if

  return true;
}

*/
/*----------------------------------------------------*/

#ifdef COMPILE_UPC
#include <upc_symtab_utils.h>
#endif
/* Avoid errors due to uses of "int" in stdio.h macros.
 */
#undef int

//TODO: Liao global_data.c
//the maximum line length for the global_data.c file
#define MAX_LINE_LEN 2000


#ifdef COMPILE_UPC
extern void fdump_tree(FILE *f, WN *wn);
#endif
/* ====================================================================
 *
 * Local data and macros.
 *
 * ====================================================================
 */

/* Default extensions for input/outfile files: */
static const char *W2C_File_Extension[W2C_NUM_FILES] = 
{
   ".c",       /* original input file */
   ".w2c.h",   /* .h output file */
   ".w2c.c",   /* .c output file */
   ".w2c.loc",  /* .loc output file */
#ifdef COMPILE_UPC
   ".global_data.c", /* initialization input file */
#endif // TODO:Liao
};

/* CITE extensions for input/outfile files: */
static const char *W2C_Cite_Extension[W2C_NUM_FILES] = 
{
   ".c",	        /* original input file */
   "-after-lno.h",	/* .c output file */
   "-after-lno.c",	/* .h output file */
   ".loc",	        /* .loc output file */
   //TODO   ".global_data.c", /* initialization input file */
};

/* ProMPF extensions for input/outfile files: */
static const char *W2C_Prompf_Extension[W2C_NUM_FILES] = 
{
   ".c",	 /* original input file */
   ".mh",	 /* transformed header file */
   ".m",	 /* transformed source file */
   ".anl_srcpos" /* srcpos mapping (temporary) output file */
};

/* Get the right extension: */
#define W2C_Extension(i) \
	(W2C_Prompf_Emission? W2C_Prompf_Extension[i] : \
	 (List_Cite ? W2C_Cite_Extension[i] : W2C_File_Extension[i]))


/* statements that can be skipped in w2c translation
 */
#define W2C_MAX_SKIP_ITEMS 128
static W2CF_SKIP_ITEM Skip[W2C_MAX_SKIP_ITEMS+1];
static INT Next_Skip_Item = 0;


/* W2C status information */
static BOOL        W2C_Outfile_Initialized = FALSE;
static BOOL        W2C_Initialized = FALSE;
static CONTEXT     Global_Context = INIT_CONTEXT;
static const char *W2C_Progname = "";
static const char *W2C_File_Name[W2C_NUM_FILES] = 
                                          {NULL, NULL, NULL, NULL};
static BOOL        File_Is_Created[W2C_NUM_FILES] = 
                                          {FALSE, FALSE, FALSE, FALSE};
//TODO                                          {FALSE, FALSE, FALSE, FALSE, FALSE};
static MEM_POOL     W2C_Parent_Pool;

/* External data set through command-line options */
FILE *W2C_File[W2C_NUM_FILES] = {NULL, NULL, NULL, NULL};
BOOL  W2C_Enabled = TRUE;           /* Invoke W2C */
BOOL  W2C_Verbose = TRUE;           /* Show translation information */
BOOL  W2C_No_Pragmas = FALSE;       /* Do not emit pragmas */
BOOL  W2C_Emit_Adims = FALSE;       /* Emit comments for array dims */
BOOL  W2C_Emit_Prefetch = FALSE;    /* Emit comments for prefetches */
BOOL  W2C_Emit_All_Regions = FALSE; /* Emit cmplr-generated regions */
BOOL  W2C_Emit_Linedirs = FALSE;    /* Emit preproc line-directives */
BOOL  W2C_Emit_Nested_PUs = FALSE;  /* Emit code for nested PUs */
BOOL  W2C_Before_CG = FALSE;        /* Invoke whirl2c before CG , Liao*/
BOOL  W2C_Emit_Frequency = FALSE;   /* Emit feedback frequency info */
BOOL  W2C_Emit_Cgtag = FALSE;       /* Emit codegen tags for loop */
BOOL  W2C_Lower_Fortran = FALSE;    /* Lower Fortran intrinsics and io */
BOOL  W2C_Emit_Omp = FALSE;         /* Force OMP pragmas wherever possible */
INT32 W2C_Line_Length = 0;   /* Max output line length; zero==default */

/* External data set through the API or otherwise */
BOOL          W2C_Only_Mark_Loads = FALSE; /* Only mark, do not xlate loads */
BOOL          W2C_Purple_Emission = FALSE; /* Emitting purple extracted srcs */
BOOL          W2C_Prompf_Emission = FALSE; /* Emitting prompf xformed sources */
const WN_MAP *W2C_Construct_Map = NULL;    /* Construct id mapping for prompf */
WN_MAP        W2C_Frequency_Map = WN_MAP_UNDEFINED; /* Frequency mapping */
BOOL          W2C_Cplus_Initializer = FALSE; /* Whether to call C++ init */


/* ====================================================================
 *
 * Process_Filename_Options()
 *
 *    Once the command line is parsed, check for option interaction.
 *
 * Open_Read_File()
 *    Opens the file with the given name and path for reading.
 *
 * Open_Append_File()
 *
 *    Opens the file with the given name for appending more to its
 *    end if it already exists, or to create it if it does not
 *    already exist.
 *
 * Open_Create_File()
 *
 *    Same as Open_Append_File(), but a new file is always created,
 *    possibly overwriting an existing file.
 *
 * Close_File()
 *
 *    Closes the given file if different from NULL and not stdout or
 *    stderr.
 *
 * Open_W2c_Output_File()
 *
 *    Assuming that Process_Filename_Options() has been called, open
 *    the given kind of output file.  No effect if the file is already
 *    open, and the output will be appended to the output file if the
 *    file has already been created by this process.
 *
 * Close_W2c_Output_File()
 *
 *    If the file-pointer is non-NULL, we assume the file is open and
 *    close it.  Otherwise, this operation has no effect.
 *
 * ====================================================================
 */

static void 
Process_Filename_Options(const char *src_filename, const char *irb_filename)
{
   /* No processing is currently necessary for -W2C:loc_file,
    * which both indicates that a source to source location
    * mapping should be generated and the name of the file into
    * which it should be written.
    *
    * The name of the original source can be used to derive the 
    * names of the output .c and .h files when these are not 
    * explicitly provided.  If no original source file-name 
    * (-W2C:src_file) has been specified, then we assume the name
    * given by src_file_name, irb_file_name, or "anonymous.c".  
    * The -W2C:doth_file -W2C:dotc_file names are derived from the
    * resultant -W2C:src_file, unless they are explicitly provided.
    *
    * Postcondition:
    *
    *   (W2C_File_Name[W2C_ORIG_FILE] != NULL) &&
    *   (W2C_File_Name[W2C_DOTH_FILE] != NULL)       &&
    *   (W2C_File_Name[W2C_DOTC_FILE] != NULL)
    */
   #define MAX_FNAME_LENGTH 256-7 /* allow for suffix ".w2c.c\0" */
   static char filename[MAX_FNAME_LENGTH+7];
   const char *fname;
   
   /* If we do not have an original source file name, then invent one */
   if (W2C_File_Name[W2C_ORIG_FILE] == NULL)
   {
      if (src_filename != NULL && src_filename[0] != '\0')
	 W2C_File_Name[W2C_ORIG_FILE] = src_filename;
      else if (irb_filename != NULL && irb_filename[0] != '\0')
	 W2C_File_Name[W2C_ORIG_FILE] = irb_filename;
      else
	 W2C_File_Name[W2C_ORIG_FILE] = "anonymous.c";
   }

   /* Copy the original file-name to a static buffer */
   if (strlen(W2C_File_Name[W2C_ORIG_FILE]) > MAX_FNAME_LENGTH)
   {
      W2C_File_Name[W2C_ORIG_FILE] = 
	 strncpy(filename, W2C_File_Name[W2C_ORIG_FILE], MAX_FNAME_LENGTH);
      filename[MAX_FNAME_LENGTH] = '\0';
      fprintf(stderr, 
	      "WARNING: src_file name truncated to (max=%d chars): \"%s\"\n",
	      MAX_FNAME_LENGTH, W2C_File_Name[W2C_ORIG_FILE]);
   }
   else
      W2C_File_Name[W2C_ORIG_FILE] =
	 strcpy(filename, W2C_File_Name[W2C_ORIG_FILE]);

   /* We want the output files to be created in the current directory,
    * so strip off any directory path, and substitute the suffix 
    * appropriately.
    */
   fname = Last_Pathname_Component(filename);
   if ( W2C_File_Name[W2C_DOTH_FILE] == NULL ) {
      W2C_File_Name[W2C_DOTH_FILE] = 
	 New_Extension ( fname, W2C_Extension(W2C_DOTH_FILE) );
   }
   if ( W2C_File_Name[W2C_DOTC_FILE] == NULL ) {
      W2C_File_Name[W2C_DOTC_FILE] = 
	 New_Extension ( fname, W2C_Extension(W2C_DOTC_FILE) );
   }
   if (W2C_File_Name[W2C_LOC_FILE] == NULL)
   {
      if (List_Cite || W2C_Prompf_Emission)
      {
	 W2C_File_Name[W2C_LOC_FILE] =
	    New_Extension(fname, W2C_Extension(W2C_LOC_FILE));
      }
   }
   //TODO, LIAO
   /*
   if (W2C_File_Name[W2C_DATA_FILE] == NULL) {
     W2C_File_Name[W2C_DATA_FILE] = 
       New_Extension ( fname, W2C_Extension(W2C_DATA_FILE) );
   }
   */

} /* Process_Filename_Options */


static FILE *
Open_Read_File(const char *filename)
{
   FILE *f = NULL;

   /* Open the input file */
   if (filename == NULL || 
       (f = fopen(filename, "r")) == NULL)
   {
      ErrMsg(EC_IR_Open, filename, errno);
   }
   return f;
} /* Open_Read_File */


static FILE *
Open_Append_File(const char *filename)
{
   FILE *f = NULL;

   /* Open the input file */
   if (filename == NULL || 
       (f = fopen(filename, "a")) == NULL)
   {
      ErrMsg(EC_IR_Open, filename, errno);
   }
   return f;
} /* Open_Append_File */


static FILE *
Open_Create_File(const char *filename)
{
   FILE *f = NULL;

   /* Open the input file */
   if (filename == NULL || 
       (f = fopen(filename, "w")) == NULL)
   {
      ErrMsg(EC_IR_Open, filename, errno);
   }
   return f;
} /* Open_Create_File */


static void
Close_File(const char *filename, FILE *afile)
{
  if (afile != NULL             && 
      !Same_File(afile, stdout) && 
      !Same_File(afile, stderr) && 
      fclose(afile) != 0)
  {
     Set_Error_Line(ERROR_LINE_UNKNOWN); /* No line number for error */
     ErrMsg(EC_Src_Close, filename, errno);
  }
} /* Close_File */


static void 
Open_W2c_Output_File(W2C_FILE_KIND kind)
{
   if (W2C_File[kind] == NULL)
   {
      if (File_Is_Created[kind])
      {
	 W2C_File[kind] = Open_Append_File(W2C_File_Name[kind]);
      }
      else
      {
	 W2C_File[kind] = Open_Create_File(W2C_File_Name[kind]);
	 File_Is_Created[kind] = TRUE;
      }
   } /* if (!files_are_open) */
} /* Open_W2c_Output_File */


static void
Close_W2c_Output_File(W2C_FILE_KIND kind)
{
   Close_File(W2C_File_Name[kind], W2C_File[kind]);
  W2C_File[kind] = NULL;
} /* Close_W2c_Output_File */


/* ====================================================================
 * Begin_New_Location_File: should be called when we start processing
 *     a new file for which we want locations information.
 *
 * End_Locations_File: should be called when we end processing of
 *     a new file for which we want locations information.
 *
 * Continue_Locations_File: should be called for each PU translation 
 *     for which we want locations information.  After the PU
 *     translation we should call Close_W2c_Output_File(W2C_LOC_FILE).
 *
 * ==================================================================== 
 */

static void
Begin_New_Locations_File(void)
{
   /* This should only be called every time a new output file
    * is to be generated by whirl2f.  Note that we open and
    * close the file here, to make sure we only generate entries
    * in the location file when W2C_Outfile_Translate_Pu() is called!
    */
   if (W2C_File_Name[W2C_LOC_FILE] != NULL)
   {
      /* Need to do this before writing to a file for which a 
       * SRCPOS mapping should be maintained.
       */
      if (W2C_Prompf_Emission)
      {
	 Open_W2c_Output_File(W2C_LOC_FILE);
	 Write_String(W2C_File[W2C_LOC_FILE], NULL/* No srcpos map */,
		      "SRCPOS_MAP_BEGIN\n");
      }
      else
      {
	 Open_W2c_Output_File(W2C_LOC_FILE);
	 Write_String(W2C_File[W2C_LOC_FILE], NULL/* No srcpos map */,
		      "(SRCPOS-MAP\n");
      }
    }
} /* Begin_New_Locations_File */


static void
End_Locations_File(void)
{
   /* This should only be called every time we are done with *all*
    * location information for a file.
    */
   if (W2C_File_Name[W2C_LOC_FILE] != NULL)
   {
      /* Need to do this before writing to a file for which a 
       * SRCPOS mapping should be maintained.
       */
      if (W2C_Prompf_Emission)
      {
	 Open_W2c_Output_File(W2C_LOC_FILE);
	 Write_String(W2C_File[W2C_LOC_FILE],
		      NULL/* No srcpos map */,
		      "SRCPOS_MAP_END\n");
      }
      else
      {
	 Open_W2c_Output_File(W2C_LOC_FILE);
	 Write_String(W2C_File[W2C_LOC_FILE], NULL/* No srcpos map */, ")\n");
      }
      Terminate_Token_Buffer(W2C_File[W2C_LOC_FILE]);
      Close_W2c_Output_File(W2C_LOC_FILE);
   }
} /* End_Locations_File */


static void
Continue_Locations_File(void)
{
   /* Need to do this for every PU, i.e. every time 
    * W2C_Outfile_Translate_Pu() is called.
    */
   if (W2C_File_Name[W2C_LOC_FILE] != NULL)
   {
      Open_W2c_Output_File(W2C_LOC_FILE);
   }
} /* Continue_Locations_File */


static void
Move_Locations_To_Anl_File(const char *loc_fname)
{
#define MAX_ANL_FNAME_LENGTH 256-5 /* allow for suffix ".anl\0" */
   char        cbuf[MAX_ANL_FNAME_LENGTH+1];
   INT         i, next_ch;
   FILE       *anl_file;
   FILE       *loc_file;
   const char *anl_fname;
   static char fname[MAX_ANL_FNAME_LENGTH+5];

   strncpy(fname, loc_fname, MAX_ANL_FNAME_LENGTH);
   anl_fname = Last_Pathname_Component(fname);
   anl_fname = New_Extension(anl_fname, ".anl");
   anl_file = Open_Append_File(anl_fname);
   loc_file = Open_Read_File(loc_fname);

   next_ch = getc(loc_file);
   while (next_ch != EOF)
   {
      for (i = 0; (next_ch != EOF && i < MAX_ANL_FNAME_LENGTH); i++)
      {
	 cbuf[i] = next_ch;
	 next_ch = getc(loc_file);
      }
      if (i > 0)
      {
	 cbuf[i] = '\0';
	 fputs(cbuf, anl_file);
      }
   }
   Close_File(anl_fname, anl_file);
   Close_File(loc_fname, loc_file);
   unlink(loc_fname);
} /* Move_Locations_To_Anl_File */


/* ====================================================================
 *                   Undo side-effects to the incoming WHIRL
 *                   ---------------------------------------
 *
 * W2C_Undo_Whirl_Side_Effects: This subroutine must be called after
 * every translation phase which may possibly create side-effects
 * in the incoming WHIRL tree.  The translation should thus become
 * side-effect free as far as concerns the incoming WHIRL tree.
 *
 * ==================================================================== 
 */

static void
W2C_Undo_Whirl_Side_Effects(void)
{
   Stab_Free_Tmpvars();
   Stab_Free_Namebufs();
}


/* ====================================================================
 *                   Setting up the global symbol table
 *                   ----------------------------------
 *
 * W2C_Enter_Global_Symtab: Enter the global symbols into the w2c
 * symbol table.  The global symbol-table should be the top-of the
 * stack at this point.
 *
 * ==================================================================== 
 */

static void
W2C_Enter_Global_Symbols(void)
{
   const ST *st;
   ST_IDX    st_idx;
   FLD_HANDLE   fld;
   TY_IDX    ty;

   /* Enter_Sym_Info for every struct or class type in the global
    * symbol table, with associated fields.  Do this prior to any
    * variable declarations, just to ensure that field names and
    * common block names retain their names to the extent this is
    * possible.
    */
   for (ty = 1; ty < TY_Table_Size(); ty++)
   {
      if (TY_Is_Structured(ty))
      {
	 (void)W2CF_Symtab_Nameof_Ty(ty);
	 for (fld = TY_flist(Ty_Table[ty]); !fld.Is_Null (); fld = FLD_next(fld))
	    (void)W2CF_Symtab_Nameof_Fld(fld);
      } /* if a struct */
   } /* for all types */

   /* Enter_Sym_Info for every variable, function, and const in the global
    * symbol table.
    */
   FOREACH_SYMBOL(GLOBAL_SYMTAB, st, st_idx)
   {
      if ((ST_sym_class(st) == CLASS_VAR || ST_sym_class(st) == CLASS_FUNC) &&
	  !Stab_Is_Based_At_Common_Or_Equivalence(st))
      {
	 if (ST_sym_class(st) == CLASS_VAR && ST_sclass(st) == SCLASS_CPLINIT)
	    W2C_Cplus_Initializer = TRUE;

	 (void)W2CF_Symtab_Nameof_St(st);
      }
      else if (ST_sym_class(st) == CLASS_CONST)
      {
	 (void)W2CF_Symtab_Nameof_St(st);
      }
   }
} /* W2C_Enter_Global_Symbols */


/* =================================================================
 * Routines for checking correct calling order of excported routines
 * =================================================================
 */

static BOOL
Check_Outfile_Initialized(const char *caller_name)
{
   if (!W2C_Outfile_Initialized)
      fprintf(stderr, 
	      "NOTE: Ignored call to %s(); call W2C_Outfile_Init() first!\n",
	      caller_name);
   return W2C_Outfile_Initialized;
} /* Check_Outfile_Initialized */

static BOOL
Check_Initialized(const char *caller_name)
{
   if (!W2C_Initialized)
      fprintf(stderr, 
	      "NOTE: Ignored call to %s(); call W2C_Init() first!\n",
	      caller_name);
   return W2C_Initialized;
} /* Check_Initialized */

static BOOL
Check_PU_Pushed(const char *caller_name)
{
   if (PUinfo_current_func == NULL)
      fprintf(stderr, 
	      "NOTE: Ignored call to %s(); call W2C_Push_PU() first!\n",
	      caller_name);
   return (PUinfo_current_func != NULL);
} /* Check_PU_Pushed */


/* =================================================================
 *                 EXPORTED LOW-LEVEL W2C INTERFACE
 *                 --------------------------------
 *
 * See comments in w2c_driver.h
 *
 * =================================================================
 */

BOOL
W2C_Should_Emit_Nested_PUs(void)
{
   return W2C_Emit_Nested_PUs;
} /* W2C_Should_Emit_Nested_PUs */


BOOL
W2C_Should_Before_CG(void)
{
   return W2C_Before_CG;
} /* W2C_Should_Before_CG */


void
W2C_Process_Command_Line (INT phase_argc, char * const phase_argv[],
			  INT argc, char * const argv[])
{
    /* Get the program name
     */
    if (argv[0] != NULL)
       W2C_Progname = argv[0];

    /* The processing of the CLIST group was moved out to the back-end.
     * Instead of directly using the Current_CLIST, we initiate
     * whirl2c specific variables to hold the values.
     */
   W2C_File_Name[W2C_ORIG_FILE] = CLIST_orig_filename;
   W2C_File_Name[W2C_DOTH_FILE] = CLIST_doth_filename;
   W2C_File_Name[W2C_DOTC_FILE] = CLIST_dotc_filename;
   W2C_File_Name[W2C_LOC_FILE] = CLIST_loc_filename;
   W2C_Enabled = CLIST_enabled;
   W2C_Verbose = CLIST_verbose;
   W2C_No_Pragmas = CLIST_no_pragmas;
   W2C_Emit_Adims = CLIST_emit_adims;

   // need reconsideration for -portable when multiple backends available,Liao
   //if (!Portable_compile) {
   //  W2C_Emit_Prefetch = CLIST_emit_prefetch;
   //}
   W2C_Emit_All_Regions = CLIST_emit_all_regions;
   W2C_Emit_Linedirs = CLIST_emit_linedirs;
   W2C_Emit_Nested_PUs = CLIST_emit_nested_pus;
   W2C_Before_CG = CLIST_before_cg;
   W2C_Emit_Frequency = CLIST_emit_frequency;
   W2C_Emit_Cgtag = CLIST_emit_cgtag;
   W2C_Lower_Fortran = CLIST_lower_ftn;
   W2C_Emit_Omp = CLIST_emit_omp;
   W2C_Line_Length = CLIST_line_length;

    Process_Filename_Options(Src_File_Name, Irb_File_Name);
} /* W2C_Process_Command_Line */


void
W2C_Init(void)
{
   /* Initialize the various whirl2c subcomponents, unless they
    * are already initialized.
    */
   const char * const caller_err_phase = Get_Error_Phase ();

   if (W2C_Initialized)
      return; /* Already initialized */
      
   Set_Error_Phase ( "W2C Initialization" );

   /* Create a pool to hold the parent map for every PU, one at a time.
    */
   MEM_POOL_Initialize(&W2C_Parent_Pool, "W2C_Parent_Pool", FALSE);
   MEM_POOL_Push(&W2C_Parent_Pool);

   /* Always do this first!
    */
   Initialize_Token_Buffer(FREE_FORMAT, W2C_Prompf_Emission);
   if (W2C_Line_Length > 0)
      Set_Maximum_Linelength(W2C_Line_Length);

   /* Enter the global symbols into the symbol table, since that
    * ensures these get the first priority at keeping their names 
    * unchanged (note that a w2c invented name-suffix is added to 
    * disambiguate names).
    */
   Stab_initialize_flags();
   W2CF_Symtab_Push();
   W2C_Enter_Global_Symbols();

   /* Initiate the various W2C modules.
    */
   CONTEXT_reset(Global_Context);
   TY2C_initialize(Global_Context);
   ST2C_initialize(Global_Context);
   TCON2C_initialize();
   WN2C_initialize();
   PUinfo_initialize();

   W2C_Initialized = TRUE;
   Set_Error_Phase ( caller_err_phase );
} /* W2C_Init */


void 
W2C_Push_PU(const WN *pu, WN *body_part_of_interest)
{
   if (!Check_Initialized("W2C_Push_PU"))
      return;

   Is_True(WN_opcode(pu) == OPC_FUNC_ENTRY, 
	   ("Invalid opcode for W2C_Push_PU()"));

   /* Set up the parent mapping
    */
   MEM_POOL_Push(&W2C_Parent_Pool);
   W2CF_Parent_Map = WN_MAP_Create(&W2C_Parent_Pool);
   W2CF_Parentize(pu);
   Stab_initialize();

   /* See if the body_part_of_interest has any part to be skipped
    * by the w2c translator.
    */
   if (WN_opc_operator(body_part_of_interest) == OPR_BLOCK)
   {
      Remove_Skips(body_part_of_interest, 
                   Skip,
                   &Next_Skip_Item,
                   W2C_MAX_SKIP_ITEMS,
                   TRUE /*is C*/);
   }

   PUinfo_init_pu(pu, body_part_of_interest);
} /* W2C_Push_PU */


void 
W2C_Pop_PU(void)
{
   SYMTAB_IDX symtab;

   if (!Check_Initialized("W2C_Pop_PU") ||
       !Check_PU_Pushed("W2C_Pop_PU"))
      return;

   PUinfo_exit_pu();

   /* Restore any removed statement sequence
    */
   if (Next_Skip_Item > 0)
   {
      Restore_Skips(Skip, Next_Skip_Item, TRUE /*Is C*/);
      Next_Skip_Item = 0;
   }

   /* Reset the symtab used by wn2c between PUs, such that local
    * declarations will reappear even when the same PU is repeatedly
    * translated.
    */
   symtab = CURRENT_SYMTAB;
   CURRENT_SYMTAB = GLOBAL_SYMTAB;
   WN2C_new_symtab();
   CURRENT_SYMTAB = symtab;
   Stab_finalize();

   WN_MAP_Delete(W2CF_Parent_Map);
   W2CF_Parent_Map = WN_MAP_UNDEFINED;
   MEM_POOL_Pop(&W2C_Parent_Pool);

   W2C_Frequency_Map = WN_MAP_UNDEFINED;
} /* W2C_Pop_PU */


void
W2C_Mark_Loads(void)
{
   W2C_Only_Mark_Loads = TRUE;
} /* W2C_Mark_Loads */


void
W2C_Nomark_Loads(void)
{
   W2C_Only_Mark_Loads = FALSE;
} /* W2C_Nomark_Loads */


void 
W2C_Set_Prompf_Emission(const WN_MAP *construct_map)
{
   W2C_Prompf_Emission = TRUE;
   W2C_Construct_Map = construct_map; /* Construct id mapping for prompf */
} /* W2C_Set_Prompf_Emission */


void 
W2C_Set_Frequency_Map(WN_MAP frequency_map)
{
   W2C_Frequency_Map = frequency_map; /* Feedback frequency mapping for wopt */
} /* W2C_Set_Frequency_Map */


const char *
W2C_Get_Transformed_Src_Path(void)
{
   return W2C_File_Name[W2C_DOTC_FILE];
} /* W2C_Get_Transformed_Src_Path */


void
W2C_Set_Purple_Emission(void)
{
   W2C_Purple_Emission = TRUE;
} /* W2C_Set_Purple_Emission */


void
W2C_Reset_Purple_Emission(void)
{
   W2C_Purple_Emission = FALSE;
} /* W2C_Reset_Purple_Emission */


void
W2C_def_TY(FILE *outfile, TY_IDX ty)
{
   TOKEN_BUFFER tokens;
   CONTEXT context = INIT_CONTEXT;

   if (!Check_Initialized("W2C_def_TY"))
      return;

   tokens = New_Token_Buffer();
   TY2C_translate(tokens, ty, context);
   Write_And_Reclaim_Tokens(outfile, W2C_File[W2C_LOC_FILE], &tokens);
   W2C_Undo_Whirl_Side_Effects();
} /* W2C_def_TY */


void
W2C_Translate_Global_Types(FILE *outfile)
{
   FILE *saved_hfile = W2C_File[W2C_DOTH_FILE];

   if (!Check_Initialized("W2C_Translate_Global_Types"))
      return;

   W2C_File[W2C_DOTH_FILE] = outfile;
   WN2C_translate_structured_types();
   W2C_File[W2C_DOTH_FILE] = saved_hfile;
   W2C_Undo_Whirl_Side_Effects();
} /* W2C_Translate_Global_Types */


void W2C_write_init(FILE *outfile) {

  if (!Check_Initialized("W2C_write_init")) {
    return;
  }
  
}

void
W2C_Translate_Global_Defs(FILE *outfile)
{
   FILE *saved_hfile = W2C_File[W2C_DOTH_FILE];

   if (!Check_Initialized("W2C_Translate_Global_Defs"))
      return;

   W2C_File[W2C_DOTH_FILE] = outfile;
   WN2C_translate_file_scope_defs(Global_Context);
   W2C_File[W2C_DOTH_FILE] = saved_hfile;
   W2C_Undo_Whirl_Side_Effects();
} /* W2C_Translate_Global_Defs */


const char * 
W2C_Object_Name(const ST *func_st)
{
   return W2CF_Symtab_Nameof_St(func_st);
} /* W2C_Function_Name */


void 
W2C_Translate_Stid_Lhs(char       *strbuf,
		       UINT        bufsize,
		       const ST   *stid_st, 
		       STAB_OFFSET stid_ofst, 
		       TY_IDX      stid_ty, 
		       TYPE_ID     stid_mtype)
{
   TOKEN_BUFFER tokens;
   TY_IDX       stored_ty;
   CONTEXT      context = INIT_CONTEXT;

   tokens = New_Token_Buffer();
   WN2C_stid_lhs(tokens,
		 &stored_ty,   /* Corrected stored type */
		 stid_st,      /* base symbol */
		 stid_ofst,    /* offset from base */
		 stid_ty,      /* stored type */
		 stid_mtype,   /* stored mtype */
		 context);
   Str_Write_And_Reclaim_Tokens(strbuf, bufsize, &tokens);
   W2C_Undo_Whirl_Side_Effects();
} /* W2C_Translate_Stid_Lhs */


void 
W2C_Translate_Istore_Lhs(char       *strbuf, 
			 UINT        bufsize,
			 const WN   *lhs,
			 STAB_OFFSET istore_ofst, 
                         TY_IDX      istore_ty_idx,
			 TYPE_ID     istore_mtype)
{
   TOKEN_BUFFER tokens;
   TY_IDX       stored_ty;
   CONTEXT      context = INIT_CONTEXT;

   tokens = New_Token_Buffer();
   WN2C_memref_lhs(tokens, 
		   &stored_ty,
		   lhs,            /* lhs address */
		   istore_ofst,    /* offset from this address */
                   istore_ty_idx,  /* type table index for stored object */
                   Ty_Table[istore_ty_idx].Pointed(), /* type for stored obj */
		   istore_mtype,   /* base-type for stored object */
		   context);
   Str_Write_And_Reclaim_Tokens(strbuf, bufsize, &tokens);
   W2C_Undo_Whirl_Side_Effects();
} /* W2C_Translate_Istore_Lhs */


void 
W2C_Translate_Wn(FILE *outfile, const WN *wn)
{
   TOKEN_BUFFER       tokens;
   CONTEXT            context = INIT_CONTEXT;
   const char * const caller_err_phase = Get_Error_Phase ();

   if (!Check_Initialized("W2C_Translate_Wn") ||
       !Check_PU_Pushed("W2C_Translate_Wn"))
      return;

   Start_Timer (T_W2C_CU);
   Set_Error_Phase ("WHIRL To C");

   tokens = New_Token_Buffer();
   (void)WN2C_translate(tokens, wn, context);
   Write_And_Reclaim_Tokens(outfile, W2C_File[W2C_LOC_FILE], &tokens);
   W2C_Undo_Whirl_Side_Effects();

   Stop_Timer (T_W2C_CU);
   Set_Error_Phase (caller_err_phase);
} /* W2C_Translate_Wn */


void 
W2C_Translate_Wn_Str(char *strbuf, UINT bufsize, const WN *wn)
{
   TOKEN_BUFFER       tokens;
   CONTEXT            context = INIT_CONTEXT;
   const char * const caller_err_phase = Get_Error_Phase ();

   if (!Check_Initialized("W2C_Translate_Wn_Str") ||
       !Check_PU_Pushed("W2C_Translate_Wn_Str"))
      return;

   Start_Timer (T_W2C_CU);
   Set_Error_Phase ("WHIRL To C");

   tokens = New_Token_Buffer();
   (void)WN2C_translate(tokens, wn, context);
   Str_Write_And_Reclaim_Tokens(strbuf, bufsize, &tokens);
   W2C_Undo_Whirl_Side_Effects();

   Stop_Timer (T_W2C_CU);
   Set_Error_Phase (caller_err_phase);
} /* W2C_Translate_Wn_Str */


void 
W2C_Translate_Purple_Main(FILE *outfile, const WN *pu, const char *region_name)
{
   TOKEN_BUFFER tokens;
   CONTEXT      context = INIT_CONTEXT;
   const char * const caller_err_phase = Get_Error_Phase ();

   if (!Check_Initialized("W2C_Translate_Purple_Main"))
      return;

   Is_True(WN_opcode(pu) == OPC_FUNC_ENTRY, 
	   ("Invalid opcode for W2C_Translate_Purple_Main()"));

   Start_Timer (T_W2C_CU);
   Set_Error_Phase ("WHIRL To C");

   /* Translate the function header as a purple main program
    */
   tokens = New_Token_Buffer();
   W2C_Push_PU(pu, WN_func_body(pu));
   (void)WN2C_translate_purple_main(tokens, pu, region_name, context);
   W2C_Pop_PU();
   W2C_Undo_Whirl_Side_Effects();
   Write_And_Reclaim_Tokens(outfile, W2C_File[W2C_LOC_FILE], &tokens);

   Stop_Timer (T_W2C_CU);
   Set_Error_Phase (caller_err_phase);
} /* W2C_Translate_Purple_Main */


void
W2C_Fini(void)
{
   /* Ignore this call if W2C_Outfile_Initialized, since a call to
    * W2C_Outfile_Fini() must take care of the finalization for
    * this case.
    */
   INT i;

   if (!Check_Initialized("W2C_Fini"))
      return;
   else if (!W2C_Outfile_Initialized)
   {
      Stab_Reset_Referenced_Flag(GLOBAL_SYMTAB);
      TY2C_finalize();
      ST2C_finalize();
      TCON2C_finalize();
      WN2C_finalize();
      PUinfo_finalize();
      W2CF_Symtab_Terminate();
      Stab_finalize_flags();
      if (W2C_File_Name[W2C_LOC_FILE] != NULL)
	 End_Locations_File(); /* Writes filenames-table to file */
      else
	 Terminate_Token_Buffer(NULL);

      /* Reset all global variables */
      W2C_Initialized = FALSE;
      CONTEXT_reset(Global_Context);
      W2C_Progname = "";
      for (i=0;i<W2C_NUM_FILES;i++) W2C_File_Name[i] = NULL;
      for (i=0;i<W2C_NUM_FILES;i++) File_Is_Created[i] = FALSE;
      for (i=0;i<W2C_NUM_FILES;i++) W2C_File[i] = NULL;
      W2C_Enabled = TRUE;           /* Invoke W2C */
      W2C_Verbose = TRUE;           /* Show translation information */
      W2C_No_Pragmas = FALSE;       /* Do not emit pragmas */
      W2C_Emit_Adims = FALSE;       /* Emit comments for array dims */
      W2C_Emit_Prefetch = FALSE;    /* Emit comments for prefetches */
      W2C_Emit_All_Regions = FALSE; /* Emit cmplr-generated regions */
      W2C_Emit_Linedirs = FALSE;    /* Emit preproc line-directives */
      W2C_Emit_Nested_PUs = FALSE;  /* Emit code for nested PUs */
      W2C_Before_CG = FALSE;        /* Invoke whirl2c before CG */
      W2C_Emit_Frequency = FALSE;   /* Emit feedback frequency info */
      W2C_Lower_Fortran = FALSE;    /* Lower Fortran intrinsics and io */
      W2C_Line_Length = 0;   /* Max output line length; zero==default */

      W2C_Only_Mark_Loads = FALSE;
      W2C_Cplus_Initializer = FALSE;

      MEM_POOL_Pop(&W2C_Parent_Pool);
      MEM_POOL_Delete(&W2C_Parent_Pool);
   } /* if (initialized) */
} /* W2C_Fini */


/* =================================================================
 *                  EXPORTED OUTPUT-FILE INTERFACE
 *                  ------------------------------
 *
 * This is the easiest interface for using whirl2c, and it
 * maintains output file status and produces a uniform output
 * format based on the -CLIST options.  There are strict rules
 * as to how this interface interacts with the lower-level 
 * interface described above (see .h file for details).
 *
 * W2C_Outfile_Init()
 *    Initializes the output-files for whirl2c, based on the
 *    command-line options.  Will call W2C_Init() if this has
 *    not already been done.
 *
 * W2C_Outfile_Translate_Pu()
 *    Translates a PU, presupposing W2C_Outfile_Init() has been 
 *    called.  The PU will be lowered (for Fortran) and the 
 *    output C code will be appended to the output files.
 *
 * W2C_Output_Fini()
 *    Finalizes a W2C translation by closing the output-files, and
 *    calling W2C_Fini().  Optionally, this may cause common blocks
 *    (for Fortran-to-C translation) to be appended to the generated
 *    header-file.
 *
 * =================================================================
 */

void
W2C_Outfile_Init(BOOL emit_global_decls)
{
   /* Initialize the various whirl2c subcomponents.  It is not
    * always desirable to open a .h file for global declarations,
    * and this can be suppressed (no global declarations will be
    * emitted) by passing emit_global_decls=FALSE.  For even more
    * control over the W2C translation process, use the lower-level
    * translation routines, instead of this more abstract
    * interface.
    */
   time_t systime;

   if (W2C_Outfile_Initialized)
      return; /* Already initialized */

   W2C_Outfile_Initialized = TRUE;
   if (W2C_Verbose)
   {
      if (W2C_Prompf_Emission || W2C_File_Name[W2C_LOC_FILE] == NULL)
	 fprintf(stderr, 
		 "%s translates %s into %s and %s, based on source %s\n", 
		 W2C_Progname,
		 Irb_File_Name, 
		 W2C_File_Name[W2C_DOTH_FILE], 
		 W2C_File_Name[W2C_DOTC_FILE], 
		 W2C_File_Name[W2C_ORIG_FILE]);
      else
	 fprintf(stderr, 
		 "%s translates %s into %s, %s and %s, based on source %s\n", 
		 W2C_Progname,
		 Irb_File_Name,
		 W2C_File_Name[W2C_DOTH_FILE],
		 W2C_File_Name[W2C_DOTC_FILE],
		 W2C_File_Name[W2C_LOC_FILE],
		 W2C_File_Name[W2C_ORIG_FILE]);
   } /* if verbose */

   /* Initialize the whirl2c modules!
    */
   if (!W2C_Initialized)
      W2C_Init();

   /* Open the output files (and write location mapping header).
    */
   Begin_New_Locations_File();
   Open_W2c_Output_File(W2C_DOTC_FILE);

   /* Write out a header-comment into the whirl2c generated 
    * source file.
    */
   systime = time(NULL);
   Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		"/*******************************************************\n"
		" * C file translated from WHIRL ");
   Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		((systime != (time_t)-1)? 
		 ctime(&systime) : "at unknown time\n"));
   Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		" *******************************************************/"
		"\n\n");

   /* Emit the header file if appropriate */
   if (emit_global_decls)
   {
      Open_W2c_Output_File(W2C_DOTH_FILE);

      /* Include <whirl2c.h> in the .h file */
      Write_String(W2C_File[W2C_DOTH_FILE], NULL/* No srcpos map */,
		   "/* Include builtin types and operators */\n"
		   "#include <whirl2c.h>\n\n");

      /* Include the .h file in the .c file */
      Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		   "/* Include file-level type and variable decls */\n"
		   "#include \"");
      Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		   W2C_File_Name[W2C_DOTH_FILE]);
      Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		   "\"\n\n");
#ifdef COMPILE_UPC //TODO, Liao, for include system headers and others
      if(Compile_Upc) 
	/* Include <whirl2c.h> in the .h file */
	Write_String(W2C_File[W2C_DOTH_FILE], NULL/* No srcpos map */,
		     "/* Include builtin types and operators */\n"
		     //WEI: Took external_defs out, use upc_proxy instead
		     "#include \"upcr_proxy.h\"\n#include<upcr.h>\n#include <whirl2c.h>\n\n");
      else {
	/* Include <whirl2c.h> in the .h file */
	Write_String(W2C_File[W2C_DOTH_FILE], NULL/* No srcpos map */,
		     "/* Include builtin types and operators */\n"
		     "#include \"whirl2c.h\"\n\n");
	/* Include the .h file in the .c file */
	Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		     "/* Include file-level type and variable decls */\n"
		     "#include \"");
	Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		     W2C_File_Name[W2C_DOTH_FILE]);
	Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		     "\"\n\n");  
      }
   }

   if (Compile_Upc) {
     //Add UPC runtime version info
     char buf[MAX_LINE_LEN];
     sprintf(buf, "/* UPC Runtime specification expected: %d.%d */\n", UPCR_SPEC_MAJOR, UPCR_SPEC_MINOR);
     Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE], buf);

     sprintf(buf, "#define UPCR_WANT_MAJOR %d\n", UPCR_SPEC_MAJOR);
     Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE], buf);

     sprintf(buf, "#define UPCR_WANT_MINOR %d\n", UPCR_SPEC_MINOR);
     Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE], buf);

     //Add translator build version
     sprintf(buf, "/* UPC translator version: release 2.2.0, built on %s at %s */\n",
		  __DATE__, __TIME__);
     Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE], buf);

     // add the init file to output
     Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		  "/* Included code from the initialization script */\n");
     
     FILE* in = fopen(W2C_File_Name[W2C_DATA_FILE], "r");
     
     FmtAssert(in, ("Could not open static data file"));
     
     while (fgets(buf, MAX_LINE_LEN, in) != NULL) {
       if (strncmp(buf, "###", 3) == 0) {
	 /* Include the .h file in the .c file after the system headers 
	    and other non upc headers are included */
	 Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		      "#include \"");
	 Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		      W2C_File_Name[W2C_DOTH_FILE]);
	 Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE],
		      "\"\n\n");  
       } else {
	 Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE], buf);
       }
     }
#endif
   }

   if (emit_global_decls)
      WN2C_translate_structured_types();

   W2C_Outfile_Initialized = TRUE;

} /* W2C_Outfile_Init */

void
W2C_Outfile_Translate_Pu(WN *pu, BOOL emit_global_decls)
{
   TOKEN_BUFFER       tokens;
   LOWER_ACTIONS      lower_actions = LOWER_NULL;
   const BOOL         pu_is_pushed = (PUinfo_current_func != NULL);
   const char * const caller_err_phase = Get_Error_Phase ();

   static int globals_done = 0;

   if (!Check_Outfile_Initialized("W2C_Outfile_Translate_Pu"))
      return;

   Is_True(WN_opcode(pu) == OPC_FUNC_ENTRY, 
	   ("Invalid opcode for W2C_Outfile_Translate_Pu()"));

   /* Make sure all necessary output files are open.
    */
   Continue_Locations_File();
   Open_W2c_Output_File(W2C_DOTC_FILE);
   if (emit_global_decls)
      Open_W2c_Output_File(W2C_DOTH_FILE);

   if (W2C_Emit_Nested_PUs && !W2C_Lower_Fortran)
      lower_actions = LOWER_MP;
   else if (!W2C_Emit_Nested_PUs && W2C_Lower_Fortran)
      lower_actions = LOWER_IO_STATEMENT | LOWER_INTRINSIC;
   else if(W2C_Emit_Nested_PUs && W2C_Lower_Fortran)
      lower_actions = LOWER_MP | LOWER_IO_STATEMENT | LOWER_INTRINSIC;

   if (lower_actions != LOWER_NULL)
      pu = WN_Lower(pu, lower_actions, NULL, "W2C Lowering");

   Start_Timer (T_W2C_CU);
   Set_Error_Phase ("WHIRL To C");
   if (!pu_is_pushed)
      W2C_Push_PU(pu, WN_func_body(pu));

   tokens = New_Token_Buffer();
#ifdef COMPILE_UPC
   if(Compile_Upc && debug_requested) {
     ST* tst;
     int i;
     FOREACH_SYMBOL(PU_lexical_level (Pu_Table[ST_pu (St_Table[WN_st_idx(pu)])]), tst, i) {
       emit_s2s_debug_type_info(tst, tokens);
    
       //change the types in the symbol table, otherwise the translation
       //code gets cinfused
       if (ST_class(tst) == CLASS_VAR) {
	 Set_ST_type((ST*)tst, TY_To_Sptr_Idx(ST_type(tst)));
       } else if (ST_class(tst) == CLASS_FUNC) {
	 TYLIST_IDX idx;
	 idx = TY_tylist(ST_pu_type(tst));
	 ST *lst;
	 while(Tylist_Table [idx]) {
	   TY_IDX tidx = Tylist_Table[idx];
	   if (Type_Is_Shared_Ptr(tidx)) {
	     Set_TYLIST_type (Tylist_Table [idx], TY_To_Sptr_Idx(tidx));
	     idx++;
	   }
	   else idx++;
	 }
       }
      
     }     
     if(!globals_done) {
       globals_done = 1;
       FOREACH_SYMBOL(GLOBAL_SYMTAB, tst, i) {
       if (ST_class(tst) == CLASS_VAR) {
	 Set_ST_type((ST*)tst, TY_To_Sptr_Idx(ST_type(tst)));
       } else if (ST_class(tst) == CLASS_FUNC) {
	 TYLIST_IDX idx;
	 idx = TY_tylist(ST_pu_type(tst));
	 ST *lst;
	 while(Tylist_Table [idx]) {
	   TY_IDX tidx = Tylist_Table[idx];
	   if (Type_Is_Shared_Ptr(tidx)) {
	     Set_TYLIST_type (Tylist_Table [idx], TY_To_Sptr_Idx(tidx));
	     idx++;
	   }
	   else idx++;
	 }
       }
     }
       
     }
  
 } 
#endif
   (void)WN2C_translate(tokens, pu, Global_Context);
   Write_And_Reclaim_Tokens(W2C_File[W2C_DOTC_FILE], 
			    W2C_File[W2C_LOC_FILE], 
			    &tokens);

   if (!pu_is_pushed)
      W2C_Pop_PU();

   W2C_Undo_Whirl_Side_Effects();

   Stop_Timer (T_W2C_CU);
   Set_Error_Phase (caller_err_phase);
} /* W2C_Outfile_Translate_Pu */


void
W2C_Outfile_Fini(BOOL emit_global_decls)
{
   /* This finalization must be complete enough to allow repeated
    * invocations of whirl2c during the same process life-time.
    */
   const char *loc_fname = W2C_File_Name[W2C_LOC_FILE];

   if (!Check_Outfile_Initialized("W2C_Outfile_Fini"))
      return;
   
   /* Reopen files if they are closed */
   Continue_Locations_File();
   Open_W2c_Output_File(W2C_DOTC_FILE);
   if (emit_global_decls)
      Open_W2c_Output_File(W2C_DOTH_FILE);

#ifdef COMPILE_UPC
   if (Compile_Upc) {
     Write_String(W2C_File[W2C_DOTC_FILE], W2C_File[W2C_LOC_FILE], "#line 1 \"_SYSTEM\"");
   }
#endif
   if (emit_global_decls)
   {
      TOKEN_BUFFER tokens = New_Token_Buffer();

      WN2C_translate_file_scope_defs(Global_Context);

      ST2C_Define_Common_Blocks(tokens, Global_Context);
#ifndef COMPILE_UPC
      Write_And_Reclaim_Tokens(W2C_File[W2C_DOTH_FILE], 
			       W2C_File[W2C_LOC_FILE], 
			       &tokens);
#else
      //WEI: Don't think we need this anymore, since Append_Symtab_Var now does this.
      //Could be wrong, though...
      if (!Compile_Upc) {
	Write_And_Reclaim_Tokens(W2C_File[W2C_DOTH_FILE], 
	W2C_File[W2C_LOC_FILE], 
	&tokens);
      }
#endif  //TODO, Liao, Append_Symtab_Var
   }

   Close_W2c_Output_File(W2C_LOC_FILE);
   Close_W2c_Output_File(W2C_DOTH_FILE);
   Close_W2c_Output_File(W2C_DOTC_FILE);

 /*  move a nested PU back to its parent's scope, by Liao */
 // TODO: add a new sub option within CLIST to activate this 
/* if (W2C_Emit_Nested_PUs) {
    TextProcessor tproc(W2C_File_Name[W2C_DOTC_FILE]);
    tproc.process();
    tproc.Output (W2C_File_Name[W2C_DOTC_FILE]);
    }
*/
   /* All files must be closed before doing a partial 
    * finalization, except W2C_LOC_FILE.
    */
   W2C_Outfile_Initialized = FALSE;
   W2C_Fini(); /* Sets W2C_Initialized to FALSE */

   if (W2C_Prompf_Emission && loc_fname != NULL)
   {
      /* Copy the locations file to the .anl file and remove it 
       */
      Move_Locations_To_Anl_File(loc_fname);
   }
} /* W2C_Outfile_Fini */


void
W2C_Cleanup(void)
{
   /* Cleanup in case of error condition (or a forgotten call to Anl_Fini())
    */
   Close_W2c_Output_File(W2C_LOC_FILE);
   Close_W2c_Output_File(W2C_DOTH_FILE);
   Close_W2c_Output_File(W2C_DOTC_FILE);
   if (W2C_File_Name[W2C_LOC_FILE] != NULL)
      unlink(W2C_File_Name[W2C_LOC_FILE]);
}
