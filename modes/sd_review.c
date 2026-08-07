#include "hw_config.h"     // Макрос SD_LOG и прототипы хронографа debug_chrono_...
#include "sd_review.h"
#include "sd_storage.h"
#include "ui_engine.h"     // Абстрактный графический движок на "новых рельсах"
#include "pico/stdlib.h"   // Системное время time_us_32()
#include "midi_uart.h"     // Отправка SysEx через midi_send_sysex
#include <stdio.h>
#include <string.h>

// ====================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ СОСТОЯНИЯ БРАУЗЕРА (СТРОГО СОХРАНЯЕМ ИЗ РЕПОЗИТОРИЯ)
// ====================================================================
static int g_current_index = 0;
static char current_browser_path[128] = "/"; 
static uint8_t g_current_page = 0; 
static bool g_force_redraw = false; 

// ====================================================================
// 1. ИНИЦИАЛИЗАЦИЯ РЕЖИМА БРАУЗЕРА (void)
// ====================================================================
void sd_review_init(void) {
    SD_LOG("sd_review_init called");
    g_current_index = 0;
    g_current_page = 0; 
    
    snprintf(current_browser_path, sizeof(current_browser_path), "/");

    if (!sd_info.is_mounted) {
        if (!sd_storage_init()) {
            SD_LOG("[SD ERROR] Initialization failed!");
            return; 
        }
    }

    debug_chrono_sd_op("SCAN_DIR...", current_browser_path);
    sd_storage_scan_files_page("/", 0);

    g_force_redraw = true; 
}

// ====================================================================
// 2. ОТПРАВКА СИСТЕКСТА В СИНТЕЗАТОР DX7 (void)
// ====================================================================
bool sd_review_send_current_file(void) {
    if (!sd_info.is_mounted || sd_info.file_count == 0) return false;
    if (g_current_index < 0 || g_current_index >= sd_info.file_count) return false;

    const char *filename = sd_info.files[g_current_index].name;
    static uint8_t sysex_buffer[4096]; 
    
    int32_t bytes_read = sd_storage_read_file(filename, sysex_buffer, sizeof(sysex_buffer));
    if (bytes_read > 0) {
        SD_LOG("[SD] Sending %s to DX7...\n", filename);
        midi_send_sysex(sysex_buffer, (uint16_t)bytes_read);
        return true;
    }
    SD_LOG("[SD ERROR] Read failed on file %s\n", filename);
    return false;
}
// ====================================================================
// 3. КОНТЕНТ-РЕНДЕРЕР СПИСКА ФАЙЛОВ (СТРОГО ПО ВАШЕМУ SD_STORAGE.H)
// ====================================================================
static void sd_review_content_draw(void) {
    // Жесткая проверка вложенности по вашему первоисточнику
    bool is_subfolder = (strcmp(current_browser_path, "/") != 0);
    
    // Проверяем, достигнут ли лимит пачки, чтобы показать кнопку DWN
    bool show_next_button = (sd_info.file_count >= SD_MAX_FILES); 
    
    int display_count = sd_info.file_count;
    if (is_subfolder) display_count++;       // +1 строка для [UP]
    if (show_next_button) display_count++;   // +1 строка для [DWN]

    if (display_count == 0) {
        ui_draw_text_rel(10, 20, "EMPTY", current_theme.text_color, 2);
        return; 
    }

    int start_view_idx = 0;
    if (g_current_index >= 5) {
        start_view_idx = g_current_index - 4;
    }

    for (int i = 0; i < 5 && (start_view_idx + i) < display_count; i++) {
        int current_item_idx = start_view_idx + i;
        uint16_t rel_y = 10 + (i * 20); 
        
        uint16_t t_color = (current_item_idx == g_current_index) ? current_theme.bg_color : current_theme.text_color;
        uint16_t b_color = (current_item_idx == g_current_index) ? current_theme.accent_color : current_theme.bg_color;

        if (current_item_idx == g_current_index) {
            ui_draw_card_rel(0, rel_y - 2, UI_WORK_WIDTH, 18, b_color, b_color);
        }

        char display_string[64]; // Восстановлен локальный буфер строки кадра!

        // 1. Отрисовка кнопки Наверх 
        if (is_subfolder && current_item_idx == 0) {
            snprintf(display_string, sizeof(display_string), "00. .. [UP]");
            ui_draw_text_rel(10, rel_y, display_string, t_color, 1);
        } 
        // 2. Отрисовка кнопки Следующая страница
        else if (show_next_button && current_item_idx == (display_count - 1)) {
            snprintf(display_string, sizeof(display_string), ".. [DWN] NEXT PAGE >>");
            ui_draw_text_rel(10, rel_y, display_string, t_color, 1);
        }
        // 3. Отрисовка реальных элементов FatFS
        else {
            // Вычисляем индекс внутри массива файлов sd_info.files
            int file_idx = is_subfolder ? (current_item_idx - 1) : current_item_idx;
            
            if (file_idx >= 0 && file_idx < sd_info.file_count) {
                int file_number = (g_current_page * SD_MAX_FILES) + file_idx + 1; 
                
                // ИСПРАВЛЕНО: Вместо несуществующего .is_dir проверяем тип СТРОГО по вашему хедеру!
                if (sd_info.files[file_idx].type == FILE_TYPE_FOLDER) {
                    // Папка — добавляем маркер звёздочки
                    snprintf(display_string, sizeof(display_string), "%02d. * %s", file_number, sd_info.files[file_idx].name);
                } else {
                    // Обычный файл пресета (.syx)
                    snprintf(display_string, sizeof(display_string), "%02d.   %s", file_number, sd_info.files[file_idx].name);
                }
                
                ui_draw_text_rel(10, rel_y, display_string, t_color, 1);
            }
        }
    }
}


// ====================================================================
// 4. ДИСПЕТЧЕР И КАРКАСНЫЙ ОПРЕДЕЛИТЕЛЬ СТРАНИЦ (МЕНЕДЖЕР UI РЕЖИМА)
// ====================================================================
void sd_review_render(void) {
    char header_string[32]; 
    bool is_subfolder = (strcmp(current_browser_path, "/") != 0);

    if (!is_subfolder) {
        snprintf(header_string, sizeof(header_string), "DIR: /");
    } else {
        snprintf(header_string, sizeof(header_string), "%.16s", current_browser_path + 1);
    }

    bool show_next_button = (sd_info.file_count >= SD_MAX_FILES);
    int display_count = sd_info.file_count + (is_subfolder ? 1 : 0) + (show_next_button ? 1 : 0);

    uint8_t current_ui_page = 0;
    uint8_t total_ui_pages = 1;

    if (display_count > 0) {
        current_ui_page = g_current_index / 5;
        total_ui_pages = (display_count + 4) / 5;
    }

    // Вызываем сквозной каркас, который сотрет экран и нарисует статусбар только при сменах
    //ui_render_mode_layout(header_string, current_ui_page, total_ui_pages, sd_review_content_draw)

    // ВЫЗОВ СКВОЗНОЙ LAYER ФУНКЦИИ С ФЛАГОМ ПРИНУДИТЕЛЬНОЙ ПЕРЕРИСОВКИ g_force_redraw
    // Мы можем передавать туда true, если у вас в ревью экран чистится при каждом движении, 
    // либо завести такую же переменную g_force_redraw, которая у вас там уже есть!
    ui_render_mode_layout(header_string, current_ui_page, total_ui_pages, g_force_redraw, sd_review_content_draw);
}

// ====================================================================
// 5. ОБРАБОТКА НАВИГАЦИИ И КЛИКОВ (УНИЧТОЖЕНИЕ БАГА 68-мс)
// ====================================================================
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
            debug_chrono_user_action("CLK_manualy");
            if (g_current_index >= 0 && g_current_index < display_count) {
                
                // НАЖАТИЕ НА НАВЕРХ
                if (is_subfolder && g_current_index == 0) {
                    SD_LOG("[SD UI] Reset path to root\n");
                    snprintf(current_browser_path, sizeof(current_browser_path), "/");
                    g_current_page = 0; 
                    
                    ui_clear_work_area();
                    debug_chrono_sd_op("SCAN_DIR...", current_browser_path);
                    sd_storage_scan_files_page(current_browser_path, 0); 
                    
                    g_current_index = 0;
                    g_force_redraw = true; 
                    
                    touched = 0; // СБРОС ТРИГГЕРА ТАЧА И ПРИНУДИТЕЛЬНЫЙ ВЫХОД
                    return; 
                } 
                // НАЖАТИЕ НА СЛЕДУЮЩУЮ СТРАНИЦУ [DWN]
                else if (show_next_button && g_current_index == (display_count - 1)) {
                    g_current_page++; 
                    SD_LOG("[SD UI] Loading next page: %d\n", g_current_page);
                    
                    ui_clear_work_area();
                    debug_chrono_sd_op("SCAN_DIR...", current_browser_path);
                    sd_storage_scan_files_page(current_browser_path, g_current_page);
                    debug_chrono_sd_op("BROWSE_DIR", current_browser_path);
                    g_current_index = is_subfolder ? 1 : 0; 
                    g_force_redraw = true;
                    
                    touched = 0; // СБРОС ТРИГГЕРА ТАЧА И ПРИНУДИТЕЛЬНЫЙ ВЫХОД
                    return;
                }
                // НАЖАТИЕ НА file-ЭЛЕМЕНТ *.sys или *.mid
                else {
                    int file_idx = is_subfolder ? (g_current_index - 1) : g_current_index;

                    if (file_idx >= 0 && file_idx < sd_info.file_count) {
                        
                        // КЛИКНУЛИ ПО ПАПКЕ (ВХОД В ПОДПАПКУ)
                        if (sd_info.files[file_idx].type == FILE_TYPE_FOLDER) {
                            snprintf(current_browser_path, sizeof(current_browser_path), "/%s", sd_info.files[file_idx].name);
                            g_current_page = 0; 

                            ui_clear_work_area();
                            debug_chrono_sd_op("SCAN_DIR...", current_browser_path);
                            sd_storage_scan_files_page(current_browser_path, 0);
                            g_current_index = 0;
                            g_force_redraw = true; 
                            
                            touched = 0; // СБРОС ТРИГГЕРА ТАЧА И ПРИНУДИТЕЛЬНЫЙ ВЫХОД
                            return; 
                        } 
                        // КЛИКНУЛИ ПО ФАЙЛУ (ОТПРАВКА MIDI)
                        else {
                            // Отрисовка DOS Попапа полностью переведена на новые рельсы ui_engine относительных координат!
                            uint16_t popup_w = (UI_WORK_WIDTH * 7) / 10;
                            uint16_t popup_h = (UI_WORK_HEIGHT * 3) / 10;
                            uint16_t popup_rel_x = (UI_WORK_WIDTH - popup_w) / 2;
                            uint16_t popup_rel_y = (UI_WORK_HEIGHT - popup_h) / 2;

                            ui_draw_card_rel(popup_rel_x, popup_rel_y, popup_w, popup_h, current_theme.bg_color, 0x0000);  
                            ui_draw_text_rel(popup_rel_x + 30, popup_rel_y + 14, "MIDI SENDING...", current_theme.bar_text_color, 1);
                            ui_draw_text_rel(popup_rel_x + 36, popup_rel_y + 32, "Please wait...", current_theme.accent_color, 1);
                            
                            sleep_ms(50); 
                            
                            sd_review_send_current_file(); // Вызов оригинального метода без аргументов
                            g_force_redraw = true; 
                            
                            touched = 0; // СБРОС ТРИГГЕРА ТАЧА И ПРИНУДИТЕЛЬНЫЙ ВЫХОД
                            return;
                        }
                    }
                }
            }
            touched = 0; 
        }
        sd_review_render();
    }
}

