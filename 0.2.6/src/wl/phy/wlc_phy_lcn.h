/*
 * LCNPHY module header file
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_lcn.h 280426 2011-08-29 21:08:23Z gpasrija $
 */

#ifndef _wlc_phy_lcn_h_
#define _wlc_phy_lcn_h_

#include <typedefs.h>
#include <wlc_phy_int.h>

#define LCNPHY_SWCTRL_NVRAM_PARAMS 5

struct phy_info_lcnphy {
	uint8 	lcnphy_cal_counter;
	bool	lcnphy_recal;
	bool	lcnphy_papd_cal_done_at_init;
	uint32  lcnphy_mcs20_po;

	uint8	lcnphy_tr_isolation_mid;	/* TR switch isolation for each sub-band */

	uint8	lcnphy_rx_power_offset;	 /* Input power offset */
	uint8	lcnphy_rssi_vf;		/* RSSI Vmid fine */
	uint8	lcnphy_rssi_vc;		/* RSSI Vmid coarse */
	uint8	lcnphy_rssi_gs;		/* RSSI gain select */
	uint8	lcnphy_rssi_vf_lowtemp;	/* RSSI Vmid fine */
	uint8	lcnphy_rssi_vc_lowtemp; /* RSSI Vmid coarse */
	uint8	lcnphy_rssi_gs_lowtemp; /* RSSI gain select */

	uint8	lcnphy_rssi_vf_hightemp;	/* RSSI Vmid fine */
	uint8	lcnphy_rssi_vc_hightemp;	/* RSSI Vmid coarse */
	uint8	lcnphy_rssi_gs_hightemp;	/* RSSI gain select */

	uint8	lcnphy_vbat_vf;		/* Vbatsense Vmid fine */
	uint8	lcnphy_vbat_vc;		/* Vbatsense Vmid coarse */
	uint8	lcnphy_vbat_gs;		/* Vbatsense gain select */

	uint8	lcnphy_temp_vf;		/* Tempsense Vmid fine */
	uint8	lcnphy_temp_vc;		/* Tempsense Vmid coarse */
	uint8	lcnphy_temp_gs;		/* Tempsense gain select */

	/* next 3 are for tempcompensated tx power control algo of 4313A0 */
	uint16 	lcnphy_rawtempsense;
	uint8   lcnphy_measPower;
	uint8   lcnphy_measPower1;
	uint8   lcnphy_measPower2;
	uint8  	lcnphy_tempsense_slope;
	uint8	lcnphy_freqoffset_corr;
	uint8	lcnphy_tempsense_option;
	int8	lcnphy_tempcorrx;
	bool	lcnphy_iqcal_swp_dis;
	bool	lcnphy_hw_iqcal_en;
	uint    lcnphy_bandedge_corr;
	bool    lcnphy_spurmod;
	uint16	lcnphy_tssi_tx_cnt; /* Tx frames at that level for NPT calculations */
	uint16	lcnphy_tssi_idx;	/* Estimated index for target power */
	uint16	lcnphy_tssi_npt;	/* NPT for TSSI averaging */

	int8	lcnphy_tx_power_idx_override; /* Forced tx power index */
	uint16	lcnphy_noise_samples;

	uint32	lcnphy_papdRxGnIdx;
	uint32	lcnphy_papd_rxGnCtrl_init;

	uint32	lcnphy_gain_idx_14_lowword;
	uint32	lcnphy_gain_idx_14_hiword;
	uint32	lcnphy_gain_idx_27_hiword;
	int16	lcnphy_ofdmgainidxtableoffset;  /* reference ofdm gain index table offset */
	int16	lcnphy_dsssgainidxtableoffset;  /* reference dsss gain index table offset */
	uint32	lcnphy_tr_R_gain_val;  /* reference value of gain_val_tbl at index 64 */
	uint32	lcnphy_tr_T_gain_val;	/* reference value of gain_val_tbl at index 65 */
	int8	lcnphy_input_pwr_offset_db;
	uint16	lcnphy_Med_Low_Gain_db;
	uint16	lcnphy_Very_Low_Gain_db;
	int8	lcnphy_lastsensed_temperature;
	int8	lcnphy_pkteng_rssi_slope;
	uint8	lcnphy_cck;
	bool    lcnphy_papd_4336_mode;
	int16	lcnphy_cck_dig_filt_type;
	int16	lcnphy_ofdm_dig_filt_type;
	int16	lcnphy_ofdm_dig_filt_type_2g;
	int16	lcnphy_ofdm_dig_filt_type_5g;
	bool	lcnphy_uses_rate_offset_table;

#if !defined(PHYCAL_CACHING)
	lcnphy_cal_results_t lcnphy_cal_results;
	uint8	lcnphy_full_cal_channel;
	uint16 	lcnphy_cal_temper;
#endif

	/* used for debug */
	uint8	lcnphy_start_idx;
	uint8	lcnphy_current_index;
	uint8	lcnphy_capped_index;
	bool	lcnphy_calreqd;
	bool    lcnphy_CalcPapdCapEnable;
	uint16	lcnphy_logen_buf_1;
	uint16	lcnphy_local_ovr_2;
	uint16	lcnphy_local_oval_6;
	uint16	lcnphy_local_oval_5;
	uint16	lcnphy_logen_mixer_1;

	uint8	lcnphy_aci_stat;
	uint	lcnphy_aci_start_time;
	int8	lcnphy_tx_power_offset[TXP_NUM_RATES];		/* Offset from base power */
	/* noise cal data */
	noise_t noise;

	bool    ePA;
	uint8   extpagain2g;
	uint8   extpagain5g;
	uint8	txpwrindex_nvram;
	int16	pa_gain_ovr_val_2g;
	int16	pa_gain_ovr_val_5g;

	int16	lcnphy_tx_iqlo_tone_freq_ovr_val;

	/* Coefficients for Temperature Conversion to Centigrade */
	/* Temp in deg = (temp_add - (T2-T1)*temp_mult)>>temp_q;  */
	int32 temp_mult;
	int32 temp_add;
	int32 temp_q;

	/* Coefficients for Vbat Conversion to Volts */
	/* Voltage = (vbat_add - (vbat_reading)*vbat_mult)>>vbat_q;  */
	int32 vbat_mult;
	int32 vbat_add;
	int32 vbat_q;

	int16 cckPwrOffset;
	int16 cckPwrIdxCorr;
	uint8 lcnphy_twopwr_txpwrctrl_en;

	uint8	dacrate;	/* DAC rate from srom */
	uint8	dacrate_2g;	/* DAC rate from srom */
	uint8	dacrate_5g;	/* DAC rate from srom */
	uint8	rfreg033;	/* rfreg033 values from srom */
	uint8	rfreg033_cck;	/* rfreg033 values from srom (CCK) */
	int8	pacalidx_2g;	/* PA Cal Idx 2g from srom */
	int8	pacalidx_5g;	/* PA Cal Idx 5g from srom */
	int16	pacalamamlim_2g;	/* PA cal amam threshold 2g */
	int16 	pacalamamlim_5g;	/* PA cal amam threshold 5g */	
	uint8	papd_rf_pwr_scale; /* parfps from srom */

	/*
	Switch control table params
	0 -> wl_pa1 wl_pa0 wl_tx1 wl_tx0
	1 -> wl_eLNArx1 wl_eLNArx0 wl_rx1 wl_rx0
	2 -> wl_eLNAAttnRx1 wl_eLNAAttnRx0 wl_AttnRx1 wl_AttnRx0
	3 -> bt_tx bt_eLNArx bt_rx
	4 -> ant(1 bit) ovr_en(1 bit) tdm(1 bit) wl_mask(8 bits)
	*/
	uint32 swctrlmap_2g[LCNPHY_SWCTRL_NVRAM_PARAMS];
	uint32 swctrlmap_5g[LCNPHY_SWCTRL_NVRAM_PARAMS];

	uint32 lcnphy_tssical_time;
	uint32 lcnphy_last_tssical;
	uint16 lcnphy_tssical_txdelay;

	/* 2g params from NVRAM */
	int16 dacgc2g;
	int16 gmgc2g;

#ifdef BAND5G
	/* 5g params from NVRAM */
	int16 dacgc5g;
	int16 gmgc5g;
	int16 rssismf5g;
	int16 rssismc5g;
	int16 rssisav5g;
#endif

	int32 tssi_maxpwr_limit;
	int32 tssi_minpwr_limit;
	uint8 tssi_ladder_offset_maxpwr_2g;
	uint8 tssi_ladder_offset_minpwr_2g;
	uint8 tssi_ladder_offset_maxpwr_5glo;
	uint8 tssi_ladder_offset_minpwr_5glo;
	uint8 tssi_ladder_offset_maxpwr_5gmid;
	uint8 tssi_ladder_offset_minpwr_5gmid;
	uint8 tssi_ladder_offset_maxpwr_5ghi;
	uint8 tssi_ladder_offset_minpwr_5ghi;
	uint8 init_txpwrindex_2g;
	uint8 init_txpwrindex_5g;
	uint8 idx0cnt;
	uint8 idx127cnt;
	uint8 dynamic_pwr_limit_en;

	uint8 xtal_mode[4];
	uint8 triso2g;
	uint8 tridx2g;
#ifdef BAND5G
	uint8 triso5g[3];
	uint8 tridx5g;
#endif
	uint8 tssi_max_npt;
	uint16 papd_corr_norm;

	/* Subset of registers/tables to identify corruption
	 * cause by pll phase drift
	 */
	uint16 rfseqtbl[3];
	uint16 resamplertbl[3];
	int8 Rssi;

	uint8 openlp_pwrctrl;
	int32 openlp_refpwrqdB;
	uint8 openlp_gainidx_b[CH_MAX_2G_CHANNEL];
	uint8 openlp_gainidx_a36;
	uint8 openlp_gainidx_a40;
	uint8 openlp_gainidx_a44;
	uint8 openlp_gainidx_a48;
	uint8 openlp_gainidx_a52;
	uint8 openlp_gainidx_a56;
	uint8 openlp_gainidx_a60;
	uint8 openlp_gainidx_a64;
	uint8 openlp_gainidx_a100;
	uint8 openlp_gainidx_a104;
	uint8 openlp_gainidx_a108;
	uint8 openlp_gainidx_a112;
	uint8 openlp_gainidx_a116;
	uint8 openlp_gainidx_a120;
	uint8 openlp_gainidx_a124;
	uint8 openlp_gainidx_a128;
	uint8 openlp_gainidx_a132;
	uint8 openlp_gainidx_a136;
	uint8 openlp_gainidx_a140;
	uint8 openlp_gainidx_a149;
	uint8 openlp_gainidx_a153;
	uint8 openlp_gainidx_a157;
	uint8 openlp_gainidx_a161;
	uint8 openlp_gainidx_a165;
	int32 openlp_pwrlimqdB;
	uint16 openlp_tempcorr;
	uint16 openlp_voltcorr;

	bool rfpll_doubler_2g;
	bool rfpll_doubler_5g;

	bool spuravoid_2g;
	bool spuravoid_5g;
	int8 iqlocalidx2goffs; /* IQLO Cal Idx backoff 2g */
	int8 iqlocalidx5goffs; /* IQLO Cal Idx backoff 5g */
	int8 iqlocalidx_2g; /* IQLO Cal Idx 2g from srom */
	int8 iqlocalidx_5g; /* IQLO Cal Idx 5g from srom */
	int16 iqlocalpwr_2g; /* IQLO Cal Pwr 2g from srom */
	int16 iqlocalpwr_5g; /* IQLO Cal Pwr 5g from srom */
	int8 pmin;    /* pwrMin_range2 */
	int8 pmax;    /* pwrMax_range2 */
	bool temppwrctrl_capable;
	bool rfpll_doubler_mode2g;
	bool rfpll_doubler_mode5g;
	int16 pacalpwr_2g; /* PA Cal Pwr 2g from srom */
	int16 pacalpwr_5g; /* PA Cal Pwr 5g from srom */

	bool txgaintbl;
	uint8 txgaintbl5g;

	int8 loccmode1;

#define LCNPHY_MAX_SAMP_CAP_DATA 128

#ifdef WLTEST
	uint16 samp_cap_r_idx;
	uint16 samp_cap_w_idx;
	uint32 samp_cap_data[LCNPHY_MAX_SAMP_CAP_DATA];
#endif
	int16 lcnphy_ofdm_dig_filt_type_curr;
	int16 lcnphy_cck_dig_filt_type_curr;

	int8 txiqlopapu_2g;
	int16 txiqlopag_2g;
#ifdef BAND5G
	int8 txiqlopapu_5g;
	int16 txiqlopag_5g;
#endif

	bool use_per_modulation_loft_cal;
	bool lpf_cck_tx_byp;
	bool lpf_ofdm_tx_byp;
	int16 di_ofdm;
	int16 dq_ofdm;
	int16 di_cck;
	int16 dq_cck;

#if defined(WLTEST)
	int16 srom_rxgainerr_2g;
	int16 srom_rxgainerr_5gl;
	int16 srom_rxgainerr_5gm;
	int16 srom_rxgainerr_5gh;

	int8 srom_noiselvl_2g;
	int8 srom_noiselvl_5gl;
	int8 srom_noiselvl_5gm;
	int8 srom_noiselvl_5gh;

	int16 rxpath_gain;
	int16 noise_offset_2g;
	int16 noise_offset_5gl;
	int16 noise_offset_5gm;
	int16 noise_offset_5gh;
#endif  /* #if defined(WLTEST) */

	/*
	0 -> Validity
	1 -> RF_REG05E
	2 -> RF_REG02A
	3 -> RF_REG02B
	4 -> RF_REG02C
	5 -> RF_REG02D
	*/
	uint8 logen_mode[6];
	int8 loidacmode_5g;
	int8 pacalidx_5glo;	/* PA Cal Idx 5g lo from srom */
	int8 pacalidx_5ghi;	/* PA Cal Idx 5g hi from srom */

};

extern int wlc_lcnphy_tssi_cal(phy_info_t *pi);

extern uint8 wlc_lcnphy_get_bbmult(phy_info_t *pi);
extern void wlc_lcnphy_clear_tx_power_offsets(phy_info_t *pi);
uint8 wlc_lcnphy_get_bbmult_from_index(phy_info_t *pi, int indx);
extern void wlc_lcnphy_read_papdepstbl(phy_info_t *pi, struct bcmstrbuf *b);
extern void wlc_lcnphy_tx_pwr_limit_check(phy_info_t *pi);
extern void wlc_lcnphy_check_pllcorruption(phy_info_t *pi);
extern void wlc_lcnphy_4313war(phy_info_t *pi);
extern void wlc_phy_lcn_updatemac_rssi(phy_info_t *pi, int8 rssi);
#if defined(WLTEST)
extern void wlc_phy_get_rxgainerr_lcnphy(phy_info_t *pi, int16 *gainerr);
extern void wlc_phy_get_SROMnoiselvl_lcnphy(phy_info_t *pi, int8 *noiselvl);
extern void wlc_lcnphy_rx_power(phy_info_t *pi, uint16 num_samps,
	uint8 wait_time, uint8 wait_for_crs, phy_iq_est_t* est);
extern void wlc_phy_get_noiseoffset_lcnphy(phy_info_t *pi, int16 *noiseoff);
extern void wlc_lcnphy_get_lna_freq_correction(phy_info_t *pi, int8 *freq_offset_fact);
#endif

#endif /* _wlc_phy_lcn_h_ */
