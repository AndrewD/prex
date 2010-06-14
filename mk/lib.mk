# Rules to compile library

include $(SRCDIR)/mk/own.mk

_RELOC_OBJ_:=	1

ifndef _KERNEL_
INCSDIR+=	$(SRCDIR)/usr/include
endif

include $(SRCDIR)/mk/common.mk

$(TARGET): $(OBJS)
	$(call echo-file,AR     ,$(TARGET))
	$(AR) rucs $(TARGET) $?
	$(STRIP) -x -R .comment -R .note $(TARGET)
