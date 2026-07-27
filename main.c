#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hw_config.h"

// 1. Аппаратные драйверы (core/)
#include "TFT_dvr.h"        
#include "midi_uart.h"      
#include "numpad_dvr.h"     
#include "encoder_dvr.h"    
#include "sd_card.h"        

// 2. Графический и системный слой (services/)
#include "ui_engine.h"
#include "debug_log.h"
#include "sysex_cc_map.h"
#include "mapping.h"
#include "sd_storage.h"

// 3. Логика режимов (modes/)
#include "modes.h"

// Глобальная переменная текущего активного режима
AppModeState g_current_mode = MODE_PLAYBACK;

// ---------------------------------------------------------------------------
// Безопасное переключение режимов и полный рендер экрана TFT
// ---------------------------------------------------------------------------
static void switch_to_next_mode(void) {
    g_current_mode = (AppModeState)((g_current_mode + 1) % MODE_COUNT);
    printf("[MODE_SYS] Switched to mode: %d\n", g_current_mode);

    // Принудительная полная перерисовочка экрана под новый режим
    switch (g_current_mode) {
        case MODE_PLAYBACK:      
            play_mode_render(); 
            break;
        case MODE_FILE_SELECT:   
            sd_review_render(); 
            break;
        case MODE_USB_MIDI:       
            midi_bridge_render(); 
            break;
        case MODE_SYSTEM_CONFIG: 
            system_mode_render(); 
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Инициализация всей периферии
// ---------------------------------------------------------------------------
static void system_init(void) {
    // 1. Старт stdio (USB CDC)
    stdio_init_all();
    printf("=== System Start ===\n");
    sleep_ms(1000); 

    printf("\n=========================================\n");
    printf("   DX7 RP2350 Controller System Start   \n");
    printf("=========================================\n");

    // 2. Явная настройка штатной кнопки GP23 (из hw_config.h)
    gpio_init(BTN_SYS_MODE);
    gpio_set_dir(BTN_SYS_MODE, GPIO_IN);
    gpio_pull_up(BTN_SYS_MODE); // Нажатие замыкает на GND (Active LOW)

    // 3. Настройка индикаторных светодиодов
    gpio_init(LED_INIT_PIN);
    gpio_set_dir(LED_INIT_PIN, GPIO_OUT);
    gpio_put(LED_INIT_PIN, 1); // Включаем LED на время инициализации

    gpio_init(LED_LOOP_PIN);
    gpio_set_dir(LED_LOOP_PIN, GPIO_OUT);
    gpio_put(LED_LOOP_PIN, 0);

    // 4. Инициализация аппаратных драйверов
    tft_init(); 
    ui_set_brightness(50); // Яркость 50% сразу после инициализации TFT
    printf("[INIT] TFT Display 50%... OK\n");

    encoder_init(); // Инициализирует GP4, GP5 и GP14 (ENC_PIN_SW)
    printf("[INIT] Encoder & SW (GP14)... OK\n");
    
    // Инициализация SD
    sd_spi_init();
    printf("Init SD...\n");
    if (!sd_storage_init()) {
        printf("SD mount failed!\n");
    } else {
        printf("SD mount OK.\n");
    }    
    printf("[INIT] SD Card SPI... OK\n");

    // 5. Инициализация профилей SysEx и файловой системы
    sysex_cc_map_init(&map_nucleus2_profile);

    if (sd_storage_init()) {
        printf("[INIT] SD Storage Mounted Successfully!\n");
        sd_storage_load_theme("theme.cfg"); 
        sd_storage_scan_files(".SYX");      
    } else {
        printf("[WARN] SD Storage Mount Failed!\n");
    }

    // 6. Запуск UI Engine
    ui_engine_init();

    // Гасим светодиод инициализации
    gpio_put(LED_INIT_PIN, 0);
    printf("=== Initialization Complete ===\n\n");
}

// ---------------------------------------------------------------------------
// Главный цикл приложения (Event Loop)
// ---------------------------------------------------------------------------
int main(void) {
    system_init();
    printf("System init done.\n");
    // Первоначальный рендер экрана Playback
    play_mode_render();

    // Переменные для обработки кнопки GP23 и тачпада
    uint16_t last_touch = 0;
    bool last_btn_state = true; // High по умолчанию из-за Pull-Up
    absolute_time_t last_btn_time = get_absolute_time();

    printf("Entering main loop...\n");
    while (true) {
        // -------------------------------------------------------------------
        // А. Опрос энкодера и его кнопки SW (GP14)
        // -------------------------------------------------------------------
        printf("Loop tick\n");
        int enc_delta         = encoder_get_delta();
        bool enc_single_click = encoder_is_button_pressed(); // Одиночное нажатие GP13
        bool enc_double_click = encoder_is_double_clicked(); // Двойное нажатие GP13

        // -------------------------------------------------------------------
        // Б. Чтение системной кнопки GP23 (с защитой от дребезга контактов)
        // -------------------------------------------------------------------
        bool current_btn_raw = gpio_get(BTN_SYS_MODE); // 0 (LOW) = Нажата
        bool btn_sys_triggered = false;

        // Если состояние кнопки изменилось и прошло > 50 мс
        if (!current_btn_raw && last_btn_state) {
            if (absolute_time_diff_us(last_btn_time, get_absolute_time()) > 50000) {
                btn_sys_triggered = true;
                last_btn_time = get_absolute_time();
            }
        }
        last_btn_state = current_btn_raw;

        // -------------------------------------------------------------------
        // В. Реакция на смену режимов
        // -------------------------------------------------------------------
        // Смена происходит по нажатию GP23 ИЛИ двойному клику ручки энкодера
        if (btn_sys_triggered || enc_double_click) {
            printf("[EVENT] Mode switch triggered!\n");
            switch_to_next_mode();
        }

        // -------------------------------------------------------------------
        // Г. Передача событий в текущий активный режим
        // -------------------------------------------------------------------
        // Тачпад (0 если пока не используется)
        uint16_t current_touch = 0; 
        // current_touch = mpr121_read_touched(); 

        switch (g_current_mode) {
            case MODE_PLAYBACK:
                play_mode_update(current_touch, enc_delta);
                break;

            case MODE_FILE_SELECT:
                sd_review_update(current_touch, enc_delta);
                break;

            case MODE_USB_MIDI:
                midi_bridge_update(current_touch, enc_delta);
                break;

            case MODE_SYSTEM_CONFIG:
                if (current_touch != last_touch || enc_delta != 0 || enc_single_click) {
                    system_mode_update(current_touch);
                }
                break;

            default:
                break;
        }

        last_touch = current_touch;

        // Короткая задержка для стабильности цикла
        sleep_ms(5);
    }

    return 0;
}