    #include "sd_card.h"
    #include "pico/stdlib.h"
    #include "hardware/spi.h"
    #include "hw_config.h"

    // ---------------------------------------------------------------------------
    // Инициализация шины SPI и GPIO пинов для SD-карты
    // ---------------------------------------------------------------------------
void sd_spi_init(void) {

    // 1. Сначала жестко инициализируем пин CS как обычный GPIO OUT и выключаем карту!
    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    gpio_put(SD_CS_PIN, 1); // Деактивируем карту (CS = 1) - КРИТИЧНО!
    
    // 1. Инициализируем сам блок SPI1 на частоте 400 кГц
    spi_init(SD_SPI_PORT, 400 * 1000); 

    // 2. Возвращаем пинам СТРОГУЮ аппаратную привязку к SPI1 (Функция 5 / ALT5)
    gpio_set_function(SD_SCK_PIN,  GPIO_FUNC_SPI); // GP10 -> SPI1 SCK
    gpio_set_function(SD_MOSI_PIN, GPIO_FUNC_SPI); // GP11 -> SPI1 TX
    gpio_set_function(SD_MISO_PIN, GPIO_FUNC_SPI); // GP8  -> SPI1 RX

    // 2. Настройка аппаратных функций пинов шины SPI1
    // Принудительно мапим пины на шину SPI1, игнорируя дефолтный SPI0:
    // (Согласно официальному Datasheet RP2350, Function Matrix, для пинов GP8, GP10, GP11
    // цифра 6 (ALT6) возможно отвечает за жесткую коммутацию с блоком SPI1,
    // в то время как дефолтный макрос уводит их на функцию SPI0)).
    
    // 2.1 В матрице RP2350 для GP8-11 блок SPI1 вызывается через явное указание или встроенный макрос:
    // gpio_set_function(SD_SCK_PIN,  GPIO_FUNC_XIP);
    
    // 2.2 или самый надежный и переносимый вариант для Pico SDK:
    //gpio_set_function(SD_SCK_PIN, 6);  // Функция 6 для GP10 — это строго SPI1 SCK
    //gpio_set_function(SD_MOSI_PIN, 6); // Функция 6 для GP11 — это строго SPI1 TX
    //gpio_set_function(SD_MISO_PIN, 6); // Функция 6 для GP8  — это строго SPI1 RX

    // 2.3 === КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ ПОД ЖЕЛЕЗО RP2350 ===
    // Отключаем триггер Шмитта и убираем внутренние утечки тока, 
    // чтобы вход MISO стал максимально чувствительным к модулю HW-203
    //gpio_set_input_hysteresis_enabled(SD_MISO_PIN, false); // Отключаем Шмитта на приемнике
    //gpio_set_slew_rate(SD_SCK_PIN, GPIO_SLEW_RATE_FAST);    // Разгоняем фронты тактов
    //gpio_set_slew_rate(SD_MOSI_PIN, GPIO_SLEW_RATE_FAST);   // Разгоняем фронты данных

    // 3. КРИТИЧЕСКИ ДЛЯ RP2350: Включаем встроенную подтяжку к 3.3V
    // это предотвратит зависание буфера FIFO, если линия «молчит», хотя мы установили доп.внешнюю
    // поддтяжку R2.2kOm на MISO; MOSI; CLK!!!
    gpio_pull_up(SD_MISO_PIN);
    gpio_pull_up(SD_MOSI_PIN);
    gpio_pull_up(SD_SCK_PIN);

    // 4. Настройка пина CS (Строго как GPIO_OUT)
    //gpio_init(SD_CS_PIN);
    //gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    //sd_cs_deselect(); // По умолчанию деактивируем карту (CS = 1)
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
        spi_set_baudrate(SD_SPI_PORT, 1000 * 1000); // вместо 12500 * 1000
    }