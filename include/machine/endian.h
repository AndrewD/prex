
#if defined(__arm__)
#include "arm/endian.h"
#elif defined (__x86__)
#include "x86/endian.h"
#elif defined (__ppc__)
#include "ppc/endian.h"
#elif defined (__mips__)
#include "mips/endian.h"
#elif defined (__sh__)
#include "sh/endian.h"
#else
#error architecture not supported
#endif
