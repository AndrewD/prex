#
# Make options to build POSIX applications.
#
INC_FLAGS = -I$(PREX_SRC)/conf \
	-I$(PREX_SRC)/user/arch/$(PREX_ARCH) \
	-I$(PREX_SRC)/user/include

ASFLAGS = $(INC_FLAGS)
CFLAGS = $(INC_FLAGS) -nostdinc
CPPFLAGS = $(INC_FLAGS) -nostdinc
LDFLAGS = -static

LD_SCRIPT = $(PREX_SRC)/user/arch/$(PREX_ARCH)/$(PREX_PLATFORM).ld

LIBC = $(PREX_SRC)/user/lib/libc.a

CRT0 = $(PREX_SRC)/user/lib/crt0.o
CFLAGS += -D_PREX_SOURCE
CPPFLAGS += -D_PREX_SOURCE
TYPE = EXEC

-include $(PREX_SRC)/user/arch/$(PREX_ARCH)/Makefile.$(PREX_PLATFORM)
include $(PREX_SRC)/mk/Makefile.inc
