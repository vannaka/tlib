#ifndef EXPORTS_H_
#define EXPORTS_H_

#include <stdint.h>

int32_t tlib_init(char *cpu_name);
void tlib_dispose(void);
void tlib_reset(void);

int32_t tlib_execute(int32_t max_insns);
void tlib_restart_translation_block(void);
void tlib_set_paused(void);
void tlib_clear_paused(void);
int32_t tlib_is_wfi(void);
int32_t tlib_get_executed_instructions(void);
int32_t tlib_get_total_executed_instructions(void);
void tlib_set_single_step(int32_t enabled);

uint32_t tlib_get_page_size(void);
void tlib_map_range(uint64_t start_addr, uint64_t length);
void tlib_unmap_range(uint64_t start, uint64_t end);
uint32_t tlib_is_range_mapped(uint64_t start, uint64_t end);
void tlib_invalidate_translation_blocks(uintptr_t start, uintptr_t end);
void tlib_set_irq(int32_t interrupt, int32_t state);
int32_t tlib_is_irq_set(void);
void tlib_add_breakpoint(uint64_t address);
void tlib_remove_breakpoint(uint64_t address);
void tlib_set_translation_cache_size(uintptr_t size);

void tlib_invalidate_translation_cache(void);
uint32_t tlib_set_maximum_block_size(uint32_t size);
uint32_t tlib_get_maximum_block_size(void);
int tlib_restore_context(void);
void* tlib_export_state(void);
int32_t tlib_get_state_size(void);

#endif
