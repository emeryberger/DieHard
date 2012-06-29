// -*- C++ -*-

/**
 * @file   mostcommonelement.h
 * @brief  For the replicated version of DieHard: find most common element in an array.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 *
 **/


#ifndef _MOSTCOMMONELEMENT_H_
#define _MOSTCOMMONELEMENT_H_

// Find the most common element in an array.  Yes, it's O(n^2), but
// it's dead simple and more than fast enough -- sorting the array
// (the basis for an easy O(n log n) algorithm) would likely be slower
// for small arrays.

template <typename T>
inline void mostCommonElement (T * array, int length, T& result, int& count) {
  int maxOccurrences = 0;
  T mostCommonItem;
  for (int i = 0; i < length; i++) {
    int thisFrequency = 0;
    T& thisItem = array[i];
    for (int j = 0; j < length; j++) {
      if (thisItem == array[j]) {
	thisFrequency++;
	if (thisFrequency > maxOccurrences) {
	  mostCommonItem = thisItem;
	  maxOccurrences = thisFrequency;
	}
      }
    }
  }
  result = mostCommonItem;
  count = maxOccurrences;
}

#endif
