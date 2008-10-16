INCLUDE=	-I$(BUILDDIR) -I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/usr/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc
CPPFLAGS+=	$(INCLUDE) -nostdinc
LDFLAGS+=	-static $(USR_LDFLAGS)

LD_SCRIPT=	$(SRCDIR)/usr/arch/$(ARCH)/$(ARCH)-$(PLATFORM).ld
LIBC=		$(BUILDDIR)/usr/lib/libc.a
CRT0=		$(BUILDDIR)/usr/lib/crt0.o
TYPE=		EXEC

ifdef PROG
TARGET ?= $(PROG)
endif

ifndef OBJS
ifdef SRCS
OBJS= $(SRCS:.c=.o)
else
OBJS= $(TARGET).o
endif
endif

include $(SRCDIR)/mk/Makefile.inc
