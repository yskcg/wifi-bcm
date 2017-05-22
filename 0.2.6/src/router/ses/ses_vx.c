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
 * $Id: ses_vx.c 241187 2011-02-17 21:52:03Z gmo $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <signal.h>
#include <ctype.h>
#include <assert.h>

#include <vxWorks.h>
#include <ioLib.h>
#include <ifLib.h>
#include <tickLib.h>
#include <taskLib.h>
#include <errnoLib.h>

#include <typedefs.h>
#include <bcmutils.h>
#include <bcmtimer.h>
#include <proto/ethernet.h>
#include <shutils.h>
#include <wlif_utils.h>
#include <wlutils.h>

#include <ses_dbg.h>
#include <ses_packet.h>
#include <ses_cmnport.h>
#include <ses_proto.h>

#include <eapd.h>
#include <UdpLib.h>
#include <security_ipc.h>

bcm_timer_module_id ses_tmodule;
char def_wl_ifname[16];
uint8 ses_packet[2048];

typedef struct ses_eapd_private {
	char ifname[SES_PE_MAX_INTERFACES][16];
	char ifmac[SES_PE_MAX_INTERFACES][6];
	int used[SES_PE_MAX_INTERFACES];
} ses_eapd_private_t;

static ses_eapd_private_t ses_eapd_db;
static int ses_eapd_db_inited = 0;

#ifdef BCMDBG
int ses_debug_level = SES_DEBUG_ERROR;
#endif

static void ses_vx_pe_timeout(ses_timer_id tid, void *cbarg);

extern int ses_main(int argc, char *argv[]);
extern int ses_clmain(int argc, char *argv[]);
extern void sys_restart(void);

static int
ses_eapd_update_db(char *osifname, char *ifmac)
{
	int i, update_idx = -1;

	if (osifname == NULL ||ifmac == NULL)
		return -1; /* invalid parameter */

	if (!ses_eapd_db_inited) {
		for (i = 0; i < SES_PE_MAX_INTERFACES; i++)
			ses_eapd_db.used[i] = 0;
		ses_eapd_db_inited = 1;
	}

	/* search existance */
	for (i = 0; i < SES_PE_MAX_INTERFACES; i++) {
		if (ses_eapd_db.used[i] &&
		     !memcmp(ses_eapd_db.ifmac[i], ifmac, 6)) {
			update_idx = i;
			break;
		}
		if (update_idx == -1 && ses_eapd_db.used[i] == 0)
			update_idx = i;
	}

	if (update_idx == -1)
		return -2; /* no more empty entry */

	/* update it */
	ses_eapd_db.used[update_idx] = 1;
	strcpy(ses_eapd_db.ifname[update_idx], osifname);
	memcpy(ses_eapd_db.ifmac[update_idx], ifmac, 6);

	return 0;
}

static int
ses_eapd_find_db(char *ifmac, char **ifname)
{
	int i;

	if (ifmac == NULL)
		return -1; /* invalid parameter */

	if (!ses_eapd_db_inited)
		return -3; /* not initial */

	/* search existance */
	for (i = 0; i < SES_PE_MAX_INTERFACES; i++) {
		if (!memcmp(ses_eapd_db.ifmac[i], ifmac, 6))
			break;
	}

	if (i == SES_PE_MAX_INTERFACES)
		return -4; /* not exist */

	*ifname = (char*)ses_eapd_db.ifname[i];

	return 0;
}

/* Get a random number from WL driver */
static int
ses_get_wlrand(char *ifname, uint16 *val)
{
	char buf[WLC_IOCTL_SMLEN];
	int ret;

	strcpy(buf, "rand");
	if ((ret = wl_ioctl(ifname, WLC_GET_VAR, buf, sizeof(buf))))
		return ret;

	*val = *(uint16 *)buf;
	return 0;
}

void
ses_random(uint8 *random, int len)
{
	int tlen = len;
	uint16 val;
	char buf[512];
	char *tbuf = buf;

	while (tlen > 1) {
		if (ses_get_wlrand(def_wl_ifname, &val) != 0) {
			SES_ERROR("Could not get RN from wl driver; using pseudo random num\n");
			goto pseudo;
		}
		memcpy(random, (uint8 *)&val, sizeof(val));
		tbuf += sprintf(tbuf, "%02X", *random++);
		tbuf += sprintf(tbuf, "%02X", *random++);
		tlen -= 2;
	}

	if (tlen == 1) {
		if (ses_get_wlrand(def_wl_ifname, &val) != 0) {
			SES_ERROR("Could not get RN from wl driver; using pseudo random num\n");
			goto pseudo;
		}
		memcpy(random, (uint8 *)&val, 1);
		tbuf += sprintf(tbuf, "%02X", *random++);
	}

	SES_INFO("Random Number of len %d: %s\n", len, buf);

	return;

pseudo:
	while (tlen--) {
		*random = (uint8)rand();
		tbuf += sprintf(tbuf, "%02X", *random++);
	}

	SES_INFO("Pseudo random Number of len %d: %s\n", len, buf);

	return;
}

/* open a UDP packet to eapd for receiving/sending data */
int
ses_open_socket(char *ifname, int *if_id)
{
	int fd, reuse = 1;
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		SES_ERROR("%s: open UDP socket failed with error %d\n", ifname, fd);
		return fd;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
		SES_ERROR("UDP setsockopt failed.\n");
		close(fd);
		return -1;
	}

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(EAPD_WKSP_SES_UDP_SPORT);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		SES_ERROR("UDP Bind failed.\n");
		close(fd);
		return -1;
	}

	SES_INFO("UDP Bind sucessfual on fd %d port %d\n", fd, EAPD_WKSP_SES_UDP_SPORT);

	*if_id = fd;

	return 0;
}

void
ses_close_socket(int fd)
{
	close(fd);
}

int
ses_wait_for_packet(void *handle, int num_fd, int fd[], int timeout)
{
	int i, status, bytes, width = -1;
	fd_set fdset, ifdset;
	ses_timer_id tid;
	ses_header_t *ses_hdr;
	char *ifname;
	int cbarg = 0;

	assert(timeout);

	/* init file descriptor set */
	FD_ZERO(&ifdset);

	/* build file descriptor set */
	for (i = 0; i < num_fd; i++) {
		/* SES_INFO("include socket %d in fdset\n", fd[i]); */
		FD_SET(fd[i], &ifdset);
		if (fd[i] > width)
			width = fd[i];
	}

	ses_timer_start(&tid, timeout, (ses_timer_cb)ses_vx_pe_timeout, &cbarg);

	/* stay here until timeout */
	for (;;) {
		fdset = ifdset;

		/* enable timer handling */
		bcm_timer_module_enable(ses_tmodule, 1);

		/* listen to data available on all sockets */
		status = select(width+1, &fdset, NULL, NULL, NULL);

		/* disable timer handling */
		bcm_timer_module_enable(ses_tmodule, 0);

		if (status < 0) {
			if (errno != EINTR) {
				SES_ERROR("select error %d\n", errno);
				status = SES_PE_STATUS_INTERNAL_ERROR;
				goto done;
			}

			/* SES_INFO("select recd EINTR\n"); */

			if (cbarg == 1) {
				status = SES_PE_STATUS_CONTINUE;
				goto done;
			}
			continue;
		}

		for (i = 0; i < num_fd; i++) {
			if (FD_ISSET(fd[i], &fdset)) {
				bytes = recv(fd[i], ses_packet, sizeof(ses_packet), 0);
				if (bytes < 0) {
					SES_ERROR("recv ses packet error %d on fd %d\n",
						errno, fd[i]);
					status = SES_PE_STATUS_INTERNAL_ERROR;
					goto done;
				}

				SES_INFO("recd %d bytes\n", bytes);

				if (bytes == 0)
					continue;

				ifname = ses_packet;
				bytes -= IFNAMSIZ;

				ses_hdr = (ses_header_t *)ses_packet;
				/* update newest ifname and real mac mapping */
				ses_eapd_update_db(ifname, ses_hdr->eth.ether_dhost);

				status = ses_packet_dispatch(handle, i, (uint8 *)ses_hdr, bytes);
				if (status != SES_PE_STATUS_CONTINUE) {
					if (status != SES_PE_STATUS_SUCCESS)
						SES_ERROR("exchange completed with status %d\n",
							status);
					goto done;
				}
			}
		}
	}

done:
	ses_timer_stop(tid);
	return status;
}

/* transmit ses packet through the socket */
int
ses_send_packet(int fd, uint8 *buf, int len)
{
	struct msghdr mh;
	struct iovec iov[2];
	struct sockaddr_in to;
	struct ether_header *eth;
	char *ifname = NULL;

	if (len < sizeof(struct ether_header))
		return SES_PE_STATUS_INTERNAL_ERROR;

	eth = (struct ether_header*) buf;
	/* get ifname */
	if (ses_eapd_find_db(eth->ether_shost, &ifname)) {
		SES_ERROR("ses_eapd_find_db failed mac = %02x:%02x:%02x:%02x:%02x:%02x\n",
			eth->ether_shost[0], eth->ether_shost[1],
			eth->ether_shost[2], eth->ether_shost[3],
			eth->ether_shost[4], eth->ether_shost[5]);
		return SES_PE_STATUS_INTERNAL_ERROR;
	}

	to.sin_addr.s_addr = inet_addr(EAPD_WKSP_UDP_ADDR);
	to.sin_family = AF_INET;
	to.sin_port = htons(EAPD_WKSP_SES_UDP_RPORT);

	iov[0].iov_base = ifname;
	iov[0].iov_len = IFNAMSIZ;
	iov[1].iov_base = buf;
	iov[1].iov_len = len;

	memset(&mh, 0, sizeof(mh));
	mh.msg_name = (void *)&to;
	mh.msg_namelen = sizeof(to);
	mh.msg_iov = iov;
	mh.msg_iovlen = 2;

	SES_INFO("sending ses packet\n");

	if (sendmsg(fd, &mh, 0) < 0) {
		SES_ERROR("sendmsg() failed with error %d\n", errno);
		return SES_PE_STATUS_INTERNAL_ERROR;
	}

	SES_INFO("sendmsg() on fd %d port %d sucessful len %d\n", fd, EAPD_WKSP_SES_UDP_RPORT, len);
	return SES_PE_STATUS_SUCCESS;
}

int
ses_timer_start(ses_timer_id *id, int time, ses_timer_cb cb, void *data)
{
	struct itimerspec timer;
	int status;

	timer.it_interval.tv_sec = time;
	timer.it_interval.tv_nsec = 0;
	timer.it_value.tv_sec = time;
	timer.it_value.tv_nsec = 0;

	status = bcm_timer_create(ses_tmodule, (bcm_timer_id *)id);
	if (status) {
		SES_ERROR("bcm_timer_create() failed with error %d\n", status);
		return SES_PE_STATUS_INTERNAL_ERROR;
	}

	status = bcm_timer_connect((bcm_timer_id)*id, (bcm_timer_cb)cb, (int)data);
	if (status) {
		SES_ERROR("bcm_timer_create() failed with error %d\n", status);
		return SES_PE_STATUS_INTERNAL_ERROR;
	}

	status = bcm_timer_settime((bcm_timer_id)*id, &timer);
	if (status) {
		SES_ERROR("bcm_timer_create() failed with error %d\n", status);
		return SES_PE_STATUS_INTERNAL_ERROR;
	}

	return SES_PE_STATUS_SUCCESS;
}

void
ses_timer_stop(ses_timer_id id)
{
	bcm_timer_delete((bcm_timer_id)id);
}

int
ses_wl_hwaddr(char *name, unsigned char *hwaddr)
{
	return wl_hwaddr(name, hwaddr);
}

static void
ses_vx_pe_timeout(ses_timer_id tid, void *cbarg)
{
	int *data = (int *)cbarg;
	*data = 1;
}

void
ses_restart(void)
{
	sys_restart();
}

void
sesStart(void)
{
	int tid = taskNameToId("SES");
	/* Create the args for ses */
	int ses_argc = 1;
	char *ses_argv[1];
	char *ses_arg = "-f";
	ses_argv[0] = ses_arg;

	if (tid == ERROR) {
		/* init timer modules */
		bzero((char*)&ses_tmodule, sizeof(bcm_timer_module_id));
		bcm_timer_module_init(SES_MAX_TIMERS, &ses_tmodule);
		tid = taskSpawn("SES",
			 70, /* priority of new task */
			 0, /* task option word */
			 20000,  /* size (bytes) of stack needed plus name */
			 (FUNCPTR)ses_main,   /* entry point of new task */
			 ses_argc, (int)ses_argv, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	else
		printf("SES task is already running.\n");
}

void
sesStop(void)
{
	int tid = taskNameToId("SES");
	if (tid != ERROR) {
		ses_gpio_btn_cleanup();
		ses_gpio_led_cleanup();
		bcm_timer_module_cleanup(ses_tmodule);
		if (taskDelete(tid) == ERROR)
			printf("Unable to kill SES task!.\n");
	}
	else
		printf("SES task is not running.\n");
}


void
sesClStart(void)
{
	int tid = taskNameToId("SESCL");
	/* Create the args for ses */
	int ses_argc = 1;
	char *ses_argv[1];
	char *ses_arg = "-f";
	ses_argv[0] = ses_arg;

	if (tid == ERROR) {
		/* init timer modules */
		bzero((char*)&ses_tmodule, sizeof(bcm_timer_module_id));
		bcm_timer_module_init(SES_MAX_TIMERS, &ses_tmodule);
		tid = taskSpawn("SESCL",
			 70, /* priority of new task */
			 0, /* task option word */
			 20000,  /* size (bytes) of stack needed plus name */
			 (FUNCPTR)ses_clmain,   /* entry point of new task */
			 ses_argc, (int)ses_argv, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	else
		printf("SES task is already running.\n");
}

void
sesClStop(void)
{
	int tid = taskNameToId("SESCL");
	if (tid != ERROR) {
		ses_gpio_btn_cleanup();
		ses_gpio_led_cleanup();
		bcm_timer_module_cleanup(ses_tmodule);
		if (taskDelete(tid) == ERROR)
			printf("Unable to kill SES task!.\n");
	}
	else
		printf("SES task is not running.\n");
}

int
ses_bridge_delif(char *ifname)
{
	char name[5];
	int instance = 0;

	bzero(name, sizeof(name));
	strncpy(name, ifname, 2);
	instance = bcm_atoi(&ifname[2]);
	bridgePortRemove(name, instance);
}
