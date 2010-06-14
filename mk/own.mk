ifndef _OWN_MK_
_OWN_MK_:=	1

# Build flavor
#_DEBUG_:=	1
#_QUICK_:=	1
#_STRICT_:=	1
#_SILENT_:=	1

# Clean the slate
CFLAGS=
CPPFLAGS=
LDFLAGS=
ASFLAGS=
STRIPFLAG=

ifndef _CONFIG_MK_
-include $(SRCDIR)/conf/config.mk
export SRCDIR
endif

LINT:=		splint
#LINT:=		lint
RM:=		rm -f
CAT:=		cat
ifdef SHELL_PATH
SHELL:=		$(SHELL_PATH)
endif

# We assume GNU make...
MAKEFLAGS+=	-rR --no-print-directory

ifeq ($(LINT),splint)
LINTFLAGS:=	-D__lint__ -weak -nolib -retvalother -fcnuse
else
LINTFLAGS:=	-D__lint__ -x -u
endif

INCSDIR:=	$(SRCDIR) $(SRCDIR)/include
DEFS+=		__$(ARCH)__ __$(PLATFORM)__

ifneq ($(NDEBUG),1)
ifeq ($(_DEBUG_),1)
DEFS+=		DEBUG
DEBUG:=		1
endif
endif

RAWCC:=		$(CC)
ifeq ($(_SILENT_),1)
CC:=		@$(CC)
CPP:=		@$(CPP)
AS:=		@$(AS)
LD:=		@$(LD)
AR:=		@$(AR)
STRIP:=		@$(STRIP)
OBJCOPY:=	@$(OBJCOPY)
OBJDUMP:=	@$(OBJDUMP)
RM:=		@$(RM)
CAT:=		@$(CAT)
endif

endif # !_OWN_MK_
