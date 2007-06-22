/*
 *  Copyright (C) 2006. QLogic Corporation. All Rights Reserved.
 */

/*
 * Copyright 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "phases.h"
#include "lang_defs.h"
#include "string_utils.h"
#include "file_names.h"
#include "file_utils.h"
#include "errors.h"
#include "opt_actions.h"
#include "option_seen.h"
#include "option_names.h"
#include "run.h"
#include "version.h"

extern int errno;

boolean keep_flag = FALSE;

string_list_t *count_files = NULL;
static string_list_t *temp_files = NULL;
#ifdef KEY /* Bug 11265 */
string_list_t *isystem_dirs = NULL;
#endif /* KEY Bug 11265 */
static char *tmpdir;
static char *saved_object = NULL;

#define DEFAULT_TMPDIR	"/tmp"

static string_pair_list_t *temp_obj_files = NULL;


/* get object file corresponding to src file */
char *
get_object_file (char *src)
{
	// bug 2025
	// Create .o files in /tmp in case the src dir is not writable.
	if (!(keep_flag || (ipa == TRUE) || remember_last_phase == P_any_as)) {
	  char *obj_name = change_suffix(src, "o");
	  string_pair_item_t *p;
	  FOREACH_STRING_PAIR (p, temp_obj_files) {
	    if (strcmp (STRING_PAIR_KEY(p), obj_name) == 0)
	      return STRING_PAIR_VAL(p);
	  }
	  // Create temp file name as in create_temp_file_name.
	  buffer_t buf;
	  sprintf(buf, "cco.");
	  char *mapped_name = tempnam (tmpdir, buf);
	  add_string_pair (temp_obj_files, obj_name, mapped_name);
	  return mapped_name;
	}

	// Handle IPA .o files corresponding to sources with the same basename,
	// e.g., a.c and foo/a.c.  Create unique .o files by substituting '/'
	// in the source name with '%'.  Bugs 9097, 9130.
	if (ipa == TRUE &&
	    !option_was_seen(O_c) &&
	    keep_flag != TRUE) {
	  char *p;
	  src = strdupa(src);
	  for (p = src; *p != '\0'; p++) {
	    if (*p == '/')
	      *p = '%';
	  }
	}

	return change_suffix(drop_path(src), "o");
}

/*
 * Need temp file names to be same if use same suffix
 * (because this can be called for both producer and consumer
 * of temp file), but also need names that won't conflict.
 * Put suffix in standard place so have easy way to check 
 * if file already created. 
 * Use tempnam to generate unique file name;
 * tempnam verifies that file is writable.
 */
char *
create_temp_file_name (char *suffix)
{
	buffer_t buf;
	buffer_t pathbuf;
	size_t pathbuf_len;
	char *s;
	string_item_t *p;
	/* use same prefix as gcc compilers;
	 * tempnam limits us to 5 chars, and may have 2-letter suffix. */
#ifdef KEY
	int len = strlen(suffix);
	if (len > 4) {
	  internal_error("create_temp_file_name: suffix too long: %s", suffix);
	  suffix = "xx";	// Let driver continue until error exit.
	} else if (len > 2) {
	  sprintf(buf, "%s.", suffix);
	} else
#endif
	sprintf(buf, "cc%s.", suffix);
	sprintf(pathbuf, "%s/%s", tmpdir, buf); /* full path of tmp files */
	pathbuf_len = strlen(pathbuf);

	for (p = temp_files->head; p != NULL; p = p->next) {
		/* Can't use get_suffix here because we don't actually
		 * want the suffix. tempnam may return a value with a period
		 * in it. This will confuse our duplicates check below.
		 * We can't change get_suffix, because in other cases we
		 * actually want the right-most period. foo.bar.c
		 * We are guaranteed here that the first period after the last
		 * directory divider is the position we want because we chose
		 * its contents above.
		 */
		char *file_name = strrchr(p->name, '/');
		if (file_name == NULL)
			file_name = p->name;
		s = strchr(file_name, '.');
		/* we know that s won't be null because we created a string
		 * with a period in it. */
		s++;
		/* assume that s points inside p->name,
		 * e.g. /tmp/ccB.abc, s points to a */
		if (strncmp(s-pathbuf_len, pathbuf, pathbuf_len) == 0) {
			/* matches the prefix and suffix character */
			return p->name;
		}
	}
	/* need new file name */
	s = tempnam (tmpdir, buf);
	add_string (temp_files, s);
	return s;
}

char *
construct_name (char *src, char *suffix)
{
	if (keep_flag || current_phase == remember_last_phase) {
		char *srcname;
		/* 
		 * if -c -o <name>, then use name.suffix
		 * (this helps when use same source to create different .o's)
		 * if outfile doesn't have .o suffix, don't do this.
		 */
		if (outfile && option_was_seen(O_c) && get_suffix(outfile))
			srcname = outfile;
		else
			srcname = src;
		return change_suffix(drop_path(srcname), suffix);
	} else {
		return create_temp_file_name (suffix);
	}
}

/* use given src name, but check if treated as a temp file or not */
char *
construct_given_name (char *src, char *suffix, boolean keep)
{
	char *s;
	s = change_suffix(drop_path(src), suffix);
	if (keep || current_phase == remember_last_phase) {
		return s;
	} else {
		s = string_copy(s);
		add_string_if_new (temp_files, s);
		return s;
	}
}

void
mark_saved_object_for_cleanup ( void )
{
	if (saved_object != NULL)
	add_string_if_new (temp_files, saved_object);
}

/* Create filename with the given extension; eg. foo.anl from foo.f */
char *
construct_file_with_extension (char *src, char *ext)
{
	return change_suffix(drop_path(src),ext);
}

void
init_temp_files (void)
{
        tmpdir = string_copy(getenv("TMPDIR"));
        if (tmpdir == NULL) {
                tmpdir = DEFAULT_TMPDIR;
	} 
	else if (!is_directory(tmpdir)) {
		error("$TMPDIR does not exist: %s", tmpdir);
	} 
	else if (!directory_is_writable(tmpdir)) {
		error("$TMPDIR not writable: %s", tmpdir);
	} 
	else if (tmpdir[strlen(tmpdir)-1] == '/') {
		/* drop / at end so strcmp matches */
		tmpdir[strlen(tmpdir)-1] = '\0';
	}
	temp_files = init_string_list();

	temp_obj_files = init_string_pair_list();
}

void
init_count_files (void)
{
        count_files = init_string_list();
}

static char *report_file;

void
init_crash_reporting (void)
{
	#ifdef PSC_TO_OPEN64
	if ((report_file = getenv("OPEN64_CRASH_REPORT")) != NULL)
	#endif
		goto bail;

	#ifdef PSC_TO_OPEN64 
	if (asprintf(&report_file, "%s/open64_crash_XXXXXX", tmpdir) == -1) {
	#endif
		report_file = NULL;
		goto bail;
	}
	
	if (mkstemp(report_file) == -1) {
		report_file = NULL;
		goto bail;
	}

	#ifdef PSC_TO_OPEN64
	setenv("OPEN64_CRASH_REPORT", report_file, 1);
	#endif
bail:
	return;
}

static int save_count;

static int
save_cpp_output (char *path)
{
	char *save_dir, *save_path, *final_path;
	FILE *ifp = NULL, *ofp = NULL;
	char *name = drop_path(path);
	struct utsname uts;
	char buf[4096];
	int saved = 0;
	size_t nread;
	char *suffix;
	char *home;
	time_t now;
	int i;

	if (strncmp(name, "cci.", 4) == 0)
		suffix = ".i";
	else if (strncmp(name, "ccii.", 5) == 0)
		suffix = ".ii";
	else
		goto bail;

	if ((ifp = fopen(path, "r")) == NULL)
		goto bail;

	#ifdef PSC_TO_OPEN64
	if ((save_dir = getenv("OPEN64_PROBLEM_REPORT_DIR")) == NULL &&
	    (home = getenv("HOME")) != NULL) {
		asprintf(&save_dir, "%s/.open64-bugs", home);
	}
	#endif

	if (save_dir && mkdir(save_dir, 0700) == -1 && errno != EEXIST) {
		save_dir = NULL;
	}

	if (save_dir == NULL) {
		save_dir = tmpdir;
	}

	asprintf(&save_path, "%s/%s_error_XXXXXX", save_dir, program_name);

	if (mkstemp(save_path) == -1) {
		goto b0rked;
	}

	if ((ofp = fopen(save_path, "w")) == NULL) {
		goto b0rked;
	}
	
	now = time(NULL);
	#ifdef PSC_TO_OPEN64
	fprintf(ofp, "/*\n\nOpen64 compiler problem report - %s",
		ctime(&now));
	fprintf(ofp, "Please report this problem to http://bugs.open64.net/\n");
	#endif
	fprintf(ofp, "If possible, please attach a copy of this file with your "
		"report.\n");
	fprintf(ofp, "\nPLEASE NOTE: This file contains a preprocessed copy of the "
		"source file\n"
		"that may have led to this problem occurring.\n");

	uname(&uts);
	fprintf(ofp, "\nCompiler command line (%s ABI used on %s system):\n",
		abi == ABI_N32 ? "32-bit" : "64-bit",
		uts.machine);

	fprintf(ofp, " ");
	for (i = 0; i < saved_argc; ++i)
		if (saved_argv[i] &&
		    strcmp(saved_argv[i], "-default_options") != 0) {
			int len;
			len = quote_shell_arg(saved_argv[i], buf);
			buf[len] = '\0';
			fprintf(ofp, " %s", buf);
		}
	fprintf(ofp, "\n\n");

	fprintf(ofp, "Version %s build information:\n",
		compiler_version);
	fprintf(ofp, "  Changeset %s\n", cset_id);
	fprintf(ofp, "  Built by %s@%s in %s\n", build_user,
		build_host, build_root);
	fprintf(ofp, "  Build date %s\n", build_date);
	
	if (report_file) {
		int newline = 1;
		struct stat st;
		FILE *rfp;

		if (stat(report_file, &st) == -1)
			goto no_report;
		
		if (st.st_size == 0)
			goto no_report;

		fprintf(ofp, "\nDetailed problem report:\n");
		if ((rfp = fopen(report_file, "r")) == NULL) {
			goto no_report;
		}

		while (fgets(buf, sizeof(buf), rfp) != NULL) {
			int len = strlen(buf);
			if (newline)
				fputs("  ", ofp);
			fputs(buf, ofp);
			newline = buf[len - 1] == '\n';
		}
		if (!newline)
			putc('\n', ofp);

		fclose(rfp);
	}

no_report:	
	if (string_list_size(error_list)) {
		string_item_t *i;
		fprintf(ofp, "\nInformation from compiler driver:\n");
		FOREACH_STRING(i, error_list) {
			fprintf(ofp, "  %s\n", STRING_NAME(i));
		}
	}

	fprintf(ofp, "\nThe remainder of this file contains a preprocessed copy of "
		"the\n"
		"source file that appears to have led to this problem.\n\n*/\n");
	
	while ((nread = fread(buf, 1, sizeof(buf), ifp)) > 0) {
		size_t nwrit;
		if ((nwrit = fwrite(buf, 1, nread, ofp)) < nread) {
			if (nwrit != 0)
				errno = EFBIG;
			goto b0rked;
		}
	}

	#ifdef PSC_TO_OPEN64
	fprintf(ofp, "\n/* End of Open64 compiler problem report. */\n");
	#endif
	
	asprintf(&final_path, "%s%s", save_path, suffix);
	rename(save_path, final_path);

	if (save_count == 0) {
		#ifdef PSC_TO_OPEN64
		fprintf(stderr, "Please report this problem to "
			"http://bugs.open64.net/\n");
		#endif
	}

	fprintf(stderr, "Problem report saved as %s\n", final_path);
	save_count++;
	saved = 1;
	
	goto bail;
b0rked:
	fprintf(stderr, "Could not save problem report to %s: %s\n",
		save_path, strerror(errno));
bail:
	if (ifp != NULL)
		fclose(ifp);
	if (ofp != NULL)
		fclose(ofp);
		
	return saved;
}

void
cleanup (void)
{
	/* cleanup temp-files */
	string_item_t *p;
	int status;
	if (temp_files == NULL) return;
	for (p = temp_files->head; p != NULL; p = p->next) {
		if (debug) printf("unlink %s\n", p->name);
		if (execute_flag) {
			if (internal_error_occurred)
				save_cpp_output(p->name);
			status = unlink(p->name);
			if (status != 0 && errno != ENOENT) {
				internal_error("cannot unlink temp file %s", p->name);
				perror(program_name);			
			}
		}
	}
	temp_files->head = temp_files->tail = NULL;

	if (save_count) {
		fprintf(stderr, "Please review the above file%s and, "
			"if possible, attach %s to your problem report.\n",
			save_count == 1 ? "" : "s",
			save_count == 1 ? "it" : "them");
	}
}

void
mark_for_cleanup (char *s)
{
	add_string_if_new (temp_files, s);
}

void
cleanup_temp_objects ()
{
  // Delete the mapped files.
  string_pair_item_t *p;
  FOREACH_STRING_PAIR (p, temp_obj_files) {
    char *s = STRING_PAIR_VAL(p);
    int status = unlink (s);
    if (status != 0 && errno != ENOENT) {
      internal_error("cannot unlink temp object file %s", s);
      perror(program_name);
    }
  }
  if (report_file) {
    unlink(report_file);
  }
}

#ifdef KEY
char *
get_report_file_name() {
  return report_file;
}
#endif
