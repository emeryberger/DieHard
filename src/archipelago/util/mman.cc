#ifdef MADVISE_RATELIMITED

#include <unistd.h>

#include "log.h"
#include "mman.h"
#include "sparseheaps.h"

#define BACKOFF_RATE 2 //really factor of 4
#define INITIAL_RATE 64
#define INCREASE_RATE 1

namespace QIH {
  
  static unsigned int rate = INITIAL_RATE;

  int Mdiscard(void *ptr, size_t sz) {
    static unsigned int count = 1;

    unsigned int res = 0;

    logWrite(INFO, "In Mdiscard, rate is %d, count is %d\n", rate, count);

    if (count == rate) {

      logWrite(INFO, "%d: madvised 1 in %d pages\n", getAllocationClock(), rate);
      
      res = madvise(ptr, sz, MADV_FREE);
      
//       if (rate < INITIAL_RATE) {
//  	rate += INCREASE_RATE;
//  	logWrite(FATAL, "%d: Next time will madvise 1 in %d pages\n", getAllocationClock(), rate);
//       }
      
//       rate += INCREASE_RATE;
//       logWrite(FATAL, "%d: Decreased madvise rate to 1 in %d pages\n", getAllocationClock(), rate);

      count = 1;
    } 
    else count++;
    
    //    if (rate)
    //  res = madvise(ptr, sz, MADV_FREE);

    return res;
  }

  void increaseMdiscardRate(void) {
//     if (rate > 1) {
//       rate = rate >> BACKOFF_RATE;
//       if (rate <= 1)
	rate = 1;
      logWrite(INFO, "%d: Increased madvise rate to 1 in %d pages\n", getAllocationClock(), rate);
      //    }
  }

  void decreaseMdiscardRate(void) {
//     if (rate < INITIAL_RATE) {
//       rate += INCREASE_RATE;
//       
//     }

  }
}

#endif
