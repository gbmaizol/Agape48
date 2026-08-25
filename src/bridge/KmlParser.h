// ---------------------------------------------------------------------------
// KmlParser - Emu48 Keypad Mapping Language.
//
// KML is a small, block-structured, line-oriented script. Emu48 skins have been
// written in it since the late 1990s, and supporting it means Agape48 inherits
// that whole library instead of starting from one skin.
//
// Grammar, in the shape that matters here:
//
//   Global            Background        Lcd             Button <id>
//     Title "..."       Offset x y        Offset x y       Type n
//     Author "..."      Size w h          Zoom n           Offset x y
//     Bitmap "f.bmp"  End                 Color i r g b    Size w h
//     Model "..."                       End                Down x y
//     Class n                                              Outin out in
//     Hardware "..."  Annunciator <n>                      Onname "..."
//     Print "..."       Offset x y                         Menu | NoHold
//     Debug n           Down   x y                       End
//   End                 Size   w h
//                     End                                Scancode <n>
//
//   Preprocessor: Include "f.kml", Ifdef/Ifndef <sym> ... Else ... End
//
// Deliberately NOT supported, and reported as an error rather than ignored:
//   - Type 2 (Emu48 "menu" buttons that fire macros)
//   - Emu48-specific hardware blocks for the 38/39/40/49 series
// Silently ignoring those produces a skin that half-works, which is worse.
// ---------------------------------------------------------------------------
#pragma once

#include <QList>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QUrl>

struct KmlButton {
    QString name;
    int     scancode = -1;   // as written in the KML
    int     row  = -1;       // resolved x48 matrix out row
    int     mask = 0;        // resolved x48 matrix in mask
    QPoint  offset;
    QSize   size;
    QPoint  down;            // source offset of the pressed artwork
    bool    noHold = false;
};

struct KmlAnnunciator {
    int    bit = 0;
    QPoint offset;
    QPoint down;
    QSize  size;
};

struct KmlSkin {
    QString title, author, model, hardware;
    QUrl    bitmap;                 // resolved against the .kml's directory
    QSize   backgroundSize;
    QPoint  lcdOffset;
    int     lcdZoom = 2;
    QList<KmlButton>      buttons;
    QList<KmlAnnunciator> annunciators;
};

class KmlParser
{
public:
    bool parse(const QString &text, const QUrl &base);

    const KmlSkin &result() const { return m_skin; }
    QString errorString() const { return m_error; }
    int errorLine() const { return m_errorLine; }

    // Emu48 scancode -> x48 (row, mask). Exposed so the JSON dialect can reuse
    // it when a converted skin carries scancodes instead of names.
    static bool scancodeToMatrix(int scancode, int *row, int *mask);

private:
    struct Line { QString text; int number; };

    bool parseBlock(const QString &keyword, const QString &argument);
    bool fail(const QString &why);

    QList<Line> m_lines;
    int         m_pos = 0;
    QUrl        m_base;
    KmlSkin     m_skin;
    QString     m_error;
    int         m_errorLine = 0;
};
