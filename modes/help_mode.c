#include "modes.h"
#include "ui_engine.h"
#include "numpad_dvr.h"
#include "TFT_dvr.h"
#include <stdio.h>

static void draw_mpr121_visual_map_clean(uint16_t touched, int start_x, int start_y, bool force_redraw);
static uint8_t help_page_idx = 0;
#define HELP_TOTAL_PAGES 5
static bool help_force_redraw = true; // Триггер для старта отрисовки

// ====================================================================
// НАПОЛНЕНИЕ СТРАНИЦ РЕЖИМА HELP_MODE (USER ИНТЕРФЕЙС МУЗЫКАНТА)
// ====================================================================

// СТРАНИЦА 1: Живой интерактивный тест тачпада (Карта MPR121)
static void draw_help_p1_interactive_numpad(void) {
    uint16_t touched = mpr121_read_touched(); // Считываем живое состояние регистров I2C
    // Вызываем оригинальный секторный фильтр отрисовки кубиков тачпада.
    // Координаты (10, 20) заданы относительно верхней границы рабочей зоны!
    draw_mpr121_visual_map_clean(touched, 10, 20, false);
}

// СТРАНИЦА 2: Гид по энкодеру и SW (Описание заложенного поведения)
static void draw_help_p2_encoder_guide(void) {
    ui_draw_text_rel(10, 10, "ROTARY ENCODER & SW GUIDE:", current_theme.accent_color, 1);
    
    // Описываем логику манипуляций согласно hw_config.h
    ui_draw_text_rel(10, 30, "ENC Turn : Scroll files / Change Page", current_theme.text_color, 1);
    ui_draw_text_rel(10, 45, "ENC SW   : Select File / Confirm (GP14)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 60, "SYS Mode : Switch active Engine (GP23)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 75, "LED Init : Blinks during system start", current_theme.text_color, 1);
}

// СТРАНИЦА 3: Настройка MIDI-каналов пользователем (Информационный маппинг)
static void draw_help_p3_midi_channel_config(void) {
    ui_draw_text_rel(10, 10, "USER MIDI CH CONFIGURATION:", current_theme.accent_color, 1);
    
    // Описание алгоритма независимой смены каналов
    ui_draw_text_rel(10, 30, "Current TX/RX Channel maps to DX7.", current_theme.text_color, 1);
    ui_draw_text_rel(10, 45, "To reassign: navigate to SYS Config,", current_theme.text_color, 1);
    ui_draw_text_rel(10, 60, "select Page 3, click Encoder SW,", current_theme.text_color, 1);
    ui_draw_text_rel(10, 75, "then rotate to increment CH 1-16.", current_theme.text_color, 1);
}

// СТРАНИЦА 4: Таблица CC# -> SysEx Parameters DX7 (Привязка контроллеров)
static void draw_help_p4_cc_to_sysex_table(void) {
    ui_draw_text_rel(10, 5, "MIDI CC -> DX7 SysEx PARAMETERS:", current_theme.accent_color, 1);
    
    // Выводим структурированную шпаргалку маппинга для музыканта
    ui_draw_text_rel(10, 22, "CC #74 -> OP1-6 Cutoff (SysEx p.12)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 35, "CC #71 -> OP1-6 Resonance (p.13)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 48, "CC #01 -> Modulation Wheel (p.01)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 61, "CC #07 -> Main Voice Volume (p.04)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 74, "CC #91 -> Reverb/Delay Depth (p.20)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 87, "CC #93 -> Chorus Level Map   (p.22)", current_theme.text_color, 1);
}

// СТРАНИЦА 5: Справочник .syx/.mid и лимиты FatFS (Форматы)
static void draw_help_p5_formats(void) {
    ui_draw_text_rel(10, 10, "SD STORAGE FORMAT LIMITS:", current_theme.accent_color, 1);
    
    // Жесткие правила файловой системы, заложенные в ffconf.h и sd_storage.h
    ui_draw_text_rel(10, 30, "System FS    : FAT32 Standard Only", current_theme.text_color, 1);
    ui_draw_text_rel(10, 45, "Preset File  : .SYX (32 single patches)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 60, "Sequence File: .MID (Standard MIDI)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 75, "Max Filename : 32 chars (Inc. ext)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 90, "Page Buffer  : Max 32 items per pack", current_theme.text_color, 1);
}

// Упорядоченный массив страниц USER-интерфейса
static void (*help_pages[HELP_TOTAL_PAGES])(void) = {
    draw_help_p1_interactive_numpad,
    draw_help_p2_encoder_guide,
    draw_help_p3_midi_channel_config,
    draw_help_p4_cc_to_sysex_table,
    draw_help_p5_formats
};

void help_render(void) {
    // Передаем help_force_redraw напрямую в шлюз ui_engine!
    // Движок сам решит, чистить холст или просто крутить живую графику
    ui_render_mode_layout("HELP Guide", help_page_idx, HELP_TOTAL_PAGES, help_force_redraw, help_pages[help_page_idx]);
    
    help_force_redraw = false; // Сбрасываем триггер после отрисовки кадра
}

void help_update(uint16_t touched, int enc_delta) {
    if (enc_delta != 0) {
        int next = help_page_idx + enc_delta;
        if (next < 0) next = HELP_TOTAL_PAGES - 1;
        if (next >= HELP_TOTAL_PAGES) next = 0;
        help_page_idx = (uint8_t)next;

        help_force_redraw = true; // ВЗВОДИМ ТРИГГЕР ПЕРЕРИСОВКИ КАРКАСА!
    }
}

// ====================================================================
// ВНУТРЕННЯЯ РЕАЛИЗАЦИЯ КАРТЫ ТАЧПАДА (СТРОГО В КОНЕЦ ФАЙЛА HELP_MODE.C)
// ====================================================================
static void draw_mpr121_visual_map_clean(uint16_t touched, int start_x, int start_y, bool force_redraw) {
    int box_w = 28;
    int box_h = 14;
    int gap = 4;

    // Статический хранитель состояния кнопок для секторного фильтра изменений
    static uint16_t local_last_mpr_state = 0xFFFF;

    // Рисуем заголовок карты кнопок относительно переданных координат
    if (force_redraw) {
        // Выводим заголовок через хелпер ui_engine (или draw_text_scaled, если start_y абсолютный)
        draw_text_scaled(start_x + 46, start_y, "NUMPAD Signatured MAP:", current_theme.accent_color, current_theme.bg_color, 1);
    }

    for (int i = 0; i < 12; i++) {
        bool is_pressed = (touched & (1 << i)) != 0;
        bool was_pressed = (local_last_mpr_state & (1 << i)) != 0;

        // СЕКТОРНЫЙ ФИЛЬТР: Перерисовываем кубик ТОЛЬКО если состояние кнопки реально изменилось!
        if (force_redraw || (is_pressed != was_pressed)) {
            int col = i % 4;
            int row = i / 4;
            int x = 14 + start_x + col * (box_w + gap);
            int y = 14 + start_y + row * (box_h + gap);
            
            // Если зажата — красим в цвет акцента темы, если отпущена — в серый цвет
            uint16_t color = is_pressed ? current_theme.accent_color : 0x31A6; 

            // Стираем и рисуем один конкретный кубик прецизионно по пикселям
            clear_rect(x, y, box_w, box_h, color);

            char num_str[3];
            snprintf(num_str, sizeof(num_str), "%02d", i);
            
            uint16_t text_x = x + 5; 
            uint16_t text_y = y + 0; 
            uint16_t text_color = is_pressed ? 0x0000 : current_theme.accent_color;

            // Отрисовываем цифру строго по центру секторов
            draw_text_scaled(text_x, text_y, num_str, text_color, color, 1);
        }
    }
    
    // Фиксируем маску тача для следующего кадра (наш сохраненный предпоследний штрих!)
    local_last_mpr_state = touched; 
}
