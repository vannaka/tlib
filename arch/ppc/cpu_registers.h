#include "cpu-defs.h"

#ifdef TARGET_PPC64
typedef enum {
    NIP_64 = 0,
    PC_64 = 1,
    MSR = 2,
    LR = 3,
    SRR0 = 100,
    SRR1 = 101,
    LPCR = 200,
} Registers;
#else
typedef enum {
    NIP_32 = 0
} Registers;
#endif