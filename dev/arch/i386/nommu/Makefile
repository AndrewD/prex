include $(PREX_SRC)/mk/own.mk

TARGET	= dev.o
TYPE	= OBJECT
OBJS-y	= ../i386/start.o ../pc/dma.o ../pc/platform.o ../i386/delay.o

OBJS-$(CONFIG_CPUFREQ) += ../i386/cpu.o
OBJS-$(CONFIG_KEYBOARD) += ../pc/kbd.o
OBJS-$(CONFIG_CONSOLE) += ../pc/console.o
OBJS-$(CONFIG_FDD) += ../pc/fdd.o
OBJS-$(CONFIG_MOUSE) += ../pc/mouse.o
OBJS-$(CONFIG_RTC) += ../pc/rtc.o


include $(PREX_SRC)/mk/dev.mk
