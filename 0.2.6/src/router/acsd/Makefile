#
# Linux dcs Makefile
#
# Copyright (C) 2010, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile 241396 2011-02-18 03:39:46Z stakita $
#

CFLAGS	+= -I. -I$(TOP)/shared -I$(SRCBASE)/include -Wall
#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s -O2
LDFLAGS += -L$(TOP)/nvram -L$(INSTALLDIR)/nvram/usr/lib -lnvram
LDFLAGS += -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib -lshared


vpath %.c $(SRCBASE)/shared/

all: acsd acs_cli

OBJS = dcs.o acs.o acsd_utils.o acsd_main.o acsd_cmd.o acsd_chanim.o

install: all
	install -d $(INSTALLDIR)/usr/sbin/
	install acsd $(INSTALLDIR)/usr/sbin/
	install acs_cli $(INSTALLDIR)/usr/sbin/
	$(STRIP) $(INSTALLDIR)/usr/sbin/acsd
	$(STRIP) $(INSTALLDIR)/usr/sbin/acs_cli

clean:
	rm -f *.o *.a *.so acsd acs_cli

${OBJS}: acsd.h acsd_svr.h

acsd: bcmutils.o ${OBJS}
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS)

acs_cli: acsd_cli.o acsd_utils.o
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS)
