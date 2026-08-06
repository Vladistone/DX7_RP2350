#ifndef SYSTEM_MODE_H
#define SYSTEM_MODE_H

#include "ui_engine.h"
#include "sd_storage.h"
//#include "TFT_dvr.h"
//#include <stdio.h>
//#include <stdarg.h>
#include <stdint.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

//void draw_mpr121_visual_map(uint16_t touched, int start_x, int start_y);
void system_render_1(uint16_t mpr_touched_state, float v_sys);
void system_print(const char* format, ...);
//void ui_render_mode_layout("SYS Config", sys_page_idx, SYS_TOTAL_PAGES, sys_pages[sys_page_idx]);
#endif // SYSTEM_MOD_H