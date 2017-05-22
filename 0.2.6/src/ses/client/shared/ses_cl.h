/* 
	Copyright (C) 2010, Broadcom Corporation
	All Rights Reserved.
	
	This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
	the contents of this file may not be disclosed to third parties, copied
	or duplicated in any form, in whole or in part, without the prior
	written permission of Broadcom Corporation.
	
    $Id: ses_cl.h 241186 2011-02-17 21:51:52Z gmo $
*/

#ifndef __SES_CL_H__
#define __SES_CL_H__

#include <ses.h>
#include <typedefs.h>
#include <wlioctl.h>
#include <proto/ethernet.h>

#define SES_RESULT_SUCCESS			 0	/* successful completion */
#define SES_RESULT_FAILURE			-1	/* general failure */
#define SES_RESULT_SES_HALT			-2	/* ses ended unexpectedly (see status) */
#define SES_RESULT_BAD_CONTEXT		-3	/* context value invalid */
#define SES_RESULT_BAD_FUNCTION		-4	/* cannot call function from callback */
#define SES_RESULT_BAD_PARAMETER	-5	/* invalid parameter value */
#define SES_RESULT_MEM_ALLOCATION	-6	/* memory allocation failure */
#define SES_RESULT_OID				-7	/* OID call failure */
#define SES_RESULT_MEDIA_CONNECT	-8	/* media connect status failure */
#define SES_RESULT_PKT_EXCHANGE		-9	/* packet exchange failure */

#define SES_RESULT_CF_NONE			-10	/* no open configurators */
#define SES_RESULT_CF_GT1			-11	/* >1 open configurators */
#define SES_RESULT_CF_RC			-12	/* configurator recently configured */
#define SES_RESULT_CL_GT1			-13	/* >1 client requests to configurator */
#define SES_RESULT_SB_TIMEOUT		-14	/* standby timeout expired */
#define SES_RESULT_RW_TIMEOUT		-15	/* registration window timeout expired */
#define SES_RESULT_USER_CANCEL		-16	/* exchange cancelled by user */

#define SES_OP_NULL				 	0
#define SES_OP_GO					1  /* initiate discovery and exchange */
#define SES_OP_CANCEL				2  /* cancel exchange and bail out */
#define SES_OP_CONTINUE				3  /* continue */
#define SES_OP_RESET				4  /* reset wpa/psk settings to null */

#define SES_FLAG_CFM_CONFIGURATOR	1	/* confirm configurator */
#define SES_FLAG_WDS_CLIENT			2	/* wds client */

typedef struct {
	char		**ifnames;				/* NIC names */
	int			ifnames_count;			/* number of NIC names */
	int			flags;					/* modifier flags */
	int			ow_timeout;				/* open window timeout */
	int			standby_timeout;		/* 3-button standby timeout */
} ses_settings_t;

typedef struct {
	char		ssid[SES_MAX_SSID_LEN+1];	/* ssid */
	char		passphrase[SES_MAX_PASSPHRASE_LEN+1];	/* passphrase */
	uint8		security;
	uint8		encryption;
	uint8		channel;
	struct ether_addr remote;
} ses_data_t;

typedef void * ses_context_t;

#define SES_EVENT_IDLE					1
#define SES_EVENT_DISCOVERING			2
#define SES_EVENT_DISCOVERED			3
#define SES_EVENT_CONFIGURATOR_CONFIRM	4
#define SES_EVENT_ASSOCIATION			5
#define SES_EVENT_CONFIGURATION			6
#define SES_EVENT_HALT					7

typedef struct {
	int			type;
} ses_idle_event_t;

typedef struct {
	int			type;
	char const	**ifnames;
	int			ifnames_count;
	int			ow_timeout;
} ses_discovering_event_t;

typedef struct {
	int			type;
	char const	*ifname;
	char const	*ssid;
	struct ether_addr *remote;
} ses_discovered_event_t;

typedef ses_discovered_event_t ses_configurator_confirm_event_t;

#define SES_ASSOC_STATE_UNASSOCIATED	 1
#define SES_ASSOC_STATE_ASSOCIATING		 2
#define SES_ASSOC_STATE_ASSOCIATED		 3

typedef struct {
	int			type;
	int			state;
	char const	*ifname;
	char const	*ssid;
	struct ether_addr *remote;
} ses_assoc_event_t;

#define SES_CONFIG_STATE_INIT		1
#define SES_CONFIG_STATE_WAIT		2
#define SES_CONFIG_STATE_RCVD		3

typedef struct {
	int			type;
	int			state;
	char const	*ifname;
	ses_data_t	*data;
} ses_configuration_event_t;

typedef struct {
	int		type;
	int		status;
} ses_halt_event_t;

typedef union {
	int		type;
	ses_idle_event_t idle_event;
	ses_discovering_event_t discovering_event;
	ses_discovered_event_t discovered_event;
	ses_configurator_confirm_event_t configurator_confirm_event;
	ses_assoc_event_t assoc_event;
	ses_configuration_event_t configuration_event;
	ses_halt_event_t halt_event;
} ses_event_t;

#define SES_ADAPTER_TYPE_UNKNOWN		0
#define SES_ADAPTER_TYPE_802_3			1
#define SES_ADAPTER_TYPE_WIRELESS		2

#define SES_MEDIA_STATE_CONNECTED		0
#define SES_MEDIA_STATE_DISCONNECTED	1

typedef struct {
	char				ssid[SES_MAX_SSID_LEN+1];       /* ssid */
	uint16				capability;
	uint8				channel;
	struct ether_addr	remote;
} ses_join_info_t;

typedef void 
(*fnc_ses_event_notify_t)
(ses_context_t ctx, ses_event_t const *event);

typedef int 
(*fnc_ses_event_get_t)
(ses_context_t ctx);

typedef void 
(*fnc_ses_event_release_t)
(ses_context_t ctx);

typedef int
(*fnc_ses_dot11_join_t)
(char const *ifname, ses_join_info_t *join_info);

typedef int
(*fnc_ses_dot11_scan_t)
(char const *ifname, int *wait_time);

typedef int
(*fnc_ses_dot11_scan_results_t)
(char const *ifname, wl_scan_results_t **results);

typedef int
(*fnc_ses_dot11_disassoc_t)
(char const *ifname);

typedef int
(*fnc_ses_get_media_state_t)
(char const *ifname, int *state);

typedef int
(*fnc_ses_get_adapter_type_t)
(char const *ifname, int *type);

typedef char *
(*fnc_ses_get_wlname_t)
(char const *ifname, char *wlname);

typedef struct {
	fnc_ses_event_notify_t			event_notify;
	fnc_ses_event_get_t				event_get;
	fnc_ses_event_release_t			event_release;
	fnc_ses_dot11_join_t			dot11_join;
	fnc_ses_dot11_scan_t			dot11_scan;
	fnc_ses_dot11_scan_results_t	dot11_scan_results;
	fnc_ses_dot11_disassoc_t		dot11_disassoc;
	fnc_ses_get_media_state_t		get_media_state;
	fnc_ses_get_adapter_type_t		get_adapter_type;
	fnc_ses_get_wlname_t			get_wlname;
} ses_cb_t;

#ifdef __cplusplus
extern "C" {
#endif

int ses_open(ses_context_t *ctx, ses_settings_t *settings, ses_cb_t *cb, void *client_data);
int ses_close(ses_context_t ctx);
int ses_go(ses_context_t ctx, ses_data_t *data);
int ses_client_data(ses_context_t ctx, void **client_data);
int ses_create_acm_WPA_PSK_profile(const char *ifname,const char *ssid,const char *key);

#ifdef __cplusplus
} /* end-extern-C */
#endif

#endif /* __SES_CL_H__ */
