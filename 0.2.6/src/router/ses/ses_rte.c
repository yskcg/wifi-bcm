/*
 * SES deamon (RTE)
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_rte.c 241187 2011-02-17 21:52:03Z gmo $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>

#include <bcmutils.h>
#include <bcmtimer.h>
#include <bcmnvram.h>
#include <proto/ethernet.h>
#include <shutils.h>
#include <wlutils.h>
#include <hndrte.h>
#include <relay_rte.h>
#include <hndrte.h>
#include <sys/socket.h>

#include <ses_dbg.h>
#include <ses_packet.h>
#include <ses_cmnport.h>
#include <ses_gpio.h>
#include <ses_cfvendor.h>

#define SES_MAX_TIMERS		16

bcm_timer_module_id ses_tmodule;
char def_wl_ifname[16];
#ifdef BCMDBG
int ses_debug_level = SES_DEBUG_ERROR;
#endif

#define SES_QLEN	8 /* keep this Q small as it can consume the i/f drivers net pool */

typedef struct ses_info {
	hndrte_dev_t *relay_dev;
	int status;
	struct pktq	Q;
} ses_info_t;

static ses_info_t ses_info;

static bool recv_ses_msg(hndrte_dev_t *src, void *ctx, struct lbuf *lb, uint16 type);
int ses_main(int argc, char *argv[]);
void ses_restart(void);
void sesStart(hndrte_dev_t *relay_dev);

static ethtype_handler_t ses_handler = { NULL, ETHER_TYPE_802_1X, recv_ses_msg, &ses_info };

void
ses_random(uint8 *random, int len)
{
	int tlen = len;
	char *var = "rand";
	uint16 val;
	char buf[512];
	char *tbuf = buf;

	while (tlen > 1) {
		if (wl_iovar_get(def_wl_ifname, var, (void *)&val, sizeof(val)) != 0) {
			SES_ERROR("Could not get RN from wl driver; using pseudo random num\n");
			goto pseudo;
		}
		memcpy(random, (uint8 *)&val, sizeof(val));
		tbuf += sprintf(tbuf, "%02X", *random++);
		tbuf += sprintf(tbuf, "%02X", *random++);
		tlen -= 2;
	}

	if (tlen == 1) {
		if (wl_iovar_get(def_wl_ifname, var, (void *)&val, sizeof(val)) != 0) {
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

static bool
recv_ses_msg(hndrte_dev_t *src, void *ctx, struct lbuf *lb, uint16 type)
{
	ses_info_t *ses = (ses_info_t *)ctx;

	/* check the length of the pkt is sane */
	if (lb->len > 2048) {
		SES_ERROR("SES: pkt too large %d, ignored /n", lb->len);
		return FALSE;
	}

	if (!pktq_full(&ses->Q))
		pktenq(&ses->Q, lb);
	else
		SES_ERROR("recv_ses_msg: pkt tossed, Q full, %s\n", src->dev_fullname);

	return TRUE;
}

/* use the mux to mimic a raw packet socket */
int
ses_open_socket(char *ifname, int *if_id)
{
	int fd;
	hndrte_dev_t *dev;

	SES_INFO("SES: Open socket for %s\n", ifname);

	dev = hndrte_get_dev(ifname);
	ASSERT(dev == ses_info.relay_dev);

	if ((fd = (int)dev->dev_funcs->ioctl(dev, RELAY_PROTOREGISTER, &ses_handler,
	                                      sizeof(ethtype_handler_t *), NULL, NULL, 0)) != 0)
	{
		SES_ERROR("%s: failed to open wpa socket \n", ifname);
		return -1;
	}

	SES_INFO("%s: opened wpa socket %08x\n", ifname, fd);

	*if_id = (int)fd;

	pktq_init(&ses_info.Q, 1, SES_QLEN);

	return 0;
}

void
ses_close_socket(int fd)
{
	hndrte_dev_t *dev = ses_info.relay_dev;

	SES_INFO("closed wpa socket %08x\n", fd);

	dev->dev_funcs->ioctl(dev, RELAY_PROTOUNREGISTER, &ses_handler,
	                      sizeof(ethtype_handler_t *), NULL, NULL, 0);
}

int
ses_wait_for_packet(void *handle, int num_fd, int fd[], int timeout)
{
	struct lbuf *pkt;

	/* Block until the timer fires or packet exhange completes */
	while (!pktq_empty(&ses_info.Q)) {
		pkt = pktdeq(&ses_info.Q);

		ses_info.status = ses_packet_dispatch(handle, 0, pkt->data, pkt->len);
		lb_free(pkt);

		if (ses_info.status != SES_PE_STATUS_CONTINUE) {
			if (ses_info.status != SES_PE_STATUS_SUCCESS)
				SES_ERROR("exchange completed with status %d\n",
				          ses_info.status);
			return (ses_info.status);
		}
	}

	return SES_PE_STATUS_CONTINUE;
}

/* transmit ses packet through the mux */
int
ses_send_packet(int fd, uint8 *buf, int len)
{
	struct lbuf *lb = NULL;
	hndrte_dev_t *relay_dev = ses_info.relay_dev;

	if (!(lb = lb_alloc(len, __FILE__, __LINE__)))
		goto err;

	bcopy(buf, lb->data, len);
	if (relay_sendpkt(relay_dev, lb) != 0)
		goto err;

	return SES_PE_STATUS_SUCCESS;

err:
	if (lb)
		lb_free(lb);
	return SES_PE_STATUS_INTERNAL_ERROR;
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
	int ret;
	ret = wl_hwaddr(name, hwaddr);
	return ret;
}

extern void bsp_reboot(void);

void
ses_restart(void)
{
	bsp_reboot();
}

void
sesStart(hndrte_dev_t *relay_dev)
{
	nvram_set("ses_wds_mode", "0");
	nvram_set("ses_cl_enable", "0");

	/* init timer modules */
	bzero((char*)&ses_tmodule, sizeof(bcm_timer_module_id));
	bcm_timer_module_init(SES_MAX_TIMERS, &ses_tmodule);
	ses_info.relay_dev = relay_dev;
	if (!ses_main(0, NULL))
		printf("SES start error\n");
}

int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	uint32 time_ms;
	ASSERT(clock_id == CLOCK_REALTIME);
	time_ms = hndrte_time();
	tp->tv_sec = (time_ms/1000);
	return 0;
}

/* Provides for delay only.. no blocking IO */
int select(int n, void *readfds, void *writefds, void *exceptfds,
           struct timeval *timeout)
{
	uint32 time_us;

	ASSERT(n == 0);

	time_us = timeout->tv_sec * 1000 * 1000 + timeout->tv_usec;

	hndrte_delay(time_us);
	return 0;
}
