#include "hw_config.h"
#include "modes.h"      
#include "ui_engine.h"  
#include "debug_log.h"  
#include "TFT_dvr.h"    
#include "sd_storage.h" 
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"   // Подключаем АЦП из SDK для аппаратного замера
#include "numpad_dvr.h"     // Подключаем маппинг кнопок MPR121
#include "pico/binary_info.h" // Для чтения аппаратных хэшей бинарника
#include "hardware/regs/sysinfo.h" // КРИТИЧНО: Прямой доступ к регистрам кремния Raspberry Pi
#include <stdio.h>      
#include <stdarg.h>

static uint8_t sys_page_idx = 0;
static bool sys_force_redraw = true;

#define SYS_TOTAL_PAGES 5
static int selected_item = 1;

// ====================================================================
// АППАРАТНЫЙ ЗАМЕР ВНЕШНЕГО Vin ОТ DX7 (GPIO26 -> ADC0)
// ====================================================================
static float read_dx7_vin_voltage(void) {
    static bool adc_hardware_ready = false;
    if (!adc_hardware_ready) {
        adc_init();          
        adc_gpio_init(26);   
        adc_hardware_ready = true;
    }
    
    adc_select_input(0); 
    
    uint32_t adc_accumulator = 0;
    for (int i = 0; i < 16; i++) {
        adc_accumulator += adc_read();
        sleep_us(10); 
    }
    
    float raw_adc_avg = (float)adc_accumulator / 16.0f;
    if (raw_adc_avg >= 4095.0f) raw_adc_avg = 4095.0f;

    const float divider_coefficient = 1.505102f;
    const float v_ref = 3.30f; 
    
    return (raw_adc_avg * v_ref / 4095.0f) * divider_coefficient; 
}

// ====================================================================
// СТРАНИЦА 1: Живая техническая диагностика контроллера (void)
// ====================================================================
static void draw_sys_p1_hardware_stats(void) {
    char buf[64]; 
    int current_y = 15;       
    const int line_step = 13; 

    // Определаем красный цвет для алармов в формате RGB565 (Pure Red)
    const uint16_t COLOR_ALERT_RED = 0xF800;

    // 1. ЧАСТОТА И АРХИТЕКТУРА ЯДРА
    uint32_t cpu_hz = clock_get_hz(clk_sys) / 1000000;
    const char* chip_name = "UNKNOWN PI CHIP";
#if PICO_RP2350
    chip_name = "RP2350 (Cortex-M33)";
#elif PICO_RP2040
    chip_name = "RP2040 (Cortex-M0+)";
#endif
    snprintf(buf, sizeof(buf), "HW: %s @%luMHz", chip_name, cpu_hz);
    ui_draw_text_rel(10, current_y, buf, current_theme.accent_color, 1);
    current_y += line_step; 
        
    // 2. ДАТА И ВРЕМЯ СБОРКИ
    snprintf(buf, sizeof(buf), "BUILD: %s | %s", __DATE__, __TIME__);
    ui_draw_text_rel(10, current_y, buf, current_theme.text_color, 1);
    current_y += line_step;

    // 3. ВОЛЬТМЕТР Vin ПИТАНИЯ ОТ DX7
    float live_vin = read_dx7_vin_voltage();
    snprintf(buf, sizeof(buf), "POWER Vin: %.2fV", live_vin);  
    // Если питание просело ниже 4.5V — тоже подсветим красным, иначе — обычный цвет
    uint16_t vin_color = (live_vin < 4.5f) ? COLOR_ALERT_RED : current_theme.text_color;
    ui_draw_text_rel(10, current_y, buf, vin_color, 1);
    current_y += line_step; 

    // 4. СТАТУС SD-КАРТЫ (КРАСНЫЙ ДЛЯ "NOT MOUNTED")
    if (sd_info.is_mounted) {
        const char* type_str = (sd_info.card_type & 12) ? "SDHC" : "SDSC";
        if (sd_info.total_capacity_mb > 4096) {
            snprintf(buf, sizeof(buf), "SD: %s | %.1fGB | FREE: %.1fGB",
                type_str, (double)sd_info.total_capacity_mb / 1024.0 / 1000.0, (double)sd_info.free_space_mb / 1024.0 / 1000.0);
        } else {
            snprintf(buf, sizeof(buf), "SD: %s | %.1fMB | FREE: %.1fMB",
                type_str, (double)sd_info.total_capacity_mb / 1000.0, (double)sd_info.free_space_mb / 1000.0);
        }
        ui_draw_text_rel(10, current_y, buf, current_theme.text_color, 1);
    } else {
        // ЕСЛИ КАРТЫ НЕТ — ПОДСВЕЧИВАЕМ СТРОКУ КРАСНЫМ!
        snprintf(buf, sizeof(buf), "SD CARD: - (NOT MOUNTED)");
        ui_draw_text_rel(10, current_y, buf, COLOR_ALERT_RED, 1);
    }
    current_y += line_step; 

    // 5. АППАРАТНЫЙ ОПРОС ШИНЫ I2C С ТАЙМАУТОМ
    uint8_t test_reg = 0x5D; 
    int i2c_status = i2c_write_blocking_until(I2C_PORT, MPR121_ADDR, &test_reg, 1, true, make_timeout_time_us(1000));
    bool mpr_connected = (i2c_status >= 0);
    
    // Статус SPI шины экрана (всегда OK, раз мы это видим)
    snprintf(buf, sizeof(buf), "BUS: TFT SPI [OK]");
    ui_draw_text_rel(10, current_y, buf, current_theme.text_color, 1);
    current_y += line_step; 

    // СТАТУС I2C ТАЧПАДА (КРАСНЫЙ ДЛЯ "FAIL")
    snprintf(buf, sizeof(buf), "     MPR I2C [%s]", mpr_connected ? "OK" : "FAIL");
    uint16_t i2c_color = mpr_connected ? current_theme.text_color : COLOR_ALERT_RED;
    ui_draw_text_rel(10, current_y, buf, i2c_color, 1);
}

 /* Кастомизация переменных тачпада */
static void draw_sys_p2_mpr121_reassign(void) {
    // HELP: Описываем логику манипуляций согласно hw_config.h
    ui_draw_text_rel(10, 30, "ENC Turn : Scroll files / Change Page", current_theme.text_color, 1);
    ui_draw_text_rel(10, 45, "ENC SW   : Select File / Confirm (GP14)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 60, "SYS Mode : Switch active Engine (GP23)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 75, "LED Init : Blinks during system start", current_theme.text_color, 1);
}
static void draw_sys_p3_blackbox_menu(void) { /* Сервисное меню логов USB Trace / SD BlackBox */ }
static void draw_sys_p4_project_struct(void) { /* Структура исходного кода прошивки из README.adoc */ }
static void draw_sys_p5_pinout(void) { /* Аппаратный справочник hw_config.h */ }

// Упорядоченный массив страниц SERVICE-интерфейса
static void (*sys_pages[SYS_TOTAL_PAGES])(void) = {
    draw_sys_p1_hardware_stats,
    draw_sys_p2_mpr121_reassign,
    draw_sys_p3_blackbox_menu,
    draw_sys_p4_project_struct,
    draw_sys_p5_pinout
};

void system_mode_render(void) {
    // Передаем системный индекс и триггер прокрутки в конвейер
    ui_render_mode_layout("SYS Config", sys_page_idx, SYS_TOTAL_PAGES, sys_force_redraw, sys_pages[sys_page_idx]);
    
    sys_force_redraw = false; // Сбрасываем триггер
}

void system_mode_update(uint16_t touched, int enc_delta) {
    // Листание страниц инженером по sys_page_idx (от 0 до 4)
    // И логика внутренних изменений переменных кастомизации / переключателей логов...
}
