/*
 * dnsr_vx.h - DNS relay vxWorks porting specific module. It includes 
 * vxWorks missing interfaces that are found in unix/linux environment.
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: dnsr_vx.h,v 1.3 2003-10-16 21:13:39 Exp $
 */

#ifndef __dnsr_vx_h__
#define __dnsr_vx_h__

/* logging utilities */
#include <logLib.h>

#define LOG_DEBUG	0
#define LOG_INFO	1
#define LOG_WARNING	2
#define LOG_ERR	3
#define LOG_CRIT	4

#define openlog(ident, option, facility)
int syslog(int level, char *format, ...);
#define closelog()

/* inet utilities */
#define INADDRSZ	sizeof(struct in_addr)
#define INET_ADDRSTRLEN	16
#define IF_NAMESIZE IFNAMSIZ
#define socklen_t int

const char* inet_ntop(int af, const void *src, char *dst, size_t size);
int inet_pton(int af, const char *src, void *dst);

/* time utilities */
#include "sys/times.h"

/* defined in src/vxWorks/target/config/bcm47xx/router/unix.c */
int gettimeofday(struct timeval *tv, struct timezone *tz);
int settimeofday(const struct timeval *tv , const struct timezone *tz);

/* task utilities */
#include <taskLib.h>

#define getpid taskIdSelf
#define die dnsr_die

#endif	/* #ifndef __log_vx_h__ */
