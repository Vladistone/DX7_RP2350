#include "debug_log.h"
#include "ui_engine.h"
#include "TFT_dvr.h"
#include "sd_storage.h"
#include <stdio.h>

// Отрисовка графической карты 12 кнопок Touchpad MPR121
static void draw_mpr121_visual_map(uint16_t touched, int start_x, int start_y) {
    draw_text_scaled(start_x, start_y, "TOUCHPAD MAP:", current_theme.text_color, current_theme.bg_color, 1);
    
    int box_w = 16;
    int box_h = 12;
    int gap = 3;

    for (int i = 0; i < 12; i++) {
        int col = i % 4;
        int row = i / 4;
        int x = start_x + col * (box_w + gap);
        int y = start_y + 12 + row * (box_h + gap);

        bool is_pressed = (touched & (1 << i)) != 0;
        uint16_t color = is_pressed ? current_theme.accent_color : 0x31A6; // Активный / Серый

        clear_rect(x, y, box_w, box_h, color);

        char num_str[3];
        snprintf(num_str, sizeof(num_str), "%d", i);
        draw_text_scaled(x + 3, y + 2, num_str, 0x0000, color, 1);
    }
}

void debug_log_render_system_screen(uint16_t mpr_touched_state, float v_sys) {
    ui_draw_statusbar("SYS", sd_info.is_mounted, 1);

    int y = 30;
    char buf[40];

    // 1. Системная информация
    draw_text_scaled(10, y, "RP2350 DIAGNOSTICS", current_theme.accent_color, current_theme.bg_color, 1);
    y += 16;

    snprintf(buf, sizeof(buf), "FW: v1.0.2  POWER: %.2fV", v_sys);
    draw_text_scaled(10, y, buf, current_theme.text_color, current_theme.bg_color, 1);
    y += 14;

    // 2. Статус SD-карты
    if (sd_info.is_mounted) {
        snprintf(buf, sizeof(buf), "SD: OK (Files: %d)", sd_info.file_count);
    } else {
        snprintf(buf, sizeof(buf), "SD: NOT MOUNTED");
    }
    draw_text_scaled(10, y, buf, current_theme.text_color, current_theme.bg_color, 1);
    y += 20;

    // 3. Визуальная карта кнопок Touchpad
    draw_mpr121_visual_map(mpr_touched_state, 10, y);
}

void debug_log_print(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args); // или вывод в ваш буфер / лог на экране
    va_end(args);
}