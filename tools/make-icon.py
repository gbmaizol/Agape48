#!/usr/bin/env python3
"""Konstrui la aplikaĵan ikonon el fotografaĵo de vera 48GX.

Unu peco da arto, ĉiu platformo, por ke ili ne povu disiĝi:

    assets/icon.png            enmetita en la duumaĵon; QGuiApplication::
                               setWindowIcon uzas ĝin, kaj tio estas kion la
                               fenestroadministranto kaj la taskostrio desegnas
                               sur ĈIU platformo
    installer/agape48.ico      la Vindoza instalilo, la Start-menua ligilo, kaj
                               Aldoni/Forigi
    platform/linux/hicolor/    la ikontemo de freedesktop je sep grandoj: la
                               menuero, la doko kaj Alt-Tab
    platform/android/res/...   la adapta ikono: antaŭa tavolo je kvin densoj,
                               fona koloro, kaj la malnova kvadrato por ĉio pli
                               aĝa ol API 26

LA FONTO ESTAS FOTOGRAFAĴO, ne la desegnita vizaĝo, de 2026sep08. Gert: "my
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
ŝanĝi. (La tondo kaj la kontrasto de 2026sep10 estas novaj kaj ankoraŭ ne estis
kontrolitaj sur la Linuksa komputilo; ImageEnhance kaj crop estas ambaŭ
determinismaj, sed tio estas argumento kaj ne mezuro.)

LA FOTOGRAFAĴO ESTAS TURNITA 30 GRADOJN MALDEKSTRUME. Gert, 2026sep08: "one
quirk to make it different from Droid48: Rotate it 30 counter-clockwise. But
this needs to be the new icon for all OSs." Tio ankaŭ solvas formoproblemon
kiun ĝi ne estis petita solvi: turni altan bildon plilarĝigas ĝian ĉirkaŭskatolon
kaj tial ĝi pli bone plenigas kvadraton, kaj la cirklon de lanĉilo, ol ĝi povus
starante rekte.

TRAVIDEBLA KIE AJN LA FORMATO PERMESAS, kaj la ardezo de Gert kie ne. Liaj
vortoj, en tiu ordo: "make the background r66,g75,b92" kaj poste "or transparent
when possible". Estas ekzakte unu loko kie tio ne eblas: la fona tavolo de
Androida ADAPTA ikono, kiu devas esti opaka kaj kiun iuj lanĉiloj pentras nigra
se ĝi ne estas - nigraj strioj denove, kio estas la plendo per kiu ĉi tio
komenciĝis. Do:

    la labortabla PNG, la .ico kaj la malnova kvadrato     travideblaj, kaj la
                klinita fotografaĵo flosas super kio ajn estas malantaŭ ĝi
    la ANTAŬA tavolo de la adapta ikono     travidebla ĉe la anguloj, sed skalita
                por kovri la maskon de la lanĉilo tute - vidu FILL_ADAPTIVE
    la FONA tavolo de la adapta ikono     la ardezo, kiu laŭ desegno nun estas
                vidata nur dum lanĉilo animacias la ikonon

La mola ombro restas en ĉiu kazo. Ĝi estas kio malhelpas ke malluma fotografaĵo
dissolviĝu en malluman taskostrion, kaj sur travidebleco ĝi simple vojaĝas kun
la bildo.
"""

import pathlib
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

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

# LA TONDO, 2026sep10. Gert, vidinte la Androidan konstruon: "The icon that was
# implemented in the latest Android build looks good to me in any size. Maybe it
# should be made with just a little bit (20%) more contrast and brightness. Even
# better if cropped better so that two thirds of the ON button appeared at the
# bottom of the circle."
#
# La ON-klavo estis TUTE NEVIDEBLA en la cirklo antaŭe: mezurite, 0,0% de ĝi
# staris interne de la 72dp-masko. La fotografaĵo estas 325x519, la klavo sidas
# ĉe ĝia malsupra maldekstra angulo, kaj cirklo montras nur la mezan trionon -
# do la klavo neniam havis ŝancon.
#
# Kial tondo kaj ne ŝovo. Ŝovi la bildon supren enmetas la ON-klavon, sed ĝi
# ankaŭ eltiras angulon el la masko: la turnita rektangulo havas nur 40
# bilderojn da marĝeno sur sia mallonga akso, kaj suprena ŝovo de d konsumas
# 0,5*d el ĝi. Tondo anstataŭe faras la bildon PLI KVADRATA, kaj kvadrata bildo
# kovras cirklon per malpli da skalo: tial FILL_ADAPTIVE malsupriĝis de 1,6 al
# 1,3 en la sama ŝanĝo. La fotografaĵo mem ne estas tuŝita; ĉi tio estas nombro
# kiun oni povas malfari.
#
# Elektita per serĉo super la kvar randoj kaj la plenigo, mezurante du aferojn
# je 432 bilderoj: kiom da la ON-klavo staras interne de la 72dp-cirklo, kaj kiom
# de tiu cirklo restas nuda. El 144 kandidatoj ok kovras la maskon kaj montras la
# klavon; ĉi tiu estas la plej proksima al "du trionoj, ĉe la fundo":
#
#   ON interne 69,5%     masko nuda 0,00% je 72dp, 1,49% je 80dp
#   la klavo staras 7 bilderojn maldekstre de la centro kaj 63 sub ĝi, do
#   ĉe la fundo kaj preskaŭ centrita
#
# Kion la cirklo nun montras: I/O, la 1, la verda dekstra ŝovsago, CONT OFF, kaj
# la ON-klavo tranĉita de la malsupra rando.
CROP = (0, 240, 230, 519)

# +20%, liaj nombroj. Aplikataj POST la tondo, ĉar ImageEnhance.Contrast
# kalkulas la mezvaloron de la bildo kiun oni donas al ĝi: enhavigi la
# forĵetatan parton en tiun mezvaloron signifus agordi la ikonon laŭ bilderoj
# kiujn neniu vidos.
#
# Ĝi ankaŭ atakas problemon kiun ĝi ne estis petita ataki: je 16 bilderoj sur
# malluma taskostrio la ikono estas preskaŭ makuleto, kaj ĝiaj opakaj bilderoj
# havis mezan lumecon de nur 51 el 255. Vidu la mezuron post la tondo en la
# transdono.
CONTRAST = 1.2
BRIGHTNESS = 1.2

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

# Kiom da la tolo la fotografaĵo okupas. PLI OL 1,0 INTENCE: ĝi estas skalita
# ĝis sia ĉirkaŭskatolo estas kvaronon pli larĝa ol la ikono, do la kvar anguloj
# de la klinita bildo falas eksteren kaj estas fortranĉitaj. Gert, 2026sep08:
# "The image is too small. make it larger, it doesn't matter if some corners will
# be cut."
#
# 1,85 DE 2026SEP10, kaj tio estas mezuro anstataŭ gusto. Gert, provinte la
# Vindozan konstruon: "The icon is too small. Make it as big as possible, even if
# it crops a little bit off the top and bottom edges", kaj tuj poste "It's also
# fine to show only 50% of the ON button."
#
# Je 1,25 la arto pentris nur 56% de sia kvadrato. La aliaj 44% estis la kvar
# travideblaj trianguloj kiujn la klino lasas ĉe la anguloj, kaj je 16 bilderoj
# en taskostrio tio aspektas kiel makuleto kun malplena spaco ĉirkaŭ ĝi - kio
# estas ekzakte lia plendo. Je 1,85 ĝi pentras 94%, la ON-klavo ankoraŭ montras
# 82% de si, kaj ĉe la anguloj restas 5,6% da nenio, do la klino ankoraŭ havas
# ion kontraŭ kio esti videbla kaj la mola ombro ankoraŭ havas lokon kie fali.
#
# La limo kiun li mem nomis estas la ON-klavo je duono, kaj tio estus 2,20. Sed
# je 2,20 la tolo estas 99,6% opaka: la ikono estas simpla kvadrata fotografaĵo,
# kaj la ombro estas fortranĉita kune kun la anguloj. Kvadrato ĝi vere fariĝas
# je 2,30. La malnova komento ĉi tie asertis ke tio okazas "preter proksimume
# 1,4"; tio estis argumento kaj ne mezuro, kaj mezurite ĝi estas malvera - je
# 1,40 eĉ la netondita arto lasis 39% de la tolo nuda.
#
# DU NOMBROJ, ĉar la du platformoj volas kontraŭajn aferojn, kaj 2026sep08 estas
# kiam tio klariĝis. Gert, rigardante la lanĉilon: "Why the launcher shows me a
# circle in the background of the icon instead of transparency?"
#
# Ĝi montras cirklon ĉar sur Androido ne plu ekzistas io tia kiel libera formo de
# ikono. Adapta ikono estas du tavoloj kaj la LANĈILO elektas la silueton -
# cirklo ĉi tie, kvadratcirklo sur aliaj telefonoj, rondigita kvadrato aliloke -
# kaj tranĉas ambaŭ tavolojn per ĝi, do ĉiu ikono sur la hejmekrano havas la
# saman formon intence. Travidebleco ne povas venki tion; la fona tavolo devas
# esti opaka, kaj lanĉilo kiu ricevas travideblan pentras ĝin nigra, kio estas
# la nigraj strioj per kiuj ĉi tiu tuta ikono komenciĝis.
#
# Kion oni POVAS fari estas lasi nenion nian por la formo de la lanĉilo plenigi:
# skali la fotografaĵon ĝis ĝi kovras la tutan maskon, kaj la cirklo ĉesas esti
# kolora plato kun bildo sur ĝi kaj fariĝas cirklo el bildo. Ĝis 2026sep10 tio
# postulis 1,6, ĉar la netondita fotografaĵo estas alta kaj mallarĝa. Kun CROP
# la sama kovro venas je 1,30, mezurite: 0,00% de la 72dp-masko nuda, kaj 1,49%
# de la 80dp kiun lanĉilo atingas dum paralaksa animacio.
#
# Kaj tial la du nombroj nun kuŝas inverse al kio oni atendus: la ADAPTA estas
# la malgranda. La cirklo de la lanĉilo jam fortranĉas ĉion krom la mezo, do
# 1,30 sufiĉas por kovri ĝin tute kaj pli nur forĵetus arton kiun la cirklo
# estus montrinta. La labortabla ikono havas nenion kiu tranĉas ĝin, do kion ĝi
# ne plenigas restas simple malplena.
FILL_DESKTOP = 1.85
FILL_ADAPTIVE = 1.30

# "Oh, and make the edges a bit blurry!" - Gert, 2026sep08. Frakcio de la propra
# grando de la ikono anstataŭ fiksa nombro da bilderoj, por ke la moleco aspektu
# same je 16 kaj je 432 anstataŭ igi malgrandan ikonon kaĉo. La fadenado estas
# puŝita INTERNEN unue: malakrigi nur la maskon disvastigus ĝin eksteren super la
# nigro kiun la turno lasas ekster la fotografaĵo, kaj pendigus malpuran
# aŭreolon sur ĉiun randon.
FEATHER = 0.018

# Antaŭaj tavoloj je 108dp, kaj la malnova 48dp-lanĉikono, je la kvin densoj
# kiujn Androido petas.
DENSITIES = {"mdpi": 1, "hdpi": 1.5, "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}


def prepared() -> Image.Image:
    """La fonto tondita kaj agordita, antaŭ la turno. Ordo gravas: tondi, poste
    kontrasti, ĉar ImageEnhance.Contrast mezuras la bildon kiun ĝi ricevas."""
    src = Image.open(SOURCE).convert("RGB").crop(CROP)
    src = ImageEnhance.Contrast(src).enhance(CONTRAST)
    src = ImageEnhance.Brightness(src).enhance(BRIGHTNESS)
    return src.convert("RGBA")


def turned(src: Image.Image) -> Image.Image:
    """Turnita ĉirkaŭ sia centro, kun propra alfa-masko. La fotografaĵo ne havas
    alfa-kanalon, do turni ĝin rekte plenigus la novajn angulojn per nigro - kio
    remetus la striojn, oblikve."""
    solid = Image.new("RGBA", src.size, (255, 255, 255, 255))
    rot = src.rotate(ANGLE, expand=True, resample=Image.BICUBIC)
    rot.putalpha(solid.rotate(ANGLE, expand=True, resample=Image.BICUBIC).getchannel("A"))
    return rot


def compose(art: Image.Image, s: int, fill: float, background) -> Image.Image:
    """La turnita fotografaĵo sur kvadrata fono - kiu povas esti nenio entute -
    kun mola ombro sub ĝi."""
    canvas = Image.new("RGBA", (s, s), background)
    inner = s * fill
    k = min(inner / art.width, inner / art.height)
    a = art.resize((max(1, int(art.width * k)), max(1, int(art.height * k))),
                   Image.LANCZOS)
    x, y = (s - a.width) // 2, (s - a.height) // 2

    # Molaj randoj. Erozii je la malakriga radiuso, poste malakrigi, por ke la
    # tuta fadeno vivu interne de la bildo kaj la 100%-opaka parto ankoraŭ
    # atingu preskaŭ ĝis la vera rando.
    r = max(1, round(s * FEATHER))
    alpha = a.getchannel("A")
    alpha = alpha.filter(ImageFilter.MinFilter(2 * r + 1))
    alpha = alpha.filter(ImageFilter.GaussianBlur(r))
    a.putalpha(alpha)

    # Malluma fotografaĵo tamen bezonas randon por legiĝi kiel objekto, sur
    # mezhela fono aŭ sur malluma taskostrio en kiun ĝi alie dissolviĝus. Prenita
    # el la moligita alfao, do la ombro sekvas la saman konturon.
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
    """La adapta ikono, ĝia malnova rezervo, kaj la du XML-dosieroj kiuj nomas
    ilin. Malnovan kvadratan desegnaĵon maskas aŭ enkadrigas ĉiu moderna
    lanĉilo, kaj tio estas dua fonto de strioj kiun nenia kvanto da travidebleco
    forigus."""
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
    art = turned(prepared())

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
