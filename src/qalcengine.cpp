// SPDX-License-Identifier: GPL-2.0-or-later
#include "qalcengine.h"

#include <libqalculate/qalculate.h>

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>

namespace {

constexpr int kPrecision = 40;   // internal significant digits
constexpr int kTimeoutMs = 3000; // guard against runaway calculations (e.g. 10^(10^9))

Calculator *calc()
{
    static Calculator *c = [] {
        auto *instance = new Calculator(true); // ignore system locale: '.' is always the decimal point
        instance->loadGlobalDefinitions();
        instance->setPrecision(kPrecision);
        // libqalculate's worker thread blocks in fread() holding the stdio lock; exit() would
        // then deadlock while flushing streams. Stop the thread before static teardown.
        qAddPostRoutine([] { CALCULATOR->terminateThreads(); });
        return instance;
    }();
    return c;
}

EvaluationOptions evalOptions()
{
    EvaluationOptions eo;
    eo.approximation = APPROXIMATION_APPROXIMATE;
    eo.parse_options.angle_unit = ANGLE_UNIT_RADIANS; // degree/grad conversion is explicit below
    eo.parse_options.base = BASE_DECIMAL;
    eo.interval_calculation = INTERVAL_CALCULATION_NONE;
    eo.allow_complex = false;
    eo.allow_infinite = false;
    return eo;
}

PrintOptions basePrintOptions()
{
    PrintOptions po;
    po.use_unicode_signs = false;
    po.spacious = false;
    po.digit_grouping = DIGIT_GROUPING_NONE;
    po.decimalpoint_sign = ".";
    po.comma_sign = ",";
    po.interval_display = INTERVAL_DISPLAY_MIDPOINT;
    po.show_ending_zeroes = false;
    po.indicate_infinite_series = false;
    return po;
}

struct Result {
    QString err;
    MathStructure m;
};

Result evaluate(const QString &expr)
{
    Result r;
    calc()->clearMessages();
    if (!calc()->calculate(&r.m, expr.toStdString(), kTimeoutMs, evalOptions())) {
        r.err = QStringLiteral("timeout");
        return r;
    }
    if (!r.m.isNumber()) {
        r.err = QStringLiteral("invalid");
        return r;
    }
    const Number &n = r.m.number();
    if (n.isUndefined() || n.hasImaginaryPart())
        r.err = QStringLiteral("invalid");
    else if (n.isInfinite())
        r.err = QStringLiteral("overflow");
    return r;
}

// Exact transport form: fractions stay fractions, floats keep full internal precision.
QString transport(const MathStructure &m)
{
    PrintOptions po = basePrintOptions();
    po.base = BASE_DECIMAL;
    po.number_fraction_format = FRACTION_FRACTIONAL;
    po.min_exp = EXP_PRECISION;
    po.exp_display = EXP_UPPERCASE_E;
    return QString::fromStdString(calc()->print(m, kTimeoutMs, po));
}

QVariantMap fail(const QString &code) { return {{QStringLiteral("err"), code}}; }

QVariantMap calcExpr(const QString &expr)
{
    const Result r = evaluate(expr);
    if (!r.err.isEmpty())
        return fail(r.err);
    return {{QStringLiteral("v"), transport(r.m)}};
}

QString P(const QString &x) { return QLatin1Char('(') + x + QLatin1Char(')'); }

bool isZero(const QString &x)
{
    const Result r = evaluate(x);
    return r.err.isEmpty() && r.m.isZero();
}

bool isInteger(const QString &x)
{
    const Result r = evaluate(x);
    return r.err.isEmpty() && r.m.number().isInteger();
}

// sin/cos of an argument, with float noise around zero snapped to exactly 0.
QString snappedTrig(const QString &f, const QString &arg, bool *ok)
{
    Result r = evaluate(f + P(arg));
    if (!r.err.isEmpty()) {
        *ok = false;
        return {};
    }
    Number a = r.m.number();
    a.abs();
    if (a.isLessThan(Number(1, 1, -30)))
        return QStringLiteral("0");
    return transport(r.m);
}

QString toRad(const QString &x, const QString &angle)
{
    if (angle == QLatin1String("DEG")) return P(x) + QStringLiteral("*pi/180");
    if (angle == QLatin1String("GRAD")) return P(x) + QStringLiteral("*pi/200");
    return P(x);
}

QString fromRad(const QString &e, const QString &angle)
{
    if (angle == QLatin1String("DEG")) return P(e) + QStringLiteral("*180/pi");
    if (angle == QLatin1String("GRAD")) return P(e) + QStringLiteral("*200/pi");
    return P(e);
}

QVariantMap trig(const QString &name, const QString &x, const QString &angle)
{
    const QString X = P(x);
    const QString invalid = QStringLiteral("invalid");

    // forward circular functions
    if (name == u"sin" || name == u"cos" || name == u"tan" || name == u"sec" || name == u"csc" || name == u"cot") {
        bool ok = true;
        const QString arg = toRad(x, angle);
        const QString s = snappedTrig(QStringLiteral("sin"), arg, &ok);
        const QString c = snappedTrig(QStringLiteral("cos"), arg, &ok);
        if (!ok) return fail(invalid);
        const bool sz = s == u"0", cz = c == u"0";
        if (name == u"sin") return calcExpr(s);
        if (name == u"cos") return calcExpr(c);
        if (name == u"tan") return cz ? fail(invalid) : calcExpr(P(s) + u'/' + P(c));
        if (name == u"sec") return cz ? fail(invalid) : calcExpr(QStringLiteral("1/") + P(c));
        if (name == u"csc") return sz ? fail(invalid) : calcExpr(QStringLiteral("1/") + P(s));
        return sz ? fail(invalid) : calcExpr(P(c) + u'/' + P(s));
    }
    // inverse circular functions (result converted to the angle unit)
    if (name == u"asin") return calcExpr(fromRad(QStringLiteral("asin") + X, angle));
    if (name == u"acos") return calcExpr(fromRad(QStringLiteral("acos") + X, angle));
    if (name == u"atan") return calcExpr(fromRad(QStringLiteral("atan") + X, angle));
    if (name == u"asec") return isZero(x) ? fail(invalid) : calcExpr(fromRad(QStringLiteral("acos(1/") + X + u')', angle));
    if (name == u"acsc") return isZero(x) ? fail(invalid) : calcExpr(fromRad(QStringLiteral("asin(1/") + X + u')', angle));
    if (name == u"acot") return calcExpr(fromRad(isZero(x) ? QStringLiteral("pi/2") : QStringLiteral("atan(1/") + X + u')', angle));
    // hyperbolic functions
    if (name == u"sinh" || name == u"cosh" || name == u"tanh" || name == u"asinh" || name == u"acosh" || name == u"atanh")
        return calcExpr(name + X);
    if (name == u"sech") return calcExpr(QStringLiteral("1/cosh") + X);
    if (name == u"csch") return isZero(x) ? fail(invalid) : calcExpr(QStringLiteral("1/sinh") + X);
    if (name == u"coth") return isZero(x) ? fail(invalid) : calcExpr(QStringLiteral("1/tanh") + X);
    if (name == u"asech") return isZero(x) ? fail(invalid) : calcExpr(QStringLiteral("acosh(1/") + X + u')');
    if (name == u"acsch") return isZero(x) ? fail(invalid) : calcExpr(QStringLiteral("asinh(1/") + X + u')');
    if (name == u"acoth") return isZero(x) ? fail(invalid) : calcExpr(QStringLiteral("atanh(1/") + X + u')');
    return fail(invalid);
}

// Plain "1234.5", "-1.5e20" → system locale ("1.234,5", "-1,5e+20"); fe forces scientific notation.
QString localize(QString s, bool fe)
{
    const QLocale loc;
    QString sign;
    if (s.startsWith(u'-')) {
        sign = QStringLiteral("-");
        s.remove(0, 1);
    }
    QString mant = s;
    long exp = 0;
    bool hasExp = false;
    const qsizetype e = s.indexOf(u'e', 0, Qt::CaseInsensitive);
    if (e >= 0) {
        mant = s.left(e);
        exp = s.mid(e + 1).toLong();
        hasExp = true;
    }
    QString ip = mant.section(u'.', 0, 0), fp = mant.section(u'.', 1);

    if (fe) { // normalise to d.ddd × 10^exp
        QString all = ip + fp;
        exp += ip.size() - 1;
        int lead = 0;
        while (lead < all.size() && all[lead] == u'0') ++lead;
        all = all.mid(lead);
        exp -= lead;
        while (all.endsWith(u'0')) all.chop(1);
        if (all.isEmpty()) {
            all = QStringLiteral("0");
            exp = 0;
        }
        ip = all.left(1);
        fp = all.mid(1);
        hasExp = true;
    }

    QString grouped;
    for (qsizetype i = 0; i < ip.size(); ++i) {
        if (i > 0 && (ip.size() - i) % 3 == 0) grouped += loc.groupSeparator();
        grouped += ip[i];
    }
    QString out = sign + grouped;
    if (!fp.isEmpty()) out += loc.decimalPoint() + fp;
    if (hasExp) out += QStringLiteral("e") + (exp < 0 ? QStringLiteral("-") : QStringLiteral("+")) + QString::number(std::labs(exp));
    return out;
}

} // namespace

QalcEngine::QalcEngine(QObject *parent)
    : QObject(parent)
{
    calc(); // load definitions once, up front
}

QVariantMap QalcEngine::op(const QString &o, const QString &a, const QString &b) const
{
    const QString A = P(a), B = P(b);
    if (o == u"+") return calcExpr(A + u'+' + B);
    if (o == u"-") return calcExpr(A + u'-' + B);
    if (o == u"*") return calcExpr(A + u'*' + B);
    if (o == u"/") {
        if (isZero(b)) return fail(isZero(a) ? QStringLiteral("invalid") : QStringLiteral("div0"));
        return calcExpr(A + u'/' + B);
    }
    if (o == u"mod") {
        if (isZero(b)) return fail(QStringLiteral("invalid"));
        return calcExpr(QStringLiteral("rem(") + a + u',' + b + u')');
    }
    if (o == u"^") return calcExpr(A + u'^' + B);
    if (o == u"root") {
        if (isZero(b)) return fail(QStringLiteral("invalid"));
        if (isInteger(b)) return calcExpr(QStringLiteral("root(") + a + u',' + b + u')');
        return calcExpr(A + QStringLiteral("^(1/") + B + u')');
    }
    if (o == u"logy") return calcExpr(QStringLiteral("log(") + a + u',' + b + u')');
    return fail(QStringLiteral("invalid"));
}

QVariantMap QalcEngine::fn(const QString &name, const QString &x, const QString &angle) const
{
    const QString X = P(x);
    if (name == u"sqr") return calcExpr(X + QStringLiteral("^2"));
    if (name == u"cube") return calcExpr(X + QStringLiteral("^3"));
    if (name == u"recip") return isZero(x) ? fail(QStringLiteral("div0")) : calcExpr(QStringLiteral("1/") + X);
    if (name == u"sqrt") return calcExpr(QStringLiteral("sqrt") + X);
    if (name == u"cbrt") return calcExpr(QStringLiteral("cbrt") + X);
    if (name == u"abs") return calcExpr(QStringLiteral("abs") + X);
    if (name == u"fact") return calcExpr(QStringLiteral("gamma(") + X + QStringLiteral("+1)"));
    if (name == u"pow10") return calcExpr(QStringLiteral("10^") + X);
    if (name == u"pow2") return calcExpr(QStringLiteral("2^") + X);
    if (name == u"log") return calcExpr(QStringLiteral("log10") + X);
    if (name == u"ln") return calcExpr(QStringLiteral("ln") + X);
    if (name == u"exp") return calcExpr(QStringLiteral("exp") + X);
    if (name == u"floor") return calcExpr(QStringLiteral("floor") + X);
    if (name == u"ceil") return calcExpr(QStringLiteral("ceil") + X);
    if (name == u"neg") return calcExpr(u'-' + X);
    if (name == u"dms") { // decimal degrees → d.mmss
        const QString a = QStringLiteral("abs") + X, d = QStringLiteral("trunc(") + a + u')';
        const QString r = P(P(a) + u'-' + d) + QStringLiteral("*60"), m = QStringLiteral("trunc(") + r + u')';
        return calcExpr(QStringLiteral("sgn") + X + QStringLiteral("*(") + d + u'+' + m + QStringLiteral("/100+") + P(r + u'-' + m) + QStringLiteral("*60/10000)"));
    }
    if (name == u"deg") { // d.mmss → decimal degrees
        const QString a = QStringLiteral("abs") + X, d = QStringLiteral("trunc(") + a + u')';
        const QString mm = P(P(a) + u'-' + d) + QStringLiteral("*100"), m = QStringLiteral("trunc(") + mm + u')';
        return calcExpr(QStringLiteral("sgn") + X + QStringLiteral("*(") + d + u'+' + m + QStringLiteral("/60+") + P(mm + u'-' + m) + QStringLiteral("*100/3600)"));
    }
    return trig(name, x, angle);
}

QVariantMap QalcEngine::value(const QString &expr) const { return calcExpr(expr); }

QString QalcEngine::format(const QString &v, int base, bool fe, int digits) const
{
    const Result r = evaluate(v);
    if (!r.err.isEmpty()) return QStringLiteral("?");

    if (base != 10) { // integer part, grouped by 4 digits
        Number n = r.m.number();
        n.trunc();
        PrintOptions po = basePrintOptions();
        po.base = base;
        po.base_display = BASE_DISPLAY_NONE;
        po.twos_complement = false;
        po.hexadecimal_twos_complement = false;
        po.lower_case_numbers = false;
        QString s = QString::fromStdString(n.print(po)).remove(u' ');
        QString sign;
        if (s.startsWith(u'-')) {
            sign = QStringLiteral("-");
            s.remove(0, 1);
        }
        QString out;
        for (qsizetype i = 0; i < s.size(); ++i) {
            if (i > 0 && (s.size() - i) % 4 == 0) out += u' ';
            out += s[i];
        }
        return sign + out;
    }

    PrintOptions po = basePrintOptions();
    po.base = BASE_DECIMAL;
    po.number_fraction_format = FRACTION_DECIMAL;
    po.min_exp = EXP_PRECISION;
    po.exp_display = EXP_LOWERCASE_E;
    calc()->setPrecision(digits);
    const std::string out = calc()->print(r.m, kTimeoutMs, po);
    calc()->setPrecision(kPrecision);
    return localize(QString::fromStdString(out), fe);
}

void QalcEngine::copyText(const QString &text) const { QGuiApplication::clipboard()->setText(text); }

QString QalcEngine::clipboardText() const { return QGuiApplication::clipboard()->text(); }
