#
# Test for kernel
#
SUBDIRS	=  task thread ipc timer except fault deadlock sem mutex \
	cap dvs ipc_mt

ifeq ($(PREX_ARCH),i386)
SUBDIRS	+= ioport
endif

#
# Test for driver
#
SUBDIRS += kbd time zero fdd console ramdisk

#
# Test for library
#
SUBDIRS += errno malloc

include $(PREX_SRC)/mk/Makefile.inc
