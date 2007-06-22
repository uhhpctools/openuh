/*
 * Copyright 2003, 2004, 2005 PathScale, Inc.  All Rights Reserved.
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


#include <sys/types.h>
#include <sys/stat.h>
#include "main.h"			// for arg_vector[0]
#include "ipc_weak.h"			// weak definition for arg_vector 

#include "defs.h"
#include "errors.h"                     // for ErrMsg
#include "erglob.h"                     // error code
#include "cxx_memory.h"			// for CXX_NEW
#include "ipc_link.h"

#include "lib_phase_dir.h"              // for BINDIR etc

// We break up the linker command line into two parts. with the list of
// WHIRL object created by IPA inserted in between.  Sine we no longer
// maintain a 1-1 relationship between the WHIRL .o's and .I's, we don't
// know the names of the .I's until after they are created
ARGV *ld_flags_part1;			// argv before the first WHIRL obj
ARGV *ld_flags_part2;			// rest of the link line
ARGV *current_ld_flags;

// For archives, we create a comma-separated list for the set of member
// objects selected.

ARGV *comma_list;
UINT32 comma_list_byte_count = 0;

#if defined(TARG_IA64) || defined(TARG_X8664)

#ifdef TARG_X8664
#define LINKER_NAME "gcc"
#define LINKER_NAME_WITH_SLASH "/gcc"
#else  /* TARG_IA64 */
#define LINKER_NAME "ld"
#define LINKER_NAME_WITH_SLASH "/ld"
#define DYNAMIC_LINKER "-dynamic-linker /lib/ld-linux-ia64.so.2"
#endif /* KEY */

static char* concat_names(char* a , char* b)
{
    char * buf;
    buf = (char *)malloc(strlen(a)+strlen(b)+1);
    strcpy(buf, a);
    strcat(buf, b);
    return (buf);
}


// Returns true if path refers to an ordinary file.
static bool file_exists(const char* path)
{
  if (!path || strlen(path) == 0)
    return false;

  struct stat buf;
  return stat(path, &buf) == 0 && S_ISREG(buf.st_mode);
}


static char* get_linker_name(int argc, char** argv)
{
    char * toolroot = getenv("TOOLROOT");
    if (!toolroot) { toolroot = "" ; }

    char* linker_name;
    char *where_am_i = getenv("COMPILER_BIN");

    if (where_am_i) {
	char *slash = strrchr (where_am_i, '/');
	#ifdef PSC_TO_OPEN64
	asprintf (&linker_name, "%.*s/../" OPEN64_TARGET "/bin/" LINKER_NAME,
	#endif
		  (int)(slash - where_am_i), where_am_i);
	if (file_exists (linker_name)) {
	    return linker_name;
	}
	free (linker_name);
    }

    linker_name = concat_names (toolroot, PHASEPATH LINKER_NAME_WITH_SLASH);
    if (file_exists (linker_name)) {
        return linker_name;
    }
    free (linker_name);

    linker_name = concat_names (toolroot, GNUPHASEPATH LINKER_NAME_WITH_SLASH);
    if (file_exists (linker_name)) {
        return linker_name;
    }
    free (linker_name);

    linker_name = concat_names (toolroot, BINPATH LINKER_NAME_WITH_SLASH);
    if (file_exists (linker_name)) {
        return linker_name;
    }
    free (linker_name);
        
    linker_name = concat_names (toolroot, ALTBINPATH LINKER_NAME_WITH_SLASH);
    if (file_exists (linker_name)) {
        return linker_name;
    }
    free (linker_name);

    return (LINKER_NAME);
}

#endif

void
ipa_init_link_line (int argc, char** argv)
{
    ld_flags_part1 = CXX_NEW (ARGV, Malloc_Mem_Pool);
    ld_flags_part2 = CXX_NEW (ARGV, Malloc_Mem_Pool);
    comma_list = CXX_NEW (ARGV, Malloc_Mem_Pool);

    // Push the path and name of the final link tool
#if defined(TARG_IA64) || defined(TARG_X8664)

#if 0
    char *t_path = arg_vector[0];
    char *t_name;
    char *buf;
    int len = strlen(t_path);
    int i,new_len;
    
    t_name = t_path + (len-1);
    for (i=0; i<len; i++,t_name--) {
    	if (*t_name == '/')
	    break;
    }
    new_len = len-i;
    buf = (char *)malloc(strlen(LINKER_NAME)+new_len+1);
    
    strncpy(buf,t_path,new_len);
    strcat(buf,LINKER_NAME);
    ld_flags_part1->push_back (buf);
    
    free(buf);
#endif
    ld_flags_part1->push_back (get_linker_name(arg_count, arg_vector));
#if defined(TARG_IA64) && defined(CROSS_COMPILATION) 
    ld_flags_part1->push_back (DYNAMIC_LINKER);
#endif

#else
    ld_flags_part1->push_back (arg_vector[0]);
#endif

    // Copy the arguments that have been saved
    for (INT i = 0; i < argc; ++i) {
	ld_flags_part1->push_back (argv[i]);
    }
    current_ld_flags = ld_flags_part1;
} // ipa_init_link_line


void
ipa_add_link_flag (const char* str)
{
    current_ld_flags->push_back (str);
} // ipa_add_link_flag


#ifdef KEY
// Prepend "../" to name if it is a relative pathname.
extern "C" char *
ipa_add_parent_dir_to_relative_pathname (const char *name)
{
  if (name[0] != '/') {
    const char *prefix = "../";
    char *buf;
    buf = (char *) malloc(strlen(prefix)+strlen(name)+1);
    strcpy(buf, prefix);
    strcat(buf, name);
    return (buf);
  }
  return (char*) name;
}

extern "C" void
ipa_erase_link_flag (const char* str)
{
  ARGV::iterator p;
  bool changed; /* bug 9772 */

  do {
    changed = FALSE;
    for (p = ld_flags_part1->begin();
	 p != ld_flags_part1->end();
	 p++) {
      if (!strcmp(str, *p)) {
	ld_flags_part1->erase(p);
	changed = TRUE;
	break;
      }
    }
  } while (changed);

  do {
    changed = FALSE;
    for (p = ld_flags_part2->begin();
	 p != ld_flags_part2->end();
	 p++) {
      if (!strcmp(str, *p)) {
	ld_flags_part2->erase(p);
	changed = TRUE;
	break;
      }
    }
  } while (changed);
} // ipa_erase_link_flag
#endif


// mark where the list of .I's should be inserted
void
ipa_insert_whirl_obj_marker ()
{
    current_ld_flags = ld_flags_part2;
}

// this is just to let C programs access it too.
extern "C" void 
ipa_insert_whirl_marker(void)
{
    ipa_insert_whirl_obj_marker();
}

void
ipa_add_comma_list (const char* name)
{
    comma_list->push_back (name);
    comma_list_byte_count += strlen(name) + 1;
}

void
ipa_compose_comma_list (const char* name)
{
    if (comma_list->empty ())
	return;

    char* result = (char *) MEM_POOL_Alloc (Malloc_Mem_Pool,
					    comma_list_byte_count);
    ARGV::const_iterator first = comma_list->begin ();

    strcpy (result, *first);
    free ((void*) *first++);		// storage for this string was
					// allocated by ld using malloc, so 
					// don't use MEM_POOL_FREE
    while (first != comma_list->end ()) {
	strcat (result, ",");
	strcat (result, *first);
	free ((void*) *first);
	++first;
    }

    comma_list_byte_count = 0;
    comma_list->erase (comma_list->begin(), comma_list->end());

    ipa_add_link_flag ("-ar_members");
    ipa_add_link_flag (result);

    char* p = (char *) MEM_POOL_Alloc (Malloc_Mem_Pool, strlen(name) + 1);
    strcpy (p, name);
    ipa_add_link_flag (p);

} // ipa_compose_comma_list


ARGV *
ipa_link_line_argv (const ARGV* output_files,
                    const char* dir, 
		    const char* symtab_base)
{
    ARGV* argv = CXX_NEW (ARGV, Malloc_Mem_Pool);

    argv->reserve (ld_flags_part1->size () + 
    	    	   ld_flags_part2->size () +
		   output_files->size () + 
		   1);

    argv->insert (argv->end (),
		  ld_flags_part1->begin (), 
		  ld_flags_part1->end ());
    
    argv->insert (argv->end (),
		  output_files->begin (), 
		  output_files->end ());

    if (symtab_base && symtab_base[0] != 0) {
	char* symtab =
	    static_cast<char*>(malloc(strlen(dir) + strlen(symtab_base) + 2));
	if (!symtab)
	    ErrMsg (EC_No_Mem, "ipa_link_line_argv");
	strcpy(symtab, dir);
	strcat(symtab, "/");
	strcat(symtab, symtab_base);
	argv->push_back(symtab);
    }
    
    argv->insert (argv->end (),
		  ld_flags_part2->begin (), 
		  ld_flags_part2->end ());

    return argv;
} // ipa_link_line_argv

#ifdef TODO
void
process_cord_obj_list (FILE *f)
{
    int i;

    for (i = 0; i < cur_argc; ++i) {
	if (arg_tag[i] == ARG_OBJ) {
	    fputs (arg_list[i].obj->other_name ?
		   arg_list[i].obj->other_name : arg_list[i].obj->name, f);
	    fputc ('\n', f);
	}
    }
} /* process_cord_obj_list */
#endif 
