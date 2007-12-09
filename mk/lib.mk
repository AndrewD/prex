#
# Make options to build user mode libraries.
#
include $(PREX_SRC)/mk/own.mk

TYPE = LIBRARY

INC_FLAGS += -I$(PREX_SRC)/conf \
	-I$(PREX_SRC)/user/arch/$(PREX_ARCH) \
	-I$(PREX_SRC)/user/include

ASFLAGS = $(INC_FLAGS)
CFLAGS = $(INC_FLAGS) -nostdinc
CPPFLAGS = $(INC_FLAGS) -nostdinc
LDFLAGS = -static
EXTRA_CFLAGS += -D_PREX_SOURCE
EXTRA_CPPFLAGS += -D_PREX_SOURCE

include $(PREX_SRC)/mk/Makefile.inc
