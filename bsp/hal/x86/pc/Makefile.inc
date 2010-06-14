
SRCS:=		x86/arch/locore.S \
		x86/arch/cpufunc.S \
		x86/arch/cpu.c \
		x86/arch/trap.c \
		x86/arch/context.c \
		x86/pc/interrupt.c \
		x86/pc/clock.c \
		x86/pc/machdep.c

ifeq ($(CONFIG_MMU),y)
SRCS+=		x86/arch/mmu.c
endif
ifeq ($(DEBUG),1)
SRCS+=		x86/pc/diag.c
endif
