INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/usr/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc -D_STANDALONE
CPPFLAGS+=	$(INCLUDE) -nostdinc -D_STANDALONE
LDFLAGS+=	-static $(USR_LDFLAGS)

LD_SCRIPT=	$(SRCDIR)/usr/arch/$(ARCH)/$(ARCH)-$(PLATFORM).ld
LIBC=		$(SRCDIR)/usr/lib/libsa.a
CRT0=		$(SRCDIR)/usr/lib/crt0.o
TYPE=		EXEC

ifdef TASK
TARGET ?= $(TASK)
endif

ifndef OBJS
ifdef SRCS
OBJS= $(SRCS:.c=.o)
else
OBJS= $(TARGET).o
endif
endif

include $(SRCDIR)/mk/Makefile.inc
