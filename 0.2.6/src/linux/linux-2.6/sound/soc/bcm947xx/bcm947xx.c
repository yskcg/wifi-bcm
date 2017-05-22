/*
 * SoC audio for BCM947XX Board
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcm947xx.c,v 1.1 2010-05-13 23:46:27 Exp $
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/i2c-gpio.h>

#include <typedefs.h>
#include <bcmdevs.h>
#include <pcicfg.h>
#include <hndsoc.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <i2s_core.h>

#include <bcmnvram.h>


#include "../codecs/wm8955.h"
#include "bcm947xx-pcm.h"
#include "bcm947xx-i2s.h"

#define BCM947XX_AP_DEBUG 0
#if BCM947XX_AP_DEBUG
#define DBG(x...) printk(KERN_ERR x)
#else
#define DBG(x...)
#endif


/* MCLK in Hz - to bcm947xx & Wolfson 8955 */
#define BCM947XX_MCLK_FREQ 20000000 /* 20 MHz */
#define BCM947XX_NVRAM_XTAL_FREQ "xtalfreq"

#define SDA_GPIO_NVRAM_NAME "i2c_sda_gpio"
#define SCL_GPIO_NVRAM_NAME "i2c_scl_gpio"




static int bcm947xx_startup(struct snd_pcm_substream *substream)
{
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	//struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret = 0;

	DBG("%s:\n", __FUNCTION__);

	return ret;
}

static void bcm947xx_shutdown(struct snd_pcm_substream *substream)
{
	DBG("%s\n", __FUNCTION__);
	return;
}

static int bcm947xx_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int fmt;
	int freq = 11289600;
	int mclk_input = BCM947XX_MCLK_FREQ;
	char *tmp;
	int ret = 0;

	fmt = SND_SOC_DAIFMT_I2S |		/* I2S mode audio */
	      SND_SOC_DAIFMT_NB_NF |		/* BCLK not inverted and normal LRCLK polarity */
	      SND_SOC_DAIFMT_CBM_CFM;		/* BCM947xx is I2S Slave */

	/* set codec DAI configuration */
	DBG("%s: calling set_fmt with fmt 0x%x\n", __FUNCTION__, fmt);
	ret = codec_dai->dai_ops.set_fmt(codec_dai, fmt);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, fmt);
	if (ret < 0)
		return ret;

	/* We need to derive the correct pll output frequency */
	/* These two PLL frequencies should cover the sample rates we'll be asked to use */
	if (freq % params_rate(params))
		freq = 12288000;
	if (freq % params_rate(params))
		DBG("%s: Error, PLL not configured for this sample rate %d\n", __FUNCTION__,
		    params_rate(params));

	tmp = nvram_get(BCM947XX_NVRAM_XTAL_FREQ);

	/* Try to get xtal frequency from NVRAM, otherwise we'll just use our default */
	if (tmp && (strlen(tmp) > 0)) {
		mclk_input = simple_strtol(tmp, NULL, 10);
		mclk_input *= 1000; /* NVRAM param is in kHz, we want MHz */
	}

	/* set up the PLL in codec */
	ret = codec_dai->dai_ops.set_pll(codec_dai, 0, mclk_input, freq);
	if (ret < 0) {
		DBG("%s: Error CODEC DAI set_pll returned %d\n", __FUNCTION__, ret);
		return ret;
	}
	/* set the codec system clock for DAC and ADC */
	ret = codec_dai->dai_ops.set_sysclk(codec_dai, WM8955_SYSCLK, freq,
		SND_SOC_CLOCK_IN);
	DBG("%s: codec set_sysclk returned %d\n", __FUNCTION__, ret);
	if (ret < 0)
		return ret;

	/* set the I2S system clock as input (unused) */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, BCM947XX_I2S_SYSCLK, freq,
		SND_SOC_CLOCK_IN);

	DBG("%s: cpu set_sysclk returned %d\n", __FUNCTION__, ret);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops bcm947xx_ops = {
	.startup = bcm947xx_startup,
	.hw_params = bcm947xx_hw_params,
	.shutdown = bcm947xx_shutdown,
};

/*
 * Logic for a wm8955
 */
static int bcm947xx_wm8955_init(struct snd_soc_codec *codec)
{
	DBG("%s\n", __FUNCTION__);

	snd_soc_dapm_sync_endpoints(codec);

	return 0;
}

/* bcm947xx digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link bcm947xx_dai = {
	.name = "WM8955",
	.stream_name = "WM8955",
	.cpu_dai = &bcm947xx_i2s_dai,
	.codec_dai = &wm8955_dai,
	.init = bcm947xx_wm8955_init,
	.ops = &bcm947xx_ops,
};

/* bcm947xx audio machine driver */
static struct snd_soc_machine snd_soc_machine_bcm947xx = {
	.name = "bcm947xx",
	.dai_link = &bcm947xx_dai,
	.num_links = 1,
};

/* bcm947xx audio private data */
static struct wm8955_setup_data bcm947xx_wm8955_setup = {
	.i2c_address = 0x1a, /* 2wire / I2C interface */
};

/* bcm947xx audio subsystem */
static struct snd_soc_device bcm947xx_snd_devdata = {
	.machine = &snd_soc_machine_bcm947xx,
	.platform = &bcm947xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm8955,
	.codec_data = &bcm947xx_wm8955_setup,
};

static struct platform_device *bcm947xx_snd_device;

static int machine_is_bcm947xx(void)
{
	DBG("%s\n", __FUNCTION__);
	return 1;
}

static struct i2c_gpio_platform_data i2c_gpio_data = {
	/* Will be replaced with board specific values from NVRAM */
	.sda_pin        = 4,
	.scl_pin        = 5,
};

static struct platform_device i2c_gpio_device = {
	.name		= "i2c-gpio",
	.id		= 0,
	.dev		= {
		.platform_data	= &i2c_gpio_data,
	},
};

static int __init bcm947xx_init(void)
{
	char *tmp;
	int ret;

	DBG("%s\n", __FUNCTION__);

	if (!machine_is_bcm947xx())
		return -ENODEV;

	tmp = nvram_get(SDA_GPIO_NVRAM_NAME);

	if (tmp && (strlen(tmp) > 0))
		i2c_gpio_data.sda_pin = simple_strtol(tmp, NULL, 10);
	else {
		printk(KERN_ERR "%s: NVRAM variable %s missing for I2C interface\n",
		       __FUNCTION__, SDA_GPIO_NVRAM_NAME);
		return -ENODEV;
	}

	tmp = nvram_get(SCL_GPIO_NVRAM_NAME);

	if (tmp && (strlen(tmp) > 0))
		i2c_gpio_data.scl_pin = simple_strtol(tmp, NULL, 10);
	else {
		printk(KERN_ERR "%s: NVRAM variable %s missing for I2C interface\n",
		       __FUNCTION__, SCL_GPIO_NVRAM_NAME);
		return -ENODEV;
	}

	ret = platform_device_register(&i2c_gpio_device);
	if (ret) {
		platform_device_put(&i2c_gpio_device);
		return ret;
	}

	bcm947xx_snd_device = platform_device_alloc("soc-audio", -1);
	if (!bcm947xx_snd_device)
		return -ENOMEM;

	platform_set_drvdata(bcm947xx_snd_device, &bcm947xx_snd_devdata);
	bcm947xx_snd_devdata.dev = &bcm947xx_snd_device->dev;
	ret = platform_device_add(bcm947xx_snd_device);

	if (ret) {
		platform_device_put(bcm947xx_snd_device);
	}

	return ret;
}

static void __exit bcm947xx_exit(void)
{
	DBG("%s\n", __FUNCTION__);
	platform_device_unregister(bcm947xx_snd_device);
}

module_init(bcm947xx_init);
module_exit(bcm947xx_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC BCM947XX");
MODULE_LICENSE("GPL");
