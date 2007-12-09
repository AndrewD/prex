#
# Make options for kernel
#
INC_FLAGS = -I$(PREX_SRC)/conf \
	-I$(PREX_SRC)/sys/arch/$(PREX_ARCH)/include \
	-I$(PREX_SRC)/sys/include

ASFLAGS = $(INC_FLAGS)
CFLAGS = $(INC_FLAGS) -nostdinc -fno-builtin
CPPFLAGS = $(INC_FLAGS)
LDFLAGS = -static -nostdlib

ifeq ($(KTRACE),1)
CFLAGS += -finstrument-functions
endif

include $(PREX_SRC)/mk/Makefile.inc
