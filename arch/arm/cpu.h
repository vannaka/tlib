/*
 * ARM virtual CPU header
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CPU_ARM_H
#define CPU_ARM_H

#include <stdbool.h>
#include <stdlib.h>
#include "cpu-defs.h"
#include "bit_helper.h" // extract32
#include "ttable.h"

#include "softfloat.h"
#include "arch_callbacks.h"

#define SUPPORTS_GUEST_PROFILING

#if TARGET_LONG_BITS == 32
#define TARGET_ARM32
#elif TARGET_LONG_BITS == 64
#define TARGET_ARM64
#else
#error "Target arch can be only 32-bit or 64-bit"
#endif

/* To enable banking of coprocessor registers depending on ns-bit we
 * add a bit to distinguish between secure and non-secure cpregs in the
 * hashtable.
 */
#define CP_REG_NS_SHIFT 29
#define CP_REG_NS_MASK  (1 << CP_REG_NS_SHIFT)

#define ENCODE_CP_REG(cp, is64, ns, crn, crm, opc1, opc2)   \
    ((ns) << CP_REG_NS_SHIFT | ((cp) << 16) | ((is64) << 15) |   \
     ((crn) << 11) | ((crm) << 7) | ((opc1) << 3) | (opc2))

#include "cpu_registers.h"

#define EXCP_UDEF           1    /* undefined instruction */
#define EXCP_SWI            2    /* software interrupt */
#define EXCP_PREFETCH_ABORT 3
#define EXCP_DATA_ABORT     4
#define EXCP_IRQ            5
#define EXCP_FIQ            6
#define EXCP_BKPT           7
#define EXCP_KERNEL_TRAP    9    /* Jumped to kernel code page.  */
#define EXCP_STREX          10
#define EXCP_NOCP           17   /* NOCP usage fault */
#define EXCP_INVSTATE       18   /* INVSTATE usage fault */

#define ARMV7M_EXCP_RESET   1
#define ARMV7M_EXCP_NMI     2
#define ARMV7M_EXCP_HARD    3
#define ARMV7M_EXCP_MEM     4
#define ARMV7M_EXCP_BUS     5
#define ARMV7M_EXCP_USAGE   6
#define ARMV7M_EXCP_SVC     11
#define ARMV7M_EXCP_DEBUG   12
#define ARMV7M_EXCP_PENDSV  14
#define ARMV7M_EXCP_SYSTICK 15

/* MemManage Fault : bits 0:7 of CFSR */
#define MEM_FAULT_MMARVALID     1 << 7
#define MEM_FAULT_MSTKERR       1 << 4
#define MEM_FAULT_MUNSTKERR     1 << 3
#define MEM_FAULT_DACCVIOL      1 << 1
#define MEM_FAULT_IACCVIOL      1 << 0
/* Usage Fault : bits 16-31 of CFSR */
#define USAGE_FAULT_OFFSET      16
#define USAGE_FAULT_DIVBYZERO   (1 << 9) << USAGE_FAULT_OFFSET
#define USAGE_FAULT_UNALIGNED   (1 << 8) << USAGE_FAULT_OFFSET
#define USAGE_FAULT_NOPC        (1 << 3) << USAGE_FAULT_OFFSET
#define USAGE_FAULT_INVPC       (1 << 2) << USAGE_FAULT_OFFSET
#define USAGE_FAULT_INVSTATE    (1 << 1) << USAGE_FAULT_OFFSET
#define USAGE_FAULT_UNDEFINSTR  (1 << 0) << USAGE_FAULT_OFFSET

#define in_privileged_mode(ENV) (((ENV)->v7m.control & 0x1) == 0 || (ENV)->v7m.handler_mode)

#define MAX_MPU_REGIONS                     32
#define MPU_SIZE_FIELD_MASK                 0x3E
#define MPU_REGION_ENABLED_BIT              0x1
#define MPU_SIZE_AND_ENABLE_FIELD_MASK      (MPU_SIZE_FIELD_MASK | MPU_REGION_ENABLED_BIT)
#define MPU_NEVER_EXECUTE_BIT               0x1000
#define MPU_PERMISSION_FIELD_MASK           0x700
#define MPU_SUBREGION_DISABLE_FIELD_MASK    0xFF00
#define MPU_TYPE_DREGION_FIELD_OFFSET       8
#define MPU_TYPE_DREGION_FIELD_MASK         (0xFF << MPU_TYPE_DREGION_FIELD_OFFSET)
#define MPU_SUBREGION_DISABLE_FIELD_OFFSET  8
#define MPU_FAULT_STATUS_BITS_FIELD_MASK    0x40f
#define MPU_FAULT_STATUS_WRITE_FIELD_OFFSET 11
#define MPU_FAULT_STATUS_WRITE_FIELD_MASK   (1 << 11)

#define BACKGROUND_FAULT_STATUS_BITS        0b0000
#define PERMISSION_FAULT_STATUS_BITS        0b1101

#define MAX_TCM_REGIONS 4

typedef struct DisasContext {
    DisasContextBase base;
    /* Nonzero if this instruction has been conditionally skipped.  */
    int condjmp;
    /* The label that will be jumped to when the instruction is skipped.  */
    int condlabel;
    /* Thumb-2 condtional execution bits.  */
    int condexec_mask;
    int condexec_cond;
    int thumb;
    TTable *cp_regs;
    int user;
    int vfp_enabled;
    int vec_len;
    int vec_stride;
} DisasContext;

/* ARM-specific interrupt pending bits.  */
#define CPU_INTERRUPT_FIQ CPU_INTERRUPT_TGT_EXT_1

typedef void ARMWriteCPFunc(void *opaque, int cp_info, int srcreg, int operand, uint32_t value);
typedef uint32_t ARMReadCPFunc(void *opaque, int cp_info, int dstreg, int operand);

#define NB_MMU_MODES 2

/* We currently assume float and double are IEEE single and double
   precision respectively.
   Doing runtime conversions is tricky because VFP registers may contain
   integer values (eg. as the result of a FTOSI instruction).
   s<2n> maps to the least significant half of d<n>
   s<2n+1> maps to the most significant half of d<n>
 */

#if defined(TARGET_ARM32)
#define CPU_PC(env) env->regs[15]
#elif defined(TARGET_ARM64)
#define CPU_PC(env) _CPU_PC(env)
inline uint64_t _CPU_PC(CPUState *env) {
    // TODO: check which mode we are in and optionally return env->regs[15]
    return env->pc;
}
#endif

// +---------------------------------------+
// | ALL FIELDS WHICH STATE MUST BE STORED |
// | DURING SERIALIZATION SHOULD BE PLACED |
// | BEFORE >CPU_COMMON< SECTION.          |
// +---------------------------------------+
typedef struct CPUState {
    /* Regs for 32-bit current mode.  */
    uint32_t regs[16];
#ifdef TARGET_ARM64
    /* Regs for 64-bit mode. */
    uint64_t xregs[32];
    uint64_t pc;
#endif
    /* Frequently accessed CPSR bits are stored separately for efficiently.
       This contains all the other bits.  Use cpsr_{read,write} to access
       the whole CPSR.  */
    uint32_t uncached_cpsr;
    uint32_t spsr;

    /* Banked registers.  */
    uint32_t banked_spsr[6];
    uint32_t banked_r13[6];
    uint32_t banked_r14[6];

    /* These hold r8-r12.  */
    uint32_t usr_regs[5];
    uint32_t fiq_regs[5];

    /* cpsr flag cache for faster execution */
    uint32_t CF;            /* 0 or 1 */
    uint32_t VF;            /* V is the bit 31. All other bits are undefined */
    uint32_t NF;            /* N is bit 31. All other bits are undefined.  */
    uint32_t ZF;            /* Z set if zero.  */
    uint32_t QF;            /* 0 or 1 */
    uint32_t GE;            /* cpsr[19:16] */
    uint32_t thumb;         /* cpsr[5]. 0 = arm mode, 1 = thumb mode. */
    uint32_t condexec_bits; /* IT bits.  cpsr[15:10,26:25].  */

    bool wfe;
    bool sev_pending;

    /* System control coprocessor (cp15) */
    struct {
        uint32_t c0_cpuid;
        uint32_t c0_cachetype;
        uint32_t c0_tcmtype;     /* TCM type. */
        uint32_t c0_ccsid[16];   /* Cache size.  */
        uint32_t c0_clid;        /* Cache level.  */
        uint32_t c0_cssel;       /* Cache size selection.  */
        uint32_t c0_c1[8];       /* Feature registers.  */
        uint32_t c0_c2[8];       /* Instruction set registers.  */
        uint32_t c1_sys;         /* System control register.  */
        uint32_t c1_coproc;      /* Coprocessor access register.  */
        uint32_t c1_xscaleauxcr; /* XScale auxiliary control register.  */
        uint32_t c2_base0;       /* MMU translation table base 0.  */
        uint32_t c2_base1;       /* MMU translation table base 1.  */
        uint32_t c2_control;     /* MMU translation table base control.  */
        uint32_t c2_mask;        /* MMU translation table base selection mask.  */
        uint32_t c2_base_mask;   /* MMU translation table base 0 mask. */
        uint32_t c2_data;        /* MPU data cachable bits.  */
        uint32_t c2_insn;        /* MPU instruction cachable bits.  */
        uint32_t c3;             /* MMU domain access control register
                                    MPU write buffer control.  */
        uint32_t c5_insn;        /* Fault status registers.  */
        uint32_t c5_data;
        uint32_t c6_insn;        /* Fault address registers.  */
        uint32_t c6_data;
        uint32_t c6_addr;
        uint32_t c6_base_address[MAX_MPU_REGIONS]; /* MPU base register.  */
        uint32_t c6_size_and_enable[MAX_MPU_REGIONS]; /* MPU size/enable register.  */
        uint32_t c6_access_control[MAX_MPU_REGIONS]; /* MPU access control register. */
        uint32_t c6_subregion_disable[MAX_MPU_REGIONS]; /* MPU subregion disable mask. This is not a hardware register */
        uint32_t c6_region_number;
        uint32_t c7_par;         /* Translation result. */
        uint32_t c9_insn;        /* Cache lockdown registers.  */
        uint32_t c9_tcmregion[2][MAX_TCM_REGIONS]; /* TCM Region Registers */
        uint32_t c9_tcmsel;      /* TCM Selection Registers */
        uint32_t c9_data;
        uint32_t c9_pmcr;        /* performance monitor control register */
        uint32_t c9_pmcnten;     /* perf monitor counter enables */
        uint32_t c9_pmovsr;      /* perf monitor overflow status */
        uint32_t c9_pmxevtyper;  /* perf monitor event type */
        uint32_t c9_pmuserenr;   /* perf monitor user enable */
        uint32_t c9_pminten;     /* perf monitor interrupt enables */
        uint32_t c12_vbar;       /* vector base address register, security extensions*/
        uint32_t c13_fcse;       /* FCSE PID.  */
        uint32_t c13_context;    /* Context ID.  */
        uint32_t c13_tls1;       /* User RW Thread register.  */
        uint32_t c13_tls2;       /* User RO Thread register.  */
        uint32_t c13_tls3;       /* Privileged Thread register.  */
        uint32_t c15_cpar;       /* XScale Coprocessor Access Register */
        uint32_t c15_ticonfig;   /* TI925T configuration byte.  */
        uint32_t c15_i_max;      /* Maximum D-cache dirty line index.  */
        uint32_t c15_i_min;      /* Minimum D-cache dirty line index.  */
        uint32_t c15_threadid;   /* TI debugger thread-ID.  */
    } cp15;

#ifdef TARGET_PROTO_ARM_M
    struct {
        uint32_t other_sp;
        uint32_t vecbase;
        uint32_t basepri;
        uint32_t control;
        uint32_t fault_status;
        uint32_t current_sp;
        uint32_t exception;
        uint32_t faultmask;
        uint32_t pending_exception;
        uint32_t cpacr;
        uint32_t fpccr;
        uint32_t fpcar;
        uint32_t fpdscr;
        /* msplim/psplim are armv8-m specific */
        uint32_t msplim;
        uint32_t psplim;
        uint32_t handler_mode;
    } v7m;

    /* PMSAv8 MPU */
    struct {
        uint32_t ctrl;
        uint32_t rnr;
        uint32_t rbar[MAX_MPU_REGIONS];
        uint32_t rlar[MAX_MPU_REGIONS];
        uint32_t mair[2]; /* The number of these registers is *not* configurable */
    } pmsav8;
#endif

    /* Thumb-2 EE state.  */
    uint32_t teecr;
    uint32_t teehbr;

    /* Internal CPU feature flags.  */
    uint32_t features;

    /* VFP coprocessor state.  */
    struct {
        float64 regs[32];

        uint32_t xregs[16];
        /* We store these fpcsr fields separately for convenience.  */
        int vec_len;
        int vec_stride;

        /* scratch space when Tn are not sufficient.  */
        uint32_t scratch[8];

        /* fp_status is the "normal" fp status. standard_fp_status retains
         * values corresponding to the ARM "Standard FPSCR Value", ie
         * default-NaN, flush-to-zero, round-to-nearest and is used by
         * any operations (generally Neon) which the architecture defines
         * as controlled by the standard FPSCR value rather than the FPSCR.
         *
         * To avoid having to transfer exception bits around, we simply
         * say that the FPSCR cumulative exception flags are the logical
         * OR of the flags in the two fp statuses. This relies on the
         * only thing which needs to read the exception flags being
         * an explicit FPSCR read.
         */
        float_status fp_status;
        float_status standard_fp_status;
        #ifdef TARGET_PROTO_ARM_M
        int32_t fpu_interrupt_irq_number;
        #endif
    } vfp;
    uint32_t exclusive_addr;
    uint32_t exclusive_val;
    uint32_t exclusive_high;

    int32_t sev_on_pending;

    /* iwMMXt coprocessor state.  */
    struct {
        uint64_t regs[16];
        uint64_t val;

        uint32_t cregs[16];
    } iwmmxt;

    uint32_t number_of_mpu_regions;

    CPU_COMMON

    /* These fields after the common ones so they are preserved on reset.  */

    TTable *cp_regs;

    /* Coprocessor IO used by peripherals */
    struct {
        ARMReadCPFunc *cp_read;
        ARMWriteCPFunc *cp_write;
        void *opaque;
    } cp[15];
} CPUState;

void switch_mode(CPUState *, int);

int cpu_handle_mmu_fault (CPUState *env, target_ulong address, int rw, int mmu_idx, int no_page_fault);

#define CPSR_M           (0x1f)
#define CPSR_T           (1 << 5)
#define CPSR_F           (1 << 6)
#define CPSR_I           (1 << 7)
#define CPSR_PRIMASK     1
#define CPSR_A           (1 << 8)
#define CPSR_E           (1 << 9)
#define CPSR_IT_2_7      (0xfc00)
#define CPSR_GE          (0xf << 16)
#define CPSR_RESERVED    (0xf << 20)
#define CPSR_J           (1 << 24)
#define CPSR_IT_0_1      (3 << 25)
#define CPSR_Q           (1 << 27)
#define CPSR_V           (1 << 28)
#define CPSR_C           (1 << 29)
#define CPSR_Z           (1 << 30)
#define CPSR_N           (1 << 31)
#define CPSR_NZCV        (CPSR_N | CPSR_Z | CPSR_C | CPSR_V)

#define CPSR_IT          (CPSR_IT_0_1 | CPSR_IT_2_7)
#define CACHED_CPSR_BITS (CPSR_T | CPSR_GE | CPSR_IT | CPSR_Q | CPSR_NZCV)
/* Bits writable in user mode.  */
#define CPSR_USER        (CPSR_NZCV | CPSR_Q | CPSR_GE)
/* Execution state bits.  MRS read as zero, MSR writes ignored.  */
#define CPSR_EXEC        (CPSR_T | CPSR_IT | CPSR_J)

/* Return the current CPSR value.  */
uint32_t cpsr_read(CPUState *env);
/* Set the CPSR.  Note that some bits of mask must be all-set or all-clear.  */
void cpsr_write(CPUState *env, uint32_t val, uint32_t mask);

#ifdef TARGET_PROTO_ARM_M
/* Return the current xPSR value.  */
static inline uint32_t xpsr_read(CPUState *env)
{
    int ZF;
    ZF = (env->ZF == 0);
    return (env->NF & 0x80000000) | (ZF << 30) | (env->CF << 29) | ((env->VF & 0x80000000) >> 3) | (env->QF << 27) |
           (env->thumb << 24) | ((env->condexec_bits & 3) << 25) | ((env->condexec_bits & 0xfc) << 8) | env->v7m.exception;
}

/* Set the xPSR.  Note that some bits of mask must be all-set or all-clear.  */
static inline void xpsr_write(CPUState *env, uint32_t val, uint32_t mask)
{
    if (mask & CPSR_NZCV) {
        env->ZF = (~val) & CPSR_Z;
        env->NF = val;
        env->CF = (val >> 29) & 1;
        env->VF = (val << 3) & 0x80000000;
    }
    if (mask & CPSR_Q) {
        env->QF = ((val & CPSR_Q) != 0);
    }
    if (mask & (1 << 24)) {
        env->thumb = ((val & (1 << 24)) != 0);
    }
    if (mask & CPSR_IT_0_1) {
        env->condexec_bits &= ~3;
        env->condexec_bits |= (val >> 25) & 3;
    }
    if (mask & CPSR_IT_2_7) {
        env->condexec_bits &= 3;
        env->condexec_bits |= (val >> 8) & 0xfc;
    }
    if (mask & 0x1ff) {
        env->v7m.exception = val & 0x1ff;
    }
}

void vfp_trigger_exception();
#endif

/* Return the current FPSCR value.  */
uint32_t vfp_get_fpscr(CPUState *env);
void vfp_set_fpscr(CPUState *env, uint32_t val);

enum arm_cpu_mode {
    ARM_CPU_MODE_USR = 0x10,
    ARM_CPU_MODE_FIQ = 0x11,
    ARM_CPU_MODE_IRQ = 0x12,
    ARM_CPU_MODE_SVC = 0x13,
    ARM_CPU_MODE_ABT = 0x17,
    ARM_CPU_MODE_UND = 0x1b,
    ARM_CPU_MODE_SYS = 0x1f
};

/* VFP system registers.  */
#define ARM_VFP_FPSID    0
#define ARM_VFP_FPSCR    1
#define ARM_VFP_MVFR1    6
#define ARM_VFP_MVFR0    7
#define ARM_VFP_FPEXC    8
#define ARM_VFP_FPINST   9
#define ARM_VFP_FPINST2  10

/* FP fields.  */
#define ARM_CONTROL_FPCA     2
#define ARM_FPCCR_LSPACT     0
#define ARM_FPCCR_LSPEN      30
#define ARM_FPCCR_ASPEN      31
#define ARM_EXC_RETURN_NFPCA 4
#define ARM_VFP_FPEXC_FPUEN  30

#define ARM_CONTROL_FPCA_MASK      (1 << ARM_CONTROL_FPCA)
#define ARM_FPCCR_LSPACT_MASK      (1 << ARM_FPCCR_LSPACT)
#define ARM_FPCCR_LSPEN_MASK       (1 << ARM_FPCCR_LSPEN)
#define ARM_FPCCR_ASPEN_MASK       (1 << ARM_FPCCR_ASPEN)
#define ARM_EXC_RETURN_NFPCA_MASK  (1 << ARM_EXC_RETURN_NFPCA)
#define ARM_VFP_FPEXC_FPUEN_MASK   (1 << ARM_VFP_FPEXC_FPUEN)
#define ARM_FPDSCR_VALUES_MASK     0x07c00000
#define ARM_EXC_RETURN_HANDLER_MODE_MASK 0x8

#define ARM_CPACR_CP10          20
#define ARM_CPACR_CP10_MASK     (3 << ARM_CPACR_CP10)

#define ARM_CPN_ACCESS_NONE     0
#define ARM_CPN_ACCESS_PRIV     1
#define ARM_CPN_ACCESS_FULL     3

/* iwMMXt coprocessor control registers.  */
#define ARM_IWMMXT_wCID  0
#define ARM_IWMMXT_wCon  1
#define ARM_IWMMXT_wCSSF 2
#define ARM_IWMMXT_wCASF 3
#define ARM_IWMMXT_wCGR0 8
#define ARM_IWMMXT_wCGR1 9
#define ARM_IWMMXT_wCGR2 10
#define ARM_IWMMXT_wCGR3 11

enum arm_features {
    ARM_FEATURE_VFP,
    ARM_FEATURE_AUXCR,  /* ARM1026 Auxiliary control register.  */
    ARM_FEATURE_XSCALE, /* Intel XScale extensions.  */
    ARM_FEATURE_IWMMXT, /* Intel iwMMXt extension.  */
    ARM_FEATURE_V6,
    ARM_FEATURE_V6K,
    ARM_FEATURE_V7,
    ARM_FEATURE_THUMB2,
    ARM_FEATURE_MPU,    /* Only has Memory Protection Unit, not full MMU.  */
    ARM_FEATURE_VFP3,
    ARM_FEATURE_VFP_FP16,
    ARM_FEATURE_NEON,
    ARM_FEATURE_THUMB_DIV, /* divide supported in Thumb encoding */
    ARM_FEATURE_OMAPCP,    /* OMAP specific CP15 ops handling.  */
    ARM_FEATURE_THUMB2EE,
    ARM_FEATURE_V7MP,      /* v7 Multiprocessing Extensions */
    ARM_FEATURE_V4T,
    ARM_FEATURE_V5,
    ARM_FEATURE_STRONGARM,
    ARM_FEATURE_VAPA,    /* cp15 VA to PA lookups */
    ARM_FEATURE_ARM_DIV, /* divide supported in ARM encoding */
    ARM_FEATURE_VFP4,    /* VFPv4 (implies that NEON is v2) */
    ARM_FEATURE_GENERIC_TIMER,
    ARM_FEATURE_V8,      /* implies PMSAv8 MPU */
    ARM_FEATURE_PMSA,
};

static inline int arm_feature(CPUState *env, int feature)
{
    return (env->features & (1u << feature)) != 0;
}

/* Interface between CPU and Interrupt controller.  */

void cpu_arm_set_cp_io(CPUState *env, int cpnum, ARMReadCPFunc *cp_read, ARMWriteCPFunc *cp_write, void *opaque);

/* Does the core conform to the the "MicroController" profile. e.g. Cortex-M3.
   Note the M in older cores (eg. ARM7TDMI) stands for Multiply. These are
   conventional cores (ie. Application or Realtime profile).  */

#define ARM_CPUID(env) (env->cp15.c0_cpuid)

// MIDR, Main ID Register value
#define ARM_CPUID_ARM1026           0x4106a262
#define ARM_CPUID_ARM926            0x41069265
#define ARM_CPUID_ARM946            0x41059461
#define ARM_CPUID_TI915T            0x54029152
#define ARM_CPUID_TI925T            0x54029252
#define ARM_CPUID_SA1100            0x4401A11B
#define ARM_CPUID_SA1110            0x6901B119
#define ARM_CPUID_PXA250            0x69052100
#define ARM_CPUID_PXA255            0x69052d00
#define ARM_CPUID_PXA260            0x69052903
#define ARM_CPUID_PXA261            0x69052d05
#define ARM_CPUID_PXA262            0x69052d06
#define ARM_CPUID_PXA270            0x69054110
#define ARM_CPUID_PXA270_A0         0x69054110
#define ARM_CPUID_PXA270_A1         0x69054111
#define ARM_CPUID_PXA270_B0         0x69054112
#define ARM_CPUID_PXA270_B1         0x69054113
#define ARM_CPUID_PXA270_C0         0x69054114
#define ARM_CPUID_PXA270_C5         0x69054117
#define ARM_CPUID_ARM1136           0x4117b363
#define ARM_CPUID_ARM1136_R2        0x4107b362
#define ARM_CPUID_ARM1176           0x410fb767
#define ARM_CPUID_ARM11MPCORE       0x410fb022
#define ARM_CPUID_CORTEXA8          0x410fc080
#define ARM_CPUID_CORTEXA9          0x410fc090
#define ARM_CPUID_CORTEXA15         0x412fc0f1
#define ARM_CPUID_CORTEXM3          0x410fc231
#define ARM_CPUID_CORTEXM33         0x411fd210
#define ARM_CPUID_CORTEXR5          0x410fc150
#define ARM_CPUID_CORTEXR5F         0x410fc151
#define ARM_CPUID_CORTEXR8          0x410fc183
#define ARM_CPUID_ANY               0xffffffff

/* The ARM MMU allows 1k pages.  */
/* ??? Linux doesn't actually use these, and they're deprecated in recent
   architecture revisions.  Maybe a configure option to disable them.  */
#define TARGET_PAGE_BITS            10

#define TARGET_PHYS_ADDR_SPACE_BITS 32
#define TARGET_VIRT_ADDR_SPACE_BITS 32

/* MMU modes definitions */
#define MMU_MODE0_SUFFIX            _kernel
#define MMU_MODE1_SUFFIX            _user
#define MMU_USER_IDX                1
static inline int cpu_mmu_index (CPUState *env)
{
    return (env->uncached_cpsr & CPSR_M) == ARM_CPU_MODE_USR ? 1 : 0;
}

#include "cpu-all.h"

enum mpu_result {
    MPU_SUCCESS = TRANSLATE_SUCCESS,
    MPU_PERMISSION_FAULT = TRANSLATE_FAIL,
    MPU_BACKGROUND_FAULT,
};

/* Bit usage in the TB flags field: */
#define ARM_TBFLAG_THUMB_SHIFT     0
#define ARM_TBFLAG_THUMB_MASK      (1 << ARM_TBFLAG_THUMB_SHIFT)
#define ARM_TBFLAG_VECLEN_SHIFT    1
#define ARM_TBFLAG_VECLEN_MASK     (0x7 << ARM_TBFLAG_VECLEN_SHIFT)
#define ARM_TBFLAG_VECSTRIDE_SHIFT 4
#define ARM_TBFLAG_VECSTRIDE_MASK  (0x3 << ARM_TBFLAG_VECSTRIDE_SHIFT)
#define ARM_TBFLAG_PRIV_SHIFT      6
#define ARM_TBFLAG_PRIV_MASK       (1 << ARM_TBFLAG_PRIV_SHIFT)
#define ARM_TBFLAG_VFPEN_SHIFT     7
#define ARM_TBFLAG_VFPEN_MASK      (1 << ARM_TBFLAG_VFPEN_SHIFT)
#define ARM_TBFLAG_CONDEXEC_SHIFT  8
#define ARM_TBFLAG_CONDEXEC_MASK   (0xff << ARM_TBFLAG_CONDEXEC_SHIFT)
/* Bits 31..16 are currently unused. */

/* some convenience accessor macros */
#define ARM_TBFLAG_THUMB(F) \
    (((F) & ARM_TBFLAG_THUMB_MASK) >> ARM_TBFLAG_THUMB_SHIFT)
#define ARM_TBFLAG_VECLEN(F) \
    (((F) & ARM_TBFLAG_VECLEN_MASK) >> ARM_TBFLAG_VECLEN_SHIFT)
#define ARM_TBFLAG_VECSTRIDE(F) \
    (((F) & ARM_TBFLAG_VECSTRIDE_MASK) >> ARM_TBFLAG_VECSTRIDE_SHIFT)
#define ARM_TBFLAG_PRIV(F) \
    (((F) & ARM_TBFLAG_PRIV_MASK) >> ARM_TBFLAG_PRIV_SHIFT)
#define ARM_TBFLAG_VFPEN(F) \
    (((F) & ARM_TBFLAG_VFPEN_MASK) >> ARM_TBFLAG_VFPEN_SHIFT)
#define ARM_TBFLAG_CONDEXEC(F) \
    (((F) & ARM_TBFLAG_CONDEXEC_MASK) >> ARM_TBFLAG_CONDEXEC_SHIFT)

static inline void cpu_get_tb_cpu_state(CPUState *env, target_ulong *pc, target_ulong *cs_base, int *flags)
{
    int privmode;
    *pc = CPU_PC(env);
    *cs_base = 0;
    *flags = (env->thumb << ARM_TBFLAG_THUMB_SHIFT) | (env->vfp.vec_len << ARM_TBFLAG_VECLEN_SHIFT) |
             (env->vfp.vec_stride << ARM_TBFLAG_VECSTRIDE_SHIFT) | (env->condexec_bits << ARM_TBFLAG_CONDEXEC_SHIFT);
#ifdef TARGET_PROTO_ARM_M
    privmode = !((env->v7m.exception == 0) && (env->v7m.control & 1));
#else
    privmode = (env->uncached_cpsr & CPSR_M) != ARM_CPU_MODE_USR;
#endif
    if (privmode) {
        *flags |= ARM_TBFLAG_PRIV_MASK;
    }
    if ((env->vfp.xregs[ARM_VFP_FPEXC] & ARM_VFP_FPEXC_FPUEN_MASK)
#ifdef TARGET_PROTO_ARM_M
        && (privmode || ((env->v7m.cpacr & ARM_CPACR_CP10_MASK) >> ARM_CPACR_CP10) == ARM_CPN_ACCESS_FULL)
#endif
    ) {
        *flags |= ARM_TBFLAG_VFPEN_MASK;
    }
}

static inline bool is_cpu_event_pending(CPUState *env)
{
    // The execution of an SEV instruction on any processor in the multiprocessor system.
    bool event_pending = env->sev_pending;
#ifdef TARGET_PROTO_ARM_M
    // Any exception entering the Pending state if SEVONPEND in the System Control Register is set.
    event_pending |= env->sev_on_pending && tlib_nvic_get_pending_masked_irq();
    // An asynchronous exception at a priority that preempts any currently active exceptions.
    event_pending |= is_interrupt_pending(env, CPU_INTERRUPT_HARD);
#else
    uint32_t cpsr = cpsr_read(env);
    // An IRQ interrupt (even when CPSR I-bit is set, some implementations check this mask)
    event_pending |= is_interrupt_pending(env, CPU_INTERRUPT_HARD);
    // An FIQ interrupt (even when CPSR F-bit is set, some implementations check this mask)
    event_pending |= is_interrupt_pending(env, CPU_INTERRUPT_FIQ);
    // An asynchronous abort (not when masked by the CPSR A-bit)
    event_pending |= is_interrupt_pending(env, CPU_INTERRUPT_EXITTB) && (cpsr & CPSR_A);
    // Events could be sent by implementation defined mechanisms, e.g.:
    // A CP15 maintenance request broadcast by other processors.
    // Virtual Interrupts (HCR), Hypervisor mode isn't implemented
    // TODO
#endif

    return event_pending;
}

static inline bool cpu_has_work(CPUState *env)
{
    if (env->wfe && is_cpu_event_pending(env)) {
        env->sev_pending = 0;
        env->wfe = 0;
    }

    if (env->wfi) {
        bool has_work = false;
#ifndef TARGET_PROTO_ARM_M
        has_work = is_interrupt_pending(env, CPU_INTERRUPT_FIQ | CPU_INTERRUPT_HARD | CPU_INTERRUPT_EXITTB);
#else
        has_work = tlib_nvic_get_pending_masked_irq() != 0;
#endif
        if(has_work) {
            env->wfi = 0;
        }
    }

    return !(env->wfe || env->wfi);
}

#include "exec-all.h"

static inline void cpu_pc_from_tb(CPUState *env, TranslationBlock *tb)
{
#if defined(TARGET_ARM32)
    env->regs[15] = tb->pc;
#elif defined(TARGET_ARM64)
    // TODO: check mode
    env->pc = tb->pc;
#endif
}

void do_v7m_exception_exit(CPUState *env);

static inline void find_pending_irq_if_primask_unset(CPUState *env)
{
#ifdef TARGET_PROTO_ARM_M
    if(!(env->uncached_cpsr & CPSR_PRIMASK))
    {
        tlib_nvic_find_pending_irq();
    }
#endif
}

#endif
