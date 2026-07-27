    #include "sd_card.h"
    #include "pico/stdlib.h"
    #include "hardware/spi.h"
    #include "hw_config.h"

    // ---------------------------------------------------------------------------
    // Инициализация шины SPI и GPIO пинов для SD-карты
    // ---------------------------------------------------------------------------
void sd_spi_init(void) {
    // 1. Старт периферийного блока SPI1 на безопасной частоте 400 кГц
    spi_init(SD_SPI_PORT, 400 * 1000); 

    // 2. Настройка аппаратных функций пинов шины SPI1
    gpio_set_function(SD_SCK_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_MISO_PIN, GPIO_FUNC_SPI);

    // КРИТИЧЕСКИ ДЛЯ RP2350: Включаем встроенную подтяжку к 3.3V
    // Это предотвратит зависание буфера FIFO, если линия «молчит»
    gpio_pull_up(SD_MISO_PIN);
    gpio_pull_up(SD_MOSI_PIN);
    gpio_pull_up(SD_SCK_PIN);

    // 3. Настройка пина CS (Строго как GPIO_OUT)
    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    sd_cs_deselect(); // По умолчанию деактивируем карту (CS = 1)
}


    // ---------------------------------------------------------------------------
    // Вспомогательные функции управления CS и Скоростью
    // ---------------------------------------------------------------------------
    // Активация SD-карты (выбор ведомого устройства - Active LOW)
    void sd_cs_select(void) {
        gpio_put(SD_CS_PIN, 0);
    }

    // Деактивация SD-карты
    void sd_cs_deselect(void) {
        gpio_put(SD_CS_PIN, 1);
    }

    // Перевод SPI на рабочую скорость (12.5 MHz - 25 MHz)
    void sd_spi_set_high_speed(void) {
        // Вызывается только ПОСЛЕ того, как карта ответила на команду CMD0 и перешла в SPI Mode
        spi_set_baudrate(SD_SPI_PORT, 2000 * 1000); // вместо 12500 * 1000
    }