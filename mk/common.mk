# common.mk - common make rules to build Prex

#
# This file includes the common rules to make Prex source tree.
# The files are installed in /mk, and are, by convention,
# named with the suffix ".mk".
#
# Supported environment variables:
#
#  SRCDIR          ... Root directory of source tree
#  NDEBUG          ... Force disable debug switch (default: 0)
#  LIBGCC_PATH     ... Path for libgcc.a (optional)
#
# Variables in local Makefile:
#
#  TARGET          ... Target file name
#  SUBDIR,SUBDIR-y ... List of subdirectories
#  SRCS,SRCS-y     ... Source files
#  OBJS,OBJS-y     ... Object files
#  LIBS            ... Libraries
#  INCSDIR         ... Include path
#  LIBSDIR         ... Library path
#  DEFS            ... Definition to pass to the compiler
#  MAP             ... Name of map file
#  DISASM          ... Name of disassemble file
#  SYMBOL          ... Name of symbol file
#

ifndef _COMMON_MK_
_COMMON_MK_:=	1

ifdef _GNUC_
include $(SRCDIR)/mk/gcc.mk
endif

ifdef _PCC_
include $(SRCDIR)/mk/pcc.mk
endif

ifdef _SUNPRO_C_
include $(SRCDIR)/mk/suncc.mk
endif

ifdef DISASM
ASMGEN=		$(OBJDUMP) $@ --disassemble-all > $(DISASM)
endif

ifdef SYMBOL
SYMGEN=		cp $@ $(SYMBOL)
endif

SUBDIR+=	$(SUBDIR-y)
OBJS+=		$(OBJS-y)
SRCS+=		$(SRCS-y)
ifeq ($(OBJS),)
OBJS+=		$(addsuffix .o,$(basename $(SRCS)))
endif

.SUFFIXES:
.SUFFIXES: .bin .a .o .S .c .cc .cpp .cxx .h

ifeq ($(_SILENT_),1)
echo-file=	@echo '  $(1) $(subst $(SRCDIR)/,,$(abspath $(2)))'

echo-files=	@(for d in $(2) _ ; do \
		  if [ "$$d" != "_" ]; then \
		    echo "  $(1) $(subst $(SRCDIR)/,,$(abspath $$d))" ; fi; \
		  done);
endif

#
# Inference rules
#
%.o: %.c
	$(call echo-file,CC     ,$<)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(OUTPUT_OPTION) $<

%.o: %.S
	$(call echo-file,AS     ,$<)
	$(CPP) $(ACPPFLAGS) $(CPPFLAGS) $< $*.tmp
	$(AS) $(ASFLAGS) $(OUTPUT_OPTION) $*.tmp
	$(RM) $*.tmp


#
# Target
#
all: config $(SUBDIR) $(TARGET)


#
# Check configuration file
#
.PHONY: config
config:
ifndef ARCH
	@echo 'Error: You must run `configure` before make.'
	exit 1
endif

#
# Rules to process sub-directory
#
ifdef SUBDIR
.PHONY: $(SUBDIR)
$(SUBDIR): dummy
	@$(MAKE) -C $@
endif

-include Makefile.dep

#
# Depend
#
.PHONY: depend dep
depend dep:
ifdef SUBDIR
	@(for d in $(SUBDIR) _ ; do \
	  if [ "$$d" != "_" ]; then $(MAKE) -C $$d depend; fi; \
	done);
endif
	$(RM) Makefile.dep
	@(for d in $(SRCS) _ ; do \
	  if [ "$$d" != "_" ]; then \
	  $(CPP) -M $(CPPFLAGS) $$d >> Makefile.dep; fi; \
	done);

#
# Lint
#
.PHONY: lint
lint:
ifdef SUBDIR
	@(for d in $(SUBDIR) _ ; do \
	  if [ "$$d" != "_" ]; then $(MAKE) -C $$d lint; fi; \
	done);
endif
	@(for d in $(filter %.c, $(SRCS)) _ ; do \
	  if [ "$$d" != "_" ]; then \
	  echo ; \
	  echo "Checking $$d" ; \
	  $(LINT) $(CPPFLAGS) $(LINTFLAGS) $$d; fi; \
	done);

#
# Clean up
#
CLEANS= Makefile.dep $(TARGET) $(OBJS) $(DISASM) $(MAP) $(SYMBOL) $(CLEANFILES)

.PHONY: clean
clean:
ifdef SUBDIR
	@(for d in $(SUBDIR) _ ; do \
	  if [ "$$d" != "_" ]; then $(MAKE) -C $$d clean; fi; \
	done);
endif
	$(call echo-files,CLEAN  ,$(CLEANS))
	$(RM) $(CLEANS)

.PHONY: dummy

#
# Build OS image
#
.PHINY: image
image: config $(TARGET)

endif # !_COMMON_MK_
