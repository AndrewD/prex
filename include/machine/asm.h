
#if defined(__arm__)
#include "arm/asm.h"
#elif defined (__x86__)
#include "x86/asm.h"
#elif defined (__ppc__)
#include "ppc/asm.h"
#elif defined (__mips__)
#include "mips/asm.h"
#elif defined (__sh__)
#include "sh/asm.h"
#else
#error architecture not supported
#endif
