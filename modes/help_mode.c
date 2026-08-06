#include "modes.h"
#include "ui_engine.h"
#include "numpad_dvr.h"
#include "TFT_dvr.h"
#include <stdio.h>

static uint8_t help_page_idx = 0;
#define HELP_TOTAL_PAGES 5

// Описание уникального пользовательского контента
static void draw_help_p1_interactive_numpad(void) { /* Живой интерактивный тест тачпада */ }
static void draw_help_p2_encoder_guide(void) { /* Логика манипуляций GP14, GP23 */ }
static void draw_help_p3_midi_channel_config(void) { /* Настройка MIDI-каналов пользователем */ }
static void draw_help_p4_cc_to_sysex_table(void) { /* Таблица маппинга CC# -> SysEx */ }
static void draw_help_p5_formats(void) { /* Справочник .syx/.mid и лимиты FatFS */ }

// Упорядоченный массив страниц USER-интерфейса
static void (*help_pages[HELP_TOTAL_PAGES])(void) = {
    draw_help_p1_interactive_numpad,
    draw_help_p2_encoder_guide,
    draw_help_p3_midi_channel_config,
    draw_help_p4_cc_to_sysex_table,
    draw_help_p5_formats
};

void help_render(void) {
    ui_render_mode_layout("HELP Guide", help_page_idx, HELP_TOTAL_PAGES, help_pages[help_page_idx]);
}

void help_update(uint16_t touched, int enc_delta) {
    if (enc_delta != 0) {
        int next = help_page_idx + enc_delta;
        if (next < 0) next = HELP_TOTAL_PAGES - 1;
        if (next >= HELP_TOTAL_PAGES) next = 0;
        help_page_idx = (uint8_t)next;
    }
}
