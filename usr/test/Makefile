#
# Test for kernel
#
include $(SRCDIR)/mk/own.mk

SUBDIR=		task thread ipc timer exception fault deadlock sem mutex \
		cap dvs ipc_mt kmon

ifeq ($(ARCH),i386)
SUBDIR+=	ioport
endif

#
# Test for driver
#
SUBDIR+=	console kbd fdd ramdisk reset time zero

#
# Test for library
#
SUBDIR+=	errno malloc stderr

#
# Test for servers
#
SUBDIR+=	fileio vfork args debug signal

include $(SRCDIR)/mk/subdir.mk
