# Rules to compile boot loader

include $(SRCDIR)/mk/own.mk

TARGET:=	bootldr

#MAP:=		bootldr.map
#DISASM:= 	bootldr.lst
#SYMBOL:=	bootldr.sym

_KERNEL_:=	1
DEFS+=		KERNEL

include $(CURDIR)/$(ARCH)/$(PLATFORM)/Makefile.inc
include $(CURDIR)/$(ARCH)/arch/Makefile.inc
include $(CURDIR)/common/Makefile.inc

LIBSDIR+=	$(SRCDIR)/conf
INCSDIR+=	$(CURDIR)/include $(CURDIR)/$(ARCH)
ifndef LDSCRIPT
LDSCRIPT:=	$(CURDIR)/$(ARCH)/arch/boot.ld
endif

include $(SRCDIR)/mk/common.mk

$(TARGET): $(OBJS) $(LIBS) $(LDSCRIPT)
	$(call echo-file,LD     ,$@)
	$(LD) $(LDFLAGS) $(OUTPUT_OPTION) $(OBJS) $(LIBS) $(PLATFORM_LIBS)
	$(ASMGEN)
	$(SYMGEN)
	$(OBJCOPY) -O binary -R .note -R .comment -S $@
