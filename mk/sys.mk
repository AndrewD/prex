include $(SRCDIR)/mk/own.mk

INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/sys/arch/$(ARCH)/include \
		-I$(SRCDIR)/sys/include -I$(SRCDIR)/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc -fno-builtin -DKERNEL
CPPFLAGS+=	$(INCLUDE) -DKERNEL
LDFLAGS+=	-static -nostdlib
LINTFLAGS+=	-DKERNEL

ifeq ($(CONFIG_KTRACE),y)
CFLAGS+= -finstrument-functions
endif

include $(SRCDIR)/mk/Makefile.inc
