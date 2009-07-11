INCLUDE=	-I$(BUILDDIR) -I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/usr/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc
CPPFLAGS+=	$(INCLUDE) -nostdinc
LDFLAGS+=	-static $(USR_LDFLAGS) -L$(BUILDDIR)/usr/lib

LIBC=		$(BUILDDIR)/usr/lib/libc.a
CRT0=		$(BUILDDIR)/usr/lib/crt0.o
TYPE=		EXEC

ifeq ($(CONFIG_MMU), y)
LD_SCRIPT=	$(SRCDIR)/usr/arch/$(ARCH)/user.ld
else
LD_SCRIPT=	$(SRCDIR)/usr/arch/$(ARCH)/user-nommu.ld
LDFLAGS+=	--relocatable -d
endif

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
