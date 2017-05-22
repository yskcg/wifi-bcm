#
# SES Linux Builds Makefile
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
# targets/builds
BUILDS = ses

${BUILDS} ::
	$(MAKE) -C $@

# clean up
BUILDS-CLEAN = ${BUILDS:=-clean}

clean : ${BUILDS-CLEAN}

${BUILDS-CLEAN} ::
	$(MAKE) -C ${@:-clean=} clean

# default build
all : ${BUILDS}

# install
BUILDS-INSTALL = ${BUILDS:=-install}

install : ${BUILDS-INSTALL}

${BUILDS-INSTALL} ::
	$(MAKE) -C ${@:-install=} install
