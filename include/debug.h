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

void mark_as_locked(struct TranslationBlock *tb, char* filename, int line_number);
void check_locked(struct TranslationBlock *tb);

#if DEBUG
#define LOCK_TB(tb) mark_as_locked(tb, __FILE__, __LINE__);
#else
#define LOCK_TB(tb)
#endif

#endif // __DEBUG_H__