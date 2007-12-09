include $(PREX_SRC)/mk/own.mk

TARGET	= bootldr
TYPE	= BINARY
SUBDIRS	= lib arch common
OBJS	= ./arch/$(PREX_ARCH)/$(PREX_PLATFORM)/boot.o \
	./common/common.o
LIBS	= ./lib/libboot.a $(LIBGCC_PATH)libgcc.a
LD_SCRIPT= ./arch/$(PREX_ARCH)/$(PREX_PLATFORM)/boot.ld
#DISASM	= bootldr.lst
#MAP	= bootldr.map

include $(PREX_SRC)/mk/boot.mk