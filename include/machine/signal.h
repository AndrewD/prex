
#if defined(__arm__)
#include "arm/signal.h"
#elif defined (__x86__)
#include "x86/signal.h"
#elif defined (__ppc__)
#include "ppc/signal.h"
#elif defined (__mips__)
#include "mips/signal.h"
#elif defined (__sh__)
#include "sh/signal.h"
#else
#error architecture not supported
#endif
