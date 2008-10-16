TARGET=		dev.o
TYPE=		OBJECT
VPATH := ../pc:../i386:$(VPATH)
OBJS-m=		dma.o machdep.o delay.o

ifdef CONFIG_CPUFREQ
OBJS-$(CONFIG_PM)+=		cpu.o
endif
OBJS-$(CONFIG_KEYBOARD)+=	kbd.o
OBJS-$(CONFIG_CONSOLE)+=	console.o
OBJS-$(CONFIG_FDD)+=		fdd.o
OBJS-$(CONFIG_MOUSE)+=		mouse.o
OBJS-$(CONFIG_RTC)+=		rtc.o

include $(SRCDIR)/mk/dev.mk
