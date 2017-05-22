#
# Toplevel Makefile for the BCM947xx Linux Router release
#
# Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
# 
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# $Id: toplevel-release.mk,v 1.43 2010-09-15 04:30:57 Exp $
#

SRCBASE := $(shell pwd)
RELEASEDIR := $(shell (cd $(SRCBASE)/.. && pwd -P))
CFG_BUILD_CFE = y

install: all
	install -d $(RELEASEDIR)/image
	$(MAKE) -C router install
	-cp router/mipsel/linux.trx $(RELEASEDIR)/image/linux-glibc.trx
	-cp router/mipsel-uclibc/linux.trx $(RELEASEDIR)/image/linux.trx
	-cp router/mipsel-uclibc/linux-lzma.trx $(RELEASEDIR)/image/linux-lzma.trx
	-cp router/mipsel-uclibc/linux.bin $(RELEASEDIR)/image/linux.bin
	-cp dongle/rte/wl/builds/4322-bmac-usbap/roml-ag-nodis/rtecdc.exe $(RELEASEDIR)/image/rtecdc-4323.exe
	-cp dongle/rte/wl/builds/43236-bmac-usbap/roml-ag-nodis/rtecdc.exe $(RELEASEDIR)/image/rtecdc-43236.exe
ifneq ($(wildcard cfe),)
ifeq ($(CFG_BUILD_CFE),y)
	cp cfe/build/broadcom/bcm947xx/cfe.bin $(RELEASEDIR)/image/
	cp cfe/build/broadcom/bcm947xx/compressed/cfez.bin $(RELEASEDIR)/image/
#	# Clean and make one more time linked at 3MB
#	$(MAKE) -C cfe clean
#	$(MAKE) -C cfe CFG_TEXT_START=0x80300000
#	cp cfe/build/broadcom/bcm947xx/cfe.bin $(RELEASEDIR)/image/cfe4.bin
#	cp cfe/build/broadcom/bcm947xx/compressed/cfez.bin $(RELEASEDIR)/image/cfez4.bin
#	# Clean and make one more time with GigE
#	$(MAKE) -C cfe clean
#	$(MAKE) -C cfe CFG_PCI=1
#	cp cfe/build/broadcom/bcm947xx/cfe.bin $(RELEASEDIR)/image/cfe-gige.bin
#	cp cfe/build/broadcom/bcm947xx/compressed/cfez.bin $(RELEASEDIR)/image/cfez-gige.bin
	# Clean and make one more time with GMAC
	$(MAKE) -C cfe clean
	$(MAKE) -C cfe CFG_GMAC=1
	cp cfe/build/broadcom/bcm947xx/cfe.bin $(RELEASEDIR)/image/cfe-gmac.bin
	cp cfe/build/broadcom/bcm947xx/compressed/cfez.bin $(RELEASEDIR)/image/cfez-gmac.bin
	cp cfe/build/chipimages/bcm5357/cfe_script.cfesh $(RELEASEDIR)/image/
#	# Clean and make one more time with NFLASH
#	$(MAKE) -C cfe clean
#	$(MAKE) -C cfe CFG_GMAC=1 CFG_NFLASH=1
#	cp cfe/build/broadcom/bcm947xx/cfe.bin $(RELEASEDIR)/image/cfe-nflash.bin
#	cp cfe/build/broadcom/bcm947xx/compressed/cfez.bin $(RELEASEDIR)/image/cfez-nflash.bin
endif
endif
	cp shared/nvram/*.txt $(RELEASEDIR)/image/
	rm -f ~/share/linux.trx
	cp -f $(RELEASEDIR)/image/linux.trx ~/share/

all clean:
	$(MAKE) -C router $@
ifneq ($(wildcard cfe),)
ifeq ($(CFG_BUILD_CFE),y)
	$(MAKE) -C cfe clean
	$(MAKE) -C cfe $@
endif
endif

.PHONY: all clean install
