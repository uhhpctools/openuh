/*
 *  Copyright (C) 2006. QLogic Corporation. All Rights Reserved.
 */

/*
 * Copyright 2002, 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
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


#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "cmplrs/rcodes.h"
#include "opt_actions.h"
#include "options.h"
#include "option_names.h"
#include "option_seen.h"
#include "lang_defs.h"
#include "errors.h"
#include "file_utils.h"
#include "file_names.h"
#include "string_utils.h"
#include "get_options.h"
#include "objects.h"
#include "pathscale_defs.h"
#include "phases.h"
#include "run.h"
#include "profile_type.h" /* for enum PROFILE_TYPE */

/* keep list of previous toggled option names, to give better messages */
typedef struct toggle_name_struct {
	int *address;
	char *name;
} toggle_name;
#define MAX_TOGGLES	50
static toggle_name toggled_names[MAX_TOGGLES];
static int last_toggle_index = 0;
static int inline_on_seen = FALSE;
int inline_t = UNDEFINED;
#ifdef KEY
/* Before front-end: UNDEFINED.  After front-end: TRUE if inliner will be run.
   Bug 11325. */
int run_inline;
#endif
boolean dashdash_flag = FALSE;
boolean read_stdin = FALSE;
boolean xpg_flag = FALSE;
int default_olevel = 2;
static int default_isa = UNDEFINED;
static int default_proc = UNDEFINED;
int instrumentation_invoked = UNDEFINED;
int profile_type = 0;
boolean ftz_crt = FALSE;
//int isa = UNDEFINED;
int proc = UNDEFINED;
#ifdef TARG_X8664
static int target_supported_abi = UNDEFINED;
static boolean target_supports_sse2 = FALSE;
static boolean target_prefers_sse3 = FALSE;;
#endif

extern boolean parsing_default_options;
extern boolean drop_option;

static void set_cpu(char *name, m_flag flag_type);

#ifdef KEY
void set_memory_model(char *model);
static int get_platform_abi();
#endif

#ifdef TARG_X8664
static void Get_x86_ISA();
static boolean Get_x86_ISA_extensions();
#endif

/* ====================================================================
 *
 * -Ofast targets
 *
 * Given an -Ofast option, tables which map the IP numbers to
 * processors for use in Ofast_Target below.
 *
 * See common/com/MIPS/config_platform.h.
 *
 * PV 378171:  Change this and config.c to use an external table.
 *
 * ====================================================================
 */

int ofast = UNDEFINED;	/* -Ofast toggle -- implicit in Process_Ofast */


static void
add_toggle_name (int *obj, char *name)
{
	int i;
	for (i = 0; i < last_toggle_index; i++) {
		if (obj == toggled_names[i].address) {
			break;
		}
	}
	if (i == last_toggle_index) {
		if (last_toggle_index >= MAX_TOGGLES) {
			internal_error("too many toggle names\n");
		} else {
			last_toggle_index++;
		}
	}
	toggled_names[i].address = obj;
	toggled_names[i].name = string_copy(option_name);
}

static char *
get_toggle_name (int *obj)
{
	int i;
	for (i = 0; i < last_toggle_index; i++) {
		if (obj == toggled_names[i].address) {
			return toggled_names[i].name;
		}
	}
	internal_error("no previously toggled name?");
	return "<unknown>";
}

/* return whether has been toggled yet */
boolean
is_toggled (int obj)
{
	return (obj != UNDEFINED);
}

/* set obj to value; allow many toggles; last toggle is final value */
void
toggle (int *obj, int value)
{
	// Silently drop a default option if it is already toggled on the
	// command line.
	if (parsing_default_options &&
	    is_toggled(*obj)) {
	  drop_option = TRUE;
	  return;
	}

	if (*obj != UNDEFINED && *obj != value) {
		warning ("%s conflicts with %s; using latter value (%s)", 
			get_toggle_name(obj), option_name, option_name);
	}
	*obj = value;
	add_toggle_name(obj, option_name);
}

/* ====================================================================
 *
 * Get_Group_Option_Value
 *
 * Given a group option string, search for the option with the given
 * name.  Return NULL if not found, the option value if found ("" if
 * value is empty).
 *
 * ====================================================================
 */

static char *
Get_Group_Option_Value (
  char *arg,	/* Raw option string */
  char *name,	/* Suboption full name */
  char *abbrev)	/* Suboption abbreviation */
{
  char *endc = arg;
  int n;

  while ( TRUE ) {
    n = strcspn ( arg, ":=" );
    if ( strncasecmp ( arg, abbrev, strlen(abbrev) ) == 0
      && strncasecmp ( arg, name, n ) == 0 )
    {
      endc += n;
      if ( *endc == '=' ) {
	/* Duplicate value lazily: */
	char *result = strdup ( endc+1 );

	* ( result + strcspn ( result, ":=" ) ) = 0;
	return result;
      } else {
	/* No value: */
	return "";
      }
    }
    if ( ( endc = strchr ( arg, ':' ) ) == NULL ) return NULL;
    arg = ++endc;
  }

  /* Shouldn't get here, but ... */
  /* return NULL;  compiler gets better */
}

/* ====================================================================
 *
 * Bool_Group_Value
 *
 * Given a group option value string for a Boolean group value,
 * determine whether it is TRUE or FALSE.
 *
 * ====================================================================
 */

static boolean
Bool_Group_Value ( char *val )
{
  if ( *val == 0 ) {
    /* Empty string is TRUE for group options */
    return TRUE;
  }

  if ( strcasecmp ( val, "OFF" ) == 0
    || strcasecmp ( val, "NO" ) == 0
    || strcasecmp ( val, "FALSE" ) == 0
    || strcasecmp ( val, "0" ) == 0 )
  {
    return FALSE;
  } else {
    return TRUE;
  }
}
#ifdef KEY /* Bug 4210 */

/* ====================================================================
 *
 * Routine to process "-module dirname" and pass "-Jdirname" to Fortran
 * front end
 *
 * ====================================================================
 */
char *f90_module_dir = 0;

void
Process_module ( char *dirname )
{
  if (0 != f90_module_dir)
  {
    error("Only one -module option allowed");
  }
  strcat(
    strcpy(
      f90_module_dir = malloc(sizeof "-J" + strlen(dirname)),
      "-J"),
    dirname);
}
#endif /* KEY Bug 4210 */

/* ====================================================================
 *
 * Routine to manage the implications of -Ofast.
 *
 * Turn on -O3 and -IPA.  Check_Target below will deal with the ABI and
 * ISA implications later.
 *
 * ====================================================================
 */

void
Process_Ofast ( char *ipname )
{
  int flag;
  char *suboption;

  /* -O3: */
  if (!Gen_feedback) {
     O3_flag = TRUE;
     toggle ( &olevel, 3 );
     add_option_seen ( O_O3 );

#ifdef TARG_IA64
     ftz_crt = TRUE;	// flush to zero
#endif

     /* -fno-math-errno */
     toggle ( &fmath_errno, 0);
     add_option_seen (O_fno_math_errno);

     /* -ffast-math */
     toggle ( &ffast_math, 1);
     add_option_seen (O_ffast_math);

     /* -IPA: */
     toggle ( &ipa, TRUE );
     add_option_seen ( O_IPA );

     /* -OPT:Ofast=ipname
      * We will call add_string_option using O_OPT_; if the descriptor
      * for it in OPTIONS changes, this code might require change...
      * Build the "Ofast=ipname" string, then call add_string_option:
      */
     toggle ( &ofast, TRUE );
     suboption = concat_strings ( "Ofast=", ipname );
     flag = add_string_option ( O_OPT_, suboption );
     add_option_seen ( flag );
   } else {
     suboption = concat_strings ( "platform=", ipname );
     flag = add_string_option ( O_TARG_, suboption );
     add_option_seen ( flag );
   }
}

/* ====================================================================
 *
 * Process_Opt_Group
 *
 * We've found a -OPT option group.  Inspect it for -OPT:reorg_common
 * options, and set -split_common and -ivpad accordingly.
 *
 * NOTE: We ignore anything that doesn't match what's expected --
 * the compiler will produce reasonable error messages for junk.
 *
 * ====================================================================
 */

void
Process_Opt_Group ( char *opt_args )
{
  char *optval = NULL;

  if ( debug ) {
    fprintf ( stderr, "Process_Opt_Group: %s\n", opt_args );
  }
  
  /* Go look for -OPT:instrument */
  optval = Get_Group_Option_Value ( opt_args, "instrument", "instr");
  if (optval != NULL) {
     instrumentation_invoked = TRUE;
  }

  /* Go look for -OPT:reorg_common: */
  optval = Get_Group_Option_Value ( opt_args, "reorg_common", "reorg");
  if ( optval != NULL && Bool_Group_Value(optval)) {
#ifndef KEY
    /* If we found it, set -Wl,-split_common,-ivpad: */
    add_option_seen ( O_split_common );
    add_option_seen ( O_ivpad );
#endif
  }
}

void
Process_Default_Group (char *default_args)
{
  char *s;
  int i;

  if ( debug ) {
    fprintf ( stderr, "Process_Default_Group: %s\n", default_args );
  }

  /* Go look for -DEFAULT:isa=mipsN: */
  s = Get_Group_Option_Value ( default_args, "isa", "isa");
  if (s != NULL && same_string_prefix (s, "mips")) {
	default_isa = atoi(s + strlen("mips"));
  }
  /* Go look for -DEFAULT:opt=[0-3]: */
  s = Get_Group_Option_Value ( default_args, "opt", "opt");
  if (s != NULL) {
	default_olevel = atoi(s);
  }
  /* Go look for -DEFAULT:arith=[0-3]: */
  s = Get_Group_Option_Value ( default_args, "arith", "arith");
  if (s != NULL) {
	i = add_string_option (O_OPT_, concat_strings("IEEE_arith=", s));
	add_option_seen (i);
  }
}

/* ====================================================================
 *
 * Routines to manage the target selection (ABI, ISA, and processor).
 *
 * Make sure that the driver picks up a consistent view of the target
 * selected, based either on user options or on defaults.
 *
 * ====================================================================
 */

/* ====================================================================
 *
 * Process_Targ_Group
 *
 * We've found a -TARG option group.  Inspect it for ABI, ISA, and/or
 * processor specification, and toggle the state appropriately.
 *
 * NOTE: We ignore anything that doesn't match what's expected --
 * the compiler will produce reasonable error messages for junk.
 *
 * ====================================================================
 */

void
Process_Targ_Group ( char *targ_args )
{
  char *cp = targ_args;	/* Skip -TARG: */
  char *cpeq;
  char *ftz;

  if ( debug ) {
    fprintf ( stderr, "Process_Targ_Group: %s\n", targ_args );
  }

  ftz = Get_Group_Option_Value ( targ_args, "flush_to_zero", "flush_to_zero");
  if ( ftz != NULL && Bool_Group_Value(ftz)) {
    /* link in ftz.o */
    ftz_crt = TRUE;
  }

  while ( *cp != 0 ) {
    switch ( *cp ) {
      case '3':
#ifdef TARG_X8664
	if (!strncasecmp(cp, "3dnow=on", 9)) {
	  add_option_seen(O_m3dnow);
	  toggle(&m3dnow, TRUE);
	} else if (!strncasecmp(cp, "3dnow=off", 10)) {
	  add_option_seen(O_mno_3dnow);
	  toggle(&m3dnow, FALSE);
	}
	break;
#endif

      case 'a':
	if ( strncasecmp ( cp, "abi", 3 ) == 0 && *(cp+3) == '=' ) {
#ifdef TARG_MIPS
	  if ( strncasecmp ( cp+4, "n32", 3 ) == 0 ) {
	    add_option_seen ( O_n32 );
	    toggle ( &abi, ABI_N32 );
	  } else if ( strncasecmp ( cp+4, "64", 2 ) == 0 ) {
	    add_option_seen ( O_m64 );
	    toggle ( &abi, ABI_64 );
	  }
#endif
#ifdef TARG_X8664
	  // The driver needs to handle all the -TARG options that it gives to
	  // the back-end, even if these -TARG options are not visible to the
	  // user.  This is because IPA invokes the driver with back-end
	  // options.  Bug 5466.
	  if ( strncasecmp ( cp+4, "n32", 3 ) == 0 ) {
	    add_option_seen ( O_m32 );
	    toggle ( &abi, ABI_N32 );
	  } else if ( strncasecmp ( cp+4, "n64", 3 ) == 0 ) {
	    add_option_seen ( O_m64 );
	    toggle ( &abi, ABI_64 );
	  }
#endif
	}
	break;

#if 0	  /* temporary hack by gbl -- O_WlC no longer exists due to a change in OPTIONS */
      case 'e':
	if ( strncasecmp ( cp, "exc_enable", 10 ) == 0 && *(cp+10) == '=' ) {
  	  int flag;
  	  buffer_t buf;
	  int mask = 0;
	  cp += 11;
    	  while ( *cp != 0 && *cp != ':' ) {
	    switch (*cp) {
	    case 'I': mask |= (1 << 5); break;
	    case 'U': mask |= (1 << 4); break;
	    case 'O': mask |= (1 << 3); break;
	    case 'Z': mask |= (1 << 2); break;
	    case 'D': mask |= (1 << 1); break;
	    case 'V': mask |= (1 << 0); break;
	    }
	    ++cp;
	  }
	  flag = add_string_option(O_WlC, "-defsym,_IEEE_ENABLE_DEFINED=1");
	  add_option_seen (flag);
	  sprintf(buf, "-defsym,_IEEE_ENABLE=%#x", mask);
	  flag = add_string_option(O_WlC, buf);
	  add_option_seen (flag);
	}
	break;
#endif

      case 'i':
	/* We support both isa=mipsn and plain mipsn in group.
	 * Simply move cp to point to value, and fall through to
	 * 'm' case:
	 */
	if ( strncasecmp ( cp, "isa", 3 ) != 0 || *(cp+3) != '=' ) {
	  break;
	} else {
	  cp += 4;
	}
	/* Fall through */

      case 'm':
#ifdef TARG_MIPS
	if ( strncasecmp ( cp, "mips", 4 ) == 0 ) {
	  if ( '1' <= *(cp+4) && *(cp+4) <= '6' ) {
	    toggle ( &isa, *(cp+4) - '0' );
	    switch ( isa ) {
	      case 1:	add_option_seen ( O_mips1 );
			break;
	      case 2:	add_option_seen ( O_mips2 );
			break;
	      case 3:	add_option_seen ( O_mips3 );
			break;
	      case 4:	add_option_seen ( O_mips4 );
			break;
	      default:	error ( "invalid ISA: %s", cp );
			break;
	    }
	  }
	}
#endif
	break;

      case 'p':
#ifdef KEY
	if (!strncasecmp(cp, "processor=", 10)) {
	  char *target = cp + 10;
	  set_cpu (target, M_ARCH);
	}
#endif
	break;

      case 's':
#ifdef TARG_X8664
	if (!strncasecmp(cp, "sse2=on", 8)) {
	  add_option_seen(O_msse2);
	  toggle(&sse2, TRUE);
	} else if (!strncasecmp(cp, "sse2=off", 9)) {
	  add_option_seen(O_mno_sse2);
	  toggle(&sse2, FALSE);
	} else if (!strncasecmp(cp, "sse3=on", 8)) {
	  add_option_seen(O_msse3);
	  toggle(&sse3, TRUE);
	} else if (!strncasecmp(cp, "sse3=off", 9)) {
	  add_option_seen(O_mno_sse3);
	  toggle(&sse3, FALSE);
	}
#endif
	break;
    }

    /* Skip to the next group option: */
    while ( *cp != 0 && *cp != ':' ) ++cp;
    if ( *cp == ':' ) ++cp;
  }
}


/* ====================================================================
 *
 * Check_Target
 *
 * Verify that the target selection is consistent and set defaults.
 *
 * ====================================================================
 */

void
Check_Target ( void )
{
  int opt_id;
  int opt_val;

  if ( debug ) {
    fprintf ( stderr, "Check_Target ABI=%d ISA=%d Processor=%d\n",
	      abi, isa, proc );
  }

#ifdef TARG_X8664
  if (target_cpu == NULL) {
    set_cpu ("opteron", M_ARCH);	// Default to Opteron.
  }

  // Uses ABI to determine ISA.  If ABI isn't set, it guesses and sets the ABI.
  Get_x86_ISA();
#endif

  if (abi == UNDEFINED) {
#ifdef TARG_IA64
	toggle(&abi, ABI_I64);
    	add_option_seen ( O_i64 );
#elif TARG_IA32
	toggle(&abi, ABI_IA32);
    	add_option_seen ( O_ia32 );
#elif TARG_MIPS
	toggle(&abi, ABI_N32);
    	add_option_seen ( O_n32 );
#elif TARG_X8664
	// User didn't specify ABI.  Use the ABI supported on host.  Bug 8488.
	if (target_supported_abi == ABI_N32) {
	  abi = ABI_N32;
	} else if (target_supported_abi == ABI_64) {
	  abi = (get_platform_abi() == ABI_N32) ?
		  ABI_N32 : target_supported_abi;
	} else if (target_supported_abi == UNDEFINED) {
	  abi = (get_platform_abi() == ABI_64) ? ABI_64 : ABI_N32;
	} else {
	  internal_error ("illegal target_supported_abi");
	}

	if (abi == ABI_64)
	  add_option_seen (O_m64);
	else
	  add_option_seen (O_m32);
#else
	warning("abi should have been specified by driverwrap");
  	/* If nothing is defined, default to -n32 */
    	toggle ( &abi, ABI_N32 );
    	add_option_seen ( O_n32 );
#endif
  }

#ifdef TARG_X8664
  // ABI must be set.
  if (!Get_x86_ISA_extensions())
    return;	// If error, quit instead of giving confusing error messages.
#endif

  /* Check ABI against ISA: */
  if ( isa != UNDEFINED ) {
    switch ( abi ) {
#ifdef TARG_MIPS
      case ABI_N32:
	if ( isa < ISA_MIPS3 ) {
	  add_option_seen ( O_mips3 );
	  warning ( "ABI specification %s conflicts with ISA "
		    "specification %s: defaulting ISA to mips3",
		    get_toggle_name (&abi),
		    get_toggle_name (&isa) );
	  option_name = get_option_name ( O_mips3 );
	  isa = UNDEFINED;	/* To avoid another message */
	  toggle ( &isa, ISA_MIPS3 );
	}
	break;

      case ABI_64:
	if ( isa < ISA_MIPS3 ) {
	  /* Default to -mips4 if processor supports it: */
	  if ( proc == UNDEFINED || proc >= PROC_R5K ) {
	    opt_id = O_mips4;
	    opt_val = ISA_MIPS4;
	    add_option_seen ( O_mips4 );
	  } else {
	    opt_id = O_mips3;
	    opt_val = ISA_MIPS3;
	    add_option_seen ( O_mips3 );
	  }
	  warning ( "ABI specification %s conflicts with ISA "
		    "specification %s: defaulting ISA to mips%d",
		    get_toggle_name (&abi),
		    get_toggle_name (&isa),
		    opt_val );
	  option_name = get_option_name ( opt_id );
	  isa = UNDEFINED;	/* To avoid another message */
	  toggle ( &isa, opt_val );
	}
	break;
#endif
    }

  } else {
    /* ISA is undefined, so derive it from ABI and possibly processor: */

    switch ( abi ) {
#ifdef TARG_MIPS
      case ABI_N32:
      case ABI_64:
        if (default_isa == ISA_MIPS3) {
	  opt_val = ISA_MIPS3;
	  opt_id = O_mips3;
	}
	else if (default_isa == ISA_MIPS4) {
	  opt_val = ISA_MIPS4;
	  opt_id = O_mips4;
	}
	else {
	  opt_val = ISA_MIPS64;
	  opt_id = O_mips64;
	}
	toggle ( &isa, opt_val );
	add_option_seen ( opt_id );
	option_name = get_option_name ( opt_id );
	break;
#elif TARG_X8664
      case ABI_N32:
      case ABI_64:
	  opt_val = ISA_X8664;
	  toggle ( &isa, opt_val );
	break;
#endif
      case ABI_I32:
      case ABI_I64:
	opt_val = ISA_IA641;
	toggle ( &isa, opt_val );
	break;
      case ABI_IA32:
	opt_val = ISA_IA32;
	toggle ( &isa, opt_val );
	break;
    }
  }
  if (isa == UNDEFINED) {
	internal_error ("isa should have been defined by now");
  }

  /* Check ABI against processor: */
  if ( proc != UNDEFINED ) {
    switch ( abi ) {
#ifdef TARG_MIPS
      case ABI_N32:
      case ABI_64:
	if ( proc < PROC_R4K ) {
	  warning ( "ABI specification %s conflicts with processor "
		    "specification %s: defaulting processor to r10000",
		    get_toggle_name (&abi),
		    get_toggle_name (&proc) );
	  option_name = get_option_name ( O_r10000 );
	  proc = UNDEFINED;	/* To avoid another message */
	  add_option_seen ( O_r10000 );
	  toggle ( &proc, PROC_R10K );
	}
	break;
#endif
    }
  }

  /* Check ISA against processor: */
  if ( proc != UNDEFINED ) {
    switch ( isa ) {
#ifdef TARG_MIPS
      case ISA_MIPS1:
	/* Anything works: */
	break;

      case ISA_MIPS2:
      case ISA_MIPS3:
	if ( proc < PROC_R4K ) {
	  warning ( "ISA specification %s conflicts with processor "
		    "specification %s: defaulting processor to r10000",
		    get_toggle_name (&isa),
		    get_toggle_name (&proc) );
	  add_option_seen ( O_r10000 );
	  proc = UNDEFINED;	/* To avoid another message */
	  option_name = get_option_name ( O_r10000 );
	  toggle ( &proc, PROC_R10K );
	}
	break;

      case ISA_MIPS4:
	if ( proc < PROC_R5K ) {
	  warning ( "ISA specification %s conflicts with processor "
		    "specification %s: defaulting processor to r10000",
		    get_toggle_name (&isa),
		    get_toggle_name (&proc) );
	  add_option_seen ( O_r10000 );
	  proc = UNDEFINED;	/* To avoid another message */
	  option_name = get_option_name ( O_r10000 );
	  toggle ( &proc, PROC_R10K );
	}
	break;
#endif
    }
  }
  else if (default_proc != UNDEFINED) {
	/* set proc if compatible */
	opt_id = 0;
#ifdef TARG_MIPS
	switch (default_proc) {
	case PROC_R4K:
		if (isa <= ISA_MIPS3) {
			opt_id = O_r4000;
		}
		break;
	case PROC_R5K:
		opt_id = O_r5000;
		break;
	case PROC_R8K:
		opt_id = O_r8000;
		break;
	case PROC_R10K:
		opt_id = O_r10000;
		break;
	}
#endif
	if (abi == ABI_I64 || abi == ABI_IA32) {
		opt_id = 0;	/* no proc for i64, ia32 yet */
	}
	if (opt_id != 0) {
		add_option_seen ( opt_id );
		option_name = get_option_name ( opt_id );
		toggle ( &proc, default_proc);
	}
  }

  if ( debug ) {
    fprintf ( stderr, "Check_Target done; ABI=%d ISA=%d Processor=%d\n",
	      abi, isa, proc );
  }
}

/* ====================================================================
 *
 * Routines to manage inlining choices (the -INLINE group and friends).
 *
 * ====================================================================
 */

/* toggle inline for a normal option (not "=on" or "=off") */

static void
toggle_inline_normal(void)
{
  if (inline_t == UNDEFINED)
    inline_t = TRUE;
}

/* toggle inline for "=on" */

static void
toggle_inline_on(void)
{
  if (inline_t == FALSE) {
    warning ("-noinline or -INLINE:=off has been seen, %s ignored",
	     option_name);
  }
  else {

    inline_t = TRUE;
    inline_on_seen = TRUE;
  }
}

/* toggle inline for "=off" */

static void
toggle_inline_off(void)
{
  if (inline_on_seen == TRUE) {
    warning ("Earlier request for inline processing has been overridden by %s",
	     option_name);
  }
  inline_t = FALSE;
}

void
Process_Profile_Arcs( void )
{
  if (strncmp (option_name, "-fprofile-arcs", 14) == 0)
    add_string_option (O_OPT_, "profile_arcs=true");
}

void
Process_Test_Coverage( void )
{
  if (strncmp (option_name, "-ftest-coverage", 15) == 0)
    add_string_option (O_CG_, "test_coverage=true");
}

/* process -INLINE option */
void
Process_Inline ( void )
{
  int more_symbols = TRUE;
  char *args = option_name+7;

  if (strncmp (option_name, "-noinline", 9) == 0)
      toggle_inline_off();
  else if (*args == '\0')
    /* Treat "-INLINE" like "-INLINE:=on" for error messages */
    toggle_inline_on();
  else do {
    char *endc;
    *args = ':';
    if ((endc = strchr(++args, ':')) == NULL)
      more_symbols = FALSE;
    else
      *endc = '\0';
    if (strcasecmp(args, "=off") == 0)
      toggle_inline_off();
    else if (strcasecmp(args, "=on") == 0)
      toggle_inline_on();
    else
      toggle_inline_normal();
    args = endc;
  }
  while (more_symbols);
}

/*
 * Processing -F option: ratfor-related stuff for Fortran, but
 * (obsolete) C code generation option in C++ and unknown for C.
 */
void dash_F_option(void)
{
    if (invoked_lang == L_f77) {
	last_phase=earliest_phase(P_ratfor,last_phase);
    } else if (invoked_lang == L_CC) {
	error("-F is not supported: cannot generate intermediate C code");
    } else {
	parse_error("-F", "unknown flag");
    }
}

/* untoggle the object, so it can be re-toggled later */
void
untoggle (int *obj, int value)
/*ARGSUSED*/
{
  *obj = UNDEFINED;
}

/* change path for particular phase(s), e.g. -Yb,/usr */
static void
change_phase_path (char *arg)
{
	char *dir;
	char *s;
	for (s = arg; s != NULL && *s != NIL && *s != ','; s++)
		;
	if (s == NULL || *s == NIL) {
		parse_error(option_name, "bad syntax for -Y option");
		return;
	}
	dir = s+1;
	if (dir[0] == '~' && (dir[1] == '/' || dir[1] == '\0')) {
	    char *home = getenv("HOME");
	    if (home)
		dir = concat_strings(home, dir+1);
	}
	if (!is_directory(dir))
		parse_error(option_name, "not a directory");
	for (s = arg; *s != ','; s++) {
		/* do separate check so can give better error message */
		if (get_phase(*s) == P_NONE) {
			parse_error(option_name, "bad phase for -Y option");
		} else {
			set_phase_dir(get_phase_mask(get_phase(*s)), dir);
#ifdef KEY
			// Special case wgen because it is affected by -Yf but
			// is not considered a front-end (because it does not
			// take C/C++ front-end flags in OPTIONS).
			if (get_phase(*s) == P_any_fe)
			  set_phase_dir(get_phase_mask(P_wgen), dir);
#endif
		}
	}
}

/* halt after a particular phase, e.g. -Hb */
/* but also process -H and warn its ignored */
static void
change_last_phase (char *s)
{
	phases_t phase;
	if (s == NULL || *s == NIL) {
		warn_ignored("-H");
	} else if ( *(s+1)!=NIL) {
		parse_error(option_name, "bad syntax for -H option");
	} else if ((phase=get_phase(*s)) == P_NONE) {
			parse_error(option_name, "bad phase for -H option");
	} else {
			last_phase=earliest_phase(phase, last_phase);
	}
}

void
save_name (char **obj, char *value)
{
	*obj = string_copy(value);
}

static void
check_output_name (char *name)
{
	if (name == NULL) return;
	if (get_source_kind(name) != S_o && file_exists(name)) {
		warning("%s %s will overwrite a file that has a source-file suffix", option_name, name);
	}
}

#ifdef KEY /* bug 4260 */
/* Disallow illegal name following "-convert" */
void
check_convert_name(char *name)
{
	static char *legal_names[] = {
	  "big_endian",
	  "big-endian",
	  "little_endian",
	  "little-endian",
	  "native"
	  };
	for (int i = 0; i < ((sizeof legal_names) / (sizeof *legal_names));
	  i += 1) {
	  if (0 == strcmp(name, legal_names[i])) {
	    return;
	  }
	}
	parse_error(option_name, "bad conversion name");
}
#endif /* KEY bug 4260 */

void
check_dashdash (void)
{
#ifndef KEY	// Silently ignore dashdash options in case pathcc is called as
		// a linker.  Bug 4736.
	if(xpg_flag)
	   dashdash_flag = 1;
	else
	   error("%s not allowed in non XPG4 environment", option_name);
#endif
}

static char *
Get_Binary_Name ( char *name)
{
  char *new;
  int len, i;
  new = string_copy(name);
  len = strlen(new);
  for ( i=0; i<len; i++ ) {
    if (strncmp(&new[i], ".x.Counts", 9) == 0) {
      new[i] = 0;
      break;
    }
  }
  return new;
}
 
void
Process_fbuse ( char *fname )
{
  static boolean is_first_count_file = TRUE;
  Use_feedback = TRUE;
  add_string (count_files, fname);
  if (is_first_count_file && (prof_file == NULL))
    prof_file = Get_Binary_Name(drop_path(fname));
  is_first_count_file = FALSE;
}

void
Process_fb_type ( char*  typename )
{
  char str[10];
  int flag, tmp;
  fb_type = string_copy(typename);
  sprintf(str,"fb_type=%s",fb_type);
  flag = add_string_option (O_OPT_, str);
  add_option_seen(flag);

  sscanf (typename, "%d", &tmp);
  profile_type |= tmp; 
}


void
Process_fb_create ( char *fname )
{
   int flag;
   fb_file = string_copy(fname);

   if (instrumentation_invoked == TRUE) {
     /* instrumentation already specified */
     flag = add_string_option (O_OPT_, "instr_unique_output=on");
   }
   else {
     toggle ( &instrumentation_invoked, TRUE );
     flag = add_string_option (O_OPT_, "instr=on:instr_unique_output=on");
   }
   add_option_seen (flag);
}


void 
Process_fb_phase(char *phase)
{
  char str[10];
  int flag;
  fb_phase = string_copy(phase);
  sprintf(str,"fb_phase=%s",fb_phase);
  flag = add_string_option (O_OPT_, str);
  add_option_seen(flag);
}


void
Process_fb_opt ( char *fname )
{
  opt_file = string_copy(fname);
  toggle ( &instrumentation_invoked, FALSE);
}


void
Process_fbexe ( char *fname )
{
  prof_file = string_copy(fname);
}

void
Process_fb_xdir ( char *fname )
{
  fb_xdir = string_copy(fname);
}

void
Process_fb_cdir ( char *fname )
{
  fb_cdir =  string_copy(fname);
}

#ifndef KEY	// -dsm no longer supported.  Bug 4406.
typedef enum {
  DSM_UNDEFINED,
  DSM_OFF,
  DSM_ON
} DSM_OPTION;

static DSM_OPTION dsm_option=DSM_UNDEFINED;
static DSM_OPTION dsm_clone=DSM_UNDEFINED;
static DSM_OPTION dsm_check=DSM_UNDEFINED;

void
set_dsm_default_options (void)
{
  if (dsm_option==DSM_UNDEFINED) dsm_option=DSM_ON;
  if (dsm_clone==DSM_UNDEFINED && invoked_lang != L_CC) dsm_clone=DSM_ON;
  if (dsm_check==DSM_UNDEFINED) dsm_check=DSM_OFF;
}

void
reset_dsm_default_options (void)
{
  dsm_option=DSM_OFF;
  dsm_clone=DSM_OFF;
  dsm_check=DSM_OFF;
}

void
set_dsm_options (void)
{

  if (dsm_option==DSM_ON) {
    add_option_seen(O_dsm);
  } else {
    reset_dsm_default_options();
    if (option_was_seen(O_dsm))
      set_option_unseen(O_dsm); 
  }

  if (dsm_clone==DSM_ON) 
    add_option_seen(O_dsm_clone);
  else
    if (option_was_seen(O_dsm_clone))
      set_option_unseen(O_dsm_clone); 
  if (dsm_check==DSM_ON) 
    add_option_seen(O_dsm_check);
  else
    if (option_was_seen(O_dsm_check))
      set_option_unseen(O_dsm_check); 
}

/* ====================================================================
 *
 * Process_Mp_Group
 *
 * We've found a -MP option group.  Inspect it for dsm request
 * and toggle the state appropriately.
 *
 * NOTE: We ignore anything that doesn't match what's expected --
 * the compiler will produce reasonable error messages for junk.
 *
 * ====================================================================
 */

void
Process_Mp_Group ( char *mp_args )
{
  char *cp = mp_args;	/* Skip -MP: */

  if ( debug ) {
    fprintf ( stderr, "Process_Mp_Group: %s\n", mp_args );
  }

  while ( *cp != 0 ) {
    switch ( *cp ) {
      case 'd':
	if ( strncasecmp ( cp, "dsm", 3 ) == 0 &&
             (*(cp+3)==':' || *(cp+3)=='\0'))
            set_dsm_default_options();
	else if ( strncasecmp ( cp, "dsm=on", 6 ) == 0 )
            set_dsm_default_options();
	else if ( strncasecmp ( cp, "dsm=off", 7 ) == 0 )
            reset_dsm_default_options();
	else if ( strncasecmp ( cp, "dsm=true", 8 ) == 0 )
            set_dsm_default_options();
	else if ( strncasecmp ( cp, "dsm=false", 9 ) == 0 )
            reset_dsm_default_options();
	else
          parse_error(option_name, "Unknown -MP: option");
	break;
      case 'c':
	if ( strncasecmp ( cp, "clone", 5 ) == 0) {
          if ( *(cp+5) == '=' ) {
	    if ( strncasecmp ( cp+6, "on", 2 ) == 0 )
              dsm_clone=DSM_ON;
	    else if ( strncasecmp ( cp+6, "off", 3 ) == 0 )
              dsm_clone=DSM_OFF;
          } else if ( *(cp+5) == ':' || *(cp+5) == '\0' ) {
              dsm_clone=DSM_ON;
          } else
            parse_error(option_name, "Unknown -MP: option");
	} else if ( strncasecmp ( cp, "check_reshape", 13 ) == 0) {
          if ( *(cp+13) == '=' ) {
	    if ( strncasecmp ( cp+14, "on", 2 ) == 0 ) {
              dsm_check=DSM_ON;
	    } else if ( strncasecmp ( cp+14, "off", 3 ) == 0 ) {
              dsm_check=DSM_OFF;
            }
          } else if ( *(cp+13) == ':' || *(cp+13) == '\0' ) {
              dsm_check=DSM_ON;
          } else
            parse_error(option_name, "Unknown -MP: option");
	}
	else
          parse_error(option_name, "Unknown -MP: option");
	break;
    case 'm':
      if (strncasecmp (cp, "manual=off", 10) == 0) {
        set_option_unseen (O_mp);
        reset_dsm_default_options ();
      }
      else
        parse_error(option_name, "Unknown -MP: option");
      break;
    case 'o':
      if (strncasecmp (cp, "open_mp=off", 11) == 0) {
	 Disable_open_mp = TRUE;
      } else if (strncasecmp (cp, "old_mp=off", 10) == 0) {
	 Disable_old_mp = TRUE;
      } else if ((strncasecmp (cp, "open_mp=on", 10) == 0) ||
		 (strncasecmp (cp, "old_mp=on", 9) == 0)) {
           /* No op; do nothing */
      } else {
	 parse_error(option_name, "Unknown -MP: option");
      }
      break;
    default:
          parse_error(option_name, "Unknown -MP: option");
    }

    /* Skip to the next group option: */
    while ( *cp != 0 && *cp != ':' ) ++cp;
    if ( *cp == ':' ) ++cp;
  }

  if ( debug ) {
    fprintf ( stderr, "Process_Dsm_Group done\n" );
  }
}

void
Process_Mp ( void )
{

  if ( debug ) {
    fprintf ( stderr, "Process_Mp\n" );
  }

  if (!option_was_seen (O_mp)) {
    /* avoid duplicates */
    add_option_seen (O_mp);
  }
  set_dsm_default_options();

  if ( debug ) {
    fprintf ( stderr, "Process_Mp done\n" );
  }
}

void Process_Cray_Mp (void) {

  if (invoked_lang == L_f90) {
    /* this part is now empty (we do the processing differently)
     * but left as a placeholder and error-checker.
     */
  }
  else error ("-cray_mp applicable only to f90");
}
#endif

void
Process_Promp ( void )
{

  if ( debug ) {
    fprintf ( stderr, "Process_Promp\n" );
  }

  /* Invoke -PROMP:=on for f77,f90 -mplist for C, and nothing for
   * other languages.
   */
  if (invoked_lang == L_f77 || invoked_lang == L_f90) {
    add_option_seen ( O_promp );
    add_option_seen(add_string_option(O_FE_, "endloop_marker=1"));
  } else if (invoked_lang == L_cc) {
    /* add_option_seen(O_mplist); */
    add_option_seen ( O_promp );
  }
  if ( debug ) {
    fprintf ( stderr, "Process_Promp done\n" );
  }
}

void
Process_Tenv_Group ( char *opt_args )
{
  if ( debug ) {
    fprintf ( stderr, "Process_TENV_Group: %s\n", opt_args );
  }
  
  /* Go look for -TENV:mcmodel=xxx */
  if (strncmp (opt_args, "mcmodel=", 8) == 0) {
    set_memory_model (opt_args + 8);
  }
}

static int
print_magic_path(const char *base, const char *fname)
{
  int m32 = check_for_saved_option("-m32");
  char *slash;
  char *path;

  if (m32) {
    char *sfx;

    asprintf(&path, "%s/32/%s", base, fname);

    if (file_exists(path))
      goto good;
    
    if (ends_with(base, "/lib64")) {
      asprintf(&path, "%.*s/%s", (int)(strlen(base) - 2), base, fname);

      if (file_exists(path))
	goto good;
    }

    sfx = get_suffix(fname);

    if (sfx != NULL &&	// bug 9049
	(!strcmp(sfx, "a") || !strcmp(sfx, "o") || !strcmp(sfx, "so")))
      goto bad;

    if ((slash = strrchr(path, '/')) && strstr(slash, ".so."))
      goto bad;
  }

  asprintf(&path, "%s/%s", base, fname);

  if (file_exists(path))
    goto good;
  
 bad:
  return 0;

 good:
  puts(path);
  return 1;
}

static int
print_phase_path(phases_t phase, const char *fname)
{
  return print_magic_path(get_phase_dir(phase), fname);
}

static int print_relative_path(const char *s, const char *fname)
{
  char *root_prefix = directory_path(get_executable_dir());
  char *base;

  asprintf(&base, "%s/%s", root_prefix, s);
  return print_magic_path(base, fname);
}

/* Keep this in sync with set_library_paths over in phases.c. */

void
print_file_path (char *fname, int exe)
{
  /* Search for fname in usual places, and print path when found. */
  /* gcc does separate searches for libraries and programs,
   * but that seems redundant as the paths are nearly identical,
   * so try combining into one search.
   */

  #ifdef PSC_TO_OPEN64
  if (print_relative_path("lib/" OPEN64_FULL_VERSION, fname))
  #endif
    return;

  if (print_phase_path(P_be, fname))
    return;

  if (print_phase_path(P_library, fname))
    return;

  if (print_phase_path(P_gcpp, fname))
    return;

  if (print_phase_path(P_gas, fname))
    return;

  if (print_phase_path(P_alt_library, fname))
    return;

  /* not found, so ask gcc */
  int m32 = check_for_saved_option("-m32");
  char *argv[4];
  argv[0] = "gcc";
  argv[1] = m32 ? "-m32" : "-m64";
  asprintf(&argv[2], "-print-%s-name=%s", exe ? "prog" : "file", fname);
  argv[3] = NULL;
  execvp(argv[0], argv);
  fprintf(stderr, "could not execute %s: %m\n", argv[0]);
  exit(1);
}

void
print_multi_lib ()
{
  char *argv[3];
  argv[0] = "gcc";
  asprintf(&argv[1], "-print-multi-lib");
  argv[2] = NULL;
  execvp(argv[0], argv);
  fprintf(stderr, "could not execute %s: %m\n", argv[0]);
  exit(1);
}

mem_model_t mem_model = M_SMALL;
char *mem_model_name = NULL;

void
set_memory_model(char *model)
{
  if (strcmp(model, "small") == 0) {
    mem_model = M_SMALL;
    mem_model_name = "small";
  }
  else if (strcmp(model, "medium") == 0) {
    mem_model = M_MEDIUM;
    mem_model_name = "medium";
  }
  else if (strcmp(model, "large") == 0) {
    mem_model = M_LARGE;
    mem_model_name = "large";
  }
  else if (strcmp(model, "kernel") == 0) {
    mem_model = M_KERNEL;
    mem_model_name = "kernel";
  } else {
    error("unknown memory model \"%s\"", model);
    mem_model_name = NULL;
  }
}

static struct 
{
  char *cpu_name;
  char *target_name;
  int abi;			// CPUs supporting ABI_64 also support ABI_N32
  boolean supports_sse2;	// TRUE if support SSE2
  boolean prefers_sse3;		// TRUE if target prefers code to use SSE3
} supported_cpu_types[] = {
  { "any_64bit_x86",	"anyx86",	ABI_64,		TRUE,	FALSE },
  { "any_32bit_x86",	"anyx86",	ABI_N32,	FALSE,	FALSE },
  { "i386",	"anyx86",		ABI_N32,	FALSE,	FALSE },
  { "i486",	"anyx86",		ABI_N32,	FALSE,	FALSE },
  { "i586",	"anyx86",		ABI_N32,	FALSE,	FALSE },
  { "athlon",	"athlon",		ABI_N32,	FALSE,	FALSE },
  { "athlon-mp", "athlon",		ABI_N32,	FALSE,	FALSE },
  { "athlon-xp", "athlon",		ABI_N32,	FALSE,	FALSE },
  { "athlon64",	"athlon64",		ABI_64,		TRUE,	FALSE },
  { "athlon64fx", "opteron",		ABI_64,		TRUE,	FALSE },
  { "i686",	"pentium4",		ABI_N32,	FALSE,	FALSE },
  { "ia32",	"pentium4",		ABI_N32,	TRUE,	FALSE },
  { "k7",	"athlon",		ABI_N32,	FALSE,	FALSE },
  { "k8",	"opteron",		ABI_64,		TRUE,	FALSE },
  { "opteron",	"opteron",		ABI_64,		TRUE,	FALSE },
  { "pentium4",	"pentium4",		ABI_N32,	TRUE,	FALSE },
  { "xeon",	"xeon",			ABI_N32,	TRUE,	FALSE },
  { "em64t",	"em64t",		ABI_64,		TRUE,	TRUE },
  { "core",	"core",			ABI_64,		TRUE,	TRUE },
  { NULL,	NULL, },
};
  
char *target_cpu = NULL;

// Get the platform's default ABI.
static int
get_platform_abi()
{
  struct utsname u;

  uname(&u);
  if (!strcmp(u.machine, "x86_64"))
    return ABI_64;
  return ABI_N32;
}

// Return the numeric value after ':' in a line in /proc/cpuinfo.
static int
get_num_after_colon (char *str)
{
  char *p;
  int num;

  p = strchr(str, ':');
  if (p == NULL) {
    error ("cannot parse /proc/cpuinfo: missing colon");
  }
  p++;
  if (sscanf(p, "%d", &num) == 0) {
    error ("cannot parse /proc/cpuinfo: missing number after colon");
  }
  return num;
}

// Get CPU name from /proc/cpuinfo.
char *
get_auto_cpu_name ()
{
  FILE *f;
  char buf[256];
  char *cpu_name = NULL;
  char *cpu_name_64bit = NULL;		// cpu_name of 64-bit version of CPU
  int cpu_family = -1;			// CPU family number
  int model = -1;			// CPU model number
  boolean amd = FALSE;			// AMD CPU
  boolean intel = FALSE;		// Intel CPU

  f = fopen("/proc/cpuinfo", "r");
  if (f == NULL) {
    error("cannot read /proc/cpuinfo");
    return NULL;
  }
  while (fgets(buf, 256, f) != NULL) {
    if (!strncmp("cpu family", buf, 10)) {
      cpu_family = get_num_after_colon(buf);
    } else if (!strncmp("model\t", buf, 6)) {
      model = get_num_after_colon(buf);
    } else if (!strncmp("model name", buf, 10)) {
      if (strstr(buf, "AMD Athlon(tm) 64 ") != NULL)
	cpu_name = "athlon64";
      else if (strstr(buf, "AMD Athlon(tm) MP ") != NULL)
	cpu_name = "athlon-mp";
      else if (strstr(buf, "AMD Athlon(tm) Processor") != NULL)
	cpu_name = "athlon";
      else if (strstr(buf, "AMD Opteron(tm) ") != NULL)
	cpu_name = "opteron";
      else if (strstr(buf, "Intel(R) Pentium(R) 4 ") != NULL)
	cpu_name = "pentium4";
      else if (strstr(buf, "Intel(R) Xeon(TM) ") != NULL) {
	cpu_name = "xeon";
	cpu_name_64bit = "em64t";
      } else if (strstr(buf, "unknown") != NULL) {	// bug 5785
	char *abi_name;
	if (get_platform_abi() == ABI_64) {
	  cpu_name = "anyx86";
	  abi_name = "64-bit";
	} else {
	  cpu_name = "i386";
	  abi_name = "32-bit";
	}
	warning("CPU model name in /proc/cpuinfo is \"unknown\".  "
		"Defaulting to basic %s x86.", abi_name);
      }
    } else if (strstr(buf, "GenuineIntel")) {
      intel = TRUE;
    } else if (strstr(buf, "AuthenticAMD")) {
      amd = TRUE;
    }
  }
  fclose(f);

  // Cannot determine CPU type from "model name".  Try the CPU family and model
  // numbers.
  if (cpu_name == NULL) {
    if (intel == TRUE) {
      if (cpu_family == 6 &&
	  model == 15) {
	cpu_name = "core";
      }
    }
  }
    
  if (cpu_name == NULL) {
    error("cannot determine CPU type from /proc/cpuinfo");
    return NULL;
  }

  // If cpuinfo doesn't say if CPU is 32 or 64-bit, ask the OS.
  if (cpu_name_64bit != NULL) {
    if (get_platform_abi() == ABI_64) {
      cpu_name = cpu_name_64bit;
    }
  }

  return cpu_name;
}

// Find the target name from the CPU name.
static void
set_cpu(char *name, m_flag flag_type)
{
  // If parsing the default options, don't change the target cpu if it is
  // already set.
  if (parsing_default_options &&
      target_cpu != NULL) {
    drop_option = TRUE;
    return;
  }

  // Warn if conflicting CPU targets are specified.
  // XXX We are not compatible with gcc here, which assigns different meanings to the
  // -march, -mtune and -mcpu flags.  We treat them as synonyms, which we should not.
  if (target_cpu != NULL &&
      strcmp(target_cpu, name)) {
    warning("CPU target %s conflicts with %s; using latter (%s)",
	    get_toggle_name((int*)&target_cpu), name, name);
    // Reset target_cpu so that the driver will complain if a new value can't
    // be determined.
    target_cpu = NULL;
  }
  target_cpu = name;
  add_toggle_name((int*)&target_cpu, name);
}

#ifdef TARG_X8664
static void
Get_x86_ISA ()
{
  char *name;

  // Get a more specific cpu name.
  if (!strcmp(target_cpu, "auto")) {		// auto
    name = get_auto_cpu_name();
    if (name == NULL)
      return;
  } else if (!strcmp(target_cpu, "anyx86")) {	// anyx86
    // Need ABI to select any_32bit_x86 or any_64bit_x86 ISA.
    if (abi == UNDEFINED) {
      if (get_platform_abi() == ABI_64) {
	abi = ABI_64;
	add_option_seen(O_m64);
      } else {
	abi = ABI_N32;
	add_option_seen(O_m32);
      }
    }
    switch (abi) {
      case ABI_N32:	name = "any_32bit_x86"; break;
      case ABI_64:	name = "any_64bit_x86"; break;
      default:		internal_error("illegal ABI");
    }
  } else
    name = target_cpu;

  for (int i = 0; supported_cpu_types[i].cpu_name; i++) {
    if (strcmp(name, supported_cpu_types[i].cpu_name) == 0) {
      target_cpu = supported_cpu_types[i].target_name;
      target_supported_abi = supported_cpu_types[i].abi;
      target_supports_sse2 = supported_cpu_types[i].supports_sse2;
      target_prefers_sse3 = supported_cpu_types[i].prefers_sse3;
      break;
    }
  }

  if (target_cpu == NULL) {
    error("unknown CPU type \"%s\"", name);
  }
}

// Return TRUE if there is no error, else return FALSE.
static boolean
Get_x86_ISA_extensions ()
{
  // Quit if the user requests an ISA extension that is not available on the
  // target processor.  Add extensions as necessary.  Bug 9692.
  if (sse2 == TRUE &&
      !target_supports_sse2) {
    error("Target processor does not support SSE2.");
    return FALSE;
  }


  if (abi == UNDEFINED) {
    internal_error("Get_x86_ISA_extensions: ABI undefined\n");
    return FALSE;
  }

  // For x86-64, 64-bit code always use SSE2 instructions.
  if (abi == ABI_64) {
    if (sse2 == FALSE) {
      warning("SSE2 required for 64-bit ABI; enabling SSE2.");
    }
    sse2 = TRUE;
  } else {
    // For m32, use SSE2 on systems that support it.
    if (sse2 == UNDEFINED &&
	target_supports_sse2) {
      sse2 = TRUE;
    }
  }

  // Use SSE3 on systems that prefer it.
  if (target_prefers_sse3 &&
      sse2 != FALSE &&
      sse3 != FALSE) {
    sse2 = TRUE;
    sse3 = TRUE;
  }

  // No error.  Don't count warnings as errors.
  return TRUE;
}
#endif

#ifdef KEY /* Bug 11265 */
static void
accumulate_isystem(char *optargs)
{
  if (!isystem_dirs) {
    isystem_dirs = init_string_list();
  }
# define INCLUDE_EQ "-include="
  char *temp = malloc(strlen(optargs) + sizeof INCLUDE_EQ);
  add_string(isystem_dirs, strcat(strcpy(temp, INCLUDE_EQ), optargs));
}
#endif /* KEY Bug 11265 */

#include "opt_action.i"
