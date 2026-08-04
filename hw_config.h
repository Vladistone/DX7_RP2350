#ifndef HW_CONFIG_H
#define HW_CONFIG_H

#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

// 1. TFT ST7789 (SPI0)
#define TFT_SPI_PORT   spi0
#define TFT_DC     0
#define TFT_CS     1
#define TFT_SCLK   2
#define TFT_MOSI   3
#define TFT_RST    15
#define TFT_BLK_PWM 22   // Подсветка через PWM

// 2. Энкодер KY-040 (GPIO)
#define ENC_PIN_A   4
#define ENC_PIN_B   5
#define ENC_PIN_SW  14

// 3. MPR121 Touchpad (I2C0)
#define I2C_PORT    i2c1
#define I2C_SDA_PIN 6
#define I2C_SCL_PIN 7
#define MPR121_ADDR 0x5A

// 4. SD Card (SPI1)
#define SD_SPI_PORT   spi1
#define SD_MISO_PIN       8
#define SD_CS_PIN         9
#define SD_SCK_PIN        10
#define SD_MOSI_PIN       11

// 5. UART MIDI (UART0)
#define MIDI_UART_ID   uart0
#define MIDI_BAUD_RATE 31250
#define MIDI_TX_PIN    12
#define MIDI_RX_PIN    13

// 6. Системные пины (из main.c)
#define BTN_SYS_MODE   23
#define LED_INIT_PIN   24
#define LED_LOOP_PIN   25

// =================================================================
// DEBUG CONFIGURATION
// =================================================================
#define DEBUG_GLOBAL_ENABLE     0   // Главный выключатель отладки
#define DEBUG_SD_REVIEW_ENABLE  0   // Отладка модуля sd_review.c

#if (DEBUG_GLOBAL_ENABLE && DEBUG_SD_REVIEW_ENABLE)
    #include "tusb.h"
    #include "pico/stdlib.h"
    #include <stdio.h>

    #define SD_LOG(fmt, ...) \
        do { \
            if (tud_cdc_connected()) { \
                uint64_t _us = time_us_64(); \
                uint32_t _sec = (uint32_t)(_us / 1000000ULL); \
                uint32_t _rem_us = (uint32_t)(_us % 1000000ULL); \
                printf("[%04u.%06u][SD_REV] " fmt "\n", _sec, _rem_us, ##__VA_ARGS__); \
                stdio_flush(); \
            } \
        } while (0)
#else
    #define SD_LOG(fmt, ...) do {} while(0)

#endif // Конец блока отладки DEBUG

#endif // HW_CONFIG_H
