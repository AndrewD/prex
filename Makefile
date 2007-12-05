-include ./conf/config.mk
ifndef SRCDIR
ifdef MAKECMDGOALS
$(MAKECMDGOALS):
else
all:
endif
	@echo "Error: Please run configure at the top of source tree"
	exit 1
else
export SRCDIR
SUBDIR=	boot dev sys usr mk
include $(SRCDIR)/mk/subdir.mk
endif
