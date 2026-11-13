// SPDX-License-Identifier: GPL-2.0-or-later
// Drives the real Controller (logic.js + libqalculate engine) with key sequences.
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopedPointer>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(CalcAPlugin)

class TstCalc : public QObject
{
    Q_OBJECT

    QQmlEngine m_engine;
    QScopedPointer<QObject> m_calc;

    QString run(const QString &base, const QString &mode, const QString &keys)
    {
        QMetaObject::invokeMethod(m_calc.get(), "setMode", Q_ARG(QVariant, mode));
        QMetaObject::invokeMethod(m_calc.get(), "setBase", Q_ARG(QVariant, base));
        QMetaObject::invokeMethod(m_calc.get(), "act", Q_ARG(QVariant, QStringLiteral("clr")));
        for (const QString &k : keys.split(u' ', Qt::SkipEmptyParts))
            QMetaObject::invokeMethod(m_calc.get(), "act", Q_ARG(QVariant, k));
        const QString err = m_calc->property("error").toString();
        return err.isEmpty() ? m_calc->property("displayText").toString() : QStringLiteral("ERR:") + err;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("CalcA-Test"));
        QCoreApplication::setApplicationName(QStringLiteral("CalcA-Test"));
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        QQmlComponent comp(&m_engine);
        comp.setData("import CalcA\nController {}", QUrl());
        m_calc.reset(comp.create());
        QVERIFY2(m_calc, qPrintable(comp.errorString()));
    }

    void keys_data()
    {
        QTest::addColumn<QString>("base");
        QTest::addColumn<QString>("mode");
        QTest::addColumn<QString>("keys");
        QTest::addColumn<QString>("expected");
        auto row = [](const char *name, const char *base, const char *mode, const char *keys, const char *exp) {
            QTest::newRow(name) << QString::fromUtf8(base) << QString::fromUtf8(mode) << QString::fromUtf8(keys) << QString::fromUtf8(exp);
        };
        row("precedence", "DEC", "std", "d:2 o:+ d:3 o:* d:4 eq", "14");
        row("decimals exact", "DEC", "std", "d:0 dot d:1 o:+ d:0 dot d:2 eq", "0,3");
        row("percent of base", "DEC", "std", "d:2 d:0 d:0 o:+ d:1 d:0 pct eq", "220");
        row("sqrt", "DEC", "std", "d:9 u:sqrt", "3");
        row("divide by zero", "DEC", "std", "d:1 o:/ d:0 eq", "ERR:div0");
        row("parentheses", "DEC", "sci", "lp d:2 o:+ d:3 rp o:* d:4 eq", "20");
        row("power + grouping", "DEC", "sci", "d:2 o:^ d:1 d:0 eq", "1.024");
        row("third times three", "DEC", "std", "d:1 o:/ d:3 o:* d:3 eq", "1");
        row("third", "DEC", "std", "d:1 o:/ d:3 eq", "0,3333333333333333");
        row("reciprocal of zero", "DEC", "std", "u:recip", "ERR:div0");
        row("repeat equals", "DEC", "std", "d:5 o:+ d:3 eq eq", "11");
        row("sin 30 deg", "DEC", "sci", "d:3 d:0 t:sin", "0,5");
        row("tan 90 deg", "DEC", "sci", "d:9 d:0 t:tan", "ERR:invalid");
        row("factorial 25 exact", "DEC", "sci", "d:2 d:5 u:fact", "15.511.210.043.330.985.984.000.000");
        row("2^64 std (16 digits)", "DEC", "std", "d:2 o:^ d:6 d:4 eq", "1,844674407370955e+19");
        row("2^64 sci exact", "DEC", "sci", "d:2 o:^ d:6 d:4 eq", "18.446.744.073.709.551.616");
        row("sqrt negative", "DEC", "std", "d:4 neg u:sqrt", "ERR:invalid");
        row("typed grouping", "DEC", "std", "d:1 d:2 d:3 d:4 d:5 d:6 d:7 dot d:8 d:9", "1.234.567,89");
        row("hex add", "HEX", "std", "d:F d:F o:+ d:1 eq", "100");
        row("hex 64 bit", "HEX", "std", "d:F d:F d:F d:F d:F d:F d:F d:F d:F d:F d:F d:F d:F d:F d:F d:F o:+ d:1 eq", "1 0000 0000 0000 0000");
        row("bin multiply", "BIN", "std", "d:1 d:0 d:1 o:* d:1 d:1 eq", "1111");
        row("bin ignores 7", "BIN", "std", "d:1 d:7 d:0", "10");
        row("exp entry", "DEC", "sci", "d:1 dot d:5 exp d:3 o:+ d:1 eq", "1.501");
        row("yroot", "DEC", "sci", "d:8 o:root d:3 eq", "2");
        row("mod", "DEC", "sci", "d:7 neg o:mod d:3 eq", "-1");
    }

    void keys()
    {
        QFETCH(QString, base);
        QFETCH(QString, mode);
        QFETCH(QString, keys);
        QFETCH(QString, expected);
        QCOMPARE(run(base, mode, keys), expected);
    }

    void baseSwitch()
    {
        run(QStringLiteral("DEC"), QStringLiteral("std"), QStringLiteral("d:2 d:5 d:5 eq"));
        QMetaObject::invokeMethod(m_calc.get(), "cycleBase");
        QCOMPARE(m_calc->property("displayText").toString(), QStringLiteral("FF"));
        QMetaObject::invokeMethod(m_calc.get(), "cycleBase");
        QCOMPARE(m_calc->property("displayText").toString(), QStringLiteral("1111 1111"));
        QMetaObject::invokeMethod(m_calc.get(), "cycleBase");
        QCOMPARE(m_calc->property("displayText").toString(), QStringLiteral("255"));
    }

    void startupShowsZero()
    {
        // Load the real main window: no error may be shown before the first key press.
        QQmlEngine engine;
        QQmlComponent comp(&engine);
        comp.loadFromModule("CalcA", "Main");
        QScopedPointer<QObject> win(comp.create());
        QVERIFY2(win, qPrintable(comp.errorString()));
        QObject *c = win->findChild<QObject *>(QStringLiteral("calc"));
        QVERIFY(c);
        QCOMPARE(c->property("error").toString(), QString());
        QCOMPARE(c->property("displayText").toString(), QStringLiteral("0"));
    }

    void historyRecall()
    {
        run(QStringLiteral("DEC"), QStringLiteral("std"), QStringLiteral("d:6 o:* d:7 eq"));
        const QVariantList hist = m_calc->property("history").toList();
        QVERIFY(!hist.isEmpty());
        QCOMPARE(hist.first().toMap().value("text").toString(), QStringLiteral("42"));
        QCOMPARE(hist.first().toMap().value("expr").toString(), QStringLiteral("6 × 7 ="));
    }
};

QTEST_MAIN(TstCalc)
#include "tst_calc.moc"
