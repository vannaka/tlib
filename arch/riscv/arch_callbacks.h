#ifndef ARCH_CALLBACKS_H_
#define ARCH_CALLBACKS_H_

#include <stdint.h>

void tlib_mip_changed(uint32_t);
uint64_t tlib_get_cpu_time();

#endif
