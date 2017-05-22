/*
 * SES driver specifc header file.
 * This file has the definitions by vendors who will port the SES configurator to
 * different wireless drivers
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_cfwl.h 241187 2011-02-17 21:52:03Z gmo $
 */

#ifndef _ses_cfwl_h_
#define _ses_cfwl_h_

#define SES_VNDR_IE_CMD_ADD             "add"
#define SES_VNDR_IE_CMD_DEL             "del"

#define SES_VNDR_IE_ADD(a, data, datalen) \
ses_dot11_vndr_ie((a), \
SES_VNDR_IE_CMD_ADD, \
SES_VNDR_IE_PKT_FLAGS, \
(uchar*)BRCM_PROP_OUI, (data), (datalen))

#define SES_VNDR_IE_DEL(a, data, datalen) \
ses_dot11_vndr_ie((a), \
SES_VNDR_IE_CMD_DEL, \
SES_VNDR_IE_PKT_FLAGS, \
(uchar*)BRCM_PROP_OUI, (data), (datalen))

#define SES_VNDR_IE_IS_SET(a, data, datalen) \
ses_dot11_vndr_ie_is_set((a), \
SES_VNDR_IE_PKT_FLAGS, \
(uchar*)BRCM_PROP_OUI, (data), (datalen))

#define SES_VNDR_IE_SAFE_ADD(a, data, datalen) \
do { \
if (SES_VNDR_IE_IS_SET((a), (data), (datalen)) == FALSE) \
	SES_VNDR_IE_ADD((a), (data), (datalen)); \
} while (0)

#define SES_VNDR_IE_SAFE_DEL(a, data, datalen) \
do { \
if (SES_VNDR_IE_IS_SET((a), (data), (datalen)) == TRUE) \
	SES_VNDR_IE_DEL((a), (data), (datalen)); \
} while (0)


int ses_dot11_transition_mode(char *ifname, bool enable);
uint32 ses_dot11_lazywds_set(char *ifname, uint32 val);
bool ses_dot11_is_wds_link_up(char *ifname, struct ether_addr *ea);
int ses_dot11_add_wds_mac(char *ifname, struct ether_addr *ea);
int ses_dot11_vndr_ie(char *adapter, char *cmd, uint32 pktflag, uchar *oui,
	void *data, int datalen);
bool ses_dot11_vndr_ie_is_set(char *adapter, uint32 pktflag, uchar *oui,
	void *data, int datalen);

#endif /* _ses_cfwl_h_ */
