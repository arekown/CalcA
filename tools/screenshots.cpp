// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Renders the README screenshots from the real UI: calca-screenshots <output dir>
#include "appsetup.h"

#include <QDir>
#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(CalcAPlugin)

namespace {

void settle(int ms = 400)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

void call(QObject *c, const char *method, const QString &arg = {})
{
    if (arg.isNull())
        QMetaObject::invokeMethod(c, method);
    else
        QMetaObject::invokeMethod(c, method, Q_ARG(QVariant, arg));
}

void keys(QObject *c, const QString &seq)
{
    call(c, "act", QStringLiteral("clr"));
    for (const QString &k : seq.split(u' ', Qt::SkipEmptyParts))
        call(c, "act", k);
}

} // namespace

int main(int argc, char *argv[])
{
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    QGuiApplication app(argc, argv);
    // separate settings so the user's CalcA settings stay untouched
    QGuiApplication::setOrganizationName(QStringLiteral("CalcA-Screenshots"));
    QGuiApplication::setApplicationName(QStringLiteral("CalcA-Screenshots"));
    loadBundledFonts();

    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("screenshots");
    QDir().mkpath(out);

    QQmlApplicationEngine engine;
    engine.loadFromModule("CalcA", "Main");
    if (engine.rootObjects().isEmpty()) return 1;
    auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QObject *c = win->findChild<QObject *>(QStringLiteral("calc"));
    if (!win || !c) return 1;

    auto shot = [&](const QString &name, int w, int h) {
        win->resize(w, h);
        settle();
        // move the (virtual) pointer off the window so no key is drawn hovered
        const QPointF outside(-20, -20);
        QMouseEvent move(QEvent::MouseMove, outside, win->mapToGlobal(outside), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QGuiApplication::sendEvent(win, &move);
        settle(150);
        const QImage img = win->grabWindow();
        if (img.isNull() || !img.save(out + u'/' + name)) qFatal("could not save %s", qPrintable(name));
    };

    call(c, "setBase", QStringLiteral("DEC"));
    call(c, "setAngle", QStringLiteral("DEG"));

    call(c, "setMode", QStringLiteral("std"));
    keys(c, QStringLiteral("d:1 d:2 d:3 d:4 dot d:5 o:* d:4 eq"));
    shot(QStringLiteral("standard.png"), 380, 620);

    call(c, "setMode", QStringLiteral("sci"));
    keys(c, QStringLiteral("d:2 o:^ d:6 d:4 eq"));
    shot(QStringLiteral("scientific.png"), 420, 680);

    call(c, "setMode", QStringLiteral("std"));
    call(c, "setBase", QStringLiteral("HEX"));
    keys(c, QStringLiteral("d:F d:F o:* d:1 d:0 eq"));
    shot(QStringLiteral("hex.png"), 380, 620);

    call(c, "setBase", QStringLiteral("DEC"));
    keys(c, QStringLiteral("d:1 o:/ d:3 o:* d:3 eq"));
    keys(c, QStringLiteral("d:2 d:0 d:0 o:+ d:1 d:5 pct eq"));
    shot(QStringLiteral("history.png"), 720, 620);
    return 0;
}
