#ifndef MIDI_UART_H
#define MIDI_UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h> // Добавлено для типа bool

void midi_init(void);
void midi_send_byte(uint8_t byte);
void midi_send_program_change(uint8_t channel, uint8_t program);
void midi_send_sysex(const uint8_t *data, size_t length);

// Чтение одного байта из UART (возвращает true, если байт прочитан)
bool midi_read_byte(uint8_t *out_byte);

// Вычитка всего входящего буфера и передача в транслятор
void midi_process_incoming(void);

// ОБЯЗАТЕЛЬНО: объявление функции обработки входящего байта
void sysex_cc_map_process_byte(uint8_t byte);

#endif // MIDI_UART_H