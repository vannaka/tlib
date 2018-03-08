#ifndef CALLBACKS_H_
#define CALLBACKS_H_

#include <stdint.h>
#include <stdlib.h>
#include "infrastructure.h"

#define DEFAULT_VOID_HANDLER1(NAME, PARAM1) \
  NAME(PARAM1) __attribute__((weak));\
\
  NAME(PARAM1)\
{\
\
}

#define DEFAULT_VOID_HANDLER2(NAME, PARAM1, PARAM2) \
  NAME(PARAM1, PARAM2) __attribute__((weak));\
\
  NAME(PARAM1, PARAM2)\
{\
\
}

#define DEFAULT_INT_HANDLER1(NAME, PARAM1) \
  NAME(PARAM1) __attribute__((weak));\
\
  NAME(PARAM1)\
{\
  return 0;\
}

#define DEFAULT_INT_HANDLER2(NAME, PARAM1, PARAM2) \
  NAME(PARAM1, PARAM2) __attribute__((weak));\
\
  NAME(PARAM1, PARAM2)\
{\
  return 0;\
}

uint32_t tlib_read_byte(uint64_t address);
uint32_t tlib_read_word(uint64_t address);
uint32_t tlib_read_double_word(uint64_t address);
void tlib_write_byte(uint64_t address, uint32_t value);
void tlib_write_word(uint64_t address, uint32_t value);
void tlib_write_double_word(uint64_t address, uint32_t value);
void *tlib_guest_offset_to_host_ptr(uint64_t offset);
int32_t tlib_is_io_accessed(uint64_t address);
uint64_t tlib_host_ptr_to_guest_offset(void *ptr);
void tlib_invalidate_tb_in_other_cpus(unsigned long start, unsigned long end);
void tlib_update_instruction_counter(int32_t value);
int32_t tlib_get_cpu_index(void);

void *tlib_malloc(size_t size);
void *tlib_realloc(void *ptr, size_t size);
void tlib_free(void *ptr);

void tlib_abort(char *message);
void tlib_log(enum log_level level, char* message);

void tlib_on_translation_block_find_slow(uint64_t pc);
void tlib_on_block_begin(uint64_t address, uint32_t size);
uint32_t tlib_is_block_begin_event_enabled(void);
void tlib_on_translation_cache_size_change(uint64_t new_size);
void tlib_on_block_translation(uint64_t start, uint32_t size, uint32_t flags);
extern int32_t tlib_is_on_block_translation_enabled;
void tlib_set_on_block_translation_enabled(int32_t value);

#endif
