#include "numpad_dvr.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hw_config.h"

// Флаг, что MPR121 уже инициализирован
static bool mpr121_initialized = false;

void mpr121_init(void) {
    // Если уже инициализирован — просто выводим сообщение и выходим
    if (mpr121_initialized) {
        printf("[MPR] Already initialized, skipping\n");
        return;
    }

    // Инициализация I2C
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // 1. Проверка наличия чипа
    int res = i2c_write_blocking(I2C_PORT, MPR121_ADDR, NULL, 0, false);
    if (res < 0) {
        printf("[MPR] ERROR: Chip not responding at 0x%02X\n", MPR121_ADDR);
        return;
    }
    printf("[MPR] Chip found at 0x%02X\n", MPR121_ADDR);

    // 2. Чтение версии
    uint8_t ver_reg = 0x73;
    uint8_t version = 0;
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, &ver_reg, 1, true);
    i2c_read_blocking(I2C_PORT, MPR121_ADDR, &version, 1, false);
    printf("[MPR] Version: 0x%02X\n", version);

    // 3. Сброс чипа через STOP mode (без Software Reset, который не работает)
    uint8_t stop_cmd[] = {0x7B, 0x00};
    res = i2c_write_blocking(I2C_PORT, MPR121_ADDR, stop_cmd, 2, false);
    if (res < 0) {
        printf("[MPR] STOP mode failed!\n");
        return;
    }
    sleep_ms(50);
    printf("[MPR] STOP mode set\n");

    // 4. Настройка фильтра (MHD, NHD, NCL)
    uint8_t filter_cmd[] = {0x5E, 0x2F};
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, filter_cmd, 2, false);

    // 5. Настройка заряда/разряда (CDT)
    uint8_t cdt_cmd[] = {0x5F, 0x02};
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, cdt_cmd, 2, false);

    // 6. Настройка максимальной ёмкости
    uint8_t max_cap_cmd[] = {0x6B, 0x10};
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, max_cap_cmd, 2, false);

    // 7. Настройка порогов
    uint8_t touch_threshold = 16;
    uint8_t release_threshold = 8;
    
    for (int i = 0; i < 12; i++) {
        uint8_t touch_cmd[] = {0x41 + (i * 2), touch_threshold};
        uint8_t release_cmd[] = {0x42 + (i * 2), release_threshold};
        i2c_write_blocking(I2C_PORT, MPR121_ADDR, touch_cmd, 2, false);
        i2c_write_blocking(I2C_PORT, MPR121_ADDR, release_cmd, 2, false);
    }
    printf("[MPR] Thresholds: Touch=%d, Release=%d\n", touch_threshold, release_threshold);

    // 8. Включение всех 12 электродов (RUN mode)
    uint8_t run_cmd[] = {0x7B, 0x0C};
    res = i2c_write_blocking(I2C_PORT, MPR121_ADDR, run_cmd, 2, false);
    if (res < 0) {
        printf("[MPR] RUN mode failed!\n");
        return;
    }
    sleep_ms(100);
    printf("[MPR] RUN mode set\n");

    // 9. Калибровка
    printf("[MPR] Calibrating...\n");
    for (int i = 0; i < 20; i++) {
        mpr121_read_touched();
        sleep_ms(50);
    }

    // 10. Проверка статуса
    uint8_t status = 0x00;
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, &status, 1, true);
    i2c_read_blocking(I2C_PORT, MPR121_ADDR, &status, 1, false);
    printf("[MPR] Status: 0x%02X\n", status);

    mpr121_initialized = true;
    printf("[MPR] Initialized successfully\n");
}

// максимально простая версия с большим таймаутом:
uint16_t mpr121_read_touched(void) {
    uint8_t reg = 0x00;
    uint8_t data[2] = {0};

    // Простое чтение с большим таймаутом
    int res = i2c_write_blocking(I2C_PORT, MPR121_ADDR, &reg, 1, true);
    if (res < 0) return 0;
    
    res = i2c_read_blocking(I2C_PORT, MPR121_ADDR, data, 2, false);
    if (res < 0) return 0;

    return (uint16_t)(data[0] | (data[1] << 8)) & 0x0FFF;
}
/*
uint16_t mpr121_read_touched(void) {
    uint8_t reg = 0x00;
    uint8_t data[2] = {0};

    // Используем blocking_until с таймаутом
    int res = i2c_write_blocking_until(I2C_PORT, MPR121_ADDR, &reg, 1, true, make_timeout_time_us(1000));
    if (res < 0) {
        //printf("[MPR] I2C write timeout\n");
        return 0;
    }

    res = i2c_read_blocking_until(I2C_PORT, MPR121_ADDR, data, 2, false, make_timeout_time_us(1000));
    if (res < 0) {
        printf("[MPR] I2C read timeout\n");
        return 0;
    }

    return (uint16_t)(data[0] | (data[1] << 8)) & 0x0FFF;
}
*/