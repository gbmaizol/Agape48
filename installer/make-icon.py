#!/usr/bin/env python3
"""Build agape48.ico from the generated face.

Kept as a script rather than a checked-in .ico for the same reason tools/
makeface.py draws the face rather than shipping one: the art is generated, so
changing the face changes the icon with it and there is no binary blob to drift.

    python installer/make-icon.py

Writes installer/agape48.ico, which agape48.iss uses for the installer, the
Start-menu shortcut and Add/Remove Programs.
"""

import pathlib
from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
FACE = HERE.parent / "assets" / "skins" / "default" / "face.png"
OUT = HERE / "agape48.ico"

# Windows picks whichever of these fits the context: 16 in the title bar and
# tray, 32 on the desktop, 256 in the large-icon view.
SIZES = [16, 24, 32, 48, 64, 128, 256]

# The calculator body, so the icon does not read as a black square at 16 px.
BACKGROUND = (34, 34, 38, 255)


def main() -> None:
    src = Image.open(FACE).convert("RGBA")

    frames = []
    for s in SIZES:
        # The face is tall and narrow - roughly 0.594 - so it has to be
        # letterboxed onto a square or Windows squashes it.
        canvas = Image.new("RGBA", (s, s), BACKGROUND)
        pad = max(1, s // 16)
        inner = s - 2 * pad
        scale = min(inner / src.width, inner / src.height)
        w, h = max(1, int(src.width * scale)), max(1, int(src.height * scale))
        canvas.paste(src.resize((w, h), Image.LANCZOS), ((s - w) // 2, (s - h) // 2))
        frames.append(canvas)

    frames[-1].save(OUT, format="ICO", sizes=[(s, s) for s in SIZES])
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes, {len(SIZES)} sizes)")


if __name__ == "__main__":
    main()
