#include "modes.h"
#include "debug_log.h"
#include "numpad_dvr.h"
#include "ui_engine.h"   
#include "encoder_dvr.h"  // КРИТИЧНО: подключаем ваш родной хедер энкодера

// Прототип функции смены страниц из debug_log.c (чтобы компилятор её видел)
// void system_mode_update(uint16_t touched, int enc_delta);

// Функция инициализации режима диагностики (вызывается один раз при входе)
void system_mode_init(void) {
    ui_clear_work_area(); // Стираем старый экран браузера файлов
}

void system_mode_render(void) {
    uint16_t touched = mpr121_read_touched(); 
    debug_log_render_system_screen(touched, 5.01f); 
}

    // ВАЖНО: В проекте функция чтения вращения вызывается БЕЗ аргументов.
    // Если в main.c дельта уже передается глобально, можно использовать её,
    // но если читаем напрямую из драйвера — используем encoder_read_delta() или ваш аналог.
    // Для совместимости с логикой sd_review заведем локальную переменную:
    // ИСПРАВЛЕНО: Теперь функция принимает и touched, и enc_delta из главного цикла main.c
void system_mode_update(uint16_t touched, int enc_delta) {
    
    // Передаем реальное вращение энкодера в менеджер страниц debug_log.c
    if (enc_delta != 0) {
        debug_log_handle_scroll(enc_delta);
    }

    // Обновляем экран с учетом новой выбранной страницы
    debug_log_render_system_screen(touched, 5.01f); 
}