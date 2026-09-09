#!/usr/bin/env python3
"""Build the application icon from a photograph of a real 48GX.

One piece of art, every platform, so they cannot drift apart:

    assets/icon.png            bundled in the binary; QGuiApplication::
                               setWindowIcon uses it, which is what the window
                               manager and the taskbar draw on EVERY platform
    installer/agape48.ico      Windows installer, Start-menu shortcut,
                               Add/Remove
    platform/linux/hicolor/    the freedesktop icon theme at seven sizes: the
                               menu entry, the dock and Alt-Tab
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

    the desktop PNG, the .ico and the legacy square     transparent, and the
                tilted photograph floats on whatever is behind it
    the adaptive FOREGROUND layer     transparent at the corners, but scaled to
                cover the launcher's mask completely - see FILL_ADAPTIVE
    the adaptive BACKGROUND layer     the slate, which by design is now only
                seen while a launcher is animating the icon

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
HICOLOR = HERE / "platform" / "linux" / "hicolor"

# Windows picks whichever of these fits: 16 in a title bar, 32 on the desktop,
# 256 in large-icon view. The PNG is one 256 and Qt scales it down itself.
#
# The same seven are what Linux gets as separate files under hicolor, for the
# same reason and with the same numbers: a menu draws 24 or 32, Alt-Tab draws
# 48 or 64, and a desktop shell reaches for 128 or 256. Downscaling a 256 by
# hand at draw time is exactly what these sizes exist to avoid.
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
PNG_SIZE = 256

ANGLE = 30                              # counter-clockwise

# Gert's, given as three numbers: "make the background r66,g75,b92", and then
# halved on 2026sep08 after he saw it cut into a circle on the launcher:
# "Works! I see a circle with the calculator in it. Please make the background
# half as dark."
#
# HALF AS DARK, not half as bright - those are opposite operations and only one
# of them is what he asked for. Darkness is 1 - L in HSL, so halving it takes
# L from 0.310 to 0.655 while hue (219 degrees) and saturation (0.165) stay
# exactly where they were. That is the difference between a lighter version of
# HIS slate and a different colour that happens to be paler: rgb(66,75,92)
# becomes rgb(153,163,181). Used ONLY where transparency is not allowed - see
# the note at the top.
BACKGROUND = (153, 163, 181, 255)
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
# TWO NUMBERS, because the two platforms want opposite things, and 2026sep08
# is when that became clear. Gert, looking at the launcher: "Why the launcher
# shows me a circle in the background of the icon instead of transparency?"
#
# It is showing a circle because on Android there is no such thing as a
# free-form icon any more. An adaptive icon is two layers and the LAUNCHER
# chooses the silhouette - a circle here, a squircle on other phones, a
# rounded square elsewhere - and cuts both layers with it, so every icon on the
# home screen is the same shape on purpose. Transparency cannot defeat that;
# the background layer is required to be opaque, and a launcher handed a
# transparent one paints it black, which is the black bars this whole icon
# started as.
#
# What CAN be done is leave the launcher's shape nothing of ours to fill: scale
# the photograph until it covers the whole mask, and the circle stops being a
# coloured plate with a picture on it and becomes a circle of picture. 1.6
# rather than 1.5: at 1.5 the circle is covered but the squircle still shows a
# wedge at one corner, and phones do not agree on the shape. Measured inside the
# mask - uncovered area at the 72dp the launcher shows: 10.65% at 1.25, 0.04% at
# 1.5, 0.00% at 1.6, and 0.67% at 1.6 even for the 80dp a launcher reaches
# during parallax.
#
# The desktop keeps 1.25 and its transparency, because there the icon really is
# free-form and the tilt is the whole point. Past about 1.4 the cut corners meet
# and the picture becomes a plain square, which on a transparent desktop icon
# would throw the rotation away - so the number that is right for Android is
# wrong there, and the other way about.
FILL_DESKTOP = 1.25
FILL_ADAPTIVE = 1.6

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


def linux(art: Image.Image) -> None:
    """The freedesktop icon theme - one PNG per size, named after the Icon= key
    of platform/linux/agape48.desktop, which is what a menu, a dock and Alt-Tab
    all look up.

    WRITTEN INTO THE TREE AND COMMITTED, like the Android res/ PNGs and unlike
    installer/agape48.ico, which is generated and gitignored. The .ico can be
    because Windows builds go through make-icon.py anyway; the Linux ones may
    not, because `cmake --install` installs them and a fresh clone must be able
    to do that without Python and Pillow. The cost is 190 KB of PNG in git and
    a rule: change the art, run this script, commit what it wrote."""
    for s in ICO_SIZES:
        write(HICOLOR / f"{s}x{s}" / "apps" / "agape48.png",
              compose(art, s, FILL_DESKTOP, CLEAR))


def main() -> None:
    art = turned(Image.open(SOURCE).convert("RGBA"))

    write(PNG, compose(art, PNG_SIZE, FILL_DESKTOP, CLEAR))

    ICO.parent.mkdir(parents=True, exist_ok=True)
    # ICO carries a full alpha channel, so Windows gets the transparent one too.
    frames = [compose(art, s, FILL_DESKTOP, CLEAR) for s in ICO_SIZES]
    frames[-1].save(ICO, format="ICO", sizes=[(s, s) for s in ICO_SIZES])
    print(f"wrote installer/agape48.ico ({ICO.stat().st_size} bytes, {len(ICO_SIZES)} sizes)")

    linux(art)
    android(art)


if __name__ == "__main__":
    main()
