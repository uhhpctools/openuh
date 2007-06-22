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


#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "basic.h"
#include "string_utils.h"
#include "objects.h"
#include "option_names.h"
#include "options.h"
#include "option_seen.h"
#include "opt_actions.h"
#include "get_options.h"
#include "errors.h"
#include "lang_defs.h"
#include "phases.h"   /* for determine_ld_phase() */
#include "file_names.h"
#include "file_utils.h"
#include "pathscale_defs.h"
#include "run.h"

string_list_t *objects;
string_list_t *lib_objects;
static string_list_t *cxx_prelinker_objects;
static string_list_t *ar_objects; 
static string_list_t *library_dirs;

static int check_for_whirl(char *name);

void
init_objects (void)
{
 	objects = init_string_list();
 	lib_objects = init_string_list();
 	cxx_prelinker_objects = init_string_list();
 	ar_objects = init_string_list();
	library_dirs = init_string_list();
}

/* This function figure out the full path of components <comp_name>
 * ued by the gcc/g++ <gcc_name>. The full path will be store into
 * <full_path> by this function, the caller should set the <full_path_len>
 * to the capacity of of <full_path> before call this function.
 */
void
find_full_path_of_gcc_file (char* const gcc_name, char* const comp_name,
                   char* full_path, int full_path_len) {
        int n = 3, i = 0;
        FILE *path_file;
        char *p;
        char **argv;
        buffer_t buf;
        char* tmp_name;

        if (abi == ABI_N32) {
          n++;
        }
        argv = (char **) alloca(n*sizeof(char*));
        argv[i++] = gcc_name;
        if (abi == ABI_N32) {
          argv[i++] = "-m32";
        }
        sprintf(buf, "-print-file-name=%s", comp_name);
        argv[i++] = buf;
        argv[i++] = NULL;

        tmp_name = create_temp_file_name ("gc");
        run_simple_program (argv[0], argv, tmp_name);

        /* now read the path */
        path_file = fopen(tmp_name, "r");
        if (path_file == NULL) {
                internal_error("couldn't open %s tmp file", comp_name);
                return;
        }
        if (fgets (full_path, full_path_len, path_file) == NULL) {
                internal_error("couldn't read %s tmp file", comp_name);
        }
        fclose(path_file);
        if (full_path[0] != '/') {
                internal_error("%s path not found", comp_name);
        }
        /* remove the trailing '\n' and/or '\r' */
        p = full_path + strlen(full_path) - 1;
        while (p >= full_path && (*p == '\r' || *p == '\n')) {
            *p = '\0';
        }
}

/* search library_dirs for the crt file */
char*
find_crt_path (char *crtname)
{
        string_item_t *p;
        buffer_t buf;
        phases_t ld_phase;
 
        /* See which phase is used to link the "TRUE" objects (i.e, 
         * not the fake objects built by ipl.  
         */
        ld_phase = determine_ld_phase (FALSE);
        if (ld_phase == P_ldplus || ld_phase == P_ld) {
                /* it is up to gcc or g++ to link objects, let gcc/g++ to 
                 * determine the path of crt 
                 */
                find_full_path_of_gcc_file (get_full_phase_name(ld_phase),
                                         crtname, buf, sizeof(buf)); 
                return string_copy (buf);
        }

        for (p = library_dirs->head; p != NULL; p = p->next) {
		sprintf(buf, "%s/%s", p->name, crtname);
		if (file_exists(buf)) {
			return string_copy(buf);
		}
        }
	/* not found */
	if (option_was_seen(O_nostdlib)) {
		error("crt files not found in any -L directories:");
        	for (p = library_dirs->head; p != NULL; p = p->next) {
			fprintf(stderr, "\t%s/%s\n", p->name, crtname);
		}
		return crtname;
 	}

        sprintf (buf, "%s/%s", get_phase_dir(P_be), crtname);
        if (file_exists(buf)) { return string_copy(buf); }

        sprintf (buf, "%s/%s", get_phase_dir(P_library), crtname);
        if (file_exists(buf)) { return string_copy(buf); }

        sprintf (buf, "%s/%s", get_phase_dir(P_alt_library), crtname);
        if (file_exists(buf)) { return string_copy(buf); }

        if (option_was_seen(O_L)) {
		error("crt files not found in any -L directories:");
        	for (p = library_dirs->head; p != NULL; p = p->next) {
			fprintf(stderr, "\t%s/%s\n", p->name, crtname);
		}
		
		return crtname;
 	}

	/* use default */
	sprintf(buf, "%s/%s", get_phase_dir(P_startup), crtname);
	return string_copy(buf);
}

static void
init_given_crt_path (char *crtname, char *prog_name, char *tmp_name)
{
        char* p;
        char buf[1024];
        
        p = find_crt_path (crtname);
        strncpy (buf, p, sizeof(buf)-1);
        buf[sizeof(buf)-1] = '\0';
        
        /* drop file name and add path to library list */
        p = drop_path (&buf[0]);
        *p = '\0';
        if (debug) fprintf(stderr, "%s found in %s\n", crtname, &buf[0]);
        add_library_dir (&buf[0]);
}

/* Find out where libstdc++.so file is stored.
   Invoke gcc -print-file-name=libstdc++.so to find the path.
*/
void init_stdc_plus_plus_path (void)
{

        phases_t ld_phase = determine_ld_phase (FALSE);
        if (ld_phase != P_ldplus || ld_phase != P_ld) {
                /* use prebuilt libstdc++.so which reside at the same directory
                 * as other phases.e.g BE, since the /path/to/be has already 
                 * in the lib-path, we need not duplicate at this place. 
                 */
        } else {
                char buf[1024];
                char* p;
                find_full_path_of_gcc_file (get_full_phase_name (ld_phase), 
                                "libstdc++.so",  &buf[0], sizeof(buf));
                p = drop_path (&buf[0]);
                *p = '\0';
                if (debug) fprintf(stderr, "libstdc++.so found in %s\n", &buf[0]);
                add_library_dir (&buf[0]);
        }
}


/* only need to init crt paths if doing ipa link */
void
init_crt_paths (void)
{
	/*
	 * Have to find out where crt files are stored.
	 * Invoke gcc -print-file-name=crt* to find the path.
	 * Assume are two paths, one for crt{1,i,n} and one for crt{begin,end}.
	 */
	char *tmp_name = create_temp_file_name("gc");
	char *gcc_name = get_full_phase_name(P_ld);
	init_given_crt_path ("crtbegin.o", gcc_name, tmp_name);
	init_given_crt_path ("crt1.o", gcc_name, tmp_name);
}

/* whether option is an object or not */
boolean
is_object_option (int flag)
{
	switch (flag) {
	case O_object:
	case O_l:
	case O_all:
        case O__whole_archive:
        case O__no_whole_archive:
        case O_WlC:
		return TRUE;
	default:
		return FALSE;
	}
}

struct prof_lib 
{
    const char *name;
    int always;
};

static struct prof_lib prof_libs[] = {
    /* from glibc-profile */
    { "BrokenLocale", 0 },
    { "anl", 0 },
    { "c", 0 },
    { "crypt", 0 },
    { "dl", 0 },
    { "m", 0 },
    { "nsl", 0 },
    { "pthread", 0 },
    { "resolv", 0 },
    { "rpcsvc", 0 },
    { "rt", 0 },
    { "util", 0 },
    /* from our own libraries */
    { "instr", 1 },
    { "msgi", 1 },
    { "mv", 1 },
    { "fortran", 1 },
    /* { "pscrt", 1 }, */
    { NULL, 0 },
};

int prof_lib_exists(const char *lib)
{
    char *path;
    int exists;
    asprintf(&path, "%s/lib%s_p.a", abi == ABI_N32 ? "/usr/lib" : "/usr/lib64",
	     lib);
    exists = access(path, R_OK) == 0;
    free(path);
    return exists;
}

void add_library(string_list_t *list, const char *lib)
{
    if (option_was_seen(O_profile)) {
	for (struct prof_lib *l = prof_libs; l->name; l++) {
	    if (strcmp(l->name, lib) != 0)
		continue;
	    if (!l->always && !prof_lib_exists(lib))
		continue;
	    lib = concat_strings(lib, "_p");
	}
    }

    add_string(list, concat_strings("-l", lib));
}

/* library list options get put in object list,
 * so order w.r.t. libraries is preserved. */
void
add_object (int flag, char *arg)
{
	phases_t ld_phase = determine_ld_phase (ipa == TRUE);

    /* cxx_prelinker_object_list contains real objects, -objectlist flags. */
	switch (flag) {
	case O_l:
		/* when -lm, implicitly add extra math libraries */
		if (strcmp(arg, "m") == 0 ||
		    strcmp(arg, "mpath") == 0) {	// bug 5184
			/* add -lmv -lmblah */
			add_library(lib_objects, "mv");
			if (xpg_flag && invoked_lang == L_f77) {
				add_library(lib_objects, "m" );
			} else {
				add_library(objects, "m");
			}
			if (invoked_lang == L_CC) {
			    add_library(cxx_prelinker_objects, "mv");
			    add_library(cxx_prelinker_objects, "m");
			}
#ifdef TARG_X8664
			extern boolean link_with_mathlib;
			// Bug 4680 - It is too early to check target_cpu so we
			// set a flag here to note that -lm was seen and later 
			// use this to add -lacml_mv in add_final_ld_args.
			link_with_mathlib = 1;
#endif
		}

		/* xpg fort77 has weird rule about putting all libs after objects */
		if (xpg_flag && invoked_lang == L_f77) {
			add_library(lib_objects, arg);
		} else {
			add_library(objects, arg);
		}
		if (invoked_lang == L_CC) {
		    add_library(cxx_prelinker_objects, arg);
		}
		break;
	case O_object:
	       if (dashdash_flag && arg[0] == '-') {
		 add_string(objects,"--");
		 dashdash_flag = 1;
	       }
	       if (strncmp(arg, "-l", 2) == 0)
		   add_object(O_l, arg + 2);
	       else
		   add_string(objects, arg);
	       if (invoked_lang == L_CC) {
		   add_string(cxx_prelinker_objects, arg);
	       }

	       break;
	case O_WlC:
	       if (ld_phase == P_ld || ld_phase == P_ldplus) {
	         add_string(objects, concat_strings("-Wl,", arg));
	       } else {
	         /* the arg would look like "-F,arg1,arg2,argn", 
	          * each token delimited by comma should be passed 
	          * to linker separately.
	          */
	         char* dup_str, *next_arg;
	         dup_str = string_copy (arg); /*so <arg> remain unchanged */
	         next_arg = strtok (dup_str, ",");
	         do {
	           add_string (objects, next_arg);
	         } while (next_arg = strtok (NULL, ","));
	       }
	       break;
	case O__whole_archive:
	       if (ld_phase == P_ld || ld_phase == P_ldplus) {
	         add_string(objects, "-Wl,-whole-archive");
	       } else {
	         add_string(objects, "-whole-archive");
	       }
	       break;
	case O__no_whole_archive:
	       if (ld_phase == P_ld || ld_phase == P_ldplus) {
	         add_string(objects, "-Wl,-no-whole-archive");
	       } else {
	         add_string(objects, "-no-whole-archive");
	       }
	       break;
	default:
		internal_error("add_object called with not-an-object");
	}
}

/* append object files to the ar_objects list. */
void
add_ar_objects (char *arg)
{
    add_string(ar_objects, arg);
}

/* append objects to end of list */
void
append_objects_to_list (string_list_t *list)
{
	// If without -ipa, don't accept IPA-created objects.
	if (ipa != TRUE) {
	  int has_ipa_obj = FALSE;
	  string_item_t *p;
	  for (p = objects->head; p != NULL; p = p->next) {
	    char *filename = p->name;
	    if (check_for_whirl(filename) == TRUE) {
	      error("IPA-created object %s not allowed without -ipa", filename);
	      has_ipa_obj = TRUE;
	    }
	  }
	  if (has_ipa_obj == TRUE)
	    do_exit(1);
	}
	append_string_lists (list, objects);
	append_string_lists (list, lib_objects);
}

/* append cxx_prelinker_objects to end of list */
void
append_cxx_prelinker_objects_to_list (string_list_t *list)
{
	append_string_lists (list, cxx_prelinker_objects);
}

void
append_ar_objects_to_list(string_list_t *list)
{
    append_string_lists (list, ar_objects);
}

void
append_libraries_to_list (string_list_t *list)
{
        string_item_t *p;
        for (p = library_dirs->head; p != NULL; p = p->next) {
		add_string(list, concat_strings("-L", p->name));
        }
}

void
dump_objects (void)
{
	printf("objects:  ");
	print_string_list (stdout, objects);
}

void
add_library_dir (char *path)
{
	add_string(library_dirs, path);
}

string_list_t *
get_library_dirs(void)
{
	return library_dirs;
}

void
add_library_options (void)
{
	int flag;
	buffer_t mbuf;
	buffer_t rbuf;
	char *suffix = NULL;
	char *mips_lib = NULL;
	char *proc_lib = NULL;
	char *lib = NULL;
	/*
	 * 32-bit libraries go in /usr/lib32. 
	 * 64-bit libraries go in /usr/lib64.
	 * isa-specific libraries append /mips{2,3,4}.
	 * non_shared libraries append /nonshared.
	 */
	switch (abi) {
#ifdef TARG_MIPS
	case ABI_N32:
	case ABI_I32:
		append_phase_dir(P_library, "32");
		append_phase_dir(P_startup, "32");
		break;
	case ABI_64:
		append_phase_dir(P_library, "64");
		append_phase_dir(P_startup, "64");
		break;
#else
        case ABI_N32:
	case ABI_64:
		break;
#endif
	case ABI_I64:
		break;
	case ABI_IA32:
 		break;
	default:
		internal_error("no abi set? (%d)", abi);
	}
}


// Check whether the option should be turned into a linker option when pathcc
// is called as a linker.
boolean
is_maybe_linker_option (int flag)
{
  // Example:
  //  case O_static:
  //    return TRUE;
  //    break;

  switch (flag) {
    default:
      break;
  }
  return FALSE;
}

// Add the linker version of the option.
void
add_maybe_linker_option (int flag)
{
  // Add ',' in front of the option name to indicate that the option is active
  // only if pathcc is called as a linker.  For example:
  //  case O_static:
  //    add_string(objects, ",-Wl,-static");
  //    break;

  switch (flag) {
    default:
      break;
  }
}

// If is_linker is TRUE, then turn the potential linker options into real
// linker options; otherwise delete them.
void
finalize_maybe_linker_options (boolean is_linker)
{
  string_item_t *p;

  if (is_linker) {
    // Potential linker options begin with ','.
    for (p = objects->head; p != NULL; p = p->next) {
      if (p->name[0] == ',') {
	// Remove the ',' in front.
        char *new_str = string_copy(&(p->name[1]));
	p->name = new_str;
      }
    }
  } else {
    string_item_t *prev = NULL;
    int deleted = FALSE;
    for (p = objects->head; p != NULL; p = p->next) {
      // Potential linker options begin with ','.
      if (p->name[0] == ',') {
	// Put back the non-linker version of the option if necessary.
	char *str = p->name;

	// Currently there is nothing, but if there is, follow this example:
	//   if (!strcmp (str, ",-Wl,-static")) {
	//     add_option_seen (O_static);
	//   }

	// Delete the option.
	if (prev == NULL) {
	  objects->head = p->next;
	} else {
	  prev->next = p->next;
	}
	deleted = TRUE;
      } else {
	prev = p;
      }
    }

    // Update the tail.
    if (deleted) {
      string_item_t *tail = NULL;
      for (p = objects->head; p != NULL; p = p->next) {
	tail = p;
      }
      objects->tail = tail;
    }
  }
}

// Check for ELF files containing WHIRL objects.  Code taken from
// ../cygnus/bfd/ipa_cmdline.c.

#include <elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

// Check to see if this is an ELF file and then if it is a WHIRL object.
#define ET_SGI_IR   (ET_LOPROC + 0)
static int
check_for_whirl(char *name)
{
    int fd = -1;
    char *raw_bits = NULL;
    int size,bufsize;
    Elf32_Ehdr *p_ehdr = NULL;
    struct stat statb;
    int test;
    
    fd = open(name, O_RDONLY, 0755);
    if (fd < 0)
	return FALSE;

    if ((test = fstat(fd, &statb) != 0)) {
    	close(fd);
	return FALSE;
    }

    if (statb.st_size < sizeof(Elf64_Ehdr)) {
    	close(fd);
    	return FALSE;
    }
    
    bufsize = sizeof(Elf64_Ehdr);
    
    raw_bits = (char *)alloca(bufsize*4);

    size = read(fd, raw_bits, bufsize);
    
		/*
		 * Check that the file is an elf executable.
		 */
    p_ehdr = (Elf32_Ehdr *)raw_bits;
    if (p_ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
	p_ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	p_ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
	p_ehdr->e_ident[EI_MAG3] != ELFMAG3) {
	    close(fd);
	    return(FALSE);
    }

    if(p_ehdr->e_ident[EI_CLASS] == ELFCLASS32){
    	Elf32_Ehdr *p32_ehdr = (Elf32_Ehdr *)raw_bits;
	if (p32_ehdr->e_type == ET_SGI_IR) {
	    close(fd);
	    return TRUE;
	}
    }
    else {
	Elf64_Ehdr *p64_ehdr = (Elf64_Ehdr *)raw_bits;
	if (p64_ehdr->e_type == ET_SGI_IR) {
	    close(fd);
	    return TRUE;
	}
     }

    close(fd);
    return FALSE;
    
}
