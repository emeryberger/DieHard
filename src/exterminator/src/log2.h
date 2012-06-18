#ifndef _LOG2_H_
#define _LOG2_H_

  /// Quickly calculate the CEILING of the log (base 2) of the argument.
#if defined(_WIN32)
  static inline int log2 (size_t sz) 
  {
    int retval;
    sz = (sz - 1) << 1;
    __asm {
      bsr eax, sz
	mov retval, eax
	}
    return retval;
  }
#else
  static inline int log2 (size_t sz) {
    int log = 0;
    int value = 1;
    while (value < sz) {
      value <<= 1;
      log++;
    }
    return log;
  }
#endif

#endif
