/*
 * Userspace header file for Eric(Yi) Feng's VMCOMM and VMSig kernel patches
 * These kernel patches allow user app to register to receive events when their
 * pages are being swapped in and out. User app has to set up a signal handler
 * for the signal below, and then call vm_register to indicate which events
 * it is interested in.
 *
 * Vitaliy Lvin <vlvin@cs.umass.edu>, April 2007
 */

#if !defined(VMCOMM_H) && defined(VMSIG)
#define VMCOMM_H

/*
 * New signal that is raised on VM events. 
 * Has to be used in conjection with vm_register (see below)
 */

#define SIGVM           35

/*
 * Flags used in conjunction with vm_register 
 * and returned in the SIGVM signal handler
 *
 * Older VMCOMM patch does not make a distinction
 * between dirty and clean pages being evicted
 * and between faulted in and prefetched pages
 */

#ifdef VMCOMM
#define VM_EVICTING_CLEAN   0x1
#define VM_EVICTING_DIRTY   0x1
#define VM_SWAPPED_OUT      0x2
#define VM_FAULTED_IN       0x4
#define VM_PREFETCHED       0x4
#else //VMSIG
#define VM_EVICTING_CLEAN	0x1
#define VM_EVICTING_DIRTY	0x2
#define VM_SWAPPED_OUT	0x4
#define VM_FAULTED_IN	0x8
#define VM_PREFETCHED	0x10
#endif


/*
 * Used to indicate that the current process wishes to receive SIGVM signals
 */

extern long vm_register(unsigned int flag); 


#ifdef VMCOMM //the following are patch-specific

/*
 * Used to return pages to the OS. 
 * Implements both MADV_DONTNEED and MADV_FREE POSIX semantics 
 * Takes an array of pointers to pages to be returned to the OS
 * and the size of the array. The size of the array has to be
 * less then VM_MAX_RELINQUISH (currently this means array has to
 * fit on a page, but it may change in the future).
 * The low-order bit of the page address indicates whether it can be
 * discarded without writing to disc or not (1 means yes, 0 means no).
 * It corresponds to POSIX MADV_FREE and MADV_DONTNEED respectively.
 */

#define VM_MAX_RELINQUISH 1024

extern long vm_relinquish(void **pages, unsigned int n);

/*
 * From mm/vmcomm.c:
 * Returns numbers of swap out's and in's during the last 
 * SWAP_HIST seconds in arrays provided by user 
 * SWAP_HIST is currently set to 5
 */
 
extern long vm_getswaprate(int n, int *swap_out, int *swap_in);

#else //VMSIG

/* 
 * A new advice flag for madvise() that lets the kernel swap out
 * the specified pages under memory pressure. If these pages don't
 * contain useful data, the user should use madvise() with the
 * MADV_DONTNEED flag.
 *
 * VM_RELINQUISH implements POSIX MADV_DONTNEED semantics
 */

#define MADV_RELINQUISH		0x5

#endif

#endif
