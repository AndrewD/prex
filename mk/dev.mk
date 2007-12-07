INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/dev/$(ARCH)/include \
		-I$(SRCDIR)/dev/include -I$(SRCDIR)/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc -fno-builtin -D__KERNEL__ -D__DRIVER__
CPPFLAGS+=	$(INCLUDE) -D__KERNEL__ -D__DRIVER__
LDFLAGS+=	-static -nostdlib
LINTFLAGS+=	-D__KERNEL__ -D__DRIVER__

ifeq ($(KTRACE),1)
CFLAGS+=	-finstrument-functions
endif

-include $(SRCDIR)/dev/$(ARCH)/$(PLATFORM)/dev.mk
include $(SRCDIR)/mk/Makefile.inc
