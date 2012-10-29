#ifndef __QIH_AMD32_256K_H__
#define __QIH_AMD32_256K_H__

//
// For 32-bit AMD processors with 256K L2 Cache
// Such as my Sempron 2200+
//

#define CACHE_SIZE           256*1024 // 256k cache
#define LOG_CACHE_SIZE       18       // log_2 512k
#define CACHE_LINE_SIZE      64       // cache line in Netburst is 64 bytes
#define LOG_CACHE_LINE_SIZE  6        // log_2 64
#define CACHE_LINE_SIZE_MASK 0xFC0    // 0x00000FC0 for 64 byte cache lines 
#define PAGE_MASK			 0xFFFFF000


#endif /*AMD32_256K_H_*/
