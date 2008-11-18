TARGET=		dev.o
TYPE=		OBJECT
VPATH:=		../i386:$(VPATH)
OBJS-m=		cpufunc.o dma.o

ifdef CONFIG_CPUFREQ
OBJS-$(CONFIG_PM)+=		cpu.o
endif
OBJS-$(CONFIG_KEYBOARD)+=	kbd.o
OBJS-$(CONFIG_CONSOLE)+=	console.o
OBJS-$(CONFIG_FDD)+=		fdd.o
OBJS-$(CONFIG_MOUSE)+=		mouse.o
OBJS-$(CONFIG_RTC)+=		rtc.o
OBJS-$(CONFIG_SERIAL)+=		serial.o

include $(SRCDIR)/mk/dev.mk
