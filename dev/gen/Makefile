TARGET=	gen.o
TYPE=	OBJECT
OBJS-m=	null.o zero.o

OBJS-$(CONFIG_TTY)+=		tty.o
OBJS-$(CONFIG_RAMDISK)+=	ramdisk.o

include $(SRCDIR)/mk/dev.mk
