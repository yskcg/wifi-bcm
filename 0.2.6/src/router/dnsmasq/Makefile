OBJS = cache.o rfc1035.o dhcp.o util.o option.o forward.o network.o dnsmasq.o
BINDIR = /usr/local/sbin
MANDIR = /usr/local/man

ifeq ($(CONFIG_RTR_OPTIMIZE_SIZE),y)
CFLAGS  += -Os
else
CFLAGS  += -O2
endif

all : dnsmasq

clean :
	rm -f *~ $(OBJS) dnsmasq

.c.o: dnsmasq.h
	$(CC) -O2 -Wall -W -fno-strict-aliasing -c $*.c 


dnsmasq : $(OBJS) dnsmasq.h config.h
	$(CC) -o $@  $(OBJS) $(LIBS)

install : dnsmasq
	install -d $(BINDIR) -d $(MANDIR)
	install -m 644 dnsmasq.8 $(MANDIR)/man8 
	install -m 755 dnsmasq $(BINDIR)
