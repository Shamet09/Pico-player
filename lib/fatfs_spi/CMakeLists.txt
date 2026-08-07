# FatFs (ff15) + driver microSD via SPI.
# Vendorizzato da carlk3/no-OS-FatFS-SD-SPI-RPi-Pico (Apache-2.0).
# Rispetto all'originale usa percorsi assoluti, cosi' la libreria puo' essere
# inclusa da qualunque punto dell'albero.

add_library(FatFs_SPI INTERFACE)

target_sources(FatFs_SPI INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/crc.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/demo_logging.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/sd_card.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/sd_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/spi.c
    ${CMAKE_CURRENT_LIST_DIR}/src/f_util.c
    ${CMAKE_CURRENT_LIST_DIR}/src/glue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/my_debug.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rtc.c
)

target_include_directories(FatFs_SPI INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(FatFs_SPI INTERFACE
    hardware_spi
    hardware_dma
    hardware_rtc
    pico_stdlib
)
