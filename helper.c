#include "dyngen-exec.h"
#include <global_helper.h>
#include "callbacks.h"
#include "debug.h"

// verify if there are instructions left to execute, update instructions count
// and trim the block and exit to the main loop if necessary
void HELPER(prepare_block_for_execution)(void* tb)
{
  cpu->current_tb = (TranslationBlock*)tb;

  uint32_t instructions_left = cpu->instructions_count_threshold - cpu->instructions_count_value;
  uint32_t current_block_size = cpu->current_tb->icount;

  if(instructions_left == 0)
  {
    // setting `tb_restart_request` to 1 will stop executing this block at the end of the header
    cpu->tb_restart_request = 1;
    return;
  }

  if(current_block_size > instructions_left)
  {
    size_of_next_block_to_translate = instructions_left;

    // invalidate this block and jump back to the main loop
    tb_phys_invalidate(cpu->current_tb, -1);
    cpu->tb_restart_request = 1;
    return;
  }

  // update instructions count and execute the block
  cpu->instructions_count_value += current_block_size;
}

void HELPER(block_begin_event)(target_ulong address, uint32_t size)
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
