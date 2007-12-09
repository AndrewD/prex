#
# Make options for Boot Loader
#
INC_FLAGS = -I$(PREX_SRC)/conf \
	-I$(PREX_SRC)/boot/arch/$(PREX_ARCH)/include \
	-I$(PREX_SRC)/boot/include

ASFLAGS = $(INC_FLAGS)
CFLAGS = $(INC_FLAGS) -nostdinc -fno-builtin
CPPFLAGS = $(INC_FLAGS)
LDFLAGS = -static -nostdlib

include $(PREX_SRC)/mk/Makefile.inc
