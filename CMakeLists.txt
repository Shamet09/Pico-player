cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# La decodifica MP3 su Cortex-M0+ ha bisogno di ottimizzazione: senza un
# CMAKE_BUILD_TYPE il Pico SDK compila senza -O e l'audio non sta dietro al
# tempo reale. Release e' quindi il default.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo di build" FORCE)
endif()

# Genera compile_commands.json: e' il file da cui l'editor (IntelliSense di
# VS Code, clangd, ...) ricava gli include path veri, invece di indovinarli.
# Senza, tutti gli #include di pico/*, hardware/*, ff.h e mp3dec.h risultano
# "non trovati" nell'editor pur compilando benissimo.
# tools/build.sh lo copia poi nella radice del progetto.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Pico SDK ---------------------------------------------------------------
include(pico_sdk_import.cmake)

# --- pico-extras (solo i moduli audio) --------------------------------------
if(NOT PICO_EXTRAS_PATH)
    if(DEFINED ENV{PICO_EXTRAS_PATH})
        set(PICO_EXTRAS_PATH $ENV{PICO_EXTRAS_PATH})
    endif()
endif()
if(NOT PICO_EXTRAS_PATH)
    message(FATAL_ERROR
        "PICO_EXTRAS_PATH non impostato. Clona raspberrypi/pico-extras e "
        "passa -DPICO_EXTRAS_PATH=/percorso/pico-extras (vedi tools/setup_deps.sh).")
endif()
if(NOT EXISTS ${PICO_EXTRAS_PATH}/src/rp2_common/pico_audio_i2s/audio_i2s.c)
    message(FATAL_ERROR "PICO_EXTRAS_PATH=${PICO_EXTRAS_PATH} non sembra pico-extras.")
endif()

project(picoplayer C CXX ASM)

if(PICO_SDK_VERSION_STRING VERSION_LESS "2.0.0")
    message(FATAL_ERROR "Serve il Pico SDK 2.0 o successivo (trovato ${PICO_SDK_VERSION_STRING}).")
endif()

pico_sdk_init()

# Non serve tutto pico-extras: bastano i tre moduli dell'audio.
# pico_audio dipende da pico_util_buffer, che sta anch'esso in pico-extras.
add_subdirectory(${PICO_EXTRAS_PATH}/src/common/pico_util_buffer   pico_util_buffer)
add_subdirectory(${PICO_EXTRAS_PATH}/src/common/pico_audio          pico_audio)
add_subdirectory(${PICO_EXTRAS_PATH}/src/rp2_common/pico_audio_i2s  pico_audio_i2s)

# --- FatFs + driver microSD (vendorizzato in lib/) --------------------------
add_subdirectory(lib/fatfs_spi)

# --- Decoder MP3 Helix (vendorizzato in lib/helix) --------------------------
add_library(helix_mp3 STATIC
    lib/helix/bitstream.c
    lib/helix/buffers.c
    lib/helix/dct32.c
    lib/helix/dequant.c
    lib/helix/dqchan.c
    lib/helix/huffman.c
    lib/helix/hufftabs.c
    lib/helix/imdct.c
    lib/helix/mp3dec.c
    lib/helix/mp3tabs.c
    lib/helix/polyphase.c
    lib/helix/scalfact.c
    lib/helix/stproc.c
    lib/helix/subband.c
    lib/helix/trigtabs.c
    lib/helix/helix_memory.c    # non e' upstream: vedi il commento nel file
)
target_include_directories(helix_mp3 PUBLIC lib/helix)
# Vedi il commento in lib/helix/assembly.h: sul Cortex-M0+ va usata la
# variante C portabile, non quella con assembly ARMv5TE.
target_compile_definitions(helix_mp3 PUBLIC HELIX_PORTABLE_C=1)
# Il codice Helix e' del 2003: silenziamo il rumore dei warning moderni su
# sorgenti che non modifichiamo.
target_compile_options(helix_mp3 PRIVATE -w)
# Nessun link a pico_stdlib di proposito: il decoder usa solo malloc/memcpy
# della newlib, e pico_stdlib e' una libreria INTERFACE -- collegarla qui
# farebbe ricompilare una quarantina di file dell'SDK dentro questo archivio,
# duplicandoli con quelli dell'eseguibile.

# --- Firmware ---------------------------------------------------------------
add_executable(picoplayer
    src/main.c
    src/shared_state.c
    src/button_fsm.c
    src/buttons.c
    src/mascot_fsm.c
    src/mascot.c
    src/play_order.c
    src/ssd1306.c
    src/playlist.c
    src/player.c
    src/ui.c
    src/battery.c
    hw_config.c
)

target_include_directories(picoplayer PRIVATE src)

target_link_libraries(picoplayer
    pico_stdlib
    pico_multicore
    hardware_i2c
    hardware_gpio
    hardware_pio
    hardware_dma
    hardware_clocks
    pico_audio_i2s
    FatFs_SPI
    helix_mp3
)

# stdio su UART0, ma spostata su GP12/GP13: i pin di default (GP0/GP1) qui
# sono occupati dall'I2C dell'OLED.
target_compile_definitions(picoplayer PRIVATE
    PICO_DEFAULT_UART=0
    PICO_DEFAULT_UART_TX_PIN=12
    PICO_DEFAULT_UART_RX_PIN=13

    # Lo stack di core1 di default e' 2 KB. Misurando i frame con
    # -fstack-usage, il percorso piu' profondo (scansione della SD:
    # player_core1_main 144 + playlist_scan_tracks 88 + scan_dir 488 + la
    # catena FatFs/driver SD) sfiora i 1400 byte, a cui va sommata la ISR del
    # DMA audio che puo' annidarsi sopra. Restavano ~450 byte di margine:
    # troppo pochi. 4 KB costano 2 KB di RAM su oltre 200 KB liberi.
    PICO_CORE1_STACK_SIZE=4096
)
pico_enable_stdio_uart(picoplayer 1)

# stdio anche su USB, se il submodule tinyusb del SDK e' disponibile.
#
# Con questo il Pico, collegato al PC con lo stesso cavo con cui lo si flasha,
# compare come porta COM: il log si legge con PuTTY (o qualunque terminale
# seriale) senza bisogno di un adattatore USB-seriale. Non sostituisce la UART
# su GP12/GP13: sono attive entrambe e stampano le stesse cose.
#
# L'attivazione e' automatica ma condizionata, non obbligatoria: tinyusb e' un
# submodule che nel SDK appena clonato NON c'e'. Renderlo obbligatorio
# spaccherebbe la build su una macchina pulita, e questo firmware deve
# compilare al primo colpo. Per abilitarlo:
#
#     sudo git -C "$PICO_SDK_PATH" submodule update --init lib/tinyusb
#
# e ricompilare (CMake se ne accorge da solo).
if(NOT PICO_TINYUSB_PATH)
    set(PICO_TINYUSB_PATH ${PICO_SDK_PATH}/lib/tinyusb)
endif()
if(EXISTS ${PICO_TINYUSB_PATH}/src/tusb.h)
    message(STATUS "PicoPlayer: tinyusb trovato, stdio anche su USB (porta COM).")
    pico_enable_stdio_usb(picoplayer 1)
    target_compile_definitions(picoplayer PRIVATE
        # Non aspettare che qualcuno apra il terminale. E' gia' il default del
        # SDK, ma qui e' un requisito: alimentato da un caricabatterie invece
        # che dal PC, il player non deve restare fermo ad aspettare una
        # connessione che non arrivera' mai.
        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=0

        # Quanto puo' bloccarsi una printf se il terminale e' aperto ma non
        # sta leggendo. Il default del SDK e' mezzo secondo: siccome le printf
        # partono anche da core1, mezzo secondo di stallo li' significa audio
        # muto per mezzo secondo (i buffer coprono ~104 ms). Con 20 ms il log
        # perde qualche carattere in un caso patologico, ma la riproduzione
        # non se ne accorge.
        PICO_STDIO_USB_STDOUT_TIMEOUT_US=20000
    )
else()
    message(STATUS
        "PicoPlayer: tinyusb assente, stdio solo su UART (GP12/GP13). "
        "Per leggere il log via USB con PuTTY: "
        "sudo git -C ${PICO_SDK_PATH} submodule update --init lib/tinyusb")
    pico_enable_stdio_usb(picoplayer 0)
endif()

pico_set_program_name(picoplayer "PicoPlayer")
pico_set_program_description(picoplayer "Lettore MP3 portatile su RP2040")
pico_add_extra_outputs(picoplayer)   # genera anche il .uf2
