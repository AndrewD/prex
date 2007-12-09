include $(PREX_SRC)/mk/own.mk

TARGET  = fs
SUBDIRS = vfs
OBJS    = ./vfs/core.o

ifeq ($(CONFIG_DEVFS),y)
SUBDIRS += devfs
OBJS	+= ./devfs/devfs.o
endif
ifeq ($(CONFIG_RAMFS),y)
SUBDIRS += ramfs
OBJS	+= ./ramfs/ramfs.o
endif
ifeq ($(CONFIG_ARFS),y)
SUBDIRS += arfs
OBJS	+= ./arfs/arfs.o
endif

#DISASM  = fs.lst
#MAP     = fs.map
#SYMBOL   = fs.sym
include $(PREX_SRC)/mk/task.mk
