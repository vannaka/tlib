/*
 *  Host code generation
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "cpu.h"
#include "tcg-op.h"

#include <global_helper.h>
#define GEN_HELPER 1
#include <global_helper.h>

int gen_new_label(void);

TCGv_ptr cpu_env;
static TCGArg *event_size_arg;

static int stopflag_label;

CPUBreakpoint *process_breakpoints(CPUState *env, target_ulong pc) {
    CPUBreakpoint *bp;
    QTAILQ_FOREACH(bp, &env->breakpoints, entry) {
        if (bp->pc == pc) {
            return bp;
        }
    }
    return NULL;
}

static inline void gen_block_header(TranslationBlock *tb)
{
    TCGv_i32 flag;
    stopflag_label = gen_new_label();
    flag = tcg_temp_local_new_i32();
    tcg_gen_ld_i32(flag, cpu_env, offsetof(CPUState, exit_request));
    tcg_gen_brcondi_i32(TCG_COND_NE, flag, 0, stopflag_label);
    tcg_temp_free_i32(flag);

    TCGv_ptr tb_pointer = tcg_const_ptr((tcg_target_long)tb);
    gen_helper_prepare_block_for_execution(tb_pointer);
    tcg_temp_free_ptr(tb_pointer);

    flag = tcg_temp_local_new_i32();
    tcg_gen_ld_i32(flag, cpu_env, offsetof(CPUState, tb_restart_request));
    tcg_gen_brcondi_i32(TCG_COND_NE, flag, 0, stopflag_label);
    tcg_temp_free_i32(flag);

    if(tlib_is_block_begin_event_enabled())
    {
      TCGv event_address = tcg_const_tl(tb->pc);
      event_size_arg = gen_opparam_ptr + 1;
      TCGv_i32 event_size = tcg_const_i32(0xFFFF); // bogus value that is to be fixed at later point

      gen_helper_block_begin_event(event_address, event_size);
      tcg_temp_free(event_address);
      tcg_temp_free_i32(event_size);
    }
}

static inline void gen_block_footer(TranslationBlock *tb)
{
    if (tlib_is_on_block_translation_enabled) {
        tlib_on_block_translation(tb->pc, tb->size, tb->disas_flags);
    }
    if(tlib_is_block_begin_event_enabled())
    {
      *event_size_arg = tb->icount;
    }
    gen_set_label(stopflag_label);
    tcg_gen_exit_tb((uintptr_t)tb + 2);
    *gen_opc_ptr = INDEX_op_end;
}

static int get_max_instruction_count(CPUState *env, TranslationBlock *tb)
{
    int instructions_count = size_of_next_block_to_translate > 0
        ? size_of_next_block_to_translate
        : maximum_block_size;

    return instructions_count > env->instructions_count_threshold
        ? env->instructions_count_threshold
        : instructions_count;
}

/* '*gen_code_size_ptr' contains the size of the generated code (host
   code).
*/
void cpu_gen_code(CPUState *env, TranslationBlock *tb, int *gen_code_size_ptr)
{
    TCGContext *s = tcg->ctx;
    uint8_t *gen_code_buf;
    int gen_code_size;

    tcg_func_start(s);
    memset((void*)tcg->gen_opc_instr_start, 0, OPC_BUF_SIZE);;
    tb->icount = 0;
    tb->size = 0;
    tb->search_pc = 0;

    gen_block_header(tb);
    gen_intermediate_code(env, tb, get_max_instruction_count(env, tb));
    gen_block_footer(tb);

    /* generate machine code */
    gen_code_buf = tb->tc_ptr;
    tb->tb_next_offset[0] = 0xffff;
    tb->tb_next_offset[1] = 0xffff;

    s->tb_next_offset = tb->tb_next_offset;
    s->tb_jmp_offset = tb->tb_jmp_offset;
    s->tb_next = NULL;

    gen_code_size = tcg_gen_code(s, gen_code_buf);
    *gen_code_size_ptr = gen_code_size;
}

/* The cpu state corresponding to 'searched_pc' is restored.
 */
int cpu_restore_state(CPUState *env,
                TranslationBlock *tb, uintptr_t searched_pc)
{
    TCGContext *s = tcg->ctx;
    int j, k;
    uintptr_t tc_ptr;
    int instructions_executed_so_far = 0;

    tcg_func_start(s);
    memset((void*)tcg->gen_opc_instr_start, 0, OPC_BUF_SIZE);
    tb->icount = 0;
    tb->size = 0;
    tb->search_pc = 1;

    gen_block_header(tb);
    gen_intermediate_code(env, tb, get_max_instruction_count(env, tb));
    gen_block_footer(tb);

    /* find opc index corresponding to search_pc */
    tc_ptr = (uintptr_t)tb->tc_ptr;
    if (searched_pc < tc_ptr)
        return -1;

    s->tb_next_offset = tb->tb_next_offset;
    s->tb_jmp_offset = tb->tb_jmp_offset;
    s->tb_next = NULL;
    j = tcg_gen_code_search_pc(s, (uint8_t *)tc_ptr, searched_pc - tc_ptr);
    if (j < 0)
        return -1;
    /* now find start of instruction before */
    while (tcg->gen_opc_instr_start[j] == 0)
        j--;

    k = j;
    while (k > 0)
    {
      instructions_executed_so_far += tcg->gen_opc_instr_start[k];
      k--;
    }

    restore_state_to_opc(env, tb, j);

    return 0;
}

int cpu_restore_state_and_restore_instructions_count(CPUState *env,
                TranslationBlock *tb, uintptr_t searched_pc)
{
    int executed_instructions = cpu_restore_state(env, tb, searched_pc);
    if(executed_instructions != -1 && tb->instructions_count_dirty)
    {
        cpu->instructions_count_value -= (tb->icount - executed_instructions);
        cpu->instructions_count_total_value -= (tb->icount - executed_instructions);
        tb->instructions_count_dirty = 0;
    }
    return executed_instructions;
}
