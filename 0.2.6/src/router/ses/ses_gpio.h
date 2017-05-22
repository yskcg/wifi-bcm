/*
 * SES GPIO Header file
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_gpio.h 241187 2011-02-17 21:52:03Z gmo $
 */

#ifndef _ses_gpio_h_
#define _ses_gpio_h_

/* #define SES_ENABLE_HWTEST	1 */

#define NVRAM_BUFSIZE           128

#define SES_LED_FASTBLINK	100	/* fast blink period */
#define SES_LED_MEDIUMBLINK	1000	/* medium blink period */
#define SES_LED_SLOWBLINK	5000	/* slow blink period */

typedef enum ses_btnpress {
	SES_NO_BTNPRESS = 0,
	SES_SHORT_BTNPRESS,
	SES_LONG_BTNPRESS
} ses_btnpress_t;

typedef enum ses_blinktype {
	SES_BLINKTYPE_RWO = 0,
	SES_BLINKTYPE_REGOK,
	} ses_blinktype_t;

#define SES_LONG_PRESSTIME	5	/* seconds */
#define SES_BTNSAMPLE_PERIOD	(500 * 1000)	/* 500 ms */
#define SES_BTN_ASSERTLVL	0
#define SES_LED_ASSERTLVL	1

int ses_gpio_btn_init(void);
int ses_gpio_led_init(void);
void ses_gpio_btn_cleanup(void);
void ses_gpio_led_cleanup(void);
ses_btnpress_t ses_btn_pressed(void);
void ses_led_on(void);
void ses_led_off(void);
void ses_led_blink(ses_blinktype_t blinktype);

#if SES_ENABLE_HWTEST

/* Required for command line interface */
#define SES_GPIO_MAX_REGSTR 	8
#define SES_GPIO_MAX_VALSTR 	11

#define SES_TEST_BTN_GPIO	6
#define SES_TEST_LED_GPIO	1

#define SES_TEST_BTN_GPIO_MASK	((unsigned long) 1 << SES_TEST_BTN_GPIO)
#define SES_TEST_LED_GPIO_MASK	((unsigned long) 1 << SES_TEST_LED_GPIO)

void ses_hw_test(int duty_percent, int strobe_period, int max_strobes);

#endif /* SES_ENABLE_HWTEST */

#endif /* _ses_gpio_h_ */
