#include <stdio.h>
#include "midi_uart.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hw_config.h"
#include "sysex_cc_map.h" // Добавлено: для доступа к sysex_cc_map_process_byte

#define DEBUG_MIDI 1

void midi_init(void) {
    uart_init(MIDI_UART_ID, MIDI_BAUD_RATE);
    gpio_set_function(MIDI_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(MIDI_RX_PIN, GPIO_FUNC_UART);
    
    uart_set_hw_flow(MIDI_UART_ID, false, false);
    uart_set_format(MIDI_UART_ID, 8, 1, UART_PARITY_NONE);
}

void midi_send_byte(uint8_t byte) {
    uart_putc_raw(MIDI_UART_ID, byte);
#if DEBUG_MIDI
    printf("[MIDI OUT] %02X\n", byte);
#endif
}

void midi_send_program_change(uint8_t channel, uint8_t program) {
    midi_send_byte(0xC0 | (channel & 0x0F));
    midi_send_byte(program & 0x7F);
}

void midi_send_sysex(const uint8_t *data, size_t length) {
#if DEBUG_MIDI
    printf("[MIDI OUT SYSEX (%d bytes)]: ", (int)length);
#endif
    for (size_t i = 0; i < length; i++) {
        uart_putc_raw(MIDI_UART_ID, data[i]);
#if DEBUG_MIDI
        printf("%02X ", data[i]);
#endif
    }
#if DEBUG_MIDI
    printf("\n");
#endif
}

// 1. Сначала определяем низкоуровневое чтение байта
bool midi_read_byte(uint8_t *out_byte) {
    if (uart_is_readable(MIDI_UART_ID)) {
        *out_byte = uart_getc(MIDI_UART_ID);
#if DEBUG_MIDI
        printf("[MIDI IN ] %02X\n", *out_byte);
#endif
        return true;
    }
    return false;
}

// 2. Затем определяем высокоуровневую обработку буфера
void midi_process_incoming(void) {
    uint8_t in_byte;
    while (midi_read_byte(&in_byte)) {
        sysex_cc_map_process_byte(in_byte); 
    }
}