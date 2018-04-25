#ifndef ARCH_CALLBACKS_H_
#define ARCH_CALLBACKS_H_

#include <stdint.h>

uint64_t tlib_get_cpu_time();
void tlib_privilege_level_changed(uint32_t);

#endif
