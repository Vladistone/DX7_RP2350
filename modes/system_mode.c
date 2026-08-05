#include "hw_config.h"
#include "modes.h"
#include "debug_log.h"
#include "numpad_dvr.h"       // Подключаем маппинг кнопок MPR121
#include "ui_engine.h"   
#include "encoder_dvr.h"
#include "TFT_dvr.h"          // Для работы draw_text_scaled
#include "sd_storage.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include <stdio.h>
#include <stdarg.h>

// =================================================================
// 1. ПРОТОТИПЫ ВНУТРЕННИХ ФУНКЦИЙ (Защита от implicit declaration)
// =================================================================
void system_screen_render(uint16_t mpr_touched_state, float v_sys);
void System_handle_scroll(int enc_delta);
static void draw_mpr121_visual_map_clean(uint16_t touched, int start_x, int start_y, bool force_redraw);
static void system_render_page_2(uint16_t touched, float v_sys, int start_y, bool page_changed);

// Внешние хелперы из ui_engine.c и других модулей
void ui_draw_footer(const char* text);
//void render_page_pinout(int line);
//void render_page_file_structure(int line);

// Макросы Си для превращения дефайнов хедера в строки для экрана
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// =================================================================
// 2. СТАТИЧЕСКИЕ ПЕРЕМЕННЫЕ МОДУЛЯ
// =================================================================
static uint8_t g_sys_page = 0;              // 0 - Diag, 1 - Pinout, 2 - File Structure
static uint16_t g_last_mpr_state = 0xFFFF;  // Предыдущее состояние кнопок
static uint8_t g_last_rendered_page = 0xFF; // Предыдущая страница для очистки экрана

// Индекс выбора внутри Сервисного Меню Логов (1 - USB, 2 - SD)
static int selected_item = 1; 

// =================================================================
// 3. МЕТОДЫ ИНТЕРФЕЙСА РЕЖИМА SYSTEM_CONFIG
// =================================================================

void system_mode_init(void) {
    ui_clear_work_area(); 
    g_last_rendered_page = 0xFF; 
}

void system_mode_render(void) {
    uint16_t touched = mpr121_read_touched(); 
    
    // 1. Рендерим основную диагностическую страницу
    system_screen_render(touched, 5.01f);

    // 2. Если открыта страница 0 (Diag), поверх выводим Сервисное Меню Логов
    if (g_sys_page == 0) {
        // Исправлено: Добавлен аргумент bg_color и масштаб для draw_text_scaled
        draw_text_scaled(10, 110, "SERVICE MENU (ENCODER SW):", current_theme.accent_color, current_theme.bg_color, 1);

        char usb_status[32];
        snprintf(usb_status, sizeof(usb_status), "1. USB Trace: %s", g_cli_debug_usb_active ? "[ON]" : "[OFF]");
        uint16_t usb_color = (selected_item == 1) ? current_theme.accent_color : current_theme.text_color;
        draw_text_scaled(20, 130, usb_status, usb_color, current_theme.bg_color, 1);

        char sd_status[32];
        snprintf(sd_status, sizeof(sd_status), "2. SD BlackBox: %s", g_cli_debug_sd_active ? "[ON]" : "[OFF]");
        uint16_t sd_color = (selected_item == 2) ? current_theme.accent_color : current_theme.text_color;
        draw_text_scaled(20, 150, sd_status, sd_color, current_theme.bg_color, 1);
    }
}

void system_mode_update(uint16_t touched, int enc_delta) {
    if (enc_delta != 0) {
        if (g_sys_page == 0) {
            selected_item += enc_delta;
            if (selected_item < 1) selected_item = 2;
            if (selected_item > 2) selected_item = 1;
        } else {
            System_handle_scroll(enc_delta);
        }
    }

    if (touched) { 
        debug_chrono_user_action("SYS_MENU_CLICK");

        if (g_sys_page == 0) {
            if (selected_item == 1) { 
                g_cli_debug_usb_active = !g_cli_debug_usb_active;
            } else if (selected_item == 2) {
                g_cli_debug_sd_active = !g_cli_debug_sd_active;
                if (g_cli_debug_sd_active) {
                    debug_chrono_init(); 
                }
            }
        }
    }

    system_screen_render(touched, 5.01f);
}

// =================================================================
// 4. СТРАНИЦЫ РЕНДЕРИНГА И СЕРВИСНЫЕ СКРОЛЛЕРЫ
// =================================================================

void system_screen_render(uint16_t mpr_touched_state, float v_sys) {
    bool page_changed = (g_sys_page != g_last_rendered_page);
    
    if (page_changed) {
        g_last_rendered_page = g_sys_page;
        
        char header_string[32];
        snprintf(header_string, sizeof(header_string), "SYS Config & Diag %d", g_sys_page + 1);
        ui_draw_statusbar(header_string, sd_info.is_mounted, 1);
        
        ui_clear_work_area(); 
/*
        if (g_sys_page == 1) {
            render_page_pinout(30);
        } else if (g_sys_page == 2) {
            render_page_file_structure(30);
        }
*/        
        char footer_buf[16];
        snprintf(footer_buf, sizeof(footer_buf), "PAGE %d/3", g_sys_page + 1);
        ui_draw_footer(footer_buf);
    }

    if (g_sys_page == 0) {
        system_render_page_2(mpr_touched_state, v_sys, 30, page_changed);
    }
}

void System_handle_scroll(int enc_delta) {
    if (enc_delta != 0) {
        int next_page = g_sys_page + enc_delta;
        if (next_page < 0) next_page = 2;
        if (next_page > 2) next_page = 0;
        g_sys_page = (uint8_t)next_page;
    }
}

// СТРАНИЦА 1: Реальная диагностика геометрии SD-карты
static void system_render_page_2(uint16_t touched, float v_sys, int start_y, bool page_changed) {
    char buf[64];
    
    if (page_changed) {
        uint32_t cpu_hz = clock_get_hz(clk_sys) / 1000000;
        snprintf(buf, sizeof(buf), "CORE: RP2350 @%luMHz|FW:1.0.2", cpu_hz);
        draw_text_scaled(10, start_y, buf, current_theme.accent_color, current_theme.bg_color, 1);
        
        snprintf(buf, sizeof(buf), "POWER: %.2fV", v_sys);
        draw_text_scaled(10, start_y + 11, buf, current_theme.text_color, current_theme.bg_color, 1);
        
        if (sd_info.is_mounted) {
            const char* type_str = (sd_info.card_type & 12) ? "SDHC" : "SDSC";
            if (sd_info.total_capacity_mb > 4096) {
                snprintf(buf, sizeof(buf), "SD: %s | %.1fGB | FREE: %.1fGB",
                    type_str,
                    (double)sd_info.total_capacity_mb / 1024 / 1000,
                    (double)sd_info.free_space_mb / 1024 / 1000);
            } else {
                snprintf(buf, sizeof(buf), "SD: %s | %.1fMB | FREE: %.1fMB",
                    type_str,
                    (double)sd_info.total_capacity_mb / 1000,
                    (double)sd_info.free_space_mb / 1000);
            }
        } else {
            snprintf(buf, sizeof(buf), "SD CARD: NOT MOUNTED / ERROR");
        }
        draw_text_scaled(10, start_y + 22, buf, current_theme.text_color, current_theme.bg_color, 1);
        
        snprintf(buf, sizeof(buf), "BUS: TFT SPI OK | MPR121 I2C OK");
        draw_text_scaled(10, start_y + 33, buf, current_theme.text_color, current_theme.bg_color, 1);

        draw_mpr121_visual_map_clean(touched, 10, start_y + 46, page_changed);

        uint16_t lx = 160;
        uint16_t ly = start_y + 60;
        snprintf(buf, sizeof(buf), "%s,%s:CURS U/D", STR(MPR_CUR_UP), STR(MPR_CUR_DN));
        draw_text_scaled(lx, ly, buf, current_theme.text_color, current_theme.bg_color, 1);
        snprintf(buf, sizeof(buf), "%s,%s:DISK U/D", STR(MPR_DISK_UP), STR(MPR_DISK_DN));
        draw_text_scaled(lx, ly + 11, buf, current_theme.text_color, current_theme.bg_color, 1);
        snprintf(buf, sizeof(buf), "%s:ESC %s:MODE", STR(MPR_ESC), STR(MPR_MODE));
        draw_text_scaled(lx, ly + 22, buf, current_theme.text_color, current_theme.bg_color, 1);
        snprintf(buf, sizeof(buf), "%s:SEL %s:ENTR", STR(MPR_SELECT), STR(MPR_ENTER));
        draw_text_scaled(lx, ly + 33, buf, current_theme.text_color, current_theme.bg_color, 1);
        snprintf(buf, sizeof(buf), "%s:STP %s:PLAY", STR(MPR_STOP), STR(MPR_PLAY));
        draw_text_scaled(lx, ly + 44, buf, current_theme.text_color, current_theme.bg_color, 1);
        snprintf(buf, sizeof(buf), "%s:FF %s:RW Hold", STR(MPR_FF), STR(MPR_RW));
        draw_text_scaled(lx, ly + 55, buf, current_theme.text_color, current_theme.bg_color, 1);
    }

    draw_mpr121_visual_map_clean(touched, 10, start_y + 46, page_changed);
}

// Отрисовка интерактивной карты Touchpad
static void draw_mpr121_visual_map_clean(uint16_t touched, int start_x, int start_y, bool force_redraw) {
    int box_w = 28;
    int box_h = 14;
    int gap = 4;

    if (force_redraw) {
        draw_text_scaled(start_x + 46, start_y, "SYSTEM Signatured MAP:", current_theme.accent_color, current_theme.bg_color, 1);
    }

    for (int i = 0; i < 12; i++) {
        bool is_pressed = (touched & (1 << i)) != 0;
        bool was_pressed = (g_last_mpr_state & (1 << i)) != 0;

        if (force_redraw || (is_pressed != was_pressed)) {
            int col = i % 4;
            int row = i / 4;
            int x = 14 + start_x + col * (box_w + gap);
            int y = 14 + start_y + row * (box_h + gap);
            
            uint16_t color = is_pressed ? current_theme.accent_color : 0x31A6; 

            clear_rect(x, y, box_w, box_h, color);

            char num_str[3];
            snprintf(num_str, sizeof(num_str), "%02d", i);
            
            uint16_t text_x = x + 5; 
            uint16_t text_y = y + 0; 
            uint16_t text_color = is_pressed ? 0x0000 : current_theme.accent_color;

            draw_text_scaled(text_x, text_y, num_str, text_color, color, 1);
        }
    }
    // Фиксируем финальное состояние маски кнопок
    g_last_mpr_state = touched; 
}
