#
# Make options for driver module
#
include $(PREX_SRC)/mk/own.mk

INC_FLAGS = -I$(PREX_SRC)/conf \
	-I$(PREX_SRC)/dev/arch/$(PREX_ARCH)/include \
	-I$(PREX_SRC)/dev/include

ASFLAGS = $(INC_FLAGS)
CFLAGS = $(INC_FLAGS) -nostdinc -fno-builtin
CPPFLAGS = $(INC_FLAGS)
LDFLAGS = -static -nostdlib -r

ifeq ($(KTRACE),1)
CFLAGS += -finstrument-functions
endif

include $(PREX_SRC)/mk/Makefile.inc
