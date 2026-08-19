#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "TFT_dvr.h"
#include "hardware/pwm.h" 

// ==============================================================================
// БЛОК 1: НИЗКОУРОВНЕВЫЙ SPI
// ==============================================================================
static void write_cmd(uint8_t cmd) {
    asm volatile("nop");
    gpio_put(TFT_DC, 0); 
    gpio_put(TFT_CS, 0);
    asm volatile("nop");
    spi_write_blocking(spi0, &cmd, 1);
    asm volatile("nop");
    gpio_put(TFT_CS, 1);
}

static void write_data(uint8_t data) {
    asm volatile("nop");
    gpio_put(TFT_DC, 1); 
    gpio_put(TFT_CS, 0);
    asm volatile("nop");
    spi_write_blocking(spi0, &data, 1);
    asm volatile("nop");
    gpio_put(TFT_CS, 1);
}

// ==============================================================================
// БЛОК 2: PWM ПОДСВЕТКИ ДИСПЛЕЯ
// ==============================================================================
void tft_backlight_init(void) {
    gpio_set_function(TFT_BLK_PWM, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(TFT_BLK_PWM);
    
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 65535); // 16-битный PWM
    pwm_init(slice_num, &config, true);
    
    // Устанавливаем уровень ШИМ на 0 (OFF)
    pwm_set_gpio_level(TFT_BLK_PWM, 0); 
}

// Установка яркости подсветки в процентах (0-100%)
void tft_backlight_set_percent(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint16_t level = (uint16_t)((percent / 100.0f) * 65535);
    pwm_set_gpio_level(TFT_BLK_PWM, level);
}

// Плавное включение подсветки за указанное время (в миллисекундах)
void tft_backlight_fade_in(uint16_t duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(TFT_BLK_PWM);
    uint16_t max_level = 65535;
    uint16_t steps = 100; // Количество шагов для плавности (можно менять)
    uint16_t step_time_ms = duration_ms / steps;
    uint16_t step_increment = max_level / steps;

    for (uint16_t level = 0; level <= max_level; level += step_increment) {
        pwm_set_gpio_level(TFT_BLK_PWM, level);
        sleep_ms(step_time_ms);
    }
    
    // Гарантируем, что подсветка включена на 100% после завершения
    pwm_set_gpio_level(TFT_BLK_PWM, max_level);
}

// ==============================================================================
// БЛОК 3: ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ
// ==============================================================================
void st7789_init_registers(void) {
    write_cmd(0x01); // SWRESET
    sleep_ms(55);

    write_cmd(0x11); // SLPOUT
    sleep_ms(55);   

    write_cmd(0x3A); // COLMOD (16-bit RGB565)
    write_data(0x55); 

    write_cmd(0x36); // MADCTL (Ориентация экрана)
    write_data(0x60);

    write_cmd(0x21); // INVON (Инверсия цвета для IPS)

    write_cmd(0x2A); // CASET (X)
    write_data(0x00); write_data(0x00);
    write_data((TFT_WIDTH - 1) >> 8); write_data((TFT_WIDTH - 1) & 0xFF);

    write_cmd(0x2B); // RASET (Y) — Используем динамический макрос TFT_MAX_Y
    write_data(0x00); write_data(0x00);
    write_data(TFT_MAX_Y >> 8); write_data(TFT_MAX_Y & 0xFF);

    write_cmd(0x29); // DISPON
    sleep_ms(55);
}

void tft_init(void) {
    // --- Инициализация пинов и SPI ---
    gpio_init(TFT_RST); gpio_set_dir(TFT_RST, GPIO_OUT);
    gpio_init(TFT_DC);  gpio_set_dir(TFT_DC, GPIO_OUT);
    gpio_init(TFT_CS);  gpio_set_dir(TFT_CS, GPIO_OUT);
    gpio_put(TFT_CS, 1);
    gpio_put(TFT_RST, 1);
    sleep_ms(100);
    gpio_put(TFT_RST, 0);
    sleep_ms(100);
    gpio_put(TFT_RST, 1);
    sleep_ms(100);

    gpio_set_function(TFT_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(TFT_SCLK, GPIO_FUNC_SPI);
    spi_init(spi0, 62500000);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_drive_strength(TFT_MOSI, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(TFT_SCLK, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(TFT_DC,   GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(TFT_CS,   GPIO_DRIVE_STRENGTH_12MA);

    // --- Инициализация PWM (подсветка ВЫКЛЮЧЕНА) ---
    tft_backlight_init();

    // --- Инициализация регистров дисплея (подсветка ВЫКЛЮЧЕНА) ---
    st7789_init_registers();

    // --- Запуск заставки, которая сама управляет подсветкой ---
    show_animated_splash();
}

// ==============================================================================
// БЛОК 4: ЗАСТАВКА С ОЧЕНЬ ПЛАВНЫМ НАРАСТАНИЕМ ПОДСВЕТКИ
// ==============================================================================
// Жирная контурная семерка (без заливки)
void draw_rotating_7(uint16_t center_x, uint16_t center_y, float angle, uint16_t color, uint16_t bg_color, int scale) {
    int idx = '7' - 32;
    if (idx < 0 || idx > 94) return;

    int pixel_size = scale * 2;  
    int gap = scale / 2 + 1;
    int step = pixel_size + gap;

    int clear_size = (8 * step) / 2 + 15;
    draw_rectangle(center_x - clear_size, center_y - clear_size, 
               clear_size * 2, clear_size * 2, bg_color);

    for (int row = 0; row < 12; row++) {
        uint8_t row_data = font_8x12[idx][row];
        for (int col = 0; col < 8; col++) {
            if (row_data & (0x80 >> col)) {
                float dx = (col - 4.0f) * step;
                float dy = (row - 6.0f) * step;

                float rx = dx * cos(angle) - dy * sin(angle);
                float ry = dx * sin(angle) + dy * cos(angle);

                int sx = center_x + (int)rx - pixel_size/2;
                int sy = center_y + (int)ry - pixel_size/2;

                if (sx >= 0 && sx + pixel_size <= TFT_WIDTH && 
                    sy >= OFFSET && sy + pixel_size <= TFT_HEIGHT) {
                    draw_rectangle(sx, sy, pixel_size, pixel_size, color);
                }
            }
        }
    }
}

// Функция для очень плавного логарифмического расчёта яркости
uint8_t calculate_smooth_brightness(float progress, uint8_t max_brightness) {
    if (progress <= 0) return 0;
    if (progress >= 1) return max_brightness;
    
    // Очень плавная логарифмическая кривая
    // Используем малую крутизну для более плавного начала
    float steepness = 2.5f;
    float brightness = max_brightness * (1 - exp(-steepness * progress));
    
    // Дополнительное сглаживание в конце
    if (brightness > max_brightness) brightness = max_brightness;
    return (uint8_t)brightness;
}

void show_animated_splash() {
    fill_screen(0x0000);

    uint16_t center_x = TFT_WIDTH / 2;
    uint16_t center_y = (TFT_HEIGHT - OFFSET) / 2;

    uint16_t color_cyan = 0x07FF;
    uint16_t color_white = 0xFFFF;
    uint16_t color_black = 0x0000;

    // ---- ЭТАП 1: Вращающаяся "7" (ОЧЕНЬ МЕДЛЕННО нарастает до 15%) ----
    int rotate_scale = 5;
    int total_frames = 24;
    
    for (int frame = 0; frame < total_frames; frame++) {
        float angle = frame * (2 * 3.14159f / total_frames);
        draw_rotating_7(center_x, center_y, angle, color_cyan, color_black, rotate_scale);
        
        // Очень медленное нарастание: progress от 0 до 0.4 (первые 40% времени)
        float progress = ((float)frame / total_frames) * 0.4f;
        uint8_t brightness = calculate_smooth_brightness(progress, 50);
        tft_backlight_set_percent(brightness);
        
        sleep_ms(40); // Увеличили задержку для более медленного вращения
    }

    fill_screen(0x0000);

    // ---- ЭТАП 2: Появление "YAMAHA" (нарастает от 15% до 35%) ----
    int yamaha_scale = 3;
    int spacing = 3;
    int char_width = 8 * yamaha_scale + spacing;
    int text_width = 6 * char_width;

    uint16_t yamaha_x = (TFT_WIDTH - text_width) / 2;
    uint16_t yamaha_y = 20;

    for (int step = 0; step < 12; step++) { // Увеличили количество шагов
        // progress от 0.4 до 0.7 (следующие 30% времени)
        float progress = 0.4f + ((float)step / 12) * 0.3f;
        uint8_t brightness = calculate_smooth_brightness(progress, 50);
        tft_backlight_set_percent(brightness);
        
        uint16_t brightness_val = (step + 1) * 2;
        uint16_t color = (brightness_val << 11) | (brightness_val << 6) | brightness_val;
        draw_text_scaled(yamaha_x, yamaha_y, "YAMAHA", color, color_black, yamaha_scale);
        sleep_ms(60); // Увеличили задержку
    }

    draw_text_scaled(yamaha_x, yamaha_y, "YAMAHA", color_white, color_black, yamaha_scale);

    // ---- ЭТАП 3: Появление "DX" (нарастает от 35% до 45%) ----
    sleep_ms(300);

    int dx_scale = 6;
    int dx_spacing = 4;
    int dx_char_width = 8 * dx_scale + dx_spacing;

    uint16_t dx_x = (TFT_WIDTH - (2 * dx_char_width + 1 * dx_char_width)) / 2;
    uint16_t dx_y = yamaha_y + 12 * yamaha_scale + 20;

    for (int step = 0; step < 12; step++) {
        // progress от 0.7 до 0.9
        float progress = 0.7f + ((float)step / 12) * 0.2f;
        uint8_t brightness = calculate_smooth_brightness(progress, 50);
        tft_backlight_set_percent(brightness);
        
        uint16_t brightness_val = (step + 1) * 2;
        uint16_t color = (brightness_val << 11) | (brightness_val << 6) | brightness_val;
        draw_text_scaled(dx_x, dx_y, "DX", color, color_black, dx_scale);
        sleep_ms(60);
    }

    draw_text_scaled(dx_x, dx_y, "DX", color_white, color_black, dx_scale);

    // ---- ЭТАП 4: Появление "7" (достигает 50%) ----
    sleep_ms(400);

    int seven_scale = dx_scale + 3;
    uint16_t seven_x = dx_x + 2 * dx_char_width + 16;
    uint16_t seven_y = dx_y - (16 * (seven_scale - dx_scale) / 2);

    for (int step = 0; step < 10; step++) {
        // progress от 0.9 до 1.0
        float progress = 0.9f + ((float)step / 10) * 0.1f;
        uint8_t brightness = calculate_smooth_brightness(progress, 50);
        tft_backlight_set_percent(brightness);
        
        uint16_t brightness_val = (step + 1) * 2;
        uint16_t color = (brightness_val << 11) | (brightness_val << 6) | brightness_val;
        uint16_t seven_color = ((color & 0xF800) >> 1) | ((color & 0x07E0) >> 1) | ((color & 0x001F) >> 1);
        draw_char_scaled(seven_x, seven_y, '7', seven_color, color_black, seven_scale);
        sleep_ms(60);
    }

    draw_char_scaled(seven_x, seven_y, '7', color_cyan, color_black, seven_scale);

    // ---- ЭТАП 5: Пульсация "7" (стабильно 50%) ----
    for (int pulse = 0; pulse < 3; pulse++) {
        sleep_ms(400);
        uint16_t seven_color = (pulse % 2 == 0) ? color_cyan : 0x07E0;
        draw_char_scaled(seven_x, seven_y, '7', seven_color, color_black, seven_scale);
    }

    sleep_ms(1000);
}

// ==============================================================================
// БЛОК 5: БАЗОВЫЕ ПРИМИТИВЫ С ДИНАМИЧЕСКИМ КЛАМПИНГОМ
// ==============================================================================
void draw_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= TFT_WIDTH || w == 0) return;
    if (x + w > TFT_WIDTH) w = TFT_WIDTH - x;

    uint32_t start_y = y + OFFSET;
    if (start_y > TFT_MAX_Y || h == 0) return;
    
    uint32_t end_y = start_y + h - 1;
    if (end_y > TFT_MAX_Y) {
        end_y = TFT_MAX_Y;
        h = end_y - start_y + 1;
    }

    write_cmd(0x2A);
    write_data(x >> 8); write_data(x & 0xFF);
    write_data((x + w - 1) >> 8); write_data((x + w - 1) & 0xFF);
    
    write_cmd(0x2B);
    write_data(start_y >> 8); write_data(start_y & 0xFF);
    write_data(end_y >> 8); write_data(end_y & 0xFF);
    
    write_cmd(0x2C);
    gpio_put(TFT_DC, 1);
    gpio_put(TFT_CS, 0);
    
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    for (uint32_t i = 0; i < ((uint32_t)w * h); i++) {
        spi_write_blocking(spi0, &hi, 1);
        spi_write_blocking(spi0, &lo, 1);
    }
    gpio_put(TFT_CS, 1);
}

void draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    draw_rectangle(x, y, 1, 1, color);
}

void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    draw_rectangle(x, y, w, h, color);
}

void fill_screen(uint16_t color) {
    uint8_t color_high = color >> 8;
    uint8_t color_low  = color & 0xFF;
    
    write_cmd(0x2A);
    write_data(0x00); write_data(0x00);
    write_data((TFT_WIDTH - 1) >> 8); write_data((TFT_WIDTH - 1) & 0xFF);
    
    write_cmd(0x2B); 
    write_data(OFFSET >> 8); write_data(OFFSET & 0xFF);
    write_data(TFT_MAX_Y >> 8); write_data(TFT_MAX_Y & 0xFF);
    
    write_cmd(0x2C); 
    gpio_put(TFT_DC, 1); 
    gpio_put(TFT_CS, 0);
    
    for (uint32_t i = 0; i < ((uint32_t)TFT_WIDTH * TFT_HEIGHT); i++) {
        spi_write_blocking(spi0, &color_high, 1);
        spi_write_blocking(spi0, &color_low, 1);
    }
    
    gpio_put(TFT_CS, 1);
}

void clear_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    draw_rectangle(x, y, w, h, color);
}

void draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color) {
    uint16_t min_x = x1, max_x = x1;
    if (x2 < min_x) min_x = x2; if (x3 < min_x) min_x = x3;
    if (x2 > max_x) max_x = x2; if (x3 > max_x) max_x = x3;
    
    uint16_t min_y = y1, max_y = y1;
    if (y2 < min_y) min_y = y2; if (y3 < min_y) min_y = y3;
    if (y2 > max_y) max_y = y2; if (y3 > max_y) max_y = y3;
    
    for (uint16_t y = min_y; y <= max_y; y++) {
        int16_t x_left = max_x, x_right = min_x;
        
        if ((y1 <= y && y <= y2) || (y2 <= y && y <= y1)) {
            float t = (float)(y - y1) / (y2 - y1);
            int16_t x = x1 + t * (x2 - x1);
            if (x < x_left) x_left = x;
            if (x > x_right) x_right = x;
        }
        if ((y2 <= y && y <= y3) || (y3 <= y && y <= y2)) {
            float t = (float)(y - y2) / (y3 - y2);
            int16_t x = x2 + t * (x3 - x2);
            if (x < x_left) x_left = x;
            if (x > x_right) x_right = x;
        }
        if ((y3 <= y && y <= y1) || (y1 <= y && y <= y3)) {
            float t = (float)(y - y3) / (y1 - y3);
            int16_t x = x3 + t * (x1 - x3);
            if (x < x_left) x_left = x;
            if (x > x_right) x_right = x;
        }
        
        if (x_left <= x_right) {
            draw_rectangle(x_left, y, x_right - x_left + 1, 1, color);
        }
    }
}    

void draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color) {
    int16_t cx = radius;
    int16_t cy = 0;
    int16_t err = 0;

    while (cx >= cy) {
        for (int16_t i = -cx; i <= cx; i++) {
            draw_rectangle(x0 + i, y0 - cy, 1, 1, color);
            draw_rectangle(x0 + i, y0 + cy, 1, 1, color);
        }
        for (int16_t i = -cy; i <= cy; i++) {
            draw_rectangle(x0 + i, y0 - cx, 1, 1, color);
            draw_rectangle(x0 + i, y0 + cx, 1, 1, color);
        }
        
        if (err <= 0) { cy += 1; err += 2*cy + 1; }
        if (err > 0)  { cx -= 1; err -= 2*cx + 1; }
    }
}

// ==============================================================================
// БЛОК 6: ОТРИСОВКА ТЕКСТА И ИКОНОК
// ==============================================================================
void draw_char_8x12(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color) {
    draw_char_scaled(x, y, ch, color, bg_color, 1);
}

void draw_string_8x12(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color) {
    while (*str) {
        if (*str >= 32 && *str <= 126) {
            draw_char_8x12(x, y, *str, color, bg_color);
            x += 9;
        }
        str++;
    }
}

void draw_char_scaled(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, int scale) {
    int idx = ch - 32;
    if (idx < 0 || idx > 94) return;

    int w = 8 * scale;
    int h = 12 * scale;

    if (x >= TFT_WIDTH || (x + w) <= 0) return;
    uint32_t start_y = y + OFFSET;
    if (start_y > TFT_MAX_Y) return;
    uint32_t end_y = start_y + h - 1;
    if (end_y > TFT_MAX_Y) {
        end_y = TFT_MAX_Y;
        h = end_y - start_y + 1;
    }

    write_cmd(0x2A);
    write_data(x >> 8); write_data(x & 0xFF);
    write_data((x + w - 1) >> 8); write_data((x + w - 1) & 0xFF);

    write_cmd(0x2B);
    write_data(start_y >> 8); write_data(start_y & 0xFF);
    write_data(end_y >> 8); write_data(end_y & 0xFF);

    write_cmd(0x2C);
    gpio_put(TFT_DC, 1);
    gpio_put(TFT_CS, 0);

    for (int row = 0; row < 12; row++) {
        uint8_t row_data = font_8x12[idx][row];
        for (int r_sc = 0; r_sc < scale; r_sc++) {
            for (int col = 0; col < 8; col++) {
                uint16_t pixel = (row_data & (0x80 >> col)) ? color : bg_color;
                uint8_t hi = pixel >> 8;
                uint8_t lo = pixel & 0xFF;
                for (int c_sc = 0; c_sc < scale; c_sc++) {
                    spi_write_blocking(spi0, &hi, 1);
                    spi_write_blocking(spi0, &lo, 1);
                }
            }
        }
    }
    gpio_put(TFT_CS, 1);
}

void draw_text_scaled(uint16_t x, uint16_t y, const char* text, uint16_t color, uint16_t bg_color, int scale) {
    uint16_t cur_x = x;
    int char_width = 8 * scale;
    int spacing = 2 * scale;
    
    while (*text) {
        if (*text >= 32 && *text <= 126) {
            draw_char_scaled(cur_x, y, *text, color, bg_color, scale);
            cur_x += char_width + spacing;
        }
        text++;
    }
}

void draw_string_smart(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color, bool wrap_text, uint16_t line_spacing) {
    uint16_t cur_x = x;
    uint16_t char_width = 8 * SCALE;
    uint16_t max_x_for_text = TFT_WIDTH - char_width;

    while (*str) {
        if (!wrap_text && (cur_x > max_x_for_text)) {
            draw_char_scaled(cur_x, y, '{', color, bg_color, SCALE);
            cur_x += char_width;
            break;
        }

        if (*str >= 32 && *str <= 126) {
            draw_char_scaled(cur_x, y, *str, color, bg_color, SCALE);
            cur_x += char_width;
        }
        str++;
    }

    if (cur_x < TFT_WIDTH) {
        draw_rectangle(cur_x, y, TFT_WIDTH - cur_x, line_spacing, bg_color);
    }
}

void draw_bitmap(uint16_t x, uint16_t y, const uint16_t *bitmap, uint8_t w, uint8_t h, uint16_t color, uint16_t bg_color) {
    if (x >= TFT_WIDTH || (x + w) <= 0) return;
    uint32_t start_y = y + OFFSET;
    if (start_y > TFT_MAX_Y) return;
    uint32_t end_y = start_y + h - 1;
    if (end_y > TFT_MAX_Y) {
        end_y = TFT_MAX_Y;
        h = end_y - start_y + 1;
    }

    write_cmd(0x2A);
    write_data(x >> 8); write_data(x & 0xFF);
    write_data((x + w - 1) >> 8); write_data((x + w - 1) & 0xFF);

    write_cmd(0x2B);
    write_data(start_y >> 8); write_data(start_y & 0xFF);
    write_data(end_y >> 8); write_data(end_y & 0xFF);

    write_cmd(0x2C);
    gpio_put(TFT_DC, 1);
    gpio_put(TFT_CS, 0);

    for (int i = 0; i < h; i++) {
        uint16_t row = bitmap[i];
        for (int j = 0; j < w; j++) {
            uint16_t pixel = (row & (0x8000 >> j)) ? color : bg_color;
            uint8_t hi = pixel >> 8;
            uint8_t lo = pixel & 0xFF;
            spi_write_blocking(spi0, &hi, 1);
            spi_write_blocking(spi0, &lo, 1);
        }
    }
    gpio_put(TFT_CS, 1);
}