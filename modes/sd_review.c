#include "sd_review.h"
#include "midi_uart.h"    // Отправка SysEx дампов в DX7 через DIN5
#include "sd_storage.h"   // Единый менеджер SD-карты
#include "ui_engine.h"    // Графика и цветовая палитра для TFT LCD
#include "ui_theme.h"
#include "TFT_dvr.h"
#include "ff.h"           // Заголовочный файл библиотеки FatFS
#include <stdio.h>
#include <string.h>
#include "hardware/timer.h"   // Для time_us_32()

void draw_text_scaled(uint16_t x, uint16_t y, const char* text, uint16_t color, uint16_t bg_color, int scale);
void ui_draw_footer(const char* footer_text);
// индекс выбранного файла в рамках интерфейса обзора
static int g_current_index = 0;

// Глобальный буфер для отслеживания текущей директории внутри режима просмотра
static char g_review_current_dir[256] = "/"; // С запасом под глубокие папки
static char g_next_path[256]; // Переименуем в g_next_path, чтобы она стала глобальной для файла
// Локальные переменные состояния меню
static int16_t current_file_idx = 0;   // Индекс выбранного файла на экране
static int16_t prev_file_idx = -1;     // Для отслеживания изменений и точечной перерисовки
static bool need_full_redraw = true;   // Флаг полной прорисовки экрана при входе


// ----------------------------------------------------------------------------
// Инициализация и сканирование каталога через sd_storage
// ----------------------------------------------------------------------------
bool sd_review_init(void) {
    printf("[SD] sd_review_init called\n");
    g_current_index = 0;

    if (!sd_info.is_mounted) {
        printf("SD not mounted, re-mounting...\n");
        if (!sd_storage_init()) {
            printf("[SD ERROR] Mount failed\n");
            return false;
        }
    } else {
        printf("[SD] Card mounted before!\n");
    }

    if (!sd_storage_scan_files("/")) {
        printf("[SD ERROR] Scan dir failed\n");
        return false;
    }

    sd_review_render();
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
    
    // ИСПРАВЛЕНО: Единое имя массива с запасом 256 байт под LFN длинные имена
    char full_file_path[256];
    memset(full_file_path, 0, sizeof(full_file_path));

    // Сборка полного пути к файлу
    if (strcmp(g_review_current_dir, "/") == 0) {
        snprintf(full_file_path, sizeof(full_file_path), "/%s", filename);
    } else {
        snprintf(full_file_path, sizeof(full_file_path), "%s/%s", g_review_current_dir, filename);
    }
    
    // Буфер чтения: 4096 байт под банк DX7
    static uint8_t sysex_buffer[4096]; 
    
    // Передаем проверенный full_file_path в менеджер чтения
    int32_t bytes_read = sd_storage_read_file(full_file_path, sysex_buffer, sizeof(sysex_buffer));
    
    if (bytes_read > 0) {
        printf("[SD] Sending %s (%ld bytes) to DX7...\n", full_file_path, bytes_read);
        midi_send_sysex(sysex_buffer, (uint16_t)bytes_read);
        return true;
    }

    printf("[SD ERROR] Read failed on full path: %s\n", full_file_path);
    return false;
}

// ----------------------------------------------------------------------------
// Логика обновления режима (вызывается из main.c) с обработкой Hot-Swap
// ----------------------------------------------------------------------------
void sd_review_update(uint16_t touched, int enc_delta) {
    static uint32_t last_enc_time = 0;
    static bool last_pressed_state = false; // Для отслеживания фронта нажатия кнопки
    uint32_t current_time = time_us_32();

    // 1. ОТДЕЛЬНЫЙ ДЕБАУНС ДЛЯ ВРАЩЕНИЯ ЭНКОДЕРА
    bool enc_moved = false;
    if (enc_delta != 0 && (current_time - last_enc_time >= 150000)) {
        last_enc_time = current_time;
        enc_moved = true;

        if (sd_info.file_count > 0) {
            g_current_index += enc_delta;
            
            // Круговое зацикливание списка файлов
            if (g_current_index < 0) {
                g_current_index = sd_info.file_count - 1;
            }
            if (g_current_index >= sd_info.file_count) {
                g_current_index = 0;
            }
        }
    }

    // 2. ОТДЕЛЬНАЯ ОБРАБОТКА НАЖАТИЯ (По фронту: нажали и отпустили)
    //touched может прийти как true от кнопки или как битовая маска от MPR121
    bool is_currently_pressed = (touched != 0);
    bool click_triggered = false;

    if (is_currently_pressed && !last_pressed_state) {
        // Клик зафиксирован строго в момент нажатия!
        click_triggered = true; 
    }
    last_pressed_state = is_currently_pressed; // Запоминаем состояние для следующего цикла

    // 3. ЛОГИКА ДЕЙСТВИЯ ПРИ КЛИКЕ
    if (click_triggered && sd_info.file_count > 0) {
        uint16_t idx = g_current_index;

        // Если выбрана ПАПКУ или пункт ".. [UP]"
        if (sd_info.files[idx].type == FILE_TYPE_FOLDER) {
            
            if (strcmp(sd_info.files[idx].name, ".. [UP]") == 0) {
                printf("[SD] Going UP to root directory\n");
                strcpy(g_review_current_dir, "/"); // Сбрасываем в корень
                sd_storage_scan_files(g_review_current_dir);
            } 
                // Если выбрана реальная папка
                else {
                    printf("[SD] Entering directory: %s\n", sd_info.files[idx].name);
                    
                    // Очищаем глобальный буфер перед склейкой для полной стерильности
                    memset(g_next_path, 0, sizeof(g_next_path));
                    
                    // Используем g_next_path вместо локальной next_path
                    snprintf(g_next_path, sizeof(g_next_path), "/%s", sd_info.files[idx].name);
                    
                    sd_storage_scan_files(g_next_path); 
                }
            g_current_index = 0; // Сброс стрелки на начало в новой папке
        } 
        // Если выбран обычный файл
        else {
            printf("[SD] Loading Sysex patch file: %s\n", sd_info.files[idx].name);
            sd_review_send_current_file(); 
        }
        
        // Принудительный рендер после клика
        sd_review_render();
    }
    // Если просто повернули энкодер — тоже обновляем экран
    else if (enc_moved) {
        sd_review_render();
    }
}

// ----------------------------------------------------------------------------
// Отрисовка интерфейса
// ----------------------------------------------------------------------------
void sd_review_render(void) {
    ui_draw_statusbar("FILE", sd_info.is_mounted, 1);
    ui_clear_work_area();

    if (sd_info.file_count == 0) {
        draw_text_scaled(10, 40, "EMPTY", current_theme.text_color, current_theme.bg_color, 1);
        ui_draw_footer("NO FILES");
        return;
    }

    // ВЫЧИСЛЯЕМ СМЕЩЕНИЕ ОКНА ПРОКРУТКИ (Показываем по 5 строк)
    int scroll_offset = 0;
    if (g_current_index >= 5) {
        scroll_offset = g_current_index - 4; // Окно начинает плавно ехать вниз за курсором
    }

    // Рисуем ровно 5 строк, которые попадают в видимую область экрана
    for (int i = 0; i < 5; i++) {
        int file_idx = scroll_offset + i;
        if (file_idx >= sd_info.file_count) break; // Если файлы кончились — выходим

        uint16_t y_pos = 35 + (i * 22); // Фиксированные 5 позиций на экране ST7789
        
        uint16_t t_color = (file_idx == g_current_index) ? current_theme.bg_color : current_theme.text_color;
        uint16_t b_color = (file_idx == g_current_index) ? current_theme.accent_color : current_theme.bg_color;

        // Подсветка выбранной строки
        if (file_idx == g_current_index) {
            clear_rect(0, y_pos - 3, TFT_WIDTH, 18, b_color);
        }

        // Выводим имя файла (благодаря LFN=2 строчные буквы теперь рисуются красиво!)
        draw_text_scaled(10, y_pos, sd_info.files[file_idx].name, t_color, b_color, 1);
    }

    ui_draw_footer("SELECT");
}
