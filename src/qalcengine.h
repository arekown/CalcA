// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// QML-facing wrapper around libqalculate.
//
// Every number that crosses the boundary is a string in libqalculate syntax:
// base 10, '.' as decimal point, exact fractions such as "1/3", 'E' exponents.
// Results come back as {"v": <number>} or {"err": "div0" | "invalid" | "overflow" | "timeout"}.
class QalcEngine : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Engine)
    QML_SINGLETON

public:
    explicit QalcEngine(QObject *parent = nullptr);

    // Binary operation: + - * / mod ^ root logy
    Q_INVOKABLE QVariantMap op(const QString &o, const QString &a, const QString &b) const;
    // Unary function (sqr, sqrt, sin, asinh, fact, dms ...); angle is DEG, RAD or GRAD.
    Q_INVOKABLE QVariantMap fn(const QString &name, const QString &x, const QString &angle) const;
    // Evaluates a constant or literal such as "pi", "e", "0xFF".
    Q_INVOKABLE QVariantMap value(const QString &expr) const;
    // Display text in the system locale. base 10/16/2; digits = significant digits (base 10).
    Q_INVOKABLE QString format(const QString &v, int base, bool fe, int digits) const;

    Q_INVOKABLE void copyText(const QString &text) const;
    Q_INVOKABLE QString clipboardText() const;
};
