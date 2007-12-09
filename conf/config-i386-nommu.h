/*
 * Prex configuration file
 */

/*
 * System paramters
 */
#define CONFIG_HZ		1000		/* Ticks/second of the clock */
#define CONFIG_TIME_SLICE	50		/* Context switch ratio (msec) */
#define CONFIG_OPEN_MAX		16		/* Max open files per process */
#define CONFIG_BUF_CACHE	32		/* Blocks for buffer cache */
#define CONFIG_PAGE_SIZE	2048		/* Physical/logical page size */

/*
 * Platform settings
 */
//#define CONFIG_MMU		/* Memory management unit */
#define CONFIG_CACHE		/* Cache memory */
//#define CONFIG_FPU		/* Floating point unit */
#define CONFIG_XIP		/* Execution in place */
#define CONFIG_LITTLE_ENDIAN	/* Byte order */
//#define CONFIG_BIG_ENDIAN	/* Byte order */
//#define CONFIG_ROM_BOOT	/* Boot from ROM */

/*
 * Device drivers
 */
#define CONFIG_KEYBOARD		/* Keyboard */
#define CONFIG_CONSOLE		/* Console */
#define CONFIG_FDD		/* Floppy disk drive */
#define CONFIG_MOUSE		/* Mouse */
#define CONFIG_RTC		/* Real time clock */
#define CONFIG_RAMDISK		/* RAM disk */

/*
 * Power management
 */
#define CONFIG_PM		/* Power management support */
#define CONFIG_PM_POWERSAVE	/* Power policy: Battery optimized */
//#define CONFIG_PM_PERFORMANCE	/* Power policy: Parformance optimized */
#define CONFIG_DVS		/* Dynamic voltage scalling */
#define CONFIG_DVS_EMULATION	/* DVS emulation on Bochs */

/*
 * File system
 */
#define CONFIG_DEVFS		/* Device file system */
#define CONFIG_RAMFS		/* RAM file system */
#define CONFIG_ARFS		/* Archive file system */
#define CONFIG_FATFS		/* FAT file system */

/*
 * Executable file format
 */
#define CONFIG_ELF		/* ELF file format */

/*
 * Diagnostic options
 */
#define CONFIG_DIAG_SCREEN	/* Diagnostic via screen */
//#define CONFIG_DIAG_BOCHS	/* Diagnostic via Bochs emulater */

/*
 * Kernel hacking
 */
#define CONFIG_KDUMP		/* Kernel dump */
//#define CONFIG_KTRACE		/* Kernel function trace */
//#define CONFIG_GDB		/* GDB stub */
//#define CONFIG_DEBUG_VM	/* Debug virtual memory allocator */
//#define CONFIG_DEBUG_PAGE	/* Debug page allocator */
//#define CONFIG_DEBUG_KMEM	/* Debug kernel memory allocator */
#define CONFIG_MIN_MEMORY	/* Test under limited memory size */

