/*
 * SES deamon (Linux)
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_clmain.c 241187 2011-02-17 21:52:03Z gmo $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <assert.h>
#include <typedefs.h>
#include <bcmtimer.h>

#include <shutils.h>
#include <ses.h>
#include <ses_dbg.h>

extern bcm_timer_module_id ses_tmodule;

int ses_clmain(int argc, char *argv[]);

/* service main entry */
int
main(int argc, char *argv[])
{
	/* We don't need/support SES Client in URE Mode */
	if (ure_any_enabled()) {
		SES_ERROR("SES Client not starting, URE mode is enabled\n");
		return 1;
	}

	/* daemonize it */
	if (daemon(1, 1) == -1) {
		SES_ERROR("Could not daemonize\n");
		return 1;
	}

	/* init timer modules one-time */
	bcm_timer_module_init(SES_MAX_TIMERS, &ses_tmodule);

	return ses_clmain(argc, argv);
}
