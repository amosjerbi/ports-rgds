// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once

#if __ARM_ARCH_4T__
.arch armv4t
#elif __ARM_ARCH_5TE__
.arch armv5te
#else
#error "Unsupported architecture"
#endif

.fpu softvfp

.cfi_sections .debug_frame

.macro SECT_TEXT name, align=2, section=text
	.section .\section\().\name, "ax", %progbits
	.align   \align
.endm

.macro SECT_RODATA name, align=2, section=rodata
	.section .\section\().\name, "a"
	.align   \align
.endm

.macro SECT_DATA name, align=2, section=data
	.section .\section\().\name, "aw"
	.align   \align
.endm

.macro SECT_BSS name, align=2, section=bss
	.section .\section\().\name, "aw", %nobits
	.align   \align
.endm

.macro FUNC_START32 name, section=text, linkage=global
	SECT_TEXT \name, 2, \section
	.\linkage \name
	.arm
	.type     \name, %function
\name:
	.cfi_startproc
	.size     \name, 3135f-\name
.endm

.macro FUNC_START16 name, section=text, linkage=global
	SECT_TEXT \name, 1, \section
	.\linkage \name
	.thumb_func
	.type     \name, %function
\name:
	.cfi_startproc
	.size     \name, 3135f-\name
.endm

.macro FUNC_END
	.pool
3135:
	.cfi_endproc
.endm

.macro REFERENCE symbol
	.section .rodata.ref.\symbol, "a"
	.align   2
	.word    \symbol
.endm
