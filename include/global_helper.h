#include "def-helper.h"

DEF_HELPER_1(prepare_block_for_execution, void, ptr)
DEF_HELPER_2(block_begin_event, void, i32, i32)
DEF_HELPER_2(log, void, i32, i32)
DEF_HELPER_0(abort, void)

#include "def-helper.h"
