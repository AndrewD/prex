
SRCS:=		arm/arch/locore.S \
		arm/gba/locore_gba.S \
		arm/arch/cpufunc.S \
		arm/arch/cpu.c \
		arm/arch/trap.c \
		arm/arch/context.c \
		arm/gba/interrupt.c \
		arm/gba/clock.c \
		arm/gba/machdep.c

ifeq ($(DEBUG),1)
SRCS+=		arm/gba/diag.c
endif
