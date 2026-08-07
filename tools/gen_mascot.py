#!/usr/bin/env python3
"""Genera src/mascot_bitmaps.h (e un PNG di anteprima) per la mascotte "Pico".

I frame sono definiti qui come pixel art leggibile ('#' = pixel acceso) invece
che come byte trascritti a mano: molto meno soggetto a errori, e permette di
esportare un'anteprima PNG per una verifica visiva prima di mettere i byte nel
firmware (come richiesto dalla sezione 7 del documento di progetto).

Formato di uscita: 1 bit/pixel, MSB a sinistra, ceil(larghezza/8) byte per riga.

Uso:  python3 tools/gen_mascot.py
"""

import os
import sys

WIDTH = 20
HEIGHT = 16

# Riferimento ASCII disegnato dall'utente:
#   /\_/\
#  ( o.o )
#   > - <

# --- geometria comune: orecchie + contorno della testa ----------------------
#      0         1
#      0123456789012345678901
BASE = [
    "  ##            ##  ",  # 0  punte delle orecchie
    "  #  #        #  #  ",  # 1
    " #    #      #    # ",  # 2
    " #     ######     # ",  # 3  raccordo fra le orecchie
    "#                  #",  # 4
    "#                  #",  # 5
    "#                  #",  # 6  <- riga occhi A
    "#                  #",  # 7  <- riga occhi B
    "#                  #",  # 8  <- riga occhi C (solo occhi a X)
    "#                  #",  # 9
    "#       ####       #",  # 10 naso
    " #                # ",  # 11 <- riga bocca A
    "  #              #  ",  # 12 <- riga bocca B
    "   ##          ##   ",  # 13
    "     ##########     ",  # 14 mento
    "                    ",  # 15
]

# --- varianti di occhi e bocca ---------------------------------------------
EYES_OPEN = {  # o.o
    6: "#   ##        ##   #",
    7: "#   ##        ##   #",
}
EYES_CLOSED = {  # blink: due trattini
    7: "#  ####      ####  #",
}
EYES_SLEEPY = {  # pausa: occhi socchiusi a "u"
    6: "#  #  #      #  #  #",
    7: "#   ##        ##   #",
}
EYES_X = {  # errore SD
    6: "#  # #        # #  #",
    7: "#   #          #   #",
    8: "#  # #        # #  #",
}
EYES_HAPPY = {  # ^.^
    6: "#   #          #   #",
    7: "#  # #        # #  #",
}

MOUTH_NEUTRAL = {  # > - <
    11: " #    #  ##  #    # ",
}
MOUTH_CLOSED = {  # pausa
    11: " #      ####      # ",
}
MOUTH_OPEN = {  # errore SD: bocca aperta/preoccupata
    11: " #     ######     # ",
    12: "  #    ######    #  ",
}
MOUTH_SMILE = {  # splash "SD ok"
    11: " #    #      #    # ",
    12: "  #    ######    #  ",
}


def frame(*overlays):
    rows = list(BASE)
    for ov in overlays:
        for y, line in ov.items():
            rows[y] = line
    return rows


FRAMES = [
    ("idle", frame(EYES_OPEN, MOUTH_NEUTRAL)),
    ("blink", frame(EYES_CLOSED, MOUTH_NEUTRAL)),
    ("paused", frame(EYES_SLEEPY, MOUTH_CLOSED)),
    ("sd_error", frame(EYES_X, MOUTH_OPEN)),
    ("sd_ok", frame(EYES_HAPPY, MOUTH_SMILE)),
]

# L'ordine deve combaciare con mascot_frame_t in src/mascot_fsm.h
EXPECTED_ORDER = ["idle", "blink", "paused", "sd_error", "sd_ok"]


def validate(name, rows):
    if len(rows) != HEIGHT:
        raise SystemExit("frame %s: %d righe invece di %d" % (name, len(rows), HEIGHT))
    for i, r in enumerate(rows):
        if len(r) != WIDTH:
            raise SystemExit(
                "frame %s riga %d: %d colonne invece di %d (%r)" % (name, i, len(r), WIDTH, r)
            )


def to_bytes(rows):
    """1 bit/pixel, MSB a sinistra, ceil(WIDTH/8) byte per riga."""
    stride = (WIDTH + 7) // 8
    out = bytearray()
    for r in rows:
        row = bytearray(stride)
        for x, ch in enumerate(r):
            if ch != " ":
                row[x // 8] |= 0x80 >> (x % 8)
        out += row
    return bytes(out)


def ascii_preview(rows):
    top = "+" + "-" * WIDTH + "+"
    return "\n".join([top] + ["|" + r.replace("#", "#") + "|" for r in rows] + [top])


def write_header(path):
    stride = (WIDTH + 7) // 8
    lines = []
    lines.append("/* mascot_bitmaps.h - GENERATO da tools/gen_mascot.py: non modificare a mano.")
    lines.append(" *")
    lines.append(" * Mascotte \"Pico\": %d frame di %dx%d px, 1 bit/pixel, MSB a sinistra," % (len(FRAMES), WIDTH, HEIGHT))
    lines.append(" * %d byte per riga, %d byte per frame." % (stride, stride * HEIGHT))
    lines.append(" * L'ordine dei frame combacia con mascot_frame_t (src/mascot_fsm.h).")
    lines.append(" */")
    lines.append("#ifndef PP_MASCOT_BITMAPS_H")
    lines.append("#define PP_MASCOT_BITMAPS_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("#define MASCOT_W       %d" % WIDTH)
    lines.append("#define MASCOT_H       %d" % HEIGHT)
    lines.append("#define MASCOT_STRIDE  %d" % stride)
    lines.append("")
    lines.append("static const uint8_t mascot_bitmaps[%d][%d] = {" % (len(FRAMES), stride * HEIGHT))
    for name, rows in FRAMES:
        data = to_bytes(rows)
        lines.append("    { /* %s */" % name)
        for y in range(HEIGHT):
            chunk = data[y * stride:(y + 1) * stride]
            art = rows[y].replace(" ", ".")
            lines.append(
                "        %s  /* %s */" % (
                    "".join("0x%02X, " % b for b in chunk),
                    art,
                )
            )
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* PP_MASCOT_BITMAPS_H */")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def write_png(path, scale=6):
    try:
        from PIL import Image
    except ImportError:
        print("PIL/Pillow non disponibile: salto l'anteprima PNG", file=sys.stderr)
        return False

    gap = 4
    w = len(FRAMES) * (WIDTH + gap) + gap
    h = HEIGHT + 2 * gap
    img = Image.new("1", (w, h), 0)
    px = img.load()
    for i, (_name, rows) in enumerate(FRAMES):
        ox = gap + i * (WIDTH + gap)
        for y, r in enumerate(rows):
            for x, ch in enumerate(r):
                if ch != " ":
                    px[ox + x, gap + y] = 1
    img = img.resize((w * scale, h * scale), Image.NEAREST)
    img.save(path)
    return True


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)

    names = [n for n, _ in FRAMES]
    if names != EXPECTED_ORDER:
        raise SystemExit("ordine dei frame diverso da mascot_frame_t: %r" % names)

    for name, rows in FRAMES:
        validate(name, rows)
        print("== %s ==" % name)
        print(ascii_preview(rows))

    hdr = os.path.join(root, "src", "mascot_bitmaps.h")
    write_header(hdr)
    print("scritto %s" % hdr)

    png = os.path.join(here, "mascot_preview.png")
    if write_png(png):
        print("scritto %s" % png)


if __name__ == "__main__":
    main()
