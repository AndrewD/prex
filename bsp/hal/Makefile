include $(SRCDIR)/mk/own.mk

TARGET:=	hal.o

_KERNEL_:=	1
DEFS+=		KERNEL

include $(CURDIR)/$(ARCH)/$(PLATFORM)/Makefile.inc

INCSDIR+=	$(CURDIR)/$(ARCH)/include \
		$(SRCDIR)/sys/include

include $(SRCDIR)/mk/obj.mk
