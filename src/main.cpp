#include <QGuiApplication>
#include <QIcon>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Windows opens files in TEXT mode by default, and the vendored core opens
    // every ROM and state file with a bare "r" or "w" - romio.c:60, binio.c:214
    // and five sites in init.c. In text mode the CRT collapses \r\n to \n, turns
    // \n back into \r\n on the way out, and stops reading at the first 0x1A.
    //
    // That is not cosmetic. Measured on the real 48GX ROM: fopen(rom, "r")
    // returns 242 bytes of 524288, because byte 242 happens to be 0x1A. The ROM
    // is then rejected by read_rom's size check and there is no calculator at
    // all. The same translation would corrupt ram, hp48, port1, port2 and any
    // imported HPHP48- object, in both directions.
    //
    // It matters far beyond this machine. The state folder is designed to be
    // moved onto a synced folder and opened from Linux, Windows or Android, and
    // the file format is portable on purpose - the config is serialised field by
    // field through write_8/16/32, so there is no endianness or LP64-vs-LLP64
    // problem to solve. Text mode was the one thing that would have quietly
    // rewritten a shared state folder into something the other two platforms
    // could not read.
    //
    // Setting the default here rather than editing seven fopen calls keeps the
    // vendored tree untouched, so a re-vendor cannot reintroduce it, and it also
    // covers any fopen added later. Qt's own file I/O does not go through the
    // CRT, so QIODevice::Text still behaves as written.
#if defined(_MSC_VER)
    _set_fmode(_O_BINARY);
#else
    // MinGW links msvcrt.dll, which does not export _set_fmode - the link fails
    // with "undefined reference to __imp__set_fmode". Assigning the global that
    // the CRT consults on every fopen is the MinGW way and has the same effect.
    _fmode = _O_BINARY;
#endif
#endif

    // Agape: "HP" spoken in Brazilian Portuguese is "aga-pe".
    // Pinned, per the Quick Controls decision of 2026aug28: Basic is the
    // style that does not drag in a platform theme, and the sheet has to
    // look the same next to the calculator face on every platform.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setApplicationName(QStringLiteral("Agape48"));
    QGuiApplication::setApplicationVersion(QStringLiteral(AGAPE48_VERSION));
    QGuiApplication::setOrganizationName(QStringLiteral("Agape48"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("agape48.local"));

    // No multisampling: the LCD is nearest-filtered on purpose and the skin is
    // a photograph. Asking for MSAA costs memory bandwidth and buys nothing.
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setSamples(0);
    QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);

    // The window and taskbar icon, on every platform from one place. Without it
    // Qt supplies its own default - a cogwheel on Linux, which is what Gert saw.
    // Windows only looked right by accident: the icon he liked belonged to the
    // Start-menu shortcut the installer creates, so a window opened any other
    // way had nothing either, and the executable itself still has no icon.
    //
    // After the QGuiApplication, not before it: the name and domain setters
    // above are plain statics, but a QIcon reaches into platform integration,
    // and the qml module's resources are registered by this point.
    //
    // Generated from the face by tools/make-icon.py, so the icon cannot drift
    // away from the calculator it depicts.
    QGuiApplication::setWindowIcon(
        QIcon(QStringLiteral(":/qt/qml/Agape48/assets/icon.png")));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Agape48", "Main");

    return app.exec();
}
