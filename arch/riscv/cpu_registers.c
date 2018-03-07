/*
 *  RISCV registers interface
 *
 *  Copyright (c) Antmicro
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
#include <stdint.h>

#include "cpu.h"
#include "cpu_registers.h"

// REMARK: here we use #ifdef/#endif,#ifdef/#endif notation just to be consistent with header file; in header it is required by our parser
#ifdef TARGET_RISCV64
uint64_t* get_reg_pointer_64(int reg)
{
    switch(reg)
    {
        case X_0_64 ... X_31_64:
            return &(cpu->gpr[reg]);
        case PC_64:
            return &(cpu->pc);
        default:
            return NULL;
    }
}

CPU_REGISTER_ACCESSOR(64)
#endif
#ifdef TARGET_RISCV32
uint32_t* get_reg_pointer_32(int reg)
{
    switch(reg)
    {
        case X_0_32 ... X_31_32:
            return &(cpu->gpr[reg]);
        case PC_32:
            return &(cpu->pc);
        default:
            return NULL;
    }
}

CPU_REGISTER_ACCESSOR(32)
#endif