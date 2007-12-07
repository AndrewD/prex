TARGET=		power.o
TYPE=		OBJECT
OBJS-$(CONFIG_PM)+= pm.o
ifdef CONFIG_CPUFREQ
OBJS-$(CONFIG_PM)+= cpufreq.o dvs.o
endif

include $(SRCDIR)/mk/dev.mk
