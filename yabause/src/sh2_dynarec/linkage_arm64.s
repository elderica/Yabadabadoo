/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Yabause - linkage_arm64.s                                             *
 *   AArch64 linkage for the SH2 recompiler, after linkage_arm.s           *
 *   Copyright (C) 2009-2011 Ari64                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Registers in generated code (see assem_arm64.h): x25 cycle count, x28
   &dynarec_local, x30 the master's return address into generated code
   (after bl slave_entry), x23 the slave's (before b cc_interrupt). Nothing
   else is live when generated code enters this file, so x19-x24 hold values
   across calls here as r4-r9 do in linkage_arm.s. */

	.file	"linkage_arm64.s"

/* Offsets in dynarec_local (also listed in assem_arm64.c) */
DL_MASTER_REG = 64
DL_MASTER_CC = 152
DL_MASTER_PC = 156
DL_MASTER_IP = 160
DL_SLAVE_REG = 168
DL_SLAVE_CC = 256
DL_SLAVE_PC = 260
DL_SLAVE_IP = 264
DL_M68KCENTICYCLES = 272
DL_M68KCYCLES = 276
DL_DECILINECOUNT = 280
DL_DECILINECYCLES = 284
DL_SH2CYCLES = 288
DL_SCUCYCLES = 292
DL_RCCOUNT = 296
DL_MINI_HT_MASTER = 304
DL_MINI_HT_SLAVE = 816
DL_RESTORE_CANDIDATE = 1328
DL_MEMORY_MAP = 1840
DL_SIZE = DL_MEMORY_MAP + 8388608

/* Address of a symbol, through the GOT (position-independent) */
.macro ldaddr reg, sym
	adrp	\reg, :got:\sym
	ldr	\reg, [\reg, #:got_lo12:\sym]
.endm

/* memory_map[0xC0000] (the data array) = dataarray_map[\n] */
.macro map_dataarray n
	ldaddr	x10, dataarray_map
	add	x11, x28, #DL_MEMORY_MAP
	ldr	x10, [x10, #\n*8]
	add	x11, x11, #0x600, lsl #12
	str	x10, [x11]
.endm

	.global	sh2_dynarec_target
	.global	dynarec_local
	.global	master_reg
	.global	master_cc
	.global	master_pc
	.global	master_ip
	.global	slave_reg
	.global	slave_cc
	.global	slave_pc
	.global	slave_ip
	.global	mini_ht_master
	.global	mini_ht_slave
	.global	restore_candidate
	.global	memory_map
	.global	rccount

	.bss
	.align	12
	.type	sh2_dynarec_target, %object
	.size	sh2_dynarec_target, 16777216
sh2_dynarec_target:
	.space	16777216
	.align	4
	.type	dynarec_local, %object
	.size	dynarec_local, DL_SIZE
dynarec_local:
	.space	DL_SIZE
master_reg = dynarec_local + DL_MASTER_REG
	.type	master_reg, %object
	.size	master_reg, 88
master_cc = dynarec_local + DL_MASTER_CC
	.type	master_cc, %object
	.size	master_cc, 4
master_pc = dynarec_local + DL_MASTER_PC
	.type	master_pc, %object
	.size	master_pc, 4
master_ip = dynarec_local + DL_MASTER_IP
	.type	master_ip, %object
	.size	master_ip, 8
slave_reg = dynarec_local + DL_SLAVE_REG
	.type	slave_reg, %object
	.size	slave_reg, 88
slave_cc = dynarec_local + DL_SLAVE_CC
	.type	slave_cc, %object
	.size	slave_cc, 4
slave_pc = dynarec_local + DL_SLAVE_PC
	.type	slave_pc, %object
	.size	slave_pc, 4
slave_ip = dynarec_local + DL_SLAVE_IP
	.type	slave_ip, %object
	.size	slave_ip, 8
rccount = dynarec_local + DL_RCCOUNT
	.type	rccount, %object
	.size	rccount, 4
mini_ht_master = dynarec_local + DL_MINI_HT_MASTER
	.type	mini_ht_master, %object
	.size	mini_ht_master, 512
mini_ht_slave = dynarec_local + DL_MINI_HT_SLAVE
	.type	mini_ht_slave, %object
	.size	mini_ht_slave, 512
restore_candidate = dynarec_local + DL_RESTORE_CANDIDATE
	.type	restore_candidate, %object
	.size	restore_candidate, 512
memory_map = dynarec_local + DL_MEMORY_MAP
	.type	memory_map, %object
	.size	memory_map, 8388608

	.text
	.align	2

/* void YabauseDynarecOneFrameExec(int m68kcycles, int m68kcenticycles) */
	.global	YabauseDynarecOneFrameExec
	.type	YabauseDynarecOneFrameExec, %function
YabauseDynarecOneFrameExec:
	stp	x29, x30, [sp, #-96]!
	mov	x29, sp
	stp	x19, x20, [sp, #16]
	stp	x21, x22, [sp, #32]
	stp	x23, x24, [sp, #48]
	stp	x25, x26, [sp, #64]
	stp	x27, x28, [sp, #80]
	ldaddr	x28, dynarec_local
	str	w0, [x28, #DL_M68KCYCLES]
	str	w1, [x28, #DL_M68KCENTICYCLES]
	str	wzr, [x28, #DL_DECILINECOUNT]
	ldr	x30, [x28, #DL_MASTER_IP]
newline:
	ldaddr	x0, decilinestop_p
	ldaddr	x1, yabsys_timing_bits
	ldaddr	x2, SH2CycleFrac_p
	ldaddr	x3, yabsys_timing_mask
	ldr	x0, [x0] /* pointer to decilinestop */
	ldr	w1, [x1] /* yabsys_timing_bits */
	ldr	x2, [x2] /* pointer to SH2CycleFrac */
	ldr	w3, [x3] /* yabsys_timing_mask */
	ldr	w0, [x0] /* decilinestop */
	ldr	w4, [x2] /* SH2CycleFrac */
	add	w5, w0, w0, lsl #2
	lsr	w6, w0, w1 /* decilinecycles = decilinestop>>yabsys_timing_bits */
	lsl	w5, w5, #1 /* cyclesinc=decilinestop*10 */
	str	w6, [x28, #DL_DECILINECYCLES]
	add	w1, w1, #1 /* yabsys_timing_bits+1 */
	add	w6, w6, w6, lsl #3 /* decilinecycles*9 */
	lsl	w3, w3, #1
	add	w5, w5, w4 /* cyclesinc+=SH2CycleFrac */
	orr	w3, w3, #1 /* ((YABSYS_TIMING_MASK << 1) | 1) */
	and	w3, w5, w3 /* SH2CycleFrac &= ... */
	lsr	w5, w5, w1 /* scucycles */
	str	w3, [x2] /* SH2CycleFrac */
	str	w5, [x28, #DL_SCUCYCLES]
	lsl	w5, w5, #1 /* sh2cycles=scucycles*2 */
	sub	w19, w5, w6 /* sh2cycles(full line) -= decilinecycles*9 */
	str	w19, [x28, #DL_SH2CYCLES]
	ldaddr	x7, MSH2
	ldaddr	x8, NumberOfInterruptsOffset
	ldaddr	x9, CurrentSH2
	ldr	x7, [x7] /* MSH2 */
	ldr	x8, [x8] /* NumberOfInterruptsOffset */
	str	x7, [x9] /* CurrentSH2 */
	map_dataarray 0 /* the master's data array */
	ldr	w12, [x7, x8]
	cbnz	w12, master_handle_interrupts
	ldr	w25, [x28, #DL_MASTER_CC]
	sub	w25, w25, w19
	br	x30
/* w19 = cycles of this part of the line */
master_handle_interrupts:
	str	x30, [x28, #DL_MASTER_IP]
	bl	DynarecMasterHandleInterrupts
	ldr	w25, [x28, #DL_MASTER_CC]
	ldr	x30, [x28, #DL_MASTER_IP]
	sub	w25, w25, w19
	br	x30
	.size	YabauseDynarecOneFrameExec, .-YabauseDynarecOneFrameExec

/* The master's slice ends (bl from generated code): run the slave */
	.global	slave_entry
	.type	slave_entry, %function
slave_entry:
	ldr	w0, [x28, #DL_SH2CYCLES]
	str	w25, [x28, #DL_MASTER_CC]
	str	x30, [x28, #DL_MASTER_IP]
	bl	DynarecFRTExec
	ldr	w0, [x28, #DL_SH2CYCLES]
	bl	DynarecWDTExec
	ldr	x20, [x28, #DL_SLAVE_IP]
	cbz	x20, cc_interrupt_master
	ldr	w19, [x28, #DL_SH2CYCLES]
	ldaddr	x7, SSH2
	ldaddr	x8, NumberOfInterruptsOffset
	ldaddr	x9, CurrentSH2
	ldr	x7, [x7]
	ldr	x8, [x8]
	str	x7, [x9] /* CurrentSH2 */
	map_dataarray 1 /* the slave's data array */
	ldr	w12, [x7, x8]
	cbnz	w12, slave_handle_interrupts
	ldr	w25, [x28, #DL_SLAVE_CC]
	sub	w25, w25, w19
	br	x20
slave_handle_interrupts:
	bl	DynarecSlaveHandleInterrupts
	ldr	w25, [x28, #DL_SLAVE_CC]
	ldr	x0, [x28, #DL_SLAVE_IP]
	sub	w25, w25, w19
	br	x0
	.size	slave_entry, .-slave_entry

/* The slave's slice ends (b from generated code, return address in x23) */
	.global	cc_interrupt
	.type	cc_interrupt, %function
cc_interrupt:
	ldr	w0, [x28, #DL_SH2CYCLES]
	str	w25, [x28, #DL_SLAVE_CC]
	str	x23, [x28, #DL_SLAVE_IP]
	bl	DynarecFRTExec
	ldr	w0, [x28, #DL_SH2CYCLES]
	bl	DynarecWDTExec
	.size	cc_interrupt, .-cc_interrupt
/* Next part of the line (a tenth), then back to the master */
	.global	cc_interrupt_master
	.type	cc_interrupt_master, %function
cc_interrupt_master:
	ldr	w0, [x28, #DL_DECILINECOUNT]
	ldr	w19, [x28, #DL_DECILINECYCLES]
	cmp	w0, #8
	add	w0, w0, #1
	b.hi	.A3
	str	w0, [x28, #DL_DECILINECOUNT]
	b.eq	.A2
	str	w19, [x28, #DL_SH2CYCLES]
	ldr	x30, [x28, #DL_MASTER_IP]
.A1:
	ldaddr	x7, MSH2
	ldaddr	x8, NumberOfInterruptsOffset
	ldaddr	x9, CurrentSH2
	ldr	x7, [x7] /* MSH2 */
	ldr	x8, [x8] /* NumberOfInterruptsOffset */
	str	x7, [x9] /* CurrentSH2 */
	map_dataarray 0 /* the master's data array */
	ldr	w12, [x7, x8]
	cbnz	w12, master_handle_interrupts
	ldr	w25, [x28, #DL_MASTER_CC]
	sub	w25, w25, w19
	br	x30
.A2:
	bl	Vdp2HBlankIN
	ldr	x30, [x28, #DL_MASTER_IP]
	b	.A1
.A3:
	ldr	w0, [x28, #DL_SCUCYCLES]
	lsl	w0, w0, #1 /* ScuExec takes SH2 cycles (it halves them itself) */
	bl	ScuExec
	ldaddr	x20, linecount_p
	bl	M68KSync
	ldaddr	x21, vblanklinecount_p
	bl	Vdp2HBlankOUT
	ldaddr	x22, maxlinecount_p
	bl	ScspExec
	ldr	x20, [x20] /* pointer to linecount */
	ldr	x21, [x21] /* pointer to vblanklinecount */
	ldr	x22, [x22] /* pointer to maxlinecount */
	ldr	w23, [x20] /* linecount */
	ldr	w21, [x21] /* vblanklinecount */
	ldr	w22, [x22] /* maxlinecount */
	add	w23, w23, #1
	str	wzr, [x28, #DL_DECILINECOUNT]
	cmp	w21, w23 /* linecount==vblanklinecount ? */
	b.eq	vblankin
	cmp	w22, w23 /* linecount==maxlinecount ? */
	b.eq	1f
	str	w23, [x20] /* linecount++ */
	b	nextline
1:
	bl	Vdp2VBlankOUT
nextline:
	/* finishline */
	/*const u32 usecinc = yabsys.DecilineUsec * 10;*/
	ldaddr	x3, decilineusec_p
	ldaddr	x24, UsecFrac_p
	ldaddr	x8, yabsys_timing_bits
	ldaddr	x9, yabsys_timing_mask
	ldr	x3, [x3] /* pointer to decilineusec */
	ldr	x24, [x24] /* pointer to usecfrac */
	ldr	w19, [x8] /* yabsys_timing_bits */
	ldr	w3, [x3] /* decilineusec */
	ldr	w0, [x24] /* usecfrac */
	ldr	w21, [x9] /* yabsys_timing_mask */
	add	w0, w0, w3, lsl #3 /* UsecFrac += yabsys.DecilineUsec * 8 */
	add	w0, w0, w3, lsl #1 /* UsecFrac += yabsys.DecilineUsec * 2 */
	str	w0, [x24]
	lsr	w0, w0, w19
	bl	SmpcExec
	/* SmpcExec may modify UsecFrac; must reload it */
	ldr	w25, [x24] /* usecfrac */
	lsr	w0, w25, w19
	and	w25, w25, w21
	bl	Cs2Exec
	str	w25, [x24] /* usecfrac */
	ldaddr	x8, saved_centicycles
	ldr	w1, [x28, #DL_M68KCENTICYCLES]
	ldr	w2, [x8]
	ldr	w0, [x28, #DL_M68KCYCLES]
	add	w2, w2, w1
	cmp	w2, #100
	b.lo	2f
	sub	w2, w2, #100
	add	w0, w0, #1
2:
	str	w2, [x8] /* saved_centicycles */
	bl	M68KExec
	ldr	x30, [x28, #DL_MASTER_IP]
	eor	w1, w22, w23 /* linecount==maxlinecount ? */
	cbnz	w1, newline
nextframe:
	str	w1, [x20] /* linecount=0 */
	bl	M68KSync
	ldr	w2, [x28, #DL_RCCOUNT]
	add	x3, x28, #DL_RESTORE_CANDIDATE
	ldaddr	x0, invalidate_count
	add	w2, w2, #1
	and	w2, w2, #0x3f
	str	w2, [x28, #DL_RCCOUNT]
	ldr	w19, [x3, w2, uxtw #2]
	str	wzr, [x0] /* invalidate_count=0 */
	cbnz	w19, .A5
.A4:
	ldp	x19, x20, [sp, #16]
	ldp	x21, x22, [sp, #32]
	ldp	x23, x24, [sp, #48]
	ldp	x25, x26, [sp, #64]
	ldp	x27, x28, [sp, #80]
	ldp	x29, x30, [sp], #96
	ret
.A5:
	/* Move 'dirty' blocks to the 'clean' list */
	lsl	w20, w2, #5
	str	wzr, [x3, w2, uxtw #2]
.A6:
	tbz	w19, #0, 3f
	mov	w0, w20
	bl	clean_blocks
3:
	lsr	w19, w19, #1
	add	w20, w20, #1
	tst	w20, #31
	b.ne	.A6
	b	.A4
vblankin:
	str	w23, [x20] /* linecount++ */
	bl	SmpcINTBACKEnd
	bl	Vdp2VBlankIN
	bl	CheatDoPatches
	b	nextline
	.size	cc_interrupt_master, .-cc_interrupt_master

/* First time through an external branch: w0 = virtual target address,
   x1 = the branch instruction to link */
	.align	2
	.global	dyna_linker
	.type	dyna_linker, %function
dyna_linker:
	bl	DynarecLink
	br	x0
	.size	dyna_linker, .-dyna_linker

/* Jump to the virtual address in a register (jump_vaddr_reg in
   assem_arm64.c): w0 = address (+1 for the slave), looked up in hash_table
   (four 8-byte words per bin: vaddr, address, vaddr, address) */
.macro jump_vaddr_reg n, r
	.global	jump_vaddr_r\n\()_master
	.type	jump_vaddr_r\n\()_master, %function
jump_vaddr_r\n\()_master:
	mov	w0, \r
	b	jump_vaddr
	.size	jump_vaddr_r\n\()_master, .-jump_vaddr_r\n\()_master
	.global	jump_vaddr_r\n\()_slave
	.type	jump_vaddr_r\n\()_slave, %function
jump_vaddr_r\n\()_slave:
	mov	w0, \r
	b	jump_vaddr_slave
	.size	jump_vaddr_r\n\()_slave, .-jump_vaddr_r\n\()_slave
.endm
	jump_vaddr_reg 0, w0
	jump_vaddr_reg 1, w1
	jump_vaddr_reg 2, w2
	jump_vaddr_reg 3, w3
	jump_vaddr_reg 4, w19
	jump_vaddr_reg 5, w20
	jump_vaddr_reg 6, w21
	jump_vaddr_reg 7, w22
	jump_vaddr_reg 8, w23
	jump_vaddr_reg 9, w24
	jump_vaddr_reg 12, w26

	.global	jump_vaddr_slave
	.type	jump_vaddr_slave, %function
jump_vaddr_slave:
	add	w0, w0, #1
	.size	jump_vaddr_slave, .-jump_vaddr_slave
	.global	jump_vaddr
	.type	jump_vaddr, %function
jump_vaddr:
	ldaddr	x1, hash_table
	eor	w2, w0, w0, lsr #16
	and	w2, w2, #0xffff
	add	x1, x1, x2, lsl #5
	ldp	x2, x3, [x1]
	cmp	x2, x0
	b.ne	1f
	br	x3
1:
	ldp	x2, x3, [x1, #16]
	cmp	x2, x0
	b.ne	2f
	br	x3
2:
	bl	get_addr
	br	x0
	.size	jump_vaddr, .-jump_vaddr

/* Called (bl) from a dirty entry stub: w0 = virtual address, x1 = source,
   x2 = copy, w3 = length. Returns if the code is unchanged, else goes to
   the address's current block. */
	.align	2
	.global	verify_code
	.type	verify_code, %function
verify_code:
	mov	w19, w0
	mov	x20, x30
	mov	x0, x1
	mov	x1, x2
	mov	w2, w3
	bl	memcmp
	cbnz	w0, 1f
	mov	x30, x20
	ret
1:
	mov	w0, w19
	bl	get_addr
	br	x0
	.size	verify_code, .-verify_code

/* DIV1: w0 = Rn, w1 = Rm, w2 = SR; returns Rn in w0 and SR in w2, keeps w1 */
	.align	2
	.global	div1
	.type	div1, %function
div1:
	stp	x1, x30, [sp, #-16]!
	bl	DynarecDiv1
	ldp	x1, x30, [sp], #16
	lsr	x2, x0, #32
	mov	w0, w0
	ret
	.size	div1, .-div1

/* MAC.L/MAC.W: w0 = MACL, w1 = MACH, w19 = SR, x20 = @Rm, x21 = @Rn;
   returns MACL in w0 and MACH in w1, x20 and x21 incremented */
.macro mac name, func
	.align	2
	.global	\name
	.type	\name, %function
\name:
	sub	sp, sp, #32
	stp	w20, w21, [sp] /* addr[0]=Rm, addr[1]=Rn (u32) */
	stp	x19, x30, [sp, #16]
	mov	w2, w19
	mov	x3, sp
	bl	\func
	ldp	x19, x30, [sp, #16]
	ldp	w20, w21, [sp]
	add	sp, sp, #32
	lsr	x1, x0, #32
	mov	w0, w0
	ret
	.size	\name, .-\name
.endm
	mac	macl, DynarecMacL
	mac	macw, DynarecMacW

/* BIOS functions (emulated BIOS): w0 = PC */
	.align	2
	.global	master_handle_bios
	.type	master_handle_bios, %function
master_handle_bios:
	str	w0, [x28, #DL_MASTER_PC]
	ldaddr	x1, MSH2
	str	w25, [x28, #DL_MASTER_CC]
	str	x30, [x28, #DL_MASTER_IP]
	ldr	x0, [x1] /* MSH2 */
	bl	BiosHandleFunc
	ldr	x30, [x28, #DL_MASTER_IP]
	ldr	w25, [x28, #DL_MASTER_CC]
	ret
	.size	master_handle_bios, .-master_handle_bios

	.align	2
	.global	slave_handle_bios
	.type	slave_handle_bios, %function
slave_handle_bios:
	str	w0, [x28, #DL_SLAVE_PC]
	ldaddr	x1, SSH2
	str	w25, [x28, #DL_SLAVE_CC]
	str	x30, [x28, #DL_SLAVE_IP]
	ldr	x0, [x1] /* SSH2 */
	bl	BiosHandleFunc
	ldr	x30, [x28, #DL_SLAVE_IP]
	ldr	w25, [x28, #DL_SLAVE_CC]
	ret
	.size	slave_handle_bios, .-slave_handle_bios

	.section	.note.GNU-stack,"",%progbits
