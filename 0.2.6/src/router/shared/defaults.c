/*
 * Router default NVRAM values
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
 * $Id: defaults.c 290116 2011-10-17 05:26:47Z kenlo $
 */

#include <epivers.h>
#include "router_version.h"
#include <typedefs.h>
#include <string.h>
#include <ctype.h>
#include <bcmnvram.h>
#include <wlioctl.h>
#include <stdio.h>
#include <ezc.h>
#include <bcmconfig.h>

#define XSTR(s) STR(s)
#define STR(s) #s

struct nvram_tuple router_defaults[] = {
	/* OS parameters */
	{ "os_name", "", 0 },			 /* OS name string */
	{ "cpu_type","BCM53572+BCM3349",0},
	{ "os_version", ROUTER_VERSION_STR, 0 }, /* OS revision */
	{ "os_date", __DATE__, 0 },		 /* OS date */
	{ "wl_version", EPI_VERSION_STR, 0 },	 /* OS revision */

	/* Version */
	{ "nvram_version", NVRAM_SOFTWARE_VERSION, 0 },

	/* Miscellaneous parameters */
	{ "timer_interval", "3600", 0 },	/* Timer interval in seconds */
	{ "ntp_server", "192.5.41.40 192.5.41.41 133.100.9.2", 0 },		/* NTP server */
	{ "time_zone", "PST8PDT", 0 },		/* Time zone (GNU TZ format) */
	{ "log_level", "0", 0 },		/* Bitmask 0:off 1:denied 2:accepted */
	{ "upnp_enable", "1", 0 },		/* Start UPnP */
#ifdef __CONFIG_DLNA_DMR__
	{ "dlna_dmr_enable", "1", 0 },		/* Start DLNA Renderer */
#endif
#ifdef __CONFIG_DLNA_DMS__
	{ "dlna_dms_enable", "1", 0 },		/* Start DLNA Server */
#endif
	{ "ezc_enable", "1", 0 },		/* Enable EZConfig updates */
	{ "ezc_version", EZC_VERSION_STR, 0 },	/* EZConfig version */
	{ "is_default", "1", 0 },		/* is it default setting: 1:yes 0:no */
	{ "os_server", "", 0 },			/* URL for getting upgrades */
	{ "stats_server", "", 0 },		/* URL for posting stats */
	{ "console_loglevel", "1", 0 },		/* Kernel panics only */

	/* Big switches */
#if defined(__CONFIG_MEDIA_IPTV__)
	{ "router_disable", "1", 0 },		/* lan_proto=static lan_stp=0 wan_proto=disabled */
#else
	{ "router_disable", "0", 0 },		/* lan_proto=static lan_stp=0 wan_proto=disabled */
#endif
	{ "ure_disable", "1", 0 },		/* sets APSTA for radio and puts wirelesss
						 * interfaces in correct lan
						 */
	{ "fw_disable", "0", 0 },		/* Disable firewall (allow new connections from the
						 * WAN)
						 */

	{ "log_ipaddr", "", 0 },		/* syslog recipient */
#ifdef BCMQOS
	{ "wan_mtu",			"1500"			},
#endif
	/* LAN H/W parameters */
	{ "lan_ifname", "", 0 },		/* LAN interface name */
	{ "lan_ifnames", "", 0 },		/* Enslaved LAN interfaces */
	{ "lan_hwnames", "", 0 },		/* LAN driver names (e.g. et0) */
	{ "lan_hwaddr", "", 0 },		/* LAN interface MAC address */

	/* LAN TCP/IP parameters */
	{ "lan_dhcp", "0", 0 },			/* DHCP client [static|dhcp] */
	{ "lan_ipaddr", "192.168.200.1", 0 },	/* LAN IP address */
	{ "lan_netmask", "255.255.255.0", 0 },	/* LAN netmask */
	{ "lan_gateway", "192.168.200.1", 0 },	/* LAN gateway */
	{ "lan_proto", "dhcp", 0 },		/* DHCP server [static|dhcp] */
	{ "lan_wins", "", 0 },			/* x.x.x.x x.x.x.x ... */
	{ "lan_domain", "", 0 },		/* LAN domain name */
	{ "lan_lease", "28800", 0 },		/* LAN lease time in seconds */
	{ "lan_stp", "1", 0 },			/* LAN spanning tree protocol */
	{ "lan_route", "", 0 },			/* Static routes
						 * (ipaddr:netmask:gateway:metric:ifname ...)
						 */
	/* Guest H/W parameters */
	{ "lan1_ifname", "", 0 },		/* LAN interface name */
	{ "lan1_ifnames", "", 0 },		/* Enslaved LAN interfaces */
	{ "lan1_hwnames", "", 0 },		/* LAN driver names (e.g. et0) */
	{ "lan1_hwaddr", "", 0 },		/* LAN interface MAC address */

	/* Guest TCP/IP parameters */
	{ "lan1_dhcp", "0", 0 },			/* DHCP client [static|dhcp] */
	{ "lan1_ipaddr", "192.168.201.1", 0 },	/* LAN IP address */
	{ "lan1_netmask", "255.255.255.0", 0 },	/* LAN netmask */
	{ "lan1_gateway", "192.168.201.1", 0 },	/* LAN gateway */
	{ "lan1_proto", "dhcp", 0 },		/* DHCP server [static|dhcp] */
	{ "lan1_wins", "", 0 },			/* x.x.x.x x.x.x.x ... */
	{ "lan1_domain", "", 0 },		/* LAN domain name */
	{ "lan1_lease", "28800", 0 },		/* LAN lease time in seconds */
	{ "lan1_stp", "1", 0 },			/* LAN spanning tree protocol */
	{ "lan1_route", "", 0 },			/* Static routes
						 * (ipaddr:netmask:gateway:metric:ifname ...)
						 */

#ifdef __CONFIG_NAT__
	/* WAN H/W parameters */
	{ "wan_ifname", "", 0 },		/* WAN interface name */
	{ "wan_ifnames", "", 0 },		/* WAN interface names */
	{ "wan_hwname", "", 0 },		/* WAN driver name (e.g. et1) */
	{ "wan_hwaddr", "", 0 },		/* WAN interface MAC address */

	/* WAN TCP/IP parameters */
	{ "wan_proto", "dhcp", 0 },		/* [static|dhcp|pppoe|disabled] */
	{ "wan_ipaddr", "0.0.0.0", 0 },		/* WAN IP address */
	{ "wan_netmask", "0.0.0.0", 0 },	/* WAN netmask */
	{ "wan_gateway", "0.0.0.0", 0 },	/* WAN gateway */
	{ "wan_dns", "", 0 },			/* x.x.x.x x.x.x.x ... */
	{ "wan_wins", "", 0 },			/* x.x.x.x x.x.x.x ... */
	{ "wan_hostname", "", 0 },		/* WAN hostname */
	{ "wan_domain", "", 0 },		/* WAN domain name */
	{ "wan_lease", "86400", 0 },		/* WAN lease time in seconds */

	/* PPPoE parameters */
	{ "wan_pppoe_ifname", "", 0 },		/* PPPoE enslaved interface */
	{ "wan_pppoe_username", "", 0 },	/* PPP username */
	{ "wan_pppoe_passwd", "", 0 },		/* PPP password */
#ifdef LW_CUST
	{ "wan_pppoe_username", "lwtest", 0 },	/* PPP username */
	{ "wan_pppoe_passwd", "lwtest", 0 },		/* PPP password */
#endif
	{ "wan_pppoe_idletime", "60", 0 },	/* Dial on demand max idle time (seconds) */
	{ "wan_pppoe_keepalive", "0", 0 },	/* Restore link automatically */
	{ "wan_pppoe_demand", "0", 0 },		/* Dial on demand */
	{ "wan_pppoe_mru", "1492", 0 },		/* Negotiate MRU to this value */
	{ "wan_pppoe_mtu", "1492", 0 },		/* Negotiate MTU to the smaller of this value or
						 * the peer MRU
						 */
	{ "wan_pppoe_service", "", 0 },		/* PPPoE service name */
	{ "wan_pppoe_ac", "", 0 },		/* PPPoE access concentrator name */
	/* Misc WAN parameters */
	{ "wan_desc", "", 0 },			/* WAN connection description */
	{ "wan_route", "", 0 },			/* Static routes
						 * (ipaddr:netmask:gateway:metric:ifname ...)
						 */
	{ "wan_primary", "0", 0 },		/* Primary wan connection */

	{ "wan_unit", "0", 0 },			/* Last configured connection */

#ifdef LW_CUST
	{ "wan_dhcpoption60", "gxcatv.nat.v2.0.0", 0 },	/* wan dhcp60 options */
#endif

	/* Filters */
	{ "filter_maclist", "", 0 },		/* xx:xx:xx:xx:xx:xx ... */
	{ "filter_macmode", "deny", 0 },	/* "allow" only, "deny" only, or "disabled"
						 * (allow all)
						 */
	{ "filter_client0", "", 0 },		/* [lan_ipaddr0-lan_ipaddr1|*]:lan_port0-lan_port1,
						 * proto,enable,day_start-day_end,sec_start-sec_end,
						 * desc
						 */
	{ "nat_type", "sym", 0 },               /* sym: Symmetric NAT, cone: Cone NAT */
	/* Port forwards */
	{ "dmz_ipaddr", "", 0 },		/* x.x.x.x (equivalent to 0-60999>dmz_ipaddr:
						 * 0-60999)
						 */
	{ "forward_port0", "", 0 },		/* wan_port0-wan_port1>lan_ipaddr:
						 * lan_port0-lan_port1[:,]proto[:,]enable[:,]desc
						 */
	{ "autofw_port0", "", 0 },		/* out_proto:out_port,in_proto:in_port0-in_port1>
						 * to_port0-to_port1,enable,desc
						 */
#ifdef BCMQOS
	{ "qos_orates",	"80-100,10-100,5-100,3-100,2-95,0-0,0-0,0-0,0-0,0-0", 0 },
	{ "qos_irates",	"0,0,0,0,0,0,0,0,0,0", 0 },
	{ "qos_enable",			"0"				},
	{ "qos_method",			"0"				},
	{ "qos_sticky",			"1"				},
	{ "qos_ack",			"1"				},
	{ "qos_icmp",			"0"				},
	{ "qos_reset",			"0"				},
	{ "qos_obw",			"384"			},
	{ "qos_ibw",			"1500"			},
	{ "qos_orules",			"" },
	{ "qos_burst0",			""				},
	{ "qos_burst1",			""				},
	{ "qos_default",		"3"				},
#endif /* BCMQOS */
	/* DHCP server parameters */
	{ "dhcp_start", "192.168.200.100", 0 },	/* First assignable DHCP address */
	{ "dhcp_end", "192.168.200.200", 0 },	/* Last assignable DHCP address */
	{ "dhcp1_start", "192.168.201.100", 0 },	/* First assignable DHCP address */
	{ "dhcp1_end", "192.168.201.200", 0 },	/* Last assignable DHCP address */
	{ "dhcp_domain", "wan", 0 },		/* Use WAN domain name first if available (wan|lan)
						 */
	{ "dhcp_wins", "wan", 0 },		/* Use WAN WINS first if available (wan|lan) */
#endif	/* __CONFIG_NAT__ */

	/* Web server parameters */
#ifdef LW_CUST
	{ "http_username", "admin", 0 },		/* Username */
	{ "http_passwd", "123456", 0 },		/* Password */
#else
	{ "http_username", "", 0 },		/* Username */
	{ "http_passwd", "admin", 0 },		/* Password */
#endif
	{ "http_wanport", "", 0 },		/* WAN port to listen on */
	{ "http_lanport", "80", 0 },		/* LAN port to listen on */

	/* Wireless parameters */
	{ "wl_ifname", "", 0 },			/* Interface name */
	{ "wl_hwaddr", "", 0 },			/* MAC address */
#ifdef __CONFIG_MEDIA_IPTV__
	{ "wl_phytype", "a", 0 },		/* Current wireless band ("a" (5 GHz),
						 * "b" (2.4 GHz), or "g" (2.4 GHz)) 
						*/
#else
	{ "wl_phytype", "b", 0 },		/* Current wireless band ("a" (5 GHz),
						 * "b" (2.4 GHz), or "g" (2.4 GHz)) 
						*/
#endif
	{ "wl_corerev", "", 0 },		/* Current core revision */
	{ "wl_phytypes", "", 0 },		/* List of supported wireless bands (e.g. "ga") */
	{ "wl_radioids", "", 0 },		/* List of radio IDs */
	{ "wl_ssid", "HiTV", 0 },		/* Service set ID (network name) */
	{ "wl_bss_enabled", "1", 0 },		/* Service set ID (network name) */
						/* See "default_get" below. */
#ifdef LW_CUST
	{ "wl_country_code", "CN", 1 },		/* Country Code (default obtained from driver) */
#else
	{ "wl_country_code", "", 1 },		/* Country Code (default obtained from driver) */
#endif
	{ "wl_radio", "1", 0 },			/* Enable (1) or disable (0) radio */
	{ "wl_closed", "0", 0 },		/* Closed (hidden) network */
	{ "wl_ap_isolate", "0", 0 },            /* AP isolate mode */
#ifdef __CONFIG_MEDIA_IPTV__
	{ "wl_wmf_bss_enable", "1", 0 },	/* WMF Enable for IPTV Media */
#else
	{ "wl_wmf_bss_enable", "0", 0 },	/* WMF Enable/Disable */
#endif
	{ "wl_mcast_regen_bss_enable", "1", 0 },	/* MCAST REGEN Enable/Disable */
	{ "wl_rxchain_pwrsave_enable", "1", 0 },	/* Rxchain powersave enable */
	{ "wl_rxchain_pwrsave_quiet_time", "1800", 0 },	/* Quiet time for power save */
	{ "wl_rxchain_pwrsave_pps", "10", 0 },	/* Packets per second threshold for power save */
	{ "wl_rxchain_pwrsave_stas_assoc_check", "0", 0 }, /* STAs associated before powersave */
	{ "wl_radio_pwrsave_enable", "0", 0 },	/* Radio powersave enable */
	{ "wl_radio_pwrsave_quiet_time", "1800", 0 },	/* Quiet time for power save */
	{ "wl_radio_pwrsave_pps", "10", 0 },	/* Packets per second threshold for power save */
	{ "wl_radio_pwrsave_level", "0", 0 },	/* Radio power save level */
	{ "wl_radio_pwrsave_stas_assoc_check", "0", 0 }, /* STAs associated before powersave */
	{ "wl_mode", "ap", 0 },			/* AP mode (ap|sta|wds) */
	{ "wl_lazywds", "0", 0 },		/* Enable "lazy" WDS mode (0|1) */
	{ "wl_wds", "", 0 },			/* xx:xx:xx:xx:xx:xx ... */
	{ "wl_wds_timeout", "1", 0 },		/* WDS link detection interval defualt 1 sec */
	{ "wl_wep", "disabled", 0 },		/* WEP data encryption (enabled|disabled) */
	{ "wl_auth", "0", 0 },			/* Shared key authentication optional (0) or
						 * required (1)
						 */
	{ "wl_key", "1", 0 },			/* Current WEP key */
	{ "wl_key1", "", 0 },			/* 5/13 char ASCII or 10/26 char hex */
	{ "wl_key2", "", 0 },			/* 5/13 char ASCII or 10/26 char hex */
	{ "wl_key3", "", 0 },			/* 5/13 char ASCII or 10/26 char hex */
	{ "wl_key4", "", 0 },			/* 5/13 char ASCII or 10/26 char hex */
	{ "wl_maclist", "", 0 },		/* xx:xx:xx:xx:xx:xx ... */
	{ "wl_macmode", "disabled", 0 },	/* "allow" only, "deny" only, or "disabled"
						 * (allow all)
						 */
#ifdef __CONFIG_MEDIA_IPTV__
	{ "wl_channel", "157", 0 },		/* Channel number */
#else
	{ "wl_channel", "11", 0 },		/* Channel number */
#endif
	{ "wl_reg_mode", "off", 0 },		/* Regulatory: 802.11H(h)/802.11D(d)/off(off) */
	{ "wl_dfs_preism", "60", 0 },		/* 802.11H pre network CAC time */
	{ "wl_dfs_postism", "60", 0 },		/* 802.11H In Service Monitoring CAC time */
	/* Radar thrs params format: version thresh0_20 thresh1_20 thresh0_40 thresh1_40 */
	{ "wl_radarthrs", "0 0x6a8 0x6c8 0x6ac 0x6c7", 0 },
	{ "wl_rate", "0", 0 },			/* Rate (bps, 0 for auto) */
#ifdef __CONFIG_MEDIA_IPTV__
	{ "wl_mrate", "12000000", 0 },		/* Mcast Rate (bps, 0 for auto) */
#else
	{ "wl_mrate", "0", 0 },			/* Mcast Rate (bps, 0 for auto) */
#endif
	{ "wl_rateset", "default", 0 },		/* "default" or "all" or "12" */
	{ "wl_frag", "2346", 0 },		/* Fragmentation threshold */
	{ "wl_rts", "2347", 0 },		/* RTS threshold */
	{ "wl_dtim", "3", 0 },			/* DTIM period */
	{ "wl_bcn", "100", 0 },			/* Beacon interval */
	{ "wl_bcn_rotate", "1", 0 },		/* Beacon rotation */
	{ "wl_plcphdr", "long", 0 },		/* 802.11b PLCP preamble type */
	{ "wl_gmode", XSTR(GMODE_AUTO), 0 },	/* 54g mode */
	{ "wl_gmode_protection", "auto", 0 },	/* 802.11g RTS/CTS protection (off|auto) */
	{ "wl_frameburst", "off", 0 },		/* BRCM Frambursting mode (off|on) */
	{ "wl_wme", "auto", 0 },		/* WME mode (off|on|auto) */
	{ "wl_wme_bss_disable", "0", 0 },	/* WME BSS disable advertising (off|on) */
	{ "wl_antdiv", "-1", 0 },		/* Antenna Diversity (-1|0|1|3) */
	{ "wl_infra", "1", 0 },			/* Network Type (BSS/IBSS) */
#ifdef LW_CUST
	{ "wl_nbw_cap", "0", 0},		/* BW Cap; def 20inB and 40inA */
#else
	{ "wl_nbw_cap", "2", 0},		/* BW Cap; def 20inB and 40inA */
#endif
	{ "wl_nctrlsb", "none", 0},		/* N-CTRL SB */
	{ "wl_nband", "2", 0},			/* N-BAND */
	{ "wl_nmcsidx", "-1", 0},		/* MCS Index for N - rate */
	{ "wl_nmode", "-1", 0},			/* N-mode */
	{ "wl_rifs_advert", "auto", 0},		/* RIFS mode advertisement */
	{ "wl_nreqd", "0", 0},			/* Require 802.11n support */
	{ "wl_vlan_prio_mode", "off", 0},	/* VLAN Priority support */
	{ "wl_leddc", "0x640000", 0},		/* 100% duty cycle for LED on router */
	{ "wl_rxstreams", "0", 0},              /* 802.11n Rx Streams, 0 is invalid, WLCONF will
						 * change it to a radio appropriate default
						 */
	{ "wl_txstreams", "0", 0},              /* 802.11n Tx Streams 0, 0 is invalid, WLCONF will
						 * change it to a radio appropriate default
						 */
	{ "wl_stbc_tx", "auto", 0 },		/* Default STBC TX setting */
	{ "wl_stbc_rx", "1", 0 },		/* Default STBC RX setting */
	{ "wl_ampdu", "auto", 0 },		/* Default AMPDU setting */
	/* Default AMPDU retry limit per-tid setting */
	{ "wl_ampdu_rtylimit_tid", "5 5 5 5 5 5 5 5", 0 },
	/* Default AMPDU regular rate retry limit per-tid setting */
	{ "wl_ampdu_rr_rtylimit_tid", "2 2 2 2 2 2 2 2", 0 },
	{ "wl_amsdu", "auto", 0 },		/* Default AMSDU setting */
	{ "wl_obss_coex", "1", 0 },		/* Default OBSS Coexistence setting - OFF */

	/* WPA parameters */
	{ "wl_auth_mode", "none", 0 },		/* Network authentication mode (radius|none) */
#ifdef LW_CUST
	{ "wl_wpa_psk", "123123123", 0 },		/* WPA pre-shared key */
#else
	{ "wl_wpa_psk", "", 0 },		/* WPA pre-shared key */
#endif
	{ "wl_wpa_gtk_rekey", "0", 0 },		/* GTK rotation interval */
	{ "wl_radius_ipaddr", "", 0 },		/* RADIUS server IP address */
	{ "wl_radius_key", "", 0 },		/* RADIUS shared secret */
	{ "wl_radius_port", "1812", 0 },	/* RADIUS server UDP port */
	{ "wl_crypto", "tkip+aes", 0 },		/* WPA data encryption */
	{ "wl_net_reauth", "36000", 0 },	/* Network Re-auth/PMK caching duration */
#ifdef LW_CUST
	{ "wl_akm", "", 0 },			/* WPA akm list */
#else
	{ "wl_akm", "psk psk2", 0 },			/* WPA akm list */
#endif

	/* SES parameters */
	{ "ses_enable", "1", 0 },		/* enable ses */
	{ "ses_event", "2", 0 },		/* initial ses event */
	{ "ses_wds_mode", "1", 0 },		/* enabled if ses cfgd */
	/* SES client parameters */
	{ "ses_cl_enable", "1", 0 },		/* enable ses */
	{ "ses_cl_event", "0", 0 },		/* initial ses event */
#ifdef __CONFIG_WPS__
	/* WSC parameters */
	{ "wps_version2", "enabled", 0 },	 /* Must specified before other wps variables */
	{ "wl_wps_mode", "disabled", 0 }, /* enabled wps */
	{ "wps_mode", "disabled", 0 },	 /* enabled wps */
	{ "wl_wps_config_state", "0", 0 },	/* config state unconfiged */
	{ "wps_modelname", "Broadcom", 0 },
	{ "wps_mfstring", "Broadcom", 0 },
	{ "wps_device_name", "BroadcomAP", 0 },
	{ "wl_wps_reg", "enabled", 0 },
	{ "wps_sta_pin", "00000000", 0 },
	{ "wps_modelnum", "123456", 0 },
	{ "wps_timeout_enable", "0", 0 },
	/* Allow or Deny Wireless External Registrar get or configure AP security settings */
	{ "wps_wer_mode", "allow", 0 },

#ifdef LW_CUST
	{ "lan_wps_oob", "disabled", 0 },	/* OOB state */
#else
	{ "lan_wps_oob", "enabled", 0 },	/* OOB state */
#endif
	{ "lan_wps_reg", "enabled", 0 },	/* For DTM 1.4 test */

	{ "lan1_wps_oob", "enabled", 0 },
	{ "lan1_wps_reg", "enabled", 0 },
#endif /* __CONFIG_WPS__ */
#ifdef __CONFIG_WFI__
	{ "wl_wfi_enable", "0", 0 },	/* 0: disable, 1: enable WifiInvite */
	{ "wl_wfi_pinmode", "0", 0 },	/* 0: auto pin, 1: manual pin */
#endif /* __CONFIG_WFI__ */
#ifdef __CONFIG_WAPI_IAS__
	/* WAPI parameters */
	{ "wl_wai_cert_name", "", 0 },		/* AP certificate name */
	{ "wl_wai_cert_index", "1", 0 },	/* AP certificate index. 1:X.509, 2:GBW */
	{ "wl_wai_cert_status", "0", 0 },	/* AP certificate status */
	{ "wl_wai_as_ip", "", 0 },		/* ASU server IP address */
	{ "wl_wai_as_port", "3810", 0 },	/* ASU server UDP port */
#endif /* __CONFIG_WAPI_IAS__ */
	/* WME parameters (cwmin cwmax aifsn txop_b txop_ag adm_control oldest_first) */
	/* EDCA parameters for STA */
	{ "wl_wme_sta_be", "15 1023 3 0 0 off off", 0 },	/* WME STA AC_BE parameters */
	{ "wl_wme_sta_bk", "15 1023 7 0 0 off off", 0 },	/* WME STA AC_BK parameters */
	{ "wl_wme_sta_vi", "7 15 2 6016 3008 off off", 0 },	/* WME STA AC_VI parameters */
	{ "wl_wme_sta_vo", "3 7 2 3264 1504 off off", 0 },	/* WME STA AC_VO parameters */

	/* EDCA parameters for AP */
	{ "wl_wme_ap_be", "15 63 3 0 0 off off", 0 },		/* WME AP AC_BE parameters */
	{ "wl_wme_ap_bk", "15 1023 7 0 0 off off", 0 },		/* WME AP AC_BK parameters */
	{ "wl_wme_ap_vi", "7 15 1 6016 3008 off off", 0 },	/* WME AP AC_VI parameters */
	{ "wl_wme_ap_vo", "3 7 1 3264 1504 off off", 0 },	/* WME AP AC_VO parameters */

	{ "wl_wme_no_ack", "off", 0},		/* WME No-Acknowledgment mode */
	{ "wl_wme_apsd", "on", 0},		/* WME APSD mode */

	/* Per AC Tx parameters */
	{ "wl_wme_txp_be", "7 3 4 2 0", 0 },	/* WME AC_BE Tx parameters */
	{ "wl_wme_txp_bk", "7 3 4 2 0", 0 },	/* WME AC_BK Tx parameters */
	{ "wl_wme_txp_vi", "7 3 4 2 0", 0 },	/* WME AC_VI Tx parameters */
	{ "wl_wme_txp_vo", "7 3 4 2 0", 0 },	/* WME AC_VO Tx parameters */

	{ "wl_maxassoc", "128", 0},		/* Max associations driver could support */
	{ "wl_bss_maxassoc", "128", 0},		/* Max associations driver could support */

	{ "wl_unit", "0", 0 },			/* Last configured interface */
	{ "wl_sta_retry_time", "5", 0 }, /* Seconds between association attempts */
#ifdef BCMDBG
	{ "wl_nas_dbg", "0", 0 }, /* Enable/Disable NAS Debugging messages */
#endif

#ifdef __CONFIG_EMF__
	/* EMF defaults */
	{ "emf_entry", "", 0 },			/* Static MFDB entry (mgrp:if) */
	{ "emf_uffp_entry", "", 0 },		/* Unreg frames forwarding ports */
	{ "emf_rtport_entry", "", 0 },		/* IGMP frames forwarding ports */
#ifdef __CONFIG_MEDIA_IPTV__
	{ "emf_enable", "1", 0 },		/* Enable EMF by default */
#else
	{ "emf_enable", "0", 0 },		/* Disable EMF by default */
#endif /* __CONFIG_MEDIA_IPTV */
#endif /* __CONFIG_EMF__ */
#ifdef __CONFIG_IPV6__
	{ "lan_ipv6_mode", "3", 0 },		/* 0=disable 1=6to4 2=native 3=6to4+native! */
	{ "lan_ipv6_dns", "", 0  },
	{ "lan_ipv6_6to4id", "0", 0  }, /* 0~65535 */
	{ "lan_ipv6_prefix", "2001:db8:1:0::/64", 0  },
	{ "wan_ipv6_prefix", "2001:db0:1:0::/64", 0  },
#endif /* __CONFIG_IPV6__ */
#ifdef __CONFIG_NETBOOT__
	{ "netboot_url", "", 0 },		/* netboot url */
	{ "netboot_username", "", 0 },	/* netboot username */
	{ "netboot_passwd", "", 0 },	/* netboor password */
#endif /* __CONFIG_NETBOOT__ */
	/* Restore defaults */
	{ "restore_defaults", "0", 0 },		/* Set to 0 to not restore defaults on boot */
#ifdef __CONFIG_EXTACS__
	{ "acs_ifnames", "", 0  },
#endif
#ifdef __CONFIG_SAMBA__
	{ "samba_mode", "", 0  },
	{ "samba_passwd", "", 0  },
#endif
#ifdef __CONFIG_WL_ACI__
	{ "aci_daemon", "up", 0 },              /* Enable (up) or disable (down) ACI mitigation */
	{ "aci_glitch_threshold", "3000", 0 },  /* Glitch counts/sec channel switch threshold */

	/* Channels to unconditionally exclude from use */
	{ "aci_excluded_channels", "140, 124, 116, 100, 64, 56, 48, 46, 42, 40, 38, 34", 0 },

	{ "aci_preferred_channels", " ", 0 },   /* Channels to prefer (none) */
	{ "aci_auto_channel", "on", 0 },        /* Turn ACI channel changing on/off */
	{ "aci_info_prints", "off", 0 },        /* Turn ACI informational prints on/off */
	{ "aci_debug_prints", "off", 0 },       /* Turn ACI debug prints on/off */
	{ "aci_scan_sleep_secs", "4", 0 },      /* Seconds to sleep between scans */
	{ "aci_def_ap_ipaddr", "192.168.200.1", 0 },   /* Default AP (LAN) address used by station */
	{ "aci_prefer_dfs", "true", 0 },        /* Try DFS channels first when searching */
	{ "aci_dfs_scan_type", "passive", 0 },   /* Do passive scan when home is DFS channel */
	{ "aci_exclude_dfs", "false", 0 },       /* Use DFS channels True, False */

	/*  After initial channel, can ACI re-enter DFS channels? (T/F) */
	{ "aci_reuse_dfs", "false", 0 },

	/* PER threshold in percentage to allow channel change */
	{ "aci_per_threshold", "0.5", 0 },

	/* PDR threshold in percentage to allow channel change */
	{ "aci_pdr_threshold", "20", 0 },

	/* How many scans to calculate one PER sample */
	{ "aci_per_scans_per_sample", "2", 0 },

	/* Minimum number of PER samples exceeding threshold to allow channel change */
	{ "aci_per_min_samples", "2", 0 },

	/* Trigger channel switch when PER exceeding threshold */
	{ "aci_per_trig_chan_switch", "true", 0 },

	/* Number of co-channel APs allowed for a channel to be considered usable */
	{ "aci_co_channel_threshold", "2", 0 },

	/* Minimum channel scans to complete before ACI can change channel */
	{ "aci_min_channel_scans", "5", 0 },
#endif /* __CONFIG_WL_ACI__ */
	{ "wl_plc", "0", 0 },			/* Enable/Disable PLC failover */
#ifdef LW_CUST
    {"wl0_vifs", "wl0.1", 0},
    {"wl0.1_unit", "0.1", 0},
    {"wl0.1_wps_mode", "disabled", 0},
    {"wl0.1_sta_retry_time", "5", 0},
    {"wl0.1_wpa_gtk_rekey", "0", 0},
    {"wl0.1_radius_port", "1812", 0},
    {"wl0.1_net_reauth", "36000", 0},
    {"wl0.1_crypto", "tkip+aes", 0},
    {"wl0.1_wep", "disabled", 0},
    {"wl0.1_auth_mode", "none", 0},
    {"wl0.1_auth", "0", 0},
    {"wl0.1_key4", "", 0},
    {"wl0.1_key3", "", 0},
    {"wl0.1_key2", "", 0},
    {"wl0.1_key1", "", 0},
    {"wl0.1_key", "1", 0},
    {"wl0.1_radio", "1", 0},
    {"wl0.1_wfi_pinmode", "0", 0},
    {"wl0.1_wfi_enable", "0", 0},
    {"wl0.1_wme_bss_disable", "0", 0},
    {"wl0.1_bss_maxassoc", "128", 0},
    {"wl0.1_infra", "1", 0},
    {"wl0.1_mode", "ap", 0},
    {"wl0.1_macmode", "disabled", 0},
    {"wl0.1_wmf_bss_enable", "0", 0},
    {"wl0.1_ap_isolate", "0", 0},
    {"wl0.1_closed", "0", 0},
    {"wl0.1_bridge", "", 0},
    {"wl0.1_ssid", "HiTV", 0},
    {"wl0.1_bss_enabled", "0", 0},
    {"wl0_ssidheader", "HiTV_", 0},
    {"wl0_ssidheader_changable", "0", 0},
#endif
	{ 0, 0, 0 }
};

/* Translates from, for example, wl0_ (or wl0.1_) to wl_. */
/* Only single digits are currently supported */

static void
fix_name(const char *name, char *fixed_name)
{
	char *pSuffix = NULL;

	/* Translate prefix wlx_ and wlx.y_ to wl_ */
	/* Expected inputs are: wld_root, wld.d_root, wld.dd_root
	 * We accept: wld + '_' anywhere
	 */
	pSuffix = strchr(name, '_');

	if ((strncmp(name, "wl", 2) == 0) && isdigit(name[2]) && (pSuffix != NULL)) {
		strcpy(fixed_name, "wl");
		strcpy(&fixed_name[2], pSuffix);
		return;
	}

	/* No match with above rules: default to input name */
	strcpy(fixed_name, name);
}


/* 
 * Find nvram param name; return pointer which should be treated as const
 * return NULL if not found.
 *
 * NOTE:  This routine special-cases the variable wl_bss_enabled.  It will
 * return the normal default value if asked for wl_ or wl0_.  But it will
 * return 0 if asked for a virtual BSS reference like wl0.1_.
 */
char *
nvram_default_get(const char *name)
{
	int idx;
	char fixed_name[NVRAM_MAX_VALUE_LEN];

	fix_name(name, fixed_name);
	if (strcmp(fixed_name, "wl_bss_enabled") == 0) {
		if (name[3] == '.' || name[4] == '.') { /* Virtual interface */
			return "0";
		}
	}

	for (idx = 0; router_defaults[idx].name != NULL; idx++) {
		if (strcmp(router_defaults[idx].name, fixed_name) == 0) {
			return router_defaults[idx].value;
		}
	}

	return NULL;
}
