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

void spawnExecutable (FILE * in,
		      FILE * out,
		      const int index,
		      char * execname)
{
  // Redirect stdin to read from the pipe.
  dup2 (fileno(in), 0);
  close (fileno(in));

  // Redirect stdout to executableStdout.
  dup2 (fileno(out), 1);
  close (fileno(out));

  system (execname);
  fflush (stdout);
}
