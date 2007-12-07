TARGET=		dev.o
TYPE=		OBJECT
OBJS-m= 	machdep.o

OBJS-$(CONFIG_CONSOLE)+=	console.o
OBJS-$(CONFIG_KEYPAD)+=		keypad.o
OBJS-$(CONFIG_KEYBOARD)+=	swkbd.o

include $(SRCDIR)/mk/dev.mk
