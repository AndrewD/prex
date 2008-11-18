TARGET=	platform.o
TYPE=	OBJECT
VPATH := ../i386:$(VPATH)
OBJS=	locore.o \
	cpufunc.o \
	cpu.o \
	trap.o \
	context.o \
	interrupt.o \
	clock.o \
	diag.o \
	machdep.o

ifeq ($(CONFIG_MMU),y)
OBJS+=	mmu.o
endif

include $(SRCDIR)/mk/sys.mk
