/*
 * SES client vendor callback functions. This file is expected to be modified by
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
 * $Id: ses_clvendor.c 241187 2011-02-17 21:52:03Z gmo $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <signal.h>
#include <assert.h>
#include <wlutils.h>
#include <shutils.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <bcmtimer.h>

#include <ses_dbg.h>
#include <ses.h>
#include <ses_gpio.h>
#include <ses_cl.h>
#include <ses_net.h>

typedef struct ses_store_tuple {
	char *name;
	char *ses_value;
} ses_store_tuple_t;

typedef struct ses_cb_data {
	int long_push_event;
	int short_push_event;
	int release;
	int unit;
	int wet_enable;
	int wds_enable;
	int wds_mode_auto;
	int control_gpio;
} ses_cb_data_t;

static ses_store_tuple_t ses_store[] = {
	{ "wl_ssid", ""},	/* ssid */
	{ "wl_wpa_psk", ""},	/* WPA pre-shared key */
	{ "wl_auth", "0"},	/* Shared key authentication (opt 0 or req 1) */
	{ "wl_wep", "disabled"}, /* WEP encryption (enabled|disabled) */
	{ "wl_auth_mode", "none"}, /* Network auth mode (radius|none) */
	{ "wl_crypto", "tkip+aes"}, /* WPA encryption (tkip|aes|tkip+aes) */
	{ "wl_akm", "psk psk2"}, /* WPA akm list (wpa|wpa2|psk|psk2) */
#ifdef SES_WDS_CLIENT
	{ "wl_wds", ""},	/* list of wds mac addresses */
	{ "wl_wds_timeout", ""}, /* wds timeout */
	{ "wl_channel", ""},	/* channel setting */
#endif /* SES_WDS_CLIENT */
	{ NULL}
};

static char *brcm_get_default_value(char *name);
static int brcm_ses_retrieve_event(ses_cb_data_t *cb_data);
static bool ses_cl_network_is_ses(int unit, int wet);
static int ses_cl_ses_set(int unit, char *ssid, char *passphrase);
static int ses_cl_ses_reset(int unit);
static void brcm_ses_sys_reinit();
static int brcm_ses_wds_add(int unit, ses_data_t *data);
static int brcm_ses_wet_add(int unit, ses_data_t *data);

extern void ses_restart(void);
extern bcm_timer_module_id ses_tmodule;
extern struct nvram_tuple router_defaults[];
extern char def_wl_ifname[];

extern int ses_wds_dot11_join(char const *ifname, ses_join_info_t *join_info);
extern int ses_wds_dot11_scan(char const *ifname, int *wait_time);
extern int ses_wds_dot11_scan_results(char const *ifname, wl_scan_results_t **results);
extern int ses_wds_dot11_disassoc(char const *ifname);
extern int ses_wds_get_media_state(char const *ifname, int *state);
extern int ses_wds_get_adapter_type(char const *ifname, int *type);
extern char *ses_wds_get_wlname(char const *ifname, char *wlname);


#define SES_WDS_ENTRY			99

#define SES_CL_BUTTON_POLL_TIME		500	/* in ms */
#define SES_CL_DEFAULT_OW_TIME		60	/* sec */

/* long push and short push defaults can be overridden by nvram variables:
 * ses_cl_long_push_event and ses_cl_short_push_event resp.
 */
/* long push defaults to "resetting wpa/psk settings" */
#define SES_CL_DEFAULT_LONG_PUSH_EVENT	SES_OP_RESET
/* short push default to "start discovery and exchange" */
#define SES_CL_DEFAULT_SHORT_PUSH_EVENT	SES_OP_GO

#define SES_CL_LOG_FILE			"/tmp/ses_cl.log"

#define SES_CL_NV_EVENT			"ses_cl_event"
#define SES_CL_NV_OW_TIME		"ses_cl_ow_time"
#define SES_CL_NV_LONG_PUSH_EVENT	"ses_cl_long_push_event"
#define SES_CL_NV_SHORT_PUSH_EVENT	"ses_cl_short_push_event"
#define SES_CL_NV_STATUS		"ses_cl_status"

/* Following nvram variables are shared by the client and configurator */
#define SES_NV_DEBUG_LEVEL		"ses_debug_level"
#define SES_NV_WDS_MODE         	"ses_wds_mode"
#define SES_NV_INTERFACE		"ses_interface"

int
brcm_ses_cl_event_get(ses_context_t ctx)
{
	struct timeval time;
	int event = SES_OP_NULL;
	ses_cb_data_t *cb_data;

	if (ses_client_data(ctx, (void **)&cb_data) != SES_RESULT_SUCCESS) {
		SES_CB("unable to retrieve cb_data\n");
		assert(0);
	}

	SES_CB("enter\n");
	while (1) {
		event = brcm_ses_retrieve_event(cb_data);
		if ((event != SES_OP_NULL) || (cb_data->release)) {
			cb_data->release = 0;
			SES_CB("returning event %d\n", event);
			/* handle reset here since the SM does not handle it */
			if (event == SES_OP_RESET) {
				ses_cl_ses_reset(cb_data->unit);
				nvram_set(SES_CL_NV_STATUS, "Success");
				brcm_ses_sys_reinit();
			}
			return (event);
		}

		time.tv_sec = SES_CL_BUTTON_POLL_TIME / 1000;
		time.tv_usec = (SES_CL_BUTTON_POLL_TIME % 1000) * 1000;

		/* enable timer handling */
		bcm_timer_module_enable(ses_tmodule, 1);

		select(0, NULL, NULL, NULL, &time);

		/* disable timer handling */
		bcm_timer_module_enable(ses_tmodule, 0);
	}
}


void
brcm_ses_cl_event_notify(ses_context_t ctx, ses_event_t const *event)
{
	FILE *fp;
	ses_cb_data_t *cb_data;
	char str[128], value[NVRAM_BUFSIZE];
	struct timeval time;

	if (ses_client_data(ctx, (void **)&cb_data) != SES_RESULT_SUCCESS) {
		SES_CB("unable to retrieve cb_data\n");
		assert(0);
	}

	fp = fopen(SES_CL_LOG_FILE, "a");

	switch (event->type) {
	case SES_EVENT_IDLE:
		if (ses_cl_network_is_ses(cb_data->unit, cb_data->wet_enable)) {
			snprintf(str, sizeof(str), "SES_EVENT_IDLE: network is SES\n");
			if (cb_data->control_gpio)
				ses_led_on();
		} else {
			snprintf(str, sizeof(str), "SES_EVENT_IDLE: network is not SES\n");
			if (cb_data->control_gpio)
				ses_led_off();
		}
		break;

	case SES_EVENT_DISCOVERING:
		snprintf(str, sizeof(str), "SES_EVENT_DISCOVERING(for %d sec)\n",
			event->discovering_event.ow_timeout);
		/* assume success else halt event tells us about failure */
		nvram_set(SES_CL_NV_STATUS, "Success");

		if (cb_data->control_gpio)
			ses_led_blink(SES_BLINKTYPE_RWO);
		break;

	case SES_EVENT_DISCOVERED:
		/* remove wl iface from bridge so that we can retrieve packets
		 * directly from the wl interface
		 */
		if (cb_data->wet_enable)
			eval("brctl", "delif", nvram_safe_get("lan_ifname"),
				def_wl_ifname);

		/* if wds auto mode, stop ses configurator since we found one */
		if (cb_data->wds_mode_auto) {
			nvram_set("ses_event", "10"); /* 10 ==> SES_EVTI_CANCEL */
			/* wait for configurator to cleanup (like sockets) before
			 * proceeding.
			 */
			time.tv_sec = 1;
			time.tv_usec = 0;
			select(0, NULL, NULL, NULL, &time);
		}

		snprintf(str, sizeof(str), "SES_EVENT_DISCOVERED: ifname %s ssid %s\n",
			event->discovered_event.ifname, event->discovered_event.ssid);
		break;

	case SES_EVENT_CONFIGURATOR_CONFIRM:
		snprintf(str, sizeof(str), "SES_EVENT_CONFIGURATOR_CONFIRM\n");
		break;

	case SES_EVENT_ASSOCIATION:
		switch (event->assoc_event.state) {
		case SES_ASSOC_STATE_UNASSOCIATED:
			snprintf(str, sizeof(str), "SES_EVENT_ASSOCIATION: "
				"ses_assoc_state_unassociated\n");
			break;
		case SES_ASSOC_STATE_ASSOCIATING:
			snprintf(str, sizeof(str), "SES_EVENT_ASSOCIATION: "
				"ses_assoc_state_associating\n");
			break;
		case SES_ASSOC_STATE_ASSOCIATED:
			snprintf(str, sizeof(str), "SES_EVENT_ASSOCIATION: "
				"ses_assoc_state_associated\n");
			break;
		default:
			assert(0);
		}
		break;

	case SES_EVENT_CONFIGURATION:
		switch (event->configuration_event.state) {
		case SES_CONFIG_STATE_INIT:
			snprintf(str, sizeof(str), "SES_EVENT_CONFIGURATION: "
				"ses_config_state_init\n");
			break;
		case SES_CONFIG_STATE_WAIT:
			snprintf(str, sizeof(str), "SES_EVENT_CONFIGURATION: "
				"ses_config_state_wait\n");
			break;
		case SES_CONFIG_STATE_RCVD:
			snprintf(str, sizeof(str), "SES_EVENT_CONFIGURATION: "
				"ses_config_state_rcvd\n");
			break;
		default:
			assert(0);
		}
		break;

	case SES_EVENT_HALT:
		snprintf(str, sizeof(str), "SES_EVENT_HALT: Error status %d\n",
			event->halt_event.status);
		snprintf(value, sizeof(value), "ERROR: %d", event->halt_event.status);
		nvram_set(SES_CL_NV_STATUS, value);
		break;

	default:
		assert(0);
	}

	if (fp) {
		fprintf(fp, str);
		fclose(fp);
	}

	SES_ERROR("%s", str);
}

void
brcm_ses_cl_event_release(ses_context_t ctx)
{
	ses_cb_data_t *cb_data;

	SES_CB("enter\n");
	if (ses_client_data(ctx, (void **)&cb_data) != SES_RESULT_SUCCESS) {
		SES_CB("unable to retrieve cb_data\n");
		assert(0);
	}

	cb_data->release = 1;
}

static void
brcm_ses_sys_reinit()
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
ses_clmain(int argc, char *argv[])
{
	ses_cb_t cb;
	ses_context_t ctx;
	ses_data_t data;
	ses_settings_t settings;
	int unit;
	char tmp[NVRAM_BUFSIZE];
	char name[NVRAM_BUFSIZE];
	char *value, *list, *next;
	char *lan_ifnames;
	ses_cb_data_t *cb_data;
	int result;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

#ifdef BCMDBG
	value = nvram_safe_get(SES_NV_DEBUG_LEVEL);
	if (strcmp(value, "")) {
		ses_debug_level = strtol(value, NULL, 0);
	}
#endif /* BCMDBG */

	cb_data = malloc(sizeof(ses_cb_data_t));
	assert(cb_data);
	memset(cb_data, 0, sizeof(ses_cb_data_t));

	cb_data->control_gpio = TRUE;

	value = nvram_safe_get(SES_CL_NV_SHORT_PUSH_EVENT);
	if (!strcmp(value, "")) {
		cb_data->short_push_event = SES_CL_DEFAULT_SHORT_PUSH_EVENT;
	} else {
		cb_data->short_push_event = atoi(value);
	}

	value = nvram_safe_get(SES_CL_NV_LONG_PUSH_EVENT);
	if (!strcmp(value, "")) {
		cb_data->long_push_event = SES_CL_DEFAULT_LONG_PUSH_EVENT;
	} else {
		cb_data->long_push_event = atoi(value);
	}

	cb_data->release = 0;

	value = nvram_safe_get(SES_CL_NV_OW_TIME);
	if (!strcmp(value, "")) {
		settings.ow_timeout = SES_CL_DEFAULT_OW_TIME;
		settings.standby_timeout = SES_CL_DEFAULT_OW_TIME;
	} else {
		settings.ow_timeout = atoi(value);
		settings.standby_timeout = atoi(value);
	}

	value = nvram_safe_get(SES_NV_INTERFACE);
	if (!strcmp(value, "")) {
		/* probe for first wl interface */
		lan_ifnames = nvram_safe_get("lan_ifnames");
		foreach(name, lan_ifnames, next) {
			if (wl_probe(name) == 0)
				break;
		}
	} else {
		strcpy(name, value);
	}

	if ((wl_probe(name) != 0) ||
	    (wl_ioctl(name, WLC_GET_INSTANCE, &unit, sizeof(unit)) != 0)) {
		SES_CB("ERROR: Invalid default wireless interface %s\n", name);
		return -1;
	}

	cb_data->unit = unit;

	strcpy(def_wl_ifname, name);
	settings.ifnames = (char const **)&list;
	list = name;
	settings.ifnames_count = 1;

	settings.flags = 0;

	snprintf(tmp, sizeof(tmp), "wl%d_mode", unit);
	if (nvram_match(tmp, "wet"))
		cb_data->wet_enable = TRUE;
	else {
#ifdef SES_WDS_CLIENT
		char *lan_ifname;

		/* Use bridge interface for WDS instead of wl interface */
		if ((lan_ifname = nvram_get("lan_ifname")) != NULL)
			strcpy(name, lan_ifname);

		value = nvram_safe_get(SES_NV_WDS_MODE);
		if (atoi(value) == SES_WDS_MODE_CLIENT) {
			cb_data->wds_enable = TRUE;
			settings.flags |= SES_FLAG_WDS_CLIENT;
		} else if (atoi(value) == SES_WDS_MODE_AUTO) {
			cb_data->wds_enable = TRUE;
			cb_data->wds_mode_auto = TRUE;
			settings.flags |= SES_FLAG_WDS_CLIENT;
			cb_data->control_gpio = FALSE;
		} else
#endif /* SES_WDS_CLIENT */
		{
			SES_ERROR("exiting: no user of ses client\n");
			return -1;
		}
	}

	SES_CB("Initiating ses %s client on wl unit %d interface name %s\n",
	       cb_data->wet_enable ? "wet" : "wds", unit, name);

	if (cb_data->control_gpio) {
		if (ses_gpio_btn_init() != 0) {
			SES_ERROR("ses_gpio_btn_init() failure\n");
			return -1;
		}
	}

	cb.event_notify = brcm_ses_cl_event_notify;
	cb.event_get = brcm_ses_cl_event_get;
	cb.event_release = brcm_ses_cl_event_release;

	if (cb_data->wet_enable) {
		cb.dot11_join = ses_dot11_join;
		cb.dot11_scan = ses_dot11_scan;
		cb.dot11_scan_results = ses_dot11_scan_results;
		cb.dot11_disassoc = ses_dot11_disassoc;
		cb.get_media_state = ses_get_media_state;
		cb.get_adapter_type = ses_get_adapter_type;
		cb.get_wlname = ses_get_wlname;
#ifdef SES_WDS_CLIENT
	} else if (cb_data->wds_enable) {
		cb.dot11_join = ses_wds_dot11_join;
		cb.dot11_scan = ses_wds_dot11_scan;
		cb.dot11_scan_results = ses_wds_dot11_scan_results;
		cb.dot11_disassoc = ses_wds_dot11_disassoc;
		cb.get_media_state = ses_wds_get_media_state;
		cb.get_adapter_type = ses_wds_get_adapter_type;
		cb.get_wlname = ses_wds_get_wlname;
#endif /* SES_WDS_CLIENT */
	} else
		assert(0);

	while (1) {
		result = ses_open(&ctx, &settings, &cb, cb_data);
		SES_CB("returned ses_open() with status %d\n", result);

		if (result == SES_RESULT_SUCCESS) {
			result = ses_go(ctx, &data);
			SES_CB("returned ses_go() with status %d\n", result);
			ses_close(ctx);
		} else {
			break;
		}

		if (result == SES_RESULT_SUCCESS) {
			SES_CB("SUCCESS with ssid %s passphrase %s security 0x%x "
				"encryption 0x%x channel %d remote ea %s\n",
				data.ssid, data.passphrase, data.security,
				data.encryption, data.channel,
				ether_etoa((unsigned char *)&data.remote, eabuf));

			if (cb_data->wds_enable) {
				brcm_ses_wds_add(unit, &data);
			} else if (cb_data->wet_enable) {
				brcm_ses_wet_add(unit, &data);
			} else
				assert(0);

			brcm_ses_sys_reinit();
			SES_CB("SHOULD NOT COME HERE!!!!!");
		}

		if ((result != SES_RESULT_USER_CANCEL) &&
		    (result != SES_RESULT_RW_TIMEOUT)) {
			break;
		}

		SES_CB("restarting state machine again\n");
	}

	brcm_ses_sys_reinit();
	SES_CB("SHOULD NOT COME HERE!!!!!");

	return 0;
}

static int
ses_cl_ses_set(int unit, char *ssid, char *passphrase)
{
	ses_store_tuple_t *t;
	char prefix[NVRAM_BUFSIZE], tmp[NVRAM_BUFSIZE];

	SES_CB("setting passphrase %s for ssid %s\n", passphrase, ssid);

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	for (t = ses_store; t->name; t++) {
		if (strcmp(t->ses_value, ""))
			nvram_set(strcat_r(prefix, &t->name[3], tmp), t->ses_value);
	}

	nvram_set(strcat_r(prefix, "ssid", tmp), ssid);
	nvram_set(strcat_r(prefix, "wpa_psk", tmp), passphrase);

	return 0;
}

static int
ses_cl_ses_reset(int unit)
{
	ses_store_tuple_t *t;
	char prefix[NVRAM_BUFSIZE], tmp[NVRAM_BUFSIZE];
	char *value;
	int i;

	SES_CB("restoring default network settings\n");

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	for (t = ses_store; t->name; t++) {
		value = brcm_get_default_value(t->name);
		assert(value);
		nvram_set(strcat_r(prefix, &t->name[3], tmp), value);
	}

#ifdef SES_WDS_CLIENT
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
#endif /* SES_WDS_CLIENT */

	return 0;
}

static bool
ses_cl_network_is_ses(int unit, int wet)
{
	ses_store_tuple_t *t;
	char prefix[NVRAM_BUFSIZE], tmp[NVRAM_BUFSIZE];
	char *value;
	int i;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	for (t = ses_store; t->name; t++) {
		value = nvram_safe_get(strcat_r(prefix, &t->name[3], tmp));

		/* remove trailing spaces */
		strcpy(tmp, value);
		for (i = strlen(tmp)-1; (i != 0) && (tmp[i] == ' '); i--);
		tmp[i+1] = '\0';

		if (wet) {
			if (!strcmp(&t->name[3], "akm")) {
				if (!strstr(tmp, "psk"))
					return FALSE;
				else
					continue;
			}
			if (!strcmp(&t->name[3], "crypto")) {
				if (!strstr(tmp, "tkip") && !strstr(tmp, "aes"))
					return FALSE;
				else
					continue;
			}
		}

		if (strcmp(t->ses_value, "")) {
			if (strcmp(tmp, t->ses_value)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

static int
brcm_ses_wds_add(int unit, ses_data_t *data)
{
	char name[NVRAM_BUFSIZE], value[NVRAM_BUFSIZE];
	char eabuf[ETHER_ADDR_STR_LEN];
	char *tmp;

	ses_cl_ses_set(unit, data->ssid, data->passphrase);

	snprintf(name, sizeof(name), "wl%d_wds", unit);
	tmp = nvram_safe_get(name);
	if (!strstr(tmp, ether_etoa((unsigned char *)(&data->remote), eabuf))) {
		snprintf(value, sizeof(value), "%s %s", tmp, eabuf);
		nvram_set(name, value);
	}

	/* set wds security settings */
	snprintf(name, sizeof(name), "wl%d_wds%d", unit, SES_WDS_ENTRY);
	snprintf(value, sizeof(value), "*,auto,tkip,psk,%s,%s",
		data->ssid, data->passphrase);
	nvram_set(name, value);

	/* set channel */
	snprintf(name, sizeof(name), "wl%d_channel", unit);
	snprintf(value, sizeof(value), "%d", data->channel);
	nvram_set(name, value);

	/* if lan ip is default then change it to dhcp */
	if (!strcmp(nvram_safe_get("lan_ipaddr"), brcm_get_default_value("lan_ipaddr")))
		nvram_set("lan_dhcp", "1");

	/* disable router mode */
	nvram_set("router_disable", "1");

	/* set the ses status */
	nvram_set("ses_status", "Success");

	nvram_commit();

	return 0;
}

static int
brcm_ses_wet_add(int unit, ses_data_t *data)
{
	char name[NVRAM_BUFSIZE], value[NVRAM_BUFSIZE];

	ses_cl_ses_set(unit, data->ssid, data->passphrase);

	/* override akm based on server settings */
	snprintf(name, sizeof(name), "wl%d_akm", unit);
	if (data->security & SES_SECURITY_WPA2_PSK)
		snprintf(value, sizeof(value), "psk2");
	else if (data->security & SES_SECURITY_WPA_PSK)
		snprintf(value, sizeof(value), "psk");
	else
		assert(0);
	nvram_set(name, value);

	/* override encryption based on server settings */
	snprintf(name, sizeof(name), "wl%d_crypto", unit);
	if (data->encryption == SES_ENCRYPTION_TKIP)
		snprintf(value, sizeof(value), "tkip");
	else if (data->encryption == SES_ENCRYPTION_AES)
		snprintf(value, sizeof(value), "aes");
	else if (data->encryption == SES_ENCRYPTION_TKIP_AES)
		snprintf(value, sizeof(value), "tkip+aes");
	else
		assert(0);
	nvram_set(name, value);

	return 0;
}

static int
brcm_ses_retrieve_event(ses_cb_data_t *cb_data)
{
	char *value;
	char buf[4];
	int event = SES_OP_NULL;

	switch (ses_btn_pressed()) {
		case SES_NO_BTNPRESS:
			value = nvram_safe_get(SES_CL_NV_EVENT);
			if ((event = atoi(value)) != SES_OP_NULL) {
				sprintf(buf, "%d", SES_OP_NULL);
				nvram_set(SES_CL_NV_EVENT, buf);
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

static char *
brcm_get_default_value(char *name)
{
	struct nvram_tuple *t;

	for (t = router_defaults; t->name; t++) {
		if (!strcmp(name, t->name))
			return t->value;
	}

	return NULL;
}

#ifdef SES_WDS_CLIENT
char *
ses_get_underlying_wlname(char const *ifname, char *wlname)
{
	char name[NVRAM_BUFSIZE], *next, *lan_ifnames;

	if (wl_probe((char *)ifname) == 0) {
		strcpy(wlname, ifname);
		return wlname;
	}

	/* probe for first wl interface */
	lan_ifnames = nvram_safe_get("lan_ifnames");
	foreach(name, lan_ifnames, next) {
		if (wl_probe(name) == 0)
			break;
	}

	assert(name);
	strcpy(wlname, name);
	return wlname;
}
#endif /* SES_WDS_CLIENT */
