/*
 * USB hotplug service
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: usb_hotplug.c 241383 2011-02-18 03:30:06Z stakita $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <typedefs.h>
#include <shutils.h>
#include <bcmconfig.h>
#include <bcmparams.h>
#include <wlutils.h>
#include <bcmgpio.h>

#if defined(__CONFIG_DLNA_DMS__)
#include <bcmnvram.h>
#endif	

#define WL_DOWNLOADER_4323_VEND_ID "a5c/bd13/1"
#define WL_DOWNLOADER_43236_VEND_ID "a5c/bd17/1"

static int usb_start_services(void);
static int usb_stop_services(void);

#ifdef LINUX26
char *mntdir = "/media";
#else
char *mntdir = "/mnt";
static int usb_mount_ufd(void);
#endif

#ifdef HOTPLUG_DBG
int hotplug_pid = -1;
FILE *fp = NULL;
#define hotplug_dbg(fmt, args...) (\
{ \
	char err_str[100] = {0}; \
	char err_str2[100] = {0}; \
	if (hotplug_pid == -1) hotplug_pid = getpid(); \
	if (!fp) fp = fopen("/tmp/usb_err", "a+"); \
	sprintf(err_str, fmt, ##args); \
	sprintf(err_str2, "PID:%d %s", hotplug_pid, err_str); \
	fwrite(err_str2, strlen(err_str2), 1,  fp); \
	fflush(fp); \
} \
)
#else
#define hotplug_dbg(fmt, args...)
#endif /* HOTPLUG_DBG */

#define LOCK_FILE      "/tmp/hotplug_lock"

#define MAX_USB_PORTS			2

int hotplug_usb_power(int port, int boolOn)
{
	char name[] = "usbport%d"; /* 1 ~ 99 ports */
	unsigned long gpiomap;
	int usb_gpio;

	if (port > MAX_USB_PORTS)
		return -1;

	sprintf(name, "usbport%d", port);
	usb_gpio = bcmgpio_getpin(name, BCMGPIO_UNDEFINED);
	if (usb_gpio ==	BCMGPIO_UNDEFINED)
		return 0;
	if (bcmgpio_connect(usb_gpio, BCMGPIO_DIRN_OUT))
		return 0;

	gpiomap = (1 << usb_gpio);
	bcmgpio_out(gpiomap, boolOn? gpiomap: 0);
	bcmgpio_disconnect(usb_gpio);
	return 1;
}

/* Return number of usbports enabled */
int hotplug_usb_init(void)
{
	/* Enable VBUS via GPIOs for port1 and port2 */
	int i, count;

	for (count = 0, i = 1; i <= MAX_USB_PORTS; i++) {
		if (hotplug_usb_power(i, TRUE) > 0)
			count++;
	}
	return count;
}

/* hotplug block, called by LINUX26 */
int
hotplug_block(void)
{
	char *action = NULL, *minor = NULL;
	char *major = NULL, *driver = NULL;
	int minor_no, major_no, device, part;
	int err;
	int retry = 3, lock_fd = -1;
	char cmdbuf[64] = {0};
	char mntdev[32] = {0};
	char mntpath[32] = {0};
	char devname[10] = {0};
	struct flock lk_info = {0};

	if (!(action = getenv("ACTION")) ||
	    !(minor = getenv("MINOR")) ||
	    !(driver = getenv("PHYSDEVDRIVER")) ||
	    !(major = getenv("MAJOR")))
	{
		return EINVAL;
	}

	hotplug_dbg("env %s %s!\n", action, driver);
	if (strncmp(driver, "sd", 2))
	{
		return EINVAL;
	}

	if ((lock_fd = open(LOCK_FILE, O_RDWR|O_CREAT, 0666)) < 0) {
		hotplug_dbg("Failed opening lock file LOCK_FILE: %s\n", strerror(errno));
		return -1;
	}

	while (--retry) {
		lk_info.l_type = F_WRLCK;
		lk_info.l_whence = SEEK_SET;
		lk_info.l_start = 0;
		lk_info.l_len = 0;
		if (!fcntl(lock_fd, F_SETLKW, &lk_info)) break;
	}

	if (!retry) {
		hotplug_dbg("Failed locking LOCK_FILE: %s\n", strerror(errno));
		return -1;
	}

	major_no = atoi(major);
	minor_no = atoi(minor);
	device = minor_no/16;
	part = minor_no%16;

	sprintf(devname, "%s%c%d", driver, 'a' + device, part);
	sprintf(mntdev, "/dev/%s", devname);
	sprintf(mntpath, "/media/%s", devname);
	if (!strcmp(action, "add")) {
		if ((devname[2] > 'd') || (devname[2] < 'a')) {
			hotplug_dbg("bad dev!\n");
			goto exit;
		}

		hotplug_dbg("adding disk...\n");

		err = mknod(mntdev, S_IRWXU|S_IFBLK, makedev(major_no, minor_no));
		hotplug_dbg("err = %d\n", err);

		err = mkdir(mntpath, 0777);
		hotplug_dbg("err %s= %s\n", mntpath, strerror(errno));
		sprintf(cmdbuf, "mount -t vfat %s %s", mntdev, mntpath);
		err = system(cmdbuf);
		hotplug_dbg("err = %d\n", err);

		if (err) {
			hotplug_dbg("unsuccess %d!\n", err);
			unlink(mntdev);
			rmdir(mntpath);
		}
		else {
			/* Start usb services */
			usb_start_services();
		}
	} else if (!strcmp(action, "remove")) {
		/* Stop usb services */
		usb_stop_services();

		hotplug_dbg("removing disk %s...\n", devname);
		sprintf(cmdbuf, "umount %s", mntpath);
		err = system(cmdbuf);
		memset(cmdbuf, 0, sizeof(cmdbuf));
		unlink(mntdev);
		rmdir(mntpath);
	} else {
		hotplug_dbg("not support action!\n");
	}

exit:
	close(lock_fd);
	unlink(LOCK_FILE);
	return 0;
}

/* hotplug usb, called by LINUX24 or USBAP */
int
hotplug_usb(void)
{
	char *device, *interface;
	char *action;
	int class, subclass, protocol;
	char *product;
	int need_interface = 1;

	if (!(action = getenv("ACTION")))
		return EINVAL;

	product = getenv("PRODUCT");

	cprintf("hotplug detected product:  %s\n", product);

	if ((device = getenv("TYPE"))) {
		sscanf(device, "%d/%d/%d", &class, &subclass, &protocol);
		if (class != 0)
			need_interface = 0;
	}

	if (need_interface) {
		if (!(interface = getenv("INTERFACE")))
			return EINVAL;
		sscanf(interface, "%d/%d/%d", &class, &subclass, &protocol);
		if (class == 0)
			return EINVAL;
	}

#ifndef LINUX26
	/* If a new USB device is added and it is of storage class */
	if (class == 8 && subclass == 6 && !strcmp(action, "add")) {
		/* Mount usb disk */
		if (usb_mount_ufd() != 0)
			return ENOENT;
		/* Start services */
		usb_start_services();
		return 0;
	}

	/* If a new USB device is removed and it is of storage class */
	if (class == 8 && subclass == 6 && !strcmp(action, "remove")) {
		/* Stop services */
		usb_stop_services();

		eval("/bin/umount", mntdir);
		return 0;
	}
#endif

#ifdef __CONFIG_USBAP__
	/* download the firmware and insmod wl_high for USBAP */
	if (!strcmp(product, WL_DOWNLOADER_43236_VEND_ID)) {
		if (!strcmp(action, "add")) {
			eval("rc", "restart");
		} else if (!strcmp(action, "remove")) {
			cprintf("wl device removed\n");
		}
	}
#endif /* __CONFIG_USBAP__ */

	return 0;
}

/*
 * Process the file in /proc/mounts to get
 * the mount path and device.
 */
static char mntpath[128] = {0};
static char devpath[128] = {0};

static void
get_mntpath()
{
	FILE *fp;
	char buf[256];

	memset(mntpath, 0, sizeof(mntpath));
	memset(devpath, 0, sizeof(devpath));

	if ((fp = fopen("/proc/mounts", "r")) != NULL) {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			if (strstr(buf, mntdir) != NULL) {
				sscanf(buf, "%s %s", devpath, mntpath);
				break;
			}
		}
		fclose(fp);
	}
}

static void
dump_disk_type(char *path)
{
	char *argv[3];

	argv[0] = "/usr/sbin/disktype";
	argv[1] = path;
	argv[2] = NULL;
	_eval(argv, ">/tmp/disktype.dump", 0, NULL);

	return;
}

#ifndef LINUX26
/*
 * Check if the UFD is still connected because the links
 * created in /dev/discs are not removed when the UFD is
 * unplugged.
 */
static bool
usb_ufd_connected(char *str)
{
	uint host_no;
	char proc_file[128];

	/* Host no. assigned by scsi driver for this UFD */
	host_no = atoi(str);

	sprintf(proc_file, "/proc/scsi/usb-storage-%d/%d", host_no, host_no);

	if (eval("/bin/grep", "-q", "Attached: Yes", proc_file) == 0)
		return TRUE;
	else
		return FALSE;
}

static int
usb_mount_ufd(void)
{
	DIR *dir;
	struct dirent *entry;
	char path[128];

	/* Is this linux24? */
	if ((dir = opendir("/dev/discs")) == NULL)
		return EINVAL;

	/* Scan through entries in the directories */
	while ((entry = readdir(dir)) != NULL) {
		if ((strncmp(entry->d_name, "disc", 4)))
			continue;

		/* Files created when the UFD is inserted are not
		 * removed when it is removed. Verify the device
		 * is still inserted.
		 * Strip the "disc" and pass the rest of the string.
		 */
		if (usb_ufd_connected(entry->d_name+4) == FALSE)
			continue;

		sprintf(path, "/dev/discs/%s/disc", entry->d_name);

		dump_disk_type(path);

		/* Check if it has FAT file system */
		if (eval("/bin/grep", "-q", "FAT", "/tmp/disktype.dump") == 0) {
			/* If it is partioned, mount first partition else raw disk */
			if (eval("/bin/grep", "-q", "Partition", "/tmp/disktype.dump") == 0)
			{
				char part[10], *partitions, *next;
				struct stat tmp_stat;

				partitions = "part1 part2 part3 part4";
				foreach(part, partitions, next) {
					sprintf(path, "/dev/discs/%s/%s", entry->d_name, part);
					if (stat(path, &tmp_stat) == 0)
						break;
				}

				/* Not found, no need to do further prcoessing */
				if (part[0] == 0)
					return EINVAL;
			}

			/* Mount here */
			eval("/bin/mount", "-t", "vfat", path, "/mnt");
			return 0;
		}
	}

	return EINVAL;
}
#endif	/* !LINUX26 */

/*
 * Mount the path and look for the WCN configuration file.
 * If it exists launch wcnparse to process the configuration.
 */
static int
get_wcn_config()
{
	int ret = ENOENT;
	struct stat tmp_stat;

	if (stat("/mnt/SMRTNTKY/WSETTING.WFC", &tmp_stat) == 0) {
		eval("/usr/sbin/wcnparse", "-C", "/mnt", "SMRTNTKY/WSETTING.WFC");
		ret = 0;
	}
	return ret;
}


#if defined(__CONFIG_DLNA_DMS__)
static void
start_dlna_dms()
{
	char *dlna_dms_enable = nvram_safe_get("dlna_dms_enable");

	if (strcmp(dlna_dms_enable, "1") == 0) {
		/* Check mount device */
		if (strlen(mntpath) == 0 || strlen(devpath) == 0)
			return;

		cprintf("Start bcmmserver.\n");
		eval("sh", "-c", "bcmmserver&");
	}
}

static void
stop_dlna_dms()
{
	cprintf("Stop bcmmserver.\n");
	eval("killall", "bcmmserver");
}
#endif	/* __CONFIG_DLNA_DMS__ */

/* Handle hotplugging of UFD */
static int
usb_start_services(void)
{
	/* Read mount path and dump to file */
	get_mntpath();

	dump_disk_type(devpath);

	/* Check WCN config */
	if (get_wcn_config() == 0)
		return 0;


#if defined(__CONFIG_DLNA_DMS__)
	start_dlna_dms();
#endif

	return 0;
}

static int
usb_stop_services(void)
{

#if defined(__CONFIG_DLNA_DMS__)
	stop_dlna_dms();
#endif

	return 0;
}
