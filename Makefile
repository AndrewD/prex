include ./conf/config.mk
SUBDIRS	= boot dev sys user mk
CLEANS = conf/config.h conf/config.mk
include $(PREX_SRC)/mk/Makefile.inc
