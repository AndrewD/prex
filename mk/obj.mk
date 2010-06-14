# Rules to link a set of .o files into one .o file

include $(SRCDIR)/mk/own.mk

_RELOC_OBJ_:=	1

ifndef _KERNEL_
INCSDIR+=	$(SRCDIR)/usr/include
endif

include $(SRCDIR)/mk/common.mk

$(TARGET): $(OBJS)
	$(call echo-file,LD     ,$@)
	$(LD) $(LDFLAGS) $(OUTPUT_OPTION) $^

