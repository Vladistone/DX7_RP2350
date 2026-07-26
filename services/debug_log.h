#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdint.h>

void debug_log_render_system_screen(uint16_t mpr_touched_state, float v_sys);
void debug_log_print(const char* format, ...);

#endif // DEBUG_LOG_H