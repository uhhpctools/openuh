/*
 Runtime library for supporting Coarray Fortran 

 Copyright (C) 2011 University of Houston.

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

 Contact information: 
 http://www.cs.uh.edu/~hpctools
*/

#include <stdlib.h>
#include <string.h>


/* for fortran command argument passing */

#define MAX_ARGLEN 255
extern int iargc_(void);
extern void getarg_(int *n, char *s, int slen);

static char **fargv;
static int   fargc;

/*
 * runtime operation for fortran command passing process 
 */
void farg_init(int *argc, char ***argv)
{
  int i;
  char arg[MAX_ARGLEN];
  char *cptr;
  fargc=iargc_() + 1;
  fargv = malloc( sizeof(char*)*fargc);

  for(i=0;i<fargc;i++)
  {
    getarg_(&i,arg,MAX_ARGLEN);
    cptr = arg;

    while (*cptr != ' ') cptr++; 
    *cptr = '\0';

    fargv[i] = malloc(sizeof(char) * strlen(arg));
    strcpy(fargv[i], arg);
  }

  *argc = fargc;
  *argv = fargv;
}

void farg_free()
{
  int i;
  if (fargv) {
    for(i=0;i<fargc;i++) {
      if (fargv[i]) free(fargv[i]);
    }
    free(fargv);
  }
}
