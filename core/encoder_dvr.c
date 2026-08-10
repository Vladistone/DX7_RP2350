#include "encoder_dvr.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hw_config.h"

static uint8_t last_state_a = 1;

// ====================================================================
// АВТОМАТ СОСТОЯНИЙ КНОПКИ (ТОЛЬКО ОДИН)
// ====================================================================
typedef enum {
    SW_IDLE,
    SW_PRESSED,
    SW_LONG_PRESS
} SwState;

static SwState sw_state = SW_IDLE;
static absolute_time_t sw_timer;
static bool long_press_reported = false;
static bool click_reported = false;

// ====================================================================
// ИНИЦИАЛИЗАЦИЯ
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
    sw_timer = get_absolute_time();
}

// ====================================================================
// ОБНОВЛЕНИЕ СОСТОЯНИЯ КНОПКИ (УПРОЩЁННАЯ ВЕРСИЯ)
// ====================================================================
void encoder_update_sw_state(void) {
    static bool last_stable_state = true;
    static absolute_time_t debounce_time;
    bool current_raw = gpio_get(ENC_PIN_SW);
    bool state_changed = false;

    // Антидребезг: 50 мс
    if (current_raw != last_stable_state) {
        if (absolute_time_diff_us(debounce_time, get_absolute_time()) > 50000) {
            last_stable_state = current_raw;
            debounce_time = get_absolute_time();
            state_changed = true;
        }
    }

    if (!state_changed) return;

    bool is_pressed = !last_stable_state;

    if (is_pressed) {
        // Нажатие
        printf("[SW] Pressed\n");
        sw_timer = get_absolute_time();
        long_press_reported = false;
        sw_state = SW_PRESSED;
    } else {
        // Отпускание
        printf("[SW] Released\n");
        if (!long_press_reported && !click_reported) {
            click_reported = true;
            printf("[SW] Click detected\n");
        }
        sw_state = SW_IDLE;
    }
}

// ====================================================================
// ВРАЩЕНИЕ ЭНКОДЕРА
// ====================================================================
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
// ТЕКУЩЕЕ СОСТОЯНИЕ КНОПКИ (ДЛЯ СОВМЕСТИМОСТИ)
// ====================================================================
bool encoder_is_button_pressed(void) {
    return !gpio_get(ENC_PIN_SW);
}

// ====================================================================
// ОБЪЕДИНЁННЫЙ ДЕТЕКТОР КЛИКОВ
// ====================================================================
uint8_t encoder_get_click_type(void) {
    static enum {
        CLICK_IDLE,
        CLICK_WAITING_FOR_SECOND,
        CLICK_TIMEOUT
    } state = CLICK_IDLE;
    
    static absolute_time_t first_click_time;
    uint8_t result = 0;

    static bool prev_click_reported = false;
    bool click_event = click_reported && !prev_click_reported;
    prev_click_reported = click_reported;

    if (click_event) {
        click_reported = false;

        switch (state) {
            case CLICK_IDLE:
                state = CLICK_WAITING_FOR_SECOND;
                first_click_time = get_absolute_time();
                printf("[SW] First click\n");
                break;

            case CLICK_WAITING_FOR_SECOND:
                if (absolute_time_diff_us(first_click_time, get_absolute_time()) < 700000) {
                    result = 2;
                    state = CLICK_IDLE;
                    printf("[SW] Double click!\n");
                } else {
                    result = 1;
                    state = CLICK_IDLE;
                    printf("[SW] Single click (late second)\n");
                }
                break;

            case CLICK_TIMEOUT:
                result = 1;
                state = CLICK_IDLE;
                printf("[SW] Single click (timeout)\n");
                break;
        }
    }

    if (state == CLICK_WAITING_FOR_SECOND && 
        absolute_time_diff_us(first_click_time, get_absolute_time()) > 700000) {
        state = CLICK_TIMEOUT;
    }

    return result;
}

// ====================================================================
// ДОЛГОЕ НАЖАТИЕ
// ====================================================================
bool encoder_is_long_pressed(void) {
    if (sw_state == SW_PRESSED) {
        if (!long_press_reported && 
            absolute_time_diff_us(sw_timer, get_absolute_time()) > 1000000) {
            long_press_reported = true;
            sw_state = SW_LONG_PRESS;
            printf("[SW] Long press (>1s)\n");
            return true;
        }
    }
    return false;
}