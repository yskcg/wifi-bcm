#
# Broadcom web pages
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
# $Id: Makefile 295348 2011-11-10 04:21:34Z kenlo $
#

all: pages.mk

ifneq ($(WIND_HOST_TYPE),x86-win32)
ifdef BCM_NETBSD
# NetBSD
CONFIG_NAT=y
else
# Linux
$(SRCBASE)/router/.config:
	cp $(SRCBASE)/router/config/defconfig $@

include $(SRCBASE)/router/.config
endif
else
# VxWorks compiled on Windows host
CONFIG_WPS=y
endif

PAGES := style.css
PAGES += index.asp
PAGES += lan.asp
ifeq ($(CONFIG_NAT),y)
PAGES += wan.asp
endif
PAGES += ssid.asp
PAGES += radio.asp
PAGES += security.asp
PAGES += firmware.asp

CONFIG_LW_CUST=y
ifeq ($(CONFIG_LW_CUST),y)
PAGES += pwps.asp wds.asp gxlogo.gif lw.js jquery-3.1.0.min.js  jquery.min.js circle-progress.min.js
endif

PAGES += basicset.asp netstatus.asp passwordmanage.asp seniorset.asp stylewireless.css supermanager.asp
PAGES += wireless.asp wirelessseniorsettings.asp wirelessterminallist.asp wirelessbrigle.asp
NEW_PAGES += html/*
NEW_CSS += css/*
NEW_IMAGES += images/*
NEW_SCRIPT += script/common/*
NEW_SCRIPT_JS += script/jquery-3.1.0.min.js
NEW_SCRIPT_JS += script/jquery.min.js

ifneq ($(WIND_HOST_TYPE),x86-win32)
ifdef BCM_NETBSD
pages.mk:
	echo PAGES := $(PAGES) > $@
else
# Linux
pages.mk: $(SRCBASE)/router/.config
	echo "PAGES := $(PAGES)" > $@
endif
else
# VxWorks
pages.mk:
	echo PAGES := $(PAGES) > $@
endif

install:
	install -d $(INSTALLDIR)/www
	install -m 644 $(PAGES) $(INSTALLDIR)/www
	install -d $(INSTALLDIR)/www/css
	install -m 644 $(NEW_CSS) $(INSTALLDIR)/www/css
	install -d $(INSTALLDIR)/www/html
	install -m 644 $(NEW_PAGES) $(INSTALLDIR)/www/html
	install -d $(INSTALLDIR)/www/images
	install -m 644 $(NEW_IMAGES) $(INSTALLDIR)/www/images
	install -d $(INSTALLDIR)/www/script
	install -m 644 $(NEW_SCRIPT_JS) $(INSTALLDIR)/www/script
	install -d $(INSTALLDIR)/www/script/common
	install -m 644 $(NEW_SCRIPT) $(INSTALLDIR)/www/script/common
clean:
	rm -f pages.mk
