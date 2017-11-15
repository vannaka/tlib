#if !defined (__DEBUG_H__)
#define __DEBUG_H__

#include <stdint.h>

#define MAX_MSG_COUNT 10000
extern char *msgs[MAX_MSG_COUNT];

#ifdef DEBUG_ON
#define LOG_CURRENT_LOCATION() do{ tlib_printf(LOG_LEVEL_INFO, "We are in %s (%s:%d)", __func__, __FILE__, __LINE__); }while(0)
#else
#define LOG_CURRENT_LOCATION()
#endif

void generate_log(uint32_t pc, char *format, ...);

#endif // __DEBUG_H__