#include "ui_theme.h"

// Массив предустановленных тем для каждого из 4 режимов
static const UI_Theme mode_themes[MODE_COUNT] = {
    [MODE_PLAYBACK] = {
        .bg_color       = COLOR_BG_NORMAL,
        .text_color     = 0xFFFF,
        .accent_color   = 0xFFE0,
        .bar_bg_color   = 0x00FF, // 0x2104
        .bar_text_color = 0xFD20 // 0xFD20 Янтарный для [PLAY]
    },
    [MODE_FILE_SELECT] = {
        .bg_color       = COLOR_BG_NORMAL,
        .text_color     = 0xFFFF,
        .accent_color   = COLOR_MIDI_SEL,
        .bar_bg_color   = COLOR_BG_SELECTED,
        .bar_text_color = COLOR_FOLDER_NORM // Горчичный для [FILE]
    },
    [MODE_USB_MIDI] = {
        .bg_color       = COLOR_BG_NORMAL,
        .text_color     = 0xFFFF,
        .accent_color   = COLOR_MIDI_SEL,
        .bar_bg_color   = 0x0010,
        .bar_text_color = COLOR_MIDI_SEL // Голубой для [MIDI]
    },
    [MODE_SYSTEM_CONFIG] = {
        .bg_color       = COLOR_BG_NORMAL,
        .text_color     = 0xFFFF,
        .accent_color   = 0xF81F,
        .bar_bg_color   = 0x3086,
        .bar_text_color = 0xF81F // Маджента для [SYS]
    }
};

// Тема по умолчанию при старте
UI_Theme current_theme = mode_themes[MODE_FILE_SELECT];

const UI_Theme* ui_theme_get_for_mode(AppModeState mode) {
    if (mode >= MODE_COUNT) {
        return &mode_themes[MODE_FILE_SELECT];
    }
    return &mode_themes[mode];
}

void ui_theme_set_active_mode(AppModeState mode) {
    current_theme = *ui_theme_get_for_mode(mode);
}