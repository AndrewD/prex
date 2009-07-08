ifndef _OWN_MK_
_OWN_MK_=1

include $(BUILDDIR)/conf/config.mk

ifndef SRCDIR
@echo "Error: Please run configure at the top of source tree"
exit 1
endif

-include $(SRCDIR)/conf/$(ARCH)/Makefile.$(ARCH)
-include $(SRCDIR)/conf/$(ARCH)/Makefile.$(ARCH)-$(PLATFORM)

endif
