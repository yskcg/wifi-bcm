OUTPUT_ARCH(mips)
ENTRY(vec_reset)
SECTIONS
{
  . = 0x80700000;
  .text :
   {
     _ftext = . ;
    *(.init)
     eprol = .;
    *(.text)
    *(.text.*)
    *(.fini)
    *(.rodata)
    *(.rodata.*)
     _etext = .;
   }
  .data :
  {
    _gp = ALIGN(16) + 0x8000;
    _fdata = .;
    *(.rdata)
    *(.data)
    CONSTRUCTORS
    *(.sdata)
  }
   . = ALIGN(16);
   _edata = .;
   _fbss = .;
  .sbss : {
    *(.sbss)
    *(.scommon)
  }
  .bss : {
    *(.bss)
    *(COMMON)
  }
  . = ALIGN(16);
   _end = .;
}
