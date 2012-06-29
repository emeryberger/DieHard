/**
 * @file   spawnexecutable.cpp
 * @brief  For the replicated version of DieHard: launches the application with redirected stdin/stdout
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 *
 **/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>

void spawnExecutable (FILE * in,
		      FILE * out,
		      const int index,
		      char * execname)
{
  int result;

  signal(SIGCHLD,SIG_DFL);

  // Redirect stdin to read from the pipe.
  dup2 (fileno(in), 0);
  close (fileno(in));

  // Redirect stdout to executableStdout.
  dup2 (fileno(out), 1);
  close (fileno(out));

  setpgrp();

  result = system (execname);
  fflush (stdout);

  if(WIFEXITED(result)) {
    exit(WEXITSTATUS(result));
  } else if(WIFSIGNALED(result)) {
    fprintf(stderr,"proxy %d raising\n",getpid());
    raise(WTERMSIG(result));
    exit(-1);
  } else { 
    // panic!
    assert(false); 
  }
}
