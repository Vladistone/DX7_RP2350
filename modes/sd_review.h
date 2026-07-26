#ifndef SD_REVIEW_H
#define SD_REVIEW_H

#include <stdint.h>
#include <stdbool.h>

// Максимальное количество файлов .SYX в списке
#define SD_MAX_FILES        64
#define SD_MAX_FILENAME_LEN 32

// Функция инициализации и монтирования SD-карты
bool sd_review_init(void);

// Основная функция обновления состояния режима
void sd_review_update(uint16_t touched, int enc_delta);

// Отрисовка списка файлов на экран (LCD / OLED)
void sd_review_render(void);

// Отправка текущего выбранного файла в DX7
bool sd_review_send_current_file(void);

#endif // SD_REVIEW_H