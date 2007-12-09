TARGET	= bootldr
TYPE	= BINARY
SUBDIRS	= lib arch common
OBJS	= \
	./arch/$(PREX_ARCH)/$(PREX_PLATFORM)/boot.o \
	./common/common.o
LIBS	= ./lib/libboot.a $(LIBGCC_PATH)libgcc.a
#DISASM	= bootldr.lst
#MAP	= bootldr.map
LD_SCRIPT= ./arch/$(PREX_ARCH)/$(PREX_PLATFORM)/boot.ld
include $(PREX_SRC)/make/boot.mk
