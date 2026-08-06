#include "hw_config.h"
#include "modes.h"
#include "ui_engine.h"
#include "debug_log.h"
#include "TFT_dvr.h"
#include <stdio.h>

static uint8_t sys_page_idx = 0;
#define SYS_TOTAL_PAGES 5

// Описание технического сервисного контента
static void draw_sys_p1_hardware_stats(void) { /* Живые вольты, такты, геометрия SD */ }
static void draw_sys_p2_mpr121_reassign(void) { /* Кастомизация переменных тачпада */ }
static void draw_sys_p3_blackbox_menu(void) { /* Сервисное меню логов USB Trace / SD BlackBox */ }
static void draw_sys_p4_project_struct(void) { /* Структура исходного кода прошивки из README.adoc */ }
static void draw_sys_p5_pinout(void) { /* Аппаратный справочник hw_config.h */ }

// Упорядоченный массив страниц SERVICE-интерфейса
static void (*sys_pages[SYS_TOTAL_PAGES])(void) = {
    draw_sys_p1_hardware_stats,
    draw_sys_p2_mpr121_reassign,
    draw_sys_p3_blackbox_menu,
    draw_sys_p4_project_struct,
    draw_sys_p5_pinout
};

void system_mode_render(void) {
    ui_render_mode_layout("SYS Config", sys_page_idx, SYS_TOTAL_PAGES, sys_pages[sys_page_idx]);
}

void system_mode_update(uint16_t touched, int enc_delta) {
    // Листание страниц инженером по sys_page_idx (от 0 до 4)
    // И логика внутренних изменений переменных кастомизации / переключателей логов...
}
