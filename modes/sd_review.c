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

void sd_review_render(void) {
    ui_draw_statusbar("FILE", sd_info.is_mounted, 1);
    ui_clear_work_area();

    if (sd_info.file_count == 0) {
        draw_text_scaled(10, 40, "EMPTY", current_theme.text_color, current_theme.bg_color, 1);
        ui_draw_footer("NO FILES");
        return;
    }

    // Выводим только первые 5 элементов (как в вашем исходном рабочем коде)
    for (int i = 0; i < sd_info.file_count && i < 5; i++) {
        uint16_t y_pos = 35 + (i * 20);
        
        uint16_t t_color = (i == g_current_index) ? current_theme.bg_color : current_theme.text_color;
        uint16_t b_color = (i == g_current_index) ? current_theme.accent_color : current_theme.bg_color;

        if (i == g_current_index) {
            clear_rect(0, y_pos - 2, TFT_WIDTH, 18, b_color);
        }

        draw_text_scaled(10, y_pos, sd_info.files[i].name, t_color, b_color, 1);
    }

    ui_draw_footer("SELECT");
}

void sd_review_update(uint16_t touched, int enc_delta) {
    static uint32_t last_action_time = 0;
    uint32_t current_time = time_us_32();

    if (current_time - last_action_time < 150000) return;

    if (enc_delta != 0 || touched) {
        last_action_time = current_time;

        if (sd_info.file_count > 0 && enc_delta != 0) {
            g_current_index += enc_delta;
            if (g_current_index < 0) g_current_index = sd_info.file_count - 1;
            if (g_current_index >= sd_info.file_count) g_current_index = 0;
        }

        if (touched) {
            uint16_t idx = g_current_index;
            if (sd_info.files[idx].type == FILE_TYPE_FOLDER) {
                // Вход во вложенную папку
                char next_path[64]; // <--- ИСПРАВЛЕНО КВАДРАТНЫМИ СКОБКАМИ! Тепер это строка на 64 байта
                snprintf(next_path, sizeof(next_path), "/%s", sd_info.files[idx].name);
                
                // Принудительно гасим рабочую область экрана перед долгим чтением, 
                // чтобы дисплей визуально не залипал на старом корне
                ui_clear_work_area(); 
                
                sd_storage_scan_files(next_path);
                g_current_index = 0;
                
                // СБРОС ТРИГГЕРА: исключаем повторный ложный заход в блок клика на следующем такте цикла
                touched = false; 
            } else {
                sd_review_send_current_file();
                touched = false;
            }
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
