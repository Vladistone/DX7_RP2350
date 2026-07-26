#include "encoder_dvr.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hw_config.h"

static uint8_t last_state_a = 1;

// Состояния для автомата двойного клика кнопки
typedef enum {
    BTN_STATE_IDLE,
    BTN_STATE_PRESSED_WAIT_RELEASE,
    BTN_STATE_WAIT_SECOND_PRESS
} ButtonClickState;

static ButtonClickState btn_click_state = BTN_STATE_IDLE;
static absolute_time_t press_timer;
static absolute_time_t debounce_timer;
static bool last_raw_btn_state = true; // Подтяжка к питанию (HIGH в покое)

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
}

// Возвращает +1 (вращение вправо), -1 (вращение влево) или 0 (нет движения)
int encoder_get_delta(void) {
    int delta = 0;
    uint8_t state_a = gpio_get(ENC_PIN_A);
    
    if (state_a != last_state_a) {
        if (state_a == 0) { // Срабатывание по спаду сигнала A (GP10)
            if (gpio_get(ENC_PIN_B) == 1) { // Сигнал B (GP11)
                delta = 1;  // Вправо
            } else {
                delta = -1; // Влево
            }
        }
        last_state_a = state_a;
    }
    return delta;
}

// Обычное одиночное нажатие (для совместимости)
bool encoder_is_button_pressed(void) {
    return !gpio_get(ENC_PIN_SW);
}

// Надежный детектор двойного клика с защитой от дребезга (окно 500 мс)
bool encoder_is_double_clicked(void) {
    bool current_raw = gpio_get(ENC_PIN_SW);
    absolute_time_t now = get_absolute_time();
    bool double_clicked_detected = false;

    // 1. АНТИДРЕБЕЗГ:фильтруем дребезг контактов в пределах 20 мс
    if (current_raw != last_raw_btn_state) {
        if (absolute_time_diff_us(debounce_timer, now) > 20000) { 
            last_raw_btn_state = current_raw;
            debounce_timer = now;
        }
    }

    bool button_pressed = !last_raw_btn_state; // Инверсия (нажатие = LOW)

    // 2. АВТОМАТ СОСТОЯНИЙ ДВОЙНОГО КЛИКА
    switch (btn_click_state) {
        case BTN_STATE_IDLE:
            if (button_pressed) {
                btn_click_state = BTN_STATE_PRESSED_WAIT_RELEASE;
                press_timer = now; 
            }
            break;

        case BTN_STATE_PRESSED_WAIT_RELEASE:
            if (!button_pressed) { // Кнопку отпустили после первого клика
                btn_click_state = BTN_STATE_WAIT_SECOND_PRESS;
                press_timer = now; // Перезапуск таймера ожидания второго клика
            }
            break;

        case BTN_STATE_WAIT_SECOND_PRESS:
            if (button_pressed) { // Поймали второе нажатие
                if (absolute_time_diff_us(press_timer, now) <= 500000) { // Уложились в 0.5 сек
                    double_clicked_detected = true;
                }
                btn_click_state = BTN_STATE_PRESSED_WAIT_RELEASE; 
            } 
            else {
                if (absolute_time_diff_us(press_timer, now) > 500000) { // Таймаут истек
                    btn_click_state = BTN_STATE_IDLE;
                }
            }
            break;
    }

    return double_clicked_detected;
}