#ifndef SD_REVIEW_H
#define SD_REVIEW_H

#include <stdint.h>
#include <stdbool.h>

// ====================================================================
// ИНТЕРФЕЙСНЫЕ МЕТОДЫ РЕЖИМА БРАУЗЕРА SD (ДЛЯ ВЫЗОВА ИЗ MAIN)
// ====================================================================

// Функция инициализации режима браузера (сброс индексов, проверка монтирования)
void sd_review_init(void);

// Основная функция обновления состояния режима (обработка энкодера и тача)
void sd_review_update(uint16_t touched, int enc_delta);

// Отрисовка списка файлов на экран на базе ui_render_mode_layout
void sd_review_render(void);

// Отправка текущего выбранного файла по UART MIDI в синтезатор DX7
bool sd_review_send_current_file(void);

#endif // SD_REVIEW_H
