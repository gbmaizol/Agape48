#!/usr/bin/env python3
"""Build the application icon from the generated face.

Two outputs, from one source, so they cannot drift apart:

    assets/icon.png       bundled in the binary; QGuiApplication::setWindowIcon
                          uses it, which is what the window manager and the
                          taskbar draw on EVERY platform
    installer/agape48.ico Windows installer, Start-menu shortcut, Add/Remove

Generated rather than checked in by hand for the same reason makeface.py draws
the face: change the art and the icon changes with it.

    python tools/make-icon.py

Run it after makeface.py, since it reads that script's output.
"""

import pathlib
from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent.parent
FACE = HERE / "assets" / "skins" / "default" / "face.png"
PNG = HERE / "assets" / "icon.png"
ICO = HERE / "installer" / "agape48.ico"

# Windows picks whichever of these fits: 16 in a title bar, 32 on the desktop,
# 256 in large-icon view. The PNG is one 256 and Qt scales it down itself.
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
PNG_SIZE = 256

# The calculator body, so the icon does not read as a black square at 16 px.
BACKGROUND = (34, 34, 38, 255)


def square(src: Image.Image, s: int) -> Image.Image:
    """The face letterboxed onto a square. Its ratio is about 0.594, so it has
    to be padded or every platform squashes it differently."""
    canvas = Image.new("RGBA", (s, s), BACKGROUND)
    pad = max(1, s // 16)
    inner = s - 2 * pad
    scale = min(inner / src.width, inner / src.height)
    w, h = max(1, int(src.width * scale)), max(1, int(src.height * scale))
    canvas.paste(src.resize((w, h), Image.LANCZOS), ((s - w) // 2, (s - h) // 2))
    return canvas


def main() -> None:
    src = Image.open(FACE).convert("RGBA")

    PNG.parent.mkdir(parents=True, exist_ok=True)
    square(src, PNG_SIZE).save(PNG, format="PNG")
    print(f"wrote {PNG} ({PNG.stat().st_size} bytes, {PNG_SIZE}x{PNG_SIZE})")

    ICO.parent.mkdir(parents=True, exist_ok=True)
    frames = [square(src, s) for s in ICO_SIZES]
    frames[-1].save(ICO, format="ICO", sizes=[(s, s) for s in ICO_SIZES])
    print(f"wrote {ICO} ({ICO.stat().st_size} bytes, {len(ICO_SIZES)} sizes)")


if __name__ == "__main__":
    main()
