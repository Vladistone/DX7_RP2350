#include "modes.h"
#include "ui_engine.h"
#include "TFT_dvr.h"
#include "midi_uart.h"
#include "sd_storage.h"
#include <stdio.h>

// Статические переменные состояния PLAY режима
static int current_selection = 0;      // Индекс выбранного патча
static bool play_force_redraw = true;  // Флаг принудительной перерисовки
static int total_items = 0;            // Общее количество патчей (загружается из SD)
static uint8_t current_bank = 1;
static uint8_t current_patch = 1;
static int selected_index = 0; // Индекс выбранной строки в видимом списке (0, 1 или 2)

// ====================================================================
// ИНИЦИАЛИЗАЦИЯ РЕЖИМА
// ====================================================================
void play_mode_init(void) {
    current_selection = 0;
    play_force_redraw = true;
    // ... ваша логика загрузки патчей ...
    play_mode_render();
}

// ====================================================================
// ОБНОВЛЕНИЕ (обработка энкодера и нажатий)
// ====================================================================
void play_mode_update(uint16_t touched, int enc_delta) {
    if (enc_delta != 0) {
        int max_items = sd_info.file_count;
        if (max_items == 0) max_items = 1;
        
        int new_sel = current_selection + enc_delta;
        if (new_sel < 0) new_sel = max_items - 1;
        if (new_sel >= max_items) new_sel = 0;
        
        if (new_sel != current_selection) {  // <-- ТОЛЬКО ПРИ РЕАЛЬНОМ ИЗМЕНЕНИИ
            current_selection = new_sel;
            printf("[PLAY] Selection: %d\n", current_selection);
            play_force_redraw = true;
        }
    }
}
/*
void play_mode_render(void) {
    ui_draw_statusbar("PLAY MODE", sd_info.is_mounted, 16);
    ui_clear_work_area();

    char buf[32];
    snprintf(buf, sizeof(buf), "BANK: %02d  PATCH: %03d", current_bank, current_patch);
    draw_text_scaled(10, 30, buf, current_theme.bar_text_color, current_theme.bg_color, 1);

    int scale = 1;
    int char_height = 12 * scale;
    int padding_y = 6;
    int row_height = char_height + (padding_y * 2);

    uint16_t list_x = 10;
    uint16_t list_y = 50;
    uint16_t list_w = TFT_WIDTH - 20;
    int total_rows = 3;
    uint16_t list_total_h = row_height * total_rows;

    // Общий контейнер списка
    clear_rect(list_x, list_y, list_w, list_total_h, current_theme.bar_bg_color);

    const char* patch_names[3] = {
        "E.PIANO 1",
        "STRINGS 2",
        "F.BASS 3"
    };

    // Рисуем строки, динамически меняя цвет подсветки для активной строки
    for (int i = 0; i < total_rows; i++) {
        uint16_t text_y = list_y + padding_y + (i * row_height);
        
        // Если строка совпадает с курсором энкодера — подсвечиваем акцентом
        uint16_t text_color = (i == selected_index) ? current_theme.accent_color : current_theme.text_color;
        
        draw_text_scaled(list_x + 12, text_y, patch_names[i], text_color, current_theme.bar_bg_color, scale);
    }

    // Транспортные иконки...
    uint16_t icon_y = 155;
    uint16_t start_x = 45;
    uint16_t spacing = 55;
    draw_bitmap(start_x + 0 * spacing, icon_y, icon_rw, 16, 12, current_theme.bar_text_color, current_theme.bg_color);
    draw_bitmap(start_x + 1 * spacing, icon_y, icon_play, 16, 12, current_theme.bar_text_color, current_theme.bg_color);
    draw_bitmap(start_x + 2 * spacing, icon_y, icon_stop, 16, 12, current_theme.bar_text_color, current_theme.bg_color);
    draw_bitmap(start_x + 3 * spacing, icon_y, icon_fw, 16, 12, current_theme.bar_text_color, current_theme.bg_color);
    draw_bitmap(start_x + 4 * spacing, icon_y, icon_up, 16, 12, current_theme.bar_text_color, current_theme.bg_color);
}
*/
// ====================================================================
// РЕНДЕРИНГ
// ====================================================================
void play_mode_render(void) {
    // ... ваша логика отрисовки, используя play_force_redraw ...
    // После отрисовки сбрасываем флаг:
    play_force_redraw = false;
}
/*
void play_mode_update(uint16_t touched, int enc_delta) {
    // 1. Чтение MIDI...
    uint8_t rx_byte;
    while (midi_read_byte(&rx_byte)) {}

    // 2. Обработка вращения энкодера
    if (enc_delta != 0) {
        // Перемещаем курсор по списку (от 0 до 2)
        int new_index = selected_index + enc_delta;
        if (new_index >= 0 && new_index < 3) {
            selected_index = new_index;
            // Также можем менять номер патча в зависимости от строки
            current_patch = selected_index + 1;
            midi_send_program_change(0, current_patch - 1);
            play_mode_render(); // Перерисовываем экран с новым положением подсветки
        }
    }
}
*/