#include "dyngen-exec.h"
#include <global_helper.h>
#include "callbacks.h"
#include "debug.h"

void HELPER(update_insn_count)(int inst_count)
{
  tlib_update_instruction_counter(inst_count);
}

void HELPER(block_begin_event)(uint32_t address, uint32_t size)
{
  tlib_on_block_begin(address, size);
}

void HELPER(abort)(void) {
	tlib_abort("aborted by gen_abort!");
}

void HELPER(log)(uint32_t id, uint32_t pc)
{
  tlib_printf(LOG_LEVEL_INFO, "Log @ pc=0x%08X (block start: 0x%08X) : '%s'", pc, CPU_PC(env), msgs[id] == NULL ? "unknown??" : msgs[id]);
}
