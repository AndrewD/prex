include $(SRCDIR)/mk/own.mk

INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/dev/$(ARCH)/include \
		-I$(SRCDIR)/dev/include -I$(SRCDIR)/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc -fno-builtin -DKERNEL
CPPFLAGS+=	$(INCLUDE) -DKERNEL
LDFLAGS+=	-static -nostdlib
LINTFLAGS+=	-DKERNEL

ifeq ($(KTRACE),1)
CFLAGS+=	-finstrument-functions
endif

include $(SRCDIR)/mk/Makefile.inc
