#include "modes.h"
#include "debug_log.h"
#include "numpad_dvr.h"

void system_mode_render(void) {
    // Вход в меню диагностики
    uint16_t touched = mpr121_read_touched();
    debug_log_render_system_screen(touched, 5.01f); // 5.01V от DX7
}

void system_mode_update(uint16_t touched) {
    // Обновление в реальном времени при нажатии на кнопки
    debug_log_render_system_screen(touched, 5.01f);
}