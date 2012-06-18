/**
 * @file   diehard.cpp
 * @brief  For the replicated version of DieHard: the main program.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 *
 **/

#include <assert.h>

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "selector.h"
#include "pipetype.h"
#include "randomnumbergenerator.h"
#include "realrandomvalue.h"

/*
       (executableStdin)            (executableStdout)       (bufferFull)
  main     ->       spawnExecutable      ->          reader       ->       voter  -> [stdout]
           ->            "               ->            "     (continuePipe)
           ->            "               ->            "          <-

*/

/// The signal handler, invoked when a child crashes.
extern "C" void handleChildCrash (int)
{
  static int count = 0;
  int status;
  char buf[255];
  
  int pid = wait3(&status, 0, 0);
  if (!WIFEXITED(status)) {
    int result = WEXITSTATUS(status);
    if (result != 0) {
      // Not a normal exit.
      if (WIFSIGNALED(status)) {
	int coreCause = WTERMSIG(status);
	sprintf (buf, "Replica %d dumped core: result = %d, signal was %d", pid, result, coreCause);
      } else if (WIFSTOPPED(status)) {
	int cause = WSTOPSIG(status);
	sprintf (buf, "Replica %d stopped: result = %d, signal was %d", pid, result, cause);
      } else {
	sprintf (buf, "Replica %d stopped, result = %d\n", pid, result);
      }
      fprintf (stderr, buf);
      exit(result);
    }
  } else {
    // sprintf (buf, "Replica %d exited normally.\n", pid);
    // fprintf (stderr, buf);
  }
  count++;
  if (count == 2) {
    ReplicasAliveNow--;
    count = 0;
  }
}


/// The number of replicas to execute.
int NUMBER_OF_REPLICAS;

/// The number of replicas currently executing.
int ReplicasAliveNow;

/// Invokes the replicated version of DieHard.
int main (int argc, char ** argv)
{

  if (argc < 4) {
    fprintf (stderr, "Usage: diehard <#clones> <library> <executable> <args>\n");
    return -1;
  }

  NUMBER_OF_REPLICAS = atoi(argv[1]);

  if (NUMBER_OF_REPLICAS < 2) {
    fprintf (stderr, "Error: Number of clones must be at least two.\n");
    return -1;
  }

  fprintf (stderr, "spawning %d replicas.\n", NUMBER_OF_REPLICAS);
  ReplicasAliveNow = NUMBER_OF_REPLICAS;

  int executablePid[NUMBER_OF_REPLICAS];
  int readerPid[NUMBER_OF_REPLICAS];
  int voterPid;

  // Establish a shared space.
  
  int fd = open ("/dev/zero", O_RDWR);
  int len = BUFFER_LENGTH * NUMBER_OF_REPLICAS;
  char * space = (char *) mmap (0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); 
  
  // Fill it with random junk.
  
  RandomNumberGenerator rng (RealRandomValue::value(), RealRandomValue::value());
  long * l = (long *) space;
  for (size_t i = 0; i < len / sizeof(long); i++) {
    *l = rng.next();
    l++;
  }
  close (fd);

  // Pipes for signalling that a buffer has filled from the replicas
  // to the voter.
  pipeType bufferFull[NUMBER_OF_REPLICAS];
  
  // Used for sending signals to wake up the replicas.
  pipeType continuePipe[NUMBER_OF_REPLICAS];

  // The pipe connecting stdin to each replica (sent by main to the
  // executables (see spawnExecutable)).
  pipeType executableStdin[NUMBER_OF_REPLICAS];
  
  // The pipe connecting each replica's stdout to write to the shared
  // memory buffer (written by 'reader').
  pipeType executableStdout[NUMBER_OF_REPLICAS];

  // copy out argv into a string.
  char executeString[2048];
  executeString[0] = '\0';
  int i = 4;
  while (i <= argc) {
    sprintf (executeString, "%s %s", executeString, argv[i-1]);
    i++;
  }
  fprintf (stderr, "executeString = %s\n", executeString);
  fflush (stderr);
  
  // Spawn the executables and the readers.
  for (int i = 0; i < NUMBER_OF_REPLICAS; i++) {

    executableStdin[i].init();
    executableStdout[i].init();

    if ((executablePid[i] = fork()) == 0) {
      // child executable
      executableStdin[i].closeOutput();
      executableStdout[i].closeInput();
      FILE * input = fdopen (executableStdin[i].getInput(), "rb");
      FILE * output = fdopen (executableStdout[i].getOutput(), "wb");

      char wholeString[4096];
      sprintf (wholeString, "LD_PRELOAD=%s:/usr/lib/libdl.so %s", argv[2], executeString);

      spawnExecutable (input, output, i, wholeString);

      exit(0);
    }

    executableStdin[i].closeInput();
    executableStdout[i].closeOutput();

    bufferFull[i].init();
    continuePipe[i].init();

    // Spawn the readers.
    if ((readerPid[i] = fork()) == 0) {
      bufferFull[i].closeInput();
      continuePipe[i].closeOutput();
      executableStdin[i].closeOutput();
 
      FILE * input = fdopen (executableStdout[i].getInput(), "rb");
      FILE * bufferFullSignal = fdopen (bufferFull[i].getOutput(), "wb");
      FILE * continueSignal = fdopen (continuePipe[i].getInput(), "rb");
      reader (space,
	      input,
	      bufferFullSignal,
	      continueSignal,
	      i);
      exit (0);
    }

    executableStdout[i].closeInput();
    bufferFull[i].closeOutput();
    continuePipe[i].closeInput();
  }

  // Spawn the voter.
  if ((voterPid = fork()) == 0) {
    for (int i = 0; i < NUMBER_OF_REPLICAS; i++) {
      executableStdin[i].closeOutput();
    }
    voter (space, bufferFull, continuePipe);
    exit (0);
  }

  for (int i = 0; i < NUMBER_OF_REPLICAS; i++) {
    bufferFull[i].closeInput();
    continuePipe[i].closeOutput();
  }

  // Ignore broken pipes.
  signal (SIGPIPE, SIG_IGN);

  // Send all other child signals to the signal handler.
  signal (SIGCHLD, handleChildCrash);


  // Broadcast stdin to the replicas.

  FILE * out[NUMBER_OF_REPLICAS];

  // Establish file mappings to the pipes.
  for (int i = 0; i < NUMBER_OF_REPLICAS; i++) {
    out[i] = fdopen (executableStdin[i].getOutput(), "wb");
  }

  // Read data from stdin until we're done,
  // and copy it to all of the processes.
  int c;
  while ((c = getchar()) != EOF) {
    for (int i = 0; i < NUMBER_OF_REPLICAS; i++) {
      fputc (c, out[i]);
    }
  }


  // We're done with input -- send everything out.
  for (int i = 0; i < NUMBER_OF_REPLICAS; i++) {
    fflush (out[i]);
    fclose (out[i]);
  }

  // We're done -- close up shop and wait for the child processes.

  for (int i = 0; i < NUMBER_OF_REPLICAS * 2; i++) {
    wait (NULL);
  }
 
  return 0;
}
