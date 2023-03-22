/*
 * Copyright (c) Antmicro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <limits.h>

#include "cpu.h"
#include "helper.h"
#include "mmu.h"
#include "syndrome.h"
#include "system_registers.h"

uint64_t arm_sctlr(struct CPUState *env, int el)
{
    tlib_assert(el >= 0 && el <= 3);
    if (el == 0) {
        el = arm_is_el2_enabled(env) && hcr_e2h_and_tge_set(env) ? 2 : 1;
    }
    return env->cp15.sctlr_el[el];
}

void HELPER(exception_bkpt_insn)(CPUARMState *env, uint32_t syndrome)
{
    helper_exception_with_syndrome(env, EXCP_BKPT, syndrome);
}

void HELPER(memory_barrier_assert)(CPUARMState *env)
{
    // A safety measure not to forget this isn't really implemented.
    tlib_assert((env->current_tb->cflags & CF_PARALLEL) == 0);
}

/* Functions called by arch-independent code. */

void cpu_exec_epilogue(CPUState *env)
{
    // Intentionally left blank.
}

void cpu_exec_prologue(CPUState *env)
{
    // Intentionally left blank.
}

void cpu_reset(CPUState *env)
{
    cpu_reset_state(env);
    cpu_reset_vfp(env);
    system_instructions_and_registers_reset(env);

    // TODO? 64-bit ARMv8 can start with AArch32 based on the AA64nAA32 configuration signal.
    if (arm_feature(env, ARM_FEATURE_AARCH64)) {
        cpu_reset_v8_a64(env);
    }
    arm_rebuild_hflags(env);
}

void do_interrupt(CPUState *env)
{
    do_interrupt_v8a(env);
}

int process_interrupt(int interrupt_request, CPUState *env)
{
    return process_interrupt_v8a(interrupt_request, env);
}

void tlib_arch_dispose()
{
    ttable_remove(cpu->arm_core_config->cp_regs);
    tlib_free(cpu->arm_core_config);
}

/* CPU initialization and reset. */

#include "cpu_names.h"

void cpu_init_a75_a76(CPUState *env, uint32_t id)
{
    set_feature(env, ARM_FEATURE_AARCH64);
    set_feature(env, ARM_FEATURE_V8);
    set_feature(env, ARM_FEATURE_NEON);
    set_feature(env, ARM_FEATURE_GENERIC_TIMER);
    set_feature(env, ARM_FEATURE_CBAR_RO);
    set_feature(env, ARM_FEATURE_EL2);
    set_feature(env, ARM_FEATURE_EL3);
    set_feature(env, ARM_FEATURE_PMU);

    // From B2.4 AArch64 registers
    env->arm_core_config->clidr = 0x82000023;
    env->arm_core_config->ctr = 0x8444C004;
    env->arm_core_config->dcz_blocksize = 4;
    env->arm_core_config->id_aa64afr0 = 0;
    env->arm_core_config->id_aa64afr1 = 0;
    env->arm_core_config->isar.id_aa64dfr0  = 0x0000000010305408ull;
    env->arm_core_config->isar.id_aa64isar0 = 0x0000100010210000ull;
    env->arm_core_config->isar.id_aa64isar1 = 0x0000000000100001ull;
    env->arm_core_config->isar.id_aa64mmfr0 = 0x0000000000101122ull;
    env->arm_core_config->isar.id_aa64mmfr1 = 0x0000000010212122ull;
    env->arm_core_config->isar.id_aa64mmfr2 = 0x0000000000001011ull;
    env->arm_core_config->isar.id_aa64pfr0  = 0x1100000010111112ull;
    env->arm_core_config->isar.id_aa64pfr1  = 0x0000000000000010ull;
    env->arm_core_config->id_afr0       = 0x00000000;
    env->arm_core_config->isar.id_dfr0  = 0x04010088;
    env->arm_core_config->isar.id_isar0 = 0x02101110;
    env->arm_core_config->isar.id_isar1 = 0x13112111;
    env->arm_core_config->isar.id_isar2 = 0x21232042;
    env->arm_core_config->isar.id_isar3 = 0x01112131;
    env->arm_core_config->isar.id_isar4 = 0x00010142;
    env->arm_core_config->isar.id_isar5 = 0x01010001;
    env->arm_core_config->isar.id_isar6 = 0x00000010;
    env->arm_core_config->isar.id_mmfr0 = 0x10201105;
    env->arm_core_config->isar.id_mmfr1 = 0x40000000;
    env->arm_core_config->isar.id_mmfr2 = 0x01260000;
    env->arm_core_config->isar.id_mmfr3 = 0x02122211;
    env->arm_core_config->isar.id_mmfr4 = 0x00021110;
    env->arm_core_config->isar.id_pfr0  = 0x10010131;
    env->arm_core_config->isar.id_pfr1  = 0x00010000;
    env->arm_core_config->isar.id_pfr2  = 0x00000011;

    // TODO: MPIDR should depend on CPUID, CLUSTERIDAFF2 and CLUSTERIDAFF3 configuration signals.
    env->arm_core_config->mpidr = (1u << 31) /* RES1 */ | (0u << 30) /* U */ | (1u << 24) /* MT */;
    env->arm_core_config->revidr = 0;

    // From B2.23
    env->arm_core_config->ccsidr[0] = 0x701fe01a;
    env->arm_core_config->ccsidr[1] = 0x201fe01a;
    env->arm_core_config->ccsidr[2] = 0x707fe03a;

    // From B2.97
    // Bit 20 is RES1 in both A75 and A76 for SCTLR, not sure why Qemu uses 0x30c50838.
    env->arm_core_config->reset_sctlr = 0x30d50838;

    // From B4.23
    env->arm_core_config->gic_num_lrs = 4;
    env->arm_core_config->gic_vpribits = 5;
    env->arm_core_config->gic_vprebits = 5;
    // From B4.7
    env->arm_core_config->gic_pribits = 5;

    // From B5.1
    env->arm_core_config->isar.mvfr0 = 0x10110222;
    env->arm_core_config->isar.mvfr1 = 0x13211111;
    env->arm_core_config->isar.mvfr2 = 0x00000043;

    // From D5.1
    env->arm_core_config->pmceid0 = 0x7FFF0F3F;
    env->arm_core_config->pmceid1 = 0x00F2AE7F;

    // From D5.4
    env->arm_core_config->isar.reset_pmcr_el0 = 0x410b3000;

    // TODO: Add missing ones? reset_fpsid, reset_cbar, reset_auxcr, reset_hivecs
    // reset_cbar should be based on GIC PERIPHBASE signal.
}

void cpu_init_a53(CPUState *env, uint32_t id)
{
    set_feature(env, ARM_FEATURE_AARCH64);
    set_feature(env, ARM_FEATURE_V8);
    set_feature(env, ARM_FEATURE_NEON);
    set_feature(env, ARM_FEATURE_GENERIC_TIMER);
    set_feature(env, ARM_FEATURE_CBAR_RO);
    set_feature(env, ARM_FEATURE_EL2);
    set_feature(env, ARM_FEATURE_EL3);
    set_feature(env, ARM_FEATURE_PMU);

    env->arm_core_config->clidr = 0x0A200023;
    env->arm_core_config->ctr = 0x84448004;
    env->arm_core_config->dcz_blocksize = 4;
    env->arm_core_config->id_aa64afr0 = 0;
    env->arm_core_config->id_aa64afr1 = 0;
    env->arm_core_config->isar.id_aa64dfr0  = 0x10305106ull;
    env->arm_core_config->isar.id_aa64isar0 = 0x00011120ull;
    env->arm_core_config->isar.id_aa64isar1 = 0x00000000ull;
    env->arm_core_config->isar.id_aa64mmfr0 = 0x00001122ull;
    env->arm_core_config->isar.id_aa64mmfr1 = 0x00000000ull;
    env->arm_core_config->isar.id_aa64pfr0  = 0x00002222ull;
    env->arm_core_config->isar.id_aa64pfr1  = 0x00000000ull;
    env->arm_core_config->id_afr0       = 0x00000000;
    env->arm_core_config->isar.id_dfr0  = 0x03010066;
    env->arm_core_config->isar.id_isar0 = 0x02101110;
    env->arm_core_config->isar.id_isar1 = 0x13112111;
    env->arm_core_config->isar.id_isar2 = 0x21232042;
    env->arm_core_config->isar.id_isar3 = 0x01112131;
    env->arm_core_config->isar.id_isar4 = 0x00011142;
    env->arm_core_config->isar.id_isar5 = 0x00011121;
    env->arm_core_config->isar.id_mmfr0 = 0x10201105;
    env->arm_core_config->isar.id_mmfr1 = 0x40000000;
    env->arm_core_config->isar.id_mmfr2 = 0x01260000;
    env->arm_core_config->isar.id_mmfr3 = 0x02102211;
    env->arm_core_config->isar.id_pfr0  = 0x00000131;
    env->arm_core_config->isar.id_pfr1  = 0x10011011;

    // TODO: MPIDR should depend on CPUID, CLUSTERIDAFF2 and CLUSTERIDAFF3 configuration signals.
    env->arm_core_config->mpidr = (1u << 31) /* RES1 */ | (0u << 30) /* U */ | (0u << 24) /* MT */;
    env->arm_core_config->revidr = 0;

    env->arm_core_config->ccsidr[0] = 0x700fe01a;
    env->arm_core_config->ccsidr[1] = 0x201fe01a;
    env->arm_core_config->ccsidr[2] = 0x707fe07a;

    env->arm_core_config->reset_sctlr = 0x00C50838;

    env->arm_core_config->gic_num_lrs = 4;
    env->arm_core_config->gic_vpribits = 5;
    env->arm_core_config->gic_vprebits = 5;
    env->arm_core_config->gic_pribits = 5;

    env->arm_core_config->isar.mvfr0 = 0x10110222;
    env->arm_core_config->isar.mvfr1 = 0x13211111;
    env->arm_core_config->isar.mvfr2 = 0x00000043;

    env->arm_core_config->pmceid0 = 0x7FFF0F3F;
    env->arm_core_config->pmceid1 = 0x00F2AE7F;

    env->arm_core_config->isar.reset_pmcr_el0 = 0x41033000;

    env->arm_core_config->midr = 0x410FD034;
}

static void cpu_init_core_config(CPUState *env, uint32_t id)
{
    env->arm_core_config = tlib_mallocz(sizeof(ARMCoreConfig));

    // Main ID Register.
    env->arm_core_config->midr = id;

    switch (id) {
    case ARM_CPUID_CORTEXA53:
        cpu_init_a53(env, id);
        break;
    case ARM_CPUID_CORTEXA75:
    case ARM_CPUID_CORTEXA76:
        cpu_init_a75_a76(env, id);
        break;
    default:
        cpu_abort(env, "Bad CPU ID: %x\n", id);
        break;
    }
}

void cpu_init_v8(CPUState *env, uint32_t id)
{
    cpu_init_core_config(env, id);
    system_instructions_and_registers_init(env, id);
}

void cpu_reset_state(CPUState *env)
{
    // Let's preserve arm_core_config, features and CPU ID.
    ARMCoreConfig *config = env->arm_core_config;
    uint64_t features = env->features;
    uint32_t id = env->cp15.c0_cpuid;

    memset(env, 0, offsetof(CPUState, breakpoints));

    // Based on 'gen_clrex' and 'gen_store_exclusive' it seems -1 means the address isn't valid.
    env->exclusive_addr = -1;

    // Restore preserved fields.
    env->arm_core_config = config;
    env->features = features;
    env->cp15.c0_cpuid = id;
}

void cpu_reset_v8_a64(CPUState *env)
{
    tlib_assert(arm_feature(env, ARM_FEATURE_AARCH64));

    uint32_t pstate;

    env->aarch64 = 1;

    // Reset values of some registers are defined per CPU model.
    env->cp15.sctlr_el[1] = env->arm_core_config->reset_sctlr;
    env->cp15.sctlr_el[2] = env->arm_core_config->reset_sctlr;
    env->cp15.sctlr_el[3] = env->arm_core_config->reset_sctlr;
    env->cp15.vmpidr_el2 = env->arm_core_config->mpidr;
    env->cp15.vpidr_el2 = env->arm_core_config->midr;
    env->cp15.c9_pmcr = env->arm_core_config->isar.reset_pmcr_el0;

    // The default reset state for AArch64 is the highest available ELx (handler=true: use SP_ELx).
    pstate = aarch64_pstate_mode(arm_highest_el(env), true);

    // Reset value for each of the Interrupt Mask Bits (DAIF) is 1.
    pstate |= PSTATE_DAIF;

    // Zero flag should be unset after reset.
    // It's interpreted as set if PSTATE_Z bit is zero.
    pstate |= PSTATE_Z;

    pstate_write(env, pstate);
}
