/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Yabause - assem_arm64.c                                               *
 *   AArch64 code generator for the SH2 recompiler, after assem_arm.c      *
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

/* Layout of dynarec_local (linkage_arm64.s); x28 points to it in generated
   code:
     0    scratch: the caller-save registers around calls (x0-x3), 32 and 40
          temporaries of the stubs
     64   master_reg[22], master_cc, master_pc, master_ip (8 bytes)
     168  slave_reg[22], slave_cc, slave_pc, slave_ip
     272  timing words of the frame loop
     304  mini_ht_master[32][2], mini_ht_slave[32][2] (pointers)
     1328 restore_candidate[512]
     1840 memory_map[1048576] (64-bit entries) */
#define DL_STUBTEMP1 32
#define DL_STUBTEMP2 40

extern u8 dynarec_local[];
extern u64 memory_map[1048576];
ALIGNED(16) extern pointer mini_ht_master[32][2];
ALIGNED(16) extern pointer mini_ht_slave[32][2];
ALIGNED(4) extern u8 restore_candidate[512];

#define DL ((pointer)dynarec_local)
// Index of memory_map[0] in 8-byte words from dynarec_local (the MMREG value)
#define MM_INDEX (((pointer)memory_map-DL)>>3)

void WriteInvalidateLong(u32 addr, u32 val);
void WriteInvalidateWord(u32 addr, u32 val);
void WriteInvalidateByte(u32 addr, u32 val);
void WriteInvalidateByteSwapped(u32 addr, u32 val);

void jump_vaddr_r0_master();
void jump_vaddr_r1_master();
void jump_vaddr_r2_master();
void jump_vaddr_r3_master();
void jump_vaddr_r4_master();
void jump_vaddr_r5_master();
void jump_vaddr_r6_master();
void jump_vaddr_r7_master();
void jump_vaddr_r8_master();
void jump_vaddr_r9_master();
void jump_vaddr_r12_master();
void jump_vaddr_r0_slave();
void jump_vaddr_r1_slave();
void jump_vaddr_r2_slave();
void jump_vaddr_r3_slave();
void jump_vaddr_r4_slave();
void jump_vaddr_r5_slave();
void jump_vaddr_r6_slave();
void jump_vaddr_r7_slave();
void jump_vaddr_r8_slave();
void jump_vaddr_r9_slave();
void jump_vaddr_r12_slave();

const pointer jump_vaddr_reg[2][16] = {
  {
    (pointer)jump_vaddr_r0_master,
    (pointer)jump_vaddr_r1_master,
    (pointer)jump_vaddr_r2_master,
    (pointer)jump_vaddr_r3_master,
    (pointer)jump_vaddr_r4_master,
    (pointer)jump_vaddr_r5_master,
    (pointer)jump_vaddr_r6_master,
    (pointer)jump_vaddr_r7_master,
    (pointer)jump_vaddr_r8_master,
    (pointer)jump_vaddr_r9_master,
    0,
    0,
    (pointer)jump_vaddr_r12_master,
    0,
    0,
    0
  },{
    (pointer)jump_vaddr_r0_slave,
    (pointer)jump_vaddr_r1_slave,
    (pointer)jump_vaddr_r2_slave,
    (pointer)jump_vaddr_r3_slave,
    (pointer)jump_vaddr_r4_slave,
    (pointer)jump_vaddr_r5_slave,
    (pointer)jump_vaddr_r6_slave,
    (pointer)jump_vaddr_r7_slave,
    (pointer)jump_vaddr_r8_slave,
    (pointer)jump_vaddr_r9_slave,
    0,
    0,
    (pointer)jump_vaddr_r12_slave,
    0,
    0,
    0
  }
};

u32 needs_clear_cache[1<<(TARGET_SIZE_2-17)];

/* Host registers */

// Allocator number -> AArch64 register (see assem_arm64.h); 15 is x17, a
// scratch register for the stubs
static const u8 a64reg[16]={0,1,2,3,19,20,21,22,23,24,25,28,26,31,16,17};
static u32 HR(int r)
{
  assert(r>=0&&r<16&&r!=13);
  return a64reg[r];
}
// Scratch registers of the code generator, used within one emit_* function
#define P_IMM 17 // immediates that need a register
#define P_T   15 // flag results, addresses of byte accesses
#define P_T2  14
#define P_TMP 16 // HOST_TEMPREG
#define P_FP  28
#define P_LR  30
#define ZR    31
#define SF    0x80000000 // 64-bit operation

static int is_callersave(int hr)
{
  return hr<4;
}

/* Condition codes */
#define C_EQ 0
#define C_NE 1
#define C_CS 2
#define C_CC 3
#define C_MI 4
#define C_PL 5
#define C_VS 6
#define C_VC 7
#define C_HI 8
#define C_LS 9
#define C_GE 10
#define C_LT 11
#define C_GT 12
#define C_LE 13

/* Linker */

void set_jump_target(pointer addr,pointer target)
{
  u32 *ptr=(u32 *)addr;
  u32 insn=*ptr;
  s64 offset=(s64)target-(s64)addr;
  assert((offset&3)==0);
  if((insn&0x7C000000)==0x14000000) { // B, BL
    assert(offset>=-134217728&&offset<134217728);
    *ptr=(insn&0xFC000000)|((offset>>2)&0x3FFFFFF);
  }
  else if((insn&0xFF000010)==0x54000000) { // B.cond
    assert(offset>=-1048576&&offset<1048576);
    *ptr=(insn&0xFF00001F)|(((offset>>2)&0x7FFFF)<<5);
  }
  else {
    assert((insn&0x9F000000)==0x10000000); // ADR (do_miniht_insert)
    assert(offset>=-1048576&&offset<1048576);
    *ptr=(insn&0x9F00001F)|((offset&3)<<29)|(((offset>>2)&0x7FFFF)<<5);
  }
}

// Target of a B, BL or B.cond
static pointer branch_target(pointer addr)
{
  u32 insn=*(u32 *)addr;
  if((insn&0x7C000000)==0x14000000)
    return addr+((s64)((s32)(insn<<6))>>4);
  assert((insn&0xFF000010)==0x54000000);
  return addr+(((s64)((s32)(insn<<8))>>13)<<2);
}

void set_jump_target_fillslot(pointer addr,pointer target,int copy)
{
  set_jump_target(addr,target);
}

/* Literal pool (not used: constants are built with movz/movk) */
void literal_pool(int n) {}
void literal_pool_jumpover(int n) {}

/* External jump stub (emit_extjump):
     movz w0,#target; movk w0,#target>>16,lsl #16
     adr  x1,<branch>
     b    dyna_linker
   The ADR gives the branch that is linked to the target. */
static pointer extjump_branch(void *stub)
{
  u32 insn=((u32 *)stub)[2];
  s64 offset;
  assert((insn&0x9F00001F)==0x10000001);
  offset=(s64)((s32)(((insn>>5)&0x7FFFF)<<13))>>11|((insn>>29)&3);
  return (pointer)stub+8+offset;
}

void *kill_pointer(void *stub)
{
  pointer branch=extjump_branch(stub);
  set_jump_target(branch,(pointer)stub);
  return (void *)branch;
}

pointer get_pointer(void *stub)
{
  return branch_target(extjump_branch(stub));
}

/* Dirty entry stub (do_dirty_stub):
     0 ldr  x1,source      (literal)
     1 ldr  x2,copy        (literal)
     2 movz w3,#len
     3 movz w0,#vaddr
     4 movk w0,#vaddr>>16,lsl #16
     5 bl   verify_code
     6 b    clean
       [nop]
       .quad source, copy
   clean: */
static u64 *dirty_stub_literals(u32 *ptr)
{
  u32 insn=ptr[0];
  assert((insn&0xFF00001F)==0x58000001); // ldr x1,literal
  return (u64 *)((pointer)ptr+(((s64)((s32)(insn<<8))>>13)<<2));
}

// Find the "clean" entry point from a "dirty" entry point
// by skipping past the call to verify_code
pointer get_clean_addr(pointer addr)
{
  u32 *ptr=(u32 *)addr;
  pointer clean;
  assert((ptr[5]&0xFC000000)==0x94000000); // bl verify_code
  clean=branch_target((pointer)(ptr+6));
  if((*(u32 *)clean&0xFC000000)==0x14000000) return branch_target(clean); // follow jump
  return clean;
}

int verify_dirty(pointer addr)
{
  u32 *ptr=(u32 *)addr;
  u64 *lit=dirty_stub_literals(ptr);
  u32 len=(ptr[2]>>5)&0xFFFF;
  assert((ptr[5]&0xFC000000)==0x94000000); // bl instruction
  return !memcmp((void *)lit[0],(void *)lit[1],len);
}

// This doesn't necessarily find all clean entry points, just
// guarantees that it's not dirty
int isclean(pointer addr)
{
  u32 *ptr=(u32 *)addr;
  if((ptr[0]&0xFF00001F)!=0x58000001) return 1;
  if((ptr[5]&0xFC000000)!=0x94000000) return 1; // bl instruction
  if(branch_target((pointer)(ptr+5))==(pointer)verify_code) return 0;
  return 1;
}

void get_bounds(pointer addr,pointer *start,pointer *end)
{
  u32 *ptr=(u32 *)addr;
  u64 *lit=dirty_stub_literals(ptr);
  assert((ptr[5]&0xFC000000)==0x94000000); // bl instruction
  *start=lit[0];
  *end=lit[0]+((ptr[2]>>5)&0xFFFF);
}

/* Register allocation */

// Note: registers are allocated clean (unmodified state)
// if you intend to modify the register, you must call dirty_reg().
void alloc_reg(struct regstat *cur,int i,signed char reg)
{
  int r,hr;
  int preferred_reg = (reg&7);
  if(reg==CCREG) preferred_reg=HOST_CCREG;
  if(reg==PTEMP) preferred_reg=12;

  // Don't allocate unused registers
  if((cur->u>>reg)&1) return;

  // see if it's already allocated
  for(hr=0;hr<HOST_REGS;hr++)
  {
    if(cur->regmap[hr]==reg) return;
  }

  // Keep the same mapping if the register was already allocated in a loop
  preferred_reg = loop_reg(i,reg,preferred_reg);

  // Try to allocate the preferred register
  if(cur->regmap[preferred_reg]==-1) {
    cur->regmap[preferred_reg]=reg;
    cur->dirty&=~(1<<preferred_reg);
    cur->isdoingcp&=~(1<<preferred_reg);
    return;
  }
  r=cur->regmap[preferred_reg];
  if(r<64&&((cur->u>>r)&1)) {
    cur->regmap[preferred_reg]=reg;
    cur->dirty&=~(1<<preferred_reg);
    cur->isdoingcp&=~(1<<preferred_reg);
    return;
  }

  // Clear any unneeded registers
  // We try to keep the mapping consistent, if possible, because it
  // makes branches easier (especially loops).  So we try to allocate
  // first (see above) before removing old mappings.  If this is not
  // possible then go ahead and clear out the registers that are no
  // longer needed.
  for(hr=0;hr<HOST_REGS;hr++)
  {
    r=cur->regmap[hr];
    if(r>=0) {
      if((cur->u>>r)&1)
        if(i==0||(unneeded_reg[i-1]>>r)&1) {cur->regmap[hr]=-1;break;}
    }
  }
  // Try to allocate any available register, but prefer
  // registers that have not been used recently.
  if(i>0) {
    for(hr=0;hr<HOST_REGS;hr++) {
      if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
        if(regs[i-1].regmap[hr]!=rs1[i-1]&&regs[i-1].regmap[hr]!=rs2[i-1]&&regs[i-1].regmap[hr]!=rt1[i-1]&&regs[i-1].regmap[hr]!=rt2[i-1]) {
          cur->regmap[hr]=reg;
          cur->dirty&=~(1<<hr);
          cur->isdoingcp&=~(1<<hr);
          return;
        }
      }
    }
  }
  // Try to allocate any available register
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
      cur->regmap[hr]=reg;
      cur->dirty&=~(1<<hr);
      cur->isdoingcp&=~(1<<hr);
      return;
    }
  }

  // Ok, now we have to evict someone
  // Pick a register we hopefully won't need soon
  unsigned char hsn[MAXREG+1];
  memset(hsn,10,sizeof(hsn));
  int j;
  lsn(hsn,i,&preferred_reg);
  if(i>0) {
    // Don't evict the cycle count at entry points, otherwise the entry
    // stub will have to write it.
    if(bt[i]&&hsn[CCREG]>2) hsn[CCREG]=2;
    if(i>1&&hsn[CCREG]>2&&(itype[i-2]==RJUMP||itype[i-2]==UJUMP||itype[i-2]==CJUMP||itype[i-2]==SJUMP)) hsn[CCREG]=2;
    for(j=10;j>=3;j--)
    {
      // Alloc preferred register if available
      if(hsn[r=cur->regmap[preferred_reg]&63]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          // Evict both parts of a 64-bit register
          if((cur->regmap[hr]&63)==r) {
            cur->regmap[hr]=-1;
            cur->dirty&=~(1<<hr);
            cur->isdoingcp&=~(1<<hr);
          }
        }
        cur->regmap[preferred_reg]=reg;
        return;
      }
      for(r=0;r<=MAXREG;r++)
      {
        if(hsn[r]==j&&r!=rs1[i-1]&&r!=rs2[i-1]&&r!=rt1[i-1]&&r!=rt2[i-1]) {
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||j<hsn[CCREG]) {
              if(cur->regmap[hr]==r+64) {
                cur->regmap[hr]=reg;
                cur->dirty&=~(1<<hr);
                cur->isdoingcp&=~(1<<hr);
                return;
              }
            }
          }
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||j<hsn[CCREG]) {
              if(cur->regmap[hr]==r) {
                cur->regmap[hr]=reg;
                cur->dirty&=~(1<<hr);
                cur->isdoingcp&=~(1<<hr);
                return;
              }
            }
          }
        }
      }
    }
  }
  for(j=10;j>=0;j--)
  {
    for(r=0;r<=MAXREG;r++)
    {
      if(hsn[r]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r+64) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isdoingcp&=~(1<<hr);
            return;
          }
        }
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isdoingcp&=~(1<<hr);
            return;
          }
        }
      }
    }
  }
  printf("This shouldn't happen (alloc_reg)");exit(1);
}

// Allocate a temporary register.  This is done without regard to
// dirty status or whether the register we request is on the unneeded list
// Note: This will only allocate one register, even if called multiple times
void alloc_reg_temp(struct regstat *cur,int i,signed char reg)
{
  int r,hr;
  int preferred_reg = -1;

  // see if it's already allocated
  for(hr=0;hr<HOST_REGS;hr++)
  {
    if(hr!=EXCLUDE_REG&&cur->regmap[hr]==reg) return;
  }

  // Try to allocate any available register
  for(hr=HOST_REGS-1;hr>=0;hr--) {
    if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
      cur->regmap[hr]=reg;
      cur->dirty&=~(1<<hr);
      cur->isdoingcp&=~(1<<hr);
      return;
    }
  }

  // Find an unneeded register
  for(hr=HOST_REGS-1;hr>=0;hr--)
  {
    r=cur->regmap[hr];
    if(r>=0) {
      if((cur->u>>r)&1) {
        if(i==0||((unneeded_reg[i-1]>>r)&1)) {
          cur->regmap[hr]=reg;
          cur->dirty&=~(1<<hr);
          cur->isdoingcp&=~(1<<hr);
          return;
        }
      }
    }
  }

  // Ok, now we have to evict someone
  // Pick a register we hopefully won't need soon
  unsigned char hsn[MAXREG+1];
  memset(hsn,10,sizeof(hsn));
  int j;
  lsn(hsn,i,&preferred_reg);
  if(i>0) {
    // Don't evict the cycle count at entry points, otherwise the entry
    // stub will have to write it.
    if(bt[i]&&hsn[CCREG]>2) hsn[CCREG]=2;
    if(i>1&&hsn[CCREG]>2&&(itype[i-2]==RJUMP||itype[i-2]==UJUMP||itype[i-2]==CJUMP||itype[i-2]==SJUMP)) hsn[CCREG]=2;
    for(j=10;j>=3;j--)
    {
      for(r=0;r<=MAXREG;r++)
      {
        if(hsn[r]==j&&r!=rs1[i-1]&&r!=rs2[i-1]&&r!=rt1[i-1]&&r!=rt2[i-1]) {
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||j<hsn[CCREG]) {
              if(cur->regmap[hr]==r+64) {
                cur->regmap[hr]=reg;
                cur->dirty&=~(1<<hr);
                cur->isdoingcp&=~(1<<hr);
                return;
              }
            }
          }
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||j<hsn[CCREG]) {
              if(cur->regmap[hr]==r) {
                cur->regmap[hr]=reg;
                cur->dirty&=~(1<<hr);
                cur->isdoingcp&=~(1<<hr);
                return;
              }
            }
          }
        }
      }
    }
  }
  for(j=10;j>=0;j--)
  {
    for(r=0;r<=MAXREG;r++)
    {
      if(hsn[r]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r+64) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isdoingcp&=~(1<<hr);
            return;
          }
        }
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isdoingcp&=~(1<<hr);
            return;
          }
        }
      }
    }
  }
  printf("This shouldn't happen");exit(1);
}
// Allocate a specific host register (the numbering of assem_arm.c).
void alloc_arm_reg(struct regstat *cur,int i,signed char reg,char hr)
{
  int n;
  u32 dirty=0;

  // see if it's already allocated (and dealloc it)
  for(n=0;n<HOST_REGS;n++)
  {
    if(n!=EXCLUDE_REG&&cur->regmap[n]==reg) {
      dirty=(cur->dirty>>n)&1;
      cur->regmap[n]=-1;
    }
  }

  cur->regmap[hr]=reg;
  cur->dirty&=~(1<<hr);
  cur->dirty|=dirty<<hr;
  cur->isdoingcp&=~(1<<hr);
}

// Alloc cycle count into dedicated register
void alloc_cc(struct regstat *cur,int i)
{
  alloc_arm_reg(cur,i,CCREG,HOST_CCREG);
}

/* Assembler */

char regname[16][4] = {
 "w0",
 "w1",
 "w2",
 "w3",
 "w19",
 "w20",
 "w21",
 "w22",
 "w23",
 "w24",
 "w25",
 "x28",
 "w26",
 "?",
 "w16",
 "w17"};

void output_w32(u32 word)
{
  *((u32 *)out)=word;
  out+=4;
}

/* Instruction encodings, on AArch64 register numbers */

static void a_addsub(u32 op,int rd,int rn,int rm,int shift,int amount)
{
  // op: 0x0B000000 add, 0x2B000000 adds, 0x4B000000 sub, 0x6B000000 subs
  output_w32(op|(shift<<22)|(rm<<16)|(amount<<10)|(rn<<5)|rd);
}
static void a_addsub_imm(u32 op,int rd,int rn,u32 imm12,int lsl12)
{
  // op: 0x11000000 add, 0x31000000 adds, 0x51000000 sub, 0x71000000 subs
  assert(imm12<4096);
  output_w32(op|(lsl12<<22)|(imm12<<10)|(rn<<5)|rd);
}
static void a_logic(u32 op,int rd,int rn,int rm,int shift,int amount)
{
  // op: 0x0A000000 and, 0x0A200000 bic, 0x2A000000 orr, 0x2A200000 orn,
  //     0x4A000000 eor, 0x6A000000 ands
  output_w32(op|(shift<<22)|(rm<<16)|(amount<<10)|(rn<<5)|rd);
}
static void a_logic_imm(u32 op,int rd,int rn,u32 enc)
{
  // op: 0x12000000 and, 0x32000000 orr, 0x52000000 eor, 0x72000000 ands
  output_w32(op|enc|(rn<<5)|rd);
}
static void a_movw(u32 op,int rd,u32 imm16,int hw)
{
  // op: 0x52800000 movz, 0x12800000 movn, 0x72800000 movk
  assert(imm16<65536);
  output_w32(op|(hw<<21)|(imm16<<5)|rd);
}
static void a_bfm(u32 op,int rd,int rn,int immr,int imms)
{
  // op: 0x13000000 sbfm, 0x33000000 bfm, 0x53000000 ubfm (32-bit)
  output_w32(op|(immr<<16)|(imms<<10)|(rn<<5)|rd);
}
#define A_SBFM 0x13000000
#define A_BFM  0x33000000
#define A_UBFM 0x53000000
static void a_bfi(int rd,int rn,int lsb,int width)
{
  a_bfm(A_BFM,rd,rn,(32-lsb)&31,width-1);
}
static void a_extr(int rd,int rn,int rm,int lsb)
{
  output_w32(0x13800000|(rm<<16)|(lsb<<10)|(rn<<5)|rd);
}
static void a_csinc(int rd,int rn,int rm,int cond)
{
  output_w32(0x1A800400|(rm<<16)|(cond<<12)|(rn<<5)|rd);
}
static void a_cset(int rd,int cond)
{
  a_csinc(rd,ZR,ZR,cond^1);
}
static void a_mov64(int rd,int rm)
{
  a_logic(SF|0x2A000000,rd,ZR,rm,0,0);
}
// T bit of SR from a condition
static void a_set_t(int sr,int cond)
{
  a_cset(P_T,cond);
  a_bfi(sr,P_T,0,1);
}

// Logical immediate encoding (N:immr:imms) for 32-bit operations
static int enc_logimm32(u32 imm,u32 *enc)
{
  int size,ones,r;
  u32 mask,elem,base;
  if(imm==0||imm==0xFFFFFFFF) return 0;
  // smallest period of the pattern
  size=32;
  while(size>2) {
    int half=size>>1;
    u32 hmask=(1u<<half)-1;
    if(((imm>>half)&hmask)!=(imm&hmask)) break;
    size=half;
  }
  mask=(size==32)?0xFFFFFFFF:((1u<<size)-1);
  elem=imm&mask;
  ones=__builtin_popcount(elem);
  base=(ones==32)?0xFFFFFFFF:((1u<<ones)-1);
  for(r=0;r<size;r++) {
    u32 rot=r?(((base>>r)|(base<<(size-r)))&mask):base;
    if(rot==elem) {
      *enc=(r<<16)|(((~(2*size-1)&0x3f)|(ones-1))<<10);
      return 1;
    }
  }
  return 0;
}
// The same for 64-bit operations (only the patterns used here: N=1)
static int enc_logimm64(u64 imm,u32 *enc)
{
  int ones,r;
  u64 base;
  if(imm==0||imm==~0ULL) return 0;
  ones=__builtin_popcountll(imm);
  base=(1ULL<<ones)-1;
  for(r=0;r<64;r++) {
    u64 rot=r?((base>>r)|(base<<(64-r))):base;
    if(rot==imm) {
      *enc=(1<<22)|(r<<16)|((ones-1)<<10);
      return 1;
    }
  }
  return 0;
}

static void a_movimm32(int rd,u32 imm)
{
  u32 enc;
  if(imm<65536) a_movw(0x52800000,rd,imm,0);
  else if((imm&0xFFFF)==0) a_movw(0x52800000,rd,imm>>16,1);
  else if(~imm<65536) a_movw(0x12800000,rd,~imm,0);
  else if((~imm&0xFFFF)==0) a_movw(0x12800000,rd,(~imm)>>16,1);
  else if(enc_logimm32(imm,&enc)) a_logic_imm(0x32000000,rd,ZR,enc);
  else {
    a_movw(0x52800000,rd,imm&0xFFFF,0);
    a_movw(0x72800000,rd,imm>>16,1);
  }
}
// Any 64-bit constant, with movz/movn and movk
static void a_movconst64(int rd,u64 imm)
{
  int hw,first=1,inv;
  if(imm<=0xFFFFFFFF) {a_movimm32(rd,imm);return;}
  // movn if more halfwords are 0xFFFF than 0
  inv=0;
  for(hw=0;hw<4;hw++) {
    u32 part=(imm>>(hw*16))&0xFFFF;
    if(part==0xFFFF) inv++;
    if(part==0) inv--;
  }
  for(hw=0;hw<4;hw++) {
    u32 part=(imm>>(hw*16))&0xFFFF;
    if(part==(inv>0?0xFFFF:0)) continue;
    if(first) a_movw(SF|(inv>0?0x12800000:0x52800000),rd,inv>0?part^0xFFFF:part,hw);
    else a_movw(SF|0x72800000,rd,part,hw);
    first=0;
  }
  if(first) a_movw(SF|(inv>0?0x12800000:0x52800000),rd,0,0);
}
// A host address: pc-relative if possible (the generated code doesn't move)
static void a_movimm64(int rd,u64 imm)
{
  s64 offset=(s64)imm-(s64)(pointer)out;
  if(imm<=0xFFFFFFFF) {a_movimm32(rd,imm);return;}
  if(offset>=-1048576&&offset<1048576) {
    // adr
    output_w32(0x10000000|((offset&3)<<29)|(((offset>>2)&0x7FFFF)<<5)|rd);
    return;
  }
  offset=(s64)(imm&~0xFFFULL)-(s64)((pointer)out&~0xFFFULL);
  if(offset>=-4294967296LL&&offset<4294967296LL) {
    // adrp, add
    offset>>=12;
    output_w32(0x90000000|((offset&3)<<29)|(((offset>>2)&0x7FFFF)<<5)|rd);
    if(imm&0xFFF) a_addsub_imm(SF|0x11000000,rd,rd,imm&0xFFF,0);
    return;
  }
  a_movconst64(rd,imm);
}

/* Loads and stores */
enum { LS_STRB, LS_STRH, LS_STRW, LS_STRX, LS_LDRB, LS_LDRH, LS_LDRW, LS_LDRX, LS_LDRSB, LS_LDRSH };
static const u32 ls_uimm[]={0x39000000,0x79000000,0xB9000000,0xF9000000,0x39400000,0x79400000,0xB9400000,0xF9400000,0x39C00000,0x79C00000};
static const u32 ls_unscaled[]={0x38000000,0x78000000,0xB8000000,0xF8000000,0x38400000,0x78400000,0xB8400000,0xF8400000,0x38C00000,0x78C00000};
static const u32 ls_reg[]={0x38206800,0x78206800,0xB8206800,0xF8206800,0x38606800,0x78606800,0xB8606800,0xF8606800,0x38E06800,0x78E06800};
static const int ls_size[]={0,1,2,3,0,1,2,3,0,1};

// [rn,#offset]
static void a_ldst(int op,int rt,int rn,s64 offset)
{
  int s=ls_size[op];
  if(offset>=0&&!(offset&((1<<s)-1))&&(offset>>s)<4096)
    output_w32(ls_uimm[op]|((offset>>s)<<10)|(rn<<5)|rt);
  else if(offset>=-256&&offset<256)
    output_w32(ls_unscaled[op]|((offset&0x1FF)<<12)|(rn<<5)|rt);
  else {
    assert(rt!=P_IMM&&rn!=P_IMM);
    a_movconst64(P_IMM,offset);
    output_w32(ls_reg[op]|(P_IMM<<16)|(rn<<5)|rt);
  }
}
// [rn,xm,lsl #size] (scaled) or [rn,xm]
static void a_ldst_reg(int op,int rt,int rn,int rm,int scaled)
{
  output_w32(ls_reg[op]|(rm<<16)|(scaled<<12)|(rn<<5)|rt);
}

/* Branches */

static int in_range26(pointer target)
{
  s64 offset=(s64)target-(s64)(pointer)out;
  return offset>=-134217728&&offset<134217728;
}
static int in_range19(pointer target)
{
  s64 offset=(s64)target-(s64)(pointer)out;
  return offset>=-1048576&&offset<1048576;
}
// imm26 field of a branch to addr (0: to be set with set_jump_target)
static u32 genjmp(pointer addr)
{
  if(addr<4) return 0;
  assert(in_range26(addr));
  return (((s64)addr-(s64)(pointer)out)>>2)&0x3FFFFFF;
}

void emit_call(pointer a)
{
  assem_debug("bl %lx\n",a);
  if(in_range26(a)) output_w32(0x94000000|genjmp(a));
  else {
    a_movimm64(P_TMP,a);
    output_w32(0xD63F0000|(P_TMP<<5)); // blr
  }
}
void emit_jmp(pointer a)
{
  assem_debug("b %lx\n",a);
  if(a<4||in_range26(a)) output_w32(0x14000000|genjmp(a));
  else {
    a_movimm64(P_TMP,a);
    output_w32(0xD61F0000|(P_TMP<<5)); // br
  }
}
static void emit_jcc(int cond,pointer a)
{
  if(a<4) output_w32(0x54000000|cond); // set_jump_target fills it in
  else if(in_range19(a)) output_w32(0x54000000|((((s64)a-(s64)(pointer)out)>>2&0x7FFFF)<<5)|cond);
  else {
    pointer skip=(pointer)out;
    output_w32(0x54000000|(cond^1));
    emit_jmp(a);
    set_jump_target(skip,(pointer)out);
  }
}
void emit_jne(pointer a) { assem_debug("bne %lx\n",a); emit_jcc(C_NE,a); }
void emit_jeq(pointer a) { assem_debug("beq %lx\n",a); emit_jcc(C_EQ,a); }
void emit_js(pointer a)  { assem_debug("bmi %lx\n",a); emit_jcc(C_MI,a); }
void emit_jns(pointer a) { assem_debug("bpl %lx\n",a); emit_jcc(C_PL,a); }
void emit_jl(pointer a)  { assem_debug("blt %lx\n",a); emit_jcc(C_LT,a); }
void emit_jge(pointer a) { assem_debug("bge %lx\n",a); emit_jcc(C_GE,a); }
void emit_jno(pointer a) { assem_debug("bvc %lx\n",a); emit_jcc(C_VC,a); }
void emit_jc(pointer a)  { assem_debug("bcs %lx\n",a); emit_jcc(C_CS,a); }
void emit_jcc_(pointer a) { assem_debug("bcc %lx\n",a); emit_jcc(C_CC,a); }

void emit_jmpreg(unsigned int r)
{
  assem_debug("br %s\n",regname[r]);
  output_w32(0xD61F0000|(HR(r)<<5));
}

/* Register-register operations (32-bit: the guest registers) */

void emit_mov(int rs,int rt)
{
  assem_debug("mov %s,%s\n",regname[rt],regname[rs]);
  // 64-bit: a register can hold a host address (constant load addresses)
  a_mov64(HR(rt),HR(rs));
}

void emit_add(int rs1,int rs2,int rt)
{
  assem_debug("add %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  a_addsub(0x0B000000,HR(rt),HR(rs1),HR(rs2),0,0);
}

void emit_adds(int rs1,int rs2,int rt)
{
  assem_debug("adds %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  a_addsub(0x2B000000,HR(rt),HR(rs1),HR(rs2),0,0);
}

void emit_adc(int rs1,int rs2,int rt)
{
  assem_debug("adc %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0x1A000000|(HR(rs2)<<16)|(HR(rs1)<<5)|HR(rt));
}

void emit_adcs(int rs1,int rs2,int rt)
{
  assem_debug("adcs %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0x3A000000|(HR(rs2)<<16)|(HR(rs1)<<5)|HR(rt));
}

void emit_sbc(int rs1,int rs2,int rt)
{
  assem_debug("sbc %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0x5A000000|(HR(rs2)<<16)|(HR(rs1)<<5)|HR(rt));
}

void emit_sbcs(int rs1,int rs2,int rt)
{
  assem_debug("sbcs %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0x7A000000|(HR(rs2)<<16)|(HR(rs1)<<5)|HR(rt));
}

void emit_neg(int rs, int rt)
{
  assem_debug("neg %s,%s\n",regname[rt],regname[rs]);
  a_addsub(0x4B000000,HR(rt),ZR,HR(rs),0,0);
}

void emit_negs(int rs, int rt)
{
  assem_debug("negs %s,%s\n",regname[rt],regname[rs]);
  a_addsub(0x6B000000,HR(rt),ZR,HR(rs),0,0);
}

void emit_sub(int rs1,int rs2,int rt)
{
  assem_debug("sub %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  a_addsub(0x4B000000,HR(rt),HR(rs1),HR(rs2),0,0);
}

void emit_subs(int rs1,int rs2,int rt)
{
  assem_debug("subs %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  a_addsub(0x6B000000,HR(rt),HR(rs1),HR(rs2),0,0);
}

void emit_zeroreg(int rt)
{
  assem_debug("mov %s,#0\n",regname[rt]);
  a_movw(0x52800000,HR(rt),0,0);
}

void emit_movimm(u32 imm,unsigned int rt)
{
  assem_debug("mov %s,#%x\n",regname[rt],imm);
  a_movimm32(HR(rt),imm);
}
// A host address (64-bit)
void emit_movimm_ptr(pointer imm,unsigned int rt)
{
  assem_debug("mov %s,#%lx\n",regname[rt],imm);
  a_movimm64(HR(rt),imm);
}

// The guest register r in memory (master_reg/slave_reg), or the cycle count
static pointer reg_addr(int r)
{
  if(r==CCREG) return slave?(pointer)&slave_cc:(pointer)&master_cc;
  return (slave?(pointer)slave_reg:(pointer)master_reg)+(r<<2);
}

void emit_loadreg(int r, int hr)
{
  if(r==MMREG)
    emit_movimm(MM_INDEX,hr);
  else {
    pointer offset=reg_addr(r)-DL;
    assem_debug("ldr %s,fp+%d\n",regname[hr],(int)offset);
    a_ldst(LS_LDRW,HR(hr),P_FP,offset);
  }
}
void emit_storereg(int r, int hr)
{
  pointer offset=reg_addr(r)-DL;
  assem_debug("str %s,fp+%d\n",regname[hr],(int)offset);
  a_ldst(LS_STRW,HR(hr),P_FP,offset);
}

void emit_test(int rs, int rt)
{
  assem_debug("tst %s,%s\n",regname[rs],regname[rt]);
  a_logic(0x6A000000,ZR,HR(rs),HR(rt),0,0);
}

static void a_testimm(int rs,u32 imm)
{
  u32 enc;
  if(enc_logimm32(imm,&enc)) a_logic_imm(0x72000000,ZR,rs,enc);
  else {
    a_movimm32(P_IMM,imm);
    a_logic(0x6A000000,ZR,rs,P_IMM,0,0);
  }
}
void emit_testimm(int rs,int imm)
{
  assem_debug("tst %s,#%x\n",regname[rs],imm);
  a_testimm(HR(rs),imm);
}

void emit_not(int rs,int rt)
{
  assem_debug("mvn %s,%s\n",regname[rt],regname[rs]);
  a_logic(0x2A200000,HR(rt),ZR,HR(rs),0,0);
}

void emit_and(unsigned int rs1,unsigned int rs2,unsigned int rt)
{
  assem_debug("and %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  a_logic(0x0A000000,HR(rt),HR(rs1),HR(rs2),0,0);
}

void emit_or(unsigned int rs1,unsigned int rs2,unsigned int rt)
{
  assem_debug("orr %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  a_logic(0x2A000000,HR(rt),HR(rs1),HR(rs2),0,0);
}

void emit_xor(unsigned int rs1,unsigned int rs2,unsigned int rt)
{
  assem_debug("eor %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  a_logic(0x4A000000,HR(rt),HR(rs1),HR(rs2),0,0);
}

// rt = rs + imm, setting the flags or not
static void a_addimm(int rs,int imm,int rt,int s)
{
  u32 add=s?0x31000000:0x11000000, sub=s?0x71000000:0x51000000;
  u32 a=imm<0?-(u32)imm:(u32)imm;
  u32 op=imm<0?sub:add;
  if(a<4096) a_addsub_imm(op,rt,rs,a,0);
  else if(!(a&0xFFF)&&a<16777216) a_addsub_imm(op,rt,rs,a>>12,1);
  else if(!s&&a<16777216) {
    a_addsub_imm(op,rt,rs,a>>12,1);
    a_addsub_imm(op,rt,rt,a&0xFFF,0);
  }
  else {
    assert(rs!=P_IMM);
    a_movimm32(P_IMM,imm);
    a_addsub(s?0x2B000000:0x0B000000,rt,rs,P_IMM,0,0);
  }
}

void emit_addimm(unsigned int rs,int imm,unsigned int rt)
{
  if(imm!=0) {
    assem_debug("add %s,%s,#%d\n",regname[rt],regname[rs],imm);
    a_addimm(HR(rs),imm,HR(rt),0);
  }
  else if(rs!=rt) emit_mov(rs,rt);
}

void emit_addimm_and_set_flags(int imm,int rt)
{
  assem_debug("adds %s,%s,#%d\n",regname[rt],regname[rt],imm);
  a_addimm(HR(rt),imm,HR(rt),1);
}
void emit_addimm_no_flags(u32 imm,unsigned int rt)
{
  emit_addimm(rt,imm,rt);
}

void emit_addnop(unsigned int r)
{
  assem_debug("nop\n");
  output_w32(0xD503201F);
}

void emit_adcimm(unsigned int rs,int imm,unsigned int rt)
{
  assem_debug("adc %s,%s,#%d\n",regname[rt],regname[rs],imm);
  if(imm==0) output_w32(0x1A000000|(ZR<<16)|(HR(rs)<<5)|HR(rt));
  else {
    a_movimm32(P_IMM,imm);
    output_w32(0x1A000000|(P_IMM<<16)|(HR(rs)<<5)|HR(rt));
  }
}

static void a_logicimm(u32 opimm,u32 opreg,int rs,u32 imm,int rt)
{
  u32 enc;
  if(enc_logimm32(imm,&enc)) a_logic_imm(opimm,rt,rs,enc);
  else {
    assert(rs!=P_IMM);
    a_movimm32(P_IMM,imm);
    a_logic(opreg,rt,rs,P_IMM,0,0);
  }
}

void emit_andimm(int rs,int imm,int rt)
{
  assem_debug("and %s,%s,#%x\n",regname[rt],regname[rs],imm);
  if(imm==0) emit_zeroreg(rt);
  else if((u32)imm==0xFFFFFFFF) a_logic(0x2A000000,HR(rt),ZR,HR(rs),0,0);
  else a_logicimm(0x12000000,0x0A000000,HR(rs),imm,HR(rt));
}

void emit_orimm(int rs,int imm,int rt)
{
  assem_debug("orr %s,%s,#%x\n",regname[rt],regname[rs],imm);
  if(imm==0) {
    if(rs!=rt) a_logic(0x2A000000,HR(rt),ZR,HR(rs),0,0);
  }
  else if((u32)imm==0xFFFFFFFF) a_movw(0x12800000,HR(rt),0,0);
  else a_logicimm(0x32000000,0x2A000000,HR(rs),imm,HR(rt));
}

void emit_xorimm(int rs,int imm,int rt)
{
  assem_debug("eor %s,%s,#%x\n",regname[rt],regname[rs],imm);
  if(imm==0) {
    if(rs!=rt) a_logic(0x2A000000,HR(rt),ZR,HR(rs),0,0);
  }
  else if((u32)imm==0xFFFFFFFF) emit_not(rs,rt);
  else a_logicimm(0x52000000,0x4A000000,HR(rs),imm,HR(rt));
}

void emit_shlimm(int rs,unsigned int imm,int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("lsl %s,%s,#%d\n",regname[rt],regname[rs],imm);
  a_bfm(A_UBFM,HR(rt),HR(rs),(32-imm)&31,31-imm);
}

void emit_shrimm(int rs,unsigned int imm,int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("lsr %s,%s,#%d\n",regname[rt],regname[rs],imm);
  a_bfm(A_UBFM,HR(rt),HR(rs),imm,31);
}

void emit_sarimm(int rs,unsigned int imm,int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("asr %s,%s,#%d\n",regname[rt],regname[rs],imm);
  a_bfm(A_SBFM,HR(rt),HR(rs),imm,31);
}

void emit_rorimm(int rs,unsigned int imm,int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("ror %s,%s,#%d\n",regname[rt],regname[rs],imm);
  a_extr(HR(rt),HR(rs),HR(rs),imm);
}

void emit_swapb(int rs,int rt)
{
  assem_debug("swap.b %s,%s\n",regname[rt],regname[rs]);
  output_w32(0x5AC00400|(HR(rs)<<5)|P_T); // rev16
  if(rs!=rt) a_logic(0x2A000000,HR(rt),ZR,HR(rs),0,0);
  a_bfm(A_BFM,HR(rt),P_T,0,15); // bfxil rt,P_T,#0,#16
}

// rt = (rs >> imm) | (rs2 << (32-imm))
void emit_shrdimm(int rs,int rs2,unsigned int imm,int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("extr %s,%s,%s,#%d\n",regname[rt],regname[rs2],regname[rs],imm);
  a_extr(HR(rt),HR(rs2),HR(rs),imm);
}

void emit_shl(unsigned int rs,unsigned int shift,unsigned int rt)
{
  assem_debug("lsl %s,%s,%s\n",regname[rt],regname[rs],regname[shift]);
  output_w32(0x1AC02000|(HR(shift)<<16)|(HR(rs)<<5)|HR(rt));
}
void emit_shr(unsigned int rs,unsigned int shift,unsigned int rt)
{
  assem_debug("lsr %s,%s,%s\n",regname[rt],regname[rs],regname[shift]);
  output_w32(0x1AC02400|(HR(shift)<<16)|(HR(rs)<<5)|HR(rt));
}
void emit_sar(unsigned int rs,unsigned int shift,unsigned int rt)
{
  assem_debug("asr %s,%s,%s\n",regname[rt],regname[rs],regname[shift]);
  output_w32(0x1AC02800|(HR(shift)<<16)|(HR(rs)<<5)|HR(rt));
}

static void a_cmpimm(int rs,int imm)
{
  u32 a=imm<0?-(u32)imm:(u32)imm;
  if(a<4096) a_addsub_imm(imm<0?0x31000000:0x71000000,ZR,rs,a,0);
  else if(!(a&0xFFF)&&a<16777216) a_addsub_imm(imm<0?0x31000000:0x71000000,ZR,rs,a>>12,1);
  else {
    assert(rs!=P_IMM);
    a_movimm32(P_IMM,imm);
    a_addsub(0x6B000000,ZR,rs,P_IMM,0,0);
  }
}
void emit_cmpimm(int rs,int imm)
{
  assem_debug("cmp %s,#%d\n",regname[rs],imm);
  a_cmpimm(HR(rs),imm);
}

void emit_cmp(int rs,int rt)
{
  assem_debug("cmp %s,%s\n",regname[rs],regname[rt]);
  a_addsub(0x6B000000,ZR,HR(rs),HR(rt),0,0);
}

void emit_movzbl_reg(int rs, int rt)
{
  assem_debug("uxtb %s,%s\n",regname[rt],regname[rs]);
  a_bfm(A_UBFM,HR(rt),HR(rs),0,7);
}
void emit_movzwl_reg(int rs, int rt)
{
  assem_debug("uxth %s,%s\n",regname[rt],regname[rs]);
  a_bfm(A_UBFM,HR(rt),HR(rs),0,15);
}
void emit_movsbl_reg(int rs, int rt)
{
  assem_debug("sxtb %s,%s\n",regname[rt],regname[rs]);
  a_bfm(A_SBFM,HR(rt),HR(rs),0,7);
}
void emit_movswl_reg(int rs, int rt)
{
  assem_debug("sxth %s,%s\n",regname[rt],regname[rs]);
  a_bfm(A_SBFM,HR(rt),HR(rs),0,15);
}

/* SH2 instructions setting the T bit (bit 0 of SR) */

void emit_sh2tst(int s1, int s2, int sr, int temp)
{
  emit_test(s1,s2);
  a_set_t(HR(sr),C_EQ);
}
void emit_sh2tstimm(int s, int imm, int sr, int temp)
{
  emit_testimm(s,imm);
  a_set_t(HR(sr),C_EQ);
}
void emit_cmpeq(int s1, int s2, int sr, int temp)
{
  emit_cmp(s1,s2);
  a_set_t(HR(sr),C_EQ);
}
void emit_cmpeqimm(int s, int imm, int sr, int temp)
{
  emit_cmpimm(s,imm);
  a_set_t(HR(sr),C_EQ);
}
void emit_cmpge(int s1, int s2, int sr, int temp)
{
  emit_cmp(s2,s1);
  a_set_t(HR(sr),C_GE);
}
void emit_cmpgt(int s1, int s2, int sr, int temp)
{
  emit_cmp(s2,s1);
  a_set_t(HR(sr),C_GT);
}
void emit_cmphi(int s1, int s2, int sr, int temp)
{
  emit_cmp(s2,s1);
  a_set_t(HR(sr),C_HI);
}
void emit_cmphs(int s1, int s2, int sr, int temp)
{
  emit_cmp(s2,s1);
  a_set_t(HR(sr),C_CS);
}
void emit_dt(int t, int sr)
{
  emit_addimm_and_set_flags(-1,t);
  a_set_t(HR(sr),C_EQ);
}
void emit_cmppz(int s, int sr)
{
  emit_cmpimm(s,0);
  a_set_t(HR(sr),C_GE);
}
void emit_cmppl(int s, int sr, int temp)
{
  emit_cmpimm(s,0);
  a_set_t(HR(sr),C_GT);
}
// Carry flag = T
static void a_t_to_carry(int sr)
{
  a_logic_imm(0x12000000,P_T,sr,0); // and P_T,sr,#1
  a_addsub_imm(0x71000000,ZR,P_T,1,0); // cmp P_T,#1
}
// Carry flag = !T
static void a_not_t_to_carry(int sr)
{
  a_logic_imm(0x12000000,P_T,sr,0); // and P_T,sr,#1
  a_addsub(0x6B000000,ZR,ZR,P_T,0,0); // cmp wzr,P_T
}
void emit_addc(int s, int t, int sr)
{
  a_t_to_carry(HR(sr));
  emit_adcs(s,t,t);
  a_set_t(HR(sr),C_CS);
}
void emit_subc(int s, int t, int sr)
{
  a_not_t_to_carry(HR(sr));
  emit_sbcs(t,s,t);
  a_set_t(HR(sr),C_CC);
}
// ADDV/SUBV: t = t +/- s, T = signed overflow
void emit_addv(int s, int t, int sr)
{
  emit_adds(t,s,t);
  a_set_t(HR(sr),C_VS);
}
void emit_subv(int s, int t, int sr)
{
  emit_subs(t,s,t);
  a_set_t(HR(sr),C_VS);
}
void emit_negc(int rs, int rt, int sr)
{
  a_not_t_to_carry(HR(sr));
  output_w32(0x7A000000|(HR(rs)<<16)|(ZR<<5)|(rt>=0?HR(rt):ZR)); // sbcs rt,wzr,rs
  a_set_t(HR(sr),C_CC);
}
void emit_shrsr(int t, int sr)
{
  a_bfi(HR(sr),HR(t),0,1);
  emit_shrimm(t,1,t);
}
void emit_sarsr(int t, int sr)
{
  a_bfi(HR(sr),HR(t),0,1);
  emit_sarimm(t,1,t);
}
void emit_shlsr(int t, int sr)
{
  a_bfm(A_UBFM,P_T,HR(t),31,31); // lsr P_T,t,#31
  a_bfi(HR(sr),P_T,0,1);
  emit_shlimm(t,1,t);
}
void emit_rotl(int t)
{
  emit_rorimm(t,31,t);
}
void emit_rotlsr(int t, int sr)
{
  emit_rorimm(t,31,t);
  a_bfi(HR(sr),HR(t),0,1);
}
void emit_rotr(int t)
{
  emit_rorimm(t,1,t);
}
void emit_rotrsr(int t, int sr)
{
  a_bfi(HR(sr),HR(t),0,1);
  emit_rorimm(t,1,t);
}
void emit_rotclsr(int t, int sr)
{
  a_bfm(A_UBFM,P_T,HR(t),31,31); // lsr P_T,t,#31
  emit_shlimm(t,1,t);
  a_bfi(HR(t),HR(sr),0,1);
  a_bfi(HR(sr),P_T,0,1);
}
void emit_rotcrsr(int t, int sr)
{
  a_mov64(P_T,HR(t));
  a_extr(HR(t),HR(sr),HR(t),1);
  a_bfi(HR(sr),P_T,0,1);
}

void emit_cmpstr(int s1, int s2, int sr, int temp)
{
  // T is set if any byte of s1 and s2 is equal: (x-0x01010101)&~x&0x80808080
  // is nonzero exactly when x=s1^s2 has a zero byte
  u32 enc;
  a_logic(0x4A000000,P_T2,HR(s1),HR(s2),0,0);
  enc_logimm32(0x01010101,&enc);
  a_logic_imm(0x32000000,P_T,ZR,enc);
  a_addsub(0x4B000000,P_T,P_T2,P_T,0,0);
  a_logic(0x0A200000,P_T,P_T,P_T2,0,0); // bic
  a_testimm(P_T,0x80808080);
  a_set_t(HR(sr),C_NE);
}

void emit_div0s(int s1, int s2, int sr, int temp)
{
  int r=HR(sr);
  emit_andimm(sr,0xfe,sr);
  a_bfm(A_UBFM,P_T,HR(s2),31,31);
  a_bfi(r,P_T,8,1); // Q
  a_bfm(A_UBFM,P_T,HR(s1),31,31);
  a_bfi(r,P_T,9,1); // M
  a_logic(0x4A000000,P_T,HR(s1),HR(s2),0,0);
  a_bfm(A_UBFM,P_T,P_T,31,31);
  a_bfi(r,P_T,0,1); // T=Q^M
}

void emit_multiply(unsigned int rs1,unsigned int rs2,unsigned int rt)
{
  assem_debug("mul %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0x1B000000|(HR(rs2)<<16)|(ZR<<10)|(HR(rs1)<<5)|HR(rt));
}
void emit_umull(unsigned int rs1, unsigned int rs2, unsigned int hi, unsigned int lo)
{
  assem_debug("umull %s:%s,%s,%s\n",regname[hi],regname[lo],regname[rs1],regname[rs2]);
  output_w32(0x9BA00000|(HR(rs2)<<16)|(ZR<<10)|(HR(rs1)<<5)|P_T);
  a_bfm(SF|0x00400000|A_UBFM,HR(hi),P_T,32,63); // lsr hi,P_T,#32
  a_logic(0x2A000000,HR(lo),ZR,P_T,0,0);
}
void emit_smull(unsigned int rs1, unsigned int rs2, unsigned int hi, unsigned int lo)
{
  assem_debug("smull %s:%s,%s,%s\n",regname[hi],regname[lo],regname[rs1],regname[rs2]);
  output_w32(0x9B200000|(HR(rs2)<<16)|(ZR<<10)|(HR(rs1)<<5)|P_T);
  a_bfm(SF|0x00400000|A_UBFM,HR(hi),P_T,32,63); // lsr hi,P_T,#32
  a_logic(0x2A000000,HR(lo),ZR,P_T,0,0);
}

/* Memory access. Guest memory is accessed at address + memory_map entry*4:
   the 64-bit entries are (host - guest)>>2 with the write-protect (bit 62)
   and unmapped (bit 63) flags on top, which the shift drops. */

void emit_readword_indexed(int offset, int rs, int rt)
{
  assem_debug("ldr %s,%s+%d\n",regname[rt],regname[rs],offset);
  a_ldst(LS_LDRW,HR(rt),HR(rs),offset);
}
void emit_readword_indexed_map(int addr, int rs, int map, int rt)
{
  if(map<0) emit_readword_indexed(addr, rs, rt);
  else {
    assert(addr==0);
    assem_debug("ldr %s,%s,%s lsl #2\n",regname[rt],regname[rs],regname[map]);
    a_ldst_reg(LS_LDRW,HR(rt),HR(rs),HR(map),1);
  }
}
// P_T = host address of rs+map
static void a_map_addr(int rs,int map)
{
  a_addsub(SF|0x0B000000,P_T,HR(rs),HR(map),0,2);
}
void emit_movsbl_indexed(int offset, int rs, int rt)
{
  assem_debug("ldrsb %s,%s+%d\n",regname[rt],regname[rs],offset);
  a_ldst(LS_LDRSB,HR(rt),HR(rs),offset);
}
void emit_movsbl_indexed_map(int addr, int rs, int map, int rt)
{
  if(map<0) emit_movsbl_indexed(addr, rs, rt);
  else {
    a_map_addr(rs,map);
    a_ldst(LS_LDRSB,HR(rt),P_T,addr);
  }
}
void emit_movswl_indexed(int offset, int rs, int rt)
{
  assem_debug("ldrsh %s,%s+%d\n",regname[rt],regname[rs],offset);
  a_ldst(LS_LDRSH,HR(rt),HR(rs),offset);
}
void emit_movswl_indexed_map(int addr, int rs, int map, int rt)
{
  if(map<0) emit_movswl_indexed(addr, rs, rt);
  else {
    a_map_addr(rs,map);
    a_ldst(LS_LDRSH,HR(rt),P_T,addr);
  }
}
void emit_movzbl_indexed(int offset, int rs, int rt)
{
  assem_debug("ldrb %s,%s+%d\n",regname[rt],regname[rs],offset);
  a_ldst(LS_LDRB,HR(rt),HR(rs),offset);
}
void emit_movzwl_indexed(int offset, int rs, int rt)
{
  assem_debug("ldrh %s,%s+%d\n",regname[rt],regname[rs],offset);
  a_ldst(LS_LDRH,HR(rt),HR(rs),offset);
}
// A variable at a fixed address: relative to dynarec_local if possible
static void a_ldst_abs(int op,int rt,pointer addr)
{
  s64 offset=(s64)addr-(s64)DL;
  if(offset>=-256&&offset<(4096<<ls_size[op])) a_ldst(op,rt,P_FP,offset);
  else {
    assert(rt!=P_IMM);
    a_movimm64(P_IMM,addr);
    a_ldst(op,rt,P_IMM,0);
  }
}
void emit_readword(pointer addr, int rt)
{
  assem_debug("ldr %s,[%lx]\n",regname[rt],addr);
  a_ldst_abs(LS_LDRW,HR(rt),addr);
}
void emit_movsbl(pointer addr, int rt)
{
  a_ldst_abs(LS_LDRSB,HR(rt),addr);
}
void emit_movswl(pointer addr, int rt)
{
  a_ldst_abs(LS_LDRSH,HR(rt),addr);
}
void emit_movzbl(pointer addr, int rt)
{
  a_ldst_abs(LS_LDRB,HR(rt),addr);
}
void emit_movzwl(pointer addr, int rt)
{
  a_ldst_abs(LS_LDRH,HR(rt),addr);
}

void emit_writeword_indexed(int rt, int offset, int rs)
{
  assem_debug("str %s,%s+%d\n",regname[rt],regname[rs],offset);
  a_ldst(LS_STRW,HR(rt),HR(rs),offset);
}
void emit_writeword_indexed_map(int rt, int addr, int rs, int map, int temp)
{
  if(map<0) emit_writeword_indexed(rt, addr, rs);
  else {
    assert(addr==0);
    assem_debug("str %s,%s,%s lsl #2\n",regname[rt],regname[rs],regname[map]);
    a_ldst_reg(LS_STRW,HR(rt),HR(rs),HR(map),1);
  }
}
void emit_writehword_indexed(int rt, int offset, int rs)
{
  assem_debug("strh %s,%s+%d\n",regname[rt],regname[rs],offset);
  a_ldst(LS_STRH,HR(rt),HR(rs),offset);
}
void emit_writehword_indexed_map(int rt, int addr, int rs, int map, int temp)
{
  if(map<0) emit_writehword_indexed(rt, addr, rs);
  else {
    a_map_addr(rs,map);
    a_ldst(LS_STRH,HR(rt),P_T,addr);
  }
}
void emit_writebyte_indexed(int rt, int offset, int rs)
{
  assem_debug("strb %s,%s+%d\n",regname[rt],regname[rs],offset);
  a_ldst(LS_STRB,HR(rt),HR(rs),offset);
}
void emit_writebyte_indexed_map(int rt, int addr, int rs, int map, int temp)
{
  if(map<0) emit_writebyte_indexed(rt, addr, rs);
  else {
    a_map_addr(rs,map);
    a_ldst(LS_STRB,HR(rt),P_T,addr);
  }
}
void emit_writeword(int rt, pointer addr)
{
  assem_debug("str %s,[%lx]\n",regname[rt],addr);
  a_ldst_abs(LS_STRW,HR(rt),addr);
}
void emit_writehword(int rt, pointer addr)
{
  a_ldst_abs(LS_STRH,HR(rt),addr);
}
void emit_writebyte(int rt, pointer addr)
{
  a_ldst_abs(LS_STRB,HR(rt),addr);
}

// Byte read-modify-write: P_T2 = byte at addr+map, op, store back
static void a_rmw(u32 opimm,u32 opreg,int addr,int map,int imm)
{
  assert(map>=0);
  a_map_addr(addr,map);
  a_ldst(LS_LDRB,P_T2,P_T,0);
  a_logicimm(opimm,opreg,P_T2,imm,P_T2);
  a_ldst(LS_STRB,P_T2,P_T,0);
}
void emit_rmw_andimm(int addr, int map, int imm)
{
  a_rmw(0x12000000,0x0A000000,addr,map,imm);
}
void emit_rmw_xorimm(int addr, int map, int imm)
{
  a_rmw(0x52000000,0x4A000000,addr,map,imm);
}
void emit_rmw_orimm(int addr, int map, int imm)
{
  a_rmw(0x32000000,0x2A000000,addr,map,imm);
}
void emit_sh2tas(int addr, int map, int sr)
{
  u32 enc;
  assert(map>=0);
  a_map_addr(addr,map);
  a_ldst(LS_LDRB,P_T2,P_T,0);
  a_cmpimm(P_T2,0);
  a_cset(P_IMM,C_EQ);
  a_bfi(HR(sr),P_IMM,0,1);
  enc_logimm32(0x80,&enc);
  a_logic_imm(0x32000000,P_T2,P_T2,enc);
  a_ldst(LS_STRB,P_T2,P_T,0);
}

void emit_teq(int rs, int rt)
{
  assem_debug("teq %s,%s\n",regname[rs],regname[rt]);
  a_logic(0x4A000000,P_T,HR(rs),HR(rt),0,0);
  a_logic(0x6A000000,ZR,P_T,P_T,0,0);
}

// Load return address
void emit_load_return_address(unsigned int rt)
{
  // (assumes this instruction will be followed by a branch instruction)
  assem_debug("adr %s,.+8\n",regname[rt]);
  output_w32(0x10000000|(2<<5)|HR(rt));
}

void emit_prefetch(void *addr)
{
}
void emit_prefetchreg(int r)
{
  output_w32(0xF9800000|(HR(r)<<5)); // prfm pldl1keep,[r]
}

// Save registers before function call
void save_regs(u32 reglist)
{
  int hr;
  reglist&=0xf; // only save the caller-save registers, x0-x3
  for(hr=0;hr<4;hr++) {
    if(!((reglist>>hr)&1)) continue;
    if(!(hr&1)&&((reglist>>(hr+1))&1)) {
      output_w32(0xA9000000|((hr+1)<<10)|(P_FP<<5)|hr|((hr&~1)<<15)); // stp
      hr++;
    }
    else a_ldst(LS_STRX,hr,P_FP,hr*8);
  }
}
// Restore registers after function call
void restore_regs(u32 reglist)
{
  int hr;
  reglist&=0xf;
  for(hr=0;hr<4;hr++) {
    if(!((reglist>>hr)&1)) continue;
    if(!(hr&1)&&((reglist>>(hr+1))&1)) {
      output_w32(0xA9400000|((hr+1)<<10)|(P_FP<<5)|hr|((hr&~1)<<15)); // ldp
      hr++;
    }
    else a_ldst(LS_LDRX,hr,P_FP,hr*8);
  }
}

/* Stubs/epilogue */

void emit_extjump(pointer addr, int target)
{
  s64 offset;
  // Careful about the code output here, get_pointer needs to parse it.
  a_movw(0x52800000,0,target&0xFFFF,0);
  a_movw(0x72800000,0,((u32)target)>>16,1);
  offset=(s64)addr-(s64)(pointer)out;
  assert(offset>=-1048576&&offset<1048576);
  output_w32(0x10000000|((offset&3)<<29)|(((offset>>2)&0x7FFFF)<<5)|1); // adr x1,addr
  emit_jmp((pointer)dyna_linker);
}

void do_readstub(int n)
{
  assem_debug("do_readstub %x\n",start+(int)stubs[n][3]*2);
  set_jump_target(stubs[n][1],(pointer)out);
  int type=stubs[n][0];
  int i=stubs[n][3];
  int rs=stubs[n][4];
  struct regstat *i_regs=(struct regstat *)stubs[n][5];
  u32 reglist=stubs[n][7];
  signed char *i_regmap=i_regs->regmap;
  int addr=get_reg(i_regmap,AGEN1+(i&1));
  int rt;

  rt=get_reg(i_regmap,rt1[i]==TBIT?-1:rt1[i]);
  assert(rs>=0);
  if(addr<0) addr=rt;
  if(addr<0) addr=get_reg(i_regmap,-1);
  assert(addr>=0);
  save_regs(reglist);
  if(type==LOADB_STUB) emit_xorimm(rs,1,0);
  else {if(rs!=0) emit_mov(rs,0);}
  int cc=get_reg(i_regmap,CCREG);
  if(cc<0) {
    emit_loadreg(CCREG,2);
  }
  if(type==LOADB_STUB)
    emit_call((pointer)DynarecReadByte);
  if(type==LOADW_STUB)
    emit_call((pointer)DynarecReadWord);
  if(type==LOADL_STUB)
    emit_call((pointer)DynarecReadLong);
  if(type==LOADS_STUB)
  {
    // RTE instruction, pop PC and SR from stack
    int pc=get_reg(i_regmap,RTEMP);
    assert(pc>=0);
    if(is_callersave(rs))
      emit_writeword(rs,DL+DL_STUBTEMP1);
    emit_call((pointer)DynarecReadLong);
    if(is_callersave(rs)&&rs!=0)
      emit_readword(DL+DL_STUBTEMP1,rs);
    if(pc==0) {
      emit_writeword(0,DL+DL_STUBTEMP1);
    }
    else
    {
      if(is_callersave(pc))
        emit_writeword(0,DL+DL_STUBTEMP1);
      else
        emit_mov(0,pc);
      if(rs==0) {
        emit_readword(DL+DL_STUBTEMP1,rs);
        emit_addimm(rs,4,rs);
      }else
        emit_addimm(rs,4,0);
    }
    emit_call((pointer)DynarecReadLong);
    assert(rt>=0);
    if(rt!=0) emit_mov(0,rt);
    if(is_callersave(pc))
      emit_readword(DL+DL_STUBTEMP1,pc);
  }
  else if(type==LOADB_STUB)
  {
    if(rt>=0) emit_movsbl_reg(0,rt);
  }
  else if(type==LOADW_STUB)
  {
    if(rt>=0) emit_movswl_reg(0,rt);
  }
  else
  {
    if(rt>0) emit_mov(0,rt);
  }
  restore_regs(reglist);
  if(type==LOADS_STUB) emit_addimm(rs,8,rs);
  emit_jmp(stubs[n][2]); // return address
}

void inline_readstub(int type, int i, u32 addr, signed char regmap[], int target, int adj, u32 reglist)
{
  assem_debug("inline_readstub\n");
  int rt=get_reg(regmap,target);
  if(rt<0) rt=get_reg(regmap,-1);
  assert(rt>=0);
  save_regs(reglist);
  emit_movimm(addr,0);
  if(type==LOADB_STUB)
    emit_call((pointer)DynarecReadByte);
  if(type==LOADW_STUB)
    emit_call((pointer)DynarecReadWord);
  if(type==LOADL_STUB)
    emit_call((pointer)DynarecReadLong);
  assert(type!=LOADS_STUB);
  if(type==LOADB_STUB)
  {
    if(rt>=0) emit_movsbl_reg(0,rt);
  }
  else if(type==LOADW_STUB)
  {
    if(rt>=0) emit_movswl_reg(0,rt);
  }
  else
  {
    if(rt>0) emit_mov(0,rt);
  }

  restore_regs(reglist);
}

#ifdef YAB_KICKYIELD
/* After a word-write stub's call: if the write woke the other CPU (DynarecWriteWord set
   yab_kick[0]), end this slice at the next cycle check, keeping the cycles left (see
   DynarecWriteWord and assem_arm.c). x0-x2 are free here (after the call, before
   restore_regs). */
extern u32 yab_kick[3];
#ifdef YAB_CALLCAP
#define KICK_CC (CLOCK_DIVIDER*YAB_CALLCAP)
#else
#define KICK_CC 0
#endif
static void emit_kick_yield(void)
{
  pointer jaddr;
  emit_movimm_ptr((pointer)yab_kick,1);
  emit_readword_indexed(0,1,0);
  emit_test(0,0);
  jaddr=(pointer)out;
  emit_jeq(0);
  emit_zeroreg(0);
  emit_writeword_indexed(0,0,1);
  emit_readword_indexed(4+4*slave,1,0);
  emit_add(0,HOST_CCREG,0);
  if(KICK_CC) { emit_movimm(KICK_CC,2); emit_sub(0,2,0); }
  emit_writeword_indexed(0,4+4*slave,1);
  emit_movimm(KICK_CC,HOST_CCREG);
  set_jump_target(jaddr,(pointer)out);
}
#endif

void do_writestub(int n)
{
  assem_debug("do_writestub %x\n",start+(int)stubs[n][3]*2);
  set_jump_target(stubs[n][1],(pointer)out);
  int type=stubs[n][0];
  int i=stubs[n][3];
  int rs=stubs[n][4];
  struct regstat *i_regs=(struct regstat *)stubs[n][5];
  u32 reglist=stubs[n][7];
  signed char *i_regmap=i_regs->regmap;
  int addr=get_reg(i_regmap,AGEN1+(i&1));
  int rt=get_reg(i_regmap,rs1[i]);
  assert(rs>=0);
  assert(rt>=0);
  if(addr<0) addr=get_reg(i_regmap,-1);
  assert(addr>=0);
  save_regs(reglist);
  // address in w0, data in w1
  if(rs!=0) {
    if(rt==0) {
      if(rs==1) {
        emit_mov(0,15);
        emit_mov(1,0);
        emit_mov(15,1);
      } else {
        emit_mov(rt,1);
        emit_mov(rs,0);
      }
    }
    else {
      emit_mov(rs,0);
      if(rt!=1) emit_mov(rt,1);
    }
  }
  else if(rt!=1) emit_mov(rt,1);

  if(type==STOREB_STUB)
    emit_call((pointer)WriteInvalidateByteSwapped);
  if(type==STOREW_STUB)
    emit_call((pointer)WriteInvalidateWord);
  if(type==STOREL_STUB)
    emit_call((pointer)WriteInvalidateLong);
#ifdef YAB_KICKYIELD
  if(type==STOREW_STUB) emit_kick_yield();
#endif

  restore_regs(reglist);
  emit_jmp(stubs[n][2]); // return address
}

void inline_writestub(int type, int i, u32 addr, signed char regmap[], int target, int adj, u32 reglist)
{
  assem_debug("inline_writestub\n");
  int rt=get_reg(regmap,target);
  assert(rt>=0);
  save_regs(reglist);
  // address in w0, data in w1
  if(rt!=1) emit_mov(rt,1);
  emit_movimm(addr,0);
  if(type==STOREB_STUB)
    emit_call((pointer)WriteInvalidateByte);
  if(type==STOREW_STUB)
    emit_call((pointer)WriteInvalidateWord);
  if(type==STOREL_STUB)
    emit_call((pointer)WriteInvalidateLong);
#ifdef YAB_KICKYIELD
  if(type==STOREW_STUB) emit_kick_yield();
#endif
  restore_regs(reglist);
}

void do_rmwstub(int n)
{
  assem_debug("do_rmwstub %x\n",start+(int)stubs[n][3]*2);
  set_jump_target(stubs[n][1],(pointer)out);
  int type=stubs[n][0];
  int i=stubs[n][3];
  int rs=stubs[n][4];
  struct regstat *i_regs=(struct regstat *)stubs[n][5];
  u32 reglist=stubs[n][7];
  signed char *i_regmap=i_regs->regmap;
  int addr=get_reg(i_regmap,AGEN1+(i&1));
  assert(rs>=0);
  if(addr<0) addr=get_reg(i_regmap,-1);
  assert(addr>=0);
  save_regs(reglist);
  // address in w0, data in w1
  emit_xorimm(rs,1,rs);
  if(rs!=0) emit_mov(rs,0);
  if(is_callersave(rs))
    emit_writeword(0,DL+DL_STUBTEMP1);

  emit_call((pointer)DynarecReadByte);
  if(type==RMWA_STUB)
    emit_andimm(0,imm[i],1);
  if(type==RMWX_STUB)
    emit_xorimm(0,imm[i],1);
  if(type==RMWO_STUB)
    emit_orimm(0,imm[i],1);
  if(type==RMWT_STUB) { // TAS.B
    emit_writeword(0,DL+DL_STUBTEMP2);
    emit_orimm(0,0x80,1);
  }
  if(is_callersave(rs))
    emit_readword(DL+DL_STUBTEMP1,0);
  else
    emit_mov(rs,0);
  emit_call((pointer)WriteInvalidateByte);

  restore_regs(reglist);

  if(opcode2[i]==11) { // TAS.B
    signed char sr;
    emit_readword(DL+DL_STUBTEMP2,HOST_TEMPREG);
    sr=get_reg(i_regs->regmap,SR);
    assert(sr>=0); // Liveness analysis?
    emit_testimm(HOST_TEMPREG,0xff);
    a_set_t(HR(sr),C_EQ);
  }
  emit_jmp(stubs[n][2]); // return address
}

void do_unalignedwritestub(int n)
{
  set_jump_target(stubs[n][1],(pointer)out);
  output_w32(0xD4200000); // brk #0
  emit_jmp(stubs[n][2]); // return address
}

pointer do_dirty_stub(int i)
{
  assem_debug("do_dirty_stub %x\n",start+i*2);
  pointer source_aligned=(pointer)source&~3;
  u32 len=(((pointer)source+slen*2+2)&~3)-source_aligned;
  u32 vaddr=start+i*2+slave;
  pointer lit,clean,entry;
  // Careful about the code output here, verify_dirty needs to parse it.
  u32 *stub=(u32 *)out;
  assert(len<65536);
  out+=7*4;
  if((pointer)out&7) output_w32(0xD503201F); // nop
  lit=(pointer)out;
  *(u64 *)out=source_aligned; out+=8;
  *(u64 *)out=(pointer)copy; out+=8;
  clean=(pointer)out;
  stub[0]=0x58000001|((((lit-(pointer)stub)>>2)&0x7FFFF)<<5);       // ldr x1,source
  stub[1]=0x58000002|((((lit+8-(pointer)(stub+1))>>2)&0x7FFFF)<<5); // ldr x2,copy
  stub[2]=0x52800003|(len<<5);                                       // movz w3,#len
  stub[3]=0x52800000|((vaddr&0xFFFF)<<5);                           // movz w0,#vaddr
  stub[4]=0x72A00000|((vaddr>>16)<<5);                              // movk w0,#vaddr>>16,lsl #16
  stub[5]=0x94000000|((((pointer)verify_code-(pointer)(stub+5))>>2)&0x3FFFFFF); // bl
  stub[6]=0x14000000|(((clean-(pointer)(stub+6))>>2)&0x3FFFFFF);    // b clean
  assert(in_range26((pointer)verify_code));
  entry=(pointer)out;
  load_regs_entry(i);
  if(entry==(pointer)out) entry=instr_addr[i];
  emit_jmp(instr_addr[i]);
  return entry;
}

/* Memory Map */

int do_map_r(int s,int ar,int map,int cache,int x,int a,int shift,int c,u32 addr)
{
  if(c) {
    return -1; // No mapping
  }
  else {
    assert(s!=map);
    if(cache>=0) {
      // Use cached offset to memory map
      a_addsub(0x0B000000,HR(map),HR(cache),HR(s),1,12); // add map,cache,s,lsr #12
    }else{
      emit_movimm(MM_INDEX,map);
      a_addsub(0x0B000000,HR(map),HR(map),HR(s),1,12);
    }
    // Schedule this while we wait on the load
    if(x) emit_xorimm(s,x,ar);
    // ldr xmap,[x28,wmap,uxtw #3]
    output_w32(0xF8605800|(HR(map)<<16)|(P_FP<<5)|HR(map));
  }
  return map;
}
int do_map_r_branch(int map, int c, u32 addr, pointer *jaddr)
{
  if(!c) {
    a_logic(SF|0x6A000000,ZR,HR(map),HR(map),0,0); // tst xmap,xmap
    *jaddr=(pointer)out;
    emit_js(0);
  }
  return map;
}

void gen_tlb_addr_r(int ar, int map) {
  if(map>=0) {
    a_addsub(SF|0x0B000000,HR(ar),HR(ar),HR(map),0,2);
  }
}

int do_map_w(int s,int ar,int map,int cache,int x,int c,u32 addr)
{
  if(c) {
    if(can_direct_write(addr)) {
      // address_generation already loaded the const
      output_w32(0xF8605800|(HR(map)<<16)|(P_FP<<5)|HR(map));
    }
    else
      return -1; // No mapping
  }
  else {
    assert(s!=map);
    if(cache>=0) {
      // Use cached offset to memory map
      a_addsub(0x0B000000,HR(map),HR(cache),HR(s),1,12);
    }else{
      emit_movimm(MM_INDEX,map);
      a_addsub(0x0B000000,HR(map),HR(map),HR(s),1,12);
    }
    // Schedule this while we wait on the load
    if(x) emit_xorimm(s,x,ar);
    output_w32(0xF8605800|(HR(map)<<16)|(P_FP<<5)|HR(map));
  }
  return map;
}
void do_map_w_branch(int map, int c, u32 addr, pointer *jaddr)
{
  if(!c||can_direct_write(addr)) {
    u32 enc;
    enc_logimm64(0x4000000000000000ULL,&enc);
    a_logic_imm(SF|0x72000000,ZR,HR(map),enc); // tst xmap,#1<<62
    *jaddr=(pointer)out;
    emit_jne(0);
  }
}

void gen_tlb_addr_w(int ar, int map) {
  if(map>=0) {
    a_addsub(SF|0x0B000000,HR(ar),HR(ar),HR(map),0,2);
  }
}

// This reverses the above operation
void gen_orig_addr_w(int ar, int map) {
  if(map>=0) {
    a_addsub(SF|0x4B000000,HR(ar),HR(ar),HR(map),0,2);
  }
}

// Generate the index of the memory_map entry, relative to dynarec_local
void generate_map_const(u32 addr,int reg) {
  emit_movimm((addr>>12)+MM_INDEX,reg);
}

/* Special assem */

void do_preload_rhash(int r) {
  // Don't need this for ARM.  On x86, this puts the value 0xf8 into the
  // register.  On ARM the hash can be done with a single instruction (below)
}

void do_preload_rhtbl(int ht) {
  pointer table=slave?(pointer)mini_ht_slave:(pointer)mini_ht_master;
  a_addsub_imm(SF|0x11000000,HR(ht),P_FP,table-DL,0);
}

// Offset of the mini_ht entry, divided by 2 (the entries are 16 bytes)
void do_rhash(int rs,int rh) {
  emit_andimm(rs,0xf8,rh);
}

void do_miniht_load(int ht,int rh) {
  assem_debug("add %s,%s,%s,lsl #1; ldr %s,[%s]\n",regname[ht],regname[ht],regname[rh],regname[rh],regname[ht]);
  a_addsub(SF|0x0B000000,HR(ht),HR(ht),HR(rh),0,1);
  a_ldst(LS_LDRW,HR(rh),HR(ht),0);
}

void do_miniht_jump(int rs,int rh,int ht) {
  pointer jaddr;
  emit_cmp(rh,rs);
  jaddr=(pointer)out;
  emit_jne(0);
  a_ldst(LS_LDRX,P_TMP,HR(ht),8);
  output_w32(0xD61F0000|(P_TMP<<5)); // br
  set_jump_target(jaddr,(pointer)out);
  emit_jmp(jump_vaddr_reg[slave][rs]);
}

void do_miniht_insert(u32 return_address,int rt,int temp) {
  pointer table=slave?(pointer)mini_ht_slave:(pointer)mini_ht_master;
  pointer entry=table+((return_address&0xFF)>>3)*16;
  emit_movimm(return_address,rt); // PC into link register
  add_to_linker((pointer)out,return_address,1);
  output_w32(0x10000000|HR(temp)); // adr temp,<block> (set by the linker)
  a_ldst_abs(LS_STRX,HR(rt),entry);
  a_ldst_abs(LS_STRX,HR(temp),entry+8);
}

void wb_valid(signed char pre[],signed char entry[],u32 dirty_pre,u32 dirty,u64 u)
{
  //if(dirty_pre==dirty) return;
  int hr,reg;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      reg=pre[hr];
      if(((~u)>>(reg&63))&1) {
        if(reg>=0) {
          if(((dirty_pre&~dirty)>>hr)&1) {
            if(reg>=0&&reg<TBIT) {
              emit_storereg(reg,hr);
            }
          }
        }
      }
    }
  }
}

// Make the instruction cache see the code written or patched in the
// areas marked in needs_clear_cache, then clear the marks
void do_clear_cache()
{
  int i,j;
  for (i=0;i<(1<<(TARGET_SIZE_2-17));i++)
  {
    u32 bitmap=needs_clear_cache[i];
    if(bitmap) {
      pointer start,end;
      for(j=0;j<32;j++)
      {
        if(bitmap&(1<<j)) {
          start=BASE_ADDR+i*131072+j*4096;
          end=start+4096;
          j++;
          while(j<32&&(bitmap&(1<<j))) {
            end+=4096;
            j++;
          }
          __builtin___clear_cache((void *)start,(void *)end);
        }
      }
      needs_clear_cache[i]=0;
    }
  }
}

/* Helpers called from generated code and from linkage_arm64.s. On ARM
   these are assembly (linkage_arm.s); here they are C with the same
   behaviour. */

void add_link(u32 vaddr,void *src);

static u32 linker_page(u32 vaddr)
{
  u32 page=(vaddr&0xDFFFFFFF)>>12;
  if(page>1024) page=1024+(page&1023);
  return page;
}

// dyna_linker (linkage_arm64.s): an external branch at 'branch' is taken
// for the first time. Link it to the target block if the block is clean,
// and return where to go.
pointer DynarecLink(u32 vaddr,u32 *branch)
{
  for(;;) {
    u32 page=linker_page(vaddr);
    struct ll_entry *head;
    pointer *ht_bin;
    for(head=jump_in[page];head;head=head->next) {
      if(head->vaddr==vaddr) {
        pointer target=(pointer)head->addr;
        pointer stub=branch_target((pointer)branch);
        if(stub!=target) {
          add_link(vaddr,(void *)stub);
          set_jump_target((pointer)branch,target);
          __builtin___clear_cache((void *)branch,(void *)(branch+1));
        }
        return target;
      }
    }
    ht_bin=hash_table[((vaddr>>16)^vaddr)&0xFFFF];
    if(ht_bin[0]==vaddr) return ht_bin[1];
    if(ht_bin[2]==vaddr) return ht_bin[3];
    for(head=jump_dirty[page];head;head=head->next) {
      if(head->vaddr==vaddr) {
        ht_bin[3]=ht_bin[1];
        ht_bin[2]=ht_bin[0];
        ht_bin[1]=(pointer)head->addr;
        ht_bin[0]=vaddr;
        return (pointer)head->addr;
      }
    }
    if(sh2_recompile_block(vaddr)) {
      printf("DynarecLink: can't compile %x\n",vaddr);
      abort();
    }
  }
}

// Stores from the generated code that miss the direct path: compiled code
// in the page is invalidated first (cached_code), as in linkage_arm.s
void WriteInvalidateLong(u32 addr, u32 val)
{
  if((cached_code[addr>>15]>>((addr>>12)&7))&1) invalidate_addr(addr);
  DynarecWriteLong(addr,val);
}
void WriteInvalidateWord(u32 addr, u32 val)
{
  val&=0xFFFF;
  if((cached_code[addr>>15]>>((addr>>12)&7))&1) invalidate_addr(addr);
  DynarecWriteWord(addr,val);
}
void WriteInvalidateByte(u32 addr, u32 val)
{
  val&=0xFF;
  if((cached_code[addr>>15]>>((addr>>12)&7))&1) invalidate_addr(addr);
  DynarecWriteByte(addr,val);
}
void WriteInvalidateByteSwapped(u32 addr, u32 val)
{
  WriteInvalidateByte(addr^1,val);
}

// CPU-architecture-specific initialization
void arch_init() {
}
