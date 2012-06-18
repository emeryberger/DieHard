/**
 * @file   reader.cpp
 * @brief  For the replicated version of DieHard: sends output to voter.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */


#include <assert.h>
#include <stdio.h>

#include "selector.h"

void reader (char * space,
	     FILE * input,
	     FILE * bufferFull,
	     FILE * continueSignal,
	     const int index)
{
  // executableStdout[index] | reader -> bufferFull[index]
  //                                  <- continuePipe[index]

  // child reader
  
  int buffCount = 0;
  char * ptr = (char *) space + index * BUFFER_LENGTH;
  
  // Now write characters into the buffer.
  int c;
  while ((c = fgetc(input)) != EOF) {

    if (buffCount >= BUFFER_LENGTH) {
      buffCount = 0;
      ptr = (char *) space + index * BUFFER_LENGTH;
      // Send a signal that we've filled up a buffer.
      fputc ('A' + (char) index, bufferFull);
      fflush (bufferFull);

      // Wait for a reply.
      fgetc (continueSignal);

    }
    buffCount++;
    *ptr++ = c;
  }

  // Done - send one final signal to the voter process.
  // Fill the rest of the buffer with zeroes.
  
  for (int j = buffCount; j < BUFFER_LENGTH; j++) {
    // Verify that ptr is in bounds.
    assert ((size_t) ptr >= (size_t) ((char *) space + index * BUFFER_LENGTH));
    assert ((size_t) ptr < (size_t) ((char *) space + (index + 1) * BUFFER_LENGTH));
    *ptr++ = '\0';
  }
  // Wake up the voter.
  fputc ('A' + (char) index, bufferFull);
  fflush (bufferFull);

}
