#include "ui_engine.h"
#include "TFT_dvr.h"
#include "sd_storage.h"
#include <stdio.h>

// Статическая переменная для прецизионного отслеживания переключения страниц
static uint8_t ui_last_page_idx = 0xFF;
// Статические переменные Движка Страниц
//static uint8_t ui_current_page = 0;
//static uint8_t ui_last_rendered_page = 0xFF;

void ui_engine_init(void) {
    fill_screen(current_theme.bg_color);
}

void ui_set_theme(uint16_t bg, uint16_t text, uint16_t accent, uint16_t bar_bg, uint16_t bar_text) {
    current_theme.bg_color = bg;
    current_theme.text_color = text;
    current_theme.accent_color = accent;
    current_theme.bar_bg_color = bar_bg;
    current_theme.bar_text_color = bar_text;
}

void ui_draw_statusbar(const char* mode_tag, bool sd_status, uint8_t midi_ch) {
    // 1. Отрисовка плашки верха (высота 22px)[cite: 11]
    clear_rect(0, 0, TFT_WIDTH, 22, current_theme.bar_bg_color);

    // 2. Вывод метки режима слева: [PLAY], [SYS], [DAW], [FILE][cite: 11]
    char left_buf[24];
    snprintf(left_buf, sizeof(left_buf), "%s", mode_tag);
    draw_text_scaled(6, 4, left_buf, current_theme.accent_color, current_theme.bar_bg_color, 1);

    // 3. Вывод статуса справа: SD:OK / SD:-- и MIDI Ch[cite: 11]
    char right_buf[16];
    snprintf(right_buf, sizeof(right_buf), "SD:%s CH:%02d", sd_status ? "OK" : "--", midi_ch);
    // Автовыравнивание по правому краю
    int text_w = strlen(right_buf) * 10; // 6px на символ
    draw_text_scaled(TFT_WIDTH - text_w - 8, 4, right_buf, current_theme.accent_color, current_theme.bar_bg_color, 1);
    //draw_text_scaled(TFT_WIDTH - 110, 4, right_buf, current_theme.accent_color, current_theme.bar_bg_color, 1);

    // 4. Линия разделения[cite: 11]
    clear_rect(0, 22, TFT_WIDTH, 2, current_theme.accent_color);
}

// функция для корректной отрисовки нижней панели (подвала) без искажений
void ui_draw_footer(const char* footer_text) {
    // Очищаем область подвала внизу экрана (высота 20px)
    clear_rect(0, TFT_HEIGHT - UI_FOOTER_HEIGHT, TFT_WIDTH, UI_FOOTER_HEIGHT, current_theme.bar_bg_color);
    //clear_rect(0, TFT_HEIGHT - 20, TFT_WIDTH, 20, current_theme.bar_bg_color);
    // Выводим текст подсказок через правильный масштабируемый шрифтовой движок
    draw_text_scaled(10, TFT_HEIGHT - 16, footer_text, current_theme.bar_text_color, current_theme.bar_bg_color, 1);
}

void ui_clear_work_area(void) {
    // Очищает область под статус-баром (y >= 24)
    // Исправлено: очищает ТОЛЬКО область между Хедером и Футером
    clear_rect(UI_WORK_X, UI_WORK_Y, UI_WORK_WIDTH, UI_WORK_HEIGHT, current_theme.bg_color);
    //clear_rect(0, 24, TFT_WIDTH, TFT_HEIGHT - 24 - 20, current_theme.bg_color); // work window (start_y: 24; end_y: 20)
}

// =================================================================
// ГЛАВНЫЙ РЕНДЕР МГОНОСТРАНИЧНОСТИ UI КОНТЕНТА ЛЮБОГО РЕЖИМА
// =================================================================
void ui_render_mode_layout(const char* header, uint8_t cur_page, uint8_t total_pages, void (*render_content_cb)(void)) {
    bool page_changed = (cur_page != ui_last_page_idx);
    
    if (page_changed) {
        ui_last_page_idx = cur_page;
        
        // 1. Отрисовка статусбара (название режима берется из аргумента header)
        char header_buf[32];
        snprintf(header_buf, sizeof(header_buf), "%s | P.%d", header, cur_page + 1);
        ui_draw_statusbar(header_buf, sd_info.is_mounted, 1);
        
        // 2. Прецизионная очистка рабочей зоны (вызывается строго при смене страницы)
        ui_clear_work_area();
        
        // 3. Отрисовка футера с текущим балансом страниц
        char footer_buf[16];
        snprintf(footer_buf, sizeof(footer_buf), "PAGE %d/%d", cur_page + 1, total_pages);
        ui_draw_footer(footer_buf);
    }

    // 4. Вызываем заполнение уникальным контентом, который передал активный режим
    if (render_content_cb != NULL) {
        render_content_cb();
    }
}
    // 2. ДИНАМИЧЕСКОЕ НАПОЛНЕНИЕ СТРАНИЦЫ 1 (Зависит от выбранного режима устройства!)
/*    if (ui_current_page == 0) {
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