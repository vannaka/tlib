#include "arch_exports.h"
#include "cpu.h"

void tlib_set_irq_pending_bit(uint32_t irq, uint32_t value) {
    xtensa_cpu_set_irq_pending_bit(env, irq, value);
}

void tlib_update_execution_mode(uint32_t mode)
{
    // Mode is defined in ExecutionMode.cs in renode-infrastructure:
    //   0: Continuous, 1: SingleStepNonBlocking, 2: SingleStepBlocking
    cpu->singlestep_enabled = mode == 1 || mode == 2;
}
