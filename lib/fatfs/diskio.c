#include "ff.h"
#include "diskio.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hw_config.h"
#include "sd_card.h" // Для доступа к функциям CS

#define DEV_MMC         0
#define SD_CMD_TIMEOUT  10000

// Низкоуровневая отправка/прием байта по SPI
static BYTE spi_xfer(BYTE data) {
    BYTE out;
    spi_write_read_blocking(SD_SPI_PORT, &data, &out, 1);
    return out;
}

// Отправка команды SD-карте
static BYTE send_cmd(BYTE cmd, DWORD arg) {
    BYTE n, res;

    if (cmd & 0x80) { // ACMD класс команд
        cmd &= 0x7F;
        res = send_cmd(55, 0); // CMD55
        if (res > 1) return res;
    }

    // Ждем готовности карты
    for (int i = 0; i < 1000; i++) {
        if (spi_xfer(0xFF) == 0xFF) break;
        sleep_us(10);
    }

    // Передаем пакет команды (6 байт)
    spi_xfer(0x40 | cmd);
    spi_xfer((BYTE)(arg >> 24));
    spi_xfer((BYTE)(arg >> 16));
    spi_xfer((BYTE)(arg >> 8));
    spi_xfer((BYTE)arg);

    // CRC (контрольная сумма) обязательна только для CMD0 и CMD8
    n = 0x01;
    if (cmd == 0)  n = 0x95; // CRC для CMD0(0)
    if (cmd == 8)  n = 0x87; // CRC для CMD8(0x1AA)
    spi_xfer(n);

    // Ожидаем ответ от карты (R1 ответ)
    n = 10;
    do {
        res = spi_xfer(0xFF);
    } while ((res & 0x80) && --n);

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

    // Сначала инициализируем SPI на низкой скорости (400 кГц)
    sd_spi_init();

    // 1. Подаем 80 холостых тактов SPI (10 байт 0xFF) при выключенном CS
    sd_cs_deselect();
    for (int n = 0; n < 10; n++) spi_xfer(0xFF);

    // 2. Включаем CS и переводим карту в SPI-режим (CMD0)
    sd_cs_select();
    if (send_cmd(0, 0) == 1) { // Карта перешла в Idle state
        ty = 0;
        if (send_cmd(8, 0x1AA) == 1) { // Проверка SDv2 (поддержка больших карт)
            for (int n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                // Инициализация карт высокой емкости (SDHC/SDXC) через ACMD41
                for (tmr = 1000; tmr; tmr--) {
                    if (send_cmd(0x80 | 41, 0x40000000) == 0) break;
                    sleep_ms(1);
                }
                if (tmr && send_cmd(58, 0) == 0) { // CMD58: Чтение OCR
                    for (int n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
                    ty = (ocr[0] & 0x40) ? 12 : 4; // SDv2 (Block) или SDv2 (Byte)
                }
            }
        } else { // Старые карты SDv1 или MMC
            ty = (send_cmd(0x80 | 41, 0) <= 1) ? 2 : 1; // SDv1 или MMC
            for (tmr = 1000; tmr; tmr--) {
                if (send_cmd(0x80 | 41, 0) == 0) break;
                if (ty == 1 && send_cmd(1, 0) == 0) break;
                sleep_ms(1);
            }
            if (!tmr || send_cmd(16, 512) != 0) ty = 0; // Установка размера блока 512 байт
        }
    }
    sd_cs_deselect();
    spi_xfer(0xFF); // Холостой такт

    if (ty) {
        // Карта успешно инициализирована! Переводим SPI на рабочую скорость (12.5 МГц)
        sd_spi_set_high_speed();
        return 0; // RES_OK
    }

    return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != DEV_MMC || !count) return RES_PARERR;

    sd_cs_select();

    // Для SDHC передаем адрес в блоках, для старых карт — в байтах (сектор * 512)
    // (Поскольку сейчас 99% карт — SDHC, используем блочную адресацию)
    DWORD token;
    if (count == 1) { // Чтение одного сектора
        if (send_cmd(17, sector) == 0) { // CMD17
            // Ожидание маркера начала данных (0xFE)
            uint32_t timeout = SD_CMD_TIMEOUT;
            do {
                token = spi_xfer(0xFF);
            } while (token == 0xFF && --timeout);

            if (token == 0xFE) {
                // Читаем 512 байт данных
                spi_read_blocking(SD_SPI_PORT, 0xFF, buff, 512);
                // Пропускаем 2 байта контрольной суммы CRC
                spi_xfer(0xFF); spi_xfer(0xFF);
                count = 0;
            }
        }
    } else { // Множественное чтение секторов
        if (send_cmd(18, sector) == 0) { // CMD18
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
            send_cmd(12, 0); // CMD12: Остановить передачу
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

    if (count == 1) { // Запись одного сектора
        if (send_cmd(24, sector) == 0) { // CMD24
            spi_xfer(0xFF); spi_xfer(0xFE); // Маркер данных
            spi_write_blocking(SD_SPI_PORT, buff, 512); // Пишем 512 байт
            spi_xfer(0xFF); spi_xfer(0xFF); // Пустой CRC
            if ((spi_xfer(0xFF) & 0x1F) == 0x05) { // Проверка ответа карты
                while (spi_xfer(0xFF) == 0); // Ждем окончания внутренней записи
                count = 0;
            }
        }
    } else { // Множественная запись
        if (send_cmd(25, sector) == 0) { // CMD25
            do {
                spi_xfer(0xFF); spi_xfer(0xFC); // Маркер множественной записи
                spi_write_blocking(SD_SPI_PORT, buff, 512);
                spi_xfer(0xFF); spi_xfer(0xFF);
                if ((spi_xfer(0xFF) & 0x1F) != 0x05) break;
                while (spi_xfer(0xFF) == 0);
                buff += 512;
            } while (--count);
            spi_xfer(0xFD); // Маркер остановки STOP TRAN
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
    DRESULT res = RES_OK;
    switch (cmd) {
        case CTRL_SYNC:
            sd_cs_select();
            if (spi_xfer(0xFF) != 0xFF) res = RES_ERROR;
            sd_cs_deselect();
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
