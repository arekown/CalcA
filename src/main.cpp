// SPDX-License-Identifier: GPL-2.0-or-later
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QTranslator>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(CalcAPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("CalcA"));
    QGuiApplication::setApplicationName(QStringLiteral("CalcA"));
    QGuiApplication::setApplicationVersion(QStringLiteral(CALCA_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.arekown.CalcA"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/CalcA/icons/io.github.arekown.CalcA.svg")));

    const QStringList fonts = {
        QStringLiteral("Orbitron-Variable.ttf"), QStringLiteral("Rajdhani-Medium.ttf"),
        QStringLiteral("Rajdhani-SemiBold.ttf"), QStringLiteral("Rajdhani-Bold.ttf"),
        QStringLiteral("ShareTechMono-Regular.ttf"),
    };
    for (const QString &f : fonts)
        QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/CalcA/fonts/") + f);

    // UI language follows the system language; English is the built-in fallback.
    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("calca"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        QGuiApplication::installTranslator(&translator);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("CalcA", "Main");
    return QGuiApplication::exec();
}
