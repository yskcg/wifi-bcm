/*
 * SES client wl helper functions. This file may be modified by
 * vendors if they have a different interface to wl. This file will be typically
 * used by linux and vx router ports.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_wl.c 241187 2011-02-17 21:52:03Z gmo $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <assert.h>
#include <wlutils.h>

#include <ses_dbg.h>
#include <ses.h>
#include <ses_cl.h>

/* set to 0 to disable AOSS coexistence */
static uint8 ses_aoss_coexistence = 1;

#define SES_AOSS_WEP_KEY		"melco"
#define SES_AOSS_WEP_INDEX		0
#define SES_AOSS_SSID_KEYWORD		"AOSS"

int
ses_dot11_disassoc(char const *ifname)
{
	SES_CB("enter\n");

	return wl_ioctl((char *)ifname, WLC_DISASSOC, NULL, 0);
}

int
ses_get_media_state(char const *ifname, int *state)
{
	int status, err;

	SES_CB("enter\n");

	err = wl_ioctl((char *)ifname, WLC_GET_UP, &status, sizeof(status));
	assert(!err);
	if (status)
		*state = SES_MEDIA_STATE_CONNECTED;
	else
		*state = SES_MEDIA_STATE_DISCONNECTED;
	return err;

}

int
ses_dot11_join(char const *ifname, ses_join_info_t *join_info)
{
	wlc_ssid_t wlc_ssid;
	int err;
	int val;
	int infra = 1;

	SES_CB("enter\n");

	/* Set wl mode to infrastructure */
	wl_ioctl((char *)ifname, WLC_SET_INFRA, &infra, sizeof(int));

	if (join_info->capability & DOT11_CAP_PRIVACY) {
		wl_ioctl((char *)ifname, WLC_GET_WSEC, &val, sizeof(val));
		val |= WEP_ENABLED;
		wl_ioctl((char *)ifname, WLC_SET_WSEC, &val, sizeof(val));

		/* melco coexistence */
		if (ses_aoss_coexistence &&
		    strstr(join_info->ssid, SES_AOSS_SSID_KEYWORD)) {
			wl_wsec_key_t wep;

			memset(&wep, 0, sizeof(wep));
			wep.index = SES_AOSS_WEP_INDEX;
			wep.flags = WL_PRIMARY_KEY;
			wep.len = strlen(SES_AOSS_WEP_KEY);
			wl_ioctl((char *)ifname, WLC_SET_KEY, &wep, sizeof(wep));
		}
	} else {
		val = 0;
		wl_ioctl((char *)ifname, WLC_SET_WSEC, &val, sizeof(val));
	}

	strncpy((char *)wlc_ssid.SSID, join_info->ssid, sizeof(wlc_ssid.SSID));
	wlc_ssid.SSID_len = strlen((char *)wlc_ssid.SSID);
	err = wl_ioctl((char *)ifname, WLC_SET_SSID, &wlc_ssid, sizeof(wlc_ssid));
	return err;
}

int
ses_dot11_scan_results(char const *ifname, wl_scan_results_t **results)
{
#define SES_SCAN_BUFSIZE	16384
	int err;

	SES_CB("enter\n");

	/* storage will be freed in cl state machine */
	*results = (wl_scan_results_t *)malloc(SES_SCAN_BUFSIZE);
	assert(*results);

	(*results)->buflen = SES_SCAN_BUFSIZE;
	err = wl_ioctl((char *)ifname, WLC_SCAN_RESULTS, *results, SES_SCAN_BUFSIZE);
	return err;
}

int
ses_dot11_scan(char const *ifname, int *wait_time)
{
	int err;
	wl_scan_params_t* params;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE;

	SES_CB("enter\n");

	params = (wl_scan_params_t*)malloc(params_size);
	assert(params);
	memset(params, 0, params_size);

	params->bss_type = DOT11_BSSTYPE_ANY;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->scan_type = DOT11_SCANTYPE_ACTIVE;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	err = wl_ioctl((char *)ifname, WLC_SCAN, params, params_size);
	*wait_time = 3000;
	return err;
}

int
ses_get_adapter_type(char const *ifname, int *type)
{
	SES_CB("ifname = %s\n", ifname);

	*type = SES_ADAPTER_TYPE_WIRELESS;
	return 0;
}

char *
ses_get_wlname(char const *ifname, char *wlname)
{
	int type;

	ses_get_adapter_type(ifname, &type);
	if (type == SES_ADAPTER_TYPE_WIRELESS) {
		strcpy(wlname, ifname);
	} else if (type == SES_ADAPTER_TYPE_802_3) {
		strcpy(wlname, "");
	} else {
		assert(0);
	}
	return wlname;
}

#ifdef SES_WDS_CLIENT

char *ses_get_underlying_wlname(char const *ifname, char *wlname);

int
ses_wds_dot11_disassoc(char const *ifname)
{
	return 0;
}

int
ses_wds_get_media_state(char const *ifname, int *state)
{
	int status, err;
	char wlname[SES_IFNAME_SIZE];

	SES_CB("enter\n");

	err = wl_ioctl(ses_get_underlying_wlname(ifname, wlname), WLC_GET_UP,
	               &status, sizeof(status));
	assert(!err);
	if (status)
		*state = SES_MEDIA_STATE_CONNECTED;
	else
		*state = SES_MEDIA_STATE_DISCONNECTED;
	return err;

}

int
ses_wds_dot11_join(char const *ifname, ses_join_info_t *join_info)
{
	struct maclist *maclist;
	bool exists = FALSE;
	char wlname[SES_IFNAME_SIZE];
	int i;
	int channel;

	SES_CB("enter\n");

	maclist = (struct maclist *)malloc(WLC_IOCTL_SMLEN);
	assert(maclist);
	wl_ioctl(ses_get_underlying_wlname(ifname, wlname), WLC_GET_WDSLIST,
	         maclist, WLC_IOCTL_SMLEN);
	for (i = 0; i < maclist->count; i++) {
		if (!memcmp(&maclist->ea[maclist->count],
		            &join_info->remote, ETHER_ADDR_LEN)) {
			exists = TRUE;
			break;
		}
	}
	if (!exists) {
		memcpy(&maclist->ea[maclist->count],
		       &join_info->remote, ETHER_ADDR_LEN);
		maclist->count++;
		wl_ioctl(wlname, WLC_SET_WDSLIST, maclist, WLC_IOCTL_SMLEN);
	}

	/* handle channel change */
	channel = join_info->channel;
	wl_ioctl(wlname, WLC_SET_CHANNEL, &channel, sizeof(int));

	/* need a down/up for channel change to take effect */
	wl_ioctl(wlname, WLC_DOWN, NULL, 0);
	wl_ioctl(wlname, WLC_UP, NULL, 0);

	return 0;
}

int
ses_wds_dot11_scan_results(char const *ifname, wl_scan_results_t **results)
{
#define SES_SCAN_BUFSIZE	16384
	int err;
	char wlname[SES_IFNAME_SIZE];

	SES_CB("enter\n");

	/* storage will be freed in cl state machine */
	*results = (wl_scan_results_t *)malloc(SES_SCAN_BUFSIZE);
	assert(*results);

	(*results)->buflen = SES_SCAN_BUFSIZE;
	err = wl_ioctl(ses_get_underlying_wlname(ifname, wlname), WLC_SCAN_RESULTS,
	               *results, SES_SCAN_BUFSIZE);
	return err;
}

int
ses_wds_dot11_scan(char const *ifname, int *wait_time)
{
	int err;
	wl_scan_params_t* params;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE;
	char wlname[SES_IFNAME_SIZE];

	SES_CB("enter\n");

	params = (wl_scan_params_t*)malloc(params_size);
	assert(params);
	memset(params, 0, params_size);

	params->bss_type = DOT11_BSSTYPE_ANY;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->scan_type = DOT11_SCANTYPE_ACTIVE;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	err = wl_ioctl(ses_get_underlying_wlname(ifname, wlname), WLC_SCAN,
	               params, params_size);
	*wait_time = 3000;
	return err;
}

int
ses_wds_get_adapter_type(char const *ifname, int *type)
{
	*type = SES_ADAPTER_TYPE_WIRELESS;
	return 0;
}

char *
ses_wds_get_wlname(char const *ifname, char *wlname)
{
	ses_get_underlying_wlname(ifname, wlname);
	return wlname;
}
#endif /* SES_WDS_CLIENT */
