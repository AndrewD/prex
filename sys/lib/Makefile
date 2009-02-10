TARGET=	libkern.a
TYPE=	LIBRARY
OBJS=	queue.o vsprintf.o atol.o \
	htonl.o htons.o ntohl.o ntohs.o \
	strncpy.o strlcpy.o strncmp.o strnlen.o memcpy.o memset.o
OBJS-$(CONFIG_DELAY)+=	delay.o

include $(SRCDIR)/mk/sys.mk
