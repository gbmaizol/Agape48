#include <QGuiApplication>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
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

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Agape48", "Main");

    return app.exec();
}
