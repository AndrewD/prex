include $(PREX_SRC)/mk/own.mk

ifeq ($(CONFIG_PROC),y)
SUBDIRS += proc boot
endif
ifeq ($(CONFIG_FS),y)
SUBDIRS += fs
endif
ifeq ($(CONFIG_EXEC),y)
SUBDIRS += exec
endif

include $(PREX_SRC)/mk/Makefile.inc
