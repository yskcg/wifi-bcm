/*
 * SES vendor callback functions. This file is expected to be modified by the
 * vendors if they want to change some SES behavior to suit their preferences
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_cfvendor.c 241187 2011-02-17 21:52:03Z gmo $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>
#include <assert.h>
#include <wlutils.h>
#include <shutils.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <bcmtimer.h>
#include <sys/socket.h>

#include <ses_dbg.h>
#include <ses.h>
#include <ses_cfvendor.h>
#include <ses_gpio.h>

typedef struct ses_store_tuple {
	char *name;
	char *ses_value;
	char *open_value;
} ses_store_tuple_t;

typedef struct ses_cb_data {
	int long_push_event;
	int short_push_event;
	int wds_mode_auto;
} ses_cb_data_t;

static char *brcm_get_default_value(char *name);
static int brcm_ses_wl_unit(char const *ifname);
static int brcm_ses_retrieve_event(ses_cb_data_t *cb_data);

extern void ses_random(uint8 *rand, int len);
extern void ses_restart(void);
extern bcm_timer_module_id ses_tmodule;
extern struct nvram_tuple router_defaults[];
extern char def_wl_ifname[];
extern int ses_dot11_add_wds_mac(char *ifname, struct ether_addr *ea);

static ses_store_tuple_t ses_store[] = {
	{ "wl_ssid", "", ""},	/* ssid */
	{ "wl_closed", "0", ""},	/* network type should be open */
	{ "wl_wpa_psk", "", ""}, /* WPA pre-shared key */
	{ "wl_auth", "0", ""},	/* Shared key authentication (opt 0 or req 1) */
	{ "wl_wep", "disabled", "disabled"}, /* WEP encryption (enabled|disabled) */
	{ "wl_auth_mode", "none", "none"}, /* Network auth mode (radius|none) */
	{ "wl_crypto", "tkip+aes", ""},	/* WPA encryption (tkip|aes|tkip+aes) */
	{ "wl_akm", "psk psk2", ""},	/* WPA akm list (wpa|wpa2|psk|psk2) */
	{ "wl_wds", "", ""},	/* list of wds mac addresses */
	{ "wl_wds_timeout", "", ""},	/* wds timeout */
	{ NULL, NULL }
};

#define SES_WDS_ENTRY                   99

#define SES_BUTTON_POLL_TIME		500	/* in ms */
#define SES_DEFAULT_OW_TIME		120	/* in sec */

/* long push and short push defaults can be overridden by nvram variables:
 * ses_long_push_event and ses_short_push_event resp.
 */
/* long push default to "restore ses network to open/factory default" */
#define SES_DEFAULT_LONG_PUSH_EVENT     SES_EVTI_NW_RESET
/* short push default to "gen ses if unconfigured" and "open window" */
#ifdef SES_LEGACY_MODE
#define SES_DEFAULT_SHORT_PUSH_EVENT    SES_EVTI_LEGACY_NW_GEN_SES_RWO
#else
#define SES_DEFAULT_SHORT_PUSH_EVENT    SES_EVTI_NW_GEN_SES_RWO
#endif /* SES_LEGACY_MODE */

#define SES_LOG_FILE		"/tmp/ses.log"

#define SES_NV_EVENT		"ses_event"
#define SES_NV_INTERFACE	"ses_interface"
#define SES_NV_OW_TIME		"ses_ow_time"
#define SES_NV_MODE_3WAY	"ses_mode_3way"
#define SES_NV_LONG_PUSH_EVENT	"ses_long_push_event"
#define SES_NV_SHORT_PUSH_EVENT	"ses_short_push_event"
#define SES_NV_STATUS		"ses_status"
#define SES_NV_DEBUG_LEVEL	"ses_debug_level"
#define SES_NV_WDS_MODE		"ses_wds_mode"

static int
brcm_ses_event_get(ses_fsm_ctx_t ctx, bool const *noblock)
{
	struct timeval time;
	int event = SES_EVTI_NULL;
	ses_cb_data_t *cb_data;
	void *client_data; /* To avoid type-punning */

	fsm_get_client_data(ctx, &client_data);

	cb_data = (ses_cb_data_t *)client_data;

	if (*noblock) {
		event = brcm_ses_retrieve_event(cb_data);
		SES_CB("returning event(*noblock) %d\n", event);
		return (event);
	} else {
		SES_CB("enter (block)\n");
		while (1) {
			event = brcm_ses_retrieve_event(cb_data);
			if ((event != SES_EVTI_NULL) || (*noblock)) {
				SES_CB("returning event(block) %d\n", event);
				return (event);
			}

			time.tv_sec = SES_BUTTON_POLL_TIME / 1000;
			time.tv_usec = (SES_BUTTON_POLL_TIME % 1000) * 1000;

			/* enable timer handling */
			bcm_timer_module_enable(ses_tmodule, 1);

			select(0, NULL, NULL, NULL, &time);

			/* disable timer handling */
			bcm_timer_module_enable(ses_tmodule, 0);
		}
	}
}

static void
brcm_ses_event_notify(ses_fsm_ctx_t ctx, char const *ifname, ses_event_t *event)
{
	FILE *fp;
	char event_str[64], status_str[32];
	char eabuf[ETHER_ADDR_STR_LEN];
	ses_cb_data_t *cb_data;
	void *client_data; /* To avoid type-punning */

	fsm_get_client_data(ctx, &client_data);

	cb_data = (ses_cb_data_t *)client_data;

	fp = fopen(SES_LOG_FILE, "a");

	switch (event->evto) {
	case SES_EVTO_UNCONFIGURED:
		snprintf(event_str, sizeof(event_str), "SES_EVTO_UNCONFIGURED");
		ses_led_off();
		break;
	case SES_EVTO_CONFIGURED:
		snprintf(event_str, sizeof(event_str), "SES_EVTO_CONFIGURED");
		ses_led_on();
		break;
	case SES_EVTO_RWO:
		snprintf(event_str, sizeof(event_str), "SES_EVTO_RWO");
		ses_led_blink(SES_BLINKTYPE_RWO);

		/* if wds auto mode, kick wds ses client if unconfigured */
		if (cb_data->wds_mode_auto) {
			if (event->u.configured == FALSE)
				nvram_set("ses_cl_event", "1"); /* 1 ==> SES_OP_GO */
		}

		break;
	case SES_EVTO_RWO_CONFIRM:
		snprintf(event_str, sizeof(event_str), "SES_EVTO_RWO_CONFIRM");
		break;
	case SES_EVTO_EXCH_INITIATED:
		snprintf(event_str, sizeof(event_str), "SES_EVTO_EXCH_INITIATED(%s)",
			ether_etoa((unsigned char *)&event->u.rem_ea, eabuf));

		/* if wds auto mode, stop wds ses client since a regular client found us */
		if (cb_data->wds_mode_auto) {
			nvram_set("ses_cl_event", "2"); /* 2 ==> SES_OP_CANCEL */
		}

		break;
	case SES_EVTO_RWC:
		snprintf(event_str, sizeof(event_str), "SES_EVTO_RWC(%s)",
			ether_etoa((unsigned char *)&event->u.rem_ea, eabuf));
		ses_led_on();
		break;
	default:
		assert(0);
	}

	switch (event->status) {
	case SES_STATUS_SUCCESS:
		nvram_set(SES_NV_STATUS, "Success");
		snprintf(status_str, sizeof(status_str), "SUCCESS");
		break;
	case SES_STATUS_FAILURE:
		nvram_set(SES_NV_STATUS, "Failure");
		snprintf(status_str, sizeof(status_str), "FAILURE");
		break;
	case SES_STATUS_UNKNOWN:
	default:
		nvram_set(SES_NV_STATUS, "Unknown");
		snprintf(status_str, sizeof(status_str), "UNKNOWN");
		break;
	}

	if (fp) {
		fprintf(fp, "%s: Status of last input event %s\n",
			event_str, status_str);
		fclose(fp);
	}

	SES_ERROR("%s: Status of last input event %s\n",
		event_str, status_str);
}

int
brcm_ses_network_store(ses_fsm_ctx_t ctx, char const *ifname)
{
#ifdef SES_LEGACY_MODE
	ses_store_tuple_t *t;
	char prefix[NVRAM_BUFSIZE], s_prefix[NVRAM_BUFSIZE], tmp[NVRAM_BUFSIZE];
	char *value;
	int unit = brcm_ses_wl_unit(ifname);

	SES_CB("enter\n");

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	snprintf(s_prefix, sizeof(s_prefix), "wl%d_ses_", unit);
	for (t = ses_store; t->name; t++) {
		value = nvram_safe_get(strcat_r(prefix, &t->name[3], tmp));
		nvram_set(strcat_r(s_prefix, &t->name[3], tmp), value);
	}
#endif /* SES_LEGACY_MODE */
	return 0;
}

int
brcm_ses_network_restore(ses_fsm_ctx_t ctx, char const *ifname)
{
#ifdef SES_LEGACY_MODE
	ses_store_tuple_t *t;
	char prefix[NVRAM_BUFSIZE], s_prefix[NVRAM_BUFSIZE], tmp[NVRAM_BUFSIZE];
	char *value;
	int unit = brcm_ses_wl_unit(ifname);

	SES_CB("enter\n");

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	snprintf(s_prefix, sizeof(s_prefix), "wl%d_ses_", unit);
	for (t = ses_store; t->name; t++) {
		value = nvram_safe_get(strcat_r(s_prefix, &t->name[3], tmp));
		nvram_set(strcat_r(prefix, &t->name[3], tmp), value);
	}

	/* request reinit(i.e rc restart) */
	fsm_op_req_reinit(ctx);
#else
	assert(0);
#endif /* SES_LEGACY_MODE */

	return 0;
}

int
brcm_ses_network_recall(ses_fsm_ctx_t ctx, char const *ifname, ses_network_t *network)
{
#ifdef SES_LEGACY_MODE
	char name[NVRAM_BUFSIZE];
	int unit = brcm_ses_wl_unit(ifname);

	snprintf(name, sizeof(name), "wl%d_ses_ssid", unit);
	strcpy(network->ssid, nvram_safe_get(name));

	snprintf(name, sizeof(name), "wl%d_ses_closed", unit);
	network->open = atoi(nvram_safe_get(name)) ? 0 : 1;

	snprintf(name, sizeof(name), "wl%d_ses_wpa_psk", unit);
	strcpy(network->passphrase, nvram_safe_get(name));

	SES_CB("returning ssid %s passphrase %s\n",
		network->ssid, network->passphrase);
#else
	assert(0);
#endif /* SES_LEGACY_MODE */

	return 0;
}

int
brcm_ses_network_get(ses_fsm_ctx_t ctx, char const *ifname, ses_network_t *network)
{
	char name[NVRAM_BUFSIZE], *next, *value;
	int unit = brcm_ses_wl_unit(ifname);

	snprintf(name, sizeof(name), "wl%d_ssid", unit);
	strcpy(network->ssid, nvram_safe_get(name));

	snprintf(name, sizeof(name), "wl%d_closed", unit);
	network->open = atoi(nvram_safe_get(name)) ? 0 : 1;

	snprintf(name, sizeof(name), "wl%d_wpa_psk", unit);
	strcpy(network->passphrase, nvram_safe_get(name));

	assert(strlen(network->passphrase) <= SES_MAX_PASSPHRASE_LEN);

	network->security = 0;
	snprintf(name, sizeof(name), "wl%d_akm", unit);
	value = nvram_safe_get(name);
	foreach(name, value, next) {
		if (!strcmp(name, "psk"))
			network->security |= SES_SECURITY_WPA_PSK;
		if (!strcmp(name, "psk2"))
			network->security |= SES_SECURITY_WPA2_PSK;
	}

	network->encryption = 0;
	snprintf(name, sizeof(name), "wl%d_crypto", unit);
	value = nvram_safe_get(name);
	if (!strcmp(value, "tkip"))
		network->encryption = SES_ENCRYPTION_TKIP;
	else if (!strcmp(value, "aes"))
		network->encryption = SES_ENCRYPTION_AES;
	else if (!strcmp(value, "tkip+aes"))
		network->encryption = SES_ENCRYPTION_TKIP_AES;

	SES_CB("returning ssid %s passphrase %s\n security 0x%x encryption 0x%x",
		network->ssid, network->passphrase, network->security, network->encryption);

	return 0;
}

int
brcm_ses_network_set(ses_fsm_ctx_t ctx, char const *ifname, ses_network_t const *network)
{
	ses_store_tuple_t *t;
	char prefix[NVRAM_BUFSIZE], tmp[NVRAM_BUFSIZE], name[NVRAM_BUFSIZE];
	int unit = brcm_ses_wl_unit(ifname);

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	if (strcmp(network->passphrase, "")) {
		SES_CB("setting ses network for ssid %s\n", network->ssid);
		for (t = ses_store; t->name; t++) {
			nvram_set(strcat_r(prefix, &t->name[3], tmp), t->ses_value);
		}
	}
#ifdef SES_LEGACY_MODE
	else {
		SES_CB("setting open network for ssid %s\n", network->ssid);
		for (t = ses_store; t->name; t++) {
			nvram_set(strcat_r(prefix, &t->name[3], tmp), t->open_value);
		}
	}
#else
	else
		assert(0);
#endif /* SES_LEGACY_MODE */

	snprintf(name, sizeof(name), "wl%d_ssid", unit);
	nvram_set(name, network->ssid);

	snprintf(name, sizeof(name), "wl%d_closed", unit);
	nvram_set(name, network->open ? "0" : "1");

	snprintf(name, sizeof(name), "wl%d_wpa_psk", unit);
	nvram_set(name, network->passphrase);

	/* set wildcard wds security settings */
	snprintf(name, sizeof(name), "wl%d_wds%d", unit, SES_WDS_ENTRY);
	snprintf(tmp, sizeof(tmp), "*,auto,tkip,psk,%s,%s",
		network->ssid, network->passphrase);
	nvram_set(name, tmp);

	/* request reinit(i.e rc restart) */
	fsm_op_req_reinit(ctx);

	return 0;
}

int
brcm_ses_network_reset(ses_fsm_ctx_t ctx, char const *ifname)
{
	ses_store_tuple_t *t;
	char prefix[NVRAM_BUFSIZE], tmp[NVRAM_BUFSIZE];
	char *value;
	int unit = brcm_ses_wl_unit(ifname);
	int i;

	SES_CB("enter\n");

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	for (t = ses_store; t->name; t++) {
		value = brcm_get_default_value(t->name);
		assert(value);
		if (!strncmp(t->name, "wl_", 3))
			nvram_set(strcat_r(prefix, &t->name[3], tmp), value);
		else
			nvram_set(t->name, value);
	}

	/* clear all wds security settings */
	for (i = 0; i <= SES_WDS_ENTRY; i ++) {
		snprintf(tmp, sizeof(tmp), "%swds%d", prefix, i);
		nvram_unset(tmp);
	}

	nvram_set("lan_dhcp", brcm_get_default_value("lan_dhcp"));

	/* don't disturb the current "router_disable" setting if we have
	 * a wireless interface using URE
	 */
	if (!ure_any_enabled())
		nvram_set("router_disable", brcm_get_default_value("router_disable"));

	/* request reinit(i.e rc restart) */
	fsm_op_req_reinit(ctx);

	return 0;
}

bool
brcm_ses_network_is_ses(ses_fsm_ctx_t ctx, char const *ifname)
{
	ses_store_tuple_t *t;
	char prefix[NVRAM_BUFSIZE], tmp[NVRAM_BUFSIZE];
	char *value;
	int unit = brcm_ses_wl_unit(ifname);
	int i;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	for (t = ses_store; t->name; t++) {
		if (strcmp(t->ses_value, "")) {
			value = nvram_safe_get(strcat_r(prefix, &t->name[3], tmp));

			/* remove trailing spaces */
			strcpy(tmp, value);
			for (i = strlen(tmp)-1; (i != 0) && (tmp[i] == ' '); i--);
			tmp[i+1] = '\0';

			if (strcmp(tmp, t->ses_value)) {
				SES_CB("returning FALSE\n");
				return FALSE;
			}
		}
	}

	SES_CB("returning TRUE\n");
	return TRUE;
}

ses_network_t *
brcm_ses_network_gen_ses(ses_fsm_ctx_t ctx, char const *ifname,
                         ses_network_t *network, int force)
{
	char *value;
	char name[NVRAM_BUFSIZE];
	uint16 rand;
	int unit = brcm_ses_wl_unit(ifname);

	if (!force) {
		/* if network is currently wpa secure in some form (psk or psk2)
		 * then keep the same ssid/psk
		 */
		snprintf(name, sizeof(name), "wl%d_akm", unit);
		if (strstr(nvram_safe_get(name), "psk")) {
			snprintf(name, sizeof(name), "wl%d_ssid", unit);
			strcpy(network->ssid, nvram_safe_get(name));
			SES_CB("returning previous ssid %s\n", network->ssid);

			snprintf(name, sizeof(name), "wl%d_wpa_psk", unit);
			strcpy(network->passphrase, nvram_safe_get(name));
			network->open = TRUE;

			return network;
		}
	}

	value = brcm_get_default_value("wl_ssid");
	assert(value);
	ses_random((uint8 *)&rand, 2);
	sprintf(network->ssid, "%s_SES_%d", value, rand);
	SES_CB("returning ssid %s\n", network->ssid);

	strcpy(network->passphrase, "");
	network->open = TRUE;

	return network;
}

ses_network_t *
brcm_ses_network_gen_rnd(ses_fsm_ctx_t ctx, ses_network_t *network)
{
#ifdef SES_LEGACY_MODE
	char *value;
	uint16 rand;

	value = brcm_get_default_value("wl_ssid");
	assert(value);
	ses_random((uint8 *)&rand, 2);
	sprintf(network->ssid, "%s_OW_%d", value, rand);
	SES_CB("returning ssid %s\n", network->ssid);

	strcpy(network->passphrase, "");
	network->open = TRUE;

	return network;
#else
	assert(0);
	return NULL;
#endif /* SES_LEGACY_MODE */
}

int
brcm_ses_network_set_bridge(ses_fsm_ctx_t ctx, bool enabled)
{
#ifdef SES_LEGACY_MODE
	SES_CB("enabled=%d\n", enabled);

	if (enabled)
		nvram_set("ses_bridge_disable", "0");
	else
		nvram_set("ses_bridge_disable", "1");

	/* request reinit(i.e rc restart) */
	fsm_op_req_reinit(ctx);
#else
	assert(0);
#endif /* SES_LEGACY_MODE */
	return 0;
}

int
brcm_ses_wds_add(ses_fsm_ctx_t ctx, char const *ifname, char const *ssid,
	char const *passphrase, struct ether_addr *ea)
{
	char name[NVRAM_BUFSIZE], value[NVRAM_BUFSIZE], *tmp;
	char eabuf[ETHER_ADDR_STR_LEN];
	int unit = brcm_ses_wl_unit(ifname);

	ses_dot11_add_wds_mac((char *)ifname, ea);

	snprintf(name, sizeof(name), "wl%d_wds", unit);
	tmp = nvram_safe_get(name);
	if (!strstr(tmp, ether_etoa((unsigned char *)(ea), eabuf))) {
		snprintf(value, sizeof(value), "%s %s", tmp, eabuf);
		nvram_set(name, value);
	}

	/* set wildcard wds security settings (in case it has changed) */
	snprintf(name, sizeof(name), "wl%d_wds%d", unit, SES_WDS_ENTRY);
	snprintf(value, sizeof(value), "*,auto,tkip,psk,%s,%s",
		ssid, passphrase);
	nvram_set(name, value);

	nvram_commit();

	return 0;
}

int
brcm_ses_stg_set(ses_fsm_ctx_t ctx, char const *name, char const *buf, int len)
{
	SES_CB("name %s value %s\n", name, buf);
	nvram_set(name, buf);
	nvram_commit();
	return 0;
}

int
brcm_ses_stg_get(ses_fsm_ctx_t ctx, char const *name, char *buf, int len)
{
	char *value;

	value = nvram_get(name);
	if (value == NULL) {
		SES_CB("name %s does not exist: returning 1\n", name);
		return 1;
	}
	SES_CB("name %s value %s\n", name, value);
	strncpy(buf, value, len);
	return 0;
}


void
brcm_ses_sys_reinit(ses_fsm_ctx_t ctx)
{
	struct timeval time;

	SES_CB("enter\n");

	ses_gpio_btn_cleanup();
	ses_gpio_led_cleanup();
	nvram_commit();
	ses_restart();

	/* sleep for long enough so that all processes are killed */
	time.tv_sec = 10;
	time.tv_usec = 0;
	select(0, NULL, NULL, NULL, &time);

	/* should not come here */
	assert(0);
}


int
ses_main(int argc, char *argv[])
{
	ses_fsm_cb_t cb;
	ses_fsm_setup_t setup;
	int unit;
	char name[NVRAM_BUFSIZE], *next, *value;
	char tmp[NVRAM_BUFSIZE];
	char *lan_ifnames, *lan_ifname;
	ses_cb_data_t *cb_data;
	ses_fsm_cb_t *pcb = malloc(sizeof(ses_fsm_cb_t));
	ses_fsm_setup_t *psetup = malloc(sizeof(ses_fsm_setup_t));

#ifdef BCMDBG
	value = nvram_safe_get(SES_NV_DEBUG_LEVEL);
	if (strcmp(value, "")) {
		ses_debug_level = strtol(value, NULL, 0);
	}
#endif /* BCMDBG */

	cb_data = malloc(sizeof(ses_cb_data_t));
	assert(cb_data);
	memset(cb_data, 0, sizeof(ses_cb_data_t));

	memset(&setup, 0, sizeof(ses_fsm_setup_t));
	memset(&cb, 0, sizeof(ses_fsm_cb_t));

	value = nvram_safe_get(SES_NV_SHORT_PUSH_EVENT);
	if (!strcmp(value, "")) {
		cb_data->short_push_event = SES_DEFAULT_SHORT_PUSH_EVENT;
	} else {
		cb_data->short_push_event = atoi(value);
	}

	value = nvram_safe_get(SES_NV_LONG_PUSH_EVENT);
	if (!strcmp(value, "")) {
		cb_data->long_push_event = SES_DEFAULT_LONG_PUSH_EVENT;
	} else {
		cb_data->long_push_event = atoi(value);
	}

	value = nvram_safe_get(SES_NV_MODE_3WAY);
	if (!strcmp(value, "1")) {
		setup.mode = SES_SETUP_MODE_3WAY;
	} else {
		setup.mode = SES_SETUP_MODE_2WAY;
	}

	value = nvram_safe_get(SES_NV_OW_TIME);
	if (!strcmp(value, "")) {
		setup.ow_time = SES_DEFAULT_OW_TIME;
	} else {
		setup.ow_time = atoi(value);
	}

	value = nvram_safe_get(SES_NV_WDS_MODE);
	if (!strcmp(value, "")) {
		setup.wds_mode = SES_WDS_MODE_AUTO;
	} else {
		setup.wds_mode = atoi(value);
	}

	if (setup.wds_mode == SES_WDS_MODE_CLIENT) {
		SES_ERROR("exiting: no user of ses configurator(system in wds client mode)\n");
		return -1;
	} else if (setup.wds_mode == SES_WDS_MODE_AUTO) {
		cb_data->wds_mode_auto = TRUE;
	}

#ifdef SES_LEGACY_MODE
	/* pe to listen on all individual interfaces */
	lan_ifname = nvram_safe_get("lan_ifnames");
#else
	/* pe to listen on the bridge */
	lan_ifname = nvram_safe_get("lan_ifname");
#endif /* SES_LEGACY_MODE */

	strcpy(setup.ifnames, lan_ifname);

	value = nvram_safe_get(SES_NV_INTERFACE);
	if (!strcmp(value, "")) {
		/* probe for first wl interface */
		lan_ifnames = nvram_safe_get("lan_ifnames");
		foreach(name, lan_ifnames, next) {
			if (wl_probe(name) == 0)
				break;
		}
		value = name;
	}

	if ((wl_probe(value) != 0) ||
	    (wl_ioctl(value, WLC_GET_INSTANCE, &unit, sizeof(unit)) != 0)) {
		SES_CB("ERROR: Invalid default wireless interface %s\n",
			value);
		return -1;
	}

	snprintf(tmp, sizeof(tmp), "wl%d_mode", unit);
	if (nvram_match(tmp, "wet")) {
		SES_ERROR("exiting: no user of ses configurator(system in wet mode)\n");
		return -1;
	}

	if (ses_gpio_btn_init() != 0) {
		SES_ERROR("ses_gpio_btn_init() failure\n");
		return -1;
	}

	SES_CB("Initiating ses on wireless unit %d\n", unit);
	strcpy(setup.def_wlname, value);
	strcpy(def_wl_ifname, value);

	cb.event_get = brcm_ses_event_get;
	cb.event_notify = brcm_ses_event_notify;
	cb.network_store = brcm_ses_network_store;
	cb.network_restore = brcm_ses_network_restore;
	cb.network_recall = brcm_ses_network_recall;
	cb.network_reset = brcm_ses_network_reset;
	cb.wds_add = brcm_ses_wds_add;
	cb.network_set = brcm_ses_network_set;
	cb.network_get = brcm_ses_network_get;
	cb.network_set_bridge = brcm_ses_network_set_bridge;
	cb.network_gen_ses = brcm_ses_network_gen_ses;
	cb.network_gen_rnd = brcm_ses_network_gen_rnd;
	cb.network_is_ses = brcm_ses_network_is_ses;
	cb.stg_set = brcm_ses_stg_set;
	cb.stg_get = brcm_ses_stg_get;
	cb.sys_reinit = brcm_ses_sys_reinit;

#ifdef __CONFIG_SES_POLL__
	setup.poll = TRUE;
#endif /* __CONFIG_SES_POLL__ */

	bcopy(&cb, pcb, sizeof(ses_fsm_cb_t));
	bcopy(&setup, psetup, sizeof(ses_fsm_setup_t));

	init_fsm(pcb, psetup, cb_data);

#ifndef __CONFIG_SES_POLL__
	SES_CB("SHOULD NOT COME HERE!!!!!");

	return 0;
#else
	return 1;
#endif /* __CONFIG_SES_POLL__ */
}

static char *
brcm_get_default_value(char *name)
{
	struct nvram_tuple *t;

#ifdef SES_DEFAULTS_STUB
	struct nvram_tuple router_defaults[] = {};
#endif /* SES_DEFAULTS_STUB */

	for (t = router_defaults; t->name; t++) {
		if (!strcmp(name, t->name))
			return t->value;
	}

	return NULL;
}

static int
brcm_ses_wl_unit(char const *ifname)
{
	int unit, ret;

	ret = wl_probe((char *)ifname);
	if (ret != 0) {
		SES_CB("ERROR: Invalid wireless interface %s", ifname);
		assert(!ret);
	}

	ret = wl_ioctl((char *)ifname, WLC_GET_INSTANCE, &unit, sizeof(unit));
	if (ret != 0) {
		SES_CB("ERROR: Invalid wireless interface %s", ifname);
		assert(!ret);
	}
	return unit;
}

static int
brcm_ses_retrieve_event(ses_cb_data_t *cb_data)
{
	char *value;
	char buf[4];
	int event = SES_EVTI_NULL;

	switch (ses_btn_pressed()) {
		case SES_NO_BTNPRESS:
			value = nvram_safe_get(SES_NV_EVENT);
			if ((event = atoi(value)) != SES_EVTI_NULL) {
				sprintf(buf, "%d", SES_EVTI_NULL);
				nvram_set(SES_NV_EVENT, buf);
			}
			break;
		case SES_SHORT_BTNPRESS:
			event = cb_data->short_push_event;
			break;
		case SES_LONG_BTNPRESS:
			/* Directly using printf so get indication in external builds */
			printf("Detected a long ses button-push\n");
			event = cb_data->long_push_event;
			break;
		default:
			assert(0);
	}

	return event;
}
