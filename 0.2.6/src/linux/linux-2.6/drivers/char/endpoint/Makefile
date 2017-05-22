#
# Makefile for the Broadcom endpoint driver
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
# $Id: Makefile,v 1.6 2010-07-13 01:50:34 Exp $
#


SHARED   := ../../../../../shared

ENDPTDRV_SRC=${SRCBASE}/voip/bcmdrivers/endpoint

USE_PREBUILD=$(shell if [ -f $(SRCBASE)/voip/xChange/prebuilt/endpointdd_linux.o ]; then echo 1; else echo 0; fi)
HAS_ENDPTSRC=$(shell if [ -d $(ENDPTDRV_SRC) ]; then echo 1; else echo 0; fi)

obj-$(CONFIG_BCM_ENDPOINT) += endpointdd.o

ifeq ($(USE_PREBUILD),1)
SRCBASE := ../../../../..
PB_PATH=$(SRCBASE)/voip/xChange/prebuilt
endpointdd-objs :=$(PB_PATH)/endpointdd_linux.o
else
-include $(ENDPTDRV_SRC)/endpt.mk

endif
