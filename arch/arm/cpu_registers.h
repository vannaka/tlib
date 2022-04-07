#include "cpu-defs.h"

// this is a trick to make the registers parser simpler
#if defined(TARGET_ARM32) && defined(TARGET_PROTO_ARM_M)
#define TARGET_PROTO_ARM32_M
#endif

typedef enum {
#ifdef TARGET_ARM32 
    R_0_32       = 0,
    R_1_32       = 1,
    R_2_32       = 2,
    R_3_32       = 3,
    R_4_32       = 4,
    R_5_32       = 5,
    R_6_32       = 6,
    R_7_32       = 7,
    R_8_32       = 8,
    R_9_32       = 9,
    R_10_32      = 10,
    R_11_32      = 11,
    R_12_32      = 12,
    R_13_32      = 13,
    SP_32        = 13,
    R_14_32      = 14,
    LR_32        = 14,
    R_15_32      = 15,
    PC_32        = 15,
    CPSR_32      = 25,
#endif
#ifdef TARGET_PROTO_ARM32_M
    Control_32   = 18,
    BasePri_32   = 19,
    VecBase_32   = 20,
    CurrentSP_32 = 21,
    OtherSP_32   = 22,
    FPCCR_32     = 23,
    FPCAR_32     = 24,
    FPDSCR_32    = 26,
    CPACR_32     = 27,
    PRIMASK_32   = 28,
#endif
} Registers;
