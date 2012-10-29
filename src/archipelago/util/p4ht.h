#ifndef __P4HT_H__
#define __P4HT_H__

#define CACHE_SIZE           512*1024 // 512k cache
#define LOG_CACHE_SIZE       19       // log_2 512k
#define CACHE_LINE_SIZE      64       // cache line in Netburst is 64 bytes
#define LOG_CACHE_LINE_SIZE  6        // log_2 64
#define CACHE_LINE_SIZE_MASK 0xFC0    // 0x00000FC0 for 64 byte cache lines 
#define CACHE_LINE_REM_MASK  0x3F
#define CACHE_LINE_DIV_SHIFT 6
#define CACHE_LINES_ON_PAGE  64
#define PAGE_MASK            ~0xFFF
#endif
