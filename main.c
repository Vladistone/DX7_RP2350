#include <stdio.h>
#include <string.h>
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

bool sd_review_init(void);

// Глобальная переменная текущего активного режима
AppModeState g_current_mode = MODE_PLAYBACK;

// Флаг, что SD-карта была просканирована
static bool sd_scanned = false;

// Явное объявление вашей реальной функции из драйвера numpad_dvr.c
uint16_t mpr121_read_touched( void); 

// ---------------------------------------------------------------------------
// Вывод красивого стартового баннера с датой и временем сборки
// ---------------------------------------------------------------------------
void print_system_banner(void) {
    char m_name[4];
    int day, year;
    // Парсим макрос даты компиляции GCC (например, "Jul 31 2026")
    sscanf(__DATE__, "%3s %d %d", m_name, &day, &year);

    // Переводим текстовый месяц в цифру
    int month = 1;
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; i++) {
        if (strcmp(m_name, months[i]) == 0) { 
            month = i + 1; 
            break; 
        }
    }

    // Вырезаем часы и минуты из времени компиляции (например, "23:50:15")
    int hour, min;
    sscanf(__TIME__, "%d:%d", &hour, &min);

    printf("\n================================================\n");
    // Выводим строго в формате ЧЧ:ММ ДД.ММ.ГГ
    printf("DX7 RP2350 Controller System Start: %02d:%02d %02d.%02d.%02d\n", 
           hour, min, day, month, year % 100);
    printf("================================================\n");
}

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
            // Сканируем SD только при первом входе в режим
            if (!sd_scanned) {
                if (sd_review_init()) {
                    sd_scanned = true;
                } else {
                    printf("[SD] Init failed\n");
                }
            }
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

//void print_system_banner(void) {
    // Пример логики, если в будущем прикрутите модуль точного времени по I2C:
    // rtc_time_t t;
    // rtc_get_time(&t);
    // printf(" DX7 RP2350 Controller System Start: %02d:%02d %02d.%02d.%02d\n", t.hour, t.min, t.day, t.month, t.year)
//}

// ---------------------------------------------------------------------------
// Инициализация всей периферии
// ---------------------------------------------------------------------------
static void system_init(void) {
    // 1. Старт stdio (USB CDC)
    stdio_init_all();
    printf("=== System Start ===\n");
    sleep_ms(1000); 
    
    // Вызываем хронометрированный баннер
    print_system_banner(); 

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
    printf("[INIT] TFT Display 50%%... OK\n");

    encoder_init(); // Инициализирует GP4, GP5 и GP14 (ENC_PIN_SW)
    printf("[INIT] Encoder & SW (GP14)... OK\n");
    
    // 5. Инициализация SD-карты (только один раз при старте)
    sd_spi_init();
    printf("Init SD...\n");
    if (!sd_storage_init()) {
        printf("[WARN] SD Storage Mount Failed!\n");
    } else {
        printf("[INIT] SD Storage Mounted!\n");
//        sd_storage_load_theme("theme.cfg");
    }
    printf("[INIT] SD Card SPI... OK\n");

    // 6. Инициализация профилей SysEx и файловой системы
    sysex_cc_map_init(&map_nucleus2_profile);

    // 7. Запуск UI Engine
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
        // А. Монолитный опрос всей физической периферии (РАБОТАЕТ ВСЕГДА)
        // -------------------------------------------------------------------
        int enc_delta         = encoder_get_delta();
        bool enc_single_click = encoder_is_button_pressed();
        bool enc_double_click = encoder_is_double_clicked();

        // Читаем 12 каналов сенсорного нумпада MPR121
        uint16_t pad_raw      = mpr121_read_touched();

        // Склеиваем события активации для режима меню (клики и тачи)
        // Объединяем физику: если нажат нумпад или кликнул энкодер — это touched!
        uint16_t current_touch = pad_raw; 
        if (enc_single_click) {
            current_touch |= 0XFFFF; // Взводим биты, если была нажата механика
        }

        // -------------------------------------------------------------------
        // Б. Чтение системной кнопки GP23 (с защитой от дребезга контактов)
        // -------------------------------------------------------------------
        bool current_btn_raw = gpio_get(BTN_SYS_MODE);
        bool btn_sys_triggered = false;

        if (!current_btn_raw && last_btn_state) {
            if (absolute_time_diff_us(last_btn_time, get_absolute_time()) > 50000) {
                btn_sys_triggered = true;
                last_btn_time = get_absolute_time();
            }
        }
        last_btn_state = current_btn_raw;

        // -------------------------------------------------------------------
        // В. Реакция на смену режимов (по кнопке или дабл-клику)
        // -------------------------------------------------------------------
        if (btn_sys_triggered || enc_double_click) {
            printf("[EVENT] Mode switch triggered!\n");
            switch_to_next_mode();
            
            // Если переключились на режим SD-карты, принудительно инициализируем том
            if (g_current_mode == MODE_FILE_SELECT) {
                sd_review_init();
            }
        }

        // -------------------------------------------------------------------
        // Г. Передача событий в текущий активный режим через эффективный SWITCH
        // -------------------------------------------------------------------
        switch (g_current_mode) {
            case MODE_PLAYBACK:
                // Передаем реальные клики и шаги в режим синтезатора
                play_mode_update(current_touch, enc_delta);
                break;

            case MODE_FILE_SELECT:
                // Если нумпад выдает мусор на шине, сбрасываем его, оставляя только механику
                if (pad_raw == 0xFFFF) {
                    pad_raw = 0;
                }
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

        // Короткая задержка для стабильности цикла, опроса I2C и SPI
        sleep_ms(5);
    }

    return 0;
}