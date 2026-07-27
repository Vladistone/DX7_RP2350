#include "sd_review.h"
#include "midi_uart.h"    // Отправка SysEx дампов в DX7 через DIN5
#include "sd_storage.h"   // Единый менеджер SD-карты
#include "ui_engine.h"    // Графика и цветовая палитра для TFT LCD
#include "ff.h"           // Заголовочный файл библиотеки FatFS
#include <stdio.h>
#include <string.h>
#include "hardware/timer.h"   // Для time_us_32()
void draw_text_scaled(uint16_t x, uint16_t y, const char* text, uint16_t color, uint16_t bg_color, int scale);

// Локальный индекс выбранного файла в рамках интерфейса обзора
static int g_current_index = 0;

// ----------------------------------------------------------------------------
// Инициализация и сканирование каталога через sd_storage
// ----------------------------------------------------------------------------
bool sd_review_init(void) {
    printf("[SD] sd_review_init called\n");
    g_current_index = 0;

    if (!sd_ensure_ready()) { // проверяем мсмонтирован ли диск
        printf("[SD ERROR] Mount failed\n");
        return false;
    }
    printf("[SD] Card mounted before!\n");

    // 2. Сканируем корень с помощью безопасной функции из sd_storage
        // Попробуй изменить путь на "0:/" или "/"
    if (!sd_storage_scan_files("/")) {
        printf("[SD ERROR] Scan dir failed\n");
        return false;
    }

    printf("[SD] Scan complete. Found files/folders: %d\n", sd_info.file_count);
    //return true;

    printf("[SD] Found %d items in root\n", sd_info.file_count);
    for (int i = 0; i < sd_info.file_count; i++) {
    printf("  %d: %s\n", i, sd_info.files[i].name);
    }

    return true;
}

// ----------------------------------------------------------------------------
// Чтение файла с SD и отправка в MIDI UART (DX7) с защитой
// ----------------------------------------------------------------------------
bool sd_review_send_current_file(void) {
    if (!sd_info.is_mounted || sd_info.file_count == 0) return false;

    // Защита индекса
    if (g_current_index < 0 || g_current_index >= sd_info.file_count) {
        return false;
    }

    // Пропускаем папки, отправляем только файлы
    if (sd_info.files[g_current_index].type != FILE_TYPE_MIDI_SYSEX) {
        printf("[SD] Selected item is not a SysEx file.\n");
        return false;
    }

    const char *filename = sd_info.files[g_current_index].name;
    
    // Буфер чтения: 4096 байт под банк DX7
    static uint8_t sysex_buffer[4096]; 
    
    // Используем безопасное чтение через sd_storage
    int32_t bytes_read = sd_storage_read_file(filename, sysex_buffer, sizeof(sysex_buffer));
    
    if (bytes_read > 0) {
        printf("[SD] Sending %s (%ld bytes) to DX7...\n", filename, bytes_read);
        midi_send_sysex(sysex_buffer, (uint16_t)bytes_read);
        return true;
    }

    printf("[SD ERROR] Read failed on file %s\n", filename);
    return false;
}

// ----------------------------------------------------------------------------
// Логика обновления режима (вызывается из main.c) с обработкой Hot-Swap
// ----------------------------------------------------------------------------
void sd_review_update(uint16_t touched, int enc_delta) {
    // Небольшая задержка для стабилизации после переключения режима
    static uint32_t last_switch_time = 0;
    if (time_us_32() - last_switch_time < 500000) { // 500 мс
        return;
    }
    last_switch_time = time_us_32();

    // 1. Проверяем безопасность и готовность карты (Hot-swap check)
    if (!sd_ensure_ready()) {
        return;
    }

    // Если файлов нет после переподключения — выходим
    if (sd_info.file_count == 0) {
        return;
    }

    // 2. Навигация энкодером, если карта готова
    if (enc_delta != 0) {
        g_current_index += enc_delta;

        // Защита от выхода за границы массива
        if (g_current_index < 0) {
            g_current_index = 0;
        } else if (g_current_index >= sd_info.file_count) {
            g_current_index = sd_info.file_count - 1;
        }

        printf("[SD Browser] Selected: [%d/%d] %s\n", 
               g_current_index + 1, sd_info.file_count, sd_info.files[g_current_index].name);
        
        sd_review_render();
    }
}

// ----------------------------------------------------------------------------
// Отрисовка интерфейса
// ----------------------------------------------------------------------------
void sd_review_render(void) {
    if (!sd_info.is_mounted) {
        draw_text_scaled(10, 50, "NO SD CARD", 0xF800, current_theme.bg_color, 2);
        return;
    }
    if (sd_info.file_count == 0) {
        draw_text_scaled(10, 50, "NO FILES FOUND", 0xF800, current_theme.bg_color, 2);
        return;
    }
    // Отобразить список файлов
    for (int i = 0; i < sd_info.file_count && i < 8; i++) {
        uint16_t color = (i == g_current_index) ? current_theme.accent_color : current_theme.text_color;
        draw_text_scaled(10, 60 + i * 24, sd_info.files[i].name, color, current_theme.bg_color, 1);
    }
}