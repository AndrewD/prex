
SRCS:=		arm/arch/locore.S \
		arm/arch/cpufunc.S \
		arm/arch/cpu.c \
		arm/arch/trap.c \
		arm/arch/context.c \
		arm/integrator/interrupt.c \
		arm/integrator/clock.c \
		arm/integrator/machdep.c

ifeq ($(CONFIG_MMU),y)
SRCS+=		arm/arch/mmu.c
endif
ifeq ($(DEBUG),1)
SRCS+=		arm/integrator/diag.c
endif
