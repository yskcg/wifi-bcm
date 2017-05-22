cmd_networking/arp.o := mipsel-uclibc-linux26-gcc -Wp,-MD,networking/.arp.o.d   -std=gnu99 -Iinclude -Ilibbb  -I/home/yangshaokun/work/ow/study_wifi/0.2.6/src/router/busybox-1.x/libbb -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D"BB_VER=KBUILD_STR(1.7.2)" -DBB_BT=AUTOCONF_TIMESTAMP -D_FORTIFY_SOURCE=2 -Os -DBCMWPA2 -DBCMQOS -D__CONFIG_WPS__ -D__CONFIG_EMF__ -D__CONFIG_IGMP_PROXY__ -DPHYMON -DLW_CUST -DVENDOR_BLUENET -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Os -fno-builtin-strlen -finline-limit=0 -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -Wdeclaration-after-statement -Wno-pointer-sign    -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(arp)"  -D"KBUILD_MODNAME=KBUILD_STR(arp)" -c -o networking/arp.o networking/arp.c

deps_networking/arp.o := \
  networking/arp.c \
    $(wildcard include/config/feature/clean/up.h) \
  include/libbb.h \
    $(wildcard include/config/selinux.h) \
    $(wildcard include/config/locale/support.h) \
    $(wildcard include/config/feature/shadowpasswds.h) \
    $(wildcard include/config/lfs.h) \
    $(wildcard include/config/feature/buffers/go/on/stack.h) \
    $(wildcard include/config/buffer.h) \
    $(wildcard include/config/ubuffer.h) \
    $(wildcard include/config/feature/buffers/go/in/bss.h) \
    $(wildcard include/config/feature/ipv6.h) \
    $(wildcard include/config/ture/ipv6.h) \
    $(wildcard include/config/feature/prefer/applets.h) \
    $(wildcard include/config/busybox/exec/path.h) \
    $(wildcard include/config/getopt/long.h) \
    $(wildcard include/config/feature/pidfile.h) \
    $(wildcard include/config/feature/syslog.h) \
    $(wildcard include/config/route.h) \
    $(wildcard include/config/gunzip.h) \
    $(wildcard include/config/ktop.h) \
    $(wildcard include/config/ioctl/hex2str/error.h) \
    $(wildcard include/config/feature/editing.h) \
    $(wildcard include/config/feature/editing/history.h) \
    $(wildcard include/config/ture/editing/savehistory.h) \
    $(wildcard include/config/feature/editing/savehistory.h) \
    $(wildcard include/config/feature/tab/completion.h) \
    $(wildcard include/config/feature/username/completion.h) \
    $(wildcard include/config/feature/editing/vi.h) \
    $(wildcard include/config/inux.h) \
    $(wildcard include/config/feature/devfs.h) \
  include/platform.h \
    $(wildcard include/config/werror.h) \
    $(wildcard include/config///.h) \
    $(wildcard include/config//nommu.h) \
    $(wildcard include/config//mmu.h) \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/byteswap.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/byteswap.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/endian.h \
    $(wildcard include/config/.h) \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/features.h \
    $(wildcard include/config/c99.h) \
    $(wildcard include/config/ix.h) \
    $(wildcard include/config/ix2.h) \
    $(wildcard include/config/ix199309.h) \
    $(wildcard include/config/ix199506.h) \
    $(wildcard include/config/en.h) \
    $(wildcard include/config/en/extended.h) \
    $(wildcard include/config/x98.h) \
    $(wildcard include/config/en2k.h) \
    $(wildcard include/config/gefile.h) \
    $(wildcard include/config/gefile64.h) \
    $(wildcard include/config/e/offset64.h) \
    $(wildcard include/config/d.h) \
    $(wildcard include/config/c.h) \
    $(wildcard include/config/ile.h) \
    $(wildcard include/config/ntrant.h) \
    $(wildcard include/config/tify/level.h) \
    $(wildcard include/config/i.h) \
    $(wildcard include/config/ern/inlines.h) \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/uClibc_config.h \
    $(wildcard include/config/mips/o32/abi//.h) \
    $(wildcard include/config/mips/n32/abi//.h) \
    $(wildcard include/config/mips/n64/abi//.h) \
    $(wildcard include/config/mips/isa/1//.h) \
    $(wildcard include/config/mips/isa/2//.h) \
    $(wildcard include/config/mips/isa/3//.h) \
    $(wildcard include/config/mips/isa/4//.h) \
    $(wildcard include/config/mips/isa/mips32//.h) \
    $(wildcard include/config/mips/isa/mips64//.h) \
    $(wildcard include/config//.h) \
    $(wildcard include/config/link//.h) \
    $(wildcard include/config//vfprintf//.h) \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/uClibc_arch_features.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/cdefs.h \
    $(wildcard include/config/espaces.h) \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/endian.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/arpa/inet.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/netinet/in.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/stdint.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/wordsize.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/socket.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/uio.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/types.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/types.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/include/stddef.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/kernel_types.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/typesizes.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/pthreadtypes.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/sched.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/time.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/select.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/select.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/sigset.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/time.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/sysmacros.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/uio.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/socket.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/include/limits.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/include/syslimits.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/limits.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/posix1_lim.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/local_lim.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/linux/limits.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/uClibc_local_lim.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/posix2_lim.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/xopen_lim.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/stdio_lim.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/sockaddr.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/asm/socket.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/asm/sockios.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/asm/ioctl.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/in.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/include/stdbool.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/mount.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/ioctl.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/ioctls.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/asm/ioctls.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/ioctl-types.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/ttydefaults.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/ctype.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/uClibc_touplow.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/dirent.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/dirent.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/errno.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/errno.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/errno_values.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/syscall.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/sysnum.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/fcntl.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/fcntl.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sgidefs.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/stat.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/stat.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/inttypes.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/mntent.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/stdio.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/paths.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/netdb.h \
    $(wildcard include/config/3/ascii/rules.h) \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/rpc/netdb.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/siginfo.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/netdb.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/setjmp.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/setjmp.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/signal.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/signum.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/sigaction.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/sigcontext.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/sigstack.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/ucontext.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/ucontext.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/sigthread.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/uClibc_stdio.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/uClibc_mutex.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/pthread.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sched.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/uClibc_clk_tck.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/initspin.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/uClibc_pthread.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/include/stdarg.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/stdlib.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/waitflags.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/waitstatus.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/alloca.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/string.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/mman.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/mman.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/statfs.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/statfs.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/time.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/wait.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/resource.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/resource.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/termios.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/termios.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/unistd.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/posix_opt.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/environments.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/confname.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/bits/getopt.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/utime.h \
  include/pwd_.h \
    $(wildcard include/config/use/bb/pwd/grp.h) \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/pwd.h \
  include/grp_.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/grp.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/sys/param.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/linux/param.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/asm/param.h \
  include/xatonum.h \
  include/inet_common.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/net/if.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/net/if_arp.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/netinet/ether.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/netinet/if_ether.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/linux/if_ether.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/linux/types.h \
    $(wildcard include/config/lbd.h) \
    $(wildcard include/config/lsf.h) \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/linux/posix_types.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/linux/stddef.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/asm/posix_types.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/asm/types.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/net/ethernet.h \
  /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/../../../../mipsel-linux-uclibc/sys-include/netpacket/packet.h \

networking/arp.o: $(deps_networking/arp.o)

$(deps_networking/arp.o):
