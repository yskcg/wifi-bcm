/*
 * Router rc control script
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: rc.c 281156 2011-09-01 08:30:55Z kenlo $
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <sys/klog.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <dirent.h>

#include <epivers.h>
#include <router_version.h>
#include <bcmnvram.h>
#include <mtd.h>
#include <shutils.h>
#include <rc.h>
#include <netconf.h>
#include <nvparse.h>
#include <bcmdevs.h>
#include <bcmparams.h>
#include <wlutils.h>
#include <ezc.h>
#include <pmon.h>

#undef HAVE_CONFMTD
#if defined(__CONFIG_WAPI__) || defined(__CONFIG_WAPI_IAS__) || \
	defined(__CONFIG_CIFS__)
#define HAVE_CONFMTD
#include <confmtd_utils.h>
#if defined(__CONFIG_WAPI__) || defined(__CONFIG_WAPI_IAS__)
#include <wapi_path.h>
#endif
#if defined(__CONFIG_CIFS__)
#include <cifs_path.h>
#endif
#endif /* __CONFIG_WAPI__ || __CONFIG_WAPI_IAS__ || __CONFIG_CIFS__ */

#ifdef __CONFIG_NAT__
static void auto_bridge(void);
#endif	/* __CONFIG_NAT__ */

#ifdef __CONFIG_EMF__
extern void load_emf(void);
#endif /* __CONFIG_EMF__ */

static void restore_defaults(void);
static void sysinit(void);
static void rc_signal(int sig);

extern struct nvram_tuple router_defaults[];

#define RESTORE_DEFAULTS() \
	(!nvram_match("restore_defaults", "0") || nvram_invmatch("os_name", "linux"))


static int
build_ifnames(char *type, char *names, int *size)
{
	char name[32], *next;
	int len = 0;
	int s;

	/* open a raw scoket for ioctl */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return -1;

	/*
	 * go thru all device names (wl<N> il<N> et<N> vlan<N>) and interfaces to 
	 * build an interface name list in which each i/f name coresponds to a device
	 * name in device name list. Interface/device name matching rule is device
	 * type dependant:
	 *
	 *	wl:	by unit # provided by the driver, for example, if eth1 is wireless
	 *		i/f and its unit # is 0, then it will be in the i/f name list if
	 *		wl0 is in the device name list.
	 *	il/et:	by mac address, for example, if et0's mac address is identical to
	 *		that of eth2's, then eth2 will be in the i/f name list if et0 is 
	 *		in the device name list.
	 *	vlan:	by name, for example, vlan0 will be in the i/f name list if vlan0
	 *		is in the device name list.
	 */
	foreach(name, type, next) {
		struct ifreq ifr;
		int i, unit;
		char var[32], *mac;
		unsigned char ea[ETHER_ADDR_LEN];

		/* vlan: add it to interface name list */
		if (!strncmp(name, "vlan", 4)) {
			/* append interface name to list */
			len += snprintf(&names[len], *size - len, "%s ", name);
			continue;
		}

		/* others: proceed only when rules are met */
		for (i = 1; i <= DEV_NUMIFS; i ++) {
			/* ignore i/f that is not ethernet */
			ifr.ifr_ifindex = i;
			if (ioctl(s, SIOCGIFNAME, &ifr))
				continue;
			if (ioctl(s, SIOCGIFHWADDR, &ifr))
				continue;
			if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
				continue;
			if (!strncmp(ifr.ifr_name, "vlan", 4))
				continue;

			/* wl: use unit # to identify wl */
			if (!strncmp(name, "wl", 2)) {
				if (wl_probe(ifr.ifr_name) ||
				    wl_ioctl(ifr.ifr_name, WLC_GET_INSTANCE, &unit, sizeof(unit)) ||
				    unit != atoi(&name[2]))
					continue;
			}
			/* et/il: use mac addr to identify et/il */
			else if (!strncmp(name, "et", 2) || !strncmp(name, "il", 2)) {
				snprintf(var, sizeof(var), "%smacaddr", name);
				if (!(mac = nvram_get(var)) || !ether_atoe(mac, ea) ||
				    bcmp(ea, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN))
					continue;
			}
			/* mac address: compare value */
			else if (ether_atoe(name, ea) &&
				!bcmp(ea, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN))
				;
			/* others: ignore */
			else
				continue;

			/* append interface name to list */
			len += snprintf(&names[len], *size - len, "%s ", ifr.ifr_name);
		}
	}

	close(s);

	*size = len;
	return 0;
}

#ifdef __CONFIG_WPS__
static void
wps_restore_defaults(void)
{
	/* cleanly up nvram for WPS */
	nvram_unset("wps_seed");
	nvram_unset("wps_config_state");
	nvram_unset("wps_addER");
	nvram_unset("wps_device_pin");
	nvram_unset("wps_pbc_force");
	nvram_unset("wps_config_command");
	nvram_unset("wps_proc_status");
	nvram_unset("wps_status");
	nvram_unset("wps_method");
	nvram_unset("wps_proc_mac");
	nvram_unset("wps_sta_pin");
	nvram_unset("wps_currentband");
	nvram_unset("wps_restart");
	nvram_unset("wps_event");

	nvram_unset("wps_enr_mode");
	nvram_unset("wps_enr_ifname");
	nvram_unset("wps_enr_ssid");
	nvram_unset("wps_enr_bssid");
	nvram_unset("wps_enr_wsec");

	nvram_unset("wps_unit");
}
#endif /* __CONFIG_WPS__ */

static void
ses_cleanup(void)
{
	/* well known event to cleanly initialize state machine */
	nvram_set("ses_event", "2");

	/* Delete lethal dynamically generated variables */
	nvram_unset("ses_bridge_disable");
}

static void
ses_restore_defaults(void)
{
	char tmp[100], prefix[] = "wlXXXXXXXXXX_ses_";
	int i, j;
	int len = 0;

	/* Delete dynamically generated variables */
	for (i = 0; i < MAX_NVPARSE; i++) {
		sprintf(prefix, "wl%d_ses_", i);
		nvram_unset(strcat_r(prefix, "ssid", tmp));
		nvram_unset(strcat_r(prefix, "closed", tmp));
		nvram_unset(strcat_r(prefix, "wpa_psk", tmp));
		nvram_unset(strcat_r(prefix, "auth", tmp));
		nvram_unset(strcat_r(prefix, "wep", tmp));
		nvram_unset(strcat_r(prefix, "auth_mode", tmp));
		nvram_unset(strcat_r(prefix, "crypto", tmp));
		nvram_unset(strcat_r(prefix, "akm", tmp));
		nvram_unset(strcat_r(prefix, "wds", tmp));
		nvram_unset(strcat_r(prefix, "wds_timeout", tmp));
	}

	/* Delete dynamically generated variables */
	for (i = 0; i < DEV_NUMIFS; i++) {
		sprintf(prefix, "wl%d_wds", i);
		len = strlen(prefix);
		for (j = 0; j < MAX_NVPARSE; j++) {
			sprintf(&prefix[len], "%d", j);
			nvram_unset(prefix);
		}
	}

	nvram_unset("ses_fsm_current_states");
	nvram_unset("ses_fsm_last_status");
}
static void
virtual_radio_restore_defaults(void)
{
	char tmp[100], prefix[] = "wlXXXXXXXXXX_mssid_";
	int i, j;

	nvram_unset("unbridged_ifnames");
	nvram_unset("ure_disable");

	/* Delete dynamically generated variables */
	for (i = 0; i < MAX_NVPARSE; i++) {
		sprintf(prefix, "wl%d_", i);
		nvram_unset(strcat_r(prefix, "vifs", tmp));
		nvram_unset(strcat_r(prefix, "ssid", tmp));
		nvram_unset(strcat_r(prefix, "guest", tmp));
		nvram_unset(strcat_r(prefix, "ure", tmp));
		nvram_unset(strcat_r(prefix, "ipconfig_index", tmp));
		nvram_unset(strcat_r(prefix, "nas_dbg", tmp));
		sprintf(prefix, "lan%d_", i);
		nvram_unset(strcat_r(prefix, "ifname", tmp));
		nvram_unset(strcat_r(prefix, "ifnames", tmp));
		nvram_unset(strcat_r(prefix, "gateway", tmp));
		nvram_unset(strcat_r(prefix, "proto", tmp));
		nvram_unset(strcat_r(prefix, "ipaddr", tmp));
		nvram_unset(strcat_r(prefix, "netmask", tmp));
		nvram_unset(strcat_r(prefix, "lease", tmp));
		nvram_unset(strcat_r(prefix, "stp", tmp));
		nvram_unset(strcat_r(prefix, "hwaddr", tmp));
		sprintf(prefix, "dhcp%d_", i);
		nvram_unset(strcat_r(prefix, "start", tmp));
		nvram_unset(strcat_r(prefix, "end", tmp));

		/* clear virtual versions */
		for (j = 0; j < 16; j++) {
			sprintf(prefix, "wl%d.%d_", i, j);
			nvram_unset(strcat_r(prefix, "ssid", tmp));
			nvram_unset(strcat_r(prefix, "ipconfig_index", tmp));
			nvram_unset(strcat_r(prefix, "guest", tmp));
			nvram_unset(strcat_r(prefix, "closed", tmp));
			nvram_unset(strcat_r(prefix, "wpa_psk", tmp));
			nvram_unset(strcat_r(prefix, "auth", tmp));
			nvram_unset(strcat_r(prefix, "wep", tmp));
			nvram_unset(strcat_r(prefix, "auth_mode", tmp));
			nvram_unset(strcat_r(prefix, "crypto", tmp));
			nvram_unset(strcat_r(prefix, "akm", tmp));
			nvram_unset(strcat_r(prefix, "hwaddr", tmp));
			nvram_unset(strcat_r(prefix, "bss_enabled", tmp));
			nvram_unset(strcat_r(prefix, "bss_maxassoc", tmp));
			nvram_unset(strcat_r(prefix, "wme_bss_disable", tmp));
			nvram_unset(strcat_r(prefix, "ifname", tmp));
			nvram_unset(strcat_r(prefix, "unit", tmp));
			nvram_unset(strcat_r(prefix, "ap_isolate", tmp));
			nvram_unset(strcat_r(prefix, "macmode", tmp));
			nvram_unset(strcat_r(prefix, "maclist", tmp));
			nvram_unset(strcat_r(prefix, "maxassoc", tmp));
			nvram_unset(strcat_r(prefix, "mode", tmp));
			nvram_unset(strcat_r(prefix, "radio", tmp));
			nvram_unset(strcat_r(prefix, "radius_ipaddr", tmp));
			nvram_unset(strcat_r(prefix, "radius_port", tmp));
			nvram_unset(strcat_r(prefix, "radius_key", tmp));
			nvram_unset(strcat_r(prefix, "key", tmp));
			nvram_unset(strcat_r(prefix, "key1", tmp));
			nvram_unset(strcat_r(prefix, "key2", tmp));
			nvram_unset(strcat_r(prefix, "key3", tmp));
			nvram_unset(strcat_r(prefix, "key4", tmp));
			nvram_unset(strcat_r(prefix, "wpa_gtk_rekey", tmp));
			nvram_unset(strcat_r(prefix, "nas_dbg", tmp));
		}
	}
}
static void
ses_cl_cleanup(void)
{
	/* well known event to cleanly initialize state machine */
	nvram_set("ses_cl_event", "0");
}

#ifdef __CONFIG_NAT__
static void
auto_bridge(void)
{

	struct nvram_tuple generic[] = {
		{ "lan_ifname", "br0", 0 },
		{ "lan_ifnames", "eth0 eth2 eth3 eth4", 0 },
		{ "wan_ifname", "eth1", 0 },
		{ "wan_ifnames", "eth1", 0 },
		{ 0, 0, 0 }
	};
#ifdef __CONFIG_VLAN__
	struct nvram_tuple vlan[] = {
		{ "lan_ifname", "br0", 0 },
		{ "lan_ifnames", "vlan0 eth1 eth2 eth3", 0 },
		{ "wan_ifname", "vlan1", 0 },
		{ "wan_ifnames", "vlan1", 0 },
		{ 0, 0, 0 }
	};
#endif	/* __CONFIG_VLAN__ */
	struct nvram_tuple dyna[] = {
		{ "lan_ifname", "br0", 0 },
		{ "lan_ifnames", "", 0 },
		{ "wan_ifname", "", 0 },
		{ "wan_ifnames", "", 0 },
		{ 0, 0, 0 }
	};
	struct nvram_tuple generic_auto_bridge[] = {
		{ "lan_ifname", "br0", 0 },
		{ "lan_ifnames", "eth0 eth1 eth2 eth3 eth4", 0 },
		{ "wan_ifname", "", 0 },
		{ "wan_ifnames", "", 0 },
		{ 0, 0, 0 }
	};
#ifdef __CONFIG_VLAN__
	struct nvram_tuple vlan_auto_bridge[] = {
		{ "lan_ifname", "br0", 0 },
		{ "lan_ifnames", "vlan0 vlan1 eth1 eth2 eth3", 0 },
		{ "wan_ifname", "", 0 },
		{ "wan_ifnames", "", 0 },
		{ 0, 0, 0 }
	};
#endif	/* __CONFIG_VLAN__ */

	struct nvram_tuple dyna_auto_bridge[] = {
		{ "lan_ifname", "br0", 0 },
		{ "lan_ifnames", "", 0 },
		{ "wan_ifname", "", 0 },
		{ "wan_ifnames", "", 0 },
		{ 0, 0, 0 }
	};

	struct nvram_tuple *linux_overrides;
	struct nvram_tuple *t, *u;
	int auto_bridge = 0, i;
#ifdef __CONFIG_VLAN__
	uint boardflags;
#endif	/* __CONFIG_VLAN_ */
	char *landevs, *wandevs;
	char lan_ifnames[128], wan_ifnames[128];
	char dyna_auto_ifnames[128];
	char wan_ifname[32], *next;
	int len;
	int ap = 0;

	printf(" INFO : enter function auto_bridge()\n");

	if (!strcmp(nvram_safe_get("auto_bridge_action"), "1")) {
		auto_bridge = 1;
		cprintf("INFO: Start auto bridge...\n");
	} else {
		nvram_set("router_disable_auto", "0");
		cprintf("INFO: Start non auto_bridge...\n");
	}

	/* Delete dynamically generated variables */
	if (auto_bridge) {
		char tmp[100], prefix[] = "wlXXXXXXXXXX_";
		for (i = 0; i < MAX_NVPARSE; i++) {

			del_filter_client(i);
			del_forward_port(i);
			del_autofw_port(i);


			snprintf(prefix, sizeof(prefix), "wan%d_", i);
			for (t = router_defaults; t->name; t ++) {
				if (!strncmp(t->name, "wan_", 4))
					nvram_unset(strcat_r(prefix, &t->name[4], tmp));
			}
		}
	}

	/* 
	 * Build bridged i/f name list and wan i/f name list from lan device name list
	 * and wan device name list. Both lan device list "landevs" and wan device list
	 * "wandevs" must exist in order to preceed.
	 */
	if ((landevs = nvram_get("landevs")) && (wandevs = nvram_get("wandevs"))) {
		/* build bridged i/f list based on nvram variable "landevs" */
		len = sizeof(lan_ifnames);
		if (!build_ifnames(landevs, lan_ifnames, &len) && len)
			dyna[1].value = lan_ifnames;
		else
			goto canned_config;
		/* build wan i/f list based on nvram variable "wandevs" */
		len = sizeof(wan_ifnames);
		if (!build_ifnames(wandevs, wan_ifnames, &len) && len) {
			dyna[3].value = wan_ifnames;
			foreach(wan_ifname, wan_ifnames, next) {
				dyna[2].value = wan_ifname;
				break;
			}
		}
		else
			ap = 1;

		if (auto_bridge)
		{
			printf("INFO: lan_ifnames=%s\n", lan_ifnames);
			printf("INFO: wan_ifnames=%s\n", wan_ifnames);
			sprintf(dyna_auto_ifnames, "%s %s", lan_ifnames, wan_ifnames);
			printf("INFO: dyna_auto_ifnames=%s\n", dyna_auto_ifnames);
			dyna_auto_bridge[1].value = dyna_auto_ifnames;
			linux_overrides = dyna_auto_bridge;
			printf("INFO: linux_overrides=dyna_auto_bridge \n");
		}
		else
		{
			linux_overrides = dyna;
			printf("INFO: linux_overrides=dyna \n");
		}

	}
	/* override lan i/f name list and wan i/f name list with default values */
	else {
canned_config:
#ifdef __CONFIG_VLAN__
		boardflags = strtoul(nvram_safe_get("boardflags"), NULL, 0);
		if (boardflags & BFL_ENETVLAN) {
			if (auto_bridge)
			{
				linux_overrides = vlan_auto_bridge;
				printf("INFO: linux_overrides=vlan_auto_bridge \n");
			}
			else
			{
				linux_overrides = vlan;
				printf("INFO: linux_overrides=vlan \n");
			}
		} else {
#endif	/* __CONFIG_VLAN__ */
			if (auto_bridge)
			{
				linux_overrides = generic_auto_bridge;
				printf("INFO: linux_overrides=generic_auto_bridge \n");
			}
			else
			{
				linux_overrides = generic;
				printf("INFO: linux_overrides=generic \n");
			}
#ifdef __CONFIG_VLAN__
		}
#endif	/* __CONFIG_VLAN__ */
	}

		for (u = linux_overrides; u && u->name; u++) {
			nvram_set(u->name, u->value);
			printf("INFO: action nvram_set %s, %s\n", u->name, u->value);
			}

	/* Force to AP */
	if (ap)
		nvram_set("router_disable", "1");

	if (auto_bridge) {
		printf("INFO: reset auto_bridge flag.\n");
		nvram_set("auto_bridge_action", "0");
	}

	nvram_commit();
	cprintf("auto_bridge done\n");
}

#endif	/* __CONFIG_NAT__ */


static void
upgrade_defaults(void)
{
	char temp[100];
	int i;
	bool bss_enabled = TRUE;
	char *val;

	/* Check whether upgrade is required or not
	 * If lan1_ifnames is not found in NVRAM , upgrade is required.
	 */
	if (!nvram_get("lan1_ifnames") && !RESTORE_DEFAULTS()) {
		cprintf("NVRAM upgrade required.  Starting.\n");

		if (nvram_match("ure_disable", "1")) {
			nvram_set("lan1_ifname", "br1");
			nvram_set("lan1_ifnames", "wl0.1 wl0.2 wl0.3 wl1.1 wl1.2 wl1.3");
		}
		else {
			nvram_set("lan1_ifname", "");
			nvram_set("lan1_ifnames", "");
			for (i = 0; i < 2; i++) {
				snprintf(temp, sizeof(temp), "wl%d_ure", i);
				if (nvram_match(temp, "1")) {
					snprintf(temp, sizeof(temp), "wl%d.1_bss_enabled", i);
					nvram_set(temp, "1");
				}
				else {
					bss_enabled = FALSE;
					snprintf(temp, sizeof(temp), "wl%d.1_bss_enabled", i);
					nvram_set(temp, "0");
				}
			}
		}
		if (nvram_get("lan1_ipaddr")) {
			nvram_set("lan1_gateway", nvram_get("lan1_ipaddr"));
		}

		for (i = 0; i < 2; i++) {
			snprintf(temp, sizeof(temp), "wl%d_bss_enabled", i);
			nvram_set(temp, "1");
			snprintf(temp, sizeof(temp), "wl%d.1_guest", i);
			if (nvram_match(temp, "1")) {
				nvram_unset(temp);
				if (bss_enabled) {
					snprintf(temp, sizeof(temp), "wl%d.1_bss_enabled", i);
					nvram_set(temp, "1");
				}
			}

			snprintf(temp, sizeof(temp), "wl%d.1_net_reauth", i);
			val = nvram_get(temp);
			if (!val || (*val == 0))
				nvram_set(temp, nvram_default_get(temp));

			snprintf(temp, sizeof(temp), "wl%d.1_wpa_gtk_rekey", i);
			val = nvram_get(temp);
			if (!val || (*val == 0))
				nvram_set(temp, nvram_default_get(temp));
		}

		nvram_commit();

		cprintf("NVRAM upgrade complete.\n");
	}
}

static void
restore_defaults(void)
{
	struct nvram_tuple generic[] = {
		{ "lan_ifname", "br0", 0 },
		{ "lan_ifnames", "eth0 eth2 eth3 eth4", 0 },
		{ "wan_ifname", "eth1", 0 },
		{ "wan_ifnames", "eth1", 0 },
		{ "lan1_ifname", "br1", 0 },
		{ "lan1_ifnames", "wl0.1 wl0.2 wl0.3 wl1.1 wl1.2 wl1.3", 0 },
		{ 0, 0, 0 }
	};
#ifdef __CONFIG_VLAN__
	struct nvram_tuple vlan[] = {
#ifdef LW_CUST
		{ "lan_ifname", "br0", 0 },
		{ "lan_ifnames", "vlan1 vlan200 eth1", 0 },
		{ "wan_ifname", "br2", 0 },
		{ "wan_ifnames", "br2 wl0.1", 0 },
		{ "lan1_ifname", "br1", 0 },
		{ "lan1_ifnames", "wl0.2 wl0.3 wl1.1 wl1.2 wl1.3", 0 },
        { "wl0_ssidheader", "HiTV_", 0},
        { "wl_ssid", "HiTV_112233" , 0},
#else
		{ "lan_ifname", "br0", 0 },
		{ "lan_ifnames", "vlan0 eth1 eth2 eth3", 0 },
		{ "wan_ifname", "vlan1", 0 },
		{ "wan_ifnames", "vlan1", 0 },
		{ "lan1_ifname", "br1", 0 },
		{ "lan1_ifnames", "wl0.1 wl0.2 wl0.3 wl1.1 wl1.2 wl1.3", 0 },
#endif
		{ 0, 0, 0 }
	};
#endif	/* __CONFIG_VLAN__ */
	struct nvram_tuple dyna[] = {
		{ "lan_ifname", "br0", 0 },
		{ "lan_ifnames", "", 0 },
		{ "wan_ifname", "br2", 0 },
		{ "wan_ifnames", "br2", 0 },
		{ "lan1_ifname", "br1", 0 },
		{ "lan1_ifnames", "wl0.2 wl0.3 wl1.1 wl1.2 wl1.3", 0 },
		{ 0, 0, 0 }
	};

	struct nvram_tuple *linux_overrides;
	struct nvram_tuple *t, *u;
	int restore_defaults, i;
#ifdef __CONFIG_VLAN__
	uint boardflags;
#endif	/* __CONFIG_VLAN_ */
	char *landevs, *wandevs;
	char lan_ifnames[128], wan_ifnames[128];
	char wan_ifname[32], *next;
	int len;
	int ap = 0;

	/* Restore defaults if told to or OS has changed */
	restore_defaults = RESTORE_DEFAULTS();

	if (restore_defaults)
		cprintf("Restoring defaults...");

	/* Delete dynamically generated variables */
	if (restore_defaults) {
		char tmp[100], prefix[] = "wlXXXXXXXXXX_";
		for (i = 0; i < MAX_NVPARSE; i++) {
#ifdef __CONFIG_NAT__
			del_filter_client(i);
			del_forward_port(i);
			del_autofw_port(i);
#endif	/* __CONFIG_NAT__ */
			snprintf(prefix, sizeof(prefix), "wl%d_", i);
			for (t = router_defaults; t->name; t ++) {
				if (!strncmp(t->name, "wl_", 3))
					nvram_unset(strcat_r(prefix, &t->name[3], tmp));
			}
#ifdef __CONFIG_NAT__
			snprintf(prefix, sizeof(prefix), "wan%d_", i);
			for (t = router_defaults; t->name; t ++) {
				if (!strncmp(t->name, "wan_", 4))
					nvram_unset(strcat_r(prefix, &t->name[4], tmp));
			}
#endif	/* __CONFIG_NAT__ */
		}
		ses_restore_defaults();
#ifdef __CONFIG_WPS__
		wps_restore_defaults();
#endif /* __CONFIG_WPS__ */
#ifdef __CONFIG_WAPI_IAS__
		nvram_unset("as_mode");
#endif /* __CONFIG_WAPI_IAS__ */
		virtual_radio_restore_defaults();
	}

	/* 
	 * Build bridged i/f name list and wan i/f name list from lan device name list
	 * and wan device name list. Both lan device list "landevs" and wan device list
	 * "wandevs" must exist in order to preceed.
	 */
	if ((landevs = nvram_get("landevs")) && (wandevs = nvram_get("wandevs"))) {
		/* build bridged i/f list based on nvram variable "landevs" */
		len = sizeof(lan_ifnames);
		if (!build_ifnames(landevs, lan_ifnames, &len) && len)
			dyna[1].value = lan_ifnames;
		else
			goto canned_config;
		/* build wan i/f list based on nvram variable "wandevs" */
		len = sizeof(wan_ifnames);
		if (!build_ifnames(wandevs, wan_ifnames, &len) && len) {
			dyna[3].value = wan_ifnames;
			foreach(wan_ifname, wan_ifnames, next) {
				dyna[2].value = wan_ifname;
				break;
			}
		}
		else
			ap = 1;
		linux_overrides = dyna;
	}
	/* override lan i/f name list and wan i/f name list with default values */
	else {
canned_config:
#ifdef __CONFIG_VLAN__
		boardflags = strtoul(nvram_safe_get("boardflags"), NULL, 0);
		if (boardflags & BFL_ENETVLAN)
			linux_overrides = vlan;
		else
#endif	/* __CONFIG_VLAN__ */
			linux_overrides = generic;
	}

	/* Check if nvram version is set, but old */
	if (nvram_get("nvram_version")) {
		int old_ver, new_ver;

		old_ver = atoi(nvram_get("nvram_version"));
		new_ver = atoi(NVRAM_SOFTWARE_VERSION);
		if (old_ver < new_ver) {
			cprintf("NVRAM: Updating from %d to %d\n", old_ver, new_ver);
			nvram_set("nvram_version", NVRAM_SOFTWARE_VERSION);
		}
	}

#ifdef LW_CUST
    char ssid_default[16];
	char mac[6][3];
    char *wl_addr = nvram_get("sb/1/macaddr");
	char *delims=":";
    if(!wl_addr)
        cprintf("oops, failed to get wifi mac\n");
	sscanf(wl_addr,"%s:%s:%s:%s:%s:%s",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	mac[3][2]=0;
	mac[4][2]=0;
	mac[5][2]=0;
	cprintf("wl_addr %s\n", wl_addr);
    memset(ssid_default, 0, sizeof(ssid_default));
    snprintf(ssid_default, sizeof(ssid_default), "%s%s%s%s",
            vlan[6].value, mac[3],mac[4],mac[5]);

    vlan[7].value = ssid_default;
    cprintf("default ssid to %s\n", vlan[7].value);
    //nvram_set("wl_ssid", ssid_default);
#endif

	/* Restore defaults */
	for (t = router_defaults; t->name; t++) {
		if (restore_defaults || !nvram_get(t->name)) {
			for (u = linux_overrides; u && u->name; u++) {
				if (!strcmp(t->name, u->name)) {
					nvram_set(u->name, u->value);
					break;
				}
			}
			if (!u || !u->name)
				nvram_set(t->name, t->value);
		}
	}

	/* Force to AP */
	if (ap)
		nvram_set("router_disable", "1");

	/* Always set OS defaults */
	nvram_set("os_name", "linux");
	nvram_set("os_version", ROUTER_VERSION_STR);
	nvram_set("os_date", __DATE__);
	/* Always set WL driver version! */
	nvram_set("wl_version", EPI_VERSION_STR);

	nvram_set("is_modified", "0");
	nvram_set("ezc_version", EZC_VERSION_STR);

	if (restore_defaults) {
		FILE *fp;
		char memdata[256] = {0};
		uint memsize = 0;
		char pktq_thresh[8] = {0};

		if ((fp = fopen("/proc/meminfo", "r")) != NULL) {
			/* get memory count in MemTotal = %d */
			while (fgets(memdata, 255, fp) != NULL) {
				if (strstr(memdata, "MemTotal") != NULL) {
					sscanf(memdata, "MemTotal:        %d kB", &memsize);
					break;
				}
			}
			fclose(fp);
		}

		if (memsize <= 32768) {
			/* Set to 512 as long as onboard memory <= 32M */
			sprintf(pktq_thresh, "512");
		}
		else {
			/* We still have to set the thresh to prevent oom killer */
			sprintf(pktq_thresh, "1024");
		}

		nvram_set("wl_txq_thresh", pktq_thresh);
		nvram_set("et_txq_thresh", pktq_thresh);
#if defined(__CONFIG_USBAP__)
		nvram_set("wl_rpcq_rxthresh", pktq_thresh);
#endif
	}

	/* Commit values */
	if (restore_defaults) {
		nvram_commit();
		cprintf("done\n");
	}
}

#ifdef __CONFIG_NAT__
static void
set_wan0_vars(void)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	/* check if there are any connections configured */
	for (unit = 0; unit < MAX_NVPARSE; unit ++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if (nvram_get(strcat_r(prefix, "unit", tmp)))
			break;
	}
	/* automatically configure wan0_ if no connections found */
	if (unit >= MAX_NVPARSE) {
		struct nvram_tuple *t;
		char *v;

		/* Write through to wan0_ variable set */
		snprintf(prefix, sizeof(prefix), "wan%d_", 0);
		for (t = router_defaults; t->name; t ++) {
			if (!strncmp(t->name, "wan_", 4)) {
				if (nvram_get(strcat_r(prefix, &t->name[4], tmp)))
					continue;
				v = nvram_get(t->name);
				nvram_set(tmp, v ? v : t->value);
			}
		}
		nvram_set(strcat_r(prefix, "unit", tmp), "0");
		nvram_set(strcat_r(prefix, "desc", tmp), "Default Connection");
		nvram_set(strcat_r(prefix, "primary", tmp), "1");
	}
}
#endif	/* __CONFIG_NAT__ */

#if defined(LINUX26) && defined(__CONFIG_NAND_JFFS2__)
int
jffs2_mtd_mount(void)
{
	FILE *fp;
	char dev[PATH_MAX];
	int i, ret = -1;
	char *jpath = "/tmp/media/nand";
	struct stat tmp_stat;

	if (!(fp = fopen("/proc/mtd", "r")))
		return ret;

	while (fgets(dev, sizeof(dev), fp)) {
		if (sscanf(dev, "mtd%d:", &i) && strstr(dev, "brcmnand")) {
#ifdef LINUX26
			snprintf(dev, sizeof(dev), "/dev/mtdblock%d", i);
#else
			snprintf(dev, sizeof(dev), "/dev/mtdblock/%d", i);
#endif
			if (stat(jpath, &tmp_stat) != 0)
				mkdir(jpath, 0777);

			/*
			 * More time will be taken for the first mounting of JFFS2 on
			 * bare nand device.
			 */
			ret = mount(dev, jpath, "jffs2", 0, NULL);

			/*
			 * Erase nand flash MTD partition and mount again, in case of mount failure.
			 */
			if (ret) {
				fprintf(stderr, "Erase nflash MTD partition and mount again\n");
				if ((ret = mtd_erase("brcmnand"))) {
					fprintf(stderr, "Erase nflash MTD partition %s failed %d\n",
						dev, ret);
				} else {
					ret = mount(dev, jpath, "jffs2", 0, NULL);
				}

				if (ret != 0) {
					fprintf(stderr, "Mount nflash MTD jffs2 "
					"partition %s to %s failed\n", dev, jpath);
				}
			}
			break;
		}
	}
	fclose(fp);

	return ret;
}
#endif /* LINUX26 && __CONFIG_NAND_JFFS2__ */

static int noconsole = 0;

static void
sysinit(void)
{
	char buf[PATH_MAX];
	struct utsname name;
	struct stat tmp_stat;
	time_t tm = 0;
	char *loglevel;

	/* /proc */
	mount("proc", "/proc", "proc", MS_MGC_VAL, NULL);
#ifdef LINUX26
	mount("sysfs", "/sys", "sysfs", MS_MGC_VAL, NULL);
#endif /* LINUX26 */

	/* /tmp */
	mount("ramfs", "/tmp", "ramfs", MS_MGC_VAL, NULL);

	/* /var */
	mkdir("/tmp/var", 0777);
	mkdir("/var/lock", 0777);
	mkdir("/var/log", 0777);
	mkdir("/var/run", 0777);
	mkdir("/var/tmp", 0777);
	mkdir("/tmp/media", 0777);

#ifdef __CONFIG_SAMBA__
	/* Add Samba Stuff */
	mkdir("/tmp/samba", 0777);
	mkdir("/tmp/samba/lib", 0777);
	mkdir("/tmp/samba/private", 0777);
	mkdir("/tmp/samba/var", 0777);
	mkdir("/tmp/samba/var/locks", 0777);
#endif

#ifdef BCMQOS
	mkdir("/tmp/qos", 0777);
#endif
	/* Setup console */
	if (console_init())
		noconsole = 1;

#ifdef LINUX26
	mkdir("/dev/shm", 0777);
	eval("/sbin/hotplug2", "--coldplug");
#endif /* LINUX26 */

	if ((loglevel = nvram_get("console_loglevel")))
		klogctl(8, NULL, atoi(loglevel));
	else
		klogctl(8, NULL, 1);

	/* Modules */
	uname(&name);
	snprintf(buf, sizeof(buf), "/lib/modules/%s", name.release);
	if (stat("/proc/modules", &tmp_stat) == 0 &&
	    stat(buf, &tmp_stat) == 0) {
		char module[80], *modules, *next;
		/* Load ctf */
		if (!nvram_match("ctf_disable", "1"))
			eval("insmod", "ctf");

#ifdef HAVE_CONFMTD
		confmtd_restore();

#if defined(__CONFIG_WAPI__) || defined(__CONFIG_WAPI_IAS__)
		if (stat(WAPI_DIR, &tmp_stat) != 0) {
			mkdir(WAPI_DIR, 0777);
			mkdir(WAPI_WAI_DIR, 0777);
			mkdir(WAPI_AS_DIR, 0777);
		}
#endif /* __CONFIG_WAPI__ || __CONFIG_WAPI_IAS__ */
#if defined(__CONFIG_CIFS__)
		if (stat(CIFS_DIR, &tmp_stat) != 0) {
			mkdir(CIFS_DIR, 0777);
			eval("cp", "/usr/sbin/cs_cfg.txt", CIFS_DIR);
			eval("cp", "/usr/sbin/cm_cfg.txt", CIFS_DIR);
			eval("cp", "/usr/sbin/pwd_list.txt", CIFS_DIR);
		}
#endif /* __CONFIG_CIFS__ */
#endif /* HAVE_CONFMTD */

#if defined(LINUX26) && defined(__CONFIG_NAND_JFFS2__)
		jffs2_mtd_mount();
#endif /* LINUX26 && __CONFIG_NAND_JFFS2__ */

/* #ifdef BCMVISTAROUTER */
#ifdef __CONFIG_IPV6__
		eval("insmod", "ipv6");
#endif /* __CONFIG_IPV6__ */
/* #endif */

#ifdef __CONFIG_EMF__
		/* Load the EMF & IGMP Snooper modules */
		load_emf();
#endif /*  __CONFIG_EMF__ */

		modules = nvram_get("kernel_mods") ? : "et bcm57xx wl";

		foreach(module, modules, next)
			eval("insmod", module);

		hotplug_usb_init();
#ifdef __CONFIG_USBAP__
		eval("insmod", "usbcore");
		eval("insmod", "usb-storage");
		{
			char	insmod_arg[128];
			int	i = 0, maxwl_eth = 0, maxunit = -1;
			char	ifname[16] = {0};
			int	unit = -1;
			char arg1[20] = {0};
			char arg2[20] = {0};
			char arg3[20] = {0};
			char arg4[20] = {0};
			char arg5[20] = {0};
			char arg6[20] = {0};
			char arg7[20] = {0};
			const int wl_wait = 3;	/* max wait time for wl_high to up */

			/* Save QTD cache params in nvram */
			sprintf(arg1, "log2_irq_thresh=%d", atoi(nvram_safe_get("ehciirqt")));
			sprintf(arg2, "qtdc_pid=%d", atoi(nvram_safe_get("qtdc_pid")));
			sprintf(arg3, "qtdc_vid=%d", atoi(nvram_safe_get("qtdc_vid")));
			sprintf(arg4, "qtdc0_ep=%d", atoi(nvram_safe_get("qtdc0_ep")));
			sprintf(arg5, "qtdc0_sz=%d", atoi(nvram_safe_get("qtdc0_sz")));
			sprintf(arg6, "qtdc1_ep=%d", atoi(nvram_safe_get("qtdc1_ep")));
			sprintf(arg7, "qtdc1_sz=%d", atoi(nvram_safe_get("qtdc1_sz")));

			eval("insmod", "ehci-hcd", arg1, arg2, arg3, arg4, arg5,
				arg6, arg7);

			/* Search for existing PCI wl devices and the max unit number used.
			 * Note that PCI driver has to be loaded before USB hotplug event.
			 * This is enforced in rc.c
			 */
			for (i = 1; i <= DEV_NUMIFS; i++) {
				sprintf(ifname, "eth%d", i);
				if (!wl_probe(ifname)) {
					if (!wl_ioctl(ifname, WLC_GET_INSTANCE, &unit,
						sizeof(unit))) {
						maxwl_eth = i;
						maxunit = (unit > maxunit) ? unit : maxunit;
					}
				}
			}

			/* Set instance base (starting unit number) for USB device */
			sprintf(insmod_arg, "instance_base=%d", maxunit + 1);
			eval("insmod", "wl_high", insmod_arg);

			/* Hold until the USB/HSIC interface is up (up to wl_wait sec) */
			sprintf(ifname, "eth%d", maxwl_eth + 1);
			i = wl_wait;
			while (wl_probe(ifname) && i--) {
				sleep(1);
			}
			if (!wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
				cprintf("wl%d is up in %d sec\n", unit, wl_wait - i);
			else
				cprintf("wl%d not up in %d sec\n", unit, wl_wait);
		}
#ifdef LINUX26
		mount("usbdeffs", "/proc/bus/usb", "usbfs", MS_MGC_VAL, NULL);
#else
		mount("none", "/proc/bus/usb", "usbdevfs", MS_MGC_VAL, NULL);
#endif /* LINUX26 */
#endif /* __CONFIG_USBAP__ */

#ifdef __CONFIG_WCN__
		modules = "scsi_mod sd_mod usbcore usb-ohci usb-storage fat vfat msdos";
		foreach(module, modules, next)
			eval("insmod", module);
#endif

#ifdef __CONFIG_SOUND__
		modules = "soundcore snd snd-timer snd-page-alloc snd-pcm snd-pcm-oss "
		        "snd-soc-core i2c-core i2c-algo-bit i2c-gpio snd-soc-bcm947xx-i2s "
		        "snd-soc-bcm947xx-pcm snd-soc-wm8750 snd-soc-wm8955 snd-soc-bcm947xx";
		foreach(module, modules, next)
			eval("insmod", module);
		mknod("/dev/dsp", S_IRWXU|S_IFCHR, makedev(14, 3));
		mkdir("/dev/snd", 0777);
		mknod("/dev/snd/controlC0", S_IRWXU|S_IFCHR, makedev(116, 0));
		mknod("/dev/snd/pcmC0D0c", S_IRWXU|S_IFCHR, makedev(116, 24));
		mknod("/dev/snd/pcmC0D0p", S_IRWXU|S_IFCHR, makedev(116, 16));
		mknod("/dev/snd/timer", S_IRWXU|S_IFCHR, makedev(116, 33));
#endif
	}

	/* Set a sane date */
	stime(&tm);

	dprintf("done\n");
}

/* States */
enum {
	RESTART,
	STOP,
	START,
	TIMER,
	IDLE
};
static int state = START;
static int signalled = -1;

/* Signal handling */
static void
rc_signal(int sig)
{
	if (sig == SIGHUP) {
		dprintf("signalling RESTART\n");
		signalled = RESTART;
	}
	else if (sig == SIGUSR2) {
		dprintf("signalling START\n");
		signalled = START;
	}
	else if (sig == SIGINT) {
		dprintf("signalling STOP\n");
		signalled = STOP;
	}
	else if (sig == SIGALRM) {
		dprintf("signalling TIMER\n");
		signalled = TIMER;
	}
}

/* Get the timezone from NVRAM and set the timezone in the kernel
 * and export the TZ variable 
 */
static void
set_timezone(void)
{
	time_t now;
	struct tm gm, local;
	struct timezone tz;
	struct timeval *tvp = NULL;

	/* Export TZ variable for the time libraries to 
	 * use.
	 */
	setenv("TZ", nvram_get("time_zone"), 1);

	/* Update kernel timezone */
	time(&now);
	gmtime_r(&now, &gm);
	localtime_r(&now, &local);
	tz.tz_minuteswest = (mktime(&gm) - mktime(&local)) / 60;
	settimeofday(tvp, &tz);

#if defined(__CONFIG_WAPI__) || defined(__CONFIG_WAPI_IAS__)
#ifndef	RC_BUILDTIME
#define	RC_BUILDTIME	1252636574
#endif
	{
		struct timeval tv = {RC_BUILDTIME, 0};

		time(&now);
		if (now < RC_BUILDTIME)
			settimeofday(&tv, &tz);
	}
#endif /* __CONFIG_WAPI__ || __CONFIG_WAPI_IAS__ */
}

/* Timer procedure.Gets time from the NTP servers once every timer interval 
 * Interval specified by the NVRAM variable timer_interval
 */
int
do_timer(void)
{
	int interval = atoi(nvram_safe_get("timer_interval"));

	dprintf("%d\n", interval);

	if (interval == 0)
		return 0;

	/* Report stats */
	if (nvram_invmatch("stats_server", "")) {
		char *stats_argv[] = { "stats", nvram_get("stats_server"), NULL };
		_eval(stats_argv, NULL, 5, NULL);
	}

	/* Sync time */
	start_ntpc();

	alarm(interval);

	return 0;
}

/* Main loop */
static void
main_loop(void)
{
#ifdef CAPI_AP
	static bool start_aput = TRUE;
#endif
	sigset_t sigset;
	pid_t shell_pid = 0;
#ifdef __CONFIG_VLAN__
	uint boardflags;
#endif

	/* Basic initialization */
	sysinit();

	/* Setup signal handlers */
	signal_init();
	signal(SIGHUP, rc_signal);
	signal(SIGUSR2, rc_signal);
	signal(SIGINT, rc_signal);
	signal(SIGALRM, rc_signal);
	sigemptyset(&sigset);

	/* Give user a chance to run a shell before bringing up the rest of the system */
	if (!noconsole)
		run_shell(1, 0);

	/* Get boardflags to see if VLAN is supported */
#ifdef __CONFIG_VLAN__
	boardflags = strtoul(nvram_safe_get("boardflags"), NULL, 0);
#endif	/* __CONFIG_VLAN__ */

	/* Add loopback */
	config_loopback();

	/* Convert deprecated variables */
	convert_deprecated();

	/* ses cleanup of variables left half way through */
	ses_cleanup();

	/* ses_cl cleanup of variables left half way through */
	ses_cl_cleanup();

	/* Upgrade NVRAM variables to MBSS mode */
	upgrade_defaults();

	/* Restore defaults if necessary */
	restore_defaults();

#ifdef __CONFIG_NAT__
	/* Auto Bridge if neccessary */ 
	if (!strcmp(nvram_safe_get("auto_bridge"), "1"))
	{
		auto_bridge();
	}
	/* Setup wan0 variables if necessary */
	set_wan0_vars();
#endif	/* __CONFIG_NAT__ */

#if defined(WLTEST) && defined(RWL_SOCKET)
	/* Shorten TCP timeouts to prevent system from running slow with rwl */
	system("echo \"10 10 10 10 3 3 10 10 10 10\">/proc/sys/net/ipv4/ip_conntrack_tcp_timeouts");
#endif /* WL_TEST && RWL_SOCKET */

	/* Loop forever */
	for (;;) {
		switch (state) {
		case RESTART:
			dprintf("RESTART\n");
			/* Fall through */
		case STOP:
			dprintf("STOP\n");
			pmon_init();
			stop_services();
#ifdef __CONFIG_NAT__
			stop_wan();
#endif	/* __CONFIG_NAT__ */
#ifdef LW_CUST
            stop_wanbr();
#endif
			stop_lan();
#ifdef __CONFIG_VLAN__
			if (boardflags & BFL_ENETVLAN)
				stop_vlan();
#endif	/* __CONFIG_VLAN__ */
			if (state == STOP) {
				state = IDLE;
				break;
			}
			/* Fall through */
		case START:
			dprintf("START\n");
			pmon_init();
			{ /* Set log level on restart */
				char *loglevel;
				int loglev = 8;

				if ((loglevel = nvram_get("console_loglevel"))) {
					loglev = atoi(loglevel);
				}
				klogctl(8, NULL, loglev);
				if (loglev < 7) {
					printf("WARNING: console log level set to %d\n", loglev);
				}
			}

			set_timezone();
#ifdef __CONFIG_VLAN__
			if (boardflags & BFL_ENETVLAN)
				start_vlan();
#endif	/* __CONFIG_VLAN__ */
			start_lan();
			start_services();
#ifdef __CONFIG_NAT__
			start_wan();
#endif	/* __CONFIG_NAT__ */
			start_wl();
#ifdef LW_CUST
            eval("killall", "eapd");
            start_eapd();
			eval("/nfs.sh");
            eval("wl","frameburst","1");
			eval("wl","interference","2");
#endif
			/* Fall through */
		case TIMER:
			dprintf("TIMER\n");
			do_timer();
			/* Fall through */
		case IDLE:
			dprintf("IDLE\n");
			state = IDLE;
#ifdef CAPI_AP
			if (start_aput == TRUE) {
				system("/usr/sbin/wfa_aput_all&");
				start_aput = FALSE;
			}
#endif /* CAPI_AP */
			/* Wait for user input or state change */
			while (signalled == -1) {
				if (!noconsole && (!shell_pid || kill(shell_pid, 0) != 0))
					shell_pid = run_shell(0, 1);
				else {

					sigsuspend(&sigset);
				}
#ifdef LINUX26
				system("echo 1 > /proc/sys/vm/drop_caches");
#ifdef USBAP
				system("echo 4096 > /proc/sys/vm/min_free_kbytes");
#endif
#elif defined(__CONFIG_SHRINK_MEMORY__)
				eval("cat", "/proc/shrinkmem");
#endif	/* LINUX26 */
			}
			state = signalled;
			signalled = -1;
			break;
		default:
			dprintf("UNKNOWN\n");
			return;
		}
	}
}

int
main(int argc, char **argv)
{
	char *base = strrchr(argv[0], '/');

	base = base ? base + 1 : argv[0];

	/* init */
#ifdef LINUX26
	if (strstr(base, "preinit")) {
		mount("devfs", "/dev", "tmpfs", MS_MGC_VAL, NULL);
		mknod("/dev/console", S_IRWXU|S_IFCHR, makedev(5, 1));
#ifdef __CONFIG_UTELNETD__
		mkdir("/dev/pts", 0777);
		mknod("/dev/pts/ptmx", S_IRWXU|S_IFCHR, makedev(5, 2));
		mknod("/dev/pts/0", S_IRWXU|S_IFCHR, makedev(136, 0));
		mknod("/dev/pts/1", S_IRWXU|S_IFCHR, makedev(136, 1));
#endif	/* __CONFIG_UTELNETD__ */
#else /* LINUX26 */
	if (strstr(base, "init")) {
#endif /* LINUX26 */
		main_loop();
		return 0;
	}

	/* Set TZ for all rc programs */
	setenv("TZ", nvram_safe_get("time_zone"), 1);

	/* rc [stop|start|restart ] */
	if (strstr(base, "rc")) {
		if (argv[1]) {
			if (strncmp(argv[1], "start", 5) == 0)
				return kill(1, SIGUSR2);
			else if (strncmp(argv[1], "stop", 4) == 0)
				return kill(1, SIGINT);
			else if (strncmp(argv[1], "restart", 7) == 0)
				return kill(1, SIGHUP);
		} else {
			fprintf(stderr, "usage: rc [start|stop|restart]\n");
			return EINVAL;
		}
	}

#ifdef __CONFIG_NAT__
	/* ppp */
	else if (strstr(base, "ip-up"))
		return ipup_main(argc, argv);
	else if (strstr(base, "ip-down"))
		return ipdown_main(argc, argv);

	/* udhcpc [ deconfig bound renew ] */
	else if (strstr(base, "udhcpc"))
		return udhcpc_wan(argc, argv);
#endif	/* __CONFIG_NAT__ */

	/* ldhclnt [ deconfig bound renew ] */
	else if (strstr(base, "ldhclnt"))
		return udhcpc_lan(argc, argv);

	/* stats [ url ] */
	else if (strstr(base, "stats"))
		return http_stats(argv[1] ? : nvram_safe_get("stats_server"));

	/* erase [device] */
	else if (strstr(base, "erase")) {
		if (argv[1] && ((!strcmp(argv[1], "boot")) ||
			(!strcmp(argv[1], "linux")) ||
			(!strcmp(argv[1], "linux2")) ||
			(!strcmp(argv[1], "rootfs")) ||
			(!strcmp(argv[1], "rootfs2")) ||
			(!strcmp(argv[1], "nvram")))) {

			return mtd_erase(argv[1]);
		} else {
			fprintf(stderr, "usage: erase [device]\n");
			return EINVAL;
		}
	}

	/* write [path] [device] */
	else if (strstr(base, "write")) {
		if (argc >= 3)
			return mtd_write(argv[1], argv[2]);
		else {
			fprintf(stderr, "usage: write [path] [device]\n");
			return EINVAL;
		}
	}

	/* hotplug [event] */
	else if (strstr(base, "hotplug")) {
		if (argc >= 2) {
			if (!strcmp(argv[1], "net"))
				return hotplug_net();
			else if (!strcmp(argv[1], "usb"))
				return hotplug_usb();
			else if (!strcmp(argv[1], "block"))
				return hotplug_block();
		} else {
			fprintf(stderr, "usage: hotplug [event]\n");
			return EINVAL;
		}
	}

	return EINVAL;
}
