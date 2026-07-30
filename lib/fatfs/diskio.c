#include "ff.h"
#include "diskio.h"
#include "SD_card.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "hw_config.h"
#include <stdio.h>
    
#define DEV_MMC         0
#define SD_CMD_TIMEOUT  10000

static BYTE CardType = 0;
static BYTE spi_xfer(BYTE data) {
    BYTE out;
    
    // АППАРАТНАЯ ЗАЩИТА ОТ ЗАВИСАНИЯ SPI ПОД RP2350:
    // Если буфер FIFO приемника забит мусором из-за наводок при выключенном CS, 
    // принудительно вычищаем его перед началом новой транзакции.
    // while (spi_is_readable(SD_SPI_PORT)) {
    //    (void)spi_get_hw(SD_SPI_PORT)->dr; // читаем регистр данных в пустоту
    // }
    // Чистый, безопасный и атомарный вызов Pico SDK сам все вычитает и запишет
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

    // 1. Подаем холостые такты при выключенном CS
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
            if (send_cmd(0x80 | 41, 0) <= 1) {
                ty = 2;
                for (tmr = 1000; tmr; tmr--) {
                    if (send_cmd(0x80 | 41, 0) == 0) break;
                    sleep_ms(1);
                }
            } else {
                ty = 1;
                for (tmr = 1000; tmr; tmr--) {
                    if (send_cmd(1, 0) == 0) break;
                    sleep_ms(1);
                }
            }
            if (!tmr || send_cmd(16, 512) != 0) ty = 0;
        }
    }

    sd_cs_deselect();
    spi_xfer(0xFF); // Холостой такт завершения транзакции

    // КРИТИЧЕСКИ ВАЖНО: сохраняем вычисленный тип карты в глобальную переменную!
    CardType = ty; 

    if (ty) {
        // Карта успешно дала свой тип! Включаем рабочую скорость (2 МГц)
        sd_spi_set_high_speed();
        return 0; // RES_OK
    }

    return STA_NOINIT;
}

// ----------------------------------------------------------------------------
// Универсальное чтение секторов (Автоматически поддерживает SDSC и SDHC)
// ----------------------------------------------------------------------------
DRESULT disk_read (
    BYTE pdrv,    /* Physical drive number (0) */
    BYTE *buff,   /* Data buffer to store read data */
    LBA_t sector, /* Sector address */
    UINT count    /* Sector count */
)
{
    if (pdrv != DEV_MMC || !count) return RES_PARERR;
    sleep_us(5); // Короткая пауза для стабилизации таймингов RP2350

    if (count == 1) {
        // Автовыбор адресации: если установлен бит CT_BLOCK (0x08) - шлем sector, иначе байты (sector << 9)
        DWORD s_addr = (CardType & 0x08) ? sector : (sector << 9);

        if (send_cmd(17, s_addr) == 0) {
            sd_cs_select(); // Восстанавливаем CS=0, так как send_cmd его принудительно выключил
            
            DWORD token;
            uint32_t timeout = SD_CMD_TIMEOUT;
            do {
                token = spi_xfer(0xFF);
            } while (token == 0xFF && --timeout);

            if (token == 0xFE) {
                // Побайтовое чтение — самое надежное решение против зависаний FIFO буфера RP2350
                for (int i = 0; i < 512; i++) {
                    buff[i] = spi_xfer(0xFF);
                }
                spi_xfer(0xFF); spi_xfer(0xFF); // Пропускаем 2 байта CRC16
                count = 0;
            }
        }
    } else {
        DWORD s_addr = (CardType & 0x08) ? sector : (sector << 9);

        if (send_cmd(18, s_addr) == 0) {
            sd_cs_select(); // Восстанавливаем CS=0
            
            do {
                DWORD token;
                uint32_t timeout = SD_CMD_TIMEOUT;
                do {
                    token = spi_xfer(0xFF);
                } while (token == 0xFF && --timeout);

                if (token != 0xFE) break;
                
                for (int i = 0; i < 512; i++) {
                    buff[i] = spi_xfer(0xFF);
                }
                spi_xfer(0xFF); spi_xfer(0xFF); // Пропускаем CRC16
                buff += 512;
            } while (--count);
            
            send_cmd(12, 0); // CMD12 остановит трансляцию множественного чтения
        }
    }

    sd_cs_deselect(); // Освобождаем шину SPI
    spi_xfer(0xFF);   // Даем 8 пустых тактов для завершения работы контроллера карты

    return count ? RES_ERROR : RES_OK;
}

// ----------------------------------------------------------------------------
// Универсальная запись секторов (Автоматически поддерживает SDSC и SDHC)
// ----------------------------------------------------------------------------
#if FF_FS_READONLY == 0
DRESULT disk_write (
    BYTE pdrv,          /* Physical drive number (0) */
    const BYTE *buff,   /* Data to be written */
    LBA_t sector,       /* Sector address */
    UINT count          /* Sector count */
)
{
    if (pdrv != DEV_MMC || !count) return RES_PARERR;

    if (count == 1) {
        DWORD s_addr = (CardType & 0x08) ? sector : (sector << 9);

        if (send_cmd(24, s_addr) == 0) {
            sd_cs_select(); // Восстанавливаем CS=0 после авто-деселекта из send_cmd
            
            spi_xfer(0xFF); spi_xfer(0xFE); // Стартовый токен одиночной записи (0xFE)
            for (int i = 0; i < 512; i++) {
                spi_xfer(buff[i]);
            }
            spi_xfer(0xFF); spi_xfer(0xFF); // Фиктивные 2 байта CRC16
            
            if ((spi_xfer(0xFF) & 0x1F) == 0x05) {
                // Даем карте физическое время на запись, не насилуя шину тактами
                while (spi_xfer(0xFF) == 0) {
                    sleep_us(50); // <--- КРИТИЧНО ДЛЯ RP2350: микропауза 50 мкс
                }
                count = 0;
            }
        }
    } else {
        DWORD s_addr = (CardType & 0x08) ? sector : (sector << 9);

        if (send_cmd(25, s_addr) == 0) {
            sd_cs_select(); // Восстанавливаем CS=0
            
            do {
                spi_xfer(0xFF); spi_xfer(0xFC); // Токен начала блока множественной записи (0xFC)
                for (int i = 0; i < 512; i++) {
                    spi_xfer(buff[i]);
                }
                spi_xfer(0xFF); spi_xfer(0xFF); // CRC16
                
                if ((spi_xfer(0xFF) & 0x1F) != 0x05) break;
                while (spi_xfer(0xFF) == 0) {  // Ждем готовности карты к следующему блоку
                    sleep_us(50); // <--- КРИТИЧНО ДЛЯ RP2350: микропауза 50 мкс
                }
                buff += 512;
            } while (--count);
            
            spi_xfer(0xFF); // Синхро-такт перед командой останова
            spi_xfer(0xFD); // Токен Stop Tran (0xFD)
            while (spi_xfer(0xFF) == 0) { // Финальное ожидание завершения записи на флеш
                sleep_us(50); // <--- КРИТИЧНО ДЛЯ RP2350: микропауза 50 мкс
            }
        }
    }
    
    sd_cs_deselect(); // Освобождаем шину SPI
    spi_xfer(0xFF);   // Синхро-такт

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