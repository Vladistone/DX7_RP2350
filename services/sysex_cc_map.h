#ifndef SYSEX_CC_MAP_H
#define SYSEX_CC_MAP_H

#include <stdint.h>
#include <stdbool.h>
#include "mapping.h"

// Длина одиночной SysEx-команды изменения параметра Yamaha DX7
#define DX7_SYSEX_LEN 7

// Инициализация модуля с выбранным профилем
void sysex_cc_map_init(const mapping_profile_t* profile);

// Смена текущего активного профиля
void sysex_cc_map_set_profile(const mapping_profile_t* profile);

// Конвертация входящего CC (0..127) в SysEx-пакет DX7 с авто-масштабированием значения
bool sysex_cc_map_cc_to_sysex(uint8_t cc_num, uint8_t cc_val, uint8_t midi_ch, 
                              uint8_t* out_sysex, uint16_t* out_len);

// Обратная конвертация SysEx от DX7 в CC (для обновления ручек/экранчиков)
bool sysex_cc_map_sysex_to_cc(const uint8_t* sysex_buf, uint16_t sysex_len, 
                              uint8_t* out_cc_num, uint8_t* out_cc_val);

// Поиск имени и данных параметра по CC (для вывода на TFT)
const dx7_cc_entry_t* sysex_cc_map_find_by_cc(uint8_t cc_num);

// Новая функция записи байтов для отправки в MIDI-translator
void sysex_cc_map_process_byte(uint8_t byte);

#endif // SYSEX_CC_MAP_H