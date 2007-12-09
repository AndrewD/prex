#
# System specific configuration parameters.
#
ifndef _OWN_MK_
_OWN_MK_=1

-include $(PREX_SRC)/conf/config.mk

export PREX_SRC PREX_ARCH PREX_PLATFORM
export CROSS_COMPILE NDEBUG


ifndef PREX_SRC
@echo "Error: Please run configure at the top of source tree"
exit 1
endif

ifeq ($(PREX_ARCH),i386)
OBJCOPYFLAGS += -I elf32-i386 -O binary -R .note -R .comment -S
endif

ifeq ($(PREX_ARCH),arm)
OBJCOPYFLAGS += -I elf32-littlearm -O binary -R .note -R .comment -S
endif

ifeq ($(PREX_PLATFORM),gba)
EXTRA_CFLAGS += -mcpu=arm7tdmi -mtune=arm7tdmi
endif

endif	# !defined(_OWN_MK_)
