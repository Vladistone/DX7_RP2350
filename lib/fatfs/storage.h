#ifndef STORAGE_H
#define STORAGE_H

#include "ff.h"
#include "diskio.h"
#include "sd_storage.h" // Подключаем реальный менеджер SD-карты

// Номера физических дисков (должны соответствовать тем, что используются в switch-case в diskio.c)
#define DEV_RAM     0
#define DEV_MMC     1
#define DEV_USB     2

bool sd_ensure_ready(void);
int32_t sd_storage_read_file(const char* filepath, uint8_t* buffer, uint32_t max_len);

// Прототипы функций для RAM-диска
DSTATUS RAM_disk_status(void);
DSTATUS RAM_disk_initialize(void);
DRESULT RAM_disk_read(BYTE *buff, LBA_t sector, UINT count);
DRESULT RAM_disk_write(const BYTE *buff, LBA_t sector, UINT count);
DRESULT RAM_disk_ioctl(BYTE cmd, void *buff);

// Прототипы функций для MMC / SD-карты
DSTATUS MMC_disk_status(void);
DSTATUS MMC_disk_initialize(void);
DRESULT MMC_disk_read(BYTE *buff, LBA_t sector, UINT count);
DRESULT MMC_disk_write(const BYTE *buff, LBA_t sector, UINT count);
DRESULT MMC_disk_ioctl(BYTE cmd, void *buff);

// Прототипы функций для USB-диска
DSTATUS USB_disk_status(void);
DSTATUS USB_disk_initialize(void);
DRESULT USB_disk_read(BYTE *buff, LBA_t sector, UINT count);
DRESULT USB_disk_write(const BYTE *buff, LBA_t sector, UINT count);
DRESULT USB_disk_ioctl(BYTE cmd, void *buff);

#endif // STORAGE_H