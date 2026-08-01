#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

//#include "ui_engine.h"
//#include "TFT_dvr.h"
//#include "sd_storage.h"
//#include <stdio.h>
//#include <stdarg.h>
#include <stdint.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

//void draw_mpr121_visual_map(uint16_t touched, int start_x, int start_y);
void debug_log_render_system_screen(uint16_t mpr_touched_state, float v_sys);
void debug_log_print(const char* format, ...);
void debug_log_handle_scroll(int enc_delta);
// Прототип футера из ui_engine.c, чтобы убрать варнинг компилятора
//void ui_draw_footer(const char* text);

#endif // DEBUG_LOG_H