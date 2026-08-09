#include "ui_engine.h"
#include "modes.h"      
#include "TFT_dvr.h"     // Видит все функции Блока 6
#include "sd_storage.h"
#include <stdio.h>

// Официальные статические трекеры состояний графического ядра
static AppModeState ui_engine_last_mode = MODE_COUNT;
static uint8_t ui_engine_last_page = 0xFF;

void ui_clear_work_area(void) {
    // Чистая очистка рабочей зоны экрана (Оффсет сверху 24px, снизу 16px)
    clear_rect(0, 24, TFT_WIDTH, TFT_HEIGHT - 24 - 16, current_theme.bg_color);
}

void ui_draw_statusbar(const char* mode_tag, bool sd_status, uint8_t midi_ch) {
    clear_rect(0, 0, TFT_WIDTH, 24, current_theme.bar_bg_color);
    draw_text_scaled(10, 6, mode_tag, current_theme.bar_text_color, current_theme.bar_bg_color, 1);
    
    // Выводим только маркер ошибки красным цветом, если карты нет
    draw_text_scaled(TFT_WIDTH - 80, 6, "SD:", current_theme.bar_text_color, current_theme.bar_bg_color, 1);
    draw_text_scaled(TFT_WIDTH - 56, 6, sd_status ? "OK" : "-", sd_status ? 0x07E0 : 0xF800, current_theme.bar_bg_color, 1);
}

void ui_draw_footer(const char* footer_text) {
    // 1. Начисто очищаем нижнюю плашку подвала высотой 16 пикселей
    clear_rect(0, TFT_HEIGHT - 16, TFT_WIDTH, 16, current_theme.bar_bg_color);
    
    // 2. ИСПРАВЛЕНО: Печатаем текст подвала строго через существующую draw_text_scaled!
    // Отступ X=10, Y вычисляется как верхний край подвала + 2 пикселя оффсета для ровного шрифта 8x12
    draw_text_scaled(10, TFT_HEIGHT - 14, footer_text, current_theme.bar_text_color, current_theme.bar_bg_color, 1);
}

// ====================================================================
// ИДЕАЛЬНО ОТПОЛИРОВАННЫЙ КОНВЕЙЕР "НОВЫХ РЕЛЬС" UI_ENGINE
// ====================================================================
void ui_render_mode_layout(const char* header, uint8_t cur_page, uint8_t total_pages, bool force_redraw, void (*render_content_cb)(void)) {
    // Проверяем изменения рантайм-кадра
    bool mode_changed = (g_current_mode != ui_engine_last_mode);
    bool page_changed = (cur_page != ui_engine_last_page);
    
    // ОТРИСОВКА КАДРА: Строго один раз в момент изменений или по принудительному флагу!
    if (mode_changed || page_changed || force_redraw) {
        ui_engine_last_mode = g_current_mode;
        ui_engine_last_page = cur_page;
        
        char header_buf[32];
        if (total_pages > 1) {
            snprintf(header_buf, sizeof(header_buf), "%s | P.%d", header, cur_page + 1);
        } else {
            snprintf(header_buf, sizeof(header_buf), "%s", header);
        }
        
        // Перестраиваем каркас и чистим рабочую область
        ui_draw_statusbar(header_buf, sd_info.is_mounted, 1);
        ui_clear_work_area();
        
        char footer_buf[32];
        snprintf(footer_buf, sizeof(footer_buf), "PAGE %d/%d", cur_page + 1, total_pages);
        ui_draw_footer(footer_buf);
        
        // Прописываем контент на чистый холст строго ОДИН РАЗ!
        if (render_content_cb != NULL) {
            render_content_cb();
        }
        return; // Кадр построен, выходим! Шина SPI полностью свободна!
    }

    // ДЛЯ ЦИКЛИЧЕСКИХ КАДРОВ (Когда стоим на месте):
    // Разрешаем сквозной вызов контента БЕЗ очистки экрана СТРОГО только на Первой странице HELP
    // (для теста тачпада MPR121) и Первой странице SYS Config (для замера живого вольтметра)
    if (cur_page == 0 && (g_current_mode == MODE_HELP || g_current_mode == MODE_SYSTEM_CONFIG)) {
        if (render_content_cb != NULL) {
            render_content_cb();
        }
    }
}

    // 2. ДИНАМИЧЕСКОЕ НАПОЛНЕНИЕ СТРАНИЦЫ 1 (Зависит от выбранного режима устройства!)
/*   if (ui_current_page == 0) {
        switch (g_current_mode) {
            case MODE_HELP:
                // Если мы в режиме HELP - на странице 1 вызываем чистую диагностику
                // (При необходимости вынесите render_page_diagnostics сюда же в ui_engine.c)
                render_page_diagnostics(touched_state, v_sys, 30, page_changed);
                break;
                
            case MODE_SYSTEM_CONFIG:
                // If we are in the configuration mode - draw the menu layout and diagnostic values
                render_page_diagnostics(touched_state, v_sys, 30, page_changed);
                
                // И прямо поверх накладываем интерактивное Сервисное Меню Логов!
                draw_text_scaled(10, 110, "SERVICE MENU (ENCODER SW):", current_theme.accent_color, current_theme.bg_color, 1);
                
                char usb_status[32];
                snprintf(usb_status, sizeof(usb_status), "1. USB Trace: %s", g_cli_debug_usb_active ? "[ON]" : "[OFF]");
                uint16_t usb_color = (selected_item == 1) ? current_theme.accent_color : current_theme.text_color;
                draw_text_scaled(20, 130, usb_status, usb_color, current_theme.bg_color, 1);

                char sd_status[32];
                snprintf(sd_status, sizeof(sd_status), "2. SD BlackBox: %s", g_cli_debug_sd_active ? "[ON]" : "[OFF]");
                uint16_t sd_color = (selected_item == 2) ? current_theme.accent_color : current_theme.text_color;
                draw_text_scaled(20, 150, sd_status, sd_color, current_theme.bg_color, 1);
                break;
                
            default:
                break;
        }
    }
*/
// =================================================================
// РЕАЛИЗАЦИЯ ХЕЛПЕРОВ
// =================================================================
void ui_draw_text_rel(int rel_x, int rel_y, const char* text, uint16_t color, uint8_t scale) {
    int abs_x = UI_WORK_X + rel_x;
    int abs_y = UI_WORK_Y + rel_y;
    draw_text_scaled(abs_x, abs_y, text, color, current_theme.bg_color, scale);
}

void ui_draw_text_centered_rel(int rel_y, const char* text, uint16_t color, uint8_t scale) {
    int text_len = strlen(text);
    int text_width = text_len * (10 * scale); // 6-8px - базовая ширина символа
    int abs_x = (TFT_WIDTH - text_width) / 2;
    if (abs_x < 0) abs_x = 0;
    
    int abs_y = UI_WORK_Y + rel_y;
    draw_text_scaled(abs_x, abs_y, text, color, current_theme.bg_color, scale);
}

void ui_draw_card_rel(int rel_x, int rel_y, int w, int h, uint16_t border_color, uint16_t bg_color) {
    int abs_x = UI_WORK_X + rel_x;
    int abs_y = UI_WORK_Y + rel_y;
    
    // Внутренняя заливка
    clear_rect(abs_x, abs_y, w, h, bg_color);
    
    // Рамка (если border_color отличается)
    if (border_color != bg_color) {
        clear_rect(abs_x, abs_y, w, 1, border_color);             // Top
        clear_rect(abs_x, abs_y + h - 1, w, 1, border_color);     // Bottom
        clear_rect(abs_x, abs_y, 1, h, border_color);             // Left
        clear_rect(abs_x + w - 1, abs_y, 1, h, border_color);     // Right
    }
}