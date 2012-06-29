// -*- C++ -*-

/**
 * @file   selector.h
 * @brief  For the replicated version of DieHard: defines and function prototypes.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 *
 **/


#ifndef _SELECTOR_H_
#define _SELECTOR_H_

extern int NUMBER_OF_REPLICAS;

extern int ReplicasAliveNow;

/// The maximum number of replicas allowed.
const int MAX_REPLICAS = 1024;

/// The length of the output buffer.
const int BUFFER_LENGTH = 4096;

class pipeType;

void reader (char * space,
	     FILE * input,
	     FILE * bufferFull,
	     FILE * continueSignal,
	     const int index);

void voter (char * space,
	    pipeType * fullpipe,
	    pipeType * continuepipe);

void spawnExecutable (FILE * in,
		      FILE * out,
		      const int index,
		      char * execname);

#endif
