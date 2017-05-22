/*
 * SES GPIO functions
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_gpio.c 241187 2011-02-17 21:52:03Z gmo $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <assert.h>
#include <bcmnvram.h>
#include <bcmgpio.h>

#include <ses_dbg.h>
#include <ses_gpio.h>



#define SES_GPIO_BUTTON_VALUE	"ses_button"
#define SES_GPIO_LED_VALUE	"ses_led"

#define SES_NV_BTN_ASSERTLVL	"ses_button_assertlvl"
#define SES_NV_LED_ASSERTLVL	"ses_led_assertlvl"

extern bcm_timer_module_id ses_tmodule;

static int ses_btn_gpio = BCMGPIO_UNDEFINED;
static int ses_led_gpio = BCMGPIO_UNDEFINED;
static int ses_btn_assertlvl = SES_BTN_ASSERTLVL;
static int ses_led_assertlvl = SES_LED_ASSERTLVL;

int ses_gpio_btn_init()
{
	char *value;

	/* Determine the GPIO pins for the SES Button */

	ses_btn_gpio = bcmgpio_getpin(SES_GPIO_BUTTON_VALUE, BCMGPIO_UNDEFINED);

	if (ses_btn_gpio != BCMGPIO_UNDEFINED) {
		SES_GPIO("Using pin %d for ses button input\n", ses_btn_gpio);

		if (bcmgpio_connect(ses_btn_gpio, BCMGPIO_DIRN_IN) != 0) {
			SES_ERROR("Error connecting GPIO %d to ses button\n",
				ses_btn_gpio);
			return -1;
		}
		value = nvram_safe_get(SES_NV_BTN_ASSERTLVL);
		if (strcmp(value, "")) {
			ses_btn_assertlvl = atoi(value) ? 1 : 0;
			SES_GPIO("Using assertlvl %d for ses button\n",
				ses_btn_assertlvl);
		}
	}

	return 0;
}

int ses_gpio_led_init()
{
	char *value;

	/* Determine the GPIO pins for the SES led */

	ses_led_gpio = bcmgpio_getpin(SES_GPIO_LED_VALUE, BCMGPIO_UNDEFINED);

	if (ses_led_gpio != BCMGPIO_UNDEFINED) {
		SES_GPIO("Using pin %d for ses led output\n", ses_led_gpio);

		if (bcmgpio_connect(ses_led_gpio, BCMGPIO_DIRN_OUT) != 0) {
			SES_GPIO("Error connecting GPIO %d to ses led\n",
				ses_led_gpio);
			return -1;
		}
		value = nvram_safe_get(SES_NV_LED_ASSERTLVL);
		if (strcmp(value, "")) {
			ses_led_assertlvl = atoi(value) ? 1 : 0;
			SES_GPIO("Using assertlvl %d for ses led\n",
				ses_led_assertlvl);
		}
	}

	return 0;
}

void ses_gpio_btn_cleanup()
{
	if (ses_btn_gpio != BCMGPIO_UNDEFINED) {
		bcmgpio_disconnect(ses_btn_gpio);
		ses_btn_gpio = BCMGPIO_UNDEFINED;
	}
}

void ses_gpio_led_cleanup()
{
	if (ses_led_gpio != BCMGPIO_UNDEFINED) {
		bcmgpio_disconnect(ses_led_gpio);
		ses_led_gpio = BCMGPIO_UNDEFINED;
	}
}

ses_btnpress_t ses_btn_pressed()
{
	unsigned long btn_mask;
	unsigned long value;
	struct timeval time;
	bool long_btnpress = FALSE;
	static bool first_time = TRUE;
	struct timespec start_ts, end_ts;

	if (ses_btn_gpio == BCMGPIO_UNDEFINED)
		return SES_NO_BTNPRESS;

	btn_mask = ((unsigned long)1 << ses_btn_gpio);

	bcmgpio_in(btn_mask, &value);
	value >>= ses_btn_gpio;

	if (value == ses_btn_assertlvl) {
		clock_gettime(CLOCK_REALTIME, &start_ts);

		do {
			time.tv_sec = 1;
			time.tv_usec = 0;
			select(0, NULL, NULL, NULL, &time);

			bcmgpio_in(btn_mask, &value);
			value >>= ses_btn_gpio;

			clock_gettime(CLOCK_REALTIME, &end_ts);
			if ((end_ts.tv_sec - start_ts.tv_sec) >= SES_LONG_PRESSTIME) {
				long_btnpress = TRUE;
				break;
			}
		} while (value == ses_btn_assertlvl);

		if (first_time)
			return SES_NO_BTNPRESS;

		if (long_btnpress == FALSE)
			return SES_SHORT_BTNPRESS;
		else
			return SES_LONG_BTNPRESS;
	}
	else {
		first_time = FALSE;
		return SES_NO_BTNPRESS;
	}
}

void ses_led_on()
{
	unsigned long led_mask;
	unsigned long value;

	ses_gpio_led_init();

	if (ses_led_gpio == BCMGPIO_UNDEFINED)
		return;

	bcmgpio_strobe_stop(ses_led_gpio);

	led_mask = ((unsigned long)1 << ses_led_gpio);
	value = ((unsigned long) ses_led_assertlvl << ses_led_gpio);

	bcmgpio_out(led_mask, value);

	ses_gpio_led_cleanup();
}

void ses_led_off()
{
	unsigned long led_mask;
	unsigned long value;

	ses_gpio_led_init();

	if (ses_led_gpio == BCMGPIO_UNDEFINED)
		return;

	bcmgpio_strobe_stop(ses_led_gpio);

	led_mask = ((unsigned long)1 << ses_led_gpio);
	value = ((unsigned long) ~ses_led_assertlvl << ses_led_gpio);
	value &= led_mask;

	bcmgpio_out(led_mask, value);

	ses_gpio_led_cleanup();
}

void ses_led_blink(ses_blinktype_t blinktype)
{
	bcmgpio_strobe_t strobe_info;

	ses_gpio_led_init();

	if (ses_led_gpio == BCMGPIO_UNDEFINED)
		return;

	strobe_info.duty_percent = 50;
	strobe_info.timer_module = ses_tmodule;
	strobe_info.num_strobes = 0;
	strobe_info.strobe_done = NULL;

	switch ((int)blinktype) {
	case SES_BLINKTYPE_RWO:
		strobe_info.strobe_period_in_ms = SES_LED_MEDIUMBLINK;
		break;

	case SES_BLINKTYPE_REGOK:
		strobe_info.strobe_period_in_ms = SES_LED_MEDIUMBLINK;
		strobe_info.num_strobes = 3;
		break;

	default:
		break;
	}

	bcmgpio_strobe_start(ses_led_gpio, &strobe_info);

	return;
}


#if SES_ENABLE_HWTEST

void ses_hw_test(int duty_percent, int strobe_period, int max_strobes)
{
	bcmgpio_strobe_t strobe_info;
	unsigned long btn_state;
	volatile int strobe_done;

	printf("Starting LED test with pulse_period = %d\n", strobe_period);
	printf("Press SES button to STOP the test...\n");

	if (bcmgpio_connect(SES_TEST_LED_GPIO, BCMGPIO_DIRN_OUT) != 0) {
		printf("bcmgpio_attach() failed for SES LED\n");
		return;
	}

	if (bcmgpio_connect(SES_TEST_BTN_GPIO, BCMGPIO_DIRN_IN) != 0) {
		printf("bcmgpio_attach() failed for SES button\n");
		bcmgpio_disconnect(SES_TEST_LED_GPIO);
		return;
	}

	strobe_info.timer_module = ses_tmodule;
	strobe_info.strobe_period_in_ms = (unsigned long) strobe_period;
	strobe_info.duty_percent = (unsigned long) duty_percent;
	strobe_info.num_strobes = 0;
	strobe_info.strobe_done = NULL;

	/* Blink the SES LED */
	bcmgpio_strobe_start(SES_TEST_LED_GPIO, &strobe_info);

	/* Wait for the SES button press */
	do {
		bcmgpio_in(SES_TEST_BTN_GPIO_MASK, &btn_state);
	} while ((btn_state & SES_TEST_BTN_GPIO_MASK) != 0);

	/* Wait for the SES button release */
	do {
		bcmgpio_in(SES_TEST_BTN_GPIO_MASK, &btn_state);
	} while ((btn_state & SES_TEST_BTN_GPIO_MASK) == 0);

	/* Stop blinking */
	bcmgpio_strobe_stop(SES_TEST_LED_GPIO);

	/* Switch the SES LED OFF by setting the GPIO high */
	bcmgpio_out(SES_TEST_LED_GPIO_MASK, 1);


	printf("Starting LED test for %d periods\n", max_strobes);

	strobe_info.timer_module = ses_tmodule;
	strobe_info.strobe_period_in_ms = (unsigned long) strobe_period;
	strobe_info.duty_percent = (unsigned long) duty_percent;
	strobe_info.num_strobes = max_strobes;
	strobe_info.strobe_done = (int *) &strobe_done;

	/* Blink the SES LED */
	bcmgpio_strobe_start(SES_TEST_LED_GPIO, &strobe_info);

	/* Wait for the blinking to stop */
	while (strobe_done != 1);

	/* Switch the SES LED OFF by setting the GPIO high */
	bcmgpio_out(SES_TEST_LED_GPIO_MASK, 1);

	printf("SES HW test complete.\n");

	bcmgpio_disconnect(SES_TEST_LED_GPIO);
	bcmgpio_disconnect(SES_TEST_BTN_GPIO);
}

#endif /* SES_ENABLE_HWTEST */
