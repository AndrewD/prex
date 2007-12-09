#
# Make rules to build object module.
#
INC_FLAGS += -I$(PREX_SRC)/conf \
	-I$(PREX_SRC)/user/arch/$(PREX_ARCH) \
	-I$(PREX_SRC)/user/include

ASFLAGS = $(INC_FLAGS)
CFLAGS = $(INC_FLAGS) -nostdinc
CPPFLAGS = $(INC_FLAGS) -nostdinc
LDFLAGS = -static

CFLAGS += -D_PREX_SOURCE
CPPFLAGS += -D_PREX_SOURCE
TYPE    = OBJECT

include $(PREX_SRC)/make/Makefile.inc
