VPATH:=	$(SRCDIR)/usr/lib/prex/syscalls:$(VPATH)

SRCS+=	$(SRCDIR)/usr/arch/$(ARCH)/_systrap.S \
	object_create.S object_destroy.S object_lookup.S \
	msg_send.S msg_receive.S msg_reply.S \
	vm_allocate.S vm_free.S vm_attribute.S vm_map.S \
	task_create.S task_terminate.S task_self.S \
	task_suspend.S task_resume.S task_setname.S \
	task_setcap.S task_chkcap.S \
	thread_create.S thread_terminate.S thread_load.S thread_self.S \
	thread_yield.S thread_suspend.S thread_resume.S thread_schedparam.S \
	thread_getpri.c thread_setpri.c \
	thread_getpolicy.c thread_setpolicy.c \
	timer_sleep.S timer_alarm.S timer_periodic.S \
	_timer_waitperiod.S timer_waitperiod.c \
	exception_setup.S exception_return.S \
	exception_raise.S exception_wait.S \
	device_open.S device_close.S device_read.S device_write.S \
	device_ioctl.S \
	mutex_init.S mutex_destroy.S mutex_trylock.S mutex_unlock.S \
	_mutex_lock.S mutex_lock.c \
	cond_init.S cond_destroy.S cond_signal.S cond_broadcast.S \
	_cond_wait.S cond_wait.c \
	sem_init.S sem_destroy.S sem_trywait.S sem_post.S sem_getvalue.S \
	_sem_wait.S sem_wait.c \
	sys_log.S sys_info.S sys_panic.S sys_time.S \
	sys_debug.S

