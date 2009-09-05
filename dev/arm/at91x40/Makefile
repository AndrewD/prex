include $(SRCDIR)/mk/own.mk

TARGET=		dev.o
TYPE=		OBJECT
OBJS-y=		../arm/start.o pio.o

OBJS-$(CONFIG_SERIAL)+= serial.o
OBJS-$(CONFIG_WATCHDOG)+= wdt.o
OBJS-$(CONFIG_LED)+= led.o

include $(SRCDIR)/mk/dev.mk
