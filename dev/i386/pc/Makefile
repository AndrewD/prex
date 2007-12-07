TARGET=		dev.o
TYPE=		OBJECT
OBJS-m=		dma.o machdep.o ../i386/delay.o

ifdef CONFIG_CPUFREQ
OBJS-$(CONFIG_PM)+=		../i386/cpu.o
endif
OBJS-$(CONFIG_KEYBOARD)+=	kbd.o
OBJS-$(CONFIG_CONSOLE)+=	console.o
OBJS-$(CONFIG_FDD)+=		fdd.o
OBJS-$(CONFIG_MOUSE)+=		mouse.o
OBJS-$(CONFIG_RTC)+=		rtc.o

include $(SRCDIR)/mk/dev.mk
