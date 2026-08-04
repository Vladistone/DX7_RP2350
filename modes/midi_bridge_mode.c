#include "modes.h"
#include "midi_bridge_mode.h"
#include "midi_uart.h"
#include "ui_engine.h"
#include "sd_storage.h"
#include <stdio.h>

//статус SD-card для отрисовки в statusbar
bool sd_is_mounted(void) {
    return sd_info.is_mounted;
}

// Храним канал внутри модуля. По умолчанию 1 (или 0, если еще не прочитан из DX7)
static uint8_t midi_ch = 1;
// Эту функцию позже вытащим из UART-обработчика SysEx ответов от DX7:
void midi_bridge_set_channel(uint8_t ch) {
    if (ch >= 1 && ch <= 16) {
        midi_ch = ch;
    }
}

// Пример функции-конвертера: CC #16 (Cutoff/Volume) -> SysEx DX7 (Voice Parameter Change)
static void convert_cc_to_dx7_sysex(uint8_t cc_num, uint8_t cc_val) {
    // Формат SysEx Parameter Change для Yamaha DX7:
    // F0 43 10 00 [Parameter Number] [Data Value] F7
    
    if (cc_num == 16) { 
        // Допустим, CC 16 мапим на громкость Оператора 1 (Параметр №63 в DX7)
        uint8_t sysex_msg[7] = {0xF0, 0x43, 0x10, 0x00, 63, cc_val & 0x7F, 0xF7};
        
        // Отправляем сформированный SysEx в физический MIDI OUT DX7
        midi_send_sysex(sysex_msg, 7);
    }
}

void midi_bridge_init(void) {
    printf("[BRIDGE] USB-MIDI Bridge initialized.\n");
}

void midi_bridge_update(uint16_t touched, int enc_delta) {
    // 1. Здесь принимаем USB MIDI пакеты от ПК или внешнего USB-контроллера
    // uint8_t cc_num, cc_val;
    // if (usb_midi_read_cc(&cc_num, &cc_val)) {
    //     convert_cc_to_dx7_sysex(cc_num, cc_val);
    // }

    // 2. Локальное управление энкодером (если нужно на лету менять MIDI-канал или пресет маппинга)
    if (enc_delta != 0) {
        // Изменение настроек моста
        midi_ch = + midi_ch;
        //ui_draw_statusbar("MIDI BRIDGE", sd_info.is_mounted, midi_ch);
    }
}

void midi_bridge_render(void) {
    char header_string[32];

    // 1. Статус-бар
    ui_draw_statusbar("MIDI BRIDGE", sd_info.is_mounted, midi_ch);
    ui_clear_work_area();

    // 3. Сообщение о статусе подключения к синтезатору
    if (midi_ch == 0) {
        ui_draw_text_centered_rel(40, "Reading DX7 II FD...", current_theme.accent_color, 1);
    } else {
        ui_draw_text_centered_rel(40, "MIDI BRIDGE ACTIVE", current_theme.accent_color, 1);
    }

    // 1. Рисуем заголовок строго по центру рабочей зоны (на 10px ниже хедера)
    //snprintf(header_string, sizeof(2), "USB/MIDI TEST %02d", midi_ch);
    //ui_draw_text_rel(8, "USB/MIDI TEST", current_theme.accent_color, 2);
    draw_text_scaled(10, 30, "USB/MIDI TEST", current_theme.text_color, current_theme.bg_color, 2);

    // 2. Рисуем информационную карточку (по центру, ширина 300px, высота 80px)
    int card_w = 300;
    int card_h = 80;
    int card_x = 10; //(UI_WORK_WIDTH - card_w) / 2;
    ui_draw_card_rel(card_x, 40, card_w, card_h, current_theme.accent_color, current_theme.bar_bg_color);

    // 3. ПРИМЕР Текста внутри карточки
    ui_draw_text_centered_rel(48, "USB > MIDI OUT", current_theme.accent_color, 2);
    ui_draw_text_centered_rel(84, "CC#16 > SysEx Ex63", current_theme.accent_color, 1);
}