#ifndef GXGDWIFIMODULE_H
#define GXGDWIFIMODULE_H

/* function declarations */
void init_gxgdWiFiModule(void);
Netsnmp_Node_Handler handle_loginusername;
Netsnmp_Node_Handler handle_loginpassword;
Netsnmp_Node_Handler handle_lanipaddress;
Netsnmp_Node_Handler handle_lannetmask;
Netsnmp_Node_Handler handle_reset;
Netsnmp_Node_Handler handle_factoryrestore;
Netsnmp_Node_Handler handle_txpower;
Netsnmp_Node_Handler handle_wifimode;
Netsnmp_Node_Handler handle_channel;
Netsnmp_Node_Handler handle_ssidnumber;
Netsnmp_Node_Handler handle_applysettings;
Netsnmp_Node_Handler handle_pppoeusername;
Netsnmp_Node_Handler handle_pppoepassword;
Netsnmp_Node_Handler handle_pppoeservicename;
Netsnmp_Node_Handler handle_pppoestart;
Netsnmp_Node_Handler handle_pppoeconnectionstatus;
Netsnmp_Node_Handler handle_pppoeon;
Netsnmp_Node_Handler handle_wanon;
Netsnmp_Node_Handler handle_wandhcpoption60;
Netsnmp_Node_Handler handle_wirelessmode;
Netsnmp_Node_Handler handle_pppoeconnectondemand;
Netsnmp_Node_Handler handle_pppoedisconnect;
Netsnmp_Node_Handler handle_wpsenable;
Netsnmp_Node_Handler handle_dhcpenable;
Netsnmp_Node_Handler handle_dhcpaddresstart;
Netsnmp_Node_Handler handle_dhcpaddressend;
Netsnmp_Node_Handler handle_dhcpnetmask;
Netsnmp_Node_Handler handle_dhcpgateway;
Netsnmp_Node_Handler handle_dhcpleasetime;
Netsnmp_Node_Handler handle_wifibandwidth;
Netsnmp_Node_Handler handle_wanipaddress;
Netsnmp_Node_Handler handle_wannetmask;
Netsnmp_Node_Handler handle_wangateway;
Netsnmp_Node_Handler handle_wandns;
Netsnmp_Node_Handler handle_wanseconddns;
Netsnmp_Node_Handler handle_wanconnectionstatus;
Netsnmp_Node_Handler handle_wantype;
Netsnmp_Node_Handler handle_wanmacaddress;
Netsnmp_Node_Handler handle_lanmacaddress;
Netsnmp_Node_Handler handle_wanstaticipaddress;
Netsnmp_Node_Handler handle_wanstaticnetmask;
Netsnmp_Node_Handler handle_wanstaticgateway;
Netsnmp_Node_Handler handle_wanstaticdns;
Netsnmp_Node_Handler handle_wanstaticdns2;
Netsnmp_Node_Handler handle_manufacture;
Netsnmp_Node_Handler handle_hardwaremodel;
Netsnmp_Node_Handler handle_softwareversion;
Netsnmp_Node_Handler handle_upgradingstart;
Netsnmp_Node_Handler handle_rebootafterupgrading;
Netsnmp_Node_Handler handle_imagefilename;
Netsnmp_Node_Handler handle_serverip;
Netsnmp_Node_Handler handle_serverport;
Netsnmp_Node_Handler handle_vlanenabled;
Netsnmp_Node_Handler handle_bridgedvlanid;
Netsnmp_Node_Handler handle_natvlanid;

/* function declarations */
//void init_gxgdWiFiModule_table(void);
void initialize_table_wirelessTable(void);
Netsnmp_Node_Handler wirelessTable_handler;
Netsnmp_First_Data_Point  wirelessTable_get_first_data_point;
Netsnmp_Next_Data_Point   wirelessTable_get_next_data_point;

void initialize_table_dhcpclientTable(void);
Netsnmp_Node_Handler dhcpclientTable_handler;
Netsnmp_First_Data_Point  dhcpclientTable_get_first_data_point;
Netsnmp_Next_Data_Point   dhcpclientTable_get_next_data_point;
void buildhcpclienttable();

void initialize_table_wifistationTable(void);
Netsnmp_Node_Handler wifistationTable_handler;
Netsnmp_First_Data_Point  wifistationTable_get_first_data_point;
Netsnmp_Next_Data_Point   wifistationTable_get_next_data_point;
void buildwifistationtable();

void initialize_table_vlanTable(void);
Netsnmp_Node_Handler vlanTable_handler;
Netsnmp_First_Data_Point  vlanTable_get_first_data_point;
Netsnmp_Next_Data_Point   vlanTable_get_next_data_point;

void initialize_table_vidTable(void);
Netsnmp_Node_Handler vidTable_handler;
Netsnmp_First_Data_Point  vidTable_get_first_data_point;
Netsnmp_Next_Data_Point   vidTable_get_next_data_point;

/* column number definitions for table wirelessTable */
#define COLUMN_SSIDTAILER		1
#define COLUMN_ISOLATION		2
#define COLUMN_SSIDBROADCAST		3
#define COLUMN_SECMODE		4
#define COLUMN_PASSPHRASE		5
#define COLUMN_MAXSTA		6
#define COLUMN_ENCRYPTIONMETHOD		7
#define COLUMN_ENABLED		8
#define COLUMN_MACADDRESS		9
#define COLUMN_HEADERCHANGABLE		10
#define COLUMN_SSIDHEADER		11
#define COLUMN_SSIDINDEX		12

/* column number definitions for table dhcpclientTable */
#define COLUMN_CLIENTINDEX		1
#define COLUMN_CLIENTMAC		2
#define COLUMN_CLIENTIP		3
#define COLUMN_CLIENTLEASETIME		4

/* column number definitions for table wifistationTable */
#define COLUMN_STATIONINDEX		1
#define COLUMN_STATIONMAC		2
#define COLUMN_ASSOCTIME		3

/* column number definitions for table vlanTable */
#define COLUMN_PORT		1
#define COLUMN_ENABLE		2
#define COLUMN_MODE		3
#define COLUMN_FWDMODE		4
#define COLUMN_VLANMODE		5
#define COLUMN_VID		6
#define COLUMN_PRIORITY		7
#define COLUMN_FRAMEENCAPMODE		8
#define COLUMN_PORTNUM		9

/* column number definitions for table vidTable */
#define COLUMN_PORT		1
#define COLUMN_ENABLE		2
#define COLUMN_NATVID		3
#define COLUMN_BRIDGEVID		4
#define COLUMN_PORTNUM		9

#define INTEGER_ENABLE 1
#define INTEGER_DISABLE 2

#define SNMPTRUE 1
#define SNMPFALSE 2

#define SSID_MAX_LEN 15

#define WIFI_80211bg 1
#define WIFI_80211b 2
#define WIFI_80211g 3
#define WIFI_80211bgn 4
#define WIFI_80211n 5
#define WIFI_80211ac 6

#define WIFI_AP 0
#define WIFI_CLIENT 1
#define WIFI_ROUTER 2
#define WIFI_REPEATER 3
#define WIFI_BRIDGE 4

#define OPENED_WIFI 1
#define CLOSED_WIFI 2

#define UPPER_CHNANNEL_40M 7
#define UPPER_CHNANNEL_20M 13

#define MAXUSRNMLEN 16

#define MAXSSIDHDRLEN 9

#define MAXSSIDLEN 15

#define WANDHCPOK 1
#define WANDHCPNOK 2

#define SETTINGSAPPLYNON 0
#define SETTINGSAPPLYINPROGRESS 1

#define APPLYSETTINGS 1

#define MACADDRSTRLEN 17

#define MAX_STA_COUNT 256

#define REBUILDTABLEINTERVAL 0

#define LW_SSID_NUMBER 2

#define APPLY_ALLSETTINGS 1
#define APPLY_WIFISETTINGS 2
#define APPLY_WANSETTINGS 3

#define PPPOEST_CONNECTED 0
#define PPPOEST_USERDISCONNECT 1
#define PPPOEST_ERRORPHYLINK 631
#define PPPOEST_ISPTIMEOUT 678
#define PPPOEST_AUTHFAIL 691
#define PPPOEST_RESTRICTEDLOGINHOUR 718
#define PPPOEST_ERRORUNKNOW 720
#define PPPOEST_NOTENABLEDFORINTERNET 769

#define WIFI_MODE_AP 0
#define WIFI_MODE_CLIENT 1
#define WIFI_MODE_ROUTER 2
#define WIFI_MODE_REPEATER 3
#define WIFI_MODE_BRIDGE 4

#define PPPOE_CONTINUES 0
#define PPPOE_ONDEMAND 1
#define PPPOE_MANUAL 2

#define CHANNEL_20M_24G 0
#define CHANNEL_40M_24G 1
#define CHANNEL_20M40M_AUTO 2
#define CHANNEL_NOTSP_BDW 9


#define WANPROTO_STATIC 1
#define WANPROTO_DHCP 2
#define WANPROTO_PPPOE 3

#define MANUFACT_UNKNOW 0
#define MANUFACT_MOTOROLA 1
#define MANUFACT_THOMSON 2
#define MANUFACT_CISCO 3
#define MANUFACT_RADIOTECH 4
#define MANUFACT_CHYALI 5
#define MANUFACT_FOXCOM 6
#define MANUFACT_LANWANG "56itech"
#define MANUFACT_ORIENTVIEW "8"
#define MANUFACT_JIUIZHOU 34
#define MANUFACT_SKYWORTH 44
#define MANUFACT_COSHIP 51

#define DEFAULT_MANUFACTURE MANUFACT_LANWANG

#define FW_FILE "/fw"
#define HARDWAREVER "1.0.0"
#define SOFTWAREVER "1.0.0"

#define INVALID_PORT 0
#define NEWIMGFULLPATH "/tmp/newimg"

#define NATEDVID 3
#define BRIDGEDVID 4

#define MAXPSKLEN 15
#define KEYMGMT_DISABLED 0
#define KEYMGMT_WEP 1
#define KEYMGMT_WPAPSK 2
#define KEYMGMT_WPA2PSK 3
#define KEYMGMT_PSKMIXED 4

#define ENCRY_TKIP 1
#define ENCRY_AES 2
#define ENCRY_TKIPAES 3

#define DEFAULT_PSK_STRING "12345678"

#define DHCPMINIMUMPOOLSIZE 20
#define DHCPPOOLSTART 10

#define PORTNAMESIZE 16
#define VLAN_WANPORT 0
#define VLAN_LANPORT 1
#define VLAN_NAT 0
#define VLAN_BRIDGE 1
#define VLAN_UNTAG 0
#define VLAN_TAG 1
#define FRMENCAP_STD 0
#define FRMENCAP_NSTD 1

#define WPS_DONE false

#define sin_addr(s) (((struct sockaddr_in *)(s))->sin_addr)

#endif /* GXGDWIFIMODULE_H */
