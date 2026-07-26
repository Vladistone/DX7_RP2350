#ifndef MAPPING_H
#define MAPPING_H

#include <stdint.h>
#include <stdbool.h>

#define MAP_NAME_LEN 16

// Элемент таблицы: Привязка MIDI CC к конкретному параметру Yamaha DX7
typedef struct {
    uint8_t cc_num;          // Номер MIDI CC (0..127)
    uint8_t dx7_param_id;    // ID параметра DX7 (0..154, например 134 = Algorithm, 135 = Feedback)
    uint8_t min_val;         // Минимальное значение (обычно 0)
    uint8_t max_val;         // Максимальное значение (например 31 для алгоритма, 99 для уровня OP)
    char name[MAP_NAME_LEN]; // Короткое название для TFT LCD ("Algorithm", "OP1 Level")
} dx7_cc_entry_t;

// Профиль внешнего MIDI-контроллера (например, nanoKONTROL2, Arturia)
typedef struct {
    const char* name;              // Название профиля ("Default", "nanoKONTROL2")
    uint16_t entry_count;          // Количество элементов в таблице
    const dx7_cc_entry_t* entries; // Массив таблиц маппинга
} mapping_profile_t;

// Внешние объявления встроенных профилей (реализуются в mapping/map_*.c)
extern const mapping_profile_t map_default_profile;
extern const mapping_profile_t map_nano2_profile;
extern const mapping_profile_t map_arturia_profile;
extern const mapping_profile_t map_nucleus2_profile;

#endif // MAPPING_H