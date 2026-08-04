#ifndef UI_ENGINE_H
#define UI_ENGINE_H

#include "TFT_dvr.h"
#include "ui_theme.h"
#include <stdint.h>
#include <stdbool.h>

// Размеры служебных панелей
#define UI_HEADER_HEIGHT   24
#define UI_FOOTER_HEIGHT   20

// Размеры и координаты Рабочей Области (Work Area)
#define UI_WORK_X          0
#define UI_WORK_Y          UI_HEADER_HEIGHT
#define UI_WORK_WIDTH      TFT_WIDTH
#define UI_WORK_HEIGHT     (TFT_HEIGHT - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT)

extern UI_Theme current_theme;

// Стандартные функции управления интерфейсом
void ui_engine_init(void);
void ui_set_theme(uint16_t bg, uint16_t text, uint16_t accent, uint16_t bar_bg, uint16_t bar_text);
void ui_draw_statusbar(const char* mode_tag, bool sd_status, uint8_t midi_ch);
void ui_draw_footer(const char* footer_text);
void ui_clear_work_area(void);

// ХЕЛПЕРЫ ПОЗИЦИОНИРОВАНИЯ (Для использования внутри режимов)
// Вывод текста относительно верхней границы рабочей зоны (rel_y = 0..UI_WORK_HEIGHT)
void ui_draw_text_rel(int rel_x, int rel_y, const char* text, uint16_t color, uint8_t scale);

// Горизонтальное центрирование текста внутри рабочей зоны
void ui_draw_text_centered_rel(int rel_y, const char* text, uint16_t color, uint8_t scale);

// Отрисовка рамки/карточки внутри рабочей зоны
void ui_draw_card_rel(int rel_x, int rel_y, int w, int h, uint16_t border_color, uint16_t bg_color);

#endif // UI_ENGINE_H