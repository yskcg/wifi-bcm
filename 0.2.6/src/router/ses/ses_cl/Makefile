#
# SES client (Secure Easy Setup) Embedded Linux Build Makefile
#
# Copyright (C) 2010, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile 241187 2011-02-17 21:52:03Z gmo $
#

CFLAGS	+= -I.. -I$(SRCBASE)/include -I$(SRCBASE)/router/shared -I$(SRCBASE)/router/libbcm -I$(SRCBASE)/ses/shared -I$(SRCBASE)/ses/client/shared -Wall
#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s -O2
CFLAGS	+= -DSES_PE_CLIENT -DSES_WDS_CLIENT -fno-strict-aliasing

# EAPD include path
CFLAGS	+= -I$(SRCBASE)/router/eapd

LDFLAGS	+= -L$(TOP)/nvram -L$(INSTALLDIR)/nvram/usr/lib -lnvram 
LDFLAGS	+= -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib -lshared 
LDFLAGS	+= -L$(TOP)/libbcm -L$(INSTALLDIR)/libbcm/usr/lib -lbcm 
LDFLAGS	+= -L$(TOP)/libbcmcrypto -L$(INSTALLDIR)/libbcmcrypto/usr/lib -lbcmcrypto 

vpath %.c $(SRCBASE)/router/ses $(SRCBASE)/ses/shared $(SRCBASE)/ses/client/shared
vpath %.o $(SRCBASE)/router/ses/prebuilt_cl

# build target
SESVENDOBJS = ses_clmain.o ses_clvendor.o ses_gpio.o ses_wl.o
SESLIBOBJS = ses_linux_eapd.o ses_packet.o fsm_assoc.o fsm_ds.o fsm_main.o fsm_sur.o ses_cl_fsm.o

OBJS = $(SESLIBOBJS) $(SESVENDOBJS)

all: ses_cl

ses_cl: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# install to target
install: all
	install -D ses_cl $(INSTALLDIR)/usr/sbin/ses_cl
	$(STRIP) $(INSTALLDIR)/usr/sbin/ses_cl

# clean up mess
clean:
	rm -f *.o .*.depend *~ ses_cl

%.o: .%.depend
.%.depend: %.c
	$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
