#include "hw_config.h"
#include "modes.h"      
#include "ui_engine.h"  
#include "debug_log.h"  
#include "TFT_dvr.h"    
#include "sd_storage.h" 
#include "ff.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"   // Подключаем АЦП из SDK для аппаратного замера
#include "numpad_dvr.h"     // Подключаем маппинг кнопок MPR121
#include "pico/binary_info.h" // Для чтения аппаратных хэшей бинарника
#include "hardware/regs/sysinfo.h" // КРИТИЧНО: Прямой доступ к регистрам кремния Raspberry Pi
#include <stdio.h>      
#include <stdarg.h>
#include <string.h>

// ============ ПЕРЕМЕННЫЕ ============
static int selected_item = 1; // курсор R.ENC на текущей странице
static bool mpr_changed[12] = {false};
static uint8_t sys_page_idx = 0;
static bool sys_force_redraw = true;
static bool adc_initialized = false;
static float cached_vin = 5.0f;
static bool vin_measured = false;
#define SYS_TOTAL_PAGES 5
static uint8_t mpr_selected = 0;
static bool mpr_edit_mode = false;
static uint8_t mpr_temp_action = 0;
static uint16_t mpr_last_state = 0xFFFF;
static int blackbox_selected_item = 0;
#define BLACKBOX_ITEMS 4
// Blackbox toggles (стр.3)
static bool bb_usb_trace = true;
static bool bb_sd_log = false;
static bool bb_midi_mon = false;
static bool bb_debug_chrono = true;

static const uint8_t sys_page_item_count[SYS_TOTAL_PAGES] = {
    6,  // P1: hardware lines
    12, // P2: MPR keys
    4,  // P3: blackbox items
    5,  // P4: project dirs
    6   // P5: pinout lines
};

static int sys_wrap_index(int idx, int delta, int count) {
    if (count <= 0) return 0;
    int n = idx + delta;
    while (n < 0) n += count;
    while (n >= count) n -= count;
    return n;
}

static uint16_t sys_sel_color(int line_idx) {
    return (selected_item == line_idx) ? current_theme.accent_color : current_theme.text_color;
}

// ** ПРОТОТИПЫ ФУНКЦИЙ (ДОБАВЛЕНО) **
static void system_mode_measure_voltage(void);
static float read_dx7_vin_voltage(void);
static void draw_sys_p1_hardware_stats(void);
static void draw_sys_p2_mpr121_reassign(void);
static void draw_sys_p3_blackbox_menu(void);
static void draw_sys_p4_project_struct(void);
static void draw_sys_p5_pinout(void);
static void handle_mpr121_edit(int enc_delta, bool sw_pressed, bool sw_held);
static void load_mpr121_mapping(void);
static void save_mpr121_mapping(void);
static void update_mpr121_display(void);
static void update_blackbox_items_display(void);

#define MPR121_MAP_PATH "map/mpr121.map"

// Действия (сокращённые названия для кнопок)
typedef enum {
    MPR_ACTION_CUR_UP, MPR_ACTION_CUR_DN, MPR_ACTION_DISK_UP, MPR_ACTION_DISK_DN,
    MPR_ACTION_ESC, MPR_ACTION_MODE, MPR_ACTION_SELECT, MPR_ACTION_ENTER,
    MPR_ACTION_STOP, MPR_ACTION_PLAY, MPR_ACTION_FF, MPR_ACTION_RW,
    MPR_ACTION_COUNT
} MprAction;

static const char* mpr_short_names[MPR_ACTION_COUNT] = {
    "CUR UP", "CUR DN", "DSK UP", "DSK DN",
    "ESC", "MODE", "SEL", "ENT",
    "STOP", "PLAY", "FF", "RW"
};

static const char* mpr_full_names[MPR_ACTION_COUNT] = {
    "CURSOR UP", "CURSOR DN", "DISK UP", "DISK DN",
    "ESC", "MODE", "SELECT", "ENTER",
    "STOP", "PLAY", "FF", "RW"
};

static uint8_t mpr_mapping[12] = {
    MPR_ACTION_CUR_UP, MPR_ACTION_CUR_DN, MPR_ACTION_DISK_UP, MPR_ACTION_DISK_DN,
    MPR_ACTION_ESC, MPR_ACTION_MODE, MPR_ACTION_SELECT, MPR_ACTION_ENTER,
    MPR_ACTION_STOP, MPR_ACTION_PLAY, MPR_ACTION_FF, MPR_ACTION_RW
};

// перед draw_sys_p2_mpr121_reassign
static void update_mpr121_display(void) {
    const uint16_t COLOR_BTN_BG   = current_theme.bar_bg_color;
    const uint16_t COLOR_ACTIVE   = 0x07FF;
    const uint16_t COLOR_EDIT     = 0x07E0;
    const uint16_t COLOR_CHANGED  = 0xF800;
    const uint16_t COLOR_BTN_TEXT = 0xFFFF;

    int start_x = 10;
    int start_y = 35;
    int box_w = 70;
    int box_h = 24;
    int gap = 5;

    uint16_t touched = mpr121_read_touched();

    for (int i = 0; i < 12; i++) {
        int col = i % 4;
        int row = i / 4;
        int x = start_x + col * (box_w + gap);
        int y = start_y + row * (box_h + gap);

        bool is_pressed = (touched & (1 << i)) != 0;
        bool is_selected = (i == mpr_selected);
        bool is_editing = (is_selected && mpr_edit_mode);
        bool is_changed = mpr_changed[i];

        uint16_t bg_color   = COLOR_BTN_BG;
        uint16_t text_color = COLOR_BTN_TEXT; 

        if (is_selected && !is_pressed) {
            bg_color   = COLOR_BTN_BG;
            text_color = COLOR_ACTIVE;
        }

        if (is_pressed) {
            bg_color   = COLOR_ACTIVE;
            text_color = COLOR_BTN_TEXT;
        }
        
        if (is_editing) {
            bg_color   = COLOR_EDIT;
            text_color = COLOR_BTN_TEXT;
        }

        if (is_changed && !is_selected && !is_pressed && !is_editing) {
            text_color = COLOR_CHANGED;
        }

        clear_rect(x, y, box_w, box_h, bg_color);
        clear_rect(x, y, box_w, 1, COLOR_BTN_TEXT);             
        clear_rect(x, y + box_h - 1, box_w, 1, COLOR_BTN_TEXT); 
        clear_rect(x, y, 1, box_h, COLOR_BTN_TEXT);             
        clear_rect(x + box_w - 1, y, 1, box_h, COLOR_BTN_TEXT); 

        char buf[8];
        uint8_t action = mpr_mapping[i];
        snprintf(buf, sizeof(buf), "%.6s", mpr_short_names[action]);
        draw_text_scaled(x + 6, y + 6, buf, text_color, bg_color, 1);
    }
}

// ⭐ ПЕРЕМЕСТИТЬ СЮДА (перед draw_sys_p3_blackbox_menu)
static void update_blackbox_items_display(void) {
    char buf[32];
    
    // Очищаем ТОЛЬКО область значений (правая часть)
    clear_rect(140, 38, 80, 12, current_theme.bg_color);
    clear_rect(140, 52, 80, 12, current_theme.bg_color);
    clear_rect(140, 66, 80, 12, current_theme.bg_color);
    clear_rect(140, 80, 80, 12, current_theme.bg_color);
    
    // Item 1: USB Trace
    uint16_t color = (blackbox_selected_item == 0) ? 0x07FF : current_theme.text_color;
    snprintf(buf, sizeof(buf), "%s", g_cli_debug_usb_active ? "[ENABLED]" : "[DISABLED]");
    draw_text_scaled(140, 40, buf, color, current_theme.bg_color, 1);
    
    // Item 2: SD Card
    color = (blackbox_selected_item == 1) ? 0x07FF : current_theme.text_color;
    snprintf(buf, sizeof(buf), "%s", g_cli_debug_sd_active ? "[ENABLED]" : "[DISABLED]");
    draw_text_scaled(140, 54, buf, color, current_theme.bg_color, 1);
    
    // Item 3: MIDI Monitor
    color = (blackbox_selected_item == 2) ? 0x07FF : current_theme.text_color;
    draw_text_scaled(140, 68, "[OFF]", color, current_theme.bg_color, 1);
    
    // Item 4: Debug Chrono
    color = (blackbox_selected_item == 3) ? 0x07FF : current_theme.text_color;
    draw_text_scaled(140, 82, "[ON]", color, current_theme.bg_color, 1);
}

// Полный порядок в system_mode.c (структура)
// 1. Заголовки
// 2. Статические переменные (sys_page_idx, sys_force_redraw, ...)
// 3. Определение MprAction и mpr_short_names
// 4. Прототипы функций
// 5. Реализация всех draw_sys_p* функций
// 6. Массив sys_pages
// 7. Реализация system_mode_render()
// 8. Реализация system_mode_update()

// ====================================================================
// ИНИЦИАЛИЗАЦИЯ ADC (вызывается один раз при входе в режим)
// ====================================================================
void system_mode_init(void) {
    sys_page_idx = 0;
    sys_force_redraw = true;
    mpr121_init();
    printf("[INIT] MPR121 Touch... %s\n", (mpr121_read_touched() != 0) ? "OK" : "FAIL");
    load_mpr121_mapping();
    
    if (!adc_initialized) {
        adc_init();
        adc_gpio_init(26);
        adc_set_clkdiv(96);
        sleep_ms(10);
        
        // Прогревочное чтение
        uint32_t irq_status = save_and_disable_interrupts();
        adc_select_input(0);
        uint16_t dummy = adc_read();
        (void)dummy;
        restore_interrupts(irq_status);
        
        adc_initialized = true;
        printf("[ADC] Init OK (clkdiv=96)\n");
    }
    
    // ** ПРИНУДИТЕЛЬНОЕ ИЗМЕРЕНИЕ С ЗАДЕРЖКОЙ **
    sleep_ms(50);  // Даем время на стабилизацию после инициализации
    system_mode_measure_voltage();
    
    // ** ПРИНУДИТЕЛЬНАЯ ПЕРЕРИСОВКА ПОСЛЕ ИЗМЕРЕНИЯ **
    sys_force_redraw = true;
    system_mode_render();
}

// ====================================================================
// ИЗМЕРЕНИЕ НАПРЯЖЕНИЯ (с защитой от блокировки USB)
// ====================================================================
static void system_mode_measure_voltage(void) {
    if (!adc_initialized) {
        printf("[ADC] ERROR: Not initialized!\n");
        return;
    }
    
    printf("[ADC] Reading Vin...\n");
    
    // Отключаем прерывания на время измерения
    uint32_t irq_status = save_and_disable_interrupts();
    
    adc_select_input(0);
    
    // Одно чтение с защитой от зависания
    uint32_t adc_accumulator = 0;
    for (int i = 0; i < 16; i++) {
        adc_accumulator += adc_read();
        // Используем busy_wait вместо sleep (не вызывает прерываний)
        busy_wait_us_32(10);
    }
    
    restore_interrupts(irq_status);
    
    float raw_adc_avg = (float)adc_accumulator / 16.0f;
    if (raw_adc_avg >= 4095.0f) raw_adc_avg = 4095.0f;
    
    const float divider_coefficient = 1.505102f;
    const float v_ref = 3.30f;
    
    cached_vin = (raw_adc_avg * v_ref / 4095.0f) * divider_coefficient;
    vin_measured = true;
    
    printf("[ADC] Vin = %.2fV\n", cached_vin);
}

// ====================================================================
// СТРАНИЦА 1: Живая техническая диагностика
// ====================================================================
static void draw_sys_p1_hardware_stats(void) {
    printf("[SYS] draw_p1\n");
    char buf[64];
    int current_y = 15;
    const int line_step = 13;
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

    // 3. ВОЛЬТМЕТР Vin (используем кэшированное значение)
    if (!vin_measured) {
        snprintf(buf, sizeof(buf), "POWER Vin: -- (measuring...)");
        ui_draw_text_rel(10, current_y, buf, current_theme.text_color, 1);
    } else {
        snprintf(buf, sizeof(buf), "POWER Vin: %.2fV", cached_vin);
        uint16_t vin_color = (cached_vin < 4.5f) ? COLOR_ALERT_RED : current_theme.text_color;
        ui_draw_text_rel(10, current_y, buf, vin_color, 1);
    }
    current_y += line_step;

    // 4. СТАТУС SD-КАРТЫ
    if (sd_info.is_mounted) {
        const char* type_str = (sd_info.card_type & 12) ? "SDHC" : "SDSC";
        if (sd_info.total_capacity_mb > 4096) {
            snprintf(buf, sizeof(buf), "SD: %s | %.1fGB | FREE: %.1fGB",
                type_str, (double)sd_info.total_capacity_mb / 1024.0 / 1000.0,
                (double)sd_info.free_space_mb / 1024.0 / 1000.0);
        } else {
            snprintf(buf, sizeof(buf), "SD: %s | %.1fMB | FREE: %.1fMB",
                type_str, (double)sd_info.total_capacity_mb / 1000.0,
                (double)sd_info.free_space_mb / 1000.0);
        }
        ui_draw_text_rel(10, current_y, buf, current_theme.text_color, 1);
    } else {
        snprintf(buf, sizeof(buf), "SD CARD: - (NOT MOUNTED)");
        ui_draw_text_rel(10, current_y, buf, COLOR_ALERT_RED, 1);
    }
    current_y += line_step;

    // 5. АППАРАТНЫЙ ОПРОС ШИНЫ I2C
    uint8_t test_reg = 0x5D;
    int i2c_status = i2c_write_blocking_until(I2C_PORT, MPR121_ADDR, &test_reg, 1, true, make_timeout_time_us(1000));
    bool mpr_connected = (i2c_status >= 0);

    snprintf(buf, sizeof(buf), "BUS: TFT SPI [OK]");
    ui_draw_text_rel(10, current_y, buf, current_theme.text_color, 1);
    current_y += line_step;

    snprintf(buf, sizeof(buf), "     MPR I2C [DISABLED]");
    //snprintf(buf, sizeof(buf), "     MPR I2C [%s]", mpr_connected ? "OK" : "FAIL");
    uint16_t i2c_color = mpr_connected ? current_theme.text_color : COLOR_ALERT_RED;
    ui_draw_text_rel(10, current_y, buf, i2c_color, 1);
}


    // ====================================================================
    // ЦВЕТОВАЯ ПАЛИТРА (СТРОГО НА ОСНОВЕ ВАШИХ ЖИВЫХ ФОТОГРАФИЙ)
    // ====================================================================
    static void draw_sys_p2_mpr121_reassign(void) {
        const uint16_t COLOR_BTN_BG   = current_theme.bar_bg_color;
        const uint16_t COLOR_ACTIVE   = 0x07FF;
        const uint16_t COLOR_EDIT     = 0x07E0;
        const uint16_t COLOR_CHANGED  = 0xF800;
        const uint16_t COLOR_BTN_TEXT = 0xFFFF;
    
        int start_x = 10;
        int start_y = 10;
        int box_w = 70;
        int box_h = 24;
        int gap = 5;
    
        // Заголовок (статическая часть)
        ui_draw_text_rel(10, 0, "NUMPAD MAPPING:", COLOR_ACTIVE, 2);
        
        // Footer через ui_engine API
        if (mpr_edit_mode) {
            ui_draw_footer("EDIT: ENC=change, SW=save");
        } else {
            ui_draw_footer("ENC: select | SW=edit | Hold SW: exit");
        }
        
        // Отрисовка всех кубиков (обновляется через update_mpr121_display)
        update_mpr121_display();
    }
/*
    // НОВАЯ ФУНКЦИЯ: Обновление ТОЛЬКО кубиков MPR121
    static void update_mpr121_display(void) {
        const uint16_t COLOR_BTN_BG   = current_theme.bar_bg_color;
        const uint16_t COLOR_ACTIVE   = 0x07FF;
        const uint16_t COLOR_EDIT     = 0x07E0;
        const uint16_t COLOR_CHANGED  = 0xF800;
        const uint16_t COLOR_BTN_TEXT = 0xFFFF;
    
        int start_x = 10;
        int start_y = 35;
        int box_w = 70;
        int box_h = 24;
        int gap = 5;
    
        uint16_t touched = mpr121_read_touched();
    
        for (int i = 0; i < 12; i++) {
            int col = i % 4;
            int row = i / 4;
            int x = start_x + col * (box_w + gap);
            int y = start_y + row * (box_h + gap);
    
            bool is_pressed = (touched & (1 << i)) != 0;
            bool is_selected = (i == mpr_selected);
            bool is_editing = (is_selected && mpr_edit_mode);
            bool is_changed = mpr_changed[i];
    
            // Расчёт цветовой схемы
            uint16_t bg_color   = COLOR_BTN_BG;
            uint16_t text_color = COLOR_BTN_TEXT; 
    
            if (is_selected && !is_pressed) {
                bg_color   = COLOR_BTN_BG;
                text_color = COLOR_ACTIVE;
            }
    
            if (is_pressed) {
                bg_color   = COLOR_ACTIVE;
                text_color = COLOR_BTN_TEXT;
            }
            
            if (is_editing) {
                bg_color   = COLOR_EDIT;
                text_color = COLOR_BTN_TEXT;
            }
    
            if (is_changed && !is_selected && !is_pressed && !is_editing) {
                text_color = COLOR_CHANGED;
            }
    
            // Отрисовка подложки кубика
            clear_rect(x, y, box_w, box_h, bg_color);
            
            // Отрисовка рамки
            clear_rect(x, y, box_w, 1, COLOR_BTN_TEXT);             
            clear_rect(x, y + box_h - 1, box_w, 1, COLOR_BTN_TEXT); 
            clear_rect(x, y, 1, box_h, COLOR_BTN_TEXT);             
            clear_rect(x + box_w - 1, y, 1, box_h, COLOR_BTN_TEXT); 
    
            // Форматирование подписи
            char buf[8];
            uint8_t action = mpr_mapping[i];
            snprintf(buf, sizeof(buf), "%.6s", mpr_short_names[action]);
            
            // Вывод текста
            draw_text_scaled(x + 6, y + 6, buf, text_color, bg_color, 1);
        }
    }
*/
    // ============================================================
    // СТРАНИЦА 3: Blackbox меню (специальная логика)
    // ============================================================
    static void draw_sys_p3_blackbox_menu(void) {
        // Статическая отрисовка заголовка и структуры (только при смене страницы)
        char buf[64];
        
        // Заголовок страницы
        ui_draw_text_rel(0, 5, "BLACKBOX LOGGING", current_theme.accent_color, 2);
        
        // Разделительная линия (опционально)
        //clear_rect(10, 28, TFT_WIDTH - 20, 1, current_theme.bar_bg_color);
        
        // Пункты меню (структура)
        ui_draw_text_rel(10, 40, "1. USB Bug Trace:", current_theme.text_color, 1);
        ui_draw_text_rel(10, 54, "2. SD Card Logger:", current_theme.text_color, 1);
        ui_draw_text_rel(10, 68, "3. MIDI Monitor:", current_theme.text_color, 1);
        ui_draw_text_rel(10, 82, "4. Debug Chrono:", current_theme.text_color, 1);
        
        // Footer через ui_engine API
        ui_draw_footer("ENC: scroll | SW: toggle");
        
        // Динамическая часть - состояние items (обновляется отдельно)
        update_blackbox_items_display();
    }

// ====================================================================
// СТРАНИЦА 4: СТРУКТУРА ПРОЕКТА
// ====================================================================
static void draw_sys_p4_project_struct(void) {
    printf("[SYS] draw_p4\n");
    ui_draw_text_rel(10, 15, "PROJECT STRUCTURE:", current_theme.accent_color, 1);
    ui_draw_text_rel(10, 30, "core/    - TFT, Encoder, SD", current_theme.text_color, 1);
    ui_draw_text_rel(10, 43, "modes/   - Play, Help, Sys", current_theme.text_color, 1);
    ui_draw_text_rel(10, 56, "services/- UI, MIDI, SysEx", current_theme.text_color, 1);
    ui_draw_text_rel(10, 69, "lib/     - FatFS, MPR121", current_theme.text_color, 1);
    ui_draw_text_rel(10, 82, "mapping/ - CC->SysEx map", current_theme.text_color, 1);
}

// ====================================================================
// СТРАНИЦА 5: PINOUT (АППАРАТНЫЙ СПРАВОЧНИК)
// ====================================================================
static void draw_sys_p5_pinout(void) {
    printf("[SYS] draw_p5\n");
    ui_draw_text_rel(10, 15, "HARDWARE PINOUT:", current_theme.accent_color, 1);
    ui_draw_text_rel(10, 30, "GP4/5  - Rotary Encoder", current_theme.text_color, 1);
    ui_draw_text_rel(10, 43, "GP14   - ENC Switch", current_theme.text_color, 1);
    ui_draw_text_rel(10, 56, "GP23   - Mode Switch", current_theme.text_color, 1);
    ui_draw_text_rel(10, 69, "GP26   - Vin ADC (DX7)", current_theme.text_color, 1);
    ui_draw_text_rel(10, 82, "I2C    - MPR121 Touch", current_theme.text_color, 1);
    ui_draw_text_rel(10, 95, "SPI    - TFT & SD Card", current_theme.text_color, 1);
}

// ====================================================================
// МАССИВ СТРАНИЦ
// ====================================================================
static void (*sys_pages[SYS_TOTAL_PAGES])(void) = {
    draw_sys_p1_hardware_stats,
    draw_sys_p2_mpr121_reassign,    // <-- Сложная страница отодвинута на позицию 3
    draw_sys_p3_blackbox_menu,      // <-- Временная замена: простая страница
    draw_sys_p4_project_struct,
    draw_sys_p5_pinout
};

// ====================================================================
// ПУБЛИЧНЫЕ ФУНКЦИИ
// ====================================================================
void system_mode_render(void) {
    // Используем единый макет из ui_engine (шапка, футер, очистка)
    ui_render_mode_layout(
        "SYS Config",                  // Заголовок
        sys_page_idx,                  // Текущая страница
        SYS_TOTAL_PAGES,               // Всего страниц
        sys_force_redraw,              // Флаг перерисовки
        sys_pages[sys_page_idx]        // Указатель на функцию отрисовки страницы
    );

    // Сбрасываем флаг после отрисовки (как в help_mode.c)
    sys_force_redraw = false;
}

void system_mode_update(uint16_t touched, int enc_delta, bool sw_held) {
    bool sw_click = (touched != 0 && !sw_held);

    // ============================================================
    // СТРАНИЦА 2: MPR121 (СПЕЦИАЛЬНАЯ ЛОГИКА)
    // ============================================================
    if (sys_page_idx == 1) {
        
        // === В РЕЖИМЕ РЕДАКТИРОВАНИЯ ===
        if (mpr_edit_mode) {
            if (enc_delta != 0) {
                int new_action = mpr_temp_action + enc_delta;
                if (new_action < 0) new_action = MPR_ACTION_COUNT - 1;
                if (new_action >= MPR_ACTION_COUNT) new_action = 0;
                mpr_temp_action = new_action;
                printf("[MPR] K%d -> %s (preview)\n", mpr_selected, mpr_full_names[mpr_temp_action]);
                
                // ⭐ ТОЛЬКО обновление кубиков, без полной перерисовки!
                update_mpr121_display();
                return;
            }
            
            if (sw_click) {
                if (mpr_mapping[mpr_selected] != mpr_temp_action) {
                    mpr_mapping[mpr_selected] = mpr_temp_action;
                    mpr_changed[mpr_selected] = true;
                    printf("[MPR] K%d saved as %s (CHANGED)\n", mpr_selected, mpr_full_names[mpr_temp_action]);
                    save_mpr121_mapping();
                }
                mpr_edit_mode = false;
                sys_force_redraw = true;
                system_mode_render();  // Полная перерисовка для смены footer
                return;
            }
            
            if (sw_held) {
                printf("[MPR] Edit canceled\n");
                mpr_edit_mode = false;
                sys_force_redraw = true;
                system_mode_render();  // Полная перерисовка для смены footer
                return;
            }
            
            return;
        }
        
        // === В ОБЫЧНОМ РЕЖИМЕ ===
        
        if (enc_delta != 0) {
            mpr_selected = (mpr_selected + enc_delta + 12) % 12;
            printf("[MPR] Selected K%d (%s)\n", mpr_selected, mpr_full_names[mpr_mapping[mpr_selected]]);
            
            // ⭐ ТОЛЬКО обновление кубиков!
            update_mpr121_display();
            return;
        }
        
        if (sw_held) {
            mpr_edit_mode = true;
            mpr_temp_action = mpr_mapping[mpr_selected];
            printf("[MPR] Edit ON for K%d (current: %s)\n", mpr_selected, mpr_full_names[mpr_temp_action]);
            sys_force_redraw = true;
            system_mode_render();  // Полная перерисовка для смены footer
            return;
        }
        
        if (sw_click) {
            sys_page_idx = (sys_page_idx + 1) % SYS_TOTAL_PAGES;
            printf("[SYS_PAGE]: %d\n", sys_page_idx + 1);
            sys_force_redraw = true;
            system_mode_render();
            return;
        }
        
        return;
    }

    // ============================================================
    // СТРАНИЦА 3: Blackbox меню (ОПТИМИЗИРОВАННАЯ ЛОГИКА)
    // ============================================================
    if (sys_page_idx == 2) {
        if (enc_delta != 0) {
            blackbox_selected_item = (blackbox_selected_item + enc_delta + BLACKBOX_ITEMS) % BLACKBOX_ITEMS;
            printf("[SYS] Blackbox item %d selected\n", blackbox_selected_item);
            
            // ⭐ ТОЛЬКО обновление items, БЕЗ полной перерисовки!
            update_blackbox_items_display();
            return;
        }
        
        if (sw_click) {
            switch(blackbox_selected_item) {
                case 0: 
                    g_cli_debug_usb_active = !g_cli_debug_usb_active;
                    printf("[SYS] USB Trace: %s\n", g_cli_debug_usb_active ? "ON" : "OFF");
                    break;
                case 1: 
                    g_cli_debug_sd_active = !g_cli_debug_sd_active;
                    printf("[SYS] SD Log: %s\n", g_cli_debug_sd_active ? "ON" : "OFF");
                    break;
                case 2: 
                    printf("[SYS] MIDI Monitor: not implemented\n");
                    break;
                case 3: 
                    printf("[SYS] Debug Chrono: not implemented\n");
                    break;
            }
            
            // ⭐ ТОЛЬКО обновление items!
            update_blackbox_items_display();
            return;
        }
        
        if (sw_held) {
            sys_page_idx = (sys_page_idx + SYS_TOTAL_PAGES - 1) % SYS_TOTAL_PAGES;
            printf("[SYS_PAGE]: %d\n", sys_page_idx + 1);
            sys_force_redraw = true;
            system_mode_render();
            return;
        }
        
        return;
    }

    // ============================================================
    // ВСЕ ОСТАЛЬНЫЕ СТРАНИЦЫ
    // ============================================================
    
    if (enc_delta != 0) {
        return;
    }
    
    if (sw_click) {
        sys_page_idx = (sys_page_idx + 1) % SYS_TOTAL_PAGES;
        printf("[SYS_PAGE]: %d\n", sys_page_idx + 1);
        sys_force_redraw = true;
        system_mode_render();
        return;
    }
    
    if (sw_held) {
        sys_page_idx = (sys_page_idx + SYS_TOTAL_PAGES - 1) % SYS_TOTAL_PAGES;
        printf("[SYS_PAGE]: %d\n", sys_page_idx + 1);
        sys_force_redraw = true;
        system_mode_render();
        return;
    }
}

static void load_mpr121_mapping(void) {
    if (!sd_info.is_mounted) {
        printf("[MPR] No SD — using default mapping\n");
        return;
    }

    FIL f;
    FRESULT res = f_open(&f, MPR121_MAP_PATH, FA_READ);
    if (res != FR_OK) {
        printf("[MPR] %s not found (using defaults)\n", MPR121_MAP_PATH);
        return;
    }

    UINT br = 0;
    uint8_t buf[12];
    res = f_read(&f, buf, sizeof(buf), &br);
    f_close(&f);

    if (res != FR_OK || br != sizeof(buf)) {
        printf("[MPR] Load failed (res=%d, br=%u)\n", res, br);
        return;
    }

    for (int i = 0; i < 12; i++) {
        if (buf[i] >= MPR_ACTION_COUNT) {
            printf("[MPR] Invalid action K%d=%u — keep defaults\n", i, buf[i]);
            return;
        }
    }

    memcpy(mpr_mapping, buf, sizeof(mpr_mapping));
    memset(mpr_changed, 0, sizeof(mpr_changed));
    printf("[MPR] Loaded mapping from %s\n", MPR121_MAP_PATH);
}

static void save_mpr121_mapping(void) {
    if (!sd_info.is_mounted) {
        printf("[MPR] Mapping saved in RAM only (no SD)\n");
        return;
    }

    FRESULT res = f_mkdir("map");
    if (res != FR_OK && res != FR_EXIST) {
        printf("[MPR] mkdir map failed: %d\n", res);
        return;
    }

    FIL f;
    res = f_open(&f, MPR121_MAP_PATH, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        printf("[MPR] Open %s failed: %d\n", MPR121_MAP_PATH, res);
        return;
    }

    UINT bw = 0;
    res = f_write(&f, mpr_mapping, sizeof(mpr_mapping), &bw);
    f_close(&f);

    if (res == FR_OK && bw == sizeof(mpr_mapping)) {
        printf("[MPR] Mapping saved to %s\n", MPR121_MAP_PATH);
    } else {
        printf("[MPR] Write failed (res=%d, bw=%u)\n", res, bw);
    }
}