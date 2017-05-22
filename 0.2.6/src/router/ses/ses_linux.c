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
 * $Id: ses_linux.c 241187 2011-02-17 21:52:03Z gmo $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <fcntl.h>
#include <linux/if_packet.h>

#include <assert.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmtimer.h>
#include <proto/ethernet.h>
#include <shutils.h>
#include <wlutils.h>

#include <ses_dbg.h>
#include <ses_packet.h>
#include <ses_cmnport.h>

bcm_timer_module_id ses_tmodule;
char def_wl_ifname[16];
uint8 ses_packet[2048];

#ifdef BCMDBG
int ses_debug_level = SES_DEBUG_ERROR;
#endif

static void ses_linux_pe_timeout(ses_timer_id tid, void *cbarg);

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
ses_random(uint8 *rand, int len)
{
	static int dev_random_fd = -1;
	int status;
	int tlen = len;
	uint16 val;
	char buf[512];
	char *tbuf = buf;

	while (tlen > 1) {
		if (ses_get_wlrand(def_wl_ifname, &val) != 0) {
			SES_ERROR("Could not get RN from wl driver; using /dev/urandom\n");
			goto pseudo;
		}
		memcpy(rand, (uint8 *)&val, sizeof(val));
		tbuf += sprintf(tbuf, "%02X", *rand++);
		tbuf += sprintf(tbuf, "%02X", *rand++);
		tlen -= 2;
	}

	if (tlen == 1) {
		if (ses_get_wlrand(def_wl_ifname, &val) != 0) {
			SES_ERROR("Could not get RN from wl driver; using /dev/urandom\n");
			goto pseudo;
		}
		memcpy(rand, (uint8 *)&val, 1);
		tbuf += sprintf(tbuf, "%02X", *rand++);
	}

	/* SES_INFO("Random Number of len %d: %s\n", len, buf); */

	return;

pseudo:
	if (dev_random_fd == -1)
		dev_random_fd = open("/dev/urandom", O_RDONLY|O_NONBLOCK);

	assert(dev_random_fd != -1);
	status = read(dev_random_fd, rand, len);
	assert(status != -1);

	return;
}

int
ses_open_socket(char *ifname, int *if_id)
{
	int fd, err;
	struct sockaddr_ll sll;
	struct ifreq ifr;

	fd = socket(PF_PACKET, SOCK_RAW, htons(ETHER_TYPE_802_1X));
	if (fd < 0) {
		SES_ERROR("%s: open socket failed with error %d\n", ifname, fd);
		return fd;
	}

	/* Get interface index */
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, SES_IFNAME_SIZE);
	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		SES_ERROR("%s: socket ioctl failed with error %d\n", ifname, err);
		return err;
	}

	/* bind the socket to the interface */
	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(ETHER_TYPE_802_1X);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		SES_ERROR("%s: bind socket failed with error %d\n", ifname, err);
		return err;
	}

	SES_INFO("%s: opened socket %d\n", ifname, fd);
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

	ses_timer_start(&tid, timeout, (ses_timer_cb)ses_linux_pe_timeout, &cbarg);

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

				status = ses_packet_dispatch(handle, i, ses_packet, bytes);
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
	struct iovec frags;

	frags.iov_base = buf;
	frags.iov_len = len;

	memset(&mh, 0, sizeof(mh));
	mh.msg_iov = &frags;
	mh.msg_iovlen = 1;

	SES_INFO("sending ses packet\n");

	if (sendmsg(fd, &mh, 0) < 0) {
		SES_ERROR("sendmsg() failed with error %d\n", errno);
		return SES_PE_STATUS_INTERNAL_ERROR;
	}

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
ses_linux_pe_timeout(ses_timer_id tid, void *cbarg)
{
	int *data = (int *)cbarg;
	*data = 1;
}

void
ses_restart(void)
{
	kill(1, SIGHUP);
}
