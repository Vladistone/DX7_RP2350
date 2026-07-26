#include "midi_bridge_mode.h"
#include "midi_uart.h"
#include <stdio.h>

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
    }
}

void midi_bridge_render(void) {
    // Отрисовка на экране: "MIDI BRIDGE ACTIVE | CC -> SYSEX"
}