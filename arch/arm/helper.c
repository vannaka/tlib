#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "helper.h"
#include "host-utils.h"
#include "infrastructure.h"
#include "arch_callbacks.h"

#define ARM_ARCHITECTURE_MASK (0xFF00FFF0)

static uint32_t cortexa15_cp15_c0_c1[8] = {
    0x00001131, 0x00011011, 0x02010555, 0x00000000, 0x10201105, 0x20000000, 0x01240000, 0x02102211
};

static uint32_t cortexr5_cp15_c0_c1[8] = {
    0x00000131, 0x00000001, 0x00010400, 0x00000000, 0x00110130, 0x00000000, 0x01200000, 0x00000211
};

static uint32_t cortexr8_cp15_c0_c1[8] = {
    0x00000131, 0x00000001, 0x00010404, 0x00000000, 0x00210030, 0x00000000, 0x01200000, 0x00002111
};

static uint32_t cortexa15_cp15_c0_c2[8] = {
    0x02101110, 0x13112111, 0x21232041, 0x11112131, 0x10011142, 0, 0, 0
};

// since Cortex-R5, r1p0
static uint32_t cortexr5_cp15_c0_c2[8] = {
    0x02101111, 0x13112111, 0x21232141, 0x01112131, 0x00010142, 0, 0, 0
};

static uint32_t cortexr8_cp15_c0_c2[8] = {
    0x02101111, 0x13112111, 0x21232141, 0x01112131, 0x00010142, 0, 0, 0
};

static uint32_t cortexa9_cp15_c0_c1[8] =
{ 0x1031, 0x11, 0x000, 0, 0x00100103, 0x20000000, 0x01230000, 0x00002111 };

static uint32_t cortexa9_cp15_c0_c2[8] =
{ 0x00101111, 0x13112111, 0x21232041, 0x11112131, 0x00111142, 0, 0, 0 };

static uint32_t cortexa8_cp15_c0_c1[8] =
{ 0x1031, 0x11, 0x400, 0, 0x31100003, 0x20000000, 0x01202000, 0x11 };

static uint32_t cortexa8_cp15_c0_c2[8] =
{ 0x00101111, 0x12112111, 0x21232031, 0x11112131, 0x00111142, 0, 0, 0 };

static uint32_t mpcore_cp15_c0_c1[8] =
{ 0x111, 0x1, 0, 0x2, 0x01100103, 0x10020302, 0x01222000, 0 };

static uint32_t mpcore_cp15_c0_c2[8] =
{ 0x00100011, 0x12002111, 0x11221011, 0x01102131, 0x141, 0, 0, 0 };

static uint32_t arm1136_cp15_c0_c1[8] =
{ 0x111, 0x1, 0x2, 0x3, 0x01130003, 0x10030302, 0x01222110, 0 };

static uint32_t arm1136_cp15_c0_c2[8] =
{ 0x00140011, 0x12002111, 0x11231111, 0x01102131, 0x141, 0, 0, 0 };

static uint32_t arm1176_cp15_c0_c1[8] =
{ 0x111, 0x11, 0x33, 0, 0x01130003, 0x10030302, 0x01222100, 0 };

static uint32_t arm1176_cp15_c0_c2[8] =
{ 0x0140011, 0x12002111, 0x11231121, 0x01102131, 0x01141, 0, 0, 0 };

static uint32_t cpu_arm_find_by_name(const char *name);
static int get_phys_addr(CPUState *env, uint32_t address,
                                int access_type, int is_user,
                                uint32_t *phys_ptr, int *prot,
                                target_ulong *page_size, int no_page_fault);

static inline void set_feature(CPUState *env, int feature)
{
    env->features |= 1u << feature;
}

static void cpu_reset_model_id(CPUState *env, uint32_t id)
{
    env->cp15.c0_cpuid = id;
    switch (id) {
    case ARM_CPUID_ARM926:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_VFP);
        env->vfp.xregs[ARM_VFP_FPSID] = 0x41011090;
        env->cp15.c0_cachetype = 0x1dd20d2;
        env->cp15.c1_sys = 0x00090078;
        break;
    case ARM_CPUID_ARM946:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_MPU);
        env->cp15.c0_cachetype = 0x0f004006;
        env->cp15.c1_sys = 0x00000078;
        break;
    case ARM_CPUID_ARM1026:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_VFP);
        set_feature(env, ARM_FEATURE_AUXCR);
        env->vfp.xregs[ARM_VFP_FPSID] = 0x410110a0;
        env->cp15.c0_cachetype = 0x1dd20d2;
        env->cp15.c1_sys = 0x00090078;
        break;
    case ARM_CPUID_ARM1136:
        /* This is the 1136 r1, which is a v6K core */
        set_feature(env, ARM_FEATURE_V6K);
    /* Fall through */
    case ARM_CPUID_ARM1136_R2:
        /* What qemu calls "arm1136_r2" is actually the 1136 r0p2, ie an
         * older core than plain "arm1136". In particular this does not
         * have the v6K features.
         */
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_VFP);
        set_feature(env, ARM_FEATURE_AUXCR);
        /* These ID register values are correct for 1136 but may be wrong
         * for 1136_r2 (in particular r0p2 does not actually implement most
         * of the ID registers).
         */
        env->vfp.xregs[ARM_VFP_FPSID] = 0x410120b4;
        env->vfp.xregs[ARM_VFP_MVFR0] = 0x11111111;
        env->vfp.xregs[ARM_VFP_MVFR1] = 0x00000000;
        memcpy(env->cp15.c0_c1, arm1136_cp15_c0_c1, 8 * sizeof(uint32_t));
        memcpy(env->cp15.c0_c2, arm1136_cp15_c0_c2, 8 * sizeof(uint32_t));
        env->cp15.c0_cachetype = 0x1dd20d2;
        env->cp15.c1_sys = 0x00050078;
        break;
    case ARM_CPUID_ARM1176:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_V6K);
        set_feature(env, ARM_FEATURE_VFP);
        set_feature(env, ARM_FEATURE_AUXCR);
        set_feature(env, ARM_FEATURE_VAPA);
        env->vfp.xregs[ARM_VFP_FPSID] = 0x410120b5;
        env->vfp.xregs[ARM_VFP_MVFR0] = 0x11111111;
        env->vfp.xregs[ARM_VFP_MVFR1] = 0x00000000;
        memcpy(env->cp15.c0_c1, arm1176_cp15_c0_c1, 8 * sizeof(uint32_t));
        memcpy(env->cp15.c0_c2, arm1176_cp15_c0_c2, 8 * sizeof(uint32_t));
        env->cp15.c0_cachetype = 0x1dd20d2;
        env->cp15.c1_sys = 0x00050078;
        break;
    case ARM_CPUID_ARM11MPCORE:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_V6K);
        set_feature(env, ARM_FEATURE_VFP);
        set_feature(env, ARM_FEATURE_AUXCR);
        set_feature(env, ARM_FEATURE_VAPA);
        env->vfp.xregs[ARM_VFP_FPSID] = 0x410120b4;
        env->vfp.xregs[ARM_VFP_MVFR0] = 0x11111111;
        env->vfp.xregs[ARM_VFP_MVFR1] = 0x00000000;
        memcpy(env->cp15.c0_c1, mpcore_cp15_c0_c1, 8 * sizeof(uint32_t));
        memcpy(env->cp15.c0_c2, mpcore_cp15_c0_c2, 8 * sizeof(uint32_t));
        env->cp15.c0_cachetype = 0x1dd20d2;
        break;
    case ARM_CPUID_CORTEXA8:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_V6K);
        set_feature(env, ARM_FEATURE_V7);
        set_feature(env, ARM_FEATURE_AUXCR);
        set_feature(env, ARM_FEATURE_THUMB2);
        set_feature(env, ARM_FEATURE_VFP);
        set_feature(env, ARM_FEATURE_VFP3);
        set_feature(env, ARM_FEATURE_NEON);
        set_feature(env, ARM_FEATURE_THUMB2EE);
        env->vfp.xregs[ARM_VFP_FPSID] = 0x410330c0;
        env->vfp.xregs[ARM_VFP_MVFR0] = 0x11110222;
        env->vfp.xregs[ARM_VFP_MVFR1] = 0x00011100;
        memcpy(env->cp15.c0_c1, cortexa8_cp15_c0_c1, 8 * sizeof(uint32_t));
        memcpy(env->cp15.c0_c2, cortexa8_cp15_c0_c2, 8 * sizeof(uint32_t));
        env->cp15.c0_cachetype = 0x82048004;
        env->cp15.c0_clid = (1 << 27) | (2 << 24) | 3;
        env->cp15.c0_ccsid[0] = 0xe007e01a; /* 16k L1 dcache. */
        env->cp15.c0_ccsid[1] = 0x2007e01a; /* 16k L1 icache. */
        env->cp15.c0_ccsid[2] = 0xf0000000; /* No L2 icache. */
        env->cp15.c1_sys = 0x00c50078;
        break;
    case ARM_CPUID_CORTEXA9:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_V6K);
        set_feature(env, ARM_FEATURE_V7);
        set_feature(env, ARM_FEATURE_AUXCR);
        set_feature(env, ARM_FEATURE_THUMB2);
        set_feature(env, ARM_FEATURE_VFP);
        set_feature(env, ARM_FEATURE_VFP3);
        set_feature(env, ARM_FEATURE_VFP_FP16);
        set_feature(env, ARM_FEATURE_NEON);
        set_feature(env, ARM_FEATURE_THUMB2EE);
        /* Note that A9 supports the MP extensions even for
         * A9UP and single-core A9MP (which are both different
         * and valid configurations; we don't model A9UP).
         */
        set_feature(env, ARM_FEATURE_V7MP);
        env->vfp.xregs[ARM_VFP_FPSID] = 0x41034000; /* Guess */
        env->vfp.xregs[ARM_VFP_MVFR0] = 0x11110222;
        env->vfp.xregs[ARM_VFP_MVFR1] = 0x01111111;
        memcpy(env->cp15.c0_c1, cortexa9_cp15_c0_c1, 8 * sizeof(uint32_t));
        memcpy(env->cp15.c0_c2, cortexa9_cp15_c0_c2, 8 * sizeof(uint32_t));
        env->cp15.c0_cachetype = 0x80038003;
        env->cp15.c0_clid = (1 << 27) | (1 << 24) | 3;
        env->cp15.c0_ccsid[0] = 0xe00fe015; /* 16k L1 dcache. */
        env->cp15.c0_ccsid[1] = 0x200fe015; /* 16k L1 icache. */
        env->cp15.c1_sys = 0x00c50078;
        break;
    case ARM_CPUID_CORTEXA15:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_V6K);
        set_feature(env, ARM_FEATURE_THUMB2);
        set_feature(env, ARM_FEATURE_V7);
        set_feature(env, ARM_FEATURE_VFP4);
        set_feature(env, ARM_FEATURE_VFP_FP16);
        set_feature(env, ARM_FEATURE_NEON);
        set_feature(env, ARM_FEATURE_AUXCR);
        set_feature(env, ARM_FEATURE_GENERIC_TIMER);
        set_feature(env, ARM_FEATURE_THUMB2EE);
        set_feature(env, ARM_FEATURE_ARM_DIV);
        set_feature(env, ARM_FEATURE_V7MP);
        env->vfp.xregs[ARM_VFP_FPSID] = 0x410430f0;
        env->vfp.xregs[ARM_VFP_MVFR0] = 0x10110222;
        env->vfp.xregs[ARM_VFP_MVFR1] = 0x11111111;
        memcpy(env->cp15.c0_c1, cortexa15_cp15_c0_c1, 8 * sizeof(uint32_t));
        memcpy(env->cp15.c0_c2, cortexa15_cp15_c0_c2, 8 * sizeof(uint32_t));
        env->cp15.c0_cachetype = 0x8444c004;
        env->cp15.c0_clid = 0x0a200023;
        env->cp15.c0_ccsid[0] = 0x701fe00a; /* 32K L1 dcache */
        env->cp15.c0_ccsid[1] = 0x201fe00a; /* 32K L1 icache */
        env->cp15.c0_ccsid[2] = 0x711fe07a; /* 4096K L2 unified cache */
        env->cp15.c1_sys = 0x00c50078;
        break;
#ifdef TARGET_PROTO_ARM_M
    case ARM_CPUID_CORTEXM33:
        set_feature(env, ARM_FEATURE_V8);
        /* fallthrough */
    case ARM_CPUID_CORTEXM3:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_THUMB2);
        set_feature(env, ARM_FEATURE_V7);
        set_feature(env, ARM_FEATURE_THUMB_DIV);
        set_feature(env, ARM_FEATURE_MPU);

        // TODO cortex-m4, check if all should be on
        set_feature(env, ARM_FEATURE_VFP);
        set_feature(env, ARM_FEATURE_VFP3);
        set_feature(env, ARM_FEATURE_VFP_FP16);
        // cortex-m7?
        set_feature(env, ARM_FEATURE_VFP4);

        break;
#endif
    case ARM_CPUID_ANY: /* For userspace emulation.  */
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_V6K);
        set_feature(env, ARM_FEATURE_V7);
        set_feature(env, ARM_FEATURE_THUMB2);
        set_feature(env, ARM_FEATURE_VFP);
        set_feature(env, ARM_FEATURE_VFP3);
        set_feature(env, ARM_FEATURE_VFP4);
        set_feature(env, ARM_FEATURE_VFP_FP16);
        set_feature(env, ARM_FEATURE_NEON);
        set_feature(env, ARM_FEATURE_THUMB2EE);
        set_feature(env, ARM_FEATURE_ARM_DIV);
        set_feature(env, ARM_FEATURE_V7MP);
        break;
    case ARM_CPUID_TI915T:
    case ARM_CPUID_TI925T:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_OMAPCP);
        env->cp15.c0_cpuid = ARM_CPUID_TI925T; /* Depends on wiring.  */
        env->cp15.c0_cachetype = 0x5109149;
        env->cp15.c1_sys = 0x00000070;
        env->cp15.c15_i_max = 0x000;
        env->cp15.c15_i_min = 0xff0;
        break;
    case ARM_CPUID_PXA250:
    case ARM_CPUID_PXA255:
    case ARM_CPUID_PXA260:
    case ARM_CPUID_PXA261:
    case ARM_CPUID_PXA262:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_XSCALE);
        /* JTAG_ID is ((id << 28) | 0x09265013) */
        env->cp15.c0_cachetype = 0xd172172;
        env->cp15.c1_sys = 0x00000078;
        break;
    case ARM_CPUID_PXA270_A0:
    case ARM_CPUID_PXA270_A1:
    case ARM_CPUID_PXA270_B0:
    case ARM_CPUID_PXA270_B1:
    case ARM_CPUID_PXA270_C0:
    case ARM_CPUID_PXA270_C5:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_XSCALE);
        /* JTAG_ID is ((id << 28) | 0x09265013) */
        set_feature(env, ARM_FEATURE_IWMMXT);
        env->iwmmxt.cregs[ARM_IWMMXT_wCID] = 0x69051000 | 'Q';
        env->cp15.c0_cachetype = 0xd172172;
        env->cp15.c1_sys = 0x00000078;
        break;
    case ARM_CPUID_SA1100:
    case ARM_CPUID_SA1110:
        set_feature(env, ARM_FEATURE_STRONGARM);
        env->cp15.c1_sys = 0x00000070;
        break;
    case ARM_CPUID_CORTEXR5F:
        set_feature(env, ARM_FEATURE_VFP);
        set_feature(env, ARM_FEATURE_VFP3);
        set_feature(env, ARM_FEATURE_VFP_FP16);
        set_feature(env, ARM_FEATURE_NEON);

        env->vfp.xregs[ARM_VFP_FPSID] = 0x41023150;
        env->vfp.xregs[ARM_VFP_MVFR0] = 0x10110221;
        env->vfp.xregs[ARM_VFP_MVFR1] = 0x00000011;
        /* fallthrough */
    case ARM_CPUID_CORTEXR5:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_V7);
        set_feature(env, ARM_FEATURE_V7MP);
        set_feature(env, ARM_FEATURE_ARM_DIV); // not for rp0p0

        set_feature(env, ARM_FEATURE_THUMB2);
        set_feature(env, ARM_FEATURE_THUMB_DIV);

        set_feature(env, ARM_FEATURE_AUXCR);
        set_feature(env, ARM_FEATURE_GENERIC_TIMER);
        set_feature(env, ARM_FEATURE_PMSA);

        memcpy(env->cp15.c0_c1, cortexr5_cp15_c0_c1, 8 * sizeof(uint32_t));
        memcpy(env->cp15.c0_c2, cortexr5_cp15_c0_c2, 8 * sizeof(uint32_t));

        env->cp15.c0_cachetype = 0x80030003; // CTR
        env->cp15.c0_tcmtype = 0x00010001; // TCMTR
        env->cp15.c0_clid = 0x09200003; // CLIDR, for all caches implemented
        env->cp15.c0_ccsid[0] = 0xf01fe019; /* 32K L1 dcache */
        env->cp15.c0_ccsid[1] = 0xf01fe019; /* 32K L1 icache */
        env->cp15.c0_ccsid[2] = 0xf03fe019; /* 64K L2 unified cache */

        env->cp15.c1_sys = 0xe50878; // SCTLR
        env->cp15.c1_coproc |= (1 << 30 /* D32DIS */) | (1 << 31 /* ASEDIS */); // CPACR
        break;
    case ARM_CPUID_CORTEXR8:
        set_feature(env, ARM_FEATURE_V4T);
        set_feature(env, ARM_FEATURE_V5);
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_V7);
        set_feature(env, ARM_FEATURE_V7MP);
        set_feature(env, ARM_FEATURE_ARM_DIV);

        set_feature(env, ARM_FEATURE_VFP);
        set_feature(env, ARM_FEATURE_VFP3);
        set_feature(env, ARM_FEATURE_VFP_FP16);
        set_feature(env, ARM_FEATURE_NEON);

        set_feature(env, ARM_FEATURE_THUMB2);

        set_feature(env, ARM_FEATURE_AUXCR);
        set_feature(env, ARM_FEATURE_GENERIC_TIMER);
        set_feature(env, ARM_FEATURE_PMSA);

        env->vfp.xregs[ARM_VFP_FPSID] = 0x41023180;
        env->vfp.xregs[ARM_VFP_MVFR0] = 0x10110021 | /* if f64 supported */ 0x00000200;
        env->vfp.xregs[ARM_VFP_MVFR1] = 0x01000011;

        memcpy(env->cp15.c0_c1, cortexr8_cp15_c0_c1, 8 * sizeof(uint32_t));
        memcpy(env->cp15.c0_c2, cortexr8_cp15_c0_c2, 8 * sizeof(uint32_t));

        env->cp15.c0_cachetype = 0x8333C003; // CTR
        env->cp15.c0_tcmtype = 0x80010001; // TCMTR
        env->cp15.c0_clid = 0x09200003; // CLIDR, for cache implemented
        env->cp15.c0_ccsid[0] = 0x201fe019; /* 32K L1 dcache */
        env->cp15.c0_ccsid[1] = 0x201fe019; /* 32K L1 icache */
        env->cp15.c0_ccsid[2] = 0x203fe019; /* 64K L2 unified cache */
        env->cp15.c1_sys = 0xc50078; // SCTLR
        env->cp15.c1_coproc = 0xC0000000; // CPACR
        break;
    default:
        cpu_abort(env, "Bad CPU ID: %x\n", id);
        break;
    }

    /* Some features automatically imply others: */
    if (arm_feature(env, ARM_FEATURE_V7)) {
        set_feature(env, ARM_FEATURE_VAPA);
    }
    if (arm_feature(env, ARM_FEATURE_ARM_DIV)) {
        set_feature(env, ARM_FEATURE_THUMB_DIV);
    }
    if (arm_feature(env, ARM_FEATURE_PMSA)) {
        set_feature(env, ARM_FEATURE_MPU);
    }
}

void system_instructions_and_registers_reset(CPUState *env);
void system_instructions_and_registers_init(CPUState *env);

void cpu_reset(CPUState *env)
{
    uint32_t id = env->cp15.c0_cpuid;
    uint32_t number_of_mpu_regions = env->number_of_mpu_regions;
    memset(env, 0, offsetof(CPUState, breakpoints));
    if (id) {
        cpu_reset_model_id(env, id);
    }
    env->number_of_mpu_regions = number_of_mpu_regions;
    /* SVC mode with interrupts disabled.  */
    env->uncached_cpsr = ARM_CPU_MODE_SVC | CPSR_A | CPSR_F | CPSR_I;

#ifdef TARGET_PROTO_ARM_M
    /* On ARMv7-M the CPSR_I is the value of the PRIMASK register, and is
       clear at reset.  Initial SP and PC are loaded from ROM.  */
    env->v7m.fpccr = (env->v7m.fpccr & ~ARM_FPCCR_LSPACT_MASK) | ARM_FPCCR_ASPEN_MASK | ARM_FPCCR_LSPEN_MASK;
    env->uncached_cpsr &= ~CPSR_PRIMASK;
#endif

    env->vfp.xregs[ARM_VFP_FPEXC] = 0;
    env->cp15.c2_base_mask = 0xffffc000u;
    /* v7 performance monitor control register: same implementor
     * field as main ID register, and we implement no event counters.
     */
    env->cp15.c9_pmcr = (env->cp15.c0_cpuid & 0xff000000);

    set_flush_to_zero(1, &env->vfp.standard_fp_status);
    set_flush_inputs_to_zero(1, &env->vfp.standard_fp_status);
    set_default_nan_mode(1, &env->vfp.standard_fp_status);
    set_float_detect_tininess(float_tininess_before_rounding, &env->vfp.fp_status);
    set_float_detect_tininess(float_tininess_before_rounding, &env->vfp.standard_fp_status);

    system_instructions_and_registers_reset(env);
}

int cpu_init(const char *cpu_model)
{
    uint32_t id = cpu_arm_find_by_name(cpu_model);
    if (id == 0) {
        return -1;
    }
    cpu->cp15.c0_cpuid = id;

    // We need this to set CPU feature flags, before calling `system_instructions_and_registers_init`
    cpu_reset_model_id(env, id);

    system_instructions_and_registers_init(env);

    cpu_reset(cpu);
    return 0;
}

struct arm_cpu_t {
    uint32_t id;
    const char *name;
};

static const struct arm_cpu_t arm_cpu_names[] = {
    { ARM_CPUID_ARM926, "arm926" }, { ARM_CPUID_ARM946, "arm946" }, { ARM_CPUID_ARM1026, "arm1026" },
    { ARM_CPUID_ARM1136, "arm1136" }, { ARM_CPUID_ARM1136_R2, "arm1136-r2" }, { ARM_CPUID_ARM1176, "arm1176" },
    { ARM_CPUID_ARM11MPCORE, "arm11mpcore" }, { ARM_CPUID_CORTEXM3, "cortex-m0" }, { ARM_CPUID_CORTEXM3, "cortex-m0+" },
    { ARM_CPUID_CORTEXM3, "cortex-m1" }, { ARM_CPUID_CORTEXM3, "cortex-m3" }, { ARM_CPUID_CORTEXM3, "cortex-m4" },
    { ARM_CPUID_CORTEXM3, "cortex-m4f" }, { ARM_CPUID_CORTEXM3, "cortex-m7" }, { ARM_CPUID_CORTEXA8, "cortex-a8" },
    { ARM_CPUID_CORTEXA9, "cortex-a9" }, { ARM_CPUID_CORTEXA15, "cortex-a15" }, { ARM_CPUID_TI925T, "ti925t" },
    { ARM_CPUID_PXA250, "pxa250" }, { ARM_CPUID_SA1100,    "sa1100" }, { ARM_CPUID_SA1110,    "sa1110" },
    { ARM_CPUID_PXA255, "pxa255" }, { ARM_CPUID_PXA260, "pxa260" }, { ARM_CPUID_PXA261, "pxa261" },
    { ARM_CPUID_PXA262, "pxa262" }, { ARM_CPUID_PXA270, "pxa270" }, { ARM_CPUID_PXA270_A0, "pxa270-a0" },
    { ARM_CPUID_PXA270_A1, "pxa270-a1" }, { ARM_CPUID_PXA270_B0, "pxa270-b0" }, { ARM_CPUID_PXA270_B1, "pxa270-b1" },
    { ARM_CPUID_PXA270_C0, "pxa270-c0" }, { ARM_CPUID_PXA270_C5, "pxa270-c5" }, { ARM_CPUID_CORTEXM33, "cortex-m33" },
    { ARM_CPUID_CORTEXR5, "cortex-r5" }, { ARM_CPUID_CORTEXR5F, "cortex-r5f" }, { ARM_CPUID_CORTEXR8, "cortex-r8" },
    { ARM_CPUID_ANY, "any" }, { 0, NULL }
};

/* return 0 if not found */
static uint32_t cpu_arm_find_by_name(const char *name)
{
    int i;
    uint32_t id;

    id = 0;
    for (i = 0; arm_cpu_names[i].name; i++) {
        if (strcmp(name, arm_cpu_names[i].name) == 0) {
            id = arm_cpu_names[i].id;
            break;
        }
    }
    return id;
}

uint32_t cpsr_read(CPUState *env)
{
    int ZF;
    ZF = (env->ZF == 0);
    return env->uncached_cpsr | (env->NF & 0x80000000) | (ZF << 30) | (env->CF << 29) | ((env->VF & 0x80000000) >> 3) |
           (env->QF << 27) | (env->thumb << 5) | ((env->condexec_bits & 3) << 25) | ((env->condexec_bits & 0xfc) << 8) |
           (env->GE << 16);
}

void cpsr_write(CPUState *env, uint32_t val, uint32_t mask)
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
    if (mask & CPSR_T) {
        env->thumb = ((val & CPSR_T) != 0);
    }
    if (mask & CPSR_IT_0_1) {
        env->condexec_bits &= ~3;
        env->condexec_bits |= (val >> 25) & 3;
    }
    if (mask & CPSR_IT_2_7) {
        env->condexec_bits &= 3;
        env->condexec_bits |= (val >> 8) & 0xfc;
    }
    if (mask & CPSR_GE) {
        env->GE = (val >> 16) & 0xf;
    }

    if ((env->uncached_cpsr ^ val) & mask & CPSR_M) {
        switch_mode(env, val & CPSR_M);
    }
    mask &= ~CACHED_CPSR_BITS;
    env->uncached_cpsr = (env->uncached_cpsr & ~mask) | (val & mask);

    find_pending_irq_if_primask_unset(env);
}

/* Sign/zero extend */
uint32_t HELPER(sxtb16)(uint32_t x)
{
    uint32_t res;
    res = (uint16_t)(int8_t)x;
    res |= (uint32_t)(int8_t)(x >> 16) << 16;
    return res;
}

uint32_t HELPER(uxtb16)(uint32_t x)
{
    uint32_t res;
    res = (uint16_t)(uint8_t)x;
    res |= (uint32_t)(uint8_t)(x >> 16) << 16;
    return res;
}

uint32_t HELPER(clz)(uint32_t x)
{
    return clz32(x);
}

int32_t HELPER(sdiv)(int32_t num, int32_t den)
{
    if (den == 0) {
        return 0;
    }
    if (num == INT_MIN && den == -1) {
        return INT_MIN;
    }
    return num / den;
}

uint32_t HELPER(udiv)(uint32_t num, uint32_t den)
{
    if (den == 0) {
        return 0;
    }
    return num / den;
}

uint32_t HELPER(rbit)(uint32_t x)
{
    x =  ((x & 0xff000000) >> 24) | ((x & 0x00ff0000) >> 8) | ((x & 0x0000ff00) << 8) | ((x & 0x000000ff) << 24);
    x =  ((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4);
    x =  ((x & 0x88888888) >> 3) | ((x & 0x44444444) >> 1) | ((x & 0x22222222) << 1) | ((x & 0x11111111) << 3);
    return x;
}

uint32_t HELPER(abs)(uint32_t x)
{
    return ((int32_t)x < 0) ? -x : x;
}

/* Map CPU modes onto saved register banks.  */
static inline int bank_number (int mode)
{
    switch (mode) {
    case ARM_CPU_MODE_USR:
    case ARM_CPU_MODE_SYS:
        return 0;
    case ARM_CPU_MODE_SVC:
        return 1;
    case ARM_CPU_MODE_ABT:
        return 2;
    case ARM_CPU_MODE_UND:
        return 3;
    case ARM_CPU_MODE_IRQ:
        return 4;
    case ARM_CPU_MODE_FIQ:
        return 5;
    }
    cpu_abort(cpu, "Bad mode %x\n", mode);
    return -1;
}

void switch_mode(CPUState *env, int mode)
{
    int old_mode;
    int i;

    old_mode = env->uncached_cpsr & CPSR_M;
    if (mode == old_mode) {
        return;
    }

    if (old_mode == ARM_CPU_MODE_FIQ) {
        memcpy(env->fiq_regs, env->regs + 8, 5 * sizeof(uint32_t));
        memcpy(env->regs + 8, env->usr_regs, 5 * sizeof(uint32_t));
    } else if (mode == ARM_CPU_MODE_FIQ) {
        memcpy(env->usr_regs, env->regs + 8, 5 * sizeof(uint32_t));
        memcpy(env->regs + 8, env->fiq_regs, 5 * sizeof(uint32_t));
    }

    i = bank_number(old_mode);
    env->banked_r13[i] = env->regs[13];
    env->banked_r14[i] = env->regs[14];
    env->banked_spsr[i] = env->spsr;

    i = bank_number(mode);
    env->regs[13] = env->banked_r13[i];
    env->regs[14] = env->banked_r14[i];
    env->spsr = env->banked_spsr[i];
}

static inline void arm_announce_stack_change()
{
    if (unlikely(env->guest_profiler_enabled)) {
        tlib_announce_stack_change(CPU_PC(env), STACK_FRAME_ADD);
    }
}

#ifdef TARGET_PROTO_ARM_M
static int v7m_push(CPUState *env, uint32_t val)
{
    uint32_t phys_ptr = 0;
    target_ulong page_size = 0;
    int ret, prot = 0;
    uint32_t address = env->regs[13] - 4;
    ret = get_phys_addr(env, address, ACCESS_DATA_STORE, !in_privileged_mode(env), &phys_ptr, &prot, &page_size, false);
    if(ret == TRANSLATE_SUCCESS) {
        env->regs[13] = address;
        stl_phys(env->regs[13], val);
    } else {
        // Stacking error - MSTKERR
        env->cp15.c5_data = ret;
        if (arm_feature(env, ARM_FEATURE_V6)) {
            env->cp15.c5_data |= (1 << 11);
        }
        env->cp15.c6_data = address;
        env->v7m.fault_status |= MEM_FAULT_MSTKERR;
        return 1;
    }
    return 0;
}

static uint32_t v7m_pop(CPUState *env)
{
    uint32_t val;
    val = ldl_phys(env->regs[13]);
    env->regs[13] += 4;
    return val;
}

/* Switch to V7M main or process stack pointer.  */
static void switch_v7m_sp(CPUState *env, int process)
{
    uint32_t tmp;
    if (env->v7m.current_sp != process) {
        tmp = env->v7m.other_sp;
        env->v7m.other_sp = env->regs[13];
        env->regs[13] = tmp;
        env->v7m.current_sp = process;
    }
}

void do_v7m_exception_exit(CPUState *env)
{
    uint32_t type;
    uint32_t xpsr;

    /* Restore FAULTMASK to 0 only if the interrupt that we are exiting is not NMI */
    /* See ARMv7-M Architecture Reference Manual - B1.4.3 */
    if (env->v7m.exception != 2) {
        cpu->v7m.faultmask = 0;
    }

    type = env->regs[15];
    if (env->v7m.exception != 0) {
        tlib_nvic_complete_irq(env->v7m.exception);
    }

    if(env->interrupt_end_callback_enabled) {
        tlib_on_interrupt_end(env->exception_index);
    }

    /* Switch to the target stack.  */
    switch_v7m_sp(env, (type & 4) != 0);
    /* Pop registers.  */
    env->regs[0] = v7m_pop(env);
    env->regs[1] = v7m_pop(env);
    env->regs[2] = v7m_pop(env);
    env->regs[3] = v7m_pop(env);
    env->regs[12] = v7m_pop(env);
    env->regs[14] = v7m_pop(env);
    env->regs[15] = v7m_pop(env) & ~1;
    xpsr = v7m_pop(env);
    xpsr_write(env, xpsr, 0xfffffdff);
    /* Pop extended frame  */
    if (~type & ARM_EXC_RETURN_NFPCA_MASK) {
        if(env->v7m.fpccr & ARM_FPCCR_LSPACT_MASK) {
            /* FP state is still valid, pop space from stack  */
            env->v7m.fpccr ^= ARM_FPCCR_LSPACT_MASK;
            env->regs[13] += 0x48;
        } else {
            if (~env->vfp.xregs[ARM_VFP_FPEXC] & ARM_VFP_FPEXC_FPUEN_MASK) {
                /* FPU is disabled, revert SP and rise Usage Fault  */
                env->regs[13] -= 0x20;
                env->v7m.control &= ~ARM_CONTROL_FPCA_MASK;
                env->exception_index = EXCP_UDEF;
                cpu_loop_exit(env);
            }
            for (int i = 0; i < 8; ++i) {
                env->vfp.regs[i] = v7m_pop(env);
                env->vfp.regs[i] |= ((uint64_t)v7m_pop(env)) << 32;
            }
            vfp_set_fpscr(env, v7m_pop(env));
            /* Pop Reserved field  */
            env->regs[13] += 0x4;
        }
    }
    /* Set CONTROL.FPCA to NOT(type[ARM_EXC_RETURN_NFPCA])  */
    env->v7m.control ^= (env->v7m.control ^ ~type >> (ARM_EXC_RETURN_NFPCA - ARM_CONTROL_FPCA)) & ARM_CONTROL_FPCA_MASK;
    /* Undo stack alignment.  */
    if (xpsr & 0x200) {
        env->regs[13] |= 4;
    }
    /* ??? The exception return type specifies Thread/Handler mode.  However
       this is also implied by the xPSR value. Not sure what to do
       if there is a mismatch.  */
    /* ??? Likewise for mismatches between the CONTROL register and the stack
       pointer.  */
    if((type & ARM_EXC_RETURN_HANDLER_MODE_MASK) != 0) {
        env->v7m.handler_mode = false;
    } else {
        env->v7m.handler_mode = true;
    }
}

void HELPER(fp_lsp)(CPUState * env)
{
    /* Save FP state if FPCCR.LSPACT is set  */
    if (unlikely(env->v7m.fpccr & ARM_FPCCR_LSPACT_MASK)) {
        env->v7m.fpccr ^= ARM_FPCCR_LSPACT_MASK;
        uint32_t fpcar = env->v7m.fpcar & ~0x3;
        for (int i = 0; i < 8; ++i) {
            stl_phys(fpcar + i * 8, env->vfp.regs[i]);
            stl_phys(fpcar + i * 8 + 4, env->vfp.regs[i] >> 32);
        }
        uint32_t fpscr = vfp_get_fpscr(env);
        stl_phys(fpcar + 0x40, fpscr);
        /* Set default values from FPDSCR to FPSCR in new context */
        vfp_set_fpscr(env, (fpscr & ~ARM_FPDSCR_VALUES_MASK) | (env->v7m.fpdscr & ARM_FPDSCR_VALUES_MASK));
    }
}

static void do_interrupt_v7m(CPUState *env)
{
    uint32_t xpsr = xpsr_read(env);
    uint32_t lr;
    uint32_t addr;
    int nr;
    int stack_status = 0;

    if (arm_feature(env, ARM_FEATURE_V8)) {
        /* [31:7] PREFIX and RES1. 
         *
         * All SecureExtensions bits are set to their disabled state:
         * [6]: 0
         * [5]: 1
         * [0]: 0
         */
        lr = 0xffffffb0;

        /* Mode */
        if (env->v7m.handler_mode == 0 ) {
            lr |= 1<<3;
        }

        /* SPSEL */
        if (env->v7m.current_sp != 0) {
            lr |= 1<<2;
        }
    } else {
        lr = 0xfffffff1;
        if (env->v7m.exception == 0) {
            lr |= 0x8;
            lr |= (env->v7m.current_sp != 0) << 2;
        }
    }

    /* v7-M and v8-M share FP stack FP context active fields */
    if (env->v7m.control & ARM_CONTROL_FPCA_MASK) {
        lr ^= ARM_EXC_RETURN_NFPCA_MASK;
    }

    /* For exceptions we just mark as pending on the NVIC, and let that
       handle it.  */
    /* TODO: Need to escalate if the current priority is higher than the
       one we're raising.  */
    switch (env->exception_index) {
    case EXCP_UDEF:
        tlib_nvic_set_pending_irq(ARMV7M_EXCP_USAGE);
        env->v7m.fault_status |= USAGE_FAULT_UNDEFINSTR;
        return;
    case EXCP_NOCP:
        tlib_nvic_set_pending_irq(ARMV7M_EXCP_USAGE);
        env->v7m.fault_status |= USAGE_FAULT_NOPC;
        return;
    case EXCP_INVSTATE:
        tlib_nvic_set_pending_irq(ARMV7M_EXCP_USAGE);
        env->v7m.fault_status |= USAGE_FAULT_INVSTATE;
        return;
    case EXCP_SWI:
        tlib_nvic_set_pending_irq(ARMV7M_EXCP_SVC);
        return;
    case EXCP_PREFETCH_ABORT:
        /* Access violation */
        env->v7m.fault_status |= MEM_FAULT_IACCVIOL;
        tlib_nvic_set_pending_irq(ARMV7M_EXCP_MEM);
        return;
    case EXCP_DATA_ABORT:
        /* ACK faulting address and set Data acces violation */
        env->v7m.fault_status |= MEM_FAULT_MMARVALID | MEM_FAULT_DACCVIOL;
        tlib_nvic_set_pending_irq(ARMV7M_EXCP_MEM);
        return;
    case EXCP_BKPT:
        nr = lduw_code(env->regs[15]) & 0xff;
        if (nr == 0xab) {
            env->regs[15] += 2;
            env->regs[0] = tlib_do_semihosting();
            return;
        }
        tlib_nvic_set_pending_irq(ARMV7M_EXCP_DEBUG);
        return;
    case EXCP_IRQ:
        env->v7m.exception = tlib_nvic_acknowledge_irq();
        break;
    default:
        cpu_abort(env, "Unhandled exception 0x%x\n", env->exception_index);
        return; /* Never happens.  Keep compiler happy.  */
    }

    env->v7m.handler_mode = true;
    env->condexec_bits = 0;

    /* Align stack pointer.  */
    /* ??? Should do this if Configuration Control Register
       STACKALIGN bit is set or extended frame is being pushed.  */
    if (env->regs[13] & 4) {
        env->regs[13] -= 4;
        xpsr |= 0x200;
    }
    /* Push extended frame  */
    if (env->v7m.control & ARM_CONTROL_FPCA_MASK) {
        env->v7m.control &= ~ARM_CONTROL_FPCA_MASK;
        if (env->v7m.fpccr & ARM_FPCCR_LSPEN_MASK) {
            /* Set lazy FP state preservation  */
            env->v7m.fpccr |= ARM_FPCCR_LSPACT_MASK;
            env->regs[13] -= 0x48;
            env->v7m.fpcar = env->regs[13];
        } else {
            if (~env->vfp.xregs[ARM_VFP_FPEXC] & ARM_VFP_FPEXC_FPUEN_MASK) {
                /* FPU is disabled, revert SP and rise Usage Fault  */
                if (xpsr & 0x200) {
                    env->regs[13] |= 4;
                }
                env->exception_index = EXCP_UDEF;
                cpu_loop_exit(env);
            }
            /* Push Reserved field  */
            env->regs[13] -= 0x4;
            uint32_t fpscr = vfp_get_fpscr(env);
            v7m_push(env, fpscr);
            /* Set default values from FPDSCR to FPSCR in new context */
            vfp_set_fpscr(env, (fpscr & ~ARM_FPDSCR_VALUES_MASK) | (env->v7m.fpdscr & ARM_FPDSCR_VALUES_MASK));
            for (int i = 0; i < 8; ++i) {
                v7m_push(env, env->vfp.regs[8 - i]);
                v7m_push(env, env->vfp.regs[8 - i] >> 32);
            }
        }
    }
    /* Switch to the handler mode.  */
    stack_status |= v7m_push(env, xpsr);
    stack_status |= v7m_push(env, env->regs[15]);
    stack_status |= v7m_push(env, env->regs[14]);
    stack_status |= v7m_push(env, env->regs[12]);
    stack_status |= v7m_push(env, env->regs[3]);
    stack_status |= v7m_push(env, env->regs[2]);
    stack_status |= v7m_push(env, env->regs[1]);
    stack_status |= v7m_push(env, env->regs[0]);

    switch_v7m_sp(env, 0);

    env->uncached_cpsr &= ~CPSR_IT;

    find_pending_irq_if_primask_unset(env);

    env->regs[14] = lr;
    addr = ldl_phys(env->v7m.vecbase + env->v7m.exception * 4);
    env->regs[15] = addr & 0xfffffffe;
    env->thumb = addr & 1;
    if(stack_status) {
        do_v7m_exception_exit(env);
        env->exception_index = EXCP_DATA_ABORT;
        do_interrupt_v7m(env);
    }

    arm_announce_stack_change();
}
#endif

/* Handle a CPU exception.  */
void do_interrupt(CPUState *env)
{
    uint32_t addr;
    uint32_t mask;
    int new_mode;
    uint32_t offset;

    if(env->interrupt_begin_callback_enabled) {
        tlib_on_interrupt_begin(env->exception_index);
    }

#ifdef TARGET_PROTO_ARM_M
    do_interrupt_v7m(env);
    return;
#endif
    /* TODO: Vectored interrupt controller.  */
    switch (env->exception_index) {
    case EXCP_UDEF:
        new_mode = ARM_CPU_MODE_UND;
        addr = 0x04;
        mask = CPSR_I;
        if (env->thumb) {
            offset = 2;
        } else {
            offset = 4;
        }
        break;
    case EXCP_SWI:
        /* Check for semihosting interrupt.  */
        if (env->thumb) {
            mask = lduw_code(env->regs[15] - 2) & 0xff;
        } else {
            mask = ldl_code(env->regs[15] - 4) & 0xffffff;
        }
        /* Only intercept calls from privileged modes, to provide some
           semblance of security.  */
        if (((mask == 0x123456 && !env->thumb) || (mask == 0xab && env->thumb)) &&
            (env->uncached_cpsr & CPSR_M) != ARM_CPU_MODE_USR) {
            env->regs[0] = tlib_do_semihosting();
            return;
        }
        new_mode = ARM_CPU_MODE_SVC;
        addr = 0x08;
        mask = CPSR_I;
        /* The PC already points to the next instruction.  */
        offset = 0;
        break;
    case EXCP_BKPT:
        /* See if this is a semihosting syscall.  */
        mask = lduw_code(env->regs[15]) & 0xff;
        if (mask == 0xab && (env->uncached_cpsr & CPSR_M) != ARM_CPU_MODE_USR) {
            env->regs[15] += 2;
            env->regs[0] = tlib_do_semihosting();
            return;
        }
        env->cp15.c5_insn = 2;
        /* Go to prefetch abort.  */
        goto case_EXCP_PREFETCH_ABORT;
    case EXCP_PREFETCH_ABORT:
case_EXCP_PREFETCH_ABORT:
        new_mode = ARM_CPU_MODE_ABT;
        addr = 0x0c;
        mask = CPSR_A | CPSR_I;
        offset = 4;
        break;
    case EXCP_DATA_ABORT:
        new_mode = ARM_CPU_MODE_ABT;
        addr = 0x10;
        mask = CPSR_A | CPSR_I;
        offset = 8;
        break;
    case EXCP_IRQ:
        new_mode = ARM_CPU_MODE_IRQ;
        addr = 0x18;
        /* Disable IRQ and imprecise data aborts.  */
        mask = CPSR_A | CPSR_I;
        offset = 4;
        break;
    case EXCP_FIQ:
        new_mode = ARM_CPU_MODE_FIQ;
        addr = 0x1c;
        /* Disable FIQ, IRQ and imprecise data aborts.  */
        mask = CPSR_A | CPSR_I | CPSR_F;
        offset = 4;
        break;
    default:
        cpu_abort(env, "Unhandled exception 0x%x\n", env->exception_index);
        return; /* Never happens.  Keep compiler happy.  */
    }
    /* High vectors.  */
    if (env->cp15.c1_sys & (1 << 13)) {
        addr += 0xffff0000;
    }
    else {
        /* CPUs w/ Security Extensions allow for relocation of the
        * vector table. c12_vbar is initialized to zero so the
        * the following maintains compat w/ targets that don't have
        * Security Extensions.
        */
        addr += env->cp15.c12_vbar;
    }
    
    switch_mode(env, new_mode);
    env->spsr = cpsr_read(env);
    /* Clear IT bits.  */
    env->condexec_bits = 0;
    /* Switch to the new mode, and to the correct instruction set.  */
    env->uncached_cpsr = (env->uncached_cpsr & ~CPSR_M) | new_mode;
    env->uncached_cpsr |= mask;

    find_pending_irq_if_primask_unset(env);

    /* this is a lie, as the was no c1_sys on V4T/V5, but who cares
     * and we should just guard the thumb mode on V4 */
    if (arm_feature(env, ARM_FEATURE_V4T)) {
        env->thumb = (env->cp15.c1_sys & (1 << 30)) != 0;
    }
    env->regs[14] = env->regs[15] + offset;
    env->regs[15] = addr;
    set_interrupt_pending(env, CPU_INTERRUPT_EXITTB);

    arm_announce_stack_change();
}

/* Check section/page access permissions.
   Returns the page protection flags, or zero if the access is not
   permitted.  */
static inline int check_ap(CPUState *env, int ap, int domain, int access_type, int is_user)
{
    int prot_ro;

    if (domain == 3) {
        return PAGE_READ | PAGE_WRITE;
    }

    if (access_type == ACCESS_DATA_STORE) {
        prot_ro = 0;
    } else {
        prot_ro = PAGE_READ;
    }

    switch (ap) {
    case 0:
        if (access_type == ACCESS_DATA_STORE) {
            return 0;
        }
        switch ((env->cp15.c1_sys >> 8) & 3) {
        case 1:
            return is_user ? 0 : PAGE_READ;
        case 2:
            return PAGE_READ;
        default:
            return 0;
        }
    case 1:
        return is_user ? 0 : PAGE_READ | PAGE_WRITE;
    case 2:
        if (is_user) {
            return prot_ro;
        } else {
            return PAGE_READ | PAGE_WRITE;
        }
    case 3:
        return PAGE_READ | PAGE_WRITE;
    case 4: /* Reserved.  */
        return 0;
    case 5:
        return is_user ? 0 : prot_ro;
    case 6:
        return prot_ro;
    case 7:
        if (!arm_feature(env, ARM_FEATURE_V6K)) {
            return 0;
        }
        return prot_ro;
    default:
        abort();
    }
}

static uint32_t get_level1_table_address(CPUState *env, uint32_t address)
{
    uint32_t table;

    if (address & env->cp15.c2_mask) {
        table = env->cp15.c2_base1 & 0xffffc000;
    } else {
        table = env->cp15.c2_base0 & env->cp15.c2_base_mask;
    }

    table |= (address >> 18) & 0x3ffc;
    return table;
}

static int get_phys_addr_v5(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot,
                            target_ulong *page_size)
{
    int code;
    uint32_t table;
    uint32_t desc;
    int type;
    int ap;
    int domain;
    uint32_t phys_addr;

    /* Pagetable walk.  */
    /* Lookup l1 descriptor.  */
    table = get_level1_table_address(env, address);
    desc = ldl_phys(table);
    type = (desc & 3);
    domain = (env->cp15.c3 >> ((desc >> 4) & 0x1e)) & 3;
    if (type == 0) {
        /* Section translation fault.  */
        code = 5;
        goto do_fault;
    }
    if (domain == 0 || domain == 2) {
        if (type == 2) {
            code = 9;  /* Section domain fault.  */
        } else {
            code = 11; /* Page domain fault.  */
        }
        goto do_fault;
    }
    if (type == 2) {
        /* 1Mb section.  */
        phys_addr = (desc & 0xfff00000) | (address & 0x000fffff);
        ap = (desc >> 10) & 3;
        code = 13;
        *page_size = 1024 * 1024;
    } else {
        /* Lookup l2 entry.  */
        if (type == 1) {
            /* Coarse pagetable.  */
            table = (desc & 0xfffffc00) | ((address >> 10) & 0x3fc);
        } else {
            /* Fine pagetable.  */
            table = (desc & 0xfffff000) | ((address >> 8) & 0xffc);
        }
        desc = ldl_phys(table);
        switch (desc & 3) {
        case 0: /* Page translation fault.  */
            code = 7;
            goto do_fault;
        case 1: /* 64k page.  */
            phys_addr = (desc & 0xffff0000) | (address & 0xffff);
            ap = (desc >> (4 + ((address >> 13) & 6))) & 3;
            *page_size = 0x10000;
            break;
        case 2: /* 4k page.  */
            phys_addr = (desc & 0xfffff000) | (address & 0xfff);
            ap = (desc >> (4 + ((address >> 13) & 6))) & 3;
            *page_size = 0x1000;
            break;
        case 3: /* 1k page.  */
            if (type == 1) {
                if (arm_feature(env, ARM_FEATURE_XSCALE)) {
                    phys_addr = (desc & 0xfffff000) | (address & 0xfff);
                } else {
                    /* Page translation fault.  */
                    code = 7;
                    goto do_fault;
                }
            } else {
                phys_addr = (desc & 0xfffffc00) | (address & 0x3ff);
            }
            ap = (desc >> 4) & 3;
            *page_size = 0x400;
            break;
        default:
            /* Never happens, but compiler isn't smart enough to tell.  */
            abort();
        }
        code = 15;
    }
    *prot = check_ap(env, ap, domain, access_type, is_user);
    if (!*prot) {
        /* Access permission fault.  */
        goto do_fault;
    }
    *prot |= PAGE_EXEC;
    *phys_ptr = phys_addr;
    return TRANSLATE_SUCCESS;
do_fault:
    return code | (domain << 4); //TRANSLATE_FAIL
}

static int get_phys_addr_v6(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot,
                            target_ulong *page_size)
{
    int code;
    uint32_t table;
    uint32_t desc;
    uint32_t xn;
    int type;
    int ap;
    int domain;
    uint32_t phys_addr;

    /* Pagetable walk.  */
    /* Lookup l1 descriptor.  */
    table = get_level1_table_address(env, address);
    desc = ldl_phys(table);
    type = (desc & 3);
    if (type == 0) {
        /* Section translation fault.  */
        code = 5;
        domain = 0;
        goto do_fault;
    } else if (type == 2 && (desc & (1 << 18))) {
        /* Supersection.  */
        domain = 0;
    } else {
        /* Section or page.  */
        domain = (desc >> 4) & 0x1e;
    }
    domain = (env->cp15.c3 >> domain) & 3;
    if (domain == 0 || domain == 2) {
        if (type == 2) {
            code = 9;  /* Section domain fault.  */
        } else {
            code = 11; /* Page domain fault.  */
        }
        goto do_fault;
    }
    if (type == 2) {
        if (desc & (1 << 18)) {
            /* Supersection.  */
            phys_addr = (desc & 0xff000000) | (address & 0x00ffffff);
            *page_size = 0x1000000;
        } else {
            /* Section.  */
            phys_addr = (desc & 0xfff00000) | (address & 0x000fffff);
            *page_size = 0x100000;
        }
        ap = ((desc >> 10) & 3) | ((desc >> 13) & 4);
        xn = desc & (1 << 4);
        code = 13;
    } else {
        /* Lookup l2 entry.  */
        table = (desc & 0xfffffc00) | ((address >> 10) & 0x3fc);
        desc = ldl_phys(table);
        ap = ((desc >> 4) & 3) | ((desc >> 7) & 4);
        switch (desc & 3) {
        case 0: /* Page translation fault.  */
            code = 7;
            goto do_fault;
        case 1: /* 64k page.  */
            phys_addr = (desc & 0xffff0000) | (address & 0xffff);
            xn = desc & (1 << 15);
            *page_size = 0x10000;
            break;
        case 2: case 3: /* 4k page.  */
            phys_addr = (desc & 0xfffff000) | (address & 0xfff);
            xn = desc & 1;
            *page_size = 0x1000;
            break;
        default:
            /* Never happens, but compiler isn't smart enough to tell.  */
            abort();
        }
        code = 15;
    }
    if (domain == 3) {
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    } else {
        if (xn && access_type == ACCESS_INST_FETCH) {
            goto do_fault;
        }

        /* The simplified model uses AP[0] as an access control bit.  */
        if ((env->cp15.c1_sys & (1 << 29)) && (ap & 1) == 0) {
            /* Access flag fault.  */
            code = (code == 15) ? 6 : 3;
            goto do_fault;
        }
        *prot = check_ap(env, ap, domain, access_type, is_user);
        if (!*prot) {
            /* Access permission fault.  */
            goto do_fault;
        }
        if (!xn) {
            *prot |= PAGE_EXEC;
        }
    }
    *phys_ptr = phys_addr;
    return TRANSLATE_SUCCESS;
do_fault:
    return code | (domain << 4); //TRANSLATE_FAIL
}

static int cortexm_check_default_mapping(uint32_t address, int *prot, int access_type)
{
    switch (address) {
    case 0x00000000 ... 0x1FFEFFFF:
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        break;
    case 0x1FFF0000 ... 0x1FFF77FF:
        *prot = PAGE_READ | PAGE_EXEC;
        break;
    case 0x1FFF7800 ... 0x1FFFFFFF:
    case 0x20000000 ... 0x3FFFFFFF:
    case 0x60000000 ... 0x7FFFFFFF:
    case 0x80000000 ... 0x9FFFFFFF:
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        break;
    case 0x40000000 ... 0x5FFFFFFF:
    case 0xA0000000 ... 0xBFFFFFFF:
    case 0xC0000000 ... 0xDFFFFFFF:
    case 0xE0000000 ... 0xE00FFFFF:
        *prot = PAGE_READ | PAGE_WRITE;
        break;
    case 0xE0100000 ... 0xFFFFFFFF:
    default:
        *prot = 0;
        return PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    }
    return !(*prot & (1 << access_type));
}

static int pmsav7_check_default_mapping(uint32_t address, int *prot, int access_type)
{
    *prot = PAGE_READ | PAGE_WRITE;
    switch (address) {
    case 0xF0000000 ... 0xFFFFFFFF:
        // executable if high exception vectors are selected
        if (!(env->cp15.c1_sys & (1 << 13))) {
            break;
        }
        /* fallthrough */
    case 0x00000000 ... 0x7FFFFFFF:
        *prot |= PAGE_EXEC;
    default:
        break;
    }
    return (*prot & (1 << access_type)) ? MPU_SUCCESS : MPU_PERMISSION_FAULT;
}

static int get_mpu_subregion_number(uint32_t region_base_address, uint32_t region_size, uint32_t address)
{
    /* Subregion size is 2^(region_size - 3) */
    uint32_t subregion_size = 1 << (region_size - 3);
    return (address - region_base_address) / subregion_size;
}

static int get_phys_addr_mpu(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot)
{
    int n;
    uint32_t base;
    uint32_t size;
    uint32_t mask;
    uint32_t perms;

    *phys_ptr = address;
    *prot = 0;

#if DEBUG
    tlib_printf(LOG_LEVEL_DEBUG, "MPU: Trying to access address 0x%X", address);
#endif

    for (n = env->number_of_mpu_regions - 1; n >= 0; n--) {
        if (!(env->cp15.c6_size_and_enable[n] & MPU_REGION_ENABLED_BIT)) {
            continue;
        }
        switch((env->cp15.c6_size_and_enable[n] & MPU_SIZE_FIELD_MASK) >> 1) {
        case 0 ... 3:
            tlib_printf(LOG_LEVEL_WARNING, "Encountered MPU region size smaller than 32bytes, this is an unpredictable setting!");
            continue;
        default:
            size = ((env->cp15.c6_size_and_enable[n] & MPU_SIZE_FIELD_MASK) >> 1) + 1;
            break;
        }

        base = env->cp15.c6_base_address[n];
        mask = (1ull << size) - 1;

        if (base & mask) {
            /* Misaligned base addr to region */
            continue;
        }
        /* Check if the region is enabled */
        if (address >= base && address <= base + mask) {
            /* Check subregions, only if region size is equal to or bigger than 256 bytes (region size = 2^size) */
            if (size >= 8 && (env->cp15.c6_subregion_disable[n] & (1 << get_mpu_subregion_number(base, size, address)))) {
                /* Subregion containing this address is disabled, try to match this address to a different region */
                continue;
            } else {
                break;
            }
        }
    }

    if (n < 0) { // background fault
        if (arm_feature(env, ARM_FEATURE_PMSA)) {
            if (is_user || !(env->cp15.c1_sys & (1 << 17 /* BR, Background Region */))) {
                return MPU_BACKGROUND_FAULT;
            }
            return pmsav7_check_default_mapping(address, prot, access_type);
        }
        if (!is_user) {
            return cortexm_check_default_mapping(address, prot, access_type);
        }
        return TRANSLATE_FAIL;
    }

    perms = (env->cp15.c6_access_control[n] & MPU_PERMISSION_FIELD_MASK) >> 8;

    switch (perms) {
    case 0:
        return MPU_PERMISSION_FAULT;
    case 1:
        if (is_user) {
            return MPU_PERMISSION_FAULT;
        }
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        break;
    case 2:
        *prot = PAGE_READ | PAGE_EXEC;
        if (!is_user) {
            *prot |= PAGE_WRITE;
        }
        break;
    case 3:
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        break;
    case 5:
        if (is_user) {
            return MPU_PERMISSION_FAULT;
        }
        *prot = PAGE_READ | PAGE_EXEC;
        break;
    case 6:
        *prot = PAGE_READ | PAGE_EXEC;
        break;
    case 7:
        if (env->cp15.c0_cpuid == ARM_CPUID_CORTEXM3) {
            *prot |= PAGE_READ | PAGE_EXEC;
        } else {
            break;
        }
        break;
    default:
        /* Bad permission.  */
        break;
    }

    /* Check if the region is executable */
    if ((env->cp15.c6_access_control[n] & MPU_NEVER_EXECUTE_BIT))
        *prot &= ~PAGE_EXEC;

    /* PAGE_READ  = 1 ; ACCESS_TYPE = 0
     * PAGE_WRITE = 2 ; ACCESS_TYPE = 1
     * PAGE_EXEC  = 3 ; ACCESS_TYPE = 2
     */
    if (*prot & (1 << access_type)) {
        return TRANSLATE_SUCCESS;
    }
    return MPU_PERMISSION_FAULT;
}

#ifdef TARGET_PROTO_ARM_M

static int cortexm_check_default_mapping_v8(uint32 address)
{
    switch(address) {
    case 0x00000000 ... 0x7FFFFFFF:
        return PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        break;
    // Devices
    case 0x80000000 ... 0xFFFFFFFF:
        return PAGE_READ | PAGE_WRITE;
        break;
    default:
        tlib_abortf("Address out of range. This should never happen");
        return 0;  /* Never happens.  Keep compiler happy.  */
    }
}

static inline bool pmsav8_get_region(CPUState *env, uint32_t address, int *region_index, bool *multiple_regions)
{
    int n;
    bool hit = false;
    *multiple_regions = false;
    *region_index = -1;

    for (n = MAX_MPU_REGIONS - 1; n >= 0; n--) {

        if(!(env->pmsav8.rlar[n] & 0x1)) {
            /* Region disabled */
            continue;
        }

        uint32_t base = env->pmsav8.rbar[n] & ~0x1f;
        uint32_t limit = env->pmsav8.rlar[n] | 0x1f;
        if(address < base || address > limit) {
            /* Addr not in this region */
            continue;
        }

        /* region matched */
        if (hit) {
            /* multiple regions always return a failure
             * in this case region_index _must not_ be used
             */
            *multiple_regions = true;
            *region_index = -1;
            break;
        }

        hit = true;
        *region_index = n;
    }
    return hit;
}

#define PMSA_ENABLED(ctrl) (( ctrl & 0b100 ))
#define PMSA_PRIVDEFENA(ctrl) (( ctrl & 0b1 ))
#define PMSA_AP_PRIVONLY(ap) (( ap & 0b01 ) == 0)
#define PMSA_AP_READONLY(ap) (( ap & 0b10 ) != 0)

static inline int pmsav8_get_phys_addr(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot)
{
    bool hit;
    int resolved_region;
    bool multiple_regions = false;
    bool mpu_enabled = PMSA_ENABLED(env->pmsav8.ctrl);

    /* flat memory mapping */
    *phys_ptr = address;
    *prot = 0;

    if (!mpu_enabled) {
        hit = false;
    } else {
        hit = pmsav8_get_region(env, address, &resolved_region, &multiple_regions);

        /* Overlapping regions generate MemManage Fault
         * R_LLLP in Arm® v8-M Architecture Reference Manual DDI0553B.l ID30062020 */
        if (multiple_regions) {
            goto do_fault;
        }
    }

    if (hit) {
        int rbar = env->pmsav8.rbar[resolved_region];
        int xn = extract32(rbar, 0, 1);
        int ap = extract32(rbar, 1, 2);

        if (!PMSA_AP_PRIVONLY(ap) || !is_user) {
            *prot |= PAGE_READ;
            if(!PMSA_AP_READONLY(ap)) {
                *prot |= PAGE_WRITE;
            }
        }

        if (!xn) {
            *prot |= PAGE_EXEC;
        }

    } else {
        /* No region hit, use background region if:
         * - MPU disabled: for all accesses
         * - MPU enabled: for privileged accesses if default memory map is enabled (PRIVDEFENA)
         */
        if (!mpu_enabled || (!is_user && PMSA_PRIVDEFENA(env->pmsav8.ctrl))) {
            *prot = cortexm_check_default_mapping_v8(address);
        } else {
            goto do_fault;
        }
    }
    
    if (is_page_access_valid(*prot, access_type)) {
        return TRANSLATE_SUCCESS;
    }

do_fault:
    return TRANSLATE_FAIL;
}
#endif

static inline int get_phys_addr(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot,
                                target_ulong *page_size, int no_page_fault)
{
    if(unlikely(cpu->external_mmu_enabled))
    {
        return get_external_mmu_phys_addr(env, address, access_type, phys_ptr, prot, no_page_fault);
    }

    /* Fast Context Switch Extension.  */
    if (address < 0x02000000) {
        address += env->cp15.c13_fcse;
    }

    /* Handle v8M specific MPU */
#ifdef TARGET_PROTO_ARM_M
    if(arm_feature(env, ARM_FEATURE_V8)) {
        *page_size = TARGET_PAGE_SIZE;
        if (env->number_of_mpu_regions == 0) {
            /* MPU disabled */
            *phys_ptr = address;
            *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            return TRANSLATE_SUCCESS;
        }
        return pmsav8_get_phys_addr(env, address, access_type, is_user, phys_ptr, prot);
    }
#endif

    if ((env->cp15.c1_sys & 1) == 0) {
        /* MMU/MPU disabled.  */
        *phys_ptr = address;
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        *page_size = TARGET_PAGE_SIZE;
        return TRANSLATE_SUCCESS;
    } else if (arm_feature(env, ARM_FEATURE_MPU)) {
        *page_size = TARGET_PAGE_SIZE;
        return get_phys_addr_mpu(env, address, access_type, is_user, phys_ptr, prot);
    } else if (env->cp15.c1_sys & (1 << 23)) {
        return get_phys_addr_v6(env, address, access_type, is_user, phys_ptr, prot, page_size);
    } else {
        return get_phys_addr_v5(env, address, access_type, is_user, phys_ptr, prot, page_size);
    }
}

int cpu_handle_mmu_fault (CPUState *env, target_ulong address, int access_type, int mmu_idx, int no_page_fault)
{
    uint32_t phys_addr = 0;
    target_ulong page_size = 0;
    int prot = 0;
    int ret, is_user;

    is_user = mmu_idx == MMU_USER_IDX;
    ret = get_phys_addr(env, address, access_type, is_user, &phys_addr, &prot, &page_size, no_page_fault);
    // returns TRANSLATE_SUCCESS (0x0) on success
    // for no PMSA returns c5_data/insn value
    // for PMSA returns enum mpu_result

    if (ret == TRANSLATE_SUCCESS) {
        /* Map a single [sub]page.  */
        phys_addr &= TARGET_PAGE_MASK;
        address &= TARGET_PAGE_MASK;
        tlb_set_page(env, address, phys_addr, prot, mmu_idx, page_size);
        return TRANSLATE_SUCCESS;
    }
    else if(cpu->external_mmu_enabled)
    {
        return TRANSLATE_FAIL;
    }

    uint32_t c5_value = ret;
    if (arm_feature(env, ARM_FEATURE_PMSA)) {
        c5_value = (ret == MPU_PERMISSION_FAULT) ? PERMISSION_FAULT_STATUS_BITS : BACKGROUND_FAULT_STATUS_BITS;
    }

    if (access_type == ACCESS_INST_FETCH) {
        env->cp15.c5_insn = c5_value;
        env->cp15.c6_insn = address;
        env->exception_index = EXCP_PREFETCH_ABORT;
    } else {
        env->cp15.c5_data = c5_value;
        if (access_type == ACCESS_DATA_STORE && (arm_feature(env, ARM_FEATURE_PMSA) || arm_feature(env, ARM_FEATURE_V6))) {
            env->cp15.c5_data |= (1 << 11);
        }
        env->cp15.c6_data = address;
        env->exception_index = EXCP_DATA_ABORT;
    }
    return TRANSLATE_FAIL;
}

target_phys_addr_t cpu_get_phys_page_debug(CPUState *env, target_ulong addr)
{
    uint32_t phys_addr = 0;
    target_ulong page_size = 0;
    int prot = 0;
    int ret;

    ret = get_phys_addr(env, addr, 0, 0, &phys_addr, &prot, &page_size, 0);

    if (ret != 0) {
        return -1;
    }

    return phys_addr;
}

void HELPER(set_cp)(CPUState * env, uint32_t insn, uint32_t val)
{
    int cp_num = (insn >> 8) & 0xf;
    int cp_info = (insn >> 5) & 7;
    int src = (insn >> 16) & 0xf;
    int operand = insn & 0xf;

    if (env->cp[cp_num].cp_write) {
        env->cp[cp_num].cp_write(env->cp[cp_num].opaque, cp_info, src, operand, val);
    }
}

uint32_t HELPER(get_cp)(CPUState * env, uint32_t insn)
{
    int cp_num = (insn >> 8) & 0xf;
    int cp_info = (insn >> 5) & 7;
    int dest = (insn >> 16) & 0xf;
    int operand = insn & 0xf;

    if (env->cp[cp_num].cp_read) {
        return env->cp[cp_num].cp_read(env->cp[cp_num].opaque, cp_info, dest, operand);
    }
    return 0;
}

/* Return basic MPU access permission bits.  */
static uint32_t simple_mpu_ap_bits(uint32_t val)
{
    uint32_t ret;
    uint32_t mask;
    int i;
    ret = 0;
    mask = 3;
    for (i = 0; i < 16; i += 2) {
        ret |= (val >> i) & mask;
        mask <<= 2;
    }
    return ret;
}

/* Pad basic MPU access permission bits to extended format.  */
static uint32_t extended_mpu_ap_bits(uint32_t val)
{
    uint32_t ret;
    uint32_t mask;
    int i;
    ret = 0;
    mask = 3;
    for (i = 0; i < 16; i += 2) {
        ret |= (val & mask) << i;
        mask <<= 2;
    }
    return ret;
}

void HELPER(set_cp15)(CPUState * env, uint32_t insn, uint32_t val)
{
    int op1;
    int op2;
    int crm;
    int crn;

    op1 = (insn >> 21) & 7;
    op2 = (insn >> 5) & 7;
    crm = insn & 0xf;
    crn = (insn >> 16) & 0xf;
    switch (crn) {
    case 0:
        /* ID codes.  */
        if (arm_feature(env, ARM_FEATURE_XSCALE)) {
            break;
        }
        if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
            break;
        }
        if (arm_feature(env, ARM_FEATURE_V7) && op1 == 2 && crm == 0 && op2 == 0) { // CSSELR
            env->cp15.c0_cssel = val & 0xf;
            break;
        }
        goto bad_reg;
    case 1: /* System configuration.  */
        if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
            op2 = 0;
        }
        switch (op2) {
        case 0: // SCTLR
            if (!arm_feature(env, ARM_FEATURE_XSCALE) || crm == 0) {
                env->cp15.c1_sys = val;
            }
            /* ??? Lots of these bits are not implemented.  */
            /* This may enable/disable the MMU, so do a TLB flush.  */
            tlb_flush(env, 1, true);
            break;
        case 1: /* Auxiliary control register.  */
            if (arm_feature(env, ARM_FEATURE_XSCALE)) {
                env->cp15.c1_xscaleauxcr = val;
                break;
            }
            /* Not implemented.  */
            tlib_write_cp15_32(insn, val);
            break;
        case 2:
            if (arm_feature(env, ARM_FEATURE_XSCALE)) {
                goto bad_reg;
            }
            if (op1 == 0 && crm == 0 && env->cp15.c1_coproc != val) { // CPACR
                env->cp15.c1_coproc = val;
                /* ??? Is this safe when called from within a TB?  */
                tb_flush(env);
            }
            break;
        default:
            goto bad_reg;
        }
        break;
    case 2: /* MMU Page table control / MPU cache control.  */
        if (arm_feature(env, ARM_FEATURE_MPU)) {
            switch (op2) {
            case 0:
                env->cp15.c2_data = val;
                break;
            case 1:
                env->cp15.c2_insn = val;
                break;
            default:
                goto bad_reg;
            }
        } else {
            switch (op2) {
            case 0:
                env->cp15.c2_base0 = val;
                break;
            case 1:
                env->cp15.c2_base1 = val;
                break;
            case 2:
                val &= 7;
                env->cp15.c2_control = val;
                env->cp15.c2_mask = ~(((uint32_t)0xffffffffu) >> val);
                env->cp15.c2_base_mask = ~((uint32_t)0x3fffu >> val);
                break;
            default:
                goto bad_reg;
            }
        }
        break;
    case 3:                /* MMU Domain access control / MPU write buffer control.  */
        env->cp15.c3 = val;
        tlb_flush(env, 1, true); /* Flush TLB as domain not tracked in TLB */
        break;
    case 4:                /* Reserved.  */
        goto bad_reg;
    case 5:                /* MMU Fault status / MPU access permission.  */
        if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
            op2 = 0;
        }
        switch (op2) {
        case 0:
            if (arm_feature(env, ARM_FEATURE_MPU)) {
                val = extended_mpu_ap_bits(val);
            }
            env->cp15.c5_data = val;
            break;
        case 1:
            if (arm_feature(env, ARM_FEATURE_MPU)) {
                val = extended_mpu_ap_bits(val);
            }
            env->cp15.c5_insn = val;
            break;
        case 2:
            if (!arm_feature(env, ARM_FEATURE_MPU)) {
                goto bad_reg;
            }
            env->cp15.c5_data = val;
            break;
        case 3:
            if (!arm_feature(env, ARM_FEATURE_MPU)) {
                goto bad_reg;
            }
            env->cp15.c5_insn = val;
            break;
        default:
            goto bad_reg;
        }
        break;
    case 6: /* MMU Fault address / MPU base/size.  */
        if (arm_feature(env, ARM_FEATURE_PMSA)) {
            if (op1 != 0) {
                goto bad_reg;
            }
            if (crm == 0 && op2 == 0) {
                env->cp15.c6_data = val;
                return;
            }
            if (crm == 2 && op2 == 0) { // RGNR, MPU Region Number Register
                if (val >= env->number_of_mpu_regions) {
                    tlib_abortf("Region number %u doesn't point to a valid region", val);
                }
                env->cp15.c6_region_number = val;
                return;
            }
            if (crm == 1) {
                uint32_t index = env->cp15.c6_region_number;
                switch (op2) {
                case 0: // DRBAR, Data Region Base Address Register
                    if (val & 0b11111) {
                        // ISA requires address to be divisible by 4, but due to current MPU implementation it also has to be divisible by 32
                        tlib_abortf("Region size smaller than 32bytes is not supported. Region base address must be divisible by 32");
                    }
                    env->cp15.c6_base_address[index] = val;
                    tlb_flush(env, 1, false);
                    return;
                case 2: // DRSR, Data Region Size and Enable Register
                    env->cp15.c6_size_and_enable[index] = val & MPU_SIZE_AND_ENABLE_FIELD_MASK;
                    env->cp15.c6_subregion_disable[index] = (val & MPU_SUBREGION_DISABLE_FIELD_MASK) >> MPU_SUBREGION_DISABLE_FIELD_OFFSET;
                    tlb_flush(env, 1, false);
                    return;
                case 4: // DRACR, Data Region Access Control Register
                    env->cp15.c6_access_control[index] = val;
                    tlb_flush(env, 1, false);
                    return;
                default:
                    goto bad_reg;
                }
            }
        }
        if (arm_feature(env, ARM_FEATURE_MPU)) {
            if (crm >= 8) {
                goto bad_reg;
            }
            env->cp15.c6_addr = val;
        } else {
            if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
                op2 = 0;
            }
            switch (op2) {
            case 0:
                env->cp15.c6_data = val;
                break;
            case 1: /* ??? This is WFAR on armv6 */
            case 2:
                env->cp15.c6_insn = val;
                break;
            default:
                goto bad_reg;
            }
        }
        break;
    case 7: /* Cache control.  */
        env->cp15.c15_i_max = 0x000;
        env->cp15.c15_i_min = 0xff0;
        if (op1 != 0) {
            goto bad_reg;
        }
        /* No cache, so nothing to do except VA->PA translations. */
        if (arm_feature(env, ARM_FEATURE_VAPA)) {
            switch (crm) {
            case 4:
                if (arm_feature(env, ARM_FEATURE_V7)) {
                    env->cp15.c7_par = val & 0xfffff6ff;
                } else {
                    env->cp15.c7_par = val & 0xfffff1ff;
                }
                break;
            case 8: {
                uint32_t phys_addr = 0;
                target_ulong page_size;
                int prot;
                int ret, is_user = op2 & 2;
                int access_type = op2 & 1;

                if (op2 & 4) {
                    /* Other states are only available with TrustZone */
                    goto bad_reg;
                }
                ret = get_phys_addr(env, val, access_type, is_user, &phys_addr, &prot, &page_size, 0);
                if (ret == 0) {
                    /* We do not set any attribute bits in the PAR */
                    if (page_size == (1 << 24) && arm_feature(env, ARM_FEATURE_V7)) {
                        env->cp15.c7_par = (phys_addr & 0xff000000) | 1 << 1;
                    } else {
                        env->cp15.c7_par = phys_addr & 0xfffff000;
                    }
                } else {
                    env->cp15.c7_par = ((ret & (10 << 1)) >> 5) | ((ret & (12 << 1)) >> 6) | ((ret & 0xf) << 1) | 1;
                }
                break;
            }
            }
        }
        break;
    case 8:     /* MMU TLB control.  */
        switch (op2) {
        case 0: /* Invalidate all.  */
            tlb_flush(env, 0, true);
            break;
        case 1: /* Invalidate single TLB entry.  */
            tlb_flush_page(env, val & TARGET_PAGE_MASK, true);
            break;
        case 2: /* Invalidate on ASID.  */
            tlb_flush(env, val == 0, true);
            break;
        case 3: /* Invalidate single entry on MVA.  */
            /* ??? This is like case 1, but ignores ASID.  */
            tlb_flush(env, 1, true);
            break;
        default:
            goto bad_reg;
        }
        break;
    case 9:
        if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
            break;
        }
        if (arm_feature(env, ARM_FEATURE_STRONGARM)) {
            break;  /* Ignore ReadBuffer access */
        }
        switch (crm) {
        case 0:     /* Cache lockdown.  */
            switch (op1) {
            case 0: /* L1 cache.  */
                switch (op2) {
                case 0:
                    env->cp15.c9_data = val;
                    break;
                case 1:
                    env->cp15.c9_insn = val;
                    break;
                default:
                    goto bad_reg;
                }
                break;
            case 1: /* L2 cache.  */
                /* Ignore writes to L2 lockdown/auxiliary registers.  */
                break;
            default:
                goto bad_reg;
            }
            break;
        case 1: /*  branch predictor, cache, and TCM operations */
            switch (op2) {
            case 0:
            case 1:
            {
                uint32_t tcm_region_index = env->cp15.c9_tcmsel;
                uint32_t tcm_region_value = env->cp15.c9_tcmregion[op2][tcm_region_index];
                if (val != tcm_region_value) {
                    tlib_abortf("Attempted to change TCM region #%u for interface #%u from 0x%08x to 0x%08x, reconfiguration at runtime is currently not supported", tcm_region_index, op2, tcm_region_value, val);
                }
                break;
            }
            default:
                goto bad_reg;
            }
            break;
        case 2: /*  branch predictor, cache, and TCM operations */
            switch (op2) {
            case 0:
                if (val >= MAX_TCM_REGIONS) {
                    tlib_abortf("Attempted access to TCM region #%u, maximal supported value is %u", val, MAX_TCM_REGIONS);
                }
                env->cp15.c9_tcmsel = val;
                break;
            default:
                goto bad_reg;
            }
            break;
        case 12: /* Performance monitor control */
            /* Performance monitors are implementation defined in v7,
             * but with an ARM recommended set of registers, which we
             * follow (although we don't actually implement any counters)
             */
            if (!arm_feature(env, ARM_FEATURE_V7)) {
                goto bad_reg;
            }
            switch (op2) {
            case 0: /* performance monitor control register */
                /* only the DP, X, D and E bits are writable */
                env->cp15.c9_pmcr &= ~0x39;
                env->cp15.c9_pmcr |= (val & 0x39);
                break;
            case 1: /* Count enable set register */
                val &= (1 << 31);
                env->cp15.c9_pmcnten |= val;
                break;
            case 2: /* Count enable clear */
                val &= (1 << 31);
                env->cp15.c9_pmcnten &= ~val;
                break;
            case 3: /* Overflow flag status */
                env->cp15.c9_pmovsr &= ~val;
                break;
            case 4: /* Software increment */
                /* RAZ/WI since we don't implement the software-count event */
                break;
            case 5: /* Event counter selection register */
                /* Since we don't implement any events, writing to this register
                 * is actually UNPREDICTABLE. So we choose to RAZ/WI.
                 */
                break;
            default:
                goto bad_reg;
            }
            break;
        case 13: /* Performance counters */
            if (!arm_feature(env, ARM_FEATURE_V7)) {
                goto bad_reg;
            }
            switch (op2) {
            case 0:
                tlib_write_cp15_32(insn, val);
                break;
            case 1: /* Event type select */
                env->cp15.c9_pmxevtyper = val & 0xff;
                break;
            case 2: /* Event count register */
                tlib_write_cp15_32(insn, val);
                break;
            default:
                goto bad_reg;
            }
            break;
        case 14: /* Performance monitor control */
            if (!arm_feature(env, ARM_FEATURE_V7)) {
                goto bad_reg;
            }
            switch (op2) {
            case 0: /* user enable */
                env->cp15.c9_pmuserenr = val & 1;
                /* changes access rights for cp registers, so flush tbs */
                tb_flush(env);
                break;
            case 1: /* interrupt enable set */
                /* We have no event counters so only the C bit can be changed */
                val &= (1 << 31);
                env->cp15.c9_pminten |= val;
                break;
            case 2: /* interrupt enable clear */
                val &= (1 << 31);
                env->cp15.c9_pminten &= ~val;
                break;
            }
            break;
        default:
            goto bad_reg;
        }
        break;
    case 10: /* MMU TLB lockdown.  */
        tlib_write_cp15_32(insn, val);
        break;
    case 12: /* Security Extensions Register */
        switch (crm) {
        case 0: /* VBAR, MVBAR, HVBAR. [Monitor, Hypervisor] Vecotor Base Address Register */

            /* VBAR */
            if( op1 == 0 && op2 == 0 ) {
                env->cp15.c12_vbar = val & 0xFFFFFFF0;
            }
            else {
                goto bad_reg;
            }

            break;
        
        case 1: /* ISR - Interrupt Status Register */
            /* Intentional fallthroug */
        default:
            goto bad_reg;
        }
        break;
    case 13: /* Process ID.  */
        switch (op2) {
        case 0:
            /* Unlike real hardware the qemu TLB uses virtual addresses,
               not modified virtual addresses, so this causes a TLB flush.
             */
            if (env->cp15.c13_fcse != val) {
                tlb_flush(env, 1, true);
            }
            env->cp15.c13_fcse = val;
            break;
        case 1:
            /* This changes the ASID, so do a TLB flush.  */
            if (env->cp15.c13_context != val && !arm_feature(env, ARM_FEATURE_MPU)) {
                tlb_flush(env, 0, true);
            }
            env->cp15.c13_context = val;
            break;
        default:
            goto bad_reg;
        }
        break;
    case 14: /* Generic timer */
        if (arm_feature(env, ARM_FEATURE_GENERIC_TIMER)) {
            /* Dummy implementation: RAZ/WI for all */
            break;
        }
        goto bad_reg;
    case 15: /* Implementation specific.  */
        if (arm_feature(env, ARM_FEATURE_XSCALE)) {
            if (op2 == 0 && crm == 1) {
                if (env->cp15.c15_cpar != (val & 0x3fff)) {
                    /* Changes cp0 to cp13 behavior, so needs a TB flush.  */
                    tb_flush(env);
                    env->cp15.c15_cpar = val & 0x3fff;
                }
                break;
            }
            goto bad_reg;
        }
        if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
            switch (crm) {
            case 0:
                break;
            case 1:                                     /* Set TI925T configuration.  */
                env->cp15.c15_ticonfig = val & 0xe7;
                env->cp15.c0_cpuid = (val & (1 << 5)) ? /* OS_TYPE bit */
                                     ARM_CPUID_TI915T : ARM_CPUID_TI925T;
                break;
            case 2:                                     /* Set I_max.  */
                env->cp15.c15_i_max = val;
                break;
            case 3:                                     /* Set I_min.  */
                env->cp15.c15_i_min = val;
                break;
            case 4:                                     /* Set thread-ID.  */
                env->cp15.c15_threadid = val & 0xffff;
                break;
            case 8:                                     /* Wait-for-interrupt (deprecated).  */
                helper_wfi();
                break;
            default:
                goto bad_reg;
            }
        }
        break;
    }
    return;
bad_reg:
    tlib_write_cp15_32(insn, val);
}

uint64_t HELPER(get_cp15_64bit)(CPUState * env, uint32_t insn)
{
    return tlib_read_cp15_64(insn);
}

uint32_t HELPER(get_cp15_32bit)(CPUState * env, uint32_t insn)
{
    return tlib_read_cp15_32(insn);
}

void HELPER(set_cp15_64bit)(CPUState * env, uint32_t insn, uint32_t val_1, uint32_t val_2)
{
    uint64_t val = ((uint64_t)val_2 << 32) | val_1;
    tlib_write_cp15_64(insn, val);
}

void HELPER(set_cp15_32bit)(CPUState * env, uint32_t insn, uint32_t val)
{
    tlib_write_cp15_32(insn, val);
}

uint32_t HELPER(get_cp15)(CPUState * env, uint32_t insn)
{
    int op1;
    int op2;
    int crm;
    int crn;

    op1 = (insn >> 21) & 7;
    op2 = (insn >> 5) & 7;
    crm = insn & 0xf;
    crn = (insn >> 16) & 0xf;
    switch (crn) {
    case 0: /* ID codes.  */
        switch (op1) {
        case 0:
            switch (crm) {
            case 0:
                switch (op2) {
                case 0:       /* Device ID.  */
                    return env->cp15.c0_cpuid;
                case 1:       /* Cache Type.  */ // CCSIDR
                    return env->cp15.c0_cachetype;
                case 2: // TCMTR, TCM Type Register
                    return env->cp15.c0_tcmtype;
                case 3:       /* TLB type register.  */
                    return 0; /* No lockable TLB entries.  */
                case 4:
                    if (arm_feature(env, ARM_FEATURE_PMSA)) { /* MPUIR, MPU Type Register */
                        return (env->number_of_mpu_regions << MPU_TYPE_DREGION_FIELD_OFFSET) & MPU_TYPE_DREGION_FIELD_MASK;
                    }
                    goto bad_reg;
                case 5:       /* MPIDR */
                    /* The MPIDR was standardised in v7; prior to
                     * this it was implemented only in the 11MPCore.
                     * For all other pre-v7 cores it does not exist.
                     */
                    if (arm_feature(env, ARM_FEATURE_V7) || ARM_CPUID(env) == ARM_CPUID_ARM11MPCORE) {
                        int mpidr = tlib_get_cpu_index();
                        /* We don't support setting cluster ID ([8..11])
                         * so these bits always RAZ.
                         */
                        if (arm_feature(env, ARM_FEATURE_V7MP)) {
                            mpidr |= (1 << 31);
                            /* Cores which are uniprocessor (non-coherent)
                             * but still implement the MP extensions set
                             * bit 30. (For instance, A9UP.) However we do
                             * not currently model any of those cores.
                             */
                        }
                        return mpidr;
                    }
                    /* otherwise fall through to the unimplemented-reg case */
                    goto case_6;
                case 6:
case_6:
                    return tlib_read_cp15_32(insn);
                default:
                    goto bad_reg;
                }
            case 1:
                if (!arm_feature(env, ARM_FEATURE_V6)) {
                    goto bad_reg;
                }
                return env->cp15.c0_c1[op2];
            case 2:
                if (!arm_feature(env, ARM_FEATURE_V6)) {
                    goto bad_reg;
                }
                return env->cp15.c0_c2[op2];
            case 3: case 4: case 5: case 6: case 7:
                return 0;
            default:
                goto bad_reg;
            }
        case 1:
            /* These registers aren't documented on arm11 cores.  However
               Linux looks at them anyway.  */
            if (!arm_feature(env, ARM_FEATURE_V6)) {
                goto bad_reg;
            }
            if (crm != 0) {
                goto bad_reg;
            }
            if (!arm_feature(env, ARM_FEATURE_V7)) {
                return 0;
            }

            switch (op2) {
            case 0:
                return env->cp15.c0_ccsid[env->cp15.c0_cssel];
            case 1:
                return env->cp15.c0_clid;
            case 7:
                return 0;
            }
            goto bad_reg;
        case 2:
            if (op2 != 0 || crm != 0) {
                goto bad_reg;
            }
            return env->cp15.c0_cssel;
        default:
            goto bad_reg;
        }
    case 1: /* System configuration.  */
        if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
            op2 = 0;
        }
        switch (op2) {
        case 0: /* Control register.  */
            return env->cp15.c1_sys;
        case 1: /* Auxiliary control register.  */
            if (arm_feature(env, ARM_FEATURE_XSCALE)) {
                return env->cp15.c1_xscaleauxcr;
            }
            if (!arm_feature(env, ARM_FEATURE_AUXCR)) {
                goto bad_reg;
            }
            switch (ARM_CPUID(env) & ARM_ARCHITECTURE_MASK) {
            case ARM_CPUID_ARM1026 & ARM_ARCHITECTURE_MASK:
                return 1;
            case ARM_CPUID_ARM1136 & ARM_ARCHITECTURE_MASK:
            case ARM_CPUID_ARM1176 & ARM_ARCHITECTURE_MASK:
                return 7;
            case ARM_CPUID_ARM11MPCORE & ARM_ARCHITECTURE_MASK:
                return 1;
            case ARM_CPUID_CORTEXA8 & ARM_ARCHITECTURE_MASK:
                return 2;
            case ARM_CPUID_CORTEXA9 & ARM_ARCHITECTURE_MASK:
                return 0;
            case ARM_CPUID_CORTEXA15 & ARM_ARCHITECTURE_MASK:
                return 0;
            default:
                goto bad_reg;
            }
        case 2: /* Coprocessor access register.  */
            if (arm_feature(env, ARM_FEATURE_XSCALE)) {
                goto bad_reg;
            }
            return env->cp15.c1_coproc;
        default:
            goto bad_reg;
        }
    case 2: /* MMU Page table control / MPU cache control.  */
        if (arm_feature(env, ARM_FEATURE_MPU)) {
            switch (op2) {
            case 0:
                return env->cp15.c2_data;
                break;
            case 1:
                return env->cp15.c2_insn;
                break;
            default:
                goto bad_reg;
            }
        } else {
            switch (op2) {
            case 0:
                return env->cp15.c2_base0;
            case 1:
                return env->cp15.c2_base1;
            case 2:
                return env->cp15.c2_control;
            default:
                goto bad_reg;
            }
        }
    case 3: /* MMU Domain access control / MPU write buffer control.  */
        return env->cp15.c3;
    case 4: /* Reserved.  */
        goto bad_reg;
    case 5: /* MMU Fault status / MPU access permission.  */
        if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
            op2 = 0;
        }
        switch (op2) {
        case 0:
            if (arm_feature(env, ARM_FEATURE_PMSA)) { // DFSR
                return (env->cp15.c5_data & (MPU_FAULT_STATUS_BITS_FIELD_MASK | MPU_FAULT_STATUS_WRITE_FIELD_MASK));
            }
            if (arm_feature(env, ARM_FEATURE_MPU)) {
                return simple_mpu_ap_bits(env->cp15.c5_data);
            }
            return env->cp15.c5_data;
        case 1:
            if (arm_feature(env, ARM_FEATURE_PMSA)) { // IFSR
                return (env->cp15.c5_insn & MPU_FAULT_STATUS_BITS_FIELD_MASK);
            }
            if (arm_feature(env, ARM_FEATURE_MPU)) {
                return simple_mpu_ap_bits(env->cp15.c5_data);
            }
            return env->cp15.c5_insn;
        case 2:
            if (!arm_feature(env, ARM_FEATURE_MPU)) {
                goto bad_reg;
            }
            return env->cp15.c5_data;
        case 3:
            if (!arm_feature(env, ARM_FEATURE_MPU)) {
                goto bad_reg;
            }
            return env->cp15.c5_insn;
        default:
            goto bad_reg;
        }
    case 6: /* MMU Fault address.  */
        if (arm_feature(env, ARM_FEATURE_PMSA)) {
            if (op1 == 0 && crm == 0 && op2 == 0) { // DFAR, Data Fault Address Register
                return env->cp15.c6_data;
            }
            if (op1 == 0 && crm == 2 && op2 == 0) { // RGNR, MPU Region Number Register
                return env->cp15.c6_region_number;
            }
            if (op1 == 0 && crm == 1) {
                uint32_t index = env->cp15.c6_region_number;
                switch (op2) {
                case 0: // DRBAR, Data Region Base Address Register
                    return env->cp15.c6_base_address[index];
                case 2: // DRSR, Data Region Size and Enable Register
                    return env->cp15.c6_size_and_enable[index] | (env->cp15.c6_subregion_disable[index] << 8);
                case 4: // DRACR, Data Region Access Control Register
                    return env->cp15.c6_access_control[index];
                default:
                    goto bad_reg;
                }
            }
        }
        if (arm_feature(env, ARM_FEATURE_MPU)) {
            if (crm >= 8) {
                goto bad_reg;
            }
            return env->cp15.c6_addr;
        } else {
            if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
                op2 = 0;
            }
            switch (op2) {
            case 0:
                return env->cp15.c6_data;
            case 1:
                if (arm_feature(env, ARM_FEATURE_V6)) {
                    /* Watchpoint Fault Adrress.  */
                    return 0; /* Not implemented.  */
                } else {
                    /* Instruction Fault Adrress.  */
                    /* Arm9 doesn't have an IFAR, but implementing it anyway
                       shouldn't do any harm.  */
                    return env->cp15.c6_insn;
                }
            case 2:
                if (arm_feature(env, ARM_FEATURE_V6)) {
                    /* Instruction Fault Adrress.  */
                    return env->cp15.c6_insn;
                } else {
                    goto bad_reg;
                }
            default:
                goto bad_reg;
            }
        }
    case 7: /* Cache control.  */
        if (crm == 4 && op1 == 0 && op2 == 0) {
            return env->cp15.c7_par;
        }
        /* FIXME: Should only clear Z flag if destination is r15.  */
        env->ZF = 0;
        return 0;
    case 8: /* MMU TLB control.  */
        goto bad_reg;
    case 9:
        switch (crm) {
        case 0:     /* Cache lockdown */
            switch (op1) {
            case 0: /* L1 cache.  */
                if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
                    return 0;
                }
                switch (op2) {
                case 0:
                    return env->cp15.c9_data;
                case 1:
                    return env->cp15.c9_insn;
                default:
                    goto bad_reg;
                }
            case 1: /* L2 cache */
                /* L2 Lockdown and Auxiliary control.  */
                switch (op2) {
                case 0:
                    /* L2 cache lockdown (A8 only) */
                    return 0;
                case 2:
                    /* L2 cache auxiliary control (A8) or control (A15) */
                    if (ARM_CPUID(env) == ARM_CPUID_CORTEXA15) {
                        /* Linux wants the number of processors from here.
                         * Might as well set the interrupt-controller bit too.
                         */
                        #define smp_cpus 1 // TODO: should return correct number of cpus
                        return ((smp_cpus  - 1) << 24) | (1 << 23);
                    }
                    return 0;
                case 3:
                    /* L2 cache extended control (A15) */
                    return 0;
                default:
                    goto bad_reg;
                }
            default:
                goto bad_reg;
            }
            break;
        case 1: /*  branch predictor, cache, and TCM operations */
            switch (op2) {
            case 0:
            case 1:
                return env->cp15.c9_tcmregion[op2][env->cp15.c9_tcmsel];
            default:
                goto bad_reg;
            }
            break;
        case 2: /*  branch predictor, cache, and TCM operations */
            switch (op2) {
            case 0:
                return env->cp15.c9_tcmsel;
            default:
                goto bad_reg;
            }
            break;
        case 12: /* Performance monitor control */
            if (!arm_feature(env, ARM_FEATURE_V7)) {
                goto bad_reg;
            }
            switch (op2) {
            case 0: /* performance monitor control register */
                return env->cp15.c9_pmcr;
            case 1: /* count enable set */
            case 2: /* count enable clear */
                return env->cp15.c9_pmcnten;
            case 3: /* overflow flag status */
                return env->cp15.c9_pmovsr;
            case 4: /* software increment */
            case 5: /* event counter selection register */
                return tlib_read_cp15_32(insn);
            default:
                goto bad_reg;
            }
        case 13: /* Performance counters */
            if (!arm_feature(env, ARM_FEATURE_V7)) {
                goto bad_reg;
            }
            switch (op2) {
            case 1: /* Event type select */
                return env->cp15.c9_pmxevtyper;
            case 0: /* Cycle count register */
            case 2: /* Event count register */
                return tlib_read_cp15_32(insn);
            default:
                goto bad_reg;
            }
        case 14: /* Performance monitor control */
            if (!arm_feature(env, ARM_FEATURE_V7)) {
                goto bad_reg;
            }
            switch (op2) {
            case 0: /* user enable */
                return env->cp15.c9_pmuserenr;
            case 1: /* interrupt enable set */
            case 2: /* interrupt enable clear */
                return env->cp15.c9_pminten;
            default:
                goto bad_reg;
            }
        default:
            goto bad_reg;
        }
        break;
    case 10: /* MMU TLB lockdown.  */
        /* ??? TLB lockdown not implemented.  */
        return 0;
    case 11: /* TCM DMA control.  */
    case 12: /* Security Extensions Register */
        switch (crm) {
        case 0: /* VBAR, MVBAR, HVBAR. [Monitor, Hypervisor] Vecotor Base Address Register */

            /* VBAR */
            if( op1 == 0 && op2 == 0 ) {
                return env->cp15.c12_vbar;
            }
            else {
                goto bad_reg;
            }

            break;
        
        case 1: /* ISR - Interrupt Status Register */
            /* Intentional fallthroug */
        default:
            goto bad_reg;
        }
    case 13: /* Process ID.  */
        switch (op2) {
        case 0:
            return env->cp15.c13_fcse;
        case 1:
            return env->cp15.c13_context;
        default:
            goto bad_reg;
        }
    case 14: /* Generic Timer. */
        if (arm_feature(env, ARM_FEATURE_GENERIC_TIMER)) {
            return tlib_read_cp15_32(insn);
        }
        goto bad_reg;
    case 15: /* Implementation specific.  */
        if (arm_feature(env, ARM_FEATURE_XSCALE)) {
            if (op2 == 0 && crm == 1) {
                return env->cp15.c15_cpar;
            }

            goto bad_reg;
        }
        if (arm_feature(env, ARM_FEATURE_OMAPCP)) {
            switch (crm) {
            case 0:
                return 0;
            case 1: /* Read TI925T configuration.  */
                return env->cp15.c15_ticonfig;
            case 2: /* Read I_max.  */
                return env->cp15.c15_i_max;
            case 3: /* Read I_min.  */
                return env->cp15.c15_i_min;
            case 4: /* Read thread-ID.  */
                return env->cp15.c15_threadid;
            case 8: /* TI925T_status */
                return 0;
            }
            /* TODO: Peripheral port remap register:
             * On OMAP2 mcr p15, 0, rn, c15, c2, 4 sets up the interrupt
             * controller base address at $rn & ~0xfff and map size of
             * 0x200 << ($rn & 0xfff), when MMU is off.  */
            goto bad_reg;
        }
        return tlib_read_cp15_32(insn);
    }
bad_reg:
    /* ??? For debugging only.  Should raise illegal instruction exception.  */
    return tlib_read_cp15_32(insn);
}

void HELPER(set_r13_banked)(CPUState * env, uint32_t mode, uint32_t val)
{
    if ((env->uncached_cpsr & CPSR_M) == mode) {
        env->regs[13] = val;
    } else {
        env->banked_r13[bank_number(mode)] = val;
    }
}

uint32_t HELPER(get_r13_banked)(CPUState * env, uint32_t mode)
{
    if ((env->uncached_cpsr & CPSR_M) == mode) {
        return env->regs[13];
    } else {
        return env->banked_r13[bank_number(mode)];
    }
}

#ifdef TARGET_PROTO_ARM_M
uint32_t HELPER(v7m_mrs)(CPUState * env, uint32_t reg)
{
    switch (reg) {
    case 0:  /* APSR */
        return xpsr_read(env) & 0xf8000000;
    case 1:  /* IAPSR */
        return xpsr_read(env) & 0xf80001ff;
    case 2:  /* EAPSR */
        return xpsr_read(env) & 0xff00fc00;
    case 3:  /* xPSR */
        return xpsr_read(env) & 0xff00fdff;
    case 5:  /* IPSR */
        return xpsr_read(env) & 0x000001ff;
    case 6:  /* EPSR */
        return xpsr_read(env) & 0x0700fc00;
    case 7:  /* IEPSR */
        return xpsr_read(env) & 0x0700edff;
    case 8:  /* MSP */
        return env->v7m.current_sp ? env->v7m.other_sp : env->regs[13];
    case 9:  /* PSP */
        return env->v7m.current_sp ? env->regs[13] : env->v7m.other_sp;
    case 10: /* MSPLIM - armv8-m specific */
        return env->v7m.msplim;
    case 11: /* PSPLIM - armv8-m specific */
        return env->v7m.psplim;
    case 16: /* PRIMASK */
        return (env->uncached_cpsr & 1) != 0;
    case 17: /* BASEPRI */
    case 18: /* BASEPRI_MAX */
        return env->v7m.basepri;
    case 19: /* FAULTMASK */
        return env->v7m.faultmask; 
    case 20: /* CONTROL */
        return env->v7m.control;
    default:
        /* ??? For debugging only.  */
        cpu_abort(env, "Unimplemented system register read (%d)\n", reg);
        return 0;
    }
}

void HELPER(v7m_msr)(CPUState * env, uint32_t reg, uint32_t val)
{
    switch (reg) {
    case 0: /* APSR */
        if(!in_privileged_mode(env))
            return;
        xpsr_write(env, val, 0xf8000000);
        break;
    case 1: /* IAPSR */
        if(!in_privileged_mode(env))
            return;
        xpsr_write(env, val, 0xf8000000);
        break;
    case 2: /* EAPSR */
        if(!in_privileged_mode(env))
            return;
        xpsr_write(env, val, 0xfe00fc00);
        break;
    case 3: /* xPSR */
        if(!in_privileged_mode(env))
            return;
        xpsr_write(env, val, 0xfe00fc00);
        break;
    case 5: /* IPSR */
        if(!in_privileged_mode(env))
            return;
        /* IPSR bits are readonly.  */
        break;
    case 6: /* EPSR */
        if(!in_privileged_mode(env))
            return;
        xpsr_write(env, val, 0x0600fc00);
        break;
    case 7: /* IEPSR */
        if(!in_privileged_mode(env))
            return;
        xpsr_write(env, val, 0x0600fc00);
        break;
    case 8: /* MSP */
        if(!in_privileged_mode(env)) {
            return;
        } else if (env->v7m.current_sp) {
            env->v7m.other_sp = val;
        } else {
            env->regs[13] = val;
        }
        break;
    case 9: /* PSP */
        if (env->v7m.current_sp) {
            env->regs[13] = val;
        } else {
            env->v7m.other_sp = val;
        }
        break;
    case 10: /* MSPLIM - armv8-m specific */
        env->v7m.msplim = val;
        break;
    case 11: /* PSPLIM - armv8-m specific */
        env->v7m.psplim = val;
        break;
    case 16: /* PRIMASK */
        if (!in_privileged_mode(env)) {
            return;
        } else if (val & 1) {
            env->uncached_cpsr |= CPSR_PRIMASK;
        } else {
            env->uncached_cpsr &= ~CPSR_PRIMASK;
            tlib_nvic_find_pending_irq();
        }
        break;
    case 17: /* BASEPRI */
        if(!in_privileged_mode(env))
            return;
        env->v7m.basepri = val & 0xff;
        tlib_nvic_write_basepri(val & 0xff);
        break;
    case 18: /* BASEPRI_MAX */
        if (!in_privileged_mode(env))
            return;
        val &= 0xff;
        if (val != 0 && (val < env->v7m.basepri || env->v7m.basepri == 0)) {
            env->v7m.basepri = val;
            tlib_nvic_write_basepri(val);
        }
        break;
    case 19: /* FAULTMASK */
        if (!in_privileged_mode(env)) {
            return;
        }
        env->v7m.faultmask = val & 1;
        break;
    case 20: /* CONTROL */
        if(!in_privileged_mode(env))
            return;
        env->v7m.control = val & 3;
        // only switch the stack if in thread mode (handler mode always uses MSP stack)
        if (env->v7m.exception == 0) {
            switch_v7m_sp(env, (val & 2) != 0);
        }
        break;
    default:
        /* ??? For debugging only.  */
        cpu_abort(env, "Unimplemented system register write (%d)\n", reg);
        return;
    }
}
#endif

void cpu_arm_set_cp_io(CPUState *env, int cpnum, ARMReadCPFunc *cp_read, ARMWriteCPFunc *cp_write, void *opaque)
{
    if (cpnum < 0 || cpnum > 14) {
        cpu_abort(env, "Bad coprocessor number: %i\n", cpnum);
        return;
    }

    env->cp[cpnum].cp_read = cp_read;
    env->cp[cpnum].cp_write = cp_write;
    env->cp[cpnum].opaque = opaque;
}

/* Note that signed overflow is undefined in C.  The following routines are
   careful to use unsigned types where modulo arithmetic is required.
   Failure to do so _will_ break on newer gcc.  */

/* Signed saturating arithmetic.  */

/* Perform 16-bit signed saturating addition.  */
#define PFX_Q
#include "op_addsub.h"
#undef PFX_Q

#define PFX_UQ
#include "op_addsub.h"
#undef PFX_UQ

/* Signed modulo arithmetic.  */
#define PFX_S
#define ARITH_GE
#include "op_addsub.h"
#undef ARITH_GE
#undef PFX_S

#define PFX_U
#define ARITH_GE
#include "op_addsub.h"
#undef ARITH_GE
#undef PFX_U

#define PFX_SH
#include "op_addsub.h"
#undef PFX_SH

#define PFX_UH
#include "op_addsub.h"
#undef PFX_UH

static inline uint8_t do_usad(uint8_t a, uint8_t b)
{
    if (a > b) {
        return a - b;
    } else {
        return b - a;
    }
}

/* Unsigned sum of absolute byte differences.  */
uint32_t HELPER(usad8)(uint32_t a, uint32_t b)
{
    uint32_t sum;
    sum = do_usad(a, b);
    sum += do_usad(a >> 8, b >> 8);
    sum += do_usad(a >> 16, b >> 16);
    sum += do_usad(a >> 24, b >> 24);
    return sum;
}

/* For ARMv6 SEL instruction.  */
uint32_t HELPER(sel_flags)(uint32_t flags, uint32_t a, uint32_t b)
{
    uint32_t mask;

    mask = 0;
    if (flags & 1) {
        mask |= 0xff;
    }
    if (flags & 2) {
        mask |= 0xff00;
    }
    if (flags & 4) {
        mask |= 0xff0000;
    }
    if (flags & 8) {
        mask |= 0xff000000;
    }
    return (a & mask) | (b & ~mask);
}

uint32_t HELPER(logicq_cc)(uint64_t val)
{
    return (val >> 32) | (val != 0);
}

/* VFP support.  We follow the convention used for VFP instrunctions:
   Single precition routines have a "s" suffix, double precision a
   "d" suffix.  */

/* Convert host exception flags to vfp form.  */
static inline int vfp_exceptbits_from_host(int host_bits)
{
    int target_bits = 0;

    if (host_bits & float_flag_invalid) {
        target_bits |= 1;
    }
    if (host_bits & float_flag_divbyzero) {
        target_bits |= 2;
    }
    if (host_bits & float_flag_overflow) {
        target_bits |= 4;
    }
    if (host_bits & (float_flag_underflow | float_flag_output_denormal)) {
        target_bits |= 8;
    }
    if (host_bits & float_flag_inexact) {
        target_bits |= 0x10;
    }
    if (host_bits & float_flag_input_denormal) {
        target_bits |= 0x80;
    }
    return target_bits;
}

uint32_t HELPER(vfp_get_fpscr)(CPUState * env)
{
    int i;
    uint32_t fpscr;

    fpscr = (env->vfp.xregs[ARM_VFP_FPSCR] & 0xffc8ffff) | (env->vfp.vec_len << 16) | (env->vfp.vec_stride << 20);
    i = get_float_exception_flags(&env->vfp.fp_status);
    i |= get_float_exception_flags(&env->vfp.standard_fp_status);
    fpscr |= vfp_exceptbits_from_host(i);
    return fpscr;
}

uint32_t vfp_get_fpscr(CPUState *env)
{
    return HELPER(vfp_get_fpscr)(env);
}

#ifdef TARGET_PROTO_ARM_M
void vfp_trigger_exception()
{
    /* Number of an NVIC interrupt that should be triggered when an fpu exception occures
     * On some platforms this line is not physically connected (eg. STM32H7 - errata ES0392 Rev 8,
     * 2.1.2 Cortex-M7 FPU interrupt not present on NVIC line 81), so a negative value means
     * don't trigger the interrupt
     */
    if (unlikely(cpu->vfp.fpu_interrupt_irq_number >= 0)) {
        /* This interrupt is an external interrupt. We add 16 to offset this number
         * and allow the user to pass IRQ numbers from the board's documentation
         */
        tlib_nvic_set_pending_irq(16 + cpu->vfp.fpu_interrupt_irq_number);
    }
}
#endif

/* Convert vfp exception flags to target form.  */
static inline int vfp_exceptbits_to_host(int target_bits)
{
    int host_bits = 0;

    if (target_bits & 1) {
        host_bits |= float_flag_invalid;
    }
    if (target_bits & 2) {
        host_bits |= float_flag_divbyzero;
    }
    if (target_bits & 4) {
        host_bits |= float_flag_overflow;
    }
    if (target_bits & 8) {
        host_bits |= float_flag_underflow;
    }
    if (target_bits & 0x10) {
        host_bits |= float_flag_inexact;
    }
    if (target_bits & 0x80) {
        host_bits |= float_flag_input_denormal;
    }
    return host_bits;
}

void HELPER(vfp_set_fpscr)(CPUState * env, uint32_t val)
{
    int i;
    uint32_t changed;

    changed = env->vfp.xregs[ARM_VFP_FPSCR];
    env->vfp.xregs[ARM_VFP_FPSCR] = (val & 0xffc8ffff);
    env->vfp.vec_len = (val >> 16) & 7;
    env->vfp.vec_stride = (val >> 20) & 3;

    changed ^= val;
    if (changed & (3 << 22)) {
        i = (val >> 22) & 3;
        switch (i) {
        case 0:
            i = float_round_nearest_even;
            break;
        case 1:
            i = float_round_up;
            break;
        case 2:
            i = float_round_down;
            break;
        case 3:
            i = float_round_to_zero;
            break;
        }
        set_float_rounding_mode(i, &env->vfp.fp_status);
    }
    if (changed & (1 << 24)) {
        set_flush_to_zero((val & (1 << 24)) != 0, &env->vfp.fp_status);
        set_flush_inputs_to_zero((val & (1 << 24)) != 0, &env->vfp.fp_status);
    }
    if (changed & (1 << 25)) {
        set_default_nan_mode((val & (1 << 25)) != 0, &env->vfp.fp_status);
    }

    i = vfp_exceptbits_to_host(val);
    set_float_exception_flags(i, &env->vfp.fp_status);
    set_float_exception_flags(0, &env->vfp.standard_fp_status);
}

void vfp_set_fpscr(CPUState *env, uint32_t val)
{
    HELPER(vfp_set_fpscr)(env, val);
}

#define VFP_HELPER(name, p) HELPER(glue(glue(vfp_,name),p))

#define VFP_BINOP(name) \
float32 VFP_HELPER(name, s)(float32 a, float32 b, void *fpstp) \
{ \
    float_status *fpst = fpstp; \
    return float32_ ## name(a, b, fpst); \
} \
float64 VFP_HELPER(name, d)(float64 a, float64 b, void *fpstp) \
{ \
    float_status *fpst = fpstp; \
    return float64_ ## name(a, b, fpst); \
}
VFP_BINOP(add)
VFP_BINOP(sub)
VFP_BINOP(mul)
VFP_BINOP(div)
#undef VFP_BINOP

float32 VFP_HELPER(neg, s)(float32 a)
{
    return float32_chs(a);
}

float64 VFP_HELPER(neg, d)(float64 a)
{
    return float64_chs(a);
}

float32 VFP_HELPER(abs, s)(float32 a)
{
    return float32_abs(a);
}

float64 VFP_HELPER(abs, d)(float64 a)
{
    return float64_abs(a);
}

float32 VFP_HELPER(sqrt, s)(float32 a, CPUState *env)
{
    return float32_sqrt(a, &env->vfp.fp_status);
}

float64 VFP_HELPER(sqrt, d)(float64 a, CPUState *env)
{
    return float64_sqrt(a, &env->vfp.fp_status);
}

/* XXX: check quiet/signaling case */
#define DO_VFP_cmp(p, type) \
void VFP_HELPER(cmp, p)(type a, type b, CPUState *env)  \
{ \
    uint32_t flags; \
    switch(type ## _compare_quiet(a, b, &env->vfp.fp_status)) { \
    case 0: flags = 0x6; break; \
    case -1: flags = 0x8; break; \
    case 1: flags = 0x2; break; \
    default: case 2: flags = 0x3; break; \
    } \
    env->vfp.xregs[ARM_VFP_FPSCR] = (flags << 28) \
        | (env->vfp.xregs[ARM_VFP_FPSCR] & 0x0fffffff); \
} \
void VFP_HELPER(cmpe, p)(type a, type b, CPUState *env) \
{ \
    uint32_t flags; \
    switch(type ## _compare(a, b, &env->vfp.fp_status)) { \
    case 0: flags = 0x6; break; \
    case -1: flags = 0x8; break; \
    case 1: flags = 0x2; break; \
    default: case 2: flags = 0x3; break; \
    } \
    env->vfp.xregs[ARM_VFP_FPSCR] = (flags << 28) \
        | (env->vfp.xregs[ARM_VFP_FPSCR] & 0x0fffffff); \
}
DO_VFP_cmp(s, float32)
DO_VFP_cmp(d, float64)
#undef DO_VFP_cmp

/* Integer to float and float to integer conversions */

#define CONV_ITOF(name, fsz, sign) \
    float##fsz HELPER(name)(uint32_t x, void *fpstp) \
{ \
    float_status *fpst = fpstp; \
    return sign##int32_to_##float##fsz(x, fpst); \
}

#define CONV_FTOI(name, fsz, sign, round) \
uint32_t HELPER(name)(float##fsz x, void *fpstp) \
{ \
    float_status *fpst = fpstp; \
    if (float##fsz##_is_any_nan(x)) { \
        float_raise(float_flag_invalid, fpst); \
        return 0; \
    } \
    return float##fsz##_to_##sign##int32##round(x, fpst); \
}

#define FLOAT_CONVS(name, p, fsz, sign) \
CONV_ITOF(vfp_##name##to##p, fsz, sign) \
CONV_FTOI(vfp_to##name##p, fsz, sign, ) \
CONV_FTOI(vfp_to##name##z##p, fsz, sign, _round_to_zero)

FLOAT_CONVS(si, s, 32, )
FLOAT_CONVS(si, d, 64, )
FLOAT_CONVS(ui, s, 32, u)
FLOAT_CONVS(ui, d, 64, u)

#undef CONV_ITOF
#undef CONV_FTOI
#undef FLOAT_CONVS

/* floating point conversion */
float64 VFP_HELPER(fcvtd, s)(float32 x, CPUState *env)
{
    float64 r = float32_to_float64(x, &env->vfp.fp_status);
    /* ARM requires that S<->D conversion of any kind of NaN generates
     * a quiet NaN by forcing the most significant frac bit to 1.
     */
    return float64_maybe_silence_nan(r, &env->vfp.fp_status);
}

float32 VFP_HELPER(fcvts, d)(float64 x, CPUState *env)
{
    float32 r =  float64_to_float32(x, &env->vfp.fp_status);
    /* ARM requires that S<->D conversion of any kind of NaN generates
     * a quiet NaN by forcing the most significant frac bit to 1.
     */
    return float32_maybe_silence_nan(r, &env->vfp.fp_status);
}

/* VFP3 fixed point conversion.  */
#define VFP_CONV_FIX(name, p, fsz, itype, sign) \
float##fsz HELPER(vfp_##name##to##p)(uint##fsz##_t  x, uint32_t shift, \
                                    void *fpstp) \
{ \
    float_status *fpst = fpstp; \
    float##fsz tmp; \
    tmp = sign##int32_to_##float##fsz((itype##_t)x, fpst); \
    return float##fsz##_scalbn(tmp, -(int)shift, fpst); \
} \
uint##fsz##_t HELPER(vfp_to##name##p)(float##fsz x, uint32_t shift, \
                                       void *fpstp) \
{ \
    float_status *fpst = fpstp; \
    float##fsz tmp; \
    if (float##fsz##_is_any_nan(x)) { \
        float_raise(float_flag_invalid, fpst); \
        return 0; \
    } \
    tmp = float##fsz##_scalbn(x, shift, fpst); \
    return float##fsz##_to_##itype##_round_to_zero(tmp, fpst); \
}

VFP_CONV_FIX(sh, d, 64, int16, )
VFP_CONV_FIX(sl, d, 64, int32, )
VFP_CONV_FIX(uh, d, 64, uint16, u)
VFP_CONV_FIX(ul, d, 64, uint32, u)
VFP_CONV_FIX(sh, s, 32, int16, )
VFP_CONV_FIX(sl, s, 32, int32, )
VFP_CONV_FIX(uh, s, 32, uint16, u)
VFP_CONV_FIX(ul, s, 32, uint32, u)
#undef VFP_CONV_FIX

/* Half precision conversions.  */
static float32 do_fcvt_f16_to_f32(uint32_t a, CPUState *env, float_status *s)
{
    int ieee = (env->vfp.xregs[ARM_VFP_FPSCR] & (1 << 26)) == 0;
    float32 r = float16_to_float32(make_float16(a), ieee, s);
    if (ieee) {
        return float32_maybe_silence_nan(r, s);
    }
    return r;
}

static uint32_t do_fcvt_f32_to_f16(float32 a, CPUState *env, float_status *s)
{
    int ieee = (env->vfp.xregs[ARM_VFP_FPSCR] & (1 << 26)) == 0;
    float16 r = float32_to_float16(a, ieee, s);
    if (ieee) {
        r = float16_maybe_silence_nan(r, s);
    }
    return float16_val(r);
}

float32 HELPER(neon_fcvt_f16_to_f32)(uint32_t a, CPUState *env)
{
    return do_fcvt_f16_to_f32(a, env, &env->vfp.standard_fp_status);
}

uint32_t HELPER(neon_fcvt_f32_to_f16)(float32 a, CPUState *env)
{
    return do_fcvt_f32_to_f16(a, env, &env->vfp.standard_fp_status);
}

float32 HELPER(vfp_fcvt_f16_to_f32)(uint32_t a, CPUState *env)
{
    return do_fcvt_f16_to_f32(a, env, &env->vfp.fp_status);
}

uint32_t HELPER(vfp_fcvt_f32_to_f16)(float32 a, CPUState *env)
{
    return do_fcvt_f32_to_f16(a, env, &env->vfp.fp_status);
}

float32 HELPER(recps_f32)(float32 a, float32 b, CPUState *env)
{
    float_status *s = &env->vfp.standard_fp_status;
    if ((float32_is_infinity(a) && float32_is_zero_or_denormal(b)) ||
        (float32_is_infinity(b) && float32_is_zero_or_denormal(a))) {
        if (!(float32_is_zero(a) || float32_is_zero(b))) {
            float_raise(float_flag_input_denormal, s);
        }
        return float32_two;
    }
    return float32_sub(float32_two, float32_mul(a, b, s), s);
}

float32 HELPER(rsqrts_f32)(float32 a, float32 b, CPUState *env)
{
    float_status *s = &env->vfp.standard_fp_status;
    float32 product;
    if ((float32_is_infinity(a) && float32_is_zero_or_denormal(b)) ||
        (float32_is_infinity(b) && float32_is_zero_or_denormal(a))) {
        if (!(float32_is_zero(a) || float32_is_zero(b))) {
            float_raise(float_flag_input_denormal, s);
        }
        return float32_one_point_five;
    }
    product = float32_mul(a, b, s);
    return float32_div(float32_sub(float32_three, product, s), float32_two, s);
}

/* NEON helpers.  */

/* Constants 256 and 512 are used in some helpers; we avoid relying on
 * int->float conversions at run-time.  */
#define float64_256 make_float64(0x4070000000000000LL)
#define float64_512 make_float64(0x4080000000000000LL)

/* The algorithm that must be used to calculate the estimate
 * is specified by the ARM ARM.
 */
static float64 recip_estimate(float64 a, CPUState *env)
{
    /* These calculations mustn't set any fp exception flags,
     * so we use a local copy of the fp_status.
     */
    float_status dummy_status = env->vfp.standard_fp_status;
    float_status *s = &dummy_status;
    /* q = (int)(a * 512.0) */
    float64 q = float64_mul(float64_512, a, s);
    int64_t q_int = float64_to_int64_round_to_zero(q, s);

    /* r = 1.0 / (((double)q + 0.5) / 512.0) */
    q = int64_to_float64(q_int, s);
    q = float64_add(q, float64_half, s);
    q = float64_div(q, float64_512, s);
    q = float64_div(float64_one, q, s);

    /* s = (int)(256.0 * r + 0.5) */
    q = float64_mul(q, float64_256, s);
    q = float64_add(q, float64_half, s);
    q_int = float64_to_int64_round_to_zero(q, s);

    /* return (double)s / 256.0 */
    return float64_div(int64_to_float64(q_int, s), float64_256, s);
}

float32 HELPER(recpe_f32)(float32 a, CPUState *env)
{
    float_status *s = &env->vfp.standard_fp_status;
    float64 f64;
    uint32_t val32 = float32_val(a);

    int result_exp;
    int a_exp = (val32  & 0x7f800000) >> 23;
    int sign = val32 & 0x80000000;

    if (float32_is_any_nan(a)) {
        if (float32_is_signaling_nan(a, s)) {
            float_raise(float_flag_invalid, s);
        }
        return float32_default_nan;
    } else if (float32_is_infinity(a)) {
        return float32_set_sign(float32_zero, float32_is_neg(a));
    } else if (float32_is_zero_or_denormal(a)) {
        if (!float32_is_zero(a)) {
            float_raise(float_flag_input_denormal, s);
        }
        float_raise(float_flag_divbyzero, s);
        return float32_set_sign(float32_infinity, float32_is_neg(a));
    } else if (a_exp >= 253) {
        float_raise(float_flag_underflow, s);
        return float32_set_sign(float32_zero, float32_is_neg(a));
    }

    f64 = make_float64((0x3feULL << 52) | ((int64_t)(val32 & 0x7fffff) << 29));

    result_exp = 253 - a_exp;

    f64 = recip_estimate(f64, env);

    val32 = sign | ((result_exp & 0xff) << 23) | ((float64_val(f64) >> 29) & 0x7fffff);
    return make_float32(val32);
}

/* The algorithm that must be used to calculate the estimate
 * is specified by the ARM ARM.
 */
static float64 recip_sqrt_estimate(float64 a, CPUState *env)
{
    /* These calculations mustn't set any fp exception flags,
     * so we use a local copy of the fp_status.
     */
    float_status dummy_status = env->vfp.standard_fp_status;
    float_status *s = &dummy_status;
    float64 q;
    int64_t q_int;

    if (float64_lt(a, float64_half, s)) {
        /* range 0.25 <= a < 0.5 */

        /* a in units of 1/512 rounded down */
        /* q0 = (int)(a * 512.0);  */
        q = float64_mul(float64_512, a, s);
        q_int = float64_to_int64_round_to_zero(q, s);

        /* reciprocal root r */
        /* r = 1.0 / sqrt(((double)q0 + 0.5) / 512.0);  */
        q = int64_to_float64(q_int, s);
        q = float64_add(q, float64_half, s);
        q = float64_div(q, float64_512, s);
        q = float64_sqrt(q, s);
        q = float64_div(float64_one, q, s);
    } else {
        /* range 0.5 <= a < 1.0 */

        /* a in units of 1/256 rounded down */
        /* q1 = (int)(a * 256.0); */
        q = float64_mul(float64_256, a, s);
        int64_t q_int = float64_to_int64_round_to_zero(q, s);

        /* reciprocal root r */
        /* r = 1.0 /sqrt(((double)q1 + 0.5) / 256); */
        q = int64_to_float64(q_int, s);
        q = float64_add(q, float64_half, s);
        q = float64_div(q, float64_256, s);
        q = float64_sqrt(q, s);
        q = float64_div(float64_one, q, s);
    }
    /* r in units of 1/256 rounded to nearest */
    /* s = (int)(256.0 * r + 0.5); */

    q = float64_mul(q, float64_256, s);
    q = float64_add(q, float64_half, s);
    q_int = float64_to_int64_round_to_zero(q, s);

    /* return (double)s / 256.0;*/
    return float64_div(int64_to_float64(q_int, s), float64_256, s);
}

float32 HELPER(rsqrte_f32)(float32 a, CPUState *env)
{
    float_status *s = &env->vfp.standard_fp_status;
    int result_exp;
    float64 f64;
    uint32_t val;
    uint64_t val64;

    val = float32_val(a);

    if (float32_is_any_nan(a)) {
        if (float32_is_signaling_nan(a, s)) {
            float_raise(float_flag_invalid, s);
        }
        return float32_default_nan;
    } else if (float32_is_zero_or_denormal(a)) {
        if (!float32_is_zero(a)) {
            float_raise(float_flag_input_denormal, s);
        }
        float_raise(float_flag_divbyzero, s);
        return float32_set_sign(float32_infinity, float32_is_neg(a));
    } else if (float32_is_neg(a)) {
        float_raise(float_flag_invalid, s);
        return float32_default_nan;
    } else if (float32_is_infinity(a)) {
        return float32_zero;
    }

    /* Normalize to a double-precision value between 0.25 and 1.0,
     * preserving the parity of the exponent.  */
    if ((val & 0x800000) == 0) {
        f64 = make_float64(((uint64_t)(val & 0x80000000) << 32) | (0x3feULL << 52) | ((uint64_t)(val & 0x7fffff) << 29));
    } else {
        f64 = make_float64(((uint64_t)(val & 0x80000000) << 32) | (0x3fdULL << 52) | ((uint64_t)(val & 0x7fffff) << 29));
    }

    result_exp = (380 - ((val & 0x7f800000) >> 23)) / 2;

    f64 = recip_sqrt_estimate(f64, env);

    val64 = float64_val(f64);

    val = ((result_exp & 0xff) << 23) | ((val64 >> 29)  & 0x7fffff);
    return make_float32(val);
}

uint32_t HELPER(recpe_u32)(uint32_t a, CPUState *env)
{
    float64 f64;

    if ((a & 0x80000000) == 0) {
        return 0xffffffff;
    }

    f64 = make_float64((0x3feULL << 52) | ((int64_t)(a & 0x7fffffff) << 21));

    f64 = recip_estimate(f64, env);

    return 0x80000000 | ((float64_val(f64) >> 21) & 0x7fffffff);
}

uint32_t HELPER(rsqrte_u32)(uint32_t a, CPUState *env)
{
    float64 f64;

    if ((a & 0xc0000000) == 0) {
        return 0xffffffff;
    }

    if (a & 0x80000000) {
        f64 = make_float64((0x3feULL << 52) | ((uint64_t)(a & 0x7fffffff) << 21));
    } else { /* bits 31-30 == '01' */
        f64 = make_float64((0x3fdULL << 52) | ((uint64_t)(a & 0x3fffffff) << 22));
    }

    f64 = recip_sqrt_estimate(f64, env);

    return 0x80000000 | ((float64_val(f64) >> 21) & 0x7fffffff);
}

/* VFPv4 fused multiply-accumulate */
float32 VFP_HELPER(muladd, s)(float32 a, float32 b, float32 c, void *fpstp)
{
    float_status *fpst = fpstp;
    return float32_muladd(a, b, c, 0, fpst);
}

float64 VFP_HELPER(muladd, d)(float64 a, float64 b, float64 c, void *fpstp)
{
    float_status *fpst = fpstp;
    return float64_muladd(a, b, c, 0, fpst);
}

void HELPER(set_teecr)(CPUState * env, uint32_t val)
{
    val &= 1;
    if (env->teecr != val) {
        env->teecr = val;
        tb_flush(env);
    }
}

#ifdef TARGET_PROTO_ARM_M
uint32_t HELPER(v8m_tt)(CPUState *env, uint32_t addr, uint32_t op)
{
    int prot;
    bool priv_access;
    uint32_t phys_ptr; /* Not used, but needed for get_phys_addr_mpu */
    bool a,t;
    int resolved_region;
    bool multiple_regions;

    if (!PMSA_ENABLED(env->pmsav8.ctrl)) {
        goto invalid;
    }

    /* From "Armv8-M Architecture Reference Manual"
     * The padding is a placeholder for fields that 
     * are only used in Secure Mode (currently unsupported)
     */
    union {
        struct {
            unsigned mpu_region : 8;
            unsigned : 8;
            unsigned mpu_region_valid : 1;
            unsigned : 1;
            unsigned read_ok : 1;
            unsigned readwrite_ok : 1;
            unsigned : 12;
        } flags;
        uint32_t value;
    } addr_info;

    /* Decode instruction variant
     * TT:    a == 0 && t == 0
     * TTA:   a == 1 && t == 0 *unimpl*
     * TTT:   a == 0 && t == 1
     * TTAT:  a == 1 && t == 1 *unimpl*
     */
    a = op & 0b10;
    t = op & 0b01;

    /* Alternate Domain (A) variants are not supported */
    if (a) {
        cpu_abort(env, "TTA and TTAT instructions are not supported");
    }

    if (t) {
        /* Force user access */
        priv_access = false;
    } else {
        /* Check privilege level for the M profile, return true otherwise */
        priv_access = in_privileged_mode(env);
    }

    if (!pmsav8_get_region(env, addr, &resolved_region, &multiple_regions) || multiple_regions) {
        /* No region hit or multiple regions */
        goto invalid;
    }
    addr_info.flags.mpu_region = resolved_region;
    addr_info.flags.mpu_region_valid = true;

    pmsav8_get_phys_addr(env, addr, PAGE_READ, !priv_access, &phys_ptr, &prot);
    addr_info.flags.read_ok = (prot & (1 << PAGE_READ)) != 0;
    addr_info.flags.readwrite_ok = addr_info.flags.read_ok && (prot & (1 << PAGE_WRITE)) != 0;

    return addr_info.value;

invalid:
    /* The Arm® v8-M Architecture Reference Manual specifies that if MREGION content is not valid if:
     * - The MPU is not implemented or MPU_CTRL.ENABLE is set to zero,
     * - The address specified by the TT instruction variant does not match any enabled MPU regions,
     * - The address matched multiple MPU regions,
     * - The TT or TTT instruction variants, without the A flag specified, were executed from an unprivileged mode. *unimpl*
     * In this case R and RW fields are RAZ
     */
    return 0;
}
#endif

void tlib_arch_dispose()
{
    ttable_remove(cpu->cp_regs);
}

void HELPER(set_system_event)(void)
{
    tlib_set_system_event(1);
}
