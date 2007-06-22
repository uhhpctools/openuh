/*
 * Copyright 2004, 2005 PathScale, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

#include "lang_defs.h"
#include "license.h"
#include "pathscale_defs.h"
#include "run.h"

#define VERSION "1.1"

int subverbose ;
char subflags[100*1024] ;

void obtain_license (char *exedir, int argc, char *argv[]) {
    int pipes[2] ;
    int pid ;
    char exename[MAXPATHLEN] ;
    char language[10] ;
    struct stat st ;
    int i ;
    char *l ;

    const char *errortext = "Unable to obtain subscription.  The PathScale compiler cannot run without a subscription.\nPlease see http://www.pathscale.com/subscription/1.1/msgs.html for details.\n" ;
   
    l = getenv ("PATHSCALE_SUBSCRIPTION_CLIENT") ;
    if (l == NULL) {
        snprintf (exename, sizeof(exename), "%s/subclient", exedir) ;
        if (subverbose) {
            fprintf (stderr, "Subscription client: looking for %s\n", exename) ;
        }
        if (stat (exename, &st) != 0) {
            snprintf (exename, sizeof(exename), "%s/subclient",
		      get_phase_dir(P_be)) ;
            if (subverbose) {
                fprintf (stderr, "Subscription client: looking for %s\n", exename) ;
            }
            if (stat (exename, &st) != 0) {
                fprintf (stderr, errortext) ;
                do_exit (1); 
            }
        }
    } else {
        strcpy (exename, l) ;
        if (subverbose) {
            fprintf (stderr, "Subscription client: looking for %s\n", exename) ;
        }
        if (stat (exename, &st) != 0) {
            fprintf (stderr, errortext) ;
            do_exit (1); 
        }
    }

    subflags[0] = '\0' ;
    for (i = 1 ; i < argc ; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'o':		// don't pass these as they might contain sensitive stuff
            case 'I':
            case 'L':
            case 'D':
            case 'U':
                break ;
            case 'W':
                if (strncmp (argv[i], "-Wl", 3) == 0) {
                }
                goto addflag ;
            case 'T':
                if (strncmp (argv[i], "-TENV:ipa_ident", 15) == 0) {
                    break ;
                }
                // fall through
            default:
            addflag:
                strcat (subflags, argv[i]) ;
                strcat (subflags, " ") ;
                break ;
            }
        }
    }

    switch (invoked_lang) {
    case L_f77:
        strcpy (language, "FORTRAN77") ;
	break;
    case L_f90:
        strcpy (language, "FORTRAN90") ;
	break;
    case L_cc:
        strcpy (language, "C") ;
	break;
    case L_CC:
        strcpy (language, "CC") ;
	break;
    }

    pid = fork() ;
    if (pid == 0) {		// child
        const char *argvec[8] ;

        argvec[0] = exename ;
        argvec[1] = "Compiler" ;
        argvec[2] = language ;
	 #ifdef PSC_TO_OPEN64
        argvec[3] = OPEN64_BUILD_DATE ;
        argvec[4] = subflags ;
        argvec[5] = OPEN64_FULL_VERSION ;
	 #endif
        argvec[6] = NULL ;
        if (subverbose) {
            argvec[6] = "--v" ;
            argvec[7] = NULL ;
        }
        execv (exename, (char*const*)argvec) ;
        fprintf (stderr, errortext) ;
        do_exit (6) ;
    } else {
        int statloc ;
        waitpid (pid, &statloc, 0) ;

        // if we were not able to get a license due to missing subclient executable, tell caller
        if (WIFEXITED(statloc)) {
            if (WEXITSTATUS (statloc) == 6) {		// no subclient program?
                do_exit (1) ;
            } else if (WEXITSTATUS (statloc) == 7) {	// hard stop?
                fprintf (stderr, "Compilation terminated\n") ;
                do_exit (1) ;
            } else if (WEXITSTATUS (statloc) != 0) {            // license client failed, can't rely on output
                fprintf (stderr, "Subscription client exited with error status\n") ;
                do_exit (1) ;
            }
        }

    }
}
