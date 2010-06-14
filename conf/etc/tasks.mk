#
# Boot tasks
#

ifeq ($(CONFIG_POSIX),y)
TASKS+= 	$(SRCDIR)/usr/server/boot/boot
TASKS+= 	$(SRCDIR)/usr/server/proc/proc
TASKS+= 	$(SRCDIR)/usr/server/exec/exec
ifeq ($(CONFIG_PM),y)
TASKS+= 	$(SRCDIR)/usr/server/pow/pow
endif
TASKS+= 	$(SRCDIR)/usr/server/fs/fs
endif

#TASKS+= 	$(SRCDIR)/usr/sample/alarm/alarm.rt
#TASKS+= 	$(SRCDIR)/usr/sample/bench/bench.rt
#TASKS+= 	$(SRCDIR)/usr/sample/ipc/ipc.rt
#TASKS+= 	$(SRCDIR)/usr/sample/mutex/mutex.rt
#TASKS+= 	$(SRCDIR)/usr/sample/sem/sem.rt
#TASKS+= 	$(SRCDIR)/usr/sample/task/task.rt
#TASKS+= 	$(SRCDIR)/usr/sample/thread/thread.rt
#TASKS+= 	$(SRCDIR)/usr/sample/balls/balls.rt

#TASKS+= 	$(SRCDIR)/usr/test/console/console.rt
#TASKS+= 	$(SRCDIR)/usr/test/deadlock/deadlock.rt
#TASKS+= 	$(SRCDIR)/usr/test/errno/errno.rt
#TASKS+= 	$(SRCDIR)/usr/test/exception/exception.rt
#TASKS+= 	$(SRCDIR)/usr/test/fault/fault.rt
#TASKS+= 	$(SRCDIR)/usr/test/fdd/fdd.rt
#TASKS+= 	$(SRCDIR)/usr/test/ipc/ipc.rt
#TASKS+= 	$(SRCDIR)/usr/test/ipc_mt/ipc_mt.rt
#TASKS+= 	$(SRCDIR)/usr/test/kbd/kbd.rt
#TASKS+= 	$(SRCDIR)/usr/test/kmon/kmon.rt
#TASKS+= 	$(SRCDIR)/usr/test/malloc/malloc.rt
#TASKS+= 	$(SRCDIR)/usr/test/mutex/mutex.rt
#TASKS+= 	$(SRCDIR)/usr/test/sem/sem.rt
#TASKS+= 	$(SRCDIR)/usr/test/task/task.rt
#TASKS+= 	$(SRCDIR)/usr/test/thread/thread.rt
#TASKS+= 	$(SRCDIR)/usr/test/time/time.rt
#TASKS+= 	$(SRCDIR)/usr/test/timer/timer.rt
#TASKS+= 	$(SRCDIR)/usr/test/ramdisk/ramdisk.rt
#TASKS+= 	$(SRCDIR)/usr/test/reset/reset.rt
#TASKS+= 	$(SRCDIR)/usr/test/zero/zero.rt

#TASKS+= 	$(SRCDIR)/usr/test/cpufreq/cpufreq.rt
#TASKS+= 	$(SRCDIR)/usr/sample/cpumon/cpumon.rt

#TASKS+= 	$(SRCDIR)/usr/server/fs/fs
#TASKS+= 	$(SRCDIR)/usr/test/fileio/fileio.rt
