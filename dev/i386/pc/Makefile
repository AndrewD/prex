include $(SRCDIR)/mk/own.mk

TARGET=		dev.o
TYPE=		OBJECT
OBJS-y=		dma.o machdep.o ../i386/delay.o

OBJS-$(CONFIG_CPUFREQ)+=	../i386/cpu.o
OBJS-$(CONFIG_KEYBOARD)+=	kbd.o
OBJS-$(CONFIG_CONSOLE)+=	console.o
OBJS-$(CONFIG_FDD)+=		fdd.o
OBJS-$(CONFIG_MOUSE)+=		mouse.o
OBJS-$(CONFIG_RTC)+=		rtc.o

include $(SRCDIR)/mk/dev.mk
