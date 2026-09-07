#!/usr/bin/env python3
"""Draw Agape48's calculator face, and write layout.json from the same geometry.

This replaced a version that composited Droid48's key PNGs. Drawing the keys
ourselves is barely more code and it wins three things at once:

  - No GPLv3 art, so Agape48 keeps the GPL-2.0-or-later of its x48 core and
    owes nobody attribution for pictures.
  - Every dimension is a parameter. The key art was fixed, so "the numbers are
    too big" and "the gaps are too wide" were unfixable with it.
  - A pressed state, which per-key PNGs would have needed a second copy of.

The legends are the HP 48GX's own printed labels - facts about the machine, not
anybody's creative work. They are transcribed against Droid48's HPView.java
arrays because that is a convenient checked source for all 49 of them.

Nothing here ships in the binary except the PNG it writes: the text is rendered
at build time, so no font is redistributed.

WHERE THE BUTTON LOOK COMES FROM
--------------------------------
Droid48 draws its buttons twice: once as the 49 fixed PNGs, and once
parametrically in HPView.drawButton(). The parametric one is the honest source
for "what are the proportions", and every number below marked [D48] is lifted
from it:

    gradient    LinearGradient(top -> bottom, 0xFF081021, 0xFF6B7173)   line 229
    border      black, 1 dp stroke                                      line 567
    radius      5 dp                                                    line 214
    cap inset   8 dp each side, 10 dp above, 5 dp below                 line 207
    centre text 17 dp                                                   line 512
    legend/alpha 11 dp  (= 0.65 of the centre text)                     line 511
    left shift  0xFFBD92BD, right shift 0xFF73DFC6                      line 505
    menu key    white fill inset 4 dp inside the same gradient cap      line 236

That last line is the answer to "the buttons there are all the same": there is
exactly ONE button in Droid48 - a gradient cap with a black frame - and the
menu and shift keys are that same cap with a flat colour laid inside it. This
file works the same way, which is what three SVGs would have bought, without
the three SVGs: a cap is DRAWN at the size it needs, so the wide ENTER key and
the short menu keys keep the corner radius and border weight of every other
key instead of having them stretched by a scale factor.
"""
import json
import os
import re

from PIL import Image, ImageColor, ImageDraw, ImageFont

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "assets", "skins", "default")
FONT_B = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"
FONT_R = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf"

# --- the 49 keys, in grid order ---------------------------------------------
# name, centre label, left-shift legend, right-shift legend, alpha letter
#
# The alpha letters are the real machine's: A-F on the six soft keys, G-L on
# the MTH row, M-R on the ' row, S-X on the SIN row, then Y on +/- and Z on
# EEX. They used to start at MTH, which put every letter one row out and
# disagreed with qml/Agape48Keymap.qml - so the face said STO was H while the
# H key pressed PRG. The keymap was right; this table was wrong.
K = [
    ("A", "", "", "", "A"), ("B", "", "", "", "B"), ("C", "", "", "", "C"),
    ("D", "", "", "", "D"), ("E", "", "", "", "E"), ("F", "", "", "", "F"),

    ("MTH", "MTH", "RAD", "POLAR", "G"), ("PRG", "PRG", "", "CHARS", "H"),
    ("CST", "CST", "", "MODES", "I"),    ("VAR", "VAR", "", "MEMORY", "J"),
    ("UP", "▲", "", "STACK", "K"),       ("NXT", "NXT", "PREV", "MENU", "L"),

    ("QUOTE", "'", "UP", "HOME", "M"),   ("STO", "STO", "DEF", "RCL", "N"),
    ("EVAL", "EVAL", "NUM", "UNDO", "O"),("LEFT", "◀", "PICTURE", "", "P"),
    ("DOWN", "▼", "VIEW", "", "Q"),      ("RIGHT", "▶", "SWAP", "", "R"),

    ("SIN", "SIN", "ASIN", "∂", "S"),    ("COS", "COS", "ACOS", "∫", "T"),
    ("TAN", "TAN", "ATAN", "Σ", "U"),    ("SQRT", "√x", "x²", "ˣ√y", "V"),
    ("POWER", "yˣ", "10ˣ", "LOG", "W"),  ("INV", "1/x", "eˣ", "LN", "X"),

    ("ENTER", "ENTER", "EQUATION", "MATRIX", ""),
    ("NEG", "+/−", "EDIT", "CMD", "Y"),  ("EEX", "EEX", "PURG", "ARG", "Z"),
    ("DEL", "DEL", "CLEAR", "", ""),     ("BS", "←", "DROP", "", ""),

    ("ALPHA", "α", "USER", "ENTRY", ""), ("N7", "7", "SOLVE", "", ""),
    ("N8", "8", "PLOT", "", ""),         ("N9", "9", "SYMBOLIC", "", ""),
    ("DIV", "÷", "( )", "#", ""),

    ("SHL", "", "", "", ""),             ("N4", "4", "TIME", "", ""),
    ("N5", "5", "STAT", "", ""),         ("N6", "6", "UNITS", "", ""),
    ("MUL", "×", "[ ]", "_", ""),

    ("SHR", "", "", "", ""),             ("N1", "1", "I/O", "", ""),
    ("N2", "2", "LIBRARY", "", ""),      ("N3", "3", "EQ LIB", "", ""),
    ("MINUS", "−", "« »", "\" \"", ""),

    ("ON", "ON", "CONT", "OFF", ""),     ("N0", "0", "=", "→", ""),
    ("PERIOD", ".", ",", "↵", ""),       ("SPC", "SPC", "π", "∡", ""),
    ("PLUS", "+", "{ }", ": :", ""),
]
ROW_LEN = [6, 6, 6, 6, 5, 5, 5, 5, 5]

# The 48GX prints a second, smaller word inside a few caps.
SUBLABEL = {"ON": "CANCEL"}

# --- the tunables ------------------------------------------------------------
# Every number that decides how the face looks lives in tools/face.json, which
# this script writes with these defaults the first time it runs, and tops up
# with any knob added later while keeping the values already in it. Edit that
# file and re-run; nothing here needs touching. That is the whole reason the
# art is drawn rather than borrowed - with fixed key PNGs none of it would be
# a knob.
#
# The face is sized FROM the LCD outwards, not the other way round: a real 48GX
# has its black display area running nearly edge to edge with the keypad inset
# inside the same line, so "edge" sets both.
DEFAULTS = {
    "lcd_zoom":       6,    # pixel size of the 131x64 display in face pixels
    "glass_pad":     10,    # green visible beyond the pixel area, all round
    "bezel_pad":     10,    # black surround outside the glass
    "edge":          14,    # bevel from the bezel to the window edge; also the
                            #   keypad's inset, so the two line up
    "gap":           22,    # between key caps. WIDEN THIS TO NARROW THE KEYS -
                            #   the column pitch still spans the full width, so
                            #   the space moves between keys instead of leaving
                            #   dead margin at the sides. [D48] uses 16 dp,
                            #   which at this face width would be about 38
    "band_h":        22,    # strip above a cap holding its two shift legends
    "cap_h":         80,    # key height. Taller than wide is what reads as slim
    "row_gap":        6,    # below a cap; the alpha letter lives beside it
    "menu_cap_h":    30,
    "menu_gap":       8,
    "top_margin":    12,
    "brand_h":       32,
    "lcd_gap":       22,    # between the display bezel and the menu row
    "bottom_margin": 16,
    "hit_pad":        4,    # frame around a cap that still counts as a press
    "ann_zoom":      4,        # 0 = the same zoom as the LCD
    "ann_gap":       0,        # 0 = spread evenly across the display


    # --- the button ----------------------------------------------------------
    # One cap, drawn at whatever size the grid asks for. [D48] proportions.
    "cap_radius_frac": 0.16,  # corner radius as a fraction of cap height. Also
                              #   hard-coded in qml/Keypad.qml, which draws the
                              #   pressed highlight over the same rectangle
    "cap_frame_w":     3,     # black frame round every cap
    "cap_grad_top":  [26, 30, 46],    # [D48] 0xFF081021, lifted off the body
    "cap_grad_bot":  [107, 113, 115], # [D48] 0xFF6B7173
    "menu_inset":      8,     # white fill inset inside the frame [D48] 4 dp
    "shift_inset":     3,     # colour fill inset inside the frame [D48] 1 dp
    "arrow_frac":      0.42,  # shift hook, as a fraction of the cap

    "font_centre_short": 28,   # ceiling for 1-2 character labels; long ones and
    "font_centre_long":  25,   #   anything that does not fit shrink to suit
    "font_legend":       18,   # [D48] runs the legends at 0.65 of the centre
    "font_alpha":        17,   #   text, and the alpha letter at the same size
    "font_sub":          13,   # CANCEL under ON
    "font_brand_maker":  17,
    "font_brand_model":  22,
    # The calculator's own name, drawn by QML rather than printed here. Gert,
    # 2026sep07: "make the top center calculator name bigger, about 1.5x the
    # size of the '48GX' text" - 22 x 1.5.
    "font_brand_plate":  33,

    "colour_body":       [34, 34, 38],
    "colour_body_edge":  [62, 62, 70],
    "colour_bezel":      [18, 18, 20],
    "colour_cap_frame":  [0, 0, 0],
    "colour_menu_cap":   [246, 248, 244],   # [D48] Color.WHITE
    "colour_text":       [240, 240, 242],
    "colour_left_shift": [189, 146, 189],   # [D48] 0xFFBD92BD
    "colour_right_shift":[115, 223, 198],   # [D48] 0xFF73DFC6
    "colour_alpha":      [228, 228, 232],
    "colour_brand":      [198, 170, 96],
    "colour_lcd_bg":     "#9fbf7a",
    "colour_lcd_pixel":  "#0d1a0d",
}

# Knobs that used to exist. A face.json still carrying them is not an error -
# it just no longer decides anything, because the flat fill and the hand-drawn
# top edge they described were replaced by the gradient.
RETIRED = {"colour_cap", "colour_cap_top", "colour_cap_edge",
           "ann_w", "ann_h"}

CFG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "face.json")


def load_config():
    cfg = dict(DEFAULTS)
    if os.path.exists(CFG_PATH):
        with open(CFG_PATH) as fh:
            user = json.load(fh)
        unknown = sorted(set(user) - set(DEFAULTS) - RETIRED)
        if unknown:
            raise SystemExit("face.json has keys this script does not know: "
                             + ", ".join(unknown))
        dead = sorted(set(user) & RETIRED)
        cfg.update({k: v for k, v in user.items() if k in DEFAULTS})
        if dead or set(DEFAULTS) - set(user):
            # Top the file up so it always shows every knob there is, keeping
            # whatever was already set.
            with open(CFG_PATH, "w") as fh:
                json.dump(cfg, fh, indent=2)
                fh.write("\n")
            if dead:
                print("dropped retired keys from face.json: " + ", ".join(dead))
            print("face.json topped up with the knobs added since it was written")
    else:
        with open(CFG_PATH, "w") as fh:
            json.dump(DEFAULTS, fh, indent=2)
            fh.write("\n")
        print("wrote %s with the defaults - edit it and re-run" % CFG_PATH)
    return cfg


C = load_config()

# --- geometry, all derived ---------------------------------------------------
LCD_ZOOM    = C["lcd_zoom"]
LCD_W, LCD_H = 131 * LCD_ZOOM, 64 * LCD_ZOOM
GLASS_PAD   = C["glass_pad"]
BEZEL_PAD   = C["bezel_pad"]
EDGE        = C["edge"]

GLASS_W     = LCD_W + 2 * GLASS_PAD
BEZEL_W     = GLASS_W + 2 * BEZEL_PAD
FACE_W      = BEZEL_W + 2 * EDGE

MARGIN_X    = EDGE
PAD_W       = FACE_W - 2 * MARGIN_X
GAP         = C["gap"]
BAND_H      = C["band_h"]
CAP_H       = C["cap_h"]
ROW_GAP     = C["row_gap"]
ROW_PITCH   = BAND_H + CAP_H + ROW_GAP
MENU_CAP_H  = C["menu_cap_h"]
MENU_PITCH  = MENU_CAP_H + C["menu_gap"]

# The annunciators come from the machine, not from me: x48 vendored the real
# HP 48 bitmaps and they are sitting in this repo. 15x12 LCD pixels each, and
# they are LCD pixels, so they are drawn at the display's own zoom on the
# display's own grid - square, hard-edged, the same size as the dots next to
# them. The two shift ones are the reason this matters: they are not arrows,
# they are solid blocks with an arrow knocked out of them in reverse video,
# which is not something anybody would guess.
ANN_SRC   = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "..", "src", "core", "x48", "bitmaps")
ANN_ZOOM  = C["ann_zoom"] or LCD_ZOOM
ANN_GAP   = C["ann_gap"]


def read_xbm(name):
    """(width, height, [rows of 0/1]) from one of the vendored .h bitmaps."""
    src = open(os.path.join(ANN_SRC, name + ".h")).read()
    w = int(re.search(r"_width (\d+)", src).group(1))
    h = int(re.search(r"_height (\d+)", src).group(1))
    by = [int(x, 16) for x in
          re.findall(r"0x([0-9a-fA-F]{2})", re.search(r"\{(.*?)\}", src, re.S).group(1))]
    stride = (w + 7) // 8
    rows = [[(by[r * stride + c // 8] >> (c % 8)) & 1 for c in range(w)]
            for r in range(h)]
    return w, h, rows


ANN_BM   = {a: read_xbm("ann_" + a)
            for a in ("left", "right", "alpha", "battery", "busy", "io")}
ANN_PX_W = max(b[0] for b in ANN_BM.values())
ANN_PX_H = max(b[1] for b in ANN_BM.values())
ANN_W, ANN_H = ANN_PX_W * ANN_ZOOM, ANN_PX_H * ANN_ZOOM

TOP_MARGIN, BRAND_H = C["top_margin"], C["brand_h"]
LCD_GAP, BOTTOM_MARGIN = C["lcd_gap"], C["bottom_margin"]
ANN_LCD_GAP = 6
GLASS_H     = ANN_H + ANN_LCD_GAP + LCD_H + 2 * GLASS_PAD
BEZEL_H     = BEZEL_PAD + GLASS_H + BEZEL_PAD
BEZEL_Y     = TOP_MARGIN + BRAND_H
KEYPAD_Y    = BEZEL_Y + BEZEL_H + LCD_GAP
FACE_H      = KEYPAD_Y + MENU_PITCH + 8 * ROW_PITCH + BOTTOM_MARGIN

HIT_PAD     = C["hit_pad"]

RADIUS_FRAC = C["cap_radius_frac"]
FRAME_W     = C["cap_frame_w"]
MENU_INSET  = C["menu_inset"]
SHIFT_INSET = C["shift_inset"]
ARROW_FRAC  = C["arrow_frac"]

BODY       = tuple(C["colour_body"])
BODY_EDGE  = tuple(C["colour_body_edge"])
BEZEL      = tuple(C["colour_bezel"])
FRAME      = tuple(C["colour_cap_frame"])
GRAD_TOP   = tuple(C["cap_grad_top"])
GRAD_BOT   = tuple(C["cap_grad_bot"])
MENU_CAP   = tuple(C["colour_menu_cap"])
TEXT       = tuple(C["colour_text"])
LSHIFT     = tuple(C["colour_left_shift"])
RSHIFT     = tuple(C["colour_right_shift"])
ALPHA_TXT  = tuple(C["colour_alpha"])
BRAND      = tuple(C["colour_brand"])
LCD_BG     = C["colour_lcd_bg"]
LCD_PIXEL  = C["colour_lcd_pixel"]

SS = 4      # supersampling for every rounded corner and every arrow


def ann_glyph(kind):
    """One annunciator, at the display's zoom, in the colour a lit pixel is."""
    w, h, rows = ANN_BM[kind]
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = im.load()
    ink = ImageColor.getrgb(LCD_PIXEL) + (255,)
    for y in range(h):
        for x in range(w):
            if rows[y][x]:
                px[x, y] = ink
    # NEAREST on purpose: these are display pixels and must stay square, the
    # same as LcdItem's own filtering.
    return im.resize((w * ANN_ZOOM, h * ANN_ZOOM), Image.NEAREST)


def f(path, size):
    return ImageFont.truetype(path, size)


def fit(draw, text, path, box_w, start, floor=8, box_h=None, anchor=None):
    """Largest size at or below `start` that fits box_w, and box_h if given."""
    for s in range(int(start), floor - 1, -1):
        fnt = f(path, s)
        if draw.textlength(text, font=fnt) > box_w:
            continue
        if box_h is not None:
            bb = draw.textbbox((0, 0), text, font=fnt, anchor=anchor or "ls")
            if bb[3] - bb[1] > box_h:
                continue
        return fnt
    return f(path, floor)


# --- the one button ----------------------------------------------------------

def _rr_mask(w, h, r):
    """Anti-aliased rounded-rectangle mask, drawn big and shrunk."""
    m = Image.new("L", (w * SS, h * SS), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, w * SS - 1, h * SS - 1],
                                        r * SS, fill=255)
    return m.resize((w, h), Image.LANCZOS)


def _vgrad(w, h, top, bot):
    """Vertical linear gradient, one column stretched sideways."""
    strip = Image.new("RGB", (1, h))
    px = strip.load()
    span = max(1, h - 1)
    for y in range(h):
        t = y / span
        px[0, y] = tuple(round(top[i] + (bot[i] - top[i]) * t) for i in range(3))
    return strip.resize((w, h), Image.BICUBIC)


def key_cap(face, x, y, w, h, inner=None, inset=0):
    """The single button every key is made of: a black-framed rounded cap with
    a vertical gradient, optionally with a flat colour laid inside it.

    Menu keys pass inner=white, the two shift keys pass their colour. Nothing
    is scaled: the cap is drawn at exactly (w, h), so the frame stays one
    weight and the corners one radius on the widest and the shortest key
    alike. That is the part a set of scaled SVGs could not have given us.
    """
    x, y, w, h = int(x), int(y), int(w), int(h)
    r = max(2, round(h * RADIUS_FRAC))
    cap = Image.new("RGB", (w, h), FRAME)
    iw, ih = w - 2 * FRAME_W, h - 2 * FRAME_W
    cap.paste(_vgrad(iw, ih, GRAD_TOP, GRAD_BOT), (FRAME_W, FRAME_W),
              _rr_mask(iw, ih, max(1, r - FRAME_W)))
    if inner is not None:
        jw, jh = w - 2 * inset, h - 2 * inset
        # Same corner radius as the cap, not a shrunken one: [D48] reuses
        # `radius` for the inner fill, and a big inset otherwise squares the
        # white menu key off.
        cap.paste(Image.new("RGB", (jw, jh), inner), (inset, inset),
                  _rr_mask(jw, jh, r))
    face.paste(cap, (x, y), _rr_mask(w, h, r))


def aa_polygon(face, pts, colour):
    """Anti-aliased polygon; PIL's own is hard-edged."""
    x0 = int(min(p[0] for p in pts)) - 1
    y0 = int(min(p[1] for p in pts)) - 1
    w = int(max(p[0] for p in pts)) + 2 - x0
    h = int(max(p[1] for p in pts)) + 2 - y0
    m = Image.new("L", (w * SS, h * SS), 0)
    ImageDraw.Draw(m).polygon([((px - x0) * SS, (py - y0) * SS) for px, py in pts],
                              fill=255)
    face.paste(Image.new("RGB", (w, h), colour), (x0, y0),
               m.resize((w, h), Image.LANCZOS))


def hook_arrow(face, cx, cy, cw, ch, left, colour=(0, 0, 0)):
    """The shift glyph: an arm with an arrowhead on one end that turns a right
    angle and drops to the corner. Droid48 draws this on both shift keys; the
    real 48GX leaves them blank, and the arrow is the more useful of the two.
    """
    bw, bh = cw * ARROW_FRAC * 1.25, ch * ARROW_FRAC
    x0 = cx + (cw - bw) / 2
    y0 = cy + (ch - bh) / 2
    x1, y1 = x0 + bw, y0 + bh
    t = bh * 0.30              # bar thickness
    hh = t * 1.25              # arrowhead half-height
    hl = t * 1.40              # arrowhead length
    ym = y0 + hh               # centre line of the horizontal arm
    if left:
        pts = [(x0, ym), (x0 + hl, ym - hh), (x0 + hl, ym - t / 2),
               (x1, ym - t / 2), (x1, y1), (x1 - t, y1),
               (x1 - t, ym + t / 2), (x0 + hl, ym + t / 2), (x0 + hl, ym + hh)]
    else:
        pts = [(x1, ym), (x1 - hl, ym - hh), (x1 - hl, ym - t / 2),
               (x0, ym - t / 2), (x0, y1), (x0 + t, y1),
               (x0 + t, ym + t / 2), (x1 - hl, ym + t / 2), (x1 - hl, ym + hh)]
    aa_polygon(face, pts, colour)


def main():
    face = Image.new("RGB", (FACE_W, FACE_H), BODY)
    d = ImageDraw.Draw(face)
    d.rounded_rectangle([2, 2, FACE_W - 3, FACE_H - 3], 28, outline=BODY_EDGE, width=3)

    d.text((EDGE + 6, TOP_MARGIN + 8), "HEWLETT·PACKARD",
           font=f(FONT_B, C["font_brand_maker"]), fill=BRAND)
    # Left of the corner, not in it: the QML menu button lives in that corner
    # and was printing itself through the middle of "48GX".
    d.text((FACE_W - EDGE - 34, TOP_MARGIN + 6), "48GX",
           font=f(FONT_B, C["font_brand_model"]), fill=BRAND, anchor="ra")

    # The nameplate band, measured here and written into layout.json for QML to
    # draw the open calculator's name into at run time. Gert, both-05 line 37:
    # "I'd like the first 20 letters of the name of the current calculator to be
    # shown between the 'HEWLETT-PACKARD' and the '48GX' at the top, same font,
    # center-aligned in the middle." Twenty became thirty once he had seen it -
    # both-06 line 20: "Looks great! Increate the limit to 30!"
    #
    # Not baked, because the name changes while the program runs. Not hardcoded
    # in Calculator.qml either: every other number about this face comes from
    # here, and a second skin would put its own nameplate somewhere else.
    #
    # Symmetric about the face's centre line, so "centred in the middle" is
    # literally true rather than centred on whatever gap happens to be left, and
    # so it cannot reach either printed word. 445 px at this size. Thirty
    # letters of a name anyone would type measure about 275 of that; thirty
    # capital Ws measure 506, so QML shrinks a name that wide to fit rather than
    # eating letters it was told to show.
    plate_top = TOP_MARGIN + 8
    plate_font = f(FONT_B, C["font_brand_maker"])
    gap = 16
    left = EDGE + 6 + d.textlength("HEWLETT·PACKARD", font=plate_font) + gap
    right = (FACE_W - EDGE - 34
             - d.textlength("48GX", font=f(FONT_B, C["font_brand_model"])) - gap)
    half = min(FACE_W / 2 - left, right - FACE_W / 2)
    ascent, descent = plate_font.getmetrics()

    # LCD bezel, glass and the annunciator strip above it. The bezel hugs the
    # glass rather than spanning the body: dogfood #3 showed a wide black frame
    # around a small screen, which is not what a 48GX looks like.
    # The annunciators live INSIDE the glass, on the green, above the dot
    # matrix - which is where they are on the machine. They used to sit on the
    # bezel above it, painted #101010 on a #121214 bezel, which is why Gert
    # could never see one light up (dogfood #13 line 12). Same bezel height as
    # before, so nothing else on the face moves: the green simply grows upward
    # to take in the strip and its gap.
    glass_w, glass_h = LCD_W + 2 * GLASS_PAD, GLASS_H
    glass_x = (FACE_W - glass_w) // 2
    bez_x0, bez_x1 = glass_x - BEZEL_PAD, glass_x + glass_w + BEZEL_PAD
    d.rounded_rectangle([bez_x0, BEZEL_Y, bez_x1, BEZEL_Y + BEZEL_H], 12, fill=BEZEL)
    glass_y = BEZEL_Y + BEZEL_PAD
    d.rectangle([glass_x, glass_y, glass_x + glass_w, glass_y + glass_h], fill=LCD_BG)
    ann_y = glass_y + GLASS_PAD
    lcd_x, lcd_y = glass_x + GLASS_PAD, ann_y + ANN_H + ANN_LCD_GAP

    # One small PNG per annunciator, drawn here rather than at run time. A
    # QML Text would need the glyph in a font on Android and on Windows, and
    # an hourglass or a pair of exchange arrows is exactly the sort of thing a
    # phone font does not have. Baking them means the six look the same
    # everywhere and the frontend only has to show and hide an Image.
    anns, ann_ids = [], [("left", 1), ("right", 2), ("alpha", 4),
                         ("battery", 8), ("busy", 16), ("io", 32)]
    # Spread across the width of the dot matrix, as they are on the machine,
    # rather than huddled in the middle.
    gap = ANN_GAP or (LCD_W - 6 * ANN_W) // 5
    total = 6 * ANN_W + 5 * gap
    ax = lcd_x + (LCD_W - total) // 2
    for i, (aid, bit) in enumerate(ann_ids):
        name = "ann_%s.png" % aid
        ann_glyph(aid).save(os.path.join(OUT_DIR, name))
        anns.append({"id": aid, "bit": bit, "image": name,
                     "rect": [ax + i * (ANN_W + gap), ann_y, ANN_W, ANN_H]})

    # keys
    keys = []
    idx, y = 0, KEYPAD_Y
    for row, n in enumerate(ROW_LEN):
        menu = row == 0
        cap_h = MENU_CAP_H if menu else CAP_H
        band = 0 if menu else BAND_H
        for col in range(n):
            name, centre, lsh, rsh, alpha = K[idx]
            if n == 6:
                x0 = MARGIN_X + round(col * PAD_W / 6)
                x1 = MARGIN_X + round((col + 1) * PAD_W / 6)
            elif name == "ENTER":
                x0, x1 = MARGIN_X, MARGIN_X + round(2 * PAD_W / 6)
            elif row == 4:
                x0 = MARGIN_X + round((col + 1) * PAD_W / 6)
                x1 = MARGIN_X + round((col + 2) * PAD_W / 6)
            else:
                x0 = MARGIN_X + round(col * PAD_W / 5)
                x1 = MARGIN_X + round((col + 1) * PAD_W / 5)
            cellw = x1 - x0
            cx, cw = x0 + GAP // 2, cellw - GAP
            cy = y + band

            # The two shift legends, centred over the CELL as one group with a
            # third of the slack between them - which is what [D48] does, and
            # is why "RAD POLAR" is allowed to be wider than the MTH cap. They
            # used to be pinned to the cap's outer corners instead. Their
            # baseline sits just above the cap, so the space in the band is
            # above the text rather than between the text and the key.
            if lsh or rsh:
                lbox = (cellw * 0.52) if (lsh and rsh) else (cellw - 4)
                room = band - 3
                fl = fit(d, lsh, FONT_B, lbox, C["font_legend"], box_h=room) if lsh else None
                fr = fit(d, rsh, FONT_B, lbox, C["font_legend"], box_h=room) if rsh else None
                lw = d.textlength(lsh, font=fl) if lsh else 0
                rw = d.textlength(rsh, font=fr) if rsh else 0
                pad = (cellw - lw - rw) / 3 if (lsh and rsh) else 0
                gx = x0 + (cellw - (lw + rw + pad)) / 2
                base = cy - 2
                if lsh:
                    d.text((gx, base), lsh, font=fl, fill=LSHIFT, anchor="ls")
                if rsh:
                    d.text((gx + lw + pad, base), rsh, font=fr, fill=RSHIFT, anchor="ls")

            if menu:
                key_cap(face, cx, cy, cw, cap_h, MENU_CAP, MENU_INSET)
            elif name in ("SHL", "SHR"):
                shade = LSHIFT if name == "SHL" else RSHIFT
                key_cap(face, cx, cy, cw, cap_h, shade, SHIFT_INSET)
                hook_arrow(face, cx, cy, cw, cap_h, name == "SHL")
            else:
                key_cap(face, cx, cy, cw, cap_h)

            sub = SUBLABEL.get(name, "")
            if centre:
                # digits and single glyphs were too big in dogfood #3
                start = (C["font_centre_short"] if len(centre) <= 2
                         else C["font_centre_long"])
                fnt = fit(d, centre, FONT_B, cw - 12, start)
                mid = cy + cap_h * (0.40 if sub else 0.50)
                d.text((cx + cw / 2, mid), centre, font=fnt,
                       fill=(24, 24, 28) if menu else TEXT, anchor="mm")
            if sub:
                d.text((cx + cw / 2, cy + cap_h * 0.76), sub,
                       font=fit(d, sub, FONT_B, cw - 14, C["font_sub"]),
                       fill=TEXT, anchor="mm")
            if alpha:
                # Beside the cap, not on it, and sitting on the line below it -
                # [D48] puts the alpha letter at the bottom right of the CELL.
                # It is dropped back inside the cell when the gap is too narrow
                # for it, which is what happens on the last column of a row.
                fa = f(FONT_R, C["font_alpha"])
                aw = d.textlength(alpha, font=fa)
                axp = min(cx + cw + 3, x1 - aw)
                d.text((axp, cy + cap_h + ROW_GAP - 1), alpha, font=fa,
                       fill=ALPHA_TXT, anchor="ls")

            keys.append({
                "id": name.lower(), "key": name,
                "label": centre or name,
                # what the finger gets: the cap plus a 4 px frame, per dogfood #3.
                "rect": [cx - HIT_PAD, cy - HIT_PAD, cw + 2 * HIT_PAD, cap_h + 2 * HIT_PAD],
                # what lights up when it is pressed.
                "cap":  [cx, cy, cw, cap_h],
            })
            idx += 1
        y += MENU_PITCH if menu else ROW_PITCH

    os.makedirs(OUT_DIR, exist_ok=True)
    png = os.path.join(OUT_DIR, "face.png")
    face.save(png, optimize=True)

    layout = {
        "schema": "agape48.skin/1",
        "title": "Agape48 default",
        "author": "drawn by tools/makeface.py",
        "model": "48GX",
        "_note": ("Generated - do not hand-edit, run tools/makeface.py. Each key's "
                  "rect is its cap plus a 4 px frame; the gaps between keys press "
                  "nothing. \"cap\" is the drawn key, which is what the pressed "
                  "highlight covers."),
        "face": {"image": "face.png", "size": [FACE_W, FACE_H]},
        # Drawn by QML, not by this script - see the band's own comment above.
        # "font" is a list because the face is lettered in DejaVu Sans
        # Condensed, which is not on a stock Windows; the alternatives are the
        # nearest condensed grotesques that are, and Qt walks the list. Naming
        # the family rather than shipping the file keeps 600 KB out of a binary
        # whose size is a stated requirement.
        "nameplate": {
            "rect": [round(FACE_W / 2 - half), plate_top,
                     round(2 * half), ascent + descent],
            "pixelSize": C["font_brand_maker"],
            # Phones only. The band's rect stays the printed row's size and the
            # bigger text simply centres in it, so the desktop face is not
            # touched - Gert, 2026sep07, after seeing it on both: "my request
            # was only for Android", "the desktop version look perfect".
            "pixelSizePhone": C["font_brand_plate"],
            "bold": True,
            "color": "#%02x%02x%02x" % BRAND,
            "maxChars": 30,
            # Condensed first, because that is what the face is lettered in.
            # The rest are ordinary grotesques that actually exist somewhere:
            # the list used to run out on Android, where none of the condensed
            # families is installed, and an empty family left Qt to choose - it
            # picked something light and almost cursive, which Gert saw on the
            # phone on 2026sep07: "use a more similar font to it. This slim,
            # almost cursive won't cut." Roboto is Android's own, Segoe UI is
            # Windows', DejaVu Sans and Liberation Sans are the Linux pair.
            "font": ["DejaVu Sans Condensed", "Liberation Sans Narrow",
                     "Arial Narrow", "Roboto Condensed",
                     "Roboto", "Noto Sans", "Segoe UI",
                     "DejaVu Sans", "Liberation Sans", "Arial"],
        },
        "lcd": {"rect": [lcd_x, lcd_y, LCD_W, LCD_H], "zoom": LCD_ZOOM,
                "pixelColor": LCD_PIXEL, "background": LCD_BG},
        "annunciators": anns,
        "keys": keys,
    }
    with open(os.path.join(OUT_DIR, "layout.json"), "w") as fh:
        json.dump(layout, fh, indent=2)
        fh.write("\n")

    print("face %dx%d ratio %.3f  %d bytes | %d keys | lcd %dx%d @(%d,%d) zoom %d"
          % (FACE_W, FACE_H, FACE_W / FACE_H, os.path.getsize(png), len(keys),
             LCD_W, LCD_H, lcd_x, lcd_y, LCD_ZOOM))


if __name__ == "__main__":
    main()
