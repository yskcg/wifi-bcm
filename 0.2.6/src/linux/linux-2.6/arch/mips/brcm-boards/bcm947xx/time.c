/*
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: time.c,v 1.9 2009-07-17 06:23:12 Exp $
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/serial_reg.h>
#include <linux/interrupt.h>
#include <asm/addrspace.h>
#include <asm/io.h>
#include <asm/time.h>

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <siutils.h>
#include <hndmips.h>
#include <mipsinc.h>
#include <hndcpu.h>
#include <bcmdevs.h>

/* Global SB handle */
extern si_t *bcm947xx_sih;
extern spinlock_t bcm947xx_sih_lock;

/* Convenience */
#define sih bcm947xx_sih
#define sih_lock bcm947xx_sih_lock

#define WATCHDOG_MIN	3000	/* milliseconds */
extern int panic_timeout;
extern int panic_on_oops;
static int watchdog = 0;

#ifndef	CONFIG_HWSIM
static u8 *mcr = NULL;
#endif /* CONFIG_HWSIM */

void __init
bcm947xx_time_init(void)
{
	unsigned int hz;
	char cn[8];

	/*
	 * Use deterministic values for initial counter interrupt
	 * so that calibrate delay avoids encountering a counter wrap.
	 */
	write_c0_count(0);
	write_c0_compare(0xffff);

	if (!(hz = si_cpu_clock(sih)))
		hz = 100000000;

	bcm_chipname(sih->chip, cn, 8);
	printk("CPU: BCM%s rev %d at %d MHz\n", cn, sih->chiprev,
	       (hz + 500000) / 1000000);

	/* Set MIPS counter frequency for fixed_rate_gettimeoffset() */
	mips_hpt_frequency = hz / 2;

	/* Set watchdog interval in ms */
	watchdog = simple_strtoul(nvram_safe_get("watchdog"), NULL, 0);

	/* Ensure at least WATCHDOG_MIN */
	if ((watchdog > 0) && (watchdog < WATCHDOG_MIN))
		watchdog = WATCHDOG_MIN;

	/* Set panic timeout in seconds */
	panic_timeout = watchdog / 1000;
	panic_on_oops = watchdog / 1000;
}

#ifdef CONFIG_HND_BMIPS3300_PROF
extern bool hndprofiling;
#ifdef CONFIG_MIPS64
typedef u_int64_t sbprof_pc;
#else
typedef u_int32_t sbprof_pc;
#endif
extern void sbprof_cpu_intr(sbprof_pc restartpc);
#endif	/* CONFIG_HND_BMIPS3300_PROF */

static irqreturn_t
bcm947xx_timer_interrupt(int irq, void *dev_id)
{
#ifdef CONFIG_HND_BMIPS3300_PROF
	/*
	 * Are there any ExcCode or other mean(s) to determine what has caused
	 * the timer interrupt? For now simply stop the normal timer proc if
	 * count register is less than compare register.
	 */
	if (hndprofiling) {
		sbprof_cpu_intr(read_c0_epc() +
		                ((read_c0_cause() >> (CAUSEB_BD - 2)) & 4));
		if (read_c0_count() < read_c0_compare())
			return (IRQ_HANDLED);
	}
#endif	/* CONFIG_HND_BMIPS3300_PROF */

	/* Generic MIPS timer code */
	timer_interrupt(irq, dev_id);

	/* Set the watchdog timer to reset after the specified number of ms */
	if (watchdog > 0)
		si_watchdog_ms(sih, watchdog);

#ifdef	CONFIG_HWSIM
	(*((int *)0xa0000f1c))++;
#else
	/* Blink one of the LEDs in the external UART */
	if (mcr && !(jiffies % (HZ/2)))
		writeb(readb(mcr) ^ UART_MCR_OUT2, mcr);
#endif

	return (IRQ_HANDLED);
}

static struct irqaction bcm947xx_timer_irqaction = {
	bcm947xx_timer_interrupt,
	IRQF_DISABLED,
	{ { 0 } },
	"timer",
	NULL,
	NULL,
	0,
	NULL
};

void __init
plat_timer_setup(struct irqaction *irq)
{
	/* Enable the timer interrupt */
	setup_irq(7, &bcm947xx_timer_irqaction);
}
