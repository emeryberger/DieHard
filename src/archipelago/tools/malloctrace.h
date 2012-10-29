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

#include "posixrecursivelock.h"


class MallocTracer {

public:

	void * calloc(size_t nmemb, size_t size) {
		MallocTracerLock l(_lock);

		// deny memory allocation from dlsym, it'll use static buffer
		if (_in_dlsym)
			return NULL;

		if (_real_calloc == NULL)
			init_all();

		void * rv = NULL;
		Header * header = (Header *)(*_real_calloc)(nmemb, sizeof(Header) + size);
		if (header != NULL) {
			header->size = size;
			_memEverRequested += size;
			_memCurrRequested += size;
			rv = header + 1;
		}

		trace_printf("%u %u c %p %u %u\n", _memEverRequested, _memCurrRequested, rv, nmemb, size);
		return rv;
	}

	void * malloc(size_t size) {
		MallocTracerLock l(_lock);

		if (_real_malloc == NULL)
			init_all();

		void * rv = NULL;
		Header * header = (Header *)(*_real_malloc)(sizeof(Header) + size);
		if (header != NULL) {
			header->size = size;
			_memEverRequested += size;
			_memCurrRequested += size;
			rv = header + 1;
		}

		trace_printf("%u %u m %p %u\n", _memEverRequested, _memCurrRequested, rv, size);
		return rv;
	}

	void free(void * ptr) {
		MallocTracerLock l(_lock);

		if (_real_free == NULL)
			init_all();

		size_t size = 0;
		if (ptr != NULL) {
			Header * header = (Header *) ptr - 1;
			size = header->size;
			_memCurrRequested -= header->size;
			(*_real_free)(header);
		}

		trace_printf("%u %u f %p %u\n", _memEverRequested, _memCurrRequested, ptr, size);
	}

	void * realloc(void * ptr, size_t size) {
		MallocTracerLock l(_lock);

		if (_real_realloc == NULL)
			init_all();

		void * rv = NULL;
		Header * header = NULL;
		if (ptr != NULL) {
			header = (Header *) ptr - 1;
			_memCurrRequested -= header->size;
		}

		header = (Header *)(*_real_realloc)(header, sizeof(Header) + size);
		if (header != NULL) {
			header->size = size;
			_memEverRequested += size;
			_memCurrRequested += size;
			rv = header + 1;
		}

		trace_printf("%u %u r %p %p %u\n", _memEverRequested, _memCurrRequested, rv, ptr, size);
		return rv;
	}

	inline static MallocTracer * getMallocTracer() {
		static char buf[sizeof(MallocTracer)];
		static MallocTracer * mt = new (buf) MallocTracer;
		return mt;
	}

private:

	struct Header {
		size_t size;
		size_t dummy;
	};

	typedef void * callocType (size_t, size_t);
	typedef void * mallocType (size_t);
	typedef void freeType (void * ptr);
	typedef void * reallocType (void *, size_t);

	typedef HL::Guard<PosixRecursiveLockType> MallocTracerLock;

	PosixRecursiveLockType _lock;

	bool _in_dlsym;

	callocType * _real_calloc;
	mallocType * _real_malloc;
	freeType * _real_free;
	reallocType * _real_realloc;

	size_t _memEverRequested;
	size_t _memCurrRequested;

	static __thread int _trace_fd;

#ifdef DEBUG
	static __thread int _debug_fd;
#endif

	MallocTracer()
		: _lock(),
		  _in_dlsym(false),
		  _real_calloc(NULL),
		  _real_malloc(NULL),
		  _real_free(NULL),
		  _real_realloc(NULL),
		  _memEverRequested(0),
		  _memCurrRequested(0) {
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

	void trace_printf(const char * fmt, ...) {
#if PRINT_TRACE
		if (_trace_fd == 0) {
			char trace_name[50];
			sprintf(trace_name, "malloctrace.%u.%lx", (unsigned int) getpid(), (unsigned long) pthread_self());

			_trace_fd = open(trace_name, O_CREAT | O_TRUNC | O_WRONLY | O_LARGEFILE, S_IRUSR | S_IWUSR);
			if (_trace_fd <= 0) {
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

		write(_trace_fd, buf, offset + n);
#endif	// PRINT_TRACE
	}

	void dbprintf(const char * fmt, ...) {
#ifdef DEBUG

#if DB_PRINT_TO_FILE
		if (_debug_fd == 0) {
			char debug_name[50];
			sprintf(debug_name, "debuglog.%d.%lx", (unsigned int) getpid(), (unsigned long) pthread_self());

			_debug_fd = open(debug_name, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
			if (_debug_fd <= 0)
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
		write(_debug_fd, buf, n);
#else
		fprintf(stderr, "<dbprintf> %s", buf);
		fflush(stderr);
#endif

#endif	// DEBUG
	}

};	// end of class MallocTracer

__thread int MallocTracer::_trace_fd;

#ifdef DEBUG
__thread int MallocTracer::_debug_fd;
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

}

#endif
