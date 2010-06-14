# pcc specific flags

ifndef _PCC_MK_
_PCC_MK_:=	1

OUTPUT_OPTION=	-o $@

DEFINES=	$(addprefix -D,$(DEFS))

CFLAGS+=	-c -O -static -nostdinc -nostdlib
CPPFLAGS+=	$(DEFINES) -I. $(addprefix -I,$(INCSDIR))
ACPPFLAGS+=	-D__ASSEMBLY__
LDFLAGS+=	-static -nostdlib $(addprefix -L,$(LIBSDIR))

ifeq ($(_DEBUG_),1)
CFLAGS+=	-g
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

endif # !_PCC_MK_
