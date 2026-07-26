#include "mapping.h"

// ------------------------------------------------------------------------------
// Профиль SSL Nucleus2 для Yamaha DX7
// ------------------------------------------------------------------------------
static const dx7_cc_entry_t nucleus2_entries[] = {
    // --- ПОЛОСЫ 1-6: Управление 6 Операторами (OP1 - OP6) ---
    // Фейдеры 1..6 -> Уровни громкости операторов (Output Level)
    {   1,   120,    0,  99,  "OP1 Level"    }, // Fader 1
    {   2,    99,    0,  99,  "OP2 Level"    }, // Fader 2
    {   3,    78,    0,  99,  "OP3 Level"    }, // Fader 3
    {   4,    57,    0,  99,  "OP4 Level"    }, // Fader 4
    {   5,    36,    0,  99,  "OP5 Level"    }, // Fader 5
    {   6,    15,    0,  99,  "OP6 Level"    }, // Fader 6

    // V-Pots 1..6 -> Грубая частота операторов (Coarse Frequency)
    {  17,   122,    0,  31,  "OP1 Coarse"   }, // V-Pot 1
    {  18,   101,    0,  31,  "OP2 Coarse"   }, // V-Pot 2
    {  19,    80,    0,  31,  "OP3 Coarse"   }, // V-Pot 3
    {  20,    59,    0,  31,  "OP4 Coarse"   }, // V-Pot 4
    {  21,    38,    0,  31,  "OP5 Coarse"   }, // V-Pot 5
    {  22,    17,    0,  31,  "OP6 Coarse"   }, // V-Pot 6

    // --- ПОЛОСЫ 9-14: Точная подстройка операторов ---
    // V-Pots 9..14 -> Точная частота операторов (Fine Frequency)
    {  25,   123,    0,  99,  "OP1 Fine"     }, // V-Pot 9
    {  26,   102,    0,  99,  "OP2 Fine"     }, // V-Pot 10
    {  27,    81,    0,  99,  "OP3 Fine"     }, // V-Pot 11
    {  28,    60,    0,  99,  "OP4 Fine"     }, // V-Pot 12
    {  29,    39,    0,  99,  "OP5 Fine"     }, // V-Pot 13
    {  30,    18,    0,  99,  "OP6 Fine"     }, // V-Pot 14

    // Фейдеры 9..14 -> Расстройка операторов (Detune: 0..14, 7 = Center)
    {   9,   124,    0,  14,  "OP1 Detune"   }, // Fader 9
    {  10,   103,    0,  14,  "OP2 Detune"   }, // Fader 10
    {  11,    82,    0,  14,  "OP3 Detune"   }, // Fader 11
    {  12,    61,    0,  14,  "OP4 Detune"   }, // Fader 12
    {  13,    40,    0,  14,  "OP5 Detune"   }, // Fader 13
    {  14,    19,    0,  14,  "OP6 Detune"   }, // Fader 14

    // --- СЕКЦИЯ 7 & 8: Алгоритм и LFO ---
    {   7,   134,    0,  31,  "Algorithm"    }, // Fader 7 (Алгоритм 1..32)
    {  23,   135,    0,   7,  "Feedback"     }, // V-Pot 7 (Обратная связь)
    {   8,   137,    0,  99,  "LFO Speed"    }, // Fader 8 (Скорость LFO)
    {  24,   139,    0,  99,  "LFO Pitch Mod"}, // V-Pot 8 (Глубина PMD)

    // --- СЕКЦИЯ 15 & 16: Системные и глобальные настройки ---
    {  15,   138,    0,  99,  "LFO Delay"    }, // Fader 15
    {  31,   142,    0,   5,  "LFO Waveform" }, // V-Pot 15
    {  16,   144,    0,  48,  "Transpose"    }, // Fader 16
    {  32,   136,    0,   1,  "Osc Key Sync" }  // V-Pot 16
};

const mapping_profile_t map_nucleus2_profile = {
    .name = "SSL Nucleus2",
    .entry_count = sizeof(nucleus2_entries) / sizeof(nucleus2_entries[0]),
    .entries = nucleus2_entries
};