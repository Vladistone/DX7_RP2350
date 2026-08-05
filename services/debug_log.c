#include "hw_config.h"

#if DEBUG_GLOBAL_ENABLE
#include <stdarg.h>
#include <string.h>
#include "ff.h"
#include "sd_storage.h" // Подключите ваш заголовок структуры sd_info

// По умолчанию при включении синтезатора всё ВЫКЛЮЧЕНО, чтобы не мешать DAW/MIDI
volatile bool g_cli_debug_usb_active = false;
volatile bool g_cli_debug_sd_active = false;

static uint64_t last_user_click_time_us = 0;
static bool sd_debug_file_ready = false;

void debug_chrono_init(void) {
    last_user_click_time_us = time_us_64();
    sd_debug_file_ready = false;

    // Если в GUI включили запись на SD, инициализируем файл
    if (g_cli_debug_sd_active && sd_info.is_mounted) {
        FIL f;
        if (f_open(&f, DEBUG_SD_FILE_NAME, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
            const char* header = "=== DX7 RP2350 SERVICE TRACE START ===\n";
            UINT written;
            f_write(&f, header, strlen(header), &written);
            f_close(&f);
            sd_debug_file_ready = true;
        }
    }
    SD_LOG("Service Log Engine Started.");
}

void debug_file_log_write(const char* fmt, ...) {
    if (!g_cli_debug_sd_active || !sd_info.is_mounted) return;
    
    // Внутренняя ленивая инициализация файла, если его забыли создать при старте
    if (!sd_debug_file_ready) {
        FIL f;
        if (f_open(&f, DEBUG_SD_FILE_NAME, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
            f_close(&f);
            sd_debug_file_ready = true;
        }
    }

    char log_buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_buf, sizeof(log_buf), fmt, args);
    va_end(args);

    FIL f;
    if (f_open(&f, DEBUG_SD_FILE_NAME, FA_WRITE | FA_OPEN_APPEND) == FR_OK) {
        UINT written;
        f_write(&f, log_buf, strlen(log_buf), &written);
        f_close(&f);
    }
}

void debug_chrono_user_action(const char* control_name) {
    uint64_t now = time_us_64();
    uint64_t idle_ms = (now - last_user_click_time_us) / 1000ULL;
    last_user_click_time_us = now;

    SD_LOG("--> [HW_INPUT] Action: '%s' (Device IDLE: %llu ms)", control_name, idle_ms);
}

void debug_chrono_sd_op(const char* op, const char* path) {
    uint64_t now = time_us_64();
    uint64_t delta_ms = (now - last_user_click_time_us) / 1000ULL;

    // Если обращение к SD произошло мгновенно после клика (меньше 15мс) - это ручной выбор папки
    if (delta_ms < 15ULL) {
        SD_LOG("[FATFS] %s on '%s' (STATUS: USER_REQUEST, delay: %llu ms)", op, path, delta_ms);
    } else {
        // Если дельта большая - код ушел читать подпапку самостоятельно (наш баг!)
        SD_LOG("[FATFS] [!! AUTO-DIVE BUG !!] %s on '%s' (STATUS: AUTONOMOUS, elapsed: %llu ms)", op, path, delta_ms);
    }
}
#endif
