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
ifdef FROMDIR
SUBDIR= $(FROMDIR)
else
SUBDIR=	boot dev sys usr mk
endif
include $(SRCDIR)/mk/subdir.mk
endif
