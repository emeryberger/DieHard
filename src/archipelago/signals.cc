#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "mman.h"
#include "util.h"
#include "sparseheaps.h"

#include "signals.h" 



namespace QIH {

#ifdef REPLACE_SIGACTION

  typedef int (*sigaction_function_type)(int,const struct sigaction *, struct sigaction *);

  static struct sigaction user_sigaction;

  int mysigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    static sigaction_function_type real_sigaction = 0;
    
    if(!real_sigaction) {
      real_sigaction = (sigaction_function_type) dlsym(RTLD_NEXT, "sigaction");
      if (real_sigaction) {
	stdoutWrite(QIH::INFO, "Dynamically linked to the real sigaction at %p\n", real_sigaction);
      }
    }
    
    return real_sigaction(signum,act,oldact);
  }

#else

#define mysigaction sigaction

#endif

  /*
   * Installs a function passed as an argument as SIGSEGV signal handler
   */
  void install_handler(int sig, signal_handler handler, int flags) {
	
    struct sigaction siga;
    //get the old handler parameters
    if (mysigaction(sig, NULL, &siga) == -1) {
      stdoutWrite(FATAL, "Failed to obtain signal information for signal %d due to %s\n", 
		  sig, getLastError());
#if !defined(OPP_ONLY_HEAP) && defined(VMSIG)
      if (sig == SIGVM) {
	stdoutWrite(FATAL, "This version of Archipelago was compiled for patched kernel.\n");
	stdoutWrite(FATAL, "Are you running the right kernel?\n");
	logWrite(FATAL, "This version of Archipelago was compiled for patched kernel.\n");
	logWrite(FATAL, "Are you running the right kernel?\n");

      }
#endif
      exit(1);
    }

#ifdef REPLACE_SIGACTION
    user_sigaction = siga;
#endif

    //set new handler function
    siga.sa_flags    |= SA_SIGINFO | flags; // | SA_RESETHAND
    siga.sa_sigaction = handler;
  
    if (mysigaction(sig, &siga, NULL) == -1) {
      stdoutWrite(FATAL, "Failed to set new signal handler for signal %d due to %s\n", 
		  sig, getLastError());
#if !defined(OPP_ONLY_HEAP) && defined(VMSIG)
      if (sig == SIGVM) {
	 stdoutWrite(FATAL, "This version of Archipelago was compiled for patched kernel\n"); 
	 stdoutWrite(FATAL, "Are you running the right kernel?\n");
	 logWrite(FATAL, "This version of Archipelago was compiled for patched kernel\n"); 
	 logWrite(FATAL, "Are you running the right kernel?\n");
      }
#endif

      exit(1);
    }

    logWrite(DEBUG, "Successfully registers a signal handler for signal %d\n", sig);

    /*
     * This takes care of having to register for VM events
     * Currently, two versions of API exist, for 2.4 and 2.6 Linux kernels
     * This cannot be used in OPP-only heap because we have no knowlege of what 
     * pages are actually used 
     */
#if !defined(OPP_ONLY_HEAP) && defined(VMSIG)
    if (sig == SIGVM) {
      if (vm_register(VM_EVICTING_DIRTY)) {
	stdoutWrite(ERROR, "Warning: Failed to register for kernel page eviction events.\n");
 	stdoutWrite(ERROR, "You will encounter ungraceful preformance degradation under memory pressure\n");
 	stdoutWrite(ERROR, "Are you running the right kernel?\n");
	logWrite(ERROR, "Warning: Failed to register for kernel page eviction events.\n");
 	logWrite(ERROR, "You will encounter ungraceful preformance degradation under memory pressure\n");
 	logWrite(ERROR, "Are you running the right kernel?\n");
      }
      else {
	logWrite(DEBUG, "Successfully registered for kernel eviction events\n");
      }
    }
#endif
  }

  void install_default_handler(int sig) {
    struct sigaction siga;
    //get the old handler parameters
    if (mysigaction(sig, NULL, &siga) == -1) {
      stdoutWrite(FATAL, "Failed to obtain signal information for signal %d due to %s\n", 
		  sig, getLastError());
      exit(1);
    }
    //set new handler function
    siga.sa_flags &= !SA_SIGINFO; //| SA_NODEFER; // | SA_RESETHAND
    siga.sa_handler = SIG_DFL;

    if (mysigaction(sig, &siga, NULL) == -1) {
      stdoutWrite(FATAL, "Failed to set the default signal handler for signal %d due to %s\n", 
		  sig, getLastError());
      exit(1);
    }
  }

  /*
   * SIGSEGV signal handler: uncompress pages in response to page fault
   */
  void sigsegv_handler(int signum, siginfo_t *siginfo, void *context) {

    void *page = getPageStart(siginfo->si_addr);//page which generated a fault

    logWrite(DEBUG, "%d: Took a fault at page %p (address %p)\n", getAllocationClock(), page, siginfo->si_addr); 

    char *reason;

    switch (siginfo->si_code) {
    case SEGV_ACCERR:
#ifndef OPP_ONLY_HEAP
      if (tryRestorePage(page))
	return;
#endif    
      reason = "Detected a likely overflow onto protected page";
      break;
    case SEGV_MAPERR:
      reason = "Detected a likely overflow onto unmapped page";
      break;
    default:
      reason = "Received SIGSEGV for unknown reason at page";
      break;
    }

#ifdef REPLACE_SIGACTION
    if (user_sigaction.sa_sigaction) {
      stdoutWrite(INFO, "Invoked user SIGSEGV handler\n");
      user_sigaction.sa_sigaction(signum, siginfo, context);
      return;
    }
#endif

    logWrite(FATAL, "%s %p, going down!\n", reason, page);
    logClose();
    stdoutWrite(FATAL, "%s %p, going down!\n", reason, page); 

    install_default_handler(SIGSEGV);
    kill(getpid(), SIGSEGV); //raise the signal again
  }

#if !defined(OPP_ONLY_HEAP) && defined(VMSIG)

#ifdef VMCOMM

#define CLEAN_MASK  0x1

  inline unsigned int collectPages(void *page, void **pages) {
    unsigned int i = 0;
    
    for (; (i <= VM_MAX_RELINQUISH); i++) {
      if (pageUnused(static_cast<char*>(page) + i * QIH_PAGE_SIZE)) {
	pages[i] = static_cast<char*>(page) + i * QIH_PAGE_SIZE + CLEAN_MASK; //set the last bit indicating that
      }
    }

    return i;
  }
#else//VMSIG

#define LOOKAHEAD_DISTANCE 800
  /*
   * Finds a contigous range of up to 800 pages following the given page
   * to be returned to the OS.
   */
  inline size_t collectPages(void *page) {
    size_t size = 0;
   
    for (int i = 0; (i < LOOKAHEAD_DISTANCE) && pageUnused(page + i * QIH_PAGE_SIZE); i++) 
	  size += QIH_PAGE_SIZE;

    return size;

  }
#endif


  /*
   * SIGVM signal handler: madvise page out if not needed
   */
  void sigvm_handler(int sugnum, siginfo_t *siginfo, void *context) {
    void *page = static_cast<char*>(getPageStart(siginfo->si_addr));//page which generated a fault
   
    logWrite(DEBUG, "%d: page %p is a candiate for eviction\n", getAllocationClock(), page, siginfo->si_addr); 


#ifdef IV

    increaseMdiscardRate();

#endif


#if defined(VMCOMM)

    void *unusedPages[VM_MAX_RELINQUISH];

    unsigned int count = //getVictimBuffer(unusedPages);//
      collectPages(page, unusedPages);

    if (count > 0) {
      vm_relinquish(unusedPages, count);
      //clearVictimBuffer();
      //logWrite(FATAL, "%d: Returning %d pages to the system\n", getAllocationClock(), count);
      logWrite(FATAL, "Empty page %p was scheduled for eviction, discarding it and %d unused pages around it\n", 
      	       page, count - 1);
    } else {
      //logWrite(FATAL, "%d: Unable to return any unused pages to system :-(\n", getAllocationClock());
      logWrite(FATAL, "Unable to find any unsed pages in the vicinity of %p, ignoring the signal\n", page);
    }

#else  //VMSIG 

    size_t size = collectPages(page);

    if (size > 0) {

      madvise(page, size, MADV_FREE);

      logWrite(DEBUG, "Empty page %p was scheduled for eviction, discarding it and next %d unused pages\n", 
	       page, (size / QIH_PAGE_SIZE) - 1);
    }
    else {
      logWrite(DEBUG, "Page %p that what scheduled for eviction was not empty, ignoring the signal\n", page);
    }

#endif

  }
#endif
} 

#ifdef REPLACE_SIGACTION

extern "C" {
  /*
  sighandler_t signal(int signum, sighandler_t handler) {
    struct sigaction sa, osa;
    
    sa.sa_handler = handler;
    sigemptyset (&sa.sa_mask);

    sa.sa_flags = 0;
    if (sigaction (signum, &sa, &osa) < 0)
      return SIG_ERR;
    
    return osa.sa_handler;
  }
  */

  int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) { 
    //fprintf(stderr,"sigaction wrapper");
    if(signum != SIGSEGV) {
      return QIH::mysigaction(signum,act,oldact);
    } else {
      ///FIXME: actually analyze struct sigaction
      if (act) {
	stdoutWrite(QIH::INFO, 
		    "User application attempted to install its own SIGSEGV handler, saving\n");
	//memcpy(&
	QIH::user_sigaction = *act;//, sizeof(struct sigaction));
      } else {
	//memcpy(oldact, &QIH::user_sigaction, sizeof(struct sigaction));
	*oldact = QIH::user_sigaction;
	stdoutWrite(QIH::INFO, 
		    "User application attempted to check its own handler, giving it\n");

      }

      return 0;
    }
  }
}

#endif
