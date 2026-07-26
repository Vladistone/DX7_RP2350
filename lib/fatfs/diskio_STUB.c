/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Basic definitions of FatFs */
#include "diskio.h"		/* Declarations FatFs API */

/* Подключаем ваш заголовок низкоуровневого драйвера SD-карты */
#include "sd_card.h"

/* Определяем физический диск для SD-карты */
#define DEV_MMC		0	/* Map MMC/SD card to physical drive 0 */

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	if (pdrv != DEV_MMC) {
		return STA_NOINIT;
	}

	// Возвращаем статус готовности вашей SD-карты (0 если готова, или STA_NOINIT)
	// Если у вас нет отдельной функции получения статуса в sd_card.h, 
	// возвращаем 0 при успешной инициализации.
	return 0;
}

/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	if (pdrv != DEV_MMC) {
		return STA_NOINIT;
	}

	// Инициализируем SPI и саму SD-карту через ваши функции из sd_card.h
	sd_spi_init();
	
	// По умолчанию возвращаем 0 (RES_OK), если инициализация прошла успешно.
	// При необходимости замените на вызов вашей функции инициализации карты.
	return 0;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if (pdrv != DEV_MMC) {
		return RES_PARERR;
	}

	// Вызов вашей функции чтения секторов с SD-карты.
	// Если в sd_card.h функция называется иначе (например, sd_read_blocks), подставьте её.
	// Пример прямой интеграции (если используется стандартный шаблон):
	// if (sd_read_blocks(buff, sector, count)) return RES_OK;

	return RES_OK; // Замените на реальный результат вызова вашей функции чтения
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if (pdrv != DEV_MMC) {
		return RES_PARERR;
	}

	// Вызов вашей функции записи секторов на SD-карту
	// if (sd_write_blocks(buff, sector, count)) return RES_OK;

	return RES_OK; // Замените на реальный результат вызова вашей функции записи
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive number (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	if (pdrv != DEV_MMC) {
		return RES_PARERR;
	}

	DRESULT res = RES_OK;

	switch (cmd) {
	case CTRL_SYNC:
		// Синхронизация данных (при необходимости)
		break;
	case GET_SECTOR_COUNT:
		// *(LBA_t*)buff = <размер_карты_в_секторах>;
		break;
	case GET_SECTOR_SIZE:
		*(WORD*)buff = 512;
		break;
	case GET_BLOCK_SIZE:
		*(DWORD*)buff = 1;
		break;
	default:
		res = RES_PARERR;
		break;
	}

	return res;
}