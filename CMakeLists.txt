cmake_minimum_required(VERSION 3.13)

# СТРОГО ДО инициализации SDK задаем плату RP2350 (Pico 2)
set(PICO_BOARD pico2 CACHE STRING "Board type")
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Platform type")

# Инициализация Pico SDK. 
# Внутри этого файла прописан поиск переменной окружения PICO_SDK_PATH
include(pico_sdk_import.cmake)

project(DX7_RP2350_upgrade C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Инициализация Pico SDK
pico_sdk_init()

# 1. Создаем исполняемый файл и перечисляем исходники
add_executable(DX7_RP2350_upgrade
    main.c
    
    # Core: Аппаратные драйверы и низкоуровневые шины
    core/encoder_dvr.c
    core/midi_uart.c
    core/numpad_dvr.c
    core/SD_card.c
    core/TFT_dvr.c
    
    # Services: Бизнес-логика, хранилище, UI, отладка
    services/sysex_cc_map.c
    services/sd_storage.c
    services/ui_engine.c
    services/ui_theme.c
    services/font.c
    services/debug_log.c
    
    # Modes: Логика режимов работы (FSM)
    modes/system_mode.c
    modes/play_mode.c
    modes/sd_review.c
    modes/midi_bridge_mode.c

    # Mapping: Встроенные профили контроллеров
    mapping/map_default.c
    mapping/map_nucleus2.c

    # Библиотека FatFS (все необходимые модули)
    lib/fatfs/ff.c
    lib/fatfs/ffsystem.c
    lib/fatfs/ffunicode.c
    lib/fatfs/diskio.c
)

# 2. Подключаем папки с заголовочными файлами (.h)
target_include_directories(DX7_RP2350_upgrade PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/core
    ${CMAKE_CURRENT_LIST_DIR}/modes
    ${CMAKE_CURRENT_LIST_DIR}/mapping
    ${CMAKE_CURRENT_LIST_DIR}/services
    ${CMAKE_CURRENT_LIST_DIR}/lib/fatfs
)

# 3. Подключение библиотек Pico SDK
target_link_libraries(DX7_RP2350_upgrade
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_uart
    hardware_gpio
    hardware_adc
)

# 4. Включаем вывод stdio
pico_enable_stdio_usb(DX7_RP2350_upgrade 1)   # Вывод через USB CDC
pico_enable_stdio_uart(DX7_RP2350_upgrade 1)  # Вывод через UART TX/RX

# 5. Генерируем прошивку (.uf2, .bin, .hex)
pico_add_extra_outputs(DX7_RP2350_upgrade)