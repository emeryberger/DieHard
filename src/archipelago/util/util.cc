#include <errno.h>
#include <unistd.h>

#include "log.h"
#include "myassert.h"
#include "randomnumbergenerator.h"
#include "processor.h"
#include "util.h"

namespace QIH {

  /*
   * Size of the page on the system, has to be initialized
   */
  size_t QIH_PAGE_SIZE = 4096;

  /*
   * Decodes errno error messages
   * FIXME: should be turned into lookup table?
   */
  const char *getLastError() {
    
    switch(errno) {
    case EFAULT: 
      return "EFAULT";
      break;
    case EINVAL: 
      return "EINVAL";
      break;
    default: 
      return "Unknown error";
      break;
    }
  }

  /*
   * Helper function to compute the size of size of an object in cache lines, rounded to the next integer
   */
  inline unsigned int sizeInCacheLines(size_t sz) {
    return (sz >> CACHE_LINE_DIV_SHIFT) + ((sz & CACHE_LINE_REM_MASK)? 1 : 0);
  }

  /*
   * Randomly determines object start on the page so that the object doesn't overflow into the next page
   */
  void *getRandomColoring(void *page, size_t sz) { 

    static RandomNumberGenerator rnd(0xDEADBEEF, 0x01001001);

    unsigned int colored_offset = 0;

    //last legal placement position on a page for the objects of this size 
    unsigned int max_legal_index = CACHE_LINES_ON_PAGE - sizeInCacheLines(sz);
  
    if (max_legal_index > 0)
      colored_offset = (rnd.next() % max_legal_index) * CACHE_LINE_SIZE;

    void *res = (void*)((char*)page + colored_offset); 

    logWrite(INFO, "Coloring for page %p, object size %d is %p (offset 0x%x)\n", page, sz, res, colored_offset); 

    return res;
  }


  /*
   * Extended wrap-around counter-based coloring, with separate counter for each size class
   */
  void *getWraparoundColoringEx(void *page, size_t sz) {
    static unsigned int counter[65]     = { 0 };
    static unsigned int max_counter[65] = {  64, 63, 62, 61, 60,
					     59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 
				             49, 48, 47, 46, 45, 44, 43, 42, 41, 40,
				             39, 38, 37, 36, 35, 34, 33, 32, 31, 30,
			       		     29, 28, 27, 26, 25, 24, 23, 22, 21, 20,
					     19, 18, 17, 16, 15, 14, 13, 12, 11, 10,
					      9,  8,  7,  6,  5,  4,  3,  2,  1,  0 };  

    unsigned int roundedSize = sizeInCacheLines(sz);
  
    unsigned int colored_offset = counter[roundedSize] * CACHE_LINE_SIZE;
  
    //update the counter
    counter[roundedSize]++;
    if (counter[roundedSize] > max_counter[roundedSize])
      counter[roundedSize] = 0;
  
    void *res = (void*)((char*)page + colored_offset); 

    //ASSERT_LOG((sz + colored_offset) >= QIH_PAGE_SIZE, "Invalid coloring %p (offset 0x%x) for object size %d\n", res, colored_offset, sz); 

    logWrite(DEBUG, "Coloring for page %p, object size %d is %p (offset 0x%x)\n", page, sz, res, colored_offset); 

    return res;

  }

  /*
   * Simple wrap-around counter-based coloring with 1 counter going backwards
   */
  void *getWraparoundColoring(void *page, size_t sz) {
    static unsigned int counter = 63;

    unsigned int max_legal_index = CACHE_LINES_ON_PAGE - sizeInCacheLines(sz);

    unsigned int colored_offset = MIN(max_legal_index, counter) * CACHE_LINE_SIZE;

    //update the counter
    if ((counter == 0) || (max_legal_index == 0))
      counter = 63;
    else 
      counter = MIN(max_legal_index, counter) - 1;

    void *res = (void*)((char*)page + colored_offset); 

    logWrite(DEBUG, "Coloring for page %p, object size %d is %p (offset 0x%x)\n", page, sz, res, colored_offset); 

    return res;

  }

  /*
   * Random coloring that takes randomness from the page address, based on Ed'd idea
   */
  void *getPageRandomColoring(void *page, size_t sz) {
    
    //last legal placement position on a page for the objects of this size
    size_t max_legal_offset = QIH_PAGE_SIZE - sizeInCacheLines(sz) * CACHE_LINE_SIZE;

    //pull 6 higher order bits into position and mask off the rest
    size_t random_seed = ((size_t)page >> (LOG_CACHE_SIZE - LOG_CACHE_LINE_SIZE)) &
      CACHE_LINE_SIZE_MASK;

    //compute the real offset by limiting the random seed
    size_t colored_offset = random_seed & max_legal_offset;
    
    void *res = (void*)((char*)page + colored_offset); 

    logWrite(DEBUG, "Coloring for page %p, object size %d is %p (offset 0x%x)\n", page, sz, res, colored_offset); 

    return res;
  }


  size_t getActualSize(void *ptr, size_t hint) {
//     size_t *curWord = (size_t *)((size_t)getPageStart(ptr) + QIH_PAGE_SIZE - sizeof(size_t)); 
	  
//     while ((curWord >= ptr) && (!*curWord)) curWord--; //walk until start or non-zero
      
//     if ((curWord >= ptr) && *curWord) {
// 	//found non-zero word
//       size_t new_size = (size_t)curWord + sizeof(size_t) - (size_t)ptr;

//       //FIXME: Remove this and corresponding change to PageDescription 

// 	return new_size;
//       } else {
// 	//page is zero up to start
// 	logWrite(INFO, "Page %p is empty up to the object start of %p\n", pd->getRealPage(), pd->getRealAddress());
	return 0;
	//      }

  }

}
