#include "sysex_cc_map.h"
#include <string.h>

static const mapping_profile_t* active_profile = NULL;

void sysex_cc_map_init(const mapping_profile_t* profile) {
    if (profile != NULL) {
        active_profile = profile;
    } else {
        active_profile = &map_default_profile;
    }
}

void sysex_cc_map_set_profile(const mapping_profile_t* profile) {
    if (profile != NULL) {
        active_profile = profile;
    }
}

const dx7_cc_entry_t* sysex_cc_map_find_by_cc(uint8_t cc_num) {
    if (!active_profile || !active_profile->entries) return NULL;

    for (uint16_t i = 0; i < active_profile->entry_count; i++) {
        if (active_profile->entries[i].cc_num == cc_num) {
            return &active_profile->entries[i];
        }
    }
    return NULL;
}

bool sysex_cc_map_cc_to_sysex(uint8_t cc_num, uint8_t cc_val, uint8_t midi_ch, 
                              uint8_t* out_sysex, uint16_t* out_len) {
    if (!active_profile || !out_sysex || !out_len) return false;

    const dx7_cc_entry_t* entry = sysex_cc_map_find_by_cc(cc_num);
    if (!entry) return false; // Для этого CC нет привязки к DX7

    // 1. Масштабирование значения CC (0..127) под целевой диапазон DX7 [min_val..max_val]
    uint8_t range = entry->max_val - entry->min_val;
    uint8_t dx7_val = entry->min_val + ((uint16_t)cc_val * range) / 127;

    // 2. Формирование 7-байтового SysEx-пакета Yamaha DX7:
    // [0] 0xF0 - Start of SysEx
    // [1] 0x43 - ID производителя (Yamaha)
    // [2] 0x1n - Sub-status 1 (Voice Parameter) + номер MIDI-канала n (0..F)
    // [3] 0x00 - Group Number (0 = Voice)
    // [4] Param ID - Идентификатор параметра DX7 (0..154)
    // [5] Value - Новое значение (0..127)
    // [6] 0xF7 - End of SysEx
    out_sysex[0] = 0xF0;
    out_sysex[1] = 0x43;
    out_sysex[2] = 0x10 | (midi_ch & 0x0F);
    out_sysex[3] = 0x00;
    out_sysex[4] = entry->dx7_param_id;
    out_sysex[5] = dx7_val & 0x7F;
    out_sysex[6] = 0xF7;

    *out_len = DX7_SYSEX_LEN;
    return true;
}

bool sysex_cc_map_sysex_to_cc(const uint8_t* sysex_buf, uint16_t sysex_len, 
                              uint8_t* out_cc_num, uint8_t* out_cc_val) {
    if (!active_profile || !sysex_buf || sysex_len < DX7_SYSEX_LEN) return false;

    // Проверка сигнатуры SysEx DX7 Single Parameter Change
    if (sysex_buf[0] != 0xF0 || sysex_buf[1] != 0x43 || 
       (sysex_buf[2] & 0xF0) != 0x10 || sysex_buf[3] != 0x00 || 
        sysex_buf[6] != 0xF7) {
        return false;
    }

    uint8_t param_id = sysex_buf[4];
    uint8_t dx7_val  = sysex_buf[5];

    // Обратный поиск: находим, какой CC управляет полученным param_id
    for (uint16_t i = 0; i < active_profile->entry_count; i++) {
        const dx7_cc_entry_t* entry = &active_profile->entries[i];
        if (entry->dx7_param_id == param_id) {
            *out_cc_num = entry->cc_num;

            // Обратное масштабирование из диапазона [min..max] в CC [0..127]
            uint8_t range = entry->max_val - entry->min_val;
            if (range > 0) {
                *out_cc_val = ((uint16_t)(dx7_val - entry->min_val) * 127) / range;
            } else {
                *out_cc_val = 0;
            }
            return true;
        }
    }

    return false;
}

// Новая функция записи байтов для отправки в MIDI-translator
void sysex_cc_map_process_byte(uint8_t byte) {
    // Здесь реализуется логика побайтового парсинга или буферизации входящего потока,
    // если это требуется для MIDI-bridge / translator.
    // Пример простейшей заглушки или пересылки:
    (void)byte;
}