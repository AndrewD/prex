include $(PREX_SRC)/mk/own.mk

TARGET	= dev.o
TYPE	= OBJECT
OBJS-y	= ../arm/start.o platform.o keypad.o

OBJS-$(CONFIG_CONSOLE) += console.o
OBJS-$(CONFIG_KEYBOARD) += swkbd.o

include $(PREX_SRC)/mk/dev.mk
