#ifdef VMSIG
#include <sys/syscall.h>
#include <unistd.h>

#include "vmcomm.h"

long vm_register(unsigned int flag) {
  return syscall(__NR_vm_register, flag);
}

#ifdef VMCOMM
long vm_relinquish(void **pages, unsigned int n) { 
  return syscall(__NR_vm_relinquish, pages, n);
} 

long vm_getswaprate(int n, int *swap_out, int *swap_in) { 
  return syscall(__NR_vm_getswaprate, n, swap_out, swap_in);
} 
#endif
#endif
