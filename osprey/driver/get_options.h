/*
 * Copyright 2003, 2004 PathScale, Inc.  All Rights Reserved.
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


#include "basic.h"

extern char *option_name;	/* current option name */

extern char *optargs;          /* string argument following fixed prefix */
extern int optargd;             /* number argument following fixed prefix */

/* add arg to prefix for indirect option */
extern int add_string_option (int flag, char *arg);
extern int add_string_option_or_dash (int flag, char *arg);

/* iteratively get a single option from the command line */
extern int get_option(int *argi, char *argv[]);

/* if an alias option, return the base "real" option */
extern int get_real_option_if_aliased (int flag);

/* do initial pass through args to check for options that affect driver */
extern void check_for_driver_controls (int argc, char *argv[]);

extern string_list_t *feedback_files;

/* explicitly set language */
void set_explicit_lang(const char *flag, const char *lang);
