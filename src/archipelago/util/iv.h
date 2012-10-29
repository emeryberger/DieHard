#ifndef __QIH_IV_H__
#define __QIH_IV_H__

/*

This is an interface to Chris Grzegorczyk's ivhs_feedback kernel module.

This module reports the number of page allocation stalls that have occured 
in the kernel between two (almost) subsequent reads from a special file 
that the module creates in the proc filesystem - /proc/ivhs/feedback.

More precisely, it reports the number of stalls that have occured since the 
read that returned 0 bytes, and a subsequent read.

Module requires a particular routine to be present in the kernel - 
get_full_page_state. It seems that only kernel versions 2.6.16 and 2.6.17 have
it. 2.6.18 doesn't seem to have it. Earlier versions then 2.6.16 may, but
the module was designed for 2.6.16.9.

The module was used in Isla Vista Heap Sizing paper to guide heap size of 
Jikes RMV and allow it to shrink in response two memory pressure.

The module sources originated from http://www.cs.ucsb.edu/~grze/iv

Note: when rebuilding the module for a new kernel, make sure to have the 
original source file in place. Make patches the source file with an address of 
get_full_page_state. It fails to do it correctly to an already patched file.

*/

extern int  start_allocstall_collection(void);

extern int  end_allocstall_collection  (void);
  
extern long get_last_allocstall_count  (void);

//kernel structure representing page state for current cpu.
//compied from include/linux/page-flags.h

struct page_state {
	unsigned long nr_dirty;		/* Dirty writeable pages */
	unsigned long nr_writeback;	/* Pages under writeback */
	unsigned long nr_unstable;	/* NFS unstable pages */
	unsigned long nr_page_table_pages;/* Pages used for pagetables */
	unsigned long nr_mapped;	/* mapped into pagetables.
					 * only modified from process context */
	unsigned long nr_slab;		/* In slab */
	/*
	 * The below are zeroed by get_page_state().  Use get_full_page_state()
	 * to add up all these.
	 */
	unsigned long pgpgin;		/* Disk reads */
	unsigned long pgpgout;		/* Disk writes */
	unsigned long pswpin;		/* swap reads */
	unsigned long pswpout;		/* swap writes */

	unsigned long pgalloc_high;	/* page allocations */
	unsigned long pgalloc_normal;
	unsigned long pgalloc_dma32;
	unsigned long pgalloc_dma;

	unsigned long pgfree;		/* page freeings */
	unsigned long pgactivate;	/* pages moved inactive->active */
	unsigned long pgdeactivate;	/* pages moved active->inactive */

	unsigned long pgfault;		/* faults (major+minor) */
	unsigned long pgmajfault;	/* faults (major only) */

	unsigned long pgrefill_high;	/* inspected in refill_inactive_zone */
	unsigned long pgrefill_normal;
	unsigned long pgrefill_dma32;
	unsigned long pgrefill_dma;

	unsigned long pgsteal_high;	/* total highmem pages reclaimed */
	unsigned long pgsteal_normal;
	unsigned long pgsteal_dma32;
	unsigned long pgsteal_dma;

	unsigned long pgscan_kswapd_high;/* total highmem pages scanned */
	unsigned long pgscan_kswapd_normal;
	unsigned long pgscan_kswapd_dma32;
	unsigned long pgscan_kswapd_dma;

	unsigned long pgscan_direct_high;/* total highmem pages scanned */
	unsigned long pgscan_direct_normal;
	unsigned long pgscan_direct_dma32;
	unsigned long pgscan_direct_dma;

	unsigned long pginodesteal;	/* pages reclaimed via inode freeing */
	unsigned long slabs_scanned;	/* slab objects scanned */
	unsigned long kswapd_steal;	/* pages reclaimed by kswapd */
	unsigned long kswapd_inodesteal;/* reclaimed via kswapd inode freeing */
	unsigned long pageoutrun;	/* kswapd's calls to page reclaim */
	unsigned long allocstall;	/* direct reclaim calls */

	unsigned long pgrotated;	/* pages rotated to tail of the LRU */
	unsigned long nr_bounce;	/* pages for bounce buffers */
};


#endif
