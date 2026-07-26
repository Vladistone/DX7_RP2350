#ifndef UI_ENGINE_H
#define UI_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "ui_theme.h"

extern UI_Theme current_theme;

void ui_engine_init(void);
void ui_set_theme(uint16_t bg, uint16_t text, uint16_t accent, uint16_t bar_bg, uint16_t bar_text);

// Универсальный системный статус-бар для всех режимов
void ui_draw_statusbar(const char* mode_tag, bool sd_status, uint8_t midi_ch);

// Очистка рабочей области (ниже статус-бара)
void ui_clear_work_area(void);

#endif // UI_ENGINE_H