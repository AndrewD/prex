/*-
 * Copyright (c) 2008, Lazarenko Andrew
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <boot.h>
#include <platform.h>
#include <conf/config.h>

#define UART_BASE	0xFFFD0000
#define PIO_BASE	0xFFFF0000

/* Usart pins */
#define TX_PIN		14
#define RX_PIN		15

/* PIO disable register */
#define PIO_PDR		(*(volatile uint32_t *)(PIO_BASE + 0x04))

/* UART Registers */
#define UART_CR		(*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_MR		(*(volatile uint32_t *)(UART_BASE + 0x04))
#define UART_IER	(*(volatile uint32_t *)(UART_BASE + 0x08))
#define UART_IDR	(*(volatile uint32_t *)(UART_BASE + 0x0c))
#define UART_IMR	(*(volatile uint32_t *)(UART_BASE + 0x10))
#define UART_CSR	(*(volatile uint32_t *)(UART_BASE + 0x14))
#define UART_RHR	(*(volatile uint32_t *)(UART_BASE + 0x18))
#define UART_THR	(*(volatile uint32_t *)(UART_BASE + 0x1c))
#define UART_BRGR	(*(volatile uint32_t *)(UART_BASE + 0x20))
#define UART_RTOR	(*(volatile uint32_t *)(UART_BASE + 0x24))
#define UART_TTGR	(*(volatile uint32_t *)(UART_BASE + 0x28))

/* UART_CR - control register */
#define CR_RSTRX	(1 << 2)		/* Reset tx */
#define CR_RSTTX	(1 << 3)		/* Reset rx */
#define CR_RXEN		(1 << 4)		/* rx enable */
#define CR_RXDIS	(1 << 5)		/* rx disable */
#define CR_TXEN		(1 << 6)		/* tx enable */
#define CR_TXDIS	(1 << 7)		/* tx disable */
#define CR_RSTSTA	(1 << 8)		/* reset status */

/* UART_MR - mode register */
#define MR_CLKS_CLOCK	(0 << 4)		/* mcu clock */
#define MR_CLKS_FDIV1	(1 << 4)
#define MR_CLKS_SLOW	(2 << 4)
#define MR_CLKS_EXT	(3 << 4)		/* external clock */
#define MR_CHRL_5_BITS	(0 << 6)		/* 5 bit */
#define MR_CHRL_6_BITS	(1 << 6)		/* 6 bit */
#define MR_CHRL_7_BITS	(2 << 6)		/* 7 bit */
#define MR_CHRL_8_BITS	(3 << 6)		/* 8 bit */
#define MR_PAR_EVEN	(0 << 9)		/* parity even */
#define MR_PAR_ODD	(1 << 9)		/* parity odd */
#define MR_PAR_SPACE	(2 << 9)		/* parity space */
#define MR_PAR_MARK	(3 << 9)		/* parity mark */
#define MR_PAR_NONE	(4 << 9)		/* parity none */
#define MR_NBSTOP_1_BIT		(0 << 12)	/* 1 stop bit */
#define MR_NBSTOP_15_BIT	(1 << 12)	/* 1.5 stop bit */
#define MR_NBSTOP_2_BIT		(2 << 12)	/* 2 stop bit */
#define MR_MODE9		(1 << 17)	/* 9 bit */

/* UART_IER, UART_IDR, UART_IMR, UART_CSR - interrupt registers */
#define IR_RXRDY	(1 <<  0)		/* rx ready */
#define IR_TXRDY	(1 <<  1)		/* tx ready */


/*
 * Setup boot information.
 */
static void
bootinfo_setup(void)
{

	bootinfo->video.text_x = 80;
	bootinfo->video.text_y = 25;


	/*
	 * On-chip SSRAM = 256K - Bootloader/Syspage size
	 */
	bootinfo->ram[0].base = 0x4000;
	bootinfo->ram[0].size = 0x40000 - 0x4000;
	bootinfo->ram[0].type = MT_USABLE;

	/*
	 * External SRAM - 2M
	 */
	bootinfo->ram[1].base = 0x10000000;
	bootinfo->ram[1].size = 0x200000;
	bootinfo->ram[1].type = MT_USABLE;

	bootinfo->nr_rams = 2;
}

#ifdef DEBUG
#ifdef CONFIG_DIAG_SERIAL
/*
 * Put chracter to serial port
 */
static void
serial_putc(int c)
{

	while (!(UART_CSR & IR_TXRDY))
		;

	UART_THR = (uint32_t)c;
}

/*
 * Setup serial port
 */
void
serial_setup(void)
{
	/*
	 * Disable PIO on usart pins
	 */
	PIO_PDR = (1 << TX_PIN) | (1 << RX_PIN);

	UART_MR = MR_CLKS_CLOCK | MR_CHRL_8_BITS |
		MR_PAR_NONE | MR_NBSTOP_1_BIT;
	UART_RTOR = 0;
	UART_BRGR = CONFIG_MCU_FREQ / (16 * CONFIG_UART_BAUD);
	UART_CR = CR_RSTTX | CR_RSTRX | CR_RSTSTA;
	UART_CR = CR_RXEN | CR_TXEN;
}
#endif /* !CONFIG_DIAG_SERIAL */

/*
 * Print one chracter
 */
void
machine_putc(int c)
{

#ifdef CONFIG_DIAG_SERIAL
	if (c == '\n') {
		serial_putc('\r');
	}
	serial_putc(c);
#endif /* !CONFIG_DIAG_SERIAL */
}
#endif /* !DEBUG */

/*
 * Setup machine state
 */
void
machine_setup(void)
{


/*#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)*/
/*     serial_setup();*/
/*#endif*/
	bootinfo_setup();
}
