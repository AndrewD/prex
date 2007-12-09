TARGET	= dev.ko
TYPE	= DRIVER
SUBDIRS = lib arch core gen power
OBJS	= \
	./arch/$(PREX_ARCH)/$(PREX_PLATFORM)/dev.o \
	./core/core.o \
	./gen/gen.o \
	./power/power.o
LIBS	= ./lib/libdev.a
#DISASM	= dev.lst
#MAP	= dev.map
LD_SCRIPT = ./arch/$(PREX_ARCH)/$(PREX_PLATFORM)/dev.ld
include $(PREX_SRC)/make/dev.mk
