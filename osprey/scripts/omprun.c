#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <libgen.h>		/* basename */
#include <string.h>		/* strdup */

#include <unistd.h>		/* env query */
extern char **environ;

static const char *short_opts = "n:hs";

static struct option long_opts[] = {
  {"help", no_argument, NULL, 0},
  {"show", no_argument, NULL, 0},

  {"num-threads", required_argument, NULL, 0},
  {"stacksize", required_argument, NULL, 0},
  {"xbarrier-type", required_argument, NULL, 0},
  {"task-cutoff", required_argument, NULL, 0},
  {"task-pool", required_argument, NULL, 0},
  {"task-queue", required_argument, NULL, 0},
  {"task-queue-num-slots", required_argument, NULL, 0},
  {"task-chunk-size", required_argument, NULL, 0},
  {"queue-storage", required_argument, NULL, 0},

  {"nested", optional_argument, NULL, 'N'},

  {NULL, 0, NULL, 0}		/* sentinel */
};

static char *progname;

static int do_show = 0;

/* ------------------------------------------------------------------------ */

static FILE *helpfh;

static void
help_and_bail (void)
{
  fprintf (helpfh, "Usage: %s [options...] program\n", progname);
  fprintf (helpfh, "\n");
  fprintf (helpfh, "Where options are\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--num-threads=NT              NT is number of OpenMP threads\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--stacksize=SS                SS is OpenMP stack size\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--nested[=B]                  B is one of\n");
  fprintf (helpfh, "                                true | false\n");
  printf
    ("                              Defaults to \"true\" if B not specified\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--xbarrier-type=BT            BT is one of\n");
  fprintf (helpfh, "                                linear|simple|tour|tree|dissem\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--task-cutoff=TC              TC is\n");
  fprintf (helpfh, "                                cutoff:val[,cutoff:val[,...]]\n");
  fprintf (helpfh, "                              where\n");
  fprintf (helpfh, "                                cutoff is one of\n");
  printf
    ("                                  always | never | num_threads | switch |\n");
  fprintf (helpfh, "                                    depth | num_children\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "                                val < 1 disables the cutoff,\n");
  fprintf (helpfh, "                                val > 0 specifies cutoff limit\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--task-pool=TP               TP is one of\n");
  fprintf (helpfh, "                               default | simple | 2level |\n");
  printf
    ("                                 simple_2level | public_private | multilevel\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--task-queue=TQ              TQ is one of\n");
  printf
    ("                               deque | cfifo | lifo | fifo | inv_deque\n");
  fprintf (helpfh, "\n");
  printf
    ("--task-queue-num-slots=NS    NS is initial size for each thread's task queue\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--task-chunk-size=CS         CS is 1 - task-queue-num-slots\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--queue-storage=QS           QS is one of\n");
  printf
    ("                               array | dyn_array | list | lockless\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--help, -h                   This message\n");
  fprintf (helpfh, "\n");
  fprintf (helpfh, "--show, -s                   Output the command to be run\n");
  fprintf (helpfh, "\n");

  exit (1);
}

static void
set_env (const char *var, const char *val)
{
  setenv (var, val, 1);
}

static void
handle_long_option (const char *opt, const char *val)
{
  if (strcmp (opt, "help") == 0)
    {
      help_and_bail ();
      /* NOT REACHED */
    }

  if (strcmp (opt, "show") == 0)
    {
      do_show = 1;
    }
  else if (strcmp (opt, "num-threads") == 0)
    {
      set_env ("OMP_NUM_THREADS", val);
    }
  else if (strcmp (opt, "stacksize") == 0)
    {
      set_env ("OMP_STACKSIZE", val);
    }
  else if (strcmp (opt, "xbarrier-type") == 0)
    {
      set_env ("O64_OMP_XBARRIER_TYPE", val);
    }
  else if (strcmp (opt, "task-cutoff") == 0)
    {
      set_env ("O64_OMP_TASK_CUTOFF", val);
    }
  else if (strcmp (opt, "task-pool") == 0)
    {
      set_env ("O64_OMP_TASK_POOL", val);
    }
  else if (strcmp (opt, "task-queue") == 0)
    {
      set_env ("O64_OMP_TASK_QUEUE", val);
    }
  else if (strcmp (opt, "task-queue-num-slots") == 0)
    {
      set_env ("O64_OMP_TASK_QUEUE_NUM_SLOTS", val);
    }
  else if (strcmp (opt, "task-chunk-size") == 0)
    {
      set_env ("O64_OMP_TASK_CHUNK_SIZE", val);
    }
  else if (strcmp (opt, "queue-storage") == 0)
    {
      set_env ("O64_OMP_QUEUE_STORAGE", val);
    }
  else if (strcmp (opt, "nested") == 0)
    {
      set_env ("OMP_NESTED", val);
    }
  else
    {
      ;				/* error */
    }
}

int
main (int argc, char *argv[])
{
  int opt_index;
  char *inf_prog;

  progname = basename (argv[0]);

  /* or stderr, perhaps? */
  helpfh = stdout;

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
	  handle_long_option ("num-threads", optarg);
	  break;

	case 'N':
	  handle_long_option ("nested", (optarg != NULL) ? optarg : "true");
	  break;

	case 's':
	  handle_long_option ("show", optarg);
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

  if (do_show)
    {
      int oi = optind;
      char **e = environ;

      printf ("%s environment:\n", progname);
      while (*e != NULL)
	{
	  if ((strncmp (*e, "OMP", 3) == 0) || (strncmp (*e, "O64", 3) == 0))
	    {
	      printf ("  %s\n", *e);
	    }
	  e += 1;
	}
      printf ("\n");
      printf ("%s execution:\n", progname);
      printf ("  %s ", argv[oi]);
      oi += 1;
      while (oi < argc)
	{
	  printf ("'%s' ", argv[oi]);
	  oi += 1;
	}
      printf ("\n");
      printf ("\n");
    }


  execvp (argv[optind], &argv[optind]);

  /* should not get here */
  return 1;
}
