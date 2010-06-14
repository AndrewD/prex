
#if defined(__arm__)
#include "arm/memory.h"
#elif defined (__x86__)
#include "x86/memory.h"
#elif defined (__ppc__)
#include "ppc/memory.h"
#elif defined (__mips__)
#include "mips/memory.h"
#elif defined (__sh__)
#include "sh/memory.h"
#else
#error architecture not supported
#endif
