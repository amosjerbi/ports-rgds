/*--------------------------------------------------------------------------------
	** Linker script for legacy DS homebrew, ARM9 side **
--------------------------------------------------------------------------------*/
OUTPUT_ARCH(arm)
ENTRY(__ds9_bootstub)

MEMORY {
	/* Physical binaries loaded from ROM (LMAs) */
	lma9    : ORIGIN = 0x2000000, LENGTH = 0x380000

	/* Runtime memory layout for code/data areas (VMAs) */
	vectors : ORIGIN = 0x0000000, LENGTH = 0x20     /* Vectors */
	itcm    : ORIGIN = 0x1ff8020, LENGTH = 0x7fe0   /* ITCM (minus vectors) */
	dtcm    : ORIGIN = 0x2ff0000, LENGTH = 0x3e80   /* DTCM (minus stacks/reserved BIOS memory) */
	dldi    : ORIGIN = 0x380b000, LENGTH = 0x4000   /* DLDI (on ARM7 WRAM) */

	/* Special TLS address space */
	tls     : ORIGIN = 0, LENGTH = 1K
}

PHDRS {
	/* ELF program headers for the ARM9 binary */
	main    PT_LOAD FLAGS(7);
	dldi    PT_LOAD FLAGS(7);
	vectors PT_LOAD FLAGS(7);
	itcm    PT_LOAD FLAGS(7);
	dtcm    PT_LOAD FLAGS(7);
	tls     PT_LOAD FLAGS(7);
	bss     PT_LOAD FLAGS(7);
}

/* Default initial stack locations and BIOS variables */
PROVIDE_HIDDEN( __sp_usr     = ORIGIN(dtcm) + LENGTH(dtcm) );
PROVIDE_HIDDEN( __sp_irq     = __sp_usr + 0x100            );
PROVIDE_HIDDEN( __sp_svc     = __sp_irq + 0x40             );
PROVIDE_HIDDEN( __irq_flags  = __sp_svc + 0x38             );
PROVIDE_HIDDEN( __irq_vector = __sp_svc + 0x3c             );

/* Overridable locations for ARM exception vectors (pointing to ARM9 BIOS by default) */
PROVIDE_HIDDEN( __arm_excpt_rst  = 0xffff0000 );
PROVIDE_HIDDEN( __arm_excpt_und  = 0xffff0004 );
PROVIDE_HIDDEN( __arm_excpt_svc  = 0xffff0008 );
PROVIDE_HIDDEN( __arm_excpt_pabt = 0xffff000c );
PROVIDE_HIDDEN( __arm_excpt_dabt = 0xffff0010 );
PROVIDE_HIDDEN( __arm_excpt_irq  = 0xffff0018 );
PROVIDE_HIDDEN( __arm_excpt_fiq  = 0xffff001c );

/* Not used */
PROVIDE_HIDDEN( __end__ = 0 );

/* Separate crt0 not used */
PROVIDE_HIDDEN( __crt0_lma   = ORIGIN(dtcm)      );
PROVIDE_HIDDEN( __crt0_start = __crt0_lma        );
PROVIDE_HIDDEN( __crt0_end   = __crt0_start + 32 );

/* DSi sections not used */
PROVIDE_HIDDEN( __twl_loadlist_lma   = 0 );
PROVIDE_HIDDEN( __twl_loadlist_start = 0 );
PROVIDE_HIDDEN( __twl_loadlist_end   = 0 );

SECTIONS {

	/* ================= */
	/*   Main RAM area   */
	/* ================= */

	/* Initialization code section (crt0) */
	.crt0 : {
		KEEP( *(.bootstub .bootstub.*) )
		*(.crt0 .crt0.*)
		*.crt0.* (.text .stub .text.* .gnu.linkonce.t.*)
		*.crt0.* (.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		*.crt0.* (.rodata .rodata.* .gnu.linkonce.r.*)
		*.crt0.* (.rodata1)
		. = ALIGN(4);
	} >lma9 :main = 0xff

	/* Loadlist data */
	.loadlist : ALIGN_WITH_INPUT {
		LONG(__itcm_start)
		LONG(__itcm_end)
		LONG(__itcm_end)

		LONG(__dtcm_start)
		LONG(__dtcm_end)
		LONG(__dtcm_bss_end)

		LONG(__tls_start)
		LONG(__tls_end)
		LONG(__main_bss_end)
	} >lma9 :main = 0xff

	/* Main user code/read-only data section */
	.main : ALIGN_WITH_INPUT {
		KEEP( *(SORT_NONE(.init)) )
		EXCLUDE_FILE(*.32.o) *(.text.unlikely .text.*_unlikely .text.unlikely.*)
		EXCLUDE_FILE(*.32.o) *(.text.exit .text.exit.*)
		EXCLUDE_FILE(*.32.o) *(.text.startup .text.startup.*)
		EXCLUDE_FILE(*.32.o) *(.text.hot .text.hot.*)
		EXCLUDE_FILE(*.32.o) *(.text .iplt .stub .text.* .gnu.linkonce.t.*)
		EXCLUDE_FILE(*.32.o) *(.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		KEEP( *(SORT_NONE(.fini)) )

		EXCLUDE_FILE(*.32.o) *(.rodata .rodata.* .gnu.linkonce.r.*)
		EXCLUDE_FILE(*.32.o) *(.rodata1)
		. = ALIGN(4);
	} >lma9 :main = 0xff

	/* Main user data section */
	.main.rw : ALIGN_WITH_INPUT {
		EXCLUDE_FILE(*.32.o) *(.data .data.* .gnu.linkonce.d.*)
		EXCLUDE_FILE(*.32.o) *(.data1)
		EXCLUDE_FILE(*.32.o) *(.igot.plt)
		SORT(CONSTRUCTORS)
		. = ALIGN(4);
	} >lma9 :main = 0xff

	/* ARM exception table */
	.ARM.extab : ALIGN_WITH_INPUT {
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} >lma9 :main = 0xff

	/* ARM exception index table (replaces .eh_frame on AArch32) */
	.ARM.exidx : ALIGN_WITH_INPUT {
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} >lma9 :main = 0xff

	/* Symbols needed by exception unwind code */
	PROVIDE_HIDDEN( __exidx_start = ADDR(.ARM.exidx)                      );
	PROVIDE_HIDDEN( __exidx_end   = ADDR(.ARM.exidx) + SIZEOF(.ARM.exidx) );

	/* Exception frame header (not used on AArch32) */
	.eh_frame_hdr : ALIGN_WITH_INPUT {
		*(.eh_frame_hdr)
		*(.eh_frame_entry .eh_frame_entry.*)
	} >lma9 :main = 0xff

	/* Exception frame information (not used on AArch32) */
	.eh_frame : ALIGN_WITH_INPUT {
		KEEP( *(.eh_frame) )
		*(.eh_frame.*)
	} >lma9 :main = 0xff

	/* GCC language agnostic exception table (not used on AArch32?) */
	.gcc_except_table : ALIGN_WITH_INPUT {
		*(.gcc_except_table .gcc_except_table.*)
	} >lma9 :main = 0xff

	/* GNU exception table (not used on AArch32?) */
	.gnu_extab : ALIGN_WITH_INPUT {
		*(.gnu_extab*)
	} >lma9 :main = 0xff

	/* TLS metadata */
	.tinfo : ALIGN_WITH_INPUT {
		LONG(__tls_start)
		LONG(SIZEOF(.main.tls))
		LONG(SIZEOF(.tdata))
	} >lma9 :main = 0xff

	PROVIDE_HIDDEN( __tls_info = ADDR(.tinfo) );

	/* Table of special constructors that run before _init */
	.preinit_array : ALIGN_WITH_INPUT {
		PROVIDE_HIDDEN( __preinit_array_start = . );
		KEEP( *(.preinit_array) )
		PROVIDE_HIDDEN( __preinit_array_end = . );
	} >lma9 :main = 0xff

	/* Table of global constructors (runs after _init) */
	.init_array : ALIGN_WITH_INPUT {
		PROVIDE_HIDDEN( __init_array_start = . );
		KEEP( *(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)) )
		KEEP( *(.init_array .ctors) )
		PROVIDE_HIDDEN( __init_array_end = . );
	} >lma9 :main = 0xff

	/* Table of global destructors (runs before _fini) */
	.fini_array : ALIGN_WITH_INPUT {
		PROVIDE_HIDDEN( __fini_array_start = . );
		KEEP( *(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)) )
		KEEP( *(.fini_array .dtors) )
		PROVIDE_HIDDEN( __fini_array_end = . );
	} >lma9 :main = 0xff

	/* ---------------- LMA sections ---------------- */

	/* DLDI area (copied to ARM7) */
	.dldi : {
		KEEP( *(.dldi) )
	} >dldi AT>lma9 :dldi = 0x00

	PROVIDE_HIDDEN( __dldi_lma = SIZEOF(.dldi) ? LOADADDR(.dldi) : 0 );

	/* ARM exception vectors (start of ITCM) */
	.vectors : {
		KEEP( *(.vectors) )
	} >vectors AT>lma9 :vectors = 0xff

	/* ITCM section */
	.itcm : {
		*(.itcm .itcm.*)
		*.itcm.* (.text .stub .text.* .gnu.linkonce.t.*)
		*.32.o (.text .stub .text.* .gnu.linkonce.t.*)
		*.itcm.* (.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		*.32.o (.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		*.itcm.* (.rodata .rodata.* .gnu.linkonce.r.*)
		*.32.o (.rodata .rodata.* .gnu.linkonce.r.*)
		*.itcm.* (.rodata1)
		*.32.o (.rodata1)
		. = ALIGN(4);
	} >itcm AT>lma9 :itcm = 0xff

	/* Load information for ITCM */
	PROVIDE_HIDDEN( __itcm_lma   = LOADADDR(.vectors)             );
	PROVIDE_HIDDEN( __itcm_start = ADDR(.itcm) - SIZEOF(.vectors) );
	PROVIDE_HIDDEN( __itcm_end   = ADDR(.itcm) + SIZEOF(.itcm)    );
	ASSERT( __itcm_lma + SIZEOF(.vectors) == LOADADDR(.itcm), "Error: gap between vectors and itcm" )

	/* DTCM section */
	.dtcm : {
		*(.dtcm .dtcm.*)
		*.32.o (.data .data.* .gnu.linkonce.d.*)
		*.32.o (.data1)
		. = ALIGN(4);
	} >dtcm AT>lma9 :dtcm = 0xff

	/* Load information for DTCM */
	PROVIDE_HIDDEN( __dtcm_lma   = LOADADDR(.dtcm)             );
	PROVIDE_HIDDEN( __dtcm_start = ADDR(.dtcm)                 );
	PROVIDE_HIDDEN( __dtcm_end   = ADDR(.dtcm) + SIZEOF(.dtcm) );

	/* DTCM BSS area */
	.dtcm.bss . (NOLOAD) : {
		*(.sbss .sbss.*)
		*.32.o (.dynbss)
		*.32.o (.bss .bss.* .gnu.linkonce.b.*)
		*.32.o (COMMON)
		. = ALIGN(4);
	} >dtcm AT>lma9 :dtcm

	/* Information for DTCM BSS */
	PROVIDE_HIDDEN( __dtcm_bss_start = ADDR(.dtcm.bss)                     );
	PROVIDE_HIDDEN( __dtcm_bss_end   = ADDR(.dtcm.bss) + SIZEOF(.dtcm.bss) );

	/* TLS static data area */
	.tdata : {
		*(.tdata .tdata.* .gnu.linkonce.td.*)
		. = ALIGN(4);
	} >tls AT>lma9 :tls = 0xff

	/* TLS BSS area */
	.tbss (NOLOAD) : {
		*(.tbss .tbss.* .gnu.linkonce.tb.*)
		*(.tcommon)
		. = ALIGN(4);
	} >tls AT>lma9 :tls

	/* TLS sanity checks */
	ASSERT( ALIGNOF(.tdata) <= 8 || ALIGNOF(.tbss) <= 8,
		"Error: TLS is overaligned (up to 8-byte alignment is supported)" )
	ASSERT( ADDR(.tbss) + SIZEOF(.tbss) <= ORIGIN(tls) + LENGTH(tls),
		"Error: TLS section too large" )

	/* Main thread TLS area */
	.main.tls ALIGN(LOADADDR(.dldi), 8) (NOLOAD) : {
		. += ADDR(.tbss) + SIZEOF(.tbss) - ADDR(.tdata);
	} :NONE

	/* Information for main thread TLS area */
	PROVIDE_HIDDEN( __tls_lma   = LOADADDR(.tdata)                 );
	PROVIDE_HIDDEN( __tls_start = ADDR(.main.tls)                  );
	PROVIDE_HIDDEN( __tls_end   = ADDR(.main.tls) + SIZEOF(.tdata) );

	/* ---------------- Main RAM BSS ---------------- */

	/* Main BSS area */
	.main.bss ADDR(.main.tls)+SIZEOF(.main.tls) (NOLOAD) : {
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(4);
	} :bss

	/* Information for BSS */
	PROVIDE_HIDDEN( __main_bss_start = ADDR(.main.bss)                     );
	PROVIDE_HIDDEN( __main_bss_end   = ADDR(.main.bss) + SIZEOF(.main.bss) );

	/* Define start of main heap */
	. = .;
	PROVIDE_HIDDEN( __heap_start_ntr = . );
	PROVIDE_HIDDEN( __heap_start_twl = . );

	/* Ensure DS-mode sections fit within the DS's main RAM allotted to ARM9 */
	ASSERT( . <= 0x2380000, "Error: DS-mode sections overflow DS ARM9 memory size (3.5 MB)" )

	/* Validate the loadlist */
	PROVIDE_HIDDEN( __loadlist_lma = __itcm_lma );
	ASSERT( __itcm_lma + SIZEOF(.vectors) + SIZEOF(.itcm) == __dtcm_lma,
		"Error: Invalid gap between ITCM and DTCM LMAs" )

	PROVIDE_HIDDEN( __loadlist_start = LOADADDR(.loadlist)                     );
	PROVIDE_HIDDEN( __loadlist_end   = LOADADDR(.loadlist) + SIZEOF(.loadlist) );

	/* ====================== */
	/*   Debugging metadata   */
	/* ====================== */

	/* Stabs debugging sections.  */
	.stab          0 : { *(.stab) } :NONE
	.stabstr       0 : { *(.stabstr) } :NONE
	.stab.excl     0 : { *(.stab.excl) } :NONE
	.stab.exclstr  0 : { *(.stab.exclstr) } :NONE
	.stab.index    0 : { *(.stab.index) } :NONE
	.stab.indexstr 0 : { *(.stab.indexstr) } :NONE
	.comment       0 : { *(.comment) } :NONE
	.gnu.build.attributes : { *(.gnu.build.attributes .gnu.build.attributes.*) } :NONE
	/* DWARF debug sections.
	   Symbols in the DWARF debugging sections are relative to the beginning
	   of the section so we begin them at 0. */
	/* DWARF 1 */
	.debug          0 : { *(.debug) } :NONE
	.line           0 : { *(.line) } :NONE
	/* GNU DWARF 1 extensions */
	.debug_srcinfo  0 : { *(.debug_srcinfo) } :NONE
	.debug_sfnames  0 : { *(.debug_sfnames) } :NONE
	/* DWARF 1.1 and DWARF 2 */
	.debug_aranges  0 : { *(.debug_aranges) } :NONE
	.debug_pubnames 0 : { *(.debug_pubnames) } :NONE
	/* DWARF 2 */
	.debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) } :NONE
	.debug_abbrev   0 : { *(.debug_abbrev) } :NONE
	.debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) } :NONE
	.debug_frame    0 : { *(.debug_frame) } :NONE
	.debug_str      0 : { *(.debug_str) } :NONE
	.debug_loc      0 : { *(.debug_loc) } :NONE
	.debug_macinfo  0 : { *(.debug_macinfo) } :NONE
	/* SGI/MIPS DWARF 2 extensions */
	.debug_weaknames 0 : { *(.debug_weaknames) } :NONE
	.debug_funcnames 0 : { *(.debug_funcnames) } :NONE
	.debug_typenames 0 : { *(.debug_typenames) } :NONE
	.debug_varnames  0 : { *(.debug_varnames) } :NONE
	/* DWARF 3 */
	.debug_pubtypes 0 : { *(.debug_pubtypes) } :NONE
	.debug_ranges   0 : { *(.debug_ranges) } :NONE
	/* DWARF Extension.  */
	.debug_macro    0 : { *(.debug_macro) } :NONE
	.debug_addr     0 : { *(.debug_addr) } :NONE
	.ARM.attributes 0 : { KEEP (*(.ARM.attributes)) KEEP (*(.gnu.attributes)) } :NONE
	.note.gnu.arm.ident 0 : { KEEP (*(.note.gnu.arm.ident)) } :NONE
	/DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.stack) }
}
