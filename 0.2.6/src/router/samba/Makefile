#
# Samba Makefile
#
# $ Copyright Open Broadcom Corporation 2010 $
#
# $Id: Makefile,v 1.6 2010-07-17 02:57:34 kenlo Exp $
#

SAMBA_SRC=samba-3.0.25b/source
PASSWD_SRC=genpasswd

all: samba samba_pass
CFLAGS += -DLINUX

configure:
	[ -f $(SAMBA_SRC)/Makefile ] || \
	(cd $(SAMBA_SRC); \
	export SMB_BUILD_CC_NEGATIVE_ENUM_VALUES=yes ; \
	export libreplace_cv_READDIR_GETDIRENTRIES=no ; \
	export libreplace_cv_READDIR_GETDENTS=no ; \
	export linux_getgrouplist_ok=no ; \
	export samba_cv_REPLACE_READDIR=no ; \
	export samba_cv_HAVE_WRFILE_KEYTAB=yes ; \
	export samba_cv_HAVE_KERNEL_OPLOCKS_LINUX=yes ; \
	export samba_cv_HAVE_IFACE_IFCONF=yes ; \
	export samba_cv_USE_SETRESUID=yes ; \
	CC=$(CC) ./configure \
		--target=mipsel-uclibc-linux26 \
		--host=mipsel-uclibc-linux26 \
		--build=`/bin/arch`-linux \
		--enable-shared \
		--disable-static \
		--disable-cups \
		--disable-iprint \
		--disable-pie \
		--disable-fam \
		--localstatedir=/tmp/samba/lib/ \
		--with-configdir=/tmp/samba/lib/ \
		--with-privatedir=/tmp/samba/private \
		--with-lockdir=/tmp/samba/var/locks \
		--with-piddir=/tmp/samba/var/locks \
		--without-ldap \
		--without-sys-quotas \
		--without-cifsmount \
		--prefix=/tmp/samba; \
	)

samba:	configure
	$(MAKE) -C $(SAMBA_SRC)

samba_pass:
	[ -f $(PASSWD_SRC)/passwd ] || ($(MAKE) -C $(PASSWD_SRC))

install: all
	$(MAKE) -C $(PASSWD_SRC) install
	install -d $(INSTALLDIR)/usr/sbin	
	install $(SAMBA_SRC)/bin/smbd $(INSTALLDIR)/usr/sbin
	$(STRIP) $(INSTALLDIR)/usr/sbin/smbd
	install $(SAMBA_SRC)/bin/smbpasswd $(INSTALLDIR)/usr/sbin
	$(STRIP) $(INSTALLDIR)/usr/sbin/smbpasswd
	install -d $(INSTALLDIR)/usr/lib
	install -m 755 $(SAMBA_SRC)/bin/libbigballofmud.so $(INSTALLDIR)/usr/lib/libbigballofmud.so.0

clean:
	$(MAKE) -C $(SAMBA_SRC) clean
	$(MAKE) -C $(PASSWD_SRC) clean

.PHONY: all clean install samba sambapass
