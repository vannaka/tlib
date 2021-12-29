#include "arch_exports.h"
#include "cpu.h"

void tlib_set_irq_pending_bit(uint32_t irq, uint32_t value) {
    xtensa_cpu_set_irq_pending_bit(env, irq, value);
}
