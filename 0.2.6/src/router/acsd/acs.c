/*
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: acs.c 245159 2011-03-08 23:03:11Z willfeng $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <assert.h>
#include <typedefs.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <bcmendian.h>

#include <bcmendian.h>
#include <bcmwifi.h>
#include <wlioctl.h>
#include <wlutils.h>
#include <shutils.h>
#include <proto/ethernet.h>

#include "acsd_svr.h"

#define PREFIX_LEN 32

/* some channel bounds */
#define ACS_CS_MIN_2G_CHAN	1	/* min channel # in 2G band */
#define ACS_CS_MAX_2G_CHAN	CH_MAX_2G_CHANNEL	/* max channel # in 2G band */
#define ACS_CS_MIN_5G_CHAN	36	/* min channel # in 5G band */
#define ACS_CS_MAX_5G_CHAN	MAXCHANNEL	/* max channel # in 5G band */

/* possible min channel # in the band */
#define ACS_CS_MIN_CHAN(band)	((band == WLC_BAND_5G) ? ACS_CS_MIN_5G_CHAN : \
			(band == WLC_BAND_2G) ? ACS_CS_MIN_2G_CHAN : 0)
/* possible max channel # in the band */
#define ACS_CS_MAX_CHAN(band)	((band == WLC_BAND_5G) ? ACS_CS_MAX_5G_CHAN : \
			(band == WLC_BAND_2G) ? ACS_CS_MAX_2G_CHAN : 0)

#define BAND_2G(band) (band == WLC_BAND_2G)
#define BAND_5G(band) (band == WLC_BAND_5G)

#define ACS_SM_BUF_LEN  1024
#define ACS_SRSLT_BUF_LEN (32*1024)

#define ACS_CHANNEL_1 1
#define ACS_CHANNEL_6 6
#define ACS_CHANNEL_11 11

#define ACS_DFLT_POLICY	0
#define ACS_DFLT_FLAGS ACS_FLAGS_LASTUSED_CHK

acs_policy_t predefined_policy[] = {
	{0, 100, {-1, 0, 0, 0, 0, 0, 0, 1, 1}, 0},
	{0, 100, {-100, 0, 0, 0, 0, 0, 0, 0, 1}, 0},
	{-65, 40, {-1, 0, -100, -1, 0, 0, 0, 0, 1}, 0},
	{-65, 40, {-1, -100, -100, -1, 0, 0, -100, 0, 1}, 0},
	{-65, 40, {-1, -100, -100, -1, -100, 0, -100, 0, 1}, 0},
	{-55, 45, {-200, 0, -100, -50, 0, 0, -50, 0, 1}, 0},
	{-70, 45, {-1, -50, -100, -10, -10, 0, -50, 0, 1}, 0}
};

acs_info_t *acs_info;

/*
 * This module retrieves the following information from the wl driver before
 * deciding on the best channel:
 * 1) scan result (wl_scan_result_t)
 * 2) channel interference stats (wl_chanim_stats_t)
 * 3) scan channel spec list
 * 4) channel spec candidate (all valid channel spec for the current band, bw, locale)
 * 5) band type, coex_enable, bw_cap.
 *
 * The facts which could be weighted in the channel scoring systems are:
 * 1) Number of BSS's detected during the scan process (from scan result)
 * 2) Channel Occupancy (percentage of time the channel is occupied by other BSS's)
 * 3) Channel Interference (from CCA stats)
 * 4) Channel FCS (from CCA stats)
 * 5) Channel MAX tx power
 * 6) Adjacent Channel Interference
 * The scoring algorithm for each factor is subject to update based on testing results.
 * The weight for each factor can be customized based on different channel eval policies.
 */

static int
acs_build_scanlist(acs_chaninfo_t *c_info)
{
	wl_uint32_list_t *list;
	chanspec_t input = 0, c = 0;
	int ret = 0, i;
	int count = 0;
	scan_chspec_elemt_t *ch_list;
	acs_rsi_t *rsi = &c_info->rs_info;

	char *data_buf, *data_buf1 = NULL;
	data_buf = acsd_malloc(ACS_SM_BUF_LEN);

	input |= WL_CHANSPEC_BW_20;

	if (BAND_5G(rsi->band_type))
		input |= WL_CHANSPEC_BAND_5G;
	else
		input |= WL_CHANSPEC_BAND_2G;

	ret = wl_iovar_getbuf(c_info->name, "chanspecs", &input, sizeof(chanspec_t),
		data_buf, ACS_SM_BUF_LEN);
	ACS_ERR(ret, "failed to get valid chanspec lists");

	list = (wl_uint32_list_t *)data_buf;
	count = dtoh32(list->count);

	c_info->scan_chspec_list.count = count;
	c_info->scan_chspec_list.idx = 0;

	if (!count) {
		ACSD_ERROR("number of valid chanspec is 0\n");
		ret = -1;
		goto cleanup_sl;
	}

	ACS_FREE(c_info->scan_chspec_list.chspec_list);

	ch_list = c_info->scan_chspec_list.chspec_list =
		(scan_chspec_elemt_t *)acsd_malloc(count * sizeof(scan_chspec_elemt_t));

	data_buf1 = acsd_malloc(ACS_SM_BUF_LEN);

	for (i = 0; i < count; i++) {
		c = (chanspec_t)dtoh32(list->element[i]);

		ch_list[i].chspec = c;

		input = c;
		ret = wl_iovar_getbuf(c_info->name, "per_chan_info", &input, sizeof(chanspec_t),
			data_buf1, ACS_SM_BUF_LEN);
		ACS_ERR(ret, "failed to get per_chan_info");

		ch_list[i].chspec_info = dtoh32(*(uint32 *)data_buf1);
		ACSD_INFO("chanspec: (0x%04x), chspec_info: 0x%x\n", c, ch_list[i].chspec_info);
	}

cleanup_sl:
	ACS_FREE(data_buf);
	ACS_FREE(data_buf1);

	return ret;
}

static int
acs_scan_prep(acs_chaninfo_t *c_info, wl_scan_params_t *params, int *params_size)
{
	int ret = 0;
	int i;
	acs_scan_chspec_t* scan_chspec_p = &c_info->scan_chspec_list;

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = WL_SCANFLAGS_PASSIVE;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = ACS_CS_SCAN_DWELL;
	params->home_time = -1;
	params->channel_num = 0;

	ret = acs_build_scanlist(c_info);
	ACS_ERR(ret, "failed to build scan chanspec list");

	params->channel_num = htod32(scan_chspec_p->count & WL_SCAN_PARAMS_COUNT_MASK);
	ACSD_INFO("scan channel number: %d\n", params->channel_num);

	for (i = 0; i < scan_chspec_p->count; i++) {
		params->channel_list[i] = htodchanspec(scan_chspec_p->chspec_list[i].chspec);
	}

	*params_size = WL_SCAN_PARAMS_FIXED_SIZE + scan_chspec_p->count * sizeof(uint16);
	ACSD_INFO("params size: %d\n", *params_size);

	return ret;
}

/* channel information (quick) scan at run time */
int
acs_run_ci_scan(acs_chaninfo_t *c_info)
{
	int ret = 0;
	int i;
	wl_scan_params_t *params = NULL;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + sizeof(uint16);
	acs_scan_chspec_t* scan_chspec_q = &c_info->scan_chspec_list;
	scan_chspec_elemt_t *scan_elemt = NULL;
	bool is_dfs = FALSE;
	channel_info_t ci;

	if (scan_chspec_q->count == 0) {
		ACSD_INFO("scan chanspec queue is empty.\n");
		return ret;
	}

	scan_elemt = &scan_chspec_q->chspec_list[scan_chspec_q->idx];
	if (scan_elemt->chspec_info & WL_CHAN_RADAR)
		is_dfs = TRUE;

	params = (wl_scan_params_t*)acsd_malloc(params_size);

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = is_dfs ? WL_SCANFLAGS_PASSIVE : 0;
	params->nprobes = -1;
	params->active_time = ACS_CI_SCAN_DWELL;
	params->passive_time = ACS_CI_SCAN_DWELL;
	params->home_time = -1;
	params->channel_num = 1; /* 1 channel for each ci scan */

	params->channel_list[0] = htodchanspec(scan_elemt->chspec);
	scan_chspec_q->idx++;
	if (scan_chspec_q->idx == scan_chspec_q->count)
		scan_chspec_q->idx = 0;

	ret = wl_ioctl(c_info->name, WLC_SCAN, params, params_size);
	ACS_ERR(ret, "WLC_SCAN failed");

	if (!ret) {
		sleep_ms(ACS_CI_SCAN_DWELL * 5);
		for (i = 0; i < 10; i++) {
			ret = wl_ioctl(c_info->name, WLC_GET_CHANNEL, &ci, sizeof(channel_info_t));
			ACS_ERR(ret, "WLC_GET_CHANNEL failed");

			ci.scan_channel = dtoh32(ci.scan_channel);
			if (!ci.scan_channel)
				break;

			ACSD_PRINT("scan in progress ...\n");
			sleep_ms(2);
		}
	}
	ACSD_INFO("ci scan on chspec: 0x%x\n", scan_elemt->chspec);
	ACS_FREE(params);
	return ret;
}

/* channel selection (full) scan at init/reset time */
int
acs_run_cs_scan(acs_chaninfo_t *c_info)
{
	int ret = 0;
	int i;
	wl_scan_params_t *params = NULL;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + ACS_NUMCHANNELS * sizeof(uint16);
	channel_info_t ci;

	params = (wl_scan_params_t*)acsd_malloc(params_size);
	ret = acs_scan_prep(c_info, params, &params_size);
	ACS_ERR(ret, "failed to do scan prep");

	ret = wl_ioctl(c_info->name, WLC_SCAN, params, params_size);
	ACS_ERR(ret, "WLC_SCAN failed");

	memset(&ci, 0, sizeof(channel_info_t));
	/* loop to check if cs scan is done, check for scan in progress */
	if (!ret) {
		/* this time needs to be < 1000 to prevent mpc kicking in for 2nd radio */
		sleep_ms(ACS_CS_SCAN_DWELL);
		for (i = 0; i < 100; i++) {
			ret = wl_ioctl(c_info->name, WLC_GET_CHANNEL, &ci, sizeof(channel_info_t));
			ACS_ERR(ret, "WLC_GET_CHANNEL failed");

			ci.scan_channel = dtoh32(ci.scan_channel);
			if (!ci.scan_channel)
				break;

			ACSD_PRINT("scan in progress ...\n");
			sleep_ms(ACS_CS_SCAN_DWELL);
		}
	}
	ACS_FREE(params);
	return ret;
}

static int
acs_get_scan(char* name, char *scan_buf, uint buf_len)
{
	wl_scan_results_t *list = (wl_scan_results_t*)scan_buf;
	int ret = 0;

	list->buflen = htod32(buf_len);
	ret = wl_ioctl(name, WLC_SCAN_RESULTS, scan_buf, buf_len);
	if (ret)
		ACSD_ERROR("err from WLC_SCAN_RESULTS: %d\n", ret);

	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);
	if (list->buflen == 0) {
		list->version = 0;
		list->count = 0;
	} else if (list->version != WL_BSS_INFO_VERSION &&
	           list->version != LEGACY2_WL_BSS_INFO_VERSION &&
	           list->version != LEGACY_WL_BSS_INFO_VERSION) {
		fprintf(stderr, "Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			list->version, WL_BSS_INFO_VERSION);
		list->buflen = 0;
		list->count = 0;
	}
	ACSD_INFO("list->count: %d, list->buflen: %d\n", list->count, list->buflen);

	return ret;
}

#ifdef ACS_DEBUG
static void
acs_dump_map(void)
{
	int i;
	ifname_idx_map_t* cur_map;

	for (i = 0; i < ACS_MAX_IF_NUM; i++) {
		cur_map = &acs_info->acs_ifmap[i];
		ACSD_PRINT("i: %d, name: %s, idx: %d, in_use: %d\n",
			i, cur_map->name, cur_map->idx, cur_map->in_use);
	}
}
#endif /* ACS_DEBUG */

static void
acs_add_map(char *name)
{
	int i;
	ifname_idx_map_t* cur_map;

	ACSD_DEBUG("add map entry for ifname: %s\n", name);

	for (i = 0; i < ACS_MAX_IF_NUM; i++) {
		cur_map = &acs_info->acs_ifmap[i];
		if (!cur_map->in_use) {
			strncpy(cur_map->name, name, sizeof(name));
			cur_map->idx = i;
			cur_map->in_use = TRUE;
			break;
		}
	}
#ifdef ACS_DEBUG
	acs_dump_map();
#endif
}

int
acs_idx_from_map(char *name)
{
	int i;
	ifname_idx_map_t *cur_map;

#ifdef ACS_DEBUG
	acs_dump_map();
#endif
	for (i = 0; i < ACS_MAX_IF_NUM; i++) {
		cur_map = &acs_info->acs_ifmap[i];
		if (cur_map->in_use && !strcmp(name, cur_map->name)) {
			ACSD_DEBUG("name: %s, cur_map->name: %s idx: %d\n",
				name, cur_map->name, cur_map->idx);
			return cur_map->idx;
		}
	}
	ACSD_ERROR("cannot find the mapped entry for ifname: %s\n", name);
	return -1;
}

/* maybe we do not care about 11b anymore */
static bool
acs_bss_is_11b(wl_bss_info_t* bi)
{
	uint i;
	bool b = TRUE;

	for (i = 0; i < bi->rateset.count; i++) {
		b = bi->rateset.rates[i] & 0x80;
		if (!b)
			break;
	}
	return b;
}

static void
acs_parse_chanspec(chanspec_t chanspec, uint* channel, uint* ext)
{
	*channel = *ext = CHSPEC_CHANNEL(chanspec);

	if (CHSPEC_IS40(chanspec)) {
		if (CHSPEC_SB_LOWER(chanspec)) {
			*channel -= CH_10MHZ_APART;
			*ext += CH_10MHZ_APART;
		}
		else if (CHSPEC_SB_UPPER(chanspec)) {
			*channel += CH_10MHZ_APART;
			*ext -= CH_10MHZ_APART;
		}
	}
	ACSD_DEBUG("chanspec: %x, channel: %d, ext: %d\n", chanspec, *channel, *ext);
}

#ifdef ACS_DEBUG
static void
acs_dump_chan_bss(acs_chan_bssinfo_t* bssinfo, int ncis)
{
	int c;
	acs_chan_bssinfo_t *cur;

	printf("channel    aAPs bAPs gAPs lSBs uSBs nEXs\n");

	for (c = 0; c < ncis; c ++) {
		cur = &bssinfo[c];
		printf("%2d\t%5d%5d%5d%5d%5d%5d\n", cur->channel, cur->aAPs,
			cur->bAPs, cur->gAPs, cur->lSBs, cur->uSBs, cur->nEXs);
	}
}
#endif /* ACS_DEBUG */

void
acs_expire_scan_entry(acs_chaninfo_t *c_info, time_t limit)
{
	time_t now;
	acs_bss_info_entry_t *curptr, *previous = NULL, *past;
	acs_bss_info_entry_t **rootp = &c_info->acs_bss_info_q;

	curptr = *rootp;
	now = time(NULL);

	while (curptr) {
		time_t diff = now - curptr->timestamp;
		if (diff > limit) {
			if (previous == NULL)
				*rootp = curptr->next;
			else
				previous->next = curptr->next;

			past = curptr;
			curptr = curptr->next;
			ACS_FREE(past);
			continue;
		}
		previous = curptr;
		curptr = curptr->next;
	}
}

static void
acs_cleanup_scan_entry(acs_chaninfo_t *c_info)
{
	acs_bss_info_entry_t *headptr = c_info->acs_bss_info_q;
	acs_bss_info_entry_t *curptr;

	while (headptr) {
		curptr = headptr;
		headptr = headptr->next;
		ACS_FREE(curptr);
	}
}

static void
display_scan_entry_local(acs_bss_info_sm_t * bsm)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	wl_format_ssid(ssidbuf, bsm->SSID, bsm->SSID_len);

	printf("SSID: \"%s\"\n", ssidbuf);
	printf("BSSID: %s\t", wl_ether_etoa(&bsm->BSSID));
	printf("chanspec: 0x%x\n", bsm->chanspec);
	printf("RSSI: %d dBm\t", (int16)bsm->RSSI);
	printf("Type: %s", ((bsm->type == ACS_BSS_TYPE_11A) ? "802.11A" :
		((bsm->type == ACS_BSS_TYPE_11G) ? "802.11G" : "802.11B")));
	printf("\n");
}

void
acs_dump_scan_entry(acs_chaninfo_t *c_info)
{
	acs_bss_info_entry_t *curptr = c_info->acs_bss_info_q;

	while (curptr) {
		display_scan_entry_local(&curptr->binfo_local);
		printf("timestamp: %u\n", (uint32)curptr->timestamp);
		curptr = curptr->next;
	}
}

static int
acs_insert_scan_entry(acs_chaninfo_t *c_info, acs_bss_info_entry_t * new)
{
	acs_bss_info_entry_t *curptr, *previous = NULL;
	acs_bss_info_entry_t **rootp = &c_info->acs_bss_info_q;

	curptr = *rootp;
	previous = curptr;

	while (curptr &&
	   memcmp(&curptr->binfo_local.BSSID, &new->binfo_local.BSSID, sizeof(struct ether_addr))) {
		previous = curptr;
		curptr = curptr->next;
	}
	new->next = curptr;
	if (previous == NULL)
		*rootp = new;
	else {
		if (curptr == NULL)
			previous->next = new;
		else /* find an existing entry */ {
			curptr->timestamp = new->timestamp;
			memcpy(&new->binfo_local, &curptr->binfo_local, sizeof(acs_bss_info_sm_t));
			ACS_FREE(new);
		}
	}
	return 0;
}

static int
acs_update_scanresult_queue(acs_chaninfo_t *c_info)
{
	wl_scan_results_t* s_result = c_info->scan_results;
	wl_bss_info_t *bi = bi = s_result->bss_info;
	int b, len = 0;
	acs_bss_info_entry_t * new_entry = NULL;
	uint channel, ext;
	chanspec_t cur_chspec;

	for (b = 0; b < s_result->count; b ++, bi = (wl_bss_info_t*)((int8*)bi + len)) {

		len = dtoh32(bi->length);
		new_entry = (acs_bss_info_entry_t*)acsd_malloc(sizeof(acs_bss_info_entry_t));

		new_entry->binfo_local.chanspec = cur_chspec = dtoh16(bi->chanspec);
		new_entry->binfo_local.RSSI = dtoh16(bi->RSSI);
		new_entry->binfo_local.SSID_len = bi->SSID_len;
		memcpy(new_entry->binfo_local.SSID, bi->SSID, bi->SSID_len);
		memcpy(&new_entry->binfo_local.BSSID, &bi->BSSID, sizeof(struct ether_addr));
		new_entry->timestamp = time(NULL);

		acs_parse_chanspec(cur_chspec, &channel, &ext);
			/* BSS type in 2.4G band */
		if (channel <= ACS_CS_MAX_2G_CHAN) {
			if (acs_bss_is_11b(bi))
				new_entry->binfo_local.type = ACS_BSS_TYPE_11B;
			else
				new_entry->binfo_local.type = ACS_BSS_TYPE_11G;
		}
		else
			new_entry->binfo_local.type = ACS_BSS_TYPE_11A;

		acs_insert_scan_entry(c_info, new_entry);
	}
	return 0;
}

static int
acs_update_chan_bssinfo(acs_chaninfo_t *c_info)
{
	acs_bss_info_entry_t *biq;
	scan_chspec_elemt_t* chspec_list;
	char * new_buf = NULL;
	uint channel, ext;
	chanspec_t cur_chspec;
	int count = 0, buf_size, c;
	acs_chan_bssinfo_t *bss_info;

	count = c_info->scan_chspec_list.count;
	chspec_list = c_info->scan_chspec_list.chspec_list;

	if (count == 0)
		return 0;

	buf_size = sizeof(acs_chan_bssinfo_t) * count;
	new_buf = acsd_malloc(buf_size);

	bss_info = (acs_chan_bssinfo_t *) new_buf;

	for (c = 0; c < count; c ++) {
		bzero(&bss_info[c], sizeof(acs_chan_bssinfo_t));

		biq = c_info->acs_bss_info_q;
		/* set channel range centered by the scan channel */
		bss_info[c].channel = CHSPEC_CHANNEL(chspec_list[c].chspec);
		ACSD_DEBUG("count: %d, channel: %d\n", c, bss_info[c].channel);

		while (biq) {
			assert(biq);
			cur_chspec = biq->binfo_local.chanspec;
			acs_parse_chanspec(cur_chspec, &channel, &ext);

			/* skip bss not on the scan channel or adjacent channels */
			if ((channel != bss_info[c].channel) &&
				(ext != bss_info[c].channel)) {
				goto next_entry;
			}

			/* count 11n ctl sidebands */
			if (CHSPEC_IS40(cur_chspec) && channel == bss_info[c].channel) {
				if (CHSPEC_SB_LOWER(cur_chspec)) {
					if (bss_info[c].lSBs < MAXNBVAL(sizeof(bss_info[c].lSBs)))
						bss_info[c].lSBs ++;
				}
				else if (CHSPEC_SB_UPPER(cur_chspec)) {
					if (bss_info[c].uSBs < MAXNBVAL(sizeof(bss_info[c].uSBs)))
						bss_info[c].uSBs ++;
				}
			}
			/* count 11n extensions */
			if (CHSPEC_IS40(cur_chspec) && ext == bss_info[c].channel) {
				if (bss_info[c].nEXs < MAXNBVAL(sizeof(bss_info[c].nEXs)))
					bss_info[c].nEXs ++;
				goto next_entry;
			}

			/* count BSSs in 2.4G band */
			if (bss_info[c].channel <= ACS_CS_MAX_2G_CHAN) {
				if (biq->binfo_local.type == ACS_BSS_TYPE_11B) {
					if (bss_info[c].bAPs < MAXNBVAL(sizeof(bss_info[c].bAPs)))
						bss_info[c].bAPs ++;
				}
				else {
					if (bss_info[c].gAPs < MAXNBVAL(sizeof(bss_info[c].gAPs)))
						bss_info[c].gAPs ++;
				}
			}
			/* count BSSs in 5G band */
			else {
				if (bss_info[c].aAPs < MAXNBVAL(sizeof(bss_info[c].aAPs)))
					bss_info[c].aAPs ++;
			}
next_entry:
			biq = biq->next;
		}
		ACSD_DEBUG(" channel %u: %u aAPs %u bAPs %u gAPs %u lSBs %u uSBs %u nEXs\n",
			bss_info[c].channel, bss_info[c].aAPs, bss_info[c].bAPs,
			bss_info[c].gAPs, bss_info[c].lSBs, bss_info[c].uSBs, bss_info[c].nEXs);
	}

	ACS_FREE(c_info->ch_bssinfo);
	c_info->ch_bssinfo = (acs_chan_bssinfo_t *) new_buf;

#ifdef ACS_DEBUG
	acs_dump_chan_bss(c_info->ch_bssinfo, c_info->scan_chspec_list.count);
	acs_dump_scan_entry(c_info);
#endif /* ACS_DEBUG */

	return 0;
}

/* radio setting information needed from the driver */
static int
acs_get_rs_info(acs_chaninfo_t * c_info, char* prefix)
{
	int ret = 0;
	char tmp[100];
	int bwcap;
	int band, channel, pref_chspec, coex;
	acs_rsi_t *rsi = &c_info->rs_info;

	channel = atoi(nvram_safe_get(strcat_r(prefix, "channel", tmp)));

	if (channel != 0) {
		wl_iovar_getint(c_info->name, "chanspec", &pref_chspec);
		wl_iovar_setint(c_info->name, "pref_chanspec", pref_chspec);
		rsi->pref_chspec = dtoh32(pref_chspec);
	}
	else
		wl_iovar_setint(c_info->name, "pref_chanspec", 0);

	ret = wl_iovar_getint(c_info->name, "obss_coex", &coex);
	ACS_ERR(ret, "failed to get obss_coex");

	rsi->coex_enb = dtoh32(coex);
	ACSD_INFO("coex_enb: %d\n",  rsi->coex_enb);

	ret = wl_iovar_getint(c_info->name, "mimo_bw_cap", &bwcap);
	ACS_ERR(ret, "failed to get miom_bw_cap");

	rsi->bw_cap = bwcap;
	ACSD_INFO("bw_cap: %d\n",  rsi->bw_cap);

	ret = wl_ioctl(c_info->name, WLC_GET_BAND, &band, sizeof(band));
	ACS_ERR(ret, "failed to get band info");

	rsi->band_type = dtoh32(band);
	ACSD_INFO("band_type: %d\n",  rsi->band_type);

	return ret;
}

int
acs_request_data(acs_chaninfo_t *c_info)
{
	int ret = 0;

	char *dump_buf = acsd_malloc(ACS_SRSLT_BUF_LEN);

	ret = acs_get_scan(c_info->name, dump_buf, ACS_SRSLT_BUF_LEN);

	ACS_FREE(c_info->scan_results);
	c_info->scan_results = (wl_scan_results_t *)dump_buf;

	acs_update_scanresult_queue(c_info);
	acs_update_chan_bssinfo(c_info);

	acsd_chanim_query(c_info, WL_CHANIM_COUNT_ALL, 0);

	return ret;
}

static chanspec_t
acs_pick_chanspec_default(acs_chaninfo_t* c_info, int bw)
{
	chanspec_t chspec = 0;
	int i, index = -1;
	int min_score = 5000;
	ch_candidate_t *candi = c_info->candidate[bw];
	ch_score_t *score_p;
	ch_score_t *best_score_p;
	chanim_config_t* config = &c_info->chanim_info->config;

	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;

	/* find the chanspec with best noise figure (cns) */
	best_score_p = candi[0].chscore;
	for (i = 0; i < c_info->c_count[bw]; i++) {
		stats = &ch_stats->stats[i];
		score_p = candi[i].chscore;

		if ((!candi[i].valid) && (candi[i].reason & ACS_INVALID_OVLP))
			continue;

		ACSD_CHANIM("score chanspec %x = %d\n",
			candi[i].chspec, score_p[CH_SCORE_CNS].score);

		if (score_p[CH_SCORE_CNS].score < best_score_p[CH_SCORE_CNS].score) {
			best_score_p = score_p;
		}
	}

	/* ban chanspec that are too far away from best figure */
	for (i = 0; i < c_info->c_count[bw]; i++) {
		stats = &ch_stats->stats[i];
		score_p = candi[i].chscore;

		if ((!candi[i].valid) && (candi[i].reason & ACS_INVALID_OVLP))
			continue;

		if (score_p[CH_SCORE_CNS].score >=
			(best_score_p[CH_SCORE_CNS].score + config->acs_trigger_var)) {
			ACSD_CHANIM("banning chanspec %x because of interference \n",
				candi[i].chspec);

			candi[i].valid = FALSE;
			candi[i].reason |= ACS_INVALID_NOISE;
		}
	}

	/* select chanspec that has less bss */
	for (i = 0; i < c_info->c_count[bw]; i++) {
		if (!candi[i].valid)
			continue;

		score_p = candi[i].chscore;

		/* if number of bss is equal, pick based of distance from interfer */
		if (score_p[CH_SCORE_BSS].score == min_score) {
			int intadj_score = candi[index].chscore[CH_SCORE_INTFADJ].score;
			if (score_p[CH_SCORE_INTFADJ].score < intadj_score)
				index = i;
		}
		if (score_p[CH_SCORE_BSS].score < min_score) {
			index = i;
			min_score = score_p[CH_SCORE_BSS].score;
		}
	}

	/* reset monitoring state machine */
	chanim_mark(c_info->chanim_info).best_score = 0;

	if (index >= 0) {

		chspec = candi[index].chspec;
		score_p = candi[index].chscore;

		chspec = candi[index].chspec;
		ACSD_INFO("less bss found on chanspec %x\n", chspec);
	}
	return chspec;
}

static chanspec_t
acs_pick_chanspec(acs_chaninfo_t* c_info, int bw)
{
	chanspec_t chspec = 0;
	int i, index = -1;
	int max_score = ACS_MIN_SCORE;
	ch_score_t *score_p;
	ch_candidate_t *candi = c_info->candidate[bw];

	/* pick the chanspec with the highest total score */
	for (i = 0; i < c_info->c_count[bw]; i++) {
		if (!candi[i].valid)
			continue;

		score_p = candi[i].chscore;
		if (score_p[CH_SCORE_TOTAL].score > max_score) {
			index = i;
			max_score = score_p[CH_SCORE_TOTAL].score;
		}
	}

	chspec = candi[index].chspec;
	score_p = candi[index].chscore;

	ACSD_INFO("The highest score: %d, chspec: 0x%x\n",
		max_score, chspec);

	return chspec;
}

void
acs_default_policy(acs_policy_t *a_pol, uint index)
{
	if (index > ACS_POLICY_USER) {
		ACSD_ERROR("Invalid acs policy index\n");
		index = ACS_DFLT_POLICY;
	}

	memcpy(a_pol, &predefined_policy[index], sizeof(acs_policy_t));

	if (index == ACS_DFLT_POLICY)
		a_pol->chan_selector = acs_pick_chanspec_default;
	else
		a_pol->chan_selector = acs_pick_chanspec;
}

#ifdef DEBUG
static void
acs_dump_policy(acs_policy_t *a_pol)
{
	printf("ACS Policy:\n");
	printf("Bg Noise threshold: %d\n", a_pol->bgnoise_thres);
	printf("Interference threshold: %d\n", a_pol->intf_threshold);
	printf("Channel Scoring Weights: \n");
	printf("\t BSS: %d\n", a_pol->acs_weight[CH_SCORE_BSS]);
	printf("\t BUSY: %d\n", a_pol->acs_weight[CH_SCORE_BUSY]);
	printf("\t INTF: %d\n", a_pol->acs_weight[CH_SCORE_INTF]);
	printf("\t INTFADJ: %d\n", a_pol->acs_weight[CH_SCORE_INTFADJ]);
	printf("\t FCS: %d\n", a_pol->acs_weight[CH_SCORE_FCS]);
	printf("\t TXPWR: %d\n", a_pol->acs_weight[CH_SCORE_TXPWR]);
	printf("\t BGNOISE: %d\n", a_pol->acs_weight[CH_SCORE_BGNOISE]);
	printf("\t CNS: %d\n", a_pol->acs_weight[CH_SCORE_CNS]);

}
#endif /* DEBUG */

static void
acs_retrieve_config(acs_chaninfo_t *c_info, char * prefix)
{
	/* retrieve policy related configuration from nvram */
	char conf_word[128], conf_var[16], tmp[100];
	char *next;
	int i = 0, val;
	uint8 index;
	acs_policy_t *a_pol = &c_info->acs_policy;
	uint32 flags;

	/* the current layout of config */
	ACSD_INFO("retrieve config from nvram ...\n");

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_flags", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs flag set. Retrieve default.\n");
		flags = ACS_DFLT_FLAGS;
	}
	else {
		char *endptr = NULL;
		flags = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs flags: 0x%x\n", flags);
	}

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_pol", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs policy set. Retrieve default.\n");

		acs_safe_get_conf(conf_word, sizeof(conf_word),
			strcat_r(prefix, "acs_pol_idx", tmp));

		if (!strcmp(conf_word, ""))
			index = ACS_DFLT_POLICY;
		else
			index = atoi(conf_word);

		acs_default_policy(a_pol, index);

		goto config_end;
	}

	index = ACS_POLICY_USER;
	foreach(conf_var, conf_word, next) {
		val = atoi(conf_var);
		ACSD_DEBUG("i: %d conf_var: %s val: %d\n", i, conf_var, val);

		if (i == 0)
			a_pol->bgnoise_thres = val;
		else if (i == 1)
			a_pol->intf_threshold = val;
		else {
			a_pol->acs_weight[i - 2] = val;
			ACSD_DEBUG("weight No. %d, value: %d\n", i-2, val);
		}
		i++;
	}
	a_pol->chan_selector = acs_pick_chanspec;

config_end:
	c_info->flags = flags;
	c_info->policy_index = index;
#ifdef DEBUG
	acs_dump_policy(a_pol);
#endif
}

static int
acs_start(char *name, acs_chaninfo_t ** c_info_ptr)
{
	int unit;
	char prefix[PREFIX_LEN], tmp[100];
	int index, ret = 0;
	acs_chaninfo_t *c_info;
	acs_rsi_t* rsi;

	ACSD_INFO("acs_start for interface %s\n", name);

	index = acs_idx_from_map(name);

	/* allocate core data structure for this if */
	c_info = acs_info->chan_info[index] =
		(acs_chaninfo_t*)acsd_malloc(sizeof(acs_chaninfo_t));

	strncpy(c_info->name, name, sizeof(name));

	ret = wl_ioctl(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	/* check radio */
	if (nvram_match(strcat_r(prefix, "radio", tmp), "0")) {
		ACSD_INFO("ifname %s: radio is off\n", name);
		c_info->mode = ACS_MODE_DISABLE;
		goto acs_start_done;
	}

	acs_retrieve_config(c_info, prefix);

	rsi = &c_info->rs_info;
	acs_get_rs_info(c_info, prefix);

	if (rsi->pref_chspec == 0 || rsi->coex_enb) {
		c_info->mode = ACS_MODE_SELECT;
	}
	else
		c_info->mode = ACS_MODE_MONITOR; /* default mode */

	ACSD_DEBUG("ifname %s - mode: %s\n", name,
		AUTOCHANNEL(c_info)? "SELECT" : "MONITOR");

	ret = acsd_chanim_init(c_info);
	ACS_ERR(ret, "chanim init failed\n");

	if (!AUTOCHANNEL(c_info))
		goto acs_start_done;

	ret = acs_run_cs_scan(c_info);
	ACS_ERR(ret, "cs scan failed\n");

	ACS_FREE(c_info->acs_bss_info_q);

	ret = acs_request_data(c_info);
	ACS_ERR(ret, "request data failed\n");

acs_start_done:
	*c_info_ptr = c_info;
	return ret;
}

static int
acs_build_candidates(acs_chaninfo_t *c_info, int bw)
{
	wl_uint32_list_t *list;
	chanspec_t input = 0, c = 0;
	int ret = 0, i, j;
	int count = 0;
	ch_candidate_t *candi;
	acs_rsi_t *rsi = &c_info->rs_info;

	char *data_buf;
	data_buf = acsd_malloc(ACS_SM_BUF_LEN);

	if (bw == ACS_BW_40)
		input |= WL_CHANSPEC_BW_40;
	else
		input |= WL_CHANSPEC_BW_20;

	if (BAND_5G(rsi->band_type))
		input |= WL_CHANSPEC_BAND_5G;
	else
		input |= WL_CHANSPEC_BAND_2G;

	ret = wl_iovar_getbuf(c_info->name, "chanspecs", &input, sizeof(chanspec_t),
		data_buf, ACS_SM_BUF_LEN);
	ACS_ERR(ret, "failed to get valid chanspec lists");

	list = (wl_uint32_list_t *)data_buf;
	count = dtoh32(list->count);

	if (!count) {
		ACSD_ERROR("number of valid chanspec is 0\n");
		ret = -1;
		goto cleanup;
	}

	ACS_FREE(c_info->candidate[bw]);
	c_info->candidate[bw] = (ch_candidate_t*)acsd_malloc(count * sizeof(ch_candidate_t));
	candi = c_info->candidate[bw];

	ACSD_DEBUG("address of candi: 0x%x\n", (uint32)candi);
	for (i = 0; i < count; i++) {
		c = (chanspec_t)dtoh32(list->element[i]);
		candi[i].chspec = c;
		candi[i].valid = TRUE;
		/* assign weight based on config */
		for (j = 0; j < CH_SCORE_MAX; j++) {
			candi[i].chscore[j].weight = c_info->acs_policy.acs_weight[j];
			ACSD_DEBUG("chanspec: (0x%04x) score: %d, weight: %d\n",
				c, candi[i].chscore[j].score, candi[i].chscore[j].weight);
		}
	}
	c_info->c_count[bw] = count;

cleanup:
	if (data_buf)
		free(data_buf);
	return ret;
}

/*
 * 20/40 Coex compliance check:
 * Return a 20/40 Coex compatible chanspec based on the scan data.
 * Verify that the 40MHz input_chspec passes 20/40 Coex rules.
 * If so, return the same chanspec.
 * Otherwise return a 20MHz chanspec which is centered on the
 * input_chspec's control channel.
 */
static chanspec_t
acs_coex_check(acs_chaninfo_t* c_info, chanspec_t input_chspec)
{
	int forty_center;
	uint ctrl_ch, ext_ch;
	bool upperSB;
	chanspec_t chspec_out;
	int ci_index, ninfo = c_info->scan_chspec_list.count;
	bool conflict = FALSE;
	acs_chan_bssinfo_t *ci = c_info->ch_bssinfo;
	char err_msg[128];

	if (!CHSPEC_IS40(input_chspec))
		ACSD_ERROR("input channel spec is not 40MHz!");

	/* this will get us the center of the input 40MHz channel */
	forty_center = CHSPEC_CHANNEL(input_chspec);
	upperSB = CHSPEC_SB_UPPER(input_chspec);

	acs_parse_chanspec(input_chspec, &ctrl_ch, &ext_ch);

	ACSD_DEBUG("InputChanspec:  40Center %d, CtrlCenter %d, ExtCenter %d\n",
	          forty_center, ctrl_ch, ext_ch);

	/* Loop over scan data looking for interferance based on 20/40 Coex Rules. */
	for (ci_index = 0; ci_index < ninfo; ci_index++) {
		ACSD_DEBUG("Examining ci[%d].channel = %d, forty_center-5 = %d, "
		          "forty_center+5 = %d\n",
		          ci_index, ci[ci_index].channel, forty_center - WLC_2G_25MHZ_OFFSET,
		          forty_center+WLC_2G_25MHZ_OFFSET);

		/* Ignore any channels not within the range we care about.
		 * 20/40 Coex rules for 2.4GHz:
		 * Must look at all channels where a 20MHz BSS would overlap with our
		 * 40MHz BW + 5MHz on each side.  This means that we must inspect any channel
		 * within 5 5MHz channels of the center of our 40MHz chanspec.
		 *
		 * Example:
		 * 40MHz Chanspec centered on Ch.8
		 *              +5 ----------40MHz-------------  +5
		 *              |  |           |              |   |
		 * -1  0  1  2  3  4  5  6  7  8  9  10  11  12  13  14
		 *
		 * Existing 20MHz BSS on Ch. 1 (Doesn't interfere with our 40MHz AP)
		 *  -----20MHz---
		 *  |     |     |
		 * -1  0  1  2  3  4  5  6  7  8  9  10  11  12  13  14
		 *
		 * Existing 20MHz BSS on Ch. 3 (Does interfere our 40MHz AP)
		 *        -----20MHz---
		 *        |     |     |
		 * -1  0  1  2  3  4  5  6  7  8  9  10  11  12  13  14
		 *
		 *  In this example, we only pay attention to channels in the range of 3 thru 13.
		 */

		if (ci[ci_index].channel < forty_center - WLC_2G_25MHZ_OFFSET ||
		    ci[ci_index].channel > forty_center + WLC_2G_25MHZ_OFFSET) {
			ACSD_DEBUG("Not in range, continue.\n");
			continue;
		}

		ACSD_DEBUG("In range.\n");

		/* Is there an existing BSS? */
		if (ci[ci_index].bAPs || ci[ci_index].gAPs || ci[ci_index].lSBs ||
		    ci[ci_index].uSBs || ci[ci_index].nEXs) {
			ACSD_DEBUG("Existing BSSs on channel %d\n", ci[ci_index].channel);

			/* Existing BSS is ONLY okay if:
			 * Our control channel is aligned with existing 20 or Control Channel
			 * Our extension channel is aligned with an existing extension channel
			 */
			if (ci[ci_index].channel == ctrl_ch) {
				ACSD_DEBUG("Examining ctrl_ch\n");

				/* Two problems that we need to detect here:
				 *
				 * 1:  If this channel is being used as a 40MHz extension.
				 * 2:  If this channel is being used as a control channel for an
				 *     existing 40MHz, we must both use the same CTRL sideband
				 */
				if (ci[ci_index].nEXs) {
					snprintf(err_msg, sizeof(err_msg), "ctrl channel: %d"
						" existing ext. channel", ctrl_ch);
					conflict = TRUE;
					break;
				} else if ((upperSB && ci[ci_index].lSBs) ||
				           (!upperSB && ci[ci_index].uSBs)) {
					snprintf(err_msg, sizeof(err_msg), "ctrl channel %d"
						" SB not aligned with existing 40BSS", ctrl_ch);
					conflict = TRUE;
					break;
				}
			} else if (ci[ci_index].channel == ext_ch) {
				ACSD_DEBUG("Examining ext_ch\n");

				/* Any BSS using this as it's center is an interferance */
				if (ci[ci_index].bAPs || ci[ci_index].gAPs || ci[ci_index].lSBs ||
				    ci[ci_index].uSBs) {
					snprintf(err_msg, sizeof(err_msg), "ext channel %d"
						" used as ctrl channel by existing BSSs", ext_ch);
					conflict = TRUE;
					break;
				}
			} else {
				/* If anyone is using this channel, it's an conflict */
				conflict = TRUE;
				snprintf(err_msg, sizeof(err_msg),
					"channel %d used by exiting BSSs ", ci[ci_index].channel);
				break;
			}
		}
	}

	if (conflict) {
		chspec_out = CH20MHZ_CHSPEC(ctrl_ch);
		if (c_info->rs_info.pref_chspec)
			ACSD_PRINT("COEX: downgraded chanspec 0x%x to 0x%x: %s\n",
				input_chspec, chspec_out, err_msg);
	} else {
		chspec_out = input_chspec;
		ACSD_DEBUG("No conflict found, returning 40MHz chanspec 0x%x\n",
		          chspec_out);
	}
	return chspec_out;
}

static bool
acs_has_valid_candidate(acs_chaninfo_t* c_info, int bw)
{
	int i;
	bool result = FALSE;
	ch_candidate_t* candi = c_info->candidate[bw];

	for (i = 0; i < c_info->c_count[bw]; i++) {
		if (candi[i].valid) {
			result = TRUE;
			break;
		}
	}
	ACSD_DEBUG("result: %d\n", result);
	return result;
}

/*
 * Individual scoring algorithm. It is subject to tuning or customization based on
 * testing results or customer requirement.
 *
 */
static int
acs_chan_score_bss(uint channel, acs_chan_bssinfo_t* bss_info, int ncis,
	bool ovlp, bool ctrl)
{
	int score = 0;
	int i, min, max;
	int ch;

	min = max = (int)channel;

	if (ovlp) {
		min = (int)channel - CH_10MHZ_APART;
		max = (int)channel + CH_10MHZ_APART;
	}

	for (i = 0; i < ncis; i++) {
		ch = (int)bss_info[i].channel;

		ACSD_DEBUG("ch: %d, min: %d, max: %d\n", ch, min, max);
		if (ch < min || ch > max)
			continue;

		score += bss_info[i].aAPs + bss_info[i].bAPs + bss_info[i].gAPs;

		if (ctrl) {

			score += bss_info[i].lSBs + bss_info[i].uSBs;
			/* take a penalty for extension channel */
			score += ACS_NOT_ALIGN_WT * bss_info[i].nEXs;
		}
		else {
			score += bss_info[i].nEXs;
			score += ACS_NOT_ALIGN_WT * (bss_info[i].lSBs + bss_info[i].uSBs);
		}
		ACSD_DEBUG("i: %d, score: %d\n", i, score);
	}

	ACSD_INFO("channel: %d, score_bss: %d\n", channel, score);
	return score;
}

static void
acs_candidate_score_bss(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	acs_chan_bssinfo_t* bss_info = c_info->ch_bssinfo;
	uint channel, ext = 0;
	int score = 0;
	int ncis = c_info->scan_chspec_list.count;
	bool ovlp = FALSE;

	acs_parse_chanspec(candi->chspec, &channel, &ext);

	if (CHSPEC_IS2G(candi->chspec))
		ovlp = TRUE;
	score += acs_chan_score_bss(channel, bss_info, ncis, ovlp, TRUE);


	candi->chscore[CH_SCORE_BSS].score = score;
	ACSD_DEBUG("bss score: %d for chanspec 0x%x\n", score, candi->chspec);
}

static void
acs_candidate_score_busy(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	uint channel, ext = 0;
	int i, ch, score = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(candi->chspec, &channel, &ext);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if ((ch == channel) || (ch == ext)) {
			score += stats->ccastats[CCASTATS_OBSS];
			hits ++;
		}

		if (hits == 2 || (hits && channel == ext)) {
			done = TRUE;
			break;
		}
	}
	if (!done) {
		ACSD_ERROR("busy check failed for chanspec: 0x%x\n", chspec);
		return;
	}

	if (hits)
		candi->chscore[CH_SCORE_BUSY].score = score/hits;
	ACSD_DEBUG("busy score: %d for chanspec 0x%x\n", score, chspec);
}

static void
acs_candidate_score_intf(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	uint channel, ext = 0;
	int i, ch, score = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(chspec, &channel, &ext);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if (ch == channel || ch == ext) {
			score += stats->ccastats[CCASTATS_NOPKT];
			hits ++;
		}

		if (hits == 2 || (hits && channel == ext)) {
			done = TRUE;
			break;
		}
	}
	if (!done) {
		ACSD_ERROR("intf check failed for chanspec: 0x%x\n", chspec);
		return;
	}

	if (hits)
		candi->chscore[CH_SCORE_INTF].score = score/hits;
	ACSD_DEBUG("intf score: %d for chanspec 0x%x\n", score, chspec);
}

static void
acs_candidate_score_intfadj(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	uint channel, ext = 0;
	int i, ch, score = 0;
	int dist, d_weight = 10;

	acs_parse_chanspec(chspec, &channel, &ext);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);
		ACSD_DEBUG("channel: %d, ch: %d\n", channel, ch);

		if (ch != channel) {
			dist = ch - channel;
			dist = dist > 0 ? dist : dist * -1;
			score += stats->ccastats[CCASTATS_NOPKT] * d_weight / dist;

			ACSD_DEBUG("dist: %d, count: %d, score: %d\n",
				dist, stats->ccastats[CCASTATS_NOPKT], score);
			if (ext != channel && ch != ext) {
				dist = ABS(ch - ext);
				score += stats->ccastats[CCASTATS_NOPKT] * d_weight / dist;
			}

			ACSD_DEBUG("channel: %d, ch: %d score: %d\n", channel, ch, score);
		}
	}

	candi->chscore[CH_SCORE_INTFADJ].score = score / d_weight;
	ACSD_DEBUG("intf_adj score: %d for chanspec 0x%x\n", score, chspec);
}

static void
acs_candidate_score_fcs(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	uint channel, ext = 0;
	int i, ch, score = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(chspec, &channel, &ext);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if (ch == channel || ch == ext) {
			score += stats->ccastats[CCASTATS_NOCTG];
			hits ++;
		}
		if (hits == 2 || (hits && channel == ext)) {
			done = TRUE;
			break;
		}
	}
	if (!done) {
		ACSD_ERROR("fcs check failed for chanspec: 0x%x\n", chspec);
		return;
	}

	if (hits)
		candi->chscore[CH_SCORE_FCS].score = score/hits;
	ACSD_DEBUG("fcs score: %d for chanspec 0x%x\n", score, chspec);
}

static void
acs_candidate_score_txpwr(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	/* TBD */
}

static void
acs_candidate_score_bgnoise(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	uint channel, ext = 0;
	int i, ch, score = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(chspec, &channel, &ext);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if (ch == channel || ch == ext) {
			if (stats->bgnoise && stats->bgnoise > ACS_BGNOISE_BASE) {
				score += stats->bgnoise - ACS_BGNOISE_BASE;
			}
			hits ++;
		}
		if (hits == 2 || (hits && channel == ext)) {
			done = TRUE;
			break;
		}
	}
	if (!done) {
		ACSD_ERROR("bgnoise check failed for chanspec: 0x%x\n", chspec);
		return;
	}
	if (hits)
		candi->chscore[CH_SCORE_BGNOISE].score = score/hits;
	ACSD_DEBUG("fcs score: %d for chanspec 0x%x\n", score, chspec);
}

static void
acs_candidate_score_total(ch_candidate_t *candi)
{
	int i, total;
	ch_score_t *score_p;

	total = 0;
	score_p = candi->chscore;

	for (i = 0; i < CH_SCORE_TOTAL; i++)
		total += score_p[i].score * score_p[i].weight;
	score_p[CH_SCORE_TOTAL].score = total;
}

static void
acs_candidate_score_cns(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	uint channel, ext = 0;
	int i, ch, score = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(chspec, &channel, &ext);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if (ch == channel || ch == ext) {
			score += stats->bgnoise;
			score += chanim_txop_to_noise(stats->ccastats[CCASTATS_TXOP]);
			hits ++;
		}
		if (hits == 2 || (hits && channel == ext)) {
			done = TRUE;
			break;
		}
	}

	if (!done) {
		ACSD_ERROR("knoise check failed for chanspec: 0x%x\n", chspec);
		return;
	}
	candi->chscore[CH_SCORE_CNS].score = score;
	ACSD_INFO("Composite Noise Score (CNS): %d for chanspec 0x%x\n",
		score, chspec);
}


static void
acs_candidate_score(acs_chaninfo_t* c_info, int bw)
{
	ch_score_t *score_p;
	ch_candidate_t* candi;
	int i;

	for (i = 0; i < c_info->c_count[bw]; i++) {
		candi = &c_info->candidate[bw][i];
		if (!candi->valid)
			continue;
		score_p = candi->chscore;

		ACSD_DEBUG("calc score for candidate chanspec: 0x%x\n",
			candi->chspec);

		/* calculate the score for each factor */
		if (score_p[CH_SCORE_BSS].weight)
			acs_candidate_score_bss(candi, c_info);

		if (score_p[CH_SCORE_BUSY].weight)
			acs_candidate_score_busy(candi, c_info);

		if (score_p[CH_SCORE_INTF].weight)
			acs_candidate_score_intf(candi, c_info);

		if (score_p[CH_SCORE_INTFADJ].weight)
			acs_candidate_score_intfadj(candi, c_info);

		if (score_p[CH_SCORE_FCS].weight)
			acs_candidate_score_fcs(candi, c_info);

		if (score_p[CH_SCORE_TXPWR].weight)
			acs_candidate_score_txpwr(candi, c_info);

		if (score_p[CH_SCORE_BGNOISE].weight)
			acs_candidate_score_bgnoise(candi, c_info);

		acs_candidate_score_cns(candi, c_info);

		acs_candidate_score_total(candi);
#ifdef ACS_DEBUG
		acs_dump_score(score_p);
#endif
	}
}

static void
acs_candidate_check_intf(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	uint channel, ext = 0;
	int i, ch, intf = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(chspec, &channel, &ext);
	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if (ch == channel || ch == ext) {
			if (c_info->flags & ACS_FLAGS_INTF_THRES_CCA) {
				intf = stats->ccastats[CCASTATS_NOPKT];
				if (intf > c_info->acs_policy.intf_threshold) {
					candi->valid = FALSE;
					candi->reason |= ACS_INVALID_INTF_CCA;
					break;
				}
			}
			if (c_info->flags & ACS_FLAGS_INTF_THRES_BGN) {
				intf = stats->bgnoise;
				if (intf && intf > c_info->acs_policy.bgnoise_thres) {
					candi->valid = FALSE;
					candi->reason |= ACS_INVALID_INTF_BGN;
					break;
				}
			}
			hits ++;
		}
		if (hits == 2 || (hits && (channel == ext))) {
			done = TRUE;
			break;
		}
	}
	if (!done) {
		ACSD_ERROR("intf check failed for chanspec: 0x%x\n", chspec);
		return;
	}
}

void
acs_select_chspec(acs_chaninfo_t *c_info)
{
	bool need_coex_check = FALSE;
	chanspec_t selected = 0, in_chspec = 0, out_chspec = 0;
	ch_candidate_t *candi;
	int bw = ACS_BW_20;
	int ret = 0, i;
	acs_rsi_t *rsi = &c_info->rs_info;

	/* if given a chanspec but just need to pass coex check */

	need_coex_check = BAND_2G(rsi->band_type) &&
		(rsi->bw_cap == WLC_N_BW_40ALL) &&
		rsi->coex_enb;

	if (CHSPEC_IS2G(rsi->pref_chspec) && CHSPEC_IS40(rsi->pref_chspec) &&
		need_coex_check) {
		selected = acs_coex_check(c_info, rsi->pref_chspec);
		goto done;
	}

	if (rsi->bw_cap == WLC_N_BW_40ALL ||
		(rsi->bw_cap ==  WLC_N_BW_20IN2G_40IN5G &&
		BAND_5G(rsi->band_type))) {
			bw = ACS_BW_40;
	}

recheck:
	/* build the candidate chanspec list */
	ret = acs_build_candidates(c_info, bw);
	candi = c_info->candidate[bw];

	/* going through the  coex check if needed */
	if ((bw == ACS_BW_40) && need_coex_check) {
		for (i = 0; i < c_info->c_count[bw]; i++) {
			in_chspec = candi[i].chspec;
			out_chspec = acs_coex_check(c_info, in_chspec);
			if (in_chspec != out_chspec) {
				candi[i].valid = FALSE;
				candi[i].reason |= ACS_INVALID_COEX;
			}
		}
	}

	/* going through the interference check if needed */

	if (c_info->flags & ACS_FLAGS_INTF_THRES_CCA ||
		c_info->flags & ACS_FLAGS_INTF_THRES_BGN) {
		for (i = 0; i < c_info->c_count[bw]; i++)
			acs_candidate_check_intf(&candi[i], c_info);
	}

	/*
	 * For 20MHz channels, only pick from 1, 6, 11
	 * For 40MHz channels, only pick from control channel being 1, 6, 11 (BT doc)
	 * Mark all the other candidates invalid
	 */

	if (BAND_2G(rsi->band_type)) {
		uint ch, ext;
		ACSD_DEBUG("Filter chanspecs for 40/20 MHz channels, count: %d\n",
			c_info->c_count[bw]);
		for (i = 0; i < c_info->c_count[bw]; i++) {
			acs_parse_chanspec(candi[i].chspec, &ch, &ext);

			ACSD_DEBUG("channel: %d, ext: %d\n", ch, ext);

			if ((ch != ACS_CHANNEL_1) && (ch != ACS_CHANNEL_6) &&
				(ch != ACS_CHANNEL_11))  {
				candi[i].valid = FALSE;
				candi[i].reason |= ACS_INVALID_OVLP;
				continue;
			}
			ACSD_DEBUG("valid channel: %d\n", ch);
		}
	}

	/* if there is at least one valid chanspec */
	if (acs_has_valid_candidate(c_info, bw)) {
		acs_policy_t *a_pol = &c_info->acs_policy;
		acs_candidate_score(c_info, bw);
		if (a_pol->chan_selector)
			selected = a_pol->chan_selector(c_info, bw);
		else
			ACSD_ERROR("chan_selector is null for the selected policy");
		goto done;
	}

	/* if we failed to pick a 40 chanspec, we fall back to 20 */
	if (bw == ACS_BW_40) {
		ACSD_DEBUG("Failed to find a valid 40 MHz chanspec\n");
		bw = ACS_BW_20;
		goto recheck;
	}
	/* pick the default chanspec if we are here */
	else {
		selected = candi[0].chspec;
	}

done:
	ACSD_PRINT("selected channel spec: 0x%4x\n", selected);
	c_info->selected_chspec = selected;
}

void
acs_set_chspec(acs_chaninfo_t * c_info)
{
	int ret = 0;
	chanspec_t chspec = c_info->selected_chspec;

	if (chspec)
		ret = wl_iovar_setint(c_info->name, "chanspec", htod32(chspec));

	if (ret)
		ACSD_ERROR("set chanspec 0x%x failed!\n", chspec);
}

static void
acs_init_info(acs_info_t ** acs_info_p)
{
	acs_info = (acs_info_t*)acsd_malloc(sizeof(acs_info_t));

	* acs_info_p = acs_info;
}

void
acs_init_run(acs_info_t ** acs_info_p)
{
	char name[16], *next;
	acs_chaninfo_t * c_info;
	int ret = 0;

	acs_init_info(acs_info_p);

	foreach(name, nvram_safe_get("acs_ifnames"), next) {
		acs_add_map(name);
		ret = acs_start(name, &c_info);

		if (ret) {
			ACSD_ERROR("acs_start failed for ifname: %s\n", name);
			break;
		}

		if (AUTOCHANNEL(c_info)) {
			acs_select_chspec(c_info);
			acs_set_chspec(c_info);
			chanim_upd_acs_record(c_info->chanim_info,
				c_info->selected_chspec, APCS_INIT);
			c_info->acs_cs_scan_timer = ACS_DFLT_CS_SCAN_TIMER;
			c_info->acs_ci_scan_timer = ACS_DFLT_CI_SCAN_TIMER;
		}
	}
}

/* check if there is still associated scbs. reture value: TRUE if yes. */
static bool
acs_check_assoc_scb(acs_chaninfo_t * c_info)
{
	bool connected = TRUE;
	uint result = 0;
	int ret = 0;

	ret = wl_iovar_getint(c_info->name, "scb_assoced", &result);
	if (ret) {
		ACSD_ERROR("failed to get scb_assoced\n");
		return connected;
	}

	connected = dtoh32(result) ? TRUE : FALSE;
	ACSD_DEBUG("connected: %d\n",  connected);

	return connected;
}

int
acs_update_driver(acs_chaninfo_t * c_info)
{
	int ret = 0;
	bool param = TRUE;
	/* if we are already beaconing, after the acs scan and new chanspec selection,
	   we need to ask the driver to do some updates (beacon, probes, etc..).
	*/
	ret = wl_iovar_setint(c_info->name, "acs_update", htod32((uint)param));
	ACS_ERR(ret, "acs update failed\n");

	return ret;
}

int
acs_scan_timer_check(acs_chaninfo_t * c_info)
{
	uint timer = c_info->acs_cs_scan_timer;
	chanim_info_t * ch_info = c_info->chanim_info;
	int ret = 0;
	uint8 cur_idx = chanim_mark(ch_info).record_idx;
	uint8 start_idx;
	chanim_acs_record_t *start_record;
	time_t now = time(NULL);
	time_t passed = 0;

	if (!timer)
		return ret;

	ACSD_DEBUG(" timer: %d\n", timer);

	start_idx = MODSUB(cur_idx, 1, CHANIM_ACS_RECORD);
	start_record = &ch_info->record[start_idx];

	passed = now - start_record->timestamp;

	if ((passed > timer) && (!acs_check_assoc_scb(c_info))) {
		/* start scan */
		ret = acs_run_cs_scan(c_info);
		ACS_ERR(ret, "cs scan failed\n");

		ret = acs_request_data(c_info);
		ACS_ERR(ret, "request data failed\n");

		acs_select_chspec(c_info);
		acs_set_chspec(c_info);
		chanim_upd_acs_record(ch_info, c_info->selected_chspec, APCS_CSTIMER);

		ret = acs_update_driver(c_info);
		ACS_ERR(ret, "update driver failed\n");
	}
	return ret;
}

int
acs_do_ci_update(uint ticks, acs_chaninfo_t * c_info)
{
	int ret = 0;

	if (ticks % c_info->acs_ci_scan_timer)
		return ret;

	ret = acs_run_ci_scan(c_info);
	ACS_ERR(ret, "ci scan failed\n");

	ret = acs_request_data(c_info);
	ACS_ERR(ret, "request data failed\n");

	acs_expire_scan_entry(c_info, (time_t)ACS_CI_SCAN_EXPIRE);

	return ret;
}

int
acs_update_status(acs_chaninfo_t * c_info)
{
	int ret = 0;
	int cur_chspec;

	ret = wl_iovar_getint(c_info->name, "chanspec", &cur_chspec);
	ACS_ERR(ret, "acs get chanspec failed\n");

	c_info->cur_chspec = (chanspec_t)dtoh32(cur_chspec);
	ACSD_INFO("%s: chanspec: 0x%x\n", c_info->name, c_info->cur_chspec);
	return ret;
}
void
acs_cleanup(acs_info_t ** acs_info_p)
{
	int i;

	for (i = 0; i < ACS_MAX_IF_NUM; i++) {
		acs_chaninfo_t* c_info = (*acs_info_p)->chan_info[i];

		ACS_FREE(c_info->scan_results);
		acs_cleanup_scan_entry(c_info);
		ACS_FREE(c_info->ch_bssinfo);
		ACS_FREE(c_info->chanim_stats);
		ACS_FREE(c_info->scan_chspec_list.chspec_list);
		ACS_FREE(c_info->candidate[ACS_BW_20]);
		ACS_FREE(c_info->candidate[ACS_BW_40]);
		ACS_FREE(c_info->chanim_info);

		ACS_FREE(c_info);
	}
	ACS_FREE(acs_info);
	*acs_info_p = NULL;
}
