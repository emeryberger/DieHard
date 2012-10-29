// -*- C++ -*-

#ifndef _MALLOCTRACE_H_
#define _MALLOCTRACE_H_

#include <dlfcn.h>
#include <stdlib.h>

#include <map>
#include <new>

#include "heaplayers.h"
#include "vamcommon.h"

#define PRINT_TRACE			1
#define PRINT_TIMESTAMP		0


// PosixRecursiveLockType: recursive lock
class PosixRecursiveLockType {

public:

	PosixRecursiveLockType() {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
		pthread_mutex_init(&mutex, &attr);
	}
  
	~PosixRecursiveLockType() {
		pthread_mutex_destroy(&mutex);
	}
  
	void lock() {
		pthread_mutex_lock(&mutex);
	}
  
	void unlock() {
		pthread_mutex_unlock(&mutex);
	}
  
private:

	pthread_mutex_t mutex;

};	// end of class PosixRecursiveLockType


struct PageCmp {
	bool operator()(const void * a, const void * b) const {
		return (size_t) a < (size_t) b;
	}
};
class MyMapHeap : public HL::OneHeap<HL::FreelistHeap<HL::StaticHeap<4 * 1024 * 1024> > > {};
typedef pair<const void *, unsigned int> PageStatusPair;
typedef map<const void *, unsigned int, PageCmp, HL::STLAllocator<PageStatusPair, MyMapHeap> > PageStatusMap;


class MallocTracer {

public:

	void * calloc(size_t nmemb, size_t size) {
		MallocTracerLock l(_lock);

		// deny memory allocation from dlsym, it'll use static buffer
		if (_in_dlsym)
			return NULL;

		if (_real_calloc == NULL)
			init_all();

		void * rv = (*_real_calloc)(nmemb, size);
		trace_printf("c %p %u %u\n", rv, nmemb, size);

		return rv;
	}

	void * malloc(size_t size) {
		MallocTracerLock l(_lock);

		if (_real_malloc == NULL)
			init_all();

		void * rv = (*_real_malloc)(size);
		trace_printf("m %p %u\n", rv, size);

		return rv;
	}

	void free(void * ptr) {
		MallocTracerLock l(_lock);

		if (_real_free == NULL)
			init_all();

		(*_real_free)(ptr);
		trace_printf("f %p\n", ptr);
	}

	void * realloc(void * ptr, size_t size) {
		MallocTracerLock l(_lock);

		if (_real_realloc == NULL)
			init_all();

		void * rv = (*_real_realloc)(ptr, size);
		trace_printf("r %p %p %u\n", rv, ptr, size);

		return rv;
	}

	int brk(void * end_data_segment) {
		MallocTracerLock l(_lock);

		if (_real_brk == NULL)
			init_all();

		int rv = (*_real_brk)(end_data_segment);
		dbprintf("brk(%p) returned %d\n", end_data_segment, rv);
		assert(rv == 0);

		if (rv == 0) {
			handleNewBreak((char *) end_data_segment);
		}

		return rv;
	}

	void * sbrk(intptr_t increment) {
		MallocTracerLock l(_lock);

		if (_real_sbrk == NULL)
			init_all();

		void * rv = (*_real_sbrk)(increment);
		dbprintf("sbrk(%d) returned %p\n", increment, rv);
		assert(rv == currentBreak);

		if (rv == currentBreak) {
			handleNewBreak(currentBreak + increment);
		}

		return rv;
	}

	void * mmap(void * start, size_t length, int prot, int flags, int fd, off_t offset) {
		MallocTracerLock l(_lock);

		if (_real_mmap == NULL)
			init_all();

		void * rv = (*_real_mmap)(start, length, prot, flags, fd, offset);
		dbprintf("mmap(%p, %x, %x, %x, %d, %x) returned %p\n", start, length, prot, flags, fd, offset, rv);
		assert(((size_t) rv & ~PAGE_MASK) == 0);

		if (rv != NULL) {
			unsigned int page_status;

			if (flags & MAP_ANONYMOUS) {
				page_status = PAGE_MMAP_ANON;
			}
			else {
				if (flags & MAP_PRIVATE) {
					page_status = PAGE_MMAP_PRIVATE;
				}
				else {
					assert(flags & MAP_SHARED);
					page_status = PAGE_MMAP_SHARED;
				}
			}

			page_status |= PAGE_ON_DEMAND;

			length = (length + PAGE_SIZE - 1) & PAGE_MASK;

			// mprotect the pages to trap future access
			int rc = mprotect(rv, length, PROT_NONE);
			assert(rc == 0);

			size_t mapped = 0;
			while (mapped < length) {
				void * page= (char *) rv + mapped;

				assert(_page_status_map.find(page) == _page_status_map.end());
				_page_status_map[page] = page_status;

				mapped += PAGE_SIZE;
			}

			currentAllocatedMmap += length;
			currentMmapUntouched += length;
			if (currentAllocatedMmap + currentAllocatedSbrk > maxAllocated) {
				maxAllocated = currentAllocatedMmap + currentAllocatedSbrk;
			}
		}

		return rv;
	}

	int munmap(void * start, size_t length) {
		MallocTracerLock l(_lock);

		if (_real_munmap == NULL)
			init_all();

		int rv = (*_real_munmap)(start, length);
		dbprintf("munmap(%p, %x) returned %d\n", start, length, rv);

		if (rv == 0) {
			length = (length + PAGE_SIZE - 1) & PAGE_MASK;

			size_t unmapped = 0;
			while (unmapped < length) {
				void * page = (char *) start + unmapped;

				// find and remove the page in the map
				PageStatusMap::iterator i = _page_status_map.find(page);
				assert(i != _page_status_map.end());

				if (i != _page_status_map.end()) {
					unsigned int page_status = (*i).second;

					switch (page_status & PAGE_MAPPING_MASK) {
					case PAGE_MMAP_ANON:
					case PAGE_MMAP_PRIVATE:
					case PAGE_MMAP_SHARED:
						break;
					default:
						assert(false);
					}

					switch (page_status & PAGE_STATUS_MASK) {
					case PAGE_ON_DEMAND:
						currentMmapUntouched -= PAGE_SIZE;
						assert(currentMmapUntouched >= 0);
						break;
					case PAGE_DISCARDED:
						currentMmapDiscarded -= PAGE_SIZE;
						break;
					case PAGE_IN_MEMORY:
						break;
					default:
						assert(false);
					}

					_page_status_map.erase(i);
				}

				unmapped += PAGE_SIZE;
			}

			currentAllocatedMmap -= length;
		}

		return rv;
	}

	int madvise(void * start, size_t length, int advice) {
		MallocTracerLock l(_lock);

		if (_real_madvise == NULL)
			init_all();

		int rv = (*_real_madvise)(start, length, advice);
		dbprintf("madvise(%p, %x, %d) returned %d\n", start, length, advice, rv);

		if (rv == 0 && advice == MADV_DONTNEED) {
			length = (length + PAGE_SIZE - 1) & PAGE_MASK;

			// mprotect the pages to trap future access
			int rc = mprotect(start, length, PROT_NONE);
			assert(rc == 0);

			size_t advised = 0;
			while (advised < length) {
				void * page = (char *) start + advised;

				// find the page in the map and change its status
				PageStatusMap::iterator i = _page_status_map.find(page);
				assert(i != _page_status_map.end());

				if (i != _page_status_map.end()) {
					unsigned int page_status = (*i).second;

					switch (page_status & PAGE_STATUS_MASK) {
					case PAGE_ON_DEMAND:
					case PAGE_DISCARDED:
						break;
					case PAGE_IN_MEMORY:
						switch (page_status & PAGE_MAPPING_MASK) {
						case PAGE_SBRK_ANON:
							currentSbrkDiscarded += PAGE_SIZE;
							break;
						case PAGE_MMAP_ANON:
						case PAGE_MMAP_PRIVATE:
						case PAGE_MMAP_SHARED:
							currentMmapDiscarded += PAGE_SIZE;
							break;
						default:
							assert(false);
						}
						page_status ^= PAGE_IN_MEMORY | PAGE_DISCARDED;
						break;
					default:
						assert(false);
					}

					(*i).second = page_status;
				}

				advised += PAGE_SIZE;
			}
		}

		return rv;
	}

	inline static MallocTracer * getMallocTracer() {
		static char buf[sizeof(MallocTracer)];
		static MallocTracer * mt = new (buf) MallocTracer;
		return mt;
	}

private:

	enum {
		PAGE_SBRK_ANON		= 0x0001,
		PAGE_MMAP_ANON		= 0x0002,
		PAGE_MMAP_PRIVATE	= 0x0004,
		PAGE_MMAP_SHARED	= 0x0008,
		PAGE_MAPPING_MASK	= 0x000F,

		PAGE_ON_DEMAND		= 0x0010,
		PAGE_DISCARDED		= 0x0020,
		PAGE_IN_MEMORY		= 0x0040,
		PAGE_STATUS_MASK	= 0x00F0,
	};

	typedef void * callocType (size_t, size_t);
	typedef void * mallocType (size_t);
	typedef void freeType (void * ptr);
	typedef void * reallocType (void *, size_t);

	typedef int brkType (void *);
	typedef void * sbrkType (intptr_t);
	typedef void * mmapType (void *, size_t, int, int, int, off_t);
	typedef int munmapType (void *, size_t);
	typedef int madviseType (void *, size_t, int);

	typedef HL::Guard<PosixRecursiveLockType> MallocTracerLock;

	PosixRecursiveLockType _lock;

	bool _in_dlsym;

	callocType * _real_calloc;
	mallocType * _real_malloc;
	freeType * _real_free;
	reallocType * _real_realloc;

	brkType * _real_brk;
	sbrkType * _real_sbrk;
	mmapType * _real_mmap;
	munmapType * _real_munmap;
	madviseType * _real_madvise;

	ssize_t currentAllocatedMmap;
	ssize_t currentAllocatedSbrk;
	ssize_t currentMmapDiscarded;
	ssize_t currentSbrkDiscarded;
	ssize_t currentMmapUntouched;
	ssize_t currentSbrkUntouched;
	ssize_t maxAllocated;
	char * initialBreak;
	char * currentBreak;

	static __thread int trace_fd;

#ifdef DEBUG
	static __thread int debug_fd;
#endif

	PageStatusMap _page_status_map;

	MallocTracer()
		: _lock(),
		  _in_dlsym(false),
		  _real_calloc(NULL),
		  _real_malloc(NULL),
		  _real_free(NULL),
		  _real_realloc(NULL) {
	}

	~MallocTracer() {
		dbprintf("MallocTracer terminating...\n");
	}

	void init_all() {
		_in_dlsym = true;

		_real_calloc = (callocType *) dlsym(RTLD_NEXT, "calloc");
		_real_malloc = (mallocType *) dlsym(RTLD_NEXT, "malloc");
		_real_free = (freeType *) dlsym(RTLD_NEXT, "free");
		_real_realloc = (reallocType *) dlsym(RTLD_NEXT, "realloc");

		_in_dlsym = false;

		assert(_real_calloc != NULL);
		assert(_real_malloc != NULL);
		assert(_real_free != NULL);
		assert(_real_realloc != NULL);

		dbprintf("MallocTracer init ending...\n");
	}

	void addPagesSbrked(size_t start, size_t length) {
		assert((length & ~PAGE_MASK) == 0);

		unsigned int page_status = PAGE_SBRK_ANON | PAGE_ON_DEMAND | PAGE_ORIG_READABLE | PAGE_ORIG_WRITABLE;

		// mprotect the pages to trap future access
		int rc = mprotect((void *) start, length, PROT_NONE);
		assert(rc == 0);

		size_t offset = 0;
		while (offset < length) {
			void * page = (void *) (start + offset);

			assert(_page_status_map.find(page) == _page_status_map.end());
			_page_status_map[page] = page_status;

			offset += PAGE_SIZE;
		}

		currentAllocatedSbrk += length;
		currentSbrkUntouched += length;
		if (currentAllocatedMmap + currentAllocatedSbrk > maxAllocated) {
			maxAllocated = currentAllocatedMmap + currentAllocatedSbrk;
		}
	}

	void removePagesSbrked(size_t start, size_t length) {
		assert((length & ~PAGE_MASK) == 0);

		size_t offset = 0;
		while (offset < length) {
			void * page = (void *) (start + offset);

			// find and remove the page in the map
			PageStatusMap::iterator i = _page_status_map.find(page);
			assert(i != _page_status_map.end());

			if (i != _page_status_map.end()) {
				unsigned int page_status = (*i).second;

				switch (page_status & PAGE_MAPPING_MASK) {
				case PAGE_SBRK_ANON:
					break;
				default:
					assert(false);
				}

				switch (page_status & PAGE_STATUS_MASK) {
				case PAGE_ON_DEMAND:
					currentSbrkUntouched -= PAGE_SIZE;
					assert(currentSbrkUntouched >= 0);
					break;
				case PAGE_DISCARDED:
					currentSbrkDiscarded -= PAGE_SIZE;
					break;
				case PAGE_IN_MEMORY:
					break;
				default:
					assert(false);
				}

				_page_status_map.erase(i);
			}

			offset += PAGE_SIZE;
		}

		currentAllocatedSbrk -= length;

		// remove these in the unprotected window
		removePageRangeUnprotectedWindow(start, length);
	}

	void handleNewBreak(char * newBreak) {
		assert(newBreak >= initialBreak);

		size_t old_page_end = ((size_t) currentBreak + PAGE_SIZE - 1) & PAGE_MASK;
		size_t new_page_end = ((size_t) newBreak + PAGE_SIZE - 1) & PAGE_MASK;
		dbprintf("old_page_end=%p new_page_end=%p\n", old_page_end, new_page_end);

		currentBreak = newBreak;
		assert(currentBreak == (*_real_sbrk)(0));

		if (new_page_end > old_page_end) {
			addPagesSbrked(old_page_end, new_page_end - old_page_end);
		}
		else if (new_page_end < old_page_end) {
			removePagesSbrked(new_page_end, old_page_end - new_page_end);
		}

		currentAllocatedSbrk = currentBreak - initialBreak;
		if (currentAllocatedMmap + currentAllocatedSbrk > maxAllocated) {
			maxAllocated = currentAllocatedMmap + currentAllocatedSbrk;
		}
	}

	void trace_printf(const char * fmt, ...) {
#if PRINT_TRACE
		if (trace_fd == 0) {
			char trace_name[50];
			sprintf(trace_name, "malloctrace.%u.%lx", (unsigned int) getpid(), (unsigned long) pthread_self());

			trace_fd = open(trace_name, O_CREAT | O_TRUNC | O_WRONLY | O_LARGEFILE, S_IRUSR | S_IWUSR);
			if (trace_fd <= 0) {
				assert(false);
				abort();
			}
		}

		char buf[100];
		size_t offset = 0;

#if PRINT_TIMESTAMP
		unsigned long long timestamp;
		unsigned long * ts_low = (unsigned long *) &timestamp;
		unsigned long * ts_hi = ts_low + 1;

		asm volatile ("rdtsc" : "=a"(*ts_low), "=d"(*ts_hi));
		sprintf(buf, "%llx ", timestamp);
		offset = strlen(buf);
#endif

		va_list ap;
		va_start(ap, fmt);

		int n = vsnprintf(buf + offset, 100, fmt, ap);
		if (n < 0 || n >= 100) {
			abort();
		}

		write(trace_fd, buf, offset + n);
#endif	// PRINT_TRACE
	}

	void dbprintf(const char * fmt, ...) {
#ifdef DEBUG

#if DB_PRINT_TO_FILE
		if (debug_fd == 0) {
			char debug_name[50];
			sprintf(debug_name, "debuglog.%d.%lx", (unsigned int) getpid(), (unsigned long) pthread_self());

			debug_fd = open(debug_name, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
			if (debug_fd <= 0)
				abort();
		}
#endif

		char buf[BUF_SZ];
		va_list ap;
		va_start(ap, fmt);

		int n = vsnprintf(buf, BUF_SZ, fmt, ap);
		if (n < 0 || n >= BUF_SZ)
			abort();

#if DB_PRINT_TO_FILE
		write(debug_fd, buf, n);
#else
		fprintf(stderr, "<dbprintf> %s", buf);
		fflush(stderr);
#endif

#endif	// DEBUG
	}

};	// end of class MallocTracer

__thread int MallocTracer::trace_fd;

#ifdef DEBUG
__thread int MallocTracer::debug_fd;
#endif


extern "C" {

	void * calloc(size_t nmemb, size_t size) {
		return MallocTracer::getMallocTracer()->calloc(nmemb, size);
	}

	void * malloc(size_t size) {
		return MallocTracer::getMallocTracer()->malloc(size);
	}

	void free(void * ptr) {
		MallocTracer::getMallocTracer()->free(ptr);
	}

	void * realloc(void * ptr, size_t size) {
		return MallocTracer::getMallocTracer()->realloc(ptr, size);
	}

	int brk(void * end_data_segment) {
		return MallocTracer::getMallocTracer()->brk(end_data_segment);
	}

	void * sbrk(intptr_t increment) {
		return MallocTracer::getMallocTracer()->sbrk(increment);
	}

	void * mmap(void * start, size_t length, int prot, int flags, int fd, off_t offset) {
		return MallocTracer::getMallocTracer()->mmap(start, length, prot, flags, fd, offset);
	}

	int munmap(void * start, size_t length) {
		return MallocTracer::getMallocTracer()->munmap(start, length);
	}

	int madvise(void * start, size_t length, int advice) {
		return MallocTracer::getMallocTracer()->madvise(start, length, advice);
	}
	
}

#endif
