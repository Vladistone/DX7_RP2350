#include "numpad_dvr.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hw_config.h"

void mpr121_init(void) {
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Сброс (Stop Mode)
    uint8_t stop_cmd[] = {0x5E, 0x00};
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, stop_cmd, 2, false);

    // Пороги срабатывания/отпускания для 12 электродов (опционально)
    for (int i = 0; i < 12; i++) {
        uint8_t touch_thresh[] = {0x41 + (i * 2), 12}; // Touch Threshold
        uint8_t release_thresh[] = {0x42 + (i * 2), 6}; // Release Threshold
        i2c_write_blocking(I2C_PORT, MPR121_ADDR, touch_thresh, 2, false);
        i2c_write_blocking(I2C_PORT, MPR121_ADDR, release_thresh, 2, false);
    }

    // Включение всех 12 электродов (Run Mode)
    uint8_t run_cmd[] = {0x5E, 0x8F};
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, run_cmd, 2, false);
}

uint16_t mpr121_read_touched( void) {
    uint8_t reg = 0x00;
    uint8_t data[ 2] = { 0};
    
    // Безопасный таймаут в 2000 микросекунд (2 миллисекунды)
    // Если чип MPR121 завис или шина занята, функция НЕ повесит процессор, 
    // а просто вернет ошибку PICO_ERROR_TIMEOUT и пойдет дальше!
    int res = i2c_write_blocking_until( I2C_PORT, MPR121_ADDR, & reg, 1, true, make_timeout_time_us(2000));
    
    if (res < 0) {
        // Ошибка шины или таймаут — плавно выходим, возвращая 0 (кнопки не нажаты)
        return 0; 
    }

    res = i2c_read_blocking_until( I2C_PORT, MPR121_ADDR, data, 2, false, make_timeout_time_us(2000));
    if (res < 0) {
        return 0;
    }

    return ( uint16_t)( data[ 0] | ( data[ 1] << 8));
}