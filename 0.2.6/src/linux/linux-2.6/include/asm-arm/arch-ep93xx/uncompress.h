/*
 * linux/include/asm-arm/arch-ep93xx/uncompress.h
 *
 * Copyright (C) 2006 Lennert Buytenhek <buytenh@wantstofly.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include <asm/arch/ep93xx-regs.h>

static unsigned char __raw_readb(unsigned int ptr)
{
	return *((volatile unsigned char *)ptr);
}

static unsigned int __raw_readl(unsigned int ptr)
{
	return *((volatile unsigned int *)ptr);
}

static void __raw_writeb(unsigned char value, unsigned int ptr)
{
	*((volatile unsigned char *)ptr) = value;
}

static void __raw_writel(unsigned int value, unsigned int ptr)
{
	*((volatile unsigned int *)ptr) = value;
}


#define PHYS_UART1_DATA		0x808c0000
#define PHYS_UART1_FLAG		0x808c0018
#define UART1_FLAG_TXFF		0x20

static inline void putc(int c)
{
	int i;

	for (i = 0; i < 1000; i++) {
		/* Transmit fifo not full?  */
		if (!(__raw_readb(PHYS_UART1_FLAG) & UART1_FLAG_TXFF))
			break;
	}

	__raw_writeb(c, PHYS_UART1_DATA);
}

static inline void flush(void)
{
}


#define PHYS_ETH_SELF_CTL		0x80010020
#define ETH_SELF_CTL_RESET		0x00000001

static void ethernet_reset(void)
{
	unsigned int v;

	/* Reset the ethernet MAC.  */
	v = __raw_readl(PHYS_ETH_SELF_CTL);
	__raw_writel(v | ETH_SELF_CTL_RESET, PHYS_ETH_SELF_CTL);

	/* Wait for reset to finish.  */
	while (__raw_readl(PHYS_ETH_SELF_CTL) & ETH_SELF_CTL_RESET)
		;
}


static void arch_decomp_setup(void)
{
	ethernet_reset();
}

#define arch_decomp_wdog()