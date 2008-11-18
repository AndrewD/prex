SUBDIR=	lib server bin sample test

include $(SRCDIR)/mk/subdir.mk

# parallel make: ensure lib built first
$(filter-out lib,$(SUBDIR)): lib
