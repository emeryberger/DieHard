/* -*- C++ -*- */

#ifndef __QIH_SIGSEGV_H__
#define __QIH_SIGSEGV_H__

#include <signal.h>
#include <sys/types.h>

namespace QIH {
  typedef void (*signal_handler)(int sugnum, siginfo_t *siginfo, void *context);

  //install handler for a signal
  void install_handler(int sig, signal_handler handler, int flags);

  //particular handlers
  void sigsegv_handler(int sugnum, siginfo_t *siginfo, void *context);
#ifndef OPP_ONLY_HEAP
  void   sigvm_handler(int sugnum, siginfo_t *siginfo, void *context);
#endif
}

#endif
