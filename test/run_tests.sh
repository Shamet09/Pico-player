#!/bin/bash
# Compila ed esegue gli unit test nativi dei moduli di logica pura.
# Usa il gcc di sistema: NON serve il toolchain ARM.
#
#   ./test/run_tests.sh
#
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
OUT="$HERE/build"
CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -Werror -O1 -g -I$ROOT/src -I$HERE"

mkdir -p "$OUT"

declare -a SUITES=(
    "test_buttons:$ROOT/src/button_fsm.c"
    "test_mascot:$ROOT/src/mascot_fsm.c"
    "test_play_order:$ROOT/src/play_order.c"
)

fail=0
for entry in "${SUITES[@]}"; do
    name="${entry%%:*}"
    deps="${entry#*:}"
    echo "=============================================================="
    echo "  build $name"
    echo "=============================================================="
    # shellcheck disable=SC2086
    if ! $CC $CFLAGS -o "$OUT/$name" "$HERE/$name.c" $deps; then
        echo "COMPILAZIONE FALLITA: $name"
        fail=1
        continue
    fi
    if ! "$OUT/$name"; then
        fail=1
    fi
done

echo
if [ "$fail" -eq 0 ]; then
    echo "############ TUTTI I TEST NATIVI: PASS ############"
else
    echo "############ ALCUNI TEST NATIVI: FAIL ############"
fi
exit $fail
