TARGET=	mem.o
TYPE=	OBJECT
OBJS=	page.o kmem.o
OBJS-$(CONFIG_KMEM_PROTECT)+= kpage.o

ifeq ($(CONFIG_MMU),y)
OBJS+=	vm.o
else
OBJS+=	vm_nommu.o
endif

include $(SRCDIR)/mk/sys.mk
