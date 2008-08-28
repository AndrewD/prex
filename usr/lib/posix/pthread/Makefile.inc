VPATH:=	$(SRCDIR)/usr/lib/posix/pthread:$(VPATH)

SRCS+=	pthread_create.c pthread_attr_init.c \
	pthread_attr_setschedparam.c pthread_attr_getschedparam.c \
	pthread_attr_setschedpolicy.c pthread_attr_getschedpolicy.c \
	pthread_attr_setstacksize.c pthread_attr_getstacksize.c \
	pthread_attr_setname.c
