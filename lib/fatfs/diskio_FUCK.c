#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/timer.h"
#include "hardware/spi.h"
#include "ff.h"
#include "diskio.h"
#include "hw_config.h"
#include "sd_card.h"

#define DEV_MMC         0
#define SD_CMD_TIMEOUT  10000

// Низкоуровневая отправка/прием байта по SPI
static BYTE spi_xfer(BYTE data) {
    BYTE out;
    int timeout = 1000;
    
    // Очистка FIFO с таймаутом
    while (spi_is_readable(SD_SPI_PORT) && timeout--) {
        (void)spi_get_hw(SD_SPI_PORT)->dr;
        sleep_us(10);
    }
    
    if (timeout == 0) {
        printf("[SD] SPI FIFO clear timeout\n");
        return 0xFF;
    }
    
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
    
    printf("[SD] disk_initialize start\n");
    if (pdrv != DEV_MMC) {
        printf("[SD] Invalid drive\n");
        return STA_NOINIT;
    }

    printf("[SD] Initializing SPI...\n");
    sd_spi_init();
    printf("[SD] SPI init done\n");

    // --- ОТПРАВКА DUMMY CLOCKS ---
    printf("[SD] Sending dummy clocks...\n");
    sd_cs_deselect();
    for (int n = 0; n < 10; n++) {
        spi_xfer(0xFF);
    }
    printf("[SD] Dummy clocks sent\n");

    // --- ЗАДЕРЖКА 100 мс ---
    printf("[SD] Waiting 100ms...\n");
    sleep_ms(100);
    printf("[SD] Delay done\n");
    
    // --- ЦИКЛ ИНИЦИАЛИЗАЦИИ С ПОВТОРАМИ ---
    int retry = 0;
    bool init_ok = false;
    while (retry < 3 && !init_ok) {
        printf("[SD] Retry %d\n", retry);
        
        sd_cs_deselect();
        sleep_ms(10);
        
        printf("[SD] Sending CMD0...\n");
        sd_cs_select();
        BYTE cmd0_res = send_cmd(0, 0);
        printf("[SD] CMD0 response: %d\n", cmd0_res);
        
        if (cmd0_res == 1) {
            printf("[SD] CMD0 OK\n");
            ty = 0;
            
            printf("[SD] Sending CMD8...\n");
            BYTE cmd8_res = send_cmd(8, 0x1AA);
            printf("[SD] CMD8 response: %d\n", cmd8_res);
            
            if (cmd8_res == 1) {
                // SDv2
                printf("[SD] CMD8 OK, reading OCR...\n");
                sd_cs_select(); 
                for (int n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
                sd_cs_deselect();
                printf("[SD] OCR: %02X %02X %02X %02X\n", ocr[0], ocr[1], ocr[2], ocr[3]);

                if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                    printf("[SD] SDv2 detected, sending ACMD41...\n");
                    for (tmr = 2000; tmr; tmr--) {
                        if (send_cmd(0x80 | 41, 0x40000000) == 0) break;
                        if (tmr % 100 == 0) printf("[SD] ACMD41 wait %d\n", tmr);
                        sleep_ms(2);
                    }
                    if (tmr) {
                        printf("[SD] ACMD41 OK, sending CMD58...\n");
                        if (send_cmd(58, 0) == 0) {
                            sd_cs_select();
                            for (int n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
                            sd_cs_deselect();
                            ty = (ocr[0] & 0x40) ? 12 : 4;
                            init_ok = true;
                            printf("[SD] SDv2 init OK, type: %d\n", ty);
                        } else {
                            printf("[SD] CMD58 failed\n");
                        }
                    } else {
                        printf("[SD] ACMD41 timeout\n");
                    }
                } else {
                    printf("[SD] Wrong OCR for SDv2\n");
                }
            } else {
                // Старые карты
                printf("[SD] CMD8 failed, trying old protocol...\n");
                ty = 0;
                
                printf("[SD] Trying ACMD41 for SDv1...\n");
                if (send_cmd(0x80 | 41, 0) <= 1) {
                    ty = 2;
                    for (tmr = 2000; tmr; tmr--) {
                        if (send_cmd(0x80 | 41, 0) == 0) break;
                        if (tmr % 100 == 0) printf("[SD] SDv1 ACMD41 wait %d\n", tmr);
                        sleep_ms(2);
                    }
                    if (tmr && send_cmd(16, 512) == 0) {
                        init_ok = true;
                        printf("[SD] SDv1 init OK\n");
                    }
                }
                
                if (!init_ok) {
                    printf("[SD] Trying MMC...\n");
                    if (send_cmd(1, 0) == 0) {
                        ty = 1;
                        for (tmr = 2000; tmr; tmr--) {
                            if (send_cmd(1, 0) == 0) break;
                            if (tmr % 100 == 0) printf("[SD] MMC wait %d\n", tmr);
                            sleep_ms(2);
                        }
                        if (tmr && send_cmd(16, 512) == 0) {
                            init_ok = true;
                            printf("[SD] MMC init OK\n");
                        }
                    }
                }
                
                if (!init_ok) {
                    printf("[SD] Old protocol init failed\n");
                }
            }
        } else {
            printf("[SD] CMD0 failed\n");
        }
        
        retry++; // <-- УВЕЛИЧИВАЕМ СЧЕТЧИК
        if (!init_ok) {
            printf("[SD] init retry %d\n", retry);
            sd_cs_deselect();
            sleep_ms(100);
            sd_spi_init();
        }
    }

    sd_cs_deselect(); 
    spi_xfer(0xFF);

    if (init_ok) {
        printf("[SD] Final OK, type: %d\n", ty);
        return 0;
    }

    printf("[SD] Init failed after retries\n");
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
