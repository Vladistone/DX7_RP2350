//cat << 'EOF' > modes/midi_bridge_mode.h
#ifndef MIDI_BRIDGE_MODE_H
#define MIDI_BRIDGE_MODE_H

#include <stdint.h>
#include <stdio.h>

// Инициализация режима моста / ретранслятора
void midi_bridge_init(void);

// Основной цикл обновления (обработка CC -> SysEx и пересылка байт)
void midi_bridge_update(uint16_t touched, int enc_delta);

// Отрисовка статуса моста на экране
void midi_bridge_render(void);

#endif // MIDI_BRIDGE_MODE_H