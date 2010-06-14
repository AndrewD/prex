# Rules to compile kernel

include $(SRCDIR)/mk/own.mk

TARGET:=	prex

#MAP:=		prex.map
#DISASM:= 	prex.lst
#SYMBOL:= 	prex.sym

_KERNEL_:=	1
DEFS+=		KERNEL

SRCS+=		kern/main.c \
		kern/sched.c \
		kern/task.c \
		kern/thread.c \
		kern/timer.c \
		kern/sysent.c \
		kern/irq.c \
		kern/device.c \
		kern/exception.c \
		kern/system.c \
		mem/kmem.c \
		mem/page.c \
		ipc/msg.c \
		ipc/object.c \
		sync/cond.c \
		sync/mutex.c \
		sync/sem.c \
		lib/queue.c \
		lib/string.c \
		lib/vsprintf.c

ifeq ($(CONFIG_MMU),y)
SRCS+=		mem/vm.c
else
SRCS+=		mem/vm_nommu.c
endif

ifeq ($(DEBUG),1)
SRCS+=		kern/debug.c
endif

HAL:=		$(SRCDIR)/bsp/hal/hal.o
LIBSDIR+=	$(SRCDIR)/conf
INCSDIR+=	$(CURDIR)/include $(SRCDIR)/bsp/hal/$(ARCH)/include
ifndef LDSCRIPT
LDSCRIPT:=	$(SRCDIR)/bsp/hal/$(ARCH)/arch/kern.ld
endif

include $(SRCDIR)/mk/common.mk

$(TARGET): $(OBJS) $(HAL) $(LDSCRIPT)
	$(call echo-file,LD     ,$@)
	$(LD) $(LDFLAGS) $(OUTPUT_OPTION) $(HAL) $(OBJS) $(PLATFORM_LIBS)
	$(ASMGEN)
	$(SYMGEN)
	$(STRIP) -s $@
