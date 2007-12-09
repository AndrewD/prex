include $(PREX_SRC)/mk/own.mk

TARGET	= prex
TYPE	= KERNEL
SUBDIRS = lib arch kern mem ipc sync
OBJS	= ./arch/$(PREX_ARCH)/$(PREX_PLATFORM)/platform.o \
	./kern/kern.o ./mem/mem.o ./ipc/ipc.o ./sync/sync.o
LIBS	= ./lib/libkern.a
LD_SCRIPT = ./arch/$(PREX_ARCH)/$(PREX_PLATFORM)/kernel.ld

#MAP	= prex.map
#DISASM	= prex.lst
#SYMBOL	= prex.sym

include $(PREX_SRC)/mk/sys.mk
