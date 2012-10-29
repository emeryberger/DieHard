#ifndef __DLMALLOC_H__
#define __DLMALLOC_H__

extern "C" {
  void * dlmalloc (size_t);
  void dlfree (void *);
  size_t dlmalloc_usable_size (void *);
}

#endif
