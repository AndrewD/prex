
#if defined(__arm__)
#include "arm/lock.h"
#elif defined (__x86__)
#include "x86/lock.h"
#elif defined (__ppc__)
#include "ppc/lock.h"
#elif defined (__mips__)
#include "mips/lock.h"
#elif defined (__sh__)
#include "sh/lock.h"
#else
#error architecture not supported
#endif
