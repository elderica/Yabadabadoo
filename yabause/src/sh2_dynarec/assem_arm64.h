/* The host registers are numbered as in assem_arm.h (0-12, the temporary is
   14) so the register allocator works the same way; assem_arm64.c maps the
   numbers to AArch64 registers:

     0-3   x0-x3    arguments, caller-save
     4-9   x19-x24  callee-save
     10    x25      cycle count (HOST_CCREG)
     11    x28      &dynarec_local (FP, never allocated)
     12    x26      callee-save
     14    x16      temporary (HOST_TEMPREG)

   x17 and x9-x15 are scratch registers of the code generator, never
   allocated. Guest registers are kept in the low 32 bits: every operation on
   them is a 32-bit one, which zeroes the upper half, so a register used as a
   guest address can be added to a 64-bit offset directly. */

#define HOST_REGS 13
#define HOST_CCREG 10
#define EXCLUDE_REG 11
#define SLAVERA_REG 8

#define HOST_IMM8 1
#define HAVE_CMOV_IMM 1
#define USE_MINI_HT 1
#define POINTERS_64BIT 1

#define ARG1_REG 0
#define ARG2_REG 1
#define ARG3_REG 2
#define ARG4_REG 3

#define FP 11
#define HOST_TEMPREG 14

// Note: FP is set to &dynarec_local when executing generated code.
// Thus the local variables are actually global and not on the stack.

extern u8 sh2_dynarec_target[16777216];
extern u64 memory_map[1048576]; // 64-bit

// The code buffer is in our own .bss (linkage_arm64.s), within branch range
// (128 MB) of the C code it calls
#define BASE_ADDR ((pointer)&sh2_dynarec_target) // Code generator target address
#define TARGET_SIZE_2 24 // 2^24 = 16 megabytes
#define JUMP_TABLE_SIZE 0
