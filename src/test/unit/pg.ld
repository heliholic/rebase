
SECTIONS {
  /* BLOCK: on Windows (PE) output section must be page-aligned. Use 4-byte alignment otherwise */
  /* SUBALIGN: force 8-byte alignment of input sections for pg_registry.
     Gcc would otherwise pad to 32 bytes between object files, breaking the array walk.
     8, not 4: sizeof(pgRegistry_t) is a whole multiple of it on both 32- and 64-bit
     hosts, and 4 would under-align the 64-bit entries (pg.c asserts both). */
  .pg_registry BLOCK( DEFINED(__section_alignment__) ? __section_alignment__ : 8 ) :   SUBALIGN(8)
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__pg_registry_start = . );
    PROVIDE_HIDDEN (___pg_registry_start = . );
    KEEP (*(.pg_registry))
    KEEP (*(SORT(.pg_registry.*)))
    PROVIDE_HIDDEN (__pg_registry_end = . );
    PROVIDE_HIDDEN (___pg_registry_end = . );
    . = ALIGN(4);

    PROVIDE_HIDDEN (__pg_resetdata_start = . );
    PROVIDE_HIDDEN (___pg_resetdata_start = . );
    KEEP (*(.pg_resetdata))
    PROVIDE_HIDDEN (__pg_resetdata_end = . );
    PROVIDE_HIDDEN (___pg_resetdata_end = . );
    . = ALIGN(4);
  }
}
INSERT AFTER .rodata;

SECTIONS {
  .pg_resetfunc :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__pg_resetfunc_start = . );
    PROVIDE_HIDDEN (___pg_resetfunc_start = . );
    KEEP (*(.pg_resetfunc))
    KEEP (*(SORT(.pg_resetfunc.*)))
    PROVIDE_HIDDEN (__pg_resetfunc_end = . );
    PROVIDE_HIDDEN (___pg_resetfunc_end = . );
    . = ALIGN(4);
  }
}
INSERT AFTER .text;
