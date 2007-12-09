include $(PREX_SRC)/mk/own.mk

ifeq ($(CONFIG_PM),y)
TARGET	= power.o
TYPE	= OBJECT
OBJS-y	= pm.o
OBJS-$(CONFIG_CPUFREQ) += cpufreq.o dvs.o
endif

include $(PREX_SRC)/mk/dev.mk
