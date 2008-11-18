TARGET=		platform.o
TYPE=		OBJECT
VPATH:=		../arm:$(VPATH)
OBJS=		locore.o cpufunc.o interrupt.o \
		context.o trap.o \
		cpu.o machdep.o clock.o diag.o

ifeq ($(CONFIG_MMU),y)
OBJS+=	mmu.o
endif

include $(SRCDIR)/mk/sys.mk
