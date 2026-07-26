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
    
    // Безопасный сброс: вычитываем и выбрасываем мусор, 
    // если он застрял в аппаратном FIFO приемника из-за наводок
    while (spi_is_readable(SD_SPI_PORT)) {
        (void)spi_get_hw(SD_SPI_PORT)->dr;
    }

    // Ваша оригинальная рабочая строка передачи данных
    spi_write_read_blocking(SD_SPI_PORT, &data, &out, 1);
    
    return out;
}


// Отправка команды SD-карте
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

    // Чистый оригинальный цикл, но увеличиваем лимит итераций до 255 под скорость RP2350
    n = 255; 
    do {
        res = spi_xfer(0xFF);
    } while ((res & 0x80) && --n);

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

    // Сначала инициализируем SPI на низкой скорости (400 кГц)
    sd_spi_init();

    // 1. Подаем 80 холостых тактов SPI (10 байт 0xFF) при выключенном CS
    sd_cs_deselect();
    for (int n = 0; n < 10; n++) spi_xfer(0xFF);

    // 2. Опрашиваем карту (Внутри send_cmd CS сам включится и выключится)
    if (send_cmd(0, 0) == 1) { // Карта перешла в Idle state
        ty = 0;
        if (send_cmd(8, 0x1AA) == 1) { // Проверка SDv2 (поддержка больших карт)
            // Перед чтением регистра OCR нужно активировать CS, так как send_cmd его уже выключил
            sd_cs_select(); 
            for (int n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
            sd_cs_deselect(); // Выключаем после чтения байт

            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                // Инициализация карт высокой емкости (SDHC/SDXC) через ACMD41
                for (tmr = 1000; tmr; tmr--) {
                    if (send_cmd(0x80 | 41, 0x40000000) == 0) break;
                    sleep_ms(1);
                }
                if (tmr && send_cmd(58, 0) == 0) { // CMD58: Чтение OCR
                    sd_cs_select(); // Активируем для чтения регистра
                    for (int n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
                    sd_cs_deselect(); // Выключаем
                    
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
    
    // Здесь sd_cs_deselect() больше не нужен, так как команды закрыты
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
