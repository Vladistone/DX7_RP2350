#ifndef TFT_DRIVER_H
#define TFT_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hw_config.h"

// Геометрия и конфигурация дисплея (единственное место для настройки)
#define TFT_WIDTH   320
#define TFT_HEIGHT  205
#define OFFSET       35   // Аппаратное Y-смещение
#define SCALE         2   // Множитель размера по умолчанию

// Автоматически вычисляемая максимальная физическая координата Y
#define TFT_MAX_Y   (OFFSET + TFT_HEIGHT - 1)

// Пины RP2040 / RP2350 (SPI0)
//#define TFT_DC        0
//#define TFT_CS        1
//#define TFT_SCLK      2
//#define TFT_MOSI      3
//#define TFT_RST      15

// Прототипы функций и анимации заставки
void tft_init(void);
void st7789_init_registers(void);
void show_animated_splash(void);
void draw_rotating_7(uint16_t center_x, uint16_t center_y, float angle, uint16_t color, uint16_t bg_color, int scale);

// Графические примитивы и заливка
void fill_screen(uint16_t color);
void draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void draw_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
void draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);
void clear_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

// Функции отрисовки текста и иконок
void draw_char_8x12(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color);
void draw_string_8x12(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color);
void draw_char_scaled(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, int scale);
void draw_text_scaled(uint16_t x, uint16_t y, const char* text, uint16_t color, uint16_t bg_color, int scale);
void draw_string_smart(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color, bool wrap_text, uint16_t line_spacing);
void draw_bitmap(uint16_t x, uint16_t y, const uint16_t *bitmap, uint8_t w, uint8_t h, uint16_t color, uint16_t bg_color);

// Массивы шрифта и иконок
extern const uint8_t font_8x12[][12];
extern const uint16_t icon_folder[12];
extern const uint16_t icon_midi[16];
extern const uint16_t icon_play[12];
extern const uint16_t icon_stop[12];
extern const uint16_t icon_rw[12];
extern const uint16_t icon_fw[12];
extern const uint16_t icon_up[12];
extern const uint16_t icon_down[12];

#endif // TFT_DRIVER_H