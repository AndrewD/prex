#
# Driver modules
#
DRIVER=		$(SRCDIR)/dev/dev.ko

#
# Boot tasks
#
BOOTTASKS+=	$(SRCDIR)/usr/server/boot/boot
BOOTTASKS+=	$(SRCDIR)/usr/server/proc/proc
BOOTTASKS+=	$(SRCDIR)/usr/server/fs/fs
BOOTTASKS+=	$(SRCDIR)/usr/server/exec/exec

#BOOTTASKS+=	$(SRCDIR)/usr/sample/alarm/alarm
#BOOTTASKS+=	$(SRCDIR)/usr/sample/bench/bench
#BOOTTASKS+=	$(SRCDIR)/usr/sample/ipc/ipc
#BOOTTASKS+=	$(SRCDIR)/usr/sample/mutex/mutex
#BOOTTASKS+=	$(SRCDIR)/usr/sample/sem/sem
#BOOTTASKS+=	$(SRCDIR)/usr/sample/task/task
#BOOTTASKS+=	$(SRCDIR)/usr/sample/thread/thread
#BOOTTASKS+=	$(SRCDIR)/usr/sample/balls/balls

#BOOTTASKS+=	$(SRCDIR)/usr/test/cap/cap
#BOOTTASKS+=	$(SRCDIR)/usr/test/console/console
#BOOTTASKS+=	$(SRCDIR)/usr/test/deadlock/deadlock
#BOOTTASKS+=	$(SRCDIR)/usr/test/errno/errno
#BOOTTASKS+=	$(SRCDIR)/usr/test/exception/exception
#BOOTTASKS+=	$(SRCDIR)/usr/test/fault/fault
#BOOTTASKS+=	$(SRCDIR)/usr/test/fdd/fdd
#BOOTTASKS+=	$(SRCDIR)/usr/test/ioport/ioport
#BOOTTASKS+=	$(SRCDIR)/usr/test/ipc/ipc
#BOOTTASKS+=	$(SRCDIR)/usr/test/ipc_mt/ipc_mt
#BOOTTASKS+=	$(SRCDIR)/usr/test/kbd/kbd
#BOOTTASKS+=	$(SRCDIR)/usr/test/kmon/kmon
#BOOTTASKS+=	$(SRCDIR)/usr/test/malloc/malloc
#BOOTTASKS+=	$(SRCDIR)/usr/test/mutex/mutex
#BOOTTASKS+=	$(SRCDIR)/usr/test/sem/sem
#BOOTTASKS+=	$(SRCDIR)/usr/test/task/task
#BOOTTASKS+=	$(SRCDIR)/usr/test/thread/thread
#BOOTTASKS+=	$(SRCDIR)/usr/test/time/time
#BOOTTASKS+=	$(SRCDIR)/usr/test/timer/timer
#BOOTTASKS+=	$(SRCDIR)/usr/test/zero/zero
#BOOTTASKS+=	$(SRCDIR)/usr/test/ramdisk/ramdisk
#BOOTTASKS+=	$(SRCDIR)/usr/test/reset/reset

#BOOTTASKS+=	$(SRCDIR)/usr/test/dvs/dvs
#BOOTTASKS+=	$(SRCDIR)/usr/sample/cpumon/cpumon

#BOOTTASKS+=	$(SRCDIR)/usr/server/fs/fs
#BOOTTASKS+=	$(SRCDIR)/usr/test/fileio/fileio


#
# Files in RAM disk (stored in /boot)
#
BOOTFILES+=	$(SRCDIR)/usr/bin/init/init
BOOTFILES+=	$(SRCDIR)/usr/bin/cmdbox/cmdbox
BOOTFILES+=	$(SRCDIR)/doc/LICENSE
BOOTFILES+=	$(SRCDIR)/usr/sample/hello/hello

#BOOTFILES+=	$(SRCDIR)/usr/test/args/args
#BOOTFILES+=	$(SRCDIR)/usr/test/debug/debug
#BOOTFILES+=	$(SRCDIR)/usr/test/stderr/stderr
#BOOTFILES+=	$(SRCDIR)/usr/test/signal/signal
#BOOTFILES+=	$(SRCDIR)/usr/test/vfork/vfork
