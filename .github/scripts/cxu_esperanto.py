#!/usr/bin/env python3
"""Ĉu ĉi tiu teksto estas en Esperanto? - la kontrolilo malantaŭ la regulo.

Uzo:
    python cxu_esperanto.py teksto.md        -> eliras 0 se Esperanto, 1 se ne
    python cxu_esperanto.py --diff peco.diff -> nur la aldonitajn komentliniojn

Ĝi ne estas lingvo-modelo kaj ne pretendas esti. Ĝi nombras vortojn: Esperanto
estas escepte facile rekonebla maŝine, ĉar ĝiaj funkcivortoj estas malmultaj kaj
ĝiaj finaĵoj estas regulaj. La homo kiu recenzas restas la fina juĝanto - la
tasko de ĉi tio estas kapti la evidentan kaj respondi tuj.

DU AFEROJ KIUJN ĜI INTENCE IGNORAS, ĉar alie ĝi punus honestan raporton:
kodblokoj inter ``` kaj citaĵoj komencantaj per >. Erarmesaĝo, protokolo aŭ
kodfragmento estas pruvo, kaj pruvo ne estas tradukenda.
"""
import argparse
import re
import sys

# Funkcivortoj kiuj preskaŭ ne aperas en la angla. "de", "en", "al" kaj "mi"
# aperas en aliaj lingvoj, sed ne en la angla, kio sufiĉas ĉi tie.
EO = {
    "la", "kaj", "estas", "estis", "estos", "ne", "ĉi", "tiu", "tiuj", "kiu",
    "kiuj", "por", "pri", "sed", "aŭ", "en", "de", "al", "mi", "vi", "ĝi",
    "ili", "ni", "kiam", "ĉar", "tio", "tiel", "jam", "nur", "ankaŭ", "sen",
    "sur", "sub", "super", "post", "antaŭ", "dum", "per", "ol", "se", "ke",
    "ĉu", "jes", "ĉiu", "ĉio", "iom", "tre", "pli", "plej", "malpli", "ambaŭ",
    "kion", "kio", "kie", "kiel", "kial", "kiom", "ĉiam", "neniam", "io",
    "iu", "nenio", "neniu", "havas", "faras", "povas", "devas", "volas",
    "montras", "aperas", "okazas", "mem", "propra", "sama", "alia", "unu",
    "du", "tri", "nun", "poste", "supre", "sube", "eble", "verŝajne", "do",
}

EN = {
    "the", "is", "are", "was", "were", "and", "that", "this", "these", "those",
    "which", "with", "from", "not", "but", "for", "what", "when", "where",
    "why", "how", "does", "did", "has", "have", "had", "would", "could",
    "should", "it", "its", "they", "them", "there", "then", "than", "because",
    "only", "already", "still", "same", "other", "every", "nothing", "anything",
    "you", "your", "we", "our", "my", "can", "cannot", "will", "just", "also",
    "about", "after", "before", "while", "into", "over", "under", "some",
}

DIAKRITOJ = set("ĉĝĥĵŝŭĈĜĤĴŜŬ")

# Finaĵoj kiuj estas la skeleto de la lingvo: substantivoj, adjektivoj, verboj.
FINAJXOJ = re.compile(r"\w{3,}(ojn|oj|ajn|aj|on|an|as|is|os|us|"
                      r"itaj|ita|antaj|anta|ado|eco|ejo|ilo|isto)$")

VORTO = re.compile(r"[^\W\d_]+", re.UNICODE)


def purigi(t):
    """Forigas tion, kio ne estas la prozo de la aŭtoro."""
    t = re.sub(r"```.*?```", " ", t, flags=re.S)      # kodblokoj
    t = re.sub(r"`[^`]*`", " ", t)                    # enlinia kodo
    t = re.sub(r"^\s*>.*$", " ", t, flags=re.M)       # citaĵoj
    t = re.sub(r"https?://\S+", " ", t)               # adresoj
    t = re.sub(r"<!--.*?-->", " ", t, flags=re.S)     # HTML-komentoj
    t = re.sub(r"^\s*-\s*\[[ xX]\]", " ", t, flags=re.M)  # kontrolskatoloj
    return t


def komentoj_el_diff(t):
    """La aldonitaj komentlinioj de unifikita diff, kaj nenio alia."""
    ligiloj = []
    for linio in t.splitlines():
        if not linio.startswith("+") or linio.startswith("+++"):
            continue
        korpo = linio[1:].strip()
        m = re.match(r"^(//+|#+|;+|\*+|/\*+)\s*(.*)$", korpo)
        if not m:
            continue
        teksto = m.group(2)
        # Dividiloj kaj vojnomoj portas nenian lingvon.
        if len(VORTO.findall(teksto)) < 2:
            continue
        ligiloj.append(teksto)
    return "\n".join(ligiloj)


def taksi(t):
    vortoj = [v.lower() for v in VORTO.findall(t)]
    n = len(vortoj)
    if n == 0:
        return dict(vortoj=0, eo=0, en=0, dens_eo=0.0, dens_en=0.0,
                    diakritoj=0, finajxoj=0, esperanta=None)
    eo = sum(1 for v in vortoj if v in EO)
    en = sum(1 for v in vortoj if v in EN)
    fin = sum(1 for v in vortoj if FINAJXOJ.match(v))
    dia = sum(1 for c in t if c in DIAKRITOJ)
    dens_eo = (eo + fin) / n
    dens_en = en / n
    # Sufiĉe da Esperanto, kaj pli da Esperanto ol da angla. Diakritoj sole
    # jam pruvas ke la teksto ne estas angla, sed ne ke ĝi estas Esperanto.
    esperanta = dens_eo >= 0.12 and (eo + fin) > en
    return dict(vortoj=n, eo=eo, en=en, dens_eo=round(dens_eo, 3),
                dens_en=round(dens_en, 3), diakritoj=dia, finajxoj=fin,
                esperanta=esperanta)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("dosiero")
    p.add_argument("--diff", action="store_true",
                   help="trakti la enigon kiel unifikitan diff kaj taksi nur "
                        "la aldonitajn komentliniojn")
    a = p.parse_args()

    kruda = open(a.dosiero, encoding="utf-8", errors="replace").read()
    teksto = komentoj_el_diff(kruda) if a.diff else purigi(kruda)
    r = taksi(teksto)

    for ŝlosilo, valoro in r.items():
        print("%-11s %s" % (ŝlosilo, valoro))

    # Tro malmulte por juĝi: ne malakcepti. Silento ne estas krimo.
    if r["vortoj"] < 6:
        print("verdikto    tro-mallonga")
        return 0
    print("verdikto    %s" % ("esperanto" if r["esperanta"] else "ne-esperanto"))
    return 0 if r["esperanta"] else 1


if __name__ == "__main__":
    sys.exit(main())
