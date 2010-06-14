
#if defined(__arm__)
#include "arm/systrap.h"
#elif defined (__x86__)
#include "x86/systrap.h"
#elif defined (__ppc__)
#include "ppc/systrap.h"
#elif defined (__mips__)
#include "mips/systrap.h"
#elif defined (__sh__)
#include "sh/systrap.h"
#else
#error architecture not supported
#endif
