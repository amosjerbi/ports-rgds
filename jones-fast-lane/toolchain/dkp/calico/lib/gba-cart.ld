/*--------------------------------------------------------------------------------
	** Linker script for GBA homebrew (cart version) **
--------------------------------------------------------------------------------*/
OUTPUT_ARCH(arm)
ENTRY(__gba_rom_header)

MEMORY {
	/* Physical binary in ROM (LMAs) */
	rom   : ORIGIN = 0x8000000, LENGTH = 0x2000000 /* Cartridge ROM (16-bit data bus) */

	/* Runtime memory layout for code/data areas (VMAs) */
	ewram : ORIGIN = 0x2000000, LENGTH = 0x40000   /* External Work RAM (16-bit data bus) */
	iwram : ORIGIN = 0x3000000, LENGTH = 0x7e00    /* Internal Work RAM (32-bit data bus) (minus stacks/reserved BIOS memory) */
}

/* Default initial stack locations and BIOS variables */
PROVIDE_HIDDEN( __sp_usr     = ORIGIN(iwram) + LENGTH(iwram) );
PROVIDE_HIDDEN( __sp_irq     = __sp_usr + 0x1a0              );
PROVIDE_HIDDEN( __sp_svc     = __sp_irq + 0x40               );
PROVIDE_HIDDEN( __irq_flags  = __sp_svc + 0x18               );
PROVIDE_HIDDEN( __boot_flag  = __sp_svc + 0x1a               );
PROVIDE_HIDDEN( __irq_vector = __sp_svc + 0x1c               );

SECTIONS {
	/* ROM header */
	.romhdr : {
		KEEP( *(.romhdr) )
		. = ALIGN(4);
	} >rom = 0xff

	/* Initialization code section (crt0) */
	.crt0 : {
		KEEP( *(.crt0 .crt0.*) )
		*(.boot .boot.*)
		*.boot.* (.text .stub .text.* .gnu.linkonce.t.*)
		*.boot.* (.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		*.boot.* (.rodata .rodata.* .gnu.linkonce.r.*)
		*.boot.* (.rodata1)
		. = ALIGN(4);
	} >rom = 0xff

	/* IWRAM section */
	.iwram : {
		*(.iwram .iwram.*)
		*.iwram.* (.text .stub .text.* .gnu.linkonce.t.*)
		*.32.o (.text .stub .text.* .gnu.linkonce.t.*)
		*.iwram.* (.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		*.32.o (.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		*.iwram.* (.rodata .rodata.* .gnu.linkonce.r.*)
		*.32.o (.rodata .rodata.* .gnu.linkonce.r.*)
		*.iwram.* (.rodata1)
		*.32.o (.rodata1)
		EXCLUDE_FILE(*.16.o) *(.data .data.* .gnu.linkonce.d.*)
		EXCLUDE_FILE(*.16.o) *(.data1)
		SORT(CONSTRUCTORS)
		. = ALIGN(4);
	} >iwram AT>rom = 0xff

	/* Load information for IWRAM */
	PROVIDE_HIDDEN( __iwram_lma   = LOADADDR(.iwram)              );
	PROVIDE_HIDDEN( __iwram_start = ADDR(.iwram)                  );
	PROVIDE_HIDDEN( __iwram_end   = ADDR(.iwram) + SIZEOF(.iwram) );

	/* IWRAM BSS area */
	.iwram.bss (NOLOAD) : {
		EXCLUDE_FILE(*.16.o) *(.dynbss)
		EXCLUDE_FILE(*.16.o) *(.bss .bss.* .gnu.linkonce.b.*)
		EXCLUDE_FILE(*.16.o) *(COMMON)
		. = ALIGN(4);
	} >iwram

	/* EWRAM section */
	.ewram : {
		*(.ewram .ewram.*)
		*.16.o (.data .data.* .gnu.linkonce.d.*)
		*.16.o (.data1)
		. = ALIGN(4);
	} >ewram AT>rom = 0xff

	/* Load information for EWRAM */
	PROVIDE_HIDDEN( __ewram_lma   = LOADADDR(.ewram)              );
	PROVIDE_HIDDEN( __ewram_start = ADDR(.ewram)                  );
	PROVIDE_HIDDEN( __ewram_end   = ADDR(.ewram) + SIZEOF(.ewram) );

	/* EWRAM BSS area */
	.ewram.bss (NOLOAD) : {
		*(.sbss .sbss.*)
		*.16.o (.dynbss)
		*.16.o (.bss .bss.* .gnu.linkonce.b.*)
		*.16.o (COMMON)
		. = ALIGN(4);
	} >ewram

	/* Define start/end of heap area (on EWRAM) */
	PROVIDE_HIDDEN( __heap_start = ADDR(.ewram.bss) + SIZEOF(.ewram.bss) );
	PROVIDE_HIDDEN( __heap_end   = ORIGIN(ewram) + LENGTH(ewram)         );

	/* _init function */
	.init : {
		KEEP( *(SORT_NONE(.init)) )
	} >rom = 0xff

	/* Main code section */
	.text : {
		*(.text.unlikely .text.*_unlikely .text.unlikely.*)
		*(.text.exit .text.exit.*) /* (TODO: remove) */
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(.text .stub .text.* .gnu.linkonce.t.*)
		*(.glue_7t .glue_7 .vfp11_veneer .v4_bx)
	} >rom = 0xff

	/* _fini function (TODO: remove) */
	.fini : {
		KEEP( *(SORT_NONE(.init)) )
	} >rom = 0xff

	/* Main constant data section */
	.rodata : {
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		*(.rodata1)
	} >rom = 0xff

	/* ARM exception table */
	.ARM.extab : {
		*(.ARM.extab* .gnu.linkonce.armextab.*)
		. = ALIGN(4);
	} >rom = 0xff

	/* ARM exception index table (replaces .eh_frame on AArch32) */
	.ARM.exidx : {
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} >rom = 0xff

	/* Symbols needed by exception unwind code */
	PROVIDE_HIDDEN( __exidx_start = ADDR(.ARM.exidx)                      );
	PROVIDE_HIDDEN( __exidx_end   = ADDR(.ARM.exidx) + SIZEOF(.ARM.exidx) );

	/* Exception frame header (not used on AArch32) */
	.eh_frame_hdr : {
		*(.eh_frame_hdr)
		*(.eh_frame_entry .eh_frame_entry.*)
	} >rom = 0xff

	/* Exception frame information (not used on AArch32) */
	.eh_frame : {
		KEEP( *(.eh_frame) )
		*(.eh_frame.*)
	} >rom = 0xff

	/* GCC language agnostic exception table (not used on AArch32?) */
	.gcc_except_table : {
		*(.gcc_except_table .gcc_except_table.*)
	} >rom = 0xff

	/* GNU exception table (not used on AArch32?) */
	.gnu_extab : {
		*(.gnu_extab*)
	} >rom = 0xff

	/* Table of special constructors that run before _init */
	.preinit_array : {
		PROVIDE_HIDDEN( __preinit_array_start = . );
		KEEP( *(.preinit_array) )
		PROVIDE_HIDDEN( __preinit_array_end = . );
	} >rom = 0xff

	/* Table of global constructors (runs after _init) */
	.init_array : {
		PROVIDE_HIDDEN( __init_array_start = . );
		KEEP( *(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)) )
		KEEP( *(.init_array .ctors) )
		PROVIDE_HIDDEN( __init_array_end = . );
	} >rom = 0xff

	/* Table of global destructors (runs before _fini) (TODO: remove) */
	.fini_array : {
		PROVIDE_HIDDEN( __fini_array_start = . );
		KEEP( *(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)) )
		KEEP( *(.fini_array .dtors) )
		PROVIDE_HIDDEN( __fini_array_end = . );
	} >rom = 0xff

	/* ====================== */
	/*   Debugging metadata   */
	/* ====================== */

	/* Stabs debugging sections.  */
	.stab          0 : { *(.stab) }
	.stabstr       0 : { *(.stabstr) }
	.stab.excl     0 : { *(.stab.excl) }
	.stab.exclstr  0 : { *(.stab.exclstr) }
	.stab.index    0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }
	.comment       0 : { *(.comment) }
	.gnu.build.attributes : { *(.gnu.build.attributes .gnu.build.attributes.*) }
	/* DWARF debug sections.
	   Symbols in the DWARF debugging sections are relative to the beginning
	   of the section so we begin them at 0. */
	/* DWARF 1 */
	.debug          0 : { *(.debug) }
	.line           0 : { *(.line) }
	/* GNU DWARF 1 extensions */
	.debug_srcinfo  0 : { *(.debug_srcinfo) }
	.debug_sfnames  0 : { *(.debug_sfnames) }
	/* DWARF 1.1 and DWARF 2 */
	.debug_aranges  0 : { *(.debug_aranges) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	/* DWARF 2 */
	.debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }
	.debug_macinfo  0 : { *(.debug_macinfo) }
	/* SGI/MIPS DWARF 2 extensions */
	.debug_weaknames 0 : { *(.debug_weaknames) }
	.debug_funcnames 0 : { *(.debug_funcnames) }
	.debug_typenames 0 : { *(.debug_typenames) }
	.debug_varnames  0 : { *(.debug_varnames) }
	/* DWARF 3 */
	.debug_pubtypes 0 : { *(.debug_pubtypes) }
	.debug_ranges   0 : { *(.debug_ranges) }
	/* DWARF Extension.  */
	.debug_macro    0 : { *(.debug_macro) }
	.debug_addr     0 : { *(.debug_addr) }
	.ARM.attributes 0 : { KEEP (*(.ARM.attributes)) KEEP (*(.gnu.attributes)) }
	.note.gnu.arm.ident 0 : { KEEP (*(.note.gnu.arm.ident)) }
	/DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.stack) }
}
