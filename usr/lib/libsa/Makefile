LIB=	../libsa.a

SRCS=	__libc_init.c _exit.c _stdio.c getchar.c \
	putchar.c puts.c printf.c \
	vsprintf.c assert.c sprintf.c syslog.c sigstub.c

CFLAGS+=	-D_STANDALONE
CPPFLAGS+=	-D_STANDALONE

include $(SRCDIR)/usr/lib/prex/Makefile.inc
include $(SRCDIR)/usr/lib/libc/ctype/Makefile.inc
include $(SRCDIR)/usr/lib/libc/errno/Makefile.inc
include $(SRCDIR)/usr/lib/libc/stdlib/Makefile.inc
include $(SRCDIR)/usr/lib/libc/string/Makefile.inc

CFLAGS_malloc.o += -D_REENTRANT

include $(SRCDIR)/mk/lib.mk
