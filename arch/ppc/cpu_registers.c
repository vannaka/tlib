/*
 *  PPC registers interface.
 *
 *  Copyright (c) Antmicro
 *  Copyright (c) Realtime Embedded
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

#ifdef TARGET_PPC64
uint64_t* get_reg_pointer_64(int reg)
{
    switch(reg)
    {
        case PC_64:
        case NIP_64:
            return &(cpu->nip);
        case SRR0:
            return &(cpu->spr[SPR_SRR0]);            
        case SRR1:
            return &(cpu->spr[SPR_SRR1]);
        case LPCR:
            return &(cpu->spr[SPR_LPCR]);
        case MSR:
            return &(cpu->msr);
        case LR:
            return &(cpu->lr);
        default:
            return NULL;
    }
}
CPU_REGISTER_ACCESSOR(64);
#else
uint32_t* get_reg_pointer_32(int reg)
{
    switch(reg)
    {
        case NIP_32:
            return &(cpu->nip);
        default:
            return NULL;
    }
}
CPU_REGISTER_ACCESSOR(32);
#endif