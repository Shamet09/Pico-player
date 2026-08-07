#!/bin/bash
# Compila il firmware per RP2040 e produce build/picoplayer.uf2.
#
#   PICO_SDK_PATH=... PICO_EXTRAS_PATH=... ./tools/build.sh [Release|Debug]
#
# Se le variabili non sono impostate, cerca le dipendenze in /opt/pico
# (percorso usato da tools/setup_deps.sh).
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"

: "${PICO_SDK_PATH:=/opt/pico/pico-sdk}"
: "${PICO_EXTRAS_PATH:=/opt/pico/pico-extras}"
BUILD_TYPE="${1:-Release}"

# Compilare direttamente su /mnt/c da WSL e' molto lento: se la sorgente sta
# su un filesystem montato, teniamo la directory di build su disco Linux e
# copiamo alla fine solo i binari.
BUILD_DIR="$ROOT/build"
COPY_BACK=""
case "$ROOT" in
    /mnt/*)
        # Non /tmp: con systemd attivo WSL puo' ripulirlo fra una sessione e
        # l'altra, buttando via la cache di build.
        BUILD_DIR="${PICOPLAYER_BUILD_DIR:-${HOME:-/root}/.cache/picoplayer-build}"
        COPY_BACK="$ROOT/build"
        ;;
esac

echo "SDK    : $PICO_SDK_PATH"
echo "EXTRAS : $PICO_EXTRAS_PATH"
echo "BUILD  : $BUILD_DIR ($BUILD_TYPE)"

for p in "$PICO_SDK_PATH" "$PICO_EXTRAS_PATH"; do
    if [ ! -d "$p" ]; then
        echo "ERRORE: $p non esiste. Lancia prima tools/setup_deps.sh" >&2
        exit 1
    fi
done

mkdir -p "$BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DPICO_SDK_PATH="$PICO_SDK_PATH" \
      -DPICO_EXTRAS_PATH="$PICO_EXTRAS_PATH" \
      -DPICO_BOARD=pico

cmake --build "$BUILD_DIR" -- -j"$(nproc)"

if [ -n "$COPY_BACK" ]; then
    mkdir -p "$COPY_BACK"
    cp -f "$BUILD_DIR"/picoplayer.uf2 \
          "$BUILD_DIR"/picoplayer.elf \
          "$BUILD_DIR"/picoplayer.bin "$COPY_BACK"/ 2>/dev/null || true
    echo
    echo "binari copiati in $COPY_BACK"
    ls -la "$COPY_BACK"
fi

echo
arm-none-eabi-size "$BUILD_DIR/picoplayer.elf"
