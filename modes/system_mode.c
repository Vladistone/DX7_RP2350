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
static bool adc_initialized = false;
static float cached_vin = 5.0f;
static bool vin_measured = false;
#define SYS_TOTAL_PAGES 5
static int selected_item = 1;

// ** ПРОТОТИПЫ ФУНКЦИЙ (ДОБАВЛЕНО) **
static void system_mode_measure_voltage(void);
static float read_dx7_vin_voltage(void);
static void draw_sys_p1_hardware_stats(void);
static void draw_sys_p2_mpr121_reassign(void);
static void draw_sys_p3_blackbox_menu(void);
static void draw_sys_p4_project_struct(void);
static void draw_sys_p5_pinout(void);
static void handle_mpr121_edit(int enc_delta, bool sw_pressed, bool sw_held);

//static uint8_t selected_key = 0;      // 0..11
//static bool edit_mode = false;        // Режим редактирования
//static uint8_t temp_action = 0;       // Временное действие

// ====================================================================
// ИНИЦИАЛИЗАЦИЯ ADC (вызывается один раз при входе в режим)
// ====================================================================
void system_mode_init(void) {
    sys_page_idx = 0;
    sys_force_redraw = true;
    mpr121_init();
    printf("[INIT] MPR121 Touch... %s\n", (mpr121_read_touched() != 0) ? "OK" : "FAIL");
    
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
// СТРАНИЦА 2: MPR121 МАППИНГ (ВИЗУАЛЬНАЯ ВЕРСИЯ)
// ====================================================================

// Состояние
static uint8_t mpr_selected = 0;
static bool mpr_edit_mode = false;
static uint8_t mpr_temp_action = 0;
static uint16_t mpr_last_state = 0xFFFF; // Для отслеживания нажатий

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

// ============================================================
// ОТРИСОВКА (ПО АНАЛОГИИ С DEBUG_LOG_OLD)
// ============================================================
static void draw_sys_p2_mpr121(void) {
    char buf[32];
    int start_y = 30;
    int box_w = 70;
    int box_h = 20;
    int gap_x = 8;
    int gap_y = 6;
    const uint16_t COLOR_EDIT = 0x07E0;
    const uint16_t COLOR_PRESSED = 0xF800; // Красный для нажатой кнопки

    // Заголовок
    draw_text_scaled(10, 15, "MPR121 MAPPING", current_theme.accent_color, current_theme.bg_color, 1);

    // Отрисовка 12 кнопок (3 колонки x 4 ряда)
    for (int i = 0; i < 12; i++) {
        int col = i % 3;
        int row = i / 3;
        int x = 10 + col * (box_w + gap_x);
        int y = start_y + row * (box_h + gap_y);

        // Проверяем, нажата ли кнопка физически
        uint16_t touched = mpr121_read_touched();
        bool is_pressed = (touched & (1 << i)) != 0;

        // Цвет фона
        uint16_t bg = current_theme.bg_color;
        if (is_pressed) {
            bg = COLOR_PRESSED; // Красный для нажатой
        } else if (i == mpr_selected) {
            bg = mpr_edit_mode ? COLOR_EDIT : current_theme.accent_color;
        }

        // Рисуем кнопку
        draw_rectangle(x, y, box_w, box_h, bg);
        draw_rectangle(x, y, box_w, box_h, current_theme.text_color); // Рамка

        // Текст (сокращённое название)
        uint8_t action = mpr_mapping[i];
        snprintf(buf, sizeof(buf), "%d:%.5s", i, mpr_short_names[action]);
        uint16_t text_color = (i == mpr_selected || is_pressed) ? current_theme.bg_color : current_theme.text_color;
        draw_text_scaled(x + 4, y + 3, buf, text_color, bg, 1);
    }

    // ============================================================
    // ФУТЕР С ПОДСКАЗКАМИ (ВСЕГДА ВНИЗУ)
    // ============================================================
    int footer_y = TFT_HEIGHT - 30;
    if (mpr_edit_mode) {
        // Режим редактирования
        uint8_t action = mpr_temp_action;
        snprintf(buf, sizeof(buf), "EDIT: K%d -> %s", mpr_selected, mpr_full_names[action]);
        draw_text_scaled(10, footer_y, buf, COLOR_EDIT, current_theme.bg_color, 1);
        draw_text_scaled(10, footer_y + 12, "SW: save | Rotate: change action", current_theme.text_color, current_theme.bg_color, 1);
    } else {
        // Обычный режим
        uint8_t action = mpr_mapping[mpr_selected];
        snprintf(buf, sizeof(buf), "K%d: %s", mpr_selected, mpr_full_names[action]);
        draw_text_scaled(10, footer_y, buf, current_theme.accent_color, current_theme.bg_color, 1);
        draw_text_scaled(10, footer_y + 12, "Hold SW(2s)=edit | Rotate=select key", current_theme.text_color, current_theme.bg_color, 1);
    }
}

// ============================================================
// ЛОГИКА (С ВИЗУАЛЬНОЙ ОБРАТНОЙ СВЯЗЬЮ)
// ============================================================
static void handle_sys_p2_mpr121(int enc_delta, bool sw_click, bool sw_hold) {
    // 1. Вход/выход из редактирования по долгому нажатию
    if (sw_hold) {
        mpr_edit_mode = !mpr_edit_mode;
        if (mpr_edit_mode) {
            mpr_temp_action = mpr_mapping[mpr_selected];
            printf("[MPR] Edit ON for K%d (current: %s)\n", mpr_selected, mpr_full_names[mpr_temp_action]);
        } else {
            printf("[MPR] Edit OFF (saved)\n");
            // Здесь можно сохранить маппинг в EEPROM/SD
        }
        sys_force_redraw = true;
        return;
    }

    // 2. Режим редактирования
    if (mpr_edit_mode) {
        // Вращение меняет действие
        if (enc_delta != 0) {
            int new_action = mpr_temp_action + enc_delta;
            if (new_action < 0) new_action = MPR_ACTION_COUNT - 1;
            if (new_action >= MPR_ACTION_COUNT) new_action = 0;
            mpr_temp_action = new_action;
            printf("[MPR] K%d -> %s (preview)\n", mpr_selected, mpr_full_names[mpr_temp_action]);
            sys_force_redraw = true;
        }
        // Короткое нажатие = сохранить и выйти
        if (sw_click) {
            mpr_mapping[mpr_selected] = mpr_temp_action;
            mpr_edit_mode = false;
            printf("[MPR] K%d saved as %s\n", mpr_selected, mpr_full_names[mpr_temp_action]);
            sys_force_redraw = true;
        }
        return; // В режиме редактирования НЕ переключаем страницы
    }

    // 3. Обычный режим: выбор кнопки
    if (enc_delta != 0) {
        int new_key = mpr_selected + enc_delta;
        if (new_key < 0) new_key = 11;
        if (new_key > 11) new_key = 0;
        mpr_selected = new_key;
        printf("[MPR] Selected K%d (%s)\n", mpr_selected, mpr_full_names[mpr_mapping[mpr_selected]]);
        sys_force_redraw = true;
    }
}

// ====================================================================
// СТРАНИЦА 3: СЕРВИСНОЕ МЕНЮ ЛОГОВ
// ====================================================================
static void draw_sys_p3_blackbox_menu(void) {
    printf("[SYS] draw_p3\n");
    // Очищаем только рабочую область (без заголовка)
    clear_rect(0, 15, TFT_WIDTH, TFT_HEIGHT - 30, current_theme.bg_color);
    
    ui_draw_text_rel(10, 15, "BLACKBOX / LOGGING:", current_theme.accent_color, 1);
    ui_draw_text_rel(10, 30, "1. USB Trace - ENABLED", current_theme.text_color, 1);
    ui_draw_text_rel(10, 43, "2. SD Card Log - DISABLED", current_theme.text_color, 1);
    ui_draw_text_rel(10, 56, "3. MIDI Monitor - OFF", current_theme.text_color, 1);
    ui_draw_text_rel(10, 69, "4. Debug Chrono - ON", current_theme.text_color, 1);
    ui_draw_text_rel(10, 82, "Press ENC SW to toggle items", current_theme.text_color, 1);
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
//static void (*sys_pages[SYS_TOTAL_PAGES])(void) = {
//    draw_sys_p1_hardware_stats,
//    draw_sys_p2_mpr121_reassign,    // <-- Сложная страница отодвинута на позицию 3
//    draw_sys_p3_blackbox_menu,      // <-- Временная замена: простая страница
//    draw_sys_p4_project_struct,
//    draw_sys_p5_pinout
//};

// ====================================================================
// ПУБЛИЧНЫЕ ФУНКЦИИ
// ====================================================================
void system_mode_render(void) {
    if (sys_force_redraw) {
        fill_screen(current_theme.bg_color);
        sys_force_redraw = false;
    }

    char header[32];
    snprintf(header, sizeof(header), "SYS Config P%d/%d", sys_page_idx + 1, SYS_TOTAL_PAGES);
    draw_text_scaled(10, 2, header, current_theme.accent_color, current_theme.bg_color, 1);
    draw_rectangle(0, 14, TFT_WIDTH, 1, current_theme.text_color);

    switch (sys_page_idx) {
        case 0: draw_sys_p1_hardware_stats(); break;
        case 1: draw_sys_p2_mpr121(); break;           // НОВОЕ ИМЯ
        case 2: draw_sys_p3_blackbox_menu(); break;
        case 3: draw_sys_p4_project_struct(); break;
        case 4: draw_sys_p5_pinout(); break;
        default: break;
    }
}

void system_mode_update(uint16_t touched, int enc_delta, bool sw_held) {
    bool sw_click = (touched != 0 && !sw_held);
    static bool hold_processed = false;  // Флаг однократной обработки

    // СТРАНИЦА 2: MPR121
    if (sys_page_idx == 1) {
        // Режим редактирования
        if (mpr_edit_mode) {
            if (enc_delta != 0) {
                int new_action = mpr_temp_action + enc_delta;
                if (new_action < 0) new_action = MPR_ACTION_COUNT - 1;
                if (new_action >= MPR_ACTION_COUNT) new_action = 0;
                mpr_temp_action = new_action;
                printf("[MPR] Action preview: %s\n", mpr_full_names[mpr_temp_action]);
                sys_force_redraw = true;
                system_mode_render();
            }
            if (sw_click) {
                mpr_mapping[mpr_selected] = mpr_temp_action;
                mpr_edit_mode = false;
                printf("[MPR] Saved K%d -> %s\n", mpr_selected, mpr_full_names[mpr_temp_action]);
                sys_force_redraw = true;
                system_mode_render();
            }
            return; // Блокируем переключение страниц
        }

        // Вход в редактирование по долгому нажатию (ОДНОКРАТНО)
        if (sw_held && !hold_processed) {
            hold_processed = true;
            mpr_edit_mode = true;
            mpr_temp_action = mpr_mapping[mpr_selected];
            printf("[MPR] Edit ON for K%d\n", mpr_selected);
            sys_force_redraw = true;
            system_mode_render();
            return;
        }

        // Сброс флага, если кнопка отпущена
        if (!sw_held && !sw_click) {
            hold_processed = false;
        }
        // выбор кнопки циклическим переходом by ENC SW (Вместо касания MPR121):
        if (sw_click && !mpr_edit_mode && sys_page_idx == 1) {
            mpr_selected = (mpr_selected + 1) % 12;
            printf("[MPR] Selected K%d (click)\n", mpr_selected);
            sys_force_redraw = true;
            system_mode_render();
            return;
        }
        // Выбор кнопки по касанию MPR121 (когда заработает)
        if (touched != 0 && !mpr_edit_mode) {
            for (int i = 0; i < 12; i++) {
                if (touched & (1 << i)) {
                    mpr_selected = i;
                    printf("[MPR] Selected K%d (touch)\n", i);
                    sys_force_redraw = true;
                    system_mode_render();
                    break;
                }
            }
        }

        // return; - НЕ возвращаем, чтобы энкодер переключал страницы
    }

    // ============================================================
    // ВСЕ СТРАНИЦЫ (включая страницу 2): переключение по энкодеру
    // ============================================================
    if (enc_delta != 0 && !mpr_edit_mode) {  // Только если НЕ в режиме редактирования
        int next = sys_page_idx + enc_delta;
        if (next < 0) next = SYS_TOTAL_PAGES - 1;
        if (next >= SYS_TOTAL_PAGES) next = 0;
        sys_page_idx = (uint8_t)next;
        printf("[SYS_PAGE]: %d\n", sys_page_idx + 1);
        sys_force_redraw = true;
        system_mode_render();
    }
}

bool system_mode_needs_redraw(void) {
    return sys_force_redraw;
}

void save_mpr121_mapping(void) {
    // Сохранить mpr_mapping в файл "mpr121.map" на SD
    // Или в EEPROM (если есть)
    printf("[MPR] Mapping saved\n");
}