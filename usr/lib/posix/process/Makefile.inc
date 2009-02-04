VPATH:=	$(SRCDIR)/usr/lib/posix/process:$(VPATH)

SRCS+=	__process.c kill.c killpg.c \
	getpgid.c getpgrp.c getpid.c getppid.c \
	getuid.c geteuid.c setuid.c seteuid.c \
	getgid.c setgid.c getegid.c setegid.c \
	setsid.c getsid.c \
	nice.c setpgid.c setpgrp.c sleep.c wait.c waitpid.c _exit.c \
	getpriority.c setpriority.c gettask.c fork.c
