#ifndef _CONFIG_H
#define _CONFIG_H

#if defined(__i386__) && defined(__pc__)
#include "config-i386-pc.h"
#endif
#if defined(__i386__) && defined(__nommu__)
#include "config-i386-nommu.h"
#endif
#if defined(__arm__) && defined(__gba__)
#include "config-arm-gba.h"
#endif

#endif /* !_CONFIG_H */
