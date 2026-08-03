#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
//#include "tusb.h"

// Подключение модулей проекта по вашей структуре
#include "hw_config.h"
#include "modes.h"
#include "encoder_dvr.h"
#include "numpad_dvr.h"
#include "sd_storage.h"
#include "ui_engine.h"
#include "debug_log.h"
#include "midi_uart.h"
#include "TFT_dvr.h"

// Объявления внешних функций инициализации, чтобы убрать варнинги компилятора
bool sd_review_init(void);
void system_mode_init(void);
bool get_encoder_switch(void);

// ИСПРАВЛЕНО: Используем строго ваш тип данных AppModeState вместо system_mode_t
AppModeState g_current_mode = MODE_PLAYBACK;

// Вспомогательные переменные для отслеживания состояния питания устройства
float g_vsys_voltage = 0.0f;
uint32_t g_last_vsys_time = 0;

// Внутренняя функция замера напряжения на системной шине питания VSYS (ADC3)
void update_vsys_voltage(void) {
    uint32_t now = time_us_32();
    if (now - g_last_vsys_time < 1000000) return; // Опрашиваем строго раз в секунду
    g_last_vsys_time = now;

    adc_select_input(3);
    uint16_t raw = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    g_vsys_voltage = raw * conversion_factor * 3.0f; // Делитель напряжения 1:3 на плате
}

// Системная инициализация всей аппаратуры контроллера при старте
void system_init(void) {
    // Штатная инициализация всех портов ввода-вывода (UART + фоновый stdio_usb)
    // stdio_init_all();
    stdio_uart_init();
    
    // Инициализация АЦП для контроля питания
    adc_init();
    adc_gpio_init(29); // Пин VSYS на RP2350
    
    // Инициализация физических низкоуровневых драйверов уровня core/
    encoder_init();
    mpr121_init();
    
    // Настройка пина дублера режимов GP23 на вход с подтяжкой к 3.3V
    gpio_init(23);
    gpio_set_dir(23, GPIO_IN);
    gpio_pull_up(23);

    printf("================================================\n");
    printf("DX7 RP2350 Controller System Start\n");
    printf("================================================\n");

    printf("[INIT] TFT Display 50%%... ");
    tft_init();
    printf("OK\n");

    printf("[INIT] Encoder & SW (GP14)... OK\n");

    printf("Init SD...\n");
    if (!sd_storage_init()) {
        printf("[WARN] SD Storage Mount Failed!\n");
    }
    printf("[INIT] SD Card SPI... OK\n");

    // Запуск графического движка и прорисовка стартового экрана
    ui_engine_init();
    
    printf("=== Initialization Complete ===\n\n");
    printf("System init done.\nEntering main loop...\n");
}

int main(void) {
    // Запуск базовой аппаратной инициализации
    system_init();

    // Переменные для фиксации состояний кнопок и тача
    uint16_t last_touch = 0;
    
    // Хронометражные переменные для защиты от дребезга и фиксации Long Click
    uint32_t press_start_time = 0;
    bool was_pressed = false;

    // Сбрасываем FSM в стартовое состояние при первом входе
    g_current_mode = MODE_PLAYBACK;
    
    // === ЖЕЛЕЗОБЕТОННЫЙ ПРОРЫВ ДЕФОЛТНОГО ЭКРАНА ПРИ СТАРТЕ ===
    // Принудительно вызываем обновление игрового режима с флагом touched = true 
    // ВСЕГО ОДИН РАЗ до входа в бесконечный цикл.
    // Это заставит ui_engine стереть заставку и нарисовать чистый PLAY mode 
    // мгновенно при включении питания, даже без подключенного USB/CoolTerm!
    play_mode_update(true, 0); 

    // =========================================================================
    // ГЛАВНЫЙ ИСПОЛНИТЕЛЬНЫЙ ЦИКЛ УСТРОЙСТВА (ОДНО ЯДРО RP2350)
    // =========================================================================
    while (1) {
        // Фоновая работа стека TinyUSB (Вызываем строго если кабель подключен к ПК)
        //if (tud_cdc_connected()) {
        //    tud_task();
        //}

        // Чтение напряжения питания
        update_vsys_voltage();

        // 1. Опрос сенсорной панели MPR121 на шине I2C_PORT (i2c1)
        uint16_t pad_raw = mpr121_read_touched();

        // ЖЕСТКИЙ ФИЛЬТР СБОЯ ШИНЫ I2C:
        // Если из-за помех кабеля или просадок шина выдала 0xFFFF — принудительно зануляем,
        // чтобы авария I2C не вызвала ложное тотальное зажатие всех 12 кнопок!
        if (pad_raw == 0xFFFF) {
            pad_raw = 0;
        }

        // 2. Опрос механических органов управления (драйверы уровня core/)
        // =========================================================================
        // ВОЗВРАЩАЕМ СТАРУЮ ПРОВЕРЕННУЮ ЛОГИКУ СОБЫТИЙ С ФИЛЬТРАЦИЕЙ ВРЕМЕНИ
        // =========================================================================

        // Читаем физику (как в твоем стабильном коде)
        int enc_delta = encoder_get_delta(); 
        bool enc_sw_now = !encoder_is_button_pressed(); 
        bool gp23_now   = !gpio_get(23);     

        static uint32_t press_duration_start = 0;
        static bool fsm_changed_this_press = false;

        bool is_fsm_switch_trigger = false;
        bool raw_button_touch = enc_sw_now; // Фиксируем факт физического контакта

        // Если нажата кнопка энкодера или дублер GP23
        if (enc_sw_now || gp23_now) {
            if (press_duration_start == 0) {
                press_duration_start = time_us_32();
                fsm_changed_this_press = false;
            } else {
                // Если удерживаем кнопку дольше 450 мс и мы ЕЩЕ не меняли режим за это нажатие
                if (!fsm_changed_this_press && (time_us_32() - press_duration_start > 450000)) {
                    is_fsm_switch_trigger = true; // Триггер для смены режима
                    fsm_changed_this_press = true; // Запираем замок, чтобы не переключать по кругу
                    raw_button_touch = false;      // Обнуляем тач, чтобы "хвост" удержания не шел в меню!
                }
            }
        } else {
            // Кнопки полностью отпущены — сбрасываем таймеры
            press_duration_start = 0;
            fsm_changed_this_press = false;
        }

        // Если нажат дублер GP23 — он работает как мгновенный жесткий триггер смены режима,
        // полностью очищая клики для меню файлов
        if (gp23_now && !fsm_changed_this_press) {
            is_fsm_switch_trigger = true;
            fsm_changed_this_press = true;
            raw_button_touch = false;
        }

        // Настоящий, отзывчивый current_touch (РАБОТАЕТ НА НАЖАТИЕ, КАК В СТАРОМ КОДЕ)
        // Но если сработал триггер удержания FSM — мы его гасим, спасая от авто-входа папки
        bool current_touch = (pad_raw > 0) || raw_button_touch;

        // 4. ГЛОБАЛЬНЫЙ ПЕРЕКЛЮЧАТЕЛЬ РЕЖИМОВ (FSM)
        if (is_fsm_switch_trigger) {
            is_fsm_switch_trigger = false;
            
            if (g_current_mode == MODE_PLAYBACK) {
                g_current_mode = MODE_FILE_SELECT;
                current_touch = false; // Блокируем сквозной проход в корень
                enc_delta = 0;
                sd_review_init(); 
            } 
            else if (g_current_mode == MODE_FILE_SELECT) {
                g_current_mode = MODE_SYSTEM_CONFIG;
                current_touch = false;
                enc_delta = 0;
                system_mode_init(); 
            } 
            else {
                g_current_mode = MODE_PLAYBACK;
                current_touch = false;
                enc_delta = 0;
            }
            
            debug_log_print("[FSM] Mode changed via Long Press to: %d\n", g_current_mode);
            
            if (gp23_now) {
                sleep_ms(200);
            }
        }

        // Вставить строго перед switch (g_current_mode):
        static bool first_pass = true;
        if (first_pass) {
            first_pass = false;
            printf("[DEBUG PASS 1] pad: 0x%04X, enc_sw: %d, gp23: %d, mode: %d\n", 
                   pad_raw, encoder_is_button_pressed(), gpio_get(23), g_current_mode);
        }
        // 5. РАСПРЕДЕЛЕНИЕ ПОЛНОМОЧИЙ: Передача событий в текущий активный режим
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
                // В режиме диагностики отслеживаем изменения состояний тача или шагов ротера
                if (current_touch != last_touch || enc_delta != 0) {
                    system_mode_update(current_touch, enc_delta);
                }
                break;

            default:
                break;
        }

        // Запоминаем состояние тача для анализа изменений на следующем витке цикла
        last_touch = current_touch;

        // Короткая задержка в 5 миллисекунд для стабильности планировщика прерываний
        sleep_ms(5);
    }

    return 0;
}
