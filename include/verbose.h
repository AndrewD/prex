#ifndef __VERBOSE_H__
#define __VERBOSE_H__
#if defined( __BOOT__)
#include <boot.h>
#elif defined (__DRIVER__)
#include <driver.h>
#elif defined (__KERNEL__)
#include <debug.h>
#else /* userspace */
#include <sys/syslog.h>
#endif
#include <sys/types.h>

/* debug string handling */
#define VERBOSE_MAP(F) /* non-additive args first */	\
	F(VB_ALL,	0x7fffffff, "all",	0,	\
	  "ALL available debug output")                 \
	F(VB_NONE,	0x00000000, "none",	0,	\
	  "NO debug output")				\
	F(VB_CRIT,	0x00000001, "crit",	0,      \
	  "Errors or other critical messages")          \
	F(VB_INFO,	0x00000002, "info",	1,	\
	  "General information")                        \
	F(VB_DEBUG,	0x00000004, "debug",	1,	\
	  "Debugging information")                      \
	F(VB_TRACE,	0x00000008, "trace",	1,	\
	  "Program flow information")			\
	F(VB_WARN,	0x00000010, "warn",	1,	\
	  "Unexpected but recoverable errors")		\
	F(VB_MEM,	0x00008000, "mem",	1,      \
	  "memory system messages")			\
	/* subsystems */				\
	F(VB_RELOC,	0x00010000, "reloc",	1,      \
	  "elf relocation info")			\
	F(VB_PTHREAD,	0x00200000, "pthread",	1,	\
	  "Pthread subsystem messages")

enum VERBOSE_FLAGS
{
#define VERBOSE_ENUM(ARG_ENUM, ARG_VALUE, ARG_STRING, ARG_ADDITIVE, ARG_HELP) \
        ARG_ENUM = ARG_VALUE ,
        VERBOSE_MAP(VERBOSE_ENUM)
};

#define verbose_get() VERBOSE_DEFAULT

/* maximum flags allowed */
#ifdef DEBUG
#ifdef CONFIG_VERBOSE_MAX
#  define VERBOSE_MAX (CONFIG_VERBOSE_MAX)
#else  /* !CONFIG_VERBOSE_MAX */
#  define VERBOSE_MAX (VB_ALL)
#endif	/* !CONFIG_VERBOSE_MAX */
#ifdef CONFIG_VERBOSE_LEVEL
#  define VERBOSE_DEFAULT (VERBOSE_MAX & (CONFIG_VERBOSE_LEVEL))
#else  /* !CONFIG_VERBOSE_LEVEL */
#  define VERBOSE_DEFAULT (VERBOSE_MAX & (~(VB_DEBUG|VB_TRACE)))
#endif	/* !CONFIG_VERBOSE_LEVEL */
#define VERBOSE_FMT(fmt) "%s(%u): " fmt "\n", __FILE__, __LINE__

#else  /* !DEBUG */
#define VERBOSE_MAX (VB_CRIT)
#define VERBOSE_DEFAULT (VERBOSE_MAX & (VB_CRIT))
#define VERBOSE_FMT(fmt) "(%u): " fmt "\n", __LINE__

#endif	/* !DEBUG */

#define VERBOSE_ON(mask) (((verbose_get() & VERBOSE_MAX) & (mask)) == (mask) \
			  && (mask))

#define VERBOSE(mask, fmt, ...)					\
	__VERBOSE(mask, VERBOSE_FMT(fmt), ## __VA_ARGS__)
#define CVERBOSE(mask, cond, fmt, ...)				\
	__CVERBOSE(mask, cond, VERBOSE_FMT(fmt), ## __VA_ARGS__)

/*
 * wrappers for error codes with format strings. Returns the error code
 * but pastes the error code as a string in debug output
 *
 * Usage:
 * return WERR(EINVAL);
 * return WERR(EIO, "extra info %d", value);
 *
 * WERR() - use for errors not expected in normal program flow that should
 * _always_ be reported
 * DERR() - use for errors that indicate bugs in code calling the function
 *
 * DERR() can be made subsystem specific with a define like this before first
 * use in a file:
 *
 * #define VB_DEBUG (VB_DEBUG | VB_SUBSYSTEM)
 */
#define WERR(err, ...) /* optionally add fmt and args */		\
	({								\
		VERBOSE(VB_WARN, "(" #err ")" __VA_ARGS__);		\
		err;							\
	})

#define DERR(err, ...) /* optionally add fmt and args */		\
	({								\
		VERBOSE(VB_DEBUG, "(" #err ")" __VA_ARGS__);		\
		err;							\
	})

#if defined(__KERNEL__)
#define __VERBOSE(mask, fmt, ...) do {					\
		if (VERBOSE_ON(mask)) {					\
			printk(fmt, ## __VA_ARGS__);			\
		}							\
	} while (0)
#define __CVERBOSE(mask, cond, fmt, ...) do {				\
		if (VERBOSE_ON(mask) && (cond)) {			\
			printk(fmt, ## __VA_ARGS__);			\
		}							\
	} while (0)
#else  /* user code */
#define __VERBOSE(mask, fmt, ...) do {					\
		if (VERBOSE_ON(mask)) {					\
			syslog(LOG_ERR, fmt, ## __VA_ARGS__);		\
		}							\
	} while (0)
#define __CVERBOSE(mask, cond, fmt, ...) do {				\
		if (VERBOSE_ON(mask) && (cond)) {			\
			syslog(LOG_ERR, fmt, ## __VA_ARGS__);		\
		}							\
	} while (0)
#endif
#endif /* __VERBOSE_H__ */
