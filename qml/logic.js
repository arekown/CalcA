// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
.pragma library

// CalcA input state machine (behaves like the Windows calculator).
// Values are exact number strings ("1/3", "-42", "0xFF"); all arithmetic goes through the engine.

let E = null; // Engine singleton, set by init()

const S = {
    mode: 'std', base: 'DEC', angle: 'DEG', fe: false, second: false, trigInv: false, hyp: false,
    tokens: [], entry: null, done: false, lastExpr: '', lastOp: null, err: '', hist: [], mem: []
};
// entry: {typed:true, s:'-12.5e+3'} while typing ('.' internally), or {typed:false, v, label, quiet}

const RADIX = { DEC: 10, HEX: 16, BIN: 2 };
const PREC = { '+': 1, '-': 1, '*': 2, '/': 2, 'mod': 2, '^': 3, 'root': 3, 'logy': 3 };
const RIGHT = { '^': true, 'root': true };
const OPL = { '+': '+', '-': '−', '*': '×', '/': '÷', 'mod': 'mod', '^': '^', 'root': 'yroot', 'logy': 'log base' };
const MAX_HISTORY = 50;

function init(engine) { E = engine; }

// ---------- engine helpers ----------
function CalcError(code) { this.code = code; }
function check(r) { if (r.err) throw new CalcError(r.err); return r.v; }
function op(o, a, b) { return check(E.op(o, a, b)); }
function fn(name, x) { return check(E.fn(name, x, S.angle)); }
function digits() { return S.mode === 'std' ? 16 : 32; }

const fmtCache = new Map();
function fmt(v, base) {
    base = base || S.base;
    const key = v + '|' + base + '|' + S.fe + '|' + digits();
    let s = fmtCache.get(key);
    if (s === undefined) {
        if (fmtCache.size > 2000) fmtCache.clear();
        s = E.format(v, RADIX[base], S.fe, digits());
        fmtCache.set(key, s);
    }
    return s;
}

// ---------- typed entry ----------
function grp4(str) { return str.replace(/\B(?=([0-9A-F]{4})+(?![0-9A-F]))/g, ' '); }
function fmtTyped(s) {
    if (!s) return '0';
    if (S.base !== 'DEC') {
        const neg = s[0] === '-', d = neg ? s.slice(1) : s;
        return (neg ? '-' : '') + grp4(d || '0');
    }
    const loc = Qt.locale();
    let [m, ex] = s.split('e');
    const neg = m[0] === '-';
    if (neg) m = m.slice(1);
    let [i, f] = m.split('.');
    i = (i || '0').replace(/\B(?=(\d{3})+(?!\d))/g, loc.groupSeparator);
    return (neg ? '-' : '') + i + (f !== undefined ? loc.decimalPoint + f : '') + (ex !== undefined ? 'e' + ex : '');
}
function entryVal(e) {
    if (!e.typed) return e.v;
    let s = e.s;
    const neg = s[0] === '-';
    if (neg) s = s.slice(1);
    if (S.base !== 'DEC') return s ? (neg ? '-' : '') + (S.base === 'HEX' ? '0x' : '0b') + s : '0';
    s = s.replace(/e[+-]?$/, '');
    if (s === '') return '0';
    if (s.endsWith('.')) s += '0';
    return (neg ? '-' : '') + s.replace('e', 'E');
}
function maxDigits() { return S.base === 'DEC' ? digits() : S.base === 'HEX' ? 16 : 64; }

// ---------- expression evaluation (shunting-yard, precedence like a real calculator) ----------
function run(tokens) {
    const vals = [], ops = [];
    const reduce = () => { const o = ops.pop(), b = vals.pop(), a = vals.pop(); vals.push(op(o, a, b)); };
    for (const t of tokens) {
        if (t.t === 'num') vals.push(t.v);
        else if (t.t === '(') ops.push('(');
        else if (t.t === ')') { while (ops.length && ops[ops.length - 1] !== '(') reduce(); ops.pop(); }
        else {
            const p = PREC[t.o];
            while (ops.length && ops[ops.length - 1] !== '(' &&
                   (PREC[ops[ops.length - 1]] > p || (PREC[ops[ops.length - 1]] === p && !RIGHT[t.o]))) reduce();
            ops.push(t.o);
        }
    }
    return { vals, ops, reduce };
}
function evalTokens(tokens) {
    const r = run(tokens);
    while (r.ops.length) { if (r.ops[r.ops.length - 1] === '(') r.ops.pop(); else r.reduce(); }
    return r.vals.length ? r.vals[r.vals.length - 1] : '0';
}
function last() { return S.tokens.length ? S.tokens[S.tokens.length - 1] : null; }
function preview() {
    const l = last();
    if (!l || l.t === '(') return '0';
    const r = run(S.tokens);
    return r.vals.length ? r.vals[r.vals.length - 1] : '0';
}
function tokensLabel(tokens) {
    let s = '';
    for (const t of tokens) {
        const part = t.t === 'num' ? t.s : t.t === 'op' ? OPL[t.o] : t.t;
        const glue = !s || s.endsWith('(') || t.t === ')';
        s += (glue ? '' : ' ') + part;
    }
    return s;
}
function openParens() { return S.tokens.reduce((n, t) => n + (t.t === '(' ? 1 : 0) - (t.t === ')' ? 1 : 0), 0); }
function num(v) { return { t: 'num', v: v, s: fmt(v) }; }

// ---------- actions ----------
function setErr(e) {
    if (e instanceof CalcError) S.err = e.code;
    else { console.warn('CalcA:', e); S.err = 'invalid'; }
}
function clearAll() { Object.assign(S, { tokens: [], entry: null, done: false, lastExpr: '', lastOp: null, err: '' }); }
function startEntry() {
    if (S.done) { S.tokens = []; S.lastExpr = ''; S.done = false; }
    if (!S.entry || !S.entry.typed) S.entry = { typed: true, s: '' };
}
function digit(d) {
    if (S.base === 'BIN' && d > '1') return;
    if (S.base === 'DEC' && /[A-F]/.test(d)) return;
    if (S.err) clearAll();
    startEntry();
    const s = S.entry.s;
    if (s.includes('e')) { if (s.split('e')[1].replace(/[+-]/, '').length < 4) S.entry.s += d; return; }
    if (s === '0' || s === '-0') { S.entry.s = s.slice(0, -1) + d; return; }
    if ((s.match(/[0-9A-F]/g) || []).length >= maxDigits()) return;
    S.entry.s += d;
}
function dot() {
    if (S.err) clearAll();
    startEntry();
    const s = S.entry.s;
    if (s.includes('.') || s.includes('e')) return;
    S.entry.s = (s === '' || s === '-' ? s + '0' : s) + '.';
}
function expKey() {
    startEntry();
    if (S.entry.s.includes('e')) return;
    S.entry.s = (S.entry.s || '0').replace(/\.$/, '') + 'e+';
}
function takeOperand() {
    if (S.entry) {
        const e = S.entry;
        S.entry = null;
        const v = entryVal(e);
        return { v: v, label: e.typed ? fmt(v) : e.label };
    }
    const l = last();
    if (l && l.t === ')') {
        let depth = 0, i = S.tokens.length - 1;
        for (; i >= 0; i--) {
            const t = S.tokens[i].t;
            if (t === ')') depth++;
            else if (t === '(' && --depth === 0) break;
        }
        const grp = S.tokens.splice(i);
        return { v: evalTokens(grp), label: tokensLabel(grp) };
    }
    const v = preview();
    return { v: v, label: fmt(v) };
}
function unary(name, labelFn) {
    try {
        const o = takeOperand();
        S.entry = { typed: false, v: fn(name, o.v), label: labelFn(o.label) };
        S.done = false;
    } catch (e) { setErr(e); }
}
const U = {
    sqr: l => 'sqr(' + l + ')', cube: l => 'cube(' + l + ')', recip: l => '1/(' + l + ')',
    sqrt: l => '√(' + l + ')', cbrt: l => 'cuberoot(' + l + ')', abs: l => 'abs(' + l + ')',
    fact: l => 'fact(' + l + ')', pow10: l => '10^(' + l + ')', pow2: l => '2^(' + l + ')',
    log: l => 'log(' + l + ')', ln: l => 'ln(' + l + ')', exp: l => 'e^(' + l + ')',
    floor: l => 'floor(' + l + ')', ceil: l => 'ceil(' + l + ')',
    dms: l => 'dms(' + l + ')', deg: l => 'degrees(' + l + ')'
};
function trigName(n) { return (S.trigInv ? 'a' : '') + n + (S.hyp ? 'h' : ''); }
function trigLabel(n) { return n + (S.hyp ? 'h' : '') + (S.trigInv ? '⁻¹' : ''); }
function negate() {
    const e = S.entry;
    if (e && e.typed && !S.done) {
        if (e.s.includes('e')) e.s = e.s.replace(/e([+-])/, (m, g) => 'e' + (g === '+' ? '-' : '+'));
        else if (/[1-9A-F]/.test(e.s) || e.s.includes('.')) e.s = e.s[0] === '-' ? e.s.slice(1) : '-' + e.s;
        return;
    }
    unary('neg', l => 'negate(' + l + ')');
}
function percent() {
    try {
        const l = last();
        const base = l && l.t === 'op' && (l.o === '+' || l.o === '-') ? preview() : null;
        const o = takeOperand();
        const p = op('/', o.v, '100');
        const v = base === null ? p : op('*', base, p);
        S.entry = { typed: false, v: v, label: fmt(v) };
        S.done = false;
    } catch (e) { setErr(e); }
}
function constant(v, label) {
    if (S.done) { S.tokens = []; S.lastExpr = ''; S.done = false; }
    S.entry = { typed: false, v: v, label: label || fmt(v), quiet: true };
}
function commitEntry() {
    const e = S.entry, l = last();
    if (l && (l.t === 'num' || l.t === ')')) S.tokens.push({ t: 'op', o: '*' });
    const v = entryVal(e);
    S.tokens.push({ t: 'num', v: v, s: e.typed ? fmt(v) : e.label });
    S.entry = null; S.done = false; S.lastExpr = '';
}
function binop(o) {
    if (S.entry) commitEntry();
    else {
        const l = last();
        if (!l || l.t === '(') S.tokens.push(num('0'));
        else if (l.t === 'op') { l.o = o; return; }
    }
    S.tokens.push({ t: 'op', o: o });
}
function lparen() {
    if (S.done) { S.entry = null; S.tokens = []; S.lastExpr = ''; S.done = false; }
    if (S.entry) { commitEntry(); S.tokens.push({ t: 'op', o: '*' }); }
    else { const l = last(); if (l && l.t === ')') S.tokens.push({ t: 'op', o: '*' }); }
    S.tokens.push({ t: '(' });
}
function rparen() {
    if (openParens() <= 0) return;
    if (S.entry) commitEntry();
    else {
        const l = last();
        if (l.t === 'op') S.tokens.push(num(preview()));
        else if (l.t === '(') S.tokens.push(num('0'));
    }
    S.tokens.push({ t: ')' });
}
function finish(r, ex) {
    S.hist.unshift({ expr: ex, v: r, base: S.base }); // shown in the base it was calculated in
    if (S.hist.length > MAX_HISTORY) S.hist.pop();
    Object.assign(S, { lastExpr: ex, tokens: [], entry: { typed: false, v: r, label: fmt(r) }, done: true });
}
function equals() {
    if (S.err) { clearAll(); return; }
    try {
        if (S.done) {
            if (!S.lastOp) return;
            const a = entryVal(S.entry), b = S.lastOp.v;
            finish(op(S.lastOp.o, a, b), fmt(a) + ' ' + OPL[S.lastOp.o] + ' ' + fmt(b) + ' =');
            return;
        }
        if (S.entry) commitEntry();
        else {
            const l = last();
            if (!l || l.t === '(') S.tokens.push(num('0'));
            else if (l.t === 'op') S.tokens.push(num(preview()));
        }
        for (let n = openParens(); n > 0; n--) S.tokens.push({ t: ')' });
        const t = S.tokens;
        S.lastOp = (t.length >= 3 && !t.some(x => x.t === '(') && t[t.length - 2].t === 'op')
            ? { o: t[t.length - 2].o, v: t[t.length - 1].v } : null;
        finish(evalTokens(t), tokensLabel(t) + ' =');
    } catch (e) { setErr(e); }
}
function backspace() {
    if (S.err) { clearAll(); return; }
    const e = S.entry;
    if (e && e.typed && !S.done) {
        e.s = /e[+-]$/.test(e.s) ? e.s.slice(0, -2) : e.s.slice(0, -1);
        if (e.s === '-') e.s = '';
    } else if (S.done) S.lastExpr = '';
}
function clearEntry() {
    if (S.err || S.done) { clearAll(); return; }
    S.entry = null;
}
function curVal() { try { return S.entry ? entryVal(S.entry) : preview(); } catch (e) { return '0'; } }
function freezeEntry() {
    if (S.entry && S.entry.typed) { const v = entryVal(S.entry); S.entry = { typed: false, v: v, label: fmt(v) }; }
}

// ---------- public API ----------
function act(a) {
    if (S.err && !/^(d:|dot$|clr$|ce$|clrce$|bs$|eq$)/.test(a)) return;
    if (S.base !== 'DEC' && (a === 'dot' || a === 'exp')) return;
    if (a.startsWith('d:')) digit(a.slice(2));
    else if (a.startsWith('o:')) binop(a.slice(2));
    else if (a.startsWith('u:')) unary(a.slice(2), U[a.slice(2)]);
    else if (a.startsWith('t:')) { const n = a.slice(2); unary(trigName(n), l => trigLabel(n) + '(' + l + ')'); }
    else if (a === 'f:rand') constant(String(Math.random()));
    else if (a.startsWith('f:')) unary(a.slice(2), U[a.slice(2)]);
    else switch (a) {
        case 'dot': dot(); break;
        case 'neg': negate(); break;
        case 'eq': equals(); break;
        case 'clr': clearAll(); break;
        case 'ce': clearEntry(); break;
        case 'clrce': if (S.entry && !S.done) clearEntry(); else clearAll(); break;
        case 'bs': backspace(); break;
        case 'pct': percent(); break;
        case 'lp': lparen(); break;
        case 'rp': rparen(); break;
        case 'exp': expKey(); break;
        case 'c:pi': try { constant(check(E.value('pi')), 'π'); } catch (e) { setErr(e); } break;
        case 'c:e': try { constant(check(E.value('e')), 'e'); } catch (e) { setErr(e); } break;
        case '2nd': S.second = !S.second; break;
        case 'tinv': S.trigInv = !S.trigInv; break;
        case 'thyp': S.hyp = !S.hyp; break;
    }
}
function setMode(m) { S.mode = m; S.second = false; }
function cycleBase() { freezeEntry(); S.base = { DEC: 'HEX', HEX: 'BIN', BIN: 'DEC' }[S.base]; }
function setBase(b) { if (RADIX[b]) { freezeEntry(); S.base = b; } }
function cycleAngle() { S.angle = { DEG: 'RAD', RAD: 'GRAD', GRAD: 'DEG' }[S.angle]; }
function setAngle(a) { if (a === 'DEG' || a === 'RAD' || a === 'GRAD') S.angle = a; }
function toggleFe() { S.fe = !S.fe; }

function memory(m) {
    if (S.err) return;
    try {
        const v = curVal();
        if (m === 'ms') S.mem.unshift(v);
        else if (m === 'mc') S.mem = [];
        else if (m === 'mr') { if (S.mem.length) constant(S.mem[0]); return; }
        else if (m === 'mp' || m === 'mm') {
            if (!S.mem.length) S.mem.unshift('0');
            S.mem[0] = op(m === 'mp' ? '+' : '-', S.mem[0], v);
        }
        freezeEntry();
    } catch (e) { setErr(e); }
}
function memoryItem(i, m) {
    try {
        if (m === 'mc') S.mem.splice(i, 1);
        else S.mem[i] = op(m === 'mp' ? '+' : '-', S.mem[i], curVal());
        freezeEntry();
    } catch (e) { setErr(e); }
}
function recallMemory(i) { S.err = ''; constant(S.mem[i]); }
function recallHistory(i) {
    const h = S.hist[i];
    S.base = h.base; // expression and value stay in the same number system
    Object.assign(S, { err: '', tokens: [], lastOp: null, lastExpr: h.expr, done: true,
                       entry: { typed: false, v: h.v, label: fmt(h.v) } });
}
function clearHistory() { S.hist = []; }
function clearMemory() { S.mem = []; }
function paste(text) {
    let t = String(text || '').trim().replace(/\s/g, '');
    let v = null;
    if (S.base === 'HEX' && /^-?[0-9a-fA-F]+$/.test(t)) v = (t[0] === '-' ? '-0x' + t.slice(1) : '0x' + t).toUpperCase().replace('0X', '0x');
    else if (S.base === 'BIN' && /^-?[01]+$/.test(t)) v = t[0] === '-' ? '-0b' + t.slice(1) : '0b' + t;
    else if (S.base === 'DEC') {
        const loc = Qt.locale();
        if (loc.decimalPoint === ',' || (t.includes(',') && !t.includes('.'))) t = t.split('.').join('').replace(',', '.');
        else t = t.split(',').join('');
        if (/^-?\d+(\.\d+)?([eE][+-]?\d+)?$/.test(t)) v = t.replace(/e/, 'E');
    }
    if (v === null) return false;
    S.err = '';
    constant(v);
    return true;
}

function keyDefs() {
    const sec = S.second;
    const D = n => [String(n), 'd:' + n, 'd', true];
    const dp = Qt.locale().decimalPoint;
    if (S.mode === 'std') return [
        ['%', 'pct', 'fn', false], ['CE', 'ce', 'fn', true], ['C', 'clr', 'fn', true], ['', 'bs', 'fn', true],
        ['<sup>1</sup>/<sub>x</sub>', 'u:recip', 'fn', false], ['x<sup>2</sup>', 'u:sqr', 'fn', false],
        ['<sup>2</sup>√x', 'u:sqrt', 'fn', false], ['÷', 'o:/', 'op', false],
        D(7), D(8), D(9), ['×', 'o:*', 'op', false],
        D(4), D(5), D(6), ['−', 'o:-', 'op', false],
        D(1), D(2), D(3), ['+', 'o:+', 'op', false],
        ['±', 'neg', 'd', false], D(0), [dp, 'dot', 'd', true], ['=', 'eq', 'eq', true]
    ];
    return [
        ['2<sup>nd</sup>', '2nd', 'fn', false], ['π', 'c:pi', 'fn', false], ['e', 'c:e', 'fn', false],
        ['C', 'clrce', 'fn', true], ['', 'bs', 'fn', true],
        sec ? ['x<sup>3</sup>', 'u:cube', 'fn', false] : ['x<sup>2</sup>', 'u:sqr', 'fn', false],
        ['<sup>1</sup>/<sub>x</sub>', 'u:recip', 'fn', false], ['|x|', 'u:abs', 'fn', false],
        ['exp', 'exp', 'fn', false], ['mod', 'o:mod', 'fn', false],
        sec ? ['<sup>3</sup>√x', 'u:cbrt', 'fn', false] : ['<sup>2</sup>√x', 'u:sqrt', 'fn', false],
        ['(', 'lp', 'fn', false], [')', 'rp', 'fn', false], ['n!', 'u:fact', 'fn', false], ['÷', 'o:/', 'op', false],
        sec ? ['<sup>y</sup>√x', 'o:root', 'fn', false] : ['x<sup>y</sup>', 'o:^', 'fn', false],
        D(7), D(8), D(9), ['×', 'o:*', 'op', false],
        sec ? ['2<sup>x</sup>', 'u:pow2', 'fn', false] : ['10<sup>x</sup>', 'u:pow10', 'fn', false],
        D(4), D(5), D(6), ['−', 'o:-', 'op', false],
        sec ? ['log<sub>y</sub>x', 'o:logy', 'fn', false] : ['log', 'u:log', 'fn', false],
        D(1), D(2), D(3), ['+', 'o:+', 'op', false],
        sec ? ['e<sup>x</sup>', 'u:exp', 'fn', false] : ['ln', 'u:ln', 'fn', false],
        ['±', 'neg', 'd', false], D(0), [dp, 'dot', 'd', true], ['=', 'eq', 'eq', true]
    ];
}
function trigDefs() {
    const sup = S.trigInv ? '<sup>-1</sup>' : '', h = S.hyp ? 'h' : '';
    return ['sin', 'cos', 'tan', 'sec', 'csc', 'cot'].map(n => [n + h + sup, 't:' + n, 'fn', false]);
}
function keyEnabled(a, keep) {
    if (S.err && !keep) return false;
    if (S.base !== 'DEC' && (a === 'dot' || a === 'exp')) return false;
    if (S.base === 'BIN' && /^d:[2-9]$/.test(a)) return false;
    return true;
}

// Snapshot of everything the view needs.
function view() {
    let display = '0';
    if (!S.err) {
        try {
            const val = S.entry ? entryVal(S.entry) : preview();
            display = S.entry && S.entry.typed ? fmtTyped(S.entry.s) : fmt(val);
        } catch (e) { setErr(e); }
    }
    const l = S.entry;
    return {
        display: display, error: S.err,
        expr: S.done ? S.lastExpr
            : tokensLabel(S.tokens) + (l && !l.typed && !l.quiet ? (S.tokens.length ? ' ' : '') + l.label : ''),
        canClearEntry: !!(S.entry && !S.done && !S.err),
        mode: S.mode, base: S.base, angle: S.angle, fe: S.fe, second: S.second, trigInv: S.trigInv, hyp: S.hyp,
        hist: S.hist.map(h => ({ expr: h.expr, text: fmt(h.v, h.base) })),
        mem: S.mem.map(v => fmt(v))
    };
}
