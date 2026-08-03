#include "sd_review.h"
#include "sd_storage.h"
#include "ui_engine.h"
#include "encoder_dvr.h"
#include "TFT_dvr.h"       // КРИТИЧНО: добавляем для макроса TFT_WIDTH и функций экрана
#include "pico/stdlib.h"   // КРИТИЧНО: добавляем для функции времени time_us_32()
#include "midi_uart.h"
#include <stdio.h>
#include <string.h>

static int g_current_index = 0;
static char current_browser_path[128] = "/"; 
static uint8_t g_current_page = 0; 
static bool g_force_redraw = false; 

// ПЕРЕКЛЮЧАТЕЛЬ ЛОГОВ SD_REVIEW: 
// 1 = Логи с таймштампами активны
// 0 = Логи полностью вырезаются компилятором (Zero Overhead)
/*
    #define DEBUG_SD_REVIEW_ENABLE  1

    #if DEBUG_SD_REVIEW_ENABLE
        // Безопасный неблокирующий вывод с точным таймштампом от старта (секунды.микросекунды)
        #define SD_LOG(fmt, ...) \
            do { \
                if (tud_cdc_connected() && tud_cdc_write_available() >= 64) { \
                    uint64_t _us = time_us_64(); \
                    uint32_t _sec = (uint32_t)(_us / 1000000ULL); \
                    uint32_t _rem_us = (uint32_t)(_us % 1000000ULL); \
                    printf("[%04u.%06u][SD_REV] " fmt "\n", _sec, _rem_us, ##__VA_ARGS__); \
                } \
            } while (0)
    #else
        #define SD_LOG(fmt, ...) do {} while(0)
    #endif
*/

bool sd_review_init(void) {
    printf("[SD] sd_review_init called\n");
    g_current_index = 0;
    g_current_page = 0; 
    
    snprintf(current_browser_path, sizeof(current_browser_path), "/");

    if (!sd_info.is_mounted) {
        if (!sd_storage_init()) return false;
    }

    sd_storage_scan_files_page("/", 0);

    g_force_redraw = true; 
    return true;
}

void sd_review_render(void) {
    char header_string[32]; 
    
    // ЖЕЛЕЗОБЕТОННАЯ ПРОВЕРКА ВЛОЖЕННОСТИ
    bool is_subfolder = (strcmp(current_browser_path, "/") != 0);

    if (!is_subfolder) {
        snprintf(header_string, sizeof(header_string), "DIR: /");
    } else {
        // Убираем "DIR: ", пишем сразу имя папки, чтобы влезло больше текста до маркеров!
        snprintf(header_string, sizeof(header_string), "%.16s", current_browser_path + 1);
    }

    ui_draw_statusbar(header_string, sd_info.is_mounted, 1);
    ui_clear_work_area();

    // Если файлов считано ровно 32 (лимит массива), значит на флешке есть ещё — резервируем строку DOWN
    bool show_next_button = (sd_info.file_count >= SD_MAX_FILES); 
    
    //display_count состоит СТРОГО из реальных файлов + навигация
    int display_count = sd_info.file_count;
    if (is_subfolder) display_count++;       // +1 строка для [UP]
    if (show_next_button) display_count++;   // +1 строка для [DWN]

    if (display_count == 0) {
        draw_text_scaled(10, 40, "EMPTY", current_theme.text_color, current_theme.bg_color, 1);
        ui_draw_footer("NO FILES");
        return;
    }

    int start_view_idx = 0;
    if (g_current_index >= 5) {
        start_view_idx = g_current_index - 4;
    }

    for (int i = 0; i < 5 && (start_view_idx + i) < display_count; i++) {
        int current_item_idx = start_view_idx + i;
        uint16_t y_pos = 35 + (i * 20);
        
        uint16_t t_color = (current_item_idx == g_current_index) ? current_theme.bg_color : current_theme.text_color;
        uint16_t b_color = (current_item_idx == g_current_index) ? current_theme.accent_color : current_theme.bg_color;

        if (current_item_idx == g_current_index) {
            clear_rect(0, y_pos - 2, TFT_WIDTH, 18, b_color);
        }

        char display_string[64]; 

        // 1. Отрисовка кнопки Наверх (всегда строго индекс 0 в подпапке)
        if (is_subfolder && current_item_idx == 0) {
            snprintf(display_string, sizeof(display_string), "00. .. [UP]");
            draw_text_scaled(10, y_pos, display_string, t_color, b_color, 1);
        } 
        // 2. Отрисовка кнопки Следующая страница (всегда самый последний индекс списка)
        else if (show_next_button && current_item_idx == (display_count - 1)) {
            snprintf(display_string, sizeof(display_string), ".. [DWN] NEXT PAGE >>");
            draw_text_scaled(10, y_pos, display_string, t_color, b_color, 1);
        }
        // 3. Отрисовка реальных файлов пресетов
        else {
            int file_idx = is_subfolder ? (current_item_idx - 1) : current_item_idx;
            
            // Защита от выхода за границы заполненного массива FatFS
            if (file_idx >= 0 && file_idx < sd_info.file_count) {
                int file_number = (g_current_page * 30) + file_idx + 1; 
                snprintf(display_string, sizeof(display_string), "%02d. %s", file_number, sd_info.files[file_idx].name);
                draw_text_scaled(10, y_pos, display_string, t_color, b_color, 1);
            }
        }
    }

    ui_draw_footer("SELECT");
}

void sd_review_update(uint16_t touched, int enc_delta) {
    static uint32_t last_action_time = 0;
    uint32_t current_time = time_us_32();

    if (g_force_redraw) {
        g_force_redraw = false;
        sd_review_render();
        return;
    }

    if (current_time - last_action_time < 150000) return;

    bool is_subfolder = (strcmp(current_browser_path, "/") != 0);
    bool show_next_button = (sd_info.file_count >= SD_MAX_FILES);
    int display_count = sd_info.file_count + (is_subfolder ? 1 : 0) + (show_next_button ? 1 : 0);

    if (enc_delta != 0 || touched) {
        last_action_time = current_time;

        if (display_count > 0 && enc_delta != 0) {
            g_current_index += enc_delta;
            if (g_current_index < 0) g_current_index = display_count - 1;
            if (g_current_index >= display_count) g_current_index = 0;
        }

        if (touched) {
            if (g_current_index >= 0 && g_current_index < display_count) {
                
                // НАЖАТИЕ НА НАВЕРХ
                if (is_subfolder && g_current_index == 0) {
                    printf("[SD UI] Reset path to root\n");
                    snprintf(current_browser_path, sizeof(current_browser_path), "/");
                    g_current_page = 0; 
                    
                    ui_clear_work_area(); 
                    sd_storage_scan_files_page(current_browser_path, 0); 
                    
                    g_current_index = 0;
                    g_force_redraw = true; 
                    return; 
                } 
                // НАЖАТИЕ НА СЛЕДУЮЩУЮ СТРАНИЦУ [DWN]
                else if (show_next_button && g_current_index == (display_count - 1)) {
                    g_current_page++; 
                    printf("[SD UI] Loading next page: %d\n", g_current_page);
                    
                    ui_clear_work_area();
                    sd_storage_scan_files_page(current_browser_path, g_current_page);
                    
                    g_current_index = is_subfolder ? 1 : 0; 
                    g_force_redraw = true;
                    return;
                }
                // НАЖАТИЕ НА ЭЛЕМЕНТ
                else {
                    int file_idx = is_subfolder ? (g_current_index - 1) : g_current_index;

                    if (file_idx >= 0 && file_idx < sd_info.file_count) {
                        if (sd_info.files[file_idx].type == FILE_TYPE_FOLDER) {
                            snprintf(current_browser_path, sizeof(current_browser_path), "/%s", sd_info.files[file_idx].name);
                            g_current_page = 0; 

                            ui_clear_work_area(); 
                            sd_storage_scan_files_page(current_browser_path, 0);
                            g_current_index = 0;
                            g_force_redraw = true; 
                            return; 
                        } 
                        else {
                            // === ИНТЕГРАЦИЯ DOS ПОПАПА "PLEASE WAIT" ===
                            // === АВТОМАТИЧЕСКИЙ РАСЧЕТ ЦЕНТРА ЧЕРЕЗ МАКРОСЫ ПРОЕКТА ===
                            // === ЧЕРНЫЙ DOS ПОПАП С БИРЮЗОВОЙ РАМКОЙ ===
                            uint16_t popup_w = (TFT_WIDTH * 7) / 10;
                            uint16_t popup_h = (TFT_HEIGHT * 3) / 10;
                            
                            uint16_t popup_x = (TFT_WIDTH - popup_w) / 2;
                            uint16_t popup_y = (TFT_HEIGHT - popup_h) / 2;

                            // 1. Подложка окна: ЖЕСТКИЙ ХАРДКОД 0x0000 (Чисто черный фон)
                            clear_rect(popup_x, popup_y, popup_w, popup_h, 0x0000); 
                            // 2. Рамка окна: Бирюзовый цвет темы акцента
                            draw_rect(popup_x, popup_y, popup_w, popup_h, current_theme.bg_color);  
                            
                            // 3. Контрастный текст на черном фоне (bg_color передаем как 0x0000)
                            draw_text_scaled(popup_x + 30, popup_y + 14, "MIDI SENDING...", current_theme.bar_text_color, 0x0000, 1);
                            draw_text_scaled(popup_x + 36, popup_y + 32, "Please wait...", current_theme.accent_color, 0x0000, 1);
                            
                            // Даем аппаратному SPI дисплею гарантированно обновить пиксели
                            sleep_ms(50); 
                            
                            sd_review_send_current_file();
                            g_force_redraw = true; 
                            return;
                        }
                    }
                }
            }
            touched = false; 
        }
        sd_review_render();
    }
}

bool sd_review_send_current_file(void) {
    if (!sd_info.is_mounted || sd_info.file_count == 0) return false;
    if (g_current_index < 0 || g_current_index >= sd_info.file_count) return false;

    const char *filename = sd_info.files[g_current_index].name;
    static uint8_t sysex_buffer[4096]; 
    
    int32_t bytes_read = sd_storage_read_file(filename, sysex_buffer, sizeof(sysex_buffer));
    if (bytes_read > 0) {
        printf("[SD] Sending %s to DX7...\n", filename);
        midi_send_sysex(sysex_buffer, (uint16_t)bytes_read);
        return true;
    }
    printf("[SD ERROR] Read failed on file %s\n", filename);
    return false;
}
