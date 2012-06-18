#include "version.h"
#include "platformspecific.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifndef DIEHARD_REPLICATED
#define DIEHARD_REPLICATED 0
#endif

#ifndef DIEHARD_DIEFAST
#define DIEHARD_DIEFAST 1 // Set this for higher security.
#endif

#define DIEHARD_DLL_NAME "C:\\Windows\\System32\\diehard-system.dll"
#define MADCHOOK_DLL_NAME "C:\\Windows\\System32\\madCHook.dll"
#define DIEHARD_GUID "D5DCD74D-EDBB-4e96-B9F1-DECF65E5BF92"
#define DIEHARD_FILENAME "C:\\Program Files\\University of Massachusetts Amherst\\DieHard\\diehard-token-"##DIEHARD_GUID
