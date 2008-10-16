TARGET=		platform.o
TYPE=		OBJECT
VPATH := ../pc:../i386:$(VPATH)
OBJS=		locore.o \
		cpu.o \
		trap.o \
		context.o \
		interrupt.o \
		clock.o \
		diag.o \
		machdep.o

ifeq ($(CONFIG_GDB),y)
OBJS+=	gdb_stub.o \
	gdb_glue.o
endif

include $(SRCDIR)/mk/sys.mk
