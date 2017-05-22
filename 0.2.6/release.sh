#!/bin/bash
#
# Copyright (C) 2010, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#  
# Creates an open license tarball
#
# $Id: release_linux.sh 242572 2011-02-24 01:40:00Z prakashd $
#

FILELIST="gpl-filelist.txt"
FILELISTTEMP="gpl-filelist-temp.txt"

RELEASE=no


# do not redistribute this package under any circumstances
if [ "$RELEASE" = "no" ] ; then
cat <<EOF
This package contains UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom
Corporation; the contents of this package may not be disclosed to
third parties, copied or duplicated in any form, in whole or in part,
without the prior written permission of Broadcom Corporation.
EOF
exit 0
fi
