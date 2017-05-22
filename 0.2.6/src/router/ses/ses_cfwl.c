/*
 * SES configurator wl helper functions. This file may be modified by
 * vendors if they have a different interface to wl.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_cfwl.c 241187 2011-02-17 21:52:03Z gmo $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <assert.h>
#include <wlutils.h>

#include <ses.h>
#include <ses_dbg.h>
#include <ses_cfvendor.h>
#include <ses_cfwl.h>

int
ses_dot11_transition_mode(char *ifname, bool enable)
{
	uint32 wsec;

	wl_ioctl(ifname, WLC_GET_WSEC, &wsec, sizeof(wsec));
	if (enable)
		wsec |= SES_OW_ENABLED;
	else
		wsec &= ~SES_OW_ENABLED;
	return wl_ioctl(ifname, WLC_SET_WSEC, &wsec, sizeof(wsec));
}

uint32
ses_dot11_lazywds_set(char *ifname, uint32 val)
{
	uint32 old_val;

	wl_ioctl(ifname, WLC_GET_LAZYWDS, &old_val, sizeof(old_val));
	wl_ioctl(ifname, WLC_SET_LAZYWDS, &val, sizeof(val));
	return old_val;
}

bool
ses_dot11_is_wds_link_up(char *ifname, struct ether_addr *ea)
{
	int len, error;
	char buf[sizeof(sta_info_t)];
	sta_info_t *sta;

	len = sprintf(buf, "sta_info");
	memcpy(&buf[len + 1], (char *)ea, ETHER_ADDR_LEN);

	error = wl_ioctl(ifname, WLC_GET_VAR, buf, sizeof(buf));
	if (error)
		return FALSE;

	sta = (sta_info_t *)buf;

	if (sta->flags & WL_STA_WDS)
		return TRUE;
	else
		return FALSE;
}

int
ses_dot11_add_wds_mac(char *ifname, struct ether_addr *ea)
{
	struct maclist *maclist;
	int i;

	maclist = (struct maclist *)malloc(WLC_IOCTL_SMLEN);
	assert(maclist);
	wl_ioctl(ifname, WLC_GET_WDSLIST, maclist, WLC_IOCTL_SMLEN);
	for (i = 0; i < maclist->count; i++) {
		if (!memcmp(&maclist->ea[maclist->count], ea, ETHER_ADDR_LEN)) {
			break;
		}
	}

	if (i == maclist->count) {
		memcpy(&maclist->ea[maclist->count], ea, ETHER_ADDR_LEN);
		maclist->count++;
		wl_ioctl(ifname, WLC_SET_WDSLIST, maclist, WLC_IOCTL_SMLEN);
	}
	free(maclist);

	return 0;
}

int
ses_dot11_vndr_ie(char *adapter, char *cmd, uint32 pktflag, uchar *oui, void *data, int datalen)
{
	char *cmdbuf;
	vndr_ie_setbuf_t *vie_buf;
	int cmdbuflen;
	int err;

	cmdbuflen = sizeof(vndr_ie_setbuf_t) + datalen - 1;
	if (!(cmdbuf = malloc(cmdbuflen))) {
		SES_ERROR("malloc error\n");
		return SES_FSM_STATUS_INTERNAL_ERROR;
	}
	memset(cmdbuf, 0, cmdbuflen);
	vie_buf = (vndr_ie_setbuf_t *) cmdbuf;

	strcpy(vie_buf->cmd, cmd);
	vie_buf->vndr_ie_buffer.iecount = 1;
	vie_buf->vndr_ie_buffer.vndr_ie_list[0].pktflag = pktflag;
	vie_buf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.id = 0; /* ignored */
	vie_buf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = DOT11_OUI_LEN + datalen;
	memcpy(&vie_buf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0], oui, DOT11_OUI_LEN);

	if (datalen > 0)
		memcpy(&vie_buf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0], data,
		       datalen);

	err = wl_iovar_set(adapter, "vndr_ie", cmdbuf, cmdbuflen);
	free(cmdbuf);

	if (err) {
		SES_ERROR("wl_iovar_set error %d\n", err);
		return SES_FSM_STATUS_INTERNAL_ERROR;
	} else {
		SES_INFO("%s\n", cmd);
	}

	return SES_FSM_STATUS_SUCCESS;
}

bool
ses_dot11_vndr_ie_is_set(char *adapter, uint32 pktflag, uchar *oui, void *data, int datalen)
{
	vndr_ie_info_t *vieinfo;
	vndr_ie_t *vie;
	char *bufaddr;
	int totie;
	int err, c;
	bool match = FALSE;
	ses_vndr_ie_t *vndr_ie, *tvndr_ie;
	char *buf;

	assert(datalen == sizeof(ses_vndr_ie_t));
	vndr_ie = (ses_vndr_ie_t *)data;

	buf = malloc(WLC_IOCTL_MAXLEN);

	if (!buf) {
		SES_ERROR("buf malloc failed\n");
		return FALSE;
	}

	bufaddr = buf;
	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "vndr_ie");

	err = wl_ioctl(adapter, WLC_GET_VAR, buf, WLC_IOCTL_MAXLEN);
	if (err) {
		SES_ERROR("wl_ioctl WLC_GET_VAR error %d\n", err);
		free(buf);
		return FALSE;
	}

	memcpy(&totie, bufaddr, sizeof(int));
	bufaddr += sizeof(int);

	for (c = 0; c < totie && !match; c++) {
	        vieinfo = (vndr_ie_info_t *) bufaddr;

		vie = (vndr_ie_t *)&vieinfo->vndr_ie_data;
		tvndr_ie = (ses_vndr_ie_t *)vie->data;
		if (vieinfo->pktflag == pktflag &&
		    memcmp(&vie->oui[0], oui, DOT11_OUI_LEN) == 0 &&
		    (vie->len - VNDR_IE_MIN_LEN) == datalen) {
			tvndr_ie = (ses_vndr_ie_t *)vie->data;
			if (tvndr_ie->type == vndr_ie->type &&
			    tvndr_ie->subtype == vndr_ie->subtype &&
			    tvndr_ie->ses_ver == vndr_ie->ses_ver) {
				/* pick up the flags */
				vndr_ie->ses_flags = tvndr_ie->ses_flags;
				match = TRUE;
				break;
			}
		}
		bufaddr += sizeof(vndr_ie_info_t) + vie->len - VNDR_IE_MIN_LEN - 1;
	}

	SES_INFO("%d\n", match);

	free(buf);
	return match;
}
