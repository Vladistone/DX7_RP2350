#include "mapping.h"

// ------------------------------------------------------------------------------
// Базовый профиль маппинга (Default Profile)
// Таблица связывает MIDI CC с официальными ID параметров Yamaha DX7 (0..154)
// ------------------------------------------------------------------------------
static const dx7_cc_entry_t default_entries[] = {
    // CC  ParamID  Min Max  LCD Name           Описание параметра DX7
    // ---  -------  --- ---  ----------------  ---------------------------------
    
    // --- Глобальные параметры синтеза ---
    {  14,   134,    0,  31,  "Algorithm"    }, // Алгоритм соединений операторов (1..32)
    {  15,   135,    0,   7,  "Feedback"     }, // Глубина обратной связи (0..7)
    {  16,   136,    0,   1,  "Osc Key Sync" }, // Синхронизация фазы осцилляторов (Off/On)
    {  17,   144,    0,  48,  "Transpose"    }, // Транспонирование (0..48, 24 = C3)

    // --- Секция LFO ---
    {  18,   137,    0,  99,  "LFO Speed"    }, // Скорость LFO (0..99)
    {  19,   138,    0,  99,  "LFO Delay"    }, // Задержка включения LFO (0..99)
    {  20,   139,    0,  99,  "LFO Pitch Mod"}, // Глубина модуляции питча (PMD)
    {  21,   140,    0,  99,  "LFO Amp Mod"  }, // Глубина модуляции громкости (AMD)
    {  22,   142,    0,   5,  "LFO Wave"     }, // Форма волны: Tri, SawDn, SawUp, Sq, Sine, S&H
    {  23,   141,    0,   7,  "LFO Pitch Sens"},// Чувствительность модуляции питча (0..7)

    // --- Выходные уровни операторов (Output Levels) ---
    // Регулировка тембра в реальном времени (Carrier = Громкость, Modulator = Тональность/Гармоники)
    {  24,   120,    0,  99,  "OP1 Output"   }, // OP1 Output Level (ID 105 + 15)
    {  25,    99,    0,  99,  "OP2 Output"   }, // OP2 Output Level (ID 84 + 15)
    {  26,    78,    0,  99,  "OP3 Output"   }, // OP3 Output Level (ID 63 + 15)
    {  27,    57,    0,  99,  "OP4 Output"   }, // OP4 Output Level (ID 42 + 15)
    {  28,    36,    0,  99,  "OP5 Output"   }, // OP5 Output Level (ID 21 + 15)
    {  29,    15,    0,  99,  "OP6 Output"   }, // OP6 Output Level (ID 0 + 15)

    // --- Грубая настройка частот операторов (Coarse Frequency) ---
    {  30,   122,    0,  31,  "OP1 Freq Coarse"},// OP1 Coarse Freq (ID 105 + 17)
    {  31,   101,    0,  31,  "OP2 Freq Coarse"},// OP2 Coarse Freq (ID 84 + 17)
    {  33,    80,    0,  31,  "OP3 Freq Coarse"},// OP3 Coarse Freq (ID 63 + 17)
    {  34,    59,    0,  31,  "OP4 Freq Coarse"},// OP4 Coarse Freq (ID 42 + 17)
    {  35,    38,    0,  31,  "OP5 Freq Coarse"},// OP5 Coarse Freq (ID 21 + 17)
    {  36,    17,    0,  31,  "OP6 Freq Coarse"} // OP6 Coarse Freq (ID 0 + 17)
};

// Экспорт структуры профиля для подключения в mapping.h и sysex_cc_map.c
const mapping_profile_t map_default_profile = {
    .name = "Default DX7",
    .entry_count = sizeof(default_entries) / sizeof(default_entries[0]),
    .entries = default_entries
};