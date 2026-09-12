#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QSurfaceFormat>
#include <QUrl>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#endif

// Q_DECL_EXPORT on Android, and this is the whole of why the first four APKs
// came up as a black screen with nothing in the log.
//
// qt_add_executable builds a SHARED LIBRARY on Android, not an executable, and
// QtActivity starts it with dlsym(handle, "main"). We build everything with
// -fvisibility=hidden - cmake/Agape48Size.cmake:58, for a smaller dynsym and
// better LTO and ICF - so the one symbol Android has to find was the one we had
// told the linker to hide. Measured in the shipped library: 487 entries in
// .dynsym and "main" not among them, and on the phone:
//
//   D/nativeloader: Load .../libagape48_arm64-v8a.so : ok
//   E/default     : dlsym failed: undefined symbol: main
//   E/default     : Could not find main method
//
// Everything loaded and then nothing ran - no QGuiApplication, no QML, and
// therefore none of the diagnostics we had added for a black screen, which is
// why it was silent as well as black.
//
// Q_DECL_EXPORT is __attribute__((visibility("default"))) here, so this one
// symbol is exempted and every other symbol keeps the size win. Not extern "C":
// main already has an unmangled name and the standard forbids giving it
// linkage of its own.
#ifdef Q_OS_ANDROID
Q_DECL_EXPORT
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

    // A dogfood build must not share the installed app's profile. On Linux that
    // is a throwaway HOME and costs nothing; on Windows QSettings lives in
    // HKCU\Software\<org>\<app> and no environment variable moves it, so the only
    // lever is the application NAME - which also moves AppLocalDataLocation, and
    // with it the default state folder and the log. One switch isolates all
    // three.
    //
    // 2026sep03, after both-03 had been run against an installed build:
    // "For the next dogfood report, let's work with a dogfood build rather than
    // making an installation right away." Before this, testing a build on this
    // machine meant writing the live settings - which is exactly what got
    // blocked, correctly, earlier the same day.
    //
    // Parsed by hand, and before the QGuiApplication: setApplicationName has to
    // run before anything touches QSettings or QStandardPaths, and
    // QCommandLineParser needs the application object that does not exist yet.
    QString profile = QStringLiteral("Agape48");
    QString stateSeed;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        const bool hasNext = (i + 1 < argc);
        if (arg == QLatin1String("--profile") && hasNext)
            profile = QString::fromLocal8Bit(argv[++i]);
        else if (arg.startsWith(QLatin1String("--profile=")))
            profile = arg.mid(10);
        else if (arg == QLatin1String("--state") && hasNext)
            stateSeed = QString::fromLocal8Bit(argv[++i]);
        else if (arg.startsWith(QLatin1String("--state=")))
            stateSeed = arg.mid(8);
    }
    // The profile name becomes a registry key AND a directory name, so it has to
    // be one harmless path segment. A path here would silently scatter settings
    // and a calculator's memory somewhere nobody chose, so refuse rather than
    // sanitise it into something that was not asked for.
    if (profile.isEmpty() || profile.contains(QLatin1Char('/'))
        || profile.contains(QLatin1Char('\\')) || profile.contains(QLatin1Char(':'))
        || profile == QLatin1String(".") || profile == QLatin1String("..")) {
        qWarning("agape48: --profile must be a single name, not a path");
        return 2;
    }

    // Agape: "HP" spoken in Brazilian Portuguese is "aga-pe".
    // Pinned, per the Quick Controls decision of 2026aug28: Basic is the
    // style that does not drag in a platform theme, and the sheet has to
    // look the same next to the calculator face on every platform.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setApplicationName(profile);
    QGuiApplication::setApplicationVersion(QStringLiteral(AGAPE48_VERSION));
    QGuiApplication::setOrganizationName(QStringLiteral("Agape48"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("agape48.local"));

    // platform/linux/agape48.desktop, minus the extension. It is how a window
    // says which menu entry it belongs to, so the shell can draw the installed
    // icon on the pinned launcher instead of a generic one. Nothing on Windows
    // or Android reads it, where it costs one stored string.
    //
    // On WAYLAND it is the whole story: it becomes the xdg-shell app_id, and
    // that is the only handle a compositor has - there is no WM_CLASS there to
    // fall back on.
    //
    // On X11 it does NOT touch WM_CLASS - measured with xprop before and after
    // on 2026sep09, still "agape48", "Agape48probe": argv[0], then
    // QGuiApplication::applicationName(), which --profile moves. That is why
    // the entry says StartupWMClass=agape48 and matches the first field, the
    // half a dogfood profile cannot change.
    //
    // It does set two other properties on the window, which is the modern way
    // to answer the same question and was measured at the same time:
    // _GTK_APPLICATION_ID = "agape48" and _KDE_NET_WM_DESKTOP_FILE = "agape48".
    QGuiApplication::setDesktopFileName(QStringLiteral("agape48"));

    // No multisampling: the LCD is nearest-filtered on purpose and the skin is
    // a photograph. Asking for MSAA costs memory bandwidth and buys nothing.
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setSamples(0);
    QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);

    // --state seeds the profile's state folder, exactly as typing it into
    // Settings would, so a fresh dogfood profile can start pointed at a folder
    // that already has a ROM in it instead of coming up with no calculator.
    //
    // Persisted rather than applied for one run only: changing the state folder
    // in the UI is itself something the dogfood reports test, and a run-only
    // override would silently undo it on the next launch. The key belongs to
    // StateFileManager, which reads it at construction - hence before the engine.
    if (!stateSeed.isEmpty()) {
        QSettings().setValue(
            QStringLiteral("state/location"),
            QUrl::fromLocalFile(QFileInfo(stateSeed).absoluteFilePath()).toString());
    }

    // The window and taskbar icon, on every platform from one place. Without it
    // Qt supplies its own default - a cogwheel, which is what Linux showed.
    // Windows only looked right by accident: the icon it drew belonged to the
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
