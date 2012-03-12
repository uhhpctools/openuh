/*
 cafrun wrapper script for running CAF programs.

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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <libgen.h>		/* basename */
#include <string.h>		/* strdup */
#include <stdarg.h>		/* varargs */

#include <sys/types.h>
#include <sys/stat.h>		/* stat */

#include <unistd.h>		/* env query */
extern char **environ;

static const char *short_opts = "n:hHv";

static struct option long_opts[] = {
  {"help", no_argument, NULL, 0},
  {"help2", no_argument, NULL, 0},
  {"verbose", no_argument, NULL, 0},

  {"num-images", required_argument, NULL, 0},
  {"log-info", required_argument, NULL, 0},
  {"log-file", required_argument, NULL, 0},
  {"image-heap", required_argument, NULL, 0},

  {NULL, 0, NULL, 0}		/* sentinel */
};

static short do_verbose;

static char *progname;

static char *comm_layer = NULL;
static char *gasnet_conduit = NULL;

static char buf[BUFSIZ];	/* utility buffer, used throughout */
static char caf_env[BUFSIZ];	/* record env vars set for/by CAF */
static short caf_env_init = 0;

static char cmd_buf[BUFSIZ];	/* for launching the inferior program */
static char launcher[BUFSIZ];

static long image_heap;
static long num_images;


/* ------------------------------------------------------------------------ */

static FILE *helpfh;

static void
help_and_bail (void)
{
  fprintf (helpfh, "Usage: %s [OPTIONS]... [PROGRAM] [-- [LAUNCHER_OPTIONS]]", progname);
  fprintf (helpfh, "\n");
  fprintf (helpfh, "Where options are\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "-n N, --num-images=N          NT is number of images to use\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--log-info=L                  Sets additional info for runtime to log.\n");
  fprintf (helpfh, "                              L=all or info list delimited by ':'.\n");
  fprintf (helpfh, "                                Available information to log:\n");
  fprintf (helpfh, "                                FATAL:DEBUG:TIME:NOTICE:TIME_SUMMARY:INIT:MEMORY:\n");
  fprintf (helpfh, "                                CACHE:BARRIER:REDUCE:SYMBOLS\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--log-file=F                  F is filename where log will be written\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--image-heap=I                I is heap size per image for storing coarray data. It\n");
  fprintf (helpfh, "                              may be an integer which indicates size in bytes, or it\n");
  fprintf (helpfh, "                              may have the suffixes K, M, G which indicates size in\n");
  fprintf (helpfh, "                              kilobytes, megabytes, and gigabytes, respectively.\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "-h, --help,                   Displays this menu\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "-H, --help2                   Displays more options for underlying launcher ($LAUNCHER)\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "-v, --verbose                 Verbose\n");
  fprintf (helpfh, "\n");

  fprintf (helpfh, "Environment Variables\n");
  fprintf (helpfh, "\n");

  fprintf (helpfh, "   LAUNCHER                   Underlying program to use for launching CAF images.\n");
  fprintf (helpfh, "                                Defaults to %s\n", launcher);
  fprintf (helpfh, "\n");
  fprintf (helpfh, "   LAUNCHER_OPTS              Additional options to pass to underlying launcher apart\n");
  fprintf (helpfh, "                              from number of processes to spawn.\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "   UHCAF_LOG_INFO             Specifies information for runtime to log.\n");
  fprintf (helpfh, "                                Can be set with --log-info\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "   UHCAF_LOG_FILE             Specifies file to store runtime log\n");
  fprintf (helpfh, "                                Can be set with --log-file\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "   UHCAF_IMAGE_NUM_IMAGES     Specifies number of images to launch.\n");
  fprintf (helpfh, "                                Can be set/overridden with --num-images\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "   UHCAF_IMAGE_HEAP_SIZE      Specifies heap size per image for storing coarray data.\n");
  fprintf (helpfh, "                                Can be set with --image-heap\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "   GASNET_PSHM_NODES          Set number of POSIX shared memory nodes for when gasnet-smp\n");
  fprintf (helpfh, "                              layer is used.\n");
  fprintf (helpfh, "                                Can be set with --num-images\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "   UHCAF_NBPUT                Set to 1 to enable Nonblocking put runtime optimization.\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "   UHCAF_GETCACHE             Set to 1 to enable getcache runtime optimization.\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "   UHCAF_GETCACHE_LINE_SIZE   Specifies the size (in bytes) of the cache line used by\n");
  fprintf (helpfh, "                              getcache optimizer. By default it is 64KB.\n");

  exit (1);
}

static void
launcher_usage (void)
{
  execlp (launcher, "--help", (char *) NULL);
  /* NOT REACHED */
  exit (1);
}

static void
add_to_env_list (const char *var, const char *val)
{
  if (! caf_env_init)
    {
      caf_env[0] = '\0';
      caf_env_init = 1;
    }
  strncat (caf_env, "  ", BUFSIZ);
  strncat (caf_env, var, BUFSIZ);
  strncat (caf_env, "=", BUFSIZ);
  strncat (caf_env, val, BUFSIZ);
  strncat (caf_env, "\n", BUFSIZ);
}

#define DO_MESSAGE(Kind)			\
  va_list ap; \
  va_start (ap, fmt); \
  vsnprintf (buf, BUFSIZ, fmt, ap); \
  va_end (ap); \
  fprintf (helpfh, "%s: %s\n", Kind, buf);

static void
print_warning (char *fmt, ...)
{
  DO_MESSAGE("Warning");
}

static void
print_error (char *fmt, ...)
{
  DO_MESSAGE("Error");

  exit (1);
}

/* ------------------------------------------------------------------- */

static void
set_env (const char *var, const char *val)
{
  setenv (var, val, 1);

  add_to_env_list (var, val);
}

static long
parse_number_unit (const char *val)
{
  char *endptr;
  long n = strtol(val, &endptr, 10);

  if ((endptr != NULL) && (*endptr != '\0'))
    {
      const char unit = *endptr;
      switch (unit)
	{
	case 'k':
	case 'K':
	  n *= 1024L;
	  break;
	case 'm':
	case 'M':
	  n *= 1024L * 1024L;
	  break;
	case 'g':
	case 'G':
	  n *= 1024L * 1024L * 1024L;
	  break;
	default:
	  print_error ("num-images has invalid unit (%c), use K, M, G", unit);
	  /* NOT REACHED */
	  break;
	}
    }
  return n;
}

static void
parse_image_heap (const char *val)
{
  image_heap = parse_number_unit (val);
  set_env ("UHCAF_IMAGE_HEAP_SIZE", val);
}

static void
parse_num_images (const char *val)
{
  num_images = parse_number_unit (val);
  set_env ("UHCAF_NUM_IMAGES", val);
}

static void
handle_long_option (const char *opt, const char *val)
{
  if (strcmp (opt, "help") == 0)
    {
      help_and_bail ();
      /* NOT REACHED */
    }
  if (strcmp (opt, "help2") == 0)
    {
      launcher_usage ();
      /* NOT REACHED */
    }

  if (strcmp (opt, "verbose") == 0)
    {
      do_verbose = 1;
    }
  else if (strcmp (opt, "num-images") == 0)
    {
      parse_num_images (val);
    }
  else if (strcmp (opt, "log-info") == 0)
    {
      char *vp = (char *) val;
      while (vp && (*vp != '\0'))
	{
	  *vp = tolower (*vp);
	  vp += 1;
	}
      set_env ("UHCAF_LOG_INFO", val);
    }
  else if (strcmp (opt, "log-file") == 0)
    {
      set_env ("UHCAF_LOG_FILE", val);
    }
  else if (strcmp (opt, "image-heap") == 0)
    {
      parse_image_heap (val);
    }
  else
    {
      ;				/* error */
    }
}

static void
env_scan (void)
{
  {
    char *image_heap_str = getenv ("UHCAF_IMAGE_HEAP_SIZE");
    if (image_heap_str != NULL)
      {
	parse_image_heap (image_heap_str);
	add_to_env_list ("UHCAF_IMAGE_HEAP_SIZE", image_heap_str);
      }
    else
      {
	parse_image_heap ("30M");
      }
  }

  {
    char *num_images_str = getenv ("UHCAF_NUM_IMAGES");
    if (num_images_str != NULL)
      {
	parse_num_images (num_images_str);
	add_to_env_list ("UHCAF_NUM_IMAGES", num_images_str);
      }
    else
      {
	num_images = 1;
      }
  }
}

static short
in_path (char *prog)
{
  char *p = getenv ("PATH");
  char exe[BUFSIZ];
  struct stat ss;

  if (p == NULL)
    {
      return 0;
    }

  while (1)
    {
      char *cn = strchr (p, ':');
      if (cn == NULL)
	{
	  break;
	}
      *cn = '\0';
      snprintf (exe, BUFSIZ, "%s/%s", p, prog);
      *cn = ':';
      if (stat (exe, &ss) == 0)
	{
	  if (S_ISREG (ss.st_mode) && (ss.st_mode & S_IXOTH))
	    {
	      return 1;
	      /* NOT REACHED */
	    }
	}
      p = cn + 1;
    }
  return 0;
}

#define CHOMP(L) (L)[strlen(L) - 1] = '\0'

static void
parse_gasnet_config (char *exe)
{
  FILE *p;
  const char *oc_layer = "OPENUH_COMM_LAYER_IS_";
  const char *oc_conduit = "OPENUH_GASNET_CONDUIT_IS_";
  char *var;
  short found_count = 0;

  snprintf (buf, BUFSIZ, "nm -a %s 2>/dev/null", exe);

  p = popen (buf, "r");
  if (p == NULL)
    {
      print_error ("Internal: unable to inspect executable (couldn't run \"nm\")");
      /* NOT REACHED */
    }

  while (1)
    {
      if (found_count == 2)	/* got both symbols, no need to read rest of output */
	{
	  break;
	}
      if (fgets (buf, BUFSIZ, p) == NULL)
	{
	  break;
	}
      var = strstr (buf, oc_layer);
      if (var != NULL)
	{
	  comm_layer = var + strlen(oc_layer);
	  CHOMP (comm_layer);
	  found_count += 1;
	  continue;
	}
      var = strstr (buf, oc_conduit);
      if (var != NULL)
	{
	  gasnet_conduit = var + strlen(oc_conduit);
	  CHOMP (gasnet_conduit);
	  found_count += 1;
	  continue;
	}
      
    }

  pclose(p);

  if (comm_layer == NULL)
    {
      print_error ("No communications layer defined");
      /* NOT REACHED */
    }

  if (gasnet_conduit == NULL)
    {
      print_error ("No GASNet conduit defined");
      /* NOT REACHED */
    }

  if (strcmp (comm_layer, "gasnet") == 0)
    {
      set_env ("GASNET_VIS_AMPIPE", "1");
      if (image_heap > 2000000000L)
	{
	  const long ih = image_heap / 1000000000L;
	  snprintf (buf, BUFSIZ, "%ldG", ih);
	  set_env ("GASNET_MAX_SEGSIZE", buf);
	}
    }

  /*
   * smp doesn't need a separate launcher program, but the other
   * conduits do
   */
  if (strcmp (gasnet_conduit, "smp") == 0)
    {
      snprintf (buf, BUFSIZ, "%d", num_images);
      set_env ("GASNET_PSHM_NODES", buf);
      /* no special launcher required */
      launcher[0] = '\0';
    }
  else
    {
      char *short_launcher = basename (launcher);

      if (! in_path (launcher))
	{
	  print_error ("Could not locate a launcher program. Specify with LAUNCHER variable");
	  /* NOT REACHED */
	}
      add_to_env_list ("LAUNCHER", launcher);
      if ((num_images > 1) &&
	  ((strcmp (short_launcher, "mpirun") == 0) ||
	   (strcmp (short_launcher, "mpiexec") == 0)))
	{
	  /* tell MPI launcher how many images */
	  char *lo = getenv ("LAUNCHER_OPTS");
	  snprintf (buf, BUFSIZ, " -n %d", num_images);
	  strncat (launcher, buf, BUFSIZ);
	  if (lo != NULL)
	    {
	      strncat (launcher, " ", BUFSIZ);
	      strncat (launcher, lo, BUFSIZ);
	    }
	}
      /* add_to_env_list ("LAUNCHER_OPTS", launcher_opts); */

      if (strcmp (gasnet_conduit, "udp") == 0)
	{
	  set_env ("GASNET_SPAWNFN", "C");
	  snprintf (buf, BUFSIZ, "%s -n %N %C", launcher);
	  set_env ("GASNET_CSPAWN_CMD", buf);
	}
    }
}

int
main (int argc, char *argv[])
{
  int opt_index;
  char *inf_prog;

  progname = basename (argv[0]);

  strncpy (launcher, "mpirun", BUFSIZ);
  num_images = 1;
  do_verbose = 0;

  /* or stderr, perhaps? */
  helpfh = stdout;

  env_scan ();

  /* let inferior program handle later args */
  setenv ("POSIXLY_CORRECT", "1", 1);

  while (1)
    {
      int c = getopt_long (argc, argv,
			   short_opts,
			   long_opts,
			   &opt_index
			   );
      if (c == -1)
	{
	  break;
	}

      switch (c)
	{
	case 0:
	  /* long option found */
	  handle_long_option (long_opts[opt_index].name, optarg);
	  break;

	case 'n':
	  handle_long_option ("num-images", optarg);
	  break;

	case 'v':
	  handle_long_option ("verbose", optarg);
	  break;

	case 'H':
	  handle_long_option ("help2", optarg);
	  break;

	case 'h':
	default:
	  handle_long_option ("help", optarg);
	  break;

	}
    }

  /* must be something left to run */
  if (optind >= argc)
    {
      help_and_bail ();
      /* NOT REACHED */
    }

  /* this sets up conduit-specific launcher */
  parse_gasnet_config (argv[optind]);

  strncpy (cmd_buf, launcher, BUFSIZ);

  {
    int oi = optind;

    while (oi < argc)
      {
	strncat (cmd_buf, " ", BUFSIZ);
	strncat (cmd_buf, argv[oi], BUFSIZ);
	oi += 1;
      }
  }

  if (do_verbose)
    {
      printf ("%s environment:\n", progname);
      puts (caf_env);
      printf ("---------------------------------\n\n");
      printf ("%s execution:\n", progname);
      printf ("  %s\n", cmd_buf);
      printf ("\n");
    }

  return system (cmd_buf);
}
