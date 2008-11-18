TARGET=		dev.o
TYPE=		DRIVER
SUBDIR=		$(ARCH) core gen power
OBJS=		./$(ARCH)/$(PLATFORM)/dev.o \
		./core/core.o ./gen/gen.o ./power/power.o

LD_SCRIPT=	./$(ARCH)/$(ARCH)/dev.ld

#MAP   = 	dev.map
#DISASM= 	dev.lst
#SYMBOL= 	dev.sym

include $(SRCDIR)/mk/dev.mk
