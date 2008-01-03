include $(SRCDIR)/mk/own.mk

INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/sys/arch/$(ARCH)/include \
		-I$(SRCDIR)/sys/include -I$(SRCDIR)/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc -fno-builtin -D__KERNEL__
CPPFLAGS+=	$(INCLUDE) -D__KERNEL__
LDFLAGS+=	-static -nostdlib
LINTFLAGS+=	-D__KERNEL__

ifeq ($(CONFIG_KTRACE),y)
CFLAGS+= -finstrument-functions
endif

-include $(SRCDIR)/sys/arch/$(ARCH)/$(PLATFORM)/sys.mk
include $(SRCDIR)/mk/Makefile.inc
