INCLUDE+=	-I$(SRCDIR) -I$(SRCDIR)/boot/$(ARCH)/include \
		-I$(SRCDIR)/boot/include -I$(SRCDIR)/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -D__KERNEL_ -D__BOOT__ -nostdinc -fno-builtin
CPPFLAGS+=	$(INCLUDE) -D__KERNEL_ -D__BOOT__
LDFLAGS+=	-static -nostdlib
LINTFLAGS+=	-D__KERNEL_ -D__BOOT__

-include $(SRCDIR)/boot/$(ARCH)/$(PLATFORM)/boot.mk
include $(SRCDIR)/mk/Makefile.inc
