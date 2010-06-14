# Rules to compile a real-time task

include $(SRCDIR)/mk/own.mk

DEFS+=		_STANDALONE
INCSDIR+=	$(SRCDIR)/usr/include
LIBSDIR+=	$(SRCDIR)/usr/lib
CRT0:=		$(SRCDIR)/usr/lib/crt0.o
LIBC:=		$(SRCDIR)/usr/lib/libsa.a

ifeq ($(CONFIG_MMU),y)
LDSCRIPT:=	$(SRCDIR)/usr/arch/$(ARCH)/user.ld
STRIPFLAG:=	-s
else
LDSCRIPT:=	$(SRCDIR)/usr/arch/$(ARCH)/user-nommu.ld
STRIPFLAG:=	--strip-debug --strip-unneeded
_RELOC_OBJ_:=	1
endif

ifdef TASK
TARGET?=	$(TASK)
ifndef SRCS
SRCS:=		$(basename $(TASK)).c
endif
endif

include $(SRCDIR)/mk/common.mk

$(TARGET): $(LIBS) $(OBJS)
	$(call echo-file,LD     ,$@)
	$(LD) $(LDFLAGS) $(OUTPUT_OPTION) $(CRT0) $(OBJS) $(LIBS) $(LIBC) $(PLATFORM_LIBS)
	$(ASMGEN)
	$(SYMGEN)
	$(STRIP) $(STRIPFLAG) $@
