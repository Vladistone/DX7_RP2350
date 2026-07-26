// OLD MAIN.C функции переезали в новые модули
// ============================================
// НАСТРОЙКИ И МАКРОСЫ ПЕРИФЕРИИ
// ============================================
#define BTN_SYS_MODE 23 // Программная кнопка (GP23)
#define LED_INIT_PIN 24 // LED_24
#define LED_LOOP_PIN 25 // LED_25

// Внешний массив шрифта (определен в font.c/TFT_driver.c)
extern const uint8_t font_8x12[95][12];

// ============================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ КНОПОК
// ============================================

// Проверка нажатия кнопки с обработкой дребезга
bool is_button_pressed(uint pin) {
    if (!gpio_get(pin)) {
        sleep_ms(30); // Задержка на антидребезг
        if (!gpio_get(pin)) {
            return true;
        }
    }
    return false;
}

// Ожидание отпускания кнопки
void wait_button_release(uint pin) {
    while (!gpio_get(pin)) {
        sleep_ms(10);
    }
    sleep_ms(30); // Задержка от дребезга при отпускании
}

// Переменные для состояния кнопок и энкодера
uint16_t last_touch = 0;
uint8_t last_enc_a = 1;
uint32_t last_scroll_time = 0; // Для контроля скорости перемотки FF/RW

// ... (внутри main.c после инициализации) ...

while (true) {
    uint32_t current_time = time_us_32() / 1000; // Текущее время в мс

    // ========================================================
    // 1. ОПРОС MPR121 (СЕНСОРНЫЕ КНОПКИ)
    // ========================================================
    uint16_t touched = mpr121_read_touched();
    
    // Битовые маски:
    // pressed  - кнопка была только что нажата (Rising Edge)
    // released - кнопка была только что отпущена (Falling Edge)
    uint16_t pressed = touched & ~last_touch;
    uint16_t released = ~touched & last_touch;

    // --- А. ОБРАБОТКА ОДИНОЧНЫХ НАЖАТИЙ (LATCHING / TRIGGERS) ---
    if (pressed) {
        if (pressed & (1 << MPR_PLAY)) {
            current_state.is_playing = true;
            draw_transport_controls(current_state.is_playing);
            // Запуск MIDI-плеера
        }
        
        if (pressed & (1 << MPR_STOP)) {
            current_state.is_playing = false;
            draw_transport_controls(current_state.is_playing);
            // Остановка MIDI, отправка All Notes Off (CC 123)
        }
        
        if (pressed & (1 << MPR_DISK_UP)) {
            // Следующий банк/диск
            current_state.current_bank++;
            render_playback_screen();
        }
        
        if (pressed & (1 << MPR_MODE)) {
            // Переключение режимов (Playback -> File Select и т.д.)
            current_mode = (current_mode == APP_MODE_PLAYBACK) ? APP_MODE_FILE_SELECT : APP_MODE_PLAYBACK;
            // Перерисовка экрана...
        }
        
        // ... (аналогично для CUR_UP, CUR_DN, ENTER, ESC) ...
    }

    // --- Б. ОБРАБОТКА УДЕРЖАНИЯ (NON-LATCHING: FF / RW) ---
    // Выполняем перемотку только если прошло достаточно времени (например, шаг 50 мс)
    if ((current_time - last_scroll_time) > 50) { 
        if (touched & (1 << MPR_FF)) {
            // Кнопка FF удерживается
            // Сдвигаем указатель MIDI-файла вперед
            // Если нужно, ставим плеер на паузу на время перемотки
            last_scroll_time = current_time;
        } 
        else if (touched & (1 << MPR_RW)) {
            // Кнопка RW удерживается
            // Сдвигаем указатель MIDI-файла назад
            last_scroll_time = current_time;
        }
    }

    // Обработка отпускания FF/RW (если нужно восстановить Play)
    if (released & ((1 << MPR_FF) | (1 << MPR_RW))) {
        // Указатель встал. Можно принудительно обновить UI.
    }

    last_touch = touched;

    // ========================================================
    // 2. ОПРОС ЭНКОДЕРА KY-040 (ДЛЯ СКРОЛЛИНГА ПАТЧЕЙ)
    // ========================================================
    uint8_t enc_a = gpio_get(ENC_PIN_A);
    uint8_t enc_b = gpio_get(ENC_PIN_B);

    // Ловим изменение состояния пина A (вращение)
    if (enc_a != last_enc_a) {
        if (enc_a == 0) { // Срабатывание по спаду
            if (enc_b == 1) {
                // Вращение ВПРАВО (Increment)
                if (current_state.current_patch < 127) {
                    current_state.current_patch++;
                }
            } else {
                // Вращение ВЛЕВО (Decrement)
                if (current_state.current_patch > 0) {
                    current_state.current_patch--;
                }
            }
            // Отправляем Program Change в DX7 и обновляем экран
            midi_send_program_change(0, current_state.current_patch);
            render_playback_screen(); 
        }
        last_enc_a = enc_a;
    }

    // ========================================================
    // 3. СИСТЕМНЫЕ ЗАДЕРЖКИ
    // ========================================================
    sleep_ms(2); // Небольшая задержка, чтобы не вешать процессор, 
                 // сохраняя высокую скорость реакции энкодера
}

// ============================================
// ВЫСОКОУРОВНЕВЫЕ ФУНКЦИИ АНИМАЦИИ И UI
// ============================================

void draw_rotating_7(uint16_t center_x, uint16_t center_y, float angle, uint16_t color, uint16_t bg_color, int scale) {
    int idx = '7' - 32;
    if (idx < 0 || idx > 94) return;
    
    int pixel_size = scale * 2;
    int gap = scale / 2 + 1;
    int step = pixel_size + gap;
    
    int clear_size = (8 * step) / 2 + 15;
    clear_rect(center_x - clear_size, center_y - clear_size, clear_size * 2, clear_size * 2, bg_color);
    
    for (int row = 0; row < 12; row++) {
        uint8_t row_data = font_8x12[idx][row];
        for (int col = 0; col < 8; col++) {
            if (row_data & (0x80 >> col)) {
                float dx = (col - 4.0f) * step;
                float dy = (row - 6.0f) * step;
                
                float rx = dx * cosf(angle) - dy * sinf(angle);
                float ry = dx * sinf(angle) + dy * cosf(angle);
                
                int sx = center_x + (int)rx - pixel_size/2;
                int sy = center_y + (int)ry - pixel_size/2;
                
                // Границы берутся динамически из TFT_WIDTH и TFT_HEIGHT драйвера
                if (sx >= 0 && sx + pixel_size <= TFT_WIDTH && 
                    sy >= 0 && sy + pixel_size <= TFT_HEIGHT) {
                    
                    clear_rect(sx, sy, pixel_size, pixel_size, color); 
                }
            }
        }
    }
}

void show_animated_splash() {
    fill_screen(0x0000);
    
    uint16_t offset_y = 0; 
    uint16_t center_x = TFT_WIDTH / 2;
    uint16_t center_y = offset_y + (TFT_HEIGHT - offset_y) / 2;
    
    uint16_t color_cyan = 0x07FF;
    uint16_t color_white = 0xFFFF;
    uint16_t color_black = 0x0000;
    
    int rotate_scale = 6;
    
    for (int frame = 0; frame < 24; frame++) {
        float angle = frame * (2 * 3.14159f / 24);
        draw_rotating_7(center_x, center_y, angle, color_cyan, color_black, rotate_scale);
        sleep_ms(30);
    }
    
    fill_screen(0x0000);
    
    int yamaha_scale = 3;
    int spacing = 3;
    int char_width = 8 * yamaha_scale + spacing;
    int text_width = 6 * char_width;
    
    uint16_t yamaha_x = (TFT_WIDTH - text_width) / 2;
    uint16_t yamaha_y = offset_y + 30;
    
    for (int step = 0; step < 10; step++) {
        uint16_t brightness = (step + 1) * 2;
        uint16_t color = (brightness << 11) | (brightness << 6) | brightness;
        draw_text_scaled(yamaha_x, yamaha_y, "YAMAHA", color, color_black, yamaha_scale);
        sleep_ms(50);
    }
    draw_text_scaled(yamaha_x, yamaha_y, "YAMAHA", color_white, color_black, yamaha_scale);
    
    sleep_ms(200);
    
    int dx_scale = 5;
    int dx_spacing = 4;
    int dx_char_width = 8 * dx_scale + dx_spacing;
    
    uint16_t dx_x = (TFT_WIDTH - (2 * dx_char_width + 1 * dx_char_width)) / 2;
    uint16_t dx_y = yamaha_y + 12 * yamaha_scale + 20;
    
    for (int step = 0; step < 10; step++) {
        uint16_t brightness = (step + 1) * 2;
        uint16_t color = (brightness << 11) | (brightness << 6) | brightness;
        draw_text_scaled(dx_x, dx_y, "DX", color, color_black, dx_scale);
        sleep_ms(50);
    }
    draw_text_scaled(dx_x, dx_y, "DX", color_white, color_black, dx_scale);
    
    sleep_ms(300);
    
    int seven_scale = dx_scale + 3;
    uint16_t seven_x = dx_x + 2 * dx_char_width + 16;
    uint16_t seven_y = dx_y - (16 * (seven_scale - dx_scale) / 2);
    
    for (int step = 0; step < 8; step++) {
        uint16_t brightness = (step + 1) * 2;
        uint16_t color = (brightness << 11) | (brightness << 6) | brightness;
        uint16_t seven_color = ((color & 0xF800) >> 1) | ((color & 0x07E0) >> 1) | ((color & 0x001F) >> 1);
        draw_char_scaled(seven_x, seven_y, '7', seven_color, color_black, seven_scale);
        sleep_ms(50);
    }
    draw_char_scaled(seven_x, seven_y, '7', color_cyan, color_black, seven_scale);
    
    for (int pulse = 0; pulse < 3; pulse++) {
        sleep_ms(300);
        uint16_t seven_color = (pulse % 2 == 0) ? color_cyan : 0x07E0;
        draw_char_scaled(seven_x, seven_y, '7', seven_color, color_black, seven_scale);
    }
    
    sleep_ms(1000);
}

// ============================================
// РЕЖИМ КОНФИГУРАЦИИ (SYSTEM MODE)
// ============================================

void run_system_config_mode() {
    // 1. Ожидаем отпускания кнопки, если зашли при старте
    wait_button_release(BTN_SYS_MODE);

    // 2. Сигнализация на дисплее
    fill_screen(0x001F); // Синий фон
    draw_text_scaled(15, 30, "SYSTEM MODE", 0xFFFF, 0x001F, 2);
    draw_text_scaled(15, 80, "PRESS GP23", 0xFFE0, 0x001F, 2);
    draw_text_scaled(15, 110, "TO EXIT", 0xFFE0, 0x001F, 2);

    // Начальное состояние диодов для спец-режима
    bool led_state = false;

    // 3. Главный цикл режима конфигурации
    while (true) {
        // Поочередное переключение LED_24 и LED_25
        gpio_put(LED_INIT_PIN, led_state);
        gpio_put(LED_LOOP_PIN, !led_state);
        led_state = !led_state;

        // Проверка нажатия кнопки GP23 для ВЫХОДА
        if (is_button_pressed(BTN_SYS_MODE)) {
            wait_button_release(BTN_SYS_MODE);
            break; 
        }

        sleep_ms(200); 
    }

    // 4. Восстановление состояния при выходе
    gpio_put(LED_INIT_PIN, 1); // Возвращаем LED_24 в состояние "Горит постоянным"
    gpio_put(LED_LOOP_PIN, 0); // Гасим LED_25 перед возвратом в main loop
    fill_screen(0x0000);       // Очищаем экран
}

// MIDI/UART TX/RX
void init_midi_uart() {
    uart_init(MIDI_UART_ID, MIDI_BAUD_RATE);
    gpio_set_function(MIDI_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(MIDI_RX_PIN, GPIO_FUNC_UART); 
    
    // Опционально: отключаем аппаратный контроль потока
    uart_set_hw_flow(MIDI_UART_ID, false, false);
    // Формат MIDI: 8 бит данных, 1 стоп-бит, без контроля четности
    uart_set_format(MIDI_UART_ID, 8, 1, UART_PARITY_NONE);
}

// touchpad_controler MPR121 12 knobes:
void init_mpr121() {
    i2c_init(I2C_PORT, 400 * 1000); // I2C Fast Mode (400 kHz)
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Сброс и переход в режим Stop для конфигурации
    uint8_t stop_cmd[] = {0x5E, 0x00};
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, stop_cmd, 2, false);

    // Включение всех 12 электродов
    uint8_t run_cmd[] = {0x5E, 0x8F};
    i2c_write_blocking(I2C_PORT, MPR121_ADDR, run_cmd, 2, false);
}

// CD-card
void init_sd_spi() {
    // Начальная скорость для SD-карт (400 kHz для инициализации, затем повышается)
    spi_init(SD_SPI_PORT, 400 * 1000);
    
    gpio_set_function(SD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_MISO_PIN, GPIO_FUNC_SPI);
    
    // CS пин управляется программно
    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    gpio_put(SD_CS_PIN, 1); // Подтяжка к питанию (выключен)
}

// Энкодер модуль KY-040
void init_encoder() {
    gpio_init(ENC_PIN_A);
    gpio_set_dir(ENC_PIN_A, GPIO_IN);
    gpio_pull_up(ENC_PIN_A);

    gpio_init(ENC_PIN_B);
    gpio_set_dir(ENC_PIN_B, GPIO_IN);
    gpio_pull_up(ENC_PIN_B);

    gpio_init(ENC_PIN_SW);
    gpio_set_dir(ENC_PIN_SW, GPIO_IN);
    gpio_pull_up(ENC_PIN_SW);
}

// ============================================
// ТОЧКА ВХОДА
// ============================================

int main() {
    stdio_init_all();
    
    // 1. Инициализация LED
    gpio_init(LED_INIT_PIN);
    gpio_set_dir(LED_INIT_PIN, GPIO_OUT);
    
    gpio_init(LED_LOOP_PIN);
    gpio_set_dir(LED_LOOP_PIN, GPIO_OUT);
    
    // 2. Индикация загрузки: мигаем LED_24 при старте
    for (int i = 0; i < 5; i++) {
        gpio_put(LED_INIT_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_INIT_PIN, 0);
        sleep_ms(100);
    }
    gpio_put(LED_INIT_PIN, 1); 

    // 3. Инициализация дисплея через функции драйвера
    tft_init_interface();
    
    // 4. Инициализация кнопки GP23
    gpio_init(BTN_SYS_MODE);
    gpio_set_dir(BTN_SYS_MODE, GPIO_IN);
    gpio_pull_up(BTN_SYS_MODE);
    
    // Вход в System Mode при старте (если кнопка зажата при подаче питания)
    if (is_button_pressed(BTN_SYS_MODE)) {
        run_system_config_mode();
    }

    // 5. Запуск сплэш-скрина
    show_animated_splash(); 

    // 6. Главный цикл программы
    while (true) {
        // Проверка нажатия GP23 во время работы
        if (is_button_pressed(BTN_SYS_MODE)) {
            run_system_config_mode();
        }

        uint32_t current_time = time_us_32() / 1000; // Текущее время в мс

        // ========================================================
        // 1. ОПРОС MPR121 (СЕНСОРНЫЕ КНОПКИ)
        // ========================================================
        uint16_t touched = mpr121_read_touched();
        
        // Битовые маски:
        // pressed  - кнопка была только что нажата (Rising Edge)
        // released - кнопка была только что отпущена (Falling Edge)
        uint16_t pressed = touched & ~last_touch;
        uint16_t released = ~touched & last_touch;

        // --- А. ОБРАБОТКА ОДИНОЧНЫХ НАЖАТИЙ (LATCHING / TRIGGERS) ---
        if (pressed) {
            if (pressed & (1 << MPR_PLAY)) {
                current_state.is_playing = true;
                draw_transport_controls(current_state.is_playing);
                // Запуск MIDI-плеера
            }
            
            if (pressed & (1 << MPR_STOP)) {
                current_state.is_playing = false;
                draw_transport_controls(current_state.is_playing);
                // Остановка MIDI, отправка All Notes Off (CC 123)
            }
            
            if (pressed & (1 << MPR_DISK_UP)) {
                // Следующий банк/диск
                current_state.current_bank++;
                render_playback_screen();
            }
            
            if (pressed & (1 << MPR_MODE)) {
                // Переключение режимов (Playback -> File Select и т.д.)
                current_mode = (current_mode == APP_MODE_PLAYBACK) ? APP_MODE_FILE_SELECT : APP_MODE_PLAYBACK;
                // Перерисовка экрана...
            }
            
            // ... (аналогично для CUR_UP, CUR_DN, ENTER, ESC) ...
        }

        // --- Б. ОБРАБОТКА УДЕРЖАНИЯ (NON-LATCHING: FF / RW) ---
        // Выполняем перемотку только если прошло достаточно времени (например, шаг 50 мс)
        if ((current_time - last_scroll_time) > 50) { 
            if (touched & (1 << MPR_FF)) {
                // Кнопка FF удерживается
                // Сдвигаем указатель MIDI-файла вперед
                // Если нужно, ставим плеер на паузу на время перемотки
                last_scroll_time = current_time;
            } 
            else if (touched & (1 << MPR_RW)) {
                // Кнопка RW удерживается
                // Сдвигаем указатель MIDI-файла назад
                last_scroll_time = current_time;
            }
        }

        // Обработка отпускания FF/RW (если нужно восстановить Play)
        if (released & ((1 << MPR_FF) | (1 << MPR_RW))) {
            // Указатель встал. Можно принудительно обновить UI.
        }

        last_touch = touched;

        // ========================================================
        // 2. ОПРОС ЭНКОДЕРА KY-040 (ДЛЯ СКРОЛЛИНГА ПАТЧЕЙ)
        // ========================================================
        uint8_t enc_a = gpio_get(ENC_PIN_A);
        uint8_t enc_b = gpio_get(ENC_PIN_B);

        // Ловим изменение состояния пина A (вращение)
        if (enc_a != last_enc_a) {
            if (enc_a == 0) { // Срабатывание по спаду
                if (enc_b == 1) {
                    // Вращение ВПРАВО (Increment)
                    if (current_state.current_patch < 127) {
                        current_state.current_patch++;
                    }
                } else {
                    // Вращение ВЛЕВО (Decrement)
                    if (current_state.current_patch > 0) {
                        current_state.current_patch--;
                    }
                }
                // Отправляем Program Change в DX7 и обновляем экран
                midi_send_program_change(0, current_state.current_patch);
                render_playback_screen(); 
            }
            last_enc_a = enc_a;
        }

        // ========================================================
        // 3. СИСТЕМНЫЕ ЗАДЕРЖКИ
        // ========================================================
        sleep_ms(2); // Небольшая задержка, чтобы не вешать процессор, сохраняя высокую скорость реакции энкодера

        // Обычная индикация главного цикла (ритмичное мигание LED_25)
        gpio_put(LED_LOOP_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_LOOP_PIN, 0);
        sleep_ms(500);
    }
}