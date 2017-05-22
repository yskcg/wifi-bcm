#
# Router rc control scripts Makefile
#
# Copyright (C) 2009, Broadcom Corporation
# All Rights Reserved.
# 
# THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
# KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
# SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
#
# $Id: Makefile 241383 2011-02-18 03:30:06Z stakita $
#

include $(TOP)/.config

ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
CFLAGS += -DLINUX26
CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include -Wall -I$(LINUXDIR)/include -I$(TOP)/busybox-1.x
else
CFLAGS += -I. -I$(TOP)/shared -I$(TOP)/busybox -I$(SRCBASE)/include -Wall
endif

#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s
ifeq ($(CONFIG_RTR_OPTIMIZE_SIZE),y)
CFLAGS  += -Os
else
CFLAGS  += -O2
endif
LDFLAGS	+= -L$(TOP)/nvram -L$(PLATFORMDIR)/install/nvram/usr/lib -lnvram -L$(TOP)/shared -L$(PLATFORMDIR)/install/shared/usr/lib -lshared \
	-L$(TOP)/libbcmcrypto -L$(INSTALLDIR)/libbcmcrypto/usr/lib -lbcmcrypto
LDFLAGS += -L$(TOP)/libbcm -L$(INSTALLDIR)/libbcm/usr/lib -lbcm

ifeq ($(BUILD_WFA_APUT),1)
CFLAGS	+= -DCAPI_AP
endif

ifeq ($(CONFIG_WAPI_IAS),y)
CFLAGS += -DRC_BUILDTIME=$(shell date -d "now" +"%s")
endif

OBJS := rc.o init.o interface.o network.o services.o udhcpc.o http.o stats.o bcmutils.o mtd.o usb_hotplug.o pmon.o 
ifeq ($(CONFIG_BCMQOS),y)
OBJS += qos.o
endif

ifeq ($(CONFIG_NAT),y)
OBJS += firewall.o
LDFLAGS += -L$(TOP)/netconf -L$(PLATFORMDIR)/install/netconf/usr/lib -lnetconf
endif

ifeq ($(CONFIG_NAT),y)
OBJS += ppp.o
endif

ifeq ($(RWL),1)
CFLAGS += -DRWL_SOCKET
endif

vpath %.c $(TOP)/shared $(SRCBASE)/shared

all: rc

clean:
	rm -f *.o rc .*.depend

install: all
	install -d $(INSTALLDIR)/sbin
	install rc $(INSTALLDIR)/sbin
	$(STRIP) $(INSTALLDIR)/sbin/rc
	cd $(INSTALLDIR)/sbin && ln -sf rc init
	cd $(INSTALLDIR)/sbin && ln -sf rc erase
	cd $(INSTALLDIR)/sbin && ln -sf rc write
	cd $(INSTALLDIR)/sbin && ln -sf rc stats
	cd $(INSTALLDIR)/sbin && ln -sf rc hotplug
	cd $(INSTALLDIR)/sbin && ln -sf rc preinit

rc: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: .%.depend
.%.depend: %.c
	$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
