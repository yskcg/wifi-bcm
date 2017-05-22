/*
 * SES Finite State Machine
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_fsm.c 241187 2011-02-17 21:52:03Z gmo $
 *
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <assert.h>
#include <wlutils.h>
#include <shutils.h>
#include <bcmutils.h>

#include <ses_dbg.h>
#include <ses_packet.h>
#include <ses_cmnport.h>
#include <ses_cfvendor.h>
#include <ses_cfwl.h>

#define FSM_STG_CURRENT_STATES		"ses_fsm_current_states"
#define FSM_STG_LAST_STATUS		"ses_fsm_last_status"

#define MAX_STATES_STR_LEN		6	/* xx:xx + NULL */
#define MAX_STATUS_STR_LEN		4

typedef uchar fsm_state_t;
typedef uint16 fsm_states_t;

#define FSM_STATE_UNKNOWN		0	/* not a state */
#define FSM_STATE_UNCONFIGURED		1
#define FSM_STATE_CONFIGURED		2
#define FSM_STATE_LEGACY_XCHG_INIT	3	/* legacy */
#define FSM_STATE_XCHG_TEST		4	/* obsolete */
#define FSM_STATE_LEGACY_XCHG_WAIT	5	/* legacy */
#define FSM_STATE_XCHG_DONE		6
#define FSM_STATE_LEGACY_U_XCHG_INIT	7	/* legacy */
#define FSM_STATE_U_XCHG_TEST		8	/* obsolete */
#define FSM_STATE_LEGACY_U_XCHG_WAIT	9	/* legacy */
#define FSM_STATE_XCHG_INIT		10
#define FSM_STATE_U_XCHG_INIT		11
#define FSM_STATE_XCHG_CONTINUE		12

#define FSMIE_STATE_UNKNOWN		0	/* not a state */
#define FSMIE_STATE_RWC			1
#define FSMIE_STATE_RWO_AD		2
#define FSMIE_STATE_RWC_AD		3

typedef int fsmie_evti_t;

#define FSMIE_EVTI_NULL			0
#define FSMIE_EVTI_ENTRY		1
#define FSMIE_EVTI_INIT_CONDITION	2	/* initial condition */
#define FSMIE_EVTI_RWO			3	/* reg. window open */
#define FSMIE_EVTI_RDS			4	/* reg. data send */
#define FSMIE_EVTI_CANCEL		5	/* reg. window cancel */
#define FSMIE_EVTI_RWC_AD_TIMEOUT	6	/* reg. window ad timeout */

typedef struct fsm_data {
	fsm_states_t states;
	ses_fsm_setup_t *setup;
	ses_fsm_cb_t *cb;
	void *client_data;
	bool reinit;
	bool noblock;
	bool exch_initiated_notified;
	bool enable_wds;
	bool xchg_thru_wds;
	uchar ses_ie_flags;
	uint32 old_lazywds;
	int rem_ow_time;
	fsmie_evti_t ie_event;
	ses_timer_id rwc_tid;
	ses_timer_id poll_tid; /* Timer for polling */
	ses_packet_exchange_t pe;
} fsm_data_t;

typedef struct fsm_dbg_str_map {
	int idx;
	char const * const name;
} fsm_dbg_str_map_t;

#define SES_VNDR_IE_PKT_FLAGS	VNDR_IE_BEACON_FLAG | VNDR_IE_PRBRSP_FLAG

#define SES_VNDR_IE_SET_RWO(ie, flags) \
(ie)->ses_flags |= (flags & SES_VNDR_IE_FLAG_MASK)

#define SES_VNDR_IE_SET_RWC(ie) \
(ie)->ses_flags &= ~(SES_VNDR_IE_FLAG_MASK)


static int fsm_validate_cb_data(ses_fsm_cb_t *cb);
static ses_eventi_t fsm_set_initial_state(fsm_data_t *p);
static ses_eventi_t fsm_state_tran(fsm_data_t *p, fsm_state_t state, ses_status_t status);
static void fsm_init_pe(ses_packet_exchange_t *pe, ses_network_t const *network, int phase,
                        ses_fsm_setup_t const *setup);
#ifdef SES_LEGACY_MODE
static int fsm_exec_pe(ses_packet_exchange_t *pe, ses_network_t const *network, int phase,
                       ses_fsm_setup_t const *setup);
#endif /* SES_LEGACY_MODE */
static ses_eventi_t fsm_do_event(fsm_data_t *p, ses_eventi_t event);
static fsm_states_t fsm_stg_get_current_states(fsm_data_t const *p);
static void fsm_stg_set_current_states(fsm_data_t const *p, fsm_states_t states);
static void fsm_stg_set_last_status(fsm_data_t const *p, ses_status_t status);
static ses_status_t fsm_stg_get_last_status(fsm_data_t const *p);
#ifdef BCMDBG
static char const * fsm_dbg_lookup_str(fsm_dbg_str_map_t const *map, int idx);
#endif /* BCMDBG */
static void fsm_gen_ses(fsm_data_t const *p, ses_network_t *network, int force);
static void fsm_rwc_timer_cb(ses_timer_id tid, fsm_data_t *p);
static int fsm_timer_rwc_set(fsm_data_t *p, int time);
static void fsm_timer_rwc_delete(fsm_data_t *p);

static void fsmie_exec_event(fsm_data_t *p, int event);
static int fsmie_do_event(fsm_data_t *p, int event);
static int fsmie_state_tran(fsm_data_t *p, fsm_state_t state);
static void fsm_set_ie_flags(fsm_data_t *p, bool configured);
static int fsm_poll(ses_timer_id tid, fsm_data_t *fsm_data);
static void fsm_run(fsm_data_t *fsm_data);

#ifdef BCMDBG
static const fsm_dbg_str_map_t
sg_state_str_map[] = {
	{FSM_STATE_CONFIGURED, "FSM_STATE_CONFIGURED"},
	{FSM_STATE_UNCONFIGURED, "FSM_STATE_UNCONFIGURED"},
	{FSM_STATE_XCHG_INIT, "FSM_STATE_XCHG_INIT"},
	{FSM_STATE_LEGACY_XCHG_WAIT, "FSM_STATE_LEGACY_XCHG_WAIT"},
	{FSM_STATE_XCHG_DONE, "FSM_STATE_XCHG_DONE"},
	{FSM_STATE_XCHG_CONTINUE, "FSM_STATE_XCHG_CONTINUE"},
	{FSM_STATE_U_XCHG_INIT, "FSM_STATE_U_XCHG_INIT"},
	{FSM_STATE_LEGACY_U_XCHG_WAIT, "FSM_STATE_LEGACY_U_XCHG_WAIT"},
	{FSM_STATE_LEGACY_XCHG_INIT, "FSM_STATE_LEGACY_XCHG_INIT"},
	{FSM_STATE_LEGACY_U_XCHG_INIT, "FSM_STATE_LEGACY_U_XCHG_INIT"},
	{0, NULL}
};

static const fsm_dbg_str_map_t
sg_eventi_str_map[] = {
	{SES_EVTI_NULL, "SES_EVTI_NULL"},
	{SES_EVTI_STATUS, "SES_EVTI_STATUS"},
	{SES_EVTI_INIT_CONDITION, "SES_EVTI_INIT_CONDITION"},
	{SES_EVTI_NW_GEN_SES, "SES_EVTI_NW_GEN_SES"},
	{SES_EVTI_RWO, "SES_EVTI_RWO"},
	{SES_EVTI_RWO_CONTINUE, "SES_EVTI_RWO_CONTINUE"},
	{SES_EVTI_NW_GEN_SES_RWO, "SES_EVTI_NW_GEN_SES_RWO"},
	{SES_EVTI_NW_RESET, "SES_EVTI_NW_RESET"},
	{SES_EVTI_LEGACY_RWO, "SES_EVTI_LEGACY_RWO"},
	{SES_EVTI_LEGACY_NW_GEN_SES_RWO, "SES_EVTI_LEGACY_NW_GEN_SES_RWO"},
	{SES_EVTI_CANCEL, "SES_EVTI_CANCEL"},
	{0, NULL}
};

static const fsm_dbg_str_map_t
sg_state_ie_str_map[] = {
	{FSMIE_STATE_RWC, "FSMIE_STATE_RWC"},
	{FSMIE_STATE_RWO_AD, "FSMIE_STATE_RWO_AD"},
	{FSMIE_STATE_RWC_AD, "FSMIE_STATE_RWC_AD"},
	{0, NULL}
};

static const fsm_dbg_str_map_t
sg_eventi_ie_str_map[] = {
	{FSMIE_EVTI_ENTRY, "FSMIE_EVTI_ENTRY"},
	{FSMIE_EVTI_INIT_CONDITION, "FSMIE_EVTI_INIT_CONDITION"},
	{FSMIE_EVTI_RWO, "FSMIE_EVTI_RWO"},
	{FSMIE_EVTI_RDS, "FSMIE_EVTI_RDS"},
	{FSMIE_EVTI_CANCEL, "FSMIE_EVTI_CANCEL"},
	{FSMIE_EVTI_RWC_AD_TIMEOUT, "FSMIE_EVTI_RWC_AD_TIMEOUT"},
	{0, NULL}
};
#endif /* BCMDBG */

#define MAKEWORD(l, h) \
((uint16)(((uchar)((l) & 0xff)) | ((uint16)((uchar)((h) & 0xff))) << 8))

#define HIBYTE(w) ((uchar)((w) >> 8))
#define LOBYTE(w) ((uchar)((w) & 0xff))

#define STATE_IE(s)	HIBYTE(s)
#define STATE_CTRL(s)	LOBYTE(s)

#define FSMCTRL_SET_STATE(p, s) \
fsm_stg_set_current_states(\
p, (p)->states = MAKEWORD((s), STATE_IE((p)->states)))

#define FSMIE_SET_STATE(p, s) \
fsm_stg_set_current_states(\
p, (p)->states = MAKEWORD(STATE_CTRL((p)->states), (s)))

#ifdef SES_LEGACY_MODE
#define APPLY_NW_ON_FAILURE(p) (TRUE)
#endif /* SES_LEGACY_MODE */

#define SES_PE_2BUTTON	SES_PE_3BUTTON_PRE_CONFIRM|SES_PE_3BUTTON_POST_CONFIRM

#ifdef BCMDBG
static char const *
fsm_dbg_lookup_str(fsm_dbg_str_map_t const *map, int idx)
{
	int i;
	for (i = 0; map[i].name != NULL; i++)
		if (idx == map[i].idx)
			return map[i].name;
	return "UNKNOWN";
}
#endif /* BCMDBG */

#define SES_POLL_TIME 1 /* One second poll */

/* Run FSM in polled mode based on a timer */
static int
fsm_poll(ses_timer_id tid, fsm_data_t *fsm_data)
{
	ses_eventi_t event = SES_EVTI_NULL;
	bool noblock = TRUE;

	assert(tid == fsm_data->poll_tid);

	/* fsm event loop */
	event = fsm_data->cb->event_get(fsm_data, &noblock);
	if (fsm_data->noblock) {
		fsm_data->noblock = FALSE;
		if (fsm_data->ie_event) {
			fsmie_exec_event(fsm_data, fsm_data->ie_event);
			fsm_data->ie_event = FSMIE_EVTI_NULL;
		}
	} else if (event == SES_EVTI_NULL)
		return SES_FSM_STATUS_SUCCESS;

	while ((event = fsm_do_event(fsm_data, event)));

	return SES_FSM_STATUS_SUCCESS;
}

/* Run FSM in blocked mode */
static void
fsm_run(fsm_data_t *fsm_data)
{
	ses_eventi_t event = SES_EVTI_NULL;

	/* fsm event loop */
	while (1) {
		event = fsm_data->cb->event_get(fsm_data, &fsm_data->noblock);
		if (fsm_data->noblock) {
			fsm_data->noblock = FALSE;
			if (fsm_data->ie_event) {
				fsmie_exec_event(fsm_data, fsm_data->ie_event);
				fsm_data->ie_event = FSMIE_EVTI_NULL;
			}
		}

		while ((event = fsm_do_event(fsm_data, event)));

	}

	free(fsm_data);
}

/* fsm main entry */
int
init_fsm(ses_fsm_cb_t *cb, ses_fsm_setup_t *setup, void *client_data)
{
	fsm_data_t *fsm_data;
	ses_eventi_t event = SES_EVTI_NULL;

	fsm_data = (fsm_data_t *)malloc(sizeof(fsm_data_t));
	if (!fsm_data)  {
		SES_ERROR("malloc() failed\n");
		return SES_FSM_STATUS_INTERNAL_ERROR;
	}

	memset(fsm_data, 0, sizeof(fsm_data_t));

	fsm_data->states = MAKEWORD(FSM_STATE_UNCONFIGURED, FSMIE_STATE_RWC);
	fsm_data->setup = setup;
	fsm_data->cb = cb;
	fsm_data->client_data = client_data;
	fsm_data->ie_event = FSMIE_EVTI_NULL;

	/* validate callback data */
	if (fsm_validate_cb_data(cb)) {
		SES_ERROR("invalid or incomplete callback structure\n");
		return SES_FSM_STATUS_INTERNAL_ERROR;
	}

	event = fsm_set_initial_state(fsm_data);

	while ((event = fsm_do_event(fsm_data, event)));

	if (setup->poll) {
		ses_timer_start(&fsm_data->poll_tid, SES_POLL_TIME,
		                (ses_timer_cb)fsm_poll, fsm_data);
		fsm_poll(fsm_data->poll_tid, fsm_data);
	} else
		fsm_run(fsm_data);
	return SES_FSM_STATUS_SUCCESS;
}

int
fsm_get_client_data(ses_fsm_ctx_t ctx, void **client_data)
{
	fsm_data_t *p = (fsm_data_t *)ctx;
	*client_data = p->client_data;
	return SES_FSM_STATUS_SUCCESS;
}

int
fsm_op_req_reinit(ses_fsm_ctx_t ctx)
{
	fsm_data_t *p = (fsm_data_t *)ctx;
	SES_INFO("reinit req\n");
	if (p->reinit == FALSE)
		p->reinit = TRUE;
	return SES_FSM_STATUS_SUCCESS;
}

static int
fsm_validate_cb_data(ses_fsm_cb_t *cb)
{
	return 0;
}

static ses_eventi_t
fsm_set_initial_state(fsm_data_t *p)
{
	bool noblock = TRUE;
	ses_eventi_t last_event;
	ses_eventi_t event = p->cb->event_get(p, &noblock);

	p->states = fsm_stg_get_current_states(p);

	fsmie_exec_event(p, FSMIE_EVTI_ENTRY);

	while ((last_event = event)) {
		event = fsm_do_event(p, event);
	}

	if (last_event != SES_EVTI_STATUS)
		event = fsm_do_event(p, SES_EVTI_STATUS);

	return event;
}

/* Look for and parse "%02x:%02X" */
static int
_fsm_get_ctrl_ie(char *buf, uint *ctrl, uint *ie)
{
	char *ctrl_str;
	char *ie_str;
	ctrl_str = buf;
	ie_str = strchr(buf, ':');

	/* Format didn't match */
	if (!ie_str)
		return 0;

	*ie_str++ = '\0';
	*ctrl = strtoul(ctrl_str, NULL, 16);
	*ie = strtoul(ie_str, NULL, 16);

	return 2;
}

static fsm_states_t
fsm_stg_get_current_states(fsm_data_t const *p)
{
	fsm_states_t ret;
	char buf[MAX_STATES_STR_LEN];
	uint ctrl, ie;

	if (p->cb->stg_get(p, FSM_STG_CURRENT_STATES,
	                   buf, MAX_STATES_STR_LEN) ||
	    _fsm_get_ctrl_ie(buf, &ctrl, &ie) == 0) {
		ret = MAKEWORD(FSM_STATE_UNCONFIGURED, FSMIE_STATE_RWC);
	} else {
		ret = MAKEWORD(ctrl, ie);
	}

	return ret;
}

static void
fsm_stg_set_current_states(fsm_data_t const *p, fsm_states_t states)
{
	char buf[MAX_STATES_STR_LEN];
	sprintf(buf, "%02X:%02X", STATE_CTRL(states), STATE_IE(states));
	p->cb->stg_set(p, FSM_STG_CURRENT_STATES, buf, MAX_STATES_STR_LEN);
}

static void
fsm_stg_set_last_status(fsm_data_t const *p, ses_status_t status)
{
	char buf[MAX_STATUS_STR_LEN];
	sprintf(buf, "%d", status);
	p->cb->stg_set(p, FSM_STG_LAST_STATUS, buf, MAX_STATUS_STR_LEN);
}

static ses_status_t
fsm_stg_get_last_status(fsm_data_t const *p)
{
	char buf[MAX_STATUS_STR_LEN];
	return p->cb->stg_get(p,
	                      FSM_STG_LAST_STATUS,
	                      buf,
	                      MAX_STATUS_STR_LEN) ?
		SES_STATUS_UNKNOWN : atoi(buf);
}

static ses_eventi_t
fsm_state_tran(fsm_data_t *p, fsm_state_t state, ses_status_t status)
{
	ses_eventi_t ret = SES_EVTI_NULL;

	if (status != SES_STATUS_UNKNOWN)
		fsm_stg_set_last_status(p, status);

	if (state != FSM_STATE_UNKNOWN && STATE_CTRL(p->states) != state) {
		ret = SES_EVTI_STATUS;
		SES_INFO("%s(%d) -> %s(%d), reinit=%d\n",
		         fsm_dbg_lookup_str(sg_state_str_map,
		                            STATE_CTRL(p->states)),
		         STATE_CTRL(p->states),
		         fsm_dbg_lookup_str(sg_state_str_map,
		                            state),
		         state,
		         p->reinit);
		FSMCTRL_SET_STATE(p, state);
		if (p->reinit == TRUE) {
			p->cb->sys_reinit(p);
			SES_ERROR("fsm_state_tran did not sys_reinit()!\n");
		}
	}

	return ret;
}

static void
fsm_init_pe(ses_packet_exchange_t *pe,
            ses_network_t const *network,
            int phase,
            ses_fsm_setup_t const *setup)
{
	memset(pe, 0, sizeof(ses_packet_exchange_t));
	pe->version = 1;
	pe->mode = SES_PE_MODE_CONFIGURATOR;
	pe->phase = phase;
	strncpy(pe->ssid, network->ssid, strlen(network->ssid));
	pe->security = network->security;
	pe->encryption = network->encryption;

	assert(strlen(network->passphrase) <= SES_MAX_PASSPHRASE_LEN);
	strcpy(pe->passphrase, network->passphrase);

	strncpy(pe->ifnames, setup->ifnames, strlen(setup->ifnames));
	strncpy(pe->wl_ifname, setup->def_wlname, strlen(setup->def_wlname));
}

#ifdef SES_LEGACY_MODE
static int
fsm_exec_pe(ses_packet_exchange_t *pe, ses_network_t const *network,
	int phase, ses_fsm_setup_t const *setup)
{
	int status;

	fsm_init_pe(pe, network, phase, setup);
	SES_setup(pe);
	status = SES_packet_exchange(pe, setup->ow_time);
	SES_cleanup(pe);
	return status;
}
#endif /* SES_LEGACY_MODE */

static void
fsm_gen_ses(fsm_data_t const *p, ses_network_t *network, int force)
{
	int i = 0;
	uint8 rn;

	p->cb->network_gen_ses(p, p->setup->def_wlname, network, force);

	if (strlen(network->passphrase) == 0) {
		while (i < SES_DEF_PASSPHRASE_LEN) {
			ses_random(&rn, 1);
			/* Make sure that it's an ascii character before using it */
			if ((islower(rn) || isdigit(rn)) && (rn < 0x7f)) {
				network->passphrase[i++] = rn;
			}
		}
		network->passphrase[i] = '\0';
	}

	assert(strlen(network->passphrase) <= SES_MAX_PASSPHRASE_LEN);

	SES_INFO("ssid=%s, passphrase=%s\n", network->ssid, network->passphrase);
}

static void
fsm_rwc_timer_cb(ses_timer_id tid, fsm_data_t *p)
{
	if (p->rwc_tid == tid) {
		p->noblock = TRUE;
		p->ie_event = FSMIE_EVTI_RWC_AD_TIMEOUT;
		SES_INFO("timer (%d) expired\n", tid);
	} else {
		SES_ERROR("fsm_rwc_timer_cb tid mismatch %d/%d\n",
		          tid, p->rwc_tid);
	}
}

static int
fsm_timer_rwc_set(fsm_data_t *p, int time)
{
	int ret;
	if (p->rwc_tid) {
		SES_INFO("timer already started!\n");
		return 0;
	}
	ret = ses_timer_start(&p->rwc_tid, time,
	                      (ses_timer_cb)fsm_rwc_timer_cb, p);

	SES_INFO("starting rwc_timer (%d sec) in state %s... %s\n",
	         time,
	         fsm_dbg_lookup_str(sg_state_str_map, STATE_CTRL(p->states)),
	         ret ? "failed!" : "succeeded!");

	return ret;
}

static void
fsm_timer_rwc_delete(fsm_data_t *p)
{
	if (p->rwc_tid) {
		SES_INFO("deleting rwc_timer in state %s\n",
		         fsm_dbg_lookup_str(sg_state_str_map,
		                            STATE_CTRL(p->states)));
		ses_timer_stop(p->rwc_tid);
		p->rwc_tid = 0;
	}
}

static void
fsm_set_ie_flags(fsm_data_t *p, bool configured)
{
	if (p->setup->wds_mode == SES_WDS_MODE_AUTO) {
		/* wds if configured */
		p->ses_ie_flags = SES_VNDR_IE_FLAG_RWO;
		if (configured)
			p->ses_ie_flags |= SES_VNDR_IE_FLAG_WDS_RWO;
	} else if (p->setup->wds_mode == SES_WDS_MODE_DISABLED) {
		/* wds disabled */
		p->ses_ie_flags = SES_VNDR_IE_FLAG_RWO;
	} else if (p->setup->wds_mode == SES_WDS_MODE_ENABLED_ALWAYS) {
		/* wds always */
		p->ses_ie_flags = SES_VNDR_IE_FLAG_RWO | SES_VNDR_IE_FLAG_WDS_RWO;
	} else if (p->setup->wds_mode == SES_WDS_MODE_ENABLED_EXCL) {
		/* wds only */
		p->ses_ie_flags = SES_VNDR_IE_FLAG_WDS_RWO;
	}

	if (p->ses_ie_flags & SES_VNDR_IE_FLAG_WDS_RWO) {
		SES_INFO("WDS in ie enabled\n");
		p->enable_wds = TRUE;
	} else {
		SES_INFO("WDS in ie disabled\n");
		p->enable_wds = FALSE;
	}
}


#define FSM_DISPLAY_STATE_INFO(sm, state, em, event) \
SES_INFO("state=%s(%d), event=%s(%d)\n", \
fsm_dbg_lookup_str((sm), (state)), \
(state), \
fsm_dbg_lookup_str(em, (event)), \
(event))

#define GOTO_STATE(p, s, e) \
do { \
SES_INFO("GOTO_STATE (%s(%d) -> %s(%d) on %s(%d))\n", \
fsm_dbg_lookup_str(sg_state_str_map, STATE_CTRL((p)->states)), \
STATE_CTRL((p)->states), \
fsm_dbg_lookup_str(sg_state_str_map, (s)), (s), \
fsm_dbg_lookup_str(sg_eventi_str_map, (e)), (e)); \
FSMCTRL_SET_STATE((p), (s)); \
return (e); \
} while (0)

#define APPLY_IF_REINIT(p) \
if ((p)->reinit == TRUE) \
(p)->cb->sys_reinit((p)); \
while (0)

#define STATE_TRAN(n, s) next_state = (n); status = (s)

#define STATE_TRAN_IS_SES_NW(p, status) \
STATE_TRAN((p)->cb->network_is_ses((p), (p)->setup->def_wlname) ? \
FSM_STATE_CONFIGURED : FSM_STATE_UNCONFIGURED, (status))

#define NOTIFY(p, eo, s) \
do { \
e.evto = (eo); \
e.status = (s); \
(p)->cb->event_notify((p), (p)->setup->def_wlname, &e); \
} while (0)

#define NOTIFY_WITH_EA(p, eo, s, ea) \
do { \
e.evto = (eo); \
e.status = (s); \
if ((ea)) memcpy(&e.u.rem_ea, (ea), ETHER_ADDR_LEN); \
(p)->cb->event_notify((p), (p)->setup->def_wlname, &e); \
} while (0)

#define NOTIFY_WITH_CONF(p, eo, s, conf) \
do { \
e.evto = (eo); \
e.status = (s); \
e.u.configured = conf; \
(p)->cb->event_notify((p), (p)->setup->def_wlname, &e); \
} while (0)

static ses_eventi_t
fsm_do_event(fsm_data_t *p, ses_eventi_t event)
{
	int status = SES_STATUS_UNKNOWN;
	int pe_status, s;
	fsm_state_t next_state = FSM_STATE_UNKNOWN;
	ses_event_t e;
	ses_network_t network;

	memset(&network, 0, sizeof(ses_network_t));
	memset(&e, 0, sizeof(ses_event_t));

	FSM_DISPLAY_STATE_INFO(sg_state_str_map, STATE_CTRL(p->states),
	                       sg_eventi_str_map, event);

	switch (STATE_CTRL(p->states)) {
	case FSM_STATE_UNCONFIGURED:
		if (p->cb->network_is_ses(p, p->setup->def_wlname))
			GOTO_STATE(p, FSM_STATE_CONFIGURED, event);
		switch (event) {
		case SES_EVTI_STATUS:
			NOTIFY(p, SES_EVTO_UNCONFIGURED, fsm_stg_get_last_status(p));
			break;
		case SES_EVTI_NW_RESET:
			p->cb->network_reset(p, p->setup->def_wlname);
			fsm_stg_set_last_status(p, SES_STATUS_SUCCESS);
			APPLY_IF_REINIT(p);
			break;
		case SES_EVTI_INIT_CONDITION:
			fsmie_exec_event(p, FSMIE_EVTI_INIT_CONDITION);
			break;
		case SES_EVTI_NW_GEN_SES:
			fsm_gen_ses(p, &network, TRUE);
			p->cb->network_set(p, p->setup->def_wlname, &network);
			STATE_TRAN(FSM_STATE_CONFIGURED, SES_STATUS_SUCCESS);
			break;
#ifdef SES_LEGACY_MODE
		case SES_EVTI_LEGACY_NW_GEN_SES_RWO:
			p->cb->network_gen_rnd(p, &network);
			p->cb->network_store(p, p->setup->def_wlname);
			p->cb->network_set_bridge(p, FALSE);
			p->cb->network_set(p, p->setup->def_wlname, &network);
			STATE_TRAN(FSM_STATE_LEGACY_U_XCHG_INIT, SES_STATUS_SUCCESS);
			break;
#endif /* SES_LEGACY_MODE */
		case SES_EVTI_NW_GEN_SES_RWO:
			p->cb->network_store(p, p->setup->def_wlname);
			fsm_gen_ses(p, &network, FALSE);
			p->cb->network_set(p, p->setup->def_wlname, &network);
			STATE_TRAN(FSM_STATE_U_XCHG_INIT, SES_STATUS_SUCCESS);
			break;
		case SES_EVTI_RWO:
			fsm_stg_set_last_status(p, SES_STATUS_FAILURE);
			NOTIFY(p, SES_EVTO_UNCONFIGURED, SES_STATUS_FAILURE);
			break;
		}
		break;
	case FSM_STATE_CONFIGURED:
		if (!p->cb->network_is_ses(p, p->setup->def_wlname))
			GOTO_STATE(p, FSM_STATE_UNCONFIGURED, event);
		switch (event) {
		case SES_EVTI_STATUS:
			NOTIFY(p, SES_EVTO_CONFIGURED, fsm_stg_get_last_status(p));
			break;
		case SES_EVTI_NW_RESET:
			p->cb->network_reset(p, p->setup->def_wlname);
			STATE_TRAN(FSM_STATE_UNCONFIGURED, SES_STATUS_SUCCESS);
			break;
		case SES_EVTI_INIT_CONDITION:
			fsmie_exec_event(p, FSMIE_EVTI_INIT_CONDITION);
			break;
		case SES_EVTI_NW_GEN_SES:
			fsm_gen_ses(p, &network, TRUE);
			p->cb->network_set(p, p->setup->def_wlname, &network);
			fsm_stg_set_last_status(p, SES_STATUS_SUCCESS);
			APPLY_IF_REINIT(p);
			break;
#ifdef SES_LEGACY_MODE
		case SES_EVTI_LEGACY_RWO:
		case SES_EVTI_LEGACY_NW_GEN_SES_RWO:
			p->cb->network_store(p, p->setup->def_wlname);
			p->cb->network_gen_rnd(p, &network);
			p->cb->network_set_bridge(p, FALSE);
			p->cb->network_set(p, p->setup->def_wlname, &network);
			STATE_TRAN(FSM_STATE_LEGACY_XCHG_INIT, SES_STATUS_SUCCESS);
			break;
#endif /* SES_LEGACY_MODE */
		case SES_EVTI_RWO:
		case SES_EVTI_NW_GEN_SES_RWO:
			p->cb->network_store(p, p->setup->def_wlname);
			STATE_TRAN(FSM_STATE_XCHG_INIT, SES_STATUS_SUCCESS);
			break;
		}
		break;
	case FSM_STATE_U_XCHG_INIT:
	case FSM_STATE_XCHG_INIT:
		switch (event) {
		case SES_EVTI_STATUS:
			NOTIFY_WITH_CONF(p, SES_EVTO_RWO, SES_STATUS_SUCCESS,
			                 (STATE_CTRL(p->states) == FSM_STATE_XCHG_INIT) ?
			                 TRUE : FALSE);

			fsm_set_ie_flags(p,
				(STATE_CTRL(p->states) == FSM_STATE_XCHG_INIT) ?
			                 TRUE : FALSE);

			fsmie_exec_event(p, FSMIE_EVTI_RWO);

			p->noblock = TRUE;
			p->exch_initiated_notified = FALSE;

			ses_dot11_transition_mode(p->setup->def_wlname, TRUE);
			if (p->enable_wds)
				p->old_lazywds = ses_dot11_lazywds_set(p->setup->def_wlname, 1);

			/* ** 2WAY ** */
			if (p->setup->mode == SES_SETUP_MODE_2WAY) {
			p->cb->network_get(p,
			                   p->setup->def_wlname, &network);

			fsm_init_pe(&p->pe, &network, SES_PE_2BUTTON, p->setup);
			SES_setup(&p->pe);
			p->rem_ow_time = p->setup->ow_time;

			STATE_TRAN(FSM_STATE_XCHG_CONTINUE, SES_STATUS_SUCCESS);
			/* ** 3WAY ** */
			} else if (p->setup->mode == SES_SETUP_MODE_3WAY) {
			/* no support as yet */
			assert(0);
			}

			break;
		case SES_EVTI_INIT_CONDITION:
			fsmie_exec_event(p, FSMIE_EVTI_INIT_CONDITION);
			STATE_TRAN_IS_SES_NW(p, SES_STATUS_UNKNOWN);
			break;
		}
		break;

	case FSM_STATE_XCHG_CONTINUE:
		p->noblock = TRUE;
		switch (event) {
		case SES_EVTI_STATUS:
		case SES_EVTI_NULL:
			/* ** 2WAY ** */
			if (p->setup->mode == SES_SETUP_MODE_2WAY) {

			pe_status = SES_packet_exchange(&p->pe, 1);
			p->rem_ow_time--;

			switch (pe_status) {
			case SES_PE_STATUS_SUCCESS:
				fsmie_exec_event(p, FSMIE_EVTI_RDS);
				STATE_TRAN(FSM_STATE_XCHG_DONE, SES_STATUS_SUCCESS);
				break;
			case SES_PE_STATUS_INITIATED:
				if (p->exch_initiated_notified == FALSE) {
					NOTIFY_WITH_EA(p, SES_EVTO_EXCH_INITIATED,
						SES_STATUS_SUCCESS, &p->pe.remote);
					p->exch_initiated_notified = TRUE;
					/* check if exchange is thru wds */
					if (p->enable_wds) {
						p->xchg_thru_wds = ses_dot11_is_wds_link_up(
							p->setup->def_wlname,
							&p->pe.remote);
					}
				}
				/* fall through */
			case SES_PE_STATUS_CONTINUE:
				/* somebody is clearing the bit in wsec, so again */
				ses_dot11_transition_mode(p->setup->def_wlname, TRUE);
				if (p->rem_ow_time <= 0) {
					fsmie_exec_event(p, FSMIE_EVTI_CANCEL);
					STATE_TRAN(FSM_STATE_XCHG_DONE, SES_STATUS_FAILURE);
				}
				break;
			default:
				fsmie_exec_event(p, FSMIE_EVTI_CANCEL);
				STATE_TRAN(FSM_STATE_XCHG_DONE, SES_STATUS_FAILURE);
			} /* end-switch */

			/* ** 3WAY ** */
			} else if (p->setup->mode == SES_SETUP_MODE_3WAY) {
			/* no support as yet */
			assert(0);
			}

			break;
		case SES_EVTI_CANCEL:
			fsmie_exec_event(p, FSMIE_EVTI_CANCEL);
			STATE_TRAN(FSM_STATE_XCHG_DONE, SES_STATUS_FAILURE);
			break;
		case SES_EVTI_INIT_CONDITION:
			fsmie_exec_event(p, FSMIE_EVTI_INIT_CONDITION);
			STATE_TRAN_IS_SES_NW(p, SES_STATUS_UNKNOWN);
			break;
		}
		break;

	case FSM_STATE_XCHG_DONE:
		switch (event) {
		case SES_EVTI_STATUS:
			s = fsm_stg_get_last_status(p);
			if (s == SES_STATUS_SUCCESS) {
				NOTIFY_WITH_EA(p, SES_EVTO_RWC, s, &p->pe.remote);
				if (p->xchg_thru_wds)
					p->cb->wds_add(p, p->setup->def_wlname,
					               p->pe.ssid, p->pe.passphrase,
					               &p->pe.remote);
			} else {
				NOTIFY(p, SES_EVTO_RWC, s);
			}
			SES_cleanup(&p->pe);
			ses_dot11_transition_mode(p->setup->def_wlname, FALSE);
			if (p->enable_wds)
				ses_dot11_lazywds_set(p->setup->def_wlname, p->old_lazywds);
			STATE_TRAN_IS_SES_NW(p, fsm_stg_get_last_status(p));

			break;
		case SES_EVTI_INIT_CONDITION:
			fsmie_exec_event(p, FSMIE_EVTI_INIT_CONDITION);
			STATE_TRAN_IS_SES_NW(p, SES_STATUS_UNKNOWN);
			break;
		}
		break;

#ifdef SES_LEGACY_MODE
	case FSM_STATE_LEGACY_XCHG_INIT:
		switch (event) {
		case SES_EVTI_STATUS:
			NOTIFY(p, SES_EVTO_RWO, SES_STATUS_SUCCESS, NULL);

			fsmie_exec_event(p, FSMIE_EVTI_RWO);

		/* ** 2WAY ** */
			if (p->setup->mode == SES_SETUP_MODE_2WAY) {
			p->cb->network_recall(p,
			                      p->setup->def_wlname, &network);
			pe_status = fsm_exec_pe(&p->pe,
			                        &network,
			                        SES_PE_2BUTTON,
			                        p->setup);
			p->cb->network_restore(p,
			                       p->setup->def_wlname);
			p->cb->network_set_bridge(p, TRUE);
			switch (pe_status) {
			case SES_PE_STATUS_SUCCESS:
				fsmie_exec_event(p, FSMIE_EVTI_RDS);
				STATE_TRAN(FSM_STATE_XCHG_DONE, SES_STATUS_SUCCESS);
				break;
			case SES_PE_STATUS_CONTINUE:
				fsmie_exec_event(p, FSMIE_EVTI_CANCEL);
				STATE_TRAN(FSM_STATE_CONFIGURED, SES_STATUS_FAILURE);
				break;
			default:
				fsmie_exec_event(p, FSMIE_EVTI_RDS);
				STATE_TRAN(FSM_STATE_CONFIGURED, SES_STATUS_FAILURE);
			} /* end-switch */

		/* ** 3WAY ** */
			} else if (p->setup->mode == SES_SETUP_MODE_3WAY) {
			pe_status = fsm_exec_pe(&p->pe,
			                        NULL,
			                        SES_PE_3BUTTON_PRE_CONFIRM,
			                        p->setup);
			if (pe_status) {
				fsmie_exec_event(p, FSMIE_EVTI_CANCEL);
				p->cb->network_restore(p,
				                       p->setup->def_wlname);
				p->cb->network_set_bridge(p, TRUE);
				STATE_TRAN(FSM_STATE_CONFIGURED, SES_STATUS_FAILURE);
			} else {
				STATE_TRAN(FSM_STATE_LEGACY_XCHG_WAIT,
				           SES_STATUS_SUCCESS);
			}

		/* ** UNKNOWN ** */
			} else {
			assert(0);
			}
			break;
		case SES_EVTI_INIT_CONDITION:
			p->cb->network_restore(p, p->setup->def_wlname);
			fsmie_exec_event(p, FSMIE_EVTI_INIT_CONDITION);
			STATE_TRAN_IS_SES_NW(p, SES_STATUS_UNKNOWN);
			break;
		}
		break;
	case FSM_STATE_LEGACY_XCHG_WAIT:
		switch (event) {
		case SES_EVTI_STATUS:
			NOTIFY(p, SES_EVTO_RWO_CONFIRM, SES_STATUS_SUCCESS, NULL);
			break;
		case SES_EVTI_RWO_CONTINUE:
			p->cb->network_recall(p,
			                      p->setup->def_wlname, &network);
			pe_status = fsm_exec_pe(&p->pe,
			                        &network,
			                        SES_PE_3BUTTON_POST_CONFIRM,
			                        p->setup);
			p->cb->network_restore(p, p->setup->def_wlname);
			p->cb->network_set_bridge(p, TRUE);
			switch (pe_status) {
			case SES_PE_STATUS_SUCCESS:
				fsmie_exec_event(p, FSMIE_EVTI_RDS);
				STATE_TRAN(FSM_STATE_XCHG_DONE, SES_STATUS_SUCCESS);
				break;
			case SES_PE_STATUS_CONTINUE:
				fsmie_exec_event(p, FSMIE_EVTI_CANCEL);
				STATE_TRAN(FSM_STATE_CONFIGURED, SES_STATUS_FAILURE);
				break;
			default:
				fsmie_exec_event(p, FSMIE_EVTI_RDS);
				STATE_TRAN(FSM_STATE_CONFIGURED, SES_STATUS_FAILURE);
			}
			break;
		case SES_EVTI_INIT_CONDITION:
			p->cb->network_restore(p, p->setup->def_wlname);
			fsmie_exec_event(p, FSMIE_EVTI_INIT_CONDITION);
			STATE_TRAN_IS_SES_NW(p, SES_STATUS_UNKNOWN);
			break;
		}
		break;
	case FSM_STATE_LEGACY_U_XCHG_INIT:
		switch (event) {
		case SES_EVTI_STATUS:
			NOTIFY(p, SES_EVTO_RWO, SES_STATUS_SUCCESS, NULL);

			fsmie_exec_event(p, FSMIE_EVTI_RWO);

		/* ** 2WAY ** */
			if (p->setup->mode == SES_SETUP_MODE_2WAY) {
			fsm_gen_ses(p, &network, FALSE);
			pe_status = fsm_exec_pe(&p->pe,
			                        &network,
			                        SES_PE_2BUTTON,
			                        p->setup);
			p->cb->network_set_bridge(p, TRUE);
			if (pe_status) {
				if (APPLY_NW_ON_FAILURE(p)) {
					p->cb->network_set(p,
					                   p->setup->def_wlname,
					                   &network);
					STATE_TRAN(FSM_STATE_CONFIGURED,
					           SES_STATUS_FAILURE);
				} else {
					p->cb->network_restore(p,
					                       p->setup->def_wlname);
					STATE_TRAN(FSM_STATE_UNCONFIGURED,
					           SES_STATUS_FAILURE);
				}
				if (pe_status == SES_PE_STATUS_CONTINUE) {
					fsmie_exec_event(p, FSMIE_EVTI_CANCEL);
				} else {
					fsmie_exec_event(p, FSMIE_EVTI_RDS);
				}
			} else {
				p->cb->network_set(p,
				                   p->setup->def_wlname,
				                   &network);
				fsmie_exec_event(p, FSMIE_EVTI_RDS);
				STATE_TRAN(FSM_STATE_XCHG_DONE, SES_STATUS_SUCCESS);
			}

		/* ** 3WAY ** */
			} else if (p->setup->mode == SES_SETUP_MODE_3WAY) {
			pe_status = fsm_exec_pe(&p->pe,
			                        NULL,
			                        SES_PE_3BUTTON_PRE_CONFIRM,
			                        p->setup);
			if (pe_status) {
				fsmie_exec_event(p, FSMIE_EVTI_CANCEL);
				if (APPLY_NW_ON_FAILURE(p)) {
					fsm_gen_ses(p, &network, FALSE);
					p->cb->network_set(p,
					                   p->setup->def_wlname, &network);
					STATE_TRAN(FSM_STATE_CONFIGURED,
					           SES_STATUS_FAILURE);
				} else {
					p->cb->network_restore(p, p->setup->def_wlname);
					STATE_TRAN(FSM_STATE_UNCONFIGURED,
					           SES_STATUS_FAILURE);
				}
				p->cb->network_set_bridge(p, TRUE);
			} else {
				STATE_TRAN(FSM_STATE_LEGACY_U_XCHG_WAIT,
				           SES_STATUS_SUCCESS);
			}

		/* ** UNKNOWN ** */
			} else {
				assert(0);
			}
			break;
		case SES_EVTI_INIT_CONDITION:
			p->cb->network_restore(p, p->setup->def_wlname);
			fsmie_exec_event(p, FSMIE_EVTI_INIT_CONDITION);
			STATE_TRAN_IS_SES_NW(p, SES_STATUS_UNKNOWN);
			break;
		}
		break;
	case FSM_STATE_LEGACY_U_XCHG_WAIT:
		switch (event) {
		case SES_EVTI_STATUS:
			NOTIFY(p, SES_EVTO_RWO_CONFIRM, SES_STATUS_SUCCESS, NULL);
			break;
		case SES_EVTI_RWO_CONTINUE:
			fsm_gen_ses(p, &network, FALSE);
			pe_status = fsm_exec_pe(&p->pe,
			                        &network,
			                        SES_PE_3BUTTON_POST_CONFIRM,
			                        p->setup);
			p->cb->network_set_bridge(p, TRUE);
			if (pe_status) {
				if (APPLY_NW_ON_FAILURE(p)) {
					p->cb->network_set(p,
					                   p->setup->def_wlname,
					                   &network);
					STATE_TRAN(FSM_STATE_CONFIGURED,
					           SES_STATUS_FAILURE);
				} else {
					p->cb->network_restore(p,
					                       p->setup->def_wlname);
					STATE_TRAN(FSM_STATE_UNCONFIGURED,
					           SES_STATUS_FAILURE);
				}
				if (pe_status == SES_PE_STATUS_CONTINUE) {
					fsmie_exec_event(p, FSMIE_EVTI_CANCEL);
				} else {
					fsmie_exec_event(p, FSMIE_EVTI_RDS);
				}
			} else {
				p->cb->network_set(p,
				                   p->setup->def_wlname,
				                   &network);
				fsmie_exec_event(p, FSMIE_EVTI_RDS);
				STATE_TRAN(FSM_STATE_XCHG_DONE, SES_STATUS_SUCCESS);
			}
			break;
		case SES_EVTI_INIT_CONDITION:
			p->cb->network_restore(p, p->setup->def_wlname);
			fsmie_exec_event(p, FSMIE_EVTI_INIT_CONDITION);
			STATE_TRAN_IS_SES_NW(p, SES_STATUS_UNKNOWN);
			break;
		}
		break;
#endif /* SES_LEGACY_MODE */
	default:
		assert(0);
	}

	return fsm_state_tran(p, next_state, status);
}

static void
fsmie_exec_event(fsm_data_t *p, int event)
{
	while (event != FSMIE_EVTI_NULL)
		event = fsmie_do_event(p, event);
}

static int
fsmie_do_event(fsm_data_t *p, int event)
{
	fsm_state_t next_state = FSM_STATE_UNKNOWN;
	static ses_vndr_ie_t s_ses_ad = {
		SES_VNDR_IE_TYPE,
		SES_VNDR_IE_SUBTYPE_CFG_AD,
		SES_VNDR_IE_VERSION,
		0
	};

	FSM_DISPLAY_STATE_INFO(sg_state_ie_str_map, STATE_IE(p->states),
	                       sg_eventi_ie_str_map, event);

	switch (STATE_IE(p->states)) {
	case FSMIE_STATE_RWC:
		switch (event) {
		case FSMIE_EVTI_RWO:
			next_state = FSMIE_STATE_RWO_AD;
			break;
		}
		break;
	case FSMIE_STATE_RWO_AD:
		SES_VNDR_IE_SET_RWC(&s_ses_ad);
		SES_VNDR_IE_SET_RWO(&s_ses_ad, p->ses_ie_flags);
		switch (event) {
		case FSMIE_EVTI_ENTRY:
			SES_VNDR_IE_SAFE_ADD(p->setup->def_wlname,
			                     &s_ses_ad,
			                     sizeof(s_ses_ad));
			break;
		case FSMIE_EVTI_CANCEL:
		case FSMIE_EVTI_INIT_CONDITION:
			SES_VNDR_IE_SAFE_DEL(p->setup->def_wlname,
			                     &s_ses_ad,
			                     sizeof(s_ses_ad));
			next_state = FSMIE_STATE_RWC;
			break;
		case FSMIE_EVTI_RDS:
			SES_VNDR_IE_SAFE_DEL(p->setup->def_wlname,
			                     &s_ses_ad,
			                     sizeof(s_ses_ad));
			next_state = FSMIE_STATE_RWC_AD;
			break;
		}
		break;
	case FSMIE_STATE_RWC_AD:
		SES_VNDR_IE_SET_RWC(&s_ses_ad);
		switch (event) {
		case FSMIE_EVTI_ENTRY:
			SES_VNDR_IE_SAFE_ADD(p->setup->def_wlname,
			                     &s_ses_ad,
			                     sizeof(s_ses_ad));
			fsm_timer_rwc_set(p, p->setup->ow_time);
			break;
		case FSMIE_EVTI_RWO:
			SES_VNDR_IE_SAFE_DEL(p->setup->def_wlname,
			                     &s_ses_ad,
			                     sizeof(s_ses_ad));
			fsm_timer_rwc_delete(p);
			next_state = FSMIE_STATE_RWO_AD;
			break;
		case FSMIE_EVTI_CANCEL:
		case FSMIE_EVTI_INIT_CONDITION:
		case FSMIE_EVTI_RWC_AD_TIMEOUT:
			SES_VNDR_IE_SAFE_DEL(p->setup->def_wlname,
			                     &s_ses_ad,
			                     sizeof(s_ses_ad));
			fsm_timer_rwc_delete(p);
			next_state = FSMIE_STATE_RWC;
			break;
		}
		break;
	}

	return fsmie_state_tran(p, next_state);
}

static int
fsmie_state_tran(fsm_data_t *p, fsm_state_t state)
{
	int ret = FSMIE_EVTI_NULL;

	if (state != FSMIE_STATE_UNKNOWN && STATE_IE(p->states) != state) {
		FSMIE_SET_STATE(p, state);
		ret = FSMIE_EVTI_ENTRY;
	}

	return ret;
}
