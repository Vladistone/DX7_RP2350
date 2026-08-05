#include "debug_log.h"
#include "sd_storage.h" // Для интеграции с FatFS
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if SYSTEM_DEBUG_MODE
volatile bool g_debug_logs_enabled = true; // По умолчанию включено
volatile bool g_sd_logging_enabled = true;  // По умолчанию пишем на SD
#endif

// Хранит время последнего зафиксированного физического прерывания периферии
static uint64_t last_hw_interrupt_time_us = 0;
static bool sd_log_ready = false;

// Внутренний хелпер для сборки строки точного времени [Sec.Ms.Us]
static void get_system_timestamp(char* out_buf, size_t buf_len) {
    uint64_t now_us = time_us_64();
    uint32_t sec = (uint32_t)(now_us / 1000000ULL);
    uint32_t ms  = (uint32_t)((now_us % 1000000ULL) / 1000ULL);
    uint32_t us  = (uint32_t)(now_us % 1000ULL);
    snprintf(out_buf, buf_len, "[%04u.%03u.%03u]", sec, ms, us);
}

void debug_log_init(void) {
    last_hw_interrupt_time_us = time_us_64();
    sd_log_ready = false;

    // ПРАВКА 2: Инициализация файла логирования на SD-карте
    #if LOG_TO_SD_AUTONOMOUS
    if (sd_info.is_mounted) {
        // Пробуем перезаписать или создать файл лога при старте контроллера
        FIL f;
        if (f_open(&f, DEBUG_LOG_FILE_NAME, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
            const char* header = "=== DX7 RP2350 BLACKBOX LOG START ===\n";
            UINT written;
            f_write(&f, header, strlen(header), &written);
            f_close(&f);
            sd_log_ready = true;
        }
    }
    #endif

    DBG_LOG("Debug Engine Active. Mode: USB %s", sd_log_ready ? "+ SD CARD AUTONOMOUS" : "ONLY");
}

// Универсальная инкапсулированная функция вывода (ПРАВКА 3)
void debug_log_write(const char* level, const char* format, ...) {
    // Если логи динамически выключены пользователем — прерываем выполнение
    if (!g_debug_logs_enabled) return;
    char time_str[24];
    char log_buffer[256];
    
    get_system_timestamp(time_str, sizeof(time_str));
    
    // Собираем префикс уровня и времени
    int prefix_len = snprintf(log_buffer, sizeof(log_buffer), "%s[%s]: ", time_str, level);
    
    // Дописываем пользовательское сообщение
    va_list args;
    va_start(args, format);
    vsnprintf(log_buffer + prefix_len, sizeof(log_buffer) - prefix_len, format, args);
    va_end(args);
    
    // Гарантируем перенос строки
    size_t total_len = strlen(log_buffer);
    if (total_len < sizeof(log_buffer) - 2 && log_buffer[total_len - 1] != '\n') {
        strcat(log_buffer, "\n");
    }

    // 1. Вывод в стандартную консоль USB CDC
    printf("%s", log_buffer);

    // 2. Автономный вывод в файл на SD (ПРАВКА 2)
    #if LOG_TO_SD_AUTONOMOUS
    if (g_sd_logging_enabled && sd_log_ready && sd_info.is_mounted) {
        if (sd_log_ready && sd_info.is_mounted) {
            FIL f;
            // Открываем файл в режиме добавления (APPEND)
            if (f_open(&f, DEBUG_LOG_FILE_NAME, FA_WRITE | FA_OPEN_APPEND) == FR_OK) {
                UINT written;
                f_write(&f, log_buffer, strlen(log_buffer), &written);
                f_close(&f); // Закрываем сразу, чтобы данные не пропали при сбросе питания
            }
        }
    }
    #endif
}

// Реализация хронометрии пользовательских нажатий
void debug_log_user_action(const char* control_name) {
    #if ENABLE_CHRONO_TRACK
    uint64_t now = time_us_64();
    uint64_t idle_duration_ms = (now - last_hw_interrupt_time_us) / 1000ULL;
    last_hw_interrupt_time_us = now; // Смещаем точку отсчета

    debug_log_write("CHRONO", "--> [HARDWARE INTERRUPT] User action: '%s' (Device was idle for: %llu ms)", 
                    control_name, idle_duration_ms);
    #endif
}

// Реализация автоматического отслеживания прыжков по папкам (Авто-дайв)
void debug_log_sd_op(const char* operation, const char* path) {
    #if ENABLE_CHRONO_TRACK
    uint64_t now = time_us_64();
    uint64_t time_delta_ms = (now - last_hw_interrupt_time_us) / 1000ULL;

    // Если код обратился к SD-карте быстрее чем за 12 мс после клика — это легитимный переход по команде.
    // Если прошло много времени, а вызов произошел автономно — это баг автоматической рекурсии.
    if (time_delta_ms < 12ULL) {
        debug_log_write("FATFS", "%s on '%s' -> (STATUS: USER EXECUTION, delay: %llu ms)", 
                        operation, path, time_delta_ms);
    } else {
        debug_log_write("FATFS", "[!! AUTO-DIVE BUG !!] %s on '%s' -> (STATUS: AUTONOMOUS TRIGGER, elapsed: %llu ms)", 
                        operation, path, time_delta_ms);
    }
    #endif
}

// Старая обертка для сохранения обратной совместимости
void debug_log_print(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buf[128];
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    debug_log_write("DEBUG", "%s", buf);
}

#endif // SYSTEM_DEBUG_MODE
