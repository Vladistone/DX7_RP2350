#ifndef SD_STORAGE_H
#define SD_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "ui_theme.h"

#define SD_MAX_FILES        64
#define SD_MAX_FILENAME_LEN 32

// Типы элементов для цветового кодирования на TFT LCD
typedef enum {
    FILE_TYPE_FOLDER,      // Папки (Желтый цвет)
    FILE_TYPE_MIDI_SYSEX,  // MIDI/SysEx патчи (Синий/Cyan цвет)
    FILE_TYPE_OTHER        // Конфиги, маппинги и т.д. (Серый цвет)
} sd_file_type_t;

// Элемент списка файлов
typedef struct {
    char name[SD_MAX_FILENAME_LEN];
    sd_file_type_t type;
} sd_file_entry_t;

// Главный контекст накопителя
typedef struct {
    bool is_mounted;
    uint16_t file_count;
    sd_file_entry_t files[SD_MAX_FILES];
} sd_storage_t;

// Глобальный экземпляр данных SD-карты
extern sd_storage_t sd_info;

// Базовые функции управления SD-картой и файлами
bool sd_storage_mount(void);
bool sd_ensure_ready(void);
bool sd_storage_load_theme(const char* theme_filename);
bool sd_storage_scan_files(const char* dir_path);
int32_t sd_storage_read_file(const char* filepath, uint8_t* buffer, uint32_t max_len);

// Получение текущей активной цветовой схемы темы
const UI_Theme sd_storage_get_current_theme(void);

#endif // SD_STORAGE_H