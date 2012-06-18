// -*- C++ -*-

/**
 * @file   callsite.h
 * @brief  All the information needed to manage one partition (size class).
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#ifndef _CALLSITE_H_
#define _CALLSITE_H_

#include "platformspecific.h"
#include <link.h>

extern void * dl_base;
extern void * stack_base;
extern unsigned long sentinel;

#define get_esp(dest) \
  asm ( \
           "movl %%esp, %0" \
           : "=r"(dest) \
           : /* no input registers */ \
           : /* no clobbered registers */ \
           )

#define get_ebp(dest) \
  asm ( \
           "movl %%ebp, %0" \
           : "=r"(dest) \
           : /* no input registers */ \
           : /* no clobbered registers */ \
           )

int dumpHeapDLCallback(struct dl_phdr_info *info, size_t size, void * data) {
  Elf32_Addr *addr = static_cast<Elf32_Addr *>(data);
  //fprintf(stderr,"got dl addr 0x%x\n",info->dlpi_addr);
  if(info->dlpi_addr != 0) {
    if(info->dlpi_addr < *addr) {
      *addr = info->dlpi_addr;
      //fprintf(stderr,"new low mark 0x%x\n",*addr);
    }
  }
  return 0;
}

inline unsigned long get_ret_address(int j) {
  /*
  switch(j) {
  case 0:
    return (unsigned long)__builtin_return_address(0);
  case 1:
    return (unsigned long)__builtin_return_address(1);
  default:
    assert(false);
    return 0;
  }
  */
  return (unsigned long)__builtin_return_address(0);
}

#define STACK_DEPTH 3

#define EXTRA_STACK_FRAMES 2
/*
inline unsigned long get_callsite(int j) {
  unsigned long pc = get_ret_address(j);
  printf("%x\n",pc);
  abort();
  // normalize the PC modulo dynamic loading base address
  if(pc >= (unsigned long)dl_base) pc -= (unsigned long)dl_base;
  return pc;
}
*/

inline unsigned long get_callsite(int skip = 2) {
  unsigned long * ebp;
  get_ebp(ebp);
  unsigned long ret = 0;
  
  //fprintf(stderr,"%x\n",ret);

  for(int i = 0; i < skip; i++) {
    ebp = (unsigned long *)*ebp;
    if(!ebp) break;
  }
  
  ret = *(ebp+1);
  if(ret > (unsigned long)dl_base) ret -= (unsigned long)dl_base;

  for(int i = 1; i < STACK_DEPTH; i++) {
    unsigned long pc;
    ebp = (unsigned long *)*ebp;
    if(!ebp) break;
    pc = *(ebp+1);
    if(pc > (unsigned long)dl_base) pc -= (unsigned long)dl_base;
    //fprintf(stderr,"%x\n",pc);
    ret = (ret << 8) ^ pc;
  }

  /*
  unsigned long pc;
  pc = (unsigned long)__builtin_return_address(1);
  if(pc > (unsigned long)dl_base) pc -= (unsigned long)dl_base;
  ret = pc;
  
  fprintf(stderr,"%x\n",pc);
  pc = (unsigned long)__builtin_return_address(2);
  if(pc > (unsigned long)dl_base) pc -= (unsigned long)dl_base;
  ret  = (ret << 8) ^ pc;
  fprintf(stderr,"%x\n",pc);
  pc = (unsigned long)__builtin_return_address(3);
  if(pc > (unsigned long)dl_base) pc -= (unsigned long)dl_base;
  ret  = (ret << 8) ^ pc;
  */

  //fprintf(stderr,"%x\n",ret);
  return ret;
}

extern void * dl_base;

#endif // _CALLSITE_H_
