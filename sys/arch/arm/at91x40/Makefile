include $(SRCDIR)/mk/own.mk

TARGET=		platform.o
TYPE=		OBJECT
VPATH := ../arm:$(VPATH)
OBJS=		locore.o cpufunc.o interrupt.o \
		context.o trap.o \
		cpu.o machdep.o clock.o diag.o

include $(SRCDIR)/mk/sys.mk
