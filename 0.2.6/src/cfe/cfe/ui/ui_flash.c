/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Flash Update commands			File: ui_flash.c
    *  
    *  The routines in this file are used for updating the 
    *  flash with new firmware.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_ioctl.h"
#include "cfe_timer.h"
#include "cfe_error.h"

#include "ui_command.h"
#include "cfe.h"

#include "cfe_fileops.h"
#include "cfe_boot.h"
#include "bsp_config.h"

#include "cfe_loader.h"

#include "net_ebuf.h"
#include "net_ether.h"
#include "net_api.h"

#include "cfe_flashimage.h"

#include "addrspace.h"
#include "initdata.h"
#include "url.h"


/*  *********************************************************************
    *  Constants
    ********************************************************************* */

/*
 * Of course, these things really belong somewhere else.
 */

#define FLASH_STAGING_BUFFER	CFG_FLASH_STAGING_BUFFER_ADDR
#ifdef _FLASHPROG_
#define FLASH_STAGING_BUFFER_SIZE     (1024*1024*16)
#else
#define FLASH_STAGING_BUFFER_SIZE CFG_FLASH_STAGING_BUFFER_SIZE
#endif


/*  *********************************************************************
    *  Exerns
    ********************************************************************* */

extern int cfe_iocb_dispatch(cfe_iocb_t *iocb);

int ui_init_flashcmds(void);
static int ui_cmd_flash(ui_cmdline_t *cmd,int argc,char *argv[]);
unsigned int flash_crc32(const unsigned char *databuf, unsigned int  datalen);
void ui_get_flash_buf(uint8_t **bufptr, int *bufsize);


/*  *********************************************************************
    *  ui_init_flashcmds()
    *  
    *  Initialize the flash commands, add them to the table.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0 if ok, else error
    ********************************************************************* */

int ui_init_flashcmds(void)
{
    cmd_addcmd("flash",
	       ui_cmd_flash,
	       NULL,
	       "Update a flash memory device",
	       "flash [options] filename [flashdevice]\n\n"
	       "Copies data from a source file name or device to a flash memory device.\n"
               "The source device can be a disk file (FAT filesystem), a remote file\n"
               "(TFTP) or a flash device.  The destination device may be a flash or eeprom.\n"
#if !CFG_EMBEDDED_PIC
	       "If the destination device is your boot flash (usually flash0), the flash\n"
	       "command will restart the firmware after the flash update is complete\n"
#endif
               "",
               "-noerase;Don't erase flash before writing|"
               "-offset=*;Begin programming at this offset in the flash device|"
               "-size=*;Size of source device when programming from flash to flash|"
	       "-noheader;Override header verification, flash binary without checking|"
#if CFE_FLASH_ERASE_FLASH_ENABLED
 	       "-forceflash;Dangerous Command, Don't use if you don't know what you do|"
    	       "-erase;Erase the partition, can set the  offset and length|"
#endif
	       "-mem;Use memory as source instead of a device");


    return 0;
}

/*  *********************************************************************
    *  flash_crc32(buf,len)
    *  
    *  Yes, this is an Ethernet CRC.  I'm lazy.
    *  
    *  Input parameters: 
    *  	   buf - buffer to CRC
    *  	   len - length of data
    *  	   
    *  Return value:
    *  	   CRC-32
    ********************************************************************* */

#define     CRC32_POLY        0xEDB88320UL    /* CRC-32 Poly */
unsigned int
flash_crc32(const unsigned char *databuf, unsigned int  datalen) 
{       
    unsigned int idx, bit, data, crc = 0xFFFFFFFFUL;
 
    for (idx = 0; idx < datalen; idx++) {
	for (data = *databuf++, bit = 0; bit < 8; bit++, data >>= 1) {
	    crc = (crc >> 1) ^ (((crc ^ data) & 1) ? CRC32_POLY : 0);
	    }
	}

    return crc;
}

/*  *********************************************************************
    *  flash_validate(ptr)
    *  
    *  Validate the flash header to make sure we can program it.
    *  
    *  Input parameters: 
    *  	   ptr - pointer to flash header
    *      outptr - pointer to data that we should program
    *	   outsize - size of data we should program
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error occured
    ********************************************************************* */

#define GET32(x) (((uint32_t) (x[0] << 24)) | \
                  ((uint32_t) (x[1] << 16)) | \
                  ((uint32_t) (x[2] << 8)) |  \
                  ((uint32_t) (x[3] << 0)))

static int flash_validate(uint8_t *ptr,int bufsize,int insize,uint8_t **outptr,int *outsize)
{
    cfe_flashimage_t *hdr = (cfe_flashimage_t *) ptr;
    uint32_t size;
    uint32_t flags;
    uint32_t hdrcrc;
    uint32_t calccrc;

    if (memcmp(hdr->seal,CFE_IMAGE_SEAL,sizeof(hdr->seal)) != 0) {
	printf("Invalid header seal.  This is not a CFE flash image.\n");
	return -1;
	}

    printf("Flash image contains CFE version %d.%d.%d for board '%s'\n",
	   hdr->majver,hdr->minver,hdr->ecover,hdr->boardname);

    size = GET32(hdr->size);
    flags = GET32(hdr->flags);
    hdrcrc = GET32(hdr->crc);
    printf("Flash image is %d bytes, flags %08X, CRC %08X\n",size,flags,hdrcrc);

    if (strcmp(CFG_BOARDNAME,(int8_t *)hdr->boardname) != 0) {
	printf("This flash image is not appropriate for board type '%s'\n",CFG_BOARDNAME);
	return -1;
	}

    if ((size == 0) || (size > bufsize) ||
	((size + sizeof(cfe_flashimage_t)) < insize)) {
	printf("Flash image size is bogus!\n");
	return -1;
	}

    calccrc = flash_crc32(ptr + sizeof(cfe_flashimage_t),size);

    if (calccrc != hdrcrc) {
	printf("CRC is incorrect. Calculated CRC is %08X\n",calccrc);
	return -1;
	}

    *outptr = ptr + sizeof(cfe_flashimage_t);
    *outsize = size;
    return 0;
}


/*  *********************************************************************
    *  ui_get_flashbuf(bufptr, bufsize)
    *  
    *  Figure out the location and size of the staging buffer.
    *  
    *  Input parameters:
    *	   bufptr - address to return buffer location
    *	   bufsize - address to return buffer size
    ********************************************************************* */


void ui_get_flash_buf(uint8_t **bufptr, int *bufsize)
{
    int size = FLASH_STAGING_BUFFER_SIZE;

    /*	
     * Get the address of the staging buffer.  We can't
     * allocate the space from the heap to store the 
     * new flash image, because the heap may not be big
     * enough.  So, if FLASH_STAGING_BUFFER_SIZE is non-zero
     * then just use it and FLASH_STAGING_BUFFER; else
     * use the larger of (mem_bottomofmem - FLASH_STAGING_BUFFER)
     * and (mem_totalsize - mem_topofmem).
     */

    if (size > 0) {
	*bufptr = (uint8_t *) KERNADDR(FLASH_STAGING_BUFFER);
	*bufsize = size;
    } else {
	int below, above;

	below = PHYSADDR(mem_bottomofmem) - FLASH_STAGING_BUFFER;
	above = (mem_totalsize << 10) - PHYSADDR(mem_topofmem);

	if (below > above) {
	    *bufptr = (uint8_t *) KERNADDR(FLASH_STAGING_BUFFER);
	    *bufsize = below;
	} else {
	    *bufptr = (uint8_t *) KERNADDR(mem_topofmem);
	    *bufsize = above;
	}
    }
}
#ifdef CFG_NFLASH
static void ui_check_flashdev(char *in, char *out)
{
	int i;
	
	if (!in) {
		strcpy(out, "flash0.0");
		return;
	}
	for (i=0 ; i< strlen(in) ; i++) {
		if (strncmp(in+i, ".trx", 4) == 0) {
			/* Program the trx image */
			ui_get_trx_flashdev(out);
			return;
		}
		if (strncmp(in+i, ".boot", 5) == 0) {
			/* Program the trx image */
			ui_get_boot_flashdev(out);
			return;
		}
	}
	strcpy(out, in);
	return;
}
#endif /* CFG_NFLASH */

#ifdef CFE_FLASH_ERASE_FLASH_ENABLED
static int erase_range(char *flashdev, int start, int len)
{
    flash_range_t range;
    int fh;

    range.range_base = start;
    range.range_length = len;
    fh = cfe_open(flashdev);
    if (fh < 0) {
	xprintf("Could not open device '%s'\n",flashdev);
	return CFE_ERR_DEVNOTFOUND;
    }
    if (cfe_ioctl(fh,IOCTL_FLASH_ERASE_RANGE,
		  (uint8_t *) &range,sizeof(range),NULL,0) != 0) {
	xprintf("Failed to erase the flash\n");
	cfe_close(fh);
	return CFE_ERR_IOERR;
    }
    cfe_close(fh);
    return 0;
}
#endif

#ifdef FLASH_PARTITION_FILL_ENABLED
static int fill_partition(char *flashdev, char *ptr, int bufsize, char c)
{
    int fh=-1;
    flash_info_t flashinfo;
    int erased_size=0;

    printf("filling %s with %x \n", flashdev, c);
    fh = cfe_open(flashdev);
    if (fh < 0) {
	xprintf("Could not open device '%s'\n",flashdev);
	return CFE_ERR_DEVNOTFOUND;
    }

    if (cfe_ioctl(fh,IOCTL_FLASH_PARTITION_INFO,
		  (unsigned char *) &flashinfo,
		  sizeof(flash_info_t),
		  NULL,0) == 0) {
		printf("flash base=%llx, size=%x\n", flashinfo.flash_base, flashinfo.flash_size);
			
	}
    memset(ptr, c, bufsize);
    erased_size = cfe_writeblk(fh, 0, (unsigned char *)ptr, flashinfo.flash_size);
    if(erased_size == flashinfo.flash_size) {
	    xprintf("Fill Done!!\n");
    } else {
    	    xprintf("Fill Failed!!!\n");
    }
    cfe_close(fh);
    return 0;
}

#endif

/*  *********************************************************************
    *  ui_cmd_flash(cmd,argc,argv)
    *  
    *  The 'flash' command lives here.  Program the boot flash,
    *  or if a device name is specified, program the alternate
    *  flash device.
    *  
    *  Input parameters: 
    *  	   cmd - command table entry
    *  	   argc,argv - parameters
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */


static int ui_cmd_flash(ui_cmdline_t *cmd,int argc,char *argv[])
{
    uint8_t *ptr = NULL;
    int fh;
    int res;
#if !CFG_EMBEDDED_PIC
    int retlen;
#endif
    char *fname;
    char *flashdev;
    cfe_loadargs_t la;
    int amtcopy;
    int devtype;
    int srcdevtype;
    int chkheader;
    int sfd;
    int copysize;
    flash_info_t flashinfo;
    int offset = 0;
    int noerase = 0;
    int memsrc = 0; /* Source is memory address */
    char *x;
    int size = 0;
    int bufsize = 0;
#ifdef CFG_NFLASH
    char buf[16];
#endif

    /* Get staging buffer */
    memsrc = cmd_sw_isset(cmd,"-mem");

    /* If memory is not being used as a source, then get staging buffer */
    if (!memsrc)
	    ui_get_flash_buf(&ptr, &bufsize);

#ifdef CFE_FLASH_ERASE_FLASH_ENABLED
    int erase_op = cmd_sw_isset(cmd, "-erase");
    if (erase_op) {
	    flashdev = cmd_getarg(cmd, 0);
	    if (flashdev == NULL) return CFE_ERR_INV_PARAM;
	    if (!strncmp(flashdev, "flash0.boot", 30) || !strncmp(flashdev, "flash1.boot", 30)) {
		xprintf("!! Erase the boot sector is dangerous!!:");
		int forceflash = cmd_sw_isset(cmd,"-forceflash");
		if (!forceflash) {
			xprintf("Reject !!\n");
			return CFE_ERR_INV_PARAM;	
		}
	    }
	    char *ss = cmd_getarg(cmd, 1);
	    /* 0 start address and 0 len viewed as erase whole partiton */
	    if (ss == NULL) {
		    erase_range(flashdev, 0, 0);
		    return CFE_OK;
	    }
	    char *ll = cmd_getarg(cmd, 2);
	    if (ll == NULL) {
		xprintf("Please specify the len !!\n");
		return CFE_ERR_INV_PARAM;
	    }
	    int start = atoi(ss);
	    int len = atoi(ll);
	    erase_range(flashdev, start, len);
	    return CFE_OK;
    }
#endif

    /*
     * Parse command line parameters
     */

    fname = cmd_getarg(cmd,0);

    if (!fname) {
	return ui_showusage(cmd);
	}

    flashdev = cmd_getarg(cmd,1);
    if (!flashdev) flashdev = "flash0.0";
#ifdef CFG_NFLASH
    ui_check_flashdev(flashdev, buf);
    flashdev = buf;
#endif   	    
    /*
     * Make sure it's a flash device.
     */

    res = cfe_getdevinfo(flashdev);
    if (res < 0) {
	return ui_showerror(CFE_ERR_DEVNOTFOUND,flashdev);
	}

    devtype = res & CFE_DEV_MASK;

    if ((res != CFE_DEV_FLASH) && (res != CFE_DEV_NVRAM)) {
	xprintf("Device '%s' is not a flash or eeprom device.\n",flashdev);
	return CFE_ERR_INV_PARAM;
	}

    /*
     * We shouldn't really allow this, but there are some circumstances
     * where you might want to bypass the header check and shoot
     * yourself in the foot.
     * Switch normally not supplied, so chkheader will be TRUE.
     */

    chkheader = !cmd_sw_isset(cmd,"-noheader");

    /*
     * Check for some obscure options here.
     */

    noerase = cmd_sw_isset(cmd,"-noerase");

    if (cmd_sw_value(cmd,"-offset",&x)) {
        offset = atoi(x);
        }

    if (cmd_sw_value(cmd,"-size",&x)) {
        size = atoi(x);
        }

    /* Fix up the ptr and size for reading from memory
     * and skip loading to go directly to programming
     */
    if (memsrc) {
	    ptr = (uint8_t *)xtoi(fname);
	    bufsize = size;
	    res = size;
	    xprintf("Reading from %s: ",fname);
	    goto program;
    }

    /*
     * Read the new flash image from the source device
     */

    srcdevtype = cfe_getdevinfo(fname) & CFE_DEV_MASK;

    xprintf("Reading %s: ",fname);

    switch (srcdevtype) {
	case CFE_DEV_FLASH:
	    sfd = cfe_open(fname);
	    if (sfd < 0) {
		return ui_showerror(sfd,"Could not open source device");
		}
	    memset(ptr,0xFF,bufsize);


	    if (cfe_ioctl(sfd,IOCTL_FLASH_GETINFO,
			  (unsigned char *) &flashinfo,
			  sizeof(flash_info_t),
			  &res,0) != 0) {
		flashinfo.flash_size = bufsize;
		}

	    if (size > 0) {
		xprintf("(size=0x%X) ",size);
		}
            else {
		size = flashinfo.flash_size;
		}

	    /* Make sure we don't overrun the staging buffer */
	    
	    if (size > bufsize) {
		size = bufsize;
		}

	    /* Read the flash device here. */

	    res = cfe_read(sfd,ptr,size);

	    cfe_close(sfd);
	    if (res < 0) {
		return ui_showerror(res,"Could not read from flash");
		}
	    chkheader = FALSE;		/* no header to check */
	    /* 
	     * Search for non-0xFF byte at the end.  This will work because
	     * flashes get erased to all FF's, we pre-fill our buffer to FF's,
	     */
	    while (res > 0) {
		if (ptr[res-1] != 0xFF) break;
		res--;
		}
	    break;	

	case CFE_DEV_SERIAL:
	    la.la_filesys = "raw";
	    la.la_filename = NULL;
	    la.la_device = fname;
	    la.la_address = (intptr_t) ptr;
	    la.la_options = 0;
	    la.la_maxsize = bufsize;
	    la.la_flags =  LOADFLG_SPECADDR;

	    res = cfe_load_program("srec",&la);
	
	    if (res < 0) {
		ui_showerror(res,"Failed.");
		return res;
		}
	    break;

	default:
		
	    res = ui_process_url(fname, cmd, &la);
	    if (res < 0) {
		ui_showerror(res,"Invalid file name %s",fname);
		return res;
		}

	    la.la_address = (intptr_t) ptr;
	    la.la_options = 0;
	    la.la_maxsize = bufsize;
	    la.la_flags =  LOADFLG_SPECADDR;

	    res = cfe_load_program("raw",&la);
	
	    if (res < 0) {
		ui_showerror(res,"Failed.");
		return res;
		}
	    break;

	}

    xprintf("Done. %d bytes read\n",res);

program:
    copysize = res;

    /*
     * Verify the header and file's CRC.
     */
    if (chkheader) {
	if (flash_validate(ptr,bufsize,res,&ptr,&copysize) < 0) return -1;
	}

    if (copysize == 0) return 0;		/* 0 bytes, don't flash */

    /*
     * Open the destination flash device.
     */

    fh = cfe_open(flashdev);
    if (fh < 0) {
	xprintf("Could not open device '%s'\n",flashdev);
	return CFE_ERR_DEVNOTFOUND;
	}

    if (cfe_ioctl(fh,IOCTL_FLASH_GETINFO,
		  (unsigned char *) &flashinfo,
		  sizeof(flash_info_t),
		  &res,0) == 0) {
	/* Truncate write if source size is greater than flash size */
	if ((copysize + offset) > flashinfo.flash_size) {
            copysize = flashinfo.flash_size - offset;
	    }
	}

    /*
     * If overwriting the boot flash, we need to use the special IOCTL
     * that will force a reboot after writing the flash.
     */

    if (flashinfo.flash_flags & FLASH_FLAG_INUSE) {
#if CFG_EMBEDDED_PIC
	xprintf("\n\n** DO NOT TURN OFF YOUR MACHINE UNTIL THE FLASH UPDATE COMPLETES!! **\n\n");
#else
#if CFG_NETWORK
	if (net_getparam(NET_DEVNAME)) {
	    xprintf("Closing network.\n");
	    net_uninit();
	    }
#endif
	xprintf("Rewriting boot flash device '%s'\n",flashdev);
	xprintf("\n\n**DO NOT TURN OFF YOUR MACHINE UNTIL IT REBOOTS!**\n\n");
	cfe_ioctl(fh,IOCTL_FLASH_WRITE_ALL, ptr,copysize,&retlen,0);
	/* should not return */
	return CFE_ERR;
#endif
	}

    /*
     * Otherwise: it's not the flash we're using right
     * now, so we can be more verbose about things, and
     * more importantly, we can return to the command
     * prompt without rebooting!
     */

    /*
     * Erase the flash, if the device requires it.  Our new flash
     * driver does the copy/merge/erase for us.
     */

    if (!noerase) {
	if ((devtype == CFE_DEV_FLASH) && !(flashinfo.flash_flags & FLASH_FLAG_NOERASE)) {
	    flash_range_t range;
	    range.range_base = offset;
	    range.range_length = copysize;
	    xprintf("Erasing flash...");
	    if (cfe_ioctl(fh,IOCTL_FLASH_ERASE_RANGE,
			  (uint8_t *) &range,sizeof(range),NULL,0) != 0) {
		printf("Failed to erase the flash\n");
		cfe_close(fh);
		return CFE_ERR_IOERR;
		}
	    }
        }

    /*
     * Program the flash
     */

    xprintf("Programming...");

    amtcopy = cfe_writeblk(fh,offset,ptr,copysize);

    if (copysize == amtcopy) {
	xprintf("done. %d bytes written\n",amtcopy);
	res = 0;
	}
    else {
	ui_showerror(amtcopy,"Failed.");
	res = CFE_ERR_IOERR;
	}
    
    /* 	
     * done!
     */

    cfe_close(fh);

    return res;
}
