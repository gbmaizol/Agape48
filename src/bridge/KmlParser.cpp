#include "KmlParser.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

// -----------------------------------------------------------------------------
// Emu48 scancode -> x48 keyboard matrix.
//
// Emu48 numbers the HP 48 keyboard with its own scancodes; x48 addresses the
// same keyboard as (out row, in mask). The mapping is a fixed 49-entry table,
// but it is NOT filled in here: the x48 side of it has to be read out of the
// vendored fork's keyboard table, and a guessed table gives you a keypad that
// looks perfect and types the wrong characters. See VENDORING.md step 4.
//
// Populate as { emu48_scancode, x48_row, x48_mask } and the whole KML path
// starts working with no other change.
// -----------------------------------------------------------------------------
struct ScanMap { int scancode; int row; int mask; };

constexpr ScanMap kScanMap[] = {
    // { 0x00, 0, 0x01 },  ...  TODO(vendor)
};

QStringList tokenize(const QString &line)
{
    // KML tokens are whitespace-separated, with "quoted strings" kept whole.
    static const QRegularExpression re(
        QStringLiteral(R"("([^"]*)"|(\S+))"));
    QStringList out;
    auto it = re.globalMatch(line);
    while (it.hasNext()) {
        const auto m = it.next();
        out << (m.capturedLength(1) >= 0 && !m.captured(1).isNull()
                    ? m.captured(1) : m.captured(2));
    }
    return out;
}

int toInt(const QString &s, bool *ok = nullptr)
{
    // KML writes numbers as decimal or 0x-prefixed hex.
    return s.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)
        ? s.mid(2).toInt(ok, 16)
        : s.toInt(ok, 10);
}

} // namespace

bool KmlParser::scancodeToMatrix(int scancode, int *row, int *mask)
{
    for (const ScanMap &m : kScanMap) {
        if (m.scancode == scancode) {
            *row = m.row;
            *mask = m.mask;
            return true;
        }
    }
    return false;
}

bool KmlParser::fail(const QString &why)
{
    m_errorLine = (m_pos > 0 && m_pos <= m_lines.size())
                      ? m_lines.at(m_pos - 1).number : 0;
    m_error = QStringLiteral("KML line %1: %2").arg(m_errorLine).arg(why);
    return false;
}

bool KmlParser::parse(const QString &text, const QUrl &base)
{
    m_base = base;
    m_skin = KmlSkin {};
    m_lines.clear();
    m_pos = 0;
    m_error.clear();

    // Pass 1: strip comments and blanks, keep original line numbers for errors.
    int n = 0;
    for (const QString &raw : text.split(QLatin1Char('\n'))) {
        ++n;
        QString l = raw;
        const int hash = l.indexOf(QLatin1Char('#'));
        if (hash >= 0)
            l.truncate(hash);
        l = l.trimmed();
        if (!l.isEmpty())
            m_lines.append({ l, n });
    }

    // Pass 2: top-level blocks.
    while (m_pos < m_lines.size()) {
        const QStringList tok = tokenize(m_lines.at(m_pos).text);
        ++m_pos;
        if (tok.isEmpty())
            continue;
        const QString kw = tok.first();

        if (kw.compare(QLatin1String("Include"), Qt::CaseInsensitive) == 0) {
            // TODO: resolve against m_base and recurse, guarding against
            // include cycles with a depth limit.
            continue;
        }
        if (!parseBlock(kw, tok.mid(1).join(QLatin1Char(' '))))
            return false;
    }

    if (m_skin.bitmap.isEmpty())
        return fail(QStringLiteral("Global block has no Bitmap"));
    if (m_skin.buttons.isEmpty())
        return fail(QStringLiteral("skin defines no buttons"));
    return true;
}

bool KmlParser::parseBlock(const QString &keyword, const QString &argument)
{
    const bool isGlobal      = keyword.compare(QLatin1String("Global"), Qt::CaseInsensitive) == 0;
    const bool isBackground  = keyword.compare(QLatin1String("Background"), Qt::CaseInsensitive) == 0;
    const bool isLcd         = keyword.compare(QLatin1String("Lcd"), Qt::CaseInsensitive) == 0;
    const bool isButton      = keyword.compare(QLatin1String("Button"), Qt::CaseInsensitive) == 0;
    const bool isAnnunciator = keyword.compare(QLatin1String("Annunciator"), Qt::CaseInsensitive) == 0;

    if (!isGlobal && !isBackground && !isLcd && !isButton && !isAnnunciator)
        return fail(QStringLiteral("unknown block \"%1\"").arg(keyword));

    KmlButton      button;
    KmlAnnunciator ann;

    if (isButton) {
        button.name = argument.trimmed();
        bool ok = false;
        const int code = toInt(button.name, &ok);
        if (ok)
            button.scancode = code;
    } else if (isAnnunciator) {
        ann.bit = toInt(argument.trimmed());
    }

    // Read to the matching End.
    while (m_pos < m_lines.size()) {
        const QStringList tok = tokenize(m_lines.at(m_pos).text);
        ++m_pos;
        if (tok.isEmpty())
            continue;
        const QString k = tok.first();
        const auto arg = [&tok](int i) { return i < tok.size() ? tok.at(i) : QString(); };
        const auto num = [&](int i) { return toInt(arg(i)); };

        if (k.compare(QLatin1String("End"), Qt::CaseInsensitive) == 0)
            break;

        if (isGlobal) {
            if      (k.compare(QLatin1String("Title"),    Qt::CaseInsensitive) == 0) m_skin.title  = arg(1);
            else if (k.compare(QLatin1String("Author"),   Qt::CaseInsensitive) == 0) m_skin.author = arg(1);
            else if (k.compare(QLatin1String("Model"),    Qt::CaseInsensitive) == 0) m_skin.model  = arg(1);
            else if (k.compare(QLatin1String("Hardware"), Qt::CaseInsensitive) == 0) m_skin.hardware = arg(1);
            else if (k.compare(QLatin1String("Bitmap"),   Qt::CaseInsensitive) == 0)
                m_skin.bitmap = m_base.resolved(QUrl(arg(1)));
            // Print / Debug / Class are informational; ignored on purpose.
        } else if (isBackground) {
            if (k.compare(QLatin1String("Size"), Qt::CaseInsensitive) == 0)
                m_skin.backgroundSize = QSize(num(1), num(2));
        } else if (isLcd) {
            if      (k.compare(QLatin1String("Offset"), Qt::CaseInsensitive) == 0)
                m_skin.lcdOffset = QPoint(num(1), num(2));
            else if (k.compare(QLatin1String("Zoom"), Qt::CaseInsensitive) == 0)
                m_skin.lcdZoom = qBound(1, num(1), 8);
        } else if (isButton) {
            if      (k.compare(QLatin1String("Offset"), Qt::CaseInsensitive) == 0) button.offset = QPoint(num(1), num(2));
            else if (k.compare(QLatin1String("Size"),   Qt::CaseInsensitive) == 0) button.size   = QSize(num(1), num(2));
            else if (k.compare(QLatin1String("Down"),   Qt::CaseInsensitive) == 0) button.down   = QPoint(num(1), num(2));
            else if (k.compare(QLatin1String("NoHold"), Qt::CaseInsensitive) == 0) button.noHold = true;
            else if (k.compare(QLatin1String("Type"),   Qt::CaseInsensitive) == 0) {
                if (num(1) == 2)
                    return fail(QStringLiteral("Type 2 (macro) buttons are not supported"));
            } else if (k.compare(QLatin1String("Outin"), Qt::CaseInsensitive) == 0) {
                // Emu48's own matrix encoding; prefer it over the scancode when
                // present, since it is already (out, in).
                button.row  = num(1);
                button.mask = num(2);
            }
        } else if (isAnnunciator) {
            if      (k.compare(QLatin1String("Offset"), Qt::CaseInsensitive) == 0) ann.offset = QPoint(num(1), num(2));
            else if (k.compare(QLatin1String("Down"),   Qt::CaseInsensitive) == 0) ann.down   = QPoint(num(1), num(2));
            else if (k.compare(QLatin1String("Size"),   Qt::CaseInsensitive) == 0) ann.size   = QSize(num(1), num(2));
        }
    }

    if (isButton) {
        if (button.size.isEmpty())
            return fail(QStringLiteral("button \"%1\" has no Size").arg(button.name));
        if (button.row < 0 && button.scancode >= 0)
            scancodeToMatrix(button.scancode, &button.row, &button.mask);
        m_skin.buttons.append(button);
    } else if (isAnnunciator) {
        m_skin.annunciators.append(ann);
    }
    return true;
}
