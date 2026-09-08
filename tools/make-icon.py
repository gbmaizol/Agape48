#!/usr/bin/env python3
"""Build the application icon from a photograph of a real 48GX.

One piece of art, every platform, so they cannot drift apart:

    assets/icon.png            bundled in the binary; QGuiApplication::
                               setWindowIcon uses it, which is what the window
                               manager and the taskbar draw on EVERY platform
    installer/agape48.ico      Windows installer, Start-menu shortcut,
                               Add/Remove
    platform/android/res/...   the adaptive icon: a foreground layer at five
                               densities, a background colour, and the legacy
                               square for anything older than API 26

THE SOURCE IS A PHOTOGRAPH, not the drawn face, since 2026sep08. Gert: "my wife
is a designer, and she made me change my mind... rotate this image 30 degrees
counter-clockwise instead, and this will be the base for the adaptive icon."
assets/icon-source.png is her HP48GXDusty.png, a dusty close-up of the bottom
left of a real keyboard - the shift keys, ON/CANCEL, ENTER - copied into the
tree so the build does not depend on a file in Dropbox, and with its EXIF
stripped (GIMP's own version string and two timestamps; no camera, no GPS).
Stripped with exiftool rather than re-encoded, so the pixels are byte-identical.

    python tools/make-icon.py

It no longer reads makeface.py's output, so the two are independent: changing
the face no longer changes the icon.

THE PHOTOGRAPH IS TURNED 30 DEGREES COUNTER-CLOCKWISE. Gert, 2026sep08: "one
quirk to make it different from Droid48: Rotate it 30 counter-clockwise. But
this needs to be the new icon for all OSs." It also solves a shape problem it
was not asked to solve: the photo is 325x519, a ratio of 0.626, and turning it
makes its bounding box 0.88 - so it fills a square, and a launcher's circle,
far better than it could upright.

TRANSPARENT WHEREVER A FORMAT ALLOWS IT, and Gert's slate where one does not.
His words, in that order: "make the background r66,g75,b92" and then "or
transparent when possible". There is exactly one place it is not possible: an
Android ADAPTIVE icon's background layer, which is required to be opaque and
which some launchers paint black if it is not - black bars again, which is the
complaint that started this. So:

    the desktop PNG, the .ico, the adaptive FOREGROUND layer and the legacy
    square      transparent, and the tilted photograph floats on whatever is
                behind it
    the adaptive BACKGROUND layer     #424B5C, the slate he chose

The soft shadow is kept in every case. It is what stops a dark photograph from
dissolving into a dark taskbar, and on transparency it simply travels with the
picture.
"""

import pathlib
from PIL import Image, ImageDraw, ImageFilter

HERE = pathlib.Path(__file__).resolve().parent.parent
SOURCE = HERE / "assets" / "icon-source.png"
PNG = HERE / "assets" / "icon.png"
ICO = HERE / "installer" / "agape48.ico"
RES = HERE / "platform" / "android" / "res"

# Windows picks whichever of these fits: 16 in a title bar, 32 on the desktop,
# 256 in large-icon view. The PNG is one 256 and Qt scales it down itself.
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
PNG_SIZE = 256

ANGLE = 30                              # counter-clockwise

# Gert's, given as three numbers: "make the background r66,g75,b92". A slate
# blue-grey that picks up the photograph's own cool cast, so the tilted picture
# reads as an object lying on a ground rather than as a hole in one. Used ONLY
# where transparency is not allowed - see the note at the top.
BACKGROUND = (66, 75, 92, 255)
CLEAR = (0, 0, 0, 0)

# How much of the canvas the photograph takes. OVER 1.0 ON PURPOSE: it is
# scaled until its bounding box is a quarter wider than the icon, so the four
# corners of the tilted picture fall outside and are cut off. Gert, 2026sep08:
# "The image is too small. make it larger, it doesn't matter if some corners
# will be cut."
#
# 1.25 rather than more. Past about 1.4 the cut corners meet, the picture
# becomes a plain square, and two things go with them: the tilt stops being
# visible at all on a transparent desktop icon, and the slate is left with
# nothing to do. At 1.25 a wedge of ground still shows at two corners, which is
# what keeps the rotation legible.
#
# One number for both, where Android's used to be smaller to stay inside the
# 72dp safe zone. Being cropped is now the point, so the zone it has to respect
# is the mask, not the canvas.
FILL_DESKTOP = 1.25
FILL_ADAPTIVE = 1.25

# "Oh, and make the edges a bit blurry!" - Gert, 2026sep08. A fraction of the
# icon's own size rather than a fixed number of pixels, so the softness looks
# the same at 16 and at 432 instead of turning a small icon to mush. The fade
# is pushed INWARDS first: blurring the mask alone would spread it outwards
# over the black the rotation leaves outside the photograph, and hang a dirty
# halo on every edge.
FEATHER = 0.018

# 108dp foreground layers, and the 48dp legacy launcher icon, at the five
# densities Android asks for.
DENSITIES = {"mdpi": 1, "hdpi": 1.5, "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}


def turned(src: Image.Image) -> Image.Image:
    """Rotated about its centre, with an alpha mask of its own. The photograph
    has no alpha channel, so rotating it directly fills the new corners with
    black - which would put the bars back, at an angle."""
    solid = Image.new("RGBA", src.size, (255, 255, 255, 255))
    rot = src.rotate(ANGLE, expand=True, resample=Image.BICUBIC)
    rot.putalpha(solid.rotate(ANGLE, expand=True, resample=Image.BICUBIC).getchannel("A"))
    return rot


def compose(art: Image.Image, s: int, fill: float, background) -> Image.Image:
    """The turned photograph on a square ground - which may be nothing at all -
    with a soft shadow under it."""
    canvas = Image.new("RGBA", (s, s), background)
    inner = s * fill
    k = min(inner / art.width, inner / art.height)
    a = art.resize((max(1, int(art.width * k)), max(1, int(art.height * k))),
                   Image.LANCZOS)
    x, y = (s - a.width) // 2, (s - a.height) // 2

    # Soft edges. Erode by the blur radius, then blur, so the whole fade lives
    # inside the picture and the 100%-opaque part still reaches nearly to the
    # true edge.
    r = max(1, round(s * FEATHER))
    alpha = a.getchannel("A")
    alpha = alpha.filter(ImageFilter.MinFilter(2 * r + 1))
    alpha = alpha.filter(ImageFilter.GaussianBlur(r))
    a.putalpha(alpha)

    # A dark photograph still needs an edge to read as an object, on a mid
    # ground or on a dark taskbar it would otherwise dissolve into. Taken from
    # the softened alpha, so the shadow follows the same outline.
    shadow = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    shadow.paste((0, 0, 0, 130), (x + max(1, s // 128), y + max(1, s // 96)), a)
    canvas = Image.alpha_composite(
        canvas, shadow.filter(ImageFilter.GaussianBlur(max(1, s / 72))))

    canvas.paste(a, (x, y), a)
    return canvas


def write(path: pathlib.Path, img: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, format="PNG")
    print(f"wrote {path.relative_to(HERE)} ({path.stat().st_size} bytes, {img.width}x{img.height})")


def android(art: Image.Image) -> None:
    """The adaptive icon, its legacy fallback, and the two XML files that name
    them. A legacy square drawable is masked or letterboxed by every modern
    launcher, which is a second source of bars that no amount of transparency
    would have removed."""
    for name, k in DENSITIES.items():
        fg = compose(art, int(108 * k), FILL_ADAPTIVE, (0, 0, 0, 0))
        write(RES / f"mipmap-{name}" / "ic_launcher_foreground.png", fg)
        legacy = compose(art, int(48 * k), FILL_DESKTOP, CLEAR)
        write(RES / f"mipmap-{name}" / "ic_launcher.png", legacy)

    r, g, b, _ = BACKGROUND
    (RES / "values").mkdir(parents=True, exist_ok=True)
    (RES / "values" / "ic_launcher_background.xml").write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n'
        "<resources>\n"
        f'    <color name="ic_launcher_background">#{r:02X}{g:02X}{b:02X}</color>\n'
        "</resources>\n")
    print("wrote platform/android/res/values/ic_launcher_background.xml")

    (RES / "mipmap-anydpi-v26").mkdir(parents=True, exist_ok=True)
    adaptive = (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
        '    <background android:drawable="@color/ic_launcher_background"/>\n'
        '    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>\n'
        "</adaptive-icon>\n")
    for leaf in ("ic_launcher.xml", "ic_launcher_round.xml"):
        (RES / "mipmap-anydpi-v26" / leaf).write_text(adaptive)
        print(f"wrote platform/android/res/mipmap-anydpi-v26/{leaf}")

    # The old square, kept in step rather than left to rot: it was a hand-placed
    # copy this script did not write, so regenerating the icon used to change
    # every platform except the one that asked.
    write(RES / "drawable" / "icon.png",
          compose(art, PNG_SIZE, FILL_DESKTOP, CLEAR))


def main() -> None:
    art = turned(Image.open(SOURCE).convert("RGBA"))

    write(PNG, compose(art, PNG_SIZE, FILL_DESKTOP, CLEAR))

    ICO.parent.mkdir(parents=True, exist_ok=True)
    # ICO carries a full alpha channel, so Windows gets the transparent one too.
    frames = [compose(art, s, FILL_DESKTOP, CLEAR) for s in ICO_SIZES]
    frames[-1].save(ICO, format="ICO", sizes=[(s, s) for s in ICO_SIZES])
    print(f"wrote installer/agape48.ico ({ICO.stat().st_size} bytes, {len(ICO_SIZES)} sizes)")

    android(art)


if __name__ == "__main__":
    main()
