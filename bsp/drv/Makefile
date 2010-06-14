include $(SRCDIR)/mk/own.mk

TARGET:=	drv.ko

#MAP:= 		drv.map
#DISASM:= 	drv.lst
#SYMBOL:= 	drv.sym

_RELOC_OBJ_:=	1
_KERNEL_:=	1
DEFS+=		KERNEL

include $(CURDIR)/dev/base/Makefile.inc
include $(CURDIR)/dev/block/Makefile.inc
include $(CURDIR)/dev/cpufreq/Makefile.inc
include $(CURDIR)/dev/dma/Makefile.inc
include $(CURDIR)/dev/input/Makefile.inc
include $(CURDIR)/dev/misc/Makefile.inc
include $(CURDIR)/dev/rtc/Makefile.inc
include $(CURDIR)/dev/serial/Makefile.inc
include $(CURDIR)/dev/video/Makefile.inc

include $(CURDIR)/lib/Makefile.inc
-include $(CURDIR)/$(ARCH)/arch/Makefile.inc
-include $(CURDIR)/$(ARCH)/$(PLATFORM)/Makefile.inc

LIBSDIR+=	$(SRCDIR)/conf
INCSDIR+=	$(CURDIR)/include $(CURDIR)/$(ARCH)/include
ifndef LDSCRIPT
LDSCRIPT:=	$(CURDIR)/$(ARCH)/arch/driver.ld
endif

include $(SRCDIR)/mk/common.mk

$(TARGET):  $(OBJS) $(LIBS) $(LDSCRIPT)
	$(call echo-file,LD     ,$@)
	$(LD) $(LDFLAGS_S) $(OUTPUT_OPTION) $(OBJS) $(LIBS) $(PLATFORM_LIBS)
	$(RM) $@
	$(LD) $(LDFLAGS) $(OUTPUT_OPTION) $(OBJS) $(LIBS) $(PLATFORM_LIBS)
	$(ASMGEN)
	$(SYMGEN)
	$(STRIP) --strip-debug --strip-unneeded $@
