#ifndef TOUCHPAD_DRIVER_H
#define TOUCHPAD_DRIVER_H

#include <stdint.h>

// Маппинг 12 сенсорных кнопок MPR121 (ELE0 - ELE11)
#define MPR_CUR_UP   0  // Курсор ВВЕРХ
#define MPR_DISK_UP  1  // Образ диска ВВЕРХ
#define MPR_DISK_DN  2  // Образ диска ВНИЗ
#define MPR_CUR_DN   3  // Курсор ВНИЗ
#define MPR_ESC      4  // ESCAPE
#define MPR_MODE     5  // Смена MODE
#define MPR_SELECT   6  // SELECT
#define MPR_ENTER    7  // ENTER
#define MPR_STOP     8  // STOP / PAUSE
#define MPR_PLAY     9  // PLAY
#define MPR_FF       10 // Fast Forward (Удержание)
#define MPR_RW       11 // Rewind (Удержание)

void mpr121_init(void);
uint16_t mpr121_read_touched(void);

#endif // TOUCHPAD_DRIVER_H