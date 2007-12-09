/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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

/*
 * kbd.c - keyboard driver for PC
 */

#include <driver.h>
#include <keycode.h>
#include <pm.h>
#include <io.h>
#include <debug.h>
#include "kmc.h"

/* Parameters */
#define KBDQ_SIZE	256
#define KBD_IRQ		1

static int kbd_init();
static int kbd_open();
static int kbd_close();
static int kbd_read();

#ifdef CONFIG_KEYBOARD
/*
 * Driver structure
 */
struct driver kbd_drv __driver_entry = {
	/* name */	"PC/AT Keyboard",
	/* order */	4,
	/* init */	kbd_init,
};
#endif

static struct devio kbd_io = {
	/* open */	kbd_open,
	/* close */	kbd_close,
	/* read */	kbd_read,
	/* write */	NULL,
	/* ioctl */	NULL,
	/* event */	NULL,
};

/*
 * Key map
 */
static const u_char key_map[] = {
	0,      K_ESC,  '1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    K_BKSP, K_TAB,
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    K_ENTR, K_CTRL, 'a',    's',
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'',   '`',    K_SHFT, '\\',   'z',    'x',    'c',    'v',
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHFT, '*',
	K_ALT,  ' ',    K_CAPS, K_F1,   K_F2,   K_F3,   K_F4,   K_F5,
	K_F6,   K_F7,   K_F8,   K_F9,   K_F10,  0,      0,      K_HOME,
	K_UP,   K_PGUP, 0,      K_LEFT, 0,      K_RGHT, 0,      K_END,
	K_DOWN, K_PGDN, K_INS,  K_DEL,  K_F11,  K_F12
};

#define KEY_MAX (sizeof(key_map) / sizeof(u_char))

static const u_char shift_map[] = {
	0,      K_ESC,  '!',    '@',    '#',    '$',    '%',    '^',
	'&',    '*',    '(',    ')',    '_',    '+',    K_BKSP, K_TAB,
	'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I',
	'O',    'P',    '{',    '}',    K_ENTR, K_CTRL, 'A',    'S',
	'D',    'F',    'G',    'H',    'J',    'K',    'L',    ':',
	'"',    '~',    0,      '|',    'Z',    'X',    'C',    'V',
	'B',    'N',    'M',    '<',    '>',    '?',    0,      '*',
	K_ALT,  ' ',    0,      0,      0,      0,      0,      0,
	0,      0,      0,      0,      0,      0,      0,      K_HOME,
	K_UP,   K_PGUP, 0,      K_LEFT, 0,      K_RGHT, 0,      K_END,
	K_DOWN, K_PGDN, K_INS,  K_DEL,  0,      0
};

static device_t kbd_dev;	/* Device object */
static int kbd_irq;		/* Handle for keyboard irq */
static int nr_open;		/* Open count */
static struct event io_event = EVENT_INIT(io_event, "kbd");

static u_char kbdq[KBDQ_SIZE];	/* Queue for ascii character */
static int q_tail;
static int q_head;

static int shift;
static int alt;
static int ctrl;
static int capslk;

static int led_sts;

/*
 * Keyboard queue operation
 */
#define kbdq_next(i)    (((i) + 1) & (KBDQ_SIZE - 1))
#define kbdq_empty()    (q_tail == q_head)
#define kbdq_full()     (kbdq_next(q_tail) == q_head)

/*
 * Put one charcter on kbd queue
 */
static void kbdq_enqueue(u_char c)
{
	/* outb(c, 0xe9); */
	sched_wakeup(&io_event);
	if (kbdq_full())
		return;
	kbdq[q_tail] = c;
	q_tail = kbdq_next(q_tail);
}

/*
 * Get one character from kbd queue
 */
static u_char kbdq_dequeue(void)
{
	u_char c;

	c = kbdq[q_head];
	q_head = kbdq_next(q_head);
	return c;
}

/*
 * Send command to keyboard controller
 */
static void kbd_cmd(int cmd)
{
	wait_ibe();
	outb(cmd, KMC_CMD);
}


/*
 * Update LEDs for current modifier state.
 */
static void kbd_setleds(void)
{
	outb(0xed, KMC_DATA);
	while (inb(KMC_STS) & 2);
	outb(led_sts, KMC_DATA);
	while (inb(KMC_STS) & 2);
}

#ifdef CONFIG_KDUMP
/*
 * Help for keyboard debug function
 */
static void kbd_dump_help(void)
{
	printk("\nKernel dump usage:\n");
	printk("F1=help F2=thread F3=task F4=object F5=timer F6=irq F7=dev F8=mem\n");
}
#endif


/*
 * Interrupt service routine
 */
static int kbd_isr(int irq)
{
	u_char sc, ac;		/* scan & ascii code */
	int val, press;

	/* Get scan code */
	wait_obf();
	sc = inb(KMC_DATA);

	/* Send ack to the controller */
	val = inb(KMC_PORTB);
	outb(val | 0x80, KMC_PORTB);
	outb(val, KMC_PORTB);

	/* Convert scan code to ascii */
	press = sc & 0x80 ? 0 : 1;
	sc = sc & 0x7f;
	if (sc >= KEY_MAX)
		return 0;
	ac = key_map[sc];

	/* Reload power management timer */
	if (press)
		pm_active();

	/* Check meta key */
	switch (ac) {
	case K_SHFT:
		shift = press;
		return 0;
	case K_CTRL:
		ctrl = press;
		return 0;
	case K_ALT:
		alt = press;
		return 0;
	case K_CAPS:
		capslk = !capslk;
		return INT_CONTINUE;
	}

	/* Ignore key release */
	if (!press)
		return 0;

#ifdef CONFIG_KDUMP
	if (ac == K_F1) {
		kbd_dump_help();
		return 0;	
	}
	if (ac >= K_F2 && ac <= K_F12) {
		kernel_dump(ac - K_F1); 
		return 0;
	}
#endif
#ifdef DEBUG
	if (ac == K_ESC) {	/* Ctrl-D */
		printk("!!! paused !!!\n");
		irq_lock();
		while (inb(KMC_DATA) == 0x01);
		irq_unlock();
	}
#endif

	if (ac >= 0x80) {
		kbdq_enqueue(ac);
		return 0;
	}

	/* Check Alt+Ctrl+Del */
	if (alt && ctrl && ac == K_DEL) {
		printk("Reset...");
		system_reset();
	}

	/* Check ctrl & shift state */
	if (ctrl) {
		if (ac >= 'a' && ac <= 'z')
			ac = ac - 'a' + 0x01;
		else if (ac == '\\')
			ac = 0x1c;
		else
			ac = 0;
	} else if (shift)
		ac = shift_map[sc];

	if (ac == 0)
		return 0;

	/* Check caps lock state */
	if (capslk) {
		if (ac >= 'A' && ac <= 'Z')
			ac += 'a' - 'A';
		else if (ac >= 'a' && ac <= 'z')
			ac -= 'a' - 'A';
	}

	/* Check alt key */
	if (alt)
		ac |= 0x80;

#ifdef DEBUG
	if (ac == 0x03)		/* Ctrl-C */
		BREAKPOINT();
	if (ac == 0x04)		/* Ctrl-D */
		system_reset();
#endif
	/* Insert key to queue */
	kbdq_enqueue(ac);
	return 0;
}

/*
 * Interrupt service thread
 */
static void kbd_ist(int irq)
{
	int val = 0;

	/* Update LEDs */
	if (capslk)
		val |= 0x04;
	if (led_sts != val) {
		led_sts = val;
		kbd_setleds();
	}
	return;
}

/*
 * Open
 */
static int kbd_open(device_t dev, int mode)
{
	/* printk("kbd_open: nr_open=%d\n", nr_open); */

/*
 * XXX: Temporary allow the multiple device open for different
 * task until tty driver or tty server is available.
 * An application may not get the correct keyboard data if
 * two or more threads reads at same tome.
 */
#if 0
	if (nr_open > 0)
		return EBUSY;
	nr_open++;
#endif
	return 0;
}

/*
 * Close
 */
static int kbd_close(device_t dev)
{
#if 0
	if (nr_open != 1)
		return EINVAL;
	nr_open--;
#endif
	return 0;
}

/*
 * Read
 */
static int kbd_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	int rc;
	size_t count;

	if (*nbyte == 0)
		return 0;

	if (kbdq_empty()) {
		rc = sched_sleep(&io_event);
		if (rc == SLP_INTR)
			return EINTR;
	}
	for (count = 0; count < *nbyte; count++) {
		if (kbdq_empty())
			break;
		*buf = kbdq_dequeue();
		buf++;
	}
	*nbyte = count;
	return 0;
}

/*
 * Initialize
 */
int kbd_init(void)
{
	kbd_dev = device_create(&kbd_io, "kbd");
	ASSERT(kbd_dev);

	/* Disable keyboard controller */
	kbd_cmd(CMD_KBD_DIS);

	led_sts = 0;
	/* kbd_setleds(); */

	kbd_irq = irq_attach(KBD_IRQ, IPL_INPUT, 0, kbd_isr, kbd_ist);
	ASSERT(kbd_irq != -1);

	/* Discard garbage data */
	while (inb(KMC_STS) & STS_OBF)
		inb(KMC_DATA);

	/* Enable keyboard controller */
	kbd_cmd(CMD_KBD_EN);

	return 0;
}
