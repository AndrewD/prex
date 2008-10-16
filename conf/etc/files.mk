#
# Driver modules
#
DRIVER=		$(BUILDDIR)/dev/dev.ko

#
# Boot tasks
#
BOOTTASKS+=	$(BUILDDIR)/usr/server/boot/boot
BOOTTASKS+=	$(BUILDDIR)/usr/server/proc/proc
BOOTTASKS+=	$(BUILDDIR)/usr/server/fs/fs
BOOTTASKS+=	$(BUILDDIR)/usr/server/exec/exec

#BOOTTASKS+=	$(BUILDDIR)/usr/sample/alarm/alarm
#BOOTTASKS+=	$(BUILDDIR)/usr/sample/bench/bench
#BOOTTASKS+=	$(BUILDDIR)/usr/sample/ipc/ipc
#BOOTTASKS+=	$(BUILDDIR)/usr/sample/mutex/mutex
#BOOTTASKS+=	$(BUILDDIR)/usr/sample/sem/sem
#BOOTTASKS+=	$(BUILDDIR)/usr/sample/task/task
#BOOTTASKS+=	$(BUILDDIR)/usr/sample/thread/thread
#BOOTTASKS+=	$(BUILDDIR)/usr/sample/balls/balls

#BOOTTASKS+=	$(BUILDDIR)/usr/test/cap/cap
#BOOTTASKS+=	$(BUILDDIR)/usr/test/console/console
#BOOTTASKS+=	$(BUILDDIR)/usr/test/deadlock/deadlock
#BOOTTASKS+=	$(BUILDDIR)/usr/test/errno/errno
#BOOTTASKS+=	$(BUILDDIR)/usr/test/exception/exception
#BOOTTASKS+=	$(BUILDDIR)/usr/test/fault/fault
#BOOTTASKS+=	$(BUILDDIR)/usr/test/fdd/fdd
#BOOTTASKS+=	$(BUILDDIR)/usr/test/ioport/ioport
#BOOTTASKS+=	$(BUILDDIR)/usr/test/ipc/ipc
#BOOTTASKS+=	$(BUILDDIR)/usr/test/ipc_mt/ipc_mt
#BOOTTASKS+=	$(BUILDDIR)/usr/test/kbd/kbd
#BOOTTASKS+=	$(BUILDDIR)/usr/test/kmon/kmon
#BOOTTASKS+=	$(BUILDDIR)/usr/test/malloc/malloc
#BOOTTASKS+=	$(BUILDDIR)/usr/test/mutex/mutex
#BOOTTASKS+=	$(BUILDDIR)/usr/test/sem/sem
#BOOTTASKS+=	$(BUILDDIR)/usr/test/task/task
#BOOTTASKS+=	$(BUILDDIR)/usr/test/thread/thread
#BOOTTASKS+=	$(BUILDDIR)/usr/test/time/time
#BOOTTASKS+=	$(BUILDDIR)/usr/test/timer/timer
#BOOTTASKS+=	$(BUILDDIR)/usr/test/zero/zero
#BOOTTASKS+=	$(BUILDDIR)/usr/test/ramdisk/ramdisk
#BOOTTASKS+=	$(BUILDDIR)/usr/test/reset/reset

#BOOTTASKS+=	$(BUILDDIR)/usr/test/dvs/dvs
#BOOTTASKS+=	$(BUILDDIR)/usr/sample/cpumon/cpumon

#BOOTTASKS+=	$(BUILDDIR)/usr/server/fs/fs
#BOOTTASKS+=	$(BUILDDIR)/usr/test/fileio/fileio


#
# Files in RAM disk (stored in /boot)
#
BOOTFILES+=	$(BUILDDIR)/usr/bin/init/init
BOOTFILES+=	$(BUILDDIR)/usr/bin/cmdbox/cmdbox
BOOTFILES+=	$(SRCDIR)/doc/LICENSE
BOOTFILES+=	$(BUILDDIR)/usr/sample/hello/hello

#BOOTFILES+=	$(BUILDDIR)/usr/test/args/args
#BOOTFILES+=	$(BUILDDIR)/usr/test/debug/debug
#BOOTFILES+=	$(BUILDDIR)/usr/test/stderr/stderr
#BOOTFILES+=	$(BUILDDIR)/usr/test/signal/signal
#BOOTFILES+=	$(BUILDDIR)/usr/test/vfork/vfork
