#
# Make options to build native Prex task.
#
include $(PREX_SRC)/mk/own.mk

INC_FLAGS = -I$(PREX_SRC)/conf \
	-I$(PREX_SRC)/user/arch/$(PREX_ARCH) \
	-I$(PREX_SRC)/user/include

ASFLAGS = $(INC_FLAGS)
CFLAGS = $(INC_FLAGS) -nostdinc
CPPFLAGS = $(INC_FLAGS) -nostdinc
LDFLAGS = -static

LD_SCRIPT = $(PREX_SRC)/user/arch/$(PREX_ARCH)/$(PREX_PLATFORM).ld

LIBC = $(PREX_SRC)/user/lib/libsa.a $(PREX_SRC)/user/lib/libc.a

CRT0 = $(PREX_SRC)/user/lib/crt0.o
CFLAGS += -D_PREX_SOURCE -D_PREX_NATIVE_TASK
CPPFLAGS += -D_PREX_SOURCE -D_PREX_NATIVE_TASK
TYPE = EXEC

ifndef OBJS
ifdef SRCS
OBJS = $(SRCS:.c=.o)
else
OBJS = $(TARGET).o
endif
endif

-include $(PREX_SRC)/user/arch/$(PREX_ARCH)/Makefile.$(PREX_PLATFORM)
include $(PREX_SRC)/mk/Makefile.inc
