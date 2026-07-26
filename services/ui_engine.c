#include "ui_engine.h"
#include "ui_theme.h"
#include "TFT_dvr.h"
#include <stdio.h>

void ui_engine_init(void) {
    fill_screen(current_theme.bg_color);
}

void ui_set_theme(uint16_t bg, uint16_t text, uint16_t accent, uint16_t bar_bg, uint16_t bar_text) {
    current_theme.bg_color = bg;
    current_theme.text_color = text;
    current_theme.accent_color = accent;
    current_theme.bar_bg_color = bar_bg;
    current_theme.bar_text_color = bar_text;
}

void ui_draw_statusbar(const char* mode_tag, bool sd_status, uint8_t midi_ch) {
    // 1. Отрисовка плашки верха (высота 22px)[cite: 11]
    clear_rect(0, 0, TFT_WIDTH, 22, current_theme.bar_bg_color);

    // 2. Вывод метки режима слева: [PLAY], [SYS], [DAW], [FILE][cite: 11]
    char left_buf[12];
    snprintf(left_buf, sizeof(left_buf), "%s", mode_tag);
    draw_text_scaled(6, 4, left_buf, current_theme.accent_color, current_theme.bar_bg_color, 1);

    // 3. Вывод статуса справа: SD:OK / SD:-- и MIDI Ch[cite: 11]
    char right_buf[16];
    snprintf(right_buf, sizeof(right_buf), "SD:%s CH:%02d", sd_status ? "OK" : "--", midi_ch);
    draw_text_scaled(TFT_WIDTH - 110, 4, right_buf, current_theme.accent_color, current_theme.bar_bg_color, 1);

    // 4. Линия разделения[cite: 11]
    clear_rect(0, 22, TFT_WIDTH, 2, current_theme.accent_color);
}

// функция для корректной отрисовки нижней панели (подвала) без искажений
void ui_draw_footer(const char* footer_text) {
    // Очищаем область подвала внизу экрана (высота 20px)
    clear_rect(0, TFT_HEIGHT - 20, TFT_WIDTH, 20, current_theme.bar_bg_color);
    
    // Выводим текст подсказок через правильный масштабируемый шрифтовой движок
    draw_text_scaled(10, TFT_HEIGHT - 16, footer_text, current_theme.bar_text_color, current_theme.bar_bg_color, 1);
}

void ui_clear_work_area(void) {
    // Очищает область под статус-баром (y >= 24)[cite: 11]
    clear_rect(0, 24, TFT_WIDTH, TFT_HEIGHT - 24, current_theme.bg_color);
}