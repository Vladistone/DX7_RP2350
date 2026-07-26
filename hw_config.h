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
#define TFT_RST    12

// 2. SD Card (SPI1)
#define SD_SPI_PORT spi1
#define SD_MISO_PIN 4
#define SD_CS_PIN   5
#define SD_SCK_PIN  6
#define SD_MOSI_PIN 7

// 3. MPR121 Touchpad (I2C0)
#define I2C_PORT    i2c0
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define MPR121_ADDR 0x5A

// 4. Энкодер KY-040 (GPIO)
#define ENC_PIN_A   10
#define ENC_PIN_B   11
#define ENC_PIN_SW  13

// 5. UART MIDI (UART0)
#define MIDI_UART_ID   uart0
#define MIDI_BAUD_RATE 31250
#define MIDI_TX_PIN    16
#define MIDI_RX_PIN    17

// 6. Системные пины (из main.c)
#define BTN_SYS_MODE   23
#define LED_INIT_PIN   24
#define LED_LOOP_PIN   25

#endif // HW_CONFIG_H