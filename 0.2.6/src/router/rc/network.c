/*
 * Network services
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: network.c 264979 2011-06-08 01:37:33Z cylee $
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <signal.h>

typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

#include <linux/sockios.h>
#include <linux/types.h>
#include <linux/ethtool.h>

#include <bcmnvram.h>
#include <netconf.h>
#include <shutils.h>
#include <wlutils.h>
#include <nvparse.h>
#include <rc.h>
#include <bcmutils.h>
#include <etioctl.h>
#include <bcmparams.h>

bool emf_enabled = FALSE;

static int
add_routes(char *prefix, char *var, char *ifname)
{
	char word[80], *next;
	char *ipaddr, *netmask, *gateway, *metric;
	char tmp[100];
    
	foreach(word, nvram_safe_get(strcat_r(prefix, var, tmp)), next) {
		dprintf("add %s\n", word);
		
		netmask = word;
		ipaddr = strsep(&netmask, ":");
		if (!ipaddr || !netmask)
			continue;
		gateway = netmask;
		netmask = strsep(&gateway, ":");
		if (!netmask || !gateway)
			continue;
		metric = gateway;
		gateway = strsep(&metric, ":");
		if (!gateway || !metric)
			continue;

		dprintf("add %s\n", ifname);
		
		route_add(ifname, atoi(metric) + 1, ipaddr, gateway, netmask);
	}

	return 0;
}

static int
del_routes(char *prefix, char *var, char *ifname)
{
	char word[80], *next;
	char *ipaddr, *netmask, *gateway, *metric;
	char tmp[100];
	
	foreach(word, nvram_safe_get(strcat_r(prefix, var, tmp)), next) {
		dprintf("add %s\n", word);
		
		netmask = word;
		ipaddr = strsep(&netmask, ":");
		if (!ipaddr || !netmask)
			continue;
		gateway = netmask;
		netmask = strsep(&gateway, ":");
		if (!netmask || !gateway)
			continue;
		metric = gateway;
		gateway = strsep(&metric, ":");
		if (!gateway || !metric)
			continue;
		
		dprintf("add %s\n", ifname);
		
		route_del(ifname, atoi(metric) + 1, ipaddr, gateway, netmask);
	}

	return 0;
}

static int
add_lan_routes(char *lan_ifname)
{
	return add_routes("lan_", "route", lan_ifname);
}

static int
del_lan_routes(char *lan_ifname)
{
	return del_routes("lan_", "route", lan_ifname);
}

/* Set initial QoS mode for all et interfaces that are up. */

static void
set_et_qos_mode(void)
{
	int i, s, qos;
	struct ifreq ifr;
	struct ethtool_drvinfo info;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return;

	qos = (strcmp(nvram_safe_get("wl_wme"), "off") != 0);

	for (i = 1; i <= DEV_NUMIFS; i ++) {
		ifr.ifr_ifindex = i;
		if (ioctl(s, SIOCGIFNAME, &ifr))
			continue;
		if (ioctl(s, SIOCGIFHWADDR, &ifr))
			continue;
		if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
			continue;
		if (ioctl(s, SIOCGIFFLAGS, &ifr))
			continue;
		if (!(ifr.ifr_flags & IFF_UP))
			continue;
		/* Set QoS for et & bcm57xx devices */
		memset(&info, 0, sizeof(info));
		info.cmd = ETHTOOL_GDRVINFO;
		ifr.ifr_data = (caddr_t)&info;
		if (ioctl(s, SIOCETHTOOL, &ifr) < 0)
			continue;
		if ((strncmp(info.driver, "et", 2) != 0) &&
		    (strncmp(info.driver, "bcm57", 5) != 0))
			continue;
		ifr.ifr_data = (caddr_t)&qos;
		ioctl(s, SIOCSETCQOS, &ifr);
	}

	close(s);
}

/*
 * Carry out a socket request including openning and closing the socket
 * Return -1 if failed to open socket (and perror); otherwise return
 * result of ioctl
 */
static int
soc_req(const char *name, int action, struct ifreq *ifr)
{
	int s;
	int rv = 0;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("socket");
		return -1;
	}
	strncpy(ifr->ifr_name, name, IFNAMSIZ);
	rv = ioctl(s, action, ifr);
	close(s);

	return rv;
}

/* Check NVRam to see if "name" is explicitly enabled */
static inline int
wl_vif_enabled(const char *name, char *tmp)
{
	return (atoi(nvram_safe_get(strcat_r(name,"_bss_enabled",tmp))));
}

/* Set the HW address for interface "name" if present in NVRam */
static void
wl_vif_hwaddr_set(const char *name)
{
	int rc;
	char *ea;
	char hwaddr[20];
	struct ifreq ifr;

	snprintf(hwaddr, sizeof(hwaddr), "%s_hwaddr", name);
	ea = nvram_get(hwaddr);
	if (ea == NULL) {
		fprintf(stderr, "NET: No hw addr found for %s\n", name);
		return;
	}

	fprintf(stderr, "NET: Setting %s hw addr to %s\n", name, ea);
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	ether_atoe(ea, (unsigned char *)ifr.ifr_hwaddr.sa_data);
	if ((rc = soc_req(name, SIOCSIFHWADDR, &ifr)) < 0) {
		fprintf(stderr, "NET: Error setting hw for %s; returned %d\n", name, rc);
	}
}

#ifdef __CONFIG_EMF__
void
emf_mfdb_update(char *lan_ifname, char *lan_port_ifname, bool add)
{
	char word[256], *next;
	char *mgrp, *ifname;

	/* Add/Delete MFDB entries corresponding to new interface */
	foreach (word, nvram_safe_get("emf_entry"), next) {
		ifname = word;
		mgrp = strsep(&ifname, ":");

		if ((mgrp == 0) || (ifname == 0))
			continue;

		/* Add/Delete MFDB entry using the group addr and interface */
		if (strcmp(lan_port_ifname, ifname) == 0) {
			eval("emf", ((add) ? "add" : "del"),
			     "mfdb", lan_ifname, mgrp, ifname);
		}
	}

	return;
}

void
emf_uffp_update(char *lan_ifname, char *lan_port_ifname, bool add)
{
	char word[256], *next;
	char *ifname;

	/* Add/Delete UFFP entries corresponding to new interface */
	foreach (word, nvram_safe_get("emf_uffp_entry"), next) {
		ifname = word;

		if (ifname == 0)
			continue;

		/* Add/Delete UFFP entry for the interface */
		if (strcmp(lan_port_ifname, ifname) == 0) {
			eval("emf", ((add) ? "add" : "del"),
			     "uffp", lan_ifname, ifname);
		}
	}

	return;
}

void
emf_rtport_update(char *lan_ifname, char *lan_port_ifname, bool add)
{
	char word[256], *next;
	char *ifname;

	/* Add/Delete RTPORT entries corresponding to new interface */
	foreach (word, nvram_safe_get("emf_rtport_entry"), next) {
		ifname = word;

		if (ifname == 0)
			continue;

		/* Add/Delete RTPORT entry for the interface */
		if (strcmp(lan_port_ifname, ifname) == 0) {
			eval("emf", ((add) ? "add" : "del"),
			     "rtport", lan_ifname, ifname);
		}
	}

	return;
}

void
start_emf(char *lan_ifname)
{
	char word[256], *next;
	char *mgrp, *ifname;

	if (!nvram_match("emf_enable", "1"))
		return;

	/* Start EMF */
	eval("emf", "start", lan_ifname);

	/* Add the static MFDB entries */
	foreach (word, nvram_safe_get("emf_entry"), next) {
		ifname = word;
		mgrp = strsep(&ifname, ":");

		if ((mgrp == 0) || (ifname == 0))
			continue;

		/* Add MFDB entry using the group addr and interface */
		eval("emf", "add", "mfdb", lan_ifname, mgrp, ifname);
	}

	/* Add the UFFP entries */
	foreach (word, nvram_safe_get("emf_uffp_entry"), next) {
		ifname = word;
		if (ifname == 0)
			continue;

		/* Add UFFP entry for the interface */
		eval("emf", "add", "uffp", lan_ifname, ifname);
	}

	/* Add the RTPORT entries */
	foreach (word, nvram_safe_get("emf_rtport_entry"), next) {
		ifname = word;
		if (ifname == 0)
			continue;

		/* Add RTPORT entry for the interface */
		eval("emf", "add", "rtport", lan_ifname, ifname);
	}

	return;
}

void
load_emf(void)
{
	/* Load the EMF & IGMP Snooper modules */
	eval("insmod", "emf");
	eval("insmod", "igs");

	emf_enabled = TRUE;
	
	return;
}

void
unload_emf(void)
{
	if (!emf_enabled)
		return;

	/* Unload the EMF & IGMP Snooper modules */
	eval("rmmod", "igs");
	eval("rmmod", "emf");

	emf_enabled = FALSE;

	return;
}
#endif /* __CONFIG_EMF__ */

void
start_lan(void)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char name[80], *next;
	char tmp[100];
	int i, s;
	struct ifreq ifr;
	char buf[255],*ptr;
	char lan_stp[10];
	char *lan_ifnames;
	char lan_dhcp[10];
	char lan_ipaddr[15];
	char lan_netmask[15];
	char lan_hwaddr[15];
	char hwaddr[ETHER_ADDR_LEN];

	/* The NVRAM variable lan_ifnames contains all the available interfaces. 
	 * This is used to build the unbridged interface list. Once the unbridged list
	 * is built lan_interfaces is rebuilt with only the interfaces in the bridge
	 */
	 
	dprintf("%s\n", lan_ifname);

	/* Create links */
	symlink("/sbin/rc", "/tmp/ldhclnt");
	
	
	nvram_unset("unbridged_ifnames");
	nvram_unset("br0_ifnames");
	nvram_unset("br1_ifnames");

#ifdef __CONFIG_EXTACS__
	nvram_unset("acs_ifnames");
#endif
	/* If we're a travel router... then we need to make sure we get
		 the primary wireless interface up before trying to attach slave
		 interface(s) to the bridge */
	if(nvram_match("ure_disable", "0") && nvram_match("router_disable", "0"))
	{
		eval("wlconf", nvram_get("wan0_ifname"), "up");
	}

	if (nvram_match("ses_bridge_disable", "1")) {
		/* dont bring up the bridge; only the individual interfaces */
		foreach(name, nvram_safe_get("lan_ifnames"), next) {
			/* Bring up interface */
			ifconfig(name, IFUP, NULL, NULL);
			/* if wl */
			eval("wlconf", name, "up");
		}
	} else {

 	/* Bring up bridged interfaces */
	for(i=0; i < MAX_NO_BRIDGE; i++) {
		if(!i) {
			lan_ifname = nvram_safe_get("lan_ifname");
			snprintf(lan_stp, sizeof(lan_stp), "lan_stp" );
			snprintf(lan_dhcp, sizeof(lan_dhcp), "lan_dhcp" );
			snprintf(lan_ipaddr, sizeof(lan_ipaddr), "lan_ipaddr" );
			snprintf(lan_hwaddr, sizeof(lan_hwaddr), "lan_hwaddr" );
			snprintf(lan_netmask, sizeof(lan_netmask), "lan_netmask" );
			lan_ifnames = nvram_safe_get("lan_ifnames");
		} 
		else {
			snprintf(tmp, sizeof(tmp), "lan%x_ifname", i);
			lan_ifname = nvram_safe_get( tmp);
			snprintf(lan_stp, sizeof(lan_stp), "lan%x_stp", i);
			snprintf(lan_dhcp, sizeof(lan_dhcp), "lan%x_dhcp",i );
			snprintf(lan_ipaddr, sizeof(lan_ipaddr), "lan%x_ipaddr",i );
			snprintf(lan_hwaddr, sizeof(lan_hwaddr), "lan%x_hwaddr",i );
			snprintf(lan_netmask, sizeof(lan_netmask), "lan%x_netmask",i );
			snprintf(tmp, sizeof(tmp), "lan%x_ifnames", i);
			lan_ifnames = nvram_safe_get( tmp);
		}
		if (strncmp(lan_ifname, "br", 2) == 0) {
			eval("brctl", "addbr", lan_ifname);
			eval("brctl", "setfd", lan_ifname, "0");
			if (nvram_match(lan_stp, "0"))
				eval("brctl", "stp", lan_ifname, "off");
			else
				eval("brctl", "stp", lan_ifname, "on");
#ifdef __CONFIG_EMF__
			if (nvram_match("emf_enable", "1")) {
				eval("emf", "add", "bridge", lan_ifname);
				eval("igs", "add", "bridge", lan_ifname);
			}
#endif /* __CONFIG_EMF__ */
			memset(hwaddr, 0, sizeof(hwaddr));

			foreach(name, lan_ifnames, next) {

				if (strncmp(name, "wl", 2) == 0) {
					if (!wl_vif_enabled(name, tmp)) {
						continue; /* Ignore disabled WL VIF */
					}
					wl_vif_hwaddr_set(name);
				}

				/* Bring up interface. Ignore any bogus/unknown interfaces on the NVRAM list */
				if (ifconfig(name, IFUP | IFF_ALLMULTI, NULL, NULL)){
					perror("ifconfig");
				}else{
					/* Set the logical bridge address to that of the first interface */
					if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
						perror("socket");
						continue;
					}
					strncpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
					if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0 &&
						memcmp(ifr.ifr_hwaddr.sa_data, "\0\0\0\0\0\0", ETHER_ADDR_LEN) == 0) {
						strncpy(ifr.ifr_name, name, IFNAMSIZ);
						if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0) {
							strncpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
							ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
							ioctl(s, SIOCSIFHWADDR, &ifr);

							memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
						}
					}
					close(s);

					/* If not a wl i/f then simply add it to the bridge */
					if (eval("wlconf", name, "up")) {				
						if (eval("brctl", "addif", lan_ifname, name))
							perror("brctl");
						else{
							snprintf(tmp, sizeof(tmp), "br%x_ifnames", i);
							ptr = nvram_get(tmp);
							if (ptr)
								snprintf(buf,sizeof(buf),"%s %s",ptr,name);
							else
								strncpy(buf,name,sizeof(buf));
							nvram_set(tmp,buf);
						}
#ifdef __CONFIG_EMF__
						if (nvram_match("emf_enable", "1"))
							eval("emf", "add", "iface", lan_ifname, name);
#endif /* __CONFIG_EMF__ */
					}else{
						char mode[] = "wlXXXXXXXXXX_mode";
						int unit = -1;
						
						/* get the instance number of the wl i/f */
						wl_ioctl(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
						
						snprintf(mode, sizeof(mode), "wl%d_mode", unit);
						
						/* WET specific configurations */
						if (nvram_match(mode, "wet")) {
							/* Receive all multicast frames in WET mode */
							ifconfig(name, IFUP | IFF_ALLMULTI, NULL, NULL);
							
							/* Enable host DHCP relay */
							if (nvram_match("lan_dhcp", "1"))
								wl_iovar_set(name, "wet_host_mac", ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
						}
						/* Dont attach the main wl i/f in wds */
						if ( (strncmp(name, "wl", 2) != 0 ) && ( nvram_match(mode, "wds")) ){ 
							/* Save this interface name in unbridged_ifnames
							 * This behaviour is consistent with BEARS release
							 */
							ptr = nvram_get("unbridged_ifnames");
							if (ptr)
								snprintf(buf, sizeof(buf), "%s %s", ptr, name);
							else
								strncpy(buf, name, sizeof(buf));
							nvram_set("unbridged_ifnames", buf);
							continue;
						}

						eval("brctl", "addif", lan_ifname, name);
#ifdef __CONFIG_EMF__
						if (nvram_match("emf_enable", "1"))
							eval("emf", "add", "iface", lan_ifname, name);
#endif /* __CONFIG_EMF__ */

						snprintf(tmp, sizeof(tmp), "br%x_ifnames", i);
						ptr = nvram_get(tmp);
						if (ptr)
							snprintf(buf,sizeof(buf),"%s %s",ptr,name);
						else
							strncpy(buf,name,sizeof(buf));
						nvram_set(tmp,buf);
						
					} /*if (eval("wlconf", na.....*/
					
				} /* if (ifconfig(name,...*/
			
			} /* foreach().... */

			if (memcmp(hwaddr, "\0\0\0\0\0\0", ETHER_ADDR_LEN) &&
			    (s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
				strncpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
				ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
				memcpy(ifr.ifr_hwaddr.sa_data, hwaddr, ETHER_ADDR_LEN);
				ioctl(s, SIOCSIFHWADDR, &ifr);
				close(s);
			}
		} /* if (strncmp(lan_ifname....*/
		/* specific non-bridged lan i/f */
		else if (strcmp(lan_ifname, "")) {
			/* Bring up interface */
			ifconfig(lan_ifname, IFUP, NULL, NULL);
			/* config wireless i/f */
			eval("wlconf", lan_ifname, "up");
		}
		else
			continue ; /* lanX_ifname is empty string , so donot do anything */
	
		/* Get current LAN hardware address */
		if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
			char eabuf[32];
			strncpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
			if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0)
				nvram_set(lan_hwaddr, ether_etoa((unsigned char *)ifr.ifr_hwaddr.sa_data, eabuf));
			close(s);
		}


		/* Launch DHCP client - AP only */
		if (nvram_match("router_disable", "1") && nvram_match(lan_dhcp, "1")) {
			char *dhcp_argv[] = {
				"udhcpc",
				"-i", lan_ifname,
				"-p", (sprintf(tmp, "/var/run/udhcpc-%s.pid", lan_ifname), tmp),
				"-s", "/tmp/ldhclnt",
				NULL
			};
			int pid;

			/* Start dhcp daemon */
			_eval(dhcp_argv, ">/dev/console", 0, &pid);
		}
		/* Handle static IP address - AP or Router */
		else {
			/* Bring up and configure LAN interface */
			ifconfig(lan_ifname, IFUP,
				nvram_safe_get(lan_ipaddr), nvram_safe_get(lan_netmask));
			/* We are done configuration */
			lan_up(lan_ifname);
		}

#ifdef __CONFIG_EMF__
		/* Start the EMF for this LAN */
		start_emf(lan_ifname);
#endif /* __CONFIG_EMF__ */
	} /* For loop */

	} /* if (nvram_match("ses_bridge_disable", "1")) */

	/* Set initial QoS mode for LAN ports. */
	set_et_qos_mode();

	/* start syslogd if either log_ipaddr or log_ram_enable is set */
	if (nvram_invmatch("log_ipaddr", "") || nvram_match("log_ram_enable", "1")) {
#if !defined(__CONFIG_BUSYBOX__) || defined(BB_SYSLOGD)
		char *argv[] = {
			"syslogd",
			NULL,		/* -C */
			NULL, NULL,	/* -R host */
			NULL
		};
		int pid;
		int argc = 1;

		if (nvram_match("log_ram_enable", "1"))
			argv[argc++] = "-C";

		if (nvram_invmatch("log_ipaddr", "")) {
			argv[argc++] = "-R";
			argv[argc++] = nvram_get("log_ipaddr");
		}


		_eval(argv, NULL, 0, &pid);
#else /* Busybox configured w/o syslogd */
		cprintf("Busybox configured w/o syslogd\n");
#endif
	}

	dprintf("%s %s\n",
		nvram_safe_get("lan_ipaddr"),
		nvram_safe_get("lan_netmask"));

}

void
stop_lan(void)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char name[80], *next, signal[] = "XXXXXXXX";
	char br_prefix[20];
	char tmp[20];
	int i=0;
	char* lan_ifnames;
	
	dprintf("%s\n", lan_ifname);

	/* Stop the syslogd daemon */
	eval("killall", "syslogd");
	/* release the DHCP address and kill the client */
	snprintf(signal, sizeof(signal), "-%d", SIGUSR2);
	eval("killall", signal, "udhcpc");
	eval("killall", "udhcpc");

	/* Remove static routes */
	del_lan_routes(lan_ifname);

	/* Bring down unbridged interfaces,if any */
	foreach(name, nvram_safe_get("unbridged_ifnames"), next) {
		eval("wlconf", name, "down");
		ifconfig(name, 0, NULL, NULL);
	}

	for(i=0; i < MAX_NO_BRIDGE; i++) {
		if(!i) {
			lan_ifname = nvram_safe_get("lan_ifname");
			snprintf(br_prefix, sizeof(br_prefix), "br0_ifnames");
		}
		else {
			snprintf(tmp, sizeof(tmp), "lan%x_ifname", i);
			lan_ifname = nvram_safe_get( tmp);
			snprintf(br_prefix, sizeof(br_prefix), "br%x_ifnames",i);
		}
		if (!strcmp(lan_ifname, "")) 
			continue;

#ifdef __CONFIG_EMF__
		/* Stop the EMF for this LAN */
		eval("emf", "stop", lan_ifname);
#endif /* __CONFIG_EMF__ */

		/* Bring down LAN interface */
		ifconfig(lan_ifname, 0, NULL, NULL);
	
		/* Bring down bridged interfaces */
		if (strncmp(lan_ifname, "br", 2) == 0) {
			lan_ifnames = nvram_safe_get(br_prefix);
			foreach(name, lan_ifnames, next) {
				eval("wlconf", name, "down");
				ifconfig(name, 0, NULL, NULL);
				eval("brctl", "delif", lan_ifname, name);
			}
			eval("brctl", "delbr", lan_ifname);
		}
		/* Bring down specific interface */
		else if (strcmp(lan_ifname, ""))
			eval("wlconf", lan_ifname, "down");
	}

	unlink("/tmp/ldhclnt");

	dprintf("done\n");
}

void
start_wl(void)
{
	int i;
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char name[80], *next;
	char tmp[100];
	char *lan_ifnames;

	/* If we're a travel router... then we need to make sure we get
		 the primary wireless interface up before trying to attach slave
		 interface(s) to the bridge */
	if(nvram_match("ure_disable", "0") && nvram_match("router_disable", "0")) {
		/* start wlireless */
		eval("wlconf", nvram_get("wan0_ifname"), "start");
	}

	if (nvram_match("ses_bridge_disable", "1")) {
		/* dont bring up the bridge; only the individual interfaces */
		foreach(name, nvram_safe_get("lan_ifnames"), next) {
			/* start wlireless */
			eval("wlconf", name, "start");
		}
	} else {
 	/* Bring up bridged interfaces */
	for(i=0; i < MAX_NO_BRIDGE; i++) {
		if(!i) {
			lan_ifname = nvram_safe_get("lan_ifname");
			lan_ifnames = nvram_safe_get("lan_ifnames");
		} 
		else {
			snprintf(tmp, sizeof(tmp), "lan%x_ifname", i);
			lan_ifname = nvram_safe_get( tmp);
			snprintf(tmp, sizeof(tmp), "lan%x_ifnames", i);
			lan_ifnames = nvram_safe_get( tmp);
		}
		if (strncmp(lan_ifname, "br", 2) == 0) {
			foreach(name, lan_ifnames, next) {
				if (strncmp(name, "wl", 2) == 0) {
					if (!wl_vif_enabled(name, tmp)) {
						continue; /* Ignore disabled WL VIF */
					}
				}
				/* If a wl i/f, start it */
				eval("wlconf", name, "start");

			} /* foreach().... */
		} /* if (strncmp(lan_ifname....*/
		/* specific non-bridged lan i/f */
		else if (strcmp(lan_ifname, "")) {
			/* start wireless i/f */
			eval("wlconf", lan_ifname, "start");
		}
	} /* For loop */
	} /* if (nvram_match("ses_bridge_disable", "1")) */
}

#ifdef __CONFIG_NAT__
static int
wan_prefix(char *ifname, char *prefix)
{
	int unit;
	
	if ((unit = wan_ifunit(ifname)) < 0)
		return -1;

	sprintf(prefix, "wan%d_", unit);
	return 0;
}

static int
add_wan_routes(char *wan_ifname)
{
	char prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	return add_routes(prefix, "route", wan_ifname);
}

static int
del_wan_routes(char *wan_ifname)
{
	char prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	return del_routes(prefix, "route", wan_ifname);
}

static int
wan_valid(char *ifname)
{
	char name[80], *next;
	
	foreach(name, nvram_safe_get("wan_ifnames"), next)
		if (ifname && !strcmp(ifname, name))
			return 1;

#ifdef LW_CUST
    if(!strcmp(ifname, "br2"))
        return 1;
#endif
	return 0;
}

#ifdef LW_CUST
void
start_wanbr(void)
{
    ifconfig("vlan2", IFUP, NULL, NULL);
    ifconfig("vlan100", IFUP, NULL, NULL);
    eval("brctl", "addbr", "br2");
    eval("brctl", "setfd", "br2", "0");
    eval("brctl", "addif", "br2", "vlan2");
    eval("brctl", "addif", "br2" ,"vlan100");
    if(nvram_match("wl0.1_bss_enabled", "1"))
    {
        ifconfig("wl0.1", IFUP, NULL, NULL);
        eval("brctl", "addif", "br2" ,"wl0.1");
        eval("ifconfig", "br2", "promisc", "up");
    }
    eval("ifconfig", "br2:1", "192.168.100.254");
}

void
stop_wanbr(void)
{
    ifconfig("vlan2", 0, NULL, NULL);
    ifconfig("vlan100", 0, NULL, NULL);
    ifconfig("br2", 0, NULL, NULL);
    eval("brctl", "delif", "br2", "vlan2");
    eval("brctl", "delif", "br2", "vlan100");
    //if(nvram_match("wl0.1_bss_enabled", "1"))
    {
        ifconfig("wl0.1", 0, NULL, NULL);
        eval("brctl", "delif", "br2" ,"wl0.1");
    }
    eval("brctl", "delbr", "br2");
}
#endif

void
start_wan(void)
{
	char *wan_ifname;
	char *wan_proto;
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char eabuf[32];
	int s;
	struct ifreq ifr;
	pid_t pid;

	/* check if we need to setup WAN */
	if (nvram_match("router_disable", "1"))
		return;

	/* do not bringup wan if in ses bridge disable mode */
	if (nvram_match("ses_bridge_disable", "1"))
		return;

	/* start connection independent firewall */
	start_firewall();

	/* Create links */
	mkdir("/tmp/ppp", 0777);
	symlink("/sbin/rc", "/tmp/ppp/ip-up");
	symlink("/sbin/rc", "/tmp/ppp/ip-down");
	
	symlink("/sbin/rc", "/tmp/udhcpc");

#ifdef LW_CUST
    start_wanbr();
#endif

	/* Start each configured and enabled wan connection and its undelying i/f */
	for (unit = 0; unit < MAX_NVPARSE; unit ++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		/* make sure the connection exists and is enabled */ 
		wan_ifname = nvram_get(strcat_r(prefix, "ifname", tmp));
		if (!wan_ifname)
			continue;
		wan_proto = nvram_get(strcat_r(prefix, "proto", tmp));
		if (!wan_proto || !strcmp(wan_proto, "disabled"))
			continue;

		/* disable the connection if the i/f is not in wan_ifnames */
		if (!wan_valid(wan_ifname)) {
			nvram_set(strcat_r(prefix, "proto", tmp), "disabled");
			continue;
		}

		dprintf("%s %s\n", wan_ifname, wan_proto);

		/* Set i/f hardware address before bringing it up */
		if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
			continue;
		strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);

		/* Configure i/f only once, specially for wl i/f shared by multiple connections */
		if (ioctl(s, SIOCGIFFLAGS, &ifr)) {
			close(s);
			continue;
		}

		if (!(ifr.ifr_flags & IFF_UP)) {
			/* Sync connection nvram address and i/f hardware address */
			memset(ifr.ifr_hwaddr.sa_data, 0, ETHER_ADDR_LEN);
			if (!nvram_invmatch(strcat_r(prefix, "hwaddr", tmp), "") ||
			    !ether_atoe(nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)),
					(unsigned char *)ifr.ifr_hwaddr.sa_data) ||
			    !memcmp(ifr.ifr_hwaddr.sa_data, "\0\0\0\0\0\0", ETHER_ADDR_LEN)) {
				if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
					close(s);
					continue;
				}
				nvram_set(strcat_r(prefix, "hwaddr", tmp),
					  ether_etoa((unsigned char *)ifr.ifr_hwaddr.sa_data, eabuf));
			} else {
				ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
				ioctl(s, SIOCSIFHWADDR, &ifr);
			}

			/* Bring up i/f */
			ifconfig(wan_ifname, IFUP, NULL, NULL);

			/* do wireless specific config */
			if (nvram_match("ure_disable", "1")) {
				eval("wlconf", wan_ifname, "up");
				eval("wlconf", wan_ifname, "start");
			}
		}

		close(s);

		/* Set initial QoS mode again now that WAN port is ready. */
		set_et_qos_mode();

		/* 
		* Configure PPPoE connection. The PPPoE client will run 
		* ip-up/ip-down scripts upon link's connect/disconnect.
		*/
		if (strcmp(wan_proto, "pppoe") == 0) {
			char *pppoe_argv[] = {
				"pppoecd",
				nvram_safe_get(strcat_r(prefix, "ifname", tmp)),
				"-u", nvram_safe_get(strcat_r(prefix, "pppoe_username", tmp)),
				"-p", nvram_safe_get(strcat_r(prefix, "pppoe_passwd", tmp)),
				"-r", nvram_safe_get(strcat_r(prefix, "pppoe_mru", tmp)),
				"-t", nvram_safe_get(strcat_r(prefix, "pppoe_mtu", tmp)),
				"-i", nvram_match(strcat_r(prefix, "pppoe_demand", tmp), "1") ?
				nvram_safe_get(strcat_r(prefix, "pppoe_idletime", tmp)) : "0",
				NULL, NULL,	/* pppoe_service */
				NULL, NULL,	/* pppoe_ac */
				NULL,		/* pppoe_keepalive */
				NULL, NULL,	/* ppp unit requested */
				NULL
			}, **arg;
			int timeout = 5;
			char pppunit[] = "XXXXXXXXXXXX";

			/* Add optional arguments */
			for (arg = pppoe_argv; *arg; arg++);
			if (nvram_invmatch(strcat_r(prefix, "pppoe_service", tmp), "")) {
				*arg++ = "-s";
				*arg++ = nvram_safe_get(strcat_r(prefix, "pppoe_service", tmp));
			}
			if (nvram_invmatch(strcat_r(prefix, "pppoe_ac", tmp), "")) {
				*arg++ = "-a";
				*arg++ = nvram_safe_get(strcat_r(prefix, "pppoe_ac", tmp));
			}
			if (nvram_match(strcat_r(prefix, "pppoe_demand", tmp), "1") || 
			    nvram_match(strcat_r(prefix, "pppoe_keepalive", tmp), "1"))
				*arg++ = "-k";
			snprintf(pppunit, sizeof(pppunit), "%d", unit);
			*arg++ = "-U";
			*arg++ = pppunit;

			/* launch pppoe client daemon */
			_eval(pppoe_argv, NULL, 0, &pid);

			/* ppp interface name is referenced from this point on */
			wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));
			
			/* Pretend that the WAN interface is up */
			if (nvram_match(strcat_r(prefix, "pppoe_demand", tmp), "1")) {
				/* Wait for pppx to be created */
				while (ifconfig(wan_ifname, IFUP, NULL, NULL) && timeout--)
					sleep(1);

				/* Retrieve IP info */
				if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
					continue;
				strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);

				/* Set temporary IP address */
				if (ioctl(s, SIOCGIFADDR, &ifr))
					perror(wan_ifname);
				nvram_set(strcat_r(prefix, "ipaddr", tmp), inet_ntoa(sin_addr(&ifr.ifr_addr)));
				nvram_set(strcat_r(prefix, "netmask", tmp), "255.255.255.255");

				/* Set temporary P-t-P address */
				if (ioctl(s, SIOCGIFDSTADDR, &ifr))
					perror(wan_ifname);
				nvram_set(strcat_r(prefix, "gateway", tmp), inet_ntoa(sin_addr(&ifr.ifr_dstaddr)));

				close(s);

				/* 
				* Preset routes so that traffic can be sent to proper pppx even before 
				* the link is brought up.
				*/
				preset_wan_routes(wan_ifname);
			}
		}
		/* 
		* Configure DHCP connection. The DHCP client will run 
		* 'udhcpc bound'/'udhcpc deconfig' upon finishing IP address 
		* renew and release.
		*/
		else if (strcmp(wan_proto, "dhcp") == 0) {
			char *wan_hostname = nvram_safe_get(strcat_r(prefix, "hostname", tmp));
			char *dhcp_argv[] = { "udhcpc",
					      "-i", wan_ifname,
					      "-p", (sprintf(tmp, "/var/run/udhcpc%d.pid", unit), tmp),
					      "-s", "/tmp/udhcpc",
					      wan_hostname && *wan_hostname ? "-H" : NULL,
					      wan_hostname && *wan_hostname ? wan_hostname : NULL,
					      NULL
			};
			/* Start dhcp client */
			_eval(dhcp_argv, NULL, 0, &pid);
		}
		/* Configure static IP connection. */
		else if (strcmp(wan_proto, "static") == 0) {
			/* Assign static IP address to i/f */
			ifconfig(wan_ifname, IFUP,
				 nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)), 
				 nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
			/* We are done configuration */
			wan_up(wan_ifname);
		}

		/* Start connection dependent firewall */
		start_firewall2(wan_ifname);

		dprintf("%s %s\n",
			nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
			nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
	}

	/* Report stats */
	if (nvram_invmatch("stats_server", "")) {
		char *stats_argv[] = { "stats", nvram_get("stats_server"), NULL };
		_eval(stats_argv, NULL, 5, NULL);
	}
}

void
stop_wan(void)
{
	char name[80], *next, signal[] = "XXXX";
	
#ifdef BCMQOS
		del_iQosRules();
#endif /* BCMQOS */
	eval("killall", "stats");
	eval("killall", "ntpclient");

	/* Shutdown and kill all possible tasks */
	eval("killall", "ip-up");
	eval("killall", "ip-down");
	snprintf(signal, sizeof(signal), "-%d", SIGHUP);
	eval("killall", signal, "pppoecd");
	eval("killall", "pppoecd");
	snprintf(signal, sizeof(signal), "-%d", SIGUSR2);
	eval("killall", signal, "udhcpc");
	eval("killall", "udhcpc");

	/* Bring down WAN interfaces */
	foreach(name, nvram_safe_get("wan_ifnames"), next)
		ifconfig(name, 0, "0.0.0.0", NULL);

	/* Remove dynamically created links */
	unlink("/tmp/udhcpc");
	
	unlink("/tmp/ppp/ip-up");
	unlink("/tmp/ppp/ip-down");
	rmdir("/tmp/ppp");

	dprintf("done\n");
}

static int
add_ns(char *wan_ifname)
{
	FILE *fp;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char word[100], *next;
	char line[100];

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	/* Open resolv.conf to read */
	if (!(fp = fopen("/tmp/resolv.conf", "r+"))) {
		perror("/tmp/resolv.conf");
		return errno;
	}
	/* Append only those not in the original list */
	foreach(word, nvram_safe_get(strcat_r(prefix, "dns", tmp)), next) {
		fseek(fp, 0, SEEK_SET);
		while (fgets(line, sizeof(line), fp)) {
			char *token = strtok(line, " \t\n");

			if (!token || strcmp(token, "nameserver") != 0)
				continue;
			if (!(token = strtok(NULL, " \t\n")))
				continue;

			if (!strcmp(token, word))
				break;
		}
		if (feof(fp))
			fprintf(fp, "nameserver %s\n", word);
	}
	fclose(fp);

	/* notify dnsmasq */
	snprintf(tmp, sizeof(tmp), "-%d", SIGHUP);
	eval("killall", tmp, "dnsmasq");
	
	return 0;
}

static int
del_ns(char *wan_ifname)
{
	FILE *fp, *fp2;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char word[100], *next;
	char line[100];

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	/* Open resolv.conf to read */
	if (!(fp = fopen("/tmp/resolv.conf", "r"))) {
		perror("fopen /tmp/resolv.conf");
		return errno;
	}
	/* Open resolv.tmp to save updated name server list */
	if (!(fp2 = fopen("/tmp/resolv.tmp", "w"))) {
		perror("fopen /tmp/resolv.tmp");
		fclose(fp);
		return errno;
	}
	/* Copy updated name servers */
	while (fgets(line, sizeof(line), fp)) {
		char *token = strtok(line, " \t\n");

		if (!token || strcmp(token, "nameserver") != 0)
			continue;
		if (!(token = strtok(NULL, " \t\n")))
			continue;

		foreach(word, nvram_safe_get(strcat_r(prefix, "dns", tmp)), next)
			if (!strcmp(word, token))
				break;
		if (!next)
			fprintf(fp2, "nameserver %s\n", token);
	}
	fclose(fp);
	fclose(fp2);
	/* Use updated file as resolv.conf */
	unlink("/tmp/resolv.conf");
	rename("/tmp/resolv.tmp", "/tmp/resolv.conf");
	
	/* notify dnsmasq */
	snprintf(tmp, sizeof(tmp), "-%d", SIGHUP);
	eval("killall", tmp, "dnsmasq");
	
	return 0;
}

/*
*/
#ifdef __CONFIG_IPV6__
/* Start the 6to4 Tunneling interface.
*	Return > 0: number of interfaces processed by this routine.
*		==0: skipped since no action is required.
*		< 0: Error number
*/
static int
start_6to4(char *wan_ifname)
{
	int i, ret = 0;
	int siMode, siCount;
	unsigned short uw6to4ID;
	in_addr_t uiWANIP;
	char *pcLANIF, *pcWANIP, tmp[64], prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return 0;

	pcWANIP = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
	uiWANIP = inet_network(pcWANIP);

	/* Check if the wan IP is private(RFC1918). 6to4 needs a global IP */
	if ((uiWANIP == 0) || (uiWANIP == -1) ||
		(uiWANIP & 0xffff0000) == 0xc0a80000 || /* 192.168.x.x */
		(uiWANIP & 0xfff00000) == 0xac100000 || /* 172.16.x.x */
		(uiWANIP & 0xff000000) == 0x0a000000) /* 10.x.x.x */
		return 0;

	/* Create 6to4 intrface and setup routing table */
	for (i = 0, siCount = 0; i < MAX_NO_BRIDGE; i++) {
		if (i == 0) {
			pcLANIF = nvram_safe_get("lan_ifname");
			siMode = atoi(nvram_safe_get("lan_ipv6_mode"));
			uw6to4ID = (unsigned short)atoi(nvram_safe_get("lan_ipv6_6to4id"));
		}
		else {
			snprintf(tmp, sizeof(tmp), "lan%x_ifname", i);
			pcLANIF = nvram_safe_get(tmp);
			snprintf(tmp, sizeof(tmp), "lan%x_ipv6_mode", i);
			siMode = atoi(nvram_safe_get(tmp));
			snprintf(tmp, sizeof(tmp), "lan%x_ipv6_6to4id", i);
			uw6to4ID = (unsigned short)atoi(nvram_safe_get(tmp));
		}

		if (siMode & IPV6_6TO4_ENABLED) {
			/* Add the 6to4 route. */
			snprintf(tmp, sizeof(tmp), "2002:%x:%x:%x::/64",
				(unsigned short)(uiWANIP>>16), (unsigned short)uiWANIP,	uw6to4ID);
			ret = eval("/usr/sbin/ip", "-6", "route", "add", tmp,
				"dev", pcLANIF, "metric", "1");
			siCount++;
		}
	}

	if (siCount == 0)
		return 0;

	/* Create 6to4 intrface and setup routing table */
	{	
		char *pc6to4IF = "v6to4"; /* The 6to4 tunneling interface name */

		/* Create the tunneling interface */
		ret = eval("/usr/sbin/ip", "tunnel", "add", pc6to4IF,
			"mode", "sit", "ttl", "64", "remote", "any", "local", pcWANIP);
		
		/* Bring the device up */
		ret = eval("/usr/sbin/ip", "link", "set", "dev", pc6to4IF, "up");
		
		/* Add 6to4 v4 anycast route to the global IPv6 network */
		ret = eval("/usr/sbin/ip", "-6", "route", "add", "2000::/3",
			"via", "::192.88.99.1", "dev", pc6to4IF, "metric", "1");
	}

#ifdef __CONFIG_RADVD__
	/* Restart radvd */
	{
		char acSignal[64];

		snprintf(acSignal, sizeof(acSignal), "-%d", SIGHUP);
		ret = eval("killall", acSignal, "radvd");
	}
#endif /* __CONFIG_RADVD__ */

#ifdef __CONFIG_NAT__
	/* Enable IPv6 protocol=41(0x29) on v4NAT */
	{
		char *pcWANIF;
			
		pcWANIF = nvram_match("wan_proto", "pppoe")? 
			nvram_safe_get("wan_pppoe_ifname"): nvram_safe_get("wan_ifname");
		add_ipv6_filter(nvram_safe_get(pcWANIF));
	}
#endif /* __CONFIG_NAT__ */

	return siCount;
}
#endif /* __CONFIG_IPV6__ */
/*
*/

void
wan_up(char *wan_ifname)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wan_proto;

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return;

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	
	dprintf("%s %s\n", wan_ifname, wan_proto);

	/* Set default route to gateway if specified */
	if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
		route_add(wan_ifname, 0, "0.0.0.0", 
			nvram_safe_get(strcat_r(prefix, "gateway", tmp)),
			"0.0.0.0");

	/* Install interface dependent static routes */
	add_wan_routes(wan_ifname);

	/* Add dns servers to resolv.conf */
	add_ns(wan_ifname);

	/* Sync time */
	start_ntpc();
	
#ifdef BCMQOS
    add_iQosRules(wan_ifname);
	start_iQos();
#endif /* BCMQOS */
/*
*/
#ifdef __CONFIG_IPV6__
	start_6to4(wan_ifname);
#endif /* __CONFIG_IPV6__ */
/*
*/

	dprintf("done\n");
}

void
wan_down(char *wan_ifname)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wan_proto;

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return;

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	
	printf("%s %s\n", wan_ifname, wan_proto);

	/* Remove default route to gateway if specified */
	if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
		route_del(wan_ifname, 0, "0.0.0.0", 
			nvram_safe_get(strcat_r(prefix, "gateway", tmp)),
			"0.0.0.0");

	/* Remove interface dependent static routes */
	del_wan_routes(wan_ifname);

	/* Update resolv.conf */
	del_ns(wan_ifname);

	dprintf("done\n");
}
#endif	/* __CONFIG_NAT__ */

/* Enable WET DHCP relay for ethernet clients */
static int
enable_dhcprelay(char *ifname)
{
	char name[80], *next;

	dprintf("%s\n", ifname);
	
	/* WET interface is meaningful only in bridged environment */
	if (strncmp(ifname, "br", 2) == 0) {
		foreach(name, nvram_safe_get("lan_ifnames"), next) {
			char mode[] = "wlXXXXXXXXXX_mode";
			int unit;

			/* make sure the interface is indeed of wl */
			if (wl_probe(name))
				continue;
			
			/* get the instance number of the wl i/f */
			wl_ioctl(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
			snprintf(mode, sizeof(mode), "wl%d_mode", unit);

			/* enable DHCP relay, there should be only one WET i/f */
			if (nvram_match(mode, "wet")) {
				uint32 ip;
				inet_aton(nvram_safe_get("lan_ipaddr"), (struct in_addr *)&ip);
				if (wl_iovar_setint(name, "wet_host_ipv4", ip))
					perror("wet_host_ipv4");
				break;
			}
		}
	}
	return 0;
}

void
lan_up(char *lan_ifname)
{
	/* Install default route to gateway - AP only */
	if (nvram_match("router_disable", "1") && nvram_invmatch("lan_gateway", ""))
		route_add(lan_ifname, 0, "0.0.0.0", nvram_safe_get("lan_gateway"), "0.0.0.0");

	/* Install interface dependent static routes */
	add_lan_routes(lan_ifname);

	/* Sync time - AP only */
	if (nvram_match("router_disable", "1"))
		start_ntpc();

	/* Enable WET DHCP relay if requested */
	if (atoi(nvram_safe_get("dhcp_relay")) == 1)
		enable_dhcprelay(lan_ifname);

	dprintf("done\n");
}

void
lan_down(char *lan_ifname)
{
	/* Remove default route to gateway - AP only */
	if (nvram_match("router_disable", "1") && nvram_invmatch("lan_gateway", ""))
		route_del(lan_ifname, 0, "0.0.0.0", nvram_safe_get("lan_gateway"), "0.0.0.0");

	/* Remove interface dependent static routes */
	del_lan_routes(lan_ifname);

	dprintf("done\n");
}

int
hotplug_net(void)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char *interface, *action;

	if (!(interface = getenv("INTERFACE")) ||
	    !(action = getenv("ACTION")))
		return EINVAL;

	if (strncmp(interface, "wds", 3))
		return 0;

#ifdef LINUX26 
	if (!strcmp(action, "add")) {
#else
	if (!strcmp(action, "register")) {
#endif
		/* Bring up the interface and add to the bridge */
		ifconfig(interface, IFUP, NULL, NULL);
		
#ifdef __CONFIG_EMF__
		if (nvram_match("emf_enable", "1")) {
			eval("emf", "add", "iface", lan_ifname, interface);
			emf_mfdb_update(lan_ifname, interface, TRUE);
			emf_uffp_update(lan_ifname, interface, TRUE);
			emf_rtport_update(lan_ifname, interface, TRUE);
		}
#endif /* __CONFIG_EMF__ */

		/* Bridge WDS interfaces */
		if (!strncmp(lan_ifname, "br", 2) && 
		    eval("brctl", "addif", lan_ifname, interface, "wait")){
		    cprintf("hotplug_net():Adding interface %s\n",interface);
		    return 0;
		}
		
		/* Inform driver to send up new WDS link event */
		if (wl_iovar_setint(interface, "wds_enable", 1)) {
			cprintf("%s set wds_enable failed\n", interface);
			return 0;
		}
	}

	return 0;
}

#ifdef __CONFIG_NAT__
int
wan_ifunit(char *wan_ifname)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	if ((unit = ppp_ifunit(wan_ifname)) >= 0)
		return unit;
	else {
		for (unit = 0; unit < MAX_NVPARSE; unit ++) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
			    (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") ||
			     nvram_match(strcat_r(prefix, "proto", tmp), "static")))
				return unit;
		}
	}
	return -1;
}

int
preset_wan_routes(char *wan_ifname)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	/* Set default route to gateway if specified */
	if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
		route_add(wan_ifname, 0, "0.0.0.0", "0.0.0.0", "0.0.0.0");

	/* Install interface dependent static routes */
	add_wan_routes(wan_ifname);
	return 0;
}

int
wan_primary_ifunit(void)
{
	int unit;
	
	for (unit = 0; unit < MAX_NVPARSE; unit ++) {
		char tmp[100], prefix[] = "wanXXXXXXXXXX_";
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
			return unit;
	}

	return 0;
}
#endif	/* __CONFIG_NAT__ */
