#ifndef UI_THEME_H
#define UI_THEME_H

#include <stdint.h>
#include <stdbool.h>
#include "modes.h"

// ui_theme.h
void ui_set_brightness(uint8_t percent);

// Структура темы оформления (ваш текущий вариант)
typedef struct {
    uint16_t bg_color;
    uint16_t text_color;
    uint16_t accent_color;
    uint16_t bar_bg_color;
    uint16_t bar_text_color;
} UI_Theme;

// ============================================================================
// ЦВЕТОВАЯ ПАЛИТРА ТЕМЫ TFT LCD (RGB565) — Ваши наработки сохранены
// ============================================================================
#define COLOR_BG_NORMAL        0x0000  // Черный
#define COLOR_BG_SELECTED      0x18E3  // Темно-серый задник под курсором[cite: 11]

// 1. Папки (Желтая палитра)[cite: 10]
#define COLOR_FOLDER_NORM      0xCDE0  // Горчичный[cite: 10]
#define COLOR_FOLDER_SEL       0xFFE0  // Ярко-желтый[cite: 10]

// 2. MIDI & SysEx файлы (Синяя палитра)[cite: 10]
#define COLOR_MIDI_NORM        0x341F  // Синий[cite: 10]
#define COLOR_MIDI_SEL         0x07FF  // Ярко-голубой[cite: 10]

// 3. Прочие файлы и элементы (Серая палитра)[cite: 10]
#define COLOR_OTHER_NORM       0x7BEF  // Серый[cite: 10]
#define COLOR_OTHER_SEL        0xFFFF  // Белый[cite: 10]

// Типы элементов в файловом браузере
typedef enum {
    ITEM_TYPE_FOLDER,
    ITEM_TYPE_MIDI,
    ITEM_TYPE_OTHER
} file_item_type_t;

// Внешняя декларация текущей активной темы
extern UI_Theme current_theme;

// Вспомогательная функция определения цвета элемента в браузере (сохранена полностью)
static inline uint16_t get_item_color(file_item_type_t type, bool is_selected) {
    switch (type) {
        case ITEM_TYPE_FOLDER:
            return is_selected ? COLOR_FOLDER_SEL : COLOR_FOLDER_NORM;
        case ITEM_TYPE_MIDI:
            return is_selected ? COLOR_MIDI_SEL : COLOR_MIDI_NORM;
        default:
            return is_selected ? COLOR_OTHER_SEL : COLOR_OTHER_NORM;
    }
}

// Функции переключения тем по режимам (интегрируются поверх)
const UI_Theme* ui_theme_get_for_mode(AppModeState mode);
void ui_theme_set_mode(AppModeState mode);

#endif // UI_THEME_H