
#if defined(__arm__)
#include "arm/ansi.h"
#elif defined (__i386__)
#include "i386/ansi.h"
#elif defined (__ppc__)
#include "ppc/ansi.h"
#elif defined (__mips__)
#include "mips/ansi.h"
#elif defined (__sh4__)
#include "sh4/ansi.h"
#else
#error architecture not supported
#endif
