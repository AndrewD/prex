# sun studio specifc flags

ifndef _SUNCC_MK_
_SUNCC_MK_:=	1

OUTPUT_OPTION=	-o $@

DEFINES=	$(addprefix -D,$(DEFS))

CFLAGS+=	-c -xspace -features=none -xnolib -xc99=none
CPPFLAGS+=	$(DEFINES) -I. $(addprefix -I,$(INCSDIR))
ACPPFLAGS+=	-D__ASSEMBLY__
LDFLAGS+=	-static -nostdlib $(addprefix -L,$(LIBSDIR))

ifeq ($(_DEBUG_),1)
CFLAGS+=	-g
else
CFLAGS+=
endif

ifeq ($(_KERNEL_),1)
CFLAGS+=	-xbuiltin=%none
endif

ifeq ($(_STRICT_),1)
CFLAGS+=	-errwarn
endif

ifdef LDSCRIPT
LDFLAGS+=	-T $(LDSCRIPT)
endif

ifdef MAP
LDFLAGS+=	-Map $(MAP)
endif

ifeq ($(_RELOC_OBJ_),1)
LDFLAGS_S=	$(LDFLAGS) --error-unresolved-symbols
LDFLAGS+=	-r -d
endif

endif # !_SUNCC_MK_
