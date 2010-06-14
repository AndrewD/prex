
#if defined(__arm__)
#include "arm/elf.h"
#elif defined (__x86__)
#include "x86/elf.h"
#elif defined (__ppc__)
#include "ppc/elf.h"
#elif defined (__mips__)
#include "mips/elf.h"
#elif defined (__sh__)
#include "sh/elf.h"
#else
#error architecture not supported
#endif
