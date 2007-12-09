#
# Makefile.inc - common make rules to build Prex
#

# Required environment variables
#
#  PREX_SRC      ... Root directory of source tree
#  PREX_ARCH     ... Architecture name
#  PREX_PLATFORM ... Platform name
#
# Optional environment variables
#
#  NDEBUG        ... 0 for debug, 1 for release (default: 0)
#  CROSS_COMPILE ... Prefix of tools for cross compile
#  KTRACE        ... 1 for the kernel function trace (default: 0)
#  LIBGCC_PATH   ... Full path for libgcc.a
#

# Variables in local Makefile
#
#  TARGET	... Target file name
#  TYPE		... Traget type
#		    e.g. OBJECT,LIBRARY,KERNEL,BINARY,EXEC,DRIVER,OS_IMAGE
#  SUBDIRS	... List of subdirectories
#  OBJS		... Object files
#  LIBS		... Libraries
#  MAP		... Name of map file
#  DISASM	... Disassemble list file
#  SYMBOL	... Symbol files

#
# Option for cross compile
#
#CROSS_COMPILE	=
#CROSS_COMPILE	= i386-elf-
#CROSS_COMPILE	= arm-elf-
#CROSS_COMPILE	= powerpc-eabi-
#CROSS_COMPILE	= sh-elf-
#CROSS_COMPILE	= mips-elf-

#
# Tools
#
CC	= $(CROSS_COMPILE)gcc
CPP	= $(CROSS_COMPILE)cpp
AS	= $(CROSS_COMPILE)as
LD	= $(CROSS_COMPILE)ld
AR	= $(CROSS_COMPILE)ar
OBJCOPY	= $(CROSS_COMPILE)objcopy
OBJDUMP	= $(CROSS_COMPILE)objdump
STRIP	= $(CROSS_COMPILE)strip
LINT	= splint
MAKE	= make
RM	= rm
#SHELL	= /bin/sh
ifdef SHELL_PATH
SHELL	= $(SHELL_PATH)
endif
ifdef DISASM
ASMGEN	= $(OBJDUMP) $@ --disassemble-all > $(DISASM)
endif

#
# Architecture dependent options
#
include $(PREX_SRC)/mk/Makefile.$(PREX_ARCH)

#
# Flags
#
DEF_FLAGS = -D__$(PREX_ARCH)__ -D__$(PREX_PLATFORM)__ \
	-D__ARCH__=$(PREX_ARCH) -D__PLATFORM__=$(PREX_PLATFORM) \
	-U$(PREX_ARCH) -U$(PREX_PLATFORM)

ifeq ($(NDEBUG),1)
CFLAGS += -fomit-frame-pointer
else
CFLAGS += -DDEBUG -g
endif

# Correct gcc behavior for some distributions.
CFLAGS += $(shell if $(CC) -fno-stack-protector -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo "-fno-stack-protector"; fi)

ifdef MAP
LDFLAGS += -Map $(MAP)
endif

ASFLAGS	+= 
CFLAGS	+= -Wall -O2 -fno-strict-aliasing $(DEF_FLAGS)
CPPFLAGS += $(DEF_FLAGS)
LDFLAGS	+=
#MAKEFLAGS += --no-print-directory

LINT_FLAGS = -D__lint__ $(DEF_FLAGS) -nolib -weak -fcnuse -nestcomment \
	-retvalother -fullinitblock
# 
# Specify path for libgcc.a
#
ifndef LIBGCC_PATH
LIBGCC_PATH := $(dir $(shell $(CC) $(GLOBAL_CFLAGS) -print-libgcc-file-name))
endif

#
# Inference rules
#
%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

%.o: %.s
	$(AS) $(ASFLAGS) $(EXTRA_ASFLAGS) -o $@ $<

%.o: %.S
	$(CPP) $(CPPFLAGS) $(EXTRA_CPPFLAGS) $< $*.tmp
	$(AS) $(ASFLAGS) $(EXTRA_ASFLAGS) -o $@ $*.tmp
	$(RM) -f $*.tmp

#
# Target
#
all: $(SUBDIRS) $(TARGET)

#
# Rules to process sub-directory
#
ifdef SUBDIRS
.PHONY: $(SUBDIRS) 
$(SUBDIRS): dummy
	$(MAKE) -C $@
endif

#
# Rules to compile a set of .o files into one .o file
#
ifeq ($(TYPE),OBJECT)
$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(EXTRA_LDFLAGS) -r -o $@ $(OBJS) $(LIBS)
endif

#
# Rules to compile library
#
ifeq ($(TYPE),LIBRARY)
$(TARGET): $(OBJS) ar-target

.PHONY: ar-target
ar-target: $(OBJS)
	$(AR) $(EXTRA_ARFLAGS) rucs $(TARGET) $?
endif 

#
# Rules to compile kernel
#
ifeq ($(TYPE),KERNEL)
$(TARGET): $(OBJS) $(LIBS)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(OBJS) $(LIBS)
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $(SYMBOL)
endif
	$(STRIP) -s $@
endif

#
# Rules to compile device driver
#
ifeq ($(TYPE),DRIVER)
$(TARGET):  $(OBJS)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(OBJS) $(LIBS)
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $(SYMBOL)
endif
	$(STRIP) --strip-debug --strip-unneeded $@
endif

#
# Rules to compile binary file
#
ifeq ($(TYPE),BINARY)
$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) $(EXTRA_LDFLAGS) -o $@ $(OBJS) $(LIBS)
	$(ASMGEN)
	$(OBJCOPY) $(OBJCOPYFLAGS) $@
endif

#
# Rules to compile executable file
#
ifeq ($(TYPE),EXEC)
$(TARGET): $(OBJS) $(LIBS)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) $(EXTRA_LDFLAGS) -o $@ \
	$(CRT0) $(OBJS) $(LIBS) $(LIBC) $(LIBGCC_PATH)libgcc.a
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $(SYMBOL)
endif
	$(STRIP) --strip-debug --strip-unneeded $@
endif

#
# Rules to create OS image
#
ifeq ($(TYPE),OS_IMAGE)
$(TARGET): dummy
ifdef RDFILES
	$(AR) rcS ramdisk.a $(RDFILES)
	$(AR) t ramdisk.a
	$(AR) rcS tmp.a $(KERNEL) $(DRIVER) $(TASKS) ramdisk.a
	$(RM) ramdisk.a
else
	$(AR) rcS tmp.a $(KERNEL) $(DRIVER) $(TASKS)
endif
	$(AR) t tmp.a
	cat $(LOADER) tmp.a > $@
	$(RM) tmp.a
endif


-include Makefile.dep

SRCS = $(wildcard *.c) $(wildcard *.S)

#
# Depend
#
.PHONY: depend dep
depend dep:
ifdef SUBDIRS
	@(for d in $(SUBDIRS) _ ; do \
	  if [ "$$d" != "_" ] ; then $(MAKE) -C $$d depend; fi; \
	done);
endif
	$(RM) -f Makefile.dep
	@(for d in $(SRCS) _ ; do \
	  if [ "$$d" != "_" ] ; then \
	  $(CPP) -M $(CPPFLAGS) $$d >> Makefile.dep; fi; \
	done);
#
# Lint
#
.PHONY: lint
lint:
	@(for d in $(SRCS) _ ; do \
	  if [ "$$d" != "_" ] ; then \
	  $(LINT) $(LINT_FLAGS) $(INC_FLAGS) $$d; fi; \
	done);

#
# Clean up
#
.PHONY: clean
clean:
ifdef SUBDIRS
	@(for d in $(SUBDIRS) _ ; do \
	  if [ "$$d" != "_" ] ; then $(MAKE) -C $$d clean; fi; \
	done);
endif
	$(RM) -f Makefile.dep
	$(RM) -f $(OBJS)
	$(RM) -f $(TARGET)
ifdef DISASM
	$(RM) -f $(DISASM)
endif
ifdef MAP
	$(RM) -f $(MAP)
endif
ifdef SYMBOL
	$(RM) -f $(SYMBOL)
endif
ifdef CLEANS
	$(RM) -f $(CLEANS)
endif

.PHONY: dummy