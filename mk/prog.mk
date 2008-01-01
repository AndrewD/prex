include $(SRCDIR)/mk/own.mk

INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/usr/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc
CPPFLAGS+=	$(INCLUDE) -nostdinc
LDFLAGS+=	-static $(USR_LDFLAGS)

LD_SCRIPT=	$(SRCDIR)/usr/arch/$(ARCH)/$(ARCH)-$(PLATFORM).ld
LIBC=		$(SRCDIR)/usr/lib/libc.a
CRT0=		$(SRCDIR)/usr/lib/crt0.o
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
