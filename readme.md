[#Back-to-Top""]
= DX7 RP2350 MIDI Controller & Translator
:toc:
:toclevels: 3
:doctype: book
:sectnums:
:partnums:
:sectnumlevels: 3
:experimental:
:tip-caption: pass:[&#128161;]
:warning-caption: pass:[&#9888;]
:note-caption: pass:[&#128204;]
:caution-caption: pass:[&#8252;]

|===
|Last update of text: |`2023-02-25 (v2.14.3)`
|Last update of relevant screenshots: |`2023-02-25 (v2.14.3)`
|===


Аппаратно-программный модуль управления синтезатором Yamaha DX7 на базе микроконтроллера **Raspberry Pi RP2350**. 

// Reusable text snippets / Проект должен обеспечивать:
- Трансляцию MIDI CC ↔ SysEx;
- File-managment SysEx-патчей с SD-карты;
- Playback файлов midi c SD-карты;
- USB/SD Mass storage при подключении к PC;
- USB/MIDI клиент при работе с DAW;
- Автономную работу с внешними USB илии MIDI контроллерами как USB/MIDI_bridge;
- GUI на собственный TFT LCD;
- TEST program для настройки конфигурации RP2350 и периферии (debug_mode);
- HELP, info mode (краткий инфрмационный бюллетень на TFT LCD);
- Выбор MIDI CC# профиля default контроллеров как Assigned Map table из preload list.

## 📁 Структура проекта

```text
DX7_RP2350/                  # корневая папка проекта
├── CMakeLists.txt           # Сценарий сборки CMake
├── hw_config.h              # Назначение пинов RP2350 (SPI, I2C, UART, GPIO SW23, LED24; LED25)
├── main.c                   # Точка входа: инициализация и главный цикл (loop)
├── pico_sdk_import.cmake    # дополнительный файл импорта компилятора программы 
│
├── core/                    # [ЖЕЛЕЗО] Драйверы периферии и USB
│   ├── encoder_dvr.*        # Энкодер навигации
│   ├── midi_uart.*          # Физический DIN5 MIDI (UART)
│   ├── numpad_dvr.*         # Сенсорная/матричная клавиатура управления
│   ├── SD_card.*            # Низкоуровневый SPI для SD-карты
│   ├── TFT_dvr.*            # Драйвер LCD дисплея
│   └── usb_descriptors.*    # TinyUSB дескрипторы (MIDI + MSC)
│
├── services/                # [СЕРВИСЫ] Бизнес-логика, UI и файлы
│   ├── sysex_cc_map.*       # Движок трансляции SysEx ↔ CC + MIDI Learn
│   ├── sd_storage.*         # Файловая система FatFS и менеджер SD
│   ├── ui_engine.*          # Отрисовка графических элементов и меню
│   ├── font.*               # Шрифты и символы
│   └── debug_log.*          # Режим отладки системы *логирование через Serial
│
├── modes/                   # [РЕЖИМЫ] Состояния устройства (FSM)
│   ├── 0 modes.h            # Главный enum состояний
│   ├── 1 system_mode.c      # Системное меню и переключатель режимов
│   ├── 2 play_mode.c        # Режим игры и быстрого выбора midi файлов и патчей
│   ├── 3 Arp_mode.c         # MIDI Арпеджиатор
│   ├── 4 midi_bridge_mode.c # Режим моста (трансляция CC ↔ SysEx)
│   ├── 5 sd_review.c        # Браузер файлов пресетов на SD
│   ├── 6 usb_sd_mode.c      # Режим флешки (Mass Storage)
│   └── 7 Help_mode.c        # режим HELP info
│
└── mapping/                 # [ПРОФИЛИ] Встроенные пресеты маппинга
    ├── mapping.h            # Структуры данных таблиц
    ├── map_default.c        # Дефолтная карта параметров DX7
    ├── map_nucleus2.c       # Профиль SSL Nucleus2
    ├── map_nano2.c          # Профиль Korg nanoKONTROL2
    ├── map_LX25P.c          # Профиль Nectar LX25 plus
    └── map_arturia.c        # Профиль Arturia MiniLab
```

## Для читаемости файлов на компактном TFT-экране применяется цветовая схема по типам элементов:
```text
Элемент/тип | Цвет (Normal)     | Цвет (Selected)       | Описание принятой палитры |
====================================================================================
Папки(/DIR) | 0xCDE0 (Горчич.)  | 0xFFE0 (Ярко-желтый)  | Желтая палитра |
MIDI/SysEx  | 0x341F (Синий)    | 0x07FF (Ярко-голубой) | Синяя палитра (.syx, .mid) |
other файлы | 0x7BEF (Серый)    | 0xFFFF (Ярко-белый)   | Серые тона .txt, .map) |
Фон курсора | 0x0000 (Черный)   | 0x18E3 (Темно-серый)  | Подсветка активной строки |
```

## Архитектура индексов параметров DX7:
Для операторов в Yamaha DX7 используется смещение с шагом 21 байт (начиная с OP6 = 0, заканчивая OP1 = 105):
```text
OP6 (Параметры 0–20): Level = 15, Freq Coarse = 17
OP5 (Параметры 21–41): Level = 36, Freq Coarse = 38
OP4 (Параметры 42–62): Level = 57, Freq Coarse = 59
OP3 (Параметры 63–83): Level = 78, Freq Coarse = 80
OP2 (Параметры 84–104): Level = 99, Freq Coarse = 101
OP1 (Параметры 105–125): Level = 120, Freq Coarse = 122
```