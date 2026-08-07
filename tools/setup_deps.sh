#!/bin/bash
# Scarica le due dipendenze esterne (Pico SDK e pico-extras).
# Le altre librerie di terze parti sono gia' vendorizzate in lib/.
#
#   ./tools/setup_deps.sh [directory]      (default: /opt/pico)
#
# Su Ubuntu/Debian il toolchain si installa con:
#   sudo apt-get install cmake ninja-build build-essential \
#        gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
#        python3-pil
set -e

DEST="${1:-/opt/pico}"
mkdir -p "$DEST"

clone() {
    local url="$1" dir="$2"
    if [ -d "$DEST/$dir/.git" ]; then
        echo "== $dir gia' presente, salto"
        return
    fi
    echo "== clono $dir"
    git clone --depth 1 "$url" "$DEST/$dir"
}

clone https://github.com/raspberrypi/pico-sdk.git    pico-sdk
clone https://github.com/raspberrypi/pico-extras.git pico-extras

cat <<EOF

Fatto. Per compilare:

    export PICO_SDK_PATH=$DEST/pico-sdk
    export PICO_EXTRAS_PATH=$DEST/pico-extras
    ./tools/build.sh

Nota: il submodule tinyusb del Pico SDK NON serve, perche' stdio va su UART
(GP12/GP13) e non su USB.
EOF
