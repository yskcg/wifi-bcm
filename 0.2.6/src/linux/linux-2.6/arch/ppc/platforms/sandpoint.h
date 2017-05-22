/*
 * Definitions for Motorola SPS Sandpoint Test Platform
 *
 * Author: Mark A. Greer
 *         mgreer@mvista.com
 *
 * 2000-2003 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

/*
 * Sandpoint uses the CHRP map (Map B).
 */

#ifndef __PPC_PLATFORMS_SANDPOINT_H
#define __PPC_PLATFORMS_SANDPOINT_H

#include <asm/ppcboot.h>

#define SANDPOINT_IDE_INT0		14	/* 8259 Test */
#define SANDPOINT_IDE_INT1		15	/* 8259 Test */

/*
 * The sandpoint boards have processor modules that either have an 8240 or
 * an MPC107 host bridge on them.  These bridges have an IDSEL line that allows
 * them to respond to PCI transactions as if they were a normal PCI devices.
 * However, the processor on the processor side of the bridge can not reach
 * out onto the PCI bus and then select the bridge or bad things will happen
 * (documented in the 8240 and 107 manuals).
 * Because of this, we always skip the bridge PCI device when accessing the
 * PCI bus.  The PCI slot that the bridge occupies is defined by the macro
 * below.
 */
#define SANDPOINT_HOST_BRIDGE_IDSEL     12

/*
 * Serial defines.
 */
#define SANDPOINT_SERIAL_0		0xfe0003f8
#define SANDPOINT_SERIAL_1		0xfe0002f8

#define RS_TABLE_SIZE  2

/* Rate for the 1.8432 Mhz clock for the onboard serial chip */
#define BASE_BAUD			( 1843200 / 16 )
#define UART_CLK			1843200

#ifdef CONFIG_SERIAL_DETECT_IRQ
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF|ASYNC_AUTO_IRQ)
#else
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF)
#endif

#define STD_SERIAL_PORT_DFNS \
        { 0, BASE_BAUD, SANDPOINT_SERIAL_0, 4, STD_COM_FLAGS, /* ttyS0 */ \
		iomem_base: (u8 *)SANDPOINT_SERIAL_0,			  \
		io_type: SERIAL_IO_MEM },				  \
        { 0, BASE_BAUD, SANDPOINT_SERIAL_1, 3, STD_COM_FLAGS, /* ttyS1 */ \
		iomem_base: (u8 *)SANDPOINT_SERIAL_1,			  \
		io_type: SERIAL_IO_MEM },

#define SERIAL_PORT_DFNS \
        STD_SERIAL_PORT_DFNS

#endif /* __PPC_PLATFORMS_SANDPOINT_H */