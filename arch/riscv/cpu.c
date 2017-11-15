/*
 *  RISC-V CPU
 *
 *  Author: Sagar Karandikar, sagark@eecs.berkeley.edu
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "cpu.h"

void cpu_reset(CPUState *env)
{
    tlb_flush(env, 1);
    cpu_state_reset(env);
    env->priv = PRV_M;
    env->mtvec = DEFAULT_MTVEC;
    env->pc = DEFAULT_RSTVEC;
    env->exception_index = EXCP_NONE;
}


