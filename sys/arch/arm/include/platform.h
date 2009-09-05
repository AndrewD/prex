
#if defined(__gba__)
#include "../gba/platform.h"
#elif defined(__integrator__)
#include "../integrator/platform.h"
#elif defined(__at91x40__)
#include "../at91x40/platform.h"
#else
#error platform not supported
#endif
