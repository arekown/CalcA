// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// QML-facing calculation engine (GMP rationals + MPFR functions).
//
// Every number that crosses the boundary is a string: an exact fraction such as "1/3" or "-42",
// or an input literal ("12.5", "1.5E+3", "0xFF", "0b101", "pi", "e").
// Results come back as {"v": <number>} or {"err": "div0" | "invalid" | "overflow"}.
class CalcEngine : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Engine)
    QML_SINGLETON

public:
    explicit CalcEngine(QObject *parent = nullptr);

    // Binary operation: + - * / mod ^ root logy
    Q_INVOKABLE QVariantMap op(const QString &o, const QString &a, const QString &b) const;
    // Unary function (sqr, sqrt, sin, asinh, fact, dms ...); angle is DEG, RAD or GRAD.
    Q_INVOKABLE QVariantMap fn(const QString &name, const QString &x, const QString &angle) const;
    // Evaluates a literal or constant ("pi", "e", "0xFF").
    Q_INVOKABLE QVariantMap value(const QString &expr) const;
    // Display text in the system locale. base 10/16/2; digits = significant digits (base 10).
    Q_INVOKABLE QString format(const QString &v, int base, bool fe, int digits) const;

    Q_INVOKABLE void copyText(const QString &text) const;
    Q_INVOKABLE QString clipboardText() const;
};
