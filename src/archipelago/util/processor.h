#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#define AMD32_256K

#if defined(P4HT)

#include <p4ht.h>

#elif defined(AMD32_256K)

#include <amd32_256k.h>

#endif

#endif
