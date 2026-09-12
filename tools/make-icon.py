#!/usr/bin/env python3
"""Konstrui la aplikaĵan ikonon el fotografaĵo de vera 48GX.

Unu peco da arto, ĉiu platformo, por ke ili ne povu disiĝi:

    assets/icon.png            enmetita en la duumaĵon; QGuiApplication::
                               setWindowIcon uzas ĝin, kaj tio estas kion la
                               fenestroadministranto kaj la taskostrio desegnas
                               sur ĈIU platformo
    installer/agape48.ico      la Vindoza instalilo, la Start-menua ligilo,
                               Aldoni/Forigi, KAJ la ikono enmetita en
                               agape48.exe mem per agape48.rc
    platform/linux/hicolor/    la ikontemo de freedesktop je sep grandoj: la
                               menuero, la doko kaj Alt-Tab
    platform/android/res/...   la adapta ikono: antaŭa tavolo je kvin densoj,
                               fona koloro, kaj la malnova kvadrato por ĉio pli
                               aĝa ol API 26

LA FONTO ESTAS FOTOGRAFAĴO, ne la desegnita vizaĝo, de 2026sep08: "my
wife is a designer, and she made me change my mind... rotate this image 30
degrees counter-clockwise instead, and this will be the base for the adaptive
icon." assets/icon-source.png estas ŝia HP48GXDusty.png, polva proksimfoto de
la malsupra maldekstra parto de vera klavaro - la ŝovklavoj, ON/CANCEL, ENTER -
kopiita en la arbon por ke la konstruo ne dependu de dosiero en Dropbox, kaj kun
forigita EXIF (la propra versinumero de GIMP kaj du tempindikoj; nek fotilo nek
GPS). Forigita per exiftool anstataŭ per rekodigo, do la bilderoj estas
bajt-identaj.

    python tools/make-icon.py

Ĝi ne plu legas la eligon de makeface.py, do la du estas sendependaj: ŝanĝi la
vizaĝon ne plu ŝanĝas la ikonon.

RELANĈI ĜIN ŜANĜAS NENION, kaj tio estis mezurita anstataŭ esperita je
2026sep09: ĉi tiu komputilo havas Pillow 10.2.0, la Vindoza 12.2.0, kaj ĉiu PNG
kiun ĉu unu ĉu la alia skribas estas bildero post bildero identa al la enarbigita
dosiero. Nur la propraj bajtoj de la kodilo diferencas, plus CRLF sur la du
XML-dosieroj kiujn write_text() faras. Do diferenco kiu montras la ikonojn
ŝanĝiĝintaj post relanĉo estas rekodigo, ne nova ikono: `git checkout -- assets
platform/android/res platform/linux/hicolor` kaj enarbigu nur kion vi intencis
ŝanĝi. (Mezurita ankaŭ por la tondo kaj la kontrasto de 2026sep10, sur la
Linuksa komputilo la saman tagon: Pillow 10.2.0 reskribis 18 el la 19 PNG-oj
kiujn Pillow 12.2.0 enarbigis, kaj ĉiuj 18 estas bildero post bildero identaj -
maksimuma diferenco de kanalo 0. Nur la bajtoj de la kodilo moviĝis; la 16x16
eĉ ne tio.)

LA FOTOGRAFAĴO ESTAS TURNITA 30 GRADOJN MALDEKSTRUME. 2026sep08: "one
quirk to make it different from Droid48: Rotate it 30 counter-clockwise. But
this needs to be the new icon for all OSs." La turno nun okazas ANTAŬ la tondo -
vidu FENESTRO - do la klino vivas interne de la bildo anstataŭ esti ĝia silueto,
kaj la ikono povas esti plena ĝis siaj randoj sen nigraj trianguloj ĉe la
anguloj.

TRAVIDEBLA KIE AJN LA FORMATO PERMESAS, kaj la ardezo kie ne. La du postuloj,
en tiu ordo: "make the background r66,g75,b92" kaj poste "or transparent
when possible". Estas ekzakte unu loko kie tio ne eblas: la fona tavolo de
Androida ADAPTA ikono, kiu devas esti opaka kaj kiun iuj lanĉiloj pentras nigra
se ĝi ne estas - nigraj strioj denove, kio estas la plendo per kiu ĉi tio
komenciĝis. Do:

    la labortabla PNG, la .ico kaj la malnova kvadrato     cirklo sur nenio,
                do la fotografaĵo flosas super kio ajn estas malantaŭ ĝi
    la ANTAŬA tavolo de la adapta ikono     kvadrato, ĉar la LANĉILO tenas la
                tondilon tie - vidu FILL_ADAPTIVE
    la FONA tavolo de la adapta ikono     la ardezo, kiu laŭ desegno nun estas
                vidata nur dum lanĉilo animacias la ikonon

La mola ombro restas en ĉiu kazo krom la adapta antaŭa tavolo, kie ĝi nur
manĝus kovron kiun la masko bezonas. Ĝi estas kio malhelpas ke malluma
fotografaĵo dissolviĝu en malluman taskostrion.
"""

import pathlib
from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter

HERE = pathlib.Path(__file__).resolve().parent.parent
SOURCE = HERE / "assets" / "icon-source.png"
PNG = HERE / "assets" / "icon.png"
ICO = HERE / "installer" / "agape48.ico"
RES = HERE / "platform" / "android" / "res"
HICOLOR = HERE / "platform" / "linux" / "hicolor"

# Vindozo elektas kiun ajn el ĉi tiuj kiu konvenas: 16 en titolstrio, 32 sur la
# labortablo, 256 en grandikona vido. La PNG estas unu 256 kaj Qt malgrandigas
# ĝin mem.
#
# La samaj sep estas kion Linukso ricevas kiel apartajn dosierojn sub hicolor,
# pro la sama kaŭzo kaj kun la samaj nombroj: menuo desegnas 24 aŭ 32, Alt-Tab
# desegnas 48 aŭ 64, kaj labortabla ŝelo prenas 128 aŭ 256. Malgrandigi 256 mane
# je desegnotempo estas ekzakte kion ĉi tiuj grandoj ekzistas por eviti.
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
PNG_SIZE = 256

ANGLE = 30                              # maldekstrume

# LA FENESTRO DE GERT, 2026sep11. Li mem tondis ĝin kaj metis ĝin en Dropbox
# kiel Agape48-Cropped-Circle.png: "I made a manual crop... because this one
# looks the coolest what cropped as a circle, showing the purple button, the
# green button and the ON."
#
# NE LIA PNG. Lia dosiero estas 134x134 bilderoj tonditaj el la antaŭrigarda
# folio de 2026sep09, do ĝi jam pasis tra malgrandigo; uzi ĝin kiel fonton
# signifus grandigi ĝin 3,2-oble por la 432-bildera Androida tavolo. Anstataŭe
# lia fenestro estis LOKALIZITA en la fotografaĵo kaj ĉi tie estas rekonstruita
# el la plena rezolucio. La mezuro, per ŝablona kongruo je 46 skaloj: lia tondo
# sidas ĉe k=0,523 de la turnita arto - ekzakte la 1,25-plenigo de tiu tago,
# 320/613 = 0,5224 - kun meza diferenco de kanalo 3,26 el 255, kio estas
# resampliga bruo kaj nenio alia. Tio faras lian 134-bilderan kvadraton
# 256x256 bilderoj de la turnita arto, kio estas PLI da detalo ol la malnova
# ikono havis, ne malpli.
#
# TONDO POST LA TURNO, ne antaŭ ĝi. Lia fenestro estas akse ordigita en la
# TURNITA spaco, do ĝi ne estas esprimebla kiel rektangulo sur la fotografaĵo;
# tial la malnova CROP=(0,240,230,519) malaperis kaj la ordo nun estas turni,
# tondi, agordi. Kion tio ŝanĝas krom la kadro: la silueto de la ikono ne plu
# ESTAS la klinita rektangulo. La klino nun montriĝas en la klavovicoj kiuj
# kuras oblikve tra plena kadro, kaj la kvar travideblaj trianguloj - 44% de la
# tolo je 1,25, kaj la kialo ke la ikono aspektis kiel makuleto - simple ne plu
# ekzistas.
FENESTRO = (170, 289, 426, 545)

# +20%, liaj nombroj de 2026sep10: "Maybe it should be made with just a little
# bit (20%) more contrast and brightness." Aplikataj POST la fenestro, ĉar
# ImageEnhance.Contrast kalkulas la mezvaloron de la bildo kiun oni donas al ĝi:
# enhavigi la forĵetatan parton en tiun mezvaloron signifus agordi la ikonon laŭ
# bilderoj kiujn neniu vidos.
CONTRAST = 1.2
BRIGHTNESS = 1.2

# LA DU ŜOVKLAVOJ, 2026sep11: "Please use this one, making the purple and
# the green colors look more bright and shiny, to catch the eye."
#
# KIAL TIO NE POVAS ESTI FARITA LAŬ NUANCO. Mezurita sur lia fenestro: 87% de
# ĉiuj saturitaj bilderoj kuŝas inter 180 kaj 250 gradoj, la verda ŝovklavo mem
# havas mezan nuancon de 198 gradoj, kaj violkoloro - 270 ĝis 300 - havas 20
# bilderojn en la tuta bildo. La fotografaĵo portas fortan bluan lumon, do la
# violkolora ŝovklavo de vera 48GX aperas blu-blanka kaj la verda aperas
# cejana. Nuanca masko elektus la tutan korpon de la kalkulilo kune kun la du
# klavoj kaj nenio krom la du klavoj.
#
# Do la masko estas GEOMETRIA - la fenestro estas fiksita, do la du klavoj estas
# ĉe fiksitaj koordinatoj - kaj interne de ĝi ĝi elektas la HELAN parton. Tio
# estas la sama regulo por ambaŭ klavoj kaj ĝi estas la ĝusta parto en ambaŭ
# kazoj: la maldekstra ŝovklavo estas malluma kun hela sago sur ĝi, la dekstra
# estas hela plato kun malluma sago. La koloroj mem estas tiuj de vera 48GX
# anstataŭ elektitaj: violkoloro kaj verdo estas kio estas presita sur la vera
# klavaro, kaj la fotografaĵo perdis ilin al la blua lumo.
#
# cx cy rx ry nuanco saturo gajno
TINT_LEFT = (29, 71, 29, 33, 280.0, 0.62, 1.22)
TINT_RIGHT = (68, 151, 36, 25, 138.0, 0.72, 1.25)
TINT_FEATHER = 2                        # bilderoj da malakrigo sur la elipso
TINT_FLOOR = 0.40                       # sub tiu lumeco la bildero ne estas tuŝita
TINT_RAMP = 0.26

# La lia, donita kiel tri nombroj: "make the background r66,g75,b92", kaj poste
# duonigita je 2026sep08 post kiam li vidis ĝin tranĉita en cirklon sur la
# lanĉilo: "Works! I see a circle with the calculator in it. Please make the
# background half as dark."
#
# DUONE TIEL MALLUMA, ne duone tiel hela - tiuj estas kontraŭaj operacioj kaj nur
# unu el ili estas kion li petis. Malluma estas 1 - L en HSL, do duonigi ĝin
# prenas L de 0,310 al 0,655 dum la nuanco (219 gradoj) kaj la satureco (0,165)
# restas ekzakte kie ili estis. Tio estas la diferenco inter pli hela versio de
# LIA ardezo kaj alia koloro kiu hazarde estas pala: rgb(66,75,92) fariĝas
# rgb(153,163,181). Uzata NUR kie travidebleco ne estas permesita - vidu la noton
# supre.
BACKGROUND = (153, 163, 181, 255)
CLEAR = (0, 0, 0, 0)

# CIRKLO SUR LA LABORTABLO, 2026sep11, kaj tio estas lia dua frazo pri la sama
# tondo: "some parts of it at the bottom must be made transparent, but they are
# out of the circle crop."
#
# La partoj kiuj devas fariĝi travideblaj kaj la partoj kiujn cirklo fortranĉas
# estas la samaj partoj, kaj tion oni povas vidi sur lia propra tondo: la
# malsupra maldekstra angulo estas la nigra ombro sub CANCEL kaj la malsupra
# dekstra estas la polva nigra maso sur la tablo. Kvadrata ikono portus ambaŭ
# kiel nigran strion laŭ sia fundo, kio estas la plendo per kiu la tuta ikono
# komenciĝis. Cirklo forigas ekzakte ilin kaj nenion alian.
#
# Kaj ĝi estas la plej granda ikono kiun ĉi tiu kadro permesas: je plenigo 1,0
# la cirklo tuŝas ĉiujn kvar randojn de la tolo.
FILL_DESKTOP = 1.0

# ANDROIDO NE RICEVAS LA CIRKLON, ĉar tie la tondilon tenas la lanĉilo. Adapta
# ikono estas du tavoloj kaj la LANĈILO elektas la silueton - cirklo sur lia
# telefono, kvadratcirklo aŭ rondigita kvadrato sur aliaj - kaj tranĉas ambaŭ
# tavolojn per ĝi. Antaŭa tavolo kiu jam estas cirklo montrus la fonan ardezon
# ĉe la kvar anguloj sur ĉiu telefono kies masko ne estas cirklo. Do la antaŭa
# tavolo estas la plena kvadrato kaj la masko faras la ceteron.
#
# 0,75 ESTAS MEZURO. La tolo estas 108dp, la masko 72dp, kaj lanĉilo atingas
# 80dp dum paralaksa animacio. Kvadrato je plenigo f kovras f*108 dp, do 80dp
# postulas f >= 0,741; 0,75 donas 81dp kaj nul nudan maskon eĉ dum la animacio.
# Kaj ĝi estas la PLEJ MALGRANDA nombro kiu faras tion, kio gravas ĉar ĉio pli
# granda forĵetus arton: je 0,75 la 72dp-cirklo montras 89% de la larĝo de lia
# fenestro, do kion li vidas sur la hejmekrano estas preskaŭ ekzakte la tondo
# kiun li faris.
#
# SEN FADENADO ĉi tie, malkiel ĉie aliloke. Mola rando sub masko estas nur
# perdita kovro: ĝi mangus 2dp ĉe ĉiu flanko kaj devigus pli grandan plenigon
# por rekompensi, kaj neniu iam vidos ĝin, ĉar la masko tranĉas antaŭ ĝi.
FILL_ADAPTIVE = 0.75

# "Oh, and make the edges a bit blurry!" - 2026sep08. Frakcio de la propra
# grando de la ikono anstataŭ fiksa nombro da bilderoj, por ke la moleco aspektu
# same je 16 kaj je 432 anstataŭ igi malgrandan ikonon kaĉo. La fadenado estas
# puŝita INTERNEN unue: malakrigi nur la maskon disvastigus ĝin eksteren super la
# nigro kiun la turno lasas ekster la fotografaĵo, kaj pendigus malpuran
# aŭreolon sur ĉiun randon.
FEATHER = 0.018

# Antaŭaj tavoloj je 108dp, kaj la malnova 48dp-lanĉikono, je la kvin densoj
# kiujn Androido petas.
DENSITIES = {"mdpi": 1, "hdpi": 1.5, "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}


def turned(src: Image.Image) -> Image.Image:
    """Turnita ĉirkaŭ sia centro, kun propra alfa-masko. La fotografaĵo ne havas
    alfa-kanalon, do turni ĝin rekte plenigus la novajn angulojn per nigro - kio
    remetus la striojn, oblikve."""
    solid = Image.new("RGBA", src.size, (255, 255, 255, 255))
    rot = src.rotate(ANGLE, expand=True, resample=Image.BICUBIC)
    rot.putalpha(solid.rotate(ANGLE, expand=True, resample=Image.BICUBIC).getchannel("A"))
    return rot


def tinted(img: Image.Image, spec) -> Image.Image:
    """Unu ŝovklavo, repentrita en sia propra koloro. Vidu TINT_LEFT.

    Pure per Pillow kaj sen numpy, ĉar la sola devigo kiun ĉi tiu skripto havas
    estas Pillow kaj la du komputiloj devas ambaŭ povi lanĉi ĝin."""
    cx, cy, rx, ry, hue, sat, gain = spec
    h, s, v = img.convert("HSV").split()

    # La pezo: la elipso oble la hela parto interne de ĝi. Sub TINT_FLOOR nul,
    # super TINT_FLOOR+TINT_RAMP unu, lineare inter ili - do la rando de la
    # klavo transiras anstataŭ stampiĝi.
    floor, ramp = TINT_FLOOR * 255, TINT_RAMP * 255
    ring = Image.new("L", img.size, 0)
    ImageDraw.Draw(ring).ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=255)
    w = ImageChops.multiply(
        ring.filter(ImageFilter.GaussianBlur(TINT_FEATHER)),
        v.point(lambda p: max(0, min(255, round((p - floor) * 255 / ramp)))))

    flat = lambda value: Image.new("L", img.size, value)
    res = Image.merge("HSV", (
        Image.composite(flat(round(hue / 360 * 255)), h, w),
        Image.composite(flat(round(sat * 255)), s, w),
        Image.composite(v.point(lambda p: min(255, round(p * gain))), v, w),
    )).convert("RGB")
    res.putalpha(img.getchannel("A"))
    return res


def artwork() -> Image.Image:
    """La fotografaĵo turnita, tondita al la elektita fenestro, agordita, kaj kun
    la du ŝovklavoj repentritaj. Ordo gravas dufoje: la turno antaŭ la tondo, ĉar
    lia fenestro estas akse ordigita en la turnita spaco, kaj la kontrasto post
    la tondo, ĉar ImageEnhance mezuras la bildon kiun ĝi ricevas."""
    win = turned(Image.open(SOURCE).convert("RGB")).crop(FENESTRO)
    rgb = ImageEnhance.Contrast(win.convert("RGB")).enhance(CONTRAST)
    rgb = ImageEnhance.Brightness(rgb).enhance(BRIGHTNESS)
    rgb.putalpha(win.getchannel("A"))
    return tinted(tinted(rgb, TINT_LEFT), TINT_RIGHT)


def compose(art: Image.Image, s: int, fill: float, background,
            circle: bool = True, feather: bool = True) -> Image.Image:
    """La arto sur kvadrata tolo - kiu povas esti nenio entute - kun mola ombro
    sub ĝi. `circle` estas la labortabla formo; Androido petas la kvadraton."""
    canvas = Image.new("RGBA", (s, s), background)
    inner = s * fill
    k = min(inner / art.width, inner / art.height)
    a = art.resize((max(1, int(art.width * k)), max(1, int(art.height * k))),
                   Image.LANCZOS)
    x, y = (s - a.width) // 2, (s - a.height) // 2

    alpha = a.getchannel("A")
    if circle:
        mask = Image.new("L", a.size, 0)
        ImageDraw.Draw(mask).ellipse((0, 0, a.width - 1, a.height - 1), fill=255)
        alpha = Image.composite(alpha, Image.new("L", a.size, 0), mask)

    # Molaj randoj. Erozii je la malakriga radiuso, poste malakrigi, por ke la
    # tuta fadeno vivu interne de la bildo kaj la 100%-opaka parto ankoraŭ
    # atingu preskaŭ ĝis la vera rando.
    if feather:
        r = max(1, round(s * FEATHER))
        alpha = alpha.filter(ImageFilter.MinFilter(2 * r + 1))
        alpha = alpha.filter(ImageFilter.GaussianBlur(r))
    a.putalpha(alpha)

    # Malluma fotografaĵo tamen bezonas randon por legiĝi kiel objekto, sur
    # mezhela fono aŭ sur malluma taskostrio en kiun ĝi alie dissolviĝus. Prenita
    # el la moligita alfao, do la ombro sekvas la saman konturon.
    if feather:
        shadow = Image.new("RGBA", (s, s), (0, 0, 0, 0))
        shadow.paste((0, 0, 0, 130), (x + max(1, s // 128), y + max(1, s // 96)), a)
        canvas = Image.alpha_composite(
            canvas, shadow.filter(ImageFilter.GaussianBlur(max(1, s / 72))))

    canvas.paste(a, (x, y), a)
    return canvas


def write(path: pathlib.Path, img: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    # SKRIBI NUR KIAM LA PIKSELOJ ŜANĜIĜIS, kaj ne kiam nur la bajtoj ŝanĝiĝis.
    # Pillow 10 kaj Pillow 12 kodas la samajn pikselojn en malsamajn bajtojn -
    # mezurite dufoje, en ambaŭ direktoj: 19 el 19 dosieroj identaj laŭpiksele
    # kaj ĉiuj malsamaj laŭbajte. Senkondiĉa skribo do malpurigis 19 enarbigitajn
    # dosierojn ĉe ĉiu kuro sur la komputilo kiu ne enarbigis ilin, kaj de
    # 2026sep12 tio kostas pli ol ĝenon: build-windows.ps1 kuras ĉi tiun
    # skripton kiel sian unuan paŝon, do cmake/BuildStamp.cmake trovis la arbon
    # malpura kaj stampis "f6c83b78+" - la fenestro About diris "ĉi tiu duumaĵo
    # ne kongruas kun sia enarbigo" pri tute pura elprenaĵo.
    if path.exists():
        try:
            with Image.open(path) as old:
                if (old.size == img.size
                        and old.convert("RGBA").tobytes() == img.convert("RGBA").tobytes()):
                    print(f"same  {path.relative_to(HERE)} ({path.stat().st_size} bytes, {img.width}x{img.height})")
                    return
        except OSError:
            pass   # nelegebla aŭ difekta: superskribi ĝin estas la ĝusta respondo
    img.save(path, format="PNG")
    print(f"wrote {path.relative_to(HERE)} ({path.stat().st_size} bytes, {img.width}x{img.height})")


def android(art: Image.Image) -> None:
    """La adapta ikono, ĝia malnova rezervo, kaj la du XML-dosieroj kiuj nomas
    ilin. Malnovan kvadratan desegnaĵon maskas aŭ enkadrigas ĉiu moderna
    lanĉilo, kaj tio estas dua fonto de strioj kiun nenia kvanto da travidebleco
    forigus."""
    for name, k in DENSITIES.items():
        fg = compose(art, int(108 * k), FILL_ADAPTIVE, CLEAR,
                     circle=False, feather=False)
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

    # La malnova kvadrato, tenata en paŝo anstataŭ lasata putri: ĝi estis
    # manmetita kopio kiun ĉi tiu skripto ne skribis, do regeneri la ikonon
    # antaŭe ŝanĝis ĉiun platformon krom tiu kiu petis.
    write(RES / "drawable" / "icon.png",
          compose(art, PNG_SIZE, FILL_DESKTOP, CLEAR))


def linux(art: Image.Image) -> None:
    """La ikontemo de freedesktop - unu PNG po grando, nomita laŭ la Icon=-ŝlosilo
    de platform/linux/agape48.desktop, kiun menuo, doko kaj Alt-Tab ĉiuj serĉas.

    SKRIBITA EN LA ARBON KAJ ENARBIGITA, kiel la PNG-oj sub Android res/ kaj
    malkiel installer/agape48.ico, kiu estas generita kaj gitignorita. La .ico
    povas esti, ĉar Vindozaj konstruoj pasas tra make-icon.py ĉiuokaze; la
    Linuksaj eble ne, ĉar `cmake --install` instalas ilin kaj freŝa klono devas
    povi fari tion sen Python kaj Pillow. La kosto estas 190 KB da PNG en git kaj
    unu regulo: ŝanĝu la arton, kuru ĉi tiun skripton, enarbigu kion ĝi skribis."""
    for s in ICO_SIZES:
        write(HICOLOR / f"{s}x{s}" / "apps" / "agape48.png",
              compose(art, s, FILL_DESKTOP, CLEAR))


def main() -> None:
    art = artwork()

    write(PNG, compose(art, PNG_SIZE, FILL_DESKTOP, CLEAR))

    ICO.parent.mkdir(parents=True, exist_ok=True)
    # ICO portas plenan alfa-kanalon, do Vindozo ankaŭ ricevas la travideblan.
    frames = [compose(art, s, FILL_DESKTOP, CLEAR) for s in ICO_SIZES]
    frames[-1].save(ICO, format="ICO", sizes=[(s, s) for s in ICO_SIZES])
    print(f"wrote installer/agape48.ico ({ICO.stat().st_size} bytes, {len(ICO_SIZES)} sizes)")

    linux(art)
    android(art)


if __name__ == "__main__":
    main()
