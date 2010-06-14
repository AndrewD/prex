
#if defined(__arm__)
#include "arm/stdarg.h"
#elif defined (__x86__)
#include "x86/stdarg.h"
#elif defined (__ppc__)
#include "ppc/stdarg.h"
#elif defined (__mips__)
#include "mips/stdarg.h"
#elif defined (__sh__)
#include "sh/stdarg.h"
#else
#error architecture not supported
#endif
