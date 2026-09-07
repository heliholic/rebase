SECTIONS {
  /* BLOCK: on Windows (PE) output section must be page-aligned. Use 4-byte alignment otherwise */
  /* SUBALIGN: pin the alignment of the input sections making up pg_registry.
     It must divide sizeof(pgRegistry_t), or the linker pads each object file's
     contribution out to it and PG_FOREACH walks into the gap -- gcc's 32-byte
     default segfaults the walk on the first entry past the padding.
     8 rather than 4 because that is _Alignof(pgRegistry_t) on a 64-bit host,
     so the section and every entry in it land on the boundary the compiler
     already assumed for the pointer members. 4 divides the entry size too and
     so happens to work today, but it records a weaker alignment on the section
     than the struct actually needs. pg.c asserts both properties. */
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
INSERT BEFORE .fini_array;

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
