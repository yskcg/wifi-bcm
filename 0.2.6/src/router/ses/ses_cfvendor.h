/*
 * SES vendor header file.
 * This file has the definitions needed by vendors to tailor SES for their needs.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_cfvendor.h 241187 2011-02-17 21:52:03Z gmo $
 */

#ifndef _ses_cfvendor_h_
#define _ses_cfvendor_h_

#define SES_FSM_STATUS_SUCCESS		0
#define SES_FSM_STATUS_INTERNAL_ERROR	1

typedef int ses_eventi_t;

/* _LEGACY_ events are to support the legacy behavior during open window
 * where network is in "open" mode
 */
#define SES_EVTI_NULL		0
#define SES_EVTI_STATUS		1
#define SES_EVTI_INIT_CONDITION	2	/* configurator factory reset */
#define SES_EVTI_NW_GEN_SES	3	/* generate SES network */
#define SES_EVTI_RWO		4	/* open registration window */
#define SES_EVTI_RWO_CONTINUE	5	/* continue RWO (3-way mode) */
#define SES_EVTI_NW_GEN_SES_RWO	6	/* generate SES network + RWO */
#define SES_EVTI_NW_RESET	7	/* restore network to factory def */
#define SES_EVTI_LEGACY_RWO	8	/* open registration window (legacy) */
#define SES_EVTI_LEGACY_NW_GEN_SES_RWO	9 /* gen SES network + RWO (legacy) */
#define SES_EVTI_CANCEL		10	/* Cancel current operation */

typedef enum ses_evento {
	SES_EVTO_UNCONFIGURED = 0,
	SES_EVTO_CONFIGURED,
	SES_EVTO_RWO,
	SES_EVTO_RWO_CONFIRM,
	SES_EVTO_RWO_TEST,
	SES_EVTO_RWC,
	SES_EVTO_EXCH_INITIATED
} ses_evento_t;

typedef int ses_status_t;

#define	SES_STATUS_UNKNOWN	-1
#define SES_STATUS_SUCCESS	0
#define SES_STATUS_FAILURE	1

typedef struct ses_event {
	ses_evento_t evto;
	ses_status_t status;
	union {
		struct ether_addr rem_ea; /* SES_EVTO_EXCH_INITIATED, SES_EVTO_RWC */
		bool configured; /* SES_EVTO_RWO */
	} u;
} ses_event_t;

typedef struct ses_network {
	bool open;			/* open or closed network */
	char ssid[SES_MAX_SSID_LEN+1];	/* ssid */
	char passphrase[SES_MAX_PASSPHRASE_LEN+1];	/* passphrase */
	uint8 security;			/* wpa-psk or wpa2-psk */
	uint8 encryption;		/* tkip, aes, tkip+aes */
} ses_network_t;

typedef void const * ses_fsm_ctx_t;

#define SES_SETUP_MODE_2WAY		1
#define SES_SETUP_MODE_3WAY		2

typedef struct ses_fsm_setup {
	int mode;		/* 2-way, 3-way handshake */
	int flags;		/* modifier flags */
	int ow_time;		/* open window timeout in seconds */
	int wds_mode;		/* wds operation mode */
	bool poll;		/* whether to run in a polled mode */
	char ifnames[SES_IFNAME_SIZE*SES_PE_MAX_INTERFACES]; /* interface names */
	char def_wlname[SES_IFNAME_SIZE]; /* default wl interface name */
} ses_fsm_setup_t;

typedef ses_eventi_t
(*fnc_event_get_t)
(ses_fsm_ctx_t ctx, bool const *noblock);

typedef void
(*fnc_event_notify_t)
(ses_fsm_ctx_t ctx, char const *ifname, ses_event_t *event);

typedef int
(*fnc_network_store_t)
(ses_fsm_ctx_t ctx, char const *ifname);

typedef int
(*fnc_network_restore_t)
(ses_fsm_ctx_t ctx, char const *ifname);

typedef int
(*fnc_network_recall_t)
(ses_fsm_ctx_t ctx, char const *ifname, ses_network_t *network);

typedef int
(*fnc_network_reset_t)
(ses_fsm_ctx_t ctx, char const *ifname);

typedef int
(*fnc_network_set_t)
(ses_fsm_ctx_t ctx, char const *ifname, ses_network_t const *network);

typedef int
(*fnc_network_get_t)
(ses_fsm_ctx_t ctx, char const *ifname, ses_network_t *network);

typedef int
(*fnc_network_set_bridge_t)
(ses_fsm_ctx_t ctx, bool enabled);

typedef ses_network_t *
(*fnc_network_gen_ses_t)
(ses_fsm_ctx_t ctx, char const *ifname, ses_network_t *network, int force);

typedef ses_network_t *
(*fnc_network_gen_rnd_t)
(ses_fsm_ctx_t ctx, ses_network_t *network);

typedef bool
(*fnc_network_is_ses_t)
(ses_fsm_ctx_t ctx, char const *ifname);

typedef int
(*fnc_wds_add_t)
(ses_fsm_ctx_t ctx, char const *ifname, char const *ssid, char const *network,
 struct ether_addr *ea);

typedef int
(*fnc_stg_set_t)
(ses_fsm_ctx_t ctx, char const *name, char const *buf, int len);

typedef int
(*fnc_stg_get_t)
(ses_fsm_ctx_t ctx, char const *name, char *buf, int len);

typedef void
(*fnc_sys_reinit_t)
(ses_fsm_ctx_t ctx);

typedef struct ses_fsm_cb {
	fnc_event_get_t 	event_get;
	fnc_event_notify_t	event_notify;
	fnc_network_store_t	network_store;
	fnc_network_restore_t	network_restore;
	fnc_network_recall_t	network_recall;
	fnc_network_reset_t	network_reset;
	fnc_network_set_t	network_set;
	fnc_network_get_t	network_get;
	fnc_network_set_bridge_t	network_set_bridge;
	fnc_network_gen_ses_t	network_gen_ses;
	fnc_network_gen_rnd_t	network_gen_rnd;
	fnc_network_is_ses_t	network_is_ses;
	fnc_wds_add_t		wds_add;
	fnc_stg_set_t		stg_set;
	fnc_stg_get_t		stg_get;
	fnc_sys_reinit_t	sys_reinit;
} ses_fsm_cb_t;

/* main API for fsm module; returns status of initialization */
int init_fsm(ses_fsm_cb_t *cb, ses_fsm_setup_t *setup, void *client_data);

int fsm_get_client_data(ses_fsm_ctx_t ctx, void **client_data);
int fsm_op_req_reinit(ses_fsm_ctx_t ctx);

int brcm_ses_network_store(ses_fsm_ctx_t ctx, char const *ifname);
int brcm_ses_network_restore(ses_fsm_ctx_t ctx, char const *ifname);
int brcm_ses_network_recall(ses_fsm_ctx_t ctx, char const *ifname, ses_network_t *network);
int brcm_ses_network_get(ses_fsm_ctx_t ctx, char const *ifname, ses_network_t *network);
int brcm_ses_network_set(ses_fsm_ctx_t ctx, char const *ifname, ses_network_t const *network);
int brcm_ses_network_reset(ses_fsm_ctx_t ctx, char const *ifname);
bool brcm_ses_network_is_ses(ses_fsm_ctx_t ctx, char const *ifname);
ses_network_t * brcm_ses_network_gen_ses(ses_fsm_ctx_t ctx, char const *ifname,
                                         ses_network_t *network, int force);
ses_network_t * brcm_ses_network_gen_rnd(ses_fsm_ctx_t ctx, ses_network_t *network);
int brcm_ses_network_set_bridge(ses_fsm_ctx_t ctx, bool enabled);
int brcm_ses_wds_add(ses_fsm_ctx_t ctx, char const *ifname, char const *ssid,
                     char const *passphrase, struct ether_addr *ea);
int brcm_ses_stg_set(ses_fsm_ctx_t ctx, char const *name, char const *buf, int len);
int brcm_ses_stg_get(ses_fsm_ctx_t ctx, char const *name, char *buf, int len);
void brcm_ses_sys_reinit(ses_fsm_ctx_t ctx);
int ses_main(int argc, char *argv[]);

#endif /* _ses_cfvendor_h_ */
