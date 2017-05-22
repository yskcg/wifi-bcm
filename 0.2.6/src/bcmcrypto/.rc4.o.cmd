cmd_drivers/net/wl/wl_sta//../../../../../../bcmcrypto/rc4.o := mipsel-uclibc-linux26-gcc -Wp,-MD,drivers/net/wl/wl_sta//../../../../../../bcmcrypto/.rc4.o.d  -nostdinc -isystem /opt/toolchains/hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/include -D__KERNEL__ -Iinclude  -include include/linux/autoconf.h -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fno-inline-functions-called-once -I../../include -DBCMDRIVER -Dlinux -Os  -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -ggdb -ffreestanding  -march=mips32 -Wa,-mips32 -Wa,--trap  -Iinclude/asm-mips/mach-generic -fomit-frame-pointer  -fno-stack-protector -Wdeclaration-after-statement -Wno-pointer-sign -DHNDCTF -DCTFPOOL -DCTFMAP -DNFLASH_SUPPORT -DWL_ALL_PASSIVE -DDMA -DWL_PPR_SUBBAND -DBCMDBG_TRAP -DWLC_LOW -DWLC_HIGH -DWLWSEC -DWLTPC -DSTA -DWET -DMAC_SPOOF -DIBSS_PEER_GROUP_KEY -DIBSS_PSK -DIBSS_PEER_MGMT -DIBSS_PEER_DISCOVERY_EVENT -DWLLED -DWME -DWLPIO -DWLAFTERBURNER -DCRAM -DWL11N -DWL11H -DWL11D -DDBAND -DWLRM -DWLCQ -DWLCNT -DDELTASTATS -DWLCOEX -DBCMSUP_PSK -DBCMINTSUP -DBCMDMA32 -DWLAMSDU -DWLAMSDU_SWDEAGG -DWLAMPDU -DWLAMPDU_HW -DWLAMPDU_MAC -DWLAMPDU_PRECEDENCE -DWLTINYDUMP -DPHY_HAL -DPHY_HAL -Idrivers/net/wl/wl_sta/ -Idrivers/net/wl/wl_sta//.. -Idrivers/net/wl/wl_sta//../../../../../../wl/linux -Idrivers/net/wl/wl_sta//../../../../../../wl/sys -Werror -Idrivers/net/wl/wl_sta//../../../../../../wl/phy  -DMODULE -mlong-calls -fno-common -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(rc4)"  -D"KBUILD_MODNAME=KBUILD_STR(wl_sta)" -c -o drivers/net/wl/wl_sta//../../../../../../bcmcrypto/.tmp_rc4.o drivers/net/wl/wl_sta//../../../../../../bcmcrypto/rc4.c

deps_drivers/net/wl/wl_sta//../../../../../../bcmcrypto/rc4.o := \
  drivers/net/wl/wl_sta//../../../../../../bcmcrypto/rc4.c \
  ../../include/typedefs.h \
  include/linux/version.h \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/lbd.h) \
    $(wildcard include/config/lsf.h) \
    $(wildcard include/config/resources/64bit.h) \
  include/linux/posix_types.h \
  include/linux/stddef.h \
  include/linux/compiler.h \
    $(wildcard include/config/enable/must/check.h) \
  include/linux/compiler-gcc4.h \
    $(wildcard include/config/forced/inlining.h) \
  include/linux/compiler-gcc.h \
  include/asm/posix_types.h \
  include/asm/sgidefs.h \
  include/asm/types.h \
    $(wildcard include/config/highmem.h) \
    $(wildcard include/config/64bit/phys/addr.h) \
    $(wildcard include/config/64bit.h) \
  ../../include/bcmdefs.h \
  ../../include/bcmcrypto/rc4.h \

drivers/net/wl/wl_sta//../../../../../../bcmcrypto/rc4.o: $(deps_drivers/net/wl/wl_sta//../../../../../../bcmcrypto/rc4.o)

$(deps_drivers/net/wl/wl_sta//../../../../../../bcmcrypto/rc4.o):
