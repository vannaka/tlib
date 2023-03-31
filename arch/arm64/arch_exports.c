/*
 * Copyright (c) Antmicro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "cpu.h"
#include "system_registers.h"
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

uint32_t tlib_get_current_el()
{
    return arm_current_el(cpu);
}
EXC_INT_0(uint32_t, tlib_get_current_el)

uint64_t tlib_get_system_register(const char *name)
{
    return sysreg_get_by_name(cpu, name);
}
EXC_INT_1(uint64_t, tlib_get_system_register, const char *, name)

uint32_t tlib_is_secure_below_el3()
{
    return arm_is_secure_below_el3(cpu);
}
EXC_INT_0(uint32_t, tlib_is_secure_below_el3)

void tlib_set_available_els(bool el2_enabled, bool el3_enabled)
{
    if (cpu->instructions_count_total_value != 0) {
        tlib_printf(LOG_LEVEL_WARNING, "Available Exception Levels can only be set before running the simulation.");
        return;
    }

    set_el_features(cpu, el2_enabled, el3_enabled);

    // Reset the Exception Level a CPU starts in.
    uint32_t reset_el = arm_highest_el(cpu);
    cpu->pstate = deposit32(cpu->pstate, 2, 2, reset_el);
    arm_rebuild_hflags(cpu);
}
EXC_VOID_2(tlib_set_available_els, bool, el2_enabled, bool, el3_enabled)

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
