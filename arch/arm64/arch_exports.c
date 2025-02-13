/*
 * Copyright (c) Antmicro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "cpu.h"
#include "system_registers.h"
#include "../arm_common/tightly_coupled_memory.h"
#include "../../unwind.h"

uint32_t tlib_check_system_register_access(const char *name, bool is_write)
{
    enum {
        REGISTER_NOT_FOUND = 1,
        ACCESSOR_NOT_FOUND = 2,
        ACCESS_VALID       = 3,
    };

    const ARMCPRegInfo *ri = sysreg_find_by_name(env, name);
    if (ri == NULL) {
        return REGISTER_NOT_FOUND;
    }

    if (ri->fieldoffset) {
        return ACCESS_VALID;
    }

    if (is_write) {
        bool write_possible = ri->writefn != NULL;
        return write_possible ? ACCESS_VALID : ACCESSOR_NOT_FOUND;
    } else {
        bool read_possible = (ri->readfn != NULL) || (ri->type & ARM_CP_CONST);
        return read_possible ? ACCESS_VALID : ACCESSOR_NOT_FOUND;
    }
}
EXC_INT_2(uint32_t, tlib_check_system_register_access, const char *, name, bool, is_write)

uint64_t tlib_get_system_register(const char *name)
{
    return sysreg_get_by_name(cpu, name);
}
EXC_INT_1(uint64_t, tlib_get_system_register, const char *, name)

uint32_t tlib_has_el3()
{
    return arm_feature(cpu, ARM_FEATURE_EL3);
}
EXC_INT_0(uint32_t, tlib_has_el3)

uint32_t tlib_set_available_els(bool el2_enabled, bool el3_enabled)
{
    enum {
        SIMULATION_ALREADY_STARTED = 1,
        SUCCESS                    = 3,
    };

    if (cpu->instructions_count_total_value != 0) {
        return SIMULATION_ALREADY_STARTED;
    }

    set_el_features(cpu, el2_enabled, el3_enabled);

    if (is_a64(env)) {
        // Reset the Exception Level a CPU starts in.
        uint32_t reset_el = arm_highest_el(cpu);
        cpu->pstate = deposit32(cpu->pstate, 2, 2, reset_el);

        tlib_on_execution_mode_changed(reset_el, arm_is_secure(env));
    } else {
        uint32_t reset_mode = arm_get_highest_cpu_mode(env);
        cpsr_write(env, reset_mode, CPSR_M, CPSRWriteRaw);
    }

    arm_rebuild_hflags(cpu);

    return SUCCESS;
}
EXC_INT_2(uint32_t, tlib_set_available_els, bool, el2_enabled, bool, el3_enabled)

void tlib_set_current_el(uint32_t el)
{
    pstate_set_el(cpu, el);
}
EXC_VOID_1(tlib_set_current_el, uint32_t, el)

void tlib_set_system_register(const char *name, uint64_t value)
{
    sysreg_set_by_name(cpu, name, value);
}
EXC_VOID_2(tlib_set_system_register, const char *, name, uint64_t, value)

void tlib_set_mpu_regions_count(uint32_t el1_regions_count, uint32_t el2_regions_count)
{
    if (el1_regions_count > MAX_MPU_REGIONS || el2_regions_count > MAX_MPU_REGIONS) {
        tlib_abortf("Unable to set MPU regions count to %d. Maximum value for this core is %d",
                    el1_regions_count > el2_regions_count ? el1_regions_count : el2_regions_count, MAX_MPU_REGIONS);
    }
    
    set_pmsav8_regions_count(cpu, el1_regions_count, el2_regions_count);
}
EXC_VOID_2(tlib_set_mpu_regions_count, uint32_t, count, uint32_t, hyp_count)

/* Based on the documentation for Cortex-R52 */
void tlib_register_tcm_region(uint32_t address, uint64_t size, uint64_t region_index)
{
    if (size == 0) {
        // Disable this region in the TCMTR
        cpu->cp15.tcm_type &= ~(1 << region_index);
        cpu->cp15.tcm_region[region_index] = 0;
    } else {
        validate_tcm_region(address, size ,region_index, TARGET_PAGE_SIZE);

        //Set this region as enabled
        cpu->cp15.tcm_type |= 1 << region_index;
        cpu->cp15.tcm_region[region_index] = address | (ctz64(size / TCM_UNIT_SIZE) << 2) | 3; // enable in all modes
    }

    // Set TCMS bit - one or more TCMS implemented
    if (cpu->cp15.tcm_type & 0x7) {
        cpu->cp15.tcm_type |= 1u << 31;
    } else {
        cpu->cp15.tcm_type &= ~(1u << 31);
    }
}

EXC_VOID_3(tlib_register_tcm_region, uint32_t, address, uint64_t, size, uint64_t, index)

