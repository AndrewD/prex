#
# Test for kernel
#
SUBDIR=		task thread ipc timer exception fault deadlock sem mutex \
		cap dvs ipc_mt kmon

#
# Test for driver
#
SUBDIR+=	console kbd fdd ramdisk reset time zero led

#
# Test for library
#
SUBDIR+=	errno malloc stderr

#
# Test for servers
#
SUBDIR+=	fileio vfork args debug signal fifo pipe fifo2

include $(SRCDIR)/mk/subdir.mk
