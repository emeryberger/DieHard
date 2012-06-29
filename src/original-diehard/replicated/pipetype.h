// -*- C++ -*-

/**
 * @file   pipetype.h
 * @brief  For the replicated version of DieHard: encapsulates pipe information.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#ifndef _PIPETYPE_H_
#define _PIPETYPE_H_

#include <unistd.h>
#include <fcntl.h>

class pipeType {
public:

  pipeType (void)
    : _input (-1),
      _output (-1)
  {
  }

  void init (void) {
    pipe ((int *) this);
  }

  int getInput (void) const {
    return _input;
  }
  int getOutput (void) const {
    return _output;
  }
  void closeInput (void) {
    close (_input);
  }
  void closeOutput (void) {
    close (_output);
  }
  ~pipeType (void) {
  }

private:
  int _input;
  int _output;
}; 

#endif
