#include "encoder_dvr.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hw_config.h"

static uint8_t last_state_a = 1;

// ====================================================================
// НОВЫЙ АВТОМАТ СОСТОЯНИЙ КНОПКИ (С АНТИДРЕБЕЗГОМ)
// ====================================================================
typedef enum {
    SW_IDLE,              // Кнопка отпущена (HIGH)
    SW_DEBOUNCE_PRESS,    // Дребезг при нажатии
    SW_PRESSED,           // Кнопка зажата (LOW)
    SW_DEBOUNCE_RELEASE,  // Дребезг при отпускании
    SW_LONG_PRESS         // Долгое нажатие (> 1 сек)
} SwState;

static SwState sw_state = SW_IDLE;
static absolute_time_t sw_timer;
static bool long_press_reported = false;   // Флаг однократного отчета
static bool click_reported = false;        // Флаг однократного отчета

// Состояния для автомата двойного клика (СОХРАНЕНЫ)
typedef enum {
    BTN_STATE_IDLE,
    BTN_STATE_PRESSED_WAIT_RELEASE,
    BTN_STATE_WAIT_SECOND_PRESS
} ButtonClickState;

static ButtonClickState btn_click_state = BTN_STATE_IDLE;
static absolute_time_t press_timer;
static absolute_time_t debounce_timer;
static bool last_raw_btn_state = true;

// ====================================================================
// ИНИЦИАЛИЗАЦИЯ (С ТАЙМЕРАМИ)
// ====================================================================
void encoder_init(void) {
    gpio_init(ENC_PIN_A);
    gpio_set_dir(ENC_PIN_A, GPIO_IN);
    gpio_pull_up(ENC_PIN_A);

    gpio_init(ENC_PIN_B);
    gpio_set_dir(ENC_PIN_B, GPIO_IN);
    gpio_pull_up(ENC_PIN_B);

    gpio_init(ENC_PIN_SW);
    gpio_set_dir(ENC_PIN_SW, GPIO_IN);
    gpio_pull_up(ENC_PIN_SW);

    last_state_a = gpio_get(ENC_PIN_A);
    
    // Инициализация таймеров
    absolute_time_t now = get_absolute_time();
    sw_timer = now;
    debounce_timer = now;
    press_timer = now;
}

/// ====================================================================
// ОБНОВЛЕНИЕ СОСТОЯНИЯ КНОПКИ (С ЗАЩИТОЙ ОТ ЗАВИСАНИЯ)
// ====================================================================
void encoder_update_sw_state(void) {
    bool raw = gpio_get(ENC_PIN_SW);
    absolute_time_t now = get_absolute_time();
    static absolute_time_t last_call_time = {0};
    
    // Защита от слишком частых вызовов (не чаще 1 мс)
    if (absolute_time_diff_us(last_call_time, now) < 1000) {
        return;
    }
    last_call_time = now;
    
    bool state_changed = false;

    // Фильтр дребезга (только при изменении)
    if (raw != last_raw_btn_state) {
        if (absolute_time_diff_us(debounce_timer, now) > 30000) {
            last_raw_btn_state = raw;
            debounce_timer = now;
            state_changed = true;
        }
    }

    if (!state_changed) return;

    bool is_pressed = !last_raw_btn_state;

    switch (sw_state) {
        case SW_IDLE:
            if (is_pressed) {
                sw_state = SW_DEBOUNCE_PRESS;
                sw_timer = now;
                long_press_reported = false;
                click_reported = false;
            }
            break;

        case SW_DEBOUNCE_PRESS:
            if (absolute_time_diff_us(sw_timer, now) > 50000) {
                if (is_pressed) {
                    sw_state = SW_PRESSED;
                    printf("[SW] Pressed (stable)\n");
                } else {
                    sw_state = SW_IDLE;
                }
            } else if (absolute_time_diff_us(sw_timer, now) > 100000) {
                // Таймаут антидребезга
                sw_state = SW_IDLE;
                printf("[SW] Debounce timeout\n");
            }
            break;

        case SW_PRESSED:
            if (!long_press_reported && absolute_time_diff_us(sw_timer, now) > 1000000) {
                long_press_reported = true;
                sw_state = SW_LONG_PRESS;
                printf("[SW] Long press (>1s)\n");
            }

            if (!is_pressed) {
                sw_state = SW_DEBOUNCE_RELEASE;
                sw_timer = now;
            }
            break;

        case SW_DEBOUNCE_RELEASE:
            if (absolute_time_diff_us(sw_timer, now) > 50000) {
                if (!is_pressed) {
                    sw_state = SW_IDLE;
                    if (!long_press_reported && !click_reported) {
                        click_reported = true;
                        printf("[SW] Click detected\n");
                    }
                } else {
                    sw_state = SW_PRESSED;
                }
            } else if (absolute_time_diff_us(sw_timer, now) > 100000) {
                sw_state = SW_IDLE;
                printf("[SW] Release timeout\n");
            }
            break;

        case SW_LONG_PRESS:
            if (!is_pressed) {
                sw_state = SW_DEBOUNCE_RELEASE;
                sw_timer = now;
            }
            break;
    }
}

// ====================================================================
// СТАРЫЕ ФУНКЦИИ (СОХРАНЕНЫ, НО С НОВОЙ ЛОГИКОЙ)
// ====================================================================

// Возвращает +1 (вращение вправо), -1 (вращение влево) или 0 (нет движения)
int encoder_get_delta(void) {
    int delta = 0;
    uint8_t state_a = gpio_get(ENC_PIN_A);

    if (state_a != last_state_a) {
        if (state_a == 0) {
            if (gpio_get(ENC_PIN_B) == 1) {
                delta = 1;
            } else {
                delta = -1;
            }
        }
        last_state_a = state_a;
    }
    return delta;
}

// ====================================================================
// encoder_is_button_pressed() - ВОЗВРАЩАЕТ ТЕКУЩЕЕ СОСТОЯНИЕ (БЕЗ АНТИДРЕБЕЗГА)
// ====================================================================
bool encoder_is_button_pressed(void) {
    return !gpio_get(ENC_PIN_SW);
}
// ====================================================================
// ОБЪЕДИНЁННЫЙ ДЕТЕКТОР КЛИКОВ (СНАЧАЛА ДВОЙНОЙ, ПОТОМ ОДИНОЧНЫЙ)
// ====================================================================
uint8_t encoder_get_click_type(void) {
    static enum {
        CLICK_IDLE,
        CLICK_WAITING_FOR_SECOND,
        CLICK_TIMEOUT
    } state = CLICK_IDLE;
    
    static absolute_time_t first_click_time;
    uint8_t result = 0;

    // 1. Получаем событие клика из автомата
    static bool prev_click_reported = false;
    bool click_event = click_reported && !prev_click_reported;
    prev_click_reported = click_reported;

    if (click_event) {
        click_reported = false; // Сбрасываем флаг

        switch (state) {
            case CLICK_IDLE:
                // Первый клик — начинаем ожидание второго
                state = CLICK_WAITING_FOR_SECOND;
                first_click_time = get_absolute_time();
                printf("[SW] First click, waiting...\n");
                break;

            case CLICK_WAITING_FOR_SECOND:
                // Второй клик — проверяем интервал
                if (absolute_time_diff_us(first_click_time, get_absolute_time()) < 700000) {
                    result = 2; // Двойной клик
                    state = CLICK_IDLE;
                    printf("[SW] Double click!\n");
                } else {
                    // Интервал превышен — возвращаем одиночный клик
                    result = 1;
                    state = CLICK_IDLE;
                    printf("[SW] Single click (second too late)\n");
                }
                break;

            case CLICK_TIMEOUT:
                // Таймаут — возвращаем одиночный
                result = 1;
                state = CLICK_IDLE;
                printf("[SW] Single click (timeout)\n");
                break;
        }
    }

    // 2. Таймаут ожидания второго клика (700 мс)
    if (state == CLICK_WAITING_FOR_SECOND && 
        absolute_time_diff_us(first_click_time, get_absolute_time()) > 700000) {
        state = CLICK_TIMEOUT;
        // Следующий вызов вернёт одиночный клик
    }

    return result;
}

// ====================================================================
// НОВАЯ ФУНКЦИЯ: ПРОВЕРКА ДОЛГОГО НАЖАТИЯ (ДЛЯ system_mode_update)
// ====================================================================
bool encoder_is_long_pressed(void) {
    // Возвращает true ОДИН РАЗ при обнаружении долгого нажатия
    static bool prev_long_reported = false;
    bool long_event = long_press_reported && !prev_long_reported;
    prev_long_reported = long_press_reported;
    return long_event;
}