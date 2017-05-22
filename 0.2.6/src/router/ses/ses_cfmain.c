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
 * $Id: ses_cfmain.c 241187 2011-02-17 21:52:03Z gmo $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <assert.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmtimer.h>
#include <shutils.h>

#include <proto/ethernet.h>

#include <ses.h>
#include <ses_dbg.h>
#include <ses_packet.h>
#include <ses_cmnport.h>

extern bcm_timer_module_id ses_tmodule;

#ifdef SES_TEST_CMDLINE_ARGS
void
ses_print_pe_usage()
{
	printf("Usage:\n");
	printf("	-h	:	this message\n");
	printf("	-f	:	invoke fsm(ignore all other args)\n");
	printf("	-e	:	echo test\n");
	printf("	-a	:	configurator-mode\n");
	printf("	-c	:	client-mode\n");
	printf("	-p phase:	3-button phase\n");
	printf("	-i list :	interface list\n");
	printf("	-o time :	open-window time\n");
	printf("	-s ssid :	ssid\n");
	printf("	-k passphrase  :	passphrase\n");
	printf("	-r rem ea:	remote ether addr\n");
	printf("	-d lvl  :	debug level\n");
	printf("        -g <duty percent> <led pulse period> <max pulses>: gpio h/w test\n");
}
#endif /* SES_TEST_CMDLINE_ARGS */

int ses_main(int argc, char *argv[]);

/* service main entry */
int
main(int argc, char *argv[])
{
#ifdef SES_TEST_CMDLINE_ARGS
	ses_packet_exchange_t spe;
	int status;
	int ow_time, duration = 0;
	char opt;
	bool echo_test = FALSE;
#endif /* SES_TEST_CMDLINE_ARGS */
	int ret = 0;


	/* daemonize it */
	if (daemon(1, 1) == -1) {
		SES_ERROR("Could not daemonize\n");
		return 1;
	}

	/* init timer modules one-time */
	bcm_timer_module_init(SES_MAX_TIMERS, &ses_tmodule);

#ifdef SES_TEST_CMDLINE_ARGS
	memset(&spe, 0, sizeof(ses_packet_exchange_t));

	/* set defaults */
	ow_time = 120;
	spe.mode = SES_PE_MODE_CONFIGURATOR;
	spe.phase = SES_PE_3BUTTON_PRE_CONFIRM | SES_PE_3BUTTON_POST_CONFIRM;
#ifdef SES_TEST_DH_TIMING
	spe.dh_len = 1536;
#endif /* SES_TEST_DH_TIMING */
	spe.version = 1;

	/* parse command line parameters */
	while ((opt = getopt(argc, argv, "faechl:o:i:s:k:r:d:p:g:")) != EOF) {
		switch (opt) {
		case 'h':
			ses_print_pe_usage();
			return 0;
		case 'f':
			return ses_main(argc, argv);
		case 'e':
			echo_test = TRUE;
			break;
		case 'a':
			spe.mode = SES_PE_MODE_CONFIGURATOR;
			break;
		case 'p':
			spe.phase = (int)strtoul(optarg, NULL, 0);
			break;
		case 'c':
			spe.mode = SES_PE_MODE_CLIENT;
			break;
		case 'o':
			ow_time = (int)strtoul(optarg, NULL, 0);
			break;
		case 'i':
			strcpy(spe.ifnames, optarg);
			break;
		case 's':
			strcpy(spe.ssid, optarg);
			break;
		case 'k':
			if (strlen(optarg) > SES_MAX_PASSPHRASE_LEN) {
				SES_ERROR("Invalid passphrase length %d\n",
					strlen(optarg));
				return 0;
			}
			strcpy(spe.passphrase, optarg);
			break;
#ifdef SES_TEST_DH_TIMING
		case 'l':
			spe.dh_len = (int)strtoul(optarg, NULL, 0);
			break;
#endif /* SES_TEST_DH_TIMING */
		case 'r':
			ether_atoe(optarg, (unsigned char *)&spe.remote);
			break;
		case 'd':
#ifdef BCMDBG
			ses_debug_level = (int)strtoul(optarg, NULL, 0);
#endif /* BCMDBG */
		case 'g':
#if SES_ENABLE_HWTEST
			if (argc != 5) {
				ses_print_pe_usage();
				break;
			}
			ses_hw_test(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
#endif /* SES_ENABLE_HWTEST */
			break;
		default:
			ses_print_pe_usage();
			return 0;
		}
	}

	if (echo_test) {
		status = SES_echo_setup(&spe);
	} else {
		status = SES_setup(&spe);
	}

	if (status != SES_PE_STATUS_SUCCESS) {
		SES_ERROR("SES_setup() failed with status %d\n", status);
		return ret;
	}

	do {
		duration++;
		status = SES_packet_exchange(&spe, 1);
	} while ((duration < ow_time) &&
	         ((status == SES_PE_STATUS_CONTINUE) ||
	          (status == SES_PE_STATUS_WFI)));

	SES_INFO("SES_packet_exchange() status = %d\n", status);

	if (duration >= ow_time) {
		SES_ERROR("ow time %d expired\n", ow_time);
		SES_cancel(&spe);
		SES_cleanup(&spe);
		return ret;
	}

	if (status == SES_PE_STATUS_SUCCESS) {
		if (echo_test == FALSE) {
			if (spe.mode == SES_PE_MODE_CLIENT) {
				SES_INFO("recd passphrase %s on if %s\n",
					spe.passphrase, spe.pe_ifname);
			} else {
				SES_INFO("exchange on if %s\n", spe.pe_ifname);
			}
		} else {
			SES_INFO("echo test successful\n");
		}
	}

	SES_cleanup(&spe);
#else
	ret = ses_main(argc, argv);
#endif /* SES_TEST_CMDLINE_ARGS */

	return ret;
}
