#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "helper.h"
#include "../arm64/system_registers_common.h"

void HELPER(set_cp_reg)(CPUState * env, void *rip, uint32_t value)
{
    const ARMCPRegInfo *ri = rip;

    if (ri->type & ARM_CP_IO) {
        // Use mutex if executed in parallel.
        ri->writefn(env, ri, value);
    } else {
        ri->writefn(env, ri, value);
    }
}

uint32_t HELPER(get_cp_reg)(CPUState * env, void *rip)
{
    const ARMCPRegInfo *ri = rip;
    uint32_t res;

    if (ri->type & ARM_CP_IO) {
        // Use mutex if executed in parallel.
        res = ri->readfn(env, ri);
    } else {
        res = ri->readfn(env, ri);
    }

    return res;
}

void HELPER(set_cp_reg64)(CPUState * env, void *rip, uint64_t value)
{
    const ARMCPRegInfo *ri = rip;

    if (ri->type & ARM_CP_IO) {
        // Use mutex if executed in parallel.
        ri->writefn(env, ri, value);
    } else {
        ri->writefn(env, ri, value);
    }
}

uint64_t HELPER(get_cp_reg64)(CPUState * env, void *rip)
{
    const ARMCPRegInfo *ri = rip;
    uint64_t res;

    if (ri->type & ARM_CP_IO) {
        // Use mutex if executed in parallel.
        res = ri->readfn(env, ri);
    } else {
        res = ri->readfn(env, ri);
    }

    return res;
}

// The keys are dynamically allocated so let's make TTable free them when removing the entry.
static void entry_remove_callback(TTable_entry *entry)
{
    tlib_free(entry->key);
}

void cp_regs_add(CPUState *env, ARMCPRegInfo *reg_info_array, uint32_t array_count)
{
    for (int i = 0; i < array_count; i++) {
        ARMCPRegInfo *reg_info = &reg_info_array[i];
        uint32_t *key = tlib_malloc(sizeof(uint32_t));

        bool ns = true; // TODO: Handle secure state banking in a correct way, when we add Secure Mode to this lib
        bool is64 = reg_info->type & ARM_CP_64BIT;
        *key = ENCODE_CP_REG(reg_info->cp, is64, ns, reg_info->crn, reg_info->crm, reg_info->op1, reg_info->op2);

        if (!ttable_insert_check(env->cp_regs, key, reg_info)) {
            tlib_abortf("Duplicated system_register definition!: name: %s, cp: %d, crn: %d, op1: %d, crm: %d, op2: %d, op0: %d",
                        reg_info->name, reg_info->cp, reg_info->crn, reg_info->op1, reg_info->crm, reg_info->op2, reg_info->op0);

            reg_info = ttable_lookup_value_eq(env->cp_regs, key);
            tlib_abortf("Previously defined as!:                 name: %s, cp: %d, crn: %d, op1: %d, crm: %d, op2: %d, op0: %d",
                        reg_info->name, reg_info->cp, reg_info->crn, reg_info->op1, reg_info->crm, reg_info->op2, reg_info->op0);
        }
    }
}

void system_instructions_and_registers_reset(CPUState *env)
{
    TTable *cp_regs = env->cp_regs;

    int i;
    for (i = 0; i < cp_regs->count; i++) {
        ARMCPRegInfo *ri = cp_regs->entries[i].value;

        // Nothing to be done for these because:
        // * all the backing fields except the 'arm_core_config' ones are always reset to zero,
        // * CONSTs have no backing fields and 'resetvalue' is always used when they're read.
        if ((ri->resetvalue == 0) || (ri->type & ARM_CP_CONST)) {
            continue;
        }

        uint32_t width = ri->cp == (ri->type & ARM_CP_64BIT) ? 64 : 32;
        uint64_t value = width == 64 ? ri->resetvalue : ri->resetvalue & UINT32_MAX;

        tlib_printf(LOG_LEVEL_NOISY, "Resetting value for '%s': 0x%" PRIx64, ri->name, value);
        if (ri->fieldoffset) {
            memcpy((void *)env + ri->fieldoffset, &value, (size_t)(width / 8));
        } else if (ri->writefn) {
            ri->writefn(env, ri, value);
        } else {
            // Shouldn't happen so let's make sure it doesn't.
            tlib_assert_not_reached();
        }
    }
}

void system_instructions_and_registers_init(CPUState *env)
{
    uint32_t ttable_size = 0;
    env->cp_regs = ttable_create(ttable_size, entry_remove_callback, ttable_compare_key_uint32);
}
