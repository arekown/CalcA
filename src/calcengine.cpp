// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "calcengine.h"

#include <gmpxx.h>
#include <mpfr.h>

#include <QClipboard>
#include <QGuiApplication>
#include <QLocale>
#include <QRegularExpression>

#include <algorithm>
#include <cstdlib>
#include <string>

namespace {

constexpr mpfr_prec_t kBits = 192;        // working precision of irrational functions (~57 digits)
constexpr int kDigits = 40;               // approximate results are rounded to 40 significant digits
constexpr long kMaxBits = 33220;          // |x| < 10^10000, like the Windows calculator
constexpr unsigned long kMaxFactorial = 3248; // 3249! > 10^10000
constexpr size_t kMaxFractionBits = 1024; // larger exact fractions are rounded to kDigits

struct Err {
    const char *code;
};
const Err kInvalid{"invalid"};
const Err kDiv0{"div0"};
const Err kOverflow{"overflow"};

// RAII wrapper for an MPFR float at working precision.
struct F {
    mpfr_t v;
    F() { mpfr_init2(v, kBits); }
    explicit F(const mpq_class &q) : F() { mpfr_set_q(v, q.get_mpq_t(), MPFR_RNDN); }
    ~F() { mpfr_clear(v); }
    F(const F &) = delete;
    F &operator=(const F &) = delete;
};

mpz_class pow10(unsigned long e)
{
    mpz_class p;
    mpz_ui_pow_ui(p.get_mpz_t(), 10, e);
    return p;
}

mpz_class trunc(const mpq_class &q)
{
    mpz_class t;
    mpz_tdiv_q(t.get_mpz_t(), q.get_num_mpz_t(), q.get_den_mpz_t());
    return t;
}

bool isInt(const mpq_class &q) { return q.get_den() == 1; }

long log2Magnitude(const mpq_class &q)
{
    return long(mpz_sizeinbase(q.get_num_mpz_t(), 2)) - long(mpz_sizeinbase(q.get_den_mpz_t(), 2));
}

// MPFR result → exact decimal fraction with kDigits significant digits.
mpq_class fromF(const F &f)
{
    if (mpfr_nan_p(f.v)) throw kInvalid;
    if (mpfr_inf_p(f.v)) throw kOverflow;
    if (mpfr_zero_p(f.v)) return 0;
    mpfr_exp_t e10 = 0;
    char *s = mpfr_get_str(nullptr, &e10, 10, kDigits, f.v, MPFR_RNDN);
    std::string digits(s);
    mpfr_free_str(s);
    if (e10 > 10001) throw kOverflow;
    if (e10 < -10001) return 0;
    const bool neg = digits[0] == '-';
    if (neg) digits.erase(0, 1);
    const long shift = long(e10) - long(digits.size());
    mpz_class m(digits, 10);
    if (neg) m = -m;
    mpq_class q = shift >= 0 ? mpq_class(m * pow10(shift)) : mpq_class(m, pow10(-shift));
    q.canonicalize();
    return q;
}

// Range check and size control for exact results.
mpq_class tidy(mpq_class q)
{
    q.canonicalize();
    if (q == 0) return q;
    const long m = log2Magnitude(q);
    if (m > kMaxBits) throw kOverflow;
    if (m < -kMaxBits - 2) return 0;
    if (!isInt(q) && mpz_sizeinbase(q.get_num_mpz_t(), 2) + mpz_sizeinbase(q.get_den_mpz_t(), 2) > kMaxFractionBits) {
        F f(q); // round huge fractions to kDigits significant digits
        return fromF(f);
    }
    return q;
}

mpq_class constPi()
{
    F f;
    mpfr_const_pi(f.v, MPFR_RNDN);
    return fromF(f);
}

mpq_class parse(QString s)
{
    s = s.trimmed();
    if (s == u"pi") return constPi();
    if (s == u"e") {
        F f;
        mpfr_set_ui(f.v, 1, MPFR_RNDN);
        mpfr_exp(f.v, f.v, MPFR_RNDN);
        return fromF(f);
    }
    bool neg = false;
    if (s.startsWith(u'-')) {
        neg = true;
        s.remove(0, 1);
    }
    mpq_class q;
    static const QRegularExpression hexRe(QStringLiteral("^0x[0-9a-fA-F]+$"));
    static const QRegularExpression binRe(QStringLiteral("^0b[01]+$"));
    static const QRegularExpression fracRe(QStringLiteral("^\\d+/\\d+$"));
    static const QRegularExpression decRe(QStringLiteral("^(\\d*)(?:\\.(\\d*))?(?:[eE]([+-]?\\d{1,6}))?$"));
    if (hexRe.match(s).hasMatch()) {
        q = mpz_class(s.mid(2).toStdString(), 16);
    } else if (binRe.match(s).hasMatch()) {
        q = mpz_class(s.mid(2).toStdString(), 2);
    } else if (fracRe.match(s).hasMatch()) {
        q = mpq_class(s.toStdString(), 10);
        if (q.get_den() == 0) throw kInvalid;
        q.canonicalize();
    } else {
        const auto m = decRe.match(s);
        if (!m.hasMatch()) throw kInvalid;
        const QString ip = m.captured(1), fp = m.captured(2);
        if (ip.isEmpty() && fp.isEmpty()) throw kInvalid;
        const long ex = m.captured(3).isEmpty() ? 0 : m.captured(3).toLong();
        const long shift = ex - long(fp.size());
        if (shift > 20000) throw kOverflow;
        if (shift < -20000) return 0;
        const mpz_class mant((ip + fp).toStdString(), 10);
        q = shift >= 0 ? mpq_class(mant * pow10(shift)) : mpq_class(mant, pow10(-shift));
        q.canonicalize();
    }
    return tidy(neg ? mpq_class(-q) : q);
}

QString toTransport(const mpq_class &q) { return QString::fromStdString(q.get_str(10)); }

// ---------- arithmetic ----------
mpq_class divide(const mpq_class &a, const mpq_class &b)
{
    if (b == 0) throw a == 0 ? kInvalid : kDiv0;
    return tidy(a / b);
}

mpq_class power(const mpq_class &a, const mpq_class &b)
{
    if (a == 0) {
        if (b < 0) throw kDiv0;
        return b == 0 ? 1 : 0;
    }
    if (isInt(b) && mpz_fits_slong_p(b.get_num_mpz_t())) {
        const long e = b.get_num().get_si();
        const long bits = long(std::max(mpz_sizeinbase(a.get_num_mpz_t(), 2), mpz_sizeinbase(a.get_den_mpz_t(), 2)));
        if (double(std::labs(e)) * double(bits) <= double(kMaxBits) + 64.0) { // exact
            mpz_class n, d;
            mpz_pow_ui(n.get_mpz_t(), a.get_num_mpz_t(), std::labs(e));
            mpz_pow_ui(d.get_mpz_t(), a.get_den_mpz_t(), std::labs(e));
            return tidy(e >= 0 ? mpq_class(n, d) : mpq_class(d, n));
        }
    }
    if (a < 0 && !isInt(b)) throw kInvalid;
    F fa(a), fb(b), r;
    mpfr_pow(r.v, fa.v, fb.v, MPFR_RNDN);
    return fromF(r);
}

// Exact root if numerator and denominator are perfect n-th powers.
bool exactRoot(const mpq_class &a, unsigned long n, mpq_class *out)
{
    mpz_class num = abs(a.get_num()), rn, rd;
    if (!mpz_root(rn.get_mpz_t(), num.get_mpz_t(), n)) return false;
    if (!mpz_root(rd.get_mpz_t(), a.get_den_mpz_t(), n)) return false;
    *out = mpq_class(a < 0 ? mpz_class(-rn) : rn, rd);
    out->canonicalize();
    return true;
}

mpq_class root(const mpq_class &a, const mpq_class &b)
{
    if (b == 0) throw kInvalid;
    if (isInt(b) && mpz_fits_slong_p(b.get_num_mpz_t())) {
        const long n = b.get_num().get_si();
        if (a < 0 && n % 2 == 0) throw kInvalid;
        mpq_class r;
        if (exactRoot(a, std::labs(n), &r)) return n > 0 ? r : divide(1, r);
        F fa(a), fr;
        mpfr_rootn_si(fr.v, fa.v, n, MPFR_RNDN);
        return fromF(fr);
    }
    if (a < 0) throw kInvalid;
    return power(a, divide(1, b));
}

using MpfrFn = int (*)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
mpq_class apply(MpfrFn f, const mpq_class &x)
{
    F fx(x), r;
    f(r.v, fx.v, MPFR_RNDN);
    return fromF(r);
}

mpq_class logBase(const mpq_class &a, const mpq_class &b)
{
    if (a <= 0 || b <= 0 || b == 1) throw kInvalid;
    F fa(a), fb(b);
    mpfr_log(fa.v, fa.v, MPFR_RNDN);
    mpfr_log(fb.v, fb.v, MPFR_RNDN);
    mpfr_div(fa.v, fa.v, fb.v, MPFR_RNDN);
    return fromF(fa);
}

mpq_class factorial(const mpq_class &x)
{
    if (isInt(x)) {
        if (x < 0) throw kInvalid;
        if (x > kMaxFactorial) throw kOverflow;
        mpz_class r;
        mpz_fac_ui(r.get_mpz_t(), x.get_num().get_ui());
        return r;
    }
    F f(x + 1);
    mpfr_gamma(f.v, f.v, MPFR_RNDN);
    return fromF(f);
}

// ---------- trigonometry ----------
void toRadians(F &out, const mpq_class &x, const QString &angle)
{
    mpq_class a = x;
    if (angle == u"DEG" || angle == u"GRAD") { // reduce exactly first: keeps sin(180°) = 0
        const mpq_class full = angle == u"DEG" ? 360 : 400;
        mpz_class k;
        mpq_class t = a / full;
        mpz_fdiv_q(k.get_mpz_t(), t.get_num_mpz_t(), t.get_den_mpz_t());
        a -= full * k;
        a /= full / 2;
        mpfr_set_q(out.v, a.get_mpq_t(), MPFR_RNDN);
        F pi;
        mpfr_const_pi(pi.v, MPFR_RNDN);
        mpfr_mul(out.v, out.v, pi.v, MPFR_RNDN);
    } else {
        mpfr_set_q(out.v, a.get_mpq_t(), MPFR_RNDN);
    }
}

mpq_class fromRadians(F &r, const QString &angle)
{
    if (angle == u"DEG" || angle == u"GRAD") {
        F pi;
        mpfr_const_pi(pi.v, MPFR_RNDN);
        mpfr_div(r.v, r.v, pi.v, MPFR_RNDN);
        mpfr_mul_ui(r.v, r.v, angle == u"DEG" ? 180 : 200, MPFR_RNDN);
    }
    return fromF(r);
}

bool snapZero(F &f)
{
    F eps;
    mpfr_set_str(eps.v, "1e-40", 10, MPFR_RNDN);
    if (mpfr_cmpabs(f.v, eps.v) < 0) mpfr_set_zero(f.v, 1);
    return mpfr_zero_p(f.v);
}

mpq_class trig(const QString &name, const mpq_class &x, const QString &angle)
{
    if (name == u"sin" || name == u"cos" || name == u"tan" || name == u"sec" || name == u"csc" || name == u"cot") {
        F arg, s, c;
        toRadians(arg, x, angle);
        mpfr_sin_cos(s.v, c.v, arg.v, MPFR_RNDN);
        const bool sz = snapZero(s), cz = snapZero(c);
        F r;
        if (name == u"sin") return fromF(s);
        if (name == u"cos") return fromF(c);
        if (name == u"tan" || name == u"sec") {
            if (cz) throw kInvalid;
            if (name == u"tan") mpfr_div(r.v, s.v, c.v, MPFR_RNDN); else mpfr_ui_div(r.v, 1, c.v, MPFR_RNDN);
            return fromF(r);
        }
        if (sz) throw kInvalid;
        if (name == u"csc") mpfr_ui_div(r.v, 1, s.v, MPFR_RNDN); else mpfr_div(r.v, c.v, s.v, MPFR_RNDN);
        return fromF(r);
    }
    if (name == u"asin" || name == u"acos" || name == u"atan" || name == u"asec" || name == u"acsc" || name == u"acot") {
        mpq_class v = x;
        if (name == u"asec" || name == u"acsc") {
            if (x == 0) throw kInvalid;
            v = 1 / x;
        }
        F fx(v), r;
        if (name == u"asin" || name == u"acsc") mpfr_asin(r.v, fx.v, MPFR_RNDN);
        else if (name == u"acos" || name == u"asec") mpfr_acos(r.v, fx.v, MPFR_RNDN);
        else if (name == u"atan") mpfr_atan(r.v, fx.v, MPFR_RNDN);
        else if (x == 0) { mpfr_const_pi(r.v, MPFR_RNDN); mpfr_div_ui(r.v, r.v, 2, MPFR_RNDN); }
        else { F inv(mpq_class(1 / x)); mpfr_atan(r.v, inv.v, MPFR_RNDN); }
        if (mpfr_nan_p(r.v)) throw kInvalid;
        return fromRadians(r, angle);
    }
    // hyperbolic (no angle unit)
    if (name == u"sinh") return apply(mpfr_sinh, x);
    if (name == u"cosh") return apply(mpfr_cosh, x);
    if (name == u"tanh") return apply(mpfr_tanh, x);
    if (name == u"sech") return apply(mpfr_sech, x);
    if (name == u"csch") { if (x == 0) throw kInvalid; return apply(mpfr_csch, x); }
    if (name == u"coth") { if (x == 0) throw kInvalid; return apply(mpfr_coth, x); }
    if (name == u"asinh") return apply(mpfr_asinh, x);
    if (name == u"acosh") { if (x < 1) throw kInvalid; return apply(mpfr_acosh, x); }
    if (name == u"atanh") { if (abs(x) >= 1) throw kInvalid; return apply(mpfr_atanh, x); }
    if (name == u"asech") { if (x <= 0 || x > 1) throw kInvalid; return apply(mpfr_acosh, mpq_class(1 / x)); }
    if (name == u"acsch") { if (x == 0) throw kInvalid; return apply(mpfr_asinh, mpq_class(1 / x)); }
    if (name == u"acoth") { if (abs(x) <= 1) throw kInvalid; return apply(mpfr_atanh, mpq_class(1 / x)); }
    throw kInvalid;
}

// ---------- functions ----------
mpq_class unary(const QString &name, const mpq_class &x, const QString &angle)
{
    if (name == u"sqr") return tidy(x * x);
    if (name == u"cube") return tidy(x * x * x);
    if (name == u"recip") return divide(1, x);
    if (name == u"neg") return -x;
    if (name == u"abs") return abs(x);
    if (name == u"sqrt") return root(x, 2);
    if (name == u"cbrt") return root(x, 3);
    if (name == u"fact") return factorial(x);
    if (name == u"pow10") return power(10, x);
    if (name == u"pow2") return power(2, x);
    if (name == u"exp") return apply(mpfr_exp, x);
    if (name == u"log") { if (x <= 0) throw kInvalid; return apply(mpfr_log10, x); }
    if (name == u"ln") { if (x <= 0) throw kInvalid; return apply(mpfr_log, x); }
    if (name == u"floor" || name == u"ceil") {
        mpz_class r;
        if (name == u"floor") mpz_fdiv_q(r.get_mpz_t(), x.get_num_mpz_t(), x.get_den_mpz_t());
        else mpz_cdiv_q(r.get_mpz_t(), x.get_num_mpz_t(), x.get_den_mpz_t());
        return r;
    }
    if (name == u"dms" || name == u"deg") { // decimal degrees ↔ d.mmss
        const int sign = sgn(x);
        const mpq_class a = abs(x), d = trunc(a);
        if (name == u"dms") {
            const mpq_class r = (a - d) * 60, m = trunc(r), s = (r - m) * 60;
            return tidy(sign * (d + m / 100 + s / 10000));
        }
        const mpq_class mm = (a - d) * 100, m = trunc(mm), s = (mm - m) * 100;
        return tidy(sign * (d + m / 60 + s / 3600));
    }
    return trig(name, x, angle);
}

mpq_class binary(const QString &o, const mpq_class &a, const mpq_class &b)
{
    if (o == u"+") return tidy(a + b);
    if (o == u"-") return tidy(a - b);
    if (o == u"*") return tidy(a * b);
    if (o == u"/") return divide(a, b);
    if (o == u"mod") { // remainder with the sign of the dividend
        if (b == 0) throw kInvalid;
        return tidy(a - b * trunc(a / b));
    }
    if (o == u"^") return power(a, b);
    if (o == u"root") return root(a, b);
    if (o == u"logy") return logBase(a, b);
    throw kInvalid;
}

template<typename Fn>
QVariantMap guarded(Fn &&fn)
{
    try {
        return {{QStringLiteral("v"), toTransport(fn())}};
    } catch (const Err &e) {
        return {{QStringLiteral("err"), QString::fromLatin1(e.code)}};
    }
}

// ---------- display ----------
// Plain "1234.5" / "1.5e20" → system locale ("1.234,5" / "1,5e+20").
QString localize(const QString &sign, const QString &ip, const QString &fp, bool hasExp, long exp)
{
    const QLocale loc;
    QString grouped;
    for (qsizetype i = 0; i < ip.size(); ++i) {
        if (i > 0 && (ip.size() - i) % 3 == 0) grouped += loc.groupSeparator();
        grouped += ip[i];
    }
    QString out = sign + grouped;
    if (!fp.isEmpty()) out += loc.decimalPoint() + fp;
    if (hasExp) out += QStringLiteral("e") + (exp < 0 ? u'-' : u'+') + QString::number(std::labs(exp));
    return out;
}

QString formatDecimal(const mpq_class &q, bool fe, int digits)
{
    if (q == 0) return fe ? localize({}, QStringLiteral("0"), {}, true, 0) : QStringLiteral("0");
    const mpz_class n = abs(q.get_num()), d = q.get_den();
    auto cmpPow10 = [&](long e) { // sign of n/d - 10^e
        return e >= 0 ? cmp(n, mpz_class(d * pow10(e))) : cmp(mpz_class(n * pow10(-e)), d);
    };
    long e = long(mpz_sizeinbase(n.get_mpz_t(), 10)) - long(mpz_sizeinbase(d.get_mpz_t(), 10));
    while (cmpPow10(e) < 0) --e;
    while (cmpPow10(e + 1) >= 0) ++e;

    // round to `digits` significant digits (half away from zero)
    const long k = digits - 1 - e;
    mpz_class num = n, den = d;
    if (k >= 0) num *= pow10(k); else den *= pow10(-k);
    mpz_class s = (2 * num + den) / (2 * den);
    if (s >= pow10(digits)) {
        s /= 10;
        ++e;
    }
    std::string ds = s.get_str(10);
    while (ds.size() > 1 && ds.back() == '0') ds.pop_back();
    const QString dstr = QString::fromStdString(ds);
    const QString sign = q < 0 ? QStringLiteral("-") : QString();

    if (fe || e >= digits || e < -7)
        return localize(sign, dstr.left(1), dstr.mid(1), true, e);
    if (e >= 0) {
        if (dstr.size() <= e + 1) return localize(sign, dstr + QString(e + 1 - dstr.size(), u'0'), {}, false, 0);
        return localize(sign, dstr.left(e + 1), dstr.mid(e + 1), false, 0);
    }
    return localize(sign, QStringLiteral("0"), QString(-e - 1, u'0') + dstr, false, 0);
}

QString formatBase(const mpq_class &q, int base)
{
    const mpz_class t = trunc(q);
    QString s = QString::fromStdString(mpz_class(abs(t)).get_str(base)).toUpper();
    QString out;
    for (qsizetype i = 0; i < s.size(); ++i) {
        if (i > 0 && (s.size() - i) % 4 == 0) out += u' ';
        out += s[i];
    }
    return (t < 0 ? QStringLiteral("-") : QString()) + out;
}

} // namespace

CalcEngine::CalcEngine(QObject *parent)
    : QObject(parent)
{
}

QVariantMap CalcEngine::op(const QString &o, const QString &a, const QString &b) const
{
    return guarded([&] { return binary(o, parse(a), parse(b)); });
}

QVariantMap CalcEngine::fn(const QString &name, const QString &x, const QString &angle) const
{
    return guarded([&] { return unary(name, parse(x), angle); });
}

QVariantMap CalcEngine::value(const QString &expr) const
{
    return guarded([&] { return parse(expr); });
}

QString CalcEngine::format(const QString &v, int base, bool fe, int digits) const
{
    try {
        const mpq_class q = parse(v);
        return base == 10 ? formatDecimal(q, fe, digits) : formatBase(q, base);
    } catch (const Err &) {
        return QStringLiteral("?");
    }
}

void CalcEngine::copyText(const QString &text) const { QGuiApplication::clipboard()->setText(text); }

QString CalcEngine::clipboardText() const { return QGuiApplication::clipboard()->text(); }
