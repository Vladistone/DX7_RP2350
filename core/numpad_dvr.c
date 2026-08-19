#include "numpad_dvr.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hw_config.h"

void mpr121_init(void) {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // 0. Проверка наличия чипа  (просто отправляем адрес)
    uint8_t reg = 0x00;
    uint8_t data[2];
    int res = i2c_write_blocking(I2C_PORT, MPR121_ADDR, NULL, 0, false);
    if (res < 0) {
        printf("[MPR] ERROR: Chip not responding at 0x%02X\n", MPR121_ADDR);
        return;
    }
    
    // 1. Сброс (Stop Mode)
    uint8_t stop_cmd[] = {0x7B, 0x00}; // было 0x5E
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, stop_cmd, 2, false);

    // 2. Пороги срабатывания/отпускания для 12 электродов (опционально)
    for (int i = 0; i < 12; i++) {
        uint8_t touch_thresh[] = {0x41 + (i * 2), 12}; // Touch Threshold
        uint8_t release_thresh[] = {0x42 + (i * 2), 6}; // Release Threshold
        i2c_write_blocking(I2C_PORT, MPR121_ADDR, touch_thresh, 2, false);
        i2c_write_blocking(I2C_PORT, MPR121_ADDR, release_thresh, 2, false);
    }

    // 3. Включение всех 12 электродов (байт 0x0F включает электроды 0-3, 0xFF - все 12?)
    // Согласно даташиту, биты 0-11 соответствуют электродам 0-11.
    // Чтобы включить все 12, нужно записать 0x0F в регистр 0x7B? НЕТ! 0x0F включает только 4 электрода.
    // Для включения всех 12 электродов нужно записать 0xFF в регистр 0x7B? 
    // Снова нет. В MPR121 регистр 0x7B - это Electrode Enable (биты 0-11). 
    // Чтобы включить 12 электродов, нужно записать 0x0F в регистр 0x7B? 
    // Стоп. В даташите MPR121: регистр 0x7B (ELE_CFG) - это Electrode Configuration.
    // Бит 0-3: ELE_CFG (количество включенных электродов).
    // 0x00 = STOP, 0x01 = 1 электрод, 0x02 = 2 электрода, ..., 0x0C = 12 электродов.
    // ПРАВИЛЬНО: для 12 электродов нужно записать 0x0C в регистр 0x7B!
    uint8_t run_cmd[] = {0x7B, 0x0C}; // было 0x5E
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, run_cmd, 2, false);
}

uint16_t mpr121_read_touched( void) {
    //uint8_t reg = 0x00;
    uint8_t data[ 2] = { 0};
    uint8_t reg = 0x73;
    //uint8_t data = 0;

    i2c_write_blocking(I2C_PORT, MPR121_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, MPR121_ADDR, &data, 1, false);
    printf("[MPR] Version: 0x%02X\n", data); // Должно быть 0xB0 или подобное
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