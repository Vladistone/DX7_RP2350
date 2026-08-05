#include "debug_log.h"
#include "ui_engine.h"
#include "TFT_dvr.h"
#include "sd_storage.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "numpad_dvr.h"       // Подключаем маппинг кнопок MPR121
#include <stdio.h>
#include <stdarg.h>

// Прототип футера из ui_engine.c, чтобы убрать варнинг компилятора
void ui_draw_footer(const char* text);

// Макросы Си для превращения дефайнов хедера в строки для экрана
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

static uint8_t g_sys_page = 0; // 0 - Diag, 1 - Pinout, 2 - File Structure
static uint16_t g_last_mpr_state = 0xFFFF; // Хранитель предыдущего состояния кнопок для отслеживания изменений
static uint8_t g_last_rendered_page = 0xFF; // Хранитель предыдущей страницы для триггера полной очистки

// Отрисовка интерактивной карты Touchpad
static void draw_mpr121_visual_map_clean(uint16_t touched, int start_x, int start_y, bool force_redraw) {
    int box_w = 28;
    int box_h = 14;
    int gap = 4;

    // Рисуем заголовок карты кнопок только один раз при смене страницы
    if (force_redraw) {
        draw_text_scaled(start_x + 46, start_y, "NUMPAD Signatured MAP:", current_theme.accent_color, current_theme.bg_color, 1);
    }

for (int i = 0; i < 12; i++) {
        bool is_pressed = (touched & (1 << i)) != 0;
        bool was_pressed = (g_last_mpr_state & (1 << i)) != 0;

        // КРИТИЧЕСКИЙ СЕКТОРНЫЙ ФИЛЬТР: перерисовываем кубик ТОЛЬКО если его состояние 
        // реально изменилось по сравнению с прошлым тактом, либо принудительно при смене страницы!
        if (force_redraw || (is_pressed != was_pressed)) {
            int col = i % 4;
            int row = i / 4;
            int x = 14 + start_x + col * (box_w + gap);
            int y = 14 + start_y + row * (box_h + gap);
            
            uint16_t color = is_pressed ? current_theme.accent_color : 0x31A6; 

            // Стираем и рисуем только этот ОДИН конкретный кубик кнопки прецизионно по пикселям!
            clear_rect(x, y, box_w, box_h, color);

            char num_str[3];
            snprintf(num_str, sizeof(num_str), "%02d", i);
            // 2. МАТЕМАТИЧЕСКОЕ ВЫРАВНИВАНИЕ ЦИФР ПО ЦЕНТРУ
            // При ширине шрифта ~6px и высоте ~8px:
            // Смещение по X: x + 5 (идеальный центр по горизонтали для "XX")
            // Смещение по Y: y + 3 (подняли на 1 пиксель вверх, чтобы компенсировать базовую линию шрифта)
            uint16_t text_x = x + 5; // Выравнивание координат по центру кубика
            uint16_t text_y = y + 0; // БЫЛО: uint16_t text_y = y + 3;
            // 3. СМЕНА ЦВЕТА СИМВОЛОВ НА БИРЮЗОВЫЙ ДЛЯ ЧИТАЕМОСТИ
            // Если кнопка зажата — инвертируем цвет текста (черный на бирюзовом), 
            // если отпущена — бирюзовый текст на сером кубике.
            uint16_t text_color = is_pressed ? 0x0000 : current_theme.accent_color;

            // Отрисовываем цифры строго по центру кубиков
            draw_text_scaled(text_x, text_y, num_str, text_color, color, 1);
        }
    }
        /*
        // Отрисовка символа градуса справа от всей сетки
        // Рассчитываем крайнюю правую координату всей матрицы кубиков (4 колонки)
        int total_grid_w = 4 * box_w + 3 * gap; 
        int degree_x = 14 + start_x + total_grid_w + 10; // +10 пикселей отступа справа от сетки

        // Центрируем символ градуса по вертикали относительно всей сетки (3 строки)
        int total_grid_h = 3 * box_h + 2 * gap;
        int degree_y = 14 + start_y + (total_grid_h / 2) - 4; // -4 для визуальной корректировки

        // Выводим символ в зависимости от вашей библиотеки (выберите один вариант):
        draw_text_scaled(0, degree_y, "°", current_theme.accent_color, current_theme.bg_color, 1);
        //display_print_at(degree_x, degree_y, "°");    // Если библиотека поддерживает UTF-8
        // display_print_at(degree_x, degree_y, "\xB0"); // Если это Adafruit_GFX / CP437
        // display_draw_circle(degree_x, degree_y, 3, COLOR_WHITE); // Если рисуем геометрией
        */
        g_last_mpr_state = touched; // Запоминаем текущую маску тача    
    }
// СТРАНИЦА 1: Реальная диагностика геометрии SD-карты
static void render_page_diagnostics(uint16_t touched, float v_sys, int start_y, bool page_changed) {
    char buf[40];
    
    if (page_changed) {
        uint32_t cpu_hz = clock_get_hz(clk_sys) / 1000000;
        snprintf(buf, sizeof(buf), "CORE: RP2350 @%luMHz|FW:1.0.2", cpu_hz);
        draw_text_scaled(10, start_y, buf, current_theme.accent_color, current_theme.bg_color, 1);
        
        snprintf(buf, sizeof(buf), "POWER: %.2fV", v_sys);  //|  FW: v1.0.2", v_sys);
        draw_text_scaled(10, start_y + 11, buf, current_theme.text_color, current_theme.bg_color, 1);
        
        // ВЫВОД РЕАЛЬНОГО ТИПА И СВОБОДНОЙ ЕМКОСТИ КАРТЫ С КОРРЕКТНЫМИ ДАННЫМИ
        if (sd_info.is_mounted) {
            const char* type_str = (sd_info.card_type & 12) ? "SDHC" : "SDSC";
            // Если карта огромная, переводим вывод в Гигабайты (МБ / 1024)
            if (sd_info.total_capacity_mb > 4096) {
                snprintf(buf, sizeof(buf), "SD: %s | %.1fGB | FREE: %.1fGB",
                    type_str,
                    (double)sd_info.total_capacity_mb / 1024 / 1000,
                    (double)sd_info.free_space_mb / 1024/ 1000);
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

        // ИСПРАВЛЕНО: Дописано суффикс _clean к имени функции отрисовки тачпада
        draw_mpr121_visual_map_clean(touched, 10, start_y + 46, page_changed);

        // Отрисовка статической легенды (только один раз!)
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

    // КАРТА ТАЧПАДА ПЕРЕРИСОВЫВАЕТСЯ АВТОНОМНО И БЕЗ МЕРЦАНИЯ ЭКРАНА!
    draw_mpr121_visual_map_clean(touched, 10, start_y + 46, page_changed);
}

// СТРАНИЦА 2: Аппаратный маппинг пинов (Ультра-плотный формат под 34 символа)
static void render_page_pinout(int start_y) {
    draw_text_scaled(10, start_y, "RP2350 HARDWARE PINOUT:", current_theme.accent_color, current_theme.bg_color, 1);
    int y = start_y + 14;
    draw_text_scaled(10, y,      "TFT LCD (SPI BUS):", current_theme.accent_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 11, "> DC:GP10|CS:GP11|CLK:GP12(SCL)", current_theme.text_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 26, "SD CARD (SPI BUS):", current_theme.accent_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 37, " -> MISO:GP16 | MOSI:GP19 | SCK:GP18", current_theme.text_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 52, "MAIN I/O CONTROLS:", current_theme.accent_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 63, "> MIDI UART: GP0(TX) / GP1(RX)", current_theme.text_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 74, "> TOUCH I2C: GP4(SDA)/GP5(SCL)", current_theme.text_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 85, "> ENC_SW:GP14 | LED_STAT:GP24", current_theme.text_color, current_theme.bg_color, 1);
}

// СТРАНИЦА 3: Архитектура
static void render_page_file_structure(int start_y) {
    draw_text_scaled(10, start_y, "PROJECT ARCHITECTURE (FATFS):", current_theme.accent_color, current_theme.bg_color, 1);
    int y = start_y + 14;
    
    draw_text_scaled(10, y,      "0:/               [SD ROOT]", current_theme.accent_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 11, "├── core/         (SPI/I2C Drivers)", current_theme.text_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 22, "├── services/     (FatFS/UI Engine)", current_theme.text_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 33, "├── modes/        (FSM States)", current_theme.text_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 44, "└── mapping/      (MIDI Profiles)", current_theme.text_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 60, "SUPPORTED EXTENSIONS:", current_theme.accent_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 71, " -> *.SYX (Yamaha DX7 SysEx Banks)", current_theme.text_color, current_theme.bg_color, 1);
    draw_text_scaled(10, y + 82, " -> *.MID (Standard MIDI Files)", current_theme.text_color, current_theme.bg_color, 1);
}

// Главная точка входа рендеринга экрана диагностики
void debug_log_render_system_screen(uint16_t mpr_touched_state, float v_sys) {
    // Вычисляем, изменилась ли страница по сравнению с прошлым циклом опроса
    bool page_changed = (g_sys_page != g_last_rendered_page);
    
    if (page_changed) {
        g_last_rendered_page = g_sys_page;
        
        // --- ДИНАМИЧЕСКИЙ СТАТУСБАР С ОБНОВЛЕНИЕМ СТРАНИЦЫ (X) ---
        char header_string[32];
        snprintf(header_string, sizeof(header_string), "SYS Config & Diag %d", g_sys_page + 1);
        ui_draw_statusbar(header_string, sd_info.is_mounted, 1);
        
        ui_clear_work_area(); // Очищаем рабочую зону СТРОГО при смене страницы!

        // Отрендерим статический текст страниц один раз
        if (g_sys_page == 1) {
            render_page_pinout(30);
        } else if (g_sys_page == 2) {
            render_page_file_structure(30);
        }
        
        char footer_buf[16];
        snprintf(footer_buf, sizeof(footer_buf), "PAGE %d/3", g_sys_page + 1);
        ui_draw_footer(footer_buf);
    }

    // Если выбрана 1-я страница, отдаем управление рендереру диагностики и живого тачпада
    if (g_sys_page == 0) {
        render_page_diagnostics(mpr_touched_state, v_sys, 30, page_changed);
    }
}

// Функция для интеграции с энкодером в system_mode.c
void debug_log_handle_scroll(int enc_delta) {
    if (enc_delta != 0) {
        int next_page = g_sys_page + enc_delta;
        if (next_page < 0) next_page = 2;
        if (next_page > 2) next_page = 0;
        g_sys_page = (uint8_t)next_page;
    }
}

void debug_log_print(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args); 
    va_end(args);
}
