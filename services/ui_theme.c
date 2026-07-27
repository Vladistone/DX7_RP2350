#include "ui_theme.h"
#include "hw_config.h"    // Для TFT_BL_PIN
#include "hardware/pwm.h" // Для pwm_set_gpio_level

static uint8_t g_brightness = 50; // Процент яркости (0-100)
// функция для установки яркости
void ui_set_brightness(uint8_t percent) {
    g_brightness = (percent > 100) ? 100 : percent;
    // Рассчет значения ШИМ для пина подсветки
    uint16_t pwm_level = (g_brightness * 65535) / 100;
    pwm_set_gpio_level(TFT_BLK_PWM, pwm_level);
}

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