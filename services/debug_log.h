#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include "pico/stdlib.h"
#include <stdbool.h>

// ====================================================================
// КОНФИГУРАЦИЯ СИСТЕМЫ ОТЛАДКИ (ПРАВКА 1: Главные переключатели)
// ====================================================================
#define SYSTEM_DEBUG_MODE     1  // 1 = Общая отладка включена, 0 = Полное вырезание (Zero Overhead)
#define ENABLE_CHRONO_TRACK   1  // 1 = Логировать хронометраж переходов, 0 = Выключить только хроно
#define LOG_TO_SD_AUTONOMOUS  1  // 2 = Запись логов в файл на SD-карту (Автономный режим)

// Имя файла для автономного логирования
#define DEBUG_LOG_FILE_NAME "0:/sys_trace.log"

// ====================================================================
// ОБЕРТКИ ДЛЯ СИСТЕМНОГО PRINTF (ПРАВКА 3)
// ====================================================================
#if SYSTEM_DEBUG_MODE
    extern volatile bool g_debug_logs_enabled; // Флаг активности логов реального времени
    extern volatile bool g_sd_logging_enabled;  // Флаг записи на SD

    // Основная функция логирования с форматированием (работает с USB и SD)
    void debug_log_write(const char* level, const char* format, ...);

    // Умные макросы для разных уровней логирования
    #define DBG_LOG(fmt, ...)  debug_log_write("INFO", fmt, ##__VA_ARGS__)
    #define DBG_WARN(fmt, ...) debug_log_write("WARN", fmt, ##__VA_ARGS__)
    #define DBG_ERR(fmt, ...)  debug_log_write("ERROR", fmt, ##__VA_ARGS__)
#else
    // Если отладка отключена, компилятор не сгенерирует ни единой строчки кода
    #define DBG_LOG(fmt, ...)  do {} while(0)
    #define DBG_WARN(fmt, ...) do {} while(0)
    #define DBG_ERR(fmt, ...)  do {} while(0)
#endif

// ====================================================================
// ИНТЕРФЕЙС МОДУЛЯ ОТЛАДКИ
// ====================================================================

// Инициализация подсистемы (открытие файлов, сброс таймеров)
void debug_log_init(void);

// Принудительный сброс буферов записи на SD-карту (вызывать перед выключением)
void debug_log_sync(void);

// ХРОНОМЕТРАЖ (Регистрация физических действий пользователя)
void debug_log_user_action(const char* control_name);

// ХРОНОМЕТРАЖ (Логирование операций FatFS для отлова автоматических прыжков)
void debug_log_sd_op(const char* operation, const char* path);

// Старый прототип для совместимости (если используется в проекте)
void debug_log_print(const char* format, ...);

#endif // DEBUG_LOG_H
