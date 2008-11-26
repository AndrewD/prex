#
# Makefile.inc - common make rules to build Prex
#

#
# Supported environment variables
#
#  SRCDIR        ... Root directory of source tree
#  ARCH          ... Architecture name
#  PLATFORM      ... Platform name
#  NDEBUG        ... 0 for debug, 1 for release (default: 0)
#  LIBGCC_PATH   ... Directory for libgcc.a
#  CROSS_COMPILE ... Prefix of tools for cross compile
#  SYMBOL        ... Generate symbol files with this suffix (if defined)
#                    set in environment or makefile (defaults to .elf for debug)
#
# Variables in local Makefile
#
#  TARGET	... Target file name
#  TYPE		... Traget type
#		    e.g. OBJECT,LIBRARY,KERNEL,BINARY,EXEC,DRIVER,OS_IMAGE
#  SUBDIR	... List of subdirectories
#  OBJS		... Object files (not for drivers)
#  OBJS-y	... Object files
#  OBJS-m	... Object files for modular driver (only for drivers)
#  OBJS-	... Disabled Object files
#  OBJS-$(CONFIG_FOO) ... Object enabled / disabled by conf/config.ARCH-PLATFORM
#  LIBS		... Libraries
#  MAP		... Name of map file
#  DISASM	... Disassemble list file
#

# Variables in conf/$(ARCH)/config.$(ARCH)-$(PLATFORM)
#
# CONFIG_FOO=y	... enable feature FOO / build driver FOO into kernel
# CONFIG_FOO=m	... build driver FOO as a module
# CONFIG_FOO=*	... set config parameter FOO to *

#
# Option for cross compile
#
#CROSS_COMPILE=
#CROSS_COMPILE=	i386-elf-
#CROSS_COMPILE=	arm-elf-
#CROSS_COMPILE=	powerpc-eabi-
#CROSS_COMPILE=	sh-elf-
#CROSS_COMPILE=	mips-elf-

#
# Tools
#
CC=		$(CROSS_COMPILE)gcc
CPP=		$(CROSS_COMPILE)cpp
AS=		$(CROSS_COMPILE)as
LD=		$(CROSS_COMPILE)ld
AR=		$(CROSS_COMPILE)ar
OBJCOPY=	$(CROSS_COMPILE)objcopy
OBJDUMP=	$(CROSS_COMPILE)objdump
STRIP=		$(CROSS_COMPILE)strip
LINT=		splint
MAKE=		make
#SHELL=		/bin/sh
ifdef SHELL_PATH
SHELL=		$(SHELL_PATH)
endif
ifdef DISASM
ASMGEN=		$(OBJDUMP) $@ --disassemble-all > $(DISASM)
endif

#
# System dependent options
#
include $(SRCDIR)/mk/own.mk

#
# Flags
#
DEFS=		-D__$(ARCH)__ -D__$(PLATFORM)__ \
		-D__ARCH__=$(ARCH) -D__PLATFORM__=$(PLATFORM) \
		-U$(ARCH) -U$(PLATFORM)

OPTIMISE?=	-Os
CFLAGS+=	$(OPTIMISE) -Wall -Wundef -fno-strict-aliasing \
		-Wstrict-prototypes -Wpointer-arith \
		$(CONFIG_CFLAGS) $(DEFS)
CFLAGS+=	-std=gnu99
CPPFLAGS+=	-pedantic -Wall $(DEFS)

ifeq ($(NDEBUG),1)
CFLAGS+=	-fomit-frame-pointer
else
CFLAGS+=	-fno-omit-frame-pointer -g
DEFS+=		-DDEBUG
SYMBOL?=	.elf
endif

CFLAGS+=	-Wsign-compare
CFLAGS+=	-Werror-implicit-function-declaration
ASFLAGS+=	-W

ifdef MAP
LDFLAGS+=	-Map $(MAP)
endif
LDFLAGS+=
MAKEFLAGS+=	-rR
LINTFLAGS+=	-D__lint__ $(DEFS) -weak -nolib -retvalother -fcnuse

BINFLAGS=	-O binary -R .note -R .comment -S

BRELDIR = $(BUILDDIR)$(RELDIR)

#
# Prepend $(BRELDIR) to all outputs
#
BOBJS += $(addprefix $(BRELDIR)/, $(OBJS))
BOBJS- += $(addprefix $(BRELDIR)/, $(OBJS-))
BOBJS-y += $(addprefix $(BRELDIR)/, $(OBJS-y))
BOBJS-m += $(addprefix $(BRELDIR)/, $(OBJS-m))
BTARGET += $(addprefix $(BRELDIR)/, $(TARGET))
BLIBS += $(addprefix $(BRELDIR)/, $(LIBS))
BLIBS-y += $(addprefix $(BRELDIR)/, $(LIBS-y))

SD1 = $(sort $(OBJS) $(OBJS-y) $(OBJS-m) $(LIBS))
SD2 = $(filter-out ..%, $(SD1))
SD3 = $(addsuffix %, $(dir $(SD2)))
SD4 = $(filter $(SD3), $(SD2))
BSUBDEPS += $(addprefix $(BRELDIR)/, $(SD4))

#
# Automatic dependencies
#
# need -MT for CPP 4.1.2 - it strips the path from the target
ifndef NO_DEPS
DFLAGS+=	-MD -MT $@ -MP -MF $(BRELDIR)/.$(*F).d
endif

#
# Specify path for libgcc.a
#
ifndef LIBGCC_PATH
LIBGCC_PATH := $(dir $(shell $(CC) $(CFLAGS) -print-libgcc-file-name))
endif
LIBGCC = $(LIBGCC_PATH)libgcc.a

# For use in expunging commas from flags, which mung our checking.
comma = ,

#
# Target
#
all: $(SUBDIR) $(BTARGET)

# dependencies for parallel make
$(BTARGET): $(SUBDIR)
$(BSUBDEPS): $(SUBDIR)

#
# Inference rules
#
$(BRELDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(DFLAGS) $(CFLAGS_$(notdir $@)) -c -o $@ $<
ifndef NO_FLAGS
	@ ( \
	    echo 'ifneq ($(strip $(subst $(comma),:,$(CFLAGS) $(CFLAGS_$(notdir $@)))),$$(strip $$(subst $$(comma),:,$$(CFLAGS) $$(CFLAGS_$(notdir $@)))))' ; \
	    echo 'FILES_FLAGS_CHANGED += $@' ; \
	    echo 'endif' \
	) > $(BRELDIR)/.$(basename $(notdir $@)).flags
endif

$(BRELDIR)/%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

$(BRELDIR)/%.o: %.S
	$(CPP) $(CPPFLAGS) $(DFLAGS) -D__ASSEMBLY__ $< $(BRELDIR)/$*.tmp
	$(AS) $(ASFLAGS) -o $@ $(BRELDIR)/$*.tmp
	rm -f $(BRELDIR)/$*.tmp
ifndef NO_FLAGS
	@ ( \
	    echo 'ifneq ($(strip $(subst $(comma),:,$(CPPFLAGS))),$$(strip $$(subst $$(comma),:,$$(CPPFLAGS))))' ; \
	    echo 'FILES_FLAGS_CHANGED += $@' ; \
	    echo 'endif' \
	) > $(BRELDIR)/.$(basename $(notdir $@)).flags
endif

#
# Rules to process sub-directory
#
.PHONY: $(SUBDIR)
$(SUBDIR):
	mkdir -p $(BUILDDIR)$(RELDIR)/$@
	RELDIR=$(RELDIR)/$@ $(MAKE) -C $@ $(MAKECMDGOALS)

#
# Rules to link a set of .o files into one .o file
#
ifeq ($(TYPE),OBJECT)
ifeq (,$(findstring __DRIVER__,$(CFLAGS)))
#
# normal objects
#
# BOBJS   unconditionally linked objects
# BOBJS-y conditionally linked based on a CONFIG_* variable
# BOBJS-  disabled objects - associated CONFIG_* variable undefined

ifneq (,$(strip $(BOBJS) $(BOBJS-y)))
$(BTARGET): $(BOBJS) $(BOBJS-y) $(BLIBS)
	$(LD) $(LDFLAGS) -r -o $@ $(sort $(BOBJS) $(BOBJS-y)) $(BLIBS)
else # ensure an object file always exists
$(BTARGET): dummy
	echo | $(AS) -o $@ -
endif

ifneq (,$(strip $(BOBJS-m)))
$(error $(BOBJS-m) must not be OBJ-m, this is only for drivers)
endif

else
#
# driver objects
#
# BOBJS-y link objects for static drivers (typically set via CONFIG_*)
# BOBJS-m link objects for modular drivers
# BOBJS-  disabled objects
# BOBJS   is no longer used for drivers
ifneq (,$(strip $(BOBJS)))
$(error $(BOBJS) must not be BOBJS, this is _not_ for drivers)
endif

# additional target for modular drivers
all: $(BTARGET:.o=.mo)
$(BTARGET:.o=.mo): $(SUBDIR)

# static driver objects
ifneq (,$(strip $(BOBJS-y)))
$(BTARGET): $(BOBJS-y) $(BLIBS-y)
	$(LD) $(LDFLAGS) -r -o $@ $(sort $(BOBJS-y)) $(BLIBS-y)
else # ensure an object file always exists
$(BTARGET): dummy
	echo | $(AS) -o $@ -
endif

# modular driver objects
ifneq (,$(strip $(BOBJS-m)))
$(BTARGET:.o=.mo): $(BOBJS-m)
	$(LD) $(LDFLAGS) -r -o $@ $(sort $(BOBJS-m))
else
$(BTARGET:.o=.mo): dummy
	echo | $(AS) -o $@ -
endif

endif # driver objects
endif # type opject

#
# Rules to compile library
#
ifeq ($(TYPE),LIBRARY)
$(BTARGET): $(BOBJS) ar-target

.PHONY: ar-target
ar-target: $(BOBJS)
	$(AR) rucs $(BTARGET) $?
ifeq ($(NDEBUG),1)
	$(STRIP) -x -R .comment -R .note $(BTARGET)
endif
endif

#
# Rules to compile kernel
#
ifeq ($(TYPE),KERNEL)
$(BTARGET): $(LD_SCRIPT) $(BOBJS) $(BLIBS) $(LIBGCC)
	$(LD) $(LDFLAGS) -o $@ -T $(LD_SCRIPT) $(BOBJS) $(BLIBS) $(LIBGCC)
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $@$(SYMBOL)
endif
	$(STRIP) -s $@
endif

#
# Rules to compile device driver
#
ifeq ($(TYPE),DRIVER)
# additional target for modular drivers
all: $(BTARGET:.o=.ko)
$(BOBJS:.o=.mo): $(SUBDIR)
$(BTARGET:.o=.ko): $(SUBDIR)

$(BTARGET:.o=.ko): $(LD_SCRIPT) $(BOBJS:.o=.mo) $(BLIBS) $(LIBGCC)
	$(LD) $(LDFLAGS) -o $@ -T $(LD_SCRIPT) $(BOBJS:.o=.mo) $(BLIBS) $(LIBGCC)
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $@$(SYMBOL)
endif
	$(STRIP) --strip-debug --strip-unneeded $@

# staticly linked drivers
$(BTARGET): $(BOBJS)
	$(LD) $(LDFLAGS) -r -o $@ $(sort $(BOBJS))
endif

#
# Rules to compile binary file
#
ifeq ($(TYPE),BINARY)
$(BTARGET): $(LD_SCRIPT) $(BOBJS) $(BLIBS) $(LIBGCC)
	$(LD) $(LDFLAGS) -o $@ -T $(LD_SCRIPT) $(BOBJS) $(BLIBS) $(LIBGCC)
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $@$(SYMBOL)
endif
	$(OBJCOPY) $(BINFLAGS) $@
endif

#
# Rules to compile executable file
#
ifeq ($(TYPE),EXEC)
$(BTARGET): $(LD_SCRIPT) $(CRTI) $(CRT0) $(BOBJS) $(BLIBS) $(LIBC) $(LDADD) $(LIBGCC) $(CRTN)
	$(LD) $(LDFLAGS) -o $@ -T $(LD_SCRIPT) $(CRTI) $(CRT0) $(BOBJS) $(BLIBS) $(LIBC) $(LDADD) $(LIBGCC) $(CRTN)
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $@$(SYMBOL)
endif
	$(STRIP) --strip-debug --strip-unneeded $@
endif

#
# Rules to create OS image
#
ifeq ($(TYPE),OS_IMAGE)
$(BTARGET): dummy
ifdef BOOTFILES
	$(AR) rcS $(BRELDIR)/ramdisk.a $(BOOTFILES)
	$(AR) t $(BRELDIR)/ramdisk.a
	$(AR) rcS $(BRELDIR)/tmp.a $(KERNEL) $(DRIVER) $(BOOTTASKS) $(BRELDIR)/ramdisk.a
	rm $(BRELDIR)/ramdisk.a
else
	$(AR) rcS $(BRELDIR)/tmp.a $(KERNEL) $(DRIVER) $(BOOTTASKS)
endif
	$(AR) t $(BRELDIR)/tmp.a
	cat $(LOADER) $(BRELDIR)/tmp.a > $@
	rm $(BRELDIR)/tmp.a
endif

#
# Manage object dependencies on headers and compiler flags
#
ifneq (,$(strip $(BOBJS) $(BOBJS-y) $(BOBJS-m) $(BTARGET)))

ifndef NO_DEPS
DEP_INCLUDES:= $(strip $(wildcard $(BRELDIR)/.*.d))

ifneq (,$(DEP_INCLUDES))
include $(DEP_INCLUDES)
endif # DEP_INCLUDES

endif # !NO_DEPS

ifndef NO_FLAGS
FLAG_INCLUDES:= $(strip $(wildcard $(BRELDIR)/.*.flags))

ifneq (,$(FLAG_INCLUDES))
FILES_FLAGS_CHANGED:=
include $(FLAG_INCLUDES)

$(FILES_FLAGS_CHANGED): dummy

endif # FLAG_INCLUDES
endif # !NO_FLAGS
endif # BOBJS

ifndef SRCS
SRCS = $(OBJS:.o=.c)
endif

#
# Lint
#
.PHONY: lint
lint: $(SUBDIR)
	@(for d in $(SRCS) _ ; do \
	  if [ "$$d" != "_" ] ; then \
	  echo ; \
	  echo "Checking $$d" ; \
	  $(LINT) $(LINTFLAGS) $(INCLUDE) $$d; fi; \
	done);

#
# Clean up
#
.PHONY: clean
clean: $(SUBDIR)
	rm -f $(BTARGET) $(DISASM) $(MAP) $(BTARGET)$(SYMBOL) $(CLEANFILES)
ifneq (,$(strip $(BOBJS) $(BOBJS-y) $(BOBJS-m) $(BOBJS-)))
	rm -f $(sort $(BOBJS) $(BOBJS-y) $(BOBJS-m) $(BOBJS-))
endif
ifneq (,$(DEP_INCLUDES))
	rm -f $(DEP_INCLUDES)
endif
ifneq (,$(FLAG_INCLUDES))
	rm -f $(FLAG_INCLUDES)
endif
ifeq ($(TYPE),DRIVER)
ifneq (,$(strip $(BTARGET:.o=.ko) $(BTARGET:.o=.ko)$(SYMBOL) $(BOBJS:.o=.mo)))
	rm -f $(sort $(BTARGET:.o=.ko) $(BTARGET:.o=.ko)$(SYMBOL) $(BOBJS:.o=.mo))
endif
endif
ifeq ($(TYPE),OBJECT)
ifneq (,$(findstring __DRIVER__,$(CFLAGS)))
	rm -f $(BTARGET:.o=.mo)
endif
endif

.PHONY: dummy
