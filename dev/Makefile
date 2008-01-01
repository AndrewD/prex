include $(SRCDIR)/mk/own.mk

TARGET=		dev.ko
TYPE=		DRIVER
SUBDIR=		lib $(ARCH) core gen power
OBJS=		./$(ARCH)/$(PLATFORM)/dev.o \
		./core/core.o ./gen/gen.o
ifeq ($(CONFIG_PM),y)
OBJS+=		./power/power.o
endif

LIBS=		./lib/libdev.a
LD_SCRIPT=	./$(ARCH)/$(PLATFORM)/dev.ld

include $(SRCDIR)/mk/dev.mk
