#if defined(__gba__)
#include "../gba/platform.h"
#elif defined(__integrator__)
#include "../integrator/platform.h"
#elif defined(__beagle__)
#include "../beagle/platform.h"
#else
#error platform not supported
#endif
