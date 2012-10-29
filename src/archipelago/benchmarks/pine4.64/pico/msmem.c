#ifndef	MSC_MALLOC

#define WIN31
#define STRICT

#include <windows.h>
#include <toolhelp.h>
#include <stdio.h>

#include "mswin.h"





/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                  Memory allocation routines.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 *	The plan is to allocate small blocks in the local heap and
 *	large blocks in the global heap.  The intention is to keep
 *	the number of global allocations to a minimum.
 *
 *	The boundry between small memory and large memory is
 *	controld by the constant SMALL_MEM_BOUNDRY.  Blocks smaller
 *	than SMALL_MEM_BOUNDRY go into the local heap.  This should
 *	be set large enough to capture the majority of small memory
 *	blocks in the local heap.  But if it is too large, the local 
 *	heap will fill up and we will end up allocating a lot of small
 *	blocks in the global heap.
 *
 *	Unfortunatly, pine seems to need a large stack.  With the
 *	stack, some global data, and the heap all cramed in to a 64K
 *	segment we end up with a heap that is smaller than ideal.
 *	This could be improved by reducing the size of the stack, or
 *	by moving the heap to a seperate memory block.  I did a little
 *	work on moving the heap, but was not successful.  My attepts
 *	can be seen in the function MemATest().
 *
 *	Currently (7/8/94) I've set the stack to 32K (in pine/makefile.win),
 *	the heap to 12K, and the small memory boundry to 32 bytes.
 *	Statistics on pine memory allocation suggest that it would be better
 *	to have a 25K local heap and a 95 byte small memory boundry.
 *
 *	Statistics on memory use can be gathered by logging memory debugging
 *	to a file, then running the following awk script:
 *

  # mem.awk
  #
  #	Looks through log and find blocks that were allocated but not
  #	freed uses block id numbers, rather than addresses, since this is
  #	more accurate (and since this is what block ids exist!)
  #
  #	awk may run out of memory if the logfile gets too big.  If this
  #	happens, then another strategy will be needed...
  #

  BEGIN {
	  FS = "[ ()]+";

	  b[0] = 16;
	  b[1] = 32;
	  b[2] = 64;
	  b[3] = 96;
	  b[4] = 128;
	  b[5] = 256;
	  b[6] = 512;
	  b[7] = 1024;
	  b[8] = 2048;
	  b[9] = 4096;
	  b[10] = 9600;
	  b[11] = 20000;
	  b[12] = 40000;
	  b[13] = 80000;
	  b[14] = 200000000;
	  b[15] = 0;
	  bcount = 15;
	  for (i = 0; i < bcount; ++i)
		  c[i] = 0;

	  maxmem = 0;
	  maxsmallmem = 0;

	  allocs = 0;
	  frees = 0;
	  globalallocs = 0;
	  pallocs = 0;
	  pfrees = 0;
  }

  {
	  #print "one", $1, "two", $2, "three", $3, "four ", $4, "five ", $5;
	  if( $1 == "MemAlloc:" ) {
		  m[$5] = $0;

		  ++allocs;
		  if ($9 == 1) ++globalallocs;

		  for (i = 0; i < bcount; ++i) {
			  if (b[i] > $7) {
				  ++c[i];
				  break;
			  }
		  }
	  }
	  else if( $1 == "MemFree:" ) {
		  delete m[$5];
		  ++frees;
	  }
	  if( $1 == "PageAlloc:" ) {
		  p[$5] = $0;

		  ++pallocs;
	  }
	  else if( $1 == "PageFree:" ) {
		  delete p[$5];
		  ++pfrees;
	  }
	  else if ($1 == "Memory") {
		  if ($6 > maxmem) maxmem = $6;
	  }
	  else if ($1 == "Small") {
		  if ($7 > maxsmallmem) maxsmallmem = $7;
	  }
  }


  END {
	  for( i in m ) {
		  print m[i]
	  }
	  for (i in p) {
		  print p[i]
	  }

	  cumulative = 0;
	  for (i = 0; i < bcount; ++i) {
		  cumulative += c[i];
		  printf "%9d : %5d  (%5d)\n", b[i], c[i], cumulative;
	  }

	  print;
	  print "Max memory use:        ", maxmem;
	  print "Max small memory use:  ", maxsmallmem;
	  print;
	  print "Local allocations   ", allocs - globalallocs;
	  print "Global allocations  ", globalallocs;
	  print "Total allocations   ", allocs;
	  print "Total memory frees  ", frees;
	  print "Blocks not freed    ", allocs - frees;
	  print;
	  print "Page allocations    ", pallocs;
	  print "Page frees          ", pfrees;
	  print "Pages not freed     ", pallocs - pfrees;
  }
 
 *
 *	Each memory block is assigned a unique id.  This is only used
 *	to match allocations with frees in the debug log.
 */



/*
 * SEGHEAP selectes between two different implementations of the memory 
 * management functions.  Defined and it uses a sub allocation scheme.
 * Undefined and it comples a direct allocation scheme.
 *
 * The sub allocation scheme is greatly prefered because it greatly reduces
 * the number of global memory allocations.
 */
#define SEGHEAP		/* Use the sub allocation scheme. */




#define  MEM_DEBUG			/* Compile in memory debugging code.*/
#define MEM_DEBUG_LEVEL		8	/* Pine debug level at which memory
					 * debug messages will be generated.*/
#ifdef MEM_DEBUG
static int		MemDebugLevel = 0;	/* Doing debugging. */
static FILE		*MemDebugFile = NULL;	/* File to write to. */
#endif



#ifdef DEBUG
#define LOCAL
#else
#define LOCAL		static
#endif


#define GET_SEGMENT(x)		(0xffff & ((DWORD)(x) >> 16))
#define GET_OFFSET(x)		(0xffff & (DWORD)(x))


#undef malloc
#undef realloc
#undef free


void		MemATest (void);





/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *	Standard memory allcation functions.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


void *
malloc (size_t size)
{
	return (MemAlloc (size));
}


void __far *
_fmalloc (size_t size)
{
	return (MemAlloc (size));
}


void __far *
realloc (void *memblock, size_t size)
{
	return (MemRealloc (memblock, size));
}


void __far *
_frealloc (void *memblock, size_t size)
{
	return (MemRealloc (memblock, size));
}


void 
free (void *memblock)
{
	MemFree (memblock);
}

void 
_ffree (void *memblock)
{
	MemFree (memblock);
}


/*
 * Turn on memory debugging and specify file to write to.
 */
void
MemDebug (int debug, FILE *debugFile)
{
#ifdef MEM_DEBUG
    if (debugFile == NULL) {
	MemDebugLevel = 0;
	MemDebugFile = NULL;
    }
    else {
	MemDebugLevel = debug;
	MemDebugFile = debugFile;
	fprintf (MemDebugFile, "Memory Debuging set on\n");
    }
#endif /* MEM_DEBUG */
}







#ifdef SEGHEAP

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                  SEGHEAP Memory allocation routines.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 * This implementation allocates memory in pages then sub alloates from the
 * pages.  This greatly reduces the number of global memory blocks allocated.
 * This code originally written by Stephen Chung and posted to a Usenet news
 * group.  I've modified them for use in Pine.  The author says:
 *
 * 
 * Copyright (C) Stephen Chung, 1991-1992.  All rights reserved. 
 *
 * Afterwords
 * ----------
 * 
 * Theoretically, you are required to obtain special approval from me (because
 * I copyrighted these routines) if you want to use them in your programs.
 * However, I usually don't really care if you are not using these routines in
 * a commercial, shareware etc. product.
 * 
 * Any questions and/or bug fixes, please send email to:
 * 
 *         Stephen Chung           stephenc@cunixf.cc.columbia.edu
 * 
 * If it bounces, then try schung@cogsci.Berkeley.EDU
 * 
 * Have fun!
 * 
 */


/*
 * The folloing control debugging code for testing out of memory conditions.
 * there are to test code.
 *
 * MEM_ALLOC_LIMT 
 *	Setting this to anything other than zero will limit memory allocation
 *	to (rougly) that many bytes.
 *
 * MEM_FAIL_SOON
 *	Compiles in a function which will cause the memory allocation to fail
 *	soon after that function is called.  this can be used to test
 *	mem alloc failurs in specific code segments.
 */
#define MEM_ALLOC_LIMIT		0
#define MEM_FAIL_SOON		0


#if MEM_FAIL_SOON
static long MemFailSoonLimit = 0;
#endif



#define MAGIC           0x42022667
#define MAGIC2		0x56743296

typedef struct MemoryStruct {
    long int		magic;
    void far		*page;
    WORD		id;
    MemSize		size;
    BOOL		allocated;
    struct MemoryStruct far *next, far *prev;
    long int		magic2;
} MEMHEADER;

typedef struct PageHeaderStruct {
    long int		magic;
    HANDLE		handle;
    WORD		id;
    MemSize		size;
    MemSize		used;
    MemSize		overhead;
    MEMHEADER far	*data, far *empty;
    struct PageHeaderStruct far *next, far *prev;
    long int		magic2;
} MEMPAGEHEADER;

typedef struct {
    MEMPAGEHEADER far *pages;
    int nr_pages;
} MAINMEMHEADER;

#define PAGESIZE        (6 * 1024)
#define USEABLESIZE     (PAGESIZE - sizeof(MEMPAGEHEADER) - sizeof(MEMHEADER))


LOCAL MAINMEMHEADER	MemHeader = { NULL, 0 };
LOCAL WORD		MemID = 0;		/* Keep track of ID. */
LOCAL WORD		PageID = 0;
LOCAL unsigned long	MemInUse = 0;		/* Total bytes in use. */
LOCAL unsigned long	MemInUseMax = 0;	/* max in use at one time. */
LOCAL unsigned long     PageMemInUse = 0;
LOCAL unsigned long	PageMemInUseMax = 0;



static MEMPAGEHEADER far *
AddPage(MemSize n)
{
    void far *cp;
    MEMHEADER far *mp;
    MEMPAGEHEADER far *p;
    HANDLE handle = NULL;
    
    
#if MEM_ALLOC_LIMIT
    if (n + PageMemInUse > MEM_ALLOC_LIMIT) {
	MessageBox (NULL, "PageAlloc:  Above preset limit, allocation fails", 
		"Out Of Memory", MB_ICONSTOP | MB_OK);
	return (NULL);
    }
#endif

    handle = GlobalAlloc(GHND, n);
    if (handle == NULL) {
#ifdef MEM_DEBUG
	if (MemDebugLevel >= 1) 
	    fprintf (MemDebugFile, "***\n*** Out of memory: allocating %d bytes\n***\n", n);
#endif
        return (NULL);
    }

    if (MemHeader.pages == NULL || MemHeader.nr_pages <= 0) {
        p = MemHeader.pages = (MEMPAGEHEADER far *) GlobalLock(handle);
        p->prev = NULL;
    } else {
        for (p = MemHeader.pages; p->next != NULL; p = p->next);
        p->next = (MEMPAGEHEADER far *) GlobalLock(handle);
        p->next->prev = p;
        p = p->next;
    }

    p->magic = MAGIC;
    p->handle = handle;
    p->next = NULL;
    p->id = PageID++;
    p->size = n;
    p->used = 0;
    p->overhead = sizeof(MEMPAGEHEADER) + sizeof(MEMHEADER);
    p->magic2 = MAGIC2;

    cp = ((char far *) p) + sizeof(MEMPAGEHEADER);
    mp = (MEMHEADER far *) cp;

    p->data = p->empty = mp;

    mp->magic = 0L;
    mp->magic2 = 0L;
    mp->allocated = FALSE;
    mp->page = p;
    mp->size = p->size - p->overhead;
    mp->next = mp->prev = NULL;

    MemHeader.nr_pages++;
    

#ifdef MEM_DEBUG
    if (MemDebugLevel >= MEM_DEBUG_LEVEL) {
	fprintf (MemDebugFile, "PageAlloc: addr(%lx) id(%u) size(%ld) global(%d)\n",
		p, p->id, (long)n, 1);
	fflush (MemDebugFile);
    }
#endif /* MEM_DEBUG */

    PageMemInUse += n;
    if (PageMemInUse > PageMemInUseMax)
	PageMemInUseMax = PageMemInUse;
    

    return (p);
}



static void 
DeletePage (MEMPAGEHEADER far *p)
{
#ifdef MEM_DEBUG    
    /* Deubgging info... */
    if (MemDebugLevel >= 4) {
	if (PageMemInUse == PageMemInUseMax) 
	   fprintf (MemDebugFile, "Page usage is up to %lu\n", PageMemInUseMax);
    }
    if (MemDebugLevel >= MEM_DEBUG_LEVEL) {
	fprintf (MemDebugFile, "PageFree: addr(%lx) id(%u)\n", 
		p, p->id);
	fflush (MemDebugFile);
    }
#endif /* MEM_DEBUG */

    PageMemInUse -= p->size;


    if (p->next == NULL && p->prev == NULL) {
        MemHeader.pages = NULL;
        MemHeader.nr_pages = 0;
	GlobalUnlock (p->handle);
        GlobalFree (p->handle);
    } else {
        if (p == MemHeader.pages) MemHeader.pages = p->next;
        MemHeader.nr_pages--;

        if (p->prev != NULL) p->prev->next = p->next;
        if (p->next != NULL) p->next->prev = p->prev;

	GlobalUnlock (p->handle);
        GlobalFree (p->handle);
    }
}


/* 
 * Segmented heap memory allocation. 
 */

MemPtr
_MemAlloc (MemSize n, char __far * file, int line)
{
    MEMPAGEHEADER far *p;
    MEMHEADER far *mp;
    char far *cp;

    if (n >= 65535) {
	assert (n < 65535);
	goto AllocFail;
    }
    
#if MEM_FAIL_SOON
    if (MemFailSoonLimit != 0 && MemFailSoonLimit < MemInUse + n) {
	MessageBox (NULL, "MemAlloc:  Told to fail here, allocation fails", 
		"Out Of Memory", MB_ICONSTOP | MB_OK);
	return (NULL);
    }
#endif

    /* Larger than page size? */

    if (n > USEABLESIZE) {
	p = AddPage(n + sizeof(MEMPAGEHEADER) + sizeof(MEMHEADER));
	if (p == NULL)
	    goto AllocFail;

        mp = p->data;
        mp->magic = MAGIC;
	mp->magic2 = MAGIC2;
	mp->id = MemID++;
        mp->allocated = TRUE;
	

        p->used = n;
        p->empty = NULL;

        cp = ((char far *) mp) + sizeof(MEMHEADER);
#ifdef MEM_DEBUG
	if (MemDebugLevel >= MEM_DEBUG_LEVEL) {
	    fprintf (MemDebugFile, "MemAlloc: addr(%lx) id(%u) size(%ld) global(%d)\n",
		    cp, mp->id, (long)n, 0);
	    fflush (MemDebugFile);
	}
#endif /* MEM_DEBUG */

	MemInUse += n;
	if (MemInUse > MemInUseMax)
	    MemInUseMax = MemInUse;
	return ((MemPtr) cp);
    }


    /* Search for the hole */

    for (p = MemHeader.pages; p != NULL; p = p->next) {
        /* Scan the chains */
        if (p->size - p->used - p->overhead <= 0) continue;
		if (p->empty == NULL) continue;

        for (mp = p->empty; mp != NULL; mp = mp->next) {
            if (!mp->allocated && mp->size >= n) break;
        }

        if (mp != NULL) break;
    }

    /* New page needed? */

    if (p == NULL) {
        p = AddPage(PAGESIZE);
	if (p == NULL)
	    goto AllocFail;
	mp = p->data;
    }

    /* Do we need to break it up? */

    if (mp->size - n > sizeof(MEMHEADER)) {
        MEMHEADER far *mp2;

        cp = ((char far *) mp) + n + sizeof(MEMHEADER);
        mp2 = (MEMHEADER far *) cp;

        mp2->magic = 0L;
	mp2->magic2 = 0L;
        mp2->allocated = FALSE;
        mp2->page = p;
        mp2->size = mp->size - n - sizeof(MEMHEADER);

        mp2->next = mp->next;
        mp2->prev = mp;
        if (mp->next != NULL) mp->next->prev = mp2;
        mp->next = mp2;


        p->overhead += sizeof(MEMHEADER);

        mp->size = n;
    }

    mp->magic = MAGIC;
    mp->magic2 = MAGIC2;
    mp->allocated = TRUE;
    mp->id = MemID++;

    p->used += n;
    cp = ((char far *) mp) + sizeof(MEMHEADER);


    /* Debugging info... */
#ifdef MEM_DEBUG
    if (MemDebugLevel >= MEM_DEBUG_LEVEL) {
	fprintf (MemDebugFile, "MemAlloc: addr(%lx) id(%u) size(%ld) global(%d)\n",
		cp, mp->id, (long)n, 0);
	fflush (MemDebugFile);
    }
#endif /* MEM_DEBUG */

    MemInUse += n;
    if (MemInUse > MemInUseMax)
	MemInUseMax = MemInUse;
    
    
    /* Search for the next empty hole */

    for (; mp != NULL; mp = mp->next) {
        if (!mp->allocated && mp->size > 0) break;
    }

    p->empty = mp;

    return ((MemPtr) cp);
    
    
AllocFail:
#if 0
    assert (FALSE /* Memory allocation failed! */);*/
#endif
    return (NULL);
}



/*
 * Segmented heap memory free.
 */
int
_MemFree (MemPtr vp, char __far *file, int line)
{
    MEMPAGEHEADER far *p;
    MEMHEADER far *mp, far *mp2;
    char far *cp;
    
    if (vp == NULL)
        return (0);
    

    cp = ((char far *) vp) - sizeof(MEMHEADER);
    mp = (MEMHEADER far *) cp;

    if (mp->magic != MAGIC || mp->magic2 != MAGIC2|| !mp->allocated) {
	assert (mp->magic == MAGIC);
	assert (mp->magic2 == MAGIC2);
	assert (mp->allocated);
	return (-1);
    }
    
#ifdef MEM_DEBUG    
    /* Deubgging info... */
    if (MemDebugLevel >= 4) {
	if (MemInUse == MemInUseMax) 
	   fprintf (MemDebugFile, "Memory usage is up to %lu\n", MemInUseMax);
    }
    if (MemDebugLevel >= MEM_DEBUG_LEVEL) {
	fprintf (MemDebugFile, "MemFree: addr(%lx) id(%u)\n", vp, mp->id);
	fflush (MemDebugFile);
    }
#endif /* MEM_DEBUG */

    MemInUse -= mp->size;


    p = (MEMPAGEHEADER far *) mp->page;
    p->used -= mp->size;

    mp->magic = 0L;
    mp->magic2 = 0L;
    mp->allocated = FALSE;

    /* Merge? */

    mp2 = mp->prev;

    if (mp2 != NULL && !mp2->allocated) {
        mp2->next = mp->next;
        if (mp->next != NULL) mp->next->prev = mp2;
        mp2->size += mp->size + sizeof(MEMHEADER);

        p->overhead -= sizeof(MEMHEADER);

        mp = mp2;
    }

    mp2 = mp->next;

    if (mp2 != NULL && !mp2->allocated) {
        mp->next = mp2->next;
        if (mp2->next != NULL) 
	    mp2->next->prev = mp;

        mp->size += mp2->size + sizeof(MEMHEADER);

	p->overhead -= sizeof(MEMHEADER);
    }

    if (mp->prev == NULL && mp->next == NULL) {
        DeletePage(p);
    } else {
        if (p->empty == NULL || mp < p->empty) p->empty = mp;
    }
    return (0);
}



MemPtr
_MemRealloc (MemPtr p, MemSize n, char __far *file, int line)
{
    MEMHEADER far *mp;
    char far *cp;

    if (p != NULL) {
	/* Block already large enough? */
	cp = ((char far *) p) - sizeof(MEMHEADER);
	mp = (MEMHEADER far *) cp;

	if (mp->magic != MAGIC || mp->magic2 != MAGIC2) {
	    assert (mp->magic == MAGIC);
	    assert (mp->magic2 == MAGIC2);
	    return (p);
	}

	if (mp->size >= n) return (p);      /* No need to do anything */
    }
    /* Else swap to another block */

    cp = MemAlloc (n);
    if (cp == NULL)
	return (NULL);

    if (p != NULL) {
	_fmemcpy(cp, p, (size_t)((mp->size >= n) ? n : mp->size));
	MemFree (p);
    }

    return ((void far *) cp);
}



void
MemFailSoon (MemSize n)
{
#if MEM_FAIL_SOON
    MemFailSoonLimit = MemInUse + n;
#ifdef MEM_DEBUG
    if (MemDebugLevel >= 1) {
	fprintf (MemDebugFile,
	   "MemFailSoon:  Fail when allocation increases by %ld (Max %ld)\n",
		n, MemFailSoonLimit);
    }
#endif
#endif
}
	    




MemSize
MemBlkSize (MemPtr p)
{
    MEMHEADER far *mp;
    char far *cp;

    if (p == NULL) return (0);
    cp = ((char far *) p) - sizeof(MEMHEADER);

    mp = (MEMHEADER far *) cp;

    if (mp->magic != MAGIC || mp->magic2 != MAGIC2) {
	assert (mp->magic == MAGIC);
	assert (mp->magic2 == MAGIC2);
	return (0);
    }

    return (mp->size);
}


#if 0
MemPtr
MemDup (void far *p)
{
    unsigned int len;
    void far *p1;

    len = MgetBlkSize (p);
    p1 = MemAlloc (len);
    if (p1 != NULL)
	_fmemcpy(p1, p, len);

    return (p1);
}


void 
MemoryStatistics (long int *allocated, long int *used, long int *overhead)
{
    MEMPAGEHEADER far *p;

    *allocated = *used = *overhead = 0L;

    for (p = MemHeader.pages; p != NULL; p = p->next) {
        *allocated += p->size;
        *used += p->used;
        *overhead += p->overhead;
    }
}
#endif


void 
MemFreeAll (void)
{
    MEMPAGEHEADER far *p, far *p1;

    for (p = MemHeader.pages; p != NULL; ) {
	p1 = p->next;
	GlobalUnlock (p->handle);
	GlobalFree (p->handle);
	p = p1;
    }

    MemHeader.pages = NULL;
    MemHeader.nr_pages = 0;
}


#ifdef MEM_DEBUG

/* For debugging purposes...  not very pretty */

void PrintMemoryChains(void)
{
    MEMPAGEHEADER far *p;
    MEMHEADER far *mp;
    char far *cp;
    char buffer[100];

    /* Block already large enough? */


    for (p = MemHeader.pages; p != NULL; p = p->next) {
        for (mp = p->data; mp != NULL; mp = mp->next) {
            fprintf (MemDebugFile, "%Fp | %u | %s", mp, mp->size, mp->allocated ? "Alloc" : "Free");
        }
    }
}

#endif /* DEBUG */



#else		/* !SEGHEAP.  Old version, not used. */







/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                  Direct memory allocation
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 * This following implementation allocates large memory blocks directly 
 * from global memory and small memory blocks from the local heap.  The
 * problem with this method is that pine's local heap is quite small
 * so most of the memory ends up comming from the global heap.
 */


#define GUARD_LOW0		0xbbbb
#define GUARD_LOW		0x9999
#define GUARD_HIGH		0xaaaaaaaa

#define SMALL_MEM_BOUNDRY	32

#define HEAP_SIZE		32000


/* Memory block header.  Placed at beginning of each allocated block. */
typedef struct {			/*size  len */
    WORD		guard0;		/* 00 - 02 */
    HGLOBAL		handle;		/* 02 - 02 */
    short		globalBlk;	/* 04 - 02 */
    MemSize		size;		/* 06 - 04 */
    WORD		id;		/* 0A - 02 */
    WORD		guard1;		/* 0C - 02 */
} MemBlk;				/* Total size:  0E */

typedef MemBlk __far *	MemBlkPtr;


/* Memory high guard tailer.  Placed at end of each allocated block. */
typedef struct {
    unsigned long	guard1;
} MemHighGuard;

typedef MemHighGuard __far *MemHighGuardPtr;


/*
 * Memory allocation globals. 
 */
LOCAL WORD		MemID = 0;		/* Keep track of ID. */
LOCAL unsigned long	MemLocalFails = 0;
LOCAL BOOL		MemLocalFull = FALSE;	/* True when local heap full*/
#ifdef MEM_DEBUG
LOCAL unsigned long	MemInUse = 0;		/* Total bytes in use. */
LOCAL unsigned long	MemInUseMax = 0;	/* max in use at one time. */
LOCAL unsigned long	SmallMemInUse = 0;
LOCAL unsigned long	SmallMemInUseMax = 0;
#endif /* MEM_DEBUG */



/*
 * Allocate a memory block.
 * The file and line indicate where we are called from (for debugging)
 * but in pine these mostly point to a bottel neck routine and are
 * useless.
 */
MemPtr
_MemAlloc (MemSize size, char __far * file, int line)
{
    MemBlkPtr			pBlk;
    MemHighGuardPtr		pHG;
    HGLOBAL			hBlk;
    HLOCAL			hLBlk;
    UINT			totalSize;
    BYTE		__far *	pb;

    assert (size <= MEM_BLOCK_SIZE_MAX);
    

    /*
     * Calculate total size we need to allocate.
     */
    totalSize = (UINT)size + sizeof (MemBlk) + sizeof (MemHighGuard);
    
    
    pBlk = NULL;
    
    /*
     * If it's a small block and the local heap is not full, try 
     * allocating from the local heap. 
     */
    if (size <= SMALL_MEM_BOUNDRY && !MemLocalFull) {
	    
	/* Allocate block from local storage. */
	hLBlk = LocalAlloc (LMEM_MOVEABLE, totalSize);
	if (hLBlk != NULL) {

	    /* Lock block and dereference. */
	    pBlk = (MemBlkPtr) LocalLock (hLBlk);
	    if (pBlk != NULL) {
		pBlk->handle = hLBlk;
		pBlk->globalBlk = FALSE;
	    }
	    else 
	        LocalFree (hLBlk);
	}
	else {
	    ++MemLocalFails;
	    MemLocalFull = TRUE;
#ifdef MEM_DEBUG
	    if (MemDebugLevel >= MEM_DEBUG_LEVEL)
		fprintf (MemDebugFile, "Local Memory alloc failed, %lu fails, %lu bytes in use\n",
			MemLocalFails, SmallMemInUse);
#endif
	}
    }

    
    /* 
     * If it is a large block, or local alloc failed, we allocate from
     * global space. 
     */
    if (pBlk == NULL) {
	    
	/* Allocate block from global storage. */
	hBlk = GlobalAlloc (GMEM_MOVEABLE, totalSize);
	if (hBlk == NULL) 
	    return (NULL);


	/* Lock block and dereference. */
	pBlk = (MemBlkPtr) GlobalLock (hBlk);
	if (pBlk == NULL) {
	    GlobalFree (hBlk);
	    return (NULL);
	}
	pBlk->handle = hBlk;
	pBlk->globalBlk = TRUE;
    }


    
    /* Fill rest of header. */
    pBlk->guard0 = GUARD_LOW0;
    pBlk->size = size;
    pBlk->id = ++MemID;
    pBlk->guard1 = GUARD_LOW;

    
    /* Find address that will be returned to caller. */
    pb = (BYTE __far *) (pBlk + 1);

    
    /* Find high guard and fill. */
    pHG = (MemHighGuardPtr) (pb + size);
    pHG->guard1 = GUARD_HIGH;
    

    /* Debugging info... */
#ifdef MEM_DEBUG
    if (MemDebugLevel >= MEM_DEBUG_LEVEL) {
	if( !file ) file = "??";
	fprintf (MemDebugFile, "MemAlloc: addr(%lx) id(%u) size(%ld) global(%d)\n",
		pb, pBlk->id, (long)size, pBlk->globalBlk);
	fflush (MemDebugFile);
    }
    MemInUse += totalSize;
    if (MemInUse > MemInUseMax)
	MemInUseMax = MemInUse;
    if (size <= SMALL_MEM_BOUNDRY) {
	SmallMemInUse += totalSize;
	if (SmallMemInUse > SmallMemInUseMax)
	    SmallMemInUseMax = SmallMemInUse;
    }
#endif /* MEM_DEBUG */
	

    return ((MemPtr) pb);
}




/*
 * Free a block.
 */
int
_MemFree (MemPtr block, char __far *file, int line)
{
    MemBlkPtr		pBlk;
    MemHighGuardPtr	pHG;
    HGLOBAL		hBlk;
    HLOCAL		hLBlk;
    BOOL		brc;
    UINT		totalSize;
    
    if (block == NULL)
        return (0);


    /* Find header and high guard and check them. */
    pBlk = ((MemBlkPtr)block) - 1;
    pHG = (MemHighGuardPtr) ((char __far *)block + pBlk->size);
    
    totalSize = pBlk->size + sizeof (MemBlk) + sizeof (MemHighGuard);

    /* If these changed them someone wrote where the should not have. */
    assert (pBlk->guard0 == GUARD_LOW0);
    assert (pBlk->guard1 == GUARD_LOW);
    assert (pHG->guard1 == GUARD_HIGH);


    
#ifdef MEM_DEBUG    
    /* Deubgging info... */
    if (MemDebugLevel >= MEM_DEBUG_LEVEL) {
	if (pBlk->size <= SMALL_MEM_BOUNDRY && 
		SmallMemInUse == SmallMemInUseMax) 
	   fprintf (MemDebugFile, "Small memory usage is up to %lu\n", SmallMemInUseMax);
	if (MemInUse == MemInUseMax) 
	   fprintf (MemDebugFile, "Memory usage is up to %lu\n", MemInUseMax);
    }
    MemInUse -= totalSize;
    if (pBlk->size <= SMALL_MEM_BOUNDRY)
        SmallMemInUse -= totalSize;
    if (MemDebugLevel >= MEM_DEBUG_LEVEL) {
	fprintf (MemDebugFile, "MemFree: addr(%lx) id(%u)\n", 
		block, pBlk->id);
	fflush (MemDebugFile);
    }
#endif /* MEM_DEBUG */



    /*
     * Header indicates which block it came from
     */
    if (!pBlk->globalBlk) {
	/* Unlock block */
	hLBlk = pBlk->handle;
	brc = LocalUnlock (hLBlk);
	assert (!brc);

	/* And free block. */
	hLBlk = LocalFree (hLBlk);
	assert (hLBlk == NULL);
	MemLocalFull = FALSE;
    }
    else {
	/* Unlock block */
	hBlk = pBlk->handle;
	brc = GlobalUnlock (hBlk);
	assert (!brc);

	/* And free block. */
	hBlk = GlobalFree (hBlk);
	assert (hBlk == NULL);
    }
    return (0);
}




/*
 * Reallocate a memory block.  Simplistic approach.
 */
MemPtr
_MemRealloc (MemPtr block, MemSize size, char __far * file, int line)
{
    MemPtr	newBlock;
    
    
    newBlock = MemAlloc (size);
    if (newBlock == NULL)
	return (NULL);

    if (block != NULL) {
	_fmemcpy (newBlock, block , (size_t)MIN (size, MemBlkSize (block)));
	MemFree (block);
    }
	
    return (newBlock);
}
	
	

/*
 * Return the size of a memory block
 */
MemSize
MemBlkSize (MemPtr block)
{
    MemBlkPtr			pBlk;

    if (block == NULL) return (0);
    pBlk = ((MemBlkPtr)block) - 1;
    assert (pBlk->guard1 == GUARD_LOW);

    return (pBlk->size);
}


#ifdef MEM_DEBUG
struct testblock {
    struct testblock	__far * prev;
    HLOCAL			h;
};



void
MemATest (void)
{
    void    __near		*n;
    struct testblock __far	*p;
    struct testblock __far	*pnew;
    HLOCAL			hl;
    int				bcount;
    UINT			segment, start, end;
    void    __far		*f;
    HGLOBAL			hg;
    DWORD			dw;
    LOCALINFO			li;
    UINT			DataSeg;
    
#if 0  
    hg = GlobalAlloc (GMEM_FIXED, HEAP_SIZE);	/* Allocate global block */
    if (hg == NULL)
	return;
    f = GlobalLock (hg);			/* Lock and get pointer. */
    if (f == NULL) 
	goto Fail1;
    segment = (UINT) GET_SEGMENT (f);		/* Get segment and offsets. */
    start = (UINT) GET_OFFSET (f);
    end = start + HEAP_SIZE - 1;
    start += 16;
    if (!LocalInit (segment, start, end))		/* Init it as the local heap*/
	goto Fail2;
#endif
#if 0
    __asm MOV DataSeg,DS;			/* Get current DS. */
    __asm MOV DS,segment;			/* Set DS to new heap. */
    hl = LocalAlloc (0, SMALL_MEM_BOUNDRY);	/* Now allocate something. */
    __asm MOV DS,DataSeg;			/* Restore DS. */
    if (hl == NULL) 
	    return;
    n = LocalLock (hl);				/* Find where it is. */
    f = (void __far *)n;
    segment = GET_SEGMENT(f);			/* What Segment. */
    dw = GlobalHandle (segment);
    hg = (HGLOBAL) (dw & 0xffff);
    if (hg == NULL)
	    return;
    
    li.dwSize = sizeof (li);			/* What size. */
    if (!LocalInfo (&li, hg))
	    return;
    
    dw = GlobalSize (hg);
    f = GlobalLock (hg);
    GlobalUnlock (hg);
    
    LocalUnlock (hl);
    LocalFree (hl);

    
#endif
    
    
	
	
    p = NULL;
    pnew = NULL;
    bcount = 0;
	
    do {
	hl = LocalAlloc (0, SMALL_MEM_BOUNDRY);
	if (hl != NULL) {
	    ++bcount;
	    n = LocalLock (hl);
	    pnew = (struct testblock __far*) n;
	    pnew->h = hl;
	    pnew->prev = p;
	    p = pnew;
        }
    } while (hl != NULL);
    
    
    if (MemDebugFile != NULL)
	fprintf (MemDebugFile, "Allocated %d blocks of size %d\n",
	    bcount, SMALL_MEM_BOUNDRY);
			
    while (p != NULL) {
	pnew = p->prev;
	hl = p->h;
	LocalUnlock (hl);
	LocalFree (hl);
	p = pnew;
    }
    fflush (MemDebugFile);
#if 0 
Fail2:	GlobalUnlock (hg);
Fail1:	GlobalFree (hg);
#endif
	return;
}
#endif /* MEM_DEBUG */

#endif /* ifdef SEGHEAP */

#endif	/* MSC_MALLOC */
