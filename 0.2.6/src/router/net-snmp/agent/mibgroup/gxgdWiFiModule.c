#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/if.h>
#include <arpa/inet.h>
#include <errno.h>

#include <bcmnvram.h>
#include <shutils.h>
#include <wlutils.h>

#include <stdbool.h>
#include "gxgdWiFiModule.h"

static int factoryrestorestatus;
static bool superadminmodeenabled;
static int applysettingstatus;
static int rebootonceupgradedone = SNMPFALSE;

static struct timeval builddhcpclienttabletime;
static struct timeval buildwifistationtabletime;
static struct timeval buildwfitabletime;
static struct timeval buildvlantabletime;
static struct timeval buildvidtabletime;

/* Typical data structure for a row entry */
struct wirelessTable_entry {
    /* Index values */
    long ssidindex;

    /* Column values */
    char ssidtailer[SSID_MAX_LEN];
    size_t ssidtailer_len;
    char old_ssidtailer[SSID_MAX_LEN];
    size_t old_ssidtailer_len;
    long isolation;
    long old_isolation;
    long ssidbroadcast;
    long old_ssidbroadcast;
    long secmode;
    long old_secmode;
    char passphrase[MAXPSKLEN];
    size_t passphrase_len;
    char old_passphrase[MAXPSKLEN];
    size_t old_passphrase_len;
    long maxsta;
    long old_maxsta;
    long encryptionmethod;
    long old_encryptionmethod;
    long enabled;
    long old_enabled;
    char macaddress[MACADDRSTRLEN];
    size_t macaddress_len;
    char old_macaddress[MACADDRSTRLEN];
    size_t old_macaddress_len;
    long headerchangable;
    long old_headerchangable;
    char ssidheader[MAXSSIDHDRLEN];
    size_t ssidheader_len;
    char old_ssidheader[MAXSSIDHDRLEN];
    size_t old_ssidheader_len;
    long old_ssidindex;

    /* Illustrate using a simple linked list */
    int   valid;
    struct wirelessTable_entry *next;
};

struct wirelessTable_entry  *wirelessTable_head = NULL;
bool setwlsecuritymode(int secmode, char *wlnameprefix, struct wirelessTable_entry *entry);
bool setwlencryptionmethod(int encryptmode, char *wlnameprefix, struct wirelessTable_entry *entry);
bool getwlsecmethod(long *secmode, char *wlnameprefix);

struct lease_t {
    unsigned char chaddr[16];
    u_int32_t yiaddr;
    u_int32_t expires;
    char hostname[64];
};

void buildvlantable();
void buildvidtable();

#define sys_restart() kill(1, SIGHUP)

inline void sys_reboot(void)
{
    eval("wl", "reboot");
    kill(1, SIGTERM);
}

void del_iQosRules(void)
{
    /* Flush all rules in mangle table */
    eval("iptables", "-t", "mangle", "-F");
}

int ifconfig(char *name, int flags, char *addr, char *netmask)
{
    int s;
    struct ifreq ifr;
    struct in_addr in_addr, in_netmask, in_broadaddr;

    /* Open a raw socket to the kernel */
    if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
        goto err;

    /* Set interface name */
    strncpy(ifr.ifr_name, name, IFNAMSIZ);

    /* Set interface flags */
    ifr.ifr_flags = flags;
    if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0)
        goto err;

    /* Set IP address */
    if (addr) {
        inet_aton(addr, &in_addr);
        sin_addr(&ifr.ifr_addr).s_addr = in_addr.s_addr;
        ifr.ifr_addr.sa_family = AF_INET;
        if (ioctl(s, SIOCSIFADDR, &ifr) < 0)
            goto err;
    }

    /* Set IP netmask and broadcast */
    if (addr && netmask) {
        inet_aton(netmask, &in_netmask);
        sin_addr(&ifr.ifr_netmask).s_addr = in_netmask.s_addr;
        ifr.ifr_netmask.sa_family = AF_INET;
        if (ioctl(s, SIOCSIFNETMASK, &ifr) < 0)
            goto err;

        in_broadaddr.s_addr = (in_addr.s_addr & in_netmask.s_addr) | ~in_netmask.s_addr;
        sin_addr(&ifr.ifr_broadaddr).s_addr = in_broadaddr.s_addr;
        ifr.ifr_broadaddr.sa_family = AF_INET;
        if (ioctl(s, SIOCSIFBRDADDR, &ifr) < 0)
            goto err;
    }

    close(s);

    return 0;

err:
    close(s);
    perror(name);
    return errno;
}	

void stop_wan(void)
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

bool updatedefaultgw(char *ipaddr, char *netmask)
{
    struct in_addr lanip, mask, gw;
    char gws[32];

    inet_aton(ipaddr, &lanip);
    inet_aton(netmask, &mask);

    bzero(gws, sizeof(gws));
    strcpy(gws, nvram_safe_get("lan_gateway"));

    inet_aton(gws, &gw);

    if((gw.s_addr & mask.s_addr) != (lanip.s_addr & mask.s_addr)){

        gw.s_addr = htonl(ntohl((lanip.s_addr & mask.s_addr)) | 0x01);

        bzero(gws,sizeof(gws));
        strcpy(gws, inet_ntoa(gw));
        nvram_set("lan_gateway", gws);
        return true;
    }
    else{
        return false;
    }
}

bool updatedhcpaddress(char *ipaddr, char *netmask)
{
    struct in_addr lanip, mask, dhcpstart, dhcpend;
    char tmpbuf[32];

    inet_aton(ipaddr, &lanip);
    inet_aton(netmask, &mask);

    bzero(tmpbuf, sizeof(tmpbuf));
    strcpy(tmpbuf, nvram_safe_get("dhcp_start"));
    inet_aton(tmpbuf, &dhcpstart);

    bzero(tmpbuf, sizeof(tmpbuf));
    strcpy(tmpbuf, nvram_safe_get("dhcp_end"));
    inet_aton(tmpbuf, &dhcpend);

    if(((dhcpstart.s_addr & mask.s_addr) != ( lanip.s_addr & mask.s_addr )) || \
            ((dhcpend.s_addr & mask.s_addr) != ( lanip.s_addr & mask.s_addr ))){

        if(~(ntohl(mask.s_addr)) < DHCPMINIMUMPOOLSIZE )
            return false;
        else{
            dhcpstart.s_addr = htonl(ntohl(lanip.s_addr & mask.s_addr) + DHCPPOOLSTART);
            dhcpend.s_addr = htonl(ntohl(lanip.s_addr & mask.s_addr) | ntohl(~mask.s_addr));

            nvram_set("dhcp_start", inet_ntoa(dhcpstart));
            nvram_set("dhcp_end", inet_ntoa(dhcpend));

            return true;
        }
    }
    else{
        return true;
    }
}

bool validatedhcpaddress(char *dhcpaddr)
{
    struct in_addr lanip, mask, dhcp;
    char ipaddr[16], netmask[16];

    bzero(ipaddr, sizeof(ipaddr));
    strcpy(ipaddr, nvram_safe_get("lan_ipaddr"));
    inet_aton(ipaddr, &lanip);

    bzero(netmask, sizeof(netmask));
    strcpy(netmask, nvram_safe_get("lan_netmask"));
    inet_aton(netmask, &mask);

    inet_aton(dhcpaddr, &dhcp);

    if((dhcp.s_addr & mask.s_addr) == ( lanip.s_addr & mask.s_addr )){
        return true;
    }
    else{
        return false;
    }
}

int nvram_nset(const char *name, const char *value, const int length)
{
    char tmpbuf[64];
    strncpy(tmpbuf, value, length);
    tmpbuf[length] = '\0';

    nvram_set(name, tmpbuf);
    return 0;
}

/** Initializes the gxgdWiFiModule module */
    void
init_gxgdWiFiModule(void)
{
    const oid loginusername_oid[] = { 1,3,6,1,4,1,46227,2,2,1,1,1 };
    const oid loginpassword_oid[] = { 1,3,6,1,4,1,46227,2,2,1,1,2 };
    const oid lanipaddress_oid[] = { 1,3,6,1,4,1,46227,2,2,1,1,3 };
    const oid lannetmask_oid[] = { 1,3,6,1,4,1,46227,2,2,1,1,4 };
    const oid reset_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,2 };
    const oid factoryrestore_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,3 };
    const oid txpower_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,6 };
    const oid wifimode_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,7 };
    const oid channel_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,8 };
    const oid ssidnumber_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,17 };
    const oid applysettings_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,21 };
    const oid pppoeusername_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,22 };
    const oid pppoepassword_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,23 };
    const oid pppoeservicename_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,24 };
    const oid pppoeconnectionstatus_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,25 };
    const oid pppoeon_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,27 };
    const oid wanon_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,28 };
    const oid wandhcpoption60_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,29 };
    const oid wirelessmode_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,30 };
    const oid pppoeconnectondemand_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,31 };
    const oid pppoedisconnect_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,33 };
    const oid wpsenable_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,37 };
    const oid dhcpenable_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,38 };
    const oid dhcpaddresstart_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,39 };
    const oid dhcpaddressend_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,40 };
    const oid dhcpnetmask_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,41 };
    const oid dhcpgateway_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,42 };
    const oid dhcpleasetime_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,43 };
    const oid wifibandwidth_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,44 };
    const oid wanipaddress_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,2 };
    const oid wannetmask_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,3 };
    const oid wangateway_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,4 };
    const oid wandns_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,5 };
    const oid wanseconddns_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,6 };
    const oid wanconnectionstatus_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,7 };
    const oid wantype_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,8 };
    const oid wanmacaddress_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,17 };
    const oid lanmacaddress_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,18 };
    const oid wanstaticipaddress_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,19 };
    const oid wanstaticnetmask_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,20 };
    const oid wanstaticgateway_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,21 };
    const oid wanstaticdns_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,22 };
    const oid wanstaticdns2_oid[] = { 1,3,6,1,4,1,46227,2,4,1,1,23 };
    const oid manufacture_oid[] = { 1,3,6,1,4,1,46227,2,5,1,1,1 };
    const oid hardwaremodel_oid[] = { 1,3,6,1,4,1,46227,2,5,1,1,2 };
    const oid softwareversion_oid[] = { 1,3,6,1,4,1,46227,2,5,1,1,3 };
    const oid upgradingstart_oid[] = { 1,3,6,1,4,1,46227,2,5,1,2,1 };
    const oid rebootafterupgrading_oid[] = { 1,3,6,1,4,1,46227,2,5,1,2,2 };
    const oid imagefilename_oid[] = { 1,3,6,1,4,1,46227,2,5,1,3,1 };
    const oid serverip_oid[] = { 1,3,6,1,4,1,46227,2,5,1,3,2 };
    const oid serverport_oid[] = { 1,3,6,1,4,1,46227,2,5,1,3,3 };
#if 0
    const oid vlanenabled_oid[] = { 1,3,6,1,4,1,46227,2,6,1,1,1 };
    const oid bridgedvlanid_oid[] = { 1,3,6,1,4,1,46227,2,6,1,1,2 };
    const oid natvlanid_oid[] = { 1,3,6,1,4,1,46227,2,6,1,1,3 };
#endif
    const oid vlanenabled_oid[] = { 1,3,6,1,4,1,46227,2,3,1,1,45 };

    DEBUGMSGTL(("gxgdWiFiModule", "Initializing\n"));

    netsnmp_register_scalar(
            netsnmp_create_handler_registration("loginusername", handle_loginusername,
                loginusername_oid, OID_LENGTH(loginusername_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("loginpassword", handle_loginpassword,
                loginpassword_oid, OID_LENGTH(loginpassword_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("lanipaddress", handle_lanipaddress,
                lanipaddress_oid, OID_LENGTH(lanipaddress_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("lannetmask", handle_lannetmask,
                lannetmask_oid, OID_LENGTH(lannetmask_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("reset", handle_reset,
                reset_oid, OID_LENGTH(reset_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("factoryrestore", handle_factoryrestore,
                factoryrestore_oid, OID_LENGTH(factoryrestore_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("txpower", handle_txpower,
                txpower_oid, OID_LENGTH(txpower_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wifimode", handle_wifimode,
                wifimode_oid, OID_LENGTH(wifimode_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("channel", handle_channel,
                channel_oid, OID_LENGTH(channel_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("ssidnumber", handle_ssidnumber,
                ssidnumber_oid, OID_LENGTH(ssidnumber_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("applysettings", handle_applysettings,
                applysettings_oid, OID_LENGTH(applysettings_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("pppoeusername", handle_pppoeusername,
                pppoeusername_oid, OID_LENGTH(pppoeusername_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("pppoepassword", handle_pppoepassword,
                pppoepassword_oid, OID_LENGTH(pppoepassword_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("pppoeservicename", handle_pppoeservicename,
                pppoeservicename_oid, OID_LENGTH(pppoeservicename_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("pppoeconnectionstatus", handle_pppoeconnectionstatus,
                pppoeconnectionstatus_oid, OID_LENGTH(pppoeconnectionstatus_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("pppoeon", handle_pppoeon,
                pppoeon_oid, OID_LENGTH(pppoeon_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wanon", handle_wanon,
                wanon_oid, OID_LENGTH(wanon_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wandhcpoption60", handle_wandhcpoption60,
                wandhcpoption60_oid, OID_LENGTH(wandhcpoption60_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wirelessmode", handle_wirelessmode,
                wirelessmode_oid, OID_LENGTH(wirelessmode_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("pppoeconnectondemand", handle_pppoeconnectondemand,
                pppoeconnectondemand_oid, OID_LENGTH(pppoeconnectondemand_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("pppoedisconnect", handle_pppoedisconnect,
                pppoedisconnect_oid, OID_LENGTH(pppoedisconnect_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wpsenable", handle_wpsenable,
                wpsenable_oid, OID_LENGTH(wpsenable_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("dhcpenable", handle_dhcpenable,
                dhcpenable_oid, OID_LENGTH(dhcpenable_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("dhcpaddresstart", handle_dhcpaddresstart,
                dhcpaddresstart_oid, OID_LENGTH(dhcpaddresstart_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("dhcpaddressend", handle_dhcpaddressend,
                dhcpaddressend_oid, OID_LENGTH(dhcpaddressend_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("dhcpnetmask", handle_dhcpnetmask,
                dhcpnetmask_oid, OID_LENGTH(dhcpnetmask_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("dhcpgateway", handle_dhcpgateway,
                dhcpgateway_oid, OID_LENGTH(dhcpgateway_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("dhcpleasetime", handle_dhcpleasetime,
                dhcpleasetime_oid, OID_LENGTH(dhcpleasetime_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wifibandwidth", handle_wifibandwidth,
                wifibandwidth_oid, OID_LENGTH(wifibandwidth_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wanipaddress", handle_wanipaddress,
                wanipaddress_oid, OID_LENGTH(wanipaddress_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wannetmask", handle_wannetmask,
                wannetmask_oid, OID_LENGTH(wannetmask_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wangateway", handle_wangateway,
                wangateway_oid, OID_LENGTH(wangateway_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wandns", handle_wandns,
                wandns_oid, OID_LENGTH(wandns_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wanseconddns", handle_wanseconddns,
                wanseconddns_oid, OID_LENGTH(wanseconddns_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wanconnectionstatus", handle_wanconnectionstatus,
                wanconnectionstatus_oid, OID_LENGTH(wanconnectionstatus_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wantype", handle_wantype,
                wantype_oid, OID_LENGTH(wantype_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wanmacaddress", handle_wanmacaddress,
                wanmacaddress_oid, OID_LENGTH(wanmacaddress_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("lanmacaddress", handle_lanmacaddress,
                lanmacaddress_oid, OID_LENGTH(lanmacaddress_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wanstaticipaddress", handle_wanstaticipaddress,
                wanstaticipaddress_oid, OID_LENGTH(wanstaticipaddress_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wanstaticnetmask", handle_wanstaticnetmask,
                wanstaticnetmask_oid, OID_LENGTH(wanstaticnetmask_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wanstaticgateway", handle_wanstaticgateway,
                wanstaticgateway_oid, OID_LENGTH(wanstaticgateway_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wanstaticdns", handle_wanstaticdns,
                wanstaticdns_oid, OID_LENGTH(wanstaticdns_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("wanstaticdns2", handle_wanstaticdns2,
                wanstaticdns2_oid, OID_LENGTH(wanstaticdns2_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("manufacture", handle_manufacture,
                manufacture_oid, OID_LENGTH(manufacture_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("hardwaremodel", handle_hardwaremodel,
                hardwaremodel_oid, OID_LENGTH(hardwaremodel_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("softwareversion", handle_softwareversion,
                softwareversion_oid, OID_LENGTH(softwareversion_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("upgradingstart", handle_upgradingstart,
                upgradingstart_oid, OID_LENGTH(upgradingstart_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("rebootafterupgrading", handle_rebootafterupgrading,
                rebootafterupgrading_oid, OID_LENGTH(rebootafterupgrading_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("imagefilename", handle_imagefilename,
                imagefilename_oid, OID_LENGTH(imagefilename_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("serverip", handle_serverip,
                serverip_oid, OID_LENGTH(serverip_oid),
                HANDLER_CAN_RWRITE
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("serverport", handle_serverport,
                serverport_oid, OID_LENGTH(serverport_oid),
                HANDLER_CAN_RWRITE
                ));
#if 0
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("vlanenabled", handle_vlanenabled,
                vlanenabled_oid, OID_LENGTH(vlanenabled_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("bridgedvlanid", handle_bridgedvlanid,
                bridgedvlanid_oid, OID_LENGTH(bridgedvlanid_oid),
                HANDLER_CAN_RONLY
                ));
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("natvlanid", handle_natvlanid,
                natvlanid_oid, OID_LENGTH(natvlanid_oid),
                HANDLER_CAN_RONLY
                ));
#endif
    netsnmp_register_scalar(
            netsnmp_create_handler_registration("vlanenabled", handle_vlanenabled,
                vlanenabled_oid, OID_LENGTH(vlanenabled_oid),
                HANDLER_CAN_RONLY
                ));

    /* here we initialize all the tables we're planning on supporting */
    initialize_table_wirelessTable();
    initialize_table_dhcpclientTable();
    initialize_table_wifistationTable();
    initialize_table_vlanTable();
    initialize_table_vidTable();
}

    int
handle_loginusername(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, length;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("http_username");
            length = strlen(pval);
            if(length <= MAXUSRNMLEN)
                snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                        pval, length);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_OCTET_STR);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if(requests->requestvb->val_len <= MAXUSRNMLEN)
                nvram_nset("http_username", requests->requestvb->val.string, \
                        requests->requestvb->val_len);
            else{
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGLENGTH);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_loginusername\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_loginpassword(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, length;
    char *pval;
    char tmpbuf[32];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("http_passwd");
            length = strlen(pval);
            if(length <= MAXUSRNMLEN)
                snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                        pval, length);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_OCTET_STR);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if(requests->requestvb->val_len <= MAXUSRNMLEN){
                nvram_nset("http_passwd", requests->requestvb->val.string,\
                        requests->requestvb->val_len);
            }
            else{
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGLENGTH);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_loginpassword\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_lanipaddress(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */
    int ret;
    struct in_addr addr;
    char *pval;
    char tmpbuf[32], ipaddr[32];
    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("lan_ipaddr");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            /* XXX: perform the value change here */
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);
            if(pval != NULL){
                nvram_set("lan_ipaddr", pval);

                bzero(tmpbuf,sizeof(tmpbuf));
                strcpy(tmpbuf, nvram_safe_get("lan_netmask"));

                bzero(ipaddr,sizeof(ipaddr));
                strcpy(ipaddr, pval);

                updatedefaultgw(ipaddr, tmpbuf);

                updatedhcpaddress(ipaddr, tmpbuf);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_lanipaddress\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_lannetmask(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */
    int ret;
    struct in_addr addr;
    char *pval;
    char tmpbuf[32], netmask[32];
    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("lan_netmask");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);
            if((pval != NULL) && (~(ntohl(addr.s_addr)) > DHCPMINIMUMPOOLSIZE)){
                nvram_set("lan_netmask", pval);

                bzero(tmpbuf,sizeof(tmpbuf));
                strcpy(tmpbuf, nvram_safe_get("lan_ipaddr"));

                bzero(netmask,sizeof(netmask));
                strcpy(netmask, pval);

                updatedefaultgw(tmpbuf, netmask);

                updatedhcpaddress(tmpbuf, netmask);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_lannetmask\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_reset(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                    SNMPFALSE);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if (*requests->requestvb->val.integer == SNMPTRUE) {
                sys_reboot();
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_reset\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_factoryrestore(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                    SNMPFALSE);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if (*requests->requestvb->val.integer == SNMPTRUE) {
                factoryrestorestatus = SNMPTRUE;
                eval("erase", "nvram");
                factoryrestorestatus = 0;
                sys_reboot();
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_factoryrestore\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

int gettxpower(void)
{
    FILE *fp;
    char power[8];
    int ipower = -1;

    fp = popen("/usr/sbin/wl -i eth1 pwr_percent", "r");

    if (fp != NULL) {
        if(fgets(power, sizeof(power), fp) != NULL) {
            ipower = atoi(power);
        }
    }

    pclose(fp);

    return ipower;
}

bool settxpower(int power)
{
    int ret = false;
    char spower[8];

    if(power < 0 || power > 100)
        return false;

    bzero(spower, sizeof(spower));
    sprintf(spower, "%d", power);

    eval("/usr/sbin/wl", "-i", "eth1", "pwr_percent", spower);
    return true;
}

    int
handle_txpower(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    char *pval;
    int txpower;
    char tmpbuf[8];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            txpower = gettxpower();

            if(txpower > 0 && txpower < 100)
                snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                        txpower);
            else
                snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                        100);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            txpower = *requests->requestvb->val.integer;
            if (txpower < 0 || txpower > 100) {
                netsnmp_set_request_error(reqinfo, requests,SNMP_ERR_BADVALUE);
            }
            else{
                settxpower(txpower);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_txpower\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wifimode(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                    WIFI_80211bgn);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wifimode\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_channel(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    char *pchan;
    int fchan, channelupper = UPPER_CHNANNEL_20M;
    char tmpbuf[8];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pchan = nvram_safe_get("wl0_channel");
            fchan = atoi(pchan);

            if(fchan <= 13 && fchan >= 1)
                snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                        fchan);
            else
                snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                        0);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            fchan = *requests->requestvb->val.integer;

            if(nvram_match("wl0_nbw_cap", "1"))
                channelupper = UPPER_CHNANNEL_40M;

            if (fchan < 1 || fchan > channelupper) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            else{
                bzero(tmpbuf, sizeof(tmpbuf));
                snprintf(tmpbuf, sizeof(tmpbuf), "%d", fchan);
                nvram_set("wl_channel",tmpbuf);
                nvram_set("wl0_channel",tmpbuf);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_channel\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_ssidnumber(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, LW_SSID_NUMBER);
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_ssidnumber\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_applysettings(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, SNMPFALSE);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if (*requests->requestvb->val.integer != APPLY_ALLSETTINGS &&
                    *requests->requestvb->val.integer != APPLY_WIFISETTINGS &&
                    *requests->requestvb->val.integer != APPLY_WANSETTINGS ) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            else{
                eval("nvram", "commit");
                sys_restart();
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_applysettings\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_pppoeusername(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, length;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan_pppoe_username");
            length = strlen(pval);
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                    pval, length);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_OCTET_STR);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if(requests->requestvb->val_len <= MAXUSRNMLEN)
                nvram_nset("wan_pppoe_username", requests->requestvb->val.string, \
                        requests->requestvb->val_len);
            else{
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGLENGTH);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_pppoeusername\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_pppoepassword(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, length;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan_pppoe_passwd");
            length = strlen(pval);
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                    pval,length);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_OCTET_STR);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if(requests->requestvb->val_len <= MAXUSRNMLEN)
                nvram_nset("wan_pppoe_passwd", requests->requestvb->val.string, \
                        requests->requestvb->val_len);
            else{
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGLENGTH);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_pppoepassword\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_pppoeservicename(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, length;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan_pppoe_service");
            length = strlen(pval);
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR, pval, length);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_OCTET_STR);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            nvram_nset("wan_pppoe_service",requests->requestvb->val.string, \
                    requests->requestvb->val_len);
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_pppoeservicename\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_pppoeconnectionstatus(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, pppoest;
    char *wan_ifname;
    char tmpbuf[32];
    FILE *fp = NULL;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            wan_ifname = nvram_safe_get("wan0_pppoe_ifname");
            bzero(tmpbuf, sizeof(tmpbuf));
            strcpy(tmpbuf, "/tmp/ppp/link.");
            strcat(tmpbuf, wan_ifname);
            if(fp=fopen(tmpbuf, "r")){
                pppoest = PPPOEST_CONNECTED;
            }
            else
                pppoest = PPPOEST_NOTENABLEDFORINTERNET;

            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, pppoest);
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_pppoeconnectionstatus\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_pppoeon(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, SNMPFALSE);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if(*requests->requestvb->val.integer == SNMPTRUE){
                nvram_set("wan_proto", "pppoe");
                eval("nvram", "commit");
                sys_restart();
            }
            else{
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_pppoeon\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wanon(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, SNMPFALSE);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if(*requests->requestvb->val.integer == SNMPTRUE){
                eval("nvram", "commit");
                sys_restart();
            }
            else{
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wanon\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wandhcpoption60(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, length;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan_dhcpoption60");
            length = strlen(pval);
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR, pval, length);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_OCTET_STR);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            nvram_nset("wan_dhcpoption60",requests->requestvb->val.string, \
                    requests->requestvb->val_len);
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_pppoeservicename\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wirelessmode(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                    WIFI_ROUTER);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        case MODE_SET_COMMIT:
            snmp_log(LOG_INFO, "try to set wifi mode, not supported\n");
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wirelessmode\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_pppoeconnectondemand(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, length, ondemond;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan_pppoe_demond");
            if(strcmp(pval, "1") == 0)
                ondemond = PPPOE_ONDEMAND;
            else 
                ondemond = PPPOE_MANUAL;

            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,ondemond);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            ondemond = *requests->requestvb->val.integer;
            if (ondemond == PPPOE_ONDEMAND) {
                nvram_set("wan_pppoe_demond", "1");
            }
            else if(ondemond == PPPOE_MANUAL || 
                    ondemond == PPPOE_CONTINUES){
                nvram_set("wan_pppoe_demond", "0");
            }
            else{
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_pppoeconnectondemand\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_pppoedisconnect(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, SNMPFALSE);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if(*requests->requestvb->val.integer == SNMPTRUE){
                stop_wan();
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_pppoedisconnect\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wpsenable(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, length;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wps_mode");
            if(nvram_match("wps_mode", "1"))
                snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, SNMPTRUE);
            else if(nvram_match("wps_mode", "0"))
                snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, SNMPFALSE);
            else
                snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, SNMPFALSE);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
#if (WPS_DONE)
            if (requests->requestvb->val.integer == SNMPTRUE) {
                nvram_set("wps_mode", "enable")
            }
            else if (requests->requestvb->val.integer == SNMPFALSE) {
                nvram_set("wps_mode", "disable")
            }
            else
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
#endif
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wpsenable\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_dhcpenable(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, dhcpenabled;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            if(nvram_match("lan_proto", "dhcp"))
                dhcpenabled = SNMPTRUE;
            else 
                dhcpenabled = SNMPFALSE;

            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, dhcpenabled);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if (*requests->requestvb->val.integer == SNMPTRUE) {
                nvram_set("lan_proto", "dhcp");
            }
            else if (*requests->requestvb->val.integer == SNMPFALSE) {
                nvram_set("lan_proto", "static");
            }
            else
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_dhcpenable\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_dhcpaddresstart(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    char lanip[16], netmask[16], dhcpstart[16];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("dhcp_start");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);
            bzero(dhcpstart, sizeof(dhcpstart));
            strcpy(dhcpstart, pval);
            if((pval != NULL) && validatedhcpaddress(dhcpstart)){
                nvram_set("dhcp_start", pval);

                bzero(lanip, sizeof(lanip));
                bzero(netmask, sizeof(netmask));

                strcpy(lanip, nvram_safe_get("lan_ipaddr"));
                strcpy(netmask, nvram_safe_get("lan_netmask"));

                updatedhcpaddress(lanip, netmask);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_dhcpaddresstart\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_dhcpaddressend(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    char lanip[16], netmask[16], dhcpend[16];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("dhcp_end");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);

            bzero(dhcpend, sizeof(dhcpend));
            strcpy(dhcpend, pval);

            if((pval != NULL) && validatedhcpaddress(dhcpend)){
                nvram_set("dhcp_end", pval);

                bzero(lanip, sizeof(lanip));
                bzero(netmask, sizeof(netmask));

                strcpy(lanip, nvram_safe_get("lan_ipaddr"));
                strcpy(netmask, nvram_safe_get("lan_netmask"));

                updatedhcpaddress(lanip, netmask);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_dhcpaddressend\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_dhcpnetmask(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    char lanip[16], netmask[16];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("lan_netmask");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);
            if((pval != NULL) && (~(ntohl(addr.s_addr)) > DHCPMINIMUMPOOLSIZE)){
                nvram_set("lan_netmask", pval);

                bzero(lanip, sizeof(lanip));
                strcpy(lanip, nvram_safe_get("lan_ipaddr"));
                bzero(netmask, sizeof(netmask));
                strcpy(netmask, pval);

                updatedefaultgw(lanip, netmask);
                updatedhcpaddress(lanip, netmask);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_dhcpnetmask\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_dhcpgateway(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    char lanip[16], netmask[16], gateway[16];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("lan_gateway");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);

            bzero(gateway, sizeof(gateway));
            strcpy(gateway, pval);

            if((pval != NULL) && validatedhcpaddress(gateway)){
                nvram_set("lan_gateway", pval);

                bzero(lanip, sizeof(lanip));
                bzero(netmask, sizeof(netmask));

                strcpy(lanip, nvram_safe_get("lan_ipaddr"));
                strcpy(netmask, nvram_safe_get("lan_netmask"));

                updatedefaultgw(lanip, netmask);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_dhcpgateway\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_dhcpleasetime(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, leasetime;
    char *pval;
    char tmpbuf[32];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("lan_lease");
            leasetime = atoi(pval);
            snmp_set_var_typed_integer(requests->requestvb, ASN_TIMETICKS,
                    leasetime * 100);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_TIMETICKS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            leasetime = *requests->requestvb->val.integer/100;
            bzero(tmpbuf, sizeof(tmpbuf));
            sprintf(tmpbuf, "%d", leasetime);
            nvram_set("lan_lease", tmpbuf);
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_dhcpleasetime\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wifibandwidth(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, bandwidth;
    char *pnval;
    char tmpbuf[8];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            if(nvram_match("wl0_nbw_cap", "0"))
                bandwidth = CHANNEL_20M_24G;
            else if(nvram_match("wl0_nbw_cap", "1"))
                bandwidth = CHANNEL_40M_24G;
            else if(nvram_match("wl0_nbw_cap", "2"))
                bandwidth = CHANNEL_20M40M_AUTO;
            else
                bandwidth = CHANNEL_NOTSP_BDW;

            if(bandwidth != CHANNEL_NOTSP_BDW)
                snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                        bandwidth);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            bandwidth = *requests->requestvb->val.integer;

            if (bandwidth != CHANNEL_20M_24G && bandwidth != CHANNEL_40M_24G \
                    && bandwidth != CHANNEL_20M40M_AUTO) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            else{
                bzero(tmpbuf, sizeof(tmpbuf));

                if(bandwidth == CHANNEL_20M_24G)
                    snprintf(tmpbuf, sizeof(tmpbuf), "%d", 0);
                else if(bandwidth == CHANNEL_40M_24G)
                    snprintf(tmpbuf, sizeof(tmpbuf), "%d", 1);
                else if(bandwidth == CHANNEL_20M40M_AUTO)
                    snprintf(tmpbuf, sizeof(tmpbuf), "%d", 2);

                nvram_set("wl_nbw_cap", tmpbuf);
                nvram_set("wl0_nbw_cap", tmpbuf);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wifibandwidth\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wanipaddress(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan0_ipaddr");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wanipaddress\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wannetmask(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan0_netmask");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wannetmask\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wangateway(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan0_gateway");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wangateway\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

static char * nvram_list_fetch(char *name, int pos)
{
	char word[256], *next=NULL;

	foreach(word, nvram_safe_get(name), next) {
		if (pos-- == 0)
			return word;
	}

	return NULL;
}

    int
handle_wandns(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_list_fetch("wan0_dns", 0);
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wandns\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wanseconddns(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_list_fetch("wan0_dns",1);
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wanseconddns\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wanconnectionstatus(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    int dhcpstatus;
    switch(reqinfo->mode) {

        case MODE_GET:
            if(nvram_match("wan_dhcp_status", "complete"))
                dhcpstatus = WANDHCPOK;
            else if(nvram_match("wan_dhcp_status", "down"))
                dhcpstatus = WANDHCPNOK;

            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                    dhcpstatus);
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;


        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wanconnectionstatus\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wantype(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, wanproto;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan_proto");
            if(strcmp(pval, "dhcp") == 0)
                wanproto = WANPROTO_DHCP;
            else if(strcmp(pval, "pppoe") == 0)
                wanproto = WANPROTO_PPPOE;
            else if(strcmp(pval, "static") == 0)
                wanproto = WANPROTO_STATIC;
            else 
                wanproto = WANPROTO_DHCP;

            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, wanproto);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if(*requests->requestvb->val.integer == WANPROTO_STATIC){
                nvram_set("wan_proto", "static");
            }
            else if(*requests->requestvb->val.integer == WANPROTO_DHCP){
                nvram_set("wan_proto", "dhcp");
            }
            else if(*requests->requestvb->val.integer == WANPROTO_DHCP){
                nvram_set("wan_proto", "pppoe");
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wantype\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wanmacaddress(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    char *mac;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            mac = nvram_safe_get("wan0_hwaddr");
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                    mac,
                    strlen(mac));
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wanmacaddress\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_lanmacaddress(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    char *mac;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            mac = nvram_safe_get("lan_hwaddr");
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                    mac,
                    strlen(mac));
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_lanmacaddress\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wanstaticipaddress(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan0_ipaddr");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);
            if(pval != NULL){
                nvram_set("wan_proto", "static");
                nvram_set("wan0_ipaddr", pval);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wanstaticipaddress\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wanstaticnetmask(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan0_netmask");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);
            if(pval != NULL){
                nvram_set("wan_proto", "static");
                nvram_set("wan0_netmask", pval);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wanstaticnetmask\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wanstaticgateway(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("wan0_gateway");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);
            if(pval != NULL){
                nvram_set("wan_proto", "static");
                nvram_set("wan0_gateway", pval);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wanstaticgateway\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wanstaticdns(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_list_fetch("wan0_dns", 0);
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);
            if(pval != NULL){
                nvram_set("wan_proto", "static");
                nvram_set("wan0_dns", pval);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wanstaticdns\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_wanstaticdns2(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_list_fetch("wan0_dns", 1);
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);
            if(pval != NULL){
                nvram_set("wan_proto", "static");
                nvram_set("wan0_dns", pval);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_wanstaticdns2\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_manufacture(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    char *manufact;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
#ifdef VENDOR_BLUENET
            manufact = MANUFACT_LANWANG;
#elif VENDOR_ORIENTVIEW
            manufact = MANUFACT_ORIENTVIEW;
#endif
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                    manufact, strlen(manufact));
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_manufacture\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_hardwaremodel(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                    HARDWAREVER, strlen(HARDWAREVER));
            break;


        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_hardwaremodel\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

bool readswversion(char * version)
{
    char buf[64];
    char *pbuf, *ptoken;
    FILE *fp;

    if(!version)
        return false;

    fp = fopen(FW_FILE, "r");
    if(!fp){
        fclose(fp);
        return false;
    }

    bzero(buf, sizeof(buf));
    fgets(buf, sizeof(buf), fp);
    fclose(fp);

    if(strlen(buf) == 0)
        return false;

    pbuf = buf;
    ptoken = strsep(&pbuf, "@");
    if(!(*pbuf))
        return false;

    pbuf = ptoken;
    while(*ptoken != '\0'){
        if(*ptoken == ' ')
            *ptoken = '\0';
        ptoken++;
    }

    strcpy(version, pbuf);
    return true;
}

    int
handle_softwareversion(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */
    char swversion[32];
    switch(reqinfo->mode) {

        case MODE_GET:
            bzero(swversion, sizeof(swversion));
            if(readswversion(swversion)){
                snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                        swversion, strlen(swversion));
            }
            else
                snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                        SOFTWAREVER, strlen(SOFTWAREVER));
            break;


        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_softwareversion\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

bool isvalidip(char *ipaddr)
{
    int result;
    struct sockaddr_in sa;
    result = inet_pton(AF_INET, ipaddr, &(sa.sin_addr));
    return result != 0;
}

bool isvalidport(int portnum)
{
    return (portnum > 0 && portnum < 65536);
}

bool isvalidimagename(char *imagename)
{
    return (*imagename != '\0');
}

bool upgradelinux(char *serverip, char *serverport, char *image)
{
    bool upgraderet = false;

    eval("rm", "-f", NEWIMGFULLPATH);
    eval("tftp", "-g", "-r", image, "-l", NEWIMGFULLPATH, serverip, serverport);

    if(access(NEWIMGFULLPATH, R_OK) == 0){
        eval("write", NEWIMGFULLPATH, "linux");
        upgraderet = true;
    }

    return upgraderet;
}

    int
handle_upgradingstart(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    char serverip[16], serverport[16], imagename[32];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, SNMPFALSE);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            bzero(serverip, sizeof(serverip));
            bzero(serverport, sizeof(serverport));
            bzero(imagename, sizeof(imagename));

            strcpy(serverip, nvram_safe_get("upgradeserverip"));
            strcpy(serverport, nvram_safe_get("upgradeserverport"));
            strcpy(imagename, nvram_safe_get("upgradefilename"));


            if (isvalidip(serverip) && isvalidport(atoi(serverport)) \
                    && isvalidimagename(imagename)) {
                upgradelinux(serverip, serverport, imagename);
                if(rebootonceupgradedone == SNMPTRUE)
                    sys_reboot();
            }
            else{
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_upgradingstart\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_rebootafterupgrading(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,
                    rebootonceupgradedone);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            if ((*requests->requestvb->val.integer == SNMPTRUE) ||
                    (*requests->requestvb->val.integer == SNMPFALSE)) {
                rebootonceupgradedone = *requests->requestvb->val.integer;
            }
            else
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_rebootafterupgrading\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

//new nvram:upgradefilename
    int
handle_imagefilename(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret,length;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("upgradefilename");
            length = strlen(pval);
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                    pval, length);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_OCTET_STR);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            nvram_nset("upgradefilename", requests->requestvb->val.string, \
                    requests->requestvb->val_len);
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_imagefilename\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

//new nvram:upgradeserverip
    int
handle_serverip(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    struct in_addr addr;
    char *pval;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("upgradeserverip");
            if(inet_aton(pval, &addr))
            {
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            else
            {
                memset(&addr, 0, sizeof(addr));
                snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                        &addr.s_addr, sizeof(addr.s_addr));
            }
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_IPADDRESS);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            addr.s_addr = *((u_long *) requests->requestvb->val.string);
            pval = inet_ntoa(addr);
            if(pval != NULL){
                nvram_set("upgradeserverip", pval);
            }
            else {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_serverip\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

//new nvram:upgradeserverport
    int
handle_serverport(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret, serverport;
    char *pval;
    char tmpbuf[8];
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            pval = nvram_safe_get("upgradeserverport");
            if(strcmp(pval, "") == 0){
                serverport = INVALID_PORT;
            }
            else{
                serverport = atoi(pval);
            }
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, serverport);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            serverport = *requests->requestvb->val.integer;
            if (serverport > 0 && serverport < 65536) {
                bzero(tmpbuf, sizeof(tmpbuf));
                snprintf(tmpbuf, sizeof(tmpbuf), "%d", serverport);
                nvram_set("upgradeserverport", tmpbuf);
            }
            else{
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_serverport\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_vlanenabled(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, SNMPTRUE);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_vlanenabled\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_bridgedvlanid(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,BRIDGEDVID);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_bridgedvlanid\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

    int
handle_natvlanid(netsnmp_mib_handler *handler,
        netsnmp_handler_registration *reginfo,
        netsnmp_agent_request_info   *reqinfo,
        netsnmp_request_info         *requests)
{
    int ret;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER,NATEDVID);
            break;

            /*
             * SET REQUEST
             *
             * multiple states in the transaction.  See:
             * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
             */
        case MODE_SET_RESERVE1:
            /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0/* XXX if malloc, or whatever, failed: */) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_NOTWRITABLE);
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0/* XXX: error? */) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_natvlanid\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

/** Initialize the wirelessTable table by defining its contents and how it's structured */
    void
initialize_table_wirelessTable(void)
{
    const oid wirelessTable_oid[] = {1,3,6,1,4,1,46227,2,3,1,1,9};
    const size_t wirelessTable_oid_len   = OID_LENGTH(wirelessTable_oid);
    netsnmp_handler_registration    *reg;
    netsnmp_iterator_info           *iinfo;
    netsnmp_table_registration_info *table_info;

    DEBUGMSGTL(("gxgdWiFiModule_table:init", "initializing table wirelessTable\n"));

    reg = netsnmp_create_handler_registration(
            "wirelessTable",     wirelessTable_handler,
            wirelessTable_oid, wirelessTable_oid_len,
            HANDLER_CAN_RWRITE
            );

    table_info = SNMP_MALLOC_TYPEDEF( netsnmp_table_registration_info );
    netsnmp_table_helper_add_indexes(table_info,
            ASN_INTEGER,  /* index: ssidindex */
            0);
    table_info->min_column = COLUMN_SSIDTAILER;
    table_info->max_column = COLUMN_SSIDINDEX;

    iinfo = SNMP_MALLOC_TYPEDEF( netsnmp_iterator_info );
    iinfo->get_first_data_point = wirelessTable_get_first_data_point;
    iinfo->get_next_data_point  = wirelessTable_get_next_data_point;
    iinfo->table_reginfo        = table_info;

    netsnmp_register_table_iterator( reg, iinfo );

    /* Initialise the contents of the table here */
    buildwfitable();
}

/* create a new row in the (unsorted) table */
struct wirelessTable_entry * wirelessTable_createEntry(long  ssidindex)
{
    struct wirelessTable_entry *entry;

    entry = SNMP_MALLOC_TYPEDEF(struct wirelessTable_entry);
    if (!entry)
        return NULL;

    entry->ssidindex = ssidindex;
    entry->next = wirelessTable_head;
    wirelessTable_head = entry;
    return entry;
}

/* remove a row from the table */
void
wirelessTable_removeEntry( struct wirelessTable_entry *entry ) {
    struct wirelessTable_entry *ptr, *prev;

    if (!entry)
        return;    /* Nothing to remove */

    for ( ptr  = wirelessTable_head, prev = NULL;
            ptr != NULL;
            prev = ptr, ptr = ptr->next ) {
        if ( ptr == entry )
            break;
    }
    if ( !ptr )
        return;    /* Can't find it */

    if ( prev == NULL )
        wirelessTable_head = ptr->next;
    else
        prev->next = ptr->next;

    SNMP_FREE( entry );   /* XXX - release any other internal resources */
}


/* Example iterator hook routines - using 'get_next' to do most of the work */
    netsnmp_variable_list *
wirelessTable_get_first_data_point(void **my_loop_context,
        void **my_data_context,
        netsnmp_variable_list *put_index_data,
        netsnmp_iterator_info *mydata)
{
    buildwfitable();
    *my_loop_context = wirelessTable_head;
    return wirelessTable_get_next_data_point(my_loop_context, my_data_context,
            put_index_data,  mydata );
}

    netsnmp_variable_list *
wirelessTable_get_next_data_point(void **my_loop_context,
        void **my_data_context,
        netsnmp_variable_list *put_index_data,
        netsnmp_iterator_info *mydata)
{
    struct wirelessTable_entry *entry = (struct wirelessTable_entry *)*my_loop_context;
    netsnmp_variable_list *idx = put_index_data;

    if ( entry ) {
        snmp_set_var_typed_integer( idx, ASN_INTEGER, entry->ssidindex );
        idx = idx->next_variable;
        *my_data_context = (void *)entry;
        *my_loop_context = (void *)entry->next;
        return put_index_data;
    } else {
        return NULL;
    }
}


/** handles requests for the wirelessTable table */
int
wirelessTable_handler(
        netsnmp_mib_handler               *handler,
        netsnmp_handler_registration      *reginfo,
        netsnmp_agent_request_info        *reqinfo,
        netsnmp_request_info              *requests) {

    netsnmp_request_info       *request;
    netsnmp_table_request_info *table_info;
    struct wirelessTable_entry          *table_entry;

    char nvname[16], tmpbuf[16], wlnameprefix[16];
    int wlifindex = 0, newval = 0, ret;

    DEBUGMSGTL(("gxgdWiFiModule_table:handler", "Processing request (%d)\n", reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
        case MODE_GET:
            for (request=requests; request; request=request->next) {
                table_entry = (struct wirelessTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_SSIDTAILER:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_value( request->requestvb, ASN_OCTET_STR,
                                table_entry->ssidtailer,
                                table_entry->ssidtailer_len);
                        break;
                    case COLUMN_ISOLATION:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->isolation);
                        break;
                    case COLUMN_SSIDBROADCAST:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->ssidbroadcast);
                        break;
                    case COLUMN_SECMODE:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->secmode);
                        break;
                    case COLUMN_PASSPHRASE:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_value( request->requestvb, ASN_OCTET_STR,
                                table_entry->passphrase,
                                table_entry->passphrase_len);
                        break;
                    case COLUMN_MAXSTA:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->maxsta);
                        break;
                    case COLUMN_ENCRYPTIONMETHOD:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->encryptionmethod);
                        break;
                    case COLUMN_ENABLED:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->enabled);
                        break;
                    case COLUMN_MACADDRESS:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_value( request->requestvb, ASN_OCTET_STR,
                                table_entry->macaddress,
                                table_entry->macaddress_len);
                        break;
                    case COLUMN_HEADERCHANGABLE:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->headerchangable);
                        break;
                    case COLUMN_SSIDHEADER:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_value( request->requestvb, ASN_OCTET_STR,
                                table_entry->ssidheader,
                                table_entry->ssidheader_len);
                        break;
                    case COLUMN_SSIDINDEX:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->ssidindex);
                        break;
                    default:
                        netsnmp_set_request_error(reqinfo, request,
                                SNMP_NOSUCHOBJECT);
                        break;
                }
            }
            break;

            /*
             * Write-support
             */
        case MODE_SET_RESERVE1:
            for (request=requests; request; request=request->next) {
                table_entry = (struct wirelessTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_SSIDTAILER:
                        /* or possibly 'netsnmp_check_vb_type_and_size' */
                        ret = netsnmp_check_vb_type_and_max_size(
                                request->requestvb, ASN_OCTET_STR, sizeof(table_entry->ssidtailer));
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_ISOLATION:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_SSIDBROADCAST:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_SECMODE:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_PASSPHRASE:
                        /* or possibly 'netsnmp_check_vb_type_and_size' */
                        ret = netsnmp_check_vb_type_and_max_size(
                                request->requestvb, ASN_OCTET_STR, sizeof(table_entry->passphrase));
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_MAXSTA:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_ENCRYPTIONMETHOD:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_ENABLED:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_MACADDRESS:
                        netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        break;
                    case COLUMN_HEADERCHANGABLE:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        if(wlifindex != 0){
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        }
                        else{
                            ret = netsnmp_check_vb_int( request->requestvb );
                            if ( ret != SNMP_ERR_NOERROR ) {
                                netsnmp_set_request_error( reqinfo, request, ret );
                                return SNMP_ERR_NOERROR;
                            }
                        }
                        break;
                    case COLUMN_SSIDHEADER:
                        /* or possibly 'netsnmp_check_vb_type_and_size' */
                        if(wlifindex == 0){
                            ret = netsnmp_check_vb_type_and_max_size(
                                    request->requestvb, ASN_OCTET_STR, MAXSSIDHDRLEN-1);
                            if ( ret != SNMP_ERR_NOERROR ) {
                                netsnmp_set_request_error( reqinfo, request, ret );
                                return SNMP_ERR_NOERROR;
                            }
                        }
                        else{
                            netsnmp_set_request_error( reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        }
                        break;
                    case COLUMN_SSIDINDEX:
                        netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        /*ret = netsnmp_check_vb_int( request->requestvb );
                          if ( ret != SNMP_ERR_NOERROR ) {
                          netsnmp_set_request_error( reqinfo, request, ret );
                          return SNMP_ERR_NOERROR;
                          }*/
                        break;
                    default:
                        netsnmp_set_request_error( reqinfo, request,
                                SNMP_ERR_NOTWRITABLE );
                        return SNMP_ERR_NOERROR;
                }
            }
            break;

        case MODE_SET_RESERVE2:
            break;

        case MODE_SET_FREE:
            break;

        case MODE_SET_ACTION:
            for (request=requests; request; request=request->next) {
                table_entry = (struct wirelessTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                bzero(nvname, sizeof(nvname));
                bzero(tmpbuf, sizeof(tmpbuf));
                bzero(wlnameprefix, sizeof(wlnameprefix));

                wlifindex = table_entry->ssidindex;

                if(wlifindex == 0){
                    strcpy(wlnameprefix, "wl0");
                }
                else{
                    sprintf(wlnameprefix, "wl0.%d", wlifindex);
                }

                switch (table_info->colnum) {
                    case COLUMN_SSIDTAILER:
                        memcpy( table_entry->old_ssidtailer,
                                table_entry->ssidtailer,
                                sizeof(table_entry->ssidtailer));
                        table_entry->old_ssidtailer_len =
                            table_entry->ssidtailer_len;
                        memset( table_entry->ssidtailer, 0,
                                sizeof(table_entry->ssidtailer));
                        memcpy( table_entry->ssidtailer,
                                request->requestvb->val.string,
                                request->requestvb->val_len);
                        table_entry->ssidtailer_len =
                            request->requestvb->val_len;

                        sprintf(nvname, "%s_ssidheader", wlnameprefix);
                        strncpy(tmpbuf, nvram_safe_get(nvname), sizeof(tmpbuf));
                        strncat(tmpbuf,table_entry->ssidtailer,table_entry->ssidtailer_len);

                        bzero(nvname, sizeof(nvname));
                        sprintf(nvname, "%s_ssid", wlnameprefix);
                        nvram_set(nvname, tmpbuf);
                        break;

                    case COLUMN_ISOLATION:
                        table_entry->old_isolation = table_entry->isolation;
                        table_entry->isolation     = *request->requestvb->val.integer;

                        sprintf(nvname, "%s_ap_isolate", wlnameprefix);
                        if(table_entry->isolation == SNMPFALSE)
                            nvram_set(nvname, "0");
                        else if(table_entry->isolation == SNMPTRUE)
                            nvram_set(nvname, "1");
                        else
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_BADVALUE);
                        break;

                    case COLUMN_SSIDBROADCAST:
                        table_entry->old_ssidbroadcast = table_entry->ssidbroadcast;
                        table_entry->ssidbroadcast     = *request->requestvb->val.integer;

                        sprintf(nvname, "%s_closed", wlnameprefix);
                        if(table_entry->ssidbroadcast == SNMPFALSE)
                            nvram_set(nvname, "1");
                        else if(table_entry->ssidbroadcast == SNMPTRUE)
                            nvram_set(nvname, "0");
                        else
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_BADVALUE);
                        break;

                    case COLUMN_SECMODE:
                        table_entry->old_secmode = table_entry->secmode;
                        table_entry->secmode     = *request->requestvb->val.integer;
                        if(!setwlsecuritymode(table_entry->secmode, wlnameprefix, table_entry))
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_BADVALUE);
                        break;

                    case COLUMN_PASSPHRASE:
                        memcpy( table_entry->old_passphrase,
                                table_entry->passphrase,
                                sizeof(table_entry->passphrase));
                        table_entry->old_passphrase_len =
                            table_entry->passphrase_len;
                        memset( table_entry->passphrase, 0,
                                sizeof(table_entry->passphrase));
                        memcpy( table_entry->passphrase,
                                request->requestvb->val.string,
                                request->requestvb->val_len);
                        table_entry->passphrase_len =
                            request->requestvb->val_len;

                        sprintf(nvname, "%s_wpa_psk", wlnameprefix);
                        nvram_set(nvname, table_entry->passphrase);
                        break;

                    case COLUMN_MAXSTA:
                        table_entry->old_maxsta = table_entry->maxsta;
                        table_entry->maxsta     = *request->requestvb->val.integer;

                        sprintf(nvname, "%s_maxassoc", wlnameprefix);
                        if(table_entry->maxsta <= 128 && table_entry->maxsta >= 0){
                            snprintf(tmpbuf, sizeof(tmpbuf), "%d", table_entry->maxsta);
                            nvram_set(nvname,tmpbuf);
                        }
                        else{
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_BADVALUE);
                        }
                        break;

                    case COLUMN_ENCRYPTIONMETHOD:
                        table_entry->old_encryptionmethod = table_entry->encryptionmethod;
                        table_entry->encryptionmethod     = *request->requestvb->val.integer;

                        if(table_entry->encryptionmethod == ENCRY_TKIP \
                                || table_entry->encryptionmethod == ENCRY_AES \
                                || table_entry->encryptionmethod == ENCRY_TKIPAES)
                            setwlencryptionmethod(table_entry->encryptionmethod, wlnameprefix, table_entry);
                        else
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_BADVALUE);
                        break;

                    case COLUMN_ENABLED:
                        table_entry->old_enabled = table_entry->enabled;
                        table_entry->enabled     = *request->requestvb->val.integer;

                        sprintf(nvname, "%s_bss_enabled", wlnameprefix);
                        if(table_entry->enabled == SNMPTRUE){
                            nvram_set(nvname, "1");
                        }
                        else if(table_entry->enabled == SNMPFALSE){
                            nvram_set(nvname, "0");
                        }
                        else{
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_BADVALUE);
                        }
                        break;

                    case COLUMN_MACADDRESS:
                        netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        break;

                    case COLUMN_HEADERCHANGABLE:
                        if(wlifindex == 0){
                            table_entry->old_headerchangable = table_entry->headerchangable;
                            table_entry->headerchangable     = *request->requestvb->val.integer;

                            if(table_entry->headerchangable == SNMPTRUE)
                                nvram_set("wl0_ssidheader_changable", "1");
                            else if(table_entry->headerchangable == SNMPFALSE)
                                nvram_set("wl0_ssidheader_changable", "0");
                            else
                                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_BADVALUE);
                        }
                        else{
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        }
                        break;

                    case COLUMN_SSIDHEADER:
                        if(nvram_invmatch("wl0_ssidheader_changable", "1"))
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        else{
                            if(wlifindex == 0){
                                memcpy( table_entry->old_ssidheader,
                                        table_entry->ssidheader,
                                        sizeof(table_entry->ssidheader));
                                table_entry->old_ssidheader_len =
                                    table_entry->ssidheader_len;
                                memset( table_entry->ssidheader, 0,
                                        sizeof(table_entry->ssidheader));
                                memcpy( table_entry->ssidheader,
                                        request->requestvb->val.string,
                                        request->requestvb->val_len);
                                table_entry->ssidheader_len =
                                    request->requestvb->val_len;

                                nvram_set("wl0_ssidheader", table_entry->ssidheader);
                            }
                            else{
                                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                            }
                        }
                        break;

                    case COLUMN_SSIDINDEX:
                        //table_entry->old_ssidindex = table_entry->ssidindex;
                        //table_entry->ssidindex     = *request->requestvb->val.integer;
                        netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        break;
                }
            }
            break;

        case MODE_SET_UNDO:
            for (request=requests; request; request=request->next) {
                table_entry = (struct wirelessTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_SSIDTAILER:
                        memcpy( table_entry->ssidtailer,
                                table_entry->old_ssidtailer,
                                sizeof(table_entry->ssidtailer));
                        memset( table_entry->old_ssidtailer, 0,
                                sizeof(table_entry->ssidtailer));
                        table_entry->ssidtailer_len =
                            table_entry->old_ssidtailer_len;
                        break;
                    case COLUMN_ISOLATION:
                        table_entry->isolation     = table_entry->old_isolation;
                        table_entry->old_isolation = 0;
                        break;
                    case COLUMN_SSIDBROADCAST:
                        table_entry->ssidbroadcast     = table_entry->old_ssidbroadcast;
                        table_entry->old_ssidbroadcast = 0;
                        break;
                    case COLUMN_SECMODE:
                        table_entry->secmode     = table_entry->old_secmode;
                        table_entry->old_secmode = 0;
                        break;
                    case COLUMN_PASSPHRASE:
                        memcpy( table_entry->passphrase,
                                table_entry->old_passphrase,
                                sizeof(table_entry->passphrase));
                        memset( table_entry->old_passphrase, 0,
                                sizeof(table_entry->passphrase));
                        table_entry->passphrase_len =
                            table_entry->old_passphrase_len;
                        break;
                    case COLUMN_MAXSTA:
                        table_entry->maxsta     = table_entry->old_maxsta;
                        table_entry->old_maxsta = 0;
                        break;
                    case COLUMN_ENCRYPTIONMETHOD:
                        table_entry->encryptionmethod     = table_entry->old_encryptionmethod;
                        table_entry->old_encryptionmethod = 0;
                        break;
                    case COLUMN_ENABLED:
                        table_entry->enabled     = table_entry->old_enabled;
                        table_entry->old_enabled = 0;
                        break;
                    case COLUMN_MACADDRESS:
                        netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        break;
                    case COLUMN_HEADERCHANGABLE:
                        if(wlifindex != 0){
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        }
                        else{
                            table_entry->headerchangable     = table_entry->old_headerchangable;
                            table_entry->old_headerchangable = 0;
                        }
                        break;
                    case COLUMN_SSIDHEADER:
                        if(wlifindex == 0){
                            memcpy( table_entry->ssidheader,
                                    table_entry->old_ssidheader,
                                    sizeof(table_entry->ssidheader));
                            memset( table_entry->old_ssidheader, 0,
                                    sizeof(table_entry->ssidheader));
                            table_entry->ssidheader_len =
                                table_entry->old_ssidheader_len;
                        }
                        else{
                            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        }
                        break;
                    case COLUMN_SSIDINDEX:
                        //table_entry->ssidindex     = table_entry->old_ssidindex;
                        //table_entry->old_ssidindex = 0;
                        netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                        break;
                }
            }
            break;

        case MODE_SET_COMMIT:
            break;
    }
    return SNMP_ERR_NOERROR;
}

bool setwlsecuritymode(int secmode, char *wlnameprefix, struct wirelessTable_entry *entry)
{
    char nvname[16];

    if(secmode != KEYMGMT_WPAPSK && secmode != KEYMGMT_PSKMIXED \
            && secmode != KEYMGMT_WPA2PSK){
        return false;
    }

    bzero(nvname, sizeof(nvname));
    snprintf(nvname, sizeof(nvname), "%s_akm", wlnameprefix);

    switch(secmode){
        case KEYMGMT_WPAPSK:
            nvram_set(nvname, "psk");
            break;
        case KEYMGMT_WPA2PSK:
            nvram_set(nvname, "psk2");
            break;
        case KEYMGMT_PSKMIXED:
            nvram_set(nvname, "psk psk2");
            break;
        default:
            break;
    }

    bzero(nvname, sizeof(nvname));
    snprintf(nvname, sizeof(nvname), "%s_crypto", wlnameprefix);
    if(nvram_invmatch(nvname, "tkip") &&
            nvram_invmatch(nvname, "aes") &&
            nvram_invmatch(nvname, "tkip+aes"))
    {
        nvram_set(nvname, "tkip+aes");
        entry->encryptionmethod = ENCRY_TKIPAES;
    }

    bzero(nvname, sizeof(nvname));
    snprintf(nvname, sizeof(nvname), "%s_wpa_psk", wlnameprefix);
    if(nvram_match(nvname, ""))
    {
        nvram_set(nvname, DEFAULT_PSK_STRING);
        bzero(entry->passphrase, sizeof(entry->passphrase));
        strcpy(entry->passphrase, DEFAULT_PSK_STRING);
        entry->passphrase_len = strlen(entry->passphrase);
    }
    return true;
}

bool setwlencryptionmethod(int encryptmode, char *wlnameprefix, struct wirelessTable_entry *entry)
{
    char nvname[16];
    long secmode = KEYMGMT_DISABLED;

    bzero(nvname, sizeof(nvname));
    snprintf(nvname, sizeof(nvname), "%s_crypto", wlnameprefix);

    switch(encryptmode){
        case ENCRY_TKIP:
            nvram_set(nvname, "tkip");
            break;
        case ENCRY_AES:
            nvram_set(nvname, "aes");
            break;
        case ENCRY_TKIPAES:
            nvram_set(nvname, "tkip+aes");
            break;
        default:
            break;
    }

    getwlsecmethod(&secmode, wlnameprefix);

    if(secmode != KEYMGMT_WPAPSK && secmode != KEYMGMT_WPA2PSK \
            && secmode != KEYMGMT_PSKMIXED){
        bzero(nvname, sizeof(nvname));
        snprintf(nvname, sizeof(nvname), "%s_akm", wlnameprefix);
        nvram_set(nvname, "psk psk2");
        entry->secmode = KEYMGMT_PSKMIXED;
    }

    bzero(nvname, sizeof(nvname));
    snprintf(nvname, sizeof(nvname), "%s_wpa_psk", wlnameprefix);
    if(nvram_match(nvname, "")){
        nvram_set(nvname, DEFAULT_PSK_STRING);
        bzero(entry->passphrase, sizeof(entry->passphrase));
        strcpy(entry->passphrase, DEFAULT_PSK_STRING);
        entry->passphrase_len = strlen(entry->passphrase);
    }
}

bool getssidtailer(char *ssidtailer, char *wlnameprefix)
{
    char ssid[MAXSSIDLEN], nvname[16];

    if(!ssidtailer)
        return false;

    bzero(ssid, sizeof(ssid));
    bzero(nvname, sizeof(nvname));

    snprintf(nvname, sizeof(nvname), "%s_ssid", wlnameprefix);
    strcpy(ssid, nvram_safe_get(nvname));

    bzero(nvname, sizeof(nvname));
    snprintf(nvname, sizeof(nvname), "%s_ssidheader", wlnameprefix);
    strcpy(ssidtailer ,ssid + strlen(nvram_safe_get(nvname)));
    return true;
}

bool getwlsecmethod(long *secmode, char *wlnameprefix)
{
    char nvname[16], tmpbuf[16];
    char *wl_akms, *akmnext;
    bool pskenabled = false, psk2enabled = false;

    bzero(nvname, sizeof(nvname));
    sprintf(nvname, "%s_akm", wlnameprefix);
    wl_akms = nvram_safe_get("wl0_akm");

    foreach(tmpbuf, wl_akms, akmnext){
        if(strcmp(tmpbuf, "psk") == 0){
            pskenabled = true;
        }
        else if(strcmp(tmpbuf, "psk2") == 0){
            psk2enabled = true;
        }
    }

    if(pskenabled && psk2enabled)
        *secmode = KEYMGMT_PSKMIXED;
    else if(pskenabled)
        *secmode = KEYMGMT_WPAPSK;
    else if(psk2enabled)
        *secmode = KEYMGMT_WPA2PSK;
    else
        *secmode = KEYMGMT_DISABLED;

    return true;
}

int buildwfitable(void)
{
    int i, index=0;
    char tmp[32], wlnameprefix[32], nvname[16];
    char *pval;
    struct timeval now;
    struct wirelessTable_entry *entry;

    gettimeofday(&now, NULL);
    if((now.tv_sec - buildwfitabletime.tv_sec) <= REBUILDTABLEINTERVAL){
        //snmp_log(LOG_INFO, "skip to rebuild dhcp client table\n");
        return;
    }
    buildwfitabletime = now;

    while(entry = wirelessTable_head)
        wirelessTable_removeEntry(entry);

    for(i=0; i < LW_SSID_NUMBER; i++){
        entry = wirelessTable_createEntry(i);

        bzero(wlnameprefix, sizeof(wlnameprefix));
        if(i == 0){
            strcpy(wlnameprefix, "wl0");
        }
        else{
            sprintf(wlnameprefix, "wl0.%d", i);
        }

        bzero(tmp, sizeof(tmp));
        getssidtailer(tmp, wlnameprefix);
        strncpy(entry->ssidtailer, tmp, sizeof(entry->ssidtailer));
        entry->ssidtailer_len = strlen(entry->ssidtailer);

        entry->isolation = SNMPFALSE;
        bzero(nvname, sizeof(nvname));
        sprintf(nvname, "%s_ap_isolate", wlnameprefix);
        if(nvram_match(nvname, "1")){
            entry->isolation = SNMPTRUE;
        }

        entry->ssidbroadcast = SNMPFALSE;
        bzero(nvname, sizeof(nvname));
        sprintf(nvname, "%s_closed", wlnameprefix);
        if(nvram_match(nvname, "0")){
            entry->ssidbroadcast = SNMPTRUE;
        }

        getwlsecmethod(&(entry->secmode), wlnameprefix);

        if(entry->secmode == KEYMGMT_WPAPSK || entry->secmode == KEYMGMT_WPA2PSK \
                || entry->secmode == KEYMGMT_PSKMIXED){
            bzero(nvname, sizeof(nvname));
            sprintf(nvname, "%s_wpa_psk", wlnameprefix);
            strncpy(entry->passphrase, nvram_safe_get(nvname), sizeof(entry->passphrase));
            entry->passphrase_len = strlen(entry->passphrase);
        }

        bzero(nvname, sizeof(nvname));
        bzero(tmp, sizeof(tmp));
        sprintf(nvname, "%s_maxassoc", wlnameprefix);
        strcpy(tmp, nvram_safe_get(nvname));
        entry->maxsta = atoi(tmp);


        bzero(nvname, sizeof(nvname));
        bzero(tmp, sizeof(tmp));
        sprintf(nvname, "%s_crypto", wlnameprefix);
        strcpy(tmp, nvram_safe_get(nvname));
        if(strcmp(tmp, "tkip+aes") == 0)
            entry->encryptionmethod = ENCRY_TKIPAES;
        else if(strcmp(tmp, "aes") == 0)
            entry->encryptionmethod = ENCRY_AES;
        else if(strcmp(tmp, "tkip") == 0)
            entry->encryptionmethod = ENCRY_TKIP;
        else
            entry->encryptionmethod = ENCRY_TKIPAES;

        bzero(nvname, sizeof(nvname));
        sprintf(nvname, "%s_bss_enabled", wlnameprefix);
        if(nvram_match(nvname, "1")){
            entry->enabled = SNMPTRUE;
        }
        else{
            entry->enabled = SNMPFALSE;
        }

        bzero(nvname, sizeof(nvname));
        sprintf(nvname, "%s_hwaddr", wlnameprefix);
        strncpy(entry->macaddress, nvram_safe_get(nvname), sizeof(entry->macaddress));
        entry->macaddress_len = strlen(entry->macaddress);

        entry->headerchangable = SNMPFALSE;
        if(i == 0){
            if(nvram_match("wl0_ssidheader_changable", "1"))
                entry->headerchangable = SNMPTRUE;
        }

        if( i == 0 ){
            bzero(tmp, sizeof(tmp));
            strcpy(tmp, nvram_safe_get("wl0_ssidheader"));
            strncpy(entry->ssidheader, tmp, sizeof(entry->ssidheader));
            entry->ssidheader_len = strlen(tmp);
        }
    }
}

/** Initialize the dhcpclientTable table by defining its contents and how it's structured */
    void
initialize_table_dhcpclientTable(void)
{
    const oid dhcpclientTable_oid[] = {1,3,6,1,4,1,46227,2,7,1};
    const size_t dhcpclientTable_oid_len   = OID_LENGTH(dhcpclientTable_oid);
    netsnmp_handler_registration    *reg;
    netsnmp_iterator_info           *iinfo;
    netsnmp_table_registration_info *table_info;

    DEBUGMSGTL(("gxgdWiFiModule_table:init", "initializing table dhcpclientTable\n"));

    reg = netsnmp_create_handler_registration(
            "dhcpclientTable",     dhcpclientTable_handler,
            dhcpclientTable_oid, dhcpclientTable_oid_len,
            HANDLER_CAN_RONLY
            );

    table_info = SNMP_MALLOC_TYPEDEF( netsnmp_table_registration_info );
    netsnmp_table_helper_add_indexes(table_info,
            ASN_INTEGER,  /* index: clientindex */
            0);
    table_info->min_column = COLUMN_CLIENTINDEX;
    table_info->max_column = COLUMN_CLIENTLEASETIME;

    iinfo = SNMP_MALLOC_TYPEDEF( netsnmp_iterator_info );
    iinfo->get_first_data_point = dhcpclientTable_get_first_data_point;
    iinfo->get_next_data_point  = dhcpclientTable_get_next_data_point;
    iinfo->table_reginfo        = table_info;

    netsnmp_register_table_iterator( reg, iinfo );

    /* Initialise the contents of the table here */
    buildhcpclienttable();
}

/* Typical data structure for a row entry */
struct dhcpclientTable_entry {
    /* Index values */
    long clientindex;

    /* Column values */
    char clientmac[MACADDRSTRLEN];
    size_t clientmac_len;
    in_addr_t clientip;
    u_long clientleasetime;

    /* Illustrate using a simple linked list */
    int   valid;
    struct dhcpclientTable_entry *next;
};

struct dhcpclientTable_entry  *dhcpclientTable_head;

/* create a new row in the (unsorted) table */
struct dhcpclientTable_entry *
dhcpclientTable_createEntry(long  clientindex) {
    struct dhcpclientTable_entry *entry;

    entry = SNMP_MALLOC_TYPEDEF(struct dhcpclientTable_entry);
    if (!entry)
        return NULL;

    entry->clientindex = clientindex;
    entry->next = dhcpclientTable_head;
    dhcpclientTable_head = entry;
    return entry;
}

/* remove a row from the table */
void
dhcpclientTable_removeEntry( struct dhcpclientTable_entry *entry ) {
    struct dhcpclientTable_entry *ptr, *prev;

    if (!entry)
        return;    /* Nothing to remove */

    for ( ptr  = dhcpclientTable_head, prev = NULL;
            ptr != NULL;
            prev = ptr, ptr = ptr->next ) {
        if ( ptr == entry )
            break;
    }
    if ( !ptr )
        return;    /* Can't find it */

    if ( prev == NULL )
        dhcpclientTable_head = ptr->next;
    else
        prev->next = ptr->next;

    SNMP_FREE( entry );   /* XXX - release any other internal resources */
}


/* Example iterator hook routines - using 'get_next' to do most of the work */
    netsnmp_variable_list *
dhcpclientTable_get_first_data_point(void **my_loop_context,
        void **my_data_context,
        netsnmp_variable_list *put_index_data,
        netsnmp_iterator_info *mydata)
{
    buildhcpclienttable();
    *my_loop_context = dhcpclientTable_head;
    return dhcpclientTable_get_next_data_point(my_loop_context, my_data_context,
            put_index_data,  mydata );
}

    netsnmp_variable_list *
dhcpclientTable_get_next_data_point(void **my_loop_context,
        void **my_data_context,
        netsnmp_variable_list *put_index_data,
        netsnmp_iterator_info *mydata)
{
    struct dhcpclientTable_entry *entry = (struct dhcpclientTable_entry *)*my_loop_context;
    netsnmp_variable_list *idx = put_index_data;

    if ( entry ) {
        snmp_set_var_typed_integer( idx, ASN_INTEGER, entry->clientindex );
        idx = idx->next_variable;
        *my_data_context = (void *)entry;
        *my_loop_context = (void *)entry->next;
        return put_index_data;
    } else {
        return NULL;
    }
}

    static char *
reltime_short(unsigned int seconds)
{
    static char buf[16];

    sprintf(buf, "%02d:%02d:%02d",
            seconds / 3600,
            (seconds % 3600) / 60,
            seconds % 60);

    return buf;
}

void buildhcpclienttable()
{
    int i, index=0;
    char sigusr1[] = "-XX";
    struct lease_t lease;
    struct dhcpclientTable_entry *entry;
    FILE *fp = NULL;
    struct timeval now;

    gettimeofday(&now, NULL);

    if((now.tv_sec - builddhcpclienttabletime.tv_sec) < REBUILDTABLEINTERVAL){
        //snmp_log(LOG_INFO, "skip to rebuild dhcp client table\n");
        return;
    }

    builddhcpclienttabletime = now;

    while(entry = dhcpclientTable_head)
        dhcpclientTable_removeEntry(entry);

    /* Write out leases file */
    sprintf(sigusr1, "-%d", SIGUSR1);
    eval("killall", sigusr1, "udhcpd");

    if (!(fp = fopen("/tmp/udhcpd0.leases", "r"))){
        snmp_log(LOG_ERR, "fail to open dhcp lease database\n");
        return;
    }

    while (fread(&lease, sizeof(lease), 1, fp)) {
        /* Do not display reserved leases */
        if (ETHER_ISNULLADDR(lease.chaddr))
            continue;

        entry = dhcpclientTable_createEntry(index++);

        ether_etoa(lease.chaddr, entry->clientmac);
        //snmp_log(LOG_ERR, "client mac address:%s\n", entry->clientmac);

        entry->clientip = lease.yiaddr;

        entry->clientleasetime = ntohl(lease.expires);
        snmp_log(LOG_ERR, "remain lease time:%s\n", reltime_short(entry->clientleasetime));
    }

    fclose(fp);
    return;
}

/** handles requests for the dhcpclientTable table */
int
dhcpclientTable_handler(
        netsnmp_mib_handler               *handler,
        netsnmp_handler_registration      *reginfo,
        netsnmp_agent_request_info        *reqinfo,
        netsnmp_request_info              *requests) {

    netsnmp_request_info       *request;
    netsnmp_table_request_info *table_info;
    struct dhcpclientTable_entry          *table_entry;

    DEBUGMSGTL(("gxgdWiFiModule_table:handler", "Processing request (%d)\n", reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
        case MODE_GET:
            for (request=requests; request; request=request->next) {
                table_entry = (struct dhcpclientTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_CLIENTINDEX:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->clientindex);
                        break;
                    case COLUMN_CLIENTMAC:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_value( request->requestvb, ASN_OCTET_STR,
                                table_entry->clientmac,
                                MACADDRSTRLEN);
                        break;
                    case COLUMN_CLIENTIP:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_IPADDRESS,
                                table_entry->clientip);
                        break;
                    case COLUMN_CLIENTLEASETIME:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_TIMETICKS,
                                table_entry->clientleasetime);
                        break;
                    default:
                        netsnmp_set_request_error(reqinfo, request,
                                SNMP_NOSUCHOBJECT);
                        break;
                }
            }
            break;

    }
    return SNMP_ERR_NOERROR;
}

/** Initialize the wifistationTable table by defining its contents and how it's structured */
    void
initialize_table_wifistationTable(void)
{
    const oid wifistationTable_oid[] = {1,3,6,1,4,1,46227,2,7,2};
    const size_t wifistationTable_oid_len   = OID_LENGTH(wifistationTable_oid);
    netsnmp_handler_registration    *reg;
    netsnmp_iterator_info           *iinfo;
    netsnmp_table_registration_info *table_info;

    DEBUGMSGTL(("gxgdWiFiModule_table:init", "initializing table wifistationTable\n"));

    reg = netsnmp_create_handler_registration(
            "wifistationTable",     wifistationTable_handler,
            wifistationTable_oid, wifistationTable_oid_len,
            HANDLER_CAN_RONLY
            );

    table_info = SNMP_MALLOC_TYPEDEF( netsnmp_table_registration_info );
    netsnmp_table_helper_add_indexes(table_info,
            ASN_INTEGER,  /* index: stationindex */
            0);
    table_info->min_column = COLUMN_STATIONINDEX;
    table_info->max_column = COLUMN_ASSOCTIME;

    iinfo = SNMP_MALLOC_TYPEDEF( netsnmp_iterator_info );
    iinfo->get_first_data_point = wifistationTable_get_first_data_point;
    iinfo->get_next_data_point  = wifistationTable_get_next_data_point;
    iinfo->table_reginfo        = table_info;

    netsnmp_register_table_iterator( reg, iinfo );

    /* Initialise the contents of the table here */
    buildwifistationtable();
}

/* Typical data structure for a row entry */
struct wifistationTable_entry {
    /* Index values */
    long stationindex;

    /* Column values */
    char stationmac[MACADDRSTRLEN];
    size_t stationmac_len;
    u_long assoctime;

    /* Illustrate using a simple linked list */
    int   valid;
    struct wifistationTable_entry *next;
};

struct wifistationTable_entry  *wifistationTable_head;

/* create a new row in the (unsorted) table */
struct wifistationTable_entry *
wifistationTable_createEntry(long  stationindex) {
    struct wifistationTable_entry *entry;

    entry = SNMP_MALLOC_TYPEDEF(struct wifistationTable_entry);
    if (!entry)
        return NULL;

    entry->stationindex = stationindex;
    entry->next = wifistationTable_head;
    wifistationTable_head = entry;
    return entry;
}

/* remove a row from the table */
void
wifistationTable_removeEntry( struct wifistationTable_entry *entry ) {
    struct wifistationTable_entry *ptr, *prev;

    if (!entry)
        return;    /* Nothing to remove */

    for ( ptr  = wifistationTable_head, prev = NULL;
            ptr != NULL;
            prev = ptr, ptr = ptr->next ) {
        if ( ptr == entry )
            break;
    }
    if ( !ptr )
        return;    /* Can't find it */

    if ( prev == NULL )
        wifistationTable_head = ptr->next;
    else
        prev->next = ptr->next;

    SNMP_FREE( entry );   /* XXX - release any other internal resources */
}


/* Example iterator hook routines - using 'get_next' to do most of the work */
    netsnmp_variable_list *
wifistationTable_get_first_data_point(void **my_loop_context,
        void **my_data_context,
        netsnmp_variable_list *put_index_data,
        netsnmp_iterator_info *mydata)
{
    buildwifistationtable();
    *my_loop_context = wifistationTable_head;
    return wifistationTable_get_next_data_point(my_loop_context, my_data_context,
            put_index_data,  mydata );
}

    netsnmp_variable_list *
wifistationTable_get_next_data_point(void **my_loop_context,
        void **my_data_context,
        netsnmp_variable_list *put_index_data,
        netsnmp_iterator_info *mydata)
{
    struct wifistationTable_entry *entry = (struct wifistationTable_entry *)*my_loop_context;
    netsnmp_variable_list *idx = put_index_data;

    if ( entry ) {
        snmp_set_var_typed_integer( idx, ASN_INTEGER, entry->stationindex );
        idx = idx->next_variable;
        *my_data_context = (void *)entry;
        *my_loop_context = (void *)entry->next;
        return put_index_data;
    } else {
        return NULL;
    }
}

void buildwifistationtable()
{
    int mac_list_size, i;
    char buf[sizeof(sta_info_t)];
    char *ifname;
    char ea_str[MACADDRSTRLEN];
    struct maclist *mac_list;
    struct wifistationTable_entry *entry;
    struct timeval now;

#if 1
    gettimeofday(&now, NULL);

    if((now.tv_sec - buildwifistationtabletime.tv_sec) < REBUILDTABLEINTERVAL){
        //snmp_log(LOG_INFO, "skip to rebuild wifi station table\n");
        return;
    }

    buildwifistationtabletime = now;
#endif

    while(entry = wifistationTable_head)
        wifistationTable_removeEntry(entry);

    mac_list_size = sizeof(mac_list->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
    mac_list = malloc(mac_list_size);

    if (!mac_list){
        snmp_log(LOG_ERR, "fail to allocate mac list buffer\n");
        return;
    }

    ifname = nvram_safe_get("wl0_ifname");
    if(!strcmp(ifname, "")){
        free(mac_list);
        snmp_log(LOG_ERR, "fail to get wireless interface name\n");
        return;
    }

    strcpy((char*)mac_list, "authe_sta_list");
    if (wl_ioctl(ifname, WLC_GET_VAR, mac_list, mac_list_size)) {
        free(mac_list);
        snmp_log(LOG_ERR, "fail to get mac list from wifi driver\n");
        return;
    }

    for (i = 0; i < mac_list->count; i++) {
        bzero(buf, sizeof(buf));
        strcpy(buf, "sta_info");
        memcpy(buf + strlen(buf) + 1,(unsigned char *) &(mac_list->ea[i]), ETHER_ADDR_LEN);

        if (!wl_ioctl(ifname, WLC_GET_VAR, buf, sizeof(buf))) {
            sta_info_t *sta = (sta_info_t *)buf;
            uint32 f = sta->flags;

            bzero(ea_str, sizeof(ea_str));
            ether_etoa((unsigned char *)&mac_list->ea[i], ea_str);

            entry = wifistationTable_createEntry(i);
            memcpy(entry->stationmac,ea_str, MACADDRSTRLEN);
            entry->assoctime = sta->in;
        }
    }
    free(mac_list);

    return;
}

/** handles requests for the wifistationTable table */
int
wifistationTable_handler(
        netsnmp_mib_handler               *handler,
        netsnmp_handler_registration      *reginfo,
        netsnmp_agent_request_info        *reqinfo,
        netsnmp_request_info              *requests) {

    netsnmp_request_info       *request;
    netsnmp_table_request_info *table_info;
    struct wifistationTable_entry          *table_entry;

    DEBUGMSGTL(("gxgdWiFiModule_table:handler", "Processing request (%d)\n", reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
        case MODE_GET:
            for (request=requests; request; request=request->next) {
                table_entry = (struct wifistationTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_STATIONINDEX:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->stationindex);
                        break;
                    case COLUMN_STATIONMAC:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_value( request->requestvb, ASN_OCTET_STR,
                                table_entry->stationmac,
                                MACADDRSTRLEN);
                        break;
                    case COLUMN_ASSOCTIME:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_TIMETICKS,
                                table_entry->assoctime);
                        break;
                    default:
                        netsnmp_set_request_error(reqinfo, request,
                                SNMP_NOSUCHOBJECT);
                        break;
                }
            }
            break;

    }
    return SNMP_ERR_NOERROR;
}

/** Initialize the vlanTable table by defining its contents and how it's structured */
    void
initialize_table_vlanTable(void)
{
    const oid vlanTable_oid[] = {1,3,6,1,4,1,46227,2,3,1,1,46};
    const size_t vlanTable_oid_len   = OID_LENGTH(vlanTable_oid);
    netsnmp_handler_registration    *reg;
    netsnmp_iterator_info           *iinfo;
    netsnmp_table_registration_info *table_info;

    DEBUGMSGTL(("gxgdWiFiModule_table:init", "initializing table vlanTable\n"));

    reg = netsnmp_create_handler_registration(
            "vlanTable",     vlanTable_handler,
            vlanTable_oid, vlanTable_oid_len,
            HANDLER_CAN_RONLY
            );

    table_info = SNMP_MALLOC_TYPEDEF( netsnmp_table_registration_info );
    netsnmp_table_helper_add_indexes(table_info,
            ASN_INTEGER,  /* index: portnum */
            0);
    table_info->min_column = COLUMN_PORT;
    table_info->max_column = COLUMN_PORTNUM;

    iinfo = SNMP_MALLOC_TYPEDEF( netsnmp_iterator_info );
    iinfo->get_first_data_point = vlanTable_get_first_data_point;
    iinfo->get_next_data_point  = vlanTable_get_next_data_point;
    iinfo->table_reginfo        = table_info;

    netsnmp_register_table_iterator( reg, iinfo );

    /* Initialise the contents of the table here */
    buildvlantable();
}

/* Typical data structure for a row entry */
struct vlanTable_entry {
    /* Column values */
    char portname[PORTNAMESIZE];
    size_t portname_len;
    long enable;
    long old_enable;
    long mode;
    long old_mode;
    long fwdmode;
    long old_fwdmode;
    long vlanmode;
    long old_vlanmode;
    long vid;
    long old_vid;
    long priority;
    long old_priority;
    long frameencapmode;
    long old_frameencapmode;
    long portnum;

    /* Illustrate using a simple linked list */
    int   valid;
    struct vlanTable_entry *next;
};

struct vlanTable_entry  *vlanTable_head;

/* create a new row in the (unsorted) table */
struct vlanTable_entry *
vlanTable_createEntry(long  portnum) {
    struct vlanTable_entry *entry;

    entry = SNMP_MALLOC_TYPEDEF(struct vlanTable_entry);
    if (!entry)
        return NULL;

    entry->portnum = portnum;
    entry->next = vlanTable_head;
    vlanTable_head = entry;
    return entry;
}

/* remove a row from the table */
void
vlanTable_removeEntry( struct vlanTable_entry *entry ) {
    struct vlanTable_entry *ptr, *prev;

    if (!entry)
        return;    /* Nothing to remove */

    for ( ptr  = vlanTable_head, prev = NULL;
            ptr != NULL;
            prev = ptr, ptr = ptr->next ) {
        if ( ptr == entry )
            break;
    }
    if ( !ptr )
        return;    /* Can't find it */

    if ( prev == NULL )
        vlanTable_head = ptr->next;
    else
        prev->next = ptr->next;

    SNMP_FREE( entry );   /* XXX - release any other internal resources */
}


/* Example iterator hook routines - using 'get_next' to do most of the work */
    netsnmp_variable_list *
vlanTable_get_first_data_point(void **my_loop_context,
        void **my_data_context,
        netsnmp_variable_list *put_index_data,
        netsnmp_iterator_info *mydata)
{
    buildvlantable();
    *my_loop_context = vlanTable_head;
    return vlanTable_get_next_data_point(my_loop_context, my_data_context,
            put_index_data,  mydata );
}

    netsnmp_variable_list *
vlanTable_get_next_data_point(void **my_loop_context,
        void **my_data_context,
        netsnmp_variable_list *put_index_data,
        netsnmp_iterator_info *mydata)
{
    struct vlanTable_entry *entry = (struct vlanTable_entry *)*my_loop_context;
    netsnmp_variable_list *idx = put_index_data;

    if ( entry ) {
        snmp_set_var_typed_integer( idx, ASN_INTEGER, entry->portnum );
        idx = idx->next_variable;
        *my_data_context = (void *)entry;
        *my_loop_context = (void *)entry->next;
        return put_index_data;
    } else {
        return NULL;
    }
}


/** handles requests for the vlanTable table */
int
vlanTable_handler(
        netsnmp_mib_handler               *handler,
        netsnmp_handler_registration      *reginfo,
        netsnmp_agent_request_info        *reqinfo,
        netsnmp_request_info              *requests) {

    netsnmp_request_info       *request;
    netsnmp_table_request_info *table_info;
    struct vlanTable_entry          *table_entry;

    DEBUGMSGTL(("gxgdWiFiModule_table:handler", "Processing request (%d)\n", reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
        case MODE_GET:
            for (request=requests; request; request=request->next) {
                table_entry = (struct vlanTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_PORT:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_value( request->requestvb, ASN_OCTET_STR,
                                table_entry->portname,
                                table_entry->portname_len);
                        break;
                    case COLUMN_ENABLE:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->enable);
                        break;
                    case COLUMN_MODE:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->mode);
                        break;
                    case COLUMN_FWDMODE:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->fwdmode);
                        break;
                    case COLUMN_VLANMODE:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->vlanmode);
                        break;
                    case COLUMN_VID:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->vid);
                        break;
                    case COLUMN_PRIORITY:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->priority);
                        break;
                    case COLUMN_FRAMEENCAPMODE:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->frameencapmode);
                        break;
                    case COLUMN_PORTNUM:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->portnum);
                        break;
                    default:
                        netsnmp_set_request_error(reqinfo, request,
                                SNMP_NOSUCHOBJECT);
                        break;
                }
            }
            break;

#if 0
            /*
             * Write-support
             */
        case MODE_SET_RESERVE1:
            for (request=requests; request; request=request->next) {
                table_entry = (struct vlanTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_ENABLE:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_MODE:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_FWDMODE:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_VLANMODE:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_VID:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_PRIORITY:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_FRAMEENCAPMODE:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_PORTNUM:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    default:
                        netsnmp_set_request_error( reqinfo, request,
                                SNMP_ERR_NOTWRITABLE );
                        return SNMP_ERR_NOERROR;
                }
            }
            break;

        case MODE_SET_RESERVE2:
            break;

        case MODE_SET_FREE:
            break;

        case MODE_SET_ACTION:
            for (request=requests; request; request=request->next) {
                table_entry = (struct vlanTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_ENABLE:
                        table_entry->old_enable = table_entry->enable;
                        table_entry->enable     = *request->requestvb->val.integer;
                        break;
                    case COLUMN_MODE:
                        table_entry->old_mode = table_entry->mode;
                        table_entry->mode     = *request->requestvb->val.integer;
                        break;
                    case COLUMN_FWDMODE:
                        table_entry->old_fwdmode = table_entry->fwdmode;
                        table_entry->fwdmode     = *request->requestvb->val.integer;
                        break;
                    case COLUMN_VLANMODE:
                        table_entry->old_vlanmode = table_entry->vlanmode;
                        table_entry->vlanmode     = *request->requestvb->val.integer;
                        break;
                    case COLUMN_VID:
                        table_entry->old_vid = table_entry->vid;
                        table_entry->vid     = *request->requestvb->val.integer;
                        break;
                    case COLUMN_PRIORITY:
                        table_entry->old_priority = table_entry->priority;
                        table_entry->priority     = *request->requestvb->val.integer;
                        break;
                    case COLUMN_FRAMEENCAPMODE:
                        table_entry->old_frameencapmode = table_entry->frameencapmode;
                        table_entry->frameencapmode     = *request->requestvb->val.integer;
                        break;
                    case COLUMN_PORTNUM:
                        table_entry->old_portnum = table_entry->portnum;
                        table_entry->portnum     = *request->requestvb->val.integer;
                        break;
                }
            }
            break;

        case MODE_SET_UNDO:
            for (request=requests; request; request=request->next) {
                table_entry = (struct vlanTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_ENABLE:
                        table_entry->enable     = table_entry->old_enable;
                        table_entry->old_enable = 0;
                        break;
                    case COLUMN_MODE:
                        table_entry->mode     = table_entry->old_mode;
                        table_entry->old_mode = 0;
                        break;
                    case COLUMN_FWDMODE:
                        table_entry->fwdmode     = table_entry->old_fwdmode;
                        table_entry->old_fwdmode = 0;
                        break;
                    case COLUMN_VLANMODE:
                        table_entry->vlanmode     = table_entry->old_vlanmode;
                        table_entry->old_vlanmode = 0;
                        break;
                    case COLUMN_VID:
                        table_entry->vid     = table_entry->old_vid;
                        table_entry->old_vid = 0;
                        break;
                    case COLUMN_PRIORITY:
                        table_entry->priority     = table_entry->old_priority;
                        table_entry->old_priority = 0;
                        break;
                    case COLUMN_FRAMEENCAPMODE:
                        table_entry->frameencapmode     = table_entry->old_frameencapmode;
                        table_entry->old_frameencapmode = 0;
                        break;
                    case COLUMN_PORTNUM:
                        table_entry->portnum     = table_entry->old_portnum;
                        table_entry->old_portnum = 0;
                        break;
                }
            }
            break;

        case MODE_SET_COMMIT:
            break;
#endif
        default:
            break;
    }
    return SNMP_ERR_NOERROR;
}

void buildvlantable()
{
    struct vlanTable_entry *entry;
    struct timeval now;

#if 1
    gettimeofday(&now, NULL);

    if((now.tv_sec - buildvlantabletime.tv_sec) < REBUILDTABLEINTERVAL){
        //snmp_log(LOG_INFO, "skip to rebuild wifi station table\n");
        return;
    }

    buildvlantabletime = now;
#endif

    while(entry = vlanTable_head)
        vlanTable_removeEntry(entry);

    entry = vlanTable_createEntry(0);
    strcpy(entry->portname, "LAN_ETH0");
    entry->portname_len=strlen(entry->portname);
    entry->enable = SNMPTRUE;
    entry->mode = VLAN_LANPORT;
    entry->fwdmode = VLAN_NAT;
    entry->vlanmode = VLAN_UNTAG;
    entry->vid = 1;
    entry->priority = 1;
    entry->frameencapmode = FRMENCAP_STD;

    entry = vlanTable_createEntry(1);
    strcpy(entry->portname, "PORT1");
    entry->portname_len=strlen(entry->portname);
    entry->enable = SNMPTRUE;
    entry->mode = VLAN_LANPORT;
    entry->fwdmode = VLAN_NAT;
    entry->vlanmode = VLAN_TAG;
    entry->vid = 3;
    entry->priority = 1;
    entry->frameencapmode = FRMENCAP_STD;

    entry = vlanTable_createEntry(2);
    strcpy(entry->portname, "PORT2");
    entry->portname_len=strlen(entry->portname);
    entry->enable = SNMPTRUE;
    entry->mode = VLAN_LANPORT;
    entry->fwdmode = VLAN_BRIDGE;
    entry->vlanmode = VLAN_TAG;
    entry->vid = 4;
    entry->priority = 1;
    entry->frameencapmode = FRMENCAP_STD;

    entry = vlanTable_createEntry(3);
    strcpy(entry->portname, "WAN_ETH");
    entry->portname_len=strlen(entry->portname);
    entry->enable = SNMPTRUE;
    entry->mode = VLAN_WANPORT;
    entry->fwdmode = VLAN_BRIDGE;
    entry->vlanmode = VLAN_UNTAG;
    entry->vid = 1;
    entry->priority = 1;
    entry->frameencapmode = FRMENCAP_STD;

    return;
}

/** Initialize the vidTable table by defining its contents and how it's structured */
    void
initialize_table_vidTable(void)
{
    const oid vidTable_oid[] = {1,3,6,1,4,1,46227,2,3,1,1,47};
    const size_t vidTable_oid_len   = OID_LENGTH(vidTable_oid);
    netsnmp_handler_registration    *reg;
    netsnmp_iterator_info           *iinfo;
    netsnmp_table_registration_info *table_info;

    DEBUGMSGTL(("gxgdWiFiModule_table:init", "initializing table vidTable\n"));

    reg = netsnmp_create_handler_registration(
            "vidTable",     vidTable_handler,
            vidTable_oid, vidTable_oid_len,
            HANDLER_CAN_RONLY
            );

    table_info = SNMP_MALLOC_TYPEDEF( netsnmp_table_registration_info );
    netsnmp_table_helper_add_indexes(table_info,
            ASN_INTEGER,  /* index: portnum */
            0);
    table_info->min_column = COLUMN_PORT;
    table_info->max_column = COLUMN_PORTNUM;

    iinfo = SNMP_MALLOC_TYPEDEF( netsnmp_iterator_info );
    iinfo->get_first_data_point = vidTable_get_first_data_point;
    iinfo->get_next_data_point  = vidTable_get_next_data_point;
    iinfo->table_reginfo        = table_info;

    netsnmp_register_table_iterator( reg, iinfo );

    /* Initialise the contents of the table here */
    buildvidtable();
}

/* Typical data structure for a row entry */
struct vidTable_entry {
    /* Column values */
    char portname[PORTNAMESIZE];
    size_t portname_len;
    long enable;
    long old_enable;
    long natvid;
    long old_natvid;
    long bridgevid;
    long old_bridgevid;
    long portnum;

    /* Illustrate using a simple linked list */
    int   valid;
    struct vidTable_entry *next;
};

struct vidTable_entry  *vidTable_head;

/* create a new row in the (unsorted) table */
struct vidTable_entry *
vidTable_createEntry(long  portnum) {
    struct vidTable_entry *entry;

    entry = SNMP_MALLOC_TYPEDEF(struct vidTable_entry);
    if (!entry)
        return NULL;

    entry->portnum = portnum;
    entry->next = vidTable_head;
    vidTable_head = entry;
    return entry;
}

/* remove a row from the table */
void
vidTable_removeEntry( struct vidTable_entry *entry ) {
    struct vidTable_entry *ptr, *prev;

    if (!entry)
        return;    /* Nothing to remove */

    for ( ptr  = vidTable_head, prev = NULL;
            ptr != NULL;
            prev = ptr, ptr = ptr->next ) {
        if ( ptr == entry )
            break;
    }
    if ( !ptr )
        return;    /* Can't find it */

    if ( prev == NULL )
        vidTable_head = ptr->next;
    else
        prev->next = ptr->next;

    SNMP_FREE( entry );   /* XXX - release any other internal resources */
}


/* Example iterator hook routines - using 'get_next' to do most of the work */
    netsnmp_variable_list *
vidTable_get_first_data_point(void **my_loop_context,
        void **my_data_context,
        netsnmp_variable_list *put_index_data,
        netsnmp_iterator_info *mydata)
{
    buildvidtable();
    *my_loop_context = vidTable_head;
    return vidTable_get_next_data_point(my_loop_context, my_data_context,
            put_index_data,  mydata );
}

    netsnmp_variable_list *
vidTable_get_next_data_point(void **my_loop_context,
        void **my_data_context,
        netsnmp_variable_list *put_index_data,
        netsnmp_iterator_info *mydata)
{
    struct vidTable_entry *entry = (struct vidTable_entry *)*my_loop_context;
    netsnmp_variable_list *idx = put_index_data;

    if ( entry ) {
        snmp_set_var_typed_integer( idx, ASN_INTEGER, entry->portnum );
        idx = idx->next_variable;
        *my_data_context = (void *)entry;
        *my_loop_context = (void *)entry->next;
        return put_index_data;
    } else {
        return NULL;
    }
}


/** handles requests for the vidTable table */
int
vidTable_handler(
        netsnmp_mib_handler               *handler,
        netsnmp_handler_registration      *reginfo,
        netsnmp_agent_request_info        *reqinfo,
        netsnmp_request_info              *requests) {

    netsnmp_request_info       *request;
    netsnmp_table_request_info *table_info;
    struct vidTable_entry          *table_entry;

    DEBUGMSGTL(("gxgdWiFiModule_table:handler", "Processing request (%d)\n", reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
        case MODE_GET:
            for (request=requests; request; request=request->next) {
                table_entry = (struct vidTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_PORT:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_value( request->requestvb, ASN_OCTET_STR,
                                table_entry->portname,
                                table_entry->portname_len);
                        break;
                    case COLUMN_ENABLE:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->enable);
                        break;
                    case COLUMN_NATVID:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->natvid);
                        break;
                    case COLUMN_BRIDGEVID:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->bridgevid);
                        break;
                    case COLUMN_PORTNUM:
                        if ( !table_entry ) {
                            netsnmp_set_request_error(reqinfo, request,
                                    SNMP_NOSUCHINSTANCE);
                            continue;
                        }
                        snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                table_entry->portnum);
                        break;
                    default:
                        netsnmp_set_request_error(reqinfo, request,
                                SNMP_NOSUCHOBJECT);
                        break;
                }
            }
            break;

#if 0
            /*
             * Write-support
             */
        case MODE_SET_RESERVE1:
            for (request=requests; request; request=request->next) {
                table_entry = (struct vidTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_ENABLE:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_NATVID:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_BRIDGEVID:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    case COLUMN_PORTNUM:
                        /* or possibly 'netsnmp_check_vb_int_range' */
                        ret = netsnmp_check_vb_int( request->requestvb );
                        if ( ret != SNMP_ERR_NOERROR ) {
                            netsnmp_set_request_error( reqinfo, request, ret );
                            return SNMP_ERR_NOERROR;
                        }
                        break;
                    default:
                        netsnmp_set_request_error( reqinfo, request,
                                SNMP_ERR_NOTWRITABLE );
                        return SNMP_ERR_NOERROR;
                }
            }
            break;

        case MODE_SET_RESERVE2:
            break;

        case MODE_SET_FREE:
            break;

        case MODE_SET_ACTION:
            for (request=requests; request; request=request->next) {
                table_entry = (struct vidTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_ENABLE:
                        table_entry->old_enable = table_entry->enable;
                        table_entry->enable     = *request->requestvb->val.integer;
                        break;
                    case COLUMN_NATVID:
                        table_entry->old_natvid = table_entry->natvid;
                        table_entry->natvid     = *request->requestvb->val.integer;
                        break;
                    case COLUMN_BRIDGEVID:
                        table_entry->old_bridgevid = table_entry->bridgevid;
                        table_entry->bridgevid     = *request->requestvb->val.integer;
                        break;
                    case COLUMN_PORTNUM:
                        table_entry->old_portnum = table_entry->portnum;
                        table_entry->portnum     = *request->requestvb->val.integer;
                        break;
                }
            }
            break;

        case MODE_SET_UNDO:
            for (request=requests; request; request=request->next) {
                table_entry = (struct vidTable_entry *)
                    netsnmp_extract_iterator_context(request);
                table_info  =     netsnmp_extract_table_info(      request);

                switch (table_info->colnum) {
                    case COLUMN_ENABLE:
                        table_entry->enable     = table_entry->old_enable;
                        table_entry->old_enable = 0;
                        break;
                    case COLUMN_NATVID:
                        table_entry->natvid     = table_entry->old_natvid;
                        table_entry->old_natvid = 0;
                        break;
                    case COLUMN_BRIDGEVID:
                        table_entry->bridgevid     = table_entry->old_bridgevid;
                        table_entry->old_bridgevid = 0;
                        break;
                    case COLUMN_PORTNUM:
                        table_entry->portnum     = table_entry->old_portnum;
                        table_entry->old_portnum = 0;
                        break;
                }
            }
            break;

        case MODE_SET_COMMIT:
            break;
#endif
        default:
            break;
    }
    return SNMP_ERR_NOERROR;
}

void buildvidtable()
{
    struct vidTable_entry *entry;
    struct timeval now;

#if 1
    gettimeofday(&now, NULL);

    if((now.tv_sec - buildvidtabletime.tv_sec) < REBUILDTABLEINTERVAL){
        //snmp_log(LOG_INFO, "skip to rebuild wifi station table\n");
        return;
    }

    buildvidtabletime = now;
#endif

    while(entry = vidTable_head)
        vidTable_removeEntry(entry);

    entry = vidTable_createEntry(0);
    strcpy(entry->portname, "LAN_ETH0");
    entry->portname_len=strlen(entry->portname);
    entry->enable = SNMPTRUE;
    entry->natvid = 1;
    entry->bridgevid = 0;

    entry = vidTable_createEntry(1);
    strcpy(entry->portname, "PORT1");
    entry->portname_len=strlen(entry->portname);
    entry->enable = SNMPTRUE;
    entry->natvid = 3;
    entry->bridgevid = 0;

    entry = vidTable_createEntry(2);
    strcpy(entry->portname, "PORT2");
    entry->portname_len=strlen(entry->portname);
    entry->enable = SNMPTRUE;
    entry->natvid = 0;
    entry->bridgevid = 4;

    entry = vidTable_createEntry(3);
    strcpy(entry->portname, "WAN_ETH");
    entry->portname_len=strlen(entry->portname);
    entry->enable = SNMPTRUE;
    entry->natvid = 0;
    entry->bridgevid = 0;

    return;
}
