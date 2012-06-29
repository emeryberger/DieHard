// -*- C++ -*-

/**
 * @file   fingerprinter.h
 * @brief  For the replicated version of DieHard: computes a "fingerprint" of a buffer.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 *
 **/

#ifndef _FINGERPRINTER_H_
#define _FINGERPRINTER_H_

class Fingerprinter {
public:

  Fingerprinter (void)
    : _value (-1),
      _start (0),
      _length (0)
  {}

  void * getStart (void) const {
    return _start;
  }

  inline void operator()(void * ptr, size_t len) {
    _start = ptr;
    _length = len;
#if 0
    _value = hash ((unsigned char *) _start, _length, 257, 1024);
#else
    // For now, we just sum up the values.
    char * p = (char *) ptr;
    _value = 0;
    for (size_t i = 0; i < len; i++) {
      //      fputc (*p, stdout);
      _value += *p;
      p++;
    }
#endif
  }

  friend bool operator==(Fingerprinter& f1, Fingerprinter& f2) {
    return (f1._value == f2._value);
  }

  Fingerprinter (const Fingerprinter& f) 
    : _value (f._value),
      _start (f._start),
      _length (f._length)
  {}

  Fingerprinter& operator=(const Fingerprinter& f) {
    _value = f._value;
    _start = f._start;
    _length = f._length;
    return *this;
  }

private:

  // Rabin-Karp hash algorithm.
  inline long hash (unsigned char * str, int len, long base, long modulus) {
    long value = 0;
    long power = 1;
    for (int i = 0; i < len; i++) {
      value += power * str[i];
      value %= modulus;
      power *= base;
      power %= modulus;
    }
    return value;
  }


  int _value;
  void * _start;
  int _length;
};


#endif
