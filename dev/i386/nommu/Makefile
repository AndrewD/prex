include $(SRCDIR)/mk/own.mk

TARGET=		dev.o
TYPE=		OBJECT
OBJS-m=		../pc/dma.o ../pc/machdep.o ../i386/delay.o

ifdef CONFIG_CPUFREQ
OBJS-$(CONFIG_PM)+=		../i386/cpu.o
endif
OBJS-$(CONFIG_KEYBOARD)+=	../pc/kbd.o
OBJS-$(CONFIG_CONSOLE)+=	../pc/console.o
OBJS-$(CONFIG_FDD)+=		../pc/fdd.o
OBJS-$(CONFIG_MOUSE)+=		../pc/mouse.o
OBJS-$(CONFIG_RTC)+=		../pc/rtc.o

include $(SRCDIR)/mk/dev.mk
