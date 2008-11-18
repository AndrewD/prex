INCLUDE=	-I$(BUILDDIR) -I$(SRCDIR) -I$(BUILDDIR)/sys/arch/$(ARCH)/include \
		-I$(SRCDIR)/sys/arch/$(ARCH)/include \
		-I$(SRCDIR)/sys/include -I$(SRCDIR)/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc -fno-builtin -D__KERNEL__
CPPFLAGS+=	$(INCLUDE) -D__KERNEL__
LDFLAGS+=	-static -nostdlib -L$(BUILDDIR)/conf
LINTFLAGS+=	-D__KERNEL__

-include $(SRCDIR)/sys/arch/$(ARCH)/$(PLATFORM)/sys.mk
include $(SRCDIR)/mk/Makefile.inc
