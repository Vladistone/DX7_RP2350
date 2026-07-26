#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include <stdint.h>

// Инициализация низкоуровневой шины SPI для SD-карты
void sd_spi_init(void);

// Переключение шины SPI на высокую скорость (после успешной инициализации карты)
void sd_spi_set_high_speed(void);

// Управление пином CS (Chip Select)
void sd_cs_select(void);
void sd_cs_deselect(void);

#endif // SD_CARD_H