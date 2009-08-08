include $(SRCDIR)/mk/own.mk

TARGET=	dev.o
TYPE=		OBJECT

OBJS-$(CONFIG_SERIAL)+=	serial.o
OBJS-$(CONFIG_RTC)+=	rtc.o
OBJS-$(CONFIG_MMC)+=	mmc.o

include $(SRCDIR)/mk/dev.mk
