include $(SRCDIR)/mk/own.mk

TARGET=		dev.ko
TYPE=		DRIVER
SUBDIR=		$(ARCH) core gen power
OBJS=		./$(ARCH)/$(PLATFORM)/dev.o \
		./core/core.o ./gen/gen.o ./power/power.o

LD_SCRIPT=	./$(ARCH)/$(PLATFORM)/dev.ld

include $(SRCDIR)/mk/dev.mk
