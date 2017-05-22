#
# C library Makefile
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
# $Id: Makefile,v 1.30 2009-08-27 03:14:48 Exp $
#

include $(TOP)/.config

# Extra libraries
lib-$(CONFIG_LIBCRYPT) += libcrypt
lib-$(CONFIG_LIBDL) += libdl
lib-$(CONFIG_LIBM) += libm
lib-$(CONFIG_LIBNSL) += libnsl
lib-$(CONFIG_LIBPTHREAD) += libpthread
lib-$(CONFIG_LIBRESOLV) += libresolv
lib-$(CONFIG_LIBUTIL) += libutil

ifeq ($(CONFIG_UCLIBC),y)

# uClibc names
LINKER := ld-uClibc.so.0
LIBC := libc.so.0
MAP := uClibc.map

else

# GNU libc names
LINKER := $(foreach lib,ld.so.* ld-linux.so.*,$(notdir $(wildcard $(LIBDIR)/$(lib))))
LIBC := libc.so.6
MAP := libc.map

endif

# Nothing to do until installation
all:

ifeq ($(CONFIG_LIBOPT),y)

# Find all ELF binaries (including extra libraries)
BINARIES := $(shell find $(TARGETDIR) -path $(TARGETDIR)/lib -prune -o -type f -print | file -f - | grep ELF | cut -d':' -f1)
BINARIES += $(wildcard $(foreach lib,$(LINKER) $(lib-y),$(LIBDIR)/$(lib) $(LIBDIR)/$(lib).so.?))

ifneq ($(wildcard $(LIBDIR)/optinfo),)
# Try the toolchain directory first for precursors
OPTINFO := $(LIBDIR)/optinfo
else
ifneq ($(wildcard $(LIBDIR)),)
OPTINFO := $(LIBDIR)
else
# Fall back on platform directory
OPTINFO := $(PLATFORM)
endif
endif
ifeq ($(CONFIG_UCLIBC),y)

# uClibc precursors
OBJS := $(wildcard $(foreach file,libgcc-need.a libc.a interp.o,$(OPTINFO)/$(file)))

ifeq ($(LINUX_VERSION),2_6)
CUSTOM_UCLIBC := $(TOOLCHAIN)/lib/libc.a
else
CUSTOM_UCLIBC :=  mipsel-uclibc/libc.a
endif

ifeq ($(CONFIG_IPV6),y)
OBJS := $(wildcard $(foreach file,libgcc-need.a interp.o,$(OPTINFO)/$(file))) $(CUSTOM_UCLIBC)
endif # CONFIG_IPV6 = y

# uClibc link parameters
ENTRY := __uClibc_init
LDFLAGS := -s -shared --warn-common --warn-once -z combreloc  -soname=$(LIBC) -init $(ENTRY)
LIBGCC := $(shell $(CC) -print-libgcc-file-name)

ifeq ($(CONFIG_IPV6),y)
$(MAP): $(CUSTOM_UCLIBC)
else
$(MAP): $(OPTINFO)/libc.a
endif
	$(NM) -o --defined-only --no-sort $< | cut -d' ' -f3 > $@

# Find all unresolved symbols that uClibc could resolve
.symbols: $(MAP) $(BINARIES)
	rm -f $@
	for symbol in `$(NM) --dynamic -u --no-sort $(BINARIES) | sort -u` ; do \
		if grep -q "^$${symbol}$$" $(MAP) ; then \
			echo "-u $${symbol}" >> $@ ; \
		fi ; \
	done

# Re-link uClibc
$(LIBC): .symbols $(OBJS)
	xargs -t $(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBGCC) < .symbols

else

# GNU libc precursors
OBJS := $(wildcard $(foreach file,soinit.os libc_pic.a sofini.os,$(OPTINFO)/$(file)))

# GNU libc link parameters
ENTRY := __libc_main
CFLAGS := -shared -Wl,-O1 -Wl,-dynamic-linker=$(LINKER) -Wl,--version-script=$(MAP) -Wl,-soname=$(LIBC) -nostdlib -nostartfiles -e $(ENTRY)

$(MAP): $(OPTINFO)/libc.map
	cp $< $@

# Find all unresolved symbols that GNU libc could resolve
.symbols: $(MAP) $(BINARIES)
	rm -f $@
	for symbol in `$(NM) --dynamic -u --no-sort $(BINARIES) | sort -u` ; do \
	    grep -q "$${symbol};" $(MAP) && echo "-u $${symbol}" >> $@ ; \
	done

# Re-link GNU libc
$(LIBC): .symbols $(OBJS) $(LIBDIR)/$(LINKER)
	xargs $(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBDIR)/$(LINKER) -lgcc < .symbols

endif

# Install optimized C library
install: $(LIBC)

else

# Install default C library
install: $(LIBDIR)/$(LIBC)

endif

# Also install extra libraries
install: $(wildcard $(foreach lib,$(LINKER) $(lib-y),$(LIBDIR)/$(lib) $(LIBDIR)/$(lib).so.?))
	install -d $(TARGETDIR)/lib
	install $^ $(TARGETDIR)/lib/
	$(STRIP) $(TARGETDIR)/lib/*.so.*

clean:
	rm -f $(LIBC) $(MAP) .symbols
