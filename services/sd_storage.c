#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "sd_storage.h"
#include "ui_engine.h"
#include "SD_card.h"
#include "ff.h"
#include "debug_log.h"

// Глобальная переменная состояния SD
sd_storage_t sd_info = { .is_mounted = false, .file_count = 0 };

static FATFS fs;          // Рабочая область FatFS
static FATFS g_fatfs;
static bool g_sd_mounted = false;
static void sd_storage_ensure_defaults(void);

// Возвращает фиксированное время (например, 1 июля 2026 года, 00:00:00)
DWORD get_fattime(void) {
    // Формат FatFS: 
    // Год (отсчет от 1980): бит 31-25 (2026 - 1980 = 46)
    // Месяц: бит 24-21 (7)
    // День: бит 20-16 (1)
    // Часы: бит 15-11 (0)
    // Минуты: бит 10-5 (0)
    // Секунды / 2: бит 4-0 (0)
    return ((DWORD)(2026 - 1980) << 25) | 
           ((DWORD)7 << 21) | 
           ((DWORD)1 << 16) | 
           ((DWORD)0 << 11) | 
           ((DWORD)0 << 5) | 
           ((DWORD)0 >> 1);
}

// Функция инициализации / повторного монтирования
bool sd_storage_init(void) {
    FRESULT res;

    // Опционально: проверка физического пина детекта карты (Card Detect), если он заведен на железо
    // if (!gpio_get(SD_DETECT_PIN)) { g_sd_mounted = false; return false; }

    // Пробуем смонтировать диск 0
    res = f_mount(&g_fatfs, "", 1); // 1 = монтировать немедленно
    if (res == FR_OK) {
        g_sd_mounted = true;
        printf("SD mounted!\n");
        return true;
    } else {
        g_sd_mounted = false;
        printf("SD mount err: %d\n", res);
        return false;
    }
}

// Универсальная обертка для обработки ошибок FatFS
FRESULT sd_safe_operation(FRESULT res) {
    if (res == FR_NOT_READY || res == FR_DISK_ERR || res == FR_INT_ERR || res == FR_NOT_ENABLED) {
        if (g_sd_mounted) {
            printf("SD Card error (%d). SD might to remove\n", res);
            g_sd_mounted = false;
            // Размонтируем корректно без паники
            f_mount(NULL, "", 0);
        }
    }
    return res;
}

// Пример функции обновления/проверки в фоновом цикле или при попытке доступа
bool sd_ensure_ready(void) {
    if (!g_sd_mounted) {
        // Пытаемся переподключить карту (например, раз в несколько секунд или по событию)
        return sd_storage_init();
    }
    return true;
}

// Вспомогательная функция: проверка расширения файла (без учета регистра)
static bool has_extension(const char *filename, const char *ext) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return false;
    
    while (*dot && *ext) {
        if (tolower((unsigned char)*dot) != tolower((unsigned char)*ext)) {
            return false;
        }
        dot++;
        ext++;
    }
    return (*dot == '\0' && *ext == '\0');
}

// Монтирование накопителя
bool sd_storage_mount(void) {
    sd_spi_init(); // Низкоуровневый SPI из core/SD_card.h

    FRESULT res = f_mount(&fs, "", 0); // 0 — зарегистрировать диск без немедленного чтения секторов
        if (res != FR_OK) {
            printf("[ERROR] SD Mount registration failed: %d\n", res);
        } else {
            printf("[INIT] SD Volume registered (Lazy Mount)\n");
    }// Используем объявленную выше статику `fs`
    if (res == FR_OK) {
        sd_info.is_mounted = true;
        sd_spi_set_high_speed(); // Перевод SPI на рабочую частоту
        debug_log_print("SD mounted success.\n");

        // Запускаем автосоздание недостающих конфигураций и папок
        sd_storage_ensure_defaults();

        return true;
    }

    sd_info.is_mounted = false;
    debug_log_print("SD mount failed!\n");
    return false;
}

// Сканирование директории и наполнение списка sd_info
bool sd_storage_scan_files(const char* dir_path) {
    sd_info.file_count = 0;
    if (!sd_info.is_mounted) return false;

    DIR dir;
    FILINFO fno;
    FRESULT res = f_opendir(&dir, dir_path);

    if (res != FR_OK) {
        return false;
    }

    // 1. Добавляем переход на уровень вверх, если мы не в корне корневой папки
    if (strcmp(dir_path, "/") != 0 && strcmp(dir_path, "") != 0) {
        snprintf(sd_info.files[0].name, SD_MAX_FILENAME_LEN, ".. [UP]");
        sd_info.files[0].type = FILE_TYPE_FOLDER;
        sd_info.file_count++;
    }

    // 2. Чтение списка файлов и папок
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
        // Пропускаем скрытые и системные файлы
        if (fno.fname[0] == '.' || (fno.fattrib & AM_SYS) || (fno.fattrib & AM_HID)) {
            continue;
        }

        // Защита от переполнения массива
        if (sd_info.file_count >= SD_MAX_FILES) {
            break;
        }

        uint16_t idx = sd_info.file_count;
        snprintf(sd_info.files[idx].name, SD_MAX_FILENAME_LEN, "%s", fno.fname);

        // Определение типа для цветовой индикации
        if (fno.fattrib & AM_DIR) {
            sd_info.files[idx].type = FILE_TYPE_FOLDER;
        } 
        else if (has_extension(fno.fname, ".syx") || has_extension(fno.fname, ".mid")) {
            sd_info.files[idx].type = FILE_TYPE_MIDI_SYSEX;
        } 
        else {
            sd_info.files[idx].type = FILE_TYPE_OTHER;
        }

        sd_info.file_count++;
    }

    f_closedir(&dir);
    return true;
}

// Чтение SysEx файла с SD-карты в указанный буфер
int32_t sd_storage_read_file(const char* filepath, uint8_t* buffer, uint32_t max_len) {
    if (!sd_info.is_mounted) return -1;

    FIL file;
    FRESULT res = f_open(&file, filepath, FA_READ);
    if (res != FR_OK) return -1;

    UINT bytes_read = 0;
    res = f_read(&file, buffer, max_len, &bytes_read);
    f_close(&file);

    return (res == FR_OK) ? (int32_t)bytes_read : -1;
}

bool sd_storage_load_theme(const char* theme_filename) {
    uint8_t buffer[64];
    int32_t bytes_read = sd_storage_read_file(theme_filename, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
        // Файл не найден или пуст — используем дефолтную цветовую схему (безопасный fallback)
        current_theme.bg_color       = 0x0000; // Черный
        current_theme.text_color     = 0xFFFF; // Белый
        current_theme.accent_color   = 0x07FF; // Ярко-голубой
        current_theme.bar_bg_color   = 0x18E3; // Темно-серый
        current_theme.bar_text_color = 0xFFFF; // Белый
        
        // (Опционально) Можем сразу создать дефолтный файл на SD-карте, чтобы он там был
        return false; 
    }

    // Если файл прочитался — парсим его значения (в зависимости от вашего формата: текст или бинарник)
    // ... парсинг ...

    return true;
}

// Внутренняя функция для создания дефолтных файлов, если они отсутствуют
static void sd_storage_ensure_defaults(void) {
    FRESULT res;
    FIL file;
    UINT bw;

    // 1. Создаем папку для маппинга контроллеров, если её нет
    res = f_mkdir("map");
    if (res == FR_OK || res == FR_EXIST) {
        debug_log_print("SD: Directory 'map' is ready.\n");
    }

    // 2. Проверяем и создаем theme.cfg по умолчанию
    res = f_stat("theme.cfg", NULL);
    if (res != FR_OK) {
        // Файла нет — создаем автоматически
        res = f_open(&file, "theme.cfg", FA_CREATE_ALWAYS | FA_WRITE);
        if (res == FR_OK) {
            const char* default_theme_data = 
                "# Yamaha DX7 Controller Theme Configuration\n"
                "BG_COLOR=0x0000\n"
                "TEXT_COLOR=0xFFFF\n"
                "ACCENT_COLOR=0x07FF\n"
                "BAR_BG_COLOR=0x18E3\n"
                "BAR_TEXT_COLOR=0xFFFF\n";
            
            f_write(&file, default_theme_data, (UINT)strlen(default_theme_data), &bw);
            f_close(&file);
            debug_log_print("SD: Created default 'theme.cfg'.\n");
        }
    }

    // 3. Создаем дефолтный файл маппинга (например, map/default.map)
    res = f_stat("map/default.map", NULL);
    if (res != FR_OK) {
        res = f_open(&file, "map/default.map", FA_CREATE_ALWAYS | FA_WRITE);
        if (res == FR_OK) {
            const char* default_map_data = 
                "# Default MIDI CC Mapping Profile\n"
                "CH=1\n"
                "CC_ENCODER_1=16\n"
                "CC_ENCODER_2=17\n";
            f_write(&file, default_map_data, (UINT)strlen(default_map_data), &bw);
            f_close(&file);
            debug_log_print("SD: Created 'map/default.map'.\n");
        }
    }

    // 4. Создаем профиль SSL Nucleus 2 (map/nucleus2.map)
    res = f_stat("map/nucleus2.map", NULL);
    if (res != FR_OK) {
        res = f_open(&file, "map/nucleus2.map", FA_CREATE_ALWAYS | FA_WRITE);
        if (res == FR_OK) {
            const char* nucleus_map_data = 
                "# SSL Nucleus 2 HUI/MIDI Profile\n"
                "CH=2\n"
                "CC_ENCODER_1=32\n"
                "CC_ENCODER_2=33\n";
            f_write(&file, nucleus_map_data, (UINT)strlen(nucleus_map_data), &bw);
            f_close(&file);
            debug_log_print("SD: Created 'map/nucleus2.map'.\n");
        }
    }
}