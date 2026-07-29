#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ff.h"
#include "diskio.h"
#include "hw_config.h"
#include "sd_card.h"

#define DEV_MMC         0
#define SD_CMD_TIMEOUT  10000

static BYTE spi_xfer(BYTE data) {
    BYTE out;
    
    // АППАРАТНАЯ ЗАЩИТА ОТ ЗАВИСАНИЯ SPI ПОД RP2350:
    // Если буфер FIFO приемника забит мусором из-за наводок при выключенном CS, 
    // принудительно вычищаем его перед началом новой транзакции.
    while (spi_is_readable(SD_SPI_PORT)) {
        (void)spi_get_hw(SD_SPI_PORT)->dr; // читаем регистр данных в пустоту
    }

    // Теперь блокирующий вызов SDK отработает абсолютно безопасно
    spi_write_read_blocking(SD_SPI_PORT, &data, &out, 1);
    return out;
}

static BYTE send_cmd(BYTE cmd, DWORD arg) {
    BYTE res;
    int n;

    if (cmd & 0x80) { 
        cmd &= 0x7F;
        res = send_cmd(55, 0);
        if (res > 1) return res;
    }

    sd_cs_deselect();
    spi_xfer(0xFF);
    sd_cs_select();
    spi_xfer(0xFF);

    spi_xfer(cmd | 0x40);
    spi_xfer((BYTE)(arg >> 24));
    spi_xfer((BYTE)(arg >> 16));
    spi_xfer((BYTE)(arg >> 8));
    spi_xfer((BYTE)arg);

    n = 0x01;
    if (cmd == 0) n = 0x95;
    if (cmd == 8) n = 0x87;
    spi_xfer(n);

    // НАДЁЖНЫЙ ВАРИАНТ ТАЙМАУТА ДЛЯ БЫСТРОГО ЯДРА RP2350
    n = 2000; // Даем карте до 20 миллисекунд общего времени на ответ
    do {
        res = spi_xfer(0xFF);
        if (res == 0x01 || res == 0x00) break; // Если пришел валидный ответ R1 — выходим!
        sleep_us(10); // <--- Физическая микропауза между байтами для 150 МГц чипа
    } while (--n);

    sd_cs_deselect();
    return res;
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != DEV_MMC) return STA_NOINIT;
    return 0;
}

DSTATUS disk_initialize(BYTE pdrv) {
    BYTE ty, ocr[4];
    UINT tmr;

    if (pdrv != DEV_MMC) return STA_NOINIT;

    // Инициализация аппаратного SPI1
    sd_spi_init();

    // Очищаем аппаратный буфер SPI1 от стартового мусора дисплея
    while (spi_is_readable(SD_SPI_PORT)) {
        (void)spi_get_hw(SD_SPI_PORT)->dr;
    }

    // 1. Подаем 80 холостых тактов при выключенном CS
    sd_cs_deselect();
    for (int n = 0; n < 15; n++) spi_xfer(0xFF);

    // Пауза стабилизации, чтобы внутренний чип карты "проснулся"
    sleep_ms(10); 

    // 2. Отправляем CMD0 (Внутри send_cmd CS включится и выключится сам)
    if (send_cmd(0, 0) == 1) { 
        ty = 0;
        
        // Отправляем CMD8
        if (send_cmd(8, 0x1AA) == 1) {
            // КРИТИЧНО: Включаем CS вручную, так как send_cmd его уже выключил,
            // а нам нужно прочитать 4 байта регистра OCR напрямую из шины!
            sd_cs_select(); 
            for (int n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
            sd_cs_deselect(); // Выключаем после чтения

            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                for (tmr = 1000; tmr; tmr--) {
                    if (send_cmd(0x80 | 41, 0x40000000) == 0) break;
                    sleep_ms(1);
                }
                if (tmr && send_cmd(58, 0) == 0) {
                    sd_cs_select(); // Включаем для чтения OCR
                    for (int n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
                    sd_cs_deselect();
                    
                    ty = (ocr[0] & 0x40) ? 12 : 4; // SDv2 (Block) или SDv2 (Byte)
                }
            }
        } else { // Старые карты SDv1 или MMC
            ty = (send_cmd(0x80 | 41, 0) <= 1) ? 2 : 1;
            for (tmr = 1000; tmr; tmr--) {
                if (send_cmd(0x80 | 41, 0) == 0) break;
                if (ty == 1 && send_cmd(1, 0) == 0) break;
                sleep_ms(1);
            }
            if (!tmr || send_cmd(16, 512) != 0) ty = 0;
        }
    }

    sd_cs_deselect();
    spi_xfer(0xFF); // Холостой такт завершения транзакции

    if (ty) {
        // Карта успешно дала свой тип! Включаем рабочую скорость (2 МГц)
        sd_spi_set_high_speed();
        return 0; // RES_OK
    }

    return STA_NOINIT;
}

DRESULT disk_read (
    BYTE pdrv,    /* Physical drive nmuber (0..) */
    BYTE *buff,   /* Data buffer to store read data */
    LBA_t sector, /* Safe sector address */
    UINT count    /* Sector count (1..128) */
)
{
    if (pdrv != DEV_MMC || !count) return RES_PARERR;
    // ВАЖНО ДЛЯ RP2350: Короткая пауза, если контроллер карты 
    // еще не переварил переключение скорости в sd_spi_set_high_speed
    sleep_us(5);
    
    sd_cs_select();

    DWORD token;
    if (count == 1) {
        if (send_cmd(17, sector) == 0) {
            uint32_t timeout = SD_CMD_TIMEOUT;
            do {
                token = spi_xfer(0xFF);
            } while (token == 0xFF && --timeout);

            if (token == 0xFE) {
                spi_read_blocking(SD_SPI_PORT, 0xFF, buff, 512);
                spi_xfer(0xFF); spi_xfer(0xFF);
                count = 0;
            }
        }
    } else {
        if (send_cmd(18, sector) == 0) {
            do {
                uint32_t timeout = SD_CMD_TIMEOUT;
                do {
                    token = spi_xfer(0xFF);
                } while (token == 0xFF && --timeout);

                if (token != 0xFE) break;
                spi_read_blocking(SD_SPI_PORT, 0xFF, buff, 512);
                spi_xfer(0xFF); spi_xfer(0xFF);
                buff += 512;
            } while (--count);
            send_cmd(12, 0);
        }
    }

    sd_cs_deselect();
    spi_xfer(0xFF);

    return count ? RES_ERROR : RES_OK;
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != DEV_MMC || !count) return RES_PARERR;

    sd_cs_select();

    if (count == 1) {
        if (send_cmd(24, sector) == 0) {
            spi_xfer(0xFF); spi_xfer(0xFE);
            spi_write_blocking(SD_SPI_PORT, buff, 512);
            spi_xfer(0xFF); spi_xfer(0xFF);
            if ((spi_xfer(0xFF) & 0x1F) == 0x05) {
                while (spi_xfer(0xFF) == 0);
                count = 0;
            }
        }
    } else {
        if (send_cmd(25, sector) == 0) {
            do {
                spi_xfer(0xFF); spi_xfer(0xFC);
                spi_write_blocking(SD_SPI_PORT, buff, 512);
                spi_xfer(0xFF); spi_xfer(0xFF);
                if ((spi_xfer(0xFF) & 0x1F) != 0x05) break;
                while (spi_xfer(0xFF) == 0);
                buff += 512;
            } while (--count);
            spi_xfer(0xFD);
            while (spi_xfer(0xFF) == 0);
        }
    }

    sd_cs_deselect();
    spi_xfer(0xFF);

    return count ? RES_ERROR : RES_OK;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != DEV_MMC) return RES_PARERR;
    switch (cmd) {
        case CTRL_SYNC:
            sd_cs_select();
            if (spi_xfer(0xFF) != 0xFF) {
                sd_cs_deselect();
                return RES_ERROR;
            }
            sd_cs_deselect();
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}