/* 
	Copyright (C) 2010, Broadcom Corporation
	All Rights Reserved.
	
	This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
	the contents of this file may not be disclosed to third parties, copied
	or duplicated in any form, in whole or in part, without the prior
	written permission of Broadcom Corporation.
	
    $Id: ses_net.h 241186 2011-02-17 21:51:52Z gmo $
*/

#ifndef __SES_NET_H__
#define __SES_NET_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int ses_dot11_join(char const *ifname, ses_join_info_t *join_info);
extern int ses_dot11_scan(char const *ifname, int *wait_time);
extern int ses_dot11_scan_results(char const *ifname, wl_scan_results_t **results);
extern int ses_dot11_disassoc(char const *ifname);
extern int ses_get_phy_addr(char const *ifname, unsigned char *phyaddr, int size);
extern int ses_get_media_state(char const *ifname, int *state);
extern int ses_get_adapter_type(char const *ifname, int *type);
extern char *ses_get_wlname(char const *ifname, char *wlname);

#ifdef __cplusplus
} /* end-extern-C */
#endif

#endif /* __SES_NET_H__ */
