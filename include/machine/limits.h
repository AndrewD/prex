
#if defined(__arm__)
#include "arm/limits.h"
#elif defined (__x86__)
#include "x86/limits.h"
#elif defined (__ppc__)
#include "ppc/limits.h"
#elif defined (__mips__)
#include "mips/limits.h"
#elif defined (__sh__)
#include "sh/limits.h"
#else
#error architecture not supported
#endif
