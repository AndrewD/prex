ifndef BUILDDIR
export BUILDDIR:=$(shell pwd)
endif
-include $(BUILDDIR)/conf/config.mk

ifndef SRCDIR
ifdef MAKECMDGOALS
$(MAKECMDGOALS):
else
all:
endif
	@echo Error: $(BUILDDIR) is not configured
	@exit 1

else
ifdef FROMDIR
SUBDIR= $(FROMDIR)
else
SUBDIR=	boot dev sys usr mk
endif
include $(SRCDIR)/mk/subdir.mk

#for parallel make
ifndef FROMDIR
sys: dev
mk: boot dev sys usr
endif

endif
