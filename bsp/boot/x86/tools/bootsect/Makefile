include $(SRCDIR)/mk/own.mk

TARGET:=	bootsect.bin
OBJS:=		bootsect.o
LDSCRIPT:=	bootsect.ld

include $(SRCDIR)/mk/common.mk

$(TARGET): $(OBJS) $(LDSCRIPT)
	$(LD) $(LDFLAGS) $(OUTPUT_OPTION) $(OBJS)
	$(OBJCOPY) -O binary -R .note -R .comment -S $@
