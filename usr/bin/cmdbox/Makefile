PROG=	cmdbox
SRCS=	main.c cmd_conf.c
CFLAGS+= -DCMDBOX

VPATH := ../cal:../cat:../clear:../cp:../date:../dmesg:../echo:\
	../free:../head:../hostname:../kill:../ls:../mkdir:../mkfifo:\
	../mount:../mv:../nice:../ps:../pwd:../reboot:../rm:\
	../rmdir:../shutdown:../sleep:../sync:../test:../touch:\
	../umount:../uname:../sh:$(VPATH)

SRCS-$(CONFIG_CMD_CAL)+=	cal.c
SRCS-$(CONFIG_CMD_CAT)+=	cat.c
SRCS-$(CONFIG_CMD_CLEAR)+=	clear.c
SRCS-$(CONFIG_CMD_CP)+=		cp.c
SRCS-$(CONFIG_CMD_DATE)+=	date.c
SRCS-$(CONFIG_CMD_DMESG)+=	dmesg.c
SRCS-$(CONFIG_CMD_ECHO)+=	echo.c
SRCS-$(CONFIG_CMD_FREE)+=	free.c
SRCS-$(CONFIG_CMD_HEAD)+=	head.c
SRCS-$(CONFIG_CMD_HOSTNAME)+=	hostname.c
SRCS-$(CONFIG_CMD_KILL)+=	kill.c
SRCS-$(CONFIG_CMD_LS)+=		ls.c
SRCS-$(CONFIG_CMD_MKDIR)+=	mkdir.c
SRCS-$(CONFIG_CMD_MKFIFO)+=	mkfifo.c
SRCS-$(CONFIG_CMD_MOUNT)+=	mount.c
SRCS-$(CONFIG_CMD_MV)+=		mv.c
SRCS-$(CONFIG_CMD_NICE)+=	nice.c
SRCS-$(CONFIG_CMD_PS)+=		ps.c
SRCS-$(CONFIG_CMD_PWD)+=	pwd.c
SRCS-$(CONFIG_CMD_REBOOT)+=	reboot.c
SRCS-$(CONFIG_CMD_RM)+=		rm.c
SRCS-$(CONFIG_CMD_RMDIR)+=	rmdir.c
SRCS-$(CONFIG_CMD_SHUTDOWN)+=	shutdown.c
SRCS-$(CONFIG_CMD_SLEEP)+=	sleep.c
SRCS-$(CONFIG_CMD_SYNC)+=	sync.c
SRCS-$(CONFIG_CMD_TEST)+=	test.c
SRCS-$(CONFIG_CMD_TOUCH)+=	touch.c
SRCS-$(CONFIG_CMD_UMOUNT)+=	umount.c
SRCS-$(CONFIG_CMD_UNAME)+=	uname.c
SRCS-$(CONFIG_CMD_SH)+=		sh.c cd.c exec.c cmds.c

SRCS+= $(SRCS-y)

include $(SRCDIR)/mk/prog.mk
