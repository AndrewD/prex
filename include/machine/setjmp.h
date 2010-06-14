
#if defined(__arm__)
#include "arm/setjmp.h"
#elif defined (__x86__)
#include "x86/setjmp.h"
#elif defined (__ppc__)
#include "ppc/setjmp.h"
#elif defined (__mips__)
#include "mips/setjmp.h"
#elif defined (__sh__)
#include "sh/setjmp.h"
#else
#error architecture not supported
#endif
