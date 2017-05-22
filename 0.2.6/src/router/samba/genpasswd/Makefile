#
# Passwd Makefile
#
# $ Copyright Open Broadcom Corporation 2010 $
#
# $Id: Makefile,v 1.4 2010-07-19 07:10:16 winniec Exp $
#

all:genpasswd

OBJS := crypt.o des.o md5.o passwd.o

genpasswd: $(OBJS)
	gcc md5.c des.c crypt.c passwd.c -o $@
	./genpasswd

install:
	install -d $(INSTALLDIR)/etc
	install passwd $(INSTALLDIR)/etc
	install group $(INSTALLDIR)/etc

clean: 
	rm -rf $(OBJS) genpasswd
