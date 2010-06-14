include $(SRCDIR)/mk/own.mk

PROG:=		exec
SRCS:=		main.c exec_execve.c exec_conf.c exec_cap.c \
		exec_elf.c exec_script.c \
		$(SRCDIR)/usr/arch/$(ARCH)/elf_reloc.c

#MAP:=		exec.map
#DISASM:= 	exec.lst
#SYMBOL:= 	exec.sym

include $(SRCDIR)/mk/prog.mk
