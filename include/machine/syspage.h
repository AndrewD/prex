
#if defined(__arm__)
#include "arm/syspage.h"
#elif defined (__x86__)
#include "x86/syspage.h"
#elif defined (__ppc__)
#include "ppc/syspage.h"
#elif defined (__mips__)
#include "mips/syspage.h"
#elif defined (__sh__)
#include "sh/syspage.h"
#else
#error architecture not supported
#endif
